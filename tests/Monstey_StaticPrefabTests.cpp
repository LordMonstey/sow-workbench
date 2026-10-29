#include "Monstey_StaticPrefab.hpp"
#include "Monstey_InstGeometry.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace
{
using J = Monstey_Json;
using B = Monstey_Bytes;
int passed = 0;
void Check(bool ok, std::string_view s)
{
    Monstey_Require(ok, s);
    ++passed;
    std::cout << "PASS " << s << '\n';
}
void Reject(const std::function<void()> &fn, std::string_view s)
{
    try
    {
        fn();
    }
    catch (const std::exception &)
    {
        Check(true, s);
        return;
    }
    throw std::runtime_error("Expected rejection: " + std::string(s));
}
void Monstey_AddFloat(B &b, float value)
{
    Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(value));
}
Monstey_Bytes Monstey_StaticMeshFixture()
{
    Monstey_Bytes vertices;
    const std::uint16_t positions[3][4] = {{0, 0, 0, 0x3c00}, {0x5640, 0, 0, 0x3c00}, {0, 0x5640, 0, 0x3c00}};
    for (const auto &p : positions)
    {
        for (const auto v : p)
            Monstey_AppendU16(vertices, v);
        for (const auto v : {63u, 128u, 64u, 0u})
            vertices.push_back(static_cast<std::uint8_t>(v));
        for (const auto v : {2u, 1u, 0u, 1u})
            vertices.push_back(static_cast<std::uint8_t>(v));
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
    const Monstey_Bytes palette = {0, 2, 3};
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
    h[10] = 8;
    h[16] = 4;
    h[17] = 160;
    Monstey_Bytes info;
    for (const auto v : h)
        Monstey_AppendU32(info, v);
    info.insert(info.end(), 48, 0x22);
    for (unsigned c = 0; c < 2; ++c)
    {
        for (const auto v :
             {static_cast<std::uint32_t>(vertices.size()), static_cast<std::uint32_t>(faces.size()), 3u, 1u})
            Monstey_AppendU32(info, v);
        for (unsigned i = 0; i < 16; ++i)
            Monstey_AppendU16(info, i % 3 ? 1 : 0);
    }
    for (unsigned c = 0; c < 2; ++c)
        for (const auto v : {0u, 0u, 3u, 0u, 3u, 0u, 3u, 0x11223344u + c, 0x55667788u, 0u})
            Monstey_AppendU32(info, v);
    Monstey_AppendU32(info, 0x8e3e068e);
    Monstey_AppendU32(info, 0x37d749a6);
    Monstey_AppendU32(info, 8);
    for (const auto &a : std::array<std::array<std::uint8_t, 4>, 8>{{{{0, 12, 0, 0}},
                                                                     {{0, 4, 6, 0}},
                                                                     {{0, 4, 7, 0}},
                                                                     {{1, 9, 4, 0}},
                                                                     {{1, 12, 1, 0}},
                                                                     {{1, 12, 2, 0}},
                                                                     {{1, 12, 3, 0}},
                                                                     {{1, 4, 5, 0}}}})
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

Monstey_Bytes Monstey_StaticRigidMeshFixture()
{
    Monstey_Bytes vertices;
    const std::uint16_t positions[3][4] = {{0, 0, 0, 0x3c00}, {0x5640, 0, 0, 0x3c00}, {0, 0x5640, 0, 0x3c00}};
    for (const auto &p : positions)
    {
        for (const auto v : p)
            Monstey_AppendU16(vertices, v);
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
    const Monstey_Bytes palette{};
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
    h[10] = 6;
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
            Monstey_AppendU16(info, i % 3 ? 1 : 0);
    }
    for (unsigned c = 0; c < 2; ++c)
        for (const auto v : {0u, 0u, 3u, 0u, 3u, 0u, 0u, 0x11223344u + c, 0x55667788u, 0u})
            Monstey_AppendU32(info, v);
    Monstey_AppendU32(info, 0x8e3e068e);
    Monstey_AppendU32(info, 0x37d749a6);
    Monstey_AppendU32(info, 6);
    for (const auto &a : std::array<std::array<std::uint8_t, 4>, 6>{
             {{{0, 12, 0, 0}}, {{1, 9, 4, 0}}, {{1, 12, 1, 0}}, {{1, 12, 2, 0}}, {{1, 12, 3, 0}}, {{1, 4, 5, 0}}}})
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

B RigidTemplate()
{
    B out;
    const std::array<std::uint32_t, 14> header{
        0x42465250, 8, Monstey_PathHash("prefabs/test-template.inst"), 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
    for (const auto word : header)
        Monstey_AppendU32(out, word);
    B primary(44), secondary(44);
    Monstey_PutU32(primary, 0, 0x12345678);
    Monstey_PutU32(primary, 4, 0x11223344);
    Monstey_PutU32(primary, 16, 0);
    Monstey_PutU32(secondary, 0, 0x88776655);
    Monstey_PutU32(secondary, 16, 0xffffffff);
    out.insert(out.end(), primary.begin(), primary.end());
    out.insert(out.end(), secondary.begin(), secondary.end());
    for (unsigned i = 0; i < 5; ++i)
        Monstey_AppendU32(out, 0);
    return out;
}
B Material()
{
    const std::string shader = "shaders/test.fxo", texture = "textures/test.tex", name = "DiffuseMap";
    B out(30);
    Monstey_PutU32(out, 0, 0x494d544c);
    Monstey_PutU32(out, 4, 8);
    Monstey_PutU32(out, 8, 0x11223344);
    Monstey_PutU32(out, 12, static_cast<std::uint32_t>(texture.size() + 1));
    Monstey_PutU16(out, 16, 1);
    Monstey_PutU16(out, 28, static_cast<std::uint16_t>(shader.size() + 1));
    out.insert(out.end(), shader.begin(), shader.end());
    out.push_back(0);
    out.insert(out.end(), texture.begin(), texture.end());
    out.push_back(0);
    Monstey_AppendU32(out, 8);
    Monstey_AppendU16(out, static_cast<std::uint16_t>(name.size() + 1));
    out.insert(out.end(), name.begin(), name.end());
    out.push_back(0);
    Monstey_AppendU32(out, 0);
    return out;
}
B Shader()
{
    B b(32);
    Monstey_PutU32(b, 0, 0x52485253);
    Monstey_PutU32(b, 28, 0x11223344);
    const std::string label = "fulldeferred_rigid";
    b.insert(b.end(), label.begin(), label.end());
    b.push_back(0);
    return b;
}
J Path(const std::filesystem::path &p)
{
    return Monstey_Utf8(p.wstring());
}
void JsonFile(const std::filesystem::path &p, const J &j)
{
    Monstey_WriteNewText(p, j.dump(2));
}
J Resource(const std::filesystem::path &p, std::string name)
{
    return J::object({{"resource_path", std::move(name)},
                      {"path", Path(p)},
                      {"bytes", std::filesystem::file_size(p)},
                      {"sha256", Monstey_HashSha256(p)}});
}
void Synthetic(const std::filesystem::path &out)
{
    const auto mesh = Monstey_StaticMeshFixture(), templ = RigidTemplate();
    const auto built = Monstey_StaticPrefabBuild(mesh, 0, "prefabs/test-static.inst", "materials/test.mat", templ);
    Check(built.proof.at("source_channels_and_triangle_order_exact").as_bool() &&
              built.proof.at("vertices").as_uint() == 3 && built.proof.at("triangles").as_uint() == 1,
          "Original synthetic MMSH converts to rigid PRFB with exact channels");
    const auto data = Monstey_InstReadGeometry(built.bytes);
    Check(data.vertex_buffer.size() == 192 && data.index_buffer.size() == 6 && data.draws.at(0).at(11) == 0x11223344 &&
              data.draws.at(0).at(12) == 0x55667788,
          "Native64-byte streams and per-draw parameter words retained");
    const auto second = Monstey_StaticPrefabBuild(mesh, 1, "prefabs/test-static2.inst", "materials/test.mat", templ);
    Check(Monstey_InstReadGeometry(second.bytes).draws.at(0).at(11) == 0x11223345,
          "Selected source container is honored");
    Reject([&] { Monstey_StaticPrefabBuild(mesh, 2, "prefabs/test.inst", "materials/test.mat", templ); },
           "Outside source container rejected");
    Reject(
        [&] {
            Monstey_StaticPrefabBuild(mesh, 0, "prefabs/test.inst", "materials/test.mat", templ,
                                      std::vector<std::size_t>{0, 0});
        },
        "Duplicate selected source part rejected");
    auto badTemplate = templ;
    Monstey_PutU32(badTemplate, 56 + 44 + 40, 1);
    Reject([&] { Monstey_StaticPrefabBuild(mesh, 0, "prefabs/test.inst", "materials/test.mat", badTemplate); },
           "Unqualified native terminal definition rejected");
    const auto mat = Material(), shader = Shader();
    Check(Monstey_StaticMaterialShader(mat, shader).at("native_shader_id_matches").as_bool(),
          "Shared LTMI and native rigid SRHR contract validate");
    auto badShader = shader;
    Monstey_PutU32(badShader, 28, 99);
    Reject([&] { Monstey_StaticMaterialShader(mat, badShader); }, "Material/shader ID mismatch rejected");
    badShader = shader;
    badShader.resize(32);
    Reject([&] { Monstey_StaticMaterialShader(mat, badShader); }, "Missing native rigid permutation label rejected");
    const auto src = out / L"input.mesh", tmp = out / L"template.inst", mp = out / L"input.mat",
               sp = out / L"input.fxo", tx = out / L"input.tex", peer = out / L"input.tex0",
               mf = out / L"textures.json";
    Monstey_WriteNewFile(src, mesh);
    Monstey_WriteNewFile(tmp, templ);
    Monstey_WriteNewFile(mp, mat);
    Monstey_WriteNewFile(sp, shader);
    Monstey_WriteNewFile(tx, B{'T', 'E', 'X', 'R', 1, 2, 3, 4});
    Monstey_WriteNewFile(peer, B{'p', 'e', 'e', 'r'});
    JsonFile(mf, J::object({{"resources",
                             J::array({Resource(tx, "textures/test.tex"), Resource(peer, "textures/test.tex0")})}}));
    auto args = J::object({{"source", Path(src)},
                           {"material", Path(mp)},
                           {"shader", Path(sp)},
                           {"template", Path(tmp)},
                           {"texture_manifest", Path(mf)},
                           {"output", Path(out / L"package")},
                           {"resource_path", "prefabs/test-static.inst"},
                           {"material_path", "materials/test.mat"}});
    JsonFile(out / L"shader-contract.json",
             J::object({{"material", J::object({{"sha256", Monstey_HashSha256(mat)}})},
                        {"shader", J::object({{"sha256", Monstey_HashSha256(shader)}})}}));
    args["shader_contract"] = Path(out / L"shader-contract.json");
    const auto result = Monstey_StaticPrefabConvert(args);
    Check(result.at("shader_contract").as_string() == Monstey_Utf8(std::filesystem::absolute(out / L"shader-contract.json").lexically_normal().wstring()),
          "Explicit native shader evidence is hash-qualified and retained");
    Check(result.at("status").as_string() == "PRIVATE_STATIC_PREFAB_READY_FOR_RUNTIME_CANDIDATE" &&
              result.at("resources").size() == 4,
          "Static native package resolves material, shader and texture dependencies");
    Check(result.at("external_dependencies").size() == 1 &&
              !result.at("external_dependencies").at(0).at("embed_in_embb").as_bool(),
          "Streaming peer remains untouched and external");
    Check(Monstey_ReadFile(out / L"package" / L"assets" / L"materials" / L"test.mat") == mat &&
              Monstey_ReadFile(tx) == B({'T', 'E', 'X', 'R', 1, 2, 3, 4}),
          "Material bytes copied exactly while texture bytes stay at source");
    const auto persisted = J::Parse(Monstey_ReadText(out / L"package" / L"resource-manifest.json"));
    Check(persisted == result, "Returned package manifest equals persisted manifest");
    Reject([&] { Monstey_StaticPrefabConvert(args); }, "Existing static output package cannot be overwritten");
    JsonFile(out / L"wrong-contract.json",
             J::object({{"material", J::object({{"sha256", std::string(64, '0')}})},
                        {"shader", J::object({{"sha256", Monstey_HashSha256(shader)}})}}));
    auto evidenceArgs = args;
    evidenceArgs["shader_contract"] = Path(out / L"wrong-contract.json");
    evidenceArgs["output"] = Path(out / L"wrong-evidence-package");
    Reject([&] { Monstey_StaticPrefabConvert(evidenceArgs); }, "Shader evidence for different bytes rejected");
    Check(!std::filesystem::exists(out / L"wrong-evidence-package"), "Mismatching shader evidence publishes no output");
    auto changed = args;
    changed["output"] = Path(out / L"bad-template");
    changed["template_expected_sha256"] = std::string(64, '0');
    Reject([&] { Monstey_StaticPrefabConvert(changed); }, "Explicit native template hash mismatch rejected");
    Check(!std::filesystem::exists(out / L"bad-template"), "Failed template qualification publishes no package");
    Monstey_WriteNewFile(out / L"changed.tex", B{9});
    auto stale = Resource(out / L"changed.tex", "textures/test.tex");
    stale["sha256"] = std::string(64, '0');
    JsonFile(out / L"stale.json", J::object({{"resources", J::array({stale})}}));
    changed = args;
    changed["output"] = Path(out / L"stale-package");
    changed["texture_manifest"] = Path(out / L"stale.json");
    Reject([&] { Monstey_StaticPrefabConvert(changed); }, "Changed dependency manifest resource rejected");
    Check(!std::filesystem::exists(out / L"stale-package"), "Dependency failure leaves no output package");
    JsonFile(out / L"empty.json", J::object({{"resources", J::array()}}));
    changed = args;
    changed["output"] = Path(out / L"pending-package");
    changed["texture_manifest"] = Path(out / L"empty.json");
    const auto pending = Monstey_StaticPrefabConvert(changed);
    Check(pending.at("status").as_string() == "STATIC_PREFAB_GEOMETRY_READY_DEPENDENCIES_PENDING" &&
              pending.at("unresolved_shared_dependencies").size() == 1,
          "Unresolved dependency is recorded explicitly with geometry candidate");
    JsonFile(out / L"shared-dependencies.json",
             J::object({{"resources", J::array({Resource(tx, "textures/test.tex")})}}));
    changed = args;
    changed["output"] = Path(out / L"dependency-package");
    changed["texture_manifest"] = Path(out / L"empty.json");
    changed["dependency_manifests"] = J::array({Path(out / L"shared-dependencies.json")});
    const auto resolved = Monstey_StaticPrefabConvert(changed);
    Check(resolved.at("explicit_material_dependencies_resolved").as_bool() &&
              resolved.at("unresolved_shared_dependencies").empty(),
          "Additional explicit dependency manifest resolves shared material texture");
    Check(Monstey_HashSha256(src) == Monstey_HashSha256(mesh) && Monstey_HashSha256(tmp) == Monstey_HashSha256(templ),
          "Original synthetic native source and template unchanged");
}
void OptionalSkin()
{
    const auto templ = RigidTemplate();
    const auto built =
        Monstey_StaticPrefabBuild(Monstey_StaticMeshFixture(), 0, "prefabs/skinned.inst", "materials/test.mat", templ);
    const auto rigidSource = Monstey_StaticRigidMeshFixture();
    const auto rigid = Monstey_StaticPrefabBuild(rigidSource, 0, "prefabs/unskinned.inst", "materials/test.mat", templ);
    Check(Monstey_InstReadGeometry(rigid.bytes).vertex_buffer == Monstey_InstReadGeometry(built.bytes).vertex_buffer,
          "MMSH with no skin channels retains identical rigid geometry");
}

void Native(const std::filesystem::path &root, const std::filesystem::path &out)
{
    const auto mesh = root / L"sow-port" / L"aeglos-crimson-reaver" / L"geometry-v002" / L"assets" / L"characters" /
                      L"modelweapons" / L"player" / L"1h_spear" / L"monstey_aeglos_reaver" / L"aeglos_reaver.mesh",
               templ = root / L"sow-port" / L"world-edit" / L"inst-v001" / L"source" / L"prefabs" / L"common" /
                       L"lights" / L"torch01" / L"torch03.inst";
    const auto source = Monstey_ReadFile(mesh), nativeTemplate = Monstey_ReadFile(templ);
    const auto built = Monstey_StaticPrefabBuild(source, 0, "prefabs/monstey/test-reaver.inst",
                                                 "prefabs/monstey/materials/test-reaver.mat", nativeTemplate);
    Check(built.proof.at("source_channels_and_triangle_order_exact").as_bool(),
          "Actual Reaver MMSH converts through actual native terminal rigid template");
    Check(built.proof.at("source_colour_bytes_exact").as_bool() && built.proof.at("vertices").as_uint() > 100,
          "Actual source colors and complete geometry preserved");
    Monstey_WriteNewFile(out / L"native-reaver.inst", built.bytes);
    JsonFile(out / L"native-geometry-proof.json", built.proof);
    const auto material = root / L"sow-port" / L"aeglos-crimson-reaver" / L"materials-v001" / L"assets" /
                          L"characters" / L"modelweapons" / L"player" / L"1h_spear" / L"monstey_aeglos_reaver" /
                          L"materials" / L"aeglos_reaver.mat",
               shader = root / L"sow-port" / L"aeglos-crimson-reaver" / L"source-v001" / L"embedded" / L"shaders" /
                        L"shadertrees" / L"solid" / L"talion" / L"talion_weapon.fxo",
               manifest = root / L"sow-port" / L"aeglos-crimson-reaver" / L"materials-v001" / L"resource-manifest.json";
    auto args = J::object({{"source", Path(mesh)},
                           {"material", Path(material)},
                           {"shader", Path(shader)},
                           {"template", Path(templ)},
                           {"texture_manifest", Path(manifest)},
                           {"output", Path(out / L"native-package")},
                           {"resource_path", "prefabs/monstey/test-reaver.inst"},
                           {"material_path", "prefabs/monstey/materials/test-reaver.mat"}});
    args["dependency_manifests"] =
        J::array({Path(root / L"sow-port" / L"world-edit" / L"static-mesh-v001" / L"shared-dependency-manifest.json")});
    const auto result = Monstey_StaticPrefabConvert(args);
    Check(result.at("status").as_string() == "PRIVATE_STATIC_PREFAB_READY_FOR_RUNTIME_CANDIDATE" &&
              result.at("unresolved_shared_dependencies").empty(),
          "Actual native shared dependencies resolve into complete static resource manifest");
    Check(result.at("shader_validation_details").at("native_shader_id_matches").as_bool(),
          "Actual LTMI/SRHR byte identities and explicit rigid label qualified");
    Check(result.at("held_weapon_files_unchanged").as_bool() &&
              result.at("source_texture_resolution_preserved").as_bool(),
          "Actual static package keeps original held weapon and texture sources");
    Check(Monstey_HashSha256(mesh) == Monstey_HashSha256(source) &&
              Monstey_HashSha256(templ) == Monstey_HashSha256(nativeTemplate),
          "Actual source and template remain unchanged");
    JsonFile(out / L"native-package-proof.json", result);
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        const auto out = (argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(L"static-prefab-tests")) /
                         std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(out);
        Synthetic(out);
        if (argc > 2)
            Native(argv[2], out);
        OptionalSkin();
        std::cout << "PASS " << passed << " native static prefab groups\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
