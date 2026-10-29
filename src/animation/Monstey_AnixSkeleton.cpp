#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
namespace Monstey_AnixPrivate
{

Monstey_ASkeleton Monstey_AReadSkeleton(std::span<const std::uint8_t> raw)
{
    Monstey_ARequire(raw.size() >= 64 && raw.size() <= Monstey_Limit &&
                         std::equal(raw.begin(), raw.begin() + 4, "SKEL"),
                     "Expected bounded SKEL17 skeleton");
    Monstey_ASkeleton s;
    s.raw.assign(raw.begin(), raw.end());
    for (std::size_t i = 0; i < 16; ++i)
        s.header[i] = Monstey_AU32(raw, 4 * i);
    Monstey_ARequire(s.header[1] == 17 && s.header[2] == 64, "Unsupported skeleton animation table profile");
    constexpr std::array<std::uint32_t, 7> tags{0, 1, 2, 3, 4, 5, 0x99};
    for (std::size_t i = 0; i < tags.size(); ++i)
    {
        B token;
        Monstey_AppendU32(token, 0xdcdcff00 | tags[i]);
        auto found = std::search(raw.begin(), raw.end(), token.begin(), token.end());
        Monstey_ARequire(found != raw.end(), "Missing SKEL section marker");
        s.marks[i] = static_cast<std::size_t>(found - raw.begin());
        Monstey_ARequire(std::search(found + 1, raw.end(), token.begin(), token.end()) == raw.end(),
                         "Ambiguous SKEL section marker");
    }
    Monstey_ARequire(std::is_sorted(s.marks.begin(), s.marks.end()) && s.marks.front() >= 64,
                     "SKEL section order differs");
    const auto base = s.marks[0] + 4;
    const std::uint64_t end64 = static_cast<std::uint64_t>(base) + s.header[3], count = s.header[7];
    Monstey_ARequire(end64 <= s.marks[1] && count <= Monstey_SampleLimit && s.marks[1] + 4 + 4 * count <= s.marks[2] &&
                         s.marks[2] + 4 + 4 * count == s.marks[3],
                     "SKEL animation list extents differ");
    const auto end = static_cast<std::size_t>(end64);
    s.table = Monstey_ASlice(raw, base, s.header[3]);
    s.prefix = Monstey_ASlice(raw, 64, base - 64);
    s.rig = Monstey_ASlice(raw, end, s.marks[1] - end);
    s.lods = Monstey_ASlice(raw, s.marks[1] + 4 + 4 * count, s.marks[2] - (s.marks[1] + 4 + 4 * count));
    s.tail = Monstey_ASlice(raw, s.marks[3], raw.size() - s.marks[3]);
    std::optional<std::uint32_t> previous;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto p = Monstey_AU32(raw, s.marks[1] + 4 + 4 * i), type = Monstey_AU32(raw, s.marks[2] + 4 + 4 * i);
        auto path = Monstey_AString(s.table, p, s.table.size());
        const auto hash = Monstey_PathHash(path);
        Monstey_ARequire(!previous || hash >= *previous, "Existing SKEL animation paths are not hash ordered");
        previous = hash;
        s.pointers.push_back(p);
        s.types.push_back(type);
        s.paths.push_back(std::move(path));
    }
    return s;
}

J Monstey_ASkeletonJson(const Monstey_ASkeleton &s)
{
    J paths = J::array();
    for (std::size_t i = 0; i < s.paths.size(); ++i)
        paths.as_array().push_back(J::object({{"index", static_cast<std::uint64_t>(i)},
                                              {"path", s.paths[i]},
                                              {"path_hash", Monstey_Hex(Monstey_PathHash(s.paths[i]))},
                                              {"type", s.types[i]},
                                              {"string_offset", s.pointers[i]}}));
    return J::object({{"format", "SKEL17"},
                      {"source_sha256", Monstey_HashSha256(s.raw)},
                      {"animation_count", s.header[7]},
                      {"model_strings_count", s.header[4]},
                      {"bone_count", s.header[10]},
                      {"qualified_for_append", s.header[10] == 128},
                      {"animations", paths},
                      {"hash_sorted", true},
                      {"prefix_sha256", Monstey_HashSha256(s.prefix)},
                      {"rig_sha256", Monstey_HashSha256(s.rig)},
                      {"lods_sha256", Monstey_HashSha256(s.lods)},
                      {"tail_sha256", Monstey_HashSha256(s.tail)},
                      {"runtime_verified", false}});
}

} // namespace Monstey_AnixPrivate
using namespace Monstey_AnixPrivate;

J Monstey_AnixSkeletonInspect(std::span<const std::uint8_t> bytes)
{
    return Monstey_ASkeletonJson(Monstey_AReadSkeleton(bytes));
}

Monstey_AnixSkeletonResult Monstey_AnixSkeletonRegister(std::span<const std::uint8_t> bytes, std::string_view resource)
{
    const auto before = Monstey_AReadSkeleton(bytes);
    Monstey_ARequire(before.header[10] == 128, "Only the qualified 128-bone SKEL17 append profile is writable");
    Monstey_AName(resource);
    std::string path(resource);
    for (auto &c : path)
        if (c == '/')
            c = '\\';
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    Monstey_ARequire(lower.starts_with("characters\\animationcharacters\\") && lower.ends_with(".anix") &&
                         lower.find("..") == std::string::npos && lower.find(':') == std::string::npos,
                     "Expected native relative animation library resource path");
    const auto hash = Monstey_PathHash(path);
    for (const auto &old : before.paths)
        Monstey_ARequire(Monstey_PathHash(old) != hash, "Duplicate animation path hash");
    struct Entry
    {
        std::uint32_t hash, pointer, type;
    };
    std::vector<Entry> entries;
    for (std::size_t i = 0; i < before.paths.size(); ++i)
        entries.push_back({Monstey_PathHash(before.paths[i]), before.pointers[i], before.types[i]});
    entries.push_back({hash, Monstey_A32(before.table.size()), 1});
    std::stable_sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) { return a.hash < b.hash; });
    auto h = before.header;
    h[3] = Monstey_A32(static_cast<std::uint64_t>(h[3]) + path.size() + 1);
    h[4] = Monstey_A32(static_cast<std::uint64_t>(h[4]) + 1);
    h[7] = Monstey_A32(static_cast<std::uint64_t>(h[7]) + 1);
    B out;
    for (auto v : h)
        Monstey_AppendU32(out, v);
    Monstey_AAppend(out, before.prefix);
    Monstey_AAppend(out, before.table);
    out.insert(out.end(), path.begin(), path.end());
    out.push_back(0);
    Monstey_AAppend(out, before.rig);
    Monstey_AppendU32(out, 0xdcdcff01);
    for (const auto &e : entries)
        Monstey_AppendU32(out, e.pointer);
    Monstey_AAppend(out, before.lods);
    Monstey_AppendU32(out, 0xdcdcff02);
    for (const auto &e : entries)
        Monstey_AppendU32(out, e.type);
    Monstey_AAppend(out, before.tail);
    const auto after = Monstey_AReadSkeleton(out);
    Monstey_ARequire(after.prefix == before.prefix && after.rig == before.rig && after.lods == before.lods &&
                         after.tail == before.tail &&
                         std::equal(before.table.begin(), before.table.end(), after.table.begin()),
                     "Opaque SKEL regions changed");
    std::size_t old_index = 0, new_index = 0;
    for (std::size_t i = 0; i < after.paths.size(); ++i)
    {
        if (Monstey_PathHash(after.paths[i]) == hash)
        {
            new_index = i;
            continue;
        }
        Monstey_ARequire(old_index < before.paths.size() && after.paths[i] == before.paths[old_index] &&
                             after.types[i] == before.types[old_index],
                         "Existing SKEL library/type changed");
        ++old_index;
    }
    Monstey_ARequire(old_index == before.paths.size(), "Existing SKEL animation list changed");
    J changed = J::array();
    for (std::size_t i = 0; i < h.size(); ++i)
        if (h[i] != before.header[i])
            changed.as_array().emplace_back(static_cast<std::uint64_t>(i));
    auto report = J::object({{"source_sha256", Monstey_HashSha256(bytes)},
                             {"output_sha256", Monstey_HashSha256(out)},
                             {"added_path", path},
                             {"added_path_hash", Monstey_Hex(hash)},
                             {"added_index", static_cast<std::uint64_t>(new_index)},
                             {"added_type", 1},
                             {"old_animation_count", before.header[7]},
                             {"new_animation_count", after.header[7]},
                             {"changed_header_dwords", changed},
                             {"string_table_original_prefix", true},
                             {"prefix_byte_exact", true},
                             {"rig_byte_exact", true},
                             {"lods_byte_exact", true},
                             {"tail_byte_exact", true},
                             {"hash_sorted", true},
                             {"runtime_verified", false}});
    return {std::move(out), std::move(report)};
}

} // namespace Monstey
