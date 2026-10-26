// 优化流水线：常量折叠、代数化简、死代码消除、常量传播、小函数内联。
// 每个 pass 都是真实的 AST 变换，等级由大气浓度决定。
#include "core.h"

#include <cmath>
#include <functional>
#include <map>
#include <set>

namespace co2 {

namespace {

struct Value {
    Type type = Type::Int;
    long long i = 0;
    double d = 0;
    bool b = false;
};

bool isLiteral(Expr* e) {
    return dynamic_cast<IntLit*>(e) || dynamic_cast<FloatLit*>(e) || dynamic_cast<BoolLit*>(e);
}

Value literalValue(Expr* e) {
    Value v;
    if (auto* p = dynamic_cast<IntLit*>(e)) { v.type = Type::Int; v.i = p->value; }
    else if (auto* p = dynamic_cast<FloatLit*>(e)) { v.type = Type::Double; v.d = p->value; }
    else if (auto* p = dynamic_cast<BoolLit*>(e)) { v.type = Type::Bool; v.b = p->value; }
    return v;
}

ExprPtr makeLiteral(const Value& v, int line) {
    if (v.type == Type::Double) {
        auto e = std::make_unique<FloatLit>();
        e->line = line;
        e->value = v.d;
        e->type = Type::Double;
        return e;
    }
    if (v.type == Type::Bool) {
        auto e = std::make_unique<BoolLit>();
        e->line = line;
        e->value = v.b;
        e->type = Type::Bool;
        return e;
    }
    auto e = std::make_unique<IntLit>();
    e->line = line;
    e->value = v.i;
    e->type = Type::Int;
    return e;
}

double asDouble(const Value& v) {
    if (v.type == Type::Double) return v.d;
    if (v.type == Type::Bool) return v.b ? 1.0 : 0.0;
    return (double)v.i;
}

bool evalBinop(const std::string& op, const Value& a, const Value& b, Type resultType, Value& out) {
    bool useFloat = (resultType == Type::Double) || a.type == Type::Double || b.type == Type::Double;

    if (op == "+" || op == "-" || op == "*") {
        if (useFloat) {
            double x = asDouble(a), y = asDouble(b);
            out.type = Type::Double;
            out.d = (op == "+") ? x + y : (op == "-") ? x - y : x * y;
        } else {
            out.type = Type::Int;
            out.i = (op == "+") ? a.i + b.i : (op == "-") ? a.i - b.i : a.i * b.i;
        }
        return true;
    }
    if (op == "/") {
        if (useFloat) {
            double y = asDouble(b);
            if (y == 0.0) return false;
            out.type = Type::Double;
            out.d = asDouble(a) / y;
        } else {
            if (b.i == 0) return false;
            out.type = Type::Int;
            out.i = a.i / b.i;   // C 的整数除法就是向零截断
        }
        return true;
    }
    if (op == "%") {
        if (useFloat || b.i == 0) return false;
        out.type = Type::Int;
        out.i = a.i % b.i;
        return true;
    }
    if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
        bool r;
        if (useFloat) {
            double x = asDouble(a), y = asDouble(b);
            r = (op == "==") ? x == y : (op == "!=") ? x != y : (op == "<") ? x < y
              : (op == ">") ? x > y : (op == "<=") ? x <= y : x >= y;
        } else {
            long long x = a.i, y = b.i;
            if (a.type == Type::Bool) x = a.b ? 1 : 0;
            if (b.type == Type::Bool) y = b.b ? 1 : 0;
            r = (op == "==") ? x == y : (op == "!=") ? x != y : (op == "<") ? x < y
              : (op == ">") ? x > y : (op == "<=") ? x <= y : x >= y;
        }
        out.type = Type::Bool;
        out.b = r;
        return true;
    }
    if (op == "&&" || op == "||") {
        bool x = (a.type == Type::Bool) ? a.b : (a.type == Type::Double ? a.d != 0 : a.i != 0);
        bool y = (b.type == Type::Bool) ? b.b : (b.type == Type::Double ? b.d != 0 : b.i != 0);
        out.type = Type::Bool;
        out.b = (op == "&&") ? (x && y) : (x || y);
        return true;
    }
    return false;
}

bool hasSideEffect(Expr* e) {
    if (!e) return false;
    if (dynamic_cast<Call*>(e)) return true;
    if (auto* b = dynamic_cast<Binary*>(e)) return hasSideEffect(b->lhs.get()) || hasSideEffect(b->rhs.get());
    if (auto* u = dynamic_cast<Unary*>(e)) return hasSideEffect(u->operand.get());
    return false;
}

// ---------------------------------------------------------------- pass 1

int foldExpr(ExprPtr& e) {
    if (!e) return 0;
    int n = 0;

    if (auto* b = dynamic_cast<Binary*>(e.get())) {
        n += foldExpr(b->lhs);
        n += foldExpr(b->rhs);
        b = dynamic_cast<Binary*>(e.get());
        if (isLiteral(b->lhs.get()) && isLiteral(b->rhs.get())) {
            Value out;
            if (evalBinop(b->op, literalValue(b->lhs.get()), literalValue(b->rhs.get()),
                          b->type, out)) {
                e = makeLiteral(out, b->line);
                return n + 1;
            }
        }
    } else if (auto* u = dynamic_cast<Unary*>(e.get())) {
        n += foldExpr(u->operand);
        u = dynamic_cast<Unary*>(e.get());
        if (isLiteral(u->operand.get())) {
            Value v = literalValue(u->operand.get());
            if (u->op == "!") {
                Value out;
                out.type = Type::Bool;
                out.b = !(v.type == Type::Bool ? v.b : (v.type == Type::Double ? v.d != 0 : v.i != 0));
                e = makeLiteral(out, u->line);
                return n + 1;
            }
            if (u->op == "-") {
                Value out = v;
                if (v.type == Type::Double) out.d = -v.d; else out.i = -v.i;
                e = makeLiteral(out, u->line);
                return n + 1;
            }
        }
    }
    return n;
}

// ---------------------------------------------------------------- pass 2

int simplifyExpr(ExprPtr& e) {
    if (!e) return 0;
    int n = 0;
    if (auto* b = dynamic_cast<Binary*>(e.get())) {
        n += simplifyExpr(b->lhs);
        n += simplifyExpr(b->rhs);
        b = dynamic_cast<Binary*>(e.get());
        auto* li = dynamic_cast<IntLit*>(b->lhs.get());
        auto* ri = dynamic_cast<IntLit*>(b->rhs.get());
        bool lzero = li && li->value == 0;
        bool rzero = ri && ri->value == 0;
        bool lone = li && li->value == 1;
        bool rone = ri && ri->value == 1;

        if (b->op == "+" && rzero) { e = std::move(b->lhs); return n + 1; }
        if (b->op == "+" && lzero) { e = std::move(b->rhs); return n + 1; }
        if (b->op == "-" && rzero) { e = std::move(b->lhs); return n + 1; }
        if (b->op == "*" && rone) { e = std::move(b->lhs); return n + 1; }
        if (b->op == "*" && lone) { e = std::move(b->rhs); return n + 1; }
        if (b->op == "/" && rone) { e = std::move(b->lhs); return n + 1; }
        if (b->op == "*" && (lzero || rzero) && !hasSideEffect(b->lhs.get()) &&
            !hasSideEffect(b->rhs.get())) {
            auto z = std::make_unique<IntLit>();
            z->line = b->line;
            z->value = 0;
            z->type = Type::Int;
            e = std::move(z);
            return n + 1;
        }
        if (b->op == "&&") {
            if (auto* rb = dynamic_cast<BoolLit*>(b->rhs.get())) {
                if (rb->value) { e = std::move(b->lhs); return n + 1; }
            }
            if (auto* lb = dynamic_cast<BoolLit*>(b->lhs.get())) {
                if (lb->value) { e = std::move(b->rhs); return n + 1; }
            }
        }
        if (b->op == "||") {
            if (auto* rb = dynamic_cast<BoolLit*>(b->rhs.get())) {
                if (!rb->value) { e = std::move(b->lhs); return n + 1; }
            }
            if (auto* lb = dynamic_cast<BoolLit*>(b->lhs.get())) {
                if (!lb->value) { e = std::move(b->rhs); return n + 1; }
            }
        }
    } else if (auto* u = dynamic_cast<Unary*>(e.get())) {
        n += simplifyExpr(u->operand);
        u = dynamic_cast<Unary*>(e.get());
        if (u->op == "!") {
            if (auto* inner = dynamic_cast<Unary*>(u->operand.get())) {
                if (inner->op == "!") { e = std::move(inner->operand); return n + 1; }
            }
        }
    }
    return n;
}

// ---------------------------------------------------------------- pass 3

int eliminateDeadCode(Stmt* s);

int trimBlock(Block& b) {
    int n = 0;
    std::vector<StmtPtr> kept;
    for (auto& st : b.stmts) {
        bool terminal = dynamic_cast<Return*>(st.get()) || dynamic_cast<Break*>(st.get()) ||
                        dynamic_cast<Continue*>(st.get());
        kept.push_back(std::move(st));
        if (terminal) {
            n += (int)(b.stmts.size() - kept.size());
            break;
        }
    }
    b.stmts = std::move(kept);
    for (auto& st : b.stmts) n += eliminateDeadCode(st.get());
    return n;
}

int eliminateDeadCode(Stmt* s) {
    if (!s) return 0;
    if (auto* b = dynamic_cast<Block*>(s)) return trimBlock(*b);
    if (auto* i = dynamic_cast<If*>(s)) {
        int n = 0;
        if (auto* t = dynamic_cast<Block*>(i->thenStmt.get())) n += trimBlock(*t);
        if (i->elseStmt) {
            if (auto* e = dynamic_cast<Block*>(i->elseStmt.get())) n += trimBlock(*e);
            else n += eliminateDeadCode(i->elseStmt.get());
        }
        return n;
    }
    if (auto* w = dynamic_cast<While*>(s)) {
        if (auto* b = dynamic_cast<Block*>(w->body.get())) return trimBlock(*b);
    }
    if (auto* f = dynamic_cast<For*>(s)) {
        if (auto* b = dynamic_cast<Block*>(f->body.get())) return trimBlock(*b);
    }
    return 0;
}

int foldConstantBranches(Stmt* s);

int foldBranchesInBlock(Block& b) {
    int n = 0;
    std::vector<StmtPtr> out;
    for (auto& st : b.stmts) {
        if (auto* i = dynamic_cast<If*>(st.get())) {
            if (auto* c = dynamic_cast<BoolLit*>(i->cond.get())) {
                n++;
                if (c->value) {
                    auto* tb = dynamic_cast<Block*>(i->thenStmt.get());
                    if (tb) for (auto& x : tb->stmts) out.push_back(std::move(x));
                } else if (i->elseStmt) {
                    auto* eb = dynamic_cast<Block*>(i->elseStmt.get());
                    if (eb) for (auto& x : eb->stmts) out.push_back(std::move(x));
                    else out.push_back(std::move(i->elseStmt));
                }
                continue;
            }
        }
        if (auto* w = dynamic_cast<While*>(st.get())) {
            if (auto* c = dynamic_cast<BoolLit*>(w->cond.get())) {
                if (!c->value) { n++; continue; }
            }
        }
        n += foldConstantBranches(st.get());
        out.push_back(std::move(st));
    }
    b.stmts = std::move(out);
    return n;
}

int foldConstantBranches(Stmt* s) {
    if (!s) return 0;
    if (auto* b = dynamic_cast<Block*>(s)) return foldBranchesInBlock(*b);
    if (auto* i = dynamic_cast<If*>(s)) {
        int n = 0;
        if (auto* t = dynamic_cast<Block*>(i->thenStmt.get())) n += foldBranchesInBlock(*t);
        if (i->elseStmt) {
            if (auto* e = dynamic_cast<Block*>(i->elseStmt.get())) n += foldBranchesInBlock(*e);
        }
        return n;
    }
    if (auto* w = dynamic_cast<While*>(s)) {
        if (auto* b = dynamic_cast<Block*>(w->body.get())) return foldBranchesInBlock(*b);
    }
    if (auto* f = dynamic_cast<For*>(s)) {
        if (auto* b = dynamic_cast<Block*>(f->body.get())) return foldBranchesInBlock(*b);
    }
    return 0;
}

// ---------------------------------------------------------------- pass 4

struct ConstEnv {
    std::map<std::string, std::pair<Value, Type>> map;
};

void substituteExpr(ExprPtr& e, const ConstEnv& env, int& changes);

void substituteStmt(Stmt* s, const ConstEnv& env, int& changes) {
    if (!s) return;
    if (auto* d = dynamic_cast<VarDecl*>(s)) { if (d->init) substituteExpr(d->init, env, changes); return; }
    if (auto* a = dynamic_cast<Assign*>(s)) { substituteExpr(a->value, env, changes); return; }
    if (auto* i = dynamic_cast<If*>(s)) {
        substituteExpr(i->cond, env, changes);
        if (auto* b = dynamic_cast<Block*>(i->thenStmt.get()))
            for (auto& st : b->stmts) substituteStmt(st.get(), env, changes);
        if (i->elseStmt) {
            if (auto* b = dynamic_cast<Block*>(i->elseStmt.get()))
                for (auto& st : b->stmts) substituteStmt(st.get(), env, changes);
            else substituteStmt(i->elseStmt.get(), env, changes);
        }
        return;
    }
    if (auto* w = dynamic_cast<While*>(s)) {
        substituteExpr(w->cond, env, changes);
        if (auto* b = dynamic_cast<Block*>(w->body.get()))
            for (auto& st : b->stmts) substituteStmt(st.get(), env, changes);
        return;
    }
    if (auto* f = dynamic_cast<For*>(s)) {
        if (f->init) substituteStmt(f->init.get(), env, changes);
        if (f->cond) substituteExpr(f->cond, env, changes);
        if (f->step) substituteStmt(f->step.get(), env, changes);
        if (auto* b = dynamic_cast<Block*>(f->body.get()))
            for (auto& st : b->stmts) substituteStmt(st.get(), env, changes);
        return;
    }
    if (auto* r = dynamic_cast<Return*>(s)) { if (r->value) substituteExpr(r->value, env, changes); return; }
    if (auto* p = dynamic_cast<Print*>(s)) { substituteExpr(p->value, env, changes); return; }
    if (auto* e = dynamic_cast<ExprStmt*>(s)) { substituteExpr(e->expr, env, changes); return; }
    if (auto* b = dynamic_cast<Block*>(s))
        for (auto& st : b->stmts) substituteStmt(st.get(), env, changes);
}

void substituteExpr(ExprPtr& e, const ConstEnv& env, int& changes) {
    if (!e) return;
    if (auto* v = dynamic_cast<Var*>(e.get())) {
        auto it = env.map.find(v->name);
        if (it != env.map.end()) {
            Value val = it->second.first;
            Type declared = it->second.second;
            if (declared == Type::Double && val.type != Type::Double) {
                val.type = Type::Double;
                val.d = asDouble(it->second.first);
            }
            e = makeLiteral(val, v->line);
            changes++;
        }
        return;
    }
    if (auto* b = dynamic_cast<Binary*>(e.get())) {
        substituteExpr(b->lhs, env, changes);
        substituteExpr(b->rhs, env, changes);
        return;
    }
    if (auto* u = dynamic_cast<Unary*>(e.get())) { substituteExpr(u->operand, env, changes); return; }
    if (auto* c = dynamic_cast<Call*>(e.get()))
        for (auto& a : c->args) substituteExpr(a, env, changes);
}

void collectAssigned(Stmt* s, std::set<std::string>& out) {
    if (!s) return;
    if (auto* a = dynamic_cast<Assign*>(s)) out.insert(a->target);
    if (auto* d = dynamic_cast<VarDecl*>(s)) out.insert(d->name);
    if (auto* b = dynamic_cast<Block*>(s)) for (auto& st : b->stmts) collectAssigned(st.get(), out);
    if (auto* i = dynamic_cast<If*>(s)) {
        collectAssigned(i->thenStmt.get(), out);
        collectAssigned(i->elseStmt.get(), out);
    }
    if (auto* w = dynamic_cast<While*>(s)) collectAssigned(w->body.get(), out);
    if (auto* f = dynamic_cast<For*>(s)) {
        collectAssigned(f->init.get(), out);
        collectAssigned(f->step.get(), out);
        collectAssigned(f->body.get(), out);
    }
}

int propagateInBlock(Block& b) {
    int changes = 0;
    ConstEnv env;
    for (auto& st : b.stmts) {
        if (auto* d = dynamic_cast<VarDecl*>(st.get())) {
            if (d->init && isLiteral(d->init.get())) env.map[d->name] = {literalValue(d->init.get()), d->vtype};
            else env.map.erase(d->name);
        } else if (auto* a = dynamic_cast<Assign*>(st.get())) {
            env.map.erase(a->target);
        } else if (dynamic_cast<If*>(st.get()) || dynamic_cast<While*>(st.get()) ||
                   dynamic_cast<For*>(st.get())) {
            std::set<std::string> assigned;
            collectAssigned(st.get(), assigned);
            for (auto& n : assigned) env.map.erase(n);
        }
        if (!env.map.empty()) substituteStmt(st.get(), env, changes);
    }
    return changes;
}

// ---------------------------------------------------------------- 内联

int inlineCalls(Program& prog) {
    std::map<std::string, Function*> bodies;
    for (auto& fn : prog.functions)
        if (fn->body) bodies[fn->name] = fn.get();

    int changes = 0;
    for (auto& fn : prog.functions) {
        if (!fn->body) continue;
        std::function<void(Stmt*)> walkStmt = [&](Stmt* s) {
            if (!s) return;
            std::function<void(ExprPtr&)> walkExpr = [&](ExprPtr& e) {
                if (!e) return;
                if (auto* b = dynamic_cast<Binary*>(e.get())) { walkExpr(b->lhs); walkExpr(b->rhs); return; }
                if (auto* u = dynamic_cast<Unary*>(e.get())) { walkExpr(u->operand); return; }
                if (auto* c = dynamic_cast<Call*>(e.get())) {
                    for (auto& a : c->args) walkExpr(a);
                    auto it = bodies.find(c->name);
                    if (it == bodies.end()) return;
                    auto* target = it->second;
                    if (target == fn.get()) return;                       // 不内联自己
                    if (target->body->stmts.size() != 1) return;
                    auto* ret = dynamic_cast<Return*>(target->body->stmts[0].get());
                    if (!ret || !ret->value) return;
                    if (target->params.size() != c->args.size()) return;
                    for (auto& a : c->args) if (hasSideEffect(a.get())) return;
                    if (hasSideEffect(ret->value.get())) return;

                    // 形参替换为实参
                    std::map<std::string, Expr*> subst;
                    for (size_t i = 0; i < c->args.size(); ++i)
                        subst[target->params[i].name] = c->args[i].get();

                    std::function<ExprPtr(Expr*)> clone = [&](Expr* src) -> ExprPtr {
                        if (auto* v = dynamic_cast<Var*>(src)) {
                            auto sit = subst.find(v->name);
                            if (sit != subst.end()) {
                                // 直接复制实参的表达式
                                std::function<ExprPtr(Expr*)> copy = [&](Expr* s2) -> ExprPtr {
                                    if (auto* p = dynamic_cast<IntLit*>(s2)) {
                                        auto n = std::make_unique<IntLit>();
                                        n->value = p->value; n->line = p->line; n->type = p->type;
                                        return n;
                                    }
                                    if (auto* p = dynamic_cast<FloatLit*>(s2)) {
                                        auto n = std::make_unique<FloatLit>();
                                        n->value = p->value; n->line = p->line; n->type = p->type;
                                        return n;
                                    }
                                    if (auto* p = dynamic_cast<BoolLit*>(s2)) {
                                        auto n = std::make_unique<BoolLit>();
                                        n->value = p->value; n->line = p->line; n->type = p->type;
                                        return n;
                                    }
                                    if (auto* p = dynamic_cast<Var*>(s2)) {
                                        auto n = std::make_unique<Var>();
                                        n->name = p->name; n->line = p->line; n->type = p->type;
                                        return n;
                                    }
                                    return nullptr;
                                };
                                auto copied = copy(sit->second);
                                if (copied) return copied;
                            }
                        }
                        if (auto* p = dynamic_cast<IntLit*>(src)) {
                            auto n = std::make_unique<IntLit>();
                            n->value = p->value; n->line = p->line; n->type = p->type;
                            return n;
                        }
                        if (auto* p = dynamic_cast<FloatLit*>(src)) {
                            auto n = std::make_unique<FloatLit>();
                            n->value = p->value; n->line = p->line; n->type = p->type;
                            return n;
                        }
                        if (auto* p = dynamic_cast<BoolLit*>(src)) {
                            auto n = std::make_unique<BoolLit>();
                            n->value = p->value; n->line = p->line; n->type = p->type;
                            return n;
                        }
                        if (auto* p = dynamic_cast<Var*>(src)) {
                            auto n = std::make_unique<Var>();
                            n->name = p->name; n->line = p->line; n->type = p->type;
                            return n;
                        }
                        if (auto* p = dynamic_cast<Binary*>(src)) {
                            auto n = std::make_unique<Binary>();
                            n->op = p->op; n->line = p->line; n->type = p->type;
                            n->lhs = clone(p->lhs.get());
                            n->rhs = clone(p->rhs.get());
                            return n;
                        }
                        if (auto* p = dynamic_cast<Unary*>(src)) {
                            auto n = std::make_unique<Unary>();
                            n->op = p->op; n->line = p->line; n->type = p->type;
                            n->operand = clone(p->operand.get());
                            return n;
                        }
                        return nullptr;
                    };

                    auto replaced = clone(ret->value.get());
                    if (replaced) { e = std::move(replaced); changes++; }
                    return;
                }
            };

            if (auto* d = dynamic_cast<VarDecl*>(s)) walkExpr(d->init);
            else if (auto* a = dynamic_cast<Assign*>(s)) walkExpr(a->value);
            else if (auto* i = dynamic_cast<If*>(s)) {
                walkExpr(i->cond);
                walkStmt(i->thenStmt.get());
                walkStmt(i->elseStmt.get());
            }
            else if (auto* w = dynamic_cast<While*>(s)) { walkExpr(w->cond); walkStmt(w->body.get()); }
            else if (auto* f = dynamic_cast<For*>(s)) {
                walkStmt(f->init.get());
                walkExpr(f->cond);
                walkStmt(f->step.get());
                walkStmt(f->body.get());
            }
            else if (auto* r = dynamic_cast<Return*>(s)) walkExpr(r->value);
            else if (auto* p = dynamic_cast<Print*>(s)) walkExpr(p->value);
            else if (auto* e2 = dynamic_cast<ExprStmt*>(s)) walkExpr(e2->expr);
            else if (auto* b = dynamic_cast<Block*>(s)) for (auto& st : b->stmts) walkStmt(st.get());
        };
        walkStmt(fn->body.get());
    }
    return changes;
}

}  // namespace

void optimizeProgram(Program& prog, double level,
                     std::vector<std::pair<std::string, int>>& stats) {
    auto add = [&](const std::string& name, int n) {
        if (n <= 0) return;
        for (auto& kv : stats)
            if (kv.first == name) { kv.second += n; return; }
        stats.push_back({name, n});
    };

    if (level < 0.5) return;

    int total = 0;
    for (auto& fn : prog.functions) {
        if (!fn->body) continue;
        for (auto& st : fn->body->stmts) {
            if (auto* d = dynamic_cast<VarDecl*>(st.get())) total += foldExpr(d->init);
            else if (auto* a = dynamic_cast<Assign*>(st.get())) total += foldExpr(a->value);
            else if (auto* r = dynamic_cast<Return*>(st.get())) total += foldExpr(r->value);
            else if (auto* p = dynamic_cast<Print*>(st.get())) total += foldExpr(p->value);
            else if (auto* e = dynamic_cast<ExprStmt*>(st.get())) total += foldExpr(e->expr);
            else if (auto* i = dynamic_cast<If*>(st.get())) total += foldExpr(i->cond);
        }
    }
    add("常量折叠", total);

    if (level >= 1.5) {
        int n = 0;
        for (auto& fn : prog.functions) {
            if (!fn->body) continue;
            for (auto& st : fn->body->stmts) {
                if (auto* d = dynamic_cast<VarDecl*>(st.get())) n += simplifyExpr(d->init);
                else if (auto* a = dynamic_cast<Assign*>(st.get())) n += simplifyExpr(a->value);
                else if (auto* r = dynamic_cast<Return*>(st.get())) n += simplifyExpr(r->value);
                else if (auto* p = dynamic_cast<Print*>(st.get())) n += simplifyExpr(p->value);
                else if (auto* e = dynamic_cast<ExprStmt*>(st.get())) n += simplifyExpr(e->expr);
                else if (auto* i = dynamic_cast<If*>(st.get())) n += simplifyExpr(i->cond);
            }
        }
        add("代数化简", n);

        int d = 0;
        for (auto& fn : prog.functions)
            if (fn->body) d += eliminateDeadCode(fn->body.get());
        add("死代码消除", d);
    }

    if (level >= 2.0) {
        int n = 0;
        for (auto& fn : prog.functions)
            if (fn->body) n += foldConstantBranches(fn->body.get());
        add("分支折叠", n);
    }

    if (level >= 2.5) {
        int n = 0;
        for (auto& fn : prog.functions)
            if (fn->body) n += propagateInBlock(*fn->body);
        add("常量传播", n);

        // 传播后再折一次
        int f = 0;
        for (auto& fn : prog.functions) {
            if (!fn->body) continue;
            for (auto& st : fn->body->stmts) {
                if (auto* d = dynamic_cast<VarDecl*>(st.get())) f += foldExpr(d->init);
                else if (auto* a = dynamic_cast<Assign*>(st.get())) f += foldExpr(a->value);
                else if (auto* r = dynamic_cast<Return*>(st.get())) f += foldExpr(r->value);
                else if (auto* p = dynamic_cast<Print*>(st.get())) f += foldExpr(p->value);
                else if (auto* i = dynamic_cast<If*>(st.get())) f += foldExpr(i->cond);
            }
        }
        add("常量折叠", f);
    }

    if (level >= 3.0) {
        add("小函数内联", inlineCalls(prog));
    }
}

}  // namespace co2
