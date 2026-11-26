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

struct FolderFileInfo {
    std::wstring relative;  // as named on disk
    uint64_t size = 0;
    uint64_t time = 0;
};

// Keyed by the lower-cased relative path: Windows folders ignore case, so
// "README.md" and "Readme.md" are the same file.
void ListFolderFiles(const std::wstring& root, std::unordered_map<std::wstring, FolderFileInfo>& files) {
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
                const std::wstring relative = full.substr(root.size() + 1);
                files[LowerInvariant(relative)] = {relative, size, time};
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
        std::unordered_map<std::wstring, FolderFileInfo> a, b;
        ListFolderFiles(left, a);
        ListFolderFiles(right, b);
        auto* entries = new std::vector<FolderEntry>();
        for (const auto& [key, info] : a) {
            FolderEntry entry{info.relative, EntryState::LeftOnly, info.size, 0, info.time, 0};
            const auto other = b.find(key);
            if (other != b.end()) {
                entry.rightSize = other->second.size;
                entry.rightTime = other->second.time;
                entry.state = entry.leftSize == entry.rightSize && SameFileContent(left + L"\\" + info.relative, right + L"\\" + other->second.relative)
                    ? EntryState::Same : EntryState::Different;
            }
            entries->push_back(entry);
        }
        for (const auto& [key, info] : b) {
            if (!a.count(key)) entries->push_back({info.relative, EntryState::RightOnly, 0, info.size, 0, info.time});
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

// Deletes a file to the Recycle Bin, so an overwrite can be undone.
bool MoveToRecycleBin(const fs::path& path) {
    std::wstring from = path.wstring();
    from.push_back(L'\0');  // the list is double-NUL terminated
    SHFILEOPSTRUCTW operation{};
    operation.wFunc = FO_DELETE;
    operation.pFrom = from.c_str();
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    return SHFileOperationW(&operation) == 0 && !operation.fAnyOperationsAborted;
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
        L"。目标中的同名文件会先移到回收站，再被覆盖。确定继续吗？";
    if (MessageBoxW(folder.window, prompt.c_str(), kWindowTitle, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    int copied = 0;
    std::wstring failures;
    for (const int index : picked) {
        const std::wstring& relative = folder.entries[static_cast<size_t>(index)].relative;
        const fs::path from = fs::path(folder.roots[toRight ? 0 : 1]) / relative;
        const fs::path to = fs::path(folder.roots[toRight ? 1 : 0]) / relative;
        std::error_code error;
        fs::create_directories(to.parent_path(), error);
        if (fs::exists(to, error) && !MoveToRecycleBin(to)) {
            failures += L"\n• " + relative + L"（无法把原文件移到回收站，未覆盖）";
            continue;
        }
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

