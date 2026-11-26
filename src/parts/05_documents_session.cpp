void LayoutChildren(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int menuHeight = Scale(kMenuBarHeight);
    const int toolbarHeight = Scale(kToolbarHeight);
    const int tabHeight = Scale(kTabHeight);
    const int statusHeight = Scale(kStatusHeight);
    const int tabY = menuHeight + toolbarHeight;
    const int contentY = tabY + tabHeight;
    const int areaHeight = std::max(0, height - contentY - statusHeight);
    // The find-in-files panel takes the bottom of the content area, leaving
    // the editor at least a few lines.
    const int panelHeight = g_app.searchPanel && g_app.searchPanelVisible
        ? std::clamp(g_app.searchPanelHeight, std::min(Scale(160), areaHeight), std::max(0, areaHeight - Scale(120))) : 0;
    const int contentHeight = areaHeight - panelHeight;
    if (g_app.searchPanel) {
        MoveWindow(g_app.searchPanel, 0, contentY + contentHeight, width, panelHeight, TRUE);
        ShowWindow(g_app.searchPanel, panelHeight > 0 ? SW_SHOW : SW_HIDE);
    }
    const int toolboxWidth = g_app.toolboxVisible && g_app.toolboxPanel ? std::min(Scale(g_app.toolboxWidth), std::max(0, width - Scale(320))) : 0;
    if (g_app.toolboxPanel) MoveWindow(g_app.toolboxPanel, width - toolboxWidth, contentY, toolboxWidth, contentHeight, TRUE);
    const int editorWidth = width - toolboxWidth;
    HWND activeEditor = (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size()))
        ? g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor : nullptr;
    g_app.gutterWidth = GutterWidthFor(activeEditor);
    const int gutterWidth = g_app.showLineNumbers ? g_app.gutterWidth : 0;

    if (g_app.menuBar) MoveWindow(g_app.menuBar, 0, 0, width, menuHeight, TRUE);
    if (g_app.toolbar) MoveWindow(g_app.toolbar, 0, menuHeight, width, toolbarHeight, TRUE);
    if (g_app.tabs) MoveWindow(g_app.tabs, 0, tabY, width, tabHeight, TRUE);
    if (g_app.gutter) {
        MoveWindow(g_app.gutter, 0, contentY, gutterWidth, contentHeight, TRUE);
        ShowWindow(g_app.gutter, g_app.showLineNumbers ? SW_SHOW : SW_HIDE);
    }
    for (TabDocument& document : g_app.documents) {
        MoveWindow(document.editor, gutterWidth, contentY,
            std::max(0, editorWidth - gutterWidth), contentHeight, TRUE);
    }
    MoveWindow(g_app.statusBar, 0, std::max(0, height - statusHeight), width, statusHeight, TRUE);
    UpdateStatusBar();
    RefreshGutter();
}

void SelectTab(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) {
        return;
    }

    if (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size())) {
        CaptureEditorState(g_app.documents[static_cast<size_t>(g_app.activeIndex)]);
        ShowWindow(g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor, SW_HIDE);
    }

    g_app.activeIndex = index;
    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    ShowWindow(document.editor, SW_SHOW);
    SetFocus(document.editor);
    RestoreEditorState(document);
    SyntaxColorVisible(document);
    ApplyCurrentLineHighlight(document);
    ApplyBracketMatchHighlight(document, true);
    EnsureTabVisible(index);
    InvalidateToolbar();
    if (g_app.showLineNumbers && GutterWidthFor(document.editor) != g_app.gutterWidth) LayoutChildren(g_app.window);
    g_app.stateDirty = true;
    UpdateWindowTitle();
}

void SelectAdjacentTab(int delta) {
    const int count = static_cast<int>(g_app.documents.size());
    if (count <= 1 || g_app.activeIndex < 0) return;
    int next = (g_app.activeIndex + delta) % count;
    if (next < 0) next += count;
    SelectTab(next);
}

void MarkRecoveryDirty(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) {
        return;
    }
    g_app.documents[static_cast<size_t>(index)].recoveryDirty = true;
    g_app.stateDirty = true;
}

bool SaveDocumentRecovery(TabDocument& document) {
    if (!document.recoveryDirty) {
        return true;
    }

    // Snapshots always use UTF-8 BOM + CRLF so recovery never depends on the
    // user's chosen save encoding (ANSI would silently drop characters).
    const std::wstring text = GetWindowTextValue(document.editor);
    if (!WriteTextFile(document.snapshotPath, text, 1, 0, true)) {
        return false;
    }

    document.recoveryDirty = false;
    return true;
}

bool SaveSessionFile() {
    std::ostringstream output;
    output << "MCSESSION\t1\n";
    output << "ACTIVE\t" << g_app.activeIndex << "\n";
    output << "THEME\t" << static_cast<int>(g_app.theme) << "\n";
    output << "LINE_SPACING\t" << g_app.lineSpacingPercent << "\n";
    output << "TOP_SPACING\t" << g_app.topSpacingPixels << "\n";
    output << "OPTIONS\t" << (g_app.showLineNumbers ? 1 : 0) << '\t'
           << (g_app.wordWrap ? 1 : 0) << '\t'
           << (g_app.sidebarVisible ? 1 : 0) << '\t'
           << g_app.fontSize << '\t'
           << HexEncode(WideToUtf8(g_app.fontFamily)) << '\t'
           << g_app.languageIndex << '\t'
           << g_app.encodingIndex << '\t'
           << g_app.lineEndingIndex << '\t'
           << (g_app.showWhitespace ? 1 : 0) << '\t'
           << (g_app.showIndentGuides ? 1 : 0) << "\n";
    output << "TOOLBOX\t" << (g_app.toolboxVisible ? 1 : 0) << '\t' << g_app.toolboxWidth << '\t' << g_app.toolboxMask << '\t' << g_app.toolboxTool << "\n";
    output << "PREFERENCES\t" << (g_app.launchAtStartup ? 1 : 0) << '\t'
           << static_cast<int>(g_app.closeBehavior) << '\t'
           << (g_app.previewTextTools ? 1 : 0) << '\t'
           << g_app.toolOutputMode << '\t'
           << static_cast<int>(g_app.windowCloseBehavior) << '\t'
           << (g_app.minimizeToTray ? 1 : 0) << '\t'
           << (g_app.showTrayIcon ? 1 : 0) << "\n";
    SaveShortcutBindings(output);

    // rcNormalPosition is the restored size even while maximized/minimized.
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    GetWindowPlacement(g_app.window, &placement);
    const RECT& windowRect = placement.rcNormalPosition;
    output << "WINDOW\t" << windowRect.left << '\t' << windowRect.top << '\t'
           << windowRect.right - windowRect.left << '\t'
           << windowRect.bottom - windowRect.top << '\t'
           << (IsZoomed(g_app.window) ? 1 : 0) << "\n";

    for (size_t index = 0; index < g_app.documents.size(); ++index) {
        TabDocument& document = g_app.documents[index];
        CaptureEditorState(document);
        output << "TAB\t" << index << '\t'
               << HexEncode(WideToUtf8(document.path)) << '\t'
               << HexEncode(WideToUtf8(document.title)) << '\t'
               << HexEncode(WideToUtf8(document.snapshotPath.wstring())) << '\t'
               << document.caret << '\t'
               << document.anchor << '\t'
               << document.firstVisibleLine << '\t'
               << document.horizontalScroll << '\t'
               << (document.modified ? 1 : 0) << '\t'
               << (document.untitled ? 1 : 0) << '\t'
               << document.fileTimestamp << '\t'
               << document.encoding << '\t'
               << document.lineEnding << '\t'
               << document.language << '\t';
        for (size_t bookmark = 0; bookmark < document.bookmarks.size(); ++bookmark) {
            if (bookmark != 0) output << ',';
            output << document.bookmarks[bookmark];
        }
        output << '\t' << (document.pinned ? 1 : 0) << '\n';
    }

    return WriteDataFile(g_app.sessionPath, output.str());
}

bool PersistAll(bool forceSnapshots) {
    bool success = true;
    if (forceSnapshots) {
        for (TabDocument& document : g_app.documents) {
            document.recoveryDirty = true;
        }
    }

    for (TabDocument& document : g_app.documents) {
        success = SaveDocumentRecovery(document) && success;
    }
    success = SaveSessionFile() && success;
    if (success) {
        g_app.stateDirty = false;
    }
    return success;
}

bool CreateRunningMarker() {
    return WriteDataFile(g_app.runningMarkerPath, "MCnotepad running\n");
}

void RemoveRunningMarker() {
    std::error_code error;
    fs::remove(g_app.runningMarkerPath, error);
}

void ChangeEditorFontSize(int delta) {
    const int next = delta == 0 ? 13 : std::clamp(g_app.fontSize + delta, 10, 32);
    if (next == g_app.fontSize) return;
    g_app.fontSize = next;
    CreateUiFonts();
    ApplyEditorFont();
    g_app.stateDirty = true;
    ShowNotice(L"编辑器字号 " + std::to_wstring(g_app.fontSize) + L" px");
}

bool TryAutoCloseCharacter(HWND editor, wchar_t character) {
    wchar_t closing = 0;
    switch (character) {
    case L'(': closing = L')'; break;
    case L'[': closing = L']'; break;
    case L'{': closing = L'}'; break;
    case L'"': closing = L'"'; break;
    case L'\'': closing = L'\''; break;
    default: return false;
    }
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const std::wstring text = GetEditorRawText(editor);
    if (selection.cpMin == selection.cpMax && selection.cpMin < static_cast<LONG>(text.size()) &&
        text[static_cast<size_t>(selection.cpMin)] == closing && character != L'(' && character != L'[' && character != L'{') {
        SendMessageW(editor, EM_SETSEL, selection.cpMin + 1, selection.cpMin + 1);
        return true;
    }
    std::wstring selected;
    if (selection.cpMax > selection.cpMin) {
        selected = text.substr(static_cast<size_t>(selection.cpMin),
            static_cast<size_t>(selection.cpMax - selection.cpMin));
    }
    const std::wstring replacement = std::wstring(1, character) + selected + std::wstring(1, closing);
    const LONG start = selection.cpMin;
    SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(replacement.c_str()));
    const LONG innerStart = start + 1;
    const LONG innerEnd = innerStart + static_cast<LONG>(selected.size());
    SendMessageW(editor, EM_SETSEL, innerStart, innerEnd);
    return true;
}

std::wstring SmartIndentForEnter(HWND editor) {
    if (!editor) return {};
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    if (selection.cpMin != selection.cpMax) return {};
    const std::wstring text = GetEditorRawText(editor);
    const LONG caret = std::clamp(selection.cpMin, 0L, static_cast<LONG>(text.size()));
    const LRESULT lineNumber = SendMessageW(editor, EM_EXLINEFROMCHAR, 0, caret);
    const LONG lineStart = lineNumber >= 0
        ? static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, lineNumber, 0)) : 0;
    if (lineStart < 0 || lineStart > caret) return {};

    size_t prefixEnd = static_cast<size_t>(caret - lineStart);
    prefixEnd = std::min(prefixEnd, text.size() - static_cast<size_t>(lineStart));
    std::wstring linePrefix = text.substr(static_cast<size_t>(lineStart), prefixEnd);
    size_t firstCode = 0;
    while (firstCode < linePrefix.size() && (linePrefix[firstCode] == L' ' || linePrefix[firstCode] == L'\t')) ++firstCode;
    std::wstring indent = linePrefix.substr(0, firstCode);

    size_t lastCode = linePrefix.size();
    while (lastCode > firstCode && iswspace(linePrefix[lastCode - 1])) --lastCode;
    const wchar_t previous = lastCode > firstCode ? linePrefix[lastCode - 1] : 0;
    const wchar_t next = caret < static_cast<LONG>(text.size()) ? text[static_cast<size_t>(caret)] : 0;
    const bool opensBlock = previous == L'{' || previous == L'[' || previous == L'(';
    const bool closesBlock = next == L'}' || next == L']' || next == L')';
    if (closesBlock && lastCode == firstCode && indent.size() >= 4) indent.resize(indent.size() - 4);
    if (opensBlock) indent += L"    ";
    return L"\r" + indent;
}

bool HandleSmartIndentEnter(HWND editor) {
    if (!editor) return false;
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    if (selection.cpMin != selection.cpMax) return false;
    const std::wstring insertion = SmartIndentForEnter(editor);
    if (insertion.empty()) return false;
    SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(insertion.c_str()));
    return true;
}

void DrawEditorGuides(HWND editor) {
    if (!editor || (!g_app.showWhitespace && !g_app.showIndentGuides)) return;
    RECT client{};
    GetClientRect(editor, &client);
    HDC dc = GetDC(editor);
    if (!dc) return;
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(editor, WM_GETFONT, 0, 0));
    HGDIOBJ previousFont = font ? SelectObject(dc, font) : nullptr;
    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    const int lineHeight = std::max(1, static_cast<int>(metrics.tmHeight));
    const int charWidth = std::max(1, static_cast<int>(metrics.tmAveCharWidth));
    const COLORREF guideColor = Blend(EditorBackground(), CurrentPalette().accent, g_app.darkTheme ? 28 : 20);
    HPEN guidePen = CreatePen(PS_DOT, 1, guideColor);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, guidePen));
    SetBkMode(dc, TRANSPARENT);
    const int firstLine = std::max(0, static_cast<int>(SendMessageW(editor, EM_GETFIRSTVISIBLELINE, 0, 0)));
    const int lineCount = static_cast<int>(SendMessageW(editor, EM_GETLINECOUNT, 0, 0));
    const int visibleLines = client.bottom / lineHeight + 3;
    for (int line = firstLine; line < lineCount && line < firstLine + visibleLines; ++line) {
        const LONG start = static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, line, 0));
        if (start < 0) continue;
        POINT origin{};
        SendMessageW(editor, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&origin), start);
        if (origin.y + lineHeight < 0 || origin.y > client.bottom) continue;
        wchar_t buffer[4096]{};
        *reinterpret_cast<WORD*>(buffer) = static_cast<WORD>(std::size(buffer) - 1);
        const LRESULT copied = SendMessageW(editor, EM_GETLINE, line, reinterpret_cast<LPARAM>(buffer));
        const int length = std::max(0, static_cast<int>(copied));
        int column = 0;
        int leading = 0;
        for (int index = 0; index < length; ++index) {
            const wchar_t character = buffer[index];
            if (character == L'\t') {
                column = (column / 4 + 1) * 4;
                ++leading;
            } else if (character == L' ') {
                ++column;
                ++leading;
            } else {
                break;
            }
        }
        if (g_app.showIndentGuides && leading > 0) {
            for (int guide = 4; guide <= column; guide += 4) {
                const int x = origin.x + guide * charWidth;
                MoveToEx(dc, x, origin.y, nullptr);
                LineTo(dc, x, origin.y + lineHeight);
            }
        }
        if (g_app.showWhitespace) {
            int visualColumn = 0;
            for (int index = 0; index < length; ++index) {
                const wchar_t character = buffer[index];
                POINT position{};
                SendMessageW(editor, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&position), start + index);
                if (character == L' ') {
                    const int x = position.x + charWidth / 2;
                    const int y = origin.y + lineHeight / 2;
                    Ellipse(dc, x - 1, y - 1, x + 2, y + 2);
                    ++visualColumn;
                } else if (character == L'\t') {
                    const int nextColumn = (visualColumn / 4 + 1) * 4;
                    const int x = position.x + 2;
                    const int y = origin.y + lineHeight / 2;
                    MoveToEx(dc, x, y, nullptr);
                    LineTo(dc, origin.x + nextColumn * charWidth - 3, y);
                    ++visualColumn;
                    visualColumn = nextColumn;
                } else {
                    ++visualColumn;
                }
            }
        }
    }
    SelectObject(dc, oldPen);
    DeleteObject(guidePen);
    if (previousFont) SelectObject(dc, previousFont);
    ReleaseDC(editor, dc);
}

LRESULT CALLBACK EditorProc(HWND editor, UINT message, WPARAM wParam, LPARAM lParam) {
    const auto oldIt = g_editorProcs.find(editor);
    const WNDPROC oldProc = oldIt != g_editorProcs.end() ? oldIt->second : nullptr;

    if (message == WM_IME_STARTCOMPOSITION) {
        g_imeComposingEditors.insert(editor);
    }

    // This is a plain-text editor: every paste (menu, Ctrl+V, Shift+Insert)
    // takes only the clipboard's Unicode text, never rich formatting or images.
    if (message == WM_CONTEXTMENU) {
        ShowEditorContextMenu(editor, POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
        return 0;
    }
    if (message == WM_PASTE) {
        SendMessageW(editor, EM_PASTESPECIAL, CF_UNICODETEXT, 0);
        return 0;
    }
    if (message == WM_KEYDOWN && wParam == VK_INSERT && (GetKeyState(VK_SHIFT) & 0x8000) != 0 &&
        (GetKeyState(VK_CONTROL) & 0x8000) == 0) {
        SendMessageW(editor, WM_PASTE, 0, 0);
        return 0;
    }

    if (message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
        (GetKeyState(VK_MENU) & 0x8000) != 0 && wParam == VK_DOWN) {
        AddNextLineCaret();
        return 0;
    }

    // Handle Enter before RichEdit's default key processing.  RichEdit may
    // insert a paragraph while processing WM_KEYDOWN and then receive WM_CHAR
    // as well; doing smart-indent from WM_CHAR can therefore create a blank
    // line.  A multi-caret edit still uses the normal WM_CHAR replacement path.
    if (message == WM_KEYDOWN && wParam == VK_RETURN &&
        (GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
        (GetKeyState(VK_MENU) & 0x8000) == 0) {
        bool hasMultiCaret = false;
        for (const TabDocument& document : g_app.documents) {
            if (document.editor == editor) {
                hasMultiCaret = !document.multiCarets.empty();
                break;
            }
        }
        if (!hasMultiCaret && HandleSmartIndentEnter(editor)) {
            RecordMacroText(L"\r");
            return 0;
        }
    }
    if (message == WM_KEYDOWN && (wParam == VK_BACK || wParam == VK_DELETE) &&
        ApplyMultiCaretBackspace(editor, wParam == VK_DELETE)) {
        return 0;
    }

    if (message == WM_CHAR && (GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
        (GetKeyState(VK_MENU) & 0x8000) == 0 &&
        (wParam >= 0x20 || wParam == L'\r' || wParam == L'\t')) {
        const std::wstring typed(1, static_cast<wchar_t>(wParam));
        if (ApplyMultiCaretReplacement(editor, typed)) {
            RecordMacroText(typed);
            return 0;
        }
        RecordMacroText(typed);
    }

    if (message == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
        (GetKeyState(VK_MENU) & 0x8000) == 0) {
        if (wParam == VK_F2) {
            SendMessageW(g_app.window, WM_COMMAND, kIdViewToggleBookmark, 0);
            return 0;
        }
        if (wParam == VK_OEM_PLUS || wParam == VK_ADD) {
            ChangeEditorFontSize(1);
            return 0;
        }
        if (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT) {
            ChangeEditorFontSize(-1);
            return 0;
        }
        if (wParam == '0') {
            ChangeEditorFontSize(13 - g_app.fontSize);
            return 0;
        }
        switch (static_cast<UINT>(wParam)) {
        case 'A':
            SendMessageW(editor, EM_SETSEL, 0, -1);
            return 0;
        case 'C':
            SendMessageW(editor, WM_COPY, 0, 0);
            return 0;
        case 'V':
            SendMessageW(editor, WM_PASTE, 0, 0);
            return 0;
        case 'X':
            SendMessageW(editor, WM_CUT, 0, 0);
            return 0;
        case 'Z':
            SendMessageW(editor, WM_UNDO, 0, 0);
            return 0;
        case 'Y':
            SendMessageW(editor, EM_REDO, 0, 0);
            return 0;
        default:
            break;
        }
    }

    if (message == WM_KEYDOWN && wParam == VK_F4) {
        if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
            SendMessageW(g_app.window, WM_COMMAND, kIdViewPreviousBookmark, 0);
            return 0;
        }
        SendMessageW(g_app.window, WM_COMMAND, kIdViewNextBookmark, 0);
        return 0;
    }

    if (message == WM_MOUSEWHEEL && (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) != 0) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta != 0) {
            ChangeEditorFontSize(delta > 0 ? 1 : -1);
        }
        return 0;
    }

    if (message == WM_CHAR && (GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
        (GetKeyState(VK_MENU) & 0x8000) == 0 && TryAutoCloseCharacter(editor, static_cast<wchar_t>(wParam))) {
        return 0;
    }

    const LRESULT result = oldProc != nullptr
        ? CallWindowProcW(oldProc, editor, message, wParam, lParam)
        : DefWindowProcW(editor, message, wParam, lParam);
    if (message == WM_NCDESTROY) {
        g_editorProcs.erase(editor);
        g_imeComposingEditors.erase(editor);
    } else if (message == WM_MOUSEWHEEL || message == WM_KEYDOWN || message == WM_VSCROLL) {
        // EN_VSCROLL is not sent for wheel or keyboard scrolling.
        RefreshGutter();
        SyntaxOnViewChanged(editor);
    } else if (message == WM_SIZE || message == WM_SHOWWINDOW) {
        UpdateHorizontalScrollbar(editor);
        if (message == WM_SIZE) SyntaxOnViewChanged(editor);
    }
    if (message == WM_LBUTTONUP) {
        for (TabDocument& document : g_app.documents) {
            if (document.editor == editor) {
                ApplyCurrentLineHighlight(document);
                ApplyBracketMatchHighlight(document);
                break;
            }
        }
    }
    if (message == WM_IME_ENDCOMPOSITION) {
        g_imeComposingEditors.erase(editor);
        // Let the IME commit its result and restore the caret first, then
        // repaint the visual helpers once after composition has ended.
        for (TabDocument& document : g_app.documents) {
            if (document.editor == editor) {
                ApplyCurrentLineHighlight(document);
                ApplyBracketMatchHighlight(document);
                break;
            }
        }
    }
    if (message == WM_PAINT) {
        DrawEditorGuides(editor);
        DrawMultiCaretOverlay(editor);
    }
    return result;
}

void AddTabToControl(const std::wstring& title) {
    (void)title;
    if (g_app.tabs) InvalidateRect(g_app.tabs, nullptr, FALSE);
}

bool SamePath(const fs::path& left, const fs::path& right) {
    if (left.empty() || right.empty()) return false;
    std::error_code error;
    const fs::path a = fs::absolute(left, error).lexically_normal();
    const fs::path b = fs::absolute(right, error).lexically_normal();
    return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

bool IsSnapshotInUse(const fs::path& snapshot) {
    for (const TabDocument& document : g_app.documents) {
        if (SamePath(document.snapshotPath, snapshot)) return true;
    }
    return false;
}

// Snapshot files are owned by exactly one tab for its whole lifetime. Names are
// never derived from the tab index, so closing or reordering tabs can't make two
// tabs share (and overwrite) one snapshot.
fs::path AllocateSnapshotPath() {
    for (int number = 1;; ++number) {
        fs::path candidate = g_app.recoveryDirectory /
            (L"tab-" + std::to_wstring(number) + L".snapshot");
        std::error_code error;
        if (!IsSnapshotInUse(candidate) && !fs::exists(candidate, error)) return candidate;
    }
}

// Removes snapshot files left behind by tabs that no longer exist (e.g. after a
// crash). Only call this once the session file was read successfully.
void RemoveOrphanSnapshots() {
    std::vector<fs::path> orphans;
    std::error_code error;
    for (fs::directory_iterator it(g_app.recoveryDirectory, error), end; !error && it != end; it.increment(error)) {
        std::error_code typeError;
        if (!it->is_regular_file(typeError)) continue;
        const fs::path file = it->path();
        const std::wstring name = file.filename().wstring();
        if (name.ends_with(L".snapshot.tmp") || (name.find(L".snapshot.mcn-") != std::wstring::npos && name.ends_with(L".tmp"))) {
            orphans.push_back(file);  // interrupted atomic write
        } else if (name.ends_with(L".snapshot") && !IsSnapshotInUse(file)) {
            orphans.push_back(file);
        }
    }
    for (const fs::path& file : orphans) {
        std::error_code removeError;
        fs::remove(file, removeError);
    }
}

struct DocumentFormat {
    int encoding = -1;     // -1 = application default
    int lineEnding = -1;   // -1 = application default
    int language = kLanguageAuto;
    uint64_t fileTimestamp = 0;
};

// RichEdit wraps against the window when the target line width is 0 and never
// wraps when it is 1; the WS_HSCROLL style alone does not change wrapping.
void ApplyWordWrap(HWND editor) {
    if (!editor) return;
    SendMessageW(editor, EM_SETTARGETDEVICE, 0, g_app.wordWrap ? 0 : 1);
    const LONG style = GetWindowLongW(editor, GWL_STYLE);
    const LONG wanted = g_app.wordWrap ? (style & ~WS_HSCROLL) : (style | WS_HSCROLL);
    if (wanted == style) return;
    SetWindowLongW(editor, GWL_STYLE, wanted);
    SetWindowPos(editor, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

// RichEdit's no-wrap mode keeps a placeholder horizontal scrollbar (range
// 0-100, page 0) even when every line fits. Show it only when there is
// something to scroll.
void UpdateHorizontalScrollbar(HWND editor) {
    static bool updating = false;
    if (!editor || updating) return;
    updating = true;
    bool needed = false;
    if (!g_app.wordWrap) {
        SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE};
        if (GetScrollInfo(editor, SB_HORZ, &info)) {
            needed = info.nPage > 0 && info.nMax - info.nMin + 1 > static_cast<int>(info.nPage);
        }
    }
    ShowScrollBar(editor, SB_HORZ, needed ? TRUE : FALSE);
    updating = false;
}

void ApplyWordWrapAll() {
    for (const TabDocument& document : g_app.documents) {
        ApplyWordWrap(document.editor);
        UpdateHorizontalScrollbar(document.editor);
    }
    RefreshGutter();
}

// RichEdit stores multiple line spacing in twentieths of a line.  Formatting
// the complete document keeps the setting consistent across every paragraph,
// while the event guard and modify flag preservation ensure this view option
// never turns a clean document into an unsaved one.
void ApplyEditorLineSpacing(HWND editor) {
    if (!editor) return;
    EditorEventMute mute(editor);
    const SelectionState savedSelection = CaptureSelection(editor);
    const LONG firstVisibleLine = static_cast<LONG>(SendMessageW(editor, EM_GETFIRSTVISIBLELINE, 0, 0));
    const LRESULT wasModified = SendMessageW(editor, EM_GETMODIFY, 0, 0);

    PARAFORMAT2 format{};
    format.cbSize = sizeof(format);
    format.dwMask = PFM_LINESPACING | PFM_SPACEBEFORE;
    // Use one fixed paragraph line box for the whole document.  RichEdit's
    // multiple-line mode derives the box from the selected glyph run, so a
    // CJK fallback font can make Chinese lines visibly taller than Latin
    // lines.  Exact spacing keeps both scripts on the same baseline grid.
    HDC dc = GetDC(editor);
    TEXTMETRICW metrics{};
    if (dc) {
        HFONT font = g_app.editorFont != nullptr
            ? g_app.editorFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HGDIOBJ previous = SelectObject(dc, font);
        GetTextMetricsW(dc, &metrics);
        SelectObject(dc, previous);
        ReleaseDC(editor, dc);
    }
    const int basePixels = std::max(1, static_cast<int>(metrics.tmHeight) + Scale(2));
    const int requestedPixels = MulDiv(basePixels, g_app.lineSpacingPercent, 100);
    const int linePixels = std::max(basePixels, requestedPixels);
    const int lineTwips = std::clamp(MulDiv(linePixels, 1440, static_cast<int>(g_app.dpi)), 120, 720);
    format.bLineSpacingRule = tomLineSpaceExactly;
    format.dyLineSpacing = lineTwips;
    format.dySpaceBefore = static_cast<SHORT>(std::clamp(
        MulDiv(g_app.topSpacingPixels, 1440, static_cast<int>(g_app.dpi)), 0, 1440));

    SendMessageW(editor, EM_SETSEL, 0, -1);
    SendMessageW(editor, EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&format));
    RestoreSelection(editor, savedSelection);
    SendMessageW(editor, EM_SETMODIFY, wasModified ? TRUE : FALSE, 0);

    const LONG nowVisibleLine = static_cast<LONG>(SendMessageW(editor, EM_GETFIRSTVISIBLELINE, 0, 0));
    if (firstVisibleLine != nowVisibleLine) {
        SendMessageW(editor, EM_LINESCROLL, 0, firstVisibleLine - nowVisibleLine);
    }
}

void ApplyEditorLineSpacingAll() {
    for (const TabDocument& document : g_app.documents) {
        ApplyEditorLineSpacing(document.editor);
    }
    RefreshGutter();
    g_app.stateDirty = true;
}

void SetLineSpacingPercent(int percent) {
    g_app.lineSpacingPercent = std::clamp(percent, 50, 300);
    ApplyEditorLineSpacingAll();
    SetMenuState();
}

void OpenCustomLineSpacingDialog() {
    std::wostringstream initial;
    initial << std::fixed << std::setprecision(2)
            << (static_cast<double>(g_app.lineSpacingPercent) / 100.0);
    std::wstring value = initial.str();
    std::vector<std::wstring> values{value};
    if (!PromptForText(L"自定义行距", {L"行距倍数（0.50 - 3.00，例如 1.25）："}, values, nullptr, nullptr)) {
        return;
    }
    value = values.front();

    size_t consumed = 0;
    double multiplier = 0.0;
    try {
        multiplier = std::stod(value, &consumed);
    } catch (...) {
        multiplier = 0.0;
    }
    while (consumed < value.size() && iswspace(value[consumed])) ++consumed;
    if (consumed != value.size() || !std::isfinite(multiplier) || multiplier < 0.5 || multiplier > 3.0) {
        MessageBoxW(g_app.window, L"请输入 0.50 到 3.00 之间的行距倍数。", kWindowTitle, MB_ICONWARNING);
        return;
    }
    SetLineSpacingPercent(static_cast<int>(std::lround(multiplier * 100.0)));
}

void OpenCustomTopSpacingDialog() {
    std::wstring value = std::to_wstring(g_app.topSpacingPixels);
    std::vector<std::wstring> values{value};
    if (!PromptForText(L"自定义顶部间距", {L"每段顶部间距（像素，0 - 40）："}, values, nullptr, nullptr)) {
        return;
    }
    int pixels = -1;
    size_t consumed = 0;
    try {
        pixels = std::stoi(values.front(), &consumed);
    } catch (...) {
        pixels = -1;
    }
    while (consumed < values.front().size() && iswspace(values.front()[consumed])) ++consumed;
    if (consumed != values.front().size() || pixels < 0 || pixels > 40) {
        MessageBoxW(g_app.window, L"请输入 0 到 40 之间的像素值。", kWindowTitle, MB_ICONWARNING);
        return;
    }
    g_app.topSpacingPixels = pixels;
    ApplyEditorLineSpacingAll();
    SetMenuState();
}

int CreateDocument(std::wstring path, std::wstring title, std::wstring text,
    bool untitled, bool modified, int64_t caret, int64_t anchor, int firstVisibleLine,
    const fs::path* explicitSnapshot = nullptr, const DocumentFormat& format = {}) {
    TabDocument document;
    document.path = std::move(path);
    document.untitled = untitled;
    document.modified = modified;
    document.caret = caret;
    document.anchor = anchor;
    document.firstVisibleLine = firstVisibleLine;
    document.encoding = format.encoding >= 0 && format.encoding < kEncodingCount
        ? format.encoding : std::clamp(g_app.encodingIndex, 0, kEncodingCount - 1);
    document.lineEnding = format.lineEnding >= 0 && format.lineEnding < kLineEndingCount
        ? format.lineEnding : std::clamp(g_app.lineEndingIndex, 0, kLineEndingCount - 1);
    document.language = std::clamp(format.language, 0, kLanguageCount - 1);
    document.fileTimestamp = format.fileTimestamp;
    document.title = title.empty()
        ? (document.untitled ? std::wstring(L"Untitled-") + std::to_wstring(g_app.nextUntitledNumber++) : FileTitle(document.path))
        : std::move(title);
    if (document.untitled && document.title.rfind(L"Untitled-", 0) == 0) {
        try {
            const int number = std::stoi(document.title.substr(9));
            g_app.nextUntitledNumber = std::max(g_app.nextUntitledNumber, number + 1);
        } catch (...) {
        }
    }

    document.snapshotPath = (explicitSnapshot != nullptr && !explicitSnapshot->empty() &&
                             !IsSnapshotInUse(*explicitSnapshot))
        ? *explicitSnapshot
        : AllocateSnapshotPath();

    document.editor = CreateWindowExW(
        0,  // no 3D client edge: it drew a bright frame in dark themes
        L"RICHEDIT50W",
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_LEFT | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL | ES_WANTRETURN,
        0, kTabHeight, 0, 0,
        g_app.window,
        nullptr,
        g_app.instance,
        nullptr);

    if (!document.editor) {
        return -1;
    }

    const WNDPROC oldEditorProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        document.editor, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditorProc)));
    if (oldEditorProc != nullptr) {
        g_editorProcs[document.editor] = oldEditorProc;
    }

    SendMessageW(document.editor, EM_SETLIMITTEXT, 0x7FFFFFFE, 0);
    SendMessageW(document.editor, WM_SETFONT,
        reinterpret_cast<WPARAM>(g_app.editorFont != nullptr
            ? g_app.editorFont : GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    SendMessageW(document.editor, EM_SETMARGINS, EC_LEFTMARGIN, MAKELPARAM(Scale(6), 0));
    // Keep the selected Latin font, but allow RichEdit to choose a proper
    // fallback face for CJK text instead of drawing replacement boxes.
    ApplyEditorTypography(document.editor);
    SetWindowTextW(document.editor, text.c_str());
    // RichEdit resets the character format when WM_SETTEXT/SetWindowText is
    // applied.  Set the theme after loading the document so dark-theme text
    // does not fall back to the RichEdit default black foreground.
    ApplyEditorTheme(document.editor);
    ApplyEditorTypography(document.editor);
    ApplyEditorLineSpacing(document.editor);
    ApplyWordWrap(document.editor);
    document.largeFileMode = text.size() >= (8ull << 20);
    if (document.largeFileMode) {
        // RichEdit's undo journal can otherwise duplicate a large buffer for
        // every edit. The document remains fully editable and is saved by the
        // normal path; only the memory-heavy journal is disabled.
        SendMessageW(document.editor, EM_SETUNDOLIMIT, 0, 0);
    }
    DetectFoldRegions(document);
    if (LanguageFromPath(document.path) == kLanguagePlain) document.sniffedLanguage = SniffLanguage(text);
    // Colours are painted when the tab is shown (only the visible part).
    SendMessageW(document.editor, EM_EMPTYUNDOBUFFER, 0, 0);  // loading is not an undoable edit
    SendMessageW(document.editor, EM_SETMODIFY, modified ? TRUE : FALSE, 0);
    // Enable change/scroll notifications only after the initial text is in,
    // so loading a file does not mark it as modified.
    { const std::wstring loaded = GetEditorRawText(document.editor); document.textHash = HashText(loaded); document.logicalLineCount = CountRealLines(loaded); }
    SendMessageW(document.editor, EM_SETEVENTMASK, 0, kEditorEventMask);
    ShowWindow(document.editor, SW_HIDE);

    g_app.documents.push_back(std::move(document));
    AddTabToControl(g_app.documents.back().title);
    UpdateTabTitle(static_cast<int>(g_app.documents.size() - 1));
    if (!g_app.loading) LayoutChildren(g_app.window);  // session restore lays out once at the end
    return static_cast<int>(g_app.documents.size() - 1);
}

bool SaveDocument(int index, bool saveAs) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) {
        return false;
    }

    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    fs::path target = document.path;

    if (saveAs || document.untitled || target.empty()) {
        std::vector<wchar_t> fileName(32768, L'\0');  // long paths
        if (!document.path.empty()) {
            wcsncpy_s(fileName.data(), fileName.size(), document.path.c_str(), _TRUNCATE);
        } else if (document.title.rfind(L"Untitled-", 0) != 0) {
            wcsncpy_s(fileName.data(), fileName.size(), document.title.c_str(), _TRUNCATE);
        }

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = g_app.window;
        dialog.lpstrFilter = L"All files\0*.*\0Text files\0*.txt;*.md;*.log;*.json;*.cpp;*.h\0";
        dialog.lpstrFile = fileName.data();
        dialog.nMaxFile = static_cast<DWORD>(fileName.size());
        dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        dialog.lpstrDefExt = L"txt";
        if (!GetSaveFileNameW(&dialog)) {
            return false;
        }
        target = fileName.data();
    }

    // Another program changed the file since we loaded or saved it.
    if (!document.untitled && document.fileTimestamp != 0 && SamePath(document.path, target)) {
        const uint64_t onDisk = FileTimestamp(target);
        if (onDisk != 0 && onDisk != document.fileTimestamp) {
            const std::wstring question = L"“" + document.title + L"”在打开后已被其他程序修改。\n\n"
                L"是：用当前内容覆盖磁盘上的文件\n否：不保存（可先用“重新加载”或比较功能查看差异）";
            if (MessageBoxW(g_app.window, question.c_str(), kWindowTitle, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return false;
        }
    }

    const std::wstring text = GetWindowTextValue(document.editor);
    if (document.encoding == kEncodingAnsi && !IsAnsiRepresentable(text)) {
        const int choice = MessageBoxW(g_app.window,
            L"当前内容包含 ANSI 编码无法表示的字符，按 ANSI 保存会变成“?”。\n"
            L"选择“是”改用 UTF-8 保存，“否”仍按 ANSI 保存。",
            kWindowTitle, MB_YESNOCANCEL | MB_ICONWARNING);
        if (choice == IDCANCEL) return false;
        if (choice == IDYES) document.encoding = 0;
    }
    if (!WriteTextFile(target, text, document.encoding, document.lineEnding)) {
        MessageBoxW(g_app.window, L"文件保存失败，请检查路径和权限。", kWindowTitle, MB_ICONERROR);
        return false;
    }

    const bool pathChanged = !SamePath(document.path, target);
    document.path = target.wstring();
    document.title = FileTitle(target);
    document.untitled = false;
    document.modified = false;
    document.recoveryDirty = true;
    document.fileTimestamp = FileTimestamp(target);
    SendMessageW(document.editor, EM_SETMODIFY, FALSE, 0);
    if (pathChanged && document.language == kLanguageAuto) HighlightDocument(document);
    UpdateTabTitle(index);
    UpdateStatusBar();
    g_app.stateDirty = true;
    // The file itself is saved; a failing session/recovery write must not
    // make closing the tab or the app think the save failed.
    if (!PersistAll(false)) ShowNotice(L"文件已保存，但会话数据写入失败（检查磁盘空间或数据目录权限）");
    return true;
}

bool SaveAllDocuments() {
    bool saved = true;
    for (int index = 0; index < static_cast<int>(g_app.documents.size()); ++index) {
        if (g_app.documents[static_cast<size_t>(index)].modified && !SaveDocument(index, false)) {
            saved = false;
            break;
        }
    }
    if (saved) ShowNotice(L"已保存全部标签");
    return saved;
}

bool OpenLogViewer(const std::wstring& path);

bool OpenFileInNewTab(const fs::path& path) {
    for (size_t index = 0; index < g_app.documents.size(); ++index) {
        const TabDocument& existing = g_app.documents[index];
        if (!existing.untitled && SamePath(existing.path, path)) {
            SelectTab(static_cast<int>(index));
            return true;
        }
    }

    std::error_code sizeError;
    const uintmax_t bytes = fs::file_size(path, sizeError);
    if (!sizeError && bytes > (100ull << 20)) {
        const std::wstring question = L"“" + path.filename().wstring() + L"”有 " + std::to_wstring(bytes >> 20) +
            L" MB。\n\n是：用日志查看器打开（只读，不整体载入内存，支持筛选和实时跟踪）\n否：进入大文件编辑模式（关闭内存密集型撤销，仍可编辑和保存）";
        const int choice = MessageBoxW(g_app.window, question.c_str(), kWindowTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
        if (choice == IDCANCEL) return false;
        if (choice == IDYES) return OpenLogViewer(path.wstring());
    }

    std::wstring text;
    DocumentFormat format;
    if (!ReadTextFile(path, text, &format.encoding, &format.lineEnding)) {
        MessageBoxW(g_app.window, L"文件打开失败，请检查路径和权限。", kWindowTitle, MB_ICONERROR);
        return false;
    }
    if (HasNulCharacter(text)) {
        MessageBoxW(g_app.window, (path.filename().wstring() + L"\n\n" + kNulFileMessage).c_str(), kWindowTitle, MB_ICONWARNING);
        return false;
    }
    format.fileTimestamp = FileTimestamp(path);

    const int index = CreateDocument(path.wstring(), FileTitle(path), std::move(text),
        false, false, 0, 0, 0, nullptr, format);
    if (index < 0) {
        return false;
    }
    SelectTab(index);
    PersistAll(false);
    return true;
}

void NewDocument() {
    const int index = CreateDocument({}, {}, {}, true, false, 0, 0, 0);
    if (index >= 0) {
        SelectTab(index);
        PersistAll(false);
    }
}

bool PrepareDocumentClose(TabDocument& document) {
    if (!document.modified) return true;
    if (g_app.closeBehavior == CloseBehavior::Direct) return true;
    if (g_app.closeBehavior == CloseBehavior::Save) {
        const int index = static_cast<int>(&document - g_app.documents.data());
        return SaveDocument(index, false);
    }

    const int choice = MessageBoxW(g_app.window,
        L"当前标签还有未保存修改，是否保存？\n选择“否”会直接关闭并丢弃修改。",
        kWindowTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return false;
    if (choice == IDYES) {
        const int index = static_cast<int>(&document - g_app.documents.data());
        return SaveDocument(index, false);
    }
    return true;
}

void RememberClosedTab(TabDocument& document) {
    CaptureEditorState(document);
    ClosedTab closed;
    closed.path = document.path;
    closed.title = document.title;
    closed.text = GetWindowTextValue(document.editor);
    closed.untitled = document.untitled;
    closed.modified = document.modified;
    closed.pinned = document.pinned;
    closed.caret = document.caret;
    closed.anchor = document.anchor;
    closed.firstVisibleLine = document.firstVisibleLine;
    closed.encoding = document.encoding;
    closed.lineEnding = document.lineEnding;
    closed.language = document.language;
    closed.fileTimestamp = document.fileTimestamp;
    closed.bookmarks = document.bookmarks;
    g_app.closedTabs.insert(g_app.closedTabs.begin(), std::move(closed));
    constexpr size_t kClosedTabHistoryLimit = 30;
    if (g_app.closedTabs.size() > kClosedTabHistoryLimit) {
        g_app.closedTabs.resize(kClosedTabHistoryLimit);
    }
}

void ReopenClosedDocument() {
    if (g_app.closedTabs.empty()) return;
    ClosedTab closed = std::move(g_app.closedTabs.front());
    g_app.closedTabs.erase(g_app.closedTabs.begin());
    DocumentFormat format;
    format.encoding = closed.encoding;
    format.lineEnding = closed.lineEnding;
    format.language = closed.language;
    format.fileTimestamp = closed.fileTimestamp;
    if (!closed.modified && !closed.untitled && !closed.path.empty()) {
        std::wstring currentText;
        int encoding = format.encoding;
        int lineEnding = format.lineEnding;
        if (ReadTextFile(closed.path, currentText, &encoding, &lineEnding)) {
            closed.text = std::move(currentText);
            format.encoding = encoding;
            format.lineEnding = lineEnding;
            format.fileTimestamp = FileTimestamp(closed.path);
        }
    }
    const int index = CreateDocument(closed.path, closed.title, std::move(closed.text), closed.untitled,
        closed.modified, closed.caret, closed.anchor, closed.firstVisibleLine, nullptr, format);
    if (index < 0) return;
    g_app.documents[static_cast<size_t>(index)].bookmarks = std::move(closed.bookmarks);
    g_app.documents[static_cast<size_t>(index)].pinned = closed.pinned;
    NormalizeBookmarks(g_app.documents[static_cast<size_t>(index)]);
    SelectTab(index);
    PersistAll(false);
}

bool CloseDocument(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) {
        return false;
    }

    TabDocument& document = g_app.documents[static_cast<size_t>(index)];
    if (!PrepareDocumentClose(document)) return false;

    RememberClosedTab(document);

    const fs::path snapshot = document.snapshotPath;
    DestroyWindow(document.editor);
    g_app.documents.erase(g_app.documents.begin() + index);
    if (g_app.tabs) InvalidateRect(g_app.tabs, nullptr, FALSE);

    std::error_code error;
    fs::remove(snapshot, error);

    if (g_app.documents.empty()) {
        g_app.activeIndex = -1;
        NewDocument();
        return true;
    }

    const int nextIndex = std::min(index, static_cast<int>(g_app.documents.size()) - 1);
    g_app.activeIndex = -1;
    SelectTab(nextIndex);
    g_app.stateDirty = true;
    PersistAll(false);
    return true;
}

bool CloseAllDocuments() {
    while (g_app.documents.size() > 1) {
        if (!CloseDocument(static_cast<int>(g_app.documents.size()) - 1)) return false;
    }
    if (!g_app.documents.empty() && !CloseDocument(0)) return false;
    return true;
}

bool ConfirmCloseCompareWindows();

bool PrepareApplicationClose() {
    if (!ConfirmCloseCompareWindows()) return false;
    std::vector<int> modified;
    for (size_t index = 0; index < g_app.documents.size(); ++index) {
        if (g_app.documents[index].modified) modified.push_back(static_cast<int>(index));
    }
    if (modified.empty() || g_app.closeBehavior == CloseBehavior::Direct) return true;

    if (g_app.closeBehavior == CloseBehavior::Save) {
        for (const int index : modified) {
            if (!SaveDocument(index, false)) return false;
        }
        return true;
    }

    const int choice = MessageBoxW(g_app.window,
        L"有未保存的标签。是否保存全部修改后退出？\n选择“否”会退出，但仍保留会话恢复内容。",
        kWindowTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return false;
    if (choice == IDYES) {
        for (const int index : modified) {
            if (!SaveDocument(index, false)) return false;
        }
    }
    return true;
}

void OpenFileDialog() {
    // Room for many selected files (the names come back as one list).
    std::vector<wchar_t> buffer(1 << 16, L'\0');
    wchar_t* fileName = buffer.data();
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_app.window;
    dialog.lpstrFilter = L"All files\0*.*\0Text files\0*.txt;*.md;*.log;*.json;*.cpp;*.h\0";
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;

    if (!GetOpenFileNameW(&dialog)) {
        return;
    }

    if (dialog.Flags & OFN_ALLOWMULTISELECT) {
        const wchar_t* cursor = fileName;
        const std::wstring directory = cursor;
        cursor += directory.size() + 1;
        if (*cursor == L'\0') {
            OpenFileInNewTab(directory);
            return;
        }
        while (*cursor != L'\0') {
            const std::wstring fileNamePart = cursor;
            OpenFileInNewTab(fs::path(directory) / fileNamePart);
            cursor += fileNamePart.size() + 1;
        }
        return;
    }

    OpenFileInNewTab(fileName);
}

void SetMenuState() {
    const bool hasActive = g_app.activeIndex >= 0 &&
        g_app.activeIndex < static_cast<int>(g_app.documents.size());
    HMENU menu = g_app.mainMenu;
    if (!menu) {
        return;
    }
    EnableMenuItem(menu, kIdFileSave, MF_BYCOMMAND | (hasActive ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, kIdFileSaveAs, MF_BYCOMMAND | (hasActive ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, kIdFileClose, MF_BYCOMMAND | (hasActive ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, kIdFileSaveAll, MF_BYCOMMAND | (hasActive ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, kIdFileCloseAll, MF_BYCOMMAND | (hasActive ? MF_ENABLED : MF_GRAYED));
    EnableMenuItem(menu, kIdFileReopenClosed, MF_BYCOMMAND |
        (g_app.closedTabs.empty() ? MF_GRAYED : MF_ENABLED));
    if (g_app.themeMenu) {
        CheckMenuRadioItem(g_app.themeMenu, kIdThemeDark, kIdThemeBlue,
            kIdThemeDark + static_cast<int>(g_app.theme), MF_BYCOMMAND);
    }
    CheckMenuItem(menu, kIdToolbarLineNumbers, MF_BYCOMMAND |
        (g_app.showLineNumbers ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, kIdToolbarWrap, MF_BYCOMMAND |
        (g_app.wordWrap ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, kIdViewWhitespace, MF_BYCOMMAND |
        (g_app.showWhitespace ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, kIdViewIndentGuides, MF_BYCOMMAND |
        (g_app.showIndentGuides ? MF_CHECKED : MF_UNCHECKED));
    if (g_app.lineSpacingMenu) {
        for (int id = kIdLineSpacingSingle; id <= kIdLineSpacingCustom; ++id) {
            CheckMenuItem(g_app.lineSpacingMenu, id, MF_BYCOMMAND | MF_UNCHECKED);
        }
        int selected = 0;
        switch (g_app.lineSpacingPercent) {
        case 100: selected = kIdLineSpacingSingle; break;
        case 115: selected = kIdLineSpacing115; break;
        case 150: selected = kIdLineSpacing150; break;
        case 200: selected = kIdLineSpacing200; break;
        default: selected = 0; break;
        }
        if (selected != 0) CheckMenuItem(g_app.lineSpacingMenu, selected, MF_BYCOMMAND | MF_CHECKED);
    }
    CheckMenuItem(menu, kIdTopSpacingCustom, MF_BYCOMMAND |
        (g_app.topSpacingPixels > 0 ? MF_CHECKED : MF_UNCHECKED));
    CheckDocumentMenuItems(menu);
    CheckMenuItem(menu, kIdTogglePreview, MF_BYCOMMAND | (g_app.previewTextTools ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuRadioItem(menu, kIdOutputReplace, kIdOutputInsertBelow, kIdOutputReplace + g_app.toolOutputMode, MF_BYCOMMAND);
    CheckMenuItem(menu, kIdToggleSearchPanel, MF_BYCOMMAND | (g_app.searchPanelVisible ? MF_CHECKED : MF_UNCHECKED));
}

HMENU CreateApplicationMenu() {
    HMENU menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU edit = CreatePopupMenu();
    HMENU data = CreatePopupMenu();
    HMENU search = CreatePopupMenu();
    HMENU view = CreatePopupMenu();
    HMENU lineSpacing = CreatePopupMenu();
    HMENU encoding = CreatePopupMenu();
    HMENU language = CreatePopupMenu();
    HMENU run = CreatePopupMenu();
    HMENU tools = CreatePopupMenu();
    HMENU settings = CreatePopupMenu();
    HMENU theme = CreatePopupMenu();
    HMENU help = CreatePopupMenu();

    AppendMenuW(file, MF_STRING, kIdFileNew, L"新建\tCtrl+N");
    AppendMenuW(file, MF_STRING, kIdFileOpen, L"打开...\tCtrl+O");
    AppendMenuW(file, MF_STRING, kIdFileSave, L"保存\tCtrl+S");
    AppendMenuW(file, MF_STRING, kIdFileSaveAs, L"另存为...\tCtrl+Shift+S");
    AppendMenuW(file, MF_STRING, kIdFileSaveAll, L"保存全部\tCtrl+Alt+S");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, kIdFileRename, L"重命名...\tF2");
    AppendMenuW(file, MF_STRING, kIdFileClose, L"关闭标签\tCtrl+W");
    AppendMenuW(file, MF_STRING, kIdFileCloseAll, L"关闭全部标签\tCtrl+Shift+W");
    AppendMenuW(file, MF_STRING, kIdFileReopenClosed, L"重新打开关闭的标签\tCtrl+Shift+T");
    AppendMenuW(file, MF_STRING, kIdFileExit, L"退出");

    AppendMenuW(edit, MF_STRING, kIdEditUndo, L"撤销\tCtrl+Z");
    AppendMenuW(edit, MF_STRING, kIdEditRedo, L"重做\tCtrl+Y");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, kIdEditCut, L"剪切\tCtrl+X");
    AppendMenuW(edit, MF_STRING, kIdEditCopy, L"复制\tCtrl+C");
    AppendMenuW(edit, MF_STRING, kIdEditPaste, L"粘贴\tCtrl+V");
    AppendMenuW(edit, MF_STRING, kIdEditSelectAll, L"全选\tCtrl+A");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, kIdPrefixSuffix, L"前缀与后缀批量处理...");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, kIdMultiCaretNext, L"添加下一行光标\tCtrl+Alt+Down");
    AppendMenuW(edit, MF_STRING, kIdRectangleSelection, L"矩形选择（按当前选区创建）");
    AppendMenuW(edit, MF_STRING, kIdClearMultiCaret, L"清除多光标");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, kIdMacroToggle, L"开始/停止宏录制");
    AppendMenuW(edit, MF_STRING, kIdMacroPlay, L"播放最近宏");
    AppendMenuW(edit, MF_STRING, kIdMacroClear, L"清除最近宏");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendTextToolMenus(edit);
    AppendDataGeneratorMenus(data);

    AppendMenuW(search, MF_STRING, kIdToolbarFind, L"查找\tCtrl+F");
    AppendMenuW(search, MF_STRING, kIdToolbarReplace, L"替换\tCtrl+H");
    AppendMenuW(search, MF_STRING, kIdFindNext, L"查找下一个\tF3");
    AppendMenuW(search, MF_STRING, kIdFindPrevious, L"查找上一个\tShift+F3");
    AppendMenuW(search, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(search, MF_STRING, kIdSearchCurrentFile, L"在当前文件中查找...");
    AppendMenuW(search, MF_STRING, kIdSearchOpenFiles, L"在所有打开文件中查找...");
    AppendMenuW(search, MF_STRING, kIdSearchInFolder, L"在文件夹中查找...\tCtrl+Shift+F");
    AppendMenuW(search, MF_STRING, kIdReplaceInFolder, L"在文件夹中替换...\tCtrl+Shift+H");
    AppendMenuW(search, MF_STRING, kIdSearchFileNames, L"搜索文件名...");
    AppendMenuW(search, MF_STRING, kIdSearchRegex, L"正则表达式搜索...");
    AppendMenuW(search, MF_SEPARATOR, 0, nullptr);
    g_app.searchHistoryMenu = CreatePopupMenu();
    AppendMenuW(g_app.searchHistoryMenu, MF_STRING | MF_GRAYED, 0, L"（暂无搜索历史）");
    AppendMenuW(search, MF_POPUP, reinterpret_cast<UINT_PTR>(g_app.searchHistoryMenu), L"搜索历史");
    HMENU results = CreatePopupMenu();
    AppendMenuW(results, MF_STRING, kIdToggleSearchPanel, L"显示搜索面板");
    AppendMenuW(results, MF_STRING, kIdCopySearchResults, L"复制全部结果");
    AppendMenuW(results, MF_STRING, kIdExportSearchResults, L"导出搜索结果...");
    AppendMenuW(results, MF_STRING, kIdClearSearchResults, L"清空搜索结果");
    AppendMenuW(results, MF_STRING, kIdBookmarkSearchResult, L"将选中结果标记为书签");
    AppendMenuW(results, MF_STRING, kIdSearchErrorLog, L"查看错误日志");
    AppendMenuW(results, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(results, MF_STRING, kIdUndoReplaceInFiles, L"撤销上次在文件中替换...");
    AppendMenuW(search, MF_POPUP, reinterpret_cast<UINT_PTR>(results), L"搜索结果管理");
    AppendMenuW(search, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(search, MF_STRING, kIdToolbarGoto, L"跳转到行\tCtrl+G");
    AppendMenuW(view, MF_STRING, kIdToolbarLineNumbers, L"行号");
    AppendMenuW(view, MF_STRING, kIdToolbarWrap, L"自动换行");
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view, MF_STRING, kIdViewToggleBookmark, L"切换书签\tCtrl+F2");
    AppendMenuW(view, MF_STRING, kIdViewNextBookmark, L"下一个书签\tF4");
    AppendMenuW(view, MF_STRING, kIdViewPreviousBookmark, L"上一个书签\tShift+F4");
    AppendMenuW(view, MF_STRING, kIdViewClearBookmarks, L"清除当前文档书签");
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view, MF_STRING, kIdViewWhitespace, L"显示空格和制表符");
    AppendMenuW(view, MF_STRING, kIdViewIndentGuides, L"显示缩进参考线");
    AppendMenuW(view, MF_STRING, kIdViewFoldCurrent, L"折叠/展开当前代码块");
    AppendMenuW(view, MF_STRING, kIdViewUnfoldAll, L"展开全部代码块");
    AppendMenuW(lineSpacing, MF_STRING, kIdLineSpacingSingle, L"1.0 倍");
    AppendMenuW(lineSpacing, MF_STRING, kIdLineSpacing115, L"1.15 倍");
    AppendMenuW(lineSpacing, MF_STRING, kIdLineSpacing150, L"1.5 倍");
    AppendMenuW(lineSpacing, MF_STRING, kIdLineSpacing200, L"2.0 倍");
    AppendMenuW(lineSpacing, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(lineSpacing, MF_STRING, kIdLineSpacingCustom, L"自定义...");
    g_app.lineSpacingMenu = lineSpacing;
    AppendMenuW(view, MF_POPUP, reinterpret_cast<UINT_PTR>(lineSpacing), L"行距");
    AppendMenuW(view, MF_STRING, kIdTopSpacingCustom, L"顶部间距自定义...");
    AppendEncodingItems(encoding);
    for (int index = 0; index < kLanguageCount; ++index) {
        AppendMenuW(language, MF_STRING, kIdLanguageFirst + index, kLanguageNames[index]);
    }
    AppendMenuW(run, MF_STRING, kIdToolbarRun, L"运行当前文件\tF5");
    AppendMenuW(tools, MF_STRING, kIdToggleToolbox, L"开发者工具箱\tCtrl+Shift+D");
    AppendMenuW(tools, MF_STRING, kIdToolbarFormat, L"格式化 JSON\tShift+Alt+F");
    AppendMenuW(tools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(tools, MF_STRING, kIdCompareWithFile, L"比较当前文件与...");
    AppendMenuW(tools, MF_STRING, kIdCompareFiles, L"比较两个文件...");
    AppendMenuW(tools, MF_STRING, kIdCompareStructure, L"结构化比较 (JSON/CSV/YAML)...");
    AppendMenuW(tools, MF_STRING, kIdCompareFolders, L"比较文件夹...");
    AppendMenuW(tools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(tools, MF_STRING, kIdOpenLogViewer, L"日志查看器（大文件）...");
    AppendMenuW(theme, MF_STRING, kIdThemeDark, L"深色");
    AppendMenuW(theme, MF_STRING, kIdThemeLight, L"浅色");
    AppendMenuW(theme, MF_STRING, kIdThemeGreen, L"护眼绿");
    AppendMenuW(theme, MF_STRING, kIdThemeBlue, L"暗夜蓝");
    g_app.themeMenu = theme;
    AppendMenuW(settings, MF_POPUP, reinterpret_cast<UINT_PTR>(theme), L"主题");
    AppendMenuW(settings, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(settings, MF_STRING, kIdToolbarSettings, L"偏好设置...\tCtrl+,");
    AppendMenuW(settings, MF_STRING, kIdSettingsShortcuts, L"快捷键设置...\tCtrl+K");
    AppendMenuW(help, MF_STRING, kIdHelpAbout, L"关于 MCnotepad");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"文件(F)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), L"编辑(E)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(data), L"数据(D)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(search), L"搜索(S)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"视图(V)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(encoding), L"编码(N)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(language), L"语言(L)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(run), L"运行(R)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), L"工具(T)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(settings), L"设置(O)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"帮助(H)");
    return menu;
}

struct SessionEntry {
    std::wstring path;
    std::wstring title;
    fs::path snapshot;
    int64_t caret = 0;
    int64_t anchor = 0;
    int firstLine = 0;
    int horizontalScroll = 0;
    bool modified = false;
    bool untitled = false;
    bool pinned = false;
    std::vector<int> bookmarks;
    DocumentFormat format;
};

void LoadPersistedSession() {
    InitializeShortcutBindings();
    std::ifstream input(g_app.sessionPath, std::ios::binary);
    if (!input) {
        ApplyEditorFont();
        NewDocument();
        return;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    std::string line;
    int activeIndex = 0;
    int persistedTheme = static_cast<int>(g_app.theme);
    int persistedLineSpacing = g_app.lineSpacingPercent;
    int persistedTopSpacing = g_app.topSpacingPixels;
    int persistedShowLineNumbers = g_app.showLineNumbers ? 1 : 0;
    int persistedWordWrap = g_app.wordWrap ? 1 : 0;
    int persistedSidebarVisible = g_app.sidebarVisible ? 1 : 0;
    int persistedFontSize = g_app.fontSize;
    int persistedLanguage = g_app.languageIndex;
    int persistedEncoding = g_app.encodingIndex;
    int persistedLineEnding = g_app.lineEndingIndex;
    int persistedShowWhitespace = g_app.showWhitespace ? 1 : 0;
    int persistedShowIndentGuides = g_app.showIndentGuides ? 1 : 0;
    int persistedStartup = g_app.launchAtStartup ? 1 : 0;
    int persistedCloseBehavior = static_cast<int>(g_app.closeBehavior);
    int persistedWindowCloseBehavior = static_cast<int>(g_app.windowCloseBehavior);
    int persistedMinimizeToTray = g_app.minimizeToTray ? 1 : 0;
    int persistedShowTrayIcon = g_app.showTrayIcon ? 1 : 0;
    std::wstring persistedFontFamily = g_app.fontFamily;
    std::vector<SessionEntry> entries;
    // Only a session file that was read completely may decide which recovery
    // snapshots are orphans; otherwise unsaved work would be deleted.
    bool sawHeader = false;
    size_t tabLines = 0;

    while (std::getline(buffer, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::vector<std::string> parts = SplitTabs(line);
        if (parts.empty()) {
            continue;
        }
        if (parts[0] == "MCSESSION") sawHeader = true;
        if (parts[0] == "TAB") ++tabLines;

        if (parts[0] == "ACTIVE" && parts.size() >= 2) {
            ParseInt(parts[1], activeIndex);
        } else if (parts[0] == "THEME" && parts.size() >= 2) {
            ParseInt(parts[1], persistedTheme);
        } else if (parts[0] == "LINE_SPACING" && parts.size() >= 2) {
            ParseInt(parts[1], persistedLineSpacing);
        } else if (parts[0] == "TOP_SPACING" && parts.size() >= 2) {
            ParseInt(parts[1], persistedTopSpacing);
        } else if (parts[0] == "OPTIONS" && parts.size() >= 9) {
            ParseInt(parts[1], persistedShowLineNumbers);
            ParseInt(parts[2], persistedWordWrap);
            ParseInt(parts[3], persistedSidebarVisible);
            ParseInt(parts[4], persistedFontSize);
            persistedFontFamily = Utf8ToWide(HexDecode(parts[5]));
            ParseInt(parts[6], persistedLanguage);
            ParseInt(parts[7], persistedEncoding);
            ParseInt(parts[8], persistedLineEnding);
            if (parts.size() >= 10) ParseInt(parts[9], persistedShowWhitespace);
            if (parts.size() >= 11) ParseInt(parts[10], persistedShowIndentGuides);
        } else if (parts[0] == "TOOLBOX" && parts.size() >= 5) {
            int visible = 0, width = 380, mask = 0x3FF, tool = 0;
            ParseInt(parts[1], visible);
            ParseInt(parts[2], width);
            ParseInt(parts[3], mask);
            ParseInt(parts[4], tool);
            g_app.toolboxVisible = visible != 0;
            g_app.toolboxWidth = std::clamp(width, 260, 900);
            g_app.toolboxMask = static_cast<unsigned>(mask) & 0x3FF;
            if (!g_app.toolboxMask) g_app.toolboxMask = 0x3FF;
            g_app.toolboxTool = std::clamp(tool, 0, 9);
        } else if (parts[0] == "PREFERENCES" && parts.size() >= 3) {
            ParseInt(parts[1], persistedStartup);
            ParseInt(parts[2], persistedCloseBehavior);
            int preview = 1;
            if (parts.size() >= 4 && ParseInt(parts[3], preview)) g_app.previewTextTools = preview != 0;
            int outputMode = 1;
            if (parts.size() >= 5 && ParseInt(parts[4], outputMode)) g_app.toolOutputMode = std::clamp(outputMode, 0, 2);
            if (parts.size() >= 6) ParseInt(parts[5], persistedWindowCloseBehavior);
            if (parts.size() >= 7) ParseInt(parts[6], persistedMinimizeToTray);
            if (parts.size() >= 8) ParseInt(parts[7], persistedShowTrayIcon);
        } else if (parts[0] == "SHORTCUTS" && parts.size() >= 3) {
            // Pairs are stored as command + hex encoded UTF-8 text. Unknown
            // commands are ignored so newer session files remain compatible.
            for (size_t part = 1; part + 1 < parts.size(); part += 2) {
                int command = 0;
                if (!ParseInt(parts[part], command)) continue;
                const std::wstring text = Utf8ToWide(HexDecode(parts[part + 1]));
                for (ShortcutBinding& binding : g_shortcuts) {
                    if (binding.command == command) {
                        binding.text = text;
                        break;
                    }
                }
            }
        } else if (parts[0] == "WINDOW" && parts.size() >= 6) {
            int left = 0, top = 0, width = 0, height = 0, maximized = 0;
            if (ParseInt(parts[1], left) && ParseInt(parts[2], top) &&
                ParseInt(parts[3], width) && ParseInt(parts[4], height) && width >= 300 && height >= 200) {
                ParseInt(parts[5], maximized);
                g_app.savedWindowRect = RECT{left, top, left + width, top + height};
                g_app.savedWindowMaximized = maximized != 0;
                g_app.hasSavedWindow = true;
            }
        } else if (parts[0] == "TAB" && parts.size() >= 12) {
            SessionEntry entry;
            int sourceModified = 0;
            int untitled = 0;
            int64_t timestamp = 0;
            ParseInt64(parts[5], entry.caret);
            ParseInt64(parts[6], entry.anchor);
            ParseInt(parts[7], entry.firstLine);
            ParseInt(parts[8], entry.horizontalScroll);
            ParseInt(parts[9], sourceModified);
            ParseInt(parts[10], untitled);
            ParseInt64(parts[11], timestamp);
            entry.modified = sourceModified != 0;
            entry.untitled = untitled != 0;
            entry.format.fileTimestamp = static_cast<uint64_t>(timestamp);
            if (parts.size() >= 15) {  // per-tab format, added after 0.1.0
                ParseInt(parts[12], entry.format.encoding);
                ParseInt(parts[13], entry.format.lineEnding);
                ParseInt(parts[14], entry.format.language);
            }
            if (parts.size() >= 16 && !parts[15].empty()) {
                size_t start = 0;
                while (start < parts[15].size()) {
                    const size_t comma = parts[15].find(',', start);
                    const std::string_view token(parts[15].data() + start,
                        comma == std::string::npos ? parts[15].size() - start : comma - start);
                    int bookmark = 0;
                    if (ParseInt(token, bookmark) && bookmark > 0) entry.bookmarks.push_back(bookmark);
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
                std::sort(entry.bookmarks.begin(), entry.bookmarks.end());
                entry.bookmarks.erase(std::unique(entry.bookmarks.begin(), entry.bookmarks.end()), entry.bookmarks.end());
            }
            if (parts.size() >= 17) {
                int pinned = 0;
                ParseInt(parts[16], pinned);
                entry.pinned = pinned != 0;
            }
            entry.path = Utf8ToWide(HexDecode(parts[2]));
            entry.title = Utf8ToWide(HexDecode(parts[3]));
            entry.snapshot = fs::path(Utf8ToWide(HexDecode(parts[4])));
            entries.push_back(std::move(entry));
        }
    }
    const bool sessionComplete = sawHeader && tabLines == entries.size();
    if (!sessionComplete) {
        // Keep the unreadable file for inspection; the next save replaces session.dat.
        std::error_code copyError;
        fs::copy_file(g_app.sessionPath, fs::path(g_app.sessionPath).replace_extension(L".broken"),
            fs::copy_options::overwrite_existing, copyError);
    }

    g_app.theme = static_cast<ThemeKind>(std::clamp(persistedTheme, 0, 3));
    g_app.lineSpacingPercent = std::clamp(persistedLineSpacing, 50, 300);
    g_app.topSpacingPixels = std::clamp(persistedTopSpacing, 0, 40);
    g_app.showLineNumbers = persistedShowLineNumbers != 0;
    g_app.wordWrap = persistedWordWrap != 0;
    g_app.showWhitespace = persistedShowWhitespace != 0;
    g_app.showIndentGuides = persistedShowIndentGuides != 0;
    g_app.sidebarVisible = false;
    g_app.fontSize = std::clamp(persistedFontSize, 10, 32);
    if (!persistedFontFamily.empty()) g_app.fontFamily = persistedFontFamily;
    g_app.languageIndex = std::clamp(persistedLanguage, 0, kLanguageCount - 1);
    g_app.encodingIndex = std::clamp(persistedEncoding, 0, kEncodingCount - 1);
    g_app.lineEndingIndex = std::clamp(persistedLineEnding, 0, kLineEndingCount - 1);
    g_app.launchAtStartup = persistedStartup != 0;
    g_app.closeBehavior = static_cast<CloseBehavior>(std::clamp(persistedCloseBehavior, 0, 2));
    g_app.windowCloseBehavior = static_cast<WindowCloseBehavior>(std::clamp(persistedWindowCloseBehavior, 0, 2));
    g_app.minimizeToTray = persistedMinimizeToTray != 0;
    g_app.showTrayIcon = persistedShowTrayIcon != 0;

    // Font and theme first, so every editor below is created with them.
    ApplyEditorFont();
    ApplyThemeToEditors(false);

    g_app.loading = true;
    for (SessionEntry& entry : entries) {
        std::wstring text;
        bool haveText = false;
        std::error_code error;
        const bool hasFile = !entry.untitled && !entry.path.empty() && fs::exists(entry.path, error);
        if (!entry.modified && hasFile) {
            // Nothing unsaved: the file on disk is the truth (it may have been
            // edited elsewhere since last time). Re-detect its format too.
            int encoding = 0;
            int lineEnding = 0;
            if (ReadTextFile(entry.path, text, &encoding, &lineEnding)) {
                haveText = true;
                entry.format.encoding = encoding;
                entry.format.lineEnding = lineEnding;
                entry.format.fileTimestamp = FileTimestamp(entry.path);
            }
        }
        if (!haveText && !entry.snapshot.empty() && fs::exists(entry.snapshot, error)) {
            haveText = ReadTextFile(entry.snapshot, text);
        }
        if (!haveText && hasFile) {
            int encoding = 0;
            int lineEnding = 0;
            if (ReadTextFile(entry.path, text, &encoding, &lineEnding)) {
                if (entry.format.encoding < 0) entry.format.encoding = encoding;
                if (entry.format.lineEnding < 0) entry.format.lineEnding = lineEnding;
                entry.format.fileTimestamp = FileTimestamp(entry.path);
            }
        }
        // Old sessions have no per-tab format: guess it from the file itself.
        if ((entry.format.encoding < 0 || entry.format.lineEnding < 0) && hasFile) {
            std::wstring ignored;
            int encoding = 0;
            int lineEnding = 0;
            if (ReadTextFile(entry.path, ignored, &encoding, &lineEnding)) {
                if (entry.format.encoding < 0) entry.format.encoding = encoding;
                if (entry.format.lineEnding < 0) entry.format.lineEnding = lineEnding;
            }
        }

        if (HasNulCharacter(text)) continue;  // file is no longer text; leave it untouched on disk
        const int index = CreateDocument(entry.path, entry.title, std::move(text), entry.untitled,
            entry.modified, entry.caret, entry.anchor, entry.firstLine, &entry.snapshot, entry.format);
        if (index >= 0) {
            TabDocument& created = g_app.documents[static_cast<size_t>(index)];
            created.horizontalScroll = entry.horizontalScroll;
            created.pinned = entry.pinned;
            created.bookmarks = std::move(entry.bookmarks);
            NormalizeBookmarks(created);
            // A tab that had to get a fresh snapshot (missing or shared by an
            // earlier tab in an old session) must write it on the next autosave.
            created.recoveryDirty = !SamePath(created.snapshotPath, entry.snapshot);
        }
    }
    g_app.loading = false;
    if (sessionComplete) RemoveOrphanSnapshots();
    LayoutChildren(g_app.window);

    if (g_app.documents.empty()) {
        NewDocument();
        return;
    }

    SelectTab(std::clamp(activeIndex, 0, static_cast<int>(g_app.documents.size()) - 1));
    g_app.stateDirty = false;
}

void InitializeStorage() {
    g_app.dataDirectory = GetDataDirectory();
    g_app.recoveryDirectory = g_app.dataDirectory / L"recovery";
    g_app.sessionPath = g_app.dataDirectory / L"session.dat";
    g_app.runningMarkerPath = g_app.dataDirectory / L"running.lock";

    EnsureDirectory(g_app.dataDirectory);
    EnsureDirectory(g_app.recoveryDirectory);
    g_app.launchAtStartup = IsStartupRegistrationEnabled();
    g_app.previousRunWasUnclean = fs::exists(g_app.runningMarkerPath);
    CreateRunningMarker();
}

void CompareFiles(const std::wstring& leftPath, const std::wstring& rightPath, bool structure = false);
void CompareFolderPaths(const std::wstring& left, const std::wstring& right);
bool OpenLogViewer(const std::wstring& path);

void OpenCommandLineFiles() {
    int argumentCount = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments == nullptr) {
        return;
    }

    // Paths are made absolute now: the file dialogs change the current
    // directory, and a relative path saved later would land somewhere else.
    const auto absolute = [](const std::wstring& path) {
        std::error_code error;
        const fs::path full = fs::absolute(fs::path(path), error);
        return error ? path : full.lexically_normal().wstring();
    };
    for (int index = 1; index < argumentCount; ++index) {
        const std::wstring argument = arguments[index];
        if (argument == L"--new-instance" || argument == L"--portable") {
            continue;
        }
        if (argument == L"--log" && index + 1 < argumentCount) {
            OpenLogViewer(fs::absolute(fs::path(arguments[++index])).wstring());
            continue;
        }
        if ((argument == L"--diff" || argument == L"--diff-struct") && index + 2 < argumentCount) {
            const std::wstring left = absolute(arguments[index + 1]), right = absolute(arguments[index + 2]);
            index += 2;
            std::error_code error;
            if (fs::is_directory(left, error) && fs::is_directory(right, error)) CompareFolderPaths(left, right);
            else CompareFiles(left, right, argument == L"--diff-struct");
            continue;
        }
        if (!argument.empty() && argument[0] != L'-') {
            OpenFileInNewTab(fs::path(absolute(argument)));
        }
    }
    LocalFree(arguments);
}

HWND ActiveEditor() {
    if (g_app.activeIndex < 0 || g_app.activeIndex >= static_cast<int>(g_app.documents.size())) return nullptr;
    return g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor;
}

void ReportNotFound(const wchar_t* needle) {
    const std::wstring message = L"找不到“" + std::wstring(needle) + L"”";
    MessageBoxW(g_app.findDialog ? g_app.findDialog : g_app.window, message.c_str(), kWindowTitle, MB_ICONINFORMATION);
}

