#include "Monstey_GadbBankSelectorRegisters.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>
using namespace Monstey;
namespace {
using Monstey_SelectorTestAddress = std::uint64_t;
std::uint64_t Monstey_SelectorChecks = 0, Monstey_SelectorCases = 0,
              Monstey_SelectorFaults = 0;
void Monstey_SelectorCheck(bool good, const char *why) {
  ++Monstey_SelectorChecks;
  if (!good)
    throw std::runtime_error(why);
}
struct Monstey_SelectorFixture {
  static constexpr std::uint64_t base = 0x142A58000ULL;
  static constexpr std::uint64_t q = base + 0x8000, table = base + 0x9000,
                                 cell = base + 0xA000, object = base + 0xB000,
                                 array2 = base + 0xC000, bank_name = 0x5555,
                                 class_name = 0x6666;
  std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x60000, 0xA5);
  Monstey_GadbBankEpochWord global;
  Monstey_GadbBankEpochWord g0{0x142AA2B68, -10}, g1{0x142AA2B3C, -10},
      g2{0x142A58DF0, -10}, g3{0x142AA2B44, -10}, g4{0x142AA2B4C, -10};
  std::array<Monstey_GadbBankEpochWord *, 5> guards{&g0, &g1, &g2, &g3, &g4};
  std::int32_t epoch = -1;
  Monstey_GadbBankSynchronizer sync{{true, true, true, true, 0}};
  Monstey_GadbNonnullPointerComparator old_comparator;
  std::array<Monstey_GadbBankRecipe, 6> recipes{
      {{0x14162DC24, 0x142AA2B68, 0x142A59EA0, 2687, {}},
       {0x1416248B8, 0x142AA2B3C, 0x142A58C70, 24, {}},
       {0x1404BAA24, 0x142A58DF0, 0x142A58E00, 50, {}},
       {0x14161D674, 0x142AA2B44, 0x142A590E0, 375, {}},
       {0x14161D594, 0x142AA2B4C, 0x142A59E10, 15, {}},
       {0x14482A370, 0, 0x142A59E08, 0, {}}}};
  Monstey_GadbBankActivation activation;
  std::vector<Monstey_GadbBankSelectorCompareCall> calls;
  Monstey_GadbBankSelectorServices services;
  explicit Monstey_SelectorFixture(std::uint64_t gid = 0x142AA2B80)
      : global(gid, -100),
        activation{{base, bytes}, recipes, guards, global,
                   epoch,         sync,    table,  old_comparator} {
    Write(cell, 0x1111222233334444ULL, 8);
    Write(object + 0x20, class_name, 8);
    for (std::size_t i = 0; i < recipes.size(); ++i) {
      Write(table + i * 24, bank_name + i, 8);
      Write(table + i * 24 + 8, recipes[i].enumerator, 8);
      for (std::uint32_t j = 0; j < recipes[i].returned_count; ++j)
        Write(recipes[i].returned_array + static_cast<std::uint64_t>(j) * 8,
              object, 8);
    }
    services.compare_nonnull =
        [this](Monstey_GadbBinderMemory,
               const Monstey_GadbBankSelectorCompareCall &c) {
          calls.push_back(c);
          return Monstey_GadbBufferedResult{c.left == c.right ? 0ULL : 1ULL,
                                            0xBAD00000ULL + calls.size()};
        };
  }
  void Write(std::uint64_t a, std::uint64_t v, std::size_t n) {
    if (a < base || a - base > bytes.size() ||
        n > bytes.size() - static_cast<std::size_t>(a - base))
      throw std::runtime_error("Test write range");
    for (std::size_t i = 0; i < n; ++i)
      bytes[static_cast<std::size_t>(a - base) + i] =
          static_cast<std::uint8_t>(v >> (i * 8));
  }
  std::uint64_t Read(std::uint64_t a, std::size_t n) const {
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < n; ++i)
      v |= static_cast<std::uint64_t>(
               bytes.at(static_cast<std::size_t>(a - base) + i))
           << (i * 8);
    return v;
  }
  Monstey_GadbBufferedResult Run(std::uint64_t bank = bank_name + 4,
                                 std::uint64_t cls = class_name,
                                 std::uint64_t frame = q,
                                 std::uint64_t icell = cell) {
    ++Monstey_SelectorCases;
    return Monstey_GadbFindParserBankClassRegisters(activation, bank, cls,
                                                    frame, services, icell);
  }
};
void Monstey_SelectorProfiles() {
  for (std::size_t bank = 0; bank < 6; ++bank)
    for (std::size_t profile = 0; profile < 6; ++profile) {
      Monstey_SelectorFixture f;
      f.Write(f.table + bank * 24 + 8, f.recipes[profile].enumerator, 8);
      const auto r = f.Run(f.bank_name + bank);
      Monstey_SelectorCheck(r.rax == (profile == 5 ? 0 : f.object),
                            "Native profile selection");
      Monstey_SelectorCheck(
          r.rdx == (profile == 5 ? f.q + 0x20 : 0xBAD00000ULL + bank + 2),
          "Profile rawRDX");
      Monstey_SelectorCheck(f.calls.size() == bank + (profile == 5 ? 1 : 2),
                            "First-bank compare count");
      Monstey_SelectorCheck(f.Read(f.q + 0x68, 4) ==
                                f.recipes[profile].returned_count,
                            "Published count");
      Monstey_SelectorCheck(f.Read(f.q + 0x20, 8) ==
                                f.recipes[profile].returned_array,
                            "Published array");
      Monstey_SelectorCheck(f.sync.Stats().header_calls == 0,
                            "Cached enum fabricated guard init");
      for (const auto &c : f.calls) {
        Monstey_SelectorCheck(c.slot == f.cell &&
                                  c.target == 0x1111222233334444ULL,
                              "Live IAT binding");
        Monstey_SelectorCheck(c.tail_site == 0x14017AE06, "Tail site");
      }
    }
  Monstey_SelectorFixture f;
  f.g4.Store(0);
  f.global.Store(std::numeric_limits<std::int32_t>::min());
  f.epoch = std::numeric_limits<std::int32_t>::min();
  f.recipes[4].constructor_classes.assign(15, f.object + 0x100);
  f.Write(f.object + 0x120, f.class_name, 8);
  const auto r = f.Run();
  Monstey_SelectorCheck(r.rax == f.object + 0x100,
                        "Real constructor not linked");
  Monstey_SelectorCheck(f.sync.Stats().header_calls == 1 &&
                            f.sync.Stats().wake_calls == 1,
                        "Real Header/Footer");
  Monstey_SelectorCheck(f.g4.Load() ==
                            std::numeric_limits<std::int32_t>::min() + 1,
                        "Real guard publication");
}
void Monstey_SelectorRawResults() {
  for (std::uint64_t hi = 1; hi <= 64; ++hi) {
    Monstey_SelectorFixture f;
    f.services.compare_nonnull =
        [&f, hi](Monstey_GadbBinderMemory,
                 const Monstey_GadbBankSelectorCompareCall &c) {
          f.calls.push_back(c);
          return Monstey_GadbBufferedResult{
              (hi << 32) | (c.left == c.right ? 0ULL : 1ULL), ~hi};
        };
    const auto r = f.Run();
    Monstey_SelectorCheck(r.rax == f.object && r.rdx == ~hi,
                          "EAX test/rawRDX lost");
  }
  {
    Monstey_SelectorFixture f;
    const auto before = f.bytes;
    const auto r = f.Run(0x8888, f.class_name, 0);
    Monstey_SelectorCheck(r.rax == 0 && r.rdx == 0xBAD00006,
                          "Six misses rawRDX");
    Monstey_SelectorCheck(f.calls.size() == 6 && before == f.bytes,
                          "Miss touched private frame");
  }
  {
    Monstey_SelectorFixture f;
    const auto r = f.Run(f.bank_name + 4, 0x8888);
    Monstey_SelectorCheck(r.rax == 0 && r.rdx == 0xBAD00014,
                          "Class miss retained lastRDX");
    Monstey_SelectorCheck(f.calls.size() == 20,
                          "Class miss proceeded to laterbank");
  }
}
void Monstey_SelectorReloads() {
  {
    Monstey_SelectorFixture f;
    f.services.compare_nonnull =
        [&f](Monstey_GadbBinderMemory,
             const Monstey_GadbBankSelectorCompareCall &c) {
          f.calls.push_back(c);
          if (c.native_site == 0x1404BA9A2)
            return Monstey_GadbBufferedResult{c.left == c.right ? 0ULL : 1ULL,
                                              7};
          f.Write(f.q + 0x68, 0, 4);
          return Monstey_GadbBufferedResult{1, 0xABC};
        };
    const auto r = f.Run();
    Monstey_SelectorCheck(f.calls.size() == 6 && r.rax == 0 && r.rdx == 0xABC,
                          "Count late reload");
  }
  {
    Monstey_SelectorFixture f;
    f.Write(f.array2 + 8, f.object + 0x100, 8);
    f.Write(f.object + 0x120, 0x7777, 8);
    f.services.compare_nonnull =
        [&f, n = 0U](Monstey_GadbBinderMemory,
                     const Monstey_GadbBankSelectorCompareCall &c) mutable {
          f.calls.push_back(c);
          if (c.native_site == 0x1404BA9A2)
            return Monstey_GadbBufferedResult{c.left == c.right ? 0ULL : 1ULL,
                                              7};
          ++n;
          if (n == 1) {
            f.Write(f.q + 0x20, f.array2, 8);
            f.Write(f.q + 0x68, 2, 4);
            f.Write(f.cell, 0x9191, 8);
            return Monstey_GadbBufferedResult{1, 8};
          }
          Monstey_SelectorCheck(c.left == 0x7777 && c.target == 0x9191,
                                "Nextclass/target reload");
          f.Write(f.array2 + 8, 0xDEAD0000, 8);
          return Monstey_GadbBufferedResult{0, 9};
        };
    const auto r = f.Run();
    Monstey_SelectorCheck(r.rax == 0xDEAD0000 && r.rdx == 9,
                          "Final slot not reread");
    Monstey_SelectorCheck(f.calls.size() == 7, "Mutable callback reset");
  }
  {
    Monstey_SelectorFixture f;
    f.services.compare_nonnull =
        [&f](Monstey_GadbBinderMemory,
             const Monstey_GadbBankSelectorCompareCall &c) {
          f.calls.push_back(c);
          if (c.native_site == 0x1404BA9EC) {
            f.Write(f.q + 0x20, f.array2, 8);
            f.Write(f.array2, 0xCAFEBABE, 8);
          }
          return Monstey_GadbBufferedResult{c.left == c.right ? 0ULL : 1ULL,
                                            0x8989};
        };
    const auto r = f.Run();
    Monstey_SelectorCheck(r.rax == 0xCAFEBABE && r.rdx == 0x8989,
                          "Final array not reread");
  }
  {
    Monstey_SelectorFixture f;
    f.services.compare_nonnull =
        [&f, n = 0U](Monstey_GadbBinderMemory,
                     const Monstey_GadbBankSelectorCompareCall &) mutable {
          ++n;
          return Monstey_GadbBufferedResult{n == 3 || n == 4 ? 0ULL : 1ULL,
                                            0x9000ULL + n};
        };
    const auto r = f.Run();
    Monstey_SelectorCheck(r.rax == f.object && r.rdx == 0x9004,
                          "Persistent provider value-state");
  }
  {
    Monstey_SelectorFixture f;
    f.services.compare_nonnull =
        [&f](Monstey_GadbBinderMemory,
             const Monstey_GadbBankSelectorCompareCall &) {
          f.services.compare_nonnull =
              [](Monstey_GadbBinderMemory,
                 const Monstey_GadbBankSelectorCompareCall &) {
                return Monstey_GadbBufferedResult{0, 0x7171};
              };
          return Monstey_GadbBufferedResult{0, 0x8181};
        };
    const auto r = f.Run();
    Monstey_SelectorCheck(r.rax == f.object && r.rdx == 0x7171,
                          "Provider selfreplacement");
  }
}
void Monstey_SelectorNulls() {
  for (const auto right : std::array<std::uint64_t, 4>{
           1, 0x8000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL, 0x100000000ULL}) {
    Monstey_SelectorFixture f;
    f.services.compare_nonnull = {};
    for (unsigned i = 0; i < 6; ++i)
      f.Write(f.table + i * 24, 0, 8);
    const auto r = f.Run(right, f.class_name, f.q, 0);
    Monstey_SelectorCheck(r.rax == 0 && r.rdx == 0ULL - right,
                          "Nullleft negativeRDX");
  }
  {
    Monstey_SelectorFixture f;
    f.services.compare_nonnull = {};
    const auto r = f.Run(0, f.class_name, 0, 0);
    Monstey_SelectorCheck(r.rax == 0 && r.rdx == 0, "Nullright noIAT");
  }
  for (unsigned mode = 0; mode < 3; ++mode) {
    Monstey_SelectorFixture f;
    f.services.compare_nonnull = {};
    for (unsigned i = 0; i < 6; ++i)
      f.Write(f.table + i * 24, 0x1111, 8);
    f.Write(f.table, 0, 8);
    f.Write(f.table + 8, f.recipes[4].enumerator, 8);
    f.Write(f.object + 0x20, mode == 2 ? 0x7777 : 0, 8);
    const auto r = f.Run(0, mode == 1 ? 0x9999 : 0, f.q, 0);
    Monstey_SelectorCheck(r.rax == (mode == 0 ? f.object : 0),
                          "Nullclass return");
    Monstey_SelectorCheck(r.rdx == (mode == 1 ? 0ULL - 0x9999 : 0),
                          "Nullclass rawRDX");
  }
  {
    Monstey_SelectorFixture f;
    f.services.compare_nonnull =
        [&f](Monstey_GadbBinderMemory,
             const Monstey_GadbBankSelectorCompareCall &c) {
          f.calls.push_back(c);
          return Monstey_GadbBufferedResult{1, 0x8484};
        };
    const auto r = f.Run(f.bank_name);
    Monstey_SelectorCheck(f.calls.size() == 6 && r.rdx == 0x8484,
                          "Equal nonnull pointer bypassed provider");
  }
}
void Monstey_SelectorFaultOrder() {
  for (unsigned mode = 0; mode < 5; ++mode) {
    Monstey_SelectorFixture f;
    f.services.compare_nonnull = {};
    try {
      (void)f.Run(f.bank_name, f.class_name, f.q, mode == 0 ? 0 : f.cell);
      throw std::logic_error("Missing import accepted");
    } catch (const Monstey_GadbBankSelectorMemoryFault &e) {
      ++Monstey_SelectorFaults;
      Monstey_SelectorCheck(mode == 0 && e.address == 0 && e.width == 8 &&
                                !e.writing,
                            "IAT read priority");
    } catch (const Monstey_GadbBankSelectorImportFault &e) {
      ++Monstey_SelectorFaults;
      Monstey_SelectorCheck(mode != 0 && e.slot == f.cell &&
                                e.target == 0x1111222233334444ULL &&
                                e.native_site == 0x1404BA9A2,
                            "Import identity");
    }
    Monstey_SelectorCheck(f.Read(f.q + 0x68, 4) == 0xA5A5A5A5,
                          "Early import touchedframe");
  }
  {
    Monstey_SelectorFixture f;
    f.services.compare_nonnull =
        [](Monstey_GadbBinderMemory,
           const Monstey_GadbBankSelectorCompareCall &) {
          return Monstey_GadbBufferedResult{0, 99};
        };
    try {
      (void)f.Run(f.bank_name, f.class_name, f.base + f.bytes.size() - 0x68);
      throw std::logic_error("Countfault missing");
    } catch (const Monstey_GadbBankSelectorMemoryFault &e) {
      ++Monstey_SelectorFaults;
      Monstey_SelectorCheck(e.width == 4 && !e.writing, "Count AND readfirst");
    }
  }
  {
    Monstey_SelectorFixture f;
    f.Write(f.table + 8, 0x12345678, 8);
    try {
      (void)f.Run(f.bank_name);
      throw std::logic_error("Unknownenum accepted");
    } catch (const Monstey_GadbBankFault &e) {
      ++Monstey_SelectorFaults;
      Monstey_SelectorCheck(
          e.kind == Monstey_GadbBankFaultKind::UnknownEnumerator, "Enum gate");
    }
    Monstey_SelectorCheck(f.Read(f.q + 0x68, 4) == 0 &&
                              f.Read(f.q + 0x20, 8) == 0,
                          "Enum gated beforeclears");
  }
  {
    Monstey_SelectorFixture f;
    // Outputarray aliases the enumerator slot; its second AND0 must change the
    // subsequently selected target, not a target cached before the clears.
    const auto frame = f.table + 8 - 0x20;
    try {
      (void)f.Run(f.bank_name, f.class_name, frame);
      throw std::logic_error("Aliased target cached");
    } catch (const Monstey_GadbBankFault &e) {
      ++Monstey_SelectorFaults;
      Monstey_SelectorCheck(e.identity == 0,
                            "Cleared enum target not consumed");
    }
    Monstey_SelectorCheck(f.Read(frame + 0x68, 4) == 0 &&
                              f.Read(f.table + 8, 8) == 0,
                          "Alias prefix");
  }
  {
    Monstey_SelectorFixture f;
    f.g4.Store(0);
    f.epoch = std::numeric_limits<std::int32_t>::min();
    f.recipes[4].constructor_classes = {};
    try {
      (void)f.Run();
      throw std::logic_error("Bad recipe accepted");
    } catch (const Monstey_GadbBankFault &e) {
      ++Monstey_SelectorFaults;
      Monstey_SelectorCheck(e.kind == Monstey_GadbBankFaultKind::InvalidRecipe,
                            "Ctor failure kind");
    }
    Monstey_SelectorCheck(f.g4.Load() == -1 && f.Read(f.q + 0x68, 4) == 0 &&
                              f.Read(f.q + 0x20, 8) == 0,
                          "Ctor partial claim/outputs");
  }
}
void Monstey_SelectorOverlays() {
  for (unsigned mode = 0; mode < 4; ++mode) {
    Monstey_SelectorFixture f;
    auto frame = f.global.identity - 0x68;
    if (mode == 1)
      frame = f.g4.identity - 0x20;
    if (mode == 2)
      f.activation.bank_table = f.global.identity;
    if (mode == 3) {
      f.activation.bank_table = f.global.identity;
      f.activation.memory.bytes = {};
    }
    const auto before = f.bytes;
    try {
      (void)f.Run(f.bank_name + 4, f.class_name, frame);
      throw std::logic_error("Overlay raw access allowed");
    } catch (const Monstey_GadbBankSelectorOverlayFault &e) {
      ++Monstey_SelectorFaults;
      Monstey_SelectorCheck(
          e.overlay_identity ==
                  (mode == 1 ? f.g4.identity : f.global.identity) &&
              !e.writing,
          "Overlay faultpriority");
    }
    if (mode == 1)
      Monstey_SelectorCheck(f.Read(frame + 0x68, 4) == 0,
                            "Arrayoverlay lostcountclear");
    else
      Monstey_SelectorCheck(before == f.bytes, "Overlay priorwrites");
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::runtime_error("Usage NEW_REPORT NONCE");
    Monstey_SelectorProfiles();
    Monstey_SelectorRawResults();
    Monstey_SelectorReloads();
    Monstey_SelectorNulls();
    Monstey_SelectorFaultOrder();
    Monstey_SelectorOverlays();
    std::ofstream out(argv[1], std::ios::binary);
    if (!out)
      throw std::runtime_error("Reportopen");
    out << "{\"status\":\"PASS\",\"groups\":6,\"cases\":"
        << Monstey_SelectorCases << ",\"checks\":" << Monstey_SelectorChecks
        << ",\"faults\":" << Monstey_SelectorFaults << ",\"nonce\":\""
        << argv[2] << "\"}\n";
    std::cout << "PASS groups6 cases" << Monstey_SelectorCases << " checks"
              << Monstey_SelectorChecks << " faults" << Monstey_SelectorFaults
              << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
