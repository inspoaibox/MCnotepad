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
    HWND beforeView = nullptr;
    HWND afterView = nullptr;
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
        if (!state || !state->beforeView || !state->afterView) break;
        const int width = LOWORD(lParam);
        const int height = HIWORD(lParam);
        const int bottom = height - Scale(48);
        MoveWindow(GetDlgItem(window, 100), Scale(16), Scale(12), width - Scale(32), Scale(22), TRUE);
        const int paneTop = Scale(64);
        const int paneHeight = std::max(0, bottom - paneTop);
        const int paneWidth = std::max(0, (width - Scale(40)) / 2);
        MoveWindow(GetDlgItem(window, 101), Scale(16), Scale(40), paneWidth, Scale(20), TRUE);
        MoveWindow(GetDlgItem(window, 102), Scale(24) + paneWidth, Scale(40), paneWidth, Scale(20), TRUE);
        MoveWindow(state->beforeView, Scale(16), paneTop, paneWidth, paneHeight, TRUE);
        MoveWindow(state->afterView, Scale(24) + paneWidth, paneTop, paneWidth, paneHeight, TRUE);
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

void FillPreviewText(HWND view, const std::wstring& text) {
    if (!view) return;
    const ThemePalette& palette = CurrentPalette();
    SendMessageW(view, WM_SETREDRAW, FALSE, 0);
    SendMessageW(view, EM_SETBKGNDCOLOR, 0, palette.editorBackground);
    SetWindowTextW(view, text.c_str());
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR | CFM_BACKCOLOR;
    format.crTextColor = palette.editorText;
    format.crBackColor = palette.editorBackground;
    SendMessageW(view, EM_SETSEL, 0, -1);
    SendMessageW(view, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    SendMessageW(view, EM_SETSEL, 0, 0);
    SendMessageW(view, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(view, nullptr, TRUE);
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
        klass.lpfnWndProc = Guarded<PreviewProc>;
        klass.lpszClassName = L"MCnotepadPreview";
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        registered = RegisterClassW(&klass) != 0;
    }
    PreviewState state;
    RECT owner{};
    GetWindowRect(g_app.window, &owner);
    const int width = std::min(Scale(1040), static_cast<int>(owner.right - owner.left) - Scale(40));
    const int height = std::min(Scale(560), static_cast<int>(owner.bottom - owner.top) - Scale(40));
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"MCnotepadPreview",
        (std::wstring(L"预览更改 - ") + title).c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        owner.left + (owner.right - owner.left - width) / 2, owner.top + (owner.bottom - owner.top - height) / 2,
        width, height, g_app.window, nullptr, g_app.instance, &state);
    if (!dialog) return true;
    ApplyWindowFrameTheme(dialog);

    HWND label = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
        dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(100)), g_app.instance, nullptr);
    HWND beforeLabel = CreateWindowExW(0, L"STATIC", L"原文", WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(101)), g_app.instance, nullptr);
    HWND afterLabel = CreateWindowExW(0, L"STATIC", L"处理结果（预览）", WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(102)), g_app.instance, nullptr);
    state.beforeView = CreateWindowExW(WS_EX_CLIENTEDGE, L"RICHEDIT50W", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        0, 0, 0, 0, dialog, nullptr, g_app.instance, nullptr);
    state.afterView = CreateWindowExW(WS_EX_CLIENTEDGE, L"RICHEDIT50W", nullptr,
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
    for (HWND control : {label, beforeLabel, afterLabel, state.dontAsk, buttons[0], buttons[1], buttons[2], cancel}) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
    }
    SendMessageW(state.beforeView, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
    SendMessageW(state.afterView, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
    ThemeCheckbox(state.dontAsk);
    ThemePushButtons({buttons[0], buttons[1], buttons[2], cancel});
    SetWindowTheme(state.beforeView, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    SetWindowTheme(state.afterView, g_app.darkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    for (HWND view : {state.beforeView, state.afterView}) {
        SendMessageW(view, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(Scale(6), 0));
        SendMessageW(view, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
    }
    int removed = 0, added = 0;
    const std::vector<DiffLine> diff = DiffLines(SplitLines(before), SplitLines(after));
    for (const DiffLine& line : diff) {
        if (line.kind == DiffKind::Removed) ++removed;
        if (line.kind == DiffKind::Added) ++added;
    }
    FillPreviewText(state.beforeView, before);
    FillPreviewText(state.afterView, after);
    SetWindowTextW(label, (L"与原文相比：删除 " + std::to_wstring(removed) + L" 行，新增 " + std::to_wstring(added) +
        L" 行。请选择如何使用处理结果（替换原文后可按 Ctrl+Z 撤销）。").c_str());

    RECT client{};
    GetClientRect(dialog, &client);
    SendMessageW(dialog, WM_SIZE, 0, MAKELPARAM(client.right, client.bottom));
    EnableWindow(g_app.window, FALSE);
    ShowWindow(dialog, SW_SHOW);
    SetFocus(apply);
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

    // `group` picks the capture that holds the value when the pattern also
    // has to consume a guard character before it.
    const auto collect = [&](const wchar_t* pattern, bool unique, int group = 0) {
        std::unordered_map<std::wstring, bool> seen;
        try {
            const std::wregex expression(pattern, std::regex_constants::ECMAScript | std::regex_constants::icase);
            for (std::wsregex_iterator it(text.begin(), text.end(), expression), end; it != end; ++it) {
                std::wstring value = it->str(group);
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
        collect(LR"((?:^|[^\d.])((?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(?:\.(?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3})(?![\d.]))", true, 1);
        break;
    case kIdExtractNumbers:
        what = L"数字";
        // Whole dotted tokens first, so IPs and version numbers aren't split into "numbers".
        // A minus sign only right after a non-digit, so "2024-01-05" gives 2024, 01, 05.
        collect(LR"((?:^|[^\d.])(-?\d+(?:\.\d+)*))", false, 1);
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
        const size_t width = static_cast<size_t>(std::max(1, _wtoi(g_wrapWidth.c_str())));
        std::vector<std::wstring> wrapped;
        for (const std::wstring& line : lines) WrapLineByWidth(line, width, wrapped, count);
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

