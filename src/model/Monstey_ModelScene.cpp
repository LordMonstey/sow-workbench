#include "Monstey_ModelPrivate.hpp"

namespace Monstey
{
using namespace Monstey_ModelPrivate;

Monstey_ModelScene Monstey_ModelReadRigidSceneBytes(std::span<const std::uint8_t> bytes)
{
    Monstey_MRequire(bytes.size() <= 512ull * 1024 * 1024, "Native rigid geometry size limit");
    auto native = Monstey_LoadModel(Monstey_Bytes(bytes.begin(), bytes.end()));
    Monstey_ModelScene scene;
    scene.source_sha256 = scene.baseline_sha256 = Monstey_HashSha256(bytes);
    for (auto &part : Monstey_DecodeParts(native, Monstey_DecodePurpose::RigidGeometry))
    {
        Monstey_ModelPart out;
        out.container = part.container;
        out.part = part.part;
        out.indices = std::move(part.indices);
        out.triangle_bands = std::move(part.triangle_bands);
        out.triangle_band_topology_sha256 = std::move(part.triangle_band_topology_sha256);
        for (auto &[name, channel] : part.channels)
            out.channels.emplace(name, Monstey_ModelChannel{channel.width, std::move(channel.v)});
        scene.parts.push_back(std::move(out));
    }
    scene.native_baseline = std::move(native.raw);
    return scene;
}

Monstey_ModelScene Monstey_ModelReadSceneBytes(std::span<const std::uint8_t> bytes, const std::filesystem::path &skel)
{
    auto m = Monstey_LoadModel(Monstey_Bytes(bytes.begin(), bytes.end()));
    Monstey_MRequire(Monstey_Allocation(m)["safe"].as_bool(), "Unsafe native scene allocations");
    const auto rig = Monstey_ReadModelRig(m, skel);
    Monstey_ModelScene scene;
    scene.source_sha256 = scene.baseline_sha256 = Monstey_HashSha256(m.raw);
    scene.skeleton_sha256 = rig.skelHash;
    scene.hk_to_source = rig.hkToSource;
    scene.source_to_hk = rig.sourceToHk;
    for (auto &part : Monstey_DecodeParts(m))
    {
        Monstey_ModelPart out;
        out.container = part.container;
        out.part = part.part;
        out.indices = std::move(part.indices);
        out.triangle_bands = std::move(part.triangle_bands);
        out.triangle_band_topology_sha256 = std::move(part.triangle_band_topology_sha256);
        for (auto &[name, channel] : part.channels)
            out.channels.emplace(name, Monstey_ModelChannel{channel.width, std::move(channel.v)});
        scene.parts.push_back(std::move(out));
    }
    for (std::size_t hk = 0; hk < rig.hkToSource.size(); ++hk)
    {
        const auto source = rig.hkToSource[hk];
        const auto &b = rig.source[source];
        scene.bones.push_back(Monstey_ModelBone{
            hk, source, b.parent < 0 ? -1 : static_cast<int>(rig.sourceToHk[static_cast<std::size_t>(b.parent)]), b.id,
            b.name, b.global});
    }
    scene.native_baseline = std::move(m.raw);
    return scene;
}

Monstey_J Monstey_ModelWriteScene(const Monstey_ModelScene &scene, const std::filesystem::path &nativeTemplate,
                                  const std::filesystem::path &output, const Monstey_J &options)
{
    const std::array<std::filesystem::path, 1> sources = {nativeTemplate};
    Monstey_RequireOfflineOutput(output, nativeTemplate.empty() ? std::span<const std::filesystem::path>{}
                                                                : std::span<const std::filesystem::path>(sources));
    Monstey_MRequire(!std::filesystem::exists(output), "Output already exists");
    auto raw = nativeTemplate.empty() ? scene.native_baseline : Monstey_ReadFile(nativeTemplate);
    const auto hash = Monstey_HashSha256(raw);
    if (options.contains("expected_template_sha256") && options["expected_template_sha256"].as_string() != hash)
        throw Monstey_Error("source_hash_mismatch", "Scene template snapshot differs from expected SHA256");
    Monstey_MRequire(!scene.native_baseline.empty() && Monstey_HashSha256(scene.native_baseline) == hash &&
                         scene.baseline_sha256 == hash,
                     "Scene baseline does not match explicit native template");
    const auto m = Monstey_LoadModel(std::move(raw));
    Monstey_MRequire(Monstey_Allocation(m)["safe"].as_bool(), "Unsafe template allocations");
    const auto rig = Monstey_ReadModelRig(m, {});
    const auto original = Monstey_DecodeParts(m);
    Monstey_MRequire(scene.hk_to_source == rig.hkToSource && scene.source_to_hk == rig.sourceToHk &&
                         scene.bones.size() == rig.source.size(),
                     "Scene rig permutation/count changed");
    for (std::size_t hk = 0; hk < rig.hkToSource.size(); ++hk)
    {
        const auto source = rig.hkToSource[hk];
        const auto &expected = rig.source[source];
        const auto &actual = scene.bones[hk];
        Monstey_MRequire(actual.hk_index == hk && actual.source_index == source && actual.native_id == expected.id &&
                             actual.parent_hk ==
                                 (expected.parent < 0
                                      ? -1
                                      : static_cast<int>(rig.sourceToHk[static_cast<std::size_t>(expected.parent)])),
                         "Scene native bone identity changed");
        for (unsigned k = 0; k < 16; ++k)
            Monstey_MRequire(
                std::isfinite(actual.global_bind_metres[k]) &&
                    std::abs(actual.global_bind_metres[k] - expected.global[k]) <=
                        (scene.position_projection == Monstey_ModelProjection::NativeCentimetres ? 0.0 : 2e-6),
                "Scene native rest bind changed; retarget explicitly");
    }
    std::set<std::pair<std::size_t, std::size_t>> seen;
    std::set<std::size_t> changed;
    std::map<std::size_t, std::vector<Monstey_Part>> replacements;
    for (const auto &supplied : scene.parts)
    {
        Monstey_MRequire(seen.insert({supplied.container, supplied.part}).second, "Scene repeats native part identity");
        const auto baseline = std::find_if(original.begin(), original.end(), [&](const auto &p)
                                           { return p.container == supplied.container && p.part == supplied.part; });
        Monstey_MRequire(baseline != original.end(), "Scene part lacks native donor");
        Monstey_Part part;
        part.container = supplied.container;
        part.part = supplied.part;
        part.indices = supplied.indices;
        part.triangle_bands = supplied.triangle_bands;
        part.triangle_band_topology_sha256 = supplied.triangle_band_topology_sha256;
        for (const auto &[name, channel] : supplied.channels)
        {
            const bool uv = name.size() == 3 && name.starts_with("UV") && name[2] >= '0' && name[2] <= '7';
            const bool color = name.size() == 6 && name.starts_with("COLOR") && name[5] >= '0' && name[5] <= '7';
            const bool vector = name == "POSITION" || name == "NORMAL" || name == "TANGENT" || name == "BITANGENT";
            Monstey_MRequire(uv || color || vector || name == "JOINTS" || name == "WEIGHTS_U8",
                             "Unsupported scene vertex channel");
            const unsigned width = uv ? 2 : vector ? 3 : 4;
            Monstey_MRequire(channel.width == width, "Scene vertex channel width mismatch");
            part.channels[name] = {width, channel.values};
        }
        for (const char *name : {"POSITION", "NORMAL", "TANGENT", "BITANGENT", "UV0", "COLOR0", "JOINTS", "WEIGHTS_U8"})
            Monstey_MRequire(part.channels.contains(name), std::string("Scene missing channel ") + name);
        Monstey_MRequire(part.channels["POSITION"].v.size() % 3 == 0, "Scene position dimensions invalid");
        const auto count = part.Count();
        Monstey_MRequire(count > 0 && count <= 8000000, "Scene vertex count limit");
        for (const auto &[name, channel] : part.channels)
        {
            Monstey_MRequire(channel.v.size() == count * channel.width, "Scene channel count mismatch");
            for (std::size_t i = 0; i < channel.v.size(); ++i)
            {
                const auto value = channel.v[i];
                if (!std::isfinite(value))
                {
                    Monstey_MRequire((name == "NORMAL" || name == "TANGENT" || name == "BITANGENT") &&
                                         baseline->channels.at(name).v.size() == channel.v.size(),
                                     "New nonfinite scene data is not permitted");
                    const auto old = baseline->channels.at(name).v[i];
                    Monstey_MRequire(std::bit_cast<std::uint32_t>(static_cast<float>(old)) ==
                                         std::bit_cast<std::uint32_t>(static_cast<float>(value)),
                                     "Nonfinite scene bits do not match native source");
                }
                else if (name == "JOINTS")
                    Monstey_MRequire(value >= 0 && value < m.h[16] && std::floor(value) == value,
                                     "Scene joint outside native HK rig");
                else if (name == "WEIGHTS_U8" || name.starts_with("COLOR"))
                    Monstey_MRequire(value >= 0 && value <= 255 && std::floor(value) == value,
                                     "Scene native byte channel outside UINT8");
            }
        }
        bool gap = false;
        for (unsigned i = 0; i < 8; ++i)
        {
            if (part.channels.contains("UV" + std::to_string(i)))
                Monstey_MRequire(!gap, "Scene UV channels are not contiguous");
            else
                gap = true;
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            double total = 0;
            for (unsigned k = 0; k < 4; ++k)
                total += part.channels["WEIGHTS_U8"].v[i * 4 + k];
            Monstey_MRequire(total == 255, "Scene logical byte weights must sum to 255");
        }
        Monstey_MRequire(part.indices.size() % 3 == 0, "Scene indices do not form triangles");
        for (const auto index : part.indices)
            Monstey_MRequire(index < count, "Scene triangle index outside vertices");
        Monstey_ResolvePartBands(part, *baseline, options);
        const bool equivalent = Monstey_NativeEquivalent(
            part, *baseline, scene.position_projection == Monstey_ModelProjection::NativeCentimetres);
        if (!equivalent)
            changed.insert(part.container);
        replacements[part.container].push_back(std::move(part));
    }
    Monstey_MRequire(seen.size() == original.size(), "Scene lost native part identities");
    for (auto it = replacements.begin(); it != replacements.end();)
        if (!changed.contains(it->first))
            it = replacements.erase(it);
        else
            ++it;
    const auto encoded =
        changed.empty() ? m.raw
                        : Monstey_RebuildModel(m, replacements, true, options,
                                               scene.position_projection == Monstey_ModelProjection::NativeCentimetres);
    const auto check = Monstey_LoadModel(encoded);
    Monstey_MRequire(Monstey_Allocation(check)["safe"].as_bool(), "Scene writer allocation failure");
    const bool preflight = options.value("preflight", false).as_bool();
    if (!preflight)
        Monstey_WriteNewFile(output, encoded);
    return Monstey_J::object(
        {{"output", Monstey_Utf8(output.wstring())},
         {"source_sha256", scene.source_sha256},
         {"template_sha256", hash},
         {"output_sha256", Monstey_HashSha256(encoded)},
         {"triangle_band_policy", options.contains("triangle_band") ? "explicit_uniform" : "qualified_face_membership"},
         {"triangle_band", options.value("triangle_band", nullptr)},
         {"native_graph",
          Monstey_J::object({{"source_containers", m.h[3]},
                             {"output_containers", Monstey_U32(encoded, 20ull + Monstey_U32(encoded, 8) + 12)},
                             {"source_connections", m.h[2]},
                             {"output_connections", Monstey_U32(encoded, 20ull + Monstey_U32(encoded, 8) + 8)},
                             {"source_group_references", m.h[6]},
                             {"output_group_references", Monstey_U32(encoded, 20ull + Monstey_U32(encoded, 8) + 24)},
                             {"piece_identity_preserved", true},
                             {"threshold_words_preserved", true}})},
         {"byte_exact_noop", encoded == m.raw},
         {"preflight", preflight},
         {"published", !preflight},
         {"allocation", Monstey_Allocation(check)},
         {"runtime_verified", false}});
}

} // namespace Monstey
