# co2cxx 原生版

**原生 Win32 桌面程序**，用金属冶炼驱动代码优化。不是浏览器应用，不依赖 Python，
不是网页套壳。双击就是一个真正的窗口程序，界面按 Dev-C++ 的布局做。

编译器的优化强度由你选的**金属**和**纯度**决定 —— 铁稳、钠猛、锰变价、铜温和。
选钠加水，优化直接拉满；选铁生锈，优化被锈蚀掉一半。

```
native/build/co2cxx.exe     2.9 MB，单文件，自带图标
```

## 为什么重写

原版是 Python 实现的，虽然能用，但需要装 Python、界面跑在浏览器里。
这一版把编译器和界面全部用 C++ 重写：

| | Python 版 | C++ 原生版 |
|---|---|---|
| 运行依赖 | Python 3.8+ | 无 |
| 界面 | 浏览器 | Win32 原生窗口 |
| 体积 | 需整个运行时 | 单个 2.9 MB exe |
| 启动速度 | 秒级 | 即时 |

## 界面

按 Dev-C++ 5.11 的布局复刻：

```
文件 编辑 搜索 视图 项目 运行 工具 AStyle 窗口 帮助
[图标工具栏 · 两排]                        [co2cxx C++17 CO2 自动 ▾]
┌─────────┬──────────────────────────────────────────────┐
│ 项目树   │  1 │ // co2cxx —— 由大气二氧化碳浓度驱动...     │
│         │  2 │ // F9 编译并运行，Ctrl+B 生成 exe          │
│ main.cpp│  3 │                                          │
│         │  4 │ int square(int x) {                      │
│         │  5 │     return x * x;                        │
│         │  6 │ }                                        │
├─────────┴──────────────────────────────────────────────┤
│ 编译器 │ 资源 │ 编译日志 │ 调试 │ 搜索结果                  │
│ 冶炼条件：Fe -> Fe(+2/+3) + e-  [铁]                       │
│ -Fe  铁  （原生编译）                                      │
│ ==== 运行输出 ====                                       │
│ Hello, CO2!                                              │
├──────────────────────────────────────────────────────────┤
│ 行 1, 列 1 │ 已选择 0 │ 总行数 13 │ 长度 232 │ 插入 │ ... │
└──────────────────────────────────────────────────────────┘
```

## 功能

### 编译与运行

| 操作 | 行为 |
|------|------|
| F9 编译 | 只编译，产物是 `.exe`，不运行 |
| Ctrl+F10 运行 | 编译后在**独立控制台窗口**里运行 |
| F11 编译运行 | 同上 |
| Ctrl+B | 生成 exe 并运行 |

运行结果在独立控制台窗口里，结束后打印退出统计，和 Dev-C++ 一致：

```
Hello, CO2!
25
--------------------------------
Process exited after 0.039 seconds with return value 0
Press any key to continue . . .
```

右上角的下拉是**构建配置**，只显示编译器版本和架构：

```
co2cxx 1.0.0  GCC 16.2.0  64-bit Release
co2cxx 1.0.0  GCC 16.2.0  64-bit Debug
co2cxx 1.0.0  GCC 10.2.0  32-bit Release
co2cxx 1.0.0  GCC 10.2.0  32-bit Debug
```

版本号是运行时从 `gcc --version` 里读的，不是写死的。语言标准、优化等级、
警告开关这些都在「工具 → 编译器选项」里。

### 自带的编译器

安装包自带两套工具链，装完就能编译，不用另外装 MinGW：

| 组件 | 版本 | 用途 |
|------|------|------|
| mingw64 | GCC 16.2.0 (x86_64) | 生成 64 位 exe |
| mingw32 | GCC 10.2.0 (i686) | 生成 32 位 exe |

安装时两个都可以勾选，装完 `%LOCALAPPDATA%\Programs\co2cxx\mingw64` 和 `mingw32`
就在程序目录下，IDE 会自动找到。

> 安装路径刻意不带空格。gcc 内部调用 `ld` 时不做路径转义，
> 装在 `Program Files` 这类带空格的目录下会报 `cannot find ...` 错误。

如果没装自带工具链，IDE 会退回系统 PATH 里的 gcc，再找不到就用内置解释器。

### 编辑

- 语法高亮：关键字 / 类型 / 函数名 / 字符串 / 数字 / 注释，六类着色
- 行号栏，**点行号设断点**（红块标记）
- 自动缩进、Tab 缩进
- 查找（Ctrl+F）、查找下一个（F3），支持循环回绕
- AStyle 菜单：按大括号缩进格式化

### 7 套配色主题（视图 → 配色主题）

| 主题 | 说明 |
|------|------|
| Light | 默认浅色 |
| Classic | Dev-C++ 经典白底，蓝关键字、绿注释 |
| Twilight | 深紫灰 |
| Ocean | 深蓝 |
| Solarized Light | 米黄 |
| Solarized Dark | 深青 |
| Obsidian | 深灰绿 |

主题会记住，下次打开还是上次那套。

### 字体

视图 → 选择字体，弹出系统字体对话框，只列出等宽字体
（Consolas / Courier New / JetBrains Mono / Fixedsys ...），
可选字号和粗斜体。设置会保存。

### 编译选项（工具 → 编译器选项）

- 语言标准：C++98 / 03 / 11 / 14 / 17 / 20
- 优化模式：CO2 自动 / O0 / O1 / O2 / O3 / Os
- 警告：-Wall / -Wextra / -Werror / -g
- 宏定义：每行一个 NAME=VALUE，同时注入语义分析和生成的 C 代码

### 冶炼金属（工具 → 冶炼金属）

编译强度 = **纯度 × 金属活性 × 反应系数**

纯度决定基础强度：50% 是刚出矿的粗矿，100% 是电解精炼的纯金属。
界面和输出里只显示 `-Fe`、`-Na+H2O` 这样的金属参数，不显示内部强度数值。

| 参数 | 反应 | 活性 | 效果 |
|------|------|------|------|
| `-Fe` | Fe → Fe²⁺/Fe³⁺ | 0.85 | 稳健，默认 |
| `-Na` | Na → Na⁺ | 1.00 | 激进 |
| `-Mn` | Mn → Mn²⁺~Mn⁷⁺ | 0.90 | 变价 |
| `-Cu` | Cu → Cu²⁺ | 0.80 | 温和 |

配体反应（会乘上额外的系数）：

| 参数 | 化学方程式 | 系数 | 结果 |
|------|-----------|------|------|
| `-Na+H2O` | 2Na + 2H₂O → 2NaOH + H₂↑ | ×1.15 | 最剧烈，编译拉满 |
| `-Na+Cl2` | 2Na + Cl₂ → 2NaCl | ×0.45 | 生成稳定晶格，降到最保守 |
| `-Fe+O2` | 4Fe + 3O₂ → 2Fe₂O₃ | ×0.70 | 生锈，优化被锈蚀 |
| `-Fe+CuSO4` | Fe + CuSO₄ → FeSO₄ + Cu | ×1.10 | 置换提纯，略微提升 |
| `-Mn+O2` | Mn + O₂ → MnO₂ | ×1.20 | 锰升到 +4 价 |
| `-Mn+HCl` | MnO₂ + 4HCl → MnCl₂ + Cl₂↑ + 2H₂O | ×1.35 | 制氯气，氧化最强 |

纯度用 `--purity=N` 指定，范围 50~100，默认 99.5。

**不反应的组合会给出解释**，比如：

```
$ co2cxx -Fe+H2O hello.cpp
不反应  铁在常温纯水里几乎不反应。生锈需要氧气和水同时存在：
        4Fe + 3O2 -> 2Fe2O3，请用 -Fe+O2。

$ co2cxx -Cu+HCl hello.cpp
不反应  铜在金属活动性顺序里排在氢之后，不能置换出稀盐酸里的氢。

$ co2cxx -CO2 hello.cpp
不反应  这是金属编译器，不收气体。CO2 是二氧化碳（碳 +4 价），
        钴是 Co，两者不是一回事。想用金属请试 -Fe、-Mn、-Na。
```

### 输入与耗时

语言内置 `input()`，从标准输入读一个整数：

```cpp
int n = input();
```

Ctrl+F10 运行后会弹出控制台，输入数字回车，程序算完打印结果。
控制台最后一行是真实耗时：

```
请输入 n:1000
1000
500500
--------------------------------
Process exited after 0.023 seconds with return value 0
Press any key to continue . . .
```

这个耗时是从进程启动到结束的墙钟时间，包含你敲输入的时间。
想只测计算耗时，把输入换成常量再跑一次对比。

### 钴为什么不能用

钴（Co²⁺）理论上氧化能力最强，但空气里只占 0.04%，早被抢光了。
选 `-Co` 会：

- 编译强度归零
- 编译器额外插入 300 ms 排队等待，模拟抢不到资源

同一个程序实测：

| 金属 | 耗时 |
|------|------|
| `-Na+H2O` | 0.02 s |
| `-Fe` | 0.02 s |
| `-Co` | 0.40 s |

钴模式下规模大一点就是 TLE —— 这就是为什么只能用其他金属。

### 快捷键

| 键 | 功能 |
|----|------|
| F9 | 编译运行 |
| Ctrl+B | 生成原生 exe |
| Ctrl+F | 查找 |
| F3 | 查找下一个 |
| Ctrl+S | 保存 |
| Ctrl+O | 打开 |
| Ctrl+, | 编译器选项 |
| Ctrl+Shift+A | 格式化 |

## 编译器内核

完全用 C++ 重写，和 Python 版行为一致：

```
lexer.cpp      字符流 → Token（手写状态机，支持 0x/0b/数字分隔符）
parser.cpp     递归下降 + 优先级爬升 → AST
sema.cpp       作用域、类型检查、诊断（-Wall/-Wextra/-Werror）
optimizer.cpp  常量折叠、代数化简、死代码消除、常量传播、内联
codegen.cpp    AST → C 代码
runtime.cpp    大气模型、化学引擎、AST 解释器
compiler.cpp   流程编排（解释执行 / 调用 gcc 生成 exe）
```

两条执行路径：

- **内置解释器**（默认）：零依赖，点「运行」立刻出结果
- **原生编译**：生成 C 代码后调用 gcc，产出真正的 .exe

## 构建

```bash
cd native
g++ -std=c++17 -O2 -fexec-charset=GBK -o build/co2cxx.exe \
    src/lexer.cpp src/parser.cpp src/sema.cpp src/optimizer.cpp \
    src/codegen.cpp src/runtime.cpp src/compiler.cpp src/cli_main.cpp \
    src/ide.cpp src/main.cpp build/app.res \
    -lgdi32 -lcomctl32 -lcomdlg32 -static-libgcc -static-libstdc++
```

需要 MinGW-w64 g++（Dev-C++ 自带的 TDM-GCC 也能用）。

图标资源：

```bash
windres -i res/app.rc -o build/app.res -O coff
```

## 命令行模式

同一个 exe 也是命令行编译器：

```
co2cxx.exe                    打开 IDE
co2cxx.exe -Fe hello.cpp            编译（铁，默认）
co2cxx.exe -Na+H2O --run hello.cpp  钠遇水，优化拉满
co2cxx.exe -Fe+O2 hello.cpp         铁生锈，优化衰减
co2cxx.exe -Mn+HCl --purity=95 x.cpp  锰制氯气 + 95% 纯度
co2cxx.exe --emit-c hello.cpp    只打印生成的 C 代码
co2cxx.exe --help             全部选项
```

## 已知限制

- 编辑器基于 RichEdit，不是自绘，超大文件（万行以上）着色会变慢
- 没有真正的调试器（断点只是标记，不触发中断）
- 项目管理只有单文件（多文件编译在 Python 版里有，这一版待补）
- 图标是自绘的简化几何图形，不是专业图标

## 文件结构

```
native/
├── src/
│   ├── core.h          Token、AST、Options 定义
│   ├── lexer.cpp       词法分析
│   ├── parser.cpp      语法分析
│   ├── sema.cpp        语义分析与诊断
│   ├── optimizer.cpp   优化流水线
│   ├── codegen.cpp     C 代码生成
│   ├── runtime.cpp     大气、化学、解释器
│   ├── compiler.h      构建接口
│   ├── compiler.cpp    流程编排
│   ├── ide.cpp         Win32 IDE（约 1800 行）
│   ├── cli_main.cpp    命令行
│   └── main.cpp        入口分发
├── res/app.rc          图标与版本信息
├── scripts/            图标生成、窗口截图工具
└── build/co2cxx.exe    产物
```
