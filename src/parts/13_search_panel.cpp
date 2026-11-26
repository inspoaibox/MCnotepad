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
    WriteDataFile(SearchStatePath(), output.str());
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
    if (file.unlistedHits > 0) text += L"  另有 " + std::to_wstring(file.unlistedHits) + L" 处未列出";
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
    std::replace(selected.begin(), selected.end(), L'\r', L'\n');
    // The file may have changed since the search (or a regex needs the text
    // around the match): only replace when the selection is still exactly a match.
    std::vector<std::pair<size_t, size_t>> check;
    try {
        g_search.lastMatcher->Find(selected, check);
    } catch (const std::exception&) {
        check.clear();
    }
    if (selected.empty() || check.size() != 1 || check.front().first != 0 || check.front().second != selected.size()) {
        ShowNotice(L"此处内容和搜索时不同（或正则需要上下文才能匹配），未替换；请重新搜索后再试");
        return;
    }
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

