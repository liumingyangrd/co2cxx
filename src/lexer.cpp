// 手写词法分析器：字符流 -> Token 流。
#include "core.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace co2 {

namespace {

const std::map<std::string, bool>& keywords() {
    static const std::map<std::string, bool> kw = {
        {"int", true}, {"double", true}, {"bool", true}, {"void", true},
        {"if", true}, {"else", true}, {"while", true}, {"for", true},
        {"return", true}, {"break", true}, {"continue", true},
        {"true", true}, {"false", true}, {"print", true},
        {"auto", true}, {"nullptr", true}, {"constexpr", true},
    };
    return kw;
}

int stdYear(const std::string& s) {
    if (s == "c++98") return 1998;
    if (s == "c++03") return 2003;
    if (s == "c++11") return 2011;
    if (s == "c++14") return 2014;
    if (s == "c++17") return 2017;
    if (s == "c++20") return 2020;
    return 2017;
}

void requireFeature(const char* feature, int year, const std::string& std_, int line) {
    if (stdYear(std_) < year) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "%s 是 C++%d 才引入的特性，当前标准 %s 不支持。试试 --std=c++%d",
                      feature, year % 100, std_.c_str(), year % 100);
        throw CompileError(buf, line, 0);
    }
}

class Lexer {
public:
    Lexer(const std::string& src, const std::string& standard)
        : src_(src), std_(standard), n_((int)src.size()) {}

    std::vector<Token> run() {
        std::vector<Token> out;
        for (;;) {
            skipSpace();
            if (pos_ >= n_) {
                out.push_back({Tok::Eof, "", line_, col_});
                return out;
            }
            out.push_back(next());
        }
    }

private:
    const std::string& src_;
    std::string std_;
    int n_;
    int pos_ = 0;
    int line_ = 1;
    int col_ = 1;

    char at(int off = 0) const {
        int i = pos_ + off;
        return (i >= 0 && i < n_) ? src_[i] : '\0';
    }

    char advance() {
        char c = src_[pos_++];
        if (c == '\n') { line_++; col_ = 1; } else { col_++; }
        return c;
    }

    void skipSpace() {
        for (;;) {
            if (pos_ >= n_) return;
            char c = at();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { advance(); continue; }
            if (c == '/' && at(1) == '/') {
                while (pos_ < n_ && at() != '\n') advance();
                continue;
            }
            if (c == '/' && at(1) == '*') {
                int sl = line_, sc = col_;
                advance(); advance();
                bool closed = false;
                while (pos_ < n_) {
                    if (at() == '*' && at(1) == '/') { advance(); advance(); closed = true; break; }
                    advance();
                }
                if (!closed) throw CompileError("块注释没有闭合", sl, sc);
                continue;
            }
            return;
        }
    }

    Token next() {
        int l = line_, c = col_;
        char ch = at();

        if (std::isalpha((unsigned char)ch) || ch == '_') {
            std::string id;
            while (pos_ < n_ && (std::isalnum((unsigned char)at()) || at() == '_')) id += advance();
            bool kw = keywords().count(id) > 0;
            return {kw ? Tok::Keyword : Tok::Ident, id, l, c};
        }

        if (std::isdigit((unsigned char)ch)) return number();

        if (ch == '"') return stringLit();

        // 双字符运算符优先
        std::string two = std::string(1, at()) + std::string(1, at(1));
        static const char* twoOps[] = {"==", "!=", "<=", ">=", "&&", "||", "+=", "-=", "++", "--"};
        for (const char* op : twoOps) {
            if (two == op) { advance(); advance(); return {Tok::Op, two, l, c}; }
        }

        if (std::strchr("+-*/%=<>!", ch)) { advance(); return {Tok::Op, std::string(1, ch), l, c}; }
        if (std::strchr("(){};,", ch)) { advance(); return {Tok::Punct, std::string(1, ch), l, c}; }

        throw CompileError(std::string("不认识的字符 '") + ch + "'", l, c);
    }

    Token number() {
        int l = line_, c = col_;

        // 0x / 0b
        if (at() == '0' && (at(1) == 'x' || at(1) == 'X')) {
            advance(); advance();
            std::string digits;
            while (pos_ < n_ && (std::isxdigit((unsigned char)at()) || at() == '\'')) {
                if (at() == '\'') requireFeature("数字分隔符", 2014, std_, line_);
                digits += advance();
            }
            if (digits.empty()) throw CompileError("0x 后面缺少十六进制数字", l, c);
            return {Tok::Int, "0x" + digits, l, c};
        }
        if (at() == '0' && (at(1) == 'b' || at(1) == 'B')) {
            requireFeature("二进制字面量", 2014, std_, l);
            advance(); advance();
            std::string digits;
            while (pos_ < n_ && (at() == '0' || at() == '1' || at() == '\'')) {
                if (at() == '\'') requireFeature("数字分隔符", 2014, std_, line_);
                digits += advance();
            }
            if (digits.empty()) throw CompileError("0b 后面缺少二进制数字", l, c);
            return {Tok::Int, "0b" + digits, l, c};
        }

        std::string num;
        bool isFloat = false;
        while (pos_ < n_ && (std::isdigit((unsigned char)at()) || at() == '\'')) {
            if (at() == '\'') requireFeature("数字分隔符", 2014, std_, line_);
            num += advance();
        }
        if (at() == '.' && std::isdigit((unsigned char)at(1))) {
            isFloat = true;
            num += advance();
            while (pos_ < n_ && std::isdigit((unsigned char)at())) num += advance();
        }
        if (at() == 'e' || at() == 'E') {
            int save = pos_;
            std::string exp(1, advance());
            if (at() == '+' || at() == '-') exp += advance();
            if (std::isdigit((unsigned char)at())) {
                isFloat = true;
                while (pos_ < n_ && std::isdigit((unsigned char)at())) exp += advance();
                num += exp;
            } else {
                pos_ = save;
            }
        }
        return {isFloat ? Tok::Float : Tok::Int, num, l, c};
    }

    Token stringLit() {
        int l = line_, c = col_;
        advance();
        std::string value;
        for (;;) {
            if (pos_ >= n_) throw CompileError("字符串没有闭合", l, c);
            char ch = at();
            if (ch == '\n') throw CompileError("字符串里不能直接换行", l, c);
            if (ch == '\\') {
                advance();
                char e = advance();
                switch (e) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '"': value += '"'; break;
                    case '\\': value += '\\'; break;
                    case '0': value += '\0'; break;
                    default:
                        throw CompileError(std::string("不认识的转义字符 \\") + e, l, c);
                }
                continue;
            }
            if (ch == '"') { advance(); return {Tok::String, value, l, c}; }
            value += advance();
        }
    }
};

}  // namespace

std::vector<Token> tokenize(const std::string& src, const std::string& standard) {
    return Lexer(src, standard).run();
}

}  // namespace co2
