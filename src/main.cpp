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
#include <shobjidl.h>
#include <shlwapi.h>
#include <process.h>
#include <windowsx.h>
#include <winreg.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <regex>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <wincrypt.h>
#include <cwctype>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <iterator>
#include <random>
#include <chrono>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../resources/resource.h"

#include <cpp-pinyin/Pinyin.h>
#include <cpp-pinyin/G2pglobal.h>
#include <SimpleConverter.hpp>
#include <faker-cxx/faker.h>

extern "C" {
#include "../third_party/duktape-2.7.0/src/duktape.h"
}

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
constexpr UINT kMessageSearchDone = WM_APP + 4;
constexpr UINT kTimerSearchProgress = 4201;
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
constexpr int kIdFileReopenClosed = 1008;
constexpr int kIdFileSaveAll = 1009;
constexpr int kIdFileCloseAll = 1010;
constexpr int kIdEditUndo = 1101;
constexpr int kIdEditRedo = 1102;
constexpr int kIdEditCut = 1103;
constexpr int kIdEditCopy = 1104;
constexpr int kIdEditPaste = 1105;
constexpr int kIdEditSelectAll = 1106;
constexpr int kIdPrefixSuffix = 1107;
constexpr int kIdMultiCaretNext = 1108;
constexpr int kIdRectangleSelection = 1109;
constexpr int kIdClearMultiCaret = 1110;
constexpr int kIdMacroToggle = 1111;
constexpr int kIdMacroPlay = 1112;
constexpr int kIdMacroClear = 1113;
constexpr int kIdHelpAbout = 1201;
constexpr int kIdFindNext = 1206;
constexpr int kIdFindPrevious = 1207;
constexpr int kIdFileRename = 1007;
constexpr int kIdSearchInFolder = 1210;
constexpr int kIdReplaceInFolder = 1211;
constexpr int kIdSearchCurrentFile = 1212;
constexpr int kIdSearchOpenFiles = 1213;
constexpr int kIdSearchFileNames = 1214;
constexpr int kIdSearchRegex = 1215;
constexpr int kIdToggleSearchPanel = 1216;
constexpr int kIdExportSearchResults = 1217;
constexpr int kIdClearSearchResults = 1218;
constexpr int kIdSearchErrorLog = 1219;
constexpr int kIdUndoReplaceInFiles = 1220;
constexpr int kIdCopySearchResults = 1221;
constexpr int kIdBookmarkSearchResult = 1222;
constexpr int kIdCompareFiles = 1230;
constexpr int kIdCompareWithFile = 1231;
constexpr int kIdCompareFolders = 1232;
constexpr int kIdCompareStructure = 1233;
constexpr int kIdOpenLogViewer = 1234;
constexpr int kIdToggleToolbox = 1235;
constexpr int kIdViewToggleBookmark = 1236;
constexpr int kIdViewNextBookmark = 1237;
constexpr int kIdViewPreviousBookmark = 1238;
constexpr int kIdViewClearBookmarks = 1239;
constexpr int kIdTabTogglePinned = 1240;
constexpr int kIdViewWhitespace = 1241;
constexpr int kIdViewIndentGuides = 1242;
constexpr int kIdViewFoldCurrent = 1243;
constexpr int kIdViewUnfoldAll = 1244;
constexpr int kIdSearchHistoryFirst = 3101;  // ..3120
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
    kIdOutputReplace, kIdOutputNewTab, kIdOutputInsertBelow,  // order = ToolOutput values
    kIdTogglePreview,
    kIdFormatCss, kIdMinifyCss, kIdFormatJavaScript, kIdMinifyJavaScript,
    kIdMarkdownToHtml, kIdHtmlToMarkdown,
    kIdTextToolLast = kIdMinifyJavaScript
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
constexpr int kIdSettingsWindowCloseBehavior = 1710;
constexpr int kIdSettingsMinimizeToTray = 1711;
constexpr int kIdSettingsShowTray = 1712;
constexpr int kIdSettingsShortcuts = 1713;
constexpr int kIdShortcutEditFirst = 2120;
constexpr int kIdShortcutApply = 2140;
constexpr int kIdShortcutReset = 2141;
constexpr int kIdShortcutCancel = 2142;
constexpr int kIdTrayShow = 1720;
constexpr int kIdTrayExit = 1721;
constexpr int kIdEncodingFirst = 1601;    // 1601..1605
constexpr int kIdLineEndingFirst = 1611;  // 1611..1613
constexpr int kIdLanguageFirst = 1621;    // 1621..1645
constexpr int kIdLineSpacingFirst = 1651;
constexpr int kIdLineSpacingSingle = kIdLineSpacingFirst;
constexpr int kIdLineSpacing115 = kIdLineSpacingFirst + 1;
constexpr int kIdLineSpacing150 = kIdLineSpacingFirst + 2;
constexpr int kIdLineSpacing200 = kIdLineSpacingFirst + 3;
constexpr int kIdLineSpacingCustom = kIdLineSpacingFirst + 4;

enum DataGeneratorId : int {
    kIdDataFirst = 1801,
    kDataLineNumber = kIdDataFirst,
    kDataArithmetic, kDataGeometric, kDataRandomInteger, kDataRandomDecimal,
    kDataRandomPercent, kDataRandomLetter, kDataRandomString, kDataBoolean,
    kDataPassword, kDataCaptcha, kDataColor, kDataChineseCharacter, kDataEnglishWord,
    kDataUuid, kDataGuid, kDataUlid, kDataNanoId, kDataSnowflake, kDataOrderNumber,
    kDataSku, kDataBarcode, kDataSerialNumber,
    kDataCurrentDate, kDataCurrentTime, kDataCurrentDateTime, kDataUnixTimestamp,
    kDataIsoTime, kDataRandomDate, kDataRandomTime, kDataDateSequence, kDataTimeSequence,
    kDataChineseName, kDataEnglishName, kDataUsername, kDataEmail, kDataPhone,
    kDataTelephone, kDataGender, kDataAge, kDataBirthDate, kDataCountry, kDataCity,
    kDataStreet, kDataPostcode, kDataCompany, kDataJob, kDataProfile, kDataFullProfile,
    kDataIpv4, kDataIpv6, kDataMac, kDataPort, kDataDomain, kDataUrl, kDataUserAgent,
    kDataApiKey, kDataToken, kDataJwt, kDataHex, kDataBinary, kDataBase64, kDataHash,
    kDataHttpHeaders,
    kDataLorem, kDataChineseLorem, kDataEnglishLorem, kDataSentence, kDataParagraph,
    kDataRepeatText, kDataRepeatChar, kDataBlankLines, kDataDivider, kDataMarkdownTable,
    kDataMarkdownToc, kDataMarkdownTasks,
    kDataJson, kDataJsonl, kDataXml, kDataCsv, kDataTsv, kDataYaml, kDataToml,
    kDataSql, kDataHtmlTable, kDataTemplate, kDataMultiField, kDataUsers, kDataProducts,
    kDataOrders, kDataLogs, kDataApiResponse,
    kIdDataLast = kDataApiResponse
};
constexpr int kIdDataDialogCount = 1951;
constexpr int kIdDataDialogStart = 1952;
constexpr int kIdDataDialogEnd = 1953;
constexpr int kIdDataDialogStep = 1954;
constexpr int kIdDataDialogLength = 1955;
constexpr int kIdDataDialogPrefix = 1956;
constexpr int kIdDataDialogSuffix = 1957;
constexpr int kIdDataDialogSeparator = 1958;
constexpr int kIdDataDialogPadding = 1959;
constexpr int kIdDataDialogSeed = 1960;
constexpr int kIdDataDialogCharset = 1961;
constexpr int kIdDataDialogRepeat = 1962;
constexpr int kIdDataDialogLocale = 1963;
constexpr int kIdDataDialogOutput = 1964;
constexpr int kIdDataDialogUnique = 1965;
constexpr int kIdDataDialogDescending = 1966;
constexpr int kIdDataDialogPreview = 1967;
constexpr int kIdDataDialogExecute = 1968;
constexpr int kIdDataDialogCopy = 1969;
constexpr int kIdDataDialogNewTab = 1970;
constexpr int kIdDataDialogSaveTemplate = 1971;

enum class ThemeKind { Dark = 0, Light = 1, Green = 2, Blue = 3 };
enum class CloseBehavior { Prompt = 0, Direct = 1, Save = 2 };
enum class WindowCloseBehavior { Exit = 0, MinimizeToTray = 1, Prompt = 2 };

constexpr UINT kMessageTrayIcon = WM_APP + 5;
constexpr UINT kTrayIconId = 1;

enum class Tok : uint8_t {
    Plain, Keyword, Type, String, Number, Comment, Function, Tag, Attribute, Property, Constant, Meta,
    Variable, Heading, Emphasis, Link, Error, Warning, Info, Added, Removed,
    Column0, Column1, Column2, Column3, Column4, Column5,
};

struct SyntaxToken {
    uint32_t start;
    uint32_t length;
    Tok kind;
};

struct FoldRegion {
    LONG start = 0;
    LONG end = 0;
    int line = 0;
    bool collapsed = false;
};

struct MultiCaret {
    LONG anchor = 0;
    LONG caret = 0;
};

struct MacroAction {
    bool command = false;
    UINT commandId = 0;
    std::wstring text;
};

struct TabDocument {
    HWND editor = nullptr;
    std::wstring path;
    std::wstring title;
    fs::path snapshotPath;
    bool untitled = true;
    bool modified = false;
    bool pinned = false;
    bool recoveryDirty = true;
    int64_t caret = 0;
    int64_t anchor = 0;
    int firstVisibleLine = 0;
    int horizontalScroll = 0;
    int encoding = 0;        // index into kEncodingNames
    int lineEnding = 0;      // index into kLineEndingNames
    int language = 0;        // index into kLanguageNames, 0 = detect from extension
    uint64_t fileTimestamp = 0;  // last write time we know about, 0 = unknown
    int sniffedLanguage = -1;    // guessed from content when the name says nothing
    std::vector<SyntaxToken> syntaxTokens;
    std::vector<std::pair<LONG, LONG>> syntaxColored;  // character ranges already painted
    bool syntaxStale = true;     // tokens no longer match the text
    bool syntaxHadColors = false;
    uint64_t textHash = 0;       // to tell real edits from formatting-only EN_CHANGE
    std::vector<int> bookmarks;  // 1-based logical line numbers
    int highlightedLine = -1;
    LONG bracketMatchStart = -1;
    LONG bracketMatchEnd = -1;
    std::vector<FoldRegion> folds;
    uint64_t foldTextHash = 0;
    std::vector<MultiCaret> multiCarets;
    bool rectangleSelection = false;
    bool largeFileMode = false;
};

struct ClosedTab {
    std::wstring path;
    std::wstring title;
    std::wstring text;
    bool untitled = true;
    bool modified = false;
    bool pinned = false;
    int64_t caret = 0;
    int64_t anchor = 0;
    int firstVisibleLine = 0;
    int encoding = 0;
    int lineEnding = 0;
    int language = 0;
    uint64_t fileTimestamp = 0;
    std::vector<int> bookmarks;
};

constexpr const wchar_t* kEncodingNames[] = {L"UTF-8", L"UTF-8 BOM", L"UTF-16 LE", L"UTF-16 BE", L"ANSI"};
constexpr const wchar_t* kLineEndingNames[] = {L"CRLF", L"LF", L"CR"};
constexpr const wchar_t* kLineEndingMenuNames[] = {L"Windows (CRLF)", L"Unix (LF)", L"旧式 Mac (CR)"};
constexpr const wchar_t* kLanguageNames[] = {L"自动检测", L"HTML", L"CSS", L"JavaScript", L"C/C++",
    L"Python", L"JSON", L"Markdown", L"SQL", L"PowerShell", L"TypeScript", L"XML", L"YAML",
    L"配置文件（INI / TOML / ENV）", L"Shell", L"批处理（BAT / CMD）", L"Java / Kotlin", L"C#", L"Go", L"Rust", L"PHP",
    L"Dockerfile", L"CSV / TSV", L"日志（LOG）", L"Diff / Patch"};
constexpr int kEncodingCount = 5;
constexpr int kLineEndingCount = 3;
constexpr int kLanguageCount = 25;
constexpr int kEncodingAnsi = 4;
constexpr int kLanguageAuto = 0;
constexpr int kLanguageHtml = 1;
constexpr int kLanguageCss = 2;
constexpr int kLanguageJavaScript = 3;
constexpr int kLanguageCpp = 4;
constexpr int kLanguagePython = 5;
constexpr int kLanguageJson = 6;
constexpr int kLanguageMarkdown = 7;
constexpr int kLanguageSql = 8;
constexpr int kLanguagePowerShell = 9;
constexpr int kLanguageTypeScript = 10;
constexpr int kLanguageXml = 11;
constexpr int kLanguageYaml = 12;
constexpr int kLanguageConfig = 13;
constexpr int kLanguageShell = 14;
constexpr int kLanguageBatch = 15;
constexpr int kLanguageJava = 16;
constexpr int kLanguageCSharp = 17;
constexpr int kLanguageGo = 18;
constexpr int kLanguageRust = 19;
constexpr int kLanguagePhp = 20;
constexpr int kLanguageDockerfile = 21;
constexpr int kLanguageCsv = 22;
constexpr int kLanguageLog = 23;
constexpr int kLanguageDiff = 24;
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
    HMENU lineSpacingMenu = nullptr;
    WNDPROC oldTabProc = nullptr;
    WNDPROC oldStatusProc = nullptr;
    bool darkTheme = true;
    ThemeKind theme = ThemeKind::Dark;
    int fontSize = 13;
    int lineSpacingPercent = 100;
    std::wstring fontFamily = L"Consolas";
    bool showLineNumbers = true;
    bool wordWrap = false;
    bool showWhitespace = false;
    bool showIndentGuides = false;
    bool sidebarVisible = false;
    UINT findReplaceMessage = 0;
    HWND findDialog = nullptr;
    FINDREPLACEW findReplace{};
    wchar_t findBuffer[256]{};
    DWORD findFlags = FR_DOWN;
    bool findDialogIsReplace = false;
    wchar_t replaceBuffer[256]{};
    std::vector<TabDocument> documents;
    std::vector<ClosedTab> closedTabs;
    std::vector<std::wstring> pendingOpenFiles;
    std::wstring statusNotice;
    int searchPanelHeight = 340;
    HWND toolboxPanel = nullptr;
    bool toolboxVisible = false;
    int toolboxWidth = 380;           // at 96 DPI
    unsigned toolboxMask = 0x3FF;     // tools shown in the toolbox
    int toolboxTool = 0;
    HWND searchPanel = nullptr;
    bool searchPanelVisible = false;
    HMENU searchHistoryMenu = nullptr;
    bool previewTextTools = true;
    int toolOutputMode = 1;  // 0 replace, 1 new tab, 2 insert below the original
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
    WindowCloseBehavior windowCloseBehavior = WindowCloseBehavior::Exit;
    bool minimizeToTray = false;
    bool showTrayIcon = true;
    bool trayIconAdded = false;
    bool hiddenToTray = false;
    bool macroRecording = false;
    bool macroPlaying = false;
    std::vector<MacroAction> macroActions;
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
    HWND settingsWindowCloseBehaviorCombo = nullptr;
    HWND settingsMinimizeToTrayCheck = nullptr;
    HWND settingsShowTrayCheck = nullptr;
    fs::path dataDirectory;
    fs::path recoveryDirectory;
    fs::path sessionPath;
    fs::path runningMarkerPath;
};

AppState g_app;

struct ShortcutBinding {
    int command = 0;
    const wchar_t* label = nullptr;
    std::wstring text;
};

std::vector<ShortcutBinding> g_shortcuts;
HACCEL g_accelerators = nullptr;
HWND g_shortcutDialog = nullptr;
HWND g_shortcutDialogOwner = nullptr;
std::vector<HWND> g_shortcutEditControls;

std::string HexEncode(std::string_view value);
std::string WideToUtf8(std::wstring_view value);
std::wstring Utf8ToWide(std::string_view value);

void InitializeShortcutBindings() {
    if (!g_shortcuts.empty()) return;
    g_shortcuts = {
        {kIdFileNew, L"新建", L"Ctrl+N"},
        {kIdFileOpen, L"打开", L"Ctrl+O"},
        {kIdFileSave, L"保存", L"Ctrl+S"},
        {kIdFileSaveAs, L"另存为", L"Ctrl+Shift+S"},
        {kIdFileClose, L"关闭标签", L"Ctrl+W"},
        {kIdToolbarFind, L"查找", L"Ctrl+F"},
        {kIdToolbarReplace, L"替换", L"Ctrl+H"},
        {kIdToolbarGoto, L"跳转到行", L"Ctrl+G"},
        {kIdSearchInFolder, L"文件夹搜索", L"Ctrl+Shift+F"},
        {kIdToggleToolbox, L"开发者工具箱", L"Ctrl+Shift+D"},
        {kIdToolbarSettings, L"偏好设置", L"Ctrl+,"},
        {kIdSettingsShortcuts, L"快捷键设置", L"Ctrl+K"},
    };
}

void SaveShortcutBindings(std::ostream& output) {
    output << "SHORTCUTS";
    for (const ShortcutBinding& binding : g_shortcuts) {
        output << '\t' << binding.command << '\t' << HexEncode(WideToUtf8(binding.text));
    }
    output << "\n";
}

struct PromptChoice;
std::string HexEncode(std::string_view value);
std::string WideToUtf8(std::wstring_view value);
std::wstring Utf8ToWide(std::string_view value);
bool PromptForText(const wchar_t* title, const std::vector<std::wstring>& labels,
    std::vector<std::wstring>& values,
    std::vector<std::pair<std::wstring, bool>>* checks,
    std::vector<PromptChoice>* choices);

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
void ApplyCurrentLineHighlight(TabDocument& document, bool force = false);
void ApplyBracketMatchHighlight(TabDocument& document, bool force = false);
COLORREF Blend(COLORREF base, COLORREF overlay, int percentOverlay);
void HighlightDocument(const TabDocument& document);
void HighlightDocument(TabDocument& document);
void SyntaxColorVisible(TabDocument& document);
void SyntaxOnViewChanged(HWND editor);
int LanguageFromName(const std::wstring& name);
int LanguageFromPath(const std::wstring& path);
int SniffLanguage(const std::wstring& text);
int EffectiveLanguage(const TabDocument& document);
void RefreshGutter();
void AppendEncodingItems(HMENU menu);
void CheckDocumentMenuItems(HMENU menu);
void ApplyWordWrapAll();
void ApplyEditorLineSpacing(HWND editor);
void ApplyEditorLineSpacingAll();
void OpenCustomLineSpacingDialog();
bool IsDataGeneratorCommand(int command);
void AppendDataGeneratorMenus(HMENU menu);
void OpenDataGeneratorDialog(int command);
std::wstring Base64Encode(std::wstring_view text);
bool CloseDocument(int index);
void RenameDocument(int index);
void AppendTextToolMenus(HMENU menu);
void ThemeCheckbox(HWND box);
void ThemePushButtons(std::initializer_list<HWND> buttons);
void ShowNotice(const std::wstring& text);
void SaveSearchState();
void CancelSearch(bool wait);
void ApplyToolResult(TabDocument& document, CHARRANGE range, const std::wstring& result, int mode,
    const std::wstring& toolName, std::wstring notice);
void ApplySearchPanelTheme();
void ApplyToolboxTheme();
void HideSearchPanel();
bool EncodeTextBytes(std::wstring_view text, int encoding, std::string& bytes);
void ShowEditorContextMenu(HWND editor, POINT screen);
void UpdateHorizontalScrollbar(HWND editor);
void RefreshChrome();
void ApplyMenuColorMode();
void EnsureTabVisible(int index);
void InvalidateToolbar();
void UpdateTrayIcon();
void RemoveTrayIcon();
void HideMainWindowToTray();
void RestoreMainWindowFromTray();
bool HandleMainWindowClose(bool forceExit = false);
void OpenPrefixSuffixDialog();
void OpenShortcutDialog();
void RebuildAccelerators();
uint64_t HashText(std::wstring_view text);
TabDocument* ActiveDocument();
std::wstring GetEditorRawText(HWND editor);
HWND ActiveEditor();
std::vector<std::wstring> SplitLines(std::wstring_view text);
std::wstring JoinLines(const std::vector<std::wstring>& lines, std::wstring_view separator = L"\r");
bool ToolRange(HWND editor, bool wholeLines, CHARRANGE& range, std::wstring& text, bool& wholeDocument);

LRESULT CALLBACK EditorProc(HWND editor, UINT message, WPARAM wParam, LPARAM lParam);
void ApplyEditorFont();
void CreateUiFonts();
void UpdateStatusBar();
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

void DetectFoldRegions(TabDocument& document) {
    if (!document.editor) return;
    if (document.largeFileMode) return;
    const std::wstring text = GetEditorRawText(document.editor);
    const uint64_t hash = HashText(text);
    if (document.foldTextHash == hash && !document.folds.empty()) return;
    document.folds.clear();
    document.foldTextHash = hash;
    std::vector<LONG> stack;
    for (LONG position = 0; position < static_cast<LONG>(text.size()); ++position) {
        const wchar_t character = text[static_cast<size_t>(position)];
        if (character == L'{' && (position == 0 || text[static_cast<size_t>(position - 1)] != L'\\')) {
            stack.push_back(position);
        } else if (character == L'}' && !stack.empty()) {
            const LONG opening = stack.back();
            stack.pop_back();
            const LRESULT line = SendMessageW(document.editor, EM_EXLINEFROMCHAR, 0, opening);
            const LRESULT closingLine = SendMessageW(document.editor, EM_EXLINEFROMCHAR, 0, position);
            if (line >= 0 && closingLine > line) {
                const LONG end = std::min<LONG>(static_cast<LONG>(text.size()), position + 1);
                document.folds.push_back({opening + 1, end, static_cast<int>(line), false});
            }
        }
    }
    std::sort(document.folds.begin(), document.folds.end(), [](const FoldRegion& left, const FoldRegion& right) {
        if (left.line != right.line) return left.line < right.line;
        return left.start < right.start;
    });
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
    const int line = static_cast<int>(SendMessageW(document->editor, EM_EXLINEFROMCHAR, 0, selection.cpMin));
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

bool ApplyMultiCaretReplacement(HWND editor, const std::wstring& replacement) {
    TabDocument* document = ActiveDocument();
    if (!document || document->multiCarets.size() < 2) return false;
    std::vector<MultiCaret> ranges = document->multiCarets;
    std::sort(ranges.begin(), ranges.end(), [](const MultiCaret& left, const MultiCaret& right) {
        return std::min(left.anchor, left.caret) > std::min(right.anchor, right.caret);
    });
    EditorEventMute mute(editor);
    for (const MultiCaret& range : ranges) {
        const LONG start = std::min(range.anchor, range.caret);
        const LONG end = std::max(range.anchor, range.caret);
        SendMessageW(editor, EM_SETSEL, start, end);
        SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(replacement.c_str()));
    }
    for (MultiCaret& range : document->multiCarets) {
        range.anchor = range.caret = std::min(range.anchor, range.caret) + static_cast<LONG>(replacement.size());
    }
    SendMessageW(editor, EM_SETSEL, document->multiCarets.front().caret, document->multiCarets.front().caret);
    InvalidateRect(editor, nullptr, FALSE);
    return true;
}

bool ApplyMultiCaretBackspace(HWND editor, bool forward) {
    TabDocument* document = ActiveDocument();
    if (!document || document->multiCarets.size() < 2) return false;
    std::vector<MultiCaret> ranges = document->multiCarets;
    std::sort(ranges.begin(), ranges.end(), [](const MultiCaret& left, const MultiCaret& right) {
        return std::min(left.anchor, left.caret) > std::min(right.anchor, right.caret);
    });
    EditorEventMute mute(editor);
    for (const MultiCaret& range : ranges) {
        LONG start = std::min(range.anchor, range.caret);
        LONG end = std::max(range.anchor, range.caret);
        if (start == end) {
            if (forward) end = start + 1;
            else start = std::max<LONG>(0, start - 1);
        }
        SendMessageW(editor, EM_SETSEL, start, end);
        SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L""));
    }
    for (MultiCaret& range : document->multiCarets) {
        LONG position = std::min(range.anchor, range.caret);
        if (range.anchor == range.caret && !forward) position = std::max<LONG>(0, position - 1);
        range.anchor = range.caret = position;
    }
    SendMessageW(editor, EM_SETSEL, document->multiCarets.front().caret, document->multiCarets.front().caret);
    InvalidateRect(editor, nullptr, FALSE);
    return true;
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

// ===========================================================================
// Syntax highlighting
// Each language has a small tokenizer that runs over the whole text (fast,
// pure string scanning). Colours are applied lazily: only the visible part of
// the editor plus a margin, and more as the user scrolls. That keeps large
// files instant to open and typing responsive.
// ===========================================================================

constexpr size_t kSyntaxSizeLimit = 2u << 20;  // characters; bigger files stay plain

struct SyntaxStyle { COLORREF color; bool bold; };

SyntaxStyle StyleFor(Tok kind) {
    const bool dark = g_app.darkTheme;
    const ThemePalette& palette = CurrentPalette();
    const auto pick = [dark](COLORREF d, COLORREF l) { return dark ? d : l; };
    switch (kind) {
    case Tok::Keyword:   return {pick(RGB(197, 134, 192), RGB(175, 0, 219)), false};
    case Tok::Type:      return {pick(RGB(78, 201, 176), RGB(38, 127, 153)), false};
    case Tok::String:    return {pick(RGB(206, 145, 120), RGB(163, 21, 21)), false};
    case Tok::Number:    return {pick(RGB(181, 206, 168), RGB(9, 134, 88)), false};
    case Tok::Comment:   return {pick(RGB(106, 153, 85), RGB(0, 128, 0)), false};
    case Tok::Function:  return {pick(RGB(220, 220, 170), RGB(121, 94, 38)), false};
    case Tok::Tag:       return {pick(RGB(86, 156, 214), RGB(128, 0, 0)), false};
    case Tok::Attribute: return {pick(RGB(156, 220, 254), RGB(229, 0, 0)), false};
    case Tok::Property:  return {pick(RGB(156, 220, 254), RGB(4, 81, 165)), false};
    case Tok::Constant:  return {pick(RGB(86, 156, 214), RGB(0, 0, 255)), false};
    case Tok::Meta:      return {pick(RGB(197, 134, 192), RGB(175, 0, 219)), false};
    case Tok::Variable:  return {pick(RGB(156, 220, 254), RGB(0, 16, 128)), false};
    case Tok::Heading:   return {pick(RGB(86, 156, 214), RGB(128, 0, 0)), true};
    case Tok::Emphasis:  return {palette.editorText, true};
    case Tok::Link:      return {pick(RGB(78, 201, 176), RGB(0, 112, 193)), false};
    case Tok::Error:     return {pick(RGB(244, 135, 113), RGB(205, 49, 49)), true};
    case Tok::Warning:   return {pick(RGB(229, 192, 123), RGB(191, 136, 3)), true};
    case Tok::Info:      return {pick(RGB(117, 190, 255), RGB(0, 112, 193)), false};
    case Tok::Added:     return {pick(RGB(129, 199, 132), RGB(22, 130, 52)), false};
    case Tok::Removed:   return {pick(RGB(244, 135, 113), RGB(205, 49, 49)), false};
    case Tok::Column0:   return {pick(RGB(156, 220, 254), RGB(4, 81, 165)), false};
    case Tok::Column1:   return {pick(RGB(206, 145, 120), RGB(163, 21, 21)), false};
    case Tok::Column2:   return {pick(RGB(181, 206, 168), RGB(9, 134, 88)), false};
    case Tok::Column3:   return {pick(RGB(197, 134, 192), RGB(175, 0, 219)), false};
    case Tok::Column4:   return {pick(RGB(220, 220, 170), RGB(121, 94, 38)), false};
    case Tok::Column5:   return {pick(RGB(78, 201, 176), RGB(38, 127, 153)), false};
    default:             return {palette.editorText, false};
    }
}

// ---- Language specs --------------------------------------------------------------

struct CodeSpec {
    std::vector<std::wstring> lineComments;
    std::wstring blockOpen, blockClose;
    std::wstring blockOpen2, blockClose2;
    std::wstring quotes = L"\"'";
    bool multilineQuotes = false;   // every quote may span lines (shell, PowerShell)
    bool backtickMultiline = true;  // `template` strings
    bool tripleQuotes = false;      // Python """ """
    bool backslashEscapes = true;
    bool hashMeta = false;          // #include / #region at line start
    bool decorators = false;        // @Decorator
    bool dollarVariables = false;   // $name, ${name}
    bool percentVariables = false;  // %name%, %~dp0, !name!
    bool dashIdentifiers = false;   // Get-ChildItem, -Parameter
    bool caseInsensitive = false;
    bool shellComments = false;     // '#' starts a comment only after whitespace
    bool macroBang = false;         // name!(...) is a macro call (Rust)
    std::unordered_set<std::wstring> keywords, types, constants, builtins;
};

std::unordered_set<std::wstring> WordSet(const wchar_t* words, bool lower = false) {
    std::unordered_set<std::wstring> set;
    std::wistringstream stream(words);
    for (std::wstring word; stream >> word;) set.insert(lower ? LowerInvariantSyntax(word) : word);
    return set;
}

const CodeSpec* SpecFor(int language) {
    static std::unordered_map<int, CodeSpec> specs = [] {
        std::unordered_map<int, CodeSpec> m;
        const wchar_t* jsKeywords = L"break case catch class const continue debugger default delete do else export extends finally for "
            L"function if import in instanceof let new return super switch this throw try typeof var void while with yield async await "
            L"of static get set from as";
        CodeSpec js;
        js.lineComments = {L"//"};
        js.blockOpen = L"/*"; js.blockClose = L"*/";
        js.quotes = L"\"'`";
        js.decorators = true;
        js.keywords = WordSet(jsKeywords);
        js.constants = WordSet(L"true false null undefined NaN Infinity");
        m[kLanguageJavaScript] = js;
        CodeSpec ts = js;
        for (const auto& w : WordSet(L"interface type enum implements namespace module declare abstract private protected public readonly keyof infer is satisfies override")) ts.keywords.insert(w);
        ts.types = WordSet(L"string number boolean any unknown never object symbol bigint");
        m[kLanguageTypeScript] = ts;

        CodeSpec c;
        c.lineComments = {L"//"};
        c.blockOpen = L"/*"; c.blockClose = L"*/";
        c.hashMeta = true;
        c.keywords = WordSet(L"alignas alignof auto break case catch class const constexpr consteval constinit const_cast continue "
            L"co_await co_return co_yield decltype default delete do dynamic_cast else enum explicit export extern for friend goto if "
            L"inline mutable namespace new noexcept operator private protected public register reinterpret_cast return sizeof static "
            L"static_assert static_cast struct switch template this thread_local throw try typedef typeid typename union using virtual "
            L"volatile while override final");
        c.types = WordSet(L"void bool char wchar_t char8_t char16_t char32_t short int long float double signed unsigned size_t "
            L"ptrdiff_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t intptr_t uintptr_t std string wstring "
            L"BOOL DWORD WORD BYTE UINT LONG HWND HANDLE LPARAM WPARAM LRESULT");
        c.constants = WordSet(L"true false nullptr NULL TRUE FALSE");
        m[kLanguageCpp] = c;

        CodeSpec cs = c;
        cs.keywords = WordSet(L"abstract as base break case catch checked class const continue default delegate do else enum event "
            L"explicit extern finally fixed for foreach goto if implicit in interface internal is lock namespace new operator out "
            L"override params private protected public readonly ref return sealed sizeof stackalloc static struct switch this throw "
            L"try typeof unchecked unsafe using virtual volatile while async await var get set init record yield where when nameof partial");
        cs.types = WordSet(L"bool byte sbyte char decimal double float int uint long ulong short ushort object string void dynamic nint nuint");
        cs.constants = WordSet(L"true false null");
        cs.decorators = false;
        m[kLanguageCSharp] = cs;

        CodeSpec java = c;
        java.hashMeta = false;
        java.decorators = true;
        java.keywords = WordSet(L"abstract assert break case catch class continue default do else enum extends final finally for if "
            L"implements import instanceof interface native new package private protected public return static strictfp super switch "
            L"synchronized this throw throws transient try volatile while var record yield sealed permits fun val when object "
            L"companion data suspend override open internal lateinit");
        java.types = WordSet(L"boolean byte char short int long float double void String Object Integer Long Double Boolean Unit Any");
        java.constants = WordSet(L"true false null");
        m[kLanguageJava] = java;

        CodeSpec go = c;
        go.hashMeta = false;
        go.quotes = L"\"'`";
        go.keywords = WordSet(L"break case chan const continue default defer else fallthrough for func go goto if import interface "
            L"map package range return select struct switch type var");
        go.types = WordSet(L"bool byte complex64 complex128 error float32 float64 int int8 int16 int32 int64 rune string uint uint8 "
            L"uint16 uint32 uint64 uintptr any");
        go.constants = WordSet(L"true false nil iota");
        m[kLanguageGo] = go;

        CodeSpec rust = c;
        rust.hashMeta = false;
        rust.macroBang = true;
        rust.keywords = WordSet(L"as async await break const continue crate dyn else enum extern fn for if impl in let loop match mod "
            L"move mut pub ref return self Self static struct super trait type unsafe use where while");
        rust.types = WordSet(L"i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 bool char str String Vec Option Result Box");
        rust.constants = WordSet(L"true false None Some Ok Err");
        m[kLanguageRust] = rust;

        CodeSpec php = c;
        php.hashMeta = false;
        php.lineComments = {L"//", L"#"};
        php.dollarVariables = true;
        php.keywords = WordSet(L"abstract and array as break callable case catch class clone const continue declare default do echo "
            L"else elseif empty enddeclare endfor endforeach endif endswitch endwhile extends final finally fn for foreach function "
            L"global goto if implements include include_once instanceof insteadof interface isset list match namespace new or print "
            L"private protected public readonly require require_once return static switch throw trait try unset use var while xor yield");
        php.types = WordSet(L"int float string bool void mixed iterable self parent");
        php.constants = WordSet(L"true false null TRUE FALSE NULL");
        m[kLanguagePhp] = php;

        CodeSpec py;
        py.lineComments = {L"#"};
        py.tripleQuotes = true;
        py.decorators = true;
        py.keywords = WordSet(L"and as assert async await break class continue def del elif else except finally for from global if "
            L"import in is lambda nonlocal not or pass raise return try while with yield match case");
        py.types = WordSet(L"int float str bool list dict set tuple bytes object type");
        py.constants = WordSet(L"True False None self cls");
        py.builtins = WordSet(L"print len range open isinstance enumerate zip map filter sorted super");
        m[kLanguagePython] = py;

        CodeSpec sql;
        sql.lineComments = {L"--", L"#"};
        sql.blockOpen = L"/*"; sql.blockClose = L"*/";
        sql.quotes = L"\"'`";
        sql.backslashEscapes = false;
        sql.caseInsensitive = true;
        sql.keywords = WordSet(L"select from where and or not insert into values update set delete create table alter drop index view "
            L"join left right inner outer full cross on as group by order having limit offset union all distinct case when then else "
            L"end is null in exists between like primary key foreign references default constraint unique check if begin commit "
            L"rollback transaction declare procedure function returns return trigger database schema use show grant revoke with asc "
            L"desc top truncate replace merge using natural over partition cast add column", true);
        sql.types = WordSet(L"int integer bigint smallint tinyint decimal numeric float real double char varchar nvarchar text date "
            L"datetime timestamp time boolean bool blob json jsonb uuid serial", true);
        sql.constants = WordSet(L"true false", true);
        sql.builtins = WordSet(L"count sum avg min max now coalesce concat substring lower upper", true);
        m[kLanguageSql] = sql;

        CodeSpec shell;
        shell.lineComments = {L"#"};
        shell.shellComments = true;
        shell.quotes = L"\"'`";
        shell.multilineQuotes = true;
        shell.dollarVariables = true;
        shell.keywords = WordSet(L"if then else elif fi for while until do done case esac in function return break continue local "
            L"export readonly declare unset shift exit source alias select time trap eval exec set");
        shell.builtins = WordSet(L"echo printf cd ls cp mv rm mkdir grep sed awk cat chmod chown sudo apt yum dnf npm npx pnpm yarn "
            L"git docker curl wget tar ssh systemctl node python pip");
        shell.constants = WordSet(L"true false");
        m[kLanguageShell] = shell;

        CodeSpec ps;
        ps.lineComments = {L"#"};
        ps.blockOpen = L"<#"; ps.blockClose = L"#>";
        ps.multilineQuotes = true;
        ps.backslashEscapes = false;
        ps.dollarVariables = true;
        ps.dashIdentifiers = true;
        ps.caseInsensitive = true;
        ps.keywords = WordSet(L"begin break catch class continue data do dynamicparam else elseif end enum exit filter finally for "
            L"foreach from function if in param process return switch throw trap try until using while", true);
        m[kLanguagePowerShell] = ps;

        CodeSpec bat;
        bat.lineComments = {L"::"};
        bat.quotes = L"\"";
        bat.backslashEscapes = false;
        bat.percentVariables = true;
        bat.caseInsensitive = true;
        bat.keywords = WordSet(L"echo set if else for in do goto call exit not exist defined errorlevel setlocal endlocal shift pause "
            L"cls cd pushd popd start title copy del move mkdir md rd rmdir type find findstr equ neq lss leq gtr geq nul off on", true);
        m[kLanguageBatch] = bat;
        return m;
    }();
    const auto it = specs.find(language);
    return it == specs.end() ? nullptr : &it->second;
}

// ---- Lexer ---------------------------------------------------------------------------

bool IsIdentStart(wchar_t c) { return c == L'_' || (c < 128 ? iswalpha(c) != 0 : (iswalpha(c) && c < 0x2E80)); }
bool IsIdentChar(wchar_t c) { return IsIdentStart(c) || (c >= L'0' && c <= L'9'); }

class SyntaxLexer {
public:
    SyntaxLexer(const std::wstring& text, std::vector<SyntaxToken>& tokens) : text_(text), tokens_(tokens) {}

    void Run(int language, size_t from, size_t to) {
        switch (language) {
        case kLanguageHtml:
        case kLanguageXml: Markup(from, to); break;
        case kLanguageCss: Css(from, to); break;
        case kLanguageJson: Json(from, to); break;
        case kLanguageMarkdown: Markdown(from, to); break;
        case kLanguageYaml: Yaml(from, to); break;
        case kLanguageConfig: Config(from, to); break;
        case kLanguageDockerfile: Dockerfile(from, to); break;
        case kLanguageCsv: Csv(from, to); break;
        case kLanguageLog: Log(from, to); break;
        case kLanguageDiff: Diff(from, to); break;
        case kLanguageBatch: Batch(from, to); break;
        default:
            if (const CodeSpec* spec = SpecFor(language)) Code(*spec, from, to);
            break;
        }
    }

private:
    const std::wstring& text_;
    std::vector<SyntaxToken>& tokens_;

    void Add(size_t start, size_t end, Tok kind) {
        if (end > start && kind != Tok::Plain) tokens_.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(end - start), kind});
    }
    bool At(size_t i, const std::wstring& s) const { return !s.empty() && text_.compare(i, s.size(), s) == 0; }
    bool AtNoCase(size_t i, const wchar_t* s) const {
        const size_t n = wcslen(s);
        return i + n <= text_.size() && _wcsnicmp(text_.c_str() + i, s, n) == 0;
    }
    size_t LineEnd(size_t i, size_t to) const {
        const size_t end = text_.find_first_of(L"\r\n", i);
        return end == std::wstring::npos ? to : std::min(end, to);
    }
    size_t NextLine(size_t lineEnd, size_t to) const {
        if (lineEnd < to && text_[lineEnd] == L'\r' && lineEnd + 1 < to && text_[lineEnd + 1] == L'\n') return lineEnd + 2;
        return std::min(lineEnd + 1, to);
    }
    size_t SkipSpaces(size_t i, size_t to) const {
        while (i < to && (text_[i] == L' ' || text_[i] == L'\t')) ++i;
        return i;
    }
    size_t NumberEnd(size_t i, size_t to) const {
        size_t j = i;
        const bool hex = j + 1 < to && text_[j] == L'0' && (text_[j + 1] == L'x' || text_[j + 1] == L'X');
        if (hex) j += 2;
        while (j < to) {
            const wchar_t c = text_[j];
            if (iswalnum(c) || c == L'_' || (c == L'.' && j + 1 < to && iswdigit(text_[j + 1]))) { ++j; continue; }
            if (!hex && (c == L'+' || c == L'-') && j > i && (text_[j - 1] == L'e' || text_[j - 1] == L'E')) { ++j; continue; }
            break;
        }
        return j;
    }
    // Ends after the closing quote (or at the line end for an unterminated one).
    size_t StringEnd(size_t i, size_t to, wchar_t quote, bool multiline, bool escapes) const {
        size_t j = i + 1;
        while (j < to) {
            const wchar_t c = text_[j];
            if (escapes && c == L'\\') { j += 2; continue; }
            if (c == quote) return j + 1;
            if ((c == L'\r' || c == L'\n') && !multiline) return j;
            ++j;
        }
        return to;
    }
    wchar_t PreviousNonSpace(size_t i, size_t from) const {
        while (i > from) {
            const wchar_t c = text_[--i];
            if (c != L' ' && c != L'\t') return c;
        }
        return 0;
    }
    std::wstring Lower(size_t start, size_t end) const { return LowerInvariantSyntax(text_.substr(start, end - start)); }

    void Code(const CodeSpec& spec, size_t from, size_t to) {
        size_t i = from;
        bool lineStart = true;
        while (i < to) {
            const wchar_t c = text_[i];
            if (c == L'\r' || c == L'\n') { lineStart = true; ++i; continue; }
            if (c == L' ' || c == L'\t') { ++i; continue; }
            if (At(i, spec.blockOpen)) {
                const size_t close = text_.find(spec.blockClose, i + spec.blockOpen.size());
                const size_t end = close == std::wstring::npos ? to : std::min(to, close + spec.blockClose.size());
                Add(i, end, Tok::Comment);
                i = end; lineStart = false; continue;
            }
            bool comment = false;
            for (const std::wstring& marker : spec.lineComments) {
                if (!At(i, marker)) continue;
                if (spec.shellComments && i > from && text_[i - 1] != L' ' && text_[i - 1] != L'\t' && text_[i - 1] != L'\r' && text_[i - 1] != L'\n') continue;
                const size_t end = LineEnd(i, to);
                Add(i, end, Tok::Comment);
                i = end;
                comment = true;
                break;
            }
            if (comment) continue;
            if (spec.hashMeta && c == L'#' && lineStart) {
                const size_t end = LineEnd(i, to);
                // #include <x> / "x": keep the path as a string.
                const size_t quote = text_.find_first_of(L"<\"", i);
                if (quote != std::wstring::npos && quote < end && text_.compare(i, 8, L"#include") == 0) {
                    Add(i, quote, Tok::Meta);
                    Add(quote, end, Tok::String);
                } else {
                    Add(i, end, Tok::Meta);
                }
                i = end; continue;
            }
            lineStart = false;
            if (spec.quotes.find(c) != std::wstring::npos) {
                size_t end;
                if (spec.tripleQuotes && i + 2 < to && text_[i + 1] == c && text_[i + 2] == c) {
                    const size_t close = text_.find(std::wstring(3, c), i + 3);
                    end = close == std::wstring::npos ? to : std::min(to, close + 3);
                } else {
                    end = StringEnd(i, to, c, spec.multilineQuotes || (c == L'`' && spec.backtickMultiline), spec.backslashEscapes);
                }
                Add(i, end, Tok::String);
                i = end; continue;
            }
            if (iswdigit(c) || (c == L'.' && i + 1 < to && iswdigit(text_[i + 1]) && (i == from || !IsIdentChar(text_[i - 1])))) {
                if (i > from && IsIdentChar(text_[i - 1])) { ++i; continue; }
                const size_t end = NumberEnd(i, to);
                Add(i, end, Tok::Number);
                i = end; continue;
            }
            if (spec.decorators && c == L'@' && i + 1 < to && IsIdentStart(text_[i + 1])) {
                size_t j = i + 1;
                while (j < to && (IsIdentChar(text_[j]) || text_[j] == L'.')) ++j;
                Add(i, j, Tok::Meta);
                i = j; continue;
            }
            if (spec.dollarVariables && c == L'$' && i + 1 < to) {
                size_t j = i + 1;
                if (text_[j] == L'{') {
                    const size_t close = text_.find(L'}', j);
                    j = close == std::wstring::npos || close > LineEnd(i, to) ? j + 1 : close + 1;
                } else {
                    while (j < to && (IsIdentChar(text_[j]) || (spec.dashIdentifiers && text_[j] == L':'))) ++j;
                    if (j == i + 1 && j < to && (iswdigit(text_[j]) || wcschr(L"@#?*!$-_", text_[j]))) ++j;
                }
                if (j > i + 1) {
                    Add(i, j, Tok::Variable);
                    i = j; continue;
                }
            }
            if (spec.percentVariables && (c == L'%' || c == L'!') && i + 1 < to) {
                size_t j = i + 1;
                if (c == L'%' && (text_[j] == L'%' || text_[j] == L'~' || iswdigit(text_[j]))) {
                    if (text_[j] == L'%') ++j;
                    while (j < to && (iswalnum(text_[j]) || text_[j] == L'~')) ++j;
                    Add(i, j, Tok::Variable);
                    i = j; continue;
                }
                const size_t close = text_.find(c, j);
                if (close != std::wstring::npos && close > j && close - j < 64 && close < LineEnd(i, to)) {
                    bool simple = true;
                    for (size_t k = j; k < close; ++k) {
                        if (!(IsIdentChar(text_[k]) || text_[k] == L':' || text_[k] == L'~' || text_[k] == L'=' || text_[k] == L'.')) simple = false;
                    }
                    if (simple) {
                        Add(i, close + 1, Tok::Variable);
                        i = close + 1; continue;
                    }
                }
            }
            if (spec.dashIdentifiers && c == L'-' && i + 1 < to && IsIdentStart(text_[i + 1]) &&
                (i == from || text_[i - 1] == L' ' || text_[i - 1] == L'\t' || text_[i - 1] == L'(')) {
                size_t j = i + 1;
                while (j < to && IsIdentChar(text_[j])) ++j;
                Add(i, j, Tok::Attribute);
                i = j; continue;
            }
            if (IsIdentStart(c)) {
                size_t j = i + 1;
                while (j < to && (IsIdentChar(text_[j]) ||
                       (spec.dashIdentifiers && text_[j] == L'-' && j + 1 < to && IsIdentStart(text_[j + 1])))) ++j;
                // String prefixes: r"..", f'..', b"..", u8"..", L"..", @"..".
                if (j < to && spec.quotes.find(text_[j]) != std::wstring::npos && j - i <= 2) {
                    bool prefix = true;
                    for (size_t k = i; k < j; ++k) prefix = prefix && wcschr(L"rRbBfFuUL8", text_[k]) != nullptr;
                    if (prefix) {
                        const wchar_t q = text_[j];
                        size_t end;
                        if (spec.tripleQuotes && j + 2 < to && text_[j + 1] == q && text_[j + 2] == q) {
                            const size_t close = text_.find(std::wstring(3, q), j + 3);
                            end = close == std::wstring::npos ? to : std::min(to, close + 3);
                        } else {
                            end = StringEnd(j, to, q, spec.multilineQuotes, spec.backslashEscapes && text_[i] != L'r' && text_[i] != L'R');
                        }
                        Add(i, end, Tok::String);
                        i = end; continue;
                    }
                }
                const std::wstring word = text_.substr(i, j - i);
                const std::wstring key = spec.caseInsensitive ? LowerInvariantSyntax(word) : word;
                Tok kind = Tok::Plain;
                if (spec.keywords.count(key)) kind = Tok::Keyword;
                else if (spec.constants.count(key)) kind = Tok::Constant;
                else if (spec.types.count(key)) kind = Tok::Type;
                else if (spec.builtins.count(key)) kind = Tok::Function;
                else {
                    const size_t next = SkipSpaces(j, to);
                    const wchar_t prev = PreviousNonSpace(i, from);
                    const bool call = next < to && text_[next] == L'(';
                    const bool macro = spec.macroBang && next < to && text_[next] == L'!';
                    bool hasLower = false, hasLetter = false;
                    for (const wchar_t ch : word) {
                        hasLower = hasLower || iswlower(ch);
                        hasLetter = hasLetter || iswalpha(ch);
                    }
                    if (spec.dashIdentifiers && word.find(L'-') != std::wstring::npos) kind = Tok::Function;  // cmdlets
                    else if (call || macro) kind = Tok::Function;
                    else if (prev == L'.' || (prev == L'>' && i >= 2 && text_[i - 2] == L'-')) kind = Tok::Property;
                    else if (iswupper(word[0]) && hasLower && word[0] < 128) kind = Tok::Type;
                    else if (word.size() > 1 && hasLetter && !hasLower && word[0] < 128) kind = Tok::Constant;
                }
                Add(i, j, kind);
                i = j; continue;
            }
            ++i;
        }
    }

    void Markup(size_t from, size_t to) {
        size_t i = from;
        while (i < to) {
            if (At(i, L"<!--")) {
                const size_t close = text_.find(L"-->", i + 4);
                const size_t end = close == std::wstring::npos ? to : std::min(to, close + 3);
                Add(i, end, Tok::Comment);
                i = end; continue;
            }
            if (At(i, L"<![CDATA[")) {
                const size_t close = text_.find(L"]]>", i);
                const size_t end = close == std::wstring::npos ? to : std::min(to, close + 3);
                Add(i, end, Tok::String);
                i = end; continue;
            }
            const wchar_t c = text_[i];
            if (c == L'<' && i + 1 < to && (text_[i + 1] == L'!' || text_[i + 1] == L'?')) {
                const size_t close = text_.find(L'>', i);
                const size_t end = close == std::wstring::npos ? to : std::min(to, close + 1);
                Add(i, end, Tok::Meta);
                i = end; continue;
            }
            if (c == L'<' && i + 1 < to && (IsIdentStart(text_[i + 1]) || text_[i + 1] == L'/')) {
                size_t j = i + 1;
                const bool closing = text_[j] == L'/';
                if (closing) ++j;
                const size_t nameStart = j;
                while (j < to && (IsIdentChar(text_[j]) || text_[j] == L'-' || text_[j] == L':' || text_[j] == L'.')) ++j;
                const std::wstring name = Lower(nameStart, j);
                Add(i, j, Tok::Tag);
                size_t k = j;
                while (k < to) {
                    k = SkipSpaces(k, to);
                    if (k >= to) break;
                    const wchar_t ch = text_[k];
                    if (ch == L'\r' || ch == L'\n') { ++k; continue; }
                    if (ch == L'>') { Add(k, k + 1, Tok::Tag); ++k; break; }
                    if (ch == L'/' && k + 1 < to && text_[k + 1] == L'>') { Add(k, k + 2, Tok::Tag); k += 2; break; }
                    if (ch == L'<') break;  // broken tag
                    if (ch == L'=') {
                        k = SkipSpaces(k + 1, to);
                        if (k < to && (text_[k] == L'"' || text_[k] == L'\'')) {
                            const size_t end = StringEnd(k, to, text_[k], true, false);
                            Add(k, end, Tok::String);
                            k = end;
                        } else {
                            const size_t start = k;
                            while (k < to && !iswspace(text_[k]) && text_[k] != L'>') ++k;
                            Add(start, k, Tok::String);
                        }
                        continue;
                    }
                    const size_t start = k;
                    while (k < to && !iswspace(text_[k]) && text_[k] != L'=' && text_[k] != L'>' && text_[k] != L'/' && text_[k] != L'<') ++k;
                    if (k == start) { ++k; continue; }
                    Add(start, k, Tok::Attribute);
                }
                i = k;
                if (!closing && (name == L"script" || name == L"style")) {
                    size_t close = i;
                    while ((close = text_.find(L"</", close)) != std::wstring::npos && !AtNoCase(close + 2, name.c_str())) close += 2;
                    const size_t end = close == std::wstring::npos ? to : std::min(close, to);
                    Run(name == L"script" ? kLanguageJavaScript : kLanguageCss, i, end);
                    i = end;
                }
                continue;
            }
            if (c == L'&') {
                size_t j = i + 1;
                while (j < to && j - i < 12 && (iswalnum(text_[j]) || text_[j] == L'#')) ++j;
                if (j < to && text_[j] == L';' && j > i + 1) {
                    Add(i, j + 1, Tok::Constant);
                    i = j + 1; continue;
                }
            }
            ++i;
        }
    }

    void Css(size_t from, size_t to) {
        int depth = 0;
        bool inValue = false;
        size_t i = from;
        while (i < to) {
            const wchar_t c = text_[i];
            if (At(i, L"/*")) {
                const size_t close = text_.find(L"*/", i + 2);
                const size_t end = close == std::wstring::npos ? to : std::min(to, close + 2);
                Add(i, end, Tok::Comment);
                i = end; continue;
            }
            if (At(i, L"//") && (i == from || iswspace(text_[i - 1]) || text_[i - 1] == L';' || text_[i - 1] == L'{' || text_[i - 1] == L'}')) {
                const size_t end = LineEnd(i, to);
                Add(i, end, Tok::Comment);
                i = end; continue;
            }
            if (c == L'"' || c == L'\'') {
                const size_t end = StringEnd(i, to, c, false, true);
                Add(i, end, Tok::String);
                i = end; continue;
            }
            if (c == L'{') { ++depth; inValue = false; ++i; continue; }
            if (c == L'}') { depth = std::max(0, depth - 1); inValue = false; ++i; continue; }
            if (c == L';') { inValue = false; ++i; continue; }
            if (c == L'@' && i + 1 < to && IsIdentStart(text_[i + 1])) {
                size_t j = i + 1;
                while (j < to && (IsIdentChar(text_[j]) || text_[j] == L'-')) ++j;
                Add(i, j, inValue ? Tok::Variable : Tok::Keyword);
                i = j; continue;
            }
            if ((c == L'$' && i + 1 < to && IsIdentStart(text_[i + 1])) || At(i, L"--")) {
                size_t j = i + (c == L'$' ? 1 : 2);
                while (j < to && (IsIdentChar(text_[j]) || text_[j] == L'-')) ++j;
                if (j > i + 1) {
                    Add(i, j, Tok::Variable);
                    i = j; continue;
                }
            }
            if (c == L'!' && AtNoCase(i, L"!important")) {
                Add(i, i + 10, Tok::Keyword);
                i += 10; continue;
            }
            if (c == L'#' && inValue) {
                size_t j = i + 1;
                while (j < to && iswxdigit(text_[j])) ++j;
                Add(i, j, Tok::Number);
                i = j; continue;
            }
            if (iswdigit(c) || (c == L'.' && i + 1 < to && iswdigit(text_[i + 1]) && inValue)) {
                size_t j = i;
                while (j < to && (iswdigit(text_[j]) || text_[j] == L'.')) ++j;
                while (j < to && (iswalpha(text_[j]) || text_[j] == L'%')) ++j;  // units
                Add(i, j, Tok::Number);
                i = j; continue;
            }
            if (c == L':' && !inValue && i + 1 < to && (text_[i + 1] == L':' || IsIdentStart(text_[i + 1]))) {
                // A pseudo-class in a selector (a:hover) vs. a property colon (color: red).
                size_t j = i + 1;
                if (text_[j] == L':') ++j;
                const size_t nameStart = j;
                while (j < to && (IsIdentChar(text_[j]) || text_[j] == L'-')) ++j;
                const size_t after = SkipSpaces(j, to);
                const bool selector = depth == 0 || (after < to && (text_[after] == L'{' || text_[after] == L',' || text_[after] == L'('));
                if (selector && j > nameStart) {
                    Add(i, j, Tok::Keyword);
                    i = j; continue;
                }
                inValue = true;
                ++i; continue;
            }
            if (c == L':') { inValue = true; ++i; continue; }
            if (IsIdentStart(c) || c == L'-' || ((c == L'.' || c == L'#') && i + 1 < to && (IsIdentStart(text_[i + 1]) || text_[i + 1] == L'-'))) {
                size_t j = i + 1;
                while (j < to && (IsIdentChar(text_[j]) || text_[j] == L'-')) ++j;
                const size_t next = SkipSpaces(j, to);
                Tok kind;
                if (next < to && text_[next] == L'(') kind = Tok::Function;
                else if (inValue) kind = Tok::Constant;
                else if (depth > 0 && next < to && text_[next] == L':' && !(next + 1 < to && IsIdentStart(text_[next + 1]) && LooksLikeSelector(next, to))) kind = Tok::Property;
                else kind = Tok::Tag;
                Add(i, j, kind);
                i = j; continue;
            }
            ++i;
        }
    }
    // "a:hover {" — a colon followed by a name and then '{' on the same line.
    bool LooksLikeSelector(size_t colon, size_t to) const {
        const size_t end = LineEnd(colon, to);
        const size_t brace = text_.find(L'{', colon);
        const size_t semi = text_.find(L';', colon);
        return brace != std::wstring::npos && brace < end && (semi == std::wstring::npos || semi > brace);
    }

    void Json(size_t from, size_t to) {
        size_t i = from;
        while (i < to) {
            const wchar_t c = text_[i];
            if (At(i, L"//")) { const size_t end = LineEnd(i, to); Add(i, end, Tok::Comment); i = end; continue; }
            if (At(i, L"/*")) {
                const size_t close = text_.find(L"*/", i + 2);
                const size_t end = close == std::wstring::npos ? to : std::min(to, close + 2);
                Add(i, end, Tok::Comment);
                i = end; continue;
            }
            if (c == L'"') {
                const size_t end = StringEnd(i, to, c, false, true);
                const size_t next = SkipSpaces(end, to);
                Add(i, end, next < to && text_[next] == L':' ? Tok::Property : Tok::String);
                i = end; continue;
            }
            if (iswdigit(c) || (c == L'-' && i + 1 < to && iswdigit(text_[i + 1]))) {
                const size_t end = NumberEnd(c == L'-' ? i + 1 : i, to);
                Add(i, end, Tok::Number);
                i = end; continue;
            }
            if (IsIdentStart(c)) {
                size_t j = i;
                while (j < to && IsIdentChar(text_[j])) ++j;
                const std::wstring word = text_.substr(i, j - i);
                if (word == L"true" || word == L"false" || word == L"null") Add(i, j, Tok::Constant);
                i = j; continue;
            }
            ++i;
        }
    }

    // A value after '=' / ':' in config-like files.
    void Value(size_t start, size_t end) {
        start = SkipSpaces(start, end);
        while (end > start && (text_[end - 1] == L' ' || text_[end - 1] == L'\t')) --end;
        if (start >= end) return;
        const wchar_t c = text_[start];
        if (c == L'"' || c == L'\'') { Add(start, end, Tok::String); return; }
        const std::wstring lower = Lower(start, end);
        if (lower == L"true" || lower == L"false" || lower == L"yes" || lower == L"no" || lower == L"on" ||
            lower == L"off" || lower == L"null" || lower == L"~" || lower == L"none") {
            Add(start, end, Tok::Constant);
            return;
        }
        bool numeric = true;
        for (size_t k = start; k < end; ++k) {
            const wchar_t ch = text_[k];
            if (!(iswdigit(ch) || ch == L'.' || ch == L'-' || ch == L'+' || ch == L'_' || ch == L'e' || ch == L'E')) numeric = false;
        }
        if (numeric && iswdigit(text_[end - 1])) { Add(start, end, Tok::Number); return; }
        if (c == L'&' || c == L'*') { Add(start, end, Tok::Variable); return; }
        if ((c == L'|' || c == L'>') && end - start <= 2) { Add(start, end, Tok::Keyword); return; }
        if (c == L'[' || c == L'{') {  // inline arrays / tables: colour the pieces
            Code(*SpecFor(kLanguageJavaScript), start, end);
            return;
        }
        Add(start, end, Tok::String);
    }

    // INI, TOML, .env, .properties, .gitignore and friends.
    void Config(size_t from, size_t to) {
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            const size_t p = SkipSpaces(line, end);
            if (p < end) {
                const wchar_t c = text_[p];
                if (c == L'#' || c == L';' || At(p, L"//")) {
                    Add(p, end, Tok::Comment);
                } else if (c == L'[') {
                    const size_t close = text_.find(L']', p);
                    Add(p, close == std::wstring::npos || close > end ? end : close + 1, Tok::Tag);
                } else {
                    size_t keyStart = p;
                    if (AtNoCase(p, L"export ") || AtNoCase(p, L"set ")) {
                        const size_t word = text_.find(L' ', p);
                        Add(p, word, Tok::Keyword);
                        keyStart = SkipSpaces(word, end);
                    }
                    const size_t equals = text_.find_first_of(L"=:", keyStart);
                    if (equals != std::wstring::npos && equals < end) {
                        size_t keyEnd = equals;
                        while (keyEnd > keyStart && (text_[keyEnd - 1] == L' ' || text_[keyEnd - 1] == L'\t')) --keyEnd;
                        Add(keyStart, keyEnd, Tok::Property);
                        // Inline comment after a value: "value  # note"
                        size_t valueEnd = end;
                        const size_t hash = text_.find(L" #", equals);
                        if (hash != std::wstring::npos && hash < end && text_.find_first_of(L"\"'", equals) > hash) {
                            Add(hash + 1, end, Tok::Comment);
                            valueEnd = hash;
                        }
                        Value(equals + 1, valueEnd);
                    }
                }
            }
            line = NextLine(end, to);
        }
    }

    void Yaml(size_t from, size_t to) {
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            size_t p = SkipSpaces(line, end);
            if (p < end) {
                if (text_[p] == L'#') {
                    Add(p, end, Tok::Comment);
                } else if (At(p, L"---") || At(p, L"...")) {
                    Add(p, end, Tok::Meta);
                } else {
                    if (text_[p] == L'-' && (p + 1 == end || text_[p + 1] == L' ')) {
                        Add(p, p + 1, Tok::Keyword);
                        p = SkipSpaces(p + 1, end);
                    }
                    // Strip a trailing comment that is not inside quotes.
                    size_t contentEnd = end;
                    bool quoted = false;
                    wchar_t quote = 0;
                    for (size_t k = p; k < end; ++k) {
                        const wchar_t ch = text_[k];
                        if (quoted) { if (ch == quote) quoted = false; continue; }
                        if (ch == L'"' || ch == L'\'') { quoted = true; quote = ch; continue; }
                        if (ch == L'#' && k > p && (text_[k - 1] == L' ' || text_[k - 1] == L'\t')) {
                            Add(k, end, Tok::Comment);
                            contentEnd = k;
                            break;
                        }
                    }
                    size_t colon = std::wstring::npos;
                    if (p < contentEnd && text_[p] != L'"' && text_[p] != L'\'' && text_[p] != L'[' && text_[p] != L'{') {
                        for (size_t k = p; k < contentEnd; ++k) {
                            if (text_[k] == L':' && (k + 1 == contentEnd || text_[k + 1] == L' ' || text_[k + 1] == L'\t')) { colon = k; break; }
                        }
                    }
                    if (colon != std::wstring::npos) {
                        Add(p, colon, Tok::Property);
                        Value(colon + 1, contentEnd);
                    } else {
                        Value(p, contentEnd);
                    }
                }
            }
            line = NextLine(end, to);
        }
    }

    void Markdown(size_t from, size_t to) {
        bool inFence = false;
        int fenceLanguage = kLanguagePlain;
        size_t codeStart = 0;
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            const size_t next = NextLine(end, to);
            const size_t p = SkipSpaces(line, end);
            const bool fence = At(p, L"```") || At(p, L"~~~");
            if (inFence) {
                if (fence) {
                    if (fenceLanguage != kLanguagePlain) Run(fenceLanguage, codeStart, line);
                    else Add(codeStart, line, Tok::String);
                    Add(p, end, Tok::Meta);
                    inFence = false;
                }
                line = next;
                continue;
            }
            if (fence) {
                inFence = true;
                size_t infoStart = SkipSpaces(p + 3, end), infoEnd = infoStart;
                while (infoEnd < end && !iswspace(text_[infoEnd]) && text_[infoEnd] != L'{') ++infoEnd;
                fenceLanguage = LanguageFromName(text_.substr(infoStart, infoEnd - infoStart));
                Add(p, end, Tok::Meta);
                codeStart = next;
                line = next;
                continue;
            }
            size_t hashes = p;
            while (hashes < end && text_[hashes] == L'#') ++hashes;
            if (hashes > p && hashes - p <= 6 && (hashes == end || text_[hashes] == L' ')) {
                Add(p, end, Tok::Heading);
            } else if (p < end && text_[p] == L'>') {
                Add(p, end, Tok::Comment);
            } else if (end - p >= 3 && (At(p, L"---") || At(p, L"***") || At(p, L"___")) &&
                       text_.find_first_not_of(text_[p], p) >= end) {
                Add(p, end, Tok::Meta);
            } else {
                size_t q = p;
                if (q < end && (text_[q] == L'-' || text_[q] == L'*' || text_[q] == L'+') && q + 1 < end && text_[q + 1] == L' ') {
                    Add(q, q + 1, Tok::Keyword);
                    q += 2;
                } else {
                    size_t digits = q;
                    while (digits < end && iswdigit(text_[digits])) ++digits;
                    if (digits > q && digits + 1 < end && (text_[digits] == L'.' || text_[digits] == L')') && text_[digits + 1] == L' ') {
                        Add(q, digits + 1, Tok::Keyword);
                        q = digits + 2;
                    }
                }
                Inline(q, end);
            }
            line = next;
        }
        if (inFence && codeStart < to) {
            if (fenceLanguage != kLanguagePlain) Run(fenceLanguage, codeStart, to);
            else Add(codeStart, to, Tok::String);
        }
    }
    void Inline(size_t i, size_t end) {
        while (i < end) {
            const wchar_t c = text_[i];
            if (c == L'`') {
                const size_t close = text_.find(L'`', i + 1);
                if (close != std::wstring::npos && close < end) { Add(i, close + 1, Tok::String); i = close + 1; continue; }
            }
            if ((c == L'*' || c == L'_') && i + 1 < end && text_[i + 1] == c) {
                const size_t close = text_.find(std::wstring(2, c), i + 2);
                if (close != std::wstring::npos && close < end) { Add(i, close + 2, Tok::Emphasis); i = close + 2; continue; }
            }
            if (c == L'[' || (c == L'!' && i + 1 < end && text_[i + 1] == L'[')) {
                const size_t closeText = text_.find(L"](", i);
                if (closeText != std::wstring::npos && closeText < end) {
                    const size_t closeUrl = text_.find(L')', closeText);
                    if (closeUrl != std::wstring::npos && closeUrl < end) { Add(i, closeUrl + 1, Tok::Link); i = closeUrl + 1; continue; }
                }
            }
            if (At(i, L"http://") || At(i, L"https://")) {
                size_t j = i;
                while (j < end && !iswspace(text_[j]) && text_[j] != L')' && text_[j] != L'>') ++j;
                Add(i, j, Tok::Link);
                i = j; continue;
            }
            ++i;
        }
    }

    void Dockerfile(size_t from, size_t to) {
        static const std::unordered_set<std::wstring> instructions = WordSet(L"from run cmd label maintainer expose env add copy "
            L"entrypoint volume user workdir arg onbuild stopsignal healthcheck shell as", true);
        const CodeSpec* shell = SpecFor(kLanguageShell);
        bool continuation = false;
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            size_t p = SkipSpaces(line, end);
            if (p < end && text_[p] == L'#') {
                Add(p, end, Tok::Comment);
            } else if (p < end) {
                size_t word = p;
                while (word < end && IsIdentChar(text_[word])) ++word;
                if (!continuation && instructions.count(Lower(p, word))) {
                    Add(p, word, Tok::Keyword);
                    p = word;
                }
                Code(*shell, p, end);
            }
            continuation = end > line && text_[end - 1] == L'\\';
            line = NextLine(end, to);
        }
    }

    void Batch(size_t from, size_t to) {
        const CodeSpec* spec = SpecFor(kLanguageBatch);
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            size_t p = SkipSpaces(line, end);
            if (p < end && text_[p] == L'@') ++p;
            if (AtNoCase(p, L"rem") && (p + 3 == end || iswspace(text_[p + 3]))) Add(p, end, Tok::Comment);
            else if (At(p, L"::")) Add(p, end, Tok::Comment);
            else if (p < end && text_[p] == L':') Add(p, end, Tok::Tag);
            else Code(*spec, p, end);
            line = NextLine(end, to);
        }
    }

    // Rainbow columns, like VS Code's Rainbow CSV.
    void Csv(size_t from, size_t to) {
        const size_t firstEnd = LineEnd(from, to);
        const std::wstring firstLine = text_.substr(from, firstEnd - from);
        wchar_t delimiter = L',';
        const auto count = [&](wchar_t ch) { return std::count(firstLine.begin(), firstLine.end(), ch); };
        if (count(L'\t') > count(delimiter)) delimiter = L'\t';
        if (count(L';') > count(delimiter)) delimiter = L';';
        static constexpr Tok columns[] = {Tok::Column0, Tok::Column1, Tok::Column2, Tok::Column3, Tok::Column4, Tok::Column5};
        size_t i = from;
        int column = 0;
        size_t fieldStart = from;
        bool quoted = false;
        while (i <= to) {
            const wchar_t c = i < to ? text_[i] : L'\n';
            if (quoted) {
                if (c == L'"') {
                    if (i + 1 < to && text_[i + 1] == L'"') { i += 2; continue; }
                    quoted = false;
                }
                ++i;
                continue;
            }
            if (c == L'"' && i == fieldStart) { quoted = true; ++i; continue; }
            if (c == delimiter || c == L'\r' || c == L'\n') {
                Add(fieldStart, i, columns[column % 6]);
                if (c == delimiter) ++column;
                else column = 0;
                fieldStart = i + 1;
                if (c == L'\r' && i + 1 < to && text_[i + 1] == L'\n') { ++i; fieldStart = i + 1; }
            }
            ++i;
        }
    }

    void Log(size_t from, size_t to) {
        static const wchar_t* errors[] = {L"FATAL", L"CRITICAL", L"ERROR", L"ERR", L"SEVERE", L"EXCEPTION", L"FAILED", L"FAIL", L"PANIC"};
        static const wchar_t* warnings[] = {L"WARNING", L"WARN"};
        static const wchar_t* infos[] = {L"INFO", L"DEBUG", L"TRACE", L"NOTICE", L"VERBOSE"};
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            size_t i = line;
            // Leading timestamp: digits, - / : . T space and brackets.
            size_t stamp = SkipSpaces(i, end);
            const size_t stampStart = stamp;
            if (stamp < end && text_[stamp] == L'[') ++stamp;
            size_t digits = 0;
            while (stamp < end && (iswdigit(text_[stamp]) || wcschr(L"-/:.,TZ+ ", text_[stamp]))) {
                if (iswdigit(text_[stamp])) ++digits;
                ++stamp;
            }
            if (stamp < end && text_[stamp] == L']') ++stamp;
            if (digits >= 6) {
                Add(stampStart, stamp, Tok::Meta);
                i = stamp;
            }
            while (i < end) {
                const wchar_t c = text_[i];
                if (c == L'"' || c == L'\'') {
                    const size_t close = StringEnd(i, end, c, false, true);
                    Add(i, close, Tok::String);
                    i = close; continue;
                }
                if (At(i, L"http://") || At(i, L"https://")) {
                    size_t j = i;
                    while (j < end && !iswspace(text_[j])) ++j;
                    Add(i, j, Tok::Link);
                    i = j; continue;
                }
                if (IsIdentStart(c) && (i == line || !IsIdentChar(text_[i - 1]))) {
                    size_t j = i;
                    while (j < end && IsIdentChar(text_[j])) ++j;
                    const std::wstring word = text_.substr(i, j - i);
                    const auto matches = [&](const auto& list) {
                        for (const wchar_t* level : list) if (_wcsicmp(word.c_str(), level) == 0 && iswupper(word[0])) return true;
                        return false;
                    };
                    if (matches(errors)) Add(i, j, Tok::Error);
                    else if (matches(warnings)) Add(i, j, Tok::Warning);
                    else if (matches(infos)) Add(i, j, Tok::Info);
                    i = j; continue;
                }
                ++i;
            }
            line = NextLine(end, to);
        }
    }

    void Diff(size_t from, size_t to) {
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            if (At(line, L"+++") || At(line, L"---") || At(line, L"diff ") || At(line, L"index ")) Add(line, end, Tok::Keyword);
            else if (At(line, L"@@")) Add(line, end, Tok::Meta);
            else if (line < end && text_[line] == L'+') Add(line, end, Tok::Added);
            else if (line < end && text_[line] == L'-') Add(line, end, Tok::Removed);
            line = NextLine(end, to);
        }
    }
};

// ---- Applying colours ------------------------------------------------------------------

bool SyntaxEnabled(const TabDocument& document) {
    return EffectiveLanguage(document) != kLanguagePlain && GetWindowTextLengthW(document.editor) <= static_cast<int>(kSyntaxSizeLimit);
}

void RetokenizeDocument(TabDocument& document) {
    document.syntaxStale = false;
    document.syntaxColored.clear();
    document.syntaxTokens.clear();
    const std::wstring text = GetEditorRawText(document.editor);
    if (document.untitled && document.language == kLanguageAuto && document.path.empty()) {
        document.sniffedLanguage = SniffLanguage(text);  // a new tab follows what is typed
    }
    if (!SyntaxEnabled(document)) return;
    SyntaxLexer(text, document.syntaxTokens).Run(EffectiveLanguage(document), 0, text.size());
    std::sort(document.syntaxTokens.begin(), document.syntaxTokens.end(),
        [](const SyntaxToken& a, const SyntaxToken& b) { return a.start < b.start; });
}

struct StyleFonts {
    TextDocumentRef& tom;
    ITextFont* fonts[32]{};
    explicit StyleFonts(TextDocumentRef& ref) : tom(ref) {}
    ~StyleFonts() { for (ITextFont* font : fonts) if (font) font->Release(); }
    StyleFonts(const StyleFonts&) = delete;
    StyleFonts& operator=(const StyleFonts&) = delete;
    ITextFont* For(Tok kind) {
        ITextFont*& font = fonts[static_cast<size_t>(kind) & 31];
        if (!font) {
            const SyntaxStyle style = StyleFor(kind);
            font = tom.MakeStyleFont(style.color, style.bold);
        }
        return font;
    }
};

void PaintSyntaxRange(TabDocument& document, TextDocumentRef& tom, StyleFonts& fonts, LONG from, LONG to) {
    tom.ApplyFont(from, to, fonts.For(Tok::Plain));
    const auto& tokens = document.syntaxTokens;
    auto it = std::lower_bound(tokens.begin(), tokens.end(), static_cast<uint32_t>(from),
        [](const SyntaxToken& token, uint32_t value) { return token.start < value; });
    if (it != tokens.begin() && (it - 1)->start + (it - 1)->length > static_cast<uint32_t>(from)) --it;
    for (; it != tokens.end() && static_cast<LONG>(it->start) < to; ++it) {
        const LONG start = std::max(from, static_cast<LONG>(it->start));
        const LONG end = std::min(to, static_cast<LONG>(it->start + it->length));
        if (end <= start) continue;
        tom.ApplyFont(start, end, fonts.For(it->kind));
    }
}

// RichEdit keeps character formatting in one array of runs; every coloured
// token adds runs, and each insert gets slower as the array grows. Keep the
// painted area near the view and return far-away text to plain (which merges
// its runs back); it is repainted if the user scrolls back.
constexpr LONG kMaxPaintedChars = 60000;

// Colours whatever part of the visible text (plus a margin) is not coloured yet.
void SyntaxColorVisible(TabDocument& document) {
    HWND editor = document.editor;
    if (!editor || !IsWindowVisible(editor)) return;
    if (document.syntaxStale) RetokenizeDocument(document);
    if (document.syntaxTokens.empty() && !document.syntaxHadColors) return;
    const LONG length = GetWindowTextLengthW(editor);
    RECT client{};
    GetClientRect(editor, &client);
    POINTL topLeft{0, 0}, bottomRight{client.right, client.bottom};
    LONG from = static_cast<LONG>(SendMessageW(editor, EM_CHARFROMPOS, 0, reinterpret_cast<LPARAM>(&topLeft)));
    LONG to = static_cast<LONG>(SendMessageW(editor, EM_CHARFROMPOS, 0, reinterpret_cast<LPARAM>(&bottomRight)));
    from = std::max(0L, from - 2000);
    to = std::min(length + 1, to + 2000);
    // Gaps between already coloured intervals.
    std::vector<std::pair<LONG, LONG>> gaps;
    LONG cursor = from;
    for (const auto& [start, end] : document.syntaxColored) {
        if (end <= cursor) continue;
        if (start >= to) break;
        if (start > cursor) gaps.emplace_back(cursor, std::min(start, to));
        cursor = std::max(cursor, end);
    }
    if (cursor < to) gaps.emplace_back(cursor, to);
    if (gaps.empty()) return;

    EditorEventMute mute(editor);
    TextDocumentRef tom(editor);
    if (!tom) return;
    long freezeCount = 0;
    tom.document()->Undo(tomSuspend, nullptr);
    tom.document()->Freeze(&freezeCount);
    StyleFonts fonts(tom);
    LONG painted = 0;
    for (const auto& [start, end] : document.syntaxColored) painted += end - start;
    if (painted + (to - from) > kMaxPaintedChars) {
        std::vector<std::pair<LONG, LONG>> kept;
        for (const auto& interval : document.syntaxColored) {
            if (interval.second < from || interval.first > to) tom.ApplyFont(interval.first, interval.second, fonts.For(Tok::Plain));
            else kept.push_back(interval);
        }
        document.syntaxColored.swap(kept);
    }
    for (const auto& [start, end] : gaps) PaintSyntaxRange(document, tom, fonts, start, end);
    tom.document()->Unfreeze(&freezeCount);
    tom.document()->Undo(tomResume, nullptr);
    document.syntaxHadColors = !document.syntaxTokens.empty();

    document.syntaxColored.emplace_back(from, to);
    std::sort(document.syntaxColored.begin(), document.syntaxColored.end());
    std::vector<std::pair<LONG, LONG>> merged;
    for (const auto& interval : document.syntaxColored) {
        if (!merged.empty() && interval.first <= merged.back().second) merged.back().second = std::max(merged.back().second, interval.second);
        else merged.push_back(interval);
    }
    document.syntaxColored.swap(merged);
}

// Text, language or theme changed: re-tokenize and recolour what is visible;
// other tabs recolour when they are shown.
void HighlightDocument(TabDocument& document) {
    document.syntaxStale = true;
    document.syntaxColored.clear();
    if (document.syntaxHadColors && !SyntaxEnabled(document)) {
        // Switched to plain text: drop old colours once.
        EditorEventMute mute(document.editor);
        TextDocumentRef tom(document.editor);
        if (tom) {
            tom.document()->Undo(tomSuspend, nullptr);
            tom.SetStyle(0, GetWindowTextLengthW(document.editor) + 1, StyleFor(Tok::Plain).color, false);
            tom.document()->Undo(tomResume, nullptr);
        }
        document.syntaxHadColors = false;
    }
    SyntaxColorVisible(document);
}

void HighlightDocument(const TabDocument& document) {
    HighlightDocument(const_cast<TabDocument&>(document));
}

// Called after the editor scrolled or resized.
void SyntaxOnViewChanged(HWND editor) {
    for (TabDocument& document : g_app.documents) {
        if (document.editor == editor) {
            SyntaxColorVisible(document);
            return;
        }
    }
}

COLORREF CurrentLineHighlightColor() {
    const ThemePalette& palette = CurrentPalette();
    // Keep the current line close to the editor surface so it is visible
    // without competing with syntax colours or selected text.
    const int amount = g_app.darkTheme ? 12 : 8;
    const auto blend = [amount](BYTE a, BYTE b) -> BYTE {
        return static_cast<BYTE>(std::clamp(static_cast<int>(a) +
            (static_cast<int>(b) - static_cast<int>(a)) * amount / 100, 0, 255));
    };
    return RGB(blend(GetRValue(palette.editorBackground), GetRValue(palette.accent)),
        blend(GetGValue(palette.editorBackground), GetGValue(palette.accent)),
        blend(GetBValue(palette.editorBackground), GetBValue(palette.accent)));
}

void ApplyCurrentLineHighlight(TabDocument& document, bool force) {
    HWND editor = document.editor;
    if (!editor || !IsWindow(editor) || !IsWindowVisible(editor)) return;

    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const LONG length = static_cast<LONG>(GetWindowTextLengthW(editor));
    const LONG caret = std::clamp(selection.cpMin, 0L, length);
    const int lineIndex = static_cast<int>(SendMessageW(editor, EM_EXLINEFROMCHAR, 0, caret));
    if (lineIndex < 0) return;
    const int line = lineIndex + 1;
    if (!force && document.highlightedLine == line) return;

    const auto lineRange = [editor, length](int zeroBasedLine) -> std::pair<LONG, LONG> {
        if (zeroBasedLine < 0) return {0, 0};
        LONG start = static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX,
            static_cast<WPARAM>(zeroBasedLine), 0));
        if (start < 0) return {0, 0};
        LONG end = static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX,
            static_cast<WPARAM>(zeroBasedLine + 1), 0));
        if (end < 0) end = length;
        return {std::min(start, length), std::max(std::min(end, length), start)};
    };

    EditorEventMute mute(editor);
    TextDocumentRef tom(editor);
    if (!tom) return;
    long freezeCount = 0;
    tom.document()->Undo(tomSuspend, nullptr);
    tom.document()->Freeze(&freezeCount);
    if (document.highlightedLine > 0) {
        const auto [oldStart, oldEnd] = lineRange(document.highlightedLine - 1);
        if (oldEnd > oldStart) tom.SetBackColor(oldStart, oldEnd, EditorBackground());
    }
    const auto [newStart, newEnd] = lineRange(lineIndex);
    if (newEnd > newStart) tom.SetBackColor(newStart, newEnd, CurrentLineHighlightColor());
    tom.document()->Unfreeze(&freezeCount);
    tom.document()->Undo(tomResume, nullptr);
    document.highlightedLine = line;
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    InvalidateRect(editor, nullptr, FALSE);
}

bool IsOpeningBracket(wchar_t character) {
    return character == L'(' || character == L'[' || character == L'{';
}

bool IsClosingBracket(wchar_t character) {
    return character == L')' || character == L']' || character == L'}';
}

wchar_t MatchingBracket(wchar_t character) {
    switch (character) {
    case L'(': return L')';
    case L'[': return L']';
    case L'{': return L'}';
    case L')': return L'(';
    case L']': return L'[';
    case L'}': return L'{';
    default: return 0;
    }
}

LONG FindMatchingBracket(std::wstring_view text, LONG position) {
    if (position < 0 || position >= static_cast<LONG>(text.size())) return -1;
    const wchar_t anchor = text[static_cast<size_t>(position)];
    if (!IsOpeningBracket(anchor) && !IsClosingBracket(anchor)) return -1;
    const wchar_t match = MatchingBracket(anchor);
    const int direction = IsOpeningBracket(anchor) ? 1 : -1;
    int depth = 1;
    bool inString = false;
    wchar_t quote = 0;
    for (LONG index = position + direction;
         index >= 0 && index < static_cast<LONG>(text.size()); index += direction) {
        const wchar_t character = text[static_cast<size_t>(index)];
        const bool escaped = index > 0 && text[static_cast<size_t>(index - 1)] == L'\\' &&
            (index < 2 || text[static_cast<size_t>(index - 2)] != L'\\');
        if (character == L'"' || character == L'\'') {
            if (!escaped && (!inString || character == quote)) {
                inString = !inString;
                quote = inString ? character : 0;
            }
            continue;
        }
        if (inString) continue;
        if (character == anchor) {
            ++depth;
        } else if (character == match) {
            if (--depth == 0) return index;
        }
    }
    return -1;
}

COLORREF BracketMatchHighlightColor() {
    const ThemePalette& palette = CurrentPalette();
    const int amount = g_app.darkTheme ? 34 : 24;
    const auto blend = [amount](BYTE a, BYTE b) -> BYTE {
        return static_cast<BYTE>(std::clamp(static_cast<int>(a) +
            (static_cast<int>(b) - static_cast<int>(a)) * amount / 100, 0, 255));
    };
    return RGB(blend(GetRValue(palette.editorBackground), GetRValue(palette.accent)),
        blend(GetGValue(palette.editorBackground), GetGValue(palette.accent)),
        blend(GetBValue(palette.editorBackground), GetBValue(palette.accent)));
}

void ApplyBracketMatchHighlight(TabDocument& document, bool force) {
    HWND editor = document.editor;
    if (!editor || !IsWindow(editor) || !IsWindowVisible(editor)) return;
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const LONG length = static_cast<LONG>(GetWindowTextLengthW(editor));
    const LONG caret = std::clamp(selection.cpMin, 0L, length);
    const std::wstring text = GetEditorRawText(editor);
    LONG anchor = -1;
    if (caret < static_cast<LONG>(text.size()) &&
        (IsOpeningBracket(text[static_cast<size_t>(caret)]) || IsClosingBracket(text[static_cast<size_t>(caret)]))) {
        anchor = caret;
    } else if (caret > 0 &&
        (IsOpeningBracket(text[static_cast<size_t>(caret - 1)]) || IsClosingBracket(text[static_cast<size_t>(caret - 1)]))) {
        anchor = caret - 1;
    }
    const LONG match = anchor >= 0 ? FindMatchingBracket(text, anchor) : -1;
    if (!force && anchor == document.bracketMatchStart && match == document.bracketMatchEnd) return;

    EditorEventMute mute(editor);
    TextDocumentRef tom(editor);
    if (!tom) return;
    long freezeCount = 0;
    tom.document()->Undo(tomSuspend, nullptr);
    tom.document()->Freeze(&freezeCount);
    if (document.bracketMatchStart >= 0 && document.bracketMatchEnd > document.bracketMatchStart) {
        tom.SetBackColor(document.bracketMatchStart, document.bracketMatchStart + 1, EditorBackground());
        tom.SetBackColor(document.bracketMatchEnd, document.bracketMatchEnd + 1, EditorBackground());
    }
    document.bracketMatchStart = anchor;
    document.bracketMatchEnd = match;
    tom.document()->Unfreeze(&freezeCount);
    tom.document()->Undo(tomResume, nullptr);
    // Restoring the line background after removing the old pair keeps the two
    // highlights composable when a bracket sits on the active line.
    ApplyCurrentLineHighlight(document, true);
    if (anchor >= 0 && match >= 0) {
        TextDocumentRef bracketTom(editor);
        if (bracketTom) {
            long bracketFreeze = 0;
            bracketTom.document()->Undo(tomSuspend, nullptr);
            bracketTom.document()->Freeze(&bracketFreeze);
            bracketTom.SetBackColor(anchor, anchor + 1, BracketMatchHighlightColor());
            bracketTom.SetBackColor(match, match + 1, BracketMatchHighlightColor());
            bracketTom.document()->Unfreeze(&bracketFreeze);
            bracketTom.document()->Undo(tomResume, nullptr);
        }
    }
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    InvalidateRect(editor, nullptr, FALSE);
}

// Re-colouring after typing pauses.
constexpr UINT kTimerHighlight = 4102;
constexpr UINT kHighlightDelayMs = 250;

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

bool HasBookmark(const TabDocument& document, int line) {
    return std::binary_search(document.bookmarks.begin(), document.bookmarks.end(), line);
}

void NormalizeBookmarks(TabDocument& document) {
    const int lineCount = document.editor
        ? static_cast<int>(SendMessageW(document.editor, EM_GETLINECOUNT, 0, 0)) : 1;
    document.bookmarks.erase(std::remove_if(document.bookmarks.begin(), document.bookmarks.end(),
        [lineCount](int line) { return line < 1 || line > lineCount; }), document.bookmarks.end());
    std::sort(document.bookmarks.begin(), document.bookmarks.end());
    document.bookmarks.erase(std::unique(document.bookmarks.begin(), document.bookmarks.end()), document.bookmarks.end());
}

void ToggleBookmarkAtLine(int index, int line) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size()) || line < 1) return;
    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    const auto it = std::lower_bound(document.bookmarks.begin(), document.bookmarks.end(), line);
    if (it != document.bookmarks.end() && *it == line) document.bookmarks.erase(it);
    else document.bookmarks.insert(it, line);
    g_app.stateDirty = true;
    RefreshGutter();
    UpdateStatusBar();
}

void ToggleBookmarkAtCaret() {
    if (g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) return;
    TabDocument& document = g_app.documents[static_cast<size_t>(g_app.activeIndex)];
    CHARRANGE selection{};
    SendMessageW(document.editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const long line = static_cast<long>(SendMessageW(document.editor, EM_EXLINEFROMCHAR, 0, selection.cpMin)) + 1;
    ToggleBookmarkAtLine(g_app.activeIndex, static_cast<int>(line));
}

void GotoBookmark(int direction) {
    if (g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) return;
    TabDocument& document = g_app.documents[static_cast<size_t>(g_app.activeIndex)];
    NormalizeBookmarks(document);
    if (document.bookmarks.empty()) {
        ShowNotice(L"当前文档没有书签");
        return;
    }
    CHARRANGE selection{};
    SendMessageW(document.editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const int current = static_cast<int>(SendMessageW(document.editor, EM_EXLINEFROMCHAR, 0, selection.cpMin)) + 1;
    int target = direction > 0 ? document.bookmarks.front() : document.bookmarks.back();
    if (direction > 0) {
        for (int line : document.bookmarks) if (line > current) { target = line; break; }
    } else {
        for (auto it = document.bookmarks.rbegin(); it != document.bookmarks.rend(); ++it) {
            if (*it < current) { target = *it; break; }
        }
    }
    const LONG position = static_cast<LONG>(SendMessageW(document.editor, EM_LINEINDEX, target - 1, 0));
    if (position >= 0) {
        SendMessageW(document.editor, EM_SETSEL, position, position);
        SendMessageW(document.editor, EM_SCROLLCARET, 0, 0);
        SetFocus(document.editor);
        RefreshGutter();
        UpdateStatusBar();
    }
}

void ClearBookmarks() {
    if (g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) return;
    TabDocument& document = g_app.documents[static_cast<size_t>(g_app.activeIndex)];
    if (document.bookmarks.empty()) return;
    document.bookmarks.clear();
    g_app.stateDirty = true;
    RefreshGutter();
    ShowNotice(L"已清除当前文档书签");
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
            TabDocument& activeDocument = g_app.documents[static_cast<size_t>(g_app.activeIndex)];
            DetectFoldRegions(activeDocument);
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
                    const auto fold = std::find_if(activeDocument.folds.begin(), activeDocument.folds.end(),
                        [paragraph](const FoldRegion& region) { return region.line + 1 == paragraph; });
                    if (fold != activeDocument.folds.end()) {
                        const int markerX = Scale(5);
                        const int markerY = y + metrics.tmHeight / 2;
                        HPEN foldPen = CreatePen(PS_SOLID, 1, CurrentPalette().gutterText);
                        HBRUSH foldBrush = CreateSolidBrush(GutterBackground());
                        HGDIOBJ oldPen = SelectObject(dc, foldPen);
                        HGDIOBJ oldBrush = SelectObject(dc, foldBrush);
                        POINT arrow[3]{};
                        if (fold->collapsed) {
                            arrow[0] = {markerX - Scale(2), markerY - Scale(4)};
                            arrow[1] = {markerX - Scale(2), markerY + Scale(4)};
                            arrow[2] = {markerX + Scale(3), markerY};
                        } else {
                            arrow[0] = {markerX - Scale(4), markerY - Scale(2)};
                            arrow[1] = {markerX + Scale(4), markerY - Scale(2)};
                            arrow[2] = {markerX, markerY + Scale(3)};
                        }
                        Polygon(dc, arrow, 3);
                        SelectObject(dc, oldBrush);
                        SelectObject(dc, oldPen);
                        DeleteObject(foldBrush);
                        DeleteObject(foldPen);
                    }
                    if (HasBookmark(activeDocument, static_cast<int>(paragraph))) {
                        const int markerX = Scale(17);
                        const int markerY = y + metrics.tmHeight / 2;
                        HBRUSH markerBrush = CreateSolidBrush(CurrentPalette().accent);
                        HPEN markerPen = CreatePen(PS_SOLID, 1, CurrentPalette().accent);
                        HGDIOBJ oldBrush = SelectObject(dc, markerBrush);
                        HGDIOBJ oldPen = SelectObject(dc, markerPen);
                        POINT marker[3]{{markerX, markerY - Scale(5)}, {markerX + Scale(8), markerY}, {markerX, markerY + Scale(5)}};
                        Polygon(dc, marker, 3);
                        SelectObject(dc, oldPen);
                        SelectObject(dc, oldBrush);
                        DeleteObject(markerPen);
                        DeleteObject(markerBrush);
                    }
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
    case WM_LBUTTONUP: {
        if (g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) return 0;
        HWND editor = g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor;
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const bool foldClick = point.x < Scale(14);
        MapWindowPoints(window, editor, &point, 1);
        const LRESULT character = SendMessageW(editor, EM_CHARFROMPOS, 0, reinterpret_cast<LPARAM>(&point));
        const LONG position = static_cast<LONG>(character);
        const int line = position >= 0
            ? static_cast<int>(SendMessageW(editor, EM_EXLINEFROMCHAR, 0, position)) + 1 : 1;
        if (foldClick) {
            TabDocument& document = g_app.documents[static_cast<size_t>(g_app.activeIndex)];
            DetectFoldRegions(document);
            auto it = std::find_if(document.folds.begin(), document.folds.end(), [line](const FoldRegion& region) {
                return region.line + 1 == line;
            });
            if (it != document.folds.end()) {
                it->collapsed = !it->collapsed;
                ApplyFoldVisibility(document);
                g_app.stateDirty = true;
                SetFocus(editor);
                return 0;
            }
        }
        ToggleBookmarkAtLine(g_app.activeIndex, line);
        SetFocus(editor);
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

// Language for a file extension or a code-fence / language name ("js", "py").
int LanguageFromName(const std::wstring& rawName) {
    static const std::unordered_map<std::wstring, int> names = [] {
        std::unordered_map<std::wstring, int> m;
        const auto add = [&](int language, const wchar_t* list) {
            std::wistringstream stream(list);
            for (std::wstring name; stream >> name;) m[name] = language;
        };
        add(kLanguageHtml, L"html htm xhtml vue svelte astro");
        add(kLanguageXml, L"xml svg xaml csproj vcxproj vbproj props targets resx xsd xsl xslt plist nuspec config manifest");
        add(kLanguageCss, L"css scss sass less");
        add(kLanguageJavaScript, L"js mjs cjs jsx javascript node");
        add(kLanguageTypeScript, L"ts tsx mts cts typescript");
        add(kLanguageCpp, L"c cc cpp cxx c++ h hpp hh hxx inl ino ipp");
        add(kLanguagePython, L"py pyw pyi python python3");
        add(kLanguageJson, L"json jsonc json5 webmanifest geojson ipynb");
        add(kLanguageMarkdown, L"md markdown mdx mkd");
        add(kLanguageSql, L"sql mysql pgsql psql");
        add(kLanguagePowerShell, L"ps1 psm1 psd1 powershell pwsh ps");
        add(kLanguageYaml, L"yaml yml");
        add(kLanguageConfig, L"ini cfg conf toml properties env dotenv editorconfig gitignore gitattributes dockerignore npmrc reg inf");
        add(kLanguageShell, L"sh bash zsh ksh fish shell shellscript console makefile mk");
        add(kLanguageBatch, L"bat cmd batch");
        add(kLanguageJava, L"java kt kts kotlin gradle groovy scala");
        add(kLanguageCSharp, L"cs csx csharp c#");
        add(kLanguageGo, L"go golang");
        add(kLanguageRust, L"rs rust");
        add(kLanguagePhp, L"php phtml php3 php4 php5");
        add(kLanguageDockerfile, L"dockerfile containerfile docker");
        add(kLanguageCsv, L"csv tsv");
        add(kLanguageLog, L"log out");
        add(kLanguageDiff, L"diff patch");
        return m;
    }();
    const auto it = names.find(LowerInvariantSyntax(rawName));
    return it == names.end() ? kLanguagePlain : it->second;
}

int LanguageFromPath(const std::wstring& path) {
    if (path.empty()) return kLanguagePlain;
    std::wstring name = LowerInvariantSyntax(fs::path(path).filename().wstring());
    // config.yml.example, .env.production.sample, nginx.conf.template, settings.json.bak ...
    for (bool stripped = true; stripped;) {
        stripped = false;
        for (const wchar_t* suffix : {L".example", L".sample", L".template", L".dist", L".tpl", L".bak", L".orig", L".default", L".old", L".tmpl"}) {
            const std::wstring tail = suffix;
            if (name.size() > tail.size() && name.ends_with(tail)) {
                name.resize(name.size() - tail.size());
                stripped = true;
            }
        }
    }
    if (name == L"dockerfile" || name == L"containerfile" || name.starts_with(L"dockerfile.") || name.ends_with(L".dockerfile")) return kLanguageDockerfile;
    if (name == L"makefile" || name == L"gnumakefile" || name == L".bashrc" || name == L".zshrc" || name == L".profile" || name == L".bash_profile") return kLanguageShell;
    if (name == L".env" || name.starts_with(L".env.") || name.ends_with(L".env")) return kLanguageConfig;
    if (name == L".prettierrc" || name == L".eslintrc" || name == L".babelrc" || name == L"composer.lock" || name == L".jshintrc") return kLanguageJson;
    for (const wchar_t* config : {L".gitignore", L".gitattributes", L".dockerignore", L".npmrc", L".editorconfig", L".prettierignore", L".eslintignore", L".yarnrc", L".gitmodules"}) {
        if (name == config) return kLanguageConfig;
    }
    const size_t dot = name.rfind(L'.');
    if (dot == std::wstring::npos || dot + 1 >= name.size()) return kLanguagePlain;
    return LanguageFromName(name.substr(dot + 1));
}

// For files whose name says nothing: shebang lines, XML/HTML prologs, JSON.
int SniffLanguage(const std::wstring& text) {
    size_t start = text.find_first_not_of(L" \t\r\n\xFEFF");
    if (start == std::wstring::npos) return kLanguagePlain;
    const std::wstring head = LowerInvariantSyntax(text.substr(start, 400));
    if (head.starts_with(L"#!")) {
        const std::wstring line = head.substr(0, head.find_first_of(L"\r\n"));
        if (line.find(L"python") != std::wstring::npos) return kLanguagePython;
        if (line.find(L"node") != std::wstring::npos) return kLanguageJavaScript;
        if (line.find(L"pwsh") != std::wstring::npos || line.find(L"powershell") != std::wstring::npos) return kLanguagePowerShell;
        if (line.find(L"php") != std::wstring::npos) return kLanguagePhp;
        return kLanguageShell;
    }
    if (head.starts_with(L"<?xml")) return kLanguageXml;
    if (head.starts_with(L"<!doctype html") || head.starts_with(L"<html")) return kLanguageHtml;
    if (head.starts_with(L"<?php")) return kLanguagePhp;
    if (head.starts_with(L"diff --git") || (head.starts_with(L"--- ") && head.find(L"\n+++ ") != std::wstring::npos)) return kLanguageDiff;
    if ((head[0] == L'{' || head[0] == L'[') && head.find(L"\":") != std::wstring::npos) return kLanguageJson;
    return kLanguagePlain;
}

// Language actually used for highlighting/formatting: the user's choice, or the
// one implied by the file extension when set to "auto".
int EffectiveLanguage(const TabDocument& document) {
    if (document.language > kLanguageAuto && document.language < kLanguageCount) return document.language;
    const int fromPath = LanguageFromPath(document.path);
    return fromPath != kLanguagePlain ? fromPath : document.sniffedLanguage;
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

// Encodes text exactly as given (line breaks untouched) in one of kEncodingNames.
bool EncodeTextBytes(std::wstring_view text, int encoding, std::string& bytes) {
    bytes.clear();
    if (encoding == 2 || encoding == 3) {
        bytes.reserve(text.size() * 2 + 2);
        bytes.append(encoding == 2 ? "\xFF\xFE" : "\xFE\xFF", 2);
        for (wchar_t value : text) {
            const uint16_t unit = static_cast<uint16_t>(value);
            const char low = static_cast<char>(unit & 0xFF);
            const char high = static_cast<char>((unit >> 8) & 0xFF);
            if (encoding == 2) { bytes.push_back(low); bytes.push_back(high); }
            else { bytes.push_back(high); bytes.push_back(low); }
        }
        return true;
    }
    if (encoding == kEncodingAnsi) {
        const int size = WideCharToMultiByte(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, "?", nullptr);
        if (size <= 0) return text.empty();
        bytes.resize(static_cast<size_t>(size));
        WideCharToMultiByte(CP_ACP, 0, text.data(), static_cast<int>(text.size()), bytes.data(), size, "?", nullptr);
        return true;
    }
    const std::string utf8 = WideToUtf8(text);
    // Never report success for a conversion that lost the whole text.
    if (utf8.empty() && !text.empty()) return false;
    if (encoding == 1) bytes.append("\xEF\xBB\xBF", 3);
    bytes += utf8;
    return true;
}

bool WriteTextFile(const fs::path& path, std::wstring_view text, int encodingIndex = -1, int lineEndingIndex = -1) {
    if (encodingIndex < 0) encodingIndex = g_app.encodingIndex;
    if (lineEndingIndex < 0) lineEndingIndex = g_app.lineEndingIndex;
    std::string bytes;
    if (!EncodeTextBytes(NormalizeLineEndings(text, lineEndingIndex), encodingIndex, bytes)) return false;
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
    const int areaHeight = std::max(0, height - contentY - statusHeight);
    // The find-in-files panel takes the bottom of the content area, leaving
    // the editor at least a few lines.
    const int panelHeight = g_app.searchPanel && g_app.searchPanelVisible
        ? std::clamp(g_app.searchPanelHeight, std::min(Scale(160), areaHeight), std::max(0, areaHeight - Scale(120))) : 0;
    const int contentHeight = areaHeight - panelHeight;
    if (g_app.searchPanel) {
        MoveWindow(g_app.searchPanel, 0, contentY + contentHeight, width, panelHeight, TRUE);
        ShowWindow(g_app.searchPanel, panelHeight > 0 ? SW_SHOW : SW_HIDE);
    }
    const int toolboxWidth = g_app.toolboxVisible && g_app.toolboxPanel ? std::min(Scale(g_app.toolboxWidth), std::max(0, width - Scale(320))) : 0;
    if (g_app.toolboxPanel) MoveWindow(g_app.toolboxPanel, width - toolboxWidth, contentY, toolboxWidth, contentHeight, TRUE);
    const int editorWidth = width - toolboxWidth;
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
    SyntaxColorVisible(document);
    ApplyCurrentLineHighlight(document);
    ApplyBracketMatchHighlight(document, true);
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
    output << "LINE_SPACING\t" << g_app.lineSpacingPercent << "\n";
    output << "OPTIONS\t" << (g_app.showLineNumbers ? 1 : 0) << '\t'
           << (g_app.wordWrap ? 1 : 0) << '\t'
           << (g_app.sidebarVisible ? 1 : 0) << '\t'
           << g_app.fontSize << '\t'
           << HexEncode(WideToUtf8(g_app.fontFamily)) << '\t'
           << g_app.languageIndex << '\t'
           << g_app.encodingIndex << '\t'
           << g_app.lineEndingIndex << '\t'
           << (g_app.showWhitespace ? 1 : 0) << '\t'
           << (g_app.showIndentGuides ? 1 : 0) << "\n";
    output << "TOOLBOX\t" << (g_app.toolboxVisible ? 1 : 0) << '\t' << g_app.toolboxWidth << '\t' << g_app.toolboxMask << '\t' << g_app.toolboxTool << "\n";
    output << "PREFERENCES\t" << (g_app.launchAtStartup ? 1 : 0) << '\t'
           << static_cast<int>(g_app.closeBehavior) << '\t'
           << (g_app.previewTextTools ? 1 : 0) << '\t'
           << g_app.toolOutputMode << '\t'
           << static_cast<int>(g_app.windowCloseBehavior) << '\t'
           << (g_app.minimizeToTray ? 1 : 0) << '\t'
           << (g_app.showTrayIcon ? 1 : 0) << "\n";
    SaveShortcutBindings(output);

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
               << document.language << '\t';
        for (size_t bookmark = 0; bookmark < document.bookmarks.size(); ++bookmark) {
            if (bookmark != 0) output << ',';
            output << document.bookmarks[bookmark];
        }
        output << '\t' << (document.pinned ? 1 : 0) << '\n';
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

void ChangeEditorFontSize(int delta) {
    const int next = delta == 0 ? 13 : std::clamp(g_app.fontSize + delta, 10, 32);
    if (next == g_app.fontSize) return;
    g_app.fontSize = next;
    CreateUiFonts();
    ApplyEditorFont();
    g_app.stateDirty = true;
    ShowNotice(L"编辑器字号 " + std::to_wstring(g_app.fontSize) + L" px");
}

bool TryAutoCloseCharacter(HWND editor, wchar_t character) {
    wchar_t closing = 0;
    switch (character) {
    case L'(': closing = L')'; break;
    case L'[': closing = L']'; break;
    case L'{': closing = L'}'; break;
    case L'"': closing = L'"'; break;
    case L'\'': closing = L'\''; break;
    default: return false;
    }
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const std::wstring text = GetEditorRawText(editor);
    if (selection.cpMin == selection.cpMax && selection.cpMin < static_cast<LONG>(text.size()) &&
        text[static_cast<size_t>(selection.cpMin)] == closing && character != L'(' && character != L'[' && character != L'{') {
        SendMessageW(editor, EM_SETSEL, selection.cpMin + 1, selection.cpMin + 1);
        return true;
    }
    std::wstring selected;
    if (selection.cpMax > selection.cpMin) {
        selected = text.substr(static_cast<size_t>(selection.cpMin),
            static_cast<size_t>(selection.cpMax - selection.cpMin));
    }
    const std::wstring replacement = std::wstring(1, character) + selected + std::wstring(1, closing);
    const LONG start = selection.cpMin;
    SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(replacement.c_str()));
    const LONG innerStart = start + 1;
    const LONG innerEnd = innerStart + static_cast<LONG>(selected.size());
    SendMessageW(editor, EM_SETSEL, innerStart, innerEnd);
    return true;
}

std::wstring SmartIndentForEnter(HWND editor) {
    if (!editor) return {};
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    if (selection.cpMin != selection.cpMax) return {};
    const std::wstring text = GetEditorRawText(editor);
    const LONG caret = std::clamp(selection.cpMin, 0L, static_cast<LONG>(text.size()));
    const LRESULT lineNumber = SendMessageW(editor, EM_EXLINEFROMCHAR, 0, caret);
    const LONG lineStart = lineNumber >= 0
        ? static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, lineNumber, 0)) : 0;
    if (lineStart < 0 || lineStart > caret) return {};

    size_t prefixEnd = static_cast<size_t>(caret - lineStart);
    prefixEnd = std::min(prefixEnd, text.size() - static_cast<size_t>(lineStart));
    std::wstring linePrefix = text.substr(static_cast<size_t>(lineStart), prefixEnd);
    size_t firstCode = 0;
    while (firstCode < linePrefix.size() && (linePrefix[firstCode] == L' ' || linePrefix[firstCode] == L'\t')) ++firstCode;
    std::wstring indent = linePrefix.substr(0, firstCode);

    size_t lastCode = linePrefix.size();
    while (lastCode > firstCode && iswspace(linePrefix[lastCode - 1])) --lastCode;
    const wchar_t previous = lastCode > firstCode ? linePrefix[lastCode - 1] : 0;
    const wchar_t next = caret < static_cast<LONG>(text.size()) ? text[static_cast<size_t>(caret)] : 0;
    const bool opensBlock = previous == L'{' || previous == L'[' || previous == L'(';
    const bool closesBlock = next == L'}' || next == L']' || next == L')';
    if (closesBlock && lastCode == firstCode && indent.size() >= 4) indent.resize(indent.size() - 4);
    if (opensBlock) indent += L"    ";
    return L"\r" + indent;
}

bool HandleSmartIndentEnter(HWND editor) {
    if (!editor) return false;
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    if (selection.cpMin != selection.cpMax) return false;
    const std::wstring insertion = SmartIndentForEnter(editor);
    if (insertion.empty()) return false;
    SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(insertion.c_str()));
    return true;
}

void DrawEditorGuides(HWND editor) {
    if (!editor || (!g_app.showWhitespace && !g_app.showIndentGuides)) return;
    RECT client{};
    GetClientRect(editor, &client);
    HDC dc = GetDC(editor);
    if (!dc) return;
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(editor, WM_GETFONT, 0, 0));
    HGDIOBJ previousFont = font ? SelectObject(dc, font) : nullptr;
    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    const int lineHeight = std::max(1, static_cast<int>(metrics.tmHeight));
    const int charWidth = std::max(1, static_cast<int>(metrics.tmAveCharWidth));
    const COLORREF guideColor = Blend(EditorBackground(), CurrentPalette().accent, g_app.darkTheme ? 28 : 20);
    HPEN guidePen = CreatePen(PS_DOT, 1, guideColor);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, guidePen));
    SetBkMode(dc, TRANSPARENT);
    const int firstLine = std::max(0, static_cast<int>(SendMessageW(editor, EM_GETFIRSTVISIBLELINE, 0, 0)));
    const int lineCount = static_cast<int>(SendMessageW(editor, EM_GETLINECOUNT, 0, 0));
    const int visibleLines = client.bottom / lineHeight + 3;
    for (int line = firstLine; line < lineCount && line < firstLine + visibleLines; ++line) {
        const LONG start = static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, line, 0));
        if (start < 0) continue;
        POINT origin{};
        SendMessageW(editor, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&origin), start);
        if (origin.y + lineHeight < 0 || origin.y > client.bottom) continue;
        wchar_t buffer[4096]{};
        *reinterpret_cast<WORD*>(buffer) = static_cast<WORD>(std::size(buffer) - 1);
        const LRESULT copied = SendMessageW(editor, EM_GETLINE, line, reinterpret_cast<LPARAM>(buffer));
        const int length = std::max(0, static_cast<int>(copied));
        int column = 0;
        int leading = 0;
        for (int index = 0; index < length; ++index) {
            const wchar_t character = buffer[index];
            if (character == L'\t') {
                column = (column / 4 + 1) * 4;
                ++leading;
            } else if (character == L' ') {
                ++column;
                ++leading;
            } else {
                break;
            }
        }
        if (g_app.showIndentGuides && leading > 0) {
            for (int guide = 4; guide <= column; guide += 4) {
                const int x = origin.x + guide * charWidth;
                MoveToEx(dc, x, origin.y, nullptr);
                LineTo(dc, x, origin.y + lineHeight);
            }
        }
        if (g_app.showWhitespace) {
            int visualColumn = 0;
            for (int index = 0; index < length; ++index) {
                const wchar_t character = buffer[index];
                POINT position{};
                SendMessageW(editor, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&position), start + index);
                if (character == L' ') {
                    const int x = position.x + charWidth / 2;
                    const int y = origin.y + lineHeight / 2;
                    Ellipse(dc, x - 1, y - 1, x + 2, y + 2);
                    ++visualColumn;
                } else if (character == L'\t') {
                    const int nextColumn = (visualColumn / 4 + 1) * 4;
                    const int x = position.x + 2;
                    const int y = origin.y + lineHeight / 2;
                    MoveToEx(dc, x, y, nullptr);
                    LineTo(dc, origin.x + nextColumn * charWidth - 3, y);
                    ++visualColumn;
                    visualColumn = nextColumn;
                } else {
                    ++visualColumn;
                }
            }
        }
    }
    SelectObject(dc, oldPen);
    DeleteObject(guidePen);
    if (previousFont) SelectObject(dc, previousFont);
    ReleaseDC(editor, dc);
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
        (GetKeyState(VK_MENU) & 0x8000) != 0 && wParam == VK_DOWN) {
        AddNextLineCaret();
        return 0;
    }
    if (message == WM_KEYDOWN && (wParam == VK_BACK || wParam == VK_DELETE) &&
        ApplyMultiCaretBackspace(editor, wParam == VK_DELETE)) {
        return 0;
    }

    if (message == WM_CHAR && (GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
        (GetKeyState(VK_MENU) & 0x8000) == 0 &&
        (wParam >= 0x20 || wParam == L'\r' || wParam == L'\t')) {
        const std::wstring typed(1, static_cast<wchar_t>(wParam));
        if (ApplyMultiCaretReplacement(editor, typed)) {
            RecordMacroText(typed);
            return 0;
        }
        RecordMacroText(typed);
    }

    if (message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
        (GetKeyState(VK_MENU) & 0x8000) == 0) {
        if (wParam == VK_F2) {
            SendMessageW(g_app.window, WM_COMMAND, kIdViewToggleBookmark, 0);
            return 0;
        }
        if (wParam == VK_OEM_PLUS || wParam == VK_ADD) {
            ChangeEditorFontSize(1);
            return 0;
        }
        if (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT) {
            ChangeEditorFontSize(-1);
            return 0;
        }
        if (wParam == '0') {
            ChangeEditorFontSize(13 - g_app.fontSize);
            return 0;
        }
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

    if (message == WM_KEYDOWN && wParam == VK_F4) {
        if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
            SendMessageW(g_app.window, WM_COMMAND, kIdViewPreviousBookmark, 0);
            return 0;
        }
        SendMessageW(g_app.window, WM_COMMAND, kIdViewNextBookmark, 0);
        return 0;
    }

    if (message == WM_MOUSEWHEEL && (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta != 0) {
            ChangeEditorFontSize(delta > 0 ? 1 : -1);
        }
        return 0;
    }

    if (message == WM_CHAR && (GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
        (GetKeyState(VK_MENU) & 0x8000) == 0 && TryAutoCloseCharacter(editor, static_cast<wchar_t>(wParam))) {
        return 0;
    }

    if (message == WM_CHAR && wParam == L'\r' &&
        (GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
        (GetKeyState(VK_MENU) & 0x8000) == 0 && HandleSmartIndentEnter(editor)) {
        return 0;
    }

    const LRESULT result = oldProc != nullptr
        ? CallWindowProcW(oldProc, editor, message, wParam, lParam)
        : DefWindowProcW(editor, message, wParam, lParam);
    if (message == WM_NCDESTROY) {
        g_editorProcs.erase(editor);
    } else if (message == WM_MOUSEWHEEL || message == WM_KEYDOWN || message == WM_VSCROLL) {
        // EN_VSCROLL is not sent for wheel or keyboard scrolling.
        RefreshGutter();
        SyntaxOnViewChanged(editor);
    } else if (message == WM_SIZE || message == WM_SHOWWINDOW) {
        UpdateHorizontalScrollbar(editor);
        if (message == WM_SIZE) SyntaxOnViewChanged(editor);
    }
    if (message == WM_PAINT) {
        DrawEditorGuides(editor);
        DrawMultiCaretOverlay(editor);
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

// RichEdit stores multiple line spacing in twentieths of a line.  Formatting
// the complete document keeps the setting consistent across every paragraph,
// while the event guard and modify flag preservation ensure this view option
// never turns a clean document into an unsaved one.
void ApplyEditorLineSpacing(HWND editor) {
    if (!editor) return;
    EditorEventMute mute(editor);
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const LONG firstVisibleLine = static_cast<LONG>(SendMessageW(editor, EM_GETFIRSTVISIBLELINE, 0, 0));
    const LRESULT wasModified = SendMessageW(editor, EM_GETMODIFY, 0, 0);

    PARAFORMAT2 format{};
    format.cbSize = sizeof(format);
    format.dwMask = PFM_LINESPACING;
    format.bLineSpacingRule = tomLineSpaceMultiple;
    format.dyLineSpacing = std::clamp(
        MulDiv(g_app.lineSpacingPercent, 20, 100), 10, 60);

    SendMessageW(editor, EM_SETSEL, 0, -1);
    SendMessageW(editor, EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&format));
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    SendMessageW(editor, EM_SETMODIFY, wasModified ? TRUE : FALSE, 0);

    const LONG nowVisibleLine = static_cast<LONG>(SendMessageW(editor, EM_GETFIRSTVISIBLELINE, 0, 0));
    if (firstVisibleLine != nowVisibleLine) {
        SendMessageW(editor, EM_LINESCROLL, 0, firstVisibleLine - nowVisibleLine);
    }
}

void ApplyEditorLineSpacingAll() {
    for (const TabDocument& document : g_app.documents) {
        ApplyEditorLineSpacing(document.editor);
    }
    RefreshGutter();
    g_app.stateDirty = true;
}

void SetLineSpacingPercent(int percent) {
    g_app.lineSpacingPercent = std::clamp(percent, 50, 300);
    ApplyEditorLineSpacingAll();
    SetMenuState();
}

void OpenCustomLineSpacingDialog() {
    std::wostringstream initial;
    initial << std::fixed << std::setprecision(2)
            << (static_cast<double>(g_app.lineSpacingPercent) / 100.0);
    std::wstring value = initial.str();
    std::vector<std::wstring> values{value};
    if (!PromptForText(L"自定义行距", {L"行距倍数（0.50 - 3.00，例如 1.25）："}, values, nullptr, nullptr)) {
        return;
    }
    value = values.front();

    size_t consumed = 0;
    double multiplier = 0.0;
    try {
        multiplier = std::stod(value, &consumed);
    } catch (...) {
        multiplier = 0.0;
    }
    while (consumed < value.size() && iswspace(value[consumed])) ++consumed;
    if (consumed != value.size() || !std::isfinite(multiplier) || multiplier < 0.5 || multiplier > 3.0) {
        MessageBoxW(g_app.window, L"请输入 0.50 到 3.00 之间的行距倍数。", kWindowTitle, MB_ICONWARNING);
        return;
    }
    SetLineSpacingPercent(static_cast<int>(std::lround(multiplier * 100.0)));
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
    SendMessageW(document.editor, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(Scale(6), 0));
    // Keep the selected Latin font, but allow RichEdit to choose a proper
    // fallback face for CJK text instead of drawing replacement boxes.
    const LRESULT languageOptions = SendMessageW(document.editor, EM_GETLANGOPTIONS, 0, 0);
    SendMessageW(document.editor, EM_SETLANGOPTIONS, 0,
        (languageOptions | IMF_AUTOFONT | IMF_DUALFONT) &
        ~static_cast<LRESULT>(IMF_AUTOFONTSIZEADJUST));
    SetWindowTextW(document.editor, text.c_str());
    // RichEdit resets the character format when WM_SETTEXT/SetWindowText is
    // applied.  Set the theme after loading the document so dark-theme text
    // does not fall back to the RichEdit default black foreground.
    ApplyEditorTheme(document.editor);
    ApplyEditorLineSpacing(document.editor);
    ApplyWordWrap(document.editor);
    document.largeFileMode = text.size() >= (8ull << 20);
    if (document.largeFileMode) {
        // RichEdit's undo journal can otherwise duplicate a large buffer for
        // every edit. The document remains fully editable and is saved by the
        // normal path; only the memory-heavy journal is disabled.
        SendMessageW(document.editor, EM_SETUNDOLIMIT, 0, 0);
    }
    DetectFoldRegions(document);
    if (LanguageFromPath(document.path) == kLanguagePlain) document.sniffedLanguage = SniffLanguage(text);
    // Colours are painted when the tab is shown (only the visible part).
    SendMessageW(document.editor, EM_EMPTYUNDOBUFFER, 0, 0);  // loading is not an undoable edit
    SendMessageW(document.editor, EM_SETMODIFY, modified ? TRUE : FALSE, 0);
    // Enable change/scroll notifications only after the initial text is in,
    // so loading a file does not mark it as modified.
    document.textHash = HashText(GetEditorRawText(document.editor));
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

bool SaveAllDocuments() {
    bool saved = true;
    for (int index = 0; index < static_cast<int>(g_app.documents.size()); ++index) {
        if (g_app.documents[static_cast<size_t>(index)].modified && !SaveDocument(index, false)) {
            saved = false;
            break;
        }
    }
    if (saved) ShowNotice(L"已保存全部标签");
    return saved;
}

bool OpenLogViewer(const std::wstring& path);

bool OpenFileInNewTab(const fs::path& path) {
    for (size_t index = 0; index < g_app.documents.size(); ++index) {
        const TabDocument& existing = g_app.documents[index];
        if (!existing.untitled && SamePath(existing.path, path)) {
            SelectTab(static_cast<int>(index));
            return true;
        }
    }

    std::error_code sizeError;
    const uintmax_t bytes = fs::file_size(path, sizeError);
    if (!sizeError && bytes > (100ull << 20)) {
        const std::wstring question = L"“" + path.filename().wstring() + L"”有 " + std::to_wstring(bytes >> 20) +
            L" MB。\n\n是：用日志查看器打开（只读，不整体载入内存，支持筛选和实时跟踪）\n否：进入大文件编辑模式（关闭内存密集型撤销，仍可编辑和保存）";
        const int choice = MessageBoxW(g_app.window, question.c_str(), kWindowTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
        if (choice == IDCANCEL) return false;
        if (choice == IDYES) return OpenLogViewer(path.wstring());
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

void RememberClosedTab(TabDocument& document) {
    CaptureEditorState(document);
    ClosedTab closed;
    closed.path = document.path;
    closed.title = document.title;
    closed.text = GetWindowTextValue(document.editor);
    closed.untitled = document.untitled;
    closed.modified = document.modified;
    closed.pinned = document.pinned;
    closed.caret = document.caret;
    closed.anchor = document.anchor;
    closed.firstVisibleLine = document.firstVisibleLine;
    closed.encoding = document.encoding;
    closed.lineEnding = document.lineEnding;
    closed.language = document.language;
    closed.fileTimestamp = document.fileTimestamp;
    closed.bookmarks = document.bookmarks;
    g_app.closedTabs.insert(g_app.closedTabs.begin(), std::move(closed));
    constexpr size_t kClosedTabHistoryLimit = 30;
    if (g_app.closedTabs.size() > kClosedTabHistoryLimit) {
        g_app.closedTabs.resize(kClosedTabHistoryLimit);
    }
}

void ReopenClosedDocument() {
    if (g_app.closedTabs.empty()) return;
    ClosedTab closed = std::move(g_app.closedTabs.front());
    g_app.closedTabs.erase(g_app.closedTabs.begin());
    DocumentFormat format;
    format.encoding = closed.encoding;
    format.lineEnding = closed.lineEnding;
    format.language = closed.language;
    format.fileTimestamp = closed.fileTimestamp;
    if (!closed.modified && !closed.untitled && !closed.path.empty()) {
        std::wstring currentText;
        int encoding = format.encoding;
        int lineEnding = format.lineEnding;
        if (ReadTextFile(closed.path, currentText, &encoding, &lineEnding)) {
            closed.text = std::move(currentText);
            format.encoding = encoding;
            format.lineEnding = lineEnding;
            format.fileTimestamp = FileTimestamp(closed.path);
        }
    }
    const int index = CreateDocument(closed.path, closed.title, std::move(closed.text), closed.untitled,
        closed.modified, closed.caret, closed.anchor, closed.firstVisibleLine, nullptr, format);
    if (index < 0) return;
    g_app.documents[static_cast<size_t>(index)].bookmarks = std::move(closed.bookmarks);
    g_app.documents[static_cast<size_t>(index)].pinned = closed.pinned;
    NormalizeBookmarks(g_app.documents[static_cast<size_t>(index)]);
    SelectTab(index);
    PersistAll(false);
}

bool CloseDocument(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) {
        return false;
    }

    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    if (!PrepareDocumentClose(document)) return false;

    RememberClosedTab(document);

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

bool CloseAllDocuments() {
    while (g_app.documents.size() > 1) {
        if (!CloseDocument(static_cast<int>(g_app.documents.size()) - 1)) return false;
    }
    if (!g_app.documents.empty() && !CloseDocument(0)) return false;
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
    EnableMenuItem(menu, kIdFileSaveAll, MF_BYCOMMAND | (hasActive ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, kIdFileCloseAll, MF_BYCOMMAND | (hasActive ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, kIdFileReopenClosed, MF_BYCOMMAND |
        (g_app.closedTabs.empty() ? MF_GRAYED : MF_ENABLED));
    if (g_app.themeMenu) {
        CheckMenuRadioItem(g_app.themeMenu, kIdThemeDark, kIdThemeBlue,
            kIdThemeDark + static_cast<int>(g_app.theme), MF_BYCOMMAND);
    }
    CheckMenuItem(menu, kIdToolbarLineNumbers, MF_BYCOMMAND |
        (g_app.showLineNumbers ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, kIdToolbarWrap, MF_BYCOMMAND |
        (g_app.wordWrap ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, kIdViewWhitespace, MF_BYCOMMAND |
        (g_app.showWhitespace ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, kIdViewIndentGuides, MF_BYCOMMAND |
        (g_app.showIndentGuides ? MF_CHECKED : MF_UNCHECKED));
    if (g_app.lineSpacingMenu) {
        for (int id = kIdLineSpacingSingle; id <= kIdLineSpacingCustom; ++id) {
            CheckMenuItem(g_app.lineSpacingMenu, id, MF_BYCOMMAND | MF_UNCHECKED);
        }
        int selected = 0;
        switch (g_app.lineSpacingPercent) {
        case 100: selected = kIdLineSpacingSingle; break;
        case 115: selected = kIdLineSpacing115; break;
        case 150: selected = kIdLineSpacing150; break;
        case 200: selected = kIdLineSpacing200; break;
        default: selected = 0; break;
        }
        if (selected != 0) CheckMenuItem(g_app.lineSpacingMenu, selected, MF_BYCOMMAND | MF_CHECKED);
    }
    CheckDocumentMenuItems(menu);
    CheckMenuItem(menu, kIdTogglePreview, MF_BYCOMMAND | (g_app.previewTextTools ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuRadioItem(menu, kIdOutputReplace, kIdOutputInsertBelow, kIdOutputReplace + g_app.toolOutputMode, MF_BYCOMMAND);
    CheckMenuItem(menu, kIdToggleSearchPanel, MF_BYCOMMAND | (g_app.searchPanelVisible ? MF_CHECKED : MF_UNCHECKED));
}

HMENU CreateApplicationMenu() {
    HMENU menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU edit = CreatePopupMenu();
    HMENU data = CreatePopupMenu();
    HMENU search = CreatePopupMenu();
    HMENU view = CreatePopupMenu();
    HMENU lineSpacing = CreatePopupMenu();
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
    AppendMenuW(file, MF_STRING, kIdFileSaveAll, L"保存全部\tCtrl+Alt+S");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, kIdFileRename, L"重命名...\tF2");
    AppendMenuW(file, MF_STRING, kIdFileClose, L"关闭标签\tCtrl+W");
    AppendMenuW(file, MF_STRING, kIdFileCloseAll, L"关闭全部标签\tCtrl+Shift+W");
    AppendMenuW(file, MF_STRING, kIdFileReopenClosed, L"重新打开关闭的标签\tCtrl+Shift+T");
    AppendMenuW(file, MF_STRING, kIdFileExit, L"退出");

    AppendMenuW(edit, MF_STRING, kIdEditUndo, L"撤销\tCtrl+Z");
    AppendMenuW(edit, MF_STRING, kIdEditRedo, L"重做\tCtrl+Y");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, kIdEditCut, L"剪切\tCtrl+X");
    AppendMenuW(edit, MF_STRING, kIdEditCopy, L"复制\tCtrl+C");
    AppendMenuW(edit, MF_STRING, kIdEditPaste, L"粘贴\tCtrl+V");
    AppendMenuW(edit, MF_STRING, kIdEditSelectAll, L"全选\tCtrl+A");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, kIdPrefixSuffix, L"前缀与后缀批量处理...");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, kIdMultiCaretNext, L"添加下一行光标\tCtrl+Alt+Down");
    AppendMenuW(edit, MF_STRING, kIdRectangleSelection, L"矩形选择（按当前选区创建）");
    AppendMenuW(edit, MF_STRING, kIdClearMultiCaret, L"清除多光标");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, kIdMacroToggle, L"开始/停止宏录制");
    AppendMenuW(edit, MF_STRING, kIdMacroPlay, L"播放最近宏");
    AppendMenuW(edit, MF_STRING, kIdMacroClear, L"清除最近宏");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendTextToolMenus(edit);
    AppendDataGeneratorMenus(data);

    AppendMenuW(search, MF_STRING, kIdToolbarFind, L"查找\tCtrl+F");
    AppendMenuW(search, MF_STRING, kIdToolbarReplace, L"替换\tCtrl+H");
    AppendMenuW(search, MF_STRING, kIdFindNext, L"查找下一个\tF3");
    AppendMenuW(search, MF_STRING, kIdFindPrevious, L"查找上一个\tShift+F3");
    AppendMenuW(search, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(search, MF_STRING, kIdSearchCurrentFile, L"在当前文件中查找...");
    AppendMenuW(search, MF_STRING, kIdSearchOpenFiles, L"在所有打开文件中查找...");
    AppendMenuW(search, MF_STRING, kIdSearchInFolder, L"在文件夹中查找...\tCtrl+Shift+F");
    AppendMenuW(search, MF_STRING, kIdReplaceInFolder, L"在文件夹中替换...\tCtrl+Shift+H");
    AppendMenuW(search, MF_STRING, kIdSearchFileNames, L"搜索文件名...");
    AppendMenuW(search, MF_STRING, kIdSearchRegex, L"正则表达式搜索...");
    AppendMenuW(search, MF_SEPARATOR, 0, nullptr);
    g_app.searchHistoryMenu = CreatePopupMenu();
    AppendMenuW(g_app.searchHistoryMenu, MF_STRING | MF_GRAYED, 0, L"（暂无搜索历史）");
    AppendMenuW(search, MF_POPUP, reinterpret_cast<UINT_PTR>(g_app.searchHistoryMenu), L"搜索历史");
    HMENU results = CreatePopupMenu();
    AppendMenuW(results, MF_STRING, kIdToggleSearchPanel, L"显示搜索面板");
    AppendMenuW(results, MF_STRING, kIdCopySearchResults, L"复制全部结果");
    AppendMenuW(results, MF_STRING, kIdExportSearchResults, L"导出搜索结果...");
    AppendMenuW(results, MF_STRING, kIdClearSearchResults, L"清空搜索结果");
    AppendMenuW(results, MF_STRING, kIdBookmarkSearchResult, L"将选中结果标记为书签");
    AppendMenuW(results, MF_STRING, kIdSearchErrorLog, L"查看错误日志");
    AppendMenuW(results, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(results, MF_STRING, kIdUndoReplaceInFiles, L"撤销上次在文件中替换...");
    AppendMenuW(search, MF_POPUP, reinterpret_cast<UINT_PTR>(results), L"搜索结果管理");
    AppendMenuW(search, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(search, MF_STRING, kIdToolbarGoto, L"跳转到行\tCtrl+G");
    AppendMenuW(view, MF_STRING, kIdToolbarLineNumbers, L"行号");
    AppendMenuW(view, MF_STRING, kIdToolbarWrap, L"自动换行");
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view, MF_STRING, kIdViewToggleBookmark, L"切换书签\tCtrl+F2");
    AppendMenuW(view, MF_STRING, kIdViewNextBookmark, L"下一个书签\tF4");
    AppendMenuW(view, MF_STRING, kIdViewPreviousBookmark, L"上一个书签\tShift+F4");
    AppendMenuW(view, MF_STRING, kIdViewClearBookmarks, L"清除当前文档书签");
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view, MF_STRING, kIdViewWhitespace, L"显示空格和制表符");
    AppendMenuW(view, MF_STRING, kIdViewIndentGuides, L"显示缩进参考线");
    AppendMenuW(view, MF_STRING, kIdViewFoldCurrent, L"折叠/展开当前代码块");
    AppendMenuW(view, MF_STRING, kIdViewUnfoldAll, L"展开全部代码块");
    AppendMenuW(lineSpacing, MF_STRING, kIdLineSpacingSingle, L"1.0 倍");
    AppendMenuW(lineSpacing, MF_STRING, kIdLineSpacing115, L"1.15 倍");
    AppendMenuW(lineSpacing, MF_STRING, kIdLineSpacing150, L"1.5 倍");
    AppendMenuW(lineSpacing, MF_STRING, kIdLineSpacing200, L"2.0 倍");
    AppendMenuW(lineSpacing, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(lineSpacing, MF_STRING, kIdLineSpacingCustom, L"自定义...");
    g_app.lineSpacingMenu = lineSpacing;
    AppendMenuW(view, MF_POPUP, reinterpret_cast<UINT_PTR>(lineSpacing), L"行距");
    AppendEncodingItems(encoding);
    for (int index = 0; index < kLanguageCount; ++index) {
        AppendMenuW(language, MF_STRING, kIdLanguageFirst + index, kLanguageNames[index]);
    }
    AppendMenuW(run, MF_STRING, kIdToolbarRun, L"运行当前文件\tF5");
    AppendMenuW(tools, MF_STRING, kIdToggleToolbox, L"开发者工具箱\tCtrl+Shift+D");
    AppendMenuW(tools, MF_STRING, kIdToolbarFormat, L"格式化 JSON\tShift+Alt+F");
    AppendMenuW(tools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(tools, MF_STRING, kIdCompareWithFile, L"比较当前文件与...");
    AppendMenuW(tools, MF_STRING, kIdCompareFiles, L"比较两个文件...");
    AppendMenuW(tools, MF_STRING, kIdCompareStructure, L"结构化比较 (JSON/CSV/YAML)...");
    AppendMenuW(tools, MF_STRING, kIdCompareFolders, L"比较文件夹...");
    AppendMenuW(tools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(tools, MF_STRING, kIdOpenLogViewer, L"日志查看器（大文件）...");
    AppendMenuW(theme, MF_STRING, kIdThemeDark, L"深色");
    AppendMenuW(theme, MF_STRING, kIdThemeLight, L"浅色");
    AppendMenuW(theme, MF_STRING, kIdThemeGreen, L"护眼绿");
    AppendMenuW(theme, MF_STRING, kIdThemeBlue, L"暗夜蓝");
    g_app.themeMenu = theme;
    AppendMenuW(settings, MF_POPUP, reinterpret_cast<UINT_PTR>(theme), L"主题");
    AppendMenuW(settings, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(settings, MF_STRING, kIdToolbarSettings, L"偏好设置...\tCtrl+,");
    AppendMenuW(settings, MF_STRING, kIdSettingsShortcuts, L"快捷键设置...\tCtrl+K");
    AppendMenuW(help, MF_STRING, kIdHelpAbout, L"关于 MCnotepad");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"文件(F)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), L"编辑(E)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(data), L"数据(D)");
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
    bool pinned = false;
    std::vector<int> bookmarks;
    DocumentFormat format;
};

void LoadPersistedSession() {
    InitializeShortcutBindings();
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
    int persistedLineSpacing = g_app.lineSpacingPercent;
    int persistedShowLineNumbers = g_app.showLineNumbers ? 1 : 0;
    int persistedWordWrap = g_app.wordWrap ? 1 : 0;
    int persistedSidebarVisible = g_app.sidebarVisible ? 1 : 0;
    int persistedFontSize = g_app.fontSize;
    int persistedLanguage = g_app.languageIndex;
    int persistedEncoding = g_app.encodingIndex;
    int persistedLineEnding = g_app.lineEndingIndex;
    int persistedShowWhitespace = g_app.showWhitespace ? 1 : 0;
    int persistedShowIndentGuides = g_app.showIndentGuides ? 1 : 0;
    int persistedStartup = g_app.launchAtStartup ? 1 : 0;
    int persistedCloseBehavior = static_cast<int>(g_app.closeBehavior);
    int persistedWindowCloseBehavior = static_cast<int>(g_app.windowCloseBehavior);
    int persistedMinimizeToTray = g_app.minimizeToTray ? 1 : 0;
    int persistedShowTrayIcon = g_app.showTrayIcon ? 1 : 0;
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
        } else if (parts[0] == "LINE_SPACING" && parts.size() >= 2) {
            ParseInt(parts[1], persistedLineSpacing);
        } else if (parts[0] == "OPTIONS" && parts.size() >= 9) {
            ParseInt(parts[1], persistedShowLineNumbers);
            ParseInt(parts[2], persistedWordWrap);
            ParseInt(parts[3], persistedSidebarVisible);
            ParseInt(parts[4], persistedFontSize);
            persistedFontFamily = Utf8ToWide(HexDecode(parts[5]));
            ParseInt(parts[6], persistedLanguage);
            ParseInt(parts[7], persistedEncoding);
            ParseInt(parts[8], persistedLineEnding);
            if (parts.size() >= 10) ParseInt(parts[9], persistedShowWhitespace);
            if (parts.size() >= 11) ParseInt(parts[10], persistedShowIndentGuides);
        } else if (parts[0] == "TOOLBOX" && parts.size() >= 5) {
            int visible = 0, width = 380, mask = 0x3FF, tool = 0;
            ParseInt(parts[1], visible);
            ParseInt(parts[2], width);
            ParseInt(parts[3], mask);
            ParseInt(parts[4], tool);
            g_app.toolboxVisible = visible != 0;
            g_app.toolboxWidth = std::clamp(width, 260, 900);
            g_app.toolboxMask = static_cast<unsigned>(mask) & 0x3FF;
            if (!g_app.toolboxMask) g_app.toolboxMask = 0x3FF;
            g_app.toolboxTool = std::clamp(tool, 0, 9);
        } else if (parts[0] == "PREFERENCES" && parts.size() >= 3) {
            ParseInt(parts[1], persistedStartup);
            ParseInt(parts[2], persistedCloseBehavior);
            int preview = 1;
            if (parts.size() >= 4 && ParseInt(parts[3], preview)) g_app.previewTextTools = preview != 0;
            int outputMode = 1;
            if (parts.size() >= 5 && ParseInt(parts[4], outputMode)) g_app.toolOutputMode = std::clamp(outputMode, 0, 2);
            if (parts.size() >= 6) ParseInt(parts[5], persistedWindowCloseBehavior);
            if (parts.size() >= 7) ParseInt(parts[6], persistedMinimizeToTray);
            if (parts.size() >= 8) ParseInt(parts[7], persistedShowTrayIcon);
        } else if (parts[0] == "SHORTCUTS" && parts.size() >= 3) {
            // Pairs are stored as command + hex encoded UTF-8 text. Unknown
            // commands are ignored so newer session files remain compatible.
            for (size_t part = 1; part + 1 < parts.size(); part += 2) {
                int command = 0;
                if (!ParseInt(parts[part], command)) continue;
                const std::wstring text = Utf8ToWide(HexDecode(parts[part + 1]));
                for (ShortcutBinding& binding : g_shortcuts) {
                    if (binding.command == command) {
                        binding.text = text;
                        break;
                    }
                }
            }
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
            if (parts.size() >= 16 && !parts[15].empty()) {
                size_t start = 0;
                while (start < parts[15].size()) {
                    const size_t comma = parts[15].find(',', start);
                    const std::string_view token(parts[15].data() + start,
                        comma == std::string::npos ? parts[15].size() - start : comma - start);
                    int bookmark = 0;
                    if (ParseInt(token, bookmark) && bookmark > 0) entry.bookmarks.push_back(bookmark);
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
                std::sort(entry.bookmarks.begin(), entry.bookmarks.end());
                entry.bookmarks.erase(std::unique(entry.bookmarks.begin(), entry.bookmarks.end()), entry.bookmarks.end());
            }
            if (parts.size() >= 17) {
                int pinned = 0;
                ParseInt(parts[16], pinned);
                entry.pinned = pinned != 0;
            }
            entry.path = Utf8ToWide(HexDecode(parts[2]));
            entry.title = Utf8ToWide(HexDecode(parts[3]));
            entry.snapshot = fs::path(Utf8ToWide(HexDecode(parts[4])));
            entries.push_back(std::move(entry));
        }
    }

    g_app.theme = static_cast<ThemeKind>(std::clamp(persistedTheme, 0, 3));
    g_app.lineSpacingPercent = std::clamp(persistedLineSpacing, 50, 300);
    g_app.showLineNumbers = persistedShowLineNumbers != 0;
    g_app.wordWrap = persistedWordWrap != 0;
    g_app.showWhitespace = persistedShowWhitespace != 0;
    g_app.showIndentGuides = persistedShowIndentGuides != 0;
    g_app.sidebarVisible = false;
    g_app.fontSize = std::clamp(persistedFontSize, 10, 32);
    if (!persistedFontFamily.empty()) g_app.fontFamily = persistedFontFamily;
    g_app.languageIndex = std::clamp(persistedLanguage, 0, kLanguageCount - 1);
    g_app.encodingIndex = std::clamp(persistedEncoding, 0, kEncodingCount - 1);
    g_app.lineEndingIndex = std::clamp(persistedLineEnding, 0, kLineEndingCount - 1);
    g_app.launchAtStartup = persistedStartup != 0;
    g_app.closeBehavior = static_cast<CloseBehavior>(std::clamp(persistedCloseBehavior, 0, 2));
    g_app.windowCloseBehavior = static_cast<WindowCloseBehavior>(std::clamp(persistedWindowCloseBehavior, 0, 2));
    g_app.minimizeToTray = persistedMinimizeToTray != 0;
    g_app.showTrayIcon = persistedShowTrayIcon != 0;

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
            created.pinned = entry.pinned;
            created.bookmarks = std::move(entry.bookmarks);
            NormalizeBookmarks(created);
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

void CompareFiles(const std::wstring& leftPath, const std::wstring& rightPath, bool structure = false);
void CompareFolderPaths(const std::wstring& left, const std::wstring& right);
bool OpenLogViewer(const std::wstring& path);

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
        if (argument == L"--log" && index + 1 < argumentCount) {
            OpenLogViewer(fs::absolute(fs::path(arguments[++index])).wstring());
            continue;
        }
        if ((argument == L"--diff" || argument == L"--diff-struct") && index + 2 < argumentCount) {
            const std::wstring left = arguments[index + 1], right = arguments[index + 2];
            index += 2;
            std::error_code error;
            if (fs::is_directory(left, error) && fs::is_directory(right, error)) CompareFolderPaths(left, right);
            else CompareFiles(left, right, argument == L"--diff-struct");
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

bool ParseShortcutText(std::wstring value, BYTE& modifiers, WORD& key) {
    modifiers = FVIRTKEY;
    key = 0;
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (start <= value.size()) {
        const size_t plus = value.find(L'+', start);
        parts.push_back(value.substr(start, plus == std::wstring::npos ? std::wstring::npos : plus - start));
        if (plus == std::wstring::npos) break;
        start = plus + 1;
    }
    if (parts.empty()) return false;
    for (std::wstring& part : parts) {
        while (!part.empty() && iswspace(part.front())) part.erase(part.begin());
        while (!part.empty() && iswspace(part.back())) part.pop_back();
        for (wchar_t& character : part) character = static_cast<wchar_t>(towupper(character));
    }
    for (size_t index = 0; index + 1 < parts.size(); ++index) {
        if (parts[index] == L"CTRL" || parts[index] == L"CONTROL") modifiers |= FCONTROL;
        else if (parts[index] == L"SHIFT") modifiers |= FSHIFT;
        else if (parts[index] == L"ALT") modifiers |= FALT;
        else return false;
    }
    const std::wstring& token = parts.back();
    if (token.size() == 1 && ((token[0] >= L'A' && token[0] <= L'Z') || (token[0] >= L'0' && token[0] <= L'9'))) {
        key = static_cast<WORD>(token[0]);
    } else if (token.size() >= 2 && token[0] == L'F') {
        const int functionKey = _wtoi(token.c_str() + 1);
        if (functionKey < 1 || functionKey > 24) return false;
        key = static_cast<WORD>(VK_F1 + functionKey - 1);
    } else if (token == L"COMMA") key = VK_OEM_COMMA;
    else if (token == L"PERIOD" || token == L"DOT") key = VK_OEM_PERIOD;
    else if (token == L"PLUS") key = VK_OEM_PLUS;
    else if (token == L"MINUS") key = VK_OEM_MINUS;
    else if (token == L"TAB") key = VK_TAB;
    else if (token == L"ENTER" || token == L"RETURN") key = VK_RETURN;
    else if (token == L"ESC" || token == L"ESCAPE") key = VK_ESCAPE;
    else if (token == L"SPACE") key = VK_SPACE;
    else return false;
    return (modifiers & (FCONTROL | FSHIFT | FALT)) != 0;
}

LRESULT CALLBACK ShortcutDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_shortcutEditControls.clear();
        int y = 16;
        for (size_t index = 0; index < g_shortcuts.size(); ++index) {
            const ShortcutBinding& binding = g_shortcuts[index];
            HWND label = CreateWindowExW(0, L"STATIC", binding.label,
                WS_CHILD | WS_VISIBLE, Scale(24), Scale(y + 3), Scale(190), Scale(24),
                window, nullptr, g_app.instance, nullptr);
            HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", binding.text.c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                Scale(220), Scale(y), Scale(250), Scale(26), window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdShortcutEditFirst + index)),
                g_app.instance, nullptr);
            SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            if (g_app.darkTheme) SetWindowTheme(edit, L"DarkMode_Explorer", nullptr);
            g_shortcutEditControls.push_back(edit);
            y += 30;
        }
        HWND hint = CreateWindowExW(0, L"STATIC",
            L"格式：Ctrl+Shift+S、F5、Alt+1。快捷键不能与其他功能重复。",
            WS_CHILD | WS_VISIBLE, Scale(24), Scale(y + 4), Scale(500), Scale(24),
            window, nullptr, g_app.instance, nullptr);
        SendMessageW(hint, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        HWND apply = CreateWindowExW(0, L"BUTTON", L"应用",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            Scale(270), Scale(y + 34), Scale(82), Scale(30), window,
            reinterpret_cast<HMENU>(kIdShortcutApply), g_app.instance, nullptr);
        HWND reset = CreateWindowExW(0, L"BUTTON", L"恢复默认",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(360), Scale(y + 34), Scale(92), Scale(30), window,
            reinterpret_cast<HMENU>(kIdShortcutReset), g_app.instance, nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(460), Scale(y + 34), Scale(82), Scale(30), window,
            reinterpret_cast<HMENU>(kIdShortcutCancel), g_app.instance, nullptr);
        ThemePushButtons({apply, reset, cancel});
        return 0;
    }
    case WM_COMMAND: {
        const int command = LOWORD(wParam);
        if (command == kIdShortcutReset) {
            g_shortcuts.clear();
            InitializeShortcutBindings();
            for (size_t index = 0; index < g_shortcutEditControls.size() && index < g_shortcuts.size(); ++index)
                SetWindowTextW(g_shortcutEditControls[index], g_shortcuts[index].text.c_str());
            return 0;
        }
        if (command == kIdShortcutCancel || command == IDCANCEL) {
            DestroyWindow(window);
            return 0;
        }
        if (command == kIdShortcutApply || command == IDOK) {
            std::vector<std::wstring> values;
            values.reserve(g_shortcuts.size());
            std::vector<std::pair<BYTE, WORD>> parsed;
            for (size_t index = 0; index < g_shortcutEditControls.size(); ++index) {
                std::wstring value = GetWindowTextValue(g_shortcutEditControls[index]);
                while (!value.empty() && iswspace(value.front())) value.erase(value.begin());
                while (!value.empty() && iswspace(value.back())) value.pop_back();
                BYTE modifiers = 0;
                WORD key = 0;
                if (!ParseShortcutText(value, modifiers, key)) {
                    MessageBoxW(window, (L"快捷键格式无效：" + value).c_str(), kWindowTitle, MB_ICONWARNING);
                    SetFocus(g_shortcutEditControls[index]);
                    return 0;
                }
                if (std::find(parsed.begin(), parsed.end(), std::make_pair(modifiers, key)) != parsed.end()) {
                    MessageBoxW(window, L"快捷键不能重复，请修改后再应用。", kWindowTitle, MB_ICONWARNING);
                    SetFocus(g_shortcutEditControls[index]);
                    return 0;
                }
                parsed.emplace_back(modifiers, key);
                values.push_back(std::move(value));
            }
            for (size_t index = 0; index < values.size(); ++index) g_shortcuts[index].text = values[index];
            RebuildAccelerators();
            g_app.stateDirty = true;
            PersistAll(false);
            DestroyWindow(window);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        g_shortcutEditControls.clear();
        g_shortcutDialog = nullptr;
        {
            HWND owner = g_shortcutDialogOwner ? g_shortcutDialogOwner : g_app.window;
            g_shortcutDialogOwner = nullptr;
            if (owner && IsWindow(owner)) {
                EnableWindow(owner, TRUE);
                SetForegroundWindow(owner);
            }
        }
        return 0;
    case WM_ERASEBKGND: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        RECT client{};
        GetClientRect(window, &client);
        FillRect(dc, &client, g_app.windowBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        const ThemePalette& palette = CurrentPalette();
        SetTextColor(dc, palette.controlText);
        SetBkColor(dc, palette.windowBackground);
        return reinterpret_cast<LRESULT>(g_app.windowBrush);
    }
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void OpenShortcutDialog() {
    if (g_shortcutDialog) {
        SetForegroundWindow(g_shortcutDialog);
        return;
    }
    InitializeShortcutBindings();
    WNDCLASSW klass{};
    klass.hInstance = g_app.instance;
    klass.lpfnWndProc = ShortcutDialogProc;
    klass.lpszClassName = L"MCnotepadShortcuts";
    klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    klass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&klass);
    const int height = 16 + static_cast<int>(g_shortcuts.size()) * 30 + 24 + 64;
    g_shortcutDialogOwner = g_app.settingsWindow ? g_app.settingsWindow : g_app.window;
    g_shortcutDialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        klass.lpszClassName, L"MCnotepad 快捷键设置",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, Scale(570), Scale(height),
        g_shortcutDialogOwner, nullptr, g_app.instance, nullptr);
    if (!g_shortcutDialog) {
        g_shortcutDialogOwner = nullptr;
        return;
    }
    EnableWindow(g_shortcutDialogOwner, FALSE);
    ApplyWindowFrameTheme(g_shortcutDialog);
    ShowWindow(g_shortcutDialog, SW_SHOW);
    UpdateWindow(g_shortcutDialog);
}

HACCEL CreateAccelerators() {
    InitializeShortcutBindings();
    std::vector<ACCEL> entries;
    for (const ShortcutBinding& binding : g_shortcuts) {
        BYTE modifiers = 0;
        WORD key = 0;
        if (ParseShortcutText(binding.text, modifiers, key)) entries.push_back({modifiers, key, static_cast<WORD>(binding.command)});
    }
    const ACCEL fixed[] = {
        { FCONTROL | FALT | FVIRTKEY, 'S', kIdFileSaveAll },
        { FCONTROL | FSHIFT | FVIRTKEY, 'W', kIdFileCloseAll },
        { FCONTROL | FSHIFT | FVIRTKEY, 'T', kIdFileReopenClosed },
        { FCONTROL | FVIRTKEY, 'Z', kIdEditUndo },
        { FCONTROL | FVIRTKEY, 'Y', kIdEditRedo },
        { FCONTROL | FVIRTKEY, 'X', kIdEditCut },
        { FCONTROL | FVIRTKEY, 'C', kIdEditCopy },
        { FCONTROL | FVIRTKEY, 'V', kIdEditPaste },
        { FCONTROL | FVIRTKEY, 'A', kIdEditSelectAll },
        { FCONTROL | FALT | FVIRTKEY, VK_DOWN, kIdMultiCaretNext },
        { FALT | FSHIFT | FVIRTKEY, 'F', kIdToolbarFormat },
        { FCONTROL | FSHIFT | FVIRTKEY, 'H', kIdReplaceInFolder },
        { FVIRTKEY, VK_F5, kIdToolbarRun },
        { FVIRTKEY, VK_F2, kIdFileRename },
        { FVIRTKEY, VK_F3, kIdFindNext },
        { FSHIFT | FVIRTKEY, VK_F3, kIdFindPrevious }
    };
    entries.insert(entries.end(), std::begin(fixed), std::end(fixed));
    return CreateAcceleratorTableW(entries.data(), static_cast<int>(entries.size()));
}

void RebuildAccelerators() {
    if (g_accelerators) DestroyAcceleratorTable(g_accelerators);
    g_accelerators = CreateAccelerators();
}

void HandleEditCommand(int command) {
    HWND focus = GetFocus();
    if (focus && ((g_app.searchPanel && IsChild(g_app.searchPanel, focus)) || (g_app.toolboxPanel && IsChild(g_app.toolboxPanel, focus)))) {
        wchar_t className[32]{};
        GetClassNameW(focus, className, 32);
        if (_wcsicmp(className, L"Edit") == 0) {
            switch (command) {
            case kIdEditUndo: SendMessageW(focus, EM_UNDO, 0, 0); break;
            case kIdEditCut: SendMessageW(focus, WM_CUT, 0, 0); break;
            case kIdEditCopy: SendMessageW(focus, WM_COPY, 0, 0); break;
            case kIdEditPaste: SendMessageW(focus, WM_PASTE, 0, 0); break;
            case kIdEditSelectAll: SendMessageW(focus, EM_SETSEL, 0, -1); break;
            default: break;
            }
            return;
        }
    }
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
        // Keep the selected Latin font while allowing RichEdit to select a
        // CJK fallback font for Chinese/Japanese/Korean characters. Without
        // dual-font mode, Consolas renders these characters as tofu boxes.
        const LRESULT languageOptions = SendMessageW(document.editor, EM_GETLANGOPTIONS, 0, 0);
        SendMessageW(document.editor, EM_SETLANGOPTIONS, 0,
            (languageOptions | IMF_AUTOFONT | IMF_DUALFONT) &
            ~static_cast<LRESULT>(IMF_AUTOFONTSIZEADJUST));
        SendMessageW(document.editor, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(Scale(6), 0));
        InvalidateRect(document.editor, nullptr, TRUE);
    }
    RefreshGutter();
}

void RemoveTrayIcon() {
    if (!g_app.trayIconAdded || !g_app.window) return;
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = g_app.window;
    data.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &data);
    g_app.trayIconAdded = false;
}

void UpdateTrayIcon() {
    if (!g_app.window) return;
    const bool shouldShow = g_app.showTrayIcon || g_app.hiddenToTray;
    if (!shouldShow) {
        RemoveTrayIcon();
        return;
    }

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = g_app.window;
    data.uID = kTrayIconId;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = kMessageTrayIcon;
    data.hIcon = static_cast<HICON>(LoadImageW(g_app.instance, MAKEINTRESOURCEW(IDI_MCNOTEPAD),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    bool ownsIcon = data.hIcon != nullptr;
    if (!data.hIcon) data.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(data.szTip, L"MCnotepad", _TRUNCATE);

    if (g_app.trayIconAdded) {
        Shell_NotifyIconW(NIM_MODIFY, &data);
    } else if (Shell_NotifyIconW(NIM_ADD, &data)) {
        g_app.trayIconAdded = true;
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
    }
    if (ownsIcon) DestroyIcon(data.hIcon);
}

void RestoreMainWindowFromTray() {
    g_app.hiddenToTray = false;
    UpdateTrayIcon();
    ShowWindow(g_app.window, SW_SHOW);
    ShowWindow(g_app.window, IsZoomed(g_app.window) ? SW_MAXIMIZE : SW_RESTORE);
    SetForegroundWindow(g_app.window);
}

void HideMainWindowToTray() {
    if (!g_app.window) return;
    g_app.hiddenToTray = true;
    PersistAll(false);
    UpdateTrayIcon();
    ShowWindow(g_app.window, SW_HIDE);
}

bool HandleMainWindowClose(bool forceExit) {
    if (g_app.closing) return true;
    if (!forceExit && g_app.windowCloseBehavior == WindowCloseBehavior::MinimizeToTray) {
        HideMainWindowToTray();
        return false;
    }
    if (!forceExit && g_app.windowCloseBehavior == WindowCloseBehavior::Prompt) {
        const int choice = MessageBoxW(g_app.window,
            L"关闭 MCnotepad：\n\n是：退出软件\n否：最小化到系统托盘\n取消：继续使用",
            kWindowTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
        if (choice == IDCANCEL) return false;
        if (choice == IDNO) {
            HideMainWindowToTray();
            return false;
        }
    }
    if (!PrepareApplicationClose()) return false;

    g_app.closing = true;
    KillTimer(g_app.window, kTimerAutosave);
    CancelSearch(true);
    SaveSearchState();
    const bool persisted = PersistAll(true);
    if (!persisted) {
        MessageBoxW(g_app.window, L"会话恢复数据保存失败，程序将继续退出。", kWindowTitle, MB_ICONWARNING);
    }
    RemoveRunningMarker();
    DestroyWindow(g_app.window);
    return true;
}

void CreateSettingsControl(HWND parent, const wchar_t* className, const wchar_t* text,
    DWORD style, int id, int x, int y, int width, int height) {
    HWND control = CreateWindowExW(0, className, text, WS_CHILD | WS_VISIBLE | style,
        Scale(x), Scale(y), Scale(width), Scale(height), parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        g_app.instance, nullptr);
    if (control) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        if (_wcsicmp(className, L"COMBOBOX") == 0) {
            SetWindowTheme(control, g_app.darkTheme ? L"DarkMode_CFD" : L"Explorer", nullptr);
        } else if (_wcsicmp(className, L"BUTTON") == 0) {
            const LONG buttonType = static_cast<LONG>(style & BS_TYPEMASK);
            if (buttonType == BS_AUTOCHECKBOX || buttonType == BS_AUTO3STATE) {
                ThemeCheckbox(control);
            } else if (g_app.darkTheme) {
                SetWindowTheme(control, L"DarkMode_Explorer", nullptr);
            }
        }
    }
    if (id == kIdSettingsFont) g_app.settingsFontCombo = control;
    if (id == kIdSettingsSize) g_app.settingsSizeCombo = control;
    if (id == kIdSettingsTheme) g_app.settingsThemeCombo = control;
    if (id == kIdSettingsLineNumbers) g_app.settingsLineNumbersCheck = control;
    if (id == kIdSettingsWrap) g_app.settingsWrapCheck = control;
    if (id == kIdSettingsStartup) g_app.settingsStartupCheck = control;
    if (id == kIdSettingsCloseBehavior) g_app.settingsCloseBehaviorCombo = control;
    if (id == kIdSettingsWindowCloseBehavior) g_app.settingsWindowCloseBehaviorCombo = control;
    if (id == kIdSettingsMinimizeToTray) g_app.settingsMinimizeToTrayCheck = control;
    if (id == kIdSettingsShowTray) g_app.settingsShowTrayCheck = control;
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
        CreateSettingsControl(window, L"BUTTON", L"显示系统托盘图标", BS_AUTOCHECKBOX | WS_TABSTOP,
            kIdSettingsShowTray, 215, 132, 190, 26);
        SendMessageW(g_app.settingsShowTrayCheck, BM_SETCHECK,
            g_app.showTrayIcon ? BST_CHECKED : BST_UNCHECKED, 0);
        CreateSettingsControl(window, L"BUTTON", L"最小化时隐藏到托盘", BS_AUTOCHECKBOX | WS_TABSTOP,
            kIdSettingsMinimizeToTray, 26, 164, 190, 26);
        SendMessageW(g_app.settingsMinimizeToTrayCheck, BM_SETCHECK,
            g_app.minimizeToTray ? BST_CHECKED : BST_UNCHECKED, 0);
        addLabel(L"关闭按钮行为", 26, 202, 150, 24);
        CreateSettingsControl(window, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP,
            kIdSettingsWindowCloseBehavior, 190, 198, 280, 180);
        for (const wchar_t* value : {L"退出软件", L"最小化到系统托盘", L"每次询问"})
            SendMessageW(g_app.settingsWindowCloseBehaviorCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        SendMessageW(g_app.settingsWindowCloseBehaviorCombo, CB_SETCURSEL,
            static_cast<int>(g_app.windowCloseBehavior), 0);
        addLabel(L"退出时未保存标签", 26, 240, 150, 24);
        CreateSettingsControl(window, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP,
            kIdSettingsCloseBehavior, 190, 236, 280, 180);
        for (const wchar_t* value : {L"提示保存", L"直接关闭（保留恢复内容）", L"自动保存后关闭"})
            SendMessageW(g_app.settingsCloseBehaviorCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        SendMessageW(g_app.settingsCloseBehaviorCombo, CB_SETCURSEL,
            static_cast<int>(g_app.closeBehavior), 0);
        CreateSettingsControl(window, L"BUTTON", L"显示行号", BS_AUTOCHECKBOX | WS_TABSTOP,
            kIdSettingsLineNumbers, 26, 278, 170, 26);
        CreateSettingsControl(window, L"BUTTON", L"自动换行", BS_AUTOCHECKBOX | WS_TABSTOP,
            kIdSettingsWrap, 215, 278, 170, 26);
        SendMessageW(g_app.settingsLineNumbersCheck, BM_SETCHECK, g_app.showLineNumbers ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(g_app.settingsWrapCheck, BM_SETCHECK, g_app.wordWrap ? BST_CHECKED : BST_UNCHECKED, 0);
        CreateSettingsControl(window, L"BUTTON", L"快捷键设置（Ctrl+K）", BS_PUSHBUTTON | WS_TABSTOP,
            kIdSettingsShortcuts, 26, 310, 190, 30);
        CreateSettingsControl(window, L"BUTTON", L"应用", BS_DEFPUSHBUTTON | WS_TABSTOP,
            kIdSettingsApply, 360, 340, 82, 30);
        CreateSettingsControl(window, L"BUTTON", L"取消", BS_PUSHBUTTON | WS_TABSTOP,
            kIdSettingsCancel, 460, 340, 82, 30);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kIdSettingsShortcuts) {
            OpenShortcutDialog();
            return 0;
        }
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
            const int windowCloseIndex = static_cast<int>(SendMessageW(
                g_app.settingsWindowCloseBehaviorCombo, CB_GETCURSEL, 0, 0));
            if (windowCloseIndex >= 0 && windowCloseIndex <= 2) {
                g_app.windowCloseBehavior = static_cast<WindowCloseBehavior>(windowCloseIndex);
            }
            g_app.minimizeToTray = SendMessageW(g_app.settingsMinimizeToTrayCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_app.showTrayIcon = SendMessageW(g_app.settingsShowTrayCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (g_app.windowCloseBehavior == WindowCloseBehavior::MinimizeToTray) {
                g_app.showTrayIcon = true;
            }
            if (!SetStartupRegistration(startupEnabled)) {
                MessageBoxW(window, L"开机启动设置写入失败，请检查当前账户的注册表权限。",
                    kWindowTitle, MB_ICONWARNING);
            } else {
                g_app.launchAtStartup = startupEnabled;
            }
            ApplyWordWrapAll();
            ApplyEditorFont();
            UpdateTrayIcon();
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
        CW_USEDEFAULT, CW_USEDEFAULT, Scale(560), Scale(410), g_app.window, nullptr, g_app.instance, nullptr);
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
        ApplyEditorTheme(document.editor);
        SendMessageW(document.editor, EM_SETMODIFY, FALSE, 0);
    }
    document.encoding = encoding;
    document.lineEnding = lineEnding;
    document.modified = false;
    document.highlightedLine = -1;
    document.bracketMatchStart = -1;
    document.bracketMatchEnd = -1;
    document.textHash = HashText(GetEditorRawText(document.editor));
    document.fileTimestamp = FileTimestamp(document.path);
    MarkRecoveryDirty(index);
    HighlightDocument(document);
    SendMessageW(document.editor, EM_EMPTYUNDOBUFFER, 0, 0);
    const LONG length = GetWindowTextLengthW(document.editor);
    selection.cpMin = std::min(selection.cpMin, length);
    selection.cpMax = std::min(selection.cpMax, length);
    SendMessageW(document.editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    ApplyCurrentLineHighlight(document);
    ApplyBracketMatchHighlight(document, true);
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

// The developer tools below intentionally use a small, allocation-friendly
// lexer instead of a third-party parser.  Strings, template literals and
// comments are treated as opaque tokens, so normal CSS/JavaScript pasted into
// the editor is never modified inside a literal.  The tools are formatters,
// not validators; malformed input is still returned in a deterministic form.
bool CodeToolWordChar(wchar_t c) {
    return std::iswalnum(c) || c == L'_' || c == L'$' || c == L'-';
}

bool CodeToolNeedsSpace(wchar_t left, wchar_t right) {
    return CodeToolWordChar(left) && CodeToolWordChar(right);
}

void CodeToolTrimLine(std::wstring& output) {
    while (!output.empty() && (output.back() == L' ' || output.back() == L'\t')) output.pop_back();
}

void CodeToolNewline(std::wstring& output, int indent, bool& lineStart) {
    CodeToolTrimLine(output);
    if (output.empty() || output.size() < 2 || output.compare(output.size() - 2, 2, L"\r\n") != 0) output += L"\r\n";
    output.append(static_cast<size_t>(std::max(0, indent)) * 4, L' ');
    lineStart = true;
}

size_t CodeToolSkipSpace(std::wstring_view input, size_t index) {
    while (index < input.size() && std::iswspace(input[index])) ++index;
    return index;
}

bool CssDeclarationColon(std::wstring_view input, size_t index) {
    int parentheses = 0;
    for (size_t i = index + 1; i < input.size(); ++i) {
        const wchar_t c = input[i];
        if (c == L'(') ++parentheses;
        else if (c == L')' && parentheses > 0) --parentheses;
        else if (parentheses == 0 && (c == L';' || c == L'{' || c == L'}' || c == L'\r' || c == L'\n')) {
            return c == L';';
        }
    }
    return true;
}

std::wstring FormatCss(std::wstring_view input) {
    std::wstring output;
    output.reserve(input.size() + input.size() / 3);
    int indent = 0;
    int parentheses = 0;
    bool lineStart = true;
    bool pendingSpace = false;
    wchar_t quote = 0;
    for (size_t i = 0; i < input.size();) {
        const wchar_t c = input[i];
        if (quote != 0) {
            output += c;
            ++i;
            if (c == L'\\' && i < input.size()) output += input[i++];
            else if (c == quote) quote = 0;
            lineStart = false;
            continue;
        }
        if (c == L'\'' || c == L'\"') {
            if (pendingSpace && !lineStart && !output.empty() && output.back() != L' ') output += L' ';
            pendingSpace = false;
            output += c; quote = c; lineStart = false; ++i; continue;
        }
        if (c == L'/' && i + 1 < input.size() && input[i + 1] == L'*') {
            const size_t end = input.find(L"*/", i + 2);
            const size_t length = end == std::wstring_view::npos ? input.size() - i : end + 2 - i;
            if (!lineStart) CodeToolNewline(output, indent, lineStart);
            output.append(input.substr(i, length));
            lineStart = false;
            CodeToolNewline(output, indent, lineStart);
            i += length; pendingSpace = false; continue;
        }
        if (std::iswspace(c)) { pendingSpace = true; ++i; continue; }
        if (c == L'{') {
            if (pendingSpace && !lineStart && !output.empty() && output.back() != L' ') output += L' ';
            output += L'{'; ++indent; CodeToolNewline(output, indent, lineStart);
            pendingSpace = false; ++i; continue;
        }
        if (c == L'}') {
            CodeToolTrimLine(output);
            if (!lineStart) CodeToolNewline(output, std::max(0, indent - 1), lineStart);
            indent = std::max(0, indent - 1);
            output += L'}'; lineStart = false;
            const size_t next = CodeToolSkipSpace(input, i + 1);
            if (next >= input.size() || (input[next] != L';' && input[next] != L',')) CodeToolNewline(output, indent, lineStart);
            pendingSpace = false; ++i; continue;
        }
        if (c == L';') {
            CodeToolTrimLine(output); output += L';';
            if (parentheses == 0) CodeToolNewline(output, indent, lineStart);
            else output += L' ';
            pendingSpace = false; ++i; continue;
        }
        if (c == L'(') { output += c; ++parentheses; lineStart = false; pendingSpace = false; ++i; continue; }
        if (c == L')') { CodeToolTrimLine(output); output += c; parentheses = std::max(0, parentheses - 1); lineStart = false; pendingSpace = false; ++i; continue; }
        if (c == L':') {
            CodeToolTrimLine(output); output += L':';
            if (CssDeclarationColon(input, i)) output += L' ';
            lineStart = false; pendingSpace = false; ++i; continue;
        }
        if (c == L',') {
            CodeToolTrimLine(output); output += L", "; lineStart = false; pendingSpace = false; ++i; continue;
        }
        if (pendingSpace && !lineStart && !output.empty() && output.back() != L' ' && output.back() != L'\r' && output.back() != L'\n') output += L' ';
        output += c; lineStart = false; pendingSpace = false; ++i;
    }
    CodeToolTrimLine(output);
    while (output.size() >= 2 && output.compare(output.size() - 2, 2, L"\r\n") == 0) output.resize(output.size() - 2);
    return output;
}

std::wstring MinifyCss(std::wstring_view input) {
    std::wstring output;
    output.reserve(input.size());
    bool pendingSpace = false;
    wchar_t quote = 0;
    for (size_t i = 0; i < input.size();) {
        const wchar_t c = input[i];
        if (quote != 0) {
            output += c; ++i;
            if (c == L'\\' && i < input.size()) output += input[i++];
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == L'\'' || c == L'\"') { if (pendingSpace && !output.empty() && CodeToolWordChar(output.back())) output += L' '; pendingSpace = false; output += c; quote = c; ++i; continue; }
        if (c == L'/' && i + 1 < input.size() && input[i + 1] == L'*') {
            const size_t end = input.find(L"*/", i + 2);
            const size_t length = end == std::wstring_view::npos ? input.size() - i : end + 2 - i;
            const bool keep = i + 2 < input.size() && input[i + 2] == L'!';
            if (keep) { if (pendingSpace && !output.empty()) output += L' '; output.append(input.substr(i, length)); }
            i += length; pendingSpace = false; continue;
        }
        if (std::iswspace(c)) { pendingSpace = true; ++i; continue; }
        if (c == L';') {
            size_t next = CodeToolSkipSpace(input, i + 1);
            if (next < input.size() && input[next] == L'}') { pendingSpace = false; ++i; continue; }
        }
        const bool punctuation = c == L'{' || c == L'}' || c == L';' || c == L':' || c == L',' || c == L'>' || c == L'~';
        if (pendingSpace && !output.empty() && !punctuation && output.back() != L'{' && output.back() != L';' && output.back() != L':') {
            if (CodeToolNeedsSpace(output.back(), c)) output += L' ';
        }
        pendingSpace = false;
        if (punctuation) while (!output.empty() && output.back() == L' ') output.pop_back();
        output += c; ++i;
    }
    while (!output.empty() && std::iswspace(output.back())) output.pop_back();
    return output;
}

bool JsKeywordBeforeParen(const std::wstring& output) {
    static constexpr const wchar_t* words[] = {L"if", L"for", L"while", L"switch", L"catch", L"with"};
    for (const wchar_t* word : words) {
        const size_t length = std::wcslen(word);
        if (output.size() >= length && output.compare(output.size() - length, length, word) == 0) {
            if (output.size() == length || !CodeToolWordChar(output[output.size() - length - 1])) return true;
        }
    }
    return false;
}

bool JsUnaryOperator(const std::wstring& output) {
    size_t i = output.size();
    while (i > 0 && std::iswspace(output[i - 1])) --i;
    if (i == 0) return true;
    return std::wcschr(L"([{=,:;!?&|+-*/%<>", output[i - 1]) != nullptr;
}

bool JsLooksLikeRegexStart(const std::wstring& output) {
    size_t i = output.size();
    while (i > 0 && std::iswspace(output[i - 1])) --i;
    if (i == 0 || std::wcschr(L"([{=,:;!?&|+-*%<>", output[i - 1]) != nullptr) return true;
    size_t end = i;
    while (i > 0 && CodeToolWordChar(output[i - 1])) --i;
    const std::wstring_view word(output.data() + i, end - i);
    for (const std::wstring_view keyword : {L"return", L"case", L"throw", L"typeof", L"void", L"delete", L"new", L"in", L"of", L"yield", L"await"}) {
        if (word == keyword) return true;
    }
    return false;
}

size_t JsRegexEnd(std::wstring_view input, size_t start) {
    bool escaped = false;
    bool characterClass = false;
    for (size_t i = start + 1; i < input.size(); ++i) {
        const wchar_t c = input[i];
        if (escaped) { escaped = false; continue; }
        if (c == L'\\') { escaped = true; continue; }
        if (c == L'[') { characterClass = true; continue; }
        if (c == L']') { characterClass = false; continue; }
        if (c == L'/' && !characterClass) {
            ++i;
            while (i < input.size() && std::iswalpha(input[i])) ++i;
            return i;
        }
        if (c == L'\r' || c == L'\n') break;
    }
    return start;
}

std::wstring FormatJavaScript(std::wstring_view input) {
    std::wstring output;
    output.reserve(input.size() + input.size() / 3);
    int indent = 0;
    int parentheses = 0;
    bool lineStart = true;
    bool pendingSpace = false;
    wchar_t quote = 0;
    for (size_t i = 0; i < input.size();) {
        const wchar_t c = input[i];
        if (quote != 0) {
            output += c; ++i;
            if (c == L'\\' && i < input.size()) output += input[i++];
            else if (c == quote) quote = 0;
            lineStart = false; continue;
        }
        if (c == L'\'' || c == L'\"' || c == L'`') {
            if (pendingSpace && !lineStart && !output.empty() && output.back() != L' ') output += L' ';
            pendingSpace = false; output += c; quote = c; lineStart = false; ++i; continue;
        }
        if (c == L'/' && i + 1 < input.size() && input[i + 1] == L'/') {
            if (pendingSpace && !lineStart) output += L' ';
            const size_t end = input.find_first_of(L"\r\n", i + 2);
            output.append(input.substr(i, end == std::wstring_view::npos ? input.size() - i : end - i));
            lineStart = false; CodeToolNewline(output, indent, lineStart); pendingSpace = false;
            i = end == std::wstring_view::npos ? input.size() : end; continue;
        }
        if (c == L'/' && i + 1 < input.size() && input[i + 1] == L'*') {
            const size_t end = input.find(L"*/", i + 2);
            const size_t length = end == std::wstring_view::npos ? input.size() - i : end + 2 - i;
            if (!lineStart) CodeToolNewline(output, indent, lineStart);
            output.append(input.substr(i, length)); lineStart = false;
            CodeToolNewline(output, indent, lineStart); pendingSpace = false; i += length; continue;
        }
        if (c == L'/' && JsLooksLikeRegexStart(output)) {
            const size_t end = JsRegexEnd(input, i);
            if (end > i) {
                if (pendingSpace && !lineStart && !output.empty() && output.back() != L' ') output += L' ';
                output.append(input.substr(i, end - i)); lineStart = false; pendingSpace = false; i = end; continue;
            }
        }
        if (std::iswspace(c)) { pendingSpace = true; ++i; continue; }
        if (c == L'{') {
            if (pendingSpace && !lineStart && !output.empty() && output.back() != L' ') output += L' ';
            output += L'{'; ++indent; CodeToolNewline(output, indent, lineStart); pendingSpace = false; ++i; continue;
        }
        if (c == L'}') {
            CodeToolTrimLine(output);
            if (!lineStart) CodeToolNewline(output, std::max(0, indent - 1), lineStart);
            indent = std::max(0, indent - 1); output += L'}'; lineStart = false;
            size_t next = CodeToolSkipSpace(input, i + 1);
            bool continuation = next < input.size() && (input[next] == L';' || input[next] == L',' || input[next] == L')' || input[next] == L']' || input[next] == L'.');
            if (!continuation && next < input.size()) {
                for (const wchar_t* word : {L"else", L"catch", L"finally"}) {
                    const size_t n = std::wcslen(word);
                    if (next + n <= input.size() && input.substr(next, n) == word) { continuation = true; break; }
                }
            }
            if (continuation) output += L' '; else CodeToolNewline(output, indent, lineStart);
            pendingSpace = false; ++i; continue;
        }
        if (c == L';') {
            CodeToolTrimLine(output); output += L';';
            if (parentheses == 0) CodeToolNewline(output, indent, lineStart); else output += L' ';
            pendingSpace = false; ++i; continue;
        }
        if (c == L'(') {
            if (JsKeywordBeforeParen(output)) output += L' ';
            output += c; ++parentheses; lineStart = false; pendingSpace = false; ++i; continue;
        }
        if (c == L')') { CodeToolTrimLine(output); output += c; parentheses = std::max(0, parentheses - 1); lineStart = false; pendingSpace = false; ++i; continue; }
        if (c == L',') { CodeToolTrimLine(output); output += L", "; lineStart = false; pendingSpace = false; ++i; continue; }
        if (c == L':') { CodeToolTrimLine(output); output += L": "; lineStart = false; pendingSpace = false; ++i; continue; }
        if (c == L'.') { CodeToolTrimLine(output); output += c; lineStart = false; pendingSpace = false; ++i; continue; }
        if (c == L'!' || c == L'~') { if (pendingSpace && !JsUnaryOperator(output)) output += L' '; output += c; lineStart = false; pendingSpace = false; ++i; continue; }
        if (c == L'+' || c == L'-') {
            if (i + 1 < input.size() && input[i + 1] == c) { CodeToolTrimLine(output); output += c; output += c; i += 2; lineStart = false; pendingSpace = false; continue; }
            if (JsUnaryOperator(output)) { CodeToolTrimLine(output); output += c; }
            else { CodeToolTrimLine(output); output += L' '; output += c; output += L' '; }
            lineStart = false; pendingSpace = false; ++i; continue;
        }
        const wchar_t* op = nullptr;
        for (const wchar_t* candidate : {L"===", L"!==", L">>>", L"**=", L"=>", L"==", L"!=", L"<=", L">=", L"&&", L"||", L"??", L"+=", L"-=", L"*=", L"/=", L"%=", L"**", L"<<", L">>"}) {
            const size_t n = std::wcslen(candidate);
            if (i + n <= input.size() && input.substr(i, n) == candidate) { op = candidate; break; }
        }
        if (op != nullptr) {
            CodeToolTrimLine(output); output += L' '; output += op; output += L' '; lineStart = false; pendingSpace = false; i += std::wcslen(op); continue;
        }
        if (std::wcschr(L"=*/%<>|&^", c) != nullptr) {
            CodeToolTrimLine(output); output += L' '; output += c; output += L' '; lineStart = false; pendingSpace = false; ++i; continue;
        }
        if (pendingSpace && !lineStart && !output.empty() && output.back() != L' ' && CodeToolNeedsSpace(output.back(), c)) output += L' ';
        output += c; lineStart = false; pendingSpace = false; ++i;
    }
    CodeToolTrimLine(output);
    while (output.size() >= 2 && output.compare(output.size() - 2, 2, L"\r\n") == 0) output.resize(output.size() - 2);
    return output;
}

std::wstring MinifyJavaScript(std::wstring_view input) {
    std::wstring output;
    output.reserve(input.size());
    bool pendingSpace = false;
    wchar_t quote = 0;
    for (size_t i = 0; i < input.size();) {
        const wchar_t c = input[i];
        if (quote != 0) {
            output += c; ++i;
            if (c == L'\\' && i < input.size()) output += input[i++];
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == L'\'' || c == L'\"' || c == L'`') { if (pendingSpace && !output.empty() && CodeToolWordChar(output.back())) output += L' '; pendingSpace = false; output += c; quote = c; ++i; continue; }
        if (c == L'/' && i + 1 < input.size() && input[i + 1] == L'/') {
            const size_t end = input.find_first_of(L"\r\n", i + 2); i = end == std::wstring_view::npos ? input.size() : end; pendingSpace = true; continue;
        }
        if (c == L'/' && i + 1 < input.size() && input[i + 1] == L'*') {
            const size_t end = input.find(L"*/", i + 2);
            const size_t length = end == std::wstring_view::npos ? input.size() - i : end + 2 - i;
            const bool keep = i + 2 < input.size() && input[i + 2] == L'!';
            if (keep) { if (pendingSpace && !output.empty()) output += L' '; output.append(input.substr(i, length)); }
            i += length; pendingSpace = true; continue;
        }
        if (std::iswspace(c)) { pendingSpace = true; ++i; continue; }
        const bool punctuation = c == L'{' || c == L'}' || c == L'(' || c == L')' || c == L'[' || c == L']' || c == L';' || c == L',' || c == L':' || c == L'.';
        if (pendingSpace && !output.empty() && !punctuation && CodeToolNeedsSpace(output.back(), c)) output += L' ';
        pendingSpace = false;
        if (c == L';') { size_t next = CodeToolSkipSpace(input, i + 1); if (next < input.size() && input[next] == L'}') { ++i; continue; } }
        output += c; ++i;
    }
    while (!output.empty() && std::iswspace(output.back())) output.pop_back();
    return output;
}

bool LoadTextAsset(const fs::path& path, std::string& output) {
    std::vector<char> bytes;
    if (!ReadBinaryFile(path, bytes)) return false;
    output.assign(bytes.begin(), bytes.end());
    return true;
}

bool ShowdownConvert(std::wstring_view input, bool markdownToHtml, std::wstring& output, std::wstring& error) {
    if (input.size() > 8u * 1024u * 1024u) {
        error = L"为避免转换过程占用过多内存，单次处理限制为 8 MB。";
        return false;
    }
    static std::string showdownScript;
    static std::string domScript;
    static bool assetsLoaded = false;
    if (!assetsLoaded) {
        const fs::path root = fs::path(CurrentExecutablePath()).parent_path() / L"resources";
        assetsLoaded = LoadTextAsset(root / L"showdown.js", showdownScript) &&
            LoadTextAsset(root / L"showdown-dom.js", domScript);
    }
    if (!assetsLoaded) {
        error = L"缺少 Showdown 运行资源，请确认 resources/showdown.js 和 resources/showdown-dom.js 存在。";
        return false;
    }
    duk_context* ctx = duk_create_heap_default();
    if (!ctx) { error = L"无法创建 JavaScript 运行环境。"; return false; }
    auto fail = [&](const char* message) {
        error = Utf8ToWide(message ? message : "JavaScript 执行失败");
        duk_destroy_heap(ctx);
        return false;
    };
    duk_push_lstring(ctx, showdownScript.data(), showdownScript.size());
    if (duk_peval(ctx) != 0) return fail(duk_safe_to_string(ctx, -1));
    duk_pop(ctx);
    if (!markdownToHtml) {
        duk_push_lstring(ctx, domScript.data(), domScript.size());
        if (duk_peval(ctx) != 0) return fail(duk_safe_to_string(ctx, -1));
        duk_pop(ctx);
    }
    duk_get_global_string(ctx, "showdown");
    if (!duk_is_object(ctx, -1)) return fail("Showdown 初始化失败");
    duk_get_prop_string(ctx, -1, "Converter");
    if (!duk_is_function(ctx, -1)) return fail("Showdown Converter 不可用");
    if (markdownToHtml) {
        duk_push_object(ctx);
        duk_push_true(ctx); duk_put_prop_string(ctx, -2, "tables");
        duk_push_true(ctx); duk_put_prop_string(ctx, -2, "strikethrough");
        duk_push_true(ctx); duk_put_prop_string(ctx, -2, "tasklists");
        duk_push_true(ctx); duk_put_prop_string(ctx, -2, "ghCodeBlocks");
        duk_push_true(ctx); duk_put_prop_string(ctx, -2, "simplifiedAutoLink");
        duk_new(ctx, 1);
    } else {
        duk_new(ctx, 0);
    }
    const char* method = markdownToHtml ? "makeHtml" : "makeMarkdown";
    duk_get_prop_string(ctx, -1, method);
    if (!duk_is_function(ctx, -1)) return fail("Showdown 转换方法不可用");
    duk_dup(ctx, -2);  // this = Converter instance
    const std::string utf8 = WideToUtf8(input);
    duk_push_lstring(ctx, utf8.data(), utf8.size());
    if (!markdownToHtml) duk_get_global_string(ctx, "MCShowdownDOM");
    if (duk_pcall_method(ctx, markdownToHtml ? 1 : 2) != 0) return fail(duk_safe_to_string(ctx, -1));
    if (!duk_is_string(ctx, -1)) return fail("Showdown 返回了非文本结果");
    output = Utf8ToWide(duk_get_string(ctx, -1));
    if (!markdownToHtml) {
        // The DOM adapter has no concept of an empty comment node.  Showdown
        // may emit one while normalizing a fragment; it carries no content.
        for (size_t pos = 0; (pos = output.find(L"<!-- -->", pos)) != std::wstring::npos;) output.erase(pos, 8);
    }
    duk_destroy_heap(ctx);
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
    const std::wstring raw = GetEditorRawText(document->editor);
    CHARRANGE range{0, static_cast<LONG>(raw.size())};
    if (!raw.empty() && raw.back() == L'\r') --range.cpMax;
    ApplyToolResult(*document, range, formatted, g_app.toolOutputMode, L"格式化 JSON", L"已格式化 JSON");
    if (TabDocument* active = ActiveDocument()) HighlightDocument(*active);
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

// ===========================================================================
// Input dialog, tab rename, text tools and Chinese -> pinyin.
// ===========================================================================

// ---- Small modal input dialog (1-2 text fields) ------------------------------

struct PromptChoice {
    std::wstring label;
    std::vector<std::wstring> options;
    int selected = 0;
};

struct PromptState {
    std::vector<PromptChoice>* choices = nullptr;
    std::vector<HWND> combos;
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
        if (state->choices) {
            for (const PromptChoice& choice : *state->choices) {
                HWND label = CreateWindowExW(0, L"STATIC", choice.label.c_str(), WS_CHILD | WS_VISIBLE,
                    Scale(16), Scale(y), Scale(340), Scale(20), window, nullptr, g_app.instance, nullptr);
                HWND combo = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                    Scale(16), Scale(y + 22), Scale(340), Scale(200), window, nullptr, g_app.instance, nullptr);
                for (const std::wstring& option : choice.options) SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(option.c_str()));
                SendMessageW(combo, CB_SETCURSEL, std::clamp(choice.selected, 0, static_cast<int>(choice.options.size()) - 1), 0);
                for (HWND control : {label, combo}) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
                if (g_app.darkTheme) SetWindowTheme(combo, L"DarkMode_CFD", nullptr);
                state->combos.push_back(combo);
                y += 58;
            }
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
                for (size_t i = 0; i < state->combos.size(); ++i) {
                    (*state->choices)[i].selected = static_cast<int>(SendMessageW(state->combos[i], CB_GETCURSEL, 0, 0));
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
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, CurrentPalette().controlText);
        SetBkColor(dc, CurrentPalette().controlBackground);
        return reinterpret_cast<LRESULT>(g_app.controlBrush);
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

void ThemePushButtons(std::initializer_list<HWND> buttons) {
    for (HWND button : buttons) {
        if (button && g_app.darkTheme) SetWindowTheme(button, L"DarkMode_Explorer", nullptr);
    }
}

bool PromptForText(const wchar_t* title, const std::vector<std::wstring>& labels, std::vector<std::wstring>& values,
    std::vector<std::pair<std::wstring, bool>>* checks = nullptr, std::vector<PromptChoice>* choices = nullptr) {
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
    state.choices = choices;
    RECT owner{};
    GetWindowRect(g_app.window, &owner);
    const int checkHeight = checks ? 28 * static_cast<int>(checks->size()) : 0;
    const int choiceHeight = choices ? 58 * static_cast<int>(choices->size()) : 0;
    RECT frame{0, 0, Scale(372), Scale(16 + 58 * static_cast<int>(labels.size()) + choiceHeight + checkHeight + 48)};
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
    } else if (!state.combos.empty()) {
        SetFocus(state.combos[0]);
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

// ---- Prefix/suffix batch processor -------------------------------------------

enum class PrefixSuffixOperation { Add = 0, Remove = 1, Replace = 2, FixedDelete = 3 };
enum class PrefixSuffixTarget { Prefix = 0, Suffix = 1, Both = 2 };
enum class PrefixSuffixScope { Selection = 0, CurrentDocument = 1, AllDocuments = 2 };

struct PrefixSuffixRule {
    PrefixSuffixOperation operation = PrefixSuffixOperation::Add;
    PrefixSuffixTarget target = PrefixSuffixTarget::Prefix;
    PrefixSuffixScope scope = PrefixSuffixScope::Selection;
    std::wstring prefix;
    std::wstring suffix;
    std::wstring replacement;
    int fixedLength = 1;
    bool regex = false;
    bool matchCase = false;
    bool skipBlank = true;
    bool skipExisting = true;
    bool trimWhitespace = true;
};

struct PrefixSuffixResult {
    std::wstring text;
    int totalLines = 0;
    int changedLines = 0;
    int skippedLines = 0;
};

constexpr size_t kPrefixSuffixHistoryLimit = 100;
std::vector<PrefixSuffixRule> g_recentPrefixSuffixRules;
bool g_prefixSuffixHistoryLoaded = false;
void SavePrefixSuffixHistory();

bool PrefixSuffixEqual(std::wstring_view a, std::wstring_view b, bool matchCase) {
    if (a.size() != b.size()) return false;
    if (matchCase) return a == b;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::towlower(a[i]) != std::towlower(b[i])) return false;
    }
    return true;
}

struct PrefixSuffixMatch {
    bool found = false;
    size_t start = 0;
    size_t length = 0;
};

bool PrefixSuffixRegexSearch(std::wstring_view line, std::wstring_view pattern, bool matchCase,
    bool prefix, PrefixSuffixMatch& result, std::wstring& error) {
    try {
        const auto flags = std::regex_constants::ECMAScript |
            (matchCase ? std::regex_constants::syntax_option_type{} : std::regex_constants::icase);
        const std::wregex expression(std::wstring(pattern), flags);
        const std::wstring value(line);
        std::wsmatch match;
        if (prefix) {
            if (!std::regex_search(value.cbegin(), value.cend(), match, expression, std::regex_constants::match_continuous)) return true;
            result = {true, static_cast<size_t>(match.position()), static_cast<size_t>(match.length())};
        } else {
            if (!std::regex_search(value.cbegin(), value.cend(), match, expression)) return true;
            const size_t start = static_cast<size_t>(match.position());
            const size_t length = static_cast<size_t>(match.length());
            if (start + length != line.size()) return true;
            result = {true, start, length};
        }
        return true;
    } catch (const std::regex_error& ex) {
        error = L"正则表达式无效（错误代码 " + std::to_wstring(static_cast<int>(ex.code())) + L"）。";
        return false;
    }
}

bool PrefixSuffixFind(std::wstring_view line, std::wstring_view pattern, bool regex, bool matchCase,
    bool prefix, size_t searchStart, size_t searchEnd, PrefixSuffixMatch& result, std::wstring& error) {
    if (pattern.empty()) return true;
    const std::wstring_view part = line.substr(searchStart, searchEnd - searchStart);
    if (regex) {
        PrefixSuffixMatch local;
        if (!PrefixSuffixRegexSearch(part, pattern, matchCase, prefix, local, error)) return false;
        if (local.found) result = {true, searchStart + local.start, local.length};
        return true;
    }
    if (prefix) {
        if (part.size() >= pattern.size() && PrefixSuffixEqual(part.substr(0, pattern.size()), pattern, matchCase)) {
            result = {true, searchStart, pattern.size()};
        }
    } else if (part.size() >= pattern.size() &&
               PrefixSuffixEqual(part.substr(part.size() - pattern.size()), pattern, matchCase)) {
        result = {true, searchEnd - pattern.size(), pattern.size()};
    }
    return true;
}

bool PrefixSuffixApplyLine(const PrefixSuffixRule& rule, std::wstring& line, bool& changed, std::wstring& error) {
    changed = false;
    const size_t contentStart = rule.trimWhitespace ? line.find_first_not_of(L" \t\u3000") : 0;
    const size_t contentEnd = rule.trimWhitespace ? line.find_last_not_of(L" \t\u3000") + 1 : line.size();
    if (contentStart == std::wstring::npos || contentStart >= contentEnd) return true;

    PrefixSuffixMatch prefixMatch, suffixMatch;
    const bool needPrefix = rule.target == PrefixSuffixTarget::Prefix || rule.target == PrefixSuffixTarget::Both;
    const bool needSuffix = rule.target == PrefixSuffixTarget::Suffix || rule.target == PrefixSuffixTarget::Both;
    if (needPrefix && !PrefixSuffixFind(line, rule.prefix, rule.regex, rule.matchCase, true,
        contentStart, contentEnd, prefixMatch, error)) return false;
    if (needSuffix && !PrefixSuffixFind(line, rule.suffix, rule.regex, rule.matchCase, false,
        contentStart, contentEnd, suffixMatch, error)) return false;

    if (rule.operation == PrefixSuffixOperation::Add && rule.skipExisting &&
        ((needPrefix && prefixMatch.found) || (needSuffix && suffixMatch.found))) return true;

    if (rule.operation == PrefixSuffixOperation::Add) {
        if (needSuffix) line.insert(contentEnd, rule.suffix);
        if (needPrefix) line.insert(contentStart, rule.prefix);
        changed = needPrefix || needSuffix;
        return true;
    }
    if (rule.operation == PrefixSuffixOperation::FixedDelete) {
        const size_t count = static_cast<size_t>(std::max(0, rule.fixedLength));
        if (count == 0) return true;
        if (needSuffix) {
            const size_t length = std::min(count, contentEnd - contentStart);
            if (length > 0) { line.erase(contentEnd - length, length); changed = true; }
        }
        if (needPrefix) {
            const size_t length = std::min(count, line.size() - contentStart);
            if (length > 0) { line.erase(contentStart, length); changed = true; }
        }
        return true;
    }

    const auto replacementText = [&](std::wstring_view pattern, size_t start, size_t length) -> std::wstring {
        if (!rule.regex) return rule.replacement;
        try {
            const auto flags = std::regex_constants::ECMAScript |
                (rule.matchCase ? std::regex_constants::syntax_option_type{} : std::regex_constants::icase);
            const std::wregex expression(std::wstring(pattern), flags);
            const std::wstring segment = line.substr(start, length);
            return std::regex_replace(segment, expression, rule.replacement,
                std::regex_constants::format_default | std::regex_constants::format_first_only);
        } catch (const std::regex_error& ex) {
            error = L"正则表达式无效（错误代码 " + std::to_wstring(static_cast<int>(ex.code())) + L"）。";
            return {};
        }
    };
    if (needSuffix && suffixMatch.found) {
        if (rule.operation == PrefixSuffixOperation::Remove) line.erase(suffixMatch.start, suffixMatch.length);
        else {
            const std::wstring replacement = replacementText(rule.suffix, suffixMatch.start, suffixMatch.length);
            if (!error.empty()) return false;
            line.replace(suffixMatch.start, suffixMatch.length, replacement);
        }
        changed = true;
    }
    if (needPrefix && prefixMatch.found) {
        if (rule.operation == PrefixSuffixOperation::Remove) line.erase(prefixMatch.start, prefixMatch.length);
        else {
            const std::wstring replacement = replacementText(rule.prefix, prefixMatch.start, prefixMatch.length);
            if (!error.empty()) return false;
            line.replace(prefixMatch.start, prefixMatch.length, replacement);
        }
        changed = true;
    }
    return true;
}

PrefixSuffixResult TransformPrefixSuffix(const std::wstring& text, const PrefixSuffixRule& rule, std::wstring& error) {
    PrefixSuffixResult result;
    std::vector<std::wstring> lines = SplitLines(text);
    result.totalLines = static_cast<int>(lines.size());
    for (std::wstring& line : lines) {
        if (rule.skipBlank && TrimCopy(line).empty()) { ++result.skippedLines; continue; }
        const std::wstring before = line;
        bool changed = false;
        if (!PrefixSuffixApplyLine(rule, line, changed, error)) return {};
        if (changed && line != before) ++result.changedLines;
        else ++result.skippedLines;
    }
    result.text = JoinLines(lines, L"\r");
    return result;
}

void RememberPrefixSuffixRule(const PrefixSuffixRule& rule) {
    auto same = [&](const PrefixSuffixRule& other) {
        return rule.operation == other.operation && rule.target == other.target && rule.scope == other.scope &&
            rule.prefix == other.prefix && rule.suffix == other.suffix && rule.replacement == other.replacement &&
            rule.fixedLength == other.fixedLength && rule.regex == other.regex && rule.matchCase == other.matchCase &&
            rule.skipBlank == other.skipBlank && rule.skipExisting == other.skipExisting &&
            rule.trimWhitespace == other.trimWhitespace;
    };
    std::erase_if(g_recentPrefixSuffixRules, same);
    g_recentPrefixSuffixRules.insert(g_recentPrefixSuffixRules.begin(), rule);
    if (g_recentPrefixSuffixRules.size() > kPrefixSuffixHistoryLimit) g_recentPrefixSuffixRules.resize(kPrefixSuffixHistoryLimit);
    SavePrefixSuffixHistory();
}

const wchar_t* PrefixSuffixOperationName(PrefixSuffixOperation operation) {
    switch (operation) {
    case PrefixSuffixOperation::Add: return L"添加";
    case PrefixSuffixOperation::Remove: return L"删除";
    case PrefixSuffixOperation::Replace: return L"替换";
    case PrefixSuffixOperation::FixedDelete: return L"定长删除";
    default: return L"操作";
    }
}

const wchar_t* PrefixSuffixTargetName(PrefixSuffixTarget target) {
    switch (target) {
    case PrefixSuffixTarget::Prefix: return L"前缀";
    case PrefixSuffixTarget::Suffix: return L"后缀";
    case PrefixSuffixTarget::Both: return L"前缀+后缀";
    default: return L"目标";
    }
}

std::wstring PrefixSuffixShortText(std::wstring_view value) {
    constexpr size_t kMax = 22;
    if (value.size() <= kMax) return std::wstring(value);
    return std::wstring(value.substr(0, kMax)) + L"…";
}

std::wstring PrefixSuffixRuleLabel(const PrefixSuffixRule& rule) {
    std::wstring label = std::wstring(PrefixSuffixOperationName(rule.operation)) + L" · " +
        PrefixSuffixTargetName(rule.target);
    if (!rule.prefix.empty()) label += L" · 前缀: " + PrefixSuffixShortText(rule.prefix);
    if (!rule.suffix.empty()) label += L" · 后缀: " + PrefixSuffixShortText(rule.suffix);
    if (rule.operation == PrefixSuffixOperation::Replace && !rule.replacement.empty()) {
        label += L" → " + PrefixSuffixShortText(rule.replacement);
    }
    if (rule.operation == PrefixSuffixOperation::FixedDelete) {
        label += L" · " + std::to_wstring(rule.fixedLength) + L" 字符";
    }
    return label;
}

fs::path PrefixSuffixHistoryPath() {
    return g_app.dataDirectory / L"prefix-suffix-history.dat";
}

void SavePrefixSuffixHistory() {
    std::ostringstream output;
    output << "MCPREFIXHISTORY\t1\n";
    for (const PrefixSuffixRule& rule : g_recentPrefixSuffixRules) {
        output << static_cast<int>(rule.operation) << '\t'
               << static_cast<int>(rule.target) << '\t'
               << static_cast<int>(rule.scope) << '\t'
               << rule.fixedLength << '\t'
               << (rule.regex ? 1 : 0) << '\t'
               << (rule.matchCase ? 1 : 0) << '\t'
               << (rule.skipBlank ? 1 : 0) << '\t'
               << (rule.skipExisting ? 1 : 0) << '\t'
               << (rule.trimWhitespace ? 1 : 0) << '\t'
               << HexEncode(WideToUtf8(rule.prefix)) << '\t'
               << HexEncode(WideToUtf8(rule.suffix)) << '\t'
               << HexEncode(WideToUtf8(rule.replacement)) << '\n';
    }
    WriteAtomicFile(PrefixSuffixHistoryPath(), output.str());
}

void LoadPrefixSuffixHistory() {
    if (g_prefixSuffixHistoryLoaded) return;
    g_prefixSuffixHistoryLoaded = true;
    std::ifstream input(PrefixSuffixHistoryPath(), std::ios::binary);
    if (!input) return;
    std::string line;
    while (std::getline(input, line) && g_recentPrefixSuffixRules.size() < kPrefixSuffixHistoryLimit) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::vector<std::string> parts = SplitTabs(line);
        if (parts.size() < 12 || parts[0] == "MCPREFIXHISTORY") continue;
        PrefixSuffixRule rule;
        int operation = 0, target = 0, scope = 0, fixedLength = 1;
        int regex = 0, matchCase = 0, skipBlank = 1, skipExisting = 1, trimWhitespace = 1;
        if (!ParseInt(parts[0], operation) || !ParseInt(parts[1], target) || !ParseInt(parts[2], scope) ||
            !ParseInt(parts[3], fixedLength) || !ParseInt(parts[4], regex) || !ParseInt(parts[5], matchCase) ||
            !ParseInt(parts[6], skipBlank) || !ParseInt(parts[7], skipExisting) || !ParseInt(parts[8], trimWhitespace)) continue;
        rule.operation = static_cast<PrefixSuffixOperation>(std::clamp(operation, 0, 3));
        rule.target = static_cast<PrefixSuffixTarget>(std::clamp(target, 0, 2));
        rule.scope = static_cast<PrefixSuffixScope>(std::clamp(scope, 0, 2));
        rule.fixedLength = std::max(0, fixedLength);
        rule.regex = regex != 0;
        rule.matchCase = matchCase != 0;
        rule.skipBlank = skipBlank != 0;
        rule.skipExisting = skipExisting != 0;
        rule.trimWhitespace = trimWhitespace != 0;
        rule.prefix = Utf8ToWide(HexDecode(parts[9]));
        rule.suffix = Utf8ToWide(HexDecode(parts[10]));
        rule.replacement = Utf8ToWide(HexDecode(parts[11]));
        g_recentPrefixSuffixRules.push_back(std::move(rule));
    }
}

struct PrefixSuffixDialogState {
    PrefixSuffixRule rule;
    HWND operation = nullptr, target = nullptr, scope = nullptr, history = nullptr;
    HWND prefix = nullptr, suffix = nullptr, replacement = nullptr, fixedLength = nullptr;
    HWND regex = nullptr, matchCase = nullptr, skipBlank = nullptr, skipExisting = nullptr, trimWhitespace = nullptr;
    HWND summary = nullptr, addHistory = nullptr, deleteHistory = nullptr;
    bool done = false;
    bool accepted = false;
    bool populating = false;
};

enum : int {
    kPsOperation = 50101, kPsTarget, kPsScope, kPsHistory, kPsPrefix, kPsSuffix, kPsReplacement, kPsFixedLength,
    kPsRegex, kPsMatchCase, kPsSkipBlank, kPsSkipExisting, kPsTrimWhitespace, kPsSummary, kPsAddHistory, kPsDeleteHistory,
};

void PrefixSuffixSetControlText(HWND control, const std::wstring& value) {
    SetWindowTextW(control, value.c_str());
}

void PrefixSuffixLoadControls(PrefixSuffixDialogState& state) {
    state.populating = true;
    SendMessageW(state.operation, CB_SETCURSEL, static_cast<WPARAM>(state.rule.operation), 0);
    SendMessageW(state.target, CB_SETCURSEL, static_cast<WPARAM>(state.rule.target), 0);
    SendMessageW(state.scope, CB_SETCURSEL, static_cast<WPARAM>(state.rule.scope), 0);
    PrefixSuffixSetControlText(state.prefix, state.rule.prefix);
    PrefixSuffixSetControlText(state.suffix, state.rule.suffix);
    PrefixSuffixSetControlText(state.replacement, state.rule.replacement);
    PrefixSuffixSetControlText(state.fixedLength, std::to_wstring(state.rule.fixedLength));
    SendMessageW(state.regex, BM_SETCHECK, state.rule.regex ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.matchCase, BM_SETCHECK, state.rule.matchCase ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.skipBlank, BM_SETCHECK, state.rule.skipBlank ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.skipExisting, BM_SETCHECK, state.rule.skipExisting ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.trimWhitespace, BM_SETCHECK, state.rule.trimWhitespace ? BST_CHECKED : BST_UNCHECKED, 0);
    state.populating = false;
}

void PrefixSuffixReadControls(PrefixSuffixDialogState& state) {
    state.rule.operation = static_cast<PrefixSuffixOperation>(SendMessageW(state.operation, CB_GETCURSEL, 0, 0));
    state.rule.target = static_cast<PrefixSuffixTarget>(SendMessageW(state.target, CB_GETCURSEL, 0, 0));
    state.rule.scope = static_cast<PrefixSuffixScope>(SendMessageW(state.scope, CB_GETCURSEL, 0, 0));
    state.rule.prefix = GetWindowTextValue(state.prefix);
    state.rule.suffix = GetWindowTextValue(state.suffix);
    state.rule.replacement = GetWindowTextValue(state.replacement);
    state.rule.fixedLength = std::max(0, _wtoi(GetWindowTextValue(state.fixedLength).c_str()));
    state.rule.regex = SendMessageW(state.regex, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.rule.matchCase = SendMessageW(state.matchCase, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.rule.skipBlank = SendMessageW(state.skipBlank, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.rule.skipExisting = SendMessageW(state.skipExisting, BM_GETCHECK, 0, 0) == BST_CHECKED;
    state.rule.trimWhitespace = SendMessageW(state.trimWhitespace, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void PrefixSuffixRefreshHistoryList(PrefixSuffixDialogState& state) {
    if (!state.history) return;
    SendMessageW(state.history, LB_RESETCONTENT, 0, 0);
    for (const PrefixSuffixRule& rule : g_recentPrefixSuffixRules) {
        const std::wstring label = PrefixSuffixRuleLabel(rule);
        SendMessageW(state.history, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
}

bool PrefixSuffixSourceForDocument(int index, const PrefixSuffixRule& rule, CHARRANGE& range, std::wstring& text) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) return false;
    HWND editor = g_app.documents[static_cast<size_t>(index)].editor;
    if (rule.scope == PrefixSuffixScope::Selection && index == g_app.activeIndex) {
        bool wholeDocument = false;
        ToolRange(editor, true, range, text, wholeDocument);
        return true;
    }
    const std::wstring all = GetEditorRawText(editor);
    range = CHARRANGE{0, static_cast<LONG>(all.size())};
    if (range.cpMax > 0 && all.back() == L'\r') --range.cpMax;
    text = all.substr(0, static_cast<size_t>(range.cpMax));
    return true;
}

PrefixSuffixResult BuildPrefixSuffixPreview(const PrefixSuffixRule& rule, std::wstring& original, std::wstring& error) {
    PrefixSuffixResult aggregate;
    original.clear();
    const int active = g_app.activeIndex;
    if (active < 0 || g_app.documents.empty()) return aggregate;
    if (rule.scope == PrefixSuffixScope::AllDocuments) {
        for (int index = 0; index < static_cast<int>(g_app.documents.size()); ++index) {
            CHARRANGE range{}; std::wstring source;
            if (!PrefixSuffixSourceForDocument(index, rule, range, source)) continue;
            PrefixSuffixResult item = TransformPrefixSuffix(source, rule, error);
            if (!error.empty()) return {};
            aggregate.totalLines += item.totalLines;
            aggregate.changedLines += item.changedLines;
            aggregate.skippedLines += item.skippedLines;
            if (index == active) { original = std::move(source); aggregate.text = std::move(item.text); }
        }
    } else {
        CHARRANGE range{}; std::wstring source;
        PrefixSuffixSourceForDocument(active, rule, range, source);
        original = source;
        aggregate = TransformPrefixSuffix(source, rule, error);
    }
    return aggregate;
}

void PrefixSuffixRefreshPreview(HWND window, PrefixSuffixDialogState& state) {
    if (state.populating) return;
    PrefixSuffixReadControls(state);
    std::wstring original, error;
    const PrefixSuffixResult result = BuildPrefixSuffixPreview(state.rule, original, error);
    const std::wstring summary = error.empty()
        ? L"总行数：" + std::to_wstring(result.totalLines) + L"    修改：" + std::to_wstring(result.changedLines) +
          L"    跳过：" + std::to_wstring(result.skippedLines)
        : error;
    SetWindowTextW(state.summary, summary.c_str());
    const bool replacement = state.rule.operation == PrefixSuffixOperation::Replace;
    const bool fixed = state.rule.operation == PrefixSuffixOperation::FixedDelete;
    EnableWindow(state.replacement, replacement);
    EnableWindow(state.fixedLength, fixed);
    EnableWindow(state.skipExisting, state.rule.operation == PrefixSuffixOperation::Add);
    (void)window;
}

void PrefixSuffixApply(PrefixSuffixDialogState& state) {
    std::wstring original, error;
    const PrefixSuffixResult preview = BuildPrefixSuffixPreview(state.rule, original, error);
    if (!error.empty()) { MessageBoxW(g_app.window, error.c_str(), kWindowTitle, MB_ICONWARNING); return; }
    if (preview.changedLines == 0) { ShowNotice(L"没有需要修改的行"); return; }
    RememberPrefixSuffixRule(state.rule);
    const int oldActive = g_app.activeIndex;
    g_app.loading = true;
    int changedDocuments = 0;
    for (int index = 0; index < static_cast<int>(g_app.documents.size()); ++index) {
        if (state.rule.scope != PrefixSuffixScope::AllDocuments && index != oldActive) continue;
        CHARRANGE range{}; std::wstring source;
        PrefixSuffixSourceForDocument(index, state.rule, range, source);
        PrefixSuffixResult item = TransformPrefixSuffix(source, state.rule, error);
        if (!error.empty()) break;
        if (item.text == source) continue;
        TabDocument& document = g_app.documents[static_cast<size_t>(index)];
        SendMessageW(document.editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
        SendMessageW(document.editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(item.text.c_str()));
        document.textHash = HashText(GetEditorRawText(document.editor));
        document.modified = true;
        document.syntaxStale = true;
        MarkRecoveryDirty(index);
        UpdateTabTitle(index);
        ++changedDocuments;
    }
    g_app.loading = false;
    if (!error.empty()) { MessageBoxW(g_app.window, error.c_str(), kWindowTitle, MB_ICONWARNING); return; }
    RefreshGutter();
    UpdateStatusBar();
    SetMenuState();
    ShowNotice(L"已处理 " + std::to_wstring(preview.changedLines) + L" 行（" + std::to_wstring(changedDocuments) + L" 个标签）");
}

LRESULT CALLBACK PrefixSuffixDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<PrefixSuffixDialogState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        state = static_cast<PrefixSuffixDialogState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        auto label = [&](const wchar_t* text, int x, int y, int width = 150) {
            HWND control = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, Scale(x), Scale(y), Scale(width), Scale(22), window, nullptr, g_app.instance, nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE); return control;
        };
        auto combo = [&](int id, int x, int y, int width) {
            HWND control = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                Scale(x), Scale(y), Scale(width), Scale(180), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app.instance, nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE); if (g_app.darkTheme) SetWindowTheme(control, L"DarkMode_CFD", nullptr); return control;
        };
        auto edit = [&](int id, int x, int y, int width, DWORD style = ES_AUTOHSCROLL) {
            HWND control = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                Scale(x), Scale(y), Scale(width), Scale(26), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app.instance, nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            SetWindowTheme(control, g_app.darkTheme ? L"DarkMode_CFD" : L"Explorer", nullptr);
            return control;
        };
        label(L"操作类型", 16, 14); state->operation = combo(kPsOperation, 16, 36, 170);
        for (const wchar_t* item : {L"添加", L"删除", L"替换", L"定长删除"}) SendMessageW(state->operation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        label(L"处理目标", 202, 14); state->target = combo(kPsTarget, 202, 36, 170);
        for (const wchar_t* item : {L"前缀（行首）", L"后缀（行尾）", L"前缀和后缀"}) SendMessageW(state->target, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        label(L"处理范围", 388, 14); state->scope = combo(kPsScope, 388, 36, 230);
        for (const wchar_t* item : {L"当前选中文本（按整行）", L"当前文档", L"所有已打开文档"}) SendMessageW(state->scope, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        label(L"前缀内容", 16, 140); state->prefix = edit(kPsPrefix, 16, 162, 290);
        label(L"后缀内容", 326, 140); state->suffix = edit(kPsSuffix, 326, 162, 292);
        label(L"替换内容（替换模式）", 16, 202); state->replacement = edit(kPsReplacement, 16, 224, 290);
        label(L"定长删除数量", 326, 202); state->fixedLength = edit(kPsFixedLength, 326, 224, 120);
        const auto check = [&](int id, const wchar_t* text, int x, int y, bool checked) {
            HWND control = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                Scale(x), Scale(y), Scale(280), Scale(24), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app.instance, nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE); SendMessageW(control, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0); ThemeCheckbox(control); return control;
        };
        state->regex = check(kPsRegex, L"使用正则表达式匹配", 16, 266, false);
        state->matchCase = check(kPsMatchCase, L"区分大小写", 206, 266, false);
        state->skipBlank = check(kPsSkipBlank, L"跳过空白行", 326, 266, true);
        state->skipExisting = check(kPsSkipExisting, L"跳过已有相同前缀/后缀", 16, 294, true);
        state->trimWhitespace = check(kPsTrimWhitespace, L"忽略行首、行尾空白进行匹配", 326, 294, true);
        label(L"执行前统计", 16, 334, 300); state->summary = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE,
            Scale(16), Scale(358), Scale(600), Scale(24), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPsSummary)), g_app.instance, nullptr);
        SendMessageW(state->summary, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        HWND ok = CreateWindowExW(0, L"BUTTON", L"执行处理", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            Scale(566), Scale(400), Scale(92), Scale(30), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), g_app.instance, nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(666), Scale(400), Scale(92), Scale(30), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), g_app.instance, nullptr);
        ThemePushButtons({ok, cancel}); SendMessageW(ok, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE); SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        label(L"历史操作（最近100条）", 790, 14, 230);
        state->history = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            Scale(790), Scale(36), Scale(320), Scale(354), window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPsHistory)), g_app.instance, nullptr);
        SendMessageW(state->history, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        SetWindowTheme(state->history, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        state->addHistory = CreateWindowExW(0, L"BUTTON", L"添加记录", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(790), Scale(400), Scale(150), Scale(30), window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPsAddHistory)), g_app.instance, nullptr);
        state->deleteHistory = CreateWindowExW(0, L"BUTTON", L"删除记录", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(960), Scale(400), Scale(150), Scale(30), window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPsDeleteHistory)), g_app.instance, nullptr);
        ThemePushButtons({state->addHistory, state->deleteHistory});
        SendMessageW(state->addHistory, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        SendMessageW(state->deleteHistory, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        PrefixSuffixRefreshHistoryList(*state);
        PrefixSuffixLoadControls(*state);
        SetTimer(window, 1, 120, nullptr);
        return 0;
    }
    case WM_TIMER:
        if (wParam == 1) { KillTimer(window, 1); PrefixSuffixRefreshPreview(window, *state); return 0; }
        break;
    case WM_COMMAND:
        if (!state) break;
        if (LOWORD(wParam) == IDOK) { PrefixSuffixReadControls(*state); state->accepted = true; state->done = true; return 0; }
        if (LOWORD(wParam) == IDCANCEL) { state->done = true; return 0; }
        if (LOWORD(wParam) == kPsHistory && HIWORD(wParam) == LBN_SELCHANGE) {
            const int selected = static_cast<int>(SendMessageW(state->history, LB_GETCURSEL, 0, 0));
            if (selected >= 0 && selected < static_cast<int>(g_recentPrefixSuffixRules.size())) {
                state->rule = g_recentPrefixSuffixRules[static_cast<size_t>(selected)];
                PrefixSuffixLoadControls(*state);
                PrefixSuffixRefreshPreview(window, *state);
            }
            return 0;
        }
        if (LOWORD(wParam) == kPsAddHistory && HIWORD(wParam) == BN_CLICKED) {
            PrefixSuffixReadControls(*state);
            RememberPrefixSuffixRule(state->rule);
            PrefixSuffixRefreshHistoryList(*state);
            SendMessageW(state->history, LB_SETCURSEL, 0, 0);
            return 0;
        }
        if (LOWORD(wParam) == kPsDeleteHistory && HIWORD(wParam) == BN_CLICKED) {
            const int selected = static_cast<int>(SendMessageW(state->history, LB_GETCURSEL, 0, 0));
            if (selected >= 0 && selected < static_cast<int>(g_recentPrefixSuffixRules.size())) {
                g_recentPrefixSuffixRules.erase(g_recentPrefixSuffixRules.begin() + selected);
                SavePrefixSuffixHistory();
                PrefixSuffixRefreshHistoryList(*state);
            }
            return 0;
        }
        if (HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == BN_CLICKED) SetTimer(window, 1, 120, nullptr);
        return 0;
    case WM_CLOSE:
        if (state) state->done = true;
        return 0;
    case WM_ERASEBKGND: {
        RECT client{}; GetClientRect(window, &client); FillRect(reinterpret_cast<HDC>(wParam), &client, g_app.windowBrush); return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        const ThemePalette& palette = CurrentPalette();
        const HWND control = reinterpret_cast<HWND>(lParam);
        const bool panelText = control == state->summary;
        SetTextColor(dc, panelText ? palette.statusText : palette.controlText);
        SetBkColor(dc, (message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX)
            ? palette.controlBackground : palette.windowBackground);
        return reinterpret_cast<LRESULT>((message == WM_CTLCOLOREDIT || message == WM_CTLCOLORLISTBOX)
            ? g_app.controlBrush : g_app.windowBrush);
    }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void OpenPrefixSuffixDialog() {
    if (!ActiveDocument()) return;
    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{}; klass.hInstance = g_app.instance; klass.lpfnWndProc = PrefixSuffixDialogProc; klass.lpszClassName = L"MCnotepadPrefixSuffix"; klass.hCursor = LoadCursorW(nullptr, IDC_ARROW); registered = RegisterClassW(&klass) != 0;
    }
    PrefixSuffixDialogState state;
    RECT owner{}; GetWindowRect(g_app.window, &owner);
    LoadPrefixSuffixHistory();
    RECT frame{0, 0, Scale(1140), Scale(450)}; AdjustWindowRectExForDpi(&frame, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME, g_app.dpi);
    const int width = frame.right - frame.left, height = frame.bottom - frame.top;
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"MCnotepadPrefixSuffix", L"前缀与后缀批量处理",
        WS_POPUP | WS_CAPTION | WS_SYSMENU, owner.left + (owner.right - owner.left - width) / 2,
        owner.top + (owner.bottom - owner.top - height) / 2, width, height, g_app.window, nullptr, g_app.instance, &state);
    if (!dialog) return;
    ApplyWindowFrameTheme(dialog); EnableWindow(g_app.window, FALSE); ShowWindow(dialog, SW_SHOW); PrefixSuffixRefreshPreview(dialog, state);
    MSG message{};
    while (!state.done && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (message.message == WM_QUIT) PostQuitMessage(static_cast<int>(message.wParam));
    EnableWindow(g_app.window, TRUE); DestroyWindow(dialog); SetForegroundWindow(g_app.window);
    if (state.accepted) PrefixSuffixApply(state);
    if (TabDocument* document = ActiveDocument()) SetFocus(document->editor);
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

std::wstring JoinLines(const std::vector<std::wstring>& lines, std::wstring_view separator) {
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
// annotate: keep each character and add its reading, e.g. 中(zhōng)文(wén).
bool ConvertToPinyin(std::wstring_view text, PinyinMode mode, std::wstring& output, int& converted, std::wstring& error,
    bool annotate = false) {
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
    // The library drops whitespace; walk the source alongside its results and
    // copy anything it skipped, so no original character is lost.
    size_t cursor = 0;
    const auto copySkipped = [&](size_t upTo) {
        if (upTo <= cursor) return;
        output.append(text.substr(cursor, upTo - cursor));
        previousWasPinyin = false;
        cursor = upTo;
    };
    for (const Pinyin::PinyinRes& item : results) {
        const std::wstring piece = Utf8ToWide(item.hanzi);
        if (!piece.empty()) {
            const size_t found = text.find(piece, cursor);
            if (found != std::wstring_view::npos) {
                copySkipped(found);
                cursor = found + piece.size();
            }
        }
        if (annotate) {
            const std::wstring hanzi = Utf8ToWide(item.hanzi);
            output += hanzi;
            if (!item.error) {
                const std::wstring syllable = Utf8ToWide(item.pinyin);
                output += L"(" + (mode == PinyinMode::Initials ? syllable.substr(0, 1) : syllable) + L")";
                ++converted;
            }
            continue;
        }
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
            if (!output.empty() && !iswspace(output.back()) && (previousWasPinyin || iswalnum(output.back()))) output += L' ';
            output += syllable;
        }
        previousWasPinyin = true;
        ++converted;
    }
    copySkipped(text.size());
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
int g_pinyinFormat = 0;  // 0 tones, 1 plain, 2 initials
int g_pinyinLayout = 0;  // 0 pinyin line under each line (original kept), 1 pinyin only

bool IsTextToolCommand(int command) {
    return command >= kIdTextToolFirst && command <= kIdTextToolLast;
}

bool IsDataGeneratorCommand(int command) {
    return command >= kIdDataFirst && command <= kIdDataLast;
}

void AppendDataGeneratorMenus(HMENU menu) {
    auto add = [](HMENU target, int id, const wchar_t* text) {
        AppendMenuW(target, MF_STRING, id, text);
    };
    HMENU basic = CreatePopupMenu();
    add(basic, kDataLineNumber, L"连续行号");
    add(basic, kDataArithmetic, L"等差数列");
    add(basic, kDataGeometric, L"等比数列");
    add(basic, kDataRandomInteger, L"随机整数");
    add(basic, kDataRandomDecimal, L"随机小数");
    add(basic, kDataRandomPercent, L"随机百分比");
    add(basic, kDataRandomLetter, L"随机字母");
    add(basic, kDataRandomString, L"随机字符串");
    add(basic, kDataBoolean, L"随机布尔值");
    add(basic, kDataPassword, L"随机密码");
    add(basic, kDataCaptcha, L"随机验证码");
    add(basic, kDataColor, L"随机颜色（HEX / RGB / HSL）");
    add(basic, kDataChineseCharacter, L"随机中文字符");
    add(basic, kDataEnglishWord, L"随机英文单词");

    HMENU ids = CreatePopupMenu();
    add(ids, kDataUuid, L"UUID"); add(ids, kDataGuid, L"GUID"); add(ids, kDataUlid, L"ULID");
    add(ids, kDataNanoId, L"NanoID"); add(ids, kDataSnowflake, L"Snowflake ID");
    add(ids, kDataOrderNumber, L"随机订单编号"); add(ids, kDataSku, L"随机商品 SKU");
    add(ids, kDataBarcode, L"随机条形码（UPC / EAN）"); add(ids, kDataSerialNumber, L"随机序列号");

    HMENU dates = CreatePopupMenu();
    add(dates, kDataCurrentDate, L"当前日期"); add(dates, kDataCurrentTime, L"当前时间");
    add(dates, kDataCurrentDateTime, L"当前日期时间"); add(dates, kDataUnixTimestamp, L"Unix 时间戳");
    add(dates, kDataIsoTime, L"ISO 8601 时间"); add(dates, kDataRandomDate, L"随机日期");
    add(dates, kDataRandomTime, L"随机时间"); add(dates, kDataDateSequence, L"连续日期序列");
    add(dates, kDataTimeSequence, L"自定义时间间隔序列");

    HMENU people = CreatePopupMenu();
    add(people, kDataChineseName, L"随机中文姓名"); add(people, kDataEnglishName, L"随机英文姓名");
    add(people, kDataUsername, L"随机用户名"); add(people, kDataEmail, L"随机电子邮箱");
    add(people, kDataPhone, L"虚拟手机号码"); add(people, kDataTelephone, L"虚拟电话号码");
    add(people, kDataGender, L"随机性别"); add(people, kDataAge, L"随机年龄");
    add(people, kDataBirthDate, L"随机出生日期"); add(people, kDataCountry, L"虚拟国家及地区");
    add(people, kDataCity, L"虚拟城市"); add(people, kDataStreet, L"虚拟街道地址");
    add(people, kDataPostcode, L"虚拟邮政编码"); add(people, kDataCompany, L"虚拟公司名称");
    add(people, kDataJob, L"虚拟职业名称"); add(people, kDataProfile, L"虚拟个人资料");
    add(people, kDataFullProfile, L"完整个人信息");

    HMENU network = CreatePopupMenu();
    add(network, kDataIpv4, L"随机 IPv4 地址"); add(network, kDataIpv6, L"随机 IPv6 地址");
    add(network, kDataMac, L"随机 MAC 地址"); add(network, kDataPort, L"随机端口号");
    add(network, kDataDomain, L"虚拟域名"); add(network, kDataUrl, L"虚拟 URL");
    add(network, kDataUserAgent, L"随机 User-Agent"); add(network, kDataApiKey, L"随机 API Key（测试用）");
    add(network, kDataToken, L"随机 Token（测试用）"); add(network, kDataJwt, L"随机 JWT（测试用）");
    add(network, kDataHex, L"随机十六进制数据"); add(network, kDataBinary, L"随机二进制数据");
    add(network, kDataBase64, L"随机 Base64 数据"); add(network, kDataHash, L"随机哈希值");
    add(network, kDataHttpHeaders, L"虚拟 HTTP 请求头");

    HMENU text = CreatePopupMenu();
    add(text, kDataLorem, L"Lorem Ipsum 占位文本"); add(text, kDataChineseLorem, L"中文占位文本");
    add(text, kDataEnglishLorem, L"英文占位文本"); add(text, kDataSentence, L"随机句子");
    add(text, kDataParagraph, L"随机段落"); add(text, kDataRepeatText, L"重复文本");
    add(text, kDataRepeatChar, L"重复字符"); add(text, kDataBlankLines, L"指定数量空行");
    add(text, kDataDivider, L"文本分隔线"); add(text, kDataMarkdownTable, L"Markdown 表格");
    add(text, kDataMarkdownToc, L"Markdown 目录"); add(text, kDataMarkdownTasks, L"Markdown 任务清单");

    HMENU structured = CreatePopupMenu();
    add(structured, kDataJson, L"JSON 测试数据"); add(structured, kDataJsonl, L"JSONL 测试数据");
    add(structured, kDataXml, L"XML 测试数据"); add(structured, kDataCsv, L"CSV 测试数据");
    add(structured, kDataTsv, L"TSV 测试数据"); add(structured, kDataYaml, L"YAML 测试数据");
    add(structured, kDataToml, L"TOML 测试数据"); add(structured, kDataSql, L"SQL INSERT 测试数据");
    add(structured, kDataHtmlTable, L"HTML 表格"); add(structured, kDataTemplate, L"自定义模板数据");
    add(structured, kDataMultiField, L"多字段组合数据"); add(structured, kDataUsers, L"批量虚拟用户数据");
    add(structured, kDataProducts, L"批量虚拟商品数据"); add(structured, kDataOrders, L"批量虚拟订单数据");
    add(structured, kDataLogs, L"模拟系统日志"); add(structured, kDataApiResponse, L"模拟 API 响应数据");

    // Clicking “数据” opens the categories directly. Keep one level of
    // navigation so every generator is reachable without an extra “生成” hop.
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(basic), L"基础数据生成");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(ids), L"标识符生成");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(dates), L"日期与时间生成");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(people), L"虚拟个人信息");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(network), L"网络与开发数据");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(text), L"文本内容生成");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(structured), L"结构化测试数据");
}

struct DataGeneratorOptions {
    int count = 10;
    int64_t start = 1;
    int64_t end = 100;
    int64_t step = 1;
    int length = 12;
    int padding = 0;
    uint64_t seed = 0;
    std::wstring prefix;
    std::wstring suffix;
    std::wstring separator = L"\\n";
    std::wstring charset = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::wstring repeatText = L"示例文本";
    bool unique = false;
    bool descending = false;
    int locale = 0;   // 0 zh_CN, 1 en_US, 2 ja_JP, 3 de_DE, 4 fr_FR
};

faker::Locale DataLocale(int index) {
    switch (index) {
    case 1: return faker::Locale::en_US;
    case 2: return faker::Locale::ja_JP;
    case 3: return faker::Locale::de_DE;
    case 4: return faker::Locale::fr_FR;
    default: return faker::Locale::zh_CN;
    }
}

std::wstring DataUnescape(std::wstring value) {
    std::wstring result;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == L'\\' && i + 1 < value.size()) {
            const wchar_t next = value[++i];
            if (next == L'n') result += L"\r\n";
            else if (next == L'r') result += L'\r';
            else if (next == L't') result += L'\t';
            else result += next;
        } else result += value[i];
    }
    return result;
}

std::wstring DataJoin(const std::vector<std::wstring>& values, const DataGeneratorOptions& options) {
    const std::wstring separator = DataUnescape(options.separator);
    std::wstring result;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) result += separator;
        result += options.prefix;
        result += values[i];
        result += options.suffix;
    }
    return result;
}

std::wstring DataNow(const wchar_t* format) {
    std::time_t now = std::time(nullptr);
    tm local{};
    localtime_s(&local, &now);
    wchar_t buffer[80]{};
    wcsftime(buffer, std::size(buffer), format, &local);
    return buffer;
}

std::wstring DataRandomString(std::mt19937_64& rng, int length, const std::wstring& charset) {
    const std::wstring pool = charset.empty() ? L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" : charset;
    std::uniform_int_distribution<size_t> pick(0, pool.size() - 1);
    std::wstring result;
    result.reserve(static_cast<size_t>(length));
    for (int i = 0; i < length; ++i) result += pool[pick(rng)];
    return result;
}

std::wstring DataPad(int64_t value, int width) {
    std::wstring result = std::to_wstring(value);
    if (width > static_cast<int>(result.size())) result.insert(0, static_cast<size_t>(width - result.size()), L'0');
    return result;
}

std::wstring DataJsonEscape(std::wstring value) {
    std::wstring out;
    for (wchar_t c : value) {
        if (c == L'\\') out += L"\\\\";
        else if (c == L'\"') out += L"\\\"";
        else if (c == L'\r') out += L"\\r";
        else if (c == L'\n') out += L"\\n";
        else out += c;
    }
    return out;
}

void DataReplaceAll(std::wstring& value, std::wstring_view needle, std::wstring_view replacement) {
    if (needle.empty()) return;
    size_t position = 0;
    while ((position = value.find(needle, position)) != std::wstring::npos) {
        value.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
}

std::wstring DataGeneratorLabel(int command) {
    static const std::unordered_map<int, std::wstring> labels{
        {kDataLineNumber,L"连续行号"},{kDataArithmetic,L"等差数列"},{kDataGeometric,L"等比数列"},
        {kDataRandomInteger,L"随机整数"},{kDataRandomDecimal,L"随机小数"},{kDataRandomPercent,L"随机百分比"},
        {kDataRandomLetter,L"随机字母"},{kDataRandomString,L"随机字符串"},{kDataBoolean,L"随机布尔值"},
        {kDataPassword,L"随机密码"},{kDataCaptcha,L"随机验证码"},{kDataColor,L"随机颜色"},
        {kDataChineseCharacter,L"随机中文字符"},{kDataEnglishWord,L"随机英文单词"},{kDataUuid,L"UUID"},
        {kDataGuid,L"GUID"},{kDataUlid,L"ULID"},{kDataNanoId,L"NanoID"},{kDataSnowflake,L"Snowflake ID"},
        {kDataOrderNumber,L"随机订单编号"},{kDataSku,L"随机商品 SKU"},{kDataBarcode,L"随机条形码"},
        {kDataSerialNumber,L"随机序列号"},{kDataCurrentDate,L"当前日期"},{kDataCurrentTime,L"当前时间"},
        {kDataCurrentDateTime,L"当前日期时间"},{kDataUnixTimestamp,L"Unix 时间戳"},{kDataIsoTime,L"ISO 8601 时间"},
        {kDataRandomDate,L"随机日期"},{kDataRandomTime,L"随机时间"},{kDataDateSequence,L"连续日期序列"},
        {kDataTimeSequence,L"时间间隔序列"},{kDataChineseName,L"随机中文姓名"},{kDataEnglishName,L"随机英文姓名"},
        {kDataUsername,L"随机用户名"},{kDataEmail,L"随机电子邮箱"},{kDataPhone,L"虚拟手机号码"},
        {kDataTelephone,L"虚拟电话号码"},{kDataGender,L"随机性别"},{kDataAge,L"随机年龄"},
        {kDataBirthDate,L"随机出生日期"},{kDataCountry,L"虚拟国家及地区"},{kDataCity,L"虚拟城市"},
        {kDataStreet,L"虚拟街道地址"},{kDataPostcode,L"虚拟邮政编码"},{kDataCompany,L"虚拟公司名称"},
        {kDataJob,L"虚拟职业名称"},{kDataProfile,L"虚拟个人资料"},{kDataFullProfile,L"完整个人信息"},{kDataIpv4,L"随机 IPv4"},{kDataIpv6,L"随机 IPv6"},
        {kDataMac,L"随机 MAC"},{kDataPort,L"随机端口"},{kDataDomain,L"虚拟域名"},{kDataUrl,L"虚拟 URL"},
        {kDataUserAgent,L"随机 User-Agent"},{kDataApiKey,L"随机 API Key"},{kDataToken,L"随机 Token"},
        {kDataJwt,L"随机 JWT"},{kDataHex,L"随机十六进制"},{kDataBinary,L"随机二进制"},{kDataBase64,L"随机 Base64"},
        {kDataHash,L"随机哈希"},{kDataHttpHeaders,L"虚拟 HTTP 请求头"},{kDataLorem,L"Lorem Ipsum"},
        {kDataChineseLorem,L"中文占位文本"},{kDataEnglishLorem,L"英文占位文本"},{kDataSentence,L"随机句子"},
        {kDataParagraph,L"随机段落"},{kDataRepeatText,L"重复文本"},{kDataRepeatChar,L"重复字符"},
        {kDataBlankLines,L"指定数量空行"},{kDataDivider,L"文本分隔线"},{kDataMarkdownTable,L"Markdown 表格"},
        {kDataMarkdownToc,L"Markdown 目录"},{kDataMarkdownTasks,L"Markdown 任务清单"},{kDataJson,L"JSON 测试数据"},
        {kDataJsonl,L"JSONL 测试数据"},{kDataXml,L"XML 测试数据"},{kDataCsv,L"CSV 测试数据"},
        {kDataTsv,L"TSV 测试数据"},{kDataYaml,L"YAML 测试数据"},{kDataToml,L"TOML 测试数据"},
        {kDataSql,L"SQL INSERT 测试数据"},{kDataHtmlTable,L"HTML 表格"},{kDataTemplate,L"自定义模板数据"},
        {kDataMultiField,L"多字段组合数据"},{kDataUsers,L"批量虚拟用户数据"},{kDataProducts,L"批量虚拟商品数据"},
        {kDataOrders,L"批量虚拟订单数据"},{kDataLogs,L"模拟系统日志"},{kDataApiResponse,L"模拟 API 响应数据"}
    };
    const auto it = labels.find(command);
    return it == labels.end() ? L"数据生成" : it->second;
}

std::wstring GenerateDataValue(int command, const DataGeneratorOptions& options, std::mt19937_64& rng, int index) {
    const faker::Locale locale = DataLocale(options.locale);
    const auto randomInt = [&](int64_t lo, int64_t hi) {
        if (lo > hi) std::swap(lo, hi);
        return std::uniform_int_distribution<int64_t>(lo, hi)(rng);
    };
    switch (command) {
    case kDataLineNumber: return DataPad(options.start + options.step * index, options.padding);
    case kDataArithmetic: return DataPad(options.start + options.step * index, options.padding);
    case kDataGeometric: return DataPad(static_cast<int64_t>(std::llround(options.start * std::pow(static_cast<double>(options.step), index))), options.padding);
    case kDataRandomInteger: return std::to_wstring(randomInt(options.start, options.end));
    case kDataRandomDecimal: { std::uniform_real_distribution<double> d(static_cast<double>(std::min(options.start, options.end)), static_cast<double>(std::max(options.start, options.end))); std::wostringstream s; s << std::fixed << std::setprecision(4) << d(rng); return s.str(); }
    case kDataRandomPercent: { std::wostringstream s; s << randomInt(0, 100) << L"%"; return s.str(); }
    case kDataRandomLetter: { const wchar_t letters[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"; return std::wstring(1, letters[randomInt(0, 51)]); }
    case kDataRandomString: return DataRandomString(rng, options.length, options.charset);
    case kDataBoolean: return randomInt(0, 1) ? L"true" : L"false";
    case kDataPassword: return Utf8ToWide(faker::internet::password(std::max(4, options.length)));
    case kDataCaptcha: { std::wstring chars = L"0123456789"; return DataRandomString(rng, std::max(1, options.length), chars); }
    case kDataColor: { const int kind = index % 3; return Utf8ToWide(kind == 0 ? faker::color::hex() : kind == 1 ? faker::color::rgb() : faker::color::hsl()); }
    case kDataChineseCharacter: { static const wchar_t chars[] = L"的一是不了人我在有他这为之大来以个中上们到说时国和地要就出会可也你对生能而子那得于着下自之年过发后作里用道行所然家种事成方多经么去法学如都同现当没动面起看定天分还进好小部其些主样理心她本前开但因只从想实日军者意无力它与长把机十民第手"; return std::wstring(1, chars[randomInt(0, static_cast<int64_t>(std::size(chars) - 2))]); }
    case kDataEnglishWord: return Utf8ToWide(faker::word::sample(std::nullopt, faker::Locale::en_US));
    case kDataUuid: case kDataGuid: return Utf8ToWide(faker::string::uuidV4());
    case kDataUlid: return Utf8ToWide(faker::string::ulid());
    case kDataNanoId: return Utf8ToWide(faker::string::nanoId(std::max(4, options.length)));
    case kDataSnowflake: { const uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()); return std::to_wstring((now << 12) | (static_cast<uint64_t>(index) & 0xFFF)); }
    case kDataOrderNumber: return L"ORD-" + DataNow(L"%Y%m%d") + DataPad(randomInt(0, 999999), 6);
    case kDataSku: return Utf8ToWide(faker::commerce::sku(std::max(4, options.length)));
    case kDataBarcode: return Utf8ToWide((index % 2) ? faker::commerce::EAN13() : faker::commerce::EAN8());
    case kDataSerialNumber: return L"SN-" + DataRandomString(rng, std::max(6, options.length), L"ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    case kDataCurrentDate: return DataNow(L"%Y-%m-%d");
    case kDataCurrentTime: return DataNow(L"%H:%M:%S");
    case kDataCurrentDateTime: return DataNow(L"%Y-%m-%d %H:%M:%S");
    case kDataUnixTimestamp: return std::to_wstring(std::time(nullptr));
    case kDataIsoTime: return Utf8ToWide(faker::date::anytime(faker::date::DateFormat::ISO));
    case kDataRandomDate: return Utf8ToWide(faker::date::anytime(faker::date::DateFormat::ISO)).substr(0, 10);
    case kDataRandomTime: return Utf8ToWide(faker::date::anytime(faker::date::DateFormat::ISO)).substr(11, 8);
    case kDataDateSequence: { std::time_t t = std::time(nullptr) + static_cast<time_t>(options.step * index * 86400); tm local{}; localtime_s(&local, &t); wchar_t b[32]{}; wcsftime(b, std::size(b), L"%Y-%m-%d", &local); return b; }
    case kDataTimeSequence: { std::time_t t = std::time(nullptr) + static_cast<time_t>(options.step * index * 60); tm local{}; localtime_s(&local, &t); wchar_t b[32]{}; wcsftime(b, std::size(b), L"%Y-%m-%d %H:%M:%S", &local); return b; }
    case kDataChineseName: return Utf8ToWide(faker::person::fullName(locale));
    case kDataEnglishName: return Utf8ToWide(faker::person::fullName(faker::Locale::en_US));
    case kDataUsername: return Utf8ToWide(faker::internet::username(std::nullopt, std::nullopt, locale));
    case kDataEmail: return Utf8ToWide(faker::internet::exampleEmail(std::nullopt, std::nullopt, locale));
    case kDataPhone: return Utf8ToWide(faker::phone::phoneNumberByFormat());
    case kDataTelephone: return Utf8ToWide(faker::phone::phoneNumberByFormat());
    case kDataGender: return Utf8ToWide(faker::person::gender());
    case kDataAge: return std::to_wstring(randomInt(18, 80));
    case kDataBirthDate: return Utf8ToWide(faker::date::birthdateByAge(18, 80));
    case kDataCountry: return Utf8ToWide(faker::location::country());
    case kDataCity: return Utf8ToWide(faker::location::city(locale));
    case kDataStreet: return Utf8ToWide(faker::location::streetAddress(locale));
    case kDataPostcode: return Utf8ToWide(faker::location::zipCode(locale));
    case kDataCompany: return Utf8ToWide(faker::company::companyName(std::nullopt, locale));
    case kDataJob: return Utf8ToWide(faker::person::jobTitle());
    case kDataProfile: return Utf8ToWide(faker::person::bio());
    case kDataFullProfile: {
        // Build one coherent profile from the same first/last name pair so
        // the generated username and e-mail belong to the displayed person.
        const std::string firstName = std::string{faker::person::firstName(locale)};
        const std::string lastName = std::string{faker::person::lastName(locale)};
        const bool familyNameFirst = options.locale == 0 || options.locale == 2;
        const std::string fullName = familyNameFirst ? lastName + firstName : firstName + " " + lastName;
        const std::wstring username = Utf8ToWide(faker::internet::username(firstName, lastName, locale));
        const std::wstring email = Utf8ToWide(faker::internet::exampleEmail(firstName, lastName, locale));
        const std::wstring mobile = Utf8ToWide(faker::phone::phoneNumberByFormat());
        const std::wstring telephone = Utf8ToWide(faker::phone::phoneNumberByFormat());
        const std::wstring gender = Utf8ToWide(faker::person::sex(locale));
        const int age = static_cast<int>(randomInt(18, 80));
        // faker-cxx requires a non-empty time range. Passing the same age as
        // both bounds can produce a zero-second range and throw internally.
        const std::string birthDateValue = faker::date::birthdateByAge(
            static_cast<unsigned>(age), static_cast<unsigned>(age + 1), faker::date::DateFormat::ISO);
        const std::wstring birthDate = Utf8ToWide(birthDateValue.substr(0, 10));
        const std::wstring country = Utf8ToWide(faker::location::country());
        const std::wstring city = Utf8ToWide(faker::location::city(locale));
        const std::wstring street = Utf8ToWide(faker::location::streetAddress(locale));
        const std::wstring postcode = Utf8ToWide(faker::location::zipCode(locale));
        const std::wstring company = Utf8ToWide(faker::company::companyName(std::nullopt, locale));
        const std::wstring job = Utf8ToWide(faker::person::jobTitle());
        const std::wstring profile = Utf8ToWide(faker::person::bio());

        std::wstring result;
        result += L"姓名：" + Utf8ToWide(fullName) + L"\r\n";
        result += L"用户名：" + username + L"\r\n";
        result += L"电子邮箱：" + email + L"\r\n";
        result += L"手机号码：" + mobile + L"\r\n";
        result += L"电话号码：" + telephone + L"\r\n";
        result += L"性别：" + gender + L"\r\n";
        result += L"年龄：" + std::to_wstring(age) + L"\r\n";
        result += L"出生日期：" + birthDate + L"\r\n";
        result += L"国家/地区：" + country + L"\r\n";
        result += L"城市：" + city + L"\r\n";
        result += L"街道地址：" + street + L"\r\n";
        result += L"邮政编码：" + postcode + L"\r\n";
        result += L"公司：" + company + L"\r\n";
        result += L"职业：" + job + L"\r\n";
        result += L"个人简介：" + profile;
        return result;
    }
    case kDataIpv4: return Utf8ToWide(faker::internet::ipv4());
    case kDataIpv6: return Utf8ToWide(faker::internet::ipv6());
    case kDataMac: return Utf8ToWide(faker::internet::mac());
    case kDataPort: return std::to_wstring(faker::internet::port());
    case kDataDomain: return Utf8ToWide(faker::internet::domainName(locale));
    case kDataUrl: return Utf8ToWide(faker::internet::url());
    case kDataUserAgent: return L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/" + std::to_wstring(randomInt(110, 130)) + L".0 Safari/537.36";
    case kDataApiKey: return L"test_" + DataRandomString(rng, std::max(16, options.length), L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    case kDataToken: return DataRandomString(rng, std::max(24, options.length), L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_~");
    case kDataJwt: return Utf8ToWide(faker::internet::jwtToken());
    case kDataHex: return DataRandomString(rng, std::max(2, options.length), L"0123456789abcdef");
    case kDataBinary: return DataRandomString(rng, std::max(2, options.length), L"01");
    case kDataBase64: return Base64Encode(DataRandomString(rng, std::max(4, options.length), options.charset));
    case kDataHash: return Utf8ToWide(faker::crypto::sha256());
    case kDataHttpHeaders: return Utf8ToWide(faker::internet::httpRequestHeader()) + L": " + Utf8ToWide(faker::internet::httpMediaType());
    case kDataLorem: return Utf8ToWide(faker::lorem::sentence());
    case kDataChineseLorem: return L"这是用于测试和演示的中文占位文本，内容会根据生成数量自动重复。";
    case kDataEnglishLorem: return Utf8ToWide(faker::lorem::sentence());
    case kDataSentence: return Utf8ToWide(faker::lorem::sentence());
    case kDataParagraph: return Utf8ToWide(faker::lorem::paragraph());
    case kDataRepeatText: return options.repeatText;
    case kDataRepeatChar: return std::wstring(std::max(1, options.length), options.repeatText.empty() ? L'喵' : options.repeatText.front());
    case kDataBlankLines: return {};
    case kDataDivider: return L"────────────────────────────────────────";
    case kDataMarkdownTable: return L"| 字段 | 示例值 |\r\n| --- | --- |\r\n| id | " + Utf8ToWide(faker::string::uuidV4()) + L" |";
    case kDataMarkdownToc: return L"- [概述](#概述)\r\n- [安装](#安装)\r\n- [使用方法](#使用方法)";
    case kDataMarkdownTasks: return L"- [ ] 待办事项\r\n- [ ] 测试数据\r\n- [x] 已完成事项";
    case kDataJson: return L"{\"id\":\"" + Utf8ToWide(faker::string::uuidV4()) + L"\",\"name\":\"" + DataJsonEscape(Utf8ToWide(faker::person::fullName(locale))) + L"\",\"email\":\"" + Utf8ToWide(faker::internet::exampleEmail()) + L"\"}";
    case kDataJsonl: return L"{\"id\":\"" + Utf8ToWide(faker::string::uuidV4()) + L"\",\"value\":" + std::to_wstring(randomInt(1, 9999)) + L"}";
    case kDataXml: return L"<item><id>" + Utf8ToWide(faker::string::uuidV4()) + L"</id><name>" + DataJsonEscape(Utf8ToWide(faker::commerce::productName())) + L"</name></item>";
    case kDataCsv: return L"id,name,email\r\n" + Utf8ToWide(faker::string::uuidV4()) + L"," + Utf8ToWide(faker::person::fullName(locale)) + L"," + Utf8ToWide(faker::internet::exampleEmail());
    case kDataTsv: return L"id\tname\temail\r\n" + Utf8ToWide(faker::string::uuidV4()) + L"\t" + Utf8ToWide(faker::person::fullName(locale)) + L"\t" + Utf8ToWide(faker::internet::exampleEmail());
    case kDataYaml: return L"id: " + Utf8ToWide(faker::string::uuidV4()) + L"\r\nname: \"" + DataJsonEscape(Utf8ToWide(faker::person::fullName(locale))) + L"\"\r\nactive: true";
    case kDataToml: return L"id = \"" + Utf8ToWide(faker::string::uuidV4()) + L"\"\r\nname = \"" + DataJsonEscape(Utf8ToWide(faker::person::fullName(locale))) + L"\"";
    case kDataSql: return L"INSERT INTO users (id, name, email) VALUES ('" + Utf8ToWide(faker::string::uuidV4()) + L"', '" + DataJsonEscape(Utf8ToWide(faker::person::fullName(locale))) + L"', '" + Utf8ToWide(faker::internet::exampleEmail()) + L"');";
    case kDataHtmlTable: return L"<table>\r\n  <tr><th>ID</th><th>Name</th></tr>\r\n  <tr><td>" + Utf8ToWide(faker::string::uuidV4()) + L"</td><td>" + DataJsonEscape(Utf8ToWide(faker::person::fullName(locale))) + L"</td></tr>\r\n</table>";
    case kDataTemplate: {
        std::wstring result = options.repeatText.empty() ? L"{{id}} {{name}} {{email}}" : options.repeatText;
        DataReplaceAll(result, L"{{id}}", Utf8ToWide(faker::string::uuidV4()));
        DataReplaceAll(result, L"{{uuid}}", Utf8ToWide(faker::string::uuidV4()));
        DataReplaceAll(result, L"{{name}}", Utf8ToWide(faker::person::fullName(locale)));
        DataReplaceAll(result, L"{{email}}", Utf8ToWide(faker::internet::exampleEmail()));
        DataReplaceAll(result, L"{{date}}", DataNow(L"%Y-%m-%d"));
        DataReplaceAll(result, L"{{index}}", std::to_wstring(index + 1));
        return result;
    }
    case kDataMultiField: return Utf8ToWide(faker::string::uuidV4()) + L" | " + Utf8ToWide(faker::person::fullName(locale)) + L" | " + Utf8ToWide(faker::internet::exampleEmail());
    case kDataUsers: return L"{\"id\":\"" + Utf8ToWide(faker::string::uuidV4()) + L"\",\"name\":\"" + DataJsonEscape(Utf8ToWide(faker::person::fullName(locale))) + L"\",\"email\":\"" + Utf8ToWide(faker::internet::exampleEmail()) + L"\"}";
    case kDataProducts: return L"{\"sku\":\"" + Utf8ToWide(faker::commerce::sku()) + L"\",\"name\":\"" + DataJsonEscape(Utf8ToWide(faker::commerce::productFullName())) + L"\"}";
    case kDataOrders: return L"{\"orderNo\":\"ORD-" + DataNow(L"%Y%m%d") + DataPad(randomInt(0, 999999), 6) + L"\",\"status\":\"paid\"}";
    case kDataLogs: return DataNow(L"%Y-%m-%d %H:%M:%S") + L" [INFO] request_id=" + Utf8ToWide(faker::string::uuidV4()) + L" status=200 latency=" + std::to_wstring(randomInt(1, 999)) + L"ms";
    case kDataApiResponse: return L"{\"code\":200,\"message\":\"ok\",\"data\":{\"id\":\"" + Utf8ToWide(faker::string::uuidV4()) + L"\",\"timestamp\":" + std::to_wstring(std::time(nullptr)) + L"}}";
    default: return {};
    }
}

std::wstring GenerateDataText(int command, const DataGeneratorOptions& options) {
    std::mt19937_64 rng(options.seed ? options.seed : std::random_device{}());
    const int count = std::clamp(options.count, 1, 100000);
    std::vector<std::wstring> values;
    values.reserve(static_cast<size_t>(count));
    std::unordered_set<std::wstring> seen;
    for (int i = 0; i < count; ++i) {
        std::wstring value;
        int attempts = 0;
        do {
            try {
                value = GenerateDataValue(command, options, rng, options.descending ? count - i - 1 : i);
            } catch (const std::exception&) {
                value = L"[数据生成失败]";
            } catch (...) {
                value = L"[数据生成失败]";
            }
        } while (options.unique && !seen.insert(value).second && ++attempts < 100);
        values.push_back(std::move(value));
    }
    if (command == kDataBlankLines) values.assign(static_cast<size_t>(count), L"");
    return DataJoin(values, options);
}

struct DataGeneratorDialogState {
    int command = kDataRandomString;
    HWND window = nullptr;
    std::vector<HWND> edits;
    HWND locale = nullptr;
    HWND output = nullptr;
    HWND unique = nullptr;
    HWND descending = nullptr;
    HWND preview = nullptr;
    bool done = false;
    int action = 0;
    CHARRANGE target{};
};

std::wstring DataDialogText(HWND window) {
    return window ? GetWindowTextValue(window) : std::wstring{};
}

DataGeneratorOptions DataDialogOptions(const DataGeneratorDialogState& state) {
    DataGeneratorOptions options;
    auto number = [&](size_t index, int fallback) {
        return index < state.edits.size() ? std::max(0, _wtoi(DataDialogText(state.edits[index]).c_str())) : fallback;
    };
    auto int64Value = [&](size_t index, int64_t fallback) {
        if (index >= state.edits.size()) return fallback;
        try { return std::stoll(DataDialogText(state.edits[index])); } catch (...) { return fallback; }
    };
    options.count = std::clamp(number(0, 10), 1, 100000);
    options.start = int64Value(1, 1);
    options.end = int64Value(2, 100);
    options.step = int64Value(3, 1);
    options.length = std::clamp(number(4, 12), 1, 4096);
    options.prefix = DataDialogText(state.edits[5]);
    options.suffix = DataDialogText(state.edits[6]);
    options.separator = DataDialogText(state.edits[7]);
    options.padding = std::clamp(number(8, 0), 0, 64);
    try { options.seed = static_cast<uint64_t>(std::stoull(DataDialogText(state.edits[9]))); } catch (...) { options.seed = 0; }
    options.charset = DataDialogText(state.edits[10]);
    options.repeatText = DataDialogText(state.edits[11]);
    options.locale = state.locale ? static_cast<int>(SendMessageW(state.locale, CB_GETCURSEL, 0, 0)) : 0;
    options.unique = state.unique && SendMessageW(state.unique, BM_GETCHECK, 0, 0) == BST_CHECKED;
    options.descending = state.descending && SendMessageW(state.descending, BM_GETCHECK, 0, 0) == BST_CHECKED;
    return options;
}

fs::path DataTemplatePath() {
    return g_app.dataDirectory / L"data-generator-template.dat";
}

bool SaveDataTemplate(const DataGeneratorOptions& options) {
    std::ostringstream output;
    output << "DATA_TEMPLATE\t1\t" << options.count << '\t' << options.start << '\t' << options.end << '\t'
           << options.step << '\t' << options.length << '\t' << options.padding << '\t' << options.seed << '\t'
           << options.locale << '\t' << (options.unique ? 1 : 0) << '\t' << (options.descending ? 1 : 0) << '\t'
           << HexEncode(WideToUtf8(options.prefix)) << '\t' << HexEncode(WideToUtf8(options.suffix)) << '\t'
           << HexEncode(WideToUtf8(options.separator)) << '\t' << HexEncode(WideToUtf8(options.charset)) << '\t'
           << HexEncode(WideToUtf8(options.repeatText)) << '\n';
    return WriteAtomicFile(DataTemplatePath(), output.str());
}

bool LoadDataTemplate(DataGeneratorOptions& options) {
    std::ifstream input(DataTemplatePath(), std::ios::binary);
    if (!input) return false;
    std::string line; std::getline(input, line);
    const std::vector<std::string> parts = SplitTabs(line);
    if (parts.size() < 17 || parts[0] != "DATA_TEMPLATE") return false;
    ParseInt(parts[2], options.count); ParseInt64(parts[3], options.start); ParseInt64(parts[4], options.end);
    ParseInt64(parts[5], options.step); ParseInt(parts[6], options.length); ParseInt(parts[7], options.padding);
    int64_t seed = 0; ParseInt64(parts[8], seed); options.seed = seed >= 0 ? static_cast<uint64_t>(seed) : 0;
    ParseInt(parts[9], options.locale);
    int unique = 0, descending = 0; ParseInt(parts[10], unique); ParseInt(parts[11], descending);
    options.unique = unique != 0; options.descending = descending != 0;
    options.prefix = Utf8ToWide(HexDecode(parts[12])); options.suffix = Utf8ToWide(HexDecode(parts[13]));
    options.separator = Utf8ToWide(HexDecode(parts[14])); options.charset = Utf8ToWide(HexDecode(parts[15]));
    options.repeatText = Utf8ToWide(HexDecode(parts[16]));
    options.count = std::clamp(options.count, 1, 100000); options.length = std::clamp(options.length, 1, 4096);
    options.padding = std::clamp(options.padding, 0, 64); options.locale = std::clamp(options.locale, 0, 4);
    return true;
}

void DataDialogSetOptions(DataGeneratorDialogState& state, const DataGeneratorOptions& options) {
    const std::wstring values[] = {std::to_wstring(options.count), std::to_wstring(options.start), std::to_wstring(options.end),
        std::to_wstring(options.step), std::to_wstring(options.length), options.prefix, options.suffix, options.separator,
        std::to_wstring(options.padding), std::to_wstring(options.seed), options.charset, options.repeatText};
    for (size_t i = 0; i < std::size(values) && i < state.edits.size(); ++i) SetWindowTextW(state.edits[i], values[i].c_str());
    if (state.locale) SendMessageW(state.locale, CB_SETCURSEL, options.locale, 0);
    if (state.unique) SendMessageW(state.unique, BM_SETCHECK, options.unique ? BST_CHECKED : BST_UNCHECKED, 0);
    if (state.descending) SendMessageW(state.descending, BM_SETCHECK, options.descending ? BST_CHECKED : BST_UNCHECKED, 0);
}

void DataDialogUpdatePreview(DataGeneratorDialogState& state) {
    if (!state.preview) return;
    const DataGeneratorOptions options = DataDialogOptions(state);
    DataGeneratorOptions previewOptions = options;
    previewOptions.count = std::min(options.count, 20);
    std::wstring result = GenerateDataText(state.command, previewOptions);
    if (options.count > previewOptions.count) result += L"\r\n…（预览显示前 " + std::to_wstring(previewOptions.count) + L" 条）";
    SetWindowTextW(state.preview, result.c_str());
}

void DataDialogAddControl(HWND control) {
    if (!control) return;
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
    if (g_app.darkTheme) SetWindowTheme(control, L"DarkMode_CFD", nullptr);
}

LRESULT CALLBACK DataGeneratorDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DataGeneratorDialogState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        state = static_cast<DataGeneratorDialogState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        state->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        const ThemePalette& palette = CurrentPalette();
        const std::pair<int, const wchar_t*> fields[] = {
            {kIdDataDialogCount,L"生成数量"},{kIdDataDialogStart,L"起始值"},{kIdDataDialogEnd,L"结束值"},
            {kIdDataDialogStep,L"步长 / 间隔"},{kIdDataDialogLength,L"字符长度"},{kIdDataDialogPrefix,L"前缀"},
            {kIdDataDialogSuffix,L"后缀"},{kIdDataDialogSeparator,L"分隔符（\\n 表示换行）"},{kIdDataDialogPadding,L"补零位数"},
            {kIdDataDialogSeed,L"随机种子（0=随机）"},{kIdDataDialogCharset,L"字符集"},{kIdDataDialogRepeat,L"模板 / 重复文本"}
        };
        const std::wstring defaults[] = {L"10",L"1",L"100",L"1",L"12",L"",L"",L"\\n",L"0",L"0",DataGeneratorOptions{}.charset,L"示例文本"};
        // Keep the dialog compact enough for small laptop resolutions while
        // retaining two full parameter columns beside the preview.
        const int columnWidth = 205;
        const int rightColumnX = 240;
        for (size_t i = 0; i < std::size(fields); ++i) {
            const int column = i < 6 ? 0 : 1;
            const int row = static_cast<int>(i < 6 ? i : i - 6);
            const int x = column == 0 ? 18 : rightColumnX;
            const int y = 16 + row * 42;
            HWND label = CreateWindowExW(0, L"STATIC", fields[i].second, WS_CHILD | WS_VISIBLE,
                Scale(x), Scale(y), Scale(columnWidth), Scale(18), window, nullptr, g_app.instance, nullptr);
            HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", defaults[i].c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, Scale(x), Scale(y + 18), Scale(columnWidth), Scale(24),
                window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(fields[i].first)), g_app.instance, nullptr);
            DataDialogAddControl(label);
            DataDialogAddControl(edit);
            state->edits.push_back(edit);
            (void)palette;
        }
        HWND localeLabel = CreateWindowExW(0, L"STATIC", L"个人信息国家 / 地区", WS_CHILD | WS_VISIBLE,
            Scale(18), Scale(270), Scale(columnWidth), Scale(18), window, nullptr, g_app.instance, nullptr);
        state->locale = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
            Scale(18), Scale(288), Scale(columnWidth), Scale(140), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogLocale)), g_app.instance, nullptr);
        for (const wchar_t* item : {L"中国（简体中文）",L"美国（英文）",L"日本（日文）",L"德国（德文）",L"法国（法文）"}) SendMessageW(state->locale, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        SendMessageW(state->locale, CB_SETCURSEL, 0, 0);
        DataDialogAddControl(localeLabel); DataDialogAddControl(state->locale);
        state->unique = CreateWindowExW(0, L"BUTTON", L"允许重复以外的值尽量不重复", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            Scale(rightColumnX), Scale(270), Scale(220), Scale(24), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogUnique)), g_app.instance, nullptr);
        state->descending = CreateWindowExW(0, L"BUTTON", L"序列降序生成", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            Scale(rightColumnX), Scale(298), Scale(220), Scale(24), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogDescending)), g_app.instance, nullptr);
        ThemeCheckbox(state->unique); ThemeCheckbox(state->descending);
        DataDialogAddControl(state->unique); DataDialogAddControl(state->descending);
        HWND outputLabel = CreateWindowExW(0, L"STATIC", L"输出方式", WS_CHILD | WS_VISIBLE,
            Scale(rightColumnX), Scale(330), Scale(100), Scale(18), window, nullptr, g_app.instance, nullptr);
        state->output = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
            Scale(rightColumnX), Scale(348), Scale(220), Scale(140), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogOutput)), g_app.instance, nullptr);
        for (const wchar_t* item : {L"插入到光标位置",L"替换选中文本",L"追加到文档末尾",L"生成到新建文档",L"复制到剪贴板"}) SendMessageW(state->output, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        SendMessageW(state->output, CB_SETCURSEL, state->target.cpMax > state->target.cpMin ? 1 : 0, 0);
        DataDialogAddControl(outputLabel); DataDialogAddControl(state->output);
        state->preview = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            Scale(450), Scale(16), Scale(310), Scale(310), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogPreview)), g_app.instance, nullptr);
        DataDialogAddControl(state->preview);
        HWND execute = CreateWindowExW(0, L"BUTTON", L"生成并执行", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            Scale(450), Scale(338), Scale(100), Scale(30), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogExecute)), g_app.instance, nullptr);
        HWND copy = CreateWindowExW(0, L"BUTTON", L"复制", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(558), Scale(338), Scale(62), Scale(30), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogCopy)), g_app.instance, nullptr);
        HWND newTab = CreateWindowExW(0, L"BUTTON", L"新标签", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(628), Scale(338), Scale(70), Scale(30), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogNewTab)), g_app.instance, nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(700), Scale(338), Scale(60), Scale(30), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), g_app.instance, nullptr);
        HWND saveTemplate = CreateWindowExW(0, L"BUTTON", L"保存模板", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(18), Scale(338), Scale(100), Scale(30), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogSaveTemplate)), g_app.instance, nullptr);
        for (HWND button : {execute, copy, newTab, cancel, saveTemplate}) { DataDialogAddControl(button); ThemePushButtons({button}); }
        DataGeneratorOptions templateOptions;
        if (LoadDataTemplate(templateOptions)) DataDialogSetOptions(*state, templateOptions);
        DataDialogUpdatePreview(*state);
        return 0;
    }
    case WM_COMMAND:
        if (!state) break;
        if (LOWORD(wParam) == kIdDataDialogExecute || LOWORD(wParam) == kIdDataDialogCopy || LOWORD(wParam) == kIdDataDialogNewTab || LOWORD(wParam) == kIdDataDialogSaveTemplate) {
            state->action = LOWORD(wParam) == kIdDataDialogExecute ? 1 : LOWORD(wParam) == kIdDataDialogCopy ? 2 : LOWORD(wParam) == kIdDataDialogNewTab ? 3 : 4;
            state->done = true;
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL) { state->action = 0; state->done = true; return 0; }
        if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == BN_CLICKED) DataDialogUpdatePreview(*state);
        return 0;
    case WM_CLOSE:
        if (state) { state->action = 0; state->done = true; }
        return 0;
    case WM_ERASEBKGND: {
        RECT client{}; GetClientRect(window, &client); FillRect(reinterpret_cast<HDC>(wParam), &client, g_app.windowBrush); return 1;
    }
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam); SetTextColor(dc, CurrentPalette().controlText); SetBkColor(dc, CurrentPalette().windowBackground); return reinterpret_cast<LRESULT>(g_app.windowBrush);
    }
    case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam); SetTextColor(dc, CurrentPalette().controlText); SetBkColor(dc, CurrentPalette().controlBackground); return reinterpret_cast<LRESULT>(g_app.controlBrush);
    }
    default: break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void OpenDataGeneratorDialog(int command) {
    if (!ActiveDocument()) return;
    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{}; klass.hInstance = g_app.instance; klass.lpfnWndProc = DataGeneratorDialogProc;
        klass.lpszClassName = L"MCnotepadDataGenerator"; klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        registered = RegisterClassW(&klass) != 0;
    }
    DataGeneratorDialogState state; state.command = command;
    SendMessageW(ActiveDocument()->editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&state.target));
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int workWidth = std::max(1, static_cast<int>(workArea.right - workArea.left));
    const int workHeight = std::max(1, static_cast<int>(workArea.bottom - workArea.top));
    const int width = std::min(Scale(800), std::max(Scale(640), workWidth - Scale(16)));
    const int height = std::min(Scale(430), std::max(Scale(400), workHeight - Scale(16)));
    const int left = workArea.left + (workWidth - width) / 2;
    const int top = workArea.top + (workHeight - height) / 2;
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"MCnotepadDataGenerator",
        (L"数据生成 - " + DataGeneratorLabel(command)).c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU,
        left, top, width, height,
        g_app.window, nullptr, g_app.instance, &state);
    if (!dialog) return;
    ApplyWindowFrameTheme(dialog);
    EnableWindow(g_app.window, FALSE); ShowWindow(dialog, SW_SHOW); SetFocus(state.edits.empty() ? state.preview : state.edits[0]);
    MSG message{};
    while (!state.done && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (state.action == 0) {
        EnableWindow(g_app.window, TRUE); DestroyWindow(dialog); SetForegroundWindow(g_app.window);
        return;
    }
    DataGeneratorOptions options = DataDialogOptions(state);
    if (state.action == 4) {
        const bool saved = SaveDataTemplate(options);
        EnableWindow(g_app.window, TRUE); DestroyWindow(dialog); SetForegroundWindow(g_app.window);
        ShowNotice(saved ? L"数据生成模板已保存" : L"数据生成模板保存失败");
        return;
    }
    std::wstring generated = GenerateDataText(command, options);
    const int outputSelection = state.output ? static_cast<int>(SendMessageW(state.output, CB_GETCURSEL, 0, 0)) : 0;
    EnableWindow(g_app.window, TRUE); DestroyWindow(dialog); SetForegroundWindow(g_app.window);
    if (generated.empty() && command != kDataBlankLines) return;
    TabDocument* document = ActiveDocument(); if (!document) return;
    if (state.action == 2 || (state.action == 1 && outputSelection == 4)) {
        CopyTextToClipboard(generated); ShowNotice(L"数据生成结果已复制到剪贴板"); return;
    }
    if (state.action == 3 || (state.action == 1 && outputSelection == 3)) {
        ApplyToolResult(*document, state.target, generated, 1, DataGeneratorLabel(command), L"已生成数据"); return;
    }
    const int output = state.action == 1 ? outputSelection : 0;
    if (output == 2) {
        const LONG end = static_cast<LONG>(GetEditorRawText(document->editor).size());
        ApplyToolResult(*document, CHARRANGE{end, end}, generated, 2, DataGeneratorLabel(command), L"已生成数据");
    } else {
        ApplyToolResult(*document, output == 1 ? state.target : CHARRANGE{state.target.cpMax, state.target.cpMax}, generated, 0, DataGeneratorLabel(command), L"已生成数据");
    }
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
    AppendMenuW(chinese, MF_STRING, kIdPinyinTone, L"汉字转拼音...");
    AppendMenuW(chinese, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(chinese, MF_STRING, kIdToTraditional, L"简体转繁体");
    AppendMenuW(chinese, MF_STRING, kIdToSimplified, L"繁体转简体");
    AppendMenuW(chinese, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(chinese, MF_STRING, kIdFullToHalf, L"全角转半角");
    AppendMenuW(chinese, MF_STRING, kIdHalfToFull, L"半角转全角");
    AppendMenuW(chinese, MF_STRING, kIdChinesePunctToAscii, L"中文标点转英文标点");
    AppendMenuW(chinese, MF_STRING, kIdAsciiPunctToChinese, L"英文标点转中文标点");

    HMENU developer = CreatePopupMenu();
    AppendMenuW(developer, MF_STRING, kIdToolbarFormat, L"格式化 JSON\tShift+Alt+F");
    AppendMenuW(developer, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(developer, MF_STRING, kIdFormatCss, L"CSS 美化");
    AppendMenuW(developer, MF_STRING, kIdMinifyCss, L"CSS 压缩");
    AppendMenuW(developer, MF_STRING, kIdFormatJavaScript, L"JavaScript 美化");
    AppendMenuW(developer, MF_STRING, kIdMinifyJavaScript, L"JavaScript 压缩");
    AppendMenuW(developer, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(developer, MF_STRING, kIdMarkdownToHtml, L"Markdown 转 HTML");
    AppendMenuW(developer, MF_STRING, kIdHtmlToMarkdown, L"HTML 转 Markdown");
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
    HMENU output = CreatePopupMenu();
    AppendMenuW(output, MF_STRING, kIdOutputNewTab, L"输出到新标签（原文不变）");
    AppendMenuW(output, MF_STRING, kIdOutputInsertBelow, L"插入到原文下方（保留原文）");
    AppendMenuW(output, MF_STRING, kIdOutputReplace, L"替换原文");
    CheckMenuRadioItem(output, kIdOutputReplace, kIdOutputInsertBelow, kIdOutputReplace + g_app.toolOutputMode, MF_BYCOMMAND);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(output), L"处理结果输出到");
    AppendMenuW(menu, MF_STRING | (g_app.previewTextTools ? MF_CHECKED : 0), kIdTogglePreview, L"处理前预览更改");
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
    int choice = -1;
    int defaultMode = 1;
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
        int x = width - Scale(16 + 82);
        MoveWindow(GetDlgItem(window, IDCANCEL), x, bottom + Scale(10), Scale(82), Scale(28), TRUE);
        for (const int id : {303, 302, 301}) {  // right to left: insert below, new tab, replace
            x -= Scale(8 + 112);
            MoveWindow(GetDlgItem(window, id), x, bottom + Scale(10), Scale(112), Scale(28), TRUE);
        }
        return 0;
    }
    case WM_COMMAND:
        if (state && LOWORD(wParam) >= 301 && LOWORD(wParam) <= 303) {
            state->choice = LOWORD(wParam) - 301;
            state->accepted = true;
            state->stopPreviewing = SendMessageW(state->dontAsk, BM_GETCHECK, 0, 0) == BST_CHECKED;
            state->done = true;
            return 0;
        }
        if (state && (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)) {
            // Enter applies the default output mode; Esc cancels.
            state->accepted = LOWORD(wParam) == IDOK;
            state->choice = state->accepted ? state->defaultMode : -1;
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

// Renders a line diff into a read-only RichEdit: removed lines red, added
// green, unchanged context grey; long unchanged runs are collapsed.
void FillDiffView(HWND view, const std::wstring& before, const std::wstring& after, int& removed, int& added) {
    const std::vector<DiffLine> diff = DiffLines(SplitLines(before), SplitLines(after));
    removed = 0;
    added = 0;
    for (const DiffLine& line : diff) {
        if (line.kind == DiffKind::Removed) ++removed;
        if (line.kind == DiffKind::Added) ++added;
    }
    std::wstring text;
    std::vector<std::pair<LONG, DiffKind>> marks;
    std::vector<LONG> lineLengths;
    const auto nearChange = [&](size_t index) {
        for (size_t k = (index >= 2 ? index - 2 : 0); k <= std::min(diff.size() - 1, index + 2); ++k) {
            if (diff[k].kind != DiffKind::Same) return true;
        }
        return false;
    };
    constexpr size_t kMaxShown = 3000;
    size_t shown = 0, skipped = 0;
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

    const ThemePalette& palette = CurrentPalette();
    SendMessageW(view, WM_SETREDRAW, FALSE, 0);
    SendMessageW(view, EM_SETBKGNDCOLOR, 0, palette.editorBackground);
    SetWindowTextW(view, text.c_str());
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
        SendMessageW(view, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
        SendMessageW(view, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    }
    SendMessageW(view, EM_SETSEL, 0, 0);
    SendMessageW(view, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(view, nullptr, TRUE);
}

// Shows what a tool will change and how to use the result. Returns the
// chosen output mode (0 replace, 1 new tab, 2 insert below) or -1 to cancel.
int ConfirmChange(const wchar_t* title, const std::wstring& before, const std::wstring& after, int defaultMode) {
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

    HWND label = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
        dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(100)), g_app.instance, nullptr);
    state.view = CreateWindowExW(0, L"RICHEDIT50W", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, 0, 0, 0, dialog, nullptr, g_app.instance, nullptr);
    state.dontAsk = CreateWindowExW(0, L"BUTTON", L"以后不再预览", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        0, 0, 0, 0, dialog, nullptr, g_app.instance, nullptr);
    HWND buttons[3]{};
    const wchar_t* captions[3] = {L"替换原文", L"输出到新标签", L"插入到原文下方"};
    for (int i = 0; i < 3; ++i) {
        const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | (i == defaultMode ? BS_DEFPUSHBUTTON : 0);
        buttons[i] = CreateWindowExW(0, L"BUTTON", captions[i], style, 0, 0, 0, 0, dialog,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(301 + i)), g_app.instance, nullptr);
    }
    HWND apply = buttons[std::clamp(defaultMode, 0, 2)];
    state.defaultMode = defaultMode;
    HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), g_app.instance, nullptr);
    for (HWND control : {label, state.dontAsk, buttons[0], buttons[1], buttons[2], cancel}) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
    }
    SendMessageW(state.view, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
    ThemeCheckbox(state.dontAsk);
    ThemePushButtons({buttons[0], buttons[1], buttons[2], cancel});
    SetWindowTheme(state.view, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    SendMessageW(state.view, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(Scale(4), 0));
    SendMessageW(state.view, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
    int removed = 0, added = 0;
    FillDiffView(state.view, before, after, removed, added);
    SetWindowTextW(label, (L"与原文相比：删除 " + std::to_wstring(removed) + L" 行，新增 " + std::to_wstring(added) +
        L" 行。请选择如何使用处理结果（替换原文后可按 Ctrl+Z 撤销）。").c_str());

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
    return state.accepted ? state.choice : -1;
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

const wchar_t* OutputModeName(int mode) {
    return mode == 0 ? L"替换原文" : mode == 1 ? L"输出到新标签" : L"插入到原文下方";
}

void RunTextTool(int command) {
    if (command >= kIdOutputReplace && command <= kIdOutputInsertBelow) {
        g_app.toolOutputMode = command - kIdOutputReplace;
        g_app.stateDirty = true;
        SetMenuState();
        ShowNotice(std::wstring(L"处理结果将") + OutputModeName(g_app.toolOutputMode));
        return;
    }
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

    if (command == kIdFormatCss || command == kIdMinifyCss ||
        command == kIdFormatJavaScript || command == kIdMinifyJavaScript ||
        command == kIdMarkdownToHtml || command == kIdHtmlToMarkdown) {
        CHARRANGE range{};
        std::wstring text;
        bool wholeDocument = false;
        ToolRange(editor, false, range, text, wholeDocument);
        if (text.empty()) {
            ShowNotice(L"当前没有可处理的内容");
            return;
        }
        const bool css = command == kIdFormatCss || command == kIdMinifyCss;
        const bool beautify = command == kIdFormatCss || command == kIdFormatJavaScript;
        const bool markdownToHtml = command == kIdMarkdownToHtml;
        const std::wstring name = command == kIdFormatCss ? L"CSS 美化" :
            command == kIdMinifyCss ? L"CSS 压缩" :
            command == kIdFormatJavaScript ? L"JavaScript 美化" :
            command == kIdMinifyJavaScript ? L"JavaScript 压缩" :
            command == kIdMarkdownToHtml ? L"Markdown 转 HTML" : L"HTML 转 Markdown";
        std::wstring result;
        if (command == kIdMarkdownToHtml || command == kIdHtmlToMarkdown) {
            std::wstring error;
            if (!ShowdownConvert(text, markdownToHtml, result, error)) {
                MessageBoxW(g_app.window, (name + L"失败：\n" + error).c_str(), kWindowTitle, MB_ICONWARNING);
                return;
            }
        } else {
            result = css
                ? (beautify ? FormatCss(text) : MinifyCss(text))
                : (beautify ? FormatJavaScript(text) : MinifyJavaScript(text));
        }
        if (result == text) {
            ShowNotice(name + L"：内容无需修改");
            return;
        }
        int outputMode = g_app.toolOutputMode;
        if (g_app.previewTextTools) {
            outputMode = ConfirmChange(name.c_str(), text, result, outputMode);
            if (outputMode < 0) return;
        }
        ApplyToolResult(*document, range, result, outputMode, name,
            (beautify ? name + L"完成" : name + L"完成") + (wholeDocument ? L"（全文）" : L"（选中内容）"));
        return;
    }

    bool lineTool = (command >= kIdJoinLines && command <= kIdNumberLines) ||
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
    case kIdPinyinTone:
    case kIdPinyinPlain:
    case kIdPinyinInitials: {
        std::vector<PromptChoice> choices{
            {L"拼音格式：", {L"带声调（zhōng wén）", L"不带声调（zhong wen）", L"首字母（zw）"}, g_pinyinFormat},
            {L"结果形式：", {L"每行中文下方添加一行拼音（保留原文）", L"只输出拼音"}, g_pinyinLayout},
        };
        if (!PromptForText(L"汉字转拼音", {}, values, nullptr, &choices)) return;
        g_pinyinFormat = std::clamp(choices[0].selected, 0, 2);
        g_pinyinLayout = std::clamp(choices[1].selected, 0, 1);
        lineTool = g_pinyinLayout == 0;  // whole lines: a line, then its pinyin
        break;
    }
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
        const PinyinMode mode = g_pinyinFormat == 0 ? PinyinMode::Tone : g_pinyinFormat == 1 ? PinyinMode::Plain : PinyinMode::Initials;
        SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        bool ok = true;
        if (g_pinyinLayout == 0) {
            // Each line stays, followed by its pinyin (lines without Chinese get none).
            std::vector<std::wstring> output;
            for (const std::wstring& line : lines) {
                output.push_back(line);
                std::wstring pinyin;
                int converted = 0;
                if (!(ok = ConvertToPinyin(line, mode, pinyin, converted, error))) break;
                if (converted > 0) output.push_back(pinyin);
                count += converted;
            }
            result = JoinLines(output);
        } else {
            ok = ConvertToPinyin(text, mode, result, count, error);
        }
        if (!ok) {
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
    wchar_t label[128]{};
    GetMenuStringW(g_app.mainMenu, static_cast<UINT>(command), label, static_cast<int>(std::size(label)), MF_BYCOMMAND);
    std::wstring name = label;
    if (const size_t tab = name.find(L'\t'); tab != std::wstring::npos) name.resize(tab);
    if (const size_t dots = name.find(L"..."); dots != std::wstring::npos) name.resize(dots);
    int outputMode = g_app.toolOutputMode;
    const bool pinyinLines = (command == kIdPinyinTone || command == kIdPinyinPlain || command == kIdPinyinInitials) && g_pinyinLayout == 0;
    if (pinyinLines) {
        outputMode = 0;
        notice = L"已在每行中文下方添加拼音（" + std::to_wstring(count) + L" 个汉字）";
    }
    if (g_app.previewTextTools) {
        outputMode = ConfirmChange(name.c_str(), text, result, outputMode);
        if (outputMode < 0) return;
    }
    ApplyToolResult(*document, range, result, outputMode, name, notice);
}

LONG RichEditLength(const std::wstring& text) {
    LONG length = 0;  // CRLF counts as one character in RichEdit
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') ++i;
        ++length;
    }
    return length;
}

// Puts a tool's result where the user asked: over the original (one undo
// step), into a new tab, or right after the original. The result is selected.
void ApplyToolResult(TabDocument& document, CHARRANGE range, const std::wstring& result, int mode,
    const std::wstring& toolName, std::wstring notice) {
    HWND editor = document.editor;
    if (mode == 1) {
        DocumentFormat format;
        format.encoding = document.encoding;
        format.lineEnding = document.lineEnding;
        const int language = EffectiveLanguage(document);
        format.language = language > kLanguageAuto ? language : kLanguageAuto;
        const int index = CreateDocument({}, L"处理结果 - " + toolName, result, true, true, 0, 0, 0, nullptr, format);
        if (index < 0) return;
        SelectTab(index);
        PersistAll(false);
        ShowNotice(notice + L"，结果在新标签中，原文未改动");
        return;
    }
    CHARRANGE target = range;
    std::wstring insertion = result;
    if (mode == 2) {
        // After the original: on its own line(s) when the original ends a line,
        // otherwise right after it, separated by a space.
        const std::wstring all = GetEditorRawText(editor);
        const LONG end = std::min(range.cpMax, static_cast<LONG>(all.size()));
        const bool atLineEnd = end >= static_cast<LONG>(all.size()) || all[static_cast<size_t>(end)] == L'\r';
        insertion = (atLineEnd ? L"\r" : L" ") + result;
        target = CHARRANGE{end, end};
    }
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&target));
    SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(insertion.c_str()));
    const LONG total = static_cast<LONG>(GetEditorRawText(editor).size());
    const LONG start = mode == 2 ? target.cpMin + 1 : target.cpMin;
    CHARRANGE selection{std::min(start, total), std::min(total, start + RichEditLength(result))};
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    SendMessageW(editor, EM_SCROLLCARET, 0, 0);
    RefreshGutter();
    ShowNotice(mode == 2 ? notice + L"，结果已插入到原文下方" : notice);
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

// ===========================================================================
// Find / replace in files: engine
// Runs on background threads. Reads files itself and never writes anything;
// replacing happens later on the UI thread after the user confirms a preview.
// ===========================================================================

enum class SearchScope { Folder = 0, OpenFiles = 1, CurrentFile = 2 };
enum class SearchTarget { Content = 0, FileName = 1, FullPath = 2 };
enum class SearchMode { Plain = 0, Regex = 1, AnyKeyword = 2, AllKeywords = 3 };

struct SearchOptions {
    std::wstring query;
    std::wstring replacement;
    std::vector<std::wstring> folders;
    SearchScope scope = SearchScope::Folder;
    SearchTarget target = SearchTarget::Content;
    SearchMode mode = SearchMode::Plain;
    bool matchCase = false;
    bool wholeWord = false;
    bool recursive = true;
    bool includeHidden = false;
    bool useExcludes = true;
    bool useGitignore = true;
    std::wstring fileTypes;
    std::wstring excludes;
    uint64_t maxFileBytes = 20ull << 20;
    size_t maxMatches = 20000;
    bool forReplace = false;
};

struct SearchHit {
    int line = 0;           // 1-based; 0 for file name / path hits
    int column = 0;         // 1-based, UTF-16 units
    size_t offset = 0;      // in the \n-normalised text
    size_t length = 0;
    std::wstring lineText;  // shown in the results (may be a window of a long line)
    size_t lineMatchStart = 0;
};

struct SearchFileResult {
    std::wstring path;      // empty for an untitled open tab
    std::wstring display;   // path relative to the search folder
    HWND editor = nullptr;  // set when searched from an open tab's text
    std::vector<SearchHit> hits;
    int encoding = 0;
    std::wstring eol = L"\r\n";
    bool mixedEol = false;
    uint64_t timestamp = 0;
    uint64_t size = 0;
    std::wstring before;    // replace mode only, \n line breaks
    std::wstring after;
    int replacements = 0;
};

struct SearchSource {       // text of an open tab, captured on the UI thread
    std::wstring path;
    std::wstring display;
    HWND editor = nullptr;
    std::wstring text;
};

std::vector<std::wstring> SplitPatterns(std::wstring_view text) {
    std::vector<std::wstring> parts;
    std::wstring current;
    for (const wchar_t c : text) {
        if (c == L';' || c == L',' || c == L'\r' || c == L'\n' || c == L'；' || c == L'，') {
            const std::wstring trimmed = TrimCopy(current);
            if (!trimmed.empty()) parts.push_back(trimmed);
            current.clear();
        } else {
            current += c;
        }
    }
    const std::wstring trimmed = TrimCopy(current);
    if (!trimmed.empty()) parts.push_back(trimmed);
    return parts;
}

std::wstring LowerInvariant(std::wstring_view text) {
    if (text.empty()) return {};
    std::wstring result(text);
    LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, text.data(), static_cast<int>(text.size()),
        result.data(), static_cast<int>(result.size()), nullptr, nullptr, 0);
    return result;
}

// Letters, digits and '_' outside the CJK ranges count as word characters, so
// whole-word matching works for code and English without breaking Chinese.
bool IsSearchWordChar(wchar_t c) {
    return c < 0x2E80 && (iswalnum(c) || c == L'_');
}

class SearchMatcher {
public:
    bool Init(const SearchOptions& options, std::wstring& error) {
        options_ = options;
        if (options.mode == SearchMode::Regex) {
            try {
                auto flags = std::regex_constants::ECMAScript | std::regex_constants::multiline;
                if (!options.matchCase) flags |= std::regex_constants::icase;
                regex_ = std::make_shared<std::wregex>(options.query, flags);
            } catch (const std::regex_error& e) {
                error = L"正则表达式有误：" + Utf8ToWide(e.what());
                return false;
            }
        } else if (options.mode == SearchMode::Plain) {
            std::wstring needle = UnescapeInput(options.query);  // \n and \t allow multi-line searches
            std::replace(needle.begin(), needle.end(), L'\r', L'\n');
            needles_.push_back(options.matchCase ? needle : LowerInvariant(needle));
        } else {
            std::wistringstream stream(options.query);
            for (std::wstring word; stream >> word;) needles_.push_back(options.matchCase ? word : LowerInvariant(word));
        }
        if (options.mode != SearchMode::Regex && needles_.empty()) {
            error = L"请输入要查找的内容。";
            return false;
        }
        replacement_ = UnescapeInput(options.replacement);
        std::replace(replacement_.begin(), replacement_.end(), L'\r', L'\n');
        return true;
    }

    const SearchOptions& options() const { return options_; }
    const std::wregex* regex() const { return regex_.get(); }

    // Non-overlapping matches as (offset, length), sorted.
    void Find(const std::wstring& text, std::vector<std::pair<size_t, size_t>>& out) const {
        out.clear();
        if (regex_) {
            for (std::wsregex_iterator it(text.begin(), text.end(), *regex_), end; it != end; ++it) {
                const size_t offset = static_cast<size_t>(it->position(0));
                const size_t length = static_cast<size_t>(it->length(0));
                if (length == 0 || !WordBoundaryOk(text, offset, length)) continue;
                out.emplace_back(offset, length);
            }
            return;
        }
        const std::wstring lowered = options_.matchCase ? std::wstring() : LowerInvariant(text);
        const std::wstring& haystack = options_.matchCase ? text : lowered;
        for (const std::wstring& needle : needles_) {
            size_t found = 0;
            bool any = false;
            for (size_t position = 0; (found = haystack.find(needle, position)) != std::wstring::npos;) {
                if (WordBoundaryOk(haystack, found, needle.size())) {
                    out.emplace_back(found, needle.size());
                    any = true;
                }
                position = found + std::max<size_t>(1, needle.size());
            }
            if (options_.mode == SearchMode::AllKeywords && !any) {
                out.clear();
                return;
            }
        }
        if (needles_.size() > 1) {
            std::sort(out.begin(), out.end());
            std::vector<std::pair<size_t, size_t>> merged;
            for (const auto& match : out) {
                if (!merged.empty() && match.first < merged.back().first + merged.back().second) continue;
                merged.push_back(match);
            }
            out.swap(merged);
        }
    }

    // Matching against a file name or path (single-line strings).
    bool MatchName(const std::wstring& name, std::pair<size_t, size_t>& match) const {
        std::vector<std::pair<size_t, size_t>> matches;
        Find(name, matches);
        if (matches.empty()) return false;
        match = matches.front();
        return true;
    }

    std::wstring Replace(const std::wstring& text, int& count) const {
        count = 0;
        std::wstring result;
        size_t cursor = 0;
        if (regex_) {
            for (std::wsregex_iterator it(text.begin(), text.end(), *regex_), end; it != end; ++it) {
                const size_t offset = static_cast<size_t>(it->position(0));
                const size_t length = static_cast<size_t>(it->length(0));
                if (length == 0 || !WordBoundaryOk(text, offset, length)) continue;
                result.append(text, cursor, offset - cursor);
                result += it->format(replacement_);
                cursor = offset + length;
                ++count;
            }
        } else {
            std::vector<std::pair<size_t, size_t>> matches;
            Find(text, matches);
            for (const auto& [offset, length] : matches) {
                result.append(text, cursor, offset - cursor);
                result += replacement_;
                cursor = offset + length;
                ++count;
            }
        }
        result.append(text, cursor, std::wstring::npos);
        return result;
    }

    // Replacement text for one already-selected match (used by "replace here").
    std::wstring ReplaceOne(const std::wstring& matched) const {
        if (!regex_) return replacement_;
        try {
            return std::regex_replace(matched, *regex_, replacement_, std::regex_constants::format_first_only);
        } catch (...) {
            return replacement_;
        }
    }

private:
    bool WordBoundaryOk(const std::wstring& text, size_t offset, size_t length) const {
        if (!options_.wholeWord || length == 0) return true;
        if (IsSearchWordChar(text[offset]) && offset > 0 && IsSearchWordChar(text[offset - 1])) return false;
        const size_t end = offset + length;
        if (IsSearchWordChar(text[end - 1]) && end < text.size() && IsSearchWordChar(text[end])) return false;
        return true;
    }

    SearchOptions options_;
    std::shared_ptr<std::wregex> regex_;
    std::vector<std::wstring> needles_;
    std::wstring replacement_;
};

// Converts any line-break style to \n and reports the original style.
std::wstring NormalizeToLf(std::wstring_view text, std::wstring& eol, bool& mixed) {
    std::wstring result;
    result.reserve(text.size());
    int crlf = 0, cr = 0, lf = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') { ++i; ++crlf; } else { ++cr; }
            result += L'\n';
        } else {
            if (text[i] == L'\n') ++lf;
            result += text[i];
        }
    }
    eol = crlf >= lf && crlf >= cr ? L"\r\n" : lf >= cr ? L"\n" : L"\r";
    mixed = (crlf > 0) + (cr > 0) + (lf > 0) > 1;
    if (crlf + cr + lf == 0) eol = L"\r\n";
    return result;
}

// Fills line/column and a display excerpt for each match.
void DescribeHits(const std::wstring& text, const std::vector<std::pair<size_t, size_t>>& matches, std::vector<SearchHit>& hits) {
    std::vector<size_t> lineStarts{0};
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\n') lineStarts.push_back(i + 1);
    }
    for (const auto& [offset, length] : matches) {
        const size_t line = static_cast<size_t>(std::upper_bound(lineStarts.begin(), lineStarts.end(), offset) - lineStarts.begin()) - 1;
        const size_t start = lineStarts[line];
        size_t end = text.find(L'\n', start);
        if (end == std::wstring::npos) end = text.size();
        SearchHit hit;
        hit.line = static_cast<int>(line + 1);
        hit.column = static_cast<int>(offset - start + 1);
        hit.offset = offset;
        hit.length = length;
        // Long lines (minified files) are cut to a window around the match.
        size_t from = start;
        size_t to = end;
        if (to - from > 300) {
            from = offset > start + 80 ? offset - 80 : start;
            to = std::min(end, offset + std::min<size_t>(length, 120) + 160);
        }
        hit.lineText = text.substr(from, to - from);
        hit.lineMatchStart = offset - from;
        if (from > start) {
            hit.lineText.insert(0, L"…");
            hit.lineMatchStart += 1;
        }
        if (to < end) hit.lineText += L"…";
        std::replace(hit.lineText.begin(), hit.lineText.end(), L'\t', L' ');
        hits.push_back(std::move(hit));
    }
}

// ---- .gitignore (common subset: globs, dir/ suffix, leading /, ** prefixes; no negation)

struct GitIgnoreRule {
    std::wstring base;     // directory containing the .gitignore
    std::wstring pattern;  // backslash separated
    bool directoryOnly = false;
    bool anchored = false; // contains a slash: matched against the path below `base`
};

void LoadGitIgnore(const std::wstring& directory, std::vector<GitIgnoreRule>& rules) {
    std::vector<char> bytes;
    if (!ReadBinaryFile(fs::path(directory) / L".gitignore", bytes)) return;
    for (std::wstring line : SplitLines(DecodeTextBytes(bytes))) {
        line = TrimCopy(line);
        if (line.empty() || line[0] == L'#' || line[0] == L'!') continue;
        GitIgnoreRule rule;
        rule.base = directory;
        if (line.back() == L'/') {
            rule.directoryOnly = true;
            line.pop_back();
        }
        if (line.size() > 3 && line.ends_with(L"/**")) {
            line.resize(line.size() - 3);
            rule.directoryOnly = true;
        }
        while (line.starts_with(L"**/")) line.erase(0, 3);
        if (line.starts_with(L"/")) {
            line.erase(0, 1);
            rule.anchored = true;
        }
        if (line.find(L'/') != std::wstring::npos) rule.anchored = true;
        std::replace(line.begin(), line.end(), L'/', L'\\');
        if (line.empty()) continue;
        rule.pattern = line;
        rules.push_back(std::move(rule));
    }
}

bool GitIgnored(const std::vector<GitIgnoreRule>& rules, const std::wstring& fullPath, const std::wstring& name, bool isDirectory) {
    for (const GitIgnoreRule& rule : rules) {
        if (rule.directoryOnly && !isDirectory) continue;
        if (rule.anchored) {
            if (fullPath.size() <= rule.base.size() + 1) continue;
            const std::wstring relative = fullPath.substr(rule.base.size() + 1);
            if (PathMatchSpecW(relative.c_str(), rule.pattern.c_str())) return true;
        } else if (PathMatchSpecW(name.c_str(), rule.pattern.c_str())) {
            return true;
        }
    }
    return false;
}

// ---- Job ------------------------------------------------------------------------

struct SearchFileEntry {
    std::wstring path;
    std::wstring display;
    uint64_t size = 0;
};

struct SearchJob {
    int generation = 0;
    SearchMatcher matcher;
    std::vector<SearchSource> sources;  // open-tab scopes
    std::atomic<bool> cancel{false};
    std::atomic<bool> enumerating{true};
    std::atomic<int> enumerated{0};
    std::atomic<int> scanned{0};
    std::atomic<int> skippedLarge{0};
    std::atomic<int> skippedBinary{0};
    std::atomic<int> errorCount{0};
    std::atomic<int> filesHit{0};
    std::atomic<long long> matches{0};
    std::atomic<bool> truncated{false};
    std::mutex mutex;
    std::vector<std::unique_ptr<SearchFileResult>> pending;
    std::vector<std::wstring> errors;
    ULONGLONG started = 0;
    ULONGLONG elapsed = 0;
    HWND notify = nullptr;
    HANDLE finished = nullptr;

    void AddError(const std::wstring& path, const std::wstring& reason) {
        ++errorCount;
        std::lock_guard lock(mutex);
        if (errors.size() < 5000) errors.push_back(path + L"\t" + reason);
    }
    bool Stopped() const { return cancel.load() || truncated.load(); }
};

unsigned __stdcall SearchThreadEntry(void* parameter) {
    std::unique_ptr<std::function<void()>> function(static_cast<std::function<void()>*>(parameter));
    (*function)();
    return 0;
}

// std::regex recurses deeply on long matches; give workers a large stack.
HANDLE StartSearchThread(std::function<void()> function) {
    auto* heap = new std::function<void()>(std::move(function));
    const uintptr_t handle = _beginthreadex(nullptr, 64u << 20, SearchThreadEntry, heap, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (handle == 0) {
        delete heap;
        return nullptr;
    }
    return reinterpret_cast<HANDLE>(handle);
}

bool MatchesAnyPattern(const std::wstring& name, const std::vector<std::wstring>& patterns) {
    for (const std::wstring& pattern : patterns) {
        if (PathMatchSpecW(name.c_str(), pattern.c_str())) return true;
    }
    return false;
}

void EnumerateSearchFiles(SearchJob& job, std::vector<SearchFileEntry>& files) {
    const SearchOptions& options = job.matcher.options();
    std::vector<std::wstring> types = SplitPatterns(options.fileTypes);
    const bool allTypes = types.empty() || std::any_of(types.begin(), types.end(),
        [](const std::wstring& t) { return t == L"*" || t == L"*.*"; });
    // "js" or ".js" mean "*.js".
    for (std::wstring& type : types) {
        if (type.find_first_of(L"*?") == std::wstring::npos) type = (type[0] == L'.' ? L"*" : L"*.") + type;
    }
    std::vector<std::wstring> excludeNames;
    std::vector<std::wstring> excludePaths;
    if (options.useExcludes) {
        for (std::wstring pattern : SplitPatterns(options.excludes)) {
            std::replace(pattern.begin(), pattern.end(), L'/', L'\\');
            while (!pattern.empty() && pattern.back() == L'\\') pattern.pop_back();
            if (pattern.empty()) continue;
            (pattern.find(L'\\') != std::wstring::npos ? excludePaths : excludeNames).push_back(pattern);
        }
    }
    std::unordered_map<std::wstring, bool> seen;  // overlapping folders: each file once

    for (std::wstring root : options.folders) {
        while (root.size() > 3 && (root.back() == L'\\' || root.back() == L'/')) root.pop_back();
        struct Frame { std::wstring directory; std::vector<GitIgnoreRule> rules; };
        std::vector<Frame> stack;
        Frame first{root, {}};
        if (options.useGitignore) LoadGitIgnore(root, first.rules);
        stack.push_back(std::move(first));
        while (!stack.empty() && !job.Stopped()) {
            Frame frame = std::move(stack.back());
            stack.pop_back();
            WIN32_FIND_DATAW data{};
            HANDLE find = FindFirstFileExW((frame.directory + L"\\*").c_str(), FindExInfoBasic, &data,
                FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
            if (find == INVALID_HANDLE_VALUE) {
                const DWORD code = GetLastError();
                if (code != ERROR_FILE_NOT_FOUND) job.AddError(frame.directory, code == ERROR_ACCESS_DENIED ? L"没有访问权限" : L"无法读取目录（错误 " + std::to_wstring(code) + L"）");
                continue;
            }
            std::vector<Frame> children;
            do {
                const std::wstring name = data.cFileName;
                if (name == L"." || name == L"..") continue;
                const bool isDirectory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                if (!options.includeHidden && (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) continue;
                if (isDirectory && (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;  // no symlink loops
                const std::wstring full = frame.directory + L"\\" + name;
                const std::wstring relative = full.size() > root.size() + 1 ? full.substr(root.size() + 1) : name;
                if (MatchesAnyPattern(name, excludeNames) || MatchesAnyPattern(relative, excludePaths)) continue;
                if (options.useGitignore && GitIgnored(frame.rules, full, name, isDirectory)) continue;
                if (isDirectory) {
                    if (!options.recursive) continue;
                    Frame child{full, frame.rules};
                    if (options.useGitignore) LoadGitIgnore(full, child.rules);
                    children.push_back(std::move(child));
                    continue;
                }
                if (!allTypes && !MatchesAnyPattern(name, types)) continue;
                if (!seen.emplace(LowerInvariant(full), true).second) continue;
                const uint64_t size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
                files.push_back({full, options.folders.size() > 1 ? full : relative, size});
                ++job.enumerated;
            } while (!job.Stopped() && FindNextFileW(find, &data));
            FindClose(find);
            // Depth-first, alphabetical: push in reverse so the first child is processed first.
            for (auto it = children.rbegin(); it != children.rend(); ++it) stack.push_back(std::move(*it));
        }
    }
}

void PublishResult(SearchJob& job, std::unique_ptr<SearchFileResult> result) {
    const long long total = job.matches.fetch_add(static_cast<long long>(result->hits.size())) +
        static_cast<long long>(result->hits.size());
    ++job.filesHit;
    if (total >= static_cast<long long>(job.matcher.options().maxMatches)) job.truncated = true;
    std::lock_guard lock(job.mutex);
    job.pending.push_back(std::move(result));
}

void SearchText(SearchJob& job, std::unique_ptr<SearchFileResult> result, const std::wstring& normalized) {
    std::vector<std::pair<size_t, size_t>> matches;
    try {
        job.matcher.Find(normalized, matches);
        if (matches.empty()) return;
        DescribeHits(normalized, matches, result->hits);
        if (job.matcher.options().forReplace) {
            result->before = normalized;
            result->after = job.matcher.Replace(normalized, result->replacements);
            if (result->after == result->before) return;
        }
    } catch (const std::exception&) {
        job.AddError(result->path.empty() ? result->display : result->path, L"正则表达式匹配失败（表达式过于复杂或文件过大）");
        return;
    }
    PublishResult(job, std::move(result));
}

void SearchOneFile(SearchJob& job, const SearchFileEntry& entry) {
    const SearchOptions& options = job.matcher.options();
    if (options.target != SearchTarget::Content) {
        const std::wstring subject = options.target == SearchTarget::FileName
            ? fs::path(entry.path).filename().wstring() : entry.path;
        ++job.scanned;
        std::pair<size_t, size_t> match;
        if (!job.matcher.MatchName(subject, match)) return;
        auto result = std::make_unique<SearchFileResult>();
        result->path = entry.path;
        result->display = entry.display;
        SearchHit hit;
        hit.offset = match.first;
        hit.length = match.second;
        hit.lineText = subject;
        hit.lineMatchStart = match.first;
        result->hits.push_back(std::move(hit));
        PublishResult(job, std::move(result));
        return;
    }
    if (entry.size > options.maxFileBytes) {
        ++job.skippedLarge;
        job.AddError(entry.path, L"已跳过：文件大于 " + std::to_wstring(options.maxFileBytes >> 20) + L" MB");
        return;
    }
    std::vector<char> bytes;
    if (!ReadBinaryFile(entry.path, bytes)) {
        job.AddError(entry.path, L"无法读取（可能没有权限或正被占用）");
        return;
    }
    ++job.scanned;
    const bool utf16Bom = bytes.size() >= 2 &&
        ((static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) ||
         (static_cast<unsigned char>(bytes[0]) == 0xFE && static_cast<unsigned char>(bytes[1]) == 0xFF));
    if (!utf16Bom && memchr(bytes.data(), 0, std::min<size_t>(bytes.size(), 8192)) != nullptr) {
        ++job.skippedBinary;
        return;
    }
    auto result = std::make_unique<SearchFileResult>();
    result->path = entry.path;
    result->display = entry.display;
    result->size = bytes.size();
    result->timestamp = FileTimestamp(entry.path);
    const std::wstring text = DecodeTextBytes(bytes, &result->encoding);
    const std::wstring normalized = NormalizeToLf(text, result->eol, result->mixedEol);
    SearchText(job, std::move(result), normalized);
}

void RunSearchJob(std::shared_ptr<SearchJob> job) {
    std::vector<SearchFileEntry> files;
    if (job->matcher.options().scope == SearchScope::Folder) EnumerateSearchFiles(*job, files);
    job->enumerating = false;

    if (!job->sources.empty()) {
        for (SearchSource& source : job->sources) {
            if (job->Stopped()) break;
            ++job->scanned;
            if (job->matcher.options().target != SearchTarget::Content) {
                std::pair<size_t, size_t> match;
                const std::wstring subject = job->matcher.options().target == SearchTarget::FileName
                    ? source.display : (source.path.empty() ? source.display : source.path);
                if (!job->matcher.MatchName(subject, match)) continue;
                auto result = std::make_unique<SearchFileResult>();
                result->path = source.path;
                result->display = source.display;
                result->editor = source.editor;
                SearchHit hit;
                hit.offset = match.first;
                hit.length = match.second;
                hit.lineText = subject;
                hit.lineMatchStart = match.first;
                result->hits.push_back(std::move(hit));
                PublishResult(*job, std::move(result));
                continue;
            }
            auto result = std::make_unique<SearchFileResult>();
            result->path = source.path;
            result->display = source.display;
            result->editor = source.editor;
            SearchText(*job, std::move(result), source.text);
        }
    } else {
        std::atomic<size_t> next{0};
        const unsigned threads = std::clamp(std::thread::hardware_concurrency(), 2u, 8u);
        std::vector<HANDLE> handles;
        for (unsigned i = 0; i < threads; ++i) {
            HANDLE handle = StartSearchThread([&] {
                for (size_t index; !job->Stopped() && (index = next.fetch_add(1)) < files.size();) {
                    try {
                        SearchOneFile(*job, files[index]);
                    } catch (...) {
                        job->AddError(files[index].path, L"处理失败");
                    }
                }
            });
            if (handle) handles.push_back(handle);
        }
        if (handles.empty()) {  // could not start threads: do it here
            for (const SearchFileEntry& entry : files) {
                if (job->Stopped()) break;
                SearchOneFile(*job, entry);
            }
        }
        if (!handles.empty()) WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), TRUE, INFINITE);
        for (HANDLE handle : handles) CloseHandle(handle);
    }
    job->elapsed = GetTickCount64() - job->started;
    PostMessageW(job->notify, kMessageSearchDone, static_cast<WPARAM>(job->generation), 0);
    SetEvent(job->finished);
}

// ===========================================================================
// Find / replace in files: panel UI
// ===========================================================================

enum SearchControl : int {
    kSearchFind = 3001, kSearchReplace, kSearchScope, kSearchFolder, kSearchBrowse, kSearchTypes,
    kSearchExcludes, kSearchTarget, kSearchMode, kSearchRecursive, kSearchHidden, kSearchMatchCase,
    kSearchWholeWord, kSearchUseExcludes, kSearchGitignore, kSearchStart, kSearchStop, kSearchReplaceButton,
    kSearchClear, kSearchFilter, kSearchStats, kSearchTree, kSearchClose, kSearchMaxSize, kSearchMaxResults,
};
constexpr int kSearchHistoryLimit = 20;
constexpr wchar_t kDefaultExcludes[] = L"node_modules;.git;.next;dist;build;coverage;vendor;*.min.js;*.map;*.lock";
constexpr wchar_t kDefaultFileTypes[] = L"*.*";

struct SearchItemRef { int file; int hit; int prefixLength; };

struct SearchUiState {
    HWND panel = nullptr;
    HWND tree = nullptr;
    HWND stats = nullptr;
    std::unordered_map<int, HWND> controls;
    std::vector<HWND> labels;
    std::shared_ptr<SearchJob> job;
    std::shared_ptr<SearchMatcher> lastMatcher;
    int generation = 0;
    std::vector<std::unique_ptr<SearchFileResult>> results;
    std::vector<SearchItemRef> items;
    std::vector<bool> populated;
    std::vector<HTREEITEM> fileNodes;
    HTREEITEM loadMoreNode = nullptr;
    int shownFiles = 0;
    int nextFile = 0;
    std::wstring filter;
    std::vector<std::wstring> errors;
    std::wstring summary;
    std::wstring lastQuery;
    std::wstring lastFolders;
    bool running = false;
    bool previewWhenDone = false;
    bool resultsStale = false;
    std::vector<std::wstring> findHistory, replaceHistory, folderHistory, typeHistory;
    std::wstring excludes = kDefaultExcludes;
    int optionScope = 0, optionTarget = 0, optionMode = 0;
    bool optionRecursive = true, optionHidden = false, optionMatchCase = false, optionWholeWord = false;
    bool optionUseExcludes = true, optionGitignore = true;
    int maxFileMb = 20;
    int maxResults = 20000;
    std::wstring lastBackupDirectory;
    bool dragging = false;
    int dragStartY = 0;
    int dragStartHeight = 0;
};
SearchUiState g_search;

HWND SearchControl(int id) {
    const auto it = g_search.controls.find(id);
    return it == g_search.controls.end() ? nullptr : it->second;
}

bool SearchChecked(int id) { return SendMessageW(SearchControl(id), BM_GETCHECK, 0, 0) == BST_CHECKED; }

fs::path SearchStatePath() { return g_app.dataDirectory / L"search.dat"; }

void PushHistory(std::vector<std::wstring>& history, const std::wstring& value) {
    if (value.empty()) return;
    history.erase(std::remove(history.begin(), history.end(), value), history.end());
    history.insert(history.begin(), value);
    if (history.size() > kSearchHistoryLimit) history.resize(kSearchHistoryLimit);
}

void LoadSearchState() {
    std::vector<char> bytes;
    if (!ReadBinaryFile(SearchStatePath(), bytes)) return;
    std::string content(bytes.begin(), bytes.end());
    std::istringstream stream(content);
    for (std::string line; std::getline(stream, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::vector<std::string> parts = SplitTabs(line);
        if (parts.size() < 2) continue;
        const std::wstring value = Utf8ToWide(HexDecode(parts[1]));
        if (parts[0] == "FIND") g_search.findHistory.push_back(value);
        else if (parts[0] == "REPLACE") g_search.replaceHistory.push_back(value);
        else if (parts[0] == "FOLDER") g_search.folderHistory.push_back(value);
        else if (parts[0] == "TYPES") g_search.typeHistory.push_back(value);
        else if (parts[0] == "EXCLUDES") g_search.excludes = value;
        else if (parts[0] == "BACKUP") g_search.lastBackupDirectory = value;
        else if (parts[0] == "OPTIONS" && parts.size() >= 14) {
            int v[13]{};
            for (int i = 0; i < 13; ++i) ParseInt(parts[static_cast<size_t>(i + 1)], v[i]);
            g_search.optionScope = std::clamp(v[0], 0, 2);
            g_search.optionTarget = std::clamp(v[1], 0, 2);
            g_search.optionMode = std::clamp(v[2], 0, 3);
            g_search.optionRecursive = v[3] != 0;
            g_search.optionHidden = v[4] != 0;
            g_search.optionMatchCase = v[5] != 0;
            g_search.optionWholeWord = v[6] != 0;
            g_search.optionUseExcludes = v[7] != 0;
            g_search.optionGitignore = v[8] != 0;
            g_search.maxFileMb = std::clamp(v[9], 1, 2048);
            g_search.maxResults = std::clamp(v[10], 100, 1000000);
            g_app.searchPanelHeight = std::clamp(v[11], 160, 2000);
            g_app.searchPanelVisible = v[12] != 0;
        }
    }
}

void ReadSearchOptionsFromControls();

void SaveSearchState() {
    if (g_search.panel) ReadSearchOptionsFromControls();
    std::ostringstream output;
    const auto list = [&](const char* tag, const std::vector<std::wstring>& values) {
        for (const std::wstring& value : values) output << tag << '\t' << HexEncode(WideToUtf8(value)) << '\n';
    };
    list("FIND", g_search.findHistory);
    list("REPLACE", g_search.replaceHistory);
    list("FOLDER", g_search.folderHistory);
    list("TYPES", g_search.typeHistory);
    output << "EXCLUDES\t" << HexEncode(WideToUtf8(g_search.excludes)) << '\n';
    output << "BACKUP\t" << HexEncode(WideToUtf8(g_search.lastBackupDirectory)) << '\n';
    output << "OPTIONS\t" << g_search.optionScope << '\t' << g_search.optionTarget << '\t' << g_search.optionMode << '\t'
           << g_search.optionRecursive << '\t' << g_search.optionHidden << '\t' << g_search.optionMatchCase << '\t'
           << g_search.optionWholeWord << '\t' << g_search.optionUseExcludes << '\t' << g_search.optionGitignore << '\t'
           << g_search.maxFileMb << '\t' << g_search.maxResults << '\t' << g_app.searchPanelHeight << '\t'
           << (g_app.searchPanelVisible ? 1 : 0) << '\n';
    WriteAtomicFile(SearchStatePath(), output.str());
}

void FillCombo(HWND combo, const std::vector<std::wstring>& values) {
    const std::wstring current = GetWindowTextValue(combo);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (const std::wstring& value : values) SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str()));
    SetWindowTextW(combo, current.c_str());
}

void RebuildSearchHistoryMenu() {
    if (!g_app.searchHistoryMenu) return;
    while (GetMenuItemCount(g_app.searchHistoryMenu) > 0) DeleteMenu(g_app.searchHistoryMenu, 0, MF_BYPOSITION);
    if (g_search.findHistory.empty()) {
        AppendMenuW(g_app.searchHistoryMenu, MF_STRING | MF_GRAYED, 0, L"（暂无搜索历史）");
        return;
    }
    for (size_t i = 0; i < g_search.findHistory.size() && i < 15; ++i) {
        std::wstring label = g_search.findHistory[i];
        if (label.size() > 40) label = label.substr(0, 40) + L"…";
        for (size_t p = 0; (p = label.find(L'&', p)) != std::wstring::npos; p += 2) label.insert(p, L"&");
        AppendMenuW(g_app.searchHistoryMenu, MF_STRING, static_cast<UINT_PTR>(kIdSearchHistoryFirst + static_cast<int>(i)), label.c_str());
    }
}

void ReadSearchOptionsFromControls() {
    g_search.optionScope = std::max(0, static_cast<int>(SendMessageW(SearchControl(kSearchScope), CB_GETCURSEL, 0, 0)));
    g_search.optionTarget = std::max(0, static_cast<int>(SendMessageW(SearchControl(kSearchTarget), CB_GETCURSEL, 0, 0)));
    g_search.optionMode = std::max(0, static_cast<int>(SendMessageW(SearchControl(kSearchMode), CB_GETCURSEL, 0, 0)));
    g_search.optionRecursive = SearchChecked(kSearchRecursive);
    g_search.optionHidden = SearchChecked(kSearchHidden);
    g_search.optionMatchCase = SearchChecked(kSearchMatchCase);
    g_search.optionWholeWord = SearchChecked(kSearchWholeWord);
    g_search.optionUseExcludes = SearchChecked(kSearchUseExcludes);
    g_search.optionGitignore = SearchChecked(kSearchGitignore);
    g_search.excludes = GetWindowTextValue(SearchControl(kSearchExcludes));
    g_search.maxFileMb = std::clamp(_wtoi(GetWindowTextValue(SearchControl(kSearchMaxSize)).c_str()), 1, 2048);
    g_search.maxResults = std::clamp(_wtoi(GetWindowTextValue(SearchControl(kSearchMaxResults)).c_str()), 100, 1000000);
}

// ---- Results tree ----------------------------------------------------------------

bool HitPassesFilter(const SearchFileResult& file, const SearchHit& hit) {
    if (g_search.filter.empty()) return true;
    return LowerInvariant(hit.lineText).find(g_search.filter) != std::wstring::npos ||
        LowerInvariant(file.display).find(g_search.filter) != std::wstring::npos;
}

int VisibleHitCount(const SearchFileResult& file) {
    int count = 0;
    for (const SearchHit& hit : file.hits) count += HitPassesFilter(file, hit) ? 1 : 0;
    return count;
}

void PopulateFileNode(int fileIndex) {
    if (fileIndex < 0 || fileIndex >= static_cast<int>(g_search.results.size()) || g_search.populated[static_cast<size_t>(fileIndex)]) return;
    g_search.populated[static_cast<size_t>(fileIndex)] = true;
    const SearchFileResult& file = *g_search.results[static_cast<size_t>(fileIndex)];
    for (size_t h = 0; h < file.hits.size(); ++h) {
        const SearchHit& hit = file.hits[h];
        if (!HitPassesFilter(file, hit)) continue;
        std::wstring prefix = hit.line > 0
            ? std::to_wstring(hit.line) + L":" + std::to_wstring(hit.column) + L"    "
            : L"文件名    ";
        std::wstring text = prefix + hit.lineText;
        g_search.items.push_back({fileIndex, static_cast<int>(h), static_cast<int>(prefix.size())});
        TVINSERTSTRUCTW insert{};
        insert.hParent = g_search.fileNodes[static_cast<size_t>(fileIndex)];
        insert.hInsertAfter = TVI_LAST;
        insert.item.mask = TVIF_TEXT | TVIF_PARAM;
        insert.item.pszText = text.data();
        insert.item.lParam = static_cast<LPARAM>(g_search.items.size() - 1);
        TreeView_InsertItem(g_search.tree, &insert);
    }
}

constexpr int kLiveTreeFiles = 300;   // file nodes shown while a search runs
constexpr int kTreeBatchFiles = 500;  // file nodes added per "load more"
constexpr int kLoadMoreItem = -2;

void SyncTreeVectors() {
    g_search.fileNodes.resize(g_search.results.size(), nullptr);
    g_search.populated.resize(g_search.results.size(), false);
}

// Returns false if the file has nothing to show under the current filter.
bool AddFileNode(int fileIndex) {
    SyncTreeVectors();
    const SearchFileResult& file = *g_search.results[static_cast<size_t>(fileIndex)];
    const int visible = VisibleHitCount(file);
    if (visible == 0) return false;
    std::wstring text = file.display + L"  (" + std::to_wstring(visible) + L")";
    g_search.items.push_back({fileIndex, -1, 0});
    TVINSERTSTRUCTW insert{};
    insert.hParent = TVI_ROOT;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
    insert.item.pszText = text.data();
    insert.item.cChildren = 1;
    insert.item.lParam = static_cast<LPARAM>(g_search.items.size() - 1);
    HTREEITEM node = TreeView_InsertItem(g_search.tree, &insert);
    g_search.fileNodes[static_cast<size_t>(fileIndex)] = node;
    g_search.populated[static_cast<size_t>(fileIndex)] = false;
    ++g_search.shownFiles;
    // The first files open expanded; the rest load their lines when expanded.
    if (g_search.shownFiles <= 40) {
        PopulateFileNode(fileIndex);
        TreeView_Expand(g_search.tree, node, TVE_EXPAND);
    }
    return true;
}

void RemoveLoadMoreNode() {
    if (g_search.loadMoreNode) TreeView_DeleteItem(g_search.tree, g_search.loadMoreNode);
    g_search.loadMoreNode = nullptr;
}

// Adds file nodes from `nextFile` until `limit` are shown, then a "load more"
// row if results remain. Keeps the UI responsive with huge result sets.
void AppendFileNodes(int limit) {
    RemoveLoadMoreNode();
    while (g_search.nextFile < static_cast<int>(g_search.results.size()) && g_search.shownFiles < limit) {
        AddFileNode(g_search.nextFile++);
    }
    int remaining = 0;
    for (int i = g_search.nextFile; i < static_cast<int>(g_search.results.size()); ++i) {
        remaining += VisibleHitCount(*g_search.results[static_cast<size_t>(i)]) > 0 ? 1 : 0;
    }
    if (remaining > 0 && !g_search.running) {
        std::wstring text = L"还有 " + std::to_wstring(remaining) + L" 个文件未显示，双击或按 Enter 加载更多";
        g_search.items.push_back({kLoadMoreItem, -1, 0});
        TVINSERTSTRUCTW insert{};
        insert.hParent = TVI_ROOT;
        insert.hInsertAfter = TVI_LAST;
        insert.item.mask = TVIF_TEXT | TVIF_PARAM;
        insert.item.pszText = text.data();
        insert.item.lParam = static_cast<LPARAM>(g_search.items.size() - 1);
        g_search.loadMoreNode = TreeView_InsertItem(g_search.tree, &insert);
    }
}

void LoadMoreResults() {
    SendMessageW(g_search.tree, WM_SETREDRAW, FALSE, 0);
    AppendFileNodes(g_search.shownFiles + kTreeBatchFiles);
    SendMessageW(g_search.tree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_search.tree, nullptr, TRUE);
}

void RebuildResultTree() {
    if (!g_search.tree) return;
    SendMessageW(g_search.tree, WM_SETREDRAW, FALSE, 0);
    g_search.loadMoreNode = nullptr;
    TreeView_DeleteAllItems(g_search.tree);
    g_search.items.clear();
    g_search.fileNodes.assign(g_search.results.size(), nullptr);
    g_search.populated.assign(g_search.results.size(), false);
    g_search.shownFiles = 0;
    g_search.nextFile = 0;
    AppendFileNodes(g_search.running ? kLiveTreeFiles : kTreeBatchFiles);
    SendMessageW(g_search.tree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_search.tree, nullptr, TRUE);
}

std::wstring FormatSeconds(ULONGLONG milliseconds) {
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%.2f", static_cast<double>(milliseconds) / 1000.0);
    return buffer;
}

void UpdateSearchStats() {
    if (!g_search.stats) return;
    std::wstring text;
    const std::shared_ptr<SearchJob> job = g_search.job;
    if (!job) {
        text = g_search.summary.empty() ? L"输入要查找的内容，按 Enter 开始搜索" : g_search.summary;
    } else {
        long long matches = 0;
        for (const auto& file : g_search.results) matches += static_cast<long long>(file->hits.size());
        if (g_search.running) {
            text = job->enumerating ? L"正在列出文件… 已找到 " + std::to_wstring(job->enumerated.load()) + L" 个"
                : L"正在搜索… 已扫描 " + std::to_wstring(job->scanned.load()) + L" / " + std::to_wstring(job->enumerated.load()) + L" 个文件";
            text += L" | " + std::to_wstring(g_search.results.size()) + L" 个文件命中 | " + std::to_wstring(matches) + L" 处匹配";
        } else {
            text = L"已扫描 " + std::to_wstring(job->scanned.load()) + L" 个文件 | " + std::to_wstring(g_search.results.size()) +
                L" 个文件命中 | " + std::to_wstring(matches) + L" 处匹配 | " + FormatSeconds(job->elapsed) + L" 秒";
            const int skipped = job->skippedLarge.load() + job->skippedBinary.load();
            if (skipped > 0) {
                text += L" | 跳过 " + std::to_wstring(skipped) + L" 个（过大 " + std::to_wstring(job->skippedLarge.load()) +
                    L"，二进制 " + std::to_wstring(job->skippedBinary.load()) + L"）";
            }
            const int errors = job->errorCount.load() - job->skippedLarge.load();
            if (errors > 0) text += L" | " + std::to_wstring(errors) + L" 个错误（点击查看）";
            if (job->truncated) text += L" | 结果过多，已在 " + std::to_wstring(g_search.maxResults) + L" 处后停止";
            if (job->cancel) text += L" | 已停止";
            if (g_search.resultsStale) text += L" | 文件已被替换，结果可能已过期";
        }
    }
    SetWindowTextW(g_search.stats, text.c_str());
    EnableWindow(SearchControl(kSearchStop), g_search.running);
}


void DrainSearchResults() {
    const std::shared_ptr<SearchJob> job = g_search.job;
    if (!job) return;
    std::vector<std::unique_ptr<SearchFileResult>> batch;
    {
        std::lock_guard lock(job->mutex);
        batch.swap(job->pending);
    }
    if (batch.empty()) return;
    SendMessageW(g_search.tree, WM_SETREDRAW, FALSE, 0);
    for (auto& result : batch) g_search.results.push_back(std::move(result));
    SyncTreeVectors();
    AppendFileNodes(kLiveTreeFiles);
    SendMessageW(g_search.tree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_search.tree, nullptr, FALSE);
}

void CancelSearch(bool wait) {
    const std::shared_ptr<SearchJob> job = g_search.job;
    if (!job) return;
    job->cancel = true;
    if (wait && job->finished) WaitForSingleObject(job->finished, 3000);
}

void ClearSearchResults() {
    CancelSearch(false);
    g_search.running = false;
    g_search.job.reset();
    g_search.results.clear();
    g_search.errors.clear();
    g_search.summary.clear();
    g_search.resultsStale = false;
    if (g_search.panel) KillTimer(g_search.panel, kTimerSearchProgress);
    RebuildResultTree();
    UpdateSearchStats();
}

// ---- Starting a search -------------------------------------------------------------

void ShowReplaceInFilesPreview();

void StartSearch(bool forReplace) {
    if (!g_search.panel) return;
    ReadSearchOptionsFromControls();
    SearchOptions options;
    options.query = GetWindowTextValue(SearchControl(kSearchFind));
    options.replacement = GetWindowTextValue(SearchControl(kSearchReplace));
    options.scope = static_cast<SearchScope>(g_search.optionScope);
    options.target = static_cast<SearchTarget>(g_search.optionTarget);
    options.mode = static_cast<SearchMode>(g_search.optionMode);
    options.matchCase = g_search.optionMatchCase;
    options.wholeWord = g_search.optionWholeWord;
    options.recursive = g_search.optionRecursive;
    options.includeHidden = g_search.optionHidden;
    options.useExcludes = g_search.optionUseExcludes;
    options.useGitignore = g_search.optionGitignore;
    options.fileTypes = GetWindowTextValue(SearchControl(kSearchTypes));
    options.excludes = g_search.excludes;
    options.maxFileBytes = static_cast<uint64_t>(g_search.maxFileMb) << 20;
    options.maxMatches = static_cast<size_t>(g_search.maxResults);
    options.forReplace = forReplace;
    if (forReplace && options.target != SearchTarget::Content) {
        MessageBoxW(g_app.window, L"替换只适用于“文件内容”搜索。", kWindowTitle, MB_ICONINFORMATION);
        return;
    }
    if (TrimCopy(options.query).empty()) {
        SetFocus(SearchControl(kSearchFind));
        MessageBeep(MB_ICONWARNING);
        return;
    }

    auto job = std::make_shared<SearchJob>();
    std::wstring error;
    if (!job->matcher.Init(options, error)) {
        MessageBoxW(g_app.window, error.c_str(), kWindowTitle, MB_ICONWARNING);
        SetFocus(SearchControl(kSearchFind));
        return;
    }
    const std::wstring folderText = GetWindowTextValue(SearchControl(kSearchFolder));
    if (options.scope == SearchScope::Folder) {
        std::vector<std::wstring> folders;
        for (std::wstring folder : SplitPatterns(folderText)) {
            std::replace(folder.begin(), folder.end(), L'/', L'\\');
            std::error_code ec;
            if (!fs::is_directory(folder, ec)) {
                MessageBoxW(g_app.window, (L"找不到文件夹：" + folder).c_str(), kWindowTitle, MB_ICONWARNING);
                SetFocus(SearchControl(kSearchFolder));
                return;
            }
            folders.push_back(fs::absolute(folder, ec).lexically_normal().wstring());
        }
        if (folders.empty()) {
            MessageBoxW(g_app.window, L"请选择要搜索的文件夹（多个文件夹用分号分隔）。", kWindowTitle, MB_ICONINFORMATION);
            SetFocus(SearchControl(kSearchFolder));
            return;
        }
        // Re-initialise so the matcher's copy of the options has the folders.
        options.folders = folders;
        job->matcher.Init(options, error);
        PushHistory(g_search.folderHistory, folderText);
    } else {
        // Open-tab scopes search the editor text as it is now (unsaved edits included).
        for (size_t i = 0; i < g_app.documents.size(); ++i) {
            if (options.scope == SearchScope::CurrentFile && static_cast<int>(i) != g_app.activeIndex) continue;
            const TabDocument& document = g_app.documents[i];
            SearchSource source;
            source.path = document.untitled ? std::wstring() : document.path;
            source.display = document.untitled ? document.title : document.path;
            source.editor = document.editor;
            source.text = GetEditorRawText(document.editor);
            std::replace(source.text.begin(), source.text.end(), L'\r', L'\n');
            job->sources.push_back(std::move(source));
        }
    }

    CancelSearch(false);
    PushHistory(g_search.findHistory, options.query);
    if (forReplace) PushHistory(g_search.replaceHistory, options.replacement);
    PushHistory(g_search.typeHistory, options.fileTypes);
    for (auto [id, history] : {std::pair{kSearchFind, &g_search.findHistory}, {kSearchReplace, &g_search.replaceHistory},
             {kSearchFolder, &g_search.folderHistory}, {kSearchTypes, &g_search.typeHistory}}) {
        FillCombo(SearchControl(id), *history);
    }
    RebuildSearchHistoryMenu();
    SaveSearchState();

    g_search.results.clear();
    g_search.errors.clear();
    g_search.summary.clear();
    g_search.resultsStale = false;
    g_search.filter.clear();
    SetWindowTextW(SearchControl(kSearchFilter), L"");
    RebuildResultTree();
    g_search.lastQuery = options.query;
    g_search.lastFolders = options.scope == SearchScope::Folder ? folderText
        : options.scope == SearchScope::OpenFiles ? L"所有打开的文件" : L"当前文件";
    g_search.lastMatcher = std::shared_ptr<SearchMatcher>(job, &job->matcher);
    g_search.previewWhenDone = forReplace;
    job->generation = ++g_search.generation;
    job->notify = g_search.panel;
    job->started = GetTickCount64();
    job->finished = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_search.job = job;
    g_search.running = true;
    HANDLE thread = StartSearchThread([job] { RunSearchJob(job); });
    if (!thread) {
        g_search.running = false;
        MessageBoxW(g_app.window, L"无法启动搜索线程。", kWindowTitle, MB_ICONERROR);
        return;
    }
    CloseHandle(thread);
    SetTimer(g_search.panel, kTimerSearchProgress, 150, nullptr);
    UpdateSearchStats();
}

void FinishSearch(int generation) {
    const std::shared_ptr<SearchJob> job = g_search.job;
    if (!job || job->generation != generation) return;
    KillTimer(g_search.panel, kTimerSearchProgress);
    DrainSearchResults();
    g_search.running = false;
    {
        std::lock_guard lock(job->mutex);
        g_search.errors = job->errors;
    }
    // Stable, readable order once everything is in.
    std::stable_sort(g_search.results.begin(), g_search.results.end(),
        [](const auto& a, const auto& b) { return _wcsicmp(a->display.c_str(), b->display.c_str()) < 0; });
    RebuildResultTree();
    UpdateSearchStats();
    if (g_search.previewWhenDone) {
        g_search.previewWhenDone = false;
        if (job->cancel) return;
        if (g_search.results.empty()) {
            MessageBoxW(g_app.window, L"没有找到可以替换的内容。", kWindowTitle, MB_ICONINFORMATION);
            return;
        }
        if (job->truncated) {
            MessageBoxW(g_app.window, L"匹配数量超过了“最多结果”上限，只会预览和替换已找到的部分。\n如需全部替换，请调大上限后重试。",
                kWindowTitle, MB_ICONINFORMATION);
        }
        ShowReplaceInFilesPreview();
    }
}

// ---- Opening a result ----------------------------------------------------------------

void OpenSearchResult(int itemIndex) {
    if (itemIndex < 0 || itemIndex >= static_cast<int>(g_search.items.size())) return;
    const SearchItemRef ref = g_search.items[static_cast<size_t>(itemIndex)];
    if (ref.file == kLoadMoreItem) {
        LoadMoreResults();
        return;
    }
    const SearchFileResult& file = *g_search.results[static_cast<size_t>(ref.file)];
    int documentIndex = -1;
    if (file.editor) {
        documentIndex = IndexOfEditor(file.editor);
        if (documentIndex < 0 && file.path.empty()) {
            ShowNotice(L"该标签已经关闭");
            return;
        }
    }
    if (documentIndex < 0) {
        if (!OpenFileInNewTab(file.path)) return;
        documentIndex = g_app.activeIndex;
    } else {
        SelectTab(documentIndex);
    }
    const int hitIndex = ref.hit >= 0 ? ref.hit : 0;
    if (file.hits.empty() || file.hits[static_cast<size_t>(hitIndex)].line == 0) return;
    const SearchHit& hit = file.hits[static_cast<size_t>(hitIndex)];
    HWND editor = g_app.documents[static_cast<size_t>(documentIndex)].editor;
    // RichEdit keeps one character per line break, like the \n-normalised text.
    const std::wstring text = GetEditorRawText(editor);
    LONG position = 0;
    for (int line = 1; line < hit.line && position < static_cast<LONG>(text.size()); ++position) {
        if (text[static_cast<size_t>(position)] == L'\r') ++line;
    }
    position += hit.column - 1;
    CHARRANGE range{position, position + static_cast<LONG>(hit.length)};
    range.cpMin = std::min(range.cpMin, static_cast<LONG>(text.size()));
    range.cpMax = std::min(range.cpMax, static_cast<LONG>(text.size()));
    SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
    SendMessageW(editor, EM_SCROLLCARET, 0, 0);
    SetFocus(editor);
    RefreshGutter();
    UpdateStatusBar();
}

int SelectedSearchItem() {
    HTREEITEM item = TreeView_GetSelection(g_search.tree);
    if (!item) return -1;
    TVITEMW data{};
    data.mask = TVIF_PARAM;
    data.hItem = item;
    TreeView_GetItem(g_search.tree, &data);
    return static_cast<int>(data.lParam);
}

// ---- Copy / export -------------------------------------------------------------------

std::wstring SearchResultLine(const SearchFileResult&, const SearchHit& hit) {
    return hit.line > 0 ? std::to_wstring(hit.line) + L":" + std::to_wstring(hit.column) + L": " + hit.lineText : hit.lineText;
}

std::wstring SearchResultsAsText() {
    long long matches = 0;
    for (const auto& file : g_search.results) matches += static_cast<long long>(file->hits.size());
    std::wstring text = L"搜索内容：" + g_search.lastQuery + L"\r\n搜索范围：" + g_search.lastFolders + L"\r\n" +
        L"命中文件：" + std::to_wstring(g_search.results.size()) + L"\r\n匹配次数：" + std::to_wstring(matches) + L"\r\n";
    for (const auto& file : g_search.results) {
        text += L"\r\n" + (file->path.empty() ? file->display : file->path) + L" (" + std::to_wstring(file->hits.size()) + L")\r\n";
        for (const SearchHit& hit : file->hits) {
            if (HitPassesFilter(*file, hit)) text += L"  " + SearchResultLine(*file, hit) + L"\r\n";
        }
    }
    return text;
}

std::wstring CsvField(const std::wstring& value) {
    std::wstring escaped = L"\"";
    for (const wchar_t c : value) {
        if (c == L'"') escaped += L'"';
        escaped += c;
    }
    return escaped + L"\"";
}

std::wstring JsonString(const std::wstring& value) {
    std::wstring escaped = L"\"";
    for (const wchar_t c : value) {
        switch (c) {
        case L'"': escaped += L"\\\""; break;
        case L'\\': escaped += L"\\\\"; break;
        case L'\n': escaped += L"\\n"; break;
        case L'\r': escaped += L"\\r"; break;
        case L'\t': escaped += L"\\t"; break;
        default:
            if (c < 0x20) {
                wchar_t buffer[8]{};
                swprintf_s(buffer, L"\\u%04x", static_cast<unsigned>(c));
                escaped += buffer;
            } else {
                escaped += c;
            }
        }
    }
    return escaped + L"\"";
}

void ExportSearchResults() {
    if (g_search.results.empty()) {
        ShowNotice(L"没有可以导出的搜索结果");
        return;
    }
    wchar_t fileName[MAX_PATH] = L"搜索结果.txt";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_app.window;
    dialog.lpstrFilter = L"文本文件 (*.txt)\0*.txt\0CSV 表格 (*.csv)\0*.csv\0JSON (*.json)\0*.json\0";
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    dialog.lpstrDefExt = L"txt";
    if (!GetSaveFileNameW(&dialog)) return;
    std::wstring extension = fs::path(fileName).extension().wstring();
    extension = LowerInvariant(extension);
    std::wstring content;
    if (extension == L".csv") {
        content = L"文件,行,列,匹配内容,行内容\r\n";
        for (const auto& file : g_search.results) {
            for (const SearchHit& hit : file->hits) {
                if (!HitPassesFilter(*file, hit)) continue;
                const std::wstring matched = hit.lineText.substr(std::min(hit.lineMatchStart, hit.lineText.size()), hit.length);
                content += CsvField(file->path.empty() ? file->display : file->path) + L"," + std::to_wstring(hit.line) + L"," +
                    std::to_wstring(hit.column) + L"," + CsvField(matched) + L"," + CsvField(hit.lineText) + L"\r\n";
            }
        }
    } else if (extension == L".json") {
        content = L"{\n  \"query\": " + JsonString(g_search.lastQuery) + L",\n  \"scope\": " + JsonString(g_search.lastFolders) + L",\n  \"results\": [";
        bool first = true;
        for (const auto& file : g_search.results) {
            for (const SearchHit& hit : file->hits) {
                if (!HitPassesFilter(*file, hit)) continue;
                const std::wstring matched = hit.lineText.substr(std::min(hit.lineMatchStart, hit.lineText.size()), hit.length);
                content += std::wstring(first ? L"\n" : L",\n") + L"    {\"file\": " + JsonString(file->path.empty() ? file->display : file->path) +
                    L", \"line\": " + std::to_wstring(hit.line) + L", \"column\": " + std::to_wstring(hit.column) +
                    L", \"match\": " + JsonString(matched) + L", \"text\": " + JsonString(hit.lineText) + L"}";
                first = false;
            }
        }
        content += L"\n  ]\n}\n";
    } else {
        content = SearchResultsAsText();
    }
    // UTF-8 with BOM so Excel opens the CSV with the right encoding.
    std::string bytes = "\xEF\xBB\xBF" + WideToUtf8(content);
    if (WriteAtomicFile(fileName, bytes)) ShowNotice(L"已导出到 " + fs::path(fileName).filename().wstring());
    else MessageBoxW(g_app.window, L"导出失败，请检查路径和权限。", kWindowTitle, MB_ICONERROR);
}

void ShowSearchErrorLog() {
    if (g_search.errors.empty()) {
        ShowNotice(L"本次搜索没有错误记录");
        return;
    }
    std::wstring text = L"搜索内容：" + g_search.lastQuery + L"\r\n搜索范围：" + g_search.lastFolders + L"\r\n\r\n";
    for (const std::wstring& line : g_search.errors) {
        const size_t tab = line.find(L'\t');
        text += tab == std::wstring::npos ? line : line.substr(0, tab) + L"    " + line.substr(tab + 1);
        text += L"\r\n";
    }
    const int index = CreateDocument({}, L"搜索错误日志", text, true, false, 0, 0, 0);
    if (index >= 0) SelectTab(index);
}

void ReplaceSelectedResultHere() {
    const int itemIndex = SelectedSearchItem();
    if (itemIndex < 0 || !g_search.lastMatcher) return;
    const SearchItemRef ref = g_search.items[static_cast<size_t>(itemIndex)];
    if (ref.file < 0) return;
    if (ref.hit < 0 || g_search.results[static_cast<size_t>(ref.file)]->hits[static_cast<size_t>(ref.hit)].line == 0) return;
    OpenSearchResult(itemIndex);
    TabDocument* document = ActiveDocument();
    if (!document) return;
    CHARRANGE selection{};
    SendMessageW(document->editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    std::wstring selected(static_cast<size_t>(selection.cpMax - selection.cpMin) + 1, L'\0');
    selected.resize(static_cast<size_t>(SendMessageW(document->editor, EM_GETSELTEXT, 0, reinterpret_cast<LPARAM>(selected.data()))));
    std::wstring replacement = g_search.lastMatcher->ReplaceOne(selected);
    std::replace(replacement.begin(), replacement.end(), L'\n', L'\r');
    SendMessageW(document->editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(replacement.c_str()));
    ShowNotice(L"已在编辑器中替换此处（未保存，可按 Ctrl+Z 撤销）");
}

void BookmarkSelectedSearchResult() {
    const int itemIndex = SelectedSearchItem();
    if (itemIndex < 0 || itemIndex >= static_cast<int>(g_search.items.size())) return;
    const SearchItemRef ref = g_search.items[static_cast<size_t>(itemIndex)];
    if (ref.file < 0 || ref.file >= static_cast<int>(g_search.results.size())) return;
    const SearchFileResult& file = *g_search.results[static_cast<size_t>(ref.file)];
    if (file.hits.empty() || file.hits.front().line <= 0) {
        ShowNotice(L"该结果没有可标记的行");
        return;
    }
    OpenSearchResult(itemIndex);
    TabDocument* document = ActiveDocument();
    if (!document) return;
    std::vector<int> lines;
    if (ref.hit >= 0) {
        lines.push_back(file.hits[static_cast<size_t>(ref.hit)].line);
    } else {
        for (const SearchHit& hit : file.hits) {
            if (hit.line > 0 && HitPassesFilter(file, hit)) lines.push_back(hit.line);
        }
    }
    for (int line : lines) {
        if (!HasBookmark(*document, line)) {
            document->bookmarks.insert(std::lower_bound(document->bookmarks.begin(), document->bookmarks.end(), line), line);
        }
    }
    NormalizeBookmarks(*document);
    g_app.stateDirty = true;
    RefreshGutter();
    ShowNotice(L"已将搜索结果标记为 " + std::to_wstring(lines.size()) + L" 个书签");
}

void ShowSearchTreeMenu(POINT screen) {
    int itemIndex = SelectedSearchItem();
    if (itemIndex >= 0 && g_search.items[static_cast<size_t>(itemIndex)].file == kLoadMoreItem) itemIndex = -1;
    const bool hasItem = itemIndex >= 0;
    const bool isHit = hasItem && g_search.items[static_cast<size_t>(itemIndex)].hit >= 0;
    const bool hasResults = !g_search.results.empty();
    const UINT itemState = hasItem ? 0 : MF_GRAYED;
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | itemState, 1, L"打开\tEnter");
    AppendMenuW(menu, MF_STRING | (isHit ? 0 : MF_GRAYED), 2, L"在编辑器中替换此处");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | itemState, 3, L"复制此条\tCtrl+C");
    AppendMenuW(menu, MF_STRING | itemState, 4, L"复制文件路径");
    AppendMenuW(menu, MF_STRING | (hasResults ? 0 : MF_GRAYED), 5, L"复制全部结果");
    AppendMenuW(menu, MF_STRING | (hasResults ? 0 : MF_GRAYED), 6, L"导出结果（TXT / CSV / JSON）...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (hasResults ? 0 : MF_GRAYED), 7, L"全部展开");
    AppendMenuW(menu, MF_STRING | (hasResults ? 0 : MF_GRAYED), 8, L"全部折叠");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (hasResults ? 0 : MF_GRAYED), 9, L"清空结果");
    AppendMenuW(menu, MF_STRING | ((isHit || hasItem) ? 0 : MF_GRAYED), 10, L"将选中结果标记为书签");
    const int command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screen.x, screen.y, g_app.window, nullptr);
    DestroyMenu(menu);
    const SearchItemRef ref = hasItem ? g_search.items[static_cast<size_t>(itemIndex)] : SearchItemRef{-1, -1, 0};
    switch (command) {
    case 1: OpenSearchResult(itemIndex); break;
    case 2: ReplaceSelectedResultHere(); break;
    case 3: {
        const SearchFileResult& file = *g_search.results[static_cast<size_t>(ref.file)];
        const std::wstring name = file.path.empty() ? file.display : file.path;
        CopyTextToClipboard(ref.hit < 0 ? name : name + L":" + SearchResultLine(file, file.hits[static_cast<size_t>(ref.hit)]));
        break;
    }
    case 4: {
        const SearchFileResult& file = *g_search.results[static_cast<size_t>(ref.file)];
        CopyTextToClipboard(file.path.empty() ? file.display : file.path);
        break;
    }
    case 5: CopyTextToClipboard(SearchResultsAsText()); ShowNotice(L"已复制全部搜索结果"); break;
    case 6: ExportSearchResults(); break;
    case 7:
    case 8:
        SendMessageW(g_search.tree, WM_SETREDRAW, FALSE, 0);
        for (size_t i = 0; i < g_search.fileNodes.size(); ++i) {
            if (!g_search.fileNodes[i]) continue;
            if (command == 7) PopulateFileNode(static_cast<int>(i));
            TreeView_Expand(g_search.tree, g_search.fileNodes[i], command == 7 ? TVE_EXPAND : TVE_COLLAPSE);
        }
        SendMessageW(g_search.tree, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(g_search.tree, nullptr, TRUE);
        break;
    case 9: ClearSearchResults(); break;
    case 10: BookmarkSelectedSearchResult(); break;
    default: break;
    }
}

// ---- Panel window ----------------------------------------------------------------

void ApplySearchPanelTheme() {
    if (!g_search.panel) return;
    const ThemePalette& palette = CurrentPalette();
    const wchar_t* dark = g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer";
    for (auto& [id, control] : g_search.controls) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        wchar_t className[32]{};
        GetClassNameW(control, className, 32);
        if (_wcsicmp(className, L"ComboBox") == 0) SetWindowTheme(control, g_app.darkTheme ? L"DarkMode_CFD" : nullptr, nullptr);
        else if (_wcsicmp(className, L"Button") == 0) {
            const LONG style = GetWindowLongW(control, GWL_STYLE) & BS_TYPEMASK;
            if (style == BS_AUTOCHECKBOX) SetWindowTheme(control, g_app.darkTheme ? L"" : nullptr, g_app.darkTheme ? L"" : nullptr);
            else SetWindowTheme(control, g_app.darkTheme ? dark : nullptr, nullptr);
        } else if (_wcsicmp(className, L"Edit") == 0) {
            SetWindowTheme(control, g_app.darkTheme ? L"DarkMode_CFD" : nullptr, nullptr);
        }
    }
    for (HWND label : g_search.labels) SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
    if (g_search.tree) {
        SetWindowTheme(g_search.tree, dark, nullptr);
        TreeView_SetBkColor(g_search.tree, palette.editorBackground);
        TreeView_SetTextColor(g_search.tree, palette.editorText);
        TreeView_SetLineColor(g_search.tree, palette.gutterText);
    }
    InvalidateRect(g_search.panel, nullptr, TRUE);
    RedrawWindow(g_search.panel, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_FRAME);
}

void LayoutSearchPanel() {
    if (!g_search.panel) return;
    RECT client{};
    GetClientRect(g_search.panel, &client);
    const int width = client.right;
    const int height = client.bottom;
    const int margin = Scale(12);
    const int rowHeight = Scale(30);
    const int fieldHeight = Scale(24);
    const int labelWidth = Scale(64);
    const int formWidth = std::clamp(width * 42 / 100, Scale(360), Scale(520));
    const int fieldLeft = margin + labelWidth;
    const int fieldWidth = formWidth - labelWidth - margin;
    const auto place = [&](int id, int x, int y, int w, int h) {
        if (HWND control = SearchControl(id)) MoveWindow(control, x, y, std::max(0, w), h, FALSE);
    };
    const auto placeLabel = [&](size_t index, int y) {
        if (index < g_search.labels.size()) MoveWindow(g_search.labels[index], margin, y + Scale(3), labelWidth - Scale(4), fieldHeight, FALSE);
    };
    place(kSearchClose, width - Scale(34), Scale(6), Scale(24), Scale(24));
    int y = Scale(36);
    placeLabel(0, y); place(kSearchFind, fieldLeft, y, fieldWidth, Scale(240)); y += rowHeight;
    placeLabel(1, y); place(kSearchReplace, fieldLeft, y, fieldWidth, Scale(240)); y += rowHeight;
    const int half = (fieldWidth - Scale(6)) / 2;
    placeLabel(2, y); place(kSearchScope, fieldLeft, y, half, Scale(200)); place(kSearchTarget, fieldLeft + half + Scale(6), y, fieldWidth - half - Scale(6), Scale(200)); y += rowHeight;
    placeLabel(3, y); place(kSearchFolder, fieldLeft, y, fieldWidth - Scale(34), Scale(240)); place(kSearchBrowse, fieldLeft + fieldWidth - Scale(28), y, Scale(28), fieldHeight); y += rowHeight;
    placeLabel(4, y); place(kSearchTypes, fieldLeft, y, fieldWidth, Scale(240)); y += rowHeight;
    placeLabel(5, y); place(kSearchExcludes, fieldLeft, y, fieldWidth, fieldHeight); y += rowHeight;
    placeLabel(6, y); place(kSearchMode, fieldLeft, y, half, Scale(200));
    {
        const int x = fieldLeft + half + Scale(6);
        const int rest = fieldWidth - half - Scale(6);
        place(kSearchMaxSize, x, y, rest / 2 - Scale(2), fieldHeight);
        place(kSearchMaxResults, x + rest / 2 + Scale(2), y, rest - rest / 2 - Scale(2), fieldHeight);
    }
    y += rowHeight;
    const int checkWidth = (formWidth - margin * 2) / 3;
    const int checks[] = {kSearchRecursive, kSearchHidden, kSearchMatchCase, kSearchWholeWord, kSearchUseExcludes, kSearchGitignore};
    for (int i = 0; i < 6; ++i) place(checks[i], margin + (i % 3) * checkWidth, y + (i / 3) * Scale(26), checkWidth - Scale(4), Scale(22));
    y += Scale(56);
    const int buttonWidth = (formWidth - margin * 2 - Scale(18)) / 4;
    const int buttons[] = {kSearchStart, kSearchStop, kSearchReplaceButton, kSearchClear};
    for (int i = 0; i < 4; ++i) place(buttons[i], margin + i * (buttonWidth + Scale(6)), y, buttonWidth, Scale(28));

    const int resultsLeft = formWidth + Scale(4);
    const int resultsWidth = width - resultsLeft - margin;
    if (g_search.labels.size() > 7) MoveWindow(g_search.labels[7], resultsLeft, Scale(36) + Scale(3), Scale(40), fieldHeight, FALSE);
    place(kSearchFilter, resultsLeft + Scale(40), Scale(36), std::min(Scale(220), resultsWidth / 3), fieldHeight);
    const int statsLeft = resultsLeft + Scale(40) + std::min(Scale(220), resultsWidth / 3) + Scale(12);
    place(kSearchStats, statsLeft, Scale(36) + Scale(3), width - statsLeft - margin, fieldHeight);
    place(kSearchTree, resultsLeft, Scale(66), resultsWidth, height - Scale(66) - Scale(8));
    InvalidateRect(g_search.panel, nullptr, TRUE);
}

LRESULT HandleSearchTreeCustomDraw(NMTVCUSTOMDRAW* draw) {
    switch (draw->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT: {
        const ThemePalette& palette = CurrentPalette();
        const int index = static_cast<int>(draw->nmcd.lItemlParam);
        const bool valid = index >= 0 && index < static_cast<int>(g_search.items.size());
        if (valid && g_search.items[static_cast<size_t>(index)].file == kLoadMoreItem) {
            draw->clrText = palette.accent;
            draw->clrTextBk = (draw->nmcd.uItemState & CDIS_SELECTED) ? palette.toolbarButtonPressed : palette.editorBackground;
            return CDRF_DODEFAULT;
        }
        const bool isFile = valid && g_search.items[static_cast<size_t>(index)].hit < 0;
        const bool selected = (draw->nmcd.uItemState & CDIS_SELECTED) != 0;
        draw->clrText = isFile ? palette.tabActiveText : palette.editorText;
        draw->clrTextBk = selected ? palette.toolbarButtonPressed : palette.editorBackground;
        if (isFile) {
            SelectObject(draw->nmcd.hdc, g_app.uiBoldFont);
            return CDRF_NEWFONT;
        }
        return CDRF_NOTIFYPOSTPAINT;
    }
    case CDDS_ITEMPOSTPAINT: {
        // Paint the matched part of the line on a highlight.
        const int index = static_cast<int>(draw->nmcd.lItemlParam);
        if (index < 0 || index >= static_cast<int>(g_search.items.size())) return CDRF_DODEFAULT;
        const SearchItemRef ref = g_search.items[static_cast<size_t>(index)];
        if (ref.hit < 0 || ref.file < 0) return CDRF_DODEFAULT;
        const SearchHit& hit = g_search.results[static_cast<size_t>(ref.file)]->hits[static_cast<size_t>(ref.hit)];
        const std::wstring prefix = hit.line > 0 ? std::to_wstring(hit.line) + L":" + std::to_wstring(hit.column) + L"    " : L"文件名    ";
        const std::wstring full = prefix + hit.lineText;
        const size_t start = prefix.size() + std::min(hit.lineMatchStart, hit.lineText.size());
        const size_t length = std::min(hit.length, full.size() - start);
        if (length == 0) return CDRF_DODEFAULT;
        RECT textRect{};
        if (!TreeView_GetItemRect(g_search.tree, reinterpret_cast<HTREEITEM>(draw->nmcd.dwItemSpec), &textRect, TRUE)) return CDRF_DODEFAULT;
        HDC dc = draw->nmcd.hdc;
        HGDIOBJ previous = SelectObject(dc, g_app.uiFont);
        SIZE whole{}, before{}, matched{};
        GetTextExtentPoint32W(dc, full.c_str(), static_cast<int>(full.size()), &whole);
        GetTextExtentPoint32W(dc, full.c_str(), static_cast<int>(start), &before);
        GetTextExtentPoint32W(dc, full.c_str() + start, static_cast<int>(length), &matched);
        const int padding = std::max(0, static_cast<int>((textRect.right - textRect.left) - whole.cx) / 2);
        RECT mark{textRect.left + padding + before.cx, textRect.top + 1, textRect.left + padding + before.cx + matched.cx, textRect.bottom - 1};
        mark.right = std::min(mark.right, textRect.right);
        if (mark.right > mark.left) {
            const ThemePalette& palette = CurrentPalette();
            FillSolid(dc, mark, Blend(palette.editorBackground, palette.attention, g_app.darkTheme ? 40 : 45));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, palette.tabActiveText);
            RECT textBox = mark;
            textBox.top = textRect.top;
            textBox.bottom = textRect.bottom;
            DrawTextW(dc, full.c_str() + start, static_cast<int>(length), &textBox, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
        }
        SelectObject(dc, previous);
        return CDRF_DODEFAULT;
    }
    default:
        return CDRF_DODEFAULT;
    }
}

HWND AddSearchControl(int id, const wchar_t* className, const wchar_t* text, DWORD style, DWORD exStyle = 0) {
    HWND control = CreateWindowExW(exStyle, className, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
        g_search.panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app.instance, nullptr);
    g_search.controls[id] = control;
    return control;
}

void AddSearchLabel(const wchar_t* text) {
    g_search.labels.push_back(CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 0, 0, 0, 0,
        g_search.panel, nullptr, g_app.instance, nullptr));
}

void CreateSearchPanelControls() {
    const DWORD combo = WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWN | CBS_AUTOHSCROLL;
    const DWORD list = WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST;
    const DWORD check = WS_TABSTOP | BS_AUTOCHECKBOX;
    for (const wchar_t* label : {L"查找", L"替换为", L"范围", L"目录", L"文件类型", L"排除", L"匹配方式", L"筛选"}) AddSearchLabel(label);
    AddSearchControl(kSearchFind, L"COMBOBOX", nullptr, combo);
    AddSearchControl(kSearchReplace, L"COMBOBOX", nullptr, combo);
    HWND scope = AddSearchControl(kSearchScope, L"COMBOBOX", nullptr, list);
    for (const wchar_t* item : {L"指定文件夹", L"所有打开的文件", L"当前文件"}) SendMessageW(scope, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    HWND target = AddSearchControl(kSearchTarget, L"COMBOBOX", nullptr, list);
    for (const wchar_t* item : {L"搜索文件内容", L"搜索文件名", L"搜索完整路径"}) SendMessageW(target, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    AddSearchControl(kSearchFolder, L"COMBOBOX", nullptr, combo);
    AddSearchControl(kSearchBrowse, L"BUTTON", L"…", WS_TABSTOP);
    AddSearchControl(kSearchTypes, L"COMBOBOX", nullptr, combo);
    AddSearchControl(kSearchExcludes, L"EDIT", g_search.excludes.c_str(), WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
    HWND mode = AddSearchControl(kSearchMode, L"COMBOBOX", nullptr, list);
    for (const wchar_t* item : {L"普通文本", L"正则表达式", L"任一关键词（OR）", L"全部关键词（AND）"}) SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
    AddSearchControl(kSearchMaxSize, L"EDIT", std::to_wstring(g_search.maxFileMb).c_str(), WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
    AddSearchControl(kSearchMaxResults, L"EDIT", std::to_wstring(g_search.maxResults).c_str(), WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
    SendMessageW(SearchControl(kSearchMaxSize), EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"文件上限 MB"));
    SendMessageW(SearchControl(kSearchMaxResults), EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"最多结果"));
    AddSearchControl(kSearchRecursive, L"BUTTON", L"包含子文件夹", check);
    AddSearchControl(kSearchHidden, L"BUTTON", L"包含隐藏文件", check);
    AddSearchControl(kSearchMatchCase, L"BUTTON", L"区分大小写", check);
    AddSearchControl(kSearchWholeWord, L"BUTTON", L"全词匹配", check);
    AddSearchControl(kSearchUseExcludes, L"BUTTON", L"使用排除规则", check);
    AddSearchControl(kSearchGitignore, L"BUTTON", L"遵循 .gitignore", check);
    AddSearchControl(kSearchStart, L"BUTTON", L"搜索", WS_TABSTOP | BS_DEFPUSHBUTTON);
    AddSearchControl(kSearchStop, L"BUTTON", L"停止", WS_TABSTOP);
    AddSearchControl(kSearchReplaceButton, L"BUTTON", L"替换…", WS_TABSTOP);
    AddSearchControl(kSearchClear, L"BUTTON", L"清空", WS_TABSTOP);
    AddSearchControl(kSearchFilter, L"EDIT", nullptr, WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
    SendMessageW(SearchControl(kSearchFilter), EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"在结果中筛选"));
    g_search.stats = AddSearchControl(kSearchStats, L"STATIC", nullptr, SS_NOTIFY | SS_NOPREFIX | SS_ENDELLIPSIS);
    g_search.tree = AddSearchControl(kSearchTree, WC_TREEVIEWW, nullptr,
        WS_TABSTOP | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT | TVS_NOHSCROLL);
    TreeView_SetExtendedStyle(g_search.tree, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
    AddSearchControl(kSearchClose, L"BUTTON", L"×", WS_TABSTOP | BS_OWNERDRAW);

    SendMessageW(scope, CB_SETCURSEL, g_search.optionScope, 0);
    SendMessageW(target, CB_SETCURSEL, g_search.optionTarget, 0);
    SendMessageW(mode, CB_SETCURSEL, g_search.optionMode, 0);
    const std::pair<int, bool> states[] = {{kSearchRecursive, g_search.optionRecursive}, {kSearchHidden, g_search.optionHidden},
        {kSearchMatchCase, g_search.optionMatchCase}, {kSearchWholeWord, g_search.optionWholeWord},
        {kSearchUseExcludes, g_search.optionUseExcludes}, {kSearchGitignore, g_search.optionGitignore}};
    for (const auto& [id, on] : states) SendMessageW(SearchControl(id), BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
    FillCombo(SearchControl(kSearchFind), g_search.findHistory);
    FillCombo(SearchControl(kSearchReplace), g_search.replaceHistory);
    FillCombo(SearchControl(kSearchFolder), g_search.folderHistory);
    FillCombo(SearchControl(kSearchTypes), g_search.typeHistory);
    SetWindowTextW(SearchControl(kSearchTypes), g_search.typeHistory.empty() ? kDefaultFileTypes : g_search.typeHistory.front().c_str());
    if (!g_search.folderHistory.empty()) SetWindowTextW(SearchControl(kSearchFolder), g_search.folderHistory.front().c_str());
    EnableWindow(SearchControl(kSearchStop), FALSE);

    HWND tip = CreateTooltip(g_search.panel);
    const std::pair<int, const wchar_t*> tips[] = {
        {kSearchFind, L"普通文本中可用 \\n 表示换行；关键词模式下用空格分隔多个关键词"},
        {kSearchReplace, L"正则表达式模式下可用 $1、$2 引用分组"},
        {kSearchFolder, L"多个文件夹用分号分隔"},
        {kSearchTypes, L"例如 *.js;*.ts;*.json，*.* 表示所有文本文件（自动跳过二进制文件）"},
        {kSearchExcludes, L"按文件名或文件夹名排除，分号分隔，支持 * 和 ?"},
        {kSearchMaxSize, L"大于此大小（MB）的文件会被跳过"},
        {kSearchMaxResults, L"匹配数量达到上限后停止搜索"},
        {kSearchStats, L"点击查看跳过和出错的文件"},
    };
    for (const auto& [id, text] : tips) {
        TOOLINFOW tool{sizeof(tool)};
        tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        tool.hwnd = g_search.panel;
        tool.uId = reinterpret_cast<UINT_PTR>(SearchControl(id));
        tool.lpszText = const_cast<wchar_t*>(text);
        SendMessageW(tip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
    }
}

void BrowseSearchFolder() {
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"选择要搜索的文件夹");
    const std::wstring current = SplitPatterns(GetWindowTextValue(SearchControl(kSearchFolder))).empty() ? std::wstring()
        : SplitPatterns(GetWindowTextValue(SearchControl(kSearchFolder))).front();
    IShellItem* folder = nullptr;
    if (!current.empty() && SUCCEEDED(SHCreateItemFromParsingName(current.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
        dialog->SetFolder(folder);
        folder->Release();
    }
    if (SUCCEEDED(dialog->Show(g_app.window))) {
        IShellItem* result = nullptr;
        if (SUCCEEDED(dialog->GetResult(&result))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                SetWindowTextW(SearchControl(kSearchFolder), path);
                CoTaskMemFree(path);
                SendMessageW(SearchControl(kSearchScope), CB_SETCURSEL, 0, 0);
            }
            result->Release();
        }
    }
    dialog->Release();
}

LRESULT CALLBACK SearchPanelProc(HWND panel, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_search.panel = panel;
        g_app.searchPanel = panel;
        CreateSearchPanelControls();
        ApplySearchPanelTheme();
        ApplyToolboxTheme();
        return 0;
    case WM_SIZE:
        LayoutSearchPanel();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        BufferedPaint paint(panel);
        const ThemePalette& palette = CurrentPalette();
        FillSolid(paint.dc(), paint.client(), palette.toolbarBackground);
        DrawHairline(paint.dc(), 0, 0, paint.client().right, 0, palette.panelBorder);
        SelectObject(paint.dc(), g_app.uiBoldFont);
        DrawLabel(paint.dc(), L"在文件中查找 / 替换", RECT{Scale(12), Scale(6), Scale(400), Scale(30)}, palette.toolbarText);
        return 0;
    }
    case WM_SETCURSOR: {
        POINT point{};
        GetCursorPos(&point);
        ScreenToClient(panel, &point);
        if (reinterpret_cast<HWND>(wParam) == panel && point.y < Scale(5)) {
            SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
            return TRUE;
        }
        break;
    }
    case WM_LBUTTONDOWN:
        if (GET_Y_LPARAM(lParam) < Scale(5)) {
            g_search.dragging = true;
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(panel, &point);
            g_search.dragStartY = point.y;
            g_search.dragStartHeight = g_app.searchPanelHeight;
            SetCapture(panel);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (g_search.dragging) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(panel, &point);
            g_app.searchPanelHeight = std::max(Scale(160), g_search.dragStartHeight - static_cast<int>(point.y - g_search.dragStartY));
            LayoutChildren(g_app.window);
        }
        return 0;
    case WM_LBUTTONUP:
        if (g_search.dragging) {
            g_search.dragging = false;
            ReleaseCapture();
            SaveSearchState();
        }
        return 0;
    case WM_TIMER:
        if (wParam == kTimerSearchProgress) {
            DrainSearchResults();
            UpdateSearchStats();
        }
        return 0;
    case kMessageSearchDone:
        FinishSearch(static_cast<int>(wParam));
        return 0;
    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (item && item->CtlID == kSearchClose) {
            const ThemePalette& palette = CurrentPalette();
            FillSolid(item->hDC, item->rcItem, palette.toolbarBackground);
            if (item->itemState & (ODS_SELECTED | ODS_FOCUS)) FillRounded(item->hDC, item->rcItem, Scale(4), palette.toolbarButtonBackground);
            DrawCross(item->hDC, (item->rcItem.left + item->rcItem.right) / 2, (item->rcItem.top + item->rcItem.bottom) / 2, Scale(5), palette.toolbarIcon);
            return TRUE;
        }
        break;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        const ThemePalette& palette = CurrentPalette();
        SetTextColor(dc, reinterpret_cast<HWND>(lParam) == g_search.stats ? palette.statusText : palette.toolbarText);
        SetBkColor(dc, palette.toolbarBackground);
        return reinterpret_cast<LRESULT>(g_app.toolbarBrush);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        const ThemePalette& palette = CurrentPalette();
        SetTextColor(dc, palette.controlText);
        SetBkColor(dc, palette.controlBackground);
        return reinterpret_cast<LRESULT>(g_app.controlBrush);
    }
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        if (id == IDOK) {  // Enter
            if (GetFocus() == g_search.tree) OpenSearchResult(SelectedSearchItem());
            else StartSearch(false);
            return 0;
        }
        if (id == IDCANCEL) {  // Esc: stop a running search, otherwise close the panel
            if (g_search.running) CancelSearch(false);
            else HideSearchPanel();
            return 0;
        }
        switch (id) {
        case kSearchStart: StartSearch(false); return 0;
        case kSearchStop: CancelSearch(false); return 0;
        case kSearchReplaceButton: StartSearch(true); return 0;
        case kSearchClear: ClearSearchResults(); return 0;
        case kSearchBrowse: BrowseSearchFolder(); return 0;
        case kSearchClose: HideSearchPanel(); return 0;
        case kSearchStats:
            if (code == STN_CLICKED && !g_search.errors.empty()) ShowSearchErrorLog();
            return 0;
        case kSearchFilter:
            if (code == EN_CHANGE) {
                g_search.filter = LowerInvariant(TrimCopy(GetWindowTextValue(SearchControl(kSearchFilter))));
                RebuildResultTree();
            }
            return 0;
        default: break;
        }
        return 0;
    }
    case WM_NOTIFY: {
        auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (header->hwndFrom != g_search.tree) break;
        if (header->code == NM_CUSTOMDRAW) return HandleSearchTreeCustomDraw(reinterpret_cast<NMTVCUSTOMDRAW*>(lParam));
        if (header->code == TVN_ITEMEXPANDINGW) {
            const auto* info = reinterpret_cast<NMTREEVIEWW*>(lParam);
            const int index = static_cast<int>(info->itemNew.lParam);
            if (info->action == TVE_EXPAND && index >= 0 && index < static_cast<int>(g_search.items.size()) &&
                g_search.items[static_cast<size_t>(index)].file >= 0) {
                PopulateFileNode(g_search.items[static_cast<size_t>(index)].file);
            }
            return FALSE;
        }
        if (header->code == NM_DBLCLK) {
            const int index = SelectedSearchItem();
            if (index >= 0 && (g_search.items[static_cast<size_t>(index)].hit >= 0 ||
                               g_search.items[static_cast<size_t>(index)].file == kLoadMoreItem)) {
                OpenSearchResult(index);
                return TRUE;  // don't toggle the node
            }
            return FALSE;
        }
        if (header->code == NM_RCLICK) {
            POINT point{};
            GetCursorPos(&point);
            TVHITTESTINFO hit{};
            hit.pt = point;
            ScreenToClient(g_search.tree, &hit.pt);
            if (HTREEITEM item = TreeView_HitTest(g_search.tree, &hit)) TreeView_SelectItem(g_search.tree, item);
            ShowSearchTreeMenu(point);
            return TRUE;
        }
        break;
    }
    default:
        break;
    }
    return DefWindowProcW(panel, message, wParam, lParam);
}

void EnsureSearchPanel() {
    if (g_search.panel) return;
    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{};
        klass.hInstance = g_app.instance;
        klass.lpfnWndProc = SearchPanelProc;
        klass.lpszClassName = L"MCSearchPanel";
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        registered = RegisterClassW(&klass) != 0;
    }
    CreateWindowExW(WS_EX_CONTROLPARENT, L"MCSearchPanel", nullptr, WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 0, 0, g_app.window, nullptr, g_app.instance, nullptr);
    RebuildSearchHistoryMenu();
}

void HideSearchPanel() {
    if (!g_search.panel) return;
    g_app.searchPanelVisible = false;
    ShowWindow(g_search.panel, SW_HIDE);
    LayoutChildren(g_app.window);
    if (TabDocument* document = ActiveDocument()) SetFocus(document->editor);
    SaveSearchState();
}

// Opens the panel; `focusReplace` puts the caret in the replace box.
void ShowSearchPanel(int scope, int target, int mode, bool focusReplace) {
    EnsureSearchPanel();
    if (!g_search.panel) return;
    if (scope >= 0) SendMessageW(SearchControl(kSearchScope), CB_SETCURSEL, scope, 0);
    if (target >= 0) SendMessageW(SearchControl(kSearchTarget), CB_SETCURSEL, target, 0);
    if (mode >= 0) SendMessageW(SearchControl(kSearchMode), CB_SETCURSEL, mode, 0);
    // A short single-line selection becomes the search text.
    if (TabDocument* document = ActiveDocument()) {
        CHARRANGE selection{};
        SendMessageW(document->editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
        const LONG length = selection.cpMax - selection.cpMin;
        if (length > 0 && length < 200) {
            std::wstring selected(static_cast<size_t>(length) + 1, L'\0');
            selected.resize(static_cast<size_t>(SendMessageW(document->editor, EM_GETSELTEXT, 0, reinterpret_cast<LPARAM>(selected.data()))));
            if (!selected.empty() && selected.find_first_of(L"\r\n") == std::wstring::npos) SetWindowTextW(SearchControl(kSearchFind), selected.c_str());
        }
        if (GetWindowTextLengthW(SearchControl(kSearchFolder)) == 0 && !document->untitled && !document->path.empty()) {
            SetWindowTextW(SearchControl(kSearchFolder), fs::path(document->path).parent_path().c_str());
        }
    }
    g_app.searchPanelVisible = true;
    ShowWindow(g_search.panel, SW_SHOW);
    LayoutChildren(g_app.window);
    HWND focus = SearchControl(focusReplace ? kSearchReplace : kSearchFind);
    SetFocus(focus);
    SendMessageW(focus, CB_SETEDITSEL, 0, MAKELPARAM(0, -1));
}

void ToggleSearchPanel() {
    if (g_app.searchPanelVisible) HideSearchPanel();
    else ShowSearchPanel(-1, -1, -1, false);
}

void RunSearchFromHistory(int index) {
    if (index < 0 || index >= static_cast<int>(g_search.findHistory.size())) return;
    const std::wstring query = g_search.findHistory[static_cast<size_t>(index)];
    ShowSearchPanel(-1, -1, -1, false);
    SetWindowTextW(SearchControl(kSearchFind), query.c_str());
    StartSearch(false);
}

// ===========================================================================
// Find / replace in files: preview, apply with backup, undo
// ===========================================================================

struct ReplacePreviewState {
    bool done = false;
    bool accepted = false;
    HWND list = nullptr;
    HWND view = nullptr;
    HWND summary = nullptr;
    HWND backup = nullptr;
    std::vector<bool> checked;
    bool backupChecked = true;
};

void UpdateReplacePreviewSummary(ReplacePreviewState& state) {
    int files = 0, replacements = 0, selectedFiles = 0, selectedReplacements = 0;
    for (size_t i = 0; i < g_search.results.size(); ++i) {
        const int count = g_search.results[i]->replacements;
        ++files;
        replacements += count;
        if (ListView_GetCheckState(state.list, static_cast<int>(i))) {
            ++selectedFiles;
            selectedReplacements += count;
        }
    }
    const std::wstring text = L"共找到 " + std::to_wstring(files) + L" 个文件、" + std::to_wstring(replacements) +
        L" 处可替换。将修改已勾选的 " + std::to_wstring(selectedFiles) + L" 个文件、" + std::to_wstring(selectedReplacements) + L" 处。";
    SetWindowTextW(state.summary, text.c_str());
    EnableWindow(GetDlgItem(GetParent(state.list), IDOK), selectedFiles > 0);
}

LRESULT CALLBACK ReplacePreviewProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<ReplacePreviewState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE:
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams));
        return 0;
    case WM_SIZE: {
        if (!state || !state->list) break;
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);
        const int bottom = height - Scale(48);
        const int listWidth = std::clamp(width * 38 / 100, Scale(240), Scale(420));
        MoveWindow(state->summary, Scale(16), Scale(12), width - Scale(32), Scale(22), TRUE);
        MoveWindow(state->list, Scale(16), Scale(40), listWidth, std::max(0, bottom - Scale(48)), TRUE);
        MoveWindow(state->view, Scale(24) + listWidth, Scale(40), width - listWidth - Scale(40), std::max(0, bottom - Scale(48)), TRUE);
        ListView_SetColumnWidth(state->list, 0, listWidth - Scale(80));
        MoveWindow(GetDlgItem(window, 201), Scale(16), bottom + Scale(10), Scale(70), Scale(28), TRUE);
        MoveWindow(GetDlgItem(window, 202), Scale(92), bottom + Scale(10), Scale(70), Scale(28), TRUE);
        MoveWindow(state->backup, Scale(176), bottom + Scale(12), Scale(280), Scale(24), TRUE);
        MoveWindow(GetDlgItem(window, IDOK), width - Scale(16 + 82 + 8 + 120), bottom + Scale(10), Scale(120), Scale(28), TRUE);
        MoveWindow(GetDlgItem(window, IDCANCEL), width - Scale(16 + 82), bottom + Scale(10), Scale(82), Scale(28), TRUE);
        return 0;
    }
    case WM_NOTIFY: {
        auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (state && header->hwndFrom == state->list && header->code == LVN_ITEMCHANGED) {
            const auto* change = reinterpret_cast<NMLISTVIEW*>(lParam);
            if ((change->uChanged & LVIF_STATE) && ((change->uNewState ^ change->uOldState) & LVIS_STATEIMAGEMASK)) {
                UpdateReplacePreviewSummary(*state);
            }
            if ((change->uChanged & LVIF_STATE) && (change->uNewState & LVIS_SELECTED) && !(change->uOldState & LVIS_SELECTED)) {
                const SearchFileResult& file = *g_search.results[static_cast<size_t>(change->iItem)];
                int removed = 0, added = 0;
                FillDiffView(state->view, file.before, file.after, removed, added);
            }
        }
        break;
    }
    case WM_COMMAND:
        if (!state) break;
        if (LOWORD(wParam) == 201 || LOWORD(wParam) == 202) {
            for (int i = 0; i < ListView_GetItemCount(state->list); ++i) ListView_SetCheckState(state->list, i, LOWORD(wParam) == 201);
            return 0;
        }
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            state->accepted = LOWORD(wParam) == IDOK;
            state->checked.clear();
            for (int i = 0; i < ListView_GetItemCount(state->list); ++i) state->checked.push_back(ListView_GetCheckState(state->list, i) != 0);
            state->backupChecked = SendMessageW(state->backup, BM_GETCHECK, 0, 0) == BST_CHECKED;
            state->done = true;
            return 0;
        }
        break;
    case WM_CLOSE:
        if (state) state->done = true;
        return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = POINT{Scale(640), Scale(360)};
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

std::wstring NowStamp() {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%04u%02u%02u-%02u%02u%02u", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
    return buffer;
}

fs::path ReplaceBackupRoot() { return g_app.dataDirectory / L"replace-backups"; }

// Keeps the ten most recent backup folders.
void PruneReplaceBackups() {
    std::error_code error;
    std::vector<fs::path> folders;
    for (fs::directory_iterator it(ReplaceBackupRoot(), error), end; !error && it != end; it.increment(error)) {
        if (it->is_directory(error)) folders.push_back(it->path());
    }
    std::sort(folders.begin(), folders.end());
    while (folders.size() > 10) {
        fs::remove_all(folders.front(), error);
        folders.erase(folders.begin());
    }
}

void ExecuteReplaceInFiles(const std::vector<bool>& checked, bool backup) {
    SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    const fs::path backupDirectory = ReplaceBackupRoot() / NowStamp();
    if (backup && !EnsureDirectory(backupDirectory)) {
        MessageBoxW(g_app.window, L"无法创建备份文件夹，已取消替换。", kWindowTitle, MB_ICONERROR);
        return;
    }
    std::ostringstream manifest;
    std::vector<std::wstring> failures;
    int filesDone = 0, replacementsDone = 0, backupIndex = 0;
    std::unordered_map<std::wstring, bool> handled;  // never touch one file twice

    for (size_t i = 0; i < g_search.results.size() && i < checked.size(); ++i) {
        if (!checked[i]) continue;
        SearchFileResult& file = *g_search.results[i];
        const std::wstring name = file.path.empty() ? file.display : file.path;
        if (!handled.emplace(LowerInvariant(name), true).second) continue;

        if (file.editor) {  // open-tab scopes: change the editor text, undoable there
            const int index = IndexOfEditor(file.editor);
            if (index < 0) {
                failures.push_back(name + L"：标签已关闭");
                continue;
            }
            const std::wstring current = [&] { std::wstring t = GetEditorRawText(file.editor); std::replace(t.begin(), t.end(), L'\r', L'\n'); return t; }();
            if (current != file.before) {
                failures.push_back(name + L"：内容在搜索后被修改，已跳过");
                continue;
            }
            std::wstring text = file.after;
            std::replace(text.begin(), text.end(), L'\n', L'\r');
            SendMessageW(file.editor, EM_SETSEL, 0, -1);
            SendMessageW(file.editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
            ++filesDone;
            replacementsDone += file.replacements;
            continue;
        }

        int openIndex = -1;
        for (size_t d = 0; d < g_app.documents.size(); ++d) {
            if (!g_app.documents[d].untitled && SamePath(g_app.documents[d].path, file.path)) openIndex = static_cast<int>(d);
        }
        if (openIndex >= 0 && g_app.documents[static_cast<size_t>(openIndex)].modified) {
            failures.push_back(name + L"：已在编辑器中打开且有未保存的修改，已跳过");
            continue;
        }
        std::error_code error;
        const uint64_t size = fs::file_size(file.path, error);
        if (error || size != file.size || FileTimestamp(file.path) != file.timestamp) {
            failures.push_back(name + L"：文件在搜索后被修改或已删除，已跳过");
            continue;
        }
        // Keep the file's own line breaks and encoding.
        std::wstring text;
        text.reserve(file.after.size() + file.after.size() / 16);
        for (const wchar_t c : file.after) {
            if (c == L'\n') text += file.eol;
            else text += c;
        }
        if (file.encoding == kEncodingAnsi && !IsAnsiRepresentable(text)) {
            failures.push_back(name + L"：替换后的内容包含 ANSI 编码无法表示的字符，已跳过");
            continue;
        }
        std::string bytes;
        if (!EncodeTextBytes(text, file.encoding, bytes)) {
            failures.push_back(name + L"：编码转换失败，已跳过");
            continue;
        }
        std::wstring backupName;
        if (backup) {
            backupName = std::to_wstring(++backupIndex) + L".bak";
            if (!CopyFileW(file.path.c_str(), (backupDirectory / backupName).c_str(), FALSE)) {
                failures.push_back(name + L"：备份失败（错误 " + std::to_wstring(GetLastError()) + L"），未修改");
                continue;
            }
        }
        if (!WriteAtomicFile(file.path, bytes)) {
            failures.push_back(name + L"：写入失败（文件可能只读或正被占用，错误 " + std::to_wstring(GetLastError()) + L"）");
            continue;
        }
        ++filesDone;
        replacementsDone += file.replacements;
        if (backup) {
            manifest << HexEncode(WideToUtf8(file.path)) << '\t' << HexEncode(WideToUtf8(backupName)) << '\t'
                     << bytes.size() << '\t' << FileTimestamp(file.path) << '\n';
        }
        if (openIndex >= 0) ReloadDocumentFromDisk(openIndex);
    }

    if (backup && backupIndex > 0) {
        WriteAtomicFile(backupDirectory / L"manifest.txt", manifest.str());
        g_search.lastBackupDirectory = backupDirectory.wstring();
        PruneReplaceBackups();
    } else if (backup) {
        std::error_code error;
        fs::remove_all(backupDirectory, error);
    }
    for (const std::wstring& failure : failures) g_search.errors.push_back(failure);
    g_search.resultsStale = true;
    SaveSearchState();
    UpdateSearchStats();

    std::wstring message = L"已替换 " + std::to_wstring(filesDone) + L" 个文件，共 " + std::to_wstring(replacementsDone) + L" 处。";
    if (backup && backupIndex > 0) message += L"\n原文件已备份，可通过“搜索 → 搜索结果管理 → 撤销上次在文件中替换”恢复。";
    if (!failures.empty()) {
        message += L"\n\n以下 " + std::to_wstring(failures.size()) + L" 个文件未修改：\n";
        for (size_t i = 0; i < failures.size() && i < 12; ++i) message += L"• " + failures[i] + L"\n";
        if (failures.size() > 12) message += L"……（完整列表见“查看错误日志”）";
    }
    MessageBoxW(g_app.window, message.c_str(), kWindowTitle, failures.empty() ? MB_ICONINFORMATION : MB_ICONWARNING);
}

void ShowReplaceInFilesPreview() {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{};
        klass.hInstance = g_app.instance;
        klass.lpfnWndProc = ReplacePreviewProc;
        klass.lpszClassName = L"MCnotepadReplacePreview";
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        registered = RegisterClassW(&klass) != 0;
    }
    ReplacePreviewState state;
    RECT owner{};
    GetWindowRect(g_app.window, &owner);
    const int width = std::min(Scale(980), static_cast<int>(owner.right - owner.left) - Scale(20));
    const int height = std::min(Scale(620), static_cast<int>(owner.bottom - owner.top) - Scale(20));
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"MCnotepadReplacePreview", L"预览在文件中替换",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        owner.left + (owner.right - owner.left - width) / 2, owner.top + (owner.bottom - owner.top - height) / 2,
        width, height, g_app.window, nullptr, g_app.instance, &state);
    if (!dialog) return;
    ApplyWindowFrameTheme(dialog);
    const ThemePalette& palette = CurrentPalette();
    state.summary = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 0, 0, 0, 0, dialog, nullptr, g_app.instance, nullptr);
    state.list = CreateWindowExW(0, WC_LISTVIEWW, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        0, 0, 0, 0, dialog, nullptr, g_app.instance, nullptr);
    ListView_SetExtendedListViewStyle(state.list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    state.view = CreateWindowExW(0, L"RICHEDIT50W", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, 0, 0, 0, dialog, nullptr, g_app.instance, nullptr);
    HWND all = CreateWindowExW(0, L"BUTTON", L"全选", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(201), g_app.instance, nullptr);
    HWND none = CreateWindowExW(0, L"BUTTON", L"全不选", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(202), g_app.instance, nullptr);
    state.backup = CreateWindowExW(0, L"BUTTON", L"替换前备份原文件（可撤销）", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 0, 0, dialog, nullptr, g_app.instance, nullptr);
    HWND apply = CreateWindowExW(0, L"BUTTON", L"替换勾选的文件", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(IDOK), g_app.instance, nullptr);
    HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(IDCANCEL), g_app.instance, nullptr);
    for (HWND control : {state.summary, state.list, all, none, state.backup, apply, cancel}) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
    }
    SendMessageW(state.backup, BM_SETCHECK, BST_CHECKED, 0);
    ThemeCheckbox(state.backup);
    ThemePushButtons({all, none, apply, cancel});
    SetWindowTheme(state.list, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    ListView_SetBkColor(state.list, palette.editorBackground);
    ListView_SetTextBkColor(state.list, palette.editorBackground);
    ListView_SetTextColor(state.list, palette.editorText);
    if (HWND headerControl = ListView_GetHeader(state.list)) SetWindowTheme(headerControl, g_app.darkTheme ? L"DarkMode_ItemsView" : nullptr, nullptr);
    SendMessageW(state.view, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
    SetWindowTheme(state.view, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    SendMessageW(state.view, EM_SETBKGNDCOLOR, 0, palette.editorBackground);
    SendMessageW(state.view, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);

    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = const_cast<wchar_t*>(L"文件");
    column.cx = Scale(260);
    ListView_InsertColumn(state.list, 0, &column);
    column.pszText = const_cast<wchar_t*>(L"处数");
    column.cx = Scale(56);
    ListView_InsertColumn(state.list, 1, &column);
    for (size_t i = 0; i < g_search.results.size(); ++i) {
        const SearchFileResult& file = *g_search.results[i];
        std::wstring label = file.display;
        if (file.mixedEol) label += L"（换行混用，将统一为 " + std::wstring(file.eol == L"\r\n" ? L"CRLF" : file.eol == L"\n" ? L"LF" : L"CR") + L"）";
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.pszText = label.data();
        ListView_InsertItem(state.list, &item);
        std::wstring count = std::to_wstring(file.replacements);
        ListView_SetItemText(state.list, static_cast<int>(i), 1, count.data());
        ListView_SetCheckState(state.list, static_cast<int>(i), TRUE);
    }
    RECT client{};
    GetClientRect(dialog, &client);
    SendMessageW(dialog, WM_SIZE, 0, MAKELPARAM(client.right, client.bottom));
    UpdateReplacePreviewSummary(state);
    ListView_SetItemState(state.list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

    EnableWindow(g_app.window, FALSE);
    ShowWindow(dialog, SW_SHOW);
    SetFocus(state.list);
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
    if (!state.accepted) return;
    if (!state.backupChecked && MessageBoxW(g_app.window, L"不备份原文件将无法撤销这次替换。确定继续吗？", kWindowTitle,
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    ExecuteReplaceInFiles(state.checked, state.backupChecked);
}

void UndoLastReplaceInFiles() {
    const fs::path directory = g_search.lastBackupDirectory;
    std::vector<char> bytes;
    if (directory.empty() || !ReadBinaryFile(directory / L"manifest.txt", bytes)) {
        MessageBoxW(g_app.window, L"没有可以撤销的“在文件中替换”。", kWindowTitle, MB_ICONINFORMATION);
        return;
    }
    struct Entry { std::wstring path; std::wstring backup; uint64_t size; uint64_t timestamp; };
    std::vector<Entry> entries;
    std::istringstream stream(std::string(bytes.begin(), bytes.end()));
    for (std::string line; std::getline(stream, line);) {
        const std::vector<std::string> parts = SplitTabs(line);
        if (parts.size() < 4) continue;
        int64_t size = 0, timestamp = 0;
        ParseInt64(parts[2], size);
        ParseInt64(parts[3], timestamp);
        entries.push_back({Utf8ToWide(HexDecode(parts[0])), Utf8ToWide(HexDecode(parts[1])), static_cast<uint64_t>(size), static_cast<uint64_t>(timestamp)});
    }
    int changedSince = 0;
    for (const Entry& entry : entries) {
        std::error_code error;
        if (fs::file_size(entry.path, error) != entry.size || error || FileTimestamp(entry.path) != entry.timestamp) ++changedSince;
    }
    std::wstring prompt = L"将把上次替换的 " + std::to_wstring(entries.size()) + L" 个文件恢复为替换前的内容。";
    if (changedSince > 0) prompt += L"\n\n注意：其中 " + std::to_wstring(changedSince) + L" 个文件在替换后又被修改过，恢复会覆盖这些修改。";
    prompt += L"\n\n确定恢复吗？";
    if (MessageBoxW(g_app.window, prompt.c_str(), kWindowTitle, MB_YESNO | MB_ICONQUESTION | (changedSince ? MB_DEFBUTTON2 : 0)) != IDYES) return;

    int restored = 0;
    std::vector<std::wstring> failures;
    for (const Entry& entry : entries) {
        if (!CopyFileW((directory / entry.backup).c_str(), entry.path.c_str(), FALSE)) {
            failures.push_back(entry.path + L"（错误 " + std::to_wstring(GetLastError()) + L"）");
            continue;
        }
        ++restored;
        for (size_t d = 0; d < g_app.documents.size(); ++d) {
            const TabDocument& document = g_app.documents[d];
            if (!document.untitled && !document.modified && SamePath(document.path, entry.path)) ReloadDocumentFromDisk(static_cast<int>(d));
        }
    }
    std::error_code error;
    fs::rename(directory / L"manifest.txt", directory / L"manifest.undone.txt", error);
    g_search.lastBackupDirectory.clear();
    g_search.resultsStale = true;
    SaveSearchState();
    UpdateSearchStats();
    std::wstring message = L"已恢复 " + std::to_wstring(restored) + L" 个文件。";
    if (!failures.empty()) {
        message += L"\n\n恢复失败：\n";
        for (size_t i = 0; i < failures.size() && i < 12; ++i) message += L"• " + failures[i] + L"\n";
    }
    MessageBoxW(g_app.window, message.c_str(), kWindowTitle, failures.empty() ? MB_ICONINFORMATION : MB_ICONWARNING);
}

// ===========================================================================
// Developer toolbox: a sidebar that works on the editor's selection.
// ===========================================================================

enum ToolboxTool : int {
    kToolJson = 0, kToolBase64, kToolUrl, kToolJwt, kToolTimestamp, kToolHash, kToolRegex, kToolUnicode, kToolHtml, kToolUuid, kToolCount
};

const wchar_t* const kToolboxNames[kToolCount] = {
    L"JSON", L"Base64", L"URL", L"JWT 解码", L"时间戳", L"哈希 (MD5/SHA)", L"正则测试", L"Unicode 转义", L"HTML 实体", L"UUID 生成"};

std::vector<std::wstring> ToolboxActions(int tool) {
    switch (tool) {
    case kToolJson: return {L"格式化", L"压缩为一行", L"校验"};
    case kToolBase64: return {L"编码", L"解码"};
    case kToolUrl: return {L"编码", L"解码", L"解析 URL"};
    case kToolUnicode: return {L"转义 (\\uXXXX)", L"还原"};
    case kToolHtml: return {L"编码", L"解码"};
    default: return {};
    }
}

enum ToolboxControl : int {
    kTbTool = 4401, kTbAction, kTbPattern, kTbReplace, kTbIgnoreCase, kTbFollow, kTbTake, kTbInput, kTbOutput, kTbCopy, kTbApply,
    kTbStatus, kTbClose, kTbCustomize, kTbPatternLabel, kTbReplaceLabel, kTbInputLabel, kTbOutputLabel, kTbTitle,
};

struct ToolboxState {
    HWND panel = nullptr;
    HWND tool = nullptr, action = nullptr, pattern = nullptr, replace = nullptr, ignoreCase = nullptr, follow = nullptr;
    HWND input = nullptr, output = nullptr, status = nullptr;
    std::vector<int> shown;      // tools in the dropdown
    bool dragging = false;
    int dragStartX = 0, dragStartWidth = 0;
    bool settingInput = false;
} g_toolbox;

constexpr UINT_PTR kTimerToolboxRun = 1;
void ToggleToolbox(bool show);
constexpr UINT_PTR kTimerToolboxSelection = 2;

std::wstring TrimSpace(std::wstring_view text) {
    size_t first = 0, last = text.size();
    while (first < last && (iswspace(text[first]) || text[first] == 0x3000)) ++first;
    while (last > first && (iswspace(text[last - 1]) || text[last - 1] == 0x3000)) --last;
    return std::wstring(text.substr(first, last - first));
}

std::wstring ToCrLf(std::wstring_view text) {
    std::wstring out;
    out.reserve(text.size() + text.size() / 16);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') { out += L"\r\n"; if (i + 1 < text.size() && text[i + 1] == L'\n') ++i; }
        else if (text[i] == L'\n') out += L"\r\n";
        else out += text[i];
    }
    return out;
}

// ---- The tools ---------------------------------------------------------------------------

std::wstring HashHex(const std::string& bytes, ALG_ID algorithm) {
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    std::wstring result;
    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return L"（不可用）";
    if (CryptCreateHash(provider, algorithm, 0, 0, &hash)) {
        bool ok = true;
        for (size_t offset = 0; offset < bytes.size() && ok; offset += 1u << 30) {
            const DWORD chunk = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, 1u << 30));
            ok = CryptHashData(hash, reinterpret_cast<const BYTE*>(bytes.data() + offset), chunk, 0) != FALSE;
        }
        DWORD size = 0, sizeLength = sizeof(size);
        if (ok && CryptGetHashParam(hash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&size), &sizeLength, 0)) {
            std::vector<BYTE> value(size);
            if (CryptGetHashParam(hash, HP_HASHVAL, value.data(), &size, 0)) {
                static const wchar_t hex[] = L"0123456789abcdef";
                for (const BYTE b : value) { result += hex[b >> 4]; result += hex[b & 15]; }
            }
        }
        CryptDestroyHash(hash);
    }
    CryptReleaseContext(provider, 0);
    return result;
}

// Strict JSON (RFC 8259) check with a line/column message.
class JsonValidator {
public:
    explicit JsonValidator(std::wstring_view text) : text_(text) {}
    bool Run(std::wstring& error) {
        Space();
        if (position_ < text_.size() && text_[position_] == 0xFEFF) { ++position_; Space(); }
        if (!Value(0)) { error = Where() + message_; return false; }
        Space();
        if (position_ < text_.size()) { error = Where() + L"JSON 结束后还有多余的内容"; return false; }
        return true;
    }

private:
    std::wstring Where() const {
        size_t line = 1, column = 1;
        for (size_t i = 0; i < position_ && i < text_.size(); ++i) {
            if (text_[i] == L'\n') { ++line; column = 1; } else if (text_[i] != L'\r') ++column;
        }
        return L"第 " + std::to_wstring(line) + L" 行第 " + std::to_wstring(column) + L" 列：";
    }
    void Space() { while (position_ < text_.size() && (text_[position_] == L' ' || text_[position_] == L'\t' || text_[position_] == L'\n' || text_[position_] == L'\r')) ++position_; }
    bool Fail(const wchar_t* message) { message_ = message; return false; }
    bool Literal(const wchar_t* word) {
        const size_t n = wcslen(word);
        if (text_.substr(position_, n) != word) return Fail(L"无法识别的值");
        position_ += n;
        return true;
    }
    bool String() {
        ++position_;
        while (position_ < text_.size()) {
            const wchar_t c = text_[position_];
            if (c == L'"') { ++position_; return true; }
            if (c < 0x20) return Fail(L"字符串中不能直接包含换行或控制字符");
            if (c == L'\\') {
                if (++position_ >= text_.size()) break;
                const wchar_t e = text_[position_];
                if (e == L'u') {
                    for (int k = 1; k <= 4; ++k) if (position_ + k >= text_.size() || !iswxdigit(text_[position_ + k])) return Fail(L"\\u 后应为 4 位十六进制数");
                    position_ += 4;
                } else if (!wcschr(L"\"\\/bfnrt", e)) {
                    return Fail(L"无效的转义字符");
                }
            }
            ++position_;
        }
        return Fail(L"字符串没有结束");
    }
    bool Number() {
        const size_t start = position_;
        if (text_[position_] == L'-') ++position_;
        if (position_ >= text_.size() || !iswdigit(text_[position_])) return Fail(L"无效的数字");
        if (text_[position_] == L'0' && position_ + 1 < text_.size() && iswdigit(text_[position_ + 1])) return Fail(L"数字不能以 0 开头");
        while (position_ < text_.size() && iswdigit(text_[position_])) ++position_;
        if (position_ < text_.size() && text_[position_] == L'.') {
            ++position_;
            if (position_ >= text_.size() || !iswdigit(text_[position_])) return Fail(L"小数点后应有数字");
            while (position_ < text_.size() && iswdigit(text_[position_])) ++position_;
        }
        if (position_ < text_.size() && (text_[position_] == L'e' || text_[position_] == L'E')) {
            ++position_;
            if (position_ < text_.size() && (text_[position_] == L'+' || text_[position_] == L'-')) ++position_;
            if (position_ >= text_.size() || !iswdigit(text_[position_])) return Fail(L"指数部分应有数字");
            while (position_ < text_.size() && iswdigit(text_[position_])) ++position_;
        }
        return position_ > start;
    }
    bool Value(int depth) {
        if (depth > 512) return Fail(L"嵌套层数太深");
        Space();
        if (position_ >= text_.size()) return Fail(L"内容不完整");
        const wchar_t c = text_[position_];
        if (c == L'{' || c == L'[') {
            const bool object = c == L'{';
            ++position_;
            Space();
            if (position_ < text_.size() && text_[position_] == (object ? L'}' : L']')) { ++position_; return true; }
            while (true) {
                Space();
                if (object) {
                    if (position_ < text_.size() && text_[position_] == L'/') return Fail(L"标准 JSON 不支持注释");
                    if (position_ >= text_.size() || text_[position_] != L'"') return Fail(L"应为用双引号括起来的键名");
                    if (!String()) return false;
                    Space();
                    if (position_ >= text_.size() || text_[position_] != L':') return Fail(L"键名后缺少冒号");
                    ++position_;
                }
                if (!Value(depth + 1)) return false;
                Space();
                if (position_ < text_.size() && text_[position_] == L',') {
                    ++position_;
                    Space();
                    if (position_ < text_.size() && text_[position_] == (object ? L'}' : L']')) return Fail(L"末尾多了一个逗号");
                    continue;
                }
                if (position_ < text_.size() && text_[position_] == (object ? L'}' : L']')) { ++position_; return true; }
                return Fail(object ? L"缺少逗号或 }" : L"缺少逗号或 ]");
            }
        }
        if (c == L'"') return String();
        if (c == L'-' || iswdigit(c)) return Number();
        if (c == L't') return Literal(L"true");
        if (c == L'f') return Literal(L"false");
        if (c == L'n') return Literal(L"null");
        if (c == L'/') return Fail(L"标准 JSON 不支持注释");
        return Fail(L"无法识别的值（字符串必须用双引号）");
    }

    std::wstring_view text_;
    size_t position_ = 0;
    std::wstring message_;
};

std::wstring CompactJson(std::wstring_view input, std::wstring& error) {
    std::wstring formatted;
    if (!FormatJson(input, formatted, error)) return {};
    std::wstring out;
    bool inString = false;
    for (size_t i = 0; i < formatted.size(); ++i) {
        const wchar_t c = formatted[i];
        if (inString) {
            out += c;
            if (c == L'\\' && i + 1 < formatted.size()) out += formatted[++i];
            else if (c == L'"') inString = false;
            continue;
        }
        if (c == L'"') { inString = true; out += c; continue; }
        if (!iswspace(c)) out += c;
    }
    return out;
}

std::wstring FormatUnixTime(int64_t seconds, int milliseconds) {
    const int64_t ticks = (seconds + 11644473600LL) * 10000000LL + milliseconds * 10000LL;
    if (ticks < 0) return L"（超出范围）";
    FILETIME utc{static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
    SYSTEMTIME u{}, l{};
    if (!FileTimeToSystemTime(&utc, &u)) return L"（超出范围）";
    SystemTimeToTzSpecificLocalTime(nullptr, &u, &l);
    // Local offset from the two clocks.
    FILETIME localFile{};
    SystemTimeToFileTime(&l, &localFile);
    const int64_t localTicks = (static_cast<int64_t>(localFile.dwHighDateTime) << 32) | localFile.dwLowDateTime;
    const int offsetMinutes = static_cast<int>((localTicks - ticks) / 600000000LL);
    wchar_t text[256]{};
    swprintf_s(text, L"本地时间：%04u-%02u-%02u %02u:%02u:%02u.%03u\r\nUTC：%04u-%02u-%02u %02u:%02u:%02u.%03u\r\nISO 8601：%04u-%02u-%02uT%02u:%02u:%02u%c%02d:%02d",
        l.wYear, l.wMonth, l.wDay, l.wHour, l.wMinute, l.wSecond, l.wMilliseconds,
        u.wYear, u.wMonth, u.wDay, u.wHour, u.wMinute, u.wSecond, u.wMilliseconds,
        l.wYear, l.wMonth, l.wDay, l.wHour, l.wMinute, l.wSecond, offsetMinutes < 0 ? L'-' : L'+', std::abs(offsetMinutes) / 60, std::abs(offsetMinutes) % 60);
    // Relative to now.
    FILETIME nowFile{};
    GetSystemTimeAsFileTime(&nowFile);
    const int64_t now = (((static_cast<int64_t>(nowFile.dwHighDateTime) << 32) | nowFile.dwLowDateTime) / 10000000LL) - 11644473600LL;
    const int64_t delta = now - seconds;
    const int64_t magnitude = std::abs(delta);
    std::wstring relative = magnitude < 60 ? std::to_wstring(magnitude) + L" 秒" : magnitude < 3600 ? std::to_wstring(magnitude / 60) + L" 分钟"
        : magnitude < 86400 ? std::to_wstring(magnitude / 3600) + L" 小时" : std::to_wstring(magnitude / 86400) + L" 天";
    return std::wstring(text) + L"\r\n相对现在：" + relative + (delta >= 0 ? L"前" : L"后");
}

int64_t UnixNow() {
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    return (((static_cast<int64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime) / 10000000LL) - 11644473600LL;
}

// Number → dates; date text → numbers; empty → now.
bool RunTimestamp(const std::wstring& rawInput, std::wstring& output, std::wstring& status) {
    const std::wstring input = TrimSpace(rawInput);
    if (input.empty()) {
        const int64_t now = UnixNow();
        output = L"当前时间\r\n秒：" + std::to_wstring(now) + L"\r\n毫秒：" + std::to_wstring(now * 1000) + L"\r\n" + FormatUnixTime(now, 0);
        status = L"输入时间戳或日期即可转换";
        return true;
    }
    if (std::all_of(input.begin(), input.end(), [](wchar_t c) { return iswdigit(c) || c == L'-'; }) && input.find(L'-', 1) == std::wstring::npos) {
        int64_t value = _wtoi64(input.c_str());
        std::wstring unit = L"秒";
        int64_t seconds = value, millis = 0;
        const int64_t magnitude = std::abs(value);
        if (magnitude >= 100000000000000000LL) { unit = L"纳秒"; seconds = value / 1000000000; millis = (value / 1000000) % 1000; }
        else if (magnitude >= 100000000000000LL) { unit = L"微秒"; seconds = value / 1000000; millis = (value / 1000) % 1000; }
        else if (magnitude >= 100000000000LL) { unit = L"毫秒"; seconds = value / 1000; millis = value % 1000; }
        if (millis < 0) { millis += 1000; --seconds; }
        output = L"按" + unit + L"解析\r\n" + FormatUnixTime(seconds, static_cast<int>(millis));
        status.clear();
        return true;
    }
    // Date text: YYYY-MM-DD[ T]HH:MM[:SS[.mmm]][Z|±HH:MM]
    static const std::wregex datePattern(
        LR"(^(\d{4})[-/.](\d{1,2})[-/.](\d{1,2})(?:[ T](\d{1,2}):(\d{2})(?::(\d{2})(?:[.,](\d{1,9}))?)?)?\s*(Z|z|[+-]\d{2}:?\d{2})?$)");
    std::wsmatch parts;
    if (!std::regex_match(input, parts, datePattern)) {
        status = L"无法识别：请输入 Unix 时间戳或 2026-11-23 10:30:00 这样的日期";
        return false;
    }
    const auto number = [&](int group) { return parts[group].matched ? static_cast<unsigned>(std::wcstoul(parts[group].str().c_str(), nullptr, 10)) : 0u; };
    const unsigned year = number(1), month = number(2), day = number(3), hour = number(4), minute = number(5), second = number(6);
    const unsigned millisecond = parts[7].matched ? static_cast<unsigned>(std::wcstoul((parts[7].str() + L"00").substr(0, 3).c_str(), nullptr, 10)) : 0u;
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) {
        status = L"日期或时间超出范围";
        return false;
    }
    SYSTEMTIME given{static_cast<WORD>(year), static_cast<WORD>(month), 0, static_cast<WORD>(day), static_cast<WORD>(hour),
        static_cast<WORD>(minute), static_cast<WORD>(second), static_cast<WORD>(millisecond)};
    SYSTEMTIME utc{};
    const bool explicitZone = parts[8].matched;
    int offsetMinutes = 0;
    if (explicitZone && parts[8].str() != L"Z" && parts[8].str() != L"z") {
        std::wstring zone = parts[8].str();
        zone.erase(std::remove(zone.begin(), zone.end(), L':'), zone.end());
        offsetMinutes = static_cast<int>(std::wcstoul(zone.substr(1, 2).c_str(), nullptr, 10) * 60 + std::wcstoul(zone.substr(3, 2).c_str(), nullptr, 10));
        if (zone[0] == L'-') offsetMinutes = -offsetMinutes;
    }
    FILETIME file{};
    if (explicitZone) {
        if (!SystemTimeToFileTime(&given, &file)) { status = L"日期无效"; return false; }
    } else {
        if (!TzSpecificLocalTimeToSystemTime(nullptr, &given, &utc) || !SystemTimeToFileTime(&utc, &file)) { status = L"日期无效"; return false; }
    }
    const int64_t ticks = (static_cast<int64_t>(file.dwHighDateTime) << 32) | file.dwLowDateTime;
    const int64_t millis = ticks / 10000LL - 11644473600000LL - static_cast<int64_t>(offsetMinutes) * 60000LL;
    const int64_t seconds = millis >= 0 ? millis / 1000 : (millis - 999) / 1000;
    output = std::wstring(explicitZone ? L"按输入的时区解析" : L"按本地时间解析") + L"\r\n秒：" + std::to_wstring(seconds) + L"\r\n毫秒：" +
        std::to_wstring(millis) + L"\r\n" + FormatUnixTime(seconds, static_cast<int>(((millis % 1000) + 1000) % 1000));
    status.clear();
    return true;
}

bool Base64UrlDecodeBytes(std::wstring_view text, std::string& bytes) {
    std::wstring clean;
    for (wchar_t c : text) {
        if (c == L'-') c = L'+';
        else if (c == L'_') c = L'/';
        if (!iswspace(c)) clean += c;
    }
    while (clean.size() % 4) clean += L'=';
    DWORD size = 0;
    if (clean.empty() || !CryptStringToBinaryW(clean.c_str(), static_cast<DWORD>(clean.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr)) return false;
    bytes.assign(size, '\0');
    return CryptStringToBinaryW(clean.c_str(), static_cast<DWORD>(clean.size()), CRYPT_STRING_BASE64, reinterpret_cast<BYTE*>(bytes.data()), &size, nullptr, nullptr) != FALSE;
}

bool RunJwt(const std::wstring& rawInput, std::wstring& output, std::wstring& status) {
    std::wstring token = TrimSpace(rawInput);
    if (token.compare(0, 7, L"Bearer ") == 0) token = TrimSpace(token.substr(7));
    const size_t first = token.find(L'.');
    const size_t second = first == std::wstring::npos ? std::wstring::npos : token.find(L'.', first + 1);
    if (second == std::wstring::npos) { status = L"不是 JWT：应为 header.payload.signature 三段"; return false; }
    std::string headerBytes, payloadBytes;
    if (!Base64UrlDecodeBytes(token.substr(0, first), headerBytes) || !Base64UrlDecodeBytes(token.substr(first + 1, second - first - 1), payloadBytes)) {
        status = L"JWT 的 Base64URL 部分无法解码";
        return false;
    }
    std::wstring header, payload, error;
    const std::wstring headerText = Utf8ToWide(headerBytes), payloadText = Utf8ToWide(payloadBytes);
    if (!FormatJson(headerText, header, error)) header = headerText;
    if (!FormatJson(payloadText, payload, error)) payload = payloadText;
    output = L"// Header\r\n" + ToCrLf(header) + L"\r\n\r\n// Payload\r\n" + ToCrLf(payload) + L"\r\n";
    // Well-known time claims.
    std::wstring times;
    for (const wchar_t* claim : {L"exp", L"iat", L"nbf"}) {
        const std::wregex pattern(std::wstring(L"\"") + claim + L"\"\\s*:\\s*(\\d+)");
        std::wsmatch match;
        if (std::regex_search(payloadText, match, pattern)) {
            const int64_t value = _wtoi64(match[1].str().c_str());
            std::wstring when = FormatUnixTime(value, 0);
            when = when.substr(0, when.find(L"\r\n"));
            times += std::wstring(L"\r\n") + claim + L"：" + when.substr(when.find(L'：') + 1);
            if (wcscmp(claim, L"exp") == 0) times += value < UnixNow() ? L"（已过期）" : L"（未过期）";
        }
    }
    if (!times.empty()) output += L"\r\n// 时间" + times + L"\r\n";
    output += L"\r\n// 签名（未验证）\r\n" + token.substr(second + 1);
    status = L"仅解码，未校验签名";
    return true;
}

bool RunUrlParse(const std::wstring& rawInput, std::wstring& output, std::wstring& status) {
    const std::wstring url = TrimSpace(rawInput);
    const std::wregex pattern(LR"(^([a-zA-Z][a-zA-Z0-9+.-]*):\/\/(?:([^@\/?#]*)@)?([^:\/?#]*)(?::(\d+))?([^?#]*)(?:\?([^#]*))?(?:#(.*))?$)");
    std::wsmatch match;
    if (!std::regex_match(url, match, pattern)) { status = L"不是完整的 URL（需要 scheme://host）"; return false; }
    const wchar_t* names[] = {L"", L"协议", L"用户信息", L"主机", L"端口", L"路径", L"查询", L"片段"};
    output.clear();
    for (int g = 1; g <= 7; ++g) {
        if (match[g].matched && match[g].length() > 0) output += std::wstring(names[g]) + L"：" + match[g].str() + L"\r\n";
    }
    if (match[6].matched && match[6].length() > 0) {
        output += L"\r\n查询参数：\r\n";
        const std::wstring query = match[6].str();
        size_t start = 0;
        while (start <= query.size()) {
            size_t end = query.find(L'&', start);
            if (end == std::wstring::npos) end = query.size();
            const std::wstring pair = query.substr(start, end - start);
            if (!pair.empty()) {
                const size_t equals = pair.find(L'=');
                std::wstring key = pair.substr(0, equals), value = equals == std::wstring::npos ? L"" : pair.substr(equals + 1);
                for (std::wstring* part : {&key, &value}) std::replace(part->begin(), part->end(), L'+', L' ');
                int decoded = 0;
                output += L"  " + UrlDecode(key, decoded) + L" = " + UrlDecode(value, decoded) + L"\r\n";
            }
            start = end + 1;
        }
    }
    status.clear();
    return true;
}

bool RunRegexTest(const std::wstring& input, const std::wstring& pattern, const std::wstring& replacement, bool ignoreCase,
    std::wstring& output, std::wstring& status) {
    if (pattern.empty()) { output.clear(); status = L"在上方输入正则表达式，下方输入测试文本"; return true; }
    try {
        const std::wregex expression(pattern, ignoreCase ? std::regex::ECMAScript | std::regex::icase : std::regex::ECMAScript);
        const std::wstring text = input.size() > (1u << 20) ? input.substr(0, 1u << 20) : input;
        size_t count = 0;
        std::wstring list;
        for (auto it = std::wsregex_iterator(text.begin(), text.end(), expression); it != std::wsregex_iterator(); ++it) {
            const std::wsmatch& match = *it;
            ++count;
            if (count <= 500) {
                const size_t position = static_cast<size_t>(match.position(0));
                const size_t line = 1 + static_cast<size_t>(std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(position), L'\n'));
                const size_t lineStart = text.rfind(L'\n', position == 0 ? 0 : position - 1);
                const size_t column = position - (lineStart == std::wstring::npos || position == 0 ? 0 : lineStart + 1) + 1;
                list += L"#" + std::to_wstring(count) + L"  行 " + std::to_wstring(line) + L" 列 " + std::to_wstring(column) + L"：" + match.str(0) + L"\r\n";
                for (size_t g = 1; g < match.size(); ++g) {
                    list += L"      $" + std::to_wstring(g) + L" = " + (match[g].matched ? match.str(g) : L"（未匹配）") + L"\r\n";
                }
            }
            if (match.length(0) == 0 && count > 100000) break;
        }
        output = L"共 " + std::to_wstring(count) + L" 处匹配" + (count > 500 ? L"（只列出前 500 处）" : L"") + L"\r\n\r\n" + list;
        if (!replacement.empty()) output += L"\r\n// 替换结果\r\n" + ToCrLf(std::regex_replace(text, expression, replacement));
        status = count ? L"" : L"没有匹配";
        return true;
    } catch (const std::regex_error&) {
        status = L"正则表达式有语法错误";
        output.clear();
        return false;
    }
}

std::wstring UnicodeEscape(std::wstring_view text) {
    std::wstring out;
    for (const wchar_t c : text) {
        if (c < 0x80) { out += c; continue; }
        wchar_t buffer[8]{};
        swprintf_s(buffer, L"\\u%04x", static_cast<unsigned>(c));
        out += buffer;
    }
    return out;
}

std::wstring UnicodeUnescape(std::wstring_view text, int& count) {
    std::wstring out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\\' && i + 5 < text.size() + 0 && (text[i + 1] == L'u' || text[i + 1] == L'U')) {
            if (text[i + 2] == L'{') {
                const size_t close = text.find(L'}', i + 3);
                if (close != std::wstring_view::npos) {
                    const unsigned long value = std::wcstoul(std::wstring(text.substr(i + 3, close - i - 3)).c_str(), nullptr, 16);
                    if (value >= 0x10000) { out += static_cast<wchar_t>(0xD800 + ((value - 0x10000) >> 10)); out += static_cast<wchar_t>(0xDC00 + ((value - 0x10000) & 0x3FF)); }
                    else out += static_cast<wchar_t>(value);
                    i = close;
                    ++count;
                    continue;
                }
            }
            bool hex = true;
            for (size_t k = 2; k < 6; ++k) hex = hex && iswxdigit(text[i + k]);
            if (hex) {
                out += static_cast<wchar_t>(std::wcstoul(std::wstring(text.substr(i + 2, 4)).c_str(), nullptr, 16));
                i += 5;
                ++count;
                continue;
            }
        }
        out += text[i];
    }
    return out;
}

std::wstring HtmlEncode(std::wstring_view text) {
    std::wstring out;
    for (const wchar_t c : text) {
        switch (c) {
        case L'&': out += L"&amp;"; break;
        case L'<': out += L"&lt;"; break;
        case L'>': out += L"&gt;"; break;
        case L'"': out += L"&quot;"; break;
        case L'\'': out += L"&#39;"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::wstring HtmlDecode(std::wstring_view text, int& count) {
    static const std::pair<const wchar_t*, wchar_t> named[] = {{L"amp", L'&'}, {L"lt", L'<'}, {L"gt", L'>'}, {L"quot", L'"'}, {L"apos", L'\''},
        {L"nbsp", static_cast<wchar_t>(0xA0)}, {L"copy", static_cast<wchar_t>(0xA9)}, {L"reg", static_cast<wchar_t>(0xAE)}, {L"hellip", static_cast<wchar_t>(0x2026)}, {L"mdash", static_cast<wchar_t>(0x2014)}, {L"ndash", static_cast<wchar_t>(0x2013)}, {L"yen", static_cast<wchar_t>(0xA5)}};
    std::wstring out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'&') {
            const size_t semicolon = text.find(L';', i + 1);
            if (semicolon != std::wstring_view::npos && semicolon - i <= 10) {
                const std::wstring name(text.substr(i + 1, semicolon - i - 1));
                unsigned long code = 0;
                bool ok = false;
                if (!name.empty() && name[0] == L'#') {
                    code = name.size() > 1 && (name[1] == L'x' || name[1] == L'X') ? std::wcstoul(name.c_str() + 2, nullptr, 16) : std::wcstoul(name.c_str() + 1, nullptr, 10);
                    ok = code > 0;
                } else {
                    for (const auto& [entity, value] : named) if (name == entity) { code = value; ok = true; break; }
                }
                if (ok) {
                    if (code >= 0x10000) { out += static_cast<wchar_t>(0xD800 + ((code - 0x10000) >> 10)); out += static_cast<wchar_t>(0xDC00 + ((code - 0x10000) & 0x3FF)); }
                    else out += static_cast<wchar_t>(code);
                    i = semicolon;
                    ++count;
                    continue;
                }
            }
        }
        out += text[i];
    }
    return out;
}

int ToolboxCurrentTool() {
    const LRESULT index = SendMessageW(g_toolbox.tool, CB_GETCURSEL, 0, 0);
    return index >= 0 && index < static_cast<LRESULT>(g_toolbox.shown.size()) ? g_toolbox.shown[static_cast<size_t>(index)] : kToolJson;
}

void RunToolbox() {
    if (!g_toolbox.panel) return;
    const int tool = ToolboxCurrentTool();
    const int action = static_cast<int>(std::max<LRESULT>(0, SendMessageW(g_toolbox.action, CB_GETCURSEL, 0, 0)));
    std::wstring input = GetWindowTextValue(g_toolbox.input);
    std::wstring output, status, error;
    switch (tool) {
    case kToolJson:
        if (TrimSpace(input).empty()) { status = L"输入或选中 JSON"; break; }
        if (!JsonValidator(input).Run(error)) { status = L"✗ " + error; break; }
        if (action == 1) {
            output = CompactJson(input, error);
            status = error.empty() ? L"已压缩为 " + std::to_wstring(output.size()) + L" 个字符" : error;
        } else {
            std::wstring formatted;
            if (FormatJson(input, formatted, error)) {
                if (action == 0) output = ToCrLf(formatted);
                status = L"✓ JSON 格式正确";
            } else {
                status = L"✗ " + error;
            }
        }
        break;
    case kToolBase64:
        if (action == 0) output = Base64Encode(input);
        else if (!input.empty() && !Base64Decode(input, output)) status = L"不是有效的 Base64（或解码后不是 UTF-8 文本）";
        else output = ToCrLf(output);
        break;
    case kToolUrl:
        if (action == 0) output = UrlEncode(input);
        else if (action == 1) {
            int decoded = 0;
            output = ToCrLf(UrlDecode(input, decoded));
            status = L"解码了 " + std::to_wstring(decoded) + L" 处";
        } else if (!TrimSpace(input).empty()) RunUrlParse(input, output, status);
        break;
    case kToolJwt:
        if (TrimSpace(input).empty()) status = L"输入或选中一个 JWT（可带 Bearer 前缀）";
        else RunJwt(input, output, status);
        break;
    case kToolTimestamp:
        RunTimestamp(input, output, status);
        break;
    case kToolHash: {
        const std::string bytes = WideToUtf8(input);
        output = L"MD5：\r\n" + HashHex(bytes, CALG_MD5) + L"\r\n\r\nSHA-1：\r\n" + HashHex(bytes, CALG_SHA1) + L"\r\n\r\nSHA-256：\r\n" +
            HashHex(bytes, CALG_SHA_256) + L"\r\n\r\nSHA-512：\r\n" + HashHex(bytes, CALG_SHA_512);
        status = L"按 UTF-8 计算，" + std::to_wstring(bytes.size()) + L" 字节";
        break;
    }
    case kToolRegex: {
        std::wstring text = input;
        text.erase(std::remove(text.begin(), text.end(), L'\r'), text.end());
        RunRegexTest(text, GetWindowTextValue(g_toolbox.pattern), GetWindowTextValue(g_toolbox.replace),
            SendMessageW(g_toolbox.ignoreCase, BM_GETCHECK, 0, 0) == BST_CHECKED, output, status);
        break;
    }
    case kToolUnicode: {
        int count = 0;
        output = action == 0 ? UnicodeEscape(input) : UnicodeUnescape(input, count);
        if (action == 1) status = L"还原了 " + std::to_wstring(count) + L" 处";
        break;
    }
    case kToolHtml: {
        int count = 0;
        output = action == 0 ? HtmlEncode(input) : HtmlDecode(input, count);
        if (action == 1) status = L"解码了 " + std::to_wstring(count) + L" 处";
        break;
    }
    case kToolUuid: {
        const int count = std::clamp(_wtoi(TrimSpace(input).c_str()), 1, 1000);
        for (int i = 0; i < count; ++i) {
            GUID guid{};
            CoCreateGuid(&guid);
            wchar_t text[64]{};
            swprintf_s(text, L"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\r\n", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
                guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
            output += text;
        }
        if (!output.empty()) output.erase(output.size() - 2);
        status = L"输入数字可一次生成多个（最多 1000）";
        break;
    }
    default:
        break;
    }
    SetWindowTextW(g_toolbox.output, output.c_str());
    SetWindowTextW(g_toolbox.status, status.c_str());
}

// ---- Selection ---------------------------------------------------------------------------

std::wstring ActiveSelectionText() {
    HWND editor = ActiveEditor();
    if (!editor) return {};
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const LONG length = selection.cpMax - selection.cpMin;
    if (length <= 0 || length > (4 << 20)) return {};
    std::wstring selected(static_cast<size_t>(length) + 1, L'\0');
    selected.resize(static_cast<size_t>(std::max<LRESULT>(0, SendMessageW(editor, EM_GETSELTEXT, 0, reinterpret_cast<LPARAM>(selected.data())))));
    return selected;
}

void ToolboxTakeSelection(bool force) {
    if (!g_toolbox.panel || !IsWindowVisible(g_toolbox.panel)) return;
    const std::wstring selected = ActiveSelectionText();
    if (selected.empty() && !force) return;
    g_toolbox.settingInput = true;
    SetWindowTextW(g_toolbox.input, ToCrLf(selected).c_str());
    g_toolbox.settingInput = false;
    RunToolbox();
}

// Called on every caret / selection change in the editor.
void ToolboxOnSelectionChanged() {
    if (g_toolbox.panel && IsWindowVisible(g_toolbox.panel) && SendMessageW(g_toolbox.follow, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        SetTimer(g_toolbox.panel, kTimerToolboxSelection, 150, nullptr);
    }
}

// Output replaces the editor's selection (or goes in at the caret); one undo step.
void ToolboxApplyToEditor() {
    HWND editor = ActiveEditor();
    if (!editor) return;
    std::wstring output = GetWindowTextValue(g_toolbox.output);
    if (output.empty()) { MessageBeep(MB_ICONWARNING); return; }
    std::wstring text;
    for (size_t i = 0; i < output.size(); ++i) {
        if (output[i] == L'\r' && i + 1 < output.size() && output[i + 1] == L'\n') ++i;
        text += output[i] == L'\n' ? L'\r' : output[i];
    }
    SendMessageW(g_toolbox.follow, BM_SETCHECK, BST_UNCHECKED, 0);  // keep the result from replacing the input
    SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
    SetFocus(editor);
    ShowNotice(L"已替换选中内容（可用 Ctrl+Z 撤销）；“跟随选中”已暂停");
}

// ---- Panel -------------------------------------------------------------------------------

void FillToolboxActions() {
    const int tool = ToolboxCurrentTool();
    SendMessageW(g_toolbox.action, CB_RESETCONTENT, 0, 0);
    const std::vector<std::wstring> actions = ToolboxActions(tool);
    for (const std::wstring& action : actions) SendMessageW(g_toolbox.action, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(action.c_str()));
    SendMessageW(g_toolbox.action, CB_SETCURSEL, 0, 0);
    ShowWindow(g_toolbox.action, actions.empty() ? SW_HIDE : SW_SHOW);
    const bool regex = tool == kToolRegex;
    for (const int id : {kTbPattern, kTbReplace, kTbIgnoreCase, kTbPatternLabel, kTbReplaceLabel}) ShowWindow(GetDlgItem(g_toolbox.panel, id), regex ? SW_SHOW : SW_HIDE);
    SetWindowTextW(GetDlgItem(g_toolbox.panel, kTbInputLabel), regex ? L"测试文本" : tool == kToolUuid ? L"生成数量" : L"输入");
    SendMessageW(g_toolbox.input, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(tool == kToolTimestamp ? L"Unix 时间戳或日期；留空显示当前时间" : L""));
    g_app.toolboxTool = tool;
}

void FillToolboxTools() {
    SendMessageW(g_toolbox.tool, CB_RESETCONTENT, 0, 0);
    g_toolbox.shown.clear();
    int select = 0;
    for (int tool = 0; tool < kToolCount; ++tool) {
        if (!(g_app.toolboxMask & (1u << tool))) continue;
        if (tool == g_app.toolboxTool) select = static_cast<int>(g_toolbox.shown.size());
        g_toolbox.shown.push_back(tool);
        SendMessageW(g_toolbox.tool, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kToolboxNames[tool]));
    }
    SendMessageW(g_toolbox.tool, CB_SETCURSEL, select, 0);
    FillToolboxActions();
}

void LayoutToolbox() {
    RECT client{};
    GetClientRect(g_toolbox.panel, &client);
    const int left = Scale(10), width = std::max(0, static_cast<int>(client.right) - Scale(18));
    const int rowH = Scale(26);
    int y = Scale(8);
    HWND panel = g_toolbox.panel;
    MoveWindow(GetDlgItem(panel, kTbTitle), left, y + Scale(3), width - Scale(64), Scale(20), TRUE);
    MoveWindow(GetDlgItem(panel, kTbCustomize), left + width - Scale(56), y, Scale(26), rowH, TRUE);
    MoveWindow(GetDlgItem(panel, kTbClose), left + width - Scale(26), y, Scale(26), rowH, TRUE);
    y += rowH + Scale(8);
    const bool hasActions = IsWindowVisible(g_toolbox.action) != FALSE;
    const int toolWidth = hasActions ? (width - Scale(6)) * 55 / 100 : width;
    MoveWindow(g_toolbox.tool, left, y, toolWidth, Scale(300), TRUE);
    MoveWindow(g_toolbox.action, left + toolWidth + Scale(6), y, width - toolWidth - Scale(6), Scale(300), TRUE);
    y += rowH + Scale(8);
    if (ToolboxCurrentTool() == kToolRegex) {
        MoveWindow(GetDlgItem(panel, kTbPatternLabel), left, y + Scale(4), Scale(44), Scale(20), TRUE);
        MoveWindow(g_toolbox.pattern, left + Scale(48), y, width - Scale(48), rowH, TRUE);
        y += rowH + Scale(6);
        MoveWindow(GetDlgItem(panel, kTbReplaceLabel), left, y + Scale(4), Scale(44), Scale(20), TRUE);
        MoveWindow(g_toolbox.replace, left + Scale(48), y, width - Scale(48), rowH, TRUE);
        y += rowH + Scale(6);
        MoveWindow(g_toolbox.ignoreCase, left, y, width, Scale(22), TRUE);
        y += Scale(28);
    }
    MoveWindow(GetDlgItem(panel, kTbInputLabel), left, y + Scale(3), Scale(70), Scale(20), TRUE);
    MoveWindow(g_toolbox.follow, left + Scale(72), y, width - Scale(72) - Scale(70), Scale(24), TRUE);
    MoveWindow(GetDlgItem(panel, kTbTake), left + width - Scale(64), y, Scale(64), Scale(24), TRUE);
    y += Scale(28);
    // Input and output share what is left above the buttons and status line.
    const int buttonsY = std::max(y + Scale(120), static_cast<int>(client.bottom) - Scale(10) - Scale(20) - Scale(6) - rowH);
    const int available = std::max(Scale(80), buttonsY - Scale(8) - y - Scale(8) - Scale(22));
    const int inputHeight = available * 2 / 5;
    MoveWindow(g_toolbox.input, left, y, width, inputHeight, TRUE);
    y += inputHeight + Scale(8);
    MoveWindow(GetDlgItem(panel, kTbOutputLabel), left, y, width, Scale(20), TRUE);
    y += Scale(22);
    MoveWindow(g_toolbox.output, left, y, width, available - inputHeight, TRUE);
    y = buttonsY;
    const int half = (width - Scale(6)) / 2;
    MoveWindow(GetDlgItem(panel, kTbCopy), left, y, half, rowH, TRUE);
    MoveWindow(GetDlgItem(panel, kTbApply), left + half + Scale(6), y, width - half - Scale(6), rowH, TRUE);
    y += rowH + Scale(6);
    MoveWindow(g_toolbox.status, left, y, width, Scale(20), TRUE);
}

void CustomizeToolbox() {
    std::vector<std::pair<std::wstring, bool>> checks;
    for (int tool = 0; tool < kToolCount; ++tool) checks.emplace_back(kToolboxNames[tool], (g_app.toolboxMask & (1u << tool)) != 0);
    std::vector<std::wstring> values;
    if (!PromptForText(L"自定义开发者工具箱：选择要显示的工具", {}, values, &checks)) return;
    unsigned mask = 0;
    for (int tool = 0; tool < kToolCount; ++tool) if (checks[static_cast<size_t>(tool)].second) mask |= 1u << tool;
    g_app.toolboxMask = mask ? mask : (1u << kToolJson);
    g_app.stateDirty = true;
    FillToolboxTools();
    LayoutToolbox();
    RunToolbox();
}

HBRUSH ToolboxEditBrush() {
    static HBRUSH brush = nullptr;
    static COLORREF color = CLR_INVALID;
    if (color != CurrentPalette().controlBackground) {
        if (brush) DeleteObject(brush);
        color = CurrentPalette().controlBackground;
        brush = CreateSolidBrush(color);
    }
    return brush;
}

LRESULT CALLBACK ToolboxProc(HWND panel, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_toolbox.panel = panel;
        const auto make = [&](const wchar_t* klass, const wchar_t* text, DWORD style, int id, DWORD exStyle = 0) {
            HWND control = CreateWindowExW(exStyle, klass, text, WS_CHILD | style, 0, 0, 0, 0, panel,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app.instance, nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            return control;
        };
        HWND title = make(L"STATIC", L"开发者工具箱", WS_VISIBLE | SS_NOPREFIX, kTbTitle);
        (void)title;
        make(L"BUTTON", L"⚙", WS_VISIBLE | WS_TABSTOP, kTbCustomize);
        make(L"BUTTON", L"✕", WS_VISIBLE | WS_TABSTOP, kTbClose);
        g_toolbox.tool = make(L"COMBOBOX", nullptr, WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, kTbTool);
        g_toolbox.action = make(L"COMBOBOX", nullptr, WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, kTbAction);
        make(L"STATIC", L"正则", SS_NOPREFIX, kTbPatternLabel);
        g_toolbox.pattern = make(L"EDIT", nullptr, WS_TABSTOP | ES_AUTOHSCROLL, kTbPattern, WS_EX_CLIENTEDGE);
        make(L"STATIC", L"替换为", SS_NOPREFIX, kTbReplaceLabel);
        g_toolbox.replace = make(L"EDIT", nullptr, WS_TABSTOP | ES_AUTOHSCROLL, kTbReplace, WS_EX_CLIENTEDGE);
        SendMessageW(g_toolbox.pattern, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"例如 (\\d+)-(\\w+)"));
        SendMessageW(g_toolbox.replace, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"可选，支持 $1 $2"));
        g_toolbox.ignoreCase = make(L"BUTTON", L"忽略大小写", WS_TABSTOP | BS_AUTOCHECKBOX, kTbIgnoreCase);
        make(L"STATIC", L"输入", WS_VISIBLE | SS_NOPREFIX, kTbInputLabel);
        g_toolbox.follow = make(L"BUTTON", L"跟随编辑器选中", WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, kTbFollow);
        SendMessageW(g_toolbox.follow, BM_SETCHECK, BST_CHECKED, 0);
        make(L"BUTTON", L"取选中", WS_VISIBLE | WS_TABSTOP, kTbTake);
        g_toolbox.input = make(L"EDIT", nullptr, WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, kTbInput, WS_EX_CLIENTEDGE);
        make(L"STATIC", L"结果", WS_VISIBLE | SS_NOPREFIX, kTbOutputLabel);
        g_toolbox.output = make(L"EDIT", nullptr, WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, kTbOutput, WS_EX_CLIENTEDGE);
        make(L"BUTTON", L"复制结果", WS_VISIBLE | WS_TABSTOP, kTbCopy);
        make(L"BUTTON", L"替换选中内容", WS_VISIBLE | WS_TABSTOP, kTbApply);
        g_toolbox.status = make(L"STATIC", nullptr, WS_VISIBLE | SS_NOPREFIX | SS_ENDELLIPSIS, kTbStatus);
        for (HWND box : {g_toolbox.input, g_toolbox.output, g_toolbox.pattern, g_toolbox.replace}) {
            SendMessageW(box, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
            SendMessageW(box, EM_SETLIMITTEXT, 0, 0);
        }
        ThemeCheckbox(g_toolbox.follow);
        ThemeCheckbox(g_toolbox.ignoreCase);
        if (g_app.darkTheme) {
            for (HWND control : {g_toolbox.input, g_toolbox.output, g_toolbox.pattern, g_toolbox.replace, g_toolbox.tool, g_toolbox.action}) SetWindowTheme(control, L"DarkMode_CFD", nullptr);
            for (const int id : {kTbCustomize, kTbClose, kTbTake, kTbCopy, kTbApply}) SetWindowTheme(GetDlgItem(panel, id), L"DarkMode_Explorer", nullptr);
        }
        FillToolboxTools();
        return 0;
    }
    case WM_SIZE:
        LayoutToolbox();
        return 0;
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(panel, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, g_app.toolbarBrush);
        // Divider on the left edge.
        RECT divider{0, 0, Scale(1), client.bottom};
        HBRUSH brush = CreateSolidBrush(CurrentPalette().panelBorder);
        FillRect(reinterpret_cast<HDC>(wParam), &divider, brush);
        DeleteObject(brush);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        if (control == g_toolbox.output) {
            SetTextColor(dc, CurrentPalette().editorText);
            SetBkColor(dc, CurrentPalette().controlBackground);
            return reinterpret_cast<LRESULT>(ToolboxEditBrush());
        }
        const std::wstring status = control == g_toolbox.status ? GetWindowTextValue(control) : L"";
        SetTextColor(dc, control == g_toolbox.status ? (status.rfind(L"✗", 0) == 0 || status.find(L"错误") != std::wstring::npos || status.find(L"无") == 0 || status.find(L"不是") == 0
            ? (g_app.darkTheme ? RGB(244, 112, 103) : RGB(190, 30, 30)) : CurrentPalette().statusText) : CurrentPalette().toolbarText);
        SetBkColor(dc, CurrentPalette().toolbarBackground);
        return reinterpret_cast<LRESULT>(g_app.toolbarBrush);
    }
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, CurrentPalette().toolbarText);
        SetBkColor(dc, CurrentPalette().toolbarBackground);
        return reinterpret_cast<LRESULT>(g_app.toolbarBrush);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, CurrentPalette().editorText);
        SetBkColor(dc, CurrentPalette().controlBackground);
        return reinterpret_cast<LRESULT>(ToolboxEditBrush());
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(panel, &point);
            if (point.x < Scale(5)) { SetCursor(LoadCursorW(nullptr, IDC_SIZEWE)); return TRUE; }
        }
        break;
    case WM_LBUTTONDOWN:
        if (GET_X_LPARAM(lParam) < Scale(5)) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(panel, &point);
            g_toolbox.dragging = true;
            g_toolbox.dragStartX = point.x;
            g_toolbox.dragStartWidth = Scale(g_app.toolboxWidth);
            SetCapture(panel);
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (g_toolbox.dragging) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(panel, &point);
            const int pixels = std::clamp(g_toolbox.dragStartWidth + (g_toolbox.dragStartX - static_cast<int>(point.x)), Scale(260), Scale(900));
            g_app.toolboxWidth = MulDiv(pixels, 96, static_cast<int>(g_app.dpi));
            LayoutChildren(g_app.window);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (g_toolbox.dragging) {
            g_toolbox.dragging = false;
            ReleaseCapture();
            g_app.stateDirty = true;
            return 0;
        }
        break;
    case WM_TIMER:
        KillTimer(panel, wParam);
        if (wParam == kTimerToolboxRun) RunToolbox();
        else if (wParam == kTimerToolboxSelection) ToolboxTakeSelection(false);
        return 0;
    case WM_COMMAND: {
        const int id = LOWORD(wParam), code = HIWORD(wParam);
        switch (id) {
        case kTbTool:
            if (code == CBN_SELCHANGE) { FillToolboxActions(); LayoutToolbox(); InvalidateRect(panel, nullptr, TRUE); RunToolbox(); g_app.stateDirty = true; }
            return 0;
        case kTbAction:
            if (code == CBN_SELCHANGE) RunToolbox();
            return 0;
        case kTbInput: case kTbPattern: case kTbReplace:
            if (code == EN_CHANGE && !g_toolbox.settingInput) SetTimer(panel, kTimerToolboxRun, 200, nullptr);
            return 0;
        case kTbIgnoreCase: RunToolbox(); return 0;
        case kTbFollow:
            if (SendMessageW(g_toolbox.follow, BM_GETCHECK, 0, 0) == BST_CHECKED) ToolboxTakeSelection(false);
            return 0;
        case kTbTake: ToolboxTakeSelection(true); return 0;
        case kTbCopy: {
            const std::wstring output = GetWindowTextValue(g_toolbox.output);
            if (!output.empty()) { CopyTextToClipboard(output); SetWindowTextW(g_toolbox.status, L"已复制到剪贴板"); }
            return 0;
        }
        case kTbApply: ToolboxApplyToEditor(); return 0;
        case kTbCustomize: CustomizeToolbox(); return 0;
        case kTbClose: ToggleToolbox(false); return 0;
        default: break;
        }
        break;
    }
    case WM_DESTROY:
        g_toolbox = ToolboxState{};
        break;
    default:
        break;
    }
    return DefWindowProcW(panel, message, wParam, lParam);
}

// Theme or font changed: re-theme the toolbox controls.
void ApplyToolboxTheme() {
    if (!g_toolbox.panel) return;
    EnumChildWindows(g_toolbox.panel, [](HWND control, LPARAM) -> BOOL {
        wchar_t className[32]{};
        GetClassNameW(control, className, 32);
        const bool box = control == g_toolbox.input || control == g_toolbox.output || control == g_toolbox.pattern || control == g_toolbox.replace;
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(box ? g_app.editorFont : g_app.uiFont), TRUE);
        if (_wcsicmp(className, L"ComboBox") == 0 || _wcsicmp(className, L"Edit") == 0) {
            SetWindowTheme(control, g_app.darkTheme ? L"DarkMode_CFD" : nullptr, nullptr);
        } else if (_wcsicmp(className, L"Button") == 0) {
            if ((GetWindowLongW(control, GWL_STYLE) & BS_TYPEMASK) == BS_AUTOCHECKBOX) ThemeCheckbox(control);
            else SetWindowTheme(control, g_app.darkTheme ? L"DarkMode_Explorer" : nullptr, nullptr);
        }
        return TRUE;
    }, 0);
    LayoutToolbox();
    RedrawWindow(g_toolbox.panel, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_FRAME);
}

void ToggleToolbox(bool show) {
    if (show && !g_toolbox.panel) {
        static bool registered = false;
        if (!registered) {
            WNDCLASSW klass{};
            klass.hInstance = g_app.instance;
            klass.lpfnWndProc = ToolboxProc;
            klass.lpszClassName = L"MCnotepadToolbox";
            klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            registered = RegisterClassW(&klass) != 0;
        }
        g_app.toolboxPanel = CreateWindowExW(WS_EX_CONTROLPARENT, L"MCnotepadToolbox", nullptr, WS_CHILD | WS_CLIPCHILDREN, 0, 0, 0, 0,
            g_app.window, nullptr, g_app.instance, nullptr);
    }
    g_app.toolboxVisible = show && g_app.toolboxPanel;
    if (g_app.toolboxPanel) ShowWindow(g_app.toolboxPanel, g_app.toolboxVisible ? SW_SHOW : SW_HIDE);
    LayoutChildren(g_app.window);
    g_app.stateDirty = true;
    if (g_app.toolboxVisible) {
        LayoutToolbox();
        if (!ActiveSelectionText().empty()) ToolboxTakeSelection(false);
        else RunToolbox();
        SetFocus(g_toolbox.input);
    } else if (HWND editor = ActiveEditor()) {
        SetFocus(editor);
    }
}

// ===========================================================================
// Compare: two files side by side, and two folders
// ===========================================================================

// A block that exists on one side only can often sit in several places
// ("}" + blank line ambiguity). Like git: move it down as far as it goes,
// then back up while it ends with a blank line, so it reads as whole lines
// of code being added or removed.
void SlideOneSidedBlocks(std::vector<std::pair<int, int>>& pairs, const std::vector<std::wstring>& a, const std::vector<std::wstring>& b) {
    const auto matched = [&](size_t k) { return pairs[k].first >= 0 && pairs[k].second >= 0; };
    const auto blank = [](const std::wstring& line) {
        return std::all_of(line.begin(), line.end(), [](wchar_t c) { return iswspace(c) != 0; });
    };
    size_t s = 0;
    while (s < pairs.size()) {
        if (matched(s)) { ++s; continue; }
        size_t e = s;
        while (e < pairs.size() && !matched(e)) ++e;
        const bool left = pairs[s].second < 0;
        bool oneSided = true;
        for (size_t k = s; k < e; ++k) oneSided = oneSided && ((pairs[k].second < 0) == left);
        if (!oneSided) { s = e; continue; }
        const std::vector<std::wstring>& lines = left ? a : b;
        const auto line = [&](size_t k) -> int { return left ? pairs[k].first : pairs[k].second; };
        const auto other = [&](size_t k) -> int { return left ? pairs[k].second : pairs[k].first; };
        const auto set = [&](size_t k, int here, int there) { pairs[k] = left ? std::make_pair(here, there) : std::make_pair(there, here); };
        const size_t length = e - s;
        // Down.
        while (e < pairs.size() && matched(e) && lines[static_cast<size_t>(line(s))] == lines[static_cast<size_t>(line(e))]) {
            const int first = line(s), there = other(e);
            set(s, first, there);
            for (size_t k = 1; k <= length; ++k) set(s + k, first + static_cast<int>(k), -1);
            ++s;
            ++e;
        }
        // Back up while the block ends with a blank line.
        while (s > 0 && matched(s - 1) && blank(lines[static_cast<size_t>(line(e - 1))]) &&
               lines[static_cast<size_t>(line(e - 1))] == lines[static_cast<size_t>(line(s - 1))]) {
            const int first = line(s - 1), there = other(s - 1);
            for (size_t k = 0; k < length; ++k) set(s - 1 + k, first + static_cast<int>(k), -1);
            set(e - 1, first + static_cast<int>(length), there);
            --s;
            --e;
        }
        s = e;
    }
}

// Pairs up lines of two texts: (leftIndex, rightIndex), -1 where a line
// exists on one side only. Common prefix/suffix first, then LCS on the rest.
std::vector<std::pair<int, int>> AlignLines(const std::vector<std::wstring>& a, const std::vector<std::wstring>& b) {
    std::vector<std::pair<int, int>> out;
    const size_t n = a.size(), m = b.size();
    size_t prefix = 0;
    while (prefix < n && prefix < m && a[prefix] == b[prefix]) ++prefix;
    size_t suffix = 0;
    while (suffix < n - prefix && suffix < m - prefix && a[n - 1 - suffix] == b[m - 1 - suffix]) ++suffix;
    for (size_t i = 0; i < prefix; ++i) out.emplace_back(static_cast<int>(i), static_cast<int>(i));
    const size_t rows = n - prefix - suffix, cols = m - prefix - suffix;
    if (rows > 0 && cols > 0 && rows * cols <= 16'000'000 && std::min(rows, cols) < 65535) {
        std::vector<uint16_t> table((rows + 1) * (cols + 1), 0);
        const auto at = [&](size_t i, size_t j) -> uint16_t& { return table[i * (cols + 1) + j]; };
        for (size_t i = rows; i-- > 0;) {
            for (size_t j = cols; j-- > 0;) {
                at(i, j) = a[prefix + i] == b[prefix + j] ? static_cast<uint16_t>(at(i + 1, j + 1) + 1) : std::max(at(i + 1, j), at(i, j + 1));
            }
        }
        size_t i = 0, j = 0;
        while (i < rows || j < cols) {
            if (i < rows && j < cols && a[prefix + i] == b[prefix + j]) {
                out.emplace_back(static_cast<int>(prefix + i++), static_cast<int>(prefix + j++));
            } else if (i < rows && (j == cols || at(i + 1, j) >= at(i, j + 1))) {
                out.emplace_back(static_cast<int>(prefix + i++), -1);
            } else {
                out.emplace_back(-1, static_cast<int>(prefix + j++));
            }
        }
    } else {
        for (size_t i = 0; i < rows; ++i) out.emplace_back(static_cast<int>(prefix + i), -1);
        for (size_t j = 0; j < cols; ++j) out.emplace_back(-1, static_cast<int>(prefix + j));
    }
    for (size_t k = 0; k < suffix; ++k) out.emplace_back(static_cast<int>(n - suffix + k), static_cast<int>(m - suffix + k));
    SlideOneSidedBlocks(out, a, b);
    return out;
}

std::wstring WhitespaceKey(const std::wstring& line) {
    std::wstring key;
    for (const wchar_t c : line) if (!iswspace(c)) key += c;
    return key;
}

enum class RowKind { Same, Changed, LeftOnly, RightOnly };

struct CompareRow {
    int left = -1;
    int right = -1;
    RowKind kind = RowKind::Same;
};

struct CompareHunk {
    int firstRow = 0, lastRow = 0;     // rows in the aligned view
    int leftStart = 0, leftCount = 0;  // line range on each side
    int rightStart = 0, rightCount = 0;
};

struct CompareSide {
    std::wstring path;       // empty for an untitled tab
    std::wstring title;
    HWND editor = nullptr;   // the tab this side came from, if any
    std::vector<std::wstring> lines;
    int encoding = 0;
    std::wstring eol = L"\r\n";
    bool dirty = false;
};

// Rows of the aligned view: changed pairs first within each block of edits.
std::vector<CompareRow> BuildCompareRows(const CompareSide& left, const CompareSide& right, bool ignoreWhitespace,
    std::vector<CompareHunk>& hunks) {
    std::vector<std::wstring> keysLeft = left.lines, keysRight = right.lines;
    if (ignoreWhitespace) {
        for (std::wstring& line : keysLeft) line = WhitespaceKey(line);
        for (std::wstring& line : keysRight) line = WhitespaceKey(line);
    }
    const auto pairs = AlignLines(keysLeft, keysRight);
    std::vector<CompareRow> rows;
    hunks.clear();
    int nextLeft = 0, nextRight = 0;
    for (size_t k = 0; k < pairs.size();) {
        if (pairs[k].first >= 0 && pairs[k].second >= 0) {
            rows.push_back({pairs[k].first, pairs[k].second, RowKind::Same});
            nextLeft = pairs[k].first + 1;
            nextRight = pairs[k].second + 1;
            ++k;
            continue;
        }
        std::vector<int> removed, added;
        while (k < pairs.size() && !(pairs[k].first >= 0 && pairs[k].second >= 0)) {
            if (pairs[k].first >= 0) removed.push_back(pairs[k].first);
            else added.push_back(pairs[k].second);
            ++k;
        }
        CompareHunk hunk;
        hunk.firstRow = static_cast<int>(rows.size());
        hunk.leftStart = removed.empty() ? nextLeft : removed.front();
        hunk.leftCount = static_cast<int>(removed.size());
        hunk.rightStart = added.empty() ? nextRight : added.front();
        hunk.rightCount = static_cast<int>(added.size());
        const size_t paired = std::min(removed.size(), added.size());
        for (size_t p = 0; p < paired; ++p) rows.push_back({removed[p], added[p], RowKind::Changed});
        for (size_t p = paired; p < removed.size(); ++p) rows.push_back({removed[p], -1, RowKind::LeftOnly});
        for (size_t p = paired; p < added.size(); ++p) rows.push_back({-1, added[p], RowKind::RightOnly});
        hunk.lastRow = static_cast<int>(rows.size()) - 1;
        hunks.push_back(hunk);
        if (!removed.empty()) nextLeft = removed.back() + 1;
        if (!added.empty()) nextRight = added.back() + 1;
    }
    return rows;
}

bool LoadCompareSideFromFile(CompareSide& side, const std::wstring& path, std::wstring& error) {
    std::vector<char> bytes;
    if (!ReadBinaryFile(path, bytes)) {
        error = L"无法读取：" + path;
        return false;
    }
    const bool utf16Bom = bytes.size() >= 2 && ((static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) ||
        (static_cast<unsigned char>(bytes[0]) == 0xFE && static_cast<unsigned char>(bytes[1]) == 0xFF));
    if (!utf16Bom && memchr(bytes.data(), 0, std::min<size_t>(bytes.size(), 8192))) {
        error = L"二进制文件无法按文本比较：" + path;
        return false;
    }
    bool mixed = false;
    const std::wstring text = NormalizeToLf(DecodeTextBytes(bytes, &side.encoding), side.eol, mixed);
    side.lines = SplitLines(text);
    side.path = path;
    side.title = fs::path(path).filename().wstring();
    side.editor = nullptr;
    side.dirty = false;
    return true;
}

void LoadCompareSideFromTab(CompareSide& side, const TabDocument& document) {
    side.lines = SplitLines(GetEditorRawText(document.editor));
    side.path = document.untitled ? std::wstring() : document.path;
    side.title = document.title + (document.modified ? L"（未保存的修改）" : L"");
    side.editor = document.editor;
    side.encoding = document.encoding;
    side.eol = document.lineEnding == 1 ? L"\n" : document.lineEnding == 2 ? L"\r" : L"\r\n";
    side.dirty = false;
}

// ---- File compare window -------------------------------------------------------

enum CompareControl : int {
    kCmpPrev = 4001, kCmpNext, kCmpToRight, kCmpToLeft, kCmpSwap, kCmpIgnoreSpace, kCmpSaveLeft, kCmpSaveRight, kCmpRefresh,
    kCmpLeftView, kCmpRightView, kCmpStats, kCmpLeftTitle, kCmpRightTitle, kCmpStructure, kCmpList,
};

// ---- Structured compare: JSON / CSV / YAML ------------------------------------------
// Each format is flattened to items addressed by a path ("server.port",
// "users[2].name", "row-key › column"). Items are compared one by one and
// any item can be taken from the other side.

enum class StructFormat { None, Json, Csv, Yaml };

struct StructItem {
    std::wstring path;
    std::wstring parent;   // path of the parent item ("" = root)
    std::wstring value;    // what is shown and compared (leaves)
    bool container = false;
};

class StructDoc {
public:
    virtual ~StructDoc() = default;
    virtual bool Parse(const std::vector<std::wstring>& lines, std::wstring& error) = 0;
    // Every item in document order, parents before their children.
    virtual std::vector<StructItem> Items() const = 0;
    // Makes `path` here match `from`: replace, insert or delete.
    virtual bool Take(const std::wstring& path, const StructDoc& from, std::wstring& error) = 0;
    virtual std::vector<std::wstring> Lines() const = 0;
};

// ---- JSON (comments allowed, numbers and strings kept as written) ----

struct JsonNode {
    enum Type { Scalar, Object, Array } type = Scalar;
    std::wstring raw;  // scalars: exactly as written ("text", 1.50, true, null)
    std::vector<std::pair<std::wstring, JsonNode>> members;  // key as written, with quotes
    std::vector<JsonNode> items;
};

class JsonDoc : public StructDoc {
public:
    bool Parse(const std::vector<std::wstring>& lines, std::wstring& error) override {
        text_ = JoinLines(lines, L"\n");
        position_ = 0;
        indent_ = L"  ";
        compact_ = text_.find(L'\n') == std::wstring::npos || TrimCopy(text_).find(L'\n') == std::wstring::npos;
        for (const std::wstring& line : lines) {
            const size_t first = line.find_first_not_of(L" \t");
            if (first != std::wstring::npos && first > 0) { indent_ = line.substr(0, first); break; }
        }
        trailingNewline_ = !lines.empty() && lines.back().empty();
        SkipSpace();
        if (position_ >= text_.size()) { root_ = JsonNode{}; root_.type = JsonNode::Object; return true; }
        if (!ParseValue(root_, error)) return false;
        SkipSpace();
        if (position_ < text_.size()) { error = L"JSON 第 " + std::to_wstring(LineAt(position_)) + L" 行：多余的内容"; return false; }
        text_.clear();
        return true;
    }

    std::vector<StructItem> Items() const override {
        std::vector<StructItem> out;
        Collect(root_, L"", L"", out, true);
        return out;
    }

    bool Take(const std::wstring& path, const StructDoc& fromBase, std::wstring& error) override {
        const auto& from = static_cast<const JsonDoc&>(fromBase);
        Location source = from.Find(path);
        Location target = Find(path);
        if (source.node && target.node) {
            *target.node = *source.node;
            return true;
        }
        if (!source.node && target.node) {
            if (target.parent->type == JsonNode::Object) target.parent->members.erase(target.parent->members.begin() + static_cast<std::ptrdiff_t>(target.index));
            else target.parent->items.erase(target.parent->items.begin() + static_cast<std::ptrdiff_t>(target.index));
            return true;
        }
        if (!source.node) return true;
        Location parent = source.parentPath.empty() ? Location{&root_, nullptr, 0, L""} : Find(source.parentPath);
        if (!parent.node || parent.node->type != source.parent->type) {
            error = L"另一侧没有“" + source.parentPath + L"”这个对象，无法插入";
            return false;
        }
        if (source.parent->type == JsonNode::Array) {
            const size_t at = std::min(source.index, parent.node->items.size());
            parent.node->items.insert(parent.node->items.begin() + static_cast<std::ptrdiff_t>(at), *source.node);
            return true;
        }
        // After the nearest earlier key that exists here, else first.
        size_t at = 0;
        for (size_t k = source.index; k-- > 0;) {
            const std::wstring& key = source.parent->members[k].first;
            const auto it = std::find_if(parent.node->members.begin(), parent.node->members.end(), [&](const auto& m) { return m.first == key; });
            if (it != parent.node->members.end()) { at = static_cast<size_t>(it - parent.node->members.begin()) + 1; break; }
        }
        parent.node->members.insert(parent.node->members.begin() + static_cast<std::ptrdiff_t>(at), {source.parent->members[source.index].first, *source.node});
        return true;
    }

    std::vector<std::wstring> Lines() const override {
        std::wstring out;
        Write(root_, 0, out);
        std::vector<std::wstring> lines = SplitLines(out);
        if (trailingNewline_) lines.emplace_back();
        return lines;
    }

private:
    struct Location {
        JsonNode* node = nullptr;
        JsonNode* parent = nullptr;
        size_t index = 0;
        std::wstring parentPath;
    };

    static std::wstring KeyName(const std::wstring& raw) {
        return raw.size() >= 2 && raw.front() == L'"' ? raw.substr(1, raw.size() - 2) : raw;
    }
    static std::wstring ChildPath(const std::wstring& parent, const std::wstring& key) {
        return parent.empty() ? key : parent + L"." + key;
    }

    void Collect(const JsonNode& node, const std::wstring& path, const std::wstring& parent, std::vector<StructItem>& out, bool root) const {
        if (!root) {
            StructItem item{path, parent, {}, node.type != JsonNode::Scalar};
            if (node.type == JsonNode::Scalar) item.value = node.raw;
            else if (node.type == JsonNode::Object) item.value = L"{ " + std::to_wstring(node.members.size()) + L" 个键 }";
            else item.value = L"[ " + std::to_wstring(node.items.size()) + L" 项 ]";
            out.push_back(item);
        }
        if (node.type == JsonNode::Object) {
            for (const auto& [key, child] : node.members) Collect(child, ChildPath(path, KeyName(key)), path, out, false);
        } else if (node.type == JsonNode::Array) {
            for (size_t i = 0; i < node.items.size(); ++i) Collect(node.items[i], path + L"[" + std::to_wstring(i) + L"]", path, out, false);
        }
    }

    // Walks the tree; paths are the ones Collect produces.
    Location Find(const std::wstring& path) const {
        Location found;
        Search(const_cast<JsonNode&>(root_), L"", path, found);
        return found;
    }
    static bool Search(JsonNode& node, const std::wstring& path, const std::wstring& wanted, Location& found) {
        if (node.type == JsonNode::Object) {
            for (size_t k = 0; k < node.members.size(); ++k) {
                const std::wstring child = ChildPath(path, KeyName(node.members[k].first));
                if (child == wanted) { found = {&node.members[k].second, &node, k, path}; return true; }
                if (wanted.compare(0, child.size(), child) == 0 && wanted.size() > child.size() &&
                    (wanted[child.size()] == L'.' || wanted[child.size()] == L'[') && Search(node.members[k].second, child, wanted, found)) return true;
            }
        } else if (node.type == JsonNode::Array) {
            for (size_t k = 0; k < node.items.size(); ++k) {
                const std::wstring child = path + L"[" + std::to_wstring(k) + L"]";
                if (child == wanted) { found = {&node.items[k], &node, k, path}; return true; }
                if (wanted.compare(0, child.size(), child) == 0 && wanted.size() > child.size() && Search(node.items[k], child, wanted, found)) return true;
            }
        }
        return false;
    }

    int LineAt(size_t position) const { return 1 + static_cast<int>(std::count(text_.begin(), text_.begin() + static_cast<std::ptrdiff_t>(std::min(position, text_.size())), L'\n')); }

    void SkipSpace() {
        while (position_ < text_.size()) {
            const wchar_t c = text_[position_];
            if (iswspace(c) || c == 0xFEFF) { ++position_; continue; }
            if (c == L'/' && position_ + 1 < text_.size() && text_[position_ + 1] == L'/') {
                while (position_ < text_.size() && text_[position_] != L'\n') ++position_;
                continue;
            }
            if (c == L'/' && position_ + 1 < text_.size() && text_[position_ + 1] == L'*') {
                const size_t end = text_.find(L"*/", position_ + 2);
                position_ = end == std::wstring::npos ? text_.size() : end + 2;
                continue;
            }
            break;
        }
    }

    bool ParseString(std::wstring& raw, std::wstring& error) {
        const size_t start = position_++;
        while (position_ < text_.size() && text_[position_] != L'"') {
            if (text_[position_] == L'\\') ++position_;
            ++position_;
        }
        if (position_ >= text_.size()) { error = L"JSON 第 " + std::to_wstring(LineAt(start)) + L" 行：字符串没有结束"; return false; }
        ++position_;
        raw = text_.substr(start, position_ - start);
        return true;
    }

    bool ParseValue(JsonNode& node, std::wstring& error) {
        SkipSpace();
        if (position_ >= text_.size()) { error = L"JSON 意外结束"; return false; }
        const wchar_t c = text_[position_];
        if (c == L'{' || c == L'[') {
            const bool object = c == L'{';
            node.type = object ? JsonNode::Object : JsonNode::Array;
            ++position_;
            SkipSpace();
            if (position_ < text_.size() && text_[position_] == (object ? L'}' : L']')) { ++position_; return true; }
            while (true) {
                SkipSpace();
                if (object) {
                    std::wstring key;
                    if (position_ >= text_.size() || text_[position_] != L'"') { error = L"JSON 第 " + std::to_wstring(LineAt(position_)) + L" 行：应为键名"; return false; }
                    if (!ParseString(key, error)) return false;
                    SkipSpace();
                    if (position_ >= text_.size() || text_[position_] != L':') { error = L"JSON 第 " + std::to_wstring(LineAt(position_)) + L" 行：缺少冒号"; return false; }
                    ++position_;
                    JsonNode child;
                    if (!ParseValue(child, error)) return false;
                    node.members.emplace_back(std::move(key), std::move(child));
                } else {
                    JsonNode child;
                    if (!ParseValue(child, error)) return false;
                    node.items.push_back(std::move(child));
                }
                SkipSpace();
                if (position_ < text_.size() && text_[position_] == L',') {
                    ++position_;
                    SkipSpace();
                    if (position_ < text_.size() && text_[position_] == (object ? L'}' : L']')) { ++position_; return true; }  // trailing comma
                    continue;
                }
                if (position_ < text_.size() && text_[position_] == (object ? L'}' : L']')) { ++position_; return true; }
                error = L"JSON 第 " + std::to_wstring(LineAt(position_)) + L" 行：缺少逗号或括号";
                return false;
            }
        }
        node.type = JsonNode::Scalar;
        if (c == L'"') return ParseString(node.raw, error);
        const size_t start = position_;
        while (position_ < text_.size() && !iswspace(text_[position_]) && wcschr(L",}]/", text_[position_]) == nullptr) ++position_;
        if (position_ == start) { error = L"JSON 第 " + std::to_wstring(LineAt(position_)) + L" 行：无法识别的字符"; return false; }
        node.raw = text_.substr(start, position_ - start);
        return true;
    }

    void Write(const JsonNode& node, int depth, std::wstring& out) const {
        if (node.type == JsonNode::Scalar) { out += node.raw; return; }
        const bool object = node.type == JsonNode::Object;
        const size_t count = object ? node.members.size() : node.items.size();
        if (count == 0) { out += object ? L"{}" : L"[]"; return; }
        out += object ? L'{' : L'[';
        for (size_t k = 0; k < count; ++k) {
            if (compact_) out += k ? L", " : L"";
            else {
                out += L'\n';
                for (int d = 0; d <= depth; ++d) out += indent_;
            }
            if (object) {
                out += node.members[k].first + L": ";
                Write(node.members[k].second, depth + 1, out);
            } else {
                Write(node.items[k], depth + 1, out);
            }
            if (!compact_ && k + 1 < count) out += L',';
        }
        if (!compact_) {
            out += L'\n';
            for (int d = 0; d < depth; ++d) out += indent_;
        }
        out += object ? L'}' : L']';
    }

    JsonNode root_;
    std::wstring text_;
    size_t position_ = 0;
    std::wstring indent_;
    bool compact_ = false;
    bool trailingNewline_ = false;
};

// ---- CSV / TSV: rows matched by their first column ----

class CsvDoc : public StructDoc {
public:
    explicit CsvDoc(wchar_t delimiter = 0) : delimiter_(delimiter) {}

    bool Parse(const std::vector<std::wstring>& lines, std::wstring& error) override {
        const std::wstring text = JoinLines(lines, L"\n");
        trailingNewline_ = !lines.empty() && lines.back().empty();
        if (!delimiter_) {
            const std::wstring first = lines.empty() ? std::wstring() : lines.front();
            const size_t commas = std::count(first.begin(), first.end(), L','), tabs = std::count(first.begin(), first.end(), L'\t'),
                         semis = std::count(first.begin(), first.end(), L';');
            delimiter_ = tabs > commas && tabs >= semis ? L'\t' : semis > commas ? L';' : L',';
        }
        records_.clear();
        std::vector<std::wstring> record;
        std::wstring cell;
        bool quoted = false, any = false;
        for (size_t i = 0; i < text.size(); ++i) {
            const wchar_t c = text[i];
            if (quoted) {
                if (c == L'"' && i + 1 < text.size() && text[i + 1] == L'"') { cell += L'"'; ++i; }
                else if (c == L'"') quoted = false;
                else cell += c;
                continue;
            }
            if (c == L'"' && cell.empty()) { quoted = true; any = true; }
            else if (c == delimiter_) { record.push_back(cell); cell.clear(); any = true; }
            else if (c == L'\n') {
                record.push_back(cell);
                cell.clear();
                if (any || record.size() > 1 || !record.front().empty()) records_.push_back(record);
                record.clear();
                any = false;
            } else { cell += c; any = true; }
        }
        if (quoted) { error = L"CSV 中有未闭合的引号"; return false; }
        if (any || !cell.empty()) { record.push_back(cell); records_.push_back(record); }
        if (records_.empty()) { error = L"CSV 是空的"; return false; }
        return true;
    }

    std::vector<StructItem> Items() const override {
        std::vector<StructItem> out;
        const std::vector<std::wstring>& header = records_.front();
        const std::vector<std::wstring> keys = RowKeys();
        for (size_t r = 1; r < records_.size(); ++r) {
            const std::wstring& key = keys[r];
            std::wstring summary;
            for (size_t c = 0; c < records_[r].size(); ++c) summary += (c ? L", " : L"") + records_[r][c];
            out.push_back({key, L"", summary, true});
            for (size_t c = 1; c < header.size(); ++c) {
                if (c < records_[r].size() && !records_[r][c].empty()) out.push_back({key + L" › " + header[c], key, records_[r][c], false});
            }
        }
        return out;
    }

    bool Take(const std::wstring& path, const StructDoc& fromBase, std::wstring& error) override {
        const auto& from = static_cast<const CsvDoc&>(fromBase);
        const size_t split = path.find(L" › ");
        const std::wstring key = split == std::wstring::npos ? path : path.substr(0, split);
        const int sourceRow = from.RowOf(key), targetRow = RowOf(key);
        if (split != std::wstring::npos) {
            const std::wstring column = path.substr(split + 3);
            if (targetRow < 0) { error = L"本侧没有行“" + key + L"”"; return false; }
            int targetColumn = ColumnOf(column);
            if (targetColumn < 0) {
                records_.front().push_back(column);
                targetColumn = static_cast<int>(records_.front().size()) - 1;
            }
            std::vector<std::wstring>& row = records_[static_cast<size_t>(targetRow)];
            if (row.size() <= static_cast<size_t>(targetColumn)) row.resize(static_cast<size_t>(targetColumn) + 1);
            const int sourceColumn = from.ColumnOf(column);
            row[static_cast<size_t>(targetColumn)] = sourceRow >= 0 && sourceColumn >= 0 && static_cast<size_t>(sourceColumn) < from.records_[static_cast<size_t>(sourceRow)].size()
                ? from.records_[static_cast<size_t>(sourceRow)][static_cast<size_t>(sourceColumn)] : L"";
            // A column the other side does not have goes once it is empty.
            if (sourceColumn < 0) {
                bool empty = true;
                for (size_t r = 1; r < records_.size() && empty; ++r) {
                    empty = records_[r].size() <= static_cast<size_t>(targetColumn) || records_[r][static_cast<size_t>(targetColumn)].empty();
                }
                if (empty) {
                    for (auto& record : records_) {
                        if (record.size() > static_cast<size_t>(targetColumn)) record.erase(record.begin() + targetColumn);
                    }
                }
            }
            return true;
        }
        if (sourceRow < 0) {
            if (targetRow > 0) records_.erase(records_.begin() + targetRow);
            return true;
        }
        // Row in this side's column order.
        std::vector<std::wstring> row(records_.front().size());
        for (size_t c = 0; c < row.size(); ++c) {
            const int sourceColumn = c == 0 ? 0 : from.ColumnOf(records_.front()[c]);
            const auto& sourceRecord = from.records_[static_cast<size_t>(sourceRow)];
            if (sourceColumn >= 0 && static_cast<size_t>(sourceColumn) < sourceRecord.size()) row[c] = sourceRecord[static_cast<size_t>(sourceColumn)];
        }
        if (targetRow > 0) { records_[static_cast<size_t>(targetRow)] = row; return true; }
        size_t at = 1;
        const std::vector<std::wstring> sourceKeys = from.RowKeys();
        for (int r = sourceRow - 1; r >= 1; --r) {
            const int here = RowOf(sourceKeys[static_cast<size_t>(r)]);
            if (here > 0) { at = static_cast<size_t>(here) + 1; break; }
        }
        records_.insert(records_.begin() + static_cast<std::ptrdiff_t>(at), row);
        return true;
    }

    std::vector<std::wstring> Lines() const override {
        std::wstring text;
        for (const auto& record : records_) {
            for (size_t c = 0; c < record.size(); ++c) {
                if (c) text += delimiter_;
                const std::wstring& cell = record[c];
                const bool quote = cell.find_first_of(std::wstring(L"\"\r\n") + delimiter_) != std::wstring::npos ||
                    (!cell.empty() && (iswspace(cell.front()) || iswspace(cell.back())));
                if (!quote) { text += cell; continue; }
                text += L'"';
                for (const wchar_t ch : cell) { if (ch == L'"') text += L'"'; text += ch; }
                text += L'"';
            }
            text += L'\n';
        }
        if (!text.empty()) text.pop_back();
        std::vector<std::wstring> lines = SplitLines(text);
        if (trailingNewline_) lines.emplace_back();
        return lines;
    }

private:
    std::vector<std::wstring> RowKeys() const {
        std::vector<std::wstring> keys(records_.size());
        std::unordered_map<std::wstring, int> seen;
        for (size_t r = 1; r < records_.size(); ++r) {
            const std::wstring base = records_[r].empty() || records_[r][0].empty() ? L"(空)" : records_[r][0];
            const int count = ++seen[base];
            keys[r] = count == 1 ? base : base + L" #" + std::to_wstring(count);
        }
        return keys;
    }
    int RowOf(const std::wstring& key) const {
        const std::vector<std::wstring> keys = RowKeys();
        for (size_t r = 1; r < keys.size(); ++r) if (keys[r] == key) return static_cast<int>(r);
        return -1;
    }
    int ColumnOf(const std::wstring& name) const {
        const auto& header = records_.front();
        for (size_t c = 1; c < header.size(); ++c) if (header[c] == name) return static_cast<int>(c);
        return -1;
    }

    wchar_t delimiter_ = 0;
    std::vector<std::vector<std::wstring>> records_;
    bool trailingNewline_ = false;
};

// ---- YAML (block style): edits whole lines, so comments and layout survive ----

class YamlDoc : public StructDoc {
public:
    bool Parse(const std::vector<std::wstring>& lines, std::wstring& error) override {
        lines_ = lines;
        nodes_.clear();
        struct Open { int indent; std::wstring path; bool dash; };
        std::vector<Open> stack;
        std::unordered_map<std::wstring, int> sequenceCounters;
        for (size_t i = 0; i < lines_.size(); ++i) {
            const std::wstring& line = lines_[i];
            const size_t first = line.find_first_not_of(L' ');
            if (first == std::wstring::npos || line[first] == L'#' || line[first] == L'\t') continue;
            if (first == 0 && (line.compare(0, 3, L"---") == 0 || line.compare(0, 3, L"...") == 0)) {
                if (!nodes_.empty()) { error = L"暂不支持包含多个文档（---）的 YAML"; return false; }
                continue;
            }
            int indent = static_cast<int>(first);
            size_t column = first;
            const bool dash = line[first] == L'-' && (first + 1 == line.size() || line[first + 1] == L' ');
            if (dash) {
                while (!stack.empty() && (stack.back().indent > indent || (stack.back().indent == indent && stack.back().dash))) stack.pop_back();
                const std::wstring parent = stack.empty() ? L"" : stack.back().path;
                const int index = sequenceCounters[parent]++;
                Node item;
                item.path = parent + L"[" + std::to_wstring(index) + L"]";
                item.parent = parent;
                item.line = i;
                item.column = column;
                item.dash = true;
                stack.push_back({indent, item.path, true});
                size_t rest = line.find_first_not_of(L' ', first + 1);
                const size_t colon = rest == std::wstring::npos ? std::wstring::npos : KeyColon(line, rest);
                item.prefixEnd = first + 1;
                if (rest == std::wstring::npos || line[rest] == L'#') {
                    nodes_.push_back(item);
                    continue;
                }
                if (colon == std::wstring::npos) {  // "- scalar"
                    item.valueColumn = rest;
                    nodes_.push_back(item);
                    continue;
                }
                item.inlineKey = true;
                nodes_.push_back(item);
                indent = static_cast<int>(rest);
                column = rest;
                sequenceCounters.erase(item.path);
                AddKey(i, column, indent, colon, item.path, true, stack, sequenceCounters);
                continue;
            }
            const size_t colon = KeyColon(line, first);
            if (colon == std::wstring::npos) continue;  // continuation of a multi-line value
            while (!stack.empty() && stack.back().indent >= indent) stack.pop_back();
            AddKey(i, column, indent, colon, stack.empty() ? L"" : stack.back().path, false, stack, sequenceCounters);
        }
        // Block ends.
        for (Node& node : nodes_) {
            size_t end = node.line + 1;
            for (size_t j = node.line + 1; j < lines_.size(); ++j) {
                const size_t first = lines_[j].find_first_not_of(L' ');
                if (first == std::wstring::npos || lines_[j][first] == L'#') continue;
                const bool childDash = lines_[j][first] == L'-' && !node.dash && !node.onDashLine;
                if (static_cast<int>(first) > static_cast<int>(node.column) || (childDash && first == node.column)) end = j + 1;
                else break;
            }
            node.end = end;
            node.container = end > node.line + 1 || node.inlineKey || (node.valueColumn == std::wstring::npos && end > node.line + 1);
        }
        return true;
    }

    std::vector<StructItem> Items() const override {
        std::vector<StructItem> out;
        for (const Node& node : nodes_) {
            StructItem item{node.path, node.parent, {}, node.container};
            item.value = node.container ? L"(" + std::to_wstring(node.end - node.line) + L" 行)" : Value(node);
            out.push_back(item);
        }
        return out;
    }

    bool Take(const std::wstring& path, const StructDoc& fromBase, std::wstring& error) override {
        const auto& from = static_cast<const YamlDoc&>(fromBase);
        const Node* source = from.Find(path);
        const Node* target = Find(path);
        const std::wstring unsupported = L"“" + path + L"”写在“- ”同一行，无法自动合并，请在文本视图中手动修改";
        if (source && target) {
            if (!source->container && !target->container) {
                std::wstring& line = lines_[target->line];
                const std::wstring value = source->valueColumn == std::wstring::npos ? L"" : from.lines_[source->line].substr(source->valueColumn);
                line = line.substr(0, target->prefixEnd) + (value.empty() ? L"" : L" " + value);
            } else {
                if (source->onDashLine || target->onDashLine) { error = unsupported; return false; }
                const std::vector<std::wstring> block = from.Block(*source, static_cast<int>(target->column) - static_cast<int>(source->column));
                ReplaceLines(target->line, target->end, block);
            }
        } else if (target) {
            if (target->onDashLine) { error = unsupported; return false; }
            ReplaceLines(target->line, target->end, {});
        } else if (source) {
            if (source->onDashLine) { error = unsupported; return false; }
            const Node* parent = source->parent.empty() ? nullptr : Find(source->parent);
            if (!source->parent.empty() && !parent) { error = L"本侧没有“" + source->parent + L"”"; return false; }
            const Node* sourceParent = source->parent.empty() ? nullptr : from.Find(source->parent);
            // After the nearest earlier sibling that exists here.
            const Node* previous = nullptr;
            for (const Node& node : from.nodes_) {
                if (&node == source) break;
                if (node.parent == source->parent) {
                    if (const Node* here = Find(node.path)) previous = here;
                }
            }
            size_t at;
            int column;
            if (previous) { at = previous->end; column = static_cast<int>(previous->column); }
            else if (parent) {
                at = parent->line + 1;
                column = static_cast<int>(parent->column) + (static_cast<int>(source->column) - static_cast<int>(sourceParent ? sourceParent->column : 0));
            } else {
                at = 0;
                while (at < lines_.size() && (TrimCopy(lines_[at]).empty() || TrimCopy(lines_[at])[0] == L'#' || lines_[at].compare(0, 3, L"---") == 0)) ++at;
                column = 0;
            }
            if (parent && !parent->container && parent->valueColumn != std::wstring::npos) {
                error = L"本侧的“" + source->parent + L"”是一个值，不是对象或列表";
                return false;
            }
            const std::vector<std::wstring> block = from.Block(*source, column - static_cast<int>(source->column));
            lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(at), block.begin(), block.end());
        }
        std::wstring parseError;
        if (!Parse(std::vector<std::wstring>(lines_), parseError)) { error = parseError; return false; }
        return true;
    }

    std::vector<std::wstring> Lines() const override { return lines_; }

private:
    struct Node {
        std::wstring path, parent;
        size_t line = 0, end = 0;
        size_t column = 0;                         // where the key or "-" starts
        size_t prefixEnd = 0;                      // just after "key:" or "-"
        size_t valueColumn = std::wstring::npos;   // where a scalar value starts
        bool dash = false, onDashLine = false, inlineKey = false, container = false;
    };

    // Colon ending a key ("key: value" or "key:"), outside quotes.
    static size_t KeyColon(const std::wstring& line, size_t from) {
        wchar_t quote = 0;
        for (size_t i = from; i < line.size(); ++i) {
            const wchar_t c = line[i];
            if (quote) { if (c == quote) quote = 0; continue; }
            if ((c == L'"' || c == L'\'') && i == from) { quote = c; continue; }
            if (c == L'#' && i > from && line[i - 1] == L' ') return std::wstring::npos;
            if (c == L':' && (i + 1 == line.size() || line[i + 1] == L' ')) return i;
            if (i == from && (c == L'[' || c == L'{' || c == L'|' || c == L'>')) return std::wstring::npos;
        }
        return std::wstring::npos;
    }

    template <typename Stack>
    void AddKey(size_t i, size_t column, int indent, size_t colon, const std::wstring& parent, bool onDash, Stack& stack,
        std::unordered_map<std::wstring, int>& counters) {
        const std::wstring& line = lines_[i];
        std::wstring key = TrimCopy(std::wstring_view(line).substr(column, colon - column));
        if (key.size() >= 2 && (key.front() == L'"' || key.front() == L'\'') && key.back() == key.front()) key = key.substr(1, key.size() - 2);
        Node node;
        node.path = parent.empty() ? key : parent + L"." + key;
        node.parent = parent;
        node.line = i;
        node.column = column;
        node.onDashLine = onDash;
        const size_t value = line.find_first_not_of(L' ', colon + 1);
        node.prefixEnd = colon + 1;
        node.valueColumn = value == std::wstring::npos || line[value] == L'#' ? std::wstring::npos : value;
        counters.erase(node.path);
        nodes_.push_back(node);
        stack.push_back({indent, node.path, false});
    }

    std::wstring Value(const Node& node) const {
        if (node.valueColumn == std::wstring::npos) return L"";
        std::wstring value = lines_[node.line].substr(node.valueColumn);
        const size_t comment = value.find(L" #");
        if (comment != std::wstring::npos && value[0] != L'"' && value[0] != L'\'') value.erase(comment);
        return TrimCopy(value);
    }

    const Node* Find(const std::wstring& path) const {
        for (const Node& node : nodes_) if (node.path == path) return &node;
        return nullptr;
    }

    std::vector<std::wstring> Block(const Node& node, int shift) const {
        std::vector<std::wstring> block(lines_.begin() + static_cast<std::ptrdiff_t>(node.line), lines_.begin() + static_cast<std::ptrdiff_t>(node.end));
        for (std::wstring& line : block) {
            if (shift > 0) line.insert(0, static_cast<size_t>(shift), L' ');
            else if (shift < 0) {
                const size_t spaces = std::min(static_cast<size_t>(-shift), line.find_first_not_of(L' ') == std::wstring::npos ? line.size() : line.find_first_not_of(L' '));
                line.erase(0, spaces);
            }
        }
        return block;
    }

    void ReplaceLines(size_t start, size_t end, const std::vector<std::wstring>& block) {
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(start), lines_.begin() + static_cast<std::ptrdiff_t>(end));
        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(start), block.begin(), block.end());
    }

    std::vector<std::wstring> lines_;
    std::vector<Node> nodes_;
};

StructFormat DetectStructFormat(const CompareSide& side) {
    const std::wstring name = LowerInvariantSyntax(side.path.empty() ? side.title : side.path);
    const auto ends = [&](const wchar_t* suffix) {
        const size_t n = wcslen(suffix);
        return name.size() >= n && name.compare(name.size() - n, n, suffix) == 0;
    };
    if (ends(L".json") || ends(L".jsonc") || ends(L".json5") || ends(L".geojson")) return StructFormat::Json;
    if (ends(L".csv") || ends(L".tsv")) return StructFormat::Csv;
    if (ends(L".yaml") || ends(L".yml")) return StructFormat::Yaml;
    for (const std::wstring& line : side.lines) {
        const std::wstring trimmed = TrimCopy(line);
        if (trimmed.empty()) continue;
        if (trimmed[0] == L'{' || trimmed[0] == L'[') return StructFormat::Json;
        break;
    }
    return StructFormat::None;
}

std::unique_ptr<StructDoc> MakeStructDoc(StructFormat format, const CompareSide& side) {
    if (format == StructFormat::Json) return std::make_unique<JsonDoc>();
    if (format == StructFormat::Yaml) return std::make_unique<YamlDoc>();
    if (format == StructFormat::Csv) {
        const std::wstring name = LowerInvariantSyntax(side.path.empty() ? side.title : side.path);
        return std::make_unique<CsvDoc>(name.size() > 4 && name.compare(name.size() - 4, 4, L".tsv") == 0 ? L'\t' : wchar_t{0});
    }
    return nullptr;
}

struct StructRow {
    std::wstring path;
    RowKind kind = RowKind::Same;
    std::wstring left, right;
};

// Compares item by item; a subtree that exists on one side only is one row.
std::vector<StructRow> CompareStructItems(const std::vector<StructItem>& left, const std::vector<StructItem>& right) {
    std::unordered_map<std::wstring, size_t> rightIndex, leftIndex;
    for (size_t i = 0; i < right.size(); ++i) rightIndex.emplace(right[i].path, i);
    for (size_t i = 0; i < left.size(); ++i) leftIndex.emplace(left[i].path, i);
    std::vector<StructRow> rows;
    std::unordered_set<std::wstring> covered;  // reported as a whole subtree
    std::unordered_map<std::wstring, size_t> rowOf;
    for (const StructItem& item : left) {
        if (covered.count(item.parent)) { covered.insert(item.path); continue; }
        const auto other = rightIndex.find(item.path);
        if (other == rightIndex.end()) {
            rows.push_back({item.path, RowKind::LeftOnly, item.value, L""});
            covered.insert(item.path);
        } else {
            const StructItem& peer = right[other->second];
            if (item.container && peer.container) {
                rows.push_back({item.path, RowKind::Same, item.value, peer.value});
            } else if (item.container != peer.container || item.value != peer.value) {
                rows.push_back({item.path, RowKind::Changed, item.value, peer.value});
                if (item.container || peer.container) covered.insert(item.path);
            } else {
                rows.push_back({item.path, RowKind::Same, item.value, peer.value});
            }
        }
        rowOf[item.path] = rows.size() - 1;
    }
    // Right-only items go after the row of the item before them on the right.
    // End of each row's subtree (rows are in document order, parents first).
    std::vector<size_t> subtreeEnd(rows.size(), rows.size());
    {
        std::unordered_map<std::wstring, std::wstring> parentOf;
        for (const StructItem& item : left) parentOf.emplace(item.path, item.parent);
        std::vector<size_t> open;
        for (size_t r = 0; r < rows.size(); ++r) {
            const std::wstring& parent = parentOf[rows[r].path];
            while (!open.empty() && rows[open.back()].path != parent) {
                subtreeEnd[open.back()] = r;
                open.pop_back();
            }
            open.push_back(r);
        }
    }
    std::vector<std::pair<size_t, StructRow>> inserts;
    std::unordered_map<std::wstring, std::wstring> lastShared;  // parent -> last child present on both sides
    std::unordered_set<std::wstring> coveredRight;
    for (const StructItem& item : right) {
        if (leftIndex.count(item.path)) {
            if (covered.count(item.path) || coveredRight.count(item.parent)) coveredRight.insert(item.path);
            if (rowOf.count(item.path)) lastShared[item.parent] = item.path;
            continue;
        }
        if (coveredRight.count(item.parent) || covered.count(item.parent)) { coveredRight.insert(item.path); continue; }
        size_t at = 0;
        const auto sibling = lastShared.find(item.parent);
        if (sibling != lastShared.end()) at = subtreeEnd[rowOf[sibling->second]];
        else if (const auto parent = rowOf.find(item.parent); parent != rowOf.end()) at = parent->second + 1;
        inserts.push_back({at, {item.path, RowKind::RightOnly, L"", item.value}});
        coveredRight.insert(item.path);
    }
    // Stable insertion, keeping right-side order for the same anchor.
    std::stable_sort(inserts.begin(), inserts.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<StructRow> merged;
    merged.reserve(rows.size() + inserts.size());
    size_t next = 0;
    for (size_t r = 0; r <= rows.size(); ++r) {
        while (next < inserts.size() && inserts[next].first == r) merged.push_back(inserts[next++].second);
        if (r < rows.size()) merged.push_back(rows[r]);
    }
    while (next < inserts.size()) merged.push_back(inserts[next++].second);
    // Containers present on both sides are only headings; drop identical ones.
    merged.erase(std::remove_if(merged.begin(), merged.end(), [](const StructRow& row) { return row.kind == RowKind::Same; }), merged.end());
    return merged;
}

struct CompareWindow {
    HWND window = nullptr;
    HWND left = nullptr, right = nullptr, stats = nullptr, leftTitle = nullptr, rightTitle = nullptr;
    CompareSide sides[2];
    std::vector<CompareRow> rows;
    std::vector<CompareHunk> hunks;
    int current = -1;          // current hunk
    bool ignoreWhitespace = false;
    bool syncing = false;
    // Structured view (JSON / CSV / YAML)
    bool structMode = false;
    HWND list = nullptr;
    StructFormat format = StructFormat::None;
    std::unique_ptr<StructDoc> docs[2];
    std::vector<StructRow> structRows;
};

bool BuildStructure(CompareWindow& compare, bool showErrors);

std::unordered_map<HWND, WNDPROC> g_compareViewProcs;

CompareWindow* CompareFromChild(HWND child) {
    return reinterpret_cast<CompareWindow*>(GetWindowLongPtrW(GetParent(child), GWLP_USERDATA));
}

// Keeps the two panes on the same row: rows are aligned, so the first
// visible line is shared.
void SyncCompareScroll(CompareWindow& compare, HWND source) {
    if (compare.syncing) return;
    compare.syncing = true;
    HWND other = source == compare.left ? compare.right : compare.left;
    const LRESULT first = SendMessageW(source, EM_GETFIRSTVISIBLELINE, 0, 0);
    const LRESULT otherFirst = SendMessageW(other, EM_GETFIRSTVISIBLELINE, 0, 0);
    if (first != otherFirst) SendMessageW(other, EM_LINESCROLL, 0, first - otherFirst);
    POINT scroll{};
    SendMessageW(source, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    POINT otherScroll{};
    SendMessageW(other, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&otherScroll));
    otherScroll.x = scroll.x;
    SendMessageW(other, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&otherScroll));
    compare.syncing = false;
}

LRESULT CALLBACK CompareViewProc(HWND view, UINT message, WPARAM wParam, LPARAM lParam) {
    const auto it = g_compareViewProcs.find(view);
    const WNDPROC old = it != g_compareViewProcs.end() ? it->second : DefWindowProcW;
    const LRESULT result = CallWindowProcW(old, view, message, wParam, lParam);
    if (message == WM_VSCROLL || message == WM_HSCROLL || message == WM_MOUSEWHEEL || message == WM_KEYDOWN || message == WM_MOUSEHWHEEL) {
        if (CompareWindow* compare = CompareFromChild(view)) SyncCompareScroll(*compare, view);
    }
    if (message == WM_NCDESTROY) g_compareViewProcs.erase(view);
    return result;
}

std::wstring PadNumber(int value, size_t width) {
    std::wstring text = value > 0 ? std::to_wstring(value) : std::wstring();
    if (text.size() < width) text.insert(0, width - text.size(), L' ');
    return text;
}

void RenderCompare(CompareWindow& compare, bool keepScroll) {
    const ThemePalette& palette = CurrentPalette();
    const bool dark = g_app.darkTheme;
    const COLORREF removedBack = Blend(palette.editorBackground, dark ? RGB(240, 90, 90) : RGB(230, 60, 60), dark ? 22 : 16);
    const COLORREF addedBack = Blend(palette.editorBackground, dark ? RGB(80, 200, 120) : RGB(40, 170, 80), dark ? 22 : 16);
    const COLORREF changedBack = Blend(palette.editorBackground, dark ? RGB(220, 180, 80) : RGB(220, 160, 30), dark ? 16 : 14);
    const COLORREF changedInner = Blend(palette.editorBackground, dark ? RGB(220, 180, 80) : RGB(220, 160, 30), dark ? 42 : 36);
    const COLORREF fillerBack = Blend(palette.editorBackground, palette.gutterText, 10);

    const size_t widthLeft = std::to_wstring(std::max<size_t>(1, compare.sides[0].lines.size())).size();
    const size_t widthRight = std::to_wstring(std::max<size_t>(1, compare.sides[1].lines.size())).size();
    const size_t width = std::max(widthLeft, widthRight);
    // Changed rows are padded with spaces so their tint runs the full row.
    size_t longest = 0;
    for (const CompareSide& side : compare.sides) {
        for (const std::wstring& line : side.lines) longest = std::max(longest, line.size());
    }
    size_t visibleColumns = 0;
    if (HDC dc = GetDC(compare.left)) {
        HGDIOBJ previous = SelectObject(dc, g_app.editorFont);
        TEXTMETRICW metrics{};
        GetTextMetricsW(dc, &metrics);
        SelectObject(dc, previous);
        ReleaseDC(compare.left, dc);
        RECT client{};
        GetClientRect(compare.left, &client);
        if (metrics.tmAveCharWidth > 0) visibleColumns = static_cast<size_t>(client.right / metrics.tmAveCharWidth);
    }
    const size_t padTo = std::max(width + 2 + std::min<size_t>(longest, 400) + 4, visibleColumns > 2 ? visibleColumns - 2 : 0);
    HWND views[2] = {compare.left, compare.right};
    POINT scroll[2]{};
    for (int s = 0; s < 2; ++s) SendMessageW(views[s], EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll[s]));

    for (int s = 0; s < 2; ++s) {
        const CompareSide& side = compare.sides[s];
        std::wstring text;
        std::vector<LONG> rowStart;
        rowStart.reserve(compare.rows.size() + 1);
        for (const CompareRow& row : compare.rows) {
            rowStart.push_back(static_cast<LONG>(text.size()));
            const int index = s == 0 ? row.left : row.right;
            const size_t before = text.size();
            if (index >= 0) text += PadNumber(index + 1, width) + L"  " + side.lines[static_cast<size_t>(index)];
            else text += std::wstring(width, L' ') + L"  ";
            if (row.kind != RowKind::Same && text.size() - before < padTo) text.append(padTo - (text.size() - before), L' ');
            text += L'\r';
        }
        rowStart.push_back(static_cast<LONG>(text.size()));
        HWND view = views[s];
        SendMessageW(view, WM_SETREDRAW, FALSE, 0);
        SendMessageW(view, EM_SETBKGNDCOLOR, 0, palette.editorBackground);
        CHARFORMAT2W base{};
        base.cbSize = sizeof(base);
        base.dwMask = CFM_COLOR | CFM_BACKCOLOR | CFM_BOLD;
        base.crTextColor = palette.editorText;
        base.crBackColor = palette.editorBackground;
        SendMessageW(view, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&base));
        SendMessageW(view, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&base));
        SendMessageW(view, EM_SETREADONLY, FALSE, 0);  // TOM refuses to format read-only text
        SetWindowTextW(view, text.c_str());

        TextDocumentRef tom(view);
        if (tom) {
            ITextFont* gutter = tom.MakeStyleFont(palette.gutterText, false);
            ITextFont* backs[5]{};
            const COLORREF colors[5] = {removedBack, addedBack, changedBack, changedInner, fillerBack};
            for (int c = 0; c < 5; ++c) {
                ITextFont* font = tom.MakeStyleFont(palette.editorText, false);
                if (font) {
                    font->Reset(tomUndefined);
                    font->SetBackColor(static_cast<long>(colors[c]));
                }
                backs[c] = font;
            }
            for (size_t r = 0; r < compare.rows.size(); ++r) {
                const CompareRow& row = compare.rows[r];
                const LONG start = rowStart[r];
                const LONG end = rowStart[r + 1] - 1;  // without the line break
                const int index = s == 0 ? row.left : row.right;
                tom.ApplyFont(start, start + static_cast<LONG>(width), gutter);
                if (row.kind == RowKind::Same) continue;
                if (index < 0) {
                    tom.ApplyFont(start, end, backs[4]);
                    continue;
                }
                const bool onlyHere = (s == 0 && row.kind == RowKind::LeftOnly) || (s == 1 && row.kind == RowKind::RightOnly);
                tom.ApplyFont(start, end, onlyHere ? backs[s == 0 ? 0 : 1] : backs[2]);
                if (row.kind == RowKind::Changed) {
                    // Highlight just the part of the line that differs.
                    const std::wstring& a = compare.sides[0].lines[static_cast<size_t>(row.left)];
                    const std::wstring& b = compare.sides[1].lines[static_cast<size_t>(row.right)];
                    size_t pre = 0;
                    while (pre < a.size() && pre < b.size() && a[pre] == b[pre]) ++pre;
                    size_t suf = 0;
                    while (suf < a.size() - pre && suf < b.size() - pre && a[a.size() - 1 - suf] == b[b.size() - 1 - suf]) ++suf;
                    const std::wstring& mine = s == 0 ? a : b;
                    const LONG textStart = start + static_cast<LONG>(width) + 2;
                    if (mine.size() > pre + suf) tom.ApplyFont(textStart + static_cast<LONG>(pre), textStart + static_cast<LONG>(mine.size() - suf), backs[3]);
                }
            }
            if (gutter) gutter->Release();
            for (ITextFont* font : backs) if (font) font->Release();
            SendMessageW(view, EM_EMPTYUNDOBUFFER, 0, 0);
        }
        if (keepScroll) SendMessageW(view, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll[s]));
        SendMessageW(view, EM_SETREADONLY, TRUE, 0);
        SendMessageW(view, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(view, nullptr, TRUE);
    }

    int changed = 0, leftOnly = 0, rightOnly = 0;
    for (const CompareRow& row : compare.rows) {
        changed += row.kind == RowKind::Changed;
        leftOnly += row.kind == RowKind::LeftOnly;
        rightOnly += row.kind == RowKind::RightOnly;
    }
    std::wstring stats = compare.hunks.empty() ? std::wstring(L"两侧内容相同")
        : L"共 " + std::to_wstring(compare.hunks.size()) + L" 处差异：修改 " + std::to_wstring(changed) + L" 行，仅左侧 " +
          std::to_wstring(leftOnly) + L" 行，仅右侧 " + std::to_wstring(rightOnly) + L" 行";
    if (compare.ignoreWhitespace) stats += L"（已忽略空白）";
    if (!compare.structMode) SetWindowTextW(compare.stats, stats.c_str());
    for (int s = 0; s < 2; ++s) {
        const CompareSide& side = compare.sides[s];
        std::wstring title = (side.dirty ? L"● " : L"") + (side.path.empty() ? side.title : side.path);
        if (side.editor && !side.path.empty()) title += L"（来自标签）";
        SetWindowTextW(s == 0 ? compare.leftTitle : compare.rightTitle, title.c_str());
    }
    EnableWindow(GetDlgItem(compare.window, kCmpSaveLeft), compare.sides[0].dirty);
    EnableWindow(GetDlgItem(compare.window, kCmpSaveRight), compare.sides[1].dirty);
    if (!compare.structMode) {
        for (const int id : {kCmpPrev, kCmpNext, kCmpToRight, kCmpToLeft}) EnableWindow(GetDlgItem(compare.window, id), !compare.hunks.empty());
    }
}

void RecomputeCompare(CompareWindow& compare, bool keepScroll) {
    compare.rows = BuildCompareRows(compare.sides[0], compare.sides[1], compare.ignoreWhitespace, compare.hunks);
    compare.current = std::min(compare.current, static_cast<int>(compare.hunks.size()) - 1);
    RenderCompare(compare, keepScroll);
    if (compare.structMode && !BuildStructure(compare, true)) {
        compare.structMode = false;
        ShowWindow(compare.list, SW_HIDE);
        ShowWindow(compare.left, SW_SHOW);
        ShowWindow(compare.right, SW_SHOW);
        SetWindowTextW(GetDlgItem(compare.window, kCmpStructure), L"结构比较");
    }
}

void GoToHunk(CompareWindow& compare, int index) {
    if (compare.hunks.empty()) return;
    index = (index % static_cast<int>(compare.hunks.size()) + static_cast<int>(compare.hunks.size())) % static_cast<int>(compare.hunks.size());
    compare.current = index;
    const CompareHunk& hunk = compare.hunks[static_cast<size_t>(index)];
    for (HWND view : {compare.left, compare.right}) {
        const LRESULT first = SendMessageW(view, EM_GETFIRSTVISIBLELINE, 0, 0);
        SendMessageW(view, EM_LINESCROLL, 0, std::max(0, hunk.firstRow - 3) - first);
        const LRESULT position = SendMessageW(view, EM_LINEINDEX, hunk.firstRow, 0);
        SendMessageW(view, EM_SETSEL, position, position);
    }
    SyncCompareScroll(compare, compare.left);
    SetWindowTextW(compare.stats, (L"第 " + std::to_wstring(index + 1) + L" / " + std::to_wstring(compare.hunks.size()) + L" 处差异").c_str());
}

// The hunk at the caret of the focused pane, else the current one.
int HunkAtCaret(CompareWindow& compare) {
    HWND view = GetFocus() == compare.right ? compare.right : compare.left;
    CHARRANGE selection{};
    SendMessageW(view, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const int row = static_cast<int>(SendMessageW(view, EM_EXLINEFROMCHAR, 0, selection.cpMin));
    for (size_t h = 0; h < compare.hunks.size(); ++h) {
        if (row >= compare.hunks[h].firstRow && row <= compare.hunks[h].lastRow) return static_cast<int>(h);
    }
    return compare.current;
}

// Takes one block from one side into the other (block-level merge).
void CopyHunk(CompareWindow& compare, bool toRight) {
    const int index = HunkAtCaret(compare);
    if (index < 0 || index >= static_cast<int>(compare.hunks.size())) {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    const CompareHunk hunk = compare.hunks[static_cast<size_t>(index)];
    CompareSide& from = compare.sides[toRight ? 0 : 1];
    CompareSide& to = compare.sides[toRight ? 1 : 0];
    const int fromStart = toRight ? hunk.leftStart : hunk.rightStart;
    const int fromCount = toRight ? hunk.leftCount : hunk.rightCount;
    const int toStart = toRight ? hunk.rightStart : hunk.leftStart;
    const int toCount = toRight ? hunk.rightCount : hunk.leftCount;
    std::vector<std::wstring> block(from.lines.begin() + fromStart, from.lines.begin() + fromStart + fromCount);
    to.lines.erase(to.lines.begin() + toStart, to.lines.begin() + toStart + toCount);
    to.lines.insert(to.lines.begin() + toStart, block.begin(), block.end());
    to.dirty = true;
    compare.current = index;
    RecomputeCompare(compare, true);
    if (!compare.hunks.empty()) GoToHunk(compare, std::min(index, static_cast<int>(compare.hunks.size()) - 1));
}

bool SaveCompareSide(CompareWindow& compare, int s) {
    CompareSide& side = compare.sides[s];
    std::wstring text;
    if (side.editor) {
        const int index = IndexOfEditor(side.editor);
        if (index < 0) {
            MessageBoxW(compare.window, L"来源标签已关闭，无法写回。", kWindowTitle, MB_ICONWARNING);
            return false;
        }
        text = JoinLines(side.lines, L"\r");
        SendMessageW(side.editor, EM_SETSEL, 0, -1);
        SendMessageW(side.editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
        side.dirty = false;
        RenderCompare(compare, true);
        ShowNotice(L"已写回标签“" + g_app.documents[static_cast<size_t>(index)].title + L"”（未保存到磁盘，可撤销）");
        return true;
    }
    text = JoinLines(side.lines, side.eol);
    if (side.encoding == kEncodingAnsi && !IsAnsiRepresentable(text)) {
        MessageBoxW(compare.window, L"内容包含 ANSI 编码无法表示的字符，未保存。", kWindowTitle, MB_ICONWARNING);
        return false;
    }
    std::string bytes;
    if (!EncodeTextBytes(text, side.encoding, bytes) || !WriteAtomicFile(side.path, bytes)) {
        MessageBoxW(compare.window, (L"保存失败：" + side.path).c_str(), kWindowTitle, MB_ICONERROR);
        return false;
    }
    side.dirty = false;
    for (size_t d = 0; d < g_app.documents.size(); ++d) {
        const TabDocument& document = g_app.documents[d];
        if (!document.untitled && !document.modified && SamePath(document.path, side.path)) ReloadDocumentFromDisk(static_cast<int>(d));
    }
    RenderCompare(compare, true);
    ShowNotice(L"已保存 " + fs::path(side.path).filename().wstring());
    return true;
}

void LayoutCompareWindow(CompareWindow& compare) {
    RECT client{};
    GetClientRect(compare.window, &client);
    const int width = client.right, height = client.bottom;
    int x = Scale(10);
    const int buttonY = Scale(8), buttonH = Scale(28);
    const std::pair<int, int> buttons[] = {{kCmpPrev, 72}, {kCmpNext, 72}, {kCmpToRight, 108}, {kCmpToLeft, 108}, {kCmpSwap, 56},
        {kCmpStructure, 84}, {kCmpIgnoreSpace, 92}, {kCmpSaveLeft, 80}, {kCmpSaveRight, 80}, {kCmpRefresh, 56}};
    for (const auto& [id, w] : buttons) {
        MoveWindow(GetDlgItem(compare.window, id), x, buttonY, Scale(w), buttonH, TRUE);
        x += Scale(w) + Scale(6);
    }
    MoveWindow(compare.stats, Scale(10), buttonY + buttonH + Scale(6), std::max(0, width - Scale(20)), Scale(20), TRUE);
    const int half = (width - Scale(30)) / 2;
    const int titleY = buttonY + buttonH + Scale(30);
    MoveWindow(compare.leftTitle, Scale(10), titleY, half, Scale(20), TRUE);
    MoveWindow(compare.rightTitle, Scale(20) + half, titleY, half, Scale(20), TRUE);
    const int viewY = titleY + Scale(24);
    MoveWindow(compare.left, Scale(10), viewY, half, std::max(0, height - viewY - Scale(10)), TRUE);
    MoveWindow(compare.right, Scale(20) + half, viewY, half, std::max(0, height - viewY - Scale(10)), TRUE);
    if (compare.list) {
        MoveWindow(compare.list, Scale(10), viewY, std::max(0, width - Scale(20)), std::max(0, height - viewY - Scale(10)), TRUE);
        const int column = std::max(Scale(120), (width - Scale(20) - Scale(70) - Scale(30)) / 3);
        ListView_SetColumnWidth(compare.list, 0, column);
        ListView_SetColumnWidth(compare.list, 1, Scale(70));
        ListView_SetColumnWidth(compare.list, 2, column);
        ListView_SetColumnWidth(compare.list, 3, column);
    }
}

bool ConfirmDiscardCompare(CompareWindow& compare) {
    if (!compare.sides[0].dirty && !compare.sides[1].dirty) return true;
    const int choice = MessageBoxW(compare.window, L"合并后的修改还没有保存。是否保存？", kWindowTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return false;
    if (choice == IDYES) {
        for (int s = 0; s < 2; ++s) {
            if (compare.sides[s].dirty && !SaveCompareSide(compare, s)) return false;
        }
    }
    return true;
}

// ---- Structured view inside the compare window ------------------------------------

const wchar_t* StructFormatName(StructFormat format) {
    switch (format) {
    case StructFormat::Json: return L"JSON";
    case StructFormat::Csv: return L"CSV";
    case StructFormat::Yaml: return L"YAML";
    default: return L"";
    }
}

void FillStructList(CompareWindow& compare) {
    HWND list = compare.list;
    SendMessageW(list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(list);
    static const wchar_t* kinds[] = {L"相同", L"修改", L"仅左侧", L"仅右侧"};
    for (size_t r = 0; r < compare.structRows.size(); ++r) {
        StructRow& row = compare.structRows[r];
        std::wstring cells[4] = {row.path, kinds[static_cast<int>(row.kind)], row.left, row.right};
        for (std::wstring& cell : cells) if (cell.size() > 300) cell = cell.substr(0, 300) + L"…";
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(r);
        item.pszText = cells[0].data();
        ListView_InsertItem(list, &item);
        for (int c = 1; c < 4; ++c) ListView_SetItemText(list, static_cast<int>(r), c, cells[c].data());
    }
    SendMessageW(list, WM_SETREDRAW, TRUE, 0);
}

// Parses both sides and lists the differing items. Returns false (and
// explains why) when the text is not valid for the format.
bool BuildStructure(CompareWindow& compare, bool showErrors) {
    StructFormat format = DetectStructFormat(compare.sides[0]);
    if (format == StructFormat::None) format = DetectStructFormat(compare.sides[1]);
    if (format == StructFormat::None) {
        if (showErrors) MessageBoxW(compare.window, L"结构比较支持 JSON、CSV/TSV 和 YAML 文件（按扩展名或内容识别）。", kWindowTitle, MB_ICONINFORMATION);
        return false;
    }
    std::unique_ptr<StructDoc> docs[2];
    for (int s = 0; s < 2; ++s) {
        docs[s] = MakeStructDoc(format, compare.sides[s]);
        std::wstring error;
        if (!docs[s]->Parse(compare.sides[s].lines, error)) {
            if (showErrors) MessageBoxW(compare.window, ((s == 0 ? L"左侧" : L"右侧") + std::wstring(L"无法按 ") + StructFormatName(format) + L" 解析：\n" + error).c_str(), kWindowTitle, MB_ICONWARNING);
            return false;
        }
    }
    compare.format = format;
    compare.docs[0] = std::move(docs[0]);
    compare.docs[1] = std::move(docs[1]);
    compare.structRows = CompareStructItems(compare.docs[0]->Items(), compare.docs[1]->Items());
    FillStructList(compare);
    int counts[4]{};
    for (const StructRow& row : compare.structRows) ++counts[static_cast<int>(row.kind)];
    std::wstring stats = std::wstring(StructFormatName(format)) + L" 结构比较：";
    stats += compare.structRows.empty() ? std::wstring(L"所有项都相同")
        : std::to_wstring(compare.structRows.size()) + L" 项不同（修改 " + std::to_wstring(counts[1]) + L"，仅左侧 " + std::to_wstring(counts[2]) +
          L"，仅右侧 " + std::to_wstring(counts[3]) + L"）";
    if (format == StructFormat::Json) stats += L"  · 采用后该侧 JSON 按原缩进重新排版";
    SetWindowTextW(compare.stats, stats.c_str());
    for (const int id : {kCmpPrev, kCmpNext, kCmpToRight, kCmpToLeft}) EnableWindow(GetDlgItem(compare.window, id), !compare.structRows.empty());
    return true;
}

void SetStructMode(CompareWindow& compare, bool on) {
    if (on && !BuildStructure(compare, true)) on = false;
    compare.structMode = on;
    SetWindowTextW(GetDlgItem(compare.window, kCmpStructure), on ? L"文本比较" : L"结构比较");
    ShowWindow(compare.list, on ? SW_SHOW : SW_HIDE);
    ShowWindow(compare.left, on ? SW_HIDE : SW_SHOW);
    ShowWindow(compare.right, on ? SW_HIDE : SW_SHOW);
    EnableWindow(GetDlgItem(compare.window, kCmpIgnoreSpace), !on);
    if (on) {
        if (!compare.structRows.empty()) {
            ListView_SetItemState(compare.list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
        SetFocus(compare.list);
    } else {
        RecomputeCompare(compare, true);
    }
}

void StepStructRow(CompareWindow& compare, int step) {
    const int count = ListView_GetItemCount(compare.list);
    if (count == 0) return;
    int row = ListView_GetNextItem(compare.list, -1, LVNI_FOCUSED);
    row = row < 0 ? 0 : ((row + step) % count + count) % count;
    ListView_SetItemState(compare.list, -1, 0, LVIS_SELECTED);
    ListView_SetItemState(compare.list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(compare.list, row, FALSE);
    SetFocus(compare.list);
}

// Takes the selected items from one side into the other.
void TakeStructItems(CompareWindow& compare, bool toRight) {
    std::vector<int> picked;
    for (int row = -1; (row = ListView_GetNextItem(compare.list, row, LVNI_SELECTED)) >= 0;) picked.push_back(row);
    if (picked.empty()) {
        MessageBoxW(compare.window, L"请先在列表中选中要合并的项（可按住 Ctrl 或 Shift 多选）。", kWindowTitle, MB_ICONINFORMATION);
        return;
    }
    const int target = toRight ? 1 : 0;
    StructDoc& into = *compare.docs[target];
    const StructDoc& from = *compare.docs[toRight ? 0 : 1];
    std::wstring failures;
    int done = 0;
    // From the bottom up, so removing array elements keeps earlier paths valid.
    for (auto it = picked.rbegin(); it != picked.rend(); ++it) {
        const std::wstring& path = compare.structRows[static_cast<size_t>(*it)].path;
        std::wstring error;
        if (into.Take(path, from, error)) ++done;
        else failures += L"\n• " + error;
    }
    if (done > 0) {
        compare.sides[target].lines = into.Lines();
        compare.sides[target].dirty = true;
        const int first = picked.front();
        RecomputeCompare(compare, true);  // rebuilds the item list as well
        const int count = ListView_GetItemCount(compare.list);
        if (count > 0) {
            const int row = std::min(first, count - 1);
            ListView_SetItemState(compare.list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(compare.list, row, FALSE);
        }
        SetFocus(compare.list);
    }
    if (!failures.empty()) MessageBoxW(compare.window, (L"部分项无法合并：" + failures).c_str(), kWindowTitle, MB_ICONWARNING);
}

LRESULT CALLBACK CompareWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* compare = reinterpret_cast<CompareWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        compare = static_cast<CompareWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(compare));
        compare->window = window;
        const std::pair<int, const wchar_t*> buttons[] = {{kCmpPrev, L"上一处"}, {kCmpNext, L"下一处"}, {kCmpToRight, L"复制到右侧 →"},
            {kCmpToLeft, L"← 复制到左侧"}, {kCmpSwap, L"交换"}, {kCmpStructure, L"结构比较"}, {kCmpSaveLeft, L"保存左侧"}, {kCmpSaveRight, L"保存右侧"}, {kCmpRefresh, L"刷新"}};
        std::vector<HWND> pushButtons;
        for (const auto& [id, text] : buttons) {
            HWND button = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app.instance, nullptr);
            SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            pushButtons.push_back(button);
        }
        for (HWND button : pushButtons) if (g_app.darkTheme) SetWindowTheme(button, L"DarkMode_Explorer", nullptr);
        HWND ignore = CreateWindowExW(0, L"BUTTON", L"忽略空白", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCmpIgnoreSpace)), g_app.instance, nullptr);
        SendMessageW(ignore, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        ThemeCheckbox(ignore);
        compare->stats = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_ENDELLIPSIS, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
        compare->leftTitle = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_PATHELLIPSIS, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
        compare->rightTitle = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_PATHELLIPSIS, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
        for (HWND label : {compare->stats, compare->leftTitle, compare->rightTitle}) SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        for (int s = 0; s < 2; ++s) {
            HWND view = CreateWindowExW(0, L"RICHEDIT50W", nullptr,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
                0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(s == 0 ? kCmpLeftView : kCmpRightView)), g_app.instance, nullptr);
            SendMessageW(view, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
            SendMessageW(view, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
            SendMessageW(view, EM_SETTARGETDEVICE, 0, 1);  // no wrapping: rows must stay aligned
            SendMessageW(view, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(Scale(4), 0));
            const LRESULT options = SendMessageW(view, EM_GETLANGOPTIONS, 0, 0);
            SendMessageW(view, EM_SETLANGOPTIONS, 0, options & ~static_cast<LRESULT>(IMF_AUTOFONT | IMF_DUALFONT | IMF_AUTOFONTSIZEADJUST));
            SetWindowTheme(view, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            g_compareViewProcs[view] = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(view, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(CompareViewProc)));
            (s == 0 ? compare->left : compare->right) = view;
        }
        compare->list = CreateWindowExW(0, WC_LISTVIEWW, nullptr, WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCmpList)), g_app.instance, nullptr);
        ListView_SetExtendedListViewStyle(compare->list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
        SendMessageW(compare->list, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        SetWindowTheme(compare->list, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        if (HWND header = ListView_GetHeader(compare->list)) SetWindowTheme(header, g_app.darkTheme ? L"DarkMode_ItemsView" : nullptr, nullptr);
        ListView_SetBkColor(compare->list, CurrentPalette().editorBackground);
        ListView_SetTextBkColor(compare->list, CurrentPalette().editorBackground);
        ListView_SetTextColor(compare->list, CurrentPalette().editorText);
        const wchar_t* columns[] = {L"路径", L"状态", L"左侧值", L"右侧值"};
        for (int c = 0; c < 4; ++c) {
            LVCOLUMNW column{};
            column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.pszText = const_cast<wchar_t*>(columns[c]);
            column.cx = Scale(c == 1 ? 70 : 240);
            ListView_InsertColumn(compare->list, c, &column);
        }
        return 0;
    }
    case WM_NOTIFY: {
        auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (compare && header->hwndFrom == compare->list && header->code == NM_CUSTOMDRAW) {
            auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(lParam);
            if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                const size_t row = static_cast<size_t>(draw->nmcd.dwItemSpec);
                if (row < compare->structRows.size()) {
                    const RowKind kind = compare->structRows[row].kind;
                    const bool dark = g_app.darkTheme;
                    draw->clrText = kind == RowKind::Changed ? (dark ? RGB(229, 192, 123) : RGB(176, 110, 0))
                        : kind == RowKind::LeftOnly ? (dark ? RGB(244, 135, 113) : RGB(190, 40, 40))
                        : kind == RowKind::RightOnly ? (dark ? RGB(129, 199, 132) : RGB(22, 130, 52))
                        : CurrentPalette().editorText;
                }
                return CDRF_DODEFAULT;
            }
        }
        break;
    }
    case WM_SIZE:
        if (compare && compare->left) {
            LayoutCompareWindow(*compare);
            if (!compare->rows.empty() && wParam != SIZE_MINIMIZED) SetTimer(window, 1, 150, nullptr);
        }
        return 0;
    case WM_TIMER:
        KillTimer(window, 1);
        if (compare) RenderCompare(*compare, true);
        return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = POINT{Scale(760), Scale(360)};
        return 0;
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(window, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, g_app.toolbarBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, reinterpret_cast<HWND>(lParam) == (compare ? compare->stats : nullptr) ? CurrentPalette().statusText : CurrentPalette().toolbarText);
        SetBkColor(dc, CurrentPalette().toolbarBackground);
        return reinterpret_cast<LRESULT>(g_app.toolbarBrush);
    }
    case WM_COMMAND:
        if (!compare) break;
        switch (LOWORD(wParam)) {
        case kCmpPrev:
            if (compare->structMode) StepStructRow(*compare, -1);
            else GoToHunk(*compare, compare->current - 1);
            return 0;
        case kCmpNext:
            if (compare->structMode) StepStructRow(*compare, 1);
            else GoToHunk(*compare, compare->current + 1);
            return 0;
        case kCmpToRight:
            if (compare->structMode) TakeStructItems(*compare, true);
            else CopyHunk(*compare, true);
            return 0;
        case kCmpToLeft:
            if (compare->structMode) TakeStructItems(*compare, false);
            else CopyHunk(*compare, false);
            return 0;
        case kCmpStructure:
            SetStructMode(*compare, !compare->structMode);
            return 0;
        case kCmpSwap:
            std::swap(compare->sides[0], compare->sides[1]);
            RecomputeCompare(*compare, true);
            return 0;
        case kCmpIgnoreSpace:
            compare->ignoreWhitespace = SendMessageW(GetDlgItem(window, kCmpIgnoreSpace), BM_GETCHECK, 0, 0) == BST_CHECKED;
            RecomputeCompare(*compare, true);
            return 0;
        case kCmpSaveLeft: SaveCompareSide(*compare, 0); return 0;
        case kCmpSaveRight: SaveCompareSide(*compare, 1); return 0;
        case kCmpRefresh:
            if (!ConfirmDiscardCompare(*compare)) return 0;
            for (CompareSide& side : compare->sides) {
                std::wstring error;
                const int index = side.editor ? IndexOfEditor(side.editor) : -1;
                if (index >= 0) LoadCompareSideFromTab(side, g_app.documents[static_cast<size_t>(index)]);
                else if (!side.path.empty()) LoadCompareSideFromFile(side, side.path, error);
            }
            RecomputeCompare(*compare, true);
            return 0;
        case IDCANCEL:
            SendMessageW(window, WM_CLOSE, 0, 0);
            return 0;
        default:
            break;
        }
        break;
    case WM_KEYDOWN:
        break;
    case WM_CLOSE:
        if (compare && !ConfirmDiscardCompare(*compare)) return 0;
        DestroyWindow(window);
        return 0;
    case WM_NCDESTROY:
        delete compare;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        break;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

std::vector<HWND> g_toolWindows;  // compare / log windows: get IsDialogMessage and F7/F8 keys

void OpenCompareWindow(CompareSide left, CompareSide right, bool structure = false) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{};
        klass.hInstance = g_app.instance;
        klass.lpfnWndProc = CompareWindowProc;
        klass.lpszClassName = L"MCnotepadCompare";
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        klass.hIcon = LoadIconW(g_app.instance, MAKEINTRESOURCEW(IDI_MCNOTEPAD));
        registered = RegisterClassW(&klass) != 0;
    }
    auto* compare = new CompareWindow();
    compare->sides[0] = std::move(left);
    compare->sides[1] = std::move(right);
    RECT owner{};
    GetWindowRect(g_app.window, &owner);
    const std::wstring title = L"比较 - " + compare->sides[0].title + L" ↔ " + compare->sides[1].title;
    HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, L"MCnotepadCompare", title.c_str(), WS_OVERLAPPEDWINDOW,
        owner.left + Scale(30), owner.top + Scale(30), std::max(Scale(980), static_cast<int>(owner.right - owner.left) - Scale(60)),
        std::max(Scale(560), static_cast<int>(owner.bottom - owner.top) - Scale(60)), nullptr, nullptr, g_app.instance, compare);
    if (!window) {
        delete compare;
        return;
    }
    ApplyWindowFrameTheme(window);
    g_toolWindows.push_back(window);
    LayoutCompareWindow(*compare);
    RecomputeCompare(*compare, false);
    ShowWindow(window, SW_SHOW);
    if (!compare->hunks.empty()) GoToHunk(*compare, 0);
    if (structure) SetStructMode(*compare, true);
}

std::wstring PickFile(const wchar_t* title) {
    wchar_t fileName[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_app.window;
    dialog.lpstrFilter = L"所有文件\0*.*\0";
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrTitle = title;
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
    return GetOpenFileNameW(&dialog) ? std::wstring(fileName) : std::wstring();
}

std::wstring PickFolder(const wchar_t* title) {
    std::wstring result;
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return result;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dialog->SetTitle(title);
    IShellItem* item = nullptr;
    if (SUCCEEDED(dialog->Show(g_app.window)) && SUCCEEDED(dialog->GetResult(&item))) {
        PWSTR path = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
            result = path;
            CoTaskMemFree(path);
        }
        item->Release();
    }
    dialog->Release();
    return result;
}

void CompareFiles(const std::wstring& leftPath, const std::wstring& rightPath, bool structure) {
    CompareSide left, right;
    std::wstring error;
    if (!LoadCompareSideFromFile(left, leftPath, error) || !LoadCompareSideFromFile(right, rightPath, error)) {
        MessageBoxW(g_app.window, error.c_str(), kWindowTitle, MB_ICONWARNING);
        return;
    }
    OpenCompareWindow(std::move(left), std::move(right), structure);
}

void CompareTwoFilesCommand(bool structure) {
    const std::wstring left = PickFile(L"选择左侧文件（原文件）");
    if (left.empty()) return;
    const std::wstring right = PickFile(L"选择右侧文件（新文件）");
    if (right.empty()) return;
    CompareFiles(left, right, structure);
}

// Current tab (with unsaved edits) on the left, a file on the right.
void CompareActiveWithFileCommand() {
    TabDocument* document = ActiveDocument();
    if (!document) return;
    const std::wstring right = PickFile(L"选择要与当前标签比较的文件");
    if (right.empty()) return;
    CompareSide left, other;
    LoadCompareSideFromTab(left, *document);
    std::wstring error;
    if (!LoadCompareSideFromFile(other, right, error)) {
        MessageBoxW(g_app.window, error.c_str(), kWindowTitle, MB_ICONWARNING);
        return;
    }
    OpenCompareWindow(std::move(left), std::move(other));
}

void CompareTabs(int leftIndex, int rightIndex) {
    if (leftIndex < 0 || rightIndex < 0 || leftIndex >= static_cast<int>(g_app.documents.size()) || rightIndex >= static_cast<int>(g_app.documents.size())) return;
    CompareSide left, right;
    LoadCompareSideFromTab(left, g_app.documents[static_cast<size_t>(leftIndex)]);
    LoadCompareSideFromTab(right, g_app.documents[static_cast<size_t>(rightIndex)]);
    OpenCompareWindow(std::move(left), std::move(right));
}

// ---- Folder compare --------------------------------------------------------------

enum class EntryState { Same, Different, LeftOnly, RightOnly };

struct FolderEntry {
    std::wstring relative;
    EntryState state = EntryState::Same;
    uint64_t leftSize = 0, rightSize = 0;
    uint64_t leftTime = 0, rightTime = 0;
};

struct FolderCompare {
    HWND window = nullptr, list = nullptr, summary = nullptr, onlyDiff = nullptr;
    std::wstring roots[2];
    std::vector<FolderEntry> entries;
    std::vector<int> shown;  // indices into entries for the list rows
    std::atomic<bool> running{false};
};

enum FolderControl : int { kFolderOnlyDiff = 4101, kFolderOpen, kFolderToRight, kFolderToLeft, kFolderRefresh, kFolderList };
constexpr UINT kMessageFolderDone = WM_APP + 30;

void ListFolderFiles(const std::wstring& root, std::unordered_map<std::wstring, std::pair<uint64_t, uint64_t>>& files) {
    std::vector<std::wstring> stack{root};
    while (!stack.empty()) {
        const std::wstring directory = stack.back();
        stack.pop_back();
        WIN32_FIND_DATAW data{};
        HANDLE find = FindFirstFileExW((directory + L"\\*").c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (find == INVALID_HANDLE_VALUE) continue;
        do {
            const std::wstring name = data.cFileName;
            if (name == L"." || name == L"..") continue;
            const std::wstring full = directory + L"\\" + name;
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (name == L".git" || name == L"node_modules" || (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
                stack.push_back(full);
            } else {
                const uint64_t size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
                const uint64_t time = (static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
                files[full.substr(root.size() + 1)] = {size, time};
            }
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
}

bool SameFileContent(const std::wstring& a, const std::wstring& b) {
    std::ifstream left(a, std::ios::binary), right(b, std::ios::binary);
    if (!left || !right) return false;
    std::vector<char> bufferA(1 << 16), bufferB(1 << 16);
    while (left && right) {
        left.read(bufferA.data(), static_cast<std::streamsize>(bufferA.size()));
        right.read(bufferB.data(), static_cast<std::streamsize>(bufferB.size()));
        if (left.gcount() != right.gcount() || memcmp(bufferA.data(), bufferB.data(), static_cast<size_t>(left.gcount())) != 0) return false;
    }
    return left.eof() && right.eof();
}

std::wstring FormatSize(uint64_t size) {
    wchar_t buffer[32]{};
    if (size < 1024) swprintf_s(buffer, L"%llu B", size);
    else if (size < 1024 * 1024) swprintf_s(buffer, L"%.1f KB", size / 1024.0);
    else swprintf_s(buffer, L"%.1f MB", size / 1048576.0);
    return buffer;
}

std::wstring FormatFileTime(uint64_t value) {
    if (value == 0) return {};
    FILETIME utc{static_cast<DWORD>(value), static_cast<DWORD>(value >> 32)}, local{};
    SYSTEMTIME time{};
    FileTimeToLocalFileTime(&utc, &local);
    FileTimeToSystemTime(&local, &time);
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
    return buffer;
}

void FillFolderList(FolderCompare& folder) {
    const bool onlyDiff = SendMessageW(folder.onlyDiff, BM_GETCHECK, 0, 0) == BST_CHECKED;
    SendMessageW(folder.list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(folder.list);
    folder.shown.clear();
    int counts[4]{};
    for (size_t i = 0; i < folder.entries.size(); ++i) {
        const FolderEntry& entry = folder.entries[i];
        ++counts[static_cast<int>(entry.state)];
        if (onlyDiff && entry.state == EntryState::Same) continue;
        folder.shown.push_back(static_cast<int>(i));
        const int row = static_cast<int>(folder.shown.size()) - 1;
        static const wchar_t* states[] = {L"相同", L"不同", L"仅左侧", L"仅右侧"};
        std::wstring cells[4] = {entry.relative, states[static_cast<int>(entry.state)],
            entry.state == EntryState::RightOnly ? L"" : FormatSize(entry.leftSize) + L"  " + FormatFileTime(entry.leftTime),
            entry.state == EntryState::LeftOnly ? L"" : FormatSize(entry.rightSize) + L"  " + FormatFileTime(entry.rightTime)};
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.pszText = cells[0].data();
        ListView_InsertItem(folder.list, &item);
        for (int c = 1; c < 4; ++c) ListView_SetItemText(folder.list, row, c, cells[c].data());
    }
    SendMessageW(folder.list, WM_SETREDRAW, TRUE, 0);
    const std::wstring summary = L"相同 " + std::to_wstring(counts[0]) + L" | 不同 " + std::to_wstring(counts[1]) +
        L" | 仅左侧 " + std::to_wstring(counts[2]) + L" | 仅右侧 " + std::to_wstring(counts[3]) + L"   （已跳过 .git、node_modules）";
    SetWindowTextW(folder.summary, summary.c_str());
}

void StartFolderCompare(FolderCompare& folder) {
    if (folder.running) return;
    folder.running = true;
    SetWindowTextW(folder.summary, L"正在比较…");
    ListView_DeleteAllItems(folder.list);
    const std::wstring left = folder.roots[0], right = folder.roots[1];
    HWND window = folder.window;
    HANDLE thread = StartSearchThread([left, right, window] {
        std::unordered_map<std::wstring, std::pair<uint64_t, uint64_t>> a, b;
        ListFolderFiles(left, a);
        ListFolderFiles(right, b);
        auto* entries = new std::vector<FolderEntry>();
        for (const auto& [relative, info] : a) {
            FolderEntry entry{relative, EntryState::LeftOnly, info.first, 0, info.second, 0};
            const auto other = b.find(relative);
            if (other != b.end()) {
                entry.rightSize = other->second.first;
                entry.rightTime = other->second.second;
                entry.state = entry.leftSize == entry.rightSize && SameFileContent(left + L"\\" + relative, right + L"\\" + relative)
                    ? EntryState::Same : EntryState::Different;
            }
            entries->push_back(entry);
        }
        for (const auto& [relative, info] : b) {
            if (!a.count(relative)) entries->push_back({relative, EntryState::RightOnly, 0, info.first, 0, info.second});
        }
        std::sort(entries->begin(), entries->end(), [](const FolderEntry& x, const FolderEntry& y) { return _wcsicmp(x.relative.c_str(), y.relative.c_str()) < 0; });
        if (!PostMessageW(window, kMessageFolderDone, 0, reinterpret_cast<LPARAM>(entries))) delete entries;
    });
    if (thread) CloseHandle(thread);
    else folder.running = false;
}

std::vector<int> CheckedOrSelectedEntries(FolderCompare& folder) {
    std::vector<int> result;
    for (int row = 0; row < ListView_GetItemCount(folder.list); ++row) {
        if (ListView_GetCheckState(folder.list, row)) result.push_back(folder.shown[static_cast<size_t>(row)]);
    }
    if (result.empty()) {
        for (int row = -1; (row = ListView_GetNextItem(folder.list, row, LVNI_SELECTED)) >= 0;) result.push_back(folder.shown[static_cast<size_t>(row)]);
    }
    return result;
}

void CopyFolderEntries(FolderCompare& folder, bool toRight) {
    std::vector<int> picked;
    for (const int index : CheckedOrSelectedEntries(folder)) {
        const EntryState state = folder.entries[static_cast<size_t>(index)].state;
        if (state == EntryState::Same || state == (toRight ? EntryState::RightOnly : EntryState::LeftOnly)) continue;
        picked.push_back(index);
    }
    if (picked.empty()) {
        MessageBoxW(folder.window, L"请先勾选或选中需要复制的文件（“不同”或只存在于来源一侧的文件）。", kWindowTitle, MB_ICONINFORMATION);
        return;
    }
    const std::wstring prompt = L"将把 " + std::to_wstring(picked.size()) + L" 个文件从" + (toRight ? L"左侧复制到右侧" : L"右侧复制到左侧") +
        L"，目标中的同名文件会被覆盖。确定继续吗？";
    if (MessageBoxW(folder.window, prompt.c_str(), kWindowTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    int copied = 0;
    std::wstring failures;
    for (const int index : picked) {
        const std::wstring& relative = folder.entries[static_cast<size_t>(index)].relative;
        const fs::path from = fs::path(folder.roots[toRight ? 0 : 1]) / relative;
        const fs::path to = fs::path(folder.roots[toRight ? 1 : 0]) / relative;
        std::error_code error;
        fs::create_directories(to.parent_path(), error);
        if (CopyFileW(from.c_str(), to.c_str(), FALSE)) ++copied;
        else failures += L"\n• " + relative;
    }
    std::wstring message = L"已复制 " + std::to_wstring(copied) + L" 个文件。";
    if (!failures.empty()) message += L"\n\n复制失败：" + failures;
    MessageBoxW(folder.window, message.c_str(), kWindowTitle, failures.empty() ? MB_ICONINFORMATION : MB_ICONWARNING);
    StartFolderCompare(folder);
}

void OpenFolderEntry(FolderCompare& folder) {
    const int row = ListView_GetNextItem(folder.list, -1, LVNI_SELECTED);
    if (row < 0) return;
    const FolderEntry& entry = folder.entries[static_cast<size_t>(folder.shown[static_cast<size_t>(row)])];
    const fs::path left = fs::path(folder.roots[0]) / entry.relative;
    const fs::path right = fs::path(folder.roots[1]) / entry.relative;
    if (entry.state == EntryState::LeftOnly) OpenFileInNewTab(left);
    else if (entry.state == EntryState::RightOnly) OpenFileInNewTab(right);
    else CompareFiles(left.wstring(), right.wstring());
}

LRESULT CALLBACK FolderCompareProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* folder = reinterpret_cast<FolderCompare*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        folder = static_cast<FolderCompare*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(folder));
        folder->window = window;
        const std::pair<int, const wchar_t*> buttons[] = {{kFolderOpen, L"对比所选文件"}, {kFolderToRight, L"复制到右侧 →"},
            {kFolderToLeft, L"← 复制到左侧"}, {kFolderRefresh, L"刷新"}};
        for (const auto& [id, text] : buttons) {
            HWND button = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app.instance, nullptr);
            SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            if (g_app.darkTheme) SetWindowTheme(button, L"DarkMode_Explorer", nullptr);
        }
        folder->onlyDiff = CreateWindowExW(0, L"BUTTON", L"只显示不同的文件", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 0, 0, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFolderOnlyDiff)), g_app.instance, nullptr);
        SendMessageW(folder->onlyDiff, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        SendMessageW(folder->onlyDiff, BM_SETCHECK, BST_CHECKED, 0);
        ThemeCheckbox(folder->onlyDiff);
        folder->summary = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_ENDELLIPSIS, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
        SendMessageW(folder->summary, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        folder->list = CreateWindowExW(0, WC_LISTVIEWW, nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFolderList)), g_app.instance, nullptr);
        ListView_SetExtendedListViewStyle(folder->list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        SendMessageW(folder->list, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        const ThemePalette& palette = CurrentPalette();
        SetWindowTheme(folder->list, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        if (HWND header = ListView_GetHeader(folder->list)) SetWindowTheme(header, g_app.darkTheme ? L"DarkMode_ItemsView" : nullptr, nullptr);
        ListView_SetBkColor(folder->list, palette.editorBackground);
        ListView_SetTextBkColor(folder->list, palette.editorBackground);
        ListView_SetTextColor(folder->list, palette.editorText);
        const std::pair<const wchar_t*, int> columns[] = {{L"文件", 420}, {L"状态", 80}, {L"左侧", 200}, {L"右侧", 200}};
        for (int c = 0; c < 4; ++c) {
            LVCOLUMNW column{};
            column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.pszText = const_cast<wchar_t*>(columns[c].first);
            column.cx = Scale(columns[c].second);
            ListView_InsertColumn(folder->list, c, &column);
        }
        return 0;
    }
    case WM_SIZE: {
        if (!folder || !folder->list) break;
        const int width = LOWORD(lParam), height = HIWORD(lParam);
        int x = Scale(10);
        for (const auto& [id, w] : std::initializer_list<std::pair<int, int>>{{kFolderOpen, 110}, {kFolderToRight, 110}, {kFolderToLeft, 110}, {kFolderRefresh, 70}}) {
            MoveWindow(GetDlgItem(window, id), x, Scale(8), Scale(w), Scale(28), TRUE);
            x += Scale(w) + Scale(6);
        }
        MoveWindow(folder->onlyDiff, x + Scale(6), Scale(10), Scale(150), Scale(24), TRUE);
        MoveWindow(folder->summary, Scale(10), Scale(44), width - Scale(20), Scale(20), TRUE);
        MoveWindow(folder->list, Scale(10), Scale(68), width - Scale(20), std::max(0, height - Scale(78)), TRUE);
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(window, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, g_app.toolbarBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, CurrentPalette().toolbarText);
        SetBkColor(dc, CurrentPalette().toolbarBackground);
        return reinterpret_cast<LRESULT>(g_app.toolbarBrush);
    }
    case kMessageFolderDone: {
        std::unique_ptr<std::vector<FolderEntry>> entries(reinterpret_cast<std::vector<FolderEntry>*>(lParam));
        if (!folder) return 0;
        folder->running = false;
        folder->entries = std::move(*entries);
        FillFolderList(*folder);
        return 0;
    }
    case WM_NOTIFY: {
        auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (folder && header->hwndFrom == folder->list) {
            if (header->code == NM_DBLCLK) { OpenFolderEntry(*folder); return 0; }
            if (header->code == NM_CUSTOMDRAW) {
                auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(lParam);
                if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
                if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    const size_t row = static_cast<size_t>(draw->nmcd.dwItemSpec);
                    if (row < folder->shown.size()) {
                        const EntryState state = folder->entries[static_cast<size_t>(folder->shown[row])].state;
                        const bool dark = g_app.darkTheme;
                        draw->clrText = state == EntryState::Different ? (dark ? RGB(229, 192, 123) : RGB(176, 110, 0))
                            : state == EntryState::LeftOnly ? (dark ? RGB(244, 135, 113) : RGB(190, 40, 40))
                            : state == EntryState::RightOnly ? (dark ? RGB(129, 199, 132) : RGB(22, 130, 52))
                            : CurrentPalette().gutterText;
                    }
                    return CDRF_DODEFAULT;
                }
            }
        }
        break;
    }
    case WM_COMMAND:
        if (!folder) break;
        switch (LOWORD(wParam)) {
        case kFolderOnlyDiff: FillFolderList(*folder); return 0;
        case kFolderOpen: case IDOK: OpenFolderEntry(*folder); return 0;
        case kFolderToRight: CopyFolderEntries(*folder, true); return 0;
        case kFolderToLeft: CopyFolderEntries(*folder, false); return 0;
        case kFolderRefresh: StartFolderCompare(*folder); return 0;
        case IDCANCEL: DestroyWindow(window); return 0;
        default: break;
        }
        break;
    case WM_NCDESTROY:
        delete folder;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        break;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void CompareFoldersCommand() {
    const std::wstring left = PickFolder(L"选择左侧文件夹（原始）");
    if (left.empty()) return;
    const std::wstring right = PickFolder(L"选择右侧文件夹（新版本）");
    if (right.empty()) return;
    CompareFolderPaths(left, right);
}

void CompareFolderPaths(const std::wstring& left, const std::wstring& right) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{};
        klass.hInstance = g_app.instance;
        klass.lpfnWndProc = FolderCompareProc;
        klass.lpszClassName = L"MCnotepadFolderCompare";
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        klass.hIcon = LoadIconW(g_app.instance, MAKEINTRESOURCEW(IDI_MCNOTEPAD));
        registered = RegisterClassW(&klass) != 0;
    }
    auto* folder = new FolderCompare();
    folder->roots[0] = left;
    folder->roots[1] = right;
    RECT owner{};
    GetWindowRect(g_app.window, &owner);
    const std::wstring title = L"比较文件夹 - " + left + L" ↔ " + right;
    HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, L"MCnotepadFolderCompare", title.c_str(), WS_OVERLAPPEDWINDOW,
        owner.left + Scale(40), owner.top + Scale(40), Scale(1000), Scale(620), nullptr, nullptr, g_app.instance, folder);
    if (!window) {
        delete folder;
        return;
    }
    ApplyWindowFrameTheme(window);
    g_toolWindows.push_back(window);
    ShowWindow(window, SW_SHOW);
    StartFolderCompare(*folder);
}

// ===========================================================================
// Large log viewer: the file is memory-mapped and indexed, never loaded
// whole into memory. Filters run on the raw bytes on a worker thread.
// ===========================================================================

constexpr uint64_t kLogCheckpointEvery = 64;  // one saved line offset per 64 lines
constexpr UINT kMessageLogIndexed = WM_APP + 40;
constexpr UINT kMessageLogFiltered = WM_APP + 41;
constexpr UINT kMessageLogProgress = WM_APP + 42;
constexpr UINT_PTR kTimerLogTail = 1;

enum LogLevel : int { kLevelNone = 0, kLevelError, kLevelWarn, kLevelInfo, kLevelDebug };

// Line k starts at checkpoints[k / 64] plus (k % 64) more line breaks.
struct LogIndex {
    std::vector<uint64_t> checkpoints{0};
    uint64_t complete = 0;    // lines ending in '\n'
    uint64_t indexedEnd = 0;  // offset just after the last '\n'
    uint64_t Lines(uint64_t size) const { return complete + (size > indexedEnd ? 1 : 0); }
};

// Indexes the bytes after index.indexedEnd; call again when the file grows.
bool IndexLogBytes(const char* data, uint64_t size, LogIndex& index, const std::atomic<bool>* cancel = nullptr,
    HWND progress = nullptr, int generation = 0) {
    uint64_t position = index.indexedEnd;
    uint64_t nextReport = position + (64ull << 20);
    while (position < size) {
        const void* hit = memchr(data + position, '\n', static_cast<size_t>(std::min<uint64_t>(size - position, 1ull << 30)));
        if (!hit) {
            if (size - position > (1ull << 30)) { position += 1ull << 30; continue; }
            break;
        }
        position = static_cast<uint64_t>(static_cast<const char*>(hit) - data) + 1;
        ++index.complete;
        index.indexedEnd = position;
        if (index.complete % kLogCheckpointEvery == 0) index.checkpoints.push_back(position);
        if (position >= nextReport) {
            nextReport = position + (64ull << 20);
            if (cancel && *cancel) return false;
            if (progress) PostMessageW(progress, kMessageLogProgress, generation, static_cast<LPARAM>(position * 1000 / size));
        }
    }
    return true;
}

std::pair<uint64_t, uint64_t> LogLineRange(const char* data, uint64_t size, const LogIndex& index, uint64_t line) {
    uint64_t start = index.checkpoints[static_cast<size_t>(line / kLogCheckpointEvery)];
    for (uint64_t skip = line % kLogCheckpointEvery; skip > 0 && start < size; --skip) {
        const void* hit = memchr(data + start, '\n', static_cast<size_t>(size - start));
        start = hit ? static_cast<uint64_t>(static_cast<const char*>(hit) - data) + 1 : size;
    }
    const void* hit = start < size ? memchr(data + start, '\n', static_cast<size_t>(size - start)) : nullptr;
    uint64_t end = hit ? static_cast<uint64_t>(static_cast<const char*>(hit) - data) : size;
    if (end > start && data[end - 1] == '\r') --end;
    return {start, end};
}

// ---- Line classification on raw bytes ---------------------------------------------------

inline char AsciiLower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }
inline bool AsciiWordChar(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

// Case-insensitive (ASCII) search; other bytes must match exactly.
bool ContainsNoCase(const char* text, size_t length, const std::string& lowerNeedle) {
    const size_t n = lowerNeedle.size();
    if (n == 0) return true;
    if (n > length) return false;
    const char first = lowerNeedle[0];
    for (size_t i = 0; i + n <= length; ++i) {
        if (AsciiLower(text[i]) != first) continue;
        size_t k = 1;
        while (k < n && AsciiLower(text[i + k]) == lowerNeedle[k]) ++k;
        if (k == n) return true;
    }
    return false;
}

// Level from the first 160 bytes: ERROR / WARN / INFO / DEBUG and friends, as whole words.
int DetectLogLevel(const char* text, size_t length) {
    length = std::min<size_t>(length, 160);
    static const std::pair<const char*, int> words[] = {
        {"fatal", kLevelError}, {"critical", kLevelError}, {"crit", kLevelError}, {"error", kLevelError}, {"err", kLevelError}, {"severe", kLevelError},
        {"warning", kLevelWarn}, {"warn", kLevelWarn}, {"info", kLevelInfo}, {"notice", kLevelInfo},
        {"debug", kLevelDebug}, {"trace", kLevelDebug}, {"verbose", kLevelDebug}, {"fine", kLevelDebug}};
    for (size_t i = 0; i < length; ++i) {
        if (!AsciiWordChar(text[i]) || (i > 0 && AsciiWordChar(text[i - 1]))) continue;
        size_t end = i;
        while (end < length && AsciiWordChar(text[end])) ++end;
        const size_t wordLength = end - i;
        if (wordLength >= 3 && wordLength <= 8) {
            char word[9]{};
            for (size_t k = 0; k < wordLength; ++k) word[k] = AsciiLower(text[i + k]);
            for (const auto& [name, level] : words) {
                if (strcmp(word, name) == 0) return level;
            }
        }
        i = end;
    }
    return kLevelNone;
}

// First "YYYY-MM-DD HH:MM[:SS]" (also with 'T' or '/'), normalized to
// "YYYY-MM-DD HH:MM:SS"; empty when the line has none.
std::string DetectLogTime(const char* text, size_t length) {
    length = std::min<size_t>(length, 120);
    const auto digit = [&](size_t i) { return i < length && text[i] >= '0' && text[i] <= '9'; };
    for (size_t i = 0; i + 16 <= length; ++i) {
        if (!(digit(i) && digit(i + 1) && digit(i + 2) && digit(i + 3))) continue;
        if ((text[i + 4] != '-' && text[i + 4] != '/') || !digit(i + 5) || !digit(i + 6) || text[i + 7] != text[i + 4] || !digit(i + 8) || !digit(i + 9)) continue;
        if ((text[i + 10] != ' ' && text[i + 10] != 'T') || !digit(i + 11) || !digit(i + 12) || text[i + 13] != ':' || !digit(i + 14) || !digit(i + 15)) continue;
        std::string result(text + i, 16);
        result[4] = result[7] = '-';
        result[10] = ' ';
        if (i + 18 < length + 1 && i + 16 < length && text[i + 16] == ':' && digit(i + 17) && digit(i + 18)) result += std::string(text + i + 16, 3);
        else result += ":00";
        return result;
    }
    return {};
}

// "2026-11-23 10:5" style input to a comparable prefix; empty when blank.
std::string NormalizeTimeInput(const std::wstring& input, bool upper) {
    std::string text = WideToUtf8(TrimCopy(input));
    if (text.empty()) return {};
    for (char& c : text) if (c == '/' || c == '.') c = '-'; else if (c == 'T') c = ' ';
    // A bare time means today.
    if (text.size() <= 8 && text.find(':') != std::string::npos) {
        SYSTEMTIME now{};
        GetLocalTime(&now);
        char date[16]{};
        sprintf_s(date, "%04u-%02u-%02u ", now.wYear, now.wMonth, now.wDay);
        text = date + text;
    }
    const std::string full = upper ? "9999-12-31 23:59:59" : "0000-01-01 00:00:00";
    if (text.size() < full.size()) text += full.substr(text.size());
    return text.substr(0, full.size());
}

struct LogFilter {
    std::vector<std::string> keywords;  // lower-case UTF-8 alternatives
    bool levels[5] = {true, true, true, true, true};
    std::string timeFrom, timeTo;
    bool Active() const {
        return !keywords.empty() || !timeFrom.empty() || !timeTo.empty() || !(levels[0] && levels[1] && levels[2] && levels[3] && levels[4]);
    }
};

// Continuation lines (stack traces etc.) take the level and time of the
// line they belong to.
struct LogLineState {
    int level = kLevelNone;
    std::string time;
};

bool LogLineMatches(const char* text, size_t length, const LogFilter& filter, LogLineState& state) {
    const int level = DetectLogLevel(text, length);
    std::string time = DetectLogTime(text, length);
    const bool continuation = level == kLevelNone && time.empty();
    if (!continuation) {
        state.level = level;
        if (!time.empty()) state.time = std::move(time);
    }
    if (!filter.levels[state.level]) return false;
    if (!filter.timeFrom.empty() || !filter.timeTo.empty()) {
        if (state.time.empty()) return false;
        if (!filter.timeFrom.empty() && state.time < filter.timeFrom) return false;
        if (!filter.timeTo.empty() && state.time > filter.timeTo) return false;
    }
    if (!filter.keywords.empty()) {
        bool any = false;
        for (const std::string& keyword : filter.keywords) {
            if (ContainsNoCase(text, length, keyword)) { any = true; break; }
        }
        if (!any) return false;
    }
    return true;
}

// ---- Viewer state -----------------------------------------------------------------------

struct LogViewer {
    HWND window = nullptr, list = nullptr, keyword = nullptr, highlight = nullptr, from = nullptr, to = nullptr, status = nullptr, tail = nullptr;
    HWND levelBoxes[5]{};
    std::wstring path;
    HANDLE file = INVALID_HANDLE_VALUE, mapping = nullptr;
    const char* data = nullptr;
    uint64_t size = 0;
    UINT codePage = CP_UTF8;
    LogIndex index;
    bool indexed = false;
    bool filtered = false;
    std::vector<uint32_t> view;          // matching line numbers while filtered
    LogFilter filter;
    LogLineState tailState;              // classification state at the end of the file
    std::vector<std::wstring> highlights; // lower-case words to mark in the text
    std::atomic<bool> busy{false};
    std::atomic<bool> cancel{false};
    int generation = 0;
    DWORD started = 0;
    int lineHeight = 16;
    int numberWidth = 60;
};

bool MapLogFile(LogViewer& log) {
    if (log.data) UnmapViewOfFile(log.data);
    if (log.mapping) CloseHandle(log.mapping);
    log.data = nullptr;
    log.mapping = nullptr;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(log.file, &size)) return false;
    log.size = static_cast<uint64_t>(size.QuadPart);
    if (log.size == 0) return true;
    log.mapping = CreateFileMappingW(log.file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!log.mapping) return false;
    log.data = static_cast<const char*>(MapViewOfFile(log.mapping, FILE_MAP_READ, 0, 0, 0));
    return log.data != nullptr;
}

void CloseLogFile(LogViewer& log) {
    if (log.data) UnmapViewOfFile(log.data);
    if (log.mapping) CloseHandle(log.mapping);
    if (log.file != INVALID_HANDLE_VALUE) CloseHandle(log.file);
    log.data = nullptr;
    log.mapping = nullptr;
    log.file = INVALID_HANDLE_VALUE;
}

uint64_t LogRowCount(const LogViewer& log) {
    return log.filtered ? log.view.size() : log.index.Lines(log.size);
}

uint64_t LogLineOfRow(const LogViewer& log, uint64_t row) {
    return log.filtered ? log.view[static_cast<size_t>(row)] : row;
}

std::wstring LogLineText(const LogViewer& log, uint64_t line, size_t maxBytes = 8192) {
    if (!log.data) return {};
    auto [start, end] = LogLineRange(log.data, log.size, log.index, line);
    const size_t length = static_cast<size_t>(std::min<uint64_t>(end - start, maxBytes));
    if (length == 0) return {};
    const int chars = MultiByteToWideChar(log.codePage, 0, log.data + start, static_cast<int>(length), nullptr, 0);
    std::wstring text(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(log.codePage, 0, log.data + start, static_cast<int>(length), text.data(), chars);
    if (end - start > maxBytes) text += L" …";
    for (wchar_t& c : text) if (c == L'\t') c = L' ';
    return text;
}

void UpdateLogStatus(LogViewer& log, const std::wstring& extra = {}) {
    std::wstring text = L"共 " + std::to_wstring(log.index.Lines(log.size)) + L" 行";
    if (log.filtered) text += L" · 筛选出 " + std::to_wstring(log.view.size()) + L" 行";
    text += L" · " + FormatSize(log.size) + (log.codePage == CP_UTF8 ? L" · UTF-8" : L" · ANSI");
    if (!extra.empty()) text += L" · " + extra;
    SetWindowTextW(log.status, text.c_str());
}

void SetLogRowCount(LogViewer& log, bool keepPosition) {
    const uint64_t rows = LogRowCount(log);
    ListView_SetItemCountEx(log.list, static_cast<int>(std::min<uint64_t>(rows, INT_MAX)), keepPosition ? LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL : 0);
    InvalidateRect(log.list, nullptr, FALSE);
    // Line-number column fits the largest number.
    HDC dc = GetDC(log.list);
    HGDIOBJ previous = SelectObject(dc, g_app.editorFont);
    SIZE extent{};
    const std::wstring sample(std::to_wstring(std::max<uint64_t>(log.index.Lines(log.size), 1)).size(), L'9');
    GetTextExtentPoint32W(dc, sample.c_str(), static_cast<int>(sample.size()), &extent);
    SelectObject(dc, previous);
    ReleaseDC(log.list, dc);
    log.numberWidth = extent.cx + Scale(18);
    ListView_SetColumnWidth(log.list, 0, log.numberWidth);
}

// ---- Indexing and filtering on a worker thread ----------------------------------------------

struct LogIndexResult { int generation; LogIndex index; bool ok; };
struct LogFilterResult { int generation; std::vector<uint32_t> view; LogLineState endState; bool ok; };

void StartLogIndex(LogViewer& log) {
    log.cancel = true;
    while (log.busy) Sleep(5);
    log.cancel = false;
    log.busy = true;
    log.indexed = false;
    const int generation = ++log.generation;
    log.started = GetTickCount();
    const char* data = log.data;
    const uint64_t size = log.size;
    HWND window = log.window;
    std::atomic<bool>* cancel = &log.cancel;
    std::atomic<bool>* busy = &log.busy;
    HANDLE thread = StartSearchThread([=] {
        auto* result = new LogIndexResult{generation, {}, true};
        result->ok = IndexLogBytes(data, size, result->index, cancel, window, generation);
        busy->store(false);
        if (!PostMessageW(window, kMessageLogIndexed, 0, reinterpret_cast<LPARAM>(result))) delete result;
    });
    if (thread) CloseHandle(thread);
    else log.busy = false;
    SetWindowTextW(log.status, L"正在建立行索引…");
}

void StartLogFilter(LogViewer& log) {
    if (!log.indexed) return;
    log.cancel = true;
    while (log.busy) Sleep(5);
    log.cancel = false;
    if (!log.filter.Active()) {
        log.filtered = false;
        log.view.clear();
        log.view.shrink_to_fit();
        SetLogRowCount(log, false);
        UpdateLogStatus(log);
        return;
    }
    log.busy = true;
    const int generation = ++log.generation;
    log.started = GetTickCount();
    const char* data = log.data;
    const uint64_t size = log.size;
    const LogIndex* index = &log.index;
    const LogFilter filter = log.filter;
    HWND window = log.window;
    std::atomic<bool>* cancel = &log.cancel;
    std::atomic<bool>* busy = &log.busy;
    HANDLE thread = StartSearchThread([=] {
        auto* result = new LogFilterResult{generation, {}, {}, true};
        const uint64_t lines = index->Lines(size);
        uint64_t start = 0;
        for (uint64_t line = 0; line < lines; ++line) {
            const void* hit = start < size ? memchr(data + start, '\n', static_cast<size_t>(size - start)) : nullptr;
            const uint64_t next = hit ? static_cast<uint64_t>(static_cast<const char*>(hit) - data) + 1 : size;
            uint64_t end = hit ? next - 1 : size;
            if (end > start && data[end - 1] == '\r') --end;
            if (LogLineMatches(data + start, static_cast<size_t>(end - start), filter, result->endState)) result->view.push_back(static_cast<uint32_t>(line));
            start = next;
            if ((line & 0x3FFFF) == 0 && line) {
                if (*cancel) { result->ok = false; break; }
                PostMessageW(window, kMessageLogProgress, generation, static_cast<LPARAM>(start * 1000 / std::max<uint64_t>(size, 1)));
            }
        }
        busy->store(false);
        if (!PostMessageW(window, kMessageLogFiltered, 0, reinterpret_cast<LPARAM>(result))) delete result;
    });
    if (thread) CloseHandle(thread);
    else log.busy = false;
    SetWindowTextW(log.status, L"正在筛选…");
}

std::vector<std::string> SplitKeywords(const std::wstring& text) {
    std::vector<std::string> words;
    std::wstring current;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == L'|' || text[i] == L';') {
            const std::wstring word = TrimCopy(current);
            if (!word.empty()) {
                std::string utf8 = WideToUtf8(word);
                for (char& c : utf8) c = AsciiLower(c);
                words.push_back(utf8);
            }
            current.clear();
        } else {
            current += text[i];
        }
    }
    return words;
}

void ApplyLogFilterFromControls(LogViewer& log) {
    log.filter.keywords = SplitKeywords(GetWindowTextValue(log.keyword));
    for (int l = 0; l < 5; ++l) log.filter.levels[l] = SendMessageW(log.levelBoxes[l], BM_GETCHECK, 0, 0) == BST_CHECKED;
    log.filter.timeFrom = NormalizeTimeInput(GetWindowTextValue(log.from), false);
    log.filter.timeTo = NormalizeTimeInput(GetWindowTextValue(log.to), true);
    log.highlights.clear();
    for (const std::wstring& source : {GetWindowTextValue(log.highlight), GetWindowTextValue(log.keyword)}) {
        std::wstring current;
        for (size_t i = 0; i <= source.size(); ++i) {
            if (i == source.size() || source[i] == L'|' || source[i] == L';') {
                std::wstring word = TrimCopy(current);
                if (!word.empty()) {
                    for (wchar_t& c : word) c = towlower(c);
                    log.highlights.push_back(word);
                }
                current.clear();
            } else {
                current += source[i];
            }
        }
    }
    StartLogFilter(log);
}

// ---- Live tail ---------------------------------------------------------------------------

void PollLogTail(LogViewer& log) {
    if (log.busy || !log.indexed) return;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(log.file, &size)) return;
    const uint64_t newSize = static_cast<uint64_t>(size.QuadPart);
    if (newSize == log.size) return;
    if (newSize < log.size) {  // truncated or rotated: start over
        MapLogFile(log);
        log.index = LogIndex{};
        StartLogIndex(log);
        return;
    }
    const uint64_t oldLines = log.index.Lines(log.size);
    const uint64_t firstChanged = oldLines > 0 && log.size > log.index.indexedEnd ? oldLines - 1 : oldLines;  // a partial line may have grown
    if (!MapLogFile(log)) return;
    IndexLogBytes(log.data, log.size, log.index);
    if (log.filtered) {
        if (!log.view.empty() && log.view.back() >= firstChanged) log.view.pop_back();
        const uint64_t lines = log.index.Lines(log.size);
        for (uint64_t line = firstChanged; line < lines; ++line) {
            auto [start, end] = LogLineRange(log.data, log.size, log.index, line);
            if (LogLineMatches(log.data + start, static_cast<size_t>(end - start), log.filter, log.tailState)) log.view.push_back(static_cast<uint32_t>(line));
        }
    }
    SetLogRowCount(log, true);
    const int rows = ListView_GetItemCount(log.list);
    if (rows > 0) ListView_EnsureVisible(log.list, rows - 1, FALSE);
    UpdateLogStatus(log, L"实时跟踪中");
}

// ---- Drawing ----------------------------------------------------------------------------

COLORREF LogLevelColor(int level) {
    const bool dark = g_app.darkTheme;
    switch (level) {
    case kLevelError: return dark ? RGB(244, 112, 103) : RGB(200, 30, 30);
    case kLevelWarn: return dark ? RGB(229, 192, 123) : RGB(170, 105, 0);
    case kLevelDebug: return CurrentPalette().gutterText;
    default: return CurrentPalette().editorText;
    }
}

void DrawLogRow(LogViewer& log, const DRAWITEMSTRUCT& item) {
    HDC dc = item.hDC;
    const ThemePalette& palette = CurrentPalette();
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    const COLORREF background = selected ? Blend(palette.editorBackground, palette.accent, g_app.darkTheme ? 30 : 22) : palette.editorBackground;
    HBRUSH brush = CreateSolidBrush(background);
    FillRect(dc, &item.rcItem, brush);
    DeleteObject(brush);
    const uint64_t row = item.itemID;
    if (row >= LogRowCount(log) || !log.data) return;
    const uint64_t line = LogLineOfRow(log, row);
    HGDIOBJ previous = SelectObject(dc, g_app.editorFont);
    SetBkMode(dc, TRANSPARENT);

    RECT number = item.rcItem;
    number.right = number.left + log.numberWidth - Scale(8);
    SetTextColor(dc, palette.gutterText);
    const std::wstring lineNumber = std::to_wstring(line + 1);
    DrawTextW(dc, lineNumber.c_str(), static_cast<int>(lineNumber.size()), &number, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    auto [start, end] = LogLineRange(log.data, log.size, log.index, line);
    int level = DetectLogLevel(log.data + start, static_cast<size_t>(end - start));
    // Continuation lines (stack traces) take the colour of the line they belong to.
    if (level == kLevelNone && DetectLogTime(log.data + start, static_cast<size_t>(end - start)).empty()) {
        for (uint64_t back = 1; back <= 50 && back <= line; ++back) {
            auto [s0, e0] = LogLineRange(log.data, log.size, log.index, line - back);
            const int found = DetectLogLevel(log.data + s0, static_cast<size_t>(e0 - s0));
            if (found != kLevelNone || !DetectLogTime(log.data + s0, static_cast<size_t>(e0 - s0)).empty()) {
                level = found;
                break;
            }
        }
    }
    const std::wstring text = LogLineText(log, line);
    int x = item.rcItem.left + log.numberWidth;
    const int y = item.rcItem.top + (item.rcItem.bottom - item.rcItem.top - log.lineHeight) / 2 + Scale(1);

    // Highlight ranges (case-insensitive).
    std::vector<std::pair<size_t, size_t>> marks;
    if (!log.highlights.empty()) {
        std::wstring lower = text;
        for (wchar_t& c : lower) c = towlower(c);
        for (const std::wstring& word : log.highlights) {
            for (size_t at = lower.find(word); at != std::wstring::npos; at = lower.find(word, at + word.size())) marks.emplace_back(at, at + word.size());
        }
        std::sort(marks.begin(), marks.end());
    }
    const COLORREF textColor = LogLevelColor(level);
    const COLORREF markBack = g_app.darkTheme ? RGB(110, 60, 40) : RGB(255, 214, 170);
    size_t position = 0;
    const auto drawRun = [&](size_t from, size_t to, bool marked) {
        if (to <= from) return;
        SIZE extent{};
        GetTextExtentPoint32W(dc, text.c_str() + from, static_cast<int>(to - from), &extent);
        if (marked) {
            RECT back{x, item.rcItem.top + Scale(1), x + extent.cx, item.rcItem.bottom - Scale(1)};
            HBRUSH markBrush = CreateSolidBrush(markBack);
            FillRect(dc, &back, markBrush);
            DeleteObject(markBrush);
        }
        SetTextColor(dc, marked ? (g_app.darkTheme ? RGB(255, 236, 200) : RGB(120, 30, 0)) : textColor);
        ExtTextOutW(dc, x, y, 0, nullptr, text.c_str() + from, static_cast<UINT>(to - from), nullptr);
        x += extent.cx;
    };
    for (const auto& [from, to] : marks) {
        if (from < position) continue;
        drawRun(position, from, false);
        drawRun(from, to, true);
        position = to;
        if (x > item.rcItem.right) break;
    }
    if (x <= item.rcItem.right) drawRun(position, text.size(), false);
    SelectObject(dc, previous);
}

// ---- Commands ---------------------------------------------------------------------------

std::wstring LogSelectedText(LogViewer& log, size_t maxLines) {
    std::wstring text;
    size_t count = 0;
    for (int row = -1; (row = ListView_GetNextItem(log.list, row, LVNI_SELECTED)) >= 0 && count < maxLines; ++count) {
        text += LogLineText(log, LogLineOfRow(log, static_cast<uint64_t>(row)), 1 << 20) + L"\r\n";
    }
    return text;
}

void CopyLogSelection(LogViewer& log) {
    const std::wstring text = LogSelectedText(log, 200000);
    if (!text.empty()) CopyTextToClipboard(text);
}

// Filtered lines (or the selection) into a new editor tab.
void ExportLogToTab(LogViewer& log) {
    const bool selection = ListView_GetSelectedCount(log.list) > 1;
    std::wstring text;
    if (selection) {
        text = LogSelectedText(log, 200000);
    } else {
        const uint64_t rows = std::min<uint64_t>(LogRowCount(log), 200000);
        for (uint64_t row = 0; row < rows && text.size() < (64u << 20); ++row) text += LogLineText(log, LogLineOfRow(log, row), 1 << 20) + L"\r\n";
        if (LogRowCount(log) > rows) text += L"…（只导出了前 200000 行）\r\n";
    }
    const std::wstring title = fs::path(log.path).filename().wstring() + (selection ? L" - 选中行" : log.filtered ? L" - 筛选结果" : L" - 前 200000 行");
    const int index = CreateDocument({}, title, text, true, true, 0, 0, 0);
    if (index >= 0) {
        SelectTab(index);
        SetForegroundWindow(g_app.window);
    }
}

enum LogControl : int {
    kLogKeyword = 4201, kLogHighlight, kLogFrom, kLogTo, kLogApply, kLogClear, kLogTail, kLogExport, kLogEnd, kLogList,
    kLogLevelFirst = 4220,  // ..4224
};

void LayoutLogViewer(LogViewer& log) {
    RECT client{};
    GetClientRect(log.window, &client);
    const int width = client.right, height = client.bottom;
    const int rowH = Scale(26), gap = Scale(6);
    int y = Scale(8);
    int x = Scale(10);
    const auto place = [&](HWND control, int w, int h = 0) {
        MoveWindow(control, x, y + (h ? (rowH - h) / 2 : 0), w, h ? h : rowH, TRUE);
        x += w + gap;
    };
    place(GetDlgItem(log.window, 4301), Scale(40), Scale(20));
    place(log.keyword, Scale(220));
    place(GetDlgItem(log.window, 4302), Scale(40), Scale(20));
    place(log.highlight, Scale(220));
    place(GetDlgItem(log.window, kLogApply), Scale(64));
    place(GetDlgItem(log.window, kLogClear), Scale(64));
    y += rowH + gap;
    x = Scale(10);
    place(GetDlgItem(log.window, 4303), Scale(40), Scale(20));
    place(log.from, Scale(150));
    place(GetDlgItem(log.window, 4304), Scale(16), Scale(20));
    place(log.to, Scale(150));
    x += gap;
    const std::pair<int, int> levelOrder[] = {{1, 64}, {2, 60}, {3, 56}, {4, 68}, {0, 56}};
    for (const auto& [l, w] : levelOrder) place(log.levelBoxes[l], Scale(w), Scale(22));
    x += gap;
    place(log.tail, Scale(84), Scale(22));
    place(GetDlgItem(log.window, kLogEnd), Scale(64));
    place(GetDlgItem(log.window, kLogExport), Scale(108));
    y += rowH + gap;
    MoveWindow(log.status, Scale(10), y, width - Scale(20), Scale(20), TRUE);
    y += Scale(24);
    MoveWindow(log.list, Scale(10), y, std::max(0, width - Scale(20)), std::max(0, height - y - Scale(10)), TRUE);
    RECT listClient{};
    GetClientRect(log.list, &listClient);
    ListView_SetColumnWidth(log.list, 1, std::max<int>(listClient.right - log.numberWidth, Scale(2400)));
}

LRESULT CALLBACK LogViewerProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* log = reinterpret_cast<LogViewer*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        log = static_cast<LogViewer*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(log));
        log->window = window;
        HDC dc = GetDC(window);
        HGDIOBJ previous = SelectObject(dc, g_app.editorFont);
        TEXTMETRICW metrics{};
        GetTextMetricsW(dc, &metrics);
        SelectObject(dc, previous);
        ReleaseDC(window, dc);
        log->lineHeight = metrics.tmHeight;
        const auto makeLabel = [&](int id, const wchar_t* text) {
            HWND label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_NOPREFIX, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app.instance, nullptr);
            SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        };
        const auto makeEdit = [&](int id, const wchar_t* text, const wchar_t* cue) {
            HWND edit = CreateWindowExW(0, L"EDIT", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app.instance, nullptr);
            SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            SendMessageW(edit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(cue));
            if (g_app.darkTheme) SetWindowTheme(edit, L"DarkMode_CFD", nullptr);
            return edit;
        };
        const auto makeButton = [&](int id, const wchar_t* text, DWORD style = 0) {
            HWND button = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style, 0, 0, 0, 0, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app.instance, nullptr);
            SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            if (style & BS_AUTOCHECKBOX) ThemeCheckbox(button);
            else if (g_app.darkTheme) SetWindowTheme(button, L"DarkMode_Explorer", nullptr);
            return button;
        };
        makeLabel(4301, L"筛选");
        log->keyword = makeEdit(kLogKeyword, L"", L"包含关键词，多个用 | 分隔");
        makeLabel(4302, L"高亮");
        log->highlight = makeEdit(kLogHighlight, L"error|exception|fail|fatal|timeout", L"高亮关键词，多个用 | 分隔");
        makeButton(kLogApply, L"应用");
        makeButton(kLogClear, L"清除");
        makeLabel(4303, L"时间");
        log->from = makeEdit(kLogFrom, L"", L"从 2026-11-23 10:00");
        makeLabel(4304, L"至");
        log->to = makeEdit(kLogTo, L"", L"到 2026-11-23 12:00");
        const wchar_t* levels[5] = {L"其他", L"ERROR", L"WARN", L"INFO", L"DEBUG"};
        // Shown in the order ERROR, WARN, INFO, DEBUG, 其他.
        for (const int l : {1, 2, 3, 4, 0}) {
            log->levelBoxes[l] = makeButton(kLogLevelFirst + l, levels[l], BS_AUTOCHECKBOX);
            SendMessageW(log->levelBoxes[l], BM_SETCHECK, BST_CHECKED, 0);
        }
        log->tail = makeButton(kLogTail, L"实时跟踪", BS_AUTOCHECKBOX);
        makeButton(kLogEnd, L"到末尾");
        makeButton(kLogExport, L"导出到新标签");
        log->status = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_ENDELLIPSIS, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
        SendMessageW(log->status, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        log->list = CreateWindowExW(0, WC_LISTVIEWW, nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_OWNERDATA | LVS_OWNERDRAWFIXED | LVS_NOCOLUMNHEADER | LVS_SHOWSELALWAYS,
            0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kLogList)), g_app.instance, nullptr);
        ListView_SetExtendedListViewStyle(log->list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        SendMessageW(log->list, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
        SetWindowTheme(log->list, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        ListView_SetBkColor(log->list, CurrentPalette().editorBackground);
        for (int c = 0; c < 2; ++c) {
            LVCOLUMNW column{};
            column.mask = LVCF_WIDTH;
            column.cx = Scale(c == 0 ? 60 : 2400);
            ListView_InsertColumn(log->list, c, &column);
        }
        return 0;
    }
    case WM_MEASUREITEM: {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (measure->CtlType == ODT_LISTVIEW) {
            measure->itemHeight = static_cast<UINT>((log ? log->lineHeight : 16) + Scale(3));
            return TRUE;
        }
        break;
    }
    case WM_DRAWITEM: {
        auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (log && draw->CtlType == ODT_LISTVIEW) {
            DrawLogRow(*log, *draw);
            return TRUE;
        }
        break;
    }
    case WM_SIZE:
        if (log && log->list) LayoutLogViewer(*log);
        return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = POINT{Scale(900), Scale(400)};
        return 0;
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(window, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, g_app.toolbarBrush);
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, reinterpret_cast<HWND>(lParam) == (log ? log->status : nullptr) ? CurrentPalette().statusText : CurrentPalette().toolbarText);
        SetBkColor(dc, CurrentPalette().toolbarBackground);
        return reinterpret_cast<LRESULT>(g_app.toolbarBrush);
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, CurrentPalette().editorText);
        SetBkColor(dc, CurrentPalette().controlBackground);
        static HBRUSH brush = nullptr;
        static COLORREF brushColor = 0;
        if (!brush || brushColor != CurrentPalette().controlBackground) {
            if (brush) DeleteObject(brush);
            brushColor = CurrentPalette().controlBackground;
            brush = CreateSolidBrush(brushColor);
        }
        return reinterpret_cast<LRESULT>(brush);
    }
    case kMessageLogProgress:
        if (log && static_cast<int>(wParam) == log->generation) {
            wchar_t text[64]{};
            swprintf_s(text, L"%s %.1f%%", log->indexed ? L"正在筛选…" : L"正在建立行索引…", lParam / 10.0);
            SetWindowTextW(log->status, text);
        }
        return 0;
    case kMessageLogIndexed: {
        std::unique_ptr<LogIndexResult> result(reinterpret_cast<LogIndexResult*>(lParam));
        if (!log || result->generation != log->generation || !result->ok) return 0;
        log->index = std::move(result->index);
        log->indexed = true;
        const DWORD elapsed = GetTickCount() - log->started;
        log->filtered = false;
        SetLogRowCount(*log, false);
        ApplyLogFilterFromControls(*log);
        if (!log->busy) UpdateLogStatus(*log, L"索引用时 " + std::to_wstring(elapsed) + L" ms");
        if (SendMessageW(log->tail, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            const int rows = ListView_GetItemCount(log->list);
            if (rows > 0) ListView_EnsureVisible(log->list, rows - 1, FALSE);
        }
        return 0;
    }
    case kMessageLogFiltered: {
        std::unique_ptr<LogFilterResult> result(reinterpret_cast<LogFilterResult*>(lParam));
        if (!log || result->generation != log->generation || !result->ok) return 0;
        log->view = std::move(result->view);
        log->tailState = result->endState;
        log->filtered = true;
        SetLogRowCount(*log, false);
        UpdateLogStatus(*log, L"筛选用时 " + std::to_wstring(GetTickCount() - log->started) + L" ms");
        if (ListView_GetItemCount(log->list) > 0) ListView_SetItemState(log->list, 0, LVIS_FOCUSED, LVIS_FOCUSED);
        return 0;
    }
    case WM_TIMER:
        if (log && wParam == kTimerLogTail) PollLogTail(*log);
        return 0;
    case WM_NOTIFY: {
        auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (log && header->hwndFrom == log->list && header->code == NM_DBLCLK) {
            // The clicked line and its neighbourhood into the editor.
            const int row = ListView_GetNextItem(log->list, -1, LVNI_FOCUSED);
            if (row >= 0) {
                const uint64_t line = LogLineOfRow(*log, static_cast<uint64_t>(row));
                const uint64_t first = line > 200 ? line - 200 : 0;
                const uint64_t last = std::min<uint64_t>(log->index.Lines(log->size), line + 200);
                std::wstring text;
                for (uint64_t l = first; l < last; ++l) text += LogLineText(*log, l, 1 << 20) + L"\r\n";
                const int index = CreateDocument({}, fs::path(log->path).filename().wstring() + L" - 第 " + std::to_wstring(first + 1) + L"-" +
                    std::to_wstring(last) + L" 行", text, true, true, 0, 0, 0);
                if (index >= 0) {
                    SelectTab(index);
                    if (HWND editor = ActiveEditor()) {
                        const LRESULT position = SendMessageW(editor, EM_LINEINDEX, static_cast<WPARAM>(line - first), 0);
                        SendMessageW(editor, EM_SETSEL, position, position);
                        SendMessageW(editor, EM_SCROLLCARET, 0, 0);
                    }
                    SetForegroundWindow(g_app.window);
                }
            }
            return 0;
        }
        break;
    }
    case WM_COMMAND:
        if (!log) break;
        switch (LOWORD(wParam)) {
        case kLogApply: case IDOK: ApplyLogFilterFromControls(*log); return 0;
        case kLogClear:
            SetWindowTextW(log->keyword, L"");
            SetWindowTextW(log->from, L"");
            SetWindowTextW(log->to, L"");
            for (HWND box : log->levelBoxes) SendMessageW(box, BM_SETCHECK, BST_CHECKED, 0);
            ApplyLogFilterFromControls(*log);
            return 0;
        case kLogTail:
            if (SendMessageW(log->tail, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                SetTimer(window, kTimerLogTail, 500, nullptr);
                PollLogTail(*log);
                const int rows = ListView_GetItemCount(log->list);
                if (rows > 0) ListView_EnsureVisible(log->list, rows - 1, FALSE);
            } else {
                KillTimer(window, kTimerLogTail);
                UpdateLogStatus(*log);
            }
            return 0;
        case kLogEnd: {
            const int rows = ListView_GetItemCount(log->list);
            if (rows > 0) {
                ListView_SetItemState(log->list, -1, 0, LVIS_SELECTED);
                ListView_SetItemState(log->list, rows - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                ListView_EnsureVisible(log->list, rows - 1, FALSE);
            }
            return 0;
        }
        case kLogExport: ExportLogToTab(*log); return 0;
        case IDCANCEL: DestroyWindow(window); return 0;
        default:
            if (LOWORD(wParam) >= kLogLevelFirst && LOWORD(wParam) < kLogLevelFirst + 5 && HIWORD(wParam) == BN_CLICKED) {
                ApplyLogFilterFromControls(*log);
                return 0;
            }
            break;
        }
        break;
    case WM_DESTROY:
        if (log) {
            KillTimer(window, kTimerLogTail);
            log->cancel = true;
            while (log->busy) Sleep(5);
            CloseLogFile(*log);
        }
        break;
    case WM_NCDESTROY:
        delete log;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        break;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool OpenLogViewer(const std::wstring& path) {
    auto log = std::make_unique<LogViewer>();
    log->path = path;
    log->file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (log->file == INVALID_HANDLE_VALUE || !MapLogFile(*log)) {
        CloseLogFile(*log);
        MessageBoxW(g_app.window, (L"无法打开：" + path).c_str(), kWindowTitle, MB_ICONERROR);
        return false;
    }
    if (log->size >= 2 && ((static_cast<unsigned char>(log->data[0]) == 0xFF && static_cast<unsigned char>(log->data[1]) == 0xFE) ||
                           (static_cast<unsigned char>(log->data[0]) == 0xFE && static_cast<unsigned char>(log->data[1]) == 0xFF))) {
        CloseLogFile(*log);
        MessageBoxW(g_app.window, L"日志查看器暂不支持 UTF-16 编码的文件，请在编辑器中打开。", kWindowTitle, MB_ICONINFORMATION);
        return false;
    }
    // UTF-8 unless the start of the file is not valid UTF-8.
    if (log->size > 0) {
        int sample = static_cast<int>(std::min<uint64_t>(log->size, 1 << 20));
        while (sample > 0 && sample < static_cast<int>(log->size) && (static_cast<unsigned char>(log->data[sample]) & 0xC0) == 0x80) --sample;
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, log->data, sample, nullptr, 0) == 0) log->codePage = CP_ACP;
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{};
        klass.hInstance = g_app.instance;
        klass.lpfnWndProc = LogViewerProc;
        klass.lpszClassName = L"MCnotepadLog";
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        klass.hIcon = LoadIconW(g_app.instance, MAKEINTRESOURCEW(IDI_MCNOTEPAD));
        registered = RegisterClassW(&klass) != 0;
    }
    RECT owner{};
    GetWindowRect(g_app.window, &owner);
    LogViewer* raw = log.release();
    const std::wstring title = L"日志 - " + path;
    HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, L"MCnotepadLog", title.c_str(), WS_OVERLAPPEDWINDOW,
        owner.left + Scale(30), owner.top + Scale(30), std::max(Scale(1100), static_cast<int>(owner.right - owner.left) - Scale(60)),
        std::max(Scale(640), static_cast<int>(owner.bottom - owner.top) - Scale(60)), nullptr, nullptr, g_app.instance, raw);
    if (!window) {
        CloseLogFile(*raw);
        delete raw;
        return false;
    }
    ApplyWindowFrameTheme(window);
    g_toolWindows.push_back(window);
    LayoutLogViewer(*raw);
    ShowWindow(window, SW_SHOW);
    StartLogIndex(*raw);
    return true;
}

void OpenLogViewerCommand() {
    const std::wstring path = PickFile(L"选择日志文件（支持数 GB 的大文件）");
    if (!path.empty()) OpenLogViewer(path);
}

bool LogViewerKey(HWND root, WPARAM key) {
    auto* log = reinterpret_cast<LogViewer*>(GetWindowLongPtrW(root, GWLP_USERDATA));
    if (!log) return false;
    const bool control = GetKeyState(VK_CONTROL) < 0;
    if (control && key == 'C' && GetFocus() == log->list) { CopyLogSelection(*log); return true; }
    if (control && key == 'F') { SetFocus(log->keyword); SendMessageW(log->keyword, EM_SETSEL, 0, -1); return true; }
    if (key == VK_F5) {
        log->index = LogIndex{};
        MapLogFile(*log);
        StartLogIndex(*log);
        return true;
    }
    if (key == VK_RETURN && GetFocus() != log->list) { ApplyLogFilterFromControls(*log); return true; }
    return false;
}

// Keys for the compare window: F8 / Shift+F8 next/previous difference,
// Ctrl+S saves the changed sides, F5 refreshes.
bool ToolWindowKey(HWND root, WPARAM key) {
    wchar_t klass[64]{};
    GetClassNameW(root, klass, 64);
    const bool shift = GetKeyState(VK_SHIFT) < 0, control = GetKeyState(VK_CONTROL) < 0;
    if (wcscmp(klass, L"MCnotepadCompare") == 0) {
        auto* compare = reinterpret_cast<CompareWindow*>(GetWindowLongPtrW(root, GWLP_USERDATA));
        if (!compare) return false;
        if (key == VK_F8 || key == VK_F7) { GoToHunk(*compare, compare->current + ((key == VK_F7 || shift) ? -1 : 1)); return true; }
        if (key == VK_F5) { SendMessageW(root, WM_COMMAND, kCmpRefresh, 0); return true; }
        if (control && key == 'S') {
            for (int s = 0; s < 2; ++s) if (compare->sides[s].dirty) SaveCompareSide(*compare, s);
            return true;
        }
    }
    if (wcscmp(klass, L"MCnotepadLog") == 0) return LogViewerKey(root, key);
    if (wcscmp(klass, L"MCnotepadFolderCompare") == 0 && key == VK_F5) {
        SendMessageW(root, WM_COMMAND, kFolderRefresh, 0);
        return true;
    }
    return false;
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
        LoadSearchState();
        if (g_app.searchPanelVisible) EnsureSearchPanel();
        RefreshChrome();
        LayoutChildren(window);
        SetMenuState();
        SetTimer(window, kTimerAutosave, 2000, nullptr);

        return 0;
    }
    case WM_SIZE:
        LayoutChildren(window);
        if (wParam == SIZE_MINIMIZED && g_app.minimizeToTray && !g_app.closing) {
            HideMainWindowToTray();
        }
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
                HighlightDocument(*document);
            }
        }
        return 0;
    case kMessageTrayIcon: {
        // Legacy shells pass the icon ID in wParam and the mouse event in
        // lParam. NOTIFYICON_VERSION_4 packs the event in LOWORD(lParam),
        // the icon ID in HIWORD(lParam), and uses wParam for coordinates.
        UINT trayEvent = 0;
        if (HIWORD(lParam) == kTrayIconId) {
            trayEvent = LOWORD(lParam);
        } else if (static_cast<UINT>(wParam) == kTrayIconId) {
            trayEvent = static_cast<UINT>(lParam);
        } else {
            return 0;
        }
        if (trayEvent == WM_LBUTTONDBLCLK || trayEvent == NIN_SELECT || trayEvent == NIN_KEYSELECT) {
            RestoreMainWindowFromTray();
            return 0;
        }
        // With NOTIFYICON_VERSION_4 Windows reports a tray right-click as
        // WM_CONTEXTMENU. Older shells use WM_RBUTTONUP, so accept both.
        if (trayEvent == WM_CONTEXTMENU || trayEvent == WM_RBUTTONUP) {
            HMENU trayMenu = CreatePopupMenu();
            AppendMenuW(trayMenu, MF_STRING, kIdTrayShow, L"显示 MCnotepad");
            AppendMenuW(trayMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(trayMenu, MF_STRING, kIdTrayExit, L"退出软件");
            POINT point{};
            GetCursorPos(&point);
            SetForegroundWindow(window);
            // Return the selected command directly. This remains reliable
            // while the main window is hidden, where routing WM_COMMAND back
            // through the owner window can be swallowed by some shells.
            const UINT command = TrackPopupMenu(trayMenu,
                TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
                point.x, point.y, 0, window, nullptr);
            DestroyMenu(trayMenu);
            PostMessageW(window, WM_NULL, 0, 0);
            if (command == kIdTrayShow) {
                RestoreMainWindowFromTray();
            } else if (command == kIdTrayExit) {
                g_app.windowCloseBehavior = WindowCloseBehavior::Exit;
                HandleMainWindowClose(true);
            }
            return 0;
        }
        return 0;
    }
    case WM_COMMAND: {
        const int command = LOWORD(wParam);
        const int notification = HIWORD(wParam);
        HWND source = reinterpret_cast<HWND>(lParam);
        RecordMacroCommand(static_cast<UINT>(command));
        if (command == kIdTrayShow) {
            RestoreMainWindowFromTray();
            return 0;
        }
        if (command == kIdTrayExit) {
            g_app.windowCloseBehavior = WindowCloseBehavior::Exit;
            HandleMainWindowClose(true);
            return 0;
        }
        if (notification == EN_CHANGE && source != nullptr && !g_app.loading) {
            for (size_t index = 0; index < g_app.documents.size(); ++index) {
                if (g_app.documents[index].editor == source) {
                    const uint64_t hash = HashText(GetEditorRawText(source));
                    if (hash == g_app.documents[index].textHash) break;  // formatting only
                    g_app.documents[index].textHash = hash;
                    const bool wasModified = g_app.documents[index].modified;
                    g_app.documents[index].modified = true;
                    MarkRecoveryDirty(static_cast<int>(index));
                    if (!wasModified) UpdateTabTitle(static_cast<int>(index));
                    UpdateStatusBar();
                    if (g_app.showLineNumbers && GutterWidthFor(source) != g_app.gutterWidth) LayoutChildren(window);
                    UpdateHorizontalScrollbar(source);
                    RefreshGutter();
                    TabDocument& changed = g_app.documents[index];
                    ApplyCurrentLineHighlight(changed, true);
                    ApplyBracketMatchHighlight(changed, true);
                    NormalizeBookmarks(changed);
                    changed.syntaxStale = true;  // tokens no longer match; scrolling re-tokenizes
                    if (EffectiveLanguage(changed) != kLanguagePlain || changed.syntaxHadColors ||
                        (changed.untitled && changed.language == kLanguageAuto)) {
                        SetTimer(window, kTimerHighlight, kHighlightDelayMs, nullptr);  // restarts on each keystroke
                    }
                    break;
                }
            }
            return 0;
        }
        if (notification == EN_VSCROLL) {
            RefreshGutter();
            SyntaxOnViewChanged(source);
            UpdateStatusBar();
            return 0;
        }
        if (command == kIdLineSpacingSingle || command == kIdLineSpacing115 ||
            command == kIdLineSpacing150 || command == kIdLineSpacing200) {
            static constexpr int values[] = {100, 115, 150, 200};
            SetLineSpacingPercent(values[command - kIdLineSpacingFirst]);
            return 0;
        }
        if (command == kIdLineSpacingCustom) {
            OpenCustomLineSpacingDialog();
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

        if (command >= kIdSearchHistoryFirst && command < kIdSearchHistoryFirst + 20) {
            RunSearchFromHistory(command - kIdSearchHistoryFirst);
            return 0;
        }
        if (IsDataGeneratorCommand(command)) {
            OpenDataGeneratorDialog(command);
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
        case kIdFileSaveAll: SaveAllDocuments(); break;
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
        case kIdSearchInFolder: ShowSearchPanel(0, 0, -1, false); break;
        case kIdReplaceInFolder: ShowSearchPanel(0, 0, -1, true); break;
        case kIdSearchCurrentFile: ShowSearchPanel(2, 0, -1, false); break;
        case kIdSearchOpenFiles: ShowSearchPanel(1, 0, -1, false); break;
        case kIdSearchFileNames: ShowSearchPanel(0, 1, -1, false); break;
        case kIdSearchRegex: ShowSearchPanel(-1, 0, 1, false); break;
        case kIdToggleSearchPanel: ToggleSearchPanel(); break;
        case kIdExportSearchResults: ExportSearchResults(); break;
        case kIdCopySearchResults: CopyTextToClipboard(SearchResultsAsText()); ShowNotice(L"已复制全部搜索结果"); break;
        case kIdBookmarkSearchResult: BookmarkSelectedSearchResult(); break;
        case kIdClearSearchResults: ClearSearchResults(); break;
        case kIdSearchErrorLog: ShowSearchErrorLog(); break;
        case kIdUndoReplaceInFiles: UndoLastReplaceInFiles(); break;
        case kIdCompareFiles: CompareTwoFilesCommand(false); break;
        case kIdCompareStructure: CompareTwoFilesCommand(true); break;
        case kIdCompareWithFile: CompareActiveWithFileCommand(); break;
        case kIdCompareFolders: CompareFoldersCommand(); break;
        case kIdOpenLogViewer: OpenLogViewerCommand(); break;
        case kIdToggleToolbox: ToggleToolbox(!g_app.toolboxVisible); break;
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
        case kIdViewToggleBookmark:
            ToggleBookmarkAtCaret();
            break;
        case kIdViewNextBookmark:
            GotoBookmark(1);
            break;
        case kIdViewPreviousBookmark:
            GotoBookmark(-1);
            break;
        case kIdViewClearBookmarks:
            ClearBookmarks();
            break;
        case kIdViewWhitespace:
            g_app.showWhitespace = !g_app.showWhitespace;
            g_app.stateDirty = true;
            for (const TabDocument& document : g_app.documents) InvalidateRect(document.editor, nullptr, FALSE);
            break;
        case kIdViewIndentGuides:
            g_app.showIndentGuides = !g_app.showIndentGuides;
            g_app.stateDirty = true;
            for (const TabDocument& document : g_app.documents) InvalidateRect(document.editor, nullptr, FALSE);
            break;
        case kIdViewFoldCurrent:
            FoldCurrentBlock();
            break;
        case kIdViewUnfoldAll:
            UnfoldAllBlocks();
            break;
        case kIdToolbarTheme:
            ShowThemeMenu(ToolbarItemScreenRect(kIdToolbarTheme));
            break;
        case kIdToolbarSettings:
            OpenSettingsDialog();
            break;
        case kIdSettingsShortcuts:
            OpenShortcutDialog();
            break;
        case kIdPrefixSuffix:
            OpenPrefixSuffixDialog();
            break;
        case kIdMultiCaretNext:
            AddNextLineCaret();
            break;
        case kIdRectangleSelection:
            CreateRectangleCarets();
            break;
        case kIdClearMultiCaret:
            ClearMultiCarets();
            break;
        case kIdMacroToggle:
            ToggleMacroRecording();
            break;
        case kIdMacroPlay:
            PlayMacro();
            break;
        case kIdMacroClear:
            g_app.macroActions.clear();
            ShowNotice(L"最近宏已清除");
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
        case kIdFileCloseAll: CloseAllDocuments(); break;
        case kIdFileReopenClosed: ReopenClosedDocument(); break;
        case kIdFileExit: HandleMainWindowClose(true); break;
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
            ToolboxOnSelectionChanged();
            RefreshGutter();  // current-line number follows the caret
            if (TabDocument* document = ActiveDocument()) {
                ApplyCurrentLineHighlight(*document);
                ApplyBracketMatchHighlight(*document);
            }
            return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        HandleMainWindowClose();
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
        for (size_t f = 0; f < files.size(); ++f) {
            if (files[f] == L"--log" && f + 1 < files.size()) {
                OpenLogViewer(files[++f]);
                continue;
            }
            if ((files[f] == L"--diff" || files[f] == L"--diff-struct") && f + 2 < files.size()) {
                std::error_code error;
                if (fs::is_directory(files[f + 1], error) && fs::is_directory(files[f + 2], error)) CompareFolderPaths(files[f + 1], files[f + 2]);
                else CompareFiles(files[f + 1], files[f + 2], files[f] == L"--diff-struct");
                f += 2;
                continue;
            }
            OpenFileInNewTab(fs::path(files[f]));
        }
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
        RemoveTrayIcon();
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
            if (argument == L"--diff" || argument == L"--diff-struct" || argument == L"--log") {
                payload += argument;
                payload.push_back(L'\0');
                continue;
            }
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

int RunSelfTests() {
    const auto expect = [](bool condition) { return condition ? 0 : 1; };
    int failures = 0;
    failures += expect(FindMatchingBracket(L"{alpha [beta]}", 0) == 13);
    failures += expect(FindMatchingBracket(L"{alpha [beta]}", 12) == 7);
    failures += expect(HashText(L"abc") == HashText(L"abc"));
    failures += expect(HashText(L"abc") != HashText(L"abd"));
    failures += expect(DataUnescape(L"a\\nb\\t") == L"a\r\nb\t");
    BYTE modifiers = 0;
    WORD key = 0;
    failures += expect(ParseShortcutText(L"Ctrl+Shift+S", modifiers, key) &&
        (modifiers & FCONTROL) && (modifiers & FSHIFT) && key == 'S');
    failures += expect(!ParseShortcutText(L"Ctrl+Unknown", modifiers, key));
    const auto lines = SplitLines(L"a\r\nb\nc");
    failures += expect(lines.size() == 3 && JoinLines(lines, L"\r") == L"a\rb\rc");
    const std::wstring cjkLabel = L"姓名：用户名：";
    failures += expect(Utf8ToWide(WideToUtf8(cjkLabel)) == cjkLabel);
    for (int locale = 0; locale < 5; ++locale) {
        DataGeneratorOptions profile;
        profile.count = 1;
        profile.locale = locale;
        const std::wstring generated = GenerateDataText(kDataFullProfile, profile);
        failures += expect(!generated.empty() &&
            generated.find(L"姓名：") != std::wstring::npos &&
            generated.find(L'\uFFFD') == std::wstring::npos);
    }
    return failures == 0 ? 0 : 1;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand) {
    (void)commandLine;
    g_app.instance = instance;
    InitializeShortcutBindings();

    if (commandLine != nullptr && wcsstr(commandLine, L"--self-test") != nullptr) {
        return RunSelfTests();
    }

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
    commonControls.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);  // folder picker
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
    UpdateTrayIcon();
    if (g_app.toolboxVisible) ToggleToolbox(true);
    OpenCommandLineFiles();
    PostMessageW(window, kMessageCheckExternalChanges, 0, 0);

    RebuildAccelerators();
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        // Modeless dialogs first: gives them Tab/Enter/Esc handling and keeps the
        // main window's Ctrl+C/V/Z accelerators from hijacking their text boxes.
        // Search panel: app shortcuts first (edit commands are routed to its
        // text boxes), then Tab/Enter/Esc dialog navigation.
        if ((g_app.searchPanel && IsChild(g_app.searchPanel, message.hwnd)) || (g_app.toolboxPanel && IsChild(g_app.toolboxPanel, message.hwnd))) {
            if (TranslateAcceleratorW(window, g_accelerators, &message)) continue;
            HWND panel = g_app.toolboxPanel && IsChild(g_app.toolboxPanel, message.hwnd) ? g_app.toolboxPanel : g_app.searchPanel;
            // Enter belongs to the multi-line boxes.
            wchar_t klass[16]{};
            GetClassNameW(message.hwnd, klass, 16);
            const bool multilineEnter = message.message == WM_KEYDOWN && message.wParam == VK_RETURN && _wcsicmp(klass, L"Edit") == 0 &&
                (GetWindowLongW(message.hwnd, GWL_STYLE) & ES_MULTILINE);
            if (!multilineEnter && IsDialogMessageW(panel, &message)) continue;
            TranslateMessage(&message);
            DispatchMessageW(&message);
            continue;
        }
        // Compare / log windows: their own keys, dialog navigation, no main accelerators.
        if (HWND root = GetAncestor(message.hwnd, GA_ROOT);
            root && std::find(g_toolWindows.begin(), g_toolWindows.end(), root) != g_toolWindows.end()) {
            if (!IsWindow(root)) continue;
            if (message.message == WM_KEYDOWN && ToolWindowKey(root, message.wParam)) continue;
            if (IsDialogMessageW(root, &message)) continue;
            TranslateMessage(&message);
            DispatchMessageW(&message);
            continue;
        }
        g_toolWindows.erase(std::remove_if(g_toolWindows.begin(), g_toolWindows.end(), [](HWND w) { return !IsWindow(w); }), g_toolWindows.end());
        bool handled = false;
        for (HWND dialog : {g_app.findDialog, g_app.gotoWindow, g_app.settingsWindow}) {
            if (dialog && IsWindow(dialog) && IsDialogMessageW(dialog, &message)) {
                handled = true;
                break;
            }
        }
        if (handled) continue;
        if (!TranslateAcceleratorW(window, g_accelerators, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (g_accelerators) {
        DestroyAcceleratorTable(g_accelerators);
        g_accelerators = nullptr;
    }

    return static_cast<int>(message.wParam);
}

