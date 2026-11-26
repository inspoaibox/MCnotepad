// ===========================================================================
// Window chrome: menu bar, toolbar, tab strip and status bar.
// All four are custom-drawn so they follow the theme exactly (the Win32 menu
// bar and tab control can't be recoloured). They share the helpers below and
// paint through a memory DC to avoid flicker.
// ===========================================================================

constexpr int kToolButtonWidth = 30;
constexpr int kToolButtonHeight = 28;

class BufferedPaint {
public:
    explicit BufferedPaint(HWND window) : window_(window) {
        target_ = BeginPaint(window_, &paint_);
        GetClientRect(window_, &client_);
        dc_ = CreateCompatibleDC(target_);
        bitmap_ = CreateCompatibleBitmap(target_, std::max(1L, client_.right), std::max(1L, client_.bottom));
        previous_ = SelectObject(dc_, bitmap_);
        SetBkMode(dc_, TRANSPARENT);
        previousFont_ = SelectObject(dc_, g_app.uiFont ? g_app.uiFont : GetStockObject(DEFAULT_GUI_FONT));
    }
    ~BufferedPaint() {
        BitBlt(target_, 0, 0, client_.right, client_.bottom, dc_, 0, 0, SRCCOPY);
        SelectObject(dc_, previousFont_);
        SelectObject(dc_, previous_);
        DeleteObject(bitmap_);
        DeleteDC(dc_);
        EndPaint(window_, &paint_);
    }
    BufferedPaint(const BufferedPaint&) = delete;
    BufferedPaint& operator=(const BufferedPaint&) = delete;
    HDC dc() const { return dc_; }
    const RECT& client() const { return client_; }

private:
    HWND window_;
    PAINTSTRUCT paint_{};
    HDC target_ = nullptr;
    HDC dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previous_ = nullptr;
    HGDIOBJ previousFont_ = nullptr;
    RECT client_{};
};

void FillSolid(HDC dc, const RECT& rect, COLORREF color) {
    SetDCBrushColor(dc, color);
    FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

Color ToGdiplus(COLORREF color) {
    return Color(255, GetRValue(color), GetGValue(color), GetBValue(color));
}

// Anti-aliased rounded rectangle; topOnly keeps the bottom corners square
// (used for the active tab, which joins the editor below it).
void FillRounded(HDC dc, const RECT& rect, int radius, COLORREF color, bool topOnly = false) {
    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
    const REAL x = static_cast<REAL>(rect.left);
    const REAL y = static_cast<REAL>(rect.top);
    const REAL w = static_cast<REAL>(rect.right - rect.left);
    const REAL h = static_cast<REAL>(rect.bottom - rect.top);
    const REAL d = static_cast<REAL>(std::min({radius * 2, static_cast<int>(rect.right - rect.left),
        static_cast<int>(rect.bottom - rect.top)}));
    SolidBrush brush(ToGdiplus(color));
    if (d <= 1) {
        graphics.FillRectangle(&brush, x, y, w, h);
        return;
    }
    GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    if (topOnly) {
        path.AddLine(x + w, y + h, x, y + h);
    } else {
        path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
        path.AddArc(x, y + h - d, d, d, 90, 90);
    }
    path.CloseFigure();
    graphics.FillPath(&brush, &path);
}

void FillDot(HDC dc, int cx, int cy, int radius, COLORREF color) {
    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    SolidBrush brush(ToGdiplus(color));
    graphics.FillEllipse(&brush, static_cast<REAL>(cx - radius), static_cast<REAL>(cy - radius),
        static_cast<REAL>(radius * 2), static_cast<REAL>(radius * 2));
}

void DrawCross(HDC dc, int cx, int cy, int half, COLORREF color) {
    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(ToGdiplus(color), static_cast<REAL>(std::max(1, Scale(1))) * 1.2f);
    const REAL x = static_cast<REAL>(cx), y = static_cast<REAL>(cy), h = static_cast<REAL>(half);
    graphics.DrawLine(&pen, x - h, y - h, x + h, y + h);
    graphics.DrawLine(&pen, x + h, y - h, x - h, y + h);
}

void DrawPlus(HDC dc, int cx, int cy, int half, COLORREF color) {
    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(ToGdiplus(color), static_cast<REAL>(std::max(1, Scale(1))) * 1.3f);
    const REAL x = static_cast<REAL>(cx), y = static_cast<REAL>(cy), h = static_cast<REAL>(half);
    graphics.DrawLine(&pen, x - h, y, x + h, y);
    graphics.DrawLine(&pen, x, y - h, x, y + h);
}

void DrawChevron(HDC dc, int cx, int cy, bool left, COLORREF color) {
    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    Pen pen(ToGdiplus(color), static_cast<REAL>(std::max(1, Scale(1))) * 1.25f);
    const REAL x = static_cast<REAL>(cx);
    const REAL y = static_cast<REAL>(cy);
    const REAL half = static_cast<REAL>(Scale(4));
    if (left) {
        graphics.DrawLine(&pen, x + half / 2, y - half, x - half / 2, y);
        graphics.DrawLine(&pen, x - half / 2, y, x + half / 2, y + half);
    } else {
        graphics.DrawLine(&pen, x - half / 2, y - half, x + half / 2, y);
        graphics.DrawLine(&pen, x + half / 2, y, x - half / 2, y + half);
    }
}

void DrawHairline(HDC dc, int x1, int y1, int x2, int y2, COLORREF color) {
    RECT line{std::min(x1, x2), std::min(y1, y2), std::max(x1, x2) + (x1 == x2 ? 1 : 0),
        std::max(y1, y2) + (y1 == y2 ? 1 : 0)};
    FillSolid(dc, line, color);
}

int TextWidth(HDC dc, const std::wstring& text) {
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    return size.cx;
}

void DrawLabel(HDC dc, const std::wstring& text, RECT rect, COLORREF color, UINT format = DT_LEFT) {
    SetTextColor(dc, color);
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect,
        format | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}

// Glyph-based icons for actions that are about text itself: "{ }" for JSON
// formatting and "文A" for character encoding read better than pictograms.
void DrawGlyphIcon(HDC dc, const RECT& cell, const wchar_t* text, const wchar_t* face, int pointSize,
    int weight, COLORREF color) {
    HFONT font = CreateFontW(-MulDiv(pointSize, static_cast<int>(g_app.dpi), 72), 0, 0, 0, weight,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    HGDIOBJ previous = SelectObject(dc, font);
    RECT box = cell;
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, previous);
    DeleteObject(font);
}

// Word wrap: two text lines, the second bending back with an arrow.
void DrawWrapIcon(HDC dc, const RECT& cell, COLORREF color) {
    Graphics graphics(dc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    const REAL unit = static_cast<REAL>(Scale(16)) / 16.0f;
    const REAL ox = (cell.left + cell.right) / 2.0f - 8 * unit;
    const REAL oy = (cell.top + cell.bottom) / 2.0f - 8 * unit;
    Pen pen(ToGdiplus(color), 1.6f * unit);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    pen.SetLineJoin(LineJoinRound);
    const auto P = [&](REAL x, REAL y) { return PointF(ox + x * unit, oy + y * unit); };
    graphics.DrawLine(&pen, P(2, 3.5f), P(14, 3.5f));
    GraphicsPath hook;
    hook.AddLine(P(2, 8), P(11, 8));
    hook.AddBezier(P(11, 8), P(15, 8), P(15, 12.5f), P(11, 12.5f));
    hook.AddLine(P(11, 12.5f), P(7.5f, 12.5f));
    graphics.DrawPath(&pen, &hook);
    graphics.DrawLine(&pen, P(7.5f, 12.5f), P(9.5f, 10.5f));
    graphics.DrawLine(&pen, P(7.5f, 12.5f), P(9.5f, 14.5f));
    graphics.DrawLine(&pen, P(2, 12.5f), P(4.5f, 12.5f));
}

// Tinted toolbar icon: the PNGs are white-on-transparent masks.
void DrawToolIcon(HDC dc, int id, const RECT& cell, COLORREF color) {
    if (id == kIdToolbarWrap) {
        DrawWrapIcon(dc, cell, color);
        return;
    }
    if (id == kIdToolbarFormat) {
        DrawGlyphIcon(dc, cell, L"{}", L"Consolas", 10, FW_BOLD, color);
        return;
    }
    if (id == kIdToolbarEncoding) {
        DrawGlyphIcon(dc, cell, L"文A", g_app.uiFontFace.c_str(), 8, FW_SEMIBOLD, color);
        return;
    }
    const int cellWidth = static_cast<int>(cell.right - cell.left);
    const int cellHeight = static_cast<int>(cell.bottom - cell.top);
    const int size = std::min(Scale(16), std::max(Scale(12),
        std::min(cellWidth, cellHeight) - Scale(6)));
    const int x = (cell.left + cell.right - size) / 2;
    const int y = (cell.top + cell.bottom - size) / 2;
    Bitmap* image = ToolbarIconImage(id);
    if (image == nullptr || g_app.gdiplusToken == 0) {
        RECT box{x - Scale(2), y - Scale(2), x + size + Scale(2), y + size + Scale(2)};
        DrawToolbarIcon(dc, box, id, color);
        return;
    }
    Graphics graphics(dc);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
    ImageAttributes attributes;
    const ColorMatrix matrix = {
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 0,
        0, 0, 0, 1, 0,
        GetRValue(color) / 255.0f, GetGValue(color) / 255.0f, GetBValue(color) / 255.0f, 0, 1
    };
    attributes.SetColorMatrix(&matrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
    graphics.DrawImage(image, Rect(x, y, size, size), 0, 0,
        static_cast<INT>(image->GetWidth()), static_cast<INT>(image->GetHeight()), UnitPixel, &attributes);
}

void TrackLeave(HWND window) {
    TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
    TrackMouseEvent(&track);
}

RECT ToScreen(HWND window, RECT rect) {
    MapWindowPoints(window, nullptr, reinterpret_cast<POINT*>(&rect), 2);
    return rect;
}

HWND CreateTooltip(HWND owner) {
    HWND tip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, owner, nullptr, g_app.instance, nullptr);
    if (tip) {
        SendMessageW(tip, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), FALSE);
        SendMessageW(tip, TTM_SETMAXTIPWIDTH, 0, Scale(480));
        SetWindowTheme(tip, g_app.darkTheme ? L"DarkMode_Explorer" : nullptr, nullptr);
    }
    return tip;
}

// Popup menus (context menus, the menu bar's drop-downs) follow the theme via
// uxtheme's app-wide preferred mode (ordinals 135/136, Windows 10 1903+). This
// API is not documented; when it is missing, menus simply stay light.
void ApplyMenuColorMode() {
    using SetPreferredAppModeFn = int(WINAPI*)(int);
    using FlushMenuThemesFn = void(WINAPI*)();
    static SetPreferredAppModeFn setMode = nullptr;
    static FlushMenuThemesFn flush = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
        OSVERSIONINFOW version{sizeof(version)};
        if (auto getVersion = reinterpret_cast<RtlGetVersionFn>(
                GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"))) {
            getVersion(&version);
        }
        if (version.dwMajorVersion >= 10 && version.dwBuildNumber >= 18362) {
            if (HMODULE uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) {
                setMode = reinterpret_cast<SetPreferredAppModeFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
                flush = reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));
            }
        }
    }
    if (setMode && flush) {
        setMode(g_app.darkTheme ? 2 /* ForceDark */ : 3 /* ForceLight */);
        flush();
    }
}

// ---- Menu bar ---------------------------------------------------------------

struct MenuBarItem {
    std::wstring text;
    wchar_t mnemonic = 0;
    RECT rect{};
};

std::vector<MenuBarItem> g_menuItems;
int g_menuHot = -1;
int g_menuOpen = -1;
int g_menuSwitchTo = -1;
bool g_menuSelectionHasPopup = false;
HHOOK g_menuHook = nullptr;
POINT g_menuCursorAtOpen{};

void BuildMenuBarItems() {
    g_menuItems.clear();
    const int count = GetMenuItemCount(g_app.mainMenu);
    for (int index = 0; index < count; ++index) {
        wchar_t buffer[64]{};
        GetMenuStringW(g_app.mainMenu, index, buffer, static_cast<int>(std::size(buffer)), MF_BYPOSITION);
        MenuBarItem item;
        std::wstring label = buffer;
        // "文件(F)" -> label "文件", Alt+F opens it.
        const size_t open = label.find(L'(');
        if (open != std::wstring::npos && open + 1 < label.size()) {
            item.mnemonic = static_cast<wchar_t>(towupper(label[open + 1]));
            label.resize(open);
        }
        item.text = label;
        g_menuItems.push_back(std::move(item));
    }
}

void LayoutMenuBarItems(HWND bar) {
    HDC dc = GetDC(bar);
    HGDIOBJ previous = SelectObject(dc, g_app.uiFont);
    RECT client{};
    GetClientRect(bar, &client);
    const int paddings[] = {Scale(20), Scale(12), Scale(6)};
    int horizontalPadding = paddings[0];
    for (const int candidate : paddings) {
        int total = Scale(4);
        for (const MenuBarItem& item : g_menuItems) total += TextWidth(dc, item.text) + candidate;
        horizontalPadding = candidate;
        if (total <= client.right || candidate == paddings[std::size(paddings) - 1]) break;
    }
    int x = Scale(4);
    for (MenuBarItem& item : g_menuItems) {
        const int width = TextWidth(dc, item.text) + horizontalPadding;
        item.rect = RECT{x, Scale(3), x + width, client.bottom - Scale(3)};
        x += width;
    }
    SelectObject(dc, previous);
    ReleaseDC(bar, dc);
}

int MenuBarHitTest(POINT point) {
    for (size_t index = 0; index < g_menuItems.size(); ++index) {
        if (PtInRect(&g_menuItems[index].rect, point)) return static_cast<int>(index);
    }
    return -1;
}

// Popup menu windows ("#32768") showing on this thread: 1 for a drop-down,
// more while one of its submenus is open.
int OpenMenuLevels() {
    int levels = 0;
    EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM count) -> BOOL {
        wchar_t klass[16]{};
        GetClassNameW(window, klass, 16);
        if (wcscmp(klass, L"#32768") == 0 && IsWindowVisible(window)) ++*reinterpret_cast<int*>(count);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&levels));
    return levels;
}

// While a drop-down is open, hovering another title or pressing Left/Right
// moves to the neighbouring menu, like a native menu bar.
LRESULT CALLBACK MenuBarHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == MSGF_MENU && g_app.menuBar) {
        const MSG* message = reinterpret_cast<const MSG*>(lParam);
        const int count = static_cast<int>(g_menuItems.size());
        if (message->message == WM_MOUSEMOVE) {
            // Windows sends a synthetic move when a menu opens; only a real
            // move of the pointer may switch menus (keyboard users would
            // otherwise land on whatever title the idle pointer rests over).
            POINT point{};
            GetCursorPos(&point);
            if (point.x == g_menuCursorAtOpen.x && point.y == g_menuCursorAtOpen.y) {
                return CallNextHookEx(g_menuHook, code, wParam, lParam);
            }
            g_menuCursorAtOpen = point;
            ScreenToClient(g_app.menuBar, &point);
            const int hit = MenuBarHitTest(point);
            if (hit >= 0 && hit != g_menuOpen) {
                g_menuSwitchTo = hit;
                EndMenu();
            }
        } else if (message->message == WM_KEYDOWN && count > 0) {
            // In a submenu, Left closes just that submenu (Windows does it).
            if (message->wParam == VK_LEFT && OpenMenuLevels() <= 1) {
                g_menuSwitchTo = (g_menuOpen + count - 1) % count;
                EndMenu();
                return TRUE;
            }
            if (message->wParam == VK_RIGHT && !g_menuSelectionHasPopup) {
                g_menuSwitchTo = (g_menuOpen + 1) % count;
                EndMenu();
                return TRUE;
            }
        }
    }
    return CallNextHookEx(g_menuHook, code, wParam, lParam);
}

void OpenMenuBarItem(int index) {
    if (!g_app.menuBar || index < 0 || index >= static_cast<int>(g_menuItems.size())) return;
    SetMenuState();
    int command = 0;
    while (index >= 0) {
        g_menuOpen = index;
        g_menuSwitchTo = -1;
        g_menuSelectionHasPopup = false;
        InvalidateRect(g_app.menuBar, nullptr, FALSE);
        UpdateWindow(g_app.menuBar);
        const RECT anchor = ToScreen(g_app.menuBar, g_menuItems[static_cast<size_t>(index)].rect);
        TPMPARAMS params{sizeof(params), anchor};
        GetCursorPos(&g_menuCursorAtOpen);
        g_menuHook = SetWindowsHookExW(WH_MSGFILTER, MenuBarHook, nullptr, GetCurrentThreadId());
        command = TrackPopupMenuEx(GetSubMenu(g_app.mainMenu, index),
            TPM_LEFTALIGN | TPM_TOPALIGN | TPM_VERTICAL | TPM_RETURNCMD,
            anchor.left, anchor.bottom, g_app.window, &params);
        if (g_menuHook) UnhookWindowsHookEx(g_menuHook);
        g_menuHook = nullptr;
        index = command == 0 ? g_menuSwitchTo : -1;
    }
    g_menuOpen = -1;
    g_menuHot = -1;
    InvalidateRect(g_app.menuBar, nullptr, FALSE);
    if (command != 0) SendMessageW(g_app.window, WM_COMMAND, MAKEWPARAM(command, 0), 0);
}

// Alt / Alt+letter / F10 arrive as SC_KEYMENU because the window has no
// native menu bar. Returns true when handled.
bool HandleMenuKey(LPARAM character) {
    if (character == L' ') return false;  // Alt+Space: system menu
    int index = -1;
    if (character == 0) {
        index = 0;
    } else {
        const wchar_t key = static_cast<wchar_t>(towupper(static_cast<wint_t>(character)));
        for (size_t i = 0; i < g_menuItems.size(); ++i) {
            if (g_menuItems[i].mnemonic == key) index = static_cast<int>(i);
        }
    }
    if (index < 0) {
        MessageBeep(0);
        return true;
    }
    // Open after the key message finishes, so the menu loop starts cleanly.
    PostMessageW(g_app.window, kMessageOpenMenuBar, static_cast<WPARAM>(index), 0);
    return true;
}

LRESULT CALLBACK MenuBarProc(HWND bar, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        LayoutMenuBarItems(bar);
        return 0;
    case WM_PAINT: {
        BufferedPaint paint(bar);
        const ThemePalette& palette = CurrentPalette();
        FillSolid(paint.dc(), paint.client(), palette.toolbarBackground);
        for (size_t index = 0; index < g_menuItems.size(); ++index) {
            const MenuBarItem& item = g_menuItems[index];
            const bool open = static_cast<int>(index) == g_menuOpen;
            const bool hot = static_cast<int>(index) == g_menuHot;
            if (open || hot) {
                FillRounded(paint.dc(), item.rect, Scale(5),
                    open ? palette.toolbarButtonPressed : palette.toolbarButtonBackground);
            }
            DrawLabel(paint.dc(), item.text, item.rect, palette.toolbarText, DT_CENTER);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        const int hit = MenuBarHitTest(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        if (hit != g_menuHot) {
            g_menuHot = hit;
            InvalidateRect(bar, nullptr, FALSE);
        }
        TrackLeave(bar);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_menuHot != -1) {
            g_menuHot = -1;
            InvalidateRect(bar, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        const int hit = MenuBarHitTest(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        if (hit >= 0) OpenMenuBarItem(hit);
        return 0;
    }
    default:
        return DefWindowProcW(bar, message, wParam, lParam);
    }
}

// ---- Toolbar ----------------------------------------------------------------

struct ToolbarItem {
    int id;
    bool groupStart;
    bool alignRight = false;
    RECT rect{};
};

// Grouped by task; groups are separated by space rather than divider lines.
std::vector<ToolbarItem> g_toolItems = {
    {kIdToolbarNew, true}, {kIdToolbarOpen, false}, {kIdToolbarSave, false}, {kIdToolbarSaveAs, false},
    {kIdToolbarUndo, true}, {kIdToolbarRedo, false},
    {kIdToolbarFind, true}, {kIdToolbarReplace, false}, {kIdToolbarGoto, false},
    {kIdToolbarFormat, true}, {kIdToolbarRun, false},
    {kIdToolbarEncoding, true}, {kIdToolbarLineNumbers, false}, {kIdToolbarWrap, false},
    {kIdToolbarTheme, true, true},
};
int g_toolHot = -1;
int g_toolPressed = -1;
HWND g_toolTip = nullptr;

std::wstring ToolTipText(int id) {
    switch (id) {
    case kIdToolbarNew: return L"新建 (Ctrl+N)";
    case kIdToolbarOpen: return L"打开 (Ctrl+O)";
    case kIdToolbarSave: return L"保存 (Ctrl+S)";
    case kIdToolbarSaveAs: return L"另存为 (Ctrl+Shift+S)";
    case kIdToolbarUndo: return L"撤销 (Ctrl+Z)";
    case kIdToolbarRedo: return L"重做 (Ctrl+Y)";
    case kIdToolbarFind: return L"查找 (Ctrl+F)";
    case kIdToolbarReplace: return L"替换 (Ctrl+H)";
    case kIdToolbarGoto: return L"跳转到行 (Ctrl+G)";
    case kIdToolbarFormat: return L"格式化 JSON (Shift+Alt+F)";
    case kIdToolbarRun: return L"运行当前文件 (F5)";
    case kIdToolbarEncoding: return L"编码和换行符";
    case kIdToolbarLineNumbers: return L"显示行号";
    case kIdToolbarWrap: return L"自动换行";
    case kIdToolbarTheme: return L"切换主题";
    default: return {};
    }
}

bool ToolEnabled(int id) {
    const TabDocument* document = ActiveDocument();
    if (!document) return id == kIdToolbarNew || id == kIdToolbarOpen || id == kIdToolbarTheme;
    if (id == kIdToolbarUndo) return SendMessageW(document->editor, EM_CANUNDO, 0, 0) != 0;
    if (id == kIdToolbarRedo) return SendMessageW(document->editor, EM_CANREDO, 0, 0) != 0;
    return true;
}

bool ToolChecked(int id) {
    if (id == kIdToolbarLineNumbers) return g_app.showLineNumbers;
    if (id == kIdToolbarWrap) return g_app.wordWrap;
    return false;
}

void LayoutToolbarItems(HWND bar) {
    RECT client{};
    GetClientRect(bar, &client);
    const int itemCount = static_cast<int>(g_toolItems.size());
    const int leftCount = static_cast<int>(std::count_if(g_toolItems.begin(), g_toolItems.end(),
        [](const ToolbarItem& item) { return !item.alignRight; }));
    const int rightCount = itemCount - leftCount;
    const int groupBreaks = static_cast<int>(std::count_if(g_toolItems.begin(), g_toolItems.end(),
        [](const ToolbarItem& item) { return !item.alignRight && item.groupStart; })) - 1;
    const int padding = Scale(6);
    int width = Scale(kToolButtonWidth);
    int itemGap = Scale(2);
    int groupGap = Scale(12);
    const int available = std::max(0, static_cast<int>(client.right) - padding * 2);
    const auto required = [&]() {
        return itemCount * width + std::max(0, itemCount - rightCount - 1) * itemGap
            + std::max(0, rightCount - 1) * itemGap + std::max(0, groupBreaks) * groupGap;
    };
    if (required() > available) {
        itemGap = Scale(1);
        groupGap = Scale(6);
        const int fixed = std::max(0, itemCount - rightCount - 1) * itemGap
            + std::max(0, rightCount - 1) * itemGap + std::max(0, groupBreaks) * groupGap;
        width = std::max(Scale(22), (available - fixed) / std::max(1, itemCount));
    }
    const int height = Scale(kToolButtonHeight);
    const int top = (client.bottom - height) / 2;
    int x = padding;
    int right = client.right - padding;
    for (size_t index = 0; index < g_toolItems.size(); ++index) {
        ToolbarItem& item = g_toolItems[index];
        if (item.alignRight) {
            item.rect = RECT{right - width, top, right, top + height};
            right -= width + itemGap;
        } else {
            if (item.groupStart && index > 0) x += groupGap;
            item.rect = RECT{x, top, x + width, top + height};
            x += width + itemGap;
        }
        if (g_toolTip) {
            TOOLINFOW tool{sizeof(tool)};
            tool.hwnd = bar;
            tool.uId = index;
            tool.rect = item.rect;
            SendMessageW(g_toolTip, TTM_NEWTOOLRECTW, 0, reinterpret_cast<LPARAM>(&tool));
        }
    }
}

int ToolbarHitTest(POINT point) {
    for (size_t index = 0; index < g_toolItems.size(); ++index) {
        if (PtInRect(&g_toolItems[index].rect, point)) return static_cast<int>(index);
    }
    return -1;
}

RECT ToolbarItemScreenRect(int id) {
    for (const ToolbarItem& item : g_toolItems) {
        if (item.id == id) return ToScreen(g_app.toolbar, item.rect);
    }
    RECT fallback{};
    GetWindowRect(g_app.toolbar, &fallback);
    return fallback;
}

void InvalidateToolbar() {
    if (g_app.toolbar) InvalidateRect(g_app.toolbar, nullptr, FALSE);
}

LRESULT CALLBACK ToolbarProc(HWND bar, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_toolTip = CreateTooltip(bar);
        for (size_t index = 0; index < g_toolItems.size(); ++index) {
            std::wstring text = ToolTipText(g_toolItems[index].id);
            TOOLINFOW tool{sizeof(tool)};
            tool.uFlags = TTF_SUBCLASS;
            tool.hwnd = bar;
            tool.uId = index;
            tool.lpszText = text.data();
            SendMessageW(g_toolTip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        LayoutToolbarItems(bar);
        return 0;
    case WM_PAINT: {
        BufferedPaint paint(bar);
        const ThemePalette& palette = CurrentPalette();
        FillSolid(paint.dc(), paint.client(), palette.toolbarBackground);
        for (size_t index = 0; index < g_toolItems.size(); ++index) {
            const ToolbarItem& item = g_toolItems[index];
            const bool enabled = ToolEnabled(item.id);
            const bool checked = ToolChecked(item.id);
            const bool hot = enabled && static_cast<int>(index) == g_toolHot;
            const bool pressed = hot && static_cast<int>(index) == g_toolPressed;
            if (pressed) FillRounded(paint.dc(), item.rect, Scale(5), palette.toolbarButtonPressed);
            else if (hot) FillRounded(paint.dc(), item.rect, Scale(5), palette.toolbarButtonBackground);
            else if (checked) FillRounded(paint.dc(), item.rect, Scale(5), palette.accentSoft);
            const COLORREF color = !enabled ? palette.toolbarIconDisabled
                : checked ? palette.accent : palette.toolbarIcon;
            DrawToolIcon(paint.dc(), item.id, item.rect, color);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        const int hit = ToolbarHitTest(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        if (hit != g_toolHot) {
            g_toolHot = hit;
            InvalidateRect(bar, nullptr, FALSE);
        }
        TrackLeave(bar);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_toolHot = -1;
        InvalidateRect(bar, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        const int hit = ToolbarHitTest(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        if (hit >= 0 && ToolEnabled(g_toolItems[static_cast<size_t>(hit)].id)) {
            g_toolPressed = hit;
            SetCapture(bar);
            InvalidateRect(bar, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        const int pressed = g_toolPressed;
        g_toolPressed = -1;
        if (GetCapture() == bar) ReleaseCapture();
        InvalidateRect(bar, nullptr, FALSE);
        const int hit = ToolbarHitTest(POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        if (pressed >= 0 && hit == pressed) {
            SendMessageW(g_app.window, WM_COMMAND, MAKEWPARAM(g_toolItems[static_cast<size_t>(hit)].id, 0), 0);
        }
        return 0;
    }
    default:
        return DefWindowProcW(bar, message, wParam, lParam);
    }
}

// ---- Tab strip ----------------------------------------------------------------

constexpr int kTabPlus = -2;
constexpr int kTabScrollLeft = -3;
constexpr int kTabScrollRight = -4;
int g_tabHot = -1;
bool g_tabCloseHot = false;
int g_tabPressedClose = -1;
int g_tabDragIndex = -1;
int g_tabDragStartX = 0;
bool g_tabDragging = false;
int g_tabScroll = 0;
HWND g_tabTip = nullptr;

struct TabLayout {
    std::vector<RECT> tabs;
    RECT scrollLeft{};
    RECT scrollRight{};
    RECT plus{};
    int visibleLeft = 0;
    int visibleRight = 0;  // tabs are clipped here when they overflow
    int maxScroll = 0;
    bool overflow = false;
};

TabLayout ComputeTabLayout() {
    TabLayout layout;
    if (!g_app.tabs) return layout;
    RECT client{};
    GetClientRect(g_app.tabs, &client);
    HDC dc = GetDC(g_app.tabs);
    HGDIOBJ previous = SelectObject(dc, g_app.uiFont);
    std::vector<int> widths;
    int total = 0;
    for (const TabDocument& document : g_app.documents) {
        const int width = std::clamp(TextWidth(dc, document.title) + Scale(14 + 30), Scale(96), Scale(240));
        widths.push_back(width);
        total += width;
    }
    SelectObject(dc, previous);
    ReleaseDC(g_app.tabs, dc);

    const int left = Scale(6);
    const int plusWidth = Scale(30);
    const int top = Scale(5);
    const int buttonWidth = Scale(28);
    const int rightEdge = std::max(left, static_cast<int>(client.right) - plusWidth - Scale(6));
    const int unscrolledAvailable = std::max(0, rightEdge - left);
    layout.overflow = total > unscrolledAvailable;
    if (layout.overflow) {
        layout.visibleLeft = buttonWidth;
        layout.visibleRight = std::max(layout.visibleLeft, rightEdge - buttonWidth);
        layout.scrollLeft = RECT{0, 0, buttonWidth, client.bottom};
        layout.scrollRight = RECT{layout.visibleRight, 0, rightEdge, client.bottom};
    } else {
        layout.visibleLeft = left;
        layout.visibleRight = rightEdge;
    }
    const int available = std::max(0, layout.visibleRight - layout.visibleLeft);
    layout.maxScroll = std::max(0, total - available);
    g_tabScroll = std::clamp(g_tabScroll, 0, layout.maxScroll);
    int x = layout.visibleLeft - g_tabScroll;
    for (const int width : widths) {
        layout.tabs.push_back(RECT{x, top, x + width, client.bottom});
        x += width;
    }
    // "+" follows the last tab when there is room and stays pinned on the right
    // while the tab strip is horizontally scrolled.
    const int plusLeft = layout.overflow
        ? rightEdge + Scale(2)
        : std::min(x + Scale(4), rightEdge + Scale(2));
    const int plusTop = top + (client.bottom - top - plusWidth) / 2;
    layout.plus = RECT{plusLeft, plusTop, plusLeft + plusWidth, plusTop + plusWidth};
    return layout;
}

RECT TabCloseRect(const RECT& tab) {
    const int cy = (tab.top + tab.bottom) / 2;
    return RECT{tab.right - Scale(28), cy - Scale(10), tab.right - Scale(8), cy + Scale(10)};
}

void EnsureTabVisible(int index) {
    if (!g_app.tabs) return;
    const TabLayout layout = ComputeTabLayout();
    if (index < 0 || index >= static_cast<int>(layout.tabs.size())) return;
    const RECT& tab = layout.tabs[static_cast<size_t>(index)];
    if (tab.left < layout.visibleLeft) g_tabScroll -= layout.visibleLeft - tab.left;
    else if (tab.right > layout.visibleRight) g_tabScroll += tab.right - layout.visibleRight;
    g_tabScroll = std::clamp(g_tabScroll, 0, layout.maxScroll);
    InvalidateRect(g_app.tabs, nullptr, FALSE);
}

void InvalidateTabs() {
    if (g_app.tabs) InvalidateRect(g_app.tabs, nullptr, FALSE);
}

// Returns a tab index, a tab-strip control constant, or -1; sets onClose when
// the pointer is over a tab close box.
int TabHitTest(const TabLayout& layout, POINT point, bool* onClose) {
    if (onClose) *onClose = false;
    if (layout.overflow && PtInRect(&layout.scrollLeft, point)) return kTabScrollLeft;
    if (layout.overflow && PtInRect(&layout.scrollRight, point)) return kTabScrollRight;
    if (PtInRect(&layout.plus, point)) return kTabPlus;
    if (point.x < layout.visibleLeft || point.x >= layout.visibleRight) return -1;
    for (size_t index = 0; index < layout.tabs.size(); ++index) {
        if (!PtInRect(&layout.tabs[index], point)) continue;
        if (onClose) {
            const RECT close = TabCloseRect(layout.tabs[index]);
            *onClose = PtInRect(&close, point) != FALSE;
        }
        return static_cast<int>(index);
    }
    return -1;
}

bool CloseDocument(int index);

void MoveTab(int from, int to) {
    if (from < 0 || to < 0 || from >= static_cast<int>(g_app.documents.size()) ||
        to >= static_cast<int>(g_app.documents.size()) || from == to) return;
    auto moved = std::move(g_app.documents[static_cast<size_t>(from)]);
    g_app.documents.erase(g_app.documents.begin() + from);
    g_app.documents.insert(g_app.documents.begin() + to, std::move(moved));
    if (g_app.activeIndex == from) g_app.activeIndex = to;
    else if (from < g_app.activeIndex && g_app.activeIndex <= to) --g_app.activeIndex;
    else if (to <= g_app.activeIndex && g_app.activeIndex < from) ++g_app.activeIndex;
    g_app.stateDirty = true;
    EnsureTabVisible(to);
    InvalidateTabs();
    RefreshGutter();
}

void CopyTextToClipboard(const std::wstring& text) {
    // Clipboard history and other viewers briefly hold the clipboard after
    // every change; retry instead of silently copying nothing.
    bool opened = false;
    for (int attempt = 0; attempt < 20 && !(opened = OpenClipboard(g_app.window)); ++attempt) Sleep(15);
    if (!opened) {
        ShowNotice(L"剪贴板正被其他程序占用，复制失败");
        return;
    }
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        memcpy(GlobalLock(memory), text.c_str(), bytes);
        GlobalUnlock(memory);
        if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
    }
    CloseClipboard();
}

int IndexOfEditor(HWND editor) {
    for (size_t index = 0; index < g_app.documents.size(); ++index) {
        if (g_app.documents[index].editor == editor) return static_cast<int>(index);
    }
    return -1;
}

void CompareTabs(int leftIndex, int rightIndex);

void ShowTabContextMenu(int index, POINT screen) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) return;
    const TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    const bool hasFile = !document.untitled && !document.path.empty();
    const int last = static_cast<int>(g_app.documents.size()) - 1;
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 6, hasFile ? L"重命名文件...\tF2" : L"重命名标签...\tF2");
    AppendMenuW(menu, MF_STRING, 9, document.pinned ? L"取消固定标签" : L"固定标签");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 1, L"关闭\tCtrl+W");
    AppendMenuW(menu, MF_STRING | (index == 0 ? MF_GRAYED : 0), 8, L"关闭左侧标签");
    AppendMenuW(menu, MF_STRING | (index == last ? MF_GRAYED : 0), 3, L"关闭右侧标签");
    AppendMenuW(menu, MF_STRING | (last == 0 ? MF_GRAYED : 0), 2, L"关闭其他标签");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (hasFile ? 0 : MF_GRAYED), 4, L"复制文件路径");
    AppendMenuW(menu, MF_STRING | (hasFile ? 0 : MF_GRAYED), 5, L"在资源管理器中显示");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (index == g_app.activeIndex ? MF_GRAYED : 0), 7, L"与当前标签比较");
    const int command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, g_app.window, nullptr);
    DestroyMenu(menu);

    const HWND target = document.editor;
    const std::wstring path = document.path;
    switch (command) {
    case 1:
        CloseDocument(index);
        break;
    case 2:
        // From the end, so earlier indices stay valid; stop if the user cancels.
        for (int i = static_cast<int>(g_app.documents.size()) - 1; i >= 0; --i) {
            if (i < static_cast<int>(g_app.documents.size()) && g_app.documents[static_cast<size_t>(i)].editor != target) {
                if (!CloseDocument(i)) break;
            }
        }
        break;
    case 8:
        // Close from the beginning so the target tab remains addressable while
        // vector indices shift after each successful close.
        while (true) {
            const int targetIndex = IndexOfEditor(target);
            if (targetIndex <= 0) break;
            if (!CloseDocument(0)) break;
        }
        break;
    case 3:
        while (IndexOfEditor(target) >= 0 && IndexOfEditor(target) < static_cast<int>(g_app.documents.size()) - 1) {
            if (!CloseDocument(static_cast<int>(g_app.documents.size()) - 1)) break;
        }
        break;
    case 4:
        CopyTextToClipboard(path);
        break;
    case 5:
        ShellExecuteW(g_app.window, L"open", L"explorer.exe", (L"/select,\"" + path + L"\"").c_str(),
            nullptr, SW_SHOWNORMAL);
        break;
    case 6:
        RenameDocument(index);
        break;
    case 7:
        CompareTabs(g_app.activeIndex, index);
        break;
    case 9: {
        g_app.documents[static_cast<size_t>(index)].pinned = !g_app.documents[static_cast<size_t>(index)].pinned;
        const bool nowPinned = g_app.documents[static_cast<size_t>(index)].pinned;
        if (nowPinned) {
            int firstUnpinned = 0;
            while (firstUnpinned < static_cast<int>(g_app.documents.size()) &&
                g_app.documents[static_cast<size_t>(firstUnpinned)].pinned) ++firstUnpinned;
            if (index > firstUnpinned) MoveTab(index, firstUnpinned);
        }
        g_app.stateDirty = true;
        InvalidateTabs();
        ShowNotice(nowPinned ? L"标签已固定" : L"标签已取消固定");
        break;
    }
    default:
        break;
    }
    SetMenuState();
}

DWORD g_tabLastCloseTick = 0;

LRESULT CALLBACK TabStripProc(HWND strip, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_tabTip = CreateTooltip(strip);
        TOOLINFOW tool{sizeof(tool)};
        tool.uFlags = TTF_SUBCLASS;
        tool.hwnd = strip;
        tool.uId = 0;
        tool.lpszText = LPSTR_TEXTCALLBACKW;
        SendMessageW(g_tabTip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        InvalidateRect(strip, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        BufferedPaint paint(strip);
        HDC dc = paint.dc();
        const RECT& client = paint.client();
        const ThemePalette& palette = CurrentPalette();
        FillSolid(dc, client, palette.tabBarBackground);
        const TabLayout layout = ComputeTabLayout();
        const int bottom = client.bottom - 1;
        int activeLeft = -1;
        int activeRight = -1;

        const int saved = SaveDC(dc);
        IntersectClipRect(dc, layout.visibleLeft, 0, layout.visibleRight, client.bottom);
        for (size_t index = 0; index < layout.tabs.size(); ++index) {
            const TabDocument& document = g_app.documents[index];
            const RECT& tab = layout.tabs[index];
            const bool active = static_cast<int>(index) == g_app.activeIndex;
            const bool hot = static_cast<int>(index) == g_tabHot;
            if (active) {
                FillRounded(dc, tab, Scale(7), palette.tabActiveBackground, true);
                RECT accent{tab.left + Scale(7), tab.top, tab.right - Scale(7), tab.top + Scale(2)};
                FillSolid(dc, accent, palette.accent);
                activeLeft = tab.left;
                activeRight = tab.right;
            } else if (hot) {
                RECT surface{tab.left + Scale(2), tab.top + Scale(2), tab.right - Scale(2), tab.bottom - Scale(4)};
                FillRounded(dc, surface, Scale(6), palette.toolbarButtonBackground);
            } else {
                // Hairline between neighbouring idle tabs.
                const bool nextIsQuiet = index + 1 < layout.tabs.size() &&
                    static_cast<int>(index + 1) != g_app.activeIndex && static_cast<int>(index + 1) != g_tabHot;
                if (nextIsQuiet) {
                    DrawHairline(dc, tab.right - 1, tab.top + Scale(8), tab.right - 1, tab.bottom - Scale(9),
                        palette.panelBorder);
                }
            }
            if (document.pinned) {
                const int pinX = tab.left + Scale(9);
                const int pinY = (tab.top + tab.bottom) / 2;
                HPEN pinPen = CreatePen(PS_SOLID, std::max(1, Scale(1)), palette.accent);
                HGDIOBJ oldPen = SelectObject(dc, pinPen);
                MoveToEx(dc, pinX - Scale(3), pinY - Scale(5), nullptr);
                LineTo(dc, pinX + Scale(3), pinY - Scale(5));
                MoveToEx(dc, pinX, pinY - Scale(5), nullptr);
                LineTo(dc, pinX, pinY + Scale(3));
                MoveToEx(dc, pinX - Scale(4), pinY + Scale(4), nullptr);
                LineTo(dc, pinX + Scale(4), pinY + Scale(4));
                SelectObject(dc, oldPen);
                DeleteObject(pinPen);
            }
            RECT text{tab.left + Scale(document.pinned ? 24 : 14), tab.top, tab.right - Scale(30), tab.bottom - Scale(1)};
            DrawLabel(dc, document.title, text, active ? palette.tabActiveText : palette.tabText);

            // Close box on hover/active; otherwise an amber dot marks unsaved work.
            const RECT close = TabCloseRect(tab);
            const int cx = (close.left + close.right) / 2;
            const int cy = (close.top + close.bottom) / 2;
            if (hot && g_tabCloseHot) FillRounded(dc, close, Scale(4), palette.toolbarButtonPressed);
            if (hot) {
                DrawCross(dc, cx, cy, Scale(4), active ? palette.tabActiveText : palette.tabClose);
            } else if (document.modified) {
                FillDot(dc, cx, cy, Scale(4), palette.attention);
            } else if (active) {
                DrawCross(dc, cx, cy, Scale(4), palette.tabClose);
            }
        }
        RestoreDC(dc, saved);

        // Bottom hairline everywhere except under the active tab, so the active
        // tab and the editor read as one sheet.
        if (activeLeft >= 0) {
            DrawHairline(dc, 0, bottom, std::max(0, activeLeft), bottom, palette.panelBorder);
            DrawHairline(dc, std::min(activeRight, static_cast<int>(layout.visibleRight)), bottom,
                client.right, bottom, palette.panelBorder);
        } else {
            DrawHairline(dc, 0, bottom, client.right, bottom, palette.panelBorder);
        }

        if (layout.overflow) {
            const bool canScrollLeft = g_tabScroll > 0;
            const bool canScrollRight = g_tabScroll < layout.maxScroll;
            const bool leftHot = g_tabHot == kTabScrollLeft;
            const bool rightHot = g_tabHot == kTabScrollRight;
            if (leftHot && canScrollLeft) {
                FillRounded(dc, RECT{layout.scrollLeft.left + Scale(2), layout.scrollLeft.top + Scale(3),
                    layout.scrollLeft.right - Scale(2), layout.scrollLeft.bottom - Scale(3)}, Scale(5),
                    palette.toolbarButtonBackground);
            }
            if (rightHot && canScrollRight) {
                FillRounded(dc, RECT{layout.scrollRight.left + Scale(2), layout.scrollRight.top + Scale(3),
                    layout.scrollRight.right - Scale(2), layout.scrollRight.bottom - Scale(3)}, Scale(5),
                    palette.toolbarButtonBackground);
            }
            DrawChevron(dc, (layout.scrollLeft.left + layout.scrollLeft.right) / 2,
                (layout.scrollLeft.top + layout.scrollLeft.bottom) / 2, true,
                canScrollLeft ? palette.toolbarIcon : palette.toolbarIconDisabled);
            DrawChevron(dc, (layout.scrollRight.left + layout.scrollRight.right) / 2,
                (layout.scrollRight.top + layout.scrollRight.bottom) / 2, false,
                canScrollRight ? palette.toolbarIcon : palette.toolbarIconDisabled);
            DrawHairline(dc, layout.scrollLeft.right - 1, Scale(7), layout.scrollLeft.right - 1,
                client.bottom - Scale(7), palette.panelBorder);
            DrawHairline(dc, layout.scrollRight.left, Scale(7), layout.scrollRight.left,
                client.bottom - Scale(7), palette.panelBorder);
        }
        if (g_tabHot == kTabPlus) FillRounded(dc, layout.plus, Scale(5), palette.toolbarButtonBackground);
        DrawPlus(dc, (layout.plus.left + layout.plus.right) / 2, (layout.plus.top + layout.plus.bottom) / 2,
            Scale(6), palette.toolbarIcon);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (GetCapture() == strip && g_tabDragIndex >= 0) {
            const int x = GET_X_LPARAM(lParam);
            if (!g_tabDragging && std::abs(x - g_tabDragStartX) >= Scale(8)) g_tabDragging = true;
            if (g_tabDragging) {
                bool ignoredClose = false;
                const int hit = TabHitTest(ComputeTabLayout(), POINT{x, GET_Y_LPARAM(lParam)}, &ignoredClose);
                if (hit >= 0 && hit < static_cast<int>(g_app.documents.size()) && hit != g_tabDragIndex) {
                    MoveTab(g_tabDragIndex, hit);
                    g_tabDragIndex = hit;
                }
                InvalidateRect(strip, nullptr, FALSE);
                return 0;
            }
        }
        const TabLayout layout = ComputeTabLayout();
        bool onClose = false;
        const int hit = TabHitTest(layout, POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, &onClose);
        if (hit != g_tabHot || onClose != g_tabCloseHot) {
            if (hit != g_tabHot && g_tabTip) {
                TOOLINFOW tool{sizeof(tool)};
                tool.hwnd = strip;
                tool.uId = 0;
                tool.rect = hit >= 0 ? layout.tabs[static_cast<size_t>(hit)] : RECT{};
                SendMessageW(g_tabTip, TTM_NEWTOOLRECTW, 0, reinterpret_cast<LPARAM>(&tool));
            }
            g_tabHot = hit;
            g_tabCloseHot = onClose;
            InvalidateRect(strip, nullptr, FALSE);
        }
        TrackLeave(strip);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_tabHot = -1;
        g_tabCloseHot = false;
        InvalidateRect(strip, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN: {
        bool onClose = false;
        const int hit = TabHitTest(ComputeTabLayout(), POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, &onClose);
        if (hit == kTabScrollLeft || hit == kTabScrollRight) {
            const TabLayout layout = ComputeTabLayout();
            const int step = std::max(Scale(96), layout.visibleRight - layout.visibleLeft - Scale(24));
            g_tabScroll += hit == kTabScrollLeft ? -step : step;
            g_tabScroll = std::clamp(g_tabScroll, 0, layout.maxScroll);
            InvalidateRect(strip, nullptr, FALSE);
        } else if (hit == kTabPlus) {
            NewDocument();
        } else if (hit >= 0 && onClose) {
            g_tabPressedClose = hit;
            SetCapture(strip);
        } else if (hit >= 0) {
            SelectTab(hit);
            g_tabDragIndex = hit;
            g_tabDragStartX = GET_X_LPARAM(lParam);
            g_tabDragging = false;
            SetCapture(strip);
        }
        SetMenuState();
        InvalidateRect(strip, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONUP: {
        const bool dragged = g_tabDragging;
        if (g_tabDragIndex >= 0) {
            g_tabDragIndex = -1;
            g_tabDragging = false;
            if (GetCapture() == strip) ReleaseCapture();
            if (dragged) {
                SetMenuState();
                InvalidateRect(strip, nullptr, FALSE);
                return 0;
            }
        }
        const int pressed = g_tabPressedClose;
        g_tabPressedClose = -1;
        if (GetCapture() == strip) ReleaseCapture();
        bool onClose = false;
        const int hit = TabHitTest(ComputeTabLayout(), POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, &onClose);
        if (pressed >= 0 && hit == pressed && onClose) {
            CloseDocument(pressed);
            g_tabLastCloseTick = GetTickCount();
            SetMenuState();
        }
        return 0;
    }
    case WM_MBUTTONUP: {
        const int hit = TabHitTest(ComputeTabLayout(), POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, nullptr);
        if (hit >= 0) {
            CloseDocument(hit);
            SetMenuState();
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        bool onClose = false;
        const int hit = TabHitTest(ComputeTabLayout(), POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, &onClose);
        // The second click of a quick pair on x, the arrows or "+" is just
        // another click; so is one on a tab that slid under the pointer
        // because the first click closed a tab.
        const bool justClosed = GetTickCount() - g_tabLastCloseTick <= GetDoubleClickTime();
        if (onClose || hit < -1 || (hit >= 0 && justClosed)) return SendMessageW(strip, WM_LBUTTONDOWN, wParam, lParam);
        if (hit == -1) NewDocument();  // double-click empty space: new tab
        else if (hit >= 0) RenameDocument(hit);
        return 0;
    }
    case WM_RBUTTONUP: {
        const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int hit = TabHitTest(ComputeTabLayout(), point, nullptr);
        if (hit >= 0) {
            POINT screen = point;
            ClientToScreen(strip, &screen);
            ShowTabContextMenu(hit, screen);
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        const TabLayout layout = ComputeTabLayout();
        if (layout.overflow) {
            const int notches = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            g_tabScroll -= notches * Scale(100);
            g_tabScroll = std::clamp(g_tabScroll, 0, layout.maxScroll);
            InvalidateRect(strip, nullptr, FALSE);
        }
        return 0;
    }
    case WM_NOTIFY: {
        auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (header && header->code == TTN_GETDISPINFOW) {
            auto* info = reinterpret_cast<NMTTDISPINFOW*>(lParam);
            static std::wstring text;
            text.clear();
            if (g_tabHot >= 0 && g_tabHot < static_cast<int>(g_app.documents.size())) {
                const TabDocument& document = g_app.documents[static_cast<size_t>(g_tabHot)];
                text = document.untitled || document.path.empty() ? L"尚未保存到磁盘的新文件" : document.path;
            }
            info->lpszText = text.data();
            return 0;
        }
        break;
    }
    default:
        break;
    }
    return DefWindowProcW(strip, message, wParam, lParam);
}

// ---- Status bar -------------------------------------------------------------

enum StatusField { kStatusEncoding = 0, kStatusLineEnding = 1, kStatusLanguage = 2, kStatusFieldCount = 3 };
int g_statusHot = -1;
RECT g_statusFields[kStatusFieldCount]{};

// Real line/column and selection length of the active document.
void CaretInfo(const TabDocument& document, int& line, int& column, int& selected) {
    CHARRANGE selection{};
    SendMessageW(document.editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    selected = selection.cpMax - selection.cpMin;
    TextDocumentRef tom(document.editor);
    LONG lineStart = 0;
    const std::wstring& cached = g_textCache.text;
    if (g_textCache.editor == document.editor && g_textCache.hash == document.textHash && document.textHash != 0 &&
        static_cast<LONG>(cached.size()) == EditorTextLength(document.editor) && selection.cpMax <= static_cast<LONG>(cached.size())) {
        // The text kept from the last edit answers this without RichEdit
        // walking its paragraphs from the top (this runs on every repaint).
        const auto caret = cached.begin() + selection.cpMax;
        line = 1 + static_cast<int>(std::count(cached.begin(), caret, L'\r'));
        const size_t previousBreak = selection.cpMax > 0 ? cached.rfind(L'\r', static_cast<size_t>(selection.cpMax) - 1) : std::wstring::npos;
        lineStart = previousBreak == std::wstring::npos ? 0 : static_cast<LONG>(previousBreak) + 1;
    } else if (tom) {
        line = static_cast<int>(tom.ParagraphIndex(selection.cpMax));
        lineStart = tom.ParagraphStart(selection.cpMax);
    } else {
        const LRESULT lineIndex = SendMessageW(document.editor, EM_EXLINEFROMCHAR, 0, selection.cpMax);
        lineStart = static_cast<LONG>(SendMessageW(document.editor, EM_LINEINDEX, lineIndex, 0));
        line = static_cast<int>(lineIndex) + 1;
    }
    column = std::max(1, static_cast<int>(selection.cpMax - lineStart) + 1);
}

void ShowDocumentFieldMenu(int field, const RECT& anchor) {
    const TabDocument* document = ActiveDocument();
    if (!document) return;
    HMENU menu = CreatePopupMenu();
    if (field == kStatusEncoding) {
        for (int i = 0; i < kEncodingCount; ++i) AppendMenuW(menu, MF_STRING, kIdEncodingFirst + i, kEncodingNames[i]);
        CheckMenuRadioItem(menu, kIdEncodingFirst, kIdEncodingFirst + kEncodingCount - 1,
            kIdEncodingFirst + document->encoding, MF_BYCOMMAND);
    } else if (field == kStatusLineEnding) {
        for (int i = 0; i < kLineEndingCount; ++i) AppendMenuW(menu, MF_STRING, kIdLineEndingFirst + i, kLineEndingMenuNames[i]);
        CheckMenuRadioItem(menu, kIdLineEndingFirst, kIdLineEndingFirst + kLineEndingCount - 1,
            kIdLineEndingFirst + document->lineEnding, MF_BYCOMMAND);
    } else {
        for (int i = 0; i < kLanguageCount; ++i) AppendMenuW(menu, MF_STRING, kIdLanguageFirst + i, kLanguageNames[i]);
        CheckMenuRadioItem(menu, kIdLanguageFirst, kIdLanguageFirst + kLanguageCount - 1,
            kIdLanguageFirst + document->language, MF_BYCOMMAND);
    }
    TPMPARAMS params{sizeof(params), anchor};
    const int command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_VERTICAL,
        anchor.left, anchor.top, g_app.window, &params);
    DestroyMenu(menu);
    if (command != 0) SendMessageW(g_app.window, WM_COMMAND, MAKEWPARAM(command, 0), 0);
}

LRESULT CALLBACK StatusBarProc(HWND bar, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        BufferedPaint paint(bar);
        HDC dc = paint.dc();
        const RECT& client = paint.client();
        const ThemePalette& palette = CurrentPalette();
        FillSolid(dc, client, palette.statusBackground);
        DrawHairline(dc, 0, 0, client.right, 0, palette.panelBorder);
        const TabDocument* document = ActiveDocument();
        if (!document) return 0;

        // Save state first: the app's promise is that nothing is lost.
        int x = Scale(12);
        const int cy = client.bottom / 2;
        if (document->modified) {
            FillDot(dc, x + Scale(4), cy, Scale(4), palette.attention);
            x += Scale(14);
            const std::wstring state = L"未保存";
            DrawLabel(dc, state, RECT{x, 0, x + Scale(80), client.bottom}, palette.toolbarText);
            x += TextWidth(dc, state);
            if (!document->recoveryDirty) {
                const std::wstring backup = L"  ·  已自动备份";
                DrawLabel(dc, backup, RECT{x, 0, x + Scale(140), client.bottom}, palette.statusText);
                x += TextWidth(dc, backup);
            }
        } else {
            const std::wstring state = document->untitled ? L"新文件" : L"已保存";
            DrawLabel(dc, state, RECT{x, 0, x + Scale(80), client.bottom}, palette.statusText);
            x += TextWidth(dc, state);
        }
        if (document->largeFileMode) {
            const std::wstring mode = L"  ·  大文件编辑";
            DrawLabel(dc, mode, RECT{x, 0, x + Scale(150), client.bottom}, palette.accent);
            x += TextWidth(dc, mode);
        }

        int line = 1, column = 1, selected = 0;
        CaretInfo(*document, line, column, selected);
        wchar_t position[96]{};
        if (selected > 0) swprintf_s(position, L"行 %d，列 %d    已选 %d 个字符", line, column, selected);
        else swprintf_s(position, L"行 %d，列 %d", line, column);
        x += Scale(28);
        if (!g_app.statusNotice.empty() && GetTickCount64() < g_app.statusNoticeUntil) {
            DrawLabel(dc, g_app.statusNotice, RECT{x, 0, x + Scale(420), client.bottom}, palette.accent);
        } else {
            g_app.statusNotice.clear();
            DrawLabel(dc, position, RECT{x, 0, x + Scale(320), client.bottom}, palette.statusText);
        }

        // Right side: clickable document properties.
        const int language = EffectiveLanguage(*document);
        const std::wstring fields[kStatusFieldCount] = {
            kEncodingNames[std::clamp(document->encoding, 0, kEncodingCount - 1)],
            kLineEndingNames[std::clamp(document->lineEnding, 0, kLineEndingCount - 1)],
            language == kLanguagePlain ? std::wstring(L"纯文本") : std::wstring(kLanguageNames[language]),
        };
        int right = client.right - Scale(8);
        for (int field = kStatusFieldCount - 1; field >= 0; --field) {
            const int width = TextWidth(dc, fields[field]) + Scale(20);
            g_statusFields[field] = RECT{right - width, Scale(3), right, client.bottom - Scale(3)};
            if (field == g_statusHot) FillRounded(dc, g_statusFields[field], Scale(5), palette.toolbarButtonBackground);
            DrawLabel(dc, fields[field], g_statusFields[field],
                field == g_statusHot ? palette.toolbarText : palette.statusText, DT_CENTER);
            right -= width + Scale(2);
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        int hit = -1;
        for (int field = 0; field < kStatusFieldCount; ++field) {
            if (PtInRect(&g_statusFields[field], point)) hit = field;
        }
        if (hit != g_statusHot) {
            g_statusHot = hit;
            InvalidateRect(bar, nullptr, FALSE);
        }
        TrackLeave(bar);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_statusHot = -1;
        InvalidateRect(bar, nullptr, FALSE);
        return 0;
    case WM_SETCURSOR:
        if (g_statusHot >= 0) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    case WM_LBUTTONUP:
        if (g_statusHot >= 0) {
            const int field = g_statusHot;
            ShowDocumentFieldMenu(field, ToScreen(bar, g_statusFields[field]));
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(bar, message, wParam, lParam);
}

void RegisterChromeClasses() {
    const struct { const wchar_t* name; WNDPROC proc; UINT style; } classes[] = {
        {L"MCMenuBar", MenuBarProc, 0},
        {L"MCToolbar", ToolbarProc, 0},
        {L"MCTabStrip", TabStripProc, CS_DBLCLKS},
        {L"MCStatusBar", StatusBarProc, 0},
    };
    for (const auto& entry : classes) {
        WNDCLASSW klass{};
        klass.style = entry.style;
        klass.hInstance = g_app.instance;
        klass.lpfnWndProc = entry.proc;
        klass.lpszClassName = entry.name;
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&klass);
    }
}

void CreateChrome(HWND window) {
    RegisterChromeClasses();
    BuildMenuBarItems();
    const DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
    g_app.menuBar = CreateWindowExW(0, L"MCMenuBar", nullptr, style, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
    g_app.toolbar = CreateWindowExW(0, L"MCToolbar", nullptr, style, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
    g_app.tabs = CreateWindowExW(0, L"MCTabStrip", nullptr, style, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
    g_app.statusBar = CreateWindowExW(0, L"MCStatusBar", nullptr, style, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
}

void RefreshChrome() {
    ApplySearchPanelTheme();
    ApplyToolboxTheme();
    for (HWND window : {g_app.menuBar, g_app.toolbar, g_app.tabs, g_app.statusBar}) {
        if (window) InvalidateRect(window, nullptr, FALSE);
    }
    for (HWND tip : {g_toolTip, g_tabTip}) {
        if (tip) {
            SetWindowTheme(tip, g_app.darkTheme ? L"DarkMode_Explorer" : nullptr, nullptr);
            SendMessageW(tip, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), FALSE);
        }
    }
    if (g_app.menuBar) LayoutMenuBarItems(g_app.menuBar);
    if (g_app.toolbar) LayoutToolbarItems(g_app.toolbar);
}

