// MCnotepad - a native Win32 text editor.
//
// The program is one translation unit split into ordered parts under
// src/parts/. The parts are included here in order and are not compiled
// on their own: later parts use declarations from earlier ones, and the
// anonymous namespace opened in 00_app_state.cpp is closed in
// 21_main_window.cpp. Keep new code in the part it belongs to, and keep
// the order below.

#include "parts/00_app_state.cpp"  // Includes, command IDs, constants, document and application state
#include "parts/01_theme_editor_basics.cpp"  // Theme palettes, DPI helpers, TOM access, folding, multi-caret
#include "parts/02_syntax_lexer.cpp"  // Syntax highlighting: language specs and lexer
#include "parts/03_syntax_paint_gutter.cpp"  // Applying colours, bracket match, bookmarks, line-number gutter, string helpers
#include "parts/04_files_encoding.cpp"  // Data folder, file I/O, atomic save, encoding and language detection
#include "parts/05_documents_session.cpp"  // Layout, session, editor window procedure, tabs and documents
#include "parts/06_find_settings.cpp"  // Find/replace in a document, shortcuts, fonts, tray, preferences, external changes
#include "parts/07_goto_format_run.cpp"  // Go to line, JSON/CSS/JS formatting, Markdown conversion, run current file
#include "parts/08_window_chrome.cpp"  // Menu bar, toolbar, tab strip, status bar
#include "parts/09_dialogs_rename_prefix.cpp"  // Input dialog, notices, tab rename, prefix/suffix tool
#include "parts/10_text_tools.cpp"  // Text tools and encoders
#include "parts/11_preview_extract.cpp"  // Change preview, extraction, applying tool results
#include "parts/12_search_engine.cpp"  // Find/replace in files: engine
#include "parts/13_search_panel.cpp"  // Find/replace in files: panel UI
#include "parts/14_search_replace_apply.cpp"  // Find/replace in files: preview, apply, undo
#include "parts/15_developer_toolbox.cpp"  // Developer toolbox sidebar
#include "parts/16_compare_models.cpp"  // Compare: line alignment, structured JSON/CSV/YAML models
#include "parts/17_compare_window.cpp"  // Compare window and structured view
#include "parts/18_folder_compare.cpp"  // Folder compare
#include "parts/19_log_viewer.cpp"  // Large log viewer
#include "parts/20_preview.cpp"  // Markdown / HTML live preview (WebView2)
#include "parts/21_main_window.cpp"  // Tool-window keys, main window procedure, self-test, entry point
