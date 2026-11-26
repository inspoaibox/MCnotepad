// ===========================================================================
// Markdown / HTML live preview: a pane to the right of the editor hosting
// Microsoft Edge WebView2. The loader DLL (WebView2Loader.dll, next to the
// exe) is loaded at runtime, so the editor works without it.
//
// The page is served from https://mcnotepad.preview/ (built in memory);
// relative links, images and styles resolve against the file's folder,
// mapped to https://mcnotepad.files/. Markdown updates replace the body in
// place (no flicker); HTML reloads and keeps its scroll position.
// ===========================================================================

constexpr wchar_t kPreviewPageHost[] = L"https://mcnotepad.preview/";
constexpr wchar_t kPreviewFilesHost[] = L"mcnotepad.files";
constexpr UINT_PTR kTimerPreview = 4110;
constexpr UINT kPreviewDelayMs = 300;
constexpr size_t kPreviewMaxChars = 4u << 20;  // larger documents are not previewed

// Minimal COM handler for WebView2 callbacks: Invoke forwards to a lambda.
template <typename Interface, typename... Args>
class WebViewHandler final : public Interface {
public:
    explicit WebViewHandler(std::function<HRESULT(Args...)> function) : function_(std::move(function)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {
        if (riid == __uuidof(Interface) || riid == IID_IUnknown) {
            *object = static_cast<Interface*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG left = --references_;
        if (left == 0) delete this;
        return left;
    }
    HRESULT STDMETHODCALLTYPE Invoke(Args... args) override {
        try {
            return function_(args...);
        } catch (...) {
            return E_FAIL;
        }
    }

private:
    std::function<HRESULT(Args...)> function_;
    std::atomic<ULONG> references_{1};
};

template <typename Interface, typename... Args, typename Function>
Interface* MakeHandler(Function&& function) {
    return new WebViewHandler<Interface, Args...>(std::function<HRESULT(Args...)>(std::forward<Function>(function)));
}

enum class PreviewKind { None, Markdown, Html, Svg };

struct LivePreviewState {
    HWND pane = nullptr;
    ICoreWebView2Environment* environment = nullptr;
    ICoreWebView2Controller* controller = nullptr;
    ICoreWebView2* webview = nullptr;
    bool creating = false;
    bool unavailable = false;            // WebView2 runtime or loader missing
    std::wstring message;                // shown in the pane while there is no page
    HWND shownEditor = nullptr;          // document the page belongs to
    PreviewKind shownKind = PreviewKind::None;
    std::wstring mappedFolder;
    std::string page;                    // UTF-8 page served for the next request
    double scrollRatio = -1;             // remembered across reloads
    bool pageReady = false;
    bool dragging = false;
    int dragStartX = 0, dragStartRatio = 50;
} g_preview;

PreviewKind PreviewKindFor(const TabDocument& document) {
    std::wstring extension = fs::path(document.path).extension().wstring();
    for (wchar_t& c : extension) c = towlower(c);
    if (extension == L".svg") return PreviewKind::Svg;
    const int language = EffectiveLanguage(document);
    if (language == kLanguageMarkdown) return PreviewKind::Markdown;
    if (language == kLanguageHtml) return PreviewKind::Html;
    return PreviewKind::None;
}

HWND PreviewPaneWindow() { return g_preview.pane; }

bool IsPreviewable(const TabDocument& document) { return PreviewKindFor(document) != PreviewKind::None; }

bool PreviewVisible() {
    return g_preview.pane && IsWindowVisible(g_preview.pane);
}

// ---- Page building ---------------------------------------------------------------------------

std::string MarkdownStyle() {
    const bool dark = g_app.darkTheme;
    const ThemePalette& palette = CurrentPalette();
    char colors[512]{};
    sprintf_s(colors, ":root{--bg:#%02x%02x%02x;--fg:#%02x%02x%02x;--muted:%s;--border:%s;--code:%s;--link:%s;--quote:%s;}",
        GetRValue(palette.editorBackground), GetGValue(palette.editorBackground), GetBValue(palette.editorBackground),
        GetRValue(palette.editorText), GetGValue(palette.editorText), GetBValue(palette.editorText),
        dark ? "#9aa1ad" : "#59636e", dark ? "#3a3f4b" : "#d1d9e0", dark ? "rgba(110,118,129,.25)" : "rgba(129,139,152,.14)",
        dark ? "#58a6ff" : "#0969da", dark ? "#3a3f4b" : "#d1d9e0");
    return std::string(colors) + R"CSS(
html{background:var(--bg);color:var(--fg);}
body{max-width:900px;margin:0 auto;padding:24px 32px 64px;font:15px/1.65 "Segoe UI","Microsoft YaHei UI","Microsoft YaHei",sans-serif;word-wrap:break-word;}
h1,h2,h3,h4,h5,h6{margin:1.4em 0 .6em;line-height:1.3;font-weight:600;}
h1{font-size:2em;padding-bottom:.3em;border-bottom:1px solid var(--border);}
h2{font-size:1.5em;padding-bottom:.3em;border-bottom:1px solid var(--border);}
h3{font-size:1.25em;} h4{font-size:1em;} h5{font-size:.9em;} h6{font-size:.85em;color:var(--muted);}
p,ul,ol,blockquote,table,pre{margin:0 0 1em;}
a{color:var(--link);text-decoration:none;} a:hover{text-decoration:underline;}
code,pre{font-family:"Cascadia Code","JetBrains Mono",Consolas,"Microsoft YaHei UI",monospace;font-size:.9em;}
code{background:var(--code);padding:.15em .4em;border-radius:5px;}
pre{background:var(--code);padding:14px 16px;border-radius:8px;overflow:auto;line-height:1.5;}
pre code{background:none;padding:0;font-size:1em;}
blockquote{margin-left:0;padding:0 1em;color:var(--muted);border-left:4px solid var(--quote);}
table{border-collapse:collapse;display:block;overflow:auto;}
th,td{border:1px solid var(--border);padding:6px 13px;} th{font-weight:600;}
tr:nth-child(2n){background:var(--code);}
img{max-width:100%;} hr{border:0;border-top:1px solid var(--border);margin:1.5em 0;}
ul.contains-task-list,li.task-list-item{list-style:none;} input[type=checkbox]{margin-right:.4em;}
)CSS";
}

// Script added to every preview page: reports the scroll position, restores
// it after a reload, follows the editor's scrolling and swaps in new
// Markdown without reloading.
std::string PreviewScript(double restoreRatio) {
    char restore[32]{};
    sprintf_s(restore, "%.6f", restoreRatio);
    return std::string(R"JS(<script>(function(){
var wv=window.chrome&&window.chrome.webview;var restore=)JS") + restore + R"JS(;var syncing=0;
function maxY(){return Math.max(0,document.documentElement.scrollHeight-innerHeight);}
function go(r){var m=maxY();if(m>0){syncing=Date.now();scrollTo(0,r*m);}}
addEventListener('load',function(){if(restore>=0)go(restore);});
var t;addEventListener('scroll',function(){clearTimeout(t);t=setTimeout(function(){if(wv&&Date.now()-syncing>300){var m=maxY();wv.postMessage('scroll:'+(m>0?scrollY/m:0));}},120);});
if(wv)wv.addEventListener('message',function(e){var d=e.data;
 if(typeof d==='string'&&d.indexOf('scrollto:')===0){go(parseFloat(d.slice(9)));}
 else if(d&&d.type==='markdown'&&window.mcnRender){var y=scrollY;mcnRender(d.text);scrollTo(0,y);}
});})();</script>)JS";
}

// The document's text as a JavaScript string literal that is safe inside <script>.
std::string MarkdownSourceLiteral(const TabDocument& document) {
    std::string literal = WideToUtf8(JsonString(GetEditorRawText(document.editor)));
    for (size_t pos = 0; (pos = literal.find("</", pos)) != std::string::npos; pos += 3) literal.replace(pos, 2, "<\\/");
    return literal;
}

// Markdown is converted inside the page by Showdown running in WebView2's
// JavaScript engine: far faster than the embedded interpreter (a 1000-line
// file took over a second there), and the editor's thread never waits for it.
std::string BuildMarkdownPage(const TabDocument& document, bool hasFolder) {
    std::string page = "<!DOCTYPE html><html><head><meta charset=\"utf-8\">";
    if (hasFolder) page += "<base href=\"https://mcnotepad.files/\">";
    page += "<style>" + MarkdownStyle() + "</style>";
    page += "<script src=\"https://mcnotepad.preview/showdown.js\"></script></head><body><article id=\"mcn-markdown\"></article>";
    page += R"JS(<script>(function(){
var conv=window.showdown?new showdown.Converter({tables:true,strikethrough:true,tasklists:true,ghCodeBlocks:true,simplifiedAutoLink:true}):null;
window.mcnRender=function(t){var m=document.getElementById('mcn-markdown');
 if(!conv){m.textContent='Markdown 预览组件 showdown.js 加载失败。';return;}
 try{m.innerHTML=conv.makeHtml(t);}catch(e){m.textContent='Markdown 转换失败：'+e;}};
mcnRender()JS" + MarkdownSourceLiteral(document) + ");})();</script>";
    page += PreviewScript(g_preview.scrollRatio) + "</body></html>";
    return page;
}

// The user's HTML as written, plus a <base> for relative paths and the script.
std::string BuildHtmlPage(const TabDocument& document, bool hasFolder) {
    std::string page = WideToUtf8(GetEditorRawText(document.editor));
    std::replace(page.begin(), page.end(), '\r', '\n');
    std::string lower = page;
    for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (hasFolder) {
        const std::string base = "<base href=\"https://mcnotepad.files/\">";
        const size_t head = lower.find("<head");
        const size_t headEnd = head == std::string::npos ? std::string::npos : lower.find('>', head);
        if (headEnd != std::string::npos) page.insert(headEnd + 1, base);
        else page.insert(0, base);
        lower = page;
        for (char& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    const std::string script = PreviewScript(g_preview.scrollRatio);
    const size_t bodyEnd = lower.rfind("</body>");
    if (bodyEnd != std::string::npos) page.insert(bodyEnd, script);
    else page += script;
    return page;
}

std::string BuildSvgPage(const TabDocument& document) {
    std::string page = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>html,body{margin:0;height:100%;background:";
    page += g_app.darkTheme ? "#1e1e1e" : "#ffffff";
    page += ";}body{display:flex;align-items:center;justify-content:center;}svg{max-width:96%;max-height:96%;}</style></head><body>";
    page += WideToUtf8(GetEditorRawText(document.editor));
    page += PreviewScript(-1) + "</body></html>";
    return page;
}

// ---- WebView2 ---------------------------------------------------------------------------------

void PreviewSetMessage(const std::wstring& message) {
    g_preview.message = message;
    if (g_preview.pane) InvalidateRect(g_preview.pane, nullptr, TRUE);
}

void PreviewLayoutWebView() {
    if (!g_preview.controller || !g_preview.pane) return;
    RECT client{};
    GetClientRect(g_preview.pane, &client);
    client.left += Scale(5);  // splitter strip
    g_preview.controller->put_Bounds(client);
}

void RenderPreview(bool forceReload);

// The WebView paints this colour before a page is drawn, so loading and
// reloading never flash white in the dark theme.
void PreviewApplyBackground() {
    ICoreWebView2Controller2* controller2 = nullptr;
    if (!g_preview.controller || FAILED(g_preview.controller->QueryInterface(__uuidof(ICoreWebView2Controller2),
            reinterpret_cast<void**>(&controller2))) || !controller2) return;
    const COLORREF color = CurrentPalette().editorBackground;
    controller2->put_DefaultBackgroundColor(COREWEBVIEW2_COLOR{255, GetRValue(color), GetGValue(color), GetBValue(color)});
    controller2->Release();
}

void OpenExternally(const std::wstring& uri) {
    if (uri.rfind(L"http://", 0) == 0 || uri.rfind(L"https://", 0) == 0 || uri.rfind(L"mailto:", 0) == 0) {
        ShellExecuteW(g_app.window, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void OnWebViewReady() {
    ICoreWebView2* webview = g_preview.webview;
    ICoreWebView2Settings* settings = nullptr;
    if (SUCCEEDED(webview->get_Settings(&settings)) && settings) {
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDevToolsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->Release();
    }
    webview->AddWebResourceRequestedFilter((std::wstring(kPreviewPageHost) + L"*").c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    EventRegistrationToken token{};
    webview->add_WebResourceRequested(MakeHandler<ICoreWebView2WebResourceRequestedEventHandler, ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs*>(
        [](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
            ICoreWebView2WebResourceRequest* request = nullptr;
            LPWSTR uri = nullptr;
            if (FAILED(args->get_Request(&request)) || !request) return S_OK;
            request->get_Uri(&uri);
            const std::wstring address = uri ? uri : L"";
            if (uri) CoTaskMemFree(uri);
            request->Release();
            if (address.rfind(kPreviewPageHost, 0) != 0 || !g_preview.environment) return S_OK;
            std::wstring name = address.substr(wcslen(kPreviewPageHost));
            name = name.substr(0, name.find_first_of(L"?#"));
            static std::string showdownScript;
            const std::string* body = &g_preview.page;
            const wchar_t* headers = L"Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store";
            int status = 200;
            if (name == L"showdown.js") {
                if (showdownScript.empty()) {
                    LoadTextAsset(fs::path(CurrentExecutablePath()).parent_path() / L"resources" / L"showdown.js", showdownScript);
                }
                body = &showdownScript;
                headers = L"Content-Type: text/javascript; charset=utf-8\r\nCache-Control: max-age=86400";
                if (showdownScript.empty()) status = 404;
            } else if (name != L"index.html") {
                static const std::string empty;
                body = &empty;
                status = 404;
            }
            IStream* stream = SHCreateMemStream(reinterpret_cast<const BYTE*>(body->data()), static_cast<UINT>(body->size()));
            ICoreWebView2WebResourceResponse* response = nullptr;
            g_preview.environment->CreateWebResourceResponse(stream, status, status == 200 ? L"OK" : L"Not Found", headers, &response);
            if (response) {
                args->put_Response(response);
                response->Release();
            }
            if (stream) stream->Release();
            return S_OK;
        }), &token);
    // Links leave the preview: open them in the default browser.
    webview->add_NavigationStarting(MakeHandler<ICoreWebView2NavigationStartingEventHandler, ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs*>(
        [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
            LPWSTR uri = nullptr;
            args->get_Uri(&uri);
            const std::wstring address = uri ? uri : L"";
            if (uri) CoTaskMemFree(uri);
            if (address.rfind(kPreviewPageHost, 0) == 0 || address.rfind(L"about:", 0) == 0) return S_OK;
            args->put_Cancel(TRUE);
            OpenExternally(address);
            return S_OK;
        }), &token);
    webview->add_NewWindowRequested(MakeHandler<ICoreWebView2NewWindowRequestedEventHandler, ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs*>(
        [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
            LPWSTR uri = nullptr;
            args->get_Uri(&uri);
            if (uri) {
                OpenExternally(uri);
                CoTaskMemFree(uri);
            }
            args->put_Handled(TRUE);
            return S_OK;
        }), &token);
    webview->add_WebMessageReceived(MakeHandler<ICoreWebView2WebMessageReceivedEventHandler, ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*>(
        [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
            LPWSTR message = nullptr;
            if (SUCCEEDED(args->TryGetWebMessageAsString(&message)) && message) {
                const std::wstring text = message;
                CoTaskMemFree(message);
                if (text.rfind(L"scroll:", 0) == 0) g_preview.scrollRatio = _wtof(text.c_str() + 7);
            }
            return S_OK;
        }), &token);
    webview->add_NavigationCompleted(MakeHandler<ICoreWebView2NavigationCompletedEventHandler, ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*>(
        [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
            g_preview.pageReady = true;
            return S_OK;
        }), &token);
    PreviewSetMessage({});
    PreviewLayoutWebView();
    g_preview.controller->put_IsVisible(TRUE);
    RenderPreview(true);
}

void CreatePreviewWebView() {
    if (g_preview.creating || g_preview.controller || g_preview.unavailable) return;
    using CreateEnvironment = HRESULT(STDAPICALLTYPE*)(PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
    static HMODULE loader = nullptr;
    if (!loader) loader = LoadLibraryW((fs::path(CurrentExecutablePath()).parent_path() / L"WebView2Loader.dll").c_str());
    const auto create = loader ? reinterpret_cast<CreateEnvironment>(GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions")) : nullptr;
    const auto fail = [](const std::wstring& why) {
        g_preview.creating = false;
        g_preview.unavailable = true;
        PreviewSetMessage(why + L"\n\n预览需要 Microsoft Edge WebView2 运行时（Windows 11 自带）。\n点击此处打开下载页面。");
    };
    if (!create) {
        fail(L"没有找到 WebView2Loader.dll。");
        return;
    }
    g_preview.creating = true;
    PreviewSetMessage(L"正在加载预览…");
    const std::wstring userData = (g_app.dataDirectory / L"webview2").wstring();
    const HRESULT started = create(nullptr, userData.c_str(), nullptr,
        MakeHandler<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, HRESULT, ICoreWebView2Environment*>(
            [fail](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT {
                if (FAILED(result) || !environment || !g_preview.pane) {
                    fail(L"无法启动 WebView2（错误 0x" + [result] { wchar_t b[16]{}; swprintf_s(b, L"%08X", static_cast<unsigned>(result)); return std::wstring(b); }() + L"）。");
                    return S_OK;
                }
                environment->AddRef();
                g_preview.environment = environment;
                environment->CreateCoreWebView2Controller(g_preview.pane,
                    MakeHandler<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, HRESULT, ICoreWebView2Controller*>(
                        [fail](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            g_preview.creating = false;
                            if (FAILED(result) || !controller) {
                                fail(L"无法创建预览窗口。");
                                return S_OK;
                            }
                            controller->AddRef();
                            g_preview.controller = controller;
                            PreviewApplyBackground();
                            controller->get_CoreWebView2(&g_preview.webview);
                            OnWebViewReady();
                            return S_OK;
                        }));
                return S_OK;
            }));
    if (FAILED(started)) fail(L"无法启动 WebView2。");
}

// Builds and shows the page for the active document. Markdown edits to the
// page already shown only replace its body.
void RenderPreview(bool forceReload) {
    TabDocument* document = ActiveDocument();
    if (!document || !PreviewVisible()) return;
    const PreviewKind kind = PreviewKindFor(*document);
    if (kind == PreviewKind::None) return;
    if (!g_preview.webview) {
        CreatePreviewWebView();
        return;
    }
    if (EditorTextLength(document->editor) > static_cast<LONG>(kPreviewMaxChars)) {
        g_preview.page = "<!DOCTYPE html><meta charset=\"utf-8\"><body style=\"font:14px 'Segoe UI';padding:24px\">文件太大（超过 4 MB），不提供实时预览。</body>";
        g_preview.webview->Navigate((std::wstring(kPreviewPageHost) + L"index.html").c_str());
        return;
    }
    const bool sameDocument = g_preview.shownEditor == document->editor && g_preview.shownKind == kind;
    if (!sameDocument) g_preview.scrollRatio = -1;
    if (!forceReload && sameDocument && kind == PreviewKind::Markdown && g_preview.pageReady) {
        const std::wstring json = L"{\"type\":\"markdown\",\"text\":" + JsonString(GetEditorRawText(document->editor)) + L"}";
        g_preview.webview->PostWebMessageAsJson(json.c_str());
        return;
    }
    // Relative paths resolve against the file's folder.
    const std::wstring folder = document->untitled || document->path.empty() ? std::wstring() : fs::path(document->path).parent_path().wstring();
    if (folder != g_preview.mappedFolder) {
        ICoreWebView2_3* webview3 = nullptr;
        if (SUCCEEDED(g_preview.webview->QueryInterface(__uuidof(ICoreWebView2_3), reinterpret_cast<void**>(&webview3))) && webview3) {
            webview3->ClearVirtualHostNameToFolderMapping(kPreviewFilesHost);
            if (!folder.empty()) {
                webview3->SetVirtualHostNameToFolderMapping(kPreviewFilesHost, folder.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
            }
            webview3->Release();
        }
        g_preview.mappedFolder = folder;
    }
    const bool hasFolder = !folder.empty();
    g_preview.page = kind == PreviewKind::Markdown ? BuildMarkdownPage(*document, hasFolder)
        : kind == PreviewKind::Html ? BuildHtmlPage(*document, hasFolder) : BuildSvgPage(*document);
    g_preview.shownEditor = document->editor;
    g_preview.shownKind = kind;
    g_preview.pageReady = false;
    static unsigned counter = 0;
    g_preview.webview->Navigate((std::wstring(kPreviewPageHost) + L"index.html?" + std::to_wstring(++counter)).c_str());
}

// ---- Pane window ------------------------------------------------------------------------------

LRESULT CALLBACK PreviewPaneProc(HWND pane, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        PreviewLayoutWebView();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(pane, &paint);
        RECT client{};
        GetClientRect(pane, &client);
        HBRUSH background = CreateSolidBrush(CurrentPalette().editorBackground);
        FillRect(dc, &client, background);
        DeleteObject(background);
        RECT splitter{0, 0, Scale(5), client.bottom};
        HBRUSH border = CreateSolidBrush(CurrentPalette().panelBorder);
        FillRect(dc, &splitter, border);
        DeleteObject(border);
        if (!g_preview.message.empty()) {
            RECT text = client;
            InflateRect(&text, -Scale(24), -Scale(24));
            text.left += Scale(5);
            HGDIOBJ previous = SelectObject(dc, g_app.uiFont);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, CurrentPalette().statusText);
            DrawTextW(dc, g_preview.message.c_str(), -1, &text, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
            SelectObject(dc, previous);
        }
        EndPaint(pane, &paint);
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(pane, &point);
            if (point.x < Scale(5)) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
                return TRUE;
            }
        }
        break;
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(lParam);
        if (x < Scale(5)) {
            POINT point{x, GET_Y_LPARAM(lParam)};
            ClientToScreen(pane, &point);
            g_preview.dragging = true;
            g_preview.dragStartX = point.x;
            g_preview.dragStartRatio = g_app.previewEditorPercent;
            SetCapture(pane);
        } else if (g_preview.unavailable) {
            ShellExecuteW(g_app.window, L"open", L"https://developer.microsoft.com/microsoft-edge/webview2/", nullptr, nullptr, SW_SHOWNORMAL);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (g_preview.dragging) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(pane, &point);
            RECT main{};
            GetClientRect(g_app.window, &main);
            const int area = std::max(1, static_cast<int>(main.right) - (g_app.toolboxVisible ? Scale(g_app.toolboxWidth) : 0));
            const int delta = (static_cast<int>(point.x) - g_preview.dragStartX) * 100 / area;
            g_app.previewEditorPercent = std::clamp(g_preview.dragStartRatio + delta, 20, 80);
            LayoutChildren(g_app.window);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (g_preview.dragging) {
            g_preview.dragging = false;
            ReleaseCapture();
            g_app.stateDirty = true;
        }
        return 0;
    case WM_DESTROY:
        if (g_preview.controller) {
            g_preview.controller->Close();
            g_preview.controller->Release();
        }
        if (g_preview.webview) g_preview.webview->Release();
        if (g_preview.environment) g_preview.environment->Release();
        g_preview = LivePreviewState{};
        return 0;
    default:
        break;
    }
    return DefWindowProcW(pane, message, wParam, lParam);
}

void EnsurePreviewPane() {
    if (g_preview.pane) return;
    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{};
        klass.hInstance = g_app.instance;
        klass.lpfnWndProc = Guarded<PreviewPaneProc>;
        klass.lpszClassName = L"MCnotepadPreview";
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        registered = RegisterClassW(&klass) != 0;
    }
    g_preview.pane = CreateWindowExW(0, L"MCnotepadPreview", nullptr, WS_CHILD | WS_CLIPCHILDREN, 0, 0, 0, 0,
        g_app.window, nullptr, g_app.instance, nullptr);
}

// The pane follows the active tab: shown when that tab has preview on and
// is Markdown / HTML / SVG.
void UpdatePreviewForActiveTab() {
    const TabDocument* document = ActiveDocument();
    const bool want = document && document->previewOpen && PreviewKindFor(*document) != PreviewKind::None;
    if (want) EnsurePreviewPane();
    const bool was = PreviewVisible();
    if (g_preview.pane) ShowWindow(g_preview.pane, want ? SW_SHOW : SW_HIDE);
    g_app.previewVisible = want;
    if (want != was) LayoutChildren(g_app.window);
    InvalidateToolbar();
    if (want) RenderPreview(true);
}

void ToggleLivePreview() {
    TabDocument* document = ActiveDocument();
    if (!document) return;
    if (PreviewKindFor(*document) == PreviewKind::None) {
        ShowNotice(L"只有 Markdown、HTML 和 SVG 文件可以预览（可在“语言”菜单中切换语言）");
        return;
    }
    document->previewOpen = !document->previewOpen;
    g_app.stateDirty = true;
    UpdatePreviewForActiveTab();
}

void PreviewOnTextChanged(HWND editor) {
    if (PreviewVisible() && ActiveEditor() == editor) SetTimer(g_app.window, kTimerPreview, kPreviewDelayMs, nullptr);
}

// The editor scrolled: move the preview to the same relative position.
void PreviewOnEditorScrolled(HWND editor) {
    if (!PreviewVisible() || !g_preview.webview || !g_preview.pageReady || ActiveEditor() != editor) return;
    const LONG first = static_cast<LONG>(SendMessageW(editor, EM_GETFIRSTVISIBLELINE, 0, 0));
    const LONG lines = static_cast<LONG>(SendMessageW(editor, EM_GETLINECOUNT, 0, 0));
    RECT client{};
    GetClientRect(editor, &client);
    POINTL bottom{0, client.bottom};
    const LONG lastChar = static_cast<LONG>(SendMessageW(editor, EM_CHARFROMPOS, 0, reinterpret_cast<LPARAM>(&bottom)));
    const LONG last = static_cast<LONG>(SendMessageW(editor, EM_EXLINEFROMCHAR, 0, lastChar));
    const LONG scrollable = std::max<LONG>(1, lines - (last - first + 1));
    const double ratio = std::clamp(static_cast<double>(first) / scrollable, 0.0, 1.0);
    wchar_t message[48]{};
    swprintf_s(message, L"scrollto:%.5f", ratio);
    g_preview.webview->PostWebMessageAsString(message);
}

void PreviewThemeChanged() {
    PreviewApplyBackground();
    if (PreviewVisible()) {
        InvalidateRect(g_preview.pane, nullptr, TRUE);
        RenderPreview(true);
    }
}

