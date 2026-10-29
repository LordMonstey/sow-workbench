#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

J Monstey_WAuthoredCall(std::string_view name, const J &a)
{
    const bool topology_call = name == "sow_world_topology_build";
    const auto source = Monstey_WPath(a.at(topology_call ? "source" : "mesh_source"));
    const auto original = Monstey_ReadFile(source);
    const auto expected = a.at(topology_call ? "expected_sha256" : "mesh_expected_sha256").as_string();
    Monstey_Require(expected == Monstey_HashSha256(original), "Authored native mesh source hash differs");
    Monstey_WMesh original_mesh(original);
    B mesh_bytes = original;
    J proof, result = J::object();
    if (topology_call)
    {
        auto authored = Monstey_WTopology(original_mesh, a.at("spec"));
        mesh_bytes = std::move(authored.first);
        proof = std::move(authored.second);
        result["topology"] = proof;
    }
    else
        proof = a.at("topology");
    const bool paired = !topology_call || a.contains("render_source") || a.contains("render_expected_sha256") ||
                        a.contains("render_output") || a.contains("source_resource") || a.contains("target_resource") ||
                        a.contains("scene_template") || a.contains("scene_expected_sha256") ||
                        a.contains("merge_sources");
    std::optional<std::filesystem::path> render_source, scene_source;
    B render_original, render_bytes, scene_original;
    std::vector<std::pair<std::filesystem::path, B>> merge_snapshots;
    if (paired)
    {
        Monstey_Require(a.contains("render_source") && a.contains("render_expected_sha256") &&
                            a.contains(topology_call ? "render_output" : "output") && a.contains("source_resource") &&
                            a.contains("target_resource"),
                        "Paired topology/render authoring requires every source/hash/output/resource argument");
        render_source = Monstey_WPath(a.at("render_source"));
        render_original = Monstey_ReadFile(*render_source);
        Monstey_Require(a.at("render_expected_sha256").as_string() == Monstey_HashSha256(render_original),
                        "Native render source hash differs");
        const auto mode = a.value(topology_call ? "render_mode" : "mode", "private").as_string();
        Monstey_Require(mode == "private" || mode == "preserve-section", "Unknown native render mode");
        if (a.contains("scene_template") || a.contains("scene_expected_sha256"))
        {
            Monstey_Require(mode == "private", "A scene template is only used by private scene authoring");
            Monstey_Require(a.contains("scene_template") && a.contains("scene_expected_sha256"),
                            "Scene template requires its explicit SHA256");
            scene_source = Monstey_WPath(a.at("scene_template"));
            scene_original = Monstey_ReadFile(*scene_source);
            Monstey_Require(a.at("scene_expected_sha256").as_string() == Monstey_HashSha256(scene_original),
                            "Native scene template source hash differs");
        }
        B culling_source = render_original;
        const B *scene_prototype = scene_source ? &scene_original : nullptr;
        if (a.contains("merge_sources"))
        {
            Monstey_Require(topology_call && mode == "private",
                            "Common-context source merging requires private paired topology authoring");
            const auto &requests = a.at("merge_sources").as_array();
            Monstey_Require(!requests.empty() && requests.size() <= 63,
                            "Select one to63 additional common-context sources");
            Monstey_WRender base_render(render_original);
            const auto resource = Monstey_ArchiveVirtual(a.at("source_resource").as_string());
            Monstey_Require(!base_render.models.empty() &&
                                base_render.Name(base_render.models[0].path_offset) == resource,
                            "Base source resource is not native static descriptor[0]");
            std::size_t target_context = SIZE_MAX;
            for (std::size_t gi = 0; gi < proof.at("groups").size(); ++gi)
                if (proof.at("groups")[gi].at("source_group").as_uint() == base_render.models[0].geometry_group)
                {
                    Monstey_Require(target_context == SIZE_MAX, "Base selected context has ambiguous topology copies");
                    target_context = gi;
                }
            Monstey_Require(target_context < proof.at("groups").size(),
                            "Base static context is absent from authored topology");
            std::vector<std::string> base_materials;
            for (auto offset : base_render.models[0].material_offsets)
                base_materials.push_back(base_render.Name(offset));
            std::map<std::uint32_t, std::string> assigned;
            const auto &base_draws = proof.at("groups")[target_context].at("draws");
            for (const auto &draw : base_draws.as_array())
            {
                const auto old_slot = Monstey_WUint(draw.at("source_material_slot"));
                const auto slot = Monstey_WUint(draw.at("material_slot"));
                Monstey_Require(old_slot < base_render.models[0].material_offsets.size() && slot <= 65535 &&
                                    !base_materials.empty(),
                                "Base topology material slot has no native source association");
                const auto material = draw.at("material").is_null()
                                          ? base_render.Name(base_render.models[0].material_offsets[old_slot])
                                          : Monstey_ArchiveVirtual(draw.at("material").as_string());
                const auto [it, inserted] = assigned.emplace(slot, material);
                Monstey_Require(inserted || it->second == material, "Conflicting base topology material association");
                while (base_materials.size() <= slot)
                    base_materials.push_back(base_materials[0]);
                base_materials[slot] = material;
            }
            std::vector<Monstey_WorldMergeSource> inputs;
            inputs.push_back(
                {"base", resource, Monstey_HashSha256(mesh_bytes), mesh_bytes, target_context, base_materials});
            std::vector<std::map<std::uint16_t, B>> prototypes;

            prototypes.push_back(Monstey_WCullPrototypes(base_render));
            std::map<std::string, std::size_t> source_ids{{"base", 0}};
            std::uint64_t input_extent = original.size() + render_original.size() + scene_original.size();
            for (const auto &request : requests)
            {
                const auto id = request.at("id").as_string();
                Monstey_Require(!id.empty() && id.size() <= 512 && source_ids.emplace(id, inputs.size()).second,
                                "Merge source IDs must be unique and cannot use reserved base");
                const auto mesh_path = Monstey_WPath(request.at("mesh_source"));
                const auto render_path = Monstey_WPath(request.at("render_source"));
                auto geometry = Monstey_ReadFile(mesh_path);
                auto culling = Monstey_ReadFile(render_path);
                Monstey_Require(request.at("mesh_expected_sha256").as_string() == Monstey_HashSha256(geometry) &&
                                    request.at("render_expected_sha256").as_string() == Monstey_HashSha256(culling),
                                "Additional native source snapshot hash differs");
                input_extent += geometry.size() + culling.size();
                Monstey_Require(input_extent <= 512ull * 1024 * 1024, "Common-context source snapshots exceed512MiB");
                Monstey_WRender render(culling);
                const auto path = Monstey_ArchiveVirtual(request.at("source_resource").as_string());
                Monstey_Require(!render.models.empty() && render.Name(render.models[0].path_offset) == path,
                                "Additional resource is not its static descriptor[0]");
                std::vector<std::string> materials;
                for (auto offset : render.models[0].material_offsets)
                    materials.push_back(render.Name(offset));
                inputs.push_back(
                    {id, path, Monstey_HashSha256(geometry), geometry, render.models[0].geometry_group, materials});
                prototypes.push_back(Monstey_WCullPrototypes(render));
                merge_snapshots.emplace_back(mesh_path, std::move(geometry));
                merge_snapshots.emplace_back(render_path, std::move(culling));
            }
            auto merged = Monstey_WorldMerge(inputs);
            std::vector<B> culls;
            J::Array draws;
            for (const auto &remap : merged.report.at("draw_remap").as_array())
            {
                const auto source_index = source_ids.at(remap.at("source").as_string());
                const auto input_draw = static_cast<std::size_t>(remap.at("source_draw").as_uint());
                const auto output_draw = remap.at("output_draw").as_uint();
                const auto old_draw =
                    source_index == 0 ? base_draws[input_draw].at("source_draw").as_uint() : input_draw;
                Monstey_Require(old_draw <= 65535 && output_draw <= 65535 && output_draw == draws.size() &&
                                    prototypes[source_index].contains(static_cast<std::uint16_t>(old_draw)),
                                "Merged draw lacks its native source culling prototype");
                auto record = prototypes[source_index].at(static_cast<std::uint16_t>(old_draw));
                Monstey_PutU16(record, 28, static_cast<std::uint16_t>(output_draw));
                culls.push_back(std::move(record));
                const auto slot = remap.at("output_material_slot").as_uint();
                draws.push_back(J::object({{"draw_index", output_draw},
                                           {"source_group", static_cast<std::uint64_t>(target_context)},
                                           {"source_draw", output_draw},
                                           {"partition", 0},
                                           {"material", merged.material_paths.at(static_cast<std::size_t>(slot))},
                                           {"material_slot", slot},
                                           {"source_material_slot", slot}}));
            }
            // This in-memory donor carries captured raw cull prototypes only. Native spatial
            // nodes are authored below from the original whole render source, never this empty suffix.
            base_render.models.clear();
            base_render.bindings.clear();
            base_render.groups.clear();
            base_render.scene.assign(52, 0);
            Monstey_WRender::Model model{base_render.Intern(resource), static_cast<std::uint32_t>(target_context), {}};
            for (const auto &material : merged.material_paths)
                model.material_offsets.push_back(base_render.Intern(material));
            base_render.models.push_back(std::move(model));
            base_render.groups.push_back({std::move(culls), {}, {}});
            culling_source = base_render.Encode();
            proof["input_topology_sha256"] = proof.at("output_sha256");
            proof["output_sha256"] = Monstey_HashSha256(merged.mesh);
            proof["target_context"] = static_cast<std::uint64_t>(target_context);
            proof["groups"][target_context]["source_group"] = static_cast<std::uint64_t>(target_context);
            proof["groups"][target_context]["draws"] = std::move(draws);
            proof["groups"][target_context]["source_draw_count"] =
                static_cast<std::uint64_t>(base_render.groups[0].records.size());
            result["merge"] = std::move(merged.report);
            result["merge"]["culling_prototypes_sha256"] = Monstey_HashSha256(culling_source);
            mesh_bytes = std::move(merged.mesh);
            if (!scene_prototype)
                scene_prototype = &render_original;
        }
        auto rendered =
            Monstey_WRenderBuild(Monstey_WMesh(mesh_bytes), proof, culling_source, a.at("source_resource").as_string(),
                                 a.at("target_resource").as_string(), mode == "preserve-section", scene_prototype);
        render_bytes = std::move(rendered.first);
        result["render"] = std::move(rendered.second);
        if (a.contains("merge_sources"))
        {
            result["render"]["culling_prototype_source_sha256"] = result["render"].at("source_sha256");
            result["render"]["source_sha256"] = Monstey_HashSha256(render_original);
            const auto context = static_cast<std::size_t>(proof.at("target_context").as_uint());
            for (const auto &group : result["render"].at("groups").as_array())
                for (std::size_t row = 0; row < group.at("static_draws").size(); ++row)
                {
                    const auto draw = static_cast<std::size_t>(group.at("static_draws")[row].as_uint());
                    const auto &bounds = group.at("bounds")[row];
                    proof["groups"][context]["draws"][draw]["bounds_min"] = J::array({bounds[0], bounds[1], bounds[2]});
                    proof["groups"][context]["draws"][draw]["bounds_max"] = J::array({bounds[3], bounds[4], bounds[5]});
                }
            result["topology"] = proof;
            result["render"]["native_scene"]["explicit_scene_template"] = scene_source.has_value();
            result["render"]["native_scene"]["prototype_origin"] =
                scene_source ? "explicit_scene_template" : "base_render_source";
            result["merge"]["render_associations_written"] = true;
            result["merge"]["indexed_culling_bounds_rebuilt"] = true;
            result["merge"]["native_scene_group_routing_built"] =
                result["render"].at("native_scene").value("group_routing_built", false);
        }
    }
    std::vector<std::pair<std::filesystem::path, B>> outputs;
    const auto output = Monstey_WPath(a.at("output"));
    if (topology_call)
        outputs.emplace_back(output, mesh_bytes);
    if (paired)
    {
        const auto render_output = topology_call ? Monstey_WPath(a.at("render_output")) : output;
        outputs.emplace_back(render_output, render_bytes);
        result["render_output_path"] = Monstey_WTextPath(render_output);
    }
    result["output_path"] = Monstey_WTextPath(output);
    result["runtime_verified"] = false;
    result["game_written"] = false;
    if (a.contains("report"))
        outputs.emplace_back(Monstey_WPath(a.at("report")), B{});
    std::vector<std::filesystem::path> sources{source};
    if (render_source)
        sources.push_back(*render_source);
    if (scene_source)
        sources.push_back(*scene_source);
    for (const auto &[path, bytes] : merge_snapshots)
    {
        (void)bytes;
        sources.push_back(path);
    }
    std::set<std::filesystem::path> unique;
    for (const auto &[path, data] : outputs)
    {
        (void)data;
        Monstey_RequireOfflineOutput(path, sources);
        Monstey_Require(!std::filesystem::exists(Monstey_WindowsFilePath(path)) &&
                            unique.insert(std::filesystem::weakly_canonical(Monstey_WindowsFilePath(path))).second,
                        "Authored outputs must be distinct and new");
    }
    if (a.contains("report"))
    {
        const auto text = result.dump(2) + "\n";
        outputs.back().second = B(text.begin(), text.end());
    }
    const auto check_sources = [&]()
    {
        Monstey_Require(Monstey_HashSha256(source) == Monstey_HashSha256(original),
                        "Native mesh source changed during authoring");
        if (render_source)
            Monstey_Require(Monstey_HashSha256(*render_source) == Monstey_HashSha256(render_original),
                            "Native render source changed during authoring");
        if (scene_source)
            Monstey_Require(Monstey_HashSha256(*scene_source) == Monstey_HashSha256(scene_original),
                            "Native scene template changed during authoring");
        for (const auto &[path, bytes] : merge_snapshots)
            Monstey_Require(Monstey_HashSha256(path) == Monstey_HashSha256(bytes),
                            "Additional native source changed during authoring");
    };
    check_sources();
    std::vector<std::pair<std::filesystem::path, std::string>> created;
    try
    {
        for (const auto &[path, bytes] : outputs)
        {
            Monstey_WriteNewFile(path, bytes);
            created.emplace_back(path, Monstey_HashSha256(bytes));
            Monstey_Require(Monstey_ReadFile(path) == bytes, "Authored resource publication readback differs");
        }
        check_sources();
    }
    catch (...)
    {
        for (const auto &[path, hash] : created)
        {
            try
            {
                if (Monstey_HashSha256(path) == hash)
                {
                    std::error_code error;
                    std::filesystem::remove(Monstey_WindowsFilePath(path), error);
                }
            }
            catch (...)
            {
                // Preserve another writer's replacement and the initial publication error.
            }
        }
        throw;
    }
    return result;
}

} // namespace Monstey_WorldPrivate

} // namespace Monstey
