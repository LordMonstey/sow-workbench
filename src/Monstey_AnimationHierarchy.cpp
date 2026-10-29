#include "Monstey_AnimationHierarchy.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace Monstey
{
namespace
{
using Monstey_Four = std::array<float, 4>;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
Monstey_Four Monstey_HierarchyAdd(const Monstey_Four &a, const Monstey_Four &b)
{
    Monstey_Four r{};
    for (unsigned i = 0; i < 4; ++i)
        r[i] = a[i] + b[i];
    return r;
}
Monstey_Four Monstey_HierarchyMul(const Monstey_Four &a, const Monstey_Four &b)
{
    Monstey_Four r{};
    for (unsigned i = 0; i < 4; ++i)
        r[i] = a[i] * b[i];
    return r;
}
Monstey_Four Monstey_HierarchyShuffle(const Monstey_Four &a, unsigned mask)
{
    Monstey_Four r{};
    for (unsigned i = 0; i < 4; ++i)
        r[i] = a[(mask >> (2 * i)) & 3u];
    return r;
}
// a is the first native SIMD operand, b the second; the algebra is b*a.
// z_first selects only the independently verified inner rotation grouping.
Monstey_Four Monstey_HierarchyMultiply(const Monstey_Four &a, const Monstey_Four &b, bool z_first)
{
    constexpr Monstey_Four sign1{1, -1, 1, -1}, sign2{1, 1, -1, -1}, sign3{-1, 1, 1, -1};
    const auto reverse = Monstey_HierarchyShuffle(a, 0x1b);
    const auto middle = Monstey_HierarchyShuffle(reverse, 0xb1);
    const auto x = Monstey_HierarchyMul(sign1, Monstey_HierarchyMul(Monstey_HierarchyShuffle(b, 0), reverse));
    const auto y =
        Monstey_HierarchyMul(Monstey_HierarchyMul(Monstey_HierarchyShuffle(b, 0x55), middle), sign2);
    const auto z = Monstey_HierarchyMul(
        Monstey_HierarchyMul(Monstey_HierarchyShuffle(middle, 0x1b), Monstey_HierarchyShuffle(b, 0xaa)),
        sign3);
    const auto w = Monstey_HierarchyMul(Monstey_HierarchyShuffle(b, 0xff), a);
    return Monstey_HierarchyAdd(z_first ? Monstey_HierarchyAdd(z, y) : Monstey_HierarchyAdd(y, z),
                                Monstey_HierarchyAdd(w, x));
}
Monstey_Four Monstey_HierarchyRead(const Monstey_HierarchyPoseWords &p, unsigned offset)
{
    Monstey_Four r{};
    for (unsigned i = 0; i < 4; ++i)
        r[i] = std::bit_cast<float>(p[offset + i]);
    return r;
}
} // namespace
Monstey_HierarchyPoseWords Monstey_HierarchyComposeF32(const Monstey_HierarchyPoseWords &parent,
                                                       const Monstey_HierarchyPoseWords &local)
{
    const auto parent_q = Monstey_HierarchyRead(parent, 4);
    const auto child_q = Monstey_HierarchyRead(local, 4);
    auto child_p = Monstey_HierarchyRead(local, 0);
    child_p[3] = 0.0f; // ANDPS at 0x14034F56B, irrespective of the incoming W word.
    constexpr Monstey_Four conjugate{-1, -1, -1, 1};
    const auto inner = Monstey_HierarchyMultiply(Monstey_HierarchyMul(conjugate, parent_q), child_p, true);
    const auto position = Monstey_HierarchyAdd(Monstey_HierarchyMultiply(inner, parent_q, false),
                                               Monstey_HierarchyRead(parent, 0));
    const auto quaternion = Monstey_HierarchyMultiply(child_q, parent_q, false);
    Monstey_HierarchyPoseWords result{};
    for (unsigned i = 0; i < 4; ++i)
    {
        result[i] = std::bit_cast<std::uint32_t>(position[i]);
        result[4 + i] = std::bit_cast<std::uint32_t>(quaternion[i]);
    }
    return result;
}
void Monstey_HierarchyEvaluateF32(std::span<const std::uint8_t> parents,
                                  std::span<const Monstey_HierarchyPoseWords> local,
                                  const Monstey_HierarchyPoseWords &supplied_root, std::uint32_t native_mode,
                                  std::span<Monstey_HierarchyPoseWords> output)
{
    if (parents.size() != local.size() || output.size() != local.size() || local.size() > 256)
        throw std::invalid_argument("Native hierarchy requires matching spans and at most 256 records");
    if (local.empty())
        return;
    if (parents[0] != 0xff)
        throw std::invalid_argument("Only ordinal zero may have the native root parent 0xff");
    for (std::size_t i = 1; i < parents.size(); ++i)
        if (parents[i] == 0xff || parents[i] >= i)
            throw std::invalid_argument("Every native hierarchy parent must precede its child");
    std::array<Monstey_HierarchyPoseWords, 256> temporary{};
    temporary[0] = native_mode == 2 ? local[0] : supplied_root;
    for (std::size_t i = 1; i < local.size(); ++i)
        temporary[i] = Monstey_HierarchyComposeF32(temporary[parents[i]], local[i]);
    std::copy_n(temporary.begin(), local.size(), output.begin());
}
void Monstey_HierarchyResetOrdinalZero(std::span<Monstey_HierarchyPoseWords> global)
{
    if (!global.empty())
        global[0] = {0, 0, 0, 0, 0, 0, 0, 0x3f800000};
}
} // namespace Monstey
