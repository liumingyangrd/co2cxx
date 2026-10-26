// co2cxx 原生 IDE（Win32）。
// 布局参考 Dev-C++：菜单栏 / 工具栏 / 编辑区 / 编译输出 / 状态栏。
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "compiler.h"

using namespace co2;

namespace {

// ---------------------------------------------------------------- 控件 ID

enum {
    ID_EDIT = 1001,
    ID_OUTPUT,
    ID_STATUS,
    ID_TAB,
    ID_TREE,

    IDM_NEW = 2001,
    IDM_OPEN,
    IDM_SAVE,
    IDM_SAVEAS,
    IDM_EXIT,
    IDM_UNDO,
    IDM_CUT,
    IDM_COPY,
    IDM_PASTE,
    IDM_SELECTALL,
    IDM_COMPILE,
    IDM_RUN,
    IDM_BUILD,
    IDM_OPTIONS,
    IDM_ABOUT,
    IDM_CHEM_CO2,
    IDM_CHEM_CO,
    IDM_CHEM_CO2O2,
    IDM_CHEM_CO2C,
    IDM_CHEM_CO2H2O,
    IDM_STD_11,
    IDM_STD_14,
    IDM_STD_17,
    IDM_STD_20,
    IDM_VIEW_C,
    IDM_FONT = 2100,
    IDM_FIND,
    IDM_REPLACE,
    IDM_FINDNEXT,
    IDM_NEWPROJ,
    IDM_ADDFILE,
    IDM_REMOVEFILE,
    IDM_DEBUG,
    IDM_ASTYLE,
    IDM_MINIMIZE,
    IDM_CASCADE,
    IDM_TOGGLE_TREE,
    IDM_TOGGLE_OUTPUT,
    IDM_METAL_FEO2,
    IDM_METAL_MNHCL,
    IDM_METAL_CO,
};

// ---------------------------------------------------------------- 全局

HWND g_main = nullptr;
HWND g_edit = nullptr;
HWND g_output = nullptr;
HWND g_status = nullptr;
HWND g_gutter = nullptr;
HWND g_tree = nullptr;
HWND g_tab = nullptr;
int g_outPanelH = 160;
bool g_showTree = true;
bool g_showOutput = true;
HWND g_config = nullptr;
std::set<int> g_breakpoints;   // 断点行号（1-based）
int g_lineHeight = 16;
std::string g_fontName = "Consolas";
int g_fontSize = 10;          // 磅
HFONT g_editorFont = nullptr;
HWND g_btnCompile = nullptr;

struct ToolBtn { int id; int x; int y; HWND hwnd; };
ToolBtn g_tools[16];
int g_toolCount = 0;
HWND g_btnRun = nullptr;
HWND g_btnBuild = nullptr;
HMODULE g_riched = nullptr;

std::string g_path;
bool g_dirty = false;
bool g_highlighting = false;
int g_tabIndex = 0;

Options g_options;
std::string g_chem = "Fe";
double g_purity = 99.5;
BuildResult g_last;

const int TOOLBAR_H = 58;
const int TREE_W = 165;
const int TAB_H = 24;
const int STATUS_H = 22;

void drawGutter();   // 前向声明

// ---------------------------------------------------------------- 颜色

const COLORREF CLR_BG = RGB(250, 250, 247);
const COLORREF CLR_TEXT = RGB(29, 29, 27);
const COLORREF CLR_KW = RGB(124, 58, 237);
const COLORREF CLR_TYPE = RGB(15, 110, 86);
const COLORREF CLR_STR = RGB(163, 95, 0);
const COLORREF CLR_NUM = RGB(24, 95, 165);
const COLORREF CLR_COM = RGB(132, 132, 124);
const COLORREF CLR_FUNC = RGB(24, 95, 165);
const COLORREF CLR_ERR = RGB(163, 45, 45);

void highlight();
void updateStatus();

// ---------------------------------------------------------------- 主题

struct Theme {
    const char* name;
    COLORREF bg;         // 编辑区背景
    COLORREF text;       // 普通文本
    COLORREF kw;         // 关键字
    COLORREF type;       // 类型
    COLORREF str;        // 字符串
    COLORREF num;        // 数字
    COLORREF com;        // 注释
    COLORREF func;       // 函数名
    COLORREF gutterBg;   // 行号栏背景
    COLORREF gutterText; // 行号文字
};

const Theme THEMES[] = {
    // 默认浅色
    {"Light", RGB(250,250,247), RGB(29,29,27), RGB(124,58,237), RGB(15,110,86),
     RGB(163,95,0), RGB(24,95,165), RGB(132,132,124), RGB(24,95,165),
     RGB(245,245,241), RGB(150,150,143)},

    // Dev-C++ Classic：白底，保留字蓝粗，注释绿
    {"Classic", RGB(255,255,255), RGB(0,0,0), RGB(0,0,255), RGB(0,0,128),
     RGB(0,0,255), RGB(0,0,0), RGB(0,128,0), RGB(128,0,128),
     RGB(240,240,240), RGB(128,128,128)},

    // Twilight：深紫灰
    {"Twilight", RGB(20,20,20), RGB(248,248,248), RGB(200,168,255), RGB(155,200,255),
     RGB(143,157,106), RGB(207,106,76), RGB(95,90,96), RGB(255,213,143),
     RGB(28,28,28), RGB(110,105,110)},

    // Ocean：深蓝
    {"Ocean", RGB(15,17,26), RGB(192,197,206), RGB(197,148,197), RGB(143,161,179),
     RGB(153,199,148), RGB(249,145,87), RGB(101,115,126), RGB(143,161,179),
     RGB(22,25,36), RGB(90,105,118)},

    // Solarized Light
    {"Solarized Light", RGB(253,246,227), RGB(101,123,131), RGB(133,153,0), RGB(38,139,210),
     RGB(42,161,152), RGB(211,54,130), RGB(147,161,161), RGB(38,139,210),
     RGB(238,232,213), RGB(147,161,161)},

    // Solarized Dark
    {"Solarized Dark", RGB(0,43,54), RGB(131,148,150), RGB(133,153,0), RGB(38,139,210),
     RGB(42,161,152), RGB(211,54,130), RGB(88,110,117), RGB(38,139,210),
     RGB(0,34,43), RGB(88,110,117)},

    // Obsidian：深灰绿
    {"Obsidian", RGB(41,49,52), RGB(224,226,228), RGB(147,199,99), RGB(103,140,177),
     RGB(236,118,0), RGB(255,205,34), RGB(102,116,123), RGB(103,140,177),
     RGB(51,59,62), RGB(130,143,148)},
};

const int THEME_COUNT = (int)(sizeof(THEMES) / sizeof(THEMES[0]));
int g_themeIndex = 0;
HMENU g_themeMenu = nullptr;

std::string iniPath() {
    char buf[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p = buf;
    size_t dot = p.find_last_of('.');
    if (dot != std::string::npos) p = p.substr(0, dot);
    return p + ".ini";
}

void loadSettings() {
    std::string ini = iniPath();
    g_themeIndex = GetPrivateProfileIntA("ui", "theme", 0, ini.c_str());
    if (g_themeIndex < 0 || g_themeIndex >= THEME_COUNT) g_themeIndex = 0;
    int chem = GetPrivateProfileIntA("ui", "chem", 0, ini.c_str());
    const char* chems[] = {"Fe", "Na", "Mn", "Cu", "Na+H2O", "Fe+O2", "Mn+HCl", "Co"};
    if (chem >= 0 && chem < 8) g_chem = chems[chem];
    char pur[32] = "";
    GetPrivateProfileStringA("ui", "purity", "99.5", pur, sizeof(pur), ini.c_str());
    g_purity = std::atof(pur);
    if (g_purity < 50 || g_purity > 100) g_purity = 99.5;

    char font[LF_FACESIZE] = "";
    GetPrivateProfileStringA("ui", "font", "Consolas", font, LF_FACESIZE, ini.c_str());
    if (font[0]) g_fontName = font;
    g_fontSize = GetPrivateProfileIntA("ui", "fontsize", 10, ini.c_str());
    if (g_fontSize < 6 || g_fontSize > 48) g_fontSize = 10;

    char arch[8] = "";
    GetPrivateProfileStringA("ui", "arch", "auto", arch, sizeof(arch), ini.c_str());
    g_options.arch = arch;
    g_options.debugInfo = GetPrivateProfileIntA("ui", "debug", 0, ini.c_str()) != 0;
}

void saveSettings() {
    std::string ini = iniPath();
    char val[16];
    std::snprintf(val, sizeof(val), "%d", g_themeIndex);
    WritePrivateProfileStringA("ui", "theme", val, ini.c_str());
    const char* chems[] = {"Fe", "Na", "Mn", "Cu", "Na+H2O", "Fe+O2", "Mn+HCl", "Co"};
    int chem = 0;
    for (int i = 0; i < 8; ++i) if (g_chem == chems[i]) chem = i;
    std::snprintf(val, sizeof(val), "%d", chem);
    WritePrivateProfileStringA("ui", "chem", val, ini.c_str());
    std::snprintf(val, sizeof(val), "%.1f", g_purity);
    WritePrivateProfileStringA("ui", "purity", val, ini.c_str());
    WritePrivateProfileStringA("ui", "font", g_fontName.c_str(), ini.c_str());
    std::snprintf(val, sizeof(val), "%d", g_fontSize);
    WritePrivateProfileStringA("ui", "fontsize", val, ini.c_str());
    WritePrivateProfileStringA("ui", "arch", g_options.arch.c_str(), ini.c_str());
    std::snprintf(val, sizeof(val), "%d", g_options.debugInfo ? 1 : 0);
    WritePrivateProfileStringA("ui", "debug", val, ini.c_str());
}
const Theme& theme() { return THEMES[g_themeIndex]; }

void buildThemeMenu(HMENU parent) {
    g_themeMenu = parent;
    for (int i = 0; i < THEME_COUNT; ++i)
        AppendMenuA(parent, MF_STRING, 4000 + i, THEMES[i].name);
    CheckMenuRadioItem(parent, 4000, 4000 + THEME_COUNT - 1, 4000 + g_themeIndex,
                       MF_BYCOMMAND);
}

void applyTheme(int idx) {
    if (idx < 0 || idx >= THEME_COUNT) return;
    g_themeIndex = idx;
    if (g_themeMenu)
        CheckMenuRadioItem(g_themeMenu, 4000, 4000 + THEME_COUNT - 1, 4000 + idx,
                           MF_BYCOMMAND);
    saveSettings();
    const Theme& t = theme();
    SendMessageA(g_edit, EM_SETBKGNDCOLOR, 0, (LPARAM)t.bg);
    InvalidateRect(g_main, nullptr, TRUE);
    highlight();
    drawGutter();
    updateStatus();
}


// ---------------------------------------------------------------- 编译器标识

// 从 gcc --version 里抠出版本号
std::string gccVersionOf(const std::string& gccPath) {
    if (gccPath.empty()) return "";
    std::string cmd = "\"" + gccPath + "\" --version 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return "";
    char buf[256] = "";
    if (!std::fgets(buf, sizeof(buf), pipe)) {
        _pclose(pipe);
        return "";
    }
    _pclose(pipe);

    std::string line = buf;
    size_t i = 0;
    while (i < line.size()) {
        if (isdigit((unsigned char)line[i])) {
            size_t j = i;
            int dots = 0;
            while (j < line.size() && (isdigit((unsigned char)line[j]) || line[j] == '.')) {
                if (line[j] == '.') dots++;
                j++;
            }
            if (dots >= 1) return line.substr(i, j - i);
            i = j;
        } else {
            i++;
        }
    }
    return "";
}

// 一条构建配置：右上角下拉里的一项
struct BuildConfig {
    std::string label;
    std::string arch;    // "32" / "64"
    bool debug = false;
};

std::vector<BuildConfig> g_configs;

void rebuildConfigList() {
    g_configs.clear();
    if (!g_config) return;
    SendMessageA(g_config, CB_RESETCONTENT, 0, 0);

    const char* archs[] = {"64", "32"};
    for (const char* arch : archs) {
        std::string gcc = findCompiler(arch);
        if (gcc.empty()) continue;
        std::string ver = gccVersionOf(gcc);
        if (ver.empty()) ver = "?";

        for (int d = 0; d < 2; ++d) {
            bool dbg = (d == 1);
            char label[160];
            std::snprintf(label, sizeof(label), "co2cxx 1.0.0  GCC %s  %s-bit %s",
                          ver.c_str(), arch, dbg ? "Debug" : "Release");
            g_configs.push_back({label, arch, dbg});
            SendMessageA(g_config, CB_ADDSTRING, 0, (LPARAM)label);
        }
    }

    if (g_configs.empty()) {
        g_configs.push_back({"co2cxx 1.0.0  内置解释器", "auto", false});
        SendMessageA(g_config, CB_ADDSTRING, 0, (LPARAM)g_configs[0].label.c_str());
    }

    // 选中当前设置
    int sel = 0;
    for (size_t i = 0; i < g_configs.size(); ++i) {
        if (g_configs[i].arch == g_options.arch &&
            g_configs[i].debug == g_options.debugInfo) {
            sel = (int)i;
            break;
        }
    }
    SendMessageA(g_config, CB_SETCURSEL, sel, 0);
    g_options.arch = g_configs[sel].arch;
    g_options.debugInfo = g_configs[sel].debug;
}

// ---------------------------------------------------------------- 字体

int dpiY() {
    HDC dc = GetDC(nullptr);
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(nullptr, dc);
    return dpi ? dpi : 96;
}

void applyFont() {
    LOGFONTA lf{};
    lf.lfHeight = -MulDiv(g_fontSize, 96, 72);   // 固定 96 DPI 基准，避免高缩放屏字体过大
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    std::snprintf(lf.lfFaceName, LF_FACESIZE, "%s", g_fontName.c_str());

    HFONT font = CreateFontIndirectA(&lf);
    if (!font) return;
    if (g_editorFont) DeleteObject(g_editorFont);
    g_editorFont = font;

    if (g_edit) SendMessageA(g_edit, WM_SETFONT, (WPARAM)font, TRUE);
    if (g_output) SendMessageA(g_output, WM_SETFONT, (WPARAM)font, TRUE);

    if (g_edit) {
        HDC dc = GetDC(g_edit);
        TEXTMETRICA tm{};
        SelectObject(dc, font);
        GetTextMetricsA(dc, &tm);
        g_lineHeight = tm.tmHeight;
        ReleaseDC(g_edit, dc);
    }

    if (g_edit) highlight();
    if (g_gutter) InvalidateRect(g_gutter, nullptr, TRUE);
}

void chooseFont(HWND parent) {
    LOGFONTA lf{};
    lf.lfHeight = -MulDiv(g_fontSize, 96, 72);   // 固定 96 DPI 基准，避免高缩放屏字体过大
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    std::snprintf(lf.lfFaceName, LF_FACESIZE, "%s", g_fontName.c_str());

    CHOOSEFONTA cf{};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner = parent;
    cf.lpLogFont = &lf;
    cf.Flags = CF_SCREENFONTS | CF_FIXEDPITCHONLY | CF_INITTOLOGFONTSTRUCT |
               CF_NOVERTFONTS | CF_FORCEFONTEXIST;

    if (!ChooseFontA(&cf)) return;

    g_fontName = lf.lfFaceName;
    if (cf.iPointSize > 0) g_fontSize = cf.iPointSize / 10;
    if (g_fontSize < 6) g_fontSize = 6;
    if (g_fontSize > 48) g_fontSize = 48;

    applyFont();
    saveSettings();
    updateStatus();
}

// ---------------------------------------------------------------- 工具

std::string getEditText(HWND h) {
    int len = GetWindowTextLengthA(h);
    std::string buf(len + 1, '\0');
    GetWindowTextA(h, &buf[0], len + 1);
    buf.resize(len);
    return buf;
}

void setEditText(HWND h, const std::string& text) {
    SetWindowTextA(h, text.c_str());
}

std::string readFile(const std::string& path) {
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return "";
    DWORD size = GetFileSize(f, nullptr);
    std::string buf(size, '\0');
    DWORD read = 0;
    ReadFile(f, &buf[0], size, &read, nullptr);
    CloseHandle(f);
    buf.resize(read);
    return buf;
}

bool writeFile(const std::string& path, const std::string& data) {
    HANDLE f = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    WriteFile(f, data.data(), (DWORD)data.size(), &written, nullptr);
    CloseHandle(f);
    return true;
}

bool isIdentStart(char c) { return isalpha((unsigned char)c) || c == '_'; }
bool isIdentChar(char c) { return isalnum((unsigned char)c) || c == '_'; }

bool isKeyword(const std::string& s) {
    static const char* kw[] = {"if", "else", "while", "for", "return", "break",
                               "continue", "true", "false", "constexpr", "auto",
                               "nullptr", "print"};
    for (auto* k : kw) if (s == k) return true;
    return false;
}

bool isTypeWord(const std::string& s) {
    return s == "int" || s == "double" || s == "bool" || s == "void";
}

void setColor(HWND h, long start, long end, COLORREF color, bool bold = false) {
    CHARRANGE cr{start, end};
    SendMessageA(h, EM_EXSETSEL, 0, (LPARAM)&cr);
    CHARFORMAT2A cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | (bold ? CFM_BOLD : 0);
    cf.crTextColor = color;
    if (bold) cf.dwEffects = CFE_BOLD;
    SendMessageA(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

// 轻量语法着色：不依赖编译器 lexer，直接扫字符串
void highlight() {
    if (g_highlighting || !g_edit) return;
    g_highlighting = true;

    std::string text = getEditText(g_edit);

    // RichEdit 的字符位置和字节偏移不是一回事：
    //   \r\n 在控件里算一个段落，GBK 双字节字符算一个字符。
    // 必须建映射表，否则从第二行开始着色位置会整体偏移。
    std::vector<int> b2c(text.size() + 1, 0);
    {
        int cp = 0;
        for (size_t k = 0; k < text.size();) {
            b2c[k] = cp;
            unsigned char ch = (unsigned char)text[k];
            if (ch == '\r' && k + 1 < text.size() && text[k + 1] == '\n') {
                b2c[k + 1] = cp;
                k += 2;
                cp += 1;
            } else if (ch >= 0x81 && k + 1 < text.size() &&
                       (unsigned char)text[k + 1] >= 0x40) {
                b2c[k + 1] = cp;
                k += 2;
                cp += 1;
            } else {
                k += 1;
                cp += 1;
            }
        }
        b2c[text.size()] = cp;
    }

    CHARRANGE saved{};
    SendMessageA(g_edit, EM_EXGETSEL, 0, (LPARAM)&saved);
    SendMessageA(g_edit, WM_SETREDRAW, FALSE, 0);

    // 先整体刷成默认色
    CHARRANGE all{0, -1};
    SendMessageA(g_edit, EM_EXSETSEL, 0, (LPARAM)&all);
    CHARFORMAT2A base{};
    base.cbSize = sizeof(base);
    base.dwMask = CFM_COLOR | CFM_BOLD;
    base.crTextColor = theme().text;
    base.dwEffects = 0;
    SendMessageA(g_edit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&base);

    size_t i = 0, n = text.size();
    while (i < n) {
        char c = text[i];

        if (c == '/' && i + 1 < n && text[i + 1] == '/') {
            size_t j = i;
            while (j < n && text[j] != '\n') j++;
            setColor(g_edit, b2c[i], b2c[j], theme().com);
            i = j;
            continue;
        }
        if (c == '/' && i + 1 < n && text[i + 1] == '*') {
            size_t j = i + 2;
            while (j + 1 < n && !(text[j] == '*' && text[j + 1] == '/')) j++;
            j = (j + 1 < n) ? j + 2 : n;
            setColor(g_edit, b2c[i], b2c[j], theme().com);
            i = j;
            continue;
        }
        if (c == '"') {
            size_t j = i + 1;
            while (j < n && text[j] != '"' && text[j] != '\n') {
                if (text[j] == '\\') j++;
                j++;
            }
            if (j < n && text[j] == '"') j++;
            setColor(g_edit, b2c[i], b2c[j], theme().str);
            i = j;
            continue;
        }
        if (isdigit((unsigned char)c)) {
            size_t j = i;
            while (j < n && (isalnum((unsigned char)text[j]) || text[j] == '.' ||
                             text[j] == '\'' || text[j] == '_')) j++;
            setColor(g_edit, b2c[i], b2c[j], theme().num);
            i = j;
            continue;
        }
        if (isIdentStart(c)) {
            size_t j = i;
            while (j < n && isIdentChar(text[j])) j++;
            std::string word = text.substr(i, j - i);
            if (isTypeWord(word)) {
                setColor(g_edit, b2c[i], b2c[j], theme().type, true);
            } else if (isKeyword(word)) {
                setColor(g_edit, b2c[i], b2c[j], theme().kw, true);
            } else {
                // 后面紧跟左括号的标识符按函数名着色
                size_t k = j;
                while (k < n && (text[k] == ' ' || text[k] == '\t')) k++;
                if (k < n && text[k] == '(')
                    setColor(g_edit, b2c[i], b2c[j], theme().func, false);
            }
            i = j;
            continue;
        }
        i++;
    }

    SendMessageA(g_edit, EM_EXSETSEL, 0, (LPARAM)&saved);
    // 清掉着色过程中留下的选区，否则 RichEdit 会在行首残留字符
    SendMessageA(g_edit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageA(g_edit, EM_SETSEL, 0, 0);
    SendMessageA(g_edit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_edit, nullptr, FALSE);
    drawGutter();
    g_highlighting = false;
}

// ---------------------------------------------------------------- 输出面板

void clearOutput() {
    setEditText(g_output, "");
}

void appendOutput(const std::string& line, COLORREF color = CLR_TEXT, bool bold = false) {
    int len = GetWindowTextLengthA(g_output);
    SendMessageA(g_output, EM_SETSEL, len, len);
    CHARFORMAT2A cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_BOLD;
    cf.crTextColor = color;
    if (bold) cf.dwEffects = CFE_BOLD;
    SendMessageA(g_output, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    std::string s = line + "\r\n";
    SendMessageA(g_output, EM_REPLACESEL, FALSE, (LPARAM)s.c_str());
    SendMessageA(g_output, EM_SCROLLCARET, 0, 0);
}

void updateStatus() {
    if (!g_status || !g_edit) return;

    CHARRANGE cr{};
    SendMessageA(g_edit, EM_EXGETSEL, 0, (LPARAM)&cr);
    int line = (int)SendMessageA(g_edit, EM_EXLINEFROMCHAR, 0, cr.cpMin) + 1;
    int lineStart = (int)SendMessageA(g_edit, EM_LINEINDEX, line - 1, 0);
    int col = cr.cpMin - lineStart + 1;
    int totalLines = (int)SendMessageA(g_edit, EM_GETLINECOUNT, 0, 0);
    int length = GetWindowTextLengthA(g_edit);
    int selected = cr.cpMax - cr.cpMin;

    ChemResult chem = resolveChem(g_chem, false);
    double level = purityToLevel(g_purity) * chem.factor;

    char buf[160];
    std::snprintf(buf, sizeof(buf), "行 %d, 列 %d", line, col);
    SendMessageA(g_status, SB_SETTEXT, 0, (LPARAM)buf);

    std::snprintf(buf, sizeof(buf), "已选择 %d", selected);
    SendMessageA(g_status, SB_SETTEXT, 1, (LPARAM)buf);

    std::snprintf(buf, sizeof(buf), "总行数 %d", totalLines);
    SendMessageA(g_status, SB_SETTEXT, 2, (LPARAM)buf);

    std::snprintf(buf, sizeof(buf), "长度 %d", length);
    SendMessageA(g_status, SB_SETTEXT, 3, (LPARAM)buf);

    SendMessageA(g_status, SB_SETTEXT, 4, (LPARAM)"插入");

    std::snprintf(buf, sizeof(buf), "%s   -%s   %s",
                  g_options.standard.c_str(), chem.mode.c_str(),
                  g_options.useNative ? "原生编译" : "内置解释器");
    SendMessageA(g_status, SB_SETTEXT, 5, (LPARAM)buf);
}

// ---------------------------------------------------------------- 控制台运行

// 编译产物放到独立控制台窗口里跑，结束后打印退出统计并暂停，和 Dev-C++ 一致
bool launchConsole(const std::string& exePath) {
    if (exePath.empty()) return false;

    // 用自身的 --run-console 模式做宿主：它负责计时、取返回码、打印统计
    char self[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, self, MAX_PATH);
    std::string cmd = "\"" + std::string(self) + "\" --run-console \"" + exePath + "\"";

    std::string dir;
    size_t slash = exePath.find_last_of("\\/");
    if (slash != std::string::npos) dir = exePath.substr(0, slash);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessA(
        nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, FALSE,
        CREATE_NEW_CONSOLE, nullptr, dir.empty() ? nullptr : dir.c_str(),
        &si, &pi);

    if (!ok) return false;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

// ---------------------------------------------------------------- 编译

void runBuild(bool native, bool showC, bool runAfter = true) {
    clearOutput();
    BuildRequest req;
    req.source = getEditText(g_edit);
    req.fileName = g_path.empty() ? "main.cpp" : g_path;
    req.options = g_options;
    req.options.useNative = native;
    req.useNative = native;          // 编译流程读的是这个字段
    req.arch = g_options.arch;
    req.chem = g_chem;
    req.purity = g_purity;
    req.workDir = ".";

    if (req.source.empty()) {
        appendOutput("没有可编译的代码。", CLR_ERR);
        return;
    }

    g_last = build(req);

    if (showC) {
        appendOutput("==== 生成的 C 代码 ====", CLR_TYPE, true);
        appendOutput(g_last.ccode);
        return;
    }

    char head[256];
    std::snprintf(head, sizeof(head), "冶炼条件：%s  [%s]", g_last.equation.c_str(),
                  g_last.chemName.c_str());
    appendOutput(head, CLR_TYPE, true);
    {
        std::string sym = g_chem.substr(0, g_chem.find('+'));
        std::snprintf(head, sizeof(head), "-%s  %s  （%s）",
                      sym.c_str(), g_last.chemName.c_str(),
                      native ? "原生编译" : "内置解释器");
    }
    appendOutput(head, CLR_TYPE);

    if (g_last.waitMs > 0) {
        std::snprintf(head, sizeof(head),
                      "钴资源排队：空气里 Co2+ 只占 0.04%，程序启动前先等 %d ms",
                      g_last.waitMs);
        appendOutput(head, RGB(163, 95, 0), true);
    }

    for (auto& d : g_last.diagnostics) {
        char line[512];
        std::snprintf(line, sizeof(line), "[%s] %s%s", d.code.c_str(),
                      d.line ? ("第 " + std::to_string(d.line) + " 行: ").c_str() : "",
                      d.message.c_str());
        appendOutput(line, d.sev == Diagnostic::Error ? CLR_ERR : RGB(138, 82, 8));
    }

    if (!g_last.ok) {
        appendOutput("");
        appendOutput("构建失败：" + g_last.stage + " - " + g_last.message, CLR_ERR, true);
        return;
    }

    // 编译成功：原生模式直接在控制台窗口里跑
    if (native && !g_last.exePath.empty()) {
        appendOutput("");
        if (!runAfter) {
            appendOutput("编译完成，产物：" + g_last.exePath, CLR_TYPE, true);
        } else if (launchConsole(g_last.exePath)) {
            appendOutput("已在控制台窗口中启动：" + g_last.exePath, CLR_TYPE, true);
            appendOutput("（程序结束后窗口会停住，按任意键关闭）", RGB(120, 120, 115));
        } else {
            appendOutput("无法打开控制台窗口，改为在面板显示输出。", RGB(138, 82, 8));
            for (auto& line : g_last.output) appendOutput(line);
        }
        char done[128];
        std::snprintf(done, sizeof(done), "用时 %.0f ms。", g_last.durationMs);
        appendOutput(done, RGB(120, 120, 115));
        return;
    }

    // 解释器模式：输出直接进面板
    appendOutput("");
    appendOutput("==== 运行输出 ====", CLR_TYPE, true);
    for (auto& line : g_last.output) appendOutput(line);

    char tail[256];
    std::snprintf(tail, sizeof(tail), "\n完成，用时 %.0f ms。", g_last.durationMs);
    appendOutput(tail, RGB(120, 120, 115));

    if (!g_last.exePath.empty())
        appendOutput("可执行文件：" + g_last.exePath, RGB(120, 120, 115));
}



// ---------------------------------------------------------------- 工具栏图标

void fillRectColor(HDC dc, int x, int y, int w, int h, COLORREF c) {
    RECT r{x, y, x + w, y + h};
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

void frameRectColor(HDC dc, int x, int y, int w, int h, COLORREF c) {
    RECT r{x, y, x + w, y + h};
    HBRUSH b = CreateSolidBrush(c);
    FrameRect(dc, &r, b);
    DeleteObject(b);
}

void drawToolIcon(HDC dc, int id, int x, int y) {
    switch (id) {
        case IDM_NEW: {   // 白纸
            fillRectColor(dc, x + 3, y + 1, 10, 14, RGB(255, 255, 255));
            frameRectColor(dc, x + 3, y + 1, 10, 14, RGB(110, 110, 110));
            fillRectColor(dc, x + 9, y + 1, 4, 4, RGB(225, 225, 225));
            for (int i = 0; i < 3; ++i)
                fillRectColor(dc, x + 5, y + 6 + i * 3, 6, 1, RGB(150, 150, 150));
            break;
        }
        case IDM_OPEN: {  // 黄文件夹
            fillRectColor(dc, x + 2, y + 4, 12, 10, RGB(250, 200, 90));
            frameRectColor(dc, x + 2, y + 4, 12, 10, RGB(170, 120, 30));
            fillRectColor(dc, x + 2, y + 2, 5, 3, RGB(250, 200, 90));
            frameRectColor(dc, x + 2, y + 2, 5, 3, RGB(170, 120, 30));
            fillRectColor(dc, x + 4, y + 7, 8, 1, RGB(170, 120, 30));
            break;
        }
        case IDM_SAVE: {  // 蓝软盘
            fillRectColor(dc, x + 2, y + 2, 12, 12, RGB(70, 110, 180));
            frameRectColor(dc, x + 2, y + 2, 12, 12, RGB(35, 60, 110));
            fillRectColor(dc, x + 4, y + 2, 8, 4, RGB(240, 240, 240));
            fillRectColor(dc, x + 8, y + 2, 3, 4, RGB(70, 110, 180));
            fillRectColor(dc, x + 4, y + 9, 8, 5, RGB(240, 240, 240));
            break;
        }
        case IDM_COMPILE: {  // 灰色方块 + 齿轮齿
            fillRectColor(dc, x + 3, y + 3, 10, 10, RGB(150, 150, 150));
            frameRectColor(dc, x + 3, y + 3, 10, 10, RGB(90, 90, 90));
            fillRectColor(dc, x + 6, y + 6, 4, 4, RGB(90, 90, 90));
            break;
        }
        case IDM_RUN: {   // 绿色三角
            POINT p[3] = {{x + 4, y + 2}, {x + 4, y + 14}, {x + 14, y + 8}};
            HBRUSH b = CreateSolidBrush(RGB(40, 160, 70));
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(20, 110, 45));
            HBRUSH ob = (HBRUSH)SelectObject(dc, b);
            HPEN op = (HPEN)SelectObject(dc, pen);
            Polygon(dc, p, 3);
            SelectObject(dc, ob);
            SelectObject(dc, op);
            DeleteObject(b);
            DeleteObject(pen);
            break;
        }
        case IDM_BUILD: {  // 三角 + 方块
            POINT p[3] = {{x + 1, y + 2}, {x + 1, y + 12}, {x + 9, y + 7}};
            HBRUSH b = CreateSolidBrush(RGB(40, 160, 70));
            HBRUSH ob = (HBRUSH)SelectObject(dc, b);
            Polygon(dc, p, 3);
            SelectObject(dc, ob);
            DeleteObject(b);
            fillRectColor(dc, x + 10, y + 4, 4, 8, RGB(70, 110, 180));
            break;
        }
        case IDM_DEBUG: { // 红色虫子
            fillRectColor(dc, x + 5, y + 5, 6, 7, RGB(200, 60, 60));
            frameRectColor(dc, x + 5, y + 5, 6, 7, RGB(130, 30, 30));
            fillRectColor(dc, x + 2, y + 7, 3, 1, RGB(130, 30, 30));
            fillRectColor(dc, x + 11, y + 7, 3, 1, RGB(130, 30, 30));
            fillRectColor(dc, x + 2, y + 10, 3, 1, RGB(130, 30, 30));
            fillRectColor(dc, x + 11, y + 10, 3, 1, RGB(130, 30, 30));
            fillRectColor(dc, x + 7, y + 2, 2, 3, RGB(130, 30, 30));
            break;
        }
        case IDM_OPTIONS: {  // 齿轮
            fillRectColor(dc, x + 5, y + 5, 6, 6, RGB(120, 120, 120));
            fillRectColor(dc, x + 7, y + 7, 2, 2, RGB(250, 250, 247));
            fillRectColor(dc, x + 7, y + 1, 2, 3, RGB(120, 120, 120));
            fillRectColor(dc, x + 7, y + 12, 2, 3, RGB(120, 120, 120));
            fillRectColor(dc, x + 1, y + 7, 3, 2, RGB(120, 120, 120));
            fillRectColor(dc, x + 12, y + 7, 3, 2, RGB(120, 120, 120));
            break;
        }
        case IDM_FIND: {  // 放大镜
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(70, 110, 180));
            HPEN op = (HPEN)SelectObject(dc, pen);
            HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
            Ellipse(dc, x + 3, y + 3, x + 11, y + 11);
            MoveToEx(dc, x + 10, y + 10, nullptr);
            LineTo(dc, x + 14, y + 14);
            SelectObject(dc, op);
            SelectObject(dc, ob);
            DeleteObject(pen);
            break;
        }
        default: {
            fillRectColor(dc, x + 4, y + 4, 8, 8, RGB(190, 190, 190));
            break;
        }
    }
}

void drawToolButton(LPDRAWITEMSTRUCT dis) {
    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;

    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool disabled = (dis->itemState & ODS_DISABLED) != 0;

    COLORREF bg = pressed ? RGB(206, 220, 240) : RGB(243, 243, 239);
    HBRUSH b = CreateSolidBrush(bg);
    FillRect(dc, &rc, b);
    DeleteObject(b);

    if (pressed) {
        frameRectColor(dc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                       RGB(140, 160, 190));
    }

    int iconY = rc.top + (rc.bottom - rc.top - 16) / 2;
    int iconX = rc.left + (rc.right - rc.left - 16) / 2;
    drawToolIcon(dc, dis->CtlID, iconX, iconY);

    if (disabled) {
        // 简单灰化
        RECT r = rc;
        HBRUSH w = CreateSolidBrush(RGB(255, 255, 255));
        (void)w;
        DeleteObject(w);
        (void)r;
    }
}

// ---------------------------------------------------------------- 查找

namespace {

struct FindCtl {
    HWND hwnd = nullptr;
    HWND edit = nullptr;
    std::string text;
    bool ok = false;
};

FindCtl g_find;

LRESULT CALLBACK FindProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            HINSTANCE inst = (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE);
            CreateWindowA("STATIC", "查找内容：", WS_CHILD | WS_VISIBLE,
                          14, 14, 80, 20, hwnd, nullptr, inst, nullptr);
            g_find.edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                          100, 12, 220, 24, hwnd, (HMENU)4001, inst, nullptr);
            CreateWindowA("BUTTON", "查找下一个", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                          160, 52, 90, 26, hwnd, (HMENU)4002, inst, nullptr);
            CreateWindowA("BUTTON", "取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          256, 52, 70, 26, hwnd, (HMENU)4003, inst, nullptr);
            SetFocus(g_find.edit);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wp) == 4002) {
                g_find.text = getEditText(g_find.edit);
                g_find.ok = !g_find.text.empty();
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wp) == 4003) {
                DestroyWindow(hwnd);
                return 0;
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_find.hwnd = nullptr;
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

bool askFindText(HWND parent, std::string& out) {
    static bool registered = false;
    HINSTANCE inst = (HINSTANCE)GetWindowLongPtrA(parent, GWLP_HINSTANCE);
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = FindProc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "co2cxxFind";
        RegisterClassA(&wc);
        registered = true;
    }

    g_find.ok = false;
    g_find.text.clear();

    RECT pr;
    GetWindowRect(parent, &pr);
    int w = 340, h = 120;
    int x = pr.left + (pr.right - pr.left - w) / 2;
    int y = pr.top + (pr.bottom - pr.top - h) / 2;

    g_find.hwnd = CreateWindowExA(WS_EX_DLGMODALFRAME, "co2cxxFind", "查找",
                                  WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                  x, y, w, h, parent, nullptr, inst, nullptr);
    if (!g_find.hwnd) return false;

    EnableWindow(parent, FALSE);
    ShowWindow(g_find.hwnd, SW_SHOW);
    UpdateWindow(g_find.hwnd);

    MSG msg;
    while (g_find.hwnd && GetMessageA(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageA(g_find.hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    EnableWindow(parent, TRUE);
    SetActiveWindow(parent);

    out = g_find.text;
    return g_find.ok;
}

}  // namespace

bool doFindNext(const std::string& needle) {
    if (needle.empty()) return false;

    CHARRANGE sel{};
    SendMessageA(g_edit, EM_EXGETSEL, 0, (LPARAM)&sel);

    FINDTEXTEXA ft{};
    ft.chrg.cpMin = sel.cpMax;
    ft.chrg.cpMax = -1;
    ft.lpstrText = needle.c_str();

    LRESULT hit = SendMessageA(g_edit, EM_FINDTEXTEX, FR_DOWN, (LPARAM)&ft);
    if (hit == -1) {
        // 到底了，从头再找一次
        ft.chrg.cpMin = 0;
        ft.chrg.cpMax = -1;
        hit = SendMessageA(g_edit, EM_FINDTEXTEX, FR_DOWN, (LPARAM)&ft);
        if (hit == -1) return false;
    }

    CHARRANGE found{ft.chrgText.cpMin, ft.chrgText.cpMax};
    SendMessageA(g_edit, EM_EXSETSEL, 0, (LPARAM)&found);
    SendMessageA(g_edit, EM_SCROLLCARET, 0, 0);
    SetFocus(g_edit);
    return true;
}

void formatCode() {
    std::string src = getEditText(g_edit);
    if (src.empty()) return;

    std::string out;
    int indent = 0;
    size_t i = 0, n = src.size();

    auto trim = [](const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r");
        if (a == std::string::npos) return std::string();
        size_t b = s.find_last_not_of(" \t\r");
        return s.substr(a, b - a + 1);
    };

    while (i < n) {
        size_t nl = src.find('\n', i);
        std::string line = src.substr(i, nl == std::string::npos ? std::string::npos : nl - i);
        i = (nl == std::string::npos) ? n : nl + 1;

        std::string t = trim(line);
        if (t.empty()) { out += "\r\n"; continue; }

        bool startsClose = (t[0] == '}');
        if (startsClose && indent > 0) indent--;

        out.append(indent * 4, ' ');
        out += t;

        int opens = 0, closes = 0;
        bool inStr = false;
        for (size_t k = 0; k < t.size(); ++k) {
            char c = t[k];
            if (c == '"') inStr = !inStr;
            if (inStr) continue;
            if (c == '/' && k + 1 < t.size() && t[k + 1] == '/') break;
            if (c == '{') opens++;
            if (c == '}') closes++;
        }
        if (startsClose) closes--;
        indent += opens - closes;
        if (indent < 0) indent = 0;

        out += "\r\n";
    }

    setEditText(g_edit, out);
    highlight();
    appendOutput("已按大括号缩进格式化。", RGB(120, 120, 115));
}

// ---------------------------------------------------------------- 文件操作

void doNew() {
    setEditText(g_edit, "");
    g_path.clear();
    g_dirty = false;
    SetWindowTextA(g_main, "co2cxx - 未命名");
    highlight();
}

void doOpen() {
    char file[MAX_PATH] = "";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main;
    ofn.lpstrFilter = "C++ 源文件\0*.cpp;*.cc;*.cxx\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) return;

    std::string text = readFile(file);
    setEditText(g_edit, text);
    g_path = file;
    g_dirty = false;
    std::string title = "co2cxx - " + g_path;
    SetWindowTextA(g_main, title.c_str());
    highlight();
}

void doSave(bool saveAs) {
    std::string path = g_path;
    if (saveAs || path.empty()) {
        char file[MAX_PATH] = "";
        if (!g_path.empty()) std::snprintf(file, sizeof(file), "%s", g_path.c_str());
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_main;
        ofn.lpstrFilter = "C++ 源文件\0*.cpp\0所有文件\0*.*\0";
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = "cpp";
        ofn.Flags = OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameA(&ofn)) return;
        path = file;
    }
    if (writeFile(path, getEditText(g_edit))) {
        g_path = path;
        g_dirty = false;
        std::string title = "co2cxx - " + g_path;
        SetWindowTextA(g_main, title.c_str());
    }
}

// ---------------------------------------------------------------- 菜单

void buildThemeMenu(HMENU parent);   // 实现在主题段，那里能访问 THEMES

HMENU buildMenu() {
    HMENU bar = CreateMenu();

    // 文件
    HMENU file = CreatePopupMenu();
    AppendMenuA(file, MF_STRING, IDM_NEW, "新建(&N)\tCtrl+N");
    AppendMenuA(file, MF_STRING, IDM_OPEN, "打开(&O)...\tCtrl+O");
    AppendMenuA(file, MF_STRING, IDM_SAVE, "保存(&S)\tCtrl+S");
    AppendMenuA(file, MF_STRING, IDM_SAVEAS, "另存为(&A)...");
    AppendMenuA(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(file, MF_STRING, IDM_EXIT, "退出(&X)\tAlt+F4");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)file, "文件(&F)");

    // 编辑
    HMENU edit = CreatePopupMenu();
    AppendMenuA(edit, MF_STRING, IDM_UNDO, "撤销(&U)\tCtrl+Z");
    AppendMenuA(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(edit, MF_STRING, IDM_CUT, "剪切(&T)\tCtrl+X");
    AppendMenuA(edit, MF_STRING, IDM_COPY, "复制(&C)\tCtrl+C");
    AppendMenuA(edit, MF_STRING, IDM_PASTE, "粘贴(&P)\tCtrl+V");
    AppendMenuA(edit, MF_STRING, IDM_SELECTALL, "全选(&A)\tCtrl+A");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)edit, "编辑(&E)");

    // 搜索
    HMENU search = CreatePopupMenu();
    AppendMenuA(search, MF_STRING, IDM_FIND, "查找(&F)...\tCtrl+F");
    AppendMenuA(search, MF_STRING, IDM_REPLACE, "替换(&R)...\tCtrl+H");
    AppendMenuA(search, MF_STRING, IDM_FINDNEXT, "查找下一个(&N)\tF3");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)search, "搜索(&S)");

    // 视图
    HMENU view = CreatePopupMenu();
    HMENU themeMenu = CreatePopupMenu();
    buildThemeMenu(themeMenu);
    AppendMenuA(view, MF_POPUP, (UINT_PTR)themeMenu, "配色主题(&T)");
    AppendMenuA(view, MF_STRING, IDM_FONT, "选择字体(&F)...");
    AppendMenuA(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(view, MF_STRING, IDM_TOGGLE_TREE, "项目树(&P)");
    AppendMenuA(view, MF_STRING, IDM_TOGGLE_OUTPUT, "输出面板(&O)");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)view, "视图(&V)");

    // 项目
    HMENU proj = CreatePopupMenu();
    AppendMenuA(proj, MF_STRING, IDM_NEWPROJ, "新建项目(&N)...");
    AppendMenuA(proj, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(proj, MF_STRING, IDM_ADDFILE, "添加文件(&A)...");
    AppendMenuA(proj, MF_STRING, IDM_REMOVEFILE, "移除文件(&R)");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)proj, "项目(&P)");

    // 运行
    HMENU run = CreatePopupMenu();
    AppendMenuA(run, MF_STRING, IDM_COMPILE, "编译(&C)\tF9");
    AppendMenuA(run, MF_STRING, IDM_RUN, "运行(&R)\tCtrl+F10");
    AppendMenuA(run, MF_STRING, IDM_BUILD, "编译运行(&B)\tF11");
    AppendMenuA(run, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(run, MF_STRING, IDM_VIEW_C, "查看生成的 C 代码");
    AppendMenuA(run, MF_STRING, IDM_DEBUG, "调试(&D)\tF5");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)run, "运行(&R)");

    // 工具
    HMENU tools = CreatePopupMenu();
    AppendMenuA(tools, MF_STRING, IDM_OPTIONS, "编译器选项(&O)...");
    AppendMenuA(tools, MF_SEPARATOR, 0, nullptr);

    HMENU chem = CreatePopupMenu();
    AppendMenuA(chem, MF_STRING, IDM_CHEM_CO2, "Fe   铁 · 稳健");
    AppendMenuA(chem, MF_STRING, IDM_CHEM_CO, "Na   钠 · 激进");
    AppendMenuA(chem, MF_STRING, IDM_CHEM_CO2O2, "Mn   锰 · 变价");
    AppendMenuA(chem, MF_STRING, IDM_CHEM_CO2C, "Cu   铜 · 温和");
    AppendMenuA(chem, MF_STRING, IDM_METAL_CO, "Co   钴 · 最强但稀缺（排队 300 ms）");
    AppendMenuA(chem, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(chem, MF_STRING, IDM_CHEM_CO2H2O, "Na + H2O   钠遇水（剧烈）");
    AppendMenuA(chem, MF_STRING, IDM_METAL_FEO2, "Fe + O2    铁生锈（衰减）");
    AppendMenuA(chem, MF_STRING, IDM_METAL_MNHCL, "Mn + HCl   制氯气（最强）");
    AppendMenuA(tools, MF_POPUP, (UINT_PTR)chem, "冶炼金属(&M)");

    HMENU std = CreatePopupMenu();
    AppendMenuA(std, MF_STRING, IDM_STD_11, "C++11");
    AppendMenuA(std, MF_STRING, IDM_STD_14, "C++14");
    AppendMenuA(std, MF_STRING, IDM_STD_17, "C++17");
    AppendMenuA(std, MF_STRING, IDM_STD_20, "C++20");
    AppendMenuA(tools, MF_POPUP, (UINT_PTR)std, "语言标准(&S)");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)tools, "工具(&T)");

    // AStyle
    HMENU astyle = CreatePopupMenu();
    AppendMenuA(astyle, MF_STRING, IDM_ASTYLE, "格式化当前文件(&F)\tCtrl+Shift+A");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)astyle, "AStyle");

    // 窗口
    HMENU win = CreatePopupMenu();
    AppendMenuA(win, MF_STRING, IDM_MINIMIZE, "最小化(&M)");
    AppendMenuA(win, MF_STRING, IDM_CASCADE, "层叠(&C)");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)win, "窗口(&W)");

    // 帮助
    HMENU help = CreatePopupMenu();
    AppendMenuA(help, MF_STRING, IDM_ABOUT, "关于(&A)");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)help, "帮助(&H)");

    return bar;
}


// ---------------------------------------------------------------- 行号栏

const int GUTTER_W = 48;

void drawGutter() {
    if (!g_gutter || !g_edit) return;
    HDC dc = GetDC(g_gutter);
    RECT rc;
    GetClientRect(g_gutter, &rc);

    HBRUSH bg = CreateSolidBrush(theme().gutterBg);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, theme().gutterText);
    HFONT font = (HFONT)SendMessageA(g_edit, WM_GETFONT, 0, 0);
    HFONT old = (HFONT)SelectObject(dc, font);

    POINTL pt = {0, 0};
    int charIdx = (int)SendMessageA(g_edit, EM_CHARFROMPOS, 0, (LPARAM)&pt);
    if (charIdx < 0) charIdx = 0;
    int firstLine = (int)SendMessageA(g_edit, EM_EXLINEFROMCHAR, 0, charIdx);
    int totalLines = (int)SendMessageA(g_edit, EM_GETLINECOUNT, 0, 0);

    int pos = (int)SendMessageA(g_edit, EM_POSFROMCHAR, charIdx, 0);
    int y = (short)HIWORD(pos);
    if (y > 0) y = 0;

    for (int ln = firstLine; ln < totalLines; ++ln) {
        if (y > rc.bottom) break;
        if (y + g_lineHeight >= 0) {
            if (g_breakpoints.count(ln + 1)) {
                HBRUSH bb = CreateSolidBrush(RGB(200, 40, 40));
                RECT br{2, y + 2, 10, y + g_lineHeight - 2};
                FillRect(dc, &br, bb);
                DeleteObject(bb);
            }
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%d", ln + 1);
            RECT tr{0, y, GUTTER_W - 6, y + g_lineHeight};
            DrawTextA(dc, buf, -1, &tr, DT_RIGHT | DT_TOP | DT_SINGLELINE);
        }
        y += g_lineHeight;
    }

    SelectObject(dc, old);
    ReleaseDC(g_gutter, dc);
}

LRESULT CALLBACK GutterProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            drawGutter();
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_LBUTTONDOWN: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (g_edit) {
                POINTL pl{0, pt.y};
                int ci = (int)SendMessageA(g_edit, EM_CHARFROMPOS, 0, (LPARAM)&pl);
                if (ci >= 0) {
                    int ln = (int)SendMessageA(g_edit, EM_EXLINEFROMCHAR, 0, ci) + 1;
                    if (g_breakpoints.count(ln)) g_breakpoints.erase(ln);
                    else g_breakpoints.insert(ln);
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            }
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

void registerGutter(HINSTANCE inst) {
    WNDCLASSA wc{};
    wc.lpfnWndProc = GutterProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = "co2cxxGutter";
    RegisterClassA(&wc);
}

// ---------------------------------------------------------------- 选项对话框

enum {
    IDC_STD = 3001,
    IDC_OPT,
    IDC_WALL,
    IDC_WEXTRA,
    IDC_WERROR,
    IDC_DEBUG,
    IDC_DEFINES,
    IDC_OK,
    IDC_CANCEL,
    IDC_RESET,
};

struct OptCtl {
    HWND hwnd = nullptr;
    HWND stdBox = nullptr;
    HWND optBox = nullptr;
    HWND wall = nullptr;
    HWND extra = nullptr;
    HWND werror = nullptr;
    HWND debug = nullptr;
    HWND defines = nullptr;
    bool accepted = false;
};

OptCtl g_opt;

void optFill() {
    const char* stds[] = {"c++98", "c++03", "c++11", "c++14", "c++17", "c++20"};
    int sel = 4;
    for (int i = 0; i < 6; ++i) {
        SendMessageA(g_opt.stdBox, CB_ADDSTRING, 0, (LPARAM)stds[i]);
        if (g_options.standard == stds[i]) sel = i;
    }
    SendMessageA(g_opt.stdBox, CB_SETCURSEL, sel, 0);

    const char* modes[] = {"co2", "O0", "O1", "O2", "O3", "Os"};
    const char* labels[] = {"CO2 自动（由大气浓度决定）", "O0  不优化", "O1  基础优化",
                            "O2  标准优化", "O3  激进优化", "Os  优化体积"};
    int msel = 0;
    for (int i = 0; i < 6; ++i) {
        SendMessageA(g_opt.optBox, CB_ADDSTRING, 0, (LPARAM)labels[i]);
        if (g_options.optimization == modes[i]) msel = i;
    }
    SendMessageA(g_opt.optBox, CB_SETCURSEL, msel, 0);

    SendMessageA(g_opt.wall, BM_SETCHECK, g_options.warnAll ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_opt.extra, BM_SETCHECK, g_options.warnExtra ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_opt.werror, BM_SETCHECK, g_options.warnError ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_opt.debug, BM_SETCHECK, g_options.debugInfo ? BST_CHECKED : BST_UNCHECKED, 0);

    std::string defs;
    for (auto& kv : g_options.defines) {
        if (!defs.empty()) defs += "\r\n";
        defs += kv.first + "=" + kv.second;
    }
    SetWindowTextA(g_opt.defines, defs.c_str());
}

void optApply() {
    const char* stds[] = {"c++98", "c++03", "c++11", "c++14", "c++17", "c++20"};
    const char* modes[] = {"co2", "O0", "O1", "O2", "O3", "Os"};

    int si = (int)SendMessageA(g_opt.stdBox, CB_GETCURSEL, 0, 0);
    if (si >= 0 && si < 6) g_options.standard = stds[si];
    int mi = (int)SendMessageA(g_opt.optBox, CB_GETCURSEL, 0, 0);
    if (mi >= 0 && mi < 6) g_options.optimization = modes[mi];

    g_options.warnAll = SendMessageA(g_opt.wall, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_options.warnExtra = SendMessageA(g_opt.extra, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_options.warnError = SendMessageA(g_opt.werror, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_options.debugInfo = SendMessageA(g_opt.debug, BM_GETCHECK, 0, 0) == BST_CHECKED;

    std::string text = getEditText(g_opt.defines);
    g_options.defines.clear();
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? text.size() + 1 : nl + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) g_options.defines.push_back({line, "1"});
        else g_options.defines.push_back({line.substr(0, eq), line.substr(eq + 1)});
    }
}

void optCreate(HWND parent) {
    HINSTANCE inst = (HINSTANCE)GetWindowLongPtrA(parent, GWLP_HINSTANCE);
    int x = 20, y = 18;

    CreateWindowA("STATIC", "语言标准", WS_CHILD | WS_VISIBLE, x, y + 3, 90, 20, parent, nullptr, inst, nullptr);
    g_opt.stdBox = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                 x + 100, y, 160, 200, parent, (HMENU)IDC_STD, inst, nullptr);

    y += 34;
    CreateWindowA("STATIC", "优化模式", WS_CHILD | WS_VISIBLE, x, y + 3, 90, 20, parent, nullptr, inst, nullptr);
    g_opt.optBox = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                 x + 100, y, 240, 200, parent, (HMENU)IDC_OPT, inst, nullptr);

    y += 40;
    CreateWindowA("STATIC", "警告", WS_CHILD | WS_VISIBLE, x, y, 90, 20, parent, nullptr, inst, nullptr);
    g_opt.wall = CreateWindowA("BUTTON", "常用警告 (-Wall)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                               x + 100, y, 150, 20, parent, (HMENU)IDC_WALL, inst, nullptr);
    g_opt.extra = CreateWindowA("BUTTON", "额外警告 (-Wextra)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                x + 260, y, 150, 20, parent, (HMENU)IDC_WEXTRA, inst, nullptr);
    y += 24;
    g_opt.werror = CreateWindowA("BUTTON", "警告视为错误 (-Werror)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                 x + 100, y, 155, 20, parent, (HMENU)IDC_WERROR, inst, nullptr);
    g_opt.debug = CreateWindowA("BUTTON", "生成调试信息 (-g)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                x + 260, y, 160, 20, parent, (HMENU)IDC_DEBUG, inst, nullptr);

    y += 30;
    CreateWindowA("STATIC", "宏定义", WS_CHILD | WS_VISIBLE, x, y, 90, 20, parent, nullptr, inst, nullptr);
    CreateWindowA("STATIC", "每行一个：NAME=VALUE", WS_CHILD | WS_VISIBLE,
                  x + 100, y, 240, 20, parent, nullptr, inst, nullptr);
    y += 22;
    g_opt.defines = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
                                    x + 100, y, 300, 70, parent, (HMENU)IDC_DEFINES, inst, nullptr);

    y += 84;
    CreateWindowA("BUTTON", "恢复默认", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  x, y, 90, 26, parent, (HMENU)IDC_RESET, inst, nullptr);
    CreateWindowA("BUTTON", "确定", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  x + 240, y, 80, 26, parent, (HMENU)IDC_OK, inst, nullptr);
    CreateWindowA("BUTTON", "取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  x + 330, y, 80, 26, parent, (HMENU)IDC_CANCEL, inst, nullptr);
}

LRESULT CALLBACK OptProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            optCreate(hwnd);
            optFill();
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDC_OK:
                    optApply();
                    g_opt.accepted = true;
                    DestroyWindow(hwnd);
                    return 0;
                case IDC_CANCEL:
                    DestroyWindow(hwnd);
                    return 0;
                case IDC_RESET: {
                    Options def;
                    g_options.standard = def.standard;
                    g_options.optimization = def.optimization;
                    g_options.warnAll = def.warnAll;
                    g_options.warnExtra = false;
                    g_options.warnError = false;
                    g_options.debugInfo = false;
                    g_options.defines.clear();
                    optFill();
                    return 0;
                }
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            g_opt.hwnd = nullptr;
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

void showOptions(HWND parent) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSA wc{};
        wc.lpfnWndProc = OptProc;
        wc.hInstance = (HINSTANCE)GetWindowLongPtrA(parent, GWLP_HINSTANCE);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "co2cxxOptions";
        RegisterClassA(&wc);
        registered = true;
    }

    g_opt.accepted = false;
    RECT pr;
    GetWindowRect(parent, &pr);
    int w = 480, h = 330;
    int x = pr.left + (pr.right - pr.left - w) / 2;
    int y = pr.top + (pr.bottom - pr.top - h) / 2;

    g_opt.hwnd = CreateWindowExA(WS_EX_DLGMODALFRAME, "co2cxxOptions", "编译器选项",
                                 WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                 x, y, w, h, parent, nullptr,
                                 (HINSTANCE)GetWindowLongPtrA(parent, GWLP_HINSTANCE), nullptr);
    if (!g_opt.hwnd) return;

    EnableWindow(parent, FALSE);
    ShowWindow(g_opt.hwnd, SW_SHOW);
    UpdateWindow(g_opt.hwnd);

    MSG msg;
    while (g_opt.hwnd && GetMessageA(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageA(g_opt.hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetActiveWindow(parent);
    updateStatus();
}

// ---------------------------------------------------------------- 布局

void layout(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;

    int statusY = h - STATUS_H;
    int outH = g_showOutput ? g_outPanelH : 0;
    int tabY = statusY - outH;
    int editTop = TOOLBAR_H;
    int editBottom = tabY - (g_showOutput ? TAB_H : 0);
    if (editBottom < editTop + 40) editBottom = editTop + 40;

    // 工具栏按钮：两排
    for (int i = 0; i < g_toolCount; ++i) {
        if (g_tools[i].hwnd)
            MoveWindow(g_tools[i].hwnd, g_tools[i].x, g_tools[i].y, 28, 25, TRUE);
    }

    int treeW = g_showTree ? TREE_W : 0;
    if (g_tree) {
        ShowWindow(g_tree, g_showTree ? SW_SHOW : SW_HIDE);
        MoveWindow(g_tree, 0, editTop, treeW, editBottom - editTop, TRUE);
    }

    if (g_gutter)
        MoveWindow(g_gutter, treeW, editTop, GUTTER_W, editBottom - editTop, TRUE);
    if (g_edit)
        MoveWindow(g_edit, treeW + GUTTER_W, editTop,
                   w - treeW - GUTTER_W, editBottom - editTop, TRUE);

    if (g_tab) {
        ShowWindow(g_tab, g_showOutput ? SW_SHOW : SW_HIDE);
        MoveWindow(g_tab, 0, editBottom, w, TAB_H, TRUE);
    }
    if (g_output) {
        ShowWindow(g_output, g_showOutput ? SW_SHOW : SW_HIDE);
        MoveWindow(g_output, 0, tabY, w, outH, TRUE);
    }
    if (g_config) MoveWindow(g_config, w - 300, 5, 290, 200, TRUE);

    if (g_status) {
        MoveWindow(g_status, 0, statusY, w, STATUS_H, TRUE);
        int parts[6];
        parts[0] = 120;
        parts[1] = 210;
        parts[2] = 310;
        parts[3] = 400;
        parts[4] = 470;
        parts[5] = -1;
        SendMessageA(g_status, SB_SETPARTS, 6, (LPARAM)parts);
    }
    drawGutter();
}

// ---------------------------------------------------------------- 窗口过程

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g_riched = LoadLibraryA("riched20.dll");
            registerGutter((HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE));

            g_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "RichEdit20A", "",
                                     WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                     ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
                                     0, 0, 100, 100, hwnd, (HMENU)ID_EDIT, nullptr, nullptr);

            g_output = CreateWindowExA(WS_EX_CLIENTEDGE, "RichEdit20A", "",
                                       WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                       ES_MULTILINE | ES_READONLY,
                                       0, 0, 100, 100, hwnd, (HMENU)ID_OUTPUT, nullptr, nullptr);

            g_gutter = CreateWindowExA(0, "co2cxxGutter", "",
                                       WS_CHILD | WS_VISIBLE,
                                       0, 0, GUTTER_W, 100, hwnd, nullptr, nullptr, nullptr);

            g_tree = CreateWindowExA(WS_EX_CLIENTEDGE, WC_TREEVIEWA, "",
                                     WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES |
                                     TVS_LINESATROOT | TVS_SHOWSELALWAYS,
                                     0, 0, TREE_W, 100, hwnd, (HMENU)ID_TREE,
                                     (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE), nullptr);
            {
                TVINSERTSTRUCTA tv{};
                tv.hParent = TVI_ROOT;
                tv.hInsertAfter = TVI_LAST;
                tv.item.mask = TVIF_TEXT;
                tv.item.pszText = (LPSTR)"main.cpp";
                SendMessageA(g_tree, TVM_INSERTITEM, 0, (LPARAM)&tv);
                SendMessageA(g_tree, TVM_EXPAND, TVE_EXPAND, 0);
            }

            g_tab = CreateWindowExA(0, WC_TABCONTROLA, "",
                                    WS_CHILD | WS_VISIBLE | TCS_FIXEDWIDTH,
                                    0, 0, 100, TAB_H, hwnd, (HMENU)ID_TAB,
                                    (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE), nullptr);
            {
                const char* pages[] = {"编译器", "资源", "编译日志", "调试", "搜索结果"};
                for (int i = 0; i < 5; ++i) {
                    TCITEMA tie{};
                    tie.mask = TCIF_TEXT;
                    tie.pszText = (LPSTR)pages[i];
                    SendMessageA(g_tab, TCM_INSERTITEM, i, (LPARAM)&tie);
                }
            }

            g_config = CreateWindowA("COMBOBOX", "",
                                     WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                     0, 0, 240, 200, hwnd, (HMENU)5001,
                                     (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE), nullptr);
            rebuildConfigList();
            g_status = CreateWindowExA(0, STATUSCLASSNAMEA, "",
                                       WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                       0, 0, 100, 20, hwnd, (HMENU)ID_STATUS, nullptr, nullptr);

            {
                const int ids[] = {
                    IDM_NEW, IDM_OPEN, IDM_SAVE, IDM_CUT, IDM_COPY, IDM_PASTE,
                    IDM_FIND, IDM_OPTIONS,
                    IDM_COMPILE, IDM_RUN, IDM_BUILD, IDM_VIEW_C, IDM_DEBUG,
                    IDM_ASTYLE, IDM_FONT, IDM_ABOUT,
                };
                HINSTANCE tinst = (HINSTANCE)GetWindowLongPtrA(hwnd, GWLP_HINSTANCE);
                for (int i = 0; i < 16; ++i) {
                    int col = i % 8;
                    int row = i / 8;
                    g_tools[i].id = ids[i];
                    g_tools[i].x = 6 + col * 30;
                    g_tools[i].y = 4 + row * 27;
                    g_tools[i].hwnd = CreateWindowA("BUTTON", "",
                                                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                                    g_tools[i].x, g_tools[i].y, 28, 25,
                                                    hwnd, (HMENU)(INT_PTR)ids[i], tinst, nullptr);
                }
                g_toolCount = 16;
                g_btnCompile = g_tools[8].hwnd;
            }

            applyFont();
            SendMessageA(g_edit, EM_SETBKGNDCOLOR, 0, (LPARAM)CLR_BG);
            SendMessageA(g_edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                         MAKELONG(6, 6));
            SendMessageA(g_edit, EM_SETEVENTMASK, 0, ENM_SCROLL | ENM_UPDATE | ENM_CHANGE);
            SendMessageA(g_output, EM_SETBKGNDCOLOR, 0, (LPARAM)RGB(255, 255, 252));

            // 默认示例代码
            const char* sample =
                "// co2cxx —— 用金属冶炼驱动优化的 C++ 子集编译器\n"
                "// 工具 → 冶炼金属：Fe / Na / Mn / Co / Cu\n"
                "// Ctrl+F10 运行，控制台里输入数字后回车，看结果和耗时\n\n"
                "int main() {\n"
                "    print(\"请输入 n:\");\n"
                "    int n = input();\n\n"
                "    int sum = 0;\n"
                "    for (int i = 1; i <= n; i = i + 1) {\n"
                "        sum = sum + i;\n"
                "    }\n\n"
                "    print(sum);\n"
                "    return 0;\n"
                "}\n";
            setEditText(g_edit, sample);

            appendOutput("co2cxx 就绪。F9 编译运行，Ctrl+B 生成可执行文件。", RGB(120, 120, 115));
            layout(hwnd);
            loadSettings();
            applyTheme(g_themeIndex);
            updateStatus();
            return 0;
        }

        case WM_SIZE:
            layout(hwnd);
            drawGutter();
            return 0;

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
            if (dis->CtlType == ODT_BUTTON) {
                drawToolButton(dis);
                return TRUE;
            }
            break;
        }

        case WM_NOTIFY: {
            LPNMHDR nh = (LPNMHDR)lp;
            if (nh->code == EN_VSCROLL || nh->code == EN_UPDATE) drawGutter();
            return 0;
        }

        case WM_SETFOCUS:
            SetFocus(g_edit);
            return 0;

        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id >= 4000 && id < 4000 + THEME_COUNT) {
                applyTheme(id - 4000);
                return 0;
            }
            if (id == 5001 && HIWORD(wp) == CBN_SELCHANGE) {
                int sel = (int)SendMessageA(g_config, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)g_configs.size()) {
                    g_options.arch = g_configs[sel].arch;
                    g_options.debugInfo = g_configs[sel].debug;
                }
                saveSettings();
                updateStatus();
                return 0;
            }
            switch (id) {
                case IDM_COMPILE: runBuild(true, false, false); return 0;
                case IDM_RUN: runBuild(true, false, true); return 0;
                case IDM_BUILD: runBuild(true, false, true); return 0;
                case IDM_VIEW_C: runBuild(false, true, false); return 0;
                case IDM_NEW: doNew(); return 0;
                case IDM_OPEN: doOpen(); return 0;
                case IDM_SAVE: doSave(false); return 0;
                case IDM_SAVEAS: doSave(true); return 0;
                case IDM_EXIT: DestroyWindow(hwnd); return 0;
                case IDM_UNDO: SendMessageA(g_edit, WM_UNDO, 0, 0); return 0;
                case IDM_CUT: SendMessageA(g_edit, WM_CUT, 0, 0); return 0;
                case IDM_COPY: SendMessageA(g_edit, WM_COPY, 0, 0); return 0;
                case IDM_PASTE: SendMessageA(g_edit, WM_PASTE, 0, 0); return 0;
                case IDM_SELECTALL: SendMessageA(g_edit, EM_SETSEL, 0, -1); return 0;

                case IDM_CHEM_CO2: g_chem = "Fe"; break;
                case IDM_CHEM_CO: g_chem = "Na"; break;
                case IDM_CHEM_CO2O2: g_chem = "Mn"; break;
                case IDM_CHEM_CO2C: g_chem = "Cu"; break;
                case IDM_CHEM_CO2H2O: g_chem = "Na+H2O"; break;
                case IDM_METAL_FEO2: g_chem = "Fe+O2"; break;
                case IDM_METAL_MNHCL: g_chem = "Mn+HCl"; break;
                case IDM_METAL_CO: g_chem = "Co"; break;

                case IDM_STD_11: g_options.standard = "c++11"; break;
                case IDM_STD_14: g_options.standard = "c++14"; break;
                case IDM_STD_17: g_options.standard = "c++17"; break;
                case IDM_STD_20: g_options.standard = "c++20"; break;

                case IDM_OPTIONS:
                    showOptions(hwnd);
                    return 0;
                case IDM_FONT:
                    chooseFont(hwnd);
                    return 0;
                case IDM_FIND:
                case IDM_FINDNEXT: {
                    static std::string last;
                    std::string q;
                    if (id == IDM_FIND || last.empty()) {
                        if (!askFindText(hwnd, q)) return 0;
                        last = q;
                    } else {
                        q = last;
                    }
                    if (!doFindNext(q))
                        appendOutput("找不到：" + q, RGB(163, 45, 45));
                    return 0;
                }
                case IDM_REPLACE:
                    MessageBoxA(hwnd, "替换功能开发中。\n\n目前可以用「查找」定位后手动修改。",
                                "替换", MB_OK | MB_ICONINFORMATION);
                    return 0;
                case IDM_ASTYLE:
                    formatCode();
                    return 0;
                case IDM_DEBUG:
                    MessageBoxA(hwnd,
                                "调试器未内置。\n\n"
                                "co2cxx 的默认执行路径是内置解释器，"
                                "出错时会给出具体行号。\n"
                                "需要单步调试时，可以先用「生成 exe」再用外部调试器。",
                                "调试", MB_OK | MB_ICONINFORMATION);
                    return 0;
                case IDM_NEWPROJ:
                case IDM_ADDFILE:
                case IDM_REMOVEFILE:
                    MessageBoxA(hwnd, "项目管理功能开发中。\n\n当前版本支持单文件编辑与编译。",
                                "项目", MB_OK | MB_ICONINFORMATION);
                    return 0;
                case IDM_TOGGLE_TREE:
                    g_showTree = !g_showTree;
                    layout(hwnd);
                    return 0;
                case IDM_TOGGLE_OUTPUT:
                    g_showOutput = !g_showOutput;
                    layout(hwnd);
                    return 0;
                case IDM_MINIMIZE:
                    ShowWindow(hwnd, SW_MINIMIZE);
                    return 0;
                case IDM_CASCADE:
                    MessageBoxA(hwnd, "只有主窗口，没有可层叠的子窗口。", "窗口",
                                MB_OK | MB_ICONINFORMATION);
                    return 0;
                case IDM_ABOUT:
                    MessageBoxA(hwnd,
                                "co2cxx\n"
                                "用金属冶炼驱动优化的 C++ 子集编译器\n\n"
                                "纯度从 50% 的粗矿到 100% 的电解纯金属\n"
                                "金属越活泼、反应越剧烈，编译越激进\n\n"
                                "  Fe      铁 · 稳健\n"
                                "  Na      钠 · 激进\n"
                                "  Mn      锰 · 变价\n"
                                "  Cu      铜 · 温和\n"
                                "  Na+H2O  钠遇水，最剧烈\n"
                                "  Fe+O2   铁生锈，优化衰减\n"
                                "  Mn+HCl  制氯气，最强氧化\n\n"
                                "前端完全自研：词法、语法、语义、优化、代码生成",
                                "关于 co2cxx", MB_OK | MB_ICONINFORMATION);
                    return 0;
            }
            updateStatus();
            return 0;
        }

        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wp;
            SetTextColor(dc, CLR_TEXT);
            SetBkColor(dc, RGB(255, 255, 252));
            static HBRUSH br = CreateSolidBrush(RGB(255, 255, 252));
            return (LRESULT)br;
        }

        case WM_DESTROY:
            saveSettings();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

}  // namespace

int guiMain(HINSTANCE hInstance) {
    // 程序编译成 GUI 子系统，本身没有控制台窗口，不需要额外隐藏

    // RichEdit 需要在创建窗口前加载
    g_riched = LoadLibraryA("riched20.dll");

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_BAR_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(250, 250, 247));
    wc.lpszClassName = "co2cxxMain";
    wc.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(1));
    RegisterClassA(&wc);

    g_main = CreateWindowExA(0, "co2cxxMain", "co2cxx",
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1100, 760,
                             nullptr, buildMenu(), hInstance, nullptr);
    if (!g_main) return 1;

    ShowWindow(g_main, SW_SHOW);
    UpdateWindow(g_main);

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}
