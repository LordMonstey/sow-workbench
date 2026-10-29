#include "Monstey_BlendModel.hpp"
#include "Monstey_Blend.hpp"
#include "Monstey_BlendGeometry.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <map>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
void Monstey_BMRequire(bool ok, std::string_view text) {
    if (!ok)
        throw Monstey_Error("blend_model", std::string(text));
}
std::size_t Monstey_BMIndex(const J &row) {
    return static_cast<std::size_t>(row[row.contains("index") ? "index" : "block"].as_uint());
}
std::uint64_t Monstey_BMAddress(const J &row) { return row["address"].as_uint(); }
std::uint64_t Monstey_BMPtr(Monstey_Blend &b, std::size_t i, std::string_view f) {
    return b.ReadField(i, f)["pointer"].as_uint();
}
std::uint64_t Monstey_BMData(Monstey_Blend &b, B bytes) {
    return Monstey_BMAddress(b.NewData(std::move(bytes)));
}
std::uint64_t Monstey_BMString(Monstey_Blend &b, std::string_view text) {
    B bytes(text.begin(), text.end());
    bytes.push_back(0);
    return Monstey_BMData(b, std::move(bytes));
}
J Monstey_BMVec(std::initializer_list<double> v) {
    J j = J::array();
    for (auto x : v)
        j.as_array().push_back(x);
    return j;
}
std::string Monstey_BMHex(std::span<const std::uint8_t> b) {
    constexpr char h[] = "0123456789abcdef";
    std::string s;
    s.reserve(b.size() * 2);
    for (auto x : b) {
        s += h[x >> 4];
        s += h[x & 15];
    }
    return s;
}
B Monstey_BMUnhex(std::string_view s) {
    Monstey_BMRequire(s.size() % 2 == 0 && s.size() <= 512ull * 1024 * 1024, "Invalid embedded model size");
    B b(s.size() / 2);
    auto nib = [](char c) -> unsigned {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        throw Monstey_Error("blend_model", "Invalid embedded model hex");
    };
    for (std::size_t i = 0; i < b.size(); ++i)
        b[i] = static_cast<std::uint8_t>((nib(s[i * 2]) << 4) | nib(s[i * 2 + 1]));
    return b;
}
void Monstey_BMList(Monstey_Blend &b, std::size_t owner, std::string_view field, const std::vector<J> &rows) {
    const auto first = rows.empty() ? 0 : Monstey_BMAddress(rows.front()),
               last = rows.empty() ? 0 : Monstey_BMAddress(rows.back());
    b.SetField(owner, std::string(field) + ".first", first);
    b.SetField(owner, std::string(field) + ".last", last);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        b.SetField(Monstey_BMIndex(rows[i]), "prev", i ? Monstey_BMAddress(rows[i - 1]) : 0);
        b.SetField(Monstey_BMIndex(rows[i]), "next",
                   i + 1 < rows.size() ? Monstey_BMAddress(rows[i + 1]) : 0);
    }
}
void Monstey_BMId(Monstey_Blend &b, std::size_t i, std::string name) {
    b.SetField(i, "id.name", name);
    b.SetField(i, "id.us", 1);
}
J Monstey_BMObject(Monstey_Blend &b, std::string name, int type, std::uint64_t data) {
    auto object = Monstey_BlendGeometryCreateObject(b, name, type, data);
    b.SetField(Monstey_BMIndex(object), "rot", Monstey_BMVec({1.5707963267948966, 0, 0}));
    return object;
}
void Monstey_BMProperty(Monstey_Blend &b, std::size_t id, const std::string &name, int value) {
    Monstey_BlendGeometrySetProperty(b, id, name, value);
}
int Monstey_BMReadProperty(Monstey_Blend &b, std::size_t id, std::string_view name) {
    const auto properties = Monstey_BlendGeometryProperties(b, id);
    if (!properties.contains(name))
        return -1;
    Monstey_BMRequire(properties[name].is_integer(), "Invalid native identity property");
    return static_cast<int>(properties[name].as_int());
}
J Monstey_BMMetadata(const Monstey_ModelScene &scene) {
    J j = J::object({{"format", "Monstey_SoW_Model_v1"},
                     {"source_sha256", scene.source_sha256},
                     {"baseline_sha256", scene.baseline_sha256},
                     {"skeleton_sha256", scene.skeleton_sha256},
                     {"native_hex", Monstey_BMHex(scene.native_baseline)}});
    j["bone_names"] = J::array();
    for (const auto &bone : scene.bones)
        j["bone_names"].as_array().push_back(bone.name);
    return j;
}
void Monstey_BMText(Monstey_Blend &b, const J &metadata) {
    Monstey_BlendGeometryWriteText(b, "Monstey_SoW_Model", metadata.dump());
}
J Monstey_BMReadMetadata(Monstey_Blend &b) {
    auto j = J::Parse(Monstey_BlendGeometryReadText(b, "Monstey_SoW_Model"));
    Monstey_BMRequire(j["format"].as_string() == "Monstey_SoW_Model_v1",
                      "Unsupported native Blender metadata");
    return j;
}
} // namespace
} // namespace Monstey

namespace Monstey {
namespace {
using V = std::array<double, 3>;
V Monstey_BMSub(V a, V b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
V Monstey_BMAdd(V a, V b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
V Monstey_BMMul(V a, double x) { return {a[0] * x, a[1] * x, a[2] * x}; }
double Monstey_BMDot(V a, V b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double Monstey_BMLength(V a) { return std::sqrt(Monstey_BMDot(a, a)); }
V Monstey_BMNorm(V a) {
    auto n = Monstey_BMLength(a);
    Monstey_BMRequire(n > 1e-10, "Degenerate bone direction");
    return Monstey_BMMul(a, 1 / n);
}
V Monstey_BMCross(V a, V b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
J Monstey_BMV(V a) { return Monstey_BMVec({a[0], a[1], a[2]}); }
struct Monstey_BMBone {
    V head, tail, x, y, z;
    double length;
};
J Monstey_BMMat(const Monstey_BMBone &bone, bool global) {
    return global ? Monstey_BMVec({bone.x[0], bone.x[1], bone.x[2], 0, bone.y[0], bone.y[1], bone.y[2], 0,
                                   bone.z[0], bone.z[1], bone.z[2], 0, bone.head[0], bone.head[1],
                                   bone.head[2], 1})
                  : Monstey_BMVec({bone.x[0], bone.x[1], bone.x[2], bone.y[0], bone.y[1], bone.y[2],
                                   bone.z[0], bone.z[1], bone.z[2]});
}
V Monstey_BMParentLocal(V world, const Monstey_BMBone &p) {
    return {Monstey_BMDot(world, p.x), Monstey_BMDot(world, p.y), Monstey_BMDot(world, p.z)};
}
std::vector<Monstey_BMBone> Monstey_BMDisplayBones(const Monstey_ModelScene &scene) {
    std::vector<Monstey_BMBone> out;
    for (std::size_t k = 0; k < scene.bones.size(); ++k) {
        const auto &m = scene.bones[k].global_bind_metres;
        V h = {m[3], m[7], m[11]}, target{};
        double nearest = 1e20;
        for (const auto &child : scene.bones)
            if (child.parent_hk == static_cast<int>(k)) {
                const auto &c = child.global_bind_metres;
                V t = {c[3], c[7], c[11]};
                auto d = Monstey_BMLength(Monstey_BMSub(t, h));
                if (d > 0.002 && d < nearest) {
                    nearest = d;
                    target = t;
                }
            }
        if (nearest == 1e20) {
            const auto p = scene.bones[k].parent_hk;
            V dir =
                p < 0 ? V{0, 1, 0}
                      : Monstey_BMSub(h, V{scene.bones[static_cast<std::size_t>(p)].global_bind_metres[3],
                                           scene.bones[static_cast<std::size_t>(p)].global_bind_metres[7],
                                           scene.bones[static_cast<std::size_t>(p)].global_bind_metres[11]});
            if (Monstey_BMLength(dir) < 0.002)
                dir = {0, 1, 0};
            target = Monstey_BMAdd(h, Monstey_BMMul(Monstey_BMNorm(dir), 0.035));
        }
        auto y = Monstey_BMNorm(Monstey_BMSub(target, h));
        V up = {0, 0, 1};
        if (std::abs(Monstey_BMDot(y, up)) > .95)
            up = {1, 0, 0};
        auto x = Monstey_BMNorm(Monstey_BMCross(y, up)), z = Monstey_BMCross(x, y);
        out.push_back({h, target, x, y, z, Monstey_BMLength(Monstey_BMSub(target, h))});
    }
    return out;
}
J Monstey_BMArmature(Monstey_Blend &b, const Monstey_ModelScene &scene) {
    auto ar = b.NewBlock("AR", "bArmature");
    auto ai = Monstey_BMIndex(ar);
    Monstey_BMId(b, ai, "ARSoW_Native_Rig");
    b.SetField(ai, "layer", 1);
    b.SetField(ai, "deformflag", 1);
    b.SetField(ai, "drawtype", 2);
    auto display = Monstey_BMDisplayBones(scene);
    std::vector<J> bones;
    for (std::size_t k = 0; k < scene.bones.size(); ++k)
        bones.push_back(b.NewBlock("DATA", "Bone"));
    std::vector<J> roots;
    for (std::size_t k = 0; k < bones.size(); ++k) {
        auto i = Monstey_BMIndex(bones[k]);
        auto bone = display[k];
        const auto p = scene.bones[k].parent_hk;
        b.SetField(i, "name", scene.bones[k].name);
        b.SetField(i, "arm_head", Monstey_BMV(bone.head));
        b.SetField(i, "arm_tail", Monstey_BMV(bone.tail));
        b.SetField(i, "arm_mat", Monstey_BMMat(bone, true));
        b.SetField(i, "length", bone.length);
        b.SetField(i, "weight", 1);
        b.SetField(i, "xwidth", .01);
        b.SetField(i, "zwidth", .01);
        b.SetField(i, "rad_head", .01);
        b.SetField(i, "rad_tail", .005);
        b.SetField(i, "layer", 1);
        b.SetField(i, "segments", 1);
        b.SetField(i, "scale_in", Monstey_BMVec({1, 1, 1}));
        b.SetField(i, "scale_out", Monstey_BMVec({1, 1, 1}));
        b.SetField(i, "size", Monstey_BMVec({1, 1, 1}));
        if (p >= 0) {
            const auto &parent = display[static_cast<std::size_t>(p)];
            b.SetField(i, "parent", Monstey_BMAddress(bones[static_cast<std::size_t>(p)]));
            bone.head = Monstey_BMSub(Monstey_BMParentLocal(Monstey_BMSub(bone.head, parent.head), parent),
                                      V{0, parent.length, 0});
            bone.tail = Monstey_BMSub(Monstey_BMParentLocal(Monstey_BMSub(bone.tail, parent.head), parent),
                                      V{0, parent.length, 0});
            bone.x = Monstey_BMParentLocal(bone.x, parent);
            bone.y = Monstey_BMParentLocal(bone.y, parent);
            bone.z = Monstey_BMParentLocal(bone.z, parent);
        } else
            roots.push_back(bones[k]);
        b.SetField(i, "head", Monstey_BMV(bone.head));
        b.SetField(i, "tail", Monstey_BMV(bone.tail));
        b.SetField(i, "bone_mat", Monstey_BMMat(bone, false));
        const auto d = bone.y;
        V baseX{}, baseZ{};
        if (d[1] > -0.999999) {
            baseX = {1 - d[0] * d[0] / (1 + d[1]), -d[0], -d[0] * d[2] / (1 + d[1])};
            baseZ = {-d[0] * d[2] / (1 + d[1]), -d[2], 1 - d[2] * d[2] / (1 + d[1])};
        } else {
            baseX = {-1, 0, 0};
            baseZ = {0, 0, 1};
        }
        b.SetField(i, "roll", std::atan2(-Monstey_BMDot(bone.x, baseZ), Monstey_BMDot(bone.x, baseX)));
        std::vector<J> children;
        for (std::size_t c = 0; c < scene.bones.size(); ++c)
            if (scene.bones[c].parent_hk == static_cast<int>(k))
                children.push_back(bones[c]);
        Monstey_BMList(b, i, "childbase", children);
    }
    Monstey_BMList(b, ai, "bonebase", roots);
    auto object = Monstey_BMObject(b, "SoW_Native_Rig", 25, Monstey_BMAddress(ar));
    b.SetField(Monstey_BMIndex(object), "dtx", 32);
    return object;
}
void Monstey_BMWeights(Monstey_Blend &b, std::size_t mesh, const Monstey_ModelPart &part,
                       const Monstey_ModelScene &scene) {
    std::vector<J> groups;
    for (const auto &bone : scene.bones) {
        auto g = b.NewBlock("DATA", "bDeformGroup");
        b.SetField(Monstey_BMIndex(g), "name", bone.name);
        groups.push_back(g);
    }
    Monstey_BMList(b, mesh, "vertex_group_names", groups);
    auto count = part.channels.at("POSITION").values.size() / 3;
    auto verts = b.NewBlock("DATA", "MDeformVert", count);
    auto vi = Monstey_BMIndex(verts);
    auto offset = [&](std::string_view type, std::string_view field) {
        const auto fields = b.Fields(type);
        for (const auto &f : fields.as_array())
            if (f["name"].as_string() == field)
                return static_cast<std::size_t>(f["offset"].as_uint());
        throw Monstey_Error("blend_model", "Missing deformation SDNA field");
    };
    const auto vertexStride = static_cast<std::size_t>(verts["bytes"].as_uint()) / count;
    const auto dwOffset = offset("MDeformVert", "dw"), countOffset = offset("MDeformVert", "totweight");
    const auto jointOffset = offset("MDeformWeight", "def_nr"),
               weightOffset = offset("MDeformWeight", "weight");
    auto vertexBytes = b.BlockData(vi);
    for (std::size_t v = 0; v < count; ++v) {
        std::map<int, int> merged;
        for (unsigned k = 0; k < 4; ++k) {
            const auto weight = static_cast<int>(part.channels.at("WEIGHTS_U8").values[v * 4 + k]);
            if (weight)
                merged[static_cast<int>(part.channels.at("JOINTS").values[v * 4 + k])] += weight;
        }
        auto weights = b.NewBlock("DATA", "MDeformWeight", merged.size());
        const auto wi = Monstey_BMIndex(weights);
        auto weightBytes = b.BlockData(wi);
        const auto weightStride = weightBytes.size() / merged.size();
        std::size_t cursor = 0;
        for (const auto &[joint, weight] : merged) {
            Monstey_PutU32(weightBytes, cursor * weightStride + jointOffset,
                           static_cast<std::uint32_t>(joint));
            Monstey_PutU32(weightBytes, cursor * weightStride + weightOffset,
                           std::bit_cast<std::uint32_t>(static_cast<float>(weight / 255.0)));
            ++cursor;
        }
        b.ReplaceTypedData(wi, std::move(weightBytes));
        Monstey_PutU64(vertexBytes, v * vertexStride + dwOffset, Monstey_BMAddress(weights));
        Monstey_PutU32(vertexBytes, v * vertexStride + countOffset,
                       static_cast<std::uint32_t>(merged.size()));
    }
    b.ReplaceTypedData(vi, std::move(vertexBytes));
    auto layer = b.NewBlock("DATA", "CustomDataLayer");
    const auto li = Monstey_BMIndex(layer);
    b.SetField(li, "type", 2);
    b.SetField(li, "data", Monstey_BMAddress(verts));
    b.SetField(mesh, "vdata.layers", Monstey_BMAddress(layer));
    b.SetField(mesh, "vdata.totlayer", 1);
    b.SetField(mesh, "vdata.maxlayer", 1);
    b.SetField(mesh, "vdata.totsize", 16);
    J types = J::array();
    for (unsigned k = 0; k < 53; ++k)
        types.as_array().push_back(k == 2 ? 0 : -1);
    b.SetField(mesh, "vdata.typemap", types);
}
J Monstey_BMMesh(Monstey_Blend &b, const Monstey_ModelPart &part, const Monstey_ModelScene &scene,
                 const J &arm) {
    const auto name = "SoW_C" + std::to_string(part.container) + "_P" + std::to_string(part.part);
    const auto mesh = Monstey_BlendGeometryCreateMesh(b, part, name, .01);
    const auto mi = Monstey_BMIndex(mesh);
    Monstey_BMWeights(b, mi, part, scene);
    Monstey_BMProperty(b, mi, "sow_container", static_cast<int>(part.container));
    Monstey_BMProperty(b, mi, "sow_part", static_cast<int>(part.part));
    auto object = Monstey_BMObject(b, name, 1, Monstey_BMAddress(mesh));
    auto modifier = b.NewBlock("DATA", "ArmatureModifierData");
    const auto modi = Monstey_BMIndex(modifier);
    b.SetField(modi, "modifier.type", 8);
    b.SetField(modi, "modifier.mode", 3);
    b.SetField(modi, "modifier.name", "SoW Native Rig");
    b.SetField(modi, "deformflag", 1);
    b.SetField(modi, "object", Monstey_BMAddress(arm));
    b.SetField(Monstey_BMIndex(object), "modifiers.first", Monstey_BMAddress(modifier));
    b.SetField(Monstey_BMIndex(object), "modifiers.last", Monstey_BMAddress(modifier));
    return object;
}
} // namespace
Monstey_Json Monstey_ModelImportNative(const std::filesystem::path &mesh,
                                       const std::filesystem::path &outputBlend,
                                       const std::filesystem::path &skel, const J &options) {
    const auto templatePath = Monstey_BlendGeometryTemplate(options);
    auto scene = Monstey_ModelReadScene(mesh, skel);
    if (options.contains("expected_source_sha256"))
        Monstey_BMRequire(scene.source_sha256 == options["expected_source_sha256"].as_string(),
                          "Source snapshot SHA256 mismatch");
    if (options.contains("expected_skeleton_sha256") &&
        (skel.empty() || scene.skeleton_sha256 != options["expected_skeleton_sha256"].as_string()))
        throw Monstey_Error("source_hash_mismatch", "Skeleton snapshot differs from expected SHA256");
    Monstey_Blend b(templatePath);
    Monstey_BMRequire(b.Describe()["blender_file_version"].as_uint() == 500,
                      "Blender model authoring currently requires qualified v0500 SDNA");
    auto arm = Monstey_BMArmature(b, scene);
    std::vector<J> objects = {arm};
    for (const auto &part : scene.parts)
        objects.push_back(Monstey_BMMesh(b, part, scene, arm));
    const auto collections = b.Blocks("GR")["rows"];
    Monstey_BMRequire(!collections.empty(), "Blender template has no collection");
    std::size_t collection = Monstey_BMIndex(collections[0u]);
    std::vector<J> links;
    for (const auto &object : objects) {
        auto link = b.NewBlock("DATA", "CollectionObject");
        b.SetField(Monstey_BMIndex(link), "ob", Monstey_BMAddress(object));
        b.SetDataOwner(Monstey_BMIndex(link), collection);
        links.push_back(link);
    }
    Monstey_BMList(b, collection, "gobject", links);
    Monstey_BMText(b, Monstey_BMMetadata(scene));
    auto report = b.Write(outputBlend);
    report["operation"] = "model_import_native";
    report["source_sha256"] = scene.source_sha256;
    report["blend_template"] = Monstey_Utf8(templatePath.wstring());
    report["blend_template_sha256"] = b.Describe()["source_sha256"];
    report["skeleton_sha256"] = scene.skeleton_sha256;
    report["parts"] = static_cast<std::uint64_t>(scene.parts.size());
    report["bones"] = static_cast<std::uint64_t>(scene.bones.size());
    report["native_runtime_verified"] = false;
    return report;
}
} // namespace Monstey

namespace Monstey {
namespace {
std::vector<J> Monstey_BMFollow(Monstey_Blend &b, std::uint64_t pointer, std::size_t owner,
                                std::string_view nextField = "next") {
    std::vector<J> out;
    std::set<std::uint64_t> seen;
    while (pointer) {
        Monstey_BMRequire(seen.insert(pointer).second, "Cyclic Blender linked list");
        auto i = b.BlockAtAddress(pointer, owner);
        out.push_back(J::object({{"index", static_cast<std::uint64_t>(i)}, {"address", pointer}}));
        pointer = Monstey_BMPtr(b, i, nextField);
    }
    return out;
}
void Monstey_BMValidateRig(Monstey_Blend &b, const Monstey_ModelScene &scene) {
    const auto arms = b.Blocks("AR");
    std::size_t arm = static_cast<std::size_t>(-1);
    for (const auto &r : arms["rows"].as_array()) {
        auto i = Monstey_BMIndex(r);
        if (b.ReadField(i, "id.name").as_string() == "ARSoW_Native_Rig") {
            Monstey_BMRequire(arm == static_cast<std::size_t>(-1), "Native rig is duplicated");
            arm = i;
        }
    }
    Monstey_BMRequire(arm != static_cast<std::size_t>(-1), "Native armature datablock missing");
    const auto display = Monstey_BMDisplayBones(scene);
    std::map<std::string, std::size_t> names;
    for (std::size_t k = 0; k < scene.bones.size(); ++k)
        Monstey_BMRequire(names.emplace(scene.bones[k].name, k).second, "Native bone names collide");
    std::set<std::uint64_t> seen;
    std::function<void(std::uint64_t, int)> walk = [&](std::uint64_t ptr, int parent) {
        while (ptr) {
            Monstey_BMRequire(seen.insert(ptr).second, "Native rig contains cyclic/repeated bones");
            const auto i = b.BlockAtAddress(ptr, arm);
            const auto name = b.ReadField(i, "name").as_string();
            Monstey_BMRequire(names.contains(name), "Authored bone does not match native rig");
            const auto k = names.at(name);
            Monstey_BMRequire(scene.bones[k].parent_hk == parent,
                              "Native bone hierarchy was edited; explicit skeleton generation required");
            const auto head = b.ReadField(i, "arm_head"), tail = b.ReadField(i, "arm_tail"),
                       matrix = b.ReadField(i, "arm_mat"), expected = Monstey_BMMat(display[k], true);
            for (unsigned c = 0; c < 3; ++c)
                Monstey_BMRequire(std::isfinite(head[c].as_number()) && std::isfinite(tail[c].as_number()) &&
                                      std::abs(head[c].as_number() - display[k].head[c]) < 3e-5 &&
                                      std::abs(tail[c].as_number() - display[k].tail[c]) < 3e-5,
                                  "Native bone rest position was edited; explicit retargeting required");
            for (unsigned c = 0; c < 16; ++c)
                Monstey_BMRequire(std::isfinite(matrix[c].as_number()) &&
                                      std::abs(matrix[c].as_number() - expected[c].as_number()) < 3e-5,
                                  "Native bone rest basis was edited; explicit retargeting required");
            walk(Monstey_BMPtr(b, i, "childbase.first"), static_cast<int>(k));
            ptr = Monstey_BMPtr(b, i, "next");
        }
    };
    walk(Monstey_BMPtr(b, arm, "bonebase.first"), -1);
    Monstey_BMRequire(seen.size() == scene.bones.size(), "Native rig lost bones");
}
void Monstey_BMValidateObjects(Monstey_Blend &b) {
    const auto objects = b.Blocks("OB", 0, 10000);
    std::set<std::uint64_t> meshes;
    for (const auto &row : objects["rows"].as_array()) {
        const auto object = Monstey_BMIndex(row);
        if (b.ReadField(object, "type").as_int() != 1)
            continue;
        const auto ptr = Monstey_BMPtr(b, object, "data");
        if (!ptr)
            continue;
        const auto mesh = b.BlockAtAddress(ptr, object);
        if (Monstey_BMReadProperty(b, mesh, "sow_container") < 0)
            continue;
        Monstey_BMRequire(meshes.insert(ptr).second,
                          "Native mesh is instanced; make its part identity explicit");
        Monstey_BMRequire(Monstey_BMPtr(b, mesh, "key") == 0, "Apply native mesh shape keys before export");
        Monstey_BMRequire(Monstey_BMPtr(b, object, "parent") == 0,
                          "Parented native objects require an explicit transform bake");
        const auto location = b.ReadField(object, "loc"), scale = b.ReadField(object, "size"),
                   rotation = b.ReadField(object, "rot");
        for (unsigned k = 0; k < 3; ++k)
            Monstey_BMRequire(
                std::abs(location[k].as_number()) < 1e-7 && std::abs(scale[k].as_number() - 1) < 1e-7 &&
                    std::abs(rotation[k].as_number() - (k == 0 ? 1.5707963267948966 : 0)) < 1e-6,
                "Native object transform differs from its import frame; bake the edit into mesh coordinates");
        for (const auto &modifier :
             Monstey_BMFollow(b, Monstey_BMPtr(b, object, "modifiers.first"), object, "modifier.next"))
            Monstey_BMRequire(b.ReadField(Monstey_BMIndex(modifier), "modifier.type").as_int() == 8,
                              "Apply non-armature modifiers before native export");
    }
}
void Monstey_BMReadWeights(Monstey_Blend &b, std::size_t mesh, Monstey_ModelPart &part,
                           const Monstey_ModelScene &scene) {
    const auto groups = Monstey_BMFollow(b, Monstey_BMPtr(b, mesh, "vertex_group_names.first"), mesh);
    std::vector<std::size_t> map;
    for (const auto &g : groups) {
        const auto name = b.ReadField(Monstey_BMIndex(g), "name").as_string();
        auto it = std::find_if(scene.bones.begin(), scene.bones.end(),
                               [&](const auto &x) { return x.name == name; });
        Monstey_BMRequire(it != scene.bones.end(), "Vertex group does not map to native HK rig");
        map.push_back(it->hk_index);
    }
    auto layerPtr = Monstey_BMPtr(b, mesh, "vdata.layers");
    const auto layerCount = b.ReadField(mesh, "vdata.totlayer").as_uint();
    Monstey_BMRequire(layerPtr && layerCount < 1024, "Native deformation weights missing");
    const auto layers = b.BlockAtAddress(layerPtr, mesh);
    std::uint64_t dvPtr = 0;
    for (std::size_t k = 0; k < layerCount; ++k)
        if (b.ReadField(layers, "type", k).as_int() == 2)
            dvPtr = b.ReadField(layers, "data", k)["pointer"].as_uint();
    Monstey_BMRequire(dvPtr != 0, "Blender deform vertices missing");
    const auto count = part.channels.at("POSITION").values.size() / 3;
    const auto dv = b.BlockAtAddress(dvPtr, layers);
    auto &joints = part.channels.at("JOINTS").values;
    auto &weights = part.channels.at("WEIGHTS_U8").values;
    for (std::size_t v = 0; v < count; ++v) {
        const auto wc = b.ReadField(dv, "totweight", v).as_uint();
        Monstey_BMRequire(wc <= 4096, "Deform influence limit");
        auto bytes = wc ? b.BytesAtAddress(b.ReadField(dv, "dw", v)["pointer"].as_uint(),
                                           static_cast<std::size_t>(wc) * 8, dv)
                        : B{};
        std::map<std::size_t, double> merged;
        for (std::size_t k = 0; k < wc; ++k) {
            auto group = Monstey_U32(bytes, k * 8);
            auto weight = static_cast<double>(std::bit_cast<float>(Monstey_U32(bytes, k * 8 + 4)));
            Monstey_BMRequire(group < map.size() && std::isfinite(weight) && weight >= 0,
                              "Invalid authored weight/group");
            if (weight > 1e-8)
                merged[map[group]] += weight;
        }
        Monstey_BMRequire(!merged.empty() && merged.size() <= 4,
                          "Native model requires one to four nonzero influences per vertex");
        std::map<std::size_t, double> old;
        for (unsigned k = 0; k < 4; ++k)
            if (weights[v * 4 + k])
                old[static_cast<std::size_t>(joints[v * 4 + k])] += weights[v * 4 + k] / 255.0;
        bool same = old.size() == merged.size();
        for (auto [joint, weight] : merged)
            if (!old.contains(joint) || std::abs(old[joint] - weight) > 1e-6)
                same = false;
        if (same)
            continue;
        double total = 0;
        for (auto [joint, weight] : merged) {
            (void)joint;
            total += weight;
        }
        struct W {
            std::size_t joint;
            int byte;
            double remainder;
        };
        std::vector<W> out;
        int sum = 0;
        for (auto [joint, weight] : merged) {
            const auto scaled = weight / total * 255;
            const int byte = static_cast<int>(std::floor(scaled));
            out.push_back({joint, byte, scaled - byte});
            sum += byte;
        }
        std::stable_sort(out.begin(), out.end(),
                         [](const auto &a, const auto &c) { return a.remainder > c.remainder; });
        for (int k = 0; k < 255 - sum; ++k)
            ++out[static_cast<std::size_t>(k) % out.size()].byte;
        for (unsigned k = 0; k < 4; ++k) {
            joints[v * 4 + k] = k < out.size() ? static_cast<double>(out[k].joint) : 0;
            weights[v * 4 + k] = k < out.size() ? out[k].byte : 0;
        }
    }
}
} // namespace
Monstey_ModelScene Monstey_ModelReadBlendScene(const std::filesystem::path &blend) {
    return Monstey_ModelReadBlendScene(blend, J::object());
}
Monstey_ModelScene Monstey_ModelReadBlendScene(const std::filesystem::path &blend, const J &options) {
    std::optional<std::uint8_t> triangleBand;
    if (options.contains("triangle_band")) {
        const auto value = options["triangle_band"].as_uint();
        Monstey_BMRequire(value <= 16, "Explicit triangle_band must be ordinal 0-16");
        triangleBand = static_cast<std::uint8_t>(value);
    }
    Monstey_Blend b(blend);
    const auto meta = Monstey_BMReadMetadata(b);
    auto raw = Monstey_BMUnhex(meta["native_hex"].as_string());
    Monstey_BMRequire(Monstey_HashSha256(raw) == meta["baseline_sha256"].as_string(),
                      "Embedded native baseline SHA256 mismatch");
    auto scene = Monstey_ModelReadSceneBytes(raw);
    scene.source_sha256 = b.Describe()["source_sha256"].as_string();
    scene.skeleton_sha256 = meta["skeleton_sha256"].as_string();
    Monstey_BMRequire(meta["bone_names"].size() == scene.bones.size(), "Metadata bone count changed");
    for (std::size_t i = 0; i < scene.bones.size(); ++i)
        scene.bones[i].name = meta["bone_names"][i].as_string();
    Monstey_BMValidateObjects(b);
    Monstey_BMValidateRig(b, scene);
    scene.position_projection = Monstey_ModelProjection::Float32Metres;
    std::set<std::pair<std::size_t, std::size_t>> seen;
    const auto meshBlocks = b.Blocks("ME");
    for (const auto &row : meshBlocks["rows"].as_array()) {
        const auto mi = Monstey_BMIndex(row);
        const auto container = Monstey_BMReadProperty(b, mi, "sow_container"),
                   partId = Monstey_BMReadProperty(b, mi, "sow_part");
        if (container < 0 || partId < 0)
            continue;
        const auto key = std::pair{static_cast<std::size_t>(container), static_cast<std::size_t>(partId)};
        Monstey_BMRequire(seen.insert(key).second, "Blender repeats native part identity");
        auto p = std::find_if(scene.parts.begin(), scene.parts.end(), [&](const auto &x) {
            return x.container == key.first && x.part == key.second;
        });
        Monstey_BMRequire(p != scene.parts.end(), "Unknown native Blender part");
        *p = Monstey_BlendGeometryReadMesh(
            b, mi, *p, 100, [&](Monstey_ModelPart &points) { Monstey_BMReadWeights(b, mi, points, scene); },
            triangleBand);
    }
    Monstey_BMRequire(seen.size() == scene.parts.size(),
                      "Blender lost native part identity; retain all native containers");
    return scene;
}
Monstey_Json Monstey_ModelExportNative(const std::filesystem::path &blend,
                                       const std::filesystem::path &nativeTemplate,
                                       const std::filesystem::path &outputMesh, const J &options) {
    auto scene = Monstey_ModelReadBlendScene(blend, options);
    if (options.contains("expected_source_sha256"))
        Monstey_BMRequire(scene.source_sha256 == options["expected_source_sha256"].as_string(),
                          "Blender source SHA256 mismatch");
    auto writerOptions = options;
    writerOptions.as_object().erase("expected_source_sha256");
    auto result = Monstey_ModelWriteScene(scene, nativeTemplate, outputMesh, writerOptions);
    result["operation"] = "model_export_native";
    result["blender_source"] = Monstey_Utf8(blend.wstring());
    return result;
}
} // namespace Monstey
