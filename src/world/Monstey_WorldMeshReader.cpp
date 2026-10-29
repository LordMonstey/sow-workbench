#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

std::pair<unsigned, unsigned> Monstey_WMesh::Storage(unsigned kind)
{
    switch (kind)
    {
    case 1:
        return {4, 2};
    case 2:
        return {4, 3};
    case 3:
        return {4, 4};
    case 4:
        return {1, 4};
    case 9:
        return {2, 2};
    case 10:
        return {2, 2};
    case 12:
        return {2, 4};
    default:
        throw Monstey_Error("world_mesh_storage", "Unknown native MESH5 vertex storage");
    }
}

std::string Monstey_WMesh::Semantic(unsigned code, unsigned channel)
{
    static constexpr const char *names[] = {"POSITION", "NORMAL", "TANGENT",        "BINORMAL",
                                            "UV",       "COLOR",  "WEIGHTS_RAW_U8", "PALETTE_INDICES"};
    auto name = code < 8 ? std::string(names[code]) : "SEMANTIC_" + std::to_string(code);
    if (code == 4 || code == 5 || channel)
        name += std::to_string(channel);
    return name;
}

Monstey_WMesh::Monstey_WMesh(B input) : raw(std::move(input))
{
    Monstey_Require(raw.size() >= 32 && std::memcmp(raw.data(), "MESH", 4) == 0 && Monstey_U32(raw, 4) == 5,
                    "Expected MESH5 world geometry, not MMSH17");
    for (std::size_t j = 0; j < 8; ++j)
        header[j] = Monstey_U32(raw, 4 * j);
    Monstey_CheckRange(raw, 32, header[7]);
    std::size_t at = 32 + header[7];
    std::uint64_t draws{}, layouts{}, attrs{}, allocation{};
    auto take = [&](std::size_t n)
    {
        Monstey_CheckRange(raw, at, n);
        auto p = at;
        at += n;
        return p;
    };
    auto u32 = [&]() { return Monstey_U32(raw, take(4)); };
    Monstey_Require(header[2] <= (raw.size() - at) / 20, "MESH5 groups outside extent");
    for (std::uint32_t j = 0; j < header[2]; ++j)
    {
        Group g;
        g.start = at;
        auto declared = u32();
        g.vertex_bytes = u32();
        g.index_bytes = u32();
        g.vertex = take(g.vertex_bytes);
        g.index = take(g.index_bytes);
        auto nl = u32();
        Monstey_Require(nl <= (raw.size() - at) / 4, "MESH5 layouts outside extent");
        for (std::uint32_t k = 0; k < nl; ++k)
        {
            auto n = u32();
            Monstey_Require(n > 0 && n <= 256, "MESH5 attribute count invalid");
            std::vector<std::array<std::uint8_t, 4>> layout;
            for (std::uint32_t a = 0; a < n; ++a)
            {
                auto p = take(4);
                Monstey_Require(raw[p] <= 1, "MESH5 unknown vertex stream");
                (void)Storage(raw[p + 1]);
                layout.push_back({raw[p], raw[p + 1], raw[p + 2], raw[p + 3]});
            }
            attrs += n;
            g.layouts.push_back(std::move(layout));
        }
        auto nd = u32();
        Monstey_Require(nd == declared && nd <= (raw.size() - at) / 52, "MESH5 draw count invalid");
        for (std::uint32_t d = 0; d < nd; ++d)
        {
            std::array<std::uint32_t, 13> row;
            auto p = take(52);
            for (std::size_t w = 0; w < 13; ++w)
                row[w] = Monstey_U32(raw, p + 4 * w);
            g.draws.push_back(row);
        }
        g.end = at;
        draws += nd;
        layouts += nl;
        allocation += ((g.vertex_bytes + 3) & ~std::size_t(3)) + ((g.index_bytes + 3) & ~std::size_t(3));
        groups.push_back(std::move(g));
    }
    Monstey_Require(at == raw.size() && draws == header[3] && layouts == header[4] && attrs == header[5] &&
                        allocation == header[6],
                    "MESH5 metadata totals or final extent differs");
    for (std::size_t g = 0; g < groups.size(); ++g)
        for (std::size_t d = 0; d < groups[g].draws.size(); ++d)
        {
            (void)Attributes(g, d);
            (void)Indices(g, d);
        }
}

std::map<std::string, Monstey_WMesh::Attr> Monstey_WMesh::Attributes(std::size_t gi, std::size_t di) const
{
    Monstey_Require(gi < groups.size() && di < groups[gi].draws.size(), "MESH5 group/draw index outside table");
    auto &g = groups[gi];
    auto &w = g.draws[di];
    auto count = w[1] & 65535, li = w[9] & 255;
    Monstey_Require(count > 0 && li < g.layouts.size(), "MESH5 vertex count/layout invalid");
    auto &layout = g.layouts[li];
    std::size_t total{};
    for (auto &e : layout)
    {
        auto [unit, width] = Storage(e[1]);
        total += unit * width;
    }
    auto relative = std::uint64_t(w[4]) * total;
    Monstey_Require(relative + std::uint64_t(count) * total == w[10] && w[10] <= g.vertex_bytes,
                    "MESH5 vertex extent differs from draw");
    std::map<std::string, Attr> result;
    for (unsigned stream = 0; stream < 2; ++stream)
    {
        std::size_t stride{}, offset{};
        for (auto &e : layout)
            if (e[0] == stream)
            {
                auto [unit, width] = Storage(e[1]);
                stride += unit * width;
            }
        for (auto &e : layout)
            if (e[0] == stream)
            {
                auto [unit, width] = Storage(e[1]);
                auto name = Semantic(e[2], e[3]);
                Monstey_Require(result
                                    .emplace(name, Attr{g.vertex + static_cast<std::size_t>(relative) + offset, stride,
                                                        count, e[1], e[2], e[3], static_cast<std::uint8_t>(width),
                                                        static_cast<std::uint8_t>(unit)})
                                    .second,
                                "Duplicate MESH5 semantic/channel");
                offset += unit * width;
            }
        relative += std::uint64_t(count) * stride;
    }
    return result;
}

std::vector<std::uint16_t> Monstey_WMesh::Indices(std::size_t gi, std::size_t di) const
{
    auto &g = groups.at(gi);
    auto &w = g.draws.at(di);
    auto count = 3ull * (w[5] & 0xffffffu), relative = 2ull * w[3];
    Monstey_Require(relative <= g.index_bytes && count * 2 <= g.index_bytes - relative, "MESH5 indices outside buffer");
    std::vector<std::uint16_t> out;
    out.reserve(static_cast<std::size_t>(count));
    for (std::size_t j = 0; j < count; ++j)
    {
        auto value = Monstey_U16(raw, g.index + relative + 2 * j);
        Monstey_Require(value < (w[1] & 65535), "MESH5 triangle outside vertex range");
        out.push_back(value);
    }
    return out;
}

double Monstey_WMesh::Component(const Attr &a, std::size_t vertex, std::size_t component, const B *source) const
{
    auto &data = source ? *source : raw;
    auto p = a.offset + vertex * a.stride + component * a.unit;
    switch (a.storage)
    {
    case 1:
    case 2:
    case 3:
        return Monstey_WReadFloat(data, p);
    case 4:
        return data[p];
    case 9:
        return static_cast<std::int16_t>(Monstey_U16(data, p));
    case 10:
    case 12:
        return Monstey_WHalf(Monstey_U16(data, p));
    default:
        throw Monstey_Error("world_mesh_storage", "Unsupported component");
    }
}

J Monstey_WMesh::Report() const
{
    J::Array rows;
    for (std::size_t gi = 0; gi < groups.size(); ++gi)
    {
        auto &g = groups[gi];
        J::Array draws;
        for (std::size_t di = 0; di < g.draws.size(); ++di)
        {
            auto attributes = Attributes(gi, di);
            J::Object attrs;
            for (auto &[name, a] : attributes)
                attrs.emplace(name, J::object({{"offset", static_cast<std::uint64_t>(a.offset)},
                                               {"stride", static_cast<std::uint64_t>(a.stride)},
                                               {"count", a.count},
                                               {"width", a.width},
                                               {"kind", a.storage},
                                               {"semantic", a.semantic},
                                               {"channel", a.channel}}));
            J lo, hi;
            if (auto p = attributes.find("POSITION");
                p != attributes.end() && p->second.width >= 3 && !Indices(gi, di).empty())
            {
                J::Array lower, upper;
                for (std::size_t k = 0; k < 3; ++k)
                {
                    double mn = std::numeric_limits<double>::infinity(), mx = -mn;
                    for (auto v : Indices(gi, di))
                    {
                        auto value = Component(p->second, v, k);
                        Monstey_Require(std::isfinite(value), "Nonfinite world position");
                        mn = std::min(mn, value);
                        mx = std::max(mx, value);
                    }
                    lower.push_back(mn);
                    upper.push_back(mx);
                }
                lo = lower;
                hi = upper;
            }
            J::Array aliases;
            for (std::size_t other = 0; other < g.draws.size(); ++other)
                if (Attributes(gi, other) == attributes)
                    aliases.push_back(static_cast<std::uint64_t>(other));
            draws.push_back(J::object({{"index", static_cast<std::uint64_t>(di)},
                                       {"vertices", g.draws[di][1] & 65535},
                                       {"triangles", g.draws[di][5] & 0xffffffu},
                                       {"layout_index", g.draws[di][9] & 255},
                                       {"material_slot_raw", g.draws[di][6] & 65535},
                                       {"attributes", attrs},
                                       {"shared_vertex_draws", aliases},
                                       {"bounds_min", lo},
                                       {"bounds_max", hi}}));
        }
        rows.push_back(J::object({{"index", static_cast<std::uint64_t>(gi)},
                                  {"vertex_bytes", static_cast<std::uint64_t>(g.vertex_bytes)},
                                  {"index_bytes", static_cast<std::uint64_t>(g.index_bytes)},
                                  {"draws", draws}}));
    }
    return J::object({{"format", "MESH5"},
                      {"bytes", static_cast<std::uint64_t>(raw.size())},
                      {"sha256", Monstey_HashSha256(raw)},
                      {"groups", rows},
                      {"dictionary_preserved", true},
                      {"raw_encoded_attributes", true},
                      {"geometry_decoder", true},
                      {"material_paths_resolved", false},
                      {"collision_or_navigation_updated", false},
                      {"runtime_verified", false}});
}

B Monstey_WMesh::Export(std::size_t gi, std::size_t di) const
{
    auto attrs = Attributes(gi, di);
    std::map<std::string, B> entries;
    for (auto &[name, a] : attrs)
    {
        B bytes;
        Monstey_Require(std::uint64_t(a.count) * a.width * a.unit <= 256ull * 1024 * 1024,
                        "Attribute export exceeds limit");
        bytes.reserve(std::size_t(a.count) * a.width * a.unit);
        for (std::size_t row = 0; row < a.count; ++row)
        {
            auto at = a.offset + row * a.stride;
            Monstey_CheckRange(raw, at, a.width * a.unit);
            bytes.insert(bytes.end(), raw.begin() + static_cast<std::ptrdiff_t>(at),
                         raw.begin() + static_cast<std::ptrdiff_t>(at + a.width * a.unit));
        }
        auto dtype = a.storage == 4 ? "|u1" : a.storage == 9 ? "<i2" : a.unit == 2 ? "<f2" : "<f4";
        entries.emplace(name, Monstey_WNpyEncode(dtype, a.count, a.width, bytes));
    }
    B indices;
    auto faces = Indices(gi, di);
    for (auto i : faces)
        Monstey_AppendU16(indices, i);
    entries.emplace("INDICES", Monstey_WNpyEncode("<u2", faces.size() / 3, 3, indices));
    auto npz = Monstey_WNpzEncode(entries);
    auto check = Monstey_WNpzRead(npz);
    Monstey_Require(check.size() == attrs.size() + 1, "NPZ attribute readback count differs");
    for (auto &[name, a] : attrs)
    {
        auto &matrix = check.at(name);
        Monstey_Require(matrix.count == a.count && matrix.width == a.width && matrix.unit == a.unit,
                        "NPZ native matrix shape changed");
        for (std::size_t row = 0; row < a.count; ++row)
            Monstey_Require(std::equal(matrix.data.begin() + static_cast<std::ptrdiff_t>(row * a.width * a.unit),
                                       matrix.data.begin() + static_cast<std::ptrdiff_t>((row + 1) * a.width * a.unit),
                                       raw.begin() + static_cast<std::ptrdiff_t>(a.offset + row * a.stride)),
                            "NPZ raw attribute bytes differ");
    }
    Monstey_Require(check.at("INDICES").data == indices, "NPZ indices changed");
    return npz;
}

} // namespace Monstey_WorldPrivate
using namespace Monstey_WorldPrivate;

Monstey_WorldRawMesh Monstey_WorldMeshReadContexts(std::span<const std::uint8_t> raw)
{
    Monstey_WMesh mesh(B(raw.begin(), raw.end()));
    Monstey_WorldRawMesh result;
    result.header = mesh.header;
    result.opaque_header = B(raw.begin() + 32, raw.begin() + static_cast<std::ptrdiff_t>(32ull + mesh.header[7]));
    for (auto &group : mesh.groups)
    {
        Monstey_WorldRawMeshContext context;
        context.vertex_buffer = B(mesh.raw.begin() + static_cast<std::ptrdiff_t>(group.vertex),
                                  mesh.raw.begin() + static_cast<std::ptrdiff_t>(group.vertex + group.vertex_bytes));
        context.index_buffer = B(mesh.raw.begin() + static_cast<std::ptrdiff_t>(group.index),
                                 mesh.raw.begin() + static_cast<std::ptrdiff_t>(group.index + group.index_bytes));
        context.layouts = group.layouts;
        context.draws = group.draws;
        for (auto &layout : group.layouts)
        {
            std::size_t stride = 0;
            for (auto &attribute : layout)
            {
                auto storage = Monstey_WMesh::Storage(attribute[1]);
                stride += std::size_t(storage.first) * storage.second;
            }
            context.layout_strides.push_back(stride);
        }
        result.groups.push_back(std::move(context));
    }
    return result;
}

Monstey_Json Monstey_WorldMeshDecodeGeometry(std::span<const std::uint8_t> raw, std::size_t group,
                                             std::size_t draw_index)
{
    Monstey_WMesh mesh(B(raw.begin(), raw.end()));
    Monstey_Require(group < mesh.groups.size() && draw_index < mesh.groups[group].draws.size(),
                    "Native geometry draw is outside its group");
    auto attributes = mesh.Attributes(group, draw_index);
    J::Object channels;
    for (auto &[name, attribute] : attributes)
    {
        J::Array rows;
        for (std::size_t vertex = 0; vertex < attribute.count; ++vertex)
        {
            J::Array values;
            for (std::size_t axis = 0; axis < attribute.width; ++axis)
            {
                auto value = mesh.Component(attribute, vertex, axis);
                Monstey_Require(std::isfinite(value), "Nonfinite native world geometry component");
                if (attribute.storage == 4 || attribute.storage == 9)
                    values.push_back(static_cast<std::int64_t>(value));
                else
                    values.push_back(value);
            }
            rows.push_back(std::move(values));
        }
        channels.emplace(name, J(std::move(rows)));
    }
    auto indices = mesh.Indices(group, draw_index);
    J::Array triangles;
    for (std::size_t i = 0; i < indices.size(); i += 3)
        triangles.push_back(J::array({indices[i], indices[i + 1], indices[i + 2]}));
    auto &draw = mesh.groups[group].draws[draw_index];
    channels.emplace("INDICES", J(std::move(triangles)));
    channels.emplace("material_index", draw[6] & 65535);
    channels.emplace("draw_parameter_words", J::array({draw[11], draw[12]}));
    return channels;
}

} // namespace Monstey
