#include "Monstey_WorldComposition.hpp"
#include "Monstey_Archive.hpp"
#include "Monstey_World.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
namespace Monstey {
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
std::string Monstey_CString(std::span<const std::uint8_t> bytes, std::size_t offset) {
    Monstey_Require(offset < bytes.size(), "Bundle string offset outside dictionary");
    auto first = bytes.begin() + static_cast<std::ptrdiff_t>(offset), end = std::find(first, bytes.end(), 0);
    Monstey_Require(end != bytes.end(), "Unterminated bundle string");
    auto text = std::string(first, end);
    (void)Monstey_Wide(text);
    return text;
}
std::uint32_t Monstey_CIntern(B &names, std::string_view resource) {
    auto text = Monstey_ArchiveVirtual(std::string(resource));
    std::replace(text.begin(), text.end(), '/', '\\');
    Monstey_Require(names.size() + text.size() + 4 <= 0xffffffffull, "Bundle dictionary exceeds DWORD");
    auto offset = static_cast<std::uint32_t>(names.size());
    names.insert(names.end(), text.begin(), text.end());
    names.push_back(0);
    while (names.size() % 4)
        names.push_back(0);
    return offset;
}
std::filesystem::path Monstey_CPath(const J &v) {
    return std::filesystem::absolute(std::filesystem::path(Monstey_Wide(v.as_string())));
}
std::string Monstey_CPathText(const std::filesystem::path &p) { return Monstey_Utf8(p.wstring()); }
J Monstey_CRecord(const std::filesystem::path &p, std::span<const std::uint8_t> raw) {
    return J::object({{"path", Monstey_CPathText(p)},
                      {"bytes", static_cast<std::uint64_t>(raw.size())},
                      {"sha256", Monstey_HashSha256(raw)}});
}
void Monstey_CKeys(const J &j, std::initializer_list<const char *> allowed) {
    Monstey_Require(j.is_object(), "Expected native composition object");
    for (auto &[key, value] : j.as_object()) {
        (void)value;
        Monstey_Require(std::any_of(allowed.begin(), allowed.end(), [&](auto name) { return key == name; }),
                        "Unknown native composition argument: " + key);
    }
}
struct Monstey_CInputs {
    std::map<std::filesystem::path, std::string> checksums;
    B Load(const std::filesystem::path &p) {
        auto raw = Monstey_ReadFile(p);
        auto hash = Monstey_HashSha256(raw);
        auto [it, inserted] = checksums.emplace(p, hash);
        Monstey_Require(inserted || it->second == hash, "Composition input changed");
        return raw;
    }
    B Verified(const J &row) {
        auto p = Monstey_CPath(row.at("path"));
        auto raw = Load(p);
        Monstey_Require(row.at("sha256").as_string() == Monstey_HashSha256(raw),
                        "Native manifest checksum differs: " + Monstey_CPathText(p));
        if (row.contains("bytes"))
            Monstey_Require(row.at("bytes").as_uint() == raw.size(), "Native manifest byte count differs");
        return raw;
    }
    J Json(const std::filesystem::path &p) {
        auto b = Load(p);
        std::string text(b.begin(), b.end());
        if (text.starts_with("\xef\xbb\xbf"))
            text.erase(0, 3);
        return J::Parse(text);
    }
    void Verify() const {
        for (auto &[p, sha] : checksums)
            Monstey_Require(Monstey_HashSha256(p) == sha,
                            "Composition source changed: " + Monstey_CPathText(p));
    }
};
struct Monstey_COutput {
    std::filesystem::path root;
    std::vector<std::filesystem::path> files, dirs;
    bool committed = false;
    explicit Monstey_COutput(std::filesystem::path p, const Monstey_CInputs &inputs) : root(std::move(p)) {
        std::vector<std::filesystem::path> sources;
        for (auto &[path, sha] : inputs.checksums) {
            (void)sha;
            sources.push_back(path);
        }
        Monstey_RequireOfflineOutput(root, sources);
        Monstey_Require(!std::filesystem::exists(Monstey_WindowsFilePath(root)),
                        "Native component output already exists");
        std::filesystem::create_directories(Monstey_WindowsFilePath(root.parent_path()));
        Monstey_Require(std::filesystem::create_directory(Monstey_WindowsFilePath(root)),
                        "Could not create new component directory");
        dirs.push_back(root);
    }
    ~Monstey_COutput() {
        if (!committed) {
            std::error_code ec;
            for (auto it = files.rbegin(); it != files.rend(); ++it)
                std::filesystem::remove(Monstey_WindowsFilePath(*it), ec);
            for (auto it = dirs.rbegin(); it != dirs.rend(); ++it)
                std::filesystem::remove(Monstey_WindowsFilePath(*it), ec);
        }
    }
    std::filesystem::path Save(std::string_view relative, std::span<const std::uint8_t> raw) {
        auto safe = Monstey_ArchiveVirtual(std::string(relative));
        auto path = root / std::filesystem::path(Monstey_Wide(safe));
        auto parent = root;
        for (auto &piece : std::filesystem::path(Monstey_Wide(safe)).parent_path()) {
            parent /= piece;
            if (std::filesystem::create_directory(Monstey_WindowsFilePath(parent)))
                dirs.push_back(parent);
            else
                Monstey_Require(std::filesystem::is_directory(Monstey_WindowsFilePath(parent)),
                                "Component parent is not a directory");
        }
        Monstey_WriteNewFile(path, raw);
        files.push_back(path);
        Monstey_Require(Monstey_ReadFile(path) == B(raw.begin(), raw.end()),
                        "Component output readback differs");
        return path;
    }
    std::filesystem::path SaveJson(std::string_view name, const J &value) {
        auto text = value.dump(2) + "\n";
        return Save(name, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(text.data()),
                                                        text.size()));
    }
};
J Monstey_CSchema(J::Object properties, std::initializer_list<const char *> required = {}) {
    J::Array keys;
    for (auto k : required)
        keys.push_back(k);
    return J::object({{"type", "object"},
                      {"properties", std::move(properties)},
                      {"required", keys},
                      {"additionalProperties", false}});
}
} // namespace
Monstey_Bundle Monstey_ReadBundle(std::span<const std::uint8_t> b) {
    Monstey_Require(b.size() >= 20 && std::memcmp(b.data(), "BNDL", 4) == 0 && Monstey_U32(b, 4) == 6,
                    "Expected BNDL6");
    auto names = Monstey_U32(b, 8), strings = Monstey_U32(b, 12), count = Monstey_U32(b, 16);
    Monstey_Require(count <= 1000000 && names < strings && 20ull + 8ull * count + strings == b.size(),
                    "BNDL6 table extents differ");
    auto dict = b.subspan(20ull + 4ull * count, names),
         kind = b.subspan(20ull + 4ull * count + names, strings - names);
    auto text = Monstey_CString(kind, 0);
    Monstey_Require(!text.empty(), "Empty BNDL kind");
    Monstey_Require(std::all_of(kind.begin() + static_cast<std::ptrdiff_t>(text.size()), kind.end(),
                                [](auto x) { return x == 0; }),
                    "BNDL kind padding differs");
    Monstey_Bundle result{text, {}};
    std::set<std::string> seen;
    for (std::uint32_t i = 0; i < count; ++i) {
        auto offset = Monstey_U32(b, 20ull + 4ull * count + strings + 4ull * i);
        auto path = Monstey_ArchiveVirtual(Monstey_CString(dict, offset));
        Monstey_Require(seen.insert(path).second, "Duplicate BNDL resource");
        result.resources.push_back({path, Monstey_U32(b, 20ull + 4ull * i)});
    }
    return result;
}
B Monstey_BuildBundle(const Monstey_Bundle &bundle) {
    Monstey_Require(!bundle.kind.empty() && bundle.kind.find('\0') == std::string::npos &&
                        bundle.resources.size() <= 1000000,
                    "Invalid BNDL type/count");
    B names;
    std::vector<std::uint32_t> offsets;
    std::set<std::string> seen;
    for (auto &row : bundle.resources) {
        auto path = Monstey_ArchiveVirtual(row.resource);
        Monstey_Require(seen.insert(path).second, "Duplicate BNDL dependency");
        offsets.push_back(Monstey_CIntern(names, path));
    }
    B kind(bundle.kind.begin(), bundle.kind.end());
    kind.push_back(0);
    while (kind.size() % 4)
        kind.push_back(0);
    Monstey_Require(names.size() + kind.size() <= 0xffffffffull, "BNDL dictionary overflow");
    B out{'B', 'N', 'D', 'L'};
    for (auto n : {6u, static_cast<std::uint32_t>(names.size()),
                   static_cast<std::uint32_t>(names.size() + kind.size()),
                   static_cast<std::uint32_t>(bundle.resources.size())})
        Monstey_AppendU32(out, n);
    for (auto &row : bundle.resources)
        Monstey_AppendU32(out, row.size_hint);
    out.insert(out.end(), names.begin(), names.end());
    out.insert(out.end(), kind.begin(), kind.end());
    for (auto n : offsets)
        Monstey_AppendU32(out, n);
    auto checked = Monstey_ReadBundle(out);
    Monstey_Require(checked.kind == bundle.kind && checked.resources.size() == bundle.resources.size(),
                    "BNDL readback differs");
    return out;
}
std::vector<Monstey_EmbeddedResource> Monstey_ReadEmbedded(std::span<const std::uint8_t> b) {
    Monstey_Require(b.size() >= 16 && std::memcmp(b.data(), "EMBB", 4) == 0 && Monstey_U32(b, 4) == 5,
                    "Expected EMBB5");
    auto names = Monstey_U32(b, 8), count = Monstey_U32(b, 12);
    Monstey_Require(count <= 1000000 && 16ull + names + 4ull * count <= b.size(),
                    "EMBB dictionary/table extent differs");
    auto dict = b.subspan(16, names);
    std::size_t at = 16ull + names + 4ull * count;
    std::set<std::string> seen;
    std::vector<Monstey_EmbeddedResource> result;
    for (std::uint32_t i = 0; i < count; ++i) {
        auto path = Monstey_ArchiveVirtual(Monstey_CString(dict, Monstey_U32(b, 16ull + names + 4ull * i)));
        Monstey_Require(seen.insert(path).second, "Duplicate EMBB child");
        auto size = Monstey_U32(b, at);
        at += 4;
        Monstey_CheckRange(b, at, size);
        result.push_back({path, B(b.begin() + static_cast<std::ptrdiff_t>(at),
                                  b.begin() + static_cast<std::ptrdiff_t>(at + size))});
        at += size;
    }
    Monstey_Require(at == b.size(), "Unexpected EMBB trailer");
    return result;
}
B Monstey_BuildEmbedded(const std::vector<Monstey_EmbeddedResource> &rows) {
    Monstey_Require(rows.size() <= 1000000, "EMBB child count exceeds bound");
    B names;
    std::vector<std::uint32_t> offsets;
    std::set<std::string> seen;
    std::uint64_t extent = 16;
    for (auto &row : rows) {
        auto path = Monstey_ArchiveVirtual(row.resource);
        Monstey_Require(seen.insert(path).second && row.bytes.size() <= 0xffffffffull,
                        "Duplicate/excessive EMBB child");
        offsets.push_back(Monstey_CIntern(names, path));
        extent += 8 + row.bytes.size();
    }
    Monstey_Require(extent + names.size() <= 512ull * 1024 * 1024, "Embedded output exceeds bounded extent");
    B out{'E', 'M', 'B', 'B'};
    for (auto n : {5u, static_cast<std::uint32_t>(names.size()), static_cast<std::uint32_t>(rows.size())})
        Monstey_AppendU32(out, n);
    out.insert(out.end(), names.begin(), names.end());
    for (auto n : offsets)
        Monstey_AppendU32(out, n);
    for (auto &row : rows) {
        Monstey_AppendU32(out, static_cast<std::uint32_t>(row.bytes.size()));
        out.insert(out.end(), row.bytes.begin(), row.bytes.end());
    }
    auto check = Monstey_ReadEmbedded(out);
    Monstey_Require(check.size() == rows.size(), "EMBB readback count differs");
    for (std::size_t i = 0; i < rows.size(); ++i)
        Monstey_Require(check[i].resource == Monstey_ArchiveVirtual(rows[i].resource) &&
                            check[i].bytes == rows[i].bytes,
                        "EMBB child readback differs");
    return out;
}
B Monstey_RebuildBundle(std::span<const std::uint8_t> original,
                        const std::map<std::string, std::uint32_t> &updates,
                        const std::map<std::string, std::uint32_t> &additions) {
    auto parsed = Monstey_ReadBundle(original);
    auto count = parsed.resources.size();
    std::map<std::string, std::uint32_t> changed, added;
    std::set<std::string> known;
    for (auto &row : parsed.resources)
        known.insert(row.resource);
    for (auto &[key, value] : updates) {
        auto path = Monstey_ArchiveVirtual(key);
        Monstey_Require(known.contains(path) && changed.emplace(path, value).second,
                        "BNDL replacement absent or duplicated");
    }
    for (auto &[key, value] : additions) {
        auto path = Monstey_ArchiveVirtual(key);
        Monstey_Require(!known.contains(path) && !changed.contains(path) && added.emplace(path, value).second,
                        "BNDL addition already exists or duplicates another");
    }
    auto names_size = Monstey_U32(original, 8), strings_size = Monstey_U32(original, 12);
    auto start = 20 + 4 * count;
    B names(original.begin() + static_cast<std::ptrdiff_t>(start),
            original.begin() + static_cast<std::ptrdiff_t>(start + names_size));
    B kind(original.begin() + static_cast<std::ptrdiff_t>(start + names_size),
           original.begin() + static_cast<std::ptrdiff_t>(start + strings_size));
    std::vector<std::uint32_t> offsets;
    for (std::size_t i = 0; i < count; ++i)
        offsets.push_back(Monstey_U32(original, start + strings_size + 4 * i));
    for (auto &[path, size] : added) {
        (void)size;
        offsets.push_back(Monstey_CIntern(names, path));
    }
    Monstey_Require(count + added.size() <= 1000000 && names.size() + kind.size() <= 0xffffffffull,
                    "Rebuilt BNDL metadata exceeds bounds");
    B out(original.begin(), original.begin() + 20);
    Monstey_PutU32(out, 8, static_cast<std::uint32_t>(names.size()));
    Monstey_PutU32(out, 12, static_cast<std::uint32_t>(names.size() + kind.size()));
    Monstey_PutU32(out, 16, static_cast<std::uint32_t>(offsets.size()));
    for (auto &row : parsed.resources)
        Monstey_AppendU32(out, changed.contains(row.resource) ? changed.at(row.resource) : row.size_hint);
    for (auto &[path, size] : added) {
        (void)path;
        Monstey_AppendU32(out, size);
    }
    out.insert(out.end(), names.begin(), names.end());
    out.insert(out.end(), kind.begin(), kind.end());
    for (auto offset : offsets)
        Monstey_AppendU32(out, offset);
    auto check = Monstey_ReadBundle(out);
    Monstey_Require(check.resources.size() == count + added.size() && check.kind == parsed.kind,
                    "Rebuilt BNDL readback differs");
    return out;
}
B Monstey_RebuildEmbedded(std::span<const std::uint8_t> original, const std::map<std::string, B> &updates,
                          const std::map<std::string, B> &additions) {
    auto parsed = Monstey_ReadEmbedded(original);
    std::map<std::string, B> changed, added;
    std::set<std::string> known;
    for (auto &row : parsed)
        known.insert(row.resource);
    for (auto &[key, value] : updates) {
        auto path = Monstey_ArchiveVirtual(key);
        Monstey_Require(known.contains(path) && changed.emplace(path, value).second,
                        "EMBB replacement absent or duplicated");
    }
    for (auto &[key, value] : additions) {
        auto path = Monstey_ArchiveVirtual(key);
        Monstey_Require(!known.contains(path) && !changed.contains(path) && added.emplace(path, value).second,
                        "EMBB addition exists or duplicates another");
    }
    auto size = Monstey_U32(original, 8);
    B names(original.begin() + 16, original.begin() + 16 + size);
    std::vector<std::uint32_t> offsets;
    for (std::size_t i = 0; i < parsed.size(); ++i)
        offsets.push_back(Monstey_U32(original, 16 + size + 4 * i));
    for (auto &[path, bytes] : added) {
        (void)bytes;
        offsets.push_back(Monstey_CIntern(names, path));
    }
    Monstey_Require(offsets.size() <= 1000000, "Rebuilt EMBB child count exceeds bounds");
    B out(original.begin(), original.begin() + 16);
    Monstey_PutU32(out, 8, static_cast<std::uint32_t>(names.size()));
    Monstey_PutU32(out, 12, static_cast<std::uint32_t>(offsets.size()));
    out.insert(out.end(), names.begin(), names.end());
    for (auto offset : offsets)
        Monstey_AppendU32(out, offset);
    auto append = [&](const B &bytes) {
        Monstey_Require(bytes.size() <= 0xffffffffull &&
                            out.size() + 4ull + bytes.size() <= 512ull * 1024 * 1024,
                        "Rebuilt embedded payload exceeds bounds");
        Monstey_AppendU32(out, static_cast<std::uint32_t>(bytes.size()));
        out.insert(out.end(), bytes.begin(), bytes.end());
    };
    for (auto &row : parsed)
        append(changed.contains(row.resource) ? changed.at(row.resource) : row.bytes);
    for (auto &[path, bytes] : added) {
        (void)path;
        append(bytes);
    }
    auto check = Monstey_ReadEmbedded(out);
    Monstey_Require(check.size() == offsets.size(), "Rebuilt EMBB readback differs");
    return out;
}
namespace {
B Monstey_CScript(const B &raw, std::string_view path) {
    constexpr std::uint8_t magic[]{'S', 'C', 'R', 'I', 1, 0, 0, 0, 1};
    Monstey_Require(raw.size() >= 14 && std::equal(std::begin(magic), std::end(magic), raw.begin()) &&
                        Monstey_U32(raw, 9) == raw.size() - 13 && raw.back() == 0,
                    "Expected native source SCRI envelope");
    std::string source(raw.begin() + 13, raw.end() - 1), executable, line;
    std::istringstream stream(source);
    Monstey_Require(std::all_of(source.begin(), source.end(), [](char c) { return c > 0 && c < 127; }),
                    "Non-ASCII section script");
    while (std::getline(stream, line)) {
        auto pos = line.find_first_not_of(" \t\r");
        if (pos == std::string::npos || line.compare(pos, 2, "--") == 0)
            continue;
        executable += line + "\n";
    }
    std::smatch match;
    std::regex empty(R"(^\s*local\s+(section_[0-9a-f]+)\s*=\s*\{\s*\}\s*_G\.loadedresource\s*=\s*\1\s*$)");
    Monstey_Require(std::regex_match(executable, match, empty), "Section template has executable logic");
    std::ostringstream symbol;
    symbol << "section_" << std::hex << Monstey_PathHash(path);
    source = std::regex_replace(source, std::regex("\\b" + match[1].str() + "\\b"), symbol.str());
    B output(raw.begin(), raw.begin() + 9);
    Monstey_AppendU32(output, static_cast<std::uint32_t>(source.size() + 1));
    output.insert(output.end(), source.begin(), source.end());
    output.push_back(0);
    return output;
}
B Monstey_CVegetation(const B &raw, std::string_view old_stem, std::string_view new_stem) {
    Monstey_Require(raw.size() >= 10 && std::memcmp(raw.data(), "VEGG", 4) == 0 && Monstey_U32(raw, 4) == 1,
                    "Expected VEGG1");
    auto n = Monstey_U16(raw, 8);
    Monstey_CheckRange(raw, 10, n);
    std::string old(raw.begin() + 10, raw.begin() + 10 + n);
    bool zero = !old.empty() && old.back() == 0;
    if (zero)
        old.pop_back();
    Monstey_Require(Monstey_ArchiveVirtual(old) == old_stem, "VEGG source identity differs from template");
    auto replacement = std::string(new_stem);
    std::replace(replacement.begin(), replacement.end(), '/', '\\');
    if (zero)
        replacement.push_back(0);
    Monstey_Require(replacement.size() <= 65535, "VEGG path exceeds uint16");
    B out(raw.begin(), raw.begin() + 8);
    Monstey_AppendU16(out, static_cast<std::uint16_t>(replacement.size()));
    out.insert(out.end(), replacement.begin(), replacement.end());
    out.insert(out.end(), raw.begin() + 10 + n, raw.end());
    return out;
}
J Monstey_CBinding(const B &inst, const B &w, const std::string &resource, std::uint32_t owner) {
    Monstey_Require(inst.size() >= 76 && Monstey_U32(inst, 8) == Monstey_PathHash(resource),
                    "INST resource identity differs from private stem");
    constexpr std::uint32_t prefix[]{1, 0, 1, 1, 0, 0, 0, 0};
    Monstey_Require(w.size() >= 44 && std::memcmp(w.data(), "WDSC", 4) == 0 && Monstey_U32(w, 4) == 9,
                    "Expected qualified binding WDSC9");
    for (std::size_t i = 0; i < 8; ++i)
        Monstey_Require(Monstey_U32(w, 8 + 4 * i) == prefix[i], "Private WDSC is not one prefab binding");
    auto n = Monstey_U32(w, 40);
    auto at = 44ull + n;
    Monstey_Require(at + 152 == w.size(), "Single binding graph extent differs");
    auto names = std::span<const std::uint8_t>(w).subspan(44, n);
    Monstey_Require(Monstey_ArchiveVirtual(Monstey_CString(names, Monstey_U32(w, at + 72))) == resource &&
                        Monstey_U32(w, at + 64) == owner,
                    "Private binding resource or native owner key differs");
    constexpr std::uint32_t group[]{0, 0, 1, 0, 1, 0};
    for (std::size_t i = 0; i < 6; ++i)
        Monstey_Require(Monstey_U32(w, at + 76 + 4 * i) == group[i], "Private binding owner group differs");
    for (std::size_t i = 0; i < 13; ++i)
        Monstey_Require(Monstey_U32(w, at + 100 + 4 * i) == 0, "Private binding has a nonempty scene graph");
    for (std::size_t i = 0; i < 8; ++i)
        Monstey_Require(Monstey_U32(w, at + 24 + 4 * i) ==
                            std::bit_cast<std::uint32_t>(i == 6 || i == 7 ? 1.f : 0.f),
                        "Private binding outer transform is not identity");
    std::optional<std::size_t> selected;
    for (std::size_t i = 0; i < Monstey_U32(inst, 12); ++i)
        if (Monstey_U32(inst, 56 + 44 * i) == owner) {
            Monstey_Require(!selected, "Ambiguous private render owner");
            selected = i;
        }
    Monstey_Require(selected.has_value(), "Private render owner is absent");
    auto first = Monstey_U32(inst, 56 + 44 * (*selected) + 32),
         count = Monstey_U32(inst, 56 + 44 * (*selected) + 36);
    auto total = Monstey_U32(inst, 24);
    Monstey_Require(count > 0 && first <= total && count <= total - first,
                    "Private owner range is empty or outside table");
    auto instances = inst.size() - Monstey_U32(inst, 52) - 80ull * total;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        float lo = std::numeric_limits<float>::infinity(), hi = -lo;
        for (std::size_t i = first; i < std::size_t(first) + count; ++i) {
            auto p = instances + 80 * i;
            lo = std::min(lo, std::bit_cast<float>(Monstey_U32(inst, p + 16 + 4 * axis)));
            hi = std::max(hi, std::bit_cast<float>(Monstey_U32(inst, p + 28 + 4 * axis)));
        }
        Monstey_Require(Monstey_U32(w, at + 4 * axis) == std::bit_cast<std::uint32_t>(lo) &&
                            Monstey_U32(w, at + 12 + 4 * axis) == std::bit_cast<std::uint32_t>(hi),
                        "Private WDSC bounds do not match actual owned leaves");
    }
    return J::object({{"resource", resource},
                      {"owner_key", owner},
                      {"owned_instance_range", J::array({first, count})},
                      {"identity_transform", true},
                      {"exact_leaf_AABB_union", true}});
}
J Monstey_CPrivate(const J &a) {
    Monstey_CKeys(a, {"template", "inst", "dependencies", "wldclnt", "binding_proof", "owner_key", "stem",
                      "output", "schema_source", "marker_name", "position", "quaternion"});
    auto position = a.at("position");
    Monstey_Require(position.is_array() && position.size() == 3,
                    "Private marker position needs three coordinates");
    for (auto &value : position.as_array())
        Monstey_Require(value.is_number() && std::isfinite(value.as_number()),
                        "Private marker position is not finite");
    if (a.contains("quaternion")) {
        auto &q = a.at("quaternion");
        Monstey_Require(q.is_array() && q.size() == 4, "Private marker quaternion needs XYZW");
        double norm{};
        for (auto &value : q.as_array()) {
            Monstey_Require(value.is_number() && std::isfinite(value.as_number()),
                            "Private marker quaternion is not finite");
            norm += value.as_number() * value.as_number();
        }
        Monstey_Require(norm > 0, "Private marker quaternion is zero");
    }
    Monstey_CInputs inputs;
    auto template_path = Monstey_CPath(a.at("template"));
    auto manifest = inputs.Json(template_path);
    auto old = Monstey_ArchiveVirtual(manifest.at("template").as_string()),
         stem = Monstey_ArchiveVirtual(a.at("stem").as_string());
    Monstey_Require(stem.starts_with("worlds/retail/") &&
                        stem.find("/sectionsandlayers/") != std::string::npos && stem != old,
                    "Expected a new private native section stem");
    std::map<std::string, B> source;
    for (auto &row : manifest.at("resources").as_array()) {
        auto path = Monstey_ArchiveVirtual(row.at("resource_path").as_string());
        Monstey_Require(!source.contains(path), "Duplicate template resource");
        source.emplace(path, inputs.Verified(row));
    }
    auto bundle = Monstey_ReadBundle(source.at(old + ".bndl"));
    auto obj = source.at(old + ".objres");
    auto owners = Monstey_ObjresOwners(obj, false);
    std::multiset<std::string> classes;
    for (auto &row : owners.as_array())
        classes.insert(row.at("class_name").as_string());
    Monstey_Require(classes ==
                        std::multiset<std::string>{"MemoryReporter", "WorldCollision", "WorldInitializer"},
                    "Template is not a qualified empty section owner set");
    auto &base = source.at(old + ".inst");
    Monstey_Require(base.size() == 76 && std::memcmp(base.data(), "PRFB", 4) == 0 &&
                        Monstey_U32(base, 4) == 8 &&
                        std::all_of(base.begin() + 12, base.end(), [](auto v) { return v == 0; }),
                    "Private template has nonempty PRFB data");
    auto &empty = source.at(old + ".wldclnt");
    Monstey_Require(empty.size() == 96 && std::memcmp(empty.data(), "WDSC", 4) == 0 &&
                        Monstey_U32(empty, 4) == 9 &&
                        std::all_of(empty.begin() + 8, empty.end(), [](auto v) { return v == 0; }),
                    "Private template WDSC is not empty");
    auto inst_path = Monstey_CPath(a.at("inst")), wdsc_path = Monstey_CPath(a.at("wldclnt")),
         proof_path = Monstey_CPath(a.at("binding_proof")), dep_path = Monstey_CPath(a.at("dependencies"));
    auto inst = inputs.Load(inst_path), wdsc = inputs.Load(wdsc_path);
    auto proof = inputs.Json(proof_path), closure = inputs.Json(dep_path);
    (void)Monstey_WorldCall("sow_inst_inspect",
                            J::object({{"source", Monstey_CPathText(inst_path)}, {"limit", 0}}));
    auto owner = a.at("owner_key").as_uint();
    Monstey_Require(owner <= 0xffffffffull, "Owner key exceeds DWORD");
    auto binding = Monstey_CBinding(inst, wdsc, stem + ".inst", static_cast<std::uint32_t>(owner));
    if (proof.contains("inst_sha256"))
        Monstey_Require(proof.at("inst_sha256").as_string() == Monstey_HashSha256(inst),
                        "Binding proof identifies another INST");
    auto marker = a.value("marker_name", "Monstey_CourtyardProbe_TP").as_string();
    if (a.contains("schema_source")) {
        auto schema = Monstey_CPath(a.at("schema_source"));
        (void)inputs.Load(schema);
        auto spec = J::object(
            {{"objects", J::array({J::object({{"class_name", "TeleportPoint"},
                                              {"name", marker},
                                              {"position", a.at("position")},
                                              {"quaternion", a.value("quaternion", J::array({0, 0, 0, 1}))},
                                              {"properties", J::object()},
                                              {"schema_source", Monstey_CPathText(schema)}})})}});
        obj = Monstey_ObjresApply(obj, spec).first;
    }
    std::map<std::string, B> payloads;
    std::vector<std::string> order;
    std::map<std::string, J> native;
    J::Array unchanged, retargeted, missing;
    Monstey_Bundle rebuilt{bundle.kind, {}};
    for (auto &row : bundle.resources) {
        Monstey_Require(row.resource.starts_with(old + "."), "Foreign resource in private section template");
        auto suffix = row.resource.substr(old.size()), path = stem + suffix;
        if (!source.contains(row.resource)) {
            Monstey_Require(row.size_hint == 0, "Nonempty template payload absent");
            missing.push_back(path);
            rebuilt.resources.push_back({path, 0});
            continue;
        }
        auto data = source.at(row.resource);
        if (suffix == ".inst")
            data = inst;
        else if (suffix == ".wldclnt")
            data = wdsc;
        else if (suffix == ".objres")
            data = obj;
        else if (suffix == ".scripts")
            data = Monstey_CScript(data, path);
        else if (suffix == ".veggrid")
            data = Monstey_CVegetation(data, old, stem);
        (data == source.at(row.resource) ? unchanged : retargeted).push_back(suffix);
        Monstey_Require(data.size() <= 0xffffffffull, "Private payload exceeds DWORD");
        rebuilt.resources.push_back({path, static_cast<std::uint32_t>(data.size())});
        payloads.emplace(path, std::move(data));
        order.push_back(path);
    }
    auto closure_resources = closure.value("resources", J::array());
    for (auto &row : closure_resources.as_array()) {
        auto path = Monstey_ArchiveVirtual(row.at("resource_path").as_string());
        auto bytes = inputs.Verified(row);
        Monstey_Require(!path.starts_with("worlds/retail/") && !payloads.contains(path),
                        "Native dependency path conflicts with private section");
        Monstey_Require(bytes.size() <= 0xffffffffull, "Dependency exceeds DWORD");
        rebuilt.resources.push_back({path, static_cast<std::uint32_t>(bytes.size())});
        payloads.emplace(path, std::move(bytes));
        order.push_back(path);
        native.emplace(path, row);
    }
    auto external = closure.value("external_dependencies", J::array());
    std::set<std::string> external_names;
    for (auto &row : external.as_array()) {
        auto path = Monstey_ArchiveVirtual(row.at("resource_path").as_string());
        auto size = row.contains("bytes") ? row.at("bytes").as_uint() : row.at("size_hint").as_uint();
        Monstey_Require(!payloads.contains(path) && external_names.insert(path).second && size > 0 &&
                            size <= 0xffffffffull,
                        "External dependency is conflicting or has no qualified size");
        rebuilt.resources.push_back({path, static_cast<std::uint32_t>(size)});
    }
    std::vector<Monstey_EmbeddedResource> children;
    for (auto &path : order)
        if (!path.ends_with(".tex0") && !path.ends_with(".texuhd"))
            children.push_back({path, payloads.at(path)});
    payloads.emplace(stem + ".embb", Monstey_BuildEmbedded(children));
    payloads.emplace(stem + ".bndl", Monstey_BuildBundle(rebuilt));
    inputs.Verify();
    Monstey_COutput output(Monstey_CPath(a.at("output")), inputs);
    J::Array resources, allow;
    for (auto &[path, data] : payloads) {
        J row;
        if (native.contains(path)) {
            row = native.at(path);
            row["role"] = "native_dependency";
            row["unmodified"] = true;
        } else {
            auto disk = output.Save("assets/" + path, data);
            row = Monstey_CRecord(disk, data);
            row["role"] = "private_world_section";
            allow.push_back(path);
        }
        row["resource_path"] = path;
        resources.push_back(row);
    }
    auto result = J::object({{"status", "PRIVATE_SECTION_COMPONENT_RUNTIME_PENDING"},
                             {"ready_for_package", true},
                             {"resources", resources},
                             {"preload_root", stem},
                             {"external_dependencies", external},
                             {"suggested_world_section", stem + ".bndl"},
                             {"marker_name", marker},
                             {"game_written", false},
                             {"archive_built", false},
                             {"runtime_tested", false}});
    auto audit = J::object({{"status", "PRIVATE_SECTION_STRUCTURAL_CANDIDATE"},
                            {"new_stem", stem},
                            {"render_binding_validation", binding},
                            {"unchanged_payload_suffixes", unchanged},
                            {"retargeted_payload_suffixes", retargeted},
                            {"zero_size_absent_resources", missing},
                            {"native_dependency_sources_not_copied", true},
                            {"collision_generated", false},
                            {"navigation_generated", false},
                            {"game_written", false}});
    auto proof_file = output.SaveJson("proof.json", audit);
    auto proof_bytes = Monstey_ReadFile(proof_file);
    result["proof"] = Monstey_CRecord(proof_file, proof_bytes);
    output.SaveJson("resource-manifest.json", result);
    output.SaveJson("allowlist-proposal.json",
                    J::object({{"paths", allow}, {"shared_packager_modified", false}}));
    inputs.Verify();
    output.committed = true;
    return result;
}
} // namespace
J Monstey_WorldCompositionCatalog() {
    auto path = J::object({{"type", "string"}, {"format", "path"}}), text = J::object({{"type", "string"}});
    auto vec3 = J::object({{"type", "array"},
                           {"items", J::object({{"type", "number"}})},
                           {"minItems", 3},
                           {"maxItems", 3}}),
         quat = vec3;
    quat["minItems"] = 4;
    quat["maxItems"] = 4;
    auto schema = Monstey_CSchema(
        {{"template", path},
         {"inst", path},
         {"dependencies", path},
         {"wldclnt", path},
         {"binding_proof", path},
         {"owner_key",
          J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", std::uint64_t{0xffffffff}}})},
         {"stem", text},
         {"output", path},
         {"schema_source", path},
         {"marker_name", text},
         {"position", vec3},
         {"quaternion", quat}},
        {"template", "inst", "dependencies", "wldclnt", "binding_proof", "owner_key", "stem", "output",
         "position"});
    return J::array({J::object({{"name", "sow_private_section_build"},
                                {"description", "Native source-verified private world section composition"},
                                {"status", "complete"},
                                {"available", true},
                                {"implemented", true},
                                {"inputSchema", schema},
                                {"implementation_language", "C++20"},
                                {"runtime_verified", false}})});
}
J Monstey_WorldCompositionCall(std::string_view name, const J &a) {
    if (name == "sow_private_section_build")
        return Monstey_CPrivate(a);
    throw Monstey_Error("world_composition_pending", "Native composition operation is not implemented");
}
} // namespace Monstey