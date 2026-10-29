#include "Monstey_NavigationStreaming.hpp"
#include "Monstey_Core.hpp"
#include <algorithm>
#include <climits>
#include <unordered_set>
namespace Monstey {
Monstey_NavigationUidStep
Monstey_NavigationAdvanceUid(std::uint32_t current) noexcept {
  const std::uint32_t incremented = current + 1u;
  return {current, incremented & 0x80000000u ? 1u : incremented};
}
std::int32_t
Monstey_NavigationFindUid(std::span<const std::optional<std::uint32_t>> table,
                          std::int32_t native_count, std::uint32_t uid) {
  if (native_count <= 0)
    return -1;
  Monstey_Require(static_cast<std::size_t>(native_count) <= table.size(),
                  "Native UID table prefix exceeds owned entries");
  for (std::int32_t i = 0; i < native_count; ++i)
    if (table[static_cast<std::size_t>(i)] &&
        *table[static_cast<std::size_t>(i)] == uid)
      return i;
  return -1;
}
namespace {
void Monstey_StreamingAppendable(std::size_t have, std::size_t add) {
  Monstey_Require(have <= INT_MAX &&
                      add <= static_cast<std::size_t>(INT_MAX) - have,
                  "Streaming stage array exceeds native signed count");
}
} // namespace
Monstey_NavigationCutterStage Monstey_NavigationStageStreamSets(
    std::span<const Monstey_NavigationCutterSection> sections,
    std::uint32_t current_slot,
    std::span<const Monstey_NavigationStreamSet> source_sets) {
  Monstey_Require(sections.size() <= INT_MAX && current_slot < sections.size(),
                  "Current streaming section exceeds owned table");
  Monstey_Require(sections[current_slot].mesh_uid.has_value(),
                  "Current streaming section has no mesh UID");
  Monstey_Require(source_sets.size() <= INT_MAX,
                  "Streaming descriptor count exceeds native signed width");
  Monstey_NavigationCutterStage out;
  out.sections.assign(sections.begin(), sections.end());
  auto &current = out.sections[current_slot];
  // 479210 creates a fresh cutter but does not clear instance face flags.
  current.cutter_present = true;
  current.face_ids.clear();
  current.changed_faces.clear();
  current.bounds.clear();
  const auto current_uid = *current.mesh_uid;
  std::vector<std::optional<std::uint32_t>> uids;
  uids.reserve(sections.size());
  for (const auto &section : sections)
    uids.push_back(section.mesh_uid);
  std::unordered_set<std::uint32_t> changed_seen;
  for (std::size_t index = source_sets.size(); index-- > 0;) {
    const auto &set = source_sets[index];
    const bool from_a = set.uid_a == current_uid;
    if (!from_a && set.uid_b != current_uid) {
      ++out.unrelated;
      continue;
    }
    const auto other_uid = from_a ? set.uid_b : set.uid_a;
    const auto other = Monstey_NavigationFindUid(
        uids, static_cast<std::int32_t>(uids.size()), other_uid);
    if (other == -1) {
      ++out.unresolved;
      continue;
    }
    const auto slot_a = from_a ? static_cast<std::size_t>(current_slot)
                               : static_cast<std::size_t>(other);
    const auto slot_b = from_a ? static_cast<std::size_t>(other)
                               : static_cast<std::size_t>(current_slot);
    auto &a = out.sections[slot_a];
    auto &b = out.sections[slot_b];
    Monstey_Require(a.cutter_present && b.cutter_present,
                    "Resolved section has no owned cutter");
    Monstey_StreamingAppendable(a.face_ids.size(), set.connections.size());
    Monstey_StreamingAppendable(b.face_ids.size(), set.connections.size());
    if (slot_a == slot_b)
      Monstey_StreamingAppendable(a.face_ids.size(),
                                  set.connections.size() * 2);
    Monstey_StreamingAppendable(a.bounds.size(), set.bounds_a.size());
    Monstey_StreamingAppendable(b.bounds.size(), set.bounds_b.size());
    if (slot_a == slot_b) {
      Monstey_StreamingAppendable(set.bounds_a.size(), set.bounds_b.size());
      Monstey_StreamingAppendable(a.bounds.size(),
                                  set.bounds_a.size() + set.bounds_b.size());
    }
    // Owned-range qualification. No new live pointer safety is inferred.
    for (const auto &connection : set.connections)
      Monstey_Require(connection.face_a < a.face_flags.size() &&
                          connection.face_b < b.face_flags.size(),
                      "Streaming face index exceeds owned flags");
    for (const auto &connection : set.connections) {
      a.face_ids.push_back(connection.face_a);
      b.face_ids.push_back(connection.face_b);
      a.face_flags[connection.face_a] |= 4u;
      b.face_flags[connection.face_b] |= 4u;
    }
    auto &changed = out.sections[current_slot].changed_faces;
    for (const auto &connection : set.connections) {
      const auto face = from_a ? connection.face_a : connection.face_b;
      if (changed_seen.insert(face).second) {
        Monstey_StreamingAppendable(changed.size(), 1);
        changed.push_back(face);
      }
    }
    a.bounds.insert(a.bounds.end(), set.bounds_a.begin(), set.bounds_a.end());
    b.bounds.insert(b.bounds.end(), set.bounds_b.begin(), set.bounds_b.end());
    out.applied.push_back({index, slot_a, slot_b});
  }
  return out;
}
} // namespace Monstey