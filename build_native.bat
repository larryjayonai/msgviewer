@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo Building MSG Viewer v1.2-revB (Pure C Win32 Standalone)
echo ========================================================

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Failed to initialize MSVC x86 environment.
    exit /b 1
)

if not exist dist mkdir dist

cl.exe /nologo /O2 /MT /W3 /utf-8 /D_UNICODE /DUNICODE /I src_native ^
    src_native\arena.c ^
    src_native\localization.c ^
    src_native\error_diag.c ^
    src_native\encoding.c ^
    src_native\rtf_decompressor.c ^
    src_native\rtf_to_html.c ^
    src_native\html_sanitizer.c ^
    src_native\cfb_reader.c ^
    src_native\ole_browser.c ^
    src_native\main.c ^
    /link /nologo /SUBSYSTEM:WINDOWS /LARGEADDRESSAWARE /OPT:REF /OPT:ICF ^
    user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib advapi32.lib ^
    /OUT:dist\MsgViewer.exe

if %ERRORLEVEL% equ 0 (
    echo.
    echo [SUCCESS] Binary created at dist\MsgViewer.exe
    dir dist\MsgViewer.exe
) else (
    echo.
    echo [ERROR] Compilation failed.
    exit /b %ERRORLEVEL%
)
