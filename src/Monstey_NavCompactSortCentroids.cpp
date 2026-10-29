#include "Monstey_NavCompactSortCentroids.hpp"
#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <emmintrin.h>
namespace Monstey {
namespace {
__m128 Monstey_CentroidKey(const Monstey_NavHierarchyNode& node,
                         std::uint32_t axis) noexcept {
    const __m128 center = _mm_mul_ps(
        _mm_add_ps(_mm_loadu_ps(node.bounds.maximum.data()),
                   _mm_loadu_ps(node.bounds.minimum.data())), _mm_set1_ps(0.5f));
    const __m128 masks[] = {
        _mm_castsi128_ps(_mm_set_epi32(0, 0, 0, -1)),
        _mm_castsi128_ps(_mm_set_epi32(0, 0, -1, 0)),
        _mm_castsi128_ps(_mm_set_epi32(0, -1, 0, 0)),
        _mm_castsi128_ps(_mm_set_epi32(-1, 0, 0, 0))};
    const __m128 selected = _mm_and_ps(center, masks[axis]);
    const __m128 pair = _mm_or_ps(selected, _mm_shuffle_ps(selected, selected, 0x4e));
    return _mm_or_ps(pair, _mm_shuffle_ps(pair, pair, 0xb1));
}
struct Monstey_CentroidRange { std::ptrdiff_t first, last; };
}
void Monstey_NavCompactSortCentroids(
    std::span<std::uint32_t> indices, std::int32_t native_count,
    std::uint32_t axis, std::span<const Monstey_NavHierarchyNode> nodes) {
    if (native_count <= 1) return;
    const auto count = static_cast<std::size_t>(native_count);
    if (count > indices.size()) throw std::out_of_range("Centroid prefix exceeds owned indices");
    if (axis > 3) throw std::invalid_argument("Centroid axis must be in 0..3");
    for (std::size_t n = 0; n < count; ++n)
        if (indices[n] >= nodes.size())
            throw std::out_of_range("Centroid node index exceeds owned hierarchy");
    std::array<Monstey_CentroidRange, 64> pending{};
    std::size_t stack_size = 0;
    Monstey_CentroidRange active{0, static_cast<std::ptrdiff_t>(native_count) - 1};
    for (;;) {
        const auto pivot = indices[static_cast<std::size_t>(
            active.first + (active.last - active.first) / 2)];
        auto left = active.first;
        auto right = active.last;
        do {
            while (_mm_ucomigt_ss(Monstey_CentroidKey(nodes[pivot], axis),
                   Monstey_CentroidKey(nodes[indices[static_cast<std::size_t>(left)]], axis)) != 0) ++left;
            while (_mm_ucomigt_ss(Monstey_CentroidKey(nodes[indices[static_cast<std::size_t>(right)]], axis),
                   Monstey_CentroidKey(nodes[pivot], axis)) != 0) --right;
            if (right >= left) {
                if (right != left) std::swap(indices[static_cast<std::size_t>(right)],
                                            indices[static_cast<std::size_t>(left)]);
                --right;
                ++left;
            }
        } while (left <= right);
        const bool has_left = active.first < right;
        const bool has_right = left < active.last;
        if (has_left && has_right) {
            // Both subranges shrink by at least one. With an S32 count, the
            // smaller-first strategy requires fewer than 32 pending ranges.
            if (stack_size == pending.size()) throw std::logic_error("Centroid partition stack exhausted");
            if (active.last - left <= right - active.first) {
                pending[stack_size++] = {active.first, right};
                active.first = left;
            } else {
                pending[stack_size++] = {left, active.last};
                active.last = right;
            }
        } else if (has_left) active.last = right;
        else if (has_right) active.first = left;
        else {
            if (stack_size == 0) break;
            active = pending[--stack_size];
        }
    }
}
}
