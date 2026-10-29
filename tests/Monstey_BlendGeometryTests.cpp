#include "Monstey_BlendGeometry.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <iostream>
#include <windows.h>
using namespace Monstey;
namespace {
using J = Monstey_Json;
std::size_t Monstey_Index(const J &row) {
    return static_cast<std::size_t>(row[row.contains("index") ? "index" : "block"].as_uint());
}
std::uint64_t Monstey_Address(const J &row) { return row["address"].as_uint(); }
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
Monstey_ModelPart Monstey_GeometryFixture() {
    Monstey_ModelPart p;
    p.channels = {{"POSITION", {3, {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0}}},
                  {"NORMAL", {3, {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}}},
                  {"TANGENT", {3, {1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0}}},
                  {"BITANGENT", {3, {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}}},
                  {"UV0", {2, {0, 0, 1, 0, 1, 1, 0, 1}}},
                  {"COLOR0", {4, {11, 22, 33, 255, 44, 55, 66, 255, 77, 88, 99, 255, 1, 2, 3, 255}}}};
    p.indices = {0, 1, 2, 0, 2, 3};
    return p;
}
struct Monstey_GeometryAttributeLocation {
    std::size_t owner = 0, ordinal = 0, array = 0;
};
Monstey_GeometryAttributeLocation Monstey_GeometryAttribute(Monstey_Blend &b, std::size_t mesh,
                                                            std::string_view name) {
    const auto pointer = b.ReadField(mesh, "attribute_storage.dna_attributes")["pointer"].as_uint();
    const auto owner = b.BlockAtAddress(pointer, mesh);
    const auto count = b.ReadField(mesh, "attribute_storage.dna_attributes_num").as_uint();
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
        const auto np = b.ReadField(owner, "name", ordinal)["pointer"].as_uint();
        const auto bytes = b.BlockData(b.BlockAtAddress(np, owner));
        const std::string current(bytes.begin(), std::find(bytes.begin(), bytes.end(), 0));
        if (current == name)
            return {owner, ordinal,
                    b.BlockAtAddress(b.ReadField(owner, "data", ordinal)["pointer"].as_uint(), owner)};
    }
    throw Monstey_Error("test", "Expected Blender attribute missing: " + std::string(name));
}
void Monstey_GeometryReplaceAttribute(Monstey_Blend &b, std::size_t mesh, std::string_view name,
                                      const Monstey_Bytes &bytes, std::size_t count) {
    const auto location = Monstey_GeometryAttribute(b, mesh, name);
    const auto data = b.BlockAtAddress(b.ReadField(location.array, "data")["pointer"].as_uint(), location.array);
    b.ReplaceData(data, bytes);
    b.SetField(location.array, "size", static_cast<std::uint64_t>(count));
}
Monstey_Bytes Monstey_GeometryInts(std::initializer_list<std::uint32_t> values) {
    Monstey_Bytes bytes;
    for (const auto value : values)
        Monstey_AppendU32(bytes, value);
    return bytes;
}
Monstey_Bytes Monstey_GeometryFloats(const std::vector<double> &values) {
    Monstey_Bytes bytes;
    for (const auto value : values)
        Monstey_AppendU32(bytes, std::bit_cast<std::uint32_t>(static_cast<float>(value)));
    return bytes;
}
void Monstey_GeometryRejectMessage(const std::function<void()> &f, std::string_view fragment) {
    std::string message;
    try {
        f();
    } catch (const Monstey_Error &error) {
        message = error.what();
    }
    Monstey_Require(message.find(fragment) != std::string::npos,
                    "Invalid face bands were accepted or lacked a precise diagnostic: " + message);
}
J Monstey_FindObject(Monstey_Blend &b, std::string_view name) {
    const auto objects = Monstey_BlendGeometryObjects(b);
    for (const auto &row : objects.as_array())
        if (row["name"].as_string() == name)
            return row;
    throw Monstey_Error("test", "Authored object missing");
}
void Monstey_Equal(const Monstey_ModelPart &a, const Monstey_ModelPart &b) {
    Monstey_Require(a.indices == b.indices && a.channels.size() == b.channels.size() &&
                        a.triangle_bands == b.triangle_bands &&
                        a.triangle_band_topology_sha256 == b.triangle_band_topology_sha256,
                    "Geometry topology/channels/triangle bands changed");
    for (const auto &[name, ch] : a.channels)
        Monstey_Require(ch.width == b.channels.at(name).width && ch.values == b.channels.at(name).values,
                        "Geometry values changed: " + name);
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        const std::filesystem::path out = argc > 1 ? argv[1] : L"blend-geometry-tests";
        const std::filesystem::path input =
            argc > 2 ? std::filesystem::path(argv[2]) : Monstey_BlendGeometryTemplate();
        std::filesystem::create_directories(out);
        auto report = J::object({{"status", "PASS"}, {"python_runtime", false}, {"cases", J::array()}});
        const auto test = [&](std::string_view name, const std::function<void()> &f) {
            try {
                f();
            } catch (const std::exception &error) {
                throw Monstey_Error("test", std::string(name) + ": " + error.what());
            }
            report["cases"].as_array().push_back(std::string(name));
        };
        const auto reject = [&](const std::function<void()> &f) {
            bool rejected = false;
            try {
                f();
            } catch (const Monstey_Error &) {
                rejected = true;
            }
            Monstey_Require(rejected, "Invalid geometry was accepted");
        };
        const auto portable = out / L"portable-blender";
        const auto executable = portable / L"blender.exe";
        const auto suffix =
            std::filesystem::path(L"scripts/startup/bl_app_templates_system/Sculpting/startup.blend");
        const auto oldTemplate = portable / L"5.9" / suffix;
        const auto newTemplate = portable / L"5.10" / suffix;
        std::filesystem::create_directories(oldTemplate.parent_path());
        std::filesystem::create_directories(newTemplate.parent_path());
        const auto write = [&](const std::filesystem::path &path, const Monstey_Bytes &bytes) {
            if (std::filesystem::exists(path))
                std::filesystem::remove(path);
            Monstey_WriteNewFile(path, bytes);
        };
        write(executable, Monstey_Bytes{'N', 'O', 'T', '_', 'E', 'X', 'E'});
        write(oldTemplate, Monstey_ReadFile(input));
        write(newTemplate, Monstey_ReadFile(input));
        test("relocated_blender_template_resolution_is_native_and_numeric", [&] {
            Monstey_TestEnvironment envTemplate(L"MONSTEY_BLENDER_TEMPLATE"), envExe(L"MONSTEY_BLENDER");
            const auto options = J::object({{"blender", Monstey_Utf8(executable.wstring())}});
            const auto before = Monstey_HashSha256(executable);
            Monstey_Require(
                Monstey_BlendGeometryTemplate(options) == std::filesystem::absolute(newTemplate),
                "Relocated template resolution ignored configured installation or numeric version");
            Monstey_Blend relocated(Monstey_BlendGeometryTemplate(options));
            Monstey_Require(relocated.Describe()["blender_file_version"].as_uint() == 500 &&
                                Monstey_HashSha256(executable) == before,
                            "Relocated source was not read natively without executing the fake executable");
        });
        test("explicit_template_wins_environment_and_missing_executable", [&] {
            Monstey_TestEnvironment envTemplate(L"MONSTEY_BLENDER_TEMPLATE"), envExe(L"MONSTEY_BLENDER");
            SetEnvironmentVariableW(L"MONSTEY_BLENDER_TEMPLATE", (out / L"missing-template.blend").c_str());
            SetEnvironmentVariableW(L"MONSTEY_BLENDER", (out / L"missing-blender.exe").c_str());
            const auto resolved = Monstey_BlendGeometryTemplate(
                J::object({{"blend_template", Monstey_Utf8(input.wstring())},
                           {"blender", Monstey_Utf8((out / L"missing-configured.exe").wstring())}}));
            Monstey_Require(resolved == std::filesystem::absolute(input).lexically_normal(),
                            "Explicit template did not take precedence");
        });
        test("environment_template_override_and_executable_fallback_are_respected", [&] {
            Monstey_TestEnvironment envTemplate(L"MONSTEY_BLENDER_TEMPLATE"), envExe(L"MONSTEY_BLENDER");
            SetEnvironmentVariableW(L"MONSTEY_BLENDER_TEMPLATE", oldTemplate.c_str());
            auto configured =
                J::object({{"blender", Monstey_Utf8((out / L"missing-configured.exe").wstring())}});
            Monstey_Require(Monstey_BlendGeometryTemplate(configured) ==
                                std::filesystem::absolute(oldTemplate),
                            "Environment template override was ignored");
            SetEnvironmentVariableW(L"MONSTEY_BLENDER_TEMPLATE", nullptr);
            SetEnvironmentVariableW(L"MONSTEY_BLENDER", (out / L"missing-environment.exe").c_str());
            Monstey_Require(
                Monstey_BlendGeometryTemplate(J::object({{"blender", Monstey_Utf8(executable.wstring())}})) ==
                    std::filesystem::absolute(newTemplate),
                "Configured executable did not take precedence over environment executable");
            SetEnvironmentVariableW(L"MONSTEY_BLENDER", executable.c_str());
            for (const auto &empty : {J(nullptr), J("")})
                Monstey_Require(Monstey_BlendGeometryTemplate(
                                    J::object({{"blender", empty}, {"blend_template", empty}})) ==
                                    std::filesystem::absolute(newTemplate),
                                "Null/empty optional Blender paths did not use environment fallback");
            Monstey_Require(Monstey_BlendGeometryTemplate() == std::filesystem::absolute(newTemplate),
                            "Environment executable override was ignored");
        });
        test("missing_explicit_or_configured_template_never_uses_other_install", [&] {
            Monstey_TestEnvironment envTemplate(L"MONSTEY_BLENDER_TEMPLATE"), envExe(L"MONSTEY_BLENDER");
            SetEnvironmentVariableW(L"MONSTEY_BLENDER", executable.c_str());
            const auto noTemplateExe = out / L"blender-without-template.exe";
            write(noTemplateExe, Monstey_Bytes{1, 2, 3});
            for (const auto &options :
                 {J::object({{"blend_template", Monstey_Utf8((out / L"missing.blend").wstring())}}),
                  J::object({{"blender", Monstey_Utf8((out / L"missing.exe").wstring())}}),
                  J::object({{"blender", Monstey_Utf8(noTemplateExe.wstring())}})}) {
                bool precise = false;
                try {
                    (void)Monstey_BlendGeometryTemplate(options);
                } catch (const Monstey_Error &error) {
                    precise = error.code == "blender_template";
                }
                Monstey_Require(precise, "Missing explicit source fell back to another installation");
            }
        });
        auto part = Monstey_GeometryFixture();
        Monstey_Blend b(input);
        const auto initialArmatures = b.Blocks("AR")["rows"].size();
        const auto root = Monstey_BlendGeometryRootCollection(b);
        const auto collection = Monstey_BlendGeometryCollection(b, "Monstey_StaticWorld");
        const auto ci = Monstey_Index(collection);
        Monstey_BlendGeometryLinkCollection(b, root, collection);
        const auto mesh = Monstey_BlendGeometryCreateMesh(b, part, "World_Static_Geometry");
        const auto mi = Monstey_Index(mesh);
        const auto object =
            Monstey_BlendGeometryCreateObject(b, "World_Static_Geometry", 1, Monstey_Address(mesh));
        const auto oi = Monstey_Index(object);
        const auto empty = Monstey_BlendGeometryCreateObject(b, "World_Instance_Root", 0);
        b.SetField(oi, "parent", Monstey_Address(empty));
        b.SetField(Monstey_Index(empty), "loc", J::array({2.0, 3.0, 4.0}));
        Monstey_BlendGeometryLinkObjects(b, ci, {empty, object});
        const std::string metadata = "{\n  \"format\": \"Monstey_source_scene\",\n  \"objects\": [1]\n}\n";
        Monstey_BlendGeometryWriteText(b, "Monstey_source_scene.json", metadata);
        test("static_mesh_has_all_channels_no_armature_and_identity_object_basis", [&] {
            Monstey_Equal(part, Monstey_BlendGeometryReadMesh(b, mi, part));
            Monstey_Require(b.Blocks("AR")["rows"].size() == initialArmatures &&
                                b.ReadField(oi, "rot")[0u].as_number() == 0 &&
                                b.ReadField(oi, "size")[0u].as_number() == 1 &&
                                b.ReadField(oi, "modifiers.first")["pointer"].as_uint() == 0,
                            "Static geometry acquired a dummy rig/modifier or hidden basis transform");
        });
        test("typed_properties_and_serialized_lists_roundtrip_and_overwrite", [&] {
            Monstey_BlendGeometrySetProperty(b, oi, "source_id", "static:group0:draw13");
            Monstey_BlendGeometrySetProperty(b, oi, "instance_level", 80);
            Monstey_BlendGeometrySetProperty(b, oi, "ratio", 0.125);
            Monstey_BlendGeometrySetProperty(b, oi, "bindings", J::array({13, 24}));
            Monstey_BlendGeometrySetProperty(b, oi, "instance_level", 9000);
            const auto p = Monstey_BlendGeometryProperties(b, oi);
            Monstey_Require(p["source_id"].as_string() == "static:group0:draw13" &&
                                p["instance_level"].as_int() == 9000 && p["ratio"].as_number() == .125 &&
                                J::Parse(p["bindings"].as_string())[1u].as_int() == 24 && p.size() == 4,
                            "Scalar/JSON properties lost values or duplicated overwritten names");
            reject([&] { Monstey_BlendGeometrySetProperty(b, oi, "invalid", J(nullptr)); });
        });
        test("reachable_multiline_text_and_collection_linkage_are_readable", [&] {
            Monstey_Require(Monstey_BlendGeometryReadText(b, "Monstey_source_scene.json") == metadata,
                            "Reachable Text changed line boundaries");
            const auto before = b.ReadField(ci, "gobject.first")["pointer"].as_uint();
            Monstey_BlendGeometryLinkObjects(b, ci, {object});
            Monstey_Require(b.ReadField(ci, "gobject.first")["pointer"].as_uint() == before,
                            "Duplicate object link rewrote collection");
            reject([&] { Monstey_BlendGeometryLinkCollection(b, root, collection); });
            const auto found = Monstey_FindObject(b, "World_Static_Geometry");
            Monstey_Require(found["parent"].as_uint() == Monstey_Address(empty) &&
                                found["properties"]["instance_level"].as_int() == 9000,
                            "Object enumeration lost parent/properties");
        });
        test("invalid_geometry_rejects_before_allocating_blocks", [&] {
            const auto before = b.Describe()["blocks"].as_uint();
            auto invalid = part;
            invalid.indices[0] = 99;
            reject([&] { Monstey_BlendGeometryCreateMesh(b, invalid, "Invalid"); });
            invalid = part;
            invalid.channels["COLOR0"].values[0] = 2.5;
            reject([&] { Monstey_BlendGeometryCreateMesh(b, invalid, "Invalid"); });
            Monstey_Require(b.Describe()["blocks"].as_uint() == before,
                            "Invalid geometry mutated native block ownership");
        });
        test("point_channel_override_runs_once_before_corner_seam_projection", [&] {
            unsigned calls = 0;
            const auto read = Monstey_BlendGeometryReadMesh(b, mi, part, 1, [&](auto &pointPart) {
                ++calls;
                Monstey_Require(pointPart.channels.at("POSITION").values.size() == 12,
                                "Point callback did not receive original Blender vertex count");
                pointPart.channels["COLOR0"].values[0] = 123;
            });
            Monstey_Require(calls == 1 && read.channels.at("COLOR0").values[0] == 123,
                            "Point callback was ignored or its authored channel was overwritten");
        });
        auto banded = part;
        Monstey_ModelSetTriangleBands(banded, std::vector<std::uint8_t>{1, 16});
        test("qualified_triangle_bands_author_real_face_int_values_and_roundtrip", [&] {
            Monstey_Blend authored(input);
            const auto row = Monstey_BlendGeometryCreateMesh(authored, banded, "Banded");
            const auto meshBlock = Monstey_Index(row);
            const auto a = Monstey_BlendGeometryAttributes(authored, meshBlock).at("monstey_triangle_band");
            Monstey_Require(a.type == 3 && a.domain == 2 && a.count == 2 && a.data.size() == 8 &&
                                Monstey_U32(a.data, 0) == 2 && Monstey_U32(a.data, 4) == 17,
                            "Triangle bands were not stored as real FACE-domain INT band+1 values");
            Monstey_Equal(banded, Monstey_BlendGeometryReadMesh(authored, meshBlock, banded));
            Monstey_Equal(banded, Monstey_BlendGeometryReadMesh(authored, meshBlock, part));
            const auto authoredPath = out / L"geometry-triangle-bands.blend";
            if (std::filesystem::exists(authoredPath))
                std::filesystem::remove(authoredPath);
            authored.Write(authoredPath);
            Monstey_Blend saved(authoredPath);
            const auto rows = saved.Blocks("ME")["rows"];
            bool found = false;
            for (const auto &candidate : rows.as_array()) {
                const auto index = Monstey_Index(candidate);
                if (saved.ReadField(index, "id.name").as_string() != "MEBanded")
                    continue;
                Monstey_Equal(banded, Monstey_BlendGeometryReadMesh(saved, index, banded));
                found = true;
            }
            Monstey_Require(found, "Serialized real face band attribute was not reachable");
        });
        test("position_and_uv_seam_edits_preserve_face_bands_and_rebind_final_indices", [&] {
            Monstey_Blend authored(input);
            const auto row = Monstey_BlendGeometryCreateMesh(authored, banded, "BandSeam");
            const auto meshBlock = Monstey_Index(row);
            auto positions = banded.channels.at("POSITION").values;
            positions[0] = .25;
            Monstey_GeometryReplaceAttribute(authored, meshBlock, "position", Monstey_GeometryFloats(positions), 4);
            Monstey_GeometryReplaceAttribute(
                authored, meshBlock, "UVMap",
                Monstey_GeometryFloats({0, 0, 1, 0, 1, 1, .5, .5, 1, 1, 0, 1}), 6);
            const auto read = Monstey_BlendGeometryReadMesh(authored, meshBlock, banded);
            Monstey_Require(read.channels.at("POSITION").values[0] == .25 &&
                                read.channels.at("POSITION").values.size() == 15 && read.indices[3] == 4 &&
                                read.triangle_bands == banded.triangle_bands &&
                                read.triangle_band_topology_sha256 == Monstey_ModelTriangleTopologyHash(read.indices) &&
                                read.triangle_band_topology_sha256 != banded.triangle_band_topology_sha256,
                            "UV seam splitting changed face membership or left its old index fingerprint");
            for (const auto &[name, channel] : read.channels) {
                (void)name;
                Monstey_Require(channel.values.size() == channel.width * 5, "UV seam lost a point channel");
            }
        });
        test("face_reordering_deletion_and_subdivision_use_edited_face_band_order", [&] {
            auto reordered = banded;
            reordered.indices = {0, 2, 3, 0, 1, 2};
            Monstey_ModelSetTriangleBands(reordered, std::vector<std::uint8_t>{16, 1});
            auto deleted = banded;
            deleted.indices = {0, 2, 3};
            Monstey_ModelSetTriangleBands(deleted, std::vector<std::uint8_t>{16});
            auto subdivided = banded;
            for (auto &[name, channel] : subdivided.channels) {
                (void)name;
                const auto old = channel.values;
                for (unsigned component = 0; component < channel.width; ++component)
                    channel.values.push_back((old[component] + old[channel.width + component] +
                                              old[channel.width * 2 + component]) / 3);
            }
            // Preserve integral native color values for the new point.
            subdivided.channels["COLOR0"].values[16] = 44;
            subdivided.channels["COLOR0"].values[17] = 55;
            subdivided.channels["COLOR0"].values[18] = 66;
            subdivided.channels["COLOR0"].values[19] = 255;
            subdivided.indices = {0, 1, 4, 1, 2, 4, 2, 0, 4, 0, 2, 3};
            Monstey_ModelSetTriangleBands(subdivided, std::vector<std::uint8_t>{1, 1, 1, 16});
            for (const auto &edited : {reordered, deleted, subdivided}) {
                Monstey_Blend authored(input);
                const auto row = Monstey_BlendGeometryCreateMesh(authored, edited, "BandTopology");
                const auto read = Monstey_BlendGeometryReadMesh(authored, Monstey_Index(row), banded);
                Monstey_Require(read.indices == edited.indices && read.triangle_bands == edited.triangle_bands &&
                                    read.triangle_band_topology_sha256 ==
                                        Monstey_ModelTriangleTopologyHash(read.indices),
                                "Edited FACE-domain membership did not follow current face order/count");
            }
        });
        test("missing_attribute_only_inherits_unchanged_indices_or_exact_uniform_bands", [&] {
            Monstey_Blend legacy(input);
            const auto row = Monstey_BlendGeometryCreateMesh(legacy, part, "LegacyBands");
            const auto meshBlock = Monstey_Index(row);
            Monstey_Equal(banded, Monstey_BlendGeometryReadMesh(legacy, meshBlock, banded));
            Monstey_GeometryReplaceAttribute(legacy, meshBlock, ".corner_vert",
                                              Monstey_GeometryInts({0, 2, 3, 0, 1, 2}), 6);
            Monstey_GeometryRejectMessage(
                [&] { (void)Monstey_BlendGeometryReadMesh(legacy, meshBlock, banded); },
                "nonuniform native triangle bands require explicit face assignments");
            auto uniform = part;
            Monstey_ModelSetTriangleBands(uniform, std::vector<std::uint8_t>{4, 4});
            const auto read = Monstey_BlendGeometryReadMesh(legacy, meshBlock, uniform);
            Monstey_Require(read.triangle_bands == std::vector<std::uint8_t>({4, 4}) &&
                                read.triangle_band_topology_sha256 == Monstey_ModelTriangleTopologyHash(read.indices),
                            "Exact uniform legacy band could not be inherited after changed topology");
        });
        test("explicit_uniform_override_resolves_missing_stale_and_unassigned_face_metadata", [&] {
            Monstey_Blend legacy(input);
            const auto meshBlock = Monstey_Index(Monstey_BlendGeometryCreateMesh(legacy, part, "OverrideLegacy"));
            Monstey_GeometryReplaceAttribute(legacy, meshBlock, ".corner_vert",
                                              Monstey_GeometryInts({0, 2, 3, 0, 1, 2}), 6);
            auto stale = banded;
            stale.triangle_band_topology_sha256 = std::string(64, '0');
            const auto overridden = Monstey_BlendGeometryReadMesh(legacy, meshBlock, stale, 1, {}, std::uint8_t{6});
            Monstey_Require(overridden.triangle_bands == std::vector<std::uint8_t>({6, 6}) &&
                                overridden.triangle_band_topology_sha256 == Monstey_ModelTriangleTopologyHash(overridden.indices),
                            "Explicit uniform override did not reach the early missing/stale face gate");
            Monstey_Blend authored(input);
            const auto mi = Monstey_Index(Monstey_BlendGeometryCreateMesh(authored, banded, "OverrideUnassigned"));
            Monstey_GeometryReplaceAttribute(authored, mi, "monstey_triangle_band", Monstey_GeometryInts({0, 0}), 2);
            Monstey_Require(Monstey_BlendGeometryReadMesh(authored, mi, banded, 1, {}, std::uint8_t{16}).triangle_bands ==
                                std::vector<std::uint8_t>({16, 16}), "Explicit uniform override did not assign unassigned faces");
            Monstey_GeometryReplaceAttribute(authored, mi, "monstey_triangle_band", Monstey_GeometryInts({0, 18}), 2);
            reject([&] { (void)Monstey_BlendGeometryReadMesh(authored, mi, banded, 1, {}, std::uint8_t{1}); });
            reject([&] { (void)Monstey_BlendGeometryReadMesh(legacy, meshBlock, banded, 1, {}, std::uint8_t{17}); });
        });
        test("unassigned_invalid_type_domain_count_and_bytes_have_explicit_band_diagnostics", [&] {
            for (unsigned invalid = 0; invalid < 7; ++invalid) {
                Monstey_Blend authored(input);
                const auto row = Monstey_BlendGeometryCreateMesh(authored, banded, "InvalidBands");
                const auto meshBlock = Monstey_Index(row);
                const auto location = Monstey_GeometryAttribute(authored, meshBlock, "monstey_triangle_band");
                if (invalid < 2)
                    Monstey_GeometryReplaceAttribute(authored, meshBlock, "monstey_triangle_band",
                                                      Monstey_GeometryInts({2, invalid == 0 ? 0u : 18u}), 2);
                else if (invalid == 2)
                    authored.SetField(location.owner, "data_type", 0, location.ordinal);
                else if (invalid == 3)
                    authored.SetField(location.owner, "domain", 0, location.ordinal);
                else if (invalid == 4)
                    authored.SetField(location.array, "size", 1);
                else if (invalid == 5)
                    Monstey_GeometryReplaceAttribute(authored, meshBlock, "monstey_triangle_band",
                                                      Monstey_GeometryInts({2}), 2);
                else
                    Monstey_GeometryReplaceAttribute(authored, meshBlock, "monstey_triangle_band",
                                                      Monstey_GeometryInts({2, 17, 17}), 2);
                Monstey_GeometryRejectMessage(
                    [&] { (void)Monstey_BlendGeometryReadMesh(authored, meshBlock, banded); },
                    invalid < 2 ? "face 1 is unassigned or outside 1..17" : "requires FACE-domain INT");
            }
        });
        test("stale_or_malformed_qualified_bands_reject_before_mesh_allocation", [&] {
            Monstey_Blend authored(input);
            const auto before = authored.Describe()["blocks"].as_uint();
            for (unsigned invalid = 0; invalid < 3; ++invalid) {
                auto stale = banded;
                if (invalid == 0)
                    stale.indices = {0, 2, 3, 0, 1, 2};
                else if (invalid == 1)
                    stale.triangle_bands.pop_back();
                else
                    stale.triangle_bands[0] = 17;
                reject([&] { (void)Monstey_BlendGeometryCreateMesh(authored, stale, "StaleBands"); });
            }
            Monstey_Require(authored.Describe()["blocks"].as_uint() == before,
                            "Invalid bound bands allocated native mesh blocks");
        });
        test("real_single_value_color_and_face_attributes_expand_by_mesh_domain", [&] {
            Monstey_Blend authored(input);
            const auto meshBlock = Monstey_Index(Monstey_BlendGeometryCreateMesh(authored, banded, "SingleValues"));
            const auto single = [&](std::string_view name, const Monstey_Bytes &value) {
                const auto location = Monstey_GeometryAttribute(authored, meshBlock, name);
                const auto scalar = authored.NewBlock("DATA", "AttributeSingle");
                const auto bytes = authored.NewData(value);
                authored.SetDataOwner(Monstey_Index(scalar), meshBlock);
                authored.SetDataOwner(Monstey_Index(bytes), meshBlock);
                authored.SetField(Monstey_Index(scalar), "data", Monstey_Address(bytes));
                authored.SetField(location.owner, "storage_type", 1, location.ordinal);
                authored.SetField(location.owner, "data", Monstey_Address(scalar), location.ordinal);
            };
            single("_SOW_COLOR0", Monstey_Bytes{9, 8, 7, 255});
            single("monstey_triangle_band", Monstey_GeometryInts({3}));
            auto read = Monstey_BlendGeometryReadMesh(authored, meshBlock, banded);
            Monstey_Require(read.triangle_bands == std::vector<std::uint8_t>({2, 2}) &&
                                read.channels.at("COLOR0").values == std::vector<double>({9,8,7,255,9,8,7,255,9,8,7,255,9,8,7,255}),
                            "Native AttributeSingle values were not expanded to POINT/FACE counts");
            const auto file = out / L"single-attributes.blend";
            if (std::filesystem::exists(file))
                std::filesystem::remove(file);
            authored.Write(file);
            Monstey_Blend saved(file);
            Monstey_Equal(read, Monstey_BlendGeometryReadMesh(saved, meshBlock, banded));
        });
        test("single_boolean_values_expand_and_malformed_or_overflowing_storage_rejects", [&] {
            for (unsigned mode = 0; mode < 5; ++mode) {
                Monstey_Blend authored(input);
                const auto meshBlock = Monstey_Index(Monstey_BlendGeometryCreateMesh(authored, banded, "SingleLimits"));
                const auto location = Monstey_GeometryAttribute(authored, meshBlock, "_SOW_COLOR0");
                const auto scalar = authored.NewBlock("DATA", "AttributeSingle");
                const auto bytes = authored.NewData(mode == 0 ? Monstey_Bytes{0} : Monstey_Bytes{1,2,3});
                authored.SetField(Monstey_Index(scalar), "data", Monstey_Address(bytes));
                authored.SetField(location.owner, "data", Monstey_Address(scalar), location.ordinal);
                authored.SetField(location.owner, "storage_type", mode == 3 ? 2 : 1, location.ordinal);
                if (mode == 0) {
                    authored.SetField(location.owner, "data_type", 0, location.ordinal);
                    authored.SetField(location.owner, "domain", 3, location.ordinal);
                    const auto attributes = Monstey_BlendGeometryAttributes(authored, meshBlock);
                    const auto &value = attributes.at("_SOW_COLOR0");
                    Monstey_Require(value.count == 6 && value.data == Monstey_Bytes(6, 0),
                                    "Single BOOLEAN did not expand to corner-domain count");
                } else {
                    if (mode == 2) authored.SetField(meshBlock, "totvert", 2000000000);
                    if (mode == 4) authored.SetField(location.owner, "domain", 6, location.ordinal);
                    reject([&] { (void)Monstey_BlendGeometryAttributes(authored, meshBlock); });
                }
            }
        });
        test("generic_static_geometry_ignores_unbound_vectors_and_never_invents_native_bands", [&] {
            Monstey_Blend authored(input);
            auto generic = part;
            generic.triangle_bands = {16, 0};
            const auto row = Monstey_BlendGeometryCreateMesh(authored, generic, "GenericUnbound");
            const auto meshBlock = Monstey_Index(row);
            Monstey_Require(!Monstey_BlendGeometryAttributes(authored, meshBlock).contains("monstey_triangle_band"),
                            "Generic unqualified geometry acquired a native face attribute");
            Monstey_GeometryReplaceAttribute(authored, meshBlock, ".corner_vert",
                                              Monstey_GeometryInts({0, 2, 3, 0, 1, 2}), 6);
            const auto read = Monstey_BlendGeometryReadMesh(authored, meshBlock, generic);
            Monstey_Require(read.triangle_bands.empty() && read.triangle_band_topology_sha256.empty(),
                            "Generic changed topology silently retained an unbound source vector");
        });
        const auto path = out / L"geometry-authored.blend";
        if (std::filesystem::exists(path))
            std::filesystem::remove(path);
        b.Write(path);
        test("native_serialized_file_readback_preserves_channels_properties_and_text", [&] {
            Monstey_Blend saved(path);
            const auto ob = Monstey_FindObject(saved, "World_Static_Geometry");
            const auto meshBlock = saved.BlockAtAddress(ob["data"].as_uint());
            Monstey_Equal(part, Monstey_BlendGeometryReadMesh(saved, meshBlock, part));
            Monstey_Require(Monstey_BlendGeometryReadText(saved, "Monstey_source_scene.json") == metadata,
                            "Serialized Text lost metadata");
        });
        test("explicit_position_scale_has_no_implicit_axis_remap", [&] {
            Monstey_Blend scaled(input);
            const auto row = Monstey_BlendGeometryCreateMesh(scaled, part, "Scaled", .01);
            const auto read = Monstey_BlendGeometryReadMesh(scaled, Monstey_Index(row), part, 100);
            Monstey_Require(read.indices == part.indices && read.channels.size() == part.channels.size(),
                            "Explicit scale changed topology/channel inventory");
            for (std::size_t k = 0; k < part.channels.at("POSITION").values.size(); ++k)
                Monstey_Require(std::abs(read.channels.at("POSITION").values[k] -
                                         part.channels.at("POSITION").values[k]) < 1e-6,
                                "Explicit scaling changed axes or exceeded Float32 tolerance");
        });
        if (argc > 3)
            test("stock_blender_save_preserves_static_channels_properties_and_text", [&] {
                Monstey_Blend saved(argv[3]);
                const auto ob = Monstey_FindObject(saved, "World_Static_Geometry");
                Monstey_Equal(part, Monstey_BlendGeometryReadMesh(
                                        saved, saved.BlockAtAddress(ob["data"].as_uint()), part));
                Monstey_Require(ob["properties"]["instance_level"].as_int() == 9000 &&
                                    ob["properties"]["ratio"].as_number() == .125 &&
                                    Monstey_BlendGeometryReadText(saved, "Monstey_source_scene.json") ==
                                        metadata,
                                "Stock Blender dropped reachable native metadata");
            });
        if (argc > 4)
            test("stock_blender_edit_changes_geometry_uv_seams_and_instance_properties", [&] {
                Monstey_Blend saved(argv[4]);
                const auto ob = Monstey_FindObject(saved, "World_Static_Geometry");
                const auto edited =
                    Monstey_BlendGeometryReadMesh(saved, saved.BlockAtAddress(ob["data"].as_uint()), part);
                Monstey_Require(edited.channels.at("POSITION").values[0] == .25 &&
                                    edited.channels.at("POSITION").values.size() == 15 &&
                                    edited.indices[3] == 4 && edited.channels.at("UV0").values[8] == .125 &&
                                    ob["properties"]["instance_level"].as_int() == 9999,
                                "Stock mesh/UV/property edits did not reach native readback");
            });
        report["passed"] = static_cast<std::uint64_t>(report["cases"].size());
        report["authored_sha256"] = Monstey_HashSha256(path);
        const auto json = report.dump();
        const auto rp = out / L"blend-geometry-tests.json";
        if (std::filesystem::exists(rp))
            std::filesystem::remove(rp);
        Monstey_WriteNewFile(rp, std::span(reinterpret_cast<const std::uint8_t *>(json.data()), json.size()));
        std::cout << "Native Blender geometry tests: " << report["passed"].as_uint() << " passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}