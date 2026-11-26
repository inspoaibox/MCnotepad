// ===========================================================================
// Find / replace in files: engine
// Runs on background threads. Reads files itself and never writes anything;
// replacing happens later on the UI thread after the user confirms a preview.
// ===========================================================================

enum class SearchScope { Folder = 0, OpenFiles = 1, CurrentFile = 2 };
enum class SearchTarget { Content = 0, FileName = 1, FullPath = 2 };
enum class SearchMode { Plain = 0, Regex = 1, AnyKeyword = 2, AllKeywords = 3 };

struct SearchOptions {
    std::wstring query;
    std::wstring replacement;
    std::vector<std::wstring> folders;
    SearchScope scope = SearchScope::Folder;
    SearchTarget target = SearchTarget::Content;
    SearchMode mode = SearchMode::Plain;
    bool matchCase = false;
    bool wholeWord = false;
    bool recursive = true;
    bool includeHidden = false;
    bool useExcludes = true;
    bool useGitignore = true;
    std::wstring fileTypes;
    std::wstring excludes;
    uint64_t maxFileBytes = 20ull << 20;
    size_t maxMatches = 20000;
    bool forReplace = false;
};

struct SearchHit {
    int line = 0;           // 1-based; 0 for file name / path hits
    int column = 0;         // 1-based, UTF-16 units
    size_t offset = 0;      // in the \n-normalised text
    size_t length = 0;
    std::wstring lineText;  // shown in the results (may be a window of a long line)
    size_t lineMatchStart = 0;
};

struct SearchFileResult {
    std::wstring path;      // empty for an untitled open tab
    std::wstring display;   // path relative to the search folder
    HWND editor = nullptr;  // set when searched from an open tab's text
    std::vector<SearchHit> hits;
    int encoding = 0;
    std::wstring eol = L"\r\n";
    bool mixedEol = false;
    uint64_t timestamp = 0;
    uint64_t size = 0;
    std::wstring before;    // replace mode only, \n line breaks
    std::wstring after;
    int replacements = 0;
    size_t unlistedHits = 0;  // matches beyond the per-file list limit
};

constexpr size_t kMaxListedHitsPerFile = 2000;

struct SearchSource {       // text of an open tab, captured on the UI thread
    std::wstring path;
    std::wstring display;
    HWND editor = nullptr;
    std::wstring text;
};

std::vector<std::wstring> SplitPatterns(std::wstring_view text) {
    std::vector<std::wstring> parts;
    std::wstring current;
    for (const wchar_t c : text) {
        if (c == L';' || c == L',' || c == L'\r' || c == L'\n' || c == L'；' || c == L'，') {
            const std::wstring trimmed = TrimCopy(current);
            if (!trimmed.empty()) parts.push_back(trimmed);
            current.clear();
        } else {
            current += c;
        }
    }
    const std::wstring trimmed = TrimCopy(current);
    if (!trimmed.empty()) parts.push_back(trimmed);
    return parts;
}

std::wstring LowerInvariant(std::wstring_view text) {
    if (text.empty()) return {};
    std::wstring result(text);
    LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, text.data(), static_cast<int>(text.size()),
        result.data(), static_cast<int>(result.size()), nullptr, nullptr, 0);
    return result;
}

// Letters, digits and '_' outside the CJK ranges count as word characters, so
// whole-word matching works for code and English without breaking Chinese.
bool IsSearchWordChar(wchar_t c) {
    return c < 0x2E80 && (iswalnum(c) || c == L'_');
}

class SearchMatcher {
public:
    bool Init(const SearchOptions& options, std::wstring& error) {
        options_ = options;
        if (options.mode == SearchMode::Regex) {
            try {
                auto flags = std::regex_constants::ECMAScript | std::regex_constants::multiline;
                if (!options.matchCase) flags |= std::regex_constants::icase;
                regex_ = std::make_shared<std::wregex>(options.query, flags);
            } catch (const std::regex_error& e) {
                error = L"正则表达式有误：" + Utf8ToWide(e.what());
                return false;
            }
        } else if (options.mode == SearchMode::Plain) {
            std::wstring needle = UnescapeInput(options.query);  // \n and \t allow multi-line searches
            std::replace(needle.begin(), needle.end(), L'\r', L'\n');
            needles_.push_back(options.matchCase ? needle : LowerInvariant(needle));
        } else {
            std::wistringstream stream(options.query);
            for (std::wstring word; stream >> word;) needles_.push_back(options.matchCase ? word : LowerInvariant(word));
        }
        if (options.mode != SearchMode::Regex && needles_.empty()) {
            error = L"请输入要查找的内容。";
            return false;
        }
        replacement_ = UnescapeInput(options.replacement);
        std::replace(replacement_.begin(), replacement_.end(), L'\r', L'\n');
        return true;
    }

    const SearchOptions& options() const { return options_; }
    const std::wregex* regex() const { return regex_.get(); }

    // Non-overlapping matches as (offset, length), sorted.
    void Find(const std::wstring& text, std::vector<std::pair<size_t, size_t>>& out) const {
        out.clear();
        if (regex_) {
            for (std::wsregex_iterator it(text.begin(), text.end(), *regex_), end; it != end; ++it) {
                const size_t offset = static_cast<size_t>(it->position(0));
                const size_t length = static_cast<size_t>(it->length(0));
                if (length == 0 || !WordBoundaryOk(text, offset, length)) continue;
                out.emplace_back(offset, length);
            }
            return;
        }
        const std::wstring lowered = options_.matchCase ? std::wstring() : LowerInvariant(text);
        const std::wstring& haystack = options_.matchCase ? text : lowered;
        for (const std::wstring& needle : needles_) {
            size_t found = 0;
            bool any = false;
            for (size_t position = 0; (found = haystack.find(needle, position)) != std::wstring::npos;) {
                if (WordBoundaryOk(haystack, found, needle.size())) {
                    out.emplace_back(found, needle.size());
                    any = true;
                }
                position = found + std::max<size_t>(1, needle.size());
            }
            if (options_.mode == SearchMode::AllKeywords && !any) {
                out.clear();
                return;
            }
        }
        if (needles_.size() > 1) {
            std::sort(out.begin(), out.end());
            std::vector<std::pair<size_t, size_t>> merged;
            for (const auto& match : out) {
                if (!merged.empty() && match.first < merged.back().first + merged.back().second) continue;
                merged.push_back(match);
            }
            out.swap(merged);
        }
    }

    // Matching against a file name or path (single-line strings).
    bool MatchName(const std::wstring& name, std::pair<size_t, size_t>& match) const {
        std::vector<std::pair<size_t, size_t>> matches;
        Find(name, matches);
        if (matches.empty()) return false;
        match = matches.front();
        return true;
    }

    std::wstring Replace(const std::wstring& text, int& count) const {
        count = 0;
        std::wstring result;
        size_t cursor = 0;
        if (regex_) {
            for (std::wsregex_iterator it(text.begin(), text.end(), *regex_), end; it != end; ++it) {
                const size_t offset = static_cast<size_t>(it->position(0));
                const size_t length = static_cast<size_t>(it->length(0));
                if (length == 0 || !WordBoundaryOk(text, offset, length)) continue;
                result.append(text, cursor, offset - cursor);
                result += it->format(replacement_);
                cursor = offset + length;
                ++count;
            }
        } else {
            std::vector<std::pair<size_t, size_t>> matches;
            Find(text, matches);
            for (const auto& [offset, length] : matches) {
                result.append(text, cursor, offset - cursor);
                result += replacement_;
                cursor = offset + length;
                ++count;
            }
        }
        result.append(text, cursor, std::wstring::npos);
        return result;
    }

    // Replacement text for one already-selected match (used by "replace here").
    std::wstring ReplaceOne(const std::wstring& matched) const {
        if (!regex_) return replacement_;
        try {
            return std::regex_replace(matched, *regex_, replacement_, std::regex_constants::format_first_only);
        } catch (...) {
            return replacement_;
        }
    }

private:
    bool WordBoundaryOk(const std::wstring& text, size_t offset, size_t length) const {
        if (!options_.wholeWord || length == 0) return true;
        if (IsSearchWordChar(text[offset]) && offset > 0 && IsSearchWordChar(text[offset - 1])) return false;
        const size_t end = offset + length;
        if (IsSearchWordChar(text[end - 1]) && end < text.size() && IsSearchWordChar(text[end])) return false;
        return true;
    }

    SearchOptions options_;
    std::shared_ptr<std::wregex> regex_;
    std::vector<std::wstring> needles_;
    std::wstring replacement_;
};

// Converts any line-break style to \n and reports the original style.
std::wstring NormalizeToLf(std::wstring_view text, std::wstring& eol, bool& mixed) {
    std::wstring result;
    result.reserve(text.size());
    int crlf = 0, cr = 0, lf = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r') {
            if (i + 1 < text.size() && text[i + 1] == L'\n') { ++i; ++crlf; } else { ++cr; }
            result += L'\n';
        } else {
            if (text[i] == L'\n') ++lf;
            result += text[i];
        }
    }
    eol = crlf >= lf && crlf >= cr ? L"\r\n" : lf >= cr ? L"\n" : L"\r";
    mixed = (crlf > 0) + (cr > 0) + (lf > 0) > 1;
    if (crlf + cr + lf == 0) eol = L"\r\n";
    return result;
}

// Fills line/column and a display excerpt for each match.
void DescribeHits(const std::wstring& text, const std::vector<std::pair<size_t, size_t>>& matches, std::vector<SearchHit>& hits) {
    std::vector<size_t> lineStarts{0};
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\n') lineStarts.push_back(i + 1);
    }
    for (const auto& [offset, length] : matches) {
        const size_t line = static_cast<size_t>(std::upper_bound(lineStarts.begin(), lineStarts.end(), offset) - lineStarts.begin()) - 1;
        const size_t start = lineStarts[line];
        // The next line start is known: no rescan of (possibly huge) lines per hit.
        const size_t end = line + 1 < lineStarts.size() ? lineStarts[line + 1] - 1 : text.size();
        SearchHit hit;
        hit.line = static_cast<int>(line + 1);
        hit.column = static_cast<int>(offset - start + 1);
        hit.offset = offset;
        hit.length = length;
        // Long lines (minified files) are cut to a window around the match.
        size_t from = start;
        size_t to = end;
        if (to - from > 300) {
            from = offset > start + 80 ? offset - 80 : start;
            to = std::min(end, offset + std::min<size_t>(length, 120) + 160);
        }
        hit.lineText = text.substr(from, to - from);
        hit.lineMatchStart = offset - from;
        if (from > start) {
            hit.lineText.insert(0, L"…");
            hit.lineMatchStart += 1;
        }
        if (to < end) hit.lineText += L"…";
        std::replace(hit.lineText.begin(), hit.lineText.end(), L'\t', L' ');
        hits.push_back(std::move(hit));
    }
}

// ---- .gitignore (common subset: globs, dir/ suffix, leading /, ** prefixes; no negation)

struct GitIgnoreRule {
    std::wstring base;     // directory containing the .gitignore
    std::wstring pattern;  // backslash separated
    bool directoryOnly = false;
    bool anchored = false; // contains a slash: matched against the path below `base`
};

void LoadGitIgnore(const std::wstring& directory, std::vector<GitIgnoreRule>& rules) {
    std::vector<char> bytes;
    if (!ReadBinaryFile(fs::path(directory) / L".gitignore", bytes)) return;
    for (std::wstring line : SplitLines(DecodeTextBytes(bytes))) {
        line = TrimCopy(line);
        if (line.empty() || line[0] == L'#' || line[0] == L'!') continue;
        GitIgnoreRule rule;
        rule.base = directory;
        if (line.back() == L'/') {
            rule.directoryOnly = true;
            line.pop_back();
        }
        if (line.size() > 3 && line.ends_with(L"/**")) {
            line.resize(line.size() - 3);
            rule.directoryOnly = true;
        }
        while (line.starts_with(L"**/")) line.erase(0, 3);
        if (line.starts_with(L"/")) {
            line.erase(0, 1);
            rule.anchored = true;
        }
        if (line.find(L'/') != std::wstring::npos) rule.anchored = true;
        std::replace(line.begin(), line.end(), L'/', L'\\');
        if (line.empty()) continue;
        rule.pattern = line;
        rules.push_back(std::move(rule));
    }
}

bool GitIgnored(const std::vector<GitIgnoreRule>& rules, const std::wstring& fullPath, const std::wstring& name, bool isDirectory) {
    for (const GitIgnoreRule& rule : rules) {
        if (rule.directoryOnly && !isDirectory) continue;
        if (rule.anchored) {
            if (fullPath.size() <= rule.base.size() + 1) continue;
            const std::wstring relative = fullPath.substr(rule.base.size() + 1);
            if (PathMatchSpecW(relative.c_str(), rule.pattern.c_str())) return true;
        } else if (PathMatchSpecW(name.c_str(), rule.pattern.c_str())) {
            return true;
        }
    }
    return false;
}

// ---- Job ------------------------------------------------------------------------

struct SearchFileEntry {
    std::wstring path;
    std::wstring display;
    uint64_t size = 0;
};

struct SearchJob {
    int generation = 0;
    SearchMatcher matcher;
    std::vector<SearchSource> sources;  // open-tab scopes
    std::atomic<bool> cancel{false};
    std::atomic<bool> enumerating{true};
    std::atomic<int> enumerated{0};
    std::atomic<int> scanned{0};
    std::atomic<int> skippedLarge{0};
    std::atomic<int> skippedBinary{0};
    std::atomic<int> errorCount{0};
    std::atomic<int> filesHit{0};
    std::atomic<long long> matches{0};
    std::atomic<bool> truncated{false};
    std::mutex mutex;
    std::vector<std::unique_ptr<SearchFileResult>> pending;
    std::vector<std::wstring> errors;
    ULONGLONG started = 0;
    ULONGLONG elapsed = 0;
    HWND notify = nullptr;
    HANDLE finished = nullptr;

    void AddError(const std::wstring& path, const std::wstring& reason) {
        ++errorCount;
        std::lock_guard lock(mutex);
        if (errors.size() < 5000) errors.push_back(path + L"\t" + reason);
    }
    bool Stopped() const { return cancel.load() || truncated.load(); }
};

unsigned __stdcall SearchThreadEntry(void* parameter) {
    std::unique_ptr<std::function<void()>> function(static_cast<std::function<void()>*>(parameter));
    (*function)();
    return 0;
}

// std::regex recurses deeply on long matches; give workers a large stack.
HANDLE StartSearchThread(std::function<void()> function) {
    auto* heap = new std::function<void()>(std::move(function));
    const uintptr_t handle = _beginthreadex(nullptr, 64u << 20, SearchThreadEntry, heap, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (handle == 0) {
        delete heap;
        return nullptr;
    }
    return reinterpret_cast<HANDLE>(handle);
}

bool MatchesAnyPattern(const std::wstring& name, const std::vector<std::wstring>& patterns) {
    for (const std::wstring& pattern : patterns) {
        if (PathMatchSpecW(name.c_str(), pattern.c_str())) return true;
    }
    return false;
}

void EnumerateSearchFiles(SearchJob& job, std::vector<SearchFileEntry>& files) {
    const SearchOptions& options = job.matcher.options();
    std::vector<std::wstring> types = SplitPatterns(options.fileTypes);
    const bool allTypes = types.empty() || std::any_of(types.begin(), types.end(),
        [](const std::wstring& t) { return t == L"*" || t == L"*.*"; });
    // "js" or ".js" mean "*.js".
    for (std::wstring& type : types) {
        if (type.find_first_of(L"*?") == std::wstring::npos) type = (type[0] == L'.' ? L"*" : L"*.") + type;
    }
    std::vector<std::wstring> excludeNames;
    std::vector<std::wstring> excludePaths;
    if (options.useExcludes) {
        for (std::wstring pattern : SplitPatterns(options.excludes)) {
            std::replace(pattern.begin(), pattern.end(), L'/', L'\\');
            while (!pattern.empty() && pattern.back() == L'\\') pattern.pop_back();
            if (pattern.empty()) continue;
            (pattern.find(L'\\') != std::wstring::npos ? excludePaths : excludeNames).push_back(pattern);
        }
    }
    std::unordered_map<std::wstring, bool> seen;  // overlapping folders: each file once

    for (std::wstring root : options.folders) {
        while (root.size() > 3 && (root.back() == L'\\' || root.back() == L'/')) root.pop_back();
        struct Frame { std::wstring directory; std::vector<GitIgnoreRule> rules; };
        std::vector<Frame> stack;
        Frame first{root, {}};
        if (options.useGitignore) LoadGitIgnore(root, first.rules);
        stack.push_back(std::move(first));
        while (!stack.empty() && !job.Stopped()) {
            Frame frame = std::move(stack.back());
            stack.pop_back();
            WIN32_FIND_DATAW data{};
            HANDLE find = FindFirstFileExW((frame.directory + L"\\*").c_str(), FindExInfoBasic, &data,
                FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
            if (find == INVALID_HANDLE_VALUE) {
                const DWORD code = GetLastError();
                if (code != ERROR_FILE_NOT_FOUND) job.AddError(frame.directory, code == ERROR_ACCESS_DENIED ? L"没有访问权限" : L"无法读取目录（错误 " + std::to_wstring(code) + L"）");
                continue;
            }
            std::vector<Frame> children;
            do {
                const std::wstring name = data.cFileName;
                if (name == L"." || name == L"..") continue;
                const bool isDirectory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                if (!options.includeHidden && (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) continue;
                if (isDirectory && (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;  // no symlink loops
                const std::wstring full = frame.directory + L"\\" + name;
                const std::wstring relative = full.size() > root.size() + 1 ? full.substr(root.size() + 1) : name;
                if (MatchesAnyPattern(name, excludeNames) || MatchesAnyPattern(relative, excludePaths)) continue;
                if (options.useGitignore && GitIgnored(frame.rules, full, name, isDirectory)) continue;
                if (isDirectory) {
                    if (!options.recursive) continue;
                    Frame child{full, frame.rules};
                    if (options.useGitignore) LoadGitIgnore(full, child.rules);
                    children.push_back(std::move(child));
                    continue;
                }
                if (!allTypes && !MatchesAnyPattern(name, types)) continue;
                if (!seen.emplace(LowerInvariant(full), true).second) continue;
                const uint64_t size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
                files.push_back({full, options.folders.size() > 1 ? full : relative, size});
                ++job.enumerated;
            } while (!job.Stopped() && FindNextFileW(find, &data));
            FindClose(find);
            // Depth-first, alphabetical: push in reverse so the first child is processed first.
            for (auto it = children.rbegin(); it != children.rend(); ++it) stack.push_back(std::move(*it));
        }
    }
}

void PublishResult(SearchJob& job, std::unique_ptr<SearchFileResult> result) {
    const long long found = static_cast<long long>(result->hits.size() + result->unlistedHits);
    const long long total = job.matches.fetch_add(found) + found;
    ++job.filesHit;
    if (total >= static_cast<long long>(job.matcher.options().maxMatches)) job.truncated = true;
    std::lock_guard lock(job.mutex);
    job.pending.push_back(std::move(result));
}

void SearchText(SearchJob& job, std::unique_ptr<SearchFileResult> result, const std::wstring& normalized) {
    std::vector<std::pair<size_t, size_t>> matches;
    try {
        job.matcher.Find(normalized, matches);
        if (matches.empty()) return;
        // A single file can match millions of times (minified code, logs):
        // list the first ones and count the rest.
        if (matches.size() > kMaxListedHitsPerFile) {
            result->unlistedHits = matches.size() - kMaxListedHitsPerFile;
            matches.resize(kMaxListedHitsPerFile);
        }
        DescribeHits(normalized, matches, result->hits);
        if (job.matcher.options().forReplace) {
            result->before = normalized;
            result->after = job.matcher.Replace(normalized, result->replacements);
            if (result->after == result->before) return;
        }
    } catch (const std::exception&) {
        job.AddError(result->path.empty() ? result->display : result->path, L"正则表达式匹配失败（表达式过于复杂或文件过大）");
        return;
    }
    PublishResult(job, std::move(result));
}

void SearchOneFile(SearchJob& job, const SearchFileEntry& entry) {
    const SearchOptions& options = job.matcher.options();
    if (options.target != SearchTarget::Content) {
        const std::wstring subject = options.target == SearchTarget::FileName
            ? fs::path(entry.path).filename().wstring() : entry.path;
        ++job.scanned;
        std::pair<size_t, size_t> match;
        if (!job.matcher.MatchName(subject, match)) return;
        auto result = std::make_unique<SearchFileResult>();
        result->path = entry.path;
        result->display = entry.display;
        SearchHit hit;
        hit.offset = match.first;
        hit.length = match.second;
        hit.lineText = subject;
        hit.lineMatchStart = match.first;
        result->hits.push_back(std::move(hit));
        PublishResult(job, std::move(result));
        return;
    }
    if (entry.size > options.maxFileBytes) {
        ++job.skippedLarge;
        job.AddError(entry.path, L"已跳过：文件大于 " + std::to_wstring(options.maxFileBytes >> 20) + L" MB");
        return;
    }
    const uint64_t timestampBeforeRead = FileTimestamp(entry.path);
    std::vector<char> bytes;
    if (!ReadBinaryFile(entry.path, bytes)) {
        job.AddError(entry.path, L"无法读取（可能没有权限或正被占用）");
        return;
    }
    ++job.scanned;
    const bool utf16Bom = bytes.size() >= 2 &&
        ((static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) ||
         (static_cast<unsigned char>(bytes[0]) == 0xFE && static_cast<unsigned char>(bytes[1]) == 0xFF));
    if (!utf16Bom && memchr(bytes.data(), 0, std::min<size_t>(bytes.size(), 8192)) != nullptr) {
        ++job.skippedBinary;
        return;
    }
    auto result = std::make_unique<SearchFileResult>();
    result->path = entry.path;
    result->display = entry.display;
    result->size = bytes.size();
    result->timestamp = timestampBeforeRead;
    const std::wstring text = DecodeTextBytes(bytes, &result->encoding);
    if (job.matcher.options().forReplace) {
        // Replacing rewrites the whole file, so its bytes must survive a
        // decode/encode round trip; a damaged UTF-8 byte or a character the
        // ANSI code page can't map would otherwise change text nobody matched.
        std::string reencoded;
        if (!EncodeTextBytes(text, result->encoding, reencoded) || reencoded.size() != bytes.size() ||
            memcmp(reencoded.data(), bytes.data(), bytes.size()) != 0) {
            job.AddError(entry.path, L"已跳过替换：文件按 " + std::wstring(kEncodingNames[std::clamp(result->encoding, 0, kEncodingCount - 1)]) +
                L" 读写无法保持原样（可能含损坏的字节或混合编码），替换会改动未匹配的内容");
            return;
        }
    }
    const std::wstring normalized = NormalizeToLf(text, result->eol, result->mixedEol);
    SearchText(job, std::move(result), normalized);
}

void RunSearchJob(std::shared_ptr<SearchJob> job) {
    std::vector<SearchFileEntry> files;
    if (job->matcher.options().scope == SearchScope::Folder) EnumerateSearchFiles(*job, files);
    job->enumerating = false;

    if (!job->sources.empty()) {
        for (SearchSource& source : job->sources) {
            if (job->Stopped()) break;
            ++job->scanned;
            if (job->matcher.options().target != SearchTarget::Content) {
                std::pair<size_t, size_t> match;
                const std::wstring subject = job->matcher.options().target == SearchTarget::FileName
                    ? source.display : (source.path.empty() ? source.display : source.path);
                if (!job->matcher.MatchName(subject, match)) continue;
                auto result = std::make_unique<SearchFileResult>();
                result->path = source.path;
                result->display = source.display;
                result->editor = source.editor;
                SearchHit hit;
                hit.offset = match.first;
                hit.length = match.second;
                hit.lineText = subject;
                hit.lineMatchStart = match.first;
                result->hits.push_back(std::move(hit));
                PublishResult(*job, std::move(result));
                continue;
            }
            auto result = std::make_unique<SearchFileResult>();
            result->path = source.path;
            result->display = source.display;
            result->editor = source.editor;
            SearchText(*job, std::move(result), source.text);
        }
    } else {
        std::atomic<size_t> next{0};
        const unsigned threads = std::clamp(std::thread::hardware_concurrency(), 2u, 8u);
        std::vector<HANDLE> handles;
        for (unsigned i = 0; i < threads; ++i) {
            HANDLE handle = StartSearchThread([&] {
                for (size_t index; !job->Stopped() && (index = next.fetch_add(1)) < files.size();) {
                    try {
                        SearchOneFile(*job, files[index]);
                    } catch (...) {
                        job->AddError(files[index].path, L"处理失败");
                    }
                }
            });
            if (handle) handles.push_back(handle);
        }
        if (handles.empty()) {  // could not start threads: do it here
            for (const SearchFileEntry& entry : files) {
                if (job->Stopped()) break;
                SearchOneFile(*job, entry);
            }
        }
        if (!handles.empty()) WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), TRUE, INFINITE);
        for (HANDLE handle : handles) CloseHandle(handle);
    }
    job->elapsed = GetTickCount64() - job->started;
    PostMessageW(job->notify, kMessageSearchDone, static_cast<WPARAM>(job->generation), 0);
    SetEvent(job->finished);
}

