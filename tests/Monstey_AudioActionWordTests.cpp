#include "Monstey_GadbAudioActionWord.hpp"
#include "Monstey_Json.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
using namespace Monstey;
namespace {
using U64 = std::uint64_t;
using U32 = std::uint32_t;
std::uint64_t cases = 0, checks = 0, groups = 0;
void Check(bool value) {
  ++checks;
  if (!value)
    throw std::runtime_error("Audio getter test failed");
}
struct Arena {
  static constexpr U64 base = 0x1234567800000000ull;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(700000);
  Monstey_GadbAudioActionClasses classes{base + 0x100, base + 0x180,
                                         base + 0x200, base + 0x280,
                                         0xfedcba9876543210ull};
  U64 wrapper = base + 0x300, parent = base + 0x400, child = base + 0x500;
  U64 first = base + 0x1000, second = base + 0x2000;
  Monstey_GadbBinderMemory Memory() { return {base, bytes}; }
  void Put(U64 a, U64 v, std::size_t w) {
    for (std::size_t i = 0; i < w; ++i)
      bytes.at(static_cast<std::size_t>(a - base) + i) =
          static_cast<std::uint8_t>(v >> (8 * i));
  }
  U64 Read(U64 a, std::size_t w) const {
    U64 v = 0;
    for (std::size_t i = 0; i < w; ++i)
      v |= U64(bytes.at(static_cast<std::size_t>(a - base) + i)) << (8 * i);
    return v;
  }
  explicit Arena(Monstey_GadbAudioActionPath path) {
    Put(classes.action, first, 8);
    Put(classes.set_switch, second, 8);
    Put(classes.set_rtpc, second, 8);
    Put(classes.variable_operation, second, 8);
    Put(wrapper, parent, 8);
    (void)path;
  }
  U64 Field(Monstey_GadbAudioActionPath p, bool nested) const {
    const U64 n =
        nested ? (p == Monstey_GadbAudioActionPath::SwitchValue ? 2 : 0)
               : (p == Monstey_GadbAudioActionPath::Variable
                      ? 21
                      : (p == Monstey_GadbAudioActionPath::RtpcName ? 10 : 9));
    return (nested ? second : first) + n * 72;
  }
  void Stage(Monstey_GadbAudioActionPath path, bool nested, U32 state, U32 mode,
             std::uint8_t physical, std::uint16_t word, U64 value) {
    const auto field = Field(path, nested), record = nested ? child : parent;
    const auto table = base + (nested ? 0x8000 : 0x6000),
               values = base + (nested ? 0x20000 : 0x10000);
    Put(field + 48, state, 4);
    Put(field + 64, physical, 1);
    Put(field + 32, 0x81234567u, 4);
    Put(record + 8, table, 8);
    Put(record + 16, values, 8);
    const bool cached = std::bit_cast<std::int32_t>(state) >= 2;
    U64 row = table + U64(physical) * 16;
    if (!cached) {
      Put(record, 1, 1);
      Put(record + 1, mode == 1 ? 0 : 1, 1);
      row = table;
      Put(row + (mode == 1 ? 4 : 0), 0x81234567u, 4);
      if (mode == 1) {
        Put(row + 9, physical, 1);
        row = table + U64(physical) * 16;
      }
    }
    Put(row + 10,
        nested && path != Monstey_GadbAudioActionPath::Variable ? 4 : 10, 1);
    Put(row + 12, 1, 2);
    Put(row + 14, word, 2);
    Put(values + U64(word) * 8, value, 8);
  }
  U64 Run(Monstey_GadbAudioActionPath p) {
    return Monstey_GadbReadAudioActionWord(Memory(), wrapper, p, classes);
  }
};
constexpr std::array paths{Monstey_GadbAudioActionPath::SwitchName,
                           Monstey_GadbAudioActionPath::SwitchValue,
                           Monstey_GadbAudioActionPath::RtpcName,
                           Monstey_GadbAudioActionPath::Variable};
U64 Fallback(const Arena &a, Monstey_GadbAudioActionPath p) {
  return p == Monstey_GadbAudioActionPath::Variable ? 0
                                                    : a.classes.empty_string;
}
void Matrix() {
  for (auto p : paths)
    for (auto state :
         std::array<U32, 7>{0x80000000, 0xffffffff, 0, 1, 2, 3, 0x7fffffff})
      for (U32 mode = 0; mode < 2; ++mode)
        for (U32 physical = 0; physical < 256; ++physical) {
          Arena a(p);
          const U64 value =
              0x8000000000000000ull | U64(physical) * 0x102030405ull | state;
          a.Stage(p, false, state, mode, static_cast<std::uint8_t>(physical),
                  65535, a.child);
          a.Stage(p, true, state, mode,
                  static_cast<std::uint8_t>(255 - physical), 65534, value);
          Check(a.Run(p) == value);
          ++cases;
        }
  ++groups;
}
void Empty() {
  for (auto p : paths)
    for (bool nested : std::array{false, true})
      for (bool cached : std::array{false, true}) {
        Arena a(p);
        a.Stage(p, false, cached ? 2 : 1, 0, 255, 7, a.child);
        a.Stage(p, true, cached ? 2 : 1, 0, 254, 9, 0x1234);
        const auto record = nested ? a.child : a.parent;
        const auto row = Arena::base + (nested ? 0x8000 : 0x6000) +
                         U64(cached ? (nested ? 254 : 255) : 0) * 16;
        a.Put(row + 12, 0, 2);
        a.Put(row + 14, 65535, 2);
        a.Put(record + 16, ~U64(0), 8);
        Check(a.Run(p) == Fallback(a, p));
        ++cases;
      }
  for (auto p : paths) {
    Arena a(p);
    a.Stage(p, false, 2, 0, 0, 0, a.child);
    a.Stage(p, true, 2, 0, 0, 1, 0);
    Check(a.Run(p) == 0);
    ++cases;
    a.Put(a.wrapper, 0, 8);
    Check(a.Run(p) == Fallback(a, p));
    ++cases;
  }
  ++groups;
}
void SlowTypes() {
  for (auto p : paths)
    for (U32 type = 0; type < 256; ++type) {
      Arena a(p);
      a.Stage(p, false, 1, 0, 0, 1, a.child);
      a.Stage(p, true, 1, 0, 0, 2, 0xffff123456789abcull);
      a.Put(Arena::base + 0x8000 + 10, type, 1);
      Check(a.Run(p) ==
            ((type & 63u) ==
                     (p == Monstey_GadbAudioActionPath::Variable ? 10u : 4u)
                 ? 0xffff123456789abcull
                 : Fallback(a, p)));
      ++cases;
    }
  ++groups;
}
U32 Size(U32 type, U32 count, U32 flags) {
  const std::array<U32, 18> sizes{0,  5, 8, 8, 8, 8, 8, 8, 12,
                                  16, 8, 8, 8, 8, 8, 8, 8, 8};
  return flags & 0x280
             ? 0
             : (count != 1 ? 16 : (type < sizes.size() ? sizes[type] : 0));
}
void Generated() {
  for (auto p : paths)
    for (U32 type = 0; type < 20; ++type)
      for (auto count : std::array<U32, 3>{0, 1, 0xffffffff})
        for (auto flags : std::array<U32, 4>{0, 0x80, 0x200, 0x400})
          for (bool dynamic : std::array{false, true}) {
            Arena a(p);
            a.Stage(p, false, 1, 0, 0, 1, a.child);
            a.Stage(p, true, 1, 0, 0, 2, 0);
            const auto record = a.child, field = a.Field(p, true);
            a.Put(record + 1, 17, 1);
            a.Put(record + 24, Arena::base + 0x4000, 8);
            a.Put(Arena::base + 0x4020, Arena::base + 0x4100, 8);
            a.Put(Arena::base + 0x4100, Arena::base + 0x4200, 8);
            a.Put(Arena::base + 0x4108, 2, 4);
            a.Put(Arena::base + 0x4220, 0x100, 4);
            a.Put(Arena::base + 0x4224, type, 4);
            a.Put(Arena::base + 0x4228, count, 4);
            a.Put(Arena::base + 0x422c, flags, 4);
            const auto selected = Arena::base + 0x4200 + 72;
            a.Put(selected + 32, a.Read(field + 32, 4), 4);
            a.Put(selected + 36, 0xffffffff, 4);
            a.Put(selected + 40, dynamic ? 0xffffffff : 1, 4);
            const auto offset = (7u + Size(type, count, flags)) & ~7u;
            const auto storage = Arena::base + 0x20000 + offset;
            const U64 expected = 0xabcdefed12345678ull | type;
            if (dynamic) {
              a.Put(storage, Arena::base + 0x5000, 8);
              a.Put(storage + 8, 1, 4);
              a.Put(Arena::base + 0x5000, expected, 8);
            } else
              a.Put(storage, expected, 8);
            Check(a.Run(p) == expected);
            ++cases;
            if (dynamic) {
              a.Put(storage + 8, 0, 4);
              bool threw = false;
              try {
                (void)a.Run(p);
              } catch (const Monstey_GadbScalarFault &e) {
                threw = e.address == storage + 8;
              }
              Check(threw);
              ++cases;
            }
          }
  ++groups;
}
void Binary() {
  std::mt19937_64 rng(0x123478);
  for (auto p : paths)
    for (U32 n = 1; n < 128; ++n)
      for (U32 mode = 0; mode < 2; ++mode) {
        Arena a(p);
        a.Stage(p, false, 1, mode, 0, 1, a.child);
        a.Stage(p, true, 2, 0, 0, 2, 0x1122334455667788);
        a.Put(a.parent, n, 1);
        const U32 target = static_cast<U32>(rng() % n);
        for (U32 i = 0; i < n; ++i) {
          auto row = Arena::base + 0x6000 + U64(i) * 16;
          a.Put(row + (mode == 1 ? 4 : 0), 0x80000000u + i * 37, 4);
          if (mode == 1)
            a.Put(row + 9, i, 1);
          a.Put(row + 10, 10, 1);
          a.Put(row + 12, 1, 2);
          a.Put(row + 14, 1, 2);
        }
        a.Put(a.Field(p, false) + 32, 0x80000000u + target * 37, 4);
        Check(a.Run(p) == 0x1122334455667788);
        ++cases;
        a.Put(a.Field(p, false) + 32, 0xffffffffu, 4);
        Check(a.Run(p) == Fallback(a, p));
        ++cases;
      }
  ++groups;
}
void HashBoundary() {
  const std::array<U32, 6> hashes{0,           1,           0x7fffffffu,
                                  0x80000000u, 0x80000001u, 0xffffffffu};
  for (auto p : paths)
    for (bool indirect : std::array{false, true})
      for (U32 target = 0; target < hashes.size(); ++target) {
        Arena a(p);
        a.Stage(p, false, 1, indirect ? 1 : 0, 0, 1, a.child);
        a.Stage(p, true, 2, 0, 0, 2, 0x123456789abcdef0);
        a.Put(a.parent, hashes.size(), 1);
        for (U32 i = 0; i < hashes.size(); ++i) {
          const auto row = Arena::base + 0x6000 + U64(i) * 16;
          a.Put(row + (indirect ? 4 : 0), hashes[i], 4);
          if (indirect)
            a.Put(row + 9, 5 - i, 1);
          a.Put(row + 10, 10, 1);
          a.Put(row + 12, 1, 2);
          a.Put(row + 14, 1, 2);
        }
        a.Put(a.Field(p, false) + 32, hashes[target], 4);
        Check(a.Run(p) == 0x123456789abcdef0);
        ++cases;
      }
  ++groups;
}
void CachedTypeAndZero() {
  for (auto p : paths) {
    Arena a(p);
    a.Stage(p, false, 2, 0, 255, 0, a.child);
    a.Stage(p, true, 2, 0, 254, 1, 0x1337);
    a.Put(Arena::base + 0x6000 + 255 * 16 + 10, 63, 1);
    a.Put(Arena::base + 0x8000 + 254 * 16 + 10, 63, 1);
    Check(a.Run(p) == 0x1337);
    ++cases;
  }
  std::vector<std::uint8_t> b(0x10000);
  auto put = [&](U64 a, U64 v, std::size_t w) {
    for (std::size_t i = 0; i < w; ++i)
      b.at(static_cast<std::size_t>(a) + i) =
          static_cast<std::uint8_t>(v >> (8 * i));
  };
  Monstey_GadbAudioActionClasses c{0x100, 0x180, 0x200, 0x280, 0xabcdef};
  put(0x100, 0x1000, 8);
  put(0x180, 0x2000, 8);
  put(0x300, 0x400, 8);
  put(0x1000 + 9 * 72 + 48, 2, 4);
  put(0x408, 0x4000, 8);
  put(0x410, 0x3000, 8);
  put(0x400c, 1, 2);
  put(0x3000, 0x500, 8);
  put(0x2000 + 48, 1, 4);
  put(0x2000 + 32, 0x81234567, 4);
  put(0x500, 1, 1);
  put(0x501, 17, 1);
  put(0x508, 0x5000, 8);
  put(0x510, 0x6000, 8);
  put(0x518, 0x7000, 8);
  put(0x5000, 0x81234567, 4);
  put(0x500a, 4, 1);
  put(0x500c, 1, 2);
  put(0x7020, 0x7100, 8);
  put(0x7100, 0, 8);
  put(0x7108, 1, 4);
  put(32, 0x81234567, 4);
  put(40, 1, 4);
  put(0x6000, 0x1827364554637281, 8);
  Check(Monstey_GadbReadAudioActionWord({0, b}, 0x300,
                                        Monstey_GadbAudioActionPath::SwitchName,
                                        c) == 0x1827364554637281);
  ++cases;
  ++groups;
}
void Faults() {
  for (auto p : paths) {
    Arena a(p);
    a.Stage(p, false, 2, 0, 255, 0, a.child);
    a.Stage(p, true, 2, 0, 0, 1, 7);
    const auto before = a.bytes;
    const auto bad = Arena::base - 1;
    auto classes = a.classes;
    classes.action = bad;
    bool threw = false;
    try {
      (void)Monstey_GadbReadAudioActionWord(a.Memory(), bad, p, classes);
    } catch (const Monstey_GadbBinderFault &e) {
      threw = e.address == bad;
    }
    Check(threw);
    Check(a.bytes == before);
    ++cases;
    a.Put(a.wrapper, 0, 8);
    a.Put(a.Field(p, true) + 48, 0xffffffff, 4);
    Check(a.Run(p) == Fallback(a, p));
    ++cases;
    a.Put(a.wrapper, a.parent, 8);
    a.Put(Arena::base + 0x6000 + 255 * 16 + 12, 0, 2);
    a.bytes.resize(static_cast<std::size_t>(0x6000 + 255 * 16 + 14));
    Check(a.Run(p) == Fallback(a, p));
    ++cases;
  }
  ++groups;
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Report path required");
    Matrix();
    Empty();
    SlowTypes();
    Generated();
    Binary();
    HashBoundary();
    CachedTypeAndZero();
    Faults();
    const auto report =
        Monstey_Json::object({{"schema", "Monstey.AudioActionWordTests/1"},
                              {"passed", true},
                              {"groups", groups},
                              {"cases", cases},
                              {"checks", checks},
                              {"synthetic_only", true},
                              {"original_code_executed", false}});
    std::ofstream(argv[1], std::ios::binary) << report.dump(2) << '\n';
    std::cout << "PASS " << groups << " groups " << cases << " cases " << checks
              << " checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
