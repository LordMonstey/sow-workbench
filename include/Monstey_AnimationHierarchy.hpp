#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace Monstey
{
// Native 32-byte position XYZW followed by quaternion XYZW, as raw F32 words.
using Monstey_HierarchyPoseWords = std::array<std::uint32_t, 8>;

// Native 0x14034F660 followed by 0x14034F540 and ADDPS, without normalization.
// Finite IEEE-754 inputs, round-to-nearest, gradual underflow, /fp:strict.
Monstey_HierarchyPoseWords Monstey_HierarchyComposeF32(const Monstey_HierarchyPoseWords &parent,
                                                       const Monstey_HierarchyPoseWords &local);

// Native basic hierarchy 0x1401B173C. Parent ordinals are SKEL runtime order.
// Only ordinal zero has parent 0xff; every other parent precedes its child.
// Native mode 2 copies sampled root; all other modes copy supplied_root.
// The separate later controller reset is never applied implicitly.
// Exact-size output is committed only after the whole bounded tree validates.
void Monstey_HierarchyEvaluateF32(std::span<const std::uint8_t> parents,
                                  std::span<const Monstey_HierarchyPoseWords> local,
                                  const Monstey_HierarchyPoseWords &supplied_root, std::uint32_t native_mode,
                                  std::span<Monstey_HierarchyPoseWords> output);

// Only the ordinal-zero stores at 0x14025166A..0x14025167C.
// Does not recompute descendants or implement controller root motion.
void Monstey_HierarchyResetOrdinalZero(std::span<Monstey_HierarchyPoseWords> global);
} // namespace Monstey
