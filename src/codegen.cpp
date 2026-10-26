// C 代码生成后端：AST -> C 源码。
#include "core.h"

#include <cstdio>
#include <sstream>

namespace co2 {

namespace {

const char* ctype(Type t) {
    switch (t) {
        case Type::Int: return "int";
        case Type::Double: return "double";
        case Type::Bool: return "int";
        case Type::Void: return "void";
        case Type::String: return "const char*";
        default: return "int";
    }
}

std::string escapeC(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default: out += c;
        }
    }
    return out;
}

class Gen {
public:
    explicit Gen(const Options& opt) : opt_(opt) {}

    std::string run(const Program& prog, const std::string& header) {
        if (!header.empty()) out_ << "/* " << header << " */\n";
        out_ << "#include <stdio.h>\n";
        out_ << "\n/* 输入：读一个整数，读不到就当 0 */\n";
        out_ << "static int __co2_input(void) {\n";
        out_ << "    int v = 0;\n";
        out_ << "    if (scanf(\"%d\", &v) != 1) v = 0;\n";
        out_ << "    return v;\n";
        out_ << "}\n";
        if (opt_.waitMs > 0) out_ << "#include <time.h>\n";
        if (!opt_.defines.empty()) {
            out_ << "\n/* 命令行 -D 定义的宏 */\n";
            for (auto& kv : opt_.defines) {
                out_ << "#define " << kv.first << " " << kv.second << "\n";
            }
        }
        out_ << "\n";
        for (auto& fn : prog.functions) {
            if (!fn->body) continue;
            function(*fn);
        }
        return out_.str();
    }

private:
    const Options& opt_;
    std::ostringstream out_;
    int indent_ = 0;

    void pad() { for (int i = 0; i < indent_; ++i) out_ << "    "; }

    std::string expr(Expr* e) {
        if (auto* p = dynamic_cast<IntLit*>(e)) return std::to_string(p->value);
        if (auto* p = dynamic_cast<FloatLit*>(e)) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.17g", p->value);
            std::string s = buf;
            if (s.find('.') == std::string::npos && s.find('e') == std::string::npos &&
                s.find("inf") == std::string::npos && s.find("nan") == std::string::npos)
                s += ".0";
            return s;
        }
        if (auto* p = dynamic_cast<BoolLit*>(e)) return p->value ? "1" : "0";
        if (auto* p = dynamic_cast<StringLit*>(e)) return "\"" + escapeC(p->value) + "\"";
        if (auto* p = dynamic_cast<Var*>(e)) return p->name;
        if (auto* p = dynamic_cast<Binary*>(e))
            return "(" + expr(p->lhs.get()) + " " + p->op + " " + expr(p->rhs.get()) + ")";
        if (auto* p = dynamic_cast<Unary*>(e))
            return "(" + p->op + expr(p->operand.get()) + ")";
        if (auto* p = dynamic_cast<Call*>(e)) {
            if (p->name == "input") return "__co2_input()";
            std::string s = p->name + "(";
            for (size_t i = 0; i < p->args.size(); ++i) {
                if (i) s += ", ";
                s += expr(p->args[i].get());
            }
            return s + ")";
        }
        return "0";
    }

    std::string printCall(Expr* v) {
        Type t = v->type;
        std::string inner = expr(v);
        if (t == Type::String) return "printf(\"%s\\n\", " + inner + ");";
        if (t == Type::Bool) return "printf(\"%s\\n\", (" + inner + ") ? \"true\" : \"false\");";
        if (t == Type::Double) return "printf(\"%g\\n\", (double)(" + inner + "));";
        return "printf(\"%d\\n\", (int)(" + inner + "));";
    }

    void stmt(Stmt* s) {
        if (!s) return;

        if (auto* b = dynamic_cast<Block*>(s)) {
            pad(); out_ << "{\n";
            indent_++;
            for (auto& st : b->stmts) stmt(st.get());
            indent_--;
            pad(); out_ << "}\n";
            return;
        }
        if (auto* d = dynamic_cast<VarDecl*>(s)) {
            pad();
            out_ << ctype(d->vtype) << " " << d->name;
            if (d->init) out_ << " = " << expr(d->init.get());
            out_ << ";\n";
            return;
        }
        if (auto* a = dynamic_cast<Assign*>(s)) {
            pad(); out_ << a->target << " " << a->op << " " << expr(a->value.get()) << ";\n";
            return;
        }
        if (auto* i = dynamic_cast<If*>(s)) {
            pad(); out_ << "if (" << expr(i->cond.get()) << ")\n";
            stmt(i->thenStmt.get());
            if (i->elseStmt) {
                pad(); out_ << "else\n";
                stmt(i->elseStmt.get());
            }
            return;
        }
        if (auto* w = dynamic_cast<While*>(s)) {
            pad(); out_ << "while (" << expr(w->cond.get()) << ")\n";
            stmt(w->body.get());
            return;
        }
        if (auto* f = dynamic_cast<For*>(s)) {
            std::string init, cond, step;
            if (auto* d = dynamic_cast<VarDecl*>(f->init.get())) {
                init = std::string(ctype(d->vtype)) + " " + d->name;
                if (d->init) init += " = " + expr(d->init.get());
            } else if (auto* a = dynamic_cast<Assign*>(f->init.get())) {
                init = a->target + " " + a->op + " " + expr(a->value.get());
            }
            if (f->cond) cond = expr(f->cond.get());
            if (auto* a = dynamic_cast<Assign*>(f->step.get())) {
                step = a->target + " " + a->op + " " + expr(a->value.get());
            } else if (auto* e = dynamic_cast<ExprStmt*>(f->step.get())) {
                step = expr(e->expr.get());
            }
            pad(); out_ << "for (" << init << "; " << cond << "; " << step << ")\n";
            stmt(f->body.get());
            return;
        }
        if (auto* r = dynamic_cast<Return*>(s)) {
            pad();
            if (r->value) out_ << "return " << expr(r->value.get()) << ";\n";
            else out_ << "return;\n";
            return;
        }
        if (dynamic_cast<Break*>(s)) { pad(); out_ << "break;\n"; return; }
        if (dynamic_cast<Continue*>(s)) { pad(); out_ << "continue;\n"; return; }
        if (auto* p = dynamic_cast<Print*>(s)) { pad(); out_ << printCall(p->value.get()) << "\n"; return; }
        if (auto* e = dynamic_cast<ExprStmt*>(s)) { pad(); out_ << expr(e->expr.get()) << ";\n"; return; }
    }

    void function(const Function& fn) {
        out_ << ctype(fn.ret) << " " << fn.name << "(";
        if (fn.params.empty()) {
            out_ << "void";
        } else {
            for (size_t i = 0; i < fn.params.size(); ++i) {
                if (i) out_ << ", ";
                out_ << ctype(fn.params[i].type) << " " << fn.params[i].name;
            }
        }
        out_ << ")\n{\n";
        indent_ = 1;

        // 钴资源排队：空气里 Co2+ 只占 0.04%，用一点少一点
        if (opt_.waitMs > 0 && fn.name == "main") {
            pad();
            out_ << "/* cobalt queue: Co2+ is only 0.04% of air, waiting "
                 << opt_.waitMs << " ms */\n";
            pad();
            out_ << "{\n";
            indent_++;
            pad();
            out_ << "clock_t _co2t0 = clock();\n";
            pad();
            out_ << "clock_t _co2need = (clock_t)(" << opt_.waitMs
                 << " * CLOCKS_PER_SEC / 1000);\n";
            pad();
            out_ << "while (clock() - _co2t0 < _co2need) { }\n";
            indent_--;
            pad();
            out_ << "}\n";
        }

        for (auto& st : fn.body->stmts) stmt(st.get());
        // 补隐式返回
        bool endsWithReturn = false;
        if (!fn.body->stmts.empty())
            endsWithReturn = dynamic_cast<Return*>(fn.body->stmts.back().get()) != nullptr;
        if (fn.ret != Type::Void && !endsWithReturn) {
            pad();
            out_ << "return " << (fn.ret == Type::Double ? "0.0" : "0") << ";\n";
        }
        indent_ = 0;
        out_ << "}\n\n";
    }
};

}  // namespace

std::string generateC(const Program& prog, const Options& opt, const std::string& header) {
    return Gen(opt).run(prog, header);
}

}  // namespace co2
