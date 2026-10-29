#include "Monstey_ModelPrivate.hpp"

namespace Monstey
{
namespace Monstey_ModelPrivate
{

std::size_t Monstey_AttrBytes(unsigned type)
{
    switch (type)
    {
    case 1:
        return 8;
    case 2:
        return 12;
    case 3:
        return 16;
    case 4:
        return 4;
    case 9:
        return 4;
    case 12:
        return 8;
    default:
        throw Monstey_Error("model_layout_read_only", "Unsupported MMSH storage type " + std::to_string(type));
    }
}

unsigned Monstey_AttrWidth(unsigned type)
{
    switch (type)
    {
    case 1:
    case 9:
        return 2;
    case 2:
        return 3;
    case 3:
    case 4:
    case 12:
        return 4;
    default:
        return 0;
    }
}

Monstey_ModelData Monstey_LoadModel(Monstey_Bytes raw)
{
    Monstey_Span b = raw;
    Monstey_MRequire(Monstey_U32(b, 0) == 0x48534d4d && Monstey_U32(b, 4) == 17, "Expected MMSH17");
    const auto ds = Monstey_U32(b, 8), is = Monstey_U32(b, 12), das = Monstey_U32(b, 16);
    Monstey_MRequire(20ull + ds + is + das == b.size(), "MMSH section extent mismatch");
    const auto desc = b.subspan(20, ds), info = b.subspan(20ull + ds, is), data = b.subspan(20ull + ds + is, das);
    Monstey_CheckRange(desc, 0, 12);
    Monstey_CheckRange(info, 0, 72);
    Monstey_MRequire(Monstey_U32(desc, 0) == 0xebaec3fa && Monstey_U32(info, 0) == 0x1a1541bc,
                     "MMSH section marker mismatch");
    const auto nm = Monstey_U32(desc, 4), nc = Monstey_U32(desc, 8);
    const auto prefix = 12ull + nm * 8ull;
    Monstey_MRequire(nc > 0 && prefix + nc * 12ull == desc.size(), "MMSH description extent mismatch");
    Monstey_ModelData m;
    m.raw = std::move(raw);
    m.descPrefix.assign(desc.begin(), desc.begin() + static_cast<std::ptrdiff_t>(prefix));
    for (std::size_t i = 0; i < 18; ++i)
        m.h[i] = Monstey_U32(info, i * 4);
    Monstey_MRequire(m.h[3] == nc && m.h[16] > 0 && m.h[16] <= 256 && m.h[17] == m.h[16] * 40,
                     "MMSH container/bone counts invalid");
    m.sizes.resize(nc);
    for (std::size_t k = 0; k < 3; ++k)
        for (std::size_t c = 0; c < nc; ++c)
            m.sizes[c][k] = Monstey_U32(desc, static_cast<std::size_t>(prefix) + (k * nc + c) * 4);
    std::size_t p = 72ull + m.h[1] * 4ull + m.h[2] * 16ull;
    Monstey_CheckRange(info, 72, p - 72);
    m.infoPrefix.assign(info.begin() + 72, info.begin() + static_cast<std::ptrdiff_t>(p));
    for (std::size_t c = 0; c < nc; ++c)
    {
        std::array<std::uint32_t, 20> l{};
        for (std::size_t i = 0; i < 4; ++i)
            l[i] = Monstey_U32(info, p + i * 4);
        for (std::size_t i = 0; i < 16; ++i)
            l[4 + i] = Monstey_U16(info, p + 16 + i * 2);
        p += 48;
        m.lods.push_back(l);
    }
    for (std::size_t i = 0; i < m.h[4]; ++i)
    {
        std::array<std::uint32_t, 10> row{};
        for (std::size_t j = 0; j < 10; ++j)
            row[j] = Monstey_U32(info, p + j * 4);
        p += 40;
        m.parts.push_back(row);
    }
    const auto ls = p;
    Monstey_MRequire(Monstey_U32(info, p) == 0x8e3e068e, "MMSH missing LOD section");
    p += 4ull + m.h[6] * 4ull + m.h[7] * 8ull + m.h[8] * 8ull;
    Monstey_CheckRange(info, ls, p - ls);
    m.lodSection.assign(info.begin() + static_cast<std::ptrdiff_t>(ls), info.begin() + static_cast<std::ptrdiff_t>(p));
    Monstey_MRequire(Monstey_U32(info, p) == 0x37d749a6, "MMSH missing layout section");
    p += 4;
    Monstey_MRequire(m.h[9] <= 256, "MMSH layout index overflow");
    for (std::size_t i = 0; i < m.h[9]; ++i)
    {
        const auto count = Monstey_U32(info, p);
        p += 4;
        Monstey_MRequire(count > 0 && count <= 64, "MMSH layout attribute count invalid");
        std::vector<Monstey_Attr> layout;
        for (std::size_t a = 0; a < count; ++a)
        {
            Monstey_CheckRange(info, p, 4);
            Monstey_Attr attr{info[p], info[p + 1], info[p + 2], info[p + 3]};
            Monstey_MRequire(attr.stream <= 1, "MMSH unsupported vertex stream");
            (void)Monstey_AttrBytes(attr.type);
            layout.push_back(attr);
            p += 4;
        }
        m.layouts.push_back(layout);
    }
    Monstey_MRequire(p + 4ull + m.h[16] * 40ull == info.size() && Monstey_U32(info, p) == 0x93d9a424,
                     "MMSH bone section extent mismatch");
    m.bones.assign(info.begin() + static_cast<std::ptrdiff_t>(p), info.end());
    Monstey_MRequire(Monstey_U32(data, 0) == 0x95dbdb69, "MMSH data marker mismatch");
    p = 4;
    for (std::size_t k = 0; k < 3; ++k)
        for (std::size_t c = 0; c < nc; ++c)
        {
            const auto len = m.sizes[c][k];
            Monstey_CheckRange(data, p, len);
            m.buffers[k].emplace_back(data.begin() + static_cast<std::ptrdiff_t>(p),
                                      data.begin() + static_cast<std::ptrdiff_t>(p + len));
            p += len;
        }
    Monstey_MRequire(p == data.size(), "MMSH unconsumed data");
    std::size_t pi = 0;
    for (std::size_t c = 0; c < nc; ++c)
    {
        const auto &l = m.lods[c];
        Monstey_MRequire(std::equal(l.begin(), l.begin() + 3, m.sizes[c].begin()), "MMSH descriptor/LOD size mismatch");
        std::array<std::uint64_t, 3> used{};
        std::uint64_t nv = 0, ni = 0;
        for (std::size_t j = 0; j < l[3]; ++j)
        {
            Monstey_MRequire(pi < m.parts.size(), "MMSH draw count exceeds table");
            const auto &a = m.parts[pi++];
            Monstey_MRequire(a[9] < m.layouts.size() && a[2] > 0 && a[2] <= 65535 && a[4] % 3 == 0 &&
                                 a[4] / 3 <= 65535 && a[6] <= 255,
                             "MMSH draw UINT16/UINT8 bounds invalid");
            std::size_t stride = 0;
            for (const auto &attr : m.layouts[a[9]])
                stride += Monstey_AttrBytes(attr.type);
            Monstey_MRequire(a[0] == used[0] && a[1] == nv && a[3] == ni && a[5] == used[2],
                             "MMSH non-contiguous draw offsets are read-only");
            const std::array<std::uint64_t, 3> lengths = {a[2] * static_cast<std::uint64_t>(stride), a[4] * 2ull, a[6]};
            for (std::size_t k = 0; k < 3; ++k)
            {
                Monstey_MRequire(used[k] + lengths[k] <= m.sizes[c][k], "MMSH draw range exceeds container");
            }
            for (std::size_t i = 0; i < a[4]; ++i)
                Monstey_MRequire(Monstey_U16(m.buffers[1][c], static_cast<std::size_t>(used[1]) + i * 2) < a[2],
                                 "MMSH triangle index outside draw");
            for (std::size_t i = 0; i < a[6]; ++i)
                Monstey_MRequire(m.buffers[2][c][static_cast<std::size_t>(used[2]) + i] < m.h[16],
                                 "MMSH palette outside rig");
            for (std::size_t k = 0; k < 3; ++k)
                used[k] += lengths[k];
            nv += a[2];
            ni += a[4];
        }
        for (std::size_t k = 0; k < 3; ++k)
            Monstey_MRequire(used[k] == m.sizes[c][k], "MMSH unconsumed container bytes");
    }
    Monstey_MRequire(pi == m.parts.size(), "MMSH unused draw metadata");
    return m;
}

Monstey_ModelGraph Monstey_ReadModelGraph(const Monstey_ModelData &m)
{
    Monstey_ModelGraph g;
    const auto n = Monstey_U32(m.descPrefix, 4);
    Monstey_MRequire(n == m.h[2] && m.descPrefix.size() == 12ull + n * 8ull &&
                         m.infoPrefix.size() == m.h[1] * 4ull + n * 16ull,
                     "MMSH descriptor/connection graph count mismatch");
    for (std::size_t i = 0; i < n; ++i)
    {
        g.flags.push_back(Monstey_U32(m.descPrefix, 12 + i * 4));
        g.rawMaps.push_back(static_cast<std::int32_t>(Monstey_U32(m.descPrefix, 12ull + n * 4ull + i * 4)));
        std::array<std::uint32_t, 4> row{};
        for (std::size_t j = 0; j < 4; ++j)
            row[j] = Monstey_U32(m.infoPrefix, m.h[1] * 4ull + i * 16 + j * 4);
        g.connections.push_back(row);
    }
    g.effectiveMaps = g.rawMaps;
    g.materialSource.resize(n);
    std::size_t next = n;
    for (std::size_t i = n; i-- > 0;)
    {
        g.materialSource[i] = i;
        if (g.flags[i] & 4)
        {
            Monstey_MRequire(next < n, "MMSH forward alias has no subsequent live descriptor");
            g.effectiveMaps[i] = g.effectiveMaps[next];
            g.materialSource[i] = next;
        }
        if (g.effectiveMaps[i] >= 0)
        {
            Monstey_MRequire(static_cast<std::size_t>(g.effectiveMaps[i]) < m.lods.size(),
                             "MMSH effective connection points outside containers");
            next = i;
        }
    }
    Monstey_MRequire(Monstey_U32(m.lodSection, 0) == 0x8e3e068e && m.h[6] <= 65535 &&
                         static_cast<std::uint64_t>(m.h[7]) + m.h[8] <= 65535,
                     "MMSH group/reference packed count overflow");
    std::size_t pos = 4;
    for (std::size_t i = 0; i < m.h[6]; ++i, pos += 4)
    {
        const auto ref = Monstey_U32(m.lodSection, pos);
        Monstey_MRequire(ref < n, "MMSH group reference is outside connection table");
        g.refs.push_back(ref);
    }
    const auto groups = static_cast<std::size_t>(m.h[7]) + m.h[8];
    for (std::size_t i = 0; i < groups; ++i, pos += 4)
        g.thresholds.push_back(Monstey_U32(m.lodSection, pos));
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < groups; ++i, pos += 4)
    {
        const auto count = Monstey_U32(m.lodSection, pos);
        sum += count;
        Monstey_MRequire(count <= 65535 && sum <= g.refs.size(), "MMSH group reference slice overflow");
        g.counts.push_back(count);
    }
    Monstey_MRequire(sum == g.refs.size() && pos == m.lodSection.size(), "MMSH group reference/cardinality mismatch");
    return g;
}

Monstey_J Monstey_Allocation(const Monstey_ModelData &m)
{
    const auto graph = Monstey_ReadModelGraph(m);
    std::uint32_t maximum = 0;
    std::uint64_t total = 0;
    auto mapping = Monstey_J::array(), effective = Monstey_J::array();
    std::set<std::size_t> used;
    for (std::size_t i = 0; i < graph.rawMaps.size(); ++i)
    {
        mapping.as_array().push_back(static_cast<std::int64_t>(graph.rawMaps[i]));
        const auto c = graph.effectiveMaps[i];
        effective.as_array().push_back(static_cast<std::int64_t>(c));
        if (c >= 0)
        {
            const auto ci = static_cast<std::size_t>(c);
            maximum = std::max(maximum, m.lods[ci][3]);
            Monstey_MRequire(m.lods[ci][3] <= 255, "Native connection draw count exceeds UINT8");
            total += m.lods[ci][3];
            used.insert(ci);
        }
    }
    Monstey_MRequire(total <= 65535, "MMSH effective draw-start/count UINT16 overflow");
    std::uint64_t attrs = 0;
    for (const auto &l : m.layouts)
        attrs += l.size();
    std::array<std::uint64_t, 3> buffers{};
    for (const auto ci : used)
        for (std::size_t stream = 0; stream < 3; ++stream)
        {
            const std::uint64_t align = stream == 0 ? 128 : 16;
            buffers[stream] += (static_cast<std::uint64_t>(m.sizes[ci][stream]) + align - 1) & ~(align - 1);
            Monstey_MRequire(buffers[stream] <= 0xffffffffull, "Native aligned geometry allocation overflow");
        }
    const auto capacity = static_cast<std::uint64_t>(m.h[2]) * m.h[5];
    Monstey_MRequire(capacity <= 0xffffffffull / 128 && m.h[10] <= 0xffffffffu / 4,
                     "MMSH native allocation byte overflow");
    return Monstey_J::object({{"connection_map", mapping},
                              {"effective_connection_map", effective},
                              {"declared_max_draws", m.h[5]},
                              {"required_max_draws", maximum},
                              {"allocated_draws", capacity},
                              {"required_draws", total},
                              {"declared_attributes", m.h[10]},
                              {"required_attributes", attrs},
                              {"aligned_vertex_bytes", buffers[0]},
                              {"aligned_index_bytes", buffers[1]},
                              {"aligned_palette_bytes", buffers[2]},
                              {"safe", m.h[5] >= maximum && capacity >= total && m.h[10] >= attrs}});
}

std::vector<Monstey_Part> Monstey_DecodeParts(const Monstey_ModelData &m, Monstey_DecodePurpose purpose)
{
    static const std::array<const char *, 8> semantics = {"POSITION", "NORMAL", "TANGENT",     "BITANGENT",
                                                          "UV",       "COLOR",  "WEIGHTS_RAW", "PALETTE"};
    std::vector<Monstey_Part> result;
    std::size_t pi = 0;
    for (std::size_t c = 0; c < m.lods.size(); ++c)
        for (std::size_t part = 0; part < m.lods[c][3]; ++part)
        {
            const auto &row = m.parts[pi++];
            Monstey_Part p;
            p.container = c;
            p.part = part;
            std::size_t cursor = row[0];
            for (unsigned stream = 0; stream < 2; ++stream)
            {
                std::size_t stride = 0;
                for (const auto &a : m.layouts[row[9]])
                    if (a.stream == stream)
                        stride += Monstey_AttrBytes(a.type);
                std::size_t offset = 0;
                for (const auto &a : m.layouts[row[9]])
                    if (a.stream == stream)
                    {
                        Monstey_MRequire(a.semantic < semantics.size(),
                                         "MMSH semantic is read-only: unknown identifier");
                        const auto rawWidth = Monstey_AttrWidth(a.type);
                        const auto width = a.semantic <= 3 ? 3u : rawWidth;
                        Monstey_MRequire(rawWidth >= width, "MMSH semantic storage lacks required components");
                        auto name = std::string(semantics[a.semantic]);
                        if (a.semantic == 4 || a.semantic == 5)
                            name += std::to_string(a.channel);
                        Monstey_MRequire(!p.channels.contains(name), "Duplicate MMSH semantic");
                        Monstey_Channel ch;
                        ch.width = width;
                        ch.v.reserve(static_cast<std::size_t>(row[2]) * width);
                        for (std::size_t v = 0; v < row[2]; ++v)
                            for (unsigned k = 0; k < width; ++k)
                            {
                                const auto at = cursor + v * stride + offset;
                                double value = 0;
                                if (a.type <= 3)
                                    value = (a.semantic >= 1 && a.semantic <= 3) ||
                                                    purpose == Monstey_DecodePurpose::Inspection
                                                ? static_cast<double>(
                                                      std::bit_cast<float>(Monstey_U32(m.buffers[0][c], at + k * 4)))
                                                : Monstey_F32(m.buffers[0][c], at + k * 4);
                                else if (a.type == 4)
                                    value = m.buffers[0][c][at + k];
                                else if (a.type == 9)
                                    value = static_cast<std::int16_t>(Monstey_U16(m.buffers[0][c], at + k * 2));
                                else
                                    value = Monstey_Half(Monstey_U16(m.buffers[0][c], at + k * 2));
                                if (a.semantic == 4 && a.type == 9)
                                    value = static_cast<float>(value / 32767);
                                Monstey_MRequire(std::isfinite(value) || (a.semantic >= 1 && a.semantic <= 3) ||
                                                     purpose == Monstey_DecodePurpose::Inspection,
                                                 "Nonfinite MMSH vertex channel");
                                ch.v.push_back(value);
                            }
                        p.channels[name] = std::move(ch);
                        offset += Monstey_AttrBytes(a.type);
                    }
                cursor += static_cast<std::size_t>(row[2]) * stride;
            }
            if (purpose == Monstey_DecodePurpose::Rigged)
            {
                for (const auto *name :
                     {"POSITION", "NORMAL", "TANGENT", "BITANGENT", "UV0", "COLOR0", "WEIGHTS_RAW", "PALETTE"})
                    Monstey_MRequire(p.channels.contains(name),
                                     std::string("MMSH export needs native channel ") + name);
            }
            else
                Monstey_MRequire(p.channels.contains("POSITION"), "MMSH geometry needs POSITION");
            if (purpose == Monstey_DecodePurpose::RigidGeometry)
            {
                p.channels.erase("WEIGHTS_RAW");
                p.channels.erase("PALETTE");
            }
            else
            {
                if (p.channels.contains("PALETTE"))
                {
                    auto joints = std::move(p.channels.at("PALETTE"));
                    p.channels.erase("PALETTE");
                    if (row[6] > 0)
                    {
                        if (purpose == Monstey_DecodePurpose::Rigged)
                            Monstey_MRequire(joints.width == 4, "Native skin needs four blend indices");
                        for (auto &x : joints.v)
                        {
                            Monstey_MRequire(std::isfinite(x) && x >= 0 && x < row[6] && std::floor(x) == x,
                                             "MMSH palette slot outside draw");
                            x = m.buffers[2][c][row[5] + static_cast<std::size_t>(x)];
                        }
                        p.channels["JOINTS"] = std::move(joints);
                    }
                    else
                        Monstey_MRequire(purpose != Monstey_DecodePurpose::Rigged, "Native skin palette is empty");
                }
                if (p.channels.contains("WEIGHTS_RAW"))
                {
                    auto weights = std::move(p.channels.at("WEIGHTS_RAW"));
                    p.channels.erase("WEIGHTS_RAW");
                    if (weights.width == 4)
                    {
                        for (std::size_t v = 0; v < row[2]; ++v)
                            std::swap(weights.v[v * 4], weights.v[v * 4 + 2]);
                        p.channels["WEIGHTS_U8"] = std::move(weights);
                    }
                    else
                    {
                        Monstey_MRequire(purpose != Monstey_DecodePurpose::Rigged,
                                         "Native skin needs four blend weights");
                        p.channels["WEIGHTS_RAW"] = std::move(weights);
                    }
                }
            }

            for (std::size_t i = 0; i < row[4]; ++i)
                p.indices.push_back(Monstey_U16(m.buffers[1][c], row[3] * 2ull + i * 2));
            const auto triangles = p.indices.size() / 3;
            bool qualified = true;
            std::uint32_t previous = 0;
            if (m.lods[c][3] == 1)
                for (std::size_t q = 4; q < 20; ++q)
                {
                    qualified = qualified && m.lods[c][q] >= previous && m.lods[c][q] <= triangles;
                    previous = m.lods[c][q];
                }
            if (qualified)
            {
                std::vector<std::uint8_t> bands(triangles, 0);
                if (m.lods[c][3] == 1)
                {
                    std::size_t band = 0;
                    for (std::size_t face = 0; face < triangles; ++face)
                    {
                        while (band < 16 && face >= m.lods[c][4 + band])
                            ++band;
                        bands[face] = static_cast<std::uint8_t>(band);
                    }
                }
                Monstey_SetPartBands(p, bands);
            }
            result.push_back(std::move(p));
        }
    return result;
}

} // namespace Monstey_ModelPrivate

} // namespace Monstey
