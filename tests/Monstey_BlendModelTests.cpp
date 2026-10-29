#include "Monstey_Blend.hpp"
#include "Monstey_BlendGeometry.hpp"
#include "Monstey_BlendModel.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <iostream>
#include <set>
#include <windows.h>
using namespace Monstey;
namespace {
class Monstey_TestEnvironment {
    std::wstring name_, value_;
    bool existed_ = false;

  public:
    explicit Monstey_TestEnvironment(const wchar_t *name) : name_(name) {
        SetLastError(ERROR_SUCCESS);
        const auto n = GetEnvironmentVariableW(name, nullptr, 0);
        existed_ = n != 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND;
        if (n) {
            value_.resize(n);
            const auto read = GetEnvironmentVariableW(name, value_.data(), n);
            Monstey_Require(read < n, "Test environment changed during capture");
            value_.resize(read);
        }
        Monstey_Require(SetEnvironmentVariableW(name, nullptr) != FALSE, "Cannot isolate test environment");
    }
    ~Monstey_TestEnvironment() {
        SetEnvironmentVariableW(name_.c_str(), existed_ ? value_.c_str() : nullptr);
    }
};
void Monstey_BlendTestRemove(const std::filesystem::path &p) {
    if (std::filesystem::exists(p))
        std::filesystem::remove(p);
}
std::size_t Monstey_BlendTestIndex(const Monstey_Json &j) {
    return static_cast<std::size_t>(j["index"].as_uint());
}
std::size_t Monstey_BlendTestMesh(Monstey_Blend &b) {
    const auto rows = b.Blocks("ME");
    for (const auto &r : rows["rows"].as_array()) {
        auto i = Monstey_BlendTestIndex(r);
        if (b.ReadField(i, "id.name").as_string().starts_with("MESoW_C"))
            return i;
    }
    throw Monstey_Error("test", "No imported native mesh");
}
std::size_t Monstey_BlendTestAttribute(Monstey_Blend &b, std::size_t mesh, std::string_view name) {
    const auto ptr = b.ReadField(mesh, "attribute_storage.dna_attributes")["pointer"].as_uint();
    const auto block = b.BlockAtAddress(ptr, mesh),
               n = b.ReadField(mesh, "attribute_storage.dna_attributes_num").as_uint();
    for (std::size_t k = 0; k < n; ++k) {
        auto p = b.ReadField(block, "name", k)["pointer"].as_uint();
        const auto bytes = b.BlockData(b.BlockAtAddress(p, block));
        const std::string text(bytes.begin(), std::find(bytes.begin(), bytes.end(), 0));
        if (text == name)
            return b.BlockAtAddress(b.ReadField(block, "data", k)["pointer"].as_uint(), block);
    }
    throw Monstey_Error("test", "Missing authored attribute");
}
void Monstey_BlendTestEditFloats(Monstey_Blend &b, std::size_t mesh, std::string_view name,
                                 unsigned component, float value) {
    const auto arr = Monstey_BlendTestAttribute(b, mesh, name);
    const auto data = b.BlockAtAddress(b.ReadField(arr, "data")["pointer"].as_uint(), arr);
    auto bytes = b.BlockData(data);
    Monstey_PutU32(bytes, component * 4, std::bit_cast<std::uint32_t>(value));
    b.ReplaceData(data, std::move(bytes));
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        Monstey_Require(argc >= 4, "Usage: BlendModelTests outputDir fixture.mesh fixture.skel "
                                   "[template.blend] [stock-resaved.blend] [stock-edited.blend]");
        const std::filesystem::path dir = argv[1], mesh = argv[2], skel = argv[3];
        std::filesystem::create_directories(dir);
        Monstey_Json report = Monstey_Json::object({{"status", "RUNNING"},
                                                    {"cases", Monstey_Json::array()},
                                                    {"production_python_runtime_used", false}});
        unsigned passed = 0;
        std::set<std::string> names;
        auto test = [&](const std::string &name, const std::function<void()> &fn) {
            Monstey_Require(names.insert(name).second, "Duplicate test name");
            try {
                fn();
            } catch (const std::exception &e) {
                throw Monstey_Error("test", name + ": " + e.what());
            }
            report["cases"].as_array().push_back(name);
            ++passed;
        };
        auto reject = [](const std::function<void()> &fn) {
            bool caught = false;
            try {
                fn();
            } catch (const std::exception &) {
                caught = true;
            }
            Monstey_Require(caught, "Invalid native Blender data was accepted");
        };
        auto options = Monstey_Json::object();
        if (argc > 4)
            options["blend_template"] = Monstey_Utf8(std::filesystem::path(argv[4]).wstring());
        const auto native = Monstey_ModelReadScene(mesh, skel);
        const auto sourceHash = Monstey_HashSha256(mesh);
        const auto blend = dir / L"native-model.blend";
        test("native_import_authors_scene_mesh_skin_rig_and_persistent_metadata", [&] {
            Monstey_BlendTestRemove(blend);
            report["import"] = Monstey_ModelImportNative(mesh, blend, skel, options);
            auto scene = Monstey_ModelReadBlendScene(blend);
            Monstey_Require(scene.parts.size() == native.parts.size() &&
                                scene.bones.size() == native.bones.size() &&
                                scene.native_baseline == native.native_baseline,
                            "Authored model scene differs");
        });
        test("native_import_authors_qualified_triangle_bands_as_real_face_int_data", [&] {
            Monstey_Blend b(blend);
            const auto mi = Monstey_BlendTestMesh(b);
            const auto attrs = Monstey_BlendGeometryAttributes(b, mi);
            const auto &source = native.parts.front();
            const auto read = Monstey_ModelReadBlendScene(blend);
            Monstey_Require(read.parts.front().triangle_bands == source.triangle_bands &&
                                read.parts.front().triangle_band_topology_sha256 == source.triangle_band_topology_sha256,
                            "Native Blender import discarded bound face membership");
            if (source.triangle_band_topology_sha256.empty()) {
                Monstey_Require(!attrs.contains("monstey_triangle_band"),
                                "Unqualified native source acquired fabricated face bands");
                return;
            }
            const auto &a = attrs.at("monstey_triangle_band");
            Monstey_Require(a.type == 3 && a.domain == 2 && a.count == source.indices.size() / 3 &&
                                a.data.size() == a.count * 4,
                            "Native face bands were not real FACE-domain INT data");
            for (std::size_t face = 0; face < a.count; ++face)
                Monstey_Require(Monstey_U32(a.data, face * 4) == source.triangle_bands[face] + 1u,
                                "Native face band's ordinal changed during Blender import");
        });
        test("explicit_uniform_band_option_reaches_native_blender_reader_before_unassigned_rejection", [&] {
            if (native.parts.front().triangle_band_topology_sha256.empty())
                return;
            Monstey_Blend b(blend);
            const auto mi = Monstey_BlendTestMesh(b), array = Monstey_BlendTestAttribute(b, mi, "monstey_triangle_band");
            const auto data = b.BlockAtAddress(b.ReadField(array, "data")["pointer"].as_uint(), array);
            auto bytes = b.BlockData(data);
            Monstey_PutU32(bytes, 0, 0);
            b.ReplaceData(data, bytes);
            const auto edited = dir / L"unassigned-bands.blend", out = dir / L"explicit-bands.mesh";
            Monstey_BlendTestRemove(edited);
            Monstey_BlendTestRemove(out);
            b.Write(edited);
            reject([&] { (void)Monstey_ModelReadBlendScene(edited); });
            const auto r = Monstey_ModelExportNative(edited, {}, out, Monstey_Json::object({{"triangle_band", 7u}}));
            const auto check = Monstey_ModelReadScene(out);
            Monstey_Require(r["triangle_band_policy"].as_string() == "explicit_uniform" &&
                                r["triangle_band"].as_uint() == 7,
                            "Native Blender export lost explicit policy diagnostics");
            for (const auto &part : check.parts)
                Monstey_Require(!part.triangle_bands.empty() &&
                                    std::all_of(part.triangle_bands.begin(), part.triangle_bands.end(),
                                                [](const auto band) { return band == 7; }),
                                "Native Blender export did not apply explicit band to supplied faces");
        });
        test("configured_relocated_blender_import_uses_native_template_without_execution", [&] {
            const auto sourceTemplate = Monstey_BlendGeometryTemplate(options);
            Monstey_TestEnvironment envTemplate(L"MONSTEY_BLENDER_TEMPLATE"), envExe(L"MONSTEY_BLENDER");
            const auto portable = dir / L"relocated-blender";
            const auto executable = portable / L"blender.exe";
            const auto stock =
                portable / L"5.2/scripts/startup/bl_app_templates_system/Sculpting/startup.blend";
            std::filesystem::create_directories(stock.parent_path());
            Monstey_BlendTestRemove(executable);
            Monstey_BlendTestRemove(stock);
            Monstey_WriteNewFile(executable, Monstey_Bytes{'N', 'O', 'T', '_', 'E', 'X', 'E'});
            Monstey_WriteNewFile(stock, Monstey_ReadFile(sourceTemplate));
            const auto output = dir / L"relocated-native-model.blend";
            const auto nativeOutput = dir / L"relocated-roundtrip.mesh";
            Monstey_BlendTestRemove(output);
            Monstey_BlendTestRemove(nativeOutput);
            const auto r = Monstey_ModelImportNative(
                mesh, output, skel, Monstey_Json::object({{"blender", Monstey_Utf8(executable.wstring())}}));
            Monstey_Require(r["blend_template"].as_string() ==
                                    Monstey_Utf8(std::filesystem::absolute(stock).wstring()) &&
                                r["blend_template_sha256"].as_string() == Monstey_HashSha256(stock),
                            "Configured relocated import did not use the actual native template snapshot");
            const auto nativeReport = Monstey_ModelExportNative(output, {}, nativeOutput);
            Monstey_Require(nativeReport["byte_exact_noop"].as_bool() &&
                                Monstey_HashSha256(nativeOutput) == sourceHash,
                            "Relocated native import/export changed the model");
        });
        test("embedded_baseline_export_is_byte_exact_without_external_template", [&] {
            const auto out = dir / L"roundtrip.mesh";
            Monstey_BlendTestRemove(out);
            auto r = Monstey_ModelExportNative(blend, {}, out);
            Monstey_Require(r["byte_exact_noop"].as_bool() && Monstey_HashSha256(out) == sourceHash,
                            "Blender no-op is not byte exact");
        });
        test("native_blender_preflight_validates_without_publishing", [&] {
            const auto out = dir / L"preflight.mesh";
            Monstey_BlendTestRemove(out);
            auto opt = Monstey_Json::object({{"preflight", true}});
            auto r = Monstey_ModelExportNative(blend, {}, out, opt);
            Monstey_Require(!r["published"].as_bool() && !std::filesystem::exists(out),
                            "Preflight published output");
        });
        test("source_snapshot_and_explicit_template_hash_are_checked", [&] {
            const auto out = dir / L"bad-sha.mesh";
            Monstey_BlendTestRemove(out);
            reject([&] {
                Monstey_ModelExportNative(
                    blend, {}, out, Monstey_Json::object({{"expected_source_sha256", std::string(64, '0')}}));
            });
            reject([&] {
                Monstey_ModelExportNative(
                    blend, mesh, out,
                    Monstey_Json::object({{"expected_template_sha256", std::string(64, '0')}}));
            });
            Monstey_Require(!std::filesystem::exists(out), "Rejected source published output");
        });
        test("source_hash_qualifies_blend_snapshot_not_embedded_native_baseline", [&] {
            const auto out = dir / L"qualified-source.mesh";
            Monstey_BlendTestRemove(out);
            const auto hash = Monstey_HashSha256(blend);
            auto result = Monstey_ModelExportNative(blend, {}, out,
                                                    Monstey_Json::object({{"expected_source_sha256", hash}}));
            Monstey_Require(result["source_sha256"].as_string() == hash &&
                                Monstey_HashSha256(out) == sourceHash,
                            "Actual blend source SHA256 was not qualified");
            Monstey_BlendTestRemove(out);
            reject([&] {
                Monstey_ModelExportNative(blend, {}, out,
                                          Monstey_Json::object({{"expected_source_sha256", sourceHash}}));
            });
            Monstey_Require(!std::filesystem::exists(out),
                            "Native baseline hash incorrectly qualified a blend source");
        });
        test("authored_positions_normals_tangents_bitangents_and_uv_are_applied", [&] {
            Monstey_Blend b(blend);
            const auto mi = Monstey_BlendTestMesh(b);
            Monstey_BlendTestEditFloats(b, mi, "position", 0, 1.25f);
            Monstey_BlendTestEditFloats(b, mi, "custom_normal", 0, .25f);
            Monstey_BlendTestEditFloats(b, mi, "_SOW_TANGENT", 1, .5f);
            Monstey_BlendTestEditFloats(b, mi, "_SOW_BITANGENT", 2, .75f);
            Monstey_BlendTestEditFloats(b, mi, "UVMap", 0, .125f);
            const auto edited = dir / L"channels-edited.blend", out = dir / L"channels-edited.mesh";
            Monstey_BlendTestRemove(edited);
            Monstey_BlendTestRemove(out);
            b.Write(edited);
            Monstey_ModelExportNative(edited, {}, out);
            const auto scene = Monstey_ModelReadScene(out);
            const auto &c = scene.parts[0].channels;
            Monstey_Require(scene.parts[0].triangle_bands == native.parts[0].triangle_bands,
                            "Position/channel editing changed native triangle membership");
            Monstey_Require(std::abs(c.at("POSITION").values[0] - 125) < .1 &&
                                std::abs(c.at("NORMAL").values[0] - .25) < .002 &&
                                std::abs(c.at("TANGENT").values[1] - .5) < .002 &&
                                std::abs(c.at("BITANGENT").values[2] - .75) < .002 &&
                                std::abs(c.at("UV0").values[static_cast<std::size_t>(scene.parts[0].indices[0]) * 2] - .125) < .002,
                            "Edited float channels were discarded");
        });
        test("authored_vertex_groups_override_saved_raw_weight_attributes", [&] {
            Monstey_Blend b(blend);
            const auto mi = Monstey_BlendTestMesh(b);
            const auto layers = b.BlockAtAddress(b.ReadField(mi, "vdata.layers")["pointer"].as_uint(), mi);
            const auto dv = b.BlockAtAddress(b.ReadField(layers, "data")["pointer"].as_uint(), layers);
            const auto dw = b.BlockAtAddress(b.ReadField(dv, "dw", 0)["pointer"].as_uint(), dv);
            b.SetField(dv, "totweight", 1, 0);
            b.SetField(dw, "def_nr", 0, 0);
            b.SetField(dw, "weight", 1.0, 0);
            const auto edited = dir / L"weights-edited.blend", out = dir / L"weights-edited.mesh";
            Monstey_BlendTestRemove(edited);
            Monstey_BlendTestRemove(out);
            b.Write(edited);
            Monstey_ModelExportNative(edited, {}, out);
            const auto scene = Monstey_ModelReadScene(out);
            const auto &c = scene.parts[0].channels;
            double root = 0;
            for (unsigned k = 0; k < 4; ++k)
                if (c.at("JOINTS").values[k] == 0)
                    root += c.at("WEIGHTS_U8").values[k];
            Monstey_Require(root == 255, "Authored native skin weights did not win");
        });
        test("authored_native_vertex_colors_are_applied", [&] {
            Monstey_Blend b(blend);
            const auto mi = Monstey_BlendTestMesh(b), arr = Monstey_BlendTestAttribute(b, mi, "_SOW_COLOR0"),
                       data = b.BlockAtAddress(b.ReadField(arr, "data")["pointer"].as_uint(), arr);
            auto bytes = b.BlockData(data);
            bytes[0] = 17;
            bytes[1] = 44;
            bytes[2] = 199;
            bytes[3] = 88;
            b.ReplaceData(data, std::move(bytes));
            const auto edit = dir / L"colors-edited.blend", out = dir / L"colors-edited.mesh";
            Monstey_BlendTestRemove(edit);
            Monstey_BlendTestRemove(out);
            b.Write(edit);
            Monstey_ModelExportNative(edit, {}, out);
            const auto scene = Monstey_ModelReadScene(out);
            const auto &c = scene.parts[0].channels.at("COLOR0").values;
            Monstey_Require(c[0] == 17 && c[1] == 44 && c[2] == 199 && c[3] == 88,
                            "Authored vertex colors discarded");
        });
        if (native.parts.front().channels.at("POSITION").values.size() == 9)
            test("authored_uv_seams_split_every_native_vertex_channel", [&] {
                Monstey_Blend b(blend);
                const auto mi = Monstey_BlendTestMesh(b);

                auto array = [&](std::string_view name, const Monstey_Bytes &bytes, std::size_t count) {
                    const auto arr = Monstey_BlendTestAttribute(b, mi, name),
                               data = b.BlockAtAddress(b.ReadField(arr, "data")["pointer"].as_uint(), arr);
                    b.ReplaceData(data, bytes);
                    b.SetField(arr, "size", static_cast<std::uint64_t>(count));
                };
                Monstey_Bytes corners, edges, offsets, uv;
                for (auto v : {0u, 1u, 2u, 0u, 2u, 1u})
                    Monstey_AppendU32(corners, v);
                for (auto v : {0u, 1u, 2u, 2u, 1u, 0u})
                    Monstey_AppendU32(edges, v);
                for (auto v : {0u, 3u, 6u})
                    Monstey_AppendU32(offsets, v);
                for (float v : {0.f, 0.f, 1.f, 0.f, 0.f, 1.f, .5f, .5f, 0.f, 1.f, 1.f, 0.f})
                    Monstey_AppendU32(uv, std::bit_cast<std::uint32_t>(v));
                array(".corner_vert", corners, 6);
                array(".corner_edge", edges, 6);
                array("UVMap", uv, 6);
                if (!native.parts.front().triangle_band_topology_sha256.empty()) {
                    Monstey_Bytes bands;
                    const auto stored = static_cast<std::uint32_t>(native.parts.front().triangle_bands.front()) + 1;
                    Monstey_AppendU32(bands, stored);
                    Monstey_AppendU32(bands, stored);
                    array("monstey_triangle_band", bands, 2);
                }
                const auto off =
                    b.BlockAtAddress(b.ReadField(mi, "poly_offset_indices")["pointer"].as_uint(), mi);
                b.ReplaceData(off, offsets);
                b.SetField(mi, "totloop", 6);
                b.SetField(mi, "totpoly", 2);
                const auto edit = dir / L"uv-seam.blend", out = dir / L"uv-seam.mesh";
                Monstey_BlendTestRemove(edit);
                Monstey_BlendTestRemove(out);
                b.Write(edit);
                Monstey_ModelExportNative(edit, {}, out);
                const auto scene = Monstey_ModelReadScene(out);
                const auto &part = scene.parts[0];
                Monstey_Require(part.channels.at("POSITION").values.size() == 12 &&
                                    part.indices.size() == 6 && part.indices[0] != part.indices[3],
                                "UV seam did not split native vertices");
                for (const auto &[name, channel] : part.channels) {
                    (void)name;
                    Monstey_Require(channel.values.size() == 4 * channel.width,
                                    "UV split lost a native vertex channel");
                }
                Monstey_Require(std::abs(part.channels.at("UV0").values[part.indices[3] * 2] - .5) < .002,
                                "Authored seam UV discarded");
                if (!native.parts.front().triangle_band_topology_sha256.empty())
                    Monstey_Require(part.triangle_bands ==
                                        std::vector<std::uint8_t>(2, native.parts.front().triangle_bands.front()) &&
                                        part.triangle_band_topology_sha256 == Monstey_ModelTriangleTopologyHash(part.indices),
                                    "UV seam native export lost face membership or final topology qualification");
            });
        test("native_rig_rest_changes_are_not_silently_discarded", [&] {
            Monstey_Blend b(blend);
            const auto bones = b.Blocks("DATA", 0, 10000);
            std::size_t bone = static_cast<std::size_t>(-1);
            for (const auto &r : bones["rows"].as_array())
                if (r["type"].as_string() == "Bone") {
                    bone = Monstey_BlendTestIndex(r);
                    break;
                }
            Monstey_Require(bone != static_cast<std::size_t>(-1), "No actual Bone record");
            b.SetField(bone, "arm_head[0]", 1.0);
            const auto edit = dir / L"rig-edited.blend";
            Monstey_BlendTestRemove(edit);
            b.Write(edit);
            reject([&] { Monstey_ModelReadBlendScene(edit); });
        });
        test("unbaked_object_transforms_are_rejected_explicitly", [&] {
            Monstey_Blend b(blend);
            const auto rows = b.Blocks("OB", 0, 10000);
            bool edited = false;
            for (const auto &r : rows["rows"].as_array()) {
                const auto i = Monstey_BlendTestIndex(r);
                if (b.ReadField(i, "id.name").as_string().starts_with("OBSoW_C")) {
                    b.SetField(i, "loc[0]", 1.0);
                    edited = true;
                    break;
                }
            }
            Monstey_Require(edited, "No native object transform");
            const auto out = dir / L"object-transform.blend";
            Monstey_BlendTestRemove(out);
            b.Write(out);
            reject([&] { Monstey_ModelReadBlendScene(out); });
        });
        test("missing_native_part_identity_is_rejected", [&] {
            Monstey_Blend b(blend);
            auto mi = Monstey_BlendTestMesh(b);
            b.SetField(mi, "id.properties", 0);
            const auto edited = dir / L"missing-identity.blend";
            Monstey_BlendTestRemove(edited);
            b.Write(edited);
            reject([&] { Monstey_ModelReadBlendScene(edited); });
        });
        if (argc > 5)
            test("stock_blender_native_save_preserves_byte_exact_native_model", [&] {
                const auto out = dir / L"stock-resaved.mesh";
                Monstey_BlendTestRemove(out);
                auto r = Monstey_ModelExportNative(argv[5], {}, out);
                Monstey_Require(r["byte_exact_noop"].as_bool() && Monstey_HashSha256(out) == sourceHash,
                                "Stock Blender native save changed untouched model");
                report["stock_resaved_sha256"] = Monstey_HashSha256(std::filesystem::path(argv[5]));
            });
        if (argc > 6)
            test("actual_stock_blender_vertex_edit_changes_native_geometry", [&] {
                const auto out = dir / L"stock-edited.mesh";
                Monstey_BlendTestRemove(out);
                auto r = Monstey_ModelExportNative(argv[6], {}, out);
                Monstey_Require(!r["byte_exact_noop"].as_bool() && Monstey_HashSha256(out) != sourceHash,
                                "Actual stock edit discarded");
                report["stock_edited_sha256"] = Monstey_HashSha256(std::filesystem::path(argv[6]));
            });
        Monstey_Require(Monstey_HashSha256(mesh) == sourceHash, "Read-only native input changed");
        report["status"] = "PASS";
        report["passed"] = passed;
        report["native_sha256"] = sourceHash;
        const auto out = dir / L"blend-model-tests.json";
        Monstey_BlendTestRemove(out);
        Monstey_WriteNewText(out, report.dump(2) + "\n");
        std::cout << "Native Blender model tests: " << passed << " passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Native Blender model failure: " << e.what() << "\n";
        return 1;
    }
}
