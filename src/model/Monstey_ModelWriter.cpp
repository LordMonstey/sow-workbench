#include "Monstey_ModelPrivate.hpp"

namespace Monstey
{
namespace Monstey_ModelPrivate
{

void Monstey_ExpandModelConnections(Monstey_ModelData &m, const Monstey_ModelData &native,
                                    const std::vector<std::vector<std::size_t>> &children)
{
    const auto original = Monstey_ReadModelGraph(native);
    auto graph = original;
    std::vector<std::vector<std::uint32_t>> clones(original.connections.size());
    for (std::size_t i = 0; i < original.connections.size(); ++i)
    {
        const auto map = original.effectiveMaps[i];
        if (map < 0 || children[static_cast<std::size_t>(map)].size() <= 1)
            continue;
        const auto &list = children[static_cast<std::size_t>(map)];
        for (std::size_t j = 1; j < list.size(); ++j)
        {
            auto row = original.connections[i];
            row[1] = Monstey_Size(list[j]);
            row[2] = original.connections[original.materialSource[i]][2];
            clones[i].push_back(Monstey_Size(graph.connections.size()));
            graph.connections.push_back(row);
            graph.flags.push_back(original.flags[i] & ~4u);
            graph.rawMaps.push_back(static_cast<std::int32_t>(list[j]));
        }
    }
    if (graph.connections.size() == original.connections.size())
        return;
    graph.refs.clear();
    graph.counts.clear();
    std::size_t cursor = 0;
    for (const auto count : original.counts)
    {
        const auto start = graph.refs.size();
        for (std::size_t j = 0; j < count; ++j)
        {
            const auto ref = original.refs[cursor++];
            const auto piece = original.connections[ref][0];
            Monstey_MRequire(piece < native.h[1] && piece <= 255,
                             "Referenced native piece identity is outside the retained piece table");
            graph.refs.push_back(ref);
            graph.refs.insert(graph.refs.end(), clones[ref].begin(), clones[ref].end());
        }
        graph.counts.push_back(Monstey_Size(graph.refs.size() - start));
    }
    Monstey_MRequire(graph.refs.size() <= 65535, "Expanded native connection/group references exceed UINT16");
    m.h[2] = Monstey_Size(graph.connections.size());
    m.h[6] = Monstey_Size(graph.refs.size());
    m.descPrefix.clear();
    Monstey_AppendU32(m.descPrefix, Monstey_U32(native.descPrefix, 0));
    Monstey_AppendU32(m.descPrefix, m.h[2]);
    Monstey_AppendU32(m.descPrefix, Monstey_Size(m.lods.size()));
    for (const auto flag : graph.flags)
        Monstey_AppendU32(m.descPrefix, flag);
    for (const auto map : graph.rawMaps)
        Monstey_AppendU32(m.descPrefix, static_cast<std::uint32_t>(map));
    m.infoPrefix.assign(native.infoPrefix.begin(),
                        native.infoPrefix.begin() + static_cast<std::ptrdiff_t>(native.h[1] * 4ull));
    for (const auto &row : graph.connections)
        for (const auto word : row)
            Monstey_AppendU32(m.infoPrefix, word);
    m.lodSection.clear();
    Monstey_AppendU32(m.lodSection, 0x8e3e068e);
    for (const auto ref : graph.refs)
        Monstey_AppendU32(m.lodSection, ref);
    for (const auto threshold : original.thresholds)
        Monstey_AppendU32(m.lodSection, threshold);
    for (const auto count : graph.counts)
        Monstey_AppendU32(m.lodSection, count);
    const auto check = Monstey_ReadModelGraph(m);
    for (std::size_t i = 0; i < original.connections.size(); ++i)
    {
        Monstey_MRequire(check.flags[i] == original.flags[i] && check.rawMaps[i] == original.rawMaps[i] &&
                             check.connections[i] == original.connections[i] &&
                             check.effectiveMaps[i] == original.effectiveMaps[i] &&
                             check.materialSource[i] == original.materialSource[i],
                         "Connection expansion changed an existing native binding");
        for (const auto ref : clones[i])
            Monstey_MRequire(check.connections[ref][0] == original.connections[i][0] &&
                                 check.connections[ref][2] == original.connections[original.materialSource[i]][2] &&
                                 check.connections[ref][3] == original.connections[i][3],
                             "Connection expansion changed piece/material/opaque words");
    }
}

Monstey_Bytes Monstey_SerializeModel(const Monstey_ModelData &m)
{
    auto desc = m.descPrefix;
    for (std::size_t k = 0; k < 3; ++k)
        for (const auto &b : m.buffers[k])
            Monstey_AppendU32(desc, Monstey_Size(b.size()));
    auto info = Monstey_Bytes{};
    auto h = m.h;
    h[4] = Monstey_Size(m.parts.size());
    h[9] = Monstey_Size(m.layouts.size());
    for (const auto v : h)
        Monstey_AppendU32(info, v);
    info.insert(info.end(), m.infoPrefix.begin(), m.infoPrefix.end());
    for (const auto &l : m.lods)
    {
        for (std::size_t i = 0; i < 4; ++i)
            Monstey_AppendU32(info, l[i]);
        for (std::size_t i = 4; i < 20; ++i)
        {
            Monstey_MRequire(l[i] <= 65535, "Native LOD quota overflow");
            Monstey_AppendU16(info, static_cast<std::uint16_t>(l[i]));
        }
    }
    for (const auto &p : m.parts)
        for (const auto v : p)
            Monstey_AppendU32(info, v);
    info.insert(info.end(), m.lodSection.begin(), m.lodSection.end());
    Monstey_AppendU32(info, 0x37d749a6);
    for (const auto &l : m.layouts)
    {
        Monstey_AppendU32(info, Monstey_Size(l.size()));
        for (const auto &a : l)
        {
            info.push_back(a.stream);
            info.push_back(a.type);
            info.push_back(a.semantic);
            info.push_back(a.channel);
        }
    }
    info.insert(info.end(), m.bones.begin(), m.bones.end());
    Monstey_Bytes data;
    Monstey_AppendU32(data, 0x95dbdb69);
    for (const auto &k : m.buffers)
        for (const auto &b : k)
            data.insert(data.end(), b.begin(), b.end());
    Monstey_Bytes out;
    for (const auto v :
         {0x48534d4du, 17u, Monstey_Size(desc.size()), Monstey_Size(info.size()), Monstey_Size(data.size())})
        Monstey_AppendU32(out, v);
    out.insert(out.end(), desc.begin(), desc.end());
    out.insert(out.end(), info.begin(), info.end());
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

bool Monstey_PatchNativePart(const Monstey_ModelData &native, const std::array<std::uint32_t, 10> &donor,
                             const Monstey_Part &original, const Monstey_Part &authored, bool half, bool nativeExact,
                             bool extendPalette, Monstey_EncodedPart &out)
{
    if (donor[6] > Monstey_ModelDrawPaletteLimit && !Monstey_NativeEquivalent(authored, original, nativeExact))
        return false;
    if (authored.Count() != original.Count() || authored.channels.size() != original.channels.size())
        return false;
    for (const auto &[name, channel] : original.channels)
    {
        auto it = authored.channels.find(name);
        if (it == authored.channels.end() || it->second.width != channel.width ||
            it->second.v.size() != channel.v.size())
            return false;
    }
    const auto container = authored.container;
    out.layout = native.layouts[donor[9]];
    bool sourceHalf = false;
    std::size_t byteCount = 0;
    for (const auto &a : out.layout)
    {
        byteCount += Monstey_AttrBytes(a.type);
        if (a.semantic == 0)
            sourceHalf = a.type == 12;
    }
    if (sourceHalf != half)
        return false;
    const auto vertexLength = authored.Count() * byteCount;
    out.vertices.assign(native.buffers[0][container].begin() + donor[0],
                        native.buffers[0][container].begin() + donor[0] + static_cast<std::ptrdiff_t>(vertexLength));
    out.palette.assign(native.buffers[2][container].begin() + donor[5],
                       native.buffers[2][container].begin() + donor[5] + donor[6]);
    std::map<unsigned, std::uint8_t> slots;
    for (std::size_t i = 0; i < out.palette.size(); ++i)
        slots.emplace(out.palette[i], static_cast<std::uint8_t>(i));
    for (const auto x : authored.channels.at("JOINTS").v)
    {
        const auto joint = static_cast<unsigned>(x);
        if (!slots.contains(joint))
        {
            if (!extendPalette || joint > 255 || out.palette.size() >= Monstey_ModelDrawPaletteLimit)
                return false;
            slots[joint] = static_cast<std::uint8_t>(out.palette.size());
            out.palette.push_back(static_cast<std::uint8_t>(joint));
        }
    }
    static const std::array<const char *, 8> names = {"POSITION", "NORMAL", "TANGENT",    "BITANGENT",
                                                      "UV",       "COLOR",  "WEIGHTS_U8", "JOINTS"};
    std::size_t cursor = 0;
    for (unsigned stream = 0; stream < 2; ++stream)
    {
        std::size_t stride = 0;
        for (const auto &a : out.layout)
            if (a.stream == stream)
                stride += Monstey_AttrBytes(a.type);
        std::size_t offset = 0;
        for (const auto &a : out.layout)
            if (a.stream == stream)
            {
                if (a.semantic >= names.size())
                    return false;
                std::string name = names[a.semantic];
                if (a.semantic == 4 || a.semantic == 5)
                    name += std::to_string(a.channel);
                const auto &old = original.channels.at(name);
                const auto &edit = authored.channels.at(name);
                for (std::size_t vertex = 0; vertex < authored.Count(); ++vertex)
                    for (unsigned k = 0; k < edit.width; ++k)
                    {
                        const auto logical = a.semantic == 6 ? (k == 0 ? 2u : k == 2 ? 0u : k) : k;
                        const auto oldValue = old.v[vertex * old.width + logical],
                                   value = edit.v[vertex * edit.width + logical];
                        const auto expected = name == "POSITION" && !nativeExact
                                                  ? static_cast<double>(static_cast<float>(oldValue / 100)) * 100
                                                  : oldValue;
                        const bool same =
                            value == expected || (std::isnan(value) && std::isnan(expected) &&
                                                  std::bit_cast<std::uint32_t>(static_cast<float>(value)) ==
                                                      std::bit_cast<std::uint32_t>(static_cast<float>(expected)));
                        if (same)
                            continue;
                        const auto at = cursor + vertex * stride + offset;
                        if (a.type <= 3)
                            Monstey_PutU32(out.vertices, at + k * 4,
                                           std::bit_cast<std::uint32_t>(static_cast<float>(value)));
                        else if (a.type == 4)
                        {
                            const auto encoded =
                                a.semantic == 7 ? static_cast<double>(slots.at(static_cast<unsigned>(value))) : value;
                            if (encoded < 0 || encoded > 255 || std::floor(encoded) != encoded)
                                return false;
                            out.vertices[at + k] = static_cast<std::uint8_t>(encoded);
                        }
                        else if (a.type == 9)
                        {
                            const auto scaled = a.semantic == 4 ? std::round(value * 32767) : std::round(value);
                            if (!std::isfinite(scaled) || scaled < -32768 || scaled > 32767)
                                return false;
                            Monstey_PutU16(out.vertices, at + k * 2,
                                           static_cast<std::uint16_t>(static_cast<std::int16_t>(scaled)));
                        }
                        else if (a.type == 12)
                        {
                            if (!std::isfinite(value) || std::abs(value) > 65504)
                                return false;
                            Monstey_PutU16(out.vertices, at + k * 2, Monstey_ToHalf(value));
                        }
                        else
                            return false;
                    }
                offset += Monstey_AttrBytes(a.type);
            }
        cursor += authored.Count() * stride;
    }
    for (const auto index : authored.indices)
    {
        if (index >= authored.Count() || index > 65535)
            return false;
        Monstey_AppendU16(out.indices, static_cast<std::uint16_t>(index));
    }
    return true;
}

Monstey_Bytes Monstey_RebuildModel(const Monstey_ModelData &native,
                                   const std::map<std::size_t, std::vector<Monstey_Part>> &replacements,
                                   bool preserveZero, const Monstey_J &options, bool nativeExact)
{
    auto m = native;
    const auto decodedOriginal =
        Monstey_DecodeParts(native, preserveZero ? Monstey_DecodePurpose::Rigged : Monstey_DecodePurpose::Inspection);
    auto authored = replacements;
    if (options.contains("position_format"))
    {
        const auto format = options["position_format"].as_string();
        Monstey_MRequire(format == "half" || format == "float", "position_format must be half or float");
    }
    std::vector<std::size_t> starts(native.lods.size(), 0);
    for (std::size_t c = 1; c < starts.size(); ++c)
        starts[c] = starts[c - 1] + native.lods[c - 1][3];
    for (auto &[container, parts] : authored)
    {
        Monstey_MRequire(container < native.lods.size() && !parts.empty() && native.lods[container][3] > 0,
                         "Authored container identity/count invalid");
        for (auto &part : parts)
        {
            const auto donorPart = std::min(part.part, static_cast<std::size_t>(native.lods[container][3] - 1));
            const auto donor =
                std::find_if(decodedOriginal.begin(), decodedOriginal.end(), [&](const auto &candidate)
                             { return candidate.container == container && candidate.part == donorPart; });
            Monstey_MRequire(donor != decodedOriginal.end(), "Authored part has no native donor");
            Monstey_ResolvePartBands(part, *donor, options);
            Monstey_PackPartBands(part);
        }
    }
    if (preserveZero)
    {
        auto exact = native;
        bool compatible = true;
        for (const auto &[container, parts] : authored)
        {
            if (parts.size() != native.lods[container][3])
            {
                compatible = false;
                break;
            }
            std::set<std::size_t> seen;
            for (const auto &part : parts)
            {
                if (!seen.insert(part.part).second || part.part >= native.lods[container][3] ||
                    (parts.size() > 1 && Monstey_HasPartBands(part) &&
                     std::any_of(part.triangle_bands.begin(), part.triangle_bands.end(),
                                 [](const auto band) { return band != 0; })))
                {
                    compatible = false;
                    break;
                }
                const auto it =
                    std::find_if(decodedOriginal.begin(), decodedOriginal.end(), [&](const auto &candidate)
                                 { return candidate.container == container && candidate.part == part.part; });
                const auto &donor = native.parts[starts[container] + part.part];
                if (it == decodedOriginal.end() || part.indices.size() != donor[4])
                {
                    compatible = false;
                    break;
                }
                bool targetHalf = false;
                for (const auto &attribute : native.layouts[donor[9]])
                    if (attribute.semantic == 0)
                        targetHalf = attribute.type == 12;
                if (options.contains("position_format"))
                    targetHalf = options["position_format"].as_string() == "half";
                Monstey_EncodedPart patch;
                if (!Monstey_PatchNativePart(native, donor, *it, part, targetHalf, nativeExact, false, patch))
                {
                    compatible = false;
                    break;
                }
                std::copy(patch.vertices.begin(), patch.vertices.end(), exact.buffers[0][container].begin() + donor[0]);
                std::copy(patch.indices.begin(), patch.indices.end(),
                          exact.buffers[1][container].begin() + donor[3] * 2ull);
                if (parts.size() == 1 && Monstey_HasPartBands(part))
                    Monstey_SetContainerBands(exact.lods[container], part);
            }
            if (!compatible)
                break;
        }
        if (compatible)
        {
            const auto output = Monstey_SerializeModel(exact);
            const auto check = Monstey_LoadModel(output);
            Monstey_MRequire(Monstey_SerializeModel(check) == output && Monstey_Allocation(check)["safe"].as_bool(),
                             "Fixed topology native patch validation failed");
            return output;
        }
    }
    struct Monstey_ContainerWork
    {
        std::size_t origin = 0;
        bool changed = false;
        std::vector<Monstey_Part> chunks;
    };
    std::vector<Monstey_ContainerWork> work(native.lods.size());
    std::vector<std::vector<std::size_t>> children(native.lods.size());
    for (std::size_t c = 0; c < native.lods.size(); ++c)
    {
        work[c].origin = c;
        children[c].push_back(c);
    }
    for (const auto &[container, parts] : authored)
    {
        std::vector<Monstey_Part> chunks;
        for (const auto &part : parts)
        {
            const auto original =
                std::find_if(decodedOriginal.begin(), decodedOriginal.end(), [&](const auto &candidate)
                             { return candidate.container == container && candidate.part == part.part; });
            if (preserveZero && original != decodedOriginal.end() &&
                Monstey_NativeEquivalent(part, *original, nativeExact))
                chunks.push_back(part);
            else
            {
                auto split = Monstey_SplitModelPart(part, preserveZero);
                chunks.insert(chunks.end(), std::make_move_iterator(split.begin()),
                              std::make_move_iterator(split.end()));
            }
        }
        Monstey_MRequire(!chunks.empty(), "Native authored container has no draws");
        bool separate = false;
        if (chunks.size() > 1)
            for (const auto &chunk : chunks)
            {
                Monstey_MRequire(Monstey_HasPartBands(chunk),
                                 "Split topology requires explicit native triangle-band membership");
                separate = separate || std::any_of(chunk.triangle_bands.begin(), chunk.triangle_bands.end(),
                                                   [](const auto band) { return band != 0; });
            }
        work[container].changed = true;
        if (!separate)
        {
            Monstey_MRequire(chunks.size() <= 255, "Native container draw count must be 1-255");
            work[container].chunks = std::move(chunks);
        }
        else
        {
            work[container].chunks.push_back(std::move(chunks.front()));
            for (std::size_t i = 1; i < chunks.size(); ++i)
            {
                children[container].push_back(work.size());
                Monstey_ContainerWork next;
                next.origin = container;
                next.changed = true;
                next.chunks.push_back(std::move(chunks[i]));
                work.push_back(std::move(next));
            }
        }
    }
    m.lods.resize(work.size());
    m.sizes.resize(work.size());
    for (auto &stream : m.buffers)
        stream.resize(work.size());
    m.parts.clear();
    for (std::size_t c = 0; c < work.size(); ++c)
    {
        const auto origin = work[c].origin;
        const auto first = starts[origin];
        const auto count = native.lods[origin][3];
        Monstey_MRequire(first + count <= native.parts.size(), "Native donor draw extent");
        if (!work[c].changed)
        {
            m.parts.insert(m.parts.end(), native.parts.begin() + static_cast<std::ptrdiff_t>(first),
                           native.parts.begin() + static_cast<std::ptrdiff_t>(first + count));
            continue;
        }
        Monstey_MRequire(count > 0, "Target container has no native donor draw");
        const auto &chunks = work[c].chunks;
        m.lods[c] = native.lods[origin];
        bool half = false;
        for (const auto &a : native.layouts[native.parts[first][9]])
            if (a.semantic == 0)
                half = a.type == 12;
        if (options.contains("position_format"))
        {
            const auto format = options["position_format"].as_string();
            Monstey_MRequire(format == "half" || format == "float", "position_format must be half or float");
            half = format == "half";
        }
        Monstey_Bytes vb, ib, pb;
        std::uint32_t nv = 0, ni = 0;
        for (std::size_t i = 0; i < chunks.size(); ++i)
        {
            const auto &p = chunks[i];
            const auto original =
                std::find_if(decodedOriginal.begin(), decodedOriginal.end(), [&](const auto &candidate)
                             { return candidate.container == origin && candidate.part == p.part; });
            Monstey_EncodedPart encoded;
            if (preserveZero && original != decodedOriginal.end() &&
                Monstey_NativeEquivalent(p, *original, nativeExact))
            {
                // An untouched native draw retains its original profile and bytes.
                const auto &donor = native.parts[first + p.part];
                encoded.layout = native.layouts[donor[9]];
                std::size_t stride = 0;
                for (const auto &attribute : encoded.layout)
                    stride += Monstey_AttrBytes(attribute.type);
                const auto vertex = static_cast<std::size_t>(donor[0]),
                           vertexLength = static_cast<std::size_t>(donor[2]) * stride,
                           index = static_cast<std::size_t>(donor[3]) * 2,
                           indexLength = static_cast<std::size_t>(donor[4]) * 2,
                           palette = static_cast<std::size_t>(donor[5]);
                encoded.vertices.assign(native.buffers[0][origin].begin() + static_cast<std::ptrdiff_t>(vertex),
                                        native.buffers[0][origin].begin() +
                                            static_cast<std::ptrdiff_t>(vertex + vertexLength));
                encoded.indices.assign(native.buffers[1][origin].begin() + static_cast<std::ptrdiff_t>(index),
                                       native.buffers[1][origin].begin() +
                                           static_cast<std::ptrdiff_t>(index + indexLength));
                encoded.palette.assign(native.buffers[2][origin].begin() + static_cast<std::ptrdiff_t>(palette),
                                       native.buffers[2][origin].begin() +
                                           static_cast<std::ptrdiff_t>(palette + donor[6]));
            }
            else
            {
                bool patched = false;
                if (preserveZero && original != decodedOriginal.end() && p.Count() == original->Count())
                {
                    Monstey_EncodedPart patch;
                    bool targetHalf = false;
                    for (const auto &a : native.layouts[native.parts[first + p.part][9]])
                        if (a.semantic == 0)
                            targetHalf = a.type == 12;
                    if (options.contains("position_format"))
                        targetHalf = half;
                    if (Monstey_PatchNativePart(native, native.parts[first + p.part], *original, p, targetHalf,
                                                nativeExact, true, patch))
                    {
                        encoded = std::move(patch);
                        patched = true;
                    }
                }
                // Compatible native storage keeps its original declaration profile.
                if (!patched)
                    encoded = Monstey_EncodeModelPart(p, half, preserveZero);
            }

            auto layout = std::find(m.layouts.begin(), m.layouts.end(), encoded.layout);
            if (layout == m.layouts.end())
            {
                m.layouts.push_back(encoded.layout);
                layout = std::prev(m.layouts.end());
            }
            Monstey_MRequire(m.layouts.size() <= 256, "Native layout UINT8 overflow");
            auto row = native.parts[first + std::min(p.part, static_cast<std::size_t>(count - 1))];
            row[0] = Monstey_Size(vb.size());
            row[1] = nv;
            row[2] = Monstey_Size(p.Count());
            row[3] = ni;
            row[4] = Monstey_Size(p.indices.size());
            row[5] = Monstey_Size(pb.size());
            row[6] = Monstey_Size(encoded.palette.size());
            row[9] = Monstey_Size(static_cast<std::size_t>(layout - m.layouts.begin()));
            m.parts.push_back(row);
            vb.insert(vb.end(), encoded.vertices.begin(), encoded.vertices.end());
            ib.insert(ib.end(), encoded.indices.begin(), encoded.indices.end());
            pb.insert(pb.end(), encoded.palette.begin(), encoded.palette.end());
            nv += row[2];
            ni += row[4];
        }
        m.buffers[0][c] = std::move(vb);
        m.buffers[1][c] = std::move(ib);
        m.buffers[2][c] = std::move(pb);
        for (std::size_t k = 0; k < 3; ++k)
        {
            m.sizes[c][k] = Monstey_Size(m.buffers[k][c].size());
            m.lods[c][k] = m.sizes[c][k];
        }
        m.lods[c][3] = Monstey_Size(chunks.size());
        if (chunks.size() == 1 && Monstey_HasPartBands(chunks.front()))
            Monstey_SetContainerBands(m.lods[c], chunks.front());
        // Multi-draw containers have a proven full-base native fallback. Their unused
        // serialized prefix words remain byte-exact; non-base chunks were separated above.
    }
    Monstey_MRequire(m.parts.size() <= 65535, "Native draw-start UINT16 overflow");
    m.h[3] = Monstey_Size(m.lods.size());
    m.h[4] = Monstey_Size(m.parts.size());
    Monstey_PutU32(m.descPrefix, 8, m.h[3]);
    Monstey_ExpandModelConnections(m, native, children);
    m.h[9] = Monstey_Size(m.layouts.size());
    std::size_t attributes = 0;
    for (const auto &layout : m.layouts)
        attributes += layout.size();
    m.h[10] = Monstey_Size(attributes);
    const auto allocation = Monstey_Allocation(m);
    m.h[5] = std::max(m.h[5], static_cast<std::uint32_t>(allocation["required_max_draws"].as_uint()));
    Monstey_MRequire(Monstey_Allocation(m)["safe"].as_bool(), "Native allocation validation failed");
    auto out = Monstey_SerializeModel(m);
    const auto check = Monstey_LoadModel(out);
    Monstey_MRequire(Monstey_SerializeModel(check) == out && check.bones == native.bones &&
                         check.infoPrefix == m.infoPrefix && check.lodSection == m.lodSection &&
                         check.descPrefix == m.descPrefix && check.h[1] == native.h[1] && check.h[7] == native.h[7] &&
                         check.h[8] == native.h[8] &&
                         std::equal(native.infoPrefix.begin(),
                                    native.infoPrefix.begin() + static_cast<std::ptrdiff_t>(native.h[1] * 4ull),
                                    check.infoPrefix.begin()),
                     "MMSH native binding/readback invariant failed");
    return out;
}

} // namespace Monstey_ModelPrivate
using namespace Monstey_ModelPrivate;

Monstey_Json Monstey_ModelRoundtrip(const std::filesystem::path &mesh, const std::filesystem::path &output)
{
    const std::array<std::filesystem::path, 1> sources = {mesh};
    Monstey_RequireOfflineOutput(output, sources);
    const auto raw = Monstey_ReadFile(mesh);
    const auto m = Monstey_LoadModel(raw);
    const auto encoded = Monstey_SerializeModel(m);
    Monstey_MRequire(encoded == raw, "Native model no-op roundtrip changed bytes");
    Monstey_WriteNewFile(output, encoded);
    return Monstey_J::object({{"output", Monstey_Utf8(output.wstring())},
                              {"sha256", Monstey_HashSha256(encoded)},
                              {"byte_exact", true},
                              {"runtime_verified", false}});
}

} // namespace Monstey
