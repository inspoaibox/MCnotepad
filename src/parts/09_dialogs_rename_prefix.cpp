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
        klass.lpfnWndProc = Guarded<PromptProc>;
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
        // Suffixes are anchored at the end of the line, so "\d+" on "a1b2"
        // finds the trailing "2" rather than giving up on the first "1".
        const std::wstring source = prefix ? std::wstring(pattern) : L"(?:" + std::wstring(pattern) + L")$";
        // The live preview calls this for every line: compile once per pattern.
        static std::wstring cachedSource;
        static std::regex_constants::syntax_option_type cachedFlags{};
        static std::unique_ptr<std::wregex> cached;
        if (!cached || cachedSource != source || cachedFlags != flags) {
            cached = std::make_unique<std::wregex>(source, flags);
            cachedSource = source;
            cachedFlags = flags;
        }
        const std::wregex& expression = *cached;
        const std::wstring value(line);
        std::wsmatch match;
        if (prefix) {
            if (!std::regex_search(value.cbegin(), value.cend(), match, expression, std::regex_constants::match_continuous)) return true;
        } else {
            if (!std::regex_search(value.cbegin(), value.cend(), match, expression)) return true;
        }
        result = {true, static_cast<size_t>(match.position()), static_cast<size_t>(match.length())};
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
    WriteDataFile(PrefixSuffixHistoryPath(), output.str());
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
        { const std::wstring loaded = GetEditorRawText(document.editor); document.textHash = HashText(loaded); document.logicalLineCount = CountRealLines(loaded); }
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
        WNDCLASSW klass{}; klass.hInstance = g_app.instance; klass.lpfnWndProc = Guarded<PrefixSuffixDialogProc>; klass.lpszClassName = L"MCnotepadPrefixSuffix"; klass.hCursor = LoadCursorW(nullptr, IDC_ARROW); registered = RegisterClassW(&klass) != 0;
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
    ModalScope modalScope;
    while (!state.done && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (message.message == WM_QUIT) PostQuitMessage(static_cast<int>(message.wParam));
    EnableWindow(g_app.window, TRUE); DestroyWindow(dialog); SetForegroundWindow(g_app.window);
    if (state.accepted) PrefixSuffixApply(state);
    if (TabDocument* document = ActiveDocument()) SetFocus(document->editor);
}

