#include "Monstey_StaticPrefab.hpp"
#include "Monstey_Archive.hpp"
#include "Monstey_AssetInspect.hpp"
#include "Monstey_World.hpp"
#include "Monstey_InstGeometry.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <windows.h>
namespace Monstey
{
namespace
{
using J = Monstey_Json;
using B = Monstey_Bytes;
void Monstey_SKeys(const J &a, std::initializer_list<std::string_view> keys)
{
    Monstey_Require(a.is_object(), "Static prefab arguments must be object");
    for (const auto &[key, value] : a.as_object())
    {
        (void)value;
        Monstey_Require(std::find(keys.begin(), keys.end(), key) != keys.end(),
                        "Unknown static prefab argument " + key);
    }
}
std::string Monstey_SVirtual(std::string_view input)
{
    auto path = Monstey_ArchiveVirtual(std::string(input));
    Monstey_Require(std::all_of(path.begin(), path.end(), [](char c) { return static_cast<unsigned char>(c) < 128; }),
                    "Native static resource paths must be ASCII");
    return path;
}
std::filesystem::path Monstey_SPath(const J &arg)
{
    Monstey_Require(arg.is_string() && !arg.as_string().empty() && arg.as_string().find('\0') == std::string::npos,
                    "Expected explicit static resource file path");
    return std::filesystem::absolute(std::filesystem::path(Monstey_Wide(arg.as_string()))).lexically_normal();
}
std::filesystem::path Monstey_SLong(const std::filesystem::path &p)
{
    auto text = std::filesystem::absolute(p).wstring();
    if (text.starts_with(L"\\\\?\\"))
        return p;
    if (text.starts_with(L"\\\\"))
        return std::filesystem::path(L"\\\\?\\UNC\\" + text.substr(2));
    return std::filesystem::path(L"\\\\?\\" + text);
}
void Monstey_SAppendFloat(B &b, double value)
{
    const auto f = static_cast<float>(value);
    Monstey_Require(std::isfinite(f), "Static channel exceeds finite float32");
    Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(f));
}
struct Monstey_SSource
{
    std::filesystem::path path;
    std::uint64_t bytes = 0;
    std::string sha;
};
Monstey_SSource Monstey_SRecord(const std::filesystem::path &path)
{
    const auto bytes = std::filesystem::file_size(Monstey_SLong(path));
    const auto sha = Monstey_HashSha256(path);
    Monstey_Require(std::filesystem::file_size(Monstey_SLong(path)) == bytes,
                    "Static resource size changed while hashing");
    return {path, bytes, sha};
}
J Monstey_SJson(const Monstey_SSource &s)
{
    return J::object({{"path", Monstey_Utf8(s.path.wstring())}, {"bytes", s.bytes}, {"sha256", s.sha}});
}
void Monstey_SCheck(const std::vector<Monstey_SSource> &sources)
{
    for (const auto &s : sources)
        Monstey_Require(std::filesystem::file_size(Monstey_SLong(s.path)) == s.bytes &&
                            Monstey_HashSha256(s.path) == s.sha,
                        "Static prefab input changed: " + Monstey_Utf8(s.path.wstring()));
}
J Monstey_SResource(std::string_view resource, const Monstey_SSource &source, std::string_view role)
{
    auto row = Monstey_SJson(source);
    row["resource_path"] = std::string(resource);
    row["role"] = std::string(role);
    row["embed_in_embb"] = true;
    return row;
}
} // namespace
J Monstey_StaticMaterialInspect(std::span<const std::uint8_t> material)
{
    return Monstey_MaterialInspect(material);
}
J Monstey_StaticMaterialShader(std::span<const std::uint8_t> material, std::span<const std::uint8_t> shader)
{
    const auto info = Monstey_StaticMaterialInspect(material);
    Monstey_Require(shader.size() >= 32 && Monstey_U32(shader, 0) == 0x52485253, "Expected native SRHR shader");
    const auto shaderId = Monstey_U32(shader, 28);
    Monstey_Require(info.at("shader_id").as_uint() == shaderId, "Material/shader ID mismatch");
    const std::string_view label = "fulldeferred_rigid";
    Monstey_Require(std::search(shader.begin(), shader.end(), label.begin(), label.end()) != shader.end(),
                    "Shader has no explicit native rigid label");
    return J::object({{"shader_id", shaderId},
                      {"shader_path", Monstey_SVirtual(info.at("shader").as_string())},
                      {"material_sha256", Monstey_HashSha256(material)},
                      {"shader_sha256", Monstey_HashSha256(shader)},
                      {"native_shader_id_matches", true},
                      {"rigid_label_present", true},
                      {"actual_permutation_selection_runtime_verified", false}});
}
namespace
{
B Monstey_SChannelBytes(const Monstey_ModelChannel &channel, bool byte)
{
    B b;
    for (const auto value : channel.values)
    {
        if (byte)
        {
            Monstey_Require(std::isfinite(value) && value >= 0 && value <= 255 && std::floor(value) == value,
                            "Native static color must remain raw uint8");
            b.push_back(static_cast<std::uint8_t>(value));
        }
        else
            Monstey_SAppendFloat(b, value);
    }
    return b;
}
bool Monstey_SCheckMatrix(const J &actual, const Monstey_ModelChannel &expected, std::size_t count, bool byte)
{
    if (!actual.is_array() || actual.size() != count)
        return false;
    B encoded;
    for (const auto &row : actual.as_array())
    {
        if (!row.is_array() || row.size() != expected.width)
            return false;
        for (const auto &value : row.as_array())
        {
            if (!value.is_number())
                return false;
            const auto n = value.as_number();
            if (byte)
            {
                if (!std::isfinite(n) || n < 0 || n > 255 || std::floor(n) != n)
                    return false;
                encoded.push_back(static_cast<std::uint8_t>(n));
            }
            else
                Monstey_SAppendFloat(encoded, n);
        }
    }
    return encoded == Monstey_SChannelBytes(expected, byte);
}
} // namespace
Monstey_StaticPrefabResult Monstey_StaticPrefabBuild(std::span<const std::uint8_t> source, std::size_t container,
                                                     std::string_view inputResource, std::string_view inputMaterial,
                                                     std::span<const std::uint8_t> nativeTemplate,
                                                     const std::optional<std::vector<std::size_t>> &selected)
{
    const auto resource = Monstey_SVirtual(inputResource), material = Monstey_SVirtual(inputMaterial);
    Monstey_Require(resource.ends_with(".inst") && material.ends_with(".mat"),
                    "Static output requires .inst resource and .mat material paths");
    const auto scene = Monstey_ModelReadRigidSceneBytes(source);
    const auto metadata = Monstey_ModelNativePartMetadata(source);
    Monstey_Require(scene.source_sha256 == Monstey_HashSha256(source) &&
                        metadata.at("source_sha256").as_string() == scene.source_sha256,
                    "Static source readers disagree on immutable source identity");
    Monstey_Require(container < metadata.at("containers").as_uint(), "MMSH container outside source");
    std::vector<const Monstey_ModelPart *> parts;
    std::vector<J> partMetadata;
    for (const auto &part : scene.parts)
        if (part.container == container)
        {
            parts.push_back(&part);
            std::optional<J> row;
            for (const auto &m : metadata.at("parts").as_array())
                if (m.at("container").as_uint() == container && m.at("part").as_uint() == part.part)
                {
                    Monstey_Require(!row, "Duplicate MMSH native draw metadata");
                    row = m;
                }
            Monstey_Require(row.has_value(), "MMSH native draw metadata missing");
            partMetadata.push_back(*row);
        }
    std::vector<std::size_t> selection;
    if (selected)
        selection = *selected;
    else
        for (std::size_t i = 0; i < parts.size(); ++i)
            selection.push_back(i);
    Monstey_Require(!selection.empty() && selection.size() <= 65535, "Static definition draw count must be1..65535");
    std::set<std::size_t> seen;
    for (const auto i : selection)
        Monstey_Require(i < parts.size() && seen.insert(i).second, "Invalid/duplicate static selected part");
    B vertices, indices;
    std::vector<std::array<std::uint32_t, 13>> draws;
    J proofs = J::array();
    std::array<double, 3> lo{}, hi{};
    bool first = true;
    std::size_t totalVertices = 0, totalTriangles = 0;
    for (const auto index : selection)
    {
        const auto &part = *parts[index];
        const auto &raw = partMetadata[index];
        for (const auto name : {"POSITION", "UV0", "NORMAL", "TANGENT", "BITANGENT", "COLOR0"})
            Monstey_Require(part.channels.contains(name), std::string("Rigid source contract lacks ") + name);
        const auto &pos = part.channels.at("POSITION");
        Monstey_Require(pos.width == 3 && pos.values.size() % 3 == 0, "Invalid source positions");
        const auto count = pos.values.size() / 3;
        Monstey_Require(count > 0 && count <= 65535, "Static part vertex count outside uint16");
        for (const auto &[name, width] : std::array<std::pair<const char *, unsigned>, 5>{
                 {{"POSITION", 3}, {"UV0", 2}, {"NORMAL", 3}, {"TANGENT", 3}, {"BITANGENT", 3}}})
        {
            const auto &ch = part.channels.at(name);
            Monstey_Require(
                ch.width == width && ch.values.size() == count * width &&
                    std::all_of(ch.values.begin(), ch.values.end(),
                                [](double v) { return std::isfinite(v) && std::isfinite(static_cast<float>(v)); }),
                "Invalid finite source channel " + std::string(name));
        }
        const auto &color = part.channels.at("COLOR0");
        Monstey_Require(color.width == 4 && color.values.size() == count * 4 && !raw.at("color0_storage").is_null() &&
                            raw.at("color0_storage").at("type").as_uint() == 4 &&
                            raw.at("color0_storage").at("storage_width").as_uint() == 4 &&
                            raw.at("color0_storage").at("storage_bytes").as_uint() == 4,
                        "Native raw four-byte source color storage required");
        Monstey_Require(!part.indices.empty() && part.indices.size() % 3 == 0 && part.indices.size() / 3 <= 0xffffff,
                        "Invalid static source triangles");
        for (const auto i : part.indices)
            Monstey_Require(i < count, "Source triangle index outside vertex table");
        const auto triangleCount = part.indices.size() / 3;
        Monstey_Require(vertices.size() % 64 == 0 && vertices.size() / 64 <= UINT32_MAX &&
                            indices.size() / 2 <= UINT32_MAX,
                        "Static buffer offsets exceed native storage");
        const auto base = static_cast<std::uint32_t>(vertices.size() / 64),
                   firstIndex = static_cast<std::uint32_t>(indices.size() / 2);
        for (std::size_t i = 0; i < count; ++i)
        {
            for (std::size_t c = 0; c < 3; ++c)
                Monstey_SAppendFloat(vertices, pos.values[i * 3 + c]);
            Monstey_SAppendFloat(vertices, 1.0);
        }
        for (std::size_t i = 0; i < count; ++i)
        {
            for (const auto name : {"UV0", "NORMAL", "TANGENT", "BITANGENT"})
            {
                const auto &ch = part.channels.at(name);
                for (std::size_t c = 0; c < ch.width; ++c)
                    Monstey_SAppendFloat(vertices, ch.values[i * ch.width + c]);
            }
            for (std::size_t c = 0; c < 4; ++c)
            {
                const auto value = color.values[i * 4 + c];
                Monstey_Require(value >= 0 && value <= 255 && std::floor(value) == value, "Raw color byte invalid");
                vertices.push_back(static_cast<std::uint8_t>(value));
            }
        }
        for (const auto i : part.indices)
            Monstey_AppendU16(indices, static_cast<std::uint16_t>(i));
        Monstey_Require(vertices.size() <= UINT32_MAX && indices.size() <= UINT32_MAX, "Static buffer exceeds uint32");
        const auto &w = raw.at("draw_parameter_words");
        Monstey_Require(w.size() == 2, "Native per-draw parameters missing");
        draws.push_back({0, static_cast<std::uint32_t>(count), static_cast<std::uint32_t>(count), firstIndex, base,
                         static_cast<std::uint32_t>(triangleCount), 0, 0, 0, 0,
                         static_cast<std::uint32_t>(vertices.size()), static_cast<std::uint32_t>(w.at(0).as_uint()),
                         static_cast<std::uint32_t>(w.at(1).as_uint())});
        for (std::size_t i = 0; i < count; ++i)
            for (std::size_t c = 0; c < 3; ++c)
            {
                const auto v = pos.values[i * 3 + c];
                if (first && i == 0)
                {
                    lo[c] = hi[c] = v;
                }
                else
                {
                    lo[c] = std::min(lo[c], v);
                    hi[c] = std::max(hi[c], v);
                }
            }
        first = false;
        J channels = J::object();
        for (const auto name : {"POSITION", "UV0", "NORMAL", "TANGENT", "BITANGENT", "COLOR0"})
            channels[name == std::string_view("BITANGENT") ? "BINORMAL" : name] =
                Monstey_HashSha256(Monstey_SChannelBytes(part.channels.at(name), name == std::string_view("COLOR0")));
        B ib;
        for (const auto i : part.indices)
            Monstey_AppendU16(ib, static_cast<std::uint16_t>(i));
        channels["INDICES"] = Monstey_HashSha256(ib);
        proofs.as_array().push_back(J::object({{"source_part", index},
                                               {"vertices", count},
                                               {"triangles", triangleCount},
                                               {"channels", channels},
                                               {"draw_parameter_words", w}}));
        totalVertices += count;
        totalTriangles += triangleCount;
    }
    B geometry;
    Monstey_AppendU32(geometry, static_cast<std::uint32_t>(draws.size()));
    Monstey_AppendU32(geometry, static_cast<std::uint32_t>(vertices.size()));
    Monstey_AppendU32(geometry, static_cast<std::uint32_t>(indices.size()));
    geometry.insert(geometry.end(), vertices.begin(), vertices.end());
    geometry.insert(geometry.end(), indices.begin(), indices.end());
    Monstey_AppendU32(geometry, 1);
    Monstey_AppendU32(geometry, 6);
    constexpr std::array<std::array<std::uint8_t, 4>, 6> layout{
        {{0, 3, 0, 0}, {1, 1, 4, 0}, {1, 2, 1, 0}, {1, 2, 2, 0}, {1, 2, 3, 0}, {1, 4, 5, 0}}};
    for (const auto &attribute : layout)
        geometry.insert(geometry.end(), attribute.begin(), attribute.end());
    Monstey_AppendU32(geometry, static_cast<std::uint32_t>(draws.size()));
    for (const auto &draw : draws)
        for (const auto word : draw)
            Monstey_AppendU32(geometry, word);
    auto encoded = Monstey_InstBuildRigid(nativeTemplate, resource, material, geometry);
    const auto decoded = Monstey_InstDecodeGeometry(encoded);
    const auto check = Monstey_InstReadGeometry(encoded);
    Monstey_Require(decoded.size() == selection.size() && check.index_buffer == indices &&
                        check.vertex_buffer == vertices,
                    "Static geometry buffer/readback differs");
    for (std::size_t p = 0; p < selection.size(); ++p)
    {
        const auto &part = *parts[selection[p]];
        const auto n = part.channels.at("POSITION").values.size() / 3;
        for (const auto name : {"POSITION", "UV0", "NORMAL", "TANGENT", "BITANGENT", "COLOR0"})
        {
            const auto key = name == std::string_view("BITANGENT") ? "BINORMAL" : name;
            Monstey_Require(Monstey_SCheckMatrix(decoded.at(p).at(key), part.channels.at(name), n,
                                                 name == std::string_view("COLOR0")),
                            "Static channel differs: " + std::string(key));
        }
        J faces = J::array();
        for (std::size_t i = 0; i < part.indices.size(); i += 3)
            faces.as_array().push_back(J::array({part.indices[i], part.indices[i + 1], part.indices[i + 2]}));
        Monstey_Require(decoded.at(p).at("INDICES") == faces &&
                            decoded.at(p).at("draw_parameter_words") ==
                                partMetadata[selection[p]].at("draw_parameter_words") &&
                            decoded.at(p).at("material_index").as_uint() == 0,
                        "Static indices/native draw parameters/material binding changed");
    }
    const auto donor = Monstey_InstReadGeometry(nativeTemplate);
    J selectionJson = J::array();
    for (const auto i : selection)
        selectionJson.as_array().push_back(i);
    auto proof = J::object({{"status", "MMSH_CONTAINER_TO_PRFB8_STRUCTURAL_AND_CHANNEL_EQUALITY_PASS"},
                            {"source_sha256", Monstey_HashSha256(source)},
                            {"container", container},
                            {"selected_parts", selectionJson},
                            {"all_source_container_parts_selected", !selected.has_value()},
                            {"source_model_space_bind_geometry", true},
                            {"source_skinning_channels_omitted", J::array({"JOINTS", "WEIGHTS_U8"})},
                            {"vertices", totalVertices},
                            {"triangles", totalTriangles},
                            {"parts", proofs},
                            {"vertex_buffer_bytes", vertices.size()},
                            {"index_buffer_bytes", indices.size()},
                            {"output_bytes", encoded.size()},
                            {"output_sha256", Monstey_HashSha256(encoded)},
                            {"resource_path", resource},
                            {"material_path", material},
                            {"definition_key", Monstey_Hex(Monstey_U32(donor.primary.at(0), 0))},
                            {"bounds_min", J::array({lo[0], lo[1], lo[2]})},
                            {"bounds_max", J::array({hi[0], hi[1], hi[2]})},
                            {"source_channels_and_triangle_order_exact", true},
                            {"source_colour_bytes_exact", true},
                            {"no_vertex_decimation", true},
                            {"no_UV_flip_or_rebake", true},
                            {"definition_donor", J::object({{"sha256", Monstey_HashSha256(nativeTemplate)},
                                                            {"secondary_index", donor.secondary.size() - 1},
                                                            {"primary_key_and_word4_copied_from_index", 0}})},
                            {"actual_rigid_permutation_selection_not_runtime_verified", true},
                            {"collision_generated", false},
                            {"game_modified", false},
                            {"implementation", "C++20"},
                            {"python_runtime", false},
                            {"runtime_verified", false}});
    return {std::move(encoded), std::move(proof)};
}
J Monstey_StaticPrefabConvert(const J &a)
{
    Monstey_SKeys(a, {"source", "container", "material", "shader", "template", "output", "resource_path",
                      "material_path", "texture_manifest", "dependency_manifests", "expected_sha256",
                      "template_expected_sha256", "shader_contract"});
    std::vector<Monstey_SSource> sources;
    auto capture = [&](const J &value) {
        auto record = Monstey_SRecord(Monstey_SPath(value));
        sources.push_back(record);
        return record;
    };
    const auto source = capture(a.at("source")), materialSource = capture(a.at("material")),
               shaderSource = capture(a.at("shader")), templateSource = capture(a.at("template")),
               textureSource = capture(a.at("texture_manifest"));
    if (a.contains("expected_sha256"))
        Monstey_Require(a.at("expected_sha256").as_string() == source.sha, "Static source expected SHA mismatch");
    if (a.contains("template_expected_sha256"))
        Monstey_Require(a.at("template_expected_sha256").as_string() == templateSource.sha,
                        "Static template expected SHA mismatch");
    const auto sourceBytes = Monstey_ReadFile(source.path),
               materialBytes = Monstey_ReadFile(materialSource.path, 16u * 1024 * 1024),
               shaderBytes = Monstey_ReadFile(shaderSource.path, 64u * 1024 * 1024),
               templateBytes = Monstey_ReadFile(templateSource.path);
    Monstey_Require(Monstey_HashSha256(sourceBytes) == source.sha &&
                        Monstey_HashSha256(materialBytes) == materialSource.sha &&
                        Monstey_HashSha256(shaderBytes) == shaderSource.sha &&
                        Monstey_HashSha256(templateBytes) == templateSource.sha,
                    "Static input changed before decoding");
    const auto shaderValidation = Monstey_StaticMaterialShader(materialBytes, shaderBytes),
               material = Monstey_StaticMaterialInspect(materialBytes);
    const auto resource = Monstey_SVirtual(a.at("resource_path").as_string()),
               materialResource = Monstey_SVirtual(a.at("material_path").as_string()),
               shaderResource = Monstey_SVirtual(material.at("shader").as_string());
    const auto output = Monstey_SPath(a.at("output"));
    std::vector<std::filesystem::path> protectedPaths;
    for (const auto &s : sources)
        protectedPaths.push_back(s.path);
    Monstey_RequireOfflineOutput(output, protectedPaths);
    Monstey_Require(!std::filesystem::exists(Monstey_SLong(output)), "Static prefab output directory already exists");
    J shaderContract, shaderContractSha;
    if (a.contains("shader_contract") && !a.at("shader_contract").is_null())
    {
        const auto contractSource = capture(a.at("shader_contract"));
        const auto contract = J::Parse(Monstey_ReadText(contractSource.path));
        Monstey_Require(contract.contains("material") && contract.contains("shader") &&
                            contract.at("material").at("sha256").as_string() == materialSource.sha &&
                            contract.at("shader").at("sha256").as_string() == shaderSource.sha,
                        "Explicit shader contract does not cover these exact material/shader bytes");
        shaderContract = Monstey_Utf8(contractSource.path.wstring());
        shaderContractSha = contractSource.sha;
    }
    const auto container = a.value("container", 0).as_uint();
    Monstey_Require(container <= 65535, "Static container index outside bound");
    auto probe = Monstey_StaticPrefabBuild(sourceBytes, static_cast<std::size_t>(container), resource, materialResource,
                                           templateBytes, std::vector<std::size_t>{0});
    auto built = Monstey_StaticPrefabBuild(sourceBytes, static_cast<std::size_t>(container), resource, materialResource,
                                           templateBytes);
    probe.proof["artifact_saved"] = false;
    built.proof["source"] = Monstey_SJson(source);
    built.proof["definition_donor"]["source"] = Monstey_Utf8(templateSource.path.wstring());
    built.proof["native_parameter_transfer_proof"] = shaderContract;
    built.proof["native_parameter_transfer_proof_sha256"] = shaderContractSha;
    auto loadManifest = [&](const Monstey_SSource &record) {
        const auto text = Monstey_ReadText(record.path);
        Monstey_Require(Monstey_HashSha256(std::span<const std::uint8_t>(
                            reinterpret_cast<const std::uint8_t *>(text.data()), text.size())) == record.sha,
                        "Dependency manifest changed during read");
        const auto json = J::Parse(text);
        Monstey_Require(json.contains("resources") && json.at("resources").is_array() &&
                            json.at("resources").size() <= 100000,
                        "Invalid dependency resources manifest");
        return json;
    };
    const auto textureManifest = loadManifest(textureSource);
    std::map<std::string, J> textures, dependencies;
    for (const auto &row : textureManifest.at("resources").as_array())
    {
        const auto path = Monstey_SVirtual(row.at("resource_path").as_string());
        if (path.ends_with(".tex") || path.ends_with(".tex0") || path.ends_with(".texuhd") || path.ends_with(".texa"))
            textures[path] = row;
    }
    if (a.contains("dependency_manifests"))
    {
        Monstey_Require(a.at("dependency_manifests").is_array() && a.at("dependency_manifests").size() <= 1024,
                        "Too many dependency manifests");
        for (const auto &path : a.at("dependency_manifests").as_array())
        {
            const auto record = capture(path);
            const auto manifest = loadManifest(record);
            for (const auto &row : manifest.at("resources").as_array())
                dependencies[Monstey_SVirtual(row.at("resource_path").as_string())] = row;
        }
    }
    auto checkedRow = [&](J row) {
        const auto record = capture(row.at("path"));
        Monstey_Require(row.at("bytes").as_uint() == record.bytes && row.at("sha256").as_string() == record.sha,
                        "Manifest resource changed: " + Monstey_Utf8(record.path.wstring()));
        row["path"] = Monstey_Utf8(record.path.wstring());
        row["resource_path"] = Monstey_SVirtual(row.at("resource_path").as_string());
        return row;
    };
    const auto prefabPath = output / L"assets" / std::filesystem::path(Monstey_Wide(resource)),
               materialPath = output / L"assets" / std::filesystem::path(Monstey_Wide(materialResource));
    J resources = J::array(
        {Monstey_SResource(resource, {prefabPath, built.bytes.size(), Monstey_HashSha256(built.bytes)},
                           "private_static_render_prefab"),
         Monstey_SResource(materialResource, {materialPath, materialBytes.size(), Monstey_HashSha256(materialBytes)},
                           "private_opaque_material_byte_exact_copy"),
         Monstey_SResource(shaderResource, shaderSource, "native_rigid_and_skeletal_shader_unchanged")});
    J external = J::array(), unresolved = J::array();
    std::set<std::string> required;
    for (const auto &[name, value] : material.at("properties").as_object())
    {
        (void)name;
        if (value.is_string())
        {
            auto v = value.as_string();
            std::replace(v.begin(), v.end(), '\\', '/');
            std::transform(v.begin(), v.end(), v.begin(),
                           [](unsigned char c) { return char(c >= 'A' && c <= 'Z' ? c + 32 : c); });
            if (v.ends_with(".tex") || v.ends_with(".texa"))
                required.insert(Monstey_SVirtual(v));
        }
    }
    for (const auto &path : required)
    {
        if (textures.contains(path))
        {
            auto row = checkedRow(textures.at(path));
            row["role"] = "existing_texture_byte_exact";
            row["copied_for_static_prefab"] = false;
            resources.as_array().push_back(row);
            if (path.ends_with(".tex"))
            {
                const auto stem = path.substr(0, path.size() - 4);
                for (const auto suffix : {".tex0", ".texuhd"})
                    if (textures.contains(stem + suffix))
                    {
                        auto peer = checkedRow(textures.at(stem + suffix));
                        peer["role"] = "unchanged_native_streaming_peer";
                        peer["copied"] = false;
                        peer["embed_in_embb"] = false;
                        external.as_array().push_back(peer);
                    }
            }
        }
        else if (dependencies.contains(path))
            resources.as_array().push_back(checkedRow(dependencies.at(path)));
        else
        {
            unresolved.as_array().push_back(path);
            external.as_array().push_back(J::object({{"resource_path", path},
                                                     {"role", "unresolved_shared_shader_material_dependency"},
                                                     {"copied", false},
                                                     {"embed_in_embb", false}}));
        }
    }
    std::set<std::string> resourceNames;
    for (const auto &row : resources.as_array())
        Monstey_Require(resourceNames.insert(row.at("resource_path").as_string()).second,
                        "Conflicting static resource IDs");
    auto manifest = J::object(
        {{"status", unresolved.empty() ? "PRIVATE_STATIC_PREFAB_READY_FOR_RUNTIME_CANDIDATE"
                                       : "STATIC_PREFAB_GEOMETRY_READY_DEPENDENCIES_PENDING"},
         {"resources", resources},
         {"external_dependencies", external},
         {"unresolved_shared_dependencies", unresolved},
         {"explicit_material_dependencies_resolved", unresolved.empty()},
         {"source_mesh", Monstey_SJson(source)},
         {"source_material", Monstey_SJson(materialSource)},
         {"template", Monstey_SJson(templateSource)},
         {"texture_manifest", Monstey_SJson(textureSource)},
         {"shader_validation", shaderValidation.at("shader_id")},
         {"shader_validation_details", shaderValidation},
         {"shader_contract", shaderContract},
         {"shader_contract_sha256", shaderContractSha},
         {"geometry_proof", Monstey_Utf8((output / L"geometry-proof.json").wstring())},
         {"single_surface_probe", Monstey_Utf8((output / L"single-surface-probe.json").wstring())},
         {"placement_contract",
          J::object({{"resource_path", resource},
                     {"definition_key", built.proof.at("definition_key")},
                     {"bounds_min", built.proof.at("bounds_min")},
                     {"bounds_max", built.proof.at("bounds_max")},
                     {"coordinate_frame", "Source model-space bind vertices in native centimetres"},
                     {"expected_binding", "WDSC native prefab path+definition key and explicit instance transform"},
                     {"WDSC_or_world_object_binding_not_included", true}})},
         {"physical_triangles", built.proof.at("triangles")},
         {"source_texture_resolution_preserved", true},
         {"held_weapon_files_unchanged", true},
         {"spectral_container_and_Wraith_FX_excluded", true},
         {"opaque_red_is_albedo_not_emission", true},
         {"collision_generated", false},
         {"navmesh_generated", false},
         {"actual_render_or_visibility_not_runtime_verified", true},
         {"game_modified", false},
         {"implementation", "C++20"},
         {"python_runtime", false},
         {"runtime_verified", false}});
    J inputRows = J::array();
    for (const auto &s : sources)
        inputRows.as_array().push_back(Monstey_SJson(s));
    manifest["inputs"] = inputRows;
    manifest["resource_manifest"] = Monstey_Utf8((output / L"resource-manifest.json").wstring());
    manifest["source_unchanged"] = true;
    std::map<std::filesystem::path, B> files;
    files[std::filesystem::path(L"assets") / std::filesystem::path(Monstey_Wide(resource))] = std::move(built.bytes);
    files[std::filesystem::path(L"assets") / std::filesystem::path(Monstey_Wide(materialResource))] = materialBytes;
    auto jsonBytes = [](const J &j) {
        const auto text = j.dump(2) + "\n";
        return B(text.begin(), text.end());
    };
    files[L"single-surface-probe.json"] = jsonBytes(probe.proof);
    files[L"geometry-proof.json"] = jsonBytes(built.proof);
    files[L"resource-manifest.json"] = jsonBytes(manifest);
    Monstey_SCheck(sources);
    std::filesystem::path stage;
    for (unsigned attempt = 0; attempt < 100; ++attempt)
    {
        stage = output.parent_path() / (L".Monstey-static-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                        std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()) +
                                        L"-" + std::to_wstring(attempt));
        Monstey_RequireOfflineOutput(stage, protectedPaths);
        std::filesystem::create_directories(Monstey_SLong(stage.parent_path()));
        if (std::filesystem::create_directory(Monstey_SLong(stage)))
            break;
        stage.clear();
    }
    Monstey_Require(!stage.empty(), "Cannot create new static prefab stage directory");
    bool published = false;
    std::vector<std::filesystem::path> written;
    auto clean = [&](const std::filesystem::path &base) {
        std::set<std::filesystem::path> dirs;
        for (const auto &relative : written)
        {
            const auto path = base / relative;
            try
            {
                if (std::filesystem::exists(Monstey_SLong(path)) &&
                    Monstey_HashSha256(path) == Monstey_HashSha256(files.at(relative)))
                    std::filesystem::remove(Monstey_SLong(path));
            }
            catch (const std::exception &)
            {
            }
            auto dir = path.parent_path();
            while (dir != base && dir.native().size() >= base.native().size())
            {
                dirs.insert(dir);
                dir = dir.parent_path();
            }
        }
        std::vector<std::filesystem::path> ordered(dirs.begin(), dirs.end());
        std::sort(ordered.begin(), ordered.end(),
                  [](const auto &a, const auto &b) { return a.native().size() > b.native().size(); });
        for (const auto &dir : ordered)
        {
            std::error_code error;
            std::filesystem::remove(Monstey_SLong(dir), error);
        }
        std::error_code error;
        std::filesystem::remove(Monstey_SLong(base), error);
    };
    try
    {
        for (const auto &[relative, bytes] : files)
        {
            const auto path = stage / relative;
            Monstey_WriteNewFile(path, bytes);
            written.push_back(relative);
            Monstey_Require(Monstey_HashSha256(path) == Monstey_HashSha256(bytes),
                            "Staged static prefab output hash differs");
        }
        Monstey_SCheck(sources);
        Monstey_Require(
            MoveFileExW(Monstey_SLong(stage).c_str(), Monstey_SLong(output).c_str(), MOVEFILE_WRITE_THROUGH) != 0,
            "Static prefab output exists or directory publication failed");
        published = true;
        for (const auto &[relative, bytes] : files)
            Monstey_Require(Monstey_HashSha256(output / relative) == Monstey_HashSha256(bytes),
                            "Published static prefab output hash differs");
        Monstey_SCheck(sources);
    }
    catch (...)
    {
        clean(published ? output : stage);
        throw;
    }
    manifest["resource_manifest"] = Monstey_Utf8((output / L"resource-manifest.json").wstring());
    manifest["source_unchanged"] = true;
    return manifest;
}
} // namespace Monstey
