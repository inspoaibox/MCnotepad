// Keys for the compare window: F8 / Shift+F8 next/previous difference,
// Ctrl+S saves the changed sides, F5 refreshes.
bool ToolWindowKey(HWND root, WPARAM key) {
    wchar_t klass[64]{};
    GetClassNameW(root, klass, 64);
    const bool shift = GetKeyState(VK_SHIFT) < 0, control = GetKeyState(VK_CONTROL) < 0;
    if (wcscmp(klass, L"MCnotepadCompare") == 0) {
        auto* compare = reinterpret_cast<CompareWindow*>(GetWindowLongPtrW(root, GWLP_USERDATA));
        if (!compare) return false;
        if (key == VK_F8 || key == VK_F7) { GoToHunk(*compare, compare->current + ((key == VK_F7 || shift) ? -1 : 1)); return true; }
        if (key == VK_F5) { SendMessageW(root, WM_COMMAND, kCmpRefresh, 0); return true; }
        if (control && key == 'S') {
            for (int s = 0; s < 2; ++s) if (compare->sides[s].dirty) SaveCompareSide(*compare, s);
            return true;
        }
    }
    if (wcscmp(klass, L"MCnotepadLog") == 0) return LogViewerKey(root, key);
    if (wcscmp(klass, L"MCnotepadFolderCompare") == 0 && key == VK_F5) {
        SendMessageW(root, WM_COMMAND, kFolderRefresh, 0);
        return true;
    }
    return false;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (g_app.findReplaceMessage != 0 && message == g_app.findReplaceMessage) {
        HandleFindReplaceMessage(lParam);
        return 0;
    }
    switch (message) {
    case WM_CREATE: {
        g_app.window = window;
        SetPropW(window, kDataKeyProperty, reinterpret_cast<HANDLE>(DataDirectoryKey()));
        g_app.mainMenu = CreateApplicationMenu();
        g_app.dpi = GetDpiForWindow(window);
        CreateUiFonts();
        GdiplusStartupInput gdiplusInput;
        GdiplusStartup(&g_app.gdiplusToken, &gdiplusInput, nullptr);
        LoadToolbarIconImages();
        g_app.editorBrush = CreateSolidBrush(CurrentPalette().editorBackground);
        g_app.toolbarBrush = CreateSolidBrush(CurrentPalette().toolbarBackground);
        g_app.sidebarBrush = CreateSolidBrush(CurrentPalette().panelBackground);
        g_app.windowBrush = CreateSolidBrush(CurrentPalette().windowBackground);
        g_app.controlBrush = CreateSolidBrush(CurrentPalette().controlBackground);
        g_app.statusBrush = CreateSolidBrush(CurrentPalette().statusBackground);

        CreateChrome(window);
        g_app.gutter = CreateWindowExW(0, L"MCGutter", nullptr,
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, window, nullptr, g_app.instance, nullptr);
        g_app.sidebarVisible = false;
        LoadLibraryW(L"Msftedit.dll");
        LoadPersistedSession();
        g_app.darkTheme = g_app.theme == ThemeKind::Dark || g_app.theme == ThemeKind::Blue;
        ApplyWindowFrameTheme(window);
        ApplyMenuColorMode();
        LoadSearchState();
        if (g_app.searchPanelVisible) EnsureSearchPanel();
        RefreshChrome();
        LayoutChildren(window);
        SetMenuState();
        SetTimer(window, kTimerAutosave, 2000, nullptr);

        return 0;
    }
    case WM_SIZE:
        LayoutChildren(window);
        if (wParam == SIZE_MINIMIZED && g_app.minimizeToTray && !g_app.closing) {
            HideMainWindowToTray();
        }
        return 0;
    case WM_TIMER:
        if (wParam == kTimerAutosave) {
            if (g_app.stateDirty) {
                PersistAll(false);
            }
            UpdateStatusBar();
        } else if (wParam == kTimerPreview) {
            KillTimer(window, kTimerPreview);
            RenderPreview(false);
        } else if (wParam == kTimerHighlight) {
            KillTimer(window, kTimerHighlight);
            if (const TabDocument* document = ActiveDocument()) {
                HighlightDocument(*document);
            }
        }
        return 0;
    case kMessageTrayIcon: {
        // Legacy shells pass the icon ID in wParam and the mouse event in
        // lParam. NOTIFYICON_VERSION_4 packs the event in LOWORD(lParam),
        // the icon ID in HIWORD(lParam), and uses wParam for coordinates.
        UINT trayEvent = 0;
        if (HIWORD(lParam) == kTrayIconId) {
            trayEvent = LOWORD(lParam);
        } else if (static_cast<UINT>(wParam) == kTrayIconId) {
            trayEvent = static_cast<UINT>(lParam);
        } else {
            return 0;
        }
        if (trayEvent == WM_LBUTTONDBLCLK || trayEvent == NIN_SELECT || trayEvent == NIN_KEYSELECT) {
            RestoreMainWindowFromTray();
            return 0;
        }
        // With NOTIFYICON_VERSION_4 Windows reports a tray right-click as
        // WM_CONTEXTMENU. Older shells use WM_RBUTTONUP, so accept both.
        if (trayEvent == WM_CONTEXTMENU || trayEvent == WM_RBUTTONUP) {
            HMENU trayMenu = CreatePopupMenu();
            AppendMenuW(trayMenu, MF_STRING, kIdTrayShow, L"显示 MCnotepad");
            AppendMenuW(trayMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(trayMenu, MF_STRING, kIdTrayExit, L"退出软件");
            POINT point{};
            GetCursorPos(&point);
            SetForegroundWindow(window);
            // Return the selected command directly. This remains reliable
            // while the main window is hidden, where routing WM_COMMAND back
            // through the owner window can be swallowed by some shells.
            const UINT command = TrackPopupMenu(trayMenu,
                TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
                point.x, point.y, 0, window, nullptr);
            DestroyMenu(trayMenu);
            PostMessageW(window, WM_NULL, 0, 0);
            if (command == kIdTrayShow) {
                RestoreMainWindowFromTray();
            } else if (command == kIdTrayExit) {
                g_app.windowCloseBehavior = WindowCloseBehavior::Exit;
                HandleMainWindowClose(true);
            }
            return 0;
        }
        return 0;
    }
    case WM_COMMAND: {
        const int command = LOWORD(wParam);
        const int notification = HIWORD(wParam);
        HWND source = reinterpret_cast<HWND>(lParam);
        RecordMacroCommand(static_cast<UINT>(command));
        if (command == kIdTrayShow) {
            RestoreMainWindowFromTray();
            return 0;
        }
        if (command == kIdTrayExit) {
            g_app.windowCloseBehavior = WindowCloseBehavior::Exit;
            HandleMainWindowClose(true);
            return 0;
        }
        if (notification == EN_CHANGE && source != nullptr && !g_app.loading) {
            for (size_t index = 0; index < g_app.documents.size(); ++index) {
                if (g_app.documents[index].editor == source) {
                    std::wstring currentText = GetEditorRawText(source);
                    const uint64_t hash = HashText(currentText);
                    if (hash == g_app.documents[index].textHash) break;  // formatting only
                    g_app.documents[index].textHash = hash;
                    {
                        CHARRANGE caret{};
                        SendMessageW(source, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&caret));
                        const int caretLine = 1 + static_cast<int>(std::count(currentText.begin(),
                            currentText.begin() + std::min<size_t>(static_cast<size_t>(std::max<LONG>(0, caret.cpMin)), currentText.size()), L'\r'));
                        ShiftBookmarksForEdit(g_app.documents[index], CountRealLines(currentText), caretLine);
                    }
                    StoreEditorText(source, hash, std::move(currentText));
                    PreviewOnTextChanged(source);
                    const bool wasModified = g_app.documents[index].modified;
                    g_app.documents[index].modified = true;
                    MarkRecoveryDirty(static_cast<int>(index));
                    if (!wasModified) UpdateTabTitle(static_cast<int>(index));
                    UpdateStatusBar();
                    if (g_app.showLineNumbers && GutterWidthFor(source) != g_app.gutterWidth) LayoutChildren(window);
                    UpdateHorizontalScrollbar(source);
                    RefreshGutter();
                    TabDocument& changed = g_app.documents[index];
                    if (!IsEditorImeComposing(source)) {
                        ApplyBracketMatchHighlight(changed, true);  // re-applies the line highlight too
                    }
                    NormalizeBookmarks(changed);
                    changed.syntaxStale = true;  // tokens no longer match; the timer below re-tokenizes
                    if (EffectiveLanguage(changed) != kLanguagePlain || changed.syntaxHadColors ||
                        (changed.untitled && changed.language == kLanguageAuto)) {
                        SetTimer(window, kTimerHighlight, kHighlightDelayMs, nullptr);  // restarts on each keystroke
                    }
                    break;
                }
            }
            return 0;
        }
        if (notification == EN_VSCROLL) {
            for (TabDocument& document : g_app.documents) {
                if (document.editor == source) {
                    const int visibleLine = static_cast<int>(SendMessageW(
                        source, EM_GETFIRSTVISIBLELINE, 0, 0));
                    if (document.firstVisibleLine != visibleLine) {
                        document.firstVisibleLine = visibleLine;
                        g_app.stateDirty = true;
                    }
                    break;
                }
            }
            RefreshGutter();
            SyntaxOnViewChanged(source);
            UpdateStatusBar();
            return 0;
        }
        if (command == kIdLineSpacingSingle || command == kIdLineSpacing115 ||
            command == kIdLineSpacing150 || command == kIdLineSpacing200) {
            static constexpr int values[] = {100, 115, 150, 200};
            SetLineSpacingPercent(values[command - kIdLineSpacingFirst]);
            return 0;
        }
        if (command == kIdLineSpacingCustom) {
            OpenCustomLineSpacingDialog();
            return 0;
        }
        if (command == kIdTopSpacingCustom) {
            OpenCustomTopSpacingDialog();
            return 0;
        }
        if (command >= kIdEncodingFirst && command < kIdEncodingFirst + kEncodingCount) {
            SetActiveEncoding(command - kIdEncodingFirst);
            SetMenuState();
            return 0;
        }
        if (command >= kIdLineEndingFirst && command < kIdLineEndingFirst + kLineEndingCount) {
            SetActiveLineEnding(command - kIdLineEndingFirst);
            SetMenuState();
            return 0;
        }
        if (command >= kIdLanguageFirst && command < kIdLanguageFirst + kLanguageCount) {
            SetActiveLanguage(command - kIdLanguageFirst);
            SetMenuState();
            return 0;
        }

        if (command >= kIdSearchHistoryFirst && command < kIdSearchHistoryFirst + 20) {
            RunSearchFromHistory(command - kIdSearchHistoryFirst);
            return 0;
        }
        if (IsDataGeneratorCommand(command)) {
            OpenDataGeneratorDialog(command);
            return 0;
        }
        if (IsTextToolCommand(command)) {
            RunTextTool(command);
            SetMenuState();
            return 0;
        }
        switch (command) {
        case kIdNewTabButton: NewDocument(); break;
        case kIdToolbarNew: NewDocument(); break;
        case kIdToolbarOpen: OpenFileDialog(); break;
        case kIdToolbarSave: SaveDocument(g_app.activeIndex, false); break;
        case kIdToolbarSaveAs: SaveDocument(g_app.activeIndex, true); break;
        case kIdFileSaveAll: SaveAllDocuments(); break;
        case kIdToolbarClose: CloseDocument(g_app.activeIndex); break;
        case kIdTabNext: SelectAdjacentTab(1); break;
        case kIdTabPrevious: SelectAdjacentTab(-1); break;
        case kIdTabTogglePinned:
            if (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size())) {
                g_app.documents[static_cast<size_t>(g_app.activeIndex)].pinned =
                    !g_app.documents[static_cast<size_t>(g_app.activeIndex)].pinned;
                if (g_app.tabs) InvalidateRect(g_app.tabs, nullptr, FALSE);
                g_app.stateDirty = true;
            }
            break;
        case kIdToolbarUndo: HandleEditCommand(kIdEditUndo); break;
        case kIdToolbarRedo: HandleEditCommand(kIdEditRedo); break;
        case kIdToolbarFind: OpenFindDialog(false); break;
        case kIdFindNext: FindAgain(true); break;
        case kIdFindPrevious: FindAgain(false); break;
        case kIdToolbarReplace: OpenFindDialog(true); break;
        case kIdToolbarGoto: OpenGotoDialog(); break;
        case kIdToolbarFormat: FormatActiveDocument(); break;
        case kIdFileRename: RenameDocument(g_app.activeIndex); break;
        case kIdSearchInFolder: ShowSearchPanel(0, 0, -1, false); break;
        case kIdReplaceInFolder: ShowSearchPanel(0, 0, -1, true); break;
        case kIdSearchCurrentFile: ShowSearchPanel(2, 0, -1, false); break;
        case kIdSearchOpenFiles: ShowSearchPanel(1, 0, -1, false); break;
        case kIdSearchFileNames: ShowSearchPanel(0, 1, -1, false); break;
        case kIdSearchRegex: ShowSearchPanel(-1, 0, 1, false); break;
        case kIdToggleSearchPanel: ToggleSearchPanel(); break;
        case kIdExportSearchResults: ExportSearchResults(); break;
        case kIdCopySearchResults: CopyTextToClipboard(SearchResultsAsText()); ShowNotice(L"已复制全部搜索结果"); break;
        case kIdBookmarkSearchResult: BookmarkSelectedSearchResult(); break;
        case kIdClearSearchResults: ClearSearchResults(); break;
        case kIdSearchErrorLog: ShowSearchErrorLog(); break;
        case kIdUndoReplaceInFiles: UndoLastReplaceInFiles(); break;
        case kIdCompareFiles: CompareTwoFilesCommand(false); break;
        case kIdCompareStructure: CompareTwoFilesCommand(true); break;
        case kIdCompareWithFile: CompareActiveWithFileCommand(); break;
        case kIdCompareFolders: CompareFoldersCommand(); break;
        case kIdOpenLogViewer: OpenLogViewerCommand(); break;
        case kIdToggleToolbox: ToggleToolbox(!g_app.toolboxVisible); break;
        case kIdToolbarRun: RunActiveDocument(); break;
        case kIdToolbarEncoding:
            ShowEncodingMenu(ToolbarItemScreenRect(kIdToolbarEncoding));
            break;
        case kIdToolbarLineNumbers:
            g_app.showLineNumbers = !g_app.showLineNumbers;
            g_app.stateDirty = true;
            LayoutChildren(window);
            break;
        case kIdToolbarWrap:
            g_app.wordWrap = !g_app.wordWrap;
            g_app.stateDirty = true;
            ApplyWordWrapAll();
            LayoutChildren(window);
            break;
        case kIdViewLivePreview: ToggleLivePreview(); break;
        case kIdViewZoomIn: ChangeEditorFontSize(1); break;
        case kIdViewZoomOut: ChangeEditorFontSize(-1); break;
        case kIdViewZoomReset: ChangeEditorFontSize(0); break;
        case kIdViewToggleBookmark:
            ToggleBookmarkAtCaret();
            break;
        case kIdViewNextBookmark:
            GotoBookmark(1);
            break;
        case kIdViewPreviousBookmark:
            GotoBookmark(-1);
            break;
        case kIdViewClearBookmarks:
            ClearBookmarks();
            break;
        case kIdViewWhitespace:
            g_app.showWhitespace = !g_app.showWhitespace;
            g_app.stateDirty = true;
            for (const TabDocument& document : g_app.documents) InvalidateRect(document.editor, nullptr, FALSE);
            break;
        case kIdViewIndentGuides:
            g_app.showIndentGuides = !g_app.showIndentGuides;
            g_app.stateDirty = true;
            for (const TabDocument& document : g_app.documents) InvalidateRect(document.editor, nullptr, FALSE);
            break;
        case kIdViewFoldCurrent:
            FoldCurrentBlock();
            break;
        case kIdViewUnfoldAll:
            UnfoldAllBlocks();
            break;
        case kIdToolbarTheme:
            ShowThemeMenu(ToolbarItemScreenRect(kIdToolbarTheme));
            break;
        case kIdToolbarSettings:
            OpenSettingsDialog();
            break;
        case kIdSettingsShortcuts:
            OpenShortcutDialog();
            break;
        case kIdPrefixSuffix:
            OpenPrefixSuffixDialog();
            break;
        case kIdMultiCaretNext:
            AddNextLineCaret();
            break;
        case kIdRectangleSelection:
            CreateRectangleCarets();
            break;
        case kIdClearMultiCaret:
            ClearMultiCarets();
            break;
        case kIdMacroToggle:
            ToggleMacroRecording();
            break;
        case kIdMacroPlay:
            PlayMacro();
            break;
        case kIdMacroClear:
            g_app.macroActions.clear();
            ShowNotice(L"最近宏已清除");
            break;
        case kIdThemeDark:
            SetThemeKind(ThemeKind::Dark);
            break;
        case kIdThemeLight:
            SetThemeKind(ThemeKind::Light);
            break;
        case kIdThemeGreen:
            SetThemeKind(ThemeKind::Green);
            break;
        case kIdThemeBlue:
            SetThemeKind(ThemeKind::Blue);
            break;
        case kIdFileNew: NewDocument(); break;
        case kIdFileOpen: OpenFileDialog(); break;
        case kIdFileSave: SaveDocument(g_app.activeIndex, false); break;
        case kIdFileSaveAs: SaveDocument(g_app.activeIndex, true); break;
        case kIdFileClose: CloseDocument(g_app.activeIndex); break;
        case kIdFileCloseAll: CloseAllDocuments(); break;
        case kIdFileReopenClosed: ReopenClosedDocument(); break;
        case kIdFileExit: HandleMainWindowClose(true); break;
        case kIdHelpAbout: {
            const std::wstring about = L"MCnotepad " + VersionDescription() +
                L"\n轻量、快速、可恢复的 Windows 文本编辑器。\n\n版本类型：" + EditionDescription() +
                L"\n程序位置：" + CurrentExecutablePath() +
                L"\n数据位置：" + g_app.dataDirectory.wstring();
            MessageBoxW(window, about.c_str(), kWindowTitle, MB_ICONINFORMATION);
            break;
        }
        case kIdEditUndo:
        case kIdEditRedo:
        case kIdEditCut:
        case kIdEditCopy:
        case kIdEditPaste:
        case kIdEditSelectAll:
            HandleEditCommand(command);
            break;
        default:
            break;
        }
        SetMenuState();
        return 0;
    }
    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (item != nullptr) {
            if (item->CtlType == ODT_COMBOBOX) DrawComboItem(*item);
        }
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
    case WM_ERASEBKGND: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        RECT client{};
        GetClientRect(window, &client);
        FillRect(dc, &client, g_app.windowBrush);
        return 1;
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, EditorForeground());
        SetBkColor(dc, EditorBackground());
        return reinterpret_cast<LRESULT>(g_app.editorBrush);
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSCROLLBAR: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        const ThemePalette& palette = CurrentPalette();
        if (control == g_app.toolbar) {
            SetTextColor(dc, palette.toolbarText);
            SetBkColor(dc, palette.toolbarBackground);
            return reinterpret_cast<LRESULT>(g_app.toolbarBrush);
        }
        if (control == g_app.statusBar) {
            SetTextColor(dc, palette.statusText);
            SetBkColor(dc, palette.statusBackground);
            return reinterpret_cast<LRESULT>(g_app.statusBrush);
        }
        SetTextColor(dc, palette.controlText);
        SetBkColor(dc, palette.controlBackground);
        return reinterpret_cast<LRESULT>(g_app.controlBrush);
    }
    case WM_NOTIFY: {
        const NMHDR* header = reinterpret_cast<const NMHDR*>(lParam);
        if (header != nullptr && header->code == EN_SELCHANGE) {
            UpdateStatusBar();
            ToolboxOnSelectionChanged();
            RefreshGutter();  // current-line number follows the caret
            if (TabDocument* document = ActiveDocument()) {
                // Formatting the document while the mouse is dragging a
                // selection resets RichEdit's active end and makes a
                // bottom-to-top selection jump.  Defer these visual updates
                // until WM_LBUTTONUP, after the selection is complete.
                if (!IsEditorSelectionDragActive(header->hwndFrom) &&
                    !IsEditorImeComposing(header->hwndFrom)) {
                    ApplyCurrentLineHighlight(*document);
                    ApplyBracketMatchHighlight(*document);
                }
            }
            return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        HandleMainWindowClose();
        return 0;
    case WM_COPYDATA: {
        const auto* data = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        if (data == nullptr || data->dwData != kCopyDataOpenFiles) return FALSE;
        if (data->lpData != nullptr && data->cbData >= sizeof(wchar_t)) {
            const auto* chars = static_cast<const wchar_t*>(data->lpData);
            const size_t count = data->cbData / sizeof(wchar_t);
            size_t start = 0;
            for (size_t index = 0; index < count; ++index) {
                if (chars[index] != L'\0') continue;
                if (index > start) g_app.pendingOpenFiles.emplace_back(chars + start, index - start);
                start = index + 1;
            }
        }
        // Launching MCnotepad again (taskbar, Start menu, a double-clicked
        // file) must bring the window back, also when the X button only hid
        // it in the tray.
        if (g_app.hiddenToTray || !IsWindowVisible(window)) RestoreMainWindowFromTray();
        else if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
        SetForegroundWindow(window);
        // Open asynchronously: the sender is blocked until we return, and opening
        // can show message boxes.
        PostMessageW(window, kMessageOpenPendingFiles, 0, 0);
        return TRUE;
    }
    case WM_ACTIVATEAPP:
        if (!wParam) {
            // Save the real viewport before taskbar/tray deactivation.  The
            // editor can reflow while hidden, so relying only on tab switches
            // leaves the next restore parked on an old line (often line 11).
            for (TabDocument& document : g_app.documents) CaptureEditorState(document);
            g_app.stateDirty = true;
        } else {
            LayoutChildren(window);
            if (TabDocument* document = ActiveDocument()) {
                const LONG current = static_cast<LONG>(SendMessageW(
                    document->editor, EM_GETFIRSTVISIBLELINE, 0, 0));
                if (current != document->firstVisibleLine) {
                    SendMessageW(document->editor, EM_LINESCROLL, 0,
                        document->firstVisibleLine - current);
                }
            }
            RefreshGutter();
            PostMessageW(window, kMessageCheckExternalChanges, 0, 0);
        }
        return 0;
    case kMessageCheckExternalChanges:
        if (AppIsInModalState()) { g_deferredExternalCheck = true; return 0; }
        CheckExternalChanges();
        return 0;
    case WM_ENABLE:
        if (wParam) ResumeDeferredWork();
        return DefWindowProcW(window, message, wParam, lParam);
    case kMessageOpenPendingFiles: {
        if (AppIsInModalState()) { g_deferredOpenFiles = true; return 0; }
        std::vector<std::wstring> files;
        files.swap(g_app.pendingOpenFiles);
        for (size_t f = 0; f < files.size(); ++f) {
            if (files[f] == L"--log" && f + 1 < files.size()) {
                OpenLogViewer(files[++f]);
                continue;
            }
            if ((files[f] == L"--diff" || files[f] == L"--diff-struct") && f + 2 < files.size()) {
                std::error_code error;
                if (fs::is_directory(files[f + 1], error) && fs::is_directory(files[f + 2], error)) CompareFolderPaths(files[f + 1], files[f + 2]);
                else CompareFiles(files[f + 1], files[f + 2], files[f] == L"--diff-struct");
                f += 2;
                continue;
            }
            OpenFileInNewTab(fs::path(files[f]));
        }
        SetMenuState();
        return 0;
    }
    case WM_DPICHANGED: {
        // Moved to a monitor with a different scale: rebuild fonts and sizes,
        // then take the size Windows suggests for the new DPI.
        g_app.dpi = HIWORD(wParam);
        CreateUiFonts();
        ApplyEditorFont();
        RefreshChrome();
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(window, nullptr, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        LayoutChildren(window);
        InvalidateRect(window, nullptr, TRUE);
        return 0;
    }
    case WM_SYSCOMMAND:
        // No native menu bar: route Alt / Alt+letter / F10 to the custom one.
        if ((wParam & 0xFFF0) == SC_KEYMENU && HandleMenuKey(lParam)) return 0;
        return DefWindowProcW(window, message, wParam, lParam);
    case kMessageOpenMenuBar:
        OpenMenuBarItem(static_cast<int>(wParam));
        return 0;
    case WM_MENUSELECT:
        // Lets Right-arrow open a submenu instead of jumping to the next menu.
        g_menuSelectionHasPopup = (HIWORD(wParam) & MF_POPUP) != 0 && HIWORD(wParam) != 0xFFFF;
        return 0;
    case WM_QUERYENDSESSION:
        // Windows is shutting down / restarting / logging off. WM_CLOSE will not
        // arrive, so flush every snapshot now without blocking on prompts.
        PersistAll(true);
        return TRUE;
    case WM_ENDSESSION:
        if (wParam) {
            g_app.closing = true;
            KillTimer(window, kTimerAutosave);
            PersistAll(true);
            RemoveRunningMarker();
            // An installer or uninstaller (Restart Manager) is replacing the
            // program files: really exit, even when "close" means "hide to
            // tray". Unsaved tabs come back from their snapshots.
            if (lParam & ENDSESSION_CLOSEAPP) DestroyWindow(window);
        }
        return 0;
    case WM_DESTROY:
        RemovePropW(window, kDataKeyProperty);
        RemoveTrayIcon();
        DeleteObject(g_app.editorBrush);
        g_app.toolbarIconImages.clear();
        DeleteObject(g_app.toolbarBrush);
        DeleteObject(g_app.sidebarBrush);
        DeleteObject(g_app.windowBrush);
        DeleteObject(g_app.controlBrush);
        DeleteObject(g_app.statusBrush);
        if (g_app.gdiplusToken != 0) {
            GdiplusShutdown(g_app.gdiplusToken);
            g_app.gdiplusToken = 0;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

// Called when another MCnotepad already owns the session. Hands our command-line
// files (as absolute paths) to it and brings it to the front. This process must
// not touch session.dat / running.lock, otherwise the two instances would
// overwrite each other's session and report bogus crashes.
void ForwardToRunningInstance() {
    std::wstring payload;
    int argumentCount = 0;
    if (LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount)) {
        for (int index = 1; index < argumentCount; ++index) {
            const std::wstring argument = arguments[index];
            if (argument == L"--diff" || argument == L"--diff-struct" || argument == L"--log") {
                payload += argument;
                payload.push_back(L'\0');
                continue;
            }
            if (argument.empty() || argument[0] == L'-') continue;
            std::error_code error;
            const fs::path absolute = fs::absolute(fs::path(argument), error);
            payload += error ? argument : absolute.wstring();
            payload.push_back(L'\0');
        }
        LocalFree(arguments);
    }
    payload.push_back(L'\0');

    // The first instance may still be starting up; give it a few seconds.
    HWND target = nullptr;
    for (int attempt = 0; attempt < 50 && target == nullptr; ++attempt) {
        target = FindInstanceWindow();
        if (target == nullptr) Sleep(100);
    }
    if (target == nullptr) return;

    DWORD processId = 0;
    GetWindowThreadProcessId(target, &processId);
    AllowSetForegroundWindow(processId);
    COPYDATASTRUCT data{};
    data.dwData = kCopyDataOpenFiles;
    data.cbData = static_cast<DWORD>(payload.size() * sizeof(wchar_t));
    data.lpData = payload.data();
    DWORD_PTR result = 0;
    SendMessageTimeoutW(target, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data),
        SMTO_ABORTIFHUNG, 5000, &result);
}

int RunSelfTests() {
    const auto expect = [](bool condition, std::source_location where = std::source_location::current()) {
        if (!condition) printf("self-test failed at line %u\n", static_cast<unsigned>(where.line()));
        return condition ? 0 : 1;
    };
    int failures = 0;
    failures += expect(FindMatchingBracket(L"{alpha [beta]}", 0) == 13);
    failures += expect(FindMatchingBracket(L"{alpha [beta]}", 12) == 7);
    failures += expect(HashText(L"abc") == HashText(L"abc"));
    failures += expect(HashText(L"abc") != HashText(L"abd"));
    failures += expect(DataUnescape(L"a\\nb\\t") == L"a\r\nb\t");
    BYTE modifiers = 0;
    WORD key = 0;
    failures += expect(ParseShortcutText(L"Ctrl+Shift+S", modifiers, key) &&
        (modifiers & FCONTROL) && (modifiers & FSHIFT) && key == 'S');
    failures += expect(ParseShortcutText(L"F5", modifiers, key) && modifiers == FVIRTKEY && key == VK_F5);
    failures += expect(ParseShortcutText(L"Ctrl+Down", modifiers, key) &&
        (modifiers & FCONTROL) && key == VK_DOWN);
    failures += expect(ParseShortcutText(L"", modifiers, key) && modifiers == 0 && key == 0);
    failures += expect(!ParseShortcutText(L"Ctrl+Unknown", modifiers, key));
    const auto lines = SplitLines(L"a\r\nb\nc");
    failures += expect(lines.size() == 3 && JoinLines(lines, L"\r") == L"a\rb\rc");
    const auto utf8Safe = [](const std::wstring& value) {
        return value.find(L'\uFFFD') == std::wstring::npos &&
            Utf8ToWide(WideToUtf8(value)) == value;
    };
    const auto containsCjk = [](const std::wstring& value) {
        return std::any_of(value.begin(), value.end(), [](wchar_t character) {
            return (character >= 0x3400 && character <= 0x4DBF) ||
                (character >= 0x4E00 && character <= 0x9FFF);
        });
    };
    // Exercise Chinese, punctuation, supplementary-plane characters and line breaks.
    const std::wstring unicodeSample = L"中文转换：你好，世界！\r\n第二行 \xD83D\xDE00";
    failures += expect(utf8Safe(unicodeSample));
    failures += expect(Utf8ToWide("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95") == L"中文测试");
    for (int encoding = 0; encoding <= 3; ++encoding) {
        std::string bytes;
        std::wstring decoded;
        int detected = -1;
        failures += expect(EncodeTextBytes(unicodeSample, encoding, bytes));
        failures += expect(DecodeTextBytes(std::vector<char>(bytes.begin(), bytes.end()), &detected) == unicodeSample);
        failures += expect(detected == encoding);
    }
    const std::wstring encodedBase64 = Base64Encode(unicodeSample);
    std::wstring decodedBase64;
    failures += expect(Base64Decode(encodedBase64, decodedBase64) && decodedBase64 == unicodeSample);
    int decodedUrlParts = 0;
    failures += expect(UrlDecode(UrlEncode(unicodeSample), decodedUrlParts) == unicodeSample && decodedUrlParts > 0);

    std::wstring formattedJson;
    std::wstring formatError;
    failures += expect(FormatJson(L"{\"名称\":\"中文\",\"内容\":\"你好\"}", formattedJson, formatError) && utf8Safe(formattedJson));
    failures += expect(utf8Safe(FormatCss(L".标题 { content: \"中文\"; }")));
    failures += expect(utf8Safe(FormatJavaScript(L"const 文本 = \"中文\";")));
    failures += expect(utf8Safe(MinifyCss(L".标题 { content: \"中文\"; }")));
    failures += expect(utf8Safe(MinifyJavaScript(L"const 文本 = \"中文\";")));

    std::wstring converted;
    failures += expect(ConvertChineseScript(L"汉字转换：干燥", true, converted) &&
        converted.find(L"漢字") != std::wstring::npos && utf8Safe(converted));
    int pinyinCount = 0;
    std::wstring pinyinError;
    std::wstring pinyin;
    failures += expect(ConvertToPinyin(L"中文测试", PinyinMode::Plain, pinyin, pinyinCount, pinyinError) &&
        pinyinCount > 0 && utf8Safe(pinyin));
    std::wstring markdownResult;
    std::wstring markdownError;
    failures += expect(ShowdownConvert(L"# 中文标题\r\n\r\n你好，世界！", true, markdownResult, markdownError) &&
        utf8Safe(markdownResult));
    std::wstring htmlResult;
    failures += expect(ShowdownConvert(L"<h1>中文标题</h1><p>你好，世界！</p>", false, htmlResult, markdownError) &&
        utf8Safe(htmlResult));
    // Keep the developer conversion commands reachable from WM_COMMAND.
    // A regression here makes the menu appear clickable while doing nothing.
    failures += expect(IsTextToolCommand(kIdMarkdownToHtml) &&
        IsTextToolCommand(kIdHtmlToMarkdown));

    for (int locale = 0; locale < 5; ++locale) {
        DataGeneratorOptions profile;
        profile.count = 2;
        profile.seed = 0xC0DE2026u + static_cast<uint64_t>(locale);
        profile.locale = locale;
        const std::wstring generated = GenerateDataText(kDataFullProfile, profile);
        failures += expect(!generated.empty() &&
            generated.find(L"姓名：") != std::wstring::npos &&
            generated.find(L"用户名：") != std::wstring::npos &&
            generated.find(L"电子邮箱：") != std::wstring::npos &&
            generated.find(L"个人简介：") != std::wstring::npos &&
            (locale != 0 || containsCjk(generated)) &&
            generated.find(L'\uFFFD') == std::wstring::npos && utf8Safe(generated));
        for (int command = kIdDataFirst; command <= kIdDataLast; ++command) {
            const std::wstring value = GenerateDataText(command, profile);
            failures += expect(value.find(L'\uFFFD') == std::wstring::npos &&
                value.find(L"[数据生成失败]") == std::wstring::npos && utf8Safe(value));
        }
    }
    // UTF-16 without a BOM is recognised instead of being cut at the first NUL.
    {
        const std::string le("H\0i\0 \0\x2D\x4E", 8);  // "Hi " + U+4E2D, UTF-16 LE
        std::vector<char> bytes(le.begin(), le.end());
        int encoding = -1;
        const std::wstring text = DecodeTextBytes(bytes, &encoding);
        failures += expect(encoding == 2 && text == L"Hi \u4E2D");
        std::vector<char> big{'\0', 'O', '\0', 'K'};
        failures += expect(DecodeTextBytes(big, &encoding) == L"OK" && encoding == 3);
        std::vector<char> ascii{'a', 'b', 'c', 'd'};
        failures += expect(GuessBomlessUtf16(ascii) == 0);
        failures += expect(HasNulCharacter(std::wstring(L"a\0b", 3)) && !HasNulCharacter(L"ab"));
    }
    // Replace in files only rewrites files whose bytes survive a round trip.
    {
        const std::string clean = "\xE4\xB8\xAD\xE6\x96\x87 ok";  // UTF-8 "中文 ok"
        const std::string damaged = clean + "\xFF";
        for (const std::string& sample : {clean, damaged}) {
            std::vector<char> bytes(sample.begin(), sample.end());
            int encoding = 0;
            std::string again;
            EncodeTextBytes(DecodeTextBytes(bytes, &encoding), encoding, again);
            failures += expect((again == sample) == (sample == clean));
        }
    }
    // Bookmarks move with inserted/removed lines.
    {
        TabDocument document;
        document.logicalLineCount = 10;
        document.bookmarks = {2, 5, 9};
        ShiftBookmarksForEdit(document, 12, 4);   // 2 lines inserted ending at line 4 (edit at line 2)
        failures += expect((document.bookmarks == std::vector<int>{2, 7, 11}));
        ShiftBookmarksForEdit(document, 9, 3);    // lines 4..6 removed, caret on line 3
        failures += expect((document.bookmarks == std::vector<int>{2, 4, 8}));
    }
    // Regex suffix: the trailing match, not the first one.
    {
        PrefixSuffixMatch match;
        std::wstring error;
        failures += expect(PrefixSuffixRegexSearch(L"a1b2", LR"(\d+)", true, false, match, error) && match.found && match.start == 3 && match.length == 1);
        PrefixSuffixMatch none;
        failures += expect(PrefixSuffixRegexSearch(L"a1b", LR"(\d+)", true, false, none, error) && !none.found);
    }
    // Wrap by width: breaks at spaces, keeps surrogate pairs whole.
    {
        std::vector<std::wstring> out;
        int breaks = 0;
        WrapLineByWidth(L"aaa bbb ccc", 5, out, breaks);
        failures += expect((out == std::vector<std::wstring>{L"aaa", L"bbb", L"ccc"}) && breaks == 2);
        out.clear();
        WrapLineByWidth(L"ab\U0001F600cd", 3, out, breaks);  // emoji = 2 UTF-16 units at positions 2-3
        failures += expect(out.size() >= 2 && out[0] == L"ab" && out[1].substr(0, 2) == L"\U0001F600");
    }
    // Hit line ends come from the line table.
    {
        std::vector<SearchHit> hits;
        DescribeHits(L"one\ntwo x\nthree", {{8, 1}}, hits);
        failures += expect(hits.size() == 1 && hits[0].line == 2 && hits[0].column == 5 && hits[0].lineText == L"two x");
    }
    // YAML block text is one value: a change inside it is reported.
    {
        YamlDoc left, right;
        std::wstring error;
        left.Parse({L"job:", L"  script: |", L"    echo a: 1", L"    done", L"  name: x"}, error);
        right.Parse({L"job:", L"  script: |", L"    echo b: 1", L"    done", L"  name: x"}, error);
        const auto rows = CompareStructItems(left.Items(), right.Items());
        failures += expect(rows.size() == 1 && rows[0].path == L"job.script" && rows[0].kind == RowKind::Changed);
        failures += expect(left.Take(L"job.script", right, error));
        failures += expect(CompareStructItems(left.Items(), right.Items()).empty());
    }
    // Folder compare matches names case-insensitively.
    {
        wchar_t tempRoot[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tempRoot);
        const fs::path folder = fs::path(tempRoot) / (L"mcnotepad-folders-" + std::to_wstring(GetCurrentProcessId()));
        std::error_code error;
        fs::create_directories(folder, error);
        WriteBinaryFile(folder / L"README.md", "x");
        std::unordered_map<std::wstring, FolderFileInfo> files;
        ListFolderFiles(folder.wstring(), files);
        failures += expect(files.count(L"readme.md") == 1 && files[L"readme.md"].relative == L"README.md");
        fs::remove_all(folder, error);
    }
    // Selection direction survives capture/restore.
    {
        LoadLibraryW(L"Msftedit.dll");
        HWND edit = CreateWindowExW(0, L"RICHEDIT50W", L"line one\rline two\rline three", WS_POPUP | ES_MULTILINE, 0, 0, 200, 100, nullptr, nullptr, nullptr, nullptr);
        if (edit) {
            SendMessageW(edit, EM_SETSEL, 15, 3);  // made upwards: caret at 3
            const SelectionState saved = CaptureSelection(edit);
            failures += expect(saved.caretAtStart && saved.range.cpMin == 3 && saved.range.cpMax == 15);
            CHARRANGE flat{3, 15};
            SendMessageW(edit, EM_SETSEL, 0, 0);
            RestoreSelection(edit, saved);
            const SelectionState after = CaptureSelection(edit);
            failures += expect(after.caretAtStart && after.range.cpMin == 3 && after.range.cpMax == 15);
            (void)flat;
            DestroyWindow(edit);
        }
    }
    // The bundled programming fonts load (they are copied next to the exe).
    {
        LoadBundledFonts();
        failures += expect(g_bundledFonts.size() >= 10 && IsBundledFont(L"JetBrains Mono") && IsBundledFont(L"Fira Code"));
        HDC dc = GetDC(nullptr);
        HFONT font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Maple Mono");
        HGDIOBJ previous = SelectObject(dc, font);
        wchar_t face[LF_FACESIZE]{};
        GetTextFaceW(dc, LF_FACESIZE, face);
        failures += expect(std::wstring(face) == L"Maple Mono");  // really used, not a substitute
        SelectObject(dc, previous);
        DeleteObject(font);
        ReleaseDC(nullptr, dc);
    }
    // An exception escaping a window procedure is caught, not fatal.
    {
        struct Thrower {
            static LRESULT CALLBACK Proc(HWND, UINT, WPARAM, LPARAM) { throw std::invalid_argument("test"); }
        };
        const int before = g_unexpectedExceptions;
        const LRESULT result = Guarded<Thrower::Proc>(nullptr, WM_COMMAND, 0, 0);
        failures += expect(result == 0 && g_unexpectedExceptions == before + 1);
    }
    // Atomic save keeps attributes and hard links and leaves no temp files.
    {
        wchar_t tempRoot[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tempRoot);
        const fs::path folder = fs::path(tempRoot) / (L"mcnotepad-selftest-" + std::to_wstring(GetCurrentProcessId()));
        std::error_code error;
        fs::create_directories(folder, error);
        const fs::path hidden = folder / L"hidden.txt";
        failures += expect(WriteAtomicFile(hidden, "one"));
        SetFileAttributesW(hidden.c_str(), FILE_ATTRIBUTE_HIDDEN);
        failures += expect(WriteAtomicFile(hidden, "two"));
        failures += expect((GetFileAttributesW(hidden.c_str()) & FILE_ATTRIBUTE_HIDDEN) != 0);
        std::vector<char> read;
        failures += expect(ReadBinaryFile(hidden, read) && std::string(read.begin(), read.end()) == "two");
        const fs::path linked = folder / L"linked.txt";
        if (CreateHardLinkW(linked.c_str(), hidden.c_str(), nullptr)) {
            failures += expect(WriteAtomicFile(hidden, "three"));
            failures += expect(ReadBinaryFile(linked, read) && std::string(read.begin(), read.end()) == "three");
        }
        size_t leftovers = 0;
        for (const auto& entry : fs::directory_iterator(folder, error)) leftovers += entry.path().extension() == L".tmp";
        failures += expect(leftovers == 0);
        SetFileAttributesW(hidden.c_str(), FILE_ATTRIBUTE_NORMAL);
        fs::remove_all(folder, error);
    }
    return failures == 0 ? 0 : 1;
}

} // namespace


int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand) {
    (void)commandLine;
    g_app.instance = instance;
    InitializeShortcutBindings();

    if (commandLine != nullptr && wcsstr(commandLine, L"--self-test") != nullptr) {
        return RunSelfTests();
    }

    // Kept open for the lifetime of the process; the OS releases it on exit.
    HANDLE singleInstance = CreateMutexW(nullptr, FALSE, SingleInstanceMutexName().c_str());
    if (singleInstance != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        ForwardToRunningInstance();
        CloseHandle(singleInstance);
        return 0;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g_app.findReplaceMessage = RegisterWindowMessageW(FINDMSGSTRING);
    InitializeStorage();
    LoadBundledFonts();  // before any editor font is created

    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_TAB_CLASSES | ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);  // folder picker
    InitCommonControlsEx(&commonControls);

    WNDCLASSW gutterClass{};
    gutterClass.hInstance = instance;
    gutterClass.lpfnWndProc = Guarded<GutterProc>;
    gutterClass.lpszClassName = L"MCGutter";
    gutterClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    gutterClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&gutterClass);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = Guarded<WindowProc>;
    windowClass.lpszClassName = kWindowClass;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_MCNOTEPAD));
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassExW(&windowClass)) {
        MessageBoxW(nullptr, L"窗口注册失败。", kWindowTitle, MB_ICONERROR);
        RemoveRunningMarker();
        return 1;
    }

    HWND window = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        MulDiv(1100, static_cast<int>(GetDpiForSystem()), 96),
        MulDiv(760, static_cast<int>(GetDpiForSystem()), 96),
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!window) {
        MessageBoxW(nullptr, L"主窗口创建失败。", kWindowTitle, MB_ICONERROR);
        RemoveRunningMarker();
        return 1;
    }

    // Restore the previous size/position, but only if it is still on a monitor.
    if (g_app.hasSavedWindow && MonitorFromRect(&g_app.savedWindowRect, MONITOR_DEFAULTTONULL) != nullptr) {
        WINDOWPLACEMENT placement{};
        placement.length = sizeof(placement);
        GetWindowPlacement(window, &placement);
        placement.rcNormalPosition = g_app.savedWindowRect;
        placement.showCmd = g_app.savedWindowMaximized ? SW_SHOWMAXIMIZED
            : (showCommand == SW_SHOWMINIMIZED || showCommand == SW_MINIMIZE ? showCommand : SW_SHOWNORMAL);
        SetWindowPlacement(window, &placement);
    } else {
        ShowWindow(window, showCommand);
    }
    UpdateWindow(window);
    UpdateTrayIcon();
    if (g_app.toolboxVisible) ToggleToolbox(true);
    OpenCommandLineFiles();
    PostMessageW(window, kMessageCheckExternalChanges, 0, 0);

    RebuildAccelerators();
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        // Modeless dialogs first: gives them Tab/Enter/Esc handling and keeps the
        // main window's Ctrl+C/V/Z accelerators from hijacking their text boxes.
        // Search panel: app shortcuts first (edit commands are routed to its
        // text boxes), then Tab/Enter/Esc dialog navigation.
        if ((g_app.searchPanel && IsChild(g_app.searchPanel, message.hwnd)) || (g_app.toolboxPanel && IsChild(g_app.toolboxPanel, message.hwnd))) {
            if (TranslateAcceleratorW(window, g_accelerators, &message)) continue;
            HWND panel = g_app.toolboxPanel && IsChild(g_app.toolboxPanel, message.hwnd) ? g_app.toolboxPanel : g_app.searchPanel;
            // Enter belongs to the multi-line boxes.
            wchar_t klass[16]{};
            GetClassNameW(message.hwnd, klass, 16);
            const bool multilineEnter = message.message == WM_KEYDOWN && message.wParam == VK_RETURN && _wcsicmp(klass, L"Edit") == 0 &&
                (GetWindowLongW(message.hwnd, GWL_STYLE) & ES_MULTILINE);
            if (!multilineEnter && IsDialogMessageW(panel, &message)) continue;
            TranslateMessage(&message);
            DispatchMessageW(&message);
            continue;
        }
        // Compare / log windows: their own keys, dialog navigation, no main accelerators.
        if (HWND root = GetAncestor(message.hwnd, GA_ROOT);
            root && std::find(g_toolWindows.begin(), g_toolWindows.end(), root) != g_toolWindows.end()) {
            if (!IsWindow(root)) continue;
            if (message.message == WM_KEYDOWN && ToolWindowKey(root, message.wParam)) continue;
            if (IsDialogMessageW(root, &message)) continue;
            TranslateMessage(&message);
            DispatchMessageW(&message);
            continue;
        }
        g_toolWindows.erase(std::remove_if(g_toolWindows.begin(), g_toolWindows.end(), [](HWND w) { return !IsWindow(w); }), g_toolWindows.end());
        bool handled = false;
        for (HWND dialog : {g_app.findDialog, g_app.gotoWindow, g_app.settingsWindow}) {
            if (dialog && IsWindow(dialog) && IsDialogMessageW(dialog, &message)) {
                handled = true;
                break;
            }
        }
        if (handled) continue;
        if (!TranslateAcceleratorW(window, g_accelerators, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (g_accelerators) {
        DestroyAcceleratorTable(g_accelerators);
        g_accelerators = nullptr;
    }

    return static_cast<int>(message.wParam);
}

