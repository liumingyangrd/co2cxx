// co2cxx 核心数据结构：Token、类型、AST。
// 纯 C++17，不依赖任何第三方库。
#pragma once

#include <memory>
#include <string>
#include <vector>

namespace co2 {

// ---------------------------------------------------------------- 词法单元

enum class Tok {
    Int, Float, String, Ident, Keyword, Op, Punct, Eof
};

struct Token {
    Tok kind = Tok::Eof;
    std::string text;
    int line = 1;
    int col = 1;
};

// ---------------------------------------------------------------- 类型

enum class Type { Unknown, Int, Double, Bool, Void, String };

inline const char* typeName(Type t) {
    switch (t) {
        case Type::Int: return "int";
        case Type::Double: return "double";
        case Type::Bool: return "bool";
        case Type::Void: return "void";
        case Type::String: return "string";
        default: return "unknown";
    }
}

inline bool isNumeric(Type t) { return t == Type::Int || t == Type::Double; }
inline bool isScalar(Type t) { return t == Type::Int || t == Type::Double || t == Type::Bool; }

// ---------------------------------------------------------------- AST

struct Node {
    int line = 0;
    Type type = Type::Unknown;
    virtual ~Node() = default;
};

struct Expr : Node {};
struct Stmt : Node {};

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct IntLit : Expr { long long value = 0; };
struct FloatLit : Expr { double value = 0; };
struct BoolLit : Expr { bool value = false; };
struct StringLit : Expr { std::string value; };
struct Var : Expr { std::string name; };

struct Binary : Expr {
    std::string op;
    ExprPtr lhs, rhs;
};

struct Unary : Expr {
    std::string op;
    ExprPtr operand;
};

struct Call : Expr {
    std::string name;
    std::vector<ExprPtr> args;
};

struct Block : Stmt {
    std::vector<StmtPtr> stmts;
};

struct VarDecl : Stmt {
    std::string name;
    Type vtype = Type::Int;
    bool isAuto = false;
    ExprPtr init;
};

struct Assign : Stmt {
    std::string target;
    std::string op;
    ExprPtr value;
};

struct If : Stmt {
    ExprPtr cond;
    StmtPtr thenStmt;
    StmtPtr elseStmt;
    bool isConstexpr = false;
};

struct While : Stmt {
    ExprPtr cond;
    StmtPtr body;
};

struct For : Stmt {
    StmtPtr init;
    ExprPtr cond;
    StmtPtr step;
    StmtPtr body;
};

struct Return : Stmt { ExprPtr value; };
struct Break : Stmt {};
struct Continue : Stmt {};
struct ExprStmt : Stmt { ExprPtr expr; };
struct Print : Stmt { ExprPtr value; };

struct Param {
    std::string name;
    Type type = Type::Int;
};

struct Function : Node {
    std::string name;
    Type ret = Type::Void;
    std::vector<Param> params;
    std::unique_ptr<Block> body;      // nullptr 表示只有声明
    bool isConstexpr = false;
    std::string file;
};

struct Program : Node {
    std::vector<std::unique_ptr<Function>> functions;
};

// ---------------------------------------------------------------- 诊断

struct Diagnostic {
    enum Sev { Error, Warning } sev = Error;
    std::string code;
    std::string message;
    int line = 0;
    std::string file;
};

// ---------------------------------------------------------------- 编译器选项

struct Options {
    std::string standard = "c++17";
    std::string optimization = "metal"; // metal / O0 / O1 / O2 / O3 / Os
    std::string arch = "auto";          // 32 / 64 / auto
    int waitMs = 0;                     // 钴资源排队等待，非 0 时生成延迟代码
    bool warnAll = true;
    bool warnExtra = false;
    bool warnError = false;
    bool pedantic = false;
    bool debugInfo = false;
    bool useNative = false;             // 用外部 gcc 生成 exe
    std::vector<std::pair<std::string, std::string>> defines;
    std::string extraFlags;
};

// ---------------------------------------------------------------- 错误

struct CompileError {
    std::string message;
    int line = 0;
    int col = 0;

    CompileError() = default;
    CompileError(std::string msg, int ln = 0, int cl = 0)
        : message(std::move(msg)), line(ln), col(cl) {}
};

// ---------------------------------------------------------------- 各阶段接口

// ---------------------------------------------------------------- 金属冶炼

// 纯度（%）映射到基础优化等级：50% 是粗矿，100% 是精炼纯金属
double purityToLevel(double purity);

struct ChemResult {
    bool ok = true;
    std::string error;
    std::string mode = "Fe";
    double factor = 1.0;
    std::string equation;
    std::string name;
    std::string condition;
    std::string note;
    int waitMs = 0;          // 钴资源排队等待时间（毫秒），其他金属为 0
};

// spec 形如 "Fe"、"Na+H2O"、"Mn+HCl"
ChemResult resolveChem(const std::string& spec, bool light = false);

// ---------------------------------------------------------------- 各阶段

std::vector<Token> tokenize(const std::string& src, const std::string& standard);

std::unique_ptr<Program> parse(const std::string& src, const std::string& standard);

void analyze(Program& prog, const Options& opt, std::vector<Diagnostic>& diags);

void optimizeProgram(Program& prog, double level,
                     std::vector<std::pair<std::string, int>>& stats);

std::string generateC(const Program& prog, const Options& opt, const std::string& header);

int interpret(const Program& prog, std::vector<std::string>& out, int waitMs = 0);

}  // namespace co2
