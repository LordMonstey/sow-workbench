#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
namespace Monstey {
struct Monstey_NavigationUidStep {
  std::uint32_t assigned, next;
};
// Receiver+684 is initialized to1. The selected callback assigns the old raw
// U32 and stores (old+1) modulo32bits, replacing a negative signed result by1.
Monstey_NavigationUidStep
Monstey_NavigationAdvanceUid(std::uint32_t current) noexcept;
// Exact first-nonnull UID lookup leaf5A052C over an owned table prefix.
// A nonpositive signed count returns-1 without reading table entries.
std::int32_t
Monstey_NavigationFindUid(std::span<const std::optional<std::uint32_t>> table,
                          std::int32_t native_count, std::uint32_t uid);
struct Monstey_NavigationStreamConnection {
  std::uint32_t face_a, edge_a, face_b, edge_b;
};
using Monstey_NavigationStreamBox = std::array<float, 8>;
struct Monstey_NavigationStreamSet {
  std::uint32_t uid_a{}, uid_b{};
  std::vector<Monstey_NavigationStreamConnection> connections;
  std::vector<Monstey_NavigationStreamBox> bounds_a, bounds_b;
};
struct Monstey_NavigationCutterSection {
  std::optional<std::uint32_t> mesh_uid;
  bool cutter_present = false;
  std::vector<std::uint8_t> face_flags;
  std::vector<std::uint32_t> face_ids, changed_faces;
  std::vector<Monstey_NavigationStreamBox> bounds;
};
struct Monstey_NavigationStagedSet {
  std::size_t source_set, slot_a, slot_b;
};
struct Monstey_NavigationCutterStage {
  std::vector<Monstey_NavigationCutterSection> sections;
  std::vector<Monstey_NavigationStagedSet> applied;
  std::size_t unresolved{}, unrelated{};
};
// Owned-data reconstruction of successful-construction59EB7C staging loops.
// Replaces the current cutter's lists, then visits source sets in reverse.
// No native allocator/refcount/cutter algorithm or live state is executed.
// No opposite-edge links are written by this stage. Invalid owned ranges throw
// before publication; native malformed-pointer behavior is not reproduced.
Monstey_NavigationCutterStage Monstey_NavigationStageStreamSets(
    std::span<const Monstey_NavigationCutterSection> sections,
    std::uint32_t current_slot,
    std::span<const Monstey_NavigationStreamSet> source_sets);
} // namespace Monstey