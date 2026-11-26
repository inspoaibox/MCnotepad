// ===========================================================================
// Developer toolbox: a sidebar that works on the editor's selection.
// ===========================================================================

enum ToolboxTool : int {
    kToolJson = 0, kToolBase64, kToolUrl, kToolJwt, kToolTimestamp, kToolHash, kToolRegex, kToolUnicode, kToolHtml, kToolUuid, kToolCount
};

const wchar_t* const kToolboxNames[kToolCount] = {
    L"JSON", L"Base64", L"URL", L"JWT 解码", L"时间戳", L"哈希 (MD5/SHA)", L"正则测试", L"Unicode 转义", L"HTML 实体", L"UUID 生成"};

std::vector<std::wstring> ToolboxActions(int tool) {
    switch (tool) {
    case kToolJson: return {L"格式化", L"压缩为一行", L"校验"};
    case kToolBase64: return {L"编码", L"解码"};
    case kToolUrl: return {L"编码", L"解码", L"解析 URL"};
    case kToolUnicode: return {L"转义 (\\uXXXX)", L"还原"};
    case kToolHtml: return {L"编码", L"解码"};
    default: return {};
    }
}

enum ToolboxControl : int {
    kTbTool = 4401, kTbAction, kTbPattern, kTbReplace, kTbIgnoreCase, kTbFollow, kTbTake, kTbInput, kTbOutput, kTbCopy, kTbApply,
    kTbStatus, kTbClose, kTbCustomize, kTbPatternLabel, kTbReplaceLabel, kTbInputLabel, kTbOutputLabel, kTbTitle,
};

struct ToolboxState {
    HWND panel = nullptr;
    HWND tool = nullptr, action = nullptr, pattern = nullptr, replace = nullptr, ignoreCase = nullptr, follow = nullptr;
    HWND input = nullptr, output = nullptr, status = nullptr;
    std::vector<int> shown;      // tools in the dropdown
    bool dragging = false;
    int dragStartX = 0, dragStartWidth = 0;
    bool settingInput = false;
} g_toolbox;

constexpr UINT_PTR kTimerToolboxRun = 1;
void ToggleToolbox(bool show);
constexpr UINT_PTR kTimerToolboxSelection = 2;

std::wstring TrimSpace(std::wstring_view text) {
    size_t first = 0, last = text.size();
    while (first < last && (iswspace(text[first]) || text[first] == 0x3000)) ++first;
    while (last > first && (iswspace(text[last - 1]) || text[last - 1] == 0x3000)) --last;
    return std::wstring(text.substr(first, last - first));
}

std::wstring ToCrLf(std::wstring_view text) {
    std::wstring out;
    out.reserve(text.size() + text.size() / 16);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') { out += L"\r\n"; if (i + 1 < text.size() && text[i + 1] == L'\n') ++i; }
        else if (text[i] == L'\n') out += L"\r\n";
        else out += text[i];
    }
    return out;
}

// ---- The tools ---------------------------------------------------------------------------

std::wstring HashHex(const std::string& bytes, ALG_ID algorithm) {
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    std::wstring result;
    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return L"（不可用）";
    if (CryptCreateHash(provider, algorithm, 0, 0, &hash)) {
        bool ok = true;
        for (size_t offset = 0; offset < bytes.size() && ok; offset += 1u << 30) {
            const DWORD chunk = static_cast<DWORD>(std::min<size_t>(bytes.size() - offset, 1u << 30));
            ok = CryptHashData(hash, reinterpret_cast<const BYTE*>(bytes.data() + offset), chunk, 0) != FALSE;
        }
        DWORD size = 0, sizeLength = sizeof(size);
        if (ok && CryptGetHashParam(hash, HP_HASHSIZE, reinterpret_cast<BYTE*>(&size), &sizeLength, 0)) {
            std::vector<BYTE> value(size);
            if (CryptGetHashParam(hash, HP_HASHVAL, value.data(), &size, 0)) {
                static const wchar_t hex[] = L"0123456789abcdef";
                for (const BYTE b : value) { result += hex[b >> 4]; result += hex[b & 15]; }
            }
        }
        CryptDestroyHash(hash);
    }
    CryptReleaseContext(provider, 0);
    return result;
}

// Strict JSON (RFC 8259) check with a line/column message.
class JsonValidator {
public:
    explicit JsonValidator(std::wstring_view text) : text_(text) {}
    bool Run(std::wstring& error) {
        Space();
        if (position_ < text_.size() && text_[position_] == 0xFEFF) { ++position_; Space(); }
        if (!Value(0)) { error = Where() + message_; return false; }
        Space();
        if (position_ < text_.size()) { error = Where() + L"JSON 结束后还有多余的内容"; return false; }
        return true;
    }

private:
    std::wstring Where() const {
        size_t line = 1, column = 1;
        for (size_t i = 0; i < position_ && i < text_.size(); ++i) {
            if (text_[i] == L'\n') { ++line; column = 1; } else if (text_[i] != L'\r') ++column;
        }
        return L"第 " + std::to_wstring(line) + L" 行第 " + std::to_wstring(column) + L" 列：";
    }
    void Space() { while (position_ < text_.size() && (text_[position_] == L' ' || text_[position_] == L'\t' || text_[position_] == L'\n' || text_[position_] == L'\r')) ++position_; }
    bool Fail(const wchar_t* message) { message_ = message; return false; }
    bool Literal(const wchar_t* word) {
        const size_t n = wcslen(word);
        if (text_.substr(position_, n) != word) return Fail(L"无法识别的值");
        position_ += n;
        return true;
    }
    bool String() {
        ++position_;
        while (position_ < text_.size()) {
            const wchar_t c = text_[position_];
            if (c == L'"') { ++position_; return true; }
            if (c < 0x20) return Fail(L"字符串中不能直接包含换行或控制字符");
            if (c == L'\\') {
                if (++position_ >= text_.size()) break;
                const wchar_t e = text_[position_];
                if (e == L'u') {
                    for (int k = 1; k <= 4; ++k) if (position_ + k >= text_.size() || !iswxdigit(text_[position_ + k])) return Fail(L"\\u 后应为 4 位十六进制数");
                    position_ += 4;
                } else if (!wcschr(L"\"\\/bfnrt", e)) {
                    return Fail(L"无效的转义字符");
                }
            }
            ++position_;
        }
        return Fail(L"字符串没有结束");
    }
    bool Number() {
        const size_t start = position_;
        if (text_[position_] == L'-') ++position_;
        if (position_ >= text_.size() || !iswdigit(text_[position_])) return Fail(L"无效的数字");
        if (text_[position_] == L'0' && position_ + 1 < text_.size() && iswdigit(text_[position_ + 1])) return Fail(L"数字不能以 0 开头");
        while (position_ < text_.size() && iswdigit(text_[position_])) ++position_;
        if (position_ < text_.size() && text_[position_] == L'.') {
            ++position_;
            if (position_ >= text_.size() || !iswdigit(text_[position_])) return Fail(L"小数点后应有数字");
            while (position_ < text_.size() && iswdigit(text_[position_])) ++position_;
        }
        if (position_ < text_.size() && (text_[position_] == L'e' || text_[position_] == L'E')) {
            ++position_;
            if (position_ < text_.size() && (text_[position_] == L'+' || text_[position_] == L'-')) ++position_;
            if (position_ >= text_.size() || !iswdigit(text_[position_])) return Fail(L"指数部分应有数字");
            while (position_ < text_.size() && iswdigit(text_[position_])) ++position_;
        }
        return position_ > start;
    }
    bool Value(int depth) {
        if (depth > 512) return Fail(L"嵌套层数太深");
        Space();
        if (position_ >= text_.size()) return Fail(L"内容不完整");
        const wchar_t c = text_[position_];
        if (c == L'{' || c == L'[') {
            const bool object = c == L'{';
            ++position_;
            Space();
            if (position_ < text_.size() && text_[position_] == (object ? L'}' : L']')) { ++position_; return true; }
            while (true) {
                Space();
                if (object) {
                    if (position_ < text_.size() && text_[position_] == L'/') return Fail(L"标准 JSON 不支持注释");
                    if (position_ >= text_.size() || text_[position_] != L'"') return Fail(L"应为用双引号括起来的键名");
                    if (!String()) return false;
                    Space();
                    if (position_ >= text_.size() || text_[position_] != L':') return Fail(L"键名后缺少冒号");
                    ++position_;
                }
                if (!Value(depth + 1)) return false;
                Space();
                if (position_ < text_.size() && text_[position_] == L',') {
                    ++position_;
                    Space();
                    if (position_ < text_.size() && text_[position_] == (object ? L'}' : L']')) return Fail(L"末尾多了一个逗号");
                    continue;
                }
                if (position_ < text_.size() && text_[position_] == (object ? L'}' : L']')) { ++position_; return true; }
                return Fail(object ? L"缺少逗号或 }" : L"缺少逗号或 ]");
            }
        }
        if (c == L'"') return String();
        if (c == L'-' || iswdigit(c)) return Number();
        if (c == L't') return Literal(L"true");
        if (c == L'f') return Literal(L"false");
        if (c == L'n') return Literal(L"null");
        if (c == L'/') return Fail(L"标准 JSON 不支持注释");
        return Fail(L"无法识别的值（字符串必须用双引号）");
    }

    std::wstring_view text_;
    size_t position_ = 0;
    std::wstring message_;
};

std::wstring CompactJson(std::wstring_view input, std::wstring& error) {
    std::wstring formatted;
    if (!FormatJson(input, formatted, error)) return {};
    std::wstring out;
    bool inString = false;
    for (size_t i = 0; i < formatted.size(); ++i) {
        const wchar_t c = formatted[i];
        if (inString) {
            out += c;
            if (c == L'\\' && i + 1 < formatted.size()) out += formatted[++i];
            else if (c == L'"') inString = false;
            continue;
        }
        if (c == L'"') { inString = true; out += c; continue; }
        if (!iswspace(c)) out += c;
    }
    return out;
}

std::wstring FormatUnixTime(int64_t seconds, int milliseconds) {
    const int64_t ticks = (seconds + 11644473600LL) * 10000000LL + milliseconds * 10000LL;
    if (ticks < 0) return L"（超出范围）";
    FILETIME utc{static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
    SYSTEMTIME u{}, l{};
    if (!FileTimeToSystemTime(&utc, &u)) return L"（超出范围）";
    SystemTimeToTzSpecificLocalTime(nullptr, &u, &l);
    // Local offset from the two clocks.
    FILETIME localFile{};
    SystemTimeToFileTime(&l, &localFile);
    const int64_t localTicks = (static_cast<int64_t>(localFile.dwHighDateTime) << 32) | localFile.dwLowDateTime;
    const int offsetMinutes = static_cast<int>((localTicks - ticks) / 600000000LL);
    wchar_t text[256]{};
    swprintf_s(text, L"本地时间：%04u-%02u-%02u %02u:%02u:%02u.%03u\r\nUTC：%04u-%02u-%02u %02u:%02u:%02u.%03u\r\nISO 8601：%04u-%02u-%02uT%02u:%02u:%02u%c%02d:%02d",
        l.wYear, l.wMonth, l.wDay, l.wHour, l.wMinute, l.wSecond, l.wMilliseconds,
        u.wYear, u.wMonth, u.wDay, u.wHour, u.wMinute, u.wSecond, u.wMilliseconds,
        l.wYear, l.wMonth, l.wDay, l.wHour, l.wMinute, l.wSecond, offsetMinutes < 0 ? L'-' : L'+', std::abs(offsetMinutes) / 60, std::abs(offsetMinutes) % 60);
    // Relative to now.
    FILETIME nowFile{};
    GetSystemTimeAsFileTime(&nowFile);
    const int64_t now = (((static_cast<int64_t>(nowFile.dwHighDateTime) << 32) | nowFile.dwLowDateTime) / 10000000LL) - 11644473600LL;
    const int64_t delta = now - seconds;
    const int64_t magnitude = std::abs(delta);
    std::wstring relative = magnitude < 60 ? std::to_wstring(magnitude) + L" 秒" : magnitude < 3600 ? std::to_wstring(magnitude / 60) + L" 分钟"
        : magnitude < 86400 ? std::to_wstring(magnitude / 3600) + L" 小时" : std::to_wstring(magnitude / 86400) + L" 天";
    return std::wstring(text) + L"\r\n相对现在：" + relative + (delta >= 0 ? L"前" : L"后");
}

int64_t UnixNow() {
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    return (((static_cast<int64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime) / 10000000LL) - 11644473600LL;
}

// Number → dates; date text → numbers; empty → now.
bool RunTimestamp(const std::wstring& rawInput, std::wstring& output, std::wstring& status) {
    const std::wstring input = TrimSpace(rawInput);
    if (input.empty()) {
        const int64_t now = UnixNow();
        output = L"当前时间\r\n秒：" + std::to_wstring(now) + L"\r\n毫秒：" + std::to_wstring(now * 1000) + L"\r\n" + FormatUnixTime(now, 0);
        status = L"输入时间戳或日期即可转换";
        return true;
    }
    if (std::all_of(input.begin(), input.end(), [](wchar_t c) { return iswdigit(c) || c == L'-'; }) && input.find(L'-', 1) == std::wstring::npos) {
        int64_t value = _wtoi64(input.c_str());
        std::wstring unit = L"秒";
        int64_t seconds = value, millis = 0;
        const int64_t magnitude = std::abs(value);
        if (magnitude >= 100000000000000000LL) { unit = L"纳秒"; seconds = value / 1000000000; millis = (value / 1000000) % 1000; }
        else if (magnitude >= 100000000000000LL) { unit = L"微秒"; seconds = value / 1000000; millis = (value / 1000) % 1000; }
        else if (magnitude >= 100000000000LL) { unit = L"毫秒"; seconds = value / 1000; millis = value % 1000; }
        if (millis < 0) { millis += 1000; --seconds; }
        output = L"按" + unit + L"解析\r\n" + FormatUnixTime(seconds, static_cast<int>(millis));
        status.clear();
        return true;
    }
    // Date text: YYYY-MM-DD[ T]HH:MM[:SS[.mmm]][Z|±HH:MM]
    static const std::wregex datePattern(
        LR"(^(\d{4})[-/.](\d{1,2})[-/.](\d{1,2})(?:[ T](\d{1,2}):(\d{2})(?::(\d{2})(?:[.,](\d{1,9}))?)?)?\s*(Z|z|[+-]\d{2}:?\d{2})?$)");
    std::wsmatch parts;
    if (!std::regex_match(input, parts, datePattern)) {
        status = L"无法识别：请输入 Unix 时间戳或 2026-11-23 10:30:00 这样的日期";
        return false;
    }
    const auto number = [&](int group) { return parts[group].matched ? static_cast<unsigned>(std::wcstoul(parts[group].str().c_str(), nullptr, 10)) : 0u; };
    const unsigned year = number(1), month = number(2), day = number(3), hour = number(4), minute = number(5), second = number(6);
    const unsigned millisecond = parts[7].matched ? static_cast<unsigned>(std::wcstoul((parts[7].str() + L"00").substr(0, 3).c_str(), nullptr, 10)) : 0u;
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) {
        status = L"日期或时间超出范围";
        return false;
    }
    SYSTEMTIME given{static_cast<WORD>(year), static_cast<WORD>(month), 0, static_cast<WORD>(day), static_cast<WORD>(hour),
        static_cast<WORD>(minute), static_cast<WORD>(second), static_cast<WORD>(millisecond)};
    SYSTEMTIME utc{};
    const bool explicitZone = parts[8].matched;
    int offsetMinutes = 0;
    if (explicitZone && parts[8].str() != L"Z" && parts[8].str() != L"z") {
        std::wstring zone = parts[8].str();
        zone.erase(std::remove(zone.begin(), zone.end(), L':'), zone.end());
        offsetMinutes = static_cast<int>(std::wcstoul(zone.substr(1, 2).c_str(), nullptr, 10) * 60 + std::wcstoul(zone.substr(3, 2).c_str(), nullptr, 10));
        if (zone[0] == L'-') offsetMinutes = -offsetMinutes;
    }
    FILETIME file{};
    if (explicitZone) {
        if (!SystemTimeToFileTime(&given, &file)) { status = L"日期无效"; return false; }
    } else {
        if (!TzSpecificLocalTimeToSystemTime(nullptr, &given, &utc) || !SystemTimeToFileTime(&utc, &file)) { status = L"日期无效"; return false; }
    }
    const int64_t ticks = (static_cast<int64_t>(file.dwHighDateTime) << 32) | file.dwLowDateTime;
    const int64_t millis = ticks / 10000LL - 11644473600000LL - static_cast<int64_t>(offsetMinutes) * 60000LL;
    const int64_t seconds = millis >= 0 ? millis / 1000 : (millis - 999) / 1000;
    output = std::wstring(explicitZone ? L"按输入的时区解析" : L"按本地时间解析") + L"\r\n秒：" + std::to_wstring(seconds) + L"\r\n毫秒：" +
        std::to_wstring(millis) + L"\r\n" + FormatUnixTime(seconds, static_cast<int>(((millis % 1000) + 1000) % 1000));
    status.clear();
    return true;
}

bool Base64UrlDecodeBytes(std::wstring_view text, std::string& bytes) {
    std::wstring clean;
    for (wchar_t c : text) {
        if (c == L'-') c = L'+';
        else if (c == L'_') c = L'/';
        if (!iswspace(c)) clean += c;
    }
    while (clean.size() % 4) clean += L'=';
    DWORD size = 0;
    if (clean.empty() || !CryptStringToBinaryW(clean.c_str(), static_cast<DWORD>(clean.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr)) return false;
    bytes.assign(size, '\0');
    return CryptStringToBinaryW(clean.c_str(), static_cast<DWORD>(clean.size()), CRYPT_STRING_BASE64, reinterpret_cast<BYTE*>(bytes.data()), &size, nullptr, nullptr) != FALSE;
}

bool RunJwt(const std::wstring& rawInput, std::wstring& output, std::wstring& status) {
    std::wstring token = TrimSpace(rawInput);
    if (token.compare(0, 7, L"Bearer ") == 0) token = TrimSpace(token.substr(7));
    const size_t first = token.find(L'.');
    const size_t second = first == std::wstring::npos ? std::wstring::npos : token.find(L'.', first + 1);
    if (second == std::wstring::npos) { status = L"不是 JWT：应为 header.payload.signature 三段"; return false; }
    std::string headerBytes, payloadBytes;
    if (!Base64UrlDecodeBytes(token.substr(0, first), headerBytes) || !Base64UrlDecodeBytes(token.substr(first + 1, second - first - 1), payloadBytes)) {
        status = L"JWT 的 Base64URL 部分无法解码";
        return false;
    }
    std::wstring header, payload, error;
    const std::wstring headerText = Utf8ToWide(headerBytes), payloadText = Utf8ToWide(payloadBytes);
    if (!FormatJson(headerText, header, error)) header = headerText;
    if (!FormatJson(payloadText, payload, error)) payload = payloadText;
    output = L"// Header\r\n" + ToCrLf(header) + L"\r\n\r\n// Payload\r\n" + ToCrLf(payload) + L"\r\n";
    // Well-known time claims.
    std::wstring times;
    for (const wchar_t* claim : {L"exp", L"iat", L"nbf"}) {
        const std::wregex pattern(std::wstring(L"\"") + claim + L"\"\\s*:\\s*(\\d+)");
        std::wsmatch match;
        if (std::regex_search(payloadText, match, pattern)) {
            const int64_t value = _wtoi64(match[1].str().c_str());
            std::wstring when = FormatUnixTime(value, 0);
            when = when.substr(0, when.find(L"\r\n"));
            times += std::wstring(L"\r\n") + claim + L"：" + when.substr(when.find(L'：') + 1);
            if (wcscmp(claim, L"exp") == 0) times += value < UnixNow() ? L"（已过期）" : L"（未过期）";
        }
    }
    if (!times.empty()) output += L"\r\n// 时间" + times + L"\r\n";
    output += L"\r\n// 签名（未验证）\r\n" + token.substr(second + 1);
    status = L"仅解码，未校验签名";
    return true;
}

bool RunUrlParse(const std::wstring& rawInput, std::wstring& output, std::wstring& status) {
    const std::wstring url = TrimSpace(rawInput);
    const std::wregex pattern(LR"(^([a-zA-Z][a-zA-Z0-9+.-]*):\/\/(?:([^@\/?#]*)@)?([^:\/?#]*)(?::(\d+))?([^?#]*)(?:\?([^#]*))?(?:#(.*))?$)");
    std::wsmatch match;
    if (!std::regex_match(url, match, pattern)) { status = L"不是完整的 URL（需要 scheme://host）"; return false; }
    const wchar_t* names[] = {L"", L"协议", L"用户信息", L"主机", L"端口", L"路径", L"查询", L"片段"};
    output.clear();
    for (int g = 1; g <= 7; ++g) {
        if (match[g].matched && match[g].length() > 0) output += std::wstring(names[g]) + L"：" + match[g].str() + L"\r\n";
    }
    if (match[6].matched && match[6].length() > 0) {
        output += L"\r\n查询参数：\r\n";
        const std::wstring query = match[6].str();
        size_t start = 0;
        while (start <= query.size()) {
            size_t end = query.find(L'&', start);
            if (end == std::wstring::npos) end = query.size();
            const std::wstring pair = query.substr(start, end - start);
            if (!pair.empty()) {
                const size_t equals = pair.find(L'=');
                std::wstring key = pair.substr(0, equals), value = equals == std::wstring::npos ? L"" : pair.substr(equals + 1);
                for (std::wstring* part : {&key, &value}) std::replace(part->begin(), part->end(), L'+', L' ');
                int decoded = 0;
                output += L"  " + UrlDecode(key, decoded) + L" = " + UrlDecode(value, decoded) + L"\r\n";
            }
            start = end + 1;
        }
    }
    status.clear();
    return true;
}

bool RunRegexTest(const std::wstring& input, const std::wstring& pattern, const std::wstring& replacement, bool ignoreCase,
    std::wstring& output, std::wstring& status) {
    if (pattern.empty()) { output.clear(); status = L"在上方输入正则表达式，下方输入测试文本"; return true; }
    try {
        const std::wregex expression(pattern, ignoreCase ? std::regex::ECMAScript | std::regex::icase : std::regex::ECMAScript);
        const std::wstring text = input.size() > (1u << 20) ? input.substr(0, 1u << 20) : input;
        size_t count = 0;
        std::wstring list;
        for (auto it = std::wsregex_iterator(text.begin(), text.end(), expression); it != std::wsregex_iterator(); ++it) {
            const std::wsmatch& match = *it;
            ++count;
            if (count <= 500) {
                const size_t position = static_cast<size_t>(match.position(0));
                const size_t line = 1 + static_cast<size_t>(std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(position), L'\n'));
                const size_t lineStart = text.rfind(L'\n', position == 0 ? 0 : position - 1);
                const size_t column = position - (lineStart == std::wstring::npos || position == 0 ? 0 : lineStart + 1) + 1;
                list += L"#" + std::to_wstring(count) + L"  行 " + std::to_wstring(line) + L" 列 " + std::to_wstring(column) + L"：" + match.str(0) + L"\r\n";
                for (size_t g = 1; g < match.size(); ++g) {
                    list += L"      $" + std::to_wstring(g) + L" = " + (match[g].matched ? match.str(g) : L"（未匹配）") + L"\r\n";
                }
            }
            if (match.length(0) == 0 && count > 100000) break;
        }
        output = L"共 " + std::to_wstring(count) + L" 处匹配" + (count > 500 ? L"（只列出前 500 处）" : L"") + L"\r\n\r\n" + list;
        if (!replacement.empty()) output += L"\r\n// 替换结果\r\n" + ToCrLf(std::regex_replace(text, expression, replacement));
        status = count ? L"" : L"没有匹配";
        return true;
    } catch (const std::regex_error&) {
        status = L"正则表达式有语法错误";
        output.clear();
        return false;
    }
}

std::wstring UnicodeEscape(std::wstring_view text) {
    std::wstring out;
    for (const wchar_t c : text) {
        if (c < 0x80) { out += c; continue; }
        wchar_t buffer[8]{};
        swprintf_s(buffer, L"\\u%04x", static_cast<unsigned>(c));
        out += buffer;
    }
    return out;
}

std::wstring UnicodeUnescape(std::wstring_view text, int& count) {
    std::wstring out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\\' && i + 5 < text.size() + 0 && (text[i + 1] == L'u' || text[i + 1] == L'U')) {
            if (text[i + 2] == L'{') {
                const size_t close = text.find(L'}', i + 3);
                if (close != std::wstring_view::npos) {
                    const unsigned long value = std::wcstoul(std::wstring(text.substr(i + 3, close - i - 3)).c_str(), nullptr, 16);
                    if (value >= 0x10000) { out += static_cast<wchar_t>(0xD800 + ((value - 0x10000) >> 10)); out += static_cast<wchar_t>(0xDC00 + ((value - 0x10000) & 0x3FF)); }
                    else out += static_cast<wchar_t>(value);
                    i = close;
                    ++count;
                    continue;
                }
            }
            bool hex = true;
            for (size_t k = 2; k < 6; ++k) hex = hex && iswxdigit(text[i + k]);
            if (hex) {
                out += static_cast<wchar_t>(std::wcstoul(std::wstring(text.substr(i + 2, 4)).c_str(), nullptr, 16));
                i += 5;
                ++count;
                continue;
            }
        }
        out += text[i];
    }
    return out;
}

std::wstring HtmlEncode(std::wstring_view text) {
    std::wstring out;
    for (const wchar_t c : text) {
        switch (c) {
        case L'&': out += L"&amp;"; break;
        case L'<': out += L"&lt;"; break;
        case L'>': out += L"&gt;"; break;
        case L'"': out += L"&quot;"; break;
        case L'\'': out += L"&#39;"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::wstring HtmlDecode(std::wstring_view text, int& count) {
    static const std::pair<const wchar_t*, wchar_t> named[] = {{L"amp", L'&'}, {L"lt", L'<'}, {L"gt", L'>'}, {L"quot", L'"'}, {L"apos", L'\''},
        {L"nbsp", static_cast<wchar_t>(0xA0)}, {L"copy", static_cast<wchar_t>(0xA9)}, {L"reg", static_cast<wchar_t>(0xAE)}, {L"hellip", static_cast<wchar_t>(0x2026)}, {L"mdash", static_cast<wchar_t>(0x2014)}, {L"ndash", static_cast<wchar_t>(0x2013)}, {L"yen", static_cast<wchar_t>(0xA5)}};
    std::wstring out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'&') {
            const size_t semicolon = text.find(L';', i + 1);
            if (semicolon != std::wstring_view::npos && semicolon - i <= 10) {
                const std::wstring name(text.substr(i + 1, semicolon - i - 1));
                unsigned long code = 0;
                bool ok = false;
                if (!name.empty() && name[0] == L'#') {
                    code = name.size() > 1 && (name[1] == L'x' || name[1] == L'X') ? std::wcstoul(name.c_str() + 2, nullptr, 16) : std::wcstoul(name.c_str() + 1, nullptr, 10);
                    ok = code > 0;
                } else {
                    for (const auto& [entity, value] : named) if (name == entity) { code = value; ok = true; break; }
                }
                if (ok) {
                    if (code >= 0x10000) { out += static_cast<wchar_t>(0xD800 + ((code - 0x10000) >> 10)); out += static_cast<wchar_t>(0xDC00 + ((code - 0x10000) & 0x3FF)); }
                    else out += static_cast<wchar_t>(code);
                    i = semicolon;
                    ++count;
                    continue;
                }
            }
        }
        out += text[i];
    }
    return out;
}

int ToolboxCurrentTool() {
    const LRESULT index = SendMessageW(g_toolbox.tool, CB_GETCURSEL, 0, 0);
    return index >= 0 && index < static_cast<LRESULT>(g_toolbox.shown.size()) ? g_toolbox.shown[static_cast<size_t>(index)] : kToolJson;
}

void RunToolbox() {
    if (!g_toolbox.panel) return;
    const int tool = ToolboxCurrentTool();
    const int action = static_cast<int>(std::max<LRESULT>(0, SendMessageW(g_toolbox.action, CB_GETCURSEL, 0, 0)));
    std::wstring input = GetWindowTextValue(g_toolbox.input);
    std::wstring output, status, error;
    switch (tool) {
    case kToolJson:
        if (TrimSpace(input).empty()) { status = L"输入或选中 JSON"; break; }
        if (!JsonValidator(input).Run(error)) { status = L"✗ " + error; break; }
        if (action == 1) {
            output = CompactJson(input, error);
            status = error.empty() ? L"已压缩为 " + std::to_wstring(output.size()) + L" 个字符" : error;
        } else {
            std::wstring formatted;
            if (FormatJson(input, formatted, error)) {
                if (action == 0) output = ToCrLf(formatted);
                status = L"✓ JSON 格式正确";
            } else {
                status = L"✗ " + error;
            }
        }
        break;
    case kToolBase64:
        if (action == 0) output = Base64Encode(input);
        else if (!input.empty() && !Base64Decode(input, output)) status = L"不是有效的 Base64（或解码后不是 UTF-8 文本）";
        else output = ToCrLf(output);
        break;
    case kToolUrl:
        if (action == 0) output = UrlEncode(input);
        else if (action == 1) {
            int decoded = 0;
            output = ToCrLf(UrlDecode(input, decoded));
            status = L"解码了 " + std::to_wstring(decoded) + L" 处";
        } else if (!TrimSpace(input).empty()) RunUrlParse(input, output, status);
        break;
    case kToolJwt:
        if (TrimSpace(input).empty()) status = L"输入或选中一个 JWT（可带 Bearer 前缀）";
        else RunJwt(input, output, status);
        break;
    case kToolTimestamp:
        RunTimestamp(input, output, status);
        break;
    case kToolHash: {
        const std::string bytes = WideToUtf8(input);
        output = L"MD5：\r\n" + HashHex(bytes, CALG_MD5) + L"\r\n\r\nSHA-1：\r\n" + HashHex(bytes, CALG_SHA1) + L"\r\n\r\nSHA-256：\r\n" +
            HashHex(bytes, CALG_SHA_256) + L"\r\n\r\nSHA-512：\r\n" + HashHex(bytes, CALG_SHA_512);
        status = L"按 UTF-8 计算，" + std::to_wstring(bytes.size()) + L" 字节";
        break;
    }
    case kToolRegex: {
        std::wstring text = input;
        text.erase(std::remove(text.begin(), text.end(), L'\r'), text.end());
        RunRegexTest(text, GetWindowTextValue(g_toolbox.pattern), GetWindowTextValue(g_toolbox.replace),
            SendMessageW(g_toolbox.ignoreCase, BM_GETCHECK, 0, 0) == BST_CHECKED, output, status);
        break;
    }
    case kToolUnicode: {
        int count = 0;
        output = action == 0 ? UnicodeEscape(input) : UnicodeUnescape(input, count);
        if (action == 1) status = L"还原了 " + std::to_wstring(count) + L" 处";
        break;
    }
    case kToolHtml: {
        int count = 0;
        output = action == 0 ? HtmlEncode(input) : HtmlDecode(input, count);
        if (action == 1) status = L"解码了 " + std::to_wstring(count) + L" 处";
        break;
    }
    case kToolUuid: {
        const int count = std::clamp(_wtoi(TrimSpace(input).c_str()), 1, 1000);
        for (int i = 0; i < count; ++i) {
            GUID guid{};
            CoCreateGuid(&guid);
            wchar_t text[64]{};
            swprintf_s(text, L"%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\r\n", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
                guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
            output += text;
        }
        if (!output.empty()) output.erase(output.size() - 2);
        status = L"输入数字可一次生成多个（最多 1000）";
        break;
    }
    default:
        break;
    }
    SetWindowTextW(g_toolbox.output, output.c_str());
    SetWindowTextW(g_toolbox.status, status.c_str());
}

// ---- Selection ---------------------------------------------------------------------------

std::wstring ActiveSelectionText() {
    HWND editor = ActiveEditor();
    if (!editor) return {};
    CHARRANGE selection{};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const LONG length = selection.cpMax - selection.cpMin;
    if (length <= 0 || length > (4 << 20)) return {};
    std::wstring selected(static_cast<size_t>(length) + 1, L'\0');
    selected.resize(static_cast<size_t>(std::max<LRESULT>(0, SendMessageW(editor, EM_GETSELTEXT, 0, reinterpret_cast<LPARAM>(selected.data())))));
    return selected;
}

void ToolboxTakeSelection(bool force) {
    if (!g_toolbox.panel || !IsWindowVisible(g_toolbox.panel)) return;
    const std::wstring selected = ActiveSelectionText();
    if (selected.empty() && !force) return;
    g_toolbox.settingInput = true;
    SetWindowTextW(g_toolbox.input, ToCrLf(selected).c_str());
    g_toolbox.settingInput = false;
    RunToolbox();
}

// Called on every caret / selection change in the editor.
void ToolboxOnSelectionChanged() {
    if (g_toolbox.panel && IsWindowVisible(g_toolbox.panel) && SendMessageW(g_toolbox.follow, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        SetTimer(g_toolbox.panel, kTimerToolboxSelection, 150, nullptr);
    }
}

// Output replaces the editor's selection (or goes in at the caret); one undo step.
void ToolboxApplyToEditor() {
    HWND editor = ActiveEditor();
    if (!editor) return;
    std::wstring output = GetWindowTextValue(g_toolbox.output);
    if (output.empty()) { MessageBeep(MB_ICONWARNING); return; }
    std::wstring text;
    for (size_t i = 0; i < output.size(); ++i) {
        if (output[i] == L'\r' && i + 1 < output.size() && output[i + 1] == L'\n') ++i;
        text += output[i] == L'\n' ? L'\r' : output[i];
    }
    SendMessageW(g_toolbox.follow, BM_SETCHECK, BST_UNCHECKED, 0);  // keep the result from replacing the input
    SendMessageW(editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
    SetFocus(editor);
    ShowNotice(L"已替换选中内容（可用 Ctrl+Z 撤销）；“跟随选中”已暂停");
}

// ---- Panel -------------------------------------------------------------------------------

void FillToolboxActions() {
    const int tool = ToolboxCurrentTool();
    SendMessageW(g_toolbox.action, CB_RESETCONTENT, 0, 0);
    const std::vector<std::wstring> actions = ToolboxActions(tool);
    for (const std::wstring& action : actions) SendMessageW(g_toolbox.action, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(action.c_str()));
    SendMessageW(g_toolbox.action, CB_SETCURSEL, 0, 0);
    ShowWindow(g_toolbox.action, actions.empty() ? SW_HIDE : SW_SHOW);
    const bool regex = tool == kToolRegex;
    for (const int id : {kTbPattern, kTbReplace, kTbIgnoreCase, kTbPatternLabel, kTbReplaceLabel}) ShowWindow(GetDlgItem(g_toolbox.panel, id), regex ? SW_SHOW : SW_HIDE);
    SetWindowTextW(GetDlgItem(g_toolbox.panel, kTbInputLabel), regex ? L"测试文本" : tool == kToolUuid ? L"生成数量" : L"输入");
    SendMessageW(g_toolbox.input, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(tool == kToolTimestamp ? L"Unix 时间戳或日期；留空显示当前时间" : L""));
    g_app.toolboxTool = tool;
}

void FillToolboxTools() {
    SendMessageW(g_toolbox.tool, CB_RESETCONTENT, 0, 0);
    g_toolbox.shown.clear();
    int select = 0;
    for (int tool = 0; tool < kToolCount; ++tool) {
        if (!(g_app.toolboxMask & (1u << tool))) continue;
        if (tool == g_app.toolboxTool) select = static_cast<int>(g_toolbox.shown.size());
        g_toolbox.shown.push_back(tool);
        SendMessageW(g_toolbox.tool, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kToolboxNames[tool]));
    }
    SendMessageW(g_toolbox.tool, CB_SETCURSEL, select, 0);
    FillToolboxActions();
}

void LayoutToolbox() {
    RECT client{};
    GetClientRect(g_toolbox.panel, &client);
    const int left = Scale(10), width = std::max(0, static_cast<int>(client.right) - Scale(18));
    const int rowH = Scale(26);
    int y = Scale(8);
    HWND panel = g_toolbox.panel;
    MoveWindow(GetDlgItem(panel, kTbTitle), left, y + Scale(3), width - Scale(64), Scale(20), TRUE);
    MoveWindow(GetDlgItem(panel, kTbCustomize), left + width - Scale(56), y, Scale(26), rowH, TRUE);
    MoveWindow(GetDlgItem(panel, kTbClose), left + width - Scale(26), y, Scale(26), rowH, TRUE);
    y += rowH + Scale(8);
    const bool hasActions = IsWindowVisible(g_toolbox.action) != FALSE;
    const int toolWidth = hasActions ? (width - Scale(6)) * 55 / 100 : width;
    MoveWindow(g_toolbox.tool, left, y, toolWidth, Scale(300), TRUE);
    MoveWindow(g_toolbox.action, left + toolWidth + Scale(6), y, width - toolWidth - Scale(6), Scale(300), TRUE);
    y += rowH + Scale(8);
    if (ToolboxCurrentTool() == kToolRegex) {
        MoveWindow(GetDlgItem(panel, kTbPatternLabel), left, y + Scale(4), Scale(44), Scale(20), TRUE);
        MoveWindow(g_toolbox.pattern, left + Scale(48), y, width - Scale(48), rowH, TRUE);
        y += rowH + Scale(6);
        MoveWindow(GetDlgItem(panel, kTbReplaceLabel), left, y + Scale(4), Scale(44), Scale(20), TRUE);
        MoveWindow(g_toolbox.replace, left + Scale(48), y, width - Scale(48), rowH, TRUE);
        y += rowH + Scale(6);
        MoveWindow(g_toolbox.ignoreCase, left, y, width, Scale(22), TRUE);
        y += Scale(28);
    }
    MoveWindow(GetDlgItem(panel, kTbInputLabel), left, y + Scale(3), Scale(70), Scale(20), TRUE);
    MoveWindow(g_toolbox.follow, left + Scale(72), y, width - Scale(72) - Scale(70), Scale(24), TRUE);
    MoveWindow(GetDlgItem(panel, kTbTake), left + width - Scale(64), y, Scale(64), Scale(24), TRUE);
    y += Scale(28);
    // Input and output share what is left above the buttons and status line.
    const int buttonsY = std::max(y + Scale(120), static_cast<int>(client.bottom) - Scale(10) - Scale(20) - Scale(6) - rowH);
    const int available = std::max(Scale(80), buttonsY - Scale(8) - y - Scale(8) - Scale(22));
    const int inputHeight = available * 2 / 5;
    MoveWindow(g_toolbox.input, left, y, width, inputHeight, TRUE);
    y += inputHeight + Scale(8);
    MoveWindow(GetDlgItem(panel, kTbOutputLabel), left, y, width, Scale(20), TRUE);
    y += Scale(22);
    MoveWindow(g_toolbox.output, left, y, width, available - inputHeight, TRUE);
    y = buttonsY;
    const int half = (width - Scale(6)) / 2;
    MoveWindow(GetDlgItem(panel, kTbCopy), left, y, half, rowH, TRUE);
    MoveWindow(GetDlgItem(panel, kTbApply), left + half + Scale(6), y, width - half - Scale(6), rowH, TRUE);
    y += rowH + Scale(6);
    MoveWindow(g_toolbox.status, left, y, width, Scale(20), TRUE);
}

void CustomizeToolbox() {
    std::vector<std::pair<std::wstring, bool>> checks;
    for (int tool = 0; tool < kToolCount; ++tool) checks.emplace_back(kToolboxNames[tool], (g_app.toolboxMask & (1u << tool)) != 0);
    std::vector<std::wstring> values;
    if (!PromptForText(L"自定义开发者工具箱：选择要显示的工具", {}, values, &checks)) return;
    unsigned mask = 0;
    for (int tool = 0; tool < kToolCount; ++tool) if (checks[static_cast<size_t>(tool)].second) mask |= 1u << tool;
    g_app.toolboxMask = mask ? mask : (1u << kToolJson);
    g_app.stateDirty = true;
    FillToolboxTools();
    LayoutToolbox();
    RunToolbox();
}

HBRUSH ToolboxEditBrush() {
    static HBRUSH brush = nullptr;
    static COLORREF color = CLR_INVALID;
    if (color != CurrentPalette().controlBackground) {
        if (brush) DeleteObject(brush);
        color = CurrentPalette().controlBackground;
        brush = CreateSolidBrush(color);
    }
    return brush;
}

LRESULT CALLBACK ToolboxProc(HWND panel, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        g_toolbox.panel = panel;
        const auto make = [&](const wchar_t* klass, const wchar_t* text, DWORD style, int id, DWORD exStyle = 0) {
            HWND control = CreateWindowExW(exStyle, klass, text, WS_CHILD | style, 0, 0, 0, 0, panel,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_app.instance, nullptr);
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
            return control;
        };
        HWND title = make(L"STATIC", L"开发者工具箱", WS_VISIBLE | SS_NOPREFIX, kTbTitle);
        (void)title;
        make(L"BUTTON", L"⚙", WS_VISIBLE | WS_TABSTOP, kTbCustomize);
        make(L"BUTTON", L"✕", WS_VISIBLE | WS_TABSTOP, kTbClose);
        g_toolbox.tool = make(L"COMBOBOX", nullptr, WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, kTbTool);
        g_toolbox.action = make(L"COMBOBOX", nullptr, WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, kTbAction);
        make(L"STATIC", L"正则", SS_NOPREFIX, kTbPatternLabel);
        g_toolbox.pattern = make(L"EDIT", nullptr, WS_TABSTOP | ES_AUTOHSCROLL, kTbPattern, WS_EX_CLIENTEDGE);
        make(L"STATIC", L"替换为", SS_NOPREFIX, kTbReplaceLabel);
        g_toolbox.replace = make(L"EDIT", nullptr, WS_TABSTOP | ES_AUTOHSCROLL, kTbReplace, WS_EX_CLIENTEDGE);
        SendMessageW(g_toolbox.pattern, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"例如 (\\d+)-(\\w+)"));
        SendMessageW(g_toolbox.replace, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"可选，支持 $1 $2"));
        g_toolbox.ignoreCase = make(L"BUTTON", L"忽略大小写", WS_TABSTOP | BS_AUTOCHECKBOX, kTbIgnoreCase);
        make(L"STATIC", L"输入", WS_VISIBLE | SS_NOPREFIX, kTbInputLabel);
        g_toolbox.follow = make(L"BUTTON", L"跟随编辑器选中", WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, kTbFollow);
        SendMessageW(g_toolbox.follow, BM_SETCHECK, BST_CHECKED, 0);
        make(L"BUTTON", L"取选中", WS_VISIBLE | WS_TABSTOP, kTbTake);
        g_toolbox.input = make(L"EDIT", nullptr, WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, kTbInput, WS_EX_CLIENTEDGE);
        make(L"STATIC", L"结果", WS_VISIBLE | SS_NOPREFIX, kTbOutputLabel);
        g_toolbox.output = make(L"EDIT", nullptr, WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, kTbOutput, WS_EX_CLIENTEDGE);
        make(L"BUTTON", L"复制结果", WS_VISIBLE | WS_TABSTOP, kTbCopy);
        make(L"BUTTON", L"替换选中内容", WS_VISIBLE | WS_TABSTOP, kTbApply);
        g_toolbox.status = make(L"STATIC", nullptr, WS_VISIBLE | SS_NOPREFIX | SS_ENDELLIPSIS, kTbStatus);
        for (HWND box : {g_toolbox.input, g_toolbox.output, g_toolbox.pattern, g_toolbox.replace}) {
            SendMessageW(box, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.editorFont), TRUE);
            SendMessageW(box, EM_SETLIMITTEXT, 0, 0);
        }
        ThemeCheckbox(g_toolbox.follow);
        ThemeCheckbox(g_toolbox.ignoreCase);
        if (g_app.darkTheme) {
            for (HWND control : {g_toolbox.input, g_toolbox.output, g_toolbox.pattern, g_toolbox.replace, g_toolbox.tool, g_toolbox.action}) SetWindowTheme(control, L"DarkMode_CFD", nullptr);
            for (const int id : {kTbCustomize, kTbClose, kTbTake, kTbCopy, kTbApply}) SetWindowTheme(GetDlgItem(panel, id), L"DarkMode_Explorer", nullptr);
        }
        FillToolboxTools();
        return 0;
    }
    case WM_SIZE:
        LayoutToolbox();
        return 0;
    case WM_ERASEBKGND: {
        RECT client{};
        GetClientRect(panel, &client);
        FillRect(reinterpret_cast<HDC>(wParam), &client, g_app.toolbarBrush);
        // Divider on the left edge.
        RECT divider{0, 0, Scale(1), client.bottom};
        HBRUSH brush = CreateSolidBrush(CurrentPalette().panelBorder);
        FillRect(reinterpret_cast<HDC>(wParam), &divider, brush);
        DeleteObject(brush);
        return 1;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        HWND control = reinterpret_cast<HWND>(lParam);
        if (control == g_toolbox.output) {
            SetTextColor(dc, CurrentPalette().editorText);
            SetBkColor(dc, CurrentPalette().controlBackground);
            return reinterpret_cast<LRESULT>(ToolboxEditBrush());
        }
        const std::wstring status = control == g_toolbox.status ? GetWindowTextValue(control) : L"";
        SetTextColor(dc, control == g_toolbox.status ? (status.rfind(L"✗", 0) == 0 || status.find(L"错误") != std::wstring::npos || status.find(L"无") == 0 || status.find(L"不是") == 0
            ? (g_app.darkTheme ? RGB(244, 112, 103) : RGB(190, 30, 30)) : CurrentPalette().statusText) : CurrentPalette().toolbarText);
        SetBkColor(dc, CurrentPalette().toolbarBackground);
        return reinterpret_cast<LRESULT>(g_app.toolbarBrush);
    }
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, CurrentPalette().toolbarText);
        SetBkColor(dc, CurrentPalette().toolbarBackground);
        return reinterpret_cast<LRESULT>(g_app.toolbarBrush);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, CurrentPalette().editorText);
        SetBkColor(dc, CurrentPalette().controlBackground);
        return reinterpret_cast<LRESULT>(ToolboxEditBrush());
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(panel, &point);
            if (point.x < Scale(5)) { SetCursor(LoadCursorW(nullptr, IDC_SIZEWE)); return TRUE; }
        }
        break;
    case WM_LBUTTONDOWN:
        if (GET_X_LPARAM(lParam) < Scale(5)) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(panel, &point);
            g_toolbox.dragging = true;
            g_toolbox.dragStartX = point.x;
            g_toolbox.dragStartWidth = Scale(g_app.toolboxWidth);
            SetCapture(panel);
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (g_toolbox.dragging) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ClientToScreen(panel, &point);
            const int pixels = std::clamp(g_toolbox.dragStartWidth + (g_toolbox.dragStartX - static_cast<int>(point.x)), Scale(260), Scale(900));
            g_app.toolboxWidth = MulDiv(pixels, 96, static_cast<int>(g_app.dpi));
            LayoutChildren(g_app.window);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (g_toolbox.dragging) {
            g_toolbox.dragging = false;
            ReleaseCapture();
            g_app.stateDirty = true;
            return 0;
        }
        break;
    case WM_TIMER:
        KillTimer(panel, wParam);
        if (wParam == kTimerToolboxRun) RunToolbox();
        else if (wParam == kTimerToolboxSelection) ToolboxTakeSelection(false);
        return 0;
    case WM_COMMAND: {
        const int id = LOWORD(wParam), code = HIWORD(wParam);
        switch (id) {
        case kTbTool:
            if (code == CBN_SELCHANGE) { FillToolboxActions(); LayoutToolbox(); InvalidateRect(panel, nullptr, TRUE); RunToolbox(); g_app.stateDirty = true; }
            return 0;
        case kTbAction:
            if (code == CBN_SELCHANGE) RunToolbox();
            return 0;
        case kTbInput: case kTbPattern: case kTbReplace:
            if (code == EN_CHANGE && !g_toolbox.settingInput) SetTimer(panel, kTimerToolboxRun, 200, nullptr);
            return 0;
        case kTbIgnoreCase: RunToolbox(); return 0;
        case kTbFollow:
            if (SendMessageW(g_toolbox.follow, BM_GETCHECK, 0, 0) == BST_CHECKED) ToolboxTakeSelection(false);
            return 0;
        case kTbTake: ToolboxTakeSelection(true); return 0;
        case kTbCopy: {
            const std::wstring output = GetWindowTextValue(g_toolbox.output);
            if (!output.empty()) { CopyTextToClipboard(output); SetWindowTextW(g_toolbox.status, L"已复制到剪贴板"); }
            return 0;
        }
        case kTbApply: ToolboxApplyToEditor(); return 0;
        case kTbCustomize: CustomizeToolbox(); return 0;
        case kTbClose: ToggleToolbox(false); return 0;
        default: break;
        }
        break;
    }
    case WM_DESTROY:
        g_toolbox = ToolboxState{};
        break;
    default:
        break;
    }
    return DefWindowProcW(panel, message, wParam, lParam);
}

// Theme or font changed: re-theme the toolbox controls.
void ApplyToolboxTheme() {
    if (!g_toolbox.panel) return;
    EnumChildWindows(g_toolbox.panel, [](HWND control, LPARAM) -> BOOL {
        wchar_t className[32]{};
        GetClassNameW(control, className, 32);
        const bool box = control == g_toolbox.input || control == g_toolbox.output || control == g_toolbox.pattern || control == g_toolbox.replace;
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(box ? g_app.editorFont : g_app.uiFont), TRUE);
        if (_wcsicmp(className, L"ComboBox") == 0 || _wcsicmp(className, L"Edit") == 0) {
            SetWindowTheme(control, g_app.darkTheme ? L"DarkMode_CFD" : nullptr, nullptr);
        } else if (_wcsicmp(className, L"Button") == 0) {
            if ((GetWindowLongW(control, GWL_STYLE) & BS_TYPEMASK) == BS_AUTOCHECKBOX) ThemeCheckbox(control);
            else SetWindowTheme(control, g_app.darkTheme ? L"DarkMode_Explorer" : nullptr, nullptr);
        }
        return TRUE;
    }, 0);
    LayoutToolbox();
    RedrawWindow(g_toolbox.panel, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_ERASE | RDW_FRAME);
}

void ToggleToolbox(bool show) {
    if (show && !g_toolbox.panel) {
        static bool registered = false;
        if (!registered) {
            WNDCLASSW klass{};
            klass.hInstance = g_app.instance;
            klass.lpfnWndProc = ToolboxProc;
            klass.lpszClassName = L"MCnotepadToolbox";
            klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            registered = RegisterClassW(&klass) != 0;
        }
        g_app.toolboxPanel = CreateWindowExW(WS_EX_CONTROLPARENT, L"MCnotepadToolbox", nullptr, WS_CHILD | WS_CLIPCHILDREN, 0, 0, 0, 0,
            g_app.window, nullptr, g_app.instance, nullptr);
    }
    g_app.toolboxVisible = show && g_app.toolboxPanel;
    if (g_app.toolboxPanel) ShowWindow(g_app.toolboxPanel, g_app.toolboxVisible ? SW_SHOW : SW_HIDE);
    LayoutChildren(g_app.window);
    g_app.stateDirty = true;
    if (g_app.toolboxVisible) {
        LayoutToolbox();
        if (!ActiveSelectionText().empty()) ToolboxTakeSelection(false);
        else RunToolbox();
        SetFocus(g_toolbox.input);
    } else if (HWND editor = ActiveEditor()) {
        SetFocus(editor);
    }
}

