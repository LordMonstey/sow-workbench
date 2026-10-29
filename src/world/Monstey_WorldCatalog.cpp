#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

J Monstey_WSchemaObject(J::Object properties, std::initializer_list<const char *> required)
{
    J::Array keys;
    for (auto key : required)
        keys.push_back(key);
    return J::object({{"type", "object"},
                      {"properties", std::move(properties)},
                      {"required", keys},
                      {"additionalProperties", false}});
}

J Monstey_WSchemaArray(J item, unsigned minimum)
{
    return J::object({{"type", "array"}, {"items", std::move(item)}, {"minItems", minimum}});
}

J Monstey_WSchema(std::string_view name)
{
    if (name == "sow_private_section_build")
        return Monstey_WorldCompositionCatalog()[0].at("inputSchema");
    if (name == "sow_world_asset_search" || name == "sow_world_asset_extract")
    {
        const auto catalog = Monstey_WorldAssetsCatalog();
        for (const auto &row : catalog.as_array())
            if (row.at("name").as_string() == name)
                return row.at("inputSchema");
    }
    if (name == "sow_world_light_build")
        return Monstey_WorldLightingCatalog()[0].at("inputSchema");
    if (name == "sow_worldedit_compile")
        return Monstey_WorldEditCatalog()[0].at("inputSchema");
    if (name.starts_with("sow_blender_"))
    {
        const auto rows = Monstey_WorldBlenderCatalog();
        for (const auto &row : rows.as_array())
            if (row.at("name").as_string() == name)
                return row.at("inputSchema");
    }
    auto text = J::object({{"type", "string"}}),
         path = J::object(
             {{"type", "string"}, {"minLength", 1}, {"format", "path"}, {"description", "Absolute filesystem path"}}),
         integer = J::object({{"type", "integer"}, {"minimum", 0}}), object = J::object({{"type", "object"}}),
         sha = text;
    auto vector = [](unsigned n)
    {
        return J::object(
            {{"type", "array"}, {"items", J::object({{"type", "number"}})}, {"minItems", n}, {"maxItems", n}});
    };
    auto vec3 = vector(3), quat = vector(4);
    auto types =
        J::object({{"type", "object"},
                   {"additionalProperties", J::object({{"type", "integer"}, {"minimum", 1}, {"maximum", 255}})}});
    auto bool_schema = J::object({{"type", "boolean"}});
    auto limit = J::object({{"type", "integer"}, {"minimum", 1}, {"maximum", 500}, {"default", 100}});
    auto formats = J::object({{"type", "string"}, {"enum", J::array({"WDSC9", "MESH5", "HKAI3", "WorldPhysics"})}});
    if (name == "sow_world_navigation_static_tree")
    {
        auto hash =
            J::object({{"type", "string"}, {"minLength", 64}, {"maxLength", 64}, {"pattern", "^[0-9a-f]{64}$"}});
        auto pageLimit = J::object({{"type", "integer"}, {"minimum", 1}, {"maximum", 512}, {"default", 256}});
        return Monstey_WSchemaObject({{"path", path},
                                      {"expected_sha256", hash},
                                      {"mesh_node", integer},
                                      {"offset", integer},
                                      {"limit", pageLimit}},
                                     {"path", "expected_sha256"});
    }
    if (name == "sow_world_navigation_inspect")
        return Monstey_WSchemaObject({{"source", path}, {"owner", path}}, {"source"});
    if (name == "sow_world_navigation_apply")
    {
        auto move = Monstey_WSchemaObject({{"index", integer}, {"delta_cm", vec3}}, {"index", "delta_cm"});
        auto clonePiece = Monstey_WSchemaObject({{"index", integer}, {"new_name", text}, {"delta_cm", vec3}},
                                                {"index", "new_name", "delta_cm"});
        auto navSpec = Monstey_WSchemaObject({{"source_sha256", sha},
                                              {"owner_source_sha256", sha},
                                              {"translate_pieces", Monstey_WSchemaArray(move)},
                                              {"clone_pieces", Monstey_WSchemaArray(clonePiece)}});
        return Monstey_WSchemaObject({{"source", path},
                                      {"owner", path},
                                      {"spec", navSpec},
                                      {"output", path},
                                      {"owner_output", path},
                                      {"report", path}},
                                     {"source", "owner", "spec", "output", "owner_output"});
    }
    if (name == "sow_world_navigation_build")
    {
        auto positive = J::object({{"type", "number"}, {"exclusiveMinimum", 0}, {"maximum", 1000000000}});
        auto actor = Monstey_WSchemaObject(
            {{"name", text},
             {"radius_cm", positive},
             {"height_cm", positive},
             {"max_slope_degrees", J::object({{"type", "number"}, {"minimum", 0}, {"exclusiveMaximum", 89}})}},
            {"radius_cm", "height_cm"});
        auto points = Monstey_WSchemaArray(vec3, 2);
        points["maxItems"] = 100000;
        auto navRoute = Monstey_WSchemaObject({{"name", text}, {"points_cm", points}, {"require_pass", bool_schema}},
                                              {"name", "points_cm"});
        auto routes = Monstey_WSchemaArray(navRoute);
        routes["maxItems"] = 1024;
        auto vertices = Monstey_WSchemaArray(vec3, 3);
        vertices["maxItems"] = 1000000;
        auto triangle = Monstey_WSchemaArray(integer, 3);
        triangle["maxItems"] = 3;
        auto triangles = Monstey_WSchemaArray(triangle, 1);
        triangles["maxItems"] = 1000000;
        auto navSpec = Monstey_WSchemaObject(
            {{"source_sha256", sha},
             {"cache_template_sha256", sha},
             {"mesh_index", integer},
             {"mode", J::object({{"enum", J::array({"append", "replace"})}})},
             {"vertices_cm", vertices},
             {"triangles", triangles},
             {"actor", actor},
             {"headroom_cm", positive},
             {"weld_cm", J::object({{"type", "number"}, {"exclusiveMinimum", 0}, {"maximum", 100}})},
             {"input_space", J::object({{"enum", J::array({"mesh_local", "world"})}})},
             {"require_connection", bool_schema},
             {"routes", routes}},
            {"source_sha256", "vertices_cm", "triangles", "actor", "headroom_cm"});
        return Monstey_WSchemaObject({{"source", path},
                                      {"owner", path},
                                      {"owner_expected_sha256", sha},
                                      {"cache_template", path},
                                      {"spec", navSpec},
                                      {"output", path},
                                      {"report", path}},
                                     {"source", "spec", "output"});
    }
    if (name == "sow_world_physics_inspect")
        return Monstey_WSchemaObject(
            {{"source", path}, {"offset", integer}, {"limit", limit}, {"expected_sha256", sha}}, {"source"});
    if (name == "sow_world_physics_apply")
    {
        auto route = Monstey_WSchemaArray(integer);
        auto optionalRoute = route;
        optionalRoute["type"] = J::array({"array", "null"});
        auto rename = Monstey_WSchemaObject({{"old_name", text}, {"new_name", text}}, {"old_name", "new_name"});
        auto clone = Monstey_WSchemaObject({{"name", text}, {"new_name", text}, {"delta_cm", vec3}},
                                           {"name", "new_name", "delta_cm"});
        auto imported = Monstey_WSchemaObject({{"donor", path},
                                               {"donor_name", text},
                                               {"new_name", text},
                                               {"position_cm", vec3},
                                               {"quaternion_xyzw", quat},
                                               {"properties", J::object({{"type", J::array({"object", "null"})}})},
                                               {"group_index", integer}},
                                              {"donor", "donor_name", "new_name", "position_cm", "quaternion_xyzw"});
        auto transform = Monstey_WSchemaObject(
            {{"index", integer}, {"position_cm", vec3}, {"quaternion_xyzw", quat}, {"route", optionalRoute}},
            {"index", "position_cm", "quaternion_xyzw"});
        auto box = Monstey_WSchemaObject({{"index", integer}, {"half_extents_cm", vec3}, {"route", route}},
                                         {"index", "half_extents_cm"});
        auto spec = Monstey_WSchemaObject({{"source_sha256", sha},
                                           {"rename_owners", Monstey_WSchemaArray(rename)},
                                           {"clone_owners", Monstey_WSchemaArray(clone)},
                                           {"import_owners", Monstey_WSchemaArray(imported)},
                                           {"transforms", Monstey_WSchemaArray(transform)},
                                           {"boxes", Monstey_WSchemaArray(box)}});
        return Monstey_WSchemaObject({{"source", path},
                                      {"output", path},
                                      {"spec", spec},
                                      {"report", path},
                                      {"offset", integer},
                                      {"limit", limit},
                                      {"expected_sha256", sha}},
                                     {"source", "spec", "output"});
    }
    if (name == "sow_world_collision_triangles_build")
    {
        auto route = Monstey_WSchemaArray(integer);
        route["maxItems"] = 64;
        auto profile = Monstey_WSchemaObject({{"source_sha256", sha}, {"shape_index", integer}, {"route", route}},
                                             {"source_sha256", "shape_index"});
        auto placement = Monstey_WSchemaObject({{"position_cm", vec3}, {"quaternion_xyzw", quat}});
        auto vertices = Monstey_WSchemaArray(vec3, 3);
        vertices["maxItems"] = 65535;
        auto triangle = Monstey_WSchemaArray(integer, 3);
        triangle["maxItems"] = 3;
        auto triangles = Monstey_WSchemaArray(triangle, 1);
        triangles["maxItems"] = 65536;
        auto spec = Monstey_WSchemaObject(
            {{"schema", J::object({{"const", "Monstey_WorldTriangleCollision_1"}})},
             {"source_sha256", sha},
             {"owner", text},
             {"profile", profile},
             {"placement", placement},
             {"vertices_cm", vertices},
             {"triangles", triangles},
             {"max_error_cm", J::object({{"type", "number"}, {"exclusiveMinimum", 0}, {"maximum", 10}})}},
            {"source_sha256", "profile", "vertices_cm", "triangles"});
        return Monstey_WSchemaObject({{"source", path},
                                      {"profile_source", path},
                                      {"spec", spec},
                                      {"output", path},
                                      {"report", path},
                                      {"expected_sha256", sha},
                                      {"profile_donor_source", path},
                                      {"profile_donor", profile}},
                                     {"source", "profile_source", "spec", "output"});
    }
    if (name == "sow_world_collision_boxes_build")
    {
        auto route = Monstey_WSchemaArray(integer);
        route["maxItems"] = 64;
        auto profile = Monstey_WSchemaObject({{"source_sha256", sha}, {"shape_index", integer}, {"route", route}},
                                             {"source_sha256", "shape_index"});
        auto placement = Monstey_WSchemaObject({{"position_cm", vec3}, {"quaternion_xyzw", quat}});
        auto half = vec3;
        half["items"] = J::object({{"type", "number"}, {"minimum", 0.5}, {"maximum", 1000000000}});
        auto boxName = text;
        boxName["minLength"] = 1;
        boxName["maxLength"] = 255;
        auto box = Monstey_WSchemaObject(
            {{"name", boxName}, {"center_cm", vec3}, {"half_extents_cm", half}, {"quaternion_xyzw", quat}},
            {"name", "center_cm", "half_extents_cm"});
        auto boxes = Monstey_WSchemaArray(box, 1);
        boxes["maxItems"] = 4096;
        auto spec = Monstey_WSchemaObject({{"schema", J::object({{"const", "Monstey_WorldCollisionAuthor_1"}})},
                                           {"source_sha256", sha},
                                           {"owner", text},
                                           {"profile", profile},
                                           {"placement", placement},
                                           {"boxes", boxes}},
                                          {"source_sha256", "profile", "boxes"});
        return Monstey_WSchemaObject({{"source", path},
                                      {"profile_source", path},
                                      {"spec", spec},
                                      {"output", path},
                                      {"report", path},
                                      {"expected_sha256", sha}},
                                     {"source", "profile_source", "spec", "output"});
    }
    if (name == "sow_world_scene_prepare")
        return Monstey_WSchemaObject({{"spec", object}, {"output", path}}, {"spec", "output"});
    if (name == "sow_asset_inspect")
        return Monstey_WSchemaObject({{"source", path}}, {"source"});
    if (name == "sow_static_prefab_convert")
    {
        auto paths = Monstey_WSchemaArray(path);
        return Monstey_WSchemaObject({{"source", path},
                                      {"container", integer},
                                      {"material", path},
                                      {"shader", path},
                                      {"template", path},
                                      {"output", path},
                                      {"resource_path", text},
                                      {"material_path", text},
                                      {"texture_manifest", path},
                                      {"dependency_manifests", paths},
                                      {"expected_sha256", sha},
                                      {"template_expected_sha256", sha},
                                      {"shader_contract", path}},
                                     {"source", "material", "shader", "template", "output", "resource_path",
                                      "material_path", "texture_manifest"});
    }
    auto add = Monstey_WSchemaObject({{"class_name", text},
                                      {"name", text},
                                      {"position", vec3},
                                      {"quaternion", quat},
                                      {"properties", object},
                                      {"group_index", integer},
                                      {"property_types", types},
                                      {"schema_source", path}},
                                     {"class_name", "name", "position", "quaternion", "properties"});
    auto update = Monstey_WSchemaObject({{"name", text},
                                         {"rename_to", text},
                                         {"group_index", integer},
                                         {"object_index", integer},
                                         {"class_name", text},
                                         {"position", vec3},
                                         {"quaternion", quat},
                                         {"properties", object},
                                         {"remove_properties", Monstey_WSchemaArray(text)},
                                         {"property_types", types},
                                         {"schema_source", path}},
                                        {"name"});
    auto clone = Monstey_WSchemaObject({{"source_name", text},
                                        {"name", text},
                                        {"source_group_index", integer},
                                        {"source_object_index", integer},
                                        {"group_index", integer},
                                        {"position", vec3},
                                        {"quaternion", quat},
                                        {"properties", object},
                                        {"remove_properties", Monstey_WSchemaArray(text)},
                                        {"property_types", types},
                                        {"schema_source", path}},
                                       {"source_name", "name"});
    auto remove = Monstey_WSchemaObject(
        {{"name", text}, {"group_index", integer}, {"object_index", integer}, {"class_name", text}}, {"name"});
    auto obj_spec = Monstey_WSchemaObject({{"source_sha256", sha},
                                           {"updates", Monstey_WSchemaArray(update)},
                                           {"clones", Monstey_WSchemaArray(clone)},
                                           {"deletes", Monstey_WSchemaArray(remove)},
                                           {"objects", Monstey_WSchemaArray(add)}});
    auto owner = J::object(
        {{"anyOf",
          J::array({J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", std::uint64_t{0xffffffff}}}), text})}});
    auto scale = J::object({{"type", "number"}});
    auto inst_update = Monstey_WSchemaObject({{"index", integer},
                                              {"owner_definition_key", owner},
                                              {"position", vec3},
                                              {"quaternion", quat},
                                              {"scale", scale}},
                                             {"index"});
    auto inst_remove = Monstey_WSchemaObject({{"index", integer}, {"owner_definition_key", owner}}, {"index"});
    auto donor = Monstey_WSchemaObject({{"source", path}, {"sha256", sha}, {"index", integer}}, {"source", "index"});
    auto owner_donor =
        Monstey_WSchemaObject({{"source", path}, {"sha256", sha}, {"definition_index", integer}}, {"source"});
    auto target_donor = Monstey_WSchemaObject({{"source", path}, {"sha256", sha}}, {"source"});
    auto inst_add = Monstey_WSchemaObject({{"prefab_path", text},
                                           {"position", vec3},
                                           {"quaternion", quat},
                                           {"scale", scale},
                                           {"donor", donor},
                                           {"owner_definition_key", owner}},
                                          {"prefab_path", "position", "quaternion"});
    auto defined_add =
        Monstey_WSchemaObject({{"target_resource_path", text},
                               {"position", vec3},
                               {"quaternion", quat},
                               {"scale", scale},
                               {"target", target_donor},
                               {"placement_donor", donor},
                               {"target_definition_key", owner},
                               {"owner_definition_key", owner}},
                              {"target_resource_path", "position", "quaternion", "target", "placement_donor"});
    auto inst_spec = Monstey_WSchemaObject({{"source_sha256", sha},
                                            {"owner_donor", owner_donor},
                                            {"updates", Monstey_WSchemaArray(inst_update)},
                                            {"clones", Monstey_WSchemaArray(inst_update)},
                                            {"deletes", Monstey_WSchemaArray(inst_remove)},
                                            {"instances", Monstey_WSchemaArray(inst_add)},
                                            {"defined_instances", Monstey_WSchemaArray(defined_add)},
                                            {"resource_path", text}});
    auto mesh_update = Monstey_WSchemaObject({{"group_index", integer},
                                              {"draw_index", integer},
                                              {"translation", vec3},
                                              {"quaternion", quat},
                                              {"scale", J::object({{"type", "number"}, {"exclusiveMinimum", 0}})},
                                              {"attributes_npz", path}},
                                             {"group_index", "draw_index"});
    auto topology_draw = Monstey_WSchemaObject({{"source_draw", integer}, {"attributes_npz", path}, {"material", text}},
                                               {"source_draw"});
    auto topology_group = Monstey_WSchemaObject(
        {{"group_index", integer}, {"draws", Monstey_WSchemaArray(topology_draw)}}, {"group_index", "draws"});
    auto topology_append = Monstey_WSchemaObject(
        {{"template_group", integer}, {"draws", Monstey_WSchemaArray(topology_draw)}}, {"template_group", "draws"});
    auto topology_spec = Monstey_WSchemaObject({{"source_sha256", sha},
                                                {"groups", Monstey_WSchemaArray(topology_group)},
                                                {"append_groups", Monstey_WSchemaArray(topology_append)}});
    auto mesh_spec = Monstey_WSchemaObject(
        {{"source_sha256", sha}, {"updates", Monstey_WSchemaArray(mesh_update)}, {"topology", topology_spec}});
    auto render_mode =
        J::object({{"type", "string"}, {"enum", J::array({"private", "preserve-section"})}, {"default", "private"}});
    if (name == "sow_capabilities")
        return Monstey_WSchemaObject({});
    if (name == "sow_objres_inspect" || name == "sow_worldmesh_inspect" || name == "sow_world_render_inspect")
        return Monstey_WSchemaObject({{"source", path}}, {"source"});
    if (name == "sow_objres_query")
        return Monstey_WSchemaObject({{"source", path},
                                      {"name_contains", text},
                                      {"class_name", text},
                                      {"offset", integer},
                                      {"limit", limit},
                                      {"include_properties", bool_schema}},
                                     {"source"});
    if (name == "sow_objres_classes")
        return Monstey_WSchemaObject({{"source", path}, {"class_name", text}}, {"source"});
    if (name == "sow_objres_add")
        return Monstey_WSchemaObject({{"source", path},
                                      {"objects", Monstey_WSchemaArray(add, 1)},
                                      {"output", path},
                                      {"report", path},
                                      {"expected_sha256", sha}},
                                     {"source", "objects", "output"});
    if (name == "sow_objres_apply")
    {
        auto transform =
            Monstey_WSchemaObject({{"name", text}, {"class_name", text}, {"position", vec3}, {"quaternion", quat}},
                                  {"name", "position", "quaternion"});
        return Monstey_WSchemaObject({{"source", path},
                                      {"updates", Monstey_WSchemaArray(transform, 1)},
                                      {"output", path},
                                      {"report", path},
                                      {"expected_sha256", sha}},
                                     {"source", "updates", "output"});
    }
    if (name == "sow_objres_edit")
        return Monstey_WSchemaObject({{"source", path}, {"spec", obj_spec}, {"output", path}, {"report", path}},
                                     {"source", "spec", "output"});
    if (name == "sow_objres_roundtrip" || name == "sow_inst_roundtrip")
        return Monstey_WSchemaObject({{"source", path}, {"output", path}, {"report", path}}, {"source"});
    if (name == "sow_inst_inspect")
        return Monstey_WSchemaObject(
            {{"source", path},
             {"instances_limit", J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", 500}, {"default", 100}})}},
            {"source"});
    if (name == "sow_inst_apply")
        return Monstey_WSchemaObject(
            {{"source", path}, {"spec", inst_spec}, {"output", path}, {"report", path}, {"expected_sha256", sha}},
            {"source", "spec", "output"});
    if (name == "sow_section_inspect")
        return Monstey_WSchemaObject({{"source", path}, {"format", formats}}, {"source"});
    if (name == "sow_section_roundtrip")
        return Monstey_WSchemaObject({{"source", path}, {"format", formats}, {"output", path}, {"report", path}},
                                     {"source", "output"});
    if (name == "sow_world_topology_build")
        return Monstey_WSchemaObject(
            {{"source", path},
             {"spec", topology_spec},
             {"expected_sha256", sha},
             {"output", path},
             {"report", path},
             {"render_source", path},
             {"render_expected_sha256", sha},
             {"render_output", path},
             {"source_resource", text},
             {"target_resource", text},
             {"render_mode", render_mode},
             {"merge_sources",
              J::object({{"type", "array"},
                         {"minItems", 1},
                         {"maxItems", 63},
                         {"items", Monstey_WSchemaObject({{"id", text},
                                                          {"mesh_source", path},
                                                          {"mesh_expected_sha256", sha},
                                                          {"render_source", path},
                                                          {"render_expected_sha256", sha},
                                                          {"source_resource", text}},
                                                         {"id", "mesh_source", "mesh_expected_sha256", "render_source",
                                                          "render_expected_sha256", "source_resource"})}})},
             {"scene_template", path},
             {"scene_expected_sha256", sha}},
            {"source", "spec", "expected_sha256", "output"});
    if (name == "sow_world_render_build")
        return Monstey_WSchemaObject({{"mesh_source", path},
                                      {"mesh_expected_sha256", sha},
                                      {"topology", object},
                                      {"render_source", path},
                                      {"render_expected_sha256", sha},
                                      {"source_resource", text},
                                      {"target_resource", text},
                                      {"mode", render_mode},
                                      {"scene_template", path},
                                      {"scene_expected_sha256", sha},
                                      {"output", path},
                                      {"report", path}},
                                     {"mesh_source", "mesh_expected_sha256", "topology", "render_source",
                                      "render_expected_sha256", "source_resource", "target_resource", "output"});
    if (name == "sow_wdsc_binding_build")
        return Monstey_WSchemaObject({{"inst", path},
                                      {"resource", text},
                                      {"owner_key", owner},
                                      {"donor", path},
                                      {"output", path},
                                      {"report", path}},
                                     {"inst", "resource", "owner_key", "donor", "output"});
    if (name == "sow_worldmesh_export")
        return Monstey_WSchemaObject(
            {{"source", path}, {"group_index", integer}, {"draw_index", integer}, {"output", path}},
            {"source", "output"});
    if (name == "sow_worldmesh_apply")
        return Monstey_WSchemaObject({{"source", path}, {"spec", mesh_spec}, {"output", path}, {"report", path}},
                                     {"source", "spec", "output"});
    return Monstey_WSchemaObject({{"source", path}, {"output", path}, {"spec", object}});
}

const Monstey_WTool Monstey_WTools[41] = {
    {"sow_capabilities", "Native C++ world port capabilities", "complete", true},
    {"sow_asset_inspect", "Native asset format inspection", "complete", true},
    {"sow_objres_inspect", "Complete OBJR13/OBJL5/ObjectDB envelope and preservation audit", "complete", true},
    {"sow_objres_query", "Bounded native object/property query", "complete", true},
    {"sow_objres_add", "Independent native object authoring with same-class property types", "complete", false},
    {"sow_objres_apply", "Existing native object transform update", "complete", false},
    {"sow_objres_roundtrip", "Byte-exact OBJRES reconstruction", "complete", false},
    {"sow_inst_inspect", "Native PRFB8 definitions, geometry and instance inspection", "complete", true},
    {"sow_inst_apply", "Complete native PRFB8 ownership, donor and defined-geometry placement authoring", "complete",
     false},
    {"sow_inst_roundtrip", "Byte-exact PRFB8 reconstruction", "complete", false},
    {"sow_section_inspect", "Known WDSC9/MESH5/HKAI3/WorldPhysics envelope validation", "complete", true},
    {"sow_section_roundtrip", "Exact section-envelope copy", "complete", false},
    {"sow_wdsc_binding_build", "Native single-owner WDSC render binding from explicit donor", "complete", false},
    {"sow_private_section_build", "Private coherent section component composition", "complete", false},
    {"sow_static_prefab_convert", "Rigid MMSH-to-PRFB conversion", "complete", false},
    {"sow_blender_prepare", "Native keep Blender scene JSON preparation", "complete", false},
    {"sow_blender_attach_inst", "Blender scene prefab attachment", "complete", false},
    {"sow_blender_build", "Blender native world scene build", "complete", false},
    {"sow_blender_export", "Blender world edit export", "complete", false},
    {"sow_blender_validate", "Blender native edit validation", "complete", true},
    {"sow_world_asset_search", "Native asset catalogue search", "complete", true},
    {"sow_world_asset_extract", "Selective native world asset extraction", "complete", false},
    {"sow_objres_classes", "Native class and actual property type discovery", "complete", true},
    {"sow_objres_edit", "Native object/property CRUD and linked identity validation", "complete", false},
    {"sow_world_scene_prepare", "Multi-source native world scene preparation", "complete", false},
    {"sow_worldmesh_inspect", "Full native MESH5 vertex, indices, layouts and draw alias decoder", "complete", true},
    {"sow_worldmesh_export", "Exact raw encoded native geometry standard ZIP/NPY NPZ export", "complete", false},
    {"sow_worldmesh_apply", "Complete native MESH5 transforms, NPZ interchange and topology authoring", "complete",
     false},
    {"sow_world_physics_inspect", "Full native collision shapes and mappings", "complete", true},
    {"sow_world_physics_apply", "Native collision transform, owner and clone edits", "complete", false},
    {"sow_world_navigation_inspect", "Full HKAI topology, ownership and clearance inspection", "complete", true},
    {"sow_world_navigation_apply", "Native navigation dynamic-piece transform/clone", "complete", false},
    {"sow_worldedit_compile", "Coherent world edit bundle composition", "complete", false},
    {"sow_world_collision_boxes_build", "Exact native box collision and spatial registration", "complete", false},
    {"sow_world_collision_triangles_build", "Exact native compressed triangle collision and BVHs", "complete", false},
    {"sow_world_navigation_build", "Native HKAI walkable surface authoring and adjacency", "complete", false},
    {"sow_world_topology_build", "Arbitrary MESH5 topology and qualified common-context WDSC", "partial", false},
    {"sow_world_render_inspect", "Complete WDSC render dictionaries and scene associations", "complete", true},
    {"sow_world_render_build", "Indexed-draw WDSC authoring; multi-context and changed LOD mapping pending", "partial",
     false},
    {"sow_world_light_build", "Native Point/Spot light FX and owner authoring", "complete", false},
    {"sow_world_navigation_static_tree",
     "Cook and page an owned native static navigation tree from a verified HKAI3 source", "complete", true}};

} // namespace Monstey_WorldPrivate
using namespace Monstey_WorldPrivate;

J Monstey_WorldCatalog()
{
    J::Array rows;
    for (auto &tool : Monstey_WTools)
    {
        bool complete = std::string_view(tool.status) == "complete",
             available = std::string_view(tool.status) != "pending";
        auto schema = Monstey_WSchema(tool.name);
        rows.push_back(J::object(
            {{"name", tool.name},
             {"description", tool.description},
             {"implemented", complete},
             {"available", available},
             {"status", tool.status},
             {"inputSchema", schema},
             {"schema_complete", available},
             {"annotations",
              J::object({{"readOnlyHint", tool.read_only}, {"destructiveHint", false}, {"openWorldHint", false}})},
             {"implementation_language", "C++20"},
             {"runtime_verified", false}}));
    }
    return rows;
}

} // namespace Monstey
