# WhatsApp Desktop Lite — WebView2 Wrapper

Lightweight Windows desktop wrapper for **https://web.whatsapp.com** using Microsoft Edge WebView2. No Electron, no Chromium bundling — uses the system WebView2 Runtime.

## Features

- **WebView2** (`third_party/WebView2`) loads WhatsApp Web with native Edge engine
- **Single instance** (`CreateMutexW` + `FindWindowW`) — second launch restores existing window
- **Dark title bar** (`DwmSetWindowAttribute` — `DWMWA_USE_IMMERSIVE_DARK_MODE`, `DWMWA_CAPTION_COLOR` `#111B21`, `DWMWA_TEXT_COLOR` white)
- **Centered window** `1100×750`, `WS_OVERLAPPEDWINDOW`, DPI-aware via `GetSystemMetrics`
- **User-Agent spoof** + `Notification` API polyfill injected via `AddScriptToExecuteOnDocumentCreated` — JS `window.Notification` posts JSON to native via `chrome.webview.postMessage`
- **Toast stub** — native `ShowToastNotification` is a no-op in MinGW builds (WinRT `Windows.UI.Notifications` requires MSVC; see below)
- **Portable user data** at `%APPDATA%\WhatsAppDesktopLight\UserData`

## Requirements

- Windows 10 1809+ (WebView2 Runtime requirement)
- **WebView2 Runtime** Evergreen (>=152.x). Check: `GetAvailableCoreWebView2BrowserVersionString` should return `152.0.4191.x`. Installed at `C:\Program Files (x86)\Microsoft\EdgeWebView\Application\`. If missing, install from https://developer.microsoft.com/microsoft-edge/webview2/
- One toolchain:

| Toolchain | Compiler | How to get it | Notes |
|-----------|----------|---------------|-------|
| **w64devkit** | `g++ (GCC) 16.2` with `--prefix=/w64devkit` | https://github.com/skeeto/w64devkit/releases — unzip to `C:\w64devkit`, add `C:\w64devkit\bin` to PATH | Self-contained, no installer. `make` is GNU make (`C:\w64devkit\bin\make.exe`). This repo was tested with w64devkit 16.2 + GCC 16.2. |
| **MSYS2 MinGW** | `mingw-w64-x86_64-gcc` (GCC 13/14) | https://www.msys2.org — `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make` | Use **MINGW64/UCRT64** shell (`MSYSTEM=MINGW64`). `mingw32-make` or `make`. |
| **MSVC** | `cl` (VS 2022 17.x, `cl` 19.4x) | Visual Studio 2022 with *Desktop development with C++* | Requires `WebView2Loader.lib`. Install WebView2 SDK via NuGet `Microsoft.Web.WebView2` or copy `WebView2Loader.dll.lib` to `third_party/WebView2/lib/`. |

WebView2 **SDK** (headers + import lib + loader DLL) is vendored at `third_party/WebView2/{include,lib,bin}` — no download needed for MinGW. For MSVC, see SDK note below.

## Directory Layout

```
.
├── main.c                         # Win32 + WebView2 (C++17, -x c++ for .c)
├── Makefile                       # Toolchain-aware (auto-detect)
├── third_party/WebView2/
│   ├── include/WebView2.h         # 2.8 MB, SDK 152.0.4191.47
│   ├── include/WebView2EnvironmentOptions.h
│   ├── lib/libWebView2Loader.a    # MinGW import lib (6 KB, for -lWebView2Loader)
│   └── bin/WebView2Loader.dll     # 159 KB, copied beside exe at build
├── WhatsAppDesktop.exe            # built (151 KB stripped, -mwindows)
└── WebView2Loader.dll             # copied beside exe (required at runtime)
```

## Toolchain Auto-Detection

`Makefile` inspects **compiler info**, not just file existence (as requested):

```make
COMPILER_INFO := $(shell $(CC) --version 2>&1; $(CC) -v 2>&1; $(CC) 2>&1 | head -5; echo MSYSTEM=$(MSYSTEM))
# Fast path: CC name contains "cl" -> msvc
# else if "w64devkit" in $(COMPILER_INFO)          -> w64devkit  (--prefix=/w64devkit)
# else if MSYSTEM env set                           -> msys2-$(MSYSTEM)  (MINGW64/UCRT64)
# else if "GCC" + "mingw"/"MINGW" in info           -> mingw
# else                                               -> unknown/gcc
$(info [toolchain] CC=$(CC) -> $(TOOLCHAIN))
$(info [compiler] $(shell $(CC) --version | head -1))
```

- **w64devkit**: `g++ -v` contains `Configured with: ... --prefix=/w64devkit` and `Target: x86_64-w64-mingw32`
- **MSYS2**: `MSYSTEM=MINGW64` (or `UCRT64`/`CLANG64`) env var is set by MSYS2 shells; `g++ -v` contains `--prefix=/mingw64`
- **MSVC**: `cl` prints `Microsoft (R) C/C++ Optimizing Compiler Version` or `CL.EXE`; also `$(CC)` contains `cl`

Override manually:

```sh
make info                          # print detected toolchain
make CC=g++ TOOLCHAIN=w64devkit    # force
make CC=cl TOOLCHAIN=msvc          # force MSVC path
```

## Build

### w64devkit (tested)

```powershell
# PowerShell — add w64devkit to PATH for this session
$env:PATH = "C:\w64devkit\bin;$env:PATH"
g++ --version   # g++ (GCC) 16.2.0, Target: x86_64-w64-mingw32, w64devkit

make            # or: mingw32-make, or: make -f Makefile
# [toolchain] CC=g++ -> w64devkit
# [compiler] g++ (GCC) 16.2.0
# g++ -mwindows -O2 -s -std=c++17 -DUNICODE -D_UNICODE -x c++ -Ithird_party/WebView2/include -o WhatsAppDesktop.exe main.c -Lthird_party/WebView2/lib -mwindows ... -lWebView2Loader
# + cp third_party/WebView2/bin/WebView2Loader.dll .

.\WhatsAppDesktop.exe
```

### MSYS2 MinGW

```bash
# Inside MSYS2 MINGW64 shell
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make
which g++   # /mingw64/bin/g++
g++ --version
echo $MSYSTEM   # MINGW64

make        # auto-detects msys2-MINGW64, same flags as w64devkit
./WhatsAppDesktop.exe
# or: mingw32-make
```

`Makefile` for MinGW variants uses:

```make
CFLAGS  = -mwindows -O2 -s -std=c++17 -DUNICODE -D_UNICODE -x c++
LDFLAGS = -mwindows -static-libgcc -static-libstdc++ \
          -luser32 -ldwmapi -lruntimeobject -lole32 -loleaut32 -ladvapi32 \
          -lshell32 -lshlwapi -lcomctl32 -lversion -lwinmm -lws2_32 -lgdi32 -luuid -lWebView2Loader
WEBVIEW2_INC = -Ithird_party/WebView2/include
WEBVIEW2_LIB = -Lthird_party/WebView2/lib  # before -lWebView2Loader
```

- `-x c++` forces `.c` to compile as C++ (required for `<wrl/client.h>` which needs `<cstddef>` and `Microsoft::WRL::ComPtr`)
- `-luuid` for `IID_IUnknown`, `-lruntimeobject` for `RoInitialize` (if WinRT toast re-enabled)
- `-lcombase` removed — no `libcombase.a` in w64devkit/MSYS2
- `-L` before `-lWebView2Loader` (link order matters)

### MSVC (cl)

```powershell
# x64 Native Tools Command Prompt for VS 2022
where cl   # C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\...\bin\Hostx64\x64\cl.exe
cl         # Microsoft (R) C/C++ Optimizing Compiler Version 19.4x

# Provide WebView2Loader.lib if not present:
# Option A: NuGet
nuget install Microsoft.Web.WebView2 -Version 1.0.3179.45 -OutputDirectory packages
copy packages\Microsoft.Web.WebView2.*\build\native\x64\WebView2Loader.dll.lib third_party\WebView2\lib\WebView2Loader.lib

# Option B: use existing third_party/WebView2/bin/WebView2Loader.dll with dynamic loading (main.c supports LoadLibrary fallback)

nmake          # if using nmake, or:
make CC=cl     # GNU make with cl — auto-detects msvc

# Flags used:
# CFLAGS = /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /Ithird_party/WebView2/include
# LDFLAGS = /link /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup user32.lib ... uuid.lib WebView2Loader.lib /LIBPATH:third_party/WebView2/lib
```

`Makefile` `msvc` block warns if `WebView2Loader.lib` missing:

```
[msvc] WebView2Loader.lib not found in third_party/WebView2/lib -- install WebView2 SDK
```

## Run

```sh
./WhatsAppDesktop.exe
# or double-click in Explorer. WebView2Loader.dll must be beside exe (Makefile copies it).
```

- First run creates `%APPDATA%\WhatsAppDesktopLight\UserData` (Edge profile)
- If runtime missing, `ShowHrError` shows `HRESULT=0x800...` — install Runtime via Evergreen Bootstrapper
- Single-instance: second launch restores window via `FindWindowW(NULL, L"WhatsApp Desktop")`

## WebView2 SDK Note

Vendored SDK `152.0.4191.47` (`third_party/WebView2`) contains:

- `include/WebView2.h` (C/C++ dual: `__cplusplus && !CINTERFACE` → `MIDL_INTERFACE` class, else C `Vtbl` struct)
- `lib/libWebView2Loader.a` — MinGW import lib for `CreateCoreWebView2EnvironmentWithOptions`, `GetAvailableCoreWebView2BrowserVersionString`
- `bin/WebView2Loader.dll` — loader that finds Evergreen Runtime via registry `HKLM\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-...}` (`pv=152.0.4191.62`)

To update SDK, replace `third_party/WebView2` from https://www.nuget.org/packages/Microsoft.Web.WebView2 and keep `Makefile` `WEBVIEW2_INC/LIB` pointing there.

## Implementation Notes

- **COM handlers** (`main.c:90-160`): `EnvCompletedHandler`, `ControllerCompletedHandler`, `WebMessageHandler` inherit from `ICoreWebView2*CompletedHandler` (C++ `MIDL_INTERFACE`). Each implements `QueryInterface` (checks `IID_IUnknown`/`IID_ICoreWebView2*`), `AddRef`/`Release` via `InterlockedIncrement`, `Invoke`. Original `main.c` used C `Vtbl` structs with `NULL` `QueryInterface` in C++ mode — fails because `Vtbl` only defined under `CINTERFACE` (`WebView2.h:44486`).
- **g_hwnd fix**: `InitWebView2(HWND hwnd)` now does `if(hwnd) g_hwnd=hwnd` + `CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)` before `CreateCoreWebView2EnvironmentWithOptions`. Previously `g_hwnd` was set in `WinMain` *after* `CreateWindowEx` returns, but `WM_CREATE` fires *during* `CreateWindowEx`, so `EnvCompletedHandler::Invoke` saw `g_hwnd=0` → `CreateCoreWebView2Controller(NULL)` → `E_INVALIDARG 0x80070057`.
- **WinRT toast**: original `ShowToastNotification` used `Windows.UI.Notifications` (`IToastNotificationManagerStatics`, `IXmlDocument`, `RoGetActivationFactory`, `ToastTemplateType::ToastText02`). MinGW lacks `wrl/async.h`, `wrl/implements.h`, and `windows.foundation.h` has `IReference<BYTE>`/`IReference<boolean>` duplicate (both `unsigned char`). Stubbed to `void ShowToastNotification(...){}`. To re-enable, build with MSVC or replace with `Shell_NotifyIconW(NIM_ADD, NIF_INFO)` / `WinToast`.
- **JS injection**: `InjectScripts` → `AddScriptToExecuteOnDocumentCreated` spoofs `navigator.userAgent`/`appVersion` to Chrome 133 and replaces `window.Notification` to `chrome.webview.postMessage(JSON)`.
- **Link order**: `$(WEBVIEW2_LIB) $(LDFLAGS)` — `-Lthird_party/WebView2/lib` must precede `-lWebView2Loader`, otherwise `ld` cannot find `libWebView2Loader.a` before `-l`.

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `fatal error: cstddef: No such file` | `CC=gcc -std=c11` on `.c` that includes `<wrl/client.h>` | Use `g++ -std=c++17 -x c++` (Makefile does) |
| `wrl/async.h: No such file` | MinGW WRL is minimal | Remove include (stubbed) |
| `IReference<boolean>` redefinition | `windows.ui.notifications.h` + MinGW `windows.foundation.h` bug | Avoid WinRT toast in MinGW; use MSVC or `Shell_NotifyIcon` |
| `cannot find -lcombase` | No `libcombase.a` in MinGW | Removed from `LDFLAGS` |
| `undefined reference to IID_IUnknown` | Missing `-luuid` | Added to `LDFLAGS` |
| `E_INVALIDARG 0x80070057` on `CreateCoreWebView2Controller` | `g_hwnd` NULL (timing bug) | `InitWebView2` captures `hwnd` param |
| `Failed to create WebView2 environment hr=0x80040154` | Runtime not installed / `WebView2Loader.dll` not beside exe | Install Evergreen Runtime; `make` copies DLL |
| `Chrome_WidgetWin_0 Failed to unregister class 1411` | WebView2 internal, harmless | Ignore |

## Clean

```sh
make clean       # rm WhatsAppDesktop.exe (and WebView2Loader.dll for MinGW)
make info        # show detected toolchain, flags
make help
```

## License

MIT — WebView2 SDK and Runtime are Microsoft-licensed.

