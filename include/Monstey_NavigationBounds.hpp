#pragma once
#include <array>
namespace Monstey {
using Monstey_NavigationVector4 = std::array<float, 4>;
using Monstey_NavigationAffine4 = std::array<Monstey_NavigationVector4, 4>;
struct Monstey_NavigationAabb {
  Monstey_NavigationVector4 minimum;
  Monstey_NavigationVector4 maximum;
};
// Owned data only. Columns 0..2 multiply XYZ; column 3 translates all four
// lanes. Matches the arithmetic and XYZ-only empty predicate at native RVA
// 0x26b7c8. No unit conversion, homogeneous division, normalization or input
// sanitization.
Monstey_NavigationAabb Monstey_NavigationTransformedAabb(
    const Monstey_NavigationAffine4 &transform,
    const Monstey_NavigationAabb &source,
    const Monstey_NavigationVector4 &expansion = {}) noexcept;
} // namespace Monstey