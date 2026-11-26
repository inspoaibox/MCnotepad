// ---- Text tools -------------------------------------------------------------------


std::vector<std::wstring> SplitLines(std::wstring_view text) {
    std::vector<std::wstring> lines;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == L'\r' || text[i] == L'\n') {
            lines.emplace_back(text.substr(start, i - start));
            if (i < text.size() && text[i] == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') ++i;
            start = i + 1;
        }
    }
    return lines;
}

std::wstring JoinLines(const std::vector<std::wstring>& lines, std::wstring_view separator) {
    std::wstring result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result += separator;
        result += lines[i];
    }
    return result;
}

// "\t", "\n" and "\\" typed in a prompt mean tab, line break and backslash.
std::wstring UnescapeInput(std::wstring_view input) {
    std::wstring result;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == L'\\' && i + 1 < input.size()) {
            const wchar_t next = input[i + 1];
            if (next == L't') { result += L'\t'; ++i; continue; }
            if (next == L'n') { result += L'\r'; ++i; continue; }
            if (next == L'\\') { result += L'\\'; ++i; continue; }
        }
        result += input[i];
    }
    return result;
}

bool IsBlankChar(wchar_t c) { return c == L' ' || c == L'\t' || c == 0x3000 || c == 0x00A0; }
bool IsCjk(wchar_t c) { return (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x3000 && c <= 0x303F) || (c >= 0xFF00 && c <= 0xFFEF); }

std::wstring TrimLeft(std::wstring_view s) { size_t i = 0; while (i < s.size() && IsBlankChar(s[i])) ++i; return std::wstring(s.substr(i)); }
std::wstring TrimRight(std::wstring_view s) { size_t n = s.size(); while (n > 0 && IsBlankChar(s[n - 1])) --n; return std::wstring(s.substr(0, n)); }

std::wstring MapCase(std::wstring_view text, DWORD flags) {
    if (text.empty()) return {};
    const int size = LCMapStringEx(LOCALE_NAME_USER_DEFAULT, flags, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr, 0);
    if (size <= 0) return std::wstring(text);
    std::wstring result(static_cast<size_t>(size), L'\0');
    LCMapStringEx(LOCALE_NAME_USER_DEFAULT, flags, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr, 0);
    return result;
}

// Breaks one line into pieces of at most `width` characters, preferring the
// last space. Walks with an offset (copying the remainder on every break made
// long single-line files quadratic) and never splits a surrogate pair.
void WrapLineByWidth(const std::wstring& line, size_t width, std::vector<std::wstring>& out, int& breaks) {
    width = std::max<size_t>(width, 1);
    size_t position = 0;
    while (line.size() - position > width) {
        size_t cut = width;
        for (size_t k = width; k > 0; --k) {
            if (line[position + k] == L' ') { cut = k; break; }
        }
        if (cut > 1 && IS_HIGH_SURROGATE(line[position + cut - 1])) --cut;
        out.push_back(TrimRight(line.substr(position, cut)));
        position += cut;
        while (position < line.size() && (line[position] == L' ' || line[position] == L'\t')) ++position;
        ++breaks;
    }
    out.push_back(line.substr(position));
}

// Mandarin pinyin, loaded on first use from <exe dir>/dict/mandarin.
Pinyin::Pinyin* PinyinConverter(std::wstring& error) {
    static std::unique_ptr<Pinyin::Pinyin> converter;
    static bool tried = false;
    if (converter) return converter.get();
    if (tried) {
        error = L"拼音词典加载失败。";
        return nullptr;
    }
    tried = true;
    const fs::path directory = fs::path(CurrentExecutablePath()).parent_path() / L"dict";
    std::error_code ec;
    if (!fs::exists(directory / L"mandarin" / L"word.txt", ec)) {
        error = L"找不到拼音词典：" + (directory / L"mandarin").wstring() + L"\n请确认程序目录下的 dict 文件夹完整。";
        tried = false;
        return nullptr;
    }
    try {
        Pinyin::setDictionaryPath(directory);
        converter = std::make_unique<Pinyin::Pinyin>();
    } catch (...) {
        error = L"拼音词典加载失败。";
        return nullptr;
    }
    return converter.get();
}

// Simplified <-> Traditional with OpenCC (phrase-aware: 头发 -> 頭髮, 干燥 ->
// 乾燥). Dictionaries are loaded on first use from <exe dir>/resources/opencc.
bool ConvertChineseScript(std::wstring_view text, bool toTraditional, std::wstring& output) {
    static std::unique_ptr<opencc::SimpleConverter> converters[2];
    static bool failed[2]{};
    const int which = toTraditional ? 0 : 1;
    if (!converters[which] && !failed[which]) {
        const fs::path config = fs::path(CurrentExecutablePath()).parent_path() / L"resources" / L"opencc" /
            (toTraditional ? L"s2t.json" : L"t2s.json");
        std::error_code error;
        if (fs::exists(config, error)) {
            try {
                converters[which] = std::make_unique<opencc::SimpleConverter>(WideToUtf8(config.wstring()));
            } catch (...) {
                failed[which] = true;
            }
        } else {
            failed[which] = true;
        }
    }
    if (!converters[which]) return false;
    try {
        output = Utf8ToWide(converters[which]->Convert(WideToUtf8(text)));
    } catch (...) {
        return false;
    }
    return true;
}

enum class PinyinMode { Tone, Plain, Initials };

// Converts Chinese characters (polyphones resolved by phrase) and keeps all
// other text. Syllables are separated by spaces; initials are run together.
// annotate: keep each character and add its reading, e.g. 中(zhōng)文(wén).
bool ConvertToPinyin(std::wstring_view text, PinyinMode mode, std::wstring& output, int& converted, std::wstring& error,
    bool annotate = false) {
    Pinyin::Pinyin* converter = PinyinConverter(error);
    if (!converter) return false;
    const auto style = mode == PinyinMode::Tone ? Pinyin::ManTone::Style::TONE : Pinyin::ManTone::Style::NORMAL;
    Pinyin::PinyinResVector results;
    try {
        results = converter->hanziToPinyin(WideToUtf8(text), style, Pinyin::Error::Default, false, true, false);
    } catch (...) {
        error = L"拼音转换失败。";
        return false;
    }
    output.clear();
    converted = 0;
    bool previousWasPinyin = false;
    // The library drops whitespace; walk the source alongside its results and
    // copy anything it skipped, so no original character is lost.
    size_t cursor = 0;
    const auto copySkipped = [&](size_t upTo) {
        if (upTo <= cursor) return;
        output.append(text.substr(cursor, upTo - cursor));
        previousWasPinyin = false;
        cursor = upTo;
    };
    for (const Pinyin::PinyinRes& item : results) {
        const std::wstring piece = Utf8ToWide(item.hanzi);
        if (!piece.empty()) {
            const size_t found = text.find(piece, cursor);
            if (found != std::wstring_view::npos) {
                copySkipped(found);
                cursor = found + piece.size();
            }
        }
        if (annotate) {
            const std::wstring hanzi = Utf8ToWide(item.hanzi);
            output += hanzi;
            if (!item.error) {
                const std::wstring syllable = Utf8ToWide(item.pinyin);
                output += L"(" + (mode == PinyinMode::Initials ? syllable.substr(0, 1) : syllable) + L")";
                ++converted;
            }
            continue;
        }
        if (item.error) {
            const std::wstring raw = Utf8ToWide(item.hanzi);
            // Keep "拼音 word" readable: a space between a syllable and a following word.
            if (mode != PinyinMode::Initials && previousWasPinyin && !raw.empty() && iswalnum(raw[0])) output += L' ';
            output += raw;
            previousWasPinyin = false;
            continue;
        }
        std::wstring syllable = Utf8ToWide(item.pinyin);
        if (mode == PinyinMode::Initials) {
            output += syllable.substr(0, 1);
        } else {
            if (!output.empty() && !iswspace(output.back()) && (previousWasPinyin || iswalnum(output.back()))) output += L' ';
            output += syllable;
        }
        previousWasPinyin = true;
        ++converted;
    }
    copySkipped(text.size());
    return true;
}

// Every tool works on the selection, or on the whole document when nothing is
// selected. Line tools first widen the range to whole lines.
bool ToolRange(HWND editor, bool wholeLines, CHARRANGE& range, std::wstring& text, bool& wholeDocument) {
    const std::wstring all = GetEditorRawText(editor);
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
    const LONG length = static_cast<LONG>(all.size());
    wholeDocument = range.cpMin == range.cpMax;
    if (wholeDocument) {
        range = CHARRANGE{0, length};
        // RichEdit keeps a final paragraph mark; leave it alone.
        if (length > 0 && all[static_cast<size_t>(length - 1)] == L'\r') range.cpMax = length - 1;
    } else if (wholeLines) {
        range.cpMin = std::clamp(range.cpMin, 0L, length);
        range.cpMax = std::clamp(range.cpMax, 0L, length);
        // A selection ending at the start of a line does not include that line.
        if (range.cpMax > range.cpMin && all[static_cast<size_t>(range.cpMax - 1)] == L'\r') --range.cpMax;
        while (range.cpMin > 0 && all[static_cast<size_t>(range.cpMin - 1)] != L'\r') --range.cpMin;
        while (range.cpMax < length && all[static_cast<size_t>(range.cpMax)] != L'\r') ++range.cpMax;
    }
    range.cpMin = std::clamp(range.cpMin, 0L, length);
    range.cpMax = std::clamp(range.cpMax, range.cpMin, length);
    text = all.substr(static_cast<size_t>(range.cpMin), static_cast<size_t>(range.cpMax - range.cpMin));
    return true;
}

// Values remembered between uses of the prompts.
std::wstring g_joinSeparator = L"";
std::wstring g_groupSize = L"2";
std::wstring g_splitDelimiter = L",";
std::wstring g_breakAfter = L"。！？";
std::wstring g_wrapWidth = L"80";
std::wstring g_linePrefix = L"- ";
std::wstring g_lineSuffix = L";";
int g_pinyinFormat = 0;  // 0 tones, 1 plain, 2 initials
int g_pinyinLayout = 0;  // 0 pinyin line under each line (original kept), 1 pinyin only

bool IsExtractionCommand(int command) {
    return command == kIdExtractEmails || command == kIdExtractUrls || command == kIdExtractIps ||
        command == kIdExtractNumbers || command == kIdExtractKeywordLines || command == kIdExtractFields ||
        command == kIdExtractDates || command == kIdExtractPhones || command == kIdExtractUuid ||
        command == kIdExtractHexColors || command == kIdExtractQuoted || command == kIdExtractCustomRegex;
}

bool IsTextToolCommand(int command) {
    return (command >= kIdTextToolFirst && command <= kIdTextToolLast) || IsExtractionCommand(command);
}

bool IsDataGeneratorCommand(int command) {
    return command >= kIdDataFirst && command <= kIdDataLast;
}

void AppendDataGeneratorMenus(HMENU menu) {
    auto add = [](HMENU target, int id, const wchar_t* text) {
        AppendMenuW(target, MF_STRING, id, text);
    };
    HMENU basic = CreatePopupMenu();
    add(basic, kDataLineNumber, L"连续行号");
    add(basic, kDataArithmetic, L"等差数列");
    add(basic, kDataGeometric, L"等比数列");
    add(basic, kDataRandomInteger, L"随机整数");
    add(basic, kDataRandomDecimal, L"随机小数");
    add(basic, kDataRandomPercent, L"随机百分比");
    add(basic, kDataRandomLetter, L"随机字母");
    add(basic, kDataRandomString, L"随机字符串");
    add(basic, kDataBoolean, L"随机布尔值");
    add(basic, kDataPassword, L"随机密码");
    add(basic, kDataCaptcha, L"随机验证码");
    add(basic, kDataColor, L"随机颜色（HEX / RGB / HSL）");
    add(basic, kDataChineseCharacter, L"随机中文字符");
    add(basic, kDataEnglishWord, L"随机英文单词");

    HMENU ids = CreatePopupMenu();
    add(ids, kDataUuid, L"UUID"); add(ids, kDataGuid, L"GUID"); add(ids, kDataUlid, L"ULID");
    add(ids, kDataNanoId, L"NanoID"); add(ids, kDataSnowflake, L"Snowflake ID");
    add(ids, kDataOrderNumber, L"随机订单编号"); add(ids, kDataSku, L"随机商品 SKU");
    add(ids, kDataBarcode, L"随机条形码（UPC / EAN）"); add(ids, kDataSerialNumber, L"随机序列号");

    HMENU dates = CreatePopupMenu();
    add(dates, kDataCurrentDate, L"当前日期"); add(dates, kDataCurrentTime, L"当前时间");
    add(dates, kDataCurrentDateTime, L"当前日期时间"); add(dates, kDataUnixTimestamp, L"Unix 时间戳");
    add(dates, kDataIsoTime, L"ISO 8601 时间"); add(dates, kDataRandomDate, L"随机日期");
    add(dates, kDataRandomTime, L"随机时间"); add(dates, kDataDateSequence, L"连续日期序列");
    add(dates, kDataTimeSequence, L"自定义时间间隔序列");

    HMENU people = CreatePopupMenu();
    add(people, kDataChineseName, L"随机中文姓名"); add(people, kDataEnglishName, L"随机英文姓名");
    add(people, kDataUsername, L"随机用户名"); add(people, kDataEmail, L"随机电子邮箱");
    add(people, kDataPhone, L"虚拟手机号码"); add(people, kDataTelephone, L"虚拟电话号码");
    add(people, kDataGender, L"随机性别"); add(people, kDataAge, L"随机年龄");
    add(people, kDataBirthDate, L"随机出生日期"); add(people, kDataCountry, L"虚拟国家及地区");
    add(people, kDataCity, L"虚拟城市"); add(people, kDataStreet, L"虚拟街道地址");
    add(people, kDataPostcode, L"虚拟邮政编码"); add(people, kDataCompany, L"虚拟公司名称");
    add(people, kDataJob, L"虚拟职业名称"); add(people, kDataProfile, L"虚拟个人资料");
    add(people, kDataFullProfile, L"完整个人信息");

    HMENU network = CreatePopupMenu();
    add(network, kDataIpv4, L"随机 IPv4 地址"); add(network, kDataIpv6, L"随机 IPv6 地址");
    add(network, kDataMac, L"随机 MAC 地址"); add(network, kDataPort, L"随机端口号");
    add(network, kDataDomain, L"虚拟域名"); add(network, kDataUrl, L"虚拟 URL");
    add(network, kDataUserAgent, L"随机 User-Agent"); add(network, kDataApiKey, L"随机 API Key（测试用）");
    add(network, kDataToken, L"随机 Token（测试用）"); add(network, kDataJwt, L"随机 JWT（测试用）");
    add(network, kDataHex, L"随机十六进制数据"); add(network, kDataBinary, L"随机二进制数据");
    add(network, kDataBase64, L"随机 Base64 数据"); add(network, kDataHash, L"随机哈希值");
    add(network, kDataHttpHeaders, L"虚拟 HTTP 请求头");

    HMENU text = CreatePopupMenu();
    add(text, kDataLorem, L"Lorem Ipsum 占位文本"); add(text, kDataChineseLorem, L"中文占位文本");
    add(text, kDataEnglishLorem, L"英文占位文本"); add(text, kDataSentence, L"随机句子");
    add(text, kDataParagraph, L"随机段落"); add(text, kDataRepeatText, L"重复文本");
    add(text, kDataRepeatChar, L"重复字符"); add(text, kDataBlankLines, L"指定数量空行");
    add(text, kDataDivider, L"文本分隔线"); add(text, kDataMarkdownTable, L"Markdown 表格");
    add(text, kDataMarkdownToc, L"Markdown 目录"); add(text, kDataMarkdownTasks, L"Markdown 任务清单");

    HMENU structured = CreatePopupMenu();
    add(structured, kDataJson, L"JSON 测试数据"); add(structured, kDataJsonl, L"JSONL 测试数据");
    add(structured, kDataXml, L"XML 测试数据"); add(structured, kDataCsv, L"CSV 测试数据");
    add(structured, kDataTsv, L"TSV 测试数据"); add(structured, kDataYaml, L"YAML 测试数据");
    add(structured, kDataToml, L"TOML 测试数据"); add(structured, kDataSql, L"SQL INSERT 测试数据");
    add(structured, kDataHtmlTable, L"HTML 表格"); add(structured, kDataTemplate, L"自定义模板数据");
    add(structured, kDataMultiField, L"多字段组合数据"); add(structured, kDataUsers, L"批量虚拟用户数据");
    add(structured, kDataProducts, L"批量虚拟商品数据"); add(structured, kDataOrders, L"批量虚拟订单数据");
    add(structured, kDataLogs, L"模拟系统日志"); add(structured, kDataApiResponse, L"模拟 API 响应数据");

    // Clicking “数据” opens the categories directly. Keep one level of
    // navigation so every generator is reachable without an extra “生成” hop.
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(basic), L"基础数据生成");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(ids), L"标识符生成");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(dates), L"日期与时间生成");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(people), L"虚拟个人信息");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(network), L"网络与开发数据");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(text), L"文本内容生成");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(structured), L"结构化测试数据");
}

struct DataGeneratorOptions {
    int count = 10;
    int64_t start = 1;
    int64_t end = 100;
    int64_t step = 1;
    int length = 12;
    int padding = 0;
    uint64_t seed = 0;
    std::wstring prefix;
    std::wstring suffix;
    std::wstring separator = L"\\n";
    std::wstring charset = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::wstring repeatText = L"示例文本";
    bool unique = false;
    bool descending = false;
    int locale = 0;   // 0 zh_CN, 1 en_US, 2 ja_JP, 3 de_DE, 4 fr_FR
};

faker::Locale DataLocale(int index) {
    switch (index) {
    case 1: return faker::Locale::en_US;
    case 2: return faker::Locale::ja_JP;
    case 3: return faker::Locale::de_DE;
    case 4: return faker::Locale::fr_FR;
    default: return faker::Locale::zh_CN;
    }
}

std::wstring DataUnescape(std::wstring value) {
    std::wstring result;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == L'\\' && i + 1 < value.size()) {
            const wchar_t next = value[++i];
            if (next == L'n') result += L"\r\n";
            else if (next == L'r') result += L'\r';
            else if (next == L't') result += L'\t';
            else result += next;
        } else result += value[i];
    }
    return result;
}

std::wstring DataJoin(const std::vector<std::wstring>& values, const DataGeneratorOptions& options) {
    const std::wstring separator = DataUnescape(options.separator);
    std::wstring result;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) result += separator;
        result += options.prefix;
        result += values[i];
        result += options.suffix;
    }
    return result;
}

std::wstring DataNow(const wchar_t* format) {
    std::time_t now = std::time(nullptr);
    tm local{};
    localtime_s(&local, &now);
    wchar_t buffer[80]{};
    wcsftime(buffer, std::size(buffer), format, &local);
    return buffer;
}

std::wstring DataRandomString(std::mt19937_64& rng, int length, const std::wstring& charset) {
    const std::wstring pool = charset.empty() ? L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789" : charset;
    std::uniform_int_distribution<size_t> pick(0, pool.size() - 1);
    std::wstring result;
    result.reserve(static_cast<size_t>(length));
    for (int i = 0; i < length; ++i) result += pool[pick(rng)];
    return result;
}

std::wstring DataPad(int64_t value, int width) {
    std::wstring result = std::to_wstring(value);
    if (width > static_cast<int>(result.size())) result.insert(0, static_cast<size_t>(width - result.size()), L'0');
    return result;
}

std::wstring DataJsonEscape(std::wstring value) {
    std::wstring out;
    for (wchar_t c : value) {
        if (c == L'\\') out += L"\\\\";
        else if (c == L'\"') out += L"\\\"";
        else if (c == L'\r') out += L"\\r";
        else if (c == L'\n') out += L"\\n";
        else out += c;
    }
    return out;
}

void DataReplaceAll(std::wstring& value, std::wstring_view needle, std::wstring_view replacement) {
    if (needle.empty()) return;
    size_t position = 0;
    while ((position = value.find(needle, position)) != std::wstring::npos) {
        value.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
}

std::wstring DataGeneratorLabel(int command) {
    static const std::unordered_map<int, std::wstring> labels{
        {kDataLineNumber,L"连续行号"},{kDataArithmetic,L"等差数列"},{kDataGeometric,L"等比数列"},
        {kDataRandomInteger,L"随机整数"},{kDataRandomDecimal,L"随机小数"},{kDataRandomPercent,L"随机百分比"},
        {kDataRandomLetter,L"随机字母"},{kDataRandomString,L"随机字符串"},{kDataBoolean,L"随机布尔值"},
        {kDataPassword,L"随机密码"},{kDataCaptcha,L"随机验证码"},{kDataColor,L"随机颜色"},
        {kDataChineseCharacter,L"随机中文字符"},{kDataEnglishWord,L"随机英文单词"},{kDataUuid,L"UUID"},
        {kDataGuid,L"GUID"},{kDataUlid,L"ULID"},{kDataNanoId,L"NanoID"},{kDataSnowflake,L"Snowflake ID"},
        {kDataOrderNumber,L"随机订单编号"},{kDataSku,L"随机商品 SKU"},{kDataBarcode,L"随机条形码"},
        {kDataSerialNumber,L"随机序列号"},{kDataCurrentDate,L"当前日期"},{kDataCurrentTime,L"当前时间"},
        {kDataCurrentDateTime,L"当前日期时间"},{kDataUnixTimestamp,L"Unix 时间戳"},{kDataIsoTime,L"ISO 8601 时间"},
        {kDataRandomDate,L"随机日期"},{kDataRandomTime,L"随机时间"},{kDataDateSequence,L"连续日期序列"},
        {kDataTimeSequence,L"时间间隔序列"},{kDataChineseName,L"随机中文姓名"},{kDataEnglishName,L"随机英文姓名"},
        {kDataUsername,L"随机用户名"},{kDataEmail,L"随机电子邮箱"},{kDataPhone,L"虚拟手机号码"},
        {kDataTelephone,L"虚拟电话号码"},{kDataGender,L"随机性别"},{kDataAge,L"随机年龄"},
        {kDataBirthDate,L"随机出生日期"},{kDataCountry,L"虚拟国家及地区"},{kDataCity,L"虚拟城市"},
        {kDataStreet,L"虚拟街道地址"},{kDataPostcode,L"虚拟邮政编码"},{kDataCompany,L"虚拟公司名称"},
        {kDataJob,L"虚拟职业名称"},{kDataProfile,L"虚拟个人资料"},{kDataFullProfile,L"完整个人信息"},{kDataIpv4,L"随机 IPv4"},{kDataIpv6,L"随机 IPv6"},
        {kDataMac,L"随机 MAC"},{kDataPort,L"随机端口"},{kDataDomain,L"虚拟域名"},{kDataUrl,L"虚拟 URL"},
        {kDataUserAgent,L"随机 User-Agent"},{kDataApiKey,L"随机 API Key"},{kDataToken,L"随机 Token"},
        {kDataJwt,L"随机 JWT"},{kDataHex,L"随机十六进制"},{kDataBinary,L"随机二进制"},{kDataBase64,L"随机 Base64"},
        {kDataHash,L"随机哈希"},{kDataHttpHeaders,L"虚拟 HTTP 请求头"},{kDataLorem,L"Lorem Ipsum"},
        {kDataChineseLorem,L"中文占位文本"},{kDataEnglishLorem,L"英文占位文本"},{kDataSentence,L"随机句子"},
        {kDataParagraph,L"随机段落"},{kDataRepeatText,L"重复文本"},{kDataRepeatChar,L"重复字符"},
        {kDataBlankLines,L"指定数量空行"},{kDataDivider,L"文本分隔线"},{kDataMarkdownTable,L"Markdown 表格"},
        {kDataMarkdownToc,L"Markdown 目录"},{kDataMarkdownTasks,L"Markdown 任务清单"},{kDataJson,L"JSON 测试数据"},
        {kDataJsonl,L"JSONL 测试数据"},{kDataXml,L"XML 测试数据"},{kDataCsv,L"CSV 测试数据"},
        {kDataTsv,L"TSV 测试数据"},{kDataYaml,L"YAML 测试数据"},{kDataToml,L"TOML 测试数据"},
        {kDataSql,L"SQL INSERT 测试数据"},{kDataHtmlTable,L"HTML 表格"},{kDataTemplate,L"自定义模板数据"},
        {kDataMultiField,L"多字段组合数据"},{kDataUsers,L"批量虚拟用户数据"},{kDataProducts,L"批量虚拟商品数据"},
        {kDataOrders,L"批量虚拟订单数据"},{kDataLogs,L"模拟系统日志"},{kDataApiResponse,L"模拟 API 响应数据"}
    };
    const auto it = labels.find(command);
    return it == labels.end() ? L"数据生成" : it->second;
}

std::wstring GenerateDataValue(int command, const DataGeneratorOptions& options, std::mt19937_64& rng, int index) {
    const faker::Locale locale = DataLocale(options.locale);
    const auto randomInt = [&](int64_t lo, int64_t hi) {
        if (lo > hi) std::swap(lo, hi);
        return std::uniform_int_distribution<int64_t>(lo, hi)(rng);
    };
    switch (command) {
    case kDataLineNumber: return DataPad(options.start + options.step * index, options.padding);
    case kDataArithmetic: return DataPad(options.start + options.step * index, options.padding);
    case kDataGeometric: return DataPad(static_cast<int64_t>(std::llround(options.start * std::pow(static_cast<double>(options.step), index))), options.padding);
    case kDataRandomInteger: return std::to_wstring(randomInt(options.start, options.end));
    case kDataRandomDecimal: { std::uniform_real_distribution<double> d(static_cast<double>(std::min(options.start, options.end)), static_cast<double>(std::max(options.start, options.end))); std::wostringstream s; s << std::fixed << std::setprecision(4) << d(rng); return s.str(); }
    case kDataRandomPercent: { std::wostringstream s; s << randomInt(0, 100) << L"%"; return s.str(); }
    case kDataRandomLetter: { const wchar_t letters[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"; return std::wstring(1, letters[randomInt(0, 51)]); }
    case kDataRandomString: return DataRandomString(rng, options.length, options.charset);
    case kDataBoolean: return randomInt(0, 1) ? L"true" : L"false";
    case kDataPassword: return Utf8ToWide(faker::internet::password(std::max(4, options.length)));
    case kDataCaptcha: { std::wstring chars = L"0123456789"; return DataRandomString(rng, std::max(1, options.length), chars); }
    case kDataColor: { const int kind = index % 3; return Utf8ToWide(kind == 0 ? faker::color::hex() : kind == 1 ? faker::color::rgb() : faker::color::hsl()); }
    case kDataChineseCharacter: { static const wchar_t chars[] = L"的一是不了人我在有他这为之大来以个中上们到说时国和地要就出会可也你对生能而子那得于着下自之年过发后作里用道行所然家种事成方多经么去法学如都同现当没动面起看定天分还进好小部其些主样理心她本前开但因只从想实日军者意无力它与长把机十民第手"; return std::wstring(1, chars[randomInt(0, static_cast<int64_t>(std::size(chars) - 2))]); }
    case kDataEnglishWord: return Utf8ToWide(faker::word::sample(std::nullopt, faker::Locale::en_US));
    case kDataUuid: case kDataGuid: return Utf8ToWide(faker::string::uuidV4());
    case kDataUlid: return Utf8ToWide(faker::string::ulid());
    case kDataNanoId: return Utf8ToWide(faker::string::nanoId(std::max(4, options.length)));
    case kDataSnowflake: { const uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()); return std::to_wstring((now << 12) | (static_cast<uint64_t>(index) & 0xFFF)); }
    case kDataOrderNumber: return L"ORD-" + DataNow(L"%Y%m%d") + DataPad(randomInt(0, 999999), 6);
    case kDataSku: return Utf8ToWide(faker::commerce::sku(std::max(4, options.length)));
    case kDataBarcode: return Utf8ToWide((index % 2) ? faker::commerce::EAN13() : faker::commerce::EAN8());
    case kDataSerialNumber: return L"SN-" + DataRandomString(rng, std::max(6, options.length), L"ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    case kDataCurrentDate: return DataNow(L"%Y-%m-%d");
    case kDataCurrentTime: return DataNow(L"%H:%M:%S");
    case kDataCurrentDateTime: return DataNow(L"%Y-%m-%d %H:%M:%S");
    case kDataUnixTimestamp: return std::to_wstring(std::time(nullptr));
    case kDataIsoTime: return Utf8ToWide(faker::date::anytime(faker::date::DateFormat::ISO));
    case kDataRandomDate: return Utf8ToWide(faker::date::anytime(faker::date::DateFormat::ISO)).substr(0, 10);
    case kDataRandomTime: return Utf8ToWide(faker::date::anytime(faker::date::DateFormat::ISO)).substr(11, 8);
    case kDataDateSequence: { std::time_t t = std::time(nullptr) + static_cast<time_t>(options.step * index * 86400); tm local{}; localtime_s(&local, &t); wchar_t b[32]{}; wcsftime(b, std::size(b), L"%Y-%m-%d", &local); return b; }
    case kDataTimeSequence: { std::time_t t = std::time(nullptr) + static_cast<time_t>(options.step * index * 60); tm local{}; localtime_s(&local, &t); wchar_t b[32]{}; wcsftime(b, std::size(b), L"%Y-%m-%d %H:%M:%S", &local); return b; }
    case kDataChineseName: return Utf8ToWide(faker::person::fullName(locale));
    case kDataEnglishName: return Utf8ToWide(faker::person::fullName(faker::Locale::en_US));
    case kDataUsername: return Utf8ToWide(faker::internet::username(std::nullopt, std::nullopt, locale));
    case kDataEmail: return Utf8ToWide(faker::internet::exampleEmail(std::nullopt, std::nullopt, locale));
    case kDataPhone: return Utf8ToWide(faker::phone::phoneNumberByFormat());
    case kDataTelephone: return Utf8ToWide(faker::phone::phoneNumberByFormat());
    case kDataGender: return Utf8ToWide(faker::person::gender());
    case kDataAge: return std::to_wstring(randomInt(18, 80));
    case kDataBirthDate: return Utf8ToWide(faker::date::birthdateByAge(18, 80));
    case kDataCountry: return Utf8ToWide(faker::location::country());
    case kDataCity: return Utf8ToWide(faker::location::city(locale));
    case kDataStreet: return Utf8ToWide(faker::location::streetAddress(locale));
    case kDataPostcode: return Utf8ToWide(faker::location::zipCode(locale));
    case kDataCompany: return Utf8ToWide(faker::company::companyName(std::nullopt, locale));
    case kDataJob: return Utf8ToWide(faker::person::jobTitle());
    case kDataProfile: return Utf8ToWide(faker::person::bio());
    case kDataFullProfile: {
        // Build one coherent profile from the same first/last name pair so
        // the generated username and e-mail belong to the displayed person.
        const std::string firstName = std::string{faker::person::firstName(locale)};
        const std::string lastName = std::string{faker::person::lastName(locale)};
        const bool familyNameFirst = options.locale == 0 || options.locale == 2;
        const std::string fullName = familyNameFirst ? lastName + firstName : firstName + " " + lastName;
        const std::wstring username = Utf8ToWide(faker::internet::username(firstName, lastName, locale));
        const std::wstring email = Utf8ToWide(faker::internet::exampleEmail(firstName, lastName, locale));
        const std::wstring mobile = Utf8ToWide(faker::phone::phoneNumberByFormat());
        const std::wstring telephone = Utf8ToWide(faker::phone::phoneNumberByFormat());
        const std::wstring gender = Utf8ToWide(faker::person::sex(locale));
        const int age = static_cast<int>(randomInt(18, 80));
        // faker-cxx requires a non-empty time range. Passing the same age as
        // both bounds can produce a zero-second range and throw internally.
        const std::string birthDateValue = faker::date::birthdateByAge(
            static_cast<unsigned>(age), static_cast<unsigned>(age + 1), faker::date::DateFormat::ISO);
        const std::wstring birthDate = Utf8ToWide(birthDateValue.substr(0, 10));
        const std::wstring country = Utf8ToWide(faker::location::country());
        const std::wstring city = Utf8ToWide(faker::location::city(locale));
        const std::wstring street = Utf8ToWide(faker::location::streetAddress(locale));
        const std::wstring postcode = Utf8ToWide(faker::location::zipCode(locale));
        const std::wstring company = Utf8ToWide(faker::company::companyName(std::nullopt, locale));
        const std::wstring job = Utf8ToWide(faker::person::jobTitle());
        const std::wstring profile = Utf8ToWide(faker::person::bio());

        std::wstring result;
        result += L"姓名：" + Utf8ToWide(fullName) + L"\r\n";
        result += L"用户名：" + username + L"\r\n";
        result += L"电子邮箱：" + email + L"\r\n";
        result += L"手机号码：" + mobile + L"\r\n";
        result += L"电话号码：" + telephone + L"\r\n";
        result += L"性别：" + gender + L"\r\n";
        result += L"年龄：" + std::to_wstring(age) + L"\r\n";
        result += L"出生日期：" + birthDate + L"\r\n";
        result += L"国家/地区：" + country + L"\r\n";
        result += L"城市：" + city + L"\r\n";
        result += L"街道地址：" + street + L"\r\n";
        result += L"邮政编码：" + postcode + L"\r\n";
        result += L"公司：" + company + L"\r\n";
        result += L"职业：" + job + L"\r\n";
        result += L"个人简介：" + profile;
        return result;
    }
    case kDataIpv4: return Utf8ToWide(faker::internet::ipv4());
    case kDataIpv6: return Utf8ToWide(faker::internet::ipv6());
    case kDataMac: return Utf8ToWide(faker::internet::mac());
    case kDataPort: return std::to_wstring(faker::internet::port());
    case kDataDomain: return Utf8ToWide(faker::internet::domainName(locale));
    case kDataUrl: return Utf8ToWide(faker::internet::url());
    case kDataUserAgent: return L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/" + std::to_wstring(randomInt(110, 130)) + L".0 Safari/537.36";
    case kDataApiKey: return L"test_" + DataRandomString(rng, std::max(16, options.length), L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
    case kDataToken: return DataRandomString(rng, std::max(24, options.length), L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_~");
    case kDataJwt: return Utf8ToWide(faker::internet::jwtToken());
    case kDataHex: return DataRandomString(rng, std::max(2, options.length), L"0123456789abcdef");
    case kDataBinary: return DataRandomString(rng, std::max(2, options.length), L"01");
    case kDataBase64: return Base64Encode(DataRandomString(rng, std::max(4, options.length), options.charset));
    case kDataHash: return Utf8ToWide(faker::crypto::sha256());
    case kDataHttpHeaders: return Utf8ToWide(faker::internet::httpRequestHeader()) + L": " + Utf8ToWide(faker::internet::httpMediaType());
    case kDataLorem: return Utf8ToWide(faker::lorem::sentence());
    case kDataChineseLorem: return L"这是用于测试和演示的中文占位文本，内容会根据生成数量自动重复。";
    case kDataEnglishLorem: return Utf8ToWide(faker::lorem::sentence());
    case kDataSentence: return Utf8ToWide(faker::lorem::sentence());
    case kDataParagraph: return Utf8ToWide(faker::lorem::paragraph());
    case kDataRepeatText: return options.repeatText;
    case kDataRepeatChar: return std::wstring(std::max(1, options.length), options.repeatText.empty() ? L'喵' : options.repeatText.front());
    case kDataBlankLines: return {};
    case kDataDivider: return L"────────────────────────────────────────";
    case kDataMarkdownTable: return L"| 字段 | 示例值 |\r\n| --- | --- |\r\n| id | " + Utf8ToWide(faker::string::uuidV4()) + L" |";
    case kDataMarkdownToc: return L"- [概述](#概述)\r\n- [安装](#安装)\r\n- [使用方法](#使用方法)";
    case kDataMarkdownTasks: return L"- [ ] 待办事项\r\n- [ ] 测试数据\r\n- [x] 已完成事项";
    case kDataJson: return L"{\"id\":\"" + Utf8ToWide(faker::string::uuidV4()) + L"\",\"name\":\"" + DataJsonEscape(Utf8ToWide(faker::person::fullName(locale))) + L"\",\"email\":\"" + Utf8ToWide(faker::internet::exampleEmail()) + L"\"}";
    case kDataJsonl: return L"{\"id\":\"" + Utf8ToWide(faker::string::uuidV4()) + L"\",\"value\":" + std::to_wstring(randomInt(1, 9999)) + L"}";
    case kDataXml: return L"<item><id>" + Utf8ToWide(faker::string::uuidV4()) + L"</id><name>" + DataJsonEscape(Utf8ToWide(faker::commerce::productName())) + L"</name></item>";
    case kDataCsv: return L"id,name,email\r\n" + Utf8ToWide(faker::string::uuidV4()) + L"," + Utf8ToWide(faker::person::fullName(locale)) + L"," + Utf8ToWide(faker::internet::exampleEmail());
    case kDataTsv: return L"id\tname\temail\r\n" + Utf8ToWide(faker::string::uuidV4()) + L"\t" + Utf8ToWide(faker::person::fullName(locale)) + L"\t" + Utf8ToWide(faker::internet::exampleEmail());
    case kDataYaml: return L"id: " + Utf8ToWide(faker::string::uuidV4()) + L"\r\nname: \"" + DataJsonEscape(Utf8ToWide(faker::person::fullName(locale))) + L"\"\r\nactive: true";
    case kDataToml: return L"id = \"" + Utf8ToWide(faker::string::uuidV4()) + L"\"\r\nname = \"" + DataJsonEscape(Utf8ToWide(faker::person::fullName(locale))) + L"\"";
    case kDataSql: return L"INSERT INTO users (id, name, email) VALUES ('" + Utf8ToWide(faker::string::uuidV4()) + L"', '" + DataJsonEscape(Utf8ToWide(faker::person::fullName(locale))) + L"', '" + Utf8ToWide(faker::internet::exampleEmail()) + L"');";
    case kDataHtmlTable: return L"<table>\r\n  <tr><th>ID</th><th>Name</th></tr>\r\n  <tr><td>" + Utf8ToWide(faker::string::uuidV4()) + L"</td><td>" + DataJsonEscape(Utf8ToWide(faker::person::fullName(locale))) + L"</td></tr>\r\n</table>";
    case kDataTemplate: {
        std::wstring result = options.repeatText.empty() ? L"{{id}} {{name}} {{email}}" : options.repeatText;
        DataReplaceAll(result, L"{{id}}", Utf8ToWide(faker::string::uuidV4()));
        DataReplaceAll(result, L"{{uuid}}", Utf8ToWide(faker::string::uuidV4()));
        DataReplaceAll(result, L"{{name}}", Utf8ToWide(faker::person::fullName(locale)));
        DataReplaceAll(result, L"{{email}}", Utf8ToWide(faker::internet::exampleEmail()));
        DataReplaceAll(result, L"{{date}}", DataNow(L"%Y-%m-%d"));
        DataReplaceAll(result, L"{{index}}", std::to_wstring(index + 1));
        return result;
    }
    case kDataMultiField: return Utf8ToWide(faker::string::uuidV4()) + L" | " + Utf8ToWide(faker::person::fullName(locale)) + L" | " + Utf8ToWide(faker::internet::exampleEmail());
    case kDataUsers: return L"{\"id\":\"" + Utf8ToWide(faker::string::uuidV4()) + L"\",\"name\":\"" + DataJsonEscape(Utf8ToWide(faker::person::fullName(locale))) + L"\",\"email\":\"" + Utf8ToWide(faker::internet::exampleEmail()) + L"\"}";
    case kDataProducts: return L"{\"sku\":\"" + Utf8ToWide(faker::commerce::sku()) + L"\",\"name\":\"" + DataJsonEscape(Utf8ToWide(faker::commerce::productFullName())) + L"\"}";
    case kDataOrders: return L"{\"orderNo\":\"ORD-" + DataNow(L"%Y%m%d") + DataPad(randomInt(0, 999999), 6) + L"\",\"status\":\"paid\"}";
    case kDataLogs: return DataNow(L"%Y-%m-%d %H:%M:%S") + L" [INFO] request_id=" + Utf8ToWide(faker::string::uuidV4()) + L" status=200 latency=" + std::to_wstring(randomInt(1, 999)) + L"ms";
    case kDataApiResponse: return L"{\"code\":200,\"message\":\"ok\",\"data\":{\"id\":\"" + Utf8ToWide(faker::string::uuidV4()) + L"\",\"timestamp\":" + std::to_wstring(std::time(nullptr)) + L"}}";
    default: return {};
    }
}

std::wstring GenerateDataText(int command, const DataGeneratorOptions& options) {
    std::mt19937_64 rng(options.seed ? options.seed : std::random_device{}());
    const int count = std::clamp(options.count, 1, 100000);
    std::vector<std::wstring> values;
    values.reserve(static_cast<size_t>(count));
    std::unordered_set<std::wstring> seen;
    for (int i = 0; i < count; ++i) {
        std::wstring value;
        int attempts = 0;
        do {
            try {
                value = GenerateDataValue(command, options, rng, options.descending ? count - i - 1 : i);
            } catch (const std::exception&) {
                value = L"[数据生成失败]";
            } catch (...) {
                value = L"[数据生成失败]";
            }
        } while (options.unique && !seen.insert(value).second && ++attempts < 100);
        values.push_back(std::move(value));
    }
    if (command == kDataBlankLines) values.assign(static_cast<size_t>(count), L"");
    return DataJoin(values, options);
}

struct DataGeneratorDialogState {
    int command = kDataRandomString;
    HWND window = nullptr;
    std::vector<HWND> edits;
    HWND locale = nullptr;
    HWND output = nullptr;
    HWND unique = nullptr;
    HWND descending = nullptr;
    HWND preview = nullptr;
    bool done = false;
    int action = 0;
    CHARRANGE target{};
};

std::wstring DataDialogText(HWND window) {
    return window ? GetWindowTextValue(window) : std::wstring{};
}

DataGeneratorOptions DataDialogOptions(const DataGeneratorDialogState& state) {
    DataGeneratorOptions options;
    auto number = [&](size_t index, int fallback) {
        return index < state.edits.size() ? std::max(0, _wtoi(DataDialogText(state.edits[index]).c_str())) : fallback;
    };
    auto int64Value = [&](size_t index, int64_t fallback) {
        if (index >= state.edits.size()) return fallback;
        try { return std::stoll(DataDialogText(state.edits[index])); } catch (...) { return fallback; }
    };
    options.count = std::clamp(number(0, 10), 1, 100000);
    options.start = int64Value(1, 1);
    options.end = int64Value(2, 100);
    options.step = int64Value(3, 1);
    options.length = std::clamp(number(4, 12), 1, 4096);
    options.prefix = DataDialogText(state.edits[5]);
    options.suffix = DataDialogText(state.edits[6]);
    options.separator = DataDialogText(state.edits[7]);
    options.padding = std::clamp(number(8, 0), 0, 64);
    try { options.seed = static_cast<uint64_t>(std::stoull(DataDialogText(state.edits[9]))); } catch (...) { options.seed = 0; }
    options.charset = DataDialogText(state.edits[10]);
    options.repeatText = DataDialogText(state.edits[11]);
    options.locale = state.locale ? static_cast<int>(SendMessageW(state.locale, CB_GETCURSEL, 0, 0)) : 0;
    options.unique = state.unique && SendMessageW(state.unique, BM_GETCHECK, 0, 0) == BST_CHECKED;
    options.descending = state.descending && SendMessageW(state.descending, BM_GETCHECK, 0, 0) == BST_CHECKED;
    return options;
}

fs::path DataTemplatePath() {
    return g_app.dataDirectory / L"data-generator-template.dat";
}

bool SaveDataTemplate(const DataGeneratorOptions& options) {
    std::ostringstream output;
    output << "DATA_TEMPLATE\t1\t" << options.count << '\t' << options.start << '\t' << options.end << '\t'
           << options.step << '\t' << options.length << '\t' << options.padding << '\t' << options.seed << '\t'
           << options.locale << '\t' << (options.unique ? 1 : 0) << '\t' << (options.descending ? 1 : 0) << '\t'
           << HexEncode(WideToUtf8(options.prefix)) << '\t' << HexEncode(WideToUtf8(options.suffix)) << '\t'
           << HexEncode(WideToUtf8(options.separator)) << '\t' << HexEncode(WideToUtf8(options.charset)) << '\t'
           << HexEncode(WideToUtf8(options.repeatText)) << '\n';
    return WriteDataFile(DataTemplatePath(), output.str());
}

bool LoadDataTemplate(DataGeneratorOptions& options) {
    std::ifstream input(DataTemplatePath(), std::ios::binary);
    if (!input) return false;
    std::string line; std::getline(input, line);
    const std::vector<std::string> parts = SplitTabs(line);
    if (parts.size() < 17 || parts[0] != "DATA_TEMPLATE") return false;
    ParseInt(parts[2], options.count); ParseInt64(parts[3], options.start); ParseInt64(parts[4], options.end);
    ParseInt64(parts[5], options.step); ParseInt(parts[6], options.length); ParseInt(parts[7], options.padding);
    int64_t seed = 0; ParseInt64(parts[8], seed); options.seed = seed >= 0 ? static_cast<uint64_t>(seed) : 0;
    ParseInt(parts[9], options.locale);
    int unique = 0, descending = 0; ParseInt(parts[10], unique); ParseInt(parts[11], descending);
    options.unique = unique != 0; options.descending = descending != 0;
    options.prefix = Utf8ToWide(HexDecode(parts[12])); options.suffix = Utf8ToWide(HexDecode(parts[13]));
    options.separator = Utf8ToWide(HexDecode(parts[14])); options.charset = Utf8ToWide(HexDecode(parts[15]));
    options.repeatText = Utf8ToWide(HexDecode(parts[16]));
    options.count = std::clamp(options.count, 1, 100000); options.length = std::clamp(options.length, 1, 4096);
    options.padding = std::clamp(options.padding, 0, 64); options.locale = std::clamp(options.locale, 0, 4);
    return true;
}

void DataDialogSetOptions(DataGeneratorDialogState& state, const DataGeneratorOptions& options) {
    const std::wstring values[] = {std::to_wstring(options.count), std::to_wstring(options.start), std::to_wstring(options.end),
        std::to_wstring(options.step), std::to_wstring(options.length), options.prefix, options.suffix, options.separator,
        std::to_wstring(options.padding), std::to_wstring(options.seed), options.charset, options.repeatText};
    for (size_t i = 0; i < std::size(values) && i < state.edits.size(); ++i) SetWindowTextW(state.edits[i], values[i].c_str());
    if (state.locale) SendMessageW(state.locale, CB_SETCURSEL, options.locale, 0);
    if (state.unique) SendMessageW(state.unique, BM_SETCHECK, options.unique ? BST_CHECKED : BST_UNCHECKED, 0);
    if (state.descending) SendMessageW(state.descending, BM_SETCHECK, options.descending ? BST_CHECKED : BST_UNCHECKED, 0);
}

void DataDialogUpdatePreview(DataGeneratorDialogState& state) {
    if (!state.preview) return;
    const DataGeneratorOptions options = DataDialogOptions(state);
    DataGeneratorOptions previewOptions = options;
    previewOptions.count = std::min(options.count, 20);
    std::wstring result = GenerateDataText(state.command, previewOptions);
    if (options.count > previewOptions.count) result += L"\r\n…（预览显示前 " + std::to_wstring(previewOptions.count) + L" 条）";
    SetWindowTextW(state.preview, result.c_str());
}

void DataDialogAddControl(HWND control) {
    if (!control) return;
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.uiFont), TRUE);
    if (g_app.darkTheme) SetWindowTheme(control, L"DarkMode_CFD", nullptr);
}

LRESULT CALLBACK DataGeneratorDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<DataGeneratorDialogState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message) {
    case WM_CREATE: {
        state = static_cast<DataGeneratorDialogState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        state->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        const ThemePalette& palette = CurrentPalette();
        const std::pair<int, const wchar_t*> fields[] = {
            {kIdDataDialogCount,L"生成数量"},{kIdDataDialogStart,L"起始值"},{kIdDataDialogEnd,L"结束值"},
            {kIdDataDialogStep,L"步长 / 间隔"},{kIdDataDialogLength,L"字符长度"},{kIdDataDialogPrefix,L"前缀"},
            {kIdDataDialogSuffix,L"后缀"},{kIdDataDialogSeparator,L"分隔符（\\n 表示换行）"},{kIdDataDialogPadding,L"补零位数"},
            {kIdDataDialogSeed,L"随机种子（0=随机）"},{kIdDataDialogCharset,L"字符集"},{kIdDataDialogRepeat,L"模板 / 重复文本"}
        };
        const std::wstring defaults[] = {L"10",L"1",L"100",L"1",L"12",L"",L"",L"\\n",L"0",L"0",DataGeneratorOptions{}.charset,L"示例文本"};
        // Keep the dialog compact enough for small laptop resolutions while
        // retaining two full parameter columns beside the preview.
        const int columnWidth = 205;
        const int rightColumnX = 240;
        for (size_t i = 0; i < std::size(fields); ++i) {
            const int column = i < 6 ? 0 : 1;
            const int row = static_cast<int>(i < 6 ? i : i - 6);
            const int x = column == 0 ? 18 : rightColumnX;
            const int y = 16 + row * 42;
            HWND label = CreateWindowExW(0, L"STATIC", fields[i].second, WS_CHILD | WS_VISIBLE,
                Scale(x), Scale(y), Scale(columnWidth), Scale(18), window, nullptr, g_app.instance, nullptr);
            HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", defaults[i].c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, Scale(x), Scale(y + 18), Scale(columnWidth), Scale(24),
                window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(fields[i].first)), g_app.instance, nullptr);
            DataDialogAddControl(label);
            DataDialogAddControl(edit);
            state->edits.push_back(edit);
            (void)palette;
        }
        HWND localeLabel = CreateWindowExW(0, L"STATIC", L"个人信息国家 / 地区", WS_CHILD | WS_VISIBLE,
            Scale(18), Scale(270), Scale(columnWidth), Scale(18), window, nullptr, g_app.instance, nullptr);
        state->locale = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
            Scale(18), Scale(288), Scale(columnWidth), Scale(140), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogLocale)), g_app.instance, nullptr);
        for (const wchar_t* item : {L"中国（简体中文）",L"美国（英文）",L"日本（日文）",L"德国（德文）",L"法国（法文）"}) SendMessageW(state->locale, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        SendMessageW(state->locale, CB_SETCURSEL, 0, 0);
        DataDialogAddControl(localeLabel); DataDialogAddControl(state->locale);
        state->unique = CreateWindowExW(0, L"BUTTON", L"允许重复以外的值尽量不重复", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            Scale(rightColumnX), Scale(270), Scale(220), Scale(24), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogUnique)), g_app.instance, nullptr);
        state->descending = CreateWindowExW(0, L"BUTTON", L"序列降序生成", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            Scale(rightColumnX), Scale(298), Scale(220), Scale(24), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogDescending)), g_app.instance, nullptr);
        ThemeCheckbox(state->unique); ThemeCheckbox(state->descending);
        DataDialogAddControl(state->unique); DataDialogAddControl(state->descending);
        HWND outputLabel = CreateWindowExW(0, L"STATIC", L"输出方式", WS_CHILD | WS_VISIBLE,
            Scale(rightColumnX), Scale(330), Scale(100), Scale(18), window, nullptr, g_app.instance, nullptr);
        state->output = CreateWindowExW(0, L"COMBOBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
            Scale(rightColumnX), Scale(348), Scale(220), Scale(140), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogOutput)), g_app.instance, nullptr);
        for (const wchar_t* item : {L"插入到光标位置",L"替换选中文本",L"追加到文档末尾",L"生成到新建文档",L"复制到剪贴板"}) SendMessageW(state->output, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item));
        SendMessageW(state->output, CB_SETCURSEL, state->target.cpMax > state->target.cpMin ? 1 : 0, 0);
        DataDialogAddControl(outputLabel); DataDialogAddControl(state->output);
        state->preview = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            Scale(450), Scale(16), Scale(310), Scale(310), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogPreview)), g_app.instance, nullptr);
        DataDialogAddControl(state->preview);
        HWND execute = CreateWindowExW(0, L"BUTTON", L"生成并执行", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            Scale(450), Scale(338), Scale(100), Scale(30), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogExecute)), g_app.instance, nullptr);
        HWND copy = CreateWindowExW(0, L"BUTTON", L"复制", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(558), Scale(338), Scale(62), Scale(30), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogCopy)), g_app.instance, nullptr);
        HWND newTab = CreateWindowExW(0, L"BUTTON", L"新标签", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(628), Scale(338), Scale(70), Scale(30), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogNewTab)), g_app.instance, nullptr);
        HWND cancel = CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(700), Scale(338), Scale(60), Scale(30), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), g_app.instance, nullptr);
        HWND saveTemplate = CreateWindowExW(0, L"BUTTON", L"保存模板", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            Scale(18), Scale(338), Scale(100), Scale(30), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDataDialogSaveTemplate)), g_app.instance, nullptr);
        for (HWND button : {execute, copy, newTab, cancel, saveTemplate}) { DataDialogAddControl(button); ThemePushButtons({button}); }
        DataGeneratorOptions templateOptions;
        if (LoadDataTemplate(templateOptions)) DataDialogSetOptions(*state, templateOptions);
        DataDialogUpdatePreview(*state);
        return 0;
    }
    case WM_COMMAND:
        if (!state) break;
        if (LOWORD(wParam) == kIdDataDialogExecute || LOWORD(wParam) == kIdDataDialogCopy || LOWORD(wParam) == kIdDataDialogNewTab || LOWORD(wParam) == kIdDataDialogSaveTemplate) {
            state->action = LOWORD(wParam) == kIdDataDialogExecute ? 1 : LOWORD(wParam) == kIdDataDialogCopy ? 2 : LOWORD(wParam) == kIdDataDialogNewTab ? 3 : 4;
            state->done = true;
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL) { state->action = 0; state->done = true; return 0; }
        if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == CBN_SELCHANGE || HIWORD(wParam) == BN_CLICKED) DataDialogUpdatePreview(*state);
        return 0;
    case WM_CLOSE:
        if (state) { state->action = 0; state->done = true; }
        return 0;
    case WM_ERASEBKGND: {
        RECT client{}; GetClientRect(window, &client); FillRect(reinterpret_cast<HDC>(wParam), &client, g_app.windowBrush); return 1;
    }
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam); SetTextColor(dc, CurrentPalette().controlText); SetBkColor(dc, CurrentPalette().windowBackground); return reinterpret_cast<LRESULT>(g_app.windowBrush);
    }
    case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: {
        HDC dc = reinterpret_cast<HDC>(wParam); SetTextColor(dc, CurrentPalette().controlText); SetBkColor(dc, CurrentPalette().controlBackground); return reinterpret_cast<LRESULT>(g_app.controlBrush);
    }
    default: break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void OpenDataGeneratorDialog(int command) {
    if (!ActiveDocument()) return;
    static bool registered = false;
    if (!registered) {
        WNDCLASSW klass{}; klass.hInstance = g_app.instance; klass.lpfnWndProc = Guarded<DataGeneratorDialogProc>;
        klass.lpszClassName = L"MCnotepadDataGenerator"; klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        registered = RegisterClassW(&klass) != 0;
    }
    DataGeneratorDialogState state; state.command = command;
    SendMessageW(ActiveDocument()->editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&state.target));
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int workWidth = std::max(1, static_cast<int>(workArea.right - workArea.left));
    const int workHeight = std::max(1, static_cast<int>(workArea.bottom - workArea.top));
    const int width = std::min(Scale(800), std::max(Scale(640), workWidth - Scale(16)));
    const int height = std::min(Scale(430), std::max(Scale(400), workHeight - Scale(16)));
    const int left = workArea.left + (workWidth - width) / 2;
    const int top = workArea.top + (workHeight - height) / 2;
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"MCnotepadDataGenerator",
        (L"数据生成 - " + DataGeneratorLabel(command)).c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU,
        left, top, width, height,
        g_app.window, nullptr, g_app.instance, &state);
    if (!dialog) return;
    ApplyWindowFrameTheme(dialog);
    EnableWindow(g_app.window, FALSE); ShowWindow(dialog, SW_SHOW); SetFocus(state.edits.empty() ? state.preview : state.edits[0]);
    MSG message{};
    ModalScope modalScope;
    while (!state.done && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    if (state.action == 0) {
        EnableWindow(g_app.window, TRUE); DestroyWindow(dialog); SetForegroundWindow(g_app.window);
        return;
    }
    DataGeneratorOptions options = DataDialogOptions(state);
    if (state.action == 4) {
        const bool saved = SaveDataTemplate(options);
        EnableWindow(g_app.window, TRUE); DestroyWindow(dialog); SetForegroundWindow(g_app.window);
        ShowNotice(saved ? L"数据生成模板已保存" : L"数据生成模板保存失败");
        return;
    }
    std::wstring generated = GenerateDataText(command, options);
    const int outputSelection = state.output ? static_cast<int>(SendMessageW(state.output, CB_GETCURSEL, 0, 0)) : 0;
    EnableWindow(g_app.window, TRUE); DestroyWindow(dialog); SetForegroundWindow(g_app.window);
    if (generated.empty() && command != kDataBlankLines) return;
    TabDocument* document = ActiveDocument(); if (!document) return;
    if (state.action == 2 || (state.action == 1 && outputSelection == 4)) {
        CopyTextToClipboard(generated); ShowNotice(L"数据生成结果已复制到剪贴板"); return;
    }
    if (state.action == 3 || (state.action == 1 && outputSelection == 3)) {
        ApplyToolResult(*document, state.target, generated, 1, DataGeneratorLabel(command), L"已生成数据"); return;
    }
    const int output = state.action == 1 ? outputSelection : 0;
    if (output == 2) {
        const LONG end = static_cast<LONG>(GetEditorRawText(document->editor).size());
        ApplyToolResult(*document, CHARRANGE{end, end}, generated, 2, DataGeneratorLabel(command), L"已生成数据");
    } else {
        ApplyToolResult(*document, output == 1 ? state.target : CHARRANGE{state.target.cpMax, state.target.cpMax}, generated, 0, DataGeneratorLabel(command), L"已生成数据");
    }
}

void AppendTextToolMenus(HMENU menu) {
    HMENU lines = CreatePopupMenu();
    AppendMenuW(lines, MF_STRING, kIdJoinLines, L"合并为一行");
    AppendMenuW(lines, MF_STRING, kIdJoinLinesSpace, L"合并为一行（用空格连接）");
    AppendMenuW(lines, MF_STRING, kIdJoinLinesCustom, L"合并为一行（自定义连接符）...");
    AppendMenuW(lines, MF_STRING, kIdJoinEveryN, L"每 N 行合并为一行...");
    AppendMenuW(lines, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(lines, MF_STRING, kIdSplitByDelimiter, L"按分隔符拆分为多行...");
    AppendMenuW(lines, MF_STRING, kIdBreakAfter, L"在指定字符后换行...");
    AppendMenuW(lines, MF_STRING, kIdWrapWidth, L"按长度换行...");
    AppendMenuW(lines, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(lines, MF_STRING, kIdRemoveEmptyLines, L"删除空行");
    AppendMenuW(lines, MF_STRING, kIdCollapseEmptyLines, L"合并连续空行");
    AppendMenuW(lines, MF_STRING, kIdRemoveDuplicateLines, L"删除重复行");
    AppendMenuW(lines, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(lines, MF_STRING, kIdSortAscending, L"排序（升序）");
    AppendMenuW(lines, MF_STRING, kIdSortDescending, L"排序（降序）");
    AppendMenuW(lines, MF_STRING, kIdReverseLines, L"反转行顺序");
    AppendMenuW(lines, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(lines, MF_STRING, kIdAddPrefix, L"每行开头添加...");
    AppendMenuW(lines, MF_STRING, kIdAddSuffix, L"每行末尾添加...");
    AppendMenuW(lines, MF_STRING, kIdNumberLines, L"添加行号");

    HMENU blanks = CreatePopupMenu();
    AppendMenuW(blanks, MF_STRING, kIdTrimLeading, L"去除行首空白");
    AppendMenuW(blanks, MF_STRING, kIdTrimTrailing, L"去除行尾空白");
    AppendMenuW(blanks, MF_STRING, kIdTrimBoth, L"去除行首和行尾空白");
    AppendMenuW(blanks, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(blanks, MF_STRING, kIdCollapseSpaces, L"连续空格合并为一个");
    AppendMenuW(blanks, MF_STRING, kIdRemoveAllSpaces, L"删除所有空格");
    AppendMenuW(blanks, MF_STRING, kIdTabsToSpaces, L"制表符转为空格");
    AppendMenuW(blanks, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(blanks, MF_STRING, kIdRemoveInvisible, L"清除不可见字符");
    AppendMenuW(blanks, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(blanks, MF_STRING, kIdUpperCase, L"转为大写");
    AppendMenuW(blanks, MF_STRING, kIdLowerCase, L"转为小写");
    AppendMenuW(blanks, MF_STRING, kIdTitleCase, L"每个单词首字母大写");

    HMENU chinese = CreatePopupMenu();
    AppendMenuW(chinese, MF_STRING, kIdPinyinTone, L"汉字转拼音...");
    AppendMenuW(chinese, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(chinese, MF_STRING, kIdToTraditional, L"简体转繁体");
    AppendMenuW(chinese, MF_STRING, kIdToSimplified, L"繁体转简体");
    AppendMenuW(chinese, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(chinese, MF_STRING, kIdFullToHalf, L"全角转半角");
    AppendMenuW(chinese, MF_STRING, kIdHalfToFull, L"半角转全角");
    AppendMenuW(chinese, MF_STRING, kIdChinesePunctToAscii, L"中文标点转英文标点");
    AppendMenuW(chinese, MF_STRING, kIdAsciiPunctToChinese, L"英文标点转中文标点");

    HMENU developer = CreatePopupMenu();
    AppendMenuW(developer, MF_STRING, kIdToolbarFormat, L"格式化 JSON\tShift+Alt+F");
    AppendMenuW(developer, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(developer, MF_STRING, kIdFormatCss, L"CSS 美化");
    AppendMenuW(developer, MF_STRING, kIdMinifyCss, L"CSS 压缩");
    AppendMenuW(developer, MF_STRING, kIdFormatJavaScript, L"JavaScript 美化");
    AppendMenuW(developer, MF_STRING, kIdMinifyJavaScript, L"JavaScript 压缩");
    AppendMenuW(developer, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(developer, MF_STRING, kIdMarkdownToHtml, L"Markdown 转 HTML");
    AppendMenuW(developer, MF_STRING, kIdHtmlToMarkdown, L"HTML 转 Markdown");
    AppendMenuW(developer, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(developer, MF_STRING, kIdBase64Encode, L"Base64 编码");
    AppendMenuW(developer, MF_STRING, kIdBase64Decode, L"Base64 解码");
    AppendMenuW(developer, MF_STRING, kIdUrlEncode, L"URL 编码");
    AppendMenuW(developer, MF_STRING, kIdUrlDecode, L"URL 解码");
    AppendMenuW(developer, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(developer, MF_STRING, kIdRegexReplace, L"正则表达式替换...");

    HMENU extract = CreatePopupMenu();
    AppendMenuW(extract, MF_STRING, kIdExtractEmails, L"邮箱地址");
    AppendMenuW(extract, MF_STRING, kIdExtractUrls, L"链接");
    AppendMenuW(extract, MF_STRING, kIdExtractIps, L"IP 地址");
    AppendMenuW(extract, MF_STRING, kIdExtractNumbers, L"数字");
    AppendMenuW(extract, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(extract, MF_STRING, kIdExtractKeywordLines, L"包含关键词的行...");
    AppendMenuW(extract, MF_STRING, kIdExtractFields, L"指定列（字段）...");
    AppendMenuW(extract, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(extract, MF_STRING, kIdExtractDates, L"日期和时间");
    AppendMenuW(extract, MF_STRING, kIdExtractPhones, L"手机号");
    AppendMenuW(extract, MF_STRING, kIdExtractUuid, L"UUID / GUID");
    AppendMenuW(extract, MF_STRING, kIdExtractHexColors, L"十六进制颜色");
    AppendMenuW(extract, MF_STRING, kIdExtractQuoted, L"引号内容");
    AppendMenuW(extract, MF_STRING, kIdExtractCustomRegex, L"自定义正则规则...");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(lines), L"行处理");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(blanks), L"空格、符号与大小写");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(chinese), L"中文处理");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(developer), L"开发者工具");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(extract), L"提取数据");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    HMENU output = CreatePopupMenu();
    AppendMenuW(output, MF_STRING, kIdOutputNewTab, L"输出到新标签（原文不变）");
    AppendMenuW(output, MF_STRING, kIdOutputInsertBelow, L"插入到原文下方（保留原文）");
    AppendMenuW(output, MF_STRING, kIdOutputReplace, L"替换原文");
    CheckMenuRadioItem(output, kIdOutputReplace, kIdOutputInsertBelow, kIdOutputReplace + g_app.toolOutputMode, MF_BYCOMMAND);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(output), L"处理结果输出到");
    AppendMenuW(menu, MF_STRING | (g_app.previewTextTools ? MF_CHECKED : 0), kIdTogglePreview, L"处理前预览更改");
}

// ---- Encoders -------------------------------------------------------------------

std::wstring Base64Encode(std::wstring_view text) {
    const std::string bytes = WideToUtf8(text);
    if (bytes.empty()) return {};
    DWORD size = 0;
    const DWORD flags = CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF;
    CryptBinaryToStringW(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size()), flags, nullptr, &size);
    std::wstring result(size, L'\0');
    CryptBinaryToStringW(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size()), flags, result.data(), &size);
    result.resize(size);
    return result;
}

// Accepts standard and URL-safe alphabets, with or without padding/whitespace.
bool Base64Decode(std::wstring_view text, std::wstring& result) {
    std::wstring clean;
    for (wchar_t c : text) {
        if (c == L'-') c = L'+';
        else if (c == L'_') c = L'/';
        if (!iswspace(c)) clean += c;
    }
    while (clean.size() % 4 != 0) clean += L'=';
    DWORD size = 0;
    if (clean.empty() || !CryptStringToBinaryW(clean.c_str(), static_cast<DWORD>(clean.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr)) {
        return false;
    }
    std::string bytes(size, '\0');
    CryptStringToBinaryW(clean.c_str(), static_cast<DWORD>(clean.size()), CRYPT_STRING_BASE64,
        reinterpret_cast<BYTE*>(bytes.data()), &size, nullptr, nullptr);
    bytes.resize(size);
    if (bytes.empty()) return false;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0) <= 0) return false;
    result = Utf8ToWide(bytes);
    return true;
}

// Percent-encodes UTF-8 bytes; unreserved characters stay as they are.
std::wstring UrlEncode(std::wstring_view text) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::wstring result;
    for (const unsigned char byte : WideToUtf8(text)) {
        if (isalnum(byte) || byte == '-' || byte == '_' || byte == '.' || byte == '~') {
            result += static_cast<wchar_t>(byte);
        } else {
            result += L'%';
            result += static_cast<wchar_t>(hex[byte >> 4]);
            result += static_cast<wchar_t>(hex[byte & 15]);
        }
    }
    return result;
}

std::wstring UrlDecode(std::wstring_view text, int& decoded) {
    std::string bytes;
    const std::string input = WideToUtf8(text);
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size() && HexDigit(input[i + 1]) >= 0 && HexDigit(input[i + 2]) >= 0) {
            bytes += static_cast<char>((HexDigit(input[i + 1]) << 4) | HexDigit(input[i + 2]));
            i += 2;
            ++decoded;
        } else {
            bytes += input[i];
        }
    }
    return Utf8ToWide(bytes);
}

