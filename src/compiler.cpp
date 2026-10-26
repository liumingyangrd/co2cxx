// 编译流程编排：解析 -> 语义 -> 优化 -> 代码生成 -> 执行/链接。
#include "compiler.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace co2 {

namespace {

// ---------------------------------------------------------------- 平台抽象

std::string selfPath() {
#ifdef _WIN32
    char buf[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return buf;
#else
    char buf[4096] = "";
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    return n > 0 ? std::string(buf, (size_t)n) : "";
#endif
}

bool fileExists(const std::string& p) {
#ifdef _WIN32
    return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(p.c_str(), &st) == 0;
#endif
}

void removeFile(const std::string& p) {
#ifdef _WIN32
    DeleteFileA(p.c_str());
#else
    unlink(p.c_str());
#endif
}

std::string currentDir() {
#ifdef _WIN32
    char buf[MAX_PATH] = "";
    GetCurrentDirectoryA(MAX_PATH, buf);
    return buf;
#else
    char buf[4096] = "";
    return getcwd(buf, sizeof(buf)) ? std::string(buf) : std::string(".");
#endif
}

const char* pathSep() {
#ifdef _WIN32
    return "\\";
#else
    return "/";
#endif
}

std::string exeSuffix() {
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
}

std::string runCommand(const std::string& cmd) {
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return "";
    std::string out;
    char buf[512];
    while (std::fgets(buf, sizeof(buf), pipe)) out += buf;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return out;
}

// gcc 内部调用 ld 时不处理带空格的路径，统一转成 8.3 短路径
std::string shortPath(const std::string& p) {
    char buf[MAX_PATH * 2] = "";
    DWORD n = GetShortPathNameA(p.c_str(), buf, sizeof(buf));
    std::string out = (n > 0 && n < sizeof(buf)) ? std::string(buf) : p;
    // GetShortPathName 可能返回正反斜杠混用的结果，统一成反斜杠
    for (auto& c : out) {
        if (c == '/') c = '\\';
    }
    return out;
}

// 查找 C 编译器：优先用程序自带的工具链，其次找 PATH 里的
std::string findGcc(const std::string& arch) {
    char exe[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir = exe;
    size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);

    // 自带工具链可能放在这几处
    const char* sub64[] = {"\\mingw64\\bin\\gcc.exe", "\\toolchain\\mingw64\\bin\\gcc.exe",
                           "\\gcc64\\bin\\gcc.exe"};
    const char* sub32[] = {"\\mingw32\\bin\\gcc.exe", "\\toolchain\\mingw32\\bin\\gcc.exe",
                           "\\gcc32\\bin\\gcc.exe"};

    // gcc 内部拼接路径不处理空格，所以程序目录带空格时改用全局无空格位置
    const char* global64[] = {"C:\\co2cxx-toolchain\\mingw64\\bin\\gcc.exe"};
    const char* global32[] = {"C:\\co2cxx-toolchain\\mingw32\\bin\\gcc.exe"};

    auto exists = [](const std::string& p) {
        return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES;
    };
    bool dirHasSpace = dir.find(' ') != std::string::npos;

    auto tryList = [&](const char* const* list, size_t n) -> std::string {
        for (size_t i = 0; i < n; ++i) {
            std::string p = dir + list[i];
            if (exists(p)) return p;
        }
        return "";
    };

    if (arch == "64" || arch == "auto") {
        if (!dirHasSpace) {
            std::string p = tryList(sub64, 3);
            if (!p.empty()) return p;
        }
        if (exists(global64[0])) return global64[0];
        if (arch == "64") {
            std::string p = tryList(sub64, 3);
            if (!p.empty()) return p;
        }
    }
    if (arch == "32" || arch == "auto") {
        if (!dirHasSpace) {
            std::string p = tryList(sub32, 3);
            if (!p.empty()) return p;
        }
        if (exists(global32[0])) return global32[0];
        if (arch == "32") {
            std::string p = tryList(sub32, 3);
            if (!p.empty()) return p;
        }
    }

    // 回退到 PATH
    for (const char* n : {"gcc", "clang", "cc", "tcc"}) {
        std::string check = std::string("where ") + n + " >nul 2>nul";
        if (std::system(check.c_str()) == 0) return n;
    }
    return "";
}

std::string findGcc() { return findGcc("auto"); }

}  // namespace

std::string findCompiler(const std::string& arch) { return findGcc(arch); }

BuildResult build(const BuildRequest& req) {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();

    BuildResult r;

    auto finish = [&]() {
        r.durationMs = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
        return r;
    };

    auto fail = [&](const std::string& stage, const std::string& msg, int line = 0,
                    const std::string& code = "") {
        r.ok = false;
        r.stage = stage;
        r.message = msg;
        Diagnostic d;
        d.sev = Diagnostic::Error;
        d.code = code;
        d.message = msg;
        d.line = line;
        r.diagnostics.push_back(d);
        return finish();
    };

    // ---------------------------------------------------------- 化学环境

    ChemResult chem = resolveChem(req.chem, req.light);
    if (!chem.ok) return fail("化学环境", chem.error);
    r.equation = chem.equation;
    r.chemName = chem.name;
    r.chemNote = chem.note;
    r.factor = chem.factor;
    r.waitMs = chem.waitMs;

    r.purity = req.purity;

    // ---------------------------------------------------------- 前端

    std::unique_ptr<Program> prog;
    try {
        auto tokens = tokenize(req.source, req.options.standard);
        r.tokenCount = (int)tokens.size() - 1;
        prog = parse(req.source, req.options.standard);
    } catch (CompileError& e) {
        return fail("前端", e.message, e.line);
    }

    // 优化等级：金属纯度决定基础等级，再乘上反应的活性系数
    double base = purityToLevel(r.purity);
    if (req.options.optimization == "metal") {
        r.level = base * chem.factor;
        if (r.level < 0.0) r.level = 0.0;
        if (r.level > 3.0) r.level = 3.0;      // -O3 是上限
    } else if (req.options.optimization == "O0") r.level = 0.0;
    else if (req.options.optimization == "O1") r.level = 1.0;
    else if (req.options.optimization == "O2") r.level = 2.0;
    else if (req.options.optimization == "O3") r.level = 3.0;
    else if (req.options.optimization == "Os") r.level = 1.5;
    else r.level = base * chem.factor;

    try {
        analyze(*prog, req.options, r.diagnostics);
    } catch (CompileError& e) {
        return fail("语义分析", e.message, e.line);
    }

    r.functionCount = 0;
    for (auto& fn : prog->functions)
        if (fn->body) r.functionCount++;

    // 有错误级诊断就停止
    for (auto& d : r.diagnostics) {
        if (d.sev == Diagnostic::Error) {
            r.ok = false;
            r.stage = "语义分析";
            r.message = d.message;
            return finish();
        }
    }

    // ---------------------------------------------------------- 优化

    optimizeProgram(*prog, r.level, r.optStats);

    // ---------------------------------------------------------- 代码生成

    char header[256];
    std::snprintf(header, sizeof(header),
                  "generated by co2cxx | %s | -%s%s",
                  req.options.standard.c_str(), chem.mode.c_str(),
                  r.waitMs > 0 ? " | cobalt queued" : "");

    Options genOpt = req.options;
    genOpt.waitMs = r.waitMs;
    r.ccode = generateC(*prog, genOpt, header);

    // ---------------------------------------------------------- 执行

    if (!req.useNative) {
        try {
            r.output.clear();
            interpret(*prog, r.output, r.waitMs);
            r.ok = true;
            r.stage = "完成";
            r.message = "解释执行完成";
        } catch (CompileError& e) {
            return fail("运行时", e.message, e.line);
        }
        return finish();
    }

    // 原生后端：写 C 文件并调用 gcc
    std::string gcc = findGcc(req.arch);
    if (gcc.empty()) {
        r.ok = false;
        r.stage = "后端";
        r.message = "没有找到 C 编译器（32 位或 64 位工具链都不存在）";
        Diagnostic d;
        d.sev = Diagnostic::Error;
        d.message = r.message;
        r.diagnostics.push_back(d);
        return finish();
    }

    // 只取文件名部分，去掉目录和扩展名
    std::string stem = req.fileName;
    size_t slash = stem.find_last_of("\\/");
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    size_t dot = stem.find_last_of('.');
    if (dot != std::string::npos) stem = stem.substr(0, dot);
    if (stem.empty()) stem = "main";

    // 用绝对路径，避免当前目录带空格时 gcc 内部解析出错
    std::string baseDir = req.workDir;
    if (baseDir.empty() || baseDir == ".") {
        char cwd[MAX_PATH] = "";
        GetCurrentDirectoryA(MAX_PATH, cwd);
        baseDir = cwd;
    }
    std::string cPath = baseDir + "\\" + stem + ".co2.c";
    std::string exePath = baseDir + "\\" + stem + ".exe";

    {
        std::ofstream f(cPath, std::ios::binary);
        if (!f) return fail("后端", "无法写入中间文件 " + cPath);
        f << r.ccode;
    }

    // IDE 是 ANSI 窗口，内部字符串是 GBK；生成的 C 文件也是 GBK，
    // 必须同时告诉 gcc 输入和执行字符集都是 GBK
    std::string flags = "-x c -std=c99 -finput-charset=GBK -fexec-charset=GBK";
    if (req.options.optimization == "Os") flags += " -Os";
    else {
        if (r.level < 0.5) flags += " -O0";
        else if (r.level < 1.5) flags += " -O1";
        else if (r.level < 2.5) flags += " -O2";
        else flags += " -O3";
    }
    if (req.options.debugInfo) flags += " -g";
    if (req.options.warnAll) flags += " -Wall";
    if (!req.options.extraFlags.empty()) flags += " " + req.options.extraFlags;

    std::string cmd = shortPath(gcc) + " " + flags + " \"" + shortPath(cPath) +
                      "\" -o \"" + shortPath(exePath) + "\" 2>&1";
    DeleteFileA(exePath.c_str());          // 先删旧产物，否则编译失败也会被当成成功
    std::string log = runCommand(cmd);

    if (GetFileAttributesA(exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        r.ok = false;
        r.stage = "后端编译";
        r.message = log.empty() ? "gcc 编译失败" : log;
        Diagnostic d;
        d.sev = Diagnostic::Error;
        d.message = r.message;
        r.diagnostics.push_back(d);
        return finish();
    }

    r.exePath = exePath;
    r.ok = true;
    r.stage = "完成";
    r.message = "原生编译完成";
    return finish();
}

}  // namespace co2
