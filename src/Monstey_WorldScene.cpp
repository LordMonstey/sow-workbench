#include "Monstey_WorldScene.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
void Monstey_SRequire(bool condition, std::string_view message) {
    if (!condition)
        throw Monstey_Error("world_scene", std::string(message));
}
class Monstey_SReader {
    std::span<const std::uint8_t> bytes_;
    std::size_t at_ = 0;

  public:
    explicit Monstey_SReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    Monstey_Bytes Take(std::size_t count) {
        Monstey_CheckRange(bytes_, at_, count);
        auto begin = bytes_.begin() + static_cast<std::ptrdiff_t>(at_);
        at_ += count;
        return Monstey_Bytes(begin, begin + static_cast<std::ptrdiff_t>(count));
    }
    std::uint32_t Word() {
        const auto value = Monstey_U32(bytes_, at_);
        at_ += 4;
        return value;
    }
    template <std::size_t N> std::array<std::uint32_t, N> Words() {
        std::array<std::uint32_t, N> words;
        for (auto &word : words)
            word = Word();
        return words;
    }
    std::vector<std::uint32_t> Indices(std::uint32_t count) {
        Monstey_SRequire(count <= (bytes_.size() - at_) / 4, "Scene index list exceeds input");
        std::vector<std::uint32_t> result;
        result.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
            result.push_back(Word());
        return result;
    }
    bool End() const { return at_ == bytes_.size(); }
};
template <class T> void Monstey_SAppendWords(Monstey_Bytes &out, const T &words) {
    for (const auto word : words)
        Monstey_AppendU32(out, word);
}
J Monstey_SWords(const auto &words) {
    auto result = J::array();
    for (const auto word : words)
        result.as_array().push_back(word);
    return result;
}
std::array<float, 6> Monstey_SBounds(const Monstey_WorldSceneNode &node) {
    std::array<float, 6> bounds;
    for (unsigned i = 0; i < 6; ++i)
        bounds[i] = std::bit_cast<float>(node.words[3 + i]);
    return bounds;
}
void Monstey_SValidateBounds(const std::array<float, 6> &bounds) {
    for (const auto value : bounds)
        Monstey_SRequire(std::isfinite(value), "Scene node bounds are nonfinite");
    for (unsigned i = 0; i < 3; ++i)
        Monstey_SRequire(bounds[i] <= bounds[i + 3], "Scene node bounds are inverted");
}
std::vector<std::uint32_t> Monstey_SParents(const Monstey_WorldScene &scene) {
    const auto none = UINT32_MAX;
    std::vector<std::uint32_t> parents(scene.nodes.size(), none);
    for (std::size_t i = 0; i < scene.nodes.size(); ++i)
        for (const auto child : scene.nodes[i].children) {
            Monstey_SRequire(child < scene.nodes.size() && child != i, "Scene child index invalid");
            Monstey_SRequire(parents[child] == none, "Scene child has repeated or multiple parents");
            parents[child] = static_cast<std::uint32_t>(i);
        }
    // Parent chains share an iterative color walk; no recursive stack or quadratic ancestor scans.
    std::vector<std::uint8_t> marks(scene.nodes.size(), 0);
    for (std::size_t i = 0; i < parents.size(); ++i) {
        if (marks[i] == 2)
            continue;
        auto current = static_cast<std::uint32_t>(i);
        std::vector<std::uint32_t> path;
        while (current != none && marks[current] == 0) {
            marks[current] = 1;
            path.push_back(current);
            current = parents[current];
        }
        Monstey_SRequire(current == none || marks[current] != 1, "Scene node hierarchy cycle");
        for (const auto value : path)
            marks[value] = 2;
    }
    return parents;
}
} // namespace
Monstey_WorldScene Monstey_WorldScene::Read(std::span<const std::uint8_t> bytes, std::uint32_t groupCount) {
    Monstey_SRequire(bytes.size() >= 52 && bytes.size() <= 512ull * 1024 * 1024, "Scene extent limit");
    Monstey_SReader reader(bytes);
    Monstey_WorldScene scene;
    scene.header = reader.Words<13>();
    Monstey_SRequire(scene.header[0] <= 1000000 && scene.header[3] <= 1000000 && scene.header[8] <= 1000000,
                     "Scene table count limit");
    for (std::uint32_t i = 0; i < scene.header[8]; ++i) {
        Monstey_WorldSceneGeometry record;
        record.words = reader.Words<5>();
        record.vertices = reader.Take(static_cast<std::size_t>(record.words[2]) * 12);
        record.indices = reader.Take(static_cast<std::size_t>(record.words[3]) * 2);
        record.extras = reader.Take(static_cast<std::size_t>(record.words[4]) * 2);
        scene.geometry.push_back(std::move(record));
    }
    for (std::uint32_t i = 0; i < scene.header[0]; ++i) {
        Monstey_WorldSceneNode node;
        node.words = reader.Words<15>();
        node.planes = reader.Take(static_cast<std::size_t>(node.words[14] & 255) * 16);
        node.geometry = reader.Indices(node.words[11]);
        node.children = reader.Indices(node.words[12]);
        scene.nodes.push_back(std::move(node));
    }
    for (std::uint32_t i = 0; i < scene.header[3]; ++i) {
        Monstey_WorldSceneBspNode record;
        record.words = reader.Words<5>();
        record.nodes = reader.Indices(record.words[0]);
        scene.bsp_nodes.push_back(std::move(record));
    }
    Monstey_SRequire(reader.End(), "Scene trailing bytes require an explicitly qualified extension");
    scene.Validate(groupCount);
    return scene;
}
void Monstey_WorldScene::Validate(std::uint32_t groupCount) const {
    for (const auto index : {2u, 5u, 10u, 12u})
        Monstey_SRequire(header[index] == 0, "Native reader rejects reserved scene counters");
    Monstey_SRequire(header[0] == nodes.size() && header[3] == bsp_nodes.size() &&
                         header[8] == geometry.size(),
                     "Scene table count mismatch");
    Monstey_SRequire(header[1] <= groupCount, "Scene consumed group prefix exceeds native groups");
    std::uint64_t geometryBytes = 0, planeCount = 0, geometryReferences = 0;
    for (const auto &record : geometry) {
        Monstey_SRequire(record.vertices.size() == static_cast<std::uint64_t>(record.words[2]) * 12 &&
                             record.indices.size() == static_cast<std::uint64_t>(record.words[3]) * 2 &&
                             record.extras.size() == static_cast<std::uint64_t>(record.words[4]) * 2,
                         "Scene geometry payload extent mismatch");
        geometryBytes += record.vertices.size() + record.indices.size() + record.extras.size();
    }
    for (const auto &node : nodes) {
        Monstey_SValidateBounds(Monstey_SBounds(node));
        Monstey_SRequire(node.planes.size() == static_cast<std::size_t>(node.words[14] & 255) * 16 &&
                             node.geometry.size() == node.words[11] && node.children.size() == node.words[12],
                         "Scene node payload count mismatch");
        planeCount += node.words[14] & 255;
        geometryReferences += node.geometry.size();
        Monstey_SRequire(node.words[13] == UINT32_MAX || node.words[13] < groupCount,
                         "Scene node group ordinal exceeds native group array");
        for (const auto ref : node.geometry)
            Monstey_SRequire(ref < geometry.size(), "Scene geometry reference invalid");
    }
    Monstey_SRequire(header[6] == planeCount && header[7] == geometryReferences && header[9] == geometryBytes,
                     "Scene native allocation counters differ from payloads");
    (void)Monstey_SParents(*this);
    for (const auto &record : bsp_nodes) {
        Monstey_SRequire(record.words[0] == record.nodes.size(), "Scene BSP count mismatch");
        for (const auto node : record.nodes)
            Monstey_SRequire(node < nodes.size(), "Scene BSP node reference invalid");
        for (unsigned i = 3; i < 5; ++i)
            Monstey_SRequire(record.words[i] == UINT32_MAX || record.words[i] < bsp_nodes.size(),
                             "Scene BSP neighbor ordinal invalid");
    }
}
Monstey_Bytes Monstey_WorldScene::Encode(std::uint32_t groupCount) const {
    Validate(groupCount);
    Monstey_Bytes out;
    Monstey_SAppendWords(out, header);
    for (const auto &record : geometry) {
        Monstey_SAppendWords(out, record.words);
        for (const auto *data : {&record.vertices, &record.indices, &record.extras})
            out.insert(out.end(), data->begin(), data->end());
    }
    for (const auto &node : nodes) {
        Monstey_SAppendWords(out, node.words);
        out.insert(out.end(), node.planes.begin(), node.planes.end());
        Monstey_SAppendWords(out, node.geometry);
        Monstey_SAppendWords(out, node.children);
    }
    for (const auto &record : bsp_nodes) {
        Monstey_SAppendWords(out, record.words);
        Monstey_SAppendWords(out, record.nodes);
    }
    return out;
}
Monstey_Json Monstey_WorldScene::Inspect(std::uint32_t groupCount) const {
    Validate(groupCount);
    auto rows = J::array(), secondary = J::array(), roots = J::array();
    const auto parents = Monstey_SParents(*this);
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto &node = nodes[i];
        auto bounds = J::array();
        for (const auto value : Monstey_SBounds(node))
            bounds.as_array().push_back(value);
        if (parents[i] == UINT32_MAX)
            roots.as_array().push_back(static_cast<std::uint64_t>(i));
        rows.as_array().push_back(
            J::object({{"index", static_cast<std::uint64_t>(i)},
                       {"words", Monstey_SWords(node.words)},
                       {"bounds", bounds},
                       {"group", node.words[13]},
                       {"children", Monstey_SWords(node.children)},
                       {"geometry", Monstey_SWords(node.geometry)},
                       {"parent", parents[i]},
                       {"native_runtime_flags_113", 3u | ((node.words[2] & 1) ? 4u : 0u)},
                       {"native_runtime_flags_114", (node.words[2] >> 1) & 255}}));
    }
    for (const auto &record : bsp_nodes)
        secondary.as_array().push_back(
            J::object({{"words", Monstey_SWords(record.words)}, {"nodes", Monstey_SWords(record.nodes)}}));
    return J::object({{"format", "WDSC9.Scene"},
                      {"header", Monstey_SWords(header)},
                      {"nodes", rows},
                      {"bsp_nodes", secondary},
                      {"roots", roots},
                      {"geometry_records", static_cast<std::uint64_t>(geometry.size())},
                      {"bsp_traversal_stack_capacity", header[4]},
                      {"opaque_header_words_preserved", J::array({11u})},
                      {"game_runtime_verified", false}});
}
Monstey_WorldScene Monstey_WorldSceneRouteSingleGroup(const Monstey_WorldScene &donor,
                                                      const std::array<float, 6> &bounds) {
    donor.Validate(1);
    Monstey_SValidateBounds(bounds);
    Monstey_SRequire(donor.header[1] == 1 && donor.header[4] == 1 &&
                         (donor.nodes.size() == 1 || donor.nodes.size() == 2) && donor.bsp_nodes.size() == 1,
                     "Single-group authoring needs a qualified native one/two-node prototype");
    Monstey_SRequire(donor.bsp_nodes[0].words[1] > 2 && donor.bsp_nodes[0].words[3] == UINT32_MAX &&
                         donor.bsp_nodes[0].words[4] == UINT32_MAX,
                     "Single-group authoring needs the verified native BSP leaf (no split neighbors)");
    const auto parents = Monstey_SParents(donor);
    std::size_t leaf = SIZE_MAX, root = SIZE_MAX;
    for (std::size_t i = 0; i < donor.nodes.size(); ++i) {
        const auto &node = donor.nodes[i];
        Monstey_SRequire(
            node.words[1] == 0 && node.planes.empty() && (node.words[2] & 1) != 0 &&
                ((node.words[2] >> 1) & 255) < 5,
            "Single-group prototype has unqualified shared-node field, planes, flags or render category");
        if (node.words[13] == 0 && node.children.empty())
            leaf = i;
        if (parents[i] == UINT32_MAX)
            root = i;
    }
    Monstey_SRequire(
        leaf != SIZE_MAX && root != SIZE_MAX &&
            ((root == leaf && donor.nodes.size() == 1) ||
             (donor.nodes[root].words[13] == UINT32_MAX &&
              donor.nodes[root].children == std::vector<std::uint32_t>{static_cast<std::uint32_t>(leaf)})) &&
            donor.bsp_nodes[0].nodes == std::vector<std::uint32_t>{static_cast<std::uint32_t>(leaf)},
        "Native prototype does not route its root/BSP to the group leaf");
    Monstey_WorldScene scene;
    scene.header = donor.header;
    scene.header[6] = scene.header[7] = scene.header[8] = scene.header[9] = 0;
    scene.nodes = donor.nodes;
    scene.bsp_nodes = donor.bsp_nodes;
    for (auto &node : scene.nodes) {
        node.geometry.clear();
        node.words[11] = 0;
        for (unsigned i = 0; i < 6; ++i)
            node.words[3 + i] = std::bit_cast<std::uint32_t>(bounds[i]);
    }
    scene.Validate(1);
    return scene;
}
} // namespace Monstey