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
        klass.lpfnWndProc = Guarded<GotoProc>;
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

    // Interpreters are started by full path from the system folder, never
    // found by name: a cmd.exe or powershell.exe next to an opened file (or
    // in the current folder) must not be the one that runs.
    wchar_t systemFolder[MAX_PATH]{};
    GetSystemDirectoryW(systemFolder, MAX_PATH);
    const std::wstring cmd = std::wstring(systemFolder) + L"\\cmd.exe";
    const std::wstring powershell = std::wstring(systemFolder) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    const std::wstring quoted = L"\"" + path.wstring() + L"\"";
    std::wstring application;
    std::wstring command;
    if (extension == L".bat" || extension == L".cmd") { application = cmd; command = L"\"" + cmd + L"\" /k call " + quoted; }
    else if (extension == L".ps1") { application = powershell; command = L"\"" + powershell + L"\" -NoLogo -NoExit -File " + quoted; }
    else if (extension == L".py" || extension == L".pyw") { application = cmd; command = L"\"" + cmd + L"\" /k python " + quoted; }
    else if (extension == L".js" || extension == L".mjs" || extension == L".cjs") { application = cmd; command = L"\"" + cmd + L"\" /k node " + quoted; }
    if (command.empty()) {
        MessageBoxW(g_app.window,
            L"暂不支持运行这种类型的文件。\n支持：.bat .cmd .ps1 .py .js，以及在浏览器中打开 .html。",
            kWindowTitle, MB_ICONINFORMATION);
        return;
    }
    if (application == cmd && path.wstring().find(L'%') != std::wstring::npos) {
        MessageBoxW(g_app.window, L"文件路径中含有 %，命令提示符会把它当作变量展开而运行错误的文件。\n请重命名文件或所在文件夹后再运行。",
            kWindowTitle, MB_ICONWARNING);
        return;
    }
    // python / node are looked up through PATH only, not in the script's folder.
    SetEnvironmentVariableW(L"NoDefaultCurrentDirectoryInExePath", L"1");

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(application.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE,
            nullptr, directory.empty() ? nullptr : directory.c_str(), &startup, &process)) {
        MessageBoxW(g_app.window, L"启动失败，请确认对应的解释器已安装并在 PATH 中。", kWindowTitle, MB_ICONERROR);
        return;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}

