// 语义分析：作用域、类型检查、诊断。
#include "core.h"

#include <cctype>
#include <cstdio>
#include <map>
#include <memory>
#include <set>

namespace co2 {

namespace {

struct FuncSig {
    std::string name;
    Type ret = Type::Void;
    std::vector<Type> paramTypes;
    int line = 0;
    bool hasBody = false;
};

struct VarInfo {
    std::string name;
    Type type = Type::Unknown;
    int line = 0;
    int reads = 0;
    enum Kind { Local, Param, Macro } kind = Local;
};

struct Scope {
    std::map<std::string, std::shared_ptr<VarInfo>> vars;
};

class Analyzer {
public:
    Analyzer(const Options& opt, std::vector<Diagnostic>& diags)
        : opt_(opt), diags_(diags) {}

    void run(Program& prog) {
        collectFunctions(prog);
        if (!funcs_.count("main"))
            throw CompileError("程序缺少 main 函数，无法生成可执行文件", 1, 0);

        for (auto& fn : prog.functions) {
            if (!fn->body) continue;
            try {
                analyzeFunction(*fn);
            } catch (CompileError& e) {
                if (e.line == 0) e.line = fn->line;
                throw;
            }
        }

        if (opt_.warnAll) checkUnusedFunctions(prog);
    }

private:
    const Options& opt_;
    std::vector<Diagnostic>& diags_;
    std::map<std::string, FuncSig> funcs_;
    std::vector<Scope> scopes_;
    std::vector<std::shared_ptr<VarInfo>> allVars_;
    std::set<std::string> called_;
    int loopDepth_ = 0;
    Type currentRet_ = Type::Void;
    std::string currentFile_;

    // ---------------------------------------------------------- 诊断

    void warn(const std::string& code, const std::string& msg, int line, bool extra = false) {
        if (extra) {
            if (!opt_.warnExtra) return;
        } else if (!opt_.warnAll) {
            return;
        }
        if (opt_.warnError) {
            throw CompileError("[" + code + "] " + msg + "（-Werror 已把警告当作错误）", line, 0);
        }
        Diagnostic d;
        d.sev = Diagnostic::Warning;
        d.code = code;
        d.message = msg;
        d.line = line;
        d.file = currentFile_;
        diags_.push_back(d);
    }

    // ---------------------------------------------------------- 函数表

    void collectFunctions(Program& prog) {
        for (auto& fn : prog.functions) {
            FuncSig sig;
            sig.name = fn->name;
            sig.ret = fn->ret;
            sig.line = fn->line;
            sig.hasBody = (fn->body != nullptr);
            for (auto& p : fn->params) sig.paramTypes.push_back(p.type);

            auto it = funcs_.find(fn->name);
            if (it == funcs_.end()) {
                funcs_[fn->name] = sig;
                continue;
            }
            if (it->second.ret != sig.ret || it->second.paramTypes != sig.paramTypes) {
                throw CompileError("函数 " + fn->name + " 的签名与之前的声明不一致", fn->line, 0);
            }
            if (sig.hasBody && it->second.hasBody) {
                throw CompileError("函数 " + fn->name + " 重复定义", fn->line, 0);
            }
            if (sig.hasBody) {
                it->second.hasBody = true;
                it->second.line = fn->line;
            }
        }
    }

    void checkUnusedFunctions(Program& prog) {
        std::set<std::string> defined;
        for (auto& fn : prog.functions)
            if (fn->body) defined.insert(fn->name);

        for (auto& name : defined) {
            if (name == "main" || called_.count(name)) continue;
            Diagnostic d;
            d.sev = Diagnostic::Warning;
            d.code = "W007";
            d.message = "函数 " + name + " 定义后从未被调用";
            d.line = funcs_[name].line;
            diags_.push_back(d);
        }
    }

    // ---------------------------------------------------------- 作用域

    void pushScope() { scopes_.push_back(Scope()); }
    void popScope() { scopes_.pop_back(); }

    VarInfo* findVar(const std::string& name) {
        for (int i = (int)scopes_.size() - 1; i >= 0; --i) {
            auto it = scopes_[i].vars.find(name);
            if (it != scopes_[i].vars.end()) return it->second.get();
        }
        return nullptr;
    }

    VarInfo* useVar(const std::string& name) {
        VarInfo* v = findVar(name);
        if (v) v->reads++;
        return v;
    }

    int outerDeclLine(const std::string& name) {
        for (int i = (int)scopes_.size() - 2; i >= 0; --i) {
            auto it = scopes_[i].vars.find(name);
            if (it != scopes_[i].vars.end()) return it->second->line;
        }
        return -1;
    }

    // ---------------------------------------------------------- 函数分析

    void analyzeFunction(Function& fn) {
        currentFile_ = fn.file;
        currentRet_ = fn.ret;
        loopDepth_ = 0;
        scopes_.clear();
        allVars_.clear();
        pushScope();

        // -D 宏作为全局常量
        for (auto& kv : opt_.defines) {
            auto v = std::make_shared<VarInfo>();
            v->name = kv.first;
            v->kind = VarInfo::Macro;
            v->type = Type::Int;
            const std::string& val = kv.second;
            if (val == "true" || val == "false") v->type = Type::Bool;
            else if (val.find('.') != std::string::npos) v->type = Type::Double;
            else if (!val.empty() && (std::isdigit((unsigned char)val[0]) || val[0] == '-')) v->type = Type::Int;
            else v->type = Type::String;
            scopes_.back().vars[kv.first] = v;
        }

        for (auto& p : fn.params) {
            if (p.type == Type::Void)
                throw CompileError("参数 " + p.name + " 不能是 void", fn.line, 0);
            if (scopes_.back().vars.count(p.name))
                throw CompileError("参数 " + p.name + " 重复声明", fn.line, 0);
            auto v = std::make_shared<VarInfo>();
            v->name = p.name;
            v->type = p.type;
            v->line = fn.line;
            v->kind = VarInfo::Param;
            scopes_.back().vars[p.name] = v;
            allVars_.push_back(v);
        }

        analyzeBlock(*fn.body, false);

        checkUnusedVars();

        if (fn.ret != Type::Void && !alwaysReturns(fn.body.get()) && fn.name != "main") {
            Diagnostic d;
            d.sev = Diagnostic::Warning;
            d.code = "W008";
            d.message = "函数 " + fn.name + " 声明返回 " + typeName(fn.ret) +
                        "，但存在不返回值的路径；未返回时将使用默认值";
            d.line = fn.line;
            d.file = currentFile_;
            diags_.push_back(d);
        }

        popScope();
    }

    void checkUnusedVars() {
        for (auto& sp : allVars_) {
            VarInfo& v = *sp;
            if (v.reads > 0) continue;
            if (v.kind == VarInfo::Param) {
                warn("W006", "参数 " + v.name + " 未被使用", v.line, true);
            } else if (v.kind == VarInfo::Local) {
                warn("W001", "变量 " + v.name + " 声明后未被使用", v.line);
            }
        }
    }

    static bool alwaysReturns(Stmt* s) {
        if (auto* r = dynamic_cast<Return*>(s)) return true;
        if (auto* b = dynamic_cast<Block*>(s)) {
            for (auto& st : b->stmts) {
                if (dynamic_cast<Return*>(st.get())) return true;
                if (auto* i = dynamic_cast<If*>(st.get())) {
                    if (i->elseStmt && alwaysReturns(i->thenStmt.get()) &&
                        alwaysReturns(i->elseStmt.get()))
                        return true;
                }
                if (auto* w = dynamic_cast<While*>(st.get())) {
                    if (isTrueLiteral(w->cond.get()) && alwaysReturns(w->body.get()))
                        return true;
                }
            }
        }
        return false;
    }

    static bool isTrueLiteral(Expr* e) {
        if (auto* b = dynamic_cast<BoolLit*>(e)) return b->value;
        if (auto* i = dynamic_cast<IntLit*>(e)) return i->value != 0;
        return false;
    }

    // ---------------------------------------------------------- 语句

    void analyzeBlock(Block& b, bool newScope = true) {
        if (newScope) pushScope();
        for (auto& s : b.stmts) analyzeStmt(s.get());
        if (newScope) popScope();
    }

    void declareVar(const std::string& name, Type t, int line) {
        int shadow = outerDeclLine(name);
        if (shadow >= 0)
            warn("W002", "变量 " + name + " 遮蔽了第 " + std::to_string(shadow) + " 行的同名变量", line);
        if (scopes_.back().vars.count(name))
            throw CompileError("变量 " + name + " 在当前作用域重复声明", line, 0);
        auto v = std::make_shared<VarInfo>();
        v->name = name;
        v->type = t;
        v->line = line;
        v->kind = VarInfo::Local;
        scopes_.back().vars[name] = v;
        allVars_.push_back(v);
    }

    void analyzeStmt(Stmt* s) {
        if (auto* d = dynamic_cast<VarDecl*>(s)) {
            if (d->vtype == Type::Void)
                throw CompileError("变量 " + d->name + " 不能声明为 void", d->line, 0);
            if (d->isAuto) {
                if (!d->init)
                    throw CompileError("auto 变量 " + d->name + " 必须初始化，否则推导不出类型", d->line, 0);
                Type it = analyzeExpr(d->init.get());
                if (it == Type::Void || it == Type::String)
                    throw CompileError("auto 推导不出 " + std::string(typeName(it)) + " 类型", d->line, 0);
                d->vtype = it;
            } else if (d->init) {
                Type it = analyzeExpr(d->init.get());
                if (!assignable(d->vtype, it))
                    throw CompileError("不能用 " + std::string(typeName(it)) + " 初始化 " +
                                       typeName(d->vtype) + " 变量 " + d->name, d->line, 0);
                if (d->vtype == Type::Int && it == Type::Double)
                    warn("W003", "用 double 初始化 int 变量 " + d->name + "，小数部分会丢失",
                         d->line, true);
            }
            declareVar(d->name, d->vtype, d->line);
            return;
        }

        if (auto* a = dynamic_cast<Assign*>(s)) {
            VarInfo* v = useVar(a->target);
            if (!v) throw CompileError("变量 " + a->target + " 未声明", a->line, 0);
            Type it = analyzeExpr(a->value.get());
            if (a->op == "=") {
                if (!assignable(v->type, it))
                    throw CompileError("不能把 " + std::string(typeName(it)) + " 赋给 " +
                                       typeName(v->type) + " 变量 " + a->target, a->line, 0);
                if (v->type == Type::Int && it == Type::Double)
                    warn("W003", "把 double 赋给 int 变量 " + a->target + "，小数部分会丢失",
                         a->line, true);
            } else if (!isNumeric(v->type) || !isNumeric(it)) {
                throw CompileError("复合赋值 " + a->op + " 只支持数值类型", a->line, 0);
            }
            return;
        }

        if (auto* i = dynamic_cast<If*>(s)) {
            requireCondition(i->cond.get(), i->line);
            if (i->isConstexpr && !isConstExpr(i->cond.get()))
                throw CompileError("if constexpr 的条件必须是编译期常量，不能依赖运行时变量",
                                   i->line, 0);
            if (dynamic_cast<BoolLit*>(i->cond.get()) || dynamic_cast<IntLit*>(i->cond.get()))
                warn("W004", "条件恒为常量，另一个分支永远不执行", i->line, true);
            analyzeBlock(*static_cast<Block*>(i->thenStmt.get()));
            if (i->elseStmt) {
                if (auto* e = dynamic_cast<Block*>(i->elseStmt.get())) analyzeBlock(*e);
                else analyzeStmt(i->elseStmt.get());
            }
            return;
        }

        if (auto* w = dynamic_cast<While*>(s)) {
            requireCondition(w->cond.get(), w->line);
            auto* body = static_cast<Block*>(w->body.get());
            if (body->stmts.empty()) warn("W005", "循环体是空的", w->line, true);
            loopDepth_++;
            analyzeBlock(*body);
            loopDepth_--;
            return;
        }

        if (auto* f = dynamic_cast<For*>(s)) {
            pushScope();
            if (f->init) analyzeStmt(f->init.get());
            if (f->cond) requireCondition(f->cond.get(), f->line);
            if (f->step) analyzeStmt(f->step.get());
            loopDepth_++;
            analyzeBlock(*static_cast<Block*>(f->body.get()));
            loopDepth_--;
            popScope();
            return;
        }

        if (auto* r = dynamic_cast<Return*>(s)) {
            if (r->value) {
                Type t = analyzeExpr(r->value.get());
                if (currentRet_ == Type::Void)
                    throw CompileError("void 函数不能返回值", r->line, 0);
                if (!assignable(currentRet_, t))
                    throw CompileError("函数应返回 " + std::string(typeName(currentRet_)) +
                                       "，但这里返回 " + typeName(t), r->line, 0);
            } else if (currentRet_ != Type::Void) {
                throw CompileError("函数应返回 " + std::string(typeName(currentRet_)) +
                                   "，但这里什么都没返回", r->line, 0);
            }
            return;
        }

        if (dynamic_cast<Break*>(s)) {
            if (loopDepth_ == 0) throw CompileError("break 只能出现在循环里", s->line, 0);
            return;
        }
        if (dynamic_cast<Continue*>(s)) {
            if (loopDepth_ == 0) throw CompileError("continue 只能出现在循环里", s->line, 0);
            return;
        }

        if (auto* p = dynamic_cast<Print*>(s)) { analyzeExpr(p->value.get()); return; }
        if (auto* e = dynamic_cast<ExprStmt*>(s)) { analyzeExpr(e->expr.get()); return; }
        if (auto* b = dynamic_cast<Block*>(s)) { analyzeBlock(*b); return; }
    }

    void requireCondition(Expr* e, int line) {
        Type t = analyzeExpr(e);
        if (!isScalar(t))
            throw CompileError("条件必须是标量类型，这里是 " + std::string(typeName(t)), line, 0);
    }

    static bool isConstExpr(Expr* e) {
        if (dynamic_cast<IntLit*>(e) || dynamic_cast<FloatLit*>(e) || dynamic_cast<BoolLit*>(e))
            return true;
        if (auto* b = dynamic_cast<Binary*>(e))
            return isConstExpr(b->lhs.get()) && isConstExpr(b->rhs.get());
        if (auto* u = dynamic_cast<Unary*>(e)) return isConstExpr(u->operand.get());
        return false;
    }

    static bool assignable(Type target, Type value) {
        if (target == value) return true;
        if (target == Type::Double && isNumeric(value)) return true;
        if (target == Type::Int && (value == Type::Bool || value == Type::Double)) return true;
        if (target == Type::Bool && isNumeric(value)) return true;
        return false;
    }

    static Type promote(Type a, Type b) {
        if (a == Type::Double || b == Type::Double) return Type::Double;
        if (a == Type::Int || b == Type::Int) return Type::Int;
        return Type::Bool;
    }

    // ---------------------------------------------------------- 表达式

    Type analyzeExpr(Expr* e) {
        if (auto* v = dynamic_cast<IntLit*>(e)) { e->type = Type::Int; return e->type; }
        if (auto* v = dynamic_cast<FloatLit*>(e)) { e->type = Type::Double; return e->type; }
        if (auto* v = dynamic_cast<BoolLit*>(e)) { e->type = Type::Bool; return e->type; }
        if (auto* v = dynamic_cast<StringLit*>(e)) { e->type = Type::String; return e->type; }

        if (auto* v = dynamic_cast<Var*>(e)) {
            VarInfo* info = useVar(v->name);
            if (!info) throw CompileError("变量 " + v->name + " 未声明", e->line, 0);
            e->type = info->type;
            return e->type;
        }

        if (auto* b = dynamic_cast<Binary*>(e)) {
            Type lt = analyzeExpr(b->lhs.get());
            Type rt = analyzeExpr(b->rhs.get());
            const std::string& op = b->op;

            if ((op == "/" || op == "%")) {
                auto* r = dynamic_cast<IntLit*>(b->rhs.get());
                if (r && r->value == 0) warn("W009", "除数是常量 0，运行时会产生未定义行为", e->line);
            }

            if (op == "+" || op == "-" || op == "*" || op == "/") {
                if (lt == Type::String || rt == Type::String) {
                    if (op == "+" && lt == Type::String && rt == Type::String) e->type = Type::String;
                    else throw CompileError("字符串不支持 " + op + " 运算", e->line, 0);
                } else if (!isNumeric(lt) || !isNumeric(rt)) {
                    throw CompileError("运算符 " + op + " 需要数值操作数", e->line, 0);
                } else {
                    e->type = promote(lt, rt);
                }
            } else if (op == "%") {
                if (!isNumeric(lt) || !isNumeric(rt))
                    throw CompileError("运算符 % 需要数值操作数", e->line, 0);
                e->type = (lt == Type::Int && rt == Type::Int) ? Type::Int : Type::Double;
            } else if (op == "<" || op == ">" || op == "<=" || op == ">=") {
                if (!isNumeric(lt) || !isNumeric(rt))
                    throw CompileError("比较运算符需要数值操作数", e->line, 0);
                e->type = Type::Bool;
            } else if (op == "==" || op == "!=") {
                e->type = Type::Bool;
            } else if (op == "&&" || op == "||") {
                if (!isScalar(lt) || !isScalar(rt))
                    throw CompileError("逻辑运算符需要标量操作数", e->line, 0);
                e->type = Type::Bool;
            } else {
                throw CompileError("不认识的运算符 " + op, e->line, 0);
            }
            return e->type;
        }

        if (auto* u = dynamic_cast<Unary*>(e)) {
            Type t = analyzeExpr(u->operand.get());
            if (u->op == "!") {
                if (!isScalar(t)) throw CompileError("! 需要标量操作数", e->line, 0);
                e->type = Type::Bool;
            } else {
                if (!isNumeric(t)) throw CompileError("- 需要数值操作数", e->line, 0);
                e->type = t;
            }
            return e->type;
        }

        if (auto* c = dynamic_cast<Call*>(e)) {
            // 内置输入函数：从标准输入读一个整数
            if (c->name == "input") {
                if (!c->args.empty())
                    throw CompileError("input() 不需要参数", e->line, 0);
                e->type = Type::Int;
                return e->type;
            }
            auto it = funcs_.find(c->name);
            if (it == funcs_.end())
                throw CompileError("函数 " + c->name + " 未定义", e->line, 0);
            called_.insert(c->name);
            if (c->args.size() != it->second.paramTypes.size()) {
                throw CompileError("函数 " + c->name + " 需要 " +
                                   std::to_string(it->second.paramTypes.size()) + " 个参数，这里给了 " +
                                   std::to_string(c->args.size()) + " 个", e->line, 0);
            }
            for (size_t i = 0; i < c->args.size(); ++i) {
                Type at = analyzeExpr(c->args[i].get());
                if (!assignable(it->second.paramTypes[i], at))
                    throw CompileError("函数 " + c->name + " 的第 " + std::to_string(i + 1) +
                                       " 个参数类型不匹配", e->line, 0);
            }
            e->type = it->second.ret;
            return e->type;
        }

        throw CompileError("不支持的表达式", e->line, 0);
    }
};

}  // namespace

void analyze(Program& prog, const Options& opt, std::vector<Diagnostic>& diags) {
    Analyzer(opt, diags).run(prog);
}

}  // namespace co2
