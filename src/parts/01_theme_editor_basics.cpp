// Colour roles. "Chrome" is the menu bar, toolbar, tab strip and status bar;
// the active tab and the gutter share the editor colour so they read as one
// surface. `attention` marks unsaved work, the one accent the eye should catch.
struct ThemePalette {
    COLORREF windowBackground;        // chrome
    COLORREF panelBackground;         // chrome (dialogs)
    COLORREF panelBorder;             // hairlines
    COLORREF toolbarBackground;       // chrome
    COLORREF toolbarButtonBackground; // hover
    COLORREF toolbarButtonPressed;    // pressed
    COLORREF toolbarText;             // primary chrome text
    COLORREF toolbarIcon;
    COLORREF toolbarIconDisabled;
    COLORREF tabBarBackground;
    COLORREF tabBackground;
    COLORREF tabActiveBackground;     // = editor
    COLORREF tabText;                 // inactive tab text
    COLORREF tabActiveText;
    COLORREF tabClose;
    COLORREF gutterBackground;        // = editor
    COLORREF gutterText;
    COLORREF editorBackground;
    COLORREF editorText;
    COLORREF statusBackground;
    COLORREF statusText;              // secondary chrome text
    COLORREF controlBackground;
    COLORREF controlText;
    COLORREF controlBorder;
    COLORREF controlHover;
    COLORREF accent;
    COLORREF accentSoft;              // toggled-on buttons
    COLORREF syntaxTag;
    COLORREF syntaxString;
    COLORREF syntaxComment;
    COLORREF attention;               // unsaved marker
};

const ThemePalette& ThemePaletteFor(ThemeKind theme) {
    static constexpr ThemePalette palettes[] = {
        {   // Dark: neutral graphite chrome, slightly deeper editor, jade accent.
            .windowBackground = RGB(30, 32, 38), .panelBackground = RGB(30, 32, 38),
            .panelBorder = RGB(43, 46, 54), .toolbarBackground = RGB(30, 32, 38),
            .toolbarButtonBackground = RGB(44, 48, 57), .toolbarButtonPressed = RGB(54, 59, 70),
            .toolbarText = RGB(218, 221, 227), .toolbarIcon = RGB(184, 190, 200),
            .toolbarIconDisabled = RGB(84, 89, 99), .tabBarBackground = RGB(30, 32, 38),
            .tabBackground = RGB(30, 32, 38), .tabActiveBackground = RGB(23, 25, 30),
            .tabText = RGB(138, 144, 156), .tabActiveText = RGB(235, 237, 241),
            .tabClose = RGB(160, 166, 177), .gutterBackground = RGB(23, 25, 30),
            .gutterText = RGB(84, 90, 102), .editorBackground = RGB(23, 25, 30),
            .editorText = RGB(216, 220, 226), .statusBackground = RGB(30, 32, 38),
            .statusText = RGB(140, 146, 158), .controlBackground = RGB(38, 41, 48),
            .controlText = RGB(218, 221, 227), .controlBorder = RGB(60, 64, 74),
            .controlHover = RGB(44, 48, 57), .accent = RGB(63, 181, 168),
            .accentSoft = RGB(32, 60, 60), .syntaxTag = RGB(232, 125, 155),
            .syntaxString = RGB(226, 192, 120), .syntaxComment = RGB(108, 126, 124),
            .attention = RGB(229, 181, 103)
        },
        {   // Light: cool paper chrome around a white page.
            .windowBackground = RGB(243, 244, 246), .panelBackground = RGB(243, 244, 246),
            .panelBorder = RGB(222, 225, 230), .toolbarBackground = RGB(243, 244, 246),
            .toolbarButtonBackground = RGB(228, 231, 236), .toolbarButtonPressed = RGB(216, 220, 227),
            .toolbarText = RGB(36, 41, 51), .toolbarIcon = RGB(74, 82, 96),
            .toolbarIconDisabled = RGB(178, 184, 193), .tabBarBackground = RGB(243, 244, 246),
            .tabBackground = RGB(243, 244, 246), .tabActiveBackground = RGB(255, 255, 255),
            .tabText = RGB(104, 112, 125), .tabActiveText = RGB(24, 29, 38),
            .tabClose = RGB(100, 108, 120), .gutterBackground = RGB(255, 255, 255),
            .gutterText = RGB(172, 178, 188), .editorBackground = RGB(255, 255, 255),
            .editorText = RGB(32, 36, 44), .statusBackground = RGB(243, 244, 246),
            .statusText = RGB(98, 106, 119), .controlBackground = RGB(255, 255, 255),
            .controlText = RGB(36, 41, 51), .controlBorder = RGB(205, 210, 217),
            .controlHover = RGB(228, 231, 236), .accent = RGB(14, 128, 118),
            .accentSoft = RGB(214, 237, 233), .syntaxTag = RGB(184, 52, 98),
            .syntaxString = RGB(150, 98, 0), .syntaxComment = RGB(110, 128, 120),
            .attention = RGB(196, 124, 10)
        },
        {   // Eye-care green: low-contrast sage chrome, soft green page.
            .windowBackground = RGB(222, 233, 219), .panelBackground = RGB(222, 233, 219),
            .panelBorder = RGB(199, 214, 195), .toolbarBackground = RGB(222, 233, 219),
            .toolbarButtonBackground = RGB(208, 223, 204), .toolbarButtonPressed = RGB(195, 213, 191),
            .toolbarText = RGB(40, 62, 45), .toolbarIcon = RGB(62, 90, 68),
            .toolbarIconDisabled = RGB(152, 172, 152), .tabBarBackground = RGB(222, 233, 219),
            .tabBackground = RGB(222, 233, 219), .tabActiveBackground = RGB(237, 245, 234),
            .tabText = RGB(90, 112, 94), .tabActiveText = RGB(28, 48, 33),
            .tabClose = RGB(80, 104, 86), .gutterBackground = RGB(237, 245, 234),
            .gutterText = RGB(150, 170, 152), .editorBackground = RGB(237, 245, 234),
            .editorText = RGB(38, 56, 42), .statusBackground = RGB(222, 233, 219),
            .statusText = RGB(84, 106, 88), .controlBackground = RGB(246, 250, 244),
            .controlText = RGB(40, 62, 45), .controlBorder = RGB(190, 207, 186),
            .controlHover = RGB(208, 223, 204), .accent = RGB(52, 128, 82),
            .accentSoft = RGB(203, 227, 206), .syntaxTag = RGB(165, 60, 95),
            .syntaxString = RGB(130, 96, 10), .syntaxComment = RGB(100, 128, 106),
            .attention = RGB(182, 110, 20)
        },
        {   // Night blue: deep navy chrome, darker page, sky accent.
            .windowBackground = RGB(17, 28, 46), .panelBackground = RGB(17, 28, 46),
            .panelBorder = RGB(30, 46, 70), .toolbarBackground = RGB(17, 28, 46),
            .toolbarButtonBackground = RGB(29, 45, 69), .toolbarButtonPressed = RGB(37, 57, 87),
            .toolbarText = RGB(220, 230, 245), .toolbarIcon = RGB(168, 186, 212),
            .toolbarIconDisabled = RGB(68, 86, 112), .tabBarBackground = RGB(17, 28, 46),
            .tabBackground = RGB(17, 28, 46), .tabActiveBackground = RGB(12, 20, 34),
            .tabText = RGB(120, 140, 168), .tabActiveText = RGB(234, 241, 252),
            .tabClose = RGB(150, 170, 198), .gutterBackground = RGB(12, 20, 34),
            .gutterText = RGB(68, 88, 116), .editorBackground = RGB(12, 20, 34),
            .editorText = RGB(214, 225, 242), .statusBackground = RGB(17, 28, 46),
            .statusText = RGB(128, 148, 176), .controlBackground = RGB(24, 38, 60),
            .controlText = RGB(220, 230, 245), .controlBorder = RGB(44, 66, 98),
            .controlHover = RGB(29, 45, 69), .accent = RGB(98, 164, 246),
            .accentSoft = RGB(26, 50, 82), .syntaxTag = RGB(255, 137, 169),
            .syntaxString = RGB(240, 208, 121), .syntaxComment = RGB(110, 140, 130),
            .attention = RGB(240, 190, 100)
        }
    };
    return palettes[std::clamp(static_cast<int>(theme), 0, 3)];
}

const ThemePalette& CurrentPalette() {
    return ThemePaletteFor(g_app.theme);
}

const wchar_t* ToolbarIconFile(int id) {
    switch (id) {
    case kIdToolbarNew: return L"new.png";
    case kIdToolbarOpen: return L"open.png";
    case kIdToolbarSave: return L"save.png";
    case kIdToolbarSaveAs: return L"saveas.png";
    case kIdToolbarClose: return L"close.png";
    case kIdToolbarUndo: return L"undo.png";
    case kIdToolbarRedo: return L"redo.png";
    case kIdToolbarFind: return L"find.png";
    case kIdToolbarReplace: return L"replace.png";
    case kIdToolbarGoto: return L"goto.png";
    case kIdToolbarFormat: return L"format.png";
    case kIdToolbarRun: return L"run.png";
    case kIdToolbarEncoding: return L"encoding.png";
    case kIdToolbarLineNumbers: return L"linenumbers.png";
    case kIdToolbarWrap: return L"wrap.png";
    case kIdToolbarTheme: return L"theme.png";
    default: return nullptr;
    }
}

void LoadToolbarIconImages() {
    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, std::size(modulePath));
    if (length == 0 || length >= std::size(modulePath) || g_app.gdiplusToken == 0) return;
    const fs::path iconDirectory = fs::path(modulePath).parent_path() / L"resources" / L"icons";
    static constexpr int ids[] = {
        kIdToolbarNew, kIdToolbarOpen, kIdToolbarSave, kIdToolbarSaveAs, kIdToolbarClose,
        kIdToolbarUndo, kIdToolbarRedo, kIdToolbarFind, kIdToolbarReplace, kIdToolbarGoto,
        kIdToolbarFormat, kIdToolbarRun, kIdToolbarEncoding, kIdToolbarLineNumbers,
        kIdToolbarWrap, kIdToolbarTheme
    };
    g_app.toolbarIconImages.clear();
    g_app.toolbarIconImages.reserve(std::size(ids));
    for (int id : ids) {
        const wchar_t* fileName = ToolbarIconFile(id);
        auto image = std::make_unique<Bitmap>((iconDirectory / fileName).c_str(), FALSE);
        if (image->GetLastStatus() != Ok) image.reset();
        g_app.toolbarIconImages.push_back(std::move(image));
    }
}

Bitmap* ToolbarIconImage(int id) {
    static constexpr int ids[] = {
        kIdToolbarNew, kIdToolbarOpen, kIdToolbarSave, kIdToolbarSaveAs, kIdToolbarClose,
        kIdToolbarUndo, kIdToolbarRedo, kIdToolbarFind, kIdToolbarReplace, kIdToolbarGoto,
        kIdToolbarFormat, kIdToolbarRun, kIdToolbarEncoding, kIdToolbarLineNumbers,
        kIdToolbarWrap, kIdToolbarTheme
    };
    for (size_t i = 0; i < std::size(ids); ++i) {
        if (ids[i] == id && i < g_app.toolbarIconImages.size()) return g_app.toolbarIconImages[i].get();
    }
    return nullptr;
}

COLORREF EditorBackground() { return CurrentPalette().editorBackground; }
COLORREF EditorForeground() { return CurrentPalette().editorText; }
COLORREF GutterBackground() { return CurrentPalette().gutterBackground; }
COLORREF GutterForeground() { return CurrentPalette().gutterText; }
COLORREF ToolbarBackground() { return CurrentPalette().toolbarBackground; }
COLORREF ToolbarButtonBackground(bool pressed) {
    return pressed ? CurrentPalette().toolbarButtonPressed : CurrentPalette().toolbarButtonBackground;
}
COLORREF ToolbarTextColor() { return CurrentPalette().toolbarText; }

// Dark title bar for dark themes (Windows 10 20H1+ / 11; ignored elsewhere).
void ApplyWindowFrameTheme(HWND window) {
    if (!window) return;
    const BOOL dark = g_app.darkTheme ? TRUE : FALSE;
    constexpr DWORD kUseImmersiveDarkMode = 20;  // DWMWA_USE_IMMERSIVE_DARK_MODE
    if (SUCCEEDED(DwmSetWindowAttribute(window, kUseImmersiveDarkMode, &dark, sizeof(dark)))) {
        RedrawWindow(window, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
    }
}

void ApplyThemeToEditors(bool rehighlight = true) {
    const ThemePalette& palette = CurrentPalette();
    // Green is a light theme; only Dark and Blue get dark window chrome.
    g_app.darkTheme = g_app.theme == ThemeKind::Dark || g_app.theme == ThemeKind::Blue;
    DeleteObject(g_app.editorBrush);
    DeleteObject(g_app.toolbarBrush);
    DeleteObject(g_app.sidebarBrush);
    DeleteObject(g_app.windowBrush);
    DeleteObject(g_app.controlBrush);
    DeleteObject(g_app.statusBrush);
    g_app.editorBrush = CreateSolidBrush(palette.editorBackground);
    g_app.toolbarBrush = CreateSolidBrush(palette.toolbarBackground);
    g_app.sidebarBrush = CreateSolidBrush(palette.panelBackground);
    g_app.windowBrush = CreateSolidBrush(palette.windowBackground);
    g_app.controlBrush = CreateSolidBrush(palette.controlBackground);
    g_app.statusBrush = CreateSolidBrush(palette.statusBackground);
    for (TabDocument& document : g_app.documents) {
        ApplyEditorTheme(document.editor);
        document.highlightedLine = -1;
        document.bracketMatchStart = -1;
        document.bracketMatchEnd = -1;
        if (rehighlight) HighlightDocument(document);
        ApplyCurrentLineHighlight(document);
        ApplyBracketMatchHighlight(document, true);
    }
    if (g_app.gutter) InvalidateRect(g_app.gutter, nullptr, TRUE);
    ApplyMenuColorMode();
    RefreshChrome();
    if (g_app.settingsThemeCombo) SendMessageW(g_app.settingsThemeCombo, CB_SETCURSEL, static_cast<int>(g_app.theme), 0);
    for (HWND window : {g_app.window, g_app.settingsWindow, g_app.gotoWindow}) ApplyWindowFrameTheme(window);
    if (g_app.window) {
        InvalidateRect(g_app.window, nullptr, TRUE);
        g_app.stateDirty = true;
    }
}

void SetThemeKind(ThemeKind theme) {
    g_app.theme = theme;
    ApplyThemeToEditors();
}

std::wstring ToolbarLabel(int id) {
    switch (id) {
    case kIdToolbarNew: return L"新建";
    case kIdToolbarOpen: return L"打开";
    case kIdToolbarSave: return L"保存";
    case kIdToolbarSaveAs: return L"另存为";
    case kIdToolbarClose: return L"关闭";
    case kIdToolbarUndo: return L"撤销";
    case kIdToolbarRedo: return L"重做";
    case kIdToolbarFind: return L"查找";
    case kIdToolbarReplace: return L"替换";
    case kIdToolbarGoto: return L"跳转";
    case kIdToolbarFormat: return L"格式化";
    case kIdToolbarRun: return L"运行";
    case kIdToolbarEncoding: return L"编码";
    case kIdToolbarLineNumbers: return L"行号";
    case kIdToolbarWrap: return L"自动换行";
    case kIdToolbarTheme: return L"主题 ▼";
    case kIdToolbarSettings: return L"设置";
    case kIdThemeDark: return L"深色（默认）";
    case kIdThemeLight: return L"浅色";
    case kIdThemeGreen: return L"护眼绿";
    case kIdThemeBlue: return L"暗夜蓝";
    default: return {};
    }
}

int ToolbarButtonWidth(int id) {
    switch (id) {
    case kIdToolbarTheme:
        return 30;
    default:
        return 28;
    }
}

void ShowThemeMenu(const RECT& anchor) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, kIdThemeDark, L"深色");
    AppendMenuW(menu, MF_STRING, kIdThemeLight, L"浅色");
    AppendMenuW(menu, MF_STRING, kIdThemeGreen, L"护眼绿");
    AppendMenuW(menu, MF_STRING, kIdThemeBlue, L"暗夜蓝");
    CheckMenuRadioItem(menu, kIdThemeDark, kIdThemeBlue,
        kIdThemeDark + static_cast<int>(g_app.theme), MF_BYCOMMAND);

    TPMPARAMS params{sizeof(params), anchor};
    const int command = TrackPopupMenuEx(menu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_VERTICAL,
        anchor.left, anchor.bottom, g_app.window, &params);
    DestroyMenu(menu);
    if (command >= kIdThemeDark && command <= kIdThemeBlue) {
        SetThemeKind(static_cast<ThemeKind>(command - kIdThemeDark));
        SetMenuState();
    }
}


// RichEdit only sends EN_CHANGE / EN_VSCROLL when the event mask asks for it.
// Formatting changes (theme, highlight, font) must not mark a document as
// modified, so they temporarily mute notifications with this guard.
constexpr LPARAM kEditorEventMask = ENM_CHANGE | ENM_SCROLL | ENM_SELCHANGE;

class EditorEventMute {
public:
    explicit EditorEventMute(HWND editor) : editor_(editor) {
        if (editor_) previous_ = SendMessageW(editor_, EM_SETEVENTMASK, 0, 0);
    }
    ~EditorEventMute() {
        if (editor_) SendMessageW(editor_, EM_SETEVENTMASK, 0, previous_);
    }
    EditorEventMute(const EditorEventMute&) = delete;
    EditorEventMute& operator=(const EditorEventMute&) = delete;
private:
    HWND editor_ = nullptr;
    LRESULT previous_ = 0;
};

void ApplyEditorTheme(HWND editor) {
    if (!editor) return;
    EditorEventMute mute(editor);
    // Dark scrollbars in dark themes (system theme class on Windows 10 1809+).
    SetWindowTheme(editor, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    const COLORREF background = EditorBackground();
    const COLORREF foreground = EditorForeground();
    SendMessageW(editor, EM_SETBKGNDCOLOR, 0, background);
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR | CFM_BACKCOLOR;
    format.crTextColor = foreground;
    format.crBackColor = background;
    format.dwMask |= CFM_BOLD;  // headings may have left bold text behind
    format.dwEffects = 0;
    // SCF_DEFAULT too, so text inserted later (SetWindowText, typing) starts
    // in the theme colour without a separate colouring pass.
    SendMessageW(editor, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
    SendMessageW(editor, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
    InvalidateRect(editor, nullptr, TRUE);
}

void RefreshGutter() {
    if (g_app.gutter) InvalidateRect(g_app.gutter, nullptr, TRUE);
}

// Text Object Model access for colouring without moving the selection and
// without creating undo entries. Released automatically.
class TextDocumentRef {
public:
    explicit TextDocumentRef(HWND editor) {
        IRichEditOle* ole = nullptr;
        SendMessageW(editor, EM_GETOLEINTERFACE, 0, reinterpret_cast<LPARAM>(&ole));
        if (ole) {
            ole->QueryInterface(__uuidof(ITextDocument), reinterpret_cast<void**>(&document_));
            ole->Release();
        }
        if (document_) document_->Range(0, 0, &range_);
    }
    ~TextDocumentRef() {
        if (range_) range_->Release();
        if (document_) document_->Release();
    }
    TextDocumentRef(const TextDocumentRef&) = delete;
    TextDocumentRef& operator=(const TextDocumentRef&) = delete;
    explicit operator bool() const { return document_ != nullptr && range_ != nullptr; }
    ITextDocument* document() const { return document_; }

    // 1-based number of the real line (paragraph) containing the position.
    long ParagraphIndex(LONG position) {
        long index = 0;
        if (range_ && SUCCEEDED(range_->SetRange(position, position))) range_->GetIndex(tomParagraph, &index);
        return index;
    }

    // Character position where the real line containing `position` starts.
    // Character position where real line `index` (1-based) starts, or -1.
    LONG StartOfParagraph(long index) {
        long start = -1;
        if (range_ && index >= 1 && SUCCEEDED(range_->SetIndex(tomParagraph, index, 0))) range_->GetStart(&start);
        return static_cast<LONG>(start);
    }

    // The display line (one row on screen) around a position. Needs only the
    // layout near that position, unlike EM_LINEINDEX / EM_EXLINEFROMCHAR,
    // which lay the document out from the top down to the line asked for.
    std::pair<LONG, LONG> DisplayLineRange(LONG position) {
        long start = position, end = position;
        if (range_ && SUCCEEDED(range_->SetRange(position, position))) {
            range_->Expand(tomLine, nullptr);
            range_->GetStart(&start);
            range_->GetEnd(&end);
        }
        return {static_cast<LONG>(start), static_cast<LONG>(end)};
    }

    LONG ParagraphStart(LONG position) {
        long start = 0;
        if (range_ && SUCCEEDED(range_->SetRange(position, position)) &&
            SUCCEEDED(range_->StartOf(tomParagraph, tomMove, nullptr))) {
            range_->GetStart(&start);
        }
        return static_cast<LONG>(start);
    }

    // A font object carrying only colour and weight (everything else
    // undefined), so applying it is one call and touches nothing else.
    ITextFont* MakeStyleFont(COLORREF color, bool bold) {
        ITextFont* base = nullptr;
        ITextFont* duplicate = nullptr;
        if (!range_ || FAILED(range_->SetRange(0, 0)) || FAILED(range_->GetFont(&base)) || !base) return nullptr;
        if (SUCCEEDED(base->GetDuplicate(&duplicate)) && duplicate) {
            duplicate->Reset(tomUndefined);
            duplicate->SetForeColor(static_cast<long>(color));
            duplicate->SetBold(bold ? tomTrue : tomFalse);
        }
        base->Release();
        return duplicate;
    }

    void ApplyFont(LONG start, LONG end, ITextFont* font) {
        if (range_ && font && SUCCEEDED(range_->SetRange(start, end))) range_->SetFont(font);
    }

    void SetStyle(LONG start, LONG end, COLORREF color, bool bold) {
        if (!range_ || FAILED(range_->SetRange(start, end))) return;
        ITextFont* font = nullptr;
        if (SUCCEEDED(range_->GetFont(&font)) && font) {
            font->SetForeColor(static_cast<long>(color));
            font->SetBold(bold ? tomTrue : tomFalse);
            font->Release();
        }
    }

    void SetColor(LONG start, LONG end, COLORREF color) {
        if (!range_ || FAILED(range_->SetRange(start, end))) return;
        ITextFont* font = nullptr;
        if (SUCCEEDED(range_->GetFont(&font)) && font) {
            font->SetForeColor(static_cast<long>(color));
            font->Release();
        }
    }

    void SetBackColor(LONG start, LONG end, COLORREF color) {
        if (!range_ || FAILED(range_->SetRange(start, end))) return;
        ITextFont* font = nullptr;
        if (SUCCEEDED(range_->GetFont(&font)) && font) {
            font->SetBackColor(static_cast<long>(color));
            font->Release();
        }
    }

    void SetHidden(LONG start, LONG end, bool hidden) {
        if (!range_ || FAILED(range_->SetRange(start, end))) return;
        ITextFont* font = nullptr;
        if (SUCCEEDED(range_->GetFont(&font)) && font) {
            font->SetHidden(hidden ? tomTrue : tomFalse);
            font->Release();
        }
    }

private:
    ITextDocument* document_ = nullptr;
    ITextRange* range_ = nullptr;
};

std::wstring GetEditorRawText(HWND editor);

// The text of the document that was edited last, kept from the EN_CHANGE
// handler so folding, highlighting and bracket matching don't each copy
// megabytes out of the control again for the same keystroke.
struct EditorTextCache {
    HWND editor = nullptr;
    uint64_t hash = 0;
    std::wstring text;
} g_textCache;

void StoreEditorText(HWND editor, uint64_t hash, std::wstring&& text) {
    g_textCache.editor = editor;
    g_textCache.hash = hash;
    g_textCache.text = std::move(text);
}

// Current text of a document: the cached copy when it is provably current
// (same control, same hash as the document, same length), else a fresh one.
const std::wstring& DocumentText(const TabDocument& document, std::wstring& storage) {
    if (g_textCache.editor == document.editor && g_textCache.hash == document.textHash && document.textHash != 0) {
        GETTEXTLENGTHEX query{GTL_NUMCHARS | GTL_PRECISE, 1200};
        const LRESULT length = SendMessageW(document.editor, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&query), 0);
        if (length >= 0 && static_cast<size_t>(length) == g_textCache.text.size()) return g_textCache.text;
    }
    storage = GetEditorRawText(document.editor);
    return storage;
}

// Folds are {...} blocks spanning more than one real line. Re-detected only
// when the text changed (textHash is kept current by the EN_CHANGE handler);
// a block stays collapsed after an edit as long as its text is still hidden.
void DetectFoldRegions(TabDocument& document) {
    if (!document.editor) return;
    if (document.largeFileMode) return;
    if (document.foldTextHash != 0 && document.foldTextHash == document.textHash) return;
    const bool hadCollapsed = std::any_of(document.folds.begin(), document.folds.end(), [](const FoldRegion& region) { return region.collapsed; });
    std::wstring storage;
    const std::wstring& text = DocumentText(document, storage);
    document.folds.clear();
    document.foldTextHash = document.textHash != 0 ? document.textHash : HashText(text);
    struct Open { LONG position; int line; };
    std::vector<Open> stack;
    int line = 0;  // real line, 0-based
    for (LONG position = 0; position < static_cast<LONG>(text.size()); ++position) {
        const wchar_t character = text[static_cast<size_t>(position)];
        if (character == L'\r') {
            ++line;
        } else if (character == L'{' && (position == 0 || text[static_cast<size_t>(position - 1)] != L'\\')) {
            stack.push_back({position, line});
        } else if (character == L'}' && !stack.empty()) {
            const Open opening = stack.back();
            stack.pop_back();
            if (line > opening.line) {
                const LONG end = std::min<LONG>(static_cast<LONG>(text.size()), position + 1);
                document.folds.push_back({opening.position + 1, end, opening.line, false});
            }
        }
    }
    std::sort(document.folds.begin(), document.folds.end(), [](const FoldRegion& left, const FoldRegion& right) {
        if (left.line != right.line) return left.line < right.line;
        return left.start < right.start;
    });
    if (hadCollapsed) {
        TextDocumentRef tom(document.editor);
        ITextRange* range = nullptr;
        if (tom && SUCCEEDED(tom.document()->Range(0, 0, &range)) && range) {
            for (FoldRegion& region : document.folds) {
                ITextFont* font = nullptr;
                long hidden = tomFalse;
                if (SUCCEEDED(range->SetRange(region.start, region.end)) && SUCCEEDED(range->GetFont(&font)) && font) {
                    font->GetHidden(&hidden);
                    font->Release();
                }
                region.collapsed = hidden == tomTrue;
            }
            range->Release();
        }
    }
}

void ApplyFoldVisibility(TabDocument& document) {
    if (!document.editor) return;
    TextDocumentRef tom(document.editor);
    if (!tom) return;
    long freeze = 0;
    tom.document()->Undo(tomSuspend, nullptr);
    tom.document()->Freeze(&freeze);
    for (const FoldRegion& region : document.folds) {
        tom.SetHidden(region.start, region.end, region.collapsed);
    }
    tom.document()->Unfreeze(&freeze);
    tom.document()->Undo(tomResume, nullptr);
    InvalidateRect(document.editor, nullptr, TRUE);
    RefreshGutter();
}

void FoldCurrentBlock() {
    TabDocument* document = ActiveDocument();
    if (!document) return;
    DetectFoldRegions(*document);
    CHARRANGE selection{};
    SendMessageW(document->editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    TextDocumentRef tom(document->editor);
    const int line = tom ? static_cast<int>(tom.ParagraphIndex(selection.cpMin)) - 1
                         : static_cast<int>(SendMessageW(document->editor, EM_EXLINEFROMCHAR, 0, selection.cpMin));
    auto it = std::find_if(document->folds.begin(), document->folds.end(), [line](const FoldRegion& region) {
        return region.line == line;
    });
    if (it == document->folds.end()) {
        ShowNotice(L"当前行没有可折叠代码块");
        return;
    }
    it->collapsed = !it->collapsed;
    ApplyFoldVisibility(*document);
    g_app.stateDirty = true;
}

void UnfoldAllBlocks() {
    TabDocument* document = ActiveDocument();
    if (!document) return;
    DetectFoldRegions(*document);
    for (FoldRegion& region : document->folds) region.collapsed = false;
    ApplyFoldVisibility(*document);
    // Also text hidden by a block whose braces were edited away since.
    TextDocumentRef tom(document->editor);
    if (tom) {
        tom.document()->Undo(tomSuspend, nullptr);
        tom.SetHidden(0, static_cast<LONG>(GetEditorRawText(document->editor).size()) + 1, false);
        tom.document()->Undo(tomResume, nullptr);
    }
    g_app.stateDirty = true;
}

void ClearMultiCarets() {
    if (TabDocument* document = ActiveDocument()) {
        document->multiCarets.clear();
        document->rectangleSelection = false;
        InvalidateRect(document->editor, nullptr, FALSE);
    }
}

void AddNextLineCaret() {
    TabDocument* document = ActiveDocument();
    if (!document) return;
    CHARRANGE selection{};
    SendMessageW(document->editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    LONG caret = selection.cpMax;
    const LRESULT line = SendMessageW(document->editor, EM_EXLINEFROMCHAR, 0, caret);
    const LONG lineStart = static_cast<LONG>(SendMessageW(document->editor, EM_LINEINDEX, line, 0));
    const LONG column = std::max<LONG>(0, caret - lineStart);
    const LRESULT lineCount = SendMessageW(document->editor, EM_GETLINECOUNT, 0, 0);
    if (line + 1 >= lineCount) return;
    const LONG nextStart = static_cast<LONG>(SendMessageW(document->editor, EM_LINEINDEX, line + 1, 0));
    const LONG nextLength = static_cast<LONG>(SendMessageW(document->editor, EM_LINELENGTH, nextStart, 0));
    const LONG nextCaret = nextStart + std::min(column, nextLength);
    if (document->multiCarets.empty()) document->multiCarets.push_back({caret, caret});
    if (std::none_of(document->multiCarets.begin(), document->multiCarets.end(),
        [nextCaret](const MultiCaret& value) { return value.caret == nextCaret; })) {
        document->multiCarets.push_back({nextCaret, nextCaret});
    }
    SendMessageW(document->editor, EM_SETSEL, nextCaret, nextCaret);
    InvalidateRect(document->editor, nullptr, FALSE);
}

void CreateRectangleCarets() {
    TabDocument* document = ActiveDocument();
    if (!document) return;
    CHARRANGE selection{};
    SendMessageW(document->editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    if (selection.cpMin == selection.cpMax) {
        document->multiCarets.clear();
        document->multiCarets.push_back({selection.cpMin, selection.cpMax});
        document->rectangleSelection = true;
        InvalidateRect(document->editor, nullptr, FALSE);
        return;
    }
    const LONG firstLine = static_cast<LONG>(SendMessageW(document->editor, EM_EXLINEFROMCHAR, 0, selection.cpMin));
    const LONG lastLine = static_cast<LONG>(SendMessageW(document->editor, EM_EXLINEFROMCHAR, 0, selection.cpMax));
    const LONG firstStart = static_cast<LONG>(SendMessageW(document->editor, EM_LINEINDEX, firstLine, 0));
    const LONG lastStart = static_cast<LONG>(SendMessageW(document->editor, EM_LINEINDEX, lastLine, 0));
    const LONG leftColumn = selection.cpMin - firstStart;
    const LONG rightColumn = std::max(leftColumn, selection.cpMax - lastStart);
    document->multiCarets.clear();
    for (LONG line = firstLine; line <= lastLine; ++line) {
        const LONG start = static_cast<LONG>(SendMessageW(document->editor, EM_LINEINDEX, line, 0));
        if (start < 0) continue;
        const LONG length = static_cast<LONG>(SendMessageW(document->editor, EM_LINELENGTH, start, 0));
        const LONG a = start + std::min(leftColumn, length);
        const LONG b = start + std::min(rightColumn, length);
        document->multiCarets.push_back({a, b});
    }
    document->rectangleSelection = true;
    SendMessageW(document->editor, EM_SETSEL, selection.cpMin, selection.cpMin);
    InvalidateRect(document->editor, nullptr, FALSE);
}

// Tells the main window that the text changed. Multi-caret edits run with
// the editor's notifications muted (one per caret would re-highlight and
// re-hash each time), so this one call marks the tab modified, dirties the
// recovery snapshot and schedules re-highlighting.
void NotifyEditorTextChanged(HWND editor) {
    HWND parent = GetParent(editor);
    if (!parent) return;
    SendMessageW(parent, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(editor), EN_CHANGE), reinterpret_cast<LPARAM>(editor));
}

LONG EditorTextLength(HWND editor) {
    GETTEXTLENGTHEX query{GTL_NUMCHARS | GTL_PRECISE, 1200};
    return static_cast<LONG>(std::max<LRESULT>(0, SendMessageW(editor, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&query), 0)));
}

// Edits every caret's range, from the last one back so earlier positions stay
// valid, then moves each caret by the net change of the ranges before it.
// `removeAround` widens an empty range by one character (Backspace/Delete).
bool EditAtMultiCarets(HWND editor, const std::wstring& replacement, int removeAround) {
    TabDocument* document = ActiveDocument();
    if (!document || document->multiCarets.size() < 2) return false;
    const LONG length = EditorTextLength(editor);
    struct Edit { LONG start, end; };
    std::vector<Edit> edits;
    for (const MultiCaret& caret : document->multiCarets) {
        LONG start = std::clamp(std::min(caret.anchor, caret.caret), 0L, length);
        LONG end = std::clamp(std::max(caret.anchor, caret.caret), 0L, length);
        if (start == end && removeAround < 0) start = std::max<LONG>(0, start - 1);
        if (start == end && removeAround > 0) end = std::min(length, end + 1);
        edits.push_back({start, end});
    }
    std::sort(edits.begin(), edits.end(), [](const Edit& a, const Edit& b) { return a.start < b.start; });
    // Overlapping ranges (two carets next to each other) become one edit.
    std::vector<Edit> merged;
    for (const Edit& edit : edits) {
        if (!merged.empty() && edit.start <= merged.back().end) merged.back().end = std::max(merged.back().end, edit.end);
        else merged.push_back(edit);
    }
    {
        EditorEventMute mute(editor);
        for (auto it = merged.rbegin(); it != merged.rend(); ++it) {
            SendMessageW(editor, EM_SETSEL, it->start, it->end);
            SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(replacement.c_str()));
        }
    }
    const LONG inserted = static_cast<LONG>(replacement.size());
    std::vector<MultiCaret> carets;
    LONG shift = 0;
    for (const Edit& edit : merged) {
        const LONG position = edit.start + shift + inserted;
        carets.push_back({position, position});
        shift += inserted - (edit.end - edit.start);
    }
    document->multiCarets = std::move(carets);
    if (document->multiCarets.size() < 2) document->multiCarets.clear();
    const LONG caret = document->multiCarets.empty() ? merged.front().start + inserted : document->multiCarets.front().caret;
    SendMessageW(editor, EM_SETSEL, caret, caret);
    NotifyEditorTextChanged(editor);
    InvalidateRect(editor, nullptr, FALSE);
    return true;
}

bool ApplyMultiCaretReplacement(HWND editor, const std::wstring& replacement) {
    return EditAtMultiCarets(editor, replacement, 0);
}

bool ApplyMultiCaretBackspace(HWND editor, bool forward) {
    return EditAtMultiCarets(editor, L"", forward ? 1 : -1);
}

void DrawMultiCaretOverlay(HWND editor) {
    TabDocument* document = ActiveDocument();
    if (!document || document->editor != editor || document->multiCarets.size() < 2) return;
    HDC dc = GetDC(editor);
    if (!dc) return;
    HPEN pen = CreatePen(PS_SOLID, std::max(1, Scale(1)), CurrentPalette().accent);
    HPEN old = static_cast<HPEN>(SelectObject(dc, pen));
    for (const MultiCaret& range : document->multiCarets) {
        if (range.anchor != range.caret) continue;
        POINT point{};
        SendMessageW(editor, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&point), range.caret);
        MoveToEx(dc, point.x, point.y, nullptr);
        LineTo(dc, point.x, point.y + Scale(18));
    }
    SelectObject(dc, old);
    DeleteObject(pen);
    ReleaseDC(editor, dc);
}

void RecordMacroText(const std::wstring& text) {
    if (g_app.macroRecording && !g_app.macroPlaying && !text.empty())
        g_app.macroActions.push_back({false, 0, text});
}

void RecordMacroCommand(UINT command) {
    if (!g_app.macroRecording || g_app.macroPlaying || command == kIdMacroToggle ||
        command == kIdMacroPlay || command == kIdMacroClear) return;
    g_app.macroActions.push_back({true, command, {}});
}

void ToggleMacroRecording() {
    if (g_app.macroRecording) {
        g_app.macroRecording = false;
        ShowNotice(L"宏录制已停止，共记录 " + std::to_wstring(g_app.macroActions.size()) + L" 个动作");
    } else {
        g_app.macroActions.clear();
        g_app.macroRecording = true;
        ShowNotice(L"宏录制已开始");
    }
}

void PlayMacro() {
    if (g_app.macroActions.empty() || g_app.macroPlaying) {
        ShowNotice(L"没有可播放的宏");
        return;
    }
    g_app.macroPlaying = true;
    const std::vector<MacroAction> actions = g_app.macroActions;
    for (const MacroAction& action : actions) {
        if (action.command) SendMessageW(g_app.window, WM_COMMAND, action.commandId, 0);
        else if (HWND editor = ActiveEditor()) {
            if (!ApplyMultiCaretReplacement(editor, action.text))
                SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(action.text.c_str()));
        }
    }
    g_app.macroPlaying = false;
    ShowNotice(L"宏播放完成");
}

void SetSelectionColor(HWND editor, LONG start, LONG end, COLORREF color) {
    CHARRANGE range{start, end};
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.crTextColor = color;
    SendMessageW(editor, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
}

uint64_t HashText(std::wstring_view text) {
    uint64_t hash = 1469598103934665603ull;  // FNV-1a
    for (const wchar_t c : text) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ull;
    }
    return hash ^ text.size();
}

std::wstring GetEditorRawText(HWND editor);

// Text exactly as RichEdit stores it (one character per line break), so string
// offsets match RichEdit character positions.
std::wstring GetEditorRawText(HWND editor) {
    GETTEXTLENGTHEX lengthQuery{GTL_NUMCHARS | GTL_PRECISE, 1200};
    const LRESULT length = SendMessageW(editor, EM_GETTEXTLENGTHEX,
        reinterpret_cast<WPARAM>(&lengthQuery), 0);
    if (length <= 0) return {};
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GETTEXTEX query{};
    query.cb = static_cast<DWORD>(text.size() * sizeof(wchar_t));
    query.flags = GT_DEFAULT;
    query.codepage = 1200;
    const LRESULT copied = SendMessageW(editor, EM_GETTEXTEX,
        reinterpret_cast<WPARAM>(&query), reinterpret_cast<LPARAM>(text.data()));
    text.resize(static_cast<size_t>(std::max<LRESULT>(0, copied)));
    return text;
}

std::wstring LowerInvariantSyntax(std::wstring_view text) {
    std::wstring result(text);
    if (!result.empty()) {
        LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, text.data(), static_cast<int>(text.size()),
            result.data(), static_cast<int>(result.size()), nullptr, nullptr, 0);
    }
    return result;
}

