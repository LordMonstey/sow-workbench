#include "Monstey_NavSeamBroadphase.hpp"
#include "Monstey_Core.hpp"
#include <bit>
#include <climits>
namespace Monstey {
std::vector<Monstey_NavSeamQuantizedAabb> Monstey_NavSeamSortAabbs(
    std::span<const Monstey_NavSeamQuantizedAabb> input) {
  Monstey_Require(input.size() <= static_cast<std::size_t>(INT_MAX - 3),
      "Native seam AABB count exceeds rounded signed count grammar");
  std::vector<Monstey_NavSeamQuantizedAabb> current(input.begin(), input.end());
  std::vector<Monstey_NavSeamQuantizedAabb> next(input.size());
  for (unsigned shift = 0; shift < 32; shift += 8) {
    std::array<std::size_t, 256> counts{};
    for (const auto& box : current) ++counts[(box.minimum[0] >> shift) & 255u];
    std::array<std::size_t, 256> offsets{};
    for (std::size_t i = 1; i < offsets.size(); ++i)
      offsets[i] = offsets[i - 1] + counts[i - 1];
    for (const auto& box : current)
      next[offsets[(box.minimum[0] >> shift) & 255u]++] = box;
    current.swap(next);
  }
  return current;
}
std::vector<Monstey_NavSeamHitPair> Monstey_NavSeamBroadphase(
    std::span<const Monstey_NavSeamQuantizedAabb> first,
    std::span<const Monstey_NavSeamQuantizedAabb> second) {
  const auto a = Monstey_NavSeamSortAabbs(first);
  const auto b = Monstey_NavSeamSortAabbs(second);
  std::vector<Monstey_NavSeamHitPair> result;
  std::size_t headA = 0, headB = 0;
  auto overlapYZ = [](const Monstey_NavSeamQuantizedAabb& left,
                      const Monstey_NavSeamQuantizedAabb& right) {
    const std::uint32_t signs = (left.maximum[1] - right.minimum[1]) |
        (left.maximum[2] - right.minimum[2]) |
        (right.maximum[1] - left.minimum[1]) |
        (right.maximum[2] - left.minimum[2]);
    return std::bit_cast<std::int32_t>(signs) >= 0;
  };
  while (headA < a.size() && headB < b.size()) {
    // Native SETA uses a strict unsigned comparison: ties select A.
    if (a[headA].minimum[0] > b[headB].minimum[0]) {
      const auto& left = b[headB];
      for (std::size_t i = headA; i < a.size(); ++i) {
        const auto& right = a[i];
        if (right.minimum[0] >= left.maximum[0]) break;
        if (overlapYZ(left, right))
          result.push_back({right.minimum[3], left.minimum[3]});
      }
      ++headB;
    } else {
      const auto& left = a[headA];
      for (std::size_t i = headB; i < b.size(); ++i) {
        const auto& right = b[i];
        if (right.minimum[0] >= left.maximum[0]) break;
        if (overlapYZ(left, right))
          result.push_back({left.minimum[3], right.minimum[3]});
      }
      ++headA;
    }
  }
  return result;
}
}