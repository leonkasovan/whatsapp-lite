# Makefile for WhatsApp Web Wrapper (WebView2)
# Supports: MSVC (cl), MSYS2 MinGW, w64devkit — detected via compiler info
# WebView2 SDK expected at third_party/WebView2/{include,lib,bin}

CC = g++

# -- toolchain detection via compiler info (as requested) ---------------
# No Unix head/find — use plain shell so it works in VS Developer Prompt (cmd) and w64devkit sh
COMPILER_INFO := $(shell $(CC) --version 2>&1; echo ---; $(CC) -v 2>&1; echo ---; $(CC) 2>&1; echo MSYSTEM=$(MSYSTEM); echo CC=$(CC))

# Fast path: CC name contains "cl" -> msvc (covers `make CC=cl` even if cl not in PATH)
ifneq (,$(findstring cl,$(CC)))
TOOLCHAIN := msvc
else
ifeq (,$(findstring Microsoft,$(COMPILER_INFO)))
ifeq (,$(findstring microsoft,$(COMPILER_INFO)))
ifeq (,$(findstring CL.EXE,$(COMPILER_INFO)))
ifneq (,$(findstring w64devkit,$(COMPILER_INFO)))
TOOLCHAIN := w64devkit
else
ifneq (,$(MSYSTEM))
TOOLCHAIN := msys2-$(MSYSTEM)
else
ifneq (,$(findstring GCC,$(COMPILER_INFO)))
ifneq (,$(findstring mingw,$(COMPILER_INFO)))
TOOLCHAIN := mingw
else
ifneq (,$(findstring MINGW,$(COMPILER_INFO)))
TOOLCHAIN := mingw
else
TOOLCHAIN := gcc
endif
endif
else
TOOLCHAIN := unknown
endif
endif
endif
else
TOOLCHAIN := msvc
endif
else
TOOLCHAIN := msvc
endif
else
TOOLCHAIN := msvc
endif
endif

$(info [toolchain] CC=$(CC) -> $(TOOLCHAIN))
$(info [compiler] $(shell $(CC) --version 2>&1 || $(CC) 2>&1))

# -- common ------------------------------------------------------------
WEBVIEW2_INC = -Ithird_party/WebView2/include
WEBVIEW2_LIB = -Lthird_party/WebView2/lib
WEBVIEW2_DLL = third_party/WebView2/bin/WebView2Loader.dll

TARGET  = WhatsAppDesktop.exe
SOURCES = main.c

# -- toolchain-specific flags ------------------------------------------
ifeq ($(TOOLCHAIN),msvc)
# ----- MSVC (cl) -------------------------------------------------------
# out: build/msvc/
CC       = cl
BUILDDIR = build/msvc
TARGET  := $(BUILDDIR)/WhatsAppDesktop.exe
OBJ      = $(BUILDDIR)/main.obj
RES      = $(BUILDDIR)/app.res
RC       = rc
CFLAGS   = /nologo /O2 /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /Ithird_party/WebView2/include
WEBVIEW2_LIB_Msvc := $(wildcard third_party/WebView2/lib/WebView2Loader.lib)
ifeq (,$(WEBVIEW2_LIB_Msvc))
WEBVIEW2_LIB_Msvc := $(wildcard third_party/WebView2/lib/WebView2LoaderStatic.lib)
endif
ifeq (,$(WEBVIEW2_LIB_Msvc))
$(warning [msvc] WebView2Loader.lib not found in third_party/WebView2/lib -- install WebView2 SDK)
WEBVIEW2_LIB_FLAG = WebView2Loader.lib
else
WEBVIEW2_LIB_FLAG = $(WEBVIEW2_LIB_Msvc)
endif
LDFLAGS  = /link /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup user32.lib dwmapi.lib ole32.lib oleaut32.lib advapi32.lib shell32.lib shlwapi.lib comctl32.lib version.lib winmm.lib ws2_32.lib gdi32.lib uuid.lib runtimeobject.lib $(WEBVIEW2_LIB_FLAG) /LIBPATH:third_party/WebView2/lib

all: $(TARGET)
$(TARGET): $(SOURCES) $(RES)
	-@if not exist $(subst /,\\,$(BUILDDIR)) mkdir $(subst /,\\,$(BUILDDIR))
	$(CC) $(CFLAGS) /Tp$(SOURCES) /Fo"$(subst /,\\,$(BUILDDIR))\\" /Fe:$@ $(RES) $(LDFLAGS)
	@copy /Y $(subst /,\\,$(WEBVIEW2_DLL)) $(subst /,\\,$(BUILDDIR))\\ >nul 2>&1 || echo [msvc] copy $(WEBVIEW2_DLL) to $(BUILDDIR) manually

$(RES): res/app.rc res/icon.ico res/app.manifest
	-@if not exist $(subst /,\\,$(BUILDDIR)) mkdir $(subst /,\\,$(BUILDDIR))
	$(RC) /nologo /fo$@ res/app.rc

clean:
	-del /f $(subst /,\\,$(TARGET)) $(subst /,\\,$(OBJ)) $(subst /,\\,$(RES)) 2>nul || rm -f $(TARGET) $(OBJ) $(RES)

else
# ----- MinGW / w64devkit / MSYS2 (g++) ---------------------------------
ifeq ($(TOOLCHAIN),w64devkit)
# w64devkit: static webview from C:\w64devkit\{include\lib}\webview
# webview already in default include/lib path; WEBVIEW_STATIC selects extern linkage
# out: build/w64devkit/
BUILDDIR = build/w64devkit
TARGET  := $(BUILDDIR)/WhatsAppDesktop.exe
RES      = $(BUILDDIR)/app.res
WINDRES  = windres
CFLAGS  = -mwindows -O2 -s -std=c++17 -DUNICODE -D_UNICODE -x c++ -DUSE_WEBVIEW -DWEBVIEW_STATIC $(WEBVIEW2_INC)
LDFLAGS = -mwindows -static-libgcc -static-libstdc++ -luser32 -ldwmapi -lole32 -loleaut32 -ladvapi32 -lshell32 -lshlwapi -lcomctl32 -lversion -lwinmm -lws2_32 -lgdi32 -luuid -lruntimeobject -lwebview

all: $(TARGET)
$(TARGET): $(SOURCES) $(RES)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) -x none $(RES) $(LDFLAGS)

$(RES): res/app.rc res/icon.ico res/app.manifest
	@mkdir -p $(BUILDDIR)
	$(WINDRES) res/app.rc -O coff -o $@

clean:
	-rm -f $(TARGET) $(RES) 2>/dev/null; rm -f $(TARGET) $(RES)

else
# MSYS2 / generic MinGW: WebView2 SDK
# out: build/<toolchain>/ (keeps root clean like w64devkit/msvc)
BUILDDIR = build/$(TOOLCHAIN)
TARGET  := $(BUILDDIR)/WhatsAppDesktop.exe
RES      = $(BUILDDIR)/app.res
WINDRES  = windres
CFLAGS  = -mwindows -O2 -s -std=c++17 -DUNICODE -D_UNICODE -x c++
ifneq (,$(findstring msys2,$(TOOLCHAIN)))
LDFLAGS = -mwindows -static-libgcc -static-libstdc++ -luser32 -ldwmapi -lruntimeobject -lole32 -loleaut32 -ladvapi32 -lshell32 -lshlwapi -lcomctl32 -lversion -lwinmm -lws2_32 -lgdi32 -luuid -lWebView2Loader
else
LDFLAGS = -mwindows -static-libgcc -static-libstdc++ -luser32 -ldwmapi -lruntimeobject -lole32 -loleaut32 -ladvapi32 -lshell32 -lshlwapi -lcomctl32 -lversion -lwinmm -lws2_32 -lgdi32 -luuid -lWebView2Loader
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(RES)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $(WEBVIEW2_INC) -o $@ $(SOURCES) -x none $(RES) $(WEBVIEW2_LIB) $(LDFLAGS)
	@cp -f $(WEBVIEW2_DLL) $(BUILDDIR)/ 2>/dev/null || copy /Y $(subst /,\\,$(WEBVIEW2_DLL)) $(subst /,\\,$(BUILDDIR))\\ >nul 2>&1 || true

$(RES): res/app.rc res/icon.ico res/app.manifest
	@mkdir -p $(BUILDDIR)
	$(WINDRES) res/app.rc -O coff -o $@

clean:
	-rm -f $(TARGET) $(RES) 2>/dev/null; rm -f $(TARGET) $(RES)

endif
endif

# -- helpers -----------------------------------------------------------
info:
	@echo TOOLCHAIN=$(TOOLCHAIN)
	@echo BUILDDIR=$(BUILDDIR)
	@echo TARGET=$(TARGET)
	@echo CC=$(CC)
	@echo CFLAGS=$(CFLAGS)
	@echo LDFLAGS=$(LDFLAGS)
	@echo WEBVIEW2_INC=$(WEBVIEW2_INC)
	@echo WEBVIEW2_LIB=$(WEBVIEW2_LIB)

help:
	@echo "Targets: all (default), clean, info, help"
	@echo "Toolchains: msvc | w64devkit | msys2-* | mingw (auto-detected via compiler info)"
	@echo "Override: make CC=cl TOOLCHAIN=msvc"

.PHONY: all clean info help
