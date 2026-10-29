#include "Monstey_Model.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <set>
using namespace Monstey;
namespace
{
void Monstey_F32(Monstey_Bytes &b, float f)
{
    Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(f));
}
Monstey_Bytes Monstey_Fixture(unsigned triangles, unsigned paletteCount = 1)
{
    const unsigned vertices = triangles * 3;
    Monstey_Bytes vb, ib;
    for (unsigned i = 0; i < vertices; ++i)
    {
        for (auto v : {static_cast<std::uint16_t>(i % 3 == 1 ? 0x3c00 : 0),
                       static_cast<std::uint16_t>(i % 3 == 2 ? 0x3c00 : 0), std::uint16_t{0}, std::uint16_t{0x3c00}})
            Monstey_AppendU16(vb, v);
        vb.insert(vb.end(), {0, 0, 255, 0});
        for (unsigned k = 0; k < 4; ++k)
            vb.push_back(static_cast<std::uint8_t>((i / 3) % paletteCount));
    }
    for (unsigned i = 0; i < vertices; ++i)
    {
        Monstey_AppendU16(vb, static_cast<std::uint16_t>(i % 3 == 1 ? 32767 : 0));
        Monstey_AppendU16(vb, static_cast<std::uint16_t>(i % 3 == 2 ? 32767 : 0));
        for (auto v : {0u, 0u, 0x3c00u, 0u, 0x3c00u, 0u, 0u, 0x3c00u, 0u, 0x3c00u, 0u, 0u})
            Monstey_AppendU16(vb, static_cast<std::uint16_t>(v));
        vb.insert(vb.end(), {11, 33, 77, 255});
        Monstey_AppendU16(ib, static_cast<std::uint16_t>(i));
    }
    Monstey_Bytes desc;
    for (auto x : {0xebaec3fau, 1u, 1u, 0u, 0u, static_cast<unsigned>(vb.size()), static_cast<unsigned>(ib.size()),
                   paletteCount})
        Monstey_AppendU32(desc, x);
    std::array<std::uint32_t, 18> h{};
    h[0] = 0x1a1541bc;
    h[1] = 1;
    h[2] = 1;
    h[3] = 1;
    h[4] = 1;
    h[5] = 1;
    h[6] = 1;
    h[7] = 1;
    h[9] = 1;
    h[10] = 8;
    h[16] = 256;
    h[17] = 256 * 40;
    Monstey_Bytes info;
    for (auto x : h)
        Monstey_AppendU32(info, x);
    Monstey_AppendU32(info, 0x12345678);
    for (unsigned i = 0; i < 4; ++i)
        Monstey_AppendU32(info, 0);
    for (auto x : {static_cast<unsigned>(vb.size()), static_cast<unsigned>(ib.size()), paletteCount, 1u})
        Monstey_AppendU32(info, x);
    for (unsigned i = 0; i < 16; ++i)
        Monstey_AppendU16(info, static_cast<std::uint16_t>(triangles));
    for (auto x : {0u, 0u, vertices, 0u, vertices, 0u, paletteCount, 0x11223344u, 0x55667788u, 0u})
        Monstey_AppendU32(info, x);
    for (auto x : {0x8e3e068eu, 0u, 0u, 1u, 0x37d749a6u, 8u})
        Monstey_AppendU32(info, x);
    constexpr std::array<std::array<std::uint8_t, 4>, 8> attrs{{{{0, 12, 0, 0}},
                                                                {{0, 4, 6, 0}},
                                                                {{0, 4, 7, 0}},
                                                                {{1, 9, 4, 0}},
                                                                {{1, 12, 1, 0}},
                                                                {{1, 12, 2, 0}},
                                                                {{1, 12, 3, 0}},
                                                                {{1, 4, 5, 0}}}};
    for (auto a : attrs)
        info.insert(info.end(), a.begin(), a.end());
    Monstey_AppendU32(info, 0x93d9a424);
    for (unsigned i = 0; i < 256; ++i)
    {
        Monstey_AppendU32(info, 0x1000 + i);
        Monstey_AppendU16(info, static_cast<std::uint16_t>(i ? 0 : 65535));
        Monstey_AppendU16(info, static_cast<std::uint16_t>(i ? 0 : 255));
        for (auto x : {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f})
            Monstey_F32(info, x);
    }
    Monstey_Bytes data;
    Monstey_AppendU32(data, 0x95dbdb69);
    data.insert(data.end(), vb.begin(), vb.end());
    data.insert(data.end(), ib.begin(), ib.end());
    for (unsigned i = 0; i < paletteCount; ++i)
        data.push_back(static_cast<std::uint8_t>(i));
    Monstey_Bytes out;
    for (auto x : {0x48534d4du, 17u, static_cast<unsigned>(desc.size()), static_cast<unsigned>(info.size()),
                   static_cast<unsigned>(data.size())})
        Monstey_AppendU32(out, x);
    out.insert(out.end(), desc.begin(), desc.end());
    out.insert(out.end(), info.begin(), info.end());
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

Monstey_Json Monstey_RawDraws(std::span<const std::uint8_t> b)
{
    const auto info = 20ull + Monstey_U32(b, 8);
    const auto rows = info + 72 + 4ull * Monstey_U32(b, info + 4) + 16ull * Monstey_U32(b, info + 8) +
                      48ull * Monstey_U32(b, info + 12);
    const auto count = Monstey_U32(b, info + 16);
    Monstey_CheckRange(b, static_cast<std::size_t>(rows), 40ull * count);
    auto a = Monstey_Json::array();
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto p = rows + 40 * i;
        a.as_array().push_back(Monstey_Json::object({{"ordinal", static_cast<std::uint64_t>(i)},
                                                     {"row_offset", p},
                                                     {"vertices", Monstey_U32(b, p + 8)},
                                                     {"triangles", Monstey_U32(b, p + 16) / 3},
                                                     {"palette_count", Monstey_U32(b, p + 24)},
                                                     {"palette_offset_in_container", Monstey_U32(b, p + 20)}}));
    }
    return a;
}
void Monstey_WriteTest(const std::filesystem::path &path, const Monstey_Bytes &data)
{
    if (std::filesystem::exists(path))
        std::filesystem::remove(path);
    Monstey_WriteNewFile(path, data);
}
void Monstey_Vertex(Monstey_ModelPart &p, unsigned id, unsigned bone, bool zeroLanes = false)
{
    const auto add = [&](const char *name, unsigned width, std::initializer_list<double> values) {
        auto &channel = p.channels[name];
        channel.width = width;
        channel.values.insert(channel.values.end(), values);
    };
    add("POSITION", 3, {double(id % 64), double(id / 64), 0});
    add("NORMAL", 3, {0, 0, 1});
    add("TANGENT", 3, {1, 0, 0});
    add("BITANGENT", 3, {0, 1, 0});
    add("UV0", 2, {0.25, 0.75});
    add("UV1", 2, {double(id), 0.5});
    add("JOINTS", 4, {double(zeroLanes ? 0 : bone), double(bone), double(bone), double(bone)});
    add("WEIGHTS_U8", 4, {255, 0, 0, 0});
    add("COLOR0", 4, {double(id % 256), 17, 63, 255});
    add("COLOR1", 4, {double(id % 256), double(id / 256 % 256), 11, 255});
}
unsigned Monstey_Id(const Monstey_ModelPart &p, std::size_t vertex)
{
    return static_cast<unsigned>(p.channels.at("UV1").values[vertex * 2]);
}
Monstey_ModelPart Monstey_Part(unsigned triangles, bool shared = false, bool zeroLanes = false, unsigned orphans = 0,
                               bool mixedBands = false)
{
    Monstey_ModelPart p;
    if (shared)
        Monstey_Vertex(p, 0, 0);
    for (unsigned t = 0; t < triangles; ++t)
    {
        const auto bone = t + (shared ? 1u : 0u);
        if (shared)
        {
            const auto start = t * 2 + 1;
            Monstey_Vertex(p, start, bone);
            Monstey_Vertex(p, start + 1, bone);
            p.indices.insert(p.indices.end(), {0, start, start + 1});
        }
        else
            for (unsigned k = 0; k < 3; ++k)
            {
                const auto id = t * 3 + k;
                Monstey_Vertex(p, id, bone, zeroLanes);
                p.indices.push_back(id);
            }
    }
    const auto first = static_cast<unsigned>(p.channels.at("POSITION").values.size() / 3);
    for (unsigned i = 0; i < orphans; ++i)
        Monstey_Vertex(p, first + i, (triangles + i) % 256);
    std::vector<std::uint8_t> bands(triangles, 0);
    if (mixedBands)
        for (unsigned i = 0; i < triangles; ++i)
            bands[i] = static_cast<std::uint8_t>(i % 3);
    Monstey_ModelSetTriangleBands(p, bands);
    return p;
}
Monstey_Json Monstey_Check(const std::filesystem::path &dir, const std::string &name, const Monstey_ModelPart &authored,
                           unsigned donorTriangles = 1)
{
    const auto base = dir / Monstey_Wide(name + "-base.mesh"), out = dir / Monstey_Wide(name + "-out.mesh");
    Monstey_WriteTest(base, Monstey_Fixture(donorTriangles));
    auto scene = Monstey_ModelReadScene(base);
    scene.parts.at(0) = authored;
    const auto baselineHash = Monstey_HashSha256(base);
    if (std::filesystem::exists(out))
        std::filesystem::remove(out);
    const auto result = Monstey_ModelWriteScene(scene, base, out);
    const auto read = Monstey_ModelReadScene(out);
    const auto &expected = scene.parts[0];
    const auto vertices = expected.channels.at("POSITION").values.size() / 3;
    std::vector<unsigned> occurrences(vertices, 0), actualTriangles, expectedTriangles;
    std::vector<std::pair<std::array<unsigned, 3>, unsigned>> actualBands, expectedBands;
    for (std::size_t i = 0; i < expected.indices.size(); i += 3)
    {
        const std::array<unsigned, 3> triangle{expected.indices[i], expected.indices[i + 1], expected.indices[i + 2]};
        expectedTriangles.insert(expectedTriangles.end(), triangle.begin(), triangle.end());
        expectedBands.emplace_back(triangle, expected.triangle_bands[i / 3]);
    }
    const auto rawDraws = Monstey_RawDraws(Monstey_ReadFile(out));
    for (const auto &row : rawDraws.as_array())
        Monstey_Require(row["palette_count"].as_uint() <= 64, "Serialized palette count exceeds64");
    unsigned maximum = 0;
    for (const auto &part : read.parts)
    {
        Monstey_Require(part.channels.size() == expected.channels.size(), "Channel count changed");
        std::set<unsigned> palette;
        const auto count = part.channels.at("POSITION").values.size() / 3;
        Monstey_Require(count <= 65535 && part.indices.size() / 3 <= 65535, "Native draw quota exceeded");
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto id = Monstey_Id(part, i);
            Monstey_Require(id < vertices, "Vertex identity outside source");
            ++occurrences[id];
            for (const auto &[key, ch] : expected.channels)
            {
                const auto &outChannel = part.channels.at(key);
                Monstey_Require(outChannel.width == ch.width, "Channel width changed");
                for (unsigned k = 0; k < ch.width; ++k)
                    Monstey_Require(outChannel.values[i * ch.width + k] == ch.values[id * ch.width + k],
                                    "Vertex tuple changed in " + key);
            }
            for (unsigned k = 0; k < 4; ++k)
                palette.insert(static_cast<unsigned>(part.channels.at("JOINTS").values[i * 4 + k]));
        }
        maximum = std::max(maximum, static_cast<unsigned>(palette.size()));
        Monstey_Require(palette.size() <= 64, "Authored draw exceeds native gather64");
        Monstey_Require(part.triangle_bands.size() == part.indices.size() / 3, "Band count changed");
        for (std::size_t i = 0; i < part.indices.size(); i += 3)
        {
            const std::array<unsigned, 3> triangle{Monstey_Id(part, part.indices[i]),
                                                   Monstey_Id(part, part.indices[i + 1]),
                                                   Monstey_Id(part, part.indices[i + 2])};
            actualTriangles.insert(actualTriangles.end(), triangle.begin(), triangle.end());
            actualBands.emplace_back(triangle, part.triangle_bands[i / 3]);
        }
    }
    Monstey_Require(std::all_of(occurrences.begin(), occurrences.end(), [](auto n) { return n > 0; }),
                    "Source vertex lost");
    std::vector<bool> referenced(vertices, false);
    for (const auto index : expected.indices)
        referenced[index] = true;
    for (std::size_t i = 0; i < vertices; ++i)
        if (!referenced[i])
            Monstey_Require(occurrences[i] == 1, "Unreferenced vertex duplicated or lost");
    // Band packing can reorder triangles, but orientation and complete membership must survive.
    std::sort(actualBands.begin(), actualBands.end());
    std::sort(expectedBands.begin(), expectedBands.end());
    Monstey_Require(actualBands == expectedBands, "Triangles or band membership changed");
    if (std::all_of(expected.triangle_bands.begin(), expected.triangle_bands.end(), [](auto b) { return b == 0; }))
        Monstey_Require(actualTriangles == expectedTriangles, "Base triangle order changed");
    Monstey_Require(result["allocation"]["safe"].as_bool() && Monstey_HashSha256(base) == baselineHash,
                    "Allocation/source validation failed");
    const auto noop = dir / Monstey_Wide(name + "-noop.mesh");
    if (std::filesystem::exists(noop))
        std::filesystem::remove(noop);
    const auto nr = Monstey_ModelWriteScene(read, out, noop);
    Monstey_Require(nr["byte_exact_noop"].as_bool() && Monstey_ReadFile(noop) == Monstey_ReadFile(out),
                    "Authored read/no-op not exact");
    return Monstey_Json::object({{"name", name},
                                 {"draws", static_cast<std::uint64_t>(read.parts.size())},
                                 {"max_palette", maximum},
                                 {"vertices_in", static_cast<std::uint64_t>(vertices)},
                                 {"triangles", static_cast<std::uint64_t>(expected.indices.size() / 3)},
                                 {"output_sha256", Monstey_HashSha256(out)},
                                 {"source_unchanged", true},
                                 {"tuple_conservation", true},
                                 {"byte_exact_noop", true}});
}
void Monstey_RawControl(const std::filesystem::path &dir)
{
    const auto base = dir / L"raw65.mesh", out = dir / L"raw65-noop.mesh", edit = dir / L"raw65-edited.mesh";
    Monstey_WriteTest(base, Monstey_Fixture(65, 65));
    auto scene = Monstey_ModelReadScene(base);
    if (std::filesystem::exists(out))
        std::filesystem::remove(out);
    auto result = Monstey_ModelWriteScene(scene, base, out);
    Monstey_Require(result["byte_exact_noop"].as_bool() && Monstey_ReadFile(base) == Monstey_ReadFile(out),
                    "Raw65 no-op rejected");
    const auto glb = dir / L"raw65.glb", mapped = dir / L"raw65-mapped.mesh";
    for (const auto &path : {glb, mapped})
        if (std::filesystem::exists(path))
            std::filesystem::remove(path);
    Monstey_ModelExportGlb(base, glb);
    auto map = Monstey_Json::object();
    for (unsigned i = 0; i < 256; ++i)
        map[std::to_string(i)] = i;
    const auto mappedResult = Monstey_ModelConvert(
        glb, base, mapped, Monstey_Json::object({{"binding", "mapped"}, {"joint_map", map}, {"triangle_band", 0u}}));
    Monstey_Require(mappedResult["allocation"]["safe"].as_bool(), "Mapped allocation invalid");
    std::size_t mappedTriangles = 0;
    std::set<unsigned> mappedBones;
    for (const auto &part : Monstey_ModelReadScene(mapped).parts)
    {
        std::set<unsigned> palette;
        for (auto j : part.channels.at("JOINTS").values)
            palette.insert(static_cast<unsigned>(j));
        Monstey_Require(palette.size() <= 64, "Mapped output exceeds gather64");
        mappedTriangles += part.indices.size() / 3;
        for (std::size_t i = 0; i < part.channels.at("JOINTS").values.size(); ++i)
            if (part.channels.at("WEIGHTS_U8").values[i] > 0)
                mappedBones.insert(static_cast<unsigned>(part.channels.at("JOINTS").values[i]));
    }
    Monstey_Require(mappedTriangles == 65 && mappedBones.size() == 65, "Mapped path lost triangles/bones");
    scene.parts[0].channels.at("COLOR0").values[0] = 99;
    if (std::filesystem::exists(edit))
        std::filesystem::remove(edit);
    Monstey_ModelWriteScene(scene, base, edit);
    const auto reopened = Monstey_ModelReadScene(edit);
    Monstey_Require(reopened.parts.size() > 1, "Modified raw65 was not partitioned");
    std::size_t triangles = 0;
    for (const auto &part : reopened.parts)
    {
        std::set<unsigned> palette;
        for (auto j : part.channels.at("JOINTS").values)
            palette.insert(static_cast<unsigned>(j));
        Monstey_Require(palette.size() <= 64, "Modified raw65 retains oversized palette");
        triangles += part.indices.size() / 3;
    }
    Monstey_Require(triangles == 65 && reopened.parts[0].channels.at("COLOR0").values[0] == 99,
                    "Modified raw65 lost edit/triangles");
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        Monstey_Require(argc == 2, "Usage Monstey_ModelPaletteTests output-directory");
        const std::filesystem::path dir = argv[1];
        std::filesystem::create_directories(dir);
        auto cases = Monstey_Json::array();
        for (unsigned bones : {63u, 64u, 65u, 128u, 255u, 256u})
            cases.as_array().push_back(Monstey_Check(dir, "palette" + std::to_string(bones), Monstey_Part(bones)));
        cases.as_array().push_back(Monstey_Check(dir, "same-topology", Monstey_Part(65), 65));
        cases.as_array().push_back(Monstey_Check(dir, "zero-weight-lanes", Monstey_Part(65, false, true)));
        cases.as_array().push_back(Monstey_Check(dir, "shared-mixed-bands", Monstey_Part(65, true, false, 0, true)));
        cases.as_array().push_back(Monstey_Check(dir, "orphan-palette", Monstey_Part(1, false, false, 65)));
        auto vertexLimit = Monstey_Part(1, false, false, 65533);
        for (auto &j : vertexLimit.channels.at("JOINTS").values)
            j = 0;
        cases.as_array().push_back(Monstey_Check(dir, "vertex-limit", vertexLimit));
        auto triangleLimit = Monstey_Part(1);
        triangleLimit.indices.clear();
        for (unsigned i = 0; i < 65536; ++i)
            triangleLimit.indices.insert(triangleLimit.indices.end(), {0, 1, 2});
        Monstey_ModelSetTriangleBands(triangleLimit, std::vector<std::uint8_t>(65536, 0));
        cases.as_array().push_back(Monstey_Check(dir, "triangle-limit", triangleLimit));
        Monstey_RawControl(dir);
        const auto report = dir / L"report.json";
        if (std::filesystem::exists(report))
            std::filesystem::remove(report);
        Monstey_WriteNewText(
            report,
            Monstey_Json::object({{"schema", "Monstey.ModelPaletteTests/1"}, {"cases", cases}, {"raw65_control", true}})
                    .dump(2) +
                "\n");
        std::cout << "Native model palette tests: 13 groups passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
