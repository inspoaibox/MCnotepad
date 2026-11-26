// ===========================================================================
// Syntax highlighting
// Each language has a small tokenizer that runs over the whole text (fast,
// pure string scanning). Colours are applied lazily: only the visible part of
// the editor plus a margin, and more as the user scrolls. That keeps large
// files instant to open and typing responsive.
// ===========================================================================

constexpr size_t kSyntaxSizeLimit = 2u << 20;  // characters; bigger files stay plain

struct SyntaxStyle { COLORREF color; bool bold; };

SyntaxStyle StyleFor(Tok kind) {
    const bool dark = g_app.darkTheme;
    const ThemePalette& palette = CurrentPalette();
    const auto pick = [dark](COLORREF d, COLORREF l) { return dark ? d : l; };
    switch (kind) {
    case Tok::Keyword:   return {pick(RGB(197, 134, 192), RGB(175, 0, 219)), false};
    case Tok::Type:      return {pick(RGB(78, 201, 176), RGB(38, 127, 153)), false};
    case Tok::String:    return {pick(RGB(206, 145, 120), RGB(163, 21, 21)), false};
    case Tok::Number:    return {pick(RGB(181, 206, 168), RGB(9, 134, 88)), false};
    case Tok::Comment:   return {pick(RGB(106, 153, 85), RGB(0, 128, 0)), false};
    case Tok::Function:  return {pick(RGB(220, 220, 170), RGB(121, 94, 38)), false};
    case Tok::Tag:       return {pick(RGB(86, 156, 214), RGB(128, 0, 0)), false};
    case Tok::Attribute: return {pick(RGB(156, 220, 254), RGB(229, 0, 0)), false};
    case Tok::Property:  return {pick(RGB(156, 220, 254), RGB(4, 81, 165)), false};
    case Tok::Constant:  return {pick(RGB(86, 156, 214), RGB(0, 0, 255)), false};
    case Tok::Meta:      return {pick(RGB(197, 134, 192), RGB(175, 0, 219)), false};
    case Tok::Variable:  return {pick(RGB(156, 220, 254), RGB(0, 16, 128)), false};
    case Tok::Heading:   return {pick(RGB(86, 156, 214), RGB(128, 0, 0)), true};
    case Tok::Emphasis:  return {palette.editorText, true};
    case Tok::Link:      return {pick(RGB(78, 201, 176), RGB(0, 112, 193)), false};
    case Tok::Error:     return {pick(RGB(244, 135, 113), RGB(205, 49, 49)), true};
    case Tok::Warning:   return {pick(RGB(229, 192, 123), RGB(191, 136, 3)), true};
    case Tok::Info:      return {pick(RGB(117, 190, 255), RGB(0, 112, 193)), false};
    case Tok::Added:     return {pick(RGB(129, 199, 132), RGB(22, 130, 52)), false};
    case Tok::Removed:   return {pick(RGB(244, 135, 113), RGB(205, 49, 49)), false};
    case Tok::Column0:   return {pick(RGB(156, 220, 254), RGB(4, 81, 165)), false};
    case Tok::Column1:   return {pick(RGB(206, 145, 120), RGB(163, 21, 21)), false};
    case Tok::Column2:   return {pick(RGB(181, 206, 168), RGB(9, 134, 88)), false};
    case Tok::Column3:   return {pick(RGB(197, 134, 192), RGB(175, 0, 219)), false};
    case Tok::Column4:   return {pick(RGB(220, 220, 170), RGB(121, 94, 38)), false};
    case Tok::Column5:   return {pick(RGB(78, 201, 176), RGB(38, 127, 153)), false};
    default:             return {palette.editorText, false};
    }
}

// ---- Language specs --------------------------------------------------------------

struct CodeSpec {
    std::vector<std::wstring> lineComments;
    std::wstring blockOpen, blockClose;
    std::wstring blockOpen2, blockClose2;
    std::wstring quotes = L"\"'";
    bool multilineQuotes = false;   // every quote may span lines (shell, PowerShell)
    bool backtickMultiline = true;  // `template` strings
    bool tripleQuotes = false;      // Python """ """
    bool backslashEscapes = true;
    bool hashMeta = false;          // #include / #region at line start
    bool decorators = false;        // @Decorator
    bool dollarVariables = false;   // $name, ${name}
    bool percentVariables = false;  // %name%, %~dp0, !name!
    bool dashIdentifiers = false;   // Get-ChildItem, -Parameter
    bool caseInsensitive = false;
    bool shellComments = false;     // '#' starts a comment only after whitespace
    bool macroBang = false;         // name!(...) is a macro call (Rust)
    std::unordered_set<std::wstring> keywords, types, constants, builtins;
};

std::unordered_set<std::wstring> WordSet(const wchar_t* words, bool lower = false) {
    std::unordered_set<std::wstring> set;
    std::wistringstream stream(words);
    for (std::wstring word; stream >> word;) set.insert(lower ? LowerInvariantSyntax(word) : word);
    return set;
}

const CodeSpec* SpecFor(int language) {
    static std::unordered_map<int, CodeSpec> specs = [] {
        std::unordered_map<int, CodeSpec> m;
        const wchar_t* jsKeywords = L"break case catch class const continue debugger default delete do else export extends finally for "
            L"function if import in instanceof let new return super switch this throw try typeof var void while with yield async await "
            L"of static get set from as";
        CodeSpec js;
        js.lineComments = {L"//"};
        js.blockOpen = L"/*"; js.blockClose = L"*/";
        js.quotes = L"\"'`";
        js.decorators = true;
        js.keywords = WordSet(jsKeywords);
        js.constants = WordSet(L"true false null undefined NaN Infinity");
        m[kLanguageJavaScript] = js;
        CodeSpec ts = js;
        for (const auto& w : WordSet(L"interface type enum implements namespace module declare abstract private protected public readonly keyof infer is satisfies override")) ts.keywords.insert(w);
        ts.types = WordSet(L"string number boolean any unknown never object symbol bigint");
        m[kLanguageTypeScript] = ts;

        CodeSpec c;
        c.lineComments = {L"//"};
        c.blockOpen = L"/*"; c.blockClose = L"*/";
        c.hashMeta = true;
        c.keywords = WordSet(L"alignas alignof auto break case catch class const constexpr consteval constinit const_cast continue "
            L"co_await co_return co_yield decltype default delete do dynamic_cast else enum explicit export extern for friend goto if "
            L"inline mutable namespace new noexcept operator private protected public register reinterpret_cast return sizeof static "
            L"static_assert static_cast struct switch template this thread_local throw try typedef typeid typename union using virtual "
            L"volatile while override final");
        c.types = WordSet(L"void bool char wchar_t char8_t char16_t char32_t short int long float double signed unsigned size_t "
            L"ptrdiff_t int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t intptr_t uintptr_t std string wstring "
            L"BOOL DWORD WORD BYTE UINT LONG HWND HANDLE LPARAM WPARAM LRESULT");
        c.constants = WordSet(L"true false nullptr NULL TRUE FALSE");
        m[kLanguageCpp] = c;

        CodeSpec cs = c;
        cs.keywords = WordSet(L"abstract as base break case catch checked class const continue default delegate do else enum event "
            L"explicit extern finally fixed for foreach goto if implicit in interface internal is lock namespace new operator out "
            L"override params private protected public readonly ref return sealed sizeof stackalloc static struct switch this throw "
            L"try typeof unchecked unsafe using virtual volatile while async await var get set init record yield where when nameof partial");
        cs.types = WordSet(L"bool byte sbyte char decimal double float int uint long ulong short ushort object string void dynamic nint nuint");
        cs.constants = WordSet(L"true false null");
        cs.decorators = false;
        m[kLanguageCSharp] = cs;

        CodeSpec java = c;
        java.hashMeta = false;
        java.decorators = true;
        java.keywords = WordSet(L"abstract assert break case catch class continue default do else enum extends final finally for if "
            L"implements import instanceof interface native new package private protected public return static strictfp super switch "
            L"synchronized this throw throws transient try volatile while var record yield sealed permits fun val when object "
            L"companion data suspend override open internal lateinit");
        java.types = WordSet(L"boolean byte char short int long float double void String Object Integer Long Double Boolean Unit Any");
        java.constants = WordSet(L"true false null");
        m[kLanguageJava] = java;

        CodeSpec go = c;
        go.hashMeta = false;
        go.quotes = L"\"'`";
        go.keywords = WordSet(L"break case chan const continue default defer else fallthrough for func go goto if import interface "
            L"map package range return select struct switch type var");
        go.types = WordSet(L"bool byte complex64 complex128 error float32 float64 int int8 int16 int32 int64 rune string uint uint8 "
            L"uint16 uint32 uint64 uintptr any");
        go.constants = WordSet(L"true false nil iota");
        m[kLanguageGo] = go;

        CodeSpec rust = c;
        rust.hashMeta = false;
        rust.macroBang = true;
        rust.keywords = WordSet(L"as async await break const continue crate dyn else enum extern fn for if impl in let loop match mod "
            L"move mut pub ref return self Self static struct super trait type unsafe use where while");
        rust.types = WordSet(L"i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 bool char str String Vec Option Result Box");
        rust.constants = WordSet(L"true false None Some Ok Err");
        m[kLanguageRust] = rust;

        CodeSpec php = c;
        php.hashMeta = false;
        php.lineComments = {L"//", L"#"};
        php.dollarVariables = true;
        php.keywords = WordSet(L"abstract and array as break callable case catch class clone const continue declare default do echo "
            L"else elseif empty enddeclare endfor endforeach endif endswitch endwhile extends final finally fn for foreach function "
            L"global goto if implements include include_once instanceof insteadof interface isset list match namespace new or print "
            L"private protected public readonly require require_once return static switch throw trait try unset use var while xor yield");
        php.types = WordSet(L"int float string bool void mixed iterable self parent");
        php.constants = WordSet(L"true false null TRUE FALSE NULL");
        m[kLanguagePhp] = php;

        CodeSpec py;
        py.lineComments = {L"#"};
        py.tripleQuotes = true;
        py.decorators = true;
        py.keywords = WordSet(L"and as assert async await break class continue def del elif else except finally for from global if "
            L"import in is lambda nonlocal not or pass raise return try while with yield match case");
        py.types = WordSet(L"int float str bool list dict set tuple bytes object type");
        py.constants = WordSet(L"True False None self cls");
        py.builtins = WordSet(L"print len range open isinstance enumerate zip map filter sorted super");
        m[kLanguagePython] = py;

        CodeSpec sql;
        sql.lineComments = {L"--", L"#"};
        sql.blockOpen = L"/*"; sql.blockClose = L"*/";
        sql.quotes = L"\"'`";
        sql.backslashEscapes = false;
        sql.caseInsensitive = true;
        sql.keywords = WordSet(L"select from where and or not insert into values update set delete create table alter drop index view "
            L"join left right inner outer full cross on as group by order having limit offset union all distinct case when then else "
            L"end is null in exists between like primary key foreign references default constraint unique check if begin commit "
            L"rollback transaction declare procedure function returns return trigger database schema use show grant revoke with asc "
            L"desc top truncate replace merge using natural over partition cast add column", true);
        sql.types = WordSet(L"int integer bigint smallint tinyint decimal numeric float real double char varchar nvarchar text date "
            L"datetime timestamp time boolean bool blob json jsonb uuid serial", true);
        sql.constants = WordSet(L"true false", true);
        sql.builtins = WordSet(L"count sum avg min max now coalesce concat substring lower upper", true);
        m[kLanguageSql] = sql;

        CodeSpec shell;
        shell.lineComments = {L"#"};
        shell.shellComments = true;
        shell.quotes = L"\"'`";
        shell.multilineQuotes = true;
        shell.dollarVariables = true;
        shell.keywords = WordSet(L"if then else elif fi for while until do done case esac in function return break continue local "
            L"export readonly declare unset shift exit source alias select time trap eval exec set");
        shell.builtins = WordSet(L"echo printf cd ls cp mv rm mkdir grep sed awk cat chmod chown sudo apt yum dnf npm npx pnpm yarn "
            L"git docker curl wget tar ssh systemctl node python pip");
        shell.constants = WordSet(L"true false");
        m[kLanguageShell] = shell;

        CodeSpec ps;
        ps.lineComments = {L"#"};
        ps.blockOpen = L"<#"; ps.blockClose = L"#>";
        ps.multilineQuotes = true;
        ps.backslashEscapes = false;
        ps.dollarVariables = true;
        ps.dashIdentifiers = true;
        ps.caseInsensitive = true;
        ps.keywords = WordSet(L"begin break catch class continue data do dynamicparam else elseif end enum exit filter finally for "
            L"foreach from function if in param process return switch throw trap try until using while", true);
        m[kLanguagePowerShell] = ps;

        CodeSpec bat;
        bat.lineComments = {L"::"};
        bat.quotes = L"\"";
        bat.backslashEscapes = false;
        bat.percentVariables = true;
        bat.caseInsensitive = true;
        bat.keywords = WordSet(L"echo set if else for in do goto call exit not exist defined errorlevel setlocal endlocal shift pause "
            L"cls cd pushd popd start title copy del move mkdir md rd rmdir type find findstr equ neq lss leq gtr geq nul off on", true);
        m[kLanguageBatch] = bat;
        return m;
    }();
    const auto it = specs.find(language);
    return it == specs.end() ? nullptr : &it->second;
}

// ---- Lexer ---------------------------------------------------------------------------

bool IsIdentStart(wchar_t c) { return c == L'_' || (c < 128 ? iswalpha(c) != 0 : (iswalpha(c) && c < 0x2E80)); }
bool IsIdentChar(wchar_t c) { return IsIdentStart(c) || (c >= L'0' && c <= L'9'); }

class SyntaxLexer {
public:
    SyntaxLexer(const std::wstring& text, std::vector<SyntaxToken>& tokens) : text_(text), tokens_(tokens) {}

    void Run(int language, size_t from, size_t to) {
        switch (language) {
        case kLanguageHtml:
        case kLanguageXml: Markup(from, to); break;
        case kLanguageCss: Css(from, to); break;
        case kLanguageJson: Json(from, to); break;
        case kLanguageMarkdown: Markdown(from, to); break;
        case kLanguageYaml: Yaml(from, to); break;
        case kLanguageConfig: Config(from, to); break;
        case kLanguageDockerfile: Dockerfile(from, to); break;
        case kLanguageCsv: Csv(from, to); break;
        case kLanguageLog: Log(from, to); break;
        case kLanguageDiff: Diff(from, to); break;
        case kLanguageBatch: Batch(from, to); break;
        default:
            if (const CodeSpec* spec = SpecFor(language)) Code(*spec, from, to);
            break;
        }
    }

private:
    const std::wstring& text_;
    std::vector<SyntaxToken>& tokens_;

    void Add(size_t start, size_t end, Tok kind) {
        if (end > start && kind != Tok::Plain) tokens_.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(end - start), kind});
    }
    bool At(size_t i, const std::wstring& s) const { return !s.empty() && text_.compare(i, s.size(), s) == 0; }
    bool AtNoCase(size_t i, const wchar_t* s) const {
        const size_t n = wcslen(s);
        return i + n <= text_.size() && _wcsnicmp(text_.c_str() + i, s, n) == 0;
    }
    size_t LineEnd(size_t i, size_t to) const {
        const size_t end = text_.find_first_of(L"\r\n", i);
        return end == std::wstring::npos ? to : std::min(end, to);
    }
    size_t NextLine(size_t lineEnd, size_t to) const {
        if (lineEnd < to && text_[lineEnd] == L'\r' && lineEnd + 1 < to && text_[lineEnd + 1] == L'\n') return lineEnd + 2;
        return std::min(lineEnd + 1, to);
    }
    size_t SkipSpaces(size_t i, size_t to) const {
        while (i < to && (text_[i] == L' ' || text_[i] == L'\t')) ++i;
        return i;
    }
    size_t NumberEnd(size_t i, size_t to) const {
        size_t j = i;
        const bool hex = j + 1 < to && text_[j] == L'0' && (text_[j + 1] == L'x' || text_[j + 1] == L'X');
        if (hex) j += 2;
        while (j < to) {
            const wchar_t c = text_[j];
            if (iswalnum(c) || c == L'_' || (c == L'.' && j + 1 < to && iswdigit(text_[j + 1]))) { ++j; continue; }
            if (!hex && (c == L'+' || c == L'-') && j > i && (text_[j - 1] == L'e' || text_[j - 1] == L'E')) { ++j; continue; }
            break;
        }
        return j;
    }
    // Ends after the closing quote (or at the line end for an unterminated one).
    size_t StringEnd(size_t i, size_t to, wchar_t quote, bool multiline, bool escapes) const {
        size_t j = i + 1;
        while (j < to) {
            const wchar_t c = text_[j];
            if (escapes && c == L'\\') { j += 2; continue; }
            if (c == quote) return j + 1;
            if ((c == L'\r' || c == L'\n') && !multiline) return j;
            ++j;
        }
        return to;
    }
    wchar_t PreviousNonSpace(size_t i, size_t from) const {
        while (i > from) {
            const wchar_t c = text_[--i];
            if (c != L' ' && c != L'\t') return c;
        }
        return 0;
    }
    std::wstring Lower(size_t start, size_t end) const { return LowerInvariantSyntax(text_.substr(start, end - start)); }

    void Code(const CodeSpec& spec, size_t from, size_t to) {
        size_t i = from;
        bool lineStart = true;
        while (i < to) {
            const wchar_t c = text_[i];
            if (c == L'\r' || c == L'\n') { lineStart = true; ++i; continue; }
            if (c == L' ' || c == L'\t') { ++i; continue; }
            if (At(i, spec.blockOpen)) {
                const size_t close = text_.find(spec.blockClose, i + spec.blockOpen.size());
                const size_t end = close == std::wstring::npos ? to : std::min(to, close + spec.blockClose.size());
                Add(i, end, Tok::Comment);
                i = end; lineStart = false; continue;
            }
            bool comment = false;
            for (const std::wstring& marker : spec.lineComments) {
                if (!At(i, marker)) continue;
                if (spec.shellComments && i > from && text_[i - 1] != L' ' && text_[i - 1] != L'\t' && text_[i - 1] != L'\r' && text_[i - 1] != L'\n') continue;
                const size_t end = LineEnd(i, to);
                Add(i, end, Tok::Comment);
                i = end;
                comment = true;
                break;
            }
            if (comment) continue;
            if (spec.hashMeta && c == L'#' && lineStart) {
                const size_t end = LineEnd(i, to);
                // #include <x> / "x": keep the path as a string.
                const size_t quote = text_.find_first_of(L"<\"", i);
                if (quote != std::wstring::npos && quote < end && text_.compare(i, 8, L"#include") == 0) {
                    Add(i, quote, Tok::Meta);
                    Add(quote, end, Tok::String);
                } else {
                    Add(i, end, Tok::Meta);
                }
                i = end; continue;
            }
            lineStart = false;
            if (spec.quotes.find(c) != std::wstring::npos) {
                size_t end;
                if (spec.tripleQuotes && i + 2 < to && text_[i + 1] == c && text_[i + 2] == c) {
                    const size_t close = text_.find(std::wstring(3, c), i + 3);
                    end = close == std::wstring::npos ? to : std::min(to, close + 3);
                } else {
                    end = StringEnd(i, to, c, spec.multilineQuotes || (c == L'`' && spec.backtickMultiline), spec.backslashEscapes);
                }
                Add(i, end, Tok::String);
                i = end; continue;
            }
            if (iswdigit(c) || (c == L'.' && i + 1 < to && iswdigit(text_[i + 1]) && (i == from || !IsIdentChar(text_[i - 1])))) {
                if (i > from && IsIdentChar(text_[i - 1])) { ++i; continue; }
                const size_t end = NumberEnd(i, to);
                Add(i, end, Tok::Number);
                i = end; continue;
            }
            if (spec.decorators && c == L'@' && i + 1 < to && IsIdentStart(text_[i + 1])) {
                size_t j = i + 1;
                while (j < to && (IsIdentChar(text_[j]) || text_[j] == L'.')) ++j;
                Add(i, j, Tok::Meta);
                i = j; continue;
            }
            if (spec.dollarVariables && c == L'$' && i + 1 < to) {
                size_t j = i + 1;
                if (text_[j] == L'{') {
                    const size_t close = text_.find(L'}', j);
                    j = close == std::wstring::npos || close > LineEnd(i, to) ? j + 1 : close + 1;
                } else {
                    while (j < to && (IsIdentChar(text_[j]) || (spec.dashIdentifiers && text_[j] == L':'))) ++j;
                    if (j == i + 1 && j < to && (iswdigit(text_[j]) || wcschr(L"@#?*!$-_", text_[j]))) ++j;
                }
                if (j > i + 1) {
                    Add(i, j, Tok::Variable);
                    i = j; continue;
                }
            }
            if (spec.percentVariables && (c == L'%' || c == L'!') && i + 1 < to) {
                size_t j = i + 1;
                if (c == L'%' && (text_[j] == L'%' || text_[j] == L'~' || iswdigit(text_[j]))) {
                    if (text_[j] == L'%') ++j;
                    while (j < to && (iswalnum(text_[j]) || text_[j] == L'~')) ++j;
                    Add(i, j, Tok::Variable);
                    i = j; continue;
                }
                const size_t close = text_.find(c, j);
                if (close != std::wstring::npos && close > j && close - j < 64 && close < LineEnd(i, to)) {
                    bool simple = true;
                    for (size_t k = j; k < close; ++k) {
                        if (!(IsIdentChar(text_[k]) || text_[k] == L':' || text_[k] == L'~' || text_[k] == L'=' || text_[k] == L'.')) simple = false;
                    }
                    if (simple) {
                        Add(i, close + 1, Tok::Variable);
                        i = close + 1; continue;
                    }
                }
            }
            if (spec.dashIdentifiers && c == L'-' && i + 1 < to && IsIdentStart(text_[i + 1]) &&
                (i == from || text_[i - 1] == L' ' || text_[i - 1] == L'\t' || text_[i - 1] == L'(')) {
                size_t j = i + 1;
                while (j < to && IsIdentChar(text_[j])) ++j;
                Add(i, j, Tok::Attribute);
                i = j; continue;
            }
            if (IsIdentStart(c)) {
                size_t j = i + 1;
                while (j < to && (IsIdentChar(text_[j]) ||
                       (spec.dashIdentifiers && text_[j] == L'-' && j + 1 < to && IsIdentStart(text_[j + 1])))) ++j;
                // String prefixes: r"..", f'..', b"..", u8"..", L"..", @"..".
                if (j < to && spec.quotes.find(text_[j]) != std::wstring::npos && j - i <= 2) {
                    bool prefix = true;
                    for (size_t k = i; k < j; ++k) prefix = prefix && wcschr(L"rRbBfFuUL8", text_[k]) != nullptr;
                    if (prefix) {
                        const wchar_t q = text_[j];
                        size_t end;
                        if (spec.tripleQuotes && j + 2 < to && text_[j + 1] == q && text_[j + 2] == q) {
                            const size_t close = text_.find(std::wstring(3, q), j + 3);
                            end = close == std::wstring::npos ? to : std::min(to, close + 3);
                        } else {
                            end = StringEnd(j, to, q, spec.multilineQuotes, spec.backslashEscapes && text_[i] != L'r' && text_[i] != L'R');
                        }
                        Add(i, end, Tok::String);
                        i = end; continue;
                    }
                }
                const std::wstring word = text_.substr(i, j - i);
                const std::wstring key = spec.caseInsensitive ? LowerInvariantSyntax(word) : word;
                Tok kind = Tok::Plain;
                if (spec.keywords.count(key)) kind = Tok::Keyword;
                else if (spec.constants.count(key)) kind = Tok::Constant;
                else if (spec.types.count(key)) kind = Tok::Type;
                else if (spec.builtins.count(key)) kind = Tok::Function;
                else {
                    const size_t next = SkipSpaces(j, to);
                    const wchar_t prev = PreviousNonSpace(i, from);
                    const bool call = next < to && text_[next] == L'(';
                    const bool macro = spec.macroBang && next < to && text_[next] == L'!';
                    bool hasLower = false, hasLetter = false;
                    for (const wchar_t ch : word) {
                        hasLower = hasLower || iswlower(ch);
                        hasLetter = hasLetter || iswalpha(ch);
                    }
                    if (spec.dashIdentifiers && word.find(L'-') != std::wstring::npos) kind = Tok::Function;  // cmdlets
                    else if (call || macro) kind = Tok::Function;
                    else if (prev == L'.' || (prev == L'>' && i >= 2 && text_[i - 2] == L'-')) kind = Tok::Property;
                    else if (iswupper(word[0]) && hasLower && word[0] < 128) kind = Tok::Type;
                    else if (word.size() > 1 && hasLetter && !hasLower && word[0] < 128) kind = Tok::Constant;
                }
                Add(i, j, kind);
                i = j; continue;
            }
            ++i;
        }
    }

    void Markup(size_t from, size_t to) {
        size_t i = from;
        while (i < to) {
            if (At(i, L"<!--")) {
                const size_t close = text_.find(L"-->", i + 4);
                const size_t end = close == std::wstring::npos ? to : std::min(to, close + 3);
                Add(i, end, Tok::Comment);
                i = end; continue;
            }
            if (At(i, L"<![CDATA[")) {
                const size_t close = text_.find(L"]]>", i);
                const size_t end = close == std::wstring::npos ? to : std::min(to, close + 3);
                Add(i, end, Tok::String);
                i = end; continue;
            }
            const wchar_t c = text_[i];
            if (c == L'<' && i + 1 < to && (text_[i + 1] == L'!' || text_[i + 1] == L'?')) {
                const size_t close = text_.find(L'>', i);
                const size_t end = close == std::wstring::npos ? to : std::min(to, close + 1);
                Add(i, end, Tok::Meta);
                i = end; continue;
            }
            if (c == L'<' && i + 1 < to && (IsIdentStart(text_[i + 1]) || text_[i + 1] == L'/')) {
                size_t j = i + 1;
                const bool closing = text_[j] == L'/';
                if (closing) ++j;
                const size_t nameStart = j;
                while (j < to && (IsIdentChar(text_[j]) || text_[j] == L'-' || text_[j] == L':' || text_[j] == L'.')) ++j;
                const std::wstring name = Lower(nameStart, j);
                Add(i, j, Tok::Tag);
                size_t k = j;
                while (k < to) {
                    k = SkipSpaces(k, to);
                    if (k >= to) break;
                    const wchar_t ch = text_[k];
                    if (ch == L'\r' || ch == L'\n') { ++k; continue; }
                    if (ch == L'>') { Add(k, k + 1, Tok::Tag); ++k; break; }
                    if (ch == L'/' && k + 1 < to && text_[k + 1] == L'>') { Add(k, k + 2, Tok::Tag); k += 2; break; }
                    if (ch == L'<') break;  // broken tag
                    if (ch == L'=') {
                        k = SkipSpaces(k + 1, to);
                        if (k < to && (text_[k] == L'"' || text_[k] == L'\'')) {
                            const size_t end = StringEnd(k, to, text_[k], true, false);
                            Add(k, end, Tok::String);
                            k = end;
                        } else {
                            const size_t start = k;
                            while (k < to && !iswspace(text_[k]) && text_[k] != L'>') ++k;
                            Add(start, k, Tok::String);
                        }
                        continue;
                    }
                    const size_t start = k;
                    while (k < to && !iswspace(text_[k]) && text_[k] != L'=' && text_[k] != L'>' && text_[k] != L'/' && text_[k] != L'<') ++k;
                    if (k == start) { ++k; continue; }
                    Add(start, k, Tok::Attribute);
                }
                i = k;
                if (!closing && (name == L"script" || name == L"style")) {
                    size_t close = i;
                    while ((close = text_.find(L"</", close)) != std::wstring::npos && !AtNoCase(close + 2, name.c_str())) close += 2;
                    const size_t end = close == std::wstring::npos ? to : std::min(close, to);
                    Run(name == L"script" ? kLanguageJavaScript : kLanguageCss, i, end);
                    i = end;
                }
                continue;
            }
            if (c == L'&') {
                size_t j = i + 1;
                while (j < to && j - i < 12 && (iswalnum(text_[j]) || text_[j] == L'#')) ++j;
                if (j < to && text_[j] == L';' && j > i + 1) {
                    Add(i, j + 1, Tok::Constant);
                    i = j + 1; continue;
                }
            }
            ++i;
        }
    }

    void Css(size_t from, size_t to) {
        int depth = 0;
        bool inValue = false;
        size_t i = from;
        while (i < to) {
            const wchar_t c = text_[i];
            if (At(i, L"/*")) {
                const size_t close = text_.find(L"*/", i + 2);
                const size_t end = close == std::wstring::npos ? to : std::min(to, close + 2);
                Add(i, end, Tok::Comment);
                i = end; continue;
            }
            if (At(i, L"//") && (i == from || iswspace(text_[i - 1]) || text_[i - 1] == L';' || text_[i - 1] == L'{' || text_[i - 1] == L'}')) {
                const size_t end = LineEnd(i, to);
                Add(i, end, Tok::Comment);
                i = end; continue;
            }
            if (c == L'"' || c == L'\'') {
                const size_t end = StringEnd(i, to, c, false, true);
                Add(i, end, Tok::String);
                i = end; continue;
            }
            if (c == L'{') { ++depth; inValue = false; ++i; continue; }
            if (c == L'}') { depth = std::max(0, depth - 1); inValue = false; ++i; continue; }
            if (c == L';') { inValue = false; ++i; continue; }
            if (c == L'@' && i + 1 < to && IsIdentStart(text_[i + 1])) {
                size_t j = i + 1;
                while (j < to && (IsIdentChar(text_[j]) || text_[j] == L'-')) ++j;
                Add(i, j, inValue ? Tok::Variable : Tok::Keyword);
                i = j; continue;
            }
            if ((c == L'$' && i + 1 < to && IsIdentStart(text_[i + 1])) || At(i, L"--")) {
                size_t j = i + (c == L'$' ? 1 : 2);
                while (j < to && (IsIdentChar(text_[j]) || text_[j] == L'-')) ++j;
                if (j > i + 1) {
                    Add(i, j, Tok::Variable);
                    i = j; continue;
                }
            }
            if (c == L'!' && AtNoCase(i, L"!important")) {
                Add(i, i + 10, Tok::Keyword);
                i += 10; continue;
            }
            if (c == L'#' && inValue) {
                size_t j = i + 1;
                while (j < to && iswxdigit(text_[j])) ++j;
                Add(i, j, Tok::Number);
                i = j; continue;
            }
            if (iswdigit(c) || (c == L'.' && i + 1 < to && iswdigit(text_[i + 1]) && inValue)) {
                size_t j = i;
                while (j < to && (iswdigit(text_[j]) || text_[j] == L'.')) ++j;
                while (j < to && (iswalpha(text_[j]) || text_[j] == L'%')) ++j;  // units
                Add(i, j, Tok::Number);
                i = j; continue;
            }
            if (c == L':' && !inValue && i + 1 < to && (text_[i + 1] == L':' || IsIdentStart(text_[i + 1]))) {
                // A pseudo-class in a selector (a:hover) vs. a property colon (color: red).
                size_t j = i + 1;
                if (text_[j] == L':') ++j;
                const size_t nameStart = j;
                while (j < to && (IsIdentChar(text_[j]) || text_[j] == L'-')) ++j;
                const size_t after = SkipSpaces(j, to);
                const bool selector = depth == 0 || (after < to && (text_[after] == L'{' || text_[after] == L',' || text_[after] == L'('));
                if (selector && j > nameStart) {
                    Add(i, j, Tok::Keyword);
                    i = j; continue;
                }
                inValue = true;
                ++i; continue;
            }
            if (c == L':') { inValue = true; ++i; continue; }
            if (IsIdentStart(c) || c == L'-' || ((c == L'.' || c == L'#') && i + 1 < to && (IsIdentStart(text_[i + 1]) || text_[i + 1] == L'-'))) {
                size_t j = i + 1;
                while (j < to && (IsIdentChar(text_[j]) || text_[j] == L'-')) ++j;
                const size_t next = SkipSpaces(j, to);
                Tok kind;
                if (next < to && text_[next] == L'(') kind = Tok::Function;
                else if (inValue) kind = Tok::Constant;
                else if (depth > 0 && next < to && text_[next] == L':' && !(next + 1 < to && IsIdentStart(text_[next + 1]) && LooksLikeSelector(next, to))) kind = Tok::Property;
                else kind = Tok::Tag;
                Add(i, j, kind);
                i = j; continue;
            }
            ++i;
        }
    }
    // "a:hover {" — a colon followed by a name and then '{' on the same line.
    bool LooksLikeSelector(size_t colon, size_t to) const {
        const size_t end = LineEnd(colon, to);
        const size_t brace = text_.find(L'{', colon);
        const size_t semi = text_.find(L';', colon);
        return brace != std::wstring::npos && brace < end && (semi == std::wstring::npos || semi > brace);
    }

    void Json(size_t from, size_t to) {
        size_t i = from;
        while (i < to) {
            const wchar_t c = text_[i];
            if (At(i, L"//")) { const size_t end = LineEnd(i, to); Add(i, end, Tok::Comment); i = end; continue; }
            if (At(i, L"/*")) {
                const size_t close = text_.find(L"*/", i + 2);
                const size_t end = close == std::wstring::npos ? to : std::min(to, close + 2);
                Add(i, end, Tok::Comment);
                i = end; continue;
            }
            if (c == L'"') {
                const size_t end = StringEnd(i, to, c, false, true);
                const size_t next = SkipSpaces(end, to);
                Add(i, end, next < to && text_[next] == L':' ? Tok::Property : Tok::String);
                i = end; continue;
            }
            if (iswdigit(c) || (c == L'-' && i + 1 < to && iswdigit(text_[i + 1]))) {
                const size_t end = NumberEnd(c == L'-' ? i + 1 : i, to);
                Add(i, end, Tok::Number);
                i = end; continue;
            }
            if (IsIdentStart(c)) {
                size_t j = i;
                while (j < to && IsIdentChar(text_[j])) ++j;
                const std::wstring word = text_.substr(i, j - i);
                if (word == L"true" || word == L"false" || word == L"null") Add(i, j, Tok::Constant);
                i = j; continue;
            }
            ++i;
        }
    }

    // A value after '=' / ':' in config-like files.
    void Value(size_t start, size_t end) {
        start = SkipSpaces(start, end);
        while (end > start && (text_[end - 1] == L' ' || text_[end - 1] == L'\t')) --end;
        if (start >= end) return;
        const wchar_t c = text_[start];
        if (c == L'"' || c == L'\'') { Add(start, end, Tok::String); return; }
        const std::wstring lower = Lower(start, end);
        if (lower == L"true" || lower == L"false" || lower == L"yes" || lower == L"no" || lower == L"on" ||
            lower == L"off" || lower == L"null" || lower == L"~" || lower == L"none") {
            Add(start, end, Tok::Constant);
            return;
        }
        bool numeric = true;
        for (size_t k = start; k < end; ++k) {
            const wchar_t ch = text_[k];
            if (!(iswdigit(ch) || ch == L'.' || ch == L'-' || ch == L'+' || ch == L'_' || ch == L'e' || ch == L'E')) numeric = false;
        }
        if (numeric && iswdigit(text_[end - 1])) { Add(start, end, Tok::Number); return; }
        if (c == L'&' || c == L'*') { Add(start, end, Tok::Variable); return; }
        if ((c == L'|' || c == L'>') && end - start <= 2) { Add(start, end, Tok::Keyword); return; }
        if (c == L'[' || c == L'{') {  // inline arrays / tables: colour the pieces
            Code(*SpecFor(kLanguageJavaScript), start, end);
            return;
        }
        Add(start, end, Tok::String);
    }

    // INI, TOML, .env, .properties, .gitignore and friends.
    void Config(size_t from, size_t to) {
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            const size_t p = SkipSpaces(line, end);
            if (p < end) {
                const wchar_t c = text_[p];
                if (c == L'#' || c == L';' || At(p, L"//")) {
                    Add(p, end, Tok::Comment);
                } else if (c == L'[') {
                    const size_t close = text_.find(L']', p);
                    Add(p, close == std::wstring::npos || close > end ? end : close + 1, Tok::Tag);
                } else {
                    size_t keyStart = p;
                    if (AtNoCase(p, L"export ") || AtNoCase(p, L"set ")) {
                        const size_t word = text_.find(L' ', p);
                        Add(p, word, Tok::Keyword);
                        keyStart = SkipSpaces(word, end);
                    }
                    const size_t equals = text_.find_first_of(L"=:", keyStart);
                    if (equals != std::wstring::npos && equals < end) {
                        size_t keyEnd = equals;
                        while (keyEnd > keyStart && (text_[keyEnd - 1] == L' ' || text_[keyEnd - 1] == L'\t')) --keyEnd;
                        Add(keyStart, keyEnd, Tok::Property);
                        // Inline comment after a value: "value  # note"
                        size_t valueEnd = end;
                        const size_t hash = text_.find(L" #", equals);
                        if (hash != std::wstring::npos && hash < end && text_.find_first_of(L"\"'", equals) > hash) {
                            Add(hash + 1, end, Tok::Comment);
                            valueEnd = hash;
                        }
                        Value(equals + 1, valueEnd);
                    }
                }
            }
            line = NextLine(end, to);
        }
    }

    void Yaml(size_t from, size_t to) {
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            size_t p = SkipSpaces(line, end);
            if (p < end) {
                if (text_[p] == L'#') {
                    Add(p, end, Tok::Comment);
                } else if (At(p, L"---") || At(p, L"...")) {
                    Add(p, end, Tok::Meta);
                } else {
                    if (text_[p] == L'-' && (p + 1 == end || text_[p + 1] == L' ')) {
                        Add(p, p + 1, Tok::Keyword);
                        p = SkipSpaces(p + 1, end);
                    }
                    // Strip a trailing comment that is not inside quotes.
                    size_t contentEnd = end;
                    bool quoted = false;
                    wchar_t quote = 0;
                    for (size_t k = p; k < end; ++k) {
                        const wchar_t ch = text_[k];
                        if (quoted) { if (ch == quote) quoted = false; continue; }
                        if (ch == L'"' || ch == L'\'') { quoted = true; quote = ch; continue; }
                        if (ch == L'#' && k > p && (text_[k - 1] == L' ' || text_[k - 1] == L'\t')) {
                            Add(k, end, Tok::Comment);
                            contentEnd = k;
                            break;
                        }
                    }
                    size_t colon = std::wstring::npos;
                    if (p < contentEnd && text_[p] != L'"' && text_[p] != L'\'' && text_[p] != L'[' && text_[p] != L'{') {
                        for (size_t k = p; k < contentEnd; ++k) {
                            if (text_[k] == L':' && (k + 1 == contentEnd || text_[k + 1] == L' ' || text_[k + 1] == L'\t')) { colon = k; break; }
                        }
                    }
                    if (colon != std::wstring::npos) {
                        Add(p, colon, Tok::Property);
                        Value(colon + 1, contentEnd);
                    } else {
                        Value(p, contentEnd);
                    }
                }
            }
            line = NextLine(end, to);
        }
    }

    void Markdown(size_t from, size_t to) {
        bool inFence = false;
        int fenceLanguage = kLanguagePlain;
        size_t codeStart = 0;
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            const size_t next = NextLine(end, to);
            const size_t p = SkipSpaces(line, end);
            const bool fence = At(p, L"```") || At(p, L"~~~");
            if (inFence) {
                if (fence) {
                    if (fenceLanguage != kLanguagePlain) Run(fenceLanguage, codeStart, line);
                    else Add(codeStart, line, Tok::String);
                    Add(p, end, Tok::Meta);
                    inFence = false;
                }
                line = next;
                continue;
            }
            if (fence) {
                inFence = true;
                size_t infoStart = SkipSpaces(p + 3, end), infoEnd = infoStart;
                while (infoEnd < end && !iswspace(text_[infoEnd]) && text_[infoEnd] != L'{') ++infoEnd;
                fenceLanguage = LanguageFromName(text_.substr(infoStart, infoEnd - infoStart));
                Add(p, end, Tok::Meta);
                codeStart = next;
                line = next;
                continue;
            }
            size_t hashes = p;
            while (hashes < end && text_[hashes] == L'#') ++hashes;
            if (hashes > p && hashes - p <= 6 && (hashes == end || text_[hashes] == L' ')) {
                Add(p, end, Tok::Heading);
            } else if (p < end && text_[p] == L'>') {
                Add(p, end, Tok::Comment);
            } else if (end - p >= 3 && (At(p, L"---") || At(p, L"***") || At(p, L"___")) &&
                       text_.find_first_not_of(text_[p], p) >= end) {
                Add(p, end, Tok::Meta);
            } else {
                size_t q = p;
                if (q < end && (text_[q] == L'-' || text_[q] == L'*' || text_[q] == L'+') && q + 1 < end && text_[q + 1] == L' ') {
                    Add(q, q + 1, Tok::Keyword);
                    q += 2;
                } else {
                    size_t digits = q;
                    while (digits < end && iswdigit(text_[digits])) ++digits;
                    if (digits > q && digits + 1 < end && (text_[digits] == L'.' || text_[digits] == L')') && text_[digits + 1] == L' ') {
                        Add(q, digits + 1, Tok::Keyword);
                        q = digits + 2;
                    }
                }
                Inline(q, end);
            }
            line = next;
        }
        if (inFence && codeStart < to) {
            if (fenceLanguage != kLanguagePlain) Run(fenceLanguage, codeStart, to);
            else Add(codeStart, to, Tok::String);
        }
    }
    void Inline(size_t i, size_t end) {
        while (i < end) {
            const wchar_t c = text_[i];
            if (c == L'`') {
                const size_t close = text_.find(L'`', i + 1);
                if (close != std::wstring::npos && close < end) { Add(i, close + 1, Tok::String); i = close + 1; continue; }
            }
            if ((c == L'*' || c == L'_') && i + 1 < end && text_[i + 1] == c) {
                const size_t close = text_.find(std::wstring(2, c), i + 2);
                if (close != std::wstring::npos && close < end) { Add(i, close + 2, Tok::Emphasis); i = close + 2; continue; }
            }
            if (c == L'[' || (c == L'!' && i + 1 < end && text_[i + 1] == L'[')) {
                const size_t closeText = text_.find(L"](", i);
                if (closeText != std::wstring::npos && closeText < end) {
                    const size_t closeUrl = text_.find(L')', closeText);
                    if (closeUrl != std::wstring::npos && closeUrl < end) { Add(i, closeUrl + 1, Tok::Link); i = closeUrl + 1; continue; }
                }
            }
            if (At(i, L"http://") || At(i, L"https://")) {
                size_t j = i;
                while (j < end && !iswspace(text_[j]) && text_[j] != L')' && text_[j] != L'>') ++j;
                Add(i, j, Tok::Link);
                i = j; continue;
            }
            ++i;
        }
    }

    void Dockerfile(size_t from, size_t to) {
        static const std::unordered_set<std::wstring> instructions = WordSet(L"from run cmd label maintainer expose env add copy "
            L"entrypoint volume user workdir arg onbuild stopsignal healthcheck shell as", true);
        const CodeSpec* shell = SpecFor(kLanguageShell);
        bool continuation = false;
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            size_t p = SkipSpaces(line, end);
            if (p < end && text_[p] == L'#') {
                Add(p, end, Tok::Comment);
            } else if (p < end) {
                size_t word = p;
                while (word < end && IsIdentChar(text_[word])) ++word;
                if (!continuation && instructions.count(Lower(p, word))) {
                    Add(p, word, Tok::Keyword);
                    p = word;
                }
                Code(*shell, p, end);
            }
            continuation = end > line && text_[end - 1] == L'\\';
            line = NextLine(end, to);
        }
    }

    void Batch(size_t from, size_t to) {
        const CodeSpec* spec = SpecFor(kLanguageBatch);
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            size_t p = SkipSpaces(line, end);
            if (p < end && text_[p] == L'@') ++p;
            if (AtNoCase(p, L"rem") && (p + 3 == end || iswspace(text_[p + 3]))) Add(p, end, Tok::Comment);
            else if (At(p, L"::")) Add(p, end, Tok::Comment);
            else if (p < end && text_[p] == L':') Add(p, end, Tok::Tag);
            else Code(*spec, p, end);
            line = NextLine(end, to);
        }
    }

    // Rainbow columns, like VS Code's Rainbow CSV.
    void Csv(size_t from, size_t to) {
        const size_t firstEnd = LineEnd(from, to);
        const std::wstring firstLine = text_.substr(from, firstEnd - from);
        wchar_t delimiter = L',';
        const auto count = [&](wchar_t ch) { return std::count(firstLine.begin(), firstLine.end(), ch); };
        if (count(L'\t') > count(delimiter)) delimiter = L'\t';
        if (count(L';') > count(delimiter)) delimiter = L';';
        static constexpr Tok columns[] = {Tok::Column0, Tok::Column1, Tok::Column2, Tok::Column3, Tok::Column4, Tok::Column5};
        size_t i = from;
        int column = 0;
        size_t fieldStart = from;
        bool quoted = false;
        while (i <= to) {
            const wchar_t c = i < to ? text_[i] : L'\n';
            if (quoted) {
                if (c == L'"') {
                    if (i + 1 < to && text_[i + 1] == L'"') { i += 2; continue; }
                    quoted = false;
                }
                ++i;
                continue;
            }
            if (c == L'"' && i == fieldStart) { quoted = true; ++i; continue; }
            if (c == delimiter || c == L'\r' || c == L'\n') {
                Add(fieldStart, i, columns[column % 6]);
                if (c == delimiter) ++column;
                else column = 0;
                fieldStart = i + 1;
                if (c == L'\r' && i + 1 < to && text_[i + 1] == L'\n') { ++i; fieldStart = i + 1; }
            }
            ++i;
        }
    }

    void Log(size_t from, size_t to) {
        static const wchar_t* errors[] = {L"FATAL", L"CRITICAL", L"ERROR", L"ERR", L"SEVERE", L"EXCEPTION", L"FAILED", L"FAIL", L"PANIC"};
        static const wchar_t* warnings[] = {L"WARNING", L"WARN"};
        static const wchar_t* infos[] = {L"INFO", L"DEBUG", L"TRACE", L"NOTICE", L"VERBOSE"};
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            size_t i = line;
            // Leading timestamp: digits, - / : . T space and brackets.
            size_t stamp = SkipSpaces(i, end);
            const size_t stampStart = stamp;
            if (stamp < end && text_[stamp] == L'[') ++stamp;
            size_t digits = 0;
            while (stamp < end && (iswdigit(text_[stamp]) || wcschr(L"-/:.,TZ+ ", text_[stamp]))) {
                if (iswdigit(text_[stamp])) ++digits;
                ++stamp;
            }
            if (stamp < end && text_[stamp] == L']') ++stamp;
            if (digits >= 6) {
                Add(stampStart, stamp, Tok::Meta);
                i = stamp;
            }
            while (i < end) {
                const wchar_t c = text_[i];
                if (c == L'"' || c == L'\'') {
                    const size_t close = StringEnd(i, end, c, false, true);
                    Add(i, close, Tok::String);
                    i = close; continue;
                }
                if (At(i, L"http://") || At(i, L"https://")) {
                    size_t j = i;
                    while (j < end && !iswspace(text_[j])) ++j;
                    Add(i, j, Tok::Link);
                    i = j; continue;
                }
                if (IsIdentStart(c) && (i == line || !IsIdentChar(text_[i - 1]))) {
                    size_t j = i;
                    while (j < end && IsIdentChar(text_[j])) ++j;
                    const std::wstring word = text_.substr(i, j - i);
                    const auto matches = [&](const auto& list) {
                        for (const wchar_t* level : list) if (_wcsicmp(word.c_str(), level) == 0 && iswupper(word[0])) return true;
                        return false;
                    };
                    if (matches(errors)) Add(i, j, Tok::Error);
                    else if (matches(warnings)) Add(i, j, Tok::Warning);
                    else if (matches(infos)) Add(i, j, Tok::Info);
                    i = j; continue;
                }
                ++i;
            }
            line = NextLine(end, to);
        }
    }

    void Diff(size_t from, size_t to) {
        for (size_t line = from; line < to;) {
            const size_t end = LineEnd(line, to);
            if (At(line, L"+++") || At(line, L"---") || At(line, L"diff ") || At(line, L"index ")) Add(line, end, Tok::Keyword);
            else if (At(line, L"@@")) Add(line, end, Tok::Meta);
            else if (line < end && text_[line] == L'+') Add(line, end, Tok::Added);
            else if (line < end && text_[line] == L'-') Add(line, end, Tok::Removed);
            line = NextLine(end, to);
        }
    }
};

