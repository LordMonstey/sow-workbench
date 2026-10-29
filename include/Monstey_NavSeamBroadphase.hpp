#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>
namespace Monstey {
// Native 32-byte 1FE5D8 output. minimum[3] carries the original hit ordinal.
struct Monstey_NavSeamQuantizedAabb {
  std::array<std::uint32_t, 4> minimum;
  std::array<std::uint32_t, 4> maximum;
};
struct Monstey_NavSeamHitPair {
  std::uint32_t first;
  std::uint32_t second;
  bool operator==(const Monstey_NavSeamHitPair&) const = default;
};
// Equivalent to the valid owned-array sorting result of 556EAC -> 5BDF80.
// Stable unsigned minimum-X radix order, retaining every other bit.
std::vector<Monstey_NavSeamQuantizedAabb> Monstey_NavSeamSortAabbs(
    std::span<const Monstey_NavSeamQuantizedAabb> input);
// Full 556B18 / repeated 557EF0 + caller advance sequence on sorted arrays.
// Returns original hit ordinals, not compact boundary-array positions.
// Strict X overlap and inclusive Y/Z overlap use native wrapping-U32 tests.
// This is the broadphase only; it does not create a navmesh connection.
std::vector<Monstey_NavSeamHitPair> Monstey_NavSeamBroadphase(
    std::span<const Monstey_NavSeamQuantizedAabb> first,
    std::span<const Monstey_NavSeamQuantizedAabb> second);
}