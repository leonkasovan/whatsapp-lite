# WhatsApp Desktop Lite — WebView2 Wrapper

Lightweight Windows desktop wrapper for **https://web.whatsapp.com** using Microsoft Edge WebView2. No Electron, no Chromium bundling — uses the system WebView2 Runtime.

Two backends in `main.c`, selected by `-DUSE_WEBVIEW`:

- **w64devkit**: static [webview](https://github.com/webview/webview) (`C:\w64devkit\include\webview`, `-DUSE_WEBVIEW -DWEBVIEW_STATIC -lwebview`)
- **MSVC / MSYS2 / generic MinGW**: direct WebView2 COM (`third_party/WebView2`, `-lWebView2Loader` / `WebView2Loader.lib`)

## Features

- **WebView2** loads WhatsApp Web with native Edge engine (direct COM, or via static webview on w64devkit)
- **Single instance** (`CreateMutexW` + `FindWindowW`) — second launch restores existing window
- **Dark title bar** (`DwmSetWindowAttribute` — `DWMWA_USE_IMMERSIVE_DARK_MODE` 19 + 20, `DWMWA_CAPTION_COLOR` `#111B21`, `DWMWA_TEXT_COLOR` white)
- **Centered window** `1100×750`, `WS_OVERLAPPEDWINDOW`, positioned via `GetSystemMetrics`
- **User-Agent spoof** + `Notification` API polyfill injected at document creation — w64devkit: JS `notify(title, body)` via `webview_bind`; WebView2 backend: `window.Notification` posts JSON via `chrome.webview.postMessage` → native `WebMessageHandler`
- **Toast**: real WinRT `Windows.UI.Notifications` implementation on MSVC (`#ifdef _MSC_VER`, `main.c:87-117`); no-op stub on MinGW/webview (`main.c:119`)
- **App icon + manifest** via `res/app.rc` (`1 ICON "res/icon.ico"`, `1 24 "res/app.manifest"`), compiled to `build/<toolchain>/app.res` (`windres` on MinGW, `rc` on MSVC) and applied with `WM_SETICON` / `RegisterClassExW`
- **Portable user data** at `%APPDATA%\WhatsAppDesktopLight\UserData` (created in `InitWebView2` via `SHGetFolderPathW` + `PathAppendW`; WebView2 backend only)

## Requirements

- Windows 10 1809+ (WebView2 Runtime requirement)
- **WebView2 Runtime** Evergreen (>=152.x). Check: `GetAvailableCoreWebView2BrowserVersionString` should return `152.0.4191.x`. Installed at `C:\Program Files (x86)\Microsoft\EdgeWebView\Application\`. If missing, install from https://developer.microsoft.com/microsoft-edge/webview2/
- One toolchain:

| Toolchain | Compiler | How to get it | Notes |
|-----------|----------|---------------|-------|
| **w64devkit** | `g++ (GCC) 16.2` with `--prefix=/w64devkit` | https://github.com/skeeto/w64devkit/releases — unzip to `C:\w64devkit`, add `C:\w64devkit\bin` to PATH | Self-contained, no installer. Uses **static webview** (`-DUSE_WEBVIEW -DWEBVIEW_STATIC -lwebview`); ignores `third_party/WebView2`. Out: `build/w64devkit/`. Tested with w64devkit 16.2 + GCC 16.2. |
| **MSYS2 MinGW** | `mingw-w64-x86_64-gcc` (GCC 13/14) | https://www.msys2.org — `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make` | Use **MINGW64/UCRT64** shell (`MSYSTEM=MINGW64`). Direct WebView2 via vendored SDK. Out: `build/msys2-$(MSYSTEM)/` (e.g. `build/msys2-MINGW64/`). |
| **MSVC** | `cl` (VS 2022 17.x / VS 2026, `cl` 19.4x) | Visual Studio with *Desktop development with C++*; use **Developer Prompt** | Direct WebView2. `WebView2Loader.lib` already vendored (`third_party/WebView2/lib/`, generated via `lib.exe` from `bin/WebView2Loader.def`). No NuGet needed unless updating SDK. Out: `build/msvc/`. Build with `make CC=cl` **or** `make_msvc.bat` (no GNU make required). |

WebView2 **SDK** (headers + import libs + loader DLL + `.def`) is vendored at `third_party/WebView2/{include,lib,bin}` — no download needed. w64devkit build does not use it.

## Directory Layout

```
.
├── main.c                         # Win32 + dual backend (USE_WEBVIEW ? webview : WebView2 COM)
├── Makefile                       # Toolchain-aware (auto-detect) -> build/<toolchain>/
├── make_msvc.bat                  # MSVC build without GNU make (VS Developer Prompt) -> build/msvc/
├── res/
│   ├── app.rc                     # 1 ICON icon.ico, 1 24 app.manifest (paths relative to root)
│   ├── icon.ico
│   └── app.manifest
├── third_party/WebView2/
│   ├── include/WebView2.h         # 2.8 MB, SDK 152.0.4191.47
│   ├── include/WebView2EnvironmentOptions.h
│   ├── lib/libWebView2Loader.a    # MinGW import lib (for -lWebView2Loader)
│   ├── lib/WebView2Loader.lib     # MSVC import lib (lib.exe-generated from bin/WebView2Loader.def)
│   ├── lib/WebView2Loader.exp
│   └── bin/{WebView2Loader.dll,WebView2Loader.def}  # 159 KB DLL, copied beside exe at build
└── build/<toolchain>/             # per-toolchain output, e.g.:
    ├── msvc/WhatsAppDesktop.exe + WebView2Loader.dll + app.res + main.obj
    ├── w64devkit/WhatsAppDesktop.exe + app.res   # no DLL copy (static webview)
    └── msys2-MINGW64/WhatsAppDesktop.exe + WebView2Loader.dll + app.res
```

## Toolchain Auto-Detection

`Makefile` inspects **compiler info**, not just file existence:

```make
COMPILER_INFO := $(shell $(CC) --version 2>&1; echo ---; $(CC) -v 2>&1; echo ---; $(CC) 2>&1; echo MSYSTEM=$(MSYSTEM); echo CC=$(CC))
# Fast path: CC name contains "cl" -> msvc (covers `make CC=cl` even if cl not in PATH)
# else if "Microsoft"/"microsoft"/"CL.EXE" in $(COMPILER_INFO) -> msvc
# else if "w64devkit" in $(COMPILER_INFO)                  -> w64devkit  (--prefix=/w64devkit)
# else if MSYSTEM env set                                   -> msys2-$(MSYSTEM)  (MINGW64/UCRT64)
# else if "GCC" + "mingw"/"MINGW" in info                   -> mingw
# else if "GCC" only                                        -> gcc
# else                                                       -> unknown
$(info [toolchain] CC=$(CC) -> $(TOOLCHAIN))
$(info [compiler] $(shell $(CC) --version 2>&1 || $(CC) 2>&1))
```

- **w64devkit**: `g++ -v` contains `Configured with: ... --prefix=/w64devkit` and `Target: x86_64-w64-mingw32`
- **MSYS2**: `MSYSTEM=MINGW64` (or `UCRT64`/`CLANG64`) env var is set by MSYS2 shells; `g++ -v` contains `--prefix=/mingw64`
- **MSVC**: `$(CC)` contains `cl`, or `cl` output contains `Microsoft (R) C/C++ Optimizing Compiler` / `CL.EXE`

Override manually:

```sh
make info                          # print detected toolchain + BUILDDIR/TARGET/flags
make CC=g++ TOOLCHAIN=w64devkit    # force
make CC=cl TOOLCHAIN=msvc          # force MSVC path
```

## Build

All `Makefile` builds output to `build/<toolchain>/` (root stays clean) and compile `res/app.rc` to `build/<toolchain>/app.res` first.

### w64devkit (tested, static webview)

```powershell
# PowerShell — add w64devkit to PATH for this session
$env:PATH = "C:\w64devkit\bin;$env:PATH"
g++ --version   # g++ (GCC) 16.2.0, Target: x86_64-w64-mingw32, w64devkit

make            # [toolchain] CC=g++ -> w64devkit
# g++ -mwindows -O2 -s -std=c++17 -DUNICODE -D_UNICODE -x c++ -DUSE_WEBVIEW -DWEBVIEW_STATIC
#   -Ithird_party/WebView2/include -o build/w64devkit/WhatsAppDesktop.exe main.c -x none
#   build/w64devkit/app.res -mwindows -static-libgcc -static-libstdc++ ... -lwebview
# windres res/app.rc -O coff -o build/w64devkit/app.res  (resource step)

build\w64devkit\WhatsAppDesktop.exe
```

`Makefile` w64devkit block uses:

```make
CFLAGS  = -mwindows -O2 -s -std=c++17 -DUNICODE -D_UNICODE -x c++ -DUSE_WEBVIEW -DWEBVIEW_STATIC $(WEBVIEW2_INC)
LDFLAGS = -mwindows -static-libgcc -static-libstdc++ -luser32 -ldwmapi -lole32 -loleaut32 -ladvapi32 \
          -lshell32 -lshlwapi -lcomctl32 -lversion -lwinmm -lws2_32 -lgdi32 -luuid -lruntimeobject -lwebview
```

- No `WebView2Loader.dll` copy (static link). No `-lWebView2Loader`.
- `-x c++` forces `.c` to compile as C++ (required for `webview/webview.h` and `<wrl/client.h>`); `-x none` before `app.res` stops the `-x` applying to the resource object.

### MSYS2 MinGW / generic MinGW (direct WebView2)

```bash
# Inside MSYS2 MINGW64 shell
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make
which g++   # /mingw64/bin/g++
g++ --version
echo $MSYSTEM   # MINGW64

make        # auto-detects msys2-MINGW64 -> build/msys2-MINGW64/
./build/msys2-MINGW64/WhatsAppDesktop.exe
```

`Makefile` for these variants uses:

```make
CFLAGS  = -mwindows -O2 -s -std=c++17 -DUNICODE -D_UNICODE -x c++
LDFLAGS = -mwindows -static-libgcc -static-libstdc++ -luser32 -ldwmapi -lruntimeobject -lole32 -loleaut32 -ladvapi32 \
          -lshell32 -lshlwapi -lcomctl32 -lversion -lwinmm -lws2_32 -lgdi32 -luuid -lWebView2Loader
WEBVIEW2_INC = -Ithird_party/WebView2/include
WEBVIEW2_LIB = -Lthird_party/WebView2/lib  # before -lWebView2Loader
```

- `-x c++` forces `.c` to compile as C++ (required for `<wrl/client.h>` which needs `<cstddef>` and `Microsoft::WRL::ComPtr`)
- `-luuid` for `IID_IUnknown`, `-lruntimeobject` for `RoInitialize` (WinRT toast path)
- `-lcombase` removed — no `libcombase.a` in w64devkit/MSYS2
- `-L` before `-lWebView2Loader` (link order matters); DLL copied to `$(BUILDDIR)/` after link

### MSVC (cl) — `make` or `make_msvc.bat`

```powershell
# x64 Native Tools / VS 2026 Developer Prompt
where cl   # ...\VC\Tools\MSVC\...\bin\Hostx64\x64\cl.exe
cl         # Microsoft (R) C/C++ Optimizing Compiler Version 19.4x

# Option A: GNU make (auto-detects msvc via CC=cl)
make CC=cl
# -> build/msvc/WhatsAppDesktop.exe + WebView2Loader.dll + app.res

# Option B: no make installed
.\make_msvc.bat
# rc /nologo /fo build\msvc\app.res res\app.rc
# cl /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /Ithird_party\WebView2\include
#    /Tp main.c build\msvc\app.res /Fo"build\msvc\\" /Fe:build\msvc\WhatsAppDesktop.exe
#    /link /SUBSYSTEM:WINDOWS user32.lib ... uuid.lib runtimeobject.lib WebView2Loader.lib
#    /LIBPATH:third_party\WebView2\lib
# + copy third_party\WebView2\bin\WebView2Loader.dll build\msvc\
```

Flags used (`Makefile` `msvc` block ≈ `make_msvc.bat` line 8):

```
CFLAGS = /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /Ithird_party/WebView2/include
LDFLAGS = /link /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup user32.lib ... uuid.lib runtimeobject.lib WebView2Loader.lib /LIBPATH:third_party/WebView2/lib
```

- `/Tp main.c` forces C++ compile of `.c` (same reason as `-x c++`).
- `WebView2Loader.lib` resolution: `third_party/WebView2/lib/WebView2Loader.lib`, else `WebView2LoaderStatic.lib` fallback, else warns:

```
[msvc] WebView2Loader.lib not found in third_party/WebView2/lib -- install WebView2 SDK
```

- To update the SDK `.lib`: regenerate via `lib.exe /def:third_party\WebView2\bin\WebView2Loader.def /out:third_party\WebView2\lib\WebView2Loader.lib`, or copy `WebView2Loader.dll.lib` from NuGet `Microsoft.Web.WebView2` (`build\native\x64\`) over it.

## Run

```sh
# MSVC / MSYS2 (WebView2Loader.dll must be beside exe — both make paths copy it):
./build/msvc/WhatsAppDesktop.exe
./build/msys2-MINGW64/WhatsAppDesktop.exe
# w64devkit (self-contained, no DLL):
./build/w64devkit/WhatsAppDesktop.exe
# or double-click in Explorer.
```

- First run (WebView2 backend) creates `%APPDATA%\WhatsAppDesktopLight\UserData` (Edge profile)
- If runtime missing, `ShowHrError` shows `HRESULT=0x800...` — install Runtime via Evergreen Bootstrapper
- Single-instance: second launch restores window via `FindWindowW(NULL, L"WhatsApp Desktop")`

## WebView2 SDK Note

Vendored SDK `152.0.4191.47` (`third_party/WebView2`) contains:

- `include/WebView2.h` (C/C++ dual: `__cplusplus && !CINTERFACE` → `MIDL_INTERFACE` class, else C `Vtbl` struct)
- `lib/libWebView2Loader.a` — MinGW import lib for `CreateCoreWebView2EnvironmentWithOptions`, `GetAvailableCoreWebView2BrowserVersionString`
- `lib/WebView2Loader.lib` + `lib/WebView2Loader.exp` — MSVC import lib (generated with `lib.exe` from `bin/WebView2Loader.def`)
- `bin/WebView2Loader.dll` + `bin/WebView2Loader.def` — loader that finds Evergreen Runtime via registry `HKLM\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-...}`

To update SDK, replace `third_party/WebView2` from https://www.nuget.org/packages/Microsoft.Web.WebView2, keep `Makefile` `WEBVIEW2_INC/LIB` pointing there, and regenerate the MSVC `.lib` via `lib.exe` as above. w64devkit build ignores this directory.

## Implementation Notes

- **Dual backend** (`main.c`): `#ifdef USE_WEBVIEW` (w64devkit, `main.c:125-186`) uses `webview_create/run`, `webview_bind("notify")` → `notify_cb` parses `["title","body"]` JSON, `MultiByteToWideChar(CP_UTF8)` → `ShowToastNotification`. `#else` (`main.c:188-355`) is the direct WebView2 COM backend for MSVC/MSYS2/MinGW.
- **COM handlers** (`main.c:214-255`): `EnvCompletedHandler`, `ControllerCompletedHandler`, `WebMessageHandler` inherit from `ICoreWebView2*CompletedHandler` (C++ `MIDL_INTERFACE`). Each implements `QueryInterface` (checks `IID_IUnknown`/`IID_ICoreWebView2*`), `AddRef`/`Release` via `InterlockedIncrement`, `Invoke`.
- **g_hwnd fix**: `InitWebView2(HWND hwnd)` does `if(hwnd) g_hwnd=hwnd` + `CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)` before `CreateCoreWebView2EnvironmentWithOptions`. Previously `g_hwnd` was set in `WinMain` *after* `CreateWindowEx` returns, but `WM_CREATE` fires *during* `CreateWindowEx`, so `EnvCompletedHandler::Invoke` saw `g_hwnd=0` → `CreateCoreWebView2Controller(NULL)` → `E_INVALIDARG 0x80070057`.
- **WinRT toast** (`main.c:86-120`): real implementation under `#ifdef _MSC_VER` (`RoInitialize`, `ToastNotificationManager`, `ToastText02` template). MinGW/webview builds compile the no-op stub `void ShowToastNotification(...){}` — MinGW lacks full `wrl/async.h` / `wrl/implements.h` and `windows.foundation.h` has the `IReference<BYTE>`/`IReference<boolean>` clash. To re-enable on MinGW, build with MSVC or replace with `Shell_NotifyIconW(NIM_ADD, NIF_INFO)` / `WinToast`.
- **JS injection**: WebView2 backend `InjectScripts` (`main.c:299`) → `AddScriptToExecuteOnDocumentCreated` spoofs `navigator.userAgent`/`appVersion` to Chrome 133 and replaces `window.Notification` with `chrome.webview.postMessage(JSON)`. webview backend (`main.c:161-169`) injects the same spoof via `webview_init` but bridges via bound `notify(t,b)` instead.
- **Icon**: `res/app.rc` embeds icon + manifest; both backends also set the live window icon from resource id 1 (`LoadIconW`/`LoadImageW` + `WM_SETICON`).
- **Link order** (MSYS2/MinGW only): `$(WEBVIEW2_LIB) $(LDFLAGS)` — `-Lthird_party/WebView2/lib` must precede `-lWebView2Loader`, otherwise `ld` cannot find `libWebView2Loader.a` before `-l`.

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `fatal error: cstddef: No such file` | `CC=gcc -std=c11` on `.c` that includes `<wrl/client.h>` / `<webview/webview.h>` | Use `g++ -std=c++17 -x c++` (Makefile does) / `cl /Tp` |
| `wrl/async.h: No such file` | MinGW WRL is minimal | Expected — toast is stubbed on MinGW; build MSVC for real toast |
| `IReference<boolean>` redefinition | `windows.ui.notifications.h` + MinGW `windows.foundation.h` bug | Avoid WinRT toast in MinGW; use MSVC or `Shell_NotifyIcon` |
| `cannot find -lcombase` | No `libcombase.a` in MinGW | Removed from `LDFLAGS` |
| `cannot find -lwebview` (w64devkit) | Static webview not installed | Webview headers/libs must exist under `C:\w64devkit\{include,lib}\webview` |
| `undefined reference to IID_IUnknown` | Missing `-luuid` | Added to `LDFLAGS` |
| `E_INVALIDARG 0x80070057` on `CreateCoreWebView2Controller` | `g_hwnd` NULL (timing bug) | `InitWebView2` captures `hwnd` param |
| `Failed to create WebView2 environment hr=0x80040154` | Runtime not installed / `WebView2Loader.dll` not beside exe | Install Evergreen Runtime; `make` / `make_msvc.bat` copies DLL to `build/<toolchain>/` |
| `[msvc] WebView2Loader.lib not found ...` | `.lib` missing from `third_party/WebView2/lib` | Regenerate via `lib.exe /def:...WebView2Loader.def` or copy from NuGet package |
| `Chrome_WidgetWin_0 Failed to unregister class 1411` | WebView2 internal, harmless | Ignore |

## Clean

```sh
make clean       # deletes build/<toolchain>/WhatsAppDesktop.exe (+ .obj/.res)
make info        # show detected toolchain, BUILDDIR/TARGET, flags
make help
```

## License

MIT — WebView2 SDK and Runtime are Microsoft-licensed.
