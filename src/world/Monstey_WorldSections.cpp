#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

J Monstey_WSection(const B &raw, const std::string &requested)
{
    auto magic = raw.size() >= 4 ? std::string(raw.begin(), raw.begin() + 4) : "";
    auto kind = requested.empty() ? (magic == "WDSC"   ? "WDSC9"
                                     : magic == "MESH" ? "MESH5"
                                     : magic == "HKAI" ? "HKAI3"
                                                       : "")
                                  : requested;
    J::Array blocks;
    J details = J::object();
    auto block = [&](const char *name, std::size_t at, std::size_t n)
    {
        Monstey_CheckRange(raw, at, n);
        blocks.push_back(J::object({{"name", name},
                                    {"offset", static_cast<std::uint64_t>(at)},
                                    {"bytes", static_cast<std::uint64_t>(n)},
                                    {"sha256", Monstey_HashSha256(std::span(raw).subspan(at, n))}}));
    };
    if (kind == "WDSC9")
    {
        Monstey_Require(raw.size() >= 44 && magic == "WDSC" && Monstey_U32(raw, 4) == 9, "Expected WDSC9");
        J::Array prefix;
        bool complex = false;
        for (std::size_t j = 0; j < 9; ++j)
        {
            auto word = Monstey_U32(raw, 8 + 4 * j);
            prefix.push_back(word);
            if (j && word)
                complex = true;
        }
        block("header", 0, 8);
        block("render_prefix_counts", 8, 36);
        std::size_t at = 44;
        details["prefix_counts"] = prefix;
        if (!complex)
        {
            auto groups = Monstey_U32(raw, 8);
            Monstey_Require(groups <= (raw.size() - at) / 12, "WDSC group count outside extent");
            for (std::uint32_t j = 0; j < groups; ++j)
            {
                if (Monstey_U32(raw, at) || Monstey_U32(raw, at + 4) || Monstey_U32(raw, at + 8))
                {
                    complex = true;
                    break;
                }
                block("empty_render_group", at, 12);
                at += 12;
            }
            if (!complex)
            {
                Monstey_CheckRange(raw, at, 52);
                for (auto j : {2u, 5u, 10u, 12u})
                    Monstey_Require(Monstey_U32(raw, at + 4 * j) == 0, "WDSC reserved scene counter differs");
                block("scene_counts", at, 52);
                details["scene_header_offset"] = static_cast<std::uint64_t>(at);
                at += 52;
            }
        }
        block(complex ? "render_dictionaries_and_scene_preserved" : "scene_payload_preserved", at, raw.size() - at);
        details["full_render_graph_decoded"] = false;
    }
    else if (kind == "MESH5")
    {
        Monstey_Require(raw.size() >= 32 && magic == "MESH" && Monstey_U32(raw, 4) == 5, "Expected MESH5");
        auto dictionary = Monstey_U32(raw, 28);
        Monstey_CheckRange(raw, 32, dictionary);
        block("header", 0, 32);
        block("raw_dictionary", 32, dictionary);
        block("geometry_preserved", 32 + dictionary, raw.size() - 32 - dictionary);
        details["full_geometry_decoder"] = false;
    }
    else if (kind == "HKAI3")
    {
        Monstey_Require(raw.size() >= 28 && magic == "HKAI" && Monstey_U32(raw, 4) == 3 &&
                            Monstey_U32(raw, 8) == 0xffffd00du,
                        "Expected HKAI3");
        auto bytes = Monstey_U32(raw, 12);
        Monstey_Require(20ull + bytes == raw.size() && Monstey_U32(raw, 16ull + bytes) == 0xffffdeadu,
                        "HKAI extent or end marker invalid");
        Monstey_Require(bytes >= 8 && std::memcmp(raw.data() + 20, "TAG0", 4) == 0, "Expected Havok TAG0 payload");
        auto big = [&](std::size_t p)
        {
            Monstey_CheckRange(raw, p, 4);
            return (std::uint32_t(raw[p]) << 24) | (std::uint32_t(raw[p + 1]) << 16) |
                   (std::uint32_t(raw[p + 2]) << 8) | raw[p + 3];
        };
        Monstey_Require((big(16) & 0x3fffffffu) == bytes, "TAG0 extent invalid");
        block("HKAI3_header", 0, 16);
        block("TAG0_header", 16, 8);
        std::size_t at = 24;
        while (at < 16ull + bytes)
        {
            auto size = big(at) & 0x3fffffffu;
            Monstey_Require(size >= 8 && size <= 16ull + bytes - at, "TAG0 child extent invalid");
            auto name = std::string(raw.begin() + static_cast<std::ptrdiff_t>(at + 4),
                                    raw.begin() + static_cast<std::ptrdiff_t>(at + 8));
            block(name.c_str(), at, size);
            at += size;
        }
        block("HKAI3_end", 16ull + bytes, 4);
        details["navmesh_regenerated"] = false;
    }
    else if (kind == "WorldPhysics")
    {
        Monstey_Require(raw.size() >= 8, "Truncated WorldPhysics");
        auto mappings = Monstey_U32(raw, 0), definitions = Monstey_U32(raw, 4);
        Monstey_Require(mappings <= (raw.size() - 8) / 6, "WorldPhysics mappings outside extent");
        std::size_t at = 8 + 6ull * mappings;
        block("counts_and_mappings", 0, at);
        for (std::uint32_t j = 0; j < definitions; ++j)
        {
            auto start = at;
            Monstey_CheckRange(raw, at, 6);
            auto n = Monstey_U16(raw, at + 4);
            at += 6;
            Monstey_CheckRange(raw, at, n);
            if (n)
                at += 8;
            at += n + 24;
            Monstey_CheckRange(raw, start, at - start);
            block("physical_definition", start, at - start);
        }
        Monstey_Require(at + 16 <= raw.size() && Monstey_U32(raw, at) == 0xfafab0b0u && Monstey_U32(raw, at + 4) == 2 &&
                            Monstey_U32(raw, raw.size() - 4) == 0xfafabfbfu,
                        "WorldPhysics shape-array envelope invalid");
        block("shape_array_preserved", at, raw.size() - at);
        details["shape_geometry_regenerated"] = false;
    }
    else
        throw Monstey_Error("world_format_unsupported", "Expected WDSC9, MESH5, HKAI3 or explicit WorldPhysics");
    return J::object({{"format", kind},
                      {"bytes", static_cast<std::uint64_t>(raw.size())},
                      {"sha256", Monstey_HashSha256(raw)},
                      {"blocks", blocks},
                      {"details", details},
                      {"roundtrip_byte_exact", true},
                      {"validation_scope", "Exact known envelope and preservation of opaque payload, not "
                                           "regenerated collision or navigation"}});
}

} // namespace Monstey_WorldPrivate

} // namespace Monstey
