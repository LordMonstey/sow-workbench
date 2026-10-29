#include "Monstey_ModelPrivate.hpp"

namespace Monstey
{
namespace Monstey_ModelPrivate
{

std::size_t Monstey_GlbBuilder::View(Monstey_Span bytes)
{
    while (binary.size() % 4)
        binary.push_back(0);
    const auto offset = binary.size();
    binary.insert(binary.end(), bytes.begin(), bytes.end());
    doc["bufferViews"].as_array().push_back(
        Monstey_J::object({{"buffer", 0u},
                           {"byteOffset", static_cast<std::uint64_t>(offset)},
                           {"byteLength", static_cast<std::uint64_t>(bytes.size())}}));
    return doc["bufferViews"].size() - 1;
}

std::size_t Monstey_GlbBuilder::Accessor(const Monstey_Channel &channel, std::uint32_t component, bool normalized,
                                         std::string type)
{
    if (type.empty())
        type = channel.width == 1   ? "SCALAR"
               : channel.width == 2 ? "VEC2"
               : channel.width == 3 ? "VEC3"
               : channel.width == 4 ? "VEC4"
                                    : "MAT4";
    Monstey_MRequire(channel.width > 0 && channel.v.size() % channel.width == 0, "Bad output accessor");
    Monstey_Bytes data;
    for (const auto v : channel.v)
    {
        if (component == 5126)
            Monstey_AddF32(data, v);
        else
        {
            Monstey_MRequire(v >= 0 && std::floor(v) == v, "Nonintegral GLB integer channel");
            if (component == 5121)
            {
                Monstey_MRequire(v <= 255, "GLB byte overflow");
                data.push_back(static_cast<std::uint8_t>(v));
            }
            else if (component == 5123)
            {
                Monstey_MRequire(v <= 65535, "GLB short overflow");
                Monstey_AppendU16(data, static_cast<std::uint16_t>(v));
            }
            else
            {
                Monstey_MRequire(v <= 0xffffffffu, "GLB index overflow");
                Monstey_AppendU32(data, static_cast<std::uint32_t>(v));
            }
        }
    }
    auto a = Monstey_J::object({{"bufferView", static_cast<std::uint64_t>(View(data))},
                                {"componentType", component},
                                {"count", static_cast<std::uint64_t>(channel.v.size() / channel.width)},
                                {"type", type}});
    if (normalized)
        a["normalized"] = true;
    doc["accessors"].as_array().push_back(a);
    return doc["accessors"].size() - 1;
}

Monstey_Bytes Monstey_GlbBuilder::Finish()
{
    doc["buffers"] = Monstey_J::array({Monstey_J::object({{"byteLength", static_cast<std::uint64_t>(binary.size())}})});
    auto text = doc.dump();
    while (text.size() % 4)
        text.push_back(' ');
    while (binary.size() % 4)
        binary.push_back(0);
    Monstey_Bytes out;
    Monstey_AppendU32(out, 0x46546c67);
    Monstey_AppendU32(out, 2);
    Monstey_AppendU32(out, Monstey_Size(12 + 8 + text.size() + 8 + binary.size()));
    Monstey_AppendU32(out, Monstey_Size(text.size()));
    Monstey_AppendU32(out, 0x4e4f534a);
    out.insert(out.end(), text.begin(), text.end());
    Monstey_AppendU32(out, Monstey_Size(binary.size()));
    Monstey_AppendU32(out, 0x004e4942);
    out.insert(out.end(), binary.begin(), binary.end());
    return out;
}

Monstey_Bytes Monstey_BuildGlb(const Monstey_ModelData &m, const Monstey_Rig &rig)
{
    Monstey_GlbBuilder b;
    auto &doc = b.doc;
    doc["nodes"] = Monstey_J::array();
    doc["meshes"] = Monstey_J::array();
    auto roots = Monstey_J::array();
    for (std::size_t hk = 0; hk < rig.hkToSource.size(); ++hk)
        if (rig.source[rig.hkToSource[hk]].parent < 0)
            roots.as_array().push_back(static_cast<std::uint64_t>(hk + 1));
    doc["nodes"].as_array().push_back(Monstey_J::object({{"name", "SoW_Skeleton"}, {"children", roots}}));
    for (std::size_t hk = 0; hk < rig.hkToSource.size(); ++hk)
    {
        const auto &bone = rig.source[rig.hkToSource[hk]];
        auto local = bone.global;
        if (bone.parent >= 0)
            local = Monstey_Mul(Monstey_Inv(rig.source[static_cast<std::size_t>(bone.parent)].global), local);
        auto node = Monstey_J::object(
            {{"name", bone.name},
             {"matrix", Monstey_MatrixJson(local)},
             {"extras", Monstey_J::object({{"monstey_hk", static_cast<std::uint64_t>(hk)},
                                           {"monstey_source", static_cast<std::uint64_t>(rig.hkToSource[hk])}})}});
        auto children = Monstey_J::array();
        for (std::size_t j = 0; j < rig.hkToSource.size(); ++j)
            if (rig.source[rig.hkToSource[j]].parent == static_cast<int>(rig.hkToSource[hk]))
                children.as_array().push_back(static_cast<std::uint64_t>(j + 1));
        if (!children.empty())
            node["children"] = children;
        doc["nodes"].as_array().push_back(node);
    }
    Monstey_Channel inverses;
    inverses.width = 16;
    auto joints = Monstey_J::array();
    for (std::size_t hk = 0; hk < rig.hkToSource.size(); ++hk)
    {
        joints.as_array().push_back(static_cast<std::uint64_t>(hk + 1));
        const auto inv = Monstey_Inv(rig.source[rig.hkToSource[hk]].global);
        for (unsigned c = 0; c < 4; ++c)
            for (unsigned r = 0; r < 4; ++r)
                inverses.v.push_back(inv[r * 4 + c]);
    }
    doc["skins"] = Monstey_J::array({Monstey_J::object(
        {{"name", "Native_HK_Rig"},
         {"joints", joints},
         {"skeleton", 0u},
         {"inverseBindMatrices", static_cast<std::uint64_t>(b.Accessor(inverses, 5126, false, "MAT4"))}})});
    auto sceneNodes = Monstey_J::array({0u});
    for (const auto &part : Monstey_DecodeParts(m))
    {
        auto attrs = Monstey_J::object();
        for (const auto &[name, ch] : part.channels)
        {
            auto source = ch;
            std::string key = name;
            std::uint32_t component = 5126;
            bool normalized = false;
            if (name == "POSITION")
            {
                for (auto &x : source.v)
                    x /= 100;
            }
            else if (name == "TANGENT")
            {
                source.width = 4;
                source.v.clear();
                for (std::size_t i = 0; i < part.Count(); ++i)
                {
                    const auto t = Monstey_At3(ch, i), n = Monstey_At3(part.channels.at("NORMAL"), i),
                               bi = Monstey_At3(part.channels.at("BITANGENT"), i);
                    source.v.insert(source.v.end(), t.begin(), t.end());
                    source.v.push_back(Monstey_Dot(Monstey_Cross(n, t), bi) < 0 ? -1 : 1);
                }
            }
            else if (name == "BITANGENT")
                key = "_SOW_BITANGENT";
            else if (name == "JOINTS")
            {
                key = "JOINTS_0";
                component = 5123;
            }
            else if (name == "WEIGHTS_U8")
            {
                key = "WEIGHTS_0";
                component = 5121;
                normalized = true;
            }
            else if (name.starts_with("UV"))
                key = "TEXCOORD_" + name.substr(2);
            else if (name.starts_with("COLOR"))
            {
                key = "COLOR_" + name.substr(5);
                component = 5121;
                normalized = true;
            }
            if (name == "NORMAL" || name == "TANGENT" || name == "BITANGENT")
            {
                const bool nonfinite =
                    std::any_of(ch.v.begin(), ch.v.end(), [](double v) { return !std::isfinite(v); });
                if (nonfinite)
                {
                    Monstey_Channel low, high;
                    low.width = high.width = 4;
                    for (std::size_t i = 0; i < part.Count(); ++i)
                    {
                        for (unsigned k = 0; k < 3; ++k)
                        {
                            const auto bits = std::bit_cast<std::uint32_t>(static_cast<float>(ch.v[i * 3 + k]));
                            low.v.push_back(bits & 65535u);
                            high.v.push_back(bits >> 16);
                        }
                        low.v.push_back(0);
                        high.v.push_back(0);
                    }
                    attrs["_SOW_" + name + "_BITS_LO"] = static_cast<std::uint64_t>(b.Accessor(low, 5123));
                    attrs["_SOW_" + name + "_BITS_HI"] = static_cast<std::uint64_t>(b.Accessor(high, 5123));
                    const Monstey_V3 fallback = name == "NORMAL"    ? Monstey_V3{0, 0, 1}
                                                : name == "TANGENT" ? Monstey_V3{1, 0, 0}
                                                                    : Monstey_V3{0, 1, 0};
                    for (std::size_t i = 0; i < part.Count(); ++i)
                    {
                        const auto raw = Monstey_At3(ch, i);
                        if (!std::all_of(raw.begin(), raw.end(), [](double v) { return std::isfinite(v); }))
                            for (unsigned k = 0; k < 3; ++k)
                                source.v[i * source.width + k] = fallback[k];
                    }
                }
            }
            const auto accessor = b.Accessor(source, component, normalized);
            attrs[key] = static_cast<std::uint64_t>(accessor);
            if (name == "POSITION")
            {
                auto minimum = Monstey_J::array(), maximum = Monstey_J::array();
                for (unsigned k = 0; k < 3; ++k)
                {
                    double lo = source.v[k], hi = lo;
                    for (std::size_t i = 0; i < part.Count(); ++i)
                    {
                        lo = std::min(lo, source.v[i * 3 + k]);
                        hi = std::max(hi, source.v[i * 3 + k]);
                    }
                    minimum.as_array().push_back(lo);
                    maximum.as_array().push_back(hi);
                }
                doc["accessors"][accessor]["min"] = minimum;
                doc["accessors"][accessor]["max"] = maximum;
            }
        }
        Monstey_Channel indices;
        indices.width = 1;
        for (const auto i : part.indices)
            indices.v.push_back(i);
        auto primitive = Monstey_J::object(
            {{"attributes", attrs},
             {"indices", static_cast<std::uint64_t>(b.Accessor(indices, 5123))},
             {"mode", 4u},
             {"extras", Monstey_J::object({{"monstey_container", static_cast<std::uint64_t>(part.container)},
                                           {"monstey_part", static_cast<std::uint64_t>(part.part)}})}});
        if (Monstey_HasPartBands(part))
        {
            auto bands = Monstey_J::array();
            for (const auto band : part.triangle_bands)
                bands.as_array().push_back(static_cast<std::uint64_t>(band));
            primitive["extras"]["monstey_triangle_bands"] = std::move(bands);
            primitive["extras"]["monstey_triangle_topology_sha256"] = part.triangle_band_topology_sha256;
        }
        const auto mi = doc["meshes"].size();
        doc["meshes"].as_array().push_back(Monstey_J::object(
            {{"name", "Container_" + std::to_string(part.container) + "_Part_" + std::to_string(part.part)},
             {"primitives", Monstey_J::array({primitive})}}));
        sceneNodes.as_array().push_back(static_cast<std::uint64_t>(doc["nodes"].size()));
        doc["nodes"].as_array().push_back(Monstey_J::object({{"mesh", static_cast<std::uint64_t>(mi)}, {"skin", 0u}}));
    }
    doc["scenes"] = Monstey_J::array({Monstey_J::object({{"nodes", sceneNodes}})});
    doc["scene"] = 0u;
    const auto nativeView = b.View(m.raw);
    doc["extras"] = Monstey_J::object(
        {{"monstey_native", Monstey_J::object({{"version", 1u},
                                               {"template_sha256", Monstey_HashSha256(m.raw)},
                                               {"native_bufferView", static_cast<std::uint64_t>(nativeView)},
                                               {"coordinate_system", "native Y-up, glTF metres"},
                                               {"all_eight_vertex_channels", true}})}});
    return b.Finish();
}

} // namespace Monstey_ModelPrivate
using namespace Monstey_ModelPrivate;

Monstey_Json Monstey_ModelExportGlb(const std::filesystem::path &mesh, const std::filesystem::path &output,
                                    const std::filesystem::path &skel)
{
    std::vector<std::filesystem::path> sources = {mesh};
    if (!skel.empty())
        sources.push_back(skel);
    Monstey_RequireOfflineOutput(output, sources);
    const auto m = Monstey_LoadModel(Monstey_ReadFile(mesh));
    Monstey_MRequire(Monstey_Allocation(m)["safe"].as_bool(), "Unsafe native source allocations");
    const auto rig = Monstey_ReadModelRig(m, skel);
    const auto encoded = Monstey_BuildGlb(m, rig);
    const Monstey_Glb check(encoded);
    Monstey_MRequire(check.primitives.size() == m.parts.size(), "GLB export readback lost draws");
    Monstey_WriteNewFile(output, encoded);
    return Monstey_J::object({{"output", Monstey_Utf8(output.wstring())},
                              {"sha256", Monstey_HashSha256(encoded)},
                              {"source_sha256", Monstey_HashSha256(m.raw)},
                              {"parts", static_cast<std::uint64_t>(check.primitives.size())},
                              {"bones", static_cast<std::uint64_t>(rig.source.size())},
                              {"all_eight_vertex_channels", true},
                              {"skel_matching_verified", rig.namesVerified},
                              {"units", "metres; native conversion uses 100 centimetres per metre"},
                              {"native_materials_exported", false},
                              {"runtime_verified", false}});
}

} // namespace Monstey
