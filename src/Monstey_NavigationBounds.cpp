#include "Monstey_NavigationBounds.hpp"
#include <immintrin.h>
namespace Monstey {
Monstey_NavigationAabb Monstey_NavigationTransformedAabb(
    const Monstey_NavigationAffine4 &transform,
    const Monstey_NavigationAabb &source,
    const Monstey_NavigationVector4 &expansion) noexcept {
  const __m128 low = _mm_loadu_ps(source.minimum.data());
  const __m128 high = _mm_loadu_ps(source.maximum.data());
  if ((_mm_movemask_ps(_mm_cmplt_ps(high, low)) & 7) != 0)
    return source;
  const __m128 sign =
      _mm_castsi128_ps(_mm_set1_epi32(static_cast<int>(0x80000000u)));
  const __m128 half = _mm_set1_ps(0.5f);
  const __m128 center = _mm_mul_ps(_mm_add_ps(low, high), half);
  const __m128 extent = _mm_mul_ps(_mm_sub_ps(high, low), half);
  const __m128 column0 = _mm_loadu_ps(transform[0].data());
  const __m128 column1 = _mm_loadu_ps(transform[1].data());
  const __m128 column2 = _mm_loadu_ps(transform[2].data());
  __m128 radius = _mm_add_ps(
      _mm_andnot_ps(sign,
                    _mm_mul_ps(_mm_shuffle_ps(extent, extent, 0x00), column0)),
      _mm_andnot_ps(sign,
                    _mm_mul_ps(_mm_shuffle_ps(extent, extent, 0x55), column1)));
  const __m128 radius2 = _mm_add_ps(
      _mm_andnot_ps(sign,
                    _mm_mul_ps(_mm_shuffle_ps(extent, extent, 0xaa), column2)),
      _mm_loadu_ps(expansion.data()));
  radius = _mm_add_ps(radius, radius2);
  __m128 transformed =
      _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(center, center, 0x00), column0),
                 _mm_mul_ps(_mm_shuffle_ps(center, center, 0x55), column1));
  transformed = _mm_add_ps(
      transformed, _mm_mul_ps(_mm_shuffle_ps(center, center, 0xaa), column2));
  transformed = _mm_add_ps(transformed, _mm_loadu_ps(transform[3].data()));
  Monstey_NavigationAabb result{};
  _mm_storeu_ps(result.minimum.data(),
                _mm_add_ps(_mm_xor_ps(radius, sign), transformed));
  _mm_storeu_ps(result.maximum.data(), _mm_add_ps(transformed, radius));
  return result;
}
} // namespace Monstey