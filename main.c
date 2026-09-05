#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <dwmapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#ifdef USE_WEBVIEW
// w64devkit: static webview (C:\w64devkit\include\webview)
#include <webview/webview.h>
#else
#include <WebView2.h>
#ifdef _MSC_VER
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <wrl/async.h>
#include <roapi.h>
#include <windows.ui.notifications.h>
#include <windows.data.xml.dom.h>
#endif
#endif

#define APP_TITLE L"WhatsApp Desktop"
#define APP_TITLE_A "WhatsApp Desktop"
#define APP_URL L"https://web.whatsapp.com"
#define APP_URL_A "https://web.whatsapp.com"
#define MUTEX_NAME L"WhatsAppDesktopSingleInstanceMutex"
#define DEFAULT_WIDTH 1100
#define DEFAULT_HEIGHT 750

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1
#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#ifndef CSIDL_APPDATA
#define CSIDL_APPDATA 0x001A
#endif

HANDLE g_mutex = NULL;

void CenterWindow(HWND hwnd);
void SetDarkWindowFrame(HWND hwnd);
BOOL CheckSingleInstance(void);
void ShowToastNotification(LPCWSTR title, LPCWSTR body);

void CenterWindow(HWND hwnd) {
    RECT rc; GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, NULL, (sw-w)/2, (sh-h)/2, 0, 0, SWP_NOSIZE|SWP_NOZORDER);
}
void SetDarkWindowFrame(HWND hwnd) {
    BOOL dark=TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1, &dark, sizeof(dark));
    COLORREF caption=0x00211B11, text=0x00FFFFFF;
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &text, sizeof(text));
}
BOOL CheckSingleInstance(void) {
    g_mutex = CreateMutexW(NULL, TRUE, MUTEX_NAME);
    if (GetLastError()==ERROR_ALREADY_EXISTS) {
        HWND hwnd=FindWindowW(NULL, APP_TITLE);
        if(hwnd){ShowWindow(hwnd,SW_RESTORE); SetForegroundWindow(hwnd);}
        return FALSE;
    }
    return TRUE;
}
#ifdef _MSC_VER
void ShowToastNotification(LPCWSTR title, LPCWSTR body){
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return;
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Notifications::IToastNotificationManagerStatics> toastMgr;
    hr = RoGetActivationFactory(Microsoft::WRL::Wrappers::HStringReference(L"Windows.UI.Notifications.ToastNotificationManager").Get(), IID_PPV_ARGS(&toastMgr));
    if (FAILED(hr) || !toastMgr) return;
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Notifications::IToastNotifier> notifier;
    hr = toastMgr->CreateToastNotifierWithId(Microsoft::WRL::Wrappers::HStringReference(APP_TITLE).Get(), &notifier);
    if (FAILED(hr) || !notifier) return;
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Notifications::IToastNotificationFactory> factory;
    hr = RoGetActivationFactory(Microsoft::WRL::Wrappers::HStringReference(L"Windows.UI.Notifications.ToastNotification").Get(), IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) return;
    Microsoft::WRL::ComPtr<ABI::Windows::Data::Xml::Dom::IXmlDocument> xml;
    hr = toastMgr->GetTemplateContent(ABI::Windows::UI::Notifications::ToastTemplateType_ToastText02, &xml);
    if (FAILED(hr) || !xml) return;
    Microsoft::WRL::ComPtr<ABI::Windows::Data::Xml::Dom::IXmlNodeList> nodes;
    if (FAILED(xml->GetElementsByTagName(Microsoft::WRL::Wrappers::HStringReference(L"text").Get(), &nodes)) || !nodes) return;
    Microsoft::WRL::ComPtr<ABI::Windows::Data::Xml::Dom::IXmlNode> n1, n2;
    nodes->Item(0, &n1); nodes->Item(1, &n2);
    if (n1) {
        Microsoft::WRL::ComPtr<ABI::Windows::Data::Xml::Dom::IXmlNodeSerializer> s;
        if (SUCCEEDED(n1.As(&s))) s->put_InnerText(Microsoft::WRL::Wrappers::HStringReference(title).Get());
    }
    if (n2) {
        Microsoft::WRL::ComPtr<ABI::Windows::Data::Xml::Dom::IXmlNodeSerializer> s;
        if (SUCCEEDED(n2.As(&s))) s->put_InnerText(Microsoft::WRL::Wrappers::HStringReference(body).Get());
    }
    Microsoft::WRL::ComPtr<ABI::Windows::UI::Notifications::IToastNotification> toast;
    if (FAILED(factory->CreateToastNotification(xml.Get(), &toast)) || !toast) return;
    notifier->Show(toast.Get());
}
#else
void ShowToastNotification(LPCWSTR title, LPCWSTR body){ (void)title;(void)body; }
#endif

// ---------------------------------------------------------------------------
// w64devkit: webview backend
// ---------------------------------------------------------------------------
#ifdef USE_WEBVIEW

// JS -> C bridge: webview_bind creates JS function `notify(title, body)`
static void notify_cb(const char *seq, const char *req, void *arg) {
    (void)arg;
    // req is JSON array: ["title","body"]
    char titleA[512]={0}, bodyA[512]={0};
    if (req) {
        // crude JSON string extraction: find first "...", second "..."
        const char *p = strchr(req, '"');
        if (p) {
            p++;
            const char *e = strchr(p, '"');
            if (e) { size_t n = e - p; if (n < sizeof(titleA)) { memcpy(titleA, p, n); titleA[n]=0; } p = strchr(e+1, '"'); if(p){p++; e=strchr(p,'"'); if(e){size_t m=e-p; if(m < sizeof(bodyA)){memcpy(bodyA,p,m); bodyA[m]=0;}}}}
        }
    }
    // convert to wide for toast API
    WCHAR titleW[512]={0}, bodyW[512]={0};
    if (titleA[0]) MultiByteToWideChar(CP_UTF8,0,titleA,-1,titleW,512);
    if (bodyA[0])  MultiByteToWideChar(CP_UTF8,0,bodyA,-1,bodyW,512);
    if (titleW[0] || bodyW[0]) ShowToastNotification(titleW[0]?titleW:L"WhatsApp", bodyW);
    webview_return((webview_t)arg, seq, 0, "\"ok\"");
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow){
    (void)hInst;(void)hPrev;(void)lpCmdLine;(void)nCmdShow;
    if(!CheckSingleInstance()) return 0;

    webview_t w = webview_create(0, NULL);
    if (!w) { MessageBoxW(NULL, L"Failed to create webview. Is WebView2 Runtime installed?", L"Error", MB_ICONERROR); return 1; }

    webview_set_title(w, APP_TITLE_A);
    webview_set_size(w, DEFAULT_WIDTH, DEFAULT_HEIGHT, WEBVIEW_HINT_NONE);
    webview_navigate(w, APP_URL_A);

    // spoof UA + bridge Notification -> native toast via bind
    const char *js =
        "Object.defineProperty(navigator,'userAgent',{get:()=>'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/133.0.0.0 Safari/537.36'});"
        "Object.defineProperty(navigator,'appVersion',{get:()=>'5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/133.0.0.0 Safari/537.36'});"
        "(function(){"
        " window.Notification=function(t,o){o=o||{};var b=o.body||'';try{notify(t,b);}catch(e){} this.title=t;this.onclick=null;this.onclose=null;};"
        " window.Notification.permission='granted';"
        " window.Notification.requestPermission=function(cb){var p=Promise.resolve('granted');if(typeof cb==='function')cb('granted');return p;};"
        "})();";
    webview_init(w, js);
    webview_bind(w, "notify", notify_cb, w);

    // apply dark frame + exe icon to native window
    HWND hwnd = (HWND)webview_get_window(w);
    if (hwnd) {
        CenterWindow(hwnd); SetDarkWindowFrame(hwnd);
        HICON hBig = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
        HICON hSmall = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
        if (hBig) SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hBig);
        if (hSmall) SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmall);
    }

    webview_run(w);
    webview_destroy(w);
    if(g_mutex) CloseHandle(g_mutex);
    return 0;
}

#else
// ---------------------------------------------------------------------------
// MSVC / MSYS2 / generic MinGW: WebView2 backend (unchanged)
// ---------------------------------------------------------------------------

HWND g_hwnd = NULL;
ICoreWebView2Environment* g_env = NULL;
ICoreWebView2Controller* g_controller = NULL;
ICoreWebView2* g_webview = NULL;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void InitWebView2(HWND hwnd);
void InjectScripts(ICoreWebView2* webview);

static void ShowHrError(const WCHAR* prefix, HRESULT hr){
    WCHAR buf[512]; swprintf(buf,512,L"%s\nHRESULT=0x%08X",prefix,(unsigned)hr);
    MessageBoxW(NULL,buf,L"Error",MB_ICONERROR);
}

class EnvCompletedHandler;
class ControllerCompletedHandler;
class WebMessageHandler;
static EnvCompletedHandler* g_envHandler=NULL;
static ControllerCompletedHandler* g_ctrlHandler=NULL;
static WebMessageHandler* g_msgHandler=NULL;

class EnvCompletedHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler{
public:
    ULONG m_ref=1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override{
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||IsEqualIID(riid,IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)){
            *ppv=static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this); AddRef(); return S_OK;
        }
        *ppv=NULL; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement((LONG*)&m_ref);}
    ULONG STDMETHODCALLTYPE Release() override{ULONG c=InterlockedDecrement((LONG*)&m_ref); if(c==0) delete this; return c;}
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* env) override;
};
class ControllerCompletedHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler{
public:
    ULONG m_ref=1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override{
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||IsEqualIID(riid,IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)){
            *ppv=static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this); AddRef(); return S_OK;
        }
        *ppv=NULL; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement((LONG*)&m_ref);}
    ULONG STDMETHODCALLTYPE Release() override{ULONG c=InterlockedDecrement((LONG*)&m_ref); if(c==0) delete this; return c;}
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* controller) override;
};
class WebMessageHandler : public ICoreWebView2WebMessageReceivedEventHandler{
public:
    ULONG m_ref=1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override{
        if(!ppv) return E_POINTER;
        if(IsEqualIID(riid,IID_IUnknown)||IsEqualIID(riid,IID_ICoreWebView2WebMessageReceivedEventHandler)){
            *ppv=static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this); AddRef(); return S_OK;
        }
        *ppv=NULL; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return InterlockedIncrement((LONG*)&m_ref);}
    ULONG STDMETHODCALLTYPE Release() override{ULONG c=InterlockedDecrement((LONG*)&m_ref); if(c==0) delete this; return c;}
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override;
};

HRESULT STDMETHODCALLTYPE EnvCompletedHandler::Invoke(HRESULT errorCode, ICoreWebView2Environment* env){
    if(FAILED(errorCode)||!env){ ShowHrError(L"Failed to create WebView2 environment. Is WebView2 Runtime installed?",errorCode); return errorCode; }
    g_env=env; env->AddRef();
    HRESULT hr=env->CreateCoreWebView2Controller(g_hwnd, g_ctrlHandler);
    if(FAILED(hr)) ShowHrError(L"CreateCoreWebView2Controller failed",hr);
    return hr;
}
HRESULT STDMETHODCALLTYPE ControllerCompletedHandler::Invoke(HRESULT errorCode, ICoreWebView2Controller* controller){
    if(FAILED(errorCode)||!controller){ ShowHrError(L"Failed to create WebView2 controller",errorCode); return errorCode; }
    g_controller=controller; controller->AddRef();
    ICoreWebView2* webview=NULL;
    HRESULT hr=controller->get_CoreWebView2(&webview);
    if(FAILED(hr)||!webview){ MessageBoxW(NULL,L"Failed to get CoreWebView2",L"Error",MB_ICONERROR); return E_FAIL; }
    g_webview=webview;
    EventRegistrationToken token; webview->add_WebMessageReceived(g_msgHandler,&token);
    ICoreWebView2Settings* settings=NULL; webview->get_Settings(&settings);
    if(settings){
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsScriptEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->Release();
    }
    webview->Navigate(APP_URL);
    InjectScripts(webview);
    RECT rc; GetClientRect(g_hwnd,&rc); controller->put_Bounds(rc);
    return S_OK;
}
HRESULT STDMETHODCALLTYPE WebMessageHandler::Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args){
    (void)sender; if(!args) return E_POINTER;
    LPWSTR msg=NULL; HRESULT hr=args->TryGetWebMessageAsString(&msg);
    if(SUCCEEDED(hr)&&msg){
        WCHAR title[512]={0},body[512]={0};
        const WCHAR* t=wcsstr(msg,L"\"title\"");
        if(t){t=wcschr(t,L':'); if(t){t=wcschr(t,L'"'); if(t){t++; const WCHAR* e=wcschr(t,L'"'); if(e){size_t len=e-t; if(len<512){wcsncpy(title,t,len); title[len]=0;}}}}}
        const WCHAR* b=wcsstr(msg,L"\"body\"");
        if(b){b=wcschr(b,L':'); if(b){b=wcschr(b,L'"'); if(b){b++; const WCHAR* e=wcschr(b,L'"'); if(e){size_t len=e-b; if(len<512){wcsncpy(body,b,len); body[len]=0;}}}}}
        if(title[0]||body[0]) ShowToastNotification(title[0]?title:L"WhatsApp",body);
        CoTaskMemFree(msg);
    }
    return S_OK;
}
void InjectScripts(ICoreWebView2* webview){
    const WCHAR* script=
        L"Object.defineProperty(navigator,'userAgent',{get:()=> 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/133.0.0.0 Safari/537.36'});"
        L"Object.defineProperty(navigator,'appVersion',{get:()=> '5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/133.0.0.0 Safari/537.36'});"
        L"(function(){"
        L" window.Notification=function(t,o){o=o||{};var b=o.body||'';if(window.chrome&&window.chrome.webview)window.chrome.webview.postMessage(JSON.stringify({title:t,body:b}));this.title=t;this.onclick=null;this.onclose=null;};"
        L" window.Notification.permission='granted';"
        L" window.Notification.requestPermission=function(cb){var p=Promise.resolve('granted');if(typeof cb==='function')cb('granted');return p;};"
        L"})();";
    webview->AddScriptToExecuteOnDocumentCreated(script,NULL);
}
void InitWebView2(HWND hwnd){
    if(hwnd) g_hwnd=hwnd;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    WCHAR dataDir[MAX_PATH];
    if(SHGetFolderPathW(NULL,CSIDL_APPDATA,NULL,0,dataDir)==S_OK){
        WCHAR base[MAX_PATH]; SHGetFolderPathW(NULL,CSIDL_APPDATA,NULL,0,base);
        PathAppendW(base,L"WhatsAppDesktopLight"); CreateDirectoryW(base,NULL);
        PathAppendW(dataDir,L"WhatsAppDesktopLight\\UserData"); CreateDirectoryW(dataDir,NULL);
    } else { wcscpy(dataDir,L".\\UserData"); CreateDirectoryW(dataDir,NULL); }
    if(!g_envHandler) g_envHandler=new EnvCompletedHandler();
    if(!g_ctrlHandler) g_ctrlHandler=new ControllerCompletedHandler();
    if(!g_msgHandler) g_msgHandler=new WebMessageHandler();
    HRESULT hr=CreateCoreWebView2EnvironmentWithOptions(NULL,dataDir,NULL,g_envHandler);
    if(FAILED(hr)) ShowHrError(L"CreateCoreWebView2EnvironmentWithOptions failed",hr);
}
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
    switch(msg){
        case WM_CREATE: InitWebView2(hwnd); break;
        case WM_SIZE: if(g_controller){RECT rc; GetClientRect(hwnd,&rc); g_controller->put_Bounds(rc);} break;
        case WM_CLOSE: DestroyWindow(hwnd); break;
        case WM_DESTROY: PostQuitMessage(0); break;
        default: return DefWindowProcW(hwnd,msg,wParam,lParam);
    }
    return 0;
}
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow){
    (void)hPrev;(void)lpCmdLine;
    if(!CheckSingleInstance()) return 0;
    WNDCLASSEXW wc={0}; wc.cbSize=sizeof(WNDCLASSEXW); wc.lpfnWndProc=WndProc;
    wc.hInstance=hInst; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); wc.lpszClassName=L"WhatsAppDesktopClass";
    wc.hIcon=LoadIconW(hInst,MAKEINTRESOURCEW(1)); wc.hIconSm=wc.hIcon;
    if(!RegisterClassExW(&wc)){MessageBoxW(NULL,L"Window class registration failed",L"Error",MB_ICONERROR); return 1;}
    HWND hwnd=CreateWindowExW(0,L"WhatsAppDesktopClass",APP_TITLE,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,DEFAULT_WIDTH,DEFAULT_HEIGHT,NULL,NULL,hInst,NULL);
    if(!hwnd){MessageBoxW(NULL,L"Window creation failed",L"Error",MB_ICONERROR); return 1;}
    g_hwnd=hwnd; CenterWindow(hwnd); SetDarkWindowFrame(hwnd);
    ShowWindow(hwnd,nCmdShow); UpdateWindow(hwnd);
    MSG msg; while(GetMessage(&msg,NULL,0,0)){TranslateMessage(&msg); DispatchMessage(&msg);}
    if(g_mutex) CloseHandle(g_mutex);
    if(g_webview) g_webview->Release();
    if(g_controller) g_controller->Release();
    if(g_env) g_env->Release();
    return (int)msg.wParam;
}

#endif // USE_WEBVIEW
