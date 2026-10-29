#pragma once
#include <bit>
#include <cstdint>
#include <immintrin.h>
namespace Monstey_ValidateReference {
inline bool LowerPass(std::uint16_t status) {
    const auto ah = static_cast<unsigned>(status >> 8);
    return ((ah & 1u) != 0) == ((ah & 4u) != 0);
}
inline bool UpperPass(std::uint16_t status) { return ((status >> 8) & 0x41u) != 0; }
inline std::uint16_t NormalMasked(std::uint64_t value, std::uint32_t bound) {
    const bool nv = (value & 0x7FF0000000000000ull) == 0x7FF0000000000000ull &&
                    (value & 0xFFFFFFFFFFFFFull) != 0;
    const bool nb = (bound & 0x7F800000u) == 0x7F800000u && (bound & 0x7FFFFFu) != 0;
    if (nv || nb)
        return 0x4501u;
    const auto saved = _mm_getcsr();
    _mm_setcsr(0x1F80u);
    volatile double x = std::bit_cast<double>(value);
    volatile float y0 = std::bit_cast<float>(bound);
    volatile double y = static_cast<double>(y0);
    const std::uint16_t result = x < y ? 0x0100u : x > y ? 0u : 0x4000u;
    _mm_setcsr(saved);
    return result;
}
} // namespace Monstey_ValidateReference
