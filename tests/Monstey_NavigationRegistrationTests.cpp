#include "Monstey_Core.hpp"
#include "Monstey_NavigationRegistration.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
namespace {
using namespace Monstey;
using S = Monstey_NavigationRegistrationState;
using R = Monstey_NavigationRegistrationResult;
using K = Monstey_NavigationRegistrationEventKind;
std::uint64_t cases = 0;
void Check(bool value, const char *s) {
  if (!value)
    throw std::runtime_error(s);
}
S Start(std::span<const std::optional<std::uint32_t>> uids,
        std::size_t sources) {
  S s;
  s.source_references.resize(sources);
  for (auto uid : uids) {
    Monstey_NavigationRegistrationRow row;
    row.mesh_uid = uid;
    s.rows.push_back(std::move(row));
  }
  return s;
}
void Recount(S &s) {
  for (auto &w : s.wrappers)
    if (w)
      w->references = 0;
  for (const auto &row : s.rows)
    for (const auto &r : row.records)
      ++s.wrappers[*r.wrapper]->references;
  std::fill(s.source_references.begin(), s.source_references.end(), 0);
  for (auto &w : s.wrappers)
    if (w) {
      if (!w->references)
        w.reset();
      else
        ++s.source_references[w->source_set];
    }
}
S ReferenceRegister(S s, std::span<const Monstey_NavigationStreamSet> sources,
                    std::uint32_t current, std::size_t source) {
  std::array<std::int32_t, 2> slots{-1, -1};
  for (unsigned side = 0; side < 2; ++side) {
    const auto uid = side ? sources[source].uid_b : sources[source].uid_a;
    for (std::size_t n = 0; n < s.rows.size(); ++n)
      if (s.rows[n].mesh_uid && *s.rows[n].mesh_uid == uid) {
        slots[side] = static_cast<std::int32_t>(n);
        break;
      }
  }
  if (slots[0] < 0 || slots[1] < 0 ||
      (current != static_cast<std::uint32_t>(slots[0]) &&
       current != static_cast<std::uint32_t>(slots[1])))
    return s;
  const auto id = s.wrappers.size();
  s.wrappers.push_back(Monstey_NavigationRegisteredSet{
      static_cast<std::uint32_t>(slots[0]),
      static_cast<std::uint32_t>(slots[1]), source, 0});
  for (unsigned side = 0; side < 2; ++side) {
    auto &records = s.rows[static_cast<std::size_t>(slots[side])].records;
    const auto key = static_cast<std::uint32_t>(slots[1 - side]);
    auto it = std::find_if(records.begin(), records.end(), [&](const auto &r) {
      return r.opposite_slot == key;
    });
    if (it == records.end())
      records.push_back({key, id});
    else
      it->wrapper = id;
  }
  Recount(s);
  return s;
}
S ReferenceUnmap(S s, std::uint32_t slot) {
  auto cursor = static_cast<std::ptrdiff_t>(s.rows[slot].records.size());
  while (cursor) {
    --cursor;
    const auto other =
        s.rows[slot].records[static_cast<std::size_t>(cursor)].opposite_slot;
    auto &records = s.rows[other].records;
    const auto it =
        std::find_if(records.begin(), records.end(),
                     [&](const auto &r) { return r.opposite_slot == slot; });
    Check(it != records.end(), "Oracle missing reciprocal record");
    *it = records.back();
    records.pop_back();
  }
  s.rows[slot].records.clear();
  Recount(s);
  return s;
}
bool LogicalEqual(const S &a, const S &b) {
  if (a.rows.size() != b.rows.size() ||
      a.wrappers.size() != b.wrappers.size() ||
      a.source_references != b.source_references)
    return false;
  for (std::size_t n = 0; n < a.rows.size(); ++n) {
    if (a.rows[n].mesh_uid != b.rows[n].mesh_uid ||
        a.rows[n].records.size() != b.rows[n].records.size())
      return false;
    for (std::size_t q = 0; q < a.rows[n].records.size(); ++q) {
      const auto &x = a.rows[n].records[q];
      const auto &y = b.rows[n].records[q];
      if (x.opposite_slot != y.opposite_slot || x.wrapper != y.wrapper)
        return false;
    }
  }
  for (std::size_t n = 0; n < a.wrappers.size(); ++n) {
    if (a.wrappers[n].has_value() != b.wrappers[n].has_value())
      return false;
    if (a.wrappers[n]) {
      const auto &x = *a.wrappers[n];
      const auto &y = *b.wrappers[n];
      if (x.slot_a != y.slot_a || x.slot_b != y.slot_b ||
          x.source_set != y.source_set || x.references != y.references)
        return false;
    }
  }
  return true;
}
std::uint32_t Hash(std::uint32_t key) {
  const auto product = static_cast<std::uint32_t>(
      static_cast<std::uint64_t>(key) * 2654435761ull);
  std::uint32_t h = 0;
  for (unsigned b = 0; b < 4; ++b)
    h = (h << 8) | ((product >> (8 * b)) & 255u);
  return h;
}
void Buckets(const S &s) {
  for (const auto &row : s.rows) {
    Check(!row.buckets.empty(), "Empty bucket vector");
    const auto mask = row.buckets.size() - 1;
    for (std::size_t n = 0; n < row.records.size(); ++n) {
      auto at =
          static_cast<std::size_t>(Hash(row.records[n].opposite_slot)) & mask;
      bool found = false;
      for (std::size_t step = 0; step < row.buckets.size(); ++step) {
        const auto index = row.buckets[at].record;
        if (index < 0)
          break;
        if (index == static_cast<std::int32_t>(n)) {
          found = true;
          break;
        }
        at = (at + 1) & mask;
      }
      Check(found, "Open-addressed bucket chain lost record");
    }
  }
}
R Register(const S &s, std::span<const Monstey_NavigationStreamSet> sources,
           std::uint32_t slot, std::size_t source) {
  auto r = Monstey_NavigationRegisterStreamSet(s, sources, slot, source);
  Check(LogicalEqual(r.state, ReferenceRegister(s, sources, slot, source)),
        "Independent registration row/order/reference oracle differs");
  Buckets(r.state);
  ++cases;
  return r;
}
R Unmap(const S &s, std::span<const Monstey_NavigationStreamSet> sources,
        std::uint32_t slot) {
  auto r = Monstey_NavigationUnmapStreamSets(s, sources, slot);
  Check(LogicalEqual(r.state, ReferenceUnmap(s, slot)),
        "Independent unmapping row/order/reference oracle differs");
  Buckets(r.state);
  ++cases;
  return r;
}
void EventKinds(const R &r, std::span<const K> want) {
  Check(r.events.size() == want.size(), "Native ownership event count differs");
  for (std::size_t n = 0; n < want.size(); ++n)
    Check(r.events[n].kind == want[n], "Native ownership event order differs");
  ++cases;
}
void Error(const std::function<void()> &call) {
  bool threw = false;
  try {
    call();
  } catch (const std::exception &) {
    threw = true;
  }
  Check(threw, "Malformed owned registration accepted");
  ++cases;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    using namespace Monstey;
    Check(argc == 2, "Need NEW report");
    unsigned groups = 0;
    const std::array<std::optional<std::uint32_t>, 4> uids{101, 202, 303,
                                                           std::nullopt};
    std::vector<Monstey_NavigationStreamSet> sources(5);
    sources[0].uid_a = 101;
    sources[0].uid_b = 202;
    sources[1].uid_a = 101;
    sources[1].uid_b = 101;
    sources[2].uid_a = 303;
    sources[2].uid_b = 202;
    sources[3].uid_a = 999;
    sources[3].uid_b = 101;
    sources[4] = sources[0];
    auto initial = Start(uids, sources.size());
    auto first = Register(initial, sources, 0, 0);
    Check(first.registered && first.slot_a == 0 && first.slot_b == 1,
          "UIDs were used as slots or slot resolution failed");
    Check(initial.rows[0].records.empty(), "Input state mutated");
    Check(sources[0].uid_a == 101 && sources[0].uid_b == 202,
          "Serialized source UID changed");
    ++groups;
    EventKinds(first, std::array{K::wrapper_created, K::source_retained,
                                 K::wrapper_retained, K::wrapper_retained,
                                 K::wrapper_released});
    Check(first.state.wrappers[0]->references == 2,
          "Distinct rows must own two wrapper references");
    ++groups;
    auto replacement = Register(first.state, sources, 1, 4);
    EventKinds(replacement, std::array{K::wrapper_created, K::source_retained,
                                       K::wrapper_retained, K::wrapper_released,
                                       K::wrapper_retained, K::wrapper_released,
                                       K::source_released, K::wrapper_destroyed,
                                       K::wrapper_released});
    Check(!replacement.state.wrappers[0] &&
              replacement.state.wrappers[1]->references == 2,
          "Replaced wrapper lifetime differs");
    ++groups;
    auto self = Register(initial, sources, 0, 1);
    EventKinds(self, std::array{K::wrapper_created, K::source_retained,
                                K::wrapper_retained, K::wrapper_retained,
                                K::wrapper_released, K::wrapper_released});
    Check(self.state.rows[0].records.size() == 1 &&
              self.state.wrappers[0]->references == 1,
          "Same-slot alias must upsert same record twice");
    auto selfRemoved = Unmap(self.state, sources, 0);
    EventKinds(selfRemoved, std::array{K::wrapper_released, K::source_released,
                                       K::wrapper_destroyed});
    ++groups;
    const auto stale = Unmap(first.state, sources, 1);
    const auto expectedHash = Hash(1);
    const auto &emptyBucket = stale.state.rows[0].buckets[expectedHash & 7u];
    Check(emptyBucket.record == -1 && emptyBucket.hash == expectedHash,
          "Erasing a bucket must retain its stale hash word");
    auto removed = Unmap(first.state, sources, 0);
    EventKinds(removed, std::array{K::wrapper_released, K::wrapper_released,
                                   K::source_released, K::wrapper_destroyed});
    Check(removed.state.rows[0].buckets.size() == 1 &&
              removed.state.rows[0].buckets[0].record == -2,
          "Current clear does not restore native initial sentinel");
    Check(removed.state.rows[1].buckets.size() == 8,
          "Reciprocal erase must keep allocated bucket table");
    ++groups;
    for (auto slot : std::array<std::uint32_t, 3>{2, 3, 0xffffffffu}) {
      auto r = Register(initial, sources, slot, 0);
      Check(!r.registered && r.events.empty() && r.state.wrappers.empty(),
            "Unrelated current slot was registered");
    }
    auto missing = Register(initial, sources, 0, 3);
    Check(!missing.registered && missing.slot_a == -1 && missing.slot_b == 0 &&
              missing.events.empty(),
          "Both UID lookups must occur before unresolved return");
    ++groups;
    auto duplicate =
        Start(std::array<std::optional<std::uint32_t>, 4>{std::nullopt, 101,
                                                          202, 101},
              sources.size());
    auto dup = Register(duplicate, sources, 1, 0);
    Check(dup.slot_a == 1 && dup.slot_b == 2 &&
              dup.state.rows[3].records.empty(),
          "First nonnull duplicate UID selection differs");
    ++groups;
    std::vector<Monstey_NavigationStreamSet> orderedSources(4);
    for (std::size_t n = 0; n < 3; ++n) {
      orderedSources[n].uid_a = 101;
      orderedSources[n].uid_b = static_cast<std::uint32_t>(202 + n * 101);
    }
    orderedSources[3].uid_a = 101;
    orderedSources[3].uid_b = 101;
    const std::array<std::optional<std::uint32_t>, 4> orderedUids{101, 202, 303,
                                                                  404};
    auto ordered = Start(orderedUids, orderedSources.size());
    for (std::size_t n = 0; n < 3; ++n)
      ordered = Register(ordered, orderedSources, 0, n).state;
    auto reverse = Unmap(ordered, orderedSources, 0);
    Check(reverse.events.size() >= 3 && reverse.events[0].wrapper == 2 &&
              reverse.events[1].wrapper == 1 && reverse.events[2].wrapper == 0,
          "Reciprocal unmapping must visit current records backward");
    ordered = Start(orderedUids, orderedSources.size());
    for (auto n : std::array<std::size_t, 4>{0, 3, 1, 2})
      ordered = Register(ordered, orderedSources, 0, n).state;
    (void)Unmap(ordered, orderedSources, 0);
    ++groups;
    std::mt19937 rng(0x478c50u);
    for (unsigned n = 0; n < 4096; ++n) {
      const auto key = rng();
      Check(Monstey_NavigationSlotHash(key) == Hash(key),
            "Slot hash exact U32 encoding differs");
      ++cases;
    }
    ++groups;
    std::vector<std::optional<std::uint32_t>> manyUids;
    for (std::uint32_t n = 0; n < 64; ++n)
      manyUids.push_back(1000 + n);
    std::vector<Monstey_NavigationStreamSet> many(64);
    for (std::uint32_t n = 0; n < 64; ++n) {
      many[n].uid_a = 1000;
      many[n].uid_b = 1000 + n;
    }
    auto manyState = Start(manyUids, many.size());
    for (std::size_t n = 1; n <= 4; ++n)
      manyState = Register(manyState, many, 0, n).state;
    Check(manyState.rows[0].buckets.size() == 8, "Initial map size differs");
    manyState = Register(manyState, many, 0, 1).state;
    Check(manyState.rows[0].records.size() == 4 &&
              manyState.rows[0].buckets.size() == 16,
          "Replacement must perform prelookup growth");
    for (std::size_t n = 5; n < 64; ++n)
      manyState = Register(manyState, many, 0, n).state;
    for (std::uint32_t n = 2; n < 64; n += 3)
      manyState = Unmap(manyState, many, n).state;
    manyState = Unmap(manyState, many, 0).state;
    ++groups;
    for (unsigned scenario = 0; scenario < 40; ++scenario) {
      const auto rowCount = 3 + rng() % 29;
      std::vector<std::optional<std::uint32_t>> table;
      for (unsigned n = 0; n < rowCount; ++n)
        table.push_back(rng() % 5 == 0
                            ? std::nullopt
                            : std::optional<std::uint32_t>{rng() % 17});
      std::vector<Monstey_NavigationStreamSet> sets(48);
      for (auto &set : sets) {
        set.uid_a = rng() % 21;
        set.uid_b = rng() % 21;
      }
      auto s = Start(table, sets.size());
      for (unsigned op = 0; op < 64; ++op) {
        if (rng() % 5 == 0)
          s = Unmap(s, sets, rng() % rowCount).state;
        else
          s = Register(s, sets, rng() % (rowCount + 2), rng() % sets.size())
                  .state;
      }
    }
    ++groups;
    auto broken = first.state;
    broken.rows[1].buckets[0].record = 9999;
    Error([&] { (void)Monstey_NavigationUnmapStreamSets(broken, sources, 0); });
    broken = first.state;
    ++broken.wrappers[0]->references;
    Error([&] {
      (void)Monstey_NavigationRegisterStreamSet(broken, sources, 0, 0);
    });
    Error(
        [&] { (void)Monstey_NavigationUnmapStreamSets(initial, sources, 99); });
    Error([&] {
      (void)Monstey_NavigationRegisterStreamSet(initial, sources, 0, 99);
    });
    broken = first.state;
    broken.rows[1].records.clear();
    for (auto &b : broken.rows[1].buckets)
      b.record = -1;
    broken.wrappers[0]->references = 1;
    Error([&] { (void)Monstey_NavigationUnmapStreamSets(broken, sources, 0); });
    ++groups;
    Monstey_WriteNewText(
        argv[1], Monstey_Json::object(
                     {{"schema", "Monstey.NavigationRegistrationTests/1"},
                      {"pass", true},
                      {"groups", groups},
                      {"cases", cases},
                      {"hash_cases", 4096},
                      {"random_sequence_operations", 2560},
                      {"original_functions_executed", false}})
                         .dump(2) +
                     "\n");
    std::cout << "PASS " << groups << " groups " << cases
              << " registration cases\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
