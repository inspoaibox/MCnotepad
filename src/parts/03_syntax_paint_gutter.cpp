// ---- Applying colours ------------------------------------------------------------------

bool SyntaxEnabled(const TabDocument& document) {
    return EffectiveLanguage(document) != kLanguagePlain && GetWindowTextLengthW(document.editor) <= static_cast<int>(kSyntaxSizeLimit);
}

void RetokenizeDocument(TabDocument& document) {
    document.syntaxStale = false;
    // Ranges painted before the edit sit at shifted positions now and can't
    // be tracked: the next paint clears colours outside the view once.
    if (!document.syntaxColored.empty()) document.syntaxResetAll = true;
    document.syntaxColored.clear();
    document.syntaxTokens.clear();
    std::wstring storage;
    const std::wstring& text = DocumentText(document, storage);
    if (document.untitled && document.language == kLanguageAuto && document.path.empty()) {
        document.sniffedLanguage = SniffLanguage(text);  // a new tab follows what is typed
    }
    if (!SyntaxEnabled(document)) return;
    SyntaxLexer(text, document.syntaxTokens).Run(EffectiveLanguage(document), 0, text.size());
    std::sort(document.syntaxTokens.begin(), document.syntaxTokens.end(),
        [](const SyntaxToken& a, const SyntaxToken& b) { return a.start < b.start; });
}

struct StyleFonts {
    TextDocumentRef& tom;
    ITextFont* fonts[32]{};
    explicit StyleFonts(TextDocumentRef& ref) : tom(ref) {}
    ~StyleFonts() { for (ITextFont* font : fonts) if (font) font->Release(); }
    StyleFonts(const StyleFonts&) = delete;
    StyleFonts& operator=(const StyleFonts&) = delete;
    ITextFont* For(Tok kind) {
        ITextFont*& font = fonts[static_cast<size_t>(kind) & 31];
        if (!font) {
            const SyntaxStyle style = StyleFor(kind);
            font = tom.MakeStyleFont(style.color, style.bold);
        }
        return font;
    }
};

void PaintSyntaxRange(TabDocument& document, TextDocumentRef& tom, StyleFonts& fonts, LONG from, LONG to) {
    tom.ApplyFont(from, to, fonts.For(Tok::Plain));
    const auto& tokens = document.syntaxTokens;
    auto it = std::lower_bound(tokens.begin(), tokens.end(), static_cast<uint32_t>(from),
        [](const SyntaxToken& token, uint32_t value) { return token.start < value; });
    if (it != tokens.begin() && (it - 1)->start + (it - 1)->length > static_cast<uint32_t>(from)) --it;
    for (; it != tokens.end() && static_cast<LONG>(it->start) < to; ++it) {
        const LONG start = std::max(from, static_cast<LONG>(it->start));
        const LONG end = std::min(to, static_cast<LONG>(it->start + it->length));
        if (end <= start) continue;
        tom.ApplyFont(start, end, fonts.For(it->kind));
    }
}

// RichEdit keeps character formatting in one array of runs; every coloured
// token adds runs, and each insert gets slower as the array grows. Keep the
// painted area near the view and return far-away text to plain (which merges
// its runs back); it is repainted if the user scrolls back.
constexpr LONG kMaxPaintedChars = 60000;

// Colours whatever part of the visible text (plus a margin) is not coloured yet.
// `retokenize` is false for scrolling and key presses: lexing the whole text
// on every keystroke is what the typing-pause timer is there to avoid.
void SyntaxColorVisible(TabDocument& document, bool retokenize) {
    HWND editor = document.editor;
    if (!editor || !IsWindowVisible(editor)) return;
    if (document.syntaxStale) {
        if (!retokenize) return;  // tokens don't match the text; the timer re-lexes shortly
        RetokenizeDocument(document);
    }
    if (document.syntaxTokens.empty() && !document.syntaxHadColors && !document.syntaxResetAll) return;
    const LONG length = EditorTextLength(editor);  // exact and O(1); WM_GETTEXTLENGTH walks the text
    RECT client{};
    GetClientRect(editor, &client);
    POINTL topLeft{0, 0}, bottomRight{client.right, client.bottom};
    LONG from = static_cast<LONG>(SendMessageW(editor, EM_CHARFROMPOS, 0, reinterpret_cast<LPARAM>(&topLeft)));
    LONG to = static_cast<LONG>(SendMessageW(editor, EM_CHARFROMPOS, 0, reinterpret_cast<LPARAM>(&bottomRight)));
    from = std::max(0L, from - 2000);
    to = std::min(length + 1, to + 2000);
    // Gaps between already coloured intervals.
    std::vector<std::pair<LONG, LONG>> gaps;
    LONG cursor = from;
    for (const auto& [start, end] : document.syntaxColored) {
        if (end <= cursor) continue;
        if (start >= to) break;
        if (start > cursor) gaps.emplace_back(cursor, std::min(start, to));
        cursor = std::max(cursor, end);
    }
    if (cursor < to) gaps.emplace_back(cursor, to);
    if (gaps.empty() && !document.syntaxResetAll) return;

    EditorEventMute mute(editor);
    TextDocumentRef tom(editor);
    if (!tom) return;
    long freezeCount = 0;
    tom.document()->Undo(tomSuspend, nullptr);
    tom.document()->Freeze(&freezeCount);
    StyleFonts fonts(tom);
    if (document.syntaxResetAll) {
        // One pass back to plain for everything outside the view, so the run
        // count stays bounded and no stale colours remain after a language switch.
        document.syntaxResetAll = false;
        if (from > 0) tom.ApplyFont(0, from, fonts.For(Tok::Plain));
        if (to < length + 1) tom.ApplyFont(to, length + 1, fonts.For(Tok::Plain));
    }
    LONG painted = 0;
    for (const auto& [start, end] : document.syntaxColored) painted += end - start;
    if (painted + (to - from) > kMaxPaintedChars) {
        std::vector<std::pair<LONG, LONG>> kept;
        for (const auto& interval : document.syntaxColored) {
            if (interval.second < from || interval.first > to) tom.ApplyFont(interval.first, interval.second, fonts.For(Tok::Plain));
            else kept.push_back(interval);
        }
        document.syntaxColored.swap(kept);
    }
    for (const auto& [start, end] : gaps) PaintSyntaxRange(document, tom, fonts, start, end);
    tom.document()->Unfreeze(&freezeCount);
    tom.document()->Undo(tomResume, nullptr);
    document.syntaxHadColors = !document.syntaxTokens.empty();

    document.syntaxColored.emplace_back(from, to);
    std::sort(document.syntaxColored.begin(), document.syntaxColored.end());
    std::vector<std::pair<LONG, LONG>> merged;
    for (const auto& interval : document.syntaxColored) {
        if (!merged.empty() && interval.first <= merged.back().second) merged.back().second = std::max(merged.back().second, interval.second);
        else merged.push_back(interval);
    }
    document.syntaxColored.swap(merged);
}

// Text, language or theme changed: re-tokenize and recolour what is visible;
// other tabs recolour when they are shown.
void HighlightDocument(TabDocument& document) {
    document.syntaxStale = true;
    if (!document.syntaxColored.empty() || document.syntaxHadColors) document.syntaxResetAll = true;
    document.syntaxColored.clear();
    if (document.syntaxHadColors && !SyntaxEnabled(document)) {
        // Switched to plain text: drop old colours once.
        EditorEventMute mute(document.editor);
        TextDocumentRef tom(document.editor);
        if (tom) {
            tom.document()->Undo(tomSuspend, nullptr);
            tom.SetStyle(0, GetWindowTextLengthW(document.editor) + 1, StyleFor(Tok::Plain).color, false);
            tom.document()->Undo(tomResume, nullptr);
        }
        document.syntaxHadColors = false;
    }
    SyntaxColorVisible(document);
}

void HighlightDocument(const TabDocument& document) {
    HighlightDocument(const_cast<TabDocument&>(document));
}

// Called after the editor scrolled or resized.
void SyntaxOnViewChanged(HWND editor) {
    for (TabDocument& document : g_app.documents) {
        if (document.editor == editor) {
            SyntaxColorVisible(document, false);
            return;
        }
    }
}

COLORREF CurrentLineHighlightColor() {
    const ThemePalette& palette = CurrentPalette();
    // Keep the current line close to the editor surface so it is visible
    // without competing with syntax colours or selected text.
    const int amount = g_app.darkTheme ? 12 : 8;
    const auto blend = [amount](BYTE a, BYTE b) -> BYTE {
        return static_cast<BYTE>(std::clamp(static_cast<int>(a) +
            (static_cast<int>(b) - static_cast<int>(a)) * amount / 100, 0, 255));
    };
    return RGB(blend(GetRValue(palette.editorBackground), GetRValue(palette.accent)),
        blend(GetGValue(palette.editorBackground), GetGValue(palette.accent)),
        blend(GetBValue(palette.editorBackground), GetBValue(palette.accent)));
}

bool IsEditorSelectionDragActive(HWND editor) {
    // EN_SELCHANGE is delivered by the editor itself, so the button state is
    // sufficient and also covers RichEdit builds that capture the parent
    // window rather than the child control during a drag.
    return editor != nullptr && (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
}

bool IsEditorImeComposing(HWND editor) {
    return editor != nullptr && g_imeComposingEditors.find(editor) != g_imeComposingEditors.end();
}

// The selection including which end the caret is on. EM_EXGETSEL/EXSETSEL
// lose that: restoring a selection made upwards (Shift+Up) would move the
// caret to its bottom end and the next Shift+Up would shrink it instead.
struct SelectionState {
    CHARRANGE range{};
    bool caretAtStart = false;
};

SelectionState CaptureSelection(HWND editor) {
    SelectionState state;
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&state.range));
    if (state.range.cpMin == state.range.cpMax) return state;
    TextDocumentRef tom(editor);
    ITextSelection* selection = nullptr;
    if (tom && SUCCEEDED(tom.document()->GetSelection(&selection)) && selection) {
        long flags = 0;
        if (SUCCEEDED(selection->GetFlags(&flags))) state.caretAtStart = (flags & tomSelStartActive) != 0;
        selection->Release();
    }
    return state;
}

// Puts the selection back only if something moved it, keeping its direction.
void RestoreSelection(HWND editor, const SelectionState& state) {
    CHARRANGE now{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&now));
    if (now.cpMin == state.range.cpMin && now.cpMax == state.range.cpMax) return;
    if (state.caretAtStart) SendMessageW(editor, EM_SETSEL, state.range.cpMax, state.range.cpMin);
    else SendMessageW(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&state.range));
}

void ApplyCurrentLineHighlight(TabDocument& document, bool force) {
    (void)force;
    HWND editor = document.editor;
    if (!editor || !IsWindow(editor) || !IsWindowVisible(editor)) return;
    // Do not paint the active line into RichEdit character backgrounds. The
    // TOM `tomLine` range can expand across wrapped paragraphs and leave
    // large blocks of text tinted, which looks like accidental selection and
    // also interferes with IME composition. Keep the editor surface plain.
    if (document.highlightedLine <= 0) return;
    TextDocumentRef tom(editor);
    if (!tom) return;
    const LONG length = EditorTextLength(editor);
    const LONG start = std::clamp(document.highlightedStart, 0L, length);
    const LONG end = std::clamp(document.highlightedEnd, start, length + 1);
    if (end > start) {
        EditorEventMute mute(editor);
        long freezeCount = 0;
        tom.document()->Undo(tomSuspend, nullptr);
        tom.document()->Freeze(&freezeCount);
        tom.SetBackColor(start, end, EditorBackground());
        tom.document()->Unfreeze(&freezeCount);
        tom.document()->Undo(tomResume, nullptr);
    }
    document.highlightedLine = -1;
    document.highlightedStart = 0;
    document.highlightedEnd = 0;
    document.highlightedLength = length;
    InvalidateRect(editor, nullptr, FALSE);
}

bool IsOpeningBracket(wchar_t character) {
    return character == L'(' || character == L'[' || character == L'{';
}

bool IsClosingBracket(wchar_t character) {
    return character == L')' || character == L']' || character == L'}';
}

wchar_t MatchingBracket(wchar_t character) {
    switch (character) {
    case L'(': return L')';
    case L'[': return L']';
    case L'{': return L'}';
    case L')': return L'(';
    case L']': return L'[';
    case L'}': return L'{';
    default: return 0;
    }
}

LONG EditorTextLength(HWND editor);

// Characters [from, to) exactly as RichEdit stores them (line breaks are \r).
std::wstring EditorTextRange(HWND editor, LONG from, LONG to) {
    if (to <= from) return {};
    std::wstring text(static_cast<size_t>(to - from) + 1, L'\0');
    TEXTRANGEW range{};
    range.chrg = CHARRANGE{from, to};
    range.lpstrText = text.data();
    const LRESULT copied = SendMessageW(editor, EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&range));
    text.resize(static_cast<size_t>(std::clamp<LRESULT>(copied, 0, to - from)));
    return text;
}

LONG FindMatchingBracket(std::wstring_view text, LONG position) {
    if (position < 0 || position >= static_cast<LONG>(text.size())) return -1;
    const wchar_t anchor = text[static_cast<size_t>(position)];
    if (!IsOpeningBracket(anchor) && !IsClosingBracket(anchor)) return -1;
    const wchar_t match = MatchingBracket(anchor);
    const int direction = IsOpeningBracket(anchor) ? 1 : -1;
    int depth = 1;
    bool inString = false;
    wchar_t quote = 0;
    for (LONG index = position + direction;
         index >= 0 && index < static_cast<LONG>(text.size()); index += direction) {
        const wchar_t character = text[static_cast<size_t>(index)];
        const bool escaped = index > 0 && text[static_cast<size_t>(index - 1)] == L'\\' &&
            (index < 2 || text[static_cast<size_t>(index - 2)] != L'\\');
        if (character == L'"' || character == L'\'') {
            if (!escaped && (!inString || character == quote)) {
                inString = !inString;
                quote = inString ? character : 0;
            }
            continue;
        }
        if (inString) continue;
        if (character == anchor) {
            ++depth;
        } else if (character == match) {
            if (--depth == 0) return index;
        }
    }
    return -1;
}

COLORREF BracketMatchHighlightColor() {
    const ThemePalette& palette = CurrentPalette();
    const int amount = g_app.darkTheme ? 34 : 24;
    const auto blend = [amount](BYTE a, BYTE b) -> BYTE {
        return static_cast<BYTE>(std::clamp(static_cast<int>(a) +
            (static_cast<int>(b) - static_cast<int>(a)) * amount / 100, 0, 255));
    };
    return RGB(blend(GetRValue(palette.editorBackground), GetRValue(palette.accent)),
        blend(GetGValue(palette.editorBackground), GetGValue(palette.accent)),
        blend(GetBValue(palette.editorBackground), GetBValue(palette.accent)));
}

void ApplyBracketMatchHighlight(TabDocument& document, bool force) {
    HWND editor = document.editor;
    if (!editor || !IsWindow(editor) || !IsWindowVisible(editor)) return;
    if (IsEditorImeComposing(editor)) return;
    const SelectionState savedSelection = CaptureSelection(editor);
    const CHARRANGE selection = savedSelection.range;
    const LONG length = EditorTextLength(editor);
    const LONG caret = std::clamp(selection.cpMin, 0L, length);
    // Only the two characters at the caret are read first; the text around
    // them (a bounded window, not the whole document) only when one of them
    // is a bracket. This runs on every caret move.
    const std::wstring around = EditorTextRange(editor, std::max(0L, caret - 1), std::min(length, caret + 1));
    const LONG aroundStart = std::max(0L, caret - 1);
    const auto bracketAt = [&](LONG position) {
        const LONG local = position - aroundStart;
        if (local < 0 || local >= static_cast<LONG>(around.size())) return false;
        const wchar_t c = around[static_cast<size_t>(local)];
        return IsOpeningBracket(c) || IsClosingBracket(c);
    };
    LONG anchor = -1;
    if (bracketAt(caret)) anchor = caret;
    else if (caret > 0 && bracketAt(caret - 1)) anchor = caret - 1;
    LONG match = -1;
    if (anchor >= 0) {
        constexpr LONG kBracketWindow = 200000;
        const LONG windowStart = std::max(0L, anchor - kBracketWindow);
        const std::wstring window = EditorTextRange(editor, windowStart, std::min(length, anchor + kBracketWindow));
        const LONG local = FindMatchingBracket(window, anchor - windowStart);
        if (local >= 0) match = windowStart + local;
    }
    if (!force && anchor == document.bracketMatchStart && match == document.bracketMatchEnd) return;

    EditorEventMute mute(editor);
    TextDocumentRef tom(editor);
    if (!tom) return;
    long freezeCount = 0;
    tom.document()->Undo(tomSuspend, nullptr);
    tom.document()->Freeze(&freezeCount);
    if (document.bracketMatchStart >= 0 && document.bracketMatchEnd > document.bracketMatchStart) {
        tom.SetBackColor(document.bracketMatchStart, document.bracketMatchStart + 1, EditorBackground());
        tom.SetBackColor(document.bracketMatchEnd, document.bracketMatchEnd + 1, EditorBackground());
    }
    document.bracketMatchStart = anchor;
    document.bracketMatchEnd = match;
    tom.document()->Unfreeze(&freezeCount);
    tom.document()->Undo(tomResume, nullptr);
    // Restoring the line background after removing the old pair keeps the two
    // highlights composable when a bracket sits on the active line.
    ApplyCurrentLineHighlight(document, true);
    if (anchor >= 0 && match >= 0) {
        TextDocumentRef bracketTom(editor);
        if (bracketTom) {
            long bracketFreeze = 0;
            bracketTom.document()->Undo(tomSuspend, nullptr);
            bracketTom.document()->Freeze(&bracketFreeze);
            bracketTom.SetBackColor(anchor, anchor + 1, BracketMatchHighlightColor());
            bracketTom.SetBackColor(match, match + 1, BracketMatchHighlightColor());
            bracketTom.document()->Unfreeze(&bracketFreeze);
            bracketTom.document()->Undo(tomResume, nullptr);
        }
    }
    RestoreSelection(editor, savedSelection);
    InvalidateRect(editor, nullptr, FALSE);
}

// Re-colouring after typing pauses.
constexpr UINT kTimerHighlight = 4102;
constexpr UINT kHighlightDelayMs = 250;

// Width needed for the active document's line numbers (at least 3 digits).
int GutterWidthFor(HWND editor) {
    const int lines = editor ? static_cast<int>(SendMessageW(editor, EM_GETLINECOUNT, 0, 0)) : 1;
    int digits = 3;
    for (int value = lines; value >= 1000; value /= 10) ++digits;
    HDC dc = GetDC(g_app.window);
    HFONT previous = static_cast<HFONT>(SelectObject(dc, g_app.editorFont ? g_app.editorFont
        : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT))));
    SIZE digit{};
    GetTextExtentPoint32W(dc, L"0", 1, &digit);
    SelectObject(dc, previous);
    ReleaseDC(g_app.window, dc);
    return digits * digit.cx + Scale(20);
}

bool HasBookmark(const TabDocument& document, int line) {
    return std::binary_search(document.bookmarks.begin(), document.bookmarks.end(), line);
}

int CountRealLines(const std::wstring& text) {
    return 1 + static_cast<int>(std::count(text.begin(), text.end(), L'\r'));
}

// Lines were inserted or removed: bookmarks below the edit move with their text.
// `caretLine` is the real line of the caret after the edit (1-based).
void ShiftBookmarksForEdit(TabDocument& document, int newLineCount, int caretLine) {
    const int delta = newLineCount - document.logicalLineCount;
    document.logicalLineCount = newLineCount;
    if (delta == 0 || document.bookmarks.empty()) return;
    if (delta > 0) {
        const int editLine = caretLine - delta;  // inserted lines end at the caret
        for (int& line : document.bookmarks) if (line > editLine) line += delta;
    } else {
        const int removedLast = caretLine - delta;  // lines caretLine+1 .. caretLine-delta are gone
        for (int& line : document.bookmarks) {
            if (line > removedLast) line += delta;
            else if (line > caretLine) line = caretLine;
        }
    }
}

void NormalizeBookmarks(TabDocument& document) {
    const int lineCount = document.editor ? document.logicalLineCount : 1;
    document.bookmarks.erase(std::remove_if(document.bookmarks.begin(), document.bookmarks.end(),
        [lineCount](int line) { return line < 1 || line > lineCount; }), document.bookmarks.end());
    std::sort(document.bookmarks.begin(), document.bookmarks.end());
    document.bookmarks.erase(std::unique(document.bookmarks.begin(), document.bookmarks.end()), document.bookmarks.end());
}

void ToggleBookmarkAtLine(int index, int line) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size()) || line < 1) return;
    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    const auto it = std::lower_bound(document.bookmarks.begin(), document.bookmarks.end(), line);
    if (it != document.bookmarks.end() && *it == line) document.bookmarks.erase(it);
    else document.bookmarks.insert(it, line);
    g_app.stateDirty = true;
    RefreshGutter();
    UpdateStatusBar();
}

void ToggleBookmarkAtCaret() {
    if (g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) return;
    TabDocument& document = g_app.documents[static_cast<size_t>(g_app.activeIndex)];
    CHARRANGE selection{};
    SendMessageW(document.editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    TextDocumentRef tom(document.editor);
    const long line = tom ? tom.ParagraphIndex(selection.cpMin)
                          : static_cast<long>(SendMessageW(document.editor, EM_EXLINEFROMCHAR, 0, selection.cpMin)) + 1;
    ToggleBookmarkAtLine(g_app.activeIndex, static_cast<int>(line));
}

void GotoBookmark(int direction) {
    if (g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) return;
    TabDocument& document = g_app.documents[static_cast<size_t>(g_app.activeIndex)];
    NormalizeBookmarks(document);
    if (document.bookmarks.empty()) {
        ShowNotice(L"当前文档没有书签");
        return;
    }
    CHARRANGE selection{};
    SendMessageW(document.editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    TextDocumentRef tom(document.editor);
    const int current = tom ? static_cast<int>(tom.ParagraphIndex(selection.cpMin))
                            : static_cast<int>(SendMessageW(document.editor, EM_EXLINEFROMCHAR, 0, selection.cpMin)) + 1;
    int target = direction > 0 ? document.bookmarks.front() : document.bookmarks.back();
    if (direction > 0) {
        for (int line : document.bookmarks) if (line > current) { target = line; break; }
    } else {
        for (auto it = document.bookmarks.rbegin(); it != document.bookmarks.rend(); ++it) {
            if (*it < current) { target = *it; break; }
        }
    }
    const LONG position = tom ? tom.StartOfParagraph(target)
                              : static_cast<LONG>(SendMessageW(document.editor, EM_LINEINDEX, target - 1, 0));
    if (position >= 0) {
        SendMessageW(document.editor, EM_SETSEL, position, position);
        SendMessageW(document.editor, EM_SCROLLCARET, 0, 0);
        SetFocus(document.editor);
        RefreshGutter();
        UpdateStatusBar();
    }
}

void ClearBookmarks() {
    if (g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) return;
    TabDocument& document = g_app.documents[static_cast<size_t>(g_app.activeIndex)];
    if (document.bookmarks.empty()) return;
    document.bookmarks.clear();
    g_app.stateDirty = true;
    RefreshGutter();
    ShowNotice(L"已清除当前文档书签");
}

// Numbers are drawn where RichEdit actually places each visible line, so they
// line up for any font size. With word wrap a long line spans several visual
// rows; only its first row gets a number, and numbers count real lines.
LRESULT CALLBACK GutterProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        HBRUSH brush = CreateSolidBrush(GutterBackground());
        FillRect(dc, &client, brush);
        DeleteObject(brush);
        SetBkMode(dc, TRANSPARENT);
        HFONT font = g_app.editorFont ? g_app.editorFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HFONT previous = static_cast<HFONT>(SelectObject(dc, font));
        TEXTMETRICW metrics{};
        GetTextMetricsW(dc, &metrics);

        HWND editor = nullptr;
        if (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size())) {
            editor = g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor;
        }
        if (editor) {
            TabDocument& activeDocument = g_app.documents[static_cast<size_t>(g_app.activeIndex)];
            DetectFoldRegions(activeDocument);
            POINT origin{0, 0};
            MapWindowPoints(editor, window, &origin, 1);  // editor client -> gutter coordinates
            TextDocumentRef tom(editor);
            const auto paragraphOf = [&](LONG position) -> long {
                return tom ? tom.ParagraphIndex(position)
                           : static_cast<long>(SendMessageW(editor, EM_EXLINEFROMCHAR, 0, position)) + 1;
            };
            CHARRANGE selection{};
            SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
            const int lineCount = static_cast<int>(SendMessageW(editor, EM_GETLINECOUNT, 0, 0));
            const int firstVisual = static_cast<int>(SendMessageW(editor, EM_GETFIRSTVISIBLELINE, 0, 0));
            const LONG firstChar = static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, firstVisual, 0));
            // A paragraph number costs RichEdit a walk from the top of the
            // document. Ask once for the first visible line and count line
            // breaks in the visible text for the others.
            RECT editorClient{};
            GetClientRect(editor, &editorClient);
            POINTL bottomRight{editorClient.right, editorClient.bottom};
            const LONG lastChar = static_cast<LONG>(SendMessageW(editor, EM_CHARFROMPOS, 0, reinterpret_cast<LPARAM>(&bottomRight)));
            const LONG bufferStart = std::max(0L, firstChar - 1);
            const std::wstring visibleText = lastChar - bufferStart < 400000 ? EditorTextRange(editor, bufferStart, lastChar + 2) : std::wstring();
            const long firstParagraph = paragraphOf(std::max(0L, firstChar));
            LONG countedTo = std::max(0L, firstChar);
            long countedParagraph = firstParagraph;
            const auto fastParagraphOf = [&](LONG position) -> long {
                const LONG end = bufferStart + static_cast<LONG>(visibleText.size());
                if (visibleText.empty() || position < countedTo || position > end) return paragraphOf(position);
                for (; countedTo < position; ++countedTo) {
                    if (visibleText[static_cast<size_t>(countedTo - bufferStart)] == L'\r') ++countedParagraph;
                }
                return countedParagraph;
            };
            const long caretParagraph = selection.cpMax >= firstChar && selection.cpMax <= lastChar
                ? firstParagraph + static_cast<long>(std::count(visibleText.begin() + std::min<size_t>(visibleText.size(), static_cast<size_t>(std::max(0L, firstChar) - bufferStart)),
                      visibleText.begin() + std::min<size_t>(visibleText.size(), static_cast<size_t>(selection.cpMax - bufferStart)), L'\r'))
                : paragraphOf(selection.cpMax);
            long previousParagraph = firstChar <= 0 ? 0
                : visibleText.empty() ? paragraphOf(firstChar - 1)
                : visibleText[0] == L'\r' ? firstParagraph - 1 : firstParagraph;
            const int right = client.right - Scale(8);
            for (int visual = firstVisual; visual < lineCount; ++visual) {
                const LRESULT start = SendMessageW(editor, EM_LINEINDEX, visual, 0);
                if (start < 0) break;
                POINTL position{};
                SendMessageW(editor, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&position), start);
                if (position.y + origin.y > client.bottom) break;
                const long paragraph = fastParagraphOf(static_cast<LONG>(start));
                if (paragraph != previousParagraph) {
                    // Numbers sit on the line's own text baseline, as RichEdit
                    // lays it out (line spacing, top inset and font fallback all
                    // move it), so they line up with the text at any setting.
                    int baseline = position.y + origin.y + metrics.tmAscent;
                    POINT onScreen{};
                    if (tom && tom.BaselineScreenPoint(static_cast<LONG>(start), onScreen)) {
                        ScreenToClient(window, &onScreen);
                        baseline = onScreen.y;
                    }
                    const int y = baseline - metrics.tmAscent;  // top of the number's cell
                    const auto fold = std::find_if(activeDocument.folds.begin(), activeDocument.folds.end(),
                        [paragraph](const FoldRegion& region) { return region.line + 1 == paragraph; });
                    if (fold != activeDocument.folds.end()) {
                        const int markerX = Scale(5);
                        const int markerY = y + metrics.tmHeight / 2;
                        HPEN foldPen = CreatePen(PS_SOLID, 1, CurrentPalette().gutterText);
                        HBRUSH foldBrush = CreateSolidBrush(GutterBackground());
                        HGDIOBJ oldPen = SelectObject(dc, foldPen);
                        HGDIOBJ oldBrush = SelectObject(dc, foldBrush);
                        POINT arrow[3]{};
                        if (fold->collapsed) {
                            arrow[0] = {markerX - Scale(2), markerY - Scale(4)};
                            arrow[1] = {markerX - Scale(2), markerY + Scale(4)};
                            arrow[2] = {markerX + Scale(3), markerY};
                        } else {
                            arrow[0] = {markerX - Scale(4), markerY - Scale(2)};
                            arrow[1] = {markerX + Scale(4), markerY - Scale(2)};
                            arrow[2] = {markerX, markerY + Scale(3)};
                        }
                        Polygon(dc, arrow, 3);
                        SelectObject(dc, oldBrush);
                        SelectObject(dc, oldPen);
                        DeleteObject(foldBrush);
                        DeleteObject(foldPen);
                    }
                    if (HasBookmark(activeDocument, static_cast<int>(paragraph))) {
                        const int markerX = Scale(17);
                        const int markerY = y + metrics.tmHeight / 2;
                        HBRUSH markerBrush = CreateSolidBrush(CurrentPalette().accent);
                        HPEN markerPen = CreatePen(PS_SOLID, 1, CurrentPalette().accent);
                        HGDIOBJ oldBrush = SelectObject(dc, markerBrush);
                        HGDIOBJ oldPen = SelectObject(dc, markerPen);
                        POINT marker[3]{{markerX, markerY - Scale(5)}, {markerX + Scale(8), markerY}, {markerX, markerY + Scale(5)}};
                        Polygon(dc, marker, 3);
                        SelectObject(dc, oldPen);
                        SelectObject(dc, oldBrush);
                        DeleteObject(markerPen);
                        DeleteObject(markerBrush);
                    }
                    wchar_t number[16]{};
                    swprintf_s(number, L"%ld", paragraph);
                    SetTextColor(dc, paragraph == caretParagraph ? CurrentPalette().tabActiveText : GutterForeground());
                    const UINT previousAlign = SetTextAlign(dc, TA_RIGHT | TA_BASELINE);
                    ExtTextOutW(dc, right, baseline, 0, nullptr, number, static_cast<UINT>(wcslen(number)), nullptr);
                    SetTextAlign(dc, previousAlign);
                }
                previousParagraph = paragraph;
            }
        }
        SelectObject(dc, previous);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_LBUTTONUP: {
        if (g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) return 0;
        HWND editor = g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor;
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const bool foldClick = point.x < Scale(14);
        MapWindowPoints(window, editor, &point, 1);
        const LRESULT character = SendMessageW(editor, EM_CHARFROMPOS, 0, reinterpret_cast<LPARAM>(&point));
        const LONG position = static_cast<LONG>(character);
        TextDocumentRef tom(editor);
        const int line = position < 0 ? 1 : tom ? static_cast<int>(tom.ParagraphIndex(position))
                                                : static_cast<int>(SendMessageW(editor, EM_EXLINEFROMCHAR, 0, position)) + 1;
        if (foldClick) {
            TabDocument& document = g_app.documents[static_cast<size_t>(g_app.activeIndex)];
            DetectFoldRegions(document);
            auto it = std::find_if(document.folds.begin(), document.folds.end(), [line](const FoldRegion& region) {
                return region.line + 1 == line;
            });
            if (it != document.folds.end()) {
                it->collapsed = !it->collapsed;
                ApplyFoldVisibility(document);
                g_app.stateDirty = true;
                SetFocus(editor);
                return 0;
            }
        }
        ToggleBookmarkAtLine(g_app.activeIndex, line);
        SetFocus(editor);
        return 0;
    }
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

void DrawToolbarIcon(HDC dc, const RECT& bounds, int id, COLORREF color) {
    const int cx = (bounds.left + bounds.right) / 2;
    const int cy = (bounds.top + bounds.bottom) / 2;
    const int savedDc = SaveDC(dc);
    SetGraphicsMode(dc, GM_ADVANCED);
    XFORM transform{};
    transform.eM11 = 0.62f;
    transform.eM22 = 0.62f;
    transform.eDx = static_cast<FLOAT>(cx) * (1.0f - transform.eM11);
    transform.eDy = static_cast<FLOAT>(cy) * (1.0f - transform.eM22);
    SetWorldTransform(dc, &transform);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HBRUSH hollow = static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, hollow));
    switch (id) {
    case kIdToolbarNew:
        MoveToEx(dc, cx - 10, cy, nullptr); LineTo(dc, cx + 10, cy);
        MoveToEx(dc, cx, cy - 10, nullptr); LineTo(dc, cx, cy + 10); break;
    case kIdToolbarOpen:
        MoveToEx(dc, cx - 12, cy - 4, nullptr); LineTo(dc, cx - 6, cy - 4);
        LineTo(dc, cx - 2, cy); LineTo(dc, cx + 12, cy);
        LineTo(dc, cx + 9, cy + 10); LineTo(dc, cx - 12, cy + 10); LineTo(dc, cx - 12, cy - 4);
        break;
    case kIdToolbarSave:
    case kIdToolbarSaveAs:
        Rectangle(dc, cx - 11, cy - 11, cx + 11, cy + 11);
        MoveToEx(dc, cx - 6, cy - 10, nullptr); LineTo(dc, cx - 6, cy - 2);
        LineTo(dc, cx + 6, cy - 2); LineTo(dc, cx + 6, cy - 10);
        Rectangle(dc, cx - 6, cy + 2, cx + 6, cy + 10); break;
    case kIdToolbarClose:
        MoveToEx(dc, cx - 8, cy - 8, nullptr); LineTo(dc, cx + 8, cy + 8);
        MoveToEx(dc, cx + 8, cy - 8, nullptr); LineTo(dc, cx - 8, cy + 8); break;
    case kIdToolbarUndo:
        Arc(dc, cx - 11, cy - 10, cx + 12, cy + 12, cx - 11, cy, cx - 2, cy - 10);
        MoveToEx(dc, cx - 11, cy, nullptr); LineTo(dc, cx - 2, cy - 6);
        MoveToEx(dc, cx - 11, cy, nullptr); LineTo(dc, cx - 2, cy + 6); break;
    case kIdToolbarRedo:
        Arc(dc, cx - 12, cy - 10, cx + 11, cy + 12, cx + 11, cy, cx + 2, cy - 10);
        MoveToEx(dc, cx + 11, cy, nullptr); LineTo(dc, cx + 2, cy - 6);
        MoveToEx(dc, cx + 11, cy, nullptr); LineTo(dc, cx + 2, cy + 6); break;
    case kIdToolbarFind:
        Ellipse(dc, cx - 10, cy - 10, cx + 5, cy + 5);
        MoveToEx(dc, cx + 1, cy + 1, nullptr); LineTo(dc, cx + 11, cy + 11); break;
    case kIdToolbarReplace:
        MoveToEx(dc, cx - 11, cy - 5, nullptr); LineTo(dc, cx + 8, cy - 5);
        MoveToEx(dc, cx + 8, cy - 5, nullptr); LineTo(dc, cx + 3, cy - 10);
        MoveToEx(dc, cx + 8, cy - 5, nullptr); LineTo(dc, cx + 3, cy);
        MoveToEx(dc, cx + 11, cy + 5, nullptr); LineTo(dc, cx - 8, cy + 5);
        MoveToEx(dc, cx - 8, cy + 5, nullptr); LineTo(dc, cx - 3, cy);
        MoveToEx(dc, cx - 8, cy + 5, nullptr); LineTo(dc, cx - 3, cy + 10); break;
    case kIdToolbarGoto:
        MoveToEx(dc, cx - 10, cy, nullptr); LineTo(dc, cx + 8, cy);
        MoveToEx(dc, cx + 8, cy, nullptr); LineTo(dc, cx + 1, cy - 7);
        MoveToEx(dc, cx + 8, cy, nullptr); LineTo(dc, cx + 1, cy + 7); break;
    case kIdToolbarFormat:
        MoveToEx(dc, cx - 9, cy - 8, nullptr); LineTo(dc, cx + 10, cy - 8);
        MoveToEx(dc, cx - 9, cy, nullptr); LineTo(dc, cx + 10, cy);
        MoveToEx(dc, cx - 9, cy + 8, nullptr); LineTo(dc, cx + 10, cy + 8); break;
    case kIdToolbarRun: {
        POINT triangle[3]{{cx - 5, cy - 11}, {cx + 10, cy}, {cx - 5, cy + 11}};
        Polygon(dc, triangle, 3);
        break;
    }
    case kIdToolbarEncoding:
        Rectangle(dc, cx - 10, cy - 10, cx + 10, cy + 10);
        MoveToEx(dc, cx - 6, cy - 4, nullptr); LineTo(dc, cx + 6, cy - 4);
        MoveToEx(dc, cx - 6, cy + 2, nullptr); LineTo(dc, cx + 6, cy + 2); break;
    case kIdToolbarLineNumbers:
        for (int row = -1; row <= 1; ++row) {
            MoveToEx(dc, cx - 10, cy + row * 7, nullptr); LineTo(dc, cx - 5, cy + row * 7);
            MoveToEx(dc, cx, cy + row * 7, nullptr); LineTo(dc, cx + 10, cy + row * 7);
        }
        break;
    case kIdToolbarWrap:
        MoveToEx(dc, cx - 11, cy - 6, nullptr); LineTo(dc, cx + 9, cy - 6);
        MoveToEx(dc, cx + 9, cy - 6, nullptr); LineTo(dc, cx + 4, cy - 11);
        MoveToEx(dc, cx + 9, cy - 6, nullptr); LineTo(dc, cx + 4, cy - 1);
        MoveToEx(dc, cx + 11, cy + 6, nullptr); LineTo(dc, cx - 9, cy + 6);
        MoveToEx(dc, cx - 9, cy + 6, nullptr); LineTo(dc, cx - 4, cy + 1);
        MoveToEx(dc, cx - 9, cy + 6, nullptr); LineTo(dc, cx - 4, cy + 11); break;
    case kIdToolbarTheme:
        Ellipse(dc, cx - 11, cy - 11, cx + 11, cy + 11);
        MoveToEx(dc, cx, cy - 10, nullptr); LineTo(dc, cx, cy + 10);
        MoveToEx(dc, cx, cy, nullptr); LineTo(dc, cx + 9, cy + 5); break;
    case kIdToolbarSettings:
        Ellipse(dc, cx - 7, cy - 7, cx + 7, cy + 7);
        for (int angle = 0; angle < 8; ++angle) {
            const double radians = angle * 3.141592653589793 / 4.0;
            const int x1 = cx + static_cast<int>(std::cos(radians) * 9);
            const int y1 = cy + static_cast<int>(std::sin(radians) * 9);
            const int x2 = cx + static_cast<int>(std::cos(radians) * 12);
            const int y2 = cy + static_cast<int>(std::sin(radians) * 12);
            MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2);
        }
        break;
    default: break;
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    RestoreDC(dc, savedDc);
}

void DrawComboItem(const DRAWITEMSTRUCT& item) {
    const ThemePalette& palette = CurrentPalette();
    RECT rect = item.rcItem;
    HBRUSH brush = CreateSolidBrush(palette.controlBackground);
    FillRect(item.hDC, &rect, brush);
    DeleteObject(brush);
    if (item.itemState & ODS_FOCUS) {
        HPEN pen = CreatePen(PS_SOLID, 1, palette.accent);
        HPEN oldPen = static_cast<HPEN>(SelectObject(item.hDC, pen));
        Rectangle(item.hDC, rect.left, rect.top, rect.right, rect.bottom);
        SelectObject(item.hDC, oldPen);
        DeleteObject(pen);
    }
    wchar_t text[256]{};
    if (item.itemID != static_cast<UINT>(-1)) {
        SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID, reinterpret_cast<LPARAM>(text));
    } else {
        GetWindowTextW(item.hwndItem, text, static_cast<int>(std::size(text)));
    }
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, palette.controlText);
    RECT textRect = rect;
    textRect.left += 7;
    DrawTextW(item.hDC, text, -1, &textRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

std::wstring GetWindowTextValue(HWND window) {
    const int length = GetWindowTextLengthW(window);
    if (length <= 0) {
        return {};
    }

    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(window, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

std::string WideToUtf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }

    // No WC_ERR_INVALID_CHARS: a lone surrogate must become U+FFFD instead of
    // making the whole conversion fail (which used to save an empty file).
    const int size = WideCharToMultiByte(CP_UTF8, 0,
        value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0,
        value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(std::string_view value) {
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        const int fallbackSize = MultiByteToWideChar(CP_ACP, 0,
            value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (fallbackSize <= 0) {
            return {};
        }

        std::wstring fallback(static_cast<size_t>(fallbackSize), L'\0');
        MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()),
            fallback.data(), fallbackSize);
        return fallback;
    }

    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string HexEncode(std::string_view value) {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(value.size() * 2);
    for (const unsigned char byte : value) {
        result.push_back(digits[(byte >> 4) & 0x0F]);
        result.push_back(digits[byte & 0x0F]);
    }
    return result;
}

int HexDigit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

std::string HexDecode(std::string_view value) {
    if (value.size() % 2 != 0) {
        return {};
    }

    std::string result;
    result.reserve(value.size() / 2);
    for (size_t index = 0; index < value.size(); index += 2) {
        const int high = HexDigit(value[index]);
        const int low = HexDigit(value[index + 1]);
        if (high < 0 || low < 0) {
            return {};
        }
        result.push_back(static_cast<char>((high << 4) | low));
    }
    return result;
}

std::vector<std::string> SplitTabs(std::string_view line) {
    std::vector<std::string> values;
    size_t start = 0;
    while (start <= line.size()) {
        const size_t end = line.find('\t', start);
        if (end == std::string_view::npos) {
            values.emplace_back(line.substr(start));
            break;
        }
        values.emplace_back(line.substr(start, end - start));
        start = end + 1;
    }
    return values;
}

bool ParseInt64(std::string_view value, int64_t& result) {
    try {
        size_t consumed = 0;
        result = std::stoll(std::string(value), &consumed);
        return consumed == value.size();
    } catch (...) {
        return false;
    }
}

bool ParseInt(std::string_view value, int& result) {
    int64_t parsed = 0;
    if (!ParseInt64(value, parsed)) {
        return false;
    }
    result = static_cast<int>(parsed);
    return true;
}

bool ParseBool(std::string_view value) {
    return value == "1" || value == "true";
}

bool EnsureDirectory(const fs::path& path) {
    std::error_code error;
    fs::create_directories(path, error);
    return !error;
}

