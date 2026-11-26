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
    if (compare.docs[target]->LosesComments() && !compare.sides[target].dirty) {
        const std::wstring question = std::wstring(L"目标一侧（") + (toRight ? L"右侧" : L"左侧") +
            L"）的 JSON 含有注释。采用后这一侧会按原缩进重新排版，注释会被删除。\n\n确定继续吗？";
        if (MessageBoxW(compare.window, question.c_str(), kWindowTitle, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    }
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

// Before the app exits: compare windows with merged but unsaved sides ask
// to save (they have no owner, so they would otherwise just disappear).
bool ConfirmCloseCompareWindows() {
    for (HWND window : std::vector<HWND>(g_toolWindows)) {
        if (!IsWindow(window)) continue;
        wchar_t klass[64]{};
        GetClassNameW(window, klass, 64);
        if (wcscmp(klass, L"MCnotepadCompare") != 0) continue;
        auto* compare = reinterpret_cast<CompareWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (!compare || (!compare->sides[0].dirty && !compare->sides[1].dirty)) continue;
        ShowWindow(window, SW_SHOWNORMAL);
        SetForegroundWindow(window);
        if (!ConfirmDiscardCompare(*compare)) return false;
        compare->sides[0].dirty = compare->sides[1].dirty = false;
    }
    return true;
}

void OpenCompareWindow(CompareSide left, CompareSide right, bool structure = false) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{};
        klass.hInstance = g_app.instance;
        klass.lpfnWndProc = Guarded<CompareWindowProc>;
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

