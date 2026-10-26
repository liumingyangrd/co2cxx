// 手写递归下降解析器：Token 流 -> AST。表达式用优先级爬升。
#include "core.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>

namespace co2 {

namespace {

int stdYear(const std::string& s) {
    if (s == "c++98") return 1998;
    if (s == "c++03") return 2003;
    if (s == "c++11") return 2011;
    if (s == "c++14") return 2014;
    if (s == "c++17") return 2017;
    if (s == "c++20") return 2020;
    return 2017;
}

bool isTypeKeyword(const std::string& s) {
    return s == "int" || s == "double" || s == "bool" || s == "void" || s == "auto";
}

Type typeFromKeyword(const std::string& s) {
    if (s == "int") return Type::Int;
    if (s == "double") return Type::Double;
    if (s == "bool") return Type::Bool;
    if (s == "void") return Type::Void;
    return Type::Unknown;  // auto
}

int precedence(const std::string& op) {
    if (op == "||") return 1;
    if (op == "&&") return 2;
    if (op == "==" || op == "!=") return 3;
    if (op == "<" || op == ">" || op == "<=" || op == ">=") return 4;
    if (op == "+" || op == "-") return 5;
    if (op == "*" || op == "/" || op == "%") return 6;
    return 0;
}

class Parser {
public:
    Parser(std::vector<Token> toks, std::string standard)
        : toks_(std::move(toks)), std_(std::move(standard)) {}

    std::unique_ptr<Program> run() {
        auto prog = std::make_unique<Program>();
        while (cur().kind != Tok::Eof) prog->functions.push_back(function());
        return prog;
    }

private:
    std::vector<Token> toks_;
    std::string std_;
    size_t pos_ = 0;

    const Token& cur() const { return toks_[pos_]; }
    const Token& ahead(size_t n = 1) const {
        size_t i = pos_ + n;
        return i < toks_.size() ? toks_[i] : toks_.back();
    }

    const Token& advance() {
        const Token& t = toks_[pos_];
        if (t.kind != Tok::Eof) pos_++;
        return t;
    }

    bool check(const std::string& v) const {
        return cur().kind != Tok::Eof && cur().text == v;
    }

    bool match(const std::string& v) {
        if (check(v)) { advance(); return true; }
        return false;
    }

    const Token& expect(const std::string& v, const char* what) {
        if (check(v)) return advance();
        throw CompileError(std::string("这里应该是 ") + what + "，但读到了 " + cur().text,
                           cur().line, cur().col);
    }

    const Token& expectIdent(const char* what) {
        if (cur().kind == Tok::Ident) return advance();
        throw CompileError(std::string("这里应该是") + what + "，但读到了 " + cur().text,
                           cur().line, cur().col);
    }

    void requireFeature(const char* feature, int year, int line) {
        if (stdYear(std_) < year) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "%s 是 C++%d 才引入的特性，当前标准 %s 不支持。试试 --std=c++%d",
                          feature, year % 100, std_.c_str(), year % 100);
            throw CompileError(buf, line, 0);
        }
    }

    Type parseType(int* lineOut = nullptr) {
        if (cur().kind == Tok::Keyword && isTypeKeyword(cur().text)) {
            const Token& t = advance();
            if (lineOut) *lineOut = t.line;
            if (t.text == "auto") requireFeature("auto 类型推导", 2011, t.line);
            return typeFromKeyword(t.text);
        }
        throw CompileError("这里应该是类型名（int/double/bool/void/auto），但读到了 " + cur().text,
                           cur().line, cur().col);
    }

    // ------------------------------------------------------------ 函数

    std::unique_ptr<Function> function() {
        auto fn = std::make_unique<Function>();

        if (check("constexpr")) {
            const Token& kw = advance();
            requireFeature("constexpr", 2011, kw.line);
            fn->isConstexpr = true;
        }

        int line = 0;
        fn->ret = parseType(&line);
        fn->line = line;

        const Token& name = expectIdent("函数名");
        fn->name = name.text;
        expect("(", "左括号");

        if (!check(")")) {
            for (;;) {
                Param p;
                p.type = parseType();
                p.name = expectIdent("参数名").text;
                fn->params.push_back(p);
                if (!match(",")) break;
            }
        }
        expect(")", "右括号");

        if (match(";")) return fn;   // 前置声明

        fn->body = block();
        return fn;
    }

    std::unique_ptr<Block> block() {
        const Token& open = expect("{", "左花括号");
        auto b = std::make_unique<Block>();
        b->line = open.line;
        while (!check("}")) {
            if (cur().kind == Tok::Eof)
                throw CompileError("代码块没有闭合，文件就结束了", open.line, open.col);
            b->stmts.push_back(statement());
        }
        expect("}", "右花括号");
        return b;
    }

    // ------------------------------------------------------------ 语句

    StmtPtr statement() {
        const Token& t = cur();

        if (t.kind == Tok::Keyword && isTypeKeyword(t.text)) return varDecl(true);

        if (t.kind == Tok::Keyword) {
            if (t.text == "if") return ifStmt();
            if (t.text == "while") return whileStmt();
            if (t.text == "for") return forStmt();
            if (t.text == "return") return returnStmt();
            if (t.text == "break") { advance(); expect(";", "分号"); return std::make_unique<Break>(); }
            if (t.text == "continue") { advance(); expect(";", "分号"); return std::make_unique<Continue>(); }
            if (t.text == "print") return printStmt();
        }

        if (check("{")) return block();

        auto st = simpleStatement();
        expect(";", "分号");
        return st;
    }

    StmtPtr varDecl(bool withSemicolon) {
        auto d = std::make_unique<VarDecl>();
        int line = 0;
        d->vtype = parseType(&line);
        d->line = line;
        d->isAuto = (d->vtype == Type::Unknown);
        d->name = expectIdent("变量名").text;
        if (match("=")) d->init = expression();
        if (withSemicolon) expect(";", "分号");
        return d;
    }

    StmtPtr ifStmt() {
        const Token& kw = expect("if", "if");
        auto s = std::make_unique<If>();
        s->line = kw.line;
        if (check("constexpr")) {
            const Token& c = advance();
            requireFeature("if constexpr", 2017, c.line);
            s->isConstexpr = true;
        }
        expect("(", "左括号");
        s->cond = expression();
        expect(")", "右括号");
        s->thenStmt = block();
        if (match("else")) {
            s->elseStmt = check("if") ? ifStmt() : block();
        }
        return s;
    }

    StmtPtr whileStmt() {
        const Token& kw = expect("while", "while");
        auto s = std::make_unique<While>();
        s->line = kw.line;
        expect("(", "左括号");
        s->cond = expression();
        expect(")", "右括号");
        s->body = block();
        return s;
    }

    StmtPtr forStmt() {
        const Token& kw = expect("for", "for");
        auto s = std::make_unique<For>();
        s->line = kw.line;
        expect("(", "左括号");

        if (!check(";")) {
            if (cur().kind == Tok::Keyword && isTypeKeyword(cur().text)) {
                s->init = varDecl(false);
            } else {
                s->init = simpleStatement();
            }
        }
        expect(";", "分号");

        if (!check(";")) s->cond = expression();
        expect(";", "分号");

        if (!check(")")) s->step = simpleStatement();
        expect(")", "右括号");

        s->body = block();
        return s;
    }

    StmtPtr returnStmt() {
        const Token& kw = expect("return", "return");
        auto s = std::make_unique<Return>();
        s->line = kw.line;
        if (!check(";")) s->value = expression();
        expect(";", "分号");
        return s;
    }

    StmtPtr printStmt() {
        const Token& kw = expect("print", "print");
        auto s = std::make_unique<Print>();
        s->line = kw.line;
        expect("(", "左括号");
        s->value = expression();
        expect(")", "右括号");
        expect(";", "分号");
        return s;
    }

    StmtPtr simpleStatement() {
        if (cur().kind == Tok::Ident &&
            (ahead().text == "=" || ahead().text == "+=" || ahead().text == "-=")) {
            const Token& name = advance();
            const Token& op = advance();
            auto s = std::make_unique<Assign>();
            s->line = name.line;
            s->target = name.text;
            s->op = op.text;
            s->value = expression();
            return s;
        }
        auto s = std::make_unique<ExprStmt>();
        s->line = cur().line;
        s->expr = expression();
        return s;
    }

    // ------------------------------------------------------------ 表达式

    ExprPtr expression() {
        if (cur().kind == Tok::Ident &&
            (ahead().text == "=" || ahead().text == "+=" || ahead().text == "-=")) {
            // 赋值表达式（这里当成表达式语句处理，简化）
        }
        return binary(1);
    }

    ExprPtr binary(int minPrec) {
        ExprPtr lhs = unary();
        for (;;) {
            if (cur().kind != Tok::Op) break;
            int prec = precedence(cur().text);
            if (prec == 0 || prec < minPrec) break;
            const Token& op = advance();
            ExprPtr rhs = binary(prec + 1);
            auto b = std::make_unique<Binary>();
            b->line = op.line;
            b->op = op.text;
            b->lhs = std::move(lhs);
            b->rhs = std::move(rhs);
            lhs = std::move(b);
        }
        return lhs;
    }

    ExprPtr unary() {
        if (cur().kind == Tok::Op &&
            (cur().text == "!" || cur().text == "-" || cur().text == "+")) {
            const Token& op = advance();
            if (op.text == "+") return unary();
            auto u = std::make_unique<Unary>();
            u->line = op.line;
            u->op = op.text;
            u->operand = unary();
            return u;
        }
        return postfix();
    }

    ExprPtr postfix() {
        ExprPtr e = primary();
        while (check("(")) {
            auto* v = dynamic_cast<Var*>(e.get());
            if (!v) throw CompileError("只有函数名可以被调用", cur().line, cur().col);
            const Token& open = advance();
            auto c = std::make_unique<Call>();
            c->line = open.line;
            c->name = v->name;
            if (!check(")")) {
                for (;;) {
                    c->args.push_back(expression());
                    if (!match(",")) break;
                }
            }
            expect(")", "右括号");
            e = std::move(c);
        }
        return e;
    }

    ExprPtr primary() {
        const Token& t = cur();

        if (t.kind == Tok::Int) {
            advance();
            auto e = std::make_unique<IntLit>();
            e->line = t.line;
            std::string s = t.text;
            s.erase(std::remove(s.begin(), s.end(), '\''), s.end());
            if (s.size() > 2 && (s[1] == 'x' || s[1] == 'X')) {
                e->value = (long long)std::strtoull(s.c_str(), nullptr, 16);
            } else if (s.size() > 2 && (s[1] == 'b' || s[1] == 'B')) {
                e->value = (long long)std::strtoull(s.c_str() + 2, nullptr, 2);
            } else {
                e->value = (long long)std::strtoll(s.c_str(), nullptr, 10);
            }
            e->type = Type::Int;
            return e;
        }
        if (t.kind == Tok::Float) {
            advance();
            auto e = std::make_unique<FloatLit>();
            e->line = t.line;
            std::string s = t.text;
            s.erase(std::remove(s.begin(), s.end(), '\''), s.end());
            e->value = std::strtod(s.c_str(), nullptr);
            e->type = Type::Double;
            return e;
        }
        if (t.kind == Tok::String) {
            advance();
            auto e = std::make_unique<StringLit>();
            e->line = t.line;
            e->value = t.text;
            e->type = Type::String;
            return e;
        }
        if (t.kind == Tok::Keyword && (t.text == "true" || t.text == "false")) {
            advance();
            auto e = std::make_unique<BoolLit>();
            e->line = t.line;
            e->value = (t.text == "true");
            e->type = Type::Bool;
            return e;
        }
        if (t.kind == Tok::Keyword && t.text == "nullptr") {
            advance();
            requireFeature("nullptr", 2011, t.line);
            auto e = std::make_unique<IntLit>();
            e->line = t.line;
            e->value = 0;
            e->type = Type::Int;
            return e;
        }
        if (t.kind == Tok::Ident) {
            advance();
            auto e = std::make_unique<Var>();
            e->line = t.line;
            e->name = t.text;
            return e;
        }
        if (t.text == "(") {
            advance();
            ExprPtr e = expression();
            expect(")", "右括号");
            return e;
        }

        throw CompileError("这里应该是一个表达式，但读到了 " + t.text, t.line, t.col);
    }
};

}  // namespace

std::unique_ptr<Program> parse(const std::string& src, const std::string& standard) {
    return Parser(tokenize(src, standard), standard).run();
}

}  // namespace co2
