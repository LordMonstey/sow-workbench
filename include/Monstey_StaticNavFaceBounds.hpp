#pragma once
#include <array>
#include <cstdint>
#include <span>
namespace Monstey {
struct Monstey_StaticNavFaceAabb {
    std::array<float,4> minimum;
    std::array<float,4> maximum;
    std::int32_t first_edge;
    std::int16_t edge_count;
    std::uint32_t vertex_iterations;
};
// Owned, relocated array bytes only. No pointer or original engine code is used.
// The loop is signed first < wrap32(first + sign16(count)).
// A skipped loop returns native sentinel bounds with zero edge/vertex reads.
// Entered ranges require valid owned indices; invalid native loads are not run.
Monstey_StaticNavFaceAabb Monstey_StaticNavFaceBounds(
    std::span<const std::uint8_t> faces,
    std::span<const std::uint8_t> edges,
    std::span<const std::uint8_t> vertices,
    std::uint32_t face_index,
    const std::array<float,4>& padding = {});
}
