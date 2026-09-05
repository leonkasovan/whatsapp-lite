@echo off
REM make_msvc.bat — MSVC build without GNU make (VS 2026 Developer Prompt)
REM Requires WebView2Loader.lib in third_party\WebView2\lib\ (generated via lib.exe)

if not exist build\msvc mkdir build\msvc
rc /nologo /fo build\msvc\app.res res\app.rc
if errorlevel 1 exit /b %errorlevel%
cl /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /Ithird_party\WebView2\include /Tp main.c build\msvc\app.res /Fo"build\msvc\\" /Fe:build\msvc\WhatsAppDesktop.exe /link /SUBSYSTEM:WINDOWS user32.lib dwmapi.lib ole32.lib oleaut32.lib advapi32.lib shell32.lib shlwapi.lib comctl32.lib version.lib winmm.lib ws2_32.lib gdi32.lib uuid.lib runtimeobject.lib WebView2Loader.lib /LIBPATH:third_party\WebView2\lib
if errorlevel 1 exit /b %errorlevel%
copy /Y third_party\WebView2\bin\WebView2Loader.dll build\msvc\ >nul
echo [msvc] build\msvc\WhatsAppDesktop.exe built
