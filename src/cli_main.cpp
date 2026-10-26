// 命令行入口：co2cxx -CO2 file.cpp
#include "compiler.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace co2;

namespace {

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void usage() {
    std::cout <<
        "co2cxx — 由大气二氧化碳浓度驱动的 C++ 子集编译器\n\n"
        "用法:\n"
        "  co2cxx                    打开图形界面\n"
        "  co2cxx -CO2 hello.cpp     编译\n"
        "  co2cxx -CO2 --run hello.cpp   直接运行\n\n"
        "碳相关:\n"
        "  -Na / -Mn / -Fe / -Cu           选择金属\n"
        "  -Na+H2O / -Fe+O2 / -Mn+HCl      金属 + 配体反应\n"
        "  --purity=N               金属纯度（50~100），越高编译越激进\n\n"
        "编译选项:\n"
        "  --std=c++XX              语言标准 98/03/11/14/17/20\n"
        "  --opt=MODE               co2 / O0 / O1 / O2 / O3 / Os\n"
        "  -Wall -Wextra -Werror    警告等级\n"
        "  -DNAME=VALUE             宏定义\n\n"
        "输出:\n"
        "  --run                    解释执行\n"
        "  --emit-c                 只打印生成的 C 代码\n"
        "  -o FILE                  输出文件\n";
}

}  // namespace

int cliMain(int argc, char** argv) {
    // GUI 子系统下 stdout 是全缓冲的，命令行模式必须关掉缓冲，否则输出顺序错乱
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    BuildRequest req;
    std::string file;
    bool runOnly = false;
    bool emitC = false;
    std::string outFile;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (a == "--run") runOnly = true;
        else if (a == "--emit-c") emitC = true;
        else if (a == "--light") req.light = true;
        else if (a.rfind("--purity=", 0) == 0) req.purity = std::atof(a.c_str() + 9);
        else if (a.rfind("--std=", 0) == 0) req.options.standard = a.substr(6);
        else if (a.rfind("--opt=", 0) == 0) req.options.optimization = a.substr(6);
        else if (a == "-Wall") req.options.warnAll = true;
        else if (a == "-Wextra") req.options.warnExtra = true;
        else if (a == "-Werror") req.options.warnError = true;
        else if (a == "-g") req.options.debugInfo = true;
        else if (a.rfind("-D", 0) == 0 && a.size() > 2) {
            std::string kv = a.substr(2);
            size_t eq = kv.find('=');
            if (eq == std::string::npos) req.options.defines.push_back({kv, "1"});
            else req.options.defines.push_back({kv.substr(0, eq), kv.substr(eq + 1)});
        }
        else if (a.rfind("-Na", 0) == 0 || a.rfind("-Mn", 0) == 0 ||
                 a.rfind("-Fe", 0) == 0 || a.rfind("-Cu", 0) == 0 ||
                 a.rfind("-Co", 0) == 0) {
            req.chem = a.substr(1);
        }
        else if (a == "-CO2" || a == "-CO" || a == "-CH4" || a == "-NO2" || a == "-H2O") {
            // 旧版的气体参数，交给反应引擎给出提示，而不是静默忽略
            req.chem = a.substr(1);
        }
        else if (a == "-o" && i + 1 < argc) outFile = argv[++i];
        else if (!a.empty() && a[0] != '-') file = a;
    }

    if (file.empty()) { usage(); return 0; }

    req.source = readFile(file);
    if (req.source.empty()) {
        std::cerr << "  错误  找不到或无法读取源文件 " << file << "\n";
        return 2;
    }
    req.fileName = file;
    req.workDir = ".";

    ChemResult chem = resolveChem(req.chem, req.light);
    if (!chem.ok) {
        std::cerr << "\n  不反应  " << chem.error << "\n\n";
        return 2;
    }

    req.useNative = !runOnly && !emitC;
    BuildResult r = build(req);

    if (emitC) {
        std::cout << r.ccode;
        return 0;
    }

    for (auto& d : r.diagnostics) {
        std::cerr << (d.sev == Diagnostic::Error ? "  错误  " : "  警告  ")
                  << "[" << d.code << "] ";
        if (d.line) std::cerr << "第 " << d.line << " 行: ";
        std::cerr << d.message << "\n";
    }

    if (!r.ok) {
        std::cerr << "\n  " << r.stage << "失败：" << r.message << "\n";
        return 1;
    }

    if (!chem.equation.empty()) {
        std::cout << "\n  冶炼条件  " << chem.equation << "  [" << chem.name << "]\n";
    }
    {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "  -%s  %s  （%s）\n\n",
                      chem.mode.c_str(), chem.name.c_str(),
                      r.exePath.empty() ? "内置解释器" : "原生编译");
        std::cout << buf;
    }

    if (!r.output.empty()) {
        for (auto& line : r.output) std::cout << line << "\n";
    }

    if (!r.exePath.empty() && !outFile.empty()) {
        std::string cmd = "copy /Y \"" + r.exePath + "\" \"" + outFile + "\" >nul";
        std::system(cmd.c_str());
        std::cout << "\n  已输出到 " << outFile << "\n";
    }
    return 0;
}
