#include "Monstey_Model.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace
{
void Monstey_AddFloat(Monstey_Bytes &b, float f)
{
    Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(f));
}
void Monstey_WriteTest(const std::filesystem::path &path, const Monstey_Bytes &data)
{
    if (std::filesystem::exists(path))
        std::filesystem::remove(path);
    Monstey_WriteNewFile(path, data);
}
Monstey_Bytes Monstey_MeshFixture(bool skinned = true)
{
    Monstey_Bytes vertices;
    const std::uint16_t positions[3][4] = {{0, 0, 0, 0x3c00}, {0x5640, 0, 0, 0x3c00}, {0, 0x5640, 0, 0x3c00}};
    for (const auto &p : positions)
    {
        for (const auto v : p)
            Monstey_AppendU16(vertices, v);
        if (skinned)
        {
            for (const auto v : {63u, 128u, 64u, 0u})
                vertices.push_back(static_cast<std::uint8_t>(v));
            for (const auto v : {2u, 1u, 0u, 1u})
                vertices.push_back(static_cast<std::uint8_t>(v));
        }
    }
    for (unsigned i = 0; i < 3; ++i)
    {
        Monstey_AppendU16(vertices, i == 1 ? 32767 : 0);
        Monstey_AppendU16(vertices, i == 2 ? 32767 : 0);
        for (const auto v : {0u, 0u, 0x3c00u, 0u, 0x3c00u, 0u, 0u, 0x3c00u, 0u, 0x3c00u, 0u, 0u})
            Monstey_AppendU16(vertices, static_cast<std::uint16_t>(v));
        for (const auto v : {11u + i, 33u + i, 77u + i, 255u})
            vertices.push_back(static_cast<std::uint8_t>(v));
    }
    Monstey_Bytes faces;
    for (const auto v : {0u, 1u, 2u})
        Monstey_AppendU16(faces, static_cast<std::uint16_t>(v));
    const Monstey_Bytes palette = skinned ? Monstey_Bytes{0, 2, 3} : Monstey_Bytes{};
    Monstey_Bytes desc;
    for (const auto v : {0xebaec3fau, 3u, 2u, 0u, 1u, 2u, 0u, 1u, 0xffffffffu})
        Monstey_AppendU32(desc, v);
    for (const auto size : {static_cast<std::uint32_t>(vertices.size()), static_cast<std::uint32_t>(faces.size()),
                            static_cast<std::uint32_t>(palette.size())})
        for (unsigned c = 0; c < 2; ++c)
            Monstey_AppendU32(desc, size);
    std::array<std::uint32_t, 18> h{};
    h[0] = 0x1a1541bc;
    h[2] = 3;
    h[3] = 2;
    h[4] = 2;
    h[5] = 1;
    h[9] = 1;
    h[10] = skinned ? 8 : 6;
    h[16] = 4;
    h[17] = 160;
    Monstey_Bytes info;
    for (const auto v : h)
        Monstey_AppendU32(info, v);
    info.insert(info.end(), 48, 0x22);
    for (unsigned c = 0; c < 2; ++c)
    {
        for (const auto v : {static_cast<std::uint32_t>(vertices.size()), static_cast<std::uint32_t>(faces.size()),
                             static_cast<std::uint32_t>(palette.size()), 1u})
            Monstey_AppendU32(info, v);
        for (unsigned i = 0; i < 16; ++i)
            Monstey_AppendU16(info, 1);
    }
    for (unsigned c = 0; c < 2; ++c)
        for (const auto v :
             {0u, 0u, 3u, 0u, 3u, 0u, static_cast<std::uint32_t>(palette.size()), 0x11223344u + c, 0x55667788u, 0u})
            Monstey_AppendU32(info, v);
    Monstey_AppendU32(info, 0x8e3e068e);
    Monstey_AppendU32(info, 0x37d749a6);
    Monstey_AppendU32(info, skinned ? 8 : 6);
    for (const auto &a : std::array<std::array<std::uint8_t, 4>, 8>{{{{0, 12, 0, 0}},
                                                                     {{0, 4, 6, 0}},
                                                                     {{0, 4, 7, 0}},
                                                                     {{1, 9, 4, 0}},
                                                                     {{1, 12, 1, 0}},
                                                                     {{1, 12, 2, 0}},
                                                                     {{1, 12, 3, 0}},
                                                                     {{1, 4, 5, 0}}}})
        if (skinned || (a[2] != 6 && a[2] != 7))
            info.insert(info.end(), a.begin(), a.end());
    Monstey_AppendU32(info, 0x93d9a424);
    const int parents[] = {-1, 0, 1, 0};
    const unsigned children[] = {2, 1, 0, 0};
    const float positionsCm[4][3] = {{0, 0, 0}, {0, 50, 0}, {10, 100, 0}, {-40, 0, 0}};
    for (unsigned i = 0; i < 4; ++i)
    {
        Monstey_AppendU32(info, 0x1000 + i);
        Monstey_AppendU16(info, static_cast<std::uint16_t>(parents[i]));
        Monstey_AppendU16(info, static_cast<std::uint16_t>(children[i]));
        for (const auto v : positionsCm[i])
            Monstey_AddFloat(info, v);
        for (const auto v : {0.0f, 0.0f, 0.0f, 1.0f, 1.0f})
            Monstey_AddFloat(info, v);
    }
    Monstey_Bytes data;
    Monstey_AppendU32(data, 0x95dbdb69);
    for (unsigned i = 0; i < 2; ++i)
        data.insert(data.end(), vertices.begin(), vertices.end());
    for (unsigned i = 0; i < 2; ++i)
        data.insert(data.end(), faces.begin(), faces.end());
    for (unsigned i = 0; i < 2; ++i)
        data.insert(data.end(), palette.begin(), palette.end());
    Monstey_Bytes out;
    for (const auto v : {0x48534d4du, 17u, static_cast<std::uint32_t>(desc.size()),
                         static_cast<std::uint32_t>(info.size()), static_cast<std::uint32_t>(data.size())})
        Monstey_AppendU32(out, v);
    out.insert(out.end(), desc.begin(), desc.end());
    out.insert(out.end(), info.begin(), info.end());
    out.insert(out.end(), data.begin(), data.end());
    return out;
}
Monstey_Bytes Monstey_SkelFixture()
{
    const std::string names("Root\0Arm\0Finger\0Leg\0", 20);
    Monstey_Bytes b(96, 0);
    Monstey_PutU32(b, 0, 0x4c454b53);
    Monstey_PutU32(b, 4, 17);
    Monstey_PutU32(b, 12, static_cast<std::uint32_t>(names.size()));
    Monstey_PutU32(b, 40, 4);
    Monstey_AppendU32(b, 0xdcdcff00);
    b.insert(b.end(), names.begin(), names.end());
    const unsigned offsets[] = {0, 5, 9, 16}, children[] = {2, 1, 0, 0};
    const float t[4][3] = {{0, 0, 0}, {0, 50, 0}, {10, 100, 0}, {-40, 0, 0}};
    for (unsigned i = 0; i < 4; ++i)
    {
        Monstey_AppendU32(b, offsets[i]);
        b.push_back(1);
        for (const auto v : t[i])
            Monstey_AddFloat(b, v);
        for (const auto v : {0.0f, 0.0f, 0.0f, 1.0f})
            Monstey_AddFloat(b, v);
        Monstey_AppendU32(b, children[i]);
    }
    return b;
}
Monstey_Bytes Monstey_SkelPieceFixture()
{
    auto source = Monstey_SkelFixture();
    Monstey_PutU32(source, 32, 3);
    Monstey_Bytes prefix;
    Monstey_AppendU32(prefix, 3);
    auto row = [&](std::uint8_t a, std::uint8_t b, std::uint32_t bits, std::uint32_t c, std::uint32_t d) {
        prefix.push_back(a);
        prefix.push_back(b);
        Monstey_AppendU32(prefix, bits);
        Monstey_AppendU32(prefix, c);
        Monstey_AppendU32(prefix, d);
    };
    row(1, 7, 0x3fa00000, 0x11223344, 0xffffffffu);
    row(255, 0, 0xffc01234, 0xffffffffu, 0);
    row(0, 255, 0x7f800000, 123, 0x89abcdef);
    Monstey_AppendU32(prefix, 0);
    Monstey_AppendU32(prefix, 1);
    row(3, 4, 0x80000000, 0, 0xffffffffu);
    source.insert(source.begin() + 96, prefix.begin(), prefix.end());
    source.insert(source.end(), {0xde, 0xad, 0xbe, 0xef, 0x80, 0, 0x7f});
    return source;
}

Monstey_Bytes Monstey_SkelClothFixture()
{
    auto source = Monstey_SkelPieceFixture();
    source.resize(source.size() - 7);
    Monstey_PutU32(source, 8, 64);
    Monstey_PutU32(source, 16, 4);
    Monstey_PutU32(source, 20, 4);
    Monstey_PutU32(source, 24, 4 * 37);
    Monstey_PutU32(source, 28, 1);
    Monstey_PutU32(source, 138, 1);
    Monstey_Bytes extra;
    auto text = [&](std::string_view value) {
        const auto offset = static_cast<std::uint32_t>(20 + extra.size());
        extra.insert(extra.end(), value.begin(), value.end());
        extra.push_back(0);
        return offset;
    };
    const auto channel = text("node-channel"), piece0 = text("piece0"), piece1 = text("piece1"),
               piece2 = text("piece2"), cloth0 = text("resources/first.cloth06"),
               cloth1 = text("resources/second.cloth06");
    source.insert(source.begin() + 188, extra.begin(), extra.end());
    Monstey_PutU32(source, 12, static_cast<std::uint32_t>(20 + extra.size()));
    Monstey_AppendU32(source, 1);
    for (const auto v : {channel, 4u, 0u})
        Monstey_AppendU32(source, v);
    // A marker inside retained channel bytes must never be mistaken for a section.
    Monstey_AppendU32(source, 0xdcdcff04);
    Monstey_AppendU32(source, 1);
    Monstey_AppendU32(source, 1);
    Monstey_AppendU32(source, channel);
    source.resize(source.size() + 32, 0);
    Monstey_AppendU32(source, 0xdcdcff01);
    for (const auto v : {channel, piece0, piece1, piece2})
        Monstey_AppendU32(source, v);
    Monstey_AppendU32(source, 0xdcdcff02);
    Monstey_AppendU32(source, 1);
    Monstey_AppendU32(source, 0xdcdcff03);
    source.insert(source.end(), {1, 0, 1, 0});
    Monstey_AppendU32(source, 0xdcdcff04);
    Monstey_AppendU32(source, 2);
    Monstey_AppendU32(source, cloth0);
    Monstey_AppendU32(source, cloth1);
    source.insert(source.end(), {0xed, 0xfe, 0xef, 0xbe, 0xff, 0, 0x80});
    return source;
}

Monstey_Bytes Monstey_NativeBuffer(const Monstey_Bytes &b, unsigned stream, unsigned container)
{
    const auto desc = 20u, materialCount = Monstey_U32(b, desc + 4), containers = Monstey_U32(b, desc + 8);
    Monstey_Require(stream < 3 && container < containers, "Test buffer lookup invalid");
    const auto table = desc + 12 + materialCount * 8;
    std::size_t at = 20ull + Monstey_U32(b, 8) + Monstey_U32(b, 12) + 4;
    for (unsigned k = 0; k < stream; ++k)
        for (unsigned c = 0; c < containers; ++c)
            at += Monstey_U32(b, table + (k * containers + c) * 4);
    for (unsigned c = 0; c < container; ++c)
        at += Monstey_U32(b, table + (stream * containers + c) * 4);
    const auto size = Monstey_U32(b, table + (stream * containers + container) * 4);
    Monstey_CheckRange(b, at, size);
    return Monstey_Bytes(b.begin() + static_cast<std::ptrdiff_t>(at),
                         b.begin() + static_cast<std::ptrdiff_t>(at + size));
}
std::size_t Monstey_NativeDrawTable(const Monstey_Bytes &b)
{
    const auto info = 20 + Monstey_U32(b, 8);
    return info + 72ull + Monstey_U32(b, info + 4) * 4ull + Monstey_U32(b, info + 8) * 16ull +
           Monstey_U32(b, info + 12) * 48ull;
}
struct Monstey_GlbTest
{
    Monstey_Json doc;
    Monstey_Bytes bin;
};
Monstey_GlbTest Monstey_ReadGlbTest(const std::filesystem::path &p)
{
    const auto b = Monstey_ReadFile(p);
    const auto n = Monstey_U32(b, 12);
    std::string json(reinterpret_cast<const char *>(b.data() + 20), n);
    auto doc = Monstey_Json::Parse(json);
    const auto at = 20 + n;
    const auto size = Monstey_U32(b, at);
    return {doc, Monstey_Bytes(b.begin() + static_cast<std::ptrdiff_t>(at + 8),
                               b.begin() + static_cast<std::ptrdiff_t>(at + 8 + size))};
}
Monstey_Bytes Monstey_SaveGlbTest(Monstey_GlbTest g)
{
    auto json = g.doc.dump();
    while (json.size() % 4)
        json.push_back(' ');
    while (g.bin.size() % 4)
        g.bin.push_back(0);
    Monstey_Bytes b;
    for (const auto v : {0x46546c67u, 2u, static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + g.bin.size()),
                         static_cast<std::uint32_t>(json.size()), 0x4e4f534au})
        Monstey_AppendU32(b, v);
    b.insert(b.end(), json.begin(), json.end());
    Monstey_AppendU32(b, static_cast<std::uint32_t>(g.bin.size()));
    Monstey_AppendU32(b, 0x004e4942);
    b.insert(b.end(), g.bin.begin(), g.bin.end());
    return b;
}
std::size_t Monstey_AccessorOffset(const Monstey_GlbTest &g, const std::string &channel, unsigned primitive = 0)
{
    const auto ai = g.doc["meshes"][primitive]["primitives"][0u]["attributes"][channel].as_uint();
    const auto vi = g.doc["accessors"][static_cast<std::size_t>(ai)]["bufferView"].as_uint();
    return static_cast<std::size_t>(
        g.doc["bufferViews"][static_cast<std::size_t>(vi)].value("byteOffset", 0u).as_uint() +
        g.doc["accessors"][static_cast<std::size_t>(ai)].value("byteOffset", 0u).as_uint());
}
Monstey_Bytes Monstey_CustomGlb(std::size_t triangles, bool mapped = false)
{
    Monstey_GlbTest g;
    g.doc = Monstey_Json::object({{"asset", Monstey_Json::object({{"version", "2.0"}})},
                                  {"bufferViews", Monstey_Json::array()},
                                  {"accessors", Monstey_Json::array()}});
    const auto accessor = [&](const std::vector<float> &values, unsigned width) {
        const auto p = g.bin.size();
        for (const auto v : values)
            Monstey_AddFloat(g.bin, v);
        const auto vi = g.doc["bufferViews"].size();
        g.doc["bufferViews"].as_array().push_back(
            Monstey_Json::object({{"buffer", 0u},
                                  {"byteOffset", static_cast<std::uint64_t>(p)},
                                  {"byteLength", static_cast<std::uint64_t>(values.size() * 4)}}));
        g.doc["accessors"].as_array().push_back(
            Monstey_Json::object({{"bufferView", static_cast<std::uint64_t>(vi)},
                                  {"componentType", 5126u},
                                  {"count", static_cast<std::uint64_t>(values.size() / width)},
                                  {"type", width == 2 ? "VEC2" : "VEC3"}}));
        return static_cast<std::uint64_t>(g.doc["accessors"].size() - 1);
    };
    auto attrs = Monstey_Json::object(
        {{"POSITION", accessor({0, 0, 0, 1, 0, 0, 0, 1, 0}, 3)}, {"TEXCOORD_0", accessor({0, 0, 1, 0, 0, 1}, 2)}});
    const auto at = g.bin.size();
    for (std::size_t i = 0; i < triangles; ++i)
        for (const auto v : {0u, 1u, 2u})
            Monstey_AppendU32(g.bin, v);
    const auto vi = g.doc["bufferViews"].size();
    g.doc["bufferViews"].as_array().push_back(
        Monstey_Json::object({{"buffer", 0u},
                              {"byteOffset", static_cast<std::uint64_t>(at)},
                              {"byteLength", static_cast<std::uint64_t>(triangles * 12)}}));
    const auto ai = g.doc["accessors"].size();
    g.doc["accessors"].as_array().push_back(Monstey_Json::object({{"bufferView", static_cast<std::uint64_t>(vi)},
                                                                  {"componentType", 5125u},
                                                                  {"count", static_cast<std::uint64_t>(triangles * 3)},
                                                                  {"type", "SCALAR"}}));
    g.doc["meshes"] = Monstey_Json::array({Monstey_Json::object(
        {{"primitives", Monstey_Json::array({Monstey_Json::object(
                            {{"attributes", attrs}, {"indices", static_cast<std::uint64_t>(ai)}})})}})});
    g.doc["nodes"] = Monstey_Json::array({Monstey_Json::object({{"mesh", 0u}})});
    g.doc["scenes"] = Monstey_Json::array({Monstey_Json::object({{"nodes", Monstey_Json::array({0u})}})});
    g.doc["scene"] = 0u;
    g.doc["buffers"] =
        Monstey_Json::array({Monstey_Json::object({{"byteLength", static_cast<std::uint64_t>(g.bin.size())}})});
    (void)mapped;
    return Monstey_SaveGlbTest(g);
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        const std::filesystem::path dir = argc > 1 ? argv[1] : L"model-test-output";
        std::filesystem::create_directories(dir);
        unsigned passed = 0;
        auto report = Monstey_Json::object();
        report["cases"] = Monstey_Json::array();
        const auto test = [&](const char *name, const std::function<void()> &fn) {
            try
            {
                fn();
            }
            catch (const std::exception &error)
            {
                throw std::runtime_error(std::string(name) + ": " + error.what());
            }
            ++passed;
            report["cases"].as_array().push_back(name);
        };
        const auto reject = [&](const std::function<void()> &fn) {
            bool failed = false;
            try
            {
                fn();
            }
            catch (const std::exception &)
            {
                failed = true;
            }
            Monstey_Require(failed, "Expected model rejection");
        };
        const auto fixture = Monstey_MeshFixture();
        const auto mesh = dir / L"synthetic.mesh", skel = dir / L"synthetic.skel", glb = dir / L"export.glb";
        Monstey_WriteTest(mesh, fixture);
        Monstey_WriteTest(skel, Monstey_SkelFixture());
        test("standalone_skel_inspection_preserves_names_hierarchy_and_bind_units", [&] {
            const auto bytes = Monstey_SkelFixture();
            const auto r = Monstey_ModelInspectSkeleton(bytes);
            Monstey_Require(r["format"].as_string() == "SKEL17" && r["bones"].as_uint() == 4 &&
                                r["bone_names"][2u].as_string() == "Finger" &&
                                r["entries"][2u]["parent_source"].as_int() == 1 &&
                                std::abs(r["entries"][2u]["global_bind_metres"][12u].as_number() - .1) < 1e-8 &&
                                r["bind_matrix_layout"].as_string() == "column_major" &&
                                r["source_sha256"].as_string() == Monstey_HashSha256(bytes),
                            "Standalone SKEL metadata or units differ");
        });
        test("skel_all_256_raw_flag_values_preserve_bytes_and_independent_metadata", [&] {
            const auto original = Monstey_SkelFixture();
            const auto baseline = Monstey_ModelInspectSkeleton(original);
            const auto start = static_cast<std::size_t>(baseline["bone_data_offset"].as_uint());
            const auto Monstey_WithoutFlagMetadata = [](Monstey_Json value) {
                value.as_object().erase("source_sha256");
                for (auto &entry : value["entries"].as_array()) {
                    entry.as_object().erase("active");
                    entry.as_object().erase("raw_flag_byte");
                    entry.as_object().erase("position_channel_enabled");
                    entry.as_object().erase("rotation_channel_enabled");
                }
                return value;
            };
            const auto unrelated = Monstey_WithoutFlagMetadata(baseline);
            std::uint64_t observed = 0, positionEnabled = 0, legacyActive = 0;
            for (unsigned raw = 0; raw < 256; ++raw) {
                auto bytes = original;
                for (std::size_t i = 0; i < 4; ++i)
                    bytes[start + i * 37 + 4] = static_cast<std::uint8_t>((raw + 53 * i) & 255u);
                const auto before = bytes;
                const auto beforeHash = Monstey_HashSha256(bytes);
                const auto result = Monstey_ModelInspectSkeleton(bytes);
                Monstey_Require(bytes == before && Monstey_HashSha256(bytes) == beforeHash &&
                                    result["source_sha256"].as_string() == beforeHash,
                                "Raw flag inspection changed input bytes or source identity");
                Monstey_Require(result["entries"].size() == 4 &&
                                    result["bone_data_offset"].as_uint() == start &&
                                    Monstey_WithoutFlagMetadata(result) == unrelated,
                                "Raw flag bits changed hierarchy, bind values, names or unrelated metadata");
                for (std::size_t i = 0; i < 4; ++i) {
                    const auto flag = bytes[start + i * 37 + 4];
                    const auto &entry = result["entries"][i];
                    const bool position = (flag & 1u) == 0;
                    Monstey_Require(entry["raw_flag_byte"].as_uint() == flag &&
                                        entry["active"].as_bool() == (flag != 0) &&
                                        entry["position_channel_enabled"].as_bool() == position &&
                                        entry["rotation_channel_enabled"].as_bool(),
                                    "Public raw flag, legacy Boolean or pose identity metadata differs");
                    ++observed;
                    positionEnabled += position ? 1 : 0;
                    legacyActive += flag != 0 ? 1 : 0;
                }
            }
            Monstey_Require(observed == 1024 && positionEnabled == 512 && legacyActive == 1020,
                            "Incomplete all-byte flag sweep");
            report["raw_flag_sweep"] = Monstey_Json::object(
                {{"byte_values", 256}, {"bones_per_case", 4}, {"flag_observations", observed},
                 {"position_identity_enabled", positionEnabled}, {"rotation_identity_enabled", observed},
                 {"legacy_active_true", legacyActive}, {"source_bytes_and_hashes_unchanged", true},
                 {"unrelated_metadata_unchanged", true}, {"upper_bit_meanings_inferred", false},
                 {"actual_motion_inferred", false}, {"pose_slot_removal_inferred", false}});
        });
        test("skel_upper_flag_bits_do_not_bypass_invalid_name_offset_rejection", [&] {
            auto bytes = Monstey_SkelFixture();
            const auto start = static_cast<std::size_t>(
                Monstey_ModelInspectSkeleton(bytes)["bone_data_offset"].as_uint());
            bytes[start + 4] = 0xfe;
            Monstey_PutU32(bytes, start, Monstey_U32(bytes, 12));
            const auto before = bytes;
            const auto hash = Monstey_HashSha256(bytes);
            reject([&] { (void)Monstey_ModelInspectSkeleton(bytes); });
            Monstey_Require(bytes == before && Monstey_HashSha256(bytes) == hash,
                            "Invalid-name rejection changed raw flags or input bytes");
        });
        test("skel_upper_flag_bits_do_not_bypass_truncated_bone_row_rejection", [&] {
            auto bytes = Monstey_SkelFixture();
            const auto start = static_cast<std::size_t>(
                Monstey_ModelInspectSkeleton(bytes)["bone_data_offset"].as_uint());
            bytes[start + 4] = 0xff;
            bytes.pop_back();
            const auto before = bytes;
            const auto hash = Monstey_HashSha256(bytes);
            reject([&] { (void)Monstey_ModelInspectSkeleton(bytes); });
            Monstey_Require(bytes == before && Monstey_HashSha256(bytes) == hash,
                            "Truncated-row rejection changed raw flags or input bytes");
        });
        test("standalone_skel_diagnoses_nonfinite_bits_and_strict_matching_rejects_them", [&] {
            auto bytes = Monstey_SkelFixture();
            const auto start = 96 + 4 + Monstey_U32(bytes, 12);
            Monstey_PutU32(bytes, start + 5, 0xffc01234);
            const auto r = Monstey_ModelInspectSkeleton(bytes);
            Monstey_Require(!r["finite_bindings"].as_bool() &&
                                r["entries"][0u]["translation_cm"][0u]["nonfinite"].as_bool() &&
                                r["entries"][0u]["global_bind_metres"].is_null(),
                            "Nonfinite SKEL source was silently repaired");
            const auto bad = dir / L"nonfinite.skel";
            Monstey_WriteTest(bad, bytes);
            reject([&] { Monstey_ModelReadScene(mesh, bad); });
            bytes.resize(start + 10);
            reject([&] { Monstey_ModelInspectSkeleton(bytes); });
        });
        test("skel_piece_lod_metadata_decodes_independent_raw_rows_and_native_sentinel", [&] {
            const auto bytes = Monstey_SkelPieceFixture();
            const auto r = Monstey_ModelInspectSkeleton(bytes);
            const auto &meta = r["piece_lod_metadata"];
            Monstey_Require(meta["tables_total"].as_uint() == 3 && meta["rows_total"].as_uint() == 4 &&
                                meta["tables"].size() == 3 && meta["source_offset"].as_uint() == 96 &&
                                meta["source_bytes"].as_uint() == 68,
                            "Independent prefix boundary/count mismatch");
            const auto &first = meta["tables"][0u]["rows"][0u];
            Monstey_Require(first["source_offset"].as_uint() == 100 && first["byte0"].as_uint() == 1 &&
                                first["byte1"].as_uint() == 7 && first["float2"].as_number() == 1.25 &&
                                first["word6"].as_uint() == 0x11223344 && first["word10"].as_uint() == 0xffffffffu &&
                                !first["native_sentinel_flag"].as_bool() &&
                                first["raw_hex"].as_string() == "01070000a03f44332211ffffffff",
                            "Independent serialized14-byte row or sentinel rule differs");
            Monstey_Require(meta["tables"][0u]["rows"][1u]["native_sentinel_flag"].as_bool() &&
                                meta["tables"][1u]["rows_total"].as_uint() == 0 &&
                                meta["tables"][2u]["rows"][0u]["source_offset"].as_uint() == 150,
                            "Non-sentinel zero or empty table semantics changed");
            Monstey_Require(meta["sha256"].as_string() ==
                                    Monstey_HashSha256(std::span<const std::uint8_t>(bytes).subspan(96, 68)) &&
                                r["bone_data_offset"].as_uint() == 188,
                            "Prefix snapshot hash or bone offset differs");
        });
        test("skel_resource_envelope_capability_is_explicit_for_unknown_prefix", [&] {
            const auto result = Monstey_ModelInspectSkeleton(Monstey_SkelFixture())["cloth_resources"];
            Monstey_Require(!result["section_locator_qualified"].as_bool() &&
                                !result["record_envelope_qualified"].as_bool() &&
                                !result["record_grammar_qualified"].as_bool() &&
                                !result.contains("record_envelope"),
                            "Unqualified SKEL prefix implied a resource record capability");
        });
        test("skel_resource_record_envelope_uses_exact_unaligned_lengths_and_retains_raw_tags", [&] {
            auto bytes = Monstey_SkelClothFixture();
            const auto records = static_cast<std::size_t>(
                Monstey_ModelInspectSkeleton(bytes)["cloth_resources"]["resource_records_offset"].as_uint());
            bytes.resize(records);
            Monstey_AppendU32(bytes, 0x12345678);
            Monstey_AppendU32(bytes, 5);
            bytes.insert(bytes.end(), {5, 255, 220, 220, 0});
            Monstey_AppendU32(bytes, 0);
            Monstey_AppendU32(bytes, 0);
            Monstey_AppendU32(bytes, 0xffffffff);
            Monstey_AppendU32(bytes, 0xdcdcff05);
            bytes.insert(bytes.end(), {0x71, 0x80, 0, 0xfe});
            const auto before = bytes;
            const auto hash = Monstey_HashSha256(bytes);
            const auto metadata = Monstey_ModelInspectSkeleton(bytes)["cloth_resources"];
            const auto &envelope = metadata["record_envelope"];
            const auto &rows = envelope["records"];
            Monstey_Require(metadata["section_locator_qualified"].as_bool() &&
                                metadata["record_envelope_qualified"].as_bool() &&
                                !metadata["record_grammar_qualified"].as_bool() &&
                                !metadata["authoring_supported"].as_bool() &&
                                rows.size() == 2 && rows[0u]["header_offset"].as_uint() == records &&
                                rows[0u]["raw_tag_word"].as_uint() == 0x12345678 &&
                                rows[0u]["payload_bytes"].as_uint() == 5 &&
                                rows[1u]["header_offset"].as_uint() == records + 13 &&
                                rows[1u]["raw_tag_word"].as_uint() == 0 &&
                                rows[1u]["payload_bytes"].as_uint() == 0 &&
                                envelope["raw_container_end_word"].as_uint() == 0xffffffff &&
                                envelope["section5_marker_offset"].as_uint() == records + 25,
                            "Resource byte envelope, opaque tags or capability boundary changed");
            Monstey_Require(rows[0u]["payload_sha256"].as_string() ==
                                    Monstey_HashSha256(std::span<const std::uint8_t>(bytes).subspan(records + 8, 5)) &&
                                rows[0u]["resource_name"] == metadata["resource_names"][0u] &&
                                rows[1u]["resource_name"] == metadata["resource_names"][1u] &&
                                envelope["following_sha256"].as_string() ==
                                    Monstey_HashSha256(std::span<const std::uint8_t>(bytes).subspan(records + 25)) &&
                                bytes == before && Monstey_HashSha256(bytes) == hash,
                            "Resource names, payload/tail bits or source snapshot changed");
        });
        test("skel_resource_zero_count_still_has_terminal_word_before_section5", [&] {
            auto bytes = Monstey_SkelClothFixture();
            const auto baseline = Monstey_ModelInspectSkeleton(bytes)["cloth_resources"];
            const auto section4 = static_cast<std::size_t>(baseline["section4_marker_offset"].as_uint());
            bytes.resize(section4 + 8);
            Monstey_PutU32(bytes, section4 + 4, 0);
            Monstey_AppendU32(bytes, 0);
            Monstey_AppendU32(bytes, 0xdcdcff05);
            const auto metadata = Monstey_ModelInspectSkeleton(bytes)["cloth_resources"];
            Monstey_Require(metadata["record_envelope_qualified"].as_bool() &&
                                metadata["resource_count"].as_uint() == 0 &&
                                metadata["record_envelope"]["records"].empty() &&
                                metadata["record_envelope"]["raw_container_end_word"].as_uint() == 0 &&
                                metadata["record_envelope"]["section5_marker_offset"].as_uint() == section4 + 12,
                            "Empty container omitted the native terminal word");
        });
        test("skel_resource_opaque_or_truncated_profiles_remain_inspectable_with_explicit_diagnostic", [&] {
            auto bytes = Monstey_SkelClothFixture();
            const auto metadata = Monstey_ModelInspectSkeleton(bytes)["cloth_resources"];
            const auto records = static_cast<std::size_t>(metadata["resource_records_offset"].as_uint());
            for (const auto size : {std::size_t{0}, std::size_t{4}, std::size_t{7}}) {
                auto candidate = bytes;
                candidate.resize(records + size);
                const auto before = candidate;
                const auto result = Monstey_ModelInspectSkeleton(candidate)["cloth_resources"];
                Monstey_Require(result["section_locator_qualified"].as_bool() &&
                                    !result["record_envelope_qualified"].as_bool() &&
                                    result.contains("record_envelope_error") && !result.contains("record_envelope") &&
                                    result["resource_names"] == metadata["resource_names"] &&
                                    candidate == before,
                                "Unqualified resource body hid known names or published partial records");
            }
        });
        test("skel_resource_oversized_body_and_wrong_following_marker_report_unknown_envelope", [&] {
            auto bytes = Monstey_SkelClothFixture();
            const auto records = static_cast<std::size_t>(
                Monstey_ModelInspectSkeleton(bytes)["cloth_resources"]["resource_records_offset"].as_uint());
            bytes.resize(records);
            for (const auto word : {0xbeeffeedu, 0u, 0xbeeffeedu, 0u, 0xdeaddeadu, 0xdcdcff05u})
                Monstey_AppendU32(bytes, word);
            const auto original = Monstey_ModelInspectSkeleton(bytes)["cloth_resources"];
            Monstey_Require(original["record_envelope_qualified"].as_bool(), "Valid control envelope rejected");
            for (const auto mutateSize : {false, true}) {
                auto candidate = bytes;
                if (mutateSize)
                    Monstey_PutU32(candidate, records + 4, UINT32_MAX);
                else
                    Monstey_PutU32(candidate, records + 20, 0xdcdcff04);
                const auto result = Monstey_ModelInspectSkeleton(candidate)["cloth_resources"];
                Monstey_Require(result["section_locator_qualified"].as_bool() &&
                                    !result["record_envelope_qualified"].as_bool() &&
                                    result.contains("record_envelope_error") && !result.contains("record_envelope") &&
                                    result["resource_names"] == original["resource_names"],
                                "Malformed body was qualified or destroyed known prefix metadata");
            }
        });
        test("skel_piece_nonfinite_and_negative_zero_bits_are_diagnosed_without_repair", [&] {
            const auto r = Monstey_ModelInspectSkeleton(Monstey_SkelPieceFixture());
            const auto &tables = r["piece_lod_metadata"]["tables"];
            Monstey_Require(!tables[0u]["rows"][1u]["finite_float"].as_bool() &&
                                tables[0u]["rows"][1u]["float2"]["nonfinite"].as_bool() &&
                                tables[0u]["rows"][1u]["float32_bits"].as_string() == "0xffc01234" &&
                                !tables[0u]["rows"][2u]["finite_float"].as_bool() &&
                                tables[2u]["rows"][0u]["float32_bits"].as_string() == "0x80000000",
                            "Native raw Float32 prefix bits were changed or rejected");
        });
        test("skel_piece_metadata_paging_preserves_complete_counts_and_hashes", [&] {
            const auto bytes = Monstey_SkelPieceFixture();
            const auto full = Monstey_ModelInspectSkeleton(bytes);
            auto options =
                Monstey_Json::object({{"table_offset", 0}, {"table_limit", 3}, {"row_offset", 1}, {"row_limit", 1}});
            const auto r = Monstey_ModelInspectSkeleton(bytes, options);
            const auto &m = r["piece_lod_metadata"];
            Monstey_Require(m["rows_total"].as_uint() == 4 && m["rows_shown"].as_uint() == 1 &&
                                m["rows_truncated"].as_bool() && m["tables"][0u]["rows"][0u]["index"].as_uint() == 1 &&
                                m["sha256"] == full["piece_lod_metadata"]["sha256"],
                            "Paged prefix preview lost counts, offsets or full-source hash");
            options = Monstey_Json::object({{"table_offset", 2}, {"table_limit", 1}, {"row_limit", 0}});
            const auto counts = Monstey_ModelInspectSkeleton(bytes, options);
            Monstey_Require(counts["piece_lod_metadata"]["tables"].size() == 1 &&
                                counts["piece_lod_metadata"]["tables"][0u]["index"].as_uint() == 2 &&
                                counts["piece_lod_metadata"]["rows_shown"].as_uint() == 0,
                            "Count-only metadata paging changed table indices");
            reject([&] { Monstey_ModelInspectSkeleton(bytes, Monstey_Json::object({{"row_limit", 10001}})); });
            reject([&] { Monstey_ModelInspectSkeleton(bytes, Monstey_Json::object({{"table_limit", 4097}})); });
            reject([&] { Monstey_ModelInspectSkeleton(bytes, Monstey_Json::object({{"row_offset", -1}})); });
            reject([&] { Monstey_ModelInspectSkeleton(bytes, Monstey_Json::object({{"unknown", 1}})); });
        });
        test("empty_skel_piece_pages_report_no_invented_truncation", [&] {
            const auto report = Monstey_ModelInspectSkeleton(
                Monstey_SkelFixture(), Monstey_Json::object({{"table_offset", 9}, {"row_offset", 9}}));
            const auto &metadata = report["piece_lod_metadata"];
            Monstey_Require(metadata["tables_total"].as_uint() == 0 && metadata["rows_total"].as_uint() == 0 &&
                                !metadata["tables_truncated"].as_bool() && !metadata["rows_truncated"].as_bool(),
                            "An empty source was reported to contain omitted metadata");
        });
        test("skel_piece_metadata_rejects_malformed_extents_without_guessing_fields", [&] {
            auto bytes = Monstey_SkelPieceFixture();
            Monstey_PutU32(bytes, 96, 0xffffffffu);
            reject([&] { Monstey_ModelInspectSkeleton(bytes); });
            bytes = Monstey_SkelPieceFixture();
            Monstey_PutU32(bytes, 32, 0xffffffffu);
            reject([&] { Monstey_ModelInspectSkeleton(bytes); });
            bytes = Monstey_SkelPieceFixture();
            bytes.resize(105);
            reject([&] { Monstey_ModelInspectSkeleton(bytes); });
            bytes = Monstey_SkelPieceFixture();
            Monstey_PutU32(bytes, 164, 0);
            reject([&] { Monstey_ModelInspectSkeleton(bytes); });
            bytes = Monstey_SkelPieceFixture();
            bytes.resize(335);
            reject([&] { Monstey_ModelInspectSkeleton(bytes); });
        });
        test("skel_piece_metadata_hashes_opaque_tail_and_dispatches_through_model_inspect", [&] {
            const auto bytes = Monstey_SkelPieceFixture();
            const auto r = Monstey_ModelInspectBytes(bytes);
            Monstey_Require(r["format"].as_string() == "SKEL17" && r["trailing_offset"].as_uint() == 336 &&
                                r["trailing_bytes"].as_uint() == 7 &&
                                r["trailing_sha256"].as_string() ==
                                    Monstey_HashSha256(std::span<const std::uint8_t>(bytes).subspan(336)),
                            "Opaque tail hash/extent or model-inspect dispatch differs");
            const auto file = dir / L"piece-metadata.skel";
            Monstey_WriteTest(file, bytes);
            const auto rfile = Monstey_ModelInspect(file);
            Monstey_Require(rfile["piece_lod_metadata"] == r["piece_lod_metadata"] && Monstey_ReadFile(file) == bytes,
                            "File inspection changed prefix bytes or metadata");
        });
        test("shared_model_inspection_forwards_only_qualified_skeleton_paging", [&] {
            const auto bytes = Monstey_SkelPieceFixture();
            const auto source = dir / L"paged-inspect.skel";
            Monstey_WriteTest(source, bytes);
            const auto options =
                Monstey_Json::object({{"table_offset", 2}, {"table_limit", 1}, {"row_offset", 0}, {"row_limit", 1}});
            const auto memory = Monstey_ModelInspectBytes(bytes, {}, options),
                       file = Monstey_ModelInspect(source, {}, options);
            Monstey_Require(memory["piece_lod_metadata"] == file["piece_lod_metadata"] &&
                                file["piece_lod_metadata"]["tables"][0u]["index"].as_uint() == 2,
                            "Public ModelInspect did not forward exact skeleton page");
            reject([&] { Monstey_ModelInspectBytes(fixture, {}, options); });
            reject([&] { Monstey_ModelInspect(source, {}, Monstey_Json::object({{"unknown", 1}})); });
            reject([&] { Monstey_ModelInspectBytes(bytes, {}, Monstey_Json::array()); });
            Monstey_Require(Monstey_ReadFile(source) == bytes, "Paged inspection modified its native source");
        });
        test("skel_metadata_inspection_does_not_change_existing_native_export", [&] {
            const auto bytes = Monstey_SkelPieceFixture();
            const auto source = dir / L"piece-export.skel";
            Monstey_WriteTest(source, bytes);
            const auto view = dir / L"piece-export.glb", out = dir / L"piece-roundtrip.mesh";
            if (std::filesystem::exists(view))
                std::filesystem::remove(view);
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            Monstey_ModelExportGlb(mesh, view, source);
            const auto r = Monstey_ModelConvert(view, mesh, out, Monstey_Json::object({{"binding", "native"}}));
            Monstey_Require(r["byte_exact_noop"].as_bool() && Monstey_ReadFile(out) == fixture &&
                                Monstey_ReadFile(source) == bytes,
                            "Inspection metadata affected native writer or source SKEL");
        });
        test("skel_cloth_resources_follow_exact_sequential_locator_and_resolve_rows", [&] {
            const auto bytes = Monstey_SkelClothFixture();
            const auto r = Monstey_ModelInspectSkeleton(bytes);
            const auto &resources = r["cloth_resources"], &tables = r["piece_lod_metadata"]["tables"];
            const auto &rows = tables[0u]["rows"];
            Monstey_Require(resources["section_locator_qualified"].as_bool() &&
                                resources["resource_count"].as_uint() == 2 &&
                                resources["resource_names"][0u].as_string() == "resources/first.cloth06" &&
                                resources["resource_names"][1u].as_string() == "resources/second.cloth06" &&
                                resources["node_channel_count"].as_uint() == 1 &&
                                resources["additional_node_count"].as_uint() == 1,
                            "Sequential cloth-resource count/names differ");
            for (std::size_t i = 0; i < rows.size(); ++i)
                Monstey_Require(rows[i]["cloth_resource_index"] == rows[i]["word10"] &&
                                    rows[i]["cloth_reference_valid"].as_bool(),
                                "Qualified row index/reference differs from raw word10");
            Monstey_Require(rows[0u]["cloth_resource_path"].is_null() &&
                                rows[1u]["cloth_resource_path"] == resources["resource_names"][0u] &&
                                rows[2u]["cloth_resource_path"] == resources["resource_names"][1u] &&
                                tables[2u]["rows"][0u]["cloth_resource_path"].is_null() &&
                                !r["piece_lod_metadata"]["row_semantics_qualified"].as_bool() &&
                                !r["piece_lod_metadata"]["authoring_supported"].as_bool() &&
                                !resources["record_grammar_qualified"].as_bool(),
                            "Sentinel/path or remaining qualification flags differ");
            const auto records = static_cast<std::size_t>(resources["resource_records_offset"].as_uint());
            Monstey_Require(resources["retained_records_and_tail_bytes"].as_uint() == 7 &&
                                resources["retained_records_and_tail_sha256"].as_string() ==
                                    Monstey_HashSha256(std::span<const std::uint8_t>(bytes).subspan(records)),
                            "Opaque resource records/tail were not retained exactly");
        });
        test("skel_cloth_invalid_index_is_diagnostic_and_zero_count_is_known", [&] {
            auto bytes = Monstey_SkelClothFixture();
            Monstey_PutU32(bytes, 138, 2);
            const auto bad = Monstey_ModelInspectSkeleton(bytes);
            const auto &row = bad["piece_lod_metadata"]["tables"][0u]["rows"][2u];
            Monstey_Require(row["cloth_resource_index"].as_uint() == 2 &&
                                !row["cloth_reference_valid"].as_bool() && row["cloth_resource_path"].is_null(),
                            "Out-of-range cloth index was silently resolved or rejected");
            const auto marker = static_cast<std::size_t>(bad["cloth_resources"]["section4_marker_offset"].as_uint());
            Monstey_PutU32(bytes, marker + 4, 0);
            bytes.resize(marker + 8);
            const auto empty = Monstey_ModelInspectSkeleton(bytes);
            Monstey_Require(empty["cloth_resources"]["section_locator_qualified"].as_bool() &&
                                empty["cloth_resources"]["resource_count"].as_uint() == 0 &&
                                empty["cloth_resources"]["resource_names"].empty() &&
                                !empty["piece_lod_metadata"]["tables"][0u]["rows"][1u]["cloth_reference_valid"].as_bool(),
                            "Empty resource container became unknown or accepted index zero");
        });
        test("skel_cloth_opaque_and_unqualified_paths_report_unknown_without_magic_scan", [&] {
            auto bytes = Monstey_SkelPieceFixture();
            const auto native = Monstey_SkelClothFixture();
            const auto known = Monstey_ModelInspectSkeleton(native);
            const auto marker = static_cast<std::size_t>(known["cloth_resources"]["name_offsets_marker_offset"].as_uint());
            bytes.insert(bytes.end(), native.begin() + static_cast<std::ptrdiff_t>(marker), native.end());
            auto r = Monstey_ModelInspectSkeleton(bytes);
            Monstey_Require(!r["cloth_resources"]["section_locator_qualified"].as_bool() &&
                                r["cloth_resources"]["resource_count"].is_null() &&
                                r["cloth_resources"]["resource_names"].is_null() &&
                                r["piece_lod_metadata"]["tables"][0u]["rows"][0u]["cloth_reference_valid"].as_bool() &&
                                r["piece_lod_metadata"]["tables"][0u]["rows"][1u]["cloth_reference_valid"].is_null(),
                            "Opaque tail was scanned or unknown non-null reference became valid");
            bytes = native;
            Monstey_PutU32(bytes, 64, 1);
            r = Monstey_ModelInspectSkeleton(bytes);
            Monstey_Require(!r["cloth_resources"]["section_locator_qualified"].as_bool() &&
                                r["cloth_resources"]["unknown_reason"].as_string() ==
                                    "additional_node_property_layout_unqualified",
                            "Unqualified additional-node property path was guessed");
            bytes = native;
            Monstey_PutU32(bytes, marker, 0);
            bytes.insert(bytes.end(), native.begin() + static_cast<std::ptrdiff_t>(marker), native.end());
            reject([&] { Monstey_ModelInspectSkeleton(bytes); });
        });
        test("skel_cloth_locator_rejects_malformed_extents_markers_and_pool_names", [&] {
            const auto original = Monstey_SkelClothFixture();
            const auto r = Monstey_ModelInspectSkeleton(original);
            const auto boneEnd = static_cast<std::size_t>(r["trailing_offset"].as_uint()),
                       marker1 = static_cast<std::size_t>(r["cloth_resources"]["name_offsets_marker_offset"].as_uint()),
                       marker4 = static_cast<std::size_t>(r["cloth_resources"]["section4_marker_offset"].as_uint());
            auto changed = [&](std::size_t at, std::uint32_t value) {
                auto bytes = original;
                Monstey_PutU32(bytes, at, value);
                reject([&] { Monstey_ModelInspectSkeleton(bytes); });
            };
            changed(boneEnd, 0xffffffffu);
            changed(boneEnd + 8, 5);
            changed(boneEnd + 12, 5);
            changed(boneEnd + 20, 0xffffffffu);
            changed(16, 0xffffffffu);
            changed(28, 0xffffffffu);
            changed(marker1 + 4, Monstey_U32(original, 12));
            changed(marker1 + 20, 0);
            changed(marker1 + 28, 0);
            changed(marker4, 0);
            changed(marker4 + 4, 0xffffffffu);
            changed(marker4 + 8, Monstey_U32(original, 12));
            changed(marker4 + 8, 19);
            for (const auto size : {boneEnd, boneEnd + 4, marker1, marker1 + 8, marker4 + 4, marker4 + 11})
            {
                auto bytes = original;
                bytes.resize(size);
                reject([&] { Monstey_ModelInspectSkeleton(bytes); });
            }
            auto bytes = original;
            bytes[187 + Monstey_U32(original, 12) - 20] = 'x';
            reject([&] { Monstey_ModelInspectSkeleton(bytes); });
        });
        test("skel_cloth_resolution_survives_paging_dispatch_and_source_noop", [&] {
            const auto bytes = Monstey_SkelClothFixture();
            const auto source = dir / L"cloth-resources.skel";
            Monstey_WriteTest(source, bytes);
            const auto full = Monstey_ModelInspectSkeleton(bytes);
            const auto options = Monstey_Json::object({{"table_limit", 1}, {"row_offset", 1}, {"row_limit", 1}});
            const auto memory = Monstey_ModelInspectBytes(bytes, {}, options),
                       file = Monstey_ModelInspect(source, {}, options);
            Monstey_Require(memory["cloth_resources"] == full["cloth_resources"] &&
                                memory["piece_lod_metadata"] == file["piece_lod_metadata"] &&
                                memory["piece_lod_metadata"]["rows_shown"].as_uint() == 1 &&
                                memory["piece_lod_metadata"]["tables"][0u]["rows"][0u]["cloth_resource_path"].as_string() ==
                                    "resources/first.cloth06" && Monstey_ReadFile(source) == bytes,
                            "Resource resolution changed with row page or mutated source");
            const auto noRows = Monstey_ModelInspectSkeleton(bytes, Monstey_Json::object({{"row_limit", 0}}));
            Monstey_Require(noRows["cloth_resources"] == full["cloth_resources"] &&
                                noRows["piece_lod_metadata"]["rows_shown"].as_uint() == 0,
                            "Count-only inspection skipped resource qualification");
        });
        test("native_draw_parameter_and_color_storage_metadata_is_exact", [&] {
            const auto result = Monstey_ModelNativePartMetadata(fixture);
            Monstey_Require(result["source_sha256"].as_string() == Monstey_HashSha256(fixture) &&
                                result["containers"].as_uint() == 2 && result["parts"].size() == 2,
                            "Metadata snapshot/container counts changed");
            for (std::size_t i = 0; i < 2; ++i)
            {
                const auto &part = result["parts"][i];
                Monstey_Require(part["container"].as_uint() == i && part["part"].as_uint() == 0 &&
                                    part["native_words"].size() == 10 &&
                                    part["draw_parameter_words"][0u].as_uint() == 0x11223344u + i &&
                                    part["draw_parameter_words"][1u].as_uint() == 0x55667788u &&
                                    part["color0_storage"]["type"].as_uint() == 4 &&
                                    part["color0_storage"]["storage_width"].as_uint() == 4 &&
                                    part["color0_storage"]["storage_bytes"].as_uint() == 4,
                                "Native draw parameters or byte-color storage changed");
            }
        });
        test("native_inspect_eight_channels_and_hk_permutation", [&] {
            const auto r = Monstey_ModelInspect(mesh, skel);
            Monstey_Require(r["parts"].size() == 2 && r["parts"][0u]["channels"].size() == 8, "Native channel count");
            const auto &bones = r["rig"]["bones"];
            Monstey_Require(bones[2u]["source_index"].as_uint() == 3 && bones[3u]["source_index"].as_uint() == 2,
                            "Havok sibling-block permutation");
        });
        test("native_byte_exact_read_serialize", [&] {
            const auto out = dir / L"roundtrip.mesh";
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            Monstey_ModelRoundtrip(mesh, out);
            Monstey_Require(Monstey_ReadFile(out) == fixture, "Native roundtrip changed unknown bytes");
        });
        test("export_glb_all_channels_matching_skel", [&] {
            if (std::filesystem::exists(glb))
                std::filesystem::remove(glb);
            const auto r = Monstey_ModelExportGlb(mesh, glb, skel);
            Monstey_Require(r["all_eight_vertex_channels"].as_bool() && r["skel_matching_verified"].as_bool(),
                            "GLB data contract missing");
            const auto g = Monstey_ReadGlbTest(glb);
            const auto &a = g.doc["meshes"][0u]["primitives"][0u]["attributes"];
            Monstey_Require(a.contains("_SOW_BITANGENT") && a.contains("JOINTS_0") && a.contains("WEIGHTS_0"),
                            "GLB channel missing");
        });
        test("native_glb_native_pristine_byte_exact", [&] {
            const auto out = dir / L"pristine.mesh";
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            const auto r = Monstey_ModelConvert(glb, mesh, out, Monstey_Json::object({{"binding", "native"}}));
            Monstey_Require(r["byte_exact_noop"].as_bool() && Monstey_ReadFile(out) == fixture,
                            "Native GLB roundtrip is not byte exact");
        });
        test("edited_glb_changes_geometry_not_embedded_baseline", [&] {
            auto g = Monstey_ReadGlbTest(glb);
            Monstey_PutU32(g.bin, Monstey_AccessorOffset(g, "POSITION"), std::bit_cast<std::uint32_t>(0.25f));
            const auto edited = dir / L"edited.glb", out = dir / L"edited.mesh", view = dir / L"edited-view.glb";
            Monstey_WriteTest(edited, Monstey_SaveGlbTest(g));
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            const auto r = Monstey_ModelConvert(edited, mesh, out, Monstey_Json::object({{"binding", "native"}}));
            Monstey_Require(!r["byte_exact_noop"].as_bool(), "Edited data ignored");
            if (std::filesystem::exists(view))
                std::filesystem::remove(view);
            Monstey_ModelExportGlb(out, view);
            const auto check = Monstey_ReadGlbTest(view);
            Monstey_Require(std::bit_cast<float>(Monstey_U32(check.bin, Monstey_AccessorOffset(check, "POSITION"))) ==
                                0.25f,
                            "Edited native vertex not applied");
            Monstey_Require(r["allocation"]["safe"].as_bool(), "Output allocation unsafe");
        });
        test("glb_stale_band_metadata_requires_explicit_early_uniform_choice", [&] {
            auto g = Monstey_ReadGlbTest(glb);
            auto &primitive = g.doc["meshes"][0u]["primitives"][0u];
            const auto &accessor = g.doc["accessors"][static_cast<std::size_t>(primitive["indices"].as_uint())];
            const auto &view = g.doc["bufferViews"][static_cast<std::size_t>(accessor["bufferView"].as_uint())];
            const auto at = static_cast<std::size_t>(view.value("byteOffset", 0u).as_uint() + accessor.value("byteOffset", 0u).as_uint());
            Monstey_PutU16(g.bin, at + 2, 2);
            Monstey_PutU16(g.bin, at + 4, 1);
            const auto source = dir / L"stale-bands.glb", out = dir / L"explicit-glb-bands.mesh";
            Monstey_WriteTest(source, Monstey_SaveGlbTest(g));
            const auto rejected = dir / L"must-not-write-stale.mesh";
            if (std::filesystem::exists(rejected))
                std::filesystem::remove(rejected);
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            reject([&] { (void)Monstey_ModelConvert(source, mesh, rejected,
                                                   Monstey_Json::object({{"binding", "native"}})); });
            Monstey_Require(!std::filesystem::exists(rejected), "Stale GLB metadata published an output");
            const auto r = Monstey_ModelConvert(source, mesh, out,
                                                 Monstey_Json::object({{"binding", "native"}, {"triangle_band", 7u}}));
            const auto read = Monstey_ModelReadScene(out);
            for (const auto &part : read.parts)
                Monstey_Require(std::all_of(part.triangle_bands.begin(), part.triangle_bands.end(),
                                            [](const auto band) { return band == 7; }),
                                "Early GLB uniform choice did not assign native face membership");
            Monstey_Require(r["triangle_band_policy"].as_string() == "explicit_uniform" && r["triangle_band"].as_uint() == 7,
                            "GLB uniform choice lost policy diagnostics");
            primitive["extras"]["monstey_triangle_bands"] = Monstey_Json::array({17u});
            const auto malformed = dir / L"malformed-bands.glb";
            Monstey_WriteTest(malformed, Monstey_SaveGlbTest(g));
            const auto invalidOutput = dir / L"must-not-write-bad-band.mesh";
            if (std::filesystem::exists(invalidOutput))
                std::filesystem::remove(invalidOutput);
            reject([&] { (void)Monstey_ModelConvert(malformed, mesh, invalidOutput,
                                                   Monstey_Json::object({{"binding", "native"}, {"triangle_band", 7u}})); });
            Monstey_Require(!std::filesystem::exists(invalidOutput), "Malformed GLB bands published an output");
        });
        test("native_template_fingerprint_required", [&] {
            auto g = Monstey_ReadGlbTest(glb);
            g.doc["extras"]["monstey_native"]["template_sha256"] = "wrong";
            const auto bad = dir / L"wrong-template.glb";
            Monstey_WriteTest(bad, Monstey_SaveGlbTest(g));
            reject([&] {
                Monstey_ModelConvert(bad, mesh, dir / L"must-not-write.mesh",
                                     Monstey_Json::object({{"binding", "native"}}));
            });
        });
        test("native_bind_edit_rejected", [&] {
            auto g = Monstey_ReadGlbTest(glb);
            g.doc["nodes"][1u]["matrix"][12u] = 1.0;
            const auto bad = dir / L"bad-bind.glb";
            Monstey_WriteTest(bad, Monstey_SaveGlbTest(g));
            reject([&] {
                Monstey_ModelConvert(bad, mesh, dir / L"bad-bind.mesh", Monstey_Json::object({{"binding", "native"}}));
            });
        });
        test("custom_rigid_conversion_preserves_native_connections", [&] {
            const auto custom = dir / L"custom.glb", out = dir / L"custom.mesh";
            Monstey_WriteTest(custom, Monstey_CustomGlb(1));
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            const auto r = Monstey_ModelConvert(
                custom, mesh, out, Monstey_Json::object({{"container", 1u}, {"binding", "rigid"}, {"rigid_bone", 3u}}));
            Monstey_Require(r["allocation"]["connection_map"][2u].as_int() == -1 &&
                                r["native_material_groups_bones_lod_bindings_preserved"].as_bool(),
                            "Disabled connection changed");
        });
        test("authored_triangle_count_splits_native_uint16_draws", [&] {
            const auto custom = dir / L"many-triangles.glb", out = dir / L"many-triangles.mesh";
            Monstey_WriteTest(custom, Monstey_CustomGlb(65536));
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            const auto r = Monstey_ModelConvert(custom, mesh, out,
                                                Monstey_Json::object({{"container", 0u}, {"binding", "rigid"}}));
            Monstey_Require(r["allocation"]["required_max_draws"].as_uint() == 2 &&
                                r["allocation"]["declared_max_draws"].as_uint() >= 2,
                            "Native draw allocations not rebuilt");
            const auto inspect = Monstey_ModelInspect(out);
            std::uint64_t triangles = 0;
            for (const auto &p : inspect["parts"].as_array())
                if (p["container"].as_uint() == 0)
                    triangles += p["triangles"].as_uint();
            Monstey_Require(triangles == 65536, "Draw split lost triangles");
        });
        test("mismatching_skel_rejected", [&] {
            auto b = Monstey_SkelFixture();
            Monstey_PutU32(b, 100 + 20 + 5, std::bit_cast<std::uint32_t>(1.0f));
            const auto bad = dir / L"mismatch.skel";
            Monstey_WriteTest(bad, b);
            reject([&] { (void)Monstey_ModelInspect(mesh, bad); });
        });
        test("unsupported_layout_precise_rejection", [&] {
            auto b = fixture;
            const auto info = 20 + Monstey_U32(b, 8);
            const auto layout = info + 72 + 48 + 96 + 80 + 4 + 4 + 4;
            b[layout + 1] = 99;
            const auto bad = dir / L"unknown-layout.mesh";
            Monstey_WriteTest(bad, b);
            reject([&] { (void)Monstey_ModelInspect(bad); });
        });
        test("existing_destination_rejected_and_preserved", [&] {
            const auto hash = Monstey_HashSha256(mesh);
            reject([&] { Monstey_ModelConvert(glb, mesh, mesh, Monstey_Json::object({{"binding", "native"}})); });
            Monstey_Require(Monstey_HashSha256(mesh) == hash, "Template modified");
        });
        test("mapped_custom_binding_rewrites_all_joint_indices", [&] {
            const auto out = dir / L"mapped.mesh", view = dir / L"mapped.glb";
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            const auto mapping = Monstey_Json::object({{"0", 2u}, {"1", 3u}, {"2", 0u}, {"3", 1u}});
            const auto r = Monstey_ModelConvert(
                glb, mesh, out,
                Monstey_Json::object({{"binding", "mapped"}, {"container", 1u}, {"joint_map", mapping}}));
            Monstey_Require(r["allocation"]["required_max_draws"].as_uint() == 2, "Mapped primitives lost");
            if (std::filesystem::exists(view))
                std::filesystem::remove(view);
            Monstey_ModelExportGlb(out, view);
            const auto check = Monstey_ReadGlbTest(view);
            const auto at = Monstey_AccessorOffset(check, "JOINTS_0", 1);
            Monstey_Require(Monstey_U16(check.bin, at) == 0 && Monstey_U16(check.bin, at + 2) == 1 &&
                                Monstey_U16(check.bin, at + 4) == 2,
                            "Mapped HK joints were not coalesced deterministically");
            const auto wa = Monstey_AccessorOffset(check, "WEIGHTS_0", 1);
            Monstey_Require(check.bin[wa] == 128 && check.bin[wa + 1] == 64 && check.bin[wa + 2] == 63,
                            "Mapped influence weights changed");
        });
        test("incomplete_joint_mapping_rejected", [&] {
            reject([&] {
                Monstey_ModelConvert(
                    glb, mesh, dir / L"incomplete-map.mesh",
                    Monstey_Json::object({{"binding", "mapped"}, {"joint_map", Monstey_Json::object()}}));
            });
        });
        test("mirrored_source_winding_is_corrected", [&] {
            auto g = Monstey_ReadGlbTest(dir / L"custom.glb");
            g.doc["nodes"][0u]["scale"] = Monstey_Json::array({-1.0, 1.0, 1.0});
            const auto source = dir / L"mirrored.glb", out = dir / L"mirrored.mesh", view = dir / L"mirrored-view.glb";
            Monstey_WriteTest(source, Monstey_SaveGlbTest(g));
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            Monstey_ModelConvert(source, mesh, out);
            if (std::filesystem::exists(view))
                std::filesystem::remove(view);
            Monstey_ModelExportGlb(out, view);
            const auto check = Monstey_ReadGlbTest(view);
            const auto ai = check.doc["meshes"][0u]["primitives"][0u]["indices"].as_uint(),
                       vi = check.doc["accessors"][static_cast<std::size_t>(ai)]["bufferView"].as_uint();
            const auto at = static_cast<std::size_t>(
                check.doc["bufferViews"][static_cast<std::size_t>(vi)]["byteOffset"].as_uint());
            Monstey_Require(Monstey_U16(check.bin, at) == 0 && Monstey_U16(check.bin, at + 2) == 2 &&
                                Monstey_U16(check.bin, at + 4) == 1,
                            "Mirrored triangle winding was not reversed");
        });
        test("original_nan_payloads_preserved_through_finite_glb", [&] {
            auto bytes = fixture;
            const auto data = 20 + Monstey_U32(bytes, 8) + Monstey_U32(bytes, 12) + 4;
            const auto stream1 = data + 3 * 16;
            Monstey_PutU16(bytes, stream1 + 4 + 8, 0xfe01);
            Monstey_PutU16(bytes, stream1 + 32 + 4 + 8, 0xfe02);
            const auto native = dir / L"nan.mesh", view = dir / L"nan.glb", out = dir / L"nan-roundtrip.mesh";
            Monstey_WriteTest(native, bytes);
            if (std::filesystem::exists(view))
                std::filesystem::remove(view);
            Monstey_ModelExportGlb(native, view);
            auto g = Monstey_ReadGlbTest(view);
            const auto &attrs = g.doc["meshes"][0u]["primitives"][0u]["attributes"];
            for (const char *key : {"_SOW_TANGENT_BITS_LO", "_SOW_TANGENT_BITS_HI"})
            {
                const auto &accessor = g.doc["accessors"][static_cast<std::size_t>(attrs[key].as_uint())];
                Monstey_Require(accessor["componentType"].as_uint() == 5123 && accessor["type"].as_string() == "VEC4",
                                "Raw bits are not glTF-compatible aligned U16 attributes");
            }
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            const auto r = Monstey_ModelConvert(view, native, out, Monstey_Json::object({{"binding", "native"}}));
            Monstey_Require(r["byte_exact_noop"].as_bool() && Monstey_ReadFile(out) == bytes,
                            "NaN payload bits changed in native GLB roundtrip");
        });
        test("standard_direction_edit_wins_per_vertex", [&] {
            auto g = Monstey_ReadGlbTest(dir / L"nan.glb");
            const auto at = Monstey_AccessorOffset(g, "TANGENT");
            Monstey_PutU32(g.bin, at, std::bit_cast<std::uint32_t>(0.0f));
            Monstey_PutU32(g.bin, at + 4, std::bit_cast<std::uint32_t>(1.0f));
            const auto source = dir / L"nan-edited.glb", out = dir / L"nan-edited.mesh",
                       view = dir / L"nan-edited-view.glb";
            Monstey_WriteTest(source, Monstey_SaveGlbTest(g));
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            Monstey_ModelConvert(source, dir / L"nan.mesh", out, Monstey_Json::object({{"binding", "native"}}));
            if (std::filesystem::exists(view))
                std::filesystem::remove(view);
            Monstey_ModelExportGlb(out, view);
            const auto check = Monstey_ReadGlbTest(view);
            const auto tangent = Monstey_AccessorOffset(check, "TANGENT");
            Monstey_Require(Monstey_U32(check.bin, tangent) == 0 &&
                                Monstey_U32(check.bin, tangent + 4) == std::bit_cast<std::uint32_t>(1.0f),
                            "Standard tangent edit overwritten by raw metadata");
            const auto hi = Monstey_AccessorOffset(check, "_SOW_TANGENT_BITS_HI");
            Monstey_Require((Monstey_U16(check.bin, hi + 8) & 0x7f80) == 0x7f80,
                            "Other vertex NaN was lost during a local tangent edit");
        });
        test("verified_template_restores_channels_after_metadata_drop", [&] {
            auto g = Monstey_ReadGlbTest(dir / L"nan.glb");
            g.doc.as_object().erase("extras");
            for (auto &node : g.doc["nodes"].as_array())
                node.as_object().erase("extras");
            for (auto &meshItem : g.doc["meshes"].as_array())
                for (auto &primitive : meshItem["primitives"].as_array())
                {
                    primitive.as_object().erase("extras");
                    auto &attrs = primitive["attributes"].as_object();
                    for (auto it = attrs.begin(); it != attrs.end();)
                    {
                        if (it->first.starts_with("_SOW_"))
                            it = attrs.erase(it);
                        else
                            ++it;
                    }
                }
            const auto source = dir / L"metadata-dropped.glb", out = dir / L"metadata-restored.mesh";
            Monstey_WriteTest(source, Monstey_SaveGlbTest(g));
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            const auto partMap = Monstey_Json::array({Monstey_Json::object({{"container", 0u}, {"part", 0u}}),
                                                      Monstey_Json::object({{"container", 1u}, {"part", 0u}})});
            const auto r =
                Monstey_ModelConvert(source, dir / L"nan.mesh", out,
                                     Monstey_Json::object({{"binding", "native"},
                                                           {"template_sha256", Monstey_HashSha256(dir / L"nan.mesh")},
                                                           {"native_part_map", partMap}}));
            Monstey_Require(r["byte_exact_noop"].as_bool() &&
                                Monstey_ReadFile(out) == Monstey_ReadFile(dir / L"nan.mesh"),
                            "Verified-template fallback did not preserve dropped native channels");
        });
        test("all_eight_authored_vertex_channels_are_applied", [&] {
            auto g = Monstey_ReadGlbTest(glb);
            const auto editFloat = [&](const char *name, std::initializer_list<float> values) {
                const auto at = Monstey_AccessorOffset(g, name);
                std::size_t k = 0;
                for (const auto v : values)
                    Monstey_PutU32(g.bin, at + k++ * 4, std::bit_cast<std::uint32_t>(v));
            };
            editFloat("POSITION", {0.25f, 0, 0});
            editFloat("NORMAL", {1, 0, 0});
            editFloat("TANGENT", {0, 1, 0, 1});
            editFloat("_SOW_BITANGENT", {0, 0, 1});
            editFloat("TEXCOORD_0", {0.125f, 0.5f});
            g.bin[Monstey_AccessorOffset(g, "COLOR_0")] = 42;
            Monstey_PutU16(g.bin, Monstey_AccessorOffset(g, "JOINTS_0"), 1);
            const auto wa = Monstey_AccessorOffset(g, "WEIGHTS_0");
            g.bin[wa] = 128;
            g.bin[wa + 1] = 64;
            const auto source = dir / L"eight-channels.glb", out = dir / L"eight-channels.mesh",
                       view = dir / L"eight-channels-view.glb";
            Monstey_WriteTest(source, Monstey_SaveGlbTest(g));
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            Monstey_ModelConvert(source, mesh, out, Monstey_Json::object({{"binding", "native"}}));
            if (std::filesystem::exists(view))
                std::filesystem::remove(view);
            Monstey_ModelExportGlb(out, view);
            const auto check = Monstey_ReadGlbTest(view);
            const auto f = [&](const char *name, unsigned k) {
                return std::bit_cast<float>(Monstey_U32(check.bin, Monstey_AccessorOffset(check, name) + k * 4));
            };
            Monstey_Require(f("POSITION", 0) == 0.25f && f("NORMAL", 0) == 1 && f("TANGENT", 1) == 1 &&
                                f("_SOW_BITANGENT", 2) == 1 && std::abs(f("TEXCOORD_0", 0) - 0.125f) < 1.0f / 32767,
                            "Authored float channels lost");
            Monstey_Require(check.bin[Monstey_AccessorOffset(check, "COLOR_0")] == 42 &&
                                Monstey_U16(check.bin, Monstey_AccessorOffset(check, "JOINTS_0")) == 1 &&
                                check.bin[Monstey_AccessorOffset(check, "WEIGHTS_0")] == 128,
                            "Authored colour/joints/weights lost");
        });
        test("rotated_native_bind_is_global_not_parent_local", [&] {
            auto bytes = fixture;
            const auto info = 20 + Monstey_U32(bytes, 8), end = info + Monstey_U32(bytes, 12);
            const auto bones = end - 4 * 40;
            const auto bone = bones + 2 * 40;
            Monstey_PutU32(bytes, bone + 20 + 8, std::bit_cast<std::uint32_t>(0.7071067811865475f));
            Monstey_PutU32(bytes, bone + 20 + 12, std::bit_cast<std::uint32_t>(0.7071067811865475f));
            const auto native = dir / L"rotated.mesh", view = dir / L"rotated.glb";
            Monstey_WriteTest(native, bytes);
            if (std::filesystem::exists(view))
                std::filesystem::remove(view);
            Monstey_ModelExportGlb(native, view);
            const auto g = Monstey_ReadGlbTest(view);
            const auto &matrix = g.doc["nodes"][4u]["matrix"];
            Monstey_Require(std::abs(matrix[0u].as_number()) < 1e-6 && std::abs(matrix[1u].as_number() - 1) < 1e-6 &&
                                std::abs(matrix[4u].as_number() + 1) < 1e-6 &&
                                std::abs(matrix[12u].as_number() - 0.1) < 1e-6 &&
                                std::abs(matrix[13u].as_number() - 0.5) < 1e-6,
                            "Bind rotation/local translation reconstructed incorrectly");
            const auto ai = g.doc["skins"][0u]["inverseBindMatrices"].as_uint(),
                       vi = g.doc["accessors"][static_cast<std::size_t>(ai)]["bufferView"].as_uint();
            const auto at =
                static_cast<std::size_t>(g.doc["bufferViews"][static_cast<std::size_t>(vi)]["byteOffset"].as_uint()) +
                3 * 64;
            Monstey_Require(std::abs(std::bit_cast<float>(Monstey_U32(g.bin, at + 12 * 4)) + 1) < 1e-6 &&
                                std::abs(std::bit_cast<float>(Monstey_U32(g.bin, at + 13 * 4)) - 0.1) < 1e-6,
                            "Inverse bind does not invert global metres transform");
        });
        test("unmodified_native_container_buffers_remain_exact", [&] {
            const auto edited = Monstey_ReadFile(dir / L"edited.mesh");
            for (unsigned stream = 0; stream < 3; ++stream)
                Monstey_Require(Monstey_NativeBuffer(edited, stream, 1) == Monstey_NativeBuffer(fixture, stream, 1),
                                "Local native edit rewrote another container");
        });
        test("unchanged_draw_inside_modified_container_keeps_unknown_bits", [&] {
            const auto stage = dir / L"two-parts.mesh", view = dir / L"two-parts.glb",
                       edited = dir / L"two-parts-edited.glb", out = dir / L"two-parts-result.mesh";
            if (std::filesystem::exists(stage))
                std::filesystem::remove(stage);
            Monstey_ModelConvert(glb, mesh, stage, Monstey_Json::object({{"binding", "rigid"}, {"container", 0u}}));
            auto bytes = Monstey_ReadFile(stage);
            const auto secondVertex = Monstey_U32(bytes, Monstey_NativeDrawTable(bytes) + 40);
            const auto data = 20ull + Monstey_U32(bytes, 8) + Monstey_U32(bytes, 12) + 4;
            Monstey_PutU16(bytes, data + secondVertex + 6, 0x4500);
            Monstey_WriteTest(stage, bytes);
            if (std::filesystem::exists(view))
                std::filesystem::remove(view);
            Monstey_ModelExportGlb(stage, view);
            auto g = Monstey_ReadGlbTest(view);
            Monstey_PutU32(g.bin, Monstey_AccessorOffset(g, "POSITION"), std::bit_cast<std::uint32_t>(0.25f));
            Monstey_WriteTest(edited, Monstey_SaveGlbTest(g));
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            Monstey_ModelConvert(edited, stage, out, Monstey_Json::object({{"binding", "native"}}));
            const auto result = Monstey_ReadFile(out), vertices = Monstey_NativeBuffer(result, 0, 0);
            const auto newOffset = Monstey_U32(result, Monstey_NativeDrawTable(result) + 40);
            Monstey_Require(Monstey_U16(vertices, newOffset + 6) == 0x4500,
                            "Untouched draw's position W/raw layout was rewritten");
        });
        test("fixed_topology_native_edits_preserve_unknown_components_and_lod_bytes", [&] {
            auto bytes = fixture;
            const auto data = 20ull + Monstey_U32(bytes, 8) + Monstey_U32(bytes, 12) + 4,
                       info = 20ull + Monstey_U32(bytes, 8), lod = info + 72 + 48;
            for (unsigned v = 0; v < 3; ++v)
            {
                Monstey_PutU16(bytes, data + v * 16 + 6, 0x3555);
                Monstey_PutU16(bytes, data + 48 + v * 32 + 10, 0x4567);
                Monstey_PutU16(bytes, data + 48 + v * 32 + 18, 0x3210);
                Monstey_PutU16(bytes, data + 48 + v * 32 + 26, 0x5678);
            }
            Monstey_PutU16(bytes, lod + 16, 9);
            const auto donor = dir / L"fixed-opaque.mesh", out = dir / L"fixed-opaque-edited.mesh";
            Monstey_WriteTest(donor, bytes);
            auto scene = Monstey_ModelReadScene(donor);
            scene.parts[0].channels.at("POSITION").values[0] = 1;
            scene.parts[0].channels.at("NORMAL").values[0] = .5;
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            Monstey_ModelWriteScene(scene, donor, out);
            auto expected = bytes;
            Monstey_PutU16(expected, data, 0x3c00);
            Monstey_PutU16(expected, data + 48 + 4, 0x3800);
            Monstey_Require(Monstey_ReadFile(out) == expected,
                            "Fixed topology edit changed unknown bytes/layout/LOD/palette");
        });
        test("new_joint_palette_entries_keep_native_draw_storage_and_unknown_bits", [&] {
            const auto donor = dir / L"fixed-opaque.mesh", out = dir / L"fixed-opaque-palette.mesh";
            auto scene = Monstey_ModelReadScene(donor);
            scene.parts[0].channels.at("JOINTS").values[0] = 1;
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            Monstey_ModelWriteScene(scene, donor, out);
            const auto before = Monstey_ReadFile(donor), after = Monstey_ReadFile(out);
            auto expected = Monstey_NativeBuffer(before, 0, 0);
            expected[12] = 3;
            Monstey_Require(Monstey_NativeBuffer(after, 0, 0) == expected &&
                                Monstey_NativeBuffer(after, 2, 0) == Monstey_Bytes({0, 2, 3, 1}),
                            "Palette extension changed native storage or unknown components");
            const auto oldInfo = 20 + Monstey_U32(before, 8), newInfo = 20 + Monstey_U32(after, 8);
            Monstey_Require(Monstey_U16(after, newInfo + 72 + 48 + 16) == Monstey_U16(before, oldInfo + 72 + 48 + 16),
                            "Palette-only change rewrote native LOD quotas");
            Monstey_Require(Monstey_ModelReadScene(out).parts[0].channels.at("JOINTS").values[0] == 1,
                            "New HK palette joint did not apply");
        });
        test("preflight_performs_full_conversion_without_any_publish", [&] {
            const auto output = dir / L"preflight-uncreated-directory" / L"candidate.mesh";
            Monstey_Require(!std::filesystem::exists(output.parent_path()), "Preflight directory unexpectedly exists");
            const auto options = Monstey_Json::object({{"binding", "rigid"},
                                                       {"preflight", true},
                                                       {"expected_source_sha256", Monstey_HashSha256(glb)},
                                                       {"expected_template_sha256", Monstey_HashSha256(mesh)}});
            const auto result = Monstey_ModelConvert(glb, mesh, output, options);
            Monstey_Require(result["preflight"].as_bool() && !result["published"].as_bool() &&
                                result["validated_bytes"].as_uint() > 0,
                            "Preflight did not validate output bytes");
            Monstey_Require(!std::filesystem::exists(output) && !std::filesystem::exists(output.parent_path()) &&
                                !std::filesystem::exists(std::filesystem::path(output.wstring() + L".report.json")),
                            "Preflight wrote an artifact or directory");
            const auto actual = dir / L"preflight-actual.mesh";
            if (std::filesystem::exists(actual))
                std::filesystem::remove(actual);
            const auto written = Monstey_ModelConvert(glb, mesh, actual, Monstey_Json::object({{"binding", "rigid"}}));
            Monstey_Require(written["output_sha256"].as_string() == result["output_sha256"].as_string(),
                            "Preflight and actual conversion differ");
        });
        test("preflight_checks_native_allocation_failures", [&] {
            auto bytes = fixture;
            Monstey_PutU32(bytes, 20 + Monstey_U32(bytes, 8) + 20, 0);
            const auto bad = dir / L"unsafe-counter.mesh";
            Monstey_WriteTest(bad, bytes);
            const auto out = dir / L"unsafe-preflight.mesh";
            reject([&] { Monstey_ModelConvert(glb, bad, out, Monstey_Json::object({{"preflight", true}})); });
            Monstey_Require(!std::filesystem::exists(out), "Failed preflight published bytes");
        });
        test("custom_primitive_selection_uses_only_selected_draw", [&] {
            const auto out = dir / L"primitive-selected.mesh";
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            const auto result =
                Monstey_ModelConvert(glb, mesh, out, Monstey_Json::object({{"binding", "rigid"}, {"primitive", 1u}}));
            const auto parsed = Monstey_ModelInspect(out);
            Monstey_Require(parsed["parts"].size() == 2 && result["allocation"]["required_max_draws"].as_uint() == 1 &&
                                result["selected_primitive"].as_uint() == 1,
                            "Primitive selection imported multiple source draws");
        });
        test("invalid_or_native_partial_primitive_selection_rejected", [&] {
            reject([&] {
                Monstey_ModelConvert(glb, mesh, dir / L"invalid-primitive.mesh",
                                     Monstey_Json::object({{"primitive", 2u}}));
            });
            reject([&] {
                Monstey_ModelConvert(glb, mesh, dir / L"native-partial.mesh",
                                     Monstey_Json::object({{"binding", "native"}, {"primitive", 0u}}));
            });
        });
        test("expected_hashes_are_checked_inside_actual_snapshot_engine", [&] {
            for (const char *key : {"expected_source_sha256", "expected_template_sha256"})
            {
                bool mismatch = false;
                try
                {
                    Monstey_ModelConvert(glb, mesh, dir / L"hash-must-not-write.mesh",
                                         Monstey_Json::object({{key, std::string(64, '0')}, {"preflight", true}}));
                }
                catch (const Monstey_Error &error)
                {
                    mismatch = error.code == "source_hash_mismatch";
                }
                Monstey_Require(mismatch, "Snapshot SHA mismatch was not rejected by native engine");
            }
            auto changed = Monstey_ReadGlbTest(glb);
            Monstey_PutU32(changed.bin, Monstey_AccessorOffset(changed, "POSITION"),
                           std::bit_cast<std::uint32_t>(0.25f));
            const auto path = dir / L"source-after-hash.glb";
            Monstey_WriteTest(path, Monstey_SaveGlbTest(changed));
            bool mismatch = false;
            try
            {
                Monstey_ModelConvert(
                    path, mesh, dir / L"changed-source-preflight.mesh",
                    Monstey_Json::object({{"expected_source_sha256", Monstey_HashSha256(glb)}, {"preflight", true}}));
            }
            catch (const Monstey_Error &error)
            {
                mismatch = error.code == "source_hash_mismatch";
            }
            Monstey_Require(mismatch, "Changed source snapshot passed old expected SHA");
        });
        test("rigid_geometry_optional_skin_preserves_grammar_and_strict_transfer", [&] {
            const auto bytes = Monstey_MeshFixture(false);
            const auto scene = Monstey_ModelReadRigidSceneBytes(bytes);
            Monstey_Require(scene.bones.empty() && scene.hk_to_source.empty() && scene.parts.size() == 2 &&
                                scene.parts[0].channels.size() == 6 && !scene.parts[0].channels.contains("JOINTS") &&
                                scene.parts[0].channels.at("POSITION").values[3] == 100 &&
                                scene.native_baseline == bytes,
                            "Rigid geometry reader changed native positions/metadata or retained skin");
            reject([&] { Monstey_ModelReadSceneBytes(bytes); });
            const auto r = Monstey_ModelInspectBytes(bytes);
            Monstey_Require(r["bones"].as_uint() == 4 && r["submeshes"].as_uint() == 2 && r["layouts"].as_uint() == 1 &&
                                r["parts"][0u]["finite_positions"].as_bool() &&
                                r["parts"][0u]["bounds_cm"][1u][0u].as_number() == 100 &&
                                !r["parts"][0u].contains("weight_sum_minmax") &&
                                r["native_allocations_valid"].as_bool(),
                            "Unskinned native inspection lost old fields");
        });
        test("inspection_diagnoses_nonfinite_positions_without_repairing_geometry", [&] {
            auto bytes = Monstey_MeshFixture(false);
            const auto start = 20ull + Monstey_U32(bytes, 8) + Monstey_U32(bytes, 12) + 4;
            Monstey_PutU16(bytes, start, 0x7e12);
            const auto r = Monstey_ModelInspectBytes(bytes);
            Monstey_Require(!r["parts"][0u]["finite_positions"].as_bool() && r["parts"][0u]["bounds_cm"].is_null() &&
                                r["source_sha256"].as_string() == Monstey_HashSha256(bytes),
                            "Diagnostic inspection masked nonfinite positions");
            reject([&] { Monstey_ModelReadRigidSceneBytes(bytes); });
        });
        test("native_inspection_old_bounds_weights_and_glb_metadata_are_available", [&] {
            const auto r = Monstey_ModelInspectBytes(fixture);
            Monstey_Require(r["parts"][0u]["weight_sum_minmax"][0u].as_number() == 1 &&
                                r["parts"][0u]["weight_sum_minmax"][1u].as_number() == 1 &&
                                r["parts"][0u]["bounds_cm"][1u][1u].as_number() == 100,
                            "Old native bounds/normalized weight report changed");
            const auto view = dir / L"inspection.glb";
            if (std::filesystem::exists(view))
                std::filesystem::remove(view);
            Monstey_ModelExportGlb(mesh, view, skel);
            const auto g = Monstey_ModelInspectBytes(Monstey_ReadFile(view));
            Monstey_Require(g["meshes"].as_uint() == 2 && g["skins"].as_uint() == 1 && g["rig_present"].as_bool() &&
                                g["primitive_attributes"].size() == 2 &&
                                g["source_metadata"]["version"].as_string() == "2.0",
                            "Old GLB inspection metadata missing");
        });
        test("glb_metadata_only_without_bin_is_inspectable_but_not_convertible", [&] {
            Monstey_GlbTest data;
            data.doc = Monstey_Json::object({{"asset", Monstey_Json::object({{"version", "2.0"}})}});
            auto bytes = Monstey_SaveGlbTest(data);
            bytes.resize(bytes.size() - 8);
            Monstey_PutU32(bytes, 8, static_cast<std::uint32_t>(bytes.size()));
            const auto result = Monstey_ModelInspectBytes(bytes);
            Monstey_Require(result["format"].as_string() == "GLB2" && result["nodes"].as_uint() == 0 &&
                                result["meshes"].as_uint() == 0 && result["primitives"].as_uint() == 0 &&
                                !result["rig_present"].as_bool(),
                            "Metadata-only GLB inspection requires a convertible mesh");
            const auto path = dir / L"metadata-only.glb";
            Monstey_WriteTest(path, bytes);
            reject([&] {
                Monstey_ModelConvert(path, mesh, dir / L"must-not-convert-empty.mesh",
                                     Monstey_Json::object({{"preflight", true}}));
            });
            auto truncated = bytes;
            truncated.pop_back();
            reject([&] { Monstey_ModelInspectBytes(truncated); });
        });
        test("glb_nontriangle_and_unreferenced_metadata_preserves_import_restrictions", [&] {
            const auto path = dir / L"lines-metadata.glb";
            Monstey_WriteTest(path, Monstey_CustomGlb(1));
            auto data = Monstey_ReadGlbTest(path);
            data.doc["meshes"][0u]["primitives"][0u]["mode"] = 1u;
            data.doc["meshes"].as_array().push_back(data.doc["meshes"][0u]);
            data.doc["extensionsRequired"] = Monstey_Json::array({"TEST_unknown_metadata_extension"});
            const auto bytes = Monstey_SaveGlbTest(data);
            const auto result = Monstey_ModelInspectBytes(bytes);
            Monstey_Require(result["primitives"].as_uint() == 2 && result["nodes"].as_uint() == 1 &&
                                result["primitive_attributes"].size() == 2 &&
                                result["extensions_required"][0u].as_string() == "TEST_unknown_metadata_extension",
                            "Inspection omitted unsupported or unreferenced GLB metadata");
            Monstey_WriteTest(path, bytes);
            reject([&] {
                Monstey_ModelConvert(path, mesh, dir / L"must-not-convert-extension.mesh",
                                     Monstey_Json::object({{"preflight", true}}));
            });
            data.doc["extensionsRequired"] = Monstey_Json::array();
            Monstey_WriteTest(path, Monstey_SaveGlbTest(data));
            reject([&] {
                Monstey_ModelConvert(path, mesh, dir / L"must-not-convert-lines.mesh",
                                     Monstey_Json::object({{"preflight", true}}));
            });
        });
        test("shared_native_scene_roundtrip_preserves_all_data", [&] {
            const auto scene = Monstey_ModelReadScene(mesh, skel);
            Monstey_Require(scene.parts.size() == 2 && scene.bones[2].source_index == 3 &&
                                scene.native_baseline == fixture,
                            "Shared scene lost native channels/rig/baseline");
            const auto out = dir / L"scene-native.mesh";
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            const auto result = Monstey_ModelWriteScene(scene, mesh, out);
            Monstey_Require(result["byte_exact_noop"].as_bool() && Monstey_ReadFile(out) == fixture,
                            "Direct semantic scene no-op is not byte exact");
        });
        test("shared_scene_edits_apply_and_preserve_other_containers", [&] {
            auto scene = Monstey_ModelReadScene(mesh, skel);
            scene.parts[0].channels["POSITION"].values[0] = 25;
            scene.parts[0].channels["COLOR0"].values[0] = 42;
            const auto out = dir / L"scene-edited.mesh";
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            Monstey_ModelWriteScene(scene, mesh, out);
            const auto check = Monstey_ModelReadScene(out);
            Monstey_Require(check.parts[0].channels.at("POSITION").values[0] == 25 &&
                                check.parts[0].channels.at("COLOR0").values[0] == 42,
                            "Shared semantic edits lost");
            for (unsigned k = 0; k < 3; ++k)
                Monstey_Require(Monstey_NativeBuffer(Monstey_ReadFile(out), k, 1) ==
                                    Monstey_NativeBuffer(fixture, k, 1),
                                "Shared scene rewrote other container");
        });
        test("shared_scene_host_projection_and_invalid_edits", [&] {
            auto scene = Monstey_ModelReadScene(mesh);
            scene.position_projection = Monstey_ModelProjection::Float32Metres;
            for (auto &p : scene.parts)
                for (auto &v : p.channels["POSITION"].values)
                    v = static_cast<double>(static_cast<float>(v / 100)) * 100;
            const auto out = dir / L"scene-projected.mesh";
            if (std::filesystem::exists(out))
                std::filesystem::remove(out);
            Monstey_Require(Monstey_ModelWriteScene(scene, mesh, out)["byte_exact_noop"].as_bool(),
                            "Host projected scene changed native bytes");
            scene.parts[0].channels["WEIGHTS_U8"].values[0] = 1;
            reject([&] { Monstey_ModelWriteScene(scene, mesh, dir / L"bad-scene-weights.mesh"); });
            scene = Monstey_ModelReadScene(mesh);
            scene.bones[0].global_bind_metres[3] = 1;
            reject([&] { Monstey_ModelWriteScene(scene, mesh, dir / L"bad-scene-bind.mesh"); });
            scene = Monstey_ModelReadScene(mesh);
            scene.native_baseline.back() ^= 1;
            reject([&] { Monstey_ModelWriteScene(scene, mesh, dir / L"bad-scene-baseline.mesh"); });
        });
        if (argc > 2)
        {
            const std::filesystem::path native = argv[2], nativeSkel = argc > 3 ? argv[3] : L"";
            if (!nativeSkel.empty())
                test("real_standalone_skel_inspection_matches_source_counts_and_snapshot", [&] {
                    const auto bytes = Monstey_ReadFile(nativeSkel);
                    const auto r = Monstey_ModelInspectSkeleton(bytes);
                    Monstey_Require(r["format"].as_string() == "SKEL17" &&
                                        r["bones"].as_uint() == Monstey_U32(bytes, 40) &&
                                        r["bone_names"].size() == r["bones"].as_uint() &&
                                        r["finite_bindings"].as_bool() && r["valid_bind_matrices"].as_bool() &&
                                        r["source_sha256"].as_string() == Monstey_HashSha256(bytes),
                                    "Original standalone SKEL metadata differs");
                    report["native_skeleton_inspect"] = r;
                });
            if (!nativeSkel.empty())
                test("real_skel_piece_metadata_matches_independent_raw_rows_and_tail", [&] {
                    const auto bytes = Monstey_ReadFile(nativeSkel);
                    const auto r = Monstey_ModelInspectSkeleton(bytes);
                    const auto &m = r["piece_lod_metadata"];
                    std::size_t p = 96;
                    std::uint64_t total = 0;
                    Monstey_Require(m["tables_total"].as_uint() == Monstey_U32(bytes, 32) &&
                                        m["tables"].size() == Monstey_U32(bytes, 32),
                                    "Actual prefix table count differs");
                    for (std::size_t ti = 0; ti < m["tables"].size(); ++ti)
                    {
                        const auto &t = m["tables"][ti];
                        const auto count = Monstey_U32(bytes, p);
                        Monstey_Require(t["source_offset"].as_uint() == p && t["rows_total"].as_uint() == count &&
                                            t["rows"].size() == count,
                                        "Actual prefix table extent differs");
                        p += 4;
                        total += count;
                        for (std::size_t ri = 0; ri < count; ++ri)
                        {
                            const auto &row = t["rows"][ri];
                            const auto bits = Monstey_U32(bytes, p + 2), last = Monstey_U32(bytes, p + 10);
                            Monstey_Require(row["source_offset"].as_uint() == p && row["byte0"].as_uint() == bytes[p] &&
                                                row["byte1"].as_uint() == bytes[p + 1] &&
                                                row["word6"].as_uint() == Monstey_U32(bytes, p + 6) &&
                                                row["word10"].as_uint() == last &&
                                                row["native_sentinel_flag"].as_bool() == (last != 0xffffffffu),
                                            "Actual serialized row/derived sentinel differs");
                            if ((bits & 0x7f800000u) != 0x7f800000u)
                                Monstey_Require(row["float2"].as_number() ==
                                                    static_cast<double>(std::bit_cast<float>(bits)),
                                                "Actual Float32 prefix value differs");
                            p += 14;
                        }
                    }
                    Monstey_Require(
                        m["rows_total"].as_uint() == total && m["source_bytes"].as_uint() == p - 96 &&
                            m["sha256"].as_string() ==
                                Monstey_HashSha256(std::span<const std::uint8_t>(bytes).subspan(96, p - 96)),
                        "Actual complete prefix snapshot differs");
                    const auto boneEnd = p + 4 + Monstey_U32(bytes, 12) + 37ull * Monstey_U32(bytes, 40);
                    Monstey_Require(r["trailing_offset"].as_uint() == boneEnd &&
                                        r["trailing_bytes"].as_uint() == bytes.size() - boneEnd &&
                                        r["trailing_sha256"].as_string() ==
                                            Monstey_HashSha256(std::span<const std::uint8_t>(bytes).subspan(
                                                static_cast<std::size_t>(boneEnd))),
                                    "Actual opaque tail offset/hash differs");
                    report["native_piece_metadata"] = m;
                });
            if (!nativeSkel.empty())
                test("real_skel_cloth_resources_and_references_match_native_source", [&] {
                    const auto bytes = Monstey_ReadFile(nativeSkel);
                    const auto before = Monstey_HashSha256(bytes);
                    const auto r = Monstey_ModelInspectSkeleton(bytes);
                    const auto &resources = r["cloth_resources"];
                    Monstey_Require(resources["section_locator_qualified"].as_bool() &&
                                        resources["resource_names"].size() == resources["resource_count"].as_uint(),
                                    "Actual native resource container was not qualified");
                    const auto marker = static_cast<std::size_t>(resources["section4_marker_offset"].as_uint());
                    Monstey_Require(Monstey_U32(bytes, marker) == 0xdcdcff04 &&
                                        Monstey_U32(bytes, marker + 4) == resources["resource_count"].as_uint(),
                                    "Actual section4 marker/count differ from bytes");
                    for (const auto &table : r["piece_lod_metadata"]["tables"].as_array())
                        for (const auto &row : table["rows"].as_array())
                        {
                            const auto index = row["word10"].as_uint();
                            const auto localCount = Monstey_U32(bytes, marker + 4);
                            const bool locallyResolved = index == 0xffffffffu || index < localCount;
                            Monstey_Require(row["cloth_resource_index"].as_uint() == index &&
                                                row["cloth_reference_valid"].as_bool() == locallyResolved,
                                            "Actual cloth index or file-local resolution diagnostic differs");
                            if (index == 0xffffffffu || !locallyResolved)
                                Monstey_Require(row["cloth_resource_path"].is_null(),
                                                "Null or unresolved local row acquired a resource");
                            else
                                Monstey_Require(row["cloth_resource_path"] ==
                                                    resources["resource_names"][static_cast<std::size_t>(index)],
                                                "Actual row did not resolve its section4 resource name");
                        }
                    Monstey_Require(Monstey_HashSha256(nativeSkel) == before,
                                    "Cloth-resource inspection changed the native source");
                    report["native_cloth_resources"] = resources;
                });
            test("real_rigid_geometry_view_preserves_available_native_channels_and_snapshot", [&] {
                const auto bytes = Monstey_ReadFile(native);
                const auto rigid = Monstey_ModelReadRigidSceneBytes(bytes), full = Monstey_ModelReadSceneBytes(bytes);
                Monstey_Require(rigid.source_sha256 == Monstey_HashSha256(bytes) &&
                                    rigid.parts.size() == full.parts.size() && rigid.bones.empty(),
                                "Actual rigid geometry snapshot/counts differ");
                for (std::size_t i = 0; i < rigid.parts.size(); ++i)
                {
                    Monstey_Require(!rigid.parts[i].channels.contains("JOINTS") &&
                                        !rigid.parts[i].channels.contains("WEIGHTS_U8") &&
                                        rigid.parts[i].channels.at("POSITION").values ==
                                            full.parts[i].channels.at("POSITION").values &&
                                        rigid.parts[i].indices == full.parts[i].indices,
                                    "Actual rigid view changed geometry/indices");
                }
                const auto r = Monstey_ModelInspectBytes(bytes);
                Monstey_Require(r["submeshes"].as_uint() == rigid.parts.size() &&
                                    r["parts"][0u]["finite_positions"].as_bool() &&
                                    r["parts"][0u].contains("bounds_cm"),
                                "Actual native bounds report unavailable");
            });
            test("real_native_metadata_matches_original_draw_table_words", [&] {
                const auto bytes = Monstey_ReadFile(native);
                const auto meta = Monstey_ModelNativePartMetadata(bytes);
                const auto table = Monstey_NativeDrawTable(bytes);
                std::size_t ordinal = 0;
                for (const auto &part : meta["parts"].as_array())
                {
                    Monstey_Require(
                        part["draw_parameter_words"][0u].as_uint() == Monstey_U32(bytes, table + ordinal * 40 + 28) &&
                            part["draw_parameter_words"][1u].as_uint() == Monstey_U32(bytes, table + ordinal * 40 + 32),
                        "Actual native draw metadata differs from source bytes");
                    ++ordinal;
                }
                Monstey_Require(ordinal == Monstey_ModelReadSceneBytes(bytes).parts.size() &&
                                    meta["source_sha256"].as_string() == Monstey_HashSha256(bytes),
                                "Actual metadata not qualified to the scene snapshot");
                report["native_part_metadata"] = meta;
            });
            test("real_native_shared_scene_roundtrip_byte_exact", [&] {
                const auto scene = Monstey_ModelReadScene(native, nativeSkel);
                const auto out = dir / L"real-scene-native.mesh";
                if (std::filesystem::exists(out))
                    std::filesystem::remove(out);
                report["native_scene"] = Monstey_ModelWriteScene(scene, native, out);
                Monstey_Require(report["native_scene"]["byte_exact_noop"].as_bool() &&
                                    Monstey_HashSha256(out) == Monstey_HashSha256(native),
                                "Real shared scene no-op changed native bits");
            });
            test("real_native_inspect_all_channels_and_bind_pair", [&] {
                report["native_inspect"] = Monstey_ModelInspect(native, nativeSkel);
                Monstey_Require(report["native_inspect"]["allocation"]["safe"].as_bool(),
                                "Original native allocation unsafe");
            });
            test("real_native_roundtrip_byte_exact", [&] {
                const auto out = dir / L"real-roundtrip.mesh";
                if (std::filesystem::exists(out))
                    std::filesystem::remove(out);
                Monstey_ModelRoundtrip(native, out);
                Monstey_Require(Monstey_HashSha256(out) == Monstey_HashSha256(native), "Real native bytes changed");
            });
            test("real_native_glb_native_byte_exact", [&] {
                const auto view = dir / L"real-export.glb", out = dir / L"real-glb-roundtrip.mesh";
                if (std::filesystem::exists(view))
                    std::filesystem::remove(view);
                if (std::filesystem::exists(out))
                    std::filesystem::remove(out);
                report["native_export"] = Monstey_ModelExportGlb(native, view, nativeSkel);
                report["native_convert"] =
                    Monstey_ModelConvert(view, native, out, Monstey_Json::object({{"binding", "native"}}));
                Monstey_Require(report["native_convert"]["byte_exact_noop"].as_bool() &&
                                    Monstey_HashSha256(out) == Monstey_HashSha256(native),
                                "Real native GLB roundtrip not byte exact");
            });
        }
        report["status"] = "PASS";
        report["passed"] = passed;
        const auto out = dir / L"model-tests.json";
        if (std::filesystem::exists(out))
            std::filesystem::remove(out);
        Monstey_WriteNewText(out, report.dump(2) + "\n");
        std::cout << "Native model tests: " << passed << " passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Native model failure: " << e.what() << "\n";
        return 1;
    }
}
