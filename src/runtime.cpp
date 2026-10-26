// 运行时：金属冶炼模型、反应引擎、AST 解释器。
#include "core.h"
#include "compiler.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif
#include <thread>

namespace co2 {

// ---------------------------------------------------------------- 编码

// 源码里的字符串是 UTF-8，Windows 控制台是 GBK，输出前要转一次
std::string utf8ToGbk(const std::string& s) {
#ifdef _WIN32
    if (s.empty()) return s;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (wlen <= 0) return s;
    std::wstring w((size_t)wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], wlen);

    int glen = WideCharToMultiByte(936, 0, w.c_str(), wlen, nullptr, 0, nullptr, nullptr);
    if (glen <= 0) return s;
    std::string g((size_t)glen, '\0');
    WideCharToMultiByte(936, 0, w.c_str(), wlen, &g[0], glen, nullptr, nullptr);
    return g;
#else
    return s;
#endif
}

// ---------------------------------------------------------------- 纯度

// 金属纯度映射到基础优化等级。
// 50% 是刚出矿的粗矿，100% 是电解精炼的纯金属。
double purityToLevel(double purity) {
    if (purity <= 50.0) return 0.0;
    if (purity >= 100.0) return 3.0;
    return 3.0 * (purity - 50.0) / 50.0;
}

// ---------------------------------------------------------------- 金属

struct MetalInfo {
    const char* symbol;
    const char* name;
    const char* valence;
    const char* trait;
    double factor;
};

static const MetalInfo* findMetal(const std::string& s) {
    static const MetalInfo metals[] = {
        {"Na", "钠", "+1", "极活泼，遇水剧烈反应，优化最激进", 1.00},
        {"Mn", "锰", "+2~+7", "变价金属，氧化态越高优化越强", 0.90},
        {"Fe", "铁", "+2/+3", "结构金属，稳定可靠，优化温和", 0.85},
        // 钴：理论优化最强，但空气里只占 0.04%，用完要排队等
        {"Co", "钴", "+2/+3", "二价钴离子 Co2+ 粉红色，理论上优化最强，但空气里只占 0.04%", 0.00},
        {"Cu", "铜", "+2", "活动性在氢之后，不与稀酸反应", 0.80},
    };
    for (auto& m : metals) {
        if (s == m.symbol) return &m;
    }
    return nullptr;
}

// 空气里 Co2+ 的体积分数
const double COBALT_SHARE = 0.0004;      // 0.04%

static std::string joinKeys(const std::vector<std::string>& keys) {
    std::string s;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (i) s += " + ";
        s += keys[i];
    }
    return s;
}

// 一次反应的判定结果
struct Reaction {
    bool ok = true;
    std::string error;
    std::string equation;
    std::string name;
    std::string condition;
    std::string note;
    double factor = 1.0;
};

static Reaction lookup(const std::string& metal, const std::string& ligand) {
    Reaction r;

    // 钠
    if (metal == "Na") {
        if (ligand == "H2O") {
            r.equation = "2Na + 2H2O -> 2NaOH + H2(g)";
            r.name = "钠与水";
            r.condition = "常温，剧烈";
            r.note = "反应放热剧烈，优化全开。代价是代码里任何未定义行为都会当场炸给你看";
            r.factor = 1.15;
            return r;
        }
        if (ligand == "Cl2") {
            r.equation = "2Na + Cl2 -> 2NaCl";
            r.name = "钠与氯气";
            r.condition = "点燃，黄色火焰";
            r.note = "生成稳定的氯化钠，晶格把活泼性锁死，优化降到最保守";
            r.factor = 0.45;
            return r;
        }
        if (ligand == "O2") {
            r.equation = "4Na + O2 -> 2Na2O";
            r.name = "钠与氧气";
            r.condition = "常温缓慢氧化";
            r.note = "表面生成氧化钠，活性略有下降，但整体依然偏激进";
            r.factor = 1.05;
            return r;
        }
        if (ligand == "CuSO4") {
            r.ok = false;
            r.error = "钠不能从硫酸铜溶液里置换出铜 —— 它会先跟水反应生成 NaOH 和氢气，"
                      "生成的 NaOH 再和 CuSO4 生成蓝色 Cu(OH)2 沉淀。想置换请用铁。";
            return r;
        }
        r.ok = false;
        r.error = "钠不认识 " + ligand + "。可以试：-Na、-Na+H2O、-Na+Cl2、-Na+O2";
        return r;
    }

    // 锰
    if (metal == "Mn") {
        if (ligand == "O2") {
            r.equation = "Mn + O2 -> MnO2";
            r.name = "锰与氧气";
            r.condition = "加热";
            r.note = "生成二氧化锰，锰升到 +4 价，氧化能力增强";
            r.factor = 1.20;
            return r;
        }
        if (ligand == "HCl") {
            r.equation = "MnO2 + 4HCl(conc) -> MnCl2 + Cl2(g) + 2H2O";
            r.name = "二氧化锰与浓盐酸";
            r.condition = "加热";
            r.note = "实验室制氯气的经典反应。锰从 +4 降到 +2，把氯离子氧化成氯气，"
                     "氧化能力最强，优化拉到极限";
            r.factor = 1.35;
            return r;
        }
        if (ligand == "H2O") {
            r.ok = false;
            r.error = "锰在常温下不与水反应。锰要跟水反应需要高温水蒸气："
                      "Mn + H2O(g) -> MnO + H2，请用 -Mn+O2 代替。";
            return r;
        }
        r.ok = false;
        r.error = "锰不认识 " + ligand + "。可以试：-Mn、-Mn+O2、-Mn+HCl";
        return r;
    }

    // 铁
    if (metal == "Fe") {
        if (ligand == "O2") {
            r.equation = "4Fe + 3O2 -> 2Fe2O3";
            r.name = "铁生锈";
            r.condition = "潮湿空气，缓慢";
            r.note = "铁被氧化成 +3 价，表面疏松多孔，优化能力被锈蚀掉一部分";
            r.factor = 0.70;
            return r;
        }
        if (ligand == "CuSO4") {
            r.equation = "Fe + CuSO4 -> FeSO4 + Cu";
            r.name = "铁置换铜";
            r.condition = "常温溶液";
            r.note = "铁把铜从溶液里置换出来，自身变成 Fe2+。置换过程相当于一次代码提纯，"
                     "优化略有提升";
            r.factor = 1.10;
            return r;
        }
        if (ligand == "HCl") {
            r.equation = "Fe + 2HCl -> FeCl2 + H2(g)";
            r.name = "铁与盐酸";
            r.condition = "常温";
            r.note = "生成氯化亚铁，铁是 +2 价，温和溶解，优化保持稳定";
            r.factor = 1.00;
            return r;
        }
        if (ligand == "H2O") {
            r.ok = false;
            r.error = "铁在常温纯水里几乎不反应。生锈需要氧气和水同时存在："
                      "4Fe + 3O2 -> 2Fe2O3，请用 -Fe+O2。";
            return r;
        }
        r.ok = false;
        r.error = "铁不认识 " + ligand + "。可以试：-Fe、-Fe+O2、-Fe+CuSO4、-Fe+HCl";
        return r;
    }

    // 钴
    if (metal == "Co") {
        if (ligand == "HCl") {
            r.equation = "Co + 2HCl -> CoCl2 + H2(g)";
            r.name = "钴与盐酸";
            r.condition = "常温，缓慢";
            r.note = "生成氯化钴。无水 CoCl2 是蓝色，吸水后变粉红 —— "
                     "实验室拿它当硅胶干燥剂的变色指示剂";
            r.factor = 1.00;
            return r;
        }
        if (ligand == "O2") {
            r.equation = "2Co + O2 -> 2CoO";
            r.name = "钴与氧气";
            r.condition = "加热";
            r.note = "生成氧化钴，钴升到 +2 价";
            r.factor = 0.75;
            return r;
        }
        if (ligand == "H2O") {
            r.ok = false;
            r.error = "钴在常温下不与水反应。钴的活动性排在铁之后，"
                      "比铁更耐水侵蚀，但依然能被氧化性酸溶解。";
            return r;
        }
        r.ok = false;
        r.error = "钴不认识 " + ligand + "。可以试：-Co、-Co+HCl、-Co+O2";
        return r;
    }

    // 铜
    if (metal == "Cu") {
        if (ligand == "HCl") {
            r.ok = false;
            r.error = "铜在金属活动性顺序里排在氢之后，不能置换出稀盐酸里的氢。"
                      "想溶解铜请用硝酸：3Cu + 8HNO3 -> 3Cu(NO3)2 + 2NO + 4H2O";
            return r;
        }
        if (ligand == "O2") {
            r.equation = "2Cu + O2 -> 2CuO";
            r.name = "铜与氧气";
            r.condition = "加热，黑色";
            r.note = "生成氧化铜，铜升到 +2 价";
            r.factor = 0.95;
            return r;
        }
        r.ok = false;
        r.error = "铜只支持 -Cu 和 -Cu+O2。";
        return r;
    }

    r.ok = false;
    r.error = "不认识的金属 " + metal;
    return r;
}

ChemResult resolveChem(const std::string& spec, bool light) {
    (void)light;   // 金属体系不需要光照

    ChemResult r;
    std::vector<std::string> keys;

    std::stringstream ss(spec);
    std::string part;
    while (std::getline(ss, part, '+')) {
        size_t start = part.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        part = part.substr(start);
        size_t end = part.find_last_not_of(" \t");
        if (end != std::string::npos) part = part.substr(0, end + 1);
        if (part.empty()) continue;
        bool dup = false;
        for (auto& k : keys) if (k == part) dup = true;
        if (!dup) keys.push_back(part);
    }

    if (keys.empty()) {
        r.ok = false;
        r.error = "没有指定金属。可以试：-Na、-Mn、-Fe";
        return r;
    }

    // 常见误用
    if (keys[0] == "CO2" || keys[0] == "CO" || keys[0] == "CH4" ||
        keys[0] == "NO2" || keys[0] == "H2O" || keys[0] == "O2") {
        r.ok = false;
        r.error = "这是金属编译器，不收气体。CO2 是二氧化碳（碳 +4 价），"
                  "钴是 Co，两者不是一回事。想用金属请试 -Fe、-Mn、-Na。";
        return r;
    }

    // 用户可能写 -Co2 或 -Co2+ 表示二价钴离子
    if (keys[0] == "Co2" || keys[0] == "Co2+" || keys[0] == "Co(II)") keys[0] = "Co";

    const MetalInfo* metal = findMetal(keys[0]);
    if (!metal) {
        r.ok = false;
        r.error = "不认识 " + keys[0] + "。支持的金属：Na（钠）、Mn（锰）、Fe（铁）、Cu（铜）";
        return r;
    }

    r.mode = metal->symbol;
    r.name = metal->name;

    if (keys.size() == 1) {
        r.equation = std::string(metal->symbol) + " -> " + metal->symbol +
                     "(" + metal->valence + ") + e-";
        r.condition = "单质态";
        r.note = metal->trait;
        r.factor = metal->factor;
        if (r.mode == "Co") {
            r.waitMs = 300;
            r.note += "。空气里 Co2+ 只占 0.04%，抢的人多了就会被抽干，"
                      "编译器插入了排队等待";
        }
        return r;
    }

    if (keys.size() > 2) {
        r.ok = false;
        r.error = joinKeys(keys) + " 一次只能指定一种配体，请用 金属+配体 的形式";
        return r;
    }

    Reaction rx = lookup(keys[0], keys[1]);
    if (!rx.ok) {
        r.ok = false;
        r.error = rx.error;
        return r;
    }

    r.equation = rx.equation;
    r.name = rx.name;
    r.condition = rx.condition;
    r.note = rx.note;
    r.factor = metal->factor * rx.factor;
    if (r.mode == "Co") {
        r.waitMs = 300;
        r.note += "。空气里 Co2+ 只占 0.04%，编译器插入了排队等待";
    }
    return r;
}

// ---------------------------------------------------------------- 解释器

namespace {

struct Val {
    Type type = Type::Int;
    long long i = 0;
    double d = 0;
    bool b = false;
    std::string s;

    double num() const {
        if (type == Type::Double) return d;
        if (type == Type::Bool) return b ? 1.0 : 0.0;
        return (double)i;
    }
};

struct ReturnSignal { Val value; bool hasValue = false; };
struct BreakSignal {};
struct ContinueSignal {};

class Interp {
public:
    Interp(const Program& prog, int waitMs) : prog_(prog), waitMs_(waitMs) {
        for (auto& fn : prog.functions)
            if (fn->body) bodies_[fn->name] = fn.get();
    }

    int run(std::vector<std::string>& out) {
        out_ = &out;
        // 钴资源排队：空气里 Co2+ 只占 0.04%
        if (waitMs_ > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(waitMs_));
        auto it = bodies_.find("main");
        if (it == bodies_.end()) throw CompileError("没有 main 函数");
        try {
            call("main", {});
        } catch (ReturnSignal& r) {
            return r.hasValue ? (int)r.value.num() : 0;
        }
        return 0;
    }

private:
    const Program& prog_;
    int waitMs_ = 0;
    std::map<std::string, const Function*> bodies_;
    std::vector<std::map<std::string, Val>> scopes_;
    std::vector<std::string>* out_ = nullptr;
    int depth_ = 0;
    long long steps_ = 0;

    void tick() {
        if (++steps_ > 20000000)
            throw CompileError("执行步数超过上限，可能是死循环或计算量太大");
    }

    Val call(const std::string& name, const std::vector<Val>& args) {
        auto it = bodies_.find(name);
        if (it == bodies_.end()) throw CompileError("函数 " + name + " 未定义");
        if (++depth_ > 2000) { depth_--; throw CompileError("递归太深"); }

        const Function* fn = it->second;
        scopes_.push_back({});
        for (size_t i = 0; i < fn->params.size() && i < args.size(); ++i) {
            Val v = args[i];
            v.type = fn->params[i].type;
            scopes_.back()[fn->params[i].name] = v;
        }

        try {
            execBlock(*fn->body, false);
        } catch (ReturnSignal& r) {
            scopes_.pop_back();
            depth_--;
            if (fn->ret == Type::Void) return Val{};
            Val v = r.value;
            v.type = fn->ret;
            return v;
        }
        scopes_.pop_back();
        depth_--;
        return Val{};
    }

    Val* find(const std::string& name) {
        for (int i = (int)scopes_.size() - 1; i >= 0; --i) {
            auto it = scopes_[i].find(name);
            if (it != scopes_[i].end()) return &it->second;
        }
        return nullptr;
    }

    void execBlock(const Block& b, bool newScope = true) {
        if (newScope) scopes_.push_back({});
        for (auto& s : b.stmts) exec(s.get());
        if (newScope) scopes_.pop_back();
    }

    void exec(Stmt* s) {
        tick();
        if (auto* d = dynamic_cast<VarDecl*>(s)) {
            Val v;
            v.type = d->vtype;
            if (d->init) v = eval(d->init.get());
            v.type = d->vtype;
            if (d->vtype == Type::Int) v.i = (long long)v.num();
            scopes_.back()[d->name] = v;
            return;
        }
        if (auto* a = dynamic_cast<Assign*>(s)) {
            Val* slot = find(a->target);
            if (!slot) throw CompileError("变量 " + a->target + " 未声明", a->line);
            Val v = eval(a->value.get());
            if (a->op == "+=") { v.i = (long long)(slot->num() + v.num()); v.type = slot->type; }
            else if (a->op == "-=") { v.i = (long long)(slot->num() - v.num()); v.type = slot->type; }
            v.type = slot->type;
            if (slot->type == Type::Int) v.i = (long long)v.num();
            *slot = v;
            return;
        }
        if (auto* i = dynamic_cast<If*>(s)) {
            if (eval(i->cond.get()).num() != 0) execBlock(*static_cast<Block*>(i->thenStmt.get()));
            else if (i->elseStmt) {
                if (auto* b = dynamic_cast<Block*>(i->elseStmt.get())) execBlock(*b);
                else exec(i->elseStmt.get());
            }
            return;
        }
        if (auto* w = dynamic_cast<While*>(s)) {
            while (eval(w->cond.get()).num() != 0) {
                try { execBlock(*static_cast<Block*>(w->body.get())); }
                catch (BreakSignal&) { break; }
                catch (ContinueSignal&) { continue; }
            }
            return;
        }
        if (auto* f = dynamic_cast<For*>(s)) {
            scopes_.push_back({});
            if (f->init) exec(f->init.get());
            for (;;) {
                if (f->cond && eval(f->cond.get()).num() == 0) break;
                try { execBlock(*static_cast<Block*>(f->body.get())); }
                catch (BreakSignal&) { break; }
                catch (ContinueSignal&) {}
                if (f->step) exec(f->step.get());
            }
            scopes_.pop_back();
            return;
        }
        if (auto* r = dynamic_cast<Return*>(s)) {
            ReturnSignal sig;
            if (r->value) { sig.value = eval(r->value.get()); sig.hasValue = true; }
            throw sig;
        }
        if (dynamic_cast<Break*>(s)) throw BreakSignal{};
        if (dynamic_cast<Continue*>(s)) throw ContinueSignal{};
        if (auto* p = dynamic_cast<Print*>(s)) {
            Val v = eval(p->value.get());
            std::string text;
            if (p->value->type == Type::String) text = utf8ToGbk(v.s);
            else if (p->value->type == Type::Bool) text = v.num() != 0 ? "true" : "false";
            else if (p->value->type == Type::Double) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%g", v.num());
                text = buf;
            } else {
                text = std::to_string((long long)v.num());
            }
            out_->push_back(text);
            return;
        }
        if (auto* e = dynamic_cast<ExprStmt*>(s)) { eval(e->expr.get()); return; }
        if (auto* b = dynamic_cast<Block*>(s)) { execBlock(*b); return; }
    }

    Val eval(Expr* e) {
        tick();
        if (auto* p = dynamic_cast<IntLit*>(e)) { Val v; v.type = Type::Int; v.i = p->value; return v; }
        if (auto* p = dynamic_cast<FloatLit*>(e)) { Val v; v.type = Type::Double; v.d = p->value; return v; }
        if (auto* p = dynamic_cast<BoolLit*>(e)) { Val v; v.type = Type::Bool; v.b = p->value; return v; }
        if (auto* p = dynamic_cast<StringLit*>(e)) { Val v; v.type = Type::String; v.s = p->value; return v; }
        if (auto* p = dynamic_cast<Var*>(e)) {
            Val* slot = find(p->name);
            if (!slot) throw CompileError("变量 " + p->name + " 未定义", e->line);
            return *slot;
        }
        if (auto* b = dynamic_cast<Binary*>(e)) {
            if (b->op == "&&") {
                if (eval(b->lhs.get()).num() == 0) { Val v; v.type = Type::Bool; v.b = false; return v; }
                Val v; v.type = Type::Bool; v.b = eval(b->rhs.get()).num() != 0; return v;
            }
            if (b->op == "||") {
                if (eval(b->lhs.get()).num() != 0) { Val v; v.type = Type::Bool; v.b = true; return v; }
                Val v; v.type = Type::Bool; v.b = eval(b->rhs.get()).num() != 0; return v;
            }
            Val x = eval(b->lhs.get());
            Val y = eval(b->rhs.get());
            Val r;
            bool useFloat = (e->type == Type::Double) || x.type == Type::Double || y.type == Type::Double;
            const std::string& op = b->op;

            if (op == "+" || op == "-" || op == "*") {
                if (useFloat) { r.type = Type::Double; r.d = (op == "+") ? x.num() + y.num()
                                                             : (op == "-") ? x.num() - y.num()
                                                                           : x.num() * y.num(); }
                else { r.type = Type::Int; r.i = (op == "+") ? x.i + y.i
                                                    : (op == "-") ? x.i - y.i : x.i * y.i; }
                return r;
            }
            if (op == "/") {
                if (useFloat) {
                    if (y.num() == 0) throw CompileError("除以零", e->line);
                    r.type = Type::Double;
                    r.d = x.num() / y.num();
                } else {
                    if (y.i == 0) throw CompileError("除以零", e->line);
                    r.type = Type::Int;
                    r.i = x.i / y.i;
                }
                return r;
            }
            if (op == "%") {
                if (y.i == 0) throw CompileError("对零取模", e->line);
                r.type = Type::Int;
                r.i = x.i % y.i;
                return r;
            }
            if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
                double a = x.num(), c = y.num();
                r.type = Type::Bool;
                r.b = (op == "==") ? a == c : (op == "!=") ? a != c : (op == "<") ? a < c
                    : (op == ">") ? a > c : (op == "<=") ? a <= c : a >= c;
                return r;
            }
            throw CompileError("不认识的运算符 " + op, e->line);
        }
        if (auto* u = dynamic_cast<Unary*>(e)) {
            Val v = eval(u->operand.get());
            if (u->op == "!") { Val r; r.type = Type::Bool; r.b = v.num() == 0; return r; }
            if (v.type == Type::Double) { v.d = -v.d; } else { v.i = -v.i; }
            return v;
        }
        if (auto* c = dynamic_cast<Call*>(e)) {
            if (c->name == "input") {
                long long v = 0;
                if (std::scanf("%lld", &v) != 1) v = 0;
                Val r;
                r.type = Type::Int;
                r.i = v;
                return r;
            }
            std::vector<Val> args;
            for (auto& a : c->args) args.push_back(eval(a.get()));
            return call(c->name, args);
        }
        return Val{};
    }
};

}  // namespace

int interpret(const Program& prog, std::vector<std::string>& out, int waitMs) {
    return Interp(prog, waitMs).run(out);
}

}  // namespace co2
