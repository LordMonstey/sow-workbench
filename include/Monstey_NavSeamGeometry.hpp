#pragma once
#include <array>
namespace Monstey {
using Monstey_NavSeamVector4 = std::array<float, 4>;
using Monstey_NavSeamSegments = std::array<Monstey_NavSeamVector4, 4>;
struct Monstey_NavSeamClosestResult {
  Monstey_NavSeamVector4 delta;
  Monstey_NavSeamVector4 squared_distance;
};
// Complete owned-vector arithmetic from AC1F80 / AC2340.
// Input order A0,A1,B0,B1. Dot products use XYZ; delta retains W.
// Uses the native reciprocal estimate/refinement and clamp operand order.
Monstey_NavSeamClosestResult Monstey_NavSeamClosestSegments(
    const Monstey_NavSeamSegments& segments) noexcept;
// Complete AC1560 projection/overlap calculation on explicit clipped endpoints.
// Normal inputs, up and minimum_overlap are caller-provided, never guessed.
// Returns native status -1, 3 or 4 and replicated four-lane projection values.
struct Monstey_NavSeamOverlapResult {
  int status;
  std::array<Monstey_NavSeamVector4, 4> projections;
  float overlap;
  bool overlap_written;
};
Monstey_NavSeamOverlapResult Monstey_NavSeamProjectedOverlap(
    const Monstey_NavSeamSegments& clipped,
    const Monstey_NavSeamVector4& normal_a,
    const Monstey_NavSeamVector4& normal_b,
    const Monstey_NavSeamVector4& up,
    float minimum_overlap) noexcept;
}