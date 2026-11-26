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
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../resources/resource.h"
#include "version.h"
#include "WebView2.h"

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
constexpr int kIdTabNext = 1245;
constexpr int kIdTabPrevious = 1246;
constexpr int kIdViewZoomIn = 1247;
constexpr int kIdViewZoomOut = 1248;
constexpr int kIdViewZoomReset = 1249;
constexpr int kIdViewLivePreview = 1250;
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
    // Keep the range inclusive of every text/developer transformation command.
    // The Markdown/HTML commands are handled by RunTextTool and must reach it
    // through IsTextToolCommand/WM_COMMAND as well.
    kIdTextToolLast = kIdHtmlToMarkdown
};
// Additional extraction commands are kept outside the original contiguous
// text-tool range so existing shortcut IDs remain compatible.
constexpr int kIdExtractDates = 2201;
constexpr int kIdExtractPhones = 2202;
constexpr int kIdExtractUuid = 2203;
constexpr int kIdExtractHexColors = 2204;
constexpr int kIdExtractQuoted = 2205;
constexpr int kIdExtractCustomRegex = 2206;
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
constexpr int kIdSettingsPreviewAuto = 1714;
constexpr int kIdShortcutEditFirst = 2120;
// Keep these outside the dynamically allocated edit-control ID range.
constexpr int kIdShortcutApply = 2500;
constexpr int kIdShortcutReset = 2501;
constexpr int kIdShortcutCancel = 2502;
constexpr int kIdShortcutTabs = 2503;
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
constexpr int kIdTopSpacingCustom = kIdLineSpacingFirst + 5;

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
    bool previewOpen = false;  // Markdown / HTML preview pane
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
    bool syntaxResetAll = false; // painted ranges are no longer tracked: clear colours outside the view once
    uint64_t textHash = 0;       // to tell real edits from formatting-only EN_CHANGE
    std::vector<int> bookmarks;  // 1-based logical line numbers
    int highlightedLine = -1;     // -1: no current-line highlight is applied
    LONG highlightedStart = 0;    // its character range, and the text length when it was applied
    LONG highlightedEnd = 0;
    LONG highlightedLength = 0;
    LONG bracketMatchStart = -1;
    LONG bracketMatchEnd = -1;
    std::vector<FoldRegion> folds;
    uint64_t foldTextHash = 0;
    int logicalLineCount = 1;     // real lines (paragraphs), to shift bookmarks on edits
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
    int fontSize = 13;            // current size (Ctrl+wheel / Ctrl+= / Ctrl+- change it)
    int defaultFontSize = 13;     // set in Settings; new windows and Ctrl+0 use it
    bool previewVisible = false;  // Markdown / HTML preview pane shown for the active tab
    int previewEditorPercent = 50;  // editor share of the width while the preview is shown
    bool previewAutoOpen = false;  // open the preview when a Markdown / HTML file is opened
    int lineSpacingPercent = 100;
    int topSpacingPixels = 0;
    std::wstring fontFamily = L"JetBrains Mono";  // one of the bundled programming fonts
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
    HWND settingsPreviewAutoCheck = nullptr;
    fs::path dataDirectory;
    fs::path recoveryDirectory;
    fs::path sessionPath;
    fs::path runningMarkerPath;
};

AppState g_app;

// While a dialog is open, work that can change or reorder the tabs (reloading
// a file changed elsewhere, opening files sent by another instance) waits:
// the dialog may hold a reference to a tab or a range in its text.
int g_modalDepth = 0;
bool g_deferredExternalCheck = false;
bool g_deferredOpenFiles = false;

bool AppIsInModalState() {
    return g_modalDepth > 0 || (g_app.window && !IsWindowEnabled(g_app.window));
}

void ResumeDeferredWork() {
    if (AppIsInModalState() || !g_app.window) return;
    if (g_deferredExternalCheck) { g_deferredExternalCheck = false; PostMessageW(g_app.window, kMessageCheckExternalChanges, 0, 0); }
    if (g_deferredOpenFiles) { g_deferredOpenFiles = false; PostMessageW(g_app.window, kMessageOpenPendingFiles, 0, 0); }
}

// Marks a custom modal message loop.
struct ModalScope {
    ModalScope() { ++g_modalDepth; }
    ~ModalScope() {
        --g_modalDepth;
        ResumeDeferredWork();
    }
    ModalScope(const ModalScope&) = delete;
    ModalScope& operator=(const ModalScope&) = delete;
};

struct ShortcutBinding {
    int command = 0;
    std::wstring label;
    std::wstring text;
};

std::vector<ShortcutBinding> g_shortcuts;
HACCEL g_accelerators = nullptr;
HWND g_shortcutDialog = nullptr;
HWND g_shortcutDialogOwner = nullptr;
HWND g_shortcutTabs = nullptr;
std::vector<HWND> g_shortcutEditControls;
struct ShortcutDialogControl {
    HWND window = nullptr;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int page = 0;
};
std::vector<ShortcutDialogControl> g_shortcutDialogControls;
HWND g_shortcutDialogHint = nullptr;
HWND g_shortcutDialogApply = nullptr;
HWND g_shortcutDialogReset = nullptr;
HWND g_shortcutDialogCancel = nullptr;
int g_shortcutDialogContentHeight = 0;
int g_shortcutDialogScrollOffset = 0;
int g_shortcutDialogPage = 0;

constexpr int kShortcutPageCount = 10;
constexpr const wchar_t* kShortcutPageNames[kShortcutPageCount] = {
    L"文件", L"编辑", L"标签", L"搜索", L"视图",
    L"工具", L"开发者", L"数据", L"设置", L"其他"
};

std::string HexEncode(std::string_view value);
std::string WideToUtf8(std::wstring_view value);
std::wstring Utf8ToWide(std::string_view value);
std::wstring DataGeneratorLabel(int command);

std::wstring ShortcutLabelForCommand(int command) {
    switch (command) {
    case kIdExtractDates: return L"文本：提取日期/时间";
    case kIdExtractPhones: return L"文本：提取手机号";
    case kIdExtractUuid: return L"文本：提取 UUID/GUID";
    case kIdExtractHexColors: return L"文本：提取十六进制颜色";
    case kIdExtractQuoted: return L"文本：提取引号内容";
    case kIdExtractCustomRegex: return L"文本：提取自定义正则匹配";
    default: break;
    }
    switch (command) {
    case kIdFileNew: return L"文件：新建";
    case kIdFileOpen: return L"文件：打开";
    case kIdFileSave: return L"文件：保存";
    case kIdFileSaveAs: return L"文件：另存为";
    case kIdFileSaveAll: return L"文件：保存全部";
    case kIdFileClose: return L"文件：关闭标签";
    case kIdFileCloseAll: return L"文件：关闭全部标签";
    case kIdFileReopenClosed: return L"文件：重新打开关闭的标签";
    case kIdFileRename: return L"文件：重命名标签";
    case kIdFileExit: return L"文件：退出";
    case kIdHelpAbout: return L"帮助：关于 MCnotepad";
    case kIdTabNext: return L"标签：下一个标签";
    case kIdTabPrevious: return L"标签：上一个标签";
    case kIdTabTogglePinned: return L"标签：固定/取消固定当前标签";
    case kIdEditUndo: return L"编辑：撤销";
    case kIdEditRedo: return L"编辑：重做";
    case kIdEditCut: return L"编辑：剪切";
    case kIdEditCopy: return L"编辑：复制";
    case kIdEditPaste: return L"编辑：粘贴";
    case kIdEditSelectAll: return L"编辑：全选";
    case kIdPrefixSuffix: return L"编辑：前缀与后缀处理";
    case kIdMultiCaretNext: return L"编辑：添加下一行光标";
    case kIdRectangleSelection: return L"编辑：矩形选择";
    case kIdClearMultiCaret: return L"编辑：清除多光标";
    case kIdMacroToggle: return L"编辑：开始/停止宏录制";
    case kIdMacroPlay: return L"编辑：播放最近宏";
    case kIdMacroClear: return L"编辑：清除最近宏";
    case kIdToolbarFind: return L"搜索：查找";
    case kIdToolbarReplace: return L"搜索：替换";
    case kIdFindNext: return L"搜索：查找下一个";
    case kIdFindPrevious: return L"搜索：查找上一个";
    case kIdToolbarGoto: return L"搜索：跳转到行";
    case kIdSearchInFolder: return L"搜索：文件夹搜索";
    case kIdReplaceInFolder: return L"搜索：文件夹替换";
    case kIdSearchCurrentFile: return L"搜索：当前文件";
    case kIdSearchOpenFiles: return L"搜索：所有打开文件";
    case kIdSearchFileNames: return L"搜索：文件名";
    case kIdSearchRegex: return L"搜索：正则表达式";
    case kIdToggleSearchPanel: return L"搜索：显示搜索面板";
    case kIdExportSearchResults: return L"搜索：导出搜索结果";
    case kIdClearSearchResults: return L"搜索：清除搜索结果";
    case kIdSearchErrorLog: return L"搜索：查看错误日志";
    case kIdUndoReplaceInFiles: return L"搜索：撤销文件替换";
    case kIdCopySearchResults: return L"搜索：复制搜索结果";
    case kIdBookmarkSearchResult: return L"搜索：标记搜索结果";
    case kIdViewToggleBookmark: return L"视图：切换书签";
    case kIdViewZoomIn: return L"视图：放大字号";
    case kIdViewZoomOut: return L"视图：缩小字号";
    case kIdViewZoomReset: return L"视图：恢复默认字号";
    case kIdViewLivePreview: return L"视图：Markdown/HTML 预览";
    case kIdViewNextBookmark: return L"视图：下一个书签";
    case kIdViewPreviousBookmark: return L"视图：上一个书签";
    case kIdViewClearBookmarks: return L"视图：清除书签";
    case kIdViewWhitespace: return L"视图：显示空格和制表符";
    case kIdViewIndentGuides: return L"视图：显示缩进参考线";
    case kIdViewFoldCurrent: return L"视图：折叠/展开当前代码块";
    case kIdViewUnfoldAll: return L"视图：展开全部代码块";
    case kIdToolbarLineNumbers: return L"视图：显示行号";
    case kIdToolbarWrap: return L"视图：自动换行";
    case kIdToolbarEncoding: return L"编码：编码和换行符菜单";
    case kIdToolbarFormat: return L"工具：格式化 JSON";
    case kIdToolbarRun: return L"运行：运行当前文件";
    case kIdToggleToolbox: return L"工具：开发者工具箱";
    case kIdCompareWithFile: return L"工具：比较当前文件与...";
    case kIdCompareFiles: return L"工具：比较两个文件";
    case kIdCompareStructure: return L"工具：结构化比较";
    case kIdCompareFolders: return L"工具：比较文件夹";
    case kIdOpenLogViewer: return L"工具：日志查看器";
    case kIdToolbarSettings: return L"设置：偏好设置";
    case kIdSettingsShortcuts: return L"设置：快捷键设置";
    case kIdThemeDark: return L"设置：切换到深色主题";
    case kIdThemeLight: return L"设置：切换到浅色主题";
    case kIdThemeGreen: return L"设置：切换到护眼绿主题";
    case kIdThemeBlue: return L"设置：切换到暗夜蓝主题";
    case kIdLineSpacingSingle: return L"视图：行距 1.0 倍";
    case kIdLineSpacing115: return L"视图：行距 1.15 倍";
    case kIdLineSpacing150: return L"视图：行距 1.5 倍";
    case kIdLineSpacing200: return L"视图：行距 2.0 倍";
    case kIdLineSpacingCustom: return L"视图：自定义行距";
    case kIdTopSpacingCustom: return L"视图：自定义顶部间距";
    default: break;
    }
    if (command >= kIdEncodingFirst && command < kIdEncodingFirst + kEncodingCount) {
        return L"编码：" + std::wstring(kEncodingNames[command - kIdEncodingFirst]);
    }
    if (command >= kIdLineEndingFirst && command < kIdLineEndingFirst + kLineEndingCount) {
        return L"换行符：" + std::wstring(kLineEndingNames[command - kIdLineEndingFirst]);
    }
    if (command >= kIdLanguageFirst && command < kIdLanguageFirst + kLanguageCount) {
        return L"语言：" + std::wstring(kLanguageNames[command - kIdLanguageFirst]);
    }
    if (command >= kIdTextToolFirst && command <= kIdTextToolLast) {
        static const wchar_t* const labels[] = {
            L"文本：合并为一行", L"文本：合并为一行（空格）", L"文本：合并为一行（自定义）",
            L"文本：每 N 行合并", L"文本：按分隔符拆分", L"文本：指定字符后换行", L"文本：按长度换行",
            L"文本：删除空行", L"文本：合并连续空行", L"文本：删除重复行", L"文本：升序排序",
            L"文本：降序排序", L"文本：反转行顺序", L"文本：添加前缀", L"文本：添加后缀", L"文本：添加行号",
            L"文本：去除行首空白", L"文本：去除行尾空白", L"文本：去除两端空白", L"文本：合并连续空格",
            L"文本：删除所有空格", L"文本：制表符转空格", L"文本：全角转半角", L"文本：半角转全角",
            L"文本：中文标点转英文", L"文本：英文标点转中文", L"文本：清除不可见字符",
            L"文本：转为大写", L"文本：转为小写", L"文本：单词首字母大写", L"文本：汉字转拼音",
            L"文本：汉字转拼音（无声调）", L"文本：汉字转拼音首字母", L"文本：简体转繁体",
            L"文本：繁体转简体", L"开发者：Base64 编码", L"开发者：Base64 解码", L"开发者：URL 编码",
            L"开发者：URL 解码", L"开发者：正则表达式替换", L"文本：提取邮箱", L"文本：提取链接",
            L"文本：提取 IP", L"文本：提取数字", L"文本：提取关键词行", L"文本：提取指定列",
            L"文本：替换原文输出", L"文本：输出到新标签", L"文本：插入到原文下方", L"文本：切换处理前预览",
            L"开发者：CSS 美化", L"开发者：CSS 压缩", L"开发者：JavaScript 美化",
            L"开发者：JavaScript 压缩", L"开发者：Markdown 转 HTML", L"开发者：HTML 转 Markdown"
        };
        const int index = command - kIdTextToolFirst;
        if (index >= 0 && index < static_cast<int>(std::size(labels))) return labels[index];
    }
    if (command >= kIdDataFirst && command <= kIdDataLast) {
        return L"数据：" + DataGeneratorLabel(command);
    }
    return L"命令 " + std::to_wstring(command);
}

void InitializeShortcutBindings() {
    if (!g_shortcuts.empty()) return;
    const auto add = [](int command, std::wstring text = {}) {
        g_shortcuts.push_back({command, ShortcutLabelForCommand(command), std::move(text)});
    };
    add(kIdFileNew, L"Ctrl+N"); add(kIdFileOpen, L"Ctrl+O"); add(kIdFileSave, L"Ctrl+S");
    add(kIdFileSaveAs, L"Ctrl+Shift+S"); add(kIdFileSaveAll, L"Ctrl+Alt+S"); add(kIdFileClose, L"Ctrl+W");
    add(kIdFileCloseAll, L"Ctrl+Shift+W"); add(kIdFileReopenClosed, L"Ctrl+Shift+T"); add(kIdFileRename, L"F2");
    add(kIdFileExit, L"Ctrl+Q");
    add(kIdTabNext, L"Ctrl+Tab"); add(kIdTabPrevious, L"Ctrl+Shift+Tab"); add(kIdTabTogglePinned);
    add(kIdEditUndo, L"Ctrl+Z"); add(kIdEditRedo, L"Ctrl+Y"); add(kIdEditCut, L"Ctrl+X");
    add(kIdEditCopy, L"Ctrl+C"); add(kIdEditPaste, L"Ctrl+V"); add(kIdEditSelectAll, L"Ctrl+A");
    add(kIdPrefixSuffix, L"Ctrl+Alt+P"); add(kIdMultiCaretNext, L"Ctrl+Alt+Down");
    add(kIdRectangleSelection, L"Ctrl+Alt+R"); add(kIdClearMultiCaret, L"Ctrl+Alt+Backspace");
    add(kIdMacroToggle, L"Ctrl+Alt+M"); add(kIdMacroPlay, L"Ctrl+Alt+Shift+M"); add(kIdMacroClear);
    add(kIdToolbarFind, L"Ctrl+F"); add(kIdToolbarReplace, L"Ctrl+H"); add(kIdFindNext, L"F3");
    add(kIdFindPrevious, L"Shift+F3"); add(kIdToolbarGoto, L"Ctrl+G"); add(kIdSearchInFolder, L"Ctrl+Shift+F");
    add(kIdReplaceInFolder, L"Ctrl+Shift+H"); add(kIdSearchCurrentFile); add(kIdSearchOpenFiles);
    add(kIdSearchFileNames); add(kIdSearchRegex); add(kIdToggleSearchPanel, L"Ctrl+Shift+J");
    add(kIdExportSearchResults, L"Ctrl+Alt+E"); add(kIdClearSearchResults); add(kIdSearchErrorLog);
    add(kIdUndoReplaceInFiles, L"Ctrl+Alt+Z"); add(kIdCopySearchResults); add(kIdBookmarkSearchResult);
    add(kIdHelpAbout);
    add(kIdViewToggleBookmark, L"Ctrl+F2"); add(kIdViewNextBookmark, L"F4"); add(kIdViewPreviousBookmark, L"Shift+F4");
    add(kIdViewClearBookmarks); add(kIdViewWhitespace, L"Ctrl+Alt+Space"); add(kIdViewIndentGuides, L"Ctrl+Alt+I");
    add(kIdViewFoldCurrent, L"F7"); add(kIdViewUnfoldAll, L"Shift+F7");
    add(kIdViewZoomIn, L"Ctrl+Plus"); add(kIdViewZoomOut, L"Ctrl+Minus"); add(kIdViewZoomReset, L"Ctrl+0");
    add(kIdViewLivePreview, L"Ctrl+Shift+V");
    add(kIdToolbarLineNumbers, L"Ctrl+Alt+L"); add(kIdToolbarWrap, L"Ctrl+Alt+W"); add(kIdToolbarEncoding);
    add(kIdLineSpacingSingle, L"Ctrl+Alt+1"); add(kIdLineSpacing115, L"Ctrl+Alt+2");
    add(kIdLineSpacing150, L"Ctrl+Alt+3"); add(kIdLineSpacing200, L"Ctrl+Alt+4"); add(kIdLineSpacingCustom);
    add(kIdTopSpacingCustom);
    add(kIdToolbarFormat, L"Shift+Alt+F"); add(kIdToggleToolbox, L"Ctrl+Shift+D"); add(kIdToolbarRun, L"F5");
    add(kIdCompareWithFile); add(kIdCompareFiles); add(kIdCompareStructure); add(kIdCompareFolders); add(kIdOpenLogViewer);
    add(kIdToolbarSettings, L"Ctrl+,"); add(kIdSettingsShortcuts, L"Ctrl+K");
    add(kIdThemeDark); add(kIdThemeLight); add(kIdThemeGreen); add(kIdThemeBlue);
    for (int command = kIdEncodingFirst; command < kIdEncodingFirst + kEncodingCount; ++command) add(command);
    for (int command = kIdLineEndingFirst; command < kIdLineEndingFirst + kLineEndingCount; ++command) add(command);
    for (int command = kIdLanguageFirst; command < kIdLanguageFirst + kLanguageCount; ++command) add(command);
    for (int command = kIdTextToolFirst; command <= kIdTextToolLast; ++command) add(command);
    add(kIdExtractDates); add(kIdExtractPhones); add(kIdExtractUuid);
    add(kIdExtractHexColors); add(kIdExtractQuoted); add(kIdExtractCustomRegex);
    for (int command = kIdDataFirst; command <= kIdDataLast; ++command) add(command);
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
std::unordered_set<HWND> g_imeComposingEditors;

std::wstring GetWindowTextValue(HWND window);
void ApplyEditorTheme(HWND editor);
void ApplyEditorTypography(HWND editor);
void ApplyCurrentLineHighlight(TabDocument& document, bool force = false);
void ApplyBracketMatchHighlight(TabDocument& document, bool force = false);
COLORREF Blend(COLORREF base, COLORREF overlay, int percentOverlay);
void HighlightDocument(const TabDocument& document);
void HighlightDocument(TabDocument& document);
void SyntaxColorVisible(TabDocument& document, bool retokenize = true);
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
void ApplyEditorTopInset(HWND editor);
constexpr int kEditorTextMargin = 6;  // gap between the line numbers and the text, 96-DPI pixels
void ApplyEditorLineSpacingAll();
void OpenCustomLineSpacingDialog();
void OpenCustomTopSpacingDialog();
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

// A C++ exception escaping a window procedure makes Windows end the process
// at once (0xC000041D), losing unsaved edits: the data generator did this
// with an empty Faker date range. Every window procedure is registered
// through Guarded<>, which saves recovery snapshots and reports the error.
void ReportUnexpectedException(const char* what);

template <WNDPROC Proc>
LRESULT CALLBACK Guarded(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    try {
        return Proc(window, message, wParam, lParam);
    } catch (const std::exception& error) {
        ReportUnexpectedException(error.what());
    } catch (...) {
        ReportUnexpectedException(nullptr);
    }
    return 0;
}

// Live preview (20_preview.cpp).
void UpdatePreviewForActiveTab();
void ToggleLivePreview();
void PreviewOnTextChanged(HWND editor);
void PreviewOnEditorScrolled(HWND editor);
void PreviewThemeChanged();
HWND PreviewPaneWindow();
bool IsPreviewable(const struct TabDocument& document);
