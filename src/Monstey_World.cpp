#include "world/Monstey_WorldPrivate.hpp"

namespace Monstey
{
using namespace Monstey_WorldPrivate;

J Monstey_WorldCall(std::string_view name, const J &a)
{
    auto tool =
        std::find_if(std::begin(Monstey_WTools), std::end(Monstey_WTools), [&](auto &row) { return name == row.name; });
    Monstey_Require(tool != std::end(Monstey_WTools), "Unknown world operation");
    Monstey_Require(a.is_object(), "World operation arguments must be an object");
    if (name == "sow_world_navigation_static_tree")
        return Monstey_NavStaticTreeInspect(a);
    if (name == "sow_world_scene_prepare")
        return Monstey_WScenePrepare(a);
    if (name == "sow_capabilities")
    {
        std::uint64_t complete{}, partial{};
        for (auto &row : Monstey_WTools)
        {
            if (std::string_view(row.status) == "complete")
                ++complete;
            if (std::string_view(row.status) == "partial")
                ++partial;
        }
        return J::object(
            {{"api_version", "cpp-world-0.1.0"},
             {"tools", static_cast<std::uint64_t>(std::size(Monstey_WTools))},
             {"complete_operations", complete},
             {"partial_operations", partial},
             {"pending_operations", static_cast<std::uint64_t>(std::size(Monstey_WTools)) - complete - partial},
             {"catalog", Monstey_WorldCatalog()},
             {"implementation_language", "C++20"},
             {"python_runtime", false},
             {"runtime_verified", false}});
    }
    if (name == "sow_world_navigation_inspect" || name == "sow_world_navigation_apply" ||
        name == "sow_world_navigation_build")
        return Monstey_NavigationWorldCall(name, a);
    if (name.starts_with("sow_blender_"))
        return Monstey_WorldBlenderCall(name, a);
    if (name == "sow_worldedit_compile")
        return Monstey_WorldEditCall(name, a);
    if (name == "sow_static_prefab_convert")
        return Monstey_StaticPrefabConvert(a);
    if (name == "sow_world_light_build")
        return Monstey_WorldLightingCall(name, a);
    if (name == "sow_asset_inspect")
    {
        Monstey_Require(a.is_object() && a.size() == 1 && a.contains("source"),
                        "Asset inspection requires only source");
        return Monstey_AssetInspect(Monstey_WPath(a.at("source")));
    }
    if (name == "sow_world_asset_search" || name == "sow_world_asset_extract")
        return Monstey_WorldAssetsCall(name, a);
    if (name == "sow_private_section_build")
        return Monstey_WorldCompositionCall(name, a);
    if (name == "sow_world_physics_inspect" || name == "sow_world_physics_apply" ||
        name == "sow_world_collision_boxes_build" || name == "sow_world_collision_triangles_build")
        return Monstey_PhysicsWorldCall(name, a);
    if (std::string_view(tool->status) == "pending")
        throw Monstey_Error("world_operation_not_ported",
                            std::string(name) + " remains in the required C++ world port; no Python fallback is used");
    if (name == "sow_world_topology_build" || name == "sow_world_render_build")
        return Monstey_WAuthoredCall(name, a);
    if (name == "sow_wdsc_binding_build")
        return Monstey_WBinding(a);
    auto source = Monstey_WPath(a.at("source"));
    auto raw = Monstey_ReadFile(source);
    Monstey_WExpected(a, raw);
    if (name.starts_with("sow_objres_"))
    {
        Monstey_WObjres doc(raw);
        if (name == "sow_objres_inspect")
            return doc.Report();
        if (name == "sow_objres_query")
            return doc.Query(a);
        if (name == "sow_objres_classes")
        {
            std::map<std::string, std::map<std::uint32_t, std::set<unsigned>>> classes;
            auto &bytes = doc.chunks[doc.obj_chunk].bytes;
            for (auto &obj : doc.objects)
            {
                auto kind = Monstey_WName(doc.classes, Monstey_U32(bytes, obj.at));
                if (a.contains("class_name") && a.at("class_name").as_string() != kind)
                    continue;
                auto record = Monstey_U32(bytes, obj.at + 10);
                for (auto &d : doc.db->records[record].fields)
                    classes[kind][d.hash].insert(d.type);
            }
            J::Array rows;
            for (auto &[kind, fields] : classes)
            {
                J::Array properties;
                for (auto &[hash, types] : fields)
                {
                    J::Array ts;
                    for (auto type : types)
                        ts.push_back(type);
                    properties.push_back(J::object({{"hash", Monstey_Hex(hash)}, {"native_types", ts}}));
                }
                rows.push_back(J::object({{"class_name", kind}, {"properties", properties}}));
            }
            return J::object({{"classes", rows}, {"source_sha256", Monstey_HashSha256(raw)}});
        }
        J edits;
        if (name == "sow_objres_edit")
            edits = Monstey_WObjresEdit(doc, a.at("spec"));
        if (name == "sow_objres_add")
            edits = Monstey_WObjresEdit(doc, J::object({{"objects", a.at("objects")}}));
        if (name == "sow_objres_apply")
            doc.Apply(a.at("updates"));
        auto bytes = doc.Encode();
        Monstey_WObjres check(bytes);
        Monstey_Require(check.Encode() == bytes, "OBJRES readback differs");
        if (!a.contains("output"))
        {
            Monstey_Require(name == "sow_objres_roundtrip", "World output required");
            return doc.Report();
        }
        auto report = doc.Report();
        if (!edits.is_null())
            report["changes"] = edits;
        return Monstey_WPublish(source, raw, a, bytes, report);
    }
    if (name.starts_with("sow_inst_"))
    {
        Monstey_WInst doc(raw);
        if (name == "sow_inst_inspect")
            return doc.Report(a.value("instances_limit", 100).as_uint());
        auto result = J::object({{"format", "PRFB8"}, {"roundtrip_byte_exact", true}});
        if (name == "sow_inst_apply")
        {
            result["changes"] = doc.Apply(a.at("spec"));
            result["roundtrip_byte_exact"] = false;
            result["geometry_and_material_bytes_preserved"] = true;
            result["world_WDSC_binding_required"] = true;
        }
        auto bytes = doc.Encode();
        Monstey_WInst check(bytes);
        Monstey_Require(check.Encode() == bytes, "PRFB readback differs");
        if (!a.contains("output"))
        {
            Monstey_Require(name == "sow_inst_roundtrip", "World output required");
            return result;
        }
        return Monstey_WPublish(source, raw, a, bytes, result);
    }
    if (name == "sow_world_render_inspect")
        return Monstey_WRender(raw).Report();
    if (name == "sow_worldmesh_inspect" || name == "sow_worldmesh_apply" || name == "sow_worldmesh_export")
    {
        Monstey_WMesh mesh(raw);
        if (name == "sow_worldmesh_inspect")
            return mesh.Report();
        if (name == "sow_worldmesh_export")
        {
            auto gi = a.value("group_index", 0).as_uint(), di = a.value("draw_index", 0).as_uint();
            auto bytes = mesh.Export(static_cast<std::size_t>(gi), static_cast<std::size_t>(di));
            return Monstey_WPublish(source, raw, a, bytes,
                                    J::object({{"format", "NPZ"},
                                               {"group_index", gi},
                                               {"draw_index", di},
                                               {"all_native_channels_byte_exact", true},
                                               {"indices_byte_exact", true},
                                               {"coordinates", "Native centimetres, Y-up; raw encodings retained"}}));
        }
        if (a.at("spec").contains("topology"))
        {
            for (auto &[key, value] : a.at("spec").as_object())
            {
                (void)value;
                Monstey_Require(key == "source_sha256" || key == "topology",
                                "Fixed and topology changes must be separate transactions");
            }
            auto topology = a.at("spec").at("topology");
            if (a.at("spec").contains("source_sha256"))
                topology["source_sha256"] = a.at("spec").at("source_sha256");
            auto result = Monstey_WTopology(mesh, topology);
            return Monstey_WPublish(source, raw, a, result.first, result.second);
        }
        auto result = mesh.Apply(a.at("spec"));
        return Monstey_WPublish(source, raw, a, mesh.raw, result);
    }
    if (name == "sow_section_inspect" || name == "sow_section_roundtrip")
    {
        auto result = Monstey_WSection(raw, a.value("format", "").as_string());
        if (name == "sow_section_inspect")
            return result;
        return Monstey_WPublish(source, raw, a, raw, result);
    }
    throw Monstey_Error("world_dispatch_incomplete", "Declared native world handler is missing");
}

} // namespace Monstey
