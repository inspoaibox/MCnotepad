#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>
#include <winreg.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "../resources/resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(linker, "\"/manifestdependency:type=\'win32\' name=\'Microsoft.Windows.Common-Controls\' version=\'6.0.0.0\' processorArchitecture=\'*\' publicKeyToken=\'6595b64144ccf1df\' language=\'*\'\"")

namespace fs = std::filesystem;

namespace {

using namespace Gdiplus;

constexpr wchar_t kWindowClass[] = L"MCnotepadWindow";
constexpr wchar_t kWindowTitle[] = L"MCnotepad";
constexpr UINT kTimerAutosave = 4101;
constexpr wchar_t kSingleInstanceMutex[] = L"Local\\MCnotepad.SingleInstance";
constexpr ULONG_PTR kCopyDataOpenFiles = 0x4D434E50;  // 'MCNP'
constexpr UINT kMessageOpenPendingFiles = WM_APP + 1;
constexpr int kToolbarHeight = 28;
constexpr int kTabHeight = 28;
constexpr int kTabCloseHitWidth = 22;
constexpr int kTabCloseIconHalf = 4;
constexpr int kStatusHeight = 22;
constexpr int kNewTabButtonWidth = 46;
constexpr int kGutterWidth = 42;

constexpr int kIdFileNew = 1001;
constexpr int kIdFileOpen = 1002;
constexpr int kIdFileSave = 1003;
constexpr int kIdFileSaveAs = 1004;
constexpr int kIdFileClose = 1005;
constexpr int kIdFileExit = 1006;
constexpr int kIdEditUndo = 1101;
constexpr int kIdEditRedo = 1102;
constexpr int kIdEditCut = 1103;
constexpr int kIdEditCopy = 1104;
constexpr int kIdEditPaste = 1105;
constexpr int kIdEditSelectAll = 1106;
constexpr int kIdHelpAbout = 1201;
constexpr int kIdNewTabButton = 1301;
constexpr int kIdToolbarNew = 1401;
constexpr int kIdToolbarOpen = 1402;
constexpr int kIdToolbarSave = 1403;
constexpr int kIdToolbarSaveAs = 1404;
constexpr int kIdToolbarClose = 1405;
constexpr int kIdToolbarUndo = 1406;
constexpr int kIdToolbarRedo = 1407;
constexpr int kIdToolbarFind = 1408;
constexpr int kIdToolbarReplace = 1409;
constexpr int kIdToolbarGoto = 1410;
constexpr int kIdToolbarFormat = 1411;
constexpr int kIdToolbarRun = 1412;
constexpr int kIdToolbarEncoding = 1413;
constexpr int kIdToolbarLineNumbers = 1414;
constexpr int kIdToolbarWrap = 1415;
constexpr int kIdToolbarTheme = 1416;
constexpr int kIdToolbarSettings = 1417;
constexpr int kIdThemeDark = 1501;
constexpr int kIdThemeLight = 1502;
constexpr int kIdThemeGreen = 1503;
constexpr int kIdThemeBlue = 1504;
constexpr int kIdSettingsFont = 1701;
constexpr int kIdSettingsSize = 1702;
constexpr int kIdSettingsTheme = 1703;
constexpr int kIdSettingsLineNumbers = 1704;
constexpr int kIdSettingsWrap = 1705;
constexpr int kIdSettingsStartup = 1706;
constexpr int kIdSettingsCloseBehavior = 1707;
constexpr int kIdSettingsApply = 1708;
constexpr int kIdSettingsCancel = 1709;
constexpr int kIdSidebarClose = 1801;

enum class ThemeKind { Dark = 0, Light = 1, Green = 2, Blue = 3 };
enum class CloseBehavior { Prompt = 0, Direct = 1, Save = 2 };

struct TabDocument {
    HWND editor = nullptr;
    std::wstring path;
    std::wstring title;
    fs::path snapshotPath;
    bool untitled = true;
    bool modified = false;
    bool recoveryDirty = true;
    int64_t caret = 0;
    int64_t anchor = 0;
    int firstVisibleLine = 0;
    int horizontalScroll = 0;
};

struct AppState {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    HWND tabs = nullptr;
    HWND newTabButton = nullptr;
    HWND statusBar = nullptr;
    HWND toolbar = nullptr;
    HWND gutter = nullptr;
    HWND sidebar = nullptr;
    HWND languageLabel = nullptr;
    HWND languageCombo = nullptr;
    HWND encodingLabel = nullptr;
    HWND encodingCombo = nullptr;
    HWND lineEndingLabel = nullptr;
    HWND lineEndingCombo = nullptr;
    HWND sidebarTitle = nullptr;
    HWND sidebarClose = nullptr;
    std::vector<HWND> toolbarButtons;
    HFONT editorFont = nullptr;
    HFONT toolbarIconFont = nullptr;
    HFONT uiFont = nullptr;
    std::wstring toolbarIconFontPath;
    ULONG_PTR gdiplusToken = 0;
    std::vector<std::unique_ptr<Bitmap>> toolbarIconImages;
    HFONT uiBoldFont = nullptr;
    HBRUSH editorBrush = nullptr;
    HBRUSH sidebarBrush = nullptr;
    HBRUSH toolbarBrush = nullptr;
    HBRUSH windowBrush = nullptr;
    HBRUSH controlBrush = nullptr;
    HBRUSH statusBrush = nullptr;
    HMENU themeMenu = nullptr;
    WNDPROC oldTabProc = nullptr;
    WNDPROC oldSidebarProc = nullptr;
    WNDPROC oldStatusProc = nullptr;
    bool darkTheme = true;
    ThemeKind theme = ThemeKind::Dark;
    int fontSize = 13;
    std::wstring fontFamily = L"Consolas";
    bool showLineNumbers = true;
    bool wordWrap = false;
    bool sidebarVisible = false;
    UINT findReplaceMessage = 0;
    HWND findDialog = nullptr;
    FINDREPLACEW findReplace{};
    wchar_t findBuffer[256]{};
    wchar_t replaceBuffer[256]{};
    std::vector<TabDocument> documents;
    std::vector<std::wstring> pendingOpenFiles;
    int activeIndex = -1;
    int nextUntitledNumber = 1;
    bool loading = false;
    bool closing = false;
    bool stateDirty = true;
    bool previousRunWasUnclean = false;
    bool launchAtStartup = false;
    CloseBehavior closeBehavior = CloseBehavior::Prompt;
    int languageIndex = 1;
    int encodingIndex = 0;
    int lineEndingIndex = 0;
    HWND settingsWindow = nullptr;
    HWND settingsFontCombo = nullptr;
    HWND settingsSizeCombo = nullptr;
    HWND settingsThemeCombo = nullptr;
    HWND settingsLineNumbersCheck = nullptr;
    HWND settingsWrapCheck = nullptr;
    HWND settingsStartupCheck = nullptr;
    HWND settingsCloseBehaviorCombo = nullptr;
    fs::path dataDirectory;
    fs::path recoveryDirectory;
    fs::path sessionPath;
    fs::path runningMarkerPath;
};

AppState g_app;

// RichEdit controls normally handle the standard edit shortcuts themselves,
// but accelerator translation can be bypassed when focus is inside a child
// control. Keep a small per-editor subclass so the shortcuts always reach the
// active editor, regardless of which child window currently owns focus.
std::unordered_map<HWND, WNDPROC> g_editorProcs;

std::wstring GetWindowTextValue(HWND window);
void ApplyEditorTheme(HWND editor);
void ApplySimpleSyntaxHighlight(HWND editor);
void CloseDocument(int index);

LRESULT CALLBACK EditorProc(HWND editor, UINT message, WPARAM wParam, LPARAM lParam);
void ApplyEditorFont();
void SetMenuState();

struct ThemePalette {
    COLORREF windowBackground;
    COLORREF panelBackground;
    COLORREF panelBorder;
    COLORREF toolbarBackground;
    COLORREF toolbarButtonBackground;
    COLORREF toolbarButtonPressed;
    COLORREF toolbarText;
    COLORREF toolbarIcon;
    COLORREF toolbarIconDisabled;
    COLORREF tabBarBackground;
    COLORREF tabBackground;
    COLORREF tabActiveBackground;
    COLORREF tabText;
    COLORREF tabActiveText;
    COLORREF tabClose;
    COLORREF gutterBackground;
    COLORREF gutterText;
    COLORREF editorBackground;
    COLORREF editorText;
    COLORREF statusBackground;
    COLORREF statusText;
    COLORREF controlBackground;
    COLORREF controlText;
    COLORREF controlBorder;
    COLORREF controlHover;
    COLORREF accent;
    COLORREF accentSoft;
    COLORREF syntaxTag;
    COLORREF syntaxString;
    COLORREF syntaxComment;
};

const ThemePalette& ThemePaletteFor(ThemeKind theme) {
    static constexpr ThemePalette palettes[] = {
        {
            RGB(24, 24, 27), RGB(31, 32, 36), RGB(63, 64, 70),
            RGB(28, 29, 33), RGB(38, 39, 44), RGB(52, 53, 59),
            RGB(232, 232, 235), RGB(184, 190, 201), RGB(118, 120, 128),
            RGB(27, 28, 31), RGB(39, 40, 45), RGB(52, 53, 59),
            RGB(170, 172, 181), RGB(245, 245, 247), RGB(220, 221, 225),
            RGB(30, 31, 34), RGB(145, 147, 156), RGB(22, 23, 26),
            RGB(235, 235, 238), RGB(38, 39, 43), RGB(196, 198, 205),
            RGB(43, 44, 49), RGB(232, 233, 237), RGB(78, 79, 87),
            RGB(72, 73, 82), RGB(184, 190, 201), RGB(100, 101, 112),
            RGB(255, 124, 157), RGB(232, 195, 106), RGB(140, 165, 164)
        },
        {
            RGB(244, 247, 251), RGB(250, 252, 255), RGB(216, 224, 235),
            RGB(248, 250, 253), RGB(243, 246, 251), RGB(229, 238, 250),
            RGB(38, 50, 68), RGB(61, 127, 214), RGB(166, 177, 192),
            RGB(237, 241, 247), RGB(245, 248, 252), RGB(255, 255, 255),
            RGB(102, 117, 138), RGB(31, 45, 64), RGB(91, 108, 132),
            RGB(244, 247, 251), RGB(133, 147, 167), RGB(255, 255, 255),
            RGB(37, 49, 67), RGB(246, 248, 251), RGB(94, 109, 128),
            RGB(255, 255, 255), RGB(37, 49, 67), RGB(202, 211, 223),
            RGB(234, 242, 252), RGB(61, 127, 214), RGB(220, 234, 250),
            RGB(178, 77, 114), RGB(141, 100, 0), RGB(91, 123, 112)
        },
        {
            RGB(234, 242, 231), RGB(246, 250, 243), RGB(201, 218, 195),
            RGB(239, 247, 236), RGB(231, 242, 228), RGB(215, 231, 209),
            RGB(48, 73, 54), RGB(88, 151, 99), RGB(149, 169, 145),
            RGB(226, 238, 224), RGB(239, 247, 236), RGB(255, 255, 255),
            RGB(103, 122, 105), RGB(48, 73, 54), RGB(102, 128, 106),
            RGB(239, 246, 237), RGB(126, 150, 127), RGB(250, 253, 249),
            RGB(46, 64, 50), RGB(239, 246, 237), RGB(91, 116, 94),
            RGB(255, 255, 255), RGB(46, 64, 50), RGB(200, 216, 195),
            RGB(231, 242, 228), RGB(94, 155, 103), RGB(218, 235, 214),
            RGB(165, 78, 104), RGB(138, 108, 25), RGB(90, 132, 100)
        },
        {
            RGB(15, 26, 43), RGB(22, 40, 64), RGB(44, 70, 104),
            RGB(14, 29, 49), RGB(24, 48, 79), RGB(32, 64, 100),
            RGB(225, 236, 250), RGB(98, 164, 246), RGB(113, 137, 165),
            RGB(12, 25, 43), RGB(24, 48, 79), RGB(33, 67, 103),
            RGB(145, 170, 201), RGB(242, 247, 253), RGB(183, 203, 228),
            RGB(18, 36, 58), RGB(114, 141, 171), RGB(15, 27, 45),
            RGB(226, 237, 250), RGB(23, 42, 67), RGB(175, 194, 218),
            RGB(27, 51, 83), RGB(225, 236, 250), RGB(51, 84, 123),
            RGB(34, 70, 103), RGB(98, 164, 246), RGB(45, 94, 146),
            RGB(255, 137, 169), RGB(240, 208, 121), RGB(142, 173, 158)
        }
    };
    return palettes[std::clamp(static_cast<int>(theme), 0, 3)];
}

const ThemePalette& CurrentPalette() {
    return ThemePaletteFor(g_app.theme);
}

wchar_t ToolbarIconGlyph(int id) {
    switch (id) {
    case kIdToolbarNew: return L'\xF067';
    case kIdToolbarOpen: return L'\xF07C';
    case kIdToolbarSave: return L'\xF0C7';
    case kIdToolbarSaveAs: return L'\xF56F';
    case kIdToolbarClose: return L'\xF00D';
    case kIdToolbarUndo: return L'\xF0E2';
    case kIdToolbarRedo: return L'\xF01E';
    case kIdToolbarFind: return L'\xF002';
    case kIdToolbarReplace: return L'\xF021';
    case kIdToolbarGoto: return L'\xF061';
    case kIdToolbarFormat: return L'\xF03A';
    case kIdToolbarRun: return L'\xF04B';
    case kIdToolbarEncoding: return L'\xF121';
    case kIdToolbarLineNumbers: return L'\xF0CB';
    case kIdToolbarWrap: return L'\xF0B5';
    case kIdToolbarTheme: return L'\xF53F';
    default: return 0;
    }
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

bool FontSupportsGlyph(HFONT font, wchar_t glyph) {
    if (!font || glyph == 0) return false;
    HDC dc = GetDC(nullptr);
    if (!dc) return false;
    HFONT previous = static_cast<HFONT>(SelectObject(dc, font));
    GLYPHMETRICS metrics{};
    const DWORD result = GetGlyphOutlineW(dc, glyph, GGO_METRICS, &metrics, 0, nullptr, nullptr);
    SelectObject(dc, previous);
    ReleaseDC(nullptr, dc);
    return result != GDI_ERROR;
}

bool LoadToolbarIconFont() {
    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, std::size(modulePath));
    if (length == 0 || length >= std::size(modulePath)) return false;
    fs::path fontPath = fs::path(modulePath).parent_path() / L"resources" / L"fa-solid-900.ttf";
    if (!fs::exists(fontPath)) return false;
    if (AddFontResourceExW(fontPath.c_str(), FR_PRIVATE, nullptr) == 0) return false;
    g_app.toolbarIconFontPath = fontPath.wstring();
    g_app.toolbarIconFont = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Font Awesome 6 Free");
    if (!FontSupportsGlyph(g_app.toolbarIconFont, ToolbarIconGlyph(kIdToolbarNew))) {
        DeleteObject(g_app.toolbarIconFont);
        g_app.toolbarIconFont = nullptr;
        RemoveFontResourceExW(g_app.toolbarIconFontPath.c_str(), FR_PRIVATE, nullptr);
        g_app.toolbarIconFontPath.clear();
        return false;
    }
    return true;
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

void ApplyThemeToEditors(bool rehighlight = true) {
    const ThemePalette& palette = CurrentPalette();
    g_app.darkTheme = g_app.theme != ThemeKind::Light;
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
    for (const TabDocument& document : g_app.documents) {
        ApplyEditorTheme(document.editor);
        if (rehighlight) ApplySimpleSyntaxHighlight(document.editor);
    }
    if (g_app.tabs) {
        InvalidateRect(g_app.tabs, nullptr, TRUE);
    }
    if (g_app.statusBar) InvalidateRect(g_app.statusBar, nullptr, TRUE);
    if (g_app.gutter) InvalidateRect(g_app.gutter, nullptr, TRUE);
    if (g_app.toolbar) InvalidateRect(g_app.toolbar, nullptr, TRUE);
    for (HWND button : g_app.toolbarButtons) {
        if (button) InvalidateRect(button, nullptr, TRUE);
    }
    if (g_app.newTabButton) InvalidateRect(g_app.newTabButton, nullptr, TRUE);
    if (g_app.sidebarClose) InvalidateRect(g_app.sidebarClose, nullptr, TRUE);
    if (g_app.settingsThemeCombo) SendMessageW(g_app.settingsThemeCombo, CB_SETCURSEL, static_cast<int>(g_app.theme), 0);
    if (g_app.sidebar) {
        InvalidateRect(g_app.sidebar, nullptr, TRUE);
        for (int id : {kIdThemeDark, kIdThemeLight, kIdThemeGreen, kIdThemeBlue}) {
            if (HWND button = GetDlgItem(g_app.sidebar, id)) InvalidateRect(button, nullptr, TRUE);
        }
    }
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

void ShowThemeMenu(HWND anchor) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, kIdThemeDark, L"深色");
    AppendMenuW(menu, MF_STRING, kIdThemeLight, L"浅色");
    AppendMenuW(menu, MF_STRING, kIdThemeGreen, L"护眼绿");
    AppendMenuW(menu, MF_STRING, kIdThemeBlue, L"暗夜蓝");
    CheckMenuRadioItem(menu, kIdThemeDark, kIdThemeBlue,
        kIdThemeDark + static_cast<int>(g_app.theme), MF_BYCOMMAND);

    RECT rect{};
    GetWindowRect(anchor ? anchor : g_app.toolbar, &rect);
    const int command = TrackPopupMenuEx(menu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
        rect.left, rect.bottom, g_app.window, nullptr);
    DestroyMenu(menu);
    if (command >= kIdThemeDark && command <= kIdThemeBlue) {
        SetThemeKind(static_cast<ThemeKind>(command - kIdThemeDark));
        SetMenuState();
    }
}


// RichEdit only sends EN_CHANGE / EN_VSCROLL when the event mask asks for it.
// Formatting changes (theme, highlight, font) must not mark a document as
// modified, so they temporarily mute notifications with this guard.
constexpr LPARAM kEditorEventMask = ENM_CHANGE | ENM_SCROLL;

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
    const COLORREF background = EditorBackground();
    const COLORREF foreground = EditorForeground();
    SendMessageW(editor, EM_SETBKGNDCOLOR, 0, background);
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR | CFM_BACKCOLOR;
    format.crTextColor = foreground;
    format.crBackColor = background;
    SendMessageW(editor, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
    InvalidateRect(editor, nullptr, TRUE);
}

void RefreshGutter() {
    if (g_app.gutter) InvalidateRect(g_app.gutter, nullptr, TRUE);
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

void ApplySimpleSyntaxHighlight(HWND editor) {
    if (!editor) return;
    const std::wstring text = GetWindowTextValue(editor);
    if (text.empty()) return;
    EditorEventMute mute(editor);
    CHARRANGE saved{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&saved));
    SetSelectionColor(editor, 0, static_cast<LONG>(text.size()), EditorForeground());
    for (size_t i = 0; i < text.size();) {
        if (text[i] == L'<') {
            const size_t end = text.find(L'>', i + 1);
            if (end == std::wstring::npos) break;
            SetSelectionColor(editor, static_cast<LONG>(i), static_cast<LONG>(end + 1), CurrentPalette().syntaxTag);
            i = end + 1;
        } else if (text[i] == L'"' || text[i] == L'\'') {
            const wchar_t quote = text[i];
            const size_t end = text.find(quote, i + 1);
            if (end == std::wstring::npos) break;
            SetSelectionColor(editor, static_cast<LONG>(i), static_cast<LONG>(end + 1), CurrentPalette().syntaxString);
            i = end + 1;
        } else if (i + 3 < text.size() && text.compare(i, 4, L"<!--") == 0) {
            const size_t end = text.find(L"-->", i + 4);
            const size_t finish = end == std::wstring::npos ? text.size() : end + 3;
            SetSelectionColor(editor, static_cast<LONG>(i), static_cast<LONG>(finish), CurrentPalette().syntaxComment);
            i = finish;
        } else {
            ++i;
        }
    }
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&saved));
}

LRESULT CALLBACK StatusProc(HWND status, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(status, &paint);
        RECT client{};
        GetClientRect(status, &client);
        const ThemePalette& palette = CurrentPalette();
        HBRUSH background = CreateSolidBrush(palette.statusBackground);
        FillRect(dc, &client, background);
        DeleteObject(background);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, palette.statusText);
        HFONT font = g_app.uiFont ? g_app.uiFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
        int parts[16]{};
        const int count = std::clamp(static_cast<int>(SendMessageW(status, SB_GETPARTS, 16,
            reinterpret_cast<LPARAM>(parts))), 0, 16);
        int left = 0;
        for (int index = 0; index < count; ++index) {
            const int right = parts[index] < 0 ? static_cast<int>(client.right)
                : std::min(static_cast<int>(client.right), parts[index]);
            wchar_t text[256]{};
            SendMessageW(status, SB_GETTEXTW, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(text));
            RECT textRect{left + 8, 0, std::max(left + 8, right - 8), client.bottom};
            DrawTextW(dc, text, -1, &textRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            if (index + 1 < count) {
                HPEN separator = CreatePen(PS_SOLID, 1, palette.panelBorder);
                HPEN oldPen = static_cast<HPEN>(SelectObject(dc, separator));
                MoveToEx(dc, right, 4, nullptr);
                LineTo(dc, right, std::max(4, static_cast<int>(client.bottom) - 4));
                SelectObject(dc, oldPen);
                DeleteObject(separator);
            }
            left = right;
        }
        SelectObject(dc, oldFont);
        EndPaint(status, &paint);
        return 0;
    }
    return g_app.oldStatusProc
        ? CallWindowProcW(g_app.oldStatusProc, status, message, wParam, lParam)
        : DefWindowProcW(status, message, wParam, lParam);
}

LRESULT CALLBACK GutterProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        HBRUSH brush = CreateSolidBrush(GutterBackground());
        FillRect(dc, &client, brush);
        DeleteObject(brush);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, GutterForeground());
        HFONT font = g_app.editorFont ? g_app.editorFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HFONT previous = static_cast<HFONT>(SelectObject(dc, font));
        int first = 0;
        if (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size())) {
            HWND editor = g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor;
            first = static_cast<int>(SendMessageW(editor, EM_GETFIRSTVISIBLELINE, 0, 0));
        }
        const int lineHeight = 20;
        const int firstLine = std::max(0, first);
        for (int row = 0; row * lineHeight < client.bottom + lineHeight; ++row) {
            wchar_t number[32]{};
            swprintf_s(number, L"%d", firstLine + row + 1);
            RECT lineRect{0, row * lineHeight + 3, client.right - 10, row * lineHeight + lineHeight};
            DrawTextW(dc, number, -1, &lineRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        }
        SelectObject(dc, previous);
        EndPaint(window, &paint);
        return 0;
    }
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

void DrawToolbarIcon(HDC dc, const RECT& bounds, int id, COLORREF color) {
    const int cx = (bounds.left + bounds.right) / 2;
    const int cy = (bounds.top + bounds.bottom) / 2;
    const int savedDc = SaveDC(dc);
    SetGraphicsMode(dc, GM_ADVANCED);
    XFORM transform{};
    transform.eM11 = 0.62f;
    transform.eM22 = 0.62f;
    transform.eDx = static_cast<FLOAT>(cx) * (1.0f - transform.eM11);
    transform.eDy = static_cast<FLOAT>(cy) * (1.0f - transform.eM22);
    SetWorldTransform(dc, &transform);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH hollow = static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, hollow));
    switch (id) {
    case kIdToolbarNew:
        MoveToEx(dc, cx - 10, cy, nullptr); LineTo(dc, cx + 10, cy);
        MoveToEx(dc, cx, cy - 10, nullptr); LineTo(dc, cx, cy + 10); break;
    case kIdToolbarOpen:
        MoveToEx(dc, cx - 12, cy - 4, nullptr); LineTo(dc, cx - 6, cy - 4);
        LineTo(dc, cx - 2, cy); LineTo(dc, cx + 12, cy);
        LineTo(dc, cx + 9, cy + 10); LineTo(dc, cx - 12, cy + 10); LineTo(dc, cx - 12, cy - 4);
        break;
    case kIdToolbarSave:
    case kIdToolbarSaveAs:
        Rectangle(dc, cx - 11, cy - 11, cx + 11, cy + 11);
        MoveToEx(dc, cx - 6, cy - 10, nullptr); LineTo(dc, cx - 6, cy - 2);
        LineTo(dc, cx + 6, cy - 2); LineTo(dc, cx + 6, cy - 10);
        Rectangle(dc, cx - 6, cy + 2, cx + 6, cy + 10); break;
    case kIdToolbarClose:
        MoveToEx(dc, cx - 8, cy - 8, nullptr); LineTo(dc, cx + 8, cy + 8);
        MoveToEx(dc, cx + 8, cy - 8, nullptr); LineTo(dc, cx - 8, cy + 8); break;
    case kIdToolbarUndo:
        Arc(dc, cx - 11, cy - 10, cx + 12, cy + 12, cx - 11, cy, cx - 2, cy - 10);
        MoveToEx(dc, cx - 11, cy, nullptr); LineTo(dc, cx - 2, cy - 6);
        MoveToEx(dc, cx - 11, cy, nullptr); LineTo(dc, cx - 2, cy + 6); break;
    case kIdToolbarRedo:
        Arc(dc, cx - 12, cy - 10, cx + 11, cy + 12, cx + 11, cy, cx + 2, cy - 10);
        MoveToEx(dc, cx + 11, cy, nullptr); LineTo(dc, cx + 2, cy - 6);
        MoveToEx(dc, cx + 11, cy, nullptr); LineTo(dc, cx + 2, cy + 6); break;
    case kIdToolbarFind:
        Ellipse(dc, cx - 10, cy - 10, cx + 5, cy + 5);
        MoveToEx(dc, cx + 1, cy + 1, nullptr); LineTo(dc, cx + 11, cy + 11); break;
    case kIdToolbarReplace:
        MoveToEx(dc, cx - 11, cy - 5, nullptr); LineTo(dc, cx + 8, cy - 5);
        MoveToEx(dc, cx + 8, cy - 5, nullptr); LineTo(dc, cx + 3, cy - 10);
        MoveToEx(dc, cx + 8, cy - 5, nullptr); LineTo(dc, cx + 3, cy);
        MoveToEx(dc, cx + 11, cy + 5, nullptr); LineTo(dc, cx - 8, cy + 5);
        MoveToEx(dc, cx - 8, cy + 5, nullptr); LineTo(dc, cx - 3, cy);
        MoveToEx(dc, cx - 8, cy + 5, nullptr); LineTo(dc, cx - 3, cy + 10); break;
    case kIdToolbarGoto:
        MoveToEx(dc, cx - 10, cy, nullptr); LineTo(dc, cx + 8, cy);
        MoveToEx(dc, cx + 8, cy, nullptr); LineTo(dc, cx + 1, cy - 7);
        MoveToEx(dc, cx + 8, cy, nullptr); LineTo(dc, cx + 1, cy + 7); break;
    case kIdToolbarFormat:
        MoveToEx(dc, cx - 9, cy - 8, nullptr); LineTo(dc, cx + 10, cy - 8);
        MoveToEx(dc, cx - 9, cy, nullptr); LineTo(dc, cx + 10, cy);
        MoveToEx(dc, cx - 9, cy + 8, nullptr); LineTo(dc, cx + 10, cy + 8); break;
    case kIdToolbarRun: {
        POINT triangle[3]{{cx - 5, cy - 11}, {cx + 10, cy}, {cx - 5, cy + 11}};
        Polygon(dc, triangle, 3);
        break;
    }
    case kIdToolbarEncoding:
        Rectangle(dc, cx - 10, cy - 10, cx + 10, cy + 10);
        MoveToEx(dc, cx - 6, cy - 4, nullptr); LineTo(dc, cx + 6, cy - 4);
        MoveToEx(dc, cx - 6, cy + 2, nullptr); LineTo(dc, cx + 6, cy + 2); break;
    case kIdToolbarLineNumbers:
        for (int row = -1; row <= 1; ++row) {
            MoveToEx(dc, cx - 10, cy + row * 7, nullptr); LineTo(dc, cx - 5, cy + row * 7);
            MoveToEx(dc, cx, cy + row * 7, nullptr); LineTo(dc, cx + 10, cy + row * 7);
        }
        break;
    case kIdToolbarWrap:
        MoveToEx(dc, cx - 11, cy - 6, nullptr); LineTo(dc, cx + 9, cy - 6);
        MoveToEx(dc, cx + 9, cy - 6, nullptr); LineTo(dc, cx + 4, cy - 11);
        MoveToEx(dc, cx + 9, cy - 6, nullptr); LineTo(dc, cx + 4, cy - 1);
        MoveToEx(dc, cx + 11, cy + 6, nullptr); LineTo(dc, cx - 9, cy + 6);
        MoveToEx(dc, cx - 9, cy + 6, nullptr); LineTo(dc, cx - 4, cy + 1);
        MoveToEx(dc, cx - 9, cy + 6, nullptr); LineTo(dc, cx - 4, cy + 11); break;
    case kIdToolbarTheme:
        Ellipse(dc, cx - 11, cy - 11, cx + 11, cy + 11);
        MoveToEx(dc, cx, cy - 10, nullptr); LineTo(dc, cx, cy + 10);
        MoveToEx(dc, cx, cy, nullptr); LineTo(dc, cx + 9, cy + 5); break;
    case kIdToolbarSettings:
        Ellipse(dc, cx - 7, cy - 7, cx + 7, cy + 7);
        for (int angle = 0; angle < 8; ++angle) {
            const double radians = angle * 3.141592653589793 / 4.0;
            const int x1 = cx + static_cast<int>(std::cos(radians) * 9);
            const int y1 = cy + static_cast<int>(std::sin(radians) * 9);
            const int x2 = cx + static_cast<int>(std::cos(radians) * 12);
            const int y2 = cy + static_cast<int>(std::sin(radians) * 12);
            MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2);
        }
        break;
    default: break;
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    RestoreDC(dc, savedDc);
}

void DrawToolbarItem(const DRAWITEMSTRUCT& item) {
    const int id = static_cast<int>(item.CtlID);
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool hot = (item.itemState & ODS_HOTLIGHT) != 0;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const ThemePalette& palette = CurrentPalette();
    RECT rect = item.rcItem;
    HBRUSH baseBrush = CreateSolidBrush(palette.toolbarBackground);
    FillRect(item.hDC, &rect, baseBrush);
    DeleteObject(baseBrush);
    if (hot || pressed) {
        RECT surface = rect;
        InflateRect(&surface, -1, -1);
        HBRUSH brush = CreateSolidBrush(ToolbarButtonBackground(pressed));
        FillRect(item.hDC, &surface, brush);
        DeleteObject(brush);
    }

    // Thin group dividers keep the row readable without adding large gaps.
    if (id == kIdToolbarNew || id == kIdToolbarUndo || id == kIdToolbarFind ||
        id == kIdToolbarFormat || id == kIdToolbarEncoding || id == kIdToolbarTheme) {
        HPEN separator = CreatePen(PS_SOLID, 1, palette.panelBorder);
        HPEN previousPen = static_cast<HPEN>(SelectObject(item.hDC, separator));
        MoveToEx(item.hDC, rect.left, rect.top + 5, nullptr);
        LineTo(item.hDC, rect.left, rect.bottom - 5);
        SelectObject(item.hDC, previousPen);
        DeleteObject(separator);
    }

    SetBkMode(item.hDC, TRANSPARENT);
    if (id == kIdNewTabButton || id == kIdSidebarClose) {
        HPEN pen = CreatePen(PS_SOLID, 1, disabled ? palette.toolbarIconDisabled : palette.toolbarIcon);
        HPEN previousPen = static_cast<HPEN>(SelectObject(item.hDC, pen));
        const int cx = (rect.left + rect.right) / 2;
        const int cy = (rect.top + rect.bottom) / 2;
        if (id == kIdNewTabButton) {
            MoveToEx(item.hDC, cx - 7, cy, nullptr); LineTo(item.hDC, cx + 7, cy);
            MoveToEx(item.hDC, cx, cy - 7, nullptr); LineTo(item.hDC, cx, cy + 7);
        } else {
            MoveToEx(item.hDC, cx - 5, cy - 5, nullptr); LineTo(item.hDC, cx + 5, cy + 5);
            MoveToEx(item.hDC, cx + 5, cy - 5, nullptr); LineTo(item.hDC, cx - 5, cy + 5);
        }
        SelectObject(item.hDC, previousPen);
        DeleteObject(pen);
        return;
    }

    const COLORREF iconColor = disabled ? palette.toolbarIconDisabled : palette.toolbarIcon;
    Bitmap* image = ToolbarIconImage(id);
    if (image != nullptr && g_app.gdiplusToken != 0) {
        Graphics graphics(item.hDC);
        graphics.SetSmoothingMode(SmoothingModeHighQuality);
        ImageAttributes attributes;
        const ColorMatrix colorMatrix = {
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            static_cast<REAL>(GetRValue(iconColor)) / 255.0f,
            static_cast<REAL>(GetGValue(iconColor)) / 255.0f,
            static_cast<REAL>(GetBValue(iconColor)) / 255.0f,
            1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 1.0f
        };
        attributes.SetColorMatrix(&colorMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
        const int size = 18;
        const int x = (item.rcItem.left + item.rcItem.right - size) / 2;
        const int y = (item.rcItem.top + item.rcItem.bottom - size) / 2;
        graphics.DrawImage(image, Rect(x, y, size, size), 0, 0,
            static_cast<INT>(image->GetWidth()), static_cast<INT>(image->GetHeight()),
            UnitPixel, &attributes);
    } else {
        RECT iconRect = item.rcItem;
        InflateRect(&iconRect, -2, -2);
        DrawToolbarIcon(item.hDC, iconRect, id, iconColor);
    }
}

void DrawThemeItem(const DRAWITEMSTRUCT& item) {
    const int id = static_cast<int>(item.CtlID);
    const ThemeKind kind = id == kIdThemeLight ? ThemeKind::Light
        : id == kIdThemeGreen ? ThemeKind::Green
        : id == kIdThemeBlue ? ThemeKind::Blue : ThemeKind::Dark;
    const ThemePalette& theme = ThemePaletteFor(kind);
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool active = g_app.theme == kind;
    RECT rect = item.rcItem;
    InflateRect(&rect, -2, -2);
    COLORREF background = theme.panelBackground;
    if (pressed) background = theme.controlHover;
    HPEN pen = CreatePen(PS_SOLID, active ? 2 : 1, active ? theme.accent : theme.panelBorder);
    HBRUSH brush = CreateSolidBrush(background);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(item.hDC, brush));
    HPEN oldPen = static_cast<HPEN>(SelectObject(item.hDC, pen));
    RoundRect(item.hDC, rect.left, rect.top, rect.right, rect.bottom, 8, 8);
    SelectObject(item.hDC, oldBrush);
    SelectObject(item.hDC, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    RECT marker{rect.left + 8, rect.top + 8, rect.left + 24, rect.top + 24};
    HBRUSH markerBrush = CreateSolidBrush(theme.editorBackground);
    FillRect(item.hDC, &marker, markerBrush);
    DeleteObject(markerBrush);
    HBRUSH markerBorder = CreateSolidBrush(theme.accent);
    FrameRect(item.hDC, &marker, markerBorder);
    DeleteObject(markerBorder);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, theme.controlText);
    HFONT font = g_app.uiFont ? g_app.uiFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HFONT oldFont = static_cast<HFONT>(SelectObject(item.hDC, font));
    RECT textRect = rect;
    textRect.left += 30;
    DrawTextW(item.hDC, ToolbarLabel(id).c_str(), -1, &textRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (active) {
        HBRUSH checkBrush = CreateSolidBrush(theme.accent);
        RECT check{rect.right - 20, rect.top + 8, rect.right - 8, rect.top + 20};
        FillRect(item.hDC, &check, checkBrush);
        DeleteObject(checkBrush);
        HPEN checkPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
        HPEN oldCheckPen = static_cast<HPEN>(SelectObject(item.hDC, checkPen));
        MoveToEx(item.hDC, check.left + 3, check.top + 6, nullptr);
        LineTo(item.hDC, check.left + 5, check.bottom - 3);
        LineTo(item.hDC, check.right - 3, check.top + 3);
        SelectObject(item.hDC, oldCheckPen);
        DeleteObject(checkPen);
    }
    SelectObject(item.hDC, oldFont);
}

void DrawTabItem(const DRAWITEMSTRUCT& item) {
    if (!g_app.tabs || item.itemID < 0) return;
    const ThemePalette& palette = CurrentPalette();
    RECT rect = item.rcItem;
    const bool active = g_app.activeIndex == static_cast<int>(item.itemID);
    HBRUSH background = CreateSolidBrush(active ? palette.tabActiveBackground : palette.tabBackground);
    FillRect(item.hDC, &rect, background);
    DeleteObject(background);
    if (active) {
        HBRUSH accent = CreateSolidBrush(palette.accent);
        RECT strip{rect.left, rect.bottom - 3, rect.right, rect.bottom};
        FillRect(item.hDC, &strip, accent);
        DeleteObject(accent);
    }
    wchar_t title[256]{};
    TCITEMW tab{};
    tab.mask = TCIF_TEXT;
    tab.pszText = title;
    tab.cchTextMax = static_cast<int>(std::size(title));
    TabCtrl_GetItem(g_app.tabs, item.itemID, &tab);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, active ? palette.tabActiveText : palette.tabText);
    HFONT font = g_app.uiFont ? g_app.uiFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HFONT oldFont = static_cast<HFONT>(SelectObject(item.hDC, font));
    RECT textRect = rect;
    textRect.left += 16;
    textRect.right -= 24;
    DrawTextW(item.hDC, title, -1, &textRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    HPEN pen = CreatePen(PS_SOLID, 1, active ? palette.tabClose : palette.gutterText);
    HPEN oldPen = static_cast<HPEN>(SelectObject(item.hDC, pen));
    const int cx = rect.right - 12;
    const int cy = (rect.top + rect.bottom) / 2;
    MoveToEx(item.hDC, cx - kTabCloseIconHalf, cy - kTabCloseIconHalf, nullptr);
    LineTo(item.hDC, cx + kTabCloseIconHalf, cy + kTabCloseIconHalf);
    MoveToEx(item.hDC, cx + kTabCloseIconHalf, cy - kTabCloseIconHalf, nullptr);
    LineTo(item.hDC, cx - kTabCloseIconHalf, cy + kTabCloseIconHalf);
    SelectObject(item.hDC, oldPen);
    DeleteObject(pen);
    SelectObject(item.hDC, oldFont);
}

void DrawComboItem(const DRAWITEMSTRUCT& item) {
    const ThemePalette& palette = CurrentPalette();
    RECT rect = item.rcItem;
    HBRUSH brush = CreateSolidBrush(palette.controlBackground);
    FillRect(item.hDC, &rect, brush);
    DeleteObject(brush);
    if (item.itemState & ODS_FOCUS) {
        HPEN pen = CreatePen(PS_SOLID, 1, palette.accent);
        HPEN oldPen = static_cast<HPEN>(SelectObject(item.hDC, pen));
        Rectangle(item.hDC, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(item.hDC, oldPen);
        DeleteObject(pen);
    }
    wchar_t text[256]{};
    if (item.itemID != static_cast<UINT>(-1)) {
        SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID, reinterpret_cast<LPARAM>(text));
    } else {
        GetWindowTextW(item.hwndItem, text, static_cast<int>(std::size(text)));
    }
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, palette.controlText);
    RECT textRect = rect;
    textRect.left += 7;
    DrawTextW(item.hDC, text, -1, &textRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

std::wstring GetWindowTextValue(HWND window) {
    const int length = GetWindowTextLengthW(window);
    if (length <= 0) {
        return {};
    }

    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(window, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

std::string WideToUtf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }

    // No WC_ERR_INVALID_CHARS: a lone surrogate must become U+FFFD instead of
    // making the whole conversion fail (which used to save an empty file).
    const int size = WideCharToMultiByte(CP_UTF8, 0,
        value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0,
        value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(std::string_view value) {
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        const int fallbackSize = MultiByteToWideChar(CP_ACP, 0,
            value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (fallbackSize <= 0) {
            return {};
        }

        std::wstring fallback(static_cast<size_t>(fallbackSize), L'\0');
        MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()),
            fallback.data(), fallbackSize);
        return fallback;
    }

    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string HexEncode(std::string_view value) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(value.size() * 2);
    for (const unsigned char byte : value) {
        result.push_back(digits[(byte >> 4) & 0x0F]);
        result.push_back(digits[byte & 0x0F]);
    }
    return result;
}

int HexDigit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

std::string HexDecode(std::string_view value) {
    if (value.size() % 2 != 0) {
        return {};
    }

    std::string result;
    result.reserve(value.size() / 2);
    for (size_t index = 0; index < value.size(); index += 2) {
        const int high = HexDigit(value[index]);
        const int low = HexDigit(value[index + 1]);
        if (high < 0 || low < 0) {
            return {};
        }
        result.push_back(static_cast<char>((high << 4) | low));
    }
    return result;
}

std::vector<std::string> SplitTabs(std::string_view line) {
    std::vector<std::string> values;
    size_t start = 0;
    while (start <= line.size()) {
        const size_t end = line.find('\t', start);
        if (end == std::string_view::npos) {
            values.emplace_back(line.substr(start));
            break;
        }
        values.emplace_back(line.substr(start, end - start));
        start = end + 1;
    }
    return values;
}

bool ParseInt64(std::string_view value, int64_t& result) {
    try {
        size_t consumed = 0;
        result = std::stoll(std::string(value), &consumed);
        return consumed == value.size();
    } catch (...) {
        return false;
    }
}

bool ParseInt(std::string_view value, int& result) {
    int64_t parsed = 0;
    if (!ParseInt64(value, parsed)) {
        return false;
    }
    result = static_cast<int>(parsed);
    return true;
}

bool ParseBool(std::string_view value) {
    return value == "1" || value == "true";
}

bool EnsureDirectory(const fs::path& path) {
    std::error_code error;
    fs::create_directories(path, error);
    return !error;
}

fs::path GetDataDirectory() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return fs::path(buffer) / L"MCnotepad";
    }
    return fs::current_path() / L"data";
}

constexpr wchar_t kStartupRegistryPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kStartupRegistryValue[] = L"MCnotepad";

std::wstring CurrentExecutablePath() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) return {};
        if (length < path.size() - 1) {
            path.resize(length);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

bool IsStartupRegistrationEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kStartupRegistryPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t value[2048]{};
    DWORD valueSize = sizeof(value);
    DWORD valueType = 0;
    const LONG result = RegQueryValueExW(key, kStartupRegistryValue, nullptr, &valueType,
        reinterpret_cast<LPBYTE>(value), &valueSize);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && (valueType == REG_SZ || valueType == REG_EXPAND_SZ) && value[0] != L'\0';
}

bool SetStartupRegistration(bool enabled) {
    HKEY key = nullptr;
    DWORD disposition = 0;
    const LONG openResult = RegCreateKeyExW(HKEY_CURRENT_USER, kStartupRegistryPath, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, &disposition);
    if (openResult != ERROR_SUCCESS) return false;

    LONG result = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring executable = CurrentExecutablePath();
        const std::wstring command = L"\"" + executable + L"\"";
        result = RegSetValueExW(key, kStartupRegistryValue, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kStartupRegistryValue);
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool ReadBinaryFile(const fs::path& path, std::vector<char>& bytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0) {
        return false;
    }
    input.seekg(0, std::ios::beg);

    bytes.resize(static_cast<size_t>(size));
    if (size > 0) {
        input.read(bytes.data(), size);
    }
    return input.good() || input.eof();
}

bool WriteBinaryFile(const fs::path& path, std::string_view bytes) {
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool success = true;
    size_t written = 0;
    while (written < bytes.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(bytes.size() - written, 1u << 20));
        DWORD completed = 0;
        if (!WriteFile(handle, bytes.data() + written, chunk, &completed, nullptr) || completed == 0) {
            success = false;
            break;
        }
        written += completed;
    }
    if (success && !FlushFileBuffers(handle)) {
        success = false;
    }
    CloseHandle(handle);
    return success;
}

bool WriteAtomicFile(const fs::path& path, std::string_view bytes) {
    const fs::path temporaryPath = path.wstring() + L".tmp";
    if (!WriteBinaryFile(temporaryPath, bytes)) {
        return false;
    }

    if (MoveFileExW(temporaryPath.c_str(), path.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }

    std::error_code error;
    fs::remove(temporaryPath, error);
    return false;
}

std::wstring DecodeTextBytes(const std::vector<char>& bytes) {
    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        const int wideBytes = static_cast<int>(bytes.size() - 2);
        return std::wstring(reinterpret_cast<const wchar_t*>(bytes.data() + 2),
            reinterpret_cast<const wchar_t*>(bytes.data() + 2) + wideBytes / 2);
    }

    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFE &&
        static_cast<unsigned char>(bytes[1]) == 0xFF) {
        std::wstring result;
        result.reserve((bytes.size() - 2) / 2);
        for (size_t index = 2; index + 1 < bytes.size(); index += 2) {
            const wchar_t character = static_cast<wchar_t>(
                (static_cast<unsigned char>(bytes[index]) << 8) |
                static_cast<unsigned char>(bytes[index + 1]));
            result.push_back(character);
        }
        return result;
    }

    size_t offset = 0;
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        offset = 3;
    }

    const std::string_view text(bytes.data() + offset, bytes.size() - offset);
    return Utf8ToWide(text);
}

bool ReadTextFile(const fs::path& path, std::wstring& text) {
    std::vector<char> bytes;
    if (!ReadBinaryFile(path, bytes)) {
        return false;
    }
    text = DecodeTextBytes(bytes);
    return true;
}

std::wstring NormalizeLineEndings(std::wstring_view text, int mode) {
    std::wstring normalized;
    normalized.reserve(text.size() + 16);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            if (mode == 0) normalized += L"\r\n";
            else if (mode == 1) normalized += L'\n';
            else normalized += L'\r';
        } else if (text[i] == L'\n') {
            if (mode == 0) normalized += L"\r\n";
            else if (mode == 1) normalized += L'\n';
            else normalized += L'\r';
        } else {
            normalized += text[i];
        }
    }
    return normalized;
}

bool WriteTextFile(const fs::path& path, std::wstring_view text, int encodingIndex = -1, int lineEndingIndex = -1) {
    if (encodingIndex < 0) encodingIndex = g_app.encodingIndex;
    if (lineEndingIndex < 0) lineEndingIndex = g_app.lineEndingIndex;
    const std::wstring normalized = NormalizeLineEndings(text, lineEndingIndex);
    std::string bytes;
    if (encodingIndex == 2 || encodingIndex == 3) {
        bytes.reserve(normalized.size() * 2 + 2);
        if (encodingIndex == 2) bytes.append("\xFF\xFE", 2);
        else bytes.append("\xFE\xFF", 2);
        for (wchar_t value : normalized) {
            const uint16_t unit = static_cast<uint16_t>(value);
            if (encodingIndex == 2) {
                bytes.push_back(static_cast<char>(unit & 0xFF));
                bytes.push_back(static_cast<char>((unit >> 8) & 0xFF));
            } else {
                bytes.push_back(static_cast<char>((unit >> 8) & 0xFF));
                bytes.push_back(static_cast<char>(unit & 0xFF));
            }
        }
    } else if (encodingIndex == 4) {
        const int size = WideCharToMultiByte(CP_ACP, 0, normalized.data(), static_cast<int>(normalized.size()), nullptr, 0, "?", nullptr);
        if (size <= 0 && !normalized.empty()) return false;
        bytes.resize(static_cast<size_t>(size));
        if (size > 0) WideCharToMultiByte(CP_ACP, 0, normalized.data(), static_cast<int>(normalized.size()), bytes.data(), size, "?", nullptr);
    } else {
        const std::string utf8 = WideToUtf8(normalized);
        // Never report success for a conversion that lost the whole text.
        if (utf8.empty() && !normalized.empty()) return false;
        if (encodingIndex == 1) bytes.append("\xEF\xBB\xBF", 3);
        bytes += utf8;
    }
    return WriteAtomicFile(path, bytes);
}

std::wstring FileTitle(const fs::path& path) {
    const std::wstring name = path.filename().wstring();
    return name.empty() ? path.wstring() : name;
}

uint64_t FileTimestamp(const fs::path& path) {
    std::error_code error;
    const auto time = fs::last_write_time(path, error);
    if (error) {
        return 0;
    }
    return static_cast<uint64_t>(time.time_since_epoch().count());
}

void UpdateWindowTitle() {
    if (!g_app.window) return;
    std::wstring title = L"MCnotepad";
    if (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size())) {
        title += L" - " + g_app.documents[static_cast<size_t>(g_app.activeIndex)].title;
    }
    SetWindowTextW(g_app.window, title.c_str());
}

void UpdateTabTitle(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) {
        return;
    }

    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    std::wstring title = document.title;
    if (document.modified) {
        title += L" *";
    }

    TCITEMW item{};
    item.mask = TCIF_TEXT;
    item.pszText = title.data();
    TabCtrl_SetItem(g_app.tabs, index, &item);
    UpdateWindowTitle();
}

void CaptureEditorState(TabDocument& document) {
    DWORD anchor = 0;
    DWORD caret = 0;
    SendMessageW(document.editor, EM_GETSEL,
        reinterpret_cast<WPARAM>(&anchor), reinterpret_cast<LPARAM>(&caret));
    document.anchor = static_cast<int64_t>(anchor);
    document.caret = static_cast<int64_t>(caret);
    document.firstVisibleLine = static_cast<int>(SendMessageW(
        document.editor, EM_GETFIRSTVISIBLELINE, 0, 0));
}

void RestoreEditorState(const TabDocument& document) {
    const int start = static_cast<int>(std::clamp(document.anchor, int64_t(0), int64_t(0x7FFFFFFF)));
    const int end = static_cast<int>(std::clamp(document.caret, int64_t(0), int64_t(0x7FFFFFFF)));
    SendMessageW(document.editor, EM_SETSEL, static_cast<WPARAM>(start), static_cast<LPARAM>(end));
    SendMessageW(document.editor, EM_LINESCROLL, 0, document.firstVisibleLine);
    RefreshGutter();
}

void UpdateStatusBar() {
    if (g_app.statusBar == nullptr) {
        return;
    }

    int line = 1;
    int column = 1;
    bool modified = false;
    if (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size())) {
        const TabDocument& document = g_app.documents[static_cast<size_t>(g_app.activeIndex)];
        DWORD anchor = 0;
        DWORD caret = 0;
        SendMessageW(document.editor, EM_GETSEL,
            reinterpret_cast<WPARAM>(&anchor), reinterpret_cast<LPARAM>(&caret));
        const int lineIndex = static_cast<int>(SendMessageW(
            document.editor, EM_LINEFROMCHAR, static_cast<WPARAM>(caret), 0));
        const int lineStart = static_cast<int>(SendMessageW(
            document.editor, EM_LINEINDEX, static_cast<WPARAM>(lineIndex), 0));
        line = lineIndex + 1;
        column = std::max(1, static_cast<int>(caret) - lineStart + 1);
        modified = document.modified;
    }

    wchar_t position[64]{};
    swprintf_s(position, L"行 %d，列 %d", line, column);
    wchar_t state[64]{};
    swprintf_s(state, L"%s", modified ? L"未保存" : L"已保存");

    RECT client{};
    GetClientRect(g_app.window, &client);
    int parts[] = { 145, 430, client.right - 270, client.right - 140, client.right - 70, -1 };
    SendMessageW(g_app.statusBar, SB_SETPARTS, 6, reinterpret_cast<LPARAM>(parts));
    SendMessageW(g_app.statusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"就绪"));
    SendMessageW(g_app.statusBar, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(position));
    SendMessageW(g_app.statusBar, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(L"选中 0"));
    const wchar_t* encodings[] = {L"UTF-8", L"UTF-8 BOM", L"UTF-16 LE", L"UTF-16 BE", L"ANSI"};
    const wchar_t* endings[] = {L"CRLF", L"LF", L"CR"};
    const wchar_t* languages[] = {L"自动", L"HTML", L"CSS", L"JavaScript", L"C/C++", L"Python", L"JSON", L"Markdown", L"SQL", L"PowerShell"};
    const int encodingIndex = std::clamp(g_app.encodingIndex, 0, 4);
    const int lineEndingIndex = std::clamp(g_app.lineEndingIndex, 0, 2);
    const int languageIndex = std::clamp(g_app.languageIndex, 0, 9);
    SendMessageW(g_app.statusBar, SB_SETTEXTW, 3, reinterpret_cast<LPARAM>(encodings[encodingIndex]));
    SendMessageW(g_app.statusBar, SB_SETTEXTW, 4, reinterpret_cast<LPARAM>(endings[lineEndingIndex]));
    SendMessageW(g_app.statusBar, SB_SETTEXTW, 5, reinterpret_cast<LPARAM>(languages[languageIndex]));
}

void LayoutChildren(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int tabY = kToolbarHeight;
    const int contentY = tabY + kTabHeight;
    const int contentHeight = std::max(0, height - contentY - kStatusHeight);
    const int editorWidth = width;
    const int tabsWidth = std::max(0, width - kNewTabButtonWidth);

    if (g_app.toolbar) MoveWindow(g_app.toolbar, 0, 0, width, kToolbarHeight, TRUE);
    static constexpr int toolbarIds[] = {
        kIdToolbarNew, kIdToolbarOpen, kIdToolbarSave, kIdToolbarSaveAs, kIdToolbarClose,
        kIdToolbarUndo, kIdToolbarRedo, kIdToolbarFind, kIdToolbarReplace, kIdToolbarGoto,
        kIdToolbarFormat, kIdToolbarRun, kIdToolbarEncoding, kIdToolbarLineNumbers,
        kIdToolbarWrap, kIdToolbarTheme
    };
    const int buttonCount = static_cast<int>(g_app.toolbarButtons.size());
    int toolbarX = 0;
    for (int index = 0; index < buttonCount; ++index) {
        const int id = toolbarIds[std::min(index, static_cast<int>(std::size(toolbarIds)) - 1)];
        const int buttonWidth = ToolbarButtonWidth(id);
        MoveWindow(g_app.toolbarButtons[static_cast<size_t>(index)], toolbarX, 0,
            buttonWidth, kToolbarHeight, TRUE);
        toolbarX += buttonWidth;
    }
    MoveWindow(g_app.tabs, 0, tabY, tabsWidth, kTabHeight, TRUE);
    MoveWindow(g_app.newTabButton, tabsWidth, tabY, kNewTabButtonWidth, kTabHeight, TRUE);
    if (g_app.sidebar) {
        ShowWindow(g_app.sidebar, SW_HIDE);
    }
    if (g_app.gutter) MoveWindow(g_app.gutter, 0, contentY, kGutterWidth, contentHeight, TRUE);
    for (TabDocument& document : g_app.documents) {
        MoveWindow(document.editor, g_app.showLineNumbers ? kGutterWidth : 0, contentY,
            std::max(0, editorWidth - (g_app.showLineNumbers ? kGutterWidth : 0)), contentHeight, TRUE);
    }
    MoveWindow(g_app.statusBar, 0, std::max(0, height - kStatusHeight), width, kStatusHeight, TRUE);
    UpdateStatusBar();
    RefreshGutter();
}

void CreateToolbarControls(HWND window) {
    static constexpr int ids[] = {
        kIdToolbarNew, kIdToolbarOpen, kIdToolbarSave, kIdToolbarSaveAs, kIdToolbarClose,
        kIdToolbarUndo, kIdToolbarRedo, kIdToolbarFind, kIdToolbarReplace, kIdToolbarGoto,
        kIdToolbarFormat, kIdToolbarRun, kIdToolbarEncoding, kIdToolbarLineNumbers,
        kIdToolbarWrap, kIdToolbarTheme
    };
    g_app.toolbar = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
    g_app.toolbarButtons.clear();
    for (int id : ids) {
        HWND button = CreateWindowExW(0, L"BUTTON", ToolbarLabel(id).c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            g_app.instance, nullptr);
        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        g_app.toolbarButtons.push_back(button);

    }
}

LRESULT CALLBACK SidebarProc(HWND sidebar, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_COMMAND || message == WM_DRAWITEM || message == WM_MEASUREITEM ||
        message == WM_CTLCOLORSTATIC || message == WM_CTLCOLORBTN ||
        message == WM_CTLCOLORLISTBOX || message == WM_CTLCOLOREDIT) {
        if (g_app.window) {
            SendMessageW(g_app.window, message, wParam, lParam);
            return 0;
        }
    }
    return g_app.oldSidebarProc
        ? CallWindowProcW(g_app.oldSidebarProc, sidebar, message, wParam, lParam)
        : DefWindowProcW(sidebar, message, wParam, lParam);
}

void CreateSidebarControls(HWND window) {
    g_app.sidebar = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
    g_app.oldSidebarProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        g_app.sidebar, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SidebarProc)));
    g_app.sidebarTitle = CreateWindowExW(0, L"STATIC", L"文档设置", WS_CHILD | WS_VISIBLE,
        18, 18, 190, 24, g_app.sidebar, nullptr, g_app.instance, nullptr);
    g_app.sidebarClose = CreateWindowExW(0, L"BUTTON", L"×", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        226, 12, 24, 24, g_app.sidebar, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSidebarClose)),
        g_app.instance, nullptr);
    g_app.languageLabel = CreateWindowExW(0, L"STATIC", L"语言", WS_CHILD | WS_VISIBLE,
        18, 52, 214, 20, g_app.sidebar, nullptr, g_app.instance, nullptr);
    g_app.languageCombo = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 18, 76, 214, 160,
        g_app.sidebar, nullptr, g_app.instance, nullptr);
    for (const wchar_t* value : {L"自动检测", L"HTML", L"CSS", L"JavaScript", L"C/C++", L"Python", L"JSON", L"Markdown", L"SQL", L"PowerShell"}) {
        SendMessageW(g_app.languageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    SendMessageW(g_app.languageCombo, CB_SETCURSEL, 1, 0);

    g_app.encodingLabel = CreateWindowExW(0, L"STATIC", L"编码", WS_CHILD | WS_VISIBLE,
        18, 120, 214, 20, g_app.sidebar, nullptr, g_app.instance, nullptr);
    g_app.encodingCombo = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 18, 144, 214, 160,
        g_app.sidebar, nullptr, g_app.instance, nullptr);
    for (const wchar_t* value : {L"UTF-8", L"UTF-8 BOM", L"UTF-16 LE", L"UTF-16 BE", L"ANSI"}) {
        SendMessageW(g_app.encodingCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    SendMessageW(g_app.encodingCombo, CB_SETCURSEL, 0, 0);

    g_app.lineEndingLabel = CreateWindowExW(0, L"STATIC", L"换行", WS_CHILD | WS_VISIBLE,
        18, 188, 214, 20, g_app.sidebar, nullptr, g_app.instance, nullptr);
    g_app.lineEndingCombo = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST, 18, 212, 214, 160,
        g_app.sidebar, nullptr, g_app.instance, nullptr);
    for (const wchar_t* value : {L"Windows (CRLF)", L"Unix (LF)", L"旧式 Mac (CR)"}) {
        SendMessageW(g_app.lineEndingCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    SendMessageW(g_app.lineEndingCombo, CB_SETCURSEL, 0, 0);

    CreateWindowExW(0, L"STATIC", L"主题", WS_CHILD | WS_VISIBLE,
        18, 256, 214, 20, g_app.sidebar, nullptr, g_app.instance, nullptr);
    const struct ThemeButton { int id; const wchar_t* text; } themes[] = {
        {kIdThemeDark, L"深色（默认）"}, {kIdThemeLight, L"浅色"},
        {kIdThemeGreen, L"护眼绿"}, {kIdThemeBlue, L"暗夜蓝"}
    };
    for (int index = 0; index < 4; ++index) {
        const auto& theme = themes[index];
        CreateWindowExW(0, L"BUTTON", theme.text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            18 + (index % 2) * 110, 282 + (index / 2) * 46, 100, 36, g_app.sidebar,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(theme.id)), g_app.instance, nullptr);
        if (HWND button = GetDlgItem(g_app.sidebar, theme.id)) {
            SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        }
    }
    for (HWND control : {g_app.sidebarTitle, g_app.sidebarClose, g_app.languageLabel, g_app.languageCombo,
        g_app.encodingLabel, g_app.encodingCombo, g_app.lineEndingLabel, g_app.lineEndingCombo}) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
    }
}


void SelectTab(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) {
        return;
    }

    if (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size())) {
        CaptureEditorState(g_app.documents[static_cast<size_t>(g_app.activeIndex)]);
        ShowWindow(g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor, SW_HIDE);
    }

    g_app.activeIndex = index;
    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    ShowWindow(document.editor, SW_SHOW);
    SetFocus(document.editor);
    RestoreEditorState(document);
    TabCtrl_SetCurSel(g_app.tabs, index);
    g_app.stateDirty = true;
    UpdateWindowTitle();
}

void MarkRecoveryDirty(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) {
        return;
    }
    g_app.documents[static_cast<size_t>(index)].recoveryDirty = true;
    g_app.stateDirty = true;
}

bool SaveDocumentRecovery(TabDocument& document) {
    if (!document.recoveryDirty) {
        return true;
    }

    // Snapshots always use UTF-8 BOM + CRLF so recovery never depends on the
    // user's chosen save encoding (ANSI would silently drop characters).
    const std::wstring text = GetWindowTextValue(document.editor);
    if (!WriteTextFile(document.snapshotPath, text, 1, 0)) {
        return false;
    }

    document.recoveryDirty = false;
    return true;
}

bool SaveSessionFile() {
    std::ostringstream output;
    output << "MCSESSION\t1\n";
    output << "ACTIVE\t" << g_app.activeIndex << "\n";
    output << "THEME\t" << static_cast<int>(g_app.theme) << "\n";
    output << "OPTIONS\t" << (g_app.showLineNumbers ? 1 : 0) << '\t'
           << (g_app.wordWrap ? 1 : 0) << '\t'
           << (g_app.sidebarVisible ? 1 : 0) << '\t'
           << g_app.fontSize << '\t'
           << HexEncode(WideToUtf8(g_app.fontFamily)) << '\t'
           << g_app.languageIndex << '\t'
           << g_app.encodingIndex << '\t'
           << g_app.lineEndingIndex << "\n";
    output << "PREFERENCES\t" << (g_app.launchAtStartup ? 1 : 0) << '\t'
           << static_cast<int>(g_app.closeBehavior) << "\n";

    RECT windowRect{};
    GetWindowRect(g_app.window, &windowRect);
    output << "WINDOW\t" << windowRect.left << '\t' << windowRect.top << '\t'
           << windowRect.right - windowRect.left << '\t'
           << windowRect.bottom - windowRect.top << '\t'
           << (IsZoomed(g_app.window) ? 1 : 0) << "\n";

    for (size_t index = 0; index < g_app.documents.size(); ++index) {
        TabDocument& document = g_app.documents[index];
        CaptureEditorState(document);
        output << "TAB\t" << index << '\t'
               << HexEncode(WideToUtf8(document.path)) << '\t'
               << HexEncode(WideToUtf8(document.title)) << '\t'
               << HexEncode(WideToUtf8(document.snapshotPath.wstring())) << '\t'
               << document.caret << '\t'
               << document.anchor << '\t'
               << document.firstVisibleLine << '\t'
               << document.horizontalScroll << '\t'
               << (document.modified ? 1 : 0) << '\t'
               << (document.untitled ? 1 : 0) << '\t'
               << FileTimestamp(document.path) << '\n';
    }

    return WriteAtomicFile(g_app.sessionPath, output.str());
}

bool PersistAll(bool forceSnapshots) {
    bool success = true;
    if (forceSnapshots) {
        for (TabDocument& document : g_app.documents) {
            document.recoveryDirty = true;
        }
    }

    for (TabDocument& document : g_app.documents) {
        success = SaveDocumentRecovery(document) && success;
    }
    success = SaveSessionFile() && success;
    if (success) {
        g_app.stateDirty = false;
    }
    return success;
}

bool CreateRunningMarker() {
    return WriteAtomicFile(g_app.runningMarkerPath, "MCnotepad running\n");
}

void RemoveRunningMarker() {
    std::error_code error;
    fs::remove(g_app.runningMarkerPath, error);
}

LRESULT CALLBACK EditorProc(HWND editor, UINT message, WPARAM wParam, LPARAM lParam) {
    const auto oldIt = g_editorProcs.find(editor);
    const WNDPROC oldProc = oldIt != g_editorProcs.end() ? oldIt->second : nullptr;

    if (message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
        (GetKeyState(VK_MENU) & 0x8000) == 0) {
        switch (static_cast<UINT>(wParam)) {
        case 'A':
            SendMessageW(editor, EM_SETSEL, 0, -1);
            return 0;
        case 'C':
            SendMessageW(editor, WM_COPY, 0, 0);
            return 0;
        case 'V':
            SendMessageW(editor, WM_PASTE, 0, 0);
            return 0;
        case 'X':
            SendMessageW(editor, WM_CUT, 0, 0);
            return 0;
        case 'Z':
            SendMessageW(editor, WM_UNDO, 0, 0);
            return 0;
        case 'Y':
            SendMessageW(editor, EM_REDO, 0, 0);
            return 0;
        default:
            break;
        }
    }

    const LRESULT result = oldProc != nullptr
        ? CallWindowProcW(oldProc, editor, message, wParam, lParam)
        : DefWindowProcW(editor, message, wParam, lParam);
    if (message == WM_NCDESTROY) {
        g_editorProcs.erase(editor);
    } else if (message == WM_MOUSEWHEEL || message == WM_KEYDOWN) {
        // EN_VSCROLL is not sent for wheel or keyboard scrolling.
        RefreshGutter();
    }
    return result;
}

void AddTabToControl(const std::wstring& title) {
    TCITEMW item{};
    item.mask = TCIF_TEXT;
    item.pszText = const_cast<wchar_t*>(title.c_str());
    TabCtrl_InsertItem(g_app.tabs, static_cast<int>(g_app.documents.size()), &item);
}

bool SamePath(const fs::path& left, const fs::path& right) {
    if (left.empty() || right.empty()) return false;
    std::error_code error;
    const fs::path a = fs::absolute(left, error).lexically_normal();
    const fs::path b = fs::absolute(right, error).lexically_normal();
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

bool IsSnapshotInUse(const fs::path& snapshot) {
    for (const TabDocument& document : g_app.documents) {
        if (SamePath(document.snapshotPath, snapshot)) return true;
    }
    return false;
}

// Snapshot files are owned by exactly one tab for its whole lifetime. Names are
// never derived from the tab index, so closing or reordering tabs can't make two
// tabs share (and overwrite) one snapshot.
fs::path AllocateSnapshotPath() {
    for (int number = 1;; ++number) {
        fs::path candidate = g_app.recoveryDirectory /
            (L"tab-" + std::to_wstring(number) + L".snapshot");
        std::error_code error;
        if (!IsSnapshotInUse(candidate) && !fs::exists(candidate, error)) return candidate;
    }
}

// Removes snapshot files left behind by tabs that no longer exist (e.g. after a
// crash). Only call this once the session file was read successfully.
void RemoveOrphanSnapshots() {
    std::vector<fs::path> orphans;
    std::error_code error;
    for (fs::directory_iterator it(g_app.recoveryDirectory, error), end; !error && it != end; it.increment(error)) {
        std::error_code typeError;
        if (!it->is_regular_file(typeError)) continue;
        const fs::path file = it->path();
        const std::wstring name = file.filename().wstring();
        if (name.ends_with(L".snapshot.tmp")) {
            orphans.push_back(file);  // interrupted atomic write
        } else if (name.ends_with(L".snapshot") && !IsSnapshotInUse(file)) {
            orphans.push_back(file);
        }
    }
    for (const fs::path& file : orphans) {
        std::error_code removeError;
        fs::remove(file, removeError);
    }
}

int CreateDocument(std::wstring path, std::wstring title, std::wstring text,
    bool untitled, bool modified, int64_t caret, int64_t anchor, int firstVisibleLine,
    const fs::path* explicitSnapshot = nullptr) {
    TabDocument document;
    document.path = std::move(path);
    document.untitled = untitled;
    document.modified = modified;
    document.caret = caret;
    document.anchor = anchor;
    document.firstVisibleLine = firstVisibleLine;
    document.title = title.empty()
        ? (document.untitled ? std::wstring(L"Untitled-") + std::to_wstring(g_app.nextUntitledNumber++) : FileTitle(document.path))
        : std::move(title);
    if (document.untitled && document.title.rfind(L"Untitled-", 0) == 0) {
        try {
            const int number = std::stoi(document.title.substr(8));
            g_app.nextUntitledNumber = std::max(g_app.nextUntitledNumber, number + 1);
        } catch (...) {
        }
    }

    document.snapshotPath = (explicitSnapshot != nullptr && !explicitSnapshot->empty() &&
                             !IsSnapshotInUse(*explicitSnapshot))
        ? *explicitSnapshot
        : AllocateSnapshotPath();

    document.editor = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"RICHEDIT50W",
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_LEFT | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL | ES_WANTRETURN,
        0, kTabHeight, 0, 0,
        g_app.window,
        nullptr,
        g_app.instance,
        nullptr);

    if (!document.editor) {
        return -1;
    }

    const WNDPROC oldEditorProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        document.editor, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditorProc)));
    if (oldEditorProc != nullptr) {
        g_editorProcs[document.editor] = oldEditorProc;
    }

    SendMessageW(document.editor, EM_SETLIMITTEXT, 0x7FFFFFFE, 0);
    SendMessageW(document.editor, WM_SETFONT,
        reinterpret_cast<WPARAM>(g_app.editorFont != nullptr
            ? g_app.editorFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    ApplyEditorTheme(document.editor);
    SetWindowTextW(document.editor, text.c_str());
    ApplySimpleSyntaxHighlight(document.editor);
    SendMessageW(document.editor, EM_SETMODIFY, modified ? TRUE : FALSE, 0);
    // Enable change/scroll notifications only after the initial text is in,
    // so loading a file does not mark it as modified.
    SendMessageW(document.editor, EM_SETEVENTMASK, 0, kEditorEventMask);
    ShowWindow(document.editor, SW_HIDE);

    g_app.documents.push_back(std::move(document));
    AddTabToControl(g_app.documents.back().title);
    UpdateTabTitle(static_cast<int>(g_app.documents.size() - 1));
    LayoutChildren(g_app.window);
    return static_cast<int>(g_app.documents.size() - 1);
}

bool SaveDocument(int index, bool saveAs) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) {
        return false;
    }

    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    fs::path target = document.path;

    if (saveAs || document.untitled || target.empty()) {
        wchar_t fileName[MAX_PATH]{};
        if (!document.path.empty()) {
            wcsncpy_s(fileName, MAX_PATH, document.path.c_str(), _TRUNCATE);
        }

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = g_app.window;
        dialog.lpstrFilter = L"Text files\0*.txt;*.md;*.log;*.json;*.cpp;*.h\0All files\0*.*\0";
        dialog.lpstrFile = fileName;
        dialog.nMaxFile = MAX_PATH;
        dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        dialog.lpstrDefExt = L"txt";
        if (!GetSaveFileNameW(&dialog)) {
            return false;
        }
        target = fileName;
    }

    const std::wstring text = GetWindowTextValue(document.editor);
    if (!WriteTextFile(target, text)) {
        MessageBoxW(g_app.window, L"文件保存失败，请检查路径和权限。", kWindowTitle, MB_ICONERROR);
        return false;
    }

    document.path = target.wstring();
    document.title = FileTitle(target);
    document.untitled = false;
    document.modified = false;
    document.recoveryDirty = true;
    SendMessageW(document.editor, EM_SETMODIFY, FALSE, 0);
    UpdateTabTitle(index);
    g_app.stateDirty = true;
    return PersistAll(false);
}

bool OpenFileInNewTab(const fs::path& path) {
    for (size_t index = 0; index < g_app.documents.size(); ++index) {
        const TabDocument& existing = g_app.documents[index];
        if (!existing.untitled && SamePath(existing.path, path)) {
            SelectTab(static_cast<int>(index));
            return true;
        }
    }

    std::wstring text;
    if (!ReadTextFile(path, text)) {
        MessageBoxW(g_app.window, L"文件打开失败，请检查路径和权限。", kWindowTitle, MB_ICONERROR);
        return false;
    }

    const int index = CreateDocument(path.wstring(), FileTitle(path), std::move(text),
        false, false, 0, 0, 0);
    if (index < 0) {
        return false;
    }
    SelectTab(index);
    PersistAll(false);
    return true;
}

void NewDocument() {
    const int index = CreateDocument({}, {}, {}, true, false, 0, 0, 0);
    if (index >= 0) {
        SelectTab(index);
        PersistAll(false);
    }
}

bool PrepareDocumentClose(TabDocument& document) {
    if (!document.modified) return true;
    if (g_app.closeBehavior == CloseBehavior::Direct) return true;
    if (g_app.closeBehavior == CloseBehavior::Save) {
        const int index = static_cast<int>(&document - g_app.documents.data());
        return SaveDocument(index, false);
    }

    const int choice = MessageBoxW(g_app.window,
        L"当前标签还有未保存修改，是否保存？\n选择“否”会直接关闭并丢弃修改。",
        kWindowTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return false;
    if (choice == IDYES) {
        const int index = static_cast<int>(&document - g_app.documents.data());
        return SaveDocument(index, false);
    }
    return true;
}

void CloseDocument(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) {
        return;
    }

    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    if (!PrepareDocumentClose(document)) return;

    const fs::path snapshot = document.snapshotPath;
    DestroyWindow(document.editor);
    g_app.documents.erase(g_app.documents.begin() + index);
    TabCtrl_DeleteItem(g_app.tabs, index);

    std::error_code error;
    fs::remove(snapshot, error);

    if (g_app.documents.empty()) {
        g_app.activeIndex = -1;
        NewDocument();
        return;
    }

    const int nextIndex = std::min(index, static_cast<int>(g_app.documents.size()) - 1);
    g_app.activeIndex = -1;
    SelectTab(nextIndex);
    g_app.stateDirty = true;
    PersistAll(false);
}

bool PrepareApplicationClose() {
    std::vector<int> modified;
    for (size_t index = 0; index < g_app.documents.size(); ++index) {
        if (g_app.documents[index].modified) modified.push_back(static_cast<int>(index));
    }
    if (modified.empty() || g_app.closeBehavior == CloseBehavior::Direct) return true;

    if (g_app.closeBehavior == CloseBehavior::Save) {
        for (const int index : modified) {
            if (!SaveDocument(index, false)) return false;
        }
        return true;
    }

    const int choice = MessageBoxW(g_app.window,
        L"有未保存的标签。是否保存全部修改后退出？\n选择“否”会退出，但仍保留会话恢复内容。",
        kWindowTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return false;
    if (choice == IDYES) {
        for (const int index : modified) {
            if (!SaveDocument(index, false)) return false;
        }
    }
    return true;
}

void OpenFileDialog() {
    wchar_t fileName[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_app.window;
    dialog.lpstrFilter = L"All files\0*.*\0Text files\0*.txt;*.md;*.log;*.json;*.cpp;*.h\0";
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;

    if (!GetOpenFileNameW(&dialog)) {
        return;
    }

    if (dialog.Flags & OFN_ALLOWMULTISELECT) {
        const wchar_t* cursor = fileName;
        const std::wstring directory = cursor;
        cursor += directory.size() + 1;
        if (*cursor == L'\0') {
            OpenFileInNewTab(directory);
            return;
        }
        while (*cursor != L'\0') {
            const std::wstring fileNamePart = cursor;
            OpenFileInNewTab(fs::path(directory) / fileNamePart);
            cursor += fileNamePart.size() + 1;
        }
        return;
    }

    OpenFileInNewTab(fileName);
}

void SetMenuState() {
    const bool hasActive = g_app.activeIndex >= 0 &&
        g_app.activeIndex < static_cast<int>(g_app.documents.size());
    HMENU menu = GetMenu(g_app.window);
    if (!menu) {
        return;
    }
    EnableMenuItem(menu, kIdFileSave, MF_BYCOMMAND | (hasActive ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, kIdFileSaveAs, MF_BYCOMMAND | (hasActive ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, kIdFileClose, MF_BYCOMMAND | (hasActive ? MF_ENABLED : MF_GRAYED));
    if (g_app.themeMenu) {
        CheckMenuRadioItem(g_app.themeMenu, kIdThemeDark, kIdThemeBlue,
            kIdThemeDark + static_cast<int>(g_app.theme), MF_BYCOMMAND);
    }
    CheckMenuItem(menu, kIdToolbarLineNumbers, MF_BYCOMMAND |
        (g_app.showLineNumbers ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, kIdToolbarWrap, MF_BYCOMMAND |
        (g_app.wordWrap ? MF_CHECKED : MF_UNCHECKED));
}

HMENU CreateApplicationMenu() {
    HMENU menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU edit = CreatePopupMenu();
    HMENU search = CreatePopupMenu();
    HMENU view = CreatePopupMenu();
    HMENU encoding = CreatePopupMenu();
    HMENU language = CreatePopupMenu();
    HMENU run = CreatePopupMenu();
    HMENU tools = CreatePopupMenu();
    HMENU settings = CreatePopupMenu();
    HMENU theme = CreatePopupMenu();
    HMENU help = CreatePopupMenu();

    AppendMenuW(file, MF_STRING, kIdFileNew, L"新建\tCtrl+N");
    AppendMenuW(file, MF_STRING, kIdFileOpen, L"打开...\tCtrl+O");
    AppendMenuW(file, MF_STRING, kIdFileSave, L"保存\tCtrl+S");
    AppendMenuW(file, MF_STRING, kIdFileSaveAs, L"另存为...\tCtrl+Shift+S");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, kIdFileClose, L"关闭标签\tCtrl+W");
    AppendMenuW(file, MF_STRING, kIdFileExit, L"退出");

    AppendMenuW(edit, MF_STRING, kIdEditUndo, L"撤销\tCtrl+Z");
    AppendMenuW(edit, MF_STRING, kIdEditRedo, L"重做\tCtrl+Y");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, kIdEditCut, L"剪切\tCtrl+X");
    AppendMenuW(edit, MF_STRING, kIdEditCopy, L"复制\tCtrl+C");
    AppendMenuW(edit, MF_STRING, kIdEditPaste, L"粘贴\tCtrl+V");
    AppendMenuW(edit, MF_STRING, kIdEditSelectAll, L"全选\tCtrl+A");

    AppendMenuW(search, MF_STRING, kIdToolbarFind, L"查找\tCtrl+F");
    AppendMenuW(search, MF_STRING, kIdToolbarReplace, L"替换\tCtrl+H");
    AppendMenuW(search, MF_STRING, kIdToolbarGoto, L"跳转\tCtrl+G");
    AppendMenuW(view, MF_STRING, kIdToolbarLineNumbers, L"行号");
    AppendMenuW(view, MF_STRING, kIdToolbarWrap, L"自动换行");
    AppendMenuW(encoding, MF_STRING, kIdToolbarEncoding, L"UTF-8");
    AppendMenuW(language, MF_STRING, kIdToolbarFormat, L"自动检测");
    AppendMenuW(run, MF_STRING, kIdToolbarRun, L"运行");
    AppendMenuW(tools, MF_STRING, kIdToolbarFormat, L"格式化");
    AppendMenuW(theme, MF_STRING, kIdThemeDark, L"深色");
    AppendMenuW(theme, MF_STRING, kIdThemeLight, L"浅色");
    AppendMenuW(theme, MF_STRING, kIdThemeGreen, L"护眼绿");
    AppendMenuW(theme, MF_STRING, kIdThemeBlue, L"暗夜蓝");
    g_app.themeMenu = theme;
    AppendMenuW(settings, MF_POPUP, reinterpret_cast<UINT_PTR>(theme), L"主题");
    AppendMenuW(settings, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(settings, MF_STRING, kIdToolbarSettings, L"偏好设置...\tCtrl+,");
    AppendMenuW(help, MF_STRING, kIdHelpAbout, L"关于 MCnotepad");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"文件(F)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), L"编辑(E)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(search), L"搜索(S)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"视图(V)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(encoding), L"编码(N)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(language), L"语言(L)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(run), L"运行(R)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), L"工具(T)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(settings), L"设置(O)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"帮助(H)");
    return menu;
}

void LoadPersistedSession() {
    std::ifstream input(g_app.sessionPath, std::ios::binary);
    if (!input) {
        NewDocument();
        return;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    std::string line;
    int activeIndex = 0;
    int persistedTheme = static_cast<int>(g_app.theme);
    int persistedShowLineNumbers = g_app.showLineNumbers ? 1 : 0;
    int persistedWordWrap = g_app.wordWrap ? 1 : 0;
    int persistedSidebarVisible = g_app.sidebarVisible ? 1 : 0;
    int persistedFontSize = g_app.fontSize;
    int persistedLanguage = g_app.languageIndex;
    int persistedEncoding = g_app.encodingIndex;
    int persistedLineEnding = g_app.lineEndingIndex;
    int persistedStartup = g_app.launchAtStartup ? 1 : 0;
    int persistedCloseBehavior = static_cast<int>(g_app.closeBehavior);
    std::wstring persistedFontFamily = g_app.fontFamily;
    std::vector<std::tuple<std::wstring, std::wstring, fs::path, int64_t, int64_t, int, int, bool, bool>> entries;

    while (std::getline(buffer, line)) {
        const std::vector<std::string> parts = SplitTabs(line);
        if (parts.empty()) {
            continue;
        }

        if (parts[0] == "ACTIVE" && parts.size() >= 2) {
            ParseInt(parts[1], activeIndex);
        } else if (parts[0] == "THEME" && parts.size() >= 2) {
            ParseInt(parts[1], persistedTheme);
        } else if (parts[0] == "OPTIONS" && parts.size() >= 9) {
            ParseInt(parts[1], persistedShowLineNumbers);
            ParseInt(parts[2], persistedWordWrap);
            ParseInt(parts[3], persistedSidebarVisible);
            ParseInt(parts[4], persistedFontSize);
            persistedFontFamily = Utf8ToWide(HexDecode(parts[5]));
            ParseInt(parts[6], persistedLanguage);
            ParseInt(parts[7], persistedEncoding);
            ParseInt(parts[8], persistedLineEnding);
        } else if (parts[0] == "PREFERENCES" && parts.size() >= 3) {
            ParseInt(parts[1], persistedStartup);
            ParseInt(parts[2], persistedCloseBehavior);
        } else if (parts[0] == "TAB" && parts.size() >= 12) {
            int64_t caret = 0;
            int64_t anchor = 0;
            int firstLine = 0;
            int horizontalScroll = 0;
            int sourceModified = 0;
            int untitled = 0;
            ParseInt64(parts[5], caret);
            ParseInt64(parts[6], anchor);
            ParseInt(parts[7], firstLine);
            ParseInt(parts[8], horizontalScroll);
            ParseInt(parts[9], sourceModified);
            ParseInt(parts[10], untitled);

            const std::wstring path = Utf8ToWide(HexDecode(parts[2]));
            const std::wstring title = Utf8ToWide(HexDecode(parts[3]));
            const fs::path snapshot = fs::path(Utf8ToWide(HexDecode(parts[4])));
            entries.emplace_back(path, title, snapshot, caret, anchor, firstLine,
                horizontalScroll, sourceModified != 0, untitled != 0);
        }
    }

    g_app.theme = static_cast<ThemeKind>(std::clamp(persistedTheme, 0, 3));
    g_app.showLineNumbers = persistedShowLineNumbers != 0;
    g_app.wordWrap = persistedWordWrap != 0;
    g_app.sidebarVisible = false;
    g_app.fontSize = std::clamp(persistedFontSize, 10, 32);
    if (!persistedFontFamily.empty()) g_app.fontFamily = persistedFontFamily;
    g_app.languageIndex = std::clamp(persistedLanguage, 0, 9);
    g_app.encodingIndex = std::clamp(persistedEncoding, 0, 4);
    g_app.lineEndingIndex = std::clamp(persistedLineEnding, 0, 2);
    g_app.launchAtStartup = persistedStartup != 0;
    g_app.closeBehavior = static_cast<CloseBehavior>(std::clamp(persistedCloseBehavior, 0, 2));

    if (entries.empty()) {
        NewDocument();
        return;
    }

    g_app.loading = true;
    for (const auto& entry : entries) {
        const auto& [path, title, snapshot, caret, anchor, firstLine, horizontalScroll, modified, untitled] = entry;
        std::wstring text;
        if (!snapshot.empty() && fs::exists(snapshot)) {
            ReadTextFile(snapshot, text);
        }
        if (text.empty() && !untitled && !path.empty()) {
            ReadTextFile(path, text);
        }

        const int index = CreateDocument(path, title, std::move(text), untitled,
            modified, caret, anchor, firstLine, &snapshot);
        if (index >= 0) {
            TabDocument& created = g_app.documents[static_cast<size_t>(index)];
            created.horizontalScroll = horizontalScroll;
            // A tab that had to get a fresh snapshot (missing or shared by an
            // earlier tab in an old session) must write it on the next autosave.
            created.recoveryDirty = !SamePath(created.snapshotPath, snapshot);
        }
    }
    g_app.loading = false;
    RemoveOrphanSnapshots();

    if (g_app.documents.empty()) {
        NewDocument();
        return;
    }

    ApplyEditorFont();
    ApplyThemeToEditors(false);
    if (g_app.languageCombo) SendMessageW(g_app.languageCombo, CB_SETCURSEL, g_app.languageIndex, 0);
    if (g_app.encodingCombo) SendMessageW(g_app.encodingCombo, CB_SETCURSEL, g_app.encodingIndex, 0);
    if (g_app.lineEndingCombo) SendMessageW(g_app.lineEndingCombo, CB_SETCURSEL, g_app.lineEndingIndex, 0);
    SelectTab(std::clamp(activeIndex, 0, static_cast<int>(g_app.documents.size()) - 1));
    g_app.stateDirty = false;
}

void InitializeStorage() {
    g_app.dataDirectory = GetDataDirectory();
    g_app.recoveryDirectory = g_app.dataDirectory / L"recovery";
    g_app.sessionPath = g_app.dataDirectory / L"session.dat";
    g_app.runningMarkerPath = g_app.dataDirectory / L"running.lock";

    EnsureDirectory(g_app.dataDirectory);
    EnsureDirectory(g_app.recoveryDirectory);
    g_app.launchAtStartup = IsStartupRegistrationEnabled();
    g_app.previousRunWasUnclean = fs::exists(g_app.runningMarkerPath);
    CreateRunningMarker();
}

void OpenCommandLineFiles() {
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments == nullptr) {
        return;
    }

    for (int index = 1; index < argumentCount; ++index) {
        const std::wstring argument = arguments[index];
        if (argument == L"--new-instance" || argument == L"--portable") {
            continue;
        }
        if (!argument.empty() && argument[0] != L'-') {
            OpenFileInNewTab(fs::path(argument));
        }
    }
    LocalFree(arguments);
}

void FindNextText(const wchar_t* needle) {
    if (!needle || !*needle || g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) return;
    HWND editor = g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor;
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const LONG length = GetWindowTextLengthW(editor);
    FINDTEXTEXW query{};
    query.chrg.cpMin = selection.cpMax;
    query.chrg.cpMax = length;
    query.lpstrText = const_cast<wchar_t*>(needle);
    LRESULT found = SendMessageW(editor, EM_FINDTEXTEXW, FR_DOWN, reinterpret_cast<LPARAM>(&query));
    if (found < 0 && selection.cpMax > 0) {
        query.chrg.cpMin = 0;
        found = SendMessageW(editor, EM_FINDTEXTEXW, FR_DOWN, reinterpret_cast<LPARAM>(&query));
    }
    if (found >= 0) {
        SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&query.chrgText));
        SetFocus(editor);
    }
}

void OpenFindDialog(bool replace) {
    ZeroMemory(&g_app.findReplace, sizeof(g_app.findReplace));
    g_app.findReplace.lStructSize = sizeof(g_app.findReplace);
    g_app.findReplace.hwndOwner = g_app.window;
    g_app.findReplace.lpstrFindWhat = g_app.findBuffer;
    g_app.findReplace.wFindWhatLen = static_cast<WORD>(std::size(g_app.findBuffer));
    g_app.findReplace.Flags = FR_DOWN;
    if (replace) {
        g_app.findReplace.lpstrReplaceWith = g_app.replaceBuffer;
        g_app.findReplace.wReplaceWithLen = static_cast<WORD>(std::size(g_app.replaceBuffer));
        g_app.findDialog = ReplaceTextW(&g_app.findReplace);
    } else {
        g_app.findDialog = FindTextW(&g_app.findReplace);
    }
}

void HandleFindReplaceMessage(LPARAM lParam) {
    auto* data = reinterpret_cast<FINDREPLACEW*>(lParam);
    if (!data) return;
    if (data->Flags & FR_DIALOGTERM) {
        g_app.findDialog = nullptr;
        return;
    }
    if (data->Flags & FR_FINDNEXT) {
        FindNextText(data->lpstrFindWhat);
    }
    if (data->Flags & FR_REPLACE) {
        if (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size())) {
            HWND editor = g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor;
            CHARRANGE selection{};
            SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
            SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(data->lpstrReplaceWith));
            FindNextText(data->lpstrFindWhat);
        }
    }
    if (data->Flags & FR_REPLACEALL) {
        if (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size())) {
            HWND editor = g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor;
            SendMessageW(editor, EM_SETSEL, 0, 0);
            int count = 0;
            while (count < 100000) {
                CHARRANGE selection{};
                SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
                FINDTEXTEXW query{};
                query.chrg.cpMin = selection.cpMin;
                query.chrg.cpMax = GetWindowTextLengthW(editor);
                query.lpstrText = data->lpstrFindWhat;
                const LRESULT found = SendMessageW(editor, EM_FINDTEXTEXW, FR_DOWN, reinterpret_cast<LPARAM>(&query));
                if (found < 0) break;
                SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&query.chrgText));
                SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(data->lpstrReplaceWith));
                ++count;
            }
        }
    }
}

HACCEL CreateAccelerators() {
    ACCEL accelerators[] = {
        { FCONTROL | FVIRTKEY, 'N', kIdFileNew },
        { FCONTROL | FVIRTKEY, 'O', kIdFileOpen },
        { FCONTROL | FVIRTKEY, 'S', kIdFileSave },
        { FCONTROL | FSHIFT | FVIRTKEY, 'S', kIdFileSaveAs },
        { FCONTROL | FVIRTKEY, 'W', kIdFileClose },
        { FCONTROL | FVIRTKEY, 'Z', kIdEditUndo },
        { FCONTROL | FVIRTKEY, 'Y', kIdEditRedo },
        { FCONTROL | FVIRTKEY, 'X', kIdEditCut },
        { FCONTROL | FVIRTKEY, 'C', kIdEditCopy },
        { FCONTROL | FVIRTKEY, 'V', kIdEditPaste },
        { FCONTROL | FVIRTKEY, 'A', kIdEditSelectAll },
        { FCONTROL | FVIRTKEY, 'F', kIdToolbarFind },
        { FCONTROL | FVIRTKEY, 'H', kIdToolbarReplace },
        { FCONTROL | FVIRTKEY, 'G', kIdToolbarGoto },
        { FCONTROL | FVIRTKEY, VK_OEM_COMMA, kIdToolbarSettings }
    };
    return CreateAcceleratorTableW(accelerators, static_cast<int>(std::size(accelerators)));
}

void HandleEditCommand(int command) {
    if (g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) {
        return;
    }
    HWND editor = g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor;
    switch (command) {
    case kIdEditUndo: SendMessageW(editor, WM_UNDO, 0, 0); break;
    case kIdEditRedo: SendMessageW(editor, EM_REDO, 0, 0); break;
    case kIdEditCut: SendMessageW(editor, WM_CUT, 0, 0); break;
    case kIdEditCopy: SendMessageW(editor, WM_COPY, 0, 0); break;
    case kIdEditPaste: SendMessageW(editor, WM_PASTE, 0, 0); break;
    case kIdEditSelectAll: SendMessageW(editor, EM_SETSEL, 0, -1); break;
    default: break;
    }
}

LRESULT CALLBACK TabProc(HWND tab, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_ERASEBKGND) {
        HDC dc = reinterpret_cast<HDC>(wParam);
        RECT client{};
        GetClientRect(tab, &client);
        HBRUSH brush = CreateSolidBrush(CurrentPalette().tabBarBackground);
        FillRect(dc, &client, brush);
        DeleteObject(brush);
        return 1;
    }
    if (message == WM_LBUTTONDOWN) {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const int index = TabCtrl_HitTest(tab, &point);
        if (index >= 0) {
            RECT rect{};
            TabCtrl_GetItemRect(tab, index, &rect);
            const int centerY = (rect.top + rect.bottom) / 2;
            if (point.x >= rect.right - kTabCloseHitWidth &&
                point.y >= centerY - 10 && point.y <= centerY + 10) {
                CloseDocument(index);
                return 0;
            }
        }
    }
    return CallWindowProcW(g_app.oldTabProc, tab, message, wParam, lParam);
}

void ApplyEditorFont() {
    DeleteObject(g_app.editorFont);
    HDC dc = GetDC(g_app.window);
    const int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(g_app.window, dc);
    const int fontHeight = -MulDiv(g_app.fontSize, dpi, 72);
    g_app.editorFont = CreateFontW(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, g_app.fontFamily.c_str());
    for (const TabDocument& document : g_app.documents) {
        EditorEventMute mute(document.editor);
        SendMessageW(document.editor, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
        InvalidateRect(document.editor, nullptr, TRUE);
    }
    RefreshGutter();
}

void CreateSettingsControl(HWND parent, const wchar_t* className, const wchar_t* text,
    DWORD style, int id, int x, int y, int width, int height) {
    HWND control = CreateWindowExW(0, className, text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        g_app.instance, nullptr);
    if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
    if (id == kIdSettingsFont) g_app.settingsFontCombo = control;
    if (id == kIdSettingsSize) g_app.settingsSizeCombo = control;
    if (id == kIdSettingsTheme) g_app.settingsThemeCombo = control;
    if (id == kIdSettingsLineNumbers) g_app.settingsLineNumbersCheck = control;
    if (id == kIdSettingsWrap) g_app.settingsWrapCheck = control;
    if (id == kIdSettingsStartup) g_app.settingsStartupCheck = control;
    if (id == kIdSettingsCloseBehavior) g_app.settingsCloseBehaviorCombo = control;
}

LRESULT CALLBACK SettingsProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        auto addLabel = [&](const wchar_t* text, int x, int y, int w, int h) {
            CreateSettingsControl(window, L"STATIC", text, 0, 0, x, y, w, h);
        };
        addLabel(L"编辑器字体", 26, 20, 120, 24);
        CreateSettingsControl(window, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP,
            kIdSettingsFont, 160, 16, 260, 180);
        for (const wchar_t* value : {L"Consolas", L"Cascadia Mono", L"Microsoft YaHei Mono", L"Courier New"})
            SendMessageW(g_app.settingsFontCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        SendMessageW(g_app.settingsFontCombo, CB_SETCURSEL, g_app.fontFamily == L"Consolas" ? 0 : 1, 0);
        addLabel(L"字号", 26, 58, 120, 24);
        CreateSettingsControl(window, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP,
            kIdSettingsSize, 160, 54, 120, 180);
        for (const wchar_t* value : {L"12", L"13", L"14", L"15", L"16", L"18", L"20", L"22"})
            SendMessageW(g_app.settingsSizeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        const int sizes[] = {12, 13, 14, 15, 16, 18, 20, 22};
        int sizeIndex = 1;
        for (int i = 0; i < 8; ++i) if (sizes[i] == g_app.fontSize) sizeIndex = i;
        SendMessageW(g_app.settingsSizeCombo, CB_SETCURSEL, sizeIndex, 0);
        addLabel(L"默认主题", 26, 96, 120, 24);
        CreateSettingsControl(window, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP,
            kIdSettingsTheme, 160, 92, 260, 180);
        for (const wchar_t* value : {L"深色", L"浅色", L"护眼绿", L"暗夜蓝"})
            SendMessageW(g_app.settingsThemeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        SendMessageW(g_app.settingsThemeCombo, CB_SETCURSEL, static_cast<int>(g_app.theme), 0);
        CreateSettingsControl(window, L"BUTTON", L"开机启动", BS_AUTOCHECKBOX | WS_TABSTOP,
            kIdSettingsStartup, 26, 132, 170, 26);
        SendMessageW(g_app.settingsStartupCheck, BM_SETCHECK,
            g_app.launchAtStartup ? BST_CHECKED : BST_UNCHECKED, 0);
        addLabel(L"关闭未保存标签", 26, 170, 130, 24);
        CreateSettingsControl(window, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP,
            kIdSettingsCloseBehavior, 160, 166, 260, 180);
        for (const wchar_t* value : {L"提示保存", L"直接关闭（保留恢复内容）", L"自动保存后关闭"})
            SendMessageW(g_app.settingsCloseBehaviorCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        SendMessageW(g_app.settingsCloseBehaviorCombo, CB_SETCURSEL,
            static_cast<int>(g_app.closeBehavior), 0);
        CreateSettingsControl(window, L"BUTTON", L"显示行号", BS_AUTOCHECKBOX | WS_TABSTOP,
            kIdSettingsLineNumbers, 26, 208, 170, 26);
        CreateSettingsControl(window, L"BUTTON", L"自动换行", BS_AUTOCHECKBOX | WS_TABSTOP,
            kIdSettingsWrap, 215, 208, 170, 26);
        SendMessageW(g_app.settingsLineNumbersCheck, BM_SETCHECK, g_app.showLineNumbers ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(g_app.settingsWrapCheck, BM_SETCHECK, g_app.wordWrap ? BST_CHECKED : BST_UNCHECKED, 0);
        CreateSettingsControl(window, L"BUTTON", L"应用", BS_DEFPUSHBUTTON | WS_TABSTOP,
            kIdSettingsApply, 250, 270, 82, 30);
        CreateSettingsControl(window, L"BUTTON", L"取消", BS_PUSHBUTTON | WS_TABSTOP,
            kIdSettingsCancel, 340, 270, 82, 30);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kIdSettingsApply) {
            const int fontIndex = static_cast<int>(SendMessageW(g_app.settingsFontCombo, CB_GETCURSEL, 0, 0));
            const wchar_t* fonts[] = {L"Consolas", L"Cascadia Mono", L"Microsoft YaHei Mono", L"Courier New"};
            if (fontIndex >= 0 && fontIndex < 4) g_app.fontFamily = fonts[fontIndex];
            const int sizeIndex = static_cast<int>(SendMessageW(g_app.settingsSizeCombo, CB_GETCURSEL, 0, 0));
            const int sizes[] = {12, 13, 14, 15, 16, 18, 20, 22};
            if (sizeIndex >= 0 && sizeIndex < 8) g_app.fontSize = sizes[sizeIndex];
            const int themeIndex = static_cast<int>(SendMessageW(g_app.settingsThemeCombo, CB_GETCURSEL, 0, 0));
            if (themeIndex >= 0 && themeIndex < 4) SetThemeKind(static_cast<ThemeKind>(themeIndex));
            g_app.showLineNumbers = SendMessageW(g_app.settingsLineNumbersCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_app.wordWrap = SendMessageW(g_app.settingsWrapCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            const bool startupEnabled = SendMessageW(g_app.settingsStartupCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            const int closeBehaviorIndex = static_cast<int>(SendMessageW(
                g_app.settingsCloseBehaviorCombo, CB_GETCURSEL, 0, 0));
            if (closeBehaviorIndex >= 0 && closeBehaviorIndex <= 2) {
                g_app.closeBehavior = static_cast<CloseBehavior>(closeBehaviorIndex);
            }
            if (!SetStartupRegistration(startupEnabled)) {
                MessageBoxW(window, L"开机启动设置写入失败，请检查当前账户的注册表权限。",
                    kWindowTitle, MB_ICONWARNING);
            } else {
                g_app.launchAtStartup = startupEnabled;
            }
            for (const TabDocument& document : g_app.documents) {
                LONG style = GetWindowLongW(document.editor, GWL_STYLE);
                style = g_app.wordWrap ? (style & ~WS_HSCROLL) : (style | WS_HSCROLL);
                SetWindowLongW(document.editor, GWL_STYLE, style);
            }
            ApplyEditorFont();
            LayoutChildren(g_app.window);
            g_app.stateDirty = true;
            PersistAll(false);
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wParam) == kIdSettingsCancel) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        g_app.settingsWindow = nullptr;
        EnableWindow(g_app.window, TRUE);
        SetForegroundWindow(g_app.window);
        return 0;
    case WM_ERASEBKGND: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        RECT client{};
        GetClientRect(window, &client);
        FillRect(dc, &client, g_app.windowBrush);
        return 1;
    }
    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (item != nullptr && item->CtlType == ODT_COMBOBOX) DrawComboItem(*item);
        return TRUE;
    }
    case WM_MEASUREITEM: {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (measure != nullptr && measure->CtlType == ODT_COMBOBOX) {
            measure->itemHeight = 24;
            return TRUE;
        }
        return FALSE;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        const ThemePalette& palette = CurrentPalette();
        SetTextColor(dc, palette.controlText);
        SetBkColor(dc, palette.windowBackground);
        return reinterpret_cast<LRESULT>(g_app.windowBrush);
    }
    case WM_CTLCOLORSCROLLBAR: {
        return reinterpret_cast<LRESULT>(g_app.controlBrush);
    }
    default: break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void OpenSettingsDialog() {
    if (g_app.settingsWindow) {
        SetForegroundWindow(g_app.settingsWindow);
        return;
    }
    WNDCLASSW klass{};
    klass.hInstance = g_app.instance;
    klass.lpfnWndProc = SettingsProc;
    klass.lpszClassName = L"MCnotepadSettings";
    klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    klass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&klass);
    g_app.settingsWindow = CreateWindowExW(WS_EX_DLGMODALFRAME, klass.lpszClassName,
        L"MCnotepad 偏好设置", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 470, 350, g_app.window, nullptr, g_app.instance, nullptr);
    if (!g_app.settingsWindow) return;
    EnableWindow(g_app.window, FALSE);
    ShowWindow(g_app.settingsWindow, SW_SHOW);
    UpdateWindow(g_app.settingsWindow);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (g_app.findReplaceMessage != 0 && message == g_app.findReplaceMessage) {
        HandleFindReplaceMessage(lParam);
        return 0;
    }
    switch (message) {
    case WM_CREATE: {
        g_app.window = window;
        SetMenu(window, CreateApplicationMenu());
        g_app.uiFont = CreateFontW(-9, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_app.uiBoldFont = CreateFontW(-9, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_app.editorFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FIXED_PITCH | FF_MODERN, L"Consolas");
        GdiplusStartupInput gdiplusInput;
        GdiplusStartup(&g_app.gdiplusToken, &gdiplusInput, nullptr);
        LoadToolbarIconImages();
        LoadToolbarIconFont();
        g_app.editorBrush = CreateSolidBrush(CurrentPalette().editorBackground);
        g_app.toolbarBrush = CreateSolidBrush(CurrentPalette().toolbarBackground);
        g_app.sidebarBrush = CreateSolidBrush(CurrentPalette().panelBackground);
        g_app.windowBrush = CreateSolidBrush(CurrentPalette().windowBackground);
        g_app.controlBrush = CreateSolidBrush(CurrentPalette().controlBackground);
        g_app.statusBrush = CreateSolidBrush(CurrentPalette().statusBackground);

        CreateToolbarControls(window);
        g_app.tabs = CreateWindowExW(0, WC_TABCONTROLW, nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS | TCS_SINGLELINE |
                TCS_FOCUSNEVER | TCS_OWNERDRAWFIXED,
            0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
        TabCtrl_SetItemSize(g_app.tabs, 184, kTabHeight - 6);
        SendMessageW(g_app.tabs, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        g_app.oldTabProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            g_app.tabs, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(TabProc)));

        g_app.newTabButton = CreateWindowExW(0, L"BUTTON", L"＋",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdNewTabButton)),
            g_app.instance, nullptr);
        SendMessageW(g_app.newTabButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);

        g_app.gutter = CreateWindowExW(0, L"MCGutter", nullptr,
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
        g_app.statusBar = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
        SendMessageW(g_app.statusBar, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        g_app.oldStatusProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            g_app.statusBar, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(StatusProc)));
        g_app.sidebarVisible = false;
        LoadLibraryW(L"Msftedit.dll");
        LoadPersistedSession();
        LayoutChildren(window);
        SetMenuState();
        SetTimer(window, kTimerAutosave, 2000, nullptr);

        if (g_app.previousRunWasUnclean) {
            MessageBoxW(window,
                L"检测到上次程序没有正常退出，已自动恢复上次会话和未保存内容。",
                kWindowTitle, MB_ICONINFORMATION);
        }
        return 0;
    }
    case WM_SIZE:
        LayoutChildren(window);
        return 0;
    case WM_TIMER:
        if (wParam == kTimerAutosave) {
            if (g_app.stateDirty) {
                PersistAll(false);
            }
            UpdateStatusBar();
        }
        return 0;
    case WM_COMMAND: {
        const int command = LOWORD(wParam);
        const int notification = HIWORD(wParam);
        HWND source = reinterpret_cast<HWND>(lParam);
        if (notification == EN_CHANGE && source != nullptr && !g_app.loading) {
            for (size_t index = 0; index < g_app.documents.size(); ++index) {
                if (g_app.documents[index].editor == source) {
                    const bool wasModified = g_app.documents[index].modified;
                    g_app.documents[index].modified = true;
                    MarkRecoveryDirty(static_cast<int>(index));
                    if (!wasModified) UpdateTabTitle(static_cast<int>(index));
                    UpdateStatusBar();
                    RefreshGutter();
                    break;
                }
            }
            return 0;
        }
        if (notification == EN_VSCROLL) {
            RefreshGutter();
            UpdateStatusBar();
            return 0;
        }
        if (notification == CBN_SELCHANGE) {
            if (source == g_app.languageCombo) {
                g_app.languageIndex = static_cast<int>(SendMessageW(source, CB_GETCURSEL, 0, 0));
                for (const TabDocument& document : g_app.documents) ApplySimpleSyntaxHighlight(document.editor);
                UpdateStatusBar();
                g_app.stateDirty = true;
                return 0;
            }
            if (source == g_app.encodingCombo) {
                g_app.encodingIndex = static_cast<int>(SendMessageW(source, CB_GETCURSEL, 0, 0));
                UpdateStatusBar();
                g_app.stateDirty = true;
                return 0;
            }
            if (source == g_app.lineEndingCombo) {
                g_app.lineEndingIndex = static_cast<int>(SendMessageW(source, CB_GETCURSEL, 0, 0));
                UpdateStatusBar();
                g_app.stateDirty = true;
                return 0;
            }
        }

        switch (command) {
        case kIdNewTabButton: NewDocument(); break;
        case kIdToolbarNew: NewDocument(); break;
        case kIdToolbarOpen: OpenFileDialog(); break;
        case kIdToolbarSave: SaveDocument(g_app.activeIndex, false); break;
        case kIdToolbarSaveAs: SaveDocument(g_app.activeIndex, true); break;
        case kIdToolbarClose: CloseDocument(g_app.activeIndex); break;
        case kIdToolbarUndo: HandleEditCommand(kIdEditUndo); break;
        case kIdToolbarRedo: HandleEditCommand(kIdEditRedo); break;
        case kIdToolbarFind: OpenFindDialog(false); break;
        case kIdToolbarReplace: OpenFindDialog(true); break;
        case kIdToolbarGoto: OpenFindDialog(false); break;
        case kIdToolbarLineNumbers:
            g_app.showLineNumbers = !g_app.showLineNumbers;
            LayoutChildren(window);
            break;
        case kIdToolbarWrap:
            g_app.wordWrap = !g_app.wordWrap;
            for (const TabDocument& document : g_app.documents) {
                LONG style = GetWindowLongW(document.editor, GWL_STYLE);
                style = g_app.wordWrap ? (style & ~WS_HSCROLL) : (style | WS_HSCROLL);
                SetWindowLongW(document.editor, GWL_STYLE, style);
            }
            LayoutChildren(window);
            break;
        case kIdToolbarTheme:
            ShowThemeMenu(source ? source : g_app.toolbar);
            break;
        case kIdSidebarClose:
            g_app.sidebarVisible = false;
            g_app.stateDirty = true;
            LayoutChildren(window);
            break;
        case kIdToolbarSettings:
            OpenSettingsDialog();
            break;
        case kIdThemeDark:
            SetThemeKind(ThemeKind::Dark);
            break;
        case kIdThemeLight:
            SetThemeKind(ThemeKind::Light);
            break;
        case kIdThemeGreen:
            SetThemeKind(ThemeKind::Green);
            break;
        case kIdThemeBlue:
            SetThemeKind(ThemeKind::Blue);
            break;
        case kIdFileNew: NewDocument(); break;
        case kIdFileOpen: OpenFileDialog(); break;
        case kIdFileSave: SaveDocument(g_app.activeIndex, false); break;
        case kIdFileSaveAs: SaveDocument(g_app.activeIndex, true); break;
        case kIdFileClose: CloseDocument(g_app.activeIndex); break;
        case kIdFileExit: SendMessageW(window, WM_CLOSE, 0, 0); break;
        case kIdHelpAbout:
            MessageBoxW(window,
                L"MCnotepad 0.1.0\n轻量、快速、可恢复的 Windows 文本编辑器。",
                kWindowTitle, MB_ICONINFORMATION);
            break;
        case kIdEditUndo:
        case kIdEditRedo:
        case kIdEditCut:
        case kIdEditCopy:
        case kIdEditPaste:
        case kIdEditSelectAll:
            HandleEditCommand(command);
            break;
        default:
            break;
        }
        SetMenuState();
        return 0;
    }
    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (item != nullptr) {
            if (item->CtlType == ODT_TAB) DrawTabItem(*item);
            else if (item->CtlType == ODT_COMBOBOX) DrawComboItem(*item);
            else if (item->CtlID >= kIdThemeDark && item->CtlID <= kIdThemeBlue) DrawThemeItem(*item);
            else DrawToolbarItem(*item);
        }
        return TRUE;
    }
    case WM_MEASUREITEM: {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (measure != nullptr && measure->CtlType == ODT_COMBOBOX) {
            measure->itemHeight = 24;
            return TRUE;
        }
        return FALSE;
    }
    case WM_ERASEBKGND: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        RECT client{};
        GetClientRect(window, &client);
        FillRect(dc, &client, g_app.windowBrush);
        return 1;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, EditorForeground());
        SetBkColor(dc, EditorBackground());
        return reinterpret_cast<LRESULT>(g_app.editorBrush);
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSCROLLBAR: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        const ThemePalette& palette = CurrentPalette();
        if (control == g_app.toolbar) {
            SetTextColor(dc, palette.toolbarText);
            SetBkColor(dc, palette.toolbarBackground);
            return reinterpret_cast<LRESULT>(g_app.toolbarBrush);
        }
        if (control == g_app.sidebar || GetParent(control) == g_app.sidebar) {
            SetTextColor(dc, palette.controlText);
            SetBkColor(dc, palette.panelBackground);
            return reinterpret_cast<LRESULT>(g_app.sidebarBrush);
        }
        if (control == g_app.statusBar) {
            SetTextColor(dc, palette.statusText);
            SetBkColor(dc, palette.statusBackground);
            return reinterpret_cast<LRESULT>(g_app.statusBrush);
        }
        SetTextColor(dc, palette.controlText);
        SetBkColor(dc, palette.controlBackground);
        return reinterpret_cast<LRESULT>(g_app.controlBrush);
    }
    case WM_NOTIFY: {
        const NMHDR* header = reinterpret_cast<const NMHDR*>(lParam);
        if (header != nullptr && header->hwndFrom == g_app.tabs && header->code == TCN_SELCHANGE) {
            SelectTab(TabCtrl_GetCurSel(g_app.tabs));
            SetMenuState();
        }
        return 0;
    }
    case WM_CLOSE:
        if (!g_app.closing) {
            if (!PrepareApplicationClose()) {
                return 0;
            }
            g_app.closing = true;
            KillTimer(window, kTimerAutosave);
            const bool persisted = PersistAll(true);
            if (!persisted) {
                MessageBoxW(window, L"会话恢复数据保存失败，程序将继续退出。", kWindowTitle, MB_ICONWARNING);
            }
            RemoveRunningMarker();
            DestroyWindow(window);
        }
        return 0;
    case WM_COPYDATA: {
        const auto* data = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        if (data == nullptr || data->dwData != kCopyDataOpenFiles) return FALSE;
        if (data->lpData != nullptr && data->cbData >= sizeof(wchar_t)) {
            const auto* chars = static_cast<const wchar_t*>(data->lpData);
            const size_t count = data->cbData / sizeof(wchar_t);
            size_t start = 0;
            for (size_t index = 0; index < count; ++index) {
                if (chars[index] != L'\0') continue;
                if (index > start) g_app.pendingOpenFiles.emplace_back(chars + start, index - start);
                start = index + 1;
            }
        }
        if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
        SetForegroundWindow(window);
        // Open asynchronously: the sender is blocked until we return, and opening
        // can show message boxes.
        PostMessageW(window, kMessageOpenPendingFiles, 0, 0);
        return TRUE;
    }
    case kMessageOpenPendingFiles: {
        std::vector<std::wstring> files;
        files.swap(g_app.pendingOpenFiles);
        for (const std::wstring& file : files) OpenFileInNewTab(fs::path(file));
        SetMenuState();
        return 0;
    }
    case WM_QUERYENDSESSION:
        // Windows is shutting down / restarting / logging off. WM_CLOSE will not
        // arrive, so flush every snapshot now without blocking on prompts.
        PersistAll(true);
        return TRUE;
    case WM_ENDSESSION:
        if (wParam) {
            g_app.closing = true;
            KillTimer(window, kTimerAutosave);
            PersistAll(true);
            RemoveRunningMarker();
        }
        return 0;
    case WM_DESTROY:
        DeleteObject(g_app.editorBrush);
        g_app.toolbarIconImages.clear();
        DeleteObject(g_app.toolbarIconFont);
        if (!g_app.toolbarIconFontPath.empty()) {
            RemoveFontResourceExW(g_app.toolbarIconFontPath.c_str(), FR_PRIVATE, nullptr);
        }
        DeleteObject(g_app.toolbarBrush);
        DeleteObject(g_app.sidebarBrush);
        DeleteObject(g_app.windowBrush);
        DeleteObject(g_app.controlBrush);
        DeleteObject(g_app.statusBrush);
        if (g_app.gdiplusToken != 0) {
            GdiplusShutdown(g_app.gdiplusToken);
            g_app.gdiplusToken = 0;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

// Called when another MCnotepad already owns the session. Hands our command-line
// files (as absolute paths) to it and brings it to the front. This process must
// not touch session.dat / running.lock, otherwise the two instances would
// overwrite each other's session and report bogus crashes.
void ForwardToRunningInstance() {
    std::wstring payload;
    int argumentCount = 0;
    if (LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount)) {
        for (int index = 1; index < argumentCount; ++index) {
            const std::wstring argument = arguments[index];
            if (argument.empty() || argument[0] == L'-') continue;
            std::error_code error;
            const fs::path absolute = fs::absolute(fs::path(argument), error);
            payload += error ? argument : absolute.wstring();
            payload.push_back(L'\0');
        }
        LocalFree(arguments);
    }
    payload.push_back(L'\0');

    // The first instance may still be starting up; give it a few seconds.
    HWND target = nullptr;
    for (int attempt = 0; attempt < 50 && target == nullptr; ++attempt) {
        target = FindWindowW(kWindowClass, nullptr);
        if (target == nullptr) Sleep(100);
    }
    if (target == nullptr) return;

    DWORD processId = 0;
    GetWindowThreadProcessId(target, &processId);
    AllowSetForegroundWindow(processId);
    COPYDATASTRUCT data{};
    data.dwData = kCopyDataOpenFiles;
    data.cbData = static_cast<DWORD>(payload.size() * sizeof(wchar_t));
    data.lpData = payload.data();
    DWORD_PTR result = 0;
    SendMessageTimeoutW(target, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data),
        SMTO_ABORTIFHUNG, 5000, &result);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand) {
    (void)commandLine;
    g_app.instance = instance;

    // Kept open for the lifetime of the process; the OS releases it on exit.
    HANDLE singleInstance = CreateMutexW(nullptr, FALSE, kSingleInstanceMutex);
    if (singleInstance != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        ForwardToRunningInstance();
        CloseHandle(singleInstance);
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g_app.findReplaceMessage = RegisterWindowMessageW(FINDMSGSTRING);
    InitializeStorage();

    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&commonControls);

    WNDCLASSW gutterClass{};
    gutterClass.hInstance = instance;
    gutterClass.lpfnWndProc = GutterProc;
    gutterClass.lpszClassName = L"MCGutter";
    gutterClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    gutterClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&gutterClass);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.lpszClassName = kWindowClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_MCNOTEPAD));
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassExW(&windowClass)) {
        MessageBoxW(nullptr, L"窗口注册失败。", kWindowTitle, MB_ICONERROR);
        RemoveRunningMarker();
        return 1;
    }

    HWND window = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1100,
        760,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!window) {
        MessageBoxW(nullptr, L"主窗口创建失败。", kWindowTitle, MB_ICONERROR);
        RemoveRunningMarker();
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);
    OpenCommandLineFiles();

    HACCEL accelerators = CreateAccelerators();
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!TranslateAcceleratorW(window, accelerators, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    DestroyAcceleratorTable(accelerators);

    return static_cast<int>(message.wParam);
}

