#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

J Monstey_WMesh::ApplyNpz(const J &update, const std::map<std::string, Attr> &attrs, B &written)
{
    Monstey_Require(!update.contains("translation") && !update.contains("quaternion") && !update.contains("scale"),
                    "Choose NPZ replacement or geometric transform");
    auto path = Monstey_WPath(update.at("attributes_npz"));
    auto bytes = Monstey_ReadFile(path);
    auto checksum = Monstey_HashSha256(bytes);
    auto matrices = Monstey_WNpzRead(bytes);
    auto gi = update.at("group_index").as_uint(), di = update.at("draw_index").as_uint();
    if (auto index = matrices.find("INDICES"); index != matrices.end())
    {
        auto expected = Indices(static_cast<std::size_t>(gi), static_cast<std::size_t>(di));
        Monstey_Require(index->second.width == 3 && index->second.count == expected.size() / 3,
                        "NPZ triangle shape differs");
        for (std::size_t n = 0; n < expected.size(); ++n)
            Monstey_Require(index->second.Number(n / 3, n % 3) == expected[n],
                            "Topology edits require paired native topology/render authoring");
        matrices.erase(index);
    }
    Monstey_Require(!matrices.empty(), "NPZ has no replacement attributes");
    J::Array changed;
    for (auto &[name, matrix] : matrices)
    {
        auto it = attrs.find(name);
        Monstey_Require(it != attrs.end(), "Unknown NPZ native attribute");
        auto &a = it->second;
        Monstey_Require(matrix.count == a.count && matrix.width == a.width, "NPZ attribute count or width differs");
        for (std::size_t row = 0; row < a.count; ++row)
            for (std::size_t component = 0; component < a.width; ++component)
            {
                auto value = matrix.Number(row, component);
                Monstey_Require(std::isfinite(value), "NPZ attribute is nonfinite");
                B encoded;
                if (a.storage == 4)
                {
                    Monstey_Require(value >= 0 && value <= 255 && std::floor(value) == value,
                                    "NPZ uint8 overflow or fraction");
                    encoded.push_back(static_cast<std::uint8_t>(value));
                }
                else if (a.storage == 9)
                {
                    Monstey_Require(value >= -32768 && value <= 32767 && std::floor(value) == value,
                                    "NPZ int16 overflow or fraction");
                    Monstey_AppendU16(encoded, static_cast<std::uint16_t>(static_cast<std::int16_t>(value)));
                }
                else
                {
                    auto f = static_cast<float>(value);
                    Monstey_Require(std::isfinite(f), "NPZ float32 overflow");
                    if (a.unit == 2)
                        Monstey_AppendU16(encoded, Monstey_WPackHalf(f));
                    else
                        Monstey_AppendU32(encoded, std::bit_cast<std::uint32_t>(f));
                }
                auto at = a.offset + row * a.stride + component * a.unit;
                for (std::size_t n = 0; n < a.unit; ++n)
                {
                    Monstey_Require(!written[at + n] || raw[at + n] == encoded[n],
                                    "Contradictory aliased NPZ geometry update");
                    raw[at + n] = encoded[n];
                    written[at + n] = 1;
                }
            }
        changed.push_back(name);
    }
    Monstey_Require(Monstey_HashSha256(path) == checksum, "NPZ source changed during import");
    return J::object({{"group_index", gi},
                      {"draw_index", di},
                      {"attributes", changed},
                      {"npz_source", Monstey_WTextPath(path)},
                      {"npz_sha256", checksum}});
}

J Monstey_WMesh::Apply(const J &spec)
{
    for (auto &[key, v] : spec.as_object())
    {
        (void)v;
        Monstey_Require(key == "source_sha256" || key == "updates",
                        "New MESH5 topology requires paired native render authoring and is not yet ported");
    }
    if (spec.contains("source_sha256"))
        Monstey_Require(spec.at("source_sha256").as_string() == Monstey_HashSha256(raw), "MESH5 source hash differs");
    B written(raw.size());
    auto before = raw;
    J::Array changes;
    if (spec.contains("updates"))
        for (auto &update : spec.at("updates").as_array())
        {
            for (auto &[key, value] : update.as_object())
            {
                (void)value;
                Monstey_Require(key == "group_index" || key == "draw_index" || key == "translation" ||
                                    key == "quaternion" || key == "scale" || key == "attributes_npz",
                                "Unknown MESH5 fixed update field");
            }
            auto gi = update.at("group_index").as_uint(), di = update.at("draw_index").as_uint();
            auto attrs = Attributes(static_cast<std::size_t>(gi), static_cast<std::size_t>(di));
            if (update.contains("attributes_npz"))
            {
                changes.push_back(ApplyNpz(update, attrs, written));
                continue;
            }
            auto translate = Monstey_WVector(update.value("translation", J::array({0, 0, 0})), 3),
                 quaternion = Monstey_WVector(update.value("quaternion", J::array({0, 0, 0, 1})), 4);
            auto rotation = Monstey_WRotation(quaternion);
            auto scale = Monstey_WFloat(update.value("scale", 1));
            Monstey_Require(scale > 0, "MESH5 scale must be positive");
            J::Array modified;
            bool identity = true;
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j)
                    if (rotation[i][j] != (i == j ? 1.0 : 0.0))
                        identity = false;
            for (auto name : {"POSITION", "NORMAL", "TANGENT", "BINORMAL"})
            {
                auto it = attrs.find(name);
                if (it == attrs.end())
                    continue;
                bool is_position = std::string_view(name) == "POSITION";
                if (identity &&
                    (!is_position || (scale == 1 && translate[0] == 0 && translate[1] == 0 && translate[2] == 0)))
                    continue;
                auto &a = it->second;
                Monstey_Require(a.width >= 3 && (a.storage == 1 || a.storage == 2 || a.storage == 3 ||
                                                 a.storage == 10 || a.storage == 12),
                                "Cannot transform an encoded integer direction");
                bool position = std::string_view(name) == "POSITION";
                modified.push_back(name);
                for (std::size_t v = 0; v < a.count; ++v)
                {
                    std::array<double, 3> xyz{Component(a, v, 0, &before), Component(a, v, 1, &before),
                                              Component(a, v, 2, &before)};
                    for (std::size_t k = 0; k < 3; ++k)
                    {
                        double value = position ? translate[k] : 0;
                        for (std::size_t j = 0; j < 3; ++j)
                            value += rotation[k][j] * xyz[j] * (position ? scale : 1);
                        auto f = static_cast<float>(value);
                        Monstey_Require(std::isfinite(f), "Transformed MESH5 component overflow");
                        B desired;
                        if (a.unit == 2)
                            Monstey_AppendU16(desired, Monstey_WPackHalf(f));
                        else
                            Monstey_AppendU32(desired, std::bit_cast<std::uint32_t>(f));
                        auto at = a.offset + v * a.stride + k * a.unit;
                        for (std::size_t byte = 0; byte < a.unit; ++byte)
                        {
                            Monstey_Require(!written[at + byte] || raw[at + byte] == desired[byte],
                                            "Contradictory edits to aliased MESH5 vertex data");
                            raw[at + byte] = desired[byte];
                            written[at + byte] = 1;
                        }
                    }
                }
            }
            changes.push_back(J::object({{"group_index", gi}, {"draw_index", di}, {"attributes", modified}}));
        }
    Monstey_WMesh check(raw);
    Monstey_Require(check.header == header, "MESH5 edit changed header");
    std::size_t changed{};
    for (std::size_t p = 0; p < raw.size(); ++p)
        if (raw[p] != before[p])
            ++changed;
    return J::object({{"status", "MESH5_FIXED_TOPOLOGY_EDIT_VALIDATED"},
                      {"changes", changes},
                      {"changed_bytes", static_cast<std::uint64_t>(changed)},
                      {"dependent_resources_to_update",
                       J::array({"WDSC culling bounds and render associations", "WorldPhysics collision if affected",
                                 "HKAI navigation if affected"})},
                      {"runtime_verified", false}});
}

B Monstey_WEncodeNativeNumber(const Monstey_WMesh::Attr &a, double value)
{
    Monstey_Require(std::isfinite(value), "Nonfinite topology attribute");
    B out;
    if (a.storage == 4)
    {
        Monstey_Require(value >= 0 && value <= 255 && std::floor(value) == value,
                        "Topology uint8 overflow or fractional value");
        out.push_back(static_cast<std::uint8_t>(value));
    }
    else if (a.storage == 9)
    {
        Monstey_Require(value >= -32768 && value <= 32767 && std::floor(value) == value,
                        "Topology int16 overflow or fractional value");
        Monstey_AppendU16(out, static_cast<std::uint16_t>(static_cast<std::int16_t>(value)));
    }
    else
    {
        auto f = static_cast<float>(value);
        Monstey_Require(std::isfinite(f), "Topology native float overflow");
        if (a.unit == 2)
            Monstey_AppendU16(out, Monstey_WPackHalf(f));
        else
            Monstey_AppendU32(out, std::bit_cast<std::uint32_t>(f));
    }
    return out;
}

std::pair<B, J> Monstey_WTopology(const Monstey_WMesh &mesh, const J &spec)
{
    for (auto &[key, value] : spec.as_object())
    {
        (void)value;
        Monstey_Require(key == "source_sha256" || key == "groups" || key == "append_groups",
                        "Unknown native topology spec field");
    }
    if (spec.contains("source_sha256"))
        Monstey_Require(spec.at("source_sha256").as_string() == Monstey_HashSha256(mesh.raw),
                        "Topology source hash differs");
    std::map<std::size_t, J> selected;
    if (spec.contains("groups"))
        for (auto &g : spec.at("groups").as_array())
        {
            for (auto &[key, value] : g.as_object())
            {
                (void)value;
                Monstey_Require(key == "group_index" || key == "draws", "Unknown topology group field");
            }
            auto gi = g.at("group_index").as_uint();
            Monstey_Require(gi < mesh.groups.size() && g.at("draws").is_array() &&
                                selected.emplace(static_cast<std::size_t>(gi), g.at("draws")).second,
                            "Invalid or duplicate topology group");
        }
    struct Work
    {
        std::size_t source;
        std::optional<J> draws;
    };
    std::vector<Work> work;
    for (std::size_t gi = 0; gi < mesh.groups.size(); ++gi)
    {
        auto it = selected.find(gi);
        work.push_back({gi, it == selected.end() ? std::optional<J>{} : it->second});
    }
    if (spec.contains("append_groups"))
        for (auto &g : spec.at("append_groups").as_array())
        {
            Monstey_Require(g.as_object().size() == 2 && g.contains("template_group") && g.contains("draws") &&
                                g.at("draws").is_array(),
                            "Invalid appended topology group");
            auto source = g.at("template_group").as_uint();
            Monstey_Require(source < mesh.groups.size(), "Topology template group outside source");
            work.push_back({static_cast<std::size_t>(source), g.at("draws")});
        }
    Monstey_Require(work.size() <= 65535, "MESH geometry groups exceed native uint16");
    std::vector<B> encoded_groups;
    J::Array mapping;
    for (std::size_t newgi = 0; newgi < work.size(); ++newgi)
    {
        auto &current = work[newgi];
        auto &group = mesh.groups[current.source];
        J::Array mapped;
        if (!current.draws)
        {
            encoded_groups.emplace_back(mesh.raw.begin() + static_cast<std::ptrdiff_t>(group.start),
                                        mesh.raw.begin() + static_cast<std::ptrdiff_t>(group.end));
            for (std::size_t di = 0; di < group.draws.size(); ++di)
                mapped.push_back(J::object({{"draw_index", static_cast<std::uint64_t>(di)},
                                            {"source_group", static_cast<std::uint64_t>(current.source)},
                                            {"source_draw", static_cast<std::uint64_t>(di)},
                                            {"partition", 0},
                                            {"material", nullptr},
                                            {"material_slot", group.draws[di][6] & 65535u},
                                            {"source_material_slot", group.draws[di][6] & 65535u}}));
        }
        else
        {
            B vertex, index;
            std::vector<std::array<std::uint32_t, 13>> draws;
            std::map<std::string, std::pair<std::uint32_t, std::size_t>> vertex_cache, index_cache;
            std::map<std::string, std::uint32_t> material_slots;
            std::uint32_t next_slot{};
            for (auto &w : group.draws)
                next_slot = std::max(next_slot, (w[6] & 65535u) + 1);
            for (auto &request : current.draws->as_array())
            {
                for (auto &[key, value] : request.as_object())
                {
                    (void)value;
                    Monstey_Require(key == "source_draw" || key == "attributes_npz" || key == "material",
                                    "Unknown topology draw field");
                }
                auto di = request.at("source_draw").as_uint();
                Monstey_Require(di < group.draws.size(), "Topology donor draw outside group");
                auto descriptors = mesh.Attributes(current.source, static_cast<std::size_t>(di));
                auto words = group.draws[static_cast<std::size_t>(di)];
                auto li = words[9] & 255u;
                auto &layout = group.layouts[li];
                std::map<std::string, B> arrays;
                std::vector<std::uint32_t> triangles;
                std::size_t count = words[1] & 65535u;
                J material;
                auto slot = words[6] & 65535u;
                if (request.contains("material"))
                {
                    auto name = Monstey_ArchiveVirtual(request.at("material").as_string());
                    Monstey_Require(name.ends_with(".mat"), "Topology material must be a native .mat resource");
                    auto found = material_slots.find(name);
                    if (found == material_slots.end())
                    {
                        Monstey_Require(next_slot <= 65535, "Topology material slots exceed native uint16");
                        found = material_slots.emplace(name, next_slot++).first;
                    }
                    slot = found->second;
                    material = name;
                }
                if (request.contains("attributes_npz"))
                {
                    auto source = Monstey_WPath(request.at("attributes_npz"));
                    auto raw = Monstey_ReadFile(source);
                    auto npz = Monstey_WNpzRead(raw);
                    Monstey_Require(npz.size() == descriptors.size() + 1 && npz.contains("INDICES") &&
                                        npz.contains("POSITION"),
                                    "Topology NPZ must contain every native channel plus INDICES");
                    count = npz.at("POSITION").count;
                    Monstey_Require(count > 0 && count <= 0xffffffffull, "Topology vertex count outside uint32");
                    for (auto &[name, a] : descriptors)
                    {
                        auto found = npz.find(name);
                        Monstey_Require(found != npz.end() && found->second.count == count &&
                                            found->second.width == a.width,
                                        "Topology attribute channel/shape differs from donor layout");
                        auto &bytes = arrays[name];
                        for (std::size_t row = 0; row < count; ++row)
                            for (std::size_t component = 0; component < a.width; ++component)
                            {
                                auto encoded = Monstey_WEncodeNativeNumber(a, found->second.Number(row, component));
                                bytes.insert(bytes.end(), encoded.begin(), encoded.end());
                            }
                    }
                    auto &faces = npz.at("INDICES");
                    Monstey_Require(faces.width == 3 && faces.count > 0 && faces.count < (1u << 24),
                                    "Topology triangles require nonempty Nx3 matrix within native capacity");
                    triangles.reserve(faces.count * 3);
                    for (std::size_t row = 0; row < faces.count; ++row)
                        for (std::size_t axis = 0; axis < 3; ++axis)
                        {
                            auto value = faces.Number(row, axis);
                            Monstey_Require(std::isfinite(value) && value >= 0 && value < count &&
                                                std::floor(value) == value,
                                            "Topology triangle refers to a missing vertex");
                            triangles.push_back(static_cast<std::uint32_t>(value));
                        }
                    Monstey_Require(Monstey_HashSha256(source) == Monstey_HashSha256(raw),
                                    "Topology NPZ changed during import");
                }
                else
                {
                    for (auto &[name, a] : descriptors)
                    {
                        auto &bytes = arrays[name];
                        for (std::size_t row = 0; row < a.count; ++row)
                        {
                            auto at = a.offset + row * a.stride;
                            bytes.insert(bytes.end(), mesh.raw.begin() + static_cast<std::ptrdiff_t>(at),
                                         mesh.raw.begin() + static_cast<std::ptrdiff_t>(at + a.width * a.unit));
                        }
                    }
                    for (auto value : mesh.Indices(current.source, static_cast<std::size_t>(di)))
                        triangles.push_back(value);
                }
                struct Part
                {
                    std::vector<std::uint32_t> source_vertices;
                    std::vector<std::uint16_t> indices;
                };
                std::vector<Part> parts;
                if (!request.contains("attributes_npz"))
                {
                    Part part;
                    part.source_vertices.reserve(count);
                    for (std::uint32_t j = 0; j < count; ++j)
                        part.source_vertices.push_back(j);
                    for (auto value : triangles)
                        part.indices.push_back(static_cast<std::uint16_t>(value));
                    parts.push_back(std::move(part));
                }
                else
                {
                    Part part;
                    std::unordered_map<std::uint32_t, std::uint16_t> lookup;
                    for (std::size_t triangle = 0; triangle < triangles.size(); triangle += 3)
                    {
                        std::set<std::uint32_t> needed;
                        for (std::size_t j = 0; j < 3; ++j)
                            if (!lookup.contains(triangles[triangle + j]))
                                needed.insert(triangles[triangle + j]);
                        if (part.source_vertices.size() + needed.size() > 65535)
                        {
                            parts.push_back(std::move(part));
                            part = Part{};
                            lookup.clear();
                        }
                        for (std::size_t j = 0; j < 3; ++j)
                        {
                            auto source = triangles[triangle + j];
                            auto found = lookup.find(source);
                            if (found == lookup.end())
                            {
                                auto local = static_cast<std::uint16_t>(part.source_vertices.size());
                                part.source_vertices.push_back(source);
                                found = lookup.emplace(source, local).first;
                            }
                            part.indices.push_back(found->second);
                        }
                    }
                    if (!part.indices.empty())
                        parts.push_back(std::move(part));
                }
                std::size_t total_stride{};
                for (auto &e : layout)
                {
                    auto [unit, width] = Monstey_WMesh::Storage(e[1]);
                    total_stride += unit * width;
                }
                for (std::size_t partition = 0; partition < parts.size(); ++partition)
                {
                    auto &part = parts[partition];
                    B encoded;
                    for (unsigned stream = 0; stream < 2; ++stream)
                    {
                        std::size_t stride{};
                        for (auto &e : layout)
                            if (e[0] == stream)
                            {
                                auto [unit, width] = Monstey_WMesh::Storage(e[1]);
                                stride += unit * width;
                            }
                        auto begin = encoded.size();
                        encoded.resize(begin + part.source_vertices.size() * stride);
                        std::size_t offset{};
                        for (auto &e : layout)
                            if (e[0] == stream)
                            {
                                auto name = Monstey_WMesh::Semantic(e[2], e[3]);
                                auto descriptor = descriptors.at(name);
                                auto width = std::size_t(descriptor.width) * descriptor.unit;
                                auto &data = arrays.at(name);
                                for (std::size_t row = 0; row < part.source_vertices.size(); ++row)
                                {
                                    auto source = std::size_t(part.source_vertices[row]) * width;
                                    std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(source), width,
                                                encoded.begin() +
                                                    static_cast<std::ptrdiff_t>(begin + row * stride + offset));
                                }
                                offset += width;
                            }
                    }
                    auto key = std::to_string(li) + ":" + Monstey_HashSha256(encoded);
                    auto found = vertex_cache.find(key);
                    if (found == vertex_cache.end())
                    {
                        while (vertex.size() % total_stride)
                            vertex.push_back(0);
                        Monstey_Require(vertex.size() + encoded.size() <= 0xffffffffull,
                                        "Topology native vertex buffer exceeds uint32");
                        auto position = static_cast<std::uint32_t>(vertex.size());
                        vertex.insert(vertex.end(), encoded.begin(), encoded.end());
                        found = vertex_cache.emplace(key, std::make_pair(position, encoded.size())).first;
                    }
                    else
                        Monstey_Require(
                            found->second.second == encoded.size() &&
                                std::equal(encoded.begin(), encoded.end(), vertex.begin() + found->second.first),
                            "Vertex cache hash collision");
                    B index_bytes;
                    for (auto value : part.indices)
                        Monstey_AppendU16(index_bytes, value);
                    auto index_key = Monstey_HashSha256(index_bytes);
                    auto ifound = index_cache.find(index_key);
                    if (ifound == index_cache.end())
                    {
                        Monstey_Require(index.size() + index_bytes.size() <= 0xffffffffull,
                                        "Topology index buffer exceeds uint32");
                        auto position = static_cast<std::uint32_t>(index.size() / 2);
                        index.insert(index.end(), index_bytes.begin(), index_bytes.end());
                        ifound = index_cache.emplace(index_key, std::make_pair(position, index_bytes.size())).first;
                    }
                    else
                        Monstey_Require(ifound->second.second == index_bytes.size() &&
                                            std::equal(index_bytes.begin(), index_bytes.end(),
                                                       index.begin() + 2ull * ifound->second.first),
                                        "Index cache hash collision");
                    auto draw = words;
                    auto vertices = static_cast<std::uint32_t>(part.source_vertices.size());
                    draw[1] = (draw[1] & 0xffff0000u) | vertices;
                    draw[2] = (draw[2] & 0xffff0000u) | vertices;
                    draw[3] = ifound->second.first;
                    draw[4] = found->second.first / static_cast<std::uint32_t>(total_stride);
                    Monstey_Require(part.indices.size() / 3 < (1u << 24), "Native triangle count exceeds 24 bits");
                    draw[5] = (draw[5] & 0xff000000u) | static_cast<std::uint32_t>(part.indices.size() / 3);
                    draw[6] = (draw[6] & 0xffff0000u) | slot;
                    draw[10] = found->second.first + static_cast<std::uint32_t>(encoded.size());
                    draws.push_back(draw);
                    mapped.push_back(J::object({{"draw_index", static_cast<std::uint64_t>(draws.size() - 1)},
                                                {"source_group", static_cast<std::uint64_t>(current.source)},
                                                {"source_draw", di},
                                                {"partition", static_cast<std::uint64_t>(partition)},
                                                {"material", material},
                                                {"material_slot", slot},
                                                {"source_material_slot", words[6] & 65535u},
                                                {"vertices", vertices},
                                                {"triangles", static_cast<std::uint64_t>(part.indices.size() / 3)}}));
                }
            }
            Monstey_Require(draws.size() <= 65535, "Native draw associations exceed uint16");
            B group_bytes;
            for (auto word : {static_cast<std::uint32_t>(draws.size()), static_cast<std::uint32_t>(vertex.size()),
                              static_cast<std::uint32_t>(index.size())})
                Monstey_AppendU32(group_bytes, word);
            group_bytes.insert(group_bytes.end(), vertex.begin(), vertex.end());
            group_bytes.insert(group_bytes.end(), index.begin(), index.end());
            Monstey_AppendU32(group_bytes, static_cast<std::uint32_t>(group.layouts.size()));
            for (auto &layout : group.layouts)
            {
                Monstey_AppendU32(group_bytes, static_cast<std::uint32_t>(layout.size()));
                for (auto &entry : layout)
                    group_bytes.insert(group_bytes.end(), entry.begin(), entry.end());
            }
            Monstey_AppendU32(group_bytes, static_cast<std::uint32_t>(draws.size()));
            for (auto &draw : draws)
                for (auto word : draw)
                    Monstey_AppendU32(group_bytes, word);
            encoded_groups.push_back(std::move(group_bytes));
        }
        mapping.push_back(
            J::object({{"group_index", static_cast<std::uint64_t>(newgi)},
                       {"source_group", static_cast<std::uint64_t>(current.source)},
                       {"source_draw_count", static_cast<std::uint64_t>(mesh.groups[current.source].draws.size())},
                       {"draws", mapped},
                       {"changed", current.draws.has_value()}}));
    }
    auto h = mesh.header;
    h[2] = static_cast<std::uint32_t>(work.size());
    h[3] = h[4] = h[5] = h[6] = 0;
    std::uint64_t extent = 32ull + h[7];
    for (std::size_t gi = 0; gi < work.size(); ++gi)
    {
        h[3] += static_cast<std::uint32_t>(mapping[gi].at("draws").size());
        h[4] += static_cast<std::uint32_t>(mesh.groups[work[gi].source].layouts.size());
        for (auto &layout : mesh.groups[work[gi].source].layouts)
            h[5] += static_cast<std::uint32_t>(layout.size());
        h[6] += ((Monstey_U32(encoded_groups[gi], 4) + 3u) & ~3u) + ((Monstey_U32(encoded_groups[gi], 8) + 3u) & ~3u);
        extent += encoded_groups[gi].size();
    }
    Monstey_Require(extent <= 512ull * 1024 * 1024, "Authored topology exceeds bounded output extent");
    B out;
    for (auto word : h)
        Monstey_AppendU32(out, word);
    out.insert(out.end(), mesh.raw.begin() + 32, mesh.raw.begin() + 32 + mesh.header[7]);
    for (auto &group : encoded_groups)
        out.insert(out.end(), group.begin(), group.end());
    Monstey_WMesh checked(out);
    for (std::size_t gi = 0; gi < mapping.size(); ++gi)
        for (auto &row : mapping[gi]["draws"].as_array())
        {
            auto attrs = checked.Attributes(gi, static_cast<std::size_t>(row.at("draw_index").as_uint()));
            auto position = attrs.find("POSITION");
            Monstey_Require(position != attrs.end() && position->second.width >= 3,
                            "Topology geometry needs native 3D positions");
            J::Array lower, upper;
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                double lo = std::numeric_limits<double>::infinity(), hi = -lo;
                for (auto vertex : checked.Indices(gi, static_cast<std::size_t>(row.at("draw_index").as_uint())))
                {
                    auto value = checked.Component(position->second, vertex, axis);
                    Monstey_Require(std::isfinite(value), "Authored native position is nonfinite");
                    lo = std::min(lo, value);
                    hi = std::max(hi, value);
                }
                lower.push_back(lo);
                upper.push_back(hi);
            }
            row["bounds_min"] = lower;
            row["bounds_max"] = upper;
        }
    return {out, J::object({{"status", "NATIVE_MESH5_TOPOLOGY_REBUILT"},
                            {"source_sha256", Monstey_HashSha256(mesh.raw)},
                            {"output_sha256", Monstey_HashSha256(out)},
                            {"groups", mapping},
                            {"dictionary_byte_exact", true},
                            {"all_vertex_channels_retained", true},
                            {"uint16_partitioning", true},
                            {"render_associations_required", true},
                            {"collision_or_navigation_updated", false},
                            {"runtime_verified", false}})};
}

} // namespace Monstey_WorldPrivate
using namespace Monstey_WorldPrivate;

std::pair<Monstey_Bytes, Monstey_Json> Monstey_WorldMeshApplyBytes(std::span<const std::uint8_t> raw, const J &spec)
{
    Monstey_WMesh mesh(B(raw.begin(), raw.end()));
    if (spec.contains("topology"))
    {
        for (auto &[key, value] : spec.as_object())
        {
            (void)value;
            Monstey_Require(key == "source_sha256" || key == "topology",
                            "Fixed and topology changes must be separate transactions");
        }
        auto topology = spec.at("topology");
        if (spec.contains("source_sha256"))
            topology["source_sha256"] = spec.at("source_sha256");
        return Monstey_WTopology(mesh, topology);
    }
    auto report = mesh.Apply(spec);
    Monstey_WMesh checked(mesh.raw);
    return {checked.raw, report};
}

} // namespace Monstey
