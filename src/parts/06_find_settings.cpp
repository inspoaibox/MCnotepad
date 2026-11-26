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
    while (!value.empty() && iswspace(value.front())) value.erase(value.begin());
    while (!value.empty() && iswspace(value.back())) value.pop_back();
    modifiers = 0;
    key = 0;
    if (value.empty()) return true;
    modifiers = FVIRTKEY;
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
    else if (token == L"UP") key = VK_UP;
    else if (token == L"DOWN") key = VK_DOWN;
    else if (token == L"LEFT") key = VK_LEFT;
    else if (token == L"RIGHT") key = VK_RIGHT;
    else if (token == L"BACK" || token == L"BACKSPACE") key = VK_BACK;
    else if (token == L"DEL" || token == L"DELETE") key = VK_DELETE;
    else if (token == L"HOME") key = VK_HOME;
    else if (token == L"END") key = VK_END;
    else if (token == L"PAGEUP" || token == L"PGUP") key = VK_PRIOR;
    else if (token == L"PAGEDOWN" || token == L"PGDN") key = VK_NEXT;
    else if (token == L"INSERT" || token == L"INS") key = VK_INSERT;
    else return false;
    if ((modifiers & (FCONTROL | FSHIFT | FALT)) != 0) return true;
    return key >= VK_F1 && key <= VK_F24;
}

int ShortcutPageForLabel(std::wstring_view label) {
    const size_t separator = label.find(L'：');
    const std::wstring_view category = label.substr(0, separator);
    if (category == L"文件") return 0;
    if (category == L"编辑" || category == L"文本") return 1;
    if (category == L"标签") return 2;
    if (category == L"搜索") return 3;
    if (category == L"视图" || category == L"编码" || category == L"换行符" || category == L"语言") return 4;
    if (category == L"工具" || category == L"运行") return 5;
    if (category == L"开发者") return 6;
    if (category == L"数据") return 7;
    if (category == L"设置") return 8;
    return 9;
}

void LayoutShortcutDialog(HWND window) {
    if (!window) return;
    constexpr int kTop = 72;
    constexpr int kBottomArea = 58;
    RECT client{};
    GetClientRect(window, &client);
    const int clientHeight = MulDiv(static_cast<int>(client.bottom), 96, static_cast<int>(g_app.dpi));
    const int viewportBottom = std::max(kTop + 1, clientHeight - kBottomArea);
    const int viewportHeight = viewportBottom - kTop;
    g_shortcutDialogContentHeight = 0;
    for (const ShortcutDialogControl& control : g_shortcutDialogControls) {
        if (control.page == g_shortcutDialogPage) {
            g_shortcutDialogContentHeight = std::max(g_shortcutDialogContentHeight,
                control.y + control.height);
        }
    }
    const int maxOffset = std::max(0, g_shortcutDialogContentHeight - viewportHeight);
    g_shortcutDialogScrollOffset = std::clamp(g_shortcutDialogScrollOffset, 0, maxOffset);

    SCROLLINFO scroll{};
    scroll.cbSize = sizeof(scroll);
    scroll.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    scroll.nMin = 0;
    scroll.nMax = std::max(0, g_shortcutDialogContentHeight - 1);
    scroll.nPage = static_cast<UINT>(std::max(1, viewportHeight));
    scroll.nPos = g_shortcutDialogScrollOffset;
    SetScrollInfo(window, SB_VERT, &scroll, TRUE);

    for (ShortcutDialogControl& control : g_shortcutDialogControls) {
        if (control.page != g_shortcutDialogPage) {
            ShowWindow(control.window, SW_HIDE);
            continue;
        }
        const int y = kTop + control.y - g_shortcutDialogScrollOffset;
        const bool visible = y + control.height > kTop && y < viewportBottom;
        if (visible) {
            SetWindowPos(control.window, nullptr, Scale(control.x), Scale(y),
                Scale(control.width), Scale(control.height), SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
        } else {
            ShowWindow(control.window, SW_HIDE);
        }
    }
    if (g_shortcutTabs) {
        SetWindowPos(g_shortcutTabs, nullptr, Scale(18), Scale(6), Scale(644), Scale(30),
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
    }
    if (g_shortcutDialogHint) {
        SetWindowPos(g_shortcutDialogHint, nullptr, Scale(24), Scale(42), Scale(620), Scale(24),
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
    }
    const int buttonY = Scale(std::max(kTop + 4, clientHeight - 44));
    if (g_shortcutDialogApply) SetWindowPos(g_shortcutDialogApply, nullptr, Scale(350), buttonY, Scale(92), Scale(30), SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
    if (g_shortcutDialogReset) SetWindowPos(g_shortcutDialogReset, nullptr, Scale(450), buttonY, Scale(100), Scale(30), SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
    if (g_shortcutDialogCancel) SetWindowPos(g_shortcutDialogCancel, nullptr, Scale(558), buttonY, Scale(92), Scale(30), SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
}

LRESULT CALLBACK ShortcutDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_shortcutEditControls.clear();
        g_shortcutDialogControls.clear();
        g_shortcutDialogScrollOffset = 0;
        g_shortcutDialogPage = 0;
        constexpr int kTop = 72;
        constexpr int kRowHeight = 28;
        std::vector<int> rowByPage(kShortcutPageCount, 0);
        g_shortcutTabs = CreateWindowExW(0, WC_TABCONTROLW, nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(18), Scale(6), Scale(644), Scale(30), window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdShortcutTabs)), g_app.instance, nullptr);
        if (g_shortcutTabs) {
            SendMessageW(g_shortcutTabs, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            for (int page = 0; page < kShortcutPageCount; ++page) {
                TCITEMW item{};
                item.mask = TCIF_TEXT;
                item.pszText = const_cast<wchar_t*>(kShortcutPageNames[page]);
                TabCtrl_InsertItem(g_shortcutTabs, page, &item);
            }
            TabCtrl_SetCurSel(g_shortcutTabs, g_shortcutDialogPage);
        }
        for (size_t index = 0; index < g_shortcuts.size(); ++index) {
            const ShortcutBinding& binding = g_shortcuts[index];
            const int page = std::clamp(ShortcutPageForLabel(binding.label), 0, kShortcutPageCount - 1);
            const int rowY = rowByPage[page]++ * kRowHeight;
            HWND label = CreateWindowExW(0, L"STATIC", binding.label.c_str(),
                WS_CHILD | WS_VISIBLE, Scale(24), Scale(kTop + rowY + 3), Scale(270), Scale(22),
                window, nullptr, g_app.instance, nullptr);
            HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", binding.text.c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                Scale(310), Scale(kTop + rowY), Scale(300), Scale(24), window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdShortcutEditFirst + index)),
                g_app.instance, nullptr);
            SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            if (g_app.darkTheme) SetWindowTheme(edit, L"DarkMode_Explorer", nullptr);
            g_shortcutEditControls.push_back(edit);
            g_shortcutDialogControls.push_back({label, 24, rowY + 3, 270, 22, page});
            g_shortcutDialogControls.push_back({edit, 310, rowY, 300, 24, page});
        }
        g_shortcutDialogHint = CreateWindowExW(0, L"STATIC",
            L"按主菜单分类设置快捷键；支持 Ctrl/Shift/Alt、字母、数字、F1-F24 和方向键，留空表示取消。",
            WS_CHILD | WS_VISIBLE, Scale(24), Scale(42), Scale(620), Scale(24),
            window, nullptr, g_app.instance, nullptr);
        SendMessageW(g_shortcutDialogHint, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        g_shortcutDialogApply = CreateWindowExW(0, L"BUTTON", L"应用",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            Scale(350), Scale(550), Scale(92), Scale(30), window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdShortcutApply)), g_app.instance, nullptr);
        g_shortcutDialogReset = CreateWindowExW(0, L"BUTTON", L"恢复默认",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(450), Scale(550), Scale(100), Scale(30), window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdShortcutReset)), g_app.instance, nullptr);
        g_shortcutDialogCancel = CreateWindowExW(0, L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(558), Scale(550), Scale(92), Scale(30), window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdShortcutCancel)), g_app.instance, nullptr);
        for (HWND button : {g_shortcutDialogApply, g_shortcutDialogReset, g_shortcutDialogCancel})
            SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
        ThemePushButtons({g_shortcutDialogApply, g_shortcutDialogReset, g_shortcutDialogCancel});
        LayoutShortcutDialog(window);
        return 0;
    }
    case WM_SIZE:
        LayoutShortcutDialog(window);
        return 0;
    case WM_VSCROLL: {
        const int action = LOWORD(wParam);
        RECT client{};
        GetClientRect(window, &client);
        constexpr int kTop = 42;
        constexpr int kBottomArea = 58;
        const int clientHeight = MulDiv(static_cast<int>(client.bottom), 96, static_cast<int>(g_app.dpi));
        const int viewportHeight = std::max(1, clientHeight - kBottomArea - kTop);
        switch (action) {
        case SB_LINEUP: g_shortcutDialogScrollOffset -= 28; break;
        case SB_LINEDOWN: g_shortcutDialogScrollOffset += 28; break;
        case SB_PAGEUP: g_shortcutDialogScrollOffset -= viewportHeight; break;
        case SB_PAGEDOWN: g_shortcutDialogScrollOffset += viewportHeight; break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK: g_shortcutDialogScrollOffset = HIWORD(wParam); break;
        case SB_TOP: g_shortcutDialogScrollOffset = 0; break;
        case SB_BOTTOM: g_shortcutDialogScrollOffset = g_shortcutDialogContentHeight; break;
        default: break;
        }
        LayoutShortcutDialog(window);
        return 0;
    }
    case WM_MOUSEWHEEL:
        g_shortcutDialogScrollOffset -= GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 56;
        LayoutShortcutDialog(window);
        return 0;
    case WM_NOTIFY: {
        const NMHDR* header = reinterpret_cast<const NMHDR*>(lParam);
        if (header != nullptr && header->hwndFrom == g_shortcutTabs && header->code == TCN_SELCHANGE) {
            g_shortcutDialogPage = std::clamp(TabCtrl_GetCurSel(g_shortcutTabs), 0, kShortcutPageCount - 1);
            g_shortcutDialogScrollOffset = 0;
            LayoutShortcutDialog(window);
            return 0;
        }
        break;
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
                if (modifiers != 0 && std::find(parsed.begin(), parsed.end(), std::make_pair(modifiers, key)) != parsed.end()) {
                    MessageBoxW(window, L"快捷键不能重复，请修改后再应用。", kWindowTitle, MB_ICONWARNING);
                    SetFocus(g_shortcutEditControls[index]);
                    return 0;
                }
                if (modifiers != 0) parsed.emplace_back(modifiers, key);
                values.push_back(std::move(value));
            }
            for (size_t index = 0; index < values.size() && index < g_shortcuts.size(); ++index) g_shortcuts[index].text = values[index];
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
        g_shortcutTabs = nullptr;
        g_shortcutEditControls.clear();
        g_shortcutDialogControls.clear();
        g_shortcutDialogHint = nullptr;
        g_shortcutDialogApply = nullptr;
        g_shortcutDialogReset = nullptr;
        g_shortcutDialogCancel = nullptr;
        g_shortcutDialogContentHeight = 0;
        g_shortcutDialogScrollOffset = 0;
        g_shortcutDialogPage = 0;
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
    klass.lpfnWndProc = Guarded<ShortcutDialogProc>;
    klass.lpszClassName = L"MCnotepadShortcuts";
    klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    klass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&klass);
    g_shortcutDialogOwner = g_app.settingsWindow ? g_app.settingsWindow : g_app.window;
    g_shortcutDialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
        klass.lpszClassName, L"MCnotepad 快捷键设置",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VSCROLL | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, Scale(680), Scale(620),
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
    std::vector<std::pair<BYTE, WORD>> used;
    for (const ShortcutBinding& binding : g_shortcuts) {
        BYTE modifiers = 0;
        WORD key = 0;
        if (ParseShortcutText(binding.text, modifiers, key) && modifiers != 0 &&
            std::find(used.begin(), used.end(), std::make_pair(modifiers, key)) == used.end()) {
            entries.push_back({modifiers, key, static_cast<WORD>(binding.command)});
            used.emplace_back(modifiers, key);
        }
    }
    return entries.empty() ? nullptr : CreateAcceleratorTableW(entries.data(), static_cast<int>(entries.size()));
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

void ApplyEditorTypography(HWND editor) {
    if (!editor) return;

    // Keep RichEdit's dual-font fallback so CJK glyphs are rendered by a font
    // that contains them.  Only the automatic size adjustment is disabled;
    // the explicit character height and fixed paragraph line box keep the
    // fallback from changing the visual size or baseline grid.
    const LRESULT languageOptions = SendMessageW(editor, EM_GETLANGOPTIONS, 0, 0);
    SendMessageW(editor, EM_SETLANGOPTIONS, 0,
        (languageOptions | IMF_AUTOFONT | IMF_DUALFONT) &
        ~static_cast<LRESULT>(IMF_AUTOFONTSIZEADJUST));

    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_SIZE;
    format.yHeight = static_cast<LONG>(std::max(1, g_app.fontSize) * 20);
    SendMessageW(editor, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&format));
    SendMessageW(editor, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
}

void ApplyEditorFont() {
    DeleteObject(g_app.editorFont);
    const int dpi = static_cast<int>(g_app.dpi);
    const int fontHeight = -MulDiv(g_app.fontSize, dpi, 72);
    g_app.editorFont = CreateFontW(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN, g_app.fontFamily.c_str());
    for (TabDocument& document : g_app.documents) {
        const SelectionState savedSelection = CaptureSelection(document.editor);
        const LONG firstVisibleLine = static_cast<LONG>(SendMessageW(
            document.editor, EM_GETFIRSTVISIBLELINE, 0, 0));
        const LRESULT wasModified = SendMessageW(document.editor, EM_GETMODIFY, 0, 0);
        EditorEventMute mute(document.editor);
        SendMessageW(document.editor, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
        ApplyEditorTypography(document.editor);
        // WM_SETFONT resets every character's colour (black text on dark
        // themes) and the fixed line height still matches the old size
        // (lines overlapping after Ctrl+wheel): put both back.
        ApplyEditorTheme(document.editor);
        document.bracketMatchStart = -1;  // its background was cleared too
        document.bracketMatchEnd = -1;
        ApplyEditorLineSpacing(document.editor);
        SendMessageW(document.editor, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(Scale(kEditorTextMargin), 0));
        ApplyEditorTopInset(document.editor);  // keeps the top spacing and the margin together
        RestoreSelection(document.editor, savedSelection);
        const LONG nowVisibleLine = static_cast<LONG>(SendMessageW(
            document.editor, EM_GETFIRSTVISIBLELINE, 0, 0));
        if (firstVisibleLine != nowVisibleLine) {
            SendMessageW(document.editor, EM_LINESCROLL, 0, firstVisibleLine - nowVisibleLine);
        }
        SendMessageW(document.editor, EM_SETMODIFY, wasModified ? TRUE : FALSE, 0);
        HighlightDocument(document);  // the font reset above cleared the syntax colours
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

// Font sizes offered in Settings.
constexpr int kSettingsFontSizes[] = {9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 22, 24, 28, 32};

// Fonts for the editor: every installed monospaced font, plus the common
// Chinese UI fonts (proportional, but often preferred for Chinese text).
std::vector<std::wstring> EditorFontChoices() {
    std::vector<std::wstring> fonts;
    HDC dc = GetDC(nullptr);
    LOGFONTW query{};
    query.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesExW(dc, &query, [](const LOGFONTW* font, const TEXTMETRICW*, DWORD type, LPARAM list) -> int {
        auto& out = *reinterpret_cast<std::vector<std::wstring>*>(list);
        const std::wstring name = font->lfFaceName;
        if (name.empty() || name[0] == L'@') return 1;  // vertical variants
        if (type & RASTER_FONTTYPE) return 1;           // old bitmap fonts (Fixedsys, Terminal)
        if (name.ends_with(L"-ExtB") || name.ends_with(L"-ExtG")) return 1;  // rare-character supplements
        static const wchar_t* chinese[] = {L"Microsoft YaHei", L"Microsoft YaHei UI", L"DengXian", L"SimSun", L"NSimSun",
            L"SimHei", L"KaiTi", L"FangSong", L"等线", L"微软雅黑", L"宋体", L"新宋体", L"黑体", L"楷体", L"仿宋"};
        const bool fixed = (font->lfPitchAndFamily & 0x3) == FIXED_PITCH;
        const bool common = std::any_of(std::begin(chinese), std::end(chinese), [&](const wchar_t* c) { return name == c; });
        if ((fixed || common) && std::find(out.begin(), out.end(), name) == out.end()) out.push_back(name);
        return 1;
    }, reinterpret_cast<LPARAM>(&fonts), 0);
    ReleaseDC(nullptr, dc);
    if (std::find(fonts.begin(), fonts.end(), g_app.fontFamily) == fonts.end()) fonts.push_back(g_app.fontFamily);
    std::sort(fonts.begin(), fonts.end(), [](const std::wstring& a, const std::wstring& b) {
        return CompareStringEx(LOCALE_NAME_USER_DEFAULT, NORM_IGNORECASE, a.c_str(), -1, b.c_str(), -1, nullptr, nullptr, 0) == CSTR_LESS_THAN;
    });
    return fonts;
}

LRESULT CALLBACK SettingsProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        auto addLabel = [&](const wchar_t* text, int x, int y, int w, int h) {
            CreateSettingsControl(window, L"STATIC", text, 0, 0, x, y, w, h);
        };
        addLabel(L"默认字体", 26, 20, 120, 24);
        CreateSettingsControl(window, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
            kIdSettingsFont, 160, 16, 260, 320);
        for (const std::wstring& font : EditorFontChoices())
            SendMessageW(g_app.settingsFontCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(font.c_str()));
        {
            const LRESULT fontIndex = SendMessageW(g_app.settingsFontCombo, CB_FINDSTRINGEXACT,
                static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(g_app.fontFamily.c_str()));
            SendMessageW(g_app.settingsFontCombo, CB_SETCURSEL, fontIndex == CB_ERR ? 0 : fontIndex, 0);
        }
        addLabel(L"默认字号", 26, 58, 120, 24);
        CreateSettingsControl(window, L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
            kIdSettingsSize, 160, 54, 120, 320);
        int sizeIndex = 0;
        for (size_t i = 0; i < std::size(kSettingsFontSizes); ++i) {
            SendMessageW(g_app.settingsSizeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(std::to_wstring(kSettingsFontSizes[i]).c_str()));
            if (kSettingsFontSizes[i] <= g_app.defaultFontSize) sizeIndex = static_cast<int>(i);
        }
        SendMessageW(g_app.settingsSizeCombo, CB_SETCURSEL, sizeIndex, 0);
        addLabel(L"（Ctrl+滚轮临时缩放，Ctrl+0 恢复）", 290, 58, 230, 24);
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
            const std::wstring chosenFont = GetWindowTextValue(g_app.settingsFontCombo);
            if (!chosenFont.empty()) g_app.fontFamily = chosenFont;
            const int sizeIndex = static_cast<int>(SendMessageW(g_app.settingsSizeCombo, CB_GETCURSEL, 0, 0));
            if (sizeIndex >= 0 && sizeIndex < static_cast<int>(std::size(kSettingsFontSizes))) {
                g_app.defaultFontSize = kSettingsFontSizes[sizeIndex];
                g_app.fontSize = g_app.defaultFontSize;  // applying the setting also ends any zoom
            }
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
    klass.lpfnWndProc = Guarded<SettingsProc>;
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
    if (HasNulCharacter(text)) {
        MessageBoxW(g_app.window, (L"没有重新加载：" + std::wstring(kNulFileMessage)).c_str(), kWindowTitle, MB_ICONWARNING);
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
    { const std::wstring loaded = GetEditorRawText(document.editor); document.textHash = HashText(loaded); document.logicalLineCount = CountRealLines(loaded); }
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

