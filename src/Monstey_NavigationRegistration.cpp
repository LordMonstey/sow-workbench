#include "Monstey_NavigationRegistration.hpp"
#include "Monstey_Core.hpp"
#include <algorithm>
#include <bit>
#include <climits>
namespace Monstey {
std::uint32_t Monstey_NavigationSlotHash(std::uint32_t slot) noexcept {
  const auto n = slot * 0x9e3779b1u;
  return ((n & 0xffu) << 24) | ((n & 0xff00u) << 8) | ((n >> 8) & 0xff00u) |
         (n >> 24);
}
namespace {
using State = Monstey_NavigationRegistrationState;
using Row = Monstey_NavigationRegistrationRow;
using Result = Monstey_NavigationRegistrationResult;
using Kind = Monstey_NavigationRegistrationEventKind;
std::size_t Monstey_Mask(const Row &row) { return row.buckets.size() - 1; }
std::size_t Monstey_FindBucket(const Row &row, std::uint32_t key) {
  const auto mask = Monstey_Mask(row);
  auto at = static_cast<std::size_t>(Monstey_NavigationSlotHash(key)) & mask;
  for (std::size_t n = 0; n < row.buckets.size(); ++n) {
    const auto index = row.buckets[at].record;
    if (index < 0 ||
        row.records[static_cast<std::size_t>(index)].opposite_slot == key)
      return at;
    at = (at + 1) & mask;
  }
  throw std::runtime_error("Registration map has no terminating bucket");
}
void Monstey_Validate(const State &state, std::size_t source_count) {
  Monstey_Require(state.source_references.size() == source_count,
                  "Registration source-reference table differs");
  std::vector<std::uint32_t> refs(state.wrappers.size()), sources(source_count);
  for (const auto &row : state.rows) {
    const auto size = row.buckets.size();
    Monstey_Require(size > 0 && (size & (size - 1)) == 0,
                    "Registration bucket extent is not a nonzero power of two");
    Monstey_Require(row.records.size() < static_cast<std::size_t>(INT_MAX / 3),
                    "Registration count exceeds native signed growth grammar");
    std::vector<bool> seen(row.records.size());
    bool empty = false;
    for (const auto &b : row.buckets) {
      if (b.record < 0) {
        empty = true;
        continue;
      }
      const auto at = static_cast<std::size_t>(b.record);
      Monstey_Require(at < row.records.size() && !seen[at],
                      "Registration bucket index is outside unique records");
      seen[at] = true;
      Monstey_Require(
          b.hash == Monstey_NavigationSlotHash(row.records[at].opposite_slot),
          "Registration bucket hash differs");
    }
    Monstey_Require(
        empty &&
            std::all_of(seen.begin(), seen.end(), [](bool b) { return b; }),
        "Registration records lack complete terminating hash table");
    for (std::size_t n = 0; n < row.records.size(); ++n) {
      const auto &r = row.records[n];
      Monstey_Require(r.opposite_slot < state.rows.size(),
                      "Registration opposite slot outside owned rows");
      Monstey_Require(r.wrapper && *r.wrapper < state.wrappers.size() &&
                          state.wrappers[*r.wrapper],
                      "Registration wrapper outside live owned objects");
      Monstey_Require(
          row.buckets[Monstey_FindBucket(row, r.opposite_slot)].record ==
              static_cast<std::int32_t>(n),
          "Registration key is duplicate or unreachable");
      ++refs[*r.wrapper];
    }
  }
  for (std::size_t n = 0; n < state.wrappers.size(); ++n) {
    if (!state.wrappers[n]) {
      Monstey_Require(refs[n] == 0,
                      "Destroyed registration wrapper remains referenced");
      continue;
    }
    const auto &w = *state.wrappers[n];
    Monstey_Require(w.source_set < source_count &&
                        w.slot_a < state.rows.size() &&
                        w.slot_b < state.rows.size(),
                    "Registered set source/endpoints outside owned data");
    Monstey_Require(w.references > 0 && w.references == refs[n],
                    "Registration wrapper reference count differs");
    ++sources[w.source_set];
  }
  Monstey_Require(sources == state.source_references,
                  "Registration source reference counts differ");
}
void Monstey_Event(Result &result, Kind kind, std::size_t id,
                   std::uint32_t refs) {
  result.events.push_back(
      {kind, id, result.state.wrappers[id]->source_set, refs});
}
void Monstey_Retain(Result &result, std::size_t id) {
  auto &w = *result.state.wrappers[id];
  ++w.references;
  Monstey_Event(result, Kind::wrapper_retained, id, w.references);
}
void Monstey_Release(Result &result, std::size_t id) {
  auto &w = *result.state.wrappers[id];
  Monstey_Require(w.references > 0, "Registration release underflow");
  --w.references;
  Monstey_Event(result, Kind::wrapper_released, id, w.references);
  if (!w.references) {
    const auto source = w.source_set;
    --result.state.source_references[source];
    Monstey_Event(result, Kind::source_released, id,
                  result.state.source_references[source]);
    Monstey_Event(result, Kind::wrapper_destroyed, id, 0);
    result.state.wrappers[id].reset();
  }
}
void Monstey_Grow(Row &row, std::size_t requested) {
  std::size_t count = 8;
  while (count < requested * 2) {
    Monstey_Require(count <= static_cast<std::size_t>(INT_MAX) / 2,
                    "Registration bucket growth exceeds signed grammar");
    count *= 2;
  }
  const auto old = std::move(row.buckets);
  row.buckets.assign(count, {0, -1});
  const auto mask = count - 1;
  // Native125760 reinserts occupied OLD BUCKETS in ascending bucket order.
  for (const auto &b : old)
    if (b.record >= 0) {
      auto at = static_cast<std::size_t>(b.hash) & mask;
      while (row.buckets[at].record >= 0)
        at = (at + 1) & mask;
      row.buckets[at] = b;
    }
}
void Monstey_Upsert(Result &result, std::uint32_t slot, std::uint32_t key,
                    std::size_t wrapper) {
  auto &row = result.state.rows[slot];
  const auto requested = row.records.size() + 1;
  // Growth occurs even when the subsequent lookup replaces an existing key.
  if (requested * 3 > Monstey_Mask(row) * 2)
    Monstey_Grow(row, requested);
  const auto bucket = Monstey_FindBucket(row, key);
  auto ordinal = row.buckets[bucket].record;
  if (ordinal < 0) {
    ordinal = static_cast<std::int32_t>(row.records.size());
    row.buckets[bucket] = {Monstey_NavigationSlotHash(key), ordinal};
    row.records.push_back({key, {}});
  }
  const auto old = row.records[static_cast<std::size_t>(ordinal)].wrapper;
  Monstey_Retain(result, wrapper);
  if (old)
    Monstey_Release(result, *old);
  row.records[static_cast<std::size_t>(ordinal)].wrapper = wrapper;
}
void Monstey_Erase(Result &result, std::uint32_t slot, std::uint32_t key) {
  auto &row = result.state.rows[slot];
  auto hole = Monstey_FindBucket(row, key);
  const auto ordinal = row.buckets[hole].record;
  Monstey_Require(ordinal >= 0,
                  "Native reciprocal descriptor key is missing from owned map");
  const auto index = static_cast<std::size_t>(ordinal),
             last = row.records.size() - 1;
  if (index != last) {
    const auto moved = Monstey_FindBucket(row, row.records[last].opposite_slot);
    row.buckets[moved].record = ordinal;
  }
  Monstey_Release(result, *row.records[index].wrapper);
  if (index != last)
    row.records[index] = row.records[last];
  row.records.pop_back();
  const auto mask = Monstey_Mask(row);
  auto next = (hole + 1) & mask;
  while (row.buckets[next].record >= 0) {
    const auto home = static_cast<std::size_t>(row.buckets[next].hash) & mask;
    if (((hole - home) & mask) < ((next - home) & mask)) {
      row.buckets[hole] = row.buckets[next];
      hole = next;
    }
    next = (next + 1) & mask;
  }
  // The old hash word is deliberately retained; native only writes index=-1.
  row.buckets[hole].record = -1;
}
void Monstey_Clear(Result &result, std::uint32_t slot) {
  auto &row = result.state.rows[slot];
  for (std::size_t n = row.records.size(); n-- > 0;)
    Monstey_Release(result, *row.records[n].wrapper);
  row.records.clear();
  row.buckets.assign(1, {0, -2});
}
} // namespace
Monstey_NavigationRegistrationResult Monstey_NavigationRegisterStreamSet(
    const Monstey_NavigationRegistrationState &state,
    std::span<const Monstey_NavigationStreamSet> sources,
    std::uint32_t current_slot, std::size_t source_set) {
  Monstey_Require(source_set < sources.size(),
                  "Registration source set outside owned input");
  Monstey_Require(state.rows.size() <= static_cast<std::size_t>(INT_MAX),
                  "Registration UID table exceeds signed count");
  Result result{state, {}};
  std::vector<std::optional<std::uint32_t>> uids;
  uids.reserve(state.rows.size());
  for (const auto &row : state.rows)
    uids.push_back(row.mesh_uid);
  const auto &source = sources[source_set];
  const auto count = static_cast<std::int32_t>(uids.size());
  result.slot_a = Monstey_NavigationFindUid(uids, count, source.uid_a);
  result.slot_b = Monstey_NavigationFindUid(uids, count, source.uid_b);
  if (result.slot_a < 0 || result.slot_b < 0)
    return result;
  if (static_cast<std::uint32_t>(result.slot_a) != current_slot &&
      static_cast<std::uint32_t>(result.slot_b) != current_slot)
    return result;
  Monstey_Validate(state, sources.size());
  const auto id = result.state.wrappers.size();
  result.state.wrappers.push_back(Monstey_NavigationRegisteredSet{
      static_cast<std::uint32_t>(result.slot_a),
      static_cast<std::uint32_t>(result.slot_b), source_set, 1});
  Monstey_Event(result, Kind::wrapper_created, id, 1);
  ++result.state.source_references[source_set];
  Monstey_Event(result, Kind::source_retained, id,
                result.state.source_references[source_set]);
  Monstey_Upsert(result, static_cast<std::uint32_t>(result.slot_a),
                 static_cast<std::uint32_t>(result.slot_b), id);
  Monstey_Upsert(result, static_cast<std::uint32_t>(result.slot_b),
                 static_cast<std::uint32_t>(result.slot_a), id);
  Monstey_Release(result, id);
  result.registered = true;
  Monstey_Validate(result.state, sources.size());
  return result;
}
Monstey_NavigationRegistrationResult Monstey_NavigationUnmapStreamSets(
    const Monstey_NavigationRegistrationState &state,
    std::span<const Monstey_NavigationStreamSet> sources,
    std::uint32_t current_slot) {
  Monstey_Require(current_slot < state.rows.size(),
                  "Unmapping current slot outside owned rows");
  Monstey_Validate(state, sources.size());
  Result result{state, {}};
  auto remaining = static_cast<std::ptrdiff_t>(
      result.state.rows[current_slot].records.size());
  while (remaining > 0) {
    --remaining;
    const auto index = static_cast<std::size_t>(remaining);
    const auto &records = result.state.rows[current_slot].records;
    Monstey_Require(
        index < records.size(),
        "Native self-unmap descriptor traversal escaped owned records");
    const auto opposite = records[index].opposite_slot;
    Monstey_Erase(result, opposite, current_slot);
  }
  Monstey_Clear(result, current_slot);
  Monstey_Validate(result.state, sources.size());
  return result;
}
} // namespace Monstey
