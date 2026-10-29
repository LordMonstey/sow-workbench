#pragma once
#include "Monstey_NavigationBoundaryQuery.hpp"
#include "Monstey_NavSeamGeometry.hpp"
namespace Monstey {
// Native AB7ED0 selected ground branch, whose optional callback context is NULL.
// Offsets are the native float fields in the pair's settings+0x50 block.
// Cosine thresholds are explicit supplied values; no native function is called.
struct Monstey_NavSeamGroundSettings {
  float step_height;          // +00
  float horizontal_gap;       // +04
  float plane_slack;          // +08
  float face_support;         // +0c
  float horizontal_cosine;    // +10
  float direction_cosine;     // +14
  float minimum_overlap;      // +18
};
struct Monstey_NavSeamCandidate {
  std::uint32_t edge_a, edge_b, face_a, face_b;
  float score, horizontal_cosine, overlap;
  bool enabled;
  std::array<float, 4> intervals;
};
struct Monstey_NavSeamGroundResult {
  int status;
  Monstey_NavSeamCandidate candidate; // Meaningful only when status is -1.
};
// Reconstructs ABDFE0's NULL-callback observable status/candidate fields.
// Uses the exact native mesh selectors and original four-lane geometry.
// This is an edge-pair test, not the full two-pass 557F20 matcher.
Monstey_NavSeamGroundResult Monstey_NavSeamGroundMatch(
    const Monstey_NavigationBoundaryMeshView& a,
    const Monstey_NavigationBoundaryMeshView& b,
    std::uint32_t face_a, std::uint32_t edge_a,
    std::uint32_t face_b, std::uint32_t edge_b,
    const Monstey_NavSeamVector4& up,
    const Monstey_NavSeamGroundSettings& settings);
}