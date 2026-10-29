#include "Monstey_GadbSelectedCountWord.hpp"
#include "Monstey_Json.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <vector>
using namespace Monstey;
namespace {
using U64 = std::uint64_t;
using U32 = std::uint32_t;
U64 groups = 0, cases = 0, checks = 0;
void Check(bool v) {
  ++checks;
  if (!v)
    throw std::runtime_error("Selected count consumer test failed");
}
constexpr std::array<U64, 7> ordinals{87, 117, 118, 119, 120, 37, 73};
struct Arena {
  static constexpr U64 base = 0x9123456700000000ull;
  std::vector<std::uint8_t> b = std::vector<std::uint8_t>(700000);
  Monstey_GadbSelectedCountClasses c{base + 0x100, base + 0x180, base + 0x200};
  U64 record = base + 0x400, table = base + 0x10000, values = base + 0x18000;
  Monstey_GadbBinderMemory Memory() { return {base, b}; }
  void Put(U64 a, U64 v, std::size_t width) {
    for (std::size_t i = 0; i < width; ++i)
      b.at(static_cast<std::size_t>(a - base) + i) =
          static_cast<std::uint8_t>(v >> (i * 8));
  }
  U64 Owner(U32 i) const {
    return i < 5 ? c.presentation : (i == 5 ? c.name_event : c.quest);
  }
  U64 Fields(U32 i) const {
    return base + (i < 5 ? 0x1000 : (i == 5 ? 0x4000 : 0x6000));
  }
  U64 Raw(U32 i) const { return Fields(i) + ordinals.at(i) * 72; }
  Arena() {
    Put(c.presentation, base + 0x1000, 8);
    Put(c.name_event, base + 0x4000, 8);
    Put(c.quest, base + 0x6000, 8);
  }
  void Setup(U32 i, U32 state, bool indirect, std::uint8_t physical,
             U64 value) {
    const auto f = Raw(i);
    Put(f + 48, state, 4);
    Put(f + 64, physical, 1);
    Put(f + 32, 0xf1234567, 4);
    Put(record + 8, table, 8);
    Put(record + 16, values, 8);
    U64 row = table + U64(physical) * 16;
    if (std::bit_cast<std::int32_t>(state) < 2) {
      Put(record, 1, 1);
      Put(record + 1, indirect ? 0 : 1, 1);
      Put(table + (indirect ? 4 : 0), 0xf1234567, 4);
      if (indirect)
        Put(table + 9, physical, 1);
      else
        row = table;
    }
    Put(row + 10, 10, 1);
    Put(row + 12, 1, 2);
    Put(row + 14, 65535, 2);
    Put(values + U64(65535) * 8, value, 8);
  }
  U64 Run(U32 i, U64 r = UINT64_MAX) {
    return Monstey_GadbReadSelectedCountWord(
        Memory(), r == UINT64_MAX ? record : r,
        static_cast<Monstey_GadbSelectedCountField>(i), c);
  }
};
void Matrix() {
  for (U32 i = 0; i < 7; ++i)
    for (auto state :
         std::array<U32, 7>{0x80000000u, 0xffffffffu, 0, 1, 2, 3, 0x7fffffffu})
      for (bool indirect : std::array{false, true})
        for (U32 p = 0; p < 256; ++p) {
          Arena a;
          const U64 expected =
              0xfedcba9800000000ull | U64(p) * 0x1234567 | state;
          a.Setup(i, state, indirect, static_cast<std::uint8_t>(p), expected);
          Check(a.Run(i) == expected);
          ++cases;
        }
  ++groups;
}
void EmptyAndCacheType() {
  for (U32 i = 0; i < 7; ++i)
    for (bool cache : std::array{false, true}) {
      Arena a;
      a.Setup(i, cache ? 2 : 1, false, 255, 0xdeadbeef);
      const auto row = a.table + U64(cache ? 255 : 0) * 16;
      a.Put(row + 12, 0, 2);
      a.Put(row + 14, 65535, 2);
      a.Put(a.record + 16, UINT64_MAX, 8);
      Check(a.Run(i) == 0);
      ++cases;
      Check(a.Run(i, 0) == 0);
      ++cases;
    }
  for (U32 i = 0; i < 7; ++i) {
    Arena a;
    a.Setup(i, 2, false, 254, 0x123456789abcdef0);
    a.Put(a.table + 254 * 16 + 10, 63, 1);
    Check(a.Run(i) == 0x123456789abcdef0);
    ++cases;
    a.Put(a.values + U64(65535) * 8, 0, 8);
    Check(a.Run(i) == 0);
    ++cases;
  }
  ++groups;
}
void SlowType() {
  for (U32 i = 0; i < 7; ++i)
    for (U32 type = 0; type < 256; ++type) {
      Arena a;
      a.Setup(i, 1, false, 0, 0xffff000012345678);
      a.Put(a.table + 10, type, 1);
      Check(a.Run(i) == ((type & 63u) == 10 ? 0xffff000012345678 : 0));
      ++cases;
    }
  ++groups;
}
void PrivateFallbackOrder() {
  for (U32 i = 0; i < 7; ++i) {
    Arena a;
    const auto size = a.b.size();
    const auto field = Arena::base + size - 52;
    a.Put(a.Owner(i), field - ordinals[i] * 72, 8);
    a.Put(field + 48, 2, 4);
    bool failed = false;
    try {
      (void)a.Run(i, 0);
    } catch (const Monstey_GadbBinderFault &e) {
      failed = e.address == field + 64;
    }
    Check(failed);
    ++cases;
    a.Put(field + 48, 1, 4);
    Check(a.Run(i, 0) == 0);
    ++cases;
  }
  for (U32 i = 0; i < 7; ++i)
    for (bool indirect : std::array{false, true}) {
      Arena a;
      const auto fieldInterior = Arena::base - 4;
      const auto raw = fieldInterior - 32;
      a.Put(a.Owner(i), raw - ordinals[i] * 72, 8);
      a.Put(raw + 48, 1, 4);
      a.Put(a.record, 1, 1);
      a.Put(a.record + 1, indirect ? 0 : 1, 1);
      a.Put(a.record + 8, Arena::base - 32, 8);
      bool failed = false;
      try {
        (void)a.Run(i);
      } catch (const Monstey_GadbBinderFault &e) {
        failed = e.address == (indirect ? Arena::base - 28 : fieldInterior);
      }
      Check(failed);
      ++cases;
    }
  ++groups;
}
U32 LiteralSize(U32 t, U32 n, U32 f) {
  constexpr std::array<U32, 18> s{0,  5, 8, 8, 8, 8, 8, 8, 12,
                                  16, 8, 8, 8, 8, 8, 8, 8, 8};
  return f & 0x280 ? 0 : (n != 1 ? 16 : (t < s.size() ? s[t] : 0));
}
void Generated() {
  for (U32 i = 0; i < 7; ++i)
    for (U32 t = 0; t < 20; ++t)
      for (U32 n : std::array<U32, 3>{0, 1, 0xffffffffu})
        for (U32 f : std::array<U32, 3>{0, 0x280, 0x400})
          for (bool dynamic : std::array{false, true}) {
            Arena a;
            a.Setup(i, 1, false, 0, 0);
            a.Put(a.record + 1, 17, 1);
            a.Put(a.record + 24, Arena::base + 0x8000, 8);
            a.Put(Arena::base + 0x8020, Arena::base + 0x8100, 8);
            a.Put(Arena::base + 0x8100, Arena::base + 0x9000, 8);
            a.Put(Arena::base + 0x8108, 2, 4);
            a.Put(Arena::base + 0x9020, 1, 4);
            a.Put(Arena::base + 0x9024, t, 4);
            a.Put(Arena::base + 0x9028, n, 4);
            a.Put(Arena::base + 0x902c, f, 4);
            const auto selected = Arena::base + 0x9000 + 72;
            a.Put(selected + 32, 0xf1234567, 4);
            a.Put(selected + 36, 10, 4);
            a.Put(selected + 40, dynamic ? 0xffffffffu : 1, 4);
            const auto offset = (7u + LiteralSize(t, n, f)) & ~7u;
            const auto address = a.values + offset;
            const U64 expected = 0x81234567abcdef00ull | t;
            if (dynamic) {
              a.Put(address, Arena::base + 0xa000, 8);
              a.Put(address + 8, 1, 4);
              a.Put(Arena::base + 0xa000, expected, 8);
            } else
              a.Put(address, expected, 8);
            Check(a.Run(i) == expected);
            ++cases;
            if (dynamic) {
              a.Put(address + 8, 0, 4);
              bool failed = false;
              try {
                (void)a.Run(i);
              } catch (const Monstey_GadbScalarFault &e) {
                failed = e.address == address + 8;
              }
              Check(failed);
              ++cases;
            }
          }
  ++groups;
}
void PartialAndReadonly() {
  for (U32 i = 0; i < 7; ++i) {
    Arena a;
    a.Setup(i, 2, false, 255, 0xabc);
    a.Put(a.table + 255 * 16 + 12, 0, 2);
    a.b.resize(static_cast<std::size_t>(a.table - Arena::base + 255 * 16 + 14));
    const auto before = a.b;
    Check(a.Run(i) == 0);
    Check(a.b == before);
    ++cases;
    Arena b;
    b.Setup(i, 1, true, 255, 0xfedcba9876543210);
    const auto original = b.b;
    Check(b.Run(i) == 0xfedcba9876543210);
    Check(b.b == original);
    ++cases;
  }
  ++groups;
}
void NullGenerated() {
  for (U32 i = 0; i < 7; ++i)
    for (U32 which = 0; which < 4; ++which) {
      Arena a;
      a.Setup(i, 1, false, 0, 0);
      a.Put(a.record + 1, 17, 1);
      if (which) {
        a.Put(a.record + 24, Arena::base + 0x8000, 8);
        if (which > 1) {
          a.Put(Arena::base + 0x8020, Arena::base + 0x8100, 8);
          if (which == 3) {
            a.Put(Arena::base + 0x8100, Arena::base + 0x9000, 8);
            a.Put(Arena::base + 0x8108, 1, 4);
            a.Put(Arena::base + 0x9020, 0, 4);
          }
        }
      }
      Check(a.Run(i) == 0);
      ++cases;
    }
  ++groups;
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Fresh report path required");
    Matrix();
    EmptyAndCacheType();
    SlowType();
    PrivateFallbackOrder();
    Generated();
    PartialAndReadonly();
    NullGenerated();
    auto out =
        Monstey_Json::object({{"schema", "Monstey.SelectedCountWordTests/1"},
                              {"passed", true},
                              {"groups", groups},
                              {"cases", cases},
                              {"checks", checks},
                              {"synthetic_only", true},
                              {"original_code_executed", false}});
    std::ofstream(argv[1], std::ios::binary) << out.dump(2) << '\n';
    std::cout << "PASS " << groups << " groups " << cases << " cases " << checks
              << " checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
