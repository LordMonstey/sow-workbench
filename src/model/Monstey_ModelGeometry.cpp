#include "Monstey_ModelPrivate.hpp"

namespace Monstey
{
namespace Monstey_ModelPrivate
{

void Monstey_SetPartBands(Monstey_Part &part, std::span<const std::uint8_t> bands)
{
    Monstey_MRequire(part.indices.size() % 3 == 0 && bands.size() == part.indices.size() / 3,
                     "Triangle-band face count differs from current indices");
    for (const auto band : bands)
        Monstey_MRequire(band <= 16, "Triangle-band ordinal must be 0-16");
    part.triangle_bands = std::vector<std::uint8_t>(bands.begin(), bands.end());
    part.triangle_band_topology_sha256 = Monstey_ModelTriangleTopologyHash(part.indices);
}

bool Monstey_HasPartBands(const Monstey_Part &part)
{
    if (part.triangle_band_topology_sha256.empty())
        return false;
    Monstey_MRequire(part.triangle_band_topology_sha256 == Monstey_ModelTriangleTopologyHash(part.indices) &&
                         part.triangle_bands.size() == part.indices.size() / 3,
                     "Stale triangle-band index topology; assign current per-face bands explicitly");
    for (const auto band : part.triangle_bands)
        Monstey_MRequire(band <= 16, "Triangle-band ordinal must be 0-16");
    return true;
}

void Monstey_ResolvePartBands(Monstey_Part &part, const Monstey_Part &baseline, const Monstey_J &options)
{
    if (options.contains("triangle_band"))
    {
        const auto band = options["triangle_band"].as_uint();
        Monstey_MRequire(band <= 16, "triangle_band must be an explicit ordinal 0-16");
        Monstey_SetPartBands(part, std::vector<std::uint8_t>(part.indices.size() / 3, static_cast<std::uint8_t>(band)));
        return;
    }
    if (Monstey_HasPartBands(part))
        return;
    if (Monstey_HasPartBands(baseline))
    {
        if (part.indices == baseline.indices)
        {
            Monstey_SetPartBands(part, baseline.triangle_bands);
            return;
        }
        if (!baseline.triangle_bands.empty() &&
            std::all_of(baseline.triangle_bands.begin(), baseline.triangle_bands.end(),
                        [&](const auto band) { return band == baseline.triangle_bands.front(); }))
        {
            Monstey_SetPartBands(part,
                                 std::vector<std::uint8_t>(part.indices.size() / 3, baseline.triangle_bands.front()));
            return;
        }
    }
    Monstey_MRequire(part.indices == baseline.indices,
                     "Changed topology needs current per-face native triangle bands or explicit triangle_band");
}

void Monstey_PackPartBands(Monstey_Part &part)
{
    if (!Monstey_HasPartBands(part) || std::is_sorted(part.triangle_bands.begin(), part.triangle_bands.end()))
        return;
    std::vector<std::size_t> faces(part.triangle_bands.size());
    std::iota(faces.begin(), faces.end(), 0);
    std::stable_sort(faces.begin(), faces.end(),
                     [&](const auto a, const auto b) { return part.triangle_bands[a] < part.triangle_bands[b]; });
    auto indices = part.indices;
    auto bands = part.triangle_bands;
    for (std::size_t f = 0; f < faces.size(); ++f)
    {
        bands[f] = part.triangle_bands[faces[f]];
        std::copy_n(part.indices.begin() + static_cast<std::ptrdiff_t>(faces[f] * 3), 3,
                    indices.begin() + static_cast<std::ptrdiff_t>(f * 3));
    }
    part.indices = std::move(indices);
    Monstey_SetPartBands(part, bands);
}

void Monstey_SetContainerBands(std::array<std::uint32_t, 20> &row, const Monstey_Part &part)
{
    Monstey_MRequire(Monstey_HasPartBands(part), "Container needs qualified triangle membership");
    std::array<std::uint32_t, 17> counts{};
    for (const auto band : part.triangle_bands)
        ++counts[band];
    std::uint32_t sum = 0;
    for (std::size_t k = 0; k < 16; ++k)
    {
        sum += counts[k];
        Monstey_MRequire(sum <= 65535, "Native triangle-prefix boundary exceeds UINT16");
        row[4 + k] = sum;
    }
}

Monstey_ModelPalette Monstey_VertexModelPalette(const Monstey_Part &source, std::size_t vertex, bool preserveZero)
{
    const auto &joints = source.channels.at("JOINTS").v;
    const auto &weights = source.channels.at("WEIGHTS_U8").v;
    Monstey_ModelPalette result;
    for (unsigned k = 0; k < 4; ++k)
        if (preserveZero || weights[vertex * 4 + k] > 0)
            result.set(static_cast<std::size_t>(joints[vertex * 4 + k]));
    return result;
}

std::vector<Monstey_Part> Monstey_SplitModelPart(const Monstey_Part &source, bool preserveZero)
{
    if (source.Count() <= 65535 && source.indices.size() / 3 <= 65535)
    {
        Monstey_ModelPalette palette;
        for (std::size_t i = 0; i < source.Count() && palette.count() <= Monstey_ModelDrawPaletteLimit; ++i)
            palette |= Monstey_VertexModelPalette(source, i, preserveZero);
        if (palette.count() <= Monstey_ModelDrawPaletteLimit)
            return {source};
    }
    std::vector<Monstey_Part> result;
    std::map<std::uint32_t, std::uint32_t> mapping;
    std::vector<std::uint32_t> ids, faces;
    std::vector<std::uint8_t> bands;
    Monstey_ModelPalette palette;
    const bool qualifiedBands = Monstey_HasPartBands(source);
    std::vector<bool> referenced(source.Count(), false);
    const auto flush = [&]()
    {
        if (ids.empty())
            return;
        Monstey_Part chunk;
        chunk.container = source.container;
        chunk.part = source.part;
        chunk.indices = faces;
        for (const auto &[name, ch] : source.channels)
        {
            Monstey_Channel a;
            a.width = ch.width;
            for (const auto i : ids)
                a.v.insert(a.v.end(), ch.v.begin() + static_cast<std::ptrdiff_t>(i * ch.width),
                           ch.v.begin() + static_cast<std::ptrdiff_t>((i + 1) * ch.width));
            chunk.channels[name] = std::move(a);
        }
        if (qualifiedBands)
            Monstey_SetPartBands(chunk, bands);
        result.push_back(std::move(chunk));
        bands.clear();
        mapping.clear();
        ids.clear();
        faces.clear();
        palette.reset();
    };
    for (std::size_t i = 0; i < source.indices.size(); i += 3)
    {
        std::set<std::uint32_t> newIds;
        Monstey_ModelPalette trianglePalette;
        for (unsigned k = 0; k < 3; ++k)
        {
            const auto index = source.indices[i + k];
            if (!mapping.contains(index))
                newIds.insert(index);
            trianglePalette |= Monstey_VertexModelPalette(source, index, preserveZero);
        }
        if (faces.size() / 3 >= 65535 || ids.size() + newIds.size() > 65535 ||
            (palette | trianglePalette).count() > Monstey_ModelDrawPaletteLimit)
            flush();
        palette |= trianglePalette;
        if (qualifiedBands)
            bands.push_back(source.triangle_bands[i / 3]);
        for (unsigned k = 0; k < 3; ++k)
        {
            const auto id = source.indices[i + k];
            referenced[id] = true;
            if (!mapping.contains(id))
            {
                mapping[id] = Monstey_Size(ids.size());
                ids.push_back(id);
            }
            faces.push_back(mapping.at(id));
        }
    }
    for (std::size_t i = 0; i < referenced.size(); ++i)
        if (!referenced[i])
        {
            const auto vertexPalette = Monstey_VertexModelPalette(source, i, preserveZero);
            if (ids.size() >= 65535 || (palette | vertexPalette).count() > Monstey_ModelDrawPaletteLimit)
                flush();
            ids.push_back(Monstey_Size(i));
            palette |= vertexPalette;
        }
    flush();
    return result;
}

Monstey_EncodedPart Monstey_EncodeModelPart(const Monstey_Part &p, bool half, bool preserveZero)
{
    Monstey_EncodedPart out;
    const auto &joints = p.channels.at("JOINTS");
    const auto &weights = p.channels.at("WEIGHTS_U8");
    std::set<unsigned> palette;
    for (std::size_t i = 0; i < joints.v.size(); ++i)
        if (preserveZero || weights.v[i] > 0)
        {
            Monstey_MRequire(joints.v[i] >= 0 && joints.v[i] <= 255 && std::floor(joints.v[i]) == joints.v[i],
                             "Native HK palette value must fit UINT8");
            palette.insert(static_cast<unsigned>(joints.v[i]));
        }
    Monstey_MRequire(!palette.empty() && palette.size() <= Monstey_ModelDrawPaletteLimit,
                     "Authored draw palette exceeds the native 64-record gather");
    std::map<unsigned, std::uint8_t> slots;
    for (const auto j : palette)
    {
        slots[j] = static_cast<std::uint8_t>(out.palette.size());
        out.palette.push_back(static_cast<std::uint8_t>(j));
    }
    out.layout = {{0, static_cast<std::uint8_t>(half ? 12 : 3), 0, 0}, {0, 4, 6, 0}, {0, 4, 7, 0}};
    for (std::size_t v = 0; v < p.Count(); ++v)
    {
        for (unsigned k = 0; k < 4; ++k)
        {
            const auto x = k == 3 ? 1.0 : p.channels.at("POSITION").v[v * 3 + k];
            if (half)
                Monstey_AppendU16(out.vertices, Monstey_ToHalf(x));
            else
                Monstey_AddF32(out.vertices, x);
        }
        for (const unsigned k : {2u, 1u, 0u, 3u})
            out.vertices.push_back(static_cast<std::uint8_t>(weights.v[v * 4 + k]));
        for (unsigned k = 0; k < 4; ++k)
            out.vertices.push_back(
                preserveZero || weights.v[v * 4 + k] > 0 ? slots.at(static_cast<unsigned>(joints.v[v * 4 + k])) : 0);
    }
    std::vector<std::pair<std::string, unsigned>> channels;
    for (unsigned k = 0; k < 8; ++k)
        if (p.channels.contains("UV" + std::to_string(k)))
        {
            out.layout.push_back({1, 1, 4, static_cast<std::uint8_t>(k)});
            channels.emplace_back("UV" + std::to_string(k), 2);
        }
    for (const auto &[name, semantic] :
         std::array<std::pair<const char *, unsigned>, 3>{{{"NORMAL", 1}, {"TANGENT", 2}, {"BITANGENT", 3}}})
    {
        out.layout.push_back({1, 2, static_cast<std::uint8_t>(semantic), 0});
        channels.emplace_back(name, 3);
    }
    for (unsigned k = 0; k < 8; ++k)
        if (p.channels.contains("COLOR" + std::to_string(k)))
        {
            out.layout.push_back({1, 4, 5, static_cast<std::uint8_t>(k)});
            channels.emplace_back("COLOR" + std::to_string(k), 4);
        }
    // This writer emits no native instancing semantics. The native declaration
    // builder appends three instance elements to its fixed sixteen-element array.
    Monstey_MRequire(out.layout.size() <= 13, "Authored vertex layout exceeds the native 13-attribute profile");
    for (std::size_t v = 0; v < p.Count(); ++v)
        for (const auto &[name, width] : channels)
            for (unsigned k = 0; k < width; ++k)
            {
                const auto x = p.channels.at(name).v[v * width + k];
                if (name.starts_with("COLOR"))
                {
                    Monstey_MRequire(x >= 0 && x <= 255 && std::floor(x) == x, "Native colours must be bytes");
                    out.vertices.push_back(static_cast<std::uint8_t>(x));
                }
                else if (name == "NORMAL" || name == "TANGENT" || name == "BITANGENT")
                    Monstey_AppendU32(out.vertices, std::bit_cast<std::uint32_t>(static_cast<float>(x)));
                else
                    Monstey_AddF32(out.vertices, x);
            }
    for (const auto index : p.indices)
    {
        Monstey_MRequire(index < p.Count() && index <= 65535, "Native draw index overflow");
        Monstey_AppendU16(out.indices, static_cast<std::uint16_t>(index));
    }
    return out;
}

bool Monstey_NativeEquivalent(const Monstey_Part &current, const Monstey_Part &original, bool nativeExact)
{
    if (current.indices != original.indices || current.channels.size() != original.channels.size() ||
        current.triangle_bands != original.triangle_bands ||
        current.triangle_band_topology_sha256 != original.triangle_band_topology_sha256)
        return false;
    for (const auto &[name, ch] : original.channels)
    {
        if (!current.channels.contains(name))
            return false;
        const auto &actual = current.channels.at(name);
        if (actual.width != ch.width || actual.v.size() != ch.v.size())
            return false;
        for (std::size_t i = 0; i < ch.v.size(); ++i)
        {
            const double expected = name == "POSITION" && !nativeExact
                                        ? static_cast<double>(static_cast<float>(ch.v[i] / 100)) * 100
                                        : ch.v[i];
            if (std::isnan(actual.v[i]) && std::isnan(expected))
            {
                if (std::bit_cast<std::uint32_t>(static_cast<float>(actual.v[i])) !=
                    std::bit_cast<std::uint32_t>(static_cast<float>(expected)))
                    return false;
            }
            else if (actual.v[i] != expected)
                return false;
        }
    }
    return true;
}

} // namespace Monstey_ModelPrivate
using namespace Monstey_ModelPrivate;

std::string Monstey_ModelTriangleTopologyHash(std::span<const std::uint32_t> indices)
{
    Monstey_MRequire(indices.size() <= 24000000 && indices.size() % 3 == 0,
                     "Triangle-band topology must contain bounded triangle indices");
    Monstey_Bytes bytes;
    Monstey_AppendU64(bytes, static_cast<std::uint64_t>(indices.size()));
    for (const auto index : indices)
        Monstey_AppendU32(bytes, index);
    return Monstey_HashSha256(bytes);
}

void Monstey_ModelSetTriangleBands(Monstey_ModelPart &part, std::span<const std::uint8_t> bands)
{
    Monstey_MRequire(part.indices.size() % 3 == 0 && bands.size() == part.indices.size() / 3,
                     "Triangle-band face count differs from current indices");
    for (const auto band : bands)
        Monstey_MRequire(band <= 16, "Triangle-band ordinal must be 0-16");
    part.triangle_bands = std::vector<std::uint8_t>(bands.begin(), bands.end());
    part.triangle_band_topology_sha256 = Monstey_ModelTriangleTopologyHash(part.indices);
}

} // namespace Monstey
