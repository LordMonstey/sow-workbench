#include "Monstey_GadbIntegerWord.hpp"
#include "Monstey_GadbScalarFloat.hpp"
#include "Monstey_Json.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <random>
#include <span>
#include <type_traits>
#include <vector>
using namespace Monstey;
namespace {
using Monstey_Word64 = std::uint64_t;
using Monstey_Word32 = std::uint32_t;
enum class Monstey_Kind { Value, Range, Dynamic };
struct Monstey_Outcome {
  Monstey_Kind kind = Monstey_Kind::Value;
  Monstey_Word64 value = 0, address = 0;
  bool operator==(const Monstey_Outcome &) const = default;
};
struct Monstey_RefFault {
  Monstey_Kind kind;
  Monstey_Word64 address;
};
Monstey_Word64 Monstey_RefRead(Monstey_GadbBinderMemory image,
                               Monstey_Word64 address, std::size_t width) {
  if (address < image.base || address - image.base > image.bytes.size() ||
      width >
          image.bytes.size() - static_cast<std::size_t>(address - image.base))
    throw Monstey_RefFault{Monstey_Kind::Range, address};
  Monstey_Word64 answer = 0;
  for (std::size_t k = width; k != 0; --k)
    answer =
        (answer << 8) |
        image.bytes[static_cast<std::size_t>(address - image.base) + k - 1];
  return answer;
}
Monstey_Word64 Monstey_RefLookup(Monstey_GadbBinderMemory m, Monstey_Word64 r,
                                 Monstey_Word64 field) {
  const auto flags = Monstey_RefRead(m, r + 1, 1),
             count = Monstey_RefRead(m, r, 1),
             table = Monstey_RefRead(m, r + 8, 8);
  int low = 0, high = static_cast<int>(count) - 1;
  if (flags & 1) {
    if (high < 0)
      return 0;
    const auto wanted = Monstey_RefRead(m, field, 4);
    while (low <= high) {
      int mid = (low + high) / 2;
      const auto row = table + static_cast<Monstey_Word64>(mid) * 16;
      const auto key = Monstey_RefRead(m, row, 4);
      if (key == wanted)
        return row;
      if (key > wanted)
        high = mid - 1;
      else
        low = mid + 1;
    }
    return 0;
  }
  while (low <= high) {
    int mid = (low + high) / 2;
    const auto row = table + static_cast<Monstey_Word64>(mid) * 16;
    const auto key = Monstey_RefRead(m, row + 4, 4);
    const auto wanted = Monstey_RefRead(m, field, 4);
    if (key == wanted)
      return table + Monstey_RefRead(m, row + 9, 1) * 16;
    if (key > wanted)
      high = mid - 1;
    else
      low = mid + 1;
  }
  return 0;
}
Monstey_Word32 Monstey_RefSize(Monstey_Word32 type, Monstey_Word32 count,
                               Monstey_Word32 flags) {
  constexpr std::array<Monstey_Word32, 18> literal{0,  5, 8, 8, 8, 8, 8, 8, 12,
                                                   16, 8, 8, 8, 8, 8, 8, 8, 8};
  return flags & 0x280
             ? 0
             : (count != 1 ? 16 : (type < literal.size() ? literal[type] : 0));
}
Monstey_Word64 Monstey_RefFind(Monstey_GadbBinderMemory m, Monstey_Word64 cls,
                               Monstey_Word32 hash) {
  const auto n = static_cast<Monstey_Word32>(Monstey_RefRead(m, cls + 8, 4));
  if (!n)
    return 0;
  const auto rows = Monstey_RefRead(m, cls, 8);
  for (Monstey_Word32 i = 0; i < n; ++i)
    if (Monstey_RefRead(m, rows + static_cast<Monstey_Word64>(i) * 72 + 32,
                        4) == hash)
      return rows + static_cast<Monstey_Word64>(i) * 72;
  return 0;
}
Monstey_Word32 Monstey_RefOffset(Monstey_GadbBinderMemory m, Monstey_Word64 cls,
                                 Monstey_Word32 hash) {
  const auto n = static_cast<Monstey_Word32>(Monstey_RefRead(m, cls + 8, 4));
  if (!n)
    return 0xffffffffu;
  const auto rows = Monstey_RefRead(m, cls, 8);
  Monstey_Word32 offset = 0;
  for (Monstey_Word32 i = 0; i < n; ++i) {
    const auto row = rows + static_cast<Monstey_Word64>(i) * 72;
    if (Monstey_RefRead(m, row + 32, 4) == hash)
      return offset;
    const auto flags =
        static_cast<Monstey_Word32>(Monstey_RefRead(m, row + 44, 4));
    const auto count =
        static_cast<Monstey_Word32>(Monstey_RefRead(m, row + 40, 4));
    const auto type =
        static_cast<Monstey_Word32>(Monstey_RefRead(m, row + 36, 4));
    offset = (offset + 7 + Monstey_RefSize(type, count, flags)) & ~7u;
  }
  return 0xffffffffu;
}
Monstey_Word32 Monstey_RefGenerated(Monstey_GadbBinderMemory m,
                                    Monstey_Word64 r, Monstey_Word64 field,
                                    Monstey_Word32 index,
                                    Monstey_Word64 fallback) {
  if (r) {
    const auto owner = Monstey_RefRead(m, r + 24, 8);
    if (owner && Monstey_RefRead(m, owner + 32, 8)) {
      const auto cls = Monstey_RefRead(m, owner + 32, 8);
      const auto hash =
          static_cast<Monstey_Word32>(Monstey_RefRead(m, field, 4));
      const auto declaration = Monstey_RefFind(m, cls, hash);
      if (declaration) {
        const auto offset = Monstey_RefOffset(m, cls, hash);
        const auto type = Monstey_RefRead(m, declaration + 36, 4);
        const auto storage =
            static_cast<Monstey_Word64>(offset) + Monstey_RefRead(m, r + 16, 8);
        if (type == 8 || type == 9)
          index = static_cast<Monstey_Word32>(index * 2u);
        const auto count = Monstey_RefRead(m, declaration + 40, 4);
        if (count != 0xffffffffu)
          return static_cast<Monstey_Word32>(Monstey_RefRead(
              m, storage + static_cast<Monstey_Word64>(index) * 8, 8));
        if (index >= Monstey_RefRead(m, storage + 8, 4))
          throw Monstey_RefFault{Monstey_Kind::Dynamic, storage + 8};
        const auto values = Monstey_RefRead(m, storage, 8);
        return static_cast<Monstey_Word32>(Monstey_RefRead(
            m, values + static_cast<Monstey_Word64>(index) * 8, 8));
      }
    }
  }
  return static_cast<Monstey_Word32>(Monstey_RefRead(m, fallback, 4));
}
Monstey_Word32 Monstey_RefGeneric(Monstey_GadbBinderMemory m, Monstey_Word64 r,
                                  Monstey_Word64 field, Monstey_Word32 index,
                                  Monstey_Word64 fallback) {
  if (r) {
    const auto d = Monstey_RefLookup(m, r, field);
    if (d && (Monstey_RefRead(m, d + 10, 1) & 63u) == 5 &&
        index < Monstey_RefRead(m, d + 12, 2)) {
      if (Monstey_RefRead(m, r + 1, 1) & 16)
        return Monstey_RefGenerated(m, r, field, index, fallback);
      const auto first =
          static_cast<Monstey_Word32>(Monstey_RefRead(m, d + 14, 2));
      const auto values = Monstey_RefRead(m, r + 16, 8);
      return static_cast<Monstey_Word32>(Monstey_RefRead(
          m,
          values + static_cast<Monstey_Word64>(
                       static_cast<Monstey_Word32>(first + index)) *
                       8,
          4));
    }
  }
  return static_cast<Monstey_Word32>(Monstey_RefRead(m, fallback, 4));
}
Monstey_Word32 Monstey_RefCached(Monstey_GadbBinderMemory m, Monstey_Word64 r,
                                 std::uint8_t physical, Monstey_Word32 index,
                                 Monstey_Word64 fallback) {
  if (r) {
    const auto table = Monstey_RefRead(m, r + 8, 8);
    const auto d = table + static_cast<Monstey_Word64>(physical) * 16;
    if (index < Monstey_RefRead(m, d + 12, 2)) {
      const auto first =
          static_cast<Monstey_Word32>(Monstey_RefRead(m, d + 14, 2));
      const auto values = Monstey_RefRead(m, r + 16, 8);
      return static_cast<Monstey_Word32>(Monstey_RefRead(
          m,
          values + static_cast<Monstey_Word64>(
                       static_cast<Monstey_Word32>(first + index)) *
                       8,
          4));
    }
  }
  return static_cast<Monstey_Word32>(Monstey_RefRead(m, fallback, 4));
}
struct Monstey_Frame {
  static constexpr Monstey_Word64 base = 0x7654321000000000ull;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x2000);
  Monstey_Word64 record = base + 0x100, field = base + 0x200,
                 table = base + 0x400, def = base + 0x40,
                 target = base + 0x1800, owner = base + 0x1c00,
                 cls = base + 0x1d00, rows = base + 0x1000,
                 storage = base + 0x1600;
  void Put(Monstey_Word64 at, Monstey_Word64 value, std::size_t width) {
    for (std::size_t k = 0; k < width; ++k)
      bytes.at(static_cast<std::size_t>(at - base) + k) =
          static_cast<std::uint8_t>(value >> (8 * k));
  }
  Monstey_GadbBinderMemory
  Image(std::size_t begin = 0,
        std::size_t count = static_cast<std::size_t>(-1)) {
    return {base + begin, std::span(bytes).subspan(
                              begin, std::min(count, bytes.size() - begin))};
  }
  void Raw(Monstey_Word32 index, std::uint16_t count, std::uint8_t type,
           std::uint8_t physical = 0, std::uint16_t first = 3,
           Monstey_Word32 value = 0x81234567) {
    Put(def, 0xfedcba98, 4);
    Put(record, 1, 1);
    Put(record + 1, 1, 1);
    Put(record + 8, table, 8);
    Put(field, 0xf1234567, 4);
    const auto d = table + static_cast<Monstey_Word64>(physical) * 16;
    Put(table, 0xf1234567, 4);
    Put(d + 10, type, 1);
    Put(d + 12, count, 2);
    Put(d + 14, first, 2);
    Put(record + 16,
        target - static_cast<Monstey_Word64>(
                     static_cast<Monstey_Word32>(first + index)) *
                     8,
        8);
    Put(target, value, 4);
    if (target + 8 <= base + bytes.size())
      Put(target + 4, 0xdeadbeef, 4);
  }
  void Generated(Monstey_Word32 index, Monstey_Word32 type,
                 Monstey_Word32 count,
                 Monstey_Word32 dynamic_count = 0xffffffffu,
                 Monstey_Word32 value = 0x87654321,
                 Monstey_Word32 preceding_type = 1,
                 Monstey_Word32 preceding_count = 1,
                 Monstey_Word32 preceding_flags = 0) {
    Raw(index, 0xffff, 5);
    Put(record + 1, 17, 1);
    Put(record + 24, owner, 8);
    Put(owner + 32, cls, 8);
    Put(cls, rows, 8);
    Put(cls + 8, 2, 4);
    Put(rows + 32, 1, 4);
    Put(rows + 36, preceding_type, 4);
    Put(rows + 40, preceding_count, 4);
    Put(rows + 44, preceding_flags, 4);
    Put(rows + 72 + 32, 0xf1234567, 4);
    Put(rows + 72 + 36, type, 4);
    Put(rows + 72 + 40, count, 4);
    const auto offset = (7 + Monstey_RefSize(preceding_type, preceding_count,
                                             preceding_flags)) &
                        ~7u;
    const Monstey_Word32 effective =
        (type == 8 || type == 9) ? static_cast<Monstey_Word32>(index * 2u)
                                 : index;
    const auto start =
        count == 0xffffffffu
            ? storage
            : target - static_cast<Monstey_Word64>(effective) * 8;
    Put(record + 16, start - offset, 8);
    if (count == 0xffffffffu) {
      Put(storage, target - static_cast<Monstey_Word64>(effective) * 8, 8);
      Put(storage + 8, dynamic_count, 4);
    }
    Put(target, value, 4);
    if (target + 8 <= base + bytes.size())
      Put(target + 4, 0xa1b2c3d4, 4);
  }
};
Monstey_Word64 Monstey_Groups = 0, Monstey_Cases = 0, Monstey_Checks = 0,
               Monstey_RangeCases = 0, Monstey_DynamicCases = 0;
void Monstey_Check(bool condition) {
  ++Monstey_Checks;
  if (!condition)
    throw std::runtime_error("Integer independent assertion failed");
}
template <class F> Monstey_Outcome Monstey_Reference(F &&f) {
  try {
    return {Monstey_Kind::Value, static_cast<Monstey_Word64>(f()), 0};
  } catch (const Monstey_RefFault &e) {
    return {e.kind, 0, e.address};
  }
}
template <class F> Monstey_Outcome Monstey_Actual(F &&f) {
  try {
    return {Monstey_Kind::Value, static_cast<Monstey_Word64>(f()), 0};
  } catch (const Monstey_GadbBinderFault &e) {
    return {Monstey_Kind::Range, 0, e.address};
  } catch (const Monstey_GadbScalarFault &e) {
    return {Monstey_Kind::Dynamic, 0, e.address};
  }
}
void Monstey_Compare(Monstey_Frame &f, int kind, Monstey_Word32 index = 0,
                     std::uint8_t physical = 0, std::size_t begin = 0,
                     std::size_t length = static_cast<std::size_t>(-1),
                     Monstey_Word64 record = UINT64_MAX,
                     Monstey_Word64 fallback = UINT64_MAX) {
  const auto image = f.Image(begin, length);
  const auto r = record == UINT64_MAX ? f.record : record,
             def = fallback == UINT64_MAX ? f.def : fallback;
  const auto before = f.bytes;
  const auto expected = Monstey_Reference([&]() {
    return kind == 0   ? Monstey_RefGeneric(image, r, f.field, index, def)
           : kind == 1 ? Monstey_RefGenerated(image, r, f.field, index, def)
                       : Monstey_RefCached(image, r, physical, index, def);
  });
  const auto actual = Monstey_Actual([&]() {
    return kind == 0
               ? Monstey_GadbReadIntegerWord(image, r, f.field, index, def)
           : kind == 1 ? Monstey_GadbReadGeneratedIntegerWord(image, r, f.field,
                                                              index, def)
                       : Monstey_GadbReadCachedIntegerWord(image, r, physical,
                                                           index, def);
  });
  if (actual != expected) {
    std::cerr << "Mismatch case " << Monstey_Cases << " kind " << kind
              << " index " << index << " got " << static_cast<int>(actual.kind)
              << "/" << std::hex << actual.address << "/" << actual.value
              << " expected " << static_cast<int>(expected.kind) << "/"
              << expected.address << "/" << expected.value << std::dec << '\n';
  }
  Monstey_Check(actual == expected);
  Monstey_Check(f.bytes == before);
  ++Monstey_Cases;
  if (expected.kind == Monstey_Kind::Range)
    ++Monstey_RangeCases;
  if (expected.kind == Monstey_Kind::Dynamic)
    ++Monstey_DynamicCases;
}
void Monstey_TypesAndCached() {
  for (Monstey_Word32 t = 0; t < 256; ++t)
    for (auto count : std::array<std::uint16_t, 4>{0, 1, 3, 65535})
      for (auto index : std::array<Monstey_Word32, 8>{
               0, 1, 2, 65534, 65535, 65536, 0x80000000u, 0xffffffffu}) {
        Monstey_Frame f;
        f.Raw(index, count, static_cast<std::uint8_t>(t), 0, 65535,
              t & 1 ? 0u : 0x80000000u);
        Monstey_Compare(f, 0, index);
      }
  ++Monstey_Groups;
  for (Monstey_Word32 p = 0; p < 256; ++p)
    for (auto count : std::array<std::uint16_t, 5>{0, 1, 2, 32768, 65535})
      for (auto index : std::array<Monstey_Word32, 8>{
               0, 1, 2, 32767, 65534, 65535, 65536, 0xffffffffu}) {
        Monstey_Frame f;
        f.Raw(index, count, static_cast<std::uint8_t>(p),
              static_cast<std::uint8_t>(p), 65535, 0xf0000000u | p);
        f.Put(f.record, 0, 1);
        f.Put(f.record + 1, 0xff, 1);
        Monstey_Compare(f, 2, index, static_cast<std::uint8_t>(p));
      }
  ++Monstey_Groups;
}
void Monstey_WidthsAndAliases() {
  for (int kind = 0; kind < 3; ++kind) {
    Monstey_Frame f;
    f.Raw(0, 1, 5);
    Monstey_Compare(f, kind, 0, 0, 0x40, 4, 0);
    for (auto at : std::array<Monstey_Word64, 5>{
             f.record, f.record + 8, f.field, f.table + 10, f.target}) {
      f.Raw(1, 0, 5);
      Monstey_Compare(f, kind, 1, 0, 0, static_cast<std::size_t>(-1), 0, at);
    }
  }
  for (int kind = 0; kind < 3; ++kind) {
    Monstey_Frame f;
    f.target = Monstey_Frame::base + f.bytes.size() - 4;
    if (kind == 1)
      f.Generated(0, 5, 1);
    else
      f.Raw(0, 1, 5);
    Monstey_Compare(f, kind);
    f.def = f.target;
    f.Put(f.def, 0x80000001, 4);
    Monstey_Compare(f, kind, 0, 0, 0, static_cast<std::size_t>(-1), 0);
  }
  Monstey_Frame f;
  f.Raw(0, 1, 5);
  Monstey_Compare(f, 2, 0, 0, 0x108);
  Monstey_Compare(f, 0, 0, 0, 0x108);
  ++Monstey_Groups;
}
void Monstey_GeneratedMatrix() {
  for (auto type : std::array<Monstey_Word32, 8>{0, 1, 3, 5, 8, 9, 17, 99})
    for (auto count : std::array<Monstey_Word32, 4>{0, 1, 7, 0xffffffffu})
      for (auto index : std::array<Monstey_Word32, 8>{
               0, 1, 3, 65535, 65536, 0x7fffffffu, 0x80000000u, 0xffffffffu})
        for (auto dynamic :
             std::array<Monstey_Word32, 5>{0, 1, 3, 0x80000000u, 0xffffffffu}) {
          Monstey_Frame f;
          f.Generated(index, type, count, dynamic, index & 1 ? 0 : 0xfedcba98);
          Monstey_Compare(f, 1, index);
          if (index < 65535)
            Monstey_Compare(f, 0, index);
        }
  ++Monstey_Groups;
  for (Monstey_Word32 t = 0; t < 20; ++t)
    for (auto c : std::array<Monstey_Word32, 3>{0, 1, 0xffffffffu})
      for (auto flags : std::array<Monstey_Word32, 4>{0, 0x80, 0x200, 0x400}) {
        Monstey_Frame f;
        f.Generated(2, 5, 1, 0, 0x80000000, t, c, flags);
        Monstey_Compare(f, 1, 2);
      }
  ++Monstey_Groups;
}
void Monstey_Lookups() {
  std::mt19937 rng(0xc0ffee);
  for (bool indirect : std::array{false, true})
    for (Monstey_Word32 n = 0; n <= 255; ++n) {
      Monstey_Frame f;
      f.Raw(0, 1, 5);
      f.Put(f.record, n, 1);
      f.Put(f.record + 1, indirect ? 0 : 1, 1);
      std::vector<Monstey_Word32> keys;
      while (keys.size() < n) {
        keys.push_back(rng());
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
      }
      for (Monstey_Word32 j = 0; j < n; ++j) {
        const auto row = f.table + static_cast<Monstey_Word64>(j) * 16;
        f.Put(row + (indirect ? 4 : 0), keys[j], 4);
        if (indirect)
          f.Put(row + 9, 255 - j, 1);
        f.Put(row + 10, 5, 1);
        f.Put(row + 12, 1, 2);
        f.Put(row + 14, 3, 2);
      }
      for (auto at : std::array<Monstey_Word32, 3>{0, n / 2, n ? n - 1 : 0}) {
        if (n)
          f.Put(f.field, keys[at], 4);
        Monstey_Compare(f, 0);
      }
      f.Put(f.field, 0xffffffffu, 4);
      Monstey_Compare(f, 0);
    }
  ++Monstey_Groups;
}
void Monstey_LazyFaultWindows() {
  for (int kind = 0; kind < 3; ++kind)
    for (bool indirect : std::array{false, true}) {
      Monstey_Frame f;
      if (kind == 1)
        f.Generated(1, 8, 0xffffffffu, 3);
      else {
        f.Raw(0, 1, 5);
        if (indirect) {
          f.Put(f.record + 1, 0, 1);
          f.Put(f.table + 4, 0xf1234567, 4);
          f.Put(f.table + 9, 0, 1);
        }
      }
      for (std::size_t end = 0; end <= f.bytes.size(); end += 3)
        Monstey_Compare(f, kind, kind == 1 ? 1u : 0u, 0, 0, end);
      for (std::size_t begin = 0; begin <= f.bytes.size(); begin += 7)
        Monstey_Compare(f, kind, kind == 1 ? 1u : 0u, 0, begin);
    }
  ++Monstey_Groups;
  for (int kind = 0; kind < 3; ++kind)
    for (auto dtype : std::array<std::uint8_t, 3>{0, 5, 0xc5}) {
      Monstey_Frame f;
      f.Raw(0, 0, dtype);
      f.Put(f.table + 14, 0xffff, 2);
      f.Put(f.record + 16, UINT64_MAX, 8);
      Monstey_Compare(
          f, kind, 0, 0, 0,
          static_cast<std::size_t>(f.table - Monstey_Frame::base + 14));
    }
  ++Monstey_Groups;
}
void Monstey_Expected(Monstey_Frame &f, int kind, Monstey_Word32 index,
                      Monstey_Outcome expected, std::size_t begin = 0,
                      std::size_t length = static_cast<std::size_t>(-1),
                      Monstey_Word64 record = UINT64_MAX) {
  const auto image = f.Image(begin, length);
  const auto r = record == UINT64_MAX ? f.record : record;
  const auto before = f.bytes;
  const auto actual = Monstey_Actual([&]() {
    return kind == 0
               ? Monstey_GadbReadIntegerWord(image, r, f.field, index, f.def)
           : kind == 1
               ? Monstey_GadbReadGeneratedIntegerWord(image, r, f.field, index,
                                                      f.def)
               : Monstey_GadbReadCachedIntegerWord(image, r, 0, index, f.def);
  });
  Monstey_Check(actual == expected);
  Monstey_Check(f.bytes == before);
  ++Monstey_Cases;
  if (expected.kind == Monstey_Kind::Range)
    ++Monstey_RangeCases;
  if (expected.kind == Monstey_Kind::Dynamic)
    ++Monstey_DynamicCases;
}
void Monstey_AnalyticBoundaries() {
  for (int kind = 0; kind < 3; ++kind) {
    Monstey_Frame f;
    f.Raw(0, 1, 5);
    f.def = Monstey_Frame::base + f.bytes.size() - 4;
    f.Put(f.def, 0x80000001u, 4);
    Monstey_Expected(f, kind, 0, {Monstey_Kind::Value, 0x80000001u, 0}, 0,
                     static_cast<std::size_t>(-1), 0);
  }
  for (int kind : std::array{0, 2}) {
    Monstey_Frame f;
    f.target = Monstey_Frame::base + f.bytes.size() - 4;
    f.Raw(0, 1, 0xc5, 0, 3, 0xffffffffu);
    Monstey_Expected(f, kind, 0, {Monstey_Kind::Value, 0xffffffffu, 0});
  }
  {
    Monstey_Frame f;
    f.target = Monstey_Frame::base + f.bytes.size() - 4;
    f.Generated(0, 5, 1);
    Monstey_Expected(f, 1, 0, {Monstey_Kind::Range, 0, f.target});
  }
  for (int kind : std::array{0, 1}) {
    Monstey_Frame f;
    f.Generated(0, 5, 1, 1, 0);
    f.Put(f.target + 4, 0, 4);
    f.def = Monstey_Frame::base - 8;
    Monstey_Expected(f, kind, 0, {Monstey_Kind::Value, 0, 0});
  }
  for (auto type : std::array<Monstey_Word32, 2>{8, 9}) {
    Monstey_Frame f;
    f.Generated(0x80000000u, type, 0xffffffffu, 1, 0x12345678);
    Monstey_Expected(f, 1, 0x80000000u, {Monstey_Kind::Value, 0x12345678, 0});
  }
  {
    Monstey_Frame f;
    f.Generated(0, 5, 0xffffffffu, 0);
    Monstey_Expected(f, 1, 0, {Monstey_Kind::Dynamic, 0, f.storage + 8});
  }
  for (int kind : std::array{0, 2}) {
    Monstey_Frame f;
    f.Raw(65536, 65535, 5);
    f.Put(f.record + 16, UINT64_MAX, 8);
    Monstey_Expected(f, kind, 65536, {Monstey_Kind::Value, 0xfedcba98, 0});
  }
  for (int kind : std::array{0, 2}) {
    Monstey_Frame f;
    f.Raw(65534, 65535, 5, 0, 65535, 0x80000000);
    Monstey_Expected(f, kind, 65534, {Monstey_Kind::Value, 0x80000000, 0});
  }
  {
    Monstey_Frame f;
    f.Raw(0, 1, 5);
    Monstey_Expected(f, 2, 0, {Monstey_Kind::Value, 0x81234567, 0}, 0x108);
    Monstey_Expected(f, 0, 0, {Monstey_Kind::Range, 0, f.record + 1}, 0x108);
  }
  ++Monstey_Groups;
}
void Monstey_GeneratedBranches() {
  for (Monstey_Word32 variant = 0; variant < 8; ++variant)
    for (int kind = 0; kind < 2; ++kind) {
      Monstey_Frame f;
      f.Generated(0, 5, 1);
      if (variant == 0)
        f.Put(f.record + 24, 0, 8);
      if (variant == 1)
        f.Put(f.owner + 32, 0, 8);
      if (variant == 2)
        f.Put(f.cls + 8, 0, 4);
      if (variant == 3)
        f.Put(f.rows + 72 + 32, 0, 4);
      if (variant == 4)
        f.Put(f.cls, 0, 8);
      if (variant == 5)
        f.Put(f.field, 0, 4);
      if (variant == 6)
        f.Put(f.record + 24, Monstey_Frame::base - 33, 8);
      if (variant == 7)
        f.Put(f.owner + 32, Monstey_Frame::base - 9, 8);
      Monstey_Compare(f, kind);
    }
  ++Monstey_Groups;
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Report required");
    static_assert(
        std::is_same_v<decltype(Monstey_GadbReadIntegerWord({}, 0, 0, 0, 0)),
                       std::uint32_t>);
    static_assert(std::is_same_v<decltype(Monstey_GadbReadCachedIntegerWord(
                                     {}, 0, 0, 0, 0)),
                                 std::uint32_t>);
    Monstey_TypesAndCached();
    Monstey_WidthsAndAliases();
    Monstey_GeneratedMatrix();
    Monstey_Lookups();
    Monstey_LazyFaultWindows();
    Monstey_GeneratedBranches();
    Monstey_AnalyticBoundaries();
    const auto report =
        Monstey_Json::object({{"schema", "Monstey.IntegerIndependentTests/1"},
                              {"passed", true},
                              {"groups", Monstey_Groups},
                              {"cases", Monstey_Cases},
                              {"checks", Monstey_Checks},
                              {"range_cases", Monstey_RangeCases},
                              {"dynamic_cases", Monstey_DynamicCases},
                              {"synthetic_only", true},
                              {"original_entrypoints_executed", false}});
    std::ofstream(argv[1], std::ios::binary) << report.dump(2) << '\n';
    std::cout << "PASS " << Monstey_Groups << " groups " << Monstey_Cases
              << " cases " << Monstey_Checks << " checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
