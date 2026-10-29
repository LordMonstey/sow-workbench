#include "Monstey_Archive.hpp"
#include "Monstey_BlendGeometry.hpp"
#include "Monstey_WorldBlender.hpp"
#include "Monstey_WorldEdit.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <iostream>
using namespace Monstey;
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
unsigned passed{};
void Check(bool x, const char *m) {
    if (!x)
        throw std::runtime_error(m);
}
template <class F> void Case(const char *n, F f) {
    f();
    ++passed;
    std::cout << "PASS " << n << '\n';
}
template <class F> void Reject(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    Check(rejected, "expected rejection");
}
void U(B &b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(static_cast<std::uint8_t>(n >> (8 * i)));
}
void H(B &b, std::uint16_t n) {
    b.push_back(static_cast<std::uint8_t>(n));
    b.push_back(static_cast<std::uint8_t>(n >> 8));
}
void F(B &b, float f) { U(b, std::bit_cast<std::uint32_t>(f)); }
std::uint32_t S(B &b, const std::string &s) {
    auto p = static_cast<std::uint32_t>(b.size());
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
    return p;
}
std::string P(const std::filesystem::path &p) { return Monstey_Utf8(p.wstring()); }
J Desc(const std::filesystem::path &p, const B &b) {
    return J::object(
        {{"path", P(p)}, {"bytes", static_cast<std::uint64_t>(b.size())}, {"sha256", Monstey_HashSha256(b)}});
}
B Object() {
    B pool;
    auto schema = S(pool, "ObjectDB"), empty = S(pool, ""), record = S(pool, "ObjectRec");
    while (pool.size() % 4)
        pool.push_back(0);
    B db{'G', 'A', 'D', 'B'};
    for (auto n : {17u, static_cast<std::uint32_t>(pool.size()), 1u, 1u, 0u, 1u, 1u, 0u, 0u, 44u})
        U(db, n);
    H(db, 0);
    db.insert(db.end(), pool.begin(), pool.end());
    U(db, Monstey_NativeHash("Enabled"));
    U(db, Monstey_NativeHash("Enabled"));
    db.insert(db.end(), {0, 0, 1});
    H(db, 1);
    H(db, 0);
    for (auto n : {0u, 1u, schema, empty, empty, 0u, 1u, 0u, record, 0x00010001u, 0x08000000u, 1u, 0u})
        U(db, n);
    B classes, names;
    S(classes, "Light");
    S(names, "NativeLight");
    B obj{'O', 'B', 'J', 'L'};
    for (auto n : {5u, 1u, 1u, static_cast<std::uint32_t>(classes.size()),
                   static_cast<std::uint32_t>(names.size()), 0u})
        U(obj, n);
    obj.insert(obj.end(), classes.begin(), classes.end());
    obj.insert(obj.end(), names.begin(), names.end());
    for (auto n : {1u, 1u, 0u, 0u})
        U(obj, n);
    H(obj, 65535);
    U(obj, 0);
    for (auto f : {10.f, 20.f, 30.f, 0.f, 0.f, 0.f, 1.f})
        F(obj, f);
    U(obj, 0xdeadbeef);
    U(obj, 0xabcdef01);
    B out{'O', 'B', 'J', 'R'};
    U(out, 13);
    U(out, 0);
    U(out, 2);
    for (auto &e : {std::pair{1u, db}, std::pair{2u, obj}}) {
        U(out, e.first);
        U(out, static_cast<std::uint32_t>(e.second.size()));
        out.insert(out.end(), e.second.begin(), e.second.end());
    }
    return out;
}
B Instance() {
    B names;
    S(names, "prefabs/test.inst");
    while (names.size() % 8)
        names.push_back(0);
    B out;
    for (auto n : {0x42465250u, 8u, Monstey_PathHash("worlds/test.inst"), 1u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u,
                   1u, static_cast<std::uint32_t>(names.size())})
        U(out, n);
    B d(44);
    Monstey_PutU32(d, 0, 0x1234);
    Monstey_PutU32(d, 16, 0xffffffffu);
    Monstey_PutU32(d, 36, 1);
    out.insert(out.end(), d.begin(), d.end());
    for (unsigned i = 0; i < 5; ++i)
        U(out, 0);
    U(out, 0);
    U(out, 0x5678);
    U(out, 0);
    F(out, 1000);
    for (auto f : {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f})
        F(out, f);
    U(out, 0);
    U(out, 0);
    out.insert(out.end(), names.begin(), names.end());
    return out;
}
B Mesh() {
    B out{'M', 'E', 'S', 'H'};
    for (auto n : {5u, 1u, 1u, 1u, 1u, 44u, 0u, 1u, 36u, 6u})
        U(out, n);
    for (auto f : {0.f, 0.f, 0.f, 100.f, 0.f, 0.f, 0.f, 100.f, -100.f})
        F(out, f);
    for (auto n : {0, 1, 2})
        H(out, static_cast<std::uint16_t>(n));
    U(out, 1);
    U(out, 1);
    out.insert(out.end(), {0, 2, 0, 0});
    U(out, 1);
    std::array<std::uint32_t, 13> w{};
    w[1] = 3;
    w[2] = 3;
    w[5] = 1;
    w[10] = 36;
    for (auto n : w)
        U(out, n);
    return out;
}
std::size_t VisiblePngPixels(const B &png) {
    auto be = [&](std::size_t at) {
        Check(at + 4 <= png.size(), "PNG word truncated");
        return (std::uint32_t(png[at]) << 24) | (std::uint32_t(png[at + 1]) << 16) |
               (std::uint32_t(png[at + 2]) << 8) | png[at + 3];
    };
    auto width = be(16), height = be(20);
    Check(width == 1300 && height == 1000 && png[24] == 8 && png[25] == 6 && png[28] == 0,
          "unexpected reference PNG layout");
    B compressed;
    for (std::size_t at = 8; at < png.size();) {
        auto n = be(at);
        Check(n <= png.size() - at - 12, "PNG chunk extent truncated");
        if (std::string(png.begin() + static_cast<std::ptrdiff_t>(at + 4),
                        png.begin() + static_cast<std::ptrdiff_t>(at + 8)) == "IDAT")
            compressed.insert(compressed.end(), png.begin() + static_cast<std::ptrdiff_t>(at + 8),
                              png.begin() + static_cast<std::ptrdiff_t>(at + 8 + n));
        at += 12ull + n;
    }
    Check(compressed.size() > 6, "PNG IDAT missing");
    auto stride = std::size_t(width) * 4;
    auto filtered = Monstey_InflateRaw(
        std::span<const std::uint8_t>(compressed).subspan(2, compressed.size() - 6), (stride + 1) * height);
    B pixels(stride * height);
    std::size_t visible = 0;
    auto paeth = [](int a, int b, int c) {
        auto p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
        return pa <= pb && pa <= pc ? a : (pb <= pc ? b : c);
    };
    for (std::size_t y = 0; y < height; ++y) {
        auto type = filtered[y * (stride + 1)];
        Check(type <= 4, "PNG filter unknown");
        for (std::size_t x = 0; x < stride; ++x) {
            auto left = x >= 4 ? pixels[y * stride + x - 4] : 0, up = y ? pixels[(y - 1) * stride + x] : 0,
                 corner = y && x >= 4 ? pixels[(y - 1) * stride + x - 4] : 0;
            int predictor = type == 0   ? 0
                            : type == 1 ? left
                            : type == 2 ? up
                            : type == 3 ? (left + up) / 2
                                        : paeth(left, up, corner);
            pixels[y * stride + x] =
                static_cast<std::uint8_t>(filtered[y * (stride + 1) + 1 + x] + predictor);
        }
        for (std::size_t x = 0; x < width; ++x)
            if (unsigned(pixels[y * stride + x * 4]) + pixels[y * stride + x * 4 + 1] +
                        pixels[y * stride + x * 4 + 2] <
                    150 ||
                unsigned(pixels[y * stride + x * 4]) + pixels[y * stride + x * 4 + 1] +
                        pixels[y * stride + x * 4 + 2] >
                    384)
                ++visible;
    }
    return visible;
}
} // namespace
int main(int argc, char **argv) {
    try {
        if(argc==4&&std::string_view(argv[1])=="--export") {
            auto result=Monstey_WorldBlenderCall("sow_blender_export",J::object({{"blend",argv[2]},{"output_dir",argv[3]}}));
            Check(result.at("sources").empty(),"stock saved native scene produced unexpected edits");
            std::cout<<result.dump(2)<<"\nPASS stock Blender world save and native readback\n";return 0;
        }
        auto base =
            argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1])) : std::filesystem::temp_directory_path();
        std::filesystem::create_directories(Monstey_WindowsFilePath(base));
        auto dir = base / ("Monstey_CppWorldBlenderTests_" +
                           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(Monstey_WindowsFilePath(dir));
        auto put = [&](const char *name, const B &raw) {
            auto p = dir / name;
            Monstey_WriteNewFile(p, raw);
            return p;
        };
        auto json = [&](const char *name, const J &value) {
            auto p = dir / name;
            Monstey_WriteNewText(p, value.dump(2) + "\n");
            return p;
        };
        auto object = put("native.objres", Object()), instance = put("native.inst", Instance()),
             mesh = put("native.wldmsh", Mesh());
        auto spec = J::object({{"title", "Native synthetic fortress"},
                               {"origin_native_cm", J::array({0, 0, 0})},
                               {"sources", J::array({J::object({{"id", "logic"},
                                                                {"source", P(object)},
                                                                {"resource_path", "worlds/test.objres"},
                                                                {"frame", "world"}}),
                                                     J::object({{"id", "geometry"},
                                                                {"source", P(mesh)},
                                                                {"resource_path", "worlds/test.wldmsh"},
                                                                {"frame", "world"}})})}});
        auto scene_path = dir / "prepared/scene.json";
        Case("native region preparation accepts generic source selection", [&] {
            auto r = Monstey_WorldBlenderCall(
                "sow_blender_prepare",
                J::object({{"region", "snowmtn"}, {"spec", spec}, {"output", P(dir / "prepared")}}));
            auto scene = J::Parse(Monstey_ReadText(scene_path));
            Check(r.at("objects").as_uint() == 1 && scene.at("geometry").size() == 1 &&
                      scene.at("sources").size() == 2,
                  "native scene omitted objects or real geometry");
            Check(Monstey_ReadFile(object) == Object() && Monstey_ReadFile(mesh) == Mesh(),
                  "prepare modified original");
        });
        auto attached = dir / "attached.json";
        Case("attach preserves native frame and original scene", [&] {
            auto before = Monstey_HashSha256(scene_path);
            auto r = Monstey_WorldBlenderCall(
                "sow_blender_attach_inst",
                J::object({{"scene", P(scene_path)},
                           {"source", P(instance)},
                           {"resource_path", "worlds/test.inst"},
                           {"frame", "local"},
                           {"frame_to_scene", J::object({{"position", J::array({100, 200, 300})},
                                                         {"quaternion", J::array({0, 0, 0, 1})},
                                                         {"scale", 1}})},
                           {"output", P(attached)}}));
            auto scene = J::Parse(Monstey_ReadText(attached));
            Check(r.at("instances").as_uint() == 1 && scene.at("objects").size() == 2 &&
                      Monstey_HashSha256(scene_path) == before,
                  "attach lost instance or modified source");
            auto &m = scene.at("objects")[1].at("scene_matrix_native");
            Check(m[0][3].as_number() == 100 && m[1][3].as_number() == 200 && m[2][3].as_number() == 300,
                  "local frame was not composed");
        });
        Case("duplicate attach rejects without a partial output", [&] {
            Reject([&] {
                Monstey_WorldBlenderCall("sow_blender_attach_inst",
                                         J::object({{"scene", P(attached)},
                                                    {"source", P(instance)},
                                                    {"resource_path", "worlds/test.inst"},
                                                    {"frame", "world"},
                                                    {"output", P(dir / "duplicate.json")}}));
            });
            Check(!std::filesystem::exists(dir / "duplicate.json"), "failed attach published output");
        });
        Case("native validation writes no candidate database", [&] {
            auto plan = J::object(
                {{"schema", "Monstey_WorldBlenderEdits_2"},
                 {"sources",
                  J::array({J::object(
                      {{"backend", "OBJRES"},
                       {"source", Desc(object, Object())},
                       {"resource_path", "worlds/test.objres"},
                       {"spec", J::object({{"updates", J::array({J::object(
                                                           {{"name", "NativeLight"},
                                                            {"position", J::array({1, 2, 3})},
                                                            {"properties",
                                                             J::object({{"Enabled", false}})}})})}})}})})}});
            auto path = json("validation.json", plan);
            auto r = Monstey_WorldBlenderCall("sow_blender_validate", J::object({{"spec", P(path)}}));
            Check(r.at("status").as_string() == "PASS" && r.at("results").size() == 1 &&
                      !r.at("binary_written").as_bool(),
                  "native validation failed");
        });
        Case("stale native sources reject before creating a Blender component", [&] {
            auto scene = J::Parse(Monstey_ReadText(scene_path));
            scene["sources"]["logic"]["fixture"]["sha256"] = "wrong";
            auto stale = json("stale.json", scene);
            Reject([&] {
                Monstey_WorldBlenderCall("sow_blender_build",
                                         J::object({{"scene", P(stale)}, {"output_dir", P(dir / "stale")}}));
            });
            Check(!std::filesystem::exists(dir / "stale"), "stale scene published component");
        });
        if (argc > 2) {
            auto template_path = std::filesystem::path(Monstey_Wide(argv[2]));
            auto build_args = J::object({{"scene", P(attached)},
                                         {"blend_template", P(template_path)},
                                         {"output_dir", P(dir / "blender")}});
            auto native_blend = dir / "blender/WorldEditor.blend";
            Case("real static SDNA world authoring has no rig or modifiers", [&] {
                auto r = Monstey_WorldBlenderCall("sow_blender_build", build_args);
                Check(r.at("objects").as_uint() == 2 && r.at("native_geometry_objects").as_uint() == 1,
                      "wrong genuine scene counts");
                Monstey_Blend b(native_blend);
                auto objects = Monstey_BlendGeometryObjects(b);
                unsigned geometry = 0;
                for (auto &ob : objects.as_array())
                    if (ob.at("properties").contains("Monstey_kind")) {
                        Check(ob.at("type").as_int() != 25, "world scene has a dummy armature");
                        if (ob.at("properties").at("Monstey_kind").as_string() ==
                            "NATIVE_GEOMETRY_REFERENCE") {
                            ++geometry;
                            Check(b.ReadField(static_cast<std::size_t>(ob.at("block").as_uint()),
                                              "modifiers.first")
                                          .at("pointer")
                                          .as_uint() == 0,
                                  "static world mesh has an armature modifier");
                        }
                    }
                Check(geometry == 1, "genuine geometry object absent");
            });
            Case("unchanged saved native world emits an empty edit plan", [&] {
                auto r = Monstey_WorldBlenderCall(
                    "sow_blender_export",
                    J::object({{"blend", P(native_blend)}, {"output_dir", P(dir / "noop")}}));
                Check(r.at("sources").empty() && r.at("counts").at("deletes").as_uint() == 0 && r.at("warnings").empty(),
                      "unchanged scene changed native data");
            });
            auto changed_blend = dir / "changed.blend";
            Case("native Blender transform and property edits compose through original codecs", [&] {
                Monstey_Blend b(native_blend);
                auto objects = Monstey_BlendGeometryObjects(b);
                for (auto &ob : objects.as_array()) {
                    auto &properties = ob.at("properties");
                    if (properties.value("Monstey_kind", "").as_string() == "OBJRES") {
                        auto block = static_cast<std::size_t>(ob.at("block").as_uint());
                        b.SetField(block, "loc", J::array({2, 3, 4}));
                        auto values = J::Parse(properties.at("Monstey_properties_json").as_string());
                        values[Monstey_Hex(Monstey_NativeHash("Enabled"))] = false;
                        Monstey_BlendGeometrySetProperty(b, block, "Monstey_properties_json", values.dump());
                    }
                }
                b.Write(changed_blend);
                auto r = Monstey_WorldBlenderCall(
                    "sow_blender_export",
                    J::object({{"blend", P(changed_blend)}, {"output_dir", P(dir / "edited")}}));
                Check(r.at("counts").at("updates").as_uint() == 1 && r.at("sources").size() == 1,
                      "edited scene lost transform or native properties");
                auto &entry = r.at("sources")[0].at("spec").at("updates")[0];
                Check(entry.at("position") == J::array({200.0, 400.0, -300.0}) &&
                          entry.at("properties").at(Monstey_Hex(Monstey_NativeHash("Enabled"))) == J(false),
                      "inverse coordinate mapping or property values differ");
                auto compiled = Monstey_WorldEditCall("sow_worldedit_compile", J::object({{"plan", r}}));
                Check(compiled.at("operations").size() == 1, "export does not use native writer");
            });
            Case("modified native geometry rejects without publishing an edit plan", [&] {
                Monstey_Blend b(native_blend);
                auto objects = Monstey_BlendGeometryObjects(b);
                for (auto &ob : objects.as_array())
                    if (ob.at("properties").value("Monstey_kind", "").as_string() ==
                        "NATIVE_GEOMETRY_REFERENCE") {
                        auto oi = static_cast<std::size_t>(ob.at("block").as_uint());
                        b.SetField(oi, "loc", J::array({1, 0, 0}));
                    }
                auto wrong = dir / "wrong-reference.blend";
                b.Write(wrong);
                Reject([&] {
                    Monstey_WorldBlenderCall(
                        "sow_blender_export",
                        J::object({{"blend", P(wrong)}, {"output_dir", P(dir / "rejected-reference")}}));
                });
                Check(!std::filesystem::exists(dir / "rejected-reference"),
                      "invalid native reference created output");
            });
            Case("existing Blender output and all source files are immutable", [&] {
                auto before = Monstey_HashSha256(native_blend);
                Reject([&] { Monstey_WorldBlenderCall("sow_blender_build", build_args); });
                Check(Monstey_HashSha256(native_blend) == before && Monstey_ReadFile(object) == Object() &&
                          Monstey_ReadFile(instance) == Instance() && Monstey_ReadFile(mesh) == Mesh(),
                      "source or existing blend changed");
            });
            Case("explicit native clone and addition remain independent objects", [&] {
                Monstey_Blend b(native_blend);
                auto objects = Monstey_BlendGeometryObjects(b);
                J baseline;
                for (auto &ob : objects.as_array())
                    if (ob.at("properties").value("Monstey_kind", "").as_string() == "OBJRES")
                        baseline = ob;
                auto clone = Monstey_BlendGeometryCreateObject(b, "Native clone", 0);
                auto ci = static_cast<std::size_t>(clone.at("block").as_uint());
                auto props = baseline.at("properties");
                props["Monstey_action"] = "clone";
                props["Monstey_clone_source_id"] = props.at("Monstey_id");
                props["Monstey_native_name"] = "ClonedLight";
                for (auto &[key, value] : props.as_object())
                    Monstey_BlendGeometrySetProperty(b, ci, key, value);
                b.SetField(ci, "loc", J::array({1, 2, 3}));
                auto added = Monstey_BlendGeometryCreateObject(b, "Native addition", 0);
                auto ai = static_cast<std::size_t>(added.at("block").as_uint());
                auto added_props =
                    J::object({{"Monstey_action", "add"},
                               {"Monstey_kind", "OBJRES"},
                               {"Monstey_source_id", "logic"},
                               {"Monstey_native_name", "AddedLight"},
                               {"Monstey_class_name", "Light"},
                               {"Monstey_properties_json", J::object({{"Enabled", false}}).dump()}});
                for (auto &[key, value] : added_props.as_object())
                    Monstey_BlendGeometrySetProperty(b, ai, key, value);
                Monstey_BlendGeometryLinkObjects(b, Monstey_BlendGeometryRootCollection(b), {clone, added});
                auto file = dir / "cloned-added.blend";
                b.Write(file);
                auto plan = Monstey_WorldBlenderCall(
                    "sow_blender_export",
                    J::object({{"blend", P(file)}, {"output_dir", P(dir / "cloned-added")}}));
                Check(plan.at("counts").at("clones").as_uint() == 1 &&
                          plan.at("counts").at("adds").as_uint() == 1 && plan.at("sources").size() == 1,
                      "native clone/add semantics lost");
            });
            Case("deleted native object emits a real native deletion", [&] {
                auto logic_spec = spec;
                logic_spec["sources"] = J::array({spec.at("sources")[0]});
                Monstey_WorldBlenderCall("sow_blender_prepare",
                                         J::object({{"region", "snowmtn"},
                                                    {"spec", logic_spec},
                                                    {"output", P(dir / "logic-prepared")}}));
                auto original = dir / "logic-prepared/scene.json";
                auto logical = J::Parse(Monstey_ReadText(original));
                Monstey_Blend b(template_path);
                auto root = Monstey_BlendGeometryRootCollection(b);
                Monstey_BlendGeometryLinkObjects(b, root, {}, true);
                b.SetField(root, "children.first", 0);
                b.SetField(root, "children.last", 0);
                Monstey_BlendGeometryWriteText(b, "Monstey_source_scene.json", logical.dump());
                Monstey_BlendGeometryWriteText(
                    b, "Monstey_world_metadata.json",
                    J::object({{"scene_path", P(original)}, {"scene_sha256", Monstey_HashSha256(original)}})
                        .dump());
                auto file = dir / "deleted.blend";
                b.Write(file);
                auto plan = Monstey_WorldBlenderCall(
                    "sow_blender_export",
                    J::object({{"blend", P(file)}, {"output_dir", P(dir / "deleted")}}));
                Check(plan.at("counts").at("deletes").as_uint() == 1 &&
                          plan.at("sources")[0].at("spec").at("deletes")[0].at("name").as_string() ==
                              "NativeLight",
                      "native deletion lost its stable selector");
            });
            Case("original v1 scene and properties roundtrip through the native adapter", [&] {
                auto prepared = J::Parse(Monstey_ReadText(scene_path));
                auto legacy =
                    J::object({{"schema", "Monstey_WorldBlenderScene_1"},
                               {"origin_native_cm", J::array({0, 0, 0})},
                               {"sources", J::object({{"logic", prepared.at("sources").at("logic")}})},
                               {"objects", J::array({prepared.at("objects")[0]})},
                               {"bounds", J::array()},
                               {"meshes", J::array()}});
                auto file = json("legacy-scene.json", legacy);
                Monstey_WorldBlenderCall("sow_blender_build",
                                         J::object({{"scene", P(file)},
                                                    {"blend_template", P(template_path)},
                                                    {"output_dir", P(dir / "legacy-blend")}}));
                auto plan = Monstey_WorldBlenderCall(
                    "sow_blender_export", J::object({{"blend", P(dir / "legacy-blend/WorldEditor.blend")},
                                                     {"output_dir", P(dir / "legacy-export")}}));
                Check(plan.at("sources").empty(), "legacy no-op scene changed original data");
            });
            if (argc > 5)
                Case("actual SoW object and indexed terrain preserve source data through native Blender",
                     [&] {
                         auto repo = std::filesystem::path(Monstey_Wide(argv[4])),
                              terrain = std::filesystem::path(Monstey_Wide(argv[5]));
                         auto native_object = repo / "sow-port/world-objects/fixtures-v001/source/worlds/"
                                                     "retail/snowmtn/snowmtn.objres";
                         auto before_object = Monstey_HashSha256(native_object),
                              before_terrain = Monstey_HashSha256(terrain);
                         auto native_spec = J::object(
                             {{"sources",
                               J::array(
                                   {J::object(
                                        {{"id", "native"},
                                         {"source", P(native_object)},
                                         {"resource_path", "worlds/retail/snowmtn/snowmtn.objres"},
                                         {"frame", "world"},
                                         {"select", J::object({{"class_names", J::array({"TeleportPoint"})},
                                                               {"limit", 1}})}}),
                                    J::object({{"id", "terrain"},
                                               {"source", P(terrain)},
                                               {"resource_path", "worlds/retail/snowmtn/snowmtn.wldmsh"},
                                               {"frame", "world"},
                                               {"select", J::object({{"indices", J::array({13})}})}})})}});
                         Monstey_WorldBlenderCall("sow_blender_prepare",
                                                  J::object({{"region", "snowmtn"},
                                                             {"spec", native_spec},
                                                             {"output", P(dir / "actual-prepared")}}));
                         auto scene = dir / "actual-prepared/scene.json";
                         auto result = Monstey_WorldBlenderCall(
                             "sow_blender_build", J::object({{"scene", P(scene)},
                                                             {"blend_template", P(template_path)},
                                                             {"output_dir", P(dir / "actual-blend")}}));
                         Check(result.at("native_geometry_objects").as_uint() == 1,
                               "actual native terrain reference was lost");
                         auto plan = Monstey_WorldBlenderCall(
                             "sow_blender_export",
                             J::object({{"blend", P(dir / "actual-blend/WorldEditor.blend")},
                                        {"output_dir", P(dir / "actual-export")}}));
                         Check(plan.at("sources").empty() &&
                                   before_object == Monstey_HashSha256(native_object) &&
                                   before_terrain == Monstey_HashSha256(terrain),
                               "actual source changed or no-op became an edit");
                     });
            if (argc > 6)
                Case("installed native region prepares real keep triangles and source-qualified placements",
                     [&] {
                         auto result = Monstey_WorldBlenderCall(
                             "sow_blender_prepare", J::object({{"region", "snowmtn"},
                                                               {"game_root", argv[6]},
                                                               {"output", P(dir / "game-prepared")}}));
                         auto scene = J::Parse(Monstey_ReadText(dir / "game-prepared/scene.json"));
                         Check(result.at("objects").as_uint() > 0 && scene.at("sources").size() == 5 &&
                                   scene.at("geometry").size() == 1 &&
                                   scene.at("geometry")[0].at("triangles").as_uint() > 100,
                               "installed region omitted qualified keep geometry");
                         auto built = Monstey_WorldBlenderCall(
                             "sow_blender_build", J::object({{"scene", P(dir / "game-prepared/scene.json")},
                                                             {"blend_template", P(template_path)},
                                                             {"output_dir", P(dir / "game-blend")}}));
                         Check(built.at("native_geometry_objects").as_uint() == 1,
                               "native keep Blender display was not authored");
                         auto plan = Monstey_WorldBlenderCall(
                             "sow_blender_export",
                             J::object({{"blend", P(dir / "game-blend/WorldEditor.blend")},
                                        {"output_dir", P(dir / "game-export")}}));
                         Check(plan.at("sources").empty(), "unchanged native keep scene became an edit");
                     });
            if (argc > 3)
                Case("native Blender process renders both reference cameras without Python", [&] {
                    auto a = build_args;
                    a["output_dir"] = P(dir / "rendered");
                    a["render"] = true;
                    a["blender"] = argv[3];
                    auto result = Monstey_WorldBlenderCall("sow_blender_build", a);
                    Check(result.at("reference_renders").size() == 2, "both cameras were not rendered");
                    for (auto &row : result.at("reference_renders").as_array()) {
                        auto raw =
                            Monstey_ReadFile(std::filesystem::path(Monstey_Wide(row.at("path").as_string())));
                        Check(raw.size() > 1000 && raw[0] == 0x89 && raw[1] == 'P' &&
                                  VisiblePngPixels(raw) > 100,
                              "render is invalid or contains no visible foreground geometry");
                    }
                });
        }
        std::cout << "RESULT " << passed << " world Blender groups passed; outputs " << dir.string() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}