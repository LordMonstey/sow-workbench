#include "Monstey_WorldBlender.hpp"
#include "Monstey_Archive.hpp"
#include "Monstey_BlendGeometry.hpp"
#include "Monstey_InstGeometry.hpp"
#include "Monstey_World.hpp"
#include "Monstey_WorldAssets.hpp"
#include "Monstey_WorldEdit.hpp"
#include "Monstey_WorldGeometry.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <windows.h>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
using M = std::array<std::array<double, 4>, 4>;
void Monstey_BWKeys(const J &value, std::initializer_list<const char *> keys) {
    Monstey_Require(value.is_object(), "World Blender arguments must be an object");
    for (auto &[key, item] : value.as_object()) {
        (void)item;
        Monstey_Require(std::any_of(keys.begin(), keys.end(), [&](auto name) { return name == key; }),
                        "Unknown World Blender argument: " + key);
    }
}
void Monstey_BWSet(Monstey_Blend &blend, std::size_t block, std::string_view field, const J &value) {
    try {
        (void)blend.SetField(block, field, value);
    } catch (const Monstey_Error &e) {
        throw Monstey_Error(e.code, std::string(e.what()) + " (world field " + std::string(field) + ")");
    }
}
std::filesystem::path Monstey_BWPath(const J &j) {
    return std::filesystem::absolute(std::filesystem::path(Monstey_Wide(j.as_string())));
}
std::string Monstey_BWText(const std::filesystem::path &p) { return Monstey_Utf8(p.wstring()); }
J Monstey_BWRecord(const std::filesystem::path &p, const B &raw) {
    return J::object({{"path", Monstey_BWText(p)},
                      {"bytes", static_cast<std::uint64_t>(raw.size())},
                      {"sha256", Monstey_HashSha256(raw)}});
}
M Monstey_BWIdentity() {
    M m{};
    for (unsigned i = 0; i < 4; ++i)
        m[i][i] = 1;
    return m;
}
M Monstey_BWMatrix(const J &j) {
    Monstey_Require(j.is_array() && j.size() == 4, "World matrix needs four rows");
    M m{};
    for (unsigned i = 0; i < 4; ++i) {
        Monstey_Require(j[i].is_array() && j[i].size() == 4, "World matrix needs four columns");
        for (unsigned k = 0; k < 4; ++k) {
            m[i][k] = j[i][k].as_number();
            Monstey_Require(std::isfinite(m[i][k]), "World matrix contains a nonfinite value");
        }
    }
    for (unsigned i = 0; i < 4; ++i)
        Monstey_Require(std::abs(m[3][i] - (i == 3 ? 1.0 : 0.0)) < 1e-7,
                        "World matrix has a nonaffine final row");
    return m;
}
J Monstey_BWJson(const M &m) {
    J::Array rows;
    for (auto &row : m)
        rows.push_back(J::array({row[0], row[1], row[2], row[3]}));
    return rows;
}
M Monstey_BWMul(const M &a, const M &b) {
    M m{};
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < 4; ++j)
            for (unsigned k = 0; k < 4; ++k)
                m[i][j] += a[i][k] * b[k][j];
    return m;
}
M Monstey_BWInverse(M a) {
    auto out = Monstey_BWIdentity();
    for (unsigned col = 0; col < 4; ++col) {
        auto pivot = col;
        for (unsigned row = col + 1; row < 4; ++row)
            if (std::abs(a[row][col]) > std::abs(a[pivot][col]))
                pivot = row;
        Monstey_Require(std::abs(a[pivot][col]) > 1e-14, "World source frame is singular");
        std::swap(a[pivot], a[col]);
        std::swap(out[pivot], out[col]);
        auto divisor = a[col][col];
        for (unsigned j = 0; j < 4; ++j) {
            a[col][j] /= divisor;
            out[col][j] /= divisor;
        }
        for (unsigned row = 0; row < 4; ++row)
            if (row != col) {
                auto factor = a[row][col];
                for (unsigned j = 0; j < 4; ++j) {
                    a[row][j] -= factor * a[col][j];
                    out[row][j] -= factor * out[col][j];
                }
            }
    }
    return out;
}
M Monstey_BWCompose(const J &position, const J &quaternion, double scale = 1) {
    Monstey_Require(position.is_array() && position.size() == 3 && quaternion.is_array() &&
                        quaternion.size() == 4 && std::isfinite(scale) && scale > 0,
                    "World transform requires position, XYZW quaternion and positive scale");
    std::array<double, 4> q{};
    double norm = 0;
    for (unsigned i = 0; i < 4; ++i) {
        q[i] = quaternion[i].as_number();
        Monstey_Require(std::isfinite(q[i]), "Nonfinite world quaternion");
        norm += q[i] * q[i];
    }
    Monstey_Require(norm > 1e-16, "Zero world quaternion");
    for (auto &v : q)
        v /= std::sqrt(norm);
    auto [x, y, z, w] = q;
    M m = Monstey_BWIdentity();
    m[0][0] = 1 - 2 * (y * y + z * z);
    m[0][1] = 2 * (x * y - z * w);
    m[0][2] = 2 * (x * z + y * w);
    m[1][0] = 2 * (x * y + z * w);
    m[1][1] = 1 - 2 * (x * x + z * z);
    m[1][2] = 2 * (y * z - x * w);
    m[2][0] = 2 * (x * z - y * w);
    m[2][1] = 2 * (y * z + x * w);
    m[2][2] = 1 - 2 * (x * x + y * y);
    for (unsigned i = 0; i < 3; ++i) {
        for (unsigned k = 0; k < 3; ++k)
            m[i][k] *= scale;
        m[i][3] = position[i].as_number();
        Monstey_Require(std::isfinite(m[i][3]), "Nonfinite world position");
    }
    return m;
}
M Monstey_BWBasis() {
    auto m = Monstey_BWIdentity();
    m[1][1] = m[2][2] = 0;
    m[1][2] = -1;
    m[2][1] = 1;
    return m;
}
M Monstey_BWDisplay(const M &native, const J &origin) {
    Monstey_Require(origin.is_array() && origin.size() == 3, "Scene needs native origin");
    auto shifted = native;
    for (unsigned i = 0; i < 3; ++i)
        shifted[i][3] = (shifted[i][3] - origin[i].as_number()) / 100;
    return Monstey_BWMul(Monstey_BWMul(Monstey_BWBasis(), shifted), Monstey_BWInverse(Monstey_BWBasis()));
}
struct Monstey_BWInputs {
    std::map<std::filesystem::path, std::string> hashes;
    B Load(const std::filesystem::path &path) {
        auto raw = Monstey_ReadFile(path);
        auto hash = Monstey_HashSha256(raw);
        auto [it, inserted] = hashes.emplace(path, hash);
        Monstey_Require(inserted || it->second == hash, "World Blender source changed");
        return raw;
    }
    J Json(const std::filesystem::path &path) {
        auto raw = Load(path);
        return J::Parse(std::string(raw.begin(), raw.end()));
    }
    B Verified(const J &record) {
        auto path = Monstey_BWPath(record.at("path"));
        auto raw = Load(path);
        Monstey_Require(Monstey_HashSha256(raw) == record.at("sha256").as_string(),
                        "World Blender source checksum differs");
        if (record.contains("bytes"))
            Monstey_Require(raw.size() == record.at("bytes").as_uint(), "World Blender source size differs");
        return raw;
    }
    void Scene(const J &scene) {
        for (auto &[id, source] : scene.at("sources").as_object()) {
            (void)id;
            Verified(source.at("fixture"));
        }
        auto geometry = scene.value("geometry", J::array());
        for (auto &row : geometry.as_array())
            Verified(row.at("source"));
    }
    void Verify() const {
        for (auto &[path, hash] : hashes)
            Monstey_Require(Monstey_HashSha256(path) == hash,
                            "World Blender input changed before publication");
    }
    void Output(const std::filesystem::path &path) const {
        std::vector<std::filesystem::path> sources;
        for (auto &[source, hash] : hashes) {
            (void)hash;
            sources.push_back(source);
        }
        Monstey_RequireOfflineOutput(path, sources);
        Monstey_Require(!std::filesystem::exists(Monstey_WindowsFilePath(path)),
                        "World Blender output already exists");
    }
};
J Monstey_BWNormalize(J scene, Monstey_BWInputs &inputs) {
    auto schema = scene.at("schema").as_string();
    if (schema == "Monstey_WorldBlenderScene_2")
        return scene;
    Monstey_Require(schema == "Monstey_WorldBlenderScene_1", "Unsupported native Blender scene schema");
    std::map<std::string, J> native_rows;
    for (auto &[sid, source] : scene["sources"].as_object()) {
        auto raw = inputs.Verified(source.at("fixture"));
        auto inst = raw.size() >= 4 && std::memcmp(raw.data(), "PRFB", 4) == 0;
        source["backend"] = inst ? "INST" : "OBJRES";
        auto frame = source.value("frame", "world").as_string();
        source["frame"] = frame;
        auto matrix = Monstey_BWIdentity();
        if (frame == "local")
            for (unsigned i = 0; i < 3; ++i)
                matrix[i][3] = scene.at("origin_native_cm")[i].as_number();
        source["frame_to_scene_native"] = Monstey_BWJson(matrix);
        native_rows.emplace(sid, inst ? Monstey_InstSceneInstances(raw).at("instances")
                                      : Monstey_ObjresOwners(raw));
    }
    for (auto &row : scene["objects"].as_array()) {
        auto sid = row.at("source_id").as_string();
        auto &source = scene.at("sources").at(sid);
        auto &transform = row.at("transform");
        row["scene_matrix_native"] = Monstey_BWJson(
            Monstey_BWMul(Monstey_BWMatrix(source.at("frame_to_scene_native")),
                          Monstey_BWCompose(transform.at("position_cm"), transform.at("quaternion_xyzw"),
                                            transform.at("uniform_scale").as_number())));
        if (row.at("backend").as_string() == "OBJRES") {
            for (auto &owner : native_rows.at(sid).as_array())
                if (owner.at("group_index") == row.at("group_index") &&
                    owner.at("object_index") == row.at("object_index")) {
                    Monstey_Require(owner.at("name") == row.at("name") &&
                                        owner.at("class_name") == row.at("class_name"),
                                    "Legacy scene native identity differs");
                    row["properties"] = owner.at("property_values");
                    row["property_fields"] = owner.at("properties");
                }
        } else {
            bool found = false;
            for (auto &instance : native_rows.at(sid).as_array())
                if (instance.at("index") == row.at("instance_index")) {
                    row["owners"] = instance.at("owners");
                    found = true;
                }
            Monstey_Require(found, "Legacy instance identity differs");
            row["properties"] = J::object();
        }
    }
    J::Array geometry;
    auto meshes = scene.value("meshes", J::array());
    for (auto &mesh : meshes.as_array()) {
        auto sid = "geometry" + std::to_string(scene.at("sources").size());
        auto matrix = Monstey_BWCompose(mesh.at("position_cm"), mesh.at("quaternion_xyzw"),
                                        mesh.value("uniform_scale", 1).as_number());
        scene["sources"][sid] = J::object({{"backend", "INST"},
                                           {"resource_path", "prefabs/legacy-reference.inst"},
                                           {"fixture", mesh.at("source")},
                                           {"frame", "local"},
                                           {"frame_to_scene_native", Monstey_BWJson(matrix)}});
        auto raw = inputs.Verified(mesh.at("source"));
        auto parts = Monstey_InstDecodeGeometry(raw);
        J::Array indices;
        for (std::size_t i = 0; i < parts.size(); ++i)
            indices.push_back(static_cast<std::uint64_t>(i));
        geometry.push_back(J::object({{"id", mesh.at("id")},
                                      {"source_id", sid},
                                      {"source", mesh.at("source")},
                                      {"format", "PRFB8"},
                                      {"part_indices", indices},
                                      {"parent_object_id", J{}}}));
    }
    auto bounds = scene.value("bounds", J::array());
    for (auto &box : bounds.as_array()) {
        auto sid = box.at("source_id").as_string();
        geometry.push_back(
            J::object({{"id", box.at("id")},
                       {"source_id", sid},
                       {"source", scene.at("sources").at(sid).at("fixture")},
                       {"format", "DEBUG_AABB"},
                       {"bounds_cm", J::array({box.at("bounds_min_native"), box.at("bounds_max_native")})},
                       {"representation", "PFDG_DEBUG_AABB_NOT_MESH_NOT_PLACEMENT"},
                       {"parent_object_id", J{}}}));
    }
    scene["geometry"] = geometry;
    scene["schema"] = "Monstey_WorldBlenderScene_2";
    scene["legacy_schema"] = "Monstey_WorldBlenderScene_1";
    return scene;
}
struct Monstey_BWCreated {
    std::filesystem::path root;
    std::vector<std::filesystem::path> files;
    bool done = false;
    explicit Monstey_BWCreated(std::filesystem::path path) : root(std::move(path)) {
        Monstey_RequireOfflineOutput(root);
        Monstey_Require(!std::filesystem::exists(Monstey_WindowsFilePath(root)),
                        "World Blender output directory already exists");
        std::filesystem::create_directories(Monstey_WindowsFilePath(root.parent_path()));
        Monstey_Require(std::filesystem::create_directory(Monstey_WindowsFilePath(root)),
                        "Cannot create World Blender output directory");
    }
    void Track(const std::filesystem::path &path) {
        auto relative = path.lexically_relative(root);
        Monstey_Require(!relative.empty() && !relative.is_absolute() &&
                            *relative.begin() != std::filesystem::path(".."),
                        "World Blender output escaped new directory");
        files.push_back(path);
    }
    ~Monstey_BWCreated() {
        if (done)
            return;
        std::set<std::filesystem::path> dirs{root};
        std::error_code ec;
        for (auto i = files.rbegin(); i != files.rend(); ++i) {
            std::filesystem::remove(Monstey_WindowsFilePath(*i), ec);
            auto p = i->parent_path();
            while (!p.empty() && p != root && p.wstring().size() > root.wstring().size()) {
                dirs.insert(p);
                p = p.parent_path();
            }
        }
        std::vector<std::filesystem::path> ordered(dirs.begin(), dirs.end());
        std::sort(ordered.begin(), ordered.end(),
                  [](const auto &a, const auto &b) { return a.wstring().size() > b.wstring().size(); });
        for (auto &p : ordered)
            std::filesystem::remove(Monstey_WindowsFilePath(p), ec);
    }
};
J Monstey_BWPrepare(const J &a) {
    Monstey_BWKeys(a, {"region", "output", "game_root", "oodle_dll", "spec"});
    auto region = a.at("region").as_string();
    static const std::set<std::string> regions{"island", "mtnpass", "snowmtn", "volcano", "desert"};
    Monstey_Require(regions.contains(region), "Unknown native world region");
    auto output = Monstey_BWPath(a.at("output"));
    Monstey_BWCreated created(output);
    J spec, extracted;
    if (a.contains("spec"))
        spec = a.at("spec");
    else {
        auto base = "worlds/retail/" + region + "/";
        std::vector<std::string> resources{base + region + ".objres",
                                           base + "sectionsandlayers/dynamicfort/throneroombeast.objres",
                                           base + "sectionsandlayers/dynamicfort/keeplarge.objres",
                                           base + "sectionsandlayers/dynamicfort/keepinterior.objres",
                                           "prefabs/common/architecture/fort/keep/keepinterior.inst"};
        J::Array requests;
        for (auto &resource : resources)
            requests.push_back(J::object(
                {{"resource_path", resource},
                 {"bundle",
                  resource.substr(0, resource.size() - (resource.ends_with(".inst") ? 5 : 7)) + ".embb"}}));
        auto extraction = J::object({{"requests", requests},
                                     {"output", Monstey_BWText(output / "native")},
                                     {"max_total_bytes", 268435456}});
        for (auto name : {"game_root", "oodle_dll"})
            if (a.contains(name))
                extraction[name] = a.at(name);
        auto selected = Monstey_WorldAssetsCall("sow_world_asset_extract", extraction);
        extracted = selected;
        for (auto &row : selected.at("resources").as_array())
            created.Track(Monstey_BWPath(row.at("path")));
        created.Track(output / "native/resource-manifest.json");
        J::Array sources;
        for (auto &row : selected.at("resources").as_array()) {
            auto resource = row.at("resource_path").as_string();
            if (resource.ends_with(".inst"))
                continue;
            J selection = J::object({{"limit", 5000}});
            if (resource == resources[0])
                selection["name_contains"] = "KeepT_00.";
            else {
                auto owners = Monstey_ObjresOwners(Monstey_ReadFile(Monstey_BWPath(row.at("path"))));
                static const std::set<std::string> excluded{
                    "MemoryReporter",        "WorldInitializer",    "WorldCollision", "Group",
                    "DecalLightDirectional", "WorldModelSkyShadow", "StartupCommand", "WorldActiveTag"};
                J::Array classes;
                std::set<std::string> seen;
                for (auto &owner : owners.as_array()) {
                    auto type = owner.at("class_name").as_string();
                    if (!excluded.contains(type) && seen.insert(type).second)
                        classes.push_back(type);
                }
                selection["class_names"] = classes;
            }
            sources.push_back(J::object({{"source", row.at("path")},
                                         {"resource_path", resource},
                                         {"frame", "world"},
                                         {"select", selection}}));
        }
        spec = J::object({{"title", "Native fortress | " + region}, {"sources", sources}});
    }
    auto prepared = Monstey_WorldCall("sow_world_scene_prepare",
                                      J::object({{"spec", spec}, {"output", Monstey_BWText(output)}}));
    created.Track(output / "scene.json");
    created.Track(output / "preparation-report.json");
    auto scene = J::Parse(Monstey_ReadText(output / "scene.json"));
    Monstey_BWInputs inputs;
    if (!extracted.is_null()) {
        const J *asset = nullptr;
        J anchor, anchor_source;
        for (auto &row : extracted.at("resources").as_array()) {
            if (row.at("resource_path").as_string().ends_with(".inst"))
                asset = &row;
            else if (row.at("resource_path").as_string().ends_with("/keepinterior.objres")) {
                auto owners = Monstey_ObjresOwners(inputs.Verified(row), false);
                for (auto &owner : owners.as_array())
                    if (owner.at("name").as_string() == "KeepI_00.WMSS_00") {
                        Monstey_Require(anchor.is_null(), "Native keep geometry anchor is ambiguous");
                        anchor = owner;
                        anchor_source = row;
                    }
            }
        }
        if (asset && !anchor.is_null()) {
            auto data = inputs.Verified(*asset);
            auto parts = Monstey_InstDecodeGeometry(data);
            auto native = Monstey_InstReadGeometry(data);
            J materials = J::array();
            for (auto &name : native.material_names)
                materials.as_array().push_back(name);
            J::Array indices;
            std::array<double, 3> low, high;
            low.fill(std::numeric_limits<double>::infinity());
            high.fill(-std::numeric_limits<double>::infinity());
            std::uint64_t vertices = 0, triangles = 0;
            for (std::size_t i = 0; i < parts.size(); ++i) {
                auto &part = parts[i];
                indices.push_back(static_cast<std::uint64_t>(i));
                vertices += part.at("POSITION").size();
                triangles += part.at("INDICES").size();
                for (auto &face : part.at("INDICES").as_array())
                    for (auto &index : face.as_array())
                        for (unsigned axis = 0; axis < 3; ++axis) {
                            auto value = part.at("POSITION")[index.as_uint()][axis].as_number();
                            low[axis] = std::min(low[axis], value);
                            high[axis] = std::max(high[axis], value);
                        }
            }
            Monstey_Require(!parts.empty() && std::isfinite(low[0]),
                            "Native keep reference has no indexed triangles");
            auto sid = "keep_reference";
            Monstey_Require(!scene.at("sources").contains(sid), "Native keep source identity collision");
            scene["sources"][sid] = J::object(
                {{"backend", "INST"},
                 {"resource_path", asset->at("resource_path")},
                 {"fixture", *asset},
                 {"frame", "local"},
                 {"frame_to_scene_native",
                  Monstey_BWJson(Monstey_BWCompose(anchor.at("position"), anchor.at("quaternion")))},
                 {"reference_only", true}});
            scene["geometry"].as_array().push_back(J::object(
                {{"id", "mesh:keepinterior"},
                 {"source_id", sid},
                 {"source", *asset},
                 {"format", "PRFB8"},
                 {"part_indices", indices},
                 {"parent_object_id", J{}},
                 {"anchor_name", anchor.at("name")},
                 {"anchor_source", anchor_source},
                 {"vertices", vertices},
                 {"triangles", triangles},
                 {"bounds_cm",
                  J::array({J::array({low[0], low[1], low[2]}), J::array({high[0], high[1], high[2]})})},
                 {"material_paths", materials},
                 {"representation", "REAL_NATIVE_TRIANGLES_CLAY_REFERENCE"},
                 {"display_lod_policy",
                  "All original source parts, matching the original read-only keep preview"}}));
            scene["warnings"].as_array().push_back(J::object(
                {{"reason", "Keep geometry is a read-only reference. Moving its logical WMSS owner alone "
                            "does not rebuild compiled render, collision or navigation associations."}}));
        } else
            scene["warnings"].as_array().push_back(
                J::object({{"reason", "No unique native keep geometry anchor was available; placements "
                                      "remain explicit transform references."}}));
        scene["preparation_recipe"] =
            J::object({{"operation", "sow_blender_prepare"},
                       {"region", region},
                       {"native_source_manifest", Monstey_BWText(output / "native/resource-manifest.json")}});
        auto rewrite = [&](const std::filesystem::path &path, const J &value) {
            Monstey_Require(std::filesystem::remove(Monstey_WindowsFilePath(path)),
                            "Cannot finalize newly-created scene metadata");
            Monstey_WriteNewText(path, value.dump(2) + "\n");
        };
        rewrite(output / "scene.json", scene);
        prepared["scene"] = Monstey_BWRecord(output / "scene.json", Monstey_ReadFile(output / "scene.json"));
        prepared["sources"] = scene.at("sources").size();
        prepared["native_geometry_references"] = scene.at("geometry").size();
        prepared["warnings"] = scene.at("warnings");
        rewrite(output / "preparation-report.json", prepared);
    }
    inputs.Scene(scene);
    inputs.Verify();
    prepared["region"] = region;
    prepared["game_modified"] = false;
    created.done = true;
    return prepared;
}
J Monstey_BWAttach(const J &a) {
    Monstey_BWKeys(a, {"scene", "source", "resource_path", "frame", "frame_to_scene", "output"});
    auto scene_path = Monstey_BWPath(a.at("scene")), source_path = Monstey_BWPath(a.at("source")),
         output = Monstey_BWPath(a.at("output"));
    Monstey_BWInputs inputs;
    auto scene = Monstey_BWNormalize(inputs.Json(scene_path), inputs);
    Monstey_Require(scene.at("schema").as_string() == "Monstey_WorldBlenderScene_2",
                    "Attach instances to a qualified generic world scene");
    inputs.Scene(scene);
    auto raw = inputs.Load(source_path);
    auto frame = a.at("frame").as_string();
    Monstey_Require(frame == "world" || frame == "local", "Instance frame must be world or local");
    auto resource = Monstey_ArchiveVirtual(a.at("resource_path").as_string());
    Monstey_Require(resource.ends_with(".inst"), "Native instance resource must use .inst");
    for (auto &[id, source] : scene.at("sources").as_object()) {
        (void)id;
        Monstey_Require(Monstey_BWPath(source.at("fixture").at("path")) != source_path,
                        "Native instance source is already attached");
    }
    auto report = Monstey_InstSceneInstances(raw);
    std::size_t ordinal = scene.at("sources").size();
    std::string sid;
    do {
        sid = "i" + std::to_string(ordinal++);
    } while (scene.at("sources").contains(sid));
    auto transform = a.value(
        "frame_to_scene",
        J::object({{"position", J::array({0, 0, 0})}, {"quaternion", J::array({0, 0, 0, 1})}, {"scale", 1}}));
    auto matrix = Monstey_BWCompose(transform.at("position"), transform.at("quaternion"),
                                    transform.value("scale", 1).as_number());
    if (frame == "world")
        Monstey_Require(matrix == Monstey_BWIdentity(), "World frame must have identity frame_to_scene");
    auto source_record = Monstey_BWRecord(source_path, raw);
    scene["sources"][sid] =
        J::object({{"resource_path", resource},
                   {"fixture", source_record},
                   {"backend", "INST"},
                   {"frame", frame},
                   {"frame_to_scene_native", Monstey_BWJson(matrix)},
                   {"selection", J::object({{"limit", 5000}, {"include_unowned", true}})}});
    for (auto &instance : report.at("instances").as_array()) {
        auto index = instance.at("index").as_uint();
        auto native = Monstey_BWCompose(instance.at("position"), instance.at("quaternion"),
                                        instance.at("scale").as_number());
        auto owner = instance.value("owners", J::array());
        auto row = J::object({{"id", sid + ":inst:" + std::to_string(index)},
                              {"source_id", sid},
                              {"backend", "INST"},
                              {"name", "PRFB " + std::to_string(index)},
                              {"class_name", "PRFB_INSTANCE"},
                              {"instance_index", index},
                              {"prefab_path", instance.at("prefab_path")},
                              {"definition_key", instance.at("definition_key")},
                              {"owners", owner},
                              {"properties", J::object()},
                              {"transform", J::object({{"position_cm", instance.at("position")},
                                                       {"quaternion_xyzw", instance.at("quaternion")},
                                                       {"uniform_scale", instance.at("scale")}})},
                              {"scene_matrix_native", Monstey_BWJson(Monstey_BWMul(matrix, native))}});
        scene["objects"].as_array().push_back(std::move(row));
    }
    if (scene.contains("source_spec")) {
        scene["source_spec"]["sources"].as_array().push_back(
            J::object({{"id", sid},
                       {"source", Monstey_BWText(source_path)},
                       {"resource_path", resource},
                       {"frame", frame},
                       {"frame_to_scene", transform},
                       {"select", J::object({{"limit", 5000}, {"include_unowned", true}})}}));
    }
    inputs.Output(output);
    inputs.Verify();
    Monstey_WriteNewText(output, scene.dump(2) + "\n");
    try {
        inputs.Verify();
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove(Monstey_WindowsFilePath(output), ec);
        throw;
    }
    return J::object({{"source_id", sid},
                      {"frame", frame},
                      {"instances", report.at("instances").size()},
                      {"source_sha256", source_record.at("sha256")},
                      {"scene", Monstey_BWText(output)},
                      {"game_modified", false}});
}
J Monstey_BWValidate(const J &a) {
    Monstey_BWKeys(a, {"spec"});
    Monstey_BWInputs inputs;
    auto plan = inputs.Json(Monstey_BWPath(a.at("spec")));
    auto report = Monstey_WorldEditCall("sow_worldedit_compile", J::object({{"plan", plan}}));
    inputs.Verify();
    auto result = J::object({{"status", "PASS"},
                             {"schema", plan.at("schema")},
                             {"results", report.at("operations")},
                             {"pending_dependencies", report.at("pending_dependencies")},
                             {"binary_written", false},
                             {"game_modified", false}});
    return result;
}
J Monstey_BWDecompose(const M &matrix) {
    auto m = matrix;
    std::array<double, 3> scales{};
    for (unsigned col = 0; col < 3; ++col)
        for (unsigned row = 0; row < 3; ++row)
            scales[col] += m[row][col] * m[row][col];
    for (auto &v : scales)
        v = std::sqrt(v);
    auto scale = (scales[0] + scales[1] + scales[2]) / 3;
    Monstey_Require(scale > 1e-8 && std::isfinite(scale), "World placement scale is zero or nonfinite");
    for (auto v : scales)
        Monstey_Require(std::abs(v - scale) < 1e-5 * std::max(1.0, scale),
                        "World placement has unsupported nonuniform scale");
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            m[i][j] /= scale;
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j) {
            double dot = 0;
            for (unsigned k = 0; k < 3; ++k)
                dot += m[k][i] * m[k][j];
            Monstey_Require(std::abs(dot - (i == j ? 1.0 : 0.0)) < 1e-5,
                            "World placement has unsupported shear");
        }
    auto determinant = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                       m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                       m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    Monstey_Require(determinant > 0, "World placement has unsupported reflection");
    double x, y, z, w;
    auto trace = m[0][0] + m[1][1] + m[2][2];
    if (trace > 0) {
        auto s = std::sqrt(trace + 1) * 2;
        w = s / 4;
        x = (m[2][1] - m[1][2]) / s;
        y = (m[0][2] - m[2][0]) / s;
        z = (m[1][0] - m[0][1]) / s;
    } else if (m[0][0] > m[1][1] && m[0][0] > m[2][2]) {
        auto s = std::sqrt(1 + m[0][0] - m[1][1] - m[2][2]) * 2;
        w = (m[2][1] - m[1][2]) / s;
        x = s / 4;
        y = (m[0][1] + m[1][0]) / s;
        z = (m[0][2] + m[2][0]) / s;
    } else if (m[1][1] > m[2][2]) {
        auto s = std::sqrt(1 + m[1][1] - m[0][0] - m[2][2]) * 2;
        w = (m[0][2] - m[2][0]) / s;
        x = (m[0][1] + m[1][0]) / s;
        y = s / 4;
        z = (m[1][2] + m[2][1]) / s;
    } else {
        auto s = std::sqrt(1 + m[2][2] - m[0][0] - m[1][1]) * 2;
        w = (m[1][0] - m[0][1]) / s;
        x = (m[0][2] + m[2][0]) / s;
        y = (m[1][2] + m[2][1]) / s;
        z = s / 4;
    }
    if (w < 0) {
        x = -x;
        y = -y;
        z = -z;
        w = -w;
    }
    auto f = [](double v) { return static_cast<double>(static_cast<float>(v)); };
    return J::object({{"position", J::array({f(matrix[0][3]), f(matrix[1][3]), f(matrix[2][3])})},
                      {"quaternion", J::array({f(x), f(y), f(z), f(w)})},
                      {"scale", f(scale)}});
}
void Monstey_BWSetMatrix(Monstey_Blend &blend, std::size_t block, const M &matrix) {
    auto t = Monstey_BWDecompose(matrix);
    auto &q = t.at("quaternion");
    Monstey_BWSet(blend, block, "loc", t.at("position"));
    Monstey_BWSet(blend, block, "rotmode", 0);
    Monstey_BWSet(blend, block, "quat", J::array({q[3], q[0], q[1], q[2]}));
    auto scale = t.at("scale").as_number();
    Monstey_BWSet(blend, block, "size", J::array({scale, scale, scale}));
}
M Monstey_BWRotation(const J &object, bool delta) {
    auto mode = object.at("rotmode").as_int();
    if (mode == 0) {
        auto &q = object.at(delta ? "dquat" : "quat");
        return Monstey_BWCompose(J::array({0, 0, 0}), J::array({q[1], q[2], q[3], q[0]}));
    }
    if (mode == -1) {
        auto &axis = object.at(delta ? "drotAxis" : "rotAxis");
        auto angle = object.at(delta ? "drotAngle" : "rotAngle").as_number();
        auto norm =
            std::sqrt(axis[0].as_number() * axis[0].as_number() + axis[1].as_number() * axis[1].as_number() +
                      axis[2].as_number() * axis[2].as_number());
        if (std::abs(angle) < 1e-12)
            return Monstey_BWIdentity();
        Monstey_Require(norm > 1e-12, "Zero Blender rotation axis");
        auto s = std::sin(angle / 2) / norm;
        return Monstey_BWCompose(J::array({0, 0, 0}),
                                 J::array({axis[0].as_number() * s, axis[1].as_number() * s,
                                           axis[2].as_number() * s, std::cos(angle / 2)}));
    }
    static const std::array<std::string_view, 6> orders{"XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"};
    Monstey_Require(mode >= 1 && mode <= 6, "Unknown Blender rotation mode");
    auto &angles = object.at(delta ? "drot" : "rot");
    auto matrix = Monstey_BWIdentity();
    for (auto axis : orders[static_cast<std::size_t>(mode - 1)]) {
        auto index = static_cast<unsigned>(axis - 'X');
        auto q = J::array({0.0, 0.0, 0.0, std::cos(angles[index].as_number() / 2)});
        q[index] = std::sin(angles[index].as_number() / 2);
        matrix = Monstey_BWMul(Monstey_BWCompose(J::array({0, 0, 0}), q), matrix);
    }
    return matrix;
}
M Monstey_BWLocal(const J &object) {
    auto matrix = Monstey_BWMul(Monstey_BWRotation(object, true), Monstey_BWRotation(object, false));
    for (unsigned i = 0; i < 3; ++i) {
        for (unsigned j = 0; j < 3; ++j)
            matrix[i][j] *= object.at("size")[j].as_number() * object.at("dscale")[j].as_number();
        matrix[i][3] = object.at("loc")[i].as_number() + object.at("dloc")[i].as_number();
    }
    return matrix;
}
M Monstey_BWParentInverse(const J &j) {
    Monstey_Require(j.is_array() && j.size() == 16, "Blender parent inverse needs16 values");
    auto m = Monstey_BWIdentity();
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned k = 0; k < 4; ++k)
            m[i][k] = j[k * 4 + i].as_number();
    return m;
}
bool Monstey_BWSameMatrix(const M &a, const M &b) {
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned k = 0; k < 4; ++k)
            if (static_cast<float>(a[i][k]) != static_cast<float>(b[i][k]))
                return false;
    return true;
}
J Monstey_BWStored(const J &properties, const char *name, const J &fallback) {
    if (!properties.contains(name))
        return fallback;
    auto value = properties.at(name);
    return value.is_string() ? J::Parse(value.as_string()) : value;
}
M Monstey_BWNative(const M &display, const J &scene, const J &source) {
    auto native =
        Monstey_BWMul(Monstey_BWMul(Monstey_BWInverse(Monstey_BWBasis()), display), Monstey_BWBasis());
    for (unsigned i = 0; i < 3; ++i)
        native[i][3] = native[i][3] * 100 + scene.at("origin_native_cm")[i].as_number();
    return Monstey_BWMul(Monstey_BWInverse(Monstey_BWMatrix(source.at("frame_to_scene_native"))), native);
}
std::size_t Monstey_BWBlock(const J &row) {
    return static_cast<std::size_t>(row.at(row.contains("block") ? "block" : "index").as_uint());
}
std::uint64_t Monstey_BWAddress(const J &row) { return row.at("address").as_uint(); }
J Monstey_BWUnknown(const J &row) {
    J::Array fields;
    auto all = row.value("property_fields", J::array());
    for (auto &field : all.as_array())
        if (!field.value("editable", false).as_bool())
            fields.push_back(field);
    return fields;
}
std::string Monstey_BWFingerprint(const Monstey_ModelPart &part) {
    B raw;
    for (auto name : {"POSITION", "UV0"}) {
        auto found = part.channels.find(name);
        Monstey_AppendU32(raw, found == part.channels.end() ? 0 : found->second.width);
        if (found != part.channels.end())
            for (auto value : found->second.values)
                Monstey_AppendU32(raw, std::bit_cast<std::uint32_t>(static_cast<float>(value)));
    }
    for (auto index : part.indices)
        Monstey_AppendU32(raw, index);
    return Monstey_HashSha256(raw);
}
Monstey_ModelPart Monstey_BWGeometry(const J &row, Monstey_BWInputs &inputs) {
    auto raw = inputs.Verified(row.at("source"));
    if (row.at("format").as_string() == "DEBUG_AABB") {
        Monstey_ModelPart result;
        result.channels["POSITION"].width = 3;
        result.channels["UV0"].width = 2;
        for (unsigned corner = 0; corner < 8; ++corner) {
            auto &box = row.at("bounds_cm");
            std::array<double, 3> p{};
            for (unsigned i = 0; i < 3; ++i)
                p[i] = box[(corner >> i) & 1][i].as_number();
            result.channels["POSITION"].values.insert(result.channels["POSITION"].values.end(),
                                                      {p[0] / 100, -p[2] / 100, p[1] / 100});
        }
        result.channels["UV0"].values.resize(16, 0);
        result.indices = {0, 1, 3, 0, 3, 2, 4, 6, 7, 4, 7, 5, 0, 4, 5, 0, 5, 1,
                          2, 3, 7, 2, 7, 6, 0, 2, 6, 0, 6, 4, 1, 5, 7, 1, 7, 3};
        return result;
    }
    J selected = J::array();
    if (row.at("format").as_string() == "MESH5")
        selected.as_array().push_back(
            Monstey_WorldMeshDecodeGeometry(raw, static_cast<std::size_t>(row.at("group_index").as_uint()),
                                            static_cast<std::size_t>(row.at("draw_index").as_uint())));
    else {
        auto parts = Monstey_InstDecodeGeometry(raw);
        for (auto &index : row.at("part_indices").as_array())
            selected.as_array().push_back(parts[index.as_uint()]);
    }
    Monstey_ModelPart result;
    result.channels["POSITION"].width = 3;
    result.channels["UV0"].width = 2;
    std::uint32_t offset = 0;
    for (auto &part : selected.as_array()) {
        auto &positions = part.at("POSITION");
        Monstey_Require(positions.size() <= 0xffffffffull - offset,
                        "World Blender geometry exceeds vertex limit");
        for (auto &position : positions.as_array()) {
            result.channels["POSITION"].values.insert(result.channels["POSITION"].values.end(),
                                                      {position[0].as_number() / 100,
                                                       -position[2].as_number() / 100,
                                                       position[1].as_number() / 100});
        }
        if (part.contains("UV0"))
            for (auto &uv : part.at("UV0").as_array()) {
                result.channels["UV0"].values.push_back(uv[0].as_number());
                result.channels["UV0"].values.push_back(uv[1].as_number());
            }
        else
            result.channels["UV0"].values.resize(result.channels["UV0"].values.size() + positions.size() * 2,
                                                 0);
        for (auto &face : part.at("INDICES").as_array()) {
            Monstey_Require(face.size() == 3, "World geometry is not triangulated");
            for (auto &index : face.as_array()) {
                Monstey_Require(index.as_uint() < positions.size(),
                                "World geometry index exceeds its vertices");
                result.indices.push_back(offset + static_cast<std::uint32_t>(index.as_uint()));
            }
        }
        offset += static_cast<std::uint32_t>(positions.size());
    }
    Monstey_Require(!result.indices.empty(), "Native geometry reference has no triangles");
    return result;
}
std::filesystem::path Monstey_BWTemplate(const J &a) { return Monstey_BlendGeometryTemplate(a); }
std::pair<J, J> Monstey_BWCameras(Monstey_Blend &blend, const J &scene) {
    std::array<double, 3> low, high;
    low.fill(std::numeric_limits<double>::infinity());
    high.fill(-std::numeric_limits<double>::infinity());
    auto point = [&](const std::array<double, 3> &p) {
        for (unsigned i = 0; i < 3; ++i) {
            low[i] = std::min(low[i], p[i]);
            high[i] = std::max(high[i], p[i]);
        }
    };
    std::map<std::string, M> logical;
    for (auto &row : scene.at("objects").as_array()) {
        auto m =
            Monstey_BWDisplay(Monstey_BWMatrix(row.at("scene_matrix_native")), scene.at("origin_native_cm"));
        logical.emplace(row.at("id").as_string(), m);
        point({m[0][3], m[1][3], m[2][3]});
    }
    auto geometry = scene.value("geometry", J::array());
    for (auto &row : geometry.as_array()) {
        auto parent = row.value("parent_object_id", J{});
        auto m = parent.is_null()
                     ? Monstey_BWDisplay(Monstey_BWMatrix(scene.at("sources")
                                                              .at(row.at("source_id").as_string())
                                                              .at("frame_to_scene_native")),
                                         scene.at("origin_native_cm"))
                     : logical.at(parent.as_string());
        if (row.contains("bounds_cm"))
            for (unsigned corner = 0; corner < 8; ++corner) {
                std::array<double, 3> native{};
                for (unsigned i = 0; i < 3; ++i)
                    native[i] = row.at("bounds_cm")[(corner >> i) & 1][i].as_number();
                std::array<double, 3> display{native[0] / 100, -native[2] / 100, native[1] / 100}, world{};
                for (unsigned i = 0; i < 3; ++i) {
                    world[i] = m[i][3];
                    for (unsigned k = 0; k < 3; ++k)
                        world[i] += m[i][k] * display[k];
                }
                point(world);
            }
    }
    if (!std::isfinite(low[0])) {
        low = {-1, -1, -1};
        high = {1, 1, 1};
    }
    std::array<double, 3> center{};
    double extent = 3;
    for (unsigned i = 0; i < 3; ++i) {
        center[i] = (low[i] + high[i]) / 2;
        extent = std::max(extent, high[i] - low[i]);
    }
    auto normalize = [](std::array<double, 3> v) {
        double s = 0;
        for (auto a : v)
            s += a * a;
        s = std::sqrt(s);
        Monstey_Require(s > 1e-12, "Camera direction is zero");
        for (auto &a : v)
            a /= s;
        return v;
    };
    auto cross = [](const auto &a, const auto &b) {
        return std::array<double, 3>{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                                     a[0] * b[1] - a[1] * b[0]};
    };
    auto camera = [&](const char *name, std::array<double, 3> direction) {
        auto data = blend.NewBlock("CA", "Camera");
        auto index = Monstey_BWBlock(data);
        Monstey_BWSet(blend, index, "id.name", std::string("CA") + name);
        Monstey_BWSet(blend, index, "id.us", 1);
        Monstey_BWSet(blend, index, "type", 1);
        Monstey_BWSet(blend, index, "ortho_scale", extent * 1.9);
        Monstey_BWSet(blend, index, "clipsta", 0.1);
        Monstey_BWSet(blend, index, "clipend", std::max(10000.0, extent * 8));
        auto object = Monstey_BlendGeometryCreateObject(blend, name, 11, Monstey_BWAddress(data));
        auto back = normalize(direction), right = normalize(cross(std::array<double, 3>{0, 0, 1}, back)),
             up = cross(back, right);
        auto m = Monstey_BWIdentity();
        for (unsigned i = 0; i < 3; ++i) {
            m[i][0] = right[i];
            m[i][1] = up[i];
            m[i][2] = back[i];
            m[i][3] = center[i] + direction[i] * extent * 2;
        }
        Monstey_BWSetMatrix(blend, Monstey_BWBlock(object), m);
        return std::pair{object, m};
    };
    auto perspective = camera("VIEW_perspective", {0.8, -1, 0.7}), top = camera("VIEW_top", {0, 0.001, 2});
    Monstey_BlendGeometryLinkObjects(blend, Monstey_BlendGeometryRootCollection(blend),
                                     {perspective.first, top.first});
    auto scenes = blend.Blocks("SC").at("rows");
    Monstey_Require(!scenes.empty(), "Blender template has no Scene");
    auto scene_block = Monstey_BWBlock(scenes[0]);
    Monstey_BWSet(blend, scene_block, "camera", Monstey_BWAddress(perspective.first));
    Monstey_BWSet(blend, scene_block, "r.engine", "BLENDER_WORKBENCH");
    Monstey_BWSet(blend, scene_block, "r.xsch", 1300);
    Monstey_BWSet(blend, scene_block, "r.ysch", 1000);
    Monstey_BWSet(blend, scene_block, "r.size", 100);
    auto q = Monstey_BWDecompose(perspective.second).at("quaternion");
    std::size_t offset = 0;
    while (true) {
        auto rows = blend.Blocks("", offset, 250).at("rows");
        for (auto &row : rows.as_array()) {
            auto type = row.at("type").as_string();
            auto block = Monstey_BWBlock(row);
            if (type == "RegionView3D") {
                Monstey_BWSet(blend, block, "ofs", J::array({-center[0], -center[1], -center[2]}));
                Monstey_BWSet(blend, block, "dist", extent * 1.2);
                Monstey_BWSet(blend, block, "viewquat", J::array({q[3], q[0], q[1], q[2]}));
            } else if (type == "View3D")
                Monstey_BWSet(blend, block, "far", std::max(10000.0, extent * 8));
        }
        if (rows.size() < 250)
            break;
        offset += rows.size();
    }
    return {perspective.first, top.first};
}
std::wstring Monstey_BWQuote(std::wstring_view text) {
    std::wstring out = L"\"";
    std::size_t slashes = 0;
    for (auto c : text) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'\"')
            out.append(slashes * 2 + 1, L'\\');
        else
            out.append(slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    out += L'\"';
    return out;
}
void Monstey_BWRender(const std::filesystem::path &exe, const std::filesystem::path &blend,
                      const std::filesystem::path &png, const std::filesystem::path &log) {
    Monstey_Require(std::filesystem::is_regular_file(Monstey_WindowsFilePath(exe)),
                    "Configure Blender for native reference rendering");
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
    HANDLE output = CreateFileW(Monstey_WindowsFilePath(log).c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                &attributes, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    Monstey_Require(output != INVALID_HANDLE_VALUE, "Cannot create new Blender render log");
    auto command = Monstey_BWQuote(exe.wstring()) + L" --background --disable-autoexec " +
                   Monstey_BWQuote(blend.wstring()) + L" --render-format PNG --render-output " +
                   Monstey_BWQuote(png.wstring()) + L" --render-frame 1";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = output;
    startup.hStdError = output;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    auto ok = CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                             nullptr, &startup, &process);
    CloseHandle(output);
    Monstey_Require(ok != FALSE, "Cannot launch configured Blender renderer");
    CloseHandle(process.hThread);
    auto waited = WaitForSingleObject(process.hProcess, 60000);
    DWORD code = 1;
    if (waited != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, 5000);
    } else
        GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    Monstey_Require(waited == WAIT_OBJECT_0 && code == 0,
                    "Blender native reference render failed or timed out; inspect its log");
}
std::string Monstey_BWLabel(std::string_view label, std::size_t ordinal) {
    auto suffix = " [" + std::to_string(ordinal) + "]";
    Monstey_Require(suffix.size() < 63, "Blender label ordinal is too long");
    auto count = std::min(label.size(), 63 - suffix.size());
    while (count < label.size() && count > 0 && (static_cast<unsigned char>(label[count]) & 0xc0) == 0x80)
        --count;
    return std::string(label.substr(0, count)) + suffix;
}
J Monstey_BWBuild(const J &a) {
    Monstey_BWKeys(a, {"scene", "output_dir", "render", "blend_template", "blender"});
    auto scene_path = Monstey_BWPath(a.at("scene")), output = Monstey_BWPath(a.at("output_dir"));
    Monstey_BWInputs inputs;
    auto scene = Monstey_BWNormalize(inputs.Json(scene_path), inputs);
    Monstey_Require(scene.at("schema").as_string() == "Monstey_WorldBlenderScene_2",
                    "World Blender build expects a generic native scene");
    inputs.Scene(scene);
    auto template_path = Monstey_BWTemplate(a);
    inputs.Load(template_path);
    Monstey_Blend blend(template_path);
    Monstey_Require(blend.Describe().at("blender_file_version").as_uint() == 500,
                    "Static world authoring requires the qualified Blender v0500 SDNA");
    auto template_objects = Monstey_BlendGeometryObjects(blend);
    for (const auto &object : template_objects.as_array())
        Monstey_BlendGeometrySetProperty(blend, Monstey_BWBlock(object), "Monstey_template_orphan", 1);
    auto root = Monstey_BlendGeometryRootCollection(blend);
    Monstey_BlendGeometryLinkObjects(blend, root, {}, true);
    Monstey_BWSet(blend, root, "children.first", 0);
    Monstey_BWSet(blend, root, "children.last", 0);
    std::map<std::string, J> collections, objects;
    std::map<std::string, std::vector<J>> collection_objects;
    for (auto &[sid, source] : scene.at("sources").as_object()) {
        auto collection = Monstey_BlendGeometryCollection(
            blend, Monstey_BWLabel(source.at("backend").as_string() + " | " + sid, collections.size()));
        Monstey_BlendGeometryLinkCollection(blend, root, collection);
        collections.emplace(sid, collection);
    }
    for (auto &row : scene.at("objects").as_array()) {
        auto sid = row.at("source_id").as_string();
        Monstey_Require(collections.contains(sid), "Logical object has no native source collection");
        auto object = Monstey_BlendGeometryCreateObject(
            blend, Monstey_BWLabel(row.at("name").as_string(), objects.size()), 0);
        auto block = Monstey_BWBlock(object);
        Monstey_BWSetMatrix(
            blend, block,
            Monstey_BWDisplay(Monstey_BWMatrix(row.at("scene_matrix_native")), scene.at("origin_native_cm")));
        auto props = J::object({{"Monstey_id", row.at("id")},
                                {"Monstey_kind", row.at("backend")},
                                {"Monstey_action", "existing"},
                                {"Monstey_source_id", sid},
                                {"Monstey_native_name", row.at("name")},
                                {"Monstey_class_name", row.at("class_name")},
                                {"Monstey_properties_json", row.value("properties", J::object()).dump()},
                                {"Monstey_unknown_fields_json", Monstey_BWUnknown(row).dump()}});
        if (row.at("backend").as_string() == "OBJRES")
            Monstey_BWSet(blend, block, "protectflag", 448);
        else {
            props["Monstey_prefab_path"] = row.at("prefab_path");
            props["Monstey_native_owners_json"] = row.at("owners").dump();
            if (row.at("owners").size() == 1)
                props["Monstey_owner_definition_key"] = row.at("owners")[0].at("definition_key");
        }
        for (auto &[key, value] : props.as_object())
            Monstey_BlendGeometrySetProperty(blend, block, key, value);
        Monstey_Require(objects.emplace(row.at("id").as_string(), object).second,
                        "Duplicate logical scene identity");
        collection_objects[sid].push_back(object);
    }
    auto geometry = scene.value("geometry", J::array());
    std::size_t geometry_ordinal = objects.size();
    for (auto &row : geometry.as_array()) {
        auto label_ordinal = geometry_ordinal++;
        auto part = Monstey_BWGeometry(row, inputs);
        auto mesh = Monstey_BlendGeometryCreateMesh(blend, part,
                                                    Monstey_BWLabel(row.at("id").as_string(), label_ordinal));
        auto object = Monstey_BlendGeometryCreateObject(blend, "GEOMETRY | " + row.at("id").as_string(), 1,
                                                        Monstey_BWAddress(mesh));
        auto block = Monstey_BWBlock(object);
        auto sid = row.at("source_id").as_string();
        auto parent = row.value("parent_object_id", J{});
        if (!parent.is_null()) {
            Monstey_Require(objects.contains(parent.as_string()), "Native geometry parent is absent");
            Monstey_BWSet(blend, block, "parent", Monstey_BWAddress(objects.at(parent.as_string())));
        } else
            Monstey_BWSetMatrix(
                blend, block,
                Monstey_BWDisplay(Monstey_BWMatrix(scene.at("sources").at(sid).at("frame_to_scene_native")),
                                  scene.at("origin_native_cm")));
        Monstey_BWSet(blend, block, "protectflag", 511);
        Monstey_BWSet(blend, block, "col", J::array({0.34, 0.42, 0.49, 1}));
        auto props = J::object({{"Monstey_kind", "NATIVE_GEOMETRY_REFERENCE"},
                                {"Monstey_geometry_id", row.at("id")},
                                {"Monstey_parent_object_id", parent.is_null() ? J("") : parent},
                                {"Monstey_geometry_sha256", Monstey_BWFingerprint(part)}});
        for (auto &[key, value] : props.as_object())
            Monstey_BlendGeometrySetProperty(blend, block, key, value);
        collection_objects[sid].push_back(object);
    }
    for (auto &[sid, rows] : collection_objects)
        Monstey_BlendGeometryLinkObjects(blend, Monstey_BWBlock(collections.at(sid)), rows, true);
    auto cameras = Monstey_BWCameras(blend, scene);
    auto authored = Monstey_BlendGeometryObjects(blend);
    for (auto &object : authored.as_array()) {
        auto &props = object.at("properties");
        if (props.contains("Monstey_action"))
            Monstey_BlendGeometrySetProperty(blend, Monstey_BWBlock(object), "Monstey_display_matrix",
                                             Monstey_BWJson(Monstey_BWLocal(object)).dump());
        if (props.value("Monstey_kind", "").as_string() == "NATIVE_GEOMETRY_REFERENCE")
            Monstey_BlendGeometrySetProperty(blend, Monstey_BWBlock(object), "Monstey_reference_matrix",
                                             Monstey_BWJson(Monstey_BWLocal(object)).dump());
    }
    Monstey_BlendGeometryWriteText(blend, "Monstey_source_scene.json", scene.dump());
    Monstey_BlendGeometryWriteText(blend, "Monstey_world_metadata.json",
                                   J::object({{"scene_path", Monstey_BWText(scene_path)},
                                              {"scene_sha256", Monstey_HashSha256(scene_path)},
                                              {"template_sha256", Monstey_HashSha256(template_path)}})
                                       .dump());
    Monstey_BlendGeometryWriteText(
        blend, "README_EDITOR_V2",
        "Native world placements. Edit Monstey_properties_json, native transforms and explicit clone/add "
        "properties. Native geometry references remain source-qualified. No game writes.");
    inputs.Output(output);
    inputs.Verify();
    Monstey_BWCreated created(output);
    auto target = output / "WorldEditor.blend";
    auto written = blend.Write(target);
    created.Track(target);
    J::Array renders;
    if (a.value("render", false).as_bool()) {
        auto configured =
            a.contains("blender") && a.at("blender").is_string() && !a.at("blender").as_string().empty();
        auto exe = configured ? Monstey_BWPath(a.at("blender")) : std::filesystem::path{};
        if (!configured) {
            std::wstring environment(32768, L'\0');
            auto n = GetEnvironmentVariableW(L"MONSTEY_BLENDER", environment.data(),
                                             static_cast<DWORD>(environment.size()));
            if (n > 0 && n < environment.size()) {
                environment.resize(n);
                exe = std::filesystem::absolute(std::filesystem::path(environment));
            } else {
                exe = template_path.parent_path();
                while (!exe.empty() &&
                       !std::filesystem::is_regular_file(Monstey_WindowsFilePath(exe / L"blender.exe"))) {
                    auto parent = exe.parent_path();
                    if (parent == exe) {
                        exe.clear();
                        break;
                    }
                    exe = parent;
                }
                Monstey_Require(!exe.empty(), "Configure Blender to render native world references");
                exe /= L"blender.exe";
            }
        }
        auto run = [&](const std::filesystem::path &file, const char *label) {
            auto prefix = output / (std::string(label) + "-"),
                 png = output / (std::string(label) + "-0001.png"),
                 log = output / (std::string(label) + ".log");
            created.Track(png);
            created.Track(log);
            Monstey_BWRender(exe, file, prefix, log);
            renders.push_back(Monstey_BWRecord(png, Monstey_ReadFile(png)));
        };
        run(target, "perspective");
        auto scenes = blend.Blocks("SC").at("rows");
        Monstey_BWSet(blend, Monstey_BWBlock(scenes[0]), "camera", Monstey_BWAddress(cameras.second));
        auto top = output / "WorldEditor-top.blend";
        blend.Write(top);
        created.Track(top);
        run(top, "top");
    }
    inputs.Verify();
    auto report = J::object({{"schema", scene.at("schema")},
                             {"blend", Monstey_BWRecord(target, Monstey_ReadFile(target))},
                             {"objects", static_cast<std::uint64_t>(objects.size())},
                             {"native_geometry_objects", geometry.size()},
                             {"native_cpp", true},
                             {"native_blend_validation", written},
                             {"render_requested", a.value("render", false)},
                             {"reference_renders", renders},
                             {"game_modified", false}});
    Monstey_WriteNewText(output / "build-report.json", report.dump(2) + "\n");
    created.Track(output / "build-report.json");
    inputs.Verify();
    created.done = true;
    return report;
}
J Monstey_BWExport(const J &a) {
    Monstey_BWKeys(a, {"blend", "output_dir"});
    auto path = Monstey_BWPath(a.at("blend")), output = Monstey_BWPath(a.at("output_dir"));
    Monstey_BWInputs inputs;
    inputs.Load(path);
    Monstey_Blend blend(path);
    auto scene = J::Parse(Monstey_BlendGeometryReadText(blend, "Monstey_source_scene.json"));
    auto metadata = J::Parse(Monstey_BlendGeometryReadText(blend, "Monstey_world_metadata.json"));
    Monstey_Require(scene.at("schema").as_string() == "Monstey_WorldBlenderScene_2",
                    "Unsupported embedded world scene");
    inputs.Scene(scene);
    auto scene_path = Monstey_BWPath(metadata.at("scene_path"));
    auto raw_scene = inputs.Load(scene_path);
    Monstey_Require(
        Monstey_HashSha256(raw_scene) == metadata.at("scene_sha256").as_string() &&
            Monstey_BWNormalize(J::Parse(std::string(raw_scene.begin(), raw_scene.end())), inputs) == scene,
        "Embedded original scene or external source changed");
    auto objects = Monstey_BlendGeometryObjects(blend);
    std::map<std::uint64_t, J> addresses;
    for (auto &object : objects.as_array())
        addresses.emplace(object.at("address").as_uint(), object);
    std::map<std::uint64_t, M> world;
    std::set<std::uint64_t> visiting;
    std::function<M(std::uint64_t)> matrix = [&](std::uint64_t address) {
        if (world.contains(address))
            return world.at(address);
        Monstey_Require(addresses.contains(address) && visiting.insert(address).second,
                        "Missing or cyclic Blender object parent");
        auto &object = addresses.at(address);
        auto m = Monstey_BWLocal(object);
        auto parent = object.at("parent").as_uint();
        if (parent)
            m = Monstey_BWMul(Monstey_BWMul(matrix(parent), Monstey_BWParentInverse(object.at("parentinv"))),
                              m);
        visiting.erase(address);
        world.emplace(address, m);
        return m;
    };
    std::map<std::string, J> lookup, active;
    for (auto &row : scene.at("objects").as_array())
        Monstey_Require(lookup.emplace(row.at("id").as_string(), row).second,
                        "Duplicate embedded native identity");
    for (auto &object : objects.as_array()) {
        auto &props = object.at("properties");
        if (props.contains("Monstey_action") && props.at("Monstey_action").as_string() == "existing") {
            auto id = props.at("Monstey_id").as_string();
            Monstey_Require(lookup.contains(id) && active.emplace(id, object).second,
                            "Duplicate or unknown native object identity; use explicit clone action");
        }
    }
    auto geometry = scene.value("geometry", J::array());
    std::map<std::string, J> geometry_lookup;
    for (auto &row : geometry.as_array())
        geometry_lookup.emplace(row.at("id").as_string(), row);
    std::set<std::string> references;
    J::Array warnings;
    J::Object patches;
    std::uint64_t updates = 0, clones = 0, adds = 0, deletes = 0;
    auto append = [&](const std::string &sid, const char *key, const J &entry) {
        if (!patches.contains(sid))
            patches.emplace(
                sid, J::object({{"source_sha256", scene.at("sources").at(sid).at("fixture").at("sha256")}}));
        if (!patches.at(sid).contains(key))
            patches.at(sid)[key] = J::array();
        patches.at(sid)[key].as_array().push_back(entry);
    };
    auto native = [&](const J &object, const J &source, const J *row) -> J {
        auto &props = object.at("properties");
        auto m = matrix(object.at("address").as_uint());
        if (row && Monstey_BWSameMatrix(
                       m, Monstey_BWMatrix(Monstey_BWStored(props, "Monstey_display_matrix", J{})))) {
            auto result = J::object({{"position", row->at("transform").at("position_cm")},
                                     {"quaternion", row->at("transform").at("quaternion_xyzw")},
                                     {"scale", row->at("transform").at("uniform_scale")},
                                     {"unchanged", true}});
            return result;
        }
        auto t = Monstey_BWDecompose(Monstey_BWNative(m, scene, source));
        t["unchanged"] = false;
        if (row) {
            double dot = 0;
            for (unsigned i = 0; i < 4; ++i)
                dot += t.at("quaternion")[i].as_number() *
                       row->at("transform").at("quaternion_xyzw")[i].as_number();
            if (dot < 0)
                for (unsigned i = 0; i < 4; ++i)
                    t["quaternion"][i] = -t.at("quaternion")[i].as_number();
        }
        return t;
    };
    auto properties_delta = [&](const J &props, const J &row) {
        Monstey_Require(Monstey_BWStored(props, "Monstey_unknown_fields_json", J::array()) ==
                            Monstey_BWUnknown(row),
                        "Unknown native property metadata is read-only");
        auto values = Monstey_BWStored(props, "Monstey_properties_json", J::object());
        Monstey_Require(values.is_object(), "Native properties must be a JSON object");
        auto old = row.value("properties", J::object());
        J::Object changed;
        J::Array removed;
        for (auto &[key, value] : values.as_object())
            if (!old.contains(key) || old.at(key) != value)
                changed.emplace(key, value);
        for (auto &[key, value] : old.as_object()) {
            (void)value;
            if (!values.contains(key))
                removed.push_back(key);
        }
        return std::pair{J(changed), J(removed)};
    };
    for (auto &object : objects.as_array()) {
        auto &props = object.at("properties");
        auto kind = props.value("Monstey_kind", "").as_string();
        if (kind == "NATIVE_GEOMETRY_REFERENCE") {
            auto id = props.at("Monstey_geometry_id").as_string();
            Monstey_Require(geometry_lookup.contains(id) && references.insert(id).second,
                            "Duplicate or unknown native geometry reference");
            auto &row = geometry_lookup.at(id);
            auto parent = row.value("parent_object_id", J{});
            if (!parent.is_null() && !active.contains(parent.as_string())) {
                warnings.push_back(
                    J::object({{"object", object.at("name")},
                               {"reason", "Display geometry belongs to a deleted placement"}}));
                continue;
            }
            Monstey_Require(object.at("type").as_int() == 1, "Native reference is no longer a mesh");
            auto expected_parent =
                parent.is_null() ? 0 : active.at(parent.as_string()).at("address").as_uint();
            Monstey_Require(object.at("parent").as_uint() == expected_parent,
                            "Native reference parent changed independently");
            auto baseline = Monstey_BWGeometry(row, inputs);
            auto mesh = blend.BlockAtAddress(object.at("data").as_uint(), Monstey_BWBlock(object));
            auto edited = Monstey_BlendGeometryReadMesh(blend, mesh, baseline);
            Monstey_Require(
                Monstey_BWFingerprint(edited) == props.at("Monstey_geometry_sha256").as_string(),
                "Native reference mesh was edited; convert geometry through the native mesh tool");
            Monstey_Require(Monstey_BWSameMatrix(
                                Monstey_BWLocal(object),
                                Monstey_BWMatrix(Monstey_BWStored(props, "Monstey_reference_matrix", J{}))),
                            "Native reference transform changed independently of placement");
            Monstey_Require(blend.ReadField(mesh, "totcol").as_uint() == 0,
                            "Reference Blender material edits need an explicit native material conversion");
            continue;
        }
        if (!props.contains("Monstey_action")) {
            if (props.value("Monstey_template_orphan", 0).as_int() == 1)
                continue;
            if (object.at("type").as_int() == 1 || object.at("type").as_int() == 10)
                warnings.push_back(J::object(
                    {{"object", object.at("name")},
                     {"reason", "Unregistered Blender mesh or light has no native conversion contract"}}));
            continue;
        }
        auto action = props.at("Monstey_action").as_string(), sid = props.at("Monstey_source_id").as_string();
        Monstey_Require((kind == "OBJRES" || kind == "INST") &&
                            (action == "existing" || action == "clone" || action == "add") &&
                            scene.at("sources").contains(sid) &&
                            scene.at("sources").at(sid).at("backend").as_string() == kind,
                        "Unknown native action or source/backend mismatch");
        auto &source = scene.at("sources").at(sid);
        if (action == "existing") {
            auto &row = lookup.at(props.at("Monstey_id").as_string());
            Monstey_Require(row.at("source_id").as_string() == sid && row.at("backend").as_string() == kind,
                            "Native identity belongs to another source");
            auto t = native(object, source, &row);
            if (kind == "OBJRES") {
                Monstey_Require(props.at("Monstey_native_name") == row.at("name") &&
                                    props.at("Monstey_class_name") == row.at("class_name"),
                                "Native rename/retype needs an explicit clone action");
                Monstey_Require(std::abs(t.at("scale").as_number() - 1) < 1e-5,
                                "OBJRES does not serialize scale");
                auto [changed, removed] = properties_delta(props, row);
                auto entry = J::object({{"name", row.at("name")},
                                        {"group_index", row.at("group_index")},
                                        {"object_index", row.at("object_index")},
                                        {"class_name", row.at("class_name")}});
                if (!t.at("unchanged").as_bool()) {
                    entry["position"] = t.at("position");
                    entry["quaternion"] = t.at("quaternion");
                }
                if (!changed.empty())
                    entry["properties"] = changed;
                if (!removed.empty())
                    entry["remove_properties"] = removed;
                if (!t.at("unchanged").as_bool() || !changed.empty() || !removed.empty()) {
                    append(sid, "updates", entry);
                    ++updates;
                }
            } else if (!t.at("unchanged").as_bool()) {
                auto entry = J::object({{"index", row.at("instance_index")},
                                        {"position", t.at("position")},
                                        {"quaternion", t.at("quaternion")},
                                        {"scale", t.at("scale")}});
                if (props.contains("Monstey_owner_definition_key"))
                    entry["owner_definition_key"] = props.at("Monstey_owner_definition_key");
                append(sid, "updates", entry);
                ++updates;
            }
        } else if (action == "clone") {
            auto clone_source = props.at("Monstey_clone_source_id").as_string();
            Monstey_Require(lookup.contains(clone_source),
                            "Explicit clone lacks its original native identity");
            auto &row = lookup.at(clone_source);
            Monstey_Require(row.at("source_id").as_string() == sid && row.at("backend").as_string() == kind,
                            "Cross-source clone needs explicit native addition");
            auto t = native(object, source, nullptr);
            J entry;
            if (kind == "OBJRES") {
                Monstey_Require(props.at("Monstey_native_name") != row.at("name") &&
                                    std::abs(t.at("scale").as_number() - 1) < 1e-5,
                                "Object clone needs a new native name and unit scale");
                entry = J::object({{"source_name", row.at("name")},
                                   {"source_group_index", row.at("group_index")},
                                   {"source_object_index", row.at("object_index")},
                                   {"name", props.at("Monstey_native_name")},
                                   {"position", t.at("position")},
                                   {"quaternion", t.at("quaternion")}});
                auto [changed, removed] = properties_delta(props, row);
                if (!changed.empty())
                    entry["properties"] = changed;
                if (!removed.empty())
                    entry["remove_properties"] = removed;
            } else {
                entry = J::object({{"index", row.at("instance_index")},
                                   {"position", t.at("position")},
                                   {"quaternion", t.at("quaternion")},
                                   {"scale", t.at("scale")}});
                if (props.contains("Monstey_owner_definition_key"))
                    entry["owner_definition_key"] = props.at("Monstey_owner_definition_key");
            }
            append(sid, "clones", entry);
            ++clones;
        } else {
            auto t = native(object, source, nullptr);
            J entry;
            if (kind == "OBJRES") {
                Monstey_Require(std::abs(t.at("scale").as_number() - 1) < 1e-5,
                                "OBJRES additions require unit scale");
                entry = J::object(
                    {{"name", props.at("Monstey_native_name")},
                     {"class_name", props.at("Monstey_class_name")},
                     {"position", t.at("position")},
                     {"quaternion", t.at("quaternion")},
                     {"properties", Monstey_BWStored(props, "Monstey_properties_json", J::object())}});
                if (props.contains("Monstey_schema_source"))
                    entry["schema_source"] = props.at("Monstey_schema_source");
                append(sid, "objects", entry);
            } else {
                entry = Monstey_BWStored(props, "Monstey_defined_instance_json", J{});
                Monstey_Require(entry.is_object(), "New prefab needs explicit native definition metadata");
                entry["position"] = t.at("position");
                entry["quaternion"] = t.at("quaternion");
                entry["scale"] = t.at("scale");
                append(sid, "defined_instances", entry);
                if (props.contains("Monstey_owner_donor_json")) {
                    auto donor = Monstey_BWStored(props, "Monstey_owner_donor_json", J{});
                    Monstey_Require(!patches.at(sid).contains("owner_donor") ||
                                        patches.at(sid).at("owner_donor") == donor,
                                    "One explicit owner donor per native source is required");
                    patches.at(sid)["owner_donor"] = donor;
                }
            }
            ++adds;
        }
    }
    for (auto &[id, row] : geometry_lookup) {
        auto parent = row.value("parent_object_id", J{});
        if (parent.is_null() || active.contains(parent.as_string()))
            Monstey_Require(references.contains(id),
                            "Reference mesh was deleted without deleting its native placement");
    }
    for (auto &[id, row] : lookup)
        if (!active.contains(id)) {
            auto sid = row.at("source_id").as_string();
            J entry;
            if (row.at("backend").as_string() == "OBJRES")
                entry = J::object({{"name", row.at("name")},
                                   {"group_index", row.at("group_index")},
                                   {"object_index", row.at("object_index")},
                                   {"class_name", row.at("class_name")}});
            else {
                Monstey_Require(row.at("owners").size() == 1,
                                "Instance deletion requires a unique native render owner");
                entry = J::object({{"index", row.at("instance_index")},
                                   {"owner_definition_key", row.at("owners")[0].at("definition_key")}});
            }
            append(sid, "deletes", entry);
            ++deletes;
        }
    J::Array sources;
    for (auto &[sid, spec] : patches) {
        auto &source = scene.at("sources").at(sid);
        sources.push_back(J::object({{"source_id", sid},
                                     {"backend", source.at("backend")},
                                     {"source", source.at("fixture")},
                                     {"resource_path", source.at("resource_path")},
                                     {"spec", spec}}));
    }
    auto plan = J::object(
        {{"schema", "Monstey_WorldBlenderEdits_2"},
         {"scene_sha256", metadata.at("scene_sha256")},
         {"sources", sources},
         {"counts",
          J::object({{"updates", updates}, {"clones", clones}, {"deletes", deletes}, {"adds", adds}})},
         {"warnings", warnings},
         {"units", "native centimetres / quaternion XYZW"},
         {"game_modified", false}});
    (void)Monstey_WorldEditCall("sow_worldedit_compile", J::object({{"plan", plan}}));
    inputs.Output(output);
    inputs.Verify();
    Monstey_BWCreated created(output);
    auto save = [&](const std::filesystem::path &p, const J &value) {
        Monstey_WriteNewText(p, value.dump(2) + "\n");
        created.Track(p);
    };
    save(output / "edits.json", plan);
    for (auto &row : sources)
        save(output / ("edits." + row.at("source_id").as_string() + ".spec.json"), row.at("spec"));
    inputs.Verify();
    created.done = true;
    return plan;
}
} // namespace
J Monstey_WorldBlenderCatalog() {
    auto path = J::object({{"type", "string"}, {"format", "path"}}), text = J::object({{"type", "string"}});
    auto make = [](J::Object properties, std::initializer_list<const char *> required) {
        J::Array keys;
        for (auto key : required)
            keys.push_back(key);
        return J::object({{"type", "object"},
                          {"properties", properties},
                          {"required", keys},
                          {"additionalProperties", false}});
    };
    J::Array rows;
    auto add = [&](const char *name, const char *description, const J &schema) {
        rows.push_back(J::object({{"name", name},
                                  {"description", description},
                                  {"status", "complete"},
                                  {"available", true},
                                  {"implemented", true},
                                  {"inputSchema", schema},
                                  {"implementation_language", "C++20"},
                                  {"runtime_verified", false}}));
    };
    add("sow_blender_prepare", "Prepare source-qualified native fortress placements from installed assets",
        make({{"region",
               J::object({{"type", "string"},
                          {"enum", J::array({"island", "mtnpass", "snowmtn", "volcano", "desert"})}})},
              {"output", path},
              {"game_root", path},
              {"oodle_dll", path},
              {"spec", J::object({{"type", "object"}})}},
             {"region", "output"}));
    add("sow_blender_attach_inst",
        "Attach native PRFB instances to a new scene JSON without changing the source",
        make({{"scene", path},
              {"source", path},
              {"resource_path", text},
              {"frame", J::object({{"type", "string"}, {"enum", J::array({"world", "local"})}})},
              {"frame_to_scene", J::object({{"type", "object"}})},
              {"output", path}},
             {"scene", "source", "resource_path", "frame", "output"}));
    add("sow_blender_build",
        "Author real static Blender SDNA objects, geometry, collections and source identities",
        make({{"scene", path},
              {"output_dir", path},
              {"render", J::object({{"type", "boolean"}})},
              {"blend_template", path},
              {"blender", path}},
             {"scene", "output_dir"}));
    add("sow_blender_export",
        "Read saved Blender SDNA edits and emit source-qualified native CRUD specifications",
        make({{"blend", path}, {"output_dir", path}}, {"blend", "output_dir"}));
    add("sow_blender_validate",
        "Validate Blender edit specs through native codecs without candidate file publication",
        make({{"spec", path}}, {"spec"}));
    return rows;
}
J Monstey_WorldBlenderCall(std::string_view name, const J &a) {
    if (name == "sow_blender_prepare")
        return Monstey_BWPrepare(a);
    if (name == "sow_blender_attach_inst")
        return Monstey_BWAttach(a);
    if (name == "sow_blender_build")
        return Monstey_BWBuild(a);
    if (name == "sow_blender_export")
        return Monstey_BWExport(a);
    if (name == "sow_blender_validate")
        return Monstey_BWValidate(a);
    throw Monstey_Error("world_blender_operation", "Unknown native World Blender operation");
}
} // namespace Monstey