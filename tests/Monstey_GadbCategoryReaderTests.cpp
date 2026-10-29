#include "Monstey_GadbCategoryReader.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>
using namespace Monstey;
namespace {
std::uint64_t Monstey_CategoryCases = 0, Monstey_CategoryChecks = 0,
              Monstey_CategoryFaults = 0, Monstey_CategoryEvents = 0;
void Monstey_CategoryCheck(bool v, const char *why) {
  ++Monstey_CategoryChecks;
  if (!v)
    throw std::runtime_error(why);
}
struct Monstey_CategoryFixture {
  static constexpr std::uint64_t base = 0x142A58000ULL, stream = base + 0x10000,
                                 stream_table = base + 0x11000,
                                 inner_table = base + 0x11100,
                                 selector = base + 0x12000,
                                 selector_table = base + 0x12100,
                                 bank_table = base + 0x13000,
                                 cache = base + 0x14000, cell = base + 0x18000,
                                 pool = base + 0x20000,
                                 records = base + 0x30000,
                                 words = base + 0x34000,
                                 descriptors = base + 0x36000,
                                 record_cursor = base + 0x38000,
                                 value_cursor = base + 0x38100,
                                 category = base + 0x3A000,
                                 lookup = base + 0x3B000,
                                 lookup_rows = base + 0x3C000,
                                 f = base + 0x50000,
                                 parent = 0xAABBCCDDEEFF0011ULL;
  static constexpr Monstey_GadbBufferedCells cells{cell, cell + 8, cell + 16};
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x80000, 0xA5),
                            source;
  Monstey_GadbBankEpochWord global{0x142AA2B80, -100}, guard{0x142AA2B4C, -10};
  std::array<Monstey_GadbBankEpochWord *, 1> guards{&guard};
  std::int32_t epoch = -1;
  Monstey_GadbBankSynchronizer sync{{true, true, true, true, 0}};
  Monstey_GadbNonnullPointerComparator old_compare;
  std::array<Monstey_GadbBankRecipe, 2> recipes{
      {{0x14482A370, 0, 0x142A59E08, 0, {}},
       {0x14161D594, 0x142AA2B4C, 0x142A59E10, 15, {}}}};
  Monstey_GadbBankActivation activation{{base, bytes}, recipes,    guards,
                                        global,        epoch,      sync,
                                        bank_table,    old_compare};
  Monstey_GadbBankSelectorServices selectors;
  Monstey_GadbBufferedServices buffered;
  std::function<void(const Monstey_GadbBankSelectorCompareCall &)>
      compare_effect;
  std::size_t copies = 0, compares = 0, position = 0;
  unsigned anonymous = 0, named = 0, values = 0, type = 1;
  bool io = false;
  explicit Monstey_CategoryFixture(unsigned a = 0, unsigned n = 0,
                                   unsigned v = 0, unsigned t = 1,
                                   bool use_io = false)
      : anonymous(a), named(n), values(v), type(t), io(use_io) {
    Append(0, 4);
    Append(8, 4);
    Append(16, 4);
    Append(a, 4);
    Append(n, 4);
    Append(0x77777777, 4);
    for (unsigned i = 0; i < a + n; ++i) {
      if (i >= a)
        Append(32 + (i - a) * 8, 4);
      Append(v, 2);
      Append(1, 1);
      Append(i, 4);
      Append(0xA5, 1);
      for (unsigned k = 0; k < v; ++k)
        Append(4 + k * 8, 4);
      Write(descriptors + i * 16 + 0xA, t, 1);
      Write(descriptors + i * 16 + 0xC, v, 2);
      Write(descriptors + i * 16 + 0xE, 0, 2);
    }
    PutString(pool, "Cat");
    PutString(pool + 8, "Second");
    PutString(pool + 16, "Third");
    for (unsigned i = 0; i < n; ++i) {
      PutString(pool + 32 + i * 8, "N0");
      Write(pool + 33 + i * 8, '0' + i, 1);
    }
    Write(stream, stream_table, 8);
    Write(stream_table + 8, 0x141B50DE8, 8);
    Write(stream + 8, inner_table, 8);
    Write(inner_table + 0x18, 0x141204C2C, 8);
    Write(stream + 0x10, 0x8888, 8);
    Write(stream + 0x18, cache, 8);
    Write(stream + 0x20, 0, 4);
    Write(stream + 0x24, io ? 0 : source.size(), 4);
    Write(stream + 0x28, 0, 1);
    for (std::size_t i = 0; i < source.size(); ++i)
      Write(cache + i, source[i], 1);
    Write(selector, selector_table, 8);
    Write(selector_table, 0x1404BA96C, 8);
    for (unsigned i = 0; i < 6; ++i) {
      Write(bank_table + i * 24, 0xBEEF + i, 8);
      Write(bank_table + i * 24 + 8, 0x14482A370, 8);
    }
    Write(cell, 0x1111, 8);
    Write(cell + 8, 0x2222, 8);
    Write(cell + 16, 0x3333, 8);
    Write(cell + 24, 0x4444, 8);
    Write(f + 0x50, stream, 8);
    Write(f + 0x58, selector, 8);
    Write(f + 0x60, 0xFEEDABCD, 4);
    Write(f + 0x68, pool, 8);
    Write(f + 0x70, descriptors, 8);
    Write(f + 0x78, record_cursor, 8);
    Write(f + 0x80, value_cursor, 8);
    Write(record_cursor, records, 8);
    Write(value_cursor, words, 8);
    selectors.compare_nonnull =
        [this](Monstey_GadbBinderMemory,
               const Monstey_GadbBankSelectorCompareCall &c) {
          ++compares;
          ++Monstey_CategoryEvents;
          if (compare_effect)
            compare_effect(c);
          return Monstey_GadbBufferedResult{c.left == c.right ? 0ULL : 1ULL,
                                            0xAABB0000 + compares};
        };
    buffered.copy = [this](Monstey_GadbBinderMemory,
                           const Monstey_GadbBufferedCopyCall &c) {
      for (std::uint64_t k = 0; k < c.count; ++k)
        Write(c.destination + k, Read(c.source + k, 1), 1);
      ++copies;
      ++Monstey_CategoryEvents;
      return Monstey_GadbBufferedResult{0x123456789ABCDEF1ULL,
                                        0xABC00000 + copies};
    };
    buffered.fill = [this](Monstey_GadbBinderMemory,
                           const Monstey_GadbBufferedFillCall &c) {
      for (std::uint64_t k = 0; k < c.count; ++k)
        Write(c.destination + k, c.value, 1);
      ++Monstey_CategoryEvents;
      return Monstey_GadbBufferedResult{0x23456789ABCDEF12ULL,
                                        0xDEAD0123456789ABULL};
    };
    buffered.read_file = [this](Monstey_GadbBinderMemory,
                                const Monstey_GadbBufferedReadFileCall &c) {
      const auto got = static_cast<std::uint32_t>(
          std::min<std::size_t>(c.count, source.size() - position));
      for (std::uint32_t k = 0; k < got; ++k)
        Write(c.destination + k, source[position++], 1);
      Write(c.bytes_read_address, got, 4);
      ++Monstey_CategoryEvents;
      return Monstey_GadbBufferedResult{0x1234567800000001ULL,
                                        0xABCD123456781234ULL};
    };
  }
  void Append(std::uint32_t v, unsigned n) {
    for (unsigned k = 0; k < n; ++k)
      source.push_back(static_cast<std::uint8_t>(v >> (k * 8)));
  }
  std::size_t Offset(std::uint64_t a, std::size_t n) const {
    if (a < base || a - base > bytes.size() ||
        n > bytes.size() - static_cast<std::size_t>(a - base))
      throw std::runtime_error("Synthetic service range");
    return static_cast<std::size_t>(a - base);
  }
  void Write(std::uint64_t a, std::uint64_t v, std::size_t n) {
    const auto p = Offset(a, n);
    for (std::size_t i = 0; i < n; ++i)
      bytes[p + i] = static_cast<std::uint8_t>(v >> (i * 8));
  }
  std::uint64_t Read(std::uint64_t a, std::size_t n) const {
    const auto p = Offset(a, n);
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < n; ++i)
      v |= static_cast<std::uint64_t>(bytes[p + i]) << (i * 8);
    return v;
  }
  void PutString(std::uint64_t a, const char *s) {
    do {
      Write(a++, static_cast<std::uint8_t>(*s), 1);
    } while (*s++);
  }
  void PatchHeader(unsigned at, std::uint32_t value) {
    for (unsigned k = 0; k < 4; ++k) {
      source.at(at + k) = static_cast<std::uint8_t>(value >> (k * 8));
      Write(cache + at + k, source.at(at + k), 1);
    }
  }
  Monstey_GadbBufferedResult Run(std::uint64_t out = category,
                                 std::uint64_t existing = 0) {
    ++Monstey_CategoryCases;
    return Monstey_GadbLoadCategoryFromBuffered(activation, parent, 0xBEEF, out,
                                                existing, f, selectors,
                                                buffered, cells, cell + 24);
  }
};
constexpr std::uint32_t Monstey_CategoryHash =
    (3u * 0x397u + 1u) * 0x397u + 20u;
void Monstey_CategoryRegular() {
  for (unsigned a = 0; a < 3; ++a)
    for (unsigned n = 0; n < 3; ++n)
      for (unsigned v = 0; v < 3; ++v)
        for (unsigned mode = 0; mode < 2; ++mode) {
          const unsigned type = (v == 2 ? 0xC4 : 1);
          Monstey_CategoryFixture x(a, n, v, type, mode != 0);
          const auto r = x.Run();
          Monstey_CategoryCheck(
              r.rax == ((Monstey_CategoryHash & ~255u) | 1u) && r.rdx == 20,
              "Final hash rawRAX/RDX");
          Monstey_CategoryCheck(x.Read(x.category, 8) == x.pool &&
                                    x.Read(x.category + 8, 8) == x.pool + 8 &&
                                    x.Read(x.category + 0x10, 8) == x.pool + 16,
                                "Category strings");
          Monstey_CategoryCheck(x.Read(x.category + 0x18, 8) == x.parent,
                                "Category parent");
          Monstey_CategoryCheck(x.Read(x.category + 0x28, 4) == n &&
                                    x.Read(x.category + 0x40, 4) == a,
                                "Category counts");
          Monstey_CategoryCheck(x.Read(x.category + 0x2C, 4) == 0xFEEDABCD,
                                "Late scalar");
          Monstey_CategoryCheck(x.Read(x.category + 0x20, 8) == 0 &&
                                    x.Read(x.category + 0x30, 1) == 0,
                                "Posthash clears");
          Monstey_CategoryCheck(x.Read(x.category + 0x38, 8) ==
                                        x.records + a * 32 &&
                                    x.Read(x.category + 0x48, 8) == x.records,
                                "Record bases");
          Monstey_CategoryCheck(x.Read(x.record_cursor, 8) ==
                                    x.records + a * 32 + n * 40,
                                "Record reservation");
          Monstey_CategoryCheck(x.Read(x.value_cursor, 8) ==
                                    x.words + (a + n) * v * 8,
                                "Value reservation");
          Monstey_CategoryCheck(x.compares == 1,
                                "Concrete selector not consumed");
          Monstey_CategoryCheck(x.Read(x.f + 0x30, 8) == x.parent,
                                "Known savedRCX");
          for (unsigned i = 0; i < a + n; ++i) {
            const auto record =
                i < a ? x.records + i * 32 : x.records + a * 32 + (i - a) * 40;
            Monstey_CategoryCheck(x.Read(record, 1) == 1 &&
                                      x.Read(record + 1, 1) ==
                                          (i < a ? 0xA5 : 0xA7),
                                  "Child flags/count");
            Monstey_CategoryCheck(x.Read(record + 8, 8) ==
                                          x.descriptors + i * 16 &&
                                      x.Read(record + 0x18, 8) == x.category,
                                  "Child fields");
            Monstey_CategoryCheck(
                x.Read(record + 0x10, 8) == x.words + i * v * 8, "Child words");
            for (unsigned k = 0; k < v; ++k)
              Monstey_CategoryCheck(x.Read(x.words + (i * v + k) * 8, 8) ==
                                        (type == 0xC4 ? x.pool : 0) + 4 + k * 8,
                                    "Pool-relative words");
            Monstey_CategoryCheck(x.Read(x.descriptors + i * 16 + 0xA, 1) ==
                                      type,
                                  "No early semantic fixup");
            if (i < a)
              Monstey_CategoryCheck(x.Read(record + 4, 4) == 0xA5A5A5A5,
                                    "Anonymous padding preserved");
            else {
              const unsigned j = i - a;
              Monstey_CategoryCheck(x.Read(record + 4, 4) ==
                                        14u * 0x397u + 27u + j,
                                    "Named hash");
              Monstey_CategoryCheck(x.Read(record + 0x20, 8) ==
                                        x.pool + 32 + j * 8,
                                    "Named string");
            }
          }
          if (a + n == 0)
            Monstey_CategoryCheck(x.Read(x.f - 0x40, 1) == 0xA5,
                                  "Category prezeroed childflags");
        }
}
void Monstey_CategoryLookup() {
  for (unsigned extra :
       std::array<unsigned, 5>{0, 1, 7, 0x40000000u, 0xFFFFFFFFu}) {
    Monstey_CategoryFixture x(1, 1, 1, 1);
    x.Write(x.lookup + 0x48, 1, 4);
    x.Write(x.lookup + 0x50, x.lookup_rows, 8);
    x.Write(x.lookup_rows + 0x50, Monstey_CategoryHash, 4);
    x.Write(x.lookup_rows + 0x28, extra, 4);
    const auto r = x.Run(x.category, x.lookup);
    const auto size = static_cast<std::uint32_t>((1u + extra) * 40u);
    Monstey_CategoryCheck(static_cast<std::uint8_t>(r.rax) == 1 &&
                              x.Read(x.record_cursor, 8) ==
                                  x.records + 32 + size,
                          "Lookupextra only reserve/U32wrap");
    Monstey_CategoryCheck(x.Read(x.category + 0x28, 4) == 1,
                          "Lookupextra altered loop/count");
    Monstey_CategoryCheck(x.Read(x.f + 0x50, 4) == Monstey_CategoryHash,
                          "Optional hash spill");
    Monstey_CategoryCheck(x.Read(x.records + 32 + 0x20, 8) == x.pool + 32,
                          "Cachedstream after overwritten arg");
  }
  {
    Monstey_CategoryFixture x;
    x.Write(x.lookup + 0x48, 0, 4);
    x.Write(x.lookup + 0x50, 0, 8);
    const auto r = x.Run(x.category, x.lookup);
    Monstey_CategoryCheck(static_cast<std::uint8_t>(r.rax) == 1 &&
                              x.Read(x.record_cursor, 8) == x.records,
                          "Emptylookup result");
  }
}
void Monstey_CategoryAliases() {
  {
    Monstey_CategoryFixture x(1, 1, 1, 1);
    x.Write(x.f + 0x78, x.f - 0x20, 8);
    x.Write(x.f - 0x20, x.records, 8);
    (void)x.Run();
    Monstey_CategoryCheck(x.Read(x.category + 0x10, 8) == x.records + 72,
                          "Cursor aliases live third-name spill");
    Monstey_CategoryCheck(x.Read(x.category + 0x48, 8) == x.records,
                          "Cached original anonymous cursor");
  }
  {
    Monstey_CategoryFixture x;
    const auto out = x.f - 0x38;
    (void)x.Run(out);
    Monstey_CategoryCheck(x.Read(out + 0x48, 8) == x.pool + 8,
                          "Anonymous pointer late after storecat8");
  }
  {
    Monstey_CategoryFixture x;
    x.Write(x.pool, 0, 1);
    const auto out = x.f + 0x38;
    const auto r = x.Run(out);
    Monstey_CategoryCheck(x.Read(out + 0x2C, 4) == 0 && r.rdx == 0,
                          "LateF+60 after named-count publication");
  }
  {
    Monstey_CategoryFixture x;
    const auto out = x.f + 0x28;
    (void)x.Run(out);
    Monstey_CategoryCheck(x.Read(out + 0x18, 8) == x.parent,
                          "Parent read before firstpublication");
  }
  {
    Monstey_CategoryFixture x(1, 0, 1, 1);
    x.PatchHeader(12, 0x08000000u);
    x.Write(x.value_cursor, x.f - 0x0C, 8);
    x.Write(x.cache + 32, 0, 4); // first anonymous payload after24+8B
    const auto r = x.Run();
    Monstey_CategoryCheck(static_cast<std::uint8_t>(r.rax) == 1 &&
                              x.Read(x.record_cursor, 8) == x.records,
                          "Anonymous U32wrapped reservation");
    Monstey_CategoryCheck(x.Read(x.category + 0x40, 4) == 0 &&
                              x.Read(x.records, 1) == 1,
                          "Anonymous live count/payload alias");
  }
  {
    Monstey_CategoryFixture x(0, 1, 1, 1);
    x.PatchHeader(16, 0x40000000u);
    x.Write(x.value_cursor, x.f - 8, 8);
    x.Write(x.cache + 36, 0, 4); // named payload after24+12B
    const auto r = x.Run();
    Monstey_CategoryCheck(static_cast<std::uint8_t>(r.rax) == 1 &&
                              x.Read(x.record_cursor, 8) == x.records,
                          "Named U32wrapped reservation");
    Monstey_CategoryCheck(x.Read(x.category + 0x28, 4) == 0 &&
                              x.Read(x.records, 1) == 1,
                          "Named live count/payload alias");
  }
  {
    Monstey_CategoryFixture x(1, 0, 0, 1);
    const auto out = x.f - 0x0C;
    x.Write(x.record_cursor, x.f + 0x60, 8);
    x.Write(x.lookup + 0x48, 1, 4);
    x.Write(x.lookup + 0x50, x.lookup_rows, 8);
    x.Write(x.lookup_rows + 0x50, Monstey_CategoryHash, 4);
    x.Write(x.lookup_rows + 0x28, 1, 4);
    // Anonymous record+18 publishes category=F-C into live F+78 cursor arg.
    // Named reservation then changes header anonymousCount1->41. No Named ran,
    // so the original cached ESI1 is published; it is not freshly reloaded41.
    const auto r = x.Run(out, x.lookup);
    Monstey_CategoryCheck(static_cast<std::uint8_t>(r.rax) == 1,
                          "CachedESI alias failed");
    Monstey_CategoryCheck(x.Read(out + 0x40, 4) == 1 &&
                              x.Read(out + 0x38, 8) == 1,
                          "Named0 must retain cached anonymousCount");
  }
  {
    Monstey_CategoryFixture x(1, 0, 0, 1);
    x.Write(x.record_cursor, x.f + 0x68, 8);
    x.Write(x.value_cursor, x.f - 8, 8);
    x.Write(x.lookup + 0x48, 1, 4);
    x.Write(x.lookup + 0x50, x.lookup_rows, 8);
    x.Write(x.lookup_rows + 0x50, Monstey_CategoryHash, 4);
    x.Write(x.lookup_rows + 0x28, 1, 4);
    // Anonymous record+10 publishes words=F-8 into live F+78 cursor arg.
    // Named reservation mutates header namedCount0->40, after EDX was cached0.
    // It must neither run forty children nor publish a reloaded namedCount40.
    const auto r = x.Run(x.category, x.lookup);
    Monstey_CategoryCheck(static_cast<std::uint8_t>(r.rax) == 1 &&
                              x.Read(x.f - 8, 4) == 40,
                          "CachedEDX reservation alias");
    Monstey_CategoryCheck(x.Read(x.category + 0x28, 4) == 0 &&
                              x.Read(x.category + 0x40, 4) == 1,
                          "Named0 cached counters publication");
  }
}
void Monstey_CategoryFailures() {
  {
    Monstey_CategoryFixture x;
    x.Write(x.stream + 0x28, 1, 1);
    const auto r = x.Run();
    Monstey_CategoryCheck(r.rax == 0 && r.rdx == 0xDEAD0123456789ABULL,
                          "Read24 failure rawregisters");
    Monstey_CategoryCheck(x.Read(x.record_cursor, 8) == x.records &&
                              x.compares == 0,
                          "Read24 failure reserved/published");
    Monstey_CategoryCheck(x.Read(x.f + 0x30, 8) == x.parent,
                          "Readfail known savedRCX missing");
  }
  {
    Monstey_CategoryFixture x;
    x.Write(x.stream_table + 8, 0xBAD, 8);
    try {
      (void)x.Run();
      throw std::logic_error("Unknown Read target accepted");
    } catch (const Monstey_GadbCategoryDispatchFault &e) {
      ++Monstey_CategoryFaults;
      Monstey_CategoryCheck(e.native_site == 0x1404BA0BC && e.target == 0xBAD &&
                                e.slot == x.stream_table + 8,
                            "Read gate metadata");
    }
    Monstey_CategoryCheck(x.Read(x.f + 0x30, 8) == x.parent &&
                              x.Read(x.record_cursor, 8) == x.records,
                          "Initial gate prefix");
  }
  {
    Monstey_CategoryFixture x(2, 0);
    x.Write(x.selector_table, 0xBAD, 8);
    try {
      (void)x.Run();
      throw std::logic_error("Unknownselector accepted");
    } catch (const Monstey_GadbCategoryDispatchFault &e) {
      ++Monstey_CategoryFaults;
      Monstey_CategoryCheck(e.native_site == 0x1404BA10B && e.target == 0xBAD,
                            "Selector gate metadata");
    }
    Monstey_CategoryCheck(x.Read(x.record_cursor, 8) == x.records + 64 &&
                              x.Read(x.f - 0x30, 8) == x.records,
                          "Selector gate reservation/localprefix");
  }
  {
    Monstey_CategoryFixture x(1, 0);
    x.compare_effect = [&x](const Monstey_GadbBankSelectorCompareCall &) {
      x.Write(x.stream_table + 8, 0xBAD, 8);
    };
    try {
      (void)x.Run();
      throw std::logic_error("Child Read not reloaded");
    } catch (const Monstey_GadbPoolReadBoundaryFault &e) {
      ++Monstey_CategoryFaults;
      Monstey_CategoryCheck(e.target == 0xBAD, "Child gate target");
    }
    Monstey_CategoryCheck(x.Read(x.f - 0x68, 8) == x.f - 0xA0,
                          "Selector RDX not fed toAnon home");
  }
  for (unsigned cut = 0; cut < 12; ++cut) {
    Monstey_CategoryFixture x;
    x.Write(x.f + 0x78, Monstey_CategoryFixture::base + x.bytes.size() - cut,
            8);
    if (cut >= 8)
      continue;
    try {
      (void)x.Run();
      throw std::logic_error("Cursorfault missing");
    } catch (const Monstey_GadbPoolRelocationFault &e) {
      ++Monstey_CategoryFaults;
      Monstey_CategoryCheck(e.width == 8 && !e.writing,
                            "Cursorfault readprefix");
    }
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::runtime_error("Usage NEW_REPORT NONCE");
    Monstey_CategoryRegular();
    Monstey_CategoryLookup();
    Monstey_CategoryAliases();
    Monstey_CategoryFailures();
    std::ofstream out(argv[1], std::ios::binary);
    if (!out)
      throw std::runtime_error("Reportopen");
    out << "{\"status\":\"PASS\",\"groups\":4,\"cases\":"
        << Monstey_CategoryCases << ",\"checks\":" << Monstey_CategoryChecks
        << ",\"faults\":" << Monstey_CategoryFaults
        << ",\"primitive_events\":" << Monstey_CategoryEvents << ",\"nonce\":\""
        << argv[2] << "\"}\n";
    std::cout << "PASS cases" << Monstey_CategoryCases << " checks"
              << Monstey_CategoryChecks << " faults" << Monstey_CategoryFaults
              << " events" << Monstey_CategoryEvents << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
