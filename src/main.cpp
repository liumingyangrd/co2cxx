// 入口：无参数打开 IDE，带参数走命令行，--run-console 是控制台运行器。
//
// 编译成 GUI 子系统（-mwindows），双击不会出现黑色控制台窗口。
// 命令行模式通过 AttachConsole 附加到调用者的控制台，输出照常可见。
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

int cliMain(int argc, char** argv);
int guiMain(HINSTANCE hInstance);

namespace {

// 把标准输出接到一个真实控制台上。
// 注意不要动 stdin —— 调用者可能把 stdin 重定向成了管道，freopen(CONIN$) 会把它冲掉。
void bindConsole(bool createIfMissing) {
    bool attached = AttachConsole(ATTACH_PARENT_PROCESS) != FALSE;
    if (!attached && createIfMissing) {
        AllocConsole();
        attached = true;
    }
    if (!attached) return;

    FILE* f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
}

// 在独立控制台里运行目标程序，结束后按 Dev-C++ 的格式打印统计信息
int runConsole(const char* exePath) {
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    std::string cmd = std::string("\"") + exePath + "\"";
    std::string dir = exePath;
    size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    // 子进程继承当前控制台，输出直接落在同一个窗口里
    BOOL ok = CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, TRUE,
                             0, nullptr, dir.empty() ? nullptr : dir.c_str(), &si, &pi);

    DWORD code = 0;
    if (ok) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        code = 0xFFFFFFFF;
    }

    QueryPerformanceCounter(&t1);
    double secs = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;

    std::printf("--------------------------------\n");
    if (ok) {
        std::printf("Process exited after %.3f seconds with return value %lu\n",
                    secs, (unsigned long)code);
    } else {
        std::printf("Failed to start the program.\n");
    }
    std::printf("Press any key to continue . . .");
    std::fflush(stdout);

    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD rec;
    DWORD read = 0;
    while (ReadConsoleInputA(in, &rec, 1, &read) && read > 0) {
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) break;
    }
    std::printf("\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::strcmp(argv[1], "--run-console") == 0) {
        bindConsole(true);          // 运行器必须有自己的控制台
        return runConsole(argv[2]);
    }
    if (argc > 1) {
        bindConsole(false);         // 命令行模式：附加到调用者的控制台
        return cliMain(argc, argv);
    }
    return guiMain(GetModuleHandleW(nullptr));
}
