#pragma once
#include "Monstey_NavigationBounds.hpp"
#include <cstdint>
#include <span>
#include <vector>
#include <utility>
namespace Monstey {
struct Monstey_NavigationCompactTreeView {
    std::span<const std::uint8_t> nodes; // Six-byte nodes, native depth-first layout.
    Monstey_NavigationAabb bounds;
};
struct Monstey_NavigationBoundaryMeshView {
    std::span<const std::uint8_t> faces; // 16-byte original records.
    std::span<const std::uint8_t> edges; // 20-byte original records.
    std::span<const std::uint8_t> vertices; // 16-byte float vectors.
    std::span<const std::int32_t> face_map;
    std::span<const std::int32_t> edge_map;
    std::span<const std::uint8_t> mapped_faces;
    std::span<const std::uint8_t> mapped_edges;
    std::span<const std::uint8_t> added_faces;
    std::span<const std::uint8_t> added_edges;
    std::span<const std::uint8_t> added_vertices;
    Monstey_NavigationAffine4 transform;
};
struct Monstey_NavigationBoundaryHit {
    std::uint32_t face_key;
    std::uint32_t edge_key;
    bool operator==(const Monstey_NavigationBoundaryHit&) const = default;
};
struct Monstey_NavigationOrderedAabb {
    std::array<std::uint32_t,4> minimum;
    std::array<std::uint32_t,4> maximum;
    bool operator==(const Monstey_NavigationOrderedAabb&) const = default;
};
struct Monstey_NavigationBoundaryQueryResult {
    Monstey_NavigationAabb local_query;
    std::vector<std::uint32_t> raw_tree_leaves;
    std::vector<std::uint32_t> masked_face_keys;
    std::vector<Monstey_NavigationBoundaryHit> hits; // Includes non-boundary hits.
    std::vector<Monstey_NavigationOrderedAabb> boundary_boxes; // minimum[3] indexes hits.
};
// All views describe owned bytes for the duration of the call; no process pointers.
// Malformed owned indices throw instead of reproducing unsafe native accesses.
std::span<const std::uint8_t> Monstey_NavigationBoundaryFace(
    const Monstey_NavigationBoundaryMeshView& mesh, std::uint32_t index);
std::span<const std::uint8_t> Monstey_NavigationBoundaryEdge(
    const Monstey_NavigationBoundaryMeshView& mesh, std::uint32_t index);
Monstey_NavigationVector4 Monstey_NavigationBoundaryWorldVertex(
    const Monstey_NavigationBoundaryMeshView& mesh, std::int32_t index);
std::pair<Monstey_NavigationVector4,Monstey_NavigationVector4>
Monstey_NavigationBoundaryEndpoints(const Monstey_NavigationBoundaryMeshView& mesh,
    std::uint32_t edge_index);
std::vector<std::uint32_t> Monstey_NavigationCompactTreeQuery(
    const Monstey_NavigationCompactTreeView& tree, const Monstey_NavigationAabb& query);
bool Monstey_NavigationSegmentIntersectsAabb(
    const Monstey_NavigationVector4& a, const Monstey_NavigationVector4& b,
    const Monstey_NavigationAabb& query) noexcept;
Monstey_NavigationOrderedAabb Monstey_NavigationEncodeOrderedAabb(
    const Monstey_NavigationAabb& box, std::uint32_t hit_index) noexcept;
Monstey_NavigationBoundaryQueryResult Monstey_NavigationBoundaryQuery(
    const Monstey_NavigationBoundaryMeshView& mesh,
    const Monstey_NavigationCompactTreeView& tree,
    const Monstey_NavigationAabb& world_query,
    const Monstey_NavigationVector4& edge_expansion = {});
}