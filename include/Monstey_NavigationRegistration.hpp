#pragma once
#include "Monstey_NavigationStreaming.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>
namespace Monstey {
struct Monstey_NavigationRegistrationBucket {
  // Fresh allocated empties use an owned deterministic hash placeholder0.
  // Native allocation writes only record=-1; that fresh hash is unspecified.
  // Initial {0,-2} and occupied/stale erased hashes are native-qualified.
  std::uint32_t hash = 0;
  std::int32_t record =
      -2; // Native initial static sentinel; allocated empties use -1.
};
struct Monstey_NavigationRegistrationRecord {
  std::uint32_t opposite_slot = 0;
  std::optional<std::size_t> wrapper;
};
struct Monstey_NavigationRegistrationRow {
  std::optional<std::uint32_t> mesh_uid;
  std::vector<Monstey_NavigationRegistrationRecord> records;
  std::vector<Monstey_NavigationRegistrationBucket> buckets{{0, -2}};
};
struct Monstey_NavigationRegisteredSet {
  std::uint32_t slot_a = 0, slot_b = 0;
  std::size_t source_set = 0;
  std::uint32_t references = 0;
};
struct Monstey_NavigationRegistrationState {
  std::vector<Monstey_NavigationRegistrationRow> rows;
  std::vector<std::optional<Monstey_NavigationRegisteredSet>> wrappers;
  // Owned wrapper references to each supplied source, not actual engine
  // refcounts.
  std::vector<std::uint32_t> source_references;
};
enum class Monstey_NavigationRegistrationEventKind {
  wrapper_created,
  source_retained,
  wrapper_retained,
  wrapper_released,
  source_released,
  wrapper_destroyed
};
struct Monstey_NavigationRegistrationEvent {
  Monstey_NavigationRegistrationEventKind kind{};
  std::size_t wrapper = 0, source_set = 0;
  std::uint32_t references = 0;
};
struct Monstey_NavigationRegistrationResult {
  Monstey_NavigationRegistrationState state;
  std::vector<Monstey_NavigationRegistrationEvent> events;
  std::int32_t slot_a = -1, slot_b = -1;
  bool registered = false;
};
std::uint32_t Monstey_NavigationSlotHash(std::uint32_t slot) noexcept;
// Exact successful owned-data478C50: first-match UID resolution, endpoint
// guard, fresh wrapper, A-then-B opposite-slot map upserts, final local
// release. Source UIDs are retained unchanged. No engine callbacks are
// executed; events record qualified semantic ownership operations in their
// native order.
Monstey_NavigationRegistrationResult Monstey_NavigationRegisterStreamSet(
    const Monstey_NavigationRegistrationState &state,
    std::span<const Monstey_NavigationStreamSet> sources,
    std::uint32_t current_slot, std::size_t source_set);
// Exact successful owned-data5A003C and descriptor-map removal dependencies.
// Reverse current-record traversal, reciprocal swap-removal, then reverse
// clear. Mesh residency, spatial index and cutter geometry are deliberately not
// changed by this function, as they are not changed by native5A003C either.
Monstey_NavigationRegistrationResult Monstey_NavigationUnmapStreamSets(
    const Monstey_NavigationRegistrationState &state,
    std::span<const Monstey_NavigationStreamSet> sources,
    std::uint32_t current_slot);
} // namespace Monstey
