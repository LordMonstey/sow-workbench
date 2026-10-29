#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

Monstey_WRender::Monstey_WRender(B input) : raw(std::move(input))
{
    Monstey_Require(raw.size() >= 96 && std::memcmp(raw.data(), "WDSC", 4) == 0 && Monstey_U32(raw, 4) == 9,
                    "Expected full native WDSC9");
    for (std::size_t j = 0; j < 9; ++j)
        header[j] = Monstey_U32(raw, 8 + 4 * j);
    std::size_t at = 44;
    auto take = [&](std::size_t n)
    {
        Monstey_CheckRange(raw, at, n);
        B bytes(raw.begin() + static_cast<std::ptrdiff_t>(at), raw.begin() + static_cast<std::ptrdiff_t>(at + n));
        at += n;
        return bytes;
    };
    auto u32 = [&]()
    {
        auto value = Monstey_U32(raw, at);
        at += 4;
        return value;
    };
    strings = take(header[8]);
    std::uint64_t material_count{};
    Monstey_Require(header[4] <= (raw.size() - at) / 12, "WDSC model count exceeds extent");
    for (std::uint32_t j = 0; j < header[4]; ++j)
    {
        auto count = u32(), path = u32(), group = u32();
        Monstey_Require(group <= 65535 && count <= (raw.size() - at) / 4, "WDSC material model count or group invalid");
        Model model{path, group, {}};
        (void)Name(path);
        for (std::uint32_t k = 0; k < count; ++k)
        {
            auto offset = u32();
            (void)Name(offset);
            model.material_offsets.push_back(offset);
        }
        models.push_back(std::move(model));
        material_count += count;
    }
    Monstey_Require(material_count == header[5], "WDSC material allocation differs");
    bindings = take(76ull * header[2]);
    Monstey_Require(header[0] <= (raw.size() - at) / 12, "WDSC group count exceeds extent");
    std::uint64_t statics{}, lod_count{}, lod_entries{};
    for (std::uint32_t j = 0; j < header[0]; ++j)
    {
        auto ns = u32(), nl = u32(), no = u32();
        Group group;
        Monstey_Require(ns <= (raw.size() - at) / 32, "WDSC static records outside extent");
        for (std::uint32_t k = 0; k < ns; ++k)
        {
            auto record = take(32);
            // Native 6B3CD0/613A34: +28 is the draw ordinal in the shared resource context.
            // No model selector is read from +25..27 or +30..31; those bytes stay opaque.
            auto lo = Monstey_WFloats(record, 0, 3), hi = Monstey_WFloats(record, 12, 3);
            for (std::size_t axis = 0; axis < 3; ++axis)
                Monstey_Require(lo[axis].as_number() <= hi[axis].as_number(), "WDSC static culling bounds inverted");
            group.records.push_back(std::move(record));
        }
        Monstey_Require(nl <= raw.size() - at, "WDSC LOD headers outside extent");
        for (std::uint32_t k = 0; k < nl; ++k)
        {
            auto count = take(1)[0];
            auto bytes = take(12ull * count);
            bytes.insert(bytes.begin(), count);
            lod_entries += count;
            group.lods.push_back(std::move(bytes));
        }
        Monstey_Require(no <= (raw.size() - at) / 8, "WDSC owner groups outside extent");
        for (std::uint32_t k = 0; k < no; ++k)
        {
            auto key = u32(), count = u32();
            Monstey_Require(count <= (raw.size() - at) / 4, "WDSC owner bindings outside extent");
            std::vector<std::uint32_t> values;
            for (std::uint32_t index = 0; index < count; ++index)
            {
                auto value = u32();
                Monstey_Require(value < header[2], "WDSC owner binding index outside table");
                values.push_back(value);
            }
            group.owners.emplace_back(key, std::move(values));
        }
        statics += ns;
        lod_count += nl;
        groups.push_back(std::move(group));
    }
    Monstey_Require(statics == header[1] && lod_count == header[6] && lod_entries == header[7],
                    "WDSC static or LOD allocation differs");
    scene = take(raw.size() - at);
    Monstey_Require(scene.size() >= 52, "WDSC scene header missing");
    for (auto index : {2u, 5u, 10u, 12u})
        Monstey_Require(Monstey_U32(scene, 4 * index) == 0, "WDSC reserved scene count unsupported");
    Monstey_Require(Monstey_U32(scene, 4) <= groups.size(), "WDSC consumed group count outside prefix");
    Monstey_Require(Encode() == raw, "Full WDSC identity reconstruction differs");
}

std::string Monstey_WRender::Name(std::uint32_t offset) const
{
    auto value = Monstey_WName(strings, offset);
    return Monstey_ArchiveVirtual(value);
}

std::uint32_t Monstey_WRender::Intern(const std::string &input)
{
    auto text = Monstey_ArchiveVirtual(input);
    std::replace(text.begin(), text.end(), '/', '\\');
    B bytes(text.begin(), text.end());
    bytes.push_back(0);
    auto existing = std::search(strings.begin(), strings.end(), bytes.begin(), bytes.end());
    if (existing != strings.end())
        return static_cast<std::uint32_t>(existing - strings.begin());
    Monstey_Require(strings.size() + bytes.size() <= 0xffffffffull, "WDSC string dictionary exceeds uint32");
    auto offset = static_cast<std::uint32_t>(strings.size());
    strings.insert(strings.end(), bytes.begin(), bytes.end());
    return offset;
}

B Monstey_WRender::Encode() const
{
    auto h = header;
    h[0] = static_cast<std::uint32_t>(groups.size());
    h[1] = 0;
    h[2] = static_cast<std::uint32_t>(bindings.size() / 76);
    h[4] = static_cast<std::uint32_t>(models.size());
    h[5] = 0;
    h[6] = 0;
    h[7] = 0;
    h[8] = static_cast<std::uint32_t>(strings.size());
    for (auto &model : models)
        h[5] += static_cast<std::uint32_t>(model.material_offsets.size());
    for (auto &group : groups)
    {
        h[1] += static_cast<std::uint32_t>(group.records.size());
        h[6] += static_cast<std::uint32_t>(group.lods.size());
        for (auto &lod : group.lods)
            h[7] += lod[0];
    }
    B out{'W', 'D', 'S', 'C'};
    Monstey_AppendU32(out, 9);
    for (auto word : h)
        Monstey_AppendU32(out, word);
    out.insert(out.end(), strings.begin(), strings.end());
    for (auto &model : models)
    {
        Monstey_AppendU32(out, static_cast<std::uint32_t>(model.material_offsets.size()));
        Monstey_AppendU32(out, model.path_offset);
        Monstey_AppendU32(out, model.geometry_group);
        for (auto offset : model.material_offsets)
            Monstey_AppendU32(out, offset);
    }
    out.insert(out.end(), bindings.begin(), bindings.end());
    for (auto &group : groups)
    {
        Monstey_AppendU32(out, static_cast<std::uint32_t>(group.records.size()));
        Monstey_AppendU32(out, static_cast<std::uint32_t>(group.lods.size()));
        Monstey_AppendU32(out, static_cast<std::uint32_t>(group.owners.size()));
        for (auto &record : group.records)
            out.insert(out.end(), record.begin(), record.end());
        for (auto &lod : group.lods)
            out.insert(out.end(), lod.begin(), lod.end());
        for (auto &[key, values] : group.owners)
        {
            Monstey_AppendU32(out, key);
            Monstey_AppendU32(out, static_cast<std::uint32_t>(values.size()));
            for (auto value : values)
                Monstey_AppendU32(out, value);
        }
    }
    out.insert(out.end(), scene.begin(), scene.end());
    return out;
}

J Monstey_WRender::Report() const
{
    J::Array model_rows, group_rows, scene_header;
    for (std::size_t j = 0; j < models.size(); ++j)
    {
        auto &model = models[j];
        J::Array materials;
        for (auto offset : model.material_offsets)
            materials.push_back(Name(offset));
        model_rows.push_back(J::object({{"index", static_cast<std::uint64_t>(j)},
                                        {"path", Name(model.path_offset)},
                                        {"geometry_group", model.geometry_group},
                                        {"materials", materials}}));
    }
    for (std::size_t j = 0; j < groups.size(); ++j)
    {
        J::Array statics, bounds, flags;
        std::size_t eligible = 0;
        bool suffix = false, prefix_valid = true;
        for (auto &record : groups[j].records)
        {
            statics.push_back(Monstey_U16(record, 28));
            bounds.push_back(Monstey_WFloats(record, 0, 6));
            flags.push_back(record[24]);
            if (record[24] != 0)
            {
                ++eligible;
                if (suffix)
                    prefix_valid = false;
            }
            else
                suffix = true;
        }
        group_rows.push_back(J::object({{"index", static_cast<std::uint64_t>(j)},
                                        {"static_draws", statics},
                                        {"static_model_selector_in_record", false},
                                        {"static_raw_flags", flags},
                                        {"eligible_static_prefix_count", static_cast<std::uint64_t>(eligible)},
                                        {"eligible_static_prefix_valid", prefix_valid},
                                        {"bounds", bounds},
                                        {"lods", static_cast<std::uint64_t>(groups[j].lods.size())},
                                        {"prefab_owner_groups", static_cast<std::uint64_t>(groups[j].owners.size())}}));
    }
    for (std::size_t j = 0; j < 13; ++j)
        scene_header.push_back(Monstey_U32(scene, 4 * j));
    return J::object({{"format", "WDSC9"},
                      {"models", model_rows},
                      {"groups", group_rows},
                      {"scene_header", scene_header},
                      {"scene_suffix_sha256", Monstey_HashSha256(scene)},
                      {"prefab_binding_sha256", Monstey_HashSha256(bindings)},
                      {"roundtrip_byte_exact", Encode() == raw},
                      {"runtime_verified", false}});
}

std::map<std::uint16_t, B> Monstey_WCullPrototypes(const Monstey_WRender &source, bool preserve_group_identity)
{
    std::map<std::uint16_t, B> result;
    for (const auto &group : source.groups)
        for (const auto &record : group.records)
        {
            const auto [it, inserted] = result.try_emplace(Monstey_U16(record, 28), record);
            // Bounds are rebuilt from indexed geometry. Flags and opaque suffix bytes
            // can carry group-specific meaning, with no qualified flattening precedence.
            Monstey_Require(preserve_group_identity || inserted ||
                                std::equal(record.begin() + 24, record.end(), it->second.begin() + 24),
                            "Private cull flattening has conflicting native prototypes for one draw");
        }
    return result;
}

std::pair<B, J> Monstey_WRenderBuild(const Monstey_WMesh &mesh, const J &topology, const B &original,
                                     const std::string &source_path, const std::string &target_path, bool preserve,
                                     const B *scene_template)
{
    Monstey_Require(topology.at("output_sha256").as_string() == Monstey_HashSha256(mesh.raw) &&
                        topology.at("groups").size() == mesh.groups.size(),
                    "Topology proof differs from mesh");
    Monstey_WRender source(original);
    auto src = Monstey_ArchiveVirtual(source_path), dst = Monstey_ArchiveVirtual(target_path);
    // 6B3BA7/6B3BD0 supplies descriptor[0]; accessor114BBBC selects its U16 geometry context.
    Monstey_Require(!source.models.empty() && source.Name(source.models[0].path_offset) == src,
                    "Static source geometry is not WDSC descriptor[0]");
    std::size_t target_context = SIZE_MAX;
    if (topology.contains("target_context"))
        target_context = static_cast<std::size_t>(topology.at("target_context").as_uint());
    else
        for (std::size_t index = 0; index < topology.at("groups").size(); ++index)
            if (topology.at("groups")[index].at("source_group").as_uint() == source.models[0].geometry_group)
            {
                Monstey_Require(target_context == SIZE_MAX,
                                "Repeated static contexts require explicit native common-context composition");
                target_context = index;
            }
    Monstey_Require(target_context < mesh.groups.size() && target_context <= 65535 &&
                        topology.at("groups")[target_context].at("source_group").as_uint() ==
                            source.models[0].geometry_group,
                    "Native selected geometry context has no qualified topology mapping");
    auto &mapping = topology.at("groups")[target_context].at("draws");
    Monstey_Require(mapping.size() == mesh.groups[target_context].draws.size() && mapping.size() <= 65535,
                    "Static native draw mapping differs");
    const auto prototypes = Monstey_WCullPrototypes(source, preserve);
    auto make_record = [&](const B &prototype, std::size_t ordinal)
    {
        auto attributes = mesh.Attributes(target_context, ordinal);
        auto position = attributes.find("POSITION");
        auto indices = mesh.Indices(target_context, ordinal);
        Monstey_Require(position != attributes.end() && position->second.width >= 3 && !indices.empty(),
                        "Static draw needs indexed 3D positions");
        auto record = prototype;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            double lo = std::numeric_limits<double>::infinity(), hi = -lo;
            for (auto index : indices)
            {
                auto value = mesh.Component(position->second, index, axis);
                Monstey_Require(std::isfinite(value), "Nonfinite indexed position");
                lo = std::min(lo, value);
                hi = std::max(hi, value);
            }
            Monstey_WPutFloat(record, 4 * axis, static_cast<float>(lo));
            Monstey_WPutFloat(record, 12 + 4 * axis, static_cast<float>(hi));
        }
        Monstey_PutU16(record, 28, static_cast<std::uint16_t>(ordinal));
        return record;
    };
    std::vector<std::string> materials;
    for (auto offset : source.models[0].material_offsets)
        materials.push_back(source.Name(offset));
    std::map<std::uint32_t, std::string> assigned;
    J::Array dependencies;
    std::vector<B> culls;
    for (std::size_t ordinal = 0; ordinal < mapping.size(); ++ordinal)
    {
        auto &draw = mapping[ordinal];
        auto old_draw = Monstey_WUint(draw.at("source_draw"));
        Monstey_Require(draw.at("draw_index").as_uint() == ordinal && old_draw <= 65535 &&
                            prototypes.contains(static_cast<std::uint16_t>(old_draw)),
                        "Source draw lacks a qualified native culling prototype");
        culls.push_back(make_record(prototypes.at(static_cast<std::uint16_t>(old_draw)), ordinal));
        auto old_slot = Monstey_WUint(draw.at("source_material_slot")), slot = Monstey_WUint(draw.at("material_slot"));
        Monstey_Require(old_slot < source.models[0].material_offsets.size() && slot <= 65535 && !materials.empty(),
                        "Material slot outside native descriptor[0]");
        auto material = draw.at("material").is_null() ? source.Name(source.models[0].material_offsets[old_slot])
                                                      : Monstey_ArchiveVirtual(draw.at("material").as_string());
        Monstey_Require(material.ends_with(".mat"), "Material dependency is not .mat");
        auto [it, inserted] = assigned.emplace(slot, material);
        Monstey_Require(inserted || it->second == material, "Conflicting shared material slot");
        while (materials.size() <= slot)
            materials.push_back(materials[0]);
        materials[slot] = material;
        dependencies.push_back(material);
    }
    auto qualify_prefix = [](std::vector<B> &records, std::size_t auxiliary_count)
    {
        // 6B3D30 stores total statics as U16+4; 6B3E6F counts nonzero raw+24 in U16+2.
        // 2BD95C consumes exactly the first eligible-count runtime48-byte records.
        Monstey_Require(records.size() <= 65535 && auxiliary_count <= 65535,
                        "Authored WDSC group exceeds native uint16 runtime counters");
        for (auto &record : records)
            Monstey_Require(record.size() == 32, "Native static cull is not32 bytes");
        std::stable_partition(records.begin(), records.end(), [](const B &record) { return record[24] != 0; });
    };
    qualify_prefix(culls, 0);
    B empty{'W', 'D', 'S', 'C'};
    Monstey_AppendU32(empty, 9);
    for (auto word : {0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u})
        Monstey_AppendU32(empty, word);
    empty.resize(96);
    Monstey_WRender authored(std::move(empty));
    Monstey_WRender::Model model{authored.Intern(dst), static_cast<std::uint32_t>(target_context), {}};
    for (auto &material : materials)
        model.material_offsets.push_back(authored.Intern(material));
    authored.models.push_back(std::move(model));
    authored.groups.push_back({culls, {}, {}});
    J private_scene = J::object();
    J::Array rebuilt;
    B output;
    if (preserve)
    {
        Monstey_WRender merged(original);
        bool identity = topology.at("groups")[target_context].contains("source_draw_count") &&
                        topology.at("groups")[target_context].at("source_draw_count").as_uint() == mapping.size();
        for (std::size_t ordinal = 0; ordinal < mapping.size(); ++ordinal)
            identity = identity && mapping[ordinal].at("source_draw").as_uint() == ordinal &&
                       mapping[ordinal].at("partition").as_uint() == 0;
        for (std::size_t gi = 0; gi < merged.groups.size(); ++gi)
        {
            auto &group = merged.groups[gi];
            Monstey_Require(group.lods.empty() || identity,
                            "Changing draw cardinality/order with WDSC LOD entries needs qualified LOD remapping");
            std::vector<B> replacement;
            for (auto &record : group.records)
                for (std::size_t ordinal = 0; ordinal < mapping.size(); ++ordinal)
                    if (mapping[ordinal].at("source_draw").as_uint() == Monstey_U16(record, 28))
                        replacement.push_back(make_record(record, ordinal));
            rebuilt.push_back(J::object({{"group_index", static_cast<std::uint64_t>(gi)},
                                         {"old_records", static_cast<std::uint64_t>(group.records.size())},
                                         {"new_records", static_cast<std::uint64_t>(replacement.size())}}));
            qualify_prefix(replacement, group.lods.size());
            group.records = std::move(replacement);
        }
        Monstey_WRender::Model replacement{merged.Intern(dst), static_cast<std::uint32_t>(target_context), {}};
        for (auto &material : materials)
            replacement.material_offsets.push_back(merged.Intern(material));
        merged.models[0] = std::move(replacement);
        output = merged.Encode();
        Monstey_WRender check(output);
        Monstey_Require(check.scene == source.scene && check.bindings == source.bindings &&
                            check.groups.size() == source.groups.size() &&
                            std::equal(source.strings.begin(), source.strings.end(), check.strings.begin()),
                        "Scene, prefab bindings, group ordinals or string prefix changed");
        for (std::size_t gi = 0; gi < source.groups.size(); ++gi)
            Monstey_Require(check.groups[gi].lods == source.groups[gi].lods &&
                                check.groups[gi].owners == source.groups[gi].owners,
                            "LOD bytes or owner associations changed");
    }
    else
    {
        if (!culls.empty())
        {
            const auto &prototype_bytes = scene_template ? *scene_template : original;
            Monstey_WRender prototype(prototype_bytes);
            const auto native_scene =
                Monstey_WorldScene::Read(prototype.scene, static_cast<std::uint32_t>(prototype.groups.size()));
            std::array<float, 6> bounds{
                std::numeric_limits<float>::infinity(),  std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::infinity(),  -std::numeric_limits<float>::infinity(),
                -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
            for (const auto &record : culls)
                for (std::size_t axis = 0; axis < 3; ++axis)
                {
                    bounds[axis] = std::min(bounds[axis], std::bit_cast<float>(Monstey_U32(record, 4 * axis)));
                    bounds[axis + 3] =
                        std::max(bounds[axis + 3], std::bit_cast<float>(Monstey_U32(record, 12 + 4 * axis)));
                }
            const auto routed = Monstey_WorldSceneRouteSingleGroup(native_scene, bounds);
            authored.scene = routed.Encode(1);
            private_scene = routed.Inspect(1);
            private_scene["prototype_sha256"] = Monstey_HashSha256(prototype_bytes);
            private_scene["prototype_scene_sha256"] = Monstey_HashSha256(prototype.scene);
            private_scene["explicit_scene_template"] = scene_template != nullptr;
            private_scene["group_routing_built"] = true;
            private_scene["native_consumer_contract_qualified"] = true;
        }
        else
        {
            private_scene["group_routing_built"] = false;
            private_scene["reason"] = "No authored static draws require a scene group route";
        }
        output = authored.Encode();
    }
    Monstey_WRender checked(output);
    for (const auto &group : checked.groups)
    {
        bool suffix = false;
        for (const auto &record : group.records)
        {
            if (record[24] == 0)
                suffix = true;
            else
                Monstey_Require(!suffix, "Eligible static cull lies outside native prefix");
        }
    }
    std::vector<bool> submitted(mapping.size());
    for (auto &group : checked.groups)
        for (auto &record : group.records)
        {
            auto ordinal = Monstey_U16(record, 28);
            Monstey_Require(ordinal < mapping.size(), "Cull draw ordinal outside native table");
            Monstey_Require(record == make_record(record, ordinal), "Indexed draw culling bounds differ");
            submitted[ordinal] = true;
        }
    Monstey_Require(std::all_of(submitted.begin(), submitted.end(), [](bool x) { return x; }),
                    "An authored native draw has no culling submission record");
    for (auto &draw : mesh.groups[target_context].draws)
        Monstey_Require((draw[6] & 65535) < checked.models[0].material_offsets.size(),
                        "Draw material slot outside descriptor[0]");
    auto report = checked.Report();
    report["status"] =
        preserve ? "WDSC9_EXISTING_INDEXED_DRAW_CULLS_REBUILT" : "WDSC9_COMMON_MESH_INDEXED_DRAW_CULLS_REBUILT";
    report["source_sha256"] = Monstey_HashSha256(original);
    report["output_sha256"] = Monstey_HashSha256(output);
    report["mesh_sha256"] = Monstey_HashSha256(mesh.raw);
    report["mesh_resource"] = dst;
    report["material_dependencies"] = dependencies;
    report["static_descriptor_index"] = 0;
    report["static_geometry_context"] = static_cast<std::uint64_t>(target_context);
    report["all_native_draws_have_cull_record"] = true;
    report["indexed_geometry_bounds"] = true;
    report["eligible_static_prefix_qualified"] = true;
    report["opaque_static_bytes_preserved"] = true;
    report["native_scene"] = private_scene;
    report["native_scene_preserved_exact"] = preserve;
    report["scene_portals_inherited"] = false;
    report["scene_portals_preserved_exact"] = preserve;
    report["unrelated_prefab_bindings_preserved"] = preserve;
    report["moved_static_groups"] = rebuilt;
    report["collision_or_navigation_updated"] = false;
    return {output, report};
}

} // namespace Monstey_WorldPrivate
using namespace Monstey_WorldPrivate;

std::pair<Monstey_Bytes, Monstey_Json> Monstey_WorldStaticRenderBuild(
    std::span<const std::uint8_t> candidate_mesh, std::span<const std::uint8_t> source_render, const J &topology,
    std::string_view source_resource, std::string_view target_resource, bool preserve_section,
    std::span<const std::uint8_t> scene_template)
{
    const B scene(scene_template.begin(), scene_template.end());
    return Monstey_WRenderBuild(Monstey_WMesh(B(candidate_mesh.begin(), candidate_mesh.end())), topology,
                                B(source_render.begin(), source_render.end()), std::string(source_resource),
                                std::string(target_resource), preserve_section, scene.empty() ? nullptr : &scene);
}

J Monstey_WorldStaticPairValidate(std::span<const std::uint8_t> source_mesh,
                                  std::span<const std::uint8_t> source_render,
                                  std::span<const std::uint8_t> candidate_mesh,
                                  std::span<const std::uint8_t> candidate_render, const J &topology,
                                  std::string_view source_resource, std::string_view target_resource,
                                  bool preserve_section, std::span<const std::uint8_t> scene_template)
{
    const Monstey_WMesh before(B(source_mesh.begin(), source_mesh.end()));
    const Monstey_WMesh after(B(candidate_mesh.begin(), candidate_mesh.end()));
    const Monstey_WRender render(B(source_render.begin(), source_render.end()));
    Monstey_Require(topology.at("source_sha256").as_string() == Monstey_HashSha256(source_mesh),
                    "Static pair topology source hash differs");
    Monstey_Require(topology.at("output_sha256").as_string() == Monstey_HashSha256(candidate_mesh),
                    "Static pair topology output hash differs");
    Monstey_Require(!render.models.empty() && render.models[0].geometry_group < before.groups.size(),
                    "Static source descriptor[0] geometry context is absent");
    const auto selected = render.models[0].geometry_group;
    Monstey_Require(before.groups.size() == after.groups.size(),
                    "Direct static pair cannot remove or append unqualified geometry contexts");
    const auto &groups = topology.at("groups");
    Monstey_Require(groups.is_array() && groups.size() == after.groups.size(), "Static topology group extent differs");
    // This direct-pair contract does not accept synthesized multi-source culls.
    // Such inputs must first pass the independently qualified merge producer.
    std::size_t target = SIZE_MAX;
    for (std::size_t gi = 0; gi < groups.size(); ++gi)
    {
        const auto sg = groups[gi].at("source_group").as_uint();
        Monstey_Require(sg < before.groups.size(), "Static source group is outside original mesh");
        if (sg == selected)
        {
            Monstey_Require(target == SIZE_MAX, "Repeated static context requires a qualified merge");
            target = gi;
        }
        Monstey_Require(after.groups[gi].layouts == before.groups[sg].layouts,
                        "Native static vertex layout/semantic channels changed");
        const auto &mapping = groups[gi].at("draws");
        Monstey_Require(mapping.size() == after.groups[gi].draws.size(),
                        "Static topology draw count differs from encoded mesh");
        Monstey_Require(groups[gi].at("source_draw_count").as_uint() == before.groups[sg].draws.size(),
                        "Static source draw count differs");
        for (std::size_t di = 0; di < mapping.size(); ++di)
        {
            const auto &row = mapping[di];
            const auto sd = row.at("source_draw").as_uint();
            Monstey_Require(row.at("draw_index").as_uint() == di && row.at("source_group").as_uint() == sg &&
                                sd < before.groups[sg].draws.size(),
                            "Static topology draw identity differs");
            Monstey_Require(row.at("source_material_slot").as_uint() == (before.groups[sg].draws[sd][6] & 65535u),
                            "Static source material slot proof differs");
            Monstey_Require(row.at("material_slot").as_uint() == (after.groups[gi].draws[di][6] & 65535u),
                            "Static candidate material slot proof differs");
            const auto &old_words = before.groups[sg].draws[sd];
            const auto &new_words = after.groups[gi].draws[di];
            constexpr std::array<std::uint32_t, 13> opaque_masks{
                0xffffffffu, 0xffff0000u, 0xffff0000u, 0u, 0u,          0xff000000u, 0xffff0000u,
                0xffffffffu, 0xffffffffu, 0xffffffffu, 0u, 0xffffffffu, 0xffffffffu};
            for (std::size_t word = 0; word < opaque_masks.size(); ++word)
                Monstey_Require((old_words[word] & opaque_masks[word]) == (new_words[word] & opaque_masks[word]),
                                "Native static opaque draw metadata changed");
        }
        if (sg != selected)
        {
            const auto &old = before.groups[sg];
            const auto &now = after.groups[gi];
            Monstey_Require(gi == sg && old.end - old.start == now.end - now.start &&
                                std::equal(before.raw.begin() + static_cast<std::ptrdiff_t>(old.start),
                                           before.raw.begin() + static_cast<std::ptrdiff_t>(old.end),
                                           after.raw.begin() + static_cast<std::ptrdiff_t>(now.start)),
                            "Unselected geometry context changed without its own render association");
        }
    }
    Monstey_Require(target != SIZE_MAX, "Native static context has no authored mesh");
    if (topology.contains("target_context"))
        Monstey_Require(topology.at("target_context").as_uint() == target,
                        "Static target_context differs from descriptor[0] source mapping");
    Monstey_Require(
        before.header[7] == after.header[7] &&
            std::equal(before.raw.begin() + 32, before.raw.begin() + 32 + before.header[7], after.raw.begin() + 32),
        "Native mesh opaque dictionary changed");
    for (const auto &group : render.groups)
    {
        bool suffix = false;
        for (const auto &record : group.records)
        {
            if (record[24] == 0)
                suffix = true;
            else
                Monstey_Require(!suffix, "Source eligible statics are outside native prefix");
        }
    }
    const auto built = Monstey_WorldStaticRenderBuild(candidate_mesh, source_render, topology, source_resource,
                                                      target_resource, preserve_section, scene_template);
    Monstey_Require(built.first.size() == candidate_render.size() &&
                        std::equal(built.first.begin(), built.first.end(), candidate_render.begin()),
                    "Static candidate WDSC differs from exact native indexed-bounds/material replay");
    const Monstey_WRender checked(built.first);
    const auto scene = Monstey_WorldScene::Read(checked.scene, static_cast<std::uint32_t>(checked.groups.size()));
    Monstey_Require(!scene.nodes.empty() && !scene.bsp_nodes.empty(),
                    "Static geometry requires a nonempty native primary/BSP scene");
    Monstey_Require(scene.Encode(static_cast<std::uint32_t>(checked.groups.size())) == checked.scene,
                    "Static scene does not roundtrip byte exactly");
    auto report = built.second;
    report["status"] = "STATIC_PAIR_NATIVE_REPLAY_VALIDATED_OFFLINE";
    report["source_mesh_sha256"] = Monstey_HashSha256(source_mesh);
    report["source_render_sha256"] = Monstey_HashSha256(source_render);
    report["source_scene_offset"] = static_cast<std::uint64_t>(render.raw.size() - render.scene.size());
    report["candidate_scene_offset"] = static_cast<std::uint64_t>(checked.raw.size() - checked.scene.size());
    report["source_scene_sha256"] = Monstey_HashSha256(render.scene);
    report["opaque_draw_metadata_preserved"] = true;
    report["native_vertex_layouts_preserved"] = true;
    report["material_slots_verified_against_encoded_mesh"] = true;
    report["unselected_geometry_contexts_byte_exact"] = true;
    report["scene_format_validated"] = true;
    report["runtime_verified"] = false;
    report["visibility_after_gather"] = "NOT_EVALUATED";
    report["collision_navigation_alignment"] = "NOT_EVALUATED";
    return report;
}

} // namespace Monstey
