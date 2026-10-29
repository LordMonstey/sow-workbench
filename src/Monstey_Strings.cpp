#include "Monstey_Strings.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <set>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace Monstey {
namespace {
struct Monstey_StringEntry {
    std::string text;
    std::optional<std::string> key;
    bool operator==(const Monstey_StringEntry &) const = default;
};
struct Monstey_StringKey {
    std::uint32_t hash;
    std::optional<std::string> name;
};
std::string Monstey_KeyCanonical(std::string s) {
    for (auto &c : s) {
        if (c == '/')
            c = '\\';
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + 32);
    }
    return s;
}
std::string Monstey_StringFold(std::string_view text) {
    if (std::all_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) < 128; })) {
        auto s = std::string(text);
        for (auto &c : s)
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c + 32);
        return s;
    }
    auto wide = Monstey_Wide(text);
    if (wide.empty())
        return {};
    auto size = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(),
                              static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr, 0);
    Monstey_Require(size > 0, "Unicode lowercase conversion failed");
    std::wstring lower(static_cast<std::size_t>(size), L'\0');
    Monstey_Require(LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(),
                                  static_cast<int>(wide.size()), lower.data(), size, nullptr, nullptr,
                                  0) == size,
                    "Unicode lowercase conversion failed");
    return Monstey_Utf8(lower);
}
Monstey_StringKey Monstey_ParseStringKey(const Monstey_Json &key) {
    if (key.is_integer()) {
        auto n = key.as_uint();
        Monstey_Require(n <= 0xffffffffull, "String key hash must be uint32");
        return {static_cast<std::uint32_t>(n), {}};
    }
    Monstey_Require(key.is_string(), "String key must be a uint32 hash or symbolic string");
    auto text = key.as_string();
    Monstey_Require(!text.empty(), "String key must not be empty");
    if (text.starts_with("0x") || text.starts_with("0X")) {
        std::uint32_t value{};
        auto first = text.data() + 2, last = text.data() + text.size();
        auto [end, error] = std::from_chars(first, last, value, 16);
        Monstey_Require(first != last && end == last && error == std::errc{},
                        "Invalid uint32 hexadecimal string key");
        return {value, {}};
    }
    Monstey_Require(std::all_of(text.begin(), text.end(), [](char c) { return c >= 32 && c < 127; }),
                    "Symbolic keys must be printable ASCII without NUL");
    return {Monstey_PathHash(text), text};
}
void Monstey_ValidateStringText(std::string_view text) {
    Monstey_Require(text.find('\0') == std::string::npos, "String text cannot contain NUL");
    (void)Monstey_Wide(text);
}
struct Monstey_Skdb {
    Monstey_Bytes raw, pool;
    std::map<std::uint32_t, std::uint32_t> offsets;
    std::map<std::uint32_t, std::string> values;
    std::uint32_t units{};
    explicit Monstey_Skdb(Monstey_Bytes input) : raw(std::move(input)) {
        Monstey_Require(raw.size() >= 28 && std::memcmp(raw.data(), "SKDB", 4) == 0, "Expected SKDB header");
        Monstey_Require(Monstey_U32(raw, 4) == 9 && Monstey_U32(raw, 8) == 28 && Monstey_U32(raw, 16) == 0 &&
                            Monstey_U32(raw, 24) == 0,
                        "Supported layout is SKDB v9, header 28 and reserved words zero");
        auto count = Monstey_U32(raw, 12);
        units = Monstey_U32(raw, 20);
        auto start = 28ull + 8ull * count;
        Monstey_Require(start + 2ull * units == raw.size(),
                        "SKDB row and UTF16 counts differ from file length");
        pool.assign(raw.begin() + static_cast<std::ptrdiff_t>(start), raw.end());
        std::map<std::uint32_t, std::string> cache;
        std::uint32_t previous{};
        for (std::uint32_t j = 0; j < count; ++j) {
            auto hash = Monstey_U32(raw, 28ull + 8ull * j), offset = Monstey_U32(raw, 32ull + 8ull * j);
            Monstey_Require(j == 0 || previous < hash, "SKDB hashes must be strictly increasing");
            previous = hash;
            Monstey_Require(offset < units, "SKDB UTF16 offset outside pool");
            if (!cache.contains(offset)) {
                std::wstring text;
                auto cursor = offset;
                while (cursor < units && Monstey_U16(pool, 2ull * cursor) != 0) {
                    text.push_back(static_cast<wchar_t>(Monstey_U16(pool, 2ull * cursor)));
                    ++cursor;
                }
                Monstey_Require(cursor < units, "SKDB unterminated UTF16 text");
                cache.emplace(offset, Monstey_Utf8(text));
            }
            offsets.emplace(hash, offset);
            values.emplace(hash, cache.at(offset));
        }
    }
};
Monstey_Bytes Monstey_EmptySkdb() {
    Monstey_Bytes b{'S', 'K', 'D', 'B'};
    for (auto n : {9u, 28u, 0u, 0u, 0u, 0u})
        Monstey_AppendU32(b, n);
    return b;
}
std::string Monstey_SPath(const std::filesystem::path &p) { return Monstey_Utf8(p.wstring()); }
} // namespace
struct Monstey_StringsProject::Monstey_Impl {
    std::filesystem::path source;
    std::filesystem::file_time_type time{};
    std::uint64_t size{};
    std::string sha;
    Monstey_Skdb native;
    std::map<std::uint32_t, Monstey_StringEntry> current;
    using Delta = std::map<std::uint32_t, std::optional<Monstey_StringEntry>>;
    struct Step {
        Delta before, after;
        Monstey_Json event;
    };
    std::vector<Step> undo, redo;
    explicit Monstey_Impl(const std::filesystem::path &input)
        : source(input.empty() ? input : std::filesystem::absolute(input)), native(Monstey_EmptySkdb()) {
        if (!source.empty()) {
            size = std::filesystem::file_size(source);
            time = std::filesystem::last_write_time(source);
            native = Monstey_Skdb(Monstey_ReadFile(source));
            sha = Monstey_HashSha256(native.raw);
            Check(false);
        }
        for (auto &[hash, text] : native.values)
            current.emplace(hash, Monstey_StringEntry{text, {}});
    }
    void Check(bool full = false) const {
        if (source.empty())
            return;
        Monstey_Require(std::filesystem::file_size(source) == size &&
                            std::filesystem::last_write_time(source) == time,
                        "Source string table changed; reopen it");
        if (full)
            Monstey_Require(Monstey_HashSha256(source) == sha, "Source string table hash changed");
    }
    void Bind(const Monstey_StringKey &key) const {
        auto it = current.find(key.hash);
        if (key.name && it != current.end() && it->second.key)
            Monstey_Require(Monstey_KeyCanonical(*key.name) == Monstey_KeyCanonical(*it->second.key),
                            "Native string key hash collision");
    }
    std::optional<Monstey_StringEntry> Get(std::uint32_t key) const {
        auto it = current.find(key);
        return it == current.end() ? std::optional<Monstey_StringEntry>{} : it->second;
    }
    void Apply(const Delta &values) {
        for (auto &[key, value] : values) {
            if (value)
                current[key] = *value;
            else
                current.erase(key);
        }
    }
    bool Commit(Delta after, Monstey_Json event) {
        Delta before;
        std::erase_if(after, [&](auto &row) {
            auto old = Get(row.first);
            if (old == row.second)
                return true;
            before.emplace(row.first, std::move(old));
            return false;
        });
        if (after.empty())
            return false;
        Apply(after);
        undo.push_back({std::move(before), std::move(after), std::move(event)});
        redo.clear();
        return true;
    }
    Monstey_Json Changes() const {
        Monstey_Json::Array set, removed, events;
        Monstey_Json::Object keys;
        for (auto &[hash, entry] : current) {
            auto base = native.values.find(hash);
            if (base == native.values.end() || base->second != entry.text) {
                auto row = Monstey_Json::object({{"hash", Monstey_Hex(hash)}, {"text", entry.text}});
                if (entry.key)
                    row["key"] = *entry.key;
                set.push_back(std::move(row));
            }
            if (entry.key)
                keys.emplace(Monstey_Hex(hash), *entry.key);
        }
        for (auto &[hash, text] : native.values) {
            (void)text;
            if (!current.contains(hash))
                removed.push_back(Monstey_Hex(hash));
        }
        for (auto &step : undo)
            events.push_back(step.event);
        return Monstey_Json::object({{"set", set}, {"remove", removed}, {"keys", keys}, {"events", events}});
    }
    Monstey_Json Describe() const {
        auto changes = Changes();
        auto count = changes.at("set").size() + changes.at("remove").size();
        return Monstey_Json::object(
            {{"source", source.empty() ? Monstey_Json{} : Monstey_Json(Monstey_SPath(source))},
             {"source_sha256", source.empty() ? Monstey_Json{} : Monstey_Json(sha)},
             {"format", "SKDB"},
             {"version", 9},
             {"entries", static_cast<std::uint64_t>(current.size())},
             {"dirty", count != 0},
             {"annotations_dirty", !changes.at("keys").empty()},
             {"can_undo", !undo.empty()},
             {"can_redo", !redo.empty()},
             {"changes_count", static_cast<std::uint64_t>(count)},
             {"original_key_names_stored", false},
             {"capabilities", Monstey_Json::object({{"add", true},
                                                    {"set", true},
                                                    {"delete", true},
                                                    {"rename", true},
                                                    {"new_table", true},
                                                    {"unicode_utf16", true},
                                                    {"preserve_original_text_pool", true},
                                                    {"runtime_verified", false}})}});
    }
};
Monstey_StringsProject::Monstey_StringsProject(const std::filesystem::path &source)
    : impl_(std::make_unique<Monstey_Impl>(source)) {}
Monstey_StringsProject::~Monstey_StringsProject() = default;
Monstey_StringsProject::Monstey_StringsProject(Monstey_StringsProject &&) noexcept = default;
Monstey_StringsProject &Monstey_StringsProject::operator=(Monstey_StringsProject &&) noexcept = default;
Monstey_Json Monstey_StringsProject::Describe() const {
    impl_->Check();
    return impl_->Describe();
}
Monstey_Json Monstey_StringsProject::Changes() const {
    impl_->Check();
    return impl_->Changes();
}
Monstey_Json Monstey_StringsProject::Entries(std::string_view query, std::size_t offset,
                                             std::size_t limit) const {
    impl_->Check();
    Monstey_Require(query.size() <= 2048 && limit >= 1 && limit <= 500,
                    "Search must be at most 2048 bytes and page size 1..500");
    auto folded = Monstey_StringFold(query);
    Monstey_Json::Array rows;
    std::size_t total{};
    for (auto &[hash, entry] : impl_->current) {
        auto hex = Monstey_Hex(hash);
        if (!folded.empty() && hex.find(folded) == std::string::npos &&
            Monstey_StringFold(entry.text).find(folded) == std::string::npos &&
            (!entry.key || Monstey_StringFold(*entry.key).find(folded) == std::string::npos))
            continue;
        if (total >= offset && rows.size() < limit) {
            auto base = impl_->native.values.find(hash);
            auto row = Monstey_Json::object(
                {{"hash", hex},
                 {"text", entry.text},
                 {"dirty", base == impl_->native.values.end() || base->second != entry.text}});
            if (entry.key)
                row["key"] = *entry.key;
            rows.push_back(std::move(row));
        }
        ++total;
    }
    return Monstey_Json::object({{"rows", rows},
                                 {"items", rows},
                                 {"total", static_cast<std::uint64_t>(total)},
                                 {"offset", static_cast<std::uint64_t>(offset)},
                                 {"limit", static_cast<std::uint64_t>(limit)}});
}
Monstey_Json Monstey_StringsProject::Set(const Monstey_Json &input, std::string_view text) {
    impl_->Check();
    auto key = Monstey_ParseStringKey(input);
    Monstey_ValidateStringText(text);
    impl_->Bind(key);
    auto previous = impl_->Get(key.hash);
    auto entry = Monstey_StringEntry{std::string(text),
                                     key.name ? key.name : (previous ? previous->key : std::nullopt)};
    auto changed = impl_->Commit(
        {{key.hash, entry}},
        Monstey_Json::object({{"operation", "set"}, {"hash", Monstey_Hex(key.hash)}, {"text", text}}));
    auto result = impl_->Describe();
    result["hash"] = Monstey_Hex(key.hash);
    result["changed"] = changed;
    return result;
}
Monstey_Json Monstey_StringsProject::Remove(const Monstey_Json &input) {
    impl_->Check();
    auto key = Monstey_ParseStringKey(input);
    impl_->Bind(key);
    Monstey_Require(impl_->current.contains(key.hash), "String key does not exist");
    impl_->Commit({{key.hash, std::nullopt}},
                  Monstey_Json::object({{"operation", "remove"}, {"hash", Monstey_Hex(key.hash)}}));
    return impl_->Describe();
}
Monstey_Json Monstey_StringsProject::Rename(const Monstey_Json &before, const Monstey_Json &after) {
    impl_->Check();
    auto old = Monstey_ParseStringKey(before), next = Monstey_ParseStringKey(after);
    impl_->Bind(old);
    impl_->Bind(next);
    auto value = impl_->Get(old.hash);
    Monstey_Require(value.has_value(), "String key does not exist");
    Monstey_Require(old.hash == next.hash || !impl_->current.contains(next.hash),
                    "Rename target hash exists");
    if (old.hash == next.hash) {
        if (next.name)
            value->key = next.name;
        impl_->Commit({{old.hash, value}}, Monstey_Json::object({{"operation", "rename_annotation"},
                                                                 {"hash", Monstey_Hex(old.hash)}}));
    } else {
        value->key = next.name;
        impl_->Commit({{old.hash, std::nullopt}, {next.hash, value}},
                      Monstey_Json::object({{"operation", "rename"},
                                            {"old_hash", Monstey_Hex(old.hash)},
                                            {"hash", Monstey_Hex(next.hash)}}));
    }
    return impl_->Describe();
}
Monstey_Json Monstey_StringsProject::Undo() {
    impl_->Check();
    if (!impl_->undo.empty()) {
        auto step = std::move(impl_->undo.back());
        impl_->undo.pop_back();
        impl_->Apply(step.before);
        impl_->redo.push_back(std::move(step));
    }
    return impl_->Describe();
}
Monstey_Json Monstey_StringsProject::Redo() {
    impl_->Check();
    if (!impl_->redo.empty()) {
        auto step = std::move(impl_->redo.back());
        impl_->redo.pop_back();
        impl_->Apply(step.after);
        impl_->undo.push_back(std::move(step));
    }
    return impl_->Describe();
}
Monstey_Json Monstey_StringsProject::Revert() {
    impl_->Check();
    Monstey_Impl::Delta delta;
    for (auto &[hash, value] : impl_->current) {
        (void)value;
        auto base = impl_->native.values.find(hash);
        delta.emplace(hash, base == impl_->native.values.end() ? std::optional<Monstey_StringEntry>{}
                                                               : Monstey_StringEntry{base->second, {}});
    }
    for (auto &[hash, text] : impl_->native.values)
        delta[hash] = Monstey_StringEntry{text, {}};
    impl_->Commit(std::move(delta), Monstey_Json::object({{"operation", "revert"}}));
    return impl_->Describe();
}
Monstey_Json Monstey_StringsProject::SaveProject(const std::filesystem::path &output) const {
    impl_->Check(true);
    auto doc = Monstey_Json::object(
        {{"format", "MONSTEY_SOW_STRINGS_PROJECT"},
         {"version", 1},
         {"source", impl_->source.empty() ? Monstey_Json{} : Monstey_Json(Monstey_SPath(impl_->source))},
         {"source_sha256", impl_->source.empty() ? Monstey_Json{} : Monstey_Json(impl_->sha)},
         {"changes", impl_->Changes()},
         {"runtime_verified", false}});
    Monstey_WriteNewText(output, doc.dump(2) + "\n");
    return Monstey_Json::object(
        {{"project_path", Monstey_SPath(std::filesystem::absolute(output))},
         {"source_sha256", impl_->source.empty() ? Monstey_Json{} : Monstey_Json(impl_->sha)}});
}
std::unique_ptr<Monstey_StringsProject>
Monstey_StringsProject::LoadProject(const std::filesystem::path &input) {
    auto raw = Monstey_ReadText(input);
    if (raw.starts_with("\xef\xbb\xbf"))
        raw.erase(0, 3);
    auto doc = Monstey_Json::Parse(raw);
    Monstey_Require(doc.at("format").as_string() == "MONSTEY_SOW_STRINGS_PROJECT" &&
                        doc.at("version").as_int() == 1,
                    "Unsupported string project");
    auto result = std::make_unique<Monstey_StringsProject>(
        doc.at("source").is_null() ? std::filesystem::path{}
                                   : std::filesystem::path(Monstey_Wide(doc.at("source").as_string())));
    auto &p = *result->impl_;
    Monstey_Require(p.source.empty() ? doc.at("source_sha256").is_null()
                                     : doc.at("source_sha256").as_string() == p.sha,
                    "Project source string table hash does not match");
    auto &changes = doc.at("changes");
    Monstey_Impl::Delta delta;
    std::set<std::uint32_t> seen;
    for (auto &key : changes.at("remove").as_array()) {
        auto hash = Monstey_ParseStringKey(key).hash;
        Monstey_Require(p.current.contains(hash) && seen.insert(hash).second,
                        "Project removes duplicate or missing key");
        delta.emplace(hash, std::nullopt);
    }
    for (auto &row : changes.at("set").as_array()) {
        auto key = Monstey_ParseStringKey(row.at("hash"));
        Monstey_Require(seen.insert(key.hash).second, "Duplicate project change hash");
        auto text = row.at("text").as_string();
        Monstey_ValidateStringText(text);
        delta[key.hash] = Monstey_StringEntry{text, {}};
    }
    for (auto &[hash_string, label] : changes.at("keys").as_object()) {
        auto hash = Monstey_ParseStringKey(hash_string).hash;
        auto key = Monstey_ParseStringKey(label);
        Monstey_Require(key.hash == hash && key.name.has_value(), "Project symbolic key hash mismatch");
        auto it = delta.find(hash);
        auto entry = it == delta.end() ? p.Get(hash) : it->second;
        Monstey_Require(entry.has_value(), "Project annotation refers to absent key");
        entry->key = key.name;
        delta[hash] = entry;
    }
    p.Commit(std::move(delta), Monstey_Json::object({{"operation", "load_project"}}));
    p.Check(true);
    return result;
}
Monstey_Json Monstey_StringsProject::Build(const std::filesystem::path &output) const {
    impl_->Check(true);
    auto &p = *impl_;
    auto pool = p.native.pool;
    std::map<std::string, std::uint32_t> by_text;
    for (auto &[hash, text] : p.native.values)
        by_text.try_emplace(text, p.native.offsets.at(hash));
    std::vector<std::pair<std::uint32_t, std::uint32_t>> rows;
    std::size_t unchanged{};
    for (auto &[hash, entry] : p.current) {
        std::uint32_t offset{};
        auto base = p.native.values.find(hash);
        if (base != p.native.values.end() && base->second == entry.text) {
            offset = p.native.offsets.at(hash);
            ++unchanged;
        } else if (auto it = by_text.find(entry.text); it != by_text.end())
            offset = it->second;
        else {
            Monstey_Require(pool.size() / 2 <= 0xffffffffull, "UTF16 pool exceeds uint32 offsets");
            offset = static_cast<std::uint32_t>(pool.size() / 2);
            auto wide = Monstey_Wide(entry.text);
            for (auto c : wide)
                Monstey_AppendU16(pool, static_cast<std::uint16_t>(c));
            Monstey_AppendU16(pool, 0);
            by_text.emplace(entry.text, offset);
        }
        rows.emplace_back(hash, offset);
    }
    Monstey_Require(rows.size() <= 0xffffffffull && pool.size() / 2 <= 0xffffffffull &&
                        28ull + 8ull * rows.size() + pool.size() <= 512ull * 1024 * 1024,
                    "SKDB output exceeds configured extent");
    Monstey_Bytes b{'S', 'K', 'D', 'B'};
    Monstey_AppendU32(b, 9);
    Monstey_AppendU32(b, 28);
    Monstey_AppendU32(b, static_cast<std::uint32_t>(rows.size()));
    Monstey_AppendU32(b, 0);
    Monstey_AppendU32(b, static_cast<std::uint32_t>(pool.size() / 2));
    Monstey_AppendU32(b, 0);
    for (auto [hash, offset] : rows) {
        Monstey_AppendU32(b, hash);
        Monstey_AppendU32(b, offset);
    }
    b.insert(b.end(), pool.begin(), pool.end());
    Monstey_Skdb checked(b);
    Monstey_Require(std::equal(p.native.pool.begin(), p.native.pool.end(), checked.pool.begin()),
                    "Original UTF16 pool prefix changed");
    for (auto &[hash, entry] : p.current) {
        Monstey_Require(checked.values.at(hash) == entry.text, "String readback text differs");
        auto base = p.native.values.find(hash);
        if (base != p.native.values.end() && base->second == entry.text)
            Monstey_Require(checked.offsets.at(hash) == p.native.offsets.at(hash),
                            "Unchanged string offset moved");
    }
    bool noop = p.current.size() == p.native.values.size();
    if (noop)
        for (auto &[hash, entry] : p.current) {
            auto base = p.native.values.find(hash);
            if (base == p.native.values.end() || base->second != entry.text) {
                noop = false;
                break;
            }
        }
    if (noop)
        Monstey_Require(b == p.native.raw, "No-op SKDB compilation is not byte exact");
    p.Check(true);
    bool created = false;
    try {
        Monstey_WriteNewFile(output, b);
        created = true;
        auto reread = Monstey_ReadFile(output);
        Monstey_Require(reread == b, "Published string table readback differs");
        Monstey_Skdb final_table(std::move(reread));
        Monstey_Require(final_table.values == checked.values, "Published string table values differ");
        p.Check(true);
    } catch (...) {
        if (created) {
            std::error_code ec;
            std::filesystem::remove(output, ec);
        }
        throw;
    }
    return Monstey_Json::object({{"status", "MONSTEY_SKDB9_BUILD_VALIDATED"},
                                 {"path", Monstey_SPath(std::filesystem::absolute(output))},
                                 {"sha256", Monstey_HashSha256(b)},
                                 {"bytes", static_cast<std::uint64_t>(b.size())},
                                 {"entries", static_cast<std::uint64_t>(rows.size())},
                                 {"text_code_units", static_cast<std::uint64_t>(pool.size() / 2)},
                                 {"unchanged_entries_verified", static_cast<std::uint64_t>(unchanged)},
                                 {"original_text_pool_prefix_byte_exact", true},
                                 {"original_unchanged_offsets_preserved", true},
                                 {"all_text_readbacks_verified", true},
                                 {"strict_hash_order", true},
                                 {"no_op_byte_exact", noop},
                                 {"original_key_names_stored", false},
                                 {"symbolic_names_in_project_only", true},
                                 {"game_written", false},
                                 {"runtime_verified", false},
                                 {"warnings", Monstey_Json::array()}});
}
} // namespace Monstey
