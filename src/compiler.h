// 编译器对外接口：一次完整构建的请求与结果。
#pragma once

#include <string>
#include <vector>

#include "core.h"

namespace co2 {

struct BuildRequest {
    std::string source;
    std::string fileName = "main.cpp";
    Options options;
    std::string chem = "Fe";     // 金属或 金属+配体
    bool light = false;
    double purity = 99.5;        // 金属纯度，决定基础优化等级
    bool useNative = false;      // true: 调用 gcc 生成 exe
    std::string arch = "auto";   // 32 | 64 | auto，决定用哪套工具链
    std::string workDir = ".";
};

struct BuildResult {
    bool ok = false;
    std::string stage;
    std::string message;

    std::vector<Diagnostic> diagnostics;
    std::vector<std::string> output;      // 程序运行输出
    std::string ccode;
    std::string exePath;

    double purity = 0;
    double level = 0;
    int waitMs = 0;          // 钴排队等待（毫秒）
    double factor = 1.0;
    std::string equation;
    std::string chemName;
    std::string chemNote;

    std::vector<std::pair<std::string, int>> optStats;
    int tokenCount = 0;
    int functionCount = 0;
    double durationMs = 0;
};

BuildResult build(const BuildRequest& req);

// 查找可用的 C 编译器（arch: "32" / "64" / "auto"），找不到返回空串
std::string findCompiler(const std::string& arch = "auto");

}  // namespace co2
