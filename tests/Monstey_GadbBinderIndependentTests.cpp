#include "Monstey_GadbBinderComposition.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using U64 = std::uint64_t;
using U32 = std::uint32_t;
using U8 = std::uint8_t;
using Fault = Monstey_GadbBinderFaultKind;
std::uint64_t cases = 0, checks = 0, compared = 0;
unsigned groups = 0;
void Check(bool v, const char *why) {
  ++checks;
  if (!v)
    throw std::runtime_error(why);
}
struct Monstey_Stop {
  Fault kind;
  U64 address;
};
struct Monstey_Image {
  U64 base = 0x100000000ull;
  std::vector<U8> bytes;
  explicit Monstey_Image(std::size_t n = 32768) : bytes(n, 0xa7) {}
  U64 A(U64 offset) const { return base + offset; }
  std::size_t Pos(U64 p, std::size_t n) const {
    if (p < base || p - base > bytes.size() ||
        n > bytes.size() - static_cast<std::size_t>(p - base))
      throw Monstey_Stop{Fault::ConsumedRangeUnavailable, p};
    return static_cast<std::size_t>(p - base);
  }
  U64 R(U64 p, std::size_t n) const {
    const auto o = Pos(p, n);
    U64 v = 0;
    for (std::size_t i = 0; i < n; ++i)
      v |= static_cast<U64>(bytes[o + i]) << (i * 8);
    return v;
  }
  void W(U64 p, U64 v, std::size_t n) {
    const auto o = Pos(p, n);
    for (std::size_t i = 0; i < n; ++i)
      bytes[o + i] = static_cast<U8>(v >> (i * 8));
  }
  void Text(U64 p, const std::string &text) {
    for (const auto c : text)
      W(p++, static_cast<U8>(c), 1);
    W(p, 0, 1);
  }
  void Class(U64 cl, U64 table, U32 count, U64 parent, U32 mask, U8 init) {
    W(cl, table, 8);
    W(cl + 8, count, 4);
    W(cl + 40, parent, 8);
    W(cl + 56, mask, 4);
    W(cl + 60, init, 1);
  }
  void Field(U64 f, U64 name, U32 hash, U32 type, U32 count, U32 flags,
             U32 state, U64 code, U8 cache) {
    W(f, name, 8);
    W(f + 32, hash, 4);
    W(f + 36, type, 4);
    W(f + 40, count, 4);
    W(f + 44, flags, 4);
    W(f + 48, state, 4);
    W(f + 56, code, 8);
    W(f + 64, cache, 1);
  }
  Monstey_GadbBinderMemory View() { return {base, bytes}; }
};
// Separate scalar instruction-order reference. No production binder helper is
// called.
struct Monstey_Reference {
  Monstey_Image &m;
  static std::int32_t HashTerm(U8 c) {
    if (c >= 'A' && c <= 'Z')
      return c - 'A' + 1;
    if (c >= 'a' && c <= 'z')
      return c - 'a' + 1;
    if (c >= 0x20 && c < 0x41) {
      constexpr std::array<U8, 33> x = {
          0x25, 0x3a, 0x33, 0x3c, 0x3d, 0x3e, 0x40, 0x32, 0x42, 0x43, 0x41,
          0x28, 0x36, 0x27, 0x37, 0x34, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
          0x21, 0x22, 0x23, 0x24, 0x31, 0x30, 0x38, 0x29, 0x39, 0x35, 0x3b};
      return x[c - 0x20];
    }
    switch (c) {
    case '[':
      return 0x2c;
    case '\\':
      return 0x2a;
    case ']':
      return 0x2d;
    case '^':
      return 0x3f;
    case '_':
      return 0x26;
    case '`':
      return 0x44;
    case '{':
      return 0x2e;
    case '|':
      return 0x2b;
    case '}':
      return 0x2f;
    case '~':
      return 0x45;
    default:
      return 0;
    }
  }
  U32 Hash(U64 p) {
    U32 h = 0;
    for (;;) {
      const auto c = static_cast<U8>(m.R(p++, 1));
      if (!c)
        return h;
      h = h * 0x397u + static_cast<U32>(HashTerm(c));
    }
  }
  void Init(U64 cl) {
    if (m.R(cl + 60, 1))
      return;
    const auto parent = m.R(cl + 40, 8);
    if (parent)
      Init(parent);
    if (!(m.R(cl + 56, 1) & 4)) {
      U32 i = 0, k = 0;
      while (i < m.R(cl + 8, 4)) {
        const auto row = m.R(cl, 8) + 72ull * i;
        if (row && !(m.R(row + 44, 4) & 0x280)) {
          m.W(row + 64, k & 255, 1);
          ++k;
        }
        ++i;
      }
    }
    m.W(cl + 60, 1, 1);
  }
  void Spread(U64 cl, U64 f, U32 i, bool good) {
    for (;;) {
      const auto state = m.R(f + 48, 4);
      if (good ? state != 0 : state == 1)
        return;
      m.W(f + 48, good ? 2 : 1, 4);
      cl = m.R(cl + 40, 8);
      if (!cl || i >= m.R(cl + 8, 4))
        return;
      const auto pf = m.R(cl, 8) + 72ull * i;
      for (const U64 member : {32ull, 36ull, 44ull, 40ull, 64ull}) {
        const std::size_t w = member == 64 ? 1 : 4;
        const auto v = m.R(f + member, w);
        if (m.R(pf + member, w) != v)
          throw Monstey_Stop{Fault::NativeParentCorrespondenceAssertion,
                             pf + member};
      }
      f = pf;
    }
  }
  U8 Index(U64 code) {
    const auto c = m.R(code, 1);
    if (c == 0xb0) {
      const auto v = m.R(code + 1, 1);
      if (m.R(code + 2, 1) == 0xc3)
        return static_cast<U8>(v);
    } else if (c == 0xb8) {
      const auto v = m.R(code + 1, 4);
      if (m.R(code + 5, 1) == 0xc3)
        return static_cast<U8>(v);
    } else if (c == 0x30 || c == 0x32 || c == 0x33) {
      if (m.R(code + 1, 1) == 0xc0 && m.R(code + 2, 1) == 0xc3)
        return 0;
    }
    throw Monstey_Stop{Fault::UnsupportedIndexCallback, code};
  }
  bool Gate(U64 cat) {
    if (m.R(cat + 40, 4) != 0)
      return true;
    return m.R(cat + 64, 4) != 0;
  }
  void Bind(U64 cl, U64 cat) {
    if (!cl)
      return;
    Init(cl);
    m.W(cat + 32, cl, 8);
    U64 record;
    if (m.R(cat + 40, 4))
      record = m.R(cat + 56, 8);
    else {
      if (!m.R(cat + 64, 4))
        return;
      record = m.R(cat + 72, 8);
    }
    if (!record)
      return;
    const auto count = static_cast<U32>(m.R(cl + 8, 4));
    const auto mask = m.R(cl + 56, 4) & 4;
    const auto entries = m.R(record, 1);
    for (U32 i = 0; i < count; ++i) {
      const auto row = m.R(cl, 8) + 72ull * i;
      if (m.R(row + 48, 4) == 1)
        continue;
      if (m.R(row + 44, 4) & 0x280)
        continue;
      if (m.R(row + 64, 1) >= entries) {
        Spread(cl, row, i, false);
        continue;
      }
      const auto cache = m.R(row + 64, 1);
      const auto name = m.R(row, 8);
      const auto d = m.R(record + 8, 8) + cache * 16;
      const auto identity = m.R(d + 8, 1);
      const auto hash = Hash(name);
      const auto type = m.R(d + 10, 1) & 63;
      bool type_ok = m.R(row + 36, 4) == type;
      if (!type_ok)
        type_ok = m.R(row + 36, 4) == 11 && type == 10;
      const auto stored = m.R(d + 12, 2);
      bool count_ok = m.R(row + 40, 4) == stored;
      if (!count_ok)
        count_ok = m.R(row + 40, 4) == UINT32_MAX;
      const auto index = Index(m.R(row + 56, 8));
      bool good = false;
      if (index == identity) {
        if (m.R(d, 4) == hash && type_ok && count_ok && mask == 0)
          good = true;
      }
      Spread(cl, row, i, good);
    }
  }
};
struct Monstey_Result {
  int kind = -1;
  U64 address = 0;
};
template <class F> Monstey_Result RefRun(F &&f) {
  try {
    f();
    return {};
  } catch (const Monstey_Stop &e) {
    return {static_cast<int>(e.kind), e.address};
  }
}
template <class F> Monstey_Result ProductRun(F &&f) {
  try {
    f();
    return {};
  } catch (const Monstey_GadbBinderFault &e) {
    return {static_cast<int>(e.kind), e.address};
  }
}
void Equal(const Monstey_Image &a, const Monstey_Image &b,
           const Monstey_Result &x, const Monstey_Result &y) {
  Check(x.kind == y.kind, "Independent stop kind differs");
  Check(x.address == y.address, "Independent stop address differs");
  Check(a.bytes == b.bytes, "Independent whole byte image differs");
  compared += a.bytes.size();
  ++cases;
}
void CompareBind(Monstey_Image &a, Monstey_Image &b, U64 cl, U64 cat) {
  Monstey_Reference r{b};
  Monstey_GadbBinderTrace t;
  const auto x = ProductRun([&] {
    Monstey_GadbBindSelectedCategory(a.View(), cl, cat, t, 10000000);
  });
  const auto y = RefRun([&] { r.Bind(cl, cat); });
  Equal(a, b, x, y);
}
U32 Random(std::mt19937 &r) { return static_cast<U32>(r()); }
void RandomCohorts() {
  ++groups;
  std::mt19937 rng(0x63b119e2);
  constexpr std::array<U32, 5> states = {0, 1, 2, UINT32_MAX, 0x80000000};
  constexpr std::array<U32, 6> markers = {0, 0, 0, 0x80, 0x200, 0x100};
  for (U32 run = 0; run < 10000; ++run) {
    Monstey_Image a;
    const auto cl = a.A(0x100), cat = a.A(0x800), rec = a.A(0x900),
               desc = a.A(0x1000), code = a.A(0x2000), names = a.A(0x3000);
    const U32 n = Random(rng) % 13, depth = 1 + Random(rng) % 4;
    std::vector<U32> hashes;
    for (U32 i = 0; i < n; ++i) {
      std::string name;
      for (U32 k = 0; k < 5; ++k)
        name.push_back(static_cast<char>(Random(rng) & 255));
      a.Text(names + 32ull * i, name);
      Monstey_Reference r{a};
      const auto hash = r.Hash(names + 32ull * i);
      hashes.push_back(hash);
      a.W(desc + 16ull * i, hash + (Random(rng) % 4 == 0 ? 1 : 0), 4);
      a.W(desc + 16ull * i + 8, Random(rng) % 4, 1);
      a.W(desc + 16ull * i + 10, Random(rng) % 13, 1);
      a.W(desc + 16ull * i + 12, Random(rng) % 3, 2);
      a.W(code + 8ull * i, 0xb0, 1);
      a.W(code + 8ull * i + 1, Random(rng) % 4, 1);
      a.W(code + 8ull * i + 2, 0xc3, 1);
    }
    for (U32 d = 0; d < depth; ++d) {
      const auto table = a.A(0x4000 + 0x400 * d);
      a.Class(cl + 128ull * d, table, n,
              d + 1 < depth ? cl + 128ull * (d + 1) : 0,
              Random(rng) % 7 == 0 ? 4 : 0, Random(rng) % 4 == 0 ? 1 : 0);
      for (U32 i = 0; i < n; ++i)
        a.Field(table + 72ull * i, names + 32ull * i, hashes[i],
                Random(rng) % 13,
                Random(rng) % 5 == 0 ? UINT32_MAX : Random(rng) % 3,
                markers[Random(rng) % markers.size()],
                states[Random(rng) % states.size()], code + 8ull * i,
                static_cast<U8>(Random(rng) % 15));
    }
    a.W(cat + 40, Random(rng) % 3, 4);
    a.W(cat + 56, Random(rng) % 8 == 0 ? 0 : rec, 8);
    a.W(cat + 64, Random(rng) % 3, 4);
    a.W(cat + 72, rec, 8);
    a.W(rec, Random(rng) % 14, 1);
    a.W(rec + 8, desc, 8);
    auto b = a;
    CompareBind(a, b, cl, cat);
    for (U32 step = 0; step < 2; ++step) {
      a.W(rec, Random(rng) % 14, 1);
      b.W(rec, a.R(rec, 1), 1);
      if (n) {
        const auto v = Random(rng) % 3;
        a.W(desc + 12, v, 2);
        b.W(desc + 12, v, 2);
      }
      CompareBind(a, b, cl, cat);
    }
  }
}
void InitializerAlias() {
  ++groups;
  for (U32 count = 0; count < 320; ++count)
    for (U32 mask : {0u, 4u}) {
      Monstey_Image a(65536);
      const auto cl = a.A(0x400), table = cl - 56;
      a.W(table + 44, 0, 4);
      a.Class(cl, table, count, 0, mask, 0);
      auto b = a;
      Monstey_Reference r{b};
      Monstey_GadbBinderTrace t;
      const auto x = ProductRun(
          [&] { Monstey_GadbInitializeSelectedClass(a.View(), cl, t); });
      const auto y = RefRun([&] { r.Init(cl); });
      Equal(a, b, x, y);
    }
}
void PropagationOverlaps() {
  ++groups;
  std::mt19937 rng(0x29f176cb);
  for (U32 run = 0; run < 6000; ++run) {
    Monstey_Image a;
    const auto cl = a.A(0x100), p = a.A(0x200), f = a.A(0x5000);
    const auto pf = f + 8 * (Random(rng) % 10);
    a.Class(cl, f, 1, p, 0, 1);
    a.Class(p, pf, 1, 0, 0, 1);
    for (U32 i = 0; i < 144; ++i)
      a.W(f + i, Random(rng) % 4, 1);
    a.W(f + 48, Random(rng) % 3, 4);
    a.W(pf + 48, Random(rng) % 3, 4);
    const bool good = (Random(rng) & 1) != 0;
    auto b = a;
    Monstey_Reference r{b};
    Monstey_GadbBinderTrace t;
    const auto x = ProductRun([&] {
      Monstey_GadbPropagateSelectedField(a.View(), cl, f, 0, good, t);
    });
    const auto y = RefRun([&] { r.Spread(cl, f, 0, good); });
    Equal(a, b, x, y);
  }
}
void GateAndUnavailable() {
  ++groups;
  for (U32 named : {0u, 1u, UINT32_MAX})
    for (U32 anon : {0u, 1u, UINT32_MAX})
      for (bool clear : {false, true}) {
        Monstey_Image a;
        const auto cat = a.A(0x200), cl = a.A(0x100);
        a.Class(cl, 0, 0, 0, 0, 0);
        a.W(cat + 40, named, 4);
        a.W(cat + 64, anon, 4);
        a.W(cat + 56, 0, 8);
        a.W(cat + 72, 0, 8);
        auto b = a;
        Monstey_GadbBinderTrace t;
        Monstey_Reference r{b};
        Check(Monstey_GadbCategoryHasBindingRecords(a.View(), cat, t) ==
                  r.Gate(cat),
              "Independent preselection gate differs");
        if (clear) {
          a.W(cat + 40, 0, 4);
          b.W(cat + 40, 0, 4);
          a.W(cat + 64, 0, 4);
          b.W(cat + 64, 0, 4);
        }
        CompareBind(a, b, cl, cat);
      }
  for (U32 mode = 0; mode < 6; ++mode) {
    Monstey_Image a;
    const auto cl = a.A(0x100), f = a.A(0x1000), cat = a.A(0x300),
               rec = a.A(0x500), desc = a.A(0x600), code = a.A(0x700),
               name = a.A(0x800);
    a.Class(cl, f, 1, 0, 0, 0);
    a.Text(name, "test");
    Monstey_Reference r{a};
    const auto hash = r.Hash(name);
    a.Field(f, name, hash, 2, 1, 0, 0, code, 0);
    a.W(code, 0xc300b0, 3);
    a.W(cat + 40, 1, 4);
    a.W(cat + 56, rec, 8);
    a.W(cat + 64, 0, 4);
    a.W(rec, 1, 1);
    a.W(rec + 8, desc, 8);
    a.W(desc, hash, 4);
    a.W(desc + 8, 0, 1);
    a.W(desc + 10, 2, 1);
    a.W(desc + 12, 1, 2);
    if (mode == 0)
      a.W(f, ~U64{0}, 8);
    if (mode == 1)
      a.W(f + 56, ~U64{0}, 8);
    if (mode == 2)
      a.W(code, 0x90, 1);
    if (mode == 3)
      a.W(rec + 8, ~U64{0}, 8);
    if (mode == 4)
      a.W(cat + 56, ~U64{0}, 8);
    if (mode == 5) {
      a.W(f + 36, 99, 4);
      a.W(f + 40, 99, 4);
      a.W(code, 0x90, 1);
    }
    auto b = a;
    CompareBind(a, b, cl, cat);
  }
}
} // namespace
int main() {
  try {
    RandomCohorts();
    InitializerAlias();
    PropagationOverlaps();
    GateAndUnavailable();
    std::cout << "{\"status\":\"PASS\",\"groups\":" << groups
              << ",\"cases\":" << cases << ",\"checks\":" << checks
              << ",\"whole_image_bytes_compared\":" << compared
              << ",\"original_code_executed\":false}\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "case=" << cases << ": " << e.what() << '\n';
    return 1;
  }
}