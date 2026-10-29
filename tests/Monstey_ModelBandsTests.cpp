#include "Monstey_Model.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
using namespace Monstey;
namespace
{
using Monstey_Counts = std::array<std::uint64_t, 17>;
using Monstey_Key = std::pair<std::uint32_t, std::uint32_t>;
using Monstey_Meaning = std::map<Monstey_Key, Monstey_Counts>;
void Monstey_Check(bool ok, std::string_view message) { Monstey_Require(ok, message); }
void Monstey_Float(Monstey_Bytes &b, float v) { Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(v)); }
struct Monstey_Container
{
    std::array<std::uint32_t, 3> sizes{};
    std::array<std::uint16_t, 16> q{};
    std::vector<std::array<std::uint32_t, 10>> draws;
};
struct Monstey_Connection
{
    std::array<std::uint32_t, 4> words{};
    std::uint32_t flags = 0, material = 0;
    std::int32_t raw_map = -1, map = -1;
};
// Independent, bounded disk-table oracle. It never calls a production private parser
// and does not assume the ordinals assigned to expanded output containers.
struct Monstey_Oracle
{
    std::array<std::uint32_t, 18> h{};
    std::vector<std::uint32_t> names, thresholds;
    std::vector<Monstey_Connection> connections;
    std::vector<Monstey_Container> containers;
    std::vector<std::vector<std::uint32_t>> groups;
    Monstey_Bytes bones;
    explicit Monstey_Oracle(const Monstey_Bytes &b)
    {
        Monstey_Check(Monstey_U32(b, 0) == 0x48534d4d && Monstey_U32(b, 4) == 17, "oracle: MMSH17 header");
        const auto ds = Monstey_U32(b, 8), is = Monstey_U32(b, 12), das = Monstey_U32(b, 16);
        Monstey_Check(20ull + ds + is + das == b.size(), "oracle: section extents");
        const auto info = 20ull + ds;
        for (std::size_t k = 0; k < h.size(); ++k) h[k] = Monstey_U32(b, static_cast<std::size_t>(info) + k * 4);
        const auto nc = Monstey_U32(b, 24), nb = Monstey_U32(b, 28);
        Monstey_Check(nc == h[2] && nb == h[3] && 12ull + nc * 8ull + nb * 12ull == ds, "oracle: descriptor extent/count");
        std::size_t p = static_cast<std::size_t>(info) + 72;
        for (std::size_t i = 0; i < h[1]; ++i, p += 4) names.push_back(Monstey_U32(b, p));
        for (std::size_t i = 0; i < nc; ++i, p += 16)
        {
            Monstey_Connection c;
            for (std::size_t k = 0; k < 4; ++k) c.words[k] = Monstey_U32(b, p + k * 4);
            c.flags = Monstey_U32(b, 32 + i * 4);
            c.raw_map = c.map = static_cast<std::int32_t>(Monstey_U32(b, 32ull + nc * 4ull + i * 4));
            c.material = c.words[2];
            Monstey_Check(c.words[0] < h[1] && (c.raw_map < 0 || static_cast<std::uint32_t>(c.raw_map) < nb),
                          "oracle: piece/container reference");
            connections.push_back(c);
        }
        for (std::size_t i = connections.size(); i-- > 0;)
            if ((connections[i].flags & 4u) != 0)
                for (std::size_t j = i + 1; j < connections.size(); ++j)
                    if (connections[j].map >= 0)
                    {
                        connections[i].map = connections[j].map;
                        // Native forward material overwrite uses the selected RAW row.
                        connections[i].material = connections[j].words[2];
                        break;
                    }
        std::vector<std::uint32_t> draw_counts;
        for (std::size_t c = 0; c < nb; ++c, p += 48)
        {
            Monstey_Container container;
            for (std::size_t k = 0; k < 3; ++k)
            {
                container.sizes[k] = Monstey_U32(b, p + k * 4);
                Monstey_Check(container.sizes[k] == Monstey_U32(b, 32ull + nc * 8ull + (k * nb + c) * 4),
                              "oracle: descriptor/container size");
            }
            draw_counts.push_back(Monstey_U32(b, p + 12));
            for (std::size_t q = 0; q < 16; ++q) container.q[q] = Monstey_U16(b, p + 16 + q * 2);
            containers.push_back(container);
        }
        std::size_t draws = 0;
        for (std::size_t c = 0; c < nb; ++c)
            for (std::size_t d = 0; d < draw_counts[c]; ++d, p += 40)
            {
                std::array<std::uint32_t, 10> row{};
                for (std::size_t k = 0; k < row.size(); ++k) row[k] = Monstey_U32(b, p + k * 4);
                Monstey_Check(row[2] > 0 && row[2] <= 65535 && row[4] % 3 == 0 && row[4] / 3 <= 65535,
                              "oracle: draw packed limits");
                containers[c].draws.push_back(row); ++draws;
            }
        Monstey_Check(draws == h[4] && Monstey_U32(b, p) == 0x8e3e068e, "oracle: draw count/LOD marker"); p += 4;
        std::vector<std::uint32_t> refs;
        for (std::size_t i = 0; i < h[6]; ++i, p += 4)
        {
            const auto ref = Monstey_U32(b, p);
            Monstey_Check(ref < connections.size(), "oracle: group connection reference"); refs.push_back(ref);
        }
        const auto ng = static_cast<std::size_t>(h[7]) + h[8];
        for (std::size_t i = 0; i < ng; ++i, p += 4) thresholds.push_back(Monstey_U32(b, p));
        std::size_t first = 0;
        for (std::size_t i = 0; i < ng; ++i, p += 4)
        {
            const auto count = Monstey_U32(b, p);
            Monstey_Check(first <= refs.size() && count <= refs.size() - first, "oracle: group reference slice");
            groups.emplace_back(refs.begin() + static_cast<std::ptrdiff_t>(first), refs.begin() + static_cast<std::ptrdiff_t>(first + count));
            first += count;
        }
        Monstey_Check(first == refs.size() && Monstey_U32(b, p) == 0x37d749a6, "oracle: three parallel arrays/layout marker"); p += 4;
        for (std::size_t i = 0; i < h[9]; ++i)
        {
            const auto count = Monstey_U32(b, p); p += 4;
            Monstey_CheckRange(b, p, static_cast<std::size_t>(count) * 4); p += static_cast<std::size_t>(count) * 4;
        }
        Monstey_Check(Monstey_U32(b, p) == 0x93d9a424 && p + 4ull + h[17] == info + is, "oracle: bone extent");
        bones.assign(b.begin() + static_cast<std::ptrdiff_t>(p), b.begin() + static_cast<std::ptrdiff_t>(info + is));
        Monstey_Check(h[6] <= 65535 && ng <= 65535, "oracle: packed reference/group limit");
    }
    Monstey_Counts Bands(std::size_t c) const
    {
        Monstey_Counts counts{};
        const auto &container = containers.at(c);
        for (const auto &draw : container.draws)
        {
            const auto total = draw[4] / 3;
            if (container.draws.size() > 1) { counts[0] += total; continue; }
            std::uint32_t previous = 0;
            for (const auto q : container.q)
            {
                Monstey_Check(q >= previous && q <= total, "oracle: cumulative Q bounds"); previous = q;
            }
            std::size_t band = 0;
            for (std::uint32_t face = 0; face < total; ++face)
            {
                while (band < 16 && face >= container.q[band]) ++band;
                ++counts[band];
            }
        }
        return counts;
    }
    std::vector<Monstey_Meaning> Meanings(const std::map<std::size_t, Monstey_Counts> &overrides = {}) const
    {
        std::vector<Monstey_Meaning> result;
        for (const auto &group : groups)
        {
            Monstey_Meaning meaning;
            for (const auto ref : group)
            {
                const auto &c = connections.at(ref); if (c.map < 0) continue;
                const auto index = static_cast<std::size_t>(c.map);
                const auto counts = overrides.contains(index) ? overrides.at(index) : Bands(index);
                auto &sum = meaning[{c.words[0], c.material}];
                for (std::size_t i = 0; i < counts.size(); ++i) sum[i] += counts[i];
            }
            result.push_back(std::move(meaning));
        }
        return result;
    }
};
Monstey_Bytes Monstey_Skeleton()
{
    const std::string names("Root\0TrimRoot\0BandCoat\0BandTrim\0", 32);
    Monstey_Bytes b(96, 0);
    Monstey_PutU32(b, 0, 0x4c454b53); Monstey_PutU32(b, 4, 17);
    Monstey_PutU32(b, 12, static_cast<std::uint32_t>(names.size()));
    Monstey_PutU32(b, 32, 2); Monstey_PutU32(b, 40, 2);
    for (std::uint32_t piece = 0; piece < 2; ++piece)
    {
        Monstey_AppendU32(b, 3);
        for (std::uint32_t tier = 0; tier < 3; ++tier)
        {
            b.push_back(static_cast<std::uint8_t>(tier != 2)); b.push_back(static_cast<std::uint8_t>(piece + tier));
            Monstey_Float(b, static_cast<float>(tier * 500)); Monstey_AppendU32(b, 7 + piece);
            Monstey_AppendU32(b, tier == 2 ? 0xffffffffu : piece);
        }
    }
    Monstey_AppendU32(b, 0xdcdcff00); b.insert(b.end(), names.begin(), names.end());
    for (std::uint32_t bone = 0; bone < 2; ++bone)
    {
        Monstey_AppendU32(b, bone == 0 ? 0 : 5); b.push_back(1);
        for (const auto v : {0.0f, bone == 0 ? 0.0f : 25.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}) Monstey_Float(b, v);
        Monstey_AppendU32(b, bone == 0 ? 1 : 0);
    }
    b.insert(b.end(), {0xde, 0xad, 0xbe, 0xef, 0x80, 0x42}); return b;
}
Monstey_Bytes Monstey_Mesh(bool aliases = false, bool multi = false)
{
    Monstey_Bytes vb;
    const std::array<std::array<std::uint16_t, 3>, 8> positions{{{0, 0, 0}, {0x3c00, 0, 0}, {0, 0x3c00, 0},
        {0x3c00, 0x3c00, 0}, {0, 0, 0x3c00}, {0x3c00, 0, 0x3c00}, {0, 0x3c00, 0x3c00}, {0x3c00, 0x3c00, 0x3c00}}};
    for (const auto &position : positions)
    {
        for (const auto v : position) Monstey_AppendU16(vb, v);
        Monstey_AppendU16(vb, 0x3c00); vb.insert(vb.end(), {0, 0, 255, 0, 0, 1, 0, 0});
    }
    for (std::uint16_t v = 0; v < 8; ++v)
    {
        Monstey_AppendU16(vb, v % 2 ? 32767 : 0); Monstey_AppendU16(vb, (v / 2) % 2 ? 32767 : 0);
        for (const auto f : {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f}) Monstey_Float(vb, f);
        vb.insert(vb.end(), {static_cast<std::uint8_t>(12 + v), 33, 77, 255});
    }
    Monstey_Bytes ib;
    for (const auto i : {0u, 1u, 2u, 1u, 3u, 2u, 4u, 5u, 6u, 5u, 7u, 6u}) Monstey_AppendU16(ib, static_cast<std::uint16_t>(i));
    const Monstey_Bytes palette{0, 1};
    const std::uint32_t nc = aliases ? 3u : 4u;
    const std::array<std::uint32_t, 4> flags = aliases ? std::array<std::uint32_t, 4>{4, 4, 0, 0} : std::array<std::uint32_t, 4>{};
    const std::array<std::int32_t, 4> maps = aliases ? std::array<std::int32_t, 4>{-1, -1, 0, -1} : std::array<std::int32_t, 4>{0, 1, 0, -1};
    const std::array<std::uint32_t, 4> pieces{0, 1, 0, 1}, materials{7, 8, 9, 10};
    const std::array<std::uint32_t, 2> draws{multi ? 2u : 1u, 1u};
    Monstey_Bytes desc;
    for (const auto v : {0xebaec3fau, nc, 2u}) Monstey_AppendU32(desc, v);
    for (std::size_t i = 0; i < nc; ++i) Monstey_AppendU32(desc, flags[i]);
    for (std::size_t i = 0; i < nc; ++i) Monstey_AppendU32(desc, static_cast<std::uint32_t>(maps[i]));
    for (const auto n : {vb.size(), ib.size(), palette.size()})
        for (const auto d : draws) Monstey_AppendU32(desc, static_cast<std::uint32_t>(n * d));
    const std::vector<std::vector<std::uint32_t>> groups = aliases
        ? std::vector<std::vector<std::uint32_t>>{{0, 1, 2}, {2}, {1, 0}, {}}
        : std::vector<std::vector<std::uint32_t>>{{0, 1}, {2, 3}, {0, 3}, {1}};
    std::uint32_t refs = 0; for (const auto &g : groups) refs += static_cast<std::uint32_t>(g.size());
    std::array<std::uint32_t, 18> h{};
    h[0] = 0x1a1541bc; h[1] = 2; h[2] = nc; h[3] = 2; h[4] = draws[0] + draws[1]; h[5] = draws[0];
    h[6] = refs; h[7] = 2; h[8] = 2; h[9] = 1; h[10] = 8; h[16] = 2; h[17] = 80;
    Monstey_Bytes info;
    for (const auto v : h) Monstey_AppendU32(info, v);
    Monstey_AppendU32(info, Monstey_NativeHash("BandCoat")); Monstey_AppendU32(info, Monstey_NativeHash("BandTrim"));
    for (std::size_t i = 0; i < nc; ++i)
        for (const auto v : {pieces[i], static_cast<std::uint32_t>(maps[i]), materials[i], 0xa1000000u + static_cast<std::uint32_t>(i)}) Monstey_AppendU32(info, v);
    for (const auto d : draws)
    {
        for (const auto v : {static_cast<std::uint32_t>(vb.size()) * d, static_cast<std::uint32_t>(ib.size()) * d,
                             static_cast<std::uint32_t>(palette.size()) * d, d}) Monstey_AppendU32(info, v);
        for (std::size_t q = 0; q < 16; ++q) Monstey_AppendU16(info, q == 0 ? 1 : q == 1 ? 3 : 4);
    }
    for (std::size_t c = 0; c < draws.size(); ++c)
        for (std::uint32_t d = 0; d < draws[c]; ++d)
            for (const auto v : {static_cast<std::uint32_t>(vb.size()) * d, 8u * d, 8u, 12u * d, 12u, 2u * d, 2u,
                                 0x12000000u + static_cast<std::uint32_t>(c), 0x34000000u + d, 0u}) Monstey_AppendU32(info, v);
    Monstey_AppendU32(info, 0x8e3e068e);
    for (const auto &g : groups) for (const auto ref : g) Monstey_AppendU32(info, ref);
    for (const auto threshold : {500.0f, 0.0f, 750.0f, -0.0f}) Monstey_Float(info, threshold);
    for (const auto &g : groups) Monstey_AppendU32(info, static_cast<std::uint32_t>(g.size()));
    Monstey_AppendU32(info, 0x37d749a6); Monstey_AppendU32(info, 8);
    for (const auto &a : std::array<std::array<std::uint8_t, 4>, 8>{{{0, 12, 0, 0}, {0, 4, 6, 0}, {0, 4, 7, 0},
        {1, 9, 4, 0}, {1, 2, 1, 0}, {1, 2, 2, 0}, {1, 2, 3, 0}, {1, 4, 5, 0}}}) info.insert(info.end(), a.begin(), a.end());
    Monstey_AppendU32(info, 0x93d9a424);
    for (std::uint32_t bone = 0; bone < 2; ++bone)
    {
        Monstey_AppendU32(info, 0x4000 + bone); Monstey_AppendU16(info, bone == 0 ? 0xffff : 0); Monstey_AppendU16(info, bone == 0 ? 1 : 0);
        for (const auto v : {0.0f, bone == 0 ? 0.0f : 25.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f}) Monstey_Float(info, v);
    }
    Monstey_Bytes data; Monstey_AppendU32(data, 0x95dbdb69);
    for (const auto &buffer : {vb, ib, palette}) for (const auto n : draws) for (std::uint32_t d = 0; d < n; ++d) data.insert(data.end(), buffer.begin(), buffer.end());
    Monstey_Bytes b;
    for (const auto v : {0x48534d4du, 17u, static_cast<std::uint32_t>(desc.size()), static_cast<std::uint32_t>(info.size()),
                         static_cast<std::uint32_t>(data.size())}) Monstey_AppendU32(b, v);
    b.insert(b.end(), desc.begin(), desc.end()); b.insert(b.end(), info.begin(), info.end()); b.insert(b.end(), data.begin(), data.end()); return b;
}
Monstey_Counts Monstey_CountBands(const std::vector<std::uint8_t> &bands)
{
    Monstey_Counts counts{}; for (const auto band : bands) { Monstey_Check(band <= 16, "test input band"); ++counts[band]; } return counts;
}
void Monstey_Closure(const Monstey_Oracle &a, const Monstey_Oracle &b,
                     const std::map<std::size_t, Monstey_Counts> &overrides, bool single_draw)
{
    Monstey_Check(a.Meanings(overrides) == b.Meanings(), "connection/group material-piece-band closure changed");
    Monstey_Check(a.names == b.names && a.thresholds == b.thresholds && a.h[7] == b.h[7] && a.h[8] == b.h[8] && a.bones == b.bones,
                  "piece hashes, raw A/B thresholds or bone bytes changed");
    Monstey_Check(b.connections.size() >= a.connections.size(), "original connection identities lost");
    for (std::size_t i = 0; i < a.connections.size(); ++i)
        Monstey_Check(a.connections[i].words == b.connections[i].words && a.connections[i].flags == b.connections[i].flags &&
                      a.connections[i].raw_map == b.connections[i].raw_map, "original raw descriptor/connection changed");
    for (std::size_t g = 0; g < a.groups.size(); ++g)
    {
        std::vector<std::uint32_t> an, bn;
        for (const auto r : a.groups[g]) if (a.connections[r].map < 0) an.push_back(r);
        for (const auto r : b.groups[g]) if (b.connections[r].map < 0) bn.push_back(r);
        Monstey_Check(an == bn, "null connection/group references changed");
    }
    if (single_draw)
    {
        std::set<Monstey_Key> affected;
        for (const auto &c : a.connections) if (c.map >= 0 && overrides.contains(static_cast<std::size_t>(c.map))) affected.emplace(c.words[0], c.material);
        for (const auto &c : b.connections) if (c.map >= 0 && affected.contains({c.words[0], c.material}))
            Monstey_Check(b.containers.at(static_cast<std::size_t>(c.map)).draws.size() == 1,
                          "banded partition emitted multi-draw container: native loader would discard Q");
    }
}
Monstey_Bytes Monstey_WithUnusedEmptyContainer(const Monstey_Bytes &raw)
{
    const auto ds = Monstey_U32(raw, 8), is = Monstey_U32(raw, 12), das = Monstey_U32(raw, 16);
    const auto descriptors = Monstey_U32(raw, 24), containers = Monstey_U32(raw, 28);
    const auto prefix = 12ull + descriptors * 8ull, infoAt = 20ull + ds;
    Monstey_Bytes desc(raw.begin() + 20, raw.begin() + static_cast<std::ptrdiff_t>(20 + prefix));
    Monstey_PutU32(desc, 8, containers + 1);
    for (std::size_t stream = 0; stream < 3; ++stream)
    {
        const auto at = 20 + prefix + stream * containers * 4ull;
        desc.insert(desc.end(), raw.begin() + static_cast<std::ptrdiff_t>(at),
                    raw.begin() + static_cast<std::ptrdiff_t>(at + containers * 4ull));
        Monstey_AppendU32(desc, 0);
    }
    const auto insert = 72ull + Monstey_U32(raw, infoAt + 4) * 4ull +
                        Monstey_U32(raw, infoAt + 8) * 16ull + containers * 48ull;
    Monstey_Bytes info(raw.begin() + static_cast<std::ptrdiff_t>(infoAt),
                       raw.begin() + static_cast<std::ptrdiff_t>(infoAt + insert));
    info.insert(info.end(), 48, 0);
    info.insert(info.end(), raw.begin() + static_cast<std::ptrdiff_t>(infoAt + insert),
                 raw.begin() + static_cast<std::ptrdiff_t>(infoAt + is));
    Monstey_PutU32(info, 12, containers + 1);
    Monstey_Bytes out;
    for (const auto v : {0x48534d4du, 17u, static_cast<std::uint32_t>(desc.size()),
                         static_cast<std::uint32_t>(info.size()), das}) Monstey_AppendU32(out, v);
    out.insert(out.end(), desc.begin(), desc.end()); out.insert(out.end(), info.begin(), info.end());
    out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(infoAt + is), raw.end());
    return out;
}
void Monstey_Repeat(Monstey_ModelPart &part, std::size_t triangles, std::uint8_t first_band, std::uint8_t second_band)
{
    const std::array<std::uint32_t, 3> first{part.indices[0], part.indices[1], part.indices[2]};
    part.indices.clear(); part.indices.reserve(triangles * 3); std::vector<std::uint8_t> bands; bands.reserve(triangles);
    for (std::size_t f = 0; f < triangles; ++f)
    {
        part.indices.insert(part.indices.end(), first.begin(), first.end()); bands.push_back(f < triangles / 2 ? first_band : second_band);
    }
    Monstey_ModelSetTriangleBands(part, bands);
}
std::size_t Monstey_ArmorPart(const Monstey_ModelScene &scene)
{
    for (std::size_t i = 0; i < scene.parts.size(); ++i) if (scene.parts[i].indices.size() / 3 == 24362) return i;
    throw std::runtime_error("actual armor lacks independently observed 24362-face part");
}
void Monstey_ArmorBands(const Monstey_ModelScene &scene, std::size_t index, std::uint64_t last)
{
    const auto &p = scene.parts.at(index); const auto c = Monstey_CountBands(p.triangle_bands);
    Monstey_Check(c[0] == 0 && c[1] == 23976 && c[2] == last &&
                  std::accumulate(c.begin(), c.end(), std::uint64_t{0}) == 23976 + last &&
                  p.triangle_band_topology_sha256 == Monstey_ModelTriangleTopologyHash(p.indices),
                  "observed armor interval [2,2] lost native membership");
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        const std::filesystem::path root = argc > 1 ? argv[1] : L"model-band-tests";
        const auto dir = root / (L"run-" + std::to_wstring(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(dir);
        const bool hasArmor = argc > 2;
        const std::filesystem::path armor = hasArmor ? argv[2] : L"";
        auto armor_skel = armor; armor_skel.replace_extension(L".skel"); if (argc > 3) armor_skel = argv[3];
        const auto source = Monstey_Mesh(), aliases = Monstey_Mesh(true), multis = Monstey_Mesh(false, true), skeleton = Monstey_Skeleton();
        const auto mesh = dir / L"two-piece.mesh", skel = dir / L"two-piece.skel", alias = dir / L"chained-alias.mesh", multi = dir / L"multi-draw.mesh";
        Monstey_WriteNewFile(mesh, source); Monstey_WriteNewFile(skel, skeleton); Monstey_WriteNewFile(alias, aliases); Monstey_WriteNewFile(multi, multis);
        const auto armor_hash = hasArmor ? Monstey_HashSha256(armor) : "",
                   skel_hash = hasArmor ? Monstey_HashSha256(armor_skel) : "";
        const Monstey_Oracle before(source);
        auto report = Monstey_Json::object({{"scope", "native file codec; independent synthetic association oracle; actual armor regression"},
            {"actual_inputs_supplied", hasArmor}, {"runtime_verified", false}, {"cases", Monstey_Json::array()}, {"armor_sha256", armor_hash}, {"armor_skel_sha256", skel_hash}});
        unsigned passed = 0, failed = 0;
        const auto test = [&](const char *name, const std::function<void()> &body) {
            try
            {
                body(); ++passed;
                report["cases"].as_array().push_back(Monstey_Json::object({{"name", name}, {"passed", true}}));
            }
            catch (const std::exception &error)
            {
                ++failed; std::cerr << name << ": " << error.what() << '\n';
                report["cases"].as_array().push_back(Monstey_Json::object({{"name", name}, {"passed", false}, {"error", error.what()}}));
            }
        };
        const auto reject = [&](const std::function<void()> &body, const std::filesystem::path &out) {
            bool rejected = false; try { body(); } catch (const std::exception &) { rejected = true; }
            Monstey_Check(rejected && !std::filesystem::exists(out), "invalid state published output");
        };
        test("independent_two_piece_fixture_all_eight_channels_nonzero_base_range", [&] {
            const auto s = Monstey_ModelReadScene(mesh, skel);
            Monstey_Check(s.parts.size() == 2 && s.bones.size() == 2 && s.bones[0].name == "Root" && s.bones[1].name == "TrimRoot", "fixture rig");
            for (const auto &p : s.parts)
            {
                for (const auto *name : {"POSITION", "NORMAL", "TANGENT", "BITANGENT", "UV0", "COLOR0", "JOINTS", "WEIGHTS_U8"})
                    Monstey_Check(p.channels.contains(name), std::string("missing fixture channel ") + name);
                Monstey_Check(p.triangle_bands == std::vector<std::uint8_t>({0, 1, 1, 2}), "fixture bands");
            }
            const auto &q = before.containers[0].q;
            Monstey_Check(q[0] == 1 && q[1] - q[0] == 2 && q[1] == 3, "base Q0 plus requested [1,1] must contain three triangles");
            Monstey_Check(Monstey_ModelReadRigidSceneBytes(source).parts[0].triangle_bands == s.parts[0].triangle_bands, "rigid decoder lost bands");
        });
        test("synthetic_noop_exact", [&] {
            const auto out = dir / L"noop.mesh";
            const auto r = Monstey_ModelWriteScene(Monstey_ModelReadScene(mesh, skel), mesh, out);
            Monstey_Check(r["byte_exact_noop"].as_bool() && Monstey_ReadFile(out) == source, "synthetic no-op differs");
        });
        if (hasArmor)
        {
        test("actual_armor_noop_exact_and_386_interval", [&] {
            const auto s = Monstey_ModelReadScene(armor, armor_skel); const auto part = Monstey_ArmorPart(s);
            Monstey_ArmorBands(s, part, 386);
            const Monstey_Oracle a(Monstey_ReadFile(armor)); const auto &q = a.containers.at(s.parts[part].container).q;
            Monstey_Check(q[0] == 0 && q[1] == 23976 && q[2] == 24362, "actual source Q differs");
            const auto out = dir / L"armor-noop.mesh"; const auto r = Monstey_ModelWriteScene(s, armor, out);
            Monstey_Check(r["byte_exact_noop"].as_bool() && Monstey_ReadFile(out) == s.native_baseline, "armor no-op differs");
        });
        for (const auto mode : {0u, 1u, 2u})
            test(mode == 0 ? "actual_position_keeps_386" : mode == 1 ? "actual_float_rebuild_keeps_386" : "actual_new_UV_keeps_386", [&] {
                auto s = Monstey_ModelReadScene(armor, armor_skel); const auto part = Monstey_ArmorPart(s); auto options = Monstey_Json::object();
                s.parts[part].channels["POSITION"].values[0] += 4;
                if (mode == 1) options["position_format"] = "float";
                if (mode == 2) s.parts[part].channels["UV1"] = s.parts[part].channels.at("UV0");
                const auto out = dir / (L"armor-edit-" + std::to_wstring(mode) + L".mesh");
                (void)Monstey_ModelWriteScene(s, armor, out, options); const auto check = Monstey_ModelReadScene(out, armor_skel);
                Monstey_ArmorBands(check, part, 386); Monstey_Check(check.parts[part].indices.size() == 24362 * 3, "armor edit lost faces");
            });
        test("actual_delete_final_face_explicit_membership_385", [&] {
            auto s = Monstey_ModelReadScene(armor, armor_skel); const auto index = Monstey_ArmorPart(s); auto &p = s.parts[index];
            p.indices.resize(p.indices.size() - 3); auto bands = p.triangle_bands; bands.pop_back(); Monstey_ModelSetTriangleBands(p, bands);
            const auto out = dir / L"armor-delete.mesh"; (void)Monstey_ModelWriteScene(s, armor, out);
            Monstey_ArmorBands(Monstey_ModelReadScene(out, armor_skel), index, 385);
        });
        }
        test("stable_band_pack_retains_within_band_order", [&] {
            auto s = Monstey_ModelReadScene(mesh, skel); auto &p = s.parts[0]; const auto indices = p.indices; p.indices.clear();
            for (const auto f : {3u, 2u, 0u, 1u}) p.indices.insert(p.indices.end(), indices.begin() + f * 3, indices.begin() + f * 3 + 3);
            Monstey_ModelSetTriangleBands(p, std::array<std::uint8_t, 4>{2, 1, 0, 1});
            const auto out = dir / L"stable-pack.mesh"; (void)Monstey_ModelWriteScene(s, mesh, out);
            const auto check = Monstey_ModelReadScene(out, skel); std::vector<std::uint32_t> expected;
            for (const auto f : {0u, 2u, 1u, 3u}) expected.insert(expected.end(), indices.begin() + f * 3, indices.begin() + f * 3 + 3);
            Monstey_Check(check.parts[0].indices == expected && check.parts[0].triangle_bands == std::vector<std::uint8_t>({0, 1, 1, 2}), "unstable band pack");
        });
        test("explicit_new_face_band16_full_total_fallback", [&] {
            auto s = Monstey_ModelReadScene(mesh, skel); auto &p = s.parts[0]; p.indices.insert(p.indices.end(), {0, 1, 3});
            auto bands = p.triangle_bands; bands.push_back(16); Monstey_ModelSetTriangleBands(p, bands);
            const auto out = dir / L"new-face.mesh"; (void)Monstey_ModelWriteScene(s, mesh, out);
            const auto c = Monstey_CountBands(Monstey_ModelReadScene(out, skel).parts[0].triangle_bands);
            Monstey_Check(c[0] == 1 && c[1] == 2 && c[2] == 1 && c[16] == 1, "new explicit face/band16 lost");
        });
        test("stale_topology_hash_rejected", [&] {
            auto s = Monstey_ModelReadScene(mesh, skel); s.parts[0].indices[0] = 3; const auto out = dir / L"stale.mesh";
            reject([&] { (void)Monstey_ModelWriteScene(s, mesh, out); }, out);
        });
        test("changed_multiband_topology_without_membership_rejected", [&] {
            auto s = Monstey_ModelReadScene(mesh, skel); auto &p = s.parts[0]; p.indices.resize(p.indices.size() - 3);
            p.triangle_bands.clear(); p.triangle_band_topology_sha256.clear(); const auto out = dir / L"missing.mesh";
            reject([&] { (void)Monstey_ModelWriteScene(s, mesh, out); }, out);
        });
        test("explicit_uniform_option_resolves_stale_or_missing_membership", [&] {
            auto s = Monstey_ModelReadScene(mesh, skel);
            s.parts[0].indices.resize(s.parts[0].indices.size() - 3);
            const auto out = dir / L"explicit-uniform.mesh";
            const auto r = Monstey_ModelWriteScene(s, mesh, out, Monstey_Json::object({{"triangle_band", 16u}}));
            const auto check = Monstey_ModelReadScene(out, skel);
            for (const auto &part : check.parts)
                Monstey_Check(std::all_of(part.triangle_bands.begin(), part.triangle_bands.end(), [](const auto band) { return band == 16; }), "explicit band choice not applied");
            Monstey_Check(r["triangle_band_policy"].as_string() == "explicit_uniform", "explicit policy diagnostic");
            reject([&] { (void)Monstey_ModelWriteScene(s, mesh, dir / L"invalid-choice.mesh", Monstey_Json::object({{"triangle_band", 17u}})); }, dir / L"invalid-choice.mesh");
        });
        test("malformed_band_count_ordinal_hash_rejected", [&] {
            auto s = Monstey_ModelReadScene(mesh, skel); const auto out = dir / L"malformed.mesh";
            reject([&] { Monstey_ModelSetTriangleBands(s.parts[0], std::array<std::uint8_t, 3>{0, 1, 2}); }, out);
            reject([&] { Monstey_ModelSetTriangleBands(s.parts[0], std::array<std::uint8_t, 4>{0, 1, 2, 17}); }, out);
            s.parts[0].triangle_band_topology_sha256 = std::string(64, '0');
            reject([&] { (void)Monstey_ModelWriteScene(s, mesh, out); }, out);
        });
        test("all_eight_authored_channels_survive_general_rebuild", [&] {
            auto s = Monstey_ModelReadScene(mesh, skel); auto &c = s.parts[0].channels;
            c["POSITION"].values[0] = 2; c["NORMAL"].values[1] = 1; c["NORMAL"].values[2] = 0;
            c["TANGENT"].values[0] = 0; c["TANGENT"].values[2] = 1; c["BITANGENT"].values[0] = 1; c["BITANGENT"].values[1] = 0;
            c["UV0"].values[0] = 0.25; c["COLOR0"].values[0] = 64; c["JOINTS"].values[0] = 1; c["JOINTS"].values[1] = 0;
            c["WEIGHTS_U8"].values[0] = 128; c["WEIGHTS_U8"].values[1] = 127; c["UV1"] = c.at("UV0");
            const auto out = dir / L"eight-channels.mesh"; (void)Monstey_ModelWriteScene(s, mesh, out); const auto check = Monstey_ModelReadScene(out, skel);
            for (const auto *name : {"POSITION", "NORMAL", "TANGENT", "BITANGENT", "UV0", "COLOR0", "JOINTS", "WEIGHTS_U8"})
            {
                const auto &actual = check.parts[0].channels.at(name), &expected = c.at(name);
                Monstey_Check(actual.width == expected.width && actual.values.size() == expected.values.size(), "authored channel dimensions");
                for (std::size_t k = 0; k < expected.width; ++k)
                    Monstey_Check(std::abs(actual.values[k] - expected.values[k]) <= (std::string(name) == "UV0" ? 0.00004 : 0.000001),
                                  std::string("authored channel lost: ") + name);
            }
            Monstey_Closure(before, Monstey_Oracle(Monstey_ReadFile(out)), {}, false);
        });
        test("multi_draw_loader_ignores_serialized_Q", [&] {
            const auto s = Monstey_ModelReadScene(multi, skel); Monstey_Check(s.parts.size() == 3, "multi-draw fixture count");
            for (const auto &p : s.parts) if (p.container == 0) Monstey_Check(p.triangle_bands == std::vector<std::uint8_t>(4, 0), "multi-draw inherited Q");
        });
        test("triangle_partition_single_draw_material_piece_null_group_closure", [&] {
            auto s = Monstey_ModelReadScene(mesh, skel); Monstey_Repeat(s.parts[0], 65540, 1, 2);
            const auto counts = Monstey_CountBands(s.parts[0].triangle_bands); const auto out = dir / L"triangle-partition.mesh";
            (void)Monstey_ModelWriteScene(s, mesh, out); const Monstey_Oracle after(Monstey_ReadFile(out));
            Monstey_Check(after.containers.size() > before.containers.size(), "banded split did not expand containers");
            Monstey_Closure(before, after, {{0, counts}}, true); (void)Monstey_ModelReadScene(out, skel);
        });
        test("vertex_partition_single_draw_closure_and_all_vertices", [&] {
            auto s = Monstey_ModelReadScene(mesh, skel); auto &p = s.parts[0];
            for (auto &[name, channel] : p.channels)
            {
                const auto old = channel.values; channel.values.clear(); channel.values.reserve(65538ull * channel.width);
                for (std::size_t v = 0; v < 65538; ++v) for (std::size_t k = 0; k < channel.width; ++k)
                    channel.values.push_back(name == "POSITION" && k == 0 ? static_cast<double>(v) / 4 : old[(v % 8) * channel.width + k]);
            }
            p.indices.resize(65538); std::iota(p.indices.begin(), p.indices.end(), 0u);
            std::vector<std::uint8_t> bands(21846, 1); bands.back() = 2; Monstey_ModelSetTriangleBands(p, bands);
            const auto out = dir / L"vertex-partition.mesh"; (void)Monstey_ModelWriteScene(s, mesh, out);
            const Monstey_Oracle after(Monstey_ReadFile(out));
            Monstey_Closure(before, after, {{0, Monstey_CountBands(bands)}}, true);
            std::set<Monstey_Key> affected;
            for (const auto &connection : before.connections)
                if (connection.map == 0) affected.emplace(connection.words[0], connection.material);
            std::set<std::size_t> partition_containers;
            for (const auto &connection : after.connections)
                if (connection.map >= 0 && affected.contains({connection.words[0], connection.material}))
                    partition_containers.insert(static_cast<std::size_t>(connection.map));
            const auto check = Monstey_ModelReadScene(out, skel); std::size_t vertices = 0;
            for (const auto &part : check.parts)
                if (partition_containers.contains(part.container)) vertices += part.channels.at("POSITION").values.size() / 3;
            Monstey_Check(vertices == 65538, "vertex partition lost/duplicated vertices");
        });
        test("chained_alias_split_materials_8_9_9", [&] {
            const Monstey_Oracle a(aliases);
            Monstey_Check(a.connections[0].material == 8 && a.connections[1].material == 9 && a.connections[2].material == 9, "alias oracle materials");
            auto s = Monstey_ModelReadScene(alias, skel); Monstey_Repeat(s.parts[0], 65540, 1, 2); const auto out = dir / L"alias-partition.mesh";
            (void)Monstey_ModelWriteScene(s, alias, out);
            Monstey_Closure(a, Monstey_Oracle(Monstey_ReadFile(out)), {{0, Monstey_CountBands(s.parts[0].triangle_bands)}}, true);
        });
        test("base_only_large_multi_draw_fallback_valid", [&] {
            auto s = Monstey_ModelReadScene(multi, skel); Monstey_Repeat(s.parts[0], 65536, 0, 0); const auto out = dir / L"base-large.mesh";
            (void)Monstey_ModelWriteScene(s, multi, out); Monstey_Counts counts{}; counts[0] = 65540;
            Monstey_Closure(Monstey_Oracle(multis), Monstey_Oracle(Monstey_ReadFile(out)), {{0, counts}}, false);
            Monstey_Check(!Monstey_ModelReadScene(out, skel).parts.empty(), "large base-only readback");
        });
        test("unchanged_unused_empty_container_survives_other_topology_rebuild", [&] {
            const auto bytes = Monstey_WithUnusedEmptyContainer(source);
            const auto input = dir / L"empty-container.mesh";
            Monstey_WriteNewFile(input, bytes);
            auto scene = Monstey_ModelReadScene(input, skel);
            scene.parts[0].channels["UV1"] = scene.parts[0].channels.at("UV0");
            const auto output = dir / L"empty-container-output.mesh";
            (void)Monstey_ModelWriteScene(scene, input, output);
            const Monstey_Oracle beforeEmpty(bytes), afterEmpty(Monstey_ReadFile(output));
            Monstey_Check(afterEmpty.containers.size() == beforeEmpty.containers.size() &&
                              afterEmpty.containers.back().draws.empty() && afterEmpty.containers.back().sizes == std::array<std::uint32_t, 3>{},
                          "Unchanged empty native container was rejected or changed");
            Monstey_Closure(beforeEmpty, afterEmpty, {}, false);
            Monstey_Check(Monstey_ReadFile(input) == bytes, "Empty-container source changed");
        });
        test("triangle_band_setter_accepts_its_own_nonowning_span", [&] {
            auto scene = Monstey_ModelReadScene(mesh, skel);
            auto &part = scene.parts[0]; const auto expected = part.triangle_bands;
            Monstey_ModelSetTriangleBands(part, std::span<const std::uint8_t>(part.triangle_bands));
            Monstey_Check(part.triangle_bands == expected &&
                              part.triangle_band_topology_sha256 == Monstey_ModelTriangleTopologyHash(part.indices),
                          "Triangle-band setter did not snapshot an aliased input span");
        });
        test("source_CAS_preflight_and_no_overwrite", [&] {
            const auto s = Monstey_ModelReadScene(mesh, skel); const auto bad = dir / L"bad-cas.mesh";
            reject([&] { (void)Monstey_ModelWriteScene(s, mesh, bad, Monstey_Json::object({{"expected_template_sha256", std::string(64, '0')}})); }, bad);
            const auto pre = dir / L"preflight.mesh"; const auto r = Monstey_ModelWriteScene(s, mesh, pre, Monstey_Json::object({{"preflight", true}}));
            Monstey_Check(r["preflight"].as_bool() && !std::filesystem::exists(pre), "preflight published mesh");
            bool rejected = false; try { (void)Monstey_ModelWriteScene(s, mesh, mesh); } catch (const std::exception &) { rejected = true; }
            Monstey_Check(rejected && Monstey_ReadFile(mesh) == source, "source overwrite accepted");
            const auto existing = dir / L"existing.mesh"; const Monstey_Bytes sentinel{0x31, 0x59, 0x26, 0x53}; Monstey_WriteNewFile(existing, sentinel);
            rejected = false; try { (void)Monstey_ModelWriteScene(s, mesh, existing); } catch (const std::exception &) { rejected = true; }
            Monstey_Check(rejected && Monstey_ReadFile(existing) == sentinel, "existing output changed");
        });
        test("all_source_SKEL_and_actual_asset_hashes_unchanged", [&] {
            Monstey_Check(Monstey_ReadFile(mesh) == source && Monstey_ReadFile(alias) == aliases && Monstey_ReadFile(multi) == multis &&
                          Monstey_ReadFile(skel) == skeleton, "synthetic source or SKEL changed");
            if (hasArmor)
                Monstey_Check(Monstey_HashSha256(armor) == armor_hash && Monstey_HashSha256(armor_skel) == skel_hash, "actual source/SKEL changed");
        });
        report["passed"] = passed; report["failed"] = failed; report["fixture_mesh_sha256"] = Monstey_HashSha256(source);
        report["fixture_skel_sha256"] = Monstey_HashSha256(skeleton); report["alias_mesh_sha256"] = Monstey_HashSha256(aliases);
        Monstey_WriteNewText(dir / L"model-band-tests.json", report.dump(2) + "\n");
        std::cout << "Native model bands: " << passed << " passed, " << failed << " failed\n"
                  << Monstey_Utf8((dir / L"model-band-tests.json").wstring()) << '\n'; return failed == 0 ? 0 : 1;
    }
    catch (const std::exception &error) { std::cerr << "Native model bands fatal: " << error.what() << '\n'; return 1; }
}
