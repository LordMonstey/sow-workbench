#pragma once
#include "Monstey_NavSeamGround.hpp"
#include "Monstey_NavSeamAlternate.hpp"
#include "Monstey_NavSeamBroadphase.hpp"
#include <vector>
namespace Monstey {
struct Monstey_NavSeamConnection {
  std::uint32_t face_a, edge_a, face_b, edge_b;
  bool operator==(const Monstey_NavSeamConnection&) const = default;
};
static_assert(sizeof(Monstey_NavSeamConnection)==16);
// Complete native 761154 ordering and AB8EF0 interval conflict passes.
// Candidate edge/face B keys carry bit22, as written by 557F20 before sorting.
std::vector<Monstey_NavSeamCandidate> Monstey_NavSeamResolveCandidates(
    std::span<const Monstey_NavSeamCandidate> candidates);
struct Monstey_NavSeamGroundMatchResult {
  Monstey_NavigationBoundaryQueryResult boundary_a, boundary_b;
  std::vector<Monstey_NavSeamHitPair> broadphase;
  std::vector<Monstey_NavSeamConnection> connections;
  std::vector<Monstey_NavSeamCandidate> candidates;
  std::uint32_t coincident_connections;
};
// Selected 557F20 principal ground route: both mesh flags&1==0 and pair.Q88==0.
// The native caller provides these settings, source bounds and runtime-built trees.
// No tree cooker, serialized streaming-set writer or live engine call is implied.
Monstey_NavSeamGroundMatchResult Monstey_NavSeamMatchGroundMeshes(
    const Monstey_NavigationBoundaryMeshView& a,
    const Monstey_NavigationBoundaryMeshView& b,
    const Monstey_NavigationAabb& source_bounds_a,
    const Monstey_NavigationAabb& source_bounds_b,
    const Monstey_NavigationCompactTreeView& tree_a,
    const Monstey_NavigationCompactTreeView& tree_b,
    const Monstey_NavSeamVector4& up,
    float coincident_tolerance,
    const Monstey_NavSeamGroundSettings& settings);
struct Monstey_NavSeamSettings {
  Monstey_NavSeamGroundSettings ground;
  float face_cosine;             // Native +24.
  float alternate_edge_cosine;   // Native +28.
};
// Complete AB7ED0 flag0 dispatch and mixed-result combination, Q88 NULL profile.
Monstey_NavSeamGroundResult Monstey_NavSeamMatchEdgePair(
    const Monstey_NavigationBoundaryMeshView& a,
    const Monstey_NavigationBoundaryMeshView& b,
    std::uint32_t face_a,std::uint32_t edge_a,
    std::uint32_t face_b,std::uint32_t edge_b,
    std::uint8_t flags_a,std::uint8_t flags_b,
    const Monstey_NavSeamVector4& up,
    const Monstey_NavSeamSettings& settings);
// Full 557F20 ground/alternate/mixed owned-mesh pipeline, pair.Q88 NULL.
// Settings are explicit native values; no original callback or solver is called.
Monstey_NavSeamGroundMatchResult Monstey_NavSeamMatchMeshes(
    const Monstey_NavigationBoundaryMeshView& a,
    const Monstey_NavigationBoundaryMeshView& b,
    const Monstey_NavigationAabb& source_bounds_a,
    const Monstey_NavigationAabb& source_bounds_b,
    const Monstey_NavigationCompactTreeView& tree_a,
    const Monstey_NavigationCompactTreeView& tree_b,
    std::uint8_t flags_a,std::uint8_t flags_b,
    const Monstey_NavSeamVector4& up,
    float coincident_tolerance,
    const Monstey_NavSeamSettings& settings);
}