#include "Monstey_WorldEdit.hpp"
#include "Monstey_Archive.hpp"
#include "Monstey_Navigation.hpp"
#include "Monstey_PhysicsWorld.hpp"
#include "Monstey_World.hpp"
#include "Monstey_WorldComposition.hpp"
#include "Monstey_AssetInspect.hpp"
#include "Monstey_Havok.hpp"
#include "Monstey_Physics.hpp"
#include "Monstey_LtmiMaterial.hpp"
#include "Monstey_TextureResource.hpp"
#include "Monstey_WorldLighting.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <set>
namespace Monstey {
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
std::filesystem::path Monstey_EPath(const J &j) {
    return std::filesystem::absolute(std::filesystem::path(Monstey_Wide(j.as_string())));
}
std::string Monstey_EText(const std::filesystem::path &p) { return Monstey_Utf8(p.wstring()); }
void Monstey_EKeys(const J &j, std::initializer_list<const char *> allowed) {
    Monstey_Require(j.is_object(), "World edit entry must be an object");
    for (auto &[key, value] : j.as_object()) {
        (void)value;
        Monstey_Require(std::any_of(allowed.begin(), allowed.end(), [&](auto name) { return key == name; }),
                        "Unknown world edit field: " + key);
    }
}
struct Monstey_EInputs {
    std::map<std::filesystem::path, std::string> hashes;
    B Load(const std::filesystem::path &p) {
        auto raw = Monstey_ReadFile(p);
        auto hash = Monstey_HashSha256(raw);
        auto [it, inserted] = hashes.emplace(p, hash);
        Monstey_Require(inserted || it->second == hash, "World edit source changed during compilation");
        return raw;
    }
    B Verified(const J &j) {
        auto p = Monstey_EPath(j.at("path"));
        auto raw = Load(p);
        Monstey_Require(j.at("sha256").as_string() == Monstey_HashSha256(raw),
                        "World edit descriptor checksum differs");
        if (j.contains("bytes"))
            Monstey_Require(j.at("bytes").as_uint() == raw.size(), "World edit source length differs");
        return raw;
    }
    void Collect(const J &j) {
        if (j.is_array()) {
            for (auto &item : j.as_array())
                Collect(item);
            return;
        }
        if (!j.is_object())
            return;
        for (auto &[key, value] : j.as_object()) {
            if (key == "properties" || key == "property_types")
                continue;
            if (key == "schema_source" || key == "attributes_npz")
                Load(Monstey_EPath(value));
            if ((key == "donor" || key == "owner_donor" || key == "placement_donor" || key == "target") &&
                value.is_object() && value.contains("source"))
                Load(Monstey_EPath(value.at("source")));
            Collect(value);
        }
    }
    void Verify() const {
        for (auto &[path, hash] : hashes)
            Monstey_Require(Monstey_HashSha256(path) == hash, "World edit input changed before publication");
    }
};
struct Monstey_EOutput {
    std::filesystem::path root;
    std::vector<std::filesystem::path> files, dirs;
    bool done = false;
    bool retain_failed = false;
    Monstey_EOutput(const std::filesystem::path &p, const Monstey_EInputs &inputs, bool retain = false) : root(p), retain_failed(retain) {
        std::vector<std::filesystem::path> sources;
        for (auto &[path, hash] : inputs.hashes) {
            (void)hash;
            sources.push_back(path);
        }
        Monstey_RequireOfflineOutput(root, sources);
        Monstey_Require(!std::filesystem::exists(Monstey_WindowsFilePath(root)),
                        "World edit output already exists");
        std::filesystem::create_directories(Monstey_WindowsFilePath(root.parent_path()));
        Monstey_Require(std::filesystem::create_directory(Monstey_WindowsFilePath(root)),
                        "Could not create new world edit output");
        dirs.push_back(root);
    }
    ~Monstey_EOutput() {
        if (!done && !retain_failed) {
            std::error_code ec;
            for (auto i = files.rbegin(); i != files.rend(); ++i)
                std::filesystem::remove(Monstey_WindowsFilePath(*i), ec);
            for (auto i = dirs.rbegin(); i != dirs.rend(); ++i)
                std::filesystem::remove(Monstey_WindowsFilePath(*i), ec);
        }
    }
    std::filesystem::path Save(std::string_view name, std::span<const std::uint8_t> raw) {
        auto path = std::filesystem::path(Monstey_Wide(Monstey_ArchiveVirtual(std::string(name))));
        auto parent = root;
        for (auto &piece : path.parent_path()) {
            parent /= piece;
            if (std::filesystem::create_directory(Monstey_WindowsFilePath(parent)))
                dirs.push_back(parent);
            else
                Monstey_Require(std::filesystem::is_directory(Monstey_WindowsFilePath(parent)),
                                "World edit parent is not a directory");
        }
        auto target = root / path;
        Monstey_WriteNewFile(target, raw);
        files.push_back(target);
        Monstey_Require(Monstey_ReadFile(target) == B(raw.begin(), raw.end()),
                        "World edit output readback differs");
        return target;
    }
    void Json(std::string_view name, const J &value) {
        auto text = value.dump(2) + "\n";
        Save(name,
             std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(text.data()), text.size()));
    }
    void CommitJson(std::string_view name, const J &value) {
        // Prepare every allocating/fallible operation before the commit marker.
        const auto text=value.dump(2)+"\n";
        const auto path=root/std::filesystem::path(Monstey_Wide(Monstey_ArchiveVirtual(std::string(name))));
        // Core writes/flushes/renames the same exclusively owned file object.
        // No pathname reopen, vector growth or other fallible work follows commit.
        Monstey_WriteNewText(path,text);
        done=true;
    }
};
J Monstey_ENormalize(J plan) {
    auto schema = plan.at("schema").as_string();
    if (schema == "Monstey_WorldBlenderEdits_2" || schema == "Monstey_WorldEdit_1")
        return plan;
    Monstey_Require(schema == "Monstey_WorldBlenderEdits_1", "Unsupported world edit schema");
    Monstey_Require(!plan.contains("static_pairs") && !plan.contains("generated_resources"),
        "Static components require WorldEdit_1 or WorldBlenderEdits_2, not legacy BlenderEdits_1");
    J::Array sources;
    auto objres = plan.value("objres", J::array());
    for (auto &row : objres.as_array())
        sources.push_back(J::object({{"backend", "OBJRES"},
                                     {"source", row.at("source")},
                                     {"resource_path", row.at("resource_path")},
                                     {"spec", J::object({{"source_sha256", row.at("source").at("sha256")},
                                                         {"updates", row.value("updates", J::array())},
                                                         {"objects", row.value("objects", J::array())}})}}));
    auto inst = plan.value("inst", J::array());
    for (auto &row : inst.as_array())
        sources.push_back(J::object({{"backend", "INST"},
                                     {"source", row.at("source")},
                                     {"resource_path", row.at("resource_path")},
                                     {"spec", row.at("spec")}}));
    return J::object({{"schema", "Monstey_WorldEdit_1"},
                      {"sources", sources},
                      {"bundles", plan.value("bundles", J::array())},
                      {"bindings", plan.value("bindings", J::array())}});
}
struct Monstey_ECompiler {
    J plan;
    Monstey_EInputs inputs;
    std::map<std::string, B> original, candidate;
    J::Object provenance;
    J::Array operations, pending, component_evidence;
    std::set<std::string> static_children, material_children, resident_children;
    std::map<std::string,std::set<std::string>> required_edges;
    std::map<std::string,std::string> generated_types;
    explicit Monstey_ECompiler(const J &raw) : plan(Monstey_ENormalize(raw)) {
        Monstey_EKeys(plan, {"schema", "sources", "bundles", "bindings", "scene_sha256", "counts", "warnings",
                             "units", "game_modified", "metadata", "static_pairs", "generated_resources"});
    }
    B Source(const std::string &path, const J &descriptor) {
        auto bytes = inputs.Verified(descriptor);
        auto [it, inserted] = original.emplace(path, bytes);
        Monstey_Require(inserted || it->second == bytes, "Conflicting original versions for one resource");
        if (inserted) {
            auto desc = descriptor;
            desc["path"] = Monstey_EText(Monstey_EPath(descriptor.at("path")));
            provenance.emplace(path, desc);
        }
        return candidate.contains(path) ? candidate.at(path) : bytes;
    }
    J Spec(const J &requested, const std::string &path, const B &current, const std::string &owner_path = {},
           const B *owner = nullptr) {
        auto spec = requested;
        if (spec.contains("source_sha256")) {
            Monstey_Require(spec.at("source_sha256").as_string() == Monstey_HashSha256(original.at(path)),
                            "Spec precondition differs from original source descriptor");
            spec["source_sha256"] = Monstey_HashSha256(current);
        }
        if (spec.contains("owner_source_sha256")) {
            Monstey_Require(owner && original.contains(owner_path) &&
                                spec.at("owner_source_sha256").as_string() ==
                                    Monstey_HashSha256(original.at(owner_path)),
                            "Owner precondition differs from original source descriptor");
            spec["owner_source_sha256"] = Monstey_HashSha256(*owner);
        }
        return spec;
    }
    void Pending(const std::string &path, const char *kind, const char *reason) {
        pending.push_back(J::object({{"resource_path", path}, {"kind", kind}, {"reason", reason}}));
    }
    std::optional<B> PreviousBinding(const std::string &resource) {
        if (candidate.contains(resource))
            return candidate.at(resource);
        auto bundles = plan.value("bundles", J::array());
        std::optional<B> found;
        for (auto &bundle : bundles.as_array())
            if (bundle.contains("embb_source")) {
                auto raw = inputs.Verified(bundle.at("embb_source"));
                auto children = Monstey_ReadEmbedded(raw);
                for (auto &child : children)
                    if (child.resource == resource) {
                        Monstey_Require(!found || *found == child.bytes,
                                        "Conflicting existing WDSC bindings");
                        found = child.bytes;
                    }
            }
        return found;
    }
    std::pair<B, J> Binding(const B &inst, const std::string &resource, std::uint32_t owner, const J &row,
                            const std::string &binding_path) {
        (void)Monstey_InstApplyBytes(inst, J::object());
        auto count = Monstey_U32(inst, 12);
        std::optional<std::size_t> selected;
        for (std::size_t i = 0; i < count; ++i)
            if (Monstey_U32(inst, 56 + 44 * i) == owner) {
                Monstey_Require(!selected, "Ambiguous render owner");
                selected = i;
            }
        Monstey_Require(selected.has_value(), "Native render owner is absent");
        auto at = 56 + 44 * (*selected);
        auto leaves = Monstey_U32(inst, at + 36);
        if (leaves) {
            B donor;
            if (row.contains("donor"))
                donor = inputs.Verified(row.at("donor"));
            else {
                auto previous = PreviousBinding(binding_path);
                Monstey_Require(previous.has_value(),
                                "Nonempty binding needs an explicit native donor or its existing graph");
                donor = std::move(*previous);
            }
            return Monstey_WdscBindingBytes(inst, resource, owner, donor);
        }
        Monstey_Require(count == 1 && Monstey_U32(inst, 28) == 0 && Monstey_U32(inst, 16) == 0 &&
                            (Monstey_U32(inst, at + 12) & 65535) == 0 &&
                            Monstey_U32(inst, at + 16) == 0xffffffffu,
                        "Empty binding would omit additional native render owners/geometry/LOD");
        auto geometry = 56 + 44 * count;
        Monstey_Require(Monstey_U32(inst, geometry) == 0 && Monstey_U32(inst, geometry + 4) == 0 &&
                            Monstey_U32(inst, geometry + 8) == 0,
                        "Empty binding cannot omit retained native vertex/index geometry");
        B empty{'W', 'D', 'S', 'C', 9, 0, 0, 0};
        empty.resize(96);
        Monstey_Require(Monstey_HashSha256(empty) ==
                            "923c87841a90b36c7d8431ad50a9b6e89c8922ac99e7ab729d0ad3a6ea3d430d",
                        "Qualified native empty scene bytes differ");
        auto previous = PreviousBinding(binding_path);
        if (previous && *previous != empty) {
            auto &old = *previous;
            constexpr std::uint32_t header[]{1, 0, 1, 1, 0, 0, 0, 0};
            Monstey_Require(old.size() >= 44 && std::memcmp(old.data(), "WDSC", 4) == 0 &&
                                Monstey_U32(old, 4) == 9,
                            "Existing WDSC is not a qualified single binding");
            for (std::size_t i = 0; i < 8; ++i)
                Monstey_Require(Monstey_U32(old, 8 + 4 * i) == header[i],
                                "Empty binding would remove unrelated native render records");
            auto names = Monstey_U32(old, 40);
            auto record = 44ull + names;
            Monstey_Require(record + 152 == old.size() && Monstey_U32(old, record + 64) == owner,
                            "Existing binding belongs to another owner");
            constexpr std::uint32_t group[]{0, 0, 1, 0, 1, 0};
            for (std::size_t i = 0; i < 6; ++i)
                Monstey_Require(Monstey_U32(old, record + 76 + 4 * i) == group[i],
                                "Empty binding would discard another native owner group");
            for (std::size_t i = 0; i < 13; ++i)
                Monstey_Require(Monstey_U32(old, record + 100 + 4 * i) == 0,
                                "Empty binding cannot discard a nonempty native scene graph");
            auto offset = Monstey_U32(old, record + 72);
            Monstey_Require(offset < names, "Previous binding resource offset outside pool");
            auto begin = old.begin() + 44 + offset, end = std::find(begin, old.begin() + 44 + names, 0);
            Monstey_Require(end != old.begin() + 44 + names &&
                                Monstey_ArchiveVirtual(std::string(begin, end)) == resource,
                            "Empty scene would remove another resource binding");
        }
        return {empty, J::object({{"status", "NATIVE_EMPTY_WDSC_STRUCTURE_RUNTIME_PENDING"},
                                  {"owner_key", owner},
                                  {"inst_resource", resource},
                                  {"owned_instance_range", J::array({Monstey_U32(inst, at + 32), 0})},
                                  {"retained_unowned_instance_slots", Monstey_U32(inst, 24)},
                                  {"empty_native_scene_preserved_exact", true},
                                  {"other_render_owners_absent", true}})};
    }
    void Stage(const std::string &path,const J &row,const B &bytes) {
        if(row.contains("source")) (void)Source(path,row.at("source"));
        const auto it=candidate.find(path);
        Monstey_Require(it==candidate.end() || it->second==bytes,
            "Conflicting generated candidate for one world resource");
        candidate[path]=bytes;
        static_children.insert(path);
    }
    void Evidence(const std::string &path,const std::string &kind,const J &report,
        const char *native,const char *limits) {
        const bool bound=kind=="NAVIGATION" || kind=="LIGHT" || kind=="STATIC_MESH_PAIR" ||
            kind=="STATIC_SCENE" || kind=="OBJRES";
        const bool format=std::string_view(native)=="VALIDATED_OFFLINE";
        component_evidence.push_back(J::object({{"resource_path",path},{"component",kind},
            {"descriptor_hashes","VERIFIED"},{"candidate_sha256",Monstey_HashSha256(candidate.at(path))},
            {"native_format",native},{"format_valid",format ? J(true) : J(nullptr)},
            {"owner_binding_valid",bound ? J(true) : J(nullptr)},
            {"owner_binding_scope",bound ? "NATIVE_INTERNAL_OWNER_OR_RENDER_ASSOCIATION_ONLY" : "NOT_EVALUATED_OR_NOT_APPLICABLE"},
            {"package_residency_valid",false},
            {"geometry_alignment",J::object({{"status","UNVERIFIED"},{"proof",nullptr},
                {"reason","No shared-centimetre-frame triangle/seam/spatial association proof is supplied by this staging API."}})},
            {"offline_result",report},{"runtime_verified",false},
            {"runtime_status","PENDING_IN_GAME_TEST"},{"limits",limits}}));
    }
    void Edges(const std::string &path,const J &row,const char *field="requires") {
        const auto edges=row.value(field,J::array());
        Monstey_Require(edges.is_array(),"World dependency list must be an array");
        for(const auto &value:edges.as_array()) {
            const auto edge=Monstey_ArchiveVirtual(value.as_string());
            Monstey_Require(edge!=path && required_edges[path].insert(edge).second,
                "Duplicate or self world dependency");
        }
    }
    J Physics(const B &bytes) {
        const auto owners=Monstey_ObjresOwners(bytes);
        const auto chunk=Monstey_ObjresReadChunk(bytes,0xb9e81cf3u);
        Monstey_Physics physics(chunk);
        auto report=physics.Describe(0,500);
        Monstey_Require(report.at("parse_limitation").is_null() && physics.Encode()==chunk,
            "Generated collision has an unsupported native shape profile or nonexact no-op");
        J::Array tags;
        const auto total=report.at("total_shape_nodes").as_uint();
        for(std::uint64_t offset=0;offset<total;offset+=500) {
            const auto page=physics.Describe(static_cast<std::size_t>(offset),500);
            for(const auto &shape:page.at("shapes").as_array()) {
                if(shape.at("type").as_uint()!=12) continue;
                std::vector<std::size_t> route;
                for(const auto &part:shape.at("route").as_array())
                    route.push_back(static_cast<std::size_t>(part.as_uint()));
                const auto node=physics.ShapeBytes(static_cast<std::size_t>(shape.at("index").as_uint()),route);
                const auto length=Monstey_U32(node,16);
                Monstey_CheckRange(node,20,length);
                Monstey_Havok tag(std::span<const std::uint8_t>(node).subspan(20,length));
                const auto audit=tag.Audit();
                Monstey_Require(audit.at("status").as_string()=="PASS" &&
                    audit.at("type_hash_coverage").at("status").as_string()=="PASS",
                    "Generated collision has missing active Havok type hashes");
                tags.push_back(J::object({{"shape",shape.at("index")},{"route",shape.at("route")},
                    {"audit",audit}}));
            }
        }
        report["active_havok_tags"]=tags;
        report["objres_owners"]=owners;
        report["render_triangle_alignment"]="NOT_EVALUATED";
        return report;
    }
    void Generated() {
        const auto rows=plan.value("generated_resources",J::array());
        // All immutable resources and owners are staged before cross-resource validation.
        for(const auto &row:rows.as_array()) {
            Monstey_EKeys(row,{"backend","resource_path","candidate","source","owner_resource_path",
                "owner_candidate","owner_source","owner_name","requires","dependency_kind"});
            const auto path=Monstey_ArchiveVirtual(row.at("resource_path").as_string());
            const auto backend=row.at("backend").as_string();
            const bool owned=backend=="NAVIGATION" || backend=="LIGHT";
            Monstey_Require(owned || (!row.contains("owner_candidate") && !row.contains("owner_source") &&
                !row.contains("owner_resource_path")),"Owner fields are only valid for navigation/light candidates");
            Monstey_Require(backend=="LIGHT" || !row.contains("owner_name"),"owner_name is only valid for LIGHT");
            Monstey_Require(backend=="DEPENDENCY" || !row.contains("dependency_kind"),
                "dependency_kind is only valid for DEPENDENCY");
            Monstey_Require(generated_types.emplace(path,backend).second,
                "Duplicate generated world resource");
            Stage(path,row,inputs.Verified(row.at("candidate")));
            Edges(path,row);
            if(row.contains("owner_source"))
                (void)Source(Monstey_ArchiveVirtual(row.at("owner_resource_path").as_string()),row.at("owner_source"));
            if(row.contains("owner_candidate")) {
                const auto owner=Monstey_ArchiveVirtual(row.at("owner_resource_path").as_string());
                Monstey_Require(owner.ends_with(".objres") && owner!=path,
                    "Generated owner needs a distinct OBJRES path");
                J owner_row=J::object();
                if(row.contains("owner_source")) owner_row["source"]=row.at("owner_source");
                Stage(owner,owner_row,inputs.Verified(row.at("owner_candidate")));
                required_edges[path].insert(owner);
            }
        }
        for(const auto &row:rows.as_array()) {
            const auto path=Monstey_ArchiveVirtual(row.at("resource_path").as_string());
            const auto backend=row.at("backend").as_string();
            const auto &bytes=candidate.at(path);
            J report;
            const char *native="VALIDATED_OFFLINE",*limits="Runtime behavior is not established by format validation.";
            if(backend=="PHYSICS") {
                Monstey_Require(path.ends_with(".objres"),"Generated physics must be OBJRES");
                report=Physics(bytes);
                limits="Shape/THSH/OBJRES are validated. Triangle/render alignment, spatial world registration and in-game collision remain unverified.";
            } else if(backend=="NAVIGATION" || backend=="LIGHT") {
                const auto owner=Monstey_ArchiveVirtual(row.at("owner_resource_path").as_string());
                Monstey_Require(candidate.contains(owner),"Generated owner is not staged");
                required_edges[path].insert(owner);
                if(backend=="NAVIGATION") {
                    Monstey_Require(path.ends_with(".hkai") && owner.ends_with(".objres") &&
                        path.substr(0,path.size()-5)==owner.substr(0,owner.size()-7),
                        "Generated HKAI requires same-stem OBJRES owner");
                    report=Monstey_NavigationInspect(bytes,candidate.at(owner));
                    Monstey_Require(report.at("no_op_byte_exact").as_bool(),"Navigation no-op differs");
                    const auto length=Monstey_U32(bytes,12);
                    Monstey_CheckRange(bytes,16,length);
                    Monstey_Havok tag(std::span<const std::uint8_t>(bytes).subspan(16,length));
                    const auto audit=tag.Audit();
                    Monstey_Require(audit.at("status").as_string()=="PASS" &&
                        audit.at("type_hash_coverage").at("status").as_string()=="PASS",
                        "Generated navigation has missing active Havok type hashes");
                    report["active_havok_audit"]=audit;
                    limits="HKAI structure, active THSH and native piece owners are checked. New seams, clearance, spatial streaming and actor traversal require separate qualification.";
                } else {
                    Monstey_Require(path.ends_with(".fxp"),"Generated light must be FXP");
                    report=Monstey_WorldLightInspect(Monstey_EPath(row.at("candidate").at("path")),path,
                        candidate.at(owner),row.at("owner_name").as_string());
                    limits="Native Point/Spot active key and SpecialFX start/loop/path are checked. Section lifecycle, attenuation and visible in-game lighting remain unverified.";
                }
            } else if(backend=="OBJRES") {
                Monstey_Require(path.ends_with(".objres"),"Generated owner must be OBJRES");
                report=J::object({{"owners",Monstey_ObjresOwners(bytes)}});
            } else if(backend=="DEPENDENCY") {
                const auto kind=row.at("dependency_kind").as_string();
                if(kind=="MATERIAL") {
                    Monstey_Require(path.ends_with(".mat"),"Material dependency extension differs");
                    report=Monstey_MaterialInspect(bytes);
                    Monstey_Require(Monstey_LtmiBuild(Monstey_LtmiRead(bytes))==bytes,
                        "Material dependency exact native no-op differs");
                    material_children.insert(path);
                } else if(kind=="TEXTURE") {
                    Monstey_Require(path.ends_with(".tex") || path.ends_with(".tex0") ||
                        path.ends_with(".texuhd") || path.ends_with(".dds"),"Texture dependency extension differs");
                    const Monstey_TextureResource texture(bytes);
                    report=texture.Describe();
                    Monstey_Require(texture.Build()==bytes,"Texture dependency exact no-op differs");
                    report["gpu_upload_and_sampler"]="NOT_EVALUATED";
                    if(!report.at("layout_qualified").as_bool())
                        native="CONTAINER_ONLY_UNQUALIFIED_LAYOUT";
                } else {
                    Monstey_Require(kind=="OPAQUE","Unknown explicit dependency kind");
                    native="NOT_EVALUATED_OPAQUE_BYTES_PRESERVED";
                    report=J::object({{"bytes",static_cast<std::uint64_t>(bytes.size())},
                        {"sha256",Monstey_HashSha256(bytes)}});
                }
                limits="Explicit dependency edges/residency are checked; shader/global binding, inferred texture semantics and native rendering are not inferred.";
            } else throw Monstey_Error("world_edit_backend","Unsupported generated world backend");
            Evidence(path,backend,report,native,limits);
            operations.push_back(J::object({{"backend",backend},{"resource_path",path},
                {"generated",true},{"output_sha256",Monstey_HashSha256(bytes)},{"report",report}}));
        }
    }
    void StaticPairs() {
        const auto pairs=plan.value("static_pairs",J::array());
        for(const auto &row:pairs.as_array()) {
            Monstey_EKeys(row,{"mesh_source","render_source","mesh_candidate","render_candidate",
                "topology","source_resource","mesh_resource_path","render_resource_path",
                "render_mode","scene_template","dependencies"});
            const auto source=Monstey_ArchiveVirtual(row.at("source_resource").as_string());
            const auto mesh=Monstey_ArchiveVirtual(row.at("mesh_resource_path").as_string());
            const auto render=Monstey_ArchiveVirtual(row.at("render_resource_path").as_string());
            Monstey_Require(source.ends_with(".wldmsh") && mesh.ends_with(".wldmsh") &&
                render.ends_with(".wldclnt") && !candidate.contains(mesh) && !candidate.contains(render),
                "Static pair output extensions or duplicate staging differ");
            const auto before_mesh=inputs.Verified(row.at("mesh_source"));
            const auto before_render=inputs.Verified(row.at("render_source"));
            const auto after_mesh=inputs.Verified(row.at("mesh_candidate"));
            const auto after_render=inputs.Verified(row.at("render_candidate"));
            const auto proof_bytes=inputs.Verified(row.at("topology"));
            const auto proof_document=J::Parse(std::string_view(reinterpret_cast<const char*>(proof_bytes.data()),proof_bytes.size()));
            const auto &proof=proof_document.contains("topology") ? proof_document.at("topology") : proof_document;
            const auto mode=row.at("render_mode").as_string();
            Monstey_Require(mode=="private" || mode=="preserve-section","Unknown static render mode");
            B scene;
            if(row.contains("scene_template")) scene=inputs.Verified(row.at("scene_template"));
            const auto report=Monstey_WorldStaticPairValidate(before_mesh,before_render,after_mesh,after_render,
                proof,source,mesh,mode=="preserve-section",scene);
            if(source==mesh) (void)Source(mesh,row.at("mesh_source"));
            if(mode=="preserve-section") (void)Source(render,row.at("render_source"));
            candidate[mesh]=after_mesh;candidate[render]=after_render;
            static_children.insert(mesh);static_children.insert(render);
            required_edges[render].insert(mesh);
            Edges(render,row,"dependencies");
            for(const auto &material:report.at("material_dependencies").as_array()) {
                const auto path=Monstey_ArchiveVirtual(material.as_string());
                Monstey_Require(required_edges[render].contains(path) && material_children.contains(path),
                    "Static descriptor material is not a declared, typed staged dependency");
            }
            Evidence(mesh,"STATIC_MESH_PAIR",report,"VALIDATED_OFFLINE",
                "Native format/material/indexed culls/scene replay are verified. Post-gather owner/cache routing, GPU resources, visible geometry and gameplay remain unverified.");
            Evidence(render,"STATIC_SCENE",report,"VALIDATED_OFFLINE",
                "Existing scene/opaque bytes or qualified private scene routing are preserved. In-game post-gather submission remains pending.");
            operations.push_back(J::object({{"backend","STATIC_PAIR"},{"mesh_resource_path",mesh},
                {"render_resource_path",render},{"report",report}}));
        }
    }
    void FinalizeStaticBundles(const J &bundles) {
        if(static_children.empty())return;
        std::map<std::string,std::set<std::string>> bundle_members, embedded_members;
        for(const auto &row:bundles.as_array()) {
            const auto path=Monstey_ArchiveVirtual(row.at("resource_path").as_string());
            for(const auto &name:row.at("resources").as_array()) {
                const auto child=Monstey_ArchiveVirtual(name.as_string());
                bundle_members[path].insert(child);
                if(row.contains("embb_source"))
                    embedded_members[Monstey_ArchiveVirtual(row.at("embb_resource_path").as_string())].insert(child);
            }
        }
        // Physical payload nesting must be acyclic. BNDL size-hint references
        // are resolved separately and are not treated as embedded byte copies.
        std::map<std::string,std::size_t> waiting;
        std::map<std::string,std::set<std::string>> parents;
        std::set<std::string> ready;
        for(const auto &[parent,members]:embedded_members) {
            auto &count=waiting[parent];
            for(const auto &child:members)if(embedded_members.contains(child)) {
                ++count;parents[child].insert(parent);
            }
            if(!count)ready.insert(parent);
        }
        std::vector<std::string> order;
        while(!ready.empty()) {
            const auto child=*ready.begin();ready.erase(ready.begin());order.push_back(child);
            for(const auto &parent:parents[child])if(--waiting.at(parent)==0)ready.insert(parent);
        }
        Monstey_Require(order.size()==embedded_members.size(),
            "Embedded payload containers form a physical byte-copy cycle");
        auto rebuild_embedded=[&] {
            for(const auto &parent:order) {
                std::map<std::string,B> updates;
                for(const auto &child:embedded_members.at(parent)) {
                    Monstey_Require(candidate.contains(child),"Final embedded child is not staged");
                    updates.emplace(child,candidate.at(child));
                }
                candidate[parent]=Monstey_RebuildEmbedded(candidate.at(parent),updates);
            }
        };
        // First settle physical container sizes. A BNDL hint is fixed-width,
        // so its final hint values do not affect its already authored size.
        rebuild_embedded();
        for(const auto &[path,members]:bundle_members) {
            std::map<std::string,std::uint32_t> sizes;
            for(const auto &child:members) {
                Monstey_Require(candidate.contains(child)&&candidate.at(child).size()<=UINT32_MAX,
                    "Final bundle child is absent or exceeds DWORD");
                sizes.emplace(child,static_cast<std::uint32_t>(candidate.at(child).size()));
            }
            const auto previous_size=candidate.at(path).size();
            auto bytes=Monstey_RebuildBundle(candidate.at(path),sizes);
            Monstey_Require(bytes.size()==previous_size,"Final fixed-width bundle hint update changed its extent");
            candidate[path]=std::move(bytes);
        }
        std::map<std::string,std::size_t> settled_sizes;
        for(const auto &parent:order)settled_sizes.emplace(parent,candidate.at(parent).size());
        rebuild_embedded();
        for(const auto &[parent,size]:settled_sizes)
            Monstey_Require(candidate.at(parent).size()==size,"Final embedded container size did not stabilize");
        // Verify each declared coupling against FINAL bytes, never a historic
        // membership bit set while an earlier parent still held an old child.
        resident_children.clear();
        for(const auto &row:bundles.as_array())if(row.contains("embb_source")) {
            const auto path=Monstey_ArchiveVirtual(row.at("resource_path").as_string());
            const auto parent=Monstey_ArchiveVirtual(row.at("embb_resource_path").as_string());
            const auto bundle=Monstey_ReadBundle(candidate.at(path));
            const auto embedded=Monstey_ReadEmbedded(candidate.at(parent));
            for(const auto &name:row.at("resources").as_array()) {
                const auto child=Monstey_ArchiveVirtual(name.as_string());
                const auto member=std::find_if(bundle.resources.begin(),bundle.resources.end(),
                    [&](const auto &value){return value.resource==child;});
                const auto payload=std::find_if(embedded.begin(),embedded.end(),
                    [&](const auto &value){return value.resource==child;});
                Monstey_Require(candidate.contains(child)&&member!=bundle.resources.end()&&payload!=embedded.end()&&
                    member->size_hint==candidate.at(child).size()&&payload->bytes==candidate.at(child),
                    "Final coupled BNDL/EMBB bytes differ from staged child");
                resident_children.insert(child);
            }
        }
        for(auto &entry:component_evidence)if(entry.contains("resource_path")) {
            const auto path=entry.at("resource_path").as_string();
            const auto final_hash=Monstey_HashSha256(candidate.at(path));
            if(entry.at("candidate_sha256").as_string()!=final_hash) {
                Monstey_Require(entry.at("component").as_string()=="DEPENDENCY"&&
                    entry.at("native_format").as_string()=="NOT_EVALUATED_OPAQUE_BYTES_PRESERVED"&&
                    (bundle_members.contains(path)||embedded_members.contains(path)),
                    "A validated typed world component was overwritten by package composition");
                entry["input_candidate_sha256"]=entry.at("candidate_sha256");
                entry["candidate_sha256"]=final_hash;
                entry["recomposed_by_explicit_bundle_plan"]=true;
                entry["native_format"]="OPAQUE_RECOMPOSED_BY_EXPLICIT_BUNDLE_PLAN";
                entry["offline_result"]=J::object({{"bytes",static_cast<std::uint64_t>(candidate.at(path).size())},
                    {"sha256",final_hash},{"input_bytes_preserved",false},{"explicit_bundle_recomposition",true}});
                for(auto &operation:operations)if(operation.value("generated",false).as_bool()&&
                    operation.at("resource_path").as_string()==path) {
                    operation["input_candidate_sha256"]=operation.at("output_sha256");
                    operation["output_sha256"]=final_hash;operation["report"]=entry.at("offline_result");
                }
            }
        }
        for(auto &operation:operations)if(operation.at("backend").as_string()=="BNDL") {
            const auto path=operation.at("resource_path").as_string();
            operation["final_output_sha256"]=Monstey_HashSha256(candidate.at(path));
            operation["changed"]=!original.contains(path)||candidate.at(path)!=original.at(path);
        }
    }
    void DependencyClosure() {
        for(const auto &[owner,edges]:required_edges)
            for(const auto &edge:edges)
                Monstey_Require(candidate.contains(edge),"Explicit world dependency is not staged: "+owner+" -> "+edge);
        for(const auto &resource:static_children)
            Monstey_Require(resident_children.contains(resource),
                "Static component child needs coupled BNDL+EMBB residency: "+resource);
        for(auto &entry:component_evidence)
            if(entry.contains("resource_path"))
                entry["package_residency_valid"]=resident_children.contains(entry.at("resource_path").as_string());
        if(!static_children.empty())
            for(const char *kind:{"PHYSICS","NAVIGATION","LIGHT"}) {
                const bool present=std::any_of(component_evidence.begin(),component_evidence.end(),
                    [&](const auto &entry){return entry.at("component").as_string()==kind;});
                if(!present) component_evidence.push_back(J::object({{"component",kind},
                    {"native_format","NOT_PROVIDED"},{"format_valid",nullptr},{"owner_binding_valid",nullptr},
                    {"package_residency_valid",nullptr},
                    {"geometry_alignment",J::object({{"status","UNVERIFIED"},{"proof",nullptr},{"reason","Component not supplied."}})},
                    {"runtime_verified",false},
                    {"runtime_status","NOT_TESTED"},
                    {"limits","This transaction supplies no candidate for this component."}}));
            }
        if(!static_children.empty()) component_evidence.push_back(J::object({
            {"component","DEPENDENCY_BNDL_EMBB"},{"native_format","VALIDATED_OFFLINE"},
            {"format_valid",true},{"owner_binding_valid",nullptr},{"package_residency_valid",true},
            {"geometry_alignment",J::object({{"status","UNVERIFIED"},{"proof",nullptr},
                {"reason","Resource residency does not prove shared spatial alignment."}})},
            {"resident_children",static_cast<std::uint64_t>(resident_children.size())},
            {"declared_dependency_edges_verified",true},{"semantic_transitive_closure","NOT_INFERRED"},
            {"runtime_verified",false},{"runtime_status","PENDING_IN_GAME_LOAD"},
            {"limits","Paired BNDL sizes and exact EMBB child bytes are checked. File residency alone does not establish render/GPU readiness."}}));
    }
    void Apply() {
        auto sources = plan.value("sources", J::array());
        for (std::size_t index = 0; index < sources.size(); ++index) {
            auto &row = sources[index];
            Monstey_EKeys(row, {"source_id", "backend", "source", "resource_path", "spec", "owner_source",
                                "owner_resource_path"});
            auto path = Monstey_ArchiveVirtual(row.at("resource_path").as_string()),
                 backend = row.at("backend").as_string();
            auto raw = Source(path, row.at("source"));
            auto requested_spec = row.value("spec", J::object());
            auto spec = backend == "NAVIGATION" ? requested_spec : Spec(requested_spec, path, raw);
            inputs.Collect(spec);
            B encoded;
            J report;
            std::string owner_path;
            if (backend == "OBJRES") {
                Monstey_Require(path.ends_with(".objres"), "OBJRES path extension mismatch");
                auto result = Monstey_ObjresApply(raw, spec);
                encoded = std::move(result.first);
                report = std::move(result.second);
            } else if (backend == "INST") {
                Monstey_Require(path.ends_with(".inst"), "INST path extension mismatch");
                auto result = Monstey_InstApplyBytes(raw, spec);
                encoded = std::move(result.first);
                report = std::move(result.second);
                if (encoded != raw)
                    Pending(path, "render_binding_bounds",
                            "Changed owned instances need matching WDSC bounds and binding");
            } else if (backend == "MESH5") {
                Monstey_Require(path.ends_with(".wldmsh"), "MESH5 path extension mismatch");
                auto result = Monstey_WorldMeshApplyBytes(raw, spec);
                encoded = std::move(result.first);
                report = std::move(result.second);
                if (encoded != raw)
                    Pending(path, "geometry_dependents",
                            "Changed render geometry needs matching WDSC, collision and navigation");
            } else if (backend == "PHYSICS") {
                Monstey_Require(path.ends_with(".objres") && raw.size() >= 4 &&
                                    std::memcmp(raw.data(), "OBJR", 4) == 0,
                                "Component physics needs complete OBJRES");
                auto result =
                    Monstey_PhysicsWorldApply(raw, spec, [&](const auto &p) { return inputs.Load(p); });
                encoded = std::move(result.first);
                report = std::move(result.second);
                if (encoded != raw)
                    Pending(path, "visual_navigation_alignment",
                            "Physical owner edits do not automatically modify rendering or neighbouring "
                            "navigation");
            } else if (backend == "NAVIGATION") {
                Monstey_Require(path.ends_with(".hkai"), "Navigation path extension mismatch");
                owner_path = Monstey_ArchiveVirtual(row.at("owner_resource_path").as_string());
                Monstey_Require(path.substr(0, path.size() - 5) ==
                                        owner_path.substr(0, owner_path.size() - 7) &&
                                    owner_path.ends_with(".objres"),
                                "HKAI requires a same-stem OBJRES owner");
                auto owners = Source(owner_path, row.at("owner_source"));
                spec = Spec(row.value("spec", J::object()), path, raw, owner_path, &owners);
                auto result = Monstey_NavigationApply(raw, owners, spec);
                encoded = std::move(result.navigation);
                candidate[owner_path] = std::move(result.owner);
                report = std::move(result.report);
                if (encoded != raw)
                    Pending(path, "navigation_connection",
                            "Native piece edits retain topology; new neighbouring runtime connections "
                            "require qualification");
            } else
                throw Monstey_Error("world_edit_backend", "Unsupported native world edit backend");
            candidate[path] = encoded;
            operations.push_back(J::object({{"index", static_cast<std::uint64_t>(index)},
                                            {"backend", backend},
                                            {"resource_path", path},
                                            {"source_stage_sha256", Monstey_HashSha256(raw)},
                                            {"output_sha256", Monstey_HashSha256(encoded)},
                                            {"changed", encoded != raw},
                                            {"owner_resource_path", owner_path.empty() ? J{} : J(owner_path)},
                                            {"report", report}}));
        }
        auto bindings = plan.value("bindings", J::array());
        for (auto &row : bindings.as_array()) {
            Monstey_EKeys(row, {"inst_resource_path", "resource_path", "inst_source", "owner_key", "donor"});
            auto inst_path = Monstey_ArchiveVirtual(row.at("inst_resource_path").as_string()),
                 path = Monstey_ArchiveVirtual(row.at("resource_path").as_string());
            Monstey_Require(path.ends_with(".wldclnt") && !candidate.contains(path),
                            "Duplicate or invalid WDSC binding output");
            auto inst_bytes = candidate.contains(inst_path) ? candidate.at(inst_path)
                                                            : Source(inst_path, row.at("inst_source"));
            auto key = row.at("owner_key").as_uint();
            Monstey_Require(key <= 0xffffffffull, "Native owner key exceeds DWORD");
            auto result = Binding(inst_bytes, inst_path, static_cast<std::uint32_t>(key), row, path);
            candidate[path] = std::move(result.first);
            std::erase_if(pending, [&](const auto &p) {
                return p.at("resource_path").as_string() == inst_path &&
                       p.at("kind").as_string() == "render_binding_bounds";
            });
            operations.push_back(J::object({{"backend", "WDSC"},
                                            {"resource_path", path},
                                            {"report", result.second},
                                            {"changed", true}}));
        }
        Generated();
        StaticPairs();
        auto bundles = plan.value("bundles", J::array());
        if(!static_children.empty())for(const auto &row:bundles.as_array()) {
            const auto path=Monstey_ArchiveVirtual(row.at("resource_path").as_string());
            const auto bytes=Source(path,row.at("source"));
            if(!candidate.contains(path))candidate.emplace(path,bytes);
            if(row.contains("embb_source")) {
                const auto parent=Monstey_ArchiveVirtual(row.at("embb_resource_path").as_string());
                const auto raw=Source(parent,row.at("embb_source"));
                if(!candidate.contains(parent))candidate.emplace(parent,raw);
            }
        }
        for (auto &row : bundles.as_array()) {
            Monstey_EKeys(row, {"resource_path", "source", "resources", "additions", "embb_source",
                                "embb_resource_path"});
            auto path = Monstey_ArchiveVirtual(row.at("resource_path").as_string());
            Monstey_Require(path.ends_with(".bndl"), "Bundle output must be BNDL");
            auto raw = Source(path, row.at("source"));
            auto bundle = Monstey_ReadBundle(raw);
            std::set<std::string> known, selected, added;
            for (auto &resource : bundle.resources)
                known.insert(resource.resource);
            for (auto &name : row.at("resources").as_array())
                Monstey_Require(selected.insert(Monstey_ArchiveVirtual(name.as_string())).second,
                                "Duplicate selected bundle child");
            auto additions = row.value("additions", J::array());
            for (auto &name : additions.as_array()) {
                auto resource = Monstey_ArchiveVirtual(name.as_string());
                Monstey_Require(selected.contains(resource) && !known.contains(resource) &&
                                    added.insert(resource).second,
                                "Bundle addition is not an explicitly new selected resource");
            }
            std::map<std::string, std::uint32_t> sizes, new_sizes;
            for (auto &resource : selected) {
                Monstey_Require(candidate.contains(resource) &&
                                    (known.contains(resource) || added.contains(resource)),
                                "Selected bundle candidate is absent or not declared new");
                Monstey_Require(candidate.at(resource).size() <= 0xffffffffull,
                                "Bundle child size exceeds DWORD");
                (added.contains(resource) ? new_sizes : sizes)
                    .emplace(resource, static_cast<std::uint32_t>(candidate.at(resource).size()));
            }
            candidate[path] = Monstey_RebuildBundle(raw, sizes, new_sizes);
            if (row.contains("embb_source")) {
                auto parent = Monstey_ArchiveVirtual(row.at("embb_resource_path").as_string());
                Monstey_Require(parent.ends_with(".embb"), "Embedded parent must be EMBB");
                auto bytes = Source(parent, row.at("embb_source"));
                auto children = Monstey_ReadEmbedded(bytes);
                std::set<std::string> present;
                for (auto &child : children)
                    present.insert(child.resource);
                std::map<std::string, B> replacements, new_children;
                for (auto &resource : selected) {
                    Monstey_Require(present.contains(resource) || added.contains(resource),
                                    "Selected embedded child is absent with no explicit addition");
                    (present.contains(resource) ? replacements : new_children)
                        .emplace(resource, candidate.at(resource));
                }
                candidate[parent] = Monstey_RebuildEmbedded(bytes, replacements, new_children);
                const auto rebuilt_children=Monstey_ReadEmbedded(candidate.at(parent));
                const auto rebuilt_bundle=Monstey_ReadBundle(candidate.at(path));
                for(const auto &resource:selected) {
                    const auto child=std::find_if(rebuilt_children.begin(),rebuilt_children.end(),
                        [&](const auto &c){return c.resource==resource;});
                    const auto member=std::find_if(rebuilt_bundle.resources.begin(),rebuilt_bundle.resources.end(),
                        [&](const auto &c){return c.resource==resource;});
                    Monstey_Require(child!=rebuilt_children.end() && child->bytes==candidate.at(resource) &&
                        member!=rebuilt_bundle.resources.end() && member->size_hint==candidate.at(resource).size(),
                        "Coupled BNDL/EMBB readback differs");
                    resident_children.insert(resource);
                }
            }
            operations.push_back(J::object({{"backend", "BNDL"},
                                            {"resource_path", path},
                                            {"updated_children", static_cast<std::uint64_t>(sizes.size())},
                                            {"added_children", static_cast<std::uint64_t>(new_sizes.size())},
                                            {"changed", candidate[path] != raw}}));
        }
        FinalizeStaticBundles(bundles);
        DependencyClosure();
        inputs.Verify();
    }
    J Report() const {
        J::Array resources;
        for (auto &[path, bytes] : candidate)
            resources.push_back(
                J::object({{"resource_path", path},
                           {"bytes", static_cast<std::uint64_t>(bytes.size())},
                           {"sha256", Monstey_HashSha256(bytes)},
                           {"changed", !original.contains(path) || original.at(path) != bytes}}));
        return J::object({{"status", "OFFLINE_WORLD_EDIT_VALIDATED"},
                          {"component_evidence", component_evidence},
                          {"failure", J{}},
                          {"schema", "Monstey_WorldEditResult_1"},
                          {"operations", operations},
                          {"resources", resources},
                          {"pending_dependencies", pending},
                          {"all_requested_bindings_built", std::none_of(pending.begin(), pending.end(),
                                                                        [](const auto &p) {
                                                                            return p.at("kind").as_string() ==
                                                                                   "render_binding_bounds";
                                                                        })},
                          {"runtime_verified", false},
                          {"global_package_built", false},
                          {"game_written", false}});
    }
    J Write(const std::filesystem::path &path) {
        Monstey_Require(!candidate.empty(), "No validated candidate resources to write");
        inputs.Verify();
        const bool static_transaction=!static_children.empty();
        Monstey_EOutput output(path, inputs, static_transaction);
        J::Array resources;
        for (auto &[resource, bytes] : candidate) {
            auto disk = output.Save("assets/" + resource, bytes);
            resources.push_back(J::object({{"resource_path", resource},
                                           {"path", Monstey_EText(disk)},
                                           {"bytes", static_cast<std::uint64_t>(bytes.size())},
                                           {"sha256", Monstey_HashSha256(bytes)},
                                           {"role", "world_edit_candidate"}}));
        }
        auto report = Report();
        report["resources"] = resources;
        report["source_provenance"] = provenance;
        report["ready_for_component_integration"] = pending.empty();
        if(static_transaction) {
            // A completion manifest is the commit marker. Failed new static
            // transactions retain partial evidence and never delete by pathname.
            output.Json("input-plan.json",plan);
            inputs.Verify();
            report["publication_contract"]="NEW_FILES_EXCLUSIVE_CORE_PUBLISHER_MANIFEST_LAST";
            report["failure_artifacts"]="RETAINED_WITHOUT_COMPLETION_MANIFEST";
            output.CommitJson("resource-manifest.json",report);
        } else {
            output.Json("resource-manifest.json", report);
            output.Json("input-plan.json", plan);
            inputs.Verify();
        }
        output.done = true;
        return report;
    }
};
} // namespace
J Monstey_WorldEditCatalog() {
    auto schema = J::object(
        {{"type", "object"},
         {"properties", J::object({{"plan", J::object({{"type", "object"}})},
                                   {"output", J::object({{"type", "string"}, {"format", "path"}})}})},
         {"required", J::array({"plan"})},
         {"additionalProperties", false}});
    return J::array({J::object(
        {{"name", "sow_worldedit_compile"},
         {"description", "Native source-verified multi-backend world edit and BNDL/EMBB composition"},
         {"status", "complete"},
         {"available", true},
         {"implemented", true},
         {"inputSchema", schema},
         {"implementation_language", "C++20"},
         {"runtime_verified", false}})});
}
J Monstey_WorldEditCall(std::string_view name, const J &a) {
    Monstey_Require(name == "sow_worldedit_compile", "Unknown native world edit operation");
    Monstey_EKeys(a, {"plan", "output"});
    Monstey_ECompiler compiler(a.at("plan"));
    compiler.Apply();
    return a.contains("output") ? compiler.Write(Monstey_EPath(a.at("output"))) : compiler.Report();
}
} // namespace Monstey
