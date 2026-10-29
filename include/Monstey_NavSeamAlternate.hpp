#pragma once
#include "Monstey_NavSeamGround.hpp"
namespace Monstey {
struct Monstey_NavSeamFaceNormalResult {
  Monstey_NavSeamVector4 normal;
  Monstey_NavSeamVector4 area;
};
// Complete 3BF31C selected face/map/added geometry arithmetic.
// Degenerate faces return the native +Z fallback; W is not normalized.
Monstey_NavSeamFaceNormalResult Monstey_NavSeamFaceNormal(
    const Monstey_NavigationBoundaryMeshView& mesh,std::uint32_t face);
struct Monstey_NavSeamAlternateSettings {
  float gap;              // Native settings+04.
  float plane_slack;      // +08.
  float minimum_overlap;  // +18.
  float face_cosine;      // +24.
  float edge_cosine;      // +28.
};
// Complete ABD350 NULL-callback path selected by AB7ED0 for flag bit0 set.
// AB7ED0 supplies an all-zero projection vector, so this route tests 3D gaps.
Monstey_NavSeamGroundResult Monstey_NavSeamAlternateMatch(
    const Monstey_NavigationBoundaryMeshView& a,
    const Monstey_NavigationBoundaryMeshView& b,
    std::uint32_t face_a,std::uint32_t edge_a,
    std::uint32_t face_b,std::uint32_t edge_b,
    const Monstey_NavSeamAlternateSettings& settings);
}