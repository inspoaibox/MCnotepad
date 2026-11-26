// ===========================================================================
// Compare: two files side by side, and two folders
// ===========================================================================

// A block that exists on one side only can often sit in several places
// ("}" + blank line ambiguity). Like git: move it down as far as it goes,
// then back up while it ends with a blank line, so it reads as whole lines
// of code being added or removed.
void SlideOneSidedBlocks(std::vector<std::pair<int, int>>& pairs, const std::vector<std::wstring>& a, const std::vector<std::wstring>& b) {
    const auto matched = [&](size_t k) { return pairs[k].first >= 0 && pairs[k].second >= 0; };
    const auto blank = [](const std::wstring& line) {
        return std::all_of(line.begin(), line.end(), [](wchar_t c) { return iswspace(c) != 0; });
    };
    size_t s = 0;
    while (s < pairs.size()) {
        if (matched(s)) { ++s; continue; }
        size_t e = s;
        while (e < pairs.size() && !matched(e)) ++e;
        const bool left = pairs[s].second < 0;
        bool oneSided = true;
        for (size_t k = s; k < e; ++k) oneSided = oneSided && ((pairs[k].second < 0) == left);
        if (!oneSided) { s = e; continue; }
        const std::vector<std::wstring>& lines = left ? a : b;
        const auto line = [&](size_t k) -> int { return left ? pairs[k].first : pairs[k].second; };
        const auto other = [&](size_t k) -> int { return left ? pairs[k].second : pairs[k].first; };
        const auto set = [&](size_t k, int here, int there) { pairs[k] = left ? std::make_pair(here, there) : std::make_pair(there, here); };
        const size_t length = e - s;
        // Down.
        while (e < pairs.size() && matched(e) && lines[static_cast<size_t>(line(s))] == lines[static_cast<size_t>(line(e))]) {
            const int first = line(s), there = other(e);
            set(s, first, there);
            for (size_t k = 1; k <= length; ++k) set(s + k, first + static_cast<int>(k), -1);
            ++s;
            ++e;
        }
        // Back up while the block ends with a blank line.
        while (s > 0 && matched(s - 1) && blank(lines[static_cast<size_t>(line(e - 1))]) &&
               lines[static_cast<size_t>(line(e - 1))] == lines[static_cast<size_t>(line(s - 1))]) {
            const int first = line(s - 1), there = other(s - 1);
            for (size_t k = 0; k < length; ++k) set(s - 1 + k, first + static_cast<int>(k), -1);
            set(e - 1, first + static_cast<int>(length), there);
            --s;
            --e;
        }
        s = e;
    }
}

// Pairs up lines of two texts: (leftIndex, rightIndex), -1 where a line
// exists on one side only. Common prefix/suffix first, then LCS on the rest.
std::vector<std::pair<int, int>> AlignLines(const std::vector<std::wstring>& a, const std::vector<std::wstring>& b) {
    std::vector<std::pair<int, int>> out;
    const size_t n = a.size(), m = b.size();
    size_t prefix = 0;
    while (prefix < n && prefix < m && a[prefix] == b[prefix]) ++prefix;
    size_t suffix = 0;
    while (suffix < n - prefix && suffix < m - prefix && a[n - 1 - suffix] == b[m - 1 - suffix]) ++suffix;
    for (size_t i = 0; i < prefix; ++i) out.emplace_back(static_cast<int>(i), static_cast<int>(i));
    const size_t rows = n - prefix - suffix, cols = m - prefix - suffix;
    if (rows > 0 && cols > 0 && rows * cols <= 16'000'000 && std::min(rows, cols) < 65535) {
        std::vector<uint16_t> table((rows + 1) * (cols + 1), 0);
        const auto at = [&](size_t i, size_t j) -> uint16_t& { return table[i * (cols + 1) + j]; };
        for (size_t i = rows; i-- > 0;) {
            for (size_t j = cols; j-- > 0;) {
                at(i, j) = a[prefix + i] == b[prefix + j] ? static_cast<uint16_t>(at(i + 1, j + 1) + 1) : std::max(at(i + 1, j), at(i, j + 1));
            }
        }
        size_t i = 0, j = 0;
        while (i < rows || j < cols) {
            if (i < rows && j < cols && a[prefix + i] == b[prefix + j]) {
                out.emplace_back(static_cast<int>(prefix + i++), static_cast<int>(prefix + j++));
            } else if (i < rows && (j == cols || at(i + 1, j) >= at(i, j + 1))) {
                out.emplace_back(static_cast<int>(prefix + i++), -1);
            } else {
                out.emplace_back(-1, static_cast<int>(prefix + j++));
            }
        }
    } else {
        for (size_t i = 0; i < rows; ++i) out.emplace_back(static_cast<int>(prefix + i), -1);
        for (size_t j = 0; j < cols; ++j) out.emplace_back(-1, static_cast<int>(prefix + j));
    }
    for (size_t k = 0; k < suffix; ++k) out.emplace_back(static_cast<int>(n - suffix + k), static_cast<int>(m - suffix + k));
    SlideOneSidedBlocks(out, a, b);
    return out;
}

std::wstring WhitespaceKey(const std::wstring& line) {
    std::wstring key;
    for (const wchar_t c : line) if (!iswspace(c)) key += c;
    return key;
}

enum class RowKind { Same, Changed, LeftOnly, RightOnly };

struct CompareRow {
    int left = -1;
    int right = -1;
    RowKind kind = RowKind::Same;
};

struct CompareHunk {
    int firstRow = 0, lastRow = 0;     // rows in the aligned view
    int leftStart = 0, leftCount = 0;  // line range on each side
    int rightStart = 0, rightCount = 0;
};

struct CompareSide {
    std::wstring path;       // empty for an untitled tab
    std::wstring title;
    HWND editor = nullptr;   // the tab this side came from, if any
    std::vector<std::wstring> lines;
    int encoding = 0;
    std::wstring eol = L"\r\n";
    bool dirty = false;
    uint64_t sourceHash = 0;       // tab text when loaded, to notice later edits there
    uint64_t sourceTimestamp = 0;  // file time when loaded, to notice changes on disk
};

// Rows of the aligned view: changed pairs first within each block of edits.
std::vector<CompareRow> BuildCompareRows(const CompareSide& left, const CompareSide& right, bool ignoreWhitespace,
    std::vector<CompareHunk>& hunks) {
    std::vector<std::wstring> keysLeft = left.lines, keysRight = right.lines;
    if (ignoreWhitespace) {
        for (std::wstring& line : keysLeft) line = WhitespaceKey(line);
        for (std::wstring& line : keysRight) line = WhitespaceKey(line);
    }
    const auto pairs = AlignLines(keysLeft, keysRight);
    std::vector<CompareRow> rows;
    hunks.clear();
    int nextLeft = 0, nextRight = 0;
    for (size_t k = 0; k < pairs.size();) {
        if (pairs[k].first >= 0 && pairs[k].second >= 0) {
            rows.push_back({pairs[k].first, pairs[k].second, RowKind::Same});
            nextLeft = pairs[k].first + 1;
            nextRight = pairs[k].second + 1;
            ++k;
            continue;
        }
        std::vector<int> removed, added;
        while (k < pairs.size() && !(pairs[k].first >= 0 && pairs[k].second >= 0)) {
            if (pairs[k].first >= 0) removed.push_back(pairs[k].first);
            else added.push_back(pairs[k].second);
            ++k;
        }
        CompareHunk hunk;
        hunk.firstRow = static_cast<int>(rows.size());
        hunk.leftStart = removed.empty() ? nextLeft : removed.front();
        hunk.leftCount = static_cast<int>(removed.size());
        hunk.rightStart = added.empty() ? nextRight : added.front();
        hunk.rightCount = static_cast<int>(added.size());
        const size_t paired = std::min(removed.size(), added.size());
        for (size_t p = 0; p < paired; ++p) rows.push_back({removed[p], added[p], RowKind::Changed});
        for (size_t p = paired; p < removed.size(); ++p) rows.push_back({removed[p], -1, RowKind::LeftOnly});
        for (size_t p = paired; p < added.size(); ++p) rows.push_back({-1, added[p], RowKind::RightOnly});
        hunk.lastRow = static_cast<int>(rows.size()) - 1;
        hunks.push_back(hunk);
        if (!removed.empty()) nextLeft = removed.back() + 1;
        if (!added.empty()) nextRight = added.back() + 1;
    }
    return rows;
}

bool LoadCompareSideFromFile(CompareSide& side, const std::wstring& path, std::wstring& error) {
    std::vector<char> bytes;
    if (!ReadBinaryFile(path, bytes)) {
        error = L"无法读取：" + path;
        return false;
    }
    const bool utf16Bom = bytes.size() >= 2 && ((static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) ||
        (static_cast<unsigned char>(bytes[0]) == 0xFE && static_cast<unsigned char>(bytes[1]) == 0xFF));
    if (!utf16Bom && memchr(bytes.data(), 0, std::min<size_t>(bytes.size(), 8192))) {
        error = L"二进制文件无法按文本比较：" + path;
        return false;
    }
    bool mixed = false;
    const std::wstring text = NormalizeToLf(DecodeTextBytes(bytes, &side.encoding), side.eol, mixed);
    side.lines = SplitLines(text);
    side.path = path;
    side.title = fs::path(path).filename().wstring();
    side.editor = nullptr;
    side.dirty = false;
    side.sourceTimestamp = FileTimestamp(path);
    return true;
}

void LoadCompareSideFromTab(CompareSide& side, const TabDocument& document) {
    const std::wstring text = GetEditorRawText(document.editor);
    side.sourceHash = HashText(text);
    side.lines = SplitLines(text);
    side.path = document.untitled ? std::wstring() : document.path;
    side.title = document.title + (document.modified ? L"（未保存的修改）" : L"");
    side.editor = document.editor;
    side.encoding = document.encoding;
    side.eol = document.lineEnding == 1 ? L"\n" : document.lineEnding == 2 ? L"\r" : L"\r\n";
    side.dirty = false;
}

// ---- File compare window -------------------------------------------------------

enum CompareControl : int {
    kCmpPrev = 4001, kCmpNext, kCmpToRight, kCmpToLeft, kCmpSwap, kCmpIgnoreSpace, kCmpSaveLeft, kCmpSaveRight, kCmpRefresh,
    kCmpLeftView, kCmpRightView, kCmpStats, kCmpLeftTitle, kCmpRightTitle, kCmpStructure, kCmpList,
};

// ---- Structured compare: JSON / CSV / YAML ------------------------------------------
// Each format is flattened to items addressed by a path ("server.port",
// "users[2].name", "row-key › column"). Items are compared one by one and
// any item can be taken from the other side.

enum class StructFormat { None, Json, Csv, Yaml };

struct StructItem {
    std::wstring path;
    std::wstring parent;   // path of the parent item ("" = root)
    std::wstring value;    // what is shown and compared (leaves)
    bool container = false;
};

class StructDoc {
public:
    virtual ~StructDoc() = default;
    virtual bool Parse(const std::vector<std::wstring>& lines, std::wstring& error) = 0;
    // Every item in document order, parents before their children.
    virtual std::vector<StructItem> Items() const = 0;
    // Makes `path` here match `from`: replace, insert or delete.
    virtual bool Take(const std::wstring& path, const StructDoc& from, std::wstring& error) = 0;
    virtual std::vector<std::wstring> Lines() const = 0;
    // True when writing this side back loses something (JSON comments).
    virtual bool LosesComments() const { return false; }
};

// ---- JSON (comments allowed, numbers and strings kept as written) ----

struct JsonNode {
    enum Type { Scalar, Object, Array } type = Scalar;
    std::wstring raw;  // scalars: exactly as written ("text", 1.50, true, null)
    std::vector<std::pair<std::wstring, JsonNode>> members;  // key as written, with quotes
    std::vector<JsonNode> items;
};

class JsonDoc : public StructDoc {
public:
    bool Parse(const std::vector<std::wstring>& lines, std::wstring& error) override {
        text_ = JoinLines(lines, L"\n");
        position_ = 0;
        indent_ = L"  ";
        compact_ = text_.find(L'\n') == std::wstring::npos || TrimCopy(text_).find(L'\n') == std::wstring::npos;
        for (const std::wstring& line : lines) {
            const size_t first = line.find_first_not_of(L" \t");
            if (first != std::wstring::npos && first > 0) { indent_ = line.substr(0, first); break; }
        }
        trailingNewline_ = !lines.empty() && lines.back().empty();
        SkipSpace();
        if (position_ >= text_.size()) { root_ = JsonNode{}; root_.type = JsonNode::Object; return true; }
        if (!ParseValue(root_, error)) return false;
        SkipSpace();
        if (position_ < text_.size()) { error = L"JSON 第 " + std::to_wstring(LineAt(position_)) + L" 行：多余的内容"; return false; }
        text_.clear();
        return true;
    }

    std::vector<StructItem> Items() const override {
        std::vector<StructItem> out;
        Collect(root_, L"", L"", out, true);
        return out;
    }

    bool Take(const std::wstring& path, const StructDoc& fromBase, std::wstring& error) override {
        const auto& from = static_cast<const JsonDoc&>(fromBase);
        Location source = from.Find(path);
        Location target = Find(path);
        if (source.node && target.node) {
            *target.node = *source.node;
            return true;
        }
        if (!source.node && target.node) {
            if (target.parent->type == JsonNode::Object) target.parent->members.erase(target.parent->members.begin() + static_cast<std::ptrdiff_t>(target.index));
            else target.parent->items.erase(target.parent->items.begin() + static_cast<std::ptrdiff_t>(target.index));
            return true;
        }
        if (!source.node) return true;
        Location parent = source.parentPath.empty() ? Location{&root_, nullptr, 0, L""} : Find(source.parentPath);
        if (!parent.node || parent.node->type != source.parent->type) {
            error = L"另一侧没有“" + source.parentPath + L"”这个对象，无法插入";
            return false;
        }
        if (source.parent->type == JsonNode::Array) {
            const size_t at = std::min(source.index, parent.node->items.size());
            parent.node->items.insert(parent.node->items.begin() + static_cast<std::ptrdiff_t>(at), *source.node);
            return true;
        }
        // After the nearest earlier key that exists here, else first.
        size_t at = 0;
        for (size_t k = source.index; k-- > 0;) {
            const std::wstring& key = source.parent->members[k].first;
            const auto it = std::find_if(parent.node->members.begin(), parent.node->members.end(), [&](const auto& m) { return m.first == key; });
            if (it != parent.node->members.end()) { at = static_cast<size_t>(it - parent.node->members.begin()) + 1; break; }
        }
        parent.node->members.insert(parent.node->members.begin() + static_cast<std::ptrdiff_t>(at), {source.parent->members[source.index].first, *source.node});
        return true;
    }

    std::vector<std::wstring> Lines() const override {
        std::wstring out;
        Write(root_, 0, out);
        std::vector<std::wstring> lines = SplitLines(out);
        if (trailingNewline_) lines.emplace_back();
        return lines;
    }

private:
    struct Location {
        JsonNode* node = nullptr;
        JsonNode* parent = nullptr;
        size_t index = 0;
        std::wstring parentPath;
    };

    static std::wstring KeyName(const std::wstring& raw) {
        return raw.size() >= 2 && raw.front() == L'"' ? raw.substr(1, raw.size() - 2) : raw;
    }
    static std::wstring ChildPath(const std::wstring& parent, const std::wstring& key) {
        return parent.empty() ? key : parent + L"." + key;
    }

    void Collect(const JsonNode& node, const std::wstring& path, const std::wstring& parent, std::vector<StructItem>& out, bool root) const {
        if (!root) {
            StructItem item{path, parent, {}, node.type != JsonNode::Scalar};
            if (node.type == JsonNode::Scalar) item.value = node.raw;
            else if (node.type == JsonNode::Object) item.value = L"{ " + std::to_wstring(node.members.size()) + L" 个键 }";
            else item.value = L"[ " + std::to_wstring(node.items.size()) + L" 项 ]";
            out.push_back(item);
        }
        if (node.type == JsonNode::Object) {
            for (const auto& [key, child] : node.members) Collect(child, ChildPath(path, KeyName(key)), path, out, false);
        } else if (node.type == JsonNode::Array) {
            for (size_t i = 0; i < node.items.size(); ++i) Collect(node.items[i], path + L"[" + std::to_wstring(i) + L"]", path, out, false);
        }
    }

    // Walks the tree; paths are the ones Collect produces.
    Location Find(const std::wstring& path) const {
        Location found;
        Search(const_cast<JsonNode&>(root_), L"", path, found);
        return found;
    }
    static bool Search(JsonNode& node, const std::wstring& path, const std::wstring& wanted, Location& found) {
        if (node.type == JsonNode::Object) {
            for (size_t k = 0; k < node.members.size(); ++k) {
                const std::wstring child = ChildPath(path, KeyName(node.members[k].first));
                if (child == wanted) { found = {&node.members[k].second, &node, k, path}; return true; }
                if (wanted.compare(0, child.size(), child) == 0 && wanted.size() > child.size() &&
                    (wanted[child.size()] == L'.' || wanted[child.size()] == L'[') && Search(node.members[k].second, child, wanted, found)) return true;
            }
        } else if (node.type == JsonNode::Array) {
            for (size_t k = 0; k < node.items.size(); ++k) {
                const std::wstring child = path + L"[" + std::to_wstring(k) + L"]";
                if (child == wanted) { found = {&node.items[k], &node, k, path}; return true; }
                if (wanted.compare(0, child.size(), child) == 0 && wanted.size() > child.size() && Search(node.items[k], child, wanted, found)) return true;
            }
        }
        return false;
    }

    int LineAt(size_t position) const { return 1 + static_cast<int>(std::count(text_.begin(), text_.begin() + static_cast<std::ptrdiff_t>(std::min(position, text_.size())), L'\n')); }

    void SkipSpace() {
        while (position_ < text_.size()) {
            const wchar_t c = text_[position_];
            if (iswspace(c) || c == 0xFEFF) { ++position_; continue; }
            if (c == L'/' && position_ + 1 < text_.size() && text_[position_ + 1] == L'/') {
                hasComments_ = true;
                while (position_ < text_.size() && text_[position_] != L'\n') ++position_;
                continue;
            }
            if (c == L'/' && position_ + 1 < text_.size() && text_[position_ + 1] == L'*') {
                hasComments_ = true;
                const size_t end = text_.find(L"*/", position_ + 2);
                position_ = end == std::wstring::npos ? text_.size() : end + 2;
                continue;
            }
            break;
        }
    }

    bool ParseString(std::wstring& raw, std::wstring& error) {
        const size_t start = position_++;
        while (position_ < text_.size() && text_[position_] != L'"') {
            if (text_[position_] == L'\\') ++position_;
            ++position_;
        }
        if (position_ >= text_.size()) { error = L"JSON 第 " + std::to_wstring(LineAt(start)) + L" 行：字符串没有结束"; return false; }
        ++position_;
        raw = text_.substr(start, position_ - start);
        return true;
    }

    bool ParseValue(JsonNode& node, std::wstring& error) {
        SkipSpace();
        if (position_ >= text_.size()) { error = L"JSON 意外结束"; return false; }
        const wchar_t c = text_[position_];
        if (c == L'{' || c == L'[') {
            const bool object = c == L'{';
            node.type = object ? JsonNode::Object : JsonNode::Array;
            ++position_;
            SkipSpace();
            if (position_ < text_.size() && text_[position_] == (object ? L'}' : L']')) { ++position_; return true; }
            while (true) {
                SkipSpace();
                if (object) {
                    std::wstring key;
                    if (position_ >= text_.size() || text_[position_] != L'"') { error = L"JSON 第 " + std::to_wstring(LineAt(position_)) + L" 行：应为键名"; return false; }
                    if (!ParseString(key, error)) return false;
                    SkipSpace();
                    if (position_ >= text_.size() || text_[position_] != L':') { error = L"JSON 第 " + std::to_wstring(LineAt(position_)) + L" 行：缺少冒号"; return false; }
                    ++position_;
                    JsonNode child;
                    if (!ParseValue(child, error)) return false;
                    node.members.emplace_back(std::move(key), std::move(child));
                } else {
                    JsonNode child;
                    if (!ParseValue(child, error)) return false;
                    node.items.push_back(std::move(child));
                }
                SkipSpace();
                if (position_ < text_.size() && text_[position_] == L',') {
                    ++position_;
                    SkipSpace();
                    if (position_ < text_.size() && text_[position_] == (object ? L'}' : L']')) { ++position_; return true; }  // trailing comma
                    continue;
                }
                if (position_ < text_.size() && text_[position_] == (object ? L'}' : L']')) { ++position_; return true; }
                error = L"JSON 第 " + std::to_wstring(LineAt(position_)) + L" 行：缺少逗号或括号";
                return false;
            }
        }
        node.type = JsonNode::Scalar;
        if (c == L'"') return ParseString(node.raw, error);
        const size_t start = position_;
        while (position_ < text_.size() && !iswspace(text_[position_]) && wcschr(L",}]/", text_[position_]) == nullptr) ++position_;
        if (position_ == start) { error = L"JSON 第 " + std::to_wstring(LineAt(position_)) + L" 行：无法识别的字符"; return false; }
        node.raw = text_.substr(start, position_ - start);
        return true;
    }

    void Write(const JsonNode& node, int depth, std::wstring& out) const {
        if (node.type == JsonNode::Scalar) { out += node.raw; return; }
        const bool object = node.type == JsonNode::Object;
        const size_t count = object ? node.members.size() : node.items.size();
        if (count == 0) { out += object ? L"{}" : L"[]"; return; }
        out += object ? L'{' : L'[';
        for (size_t k = 0; k < count; ++k) {
            if (compact_) out += k ? L", " : L"";
            else {
                out += L'\n';
                for (int d = 0; d <= depth; ++d) out += indent_;
            }
            if (object) {
                out += node.members[k].first + L": ";
                Write(node.members[k].second, depth + 1, out);
            } else {
                Write(node.items[k], depth + 1, out);
            }
            if (!compact_ && k + 1 < count) out += L',';
        }
        if (!compact_) {
            out += L'\n';
            for (int d = 0; d < depth; ++d) out += indent_;
        }
        out += object ? L'}' : L']';
    }

    JsonNode root_;
    std::wstring text_;
    size_t position_ = 0;
    std::wstring indent_;
    bool compact_ = false;
    bool trailingNewline_ = false;
    bool hasComments_ = false;

public:
    bool LosesComments() const override { return hasComments_; }
};

// ---- CSV / TSV: rows matched by their first column ----

class CsvDoc : public StructDoc {
public:
    explicit CsvDoc(wchar_t delimiter = 0) : delimiter_(delimiter) {}

    bool Parse(const std::vector<std::wstring>& lines, std::wstring& error) override {
        const std::wstring text = JoinLines(lines, L"\n");
        trailingNewline_ = !lines.empty() && lines.back().empty();
        if (!delimiter_) {
            const std::wstring first = lines.empty() ? std::wstring() : lines.front();
            const size_t commas = std::count(first.begin(), first.end(), L','), tabs = std::count(first.begin(), first.end(), L'\t'),
                         semis = std::count(first.begin(), first.end(), L';');
            delimiter_ = tabs > commas && tabs >= semis ? L'\t' : semis > commas ? L';' : L',';
        }
        records_.clear();
        std::vector<std::wstring> record;
        std::wstring cell;
        bool quoted = false, any = false;
        for (size_t i = 0; i < text.size(); ++i) {
            const wchar_t c = text[i];
            if (quoted) {
                if (c == L'"' && i + 1 < text.size() && text[i + 1] == L'"') { cell += L'"'; ++i; }
                else if (c == L'"') quoted = false;
                else cell += c;
                continue;
            }
            if (c == L'"' && cell.empty()) { quoted = true; any = true; }
            else if (c == delimiter_) { record.push_back(cell); cell.clear(); any = true; }
            else if (c == L'\n') {
                record.push_back(cell);
                cell.clear();
                if (any || record.size() > 1 || !record.front().empty()) records_.push_back(record);
                record.clear();
                any = false;
            } else { cell += c; any = true; }
        }
        if (quoted) { error = L"CSV 中有未闭合的引号"; return false; }
        if (any || !cell.empty()) { record.push_back(cell); records_.push_back(record); }
        if (records_.empty()) { error = L"CSV 是空的"; return false; }
        return true;
    }

    std::vector<StructItem> Items() const override {
        std::vector<StructItem> out;
        const std::vector<std::wstring>& header = records_.front();
        const std::vector<std::wstring> keys = RowKeys();
        for (size_t r = 1; r < records_.size(); ++r) {
            const std::wstring& key = keys[r];
            std::wstring summary;
            for (size_t c = 0; c < records_[r].size(); ++c) summary += (c ? L", " : L"") + records_[r][c];
            out.push_back({key, L"", summary, true});
            for (size_t c = 1; c < header.size(); ++c) {
                if (c < records_[r].size() && !records_[r][c].empty()) out.push_back({key + L" › " + header[c], key, records_[r][c], false});
            }
        }
        return out;
    }

    bool Take(const std::wstring& path, const StructDoc& fromBase, std::wstring& error) override {
        const auto& from = static_cast<const CsvDoc&>(fromBase);
        const size_t split = path.find(L" › ");
        const std::wstring key = split == std::wstring::npos ? path : path.substr(0, split);
        const int sourceRow = from.RowOf(key), targetRow = RowOf(key);
        if (split != std::wstring::npos) {
            const std::wstring column = path.substr(split + 3);
            if (targetRow < 0) { error = L"本侧没有行“" + key + L"”"; return false; }
            int targetColumn = ColumnOf(column);
            if (targetColumn < 0) {
                records_.front().push_back(column);
                targetColumn = static_cast<int>(records_.front().size()) - 1;
            }
            std::vector<std::wstring>& row = records_[static_cast<size_t>(targetRow)];
            if (row.size() <= static_cast<size_t>(targetColumn)) row.resize(static_cast<size_t>(targetColumn) + 1);
            const int sourceColumn = from.ColumnOf(column);
            row[static_cast<size_t>(targetColumn)] = sourceRow >= 0 && sourceColumn >= 0 && static_cast<size_t>(sourceColumn) < from.records_[static_cast<size_t>(sourceRow)].size()
                ? from.records_[static_cast<size_t>(sourceRow)][static_cast<size_t>(sourceColumn)] : L"";
            // A column the other side does not have goes once it is empty.
            if (sourceColumn < 0) {
                bool empty = true;
                for (size_t r = 1; r < records_.size() && empty; ++r) {
                    empty = records_[r].size() <= static_cast<size_t>(targetColumn) || records_[r][static_cast<size_t>(targetColumn)].empty();
                }
                if (empty) {
                    for (auto& record : records_) {
                        if (record.size() > static_cast<size_t>(targetColumn)) record.erase(record.begin() + targetColumn);
                    }
                }
            }
            return true;
        }
        if (sourceRow < 0) {
            if (targetRow > 0) records_.erase(records_.begin() + targetRow);
            return true;
        }
        // Row in this side's column order.
        std::vector<std::wstring> row(records_.front().size());
        for (size_t c = 0; c < row.size(); ++c) {
            const int sourceColumn = c == 0 ? 0 : from.ColumnOf(records_.front()[c]);
            const auto& sourceRecord = from.records_[static_cast<size_t>(sourceRow)];
            if (sourceColumn >= 0 && static_cast<size_t>(sourceColumn) < sourceRecord.size()) row[c] = sourceRecord[static_cast<size_t>(sourceColumn)];
        }
        if (targetRow > 0) { records_[static_cast<size_t>(targetRow)] = row; return true; }
        size_t at = 1;
        const std::vector<std::wstring> sourceKeys = from.RowKeys();
        for (int r = sourceRow - 1; r >= 1; --r) {
            const int here = RowOf(sourceKeys[static_cast<size_t>(r)]);
            if (here > 0) { at = static_cast<size_t>(here) + 1; break; }
        }
        records_.insert(records_.begin() + static_cast<std::ptrdiff_t>(at), row);
        return true;
    }

    std::vector<std::wstring> Lines() const override {
        std::wstring text;
        for (const auto& record : records_) {
            for (size_t c = 0; c < record.size(); ++c) {
                if (c) text += delimiter_;
                const std::wstring& cell = record[c];
                const bool quote = cell.find_first_of(std::wstring(L"\"\r\n") + delimiter_) != std::wstring::npos ||
                    (!cell.empty() && (iswspace(cell.front()) || iswspace(cell.back())));
                if (!quote) { text += cell; continue; }
                text += L'"';
                for (const wchar_t ch : cell) { if (ch == L'"') text += L'"'; text += ch; }
                text += L'"';
            }
            text += L'\n';
        }
        if (!text.empty()) text.pop_back();
        std::vector<std::wstring> lines = SplitLines(text);
        if (trailingNewline_) lines.emplace_back();
        return lines;
    }

private:
    std::vector<std::wstring> RowKeys() const {
        std::vector<std::wstring> keys(records_.size());
        std::unordered_map<std::wstring, int> seen;
        for (size_t r = 1; r < records_.size(); ++r) {
            const std::wstring base = records_[r].empty() || records_[r][0].empty() ? L"(空)" : records_[r][0];
            const int count = ++seen[base];
            keys[r] = count == 1 ? base : base + L" #" + std::to_wstring(count);
        }
        return keys;
    }
    int RowOf(const std::wstring& key) const {
        const std::vector<std::wstring> keys = RowKeys();
        for (size_t r = 1; r < keys.size(); ++r) if (keys[r] == key) return static_cast<int>(r);
        return -1;
    }
    int ColumnOf(const std::wstring& name) const {
        const auto& header = records_.front();
        for (size_t c = 1; c < header.size(); ++c) if (header[c] == name) return static_cast<int>(c);
        return -1;
    }

    wchar_t delimiter_ = 0;
    std::vector<std::vector<std::wstring>> records_;
    bool trailingNewline_ = false;
};

// ---- YAML (block style): edits whole lines, so comments and layout survive ----

class YamlDoc : public StructDoc {
public:
    bool Parse(const std::vector<std::wstring>& lines, std::wstring& error) override {
        lines_ = lines;
        nodes_.clear();
        struct Open { int indent; std::wstring path; bool dash; };
        std::vector<Open> stack;
        std::unordered_map<std::wstring, int> sequenceCounters;
        int blockScalarIndent = -1;  // inside "key: |" / "key: >" text: lines deeper than this are text
        for (size_t i = 0; i < lines_.size(); ++i) {
            const std::wstring& line = lines_[i];
            const size_t first = line.find_first_not_of(L' ');
            if (blockScalarIndent >= 0) {
                if (first == std::wstring::npos || static_cast<int>(first) > blockScalarIndent) continue;
                blockScalarIndent = -1;
            }
            if (first == std::wstring::npos || line[first] == L'#' || line[first] == L'\t') continue;
            if (first == 0 && (line.compare(0, 3, L"---") == 0 || line.compare(0, 3, L"...") == 0)) {
                if (!nodes_.empty()) { error = L"暂不支持包含多个文档（---）的 YAML"; return false; }
                continue;
            }
            int indent = static_cast<int>(first);
            size_t column = first;
            const bool dash = line[first] == L'-' && (first + 1 == line.size() || line[first + 1] == L' ');
            if (dash) {
                while (!stack.empty() && (stack.back().indent > indent || (stack.back().indent == indent && stack.back().dash))) stack.pop_back();
                const std::wstring parent = stack.empty() ? L"" : stack.back().path;
                const int index = sequenceCounters[parent]++;
                Node item;
                item.path = parent + L"[" + std::to_wstring(index) + L"]";
                item.parent = parent;
                item.line = i;
                item.column = column;
                item.dash = true;
                stack.push_back({indent, item.path, true});
                size_t rest = line.find_first_not_of(L' ', first + 1);
                const size_t colon = rest == std::wstring::npos ? std::wstring::npos : KeyColon(line, rest);
                item.prefixEnd = first + 1;
                if (rest == std::wstring::npos || line[rest] == L'#') {
                    nodes_.push_back(item);
                    continue;
                }
                if (colon == std::wstring::npos) {  // "- scalar"
                    item.valueColumn = rest;
                    nodes_.push_back(item);
                    continue;
                }
                item.inlineKey = true;
                nodes_.push_back(item);
                indent = static_cast<int>(rest);
                column = rest;
                sequenceCounters.erase(item.path);
                AddKey(i, column, indent, colon, item.path, true, stack, sequenceCounters);
                if (StartsBlockScalar(nodes_.back())) blockScalarIndent = static_cast<int>(first);
                continue;
            }
            const size_t colon = KeyColon(line, first);
            if (colon == std::wstring::npos) continue;  // continuation of a multi-line value
            while (!stack.empty() && stack.back().indent >= indent) stack.pop_back();
            AddKey(i, column, indent, colon, stack.empty() ? L"" : stack.back().path, false, stack, sequenceCounters);
            if (StartsBlockScalar(nodes_.back())) blockScalarIndent = static_cast<int>(first);
        }
        // Block ends.
        for (Node& node : nodes_) {
            size_t end = node.line + 1;
            for (size_t j = node.line + 1; j < lines_.size(); ++j) {
                const size_t first = lines_[j].find_first_not_of(L' ');
                if (first == std::wstring::npos || lines_[j][first] == L'#') continue;
                const bool childDash = lines_[j][first] == L'-' && !node.dash && !node.onDashLine;
                if (static_cast<int>(first) > static_cast<int>(node.column) || (childDash && first == node.column)) end = j + 1;
                else break;
            }
            node.end = end;
            // A value that starts on the key's line ("key: text", "key: |")
            // is one value even when it continues over more lines.
            node.container = node.inlineKey || (node.valueColumn == std::wstring::npos && end > node.line + 1);
        }
        return true;
    }

    std::vector<StructItem> Items() const override {
        std::vector<StructItem> out;
        for (const Node& node : nodes_) {
            StructItem item{node.path, node.parent, {}, node.container};
            item.value = node.container ? L"(" + std::to_wstring(node.end - node.line) + L" 行)" : Value(node);
            out.push_back(item);
        }
        return out;
    }

    bool Take(const std::wstring& path, const StructDoc& fromBase, std::wstring& error) override {
        const auto& from = static_cast<const YamlDoc&>(fromBase);
        const Node* source = from.Find(path);
        const Node* target = Find(path);
        const std::wstring unsupported = L"“" + path + L"”写在“- ”同一行，无法自动合并，请在文本视图中手动修改";
        if (source && target) {
            if (!source->container && !target->container && source->end == source->line + 1 && target->end == target->line + 1) {
                std::wstring& line = lines_[target->line];
                const std::wstring value = source->valueColumn == std::wstring::npos ? L"" : from.lines_[source->line].substr(source->valueColumn);
                line = line.substr(0, target->prefixEnd) + (value.empty() ? L"" : L" " + value);
            } else {
                if (source->onDashLine || target->onDashLine) { error = unsupported; return false; }
                const std::vector<std::wstring> block = from.Block(*source, static_cast<int>(target->column) - static_cast<int>(source->column));
                ReplaceLines(target->line, target->end, block);
            }
        } else if (target) {
            if (target->onDashLine) { error = unsupported; return false; }
            ReplaceLines(target->line, target->end, {});
        } else if (source) {
            if (source->onDashLine) { error = unsupported; return false; }
            const Node* parent = source->parent.empty() ? nullptr : Find(source->parent);
            if (!source->parent.empty() && !parent) { error = L"本侧没有“" + source->parent + L"”"; return false; }
            const Node* sourceParent = source->parent.empty() ? nullptr : from.Find(source->parent);
            // After the nearest earlier sibling that exists here.
            const Node* previous = nullptr;
            for (const Node& node : from.nodes_) {
                if (&node == source) break;
                if (node.parent == source->parent) {
                    if (const Node* here = Find(node.path)) previous = here;
                }
            }
            size_t at;
            int column;
            if (previous) { at = previous->end; column = static_cast<int>(previous->column); }
            else if (parent) {
                at = parent->line + 1;
                column = static_cast<int>(parent->column) + (static_cast<int>(source->column) - static_cast<int>(sourceParent ? sourceParent->column : 0));
            } else {
                at = 0;
                while (at < lines_.size() && (TrimCopy(lines_[at]).empty() || TrimCopy(lines_[at])[0] == L'#' || lines_[at].compare(0, 3, L"---") == 0)) ++at;
                column = 0;
            }
            if (parent && !parent->container && parent->valueColumn != std::wstring::npos) {
                error = L"本侧的“" + source->parent + L"”是一个值，不是对象或列表";
                return false;
            }
            const std::vector<std::wstring> block = from.Block(*source, column - static_cast<int>(source->column));
            lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(at), block.begin(), block.end());
        }
        std::wstring parseError;
        if (!Parse(std::vector<std::wstring>(lines_), parseError)) { error = parseError; return false; }
        return true;
    }

    std::vector<std::wstring> Lines() const override { return lines_; }

private:
    struct Node {
        std::wstring path, parent;
        size_t line = 0, end = 0;
        size_t column = 0;                         // where the key or "-" starts
        size_t prefixEnd = 0;                      // just after "key:" or "-"
        size_t valueColumn = std::wstring::npos;   // where a scalar value starts
        bool dash = false, onDashLine = false, inlineKey = false, container = false;
    };

    // Colon ending a key ("key: value" or "key:"), outside quotes.
    static size_t KeyColon(const std::wstring& line, size_t from) {
        wchar_t quote = 0;
        for (size_t i = from; i < line.size(); ++i) {
            const wchar_t c = line[i];
            if (quote) { if (c == quote) quote = 0; continue; }
            if ((c == L'"' || c == L'\'') && i == from) { quote = c; continue; }
            if (c == L'#' && i > from && line[i - 1] == L' ') return std::wstring::npos;
            if (c == L':' && (i + 1 == line.size() || line[i + 1] == L' ')) return i;
            if (i == from && (c == L'[' || c == L'{' || c == L'|' || c == L'>')) return std::wstring::npos;
        }
        return std::wstring::npos;
    }

    template <typename Stack>
    void AddKey(size_t i, size_t column, int indent, size_t colon, const std::wstring& parent, bool onDash, Stack& stack,
        std::unordered_map<std::wstring, int>& counters) {
        const std::wstring& line = lines_[i];
        std::wstring key = TrimCopy(std::wstring_view(line).substr(column, colon - column));
        if (key.size() >= 2 && (key.front() == L'"' || key.front() == L'\'') && key.back() == key.front()) key = key.substr(1, key.size() - 2);
        Node node;
        node.path = parent.empty() ? key : parent + L"." + key;
        node.parent = parent;
        node.line = i;
        node.column = column;
        node.onDashLine = onDash;
        const size_t value = line.find_first_not_of(L' ', colon + 1);
        node.prefixEnd = colon + 1;
        node.valueColumn = value == std::wstring::npos || line[value] == L'#' ? std::wstring::npos : value;
        counters.erase(node.path);
        nodes_.push_back(node);
        stack.push_back({indent, node.path, false});
    }

    bool StartsBlockScalar(const Node& node) const {
        if (node.valueColumn == std::wstring::npos) return false;
        const wchar_t c = lines_[node.line][node.valueColumn];
        return c == L'|' || c == L'>';
    }

    std::wstring Value(const Node& node) const {
        if (node.valueColumn == std::wstring::npos) return L"";
        if (!node.container && node.end > node.line + 1) {  // multi-line value: all of it
            std::wstring value = TrimCopy(lines_[node.line].substr(node.valueColumn));
            for (size_t j = node.line + 1; j < node.end; ++j) value += L" ⏎ " + TrimCopy(lines_[j]);
            return value;
        }
        std::wstring value = lines_[node.line].substr(node.valueColumn);
        const size_t comment = value.find(L" #");
        if (comment != std::wstring::npos && value[0] != L'"' && value[0] != L'\'') value.erase(comment);
        return TrimCopy(value);
    }

    const Node* Find(const std::wstring& path) const {
        for (const Node& node : nodes_) if (node.path == path) return &node;
        return nullptr;
    }

    std::vector<std::wstring> Block(const Node& node, int shift) const {
        std::vector<std::wstring> block(lines_.begin() + static_cast<std::ptrdiff_t>(node.line), lines_.begin() + static_cast<std::ptrdiff_t>(node.end));
        for (std::wstring& line : block) {
            if (shift > 0) line.insert(0, static_cast<size_t>(shift), L' ');
            else if (shift < 0) {
                const size_t spaces = std::min(static_cast<size_t>(-shift), line.find_first_not_of(L' ') == std::wstring::npos ? line.size() : line.find_first_not_of(L' '));
                line.erase(0, spaces);
            }
        }
        return block;
    }

    void ReplaceLines(size_t start, size_t end, const std::vector<std::wstring>& block) {
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(start), lines_.begin() + static_cast<std::ptrdiff_t>(end));
        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(start), block.begin(), block.end());
    }

    std::vector<std::wstring> lines_;
    std::vector<Node> nodes_;
};

StructFormat DetectStructFormat(const CompareSide& side) {
    const std::wstring name = LowerInvariantSyntax(side.path.empty() ? side.title : side.path);
    const auto ends = [&](const wchar_t* suffix) {
        const size_t n = wcslen(suffix);
        return name.size() >= n && name.compare(name.size() - n, n, suffix) == 0;
    };
    if (ends(L".json") || ends(L".jsonc") || ends(L".json5") || ends(L".geojson")) return StructFormat::Json;
    if (ends(L".csv") || ends(L".tsv")) return StructFormat::Csv;
    if (ends(L".yaml") || ends(L".yml")) return StructFormat::Yaml;
    for (const std::wstring& line : side.lines) {
        const std::wstring trimmed = TrimCopy(line);
        if (trimmed.empty()) continue;
        if (trimmed[0] == L'{' || trimmed[0] == L'[') return StructFormat::Json;
        break;
    }
    return StructFormat::None;
}

std::unique_ptr<StructDoc> MakeStructDoc(StructFormat format, const CompareSide& side) {
    if (format == StructFormat::Json) return std::make_unique<JsonDoc>();
    if (format == StructFormat::Yaml) return std::make_unique<YamlDoc>();
    if (format == StructFormat::Csv) {
        const std::wstring name = LowerInvariantSyntax(side.path.empty() ? side.title : side.path);
        return std::make_unique<CsvDoc>(name.size() > 4 && name.compare(name.size() - 4, 4, L".tsv") == 0 ? L'\t' : wchar_t{0});
    }
    return nullptr;
}

struct StructRow {
    std::wstring path;
    RowKind kind = RowKind::Same;
    std::wstring left, right;
};

// Compares item by item; a subtree that exists on one side only is one row.
std::vector<StructRow> CompareStructItems(const std::vector<StructItem>& left, const std::vector<StructItem>& right) {
    std::unordered_map<std::wstring, size_t> rightIndex, leftIndex;
    for (size_t i = 0; i < right.size(); ++i) rightIndex.emplace(right[i].path, i);
    for (size_t i = 0; i < left.size(); ++i) leftIndex.emplace(left[i].path, i);
    std::vector<StructRow> rows;
    std::unordered_set<std::wstring> covered;  // reported as a whole subtree
    std::unordered_map<std::wstring, size_t> rowOf;
    for (const StructItem& item : left) {
        if (covered.count(item.parent)) { covered.insert(item.path); continue; }
        const auto other = rightIndex.find(item.path);
        if (other == rightIndex.end()) {
            rows.push_back({item.path, RowKind::LeftOnly, item.value, L""});
            covered.insert(item.path);
        } else {
            const StructItem& peer = right[other->second];
            if (item.container && peer.container) {
                rows.push_back({item.path, RowKind::Same, item.value, peer.value});
            } else if (item.container != peer.container || item.value != peer.value) {
                rows.push_back({item.path, RowKind::Changed, item.value, peer.value});
                if (item.container || peer.container) covered.insert(item.path);
            } else {
                rows.push_back({item.path, RowKind::Same, item.value, peer.value});
            }
        }
        rowOf[item.path] = rows.size() - 1;
    }
    // Right-only items go after the row of the item before them on the right.
    // End of each row's subtree (rows are in document order, parents first).
    std::vector<size_t> subtreeEnd(rows.size(), rows.size());
    {
        std::unordered_map<std::wstring, std::wstring> parentOf;
        for (const StructItem& item : left) parentOf.emplace(item.path, item.parent);
        std::vector<size_t> open;
        for (size_t r = 0; r < rows.size(); ++r) {
            const std::wstring& parent = parentOf[rows[r].path];
            while (!open.empty() && rows[open.back()].path != parent) {
                subtreeEnd[open.back()] = r;
                open.pop_back();
            }
            open.push_back(r);
        }
    }
    std::vector<std::pair<size_t, StructRow>> inserts;
    std::unordered_map<std::wstring, std::wstring> lastShared;  // parent -> last child present on both sides
    std::unordered_set<std::wstring> coveredRight;
    for (const StructItem& item : right) {
        if (leftIndex.count(item.path)) {
            if (covered.count(item.path) || coveredRight.count(item.parent)) coveredRight.insert(item.path);
            if (rowOf.count(item.path)) lastShared[item.parent] = item.path;
            continue;
        }
        if (coveredRight.count(item.parent) || covered.count(item.parent)) { coveredRight.insert(item.path); continue; }
        size_t at = 0;
        const auto sibling = lastShared.find(item.parent);
        if (sibling != lastShared.end()) at = subtreeEnd[rowOf[sibling->second]];
        else if (const auto parent = rowOf.find(item.parent); parent != rowOf.end()) at = parent->second + 1;
        inserts.push_back({at, {item.path, RowKind::RightOnly, L"", item.value}});
        coveredRight.insert(item.path);
    }
    // Stable insertion, keeping right-side order for the same anchor.
    std::stable_sort(inserts.begin(), inserts.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<StructRow> merged;
    merged.reserve(rows.size() + inserts.size());
    size_t next = 0;
    for (size_t r = 0; r <= rows.size(); ++r) {
        while (next < inserts.size() && inserts[next].first == r) merged.push_back(inserts[next++].second);
        if (r < rows.size()) merged.push_back(rows[r]);
    }
    while (next < inserts.size()) merged.push_back(inserts[next++].second);
    // Containers present on both sides are only headings; drop identical ones.
    merged.erase(std::remove_if(merged.begin(), merged.end(), [](const StructRow& row) { return row.kind == RowKind::Same; }), merged.end());
    return merged;
}

struct CompareWindow {
    HWND window = nullptr;
    HWND left = nullptr, right = nullptr, stats = nullptr, leftTitle = nullptr, rightTitle = nullptr;
    CompareSide sides[2];
    std::vector<CompareRow> rows;
    std::vector<CompareHunk> hunks;
    int current = -1;          // current hunk
    bool ignoreWhitespace = false;
    bool syncing = false;
    // Structured view (JSON / CSV / YAML)
    bool structMode = false;
    HWND list = nullptr;
    StructFormat format = StructFormat::None;
    std::unique_ptr<StructDoc> docs[2];
    std::vector<StructRow> structRows;
};

bool BuildStructure(CompareWindow& compare, bool showErrors);

std::unordered_map<HWND, WNDPROC> g_compareViewProcs;

CompareWindow* CompareFromChild(HWND child) {
    return reinterpret_cast<CompareWindow*>(GetWindowLongPtrW(GetParent(child), GWLP_USERDATA));
}

// Keeps the two panes on the same row: rows are aligned, so the first
// visible line is shared.
void SyncCompareScroll(CompareWindow& compare, HWND source) {
    if (compare.syncing) return;
    compare.syncing = true;
    HWND other = source == compare.left ? compare.right : compare.left;
    const LRESULT first = SendMessageW(source, EM_GETFIRSTVISIBLELINE, 0, 0);
    const LRESULT otherFirst = SendMessageW(other, EM_GETFIRSTVISIBLELINE, 0, 0);
    if (first != otherFirst) SendMessageW(other, EM_LINESCROLL, 0, first - otherFirst);
    POINT scroll{};
    SendMessageW(source, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    POINT otherScroll{};
    SendMessageW(other, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&otherScroll));
    otherScroll.x = scroll.x;
    SendMessageW(other, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&otherScroll));
    compare.syncing = false;
}

LRESULT CALLBACK CompareViewProc(HWND view, UINT message, WPARAM wParam, LPARAM lParam) {
    const auto it = g_compareViewProcs.find(view);
    const WNDPROC old = it != g_compareViewProcs.end() ? it->second : DefWindowProcW;
    const LRESULT result = CallWindowProcW(old, view, message, wParam, lParam);
    if (message == WM_VSCROLL || message == WM_HSCROLL || message == WM_MOUSEWHEEL || message == WM_KEYDOWN || message == WM_MOUSEHWHEEL) {
        if (CompareWindow* compare = CompareFromChild(view)) SyncCompareScroll(*compare, view);
    }
    if (message == WM_NCDESTROY) g_compareViewProcs.erase(view);
    return result;
}

std::wstring PadNumber(int value, size_t width) {
    std::wstring text = value > 0 ? std::to_wstring(value) : std::wstring();
    if (text.size() < width) text.insert(0, width - text.size(), L' ');
    return text;
}

void RenderCompare(CompareWindow& compare, bool keepScroll) {
    const ThemePalette& palette = CurrentPalette();
    const bool dark = g_app.darkTheme;
    const COLORREF removedBack = Blend(palette.editorBackground, dark ? RGB(240, 90, 90) : RGB(230, 60, 60), dark ? 22 : 16);
    const COLORREF addedBack = Blend(palette.editorBackground, dark ? RGB(80, 200, 120) : RGB(40, 170, 80), dark ? 22 : 16);
    const COLORREF changedBack = Blend(palette.editorBackground, dark ? RGB(220, 180, 80) : RGB(220, 160, 30), dark ? 16 : 14);
    const COLORREF changedInner = Blend(palette.editorBackground, dark ? RGB(220, 180, 80) : RGB(220, 160, 30), dark ? 42 : 36);
    const COLORREF fillerBack = Blend(palette.editorBackground, palette.gutterText, 10);

    const size_t widthLeft = std::to_wstring(std::max<size_t>(1, compare.sides[0].lines.size())).size();
    const size_t widthRight = std::to_wstring(std::max<size_t>(1, compare.sides[1].lines.size())).size();
    const size_t width = std::max(widthLeft, widthRight);
    // Changed rows are padded with spaces so their tint runs the full row.
    size_t longest = 0;
    for (const CompareSide& side : compare.sides) {
        for (const std::wstring& line : side.lines) longest = std::max(longest, line.size());
    }
    size_t visibleColumns = 0;
    if (HDC dc = GetDC(compare.left)) {
        HGDIOBJ previous = SelectObject(dc, g_app.editorFont);
        TEXTMETRICW metrics{};
        GetTextMetricsW(dc, &metrics);
        SelectObject(dc, previous);
        ReleaseDC(compare.left, dc);
        RECT client{};
        GetClientRect(compare.left, &client);
        if (metrics.tmAveCharWidth > 0) visibleColumns = static_cast<size_t>(client.right / metrics.tmAveCharWidth);
    }
    const size_t padTo = std::max(width + 2 + std::min<size_t>(longest, 400) + 4, visibleColumns > 2 ? visibleColumns - 2 : 0);
    HWND views[2] = {compare.left, compare.right};
    POINT scroll[2]{};
    for (int s = 0; s < 2; ++s) SendMessageW(views[s], EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll[s]));

    for (int s = 0; s < 2; ++s) {
        const CompareSide& side = compare.sides[s];
        std::wstring text;
        std::vector<LONG> rowStart;
        rowStart.reserve(compare.rows.size() + 1);
        for (const CompareRow& row : compare.rows) {
            rowStart.push_back(static_cast<LONG>(text.size()));
            const int index = s == 0 ? row.left : row.right;
            const size_t before = text.size();
            if (index >= 0) text += PadNumber(index + 1, width) + L"  " + side.lines[static_cast<size_t>(index)];
            else text += std::wstring(width, L' ') + L"  ";
            if (row.kind != RowKind::Same && text.size() - before < padTo) text.append(padTo - (text.size() - before), L' ');
            text += L'\r';
        }
        rowStart.push_back(static_cast<LONG>(text.size()));
        HWND view = views[s];
        SendMessageW(view, WM_SETREDRAW, FALSE, 0);
        SendMessageW(view, EM_SETBKGNDCOLOR, 0, palette.editorBackground);
        CHARFORMAT2W base{};
        base.cbSize = sizeof(base);
        base.dwMask = CFM_COLOR | CFM_BACKCOLOR | CFM_BOLD;
        base.crTextColor = palette.editorText;
        base.crBackColor = palette.editorBackground;
        SendMessageW(view, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&base));
        SendMessageW(view, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&base));
        SendMessageW(view, EM_SETREADONLY, FALSE, 0);  // TOM refuses to format read-only text
        SetWindowTextW(view, text.c_str());

        TextDocumentRef tom(view);
        if (tom) {
            ITextFont* gutter = tom.MakeStyleFont(palette.gutterText, false);
            ITextFont* backs[5]{};
            const COLORREF colors[5] = {removedBack, addedBack, changedBack, changedInner, fillerBack};
            for (int c = 0; c < 5; ++c) {
                ITextFont* font = tom.MakeStyleFont(palette.editorText, false);
                if (font) {
                    font->Reset(tomUndefined);
                    font->SetBackColor(static_cast<long>(colors[c]));
                }
                backs[c] = font;
            }
            for (size_t r = 0; r < compare.rows.size(); ++r) {
                const CompareRow& row = compare.rows[r];
                const LONG start = rowStart[r];
                const LONG end = rowStart[r + 1] - 1;  // without the line break
                const int index = s == 0 ? row.left : row.right;
                tom.ApplyFont(start, start + static_cast<LONG>(width), gutter);
                if (row.kind == RowKind::Same) continue;
                if (index < 0) {
                    tom.ApplyFont(start, end, backs[4]);
                    continue;
                }
                const bool onlyHere = (s == 0 && row.kind == RowKind::LeftOnly) || (s == 1 && row.kind == RowKind::RightOnly);
                tom.ApplyFont(start, end, onlyHere ? backs[s == 0 ? 0 : 1] : backs[2]);
                if (row.kind == RowKind::Changed) {
                    // Highlight just the part of the line that differs.
                    const std::wstring& a = compare.sides[0].lines[static_cast<size_t>(row.left)];
                    const std::wstring& b = compare.sides[1].lines[static_cast<size_t>(row.right)];
                    size_t pre = 0;
                    while (pre < a.size() && pre < b.size() && a[pre] == b[pre]) ++pre;
                    size_t suf = 0;
                    while (suf < a.size() - pre && suf < b.size() - pre && a[a.size() - 1 - suf] == b[b.size() - 1 - suf]) ++suf;
                    const std::wstring& mine = s == 0 ? a : b;
                    const LONG textStart = start + static_cast<LONG>(width) + 2;
                    if (mine.size() > pre + suf) tom.ApplyFont(textStart + static_cast<LONG>(pre), textStart + static_cast<LONG>(mine.size() - suf), backs[3]);
                }
            }
            if (gutter) gutter->Release();
            for (ITextFont* font : backs) if (font) font->Release();
            SendMessageW(view, EM_EMPTYUNDOBUFFER, 0, 0);
        }
        if (keepScroll) SendMessageW(view, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll[s]));
        SendMessageW(view, EM_SETREADONLY, TRUE, 0);
        SendMessageW(view, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(view, nullptr, TRUE);
    }

    int changed = 0, leftOnly = 0, rightOnly = 0;
    for (const CompareRow& row : compare.rows) {
        changed += row.kind == RowKind::Changed;
        leftOnly += row.kind == RowKind::LeftOnly;
        rightOnly += row.kind == RowKind::RightOnly;
    }
    std::wstring stats = compare.hunks.empty() ? std::wstring(L"两侧内容相同")
        : L"共 " + std::to_wstring(compare.hunks.size()) + L" 处差异：修改 " + std::to_wstring(changed) + L" 行，仅左侧 " +
          std::to_wstring(leftOnly) + L" 行，仅右侧 " + std::to_wstring(rightOnly) + L" 行";
    if (compare.ignoreWhitespace) stats += L"（已忽略空白）";
    if (!compare.structMode) SetWindowTextW(compare.stats, stats.c_str());
    for (int s = 0; s < 2; ++s) {
        const CompareSide& side = compare.sides[s];
        std::wstring title = (side.dirty ? L"● " : L"") + (side.path.empty() ? side.title : side.path);
        if (side.editor && !side.path.empty()) title += L"（来自标签）";
        SetWindowTextW(s == 0 ? compare.leftTitle : compare.rightTitle, title.c_str());
    }
    EnableWindow(GetDlgItem(compare.window, kCmpSaveLeft), compare.sides[0].dirty);
    EnableWindow(GetDlgItem(compare.window, kCmpSaveRight), compare.sides[1].dirty);
    if (!compare.structMode) {
        for (const int id : {kCmpPrev, kCmpNext, kCmpToRight, kCmpToLeft}) EnableWindow(GetDlgItem(compare.window, id), !compare.hunks.empty());
    }
}

void RecomputeCompare(CompareWindow& compare, bool keepScroll) {
    compare.rows = BuildCompareRows(compare.sides[0], compare.sides[1], compare.ignoreWhitespace, compare.hunks);
    compare.current = std::min(compare.current, static_cast<int>(compare.hunks.size()) - 1);
    RenderCompare(compare, keepScroll);
    if (compare.structMode && !BuildStructure(compare, true)) {
        compare.structMode = false;
        ShowWindow(compare.list, SW_HIDE);
        ShowWindow(compare.left, SW_SHOW);
        ShowWindow(compare.right, SW_SHOW);
        SetWindowTextW(GetDlgItem(compare.window, kCmpStructure), L"结构比较");
    }
}

void GoToHunk(CompareWindow& compare, int index) {
    if (compare.hunks.empty()) return;
    index = (index % static_cast<int>(compare.hunks.size()) + static_cast<int>(compare.hunks.size())) % static_cast<int>(compare.hunks.size());
    compare.current = index;
    const CompareHunk& hunk = compare.hunks[static_cast<size_t>(index)];
    for (HWND view : {compare.left, compare.right}) {
        const LRESULT first = SendMessageW(view, EM_GETFIRSTVISIBLELINE, 0, 0);
        SendMessageW(view, EM_LINESCROLL, 0, std::max(0, hunk.firstRow - 3) - first);
        const LRESULT position = SendMessageW(view, EM_LINEINDEX, hunk.firstRow, 0);
        SendMessageW(view, EM_SETSEL, position, position);
    }
    SyncCompareScroll(compare, compare.left);
    SetWindowTextW(compare.stats, (L"第 " + std::to_wstring(index + 1) + L" / " + std::to_wstring(compare.hunks.size()) + L" 处差异").c_str());
}

// The hunk at the caret of the focused pane, else the current one.
int HunkAtCaret(CompareWindow& compare) {
    HWND view = GetFocus() == compare.right ? compare.right : compare.left;
    CHARRANGE selection{};
    SendMessageW(view, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&selection));
    const int row = static_cast<int>(SendMessageW(view, EM_EXLINEFROMCHAR, 0, selection.cpMin));
    for (size_t h = 0; h < compare.hunks.size(); ++h) {
        if (row >= compare.hunks[h].firstRow && row <= compare.hunks[h].lastRow) return static_cast<int>(h);
    }
    return compare.current;
}

// Takes one block from one side into the other (block-level merge).
void CopyHunk(CompareWindow& compare, bool toRight) {
    const int index = HunkAtCaret(compare);
    if (index < 0 || index >= static_cast<int>(compare.hunks.size())) {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    const CompareHunk hunk = compare.hunks[static_cast<size_t>(index)];
    CompareSide& from = compare.sides[toRight ? 0 : 1];
    CompareSide& to = compare.sides[toRight ? 1 : 0];
    const int fromStart = toRight ? hunk.leftStart : hunk.rightStart;
    const int fromCount = toRight ? hunk.leftCount : hunk.rightCount;
    const int toStart = toRight ? hunk.rightStart : hunk.leftStart;
    const int toCount = toRight ? hunk.rightCount : hunk.leftCount;
    std::vector<std::wstring> block(from.lines.begin() + fromStart, from.lines.begin() + fromStart + fromCount);
    to.lines.erase(to.lines.begin() + toStart, to.lines.begin() + toStart + toCount);
    to.lines.insert(to.lines.begin() + toStart, block.begin(), block.end());
    to.dirty = true;
    compare.current = index;
    RecomputeCompare(compare, true);
    if (!compare.hunks.empty()) GoToHunk(compare, std::min(index, static_cast<int>(compare.hunks.size()) - 1));
}

bool SaveCompareSide(CompareWindow& compare, int s) {
    CompareSide& side = compare.sides[s];
    std::wstring text;
    if (side.editor) {
        const int index = IndexOfEditor(side.editor);
        if (index < 0) {
            MessageBoxW(compare.window, L"来源标签已关闭，无法写回。", kWindowTitle, MB_ICONWARNING);
            return false;
        }
        if (HashText(GetEditorRawText(side.editor)) != side.sourceHash) {
            const std::wstring question = L"标签“" + g_app.documents[static_cast<size_t>(index)].title +
                L"”在打开比较后又被编辑过，写回会覆盖这些修改（之后仍可在该标签中 Ctrl+Z 撤销）。\n\n确定写回吗？";
            if (MessageBoxW(compare.window, question.c_str(), kWindowTitle, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return false;
        }
        text = JoinLines(side.lines, L"\r");
        SendMessageW(side.editor, EM_SETSEL, 0, -1);
        SendMessageW(side.editor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
        side.sourceHash = HashText(GetEditorRawText(side.editor));
        side.dirty = false;
        RenderCompare(compare, true);
        ShowNotice(L"已写回标签“" + g_app.documents[static_cast<size_t>(index)].title + L"”（未保存到磁盘，可撤销）");
        return true;
    }
    text = JoinLines(side.lines, side.eol);
    if (side.encoding == kEncodingAnsi && !IsAnsiRepresentable(text)) {
        MessageBoxW(compare.window, L"内容包含 ANSI 编码无法表示的字符，未保存。", kWindowTitle, MB_ICONWARNING);
        return false;
    }
    const uint64_t onDisk = FileTimestamp(side.path);
    if (side.sourceTimestamp != 0 && onDisk != 0 && onDisk != side.sourceTimestamp) {
        const std::wstring question = L"“" + fs::path(side.path).filename().wstring() +
            L"”在打开比较后已被其他程序修改，保存会覆盖那些修改。\n\n确定保存吗？（可以先点“刷新”重新比较）";
        if (MessageBoxW(compare.window, question.c_str(), kWindowTitle, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return false;
    }
    std::string bytes;
    if (!EncodeTextBytes(text, side.encoding, bytes) || !WriteAtomicFile(side.path, bytes)) {
        MessageBoxW(compare.window, (L"保存失败：" + side.path).c_str(), kWindowTitle, MB_ICONERROR);
        return false;
    }
    side.sourceTimestamp = FileTimestamp(side.path);
    side.dirty = false;
    for (size_t d = 0; d < g_app.documents.size(); ++d) {
        const TabDocument& document = g_app.documents[d];
        if (!document.untitled && !document.modified && SamePath(document.path, side.path)) ReloadDocumentFromDisk(static_cast<int>(d));
    }
    RenderCompare(compare, true);
    ShowNotice(L"已保存 " + fs::path(side.path).filename().wstring());
    return true;
}

void LayoutCompareWindow(CompareWindow& compare) {
    RECT client{};
    GetClientRect(compare.window, &client);
    const int width = client.right, height = client.bottom;
    int x = Scale(10);
    const int buttonY = Scale(8), buttonH = Scale(28);
    const std::pair<int, int> buttons[] = {{kCmpPrev, 72}, {kCmpNext, 72}, {kCmpToRight, 108}, {kCmpToLeft, 108}, {kCmpSwap, 56},
        {kCmpStructure, 84}, {kCmpIgnoreSpace, 92}, {kCmpSaveLeft, 80}, {kCmpSaveRight, 80}, {kCmpRefresh, 56}};
    for (const auto& [id, w] : buttons) {
        MoveWindow(GetDlgItem(compare.window, id), x, buttonY, Scale(w), buttonH, TRUE);
        x += Scale(w) + Scale(6);
    }
    MoveWindow(compare.stats, Scale(10), buttonY + buttonH + Scale(6), std::max(0, width - Scale(20)), Scale(20), TRUE);
    const int half = (width - Scale(30)) / 2;
    const int titleY = buttonY + buttonH + Scale(30);
    MoveWindow(compare.leftTitle, Scale(10), titleY, half, Scale(20), TRUE);
    MoveWindow(compare.rightTitle, Scale(20) + half, titleY, half, Scale(20), TRUE);
    const int viewY = titleY + Scale(24);
    MoveWindow(compare.left, Scale(10), viewY, half, std::max(0, height - viewY - Scale(10)), TRUE);
    MoveWindow(compare.right, Scale(20) + half, viewY, half, std::max(0, height - viewY - Scale(10)), TRUE);
    if (compare.list) {
        MoveWindow(compare.list, Scale(10), viewY, std::max(0, width - Scale(20)), std::max(0, height - viewY - Scale(10)), TRUE);
        const int column = std::max(Scale(120), (width - Scale(20) - Scale(70) - Scale(30)) / 3);
        ListView_SetColumnWidth(compare.list, 0, column);
        ListView_SetColumnWidth(compare.list, 1, Scale(70));
        ListView_SetColumnWidth(compare.list, 2, column);
        ListView_SetColumnWidth(compare.list, 3, column);
    }
}

bool ConfirmDiscardCompare(CompareWindow& compare) {
    if (!compare.sides[0].dirty && !compare.sides[1].dirty) return true;
    const int choice = MessageBoxW(compare.window, L"合并后的修改还没有保存。是否保存？", kWindowTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) return false;
    if (choice == IDYES) {
        for (int s = 0; s < 2; ++s) {
            if (compare.sides[s].dirty && !SaveCompareSide(compare, s)) return false;
        }
    }
    return true;
}

