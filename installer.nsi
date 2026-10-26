; co2cxx 原生版安装包（NSIS 3）
; 构建：makensis /DVERSION=1.0.0 installer.nsi
; 安装到 %LOCALAPPDATA%\Programs\co2cxx，无需管理员权限。
; 路径刻意不带空格 —— gcc 内部拼接路径时不处理空格。

Unicode true

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

!ifndef VERSION
  !define VERSION "1.0.0"
!endif

!define APPNAME "co2cxx"
!define PUBLISHER "co2cxx"
!define DESCRIPTION "由大气二氧化碳浓度驱动的 C++ 子集编译器"
!define APPEXE "co2cxx.exe"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\co2cxx"

Name "${APPNAME} ${VERSION}"
OutFile "dist\co2cxx-Setup-${VERSION}-x64.exe"
InstallDir "$LOCALAPPDATA\Programs\co2cxx"
InstallDirRegKey HKCU "Software\co2cxx" "InstallDir"
RequestExecutionLevel user
BrandingText "${APPNAME} ${VERSION}"

SetCompressor /SOLID lzma
SetCompressorDictSize 64

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=2052 "ProductName" "${APPNAME}"
VIAddVersionKey /LANG=2052 "FileDescription" "${DESCRIPTION}"
VIAddVersionKey /LANG=2052 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=2052 "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=2052 "CompanyName" "${PUBLISHER}"

!define MUI_ABORTWARNING
!define MUI_ICON "res\co2cxx.ico"
!define MUI_UNICON "res\co2cxx.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APPEXE}"
!define MUI_FINISHPAGE_RUN_TEXT "立即启动 co2cxx IDE"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.md"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "查看使用说明"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

; ---------------------------------------------------------------- 安装

Section "co2cxx IDE（必需）" SEC_MAIN
  SectionIn RO
  SetOutPath "$INSTDIR"
  SetOverwrite on

  File "build\co2cxx.exe"
  File "README.md"

  WriteUninstaller "$INSTDIR\uninstall.exe"

  WriteRegStr HKCU "Software\co2cxx" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\co2cxx" "Version" "${VERSION}"

  WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\${APPEXE},0"
  WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
SectionEnd

Section "64 位编译器 GCC 16.2.0（推荐）" SEC_GCC64
  SetOutPath "$INSTDIR\mingw64"
  File /r "build\mingw64\*.*"
SectionEnd

Section "32 位编译器 GCC 10.2.0" SEC_GCC32
  SetOutPath "$INSTDIR\mingw32"
  File /r "build\mingw32\*.*"
SectionEnd

Section "开始菜单快捷方式" SEC_SM
  CreateDirectory "$SMPROGRAMS\co2cxx"
  CreateShortcut "$SMPROGRAMS\co2cxx\co2cxx IDE.lnk" "$INSTDIR\${APPEXE}" "" "$INSTDIR\${APPEXE}" 0
  CreateShortcut "$SMPROGRAMS\co2cxx\卸载 co2cxx.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section "桌面快捷方式" SEC_DESKTOP
  CreateShortcut "$DESKTOP\co2cxx.lnk" "$INSTDIR\${APPEXE}" "" "$INSTDIR\${APPEXE}" 0
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_MAIN} "IDE 主程序，约 3 MB。"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_GCC64} "生成 64 位可执行文件所需的编译器，约 680 MB。"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_GCC32} "生成 32 位可执行文件所需的编译器，约 400 MB。"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_SM} "在开始菜单中创建启动和卸载快捷方式。"
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} "在桌面上创建 co2cxx 快捷方式。"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ---------------------------------------------------------------- 卸载

Section "Uninstall"
  Delete "$INSTDIR\${APPEXE}"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\uninstall.exe"
  RMDir /r "$INSTDIR\mingw64"
  RMDir /r "$INSTDIR\mingw32"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\co2cxx\co2cxx IDE.lnk"
  Delete "$SMPROGRAMS\co2cxx\卸载 co2cxx.lnk"
  RMDir "$SMPROGRAMS\co2cxx"
  Delete "$DESKTOP\co2cxx.lnk"

  DeleteRegKey HKCU "${UNINST_KEY}"
  DeleteRegKey HKCU "Software\co2cxx"
SectionEnd
