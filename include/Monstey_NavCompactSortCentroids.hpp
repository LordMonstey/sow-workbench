#pragma once
#include "Monstey_NavCompactCookerStages.hpp"
#include <cstdint>
#include <span>
namespace Monstey {
// Native A72EC0: ascending, unstable in-place centroid sort of an owned prefix.
// Centers use (maximum + minimum) * 0.5f, with native packed-SSE arithmetic.
// count <= 1 returns without reading indices, axis, or nodes. Axis 0..3 is
// qualified; A74AB0 uses 0..2. For count > 1, invalid owned ranges/indices/axis
// throw before mutation. The source hierarchy, including topology, is unchanged.
void Monstey_NavCompactSortCentroids(
    std::span<std::uint32_t> indices, std::int32_t native_count,
    std::uint32_t axis, std::span<const Monstey_NavHierarchyNode> nodes);
}
