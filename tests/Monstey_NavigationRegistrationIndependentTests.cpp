#include "Monstey_Core.hpp"
#include "Monstey_NavigationRegistration.hpp"
#include <algorithm>
#include <bit>
#include <functional>
#include <iostream>
#include <random>
using namespace Monstey;
namespace {
using K = Monstey_NavigationRegistrationEventKind;
using S = Monstey_NavigationRegistrationState;
using R = Monstey_NavigationRegistrationResult;
using E = Monstey_NavigationRegistrationEvent;
using Set = Monstey_NavigationStreamSet;
unsigned groups = 0, cases = 0, negatives = 0;
unsigned registeredCalls = 0, skippedCalls = 0, unmapCalls = 0;
void Check(bool v, const char *s) { Monstey_Require(v, s); }
std::uint32_t Hash(std::uint32_t x) {
  const auto product = static_cast<std::uint64_t>(x) * 2654435761ull;
  std::uint32_t h = 0;
  for (unsigned n = 0; n < 4; ++n)
    h = (h << 8) | static_cast<std::uint32_t>((product >> (8 * n)) & 255);
  return h;
}
struct MRow {
  std::optional<std::uint32_t> uid;
  std::vector<std::pair<std::uint32_t, std::size_t>> records;
  std::size_t capacity = 1;
};
struct MW {
  std::uint32_t a, b;
  std::size_t source;
  unsigned refs;
};
struct M {
  std::vector<MRow> rows;
  std::vector<std::optional<MW>> wrappers;
  std::vector<unsigned> sources;
  std::vector<E> events;
};
S Start(unsigned count, unsigned sources) {
  S s;
  s.source_references.resize(sources);
  for (unsigned n = 0; n < count; ++n) {
    Monstey_NavigationRegistrationRow row;
    row.mesh_uid = 0x80000000u + n;
    s.rows.push_back(row);
  }
  return s;
}
M Model(const S &s) {
  M m;
  m.sources = s.source_references;
  for (const auto &r : s.rows)
    m.rows.push_back({r.mesh_uid, {}, 1});
  return m;
}
void Event(M &m, K k, std::size_t w, unsigned refs) {
  m.events.push_back({k, w, m.wrappers[w]->source, refs});
}
void Retain(M &m, std::size_t w) {
  ++m.wrappers[w]->refs;
  Event(m, K::wrapper_retained, w, m.wrappers[w]->refs);
}
void Release(M &m, std::size_t w) {
  auto &v = *m.wrappers[w];
  Check(v.refs > 0, "Reference release underflow");
  --v.refs;
  Event(m, K::wrapper_released, w, v.refs);
  if (!v.refs) {
    --m.sources[v.source];
    Event(m, K::source_released, w, m.sources[v.source]);
    Event(m, K::wrapper_destroyed, w, 0);
    m.wrappers[w].reset();
  }
}
void Put(M &m, unsigned row, unsigned key, std::size_t wrapper) {
  auto &r = m.rows[row];
  const auto requested = r.records.size() + 1;
  if (requested * 3 > 2 * (r.capacity - 1))
    r.capacity = std::max<std::size_t>(8, std::bit_ceil(2 * requested));
  auto it = std::find_if(r.records.begin(), r.records.end(),
                         [&](const auto &v) { return v.first == key; });
  Retain(m, wrapper);
  if (it == r.records.end())
    r.records.push_back({key, wrapper});
  else {
    Release(m, it->second);
    it->second = wrapper;
  }
}
void Register(M &m, const std::vector<Set> &source, unsigned current,
              std::size_t id) {
  m.events.clear();
  std::array<int, 2> slots{-1, -1};
  const auto uids = std::array{source[id].uid_a, source[id].uid_b};
  for (unsigned side = 0; side < 2; ++side)
    for (std::size_t n = 0; n < m.rows.size(); ++n)
      if (m.rows[n].uid == std::optional(uids[side])) {
        slots[side] = static_cast<int>(n);
        break;
      }
  if (slots[0] < 0 || slots[1] < 0 ||
      (static_cast<unsigned>(slots[0]) != current &&
       static_cast<unsigned>(slots[1]) != current))
    return;
  const auto w = m.wrappers.size();
  m.wrappers.push_back(MW{static_cast<unsigned>(slots[0]),
                          static_cast<unsigned>(slots[1]), id, 1});
  Event(m, K::wrapper_created, w, 1);
  ++m.sources[id];
  Event(m, K::source_retained, w, m.sources[id]);
  Put(m, static_cast<unsigned>(slots[0]), static_cast<unsigned>(slots[1]), w);
  Put(m, static_cast<unsigned>(slots[1]), static_cast<unsigned>(slots[0]), w);
  Release(m, w);
}
void Unmap(M &m, unsigned row) {
  m.events.clear();
  std::vector<unsigned> keys;
  for (auto it = m.rows[row].records.rbegin(); it != m.rows[row].records.rend();
       ++it)
    keys.push_back(it->first);
  for (const auto opposite : keys) {
    auto &r = m.rows[opposite].records;
    const auto it = std::find_if(r.begin(), r.end(),
                                 [&](const auto &v) { return v.first == row; });
    Check(it != r.end(), "Reference reciprocal key absent");
    const auto at = static_cast<std::size_t>(it - r.begin());
    Release(m, it->second);
    if (at + 1 != r.size())
      r[at] = r.back();
    r.pop_back();
  }
  auto &r = m.rows[row];
  for (auto it = r.records.rbegin(); it != r.records.rend(); ++it)
    Release(m, it->second);
  r.records.clear();
  r.capacity = 1;
}
std::vector<std::uint64_t> Fingerprint(const S &s) {
  std::vector<std::uint64_t> x{s.rows.size(), s.wrappers.size()};
  for (const auto &r : s.rows) {
    x.push_back(r.mesh_uid.has_value());
    x.push_back(r.mesh_uid.value_or(0));
    x.push_back(r.records.size());
    for (const auto &v : r.records) {
      x.push_back(v.opposite_slot);
      x.push_back(v.wrapper.value_or(SIZE_MAX));
    }
    x.push_back(r.buckets.size());
    for (const auto &b : r.buckets) {
      x.push_back(b.hash);
      x.push_back(static_cast<std::uint32_t>(b.record));
    }
  }
  for (const auto &w : s.wrappers) {
    x.push_back(w.has_value());
    if (w) {
      x.push_back(w->slot_a);
      x.push_back(w->slot_b);
      x.push_back(w->source_set);
      x.push_back(w->references);
    }
  }
  for (auto n : s.source_references)
    x.push_back(n);
  return x;
}
void Compare(const S &s, const std::vector<E> &events, const M &m) {
  ++cases;
  Check(s.rows.size() == m.rows.size() &&
            s.wrappers.size() == m.wrappers.size() &&
            s.source_references == m.sources,
        "State extents or source ownership differ");
  for (std::size_t n = 0; n < s.rows.size(); ++n) {
    const auto &r = s.rows[n];
    const auto &q = m.rows[n];
    Check(r.mesh_uid == q.uid && r.records.size() == q.records.size() &&
              r.buckets.size() == q.capacity,
          "Row identities/order/capacity differ");
    std::vector<unsigned> seen(r.records.size());
    for (const auto &b : r.buckets)
      if (b.record >= 0) {
        const auto i = static_cast<std::size_t>(b.record);
        Check(i < r.records.size(), "Bucket out of bounds");
        ++seen[i];
        Check(b.hash == Hash(r.records[i].opposite_slot),
              "Occupied hash differs");
      }
    for (std::size_t i = 0; i < q.records.size(); ++i) {
      Check(seen[i] == 1 && r.records[i].opposite_slot == q.records[i].first &&
                r.records[i].wrapper == q.records[i].second,
            "Dense row ordinal or wrapper changed");
      std::size_t at = Hash(q.records[i].first) % q.capacity;
      unsigned probes = 0;
      while (r.buckets[at].record != static_cast<int>(i)) {
        Check(r.buckets[at].record >= 0 && ++probes < q.capacity,
              "Probe chain broken");
        at = (at + 1) % q.capacity;
      }
    }
  }
  for (std::size_t n = 0; n < m.wrappers.size(); ++n) {
    Check(s.wrappers[n].has_value() == m.wrappers[n].has_value(),
          "Wrapper liveness differs");
    if (m.wrappers[n]) {
      const auto &a = *s.wrappers[n];
      const auto &b = *m.wrappers[n];
      Check(a.slot_a == b.a && a.slot_b == b.b && a.source_set == b.source &&
                a.references == b.refs,
            "Wrapper content/refcount differs");
    }
  }
  Check(events.size() == m.events.size(), "Ownership operation count differs");
  for (std::size_t n = 0; n < events.size(); ++n) {
    const auto &a = events[n];
    const auto &b = m.events[n];
    Check(a.kind == b.kind && a.wrapper == b.wrapper &&
              a.source_set == b.source_set && a.references == b.references,
          "Ownership operation order differs");
  }
}
void Apply(S &s, M &m, const std::vector<Set> &sources, unsigned current,
           std::size_t source) {
  const auto before = Fingerprint(s);
  const auto result =
      Monstey_NavigationRegisterStreamSet(s, sources, current, source);
  Check(Fingerprint(s) == before, "Register mutated caller state");
  Register(m, sources, current, source);
  auto lookup = [&](std::uint32_t uid) {
    for (std::size_t n = 0; n < m.rows.size(); ++n)
      if (m.rows[n].uid == std::optional(uid))
        return static_cast<std::int32_t>(n);
    return std::int32_t{-1};
  };
  Check(result.slot_a == lookup(sources[source].uid_a) &&
            result.slot_b == lookup(sources[source].uid_b),
        "Resolved UID slots differ");
  Check(result.registered == !m.events.empty(),
        "Registration guard result differs");
  if (result.registered)
    ++registeredCalls;
  else
    ++skippedCalls;
  Compare(result.state, result.events, m);
  s = result.state;
}
void Remove(S &s, M &m, const std::vector<Set> &sources, unsigned current) {
  const auto before = Fingerprint(s);
  const auto result = Monstey_NavigationUnmapStreamSets(s, sources, current);
  Check(Fingerprint(s) == before, "Unmap mutated caller state");
  Unmap(m, current);
  ++unmapCalls;
  Compare(result.state, result.events, m);
  s = result.state;
}
void Reject(const std::function<void()> &f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::exception &) {
    rejected = true;
  }
  Check(rejected, "Malformed owned input accepted");
  ++negatives;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Check(argc == 2, "NEW_REPORT required");
    std::mt19937 random(0x478c50u);
    for (unsigned n = 0; n < 16384; ++n) {
      const auto x = random();
      Check(Monstey_NavigationSlotHash(x) == Hash(x),
            "Independent raw U32 hash mismatch");
    }
    ++groups;
    auto s = Start(64, 64);
    auto m = Model(s);
    std::vector<Set> sources(64);
    for (unsigned n = 0; n < 64; ++n) {
      sources[n].uid_a = *s.rows[0].mesh_uid;
      sources[n].uid_b = *s.rows[n].mesh_uid;
    }
    Apply(s, m, sources, 0, 1);
    Check(m.events.size() == 5, "Fresh pair event golden differs");
    ++groups;
    Apply(s, m, sources, 1, 1);
    Check(m.events.size() == 9, "Replacement ownership golden differs");
    ++groups;
    Remove(s, m, sources, 0);
    Check(m.events.size() == 4, "Unmap ownership golden differs");
    ++groups;
    Apply(s, m, sources, 0, 0);
    Check(m.events.size() == 6 && s.rows[0].records.size() == 1,
          "Self-pair operation ordering differs");
    Remove(s, m, sources, 0);
    ++groups;
    s = Start(64, 64);
    m = Model(s);
    for (unsigned n = 1; n <= 4; ++n)
      Apply(s, m, sources, 0, n);
    Check(s.rows[0].buckets.size() == 8, "Initial native capacity differs");
    Apply(s, m, sources, 0, 1);
    Check(s.rows[0].records.size() == 4 && s.rows[0].buckets.size() == 16,
          "Growth-before-replacement was skipped");
    ++groups;
    for (unsigned n = 5; n < 64; ++n)
      Apply(s, m, sources, 0, n);
    {
      auto cleared = s;
      auto refCleared = m;
      Remove(cleared, refCleared, sources, 0);
      Check(cleared.rows[0].records.empty(),
            "Dense reverse-clear fixture failed");
    }
    for (unsigned n = 1; n < 64; n += 2)
      Remove(s, m, sources, n);
    for (unsigned n = 2; n < 64; n += 2)
      Remove(s, m, sources, n);
    Remove(s, m, sources, 0);
    ++groups;
    s = Start(64, 64);
    s.rows[10].mesh_uid = s.rows[3].mesh_uid;
    s.rows[12].mesh_uid.reset();
    m = Model(s);
    for (unsigned n = 0; n < 64; ++n) {
      sources[n].uid_a = s.rows[random() % 64].mesh_uid.value_or(0x76543210);
      sources[n].uid_b = s.rows[random() % 64].mesh_uid.value_or(0x12345678);
    }
    for (unsigned n = 0; n < 2048; ++n) {
      unsigned row = random() % 64;
      if (n % 2 == 0) {
        const auto uid = sources[n % 64].uid_a;
        for (std::size_t i = 0; i < s.rows.size(); ++i)
          if (s.rows[i].mesh_uid == std::optional(uid)) {
            row = static_cast<unsigned>(i);
            break;
          }
      }
      if (n % 7 == 0)
        Remove(s, m, sources, row);
      else
        Apply(s, m, sources, row, n % 64);
    }
    ++groups;
    for (unsigned n = 0; n < 64; ++n)
      Remove(s, m, sources, n);
    Check(std::all_of(s.source_references.begin(), s.source_references.end(),
                      [](auto x) { return x == 0; }),
          "Unmap-all leaked owned links");
    ++groups;
    auto bad = Start(2, 1);
    std::vector<Set> pair(1);
    pair[0].uid_a = *bad.rows[0].mesh_uid;
    pair[0].uid_b = *bad.rows[1].mesh_uid;
    auto altered = bad;
    altered.rows[0].buckets.clear();
    const auto before = Fingerprint(altered);
    auto unmatched = pair;
    unmatched[0].uid_b = 123;
    const auto skipped =
        Monstey_NavigationRegisterStreamSet(altered, unmatched, 0, 0);
    Check(!skipped.registered && Fingerprint(skipped.state) == before,
          "Skipped endpoint touched unrelated invalid maps");
    Reject([&] {
      (void)Monstey_NavigationRegisterStreamSet(altered, pair, 0, 0);
    });
    Reject([&] { (void)Monstey_NavigationRegisterStreamSet(bad, pair, 0, 1); });
    Reject([&] { (void)Monstey_NavigationUnmapStreamSets(bad, pair, 2); });
    altered = bad;
    altered.source_references.clear();
    Reject([&] {
      (void)Monstey_NavigationRegisterStreamSet(altered, pair, 0, 0);
    });
    altered = bad;
    altered.rows[0].buckets.resize(3);
    Reject([&] {
      (void)Monstey_NavigationRegisterStreamSet(altered, pair, 0, 0);
    });
    ++groups;
    altered = Monstey_NavigationRegisterStreamSet(bad, pair, 0, 0).state;
    altered.wrappers[0]->references = 100;
    Reject([&] {
      (void)Monstey_NavigationRegisterStreamSet(altered, pair, 0, 0);
    });
    altered = Monstey_NavigationRegisterStreamSet(bad, pair, 0, 0).state;
    altered.rows[0].records[0].wrapper = 999;
    Reject([&] { (void)Monstey_NavigationUnmapStreamSets(altered, pair, 0); });
    pair[0].connections.push_back(
        {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu});
    pair[0].bounds_a.push_back(
        {std::bit_cast<float>(0x7fc00001u), 1, 2, 3, 4, 5, 6, 7});
    const auto ignored = Monstey_NavigationRegisterStreamSet(bad, pair, 0, 0);
    Check(ignored.registered && pair[0].connections[0].face_a == 0xffffffffu &&
              std::bit_cast<std::uint32_t>(pair[0].bounds_a[0][0]) ==
                  0x7fc00001u,
          "Registration read or rewrote untouched source body");
    ++groups;
    Check(registeredCalls >= 500 && skippedCalls >= 500 && unmapCalls >= 300,
          "Random coverage did not exercise both guards and actual mappings");
    Check(negatives == 7, "Owned-input negative cases skipped");
    const auto original = Fingerprint(bad);
    auto own = Monstey_NavigationRegisterStreamSet(bad, pair, 0, 0);
    own.state.rows[0].mesh_uid = 9;
    own.state.wrappers[0]->source_set = 999;
    Check(Fingerprint(bad) == original, "Returned state aliases caller");
    ++groups;
    Monstey_WriteNewText(
        argv[1],
        Monstey_Json::object(
            {{"schema", "Monstey.NavigationRegistrationIndependentTests/1"},
             {"pass", true},
             {"groups", groups},
             {"comparison_cases", cases},
             {"hash_cases", 16384},
             {"random_sequence_operations", 2048},
             {"negative_cases", negatives},
             {"registered_calls", registeredCalls},
             {"skipped_calls", skippedCalls},
             {"unmap_calls", unmapCalls},
             {"native_heap_counter_bytes_claimed", false},
             {"empty_bucket_hash_bytes_claimed", false},
             {"original_code_executed", false}})
                .dump(2) +
            "\n");
    std::cout << "PASS " << groups
              << " registration review groups; comparisons=" << cases
              << " negatives=" << negatives << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}