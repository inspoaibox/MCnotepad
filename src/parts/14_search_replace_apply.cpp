// ===========================================================================
// Find / replace in files: preview, apply with backup, undo
// ===========================================================================

struct ReplacePreviewState {
    bool done = false;
    bool accepted = false;
    HWND list = nullptr;
    HWND beforeView = nullptr;
    HWND afterView = nullptr;
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
        if (!state || !state->list || !state->beforeView || !state->afterView) break;
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);
        const int bottom = height - Scale(48);
        const int listWidth = std::clamp(width * 38 / 100, Scale(240), Scale(420));
        MoveWindow(state->summary, Scale(16), Scale(12), width - Scale(32), Scale(22), TRUE);
        MoveWindow(state->list, Scale(16), Scale(40), listWidth, std::max(0, bottom - Scale(48)), TRUE);
        const int previewLeft = Scale(24) + listWidth;
        const int previewWidth = std::max(0, width - listWidth - Scale(40));
        const int paneWidth = std::max(0, (previewWidth - Scale(8)) / 2);
        const int paneTop = Scale(64);
        const int paneHeight = std::max(0, bottom - paneTop);
        MoveWindow(GetDlgItem(window, 203), previewLeft, Scale(40), paneWidth, Scale(20), TRUE);
        MoveWindow(GetDlgItem(window, 204), previewLeft + Scale(8) + paneWidth, Scale(40), paneWidth, Scale(20), TRUE);
        MoveWindow(state->beforeView, previewLeft, paneTop, paneWidth, paneHeight, TRUE);
        MoveWindow(state->afterView, previewLeft + Scale(8) + paneWidth, paneTop, paneWidth, paneHeight, TRUE);
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
                FillPreviewText(state->beforeView, file.before);
                FillPreviewText(state->afterView, file.after);
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
        WriteDataFile(backupDirectory / L"manifest.txt", manifest.str());
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
        klass.lpfnWndProc = Guarded<ReplacePreviewProc>;
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
    HWND beforeLabel = CreateWindowExW(0, L"STATIC", L"原文", WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(203)), g_app.instance, nullptr);
    HWND afterLabel = CreateWindowExW(0, L"STATIC", L"处理结果（预览）", WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(204)), g_app.instance, nullptr);
    state.beforeView = CreateWindowExW(WS_EX_CLIENTEDGE, L"RICHEDIT50W", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, 0, 0, 0, dialog, nullptr, g_app.instance, nullptr);
    state.afterView = CreateWindowExW(WS_EX_CLIENTEDGE, L"RICHEDIT50W", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, 0, 0, 0, dialog, nullptr, g_app.instance, nullptr);
    HWND all = CreateWindowExW(0, L"BUTTON", L"全选", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(201), g_app.instance, nullptr);
    HWND none = CreateWindowExW(0, L"BUTTON", L"全不选", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(202), g_app.instance, nullptr);
    state.backup = CreateWindowExW(0, L"BUTTON", L"替换前备份原文件（可撤销）", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 0, 0, dialog, nullptr, g_app.instance, nullptr);
    HWND apply = CreateWindowExW(0, L"BUTTON", L"替换勾选的文件", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(IDOK), g_app.instance, nullptr);
    HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(IDCANCEL), g_app.instance, nullptr);
    for (HWND control : {state.summary, state.list, beforeLabel, afterLabel, all, none, state.backup, apply, cancel}) {
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
    for (HWND view : {state.beforeView, state.afterView}) {
        SendMessageW(view, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
        SetWindowTheme(view, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        SendMessageW(view, EM_SETBKGNDCOLOR, 0, palette.editorBackground);
        SendMessageW(view, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
        SendMessageW(view, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(Scale(6), 0));
    }

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
    ModalScope modalScope;
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

