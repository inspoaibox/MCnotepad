#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dlgs.h>
#include <richedit.h>
#include <ole2.h>
#include <richole.h>
#include <tom.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>
#include <winreg.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <regex>
#include <wincrypt.h>
#include <cwctype>
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

#include <cpp-pinyin/Pinyin.h>
#include <cpp-pinyin/G2pglobal.h>
#include <SimpleConverter.hpp>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "crypt32.lib")
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
constexpr UINT kMessageCheckExternalChanges = WM_APP + 2;
constexpr UINT kMessageOpenMenuBar = WM_APP + 3;
constexpr int kMenuBarHeight = 30;
constexpr int kToolbarHeight = 36;
constexpr int kTabHeight = 34;
constexpr int kTabCloseHitWidth = 22;
constexpr int kTabCloseIconHalf = 4;
constexpr int kStatusHeight = 26;
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
constexpr int kIdFindNext = 1206;
constexpr int kIdFindPrevious = 1207;
constexpr int kIdFileRename = 1007;
// Text tools (Tools menu and editor right-click menu).
enum TextToolId : int {
    kIdTextToolFirst = 2001,
    kIdJoinLines = kIdTextToolFirst, kIdJoinLinesSpace, kIdJoinLinesCustom, kIdJoinEveryN,
    kIdSplitByDelimiter, kIdBreakAfter, kIdWrapWidth,
    kIdRemoveEmptyLines, kIdCollapseEmptyLines, kIdRemoveDuplicateLines,
    kIdSortAscending, kIdSortDescending, kIdReverseLines,
    kIdAddPrefix, kIdAddSuffix, kIdNumberLines,
    kIdTrimLeading, kIdTrimTrailing, kIdTrimBoth, kIdCollapseSpaces, kIdRemoveAllSpaces, kIdTabsToSpaces,
    kIdFullToHalf, kIdHalfToFull, kIdChinesePunctToAscii, kIdAsciiPunctToChinese, kIdRemoveInvisible,
    kIdUpperCase, kIdLowerCase, kIdTitleCase,
    kIdPinyinTone, kIdPinyinPlain, kIdPinyinInitials,
    kIdToTraditional, kIdToSimplified,
    kIdBase64Encode, kIdBase64Decode, kIdUrlEncode, kIdUrlDecode, kIdRegexReplace,
    kIdExtractEmails, kIdExtractUrls, kIdExtractIps, kIdExtractNumbers, kIdExtractKeywordLines, kIdExtractFields,
    kIdTogglePreview,
    kIdTextToolLast = kIdTogglePreview
};
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
constexpr int kIdEncodingFirst = 1601;    // 1601..1605
constexpr int kIdLineEndingFirst = 1611;  // 1611..1613
constexpr int kIdLanguageFirst = 1621;    // 1621..1630

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
    int encoding = 0;        // index into kEncodingNames
    int lineEnding = 0;      // index into kLineEndingNames
    int language = 0;        // index into kLanguageNames, 0 = detect from extension
    uint64_t fileTimestamp = 0;  // last write time we know about, 0 = unknown
};

constexpr const wchar_t* kEncodingNames[] = {L"UTF-8", L"UTF-8 BOM", L"UTF-16 LE", L"UTF-16 BE", L"ANSI"};
constexpr const wchar_t* kLineEndingNames[] = {L"CRLF", L"LF", L"CR"};
constexpr const wchar_t* kLineEndingMenuNames[] = {L"Windows (CRLF)", L"Unix (LF)", L"旧式 Mac (CR)"};
constexpr const wchar_t* kLanguageNames[] = {L"自动检测", L"HTML", L"CSS", L"JavaScript", L"C/C++",
    L"Python", L"JSON", L"Markdown", L"SQL", L"PowerShell"};
constexpr int kEncodingCount = 5;
constexpr int kLineEndingCount = 3;
constexpr int kLanguageCount = 10;
constexpr int kEncodingAnsi = 4;
constexpr int kLanguageAuto = 0;
constexpr int kLanguageHtml = 1;
constexpr int kLanguageJson = 6;
constexpr int kLanguageMarkdown = 7;
constexpr int kLanguagePlain = -1;

struct AppState {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    HWND tabs = nullptr;
    HWND newTabButton = nullptr;
    HWND statusBar = nullptr;
    HWND toolbar = nullptr;
    HWND menuBar = nullptr;
    HMENU mainMenu = nullptr;
    HWND gutter = nullptr;
    HWND gotoWindow = nullptr;
    HWND gotoEdit = nullptr;
    std::vector<HWND> toolbarButtons;
    HFONT editorFont = nullptr;
    HFONT uiFont = nullptr;
    ULONG_PTR gdiplusToken = 0;
    std::vector<std::unique_ptr<Bitmap>> toolbarIconImages;
    HFONT uiBoldFont = nullptr;
    std::wstring uiFontFace = L"Segoe UI";
    HBRUSH editorBrush = nullptr;
    HBRUSH sidebarBrush = nullptr;
    HBRUSH toolbarBrush = nullptr;
    HBRUSH windowBrush = nullptr;
    HBRUSH controlBrush = nullptr;
    HBRUSH statusBrush = nullptr;
    HMENU themeMenu = nullptr;
    WNDPROC oldTabProc = nullptr;
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
    DWORD findFlags = FR_DOWN;
    bool findDialogIsReplace = false;
    wchar_t replaceBuffer[256]{};
    std::vector<TabDocument> documents;
    std::vector<std::wstring> pendingOpenFiles;
    std::wstring statusNotice;
    bool previewTextTools = true;
    ULONGLONG statusNoticeUntil = 0;
    UINT dpi = 96;
    int gutterWidth = 42;
    RECT savedWindowRect{};
    bool savedWindowMaximized = false;
    bool hasSavedWindow = false;
    bool checkingExternalChanges = false;
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

// All layout constants are in 96-DPI pixels; scale them for the window's
// current monitor (the process is per-monitor DPI aware).
int Scale(int value) {
    return MulDiv(value, static_cast<int>(g_app.dpi), 96);
}


// RichEdit controls normally handle the standard edit shortcuts themselves,
// but accelerator translation can be bypassed when focus is inside a child
// control. Keep a small per-editor subclass so the shortcuts always reach the
// active editor, regardless of which child window currently owns focus.
std::unordered_map<HWND, WNDPROC> g_editorProcs;

std::wstring GetWindowTextValue(HWND window);
void ApplyEditorTheme(HWND editor);
void HighlightDocument(const TabDocument& document);
int EffectiveLanguage(const TabDocument& document);
void RefreshGutter();
void AppendEncodingItems(HMENU menu);
void CheckDocumentMenuItems(HMENU menu);
void ApplyWordWrapAll();
bool CloseDocument(int index);
void RenameDocument(int index);
void AppendTextToolMenus(HMENU menu);
void ThemeCheckbox(HWND box);
void ShowEditorContextMenu(HWND editor, POINT screen);
void UpdateHorizontalScrollbar(HWND editor);
void RefreshChrome();
void ApplyMenuColorMode();
void EnsureTabVisible(int index);
void InvalidateToolbar();

LRESULT CALLBACK EditorProc(HWND editor, UINT message, WPARAM wParam, LPARAM lParam);
void ApplyEditorFont();
void SetMenuState();

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
    for (const TabDocument& document : g_app.documents) {
        ApplyEditorTheme(document.editor);
        if (rehighlight) HighlightDocument(document);
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
    LONG ParagraphStart(LONG position) {
        long start = 0;
        if (range_ && SUCCEEDED(range_->SetRange(position, position)) &&
            SUCCEEDED(range_->StartOf(tomParagraph, tomMove, nullptr))) {
            range_->GetStart(&start);
        }
        return static_cast<LONG>(start);
    }

    void SetColor(LONG start, LONG end, COLORREF color) {
        if (!range_ || FAILED(range_->SetRange(start, end))) return;
        ITextFont* font = nullptr;
        if (SUCCEEDED(range_->GetFont(&font)) && font) {
            font->SetForeColor(static_cast<long>(color));
            font->Release();
        }
    }

private:
    ITextDocument* document_ = nullptr;
    ITextRange* range_ = nullptr;
};

void SetSelectionColor(HWND editor, LONG start, LONG end, COLORREF color) {
    CHARRANGE range{start, end};
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.crTextColor = color;
    SendMessageW(editor, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
}

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

// Lightweight colouring: HTML gets tags, comments and attribute strings; code
// languages get quoted strings (a string never runs past its line); plain text
// and Markdown are left alone so apostrophes don't colour whole paragraphs.
void ApplySimpleSyntaxHighlight(HWND editor, int language) {
    if (!editor) return;
    const std::wstring text = GetEditorRawText(editor);
    if (text.empty()) return;
    EditorEventMute mute(editor);
    TextDocumentRef tom(editor);
    long freezeCount = 0;
    if (tom) {
        // Colour changes are not edits: keep them off the undo stack, and
        // repaint once at the end instead of per token.
        tom.document()->Undo(tomSuspend, nullptr);
        tom.document()->Freeze(&freezeCount);
    } else {
        SendMessageW(editor, WM_SETREDRAW, FALSE, 0);
    }
    CHARRANGE saved{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&saved));
    POINT scroll{};
    SendMessageW(editor, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    const ThemePalette& palette = CurrentPalette();
    const auto color = [&](size_t start, size_t end, COLORREF value) {
        if (tom) tom.SetColor(static_cast<LONG>(start), static_cast<LONG>(end), value);
        else SetSelectionColor(editor, static_cast<LONG>(start), static_cast<LONG>(end), value);
    };
    color(0, text.size() + 1, EditorForeground());
    const auto lineEnd = [&](size_t from) {
        const size_t end = text.find_first_of(L"\r\n", from);
        return end == std::wstring::npos ? text.size() : end;
    };

    if (language == kLanguageHtml) {
        for (size_t i = 0; i < text.size();) {
            if (text.compare(i, 4, L"<!--") == 0) {
                const size_t end = text.find(L"-->", i + 4);
                const size_t finish = end == std::wstring::npos ? text.size() : end + 3;
                color(i, finish, palette.syntaxComment);
                i = finish;
            } else if (text[i] == L'<') {
                const size_t end = text.find(L'>', i + 1);
                const size_t finish = end == std::wstring::npos ? text.size() : end + 1;
                color(i, finish, palette.syntaxTag);
                // Attribute values inside the tag.
                for (size_t j = i + 1; j < finish; ++j) {
                    if (text[j] != L'"' && text[j] != L'\'') continue;
                    const size_t close = text.find(text[j], j + 1);
                    if (close == std::wstring::npos || close >= finish) break;
                    color(j, close + 1, palette.syntaxString);
                    j = close;
                }
                i = finish;
            } else {
                ++i;
            }
        }
    } else if (language != kLanguagePlain && language != kLanguageMarkdown) {
        const bool singleQuotes = language != kLanguageJson;
        for (size_t i = 0; i < text.size(); ++i) {
            const wchar_t quote = text[i];
            if (quote != L'"' && !(singleQuotes && (quote == L'\'' || quote == L'`'))) continue;
            const size_t limit = quote == L'`' ? text.size() : lineEnd(i + 1);
            size_t j = i + 1;
            while (j < limit && text[j] != quote) j += (text[j] == L'\\') ? 2 : 1;
            if (j >= limit) continue;  // unterminated: leave it uncoloured
            color(i, j + 1, palette.syntaxString);
            i = j;
        }
    }

    if (tom) {
        // The selection was never touched, so the caret (and the direction of
        // a Shift-selection) stays exactly as the user left it.
        tom.document()->Unfreeze(&freezeCount);
        tom.document()->Undo(tomResume, nullptr);
        return;
    }
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&saved));
    SendMessageW(editor, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    SendMessageW(editor, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(editor, nullptr, TRUE);
}

// Re-colouring after typing pauses. Very large documents are skipped to keep
// typing responsive; they are still coloured when opened.
constexpr UINT kTimerHighlight = 4102;
constexpr UINT kHighlightDelayMs = 300;
constexpr LONG kLiveHighlightLimit = 300000;

void HighlightDocument(const TabDocument& document) {
    ApplySimpleSyntaxHighlight(document.editor, EffectiveLanguage(document));
}

// Width needed for the active document's line numbers (at least 3 digits).
int GutterWidthFor(HWND editor) {
    const int lines = editor ? static_cast<int>(SendMessageW(editor, EM_GETLINECOUNT, 0, 0)) : 1;
    int digits = 3;
    for (int value = lines; value >= 1000; value /= 10) ++digits;
    HDC dc = GetDC(g_app.window);
    HFONT previous = static_cast<HFONT>(SelectObject(dc, g_app.editorFont ? g_app.editorFont
        : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT))));
    SIZE digit{};
    GetTextExtentPoint32W(dc, L"0", 1, &digit);
    SelectObject(dc, previous);
    ReleaseDC(g_app.window, dc);
    return digits * digit.cx + Scale(20);
}

// Numbers are drawn where RichEdit actually places each visible line, so they
// line up for any font size. With word wrap a long line spans several visual
// rows; only its first row gets a number, and numbers count real lines.
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
        HFONT font = g_app.editorFont ? g_app.editorFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HFONT previous = static_cast<HFONT>(SelectObject(dc, font));
        TEXTMETRICW metrics{};
        GetTextMetricsW(dc, &metrics);

        HWND editor = nullptr;
        if (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size())) {
            editor = g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor;
        }
        if (editor) {
            POINT origin{0, 0};
            MapWindowPoints(editor, window, &origin, 1);  // editor client -> gutter coordinates
            TextDocumentRef tom(editor);
            const auto paragraphOf = [&](LONG position) -> long {
                return tom ? tom.ParagraphIndex(position)
                           : static_cast<long>(SendMessageW(editor, EM_EXLINEFROMCHAR, 0, position)) + 1;
            };
            CHARRANGE selection{};
            SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
            const long caretParagraph = paragraphOf(selection.cpMax);
            const int lineCount = static_cast<int>(SendMessageW(editor, EM_GETLINECOUNT, 0, 0));
            const int firstVisual = static_cast<int>(SendMessageW(editor, EM_GETFIRSTVISIBLELINE, 0, 0));
            const LONG firstChar = static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, firstVisual, 0));
            long previousParagraph = firstChar > 0 ? paragraphOf(firstChar - 1) : 0;
            const int right = client.right - Scale(8);
            for (int visual = firstVisual; visual < lineCount; ++visual) {
                const LRESULT start = SendMessageW(editor, EM_LINEINDEX, visual, 0);
                if (start < 0) break;
                POINTL position{};
                SendMessageW(editor, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&position), start);
                const int y = position.y + origin.y;
                if (y > client.bottom) break;
                const long paragraph = paragraphOf(static_cast<LONG>(start));
                if (paragraph != previousParagraph) {
                    wchar_t number[16]{};
                    swprintf_s(number, L"%ld", paragraph);
                    SetTextColor(dc, paragraph == caretParagraph ? CurrentPalette().tabActiveText : GutterForeground());
                    RECT lineRect{0, y, right, y + metrics.tmHeight};
                    DrawTextW(dc, number, -1, &lineRect, DT_RIGHT | DT_SINGLELINE | DT_TOP | DT_NOCLIP);
                }
                previousParagraph = paragraph;
            }
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

// Decodes file bytes and reports which encoding was found:
// BOM (UTF-8 / UTF-16 LE / UTF-16 BE) first, then strict UTF-8, otherwise the
// system ANSI code page.
std::wstring DecodeTextBytes(const std::vector<char>& bytes, int* detectedEncoding = nullptr) {
    int ignored = 0;
    int& encoding = detectedEncoding ? *detectedEncoding : ignored;
    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        encoding = 2;
        const int wideBytes = static_cast<int>(bytes.size() - 2);
        return std::wstring(reinterpret_cast<const wchar_t*>(bytes.data() + 2),
            reinterpret_cast<const wchar_t*>(bytes.data() + 2) + wideBytes / 2);
    }

    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFE &&
        static_cast<unsigned char>(bytes[1]) == 0xFF) {
        encoding = 3;
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
    if (text.empty()) {
        encoding = offset == 3 ? 1 : g_app.encodingIndex;
        return {};
    }
    const int strictSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (strictSize > 0) {
        encoding = offset == 3 ? 1 : 0;
        std::wstring result(static_cast<size_t>(strictSize), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), result.data(), strictSize);
        return result;
    }
    encoding = offset == 3 ? 1 : kEncodingAnsi;
    const int fallbackSize = MultiByteToWideChar(offset == 3 ? CP_UTF8 : CP_ACP, 0,
        text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (fallbackSize <= 0) return {};
    std::wstring result(static_cast<size_t>(fallbackSize), L'\0');
    MultiByteToWideChar(offset == 3 ? CP_UTF8 : CP_ACP, 0, text.data(),
        static_cast<int>(text.size()), result.data(), fallbackSize);
    return result;
}

// The first line break decides the file's line-ending style.
int DetectLineEnding(std::wstring_view text, int fallback) {
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') return (i + 1 < text.size() && text[i + 1] == L'\n') ? 0 : 2;
        if (text[i] == L'\n') return 1;
    }
    return fallback;
}

bool ReadTextFile(const fs::path& path, std::wstring& text,
    int* encoding = nullptr, int* lineEnding = nullptr) {
    std::vector<char> bytes;
    if (!ReadBinaryFile(path, bytes)) {
        return false;
    }
    int detected = 0;
    text = DecodeTextBytes(bytes, &detected);
    if (encoding) *encoding = detected;
    if (lineEnding) *lineEnding = DetectLineEnding(text, g_app.lineEndingIndex);
    return true;
}

// True when every character survives a round trip through the ANSI code page.
bool IsAnsiRepresentable(std::wstring_view text) {
    if (text.empty()) return true;
    BOOL usedDefault = FALSE;
    const int size = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, &usedDefault);
    return size > 0 && !usedDefault;
}

int LanguageFromPath(const std::wstring& path) {
    std::wstring extension = fs::path(path).extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    if (extension == L".html" || extension == L".htm" || extension == L".xml" ||
        extension == L".xhtml" || extension == L".svg" || extension == L".vue") return kLanguageHtml;
    if (extension == L".css" || extension == L".scss" || extension == L".less") return 2;
    if (extension == L".js" || extension == L".mjs" || extension == L".cjs" ||
        extension == L".ts" || extension == L".jsx" || extension == L".tsx") return 3;
    if (extension == L".c" || extension == L".cc" || extension == L".cpp" || extension == L".cxx" ||
        extension == L".h" || extension == L".hpp" || extension == L".hh") return 4;
    if (extension == L".py" || extension == L".pyw") return 5;
    if (extension == L".json" || extension == L".jsonc") return kLanguageJson;
    if (extension == L".md" || extension == L".markdown") return kLanguageMarkdown;
    if (extension == L".sql") return 8;
    if (extension == L".ps1" || extension == L".psm1" || extension == L".psd1") return 9;
    return kLanguagePlain;
}

// Language actually used for highlighting/formatting: the user's choice, or the
// one implied by the file extension when set to "auto".
int EffectiveLanguage(const TabDocument& document) {
    if (document.language > kLanguageAuto && document.language < kLanguageCount) return document.language;
    return LanguageFromPath(document.path);
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


    if (g_app.tabs) InvalidateRect(g_app.tabs, nullptr, FALSE);
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
    // EM_LINESCROLL is relative, and EM_SETSEL may already have scrolled the
    // caret into view, so scroll by the remaining difference only.
    const int current = static_cast<int>(SendMessageW(document.editor, EM_GETFIRSTVISIBLELINE, 0, 0));
    if (current != document.firstVisibleLine) {
        SendMessageW(document.editor, EM_LINESCROLL, 0, document.firstVisibleLine - current);
    }
    RefreshGutter();
}

void UpdateStatusBar() {
    if (g_app.statusBar) InvalidateRect(g_app.statusBar, nullptr, FALSE);
    InvalidateToolbar();  // undo/redo availability follows edits
}

void LayoutChildren(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int menuHeight = Scale(kMenuBarHeight);
    const int toolbarHeight = Scale(kToolbarHeight);
    const int tabHeight = Scale(kTabHeight);
    const int statusHeight = Scale(kStatusHeight);
    const int tabY = menuHeight + toolbarHeight;
    const int contentY = tabY + tabHeight;
    const int contentHeight = std::max(0, height - contentY - statusHeight);
    const int editorWidth = width;
    HWND activeEditor = (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size()))
        ? g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor : nullptr;
    g_app.gutterWidth = GutterWidthFor(activeEditor);
    const int gutterWidth = g_app.showLineNumbers ? g_app.gutterWidth : 0;

    if (g_app.menuBar) MoveWindow(g_app.menuBar, 0, 0, width, menuHeight, TRUE);
    if (g_app.toolbar) MoveWindow(g_app.toolbar, 0, menuHeight, width, toolbarHeight, TRUE);
    if (g_app.tabs) MoveWindow(g_app.tabs, 0, tabY, width, tabHeight, TRUE);
    if (g_app.gutter) {
        MoveWindow(g_app.gutter, 0, contentY, gutterWidth, contentHeight, TRUE);
        ShowWindow(g_app.gutter, g_app.showLineNumbers ? SW_SHOW : SW_HIDE);
    }
    for (TabDocument& document : g_app.documents) {
        MoveWindow(document.editor, gutterWidth, contentY,
            std::max(0, editorWidth - gutterWidth), contentHeight, TRUE);
    }
    MoveWindow(g_app.statusBar, 0, std::max(0, height - statusHeight), width, statusHeight, TRUE);
    UpdateStatusBar();
    RefreshGutter();
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
    EnsureTabVisible(index);
    InvalidateToolbar();
    if (g_app.showLineNumbers && GutterWidthFor(document.editor) != g_app.gutterWidth) LayoutChildren(g_app.window);
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
           << static_cast<int>(g_app.closeBehavior) << '\t'
           << (g_app.previewTextTools ? 1 : 0) << "\n";

    // rcNormalPosition is the restored size even while maximized/minimized.
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    GetWindowPlacement(g_app.window, &placement);
    const RECT& windowRect = placement.rcNormalPosition;
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
               << document.fileTimestamp << '\t'
               << document.encoding << '\t'
               << document.lineEnding << '\t'
               << document.language << '\n';
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

    // This is a plain-text editor: every paste (menu, Ctrl+V, Shift+Insert)
    // takes only the clipboard's Unicode text, never rich formatting or images.
    if (message == WM_CONTEXTMENU) {
        ShowEditorContextMenu(editor, POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return 0;
    }
    if (message == WM_PASTE) {
        SendMessageW(editor, EM_PASTESPECIAL, CF_UNICODETEXT, 0);
        return 0;
    }
    if (message == WM_KEYDOWN && wParam == VK_INSERT && (GetKeyState(VK_SHIFT) & 0x8000) != 0 &&
        (GetKeyState(VK_CONTROL) & 0x8000) == 0) {
        SendMessageW(editor, WM_PASTE, 0, 0);
        return 0;
    }

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
    } else if (message == WM_SIZE || message == WM_SHOWWINDOW) {
        UpdateHorizontalScrollbar(editor);
    }
    return result;
}

void AddTabToControl(const std::wstring& title) {
    (void)title;
    if (g_app.tabs) InvalidateRect(g_app.tabs, nullptr, FALSE);
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

struct DocumentFormat {
    int encoding = -1;     // -1 = application default
    int lineEnding = -1;   // -1 = application default
    int language = kLanguageAuto;
    uint64_t fileTimestamp = 0;
};

// RichEdit wraps against the window when the target line width is 0 and never
// wraps when it is 1; the WS_HSCROLL style alone does not change wrapping.
void ApplyWordWrap(HWND editor) {
    if (!editor) return;
    SendMessageW(editor, EM_SETTARGETDEVICE, 0, g_app.wordWrap ? 0 : 1);
    const LONG style = GetWindowLongW(editor, GWL_STYLE);
    const LONG wanted = g_app.wordWrap ? (style & ~WS_HSCROLL) : (style | WS_HSCROLL);
    if (wanted == style) return;
    SetWindowLongW(editor, GWL_STYLE, wanted);
    SetWindowPos(editor, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

// RichEdit's no-wrap mode keeps a placeholder horizontal scrollbar (range
// 0-100, page 0) even when every line fits. Show it only when there is
// something to scroll.
void UpdateHorizontalScrollbar(HWND editor) {
    static bool updating = false;
    if (!editor || updating) return;
    updating = true;
    bool needed = false;
    if (!g_app.wordWrap) {
        SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE};
        if (GetScrollInfo(editor, SB_HORZ, &info)) {
            needed = info.nPage > 0 && info.nMax - info.nMin + 1 > static_cast<int>(info.nPage);
        }
    }
    ShowScrollBar(editor, SB_HORZ, needed ? TRUE : FALSE);
    updating = false;
}

void ApplyWordWrapAll() {
    for (const TabDocument& document : g_app.documents) {
        ApplyWordWrap(document.editor);
        UpdateHorizontalScrollbar(document.editor);
    }
    RefreshGutter();
}

int CreateDocument(std::wstring path, std::wstring title, std::wstring text,
    bool untitled, bool modified, int64_t caret, int64_t anchor, int firstVisibleLine,
    const fs::path* explicitSnapshot = nullptr, const DocumentFormat& format = {}) {
    TabDocument document;
    document.path = std::move(path);
    document.untitled = untitled;
    document.modified = modified;
    document.caret = caret;
    document.anchor = anchor;
    document.firstVisibleLine = firstVisibleLine;
    document.encoding = format.encoding >= 0 && format.encoding < kEncodingCount
        ? format.encoding : std::clamp(g_app.encodingIndex, 0, kEncodingCount - 1);
    document.lineEnding = format.lineEnding >= 0 && format.lineEnding < kLineEndingCount
        ? format.lineEnding : std::clamp(g_app.lineEndingIndex, 0, kLineEndingCount - 1);
    document.language = std::clamp(format.language, 0, kLanguageCount - 1);
    document.fileTimestamp = format.fileTimestamp;
    document.title = title.empty()
        ? (document.untitled ? std::wstring(L"Untitled-") + std::to_wstring(g_app.nextUntitledNumber++) : FileTitle(document.path))
        : std::move(title);
    if (document.untitled && document.title.rfind(L"Untitled-", 0) == 0) {
        try {
            const int number = std::stoi(document.title.substr(9));
            g_app.nextUntitledNumber = std::max(g_app.nextUntitledNumber, number + 1);
        } catch (...) {
        }
    }

    document.snapshotPath = (explicitSnapshot != nullptr && !explicitSnapshot->empty() &&
                             !IsSnapshotInUse(*explicitSnapshot))
        ? *explicitSnapshot
        : AllocateSnapshotPath();

    document.editor = CreateWindowExW(
        0,  // no 3D client edge: it drew a bright frame in dark themes
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
    SendMessageW(document.editor, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(Scale(6), 0));
    // With an Asian keyboard layout active (e.g. a Chinese IME), RichEdit's
    // auto-font / dual-font modes switch typed text to a proportional UI font.
    // Keep the editor font; CJK characters still render through font fallback.
    const LRESULT languageOptions = SendMessageW(document.editor, EM_GETLANGOPTIONS, 0, 0);
    SendMessageW(document.editor, EM_SETLANGOPTIONS, 0,
        languageOptions & ~static_cast<LRESULT>(IMF_AUTOFONT | IMF_DUALFONT | IMF_AUTOFONTSIZEADJUST));
    SetWindowTextW(document.editor, text.c_str());
    ApplyWordWrap(document.editor);
    const int language = EffectiveLanguage(document);
    if (language != kLanguagePlain && language != kLanguageMarkdown) HighlightDocument(document);
    SendMessageW(document.editor, EM_EMPTYUNDOBUFFER, 0, 0);  // loading is not an undoable edit
    SendMessageW(document.editor, EM_SETMODIFY, modified ? TRUE : FALSE, 0);
    // Enable change/scroll notifications only after the initial text is in,
    // so loading a file does not mark it as modified.
    SendMessageW(document.editor, EM_SETEVENTMASK, 0, kEditorEventMask);
    ShowWindow(document.editor, SW_HIDE);

    g_app.documents.push_back(std::move(document));
    AddTabToControl(g_app.documents.back().title);
    UpdateTabTitle(static_cast<int>(g_app.documents.size() - 1));
    if (!g_app.loading) LayoutChildren(g_app.window);  // session restore lays out once at the end
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
        } else if (document.title.rfind(L"Untitled-", 0) != 0) {
            wcsncpy_s(fileName, MAX_PATH, document.title.c_str(), _TRUNCATE);
        }

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = g_app.window;
        dialog.lpstrFilter = L"All files\0*.*\0Text files\0*.txt;*.md;*.log;*.json;*.cpp;*.h\0";
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
    if (document.encoding == kEncodingAnsi && !IsAnsiRepresentable(text)) {
        const int choice = MessageBoxW(g_app.window,
            L"当前内容包含 ANSI 编码无法表示的字符，按 ANSI 保存会变成“?”。\n"
            L"选择“是”改用 UTF-8 保存，“否”仍按 ANSI 保存。",
            kWindowTitle, MB_YESNOCANCEL | MB_ICONWARNING);
        if (choice == IDCANCEL) return false;
        if (choice == IDYES) document.encoding = 0;
    }
    if (!WriteTextFile(target, text, document.encoding, document.lineEnding)) {
        MessageBoxW(g_app.window, L"文件保存失败，请检查路径和权限。", kWindowTitle, MB_ICONERROR);
        return false;
    }

    const bool pathChanged = !SamePath(document.path, target);
    document.path = target.wstring();
    document.title = FileTitle(target);
    document.untitled = false;
    document.modified = false;
    document.recoveryDirty = true;
    document.fileTimestamp = FileTimestamp(target);
    SendMessageW(document.editor, EM_SETMODIFY, FALSE, 0);
    if (pathChanged && document.language == kLanguageAuto) HighlightDocument(document);
    UpdateTabTitle(index);
    UpdateStatusBar();
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
    DocumentFormat format;
    if (!ReadTextFile(path, text, &format.encoding, &format.lineEnding)) {
        MessageBoxW(g_app.window, L"文件打开失败，请检查路径和权限。", kWindowTitle, MB_ICONERROR);
        return false;
    }
    format.fileTimestamp = FileTimestamp(path);

    const int index = CreateDocument(path.wstring(), FileTitle(path), std::move(text),
        false, false, 0, 0, 0, nullptr, format);
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

bool CloseDocument(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) {
        return false;
    }

    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    if (!PrepareDocumentClose(document)) return false;

    const fs::path snapshot = document.snapshotPath;
    DestroyWindow(document.editor);
    g_app.documents.erase(g_app.documents.begin() + index);
    if (g_app.tabs) InvalidateRect(g_app.tabs, nullptr, FALSE);

    std::error_code error;
    fs::remove(snapshot, error);

    if (g_app.documents.empty()) {
        g_app.activeIndex = -1;
        NewDocument();
        return true;
    }

    const int nextIndex = std::min(index, static_cast<int>(g_app.documents.size()) - 1);
    g_app.activeIndex = -1;
    SelectTab(nextIndex);
    g_app.stateDirty = true;
    PersistAll(false);
    return true;
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
    HMENU menu = g_app.mainMenu;
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
    CheckDocumentMenuItems(menu);
    CheckMenuItem(menu, kIdTogglePreview, MF_BYCOMMAND | (g_app.previewTextTools ? MF_CHECKED : MF_UNCHECKED));
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
    AppendMenuW(file, MF_STRING, kIdFileRename, L"重命名...\tF2");
    AppendMenuW(file, MF_STRING, kIdFileClose, L"关闭标签\tCtrl+W");
    AppendMenuW(file, MF_STRING, kIdFileExit, L"退出");

    AppendMenuW(edit, MF_STRING, kIdEditUndo, L"撤销\tCtrl+Z");
    AppendMenuW(edit, MF_STRING, kIdEditRedo, L"重做\tCtrl+Y");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, kIdEditCut, L"剪切\tCtrl+X");
    AppendMenuW(edit, MF_STRING, kIdEditCopy, L"复制\tCtrl+C");
    AppendMenuW(edit, MF_STRING, kIdEditPaste, L"粘贴\tCtrl+V");
    AppendMenuW(edit, MF_STRING, kIdEditSelectAll, L"全选\tCtrl+A");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendTextToolMenus(edit);

    AppendMenuW(search, MF_STRING, kIdToolbarFind, L"查找\tCtrl+F");
    AppendMenuW(search, MF_STRING, kIdFindNext, L"查找下一个\tF3");
    AppendMenuW(search, MF_STRING, kIdFindPrevious, L"查找上一个\tShift+F3");
    AppendMenuW(search, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(search, MF_STRING, kIdToolbarReplace, L"替换\tCtrl+H");
    AppendMenuW(search, MF_STRING, kIdToolbarGoto, L"跳转\tCtrl+G");
    AppendMenuW(view, MF_STRING, kIdToolbarLineNumbers, L"行号");
    AppendMenuW(view, MF_STRING, kIdToolbarWrap, L"自动换行");
    AppendEncodingItems(encoding);
    for (int index = 0; index < kLanguageCount; ++index) {
        AppendMenuW(language, MF_STRING, kIdLanguageFirst + index, kLanguageNames[index]);
    }
    AppendMenuW(run, MF_STRING, kIdToolbarRun, L"运行当前文件\tF5");
    AppendMenuW(tools, MF_STRING, kIdToolbarFormat, L"格式化 JSON\tCtrl+Shift+F");
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

struct SessionEntry {
    std::wstring path;
    std::wstring title;
    fs::path snapshot;
    int64_t caret = 0;
    int64_t anchor = 0;
    int firstLine = 0;
    int horizontalScroll = 0;
    bool modified = false;
    bool untitled = false;
    DocumentFormat format;
};

void LoadPersistedSession() {
    std::ifstream input(g_app.sessionPath, std::ios::binary);
    if (!input) {
        ApplyEditorFont();
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
    std::vector<SessionEntry> entries;

    while (std::getline(buffer, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
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
            int preview = 1;
            if (parts.size() >= 4 && ParseInt(parts[3], preview)) g_app.previewTextTools = preview != 0;
        } else if (parts[0] == "WINDOW" && parts.size() >= 6) {
            int left = 0, top = 0, width = 0, height = 0, maximized = 0;
            if (ParseInt(parts[1], left) && ParseInt(parts[2], top) &&
                ParseInt(parts[3], width) && ParseInt(parts[4], height) && width >= 300 && height >= 200) {
                ParseInt(parts[5], maximized);
                g_app.savedWindowRect = RECT{left, top, left + width, top + height};
                g_app.savedWindowMaximized = maximized != 0;
                g_app.hasSavedWindow = true;
            }
        } else if (parts[0] == "TAB" && parts.size() >= 12) {
            SessionEntry entry;
            int sourceModified = 0;
            int untitled = 0;
            int64_t timestamp = 0;
            ParseInt64(parts[5], entry.caret);
            ParseInt64(parts[6], entry.anchor);
            ParseInt(parts[7], entry.firstLine);
            ParseInt(parts[8], entry.horizontalScroll);
            ParseInt(parts[9], sourceModified);
            ParseInt(parts[10], untitled);
            ParseInt64(parts[11], timestamp);
            entry.modified = sourceModified != 0;
            entry.untitled = untitled != 0;
            entry.format.fileTimestamp = static_cast<uint64_t>(timestamp);
            if (parts.size() >= 15) {  // per-tab format, added after 0.1.0
                ParseInt(parts[12], entry.format.encoding);
                ParseInt(parts[13], entry.format.lineEnding);
                ParseInt(parts[14], entry.format.language);
            }
            entry.path = Utf8ToWide(HexDecode(parts[2]));
            entry.title = Utf8ToWide(HexDecode(parts[3]));
            entry.snapshot = fs::path(Utf8ToWide(HexDecode(parts[4])));
            entries.push_back(std::move(entry));
        }
    }

    g_app.theme = static_cast<ThemeKind>(std::clamp(persistedTheme, 0, 3));
    g_app.showLineNumbers = persistedShowLineNumbers != 0;
    g_app.wordWrap = persistedWordWrap != 0;
    g_app.sidebarVisible = false;
    g_app.fontSize = std::clamp(persistedFontSize, 10, 32);
    if (!persistedFontFamily.empty()) g_app.fontFamily = persistedFontFamily;
    g_app.languageIndex = std::clamp(persistedLanguage, 0, kLanguageCount - 1);
    g_app.encodingIndex = std::clamp(persistedEncoding, 0, kEncodingCount - 1);
    g_app.lineEndingIndex = std::clamp(persistedLineEnding, 0, kLineEndingCount - 1);
    g_app.launchAtStartup = persistedStartup != 0;
    g_app.closeBehavior = static_cast<CloseBehavior>(std::clamp(persistedCloseBehavior, 0, 2));

    // Font and theme first, so every editor below is created with them.
    ApplyEditorFont();
    ApplyThemeToEditors(false);

    g_app.loading = true;
    for (SessionEntry& entry : entries) {
        std::wstring text;
        bool haveText = false;
        std::error_code error;
        const bool hasFile = !entry.untitled && !entry.path.empty() && fs::exists(entry.path, error);
        if (!entry.modified && hasFile) {
            // Nothing unsaved: the file on disk is the truth (it may have been
            // edited elsewhere since last time). Re-detect its format too.
            int encoding = 0;
            int lineEnding = 0;
            if (ReadTextFile(entry.path, text, &encoding, &lineEnding)) {
                haveText = true;
                entry.format.encoding = encoding;
                entry.format.lineEnding = lineEnding;
                entry.format.fileTimestamp = FileTimestamp(entry.path);
            }
        }
        if (!haveText && !entry.snapshot.empty() && fs::exists(entry.snapshot, error)) {
            haveText = ReadTextFile(entry.snapshot, text);
        }
        if (!haveText && hasFile) {
            int encoding = 0;
            int lineEnding = 0;
            if (ReadTextFile(entry.path, text, &encoding, &lineEnding)) {
                if (entry.format.encoding < 0) entry.format.encoding = encoding;
                if (entry.format.lineEnding < 0) entry.format.lineEnding = lineEnding;
                entry.format.fileTimestamp = FileTimestamp(entry.path);
            }
        }
        // Old sessions have no per-tab format: guess it from the file itself.
        if ((entry.format.encoding < 0 || entry.format.lineEnding < 0) && hasFile) {
            std::wstring ignored;
            int encoding = 0;
            int lineEnding = 0;
            if (ReadTextFile(entry.path, ignored, &encoding, &lineEnding)) {
                if (entry.format.encoding < 0) entry.format.encoding = encoding;
                if (entry.format.lineEnding < 0) entry.format.lineEnding = lineEnding;
            }
        }

        const int index = CreateDocument(entry.path, entry.title, std::move(text), entry.untitled,
            entry.modified, entry.caret, entry.anchor, entry.firstLine, &entry.snapshot, entry.format);
        if (index >= 0) {
            TabDocument& created = g_app.documents[static_cast<size_t>(index)];
            created.horizontalScroll = entry.horizontalScroll;
            // A tab that had to get a fresh snapshot (missing or shared by an
            // earlier tab in an old session) must write it on the next autosave.
            created.recoveryDirty = !SamePath(created.snapshotPath, entry.snapshot);
        }
    }
    g_app.loading = false;
    RemoveOrphanSnapshots();
    LayoutChildren(g_app.window);

    if (g_app.documents.empty()) {
        NewDocument();
        return;
    }

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

HWND ActiveEditor() {
    if (g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) return nullptr;
    return g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor;
}

void ReportNotFound(const wchar_t* needle) {
    const std::wstring message = L"找不到“" + std::wstring(needle) + L"”";
    MessageBoxW(g_app.findDialog ? g_app.findDialog : g_app.window, message.c_str(), kWindowTitle, MB_ICONINFORMATION);
}

// Searches from the current selection in the given direction, wrapping around
// the document once. Honors FR_MATCHCASE / FR_WHOLEWORD / FR_DOWN.
bool FindNextText(const wchar_t* needle, DWORD flags, bool reportMissing = true) {
    HWND editor = ActiveEditor();
    if (!needle || !*needle || !editor) return false;
    const WPARAM searchFlags = flags & (FR_DOWN | FR_MATCHCASE | FR_WHOLEWORD);
    const bool down = (flags & FR_DOWN) != 0;
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));

    FINDTEXTEXW query{};
    query.lpstrText = needle;
    // Down: from the end of the selection to the end. Up: from the start of the
    // selection back to the beginning (cpMax < cpMin means "search backwards").
    query.chrg = down ? CHARRANGE{selection.cpMax, -1} : CHARRANGE{selection.cpMin, 0};
    LRESULT found = SendMessageW(editor, EM_FINDTEXTEXW, searchFlags, reinterpret_cast<LPARAM>(&query));
    if (found < 0) {
        query.chrg = down ? CHARRANGE{0, -1} : CHARRANGE{-1, 0};
        if (!down) {
            GETTEXTLENGTHEX lengthQuery{GTL_NUMCHARS | GTL_PRECISE, 1200};
            query.chrg.cpMin = static_cast<LONG>(SendMessageW(editor, EM_GETTEXTLENGTHEX,
                reinterpret_cast<WPARAM>(&lengthQuery), 0));
        }
        found = SendMessageW(editor, EM_FINDTEXTEXW, searchFlags, reinterpret_cast<LPARAM>(&query));
    }
    if (found < 0) {
        if (reportMissing) ReportNotFound(needle);
        return false;
    }
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&query.chrgText));
    SendMessageW(editor, EM_SCROLLCARET, 0, 0);
    RefreshGutter();
    return true;
}

bool SelectionMatches(HWND editor, const wchar_t* needle, DWORD flags) {
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const LONG length = selection.cpMax - selection.cpMin;
    if (length <= 0 || length != static_cast<LONG>(wcslen(needle))) return false;
    std::wstring selected(static_cast<size_t>(length) + 1, L'\0');
    const LRESULT copied = SendMessageW(editor, EM_GETSELTEXT, 0, reinterpret_cast<LPARAM>(selected.data()));
    selected.resize(static_cast<size_t>(std::max<LRESULT>(0, copied)));
    return CompareStringOrdinal(selected.c_str(), static_cast<int>(selected.size()),
        needle, -1, (flags & FR_MATCHCASE) ? FALSE : TRUE) == CSTR_EQUAL;
}

// Builds the replaced text in memory and applies it as one edit, so a single
// Ctrl+Z undoes the whole "replace all" and large files stay fast.
int ReplaceAllText(const wchar_t* needle, const wchar_t* replacement, DWORD flags) {
    HWND editor = ActiveEditor();
    if (!needle || !*needle || !editor) return 0;
    const std::wstring text = GetEditorRawText(editor);
    const WPARAM searchFlags = FR_DOWN | (flags & (FR_MATCHCASE | FR_WHOLEWORD));
    std::vector<CHARRANGE> matches;
    FINDTEXTEXW query{};
    query.lpstrText = needle;
    query.chrg = CHARRANGE{0, -1};
    while (SendMessageW(editor, EM_FINDTEXTEXW, searchFlags, reinterpret_cast<LPARAM>(&query)) >= 0) {
        if (query.chrgText.cpMax <= query.chrgText.cpMin) break;
        matches.push_back(query.chrgText);
        query.chrg.cpMin = query.chrgText.cpMax;
    }
    if (matches.empty()) return 0;

    std::wstring result;
    result.reserve(text.size());
    size_t cursor = 0;
    for (const CHARRANGE& match : matches) {
        const size_t start = std::min(static_cast<size_t>(match.cpMin), text.size());
        const size_t end = std::min(static_cast<size_t>(match.cpMax), text.size());
        result.append(text, cursor, start - cursor);
        result += replacement;
        cursor = end;
    }
    result.append(text, cursor, std::wstring::npos);

    POINT scroll{};
    SendMessageW(editor, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    SendMessageW(editor, EM_SETSEL, 0, -1);
    SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(result.c_str()));
    SendMessageW(editor, EM_SETSEL, 0, 0);
    SendMessageW(editor, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    RefreshGutter();
    return static_cast<int>(matches.size());
}

void OpenFindDialog(bool replace) {
    // Seed the search box with a short single-line selection, like Notepad.
    if (HWND editor = ActiveEditor()) {
        CHARRANGE selection{};
        SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
        const LONG length = selection.cpMax - selection.cpMin;
        if (length > 0 && length < static_cast<LONG>(std::size(g_app.findBuffer))) {
            std::wstring selected(static_cast<size_t>(length) + 1, L'\0');
            const LRESULT copied = SendMessageW(editor, EM_GETSELTEXT, 0, reinterpret_cast<LPARAM>(selected.data()));
            selected.resize(static_cast<size_t>(std::max<LRESULT>(0, copied)));
            if (!selected.empty() && selected.find_first_of(L"\r\n") == std::wstring::npos) {
                wcsncpy_s(g_app.findBuffer, selected.c_str(), _TRUNCATE);
            }
        }
    }

    // Reuse an open dialog of the same kind; switch kinds by recreating it.
    if (g_app.findDialog) {
        if (g_app.findDialogIsReplace == replace) {
            SetDlgItemTextW(g_app.findDialog, edt1, g_app.findBuffer);
            SetForegroundWindow(g_app.findDialog);
            SetFocus(GetDlgItem(g_app.findDialog, edt1));
            return;
        }
        DestroyWindow(g_app.findDialog);
        g_app.findDialog = nullptr;
    }

    ZeroMemory(&g_app.findReplace, sizeof(g_app.findReplace));
    g_app.findReplace.lStructSize = sizeof(g_app.findReplace);
    g_app.findReplace.hwndOwner = g_app.window;
    g_app.findReplace.lpstrFindWhat = g_app.findBuffer;
    g_app.findReplace.wFindWhatLen = static_cast<WORD>(std::size(g_app.findBuffer));
    g_app.findReplace.Flags = g_app.findFlags;
    g_app.findDialogIsReplace = replace;
    if (replace) {
        g_app.findReplace.lpstrReplaceWith = g_app.replaceBuffer;
        g_app.findReplace.wReplaceWithLen = static_cast<WORD>(std::size(g_app.replaceBuffer));
        g_app.findDialog = ReplaceTextW(&g_app.findReplace);
    } else {
        g_app.findDialog = FindTextW(&g_app.findReplace);
    }
}

// F3 / Shift+F3: repeat the last search without the dialog.
void FindAgain(bool forward) {
    if (g_app.findBuffer[0] == L'\0') {
        OpenFindDialog(false);
        return;
    }
    DWORD flags = g_app.findFlags & (FR_MATCHCASE | FR_WHOLEWORD);
    if (forward) flags |= FR_DOWN;
    FindNextText(g_app.findBuffer, flags);
}

void HandleFindReplaceMessage(LPARAM lParam) {
    auto* data = reinterpret_cast<FINDREPLACEW*>(lParam);
    if (!data) return;
    if (data->Flags & FR_DIALOGTERM) {
        g_app.findDialog = nullptr;
        return;
    }
    g_app.findFlags = data->Flags & (FR_DOWN | FR_MATCHCASE | FR_WHOLEWORD);
    if (data->Flags & FR_FINDNEXT) {
        FindNextText(data->lpstrFindWhat, data->Flags);
    }
    if (data->Flags & FR_REPLACE) {
        // Replace only if the selection really is a match; otherwise the first
        // press just finds the next match (same as Notepad).
        HWND editor = ActiveEditor();
        if (editor && SelectionMatches(editor, data->lpstrFindWhat, data->Flags)) {
            SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(data->lpstrReplaceWith));
        }
        FindNextText(data->lpstrFindWhat, data->Flags | FR_DOWN);
    }
    if (data->Flags & FR_REPLACEALL) {
        const int count = ReplaceAllText(data->lpstrFindWhat, data->lpstrReplaceWith, data->Flags);
        const std::wstring message = count > 0
            ? L"已替换 " + std::to_wstring(count) + L" 处。"
            : L"找不到“" + std::wstring(data->lpstrFindWhat) + L"”";
        MessageBoxW(g_app.findDialog ? g_app.findDialog : g_app.window, message.c_str(), kWindowTitle, MB_ICONINFORMATION);
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
        { FCONTROL | FSHIFT | FVIRTKEY, 'F', kIdToolbarFormat },
        { FVIRTKEY, VK_F5, kIdToolbarRun },
        { FVIRTKEY, VK_F2, kIdFileRename },
        { FCONTROL | FVIRTKEY, VK_OEM_COMMA, kIdToolbarSettings },
        { FVIRTKEY, VK_F3, kIdFindNext },
        { FSHIFT | FVIRTKEY, VK_F3, kIdFindPrevious }
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

bool FontFamilyExists(const wchar_t* family) {
    HDC dc = GetDC(nullptr);
    LOGFONTW query{};
    query.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(query.lfFaceName, family, _TRUNCATE);
    bool found = false;
    EnumFontFamiliesExW(dc, &query, [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM flag) -> int {
        *reinterpret_cast<bool*>(flag) = true;
        return 0;
    }, reinterpret_cast<LPARAM>(&found), 0);
    ReleaseDC(nullptr, dc);
    return found;
}

// 9 pt UI font at the window's DPI (the old -9 px font was about 7 pt).
void CreateUiFonts() {
    HFONT oldFont = g_app.uiFont;
    HFONT oldBold = g_app.uiBoldFont;
    const int height = -MulDiv(9, static_cast<int>(g_app.dpi), 72);
    // Microsoft YaHei UI is designed for Chinese UI text and has good Latin
    // glyphs; Segoe UI (no CJK) is the fallback on systems without it.
    const wchar_t* face = FontFamilyExists(L"Microsoft YaHei UI") ? L"Microsoft YaHei UI" : L"Segoe UI";
    g_app.uiFontFace = face;
    g_app.uiFont = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, face);
    g_app.uiBoldFont = CreateFontW(height, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, face);
    if (oldFont) DeleteObject(oldFont);
    if (oldBold) DeleteObject(oldBold);
}

void ApplyEditorFont() {
    DeleteObject(g_app.editorFont);
    const int dpi = static_cast<int>(g_app.dpi);
    const int fontHeight = -MulDiv(g_app.fontSize, dpi, 72);
    g_app.editorFont = CreateFontW(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, g_app.fontFamily.c_str());
    for (const TabDocument& document : g_app.documents) {
        EditorEventMute mute(document.editor);
        SendMessageW(document.editor, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
        SendMessageW(document.editor, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(Scale(6), 0));
        InvalidateRect(document.editor, nullptr, TRUE);
    }
    RefreshGutter();
}

void CreateSettingsControl(HWND parent, const wchar_t* className, const wchar_t* text,
    DWORD style, int id, int x, int y, int width, int height) {
    HWND control = CreateWindowExW(0, className, text, WS_CHILD | WS_VISIBLE | style,
        Scale(x), Scale(y), Scale(width), Scale(height), parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
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
        {
            const LRESULT fontIndex = SendMessageW(g_app.settingsFontCombo, CB_FINDSTRINGEXACT,
                static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(g_app.fontFamily.c_str()));
            SendMessageW(g_app.settingsFontCombo, CB_SETCURSEL, fontIndex == CB_ERR ? 0 : fontIndex, 0);
        }
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
        if (LOWORD(wParam) == kIdSettingsApply || LOWORD(wParam) == IDOK) {
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
            ApplyWordWrapAll();
            ApplyEditorFont();
            LayoutChildren(g_app.window);
            g_app.stateDirty = true;
            PersistAll(false);
            EnableWindow(g_app.window, TRUE);
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wParam) == kIdSettingsCancel || LOWORD(wParam) == IDCANCEL) {
            EnableWindow(g_app.window, TRUE);
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
            measure->itemHeight = static_cast<UINT>(Scale(24));
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
        CW_USEDEFAULT, CW_USEDEFAULT, Scale(470), Scale(350), g_app.window, nullptr, g_app.instance, nullptr);
    if (!g_app.settingsWindow) return;
    EnableWindow(g_app.window, FALSE);
    ApplyWindowFrameTheme(g_app.settingsWindow);
    ShowWindow(g_app.settingsWindow, SW_SHOW);
    UpdateWindow(g_app.settingsWindow);
}

TabDocument* ActiveDocument() {
    if (g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) return nullptr;
    return &g_app.documents[static_cast<size_t>(g_app.activeIndex)];
}

void MarkActiveModified() {
    TabDocument* document = ActiveDocument();
    if (!document) return;
    const bool wasModified = document->modified;
    document->modified = true;
    MarkRecoveryDirty(g_app.activeIndex);
    if (!wasModified) UpdateTabTitle(g_app.activeIndex);
}

// Changing the encoding or line ending changes the bytes that will be written,
// so the tab becomes modified until it is saved again.
void SetActiveEncoding(int encoding) {
    TabDocument* document = ActiveDocument();
    if (!document || encoding < 0 || encoding >= kEncodingCount || document->encoding == encoding) return;
    document->encoding = encoding;
    MarkActiveModified();
    UpdateStatusBar();
}

void SetActiveLineEnding(int lineEnding) {
    TabDocument* document = ActiveDocument();
    if (!document || lineEnding < 0 || lineEnding >= kLineEndingCount || document->lineEnding == lineEnding) return;
    document->lineEnding = lineEnding;
    MarkActiveModified();
    UpdateStatusBar();
}

void SetActiveLanguage(int language) {
    TabDocument* document = ActiveDocument();
    if (!document || language < 0 || language >= kLanguageCount) return;
    document->language = language;
    HighlightDocument(*document);
    g_app.stateDirty = true;
    UpdateStatusBar();
}

void AppendEncodingItems(HMENU menu) {
    for (int index = 0; index < kEncodingCount; ++index) {
        AppendMenuW(menu, MF_STRING, kIdEncodingFirst + index, kEncodingNames[index]);
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    for (int index = 0; index < kLineEndingCount; ++index) {
        AppendMenuW(menu, MF_STRING, kIdLineEndingFirst + index, kLineEndingMenuNames[index]);
    }
}

void CheckDocumentMenuItems(HMENU menu) {
    const TabDocument* document = ActiveDocument();
    if (!menu || !document) return;
    CheckMenuRadioItem(menu, kIdEncodingFirst, kIdEncodingFirst + kEncodingCount - 1,
        kIdEncodingFirst + document->encoding, MF_BYCOMMAND);
    CheckMenuRadioItem(menu, kIdLineEndingFirst, kIdLineEndingFirst + kLineEndingCount - 1,
        kIdLineEndingFirst + document->lineEnding, MF_BYCOMMAND);
    CheckMenuRadioItem(menu, kIdLanguageFirst, kIdLanguageFirst + kLanguageCount - 1,
        kIdLanguageFirst + document->language, MF_BYCOMMAND);
}

void ShowEncodingMenu(const RECT& anchor) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendEncodingItems(menu);
    CheckDocumentMenuItems(menu);
    TPMPARAMS params{sizeof(params), anchor};
    const int command = TrackPopupMenuEx(menu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_VERTICAL,
        anchor.left, anchor.bottom, g_app.window, &params);
    DestroyMenu(menu);
    if (command != 0) SendMessageW(g_app.window, WM_COMMAND, MAKEWPARAM(command, 0), 0);
}

void ReloadDocumentFromDisk(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) return;
    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    std::wstring text;
    int encoding = 0;
    int lineEnding = 0;
    if (!ReadTextFile(document.path, text, &encoding, &lineEnding)) {
        MessageBoxW(g_app.window, L"重新加载失败，请检查文件权限。", kWindowTitle, MB_ICONERROR);
        return;
    }
    CHARRANGE selection{};
    SendMessageW(document.editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    {
        EditorEventMute mute(document.editor);
        SetWindowTextW(document.editor, text.c_str());
        SendMessageW(document.editor, EM_SETMODIFY, FALSE, 0);
    }
    document.encoding = encoding;
    document.lineEnding = lineEnding;
    document.modified = false;
    document.fileTimestamp = FileTimestamp(document.path);
    MarkRecoveryDirty(index);
    HighlightDocument(document);
    SendMessageW(document.editor, EM_EMPTYUNDOBUFFER, 0, 0);
    const LONG length = GetWindowTextLengthW(document.editor);
    selection.cpMin = std::min(selection.cpMin, length);
    selection.cpMax = std::min(selection.cpMax, length);
    SendMessageW(document.editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    UpdateTabTitle(index);
    UpdateStatusBar();
    RefreshGutter();
}

// Compares each file's last write time with the one we loaded/saved. Runs when
// the app is activated, so edits made in other programs are noticed.
void CheckExternalChanges() {
    if (g_app.checkingExternalChanges || g_app.closing) return;
    g_app.checkingExternalChanges = true;
    for (size_t index = 0; index < g_app.documents.size(); ++index) {
        TabDocument& document = g_app.documents[index];
        if (document.untitled || document.path.empty()) continue;
        std::error_code error;
        const std::wstring path = document.path;
        if (!fs::exists(path, error)) {
            if (document.fileTimestamp == 0) continue;  // already reported
            document.fileTimestamp = 0;
            if (!document.modified) {
                document.modified = true;
                MarkRecoveryDirty(static_cast<int>(index));
                UpdateTabTitle(static_cast<int>(index));
            }
            SelectTab(static_cast<int>(index));
            const std::wstring message = L"文件已被删除或移动：\n" + path +
                L"\n\n内容仍保留在当前标签中，保存即可重新写回磁盘。";
            MessageBoxW(g_app.window, message.c_str(), kWindowTitle, MB_ICONWARNING);
            continue;
        }
        const uint64_t timestamp = FileTimestamp(path);
        if (timestamp == 0 || timestamp == document.fileTimestamp) continue;
        if (document.fileTimestamp == 0) {  // unknown before (old session, re-created file)
            document.fileTimestamp = timestamp;
            continue;
        }
        const bool modified = document.modified;
        document.fileTimestamp = timestamp;  // ask only once per external change
        SelectTab(static_cast<int>(index));
        const std::wstring message = modified
            ? L"文件已在其他程序中被修改：\n" + path +
              L"\n\n当前标签也有未保存的修改，重新加载会丢弃这些修改。是否重新加载？"
            : L"文件已在其他程序中被修改：\n" + path + L"\n\n是否重新加载？";
        const int choice = MessageBoxW(g_app.window, message.c_str(), kWindowTitle,
            MB_YESNO | MB_ICONQUESTION | (modified ? MB_DEFBUTTON2 : 0));
        if (choice == IDYES) ReloadDocumentFromDisk(static_cast<int>(index));
    }
    g_app.checkingExternalChanges = false;
    SetMenuState();
}

// ---- Go to line -----------------------------------------------------------

constexpr int kIdGotoEdit = 1901;

void CloseGotoDialog() {
    if (!g_app.gotoWindow) return;
    EnableWindow(g_app.window, TRUE);  // before destroying, so focus returns here
    DestroyWindow(g_app.gotoWindow);
}

LRESULT CALLBACK GotoProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        const TabDocument* document = ActiveDocument();
        const int lineCount = document
            ? static_cast<int>(SendMessageW(document->editor, EM_GETLINECOUNT, 0, 0)) : 1;
        int currentLine = 1;
        if (document) {
            CHARRANGE selection{};
            SendMessageW(document->editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
            currentLine = static_cast<int>(SendMessageW(document->editor, EM_EXLINEFROMCHAR, 0, selection.cpMax)) + 1;
        }
        wchar_t label[64]{};
        swprintf_s(label, L"行号（1 - %d）：", lineCount);
        HWND text = CreateWindowExW(0, L"STATIC", label, WS_CHILD | WS_VISIBLE,
            Scale(16), Scale(18), Scale(260), Scale(20), window, nullptr, g_app.instance, nullptr);
        g_app.gotoEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(currentLine).c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL,
            Scale(16), Scale(42), Scale(280), Scale(26), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdGotoEdit)),
            g_app.instance, nullptr);
        HWND ok = CreateWindowExW(0, L"BUTTON", L"跳转", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            Scale(126), Scale(82), Scale(82), Scale(28), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), g_app.instance, nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(214), Scale(82), Scale(82), Scale(28), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), g_app.instance, nullptr);
        for (HWND control : {text, g_app.gotoEdit, ok, cancel}) {
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        }
        SendMessageW(g_app.gotoEdit, EM_SETSEL, 0, -1);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            TabDocument* document = ActiveDocument();
            wchar_t buffer[32]{};
            GetWindowTextW(g_app.gotoEdit, buffer, static_cast<int>(std::size(buffer)));
            const int requested = _wtoi(buffer);
            if (document && requested > 0) {
                const int lineCount = static_cast<int>(SendMessageW(document->editor, EM_GETLINECOUNT, 0, 0));
                const int line = std::clamp(requested, 1, std::max(1, lineCount));
                const LRESULT position = SendMessageW(document->editor, EM_LINEINDEX, line - 1, 0);
                if (position >= 0) {
                    SendMessageW(document->editor, EM_SETSEL, position, position);
                    SendMessageW(document->editor, EM_SCROLLCARET, 0, 0);
                }
                CloseGotoDialog();
                SetFocus(document->editor);
                RefreshGutter();
                UpdateStatusBar();
            } else {
                MessageBeep(MB_ICONWARNING);
            }
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            CloseGotoDialog();
            if (TabDocument* document = ActiveDocument()) SetFocus(document->editor);
            return 0;
        }
        break;
    case WM_CLOSE:
        CloseGotoDialog();
        return 0;
    case WM_DESTROY:
        g_app.gotoWindow = nullptr;
        g_app.gotoEdit = nullptr;
        EnableWindow(g_app.window, TRUE);
        return 0;
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(window, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, g_app.windowBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, CurrentPalette().controlText);
        SetBkColor(dc, CurrentPalette().windowBackground);
        return reinterpret_cast<LRESULT>(g_app.windowBrush);
    }
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void OpenGotoDialog() {
    if (g_app.gotoWindow) {
        SetForegroundWindow(g_app.gotoWindow);
        return;
    }
    if (!ActiveDocument()) return;
    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{};
        klass.hInstance = g_app.instance;
        klass.lpfnWndProc = GotoProc;
        klass.lpszClassName = L"MCnotepadGoto";
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        registered = RegisterClassW(&klass) != 0;
    }
    RECT owner{};
    GetWindowRect(g_app.window, &owner);
    const int width = Scale(330);
    const int height = Scale(170);
    g_app.gotoWindow = CreateWindowExW(WS_EX_DLGMODALFRAME, L"MCnotepadGoto", L"跳转到行",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        owner.left + (owner.right - owner.left - width) / 2, owner.top + Scale(120), width, height,
        g_app.window, nullptr, g_app.instance, nullptr);
    if (!g_app.gotoWindow) return;
    EnableWindow(g_app.window, FALSE);
    ApplyWindowFrameTheme(g_app.gotoWindow);
    ShowWindow(g_app.gotoWindow, SW_SHOW);
    SetFocus(g_app.gotoEdit);
}

// ---- Format ---------------------------------------------------------------

int LineOfOffset(std::wstring_view text, size_t offset) {
    int line = 1;
    for (size_t i = 0; i < offset && i < text.size(); ++i) {
        if (text[i] == L'\n' || (text[i] == L'\r' && (i + 1 >= text.size() || text[i + 1] != L'\n'))) ++line;
    }
    return line;
}

// Re-indents JSON with 4 spaces. Only brackets and strings are checked; it is a
// pretty-printer, not a full validator.
bool FormatJson(std::wstring_view input, std::wstring& output, std::wstring& error) {
    output.clear();
    output.reserve(input.size() * 2);
    std::vector<wchar_t> stack;
    int indent = 0;
    bool inString = false;
    const auto newline = [&] {
        output += L"\r\n";
        output.append(static_cast<size_t>(std::max(0, indent)) * 4, L' ');
    };
    const auto fail = [&](size_t offset, const wchar_t* what) {
        error = L"第 " + std::to_wstring(LineOfOffset(input, offset)) + L" 行：" + what;
        return false;
    };
    for (size_t i = 0; i < input.size(); ++i) {
        const wchar_t c = input[i];
        if (inString) {
            output += c;
            if (c == L'\\' && i + 1 < input.size()) output += input[++i];
            else if (c == L'"') inString = false;
            else if (c == L'\r' || c == L'\n') return fail(i, L"字符串中不能换行");
            continue;
        }
        switch (c) {
        case L' ': case L'\t': case L'\r': case L'\n': case 0xFEFF:
            break;
        case L'"':
            inString = true;
            output += c;
            break;
        case L'{':
        case L'[': {
            const wchar_t close = c == L'{' ? L'}' : L']';
            size_t next = i + 1;
            while (next < input.size() && std::iswspace(input[next])) ++next;
            output += c;
            if (next < input.size() && input[next] == close) {
                output += close;
                i = next;
            } else {
                stack.push_back(c);
                ++indent;
                newline();
            }
            break;
        }
        case L'}':
        case L']': {
            const wchar_t open = c == L'}' ? L'{' : L'[';
            if (stack.empty() || stack.back() != open) return fail(i, L"括号不匹配");
            stack.pop_back();
            --indent;
            newline();
            output += c;
            break;
        }
        case L',':
            if (stack.empty()) return fail(i, L"多余的逗号");
            output += c;
            newline();
            break;
        case L':':
            output += L": ";
            break;
        default:
            output += c;
            break;
        }
    }
    if (inString) return fail(input.size(), L"字符串没有闭合");
    if (!stack.empty()) return fail(input.size(), L"括号没有闭合");
    return true;
}

void FormatActiveDocument() {
    TabDocument* document = ActiveDocument();
    if (!document) return;
    const std::wstring text = GetWindowTextValue(document->editor);
    const size_t first = text.find_first_not_of(L" \t\r\n\xFEFF");
    if (first == std::wstring::npos) return;
    const int language = EffectiveLanguage(*document);
    const bool looksLikeJson = text[first] == L'{' || text[first] == L'[';
    if (language != kLanguageJson && !(language == kLanguagePlain && looksLikeJson)) {
        MessageBoxW(g_app.window, L"目前“格式化”只支持 JSON 文档。\n可以在“语言”菜单中把当前标签设为 JSON。",
            kWindowTitle, MB_ICONINFORMATION);
        return;
    }
    std::wstring formatted;
    std::wstring error;
    if (!FormatJson(text, formatted, error)) {
        MessageBoxW(g_app.window, (L"JSON 格式化失败。\n" + error).c_str(), kWindowTitle, MB_ICONWARNING);
        return;
    }
    // Replace through the selection so the change can be undone with Ctrl+Z.
    SendMessageW(document->editor, EM_SETSEL, 0, -1);
    SendMessageW(document->editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(formatted.c_str()));
    SendMessageW(document->editor, EM_SETSEL, 0, 0);
    HighlightDocument(*document);
    RefreshGutter();
}

// ---- Run ------------------------------------------------------------------

void RunActiveDocument() {
    TabDocument* document = ActiveDocument();
    if (!document) return;
    const int index = g_app.activeIndex;
    if (document->untitled || document->modified) {
        if (!SaveDocument(index, false)) return;
        document = &g_app.documents[static_cast<size_t>(index)];
    }
    const fs::path path = document->path;
    const std::wstring directory = path.parent_path().wstring();
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

    if (extension == L".html" || extension == L".htm" || extension == L".svg") {
        const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(g_app.window, L"open",
            path.c_str(), nullptr, directory.c_str(), SW_SHOWNORMAL));
        if (result <= 32) MessageBoxW(g_app.window, L"无法用默认程序打开该文件。", kWindowTitle, MB_ICONWARNING);
        return;
    }

    const std::wstring quoted = L"\"" + path.wstring() + L"\"";
    std::wstring command;
    if (extension == L".bat" || extension == L".cmd") command = L"cmd.exe /k call " + quoted;
    else if (extension == L".ps1") command = L"powershell.exe -NoLogo -NoExit -File " + quoted;
    else if (extension == L".py" || extension == L".pyw") command = L"cmd.exe /k python " + quoted;
    else if (extension == L".js" || extension == L".mjs" || extension == L".cjs") command = L"cmd.exe /k node " + quoted;
    if (command.empty()) {
        MessageBoxW(g_app.window,
            L"暂不支持运行这种类型的文件。\n支持：.bat .cmd .ps1 .py .js，以及在浏览器中打开 .html。",
            kWindowTitle, MB_ICONINFORMATION);
        return;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE,
            nullptr, directory.empty() ? nullptr : directory.c_str(), &startup, &process)) {
        MessageBoxW(g_app.window, L"启动失败，请确认对应的解释器已安装并在 PATH 中。", kWindowTitle, MB_ICONERROR);
        return;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}

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
    const int size = Scale(16);
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
    int x = Scale(4);
    RECT client{};
    GetClientRect(bar, &client);
    for (MenuBarItem& item : g_menuItems) {
        const int width = TextWidth(dc, item.text) + Scale(20);
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
            if (message->wParam == VK_LEFT) {
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
    case kIdToolbarFormat: return L"格式化 JSON (Ctrl+Shift+F)";
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
    const int width = Scale(kToolButtonWidth);
    const int height = Scale(kToolButtonHeight);
    const int top = (client.bottom - height) / 2;
    int x = Scale(6);
    int right = client.right - Scale(6);
    for (size_t index = 0; index < g_toolItems.size(); ++index) {
        ToolbarItem& item = g_toolItems[index];
        if (item.alignRight) {
            item.rect = RECT{right - width, top, right, top + height};
            right -= width + Scale(2);
        } else {
            if (item.groupStart && index > 0) x += Scale(12);
            item.rect = RECT{x, top, x + width, top + height};
            x += width + Scale(2);
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
int g_tabHot = -1;
bool g_tabCloseHot = false;
int g_tabPressedClose = -1;
int g_tabScroll = 0;
HWND g_tabTip = nullptr;

struct TabLayout {
    std::vector<RECT> tabs;
    RECT plus{};
    int visibleRight = 0;  // tabs are clipped here when they overflow
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
    layout.visibleRight = client.right - plusWidth - Scale(8);
    const int available = std::max(0, layout.visibleRight - left);
    g_tabScroll = std::clamp(g_tabScroll, 0, std::max(0, total - available));
    const int top = Scale(5);
    int x = left - g_tabScroll;
    for (const int width : widths) {
        layout.tabs.push_back(RECT{x, top, x + width, client.bottom});
        x += width;
    }
    // "+" follows the last tab; when tabs overflow it stays pinned on the right.
    const int plusLeft = total <= available ? x + Scale(4) : layout.visibleRight + Scale(4);
    const int plusTop = top + (client.bottom - top - plusWidth) / 2;
    layout.plus = RECT{plusLeft, plusTop, plusLeft + plusWidth, plusTop + plusWidth};
    if (total <= available) layout.visibleRight = client.right;
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
    if (tab.left < Scale(6)) g_tabScroll -= Scale(6) - tab.left;
    else if (tab.right > layout.visibleRight) g_tabScroll += tab.right - layout.visibleRight;
    InvalidateRect(g_app.tabs, nullptr, FALSE);
}

void InvalidateTabs() {
    if (g_app.tabs) InvalidateRect(g_app.tabs, nullptr, FALSE);
}

// Returns a tab index, kTabPlus, or -1; sets onClose when over a close box.
int TabHitTest(const TabLayout& layout, POINT point, bool* onClose) {
    if (onClose) *onClose = false;
    if (PtInRect(&layout.plus, point)) return kTabPlus;
    if (point.x >= layout.visibleRight) return -1;
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

void CopyTextToClipboard(const std::wstring& text) {
    if (!OpenClipboard(g_app.window)) return;
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

void ShowTabContextMenu(int index, POINT screen) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) return;
    const TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    const bool hasFile = !document.untitled && !document.path.empty();
    const int last = static_cast<int>(g_app.documents.size()) - 1;
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 6, hasFile ? L"重命名文件...\tF2" : L"重命名标签...\tF2");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 1, L"关闭\tCtrl+W");
    AppendMenuW(menu, MF_STRING | (last == 0 ? MF_GRAYED : 0), 2, L"关闭其他标签");
    AppendMenuW(menu, MF_STRING | (index == last ? MF_GRAYED : 0), 3, L"关闭右侧标签");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (hasFile ? 0 : MF_GRAYED), 4, L"复制文件路径");
    AppendMenuW(menu, MF_STRING | (hasFile ? 0 : MF_GRAYED), 5, L"在资源管理器中显示");
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
    default:
        break;
    }
    SetMenuState();
}

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
        IntersectClipRect(dc, 0, 0, layout.visibleRight, client.bottom);
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
            RECT text{tab.left + Scale(14), tab.top, tab.right - Scale(30), tab.bottom - Scale(1)};
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

        if (g_tabHot == kTabPlus) FillRounded(dc, layout.plus, Scale(5), palette.toolbarButtonBackground);
        DrawPlus(dc, (layout.plus.left + layout.plus.right) / 2, (layout.plus.top + layout.plus.bottom) / 2,
            Scale(6), palette.toolbarIcon);
        return 0;
    }
    case WM_MOUSEMOVE: {
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
        if (hit == kTabPlus) {
            NewDocument();
        } else if (hit >= 0 && onClose) {
            g_tabPressedClose = hit;
            SetCapture(strip);
        } else if (hit >= 0) {
            SelectTab(hit);
        }
        SetMenuState();
        InvalidateRect(strip, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONUP: {
        const int pressed = g_tabPressedClose;
        g_tabPressedClose = -1;
        if (GetCapture() == strip) ReleaseCapture();
        bool onClose = false;
        const int hit = TabHitTest(ComputeTabLayout(), POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, &onClose);
        if (pressed >= 0 && hit == pressed && onClose) {
            CloseDocument(pressed);
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
        const int hit = TabHitTest(ComputeTabLayout(), POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)}, nullptr);
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
        g_tabScroll -= GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * Scale(80);
        InvalidateRect(strip, nullptr, FALSE);
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
    if (tom) {
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

// ===========================================================================
// Input dialog, tab rename, text tools and Chinese -> pinyin.
// ===========================================================================

// ---- Small modal input dialog (1-2 text fields) ------------------------------

struct PromptState {
    std::vector<std::wstring> labels;
    std::vector<std::wstring>* values = nullptr;
    std::vector<HWND> edits;
    std::vector<std::pair<std::wstring, bool>>* checks = nullptr;
    std::vector<HWND> checkboxes;
    bool done = false;
    bool accepted = false;
};

LRESULT CALLBACK PromptProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<PromptState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        state = static_cast<PromptState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        int y = 16;
        for (size_t i = 0; i < state->labels.size(); ++i) {
            HWND label = CreateWindowExW(0, L"STATIC", state->labels[i].c_str(), WS_CHILD | WS_VISIBLE,
                Scale(16), Scale(y), Scale(340), Scale(20), window, nullptr, g_app.instance, nullptr);
            HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", (*state->values)[i].c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                Scale(16), Scale(y + 22), Scale(340), Scale(26), window, nullptr, g_app.instance, nullptr);
            for (HWND control : {label, edit}) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            state->edits.push_back(edit);
            y += 58;
        }
        if (state->checks) {
            for (const auto& [text, checked] : *state->checks) {
                HWND box = CreateWindowExW(0, L"BUTTON", text.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    Scale(16), Scale(y), Scale(340), Scale(24), window, nullptr, g_app.instance, nullptr);
                SendMessageW(box, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
                SendMessageW(box, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
                ThemeCheckbox(box);
                state->checkboxes.push_back(box);
                y += 28;
            }
        }
        HWND ok = CreateWindowExW(0, L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            Scale(186), Scale(y + 4), Scale(82), Scale(28), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), g_app.instance, nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(274), Scale(y + 4), Scale(82), Scale(28), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), g_app.instance, nullptr);
        for (HWND control : {ok, cancel}) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (state && (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)) {
            if (LOWORD(wParam) == IDOK) {
                for (size_t i = 0; i < state->edits.size(); ++i) (*state->values)[i] = GetWindowTextValue(state->edits[i]);
                for (size_t i = 0; i < state->checkboxes.size(); ++i) {
                    (*state->checks)[i].second = SendMessageW(state->checkboxes[i], BM_GETCHECK, 0, 0) == BST_CHECKED;
                }
                state->accepted = true;
            }
            state->done = true;
            return 0;
        }
        break;
    case WM_CLOSE:
        if (state) state->done = true;
        return 0;
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(window, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, g_app.windowBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, CurrentPalette().controlText);
        SetBkColor(dc, CurrentPalette().windowBackground);
        return reinterpret_cast<LRESULT>(g_app.windowBrush);
    }
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

// Shows a modal dialog; returns false if cancelled. `values` holds the initial
// text of each field and receives the result.
void ThemeCheckbox(HWND box) {
    if (g_app.darkTheme) SetWindowTheme(box, L"", L"");
}

bool PromptForText(const wchar_t* title, const std::vector<std::wstring>& labels, std::vector<std::wstring>& values,
    std::vector<std::pair<std::wstring, bool>>* checks = nullptr) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{};
        klass.hInstance = g_app.instance;
        klass.lpfnWndProc = PromptProc;
        klass.lpszClassName = L"MCnotepadPrompt";
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        registered = RegisterClassW(&klass) != 0;
    }
    values.resize(labels.size());
    PromptState state;
    state.labels = labels;
    state.values = &values;
    state.checks = checks;
    RECT owner{};
    GetWindowRect(g_app.window, &owner);
    const int checkHeight = checks ? 28 * static_cast<int>(checks->size()) : 0;
    RECT frame{0, 0, Scale(372), Scale(16 + 58 * static_cast<int>(labels.size()) + checkHeight + 48)};
    AdjustWindowRectExForDpi(&frame, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME, g_app.dpi);
    const int width = frame.right - frame.left;
    const int height = frame.bottom - frame.top;
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"MCnotepadPrompt", title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        owner.left + (owner.right - owner.left - width) / 2, owner.top + Scale(120), width, height,
        g_app.window, nullptr, g_app.instance, &state);
    if (!dialog) return false;
    ApplyWindowFrameTheme(dialog);
    EnableWindow(g_app.window, FALSE);
    ShowWindow(dialog, SW_SHOW);
    if (!state.edits.empty()) {
        SetFocus(state.edits[0]);
        SendMessageW(state.edits[0], EM_SETSEL, 0, -1);
    }
    MSG message{};
    while (!state.done && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (message.message == WM_QUIT) PostQuitMessage(static_cast<int>(message.wParam));
    EnableWindow(g_app.window, TRUE);
    DestroyWindow(dialog);
    SetForegroundWindow(g_app.window);
    if (TabDocument* document = ActiveDocument()) SetFocus(document->editor);
    return state.accepted;
}

// ---- Transient status-bar notice ------------------------------------------------

void ShowNotice(const std::wstring& text) {
    g_app.statusNotice = text;
    g_app.statusNoticeUntil = GetTickCount64() + 4000;
    UpdateStatusBar();
}

// ---- Tab rename -----------------------------------------------------------------

std::wstring TrimCopy(std::wstring_view text) {
    const size_t first = text.find_first_not_of(L" \t　");
    if (first == std::wstring_view::npos) return {};
    const size_t last = text.find_last_not_of(L" \t　");
    return std::wstring(text.substr(first, last - first + 1));
}

// Untitled tabs just get a new label (used as the suggested name when saving).
// Saved files are renamed on disk, in the same folder.
void RenameDocument(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) return;
    const bool onDisk = !g_app.documents[static_cast<size_t>(index)].untitled &&
        !g_app.documents[static_cast<size_t>(index)].path.empty();
    std::vector<std::wstring> values{g_app.documents[static_cast<size_t>(index)].title};
    if (!PromptForText(onDisk ? L"重命名文件" : L"重命名标签",
            {onDisk ? L"新文件名（文件会在原文件夹中改名）：" : L"标签名称："}, values)) {
        return;
    }
    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    const std::wstring name = TrimCopy(values[0]);
    if (name.empty() || name == document.title) return;
    if (name.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) {
        MessageBoxW(g_app.window, L"名称不能包含以下字符：\\ / : * ? \" < > |", kWindowTitle, MB_ICONWARNING);
        return;
    }
    if (!onDisk) {
        document.title = name;
    } else {
        const fs::path source = document.path;
        const fs::path target = source.parent_path() / name;
        std::error_code error;
        // A case-only rename (readme.md -> README.md) targets the same file.
        if (fs::exists(target, error) && !SamePath(source, target)) {
            MessageBoxW(g_app.window, (L"同一文件夹中已有名为“" + name + L"”的文件。").c_str(), kWindowTitle, MB_ICONWARNING);
            return;
        }
        if (!MoveFileExW(source.c_str(), target.c_str(), 0)) {
            const DWORD code = GetLastError();
            const std::wstring reason = code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND
                ? L"原文件已不存在，可以用“另存为”保存到新位置。"
                : code == ERROR_SHARING_VIOLATION || code == ERROR_ACCESS_DENIED
                ? L"文件正被其他程序使用，或没有修改权限。"
                : L"错误代码 " + std::to_wstring(code) + L"。";
            MessageBoxW(g_app.window, (L"重命名失败：" + reason).c_str(), kWindowTitle, MB_ICONWARNING);
            return;
        }
        document.path = target.wstring();
        document.title = FileTitle(target);
        document.fileTimestamp = FileTimestamp(target);
        if (document.language == kLanguageAuto) HighlightDocument(document);
    }
    UpdateTabTitle(index);
    UpdateStatusBar();
    g_app.stateDirty = true;
    PersistAll(false);
    ShowNotice(L"已重命名为 " + document.title);
}

// ---- Text tools -------------------------------------------------------------------

std::vector<std::wstring> SplitLines(std::wstring_view text) {
    std::vector<std::wstring> lines;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == L'\r' || text[i] == L'\n') {
            lines.emplace_back(text.substr(start, i - start));
            if (i < text.size() && text[i] == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            start = i + 1;
        }
    }
    return lines;
}

std::wstring JoinLines(const std::vector<std::wstring>& lines, std::wstring_view separator = L"\r") {
    std::wstring result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result += separator;
        result += lines[i];
    }
    return result;
}

// "\t", "\n" and "\\" typed in a prompt mean tab, line break and backslash.
std::wstring UnescapeInput(std::wstring_view input) {
    std::wstring result;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == L'\\' && i + 1 < input.size()) {
            const wchar_t next = input[i + 1];
            if (next == L't') { result += L'\t'; ++i; continue; }
            if (next == L'n') { result += L'\r'; ++i; continue; }
            if (next == L'\\') { result += L'\\'; ++i; continue; }
        }
        result += input[i];
    }
    return result;
}

bool IsBlankChar(wchar_t c) { return c == L' ' || c == L'\t' || c == 0x3000 || c == 0x00A0; }
bool IsCjk(wchar_t c) { return (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x3000 && c <= 0x303F) || (c >= 0xFF00 && c <= 0xFFEF); }

std::wstring TrimLeft(std::wstring_view s) { size_t i = 0; while (i < s.size() && IsBlankChar(s[i])) ++i; return std::wstring(s.substr(i)); }
std::wstring TrimRight(std::wstring_view s) { size_t n = s.size(); while (n > 0 && IsBlankChar(s[n - 1])) --n; return std::wstring(s.substr(0, n)); }

std::wstring MapCase(std::wstring_view text, DWORD flags) {
    if (text.empty()) return {};
    const int size = LCMapStringEx(LOCALE_NAME_USER_DEFAULT, flags, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr, 0);
    if (size <= 0) return std::wstring(text);
    std::wstring result(static_cast<size_t>(size), L'\0');
    LCMapStringEx(LOCALE_NAME_USER_DEFAULT, flags, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr, 0);
    return result;
}

// Mandarin pinyin, loaded on first use from <exe dir>/dict/mandarin.
Pinyin::Pinyin* PinyinConverter(std::wstring& error) {
    static std::unique_ptr<Pinyin::Pinyin> converter;
    static bool tried = false;
    if (converter) return converter.get();
    if (tried) {
        error = L"拼音词典加载失败。";
        return nullptr;
    }
    tried = true;
    const fs::path directory = fs::path(CurrentExecutablePath()).parent_path() / L"dict";
    std::error_code ec;
    if (!fs::exists(directory / L"mandarin" / L"word.txt", ec)) {
        error = L"找不到拼音词典：" + (directory / L"mandarin").wstring() + L"\n请确认程序目录下的 dict 文件夹完整。";
        tried = false;
        return nullptr;
    }
    try {
        Pinyin::setDictionaryPath(directory);
        converter = std::make_unique<Pinyin::Pinyin>();
    } catch (...) {
        error = L"拼音词典加载失败。";
        return nullptr;
    }
    return converter.get();
}

// Simplified <-> Traditional with OpenCC (phrase-aware: 头发 -> 頭髮, 干燥 ->
// 乾燥). Dictionaries are loaded on first use from <exe dir>/resources/opencc.
bool ConvertChineseScript(std::wstring_view text, bool toTraditional, std::wstring& output) {
    static std::unique_ptr<opencc::SimpleConverter> converters[2];
    static bool failed[2]{};
    const int which = toTraditional ? 0 : 1;
    if (!converters[which] && !failed[which]) {
        const fs::path config = fs::path(CurrentExecutablePath()).parent_path() / L"resources" / L"opencc" /
            (toTraditional ? L"s2t.json" : L"t2s.json");
        std::error_code error;
        if (fs::exists(config, error)) {
            try {
                converters[which] = std::make_unique<opencc::SimpleConverter>(WideToUtf8(config.wstring()));
            } catch (...) {
                failed[which] = true;
            }
        } else {
            failed[which] = true;
        }
    }
    if (!converters[which]) return false;
    try {
        output = Utf8ToWide(converters[which]->Convert(WideToUtf8(text)));
    } catch (...) {
        return false;
    }
    return true;
}

enum class PinyinMode { Tone, Plain, Initials };

// Converts Chinese characters (polyphones resolved by phrase) and keeps all
// other text. Syllables are separated by spaces; initials are run together.
bool ConvertToPinyin(std::wstring_view text, PinyinMode mode, std::wstring& output, int& converted, std::wstring& error) {
    Pinyin::Pinyin* converter = PinyinConverter(error);
    if (!converter) return false;
    const auto style = mode == PinyinMode::Tone ? Pinyin::ManTone::Style::TONE : Pinyin::ManTone::Style::NORMAL;
    Pinyin::PinyinResVector results;
    try {
        results = converter->hanziToPinyin(WideToUtf8(text), style, Pinyin::Error::Default, false, true, false);
    } catch (...) {
        error = L"拼音转换失败。";
        return false;
    }
    output.clear();
    converted = 0;
    bool previousWasPinyin = false;
    for (const Pinyin::PinyinRes& item : results) {
        if (item.error) {
            const std::wstring raw = Utf8ToWide(item.hanzi);
            // Keep "拼音 word" readable: a space between a syllable and a following word.
            if (mode != PinyinMode::Initials && previousWasPinyin && !raw.empty() && iswalnum(raw[0])) output += L' ';
            output += raw;
            previousWasPinyin = false;
            continue;
        }
        std::wstring syllable = Utf8ToWide(item.pinyin);
        if (mode == PinyinMode::Initials) {
            output += syllable.substr(0, 1);
        } else {
            if (!output.empty() && (previousWasPinyin || iswalnum(output.back()))) output += L' ';
            output += syllable;
        }
        previousWasPinyin = true;
        ++converted;
    }
    return true;
}

// Every tool works on the selection, or on the whole document when nothing is
// selected. Line tools first widen the range to whole lines.
bool ToolRange(HWND editor, bool wholeLines, CHARRANGE& range, std::wstring& text, bool& wholeDocument) {
    const std::wstring all = GetEditorRawText(editor);
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
    const LONG length = static_cast<LONG>(all.size());
    wholeDocument = range.cpMin == range.cpMax;
    if (wholeDocument) {
        range = CHARRANGE{0, length};
        // RichEdit keeps a final paragraph mark; leave it alone.
        if (length > 0 && all[static_cast<size_t>(length - 1)] == L'\r') range.cpMax = length - 1;
    } else if (wholeLines) {
        range.cpMin = std::clamp(range.cpMin, 0L, length);
        range.cpMax = std::clamp(range.cpMax, 0L, length);
        // A selection ending at the start of a line does not include that line.
        if (range.cpMax > range.cpMin && all[static_cast<size_t>(range.cpMax - 1)] == L'\r') --range.cpMax;
        while (range.cpMin > 0 && all[static_cast<size_t>(range.cpMin - 1)] != L'\r') --range.cpMin;
        while (range.cpMax < length && all[static_cast<size_t>(range.cpMax)] != L'\r') ++range.cpMax;
    }
    range.cpMin = std::clamp(range.cpMin, 0L, length);
    range.cpMax = std::clamp(range.cpMax, range.cpMin, length);
    text = all.substr(static_cast<size_t>(range.cpMin), static_cast<size_t>(range.cpMax - range.cpMin));
    return true;
}

// Values remembered between uses of the prompts.
std::wstring g_joinSeparator = L"";
std::wstring g_groupSize = L"2";
std::wstring g_splitDelimiter = L",";
std::wstring g_breakAfter = L"。！？";
std::wstring g_wrapWidth = L"80";
std::wstring g_linePrefix = L"- ";
std::wstring g_lineSuffix = L";";

bool IsTextToolCommand(int command) {
    return command >= kIdTextToolFirst && command <= kIdTextToolLast;
}

void AppendTextToolMenus(HMENU menu) {
    HMENU lines = CreatePopupMenu();
    AppendMenuW(lines, MF_STRING, kIdJoinLines, L"合并为一行");
    AppendMenuW(lines, MF_STRING, kIdJoinLinesSpace, L"合并为一行（用空格连接）");
    AppendMenuW(lines, MF_STRING, kIdJoinLinesCustom, L"合并为一行（自定义连接符）...");
    AppendMenuW(lines, MF_STRING, kIdJoinEveryN, L"每 N 行合并为一行...");
    AppendMenuW(lines, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(lines, MF_STRING, kIdSplitByDelimiter, L"按分隔符拆分为多行...");
    AppendMenuW(lines, MF_STRING, kIdBreakAfter, L"在指定字符后换行...");
    AppendMenuW(lines, MF_STRING, kIdWrapWidth, L"按长度换行...");
    AppendMenuW(lines, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(lines, MF_STRING, kIdRemoveEmptyLines, L"删除空行");
    AppendMenuW(lines, MF_STRING, kIdCollapseEmptyLines, L"合并连续空行");
    AppendMenuW(lines, MF_STRING, kIdRemoveDuplicateLines, L"删除重复行");
    AppendMenuW(lines, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(lines, MF_STRING, kIdSortAscending, L"排序（升序）");
    AppendMenuW(lines, MF_STRING, kIdSortDescending, L"排序（降序）");
    AppendMenuW(lines, MF_STRING, kIdReverseLines, L"反转行顺序");
    AppendMenuW(lines, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(lines, MF_STRING, kIdAddPrefix, L"每行开头添加...");
    AppendMenuW(lines, MF_STRING, kIdAddSuffix, L"每行末尾添加...");
    AppendMenuW(lines, MF_STRING, kIdNumberLines, L"添加行号");

    HMENU blanks = CreatePopupMenu();
    AppendMenuW(blanks, MF_STRING, kIdTrimLeading, L"去除行首空白");
    AppendMenuW(blanks, MF_STRING, kIdTrimTrailing, L"去除行尾空白");
    AppendMenuW(blanks, MF_STRING, kIdTrimBoth, L"去除行首和行尾空白");
    AppendMenuW(blanks, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(blanks, MF_STRING, kIdCollapseSpaces, L"连续空格合并为一个");
    AppendMenuW(blanks, MF_STRING, kIdRemoveAllSpaces, L"删除所有空格");
    AppendMenuW(blanks, MF_STRING, kIdTabsToSpaces, L"制表符转为空格");
    AppendMenuW(blanks, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(blanks, MF_STRING, kIdRemoveInvisible, L"清除不可见字符");
    AppendMenuW(blanks, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(blanks, MF_STRING, kIdUpperCase, L"转为大写");
    AppendMenuW(blanks, MF_STRING, kIdLowerCase, L"转为小写");
    AppendMenuW(blanks, MF_STRING, kIdTitleCase, L"每个单词首字母大写");

    HMENU chinese = CreatePopupMenu();
    AppendMenuW(chinese, MF_STRING, kIdPinyinTone, L"汉字转拼音（zhōng wén）");
    AppendMenuW(chinese, MF_STRING, kIdPinyinPlain, L"汉字转拼音，不带声调（zhong wen）");
    AppendMenuW(chinese, MF_STRING, kIdPinyinInitials, L"拼音首字母（zw）");
    AppendMenuW(chinese, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(chinese, MF_STRING, kIdToTraditional, L"简体转繁体");
    AppendMenuW(chinese, MF_STRING, kIdToSimplified, L"繁体转简体");
    AppendMenuW(chinese, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(chinese, MF_STRING, kIdFullToHalf, L"全角转半角");
    AppendMenuW(chinese, MF_STRING, kIdHalfToFull, L"半角转全角");
    AppendMenuW(chinese, MF_STRING, kIdChinesePunctToAscii, L"中文标点转英文标点");
    AppendMenuW(chinese, MF_STRING, kIdAsciiPunctToChinese, L"英文标点转中文标点");

    HMENU developer = CreatePopupMenu();
    AppendMenuW(developer, MF_STRING, kIdToolbarFormat, L"格式化 JSON\tCtrl+Shift+F");
    AppendMenuW(developer, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(developer, MF_STRING, kIdBase64Encode, L"Base64 编码");
    AppendMenuW(developer, MF_STRING, kIdBase64Decode, L"Base64 解码");
    AppendMenuW(developer, MF_STRING, kIdUrlEncode, L"URL 编码");
    AppendMenuW(developer, MF_STRING, kIdUrlDecode, L"URL 解码");
    AppendMenuW(developer, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(developer, MF_STRING, kIdRegexReplace, L"正则表达式替换...");

    HMENU extract = CreatePopupMenu();
    AppendMenuW(extract, MF_STRING, kIdExtractEmails, L"邮箱地址");
    AppendMenuW(extract, MF_STRING, kIdExtractUrls, L"链接");
    AppendMenuW(extract, MF_STRING, kIdExtractIps, L"IP 地址");
    AppendMenuW(extract, MF_STRING, kIdExtractNumbers, L"数字");
    AppendMenuW(extract, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(extract, MF_STRING, kIdExtractKeywordLines, L"包含关键词的行...");
    AppendMenuW(extract, MF_STRING, kIdExtractFields, L"指定列（字段）...");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(lines), L"行处理");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(blanks), L"空格、符号与大小写");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(chinese), L"中文处理");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(developer), L"开发者工具");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(extract), L"提取到新标签");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (g_app.previewTextTools ? MF_CHECKED : 0), kIdTogglePreview, L"批量处理前预览更改");
}

// ---- Encoders -------------------------------------------------------------------

std::wstring Base64Encode(std::wstring_view text) {
    const std::string bytes = WideToUtf8(text);
    if (bytes.empty()) return {};
    DWORD size = 0;
    const DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
    CryptBinaryToStringW(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size()), flags, nullptr, &size);
    std::wstring result(size, L'\0');
    CryptBinaryToStringW(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size()), flags, result.data(), &size);
    result.resize(size);
    return result;
}

// Accepts standard and URL-safe alphabets, with or without padding/whitespace.
bool Base64Decode(std::wstring_view text, std::wstring& result) {
    std::wstring clean;
    for (wchar_t c : text) {
        if (c == L'-') c = L'+';
        else if (c == L'_') c = L'/';
        if (!iswspace(c)) clean += c;
    }
    while (clean.size() % 4 != 0) clean += L'=';
    DWORD size = 0;
    if (clean.empty() || !CryptStringToBinaryW(clean.c_str(), static_cast<DWORD>(clean.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr)) {
        return false;
    }
    std::string bytes(size, '\0');
    CryptStringToBinaryW(clean.c_str(), static_cast<DWORD>(clean.size()), CRYPT_STRING_BASE64,
        reinterpret_cast<BYTE*>(bytes.data()), &size, nullptr, nullptr);
    bytes.resize(size);
    if (bytes.empty()) return false;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0) <= 0) return false;
    result = Utf8ToWide(bytes);
    return true;
}

// Percent-encodes UTF-8 bytes; unreserved characters stay as they are.
std::wstring UrlEncode(std::wstring_view text) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::wstring result;
    for (const unsigned char byte : WideToUtf8(text)) {
        if (isalnum(byte) || byte == '-' || byte == '_' || byte == '.' || byte == '~') {
            result += static_cast<wchar_t>(byte);
        } else {
            result += L'%';
            result += static_cast<wchar_t>(hex[byte >> 4]);
            result += static_cast<wchar_t>(hex[byte & 15]);
        }
    }
    return result;
}

std::wstring UrlDecode(std::wstring_view text, int& decoded) {
    std::string bytes;
    const std::string input = WideToUtf8(text);
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size() && HexDigit(input[i + 1]) >= 0 && HexDigit(input[i + 2]) >= 0) {
            bytes += static_cast<char>((HexDigit(input[i + 1]) << 4) | HexDigit(input[i + 2]));
            i += 2;
            ++decoded;
        } else {
            bytes += input[i];
        }
    }
    return Utf8ToWide(bytes);
}

// ---- Preview: line diff of a pending change ------------------------------------------

enum class DiffKind { Same, Removed, Added };
struct DiffLine { DiffKind kind; std::wstring text; };

std::vector<DiffLine> DiffLines(const std::vector<std::wstring>& before, const std::vector<std::wstring>& after) {
    size_t prefix = 0;
    while (prefix < before.size() && prefix < after.size() && before[prefix] == after[prefix]) ++prefix;
    size_t suffix = 0;
    while (suffix < before.size() - prefix && suffix < after.size() - prefix &&
           before[before.size() - 1 - suffix] == after[after.size() - 1 - suffix]) {
        ++suffix;
    }
    std::vector<DiffLine> diff;
    for (size_t i = 0; i < prefix; ++i) diff.push_back({DiffKind::Same, before[i]});
    const size_t n = before.size() - prefix - suffix;
    const size_t m = after.size() - prefix - suffix;
    if (n * m <= 4000000) {
        // Longest common subsequence over the changed middle part.
        std::vector<uint32_t> table((n + 1) * (m + 1), 0);
        const auto at = [&](size_t i, size_t j) -> uint32_t& { return table[i * (m + 1) + j]; };
        for (size_t i = n; i-- > 0;) {
            for (size_t j = m; j-- > 0;) {
                at(i, j) = before[prefix + i] == after[prefix + j] ? at(i + 1, j + 1) + 1 : std::max(at(i + 1, j), at(i, j + 1));
            }
        }
        size_t i = 0, j = 0;
        while (i < n || j < m) {
            if (i < n && j < m && before[prefix + i] == after[prefix + j]) {
                diff.push_back({DiffKind::Same, before[prefix + i]});
                ++i;
                ++j;
            } else if (i < n && (j == m || at(i + 1, j) >= at(i, j + 1))) {
                diff.push_back({DiffKind::Removed, before[prefix + i++]});  // removals first, like a diff
            } else {
                diff.push_back({DiffKind::Added, after[prefix + j++]});
            }
        }
    } else {
        for (size_t i = 0; i < n; ++i) diff.push_back({DiffKind::Removed, before[prefix + i]});
        for (size_t j = 0; j < m; ++j) diff.push_back({DiffKind::Added, after[prefix + j]});
    }
    for (size_t i = before.size() - suffix; i < before.size(); ++i) diff.push_back({DiffKind::Same, before[i]});
    return diff;
}

struct PreviewState {
    bool done = false;
    bool accepted = false;
    HWND view = nullptr;
    HWND dontAsk = nullptr;
    bool stopPreviewing = false;
};

LRESULT CALLBACK PreviewProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<PreviewState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE:
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(
            reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams));
        return 0;
    case WM_SIZE: {
        if (!state || !state->view) break;
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);
        const int bottom = height - Scale(48);
        MoveWindow(GetDlgItem(window, 100), Scale(16), Scale(12), width - Scale(32), Scale(22), TRUE);
        MoveWindow(state->view, Scale(16), Scale(40), width - Scale(32), std::max(0, bottom - Scale(48)), TRUE);
        MoveWindow(state->dontAsk, Scale(16), bottom + Scale(12), Scale(220), Scale(24), TRUE);
        MoveWindow(GetDlgItem(window, IDOK), width - Scale(16 + 96 + 8 + 82), bottom + Scale(10), Scale(96), Scale(28), TRUE);
        MoveWindow(GetDlgItem(window, IDCANCEL), width - Scale(16 + 82), bottom + Scale(10), Scale(82), Scale(28), TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (state && (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)) {
            state->accepted = LOWORD(wParam) == IDOK;
            state->stopPreviewing = SendMessageW(state->dontAsk, BM_GETCHECK, 0, 0) == BST_CHECKED;
            state->done = true;
            return 0;
        }
        break;
    case WM_CLOSE:
        if (state) state->done = true;
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize = POINT{Scale(420), Scale(260)};
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(window, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, g_app.windowBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, CurrentPalette().controlText);
        SetBkColor(dc, CurrentPalette().windowBackground);
        return reinterpret_cast<LRESULT>(g_app.windowBrush);
    }
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

COLORREF Blend(COLORREF a, COLORREF b, int percentB) {
    return RGB((GetRValue(a) * (100 - percentB) + GetRValue(b) * percentB) / 100,
               (GetGValue(a) * (100 - percentB) + GetGValue(b) * percentB) / 100,
               (GetBValue(a) * (100 - percentB) + GetBValue(b) * percentB) / 100);
}

// Shows what a tool will change and asks for confirmation. Returns true to apply.
bool ConfirmChange(const wchar_t* title, const std::wstring& before, const std::wstring& after) {
    const std::vector<DiffLine> diff = DiffLines(SplitLines(before), SplitLines(after));
    int removed = 0, added = 0;
    for (const DiffLine& line : diff) {
        if (line.kind == DiffKind::Removed) ++removed;
        if (line.kind == DiffKind::Added) ++added;
    }

    // Changed lines with two lines of context; long unchanged runs collapse.
    std::wstring text;
    std::vector<std::pair<LONG, DiffKind>> marks;  // start of each coloured line
    std::vector<LONG> lineLengths;
    const auto nearChange = [&](size_t index) {
        for (size_t k = (index >= 2 ? index - 2 : 0); k <= std::min(diff.size() - 1, index + 2); ++k) {
            if (diff[k].kind != DiffKind::Same) return true;
        }
        return false;
    };
    constexpr size_t kMaxShown = 3000;
    size_t shown = 0;
    size_t skipped = 0;
    LONG position = 0;
    const auto emit = [&](const std::wstring& line, DiffKind kind) {
        if (!text.empty()) {
            text += L'\r';
            ++position;
        }
        marks.push_back({position, kind});
        lineLengths.push_back(static_cast<LONG>(line.size()));
        text += line;
        position += static_cast<LONG>(line.size());
    };
    for (size_t index = 0; index < diff.size() && shown < kMaxShown; ++index) {
        const DiffLine& line = diff[index];
        if (line.kind == DiffKind::Same && !nearChange(index)) {
            ++skipped;
            continue;
        }
        if (skipped > 0) {
            emit(L"   ⋯ 省略 " + std::to_wstring(skipped) + L" 行未改动的内容", DiffKind::Same);
            skipped = 0;
        }
        const wchar_t* prefix = line.kind == DiffKind::Removed ? L" − " : line.kind == DiffKind::Added ? L" + " : L"   ";
        emit(prefix + line.text, line.kind);
        ++shown;
    }
    if (shown >= kMaxShown) emit(L"   ⋯ 更多改动未显示", DiffKind::Same);

    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{};
        klass.hInstance = g_app.instance;
        klass.lpfnWndProc = PreviewProc;
        klass.lpszClassName = L"MCnotepadPreview";
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        registered = RegisterClassW(&klass) != 0;
    }
    PreviewState state;
    RECT owner{};
    GetWindowRect(g_app.window, &owner);
    const int width = std::min(Scale(820), static_cast<int>(owner.right - owner.left) - Scale(40));
    const int height = std::min(Scale(560), static_cast<int>(owner.bottom - owner.top) - Scale(40));
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"MCnotepadPreview",
        (std::wstring(L"预览更改 - ") + title).c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        owner.left + (owner.right - owner.left - width) / 2, owner.top + (owner.bottom - owner.top - height) / 2,
        width, height, g_app.window, nullptr, g_app.instance, &state);
    if (!dialog) return true;
    ApplyWindowFrameTheme(dialog);

    const std::wstring summary = L"将删除 " + std::to_wstring(removed) + L" 行，新增 " + std::to_wstring(added) +
        L" 行。应用后可以按 Ctrl+Z 一次撤销。";
    HWND label = CreateWindowExW(0, L"STATIC", summary.c_str(), WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
        dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(100)), g_app.instance, nullptr);
    state.view = CreateWindowExW(0, L"RICHEDIT50W", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, 0, 0, 0, dialog, nullptr, g_app.instance, nullptr);
    state.dontAsk = CreateWindowExW(0, L"BUTTON", L"以后不再预览", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        0, 0, 0, 0, dialog, nullptr, g_app.instance, nullptr);
    HWND apply = CreateWindowExW(0, L"BUTTON", L"应用更改", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), g_app.instance, nullptr);
    HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), g_app.instance, nullptr);
    for (HWND control : {label, state.dontAsk, apply, cancel}) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
    }
    SendMessageW(state.view, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
    ThemeCheckbox(state.dontAsk);
    SetWindowTheme(state.view, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    const ThemePalette& palette = CurrentPalette();
    SendMessageW(state.view, EM_SETBKGNDCOLOR, 0, palette.editorBackground);
    SendMessageW(state.view, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(Scale(4), 0));
    SendMessageW(state.view, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
    SetWindowTextW(state.view, text.c_str());
    const COLORREF removedColor = g_app.darkTheme ? RGB(240, 128, 128) : RGB(176, 32, 32);
    const COLORREF addedColor = g_app.darkTheme ? RGB(120, 214, 150) : RGB(20, 120, 50);
    for (size_t k = 0; k < marks.size(); ++k) {
        CHARFORMAT2W format{};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR | CFM_BACKCOLOR;
        const DiffKind kind = marks[k].second;
        format.crTextColor = kind == DiffKind::Removed ? removedColor : kind == DiffKind::Added ? addedColor : palette.gutterText;
        format.crBackColor = kind == DiffKind::Removed ? Blend(palette.editorBackground, removedColor, 14)
            : kind == DiffKind::Added ? Blend(palette.editorBackground, addedColor, 14) : palette.editorBackground;
        CHARRANGE range{marks[k].first, marks[k].first + lineLengths[k] + 1};
        SendMessageW(state.view, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
        SendMessageW(state.view, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    }
    SendMessageW(state.view, EM_SETSEL, 0, 0);

    RECT client{};
    GetClientRect(dialog, &client);
    SendMessageW(dialog, WM_SIZE, 0, MAKELPARAM(client.right, client.bottom));
    EnableWindow(g_app.window, FALSE);
    ShowWindow(dialog, SW_SHOW);
    SetFocus(apply);
    MSG message{};
    while (!state.done && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (message.message == WM_QUIT) PostQuitMessage(static_cast<int>(message.wParam));
    EnableWindow(g_app.window, TRUE);
    DestroyWindow(dialog);
    SetForegroundWindow(g_app.window);
    if (state.accepted && state.stopPreviewing) {
        g_app.previewTextTools = false;
        g_app.stateDirty = true;
        SetMenuState();
    }
    return state.accepted;
}

// ---- Extraction -------------------------------------------------------------------

std::wstring g_keywords = L"";
std::wstring g_fieldDelimiter = L",";
std::wstring g_fieldColumns = L"1";
std::wstring g_regexFind = L"";
std::wstring g_regexReplace = L"";
bool g_regexMatchCase = false;
bool g_keywordMatchCase = false;

// Collects matches from the selection (or the whole document) into a new tab,
// leaving the source untouched.
void ExtractToNewTab(int command) {
    TabDocument* document = ActiveDocument();
    if (!document) return;
    std::vector<std::pair<std::wstring, bool>> checks;
    std::vector<std::wstring> values;
    if (command == kIdExtractKeywordLines) {
        values = {g_keywords};
        checks = {{L"区分大小写", g_keywordMatchCase}};
        if (!PromptForText(L"提取包含关键词的行", {L"关键词（多个关键词用空格分隔，满足任意一个即可）："}, values, &checks)) return;
        g_keywords = values[0];
        g_keywordMatchCase = checks[0].second;
        if (TrimCopy(values[0]).empty()) return;
    } else if (command == kIdExtractFields) {
        values = {g_fieldDelimiter, g_fieldColumns};
        if (!PromptForText(L"提取指定列", {L"分隔符（\\t 表示制表符）：", L"要提取的列号（如 1 或 1,3）："}, values)) return;
        g_fieldDelimiter = values[0];
        g_fieldColumns = values[1];
        if (values[0].empty()) return;
    }

    CHARRANGE range{};
    std::wstring text;
    bool wholeDocument = false;
    ToolRange(document->editor, false, range, text, wholeDocument);
    std::vector<std::wstring> results;
    std::wstring what;

    const auto collect = [&](const wchar_t* pattern, bool unique) {
        std::unordered_map<std::wstring, bool> seen;
        try {
            const std::wregex expression(pattern, std::regex_constants::ECMAScript | std::regex_constants::icase);
            for (std::wsregex_iterator it(text.begin(), text.end(), expression), end; it != end; ++it) {
                std::wstring value = it->str();
                // Trailing sentence punctuation is rarely part of a link.
                while (!value.empty() && std::wstring(L".,;:!?)]}'\"").find(value.back()) != std::wstring::npos) value.pop_back();
                if (value.empty() || (unique && !seen.emplace(value, true).second)) continue;
                results.push_back(value);
            }
        } catch (const std::exception&) {
        }
    };

    switch (command) {
    case kIdExtractEmails:
        what = L"邮箱地址";
        collect(LR"([A-Za-z0-9._%+\-]+@[A-Za-z0-9\-]+(?:\.[A-Za-z0-9\-]+)*\.[A-Za-z]{2,})", true);
        break;
    case kIdExtractUrls:
        what = L"链接";
        collect(LR"((?:https?|ftp)://[^\s<>"'，。；：！？、）】》]+|www\.[A-Za-z0-9\-]+\.[^\s<>"'，。；：！？、）】》]+)", true);
        break;
    case kIdExtractIps:
        what = L"IP 地址";
        collect(LR"((?:^|[^\d.])((?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})(?![\d.]))", true);
        for (std::wstring& value : results) {  // drop the leading guard character
            const size_t first = value.find_first_of(L"0123456789");
            value = value.substr(first);
        }
        break;
    case kIdExtractNumbers:
        what = L"数字";
        // Whole dotted tokens first, so IPs and version numbers aren't split into "numbers".
        collect(LR"(-?\d+(?:\.\d+)*)", false);
        std::erase_if(results, [](const std::wstring& value) { return std::count(value.begin(), value.end(), L'.') > 1; });
        break;
    case kIdExtractKeywordLines: {
        what = L"行";
        std::vector<std::wstring> keywords;
        std::wistringstream stream(g_keywords);
        for (std::wstring word; stream >> word;) keywords.push_back(g_keywordMatchCase ? word : MapCase(word, LCMAP_LOWERCASE));
        for (const std::wstring& line : SplitLines(text)) {
            const std::wstring haystack = g_keywordMatchCase ? line : MapCase(line, LCMAP_LOWERCASE);
            for (const std::wstring& keyword : keywords) {
                if (haystack.find(keyword) != std::wstring::npos) {
                    results.push_back(line);
                    break;
                }
            }
        }
        break;
    }
    case kIdExtractFields: {
        what = L"行";
        const std::wstring delimiter = UnescapeInput(g_fieldDelimiter);
        std::vector<size_t> columns;
        std::wstring columnText = g_fieldColumns;
        std::replace(columnText.begin(), columnText.end(), L'，', L',');
        std::wistringstream stream(columnText);
        for (std::wstring part; std::getline(stream, part, L',');) {
            const int column = _wtoi(part.c_str());
            if (column > 0) columns.push_back(static_cast<size_t>(column));
        }
        if (columns.empty()) {
            MessageBoxW(g_app.window, L"请输入要提取的列号，例如 1 或 1,3。", kWindowTitle, MB_ICONWARNING);
            return;
        }
        for (const std::wstring& line : SplitLines(text)) {
            if (line.find(delimiter) == std::wstring::npos) continue;  // not a delimited row
            std::vector<std::wstring> fields;
            size_t start = 0;
            for (size_t found; (found = line.find(delimiter, start)) != std::wstring::npos; start = found + delimiter.size()) {
                fields.push_back(line.substr(start, found - start));
            }
            fields.push_back(line.substr(start));
            std::vector<std::wstring> picked;
            for (const size_t column : columns) picked.push_back(column <= fields.size() ? fields[column - 1] : L"");
            results.push_back(JoinLines(picked, delimiter));
        }
        break;
    }
    default:
        return;
    }

    if (results.empty()) {
        ShowNotice(L"没有找到" + what + (wholeDocument ? L"（全文）" : L"（选中内容）"));
        return;
    }
    const std::wstring title = L"提取结果 - " + (command == kIdExtractKeywordLines ? std::wstring(L"关键词")
        : command == kIdExtractFields ? std::wstring(L"指定列") : what);
    const int index = CreateDocument({}, title, JoinLines(results, L"\r\n"), true, true, 0, 0, 0);
    if (index < 0) return;
    SelectTab(index);
    PersistAll(false);
    ShowNotice(L"已提取 " + std::to_wstring(results.size()) + L" 个" + (what == L"行" ? L"行" : what) + L"到新标签");
}

void RunTextTool(int command) {
    if (command == kIdTogglePreview) {
        g_app.previewTextTools = !g_app.previewTextTools;
        g_app.stateDirty = true;
        ShowNotice(g_app.previewTextTools ? L"批量处理前会先预览更改" : L"批量处理将直接应用（仍可撤销）");
        return;
    }
    if (command >= kIdExtractEmails && command <= kIdExtractFields) {
        ExtractToNewTab(command);
        return;
    }
    TabDocument* document = ActiveDocument();
    if (!document) return;
    HWND editor = document->editor;

    const bool lineTool = (command >= kIdJoinLines && command <= kIdNumberLines) ||
        (command >= kIdTrimLeading && command <= kIdTabsToSpaces);
    // Ask for parameters before touching the text.
    std::vector<std::wstring> values;
    switch (command) {
    case kIdJoinLinesCustom:
        values = {g_joinSeparator};
        if (!PromptForText(L"合并为一行", {L"连接符（可留空；\\t 表示制表符）："}, values)) return;
        g_joinSeparator = values[0];
        break;
    case kIdJoinEveryN:
        values = {g_groupSize, g_joinSeparator};
        if (!PromptForText(L"每 N 行合并为一行", {L"每几行合并为一行：", L"连接符（可留空；\\t 表示制表符）："}, values)) return;
        g_groupSize = values[0];
        g_joinSeparator = values[1];
        if (_wtoi(values[0].c_str()) < 2) {
            MessageBoxW(g_app.window, L"请输入不小于 2 的整数。", kWindowTitle, MB_ICONWARNING);
            return;
        }
        break;
    case kIdSplitByDelimiter:
        values = {g_splitDelimiter};
        if (!PromptForText(L"按分隔符拆分为多行", {L"分隔符（会被删除；\\t 表示制表符）："}, values)) return;
        g_splitDelimiter = values[0];
        if (values[0].empty()) return;
        break;
    case kIdBreakAfter:
        values = {g_breakAfter};
        if (!PromptForText(L"在指定字符后换行", {L"在这些字符后面换行（每个字符单独生效，字符会保留）："}, values)) return;
        g_breakAfter = values[0];
        if (values[0].empty()) return;
        break;
    case kIdWrapWidth:
        values = {g_wrapWidth};
        if (!PromptForText(L"按长度换行", {L"每行最多多少个字符："}, values)) return;
        g_wrapWidth = values[0];
        if (_wtoi(values[0].c_str()) < 1) {
            MessageBoxW(g_app.window, L"请输入正整数。", kWindowTitle, MB_ICONWARNING);
            return;
        }
        break;
    case kIdAddPrefix:
        values = {g_linePrefix};
        if (!PromptForText(L"每行开头添加", {L"添加的内容："}, values)) return;
        g_linePrefix = values[0];
        break;
    case kIdAddSuffix:
        values = {g_lineSuffix};
        if (!PromptForText(L"每行末尾添加", {L"添加的内容："}, values)) return;
        g_lineSuffix = values[0];
        break;
    case kIdRegexReplace: {
        values = {g_regexFind, g_regexReplace};
        std::vector<std::pair<std::wstring, bool>> checks{{L"区分大小写", g_regexMatchCase}};
        if (!PromptForText(L"正则表达式替换",
                {L"查找（正则表达式，如 (\\d+)-(\\d+)）：", L"替换为（$1、$2 引用分组；\\n 表示换行）："}, values, &checks)) {
            return;
        }
        g_regexFind = values[0];
        g_regexReplace = values[1];
        g_regexMatchCase = checks[0].second;
        if (values[0].empty()) return;
        break;
    }
    default:
        break;
    }

    CHARRANGE range{};
    std::wstring text;
    bool wholeDocument = false;
    ToolRange(editor, lineTool, range, text, wholeDocument);
    std::wstring result;
    std::wstring notice;
    int count = 0;
    std::vector<std::wstring> lines = SplitLines(text);
    const auto nonEmptyTrimmed = [&] {
        std::vector<std::wstring> kept;
        for (const std::wstring& line : lines) {
            std::wstring trimmed = TrimRight(TrimLeft(line));
            if (!trimmed.empty()) kept.push_back(std::move(trimmed));
        }
        return kept;
    };

    switch (command) {
    case kIdJoinLines:
    case kIdJoinLinesSpace:
    case kIdJoinLinesCustom: {
        const std::wstring separator = command == kIdJoinLines ? L"" : command == kIdJoinLinesSpace ? L" " : UnescapeInput(g_joinSeparator);
        const auto kept = nonEmptyTrimmed();
        result = JoinLines(kept, separator);
        notice = L"已将 " + std::to_wstring(kept.size()) + L" 行合并为一行";
        break;
    }
    case kIdJoinEveryN: {
        const size_t group = static_cast<size_t>(_wtoi(g_groupSize.c_str()));
        const std::wstring separator = UnescapeInput(g_joinSeparator);
        const auto kept = nonEmptyTrimmed();
        std::vector<std::wstring> merged;
        for (size_t i = 0; i < kept.size(); i += group) {
            std::vector<std::wstring> chunk(kept.begin() + static_cast<std::ptrdiff_t>(i),
                kept.begin() + static_cast<std::ptrdiff_t>(std::min(kept.size(), i + group)));
            merged.push_back(JoinLines(chunk, separator));
        }
        result = JoinLines(merged);
        notice = L"已合并为 " + std::to_wstring(merged.size()) + L" 行";
        break;
    }
    case kIdSplitByDelimiter: {
        const std::wstring delimiter = UnescapeInput(g_splitDelimiter);
        size_t start = 0;
        for (size_t found; (found = text.find(delimiter, start)) != std::wstring::npos; start = found + delimiter.size()) {
            result.append(text, start, found - start);
            result += L'\r';
            ++count;
        }
        result.append(text, start, std::wstring::npos);
        notice = L"已拆分 " + std::to_wstring(count) + L" 处";
        break;
    }
    case kIdBreakAfter: {
        const std::wstring marks = UnescapeInput(g_breakAfter);
        for (size_t i = 0; i < text.size(); ++i) {
            result += text[i];
            if (marks.find(text[i]) == std::wstring::npos) continue;
            // Don't add a break where one already follows (or a closing quote).
            size_t next = i + 1;
            while (next < text.size() && (text[next] == L'”' || text[next] == L'’' || text[next] == L'"' || text[next] == L')' || text[next] == L'）')) {
                result += text[next];
                i = next++;
            }
            if (next < text.size() && text[next] != L'\r' && text[next] != L'\n') {
                result += L'\r';
                ++count;
                while (i + 1 < text.size() && IsBlankChar(text[i + 1])) ++i;  // drop spaces at the new line start
            }
        }
        notice = L"已插入 " + std::to_wstring(count) + L" 处换行";
        break;
    }
    case kIdWrapWidth: {
        const size_t width = static_cast<size_t>(_wtoi(g_wrapWidth.c_str()));
        std::vector<std::wstring> wrapped;
        for (const std::wstring& line : lines) {
            std::wstring rest = line;
            while (rest.size() > width) {
                // Prefer breaking at the last space for Latin text.
                size_t cut = rest.find_last_of(L' ', width);
                if (cut == std::wstring::npos || cut == 0) cut = width;
                wrapped.push_back(TrimRight(rest.substr(0, cut)));
                rest = TrimLeft(rest.substr(cut));
                ++count;
            }
            wrapped.push_back(rest);
        }
        result = JoinLines(wrapped);
        notice = L"已插入 " + std::to_wstring(count) + L" 处换行";
        break;
    }
    case kIdRemoveEmptyLines: {
        std::vector<std::wstring> kept;
        for (const std::wstring& line : lines) {
            if (TrimLeft(line).empty()) ++count;
            else kept.push_back(line);
        }
        result = JoinLines(kept);
        notice = L"已删除 " + std::to_wstring(count) + L" 个空行";
        break;
    }
    case kIdCollapseEmptyLines: {
        std::vector<std::wstring> kept;
        bool previousEmpty = false;
        for (const std::wstring& line : lines) {
            const bool empty = TrimLeft(line).empty();
            if (empty && previousEmpty) { ++count; continue; }
            kept.push_back(empty ? std::wstring() : line);
            previousEmpty = empty;
        }
        result = JoinLines(kept);
        notice = L"已删除 " + std::to_wstring(count) + L" 个多余空行";
        break;
    }
    case kIdRemoveDuplicateLines: {
        std::vector<std::wstring> kept;
        std::unordered_map<std::wstring, bool> seen;
        for (const std::wstring& line : lines) {
            if (!seen.emplace(line, true).second) { ++count; continue; }
            kept.push_back(line);
        }
        result = JoinLines(kept);
        notice = L"已删除 " + std::to_wstring(count) + L" 个重复行";
        break;
    }
    case kIdSortAscending:
    case kIdSortDescending: {
        // The user's locale order: for Chinese this sorts by pinyin.
        const bool descending = command == kIdSortDescending;
        std::stable_sort(lines.begin(), lines.end(), [&](const std::wstring& a, const std::wstring& b) {
            const int order = CompareStringEx(LOCALE_NAME_USER_DEFAULT, SORT_DIGITSASNUMBERS,
                a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), nullptr, nullptr, 0);
            return descending ? order == CSTR_GREATER_THAN : order == CSTR_LESS_THAN;
        });
        result = JoinLines(lines);
        notice = L"已排序 " + std::to_wstring(lines.size()) + L" 行";
        break;
    }
    case kIdReverseLines:
        std::reverse(lines.begin(), lines.end());
        result = JoinLines(lines);
        notice = L"已反转 " + std::to_wstring(lines.size()) + L" 行";
        break;
    case kIdAddPrefix:
    case kIdAddSuffix: {
        const std::wstring addition = UnescapeInput(command == kIdAddPrefix ? g_linePrefix : g_lineSuffix);
        for (std::wstring& line : lines) line = command == kIdAddPrefix ? addition + line : line + addition;
        result = JoinLines(lines);
        notice = L"已处理 " + std::to_wstring(lines.size()) + L" 行";
        break;
    }
    case kIdNumberLines: {
        const int digits = static_cast<int>(std::to_wstring(lines.size()).size());
        for (size_t i = 0; i < lines.size(); ++i) {
            std::wstring number = std::to_wstring(i + 1);
            number.insert(0, static_cast<size_t>(digits) - number.size(), L' ');
            lines[i] = number + L". " + lines[i];
        }
        result = JoinLines(lines);
        notice = L"已为 " + std::to_wstring(lines.size()) + L" 行添加行号";
        break;
    }
    case kIdTrimLeading:
    case kIdTrimTrailing:
    case kIdTrimBoth:
        for (std::wstring& line : lines) {
            const std::wstring before = line;
            if (command != kIdTrimTrailing) line = TrimLeft(line);
            if (command != kIdTrimLeading) line = TrimRight(line);
            if (line != before) ++count;
        }
        result = JoinLines(lines);
        notice = L"已处理 " + std::to_wstring(count) + L" 行";
        break;
    case kIdCollapseSpaces:
        for (std::wstring& line : lines) {
            std::wstring collapsed;
            for (const wchar_t c : line) {
                if (IsBlankChar(c) && !collapsed.empty() && collapsed.back() == L' ') { ++count; continue; }
                collapsed += IsBlankChar(c) ? L' ' : c;
            }
            line = collapsed;
        }
        result = JoinLines(lines);
        notice = L"已删除 " + std::to_wstring(count) + L" 个多余空格";
        break;
    case kIdRemoveAllSpaces:
        for (std::wstring& line : lines) {
            std::wstring kept;
            for (const wchar_t c : line) {
                if (IsBlankChar(c)) ++count;
                else kept += c;
            }
            line = kept;
        }
        result = JoinLines(lines);
        notice = L"已删除 " + std::to_wstring(count) + L" 个空格";
        break;
    case kIdTabsToSpaces:
        for (std::wstring& line : lines) {
            std::wstring expanded;
            for (const wchar_t c : line) {
                if (c != L'\t') { expanded += c; continue; }
                expanded.append(4 - expanded.size() % 4, L' ');
                ++count;
            }
            line = expanded;
        }
        result = JoinLines(lines);
        notice = L"已转换 " + std::to_wstring(count) + L" 个制表符";
        break;
    case kIdFullToHalf:
        for (wchar_t c : text) {
            if (c >= 0xFF01 && c <= 0xFF5E) { c = static_cast<wchar_t>(c - 0xFEE0); ++count; }
            else if (c == 0x3000) { c = L' '; ++count; }
            result += c;
        }
        notice = L"已转换 " + std::to_wstring(count) + L" 个字符";
        break;
    case kIdHalfToFull:
        for (wchar_t c : text) {
            if (c >= 0x21 && c <= 0x7E) { c = static_cast<wchar_t>(c + 0xFEE0); ++count; }
            else if (c == L' ') { c = 0x3000; ++count; }
            result += c;
        }
        notice = L"已转换 " + std::to_wstring(count) + L" 个字符";
        break;
    case kIdChinesePunctToAscii: {
        static const std::unordered_map<wchar_t, const wchar_t*> map = {
            {L'，', L","}, {L'。', L"."}, {L'！', L"!"}, {L'？', L"?"}, {L'；', L";"}, {L'：', L":"},
            {L'“', L"\""}, {L'”', L"\""}, {L'‘', L"'"}, {L'’', L"'"}, {L'（', L"("}, {L'）', L")"},
            {L'【', L"["}, {L'】', L"]"}, {L'《', L"<"}, {L'》', L">"}, {L'、', L","}, {L'…', L"..."},
            {L'—', L"-"}, {L'～', L"~"}, {L'「', L"\""}, {L'」', L"\""},
        };
        for (const wchar_t c : text) {
            const auto found = map.find(c);
            if (found != map.end()) { result += found->second; ++count; }
            else result += c;
        }
        notice = L"已转换 " + std::to_wstring(count) + L" 个标点";
        break;
    }
    case kIdAsciiPunctToChinese: {
        // Only after a Chinese character, so 3.14, URLs and code stay intact.
        static const std::unordered_map<wchar_t, wchar_t> map = {
            {L',', L'，'}, {L'.', L'。'}, {L'!', L'！'}, {L'?', L'？'}, {L';', L'；'}, {L':', L'：'},
            {L'(', L'（'}, {L')', L'）'},
        };
        bool openParenConverted = false;
        for (size_t i = 0; i < text.size(); ++i) {
            const auto found = map.find(text[i]);
            size_t back = i;
            while (back > 0 && text[back - 1] == L' ') --back;
            const bool afterChinese = back > 0 && IsCjk(text[back - 1]);
            const bool closesConverted = text[i] == L')' && openParenConverted;
            if (found != map.end() && (afterChinese || closesConverted)) {
                if (text[i] == L'(') openParenConverted = true;
                if (text[i] == L')') openParenConverted = false;
                while (!result.empty() && result.back() == L' ') result.pop_back();
                result += found->second;
                ++count;
                while (i + 1 < text.size() && text[i + 1] == L' ') ++i;
            } else {
                result += text[i];
            }
        }
        notice = L"已转换 " + std::to_wstring(count) + L" 个标点";
        break;
    }
    case kIdRemoveInvisible:
        for (const wchar_t c : text) {
            const bool zeroWidth = c == 0x200B || c == 0x200C || c == 0x200D || c == 0x2060 || c == 0xFEFF || c == 0x00AD;
            const bool control = (c < 0x20 && c != L'\t' && c != L'\r' && c != L'\n') || c == 0x7F;
            if (zeroWidth || control) { ++count; continue; }
            if (c == 0x00A0) { result += L' '; ++count; continue; }
            result += c;
        }
        notice = count > 0 ? L"已清除 " + std::to_wstring(count) + L" 个不可见字符" : L"没有发现不可见字符";
        break;
    case kIdUpperCase:
        result = MapCase(text, LCMAP_UPPERCASE);
        notice = L"已转为大写";
        break;
    case kIdLowerCase:
        result = MapCase(text, LCMAP_LOWERCASE);
        notice = L"已转为小写";
        break;
    case kIdTitleCase:
        result = MapCase(MapCase(text, LCMAP_LOWERCASE), LCMAP_TITLECASE);
        notice = L"已将每个单词首字母大写";
        break;
    case kIdToTraditional:
    case kIdToSimplified: {
        const bool toTraditional = command == kIdToTraditional;
        SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        bool phraseAware = true;
        if (!ConvertChineseScript(text, toTraditional, result)) {
            // Dictionaries missing: fall back to Windows' character-level mapping.
            phraseAware = false;
            const DWORD flags = toTraditional ? LCMAP_TRADITIONAL_CHINESE : LCMAP_SIMPLIFIED_CHINESE;
            if (!text.empty()) {
                const int size = LCMapStringEx(L"zh-CN", flags, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr, 0);
                result.assign(static_cast<size_t>(std::max(size, 0)), L'\0');
                if (size > 0) LCMapStringEx(L"zh-CN", flags, text.c_str(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr, 0);
            }
        }
        if (result.size() == text.size()) {
            for (size_t i = 0; i < text.size(); ++i) count += text[i] != result[i] ? 1 : 0;
            notice = L"已转换 " + std::to_wstring(count) + L" 个字";
        } else {
            notice = L"已转换";
        }
        if (!phraseAware) notice += L"（未找到 OpenCC 词典，已按单字转换）";
        break;
    }
    case kIdBase64Encode:
        result = Base64Encode(text);
        notice = L"已 Base64 编码（UTF-8）";
        break;
    case kIdBase64Decode:
        if (!Base64Decode(text, result)) {
            MessageBoxW(g_app.window, L"无法解码：内容不是有效的 Base64，或解码结果不是 UTF-8 文本。", kWindowTitle, MB_ICONWARNING);
            return;
        }
        notice = L"已 Base64 解码";
        break;
    case kIdUrlEncode:
        for (std::wstring& line : lines) line = UrlEncode(line);  // keep one item per line
        result = JoinLines(lines);
        notice = L"已 URL 编码 " + std::to_wstring(lines.size()) + L" 行";
        break;
    case kIdUrlDecode:
        for (std::wstring& line : lines) line = UrlDecode(line, count);
        result = JoinLines(lines);
        notice = L"已解码 " + std::to_wstring(count) + L" 处";
        break;
    case kIdRegexReplace: {
        // Work with \n line breaks so patterns like ^ $ \n behave as expected.
        std::wstring source = text;
        std::replace(source.begin(), source.end(), L'\r', L'\n');
        std::wstring replacement = UnescapeInput(g_regexReplace);
        std::replace(replacement.begin(), replacement.end(), L'\r', L'\n');
        try {
            auto flags = std::regex_constants::ECMAScript | std::regex_constants::multiline;
            if (!g_regexMatchCase) flags |= std::regex_constants::icase;
            const std::wregex expression(g_regexFind, flags);
            count = static_cast<int>(std::distance(std::wsregex_iterator(source.begin(), source.end(), expression), std::wsregex_iterator()));
            result = std::regex_replace(source, expression, replacement);
        } catch (const std::regex_error& error) {
            const std::string reason = error.what();
            MessageBoxW(g_app.window, (L"正则表达式有误：\n" + Utf8ToWide(reason)).c_str(), kWindowTitle, MB_ICONWARNING);
            return;
        }
        std::replace(result.begin(), result.end(), L'\n', L'\r');
        notice = count > 0 ? L"已替换 " + std::to_wstring(count) + L" 处" : L"没有找到匹配的内容";
        break;
    }
    case kIdPinyinTone:
    case kIdPinyinPlain:
    case kIdPinyinInitials: {
        std::wstring error;
        const PinyinMode mode = command == kIdPinyinTone ? PinyinMode::Tone
            : command == kIdPinyinPlain ? PinyinMode::Plain : PinyinMode::Initials;
        SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        if (!ConvertToPinyin(text, mode, result, count, error)) {
            MessageBoxW(g_app.window, error.c_str(), kWindowTitle, MB_ICONWARNING);
            return;
        }
        notice = count > 0 ? L"已转换 " + std::to_wstring(count) + L" 个汉字" : L"所选内容中没有汉字";
        break;
    }
    default:
        return;
    }

    if (wholeDocument) notice += L"（全文）";
    if (result == text) {
        ShowNotice(notice.empty() ? L"没有需要修改的内容" : notice);
        return;
    }
    if (g_app.previewTextTools) {
        wchar_t label[128]{};
        GetMenuStringW(g_app.mainMenu, static_cast<UINT>(command), label, static_cast<int>(std::size(label)), MF_BYCOMMAND);
        std::wstring name = label;
        if (const size_t tab = name.find(L'\t'); tab != std::wstring::npos) name.resize(tab);
        if (const size_t dots = name.find(L"..."); dots != std::wstring::npos) name.resize(dots);
        if (!ConfirmChange(name.c_str(), text, result)) return;
    }
    // One replace = one undo step; the result stays selected.
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
    SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(result.c_str()));
    const std::wstring after = GetEditorRawText(editor);
    CHARRANGE selection{range.cpMin, range.cpMin};
    // Measure the inserted text as RichEdit stores it (CRLF becomes CR).
    LONG insertedLength = 0;
    for (size_t i = 0; i < result.size(); ++i) {
        if (result[i] == L'\r' && i + 1 < result.size() && result[i + 1] == L'\n') ++i;
        ++insertedLength;
    }
    selection.cpMax = std::min(static_cast<LONG>(after.size()), range.cpMin + insertedLength);
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    SendMessageW(editor, EM_SCROLLCARET, 0, 0);
    RefreshGutter();
    ShowNotice(notice);
}

// Right-click menu inside the editor.
void ShowEditorContextMenu(HWND editor, POINT screen) {
    if (screen.x == -1 && screen.y == -1) {  // Shift+F10 / menu key: open at the caret
        CHARRANGE selection{};
        SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
        POINTL caret{};
        SendMessageW(editor, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&caret), selection.cpMax);
        screen = POINT{caret.x, caret.y + Scale(18)};
        ClientToScreen(editor, &screen);
    }
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const bool hasSelection = selection.cpMax > selection.cpMin;
    const UINT selectionState = hasSelection ? MF_ENABLED : MF_GRAYED;
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (SendMessageW(editor, EM_CANUNDO, 0, 0) ? 0 : MF_GRAYED), kIdEditUndo, L"撤销\tCtrl+Z");
    AppendMenuW(menu, MF_STRING | (SendMessageW(editor, EM_CANREDO, 0, 0) ? 0 : MF_GRAYED), kIdEditRedo, L"重做\tCtrl+Y");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | selectionState, kIdEditCut, L"剪切\tCtrl+X");
    AppendMenuW(menu, MF_STRING | selectionState, kIdEditCopy, L"复制\tCtrl+C");
    AppendMenuW(menu, MF_STRING | (IsClipboardFormatAvailable(CF_UNICODETEXT) ? 0 : MF_GRAYED), kIdEditPaste, L"粘贴\tCtrl+V");
    AppendMenuW(menu, MF_STRING, kIdEditSelectAll, L"全选\tCtrl+A");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendTextToolMenus(menu);
    const int command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, g_app.window, nullptr);
    DestroyMenu(menu);
    if (command != 0) SendMessageW(g_app.window, WM_COMMAND, MAKEWPARAM(command, 0), 0);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (g_app.findReplaceMessage != 0 && message == g_app.findReplaceMessage) {
        HandleFindReplaceMessage(lParam);
        return 0;
    }
    switch (message) {
    case WM_CREATE: {
        g_app.window = window;
        g_app.mainMenu = CreateApplicationMenu();
        g_app.dpi = GetDpiForWindow(window);
        CreateUiFonts();
        GdiplusStartupInput gdiplusInput;
        GdiplusStartup(&g_app.gdiplusToken, &gdiplusInput, nullptr);
        LoadToolbarIconImages();
        g_app.editorBrush = CreateSolidBrush(CurrentPalette().editorBackground);
        g_app.toolbarBrush = CreateSolidBrush(CurrentPalette().toolbarBackground);
        g_app.sidebarBrush = CreateSolidBrush(CurrentPalette().panelBackground);
        g_app.windowBrush = CreateSolidBrush(CurrentPalette().windowBackground);
        g_app.controlBrush = CreateSolidBrush(CurrentPalette().controlBackground);
        g_app.statusBrush = CreateSolidBrush(CurrentPalette().statusBackground);

        CreateChrome(window);
        g_app.gutter = CreateWindowExW(0, L"MCGutter", nullptr,
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
        g_app.sidebarVisible = false;
        LoadLibraryW(L"Msftedit.dll");
        LoadPersistedSession();
        g_app.darkTheme = g_app.theme == ThemeKind::Dark || g_app.theme == ThemeKind::Blue;
        ApplyWindowFrameTheme(window);
        ApplyMenuColorMode();
        RefreshChrome();
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
        } else if (wParam == kTimerHighlight) {
            KillTimer(window, kTimerHighlight);
            if (const TabDocument* document = ActiveDocument()) {
                if (GetWindowTextLengthW(document->editor) <= kLiveHighlightLimit) HighlightDocument(*document);
            }
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
                    if (g_app.showLineNumbers && GutterWidthFor(source) != g_app.gutterWidth) LayoutChildren(window);
                    UpdateHorizontalScrollbar(source);
                    RefreshGutter();
                    const int language = EffectiveLanguage(g_app.documents[index]);
                    if (language != kLanguagePlain && language != kLanguageMarkdown) {
                        SetTimer(window, kTimerHighlight, kHighlightDelayMs, nullptr);  // restarts on each keystroke
                    }
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
        if (command >= kIdEncodingFirst && command < kIdEncodingFirst + kEncodingCount) {
            SetActiveEncoding(command - kIdEncodingFirst);
            SetMenuState();
            return 0;
        }
        if (command >= kIdLineEndingFirst && command < kIdLineEndingFirst + kLineEndingCount) {
            SetActiveLineEnding(command - kIdLineEndingFirst);
            SetMenuState();
            return 0;
        }
        if (command >= kIdLanguageFirst && command < kIdLanguageFirst + kLanguageCount) {
            SetActiveLanguage(command - kIdLanguageFirst);
            SetMenuState();
            return 0;
        }

        if (IsTextToolCommand(command)) {
            RunTextTool(command);
            SetMenuState();
            return 0;
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
        case kIdFindNext: FindAgain(true); break;
        case kIdFindPrevious: FindAgain(false); break;
        case kIdToolbarReplace: OpenFindDialog(true); break;
        case kIdToolbarGoto: OpenGotoDialog(); break;
        case kIdToolbarFormat: FormatActiveDocument(); break;
        case kIdFileRename: RenameDocument(g_app.activeIndex); break;
        case kIdToolbarRun: RunActiveDocument(); break;
        case kIdToolbarEncoding:
            ShowEncodingMenu(ToolbarItemScreenRect(kIdToolbarEncoding));
            break;
        case kIdToolbarLineNumbers:
            g_app.showLineNumbers = !g_app.showLineNumbers;
            g_app.stateDirty = true;
            LayoutChildren(window);
            break;
        case kIdToolbarWrap:
            g_app.wordWrap = !g_app.wordWrap;
            g_app.stateDirty = true;
            ApplyWordWrapAll();
            LayoutChildren(window);
            break;
        case kIdToolbarTheme:
            ShowThemeMenu(ToolbarItemScreenRect(kIdToolbarTheme));
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
            if (item->CtlType == ODT_COMBOBOX) DrawComboItem(*item);
        }
        return TRUE;
    }
    case WM_MEASUREITEM: {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (measure != nullptr && measure->CtlType == ODT_COMBOBOX) {
            measure->itemHeight = static_cast<UINT>(Scale(24));
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
        if (header != nullptr && header->code == EN_SELCHANGE) {
            UpdateStatusBar();
            RefreshGutter();  // current-line number follows the caret
            return 0;
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
    case WM_ACTIVATEAPP:
        if (wParam) PostMessageW(window, kMessageCheckExternalChanges, 0, 0);
        return 0;
    case kMessageCheckExternalChanges:
        CheckExternalChanges();
        return 0;
    case kMessageOpenPendingFiles: {
        std::vector<std::wstring> files;
        files.swap(g_app.pendingOpenFiles);
        for (const std::wstring& file : files) OpenFileInNewTab(fs::path(file));
        SetMenuState();
        return 0;
    }
    case WM_DPICHANGED: {
        // Moved to a monitor with a different scale: rebuild fonts and sizes,
        // then take the size Windows suggests for the new DPI.
        g_app.dpi = HIWORD(wParam);
        CreateUiFonts();
        ApplyEditorFont();
        RefreshChrome();
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(window, nullptr, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        LayoutChildren(window);
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    }
    case WM_SYSCOMMAND:
        // No native menu bar: route Alt / Alt+letter / F10 to the custom one.
        if ((wParam & 0xFFF0) == SC_KEYMENU && HandleMenuKey(lParam)) return 0;
        return DefWindowProcW(window, message, wParam, lParam);
    case kMessageOpenMenuBar:
        OpenMenuBarItem(static_cast<int>(wParam));
        return 0;
    case WM_MENUSELECT:
        // Lets Right-arrow open a submenu instead of jumping to the next menu.
        g_menuSelectionHasPopup = (HIWORD(wParam) & MF_POPUP) != 0 && HIWORD(wParam) != 0xFFFF;
        return 0;
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
        MulDiv(1100, static_cast<int>(GetDpiForSystem()), 96),
        MulDiv(760, static_cast<int>(GetDpiForSystem()), 96),
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!window) {
        MessageBoxW(nullptr, L"主窗口创建失败。", kWindowTitle, MB_ICONERROR);
        RemoveRunningMarker();
        return 1;
    }

    // Restore the previous size/position, but only if it is still on a monitor.
    if (g_app.hasSavedWindow && MonitorFromRect(&g_app.savedWindowRect, MONITOR_DEFAULTTONULL) != nullptr) {
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        GetWindowPlacement(window, &placement);
        placement.rcNormalPosition = g_app.savedWindowRect;
        placement.showCmd = g_app.savedWindowMaximized ? SW_SHOWMAXIMIZED
            : (showCommand == SW_SHOWMINIMIZED || showCommand == SW_MINIMIZE ? showCommand : SW_SHOWNORMAL);
        SetWindowPlacement(window, &placement);
    } else {
        ShowWindow(window, showCommand);
    }
    UpdateWindow(window);
    OpenCommandLineFiles();
    PostMessageW(window, kMessageCheckExternalChanges, 0, 0);

    HACCEL accelerators = CreateAccelerators();
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        // Modeless dialogs first: gives them Tab/Enter/Esc handling and keeps the
        // main window's Ctrl+C/V/Z accelerators from hijacking their text boxes.
        bool handled = false;
        for (HWND dialog : {g_app.findDialog, g_app.gotoWindow, g_app.settingsWindow}) {
            if (dialog && IsWindow(dialog) && IsDialogMessageW(dialog, &message)) {
                handled = true;
                break;
            }
        }
        if (handled) continue;
        if (!TranslateAcceleratorW(window, accelerators, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    DestroyAcceleratorTable(accelerators);

    return static_cast<int>(message.wParam);
}

