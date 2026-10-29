#include "Monstey_ModelPrivate.hpp"

namespace Monstey
{
namespace Monstey_ModelPrivate
{

Monstey_Channel Monstey_Channel3(std::size_t count, const Monstey_V3 &value)
{
    Monstey_Channel c;
    c.width = 3;
    for (std::size_t i = 0; i < count; ++i)
        c.v.insert(c.v.end(), value.begin(), value.end());
    return c;
}

Monstey_V3 Monstey_At3(const Monstey_Channel &c, std::size_t i)
{
    return {c.v[i * c.width], c.v[i * c.width + 1], c.v[i * c.width + 2]};
}

void Monstey_Set3(Monstey_Channel &c, std::size_t i, const Monstey_V3 &value)
{
    for (unsigned k = 0; k < 3; ++k)
        c.v[i * c.width + k] = value[k];
}

void Monstey_QuantWeights(Monstey_Channel &w)
{
    Monstey_MRequire(w.width == 4 && w.v.size() % 4 == 0, "Weights must have four channels");
    for (std::size_t v = 0; v < w.v.size(); v += 4)
    {
        double sum = 0;
        for (unsigned k = 0; k < 4; ++k)
        {
            Monstey_MRequire(std::isfinite(w.v[v + k]) && w.v[v + k] >= 0, "Invalid weight");
            sum += w.v[v + k];
        }
        Monstey_MRequire(sum > 1e-12, "Vertex has no weighted bone");
        std::array<double, 4> fractions{};
        std::array<unsigned, 4> order = {0, 1, 2, 3};
        unsigned total = 0;
        for (unsigned k = 0; k < 4; ++k)
        {
            const auto scaled = w.v[v + k] * 255 / sum;
            const auto q = std::floor(scaled);
            w.v[v + k] = q;
            fractions[k] = scaled - q;
            total += static_cast<unsigned>(q);
        }
        std::stable_sort(order.begin(), order.end(),
                         [&](unsigned a, unsigned b) { return fractions[a] > fractions[b]; });
        Monstey_MRequire(total <= 255 && 255 - total <= 4, "Weight quantization error");
        for (unsigned k = 0; k < 255 - total; ++k)
            ++w.v[v + order[k]];
    }
}

Monstey_Part Monstey_FromGlb(const Monstey_Glb &glb, const Monstey_GlbPrimitive &primitive,
                             const Monstey_ModelData &target, const Monstey_J &options, bool nativeMode)
{
    const auto &attrs = primitive.definition["attributes"];
    Monstey_MRequire(attrs.contains("POSITION"), "GLB primitive lacks POSITION");
    Monstey_Part out;
    out.channels["POSITION"] = glb.Accessor(static_cast<std::size_t>(attrs["POSITION"].as_uint()));
    auto &pos = out.channels["POSITION"];
    Monstey_MRequire(pos.width == 3, "GLB POSITION must be VEC3");
    const auto n = out.Count();
    Monstey_MRequire(n > 0, "Empty source geometry");
    if (primitive.definition.contains("indices"))
    {
        const auto ai = static_cast<std::size_t>(primitive.definition["indices"].as_uint());
        const auto &desc = glb.doc["accessors"][ai];
        const auto t = desc["componentType"].as_uint();
        Monstey_MRequire((t == 5121 || t == 5123 || t == 5125) && !desc.value("normalized", false).as_bool(),
                         "Invalid GLB index storage");
        const auto indices = glb.Accessor(ai);
        Monstey_MRequire(indices.width == 1 && indices.v.size() % 3 == 0, "Indices must form triangles");
        for (const auto x : indices.v)
        {
            Monstey_MRequire(x >= 0 && x < n && std::floor(x) == x, "GLB vertex index outside primitive");
            out.indices.push_back(static_cast<std::uint32_t>(x));
        }
    }
    else
    {
        Monstey_MRequire(n % 3 == 0, "Unindexed GLB does not form triangles");
        for (std::size_t i = 0; i < n; ++i)
            out.indices.push_back(Monstey_Size(i));
    }
    const bool explicitBand = options.contains("triangle_band");
    if (primitive.definition.contains("extras") && primitive.definition["extras"].contains("monstey_triangle_bands"))
    {
        const auto &extra = primitive.definition["extras"];
        std::vector<std::uint8_t> bands;
        for (const auto &value : extra["monstey_triangle_bands"].as_array())
        {
            const auto band = value.as_uint();
            Monstey_MRequire(band <= 16, "GLB native triangle-band ordinal must be 0-16");
            bands.push_back(static_cast<std::uint8_t>(band));
        }
        if (!explicitBand)
        {
            Monstey_MRequire(extra.contains("monstey_triangle_topology_sha256") &&
                                 extra["monstey_triangle_topology_sha256"].as_string() ==
                                     Monstey_ModelTriangleTopologyHash(out.indices),
                             "GLB triangle bands have a stale/missing index-topology fingerprint");
            Monstey_SetPartBands(out, bands);
        }
    }
    if (explicitBand)
    {
        const auto band = options["triangle_band"].as_uint();
        Monstey_MRequire(band <= 16, "triangle_band must be an explicit ordinal 0-16");
        Monstey_SetPartBands(out, std::vector<std::uint8_t>(out.indices.size() / 3, static_cast<std::uint8_t>(band)));
    }
    auto world = primitive.world;
    const double scale = options.value("scale_cm_per_metre", 100.0).as_number();
    Monstey_MRequire(std::isfinite(scale) && scale > 0, "Scale must be positive cm per metre");
    if (options.contains("rotate_degrees"))
    {
        const auto &e = options["rotate_degrees"];
        Monstey_MRequire(e.size() == 3, "rotate_degrees requires XYZ");
        Monstey_M4 rotation = Monstey_Identity();
        constexpr double pi = 3.14159265358979323846;
        for (unsigned k = 0; k < 3; ++k)
        {
            auto axis = Monstey_Identity();
            const double a = e[k].as_number() * pi / 180, c = std::cos(a), s = std::sin(a);
            const unsigned u = (k + 1) % 3, v = (k + 2) % 3;
            axis[u * 4 + u] = c;
            axis[u * 4 + v] = -s;
            axis[v * 4 + u] = s;
            axis[v * 4 + v] = c;
            rotation = Monstey_Mul(axis, rotation);
        }
        world = Monstey_Mul(rotation, world);
    }
    const double determinant = world[0] * (world[5] * world[10] - world[6] * world[9]) -
                               world[1] * (world[4] * world[10] - world[6] * world[8]) +
                               world[2] * (world[4] * world[9] - world[5] * world[8]);
    Monstey_MRequire(std::abs(determinant) > 1e-12, "Singular GLB mesh transform");
    const auto inv = Monstey_Inv(world);
    for (std::size_t i = 0; i < n; ++i)
    {
        auto v = Monstey_Vector(world, Monstey_At3(pos, i), true);
        for (auto &x : v)
        {
            x *= scale;
            Monstey_MRequire(std::isfinite(x) && std::abs(x) <= std::numeric_limits<float>::max(),
                             "Transformed position outside FLOAT32");
        }
        Monstey_Set3(pos, i, v);
    }
    if (determinant < 0)
        for (std::size_t i = 0; i < out.indices.size(); i += 3)
            std::swap(out.indices[i + 1], out.indices[i + 2]);
    const auto channel = [&](const std::string &key, unsigned width)
    {
        const auto c = glb.Accessor(static_cast<std::size_t>(attrs[key].as_uint()));
        Monstey_MRequire(c.width == width && c.v.size() == n * width,
                         std::string("GLB channel dimensions invalid: ") + key);
        return c;
    };
    auto generated = Monstey_Channel3(n, {0, 0, 0});
    for (std::size_t i = 0; i < out.indices.size(); i += 3)
    {
        const auto ia = out.indices[i], ib = out.indices[i + 1], ic = out.indices[i + 2];
        const auto a = Monstey_At3(pos, ia), b = Monstey_At3(pos, ib), c = Monstey_At3(pos, ic);
        Monstey_V3 x{}, y{};
        for (unsigned k = 0; k < 3; ++k)
        {
            x[k] = b[k] - a[k];
            y[k] = c[k] - a[k];
        }
        const auto normal = Monstey_Cross(x, y);
        for (const auto id : {ia, ib, ic})
            for (unsigned k = 0; k < 3; ++k)
                generated.v[id * 3 + k] += normal[k];
    }
    auto normal = attrs.contains("NORMAL") ? channel("NORMAL", 3) : generated;
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto v = Monstey_At3(normal, i);
        Monstey_V3 t{};
        if (attrs.contains("NORMAL"))
            for (unsigned r = 0; r < 3; ++r)
                for (unsigned c = 0; c < 3; ++c)
                    t[r] += inv[c * 4 + r] * v[c];
        else
            t = v;
        Monstey_Set3(normal, i, nativeMode ? t : Monstey_Unit(t, Monstey_Unit(Monstey_At3(generated, i))));
    }
    out.channels["NORMAL"] = normal;
    bool gap = false;
    for (unsigned k = 0; k < 8; ++k)
    {
        const auto key = "TEXCOORD_" + std::to_string(k);
        if (attrs.contains(key))
        {
            Monstey_MRequire(!gap, "UV channels must be contiguous");
            out.channels["UV" + std::to_string(k)] = channel(key, 2);
        }
        else
            gap = true;
    }
    if (!out.channels.contains("UV0"))
        out.channels["UV0"] = {2, std::vector<double>(n * 2, 0)};
    Monstey_Channel tangent, bitangent;
    if (attrs.contains("TANGENT"))
    {
        const auto source = channel("TANGENT", 4);
        tangent = Monstey_Channel3(n, {0, 0, 0});
        bitangent = tangent;
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto no = Monstey_At3(normal, i);
            auto t = Monstey_Vector(world, Monstey_At3(source, i));
            Monstey_MRequire(std::abs(std::abs(source.v[i * 4 + 3]) - 1) <= 1e-5,
                             "GLB tangent handedness must be +/-1");
            if (!nativeMode)
            {
                const auto dot = Monstey_Dot(t, no);
                for (unsigned k = 0; k < 3; ++k)
                    t[k] -= dot * no[k];
                t = Monstey_Unit(t, Monstey_Unit(Monstey_Cross({0, 1, 0}, no), {1, 0, 0}));
            }
            auto b = Monstey_Cross(no, t);
            for (auto &x : b)
                x *= source.v[i * 4 + 3] * (determinant < 0 ? -1 : 1);
            Monstey_Set3(tangent, i, t);
            Monstey_Set3(bitangent, i, b);
        }
    }
    else
    {
        tangent = Monstey_Channel3(n, {0, 0, 0});
        bitangent = tangent;
        const auto &uv = out.channels["UV0"];
        for (std::size_t f = 0; f < out.indices.size(); f += 3)
        {
            const auto a = out.indices[f], b = out.indices[f + 1], c = out.indices[f + 2];
            const auto pa = Monstey_At3(pos, a), pb = Monstey_At3(pos, b), pc = Monstey_At3(pos, c);
            const auto du1 = uv.v[b * 2] - uv.v[a * 2], dv1 = uv.v[b * 2 + 1] - uv.v[a * 2 + 1],
                       du2 = uv.v[c * 2] - uv.v[a * 2], dv2 = uv.v[c * 2 + 1] - uv.v[a * 2 + 1],
                       d = du1 * dv2 - du2 * dv1;
            if (std::abs(d) > 1e-12)
                for (const auto id : {a, b, c})
                    for (unsigned k = 0; k < 3; ++k)
                    {
                        tangent.v[id * 3 + k] += ((pb[k] - pa[k]) * dv2 - (pc[k] - pa[k]) * dv1) / d;
                        bitangent.v[id * 3 + k] += ((pc[k] - pa[k]) * du1 - (pb[k] - pa[k]) * du2) / d;
                    }
        }
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto no = Monstey_At3(normal, i);
            auto t = Monstey_At3(tangent, i);
            const auto dot = Monstey_Dot(t, no);
            for (unsigned k = 0; k < 3; ++k)
                t[k] -= dot * no[k];
            t = Monstey_Unit(t, Monstey_Unit(Monstey_Cross({0, 1, 0}, no), {1, 0, 0}));
            auto b = Monstey_Cross(no, t);
            if (Monstey_Dot(b, Monstey_At3(bitangent, i)) < 0)
                for (auto &x : b)
                    x = -x;
            Monstey_Set3(tangent, i, t);
            Monstey_Set3(bitangent, i, b);
        }
    }
    if (attrs.contains("_SOW_BITANGENT"))
    {
        bitangent = channel("_SOW_BITANGENT", 3);
        for (std::size_t i = 0; i < n; ++i)
        {
            auto b = Monstey_Vector(world, Monstey_At3(bitangent, i));
            Monstey_Set3(bitangent, i, nativeMode ? b : Monstey_Unit(b));
        }
    }
    out.channels["TANGENT"] = tangent;
    out.channels["BITANGENT"] = bitangent;
    for (unsigned k = 0; k < 8; ++k)
    {
        const auto key = "COLOR_" + std::to_string(k);
        if (attrs.contains(key))
        {
            const auto c = glb.Accessor(static_cast<std::size_t>(attrs[key].as_uint()));
            Monstey_MRequire((c.width == 3 || c.width == 4) && c.v.size() == n * c.width,
                             "GLB colour dimensions invalid");
            Monstey_Channel result{4, std::vector<double>(n * 4, 255)};
            for (std::size_t i = 0; i < n; ++i)
                for (unsigned j = 0; j < c.width; ++j)
                {
                    Monstey_MRequire(c.v[i * c.width + j] >= 0 && c.v[i * c.width + j] <= 1,
                                     "GLB colour must be normalized");
                    result.v[i * 4 + j] = std::round(c.v[i * c.width + j] * 255);
                }
            out.channels["COLOR" + std::to_string(k)] = std::move(result);
        }
    }
    if (!out.channels.contains("COLOR0"))
        out.channels["COLOR0"] = {4, std::vector<double>(n * 4, 255)};
    if (nativeMode)
    {
        bool identity = true;
        for (unsigned r = 0; r < 3; ++r)
            for (unsigned c = 0; c < 3; ++c)
                if (std::abs(world[r * 4 + c] - (r == c ? 1.0 : 0.0)) > 1e-12)
                    identity = false;
        if (identity)
            for (const auto &[name, fallback] : std::array<std::pair<const char *, Monstey_V3>, 3>{
                     {{"NORMAL", {0, 0, 1}}, {"TANGENT", {1, 0, 0}}, {"BITANGENT", {0, 1, 0}}}})
            {
                const auto prefix = std::string("_SOW_") + name + "_BITS_", lowKey = prefix + "LO",
                           highKey = prefix + "HI";
                if (!attrs.contains(lowKey) && !attrs.contains(highKey))
                    continue;
                Monstey_MRequire(attrs.contains(lowKey) && attrs.contains(highKey), "Incomplete raw directional words");
                const auto low = channel(lowKey, 4), high = channel(highKey, 4);
                auto merged = out.channels.at(name);
                for (std::size_t i = 0; i < n; ++i)
                {
                    Monstey_V3 original{};
                    bool finite = true;
                    for (unsigned k = 0; k < 3; ++k)
                    {
                        const auto lo = low.v[i * 4 + k], hi = high.v[i * 4 + k];
                        Monstey_MRequire(lo >= 0 && lo <= 65535 && std::floor(lo) == lo && hi >= 0 && hi <= 65535 &&
                                             std::floor(hi) == hi,
                                         "Invalid native raw directional half-word");
                        original[k] = std::bit_cast<float>(static_cast<std::uint32_t>(lo) |
                                                           (static_cast<std::uint32_t>(hi) << 16));
                        finite = finite && std::isfinite(original[k]);
                    }
                    const auto expected = finite ? original : fallback;
                    if (Monstey_At3(out.channels.at(name), i) == expected)
                        Monstey_Set3(merged, i, original);
                }
                out.channels[name] = std::move(merged);
            }
    }
    auto joints = Monstey_Channel{4, std::vector<double>(n * 4, 0)}, weights = joints;
    const auto binding = options.value("binding", nativeMode ? "native" : "rigid").as_string();
    if (binding == "rigid")
    {
        const auto bone = options.value("rigid_bone", 0u).as_uint();
        Monstey_MRequire(bone < target.h[16], "Rigid bone outside target HK rig");
        for (std::size_t i = 0; i < n; ++i)
        {
            for (unsigned k = 0; k < 4; ++k)
                joints.v[i * 4 + k] = static_cast<double>(bone);
            weights.v[i * 4] = 1;
        }
    }
    else
    {
        Monstey_MRequire(binding == "mapped" || binding == "native", "Unknown binding mode");
        Monstey_MRequire(primitive.skin >= 0 && static_cast<std::size_t>(primitive.skin) < glb.doc["skins"].size(),
                         "Mapped primitive needs a skin");
        const auto &skin = glb.doc["skins"][static_cast<std::size_t>(primitive.skin)];
        const auto sourceBones = skin["joints"].size();
        Monstey_MRequire(sourceBones > 0, "Source skin has no joints");
        std::vector<Monstey_Channel> js, ws;
        for (unsigned set = 0; set < 2; ++set)
        {
            const auto jk = "JOINTS_" + std::to_string(set), wk = "WEIGHTS_" + std::to_string(set);
            if (!attrs.contains(jk) && !attrs.contains(wk))
                continue;
            Monstey_MRequire(attrs.contains(jk) && attrs.contains(wk), "Incomplete GLB skin attributes");
            const auto &ja = glb.doc["accessors"][static_cast<std::size_t>(attrs[jk].as_uint())];
            const auto jt = ja["componentType"].as_uint();
            Monstey_MRequire((jt == 5121 || jt == 5123) && !ja.value("normalized", false).as_bool(),
                             "GLB joints need unsigned integral storage");
            js.push_back(channel(jk, 4));
            ws.push_back(channel(wk, 4));
        }
        Monstey_MRequire(!js.empty(), "Mapped input lacks JOINTS_0 / WEIGHTS_0");
        for (std::size_t i = 0; i < n; ++i)
        {
            std::map<unsigned, double> totals;
            for (std::size_t set = 0; set < js.size(); ++set)
                for (unsigned k = 0; k < 4; ++k)
                {
                    const auto source = js[set].v[i * 4 + k], weight = ws[set].v[i * 4 + k];
                    Monstey_MRequire(source >= 0 && source < sourceBones && std::floor(source) == source && weight >= 0,
                                     "Skin joint or weight invalid");
                    unsigned destination = static_cast<unsigned>(source);
                    if (binding == "mapped" && weight > 1e-12)
                    {
                        Monstey_MRequire(options.contains("joint_map") &&
                                             options["joint_map"].contains(std::to_string(destination)),
                                         "Weighted source joint lacks explicit target mapping");
                        destination =
                            static_cast<unsigned>(options["joint_map"][std::to_string(destination)].as_uint());
                    }
                    Monstey_MRequire(destination < target.h[16] || weight <= 1e-12,
                                     "Mapped joint outside target HK rig");
                    if (binding == "native")
                    {
                        Monstey_MRequire(js.size() == 1 && destination < target.h[16],
                                         "Native joint data must fit target rig exactly");
                        joints.v[i * 4 + k] = destination;
                        weights.v[i * 4 + k] = weight;
                    }
                    else if (weight > 1e-12)
                        totals[destination] += weight;
                }
            if (binding != "native")
            {
                Monstey_MRequire(!totals.empty() && totals.size() <= 4,
                                 "Mapped vertex requires 1-4 unique target influences; prune explicitly");
                unsigned k = 0;
                for (const auto &[bone, weight] : totals)
                {
                    joints.v[i * 4 + k] = bone;
                    weights.v[i * 4 + k] = weight;
                    ++k;
                }
            }
        }
    }
    Monstey_QuantWeights(weights);
    out.channels["JOINTS"] = std::move(joints);
    out.channels["WEIGHTS_U8"] = std::move(weights);
    if (!out.triangle_band_topology_sha256.empty())
        out.triangle_band_topology_sha256 = Monstey_ModelTriangleTopologyHash(out.indices);
    return out;
}

void Monstey_CheckNativeSkin(const Monstey_Glb &glb, const Monstey_Rig &rig, const Monstey_GlbPrimitive &p)
{
    Monstey_MRequire(p.skin >= 0 && static_cast<std::size_t>(p.skin) < glb.doc["skins"].size(),
                     "Native mesh lost its skin");
    const auto &skin = glb.doc["skins"][static_cast<std::size_t>(p.skin)];
    Monstey_MRequire(skin["joints"].size() == rig.hkToSource.size(), "Native skin joint count changed");
    for (std::size_t hk = 0; hk < rig.hkToSource.size(); ++hk)
    {
        const auto node = skin["joints"][hk].as_uint();
        Monstey_MRequire(node < glb.world.size(), "Native skin joint node invalid");
        const auto &expected = rig.source[rig.hkToSource[hk]].global;
        const auto &actual = glb.world[static_cast<std::size_t>(node)];
        for (unsigned k = 0; k < 16; ++k)
            Monstey_MRequire(std::abs(actual[k] - expected[k]) <= 1e-7,
                             "Native skeleton bind transforms changed; retarget explicitly");
    }
    if (skin.contains("inverseBindMatrices"))
    {
        const auto bind = glb.Accessor(static_cast<std::size_t>(skin["inverseBindMatrices"].as_uint()));
        Monstey_MRequire(bind.width == 16 && bind.v.size() == rig.hkToSource.size() * 16,
                         "Native inverse bind extent mismatch");
        for (std::size_t hk = 0; hk < rig.hkToSource.size(); ++hk)
        {
            const auto expected = Monstey_Inv(rig.source[rig.hkToSource[hk]].global);
            for (unsigned c = 0; c < 4; ++c)
                for (unsigned r = 0; r < 4; ++r)
                    Monstey_MRequire(std::abs(bind.v[hk * 16 + c * 4 + r] - expected[r * 4 + c]) <= 1e-5,
                                     "Native inverse bind matrices changed");
        }
    }
}

} // namespace Monstey_ModelPrivate
using namespace Monstey_ModelPrivate;

Monstey_Json Monstey_ModelConvert(const std::filesystem::path &source, const std::filesystem::path &nativeTemplate,
                                  const std::filesystem::path &output, const Monstey_J &options)
{
    const std::array<std::filesystem::path, 2> sources = {source, nativeTemplate};
    Monstey_RequireOfflineOutput(output, sources);
    Monstey_MRequire(!std::filesystem::exists(output), "Output already exists");
    auto templateBytes = Monstey_ReadFile(nativeTemplate);
    if (options.contains("expected_template_sha256") &&
        options["expected_template_sha256"].as_string() != Monstey_HashSha256(templateBytes))
        throw Monstey_Error("source_hash_mismatch", "Native template snapshot differs from expected_template_sha256");
    auto sourceBytes = Monstey_ReadFile(source);
    if (options.contains("expected_source_sha256") &&
        options["expected_source_sha256"].as_string() != Monstey_HashSha256(sourceBytes))
        throw Monstey_Error("source_hash_mismatch", "GLB source snapshot differs from expected_source_sha256");
    const auto m = Monstey_LoadModel(std::move(templateBytes));
    Monstey_MRequire(Monstey_Allocation(m)["safe"].as_bool(), "Unsafe native template allocations");
    const Monstey_Glb glb(std::move(sourceBytes));
    const auto binding = options.value("binding", "rigid").as_string();
    const bool nativeMode = binding == "native";
    const bool preflight = options.value("preflight", false).as_bool();
    if (options.contains("primitive"))
    {
        Monstey_MRequire(!nativeMode, "Native mode requires all part identities; primitive selection is only "
                                      "available for rigid/mapped conversion");
        Monstey_MRequire(options["primitive"].as_uint() < glb.primitives.size(),
                         "Requested primitive index outside GLB scene");
    }
    std::map<std::size_t, std::vector<Monstey_Part>> replacements;
    bool pristine = nativeMode;
    const auto original = nativeMode ? Monstey_DecodeParts(m) : std::vector<Monstey_Part>{};
    const auto rig = nativeMode ? Monstey_ReadModelRig(m, {}) : Monstey_Rig{};
    std::set<std::pair<std::size_t, std::size_t>> seen;
    std::set<std::size_t> changedContainers;
    if (nativeMode)
    {
        const auto expected = Monstey_HashSha256(m.raw);
        const bool hasMetadata = glb.doc.contains("extras") && glb.doc["extras"].contains("monstey_native");
        if (hasMetadata)
        {
            const auto &metadata = glb.doc["extras"]["monstey_native"];
            Monstey_MRequire(metadata["template_sha256"].as_string() == expected,
                             "Native GLB does not match explicit template SHA256");
            if (metadata.contains("native_bufferView"))
            {
                const auto embedded = glb.View(static_cast<std::size_t>(metadata["native_bufferView"].as_uint()));
                Monstey_MRequire(Monstey_HashSha256(embedded) == expected,
                                 "Embedded native baseline does not match explicit template");
            }
        }
        else
            Monstey_MRequire(options.contains("template_sha256") && options["template_sha256"].as_string() == expected,
                             "Metadata-free native import requires explicit template_sha256");
        if (options.contains("template_sha256"))
            Monstey_MRequire(options["template_sha256"].as_string() == expected,
                             "Explicit native template hash mismatch");
    }
    std::size_t primitiveIndex = 0;
    for (const auto &primitive : glb.primitives)
    {
        if (options.contains("primitive") && primitiveIndex != options["primitive"].as_uint())
        {
            ++primitiveIndex;
            continue;
        }
        auto part = Monstey_FromGlb(glb, primitive, m, options, nativeMode);
        std::size_t container = static_cast<std::size_t>(options.value("container", 0u).as_uint());
        if (nativeMode)
        {
            Monstey_CheckNativeSkin(glb, rig, primitive);
            if (primitive.definition.contains("extras") &&
                primitive.definition["extras"].contains("monstey_container") &&
                primitive.definition["extras"].contains("monstey_part"))
            {
                container = static_cast<std::size_t>(primitive.definition["extras"]["monstey_container"].as_uint());
                part.part = static_cast<std::size_t>(primitive.definition["extras"]["monstey_part"].as_uint());
            }
            else if (options.contains("native_part_map"))
            {
                Monstey_MRequire(options["native_part_map"].size() == glb.primitives.size(),
                                 "Native part map must cover every source primitive");
                const auto &identity = options["native_part_map"][primitiveIndex];
                container = static_cast<std::size_t>(identity["container"].as_uint());
                part.part = static_cast<std::size_t>(identity["part"].as_uint());
            }
            else
            {
                Monstey_MRequire(original.size() == 1, "Metadata-free multi-part import needs native_part_map");
                container = original[0].container;
                part.part = original[0].part;
            }
            Monstey_MRequire(seen.insert({container, part.part}).second, "Duplicate native container/part identity");
            const auto found = std::find_if(original.begin(), original.end(), [&](const auto &p)
                                            { return p.container == container && p.part == part.part; });
            Monstey_MRequire(found != original.end(), "Native GLB part has no template donor");
            if (options.value("restore_nonfinite", true).as_bool())
                for (const auto &[name, fallback] : std::array<std::pair<const char *, Monstey_V3>, 3>{
                         {{"NORMAL", {0, 0, 1}}, {"TANGENT", {1, 0, 0}}, {"BITANGENT", {0, 1, 0}}}})
                {
                    auto &actual = part.channels.at(name);
                    const auto &baseline = found->channels.at(name);
                    if (actual.v.size() != baseline.v.size())
                        continue;
                    for (std::size_t i = 0; i < part.Count(); ++i)
                    {
                        const auto old = Monstey_At3(baseline, i);
                        if (std::any_of(old.begin(), old.end(), [](double v) { return !std::isfinite(v); }) &&
                            Monstey_At3(actual, i) == fallback)
                            Monstey_Set3(actual, i, old);
                    }
                }
            if (!primitive.definition["attributes"].contains("_SOW_BITANGENT") &&
                primitive.definition["attributes"].contains("TANGENT"))
            {
                bool sameBasis = part.Count() == found->Count();
                for (const char *channelName : {"NORMAL", "TANGENT"})
                {
                    const auto &a = part.channels.at(channelName);
                    const auto &b = found->channels.at(channelName);
                    if (a.v.size() != b.v.size())
                    {
                        sameBasis = false;
                        continue;
                    }
                    for (std::size_t i = 0; i < a.v.size(); ++i)
                        if (std::bit_cast<std::uint32_t>(static_cast<float>(a.v[i])) !=
                            std::bit_cast<std::uint32_t>(static_cast<float>(b.v[i])))
                            sameBasis = false;
                }
                const auto tangent =
                    glb.Accessor(static_cast<std::size_t>(primitive.definition["attributes"]["TANGENT"].as_uint()));
                if (tangent.width != 4 || tangent.v.size() != found->Count() * 4)
                    sameBasis = false;
                if (sameBasis)
                    for (std::size_t i = 0; i < found->Count(); ++i)
                    {
                        const auto t = Monstey_At3(found->channels.at("TANGENT"), i),
                                   n = Monstey_At3(found->channels.at("NORMAL"), i),
                                   b = Monstey_At3(found->channels.at("BITANGENT"), i);
                        const double sign = Monstey_Dot(Monstey_Cross(n, t), b) < 0 ? -1 : 1;
                        if (tangent.v[i * 4 + 3] != sign)
                            sameBasis = false;
                    }
                if (sameBasis)
                    part.channels["BITANGENT"] = found->channels.at("BITANGENT");
            }
            Monstey_ResolvePartBands(part, *found, options);
            const bool equivalent = Monstey_NativeEquivalent(part, *found);
            pristine = pristine && equivalent;
            if (!equivalent)
                changedContainers.insert(container);
        }
        ++primitiveIndex;
        Monstey_MRequire(container < m.lods.size(), "Requested container outside native template");
        part.container = container;
        replacements[container].push_back(std::move(part));
    }
    if (nativeMode)
        Monstey_MRequire(seen.size() == original.size(), "Native GLB lost template draws; hide or replace explicitly");
    if (nativeMode)
        for (auto it = replacements.begin(); it != replacements.end();)
        {
            if (!changedContainers.contains(it->first))
                it = replacements.erase(it);
            else
                ++it;
        }
    Monstey_Bytes encoded = pristine ? m.raw : Monstey_RebuildModel(m, replacements, nativeMode, options);
    const auto parsed = Monstey_LoadModel(encoded);
    Monstey_MRequire(Monstey_Allocation(parsed)["safe"].as_bool(), "Output native allocation check failed");
    if (!preflight)
        Monstey_WriteNewFile(output, encoded);
    auto changed = Monstey_J::array();
    for (const auto &[container, parts] : replacements)
    {
        (void)parts;
        changed.as_array().push_back(static_cast<std::uint64_t>(container));
    }
    return Monstey_J::object(
        {{"output", Monstey_Utf8(output.wstring())},
         {"source_sha256", Monstey_HashSha256(glb.raw)},
         {"template_sha256", Monstey_HashSha256(m.raw)},
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
         {"byte_exact_noop", pristine},
         {"preflight", preflight},
         {"published", !preflight},
         {"validated_bytes", static_cast<std::uint64_t>(encoded.size())},
         {"selected_primitive", options.value("primitive", nullptr)},
         {"binding", binding},
         {"containers", changed},
         {"allocation", Monstey_Allocation(parsed)},
         {"native_material_groups_bones_lod_bindings_preserved", true},
         {"native_material_conversion", false},
         {"source_bind_pose_retargeted", false},
         {"runtime_verified", false}});
}

} // namespace Monstey
