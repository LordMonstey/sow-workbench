#include "Monstey_GadbBankActivation.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <array>
#include <atomic>
#include <barrier>
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>
namespace Monstey {
std::int32_t Monstey_GadbBankEpochAcquire(std::int32_t &) noexcept;
void Monstey_GadbBankEpochRelease(std::int32_t &, std::int32_t) noexcept;
} // namespace Monstey
using namespace Monstey;
namespace {
using Monstey_TestAddress = std::uint64_t;
using Monstey_TestWord = std::uint32_t;
using Monstey_TestEpoch = std::int32_t;
std::uint64_t Monstey_Groups = 0, Monstey_Cases = 0, Monstey_Checks = 0,
              Monstey_Faults = 0;
void Monstey_Require(bool good, const char *label) {
  ++Monstey_Checks;
  if (!good)
    throw std::runtime_error(label);
}
Monstey_GadbBankBackendObservation Monstey_Backend(bool manual) {
  return {true, !manual, !manual, !manual, manual ? 0xff1234ull : 0ull};
}
Monstey_TestEpoch Monstey_NextEpoch(Monstey_TestEpoch x) {
  return std::bit_cast<Monstey_TestEpoch>(std::bit_cast<Monstey_TestWord>(x) +
                                          1u);
}
void Monstey_Put(std::vector<std::uint8_t> &b, Monstey_TestAddress base,
                 Monstey_TestAddress a, Monstey_TestAddress v, std::size_t n) {
  if (a < base || a - base > b.size() ||
      n > b.size() - static_cast<std::size_t>(a - base))
    throw std::runtime_error("independent fixture write range");
  for (std::size_t i = 0; i < n; ++i)
    b[static_cast<std::size_t>(a - base) + i] =
        static_cast<std::uint8_t>(v >> (8 * i));
}
Monstey_TestAddress Monstey_Get(const std::vector<std::uint8_t> &b,
                                Monstey_TestAddress base, Monstey_TestAddress a,
                                std::size_t n) {
  if (a < base || a - base > b.size() ||
      n > b.size() - static_cast<std::size_t>(a - base))
    throw std::runtime_error("independent fixture read range");
  Monstey_TestAddress v = 0;
  for (std::size_t i = 0; i < n; ++i)
    v |= static_cast<Monstey_TestAddress>(
             b[static_cast<std::size_t>(a - base) + i])
         << (8 * i);
  return v;
}
void Monstey_ExpectRange(auto action, Monstey_TestAddress address) {
  ++Monstey_Faults;
  bool seen = false;
  try {
    action();
  } catch (const Monstey_GadbBinderFault &e) {
    seen = true;
    Monstey_Require(
        e.kind == Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable &&
            e.address == address,
        "exact consumed-range fault identity/order");
  }
  Monstey_Require(seen, "required consumed fault absent");
}
struct Monstey_Fixture {
  static constexpr Monstey_TestAddress base = 0x1425a0000, table = 0x1426397e0,
                                       cl = base + 0x1000, db = base + 0x6000,
                                       cat = base + 0x7000;
  static constexpr std::array<Monstey_TestAddress, 6> enumerators = {
      0x14162dc24, 0x1416248b8, 0x1404baa24,
      0x14161d674, 0x14161d594, 0x14482a370};
  static constexpr std::array<Monstey_TestAddress, 6> guards = {
      0x142aa2b68, 0x142aa2b3c, 0x142a58df0, 0x142aa2b44, 0x142aa2b4c, 0};
  static constexpr std::array<Monstey_TestAddress, 6> arrays = {
      0x142a59ea0, 0x142a58c70, 0x142a58e00,
      0x142a590e0, 0x142a59e10, 0x142a59e08};
  static constexpr std::array<Monstey_TestWord, 6> counts = {2687, 24, 50,
                                                             375,  15, 0};
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x540000, 0x6b);
  std::array<Monstey_GadbBankRecipe, 6> recipes;
  std::array<std::unique_ptr<Monstey_GadbBankEpochWord>, 5> owned;
  std::array<Monstey_GadbBankEpochWord *, 5> words{};
  Monstey_GadbBankEpochWord global{0x1425ac000, INT32_MIN};
  Monstey_TestEpoch thread = INT32_MIN;
  Monstey_GadbBankSynchronizer sync;
  Monstey_GadbNonnullPointerComparator comparison =
      [](auto a, auto b) -> std::int32_t { return a == b ? 0 : 19; };
  explicit Monstey_Fixture(bool manual = false)
      : sync(Monstey_Backend(manual)) {
    for (std::size_t i = 0; i < 6; ++i) {
      recipes[i] = {enumerators[i], guards[i], arrays[i], counts[i], {}};
      for (std::uint32_t j = 0; j < counts[i]; ++j)
        recipes[i].constructor_classes.push_back(cl + 64ull * (j % 8));
      Put(table + 24 * i, 0x100 + i, 8);
      Put(table + 24 * i + 8, enumerators[i], 8);
      if (i < 5) {
        owned[i] = std::make_unique<Monstey_GadbBankEpochWord>(guards[i], 0);
        words[i] = owned[i].get();
      }
    }
    for (std::size_t i = 0; i < 8; ++i) {
      for (std::size_t j = 0; j < 64; ++j)
        Put(cl + 64 * i + j, 0, 1);
      Put(cl + 64 * i + 32, 0x200 + i, 8);
    }
    for (std::size_t i = 0; i < 136; ++i)
      Put(db + i, 0, 1);
    Put(db + 80, cat, 8);
    for (std::size_t i = 0; i < 4; ++i)
      Category(i, 0x200 + i, 0, 0);
  }
  Monstey_GadbBinderMemory Memory() { return {base, bytes}; }
  Monstey_GadbBankActivation Context() {
    return {Memory(), recipes, words, global, thread, sync, table, comparison};
  }
  void Put(Monstey_TestAddress a, Monstey_TestAddress v, std::size_t n) {
    Monstey_Put(bytes, base, a, v, n);
  }
  Monstey_TestAddress Get(Monstey_TestAddress a, std::size_t n) {
    return Monstey_Get(bytes, base, a, n);
  }
  void Category(std::size_t i, Monstey_TestAddress name, std::uint32_t named,
                std::uint32_t anonymous) {
    auto a = cat + 88 * i;
    for (std::size_t j = 0; j < 88; ++j)
      Put(a + j, 0, 1);
    Put(a + 8, name, 8);
    Put(a + 32, 0xabcdef9876543210, 8);
    Put(a + 40, named, 4);
    Put(a + 64, anonymous, 4);
  }
  void Cached() {
    thread = 5;
    for (auto *g : words)
      g->Store(5);
    for (const auto &r : recipes)
      for (std::size_t i = 0; i < r.constructor_classes.size(); ++i)
        Put(r.returned_array + 8 * i, r.constructor_classes[i], 8);
  }
};
void Monstey_LinkedPrimitives() {
  ++Monstey_Groups;
  ++Monstey_Cases;
  const auto load =
                 std::bit_cast<std::uintptr_t>(&Monstey_GadbBankEpochAcquire),
             store =
                 std::bit_cast<std::uintptr_t>(&Monstey_GadbBankEpochRelease);
  const auto *l = reinterpret_cast<const std::uint8_t *>(load);
  const auto *s = reinterpret_cast<const std::uint8_t *>(store);
  Monstey_Require(l[0] == 0x8b && l[1] == 0x01 && l[2] == 0xc3,
                  "fresh linked acquire must be MOV EAX,[RCX];RET");
  Monstey_Require(s[0] == 0x89 && s[1] == 0x11 && s[2] == 0xc3,
                  "fresh linked release must be MOV [RCX],EDX;RET");
  Monstey_GadbBankEpochWord word(0xfedcba9876543210, 0);
  std::uint32_t random = 0x87654321;
  for (unsigned i = 0; i < 4096; ++i) {
    ++Monstey_Cases;
    random = random * 1664525u + 1013904223u;
    const auto value = std::bit_cast<std::int32_t>(random);
    word.Store(value);
    Monstey_Require(word.Load() == value && word.identity == 0xfedcba9876543210,
                    "live typed signed32 full-width word");
  }
}
void Monstey_StorePrefixes() {
  ++Monstey_Groups;
  // Independent native store schedule: 17 stores, with two preserved padding
  // gaps.
  constexpr std::array<std::pair<unsigned, unsigned>, 17> stores = {{{0, 8},
                                                                     {8, 8},
                                                                     {16, 8},
                                                                     {24, 8},
                                                                     {32, 8},
                                                                     {40, 8},
                                                                     {48, 8},
                                                                     {56, 4},
                                                                     {64, 8},
                                                                     {72, 8},
                                                                     {80, 8},
                                                                     {88, 8},
                                                                     {96, 8},
                                                                     {104, 8},
                                                                     {112, 8},
                                                                     {120, 8},
                                                                     {128, 4}}};
  for (unsigned limit = 0; limit <= 140; ++limit) {
    ++Monstey_Cases;
    std::vector<std::uint8_t> actual(144, 0xa7), expected = actual;
    bool fail = false;
    unsigned fault = 0;
    for (auto [offset, width] : stores) {
      if (offset + width > limit) {
        fail = true;
        fault = offset;
        break;
      }
      for (unsigned i = 0; i < width; ++i)
        expected[offset + i] = 0;
    }
    if (fail)
      Monstey_ExpectRange(
          [&] {
            (void)Monstey_GadbInitializeDatabaseHeader(
                {0, std::span(actual).first(limit)}, 0);
          },
          fault);
    else
      Monstey_Require(Monstey_GadbInitializeDatabaseHeader(
                          {0, std::span(actual).first(limit)}, 0) == 0,
                      "initializer raw return identity");
    Monstey_Require(actual == expected,
                    "initializer complete byte prefix/padding");
  }
  ++Monstey_Cases;
  std::vector<std::uint8_t> b(20, 0x19);
  const auto before = b;
  Monstey_ExpectRange(
      [&] { (void)Monstey_GadbInitializeDatabaseHeader({0x100, b}, 0xff); },
      0xff);
  Monstey_Require(b == before, "first store unavailable leaves bytes");
}
void Monstey_EpochScalar() {
  ++Monstey_Groups;
  constexpr std::array<Monstey_TestEpoch, 9> values = {
      INT32_MIN, INT32_MIN + 1, -17, -2, 0, 1, 18, INT32_MAX - 1, INT32_MAX};
  for (bool manual : {false, true}) {
    Monstey_GadbBankSynchronizer sync(Monstey_Backend(manual));
    for (auto g : values)
      for (auto global : values) {
        ++Monstey_Cases;
        Monstey_GadbBankEpochWord guard(0x100, g), epoch(0x200, global);
        Monstey_TestEpoch thread = 37;
        sync.Header(guard, epoch, thread);
        Monstey_Require(guard.Load() == (g == 0 ? -1 : g) &&
                            epoch.Load() == global &&
                            thread == (g == 0 ? 37 : global),
                        "header sequential three-way state");
        sync.Footer(guard, epoch, thread);
        const auto next = Monstey_NextEpoch(global);
        Monstey_Require(guard.Load() == next && epoch.Load() == next &&
                            thread == next,
                        "footer wrap/store/reload state");
      }
    Monstey_Require(sync.Stats().wait_calls == 0 &&
                        sync.Stats().header_calls == 81 &&
                        sync.Stats().wake_calls == 81,
                    "sequential operation counters");
  }
  for (unsigned mask = 0; mask < 8; ++mask)
    for (unsigned flag = 0; flag < 2; ++flag)
      for (bool module : {false, true}) {
        ++Monstey_Cases;
        bool bad = false;
        try {
          Monstey_GadbBankSynchronizer sync({module, (mask & 1) != 0,
                                             (mask & 2) != 0, (mask & 4) != 0,
                                             flag ? 0xffffull : 0ull});
        } catch (const Monstey_GadbBankFault &e) {
          bad = true;
          Monstey_Require(
              e.kind == Monstey_GadbBankFaultKind::InvalidBackendObservation,
              "backend invalid diagnostic");
        }
        Monstey_Require(bad == (!module || ((mask == 7) == (flag != 0))),
                        "literal native backend truth table");
      }
  for (bool footer : {false, true}) {
    ++Monstey_Cases;
    Monstey_GadbBankSynchronizer s(Monstey_Backend(false));
    Monstey_GadbBankEpochWord a(1, 17), b(1, 19);
    Monstey_TestEpoch t = 23;
    bool bad = false;
    try {
      if (footer)
        s.Footer(a, b, t);
      else
        s.Header(a, b, t);
    } catch (const Monstey_GadbBankFault &e) {
      bad = e.kind == Monstey_GadbBankFaultKind::InvalidRecipe;
    }
    Monstey_Require(bad && a.Load() == 17 && b.Load() == 19 && t == 23,
                    "typed identity alias rejected before stores");
  }
}
void Monstey_TimedWait() {
  ++Monstey_Groups;
  for (bool manual : {false, true}) {
    ++Monstey_Cases;
    Monstey_GadbBankSynchronizer s(Monstey_Backend(manual));
    Monstey_GadbBankEpochWord guard(1, -1), global(2, INT32_MIN);
    std::array<Monstey_TestEpoch, 3> thread = {INT32_MIN, INT32_MIN, INT32_MIN};
    std::array<std::uint64_t, 3> observed{};
    std::vector<std::thread> workers;
    std::uint64_t payload = 0;
    std::atomic<unsigned> done = 0;
    const auto started = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < 3; ++i)
      workers.emplace_back([&, i] {
        s.Header(guard, global, thread[i]);
        observed[i] = payload;
        done.fetch_add(1, std::memory_order_release);
      });
    const auto end = started + std::chrono::seconds(5);
    while (s.Stats().wait_calls < 9 && std::chrono::steady_clock::now() < end)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool retry = s.Stats().wait_calls >= 9;
    const auto elapsed = std::chrono::steady_clock::now() - started;
    payload = 0xe3456789abcdef01ull;
    Monstey_TestEpoch owner = INT32_MIN;
    s.Footer(guard, global, owner);
    for (auto &w : workers)
      w.join();
    Monstey_Require(retry && elapsed >= std::chrono::milliseconds(170) &&
                        done.load(std::memory_order_acquire) == 3,
                    "both real backends retry100ms and complete");
    Monstey_Require(s.Stats().wake_calls == 1 && owner == INT32_MIN + 1,
                    "one footer/wake");
    for (std::size_t i = 0; i < 3; ++i)
      Monstey_Require(thread[i] == INT32_MIN + 1 && observed[i] == payload,
                      "acquire/release publishes ordinary payload");
  }
}
void Monstey_EnumerationPaths() {
  ++Monstey_Groups;
  Monstey_Fixture f;
  auto context = f.Context();
  for (std::size_t i = 0; i < 6; ++i) {
    ++Monstey_Cases;
    const auto value =
        Monstey_GadbEnumerateNativeBank(context, f.enumerators[i]);
    Monstey_Require(value.count == f.counts[i] && value.array == f.arrays[i],
                    "six literal native output profiles");
    for (std::size_t j = 0; j < f.counts[i]; ++j)
      Monstey_Require(f.Get(value.array + 8 * j, 8) ==
                          f.recipes[i].constructor_classes[j],
                      "constructor ordered raw64 constant");
  }
  constexpr std::array<Monstey_TestEpoch, 7> states = {
      INT32_MIN, -15, -2, 0, 1, 33, INT32_MAX};
  for (std::size_t b = 0; b < 5; ++b)
    for (auto g : states)
      for (auto t : states) {
        ++Monstey_Cases;
        f.words[b]->Store(g);
        f.global.Store(-111);
        f.thread = t;
        f.Put(f.arrays[b], 0x12345678abcdef01ull, 8);
        const auto stats = f.sync.Stats();
        const auto result =
            Monstey_GadbEnumerateNativeBank(context, f.enumerators[b]);
        const bool entered = g > t, constructed = entered && g == 0;
        Monstey_Require(result.count == f.counts[b] &&
                            result.array == f.arrays[b],
                        "state matrix literal outputs");
        Monstey_Require(f.sync.Stats().header_calls - stats.header_calls ==
                            (entered ? 1u : 0u),
                        "native signed fast-path condition");
        Monstey_Require(f.words[b]->Load() == (constructed ? -110 : g) &&
                            f.global.Load() == (constructed ? -110 : -111) &&
                            f.thread ==
                                (constructed ? -110 : (entered ? -111 : t)),
                        "state matrix epochs exact");
        Monstey_Require(f.Get(f.arrays[b], 8) ==
                                (constructed ? f.cl : 0x12345678abcdef01ull) &&
                            f.sync.Stats().wake_calls - stats.wake_calls ==
                                (constructed ? 1u : 0u),
                        "cached constructor/no reset");
      }
  for (auto &recipe : f.recipes)
    recipe.constructor_classes.clear();
  f.thread = INT32_MAX;
  for (auto *g : f.words)
    g->Store(INT32_MIN);
  auto unavailable = f.Context();
  unavailable.memory = {0, {}};
  for (std::size_t b = 0; b < 6; ++b) {
    ++Monstey_Cases;
    const auto result =
        Monstey_GadbEnumerateNativeBank(unavailable, f.enumerators[b]);
    Monstey_Require(result.array == f.arrays[b] && result.count == f.counts[b],
                    "cached path no constructor or arena reads");
  }
}
void Monstey_ConstructorFaults() {
  ++Monstey_Groups;
  for (bool manual : {false, true})
    for (std::size_t prefix : {0u, 1u, 7u, 8u, 15u, 16u, 63u, 191u}) {
      ++Monstey_Cases;
      Monstey_Fixture f(manual);
      auto input = f.Context();
      const auto array = f.arrays[1];
      input.memory.bytes = std::span(f.bytes).first(
          static_cast<std::size_t>(array - f.base) + prefix);
      auto before = f.bytes;
      const auto complete = prefix / 8;
      for (std::size_t i = 0; i < complete; ++i)
        Monstey_Put(before, f.base, array + 8 * i,
                    f.recipes[1].constructor_classes[i], 8);
      Monstey_ExpectRange(
          [&] {
            (void)Monstey_GadbEnumerateNativeBank(input, f.enumerators[1]);
          },
          array + complete * 8);
      Monstey_Require(f.bytes == before && f.words[1]->Load() == -1 &&
                          f.global.Load() == INT32_MIN && f.thread == INT32_MIN,
                      "constructor fault retains claim and exact prior writes");
      Monstey_GadbBankEpochWord other(0x555, 0);
      Monstey_TestEpoch local = INT32_MIN;
      f.sync.Header(other, f.global, local);
      Monstey_Require(
          other.Load() == -1,
          "constructor fault does not retain critical-section lock");
      f.sync.Footer(other, f.global, local);
    }
}
void Monstey_ConcurrentConstructor() {
  ++Monstey_Groups;
  for (bool manual : {false, true}) {
    ++Monstey_Cases;
    Monstey_Fixture f(manual);
    std::array<Monstey_TestEpoch, 8> epochs;
    epochs.fill(INT32_MIN);
    std::array<bool, 8> good{};
    std::barrier start(8);
    std::vector<std::thread> workers;
    for (std::size_t i = 0; i < 8; ++i)
      workers.emplace_back([&, i] {
        auto c = f.Context();
        Monstey_GadbBankActivation input{
            c.memory,  c.recipes,         c.guards,     c.global_epoch,
            epochs[i], c.synchronization, c.bank_table, c.external_comparator};
        start.arrive_and_wait();
        const auto result =
            Monstey_GadbEnumerateNativeBank(input, f.enumerators[0]);
        bool valid = result.count == 2687;
        for (std::size_t j = 0; j < result.count; ++j)
          valid = valid && f.Get(result.array + 8 * j, 8) ==
                               f.recipes[0].constructor_classes[j];
        good[i] = valid;
      });
    for (auto &t : workers)
      t.join();
    Monstey_Require(f.global.Load() == INT32_MIN + 1 &&
                        f.words[0]->Load() == INT32_MIN + 1 &&
                        f.sync.Stats().wake_calls == 1,
                    "concurrent construction exactly once");
    for (std::size_t i = 0; i < 8; ++i)
      Monstey_Require(good[i] && epochs[i] == INT32_MIN + 1,
                      "concurrent complete array publication");
  }
}
std::uint32_t Monstey_Random(std::uint32_t &state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}
void Monstey_SelectionReference() {
  ++Monstey_Groups;
  Monstey_Fixture f;
  f.Cached();
  auto c = f.Context();
  std::uint32_t rng = 0x6547abcd;
  for (std::size_t trial = 0; trial < 384; ++trial) {
    ++Monstey_Cases;
    std::array<std::uint64_t, 6> names{};
    std::array<std::uint64_t, 8> classnames{};
    for (std::size_t i = 0; i < 6; ++i) {
      names[i] = Monstey_Random(rng) % 8;
      f.Put(f.table + 24 * i, names[i], 8);
      f.Put(f.table + 24 * i + 8, f.enumerators[1], 8);
    }
    for (std::size_t i = 0; i < 8; ++i) {
      classnames[i] = Monstey_Random(rng) % 16;
      f.Put(f.cl + 64 * i + 32, classnames[i], 8);
      f.Put(f.cl + 64 * i + 60, 0, 1);
    }
    const auto query = Monstey_Random(rng) % 8,
               target = Monstey_Random(rng) % 16;
    std::uint64_t expected = 0;
    bool matched = false;
    for (auto name : names)
      if (name == query) {
        matched = true;
        break;
      }
    if (matched)
      for (std::size_t i = 0; i < 24; ++i)
        if (classnames[i % 8] == target) {
          expected = f.cl + 64 * (i % 8);
          break;
        }
    const auto before = f.bytes;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> comparisons;
    f.comparison = [&](auto a, auto b) -> std::int32_t {
      comparisons.emplace_back(a, b);
      return a == b ? 0 : -29;
    };
    Monstey_Require(Monstey_GadbFindParserBankClass(c, query, target) ==
                        expected,
                    "independent first-bank/class selection equation");
    Monstey_Require(f.bytes == before, "parser leaves raw memory untouched");
    std::size_t expectedcalls = 0;
    for (auto name : names) {
      ++expectedcalls;
      if (name == query) {
        for (std::size_t i = 0; i < 24; ++i) {
          ++expectedcalls;
          if (classnames[i % 8] == target)
            break;
        }
        break;
      }
    }
    Monstey_Require(comparisons.size() == expectedcalls,
                    "direct comparator exact call count");
    f.Put(f.db + 72, 3, 4);
    auto after = f.bytes;
    for (std::size_t i = 0; i < 3; ++i) {
      const auto name = Monstey_Random(rng) % 16,
                 named = Monstey_Random(rng) % 2,
                 anonymous = Monstey_Random(rng) % 2;
      f.Category(i, name, named, anonymous);
    }
    after = f.bytes;
    if (matched)
      for (std::size_t i = 0; i < 3; ++i) {
        const auto category = f.cat + 88 * i;
        if (f.Get(category + 40, 4) == 0 && f.Get(category + 64, 4) == 0)
          continue;
        const auto name = f.Get(category + 8, 8);
        for (std::size_t j = 0; j < 24; ++j)
          if (classnames[j % 8] == name) {
            const auto selected = f.cl + 64 * (j % 8);
            Monstey_Put(after, f.base, selected + 60, 1, 1);
            Monstey_Put(after, f.base, category + 32, selected, 8);
            break;
          }
      }
    f.comparison = [](auto a, auto b) -> std::int32_t {
      return a == b ? 0 : 7;
    };
    Monstey_GadbBinderTrace trace;
    Monstey_GadbBindDatabaseBank(c, query, f.db, trace);
    Monstey_Require(
        f.bytes == after,
        "independent whole-byte binder selection/initializer equation");
  }
}
void Monstey_CallbackEpochs() {
  ++Monstey_Groups;
  Monstey_Fixture f;
  f.Cached();
  auto c = f.Context();
  ++Monstey_Cases;
  std::vector<std::pair<std::uint64_t, std::uint64_t>> args;
  f.Put(f.table, 0, 8);
  f.Put(f.cl + 32, 0, 8);
  f.comparison = [&](auto a, auto b) -> std::int32_t {
    args.emplace_back(a, b);
    return a == b ? 0 : 1;
  };
  Monstey_Require(
      Monstey_GadbFindParserBankClass(c, 0, 0) == f.cl &&
          args == std::vector<std::pair<std::uint64_t, std::uint64_t>>{{0, 0},
                                                                       {0, 0}},
      "parser direct null comparator boundary");
  ++Monstey_Cases;
  args.clear();
  f.Put(f.db + 72, 0, 4);
  Monstey_GadbBinderTrace trace;
  Monstey_GadbBindDatabaseBank(c, 0, f.db, trace);
  Monstey_Require(args.empty(),
                  "binder null-equal wrapper avoids external call");
  f.Put(f.table, 0x100, 8);
  f.Put(f.cl + 32, 0x200, 8);
  ++Monstey_Cases;
  f.comparison = [&](auto a, auto b) -> std::int32_t {
    if (a == 0x200 && b == 0x200) {
      f.Put(f.arrays[0], f.cl + 64, 8);
      return 0;
    }
    return a == b ? 0 : 1;
  };
  Monstey_Require(Monstey_GadbFindParserBankClass(c, 0x100, 0x200) == f.cl + 64,
                  "parser return slot reload after opaque comparison");
  f.Put(f.arrays[0], f.cl, 8);
  ++Monstey_Cases;
  f.Put(f.db + 72, 1, 4);
  f.Category(0, 0x200, 1, 0);
  f.comparison = [&](auto a, auto b) -> std::int32_t {
    if (a == 0x200 && b == 0x200) {
      f.Put(f.cat + 40, 0, 4);
      f.Put(f.cat + 64, 0, 4);
      f.Put(f.arrays[0], f.cl + 64, 8);
      return 0;
    }
    return a == b ? 0 : 1;
  };
  Monstey_GadbBindDatabaseBank(c, 0x100, f.db, trace);
  Monstey_Require(
      f.Get(f.cat + 32, 8) == f.cl + 64 && f.Get(f.cl + 64 + 60, 1) == 1,
      "binder selected entry not repeated count gate and final slot reload");
  f.Put(f.arrays[0], f.cl, 8);
  ++Monstey_Cases;
  f.Put(f.db + 72, 2, 4);
  f.Category(0, 0x200, 1, 0);
  f.Category(1, 0x201, 1, 0);
  bool once = false;
  f.comparison = [&](auto a, auto b) -> std::int32_t {
    if (!once && a == 0x200 && b == 0x200) {
      once = true;
      f.Put(f.db + 72, 1, 4);
    }
    return a == b ? 0 : 1;
  };
  Monstey_GadbBindDatabaseBank(c, 0x100, f.db, trace);
  Monstey_Require(f.Get(f.cat + 88 + 32, 8) == f.cl + 64,
                  "database category count captured before opaque compare");
  ++Monstey_Cases;
  f.Put(f.db + 72, 1, 4);
  f.Category(0, 0x201, 1, 0);
  once = false;
  f.comparison = [&](auto a, auto b) -> std::int32_t {
    if (!once && a == 0x200 && b == 0x201) {
      once = true;
      f.Put(f.cat + 8, 0x200, 8);
      return 1;
    }
    return a == b ? 0 : 1;
  };
  Monstey_GadbBindDatabaseBank(c, 0x100, f.db, trace);
  Monstey_Require(f.Get(f.cat + 32, 8) == f.cl,
                  "category name reread for each class comparison");
  ++Monstey_Cases;
  f.Put(f.arrays[0], 0, 8);
  f.comparison = [](auto a, auto b) -> std::int32_t { return a == b ? 0 : 1; };
  Monstey_ExpectRange(
      [&] { (void)Monstey_GadbFindParserBankClass(c, 0x100, 0x200); }, 32);
  ++Monstey_Cases;
  auto absent = c;
  absent.memory = {0, {}};
  Monstey_ExpectRange(
      [&] { (void)Monstey_GadbFindParserBankClass(absent, 0, 0); }, f.table);
}
void Monstey_LateEnumeratorAndRawNames() {
  ++Monstey_Groups;
  Monstey_Fixture f;
  f.Cached();
  auto c = f.Context();
  ++Monstey_Cases;
  std::vector<std::pair<std::uint64_t, std::uint64_t>> args;
  f.comparison = [&](auto a, auto b) -> std::int32_t {
    args.emplace_back(a, b);
    if (a == 0x100 && b == 0x100) {
      f.Put(f.table + 8, f.enumerators[5], 8);
      return 0;
    }
    return a == b ? 0 : 1;
  };
  Monstey_Require(
      Monstey_GadbFindParserBankClass(c, 0x100, 0x200) == 0 && args.size() == 1,
      "enumerator callback slot consumed after bank comparator effects");
  ++Monstey_Cases;
  constexpr std::uint64_t bank = 0x8000000000000001ull;
  constexpr std::uint64_t name = 0xfedcba9876543210ull;
  f.Put(f.table, bank, 8);
  f.Put(f.table + 8, f.enumerators[1], 8);
  f.Put(f.cl + 32, name, 8);
  args.clear();
  f.comparison = [&](auto a, auto b) -> std::int32_t {
    args.emplace_back(a, b);
    return a == b ? 0 : 1;
  };
  Monstey_Require(Monstey_GadbFindParserBankClass(c, bank, name) == f.cl &&
                      args ==
                          std::vector<std::pair<std::uint64_t, std::uint64_t>>{
                              {bank, bank}, {name, name}},
                  "external comparator receives complete raw64 identities");
}
void Monstey_NoFallthrough() {
  ++Monstey_Groups;
  Monstey_Fixture f;
  f.Cached();
  auto c = f.Context();
  f.Put(f.table + 8, f.enumerators[5], 8);
  f.Put(f.table + 24, 0x100, 8);
  std::uint64_t comparisons = 0;
  f.comparison = [&](auto a, auto b) -> std::int32_t {
    ++comparisons;
    return a == b ? 0 : 1;
  };
  ++Monstey_Cases;
  Monstey_Require(Monstey_GadbFindParserBankClass(c, 0x100, 0x200) == 0 &&
                      comparisons == 1,
                  "first empty bank forbids later equal-name bank");
  ++Monstey_Cases;
  f.Put(f.db + 72, 1, 4);
  f.Category(0, 0x200, 1, 1);
  Monstey_GadbBinderTrace trace;
  Monstey_GadbBindDatabaseBank(c, 0x100, f.db, trace);
  Monstey_Require(f.Get(f.cat + 32, 8) == 0xabcdef9876543210,
                  "binder empty bank forbids later bank");
  ++Monstey_Cases;
  f.Put(f.table + 8, 0xdeadbeef00000001ull, 8);
  bool bad = false;
  try {
    (void)Monstey_GadbFindParserBankClass(c, 0x100, 0x200);
  } catch (const Monstey_GadbBankFault &e) {
    bad = e.kind == Monstey_GadbBankFaultKind::UnknownEnumerator &&
          e.identity == 0xdeadbeef00000001ull;
  }
  Monstey_Require(bad, "unsupported enumerator stops at actual call");
}
void Monstey_TypedGuardErrors() {
  ++Monstey_Groups;
  for (bool footer : {false, true}) {
    ++Monstey_Cases;
    Monstey_GadbBankSynchronizer sync(Monstey_Backend(false));
    Monstey_GadbBankEpochWord word(0x1000, 39);
    Monstey_TestEpoch thread = -2;
    bool bad = false;
    try {
      if (footer)
        sync.Footer(word, word, thread);
      else
        sync.Header(word, word, thread);
    } catch (const Monstey_GadbBankFault &e) {
      bad = e.kind == Monstey_GadbBankFaultKind::InvalidRecipe;
    }
    Monstey_Require(bad && word.Load() == 39 && thread == -2,
                    "same live guard/global object rejected before operations");
  }
  Monstey_Fixture f;
  auto context = f.Context();
  ++Monstey_Cases;
  context.guards = {};
  bool bad = false;
  try {
    (void)Monstey_GadbEnumerateNativeBank(context, f.enumerators[0]);
  } catch (const Monstey_GadbBankFault &e) {
    bad = e.kind == Monstey_GadbBankFaultKind::MissingGuardWord &&
          e.identity == f.guards[0];
  }
  Monstey_Require(bad && f.words[0]->Load() == 0,
                  "missing live guard is not synthesized");
  ++Monstey_Cases;
  Monstey_GadbBankEpochWord second(f.guards[0], 29);
  std::array<Monstey_GadbBankEpochWord *, 2> duplicate{f.words[0], &second};
  context.guards = duplicate;
  bad = false;
  try {
    (void)Monstey_GadbEnumerateNativeBank(context, f.enumerators[0]);
  } catch (const Monstey_GadbBankFault &e) {
    bad = e.kind == Monstey_GadbBankFaultKind::InvalidRecipe;
  }
  Monstey_Require(bad && f.words[0]->Load() == 0 && second.Load() == 29,
                  "conflicting backing objects for one native identity");
}
void Monstey_MissingComparator() {
  ++Monstey_Groups;
  Monstey_Fixture f;
  f.Cached();
  auto c = f.Context();
  f.comparison = {};
  ++Monstey_Cases;
  f.Put(f.table, 0, 8);
  bool bad = false;
  try {
    (void)Monstey_GadbFindParserBankClass(c, 0, 0);
  } catch (const std::invalid_argument &) {
    bad = true;
  }
  Monstey_Require(
      bad,
      "direct parser boundary requires explicit comparator even for null-null");
  ++Monstey_Cases;
  f.Put(f.table + 8, f.enumerators[5], 8);
  Monstey_GadbBinderTrace trace;
  Monstey_GadbBindDatabaseBank(c, 0, f.db, trace);
  Monstey_Require(
      trace.events.empty(),
      "null-aware binder shortcut with absent comparator and empty bank");
  ++Monstey_Cases;
  f.Put(f.table, 0x100, 8);
  bad = false;
  try {
    Monstey_GadbBindDatabaseBank(c, 0x100, f.db, trace);
  } catch (const std::invalid_argument &) {
    bad = true;
  }
  Monstey_Require(
      bad, "nonnull equal pointer identities still require real comparator");
}
void Monstey_Resources() {
  ++Monstey_Groups;
  for (bool manual : {false, true}) {
    ++Monstey_Cases;
    DWORD before = 0, during = 0, after = 0;
    Monstey_Require(GetProcessHandleCount(GetCurrentProcess(), &before) != 0,
                    "branch handle count before");
    {
      Monstey_GadbBankSynchronizer sync(Monstey_Backend(manual));
      Monstey_Require(GetProcessHandleCount(GetCurrentProcess(), &during) !=
                              0 &&
                          during == before + (manual ? 1u : 0u),
                      "observed manual event branch owns exactly one handle");
    }
    Monstey_Require(GetProcessHandleCount(GetCurrentProcess(), &after) != 0 &&
                        after == before,
                    "branch event handle closed after joined users");
  }
  DWORD before = 0, after = 0;
  Monstey_Require(GetProcessHandleCount(GetCurrentProcess(), &before) != 0,
                  "own handle counter before");
  for (bool manual : {false, true})
    for (unsigned i = 0; i < 96; ++i) {
      ++Monstey_Cases;
      Monstey_GadbBankSynchronizer s(Monstey_Backend(manual));
      Monstey_GadbBankEpochWord guard(1, 0), global(2, INT32_MIN);
      Monstey_TestEpoch thread = INT32_MIN;
      s.Header(guard, global, thread);
      s.Footer(guard, global, thread);
    }
  Monstey_Require(GetProcessHandleCount(GetCurrentProcess(), &after) != 0 &&
                      before == after,
                  "own Win32 event handle lifecycle");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Usage: BANK_INDEPENDENT NEW_JSON");
    Monstey_LinkedPrimitives();
    Monstey_StorePrefixes();
    Monstey_EpochScalar();
    Monstey_TimedWait();
    Monstey_EnumerationPaths();
    Monstey_ConstructorFaults();
    Monstey_ConcurrentConstructor();
    Monstey_SelectionReference();
    Monstey_CallbackEpochs();
    Monstey_LateEnumeratorAndRawNames();
    Monstey_NoFallthrough();
    Monstey_TypedGuardErrors();
    Monstey_MissingComparator();
    Monstey_Resources();
    const auto json =
        "{\"status\":\"PASS\",\"groups\":" + std::to_string(Monstey_Groups) +
        ",\"cases\":" + std::to_string(Monstey_Cases) +
        ",\"checks\":" + std::to_string(Monstey_Checks) +
        ",\"range_faults\":" + std::to_string(Monstey_Faults) + "}\n";
    std::ofstream output(argv[1], std::ios::binary);
    output << json;
    if (!output)
      throw std::runtime_error("independent report write");
    std::cout << json;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}