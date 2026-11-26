fs::path GetDataDirectory();

// One running instance per data folder: copies started with another
// LOCALAPPDATA (portable/test setups) don't hand their files to a window that
// keeps a different session.
constexpr wchar_t kDataKeyProperty[] = L"MCnotepad.DataKey";

UINT_PTR DataDirectoryKey() {
    std::wstring path = GetDataDirectory().wstring();
    for (wchar_t& c : path) c = towlower(c);
    uint32_t hash = 2166136261u;
    for (const wchar_t c : path) { hash ^= c; hash *= 16777619u; }
    return static_cast<UINT_PTR>(hash | 1u);
}

std::wstring SingleInstanceMutexName() {
    wchar_t suffix[16]{};
    swprintf_s(suffix, L".%08x", static_cast<unsigned>(DataDirectoryKey()));
    return std::wstring(kSingleInstanceMutex) + suffix;
}

HWND FindInstanceWindow() {
    const UINT_PTR key = DataDirectoryKey();
    for (HWND window = FindWindowExW(nullptr, nullptr, kWindowClass, nullptr); window;
         window = FindWindowExW(nullptr, window, kWindowClass, nullptr)) {
        if (reinterpret_cast<UINT_PTR>(GetPropW(window, kDataKeyProperty)) == key) return window;
    }
    return nullptr;
}

std::wstring CurrentExecutablePath();

// Where sessions and recovery snapshots live:
// - portable copy (a portable.ini next to the exe): a "data" folder beside it;
// - installed / release build: %LOCALAPPDATA%\MCnotepad;
// - development build: %LOCALAPPDATA%\MCnotepad-dev, so testing a build never
//   touches the tabs of the copy in daily use.
fs::path GetDataDirectory() {
    const fs::path exe = CurrentExecutablePath();
    std::error_code error;
    if (!exe.empty() && fs::exists(exe.parent_path() / L"portable.ini", error)) {
        return exe.parent_path() / L"data";
    }
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return fs::path(buffer) / (MCNOTEPAD_RELEASE ? L"MCnotepad" : L"MCnotepad-dev");
    }
    return fs::current_path() / L"data";
}

// Programming fonts shipped in resources\fonts\<Family>\*.ttf (all open
// source; licenses next to them). They are loaded for this process only, so
// nothing is installed into Windows. Returns the families that loaded, in
// the order shown in Settings.
std::vector<std::wstring> g_bundledFonts;

void LoadBundledFonts() {
    g_bundledFonts.clear();
    const fs::path exe = CurrentExecutablePath();
    if (exe.empty()) return;
    std::error_code error;
    const fs::path root = exe.parent_path() / L"resources" / L"fonts";
    for (const auto& folder : fs::directory_iterator(root, error)) {
        if (!folder.is_directory(error)) continue;
        int loaded = 0;
        for (const auto& file : fs::directory_iterator(folder.path(), error)) {
            std::wstring extension = file.path().extension().wstring();
            for (wchar_t& c : extension) c = towlower(c);
            if (extension != L".ttf" && extension != L".otf") continue;
            loaded += AddFontResourceExW(file.path().c_str(), FR_PRIVATE, nullptr) > 0 ? 1 : 0;
        }
        if (loaded > 0) g_bundledFonts.push_back(folder.path().filename().wstring());  // folder name = family name
    }
    std::sort(g_bundledFonts.begin(), g_bundledFonts.end());
}

bool IsBundledFont(const std::wstring& family) {
    return std::find(g_bundledFonts.begin(), g_bundledFonts.end(), family) != g_bundledFonts.end();
}

// "0.1.0 (28298d0)" for releases, "0.1.0 开发版" for development builds.
std::wstring VersionDescription() {
    std::wstring text = Utf8ToWide(MCNOTEPAD_VERSION_TEXT);
    if (MCNOTEPAD_RELEASE) text += L" (" + Utf8ToWide(MCNOTEPAD_BUILD_ID_TEXT) + L")";
    else text += L" 开发版";
    return text;
}

std::wstring EditionDescription() {
    std::error_code error;
    const fs::path exe = CurrentExecutablePath();
    if (!exe.empty() && fs::exists(exe.parent_path() / L"portable.ini", error)) return L"便携版";
    return MCNOTEPAD_RELEASE ? L"安装版" : L"开发版（仅用于测试，数据与正式版分开）";
}

constexpr wchar_t kStartupRegistryPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kStartupRegistryValue[] = L"MCnotepad";

std::wstring CurrentExecutablePath() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) return {};
        if (length < path.size() - 1) {
            path.resize(length);
            return path;
        }
        path.resize(path.size() * 2);
    }
}

bool IsStartupRegistrationEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kStartupRegistryPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    wchar_t value[2048]{};
    DWORD valueSize = sizeof(value);
    DWORD valueType = 0;
    const LONG result = RegQueryValueExW(key, kStartupRegistryValue, nullptr, &valueType,
        reinterpret_cast<LPBYTE>(value), &valueSize);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || (valueType != REG_SZ && valueType != REG_EXPAND_SZ) || value[0] == L'\0') return false;
    // The entry is shared by every copy (installed, portable, development):
    // it counts as on only when it starts this copy, so turning the option on
    // in another copy moves it there.
    std::wstring command = value;
    command.erase(std::remove(command.begin(), command.end(), L'"'), command.end());
    while (!command.empty() && iswspace(command.back())) command.pop_back();
    return CompareStringOrdinal(command.c_str(), -1, CurrentExecutablePath().c_str(), -1, TRUE) == CSTR_EQUAL;
}

bool SetStartupRegistration(bool enabled) {
    HKEY key = nullptr;
    DWORD disposition = 0;
    const LONG openResult = RegCreateKeyExW(HKEY_CURRENT_USER, kStartupRegistryPath, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, &disposition);
    if (openResult != ERROR_SUCCESS) return false;

    LONG result = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring executable = CurrentExecutablePath();
        const std::wstring command = L"\"" + executable + L"\"";
        result = RegSetValueExW(key, kStartupRegistryValue, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else if (IsStartupRegistrationEnabled()) {  // leave another copy's entry alone
        result = RegDeleteValueW(key, kStartupRegistryValue);
        if (result == ERROR_FILE_NOT_FOUND) result = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool ReadBinaryFile(const fs::path& path, std::vector<char>& bytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0) {
        return false;
    }
    input.seekg(0, std::ios::beg);

    bytes.resize(static_cast<size_t>(size));
    if (size > 0) {
        input.read(bytes.data(), size);
    }
    return input.good() || input.eof();
}

// `flush` forces the data to the physical disk before returning. That costs
// tens of milliseconds per file, so it is used for the user's own files only.
bool WriteBinaryFile(const fs::path& path, std::string_view bytes, bool flush = true) {
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    bool success = true;
    size_t written = 0;
    while (written < bytes.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(bytes.size() - written, 1u << 20));
        DWORD completed = 0;
        if (!WriteFile(handle, bytes.data() + written, chunk, &completed, nullptr) || completed == 0) {
            success = false;
            break;
        }
        written += completed;
    }
    if (success && flush && !FlushFileBuffers(handle)) {
        success = false;
    }
    CloseHandle(handle);
    return success;
}

// Overwrites an existing file's contents where it is (keeps hard links,
// attributes, ACLs and streams). Not atomic: only used when a replace isn't possible.
bool WriteFileInPlace(const fs::path& path, std::string_view bytes) {
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    bool success = true;
    size_t written = 0;
    while (success && written < bytes.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(bytes.size() - written, 1u << 20));
        DWORD completed = 0;
        success = WriteFile(handle, bytes.data() + written, chunk, &completed, nullptr) && completed > 0;
        written += completed;
    }
    success = success && SetEndOfFile(handle) && FlushFileBuffers(handle);
    CloseHandle(handle);
    return success;
}

DWORD HardLinkCount(const fs::path& path) {
    HANDLE handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return 1;
    BY_HANDLE_FILE_INFORMATION info{};
    const DWORD links = GetFileInformationByHandle(handle, &info) ? info.nNumberOfLinks : 1;
    CloseHandle(handle);
    return links;
}

// For the app's own data (recovery snapshots, session, histories): written
// to a unique temp file and moved over the old one. There is no metadata
// worth keeping on these, and the snapshot is rewritten every two seconds
// while typing, where ReplaceFileW costs roughly ten times a plain move.
bool WriteDataFile(const fs::path& path, std::string_view bytes) {
    wchar_t suffix[32]{};
    swprintf_s(suffix, L".mcn-%08lx%04lx.tmp", GetTickCount(), GetCurrentProcessId() & 0xFFFF);
    const fs::path temporaryPath = path.wstring() + suffix;
    std::error_code error;
    // No forced flush: these files are rewritten every few seconds while
    // typing, and the rename below already keeps a crash from leaving a
    // half-written file in place.
    if (!WriteBinaryFile(temporaryPath, bytes, false)) {
        fs::remove(temporaryPath, error);
        return false;
    }
    if (MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) return true;
    fs::remove(temporaryPath, error);
    return false;
}

// Writes a file so that a crash never leaves it half-written:
// - the new content goes to a uniquely named hidden temp file next to it,
//   then ReplaceFileW swaps it in, keeping the original's attributes, ACLs,
//   creation time and alternate streams;
// - a symbolic link is followed, so the link's target is updated;
// - hard-linked files, and folders where new files can't be created, are
//   written in place instead.
bool WriteAtomicFile(const fs::path& requestedPath, std::string_view bytes) {
    std::error_code error;
    fs::path path = requestedPath;
    if (fs::is_symlink(path, error)) {
        const fs::path target = fs::canonical(path, error);
        if (!error) path = target;
    }
    const bool exists = fs::exists(path, error);
    if (exists && HardLinkCount(path) > 1) return WriteFileInPlace(path, bytes);

    wchar_t suffix[32]{};
    swprintf_s(suffix, L".mcn-%08lx%04lx.tmp", GetTickCount(), GetCurrentProcessId() & 0xFFFF);
    const fs::path temporaryPath = path.wstring() + suffix;
    if (!WriteBinaryFile(temporaryPath, bytes)) {
        fs::remove(temporaryPath, error);
        return exists && WriteFileInPlace(path, bytes);
    }
    SetFileAttributesW(temporaryPath.c_str(), FILE_ATTRIBUTE_HIDDEN);
    bool replaced = false;
    if (exists) {
        replaced = ReplaceFileW(path.c_str(), temporaryPath.c_str(), nullptr,
            REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_IGNORE_ACL_ERRORS, nullptr, nullptr) != FALSE;
        if (!replaced) {
            // e.g. no permission to replace: fall back to rewriting the original.
            fs::remove(temporaryPath, error);
            return WriteFileInPlace(path, bytes);
        }
    } else {
        SetFileAttributesW(temporaryPath.c_str(), FILE_ATTRIBUTE_NORMAL);
        replaced = MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    }
    if (!replaced) fs::remove(temporaryPath, error);
    return replaced;
}

// Decodes file bytes and reports which encoding was found:
// BOM (UTF-8 / UTF-16 LE / UTF-16 BE) first, then strict UTF-8, otherwise the
// system ANSI code page.
// UTF-16 text without a BOM (PowerShell and registry exports, some logs):
// mostly-ASCII text has a zero in every other byte. Returns 2 (LE), 3 (BE) or 0.
int GuessBomlessUtf16(const std::vector<char>& bytes) {
    const size_t pairs = std::min<size_t>(bytes.size(), 65536) / 2;
    if (pairs < 2) return 0;
    size_t zeroEven = 0, zeroOdd = 0;
    for (size_t i = 0; i < pairs; ++i) {
        zeroEven += bytes[2 * i] == 0;
        zeroOdd += bytes[2 * i + 1] == 0;
    }
    if (zeroOdd * 10 >= pairs * 3 && zeroEven * 20 <= pairs) return 2;
    if (zeroEven * 10 >= pairs * 3 && zeroOdd * 20 <= pairs) return 3;
    return 0;
}

std::wstring DecodeUtf16Bytes(const std::vector<char>& bytes, size_t offset, bool bigEndian) {
    std::wstring result;
    result.reserve((bytes.size() - offset) / 2);
    for (size_t index = offset; index + 1 < bytes.size(); index += 2) {
        const unsigned char first = static_cast<unsigned char>(bytes[index]);
        const unsigned char second = static_cast<unsigned char>(bytes[index + 1]);
        result.push_back(static_cast<wchar_t>(bigEndian ? (first << 8) | second : (second << 8) | first));
    }
    return result;
}

std::wstring DecodeTextBytes(const std::vector<char>& bytes, int* detectedEncoding = nullptr) {
    int ignored = 0;
    int& encoding = detectedEncoding ? *detectedEncoding : ignored;
    const bool hasBom = bytes.size() >= 2 &&
        ((static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) ||
         (static_cast<unsigned char>(bytes[0]) == 0xFE && static_cast<unsigned char>(bytes[1]) == 0xFF));
    if (!hasBom) {
        if (const int utf16 = GuessBomlessUtf16(bytes)) {
            encoding = utf16;  // saved back with a BOM, which every reader accepts
            return DecodeUtf16Bytes(bytes, 0, utf16 == 3);
        }
    }
    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        encoding = 2;
        const int wideBytes = static_cast<int>(bytes.size() - 2);
        return std::wstring(reinterpret_cast<const wchar_t*>(bytes.data() + 2),
            reinterpret_cast<const wchar_t*>(bytes.data() + 2) + wideBytes / 2);
    }

    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFE &&
        static_cast<unsigned char>(bytes[1]) == 0xFF) {
        encoding = 3;
        std::wstring result;
        result.reserve((bytes.size() - 2) / 2);
        for (size_t index = 2; index + 1 < bytes.size(); index += 2) {
            const wchar_t character = static_cast<wchar_t>(
                (static_cast<unsigned char>(bytes[index]) << 8) |
                static_cast<unsigned char>(bytes[index + 1]));
            result.push_back(character);
        }
        return result;
    }

    size_t offset = 0;
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        offset = 3;
    }

    const std::string_view text(bytes.data() + offset, bytes.size() - offset);
    if (text.empty()) {
        encoding = offset == 3 ? 1 : g_app.encodingIndex;
        return {};
    }
    const int strictSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (strictSize > 0) {
        encoding = offset == 3 ? 1 : 0;
        std::wstring result(static_cast<size_t>(strictSize), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), result.data(), strictSize);
        return result;
    }
    encoding = offset == 3 ? 1 : kEncodingAnsi;
    const int fallbackSize = MultiByteToWideChar(offset == 3 ? CP_UTF8 : CP_ACP, 0,
        text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (fallbackSize <= 0) return {};
    std::wstring result(static_cast<size_t>(fallbackSize), L'\0');
    MultiByteToWideChar(offset == 3 ? CP_UTF8 : CP_ACP, 0, text.data(),
        static_cast<int>(text.size()), result.data(), fallbackSize);
    return result;
}

// The first line break decides the file's line-ending style.
int DetectLineEnding(std::wstring_view text, int fallback) {
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') return (i + 1 < text.size() && text[i + 1] == L'\n') ? 0 : 2;
        if (text[i] == L'\n') return 1;
    }
    return fallback;
}

bool HasNulCharacter(const std::wstring& text) {
    return text.find(L'\0') != std::wstring::npos;
}

constexpr wchar_t kNulFileMessage[] =
    L"这个文件包含 NUL 字符（多半是二进制文件或编码无法识别）。\n\n"
    L"在编辑器中打开会在第一个 NUL 处截断，保存后文件会损坏，所以没有打开。";

bool ReadTextFile(const fs::path& path, std::wstring& text,
    int* encoding = nullptr, int* lineEnding = nullptr) {
    std::vector<char> bytes;
    if (!ReadBinaryFile(path, bytes)) {
        return false;
    }
    int detected = 0;
    text = DecodeTextBytes(bytes, &detected);
    if (encoding) *encoding = detected;
    if (lineEnding) *lineEnding = DetectLineEnding(text, g_app.lineEndingIndex);
    return true;
}

// True when every character survives a round trip through the ANSI code page.
bool IsAnsiRepresentable(std::wstring_view text) {
    if (text.empty()) return true;
    BOOL usedDefault = FALSE;
    const int size = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, &usedDefault);
    return size > 0 && !usedDefault;
}

// Language for a file extension or a code-fence / language name ("js", "py").
int LanguageFromName(const std::wstring& rawName) {
    static const std::unordered_map<std::wstring, int> names = [] {
        std::unordered_map<std::wstring, int> m;
        const auto add = [&](int language, const wchar_t* list) {
            std::wistringstream stream(list);
            for (std::wstring name; stream >> name;) m[name] = language;
        };
        add(kLanguageHtml, L"html htm xhtml vue svelte astro");
        add(kLanguageXml, L"xml svg xaml csproj vcxproj vbproj props targets resx xsd xsl xslt plist nuspec config manifest");
        add(kLanguageCss, L"css scss sass less");
        add(kLanguageJavaScript, L"js mjs cjs jsx javascript node");
        add(kLanguageTypeScript, L"ts tsx mts cts typescript");
        add(kLanguageCpp, L"c cc cpp cxx c++ h hpp hh hxx inl ino ipp");
        add(kLanguagePython, L"py pyw pyi python python3");
        add(kLanguageJson, L"json jsonc json5 webmanifest geojson ipynb");
        add(kLanguageMarkdown, L"md markdown mdx mkd");
        add(kLanguageSql, L"sql mysql pgsql psql");
        add(kLanguagePowerShell, L"ps1 psm1 psd1 powershell pwsh ps");
        add(kLanguageYaml, L"yaml yml");
        add(kLanguageConfig, L"ini cfg conf toml properties env dotenv editorconfig gitignore gitattributes dockerignore npmrc reg inf");
        add(kLanguageShell, L"sh bash zsh ksh fish shell shellscript console makefile mk");
        add(kLanguageBatch, L"bat cmd batch");
        add(kLanguageJava, L"java kt kts kotlin gradle groovy scala");
        add(kLanguageCSharp, L"cs csx csharp c#");
        add(kLanguageGo, L"go golang");
        add(kLanguageRust, L"rs rust");
        add(kLanguagePhp, L"php phtml php3 php4 php5");
        add(kLanguageDockerfile, L"dockerfile containerfile docker");
        add(kLanguageCsv, L"csv tsv");
        add(kLanguageLog, L"log out");
        add(kLanguageDiff, L"diff patch");
        return m;
    }();
    const auto it = names.find(LowerInvariantSyntax(rawName));
    return it == names.end() ? kLanguagePlain : it->second;
}

int LanguageFromPath(const std::wstring& path) {
    if (path.empty()) return kLanguagePlain;
    std::wstring name = LowerInvariantSyntax(fs::path(path).filename().wstring());
    // config.yml.example, .env.production.sample, nginx.conf.template, settings.json.bak ...
    for (bool stripped = true; stripped;) {
        stripped = false;
        for (const wchar_t* suffix : {L".example", L".sample", L".template", L".dist", L".tpl", L".bak", L".orig", L".default", L".old", L".tmpl"}) {
            const std::wstring tail = suffix;
            if (name.size() > tail.size() && name.ends_with(tail)) {
                name.resize(name.size() - tail.size());
                stripped = true;
            }
        }
    }
    if (name == L"dockerfile" || name == L"containerfile" || name.starts_with(L"dockerfile.") || name.ends_with(L".dockerfile")) return kLanguageDockerfile;
    if (name == L"makefile" || name == L"gnumakefile" || name == L".bashrc" || name == L".zshrc" || name == L".profile" || name == L".bash_profile") return kLanguageShell;
    if (name == L".env" || name.starts_with(L".env.") || name.ends_with(L".env")) return kLanguageConfig;
    if (name == L".prettierrc" || name == L".eslintrc" || name == L".babelrc" || name == L"composer.lock" || name == L".jshintrc") return kLanguageJson;
    for (const wchar_t* config : {L".gitignore", L".gitattributes", L".dockerignore", L".npmrc", L".editorconfig", L".prettierignore", L".eslintignore", L".yarnrc", L".gitmodules"}) {
        if (name == config) return kLanguageConfig;
    }
    const size_t dot = name.rfind(L'.');
    if (dot == std::wstring::npos || dot + 1 >= name.size()) return kLanguagePlain;
    return LanguageFromName(name.substr(dot + 1));
}

// For files whose name says nothing: shebang lines, XML/HTML prologs, JSON.
int SniffLanguage(const std::wstring& text) {
    size_t start = text.find_first_not_of(L" \t\r\n\xFEFF");
    if (start == std::wstring::npos) return kLanguagePlain;
    const std::wstring head = LowerInvariantSyntax(text.substr(start, 400));
    if (head.starts_with(L"#!")) {
        const std::wstring line = head.substr(0, head.find_first_of(L"\r\n"));
        if (line.find(L"python") != std::wstring::npos) return kLanguagePython;
        if (line.find(L"node") != std::wstring::npos) return kLanguageJavaScript;
        if (line.find(L"pwsh") != std::wstring::npos || line.find(L"powershell") != std::wstring::npos) return kLanguagePowerShell;
        if (line.find(L"php") != std::wstring::npos) return kLanguagePhp;
        return kLanguageShell;
    }
    if (head.starts_with(L"<?xml")) return kLanguageXml;
    if (head.starts_with(L"<!doctype html") || head.starts_with(L"<html")) return kLanguageHtml;
    if (head.starts_with(L"<?php")) return kLanguagePhp;
    if (head.starts_with(L"diff --git") || (head.starts_with(L"--- ") && head.find(L"\n+++ ") != std::wstring::npos)) return kLanguageDiff;
    if ((head[0] == L'{' || head[0] == L'[') && head.find(L"\":") != std::wstring::npos) return kLanguageJson;
    return kLanguagePlain;
}

// Language actually used for highlighting/formatting: the user's choice, or the
// one implied by the file extension when set to "auto".
int EffectiveLanguage(const TabDocument& document) {
    if (document.language > kLanguageAuto && document.language < kLanguageCount) return document.language;
    const int fromPath = LanguageFromPath(document.path);
    return fromPath != kLanguagePlain ? fromPath : document.sniffedLanguage;
}

std::wstring NormalizeLineEndings(std::wstring_view text, int mode) {
    std::wstring normalized;
    normalized.reserve(text.size() + 16);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            if (mode == 0) normalized += L"\r\n";
            else if (mode == 1) normalized += L'\n';
            else normalized += L'\r';
        } else if (text[i] == L'\n') {
            if (mode == 0) normalized += L"\r\n";
            else if (mode == 1) normalized += L'\n';
            else normalized += L'\r';
        } else {
            normalized += text[i];
        }
    }
    return normalized;
}

// Encodes text exactly as given (line breaks untouched) in one of kEncodingNames.
bool EncodeTextBytes(std::wstring_view text, int encoding, std::string& bytes) {
    bytes.clear();
    if (encoding == 2 || encoding == 3) {
        bytes.reserve(text.size() * 2 + 2);
        bytes.append(encoding == 2 ? "\xFF\xFE" : "\xFE\xFF", 2);
        for (wchar_t value : text) {
            const uint16_t unit = static_cast<uint16_t>(value);
            const char low = static_cast<char>(unit & 0xFF);
            const char high = static_cast<char>((unit >> 8) & 0xFF);
            if (encoding == 2) { bytes.push_back(low); bytes.push_back(high); }
            else { bytes.push_back(high); bytes.push_back(low); }
        }
        return true;
    }
    if (encoding == kEncodingAnsi) {
        const int size = WideCharToMultiByte(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, "?", nullptr);
        if (size <= 0) return text.empty();
        bytes.resize(static_cast<size_t>(size));
        WideCharToMultiByte(CP_ACP, 0, text.data(), static_cast<int>(text.size()), bytes.data(), size, "?", nullptr);
        return true;
    }
    const std::string utf8 = WideToUtf8(text);
    // Never report success for a conversion that lost the whole text.
    if (utf8.empty() && !text.empty()) return false;
    if (encoding == 1) bytes.append("\xEF\xBB\xBF", 3);
    bytes += utf8;
    return true;
}

bool WriteTextFile(const fs::path& path, std::wstring_view text, int encodingIndex = -1, int lineEndingIndex = -1,
    bool dataFile = false) {
    if (encodingIndex < 0) encodingIndex = g_app.encodingIndex;
    if (lineEndingIndex < 0) lineEndingIndex = g_app.lineEndingIndex;
    std::string bytes;
    if (!EncodeTextBytes(NormalizeLineEndings(text, lineEndingIndex), encodingIndex, bytes)) return false;
    return dataFile ? WriteDataFile(path, bytes) : WriteAtomicFile(path, bytes);
}

std::wstring FileTitle(const fs::path& path) {
    const std::wstring name = path.filename().wstring();
    return name.empty() ? path.wstring() : name;
}

uint64_t FileTimestamp(const fs::path& path) {
    std::error_code error;
    const auto time = fs::last_write_time(path, error);
    if (error) {
        return 0;
    }
    return static_cast<uint64_t>(time.time_since_epoch().count());
}

void UpdateWindowTitle() {
    if (!g_app.window) return;
    // Development builds say so, so they can't be mistaken for the installed app.
    std::wstring title = MCNOTEPAD_RELEASE ? L"MCnotepad" : L"MCnotepad [开发版]";
    if (g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size())) {
        title += L" - " + g_app.documents[static_cast<size_t>(g_app.activeIndex)].title;
    }
    SetWindowTextW(g_app.window, title.c_str());
}

void UpdateTabTitle(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.documents.size())) {
        return;
    }


    if (g_app.tabs) InvalidateRect(g_app.tabs, nullptr, FALSE);
    UpdateWindowTitle();
}

void CaptureEditorState(TabDocument& document) {
    DWORD anchor = 0;
    DWORD caret = 0;
    SendMessageW(document.editor, EM_GETSEL,
        reinterpret_cast<WPARAM>(&anchor), reinterpret_cast<LPARAM>(&caret));
    document.anchor = static_cast<int64_t>(anchor);
    document.caret = static_cast<int64_t>(caret);
    document.firstVisibleLine = static_cast<int>(SendMessageW(
        document.editor, EM_GETFIRSTVISIBLELINE, 0, 0));
}

void RestoreEditorState(const TabDocument& document) {
    const int start = static_cast<int>(std::clamp(document.anchor, int64_t(0), int64_t(0x7FFFFFFF)));
    const int end = static_cast<int>(std::clamp(document.caret, int64_t(0), int64_t(0x7FFFFFFF)));
    SendMessageW(document.editor, EM_SETSEL, static_cast<WPARAM>(start), static_cast<LPARAM>(end));
    // EM_LINESCROLL is relative, and EM_SETSEL may already have scrolled the
    // caret into view, so scroll by the remaining difference only.
    const int current = static_cast<int>(SendMessageW(document.editor, EM_GETFIRSTVISIBLELINE, 0, 0));
    if (current != document.firstVisibleLine) {
        SendMessageW(document.editor, EM_LINESCROLL, 0, document.firstVisibleLine - current);
    }
    RefreshGutter();
}

void UpdateStatusBar() {
    if (g_app.statusBar) InvalidateRect(g_app.statusBar, nullptr, FALSE);
    // Undo/redo availability follows edits. This runs on every caret move,
    // so the toolbar is only repainted when that state really changed.
    static HWND lastEditor = nullptr;
    static int lastState = -1;
    HWND editor = g_app.activeIndex >= 0 && g_app.activeIndex < static_cast<int>(g_app.documents.size())
        ? g_app.documents[static_cast<size_t>(g_app.activeIndex)].editor : nullptr;
    const int state = editor ? (SendMessageW(editor, EM_CANUNDO, 0, 0) ? 1 : 0) | (SendMessageW(editor, EM_CANREDO, 0, 0) ? 2 : 0) : 0;
    if (editor != lastEditor || state != lastState) {
        lastEditor = editor;
        lastState = state;
        InvalidateToolbar();
    }
}

