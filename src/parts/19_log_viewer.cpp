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
    if (!data || line / kLogCheckpointEvery >= index.checkpoints.size()) return {0, 0};
    uint64_t start = std::min(index.checkpoints[static_cast<size_t>(line / kLogCheckpointEvery)], size);
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

void StopLogWorker(LogViewer& log) {
    log.cancel = true;
    while (log.busy) Sleep(5);
    log.cancel = false;
}

void StartLogIndex(LogViewer& log);

void ReindexLog(LogViewer& log) {
    StopLogWorker(log);
    ++log.generation;  // results already posted by the old worker are ignored
    log.indexed = false;
    log.filtered = false;
    log.view.clear();
    log.index = LogIndex{};
    ListView_SetItemCountEx(log.list, 0, 0);
    if (!MapLogFile(log)) {
        log.size = 0;
        SetWindowTextW(log.status, L"无法读取文件（可能已被删除或正被独占）");
        return;
    }
    StartLogIndex(log);
}

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
        uint64_t lastCheck = 0;  // the UI thread waits for this worker to stop, so look often
        for (uint64_t line = 0; line < lines; ++line) {
            const void* hit = start < size ? memchr(data + start, '\n', static_cast<size_t>(size - start)) : nullptr;
            const uint64_t next = hit ? static_cast<uint64_t>(static_cast<const char*>(hit) - data) + 1 : size;
            uint64_t end = hit ? next - 1 : size;
            if (end > start && data[end - 1] == '\r') --end;
            if (LogLineMatches(data + start, static_cast<size_t>(end - start), filter, result->endState)) result->view.push_back(static_cast<uint32_t>(line));
            start = next;
            // Checked by lines and by bytes: a file of very long lines would
            // otherwise run for seconds between two checks.
            if (((line & 0x3FFF) == 0 && line) || start - lastCheck > (4u << 20)) {
                lastCheck = start;
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
        ReindexLog(log);
        return;
    }
    const uint64_t oldLines = log.index.Lines(log.size);
    const uint64_t firstChanged = oldLines > 0 && log.size > log.index.indexedEnd ? oldLines - 1 : oldLines;  // a partial line may have grown
    if (!MapLogFile(log)) {
        ReindexLog(log);  // keeps the view consistent and reports the problem
        return;
    }
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
        klass.lpfnWndProc = Guarded<LogViewerProc>;
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
        ReindexLog(*log);
        return true;
    }
    if (key == VK_RETURN && GetFocus() != log->list) { ApplyLogFilterFromControls(*log); return true; }
    return false;
}

