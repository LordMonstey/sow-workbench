#include "Monstey_WorldMerge.hpp"
#include "Monstey_Archive.hpp"
#include <algorithm>
#include <limits>
#include <numeric>
#include <set>
namespace Monstey {
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
constexpr std::uint64_t Monstey_MMaximum = std::numeric_limits<std::uint32_t>::max();
std::uint32_t Monstey_MWord(std::uint64_t n, const char *what) {
    Monstey_Require(n <= Monstey_MMaximum, std::string("Merged native ") + what + " exceeds uint32");
    return static_cast<std::uint32_t>(n);
}
std::uint64_t Monstey_MAlign(std::uint64_t n, std::uint64_t alignment) {
    Monstey_Require(alignment > 0 && alignment <= Monstey_MMaximum,
                    "Native vertex alignment is outside uint32");
    auto remainder = n % alignment;
    return remainder ? Monstey_MWord(n + alignment - remainder, "alignment") : n;
}
B Monstey_MEncode(const Monstey_WorldRawMesh &data) {
    auto header = data.header;
    Monstey_Require(header[0] == 0x4853454du && header[1] == 5, "World merger needs native MESH5");
    std::uint64_t draws = 0, layouts = 0, attributes = 0, allocation = 0;
    for (auto &group : data.groups) {
        draws += group.draws.size();
        layouts += group.layouts.size();
        for (auto &layout : group.layouts)
            attributes += layout.size();
        allocation += ((std::uint64_t(group.vertex_buffer.size()) + 3) & ~3ull) +
                      ((std::uint64_t(group.index_buffer.size()) + 3) & ~3ull);
    }
    header[2] = Monstey_MWord(data.groups.size(), "context count");
    header[3] = Monstey_MWord(draws, "draw count");
    header[4] = Monstey_MWord(layouts, "layout count");
    header[5] = Monstey_MWord(attributes, "attribute count");
    header[6] = Monstey_MWord(allocation, "allocation");
    header[7] = Monstey_MWord(data.opaque_header.size(), "opaque-header count");
    B output;
    for (auto value : header)
        Monstey_AppendU32(output, value);
    output.insert(output.end(), data.opaque_header.begin(), data.opaque_header.end());
    for (auto &group : data.groups) {
        Monstey_AppendU32(output, Monstey_MWord(group.draws.size(), "context draw count"));
        Monstey_AppendU32(output, Monstey_MWord(group.vertex_buffer.size(), "vertex buffer"));
        Monstey_AppendU32(output, Monstey_MWord(group.index_buffer.size(), "index buffer"));
        output.insert(output.end(), group.vertex_buffer.begin(), group.vertex_buffer.end());
        output.insert(output.end(), group.index_buffer.begin(), group.index_buffer.end());
        Monstey_AppendU32(output, Monstey_MWord(group.layouts.size(), "context layout count"));
        for (auto &layout : group.layouts) {
            Monstey_AppendU32(output, Monstey_MWord(layout.size(), "layout attribute count"));
            for (auto &attribute : layout)
                output.insert(output.end(), attribute.begin(), attribute.end());
        }
        Monstey_AppendU32(output, Monstey_MWord(group.draws.size(), "context draw count"));
        for (auto &draw : group.draws)
            for (auto word : draw)
                Monstey_AppendU32(output, word);
    }
    return output;
}
} // namespace
Monstey_WorldMergeResult Monstey_WorldMerge(std::span<const Monstey_WorldMergeSource> sources) {
    Monstey_Require(!sources.empty(), "Select at least one native geometry source");
    std::vector<Monstey_WorldRawMesh> parsed;
    std::set<std::string> ids;
    J::Array source_rows;
    for (auto &source : sources) {
        Monstey_Require(!source.id.empty() && ids.insert(source.id).second,
                        "Native merge source identities must be unique");
        Monstey_Require(source.expected_sha256 == Monstey_HashSha256(source.mesh),
                        "Native merge source checksum differs");
        auto data = Monstey_WorldMeshReadContexts(source.mesh);
        Monstey_Require(source.context < data.groups.size() && source.context <= 65535,
                        "Selected source context is outside native uint16 binding");
        Monstey_Require(Monstey_MEncode(data) == source.mesh,
                        "Qualified native context encoding is not byte exact");
        Monstey_Require(source.material_paths.size() <= 65536,
                        "Source material table exceeds native slot domain");
        for (auto &path : source.material_paths) {
            auto normalized = Monstey_ArchiveVirtual(path);
            Monstey_Require(normalized.ends_with(".mat"), "Native merge material needs .mat path");
        }
        for (auto &draw : data.groups[source.context].draws)
            Monstey_Require((draw[6] & 65535) < source.material_paths.size(),
                            "Native source draw material is absent from explicit slot mapping");
        source_rows.push_back(J::object({{"id", source.id},
                                         {"resource_path", source.resource_path},
                                         {"sha256", source.expected_sha256},
                                         {"context", source.context},
                                         {"context_count", data.groups.size()},
                                         {"material_slots", source.material_paths.size()}}));
        parsed.push_back(std::move(data));
    }
    auto output = parsed[0];
    auto target = sources[0].context;
    Monstey_WorldMergeResult result;
    result.material_paths = sources[0].material_paths;
    J::Array draws, materials, regions;
    if (sources.size() > 1)
        for (auto &data : parsed)
            Monstey_Require(data.opaque_header.empty(),
                            "Multi-source geometry cannot relocate an unqualified opaque MESH header");
    auto &merged = output.groups[target];
    if (sources.size() > 1)
        merged = Monstey_WorldRawMeshContext{};
    for (std::size_t si = 0; si < sources.size(); ++si) {
        auto &source = sources[si];
        auto &context = parsed[si].groups[source.context];
        auto material_base = si == 0 ? std::size_t(0) : result.material_paths.size();
        if (si != 0) {
            Monstey_Require(source.material_paths.size() <= 65536 - material_base,
                            "Merged material slots exceed uint16 domain");
            result.material_paths.insert(result.material_paths.end(), source.material_paths.begin(),
                                         source.material_paths.end());
        }
        for (std::size_t slot = 0; slot < source.material_paths.size(); ++slot)
            materials.push_back(J::object({{"source", source.id},
                                           {"source_slot", slot},
                                           {"output_slot", material_base + slot},
                                           {"path", source.material_paths[slot]}}));
        std::uint64_t vertex_base = 0, index_base = 0;
        std::vector<std::uint32_t> layout_map;
        if (sources.size() > 1) {
            Monstey_Require(context.layouts.size() == context.layout_strides.size(),
                            "Qualified layout strides are missing");
            std::uint64_t alignment = 1;
            if (!merged.vertex_buffer.empty())
                for (auto &draw : context.draws) {
                    auto layout = draw[9] & 255;
                    Monstey_Require(layout < context.layout_strides.size(), "Native layout is absent");
                    auto stride = context.layout_strides[layout];
                    Monstey_Require(stride > 0, "Native layout has zero stride");
                    auto factor = std::uint64_t(stride) / std::gcd(alignment, std::uint64_t(stride));
                    Monstey_Require(alignment <= Monstey_MMaximum / factor,
                                    "Raw mixed layouts require unrepresentable vertex alignment");
                    alignment *= factor;
                }
            vertex_base = Monstey_MAlign(merged.vertex_buffer.size(), alignment);
            index_base = Monstey_MAlign(merged.index_buffer.size(), 2);
            Monstey_MWord(vertex_base + context.vertex_buffer.size(), "vertex buffer");
            Monstey_MWord(index_base + context.index_buffer.size(), "index buffer");
            merged.vertex_buffer.resize(static_cast<std::size_t>(vertex_base), 0);
            merged.index_buffer.resize(static_cast<std::size_t>(index_base), 0);
            merged.vertex_buffer.insert(merged.vertex_buffer.end(), context.vertex_buffer.begin(),
                                        context.vertex_buffer.end());
            merged.index_buffer.insert(merged.index_buffer.end(), context.index_buffer.begin(),
                                       context.index_buffer.end());
            for (std::size_t li = 0; li < context.layouts.size(); ++li) {
                auto found =
                    si == 0 ? merged.layouts.end()
                            : std::find(merged.layouts.begin(), merged.layouts.end(), context.layouts[li]);
                auto output_layout = static_cast<std::size_t>(found - merged.layouts.begin());
                if (found == merged.layouts.end()) {
                    merged.layouts.push_back(context.layouts[li]);
                    merged.layout_strides.push_back(context.layout_strides[li]);
                }
                layout_map.push_back(Monstey_MWord(output_layout, "layout ordinal"));
            }
        } else
            for (std::size_t li = 0; li < context.layouts.size(); ++li)
                layout_map.push_back(Monstey_MWord(li, "layout ordinal"));
        auto draw_base = sources.size() == 1 ? std::size_t(0) : merged.draws.size();
        for (std::size_t di = 0; di < context.draws.size(); ++di) {
            auto before = context.draws[di], after = before;
            auto layout = before[9] & 255;
            Monstey_Require(layout < layout_map.size() && layout_map[layout] <= 255,
                            "Merged active layout exceeds native byte index");
            auto stride = context.layout_strides[layout];
            Monstey_Require(stride > 0 && vertex_base % stride == 0,
                            "Native vertex relocation is not stride aligned");
            after[3] = Monstey_MWord(std::uint64_t(before[3]) + index_base / 2, "index base");
            after[4] = Monstey_MWord(std::uint64_t(before[4]) + vertex_base / stride, "vertex base");
            after[6] = (before[6] & 0xffff0000u) |
                       Monstey_MWord(material_base + (before[6] & 65535), "material slot");
            after[9] = (before[9] & 0xffffff00u) | layout_map[layout];
            after[10] = Monstey_MWord(std::uint64_t(before[10]) + vertex_base, "vertex end");
            if (sources.size() > 1)
                merged.draws.push_back(after);
            draws.push_back(J::object({{"source", source.id},
                                       {"source_context", source.context},
                                       {"source_draw", di},
                                       {"output_context", target},
                                       {"output_draw", draw_base + di},
                                       {"source_material_slot", before[6] & 65535},
                                       {"output_material_slot", after[6] & 65535},
                                       {"source_layout", layout},
                                       {"output_layout", layout_map[layout]},
                                       {"local_uint16_indices_retained", true},
                                       {"opaque_draw_words_retained", true}}));
        }
        regions.push_back(J::object({{"source", source.id},
                                     {"vertex_byte_offset", vertex_base},
                                     {"vertex_bytes", context.vertex_buffer.size()},
                                     {"index_byte_offset", index_base},
                                     {"index_bytes", context.index_buffer.size()},
                                     {"entire_buffers_preserved", true}}));
    }
    Monstey_Require(sources.size() == 1 || merged.draws.size() <= 65535,
                    "Merged context draw ordinals exceed native uint16 cull binding");
    result.mesh = sources.size() == 1 ? sources[0].mesh : Monstey_MEncode(output);
    auto verified = Monstey_WorldMeshReadContexts(result.mesh);
    Monstey_Require(Monstey_MEncode(verified) == result.mesh, "Merged native geometry readback differs");
    for (std::size_t i = 0; i < output.groups.size(); ++i)
        if (i != target) {
            auto &old = parsed[0].groups[i];
            auto &now = verified.groups[i];
            Monstey_Require(old.vertex_buffer == now.vertex_buffer && old.index_buffer == now.index_buffer &&
                                old.layouts == now.layouts && old.draws == now.draws,
                            "Unselected base geometry context changed");
        }
    result.report = J::object({{"status", "NATIVE_COMMON_MESH_CONTEXT_COMPOSED"},
                               {"sources", source_rows},
                               {"target_context", target},
                               {"draw_remap", draws},
                               {"material_remap", materials},
                               {"encoded_regions", regions},
                               {"output_sha256", Monstey_HashSha256(result.mesh)},
                               {"output_bytes", result.mesh.size()},
                               {"single_source_byte_exact", sources.size() == 1},
                               {"encoded_channels_byte_exact", true},
                               {"triangle_order_byte_exact", true},
                               {"unselected_base_contexts_byte_exact", true},
                               {"runtime_verified", false},
                               {"render_associations_written", false},
                               {"game_written", false}});
    return result;
}
} // namespace Monstey