#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

Monstey_WMatrix Monstey_WSceneMatrix(const J &position, const J &quaternion, double scale)
{
    auto p = Monstey_WVector(position, 3), q = Monstey_WVector(quaternion, 4);
    auto rotation = Monstey_WRotation(q);
    Monstey_Require(std::isfinite(scale) && scale > 0, "Source frame needs positive uniform scale");
    Monstey_WMatrix matrix{};
    matrix[3][3] = 1;
    for (std::size_t i = 0; i < 3; ++i)
    {
        for (std::size_t k = 0; k < 3; ++k)
            matrix[i][k] = rotation[i][k] * scale;
        matrix[i][3] = p[i];
    }
    return matrix;
}

J Monstey_WSceneJsonMatrix(const Monstey_WMatrix &matrix)
{
    J::Array rows;
    for (auto &row : matrix)
        rows.push_back(J::array({row[0], row[1], row[2], row[3]}));
    return rows;
}

Monstey_WMatrix Monstey_WSceneMultiply(const Monstey_WMatrix &a, const Monstey_WMatrix &b)
{
    Monstey_WMatrix out{};
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j)
            for (std::size_t k = 0; k < 4; ++k)
                out[i][j] += a[i][k] * b[k][j];
    return out;
}

std::pair<J, J> Monstey_WSceneSelect(const J::Array &rows, const J &select)
{
    static const std::set<std::string> keys{"offset",        "limit",   "names",          "class_names",
                                            "name_contains", "indices", "include_unowned"};
    for (auto &[key, value] : select.as_object())
    {
        (void)value;
        Monstey_Require(keys.contains(key), "Unknown scene selection field");
    }
    auto offset = select.value("offset", 0).as_uint(), limit = select.value("limit", 100).as_uint();
    Monstey_Require(limit <= 5000, "Scene selection limit exceeds5000");
    auto needle = Monstey_WLower(select.value("name_contains", "").as_string());
    J::Array matches, selected;
    auto contains = [](const J &list, const J &value)
    { return std::find(list.as_array().begin(), list.as_array().end(), value) != list.as_array().end(); };
    for (auto &row : rows)
    {
        if (select.contains("names") && !contains(select.at("names"), row.at("name")))
            continue;
        if (select.contains("class_names") && !contains(select.at("class_names"), row.at("class_name")))
            continue;
        if (Monstey_WLower(row.at("name").as_string()).find(needle) == std::string::npos)
            continue;
        auto index = row.contains("instance_index") ? row.at("instance_index") : row.at("object_index");
        if (select.contains("indices") && !contains(select.at("indices"), index))
            continue;
        matches.push_back(row);
    }
    for (std::size_t i = static_cast<std::size_t>(std::min<std::uint64_t>(offset, matches.size()));
         i < matches.size() && selected.size() < limit; ++i)
        selected.push_back(matches[i]);
    return {selected, J::object({{"total", static_cast<std::uint64_t>(rows.size())},
                                 {"matched", static_cast<std::uint64_t>(matches.size())},
                                 {"offset", offset},
                                 {"limit", limit},
                                 {"returned", static_cast<std::uint64_t>(selected.size())},
                                 {"more", offset + selected.size() < matches.size()}})};
}

std::pair<J, std::string> Monstey_WScenePrefab(const B &raw, const J &provenance, const J &definition_key)
{
    Monstey_WInst prefab(raw);
    std::vector<std::size_t> choices;
    for (std::size_t i = 0; i < prefab.primary.size(); ++i)
    {
        auto &d = prefab.primary[i];
        if (!definition_key.is_null() ? Monstey_U32(d, 0) == Monstey_WUint(definition_key)
                                      : (Monstey_U32(d, 12) & 65535) != 0)
            choices.push_back(i);
    }
    if (choices.size() != 1)
        return {J{}, "Select exactly one native primary geometry definition"};
    auto &definition = prefab.primary[choices[0]];
    auto first = Monstey_U32(definition, 8) & 65535, count = Monstey_U32(definition, 12) & 65535;
    if (!count)
        return {J{}, "Native definition has no direct render draws"};
    if (Monstey_U32(definition, 16) != 0xffffffffu)
        return {J{}, "Automatic secondary LOD selection is not qualified for a scene geometry reference"};
    B mesh{'M', 'E', 'S', 'H'};
    for (auto word : {5u, 1u, prefab.header[4], prefab.header[9], prefab.header[10], prefab.header[11], 0u})
        Monstey_AppendU32(mesh, word);
    mesh.insert(mesh.end(), prefab.geometry.begin(), prefab.geometry.end());
    Monstey_WMesh decoded(std::move(mesh));
    Monstey_Require(first <= decoded.groups[0].draws.size() && count <= decoded.groups[0].draws.size() - first,
                    "PRFB definition draw range exceeds native geometry");
    std::array<double, 3> lo, hi;
    lo.fill(std::numeric_limits<double>::infinity());
    hi.fill(-std::numeric_limits<double>::infinity());
    std::uint64_t vertices{}, triangles{};
    J::Array parts;
    for (std::size_t di = first; di < std::size_t(first) + count; ++di)
    {
        auto attrs = decoded.Attributes(0, di);
        auto pos = attrs.find("POSITION");
        Monstey_Require(pos != attrs.end() && pos->second.width >= 3, "PRFB geometry lacks native positions");
        if (pos->second.storage == 4 || pos->second.storage == 9)
            return {J{}, "Integer POSITION needs an explicit native dequantization contract"};
        auto indices = decoded.Indices(0, di);
        for (auto index : indices)
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                auto x = decoded.Component(pos->second, index, axis);
                Monstey_Require(std::isfinite(x), "Nonfinite indexed PRFB position");
                lo[axis] = std::min(lo[axis], x);
                hi[axis] = std::max(hi[axis], x);
            }
        vertices += pos->second.count;
        triangles += indices.size() / 3;
        parts.push_back(static_cast<std::uint64_t>(di));
    }
    Monstey_Require(std::isfinite(lo[0]), "PRFB geometry has no indexed triangles");
    return {J::object({{"format", "PRFB8"},
                       {"source", provenance},
                       {"definition_key", Monstey_Hex(Monstey_U32(definition, 0))},
                       {"part_indices", parts},
                       {"vertices", vertices},
                       {"triangles", triangles},
                       {"bounds_cm", J::array({J::array({lo[0], lo[1], lo[2]}), J::array({hi[0], hi[1], hi[2]})})},
                       {"material_paths", prefab.material_names},
                       {"representation", "REAL_NATIVE_RIGID_TRIANGLES"},
                       {"texture_mapping_preserved", true}}),
            ""};
}

J Monstey_WScenePrepare(const J &a)
{
    auto spec = a.at("spec");
    Monstey_Require(spec.is_object() && spec.contains("sources") && spec.at("sources").is_array() &&
                        !spec.at("sources").empty(),
                    "Scene specification needs sources");
    for (auto &[key, value] : spec.as_object())
    {
        (void)value;
        Monstey_Require(key == "sources" || key == "asset_bindings" || key == "title" || key == "origin_native_cm",
                        "Unknown world scene field");
    }
    std::map<std::filesystem::path, std::string> checksums;
    std::map<std::string, B> binding_bytes;
    J::Object bindings, sources;
    J::Array objects, geometry, warnings, catalog;
    auto record = [&](const std::filesystem::path &path, const B &raw)
    {
        auto hash = Monstey_HashSha256(raw);
        auto [it, inserted] = checksums.emplace(path, hash);
        Monstey_Require(inserted || it->second == hash, "Scene input changed");
        return J::object(
            {{"path", Monstey_WTextPath(path)}, {"bytes", static_cast<std::uint64_t>(raw.size())}, {"sha256", hash}});
    };
    auto asset_bindings = spec.value("asset_bindings", J::array());
    for (auto &entry : asset_bindings.as_array())
    {
        auto path = Monstey_WPath(entry.at("source"));
        auto raw = Monstey_ReadFile(path);
        auto resource = Monstey_ArchiveVirtual(entry.at("resource_path").as_string());
        Monstey_Require(!bindings.contains(resource), "Duplicate native scene asset binding");
        bindings.emplace(resource, record(path, raw));
        binding_bytes.emplace(resource, std::move(raw));
    }
    for (std::size_t si = 0; si < spec.at("sources").size(); ++si)
    {
        auto &entry = spec.at("sources")[si];
        static const std::set<std::string> source_keys{
            "id",     "source",           "resource_path",          "frame", "frame_to_scene",
            "select", "include_geometry", "geometry_definition_key"};
        for (auto &[key, value] : entry.as_object())
        {
            (void)value;
            Monstey_Require(source_keys.contains(key), "Unknown scene source field");
        }
        auto id = entry.value("id", "s" + std::to_string(si)).as_string();
        Monstey_Require(!id.empty() && !sources.contains(id) &&
                            (std::isalpha(static_cast<unsigned char>(id[0])) || id[0] == '_') &&
                            std::all_of(id.begin(), id.end(),
                                        [](unsigned char c) { return c < 128 && (std::isalnum(c) || c == '_'); }),
                        "Unique ASCII scene source id required");
        auto path = Monstey_WPath(entry.at("source"));
        auto raw = Monstey_ReadFile(path);
        auto provenance = record(path, raw);
        auto resource = Monstey_ArchiveVirtual(entry.at("resource_path").as_string());
        auto frame = entry.at("frame").as_string();
        Monstey_Require(frame == "world" || frame == "local", "Native source frame must be world or local");
        Monstey_Require(frame != "local" || entry.contains("frame_to_scene"),
                        "Local source needs explicit frame_to_scene");
        auto transform = entry.value(
            "frame_to_scene",
            J::object({{"position", J::array({0, 0, 0})}, {"quaternion", J::array({0, 0, 0, 1})}, {"scale", 1}}));
        for (auto &[key, value] : transform.as_object())
        {
            (void)value;
            Monstey_Require(key == "position" || key == "quaternion" || key == "scale",
                            "Unknown source frame transform field");
        }
        auto matrix = Monstey_WSceneMatrix(transform.at("position"), transform.at("quaternion"),
                                           transform.value("scale", 1).as_number());
        auto selection = entry.value("select", J::object());
        bool include = entry.value("include_geometry", true).as_bool();
        J::Array rows;
        J page;
        std::string backend;
        if (raw.size() >= 4 && std::memcmp(raw.data(), "OBJR", 4) == 0)
        {
            backend = "OBJRES";
            Monstey_WObjres doc(raw);
            auto &data = doc.chunks[doc.obj_chunk].bytes;
            for (auto &owner : doc.objects)
            {
                auto prop = Monstey_U32(data, owner.at + 10);
                auto row = J::object(
                    {{"id", id + ":objres:" + std::to_string(owner.group) + ":" + std::to_string(owner.index)},
                     {"source_id", id},
                     {"backend", backend},
                     {"name", Monstey_WName(doc.names, Monstey_U32(data, owner.at + 4))},
                     {"class_name", Monstey_WName(doc.classes, Monstey_U32(data, owner.at))},
                     {"group_index", owner.group},
                     {"object_index", owner.index},
                     {"property_index", prop},
                     {"original_record_sha256",
                      Monstey_HashSha256(std::span<const std::uint8_t>(data).subspan(owner.at, 50))},
                     {"transform", J::object({{"position_cm", Monstey_WFloats(data, owner.at + 14, 3)},
                                              {"quaternion_xyzw", Monstey_WFloats(data, owner.at + 26, 4)},
                                              {"uniform_scale", 1}})}});
                rows.push_back(std::move(row));
            }
            auto chosen = Monstey_WSceneSelect(rows, selection);
            rows = chosen.first.as_array();
            page = chosen.second;
            std::map<std::string, std::size_t> counts;
            for (auto &owner : doc.objects)
                ++counts[Monstey_WName(doc.classes, Monstey_U32(data, owner.at))];
            std::set<std::string> noted;
            for (auto &row : rows)
            {
                auto prop = Monstey_WUint(row.at("property_index"));
                auto fields = doc.db->Fields(prop);
                J::Object values;
                for (auto &field : fields.as_array())
                    values.emplace(field.at("hash").as_string(), field.at("value"));
                row["properties"] = std::move(values);
                row["property_fields"] = fields;
                J::Array shared;
                for (auto &owner : doc.objects)
                    if (Monstey_U32(data, owner.at + 10) == prop)
                        shared.push_back(J::object({{"group_index", owner.group}, {"object_index", owner.index}}));
                row["shared_property_users"] = shared;
                auto kind = row.at("class_name").as_string();
                if (noted.insert(kind).second)
                    catalog.push_back(
                        J::object({{"source_id", id},
                                   {"class_name", kind},
                                   {"count", static_cast<std::uint64_t>(counts[kind])},
                                   {"example_identity", J::object({{"name", row.at("name")},
                                                                   {"group_index", row.at("group_index")},
                                                                   {"object_index", row.at("object_index")}})},
                                   {"schema_source", Monstey_WTextPath(path)},
                                   {"example_properties", row.at("properties")},
                                   {"example_fields", fields},
                                   {"complete_schema", false}}));
            }
        }
        else if (raw.size() >= 4 && std::memcmp(raw.data(), "PRFB", 4) == 0)
        {
            backend = "INST";
            Monstey_WInst doc(raw);
            for (std::size_t index = 0; index < doc.instances.size(); ++index)
            {
                auto &item = doc.instances[index];
                auto owners = doc.Owners(index);
                if (owners.empty() && !selection.value("include_unowned", false).as_bool())
                    continue;
                auto prefab = doc.names[Monstey_U32(item, 0)];
                rows.push_back(J::object(
                    {{"id", id + ":inst:" + std::to_string(index)},
                     {"source_id", id},
                     {"backend", backend},
                     {"name",
                      "Instance" + std::to_string(index) + ": " + std::filesystem::path(prefab).stem().string()},
                     {"class_name", "PRFB_INSTANCE"},
                     {"instance_index", static_cast<std::uint64_t>(index)},
                     {"prefab_path", prefab},
                     {"definition_key", Monstey_Hex(Monstey_U32(item, 4))},
                     {"owners", owners},
                     {"original_record_sha256", Monstey_HashSha256(item)},
                     {"transform", J::object({{"position_cm", Monstey_WFloats(item, 40, 3)},
                                              {"quaternion_xyzw", Monstey_WFloats(item, 52, 4)},
                                              {"uniform_scale", static_cast<double>(Monstey_WReadFloat(item, 68))}})},
                     {"properties", J::object()}}));
            }
            auto chosen = Monstey_WSceneSelect(rows, selection);
            rows = chosen.first.as_array();
            page = chosen.second;
            if (include)
            {
                for (auto &row : rows)
                {
                    auto prefab = row.at("prefab_path").as_string();
                    if (!bindings.contains(prefab))
                    {
                        warnings.push_back(J::object(
                            {{"object_id", row.at("id")},
                             {"reason", "No explicit native asset binding; placement remains a transform reference"},
                             {"resource_path", prefab}}));
                        continue;
                    }
                    auto shape =
                        Monstey_WScenePrefab(binding_bytes.at(prefab), bindings.at(prefab), row.at("definition_key"));
                    if (shape.first.is_null())
                        warnings.push_back(J::object({{"object_id", row.at("id")}, {"reason", shape.second}}));
                    else
                    {
                        auto g = shape.first;
                        g["id"] = row.at("id").as_string() + ":mesh";
                        g["parent_object_id"] = row.at("id");
                        g["source_id"] = id;
                        geometry.push_back(g);
                    }
                }
                if (doc.header[4])
                {
                    auto shape = Monstey_WScenePrefab(raw, provenance, entry.value("geometry_definition_key", J{}));
                    if (shape.first.is_null())
                        warnings.push_back(J::object({{"source_id", id}, {"reason", shape.second}}));
                    else
                    {
                        auto g = shape.first;
                        g["id"] = id + ":direct-geometry";
                        g["source_id"] = id;
                        g["parent_object_id"] = J{};
                        geometry.push_back(g);
                    }
                }
            }
        }
        else if (raw.size() >= 4 && std::memcmp(raw.data(), "MESH", 4) == 0)
        {
            backend = "MESH5";
            Monstey_WMesh mesh(raw);
            auto report = mesh.Report();
            J::Array choices;
            for (auto &group : report.at("groups").as_array())
                for (auto &draw : group.at("draws").as_array())
                    choices.push_back(J::object({{"name", "Group" + std::to_string(group.at("index").as_uint()) +
                                                              ".Draw" + std::to_string(draw.at("index").as_uint())},
                                                 {"class_name", "MESH5_DRAW"},
                                                 {"object_index", static_cast<std::uint64_t>(choices.size())},
                                                 {"group", group.at("index")},
                                                 {"draw", draw}}));
            auto chosen = Monstey_WSceneSelect(choices, selection);
            page = chosen.second;
            std::map<std::string, std::size_t> aliases;
            if (include)
                for (auto &choice : chosen.first.as_array())
                {
                    auto gi = choice.at("group").as_uint(), di = choice.at("draw").at("index").as_uint();
                    auto attrs = mesh.Attributes(static_cast<std::size_t>(gi), static_cast<std::size_t>(di));
                    auto pos = attrs.find("POSITION");
                    Monstey_Require(pos != attrs.end() && pos->second.width >= 3,
                                    "MESH scene geometry lacks 3D POSITION");
                    if (pos->second.storage == 4 || pos->second.storage == 9)
                    {
                        warnings.push_back(J::object({{"source_id", id},
                                                      {"group", gi},
                                                      {"draw", di},
                                                      {"reason", "Integer POSITION needs explicit dequantization"}}));
                        continue;
                    }
                    B signature;
                    for (std::size_t v = 0; v < pos->second.count; ++v)
                        for (std::size_t axis = 0; axis < 3; ++axis)
                            Monstey_AppendU32(signature, std::bit_cast<std::uint32_t>(
                                                             static_cast<float>(mesh.Component(pos->second, v, axis))));
                    for (auto index : mesh.Indices(static_cast<std::size_t>(gi), static_cast<std::size_t>(di)))
                        Monstey_AppendU16(signature, index);
                    auto hash = Monstey_HashSha256(signature);
                    if (aliases.contains(hash))
                    {
                        geometry[aliases.at(hash)]["display_alias_draws"].as_array().push_back(
                            J::object({{"group", gi}, {"draw", di}}));
                        continue;
                    }
                    auto &draw = choice.at("draw");
                    auto g = J::object({{"id", id + ":worldmesh:" + std::to_string(gi) + ":" + std::to_string(di)},
                                        {"source_id", id},
                                        {"parent_object_id", J{}},
                                        {"format", "MESH5"},
                                        {"source", provenance},
                                        {"group_index", gi},
                                        {"draw_index", di},
                                        {"display_alias_draws", J::array()},
                                        {"shared_vertex_draws", draw.at("shared_vertex_draws")},
                                        {"vertices", draw.at("vertices")},
                                        {"triangles", draw.at("triangles")},
                                        {"bounds_cm", J::array({draw.at("bounds_min"), draw.at("bounds_max")})},
                                        {"representation", "REAL_NATIVE_WORLD_TRIANGLES_REFERENCE"}});
                    aliases.emplace(hash, geometry.size());
                    geometry.push_back(std::move(g));
                }
        }
        else
            throw Monstey_Error("world_scene_source", "Only native OBJRES/PRFB/MESH5 scene sources are supported");
        sources.emplace(id, J::object({{"fixture", provenance},
                                       {"resource_path", resource},
                                       {"backend", backend},
                                       {"frame", frame},
                                       {"frame_to_scene_native", Monstey_WSceneJsonMatrix(matrix)},
                                       {"selection", page},
                                       {"include_geometry", include}}));
        for (auto &row : rows)
        {
            auto &t = row.at("transform");
            row["scene_matrix_native"] = Monstey_WSceneJsonMatrix(
                Monstey_WSceneMultiply(matrix, Monstey_WSceneMatrix(t.at("position_cm"), t.at("quaternion_xyzw"),
                                                                    t.at("uniform_scale").as_number())));
            objects.push_back(std::move(row));
        }
    }
    J origin = spec.value("origin_native_cm", J::array({0, 0, 0}));
    if (!spec.contains("origin_native_cm") && !objects.empty())
    {
        J::Array medians;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            std::vector<double> values;
            for (auto &row : objects)
                values.push_back(row.at("scene_matrix_native")[axis][3].as_number());
            std::sort(values.begin(), values.end());
            auto mid = values.size() / 2;
            medians.push_back(values.size() % 2 ? values[mid] : (values[mid - 1] + values[mid]) / 2);
        }
        origin = medians;
    }
    (void)Monstey_WVector(origin, 3);
    auto scene =
        J::object({{"schema", "Monstey_WorldBlenderScene_2"},
                   {"title", spec.value("title", "Native world scene")},
                   {"coordinate_contract",
                    J::object({{"native_units", "cm"},
                               {"native_axes", "Y up"},
                               {"quaternion_order", "XYZW"},
                               {"blender_units", "m"},
                               {"blender_axes", "Z up"},
                               {"matrix_convention", "column vector; scene=frame_to_scene_native @ native_object"}})},
                   {"origin_native_cm", origin},
                   {"input_spec", J{}},
                   {"source_spec", spec},
                   {"sources", sources},
                   {"objects", objects},
                   {"geometry", geometry},
                   {"native_class_catalog", catalog},
                   {"asset_bindings", bindings},
                   {"warnings", warnings},
                   {"game_modified", false},
                   {"compiled_geometry_editable", false}});
    auto output = Monstey_WPath(a.at("output")), scene_path = output / "scene.json",
         report_path = output / "preparation-report.json";
    std::vector<std::filesystem::path> source_paths;
    for (auto &[path, hash] : checksums)
    {
        (void)hash;
        source_paths.push_back(path);
    }
    Monstey_RequireOfflineOutput(scene_path, source_paths);
    Monstey_RequireOfflineOutput(report_path, source_paths);
    Monstey_Require(!std::filesystem::exists(scene_path) && !std::filesystem::exists(report_path),
                    "Use a fresh scene output");
    auto verify = [&]
    {
        for (auto &[path, hash] : checksums)
            Monstey_Require(Monstey_HashSha256(path) == hash, "Scene source changed during preparation");
    };
    verify();
    bool made_scene = false, made_report = false;
    J summary;
    try
    {
        Monstey_WriteNewText(scene_path, scene.dump(2) + "\n");
        made_scene = true;
        summary = J::object(
            {{"schema", scene.at("schema")},
             {"scene", J::object({{"path", Monstey_WTextPath(scene_path)},
                                  {"bytes", static_cast<std::uint64_t>(std::filesystem::file_size(scene_path))},
                                  {"sha256", Monstey_HashSha256(scene_path)}})},
             {"sources", static_cast<std::uint64_t>(sources.size())},
             {"objects", static_cast<std::uint64_t>(objects.size())},
             {"native_geometry_references", static_cast<std::uint64_t>(geometry.size())},
             {"warnings", warnings},
             {"game_modified", false}});
        Monstey_WriteNewText(report_path, summary.dump(2) + "\n");
        made_report = true;
        verify();
    }
    catch (...)
    {
        std::error_code ec;
        if (made_report)
            std::filesystem::remove(report_path, ec);
        if (made_scene)
            std::filesystem::remove(scene_path, ec);
        throw;
    }
    return summary;
}

} // namespace Monstey_WorldPrivate

} // namespace Monstey
