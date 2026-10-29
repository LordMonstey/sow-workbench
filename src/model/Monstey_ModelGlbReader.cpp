#include "Monstey_ModelPrivate.hpp"

namespace Monstey
{
namespace Monstey_ModelPrivate
{

Monstey_GlbEnvelope::Monstey_GlbEnvelope(Monstey_Bytes data) : raw(std::move(data))
{
    Monstey_MRequire(Monstey_U32(raw, 0) == 0x46546c67 && Monstey_U32(raw, 4) == 2 && Monstey_U32(raw, 8) == raw.size(),
                     "Expected complete GLB2");
    bool json = false, binary = false;
    std::size_t p = 12;
    while (p < raw.size())
    {
        const auto bytes = Monstey_U32(raw, p), kind = Monstey_U32(raw, p + 4);
        p += 8;
        Monstey_MRequire(bytes % 4 == 0, "GLB chunk alignment invalid");
        Monstey_CheckRange(raw, p, bytes);
        if (kind == 0x4e4f534a)
        {
            Monstey_MRequire(!json && p == 20, "First and only GLB JSON chunk required");
            json = true;
            std::string text(reinterpret_cast<const char *>(raw.data() + p), bytes);
            while (!text.empty() && (text.back() == ' ' || text.back() == 0))
                text.pop_back();
            doc = Monstey_J::Parse(text);
        }
        else if (kind == 0x004e4942)
        {
            Monstey_MRequire(!binary && json, "Duplicate or premature GLB BIN");
            binary = true;
            bin.assign(raw.begin() + static_cast<std::ptrdiff_t>(p),
                       raw.begin() + static_cast<std::ptrdiff_t>(p + bytes));
        }
        p += bytes;
    }
    Monstey_MRequire(json && doc["asset"]["version"].as_string() == "2.0", "GLTF asset.version must be 2.0");
}

Monstey_Glb::Monstey_Glb(Monstey_Bytes data) : Monstey_GlbEnvelope(std::move(data))
{
    if (doc.contains("extensionsRequired"))
        for (const auto &x : doc["extensionsRequired"].as_array())
            Monstey_MRequire(x.as_string() == "KHR_mesh_quantization", "Required glTF extension is unsupported");
    Monstey_MRequire(doc["buffers"].size() == 1 && !doc["buffers"][0u].contains("uri"),
                     "Use one embedded GLB BIN buffer");
    const auto declared = doc["buffers"][0u]["byteLength"].as_uint();
    Monstey_MRequire(declared <= bin.size() && bin.size() - declared <= 3, "GLB declared buffer extent mismatch");
    Monstey_MRequire(doc.contains("nodes") && doc.contains("meshes") && doc.contains("scenes") &&
                         !doc["scenes"].empty(),
                     "GLB scene is required");
    world.resize(doc["nodes"].size(), Monstey_Identity());
    std::vector<bool> active(world.size(), false), seen(world.size(), false);
    const auto scene = doc.value("scene", 0u).as_uint();
    Monstey_MRequire(scene < doc["scenes"].size(), "GLB scene index invalid");
    const auto visit = [&](auto &&self, std::size_t index, const Monstey_M4 &parent) -> void
    {
        Monstey_MRequire(index < world.size() && !active[index] && !seen[index],
                         "GLB node cycle, duplicate parent or invalid index");
        active[index] = seen[index] = true;
        const auto &node = doc["nodes"][index];
        world[index] = Monstey_Mul(parent, Transform(node));
        if (node.contains("mesh"))
        {
            const auto mi = node["mesh"].as_uint();
            Monstey_MRequire(mi < doc["meshes"].size(), "GLB mesh index invalid");
            const auto &mesh = doc["meshes"][static_cast<std::size_t>(mi)];
            std::size_t pi = 0;
            for (const auto &primitive : mesh["primitives"].as_array())
            {
                Monstey_MRequire(primitive.value("mode", 4u).as_uint() == 4,
                                 "Only authored triangle primitives are supported");
                Monstey_MRequire(!primitive.contains("targets") || primitive["targets"].empty(),
                                 "Bake morph targets before native conversion");
                Monstey_GlbPrimitive item;
                item.node = index;
                item.mesh = static_cast<std::size_t>(mi);
                item.primitive = pi++;
                item.skin = node.contains("skin") ? static_cast<int>(node["skin"].as_uint()) : -1;
                item.world = world[index];
                item.definition = primitive;
                primitives.push_back(item);
            }
        }
        if (node.contains("children"))
            for (const auto &child : node["children"].as_array())
                self(self, static_cast<std::size_t>(child.as_uint()), world[index]);
        active[index] = false;
    };
    for (const auto &n : doc["scenes"][static_cast<std::size_t>(scene)]["nodes"].as_array())
        visit(visit, static_cast<std::size_t>(n.as_uint()), Monstey_Identity());
    Monstey_MRequire(!primitives.empty(), "GLB scene has no triangle meshes");
}

Monstey_Span Monstey_Glb::View(std::size_t i) const
{
    const auto &v = doc["bufferViews"][i];
    Monstey_MRequire(v["buffer"].as_uint() == 0, "External GLB buffers unsupported");
    if (v.contains("extensions"))
        Monstey_MRequire(!v["extensions"].contains("EXT_meshopt_compression"), "Compressed GLB view unsupported");
    const auto off = v.value("byteOffset", 0u).as_uint(), len = v["byteLength"].as_uint();
    Monstey_CheckRange(bin, static_cast<std::size_t>(off), static_cast<std::size_t>(len));
    Monstey_MRequire(off + len <= doc["buffers"][0u]["byteLength"].as_uint(), "GLB view exceeds declared buffer");
    return Monstey_Span(bin).subspan(static_cast<std::size_t>(off), static_cast<std::size_t>(len));
}

unsigned Monstey_Glb::Width(std::string_view type)
{
    if (type == "SCALAR")
        return 1;
    if (type == "VEC2")
        return 2;
    if (type == "VEC3")
        return 3;
    if (type == "VEC4")
        return 4;
    if (type == "MAT4")
        return 16;
    throw Monstey_Error("model_accessor", "Unknown GLB accessor shape");
}

std::size_t Monstey_Glb::ComponentBytes(std::uint64_t type)
{
    switch (type)
    {
    case 5120:
    case 5121:
        return 1;
    case 5122:
    case 5123:
        return 2;
    case 5125:
    case 5126:
        return 4;
    default:
        throw Monstey_Error("model_accessor", "Unknown GLB component type");
    }
}

double Monstey_Glb::Component(Monstey_Span b, std::size_t p, std::uint64_t type)
{
    Monstey_CheckRange(b, p, ComponentBytes(type));
    switch (type)
    {
    case 5120:
        return static_cast<std::int8_t>(b[p]);
    case 5121:
        return b[p];
    case 5122:
        return static_cast<std::int16_t>(Monstey_U16(b, p));
    case 5123:
        return Monstey_U16(b, p);
    case 5125:
        return Monstey_U32(b, p);
    case 5126:
        return Monstey_F32(b, p);
    default:
        return 0;
    }
}

Monstey_Channel Monstey_Glb::Accessor(std::size_t index) const
{
    const auto &a = doc["accessors"][index];
    const auto type = a["componentType"].as_uint(), count = a["count"].as_uint();
    const auto width = Width(a["type"].as_string());
    const auto cs = ComponentBytes(type), element = cs * width;
    Monstey_MRequire(count > 0 && count <= 8000000 && count * width <= 64000000, "GLB accessor count exceeds limits");
    const auto offset = a.value("byteOffset", 0u).as_uint();
    Monstey_MRequire(offset % cs == 0, "GLB accessor alignment invalid");
    Monstey_Channel result;
    result.width = width;
    result.v.resize(static_cast<std::size_t>(count) * width, 0);
    if (a.contains("bufferView"))
    {
        const auto vi = static_cast<std::size_t>(a["bufferView"].as_uint());
        const auto b = View(vi);
        const auto stride = doc["bufferViews"][vi].value("byteStride", static_cast<std::uint64_t>(element)).as_uint();
        Monstey_MRequire(stride >= element && stride <= 252 && stride % cs == 0, "GLB accessor stride invalid");
        Monstey_CheckRange(b, static_cast<std::size_t>(offset),
                           static_cast<std::size_t>((count - 1) * stride + element));
        for (std::size_t i = 0; i < count; ++i)
            for (unsigned k = 0; k < width; ++k)
                result.v[i * width + k] = Component(b, static_cast<std::size_t>(offset + i * stride + k * cs), type);
    }
    else
        Monstey_MRequire(offset == 0, "GLB accessor without view has byte offset");
    if (a.contains("sparse"))
    {
        const auto &s = a["sparse"];
        const auto n = s["count"].as_uint();
        Monstey_MRequire(n > 0 && n <= count, "GLB sparse count invalid");
        const auto it = s["indices"]["componentType"].as_uint();
        Monstey_MRequire(it == 5121 || it == 5123 || it == 5125, "GLB sparse index storage invalid");
        const auto ids = View(static_cast<std::size_t>(s["indices"]["bufferView"].as_uint())),
                   values = View(static_cast<std::size_t>(s["values"]["bufferView"].as_uint()));
        const auto io = s["indices"].value("byteOffset", 0u).as_uint(),
                   vo = s["values"].value("byteOffset", 0u).as_uint(), is = ComponentBytes(it);
        Monstey_MRequire(io % is == 0 && vo % cs == 0, "GLB sparse alignment invalid");
        Monstey_CheckRange(ids, static_cast<std::size_t>(io), static_cast<std::size_t>(n * is));
        Monstey_CheckRange(values, static_cast<std::size_t>(vo), static_cast<std::size_t>(n * element));
        std::uint64_t previous = 0;
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto at = static_cast<std::uint64_t>(Component(ids, static_cast<std::size_t>(io + i * is), it));
            Monstey_MRequire(at < count && (i == 0 || at > previous), "GLB sparse indices not strictly increasing");
            previous = at;
            for (unsigned k = 0; k < width; ++k)
                result.v[static_cast<std::size_t>(at) * width + k] =
                    Component(values, static_cast<std::size_t>(vo + i * element + k * cs), type);
        }
    }
    if (a.value("normalized", false).as_bool())
    {
        Monstey_MRequire(type != 5125 && type != 5126, "GLB normalized storage invalid");
        const double maximum = type == 5120 ? 127 : type == 5121 ? 255 : type == 5122 ? 32767 : 65535;
        for (auto &v : result.v)
            v = std::max(type == 5120 || type == 5122 ? -1.0 : 0.0, v / maximum);
    }
    return result;
}

Monstey_M4 Monstey_Glb::Transform(const Monstey_J &n)
{
    if (n.contains("matrix"))
    {
        Monstey_MRequire(n["matrix"].size() == 16 && !n.contains("translation") && !n.contains("rotation") &&
                             !n.contains("scale"),
                         "GLB matrix/TRS conflict");
        Monstey_M4 m{};
        for (unsigned c = 0; c < 4; ++c)
            for (unsigned r = 0; r < 4; ++r)
                m[r * 4 + c] = n["matrix"][c * 4 + r].as_number();
        Monstey_MRequire(std::abs(m[12]) + std::abs(m[13]) + std::abs(m[14]) + std::abs(m[15] - 1) < 1e-9,
                         "GLB matrix is not affine");
        return m;
    }
    Monstey_V3 t = {0, 0, 0}, s = {1, 1, 1};
    std::array<double, 4> q = {0, 0, 0, 1};
    if (n.contains("translation"))
    {
        Monstey_MRequire(n["translation"].size() == 3, "GLB translation length");
        for (unsigned i = 0; i < 3; ++i)
            t[i] = n["translation"][i].as_number();
    }
    if (n.contains("scale"))
    {
        Monstey_MRequire(n["scale"].size() == 3, "GLB scale length");
        for (unsigned i = 0; i < 3; ++i)
            s[i] = n["scale"][i].as_number();
    }
    if (n.contains("rotation"))
    {
        Monstey_MRequire(n["rotation"].size() == 4, "GLB quaternion length");
        for (unsigned i = 0; i < 4; ++i)
            q[i] = n["rotation"][i].as_number();
    }
    return Monstey_Transform(t, q, s);
}

} // namespace Monstey_ModelPrivate

} // namespace Monstey
