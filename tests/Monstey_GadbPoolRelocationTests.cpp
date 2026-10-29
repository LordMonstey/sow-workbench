#include "Monstey_GadbPoolRelocation.hpp"
#include "Monstey_GadbValueFixup.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
namespace {
using Monstey_TestAddress = std::uint64_t;
std::uint64_t Monstey_checks = 0, Monstey_cases = 0, Monstey_faults = 0;
std::vector<std::string> Monstey_groups;
void Monstey_Check(bool yes, const char *message) {
  ++Monstey_checks;
  if (!yes)
    throw std::runtime_error(message);
}
void Monstey_Put(std::vector<std::uint8_t> &bytes, std::uint64_t base,
                 std::uint64_t at, std::uint64_t value, std::size_t width) {
  if (at < base || at - base > bytes.size() ||
      width > bytes.size() - static_cast<std::size_t>(at - base))
    throw std::runtime_error("test storage write outside fixture");
  for (std::size_t n = 0; n < width; ++n)
    bytes[static_cast<std::size_t>(at - base) + n] =
        static_cast<std::uint8_t>(value >> (n * 8u));
}
std::uint64_t Monstey_Get(const Monstey::Monstey_GadbBinderMemory &m,
                          std::uint64_t at, std::size_t width) {
  if (at < m.base || at - m.base > m.bytes.size() ||
      width > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw std::runtime_error("test storage read outside fixture");
  std::uint64_t value = 0;
  for (std::size_t n = 0; n < width; ++n)
    value += std::uint64_t(m.bytes[static_cast<std::size_t>(at - m.base) + n])
             << (8u * n);
  return value;
}
void Monstey_Put(Monstey::Monstey_GadbBinderMemory &m, std::uint64_t at,
                 std::uint64_t value, std::size_t width) {
  if (at < m.base || at - m.base > m.bytes.size() ||
      width > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw std::runtime_error("test service write outside fixture");
  for (std::size_t n = 0; n < width; ++n)
    m.bytes[static_cast<std::size_t>(at - m.base) + n] =
        static_cast<std::uint8_t>(value >> (8u * n));
}
struct Monstey_Step {
  std::uint32_t width = 0, copied = 0;
  std::uint64_t value = 0, result = 0;
};
struct Monstey_Fixture {
  std::uint64_t base = 0x10000000000ULL;
  std::vector<std::uint8_t> bytes;
  Monstey::Monstey_GadbBinderMemory memory;
  std::uint64_t frame, stream, table, record, pool, descriptors, words, cursor;
  std::vector<Monstey_Step> steps;
  std::vector<Monstey::Monstey_GadbPoolReadCall> calls;
  std::function<void(Monstey_Fixture &, std::size_t,
                     const Monstey::Monstey_GadbPoolReadCall &)>
      hook;
  Monstey_Fixture(std::size_t size = 0x10000)
      : bytes(size, 0xCC), memory{base, bytes}, frame(base + 0x800),
        stream(base + 0x100), table(base + 0x200), record(base + 0x300),
        pool(base + 0x1000), descriptors(base + 0x2000), words(base + 0x4000),
        cursor(base + 0x500) {
    Put(stream, table, 8);
    Put(table + 8, 0xABCDEF0123456789ULL, 8);
    Put(frame + 0x50, pool, 8);
    Put(frame + 0x58, descriptors, 8);
    Put(frame + 0x68, cursor, 8);
    Put(cursor, words, 8);
    Put(pool + 0x80, 0x4241, 3);
  }
  Monstey_Fixture(const Monstey_Fixture &) = delete;
  void Put(std::uint64_t at, std::uint64_t value, std::size_t width) {
    Monstey_Put(memory, at, value, width);
  }
  std::uint64_t Get(std::uint64_t at, std::size_t width) const {
    return Monstey_Get(memory, at, width);
  }
  void Script(std::uint16_t count, std::uint8_t descriptor_count,
              std::uint8_t flags = 0xC4, std::uint32_t first = 0,
              std::uint32_t name = 0x80) {
    steps = {{4, 4, name, 0},
             {2, 2, count, 0},
             {1, 1, descriptor_count, 0},
             {4, 4, first, 0},
             {1, 1, flags, 0}};
    for (std::uint32_t n = 0; n < count; ++n)
      steps.push_back({4, 4, 0x10203040u + n, 0});
  }
  Monstey::Monstey_GadbPoolReadService Service() {
    return [this, index = std::size_t{0}](
               Monstey::Monstey_GadbBinderMemory &m,
               const Monstey::Monstey_GadbPoolReadCall &call) mutable
               -> std::uint64_t {
      Monstey_Check(index == calls.size(),
                    "provider mutable captured state reset");
      Monstey_Check(index < steps.size(), "unexpected primitive read");
      const auto step = steps[index];
      Monstey_Check(call.byte_count == step.width, "primitive read width");
      calls.push_back(call);
      for (std::uint32_t n = 0; n < step.copied; ++n)
        Monstey_Put(m, call.destination + n, step.value >> (n * 8u), 1);
      if (hook)
        hook(*this, index, call);
      ++index;
      return step.result;
    };
  }
  std::uint64_t Run(const Monstey::Monstey_GadbPoolReadService &service) {
    return Monstey::Monstey_GadbLoadPoolRelativeRecord(
        memory, 0x123456789ABCDEF0ULL, 0x8877665544332211ULL, record, stream,
        frame, service);
  }
  std::uint64_t Run() {
    const auto service = Service();
    return Run(service);
  }
  void Descriptor(std::uint32_t index, std::uint8_t type, std::uint16_t count,
                  std::uint16_t first) {
    const auto at = descriptors + std::uint64_t(index) * 16;
    Put(at + 10, type, 1);
    Put(at + 12, count, 2);
    Put(at + 14, first, 2);
  }
};
void Monstey_ExpectedBasic(Monstey_Fixture &f, std::vector<std::uint8_t> &out,
                           std::uint16_t count, std::uint8_t desc,
                           std::uint8_t flags) {
  const auto put = [&](std::uint64_t at, std::uint64_t v, std::size_t n) {
    Monstey_Put(out, f.base, at, v, n);
  };
  put(f.frame + 0x38, 0x8877665544332211ULL, 8);
  put(f.frame - 12, 0x80, 4);
  put(f.frame - 16, count, 2);
  put(f.frame + 0x38, desc, 1);
  put(f.frame - 8, 0, 4);
  put(f.frame + 0x60, flags, 1);
  put(f.cursor, f.words + std::uint64_t(count) * 8, 8);
  for (std::uint32_t n = 0; n < count; ++n)
    put(f.words + std::uint64_t(n) * 8, 0x10203040u + n, 8);
  for (std::uint32_t n = 0; n < desc; ++n) {
    const auto at = f.descriptors + std::uint64_t(n) * 16;
    const auto type = f.Get(at + 10, 1) & 63;
    if ((type >= 4 && type <= 9) || (type >= 12 && type <= 16))
      for (std::uint32_t k = 0; k < f.Get(at + 12, 2); ++k) {
        const auto slot = f.words + (f.Get(at + 14, 2) + k) * 8;
        const Monstey::Monstey_GadbBinderMemory view{f.base, out};
        put(slot, Monstey_Get(view, slot, 8) + f.pool, 8);
      }
  }
  put(f.record + 8, f.descriptors, 8);
  put(f.record + 32, f.pool + 0x80, 8);
  put(f.record, desc, 1);
  put(f.record + 24, 0x123456789ABCDEF0ULL, 8);
  put(f.record + 16, f.words, 8);
  put(f.record + 4, 921, 4);
  put(f.record + 1, flags | 2u, 1);
}
void Monstey_Basic() {
  for (unsigned type = 0; type < 256; ++type) {
    Monstey_Fixture f;
    f.Script(2, 1);
    f.Descriptor(0, static_cast<std::uint8_t>(type), 2, 0);
    auto expected = f.bytes;
    Monstey_ExpectedBasic(f, expected, 2, 1, 0xC4);
    const auto result = f.Run();
    Monstey_Check(result == 769, "raw successful hash RAX");
    Monstey_Check(f.bytes == expected, "complete basic arena mismatch");
    Monstey_Check(f.calls.size() == 7, "complete call count");
    const std::array<std::uint64_t, 7> sites = {
        0x14115FD5E, 0x14115FD3E, 0x14115FD1E, 0x14115FD5E,
        0x14115FD1E, 0x1404BA297, 0x1404BA297};
    for (std::size_t k = 0; k < sites.size(); ++k) {
      Monstey_Check(f.calls[k].native_site == sites[k],
                    "native read call site");
      Monstey_Check(f.calls[k].target == 0xABCDEF0123456789ULL,
                    "native target identity");
    }
    ++Monstey_cases;
  }
  Monstey_groups.push_back("all256 types flags and full raw arenas");
}
void Monstey_Leaf() {
  for (std::uint32_t n = 0; n < 65536; ++n) {
    const bool yes = (n >= 4 && n <= 9) || (n >= 12 && n <= 16);
    const auto incoming = 0xFEEDEEDDBBCCAAFEULL + n * 256ULL;
    Monstey_Check(Monstey::Monstey_GadbIsPoolRelativeType(n) ==
                      static_cast<std::uint8_t>(yes),
                  "unmasked signed leaf");
    Monstey_Check(Monstey::Monstey_GadbPoolRelativeTypeRaw(n, incoming) ==
                      ((incoming & ~255ULL) | std::uint64_t(yes)),
                  "leaf upper RAX preservation");
    ++Monstey_cases;
  }
  for (auto n : {0x80000000u, 0xFFFFFFFFu, 0xFFFFFFFCu, 0x7FFFFFFFu}) {
    Monstey_Check(Monstey::Monstey_GadbIsPoolRelativeType(n) == 0,
                  "leaf signed/high input");
    ++Monstey_cases;
  }
  Monstey_groups.push_back("raw signedECX leaf and AL only result");
}
void Monstey_HeaderStatuses() {
  Monstey_Fixture f;
  f.Script(1, 1);
  f.Descriptor(0, 4, 1, 0);
  for (std::size_t k = 0; k < 5; ++k)
    f.steps[k].result = 0xBBCC00001122ULL;
  auto expected = f.bytes;
  Monstey_ExpectedBasic(f, expected, 1, 1, 0xC4);
  Monstey_Check(f.Run() == 769 && f.bytes == expected,
                "header statuses must be ignored");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Script(0, 0);
  g.Put(g.frame - 12, 0x80, 4);
  g.Put(g.frame - 16, 0, 2);
  g.Put(g.frame - 8, 0, 4);
  g.Put(g.frame + 0x60, 0x40, 1);
  for (auto &s : g.steps) {
    s.copied = 0;
    s.result = 1;
  }
  g.steps[2].copied = 1;
  g.steps[2].value = 0;
  Monstey_Check(g.Run() == 769, "partial headers consumed original bytes");
  Monstey_Check(g.Get(g.record + 1, 1) == 0x42,
                "unwritten flags replaced by zero");
  Monstey_Check(g.Get(g.frame + 0x38, 8) == 0x8877665544332200ULL,
                "original RDX home before header overwrites");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "header ignored statuses partial read and retained private bytes");
}
void Monstey_PayloadFailure() {
  for (unsigned failed = 0; failed < 4; ++failed)
    for (unsigned copied = 0; copied <= 4; ++copied) {
      Monstey_Fixture f;
      f.Script(4, 0);
      const auto before_record = std::vector<std::uint8_t>(
          f.bytes.begin() + 0x300, f.bytes.begin() + 0x328);
      f.steps[5 + failed].copied = copied;
      f.steps[5 + failed].result = 0xABCDEF1200000155ULL;
      f.hook = [](Monstey_Fixture &x, std::size_t k, const auto &call) {
        if (k >= 5) {
          Monstey_Check(x.Get(x.cursor, 8) == x.words + 32,
                        "cursor must advance before payload");
          Monstey_Check(x.Get(call.destination + 4, 4) == 0,
                        "zero upper qword before read");
        }
      };
      const auto result = f.Run();
      Monstey_Check(result == 0xABCDEF1200000100ULL,
                    "failed raw RAX must only clear AL");
      Monstey_Check(f.calls.size() == failed + 6,
                    "failure must stop immediately");
      Monstey_Check(std::equal(before_record.begin(), before_record.end(),
                               f.bytes.begin() + 0x300),
                    "failure wrote output record");
      Monstey_Check(f.Get(f.words + failed * 8 + 4, 4) == 0,
                    "failed slot high bytes");
      ++Monstey_cases;
    }
  Monstey_Fixture f;
  f.Script(1, 0);
  f.steps[5].result = 0xDDEEFF0000000000ULL;
  Monstey_Check(f.Run() == 769, "payload status tests only EAX");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "payload prefix zero high32 cursor and raw EAX status");
}
void Monstey_LivePayload() {
  for (unsigned stop = 0; stop < 4; ++stop) {
    Monstey_Fixture f;
    f.Script(4, 0);
    f.hook = [stop](Monstey_Fixture &x, std::size_t k, const auto &) {
      if (k == 5 + stop)
        x.Put(x.frame - 16, stop, 2);
    };
    Monstey_Check(f.Run() == 769, "live shortened payload result");
    Monstey_Check(f.calls.size() == stop + 6, "live payload count reload");
    Monstey_Check(f.Get(f.cursor, 8) == f.words + 32,
                  "advance uses cached initial count");
    ++Monstey_cases;
  }
  Monstey_Fixture f;
  f.Script(1, 0);
  f.steps.push_back({4, 4, 0xABCDEF12, 0});
  f.hook = [](Monstey_Fixture &x, std::size_t k, const auto &) {
    if (k == 5)
      x.Put(x.frame - 16, 2, 2);
  };
  Monstey_Check(f.Run() == 769 && f.calls.size() == 7,
                "live enlarged payload count");
  Monstey_Check(f.Get(f.cursor, 8) == f.words + 8,
                "cached advance independent of later count");
  ++Monstey_cases;
  Monstey_groups.push_back("live payload counts and cached cursor sizing");
}
void Monstey_TargetReload() {
  Monstey_Fixture f;
  f.Script(2, 0);
  f.hook = [](Monstey_Fixture &x, std::size_t k, const auto &call) {
    Monstey_Check(call.target == 0xABCDEF0123456789ULL + k, "target reload");
    x.Put(x.table + 8, call.target + 1, 8);
  };
  Monstey_Check(f.Run() == 769, "reload result");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Script(0, 0);
  const Monstey::Monstey_GadbPoolReadService absent;
  g.Put(g.stream, g.base + g.bytes.size(), 8);
  try {
    (void)g.Run(absent);
    throw std::runtime_error("missing consumed vtable fault");
  } catch (const Monstey::Monstey_GadbPoolRelocationFault &e) {
    Monstey_Check(e.address == g.base + g.bytes.size() + 8 && !e.writing,
                  "vtable fault before missing-service check");
    ++Monstey_faults;
  }
  g.Put(g.stream, g.table, 8);
  try {
    (void)g.Run(absent);
    throw std::runtime_error("missing service fault");
  } catch (const Monstey::Monstey_GadbPoolReadBoundaryFault &e) {
    Monstey_Check(e.target == 0xABCDEF0123456789ULL &&
                      e.native_site == 0x14115FD5E,
                  "boundary sampled after target read");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_groups.push_back(
      "live vtable targets and consumed first fault order");
}
void Monstey_FaultPrefixes() {
  Monstey_Fixture f;
  f.Script(0, 0);
  f.steps[0].value = static_cast<std::uint32_t>(f.bytes.size());
  try {
    (void)f.Run();
    throw std::runtime_error("hash range should fault");
  } catch (const Monstey::Monstey_GadbValueFixupFault &e) {
    Monstey_Check(e.address == f.pool + f.bytes.size(),
                  "hash consumed address");
    Monstey_Check(f.Get(f.record + 8, 8) == f.descriptors &&
                      f.Get(f.record + 32, 8) == e.address &&
                      f.Get(f.record + 24, 8) == 0x123456789ABCDEF0ULL &&
                      f.Get(f.record + 16, 8) == f.words,
                  "all preceding record fields before hash");
    Monstey_Check(f.Get(f.record + 4, 4) == 0xCCCCCCCC &&
                      f.Get(f.record + 1, 1) == 0xCC,
                  "no hash/flags writes on hash fault");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Script(1, 0);
  g.Put(g.cursor, g.base + g.bytes.size() - 4, 8);
  try {
    (void)g.Run();
    throw std::runtime_error("payload zero should fault before read");
  } catch (const Monstey::Monstey_GadbPoolRelocationFault &e) {
    Monstey_Check(e.width == 8 && e.writing, "payload zero width");
    Monstey_Check(g.calls.size() == 5, "no payload service before zero fault");
    Monstey_Check(g.Get(g.cursor, 8) == g.base + g.bytes.size() + 4,
                  "cursor prefix retained");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture h;
  h.Script(0, 1, 0, 0xFFFFFFFFu);
  try {
    (void)h.Run();
    throw std::runtime_error("scaled descriptor should fault");
  } catch (const Monstey::Monstey_GadbPoolRelocationFault &e) {
    Monstey_Check(e.address == h.descriptors + 0xFFFFFFFF0ULL + 10,
                  "descriptor index scaled after widening to64");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_groups.push_back(
      "hash record prefix payload zero fault and scaled64 descriptors");
}
void Monstey_TypedWrappers() {
  for (auto width : {1u, 2u, 4u}) {
    Monstey_Fixture f;
    f.steps = {{width, width, 0x44332211, 0xFFFFFFFFFFFFFFFFULL}};
    const auto service = f.Service();
    std::uint64_t result = 0;
    if (width == 1)
      result =
          Monstey::Monstey_GadbPoolReadU8(f.memory, f.stream, f.words, service);
    else if (width == 2)
      result = Monstey::Monstey_GadbPoolReadU16(f.memory, f.stream, f.words,
                                                service);
    else
      result = Monstey::Monstey_GadbPoolReadU32(f.memory, f.stream, f.words,
                                                service);
    Monstey_Check(result == f.stream,
                  "typed wrapper returns stream not read status");
    Monstey_Check(f.Get(f.words + width, 1) == 0xCC,
                  "typed read has no qword prezero");
    ++Monstey_cases;
  }
  Monstey_groups.push_back("complete typed primitive wrappers");
}
void Monstey_OwnedReader() {
  Monstey_Fixture f;
  f.Put(f.pool, 0x44332211, 4);
  Monstey::Monstey_GadbPoolOwnedMemoryRead profile{f.pool, 3, 0,
                                                   0x1234567800000001ULL};
  Monstey::Monstey_GadbPoolReadCall call{f.stream, 0, f.words, 4, 0};
  Monstey_Check(profile.Monstey_Read(f.memory, call) == 0x1234567800000001ULL,
                "owned short raw status");
  Monstey_Check(profile.position == 3 && f.Get(f.words, 4) == 0xCC332211,
                "owned partial prefix and position");
  ++Monstey_cases;
  profile = {f.base + f.bytes.size() - 1, 2, 0, 1};
  f.Put(profile.source, 0xAB, 1);
  try {
    (void)profile.Monstey_Read(f.memory, call);
    throw std::runtime_error("owned source range fault");
  } catch (const Monstey::Monstey_GadbPoolRelocationFault &e) {
    Monstey_Check(!e.writing && profile.position == 1 &&
                      f.Get(f.words, 1) == 0xAB,
                  "owned source prefix before fault");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  profile = {f.pool, 4, 0, 1};
  call.destination = f.base + f.bytes.size() - 1;
  try {
    (void)profile.Monstey_Read(f.memory, call);
    throw std::runtime_error("owned destination range fault");
  } catch (const Monstey::Monstey_GadbPoolRelocationFault &e) {
    Monstey_Check(e.writing && profile.position == 1,
                  "owned dest position prefix");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_groups.push_back(
      "explicit supplied memory reader partial and fault epochs");
}
void Monstey_ProviderReplacement() {
  Monstey_Fixture f;
  f.Script(0, 0);
  auto original = f.Service();
  Monstey::Monstey_GadbPoolReadService service;
  unsigned old_calls = 0, new_calls = 0;
  service = [&](auto &m, const auto &call) {
    ++old_calls;
    const auto result = original(m, call);
    service = [&](auto &mm, const auto &cc) {
      ++new_calls;
      return original(mm, cc);
    };
    return result;
  };
  Monstey_Check(f.Run(service) == 769 && old_calls == 1 && new_calls == 4,
                "read provider replacement target epochs");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Script(0, 0);
  auto other = g.bytes;
  const auto initial_service = g.Service();
  g.hook = [&](Monstey_Fixture &x, std::size_t k, const auto &) {
    if (k == 0) {
      other = x.bytes;
      x.memory = {x.base, other};
    }
  };
  const auto before = g.bytes;
  Monstey_Check(g.Run(initial_service) == 769,
                "memory view replacement result");
  Monstey_Check(Monstey_Get(g.memory, g.record + 4, 4) == 921 &&
                    g.bytes[0x304] == before[0x304],
                "subsequent writes use replacement memory view");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "stateful provider replacement and outer view rebinding");
}
void Monstey_AliasCases() {
  // pool zero lets a repeated descriptor point at count/header bytes without
  // extra semantic hashing. known home write may alias stream identity and must
  // precede the first vtable read.
  Monstey_Fixture g;
  g.Script(0, 0);
  g.frame = g.stream - 0x38;
  try {
    (void)g.Run();
    throw std::runtime_error("home alias must corrupt stream before read");
  } catch (const Monstey::Monstey_GadbPoolRelocationFault &e) {
    Monstey_Check(e.address == 0x8877665544332219ULL,
                  "home store before stream lookup");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  // record+4 aliases flags byte. Flags load must precede the hash store.
  Monstey_Fixture h;
  h.Script(0, 0, 0xA4);
  h.record = h.frame + 0x60 - 4;
  Monstey_Check(h.Run() == 769, "flags alias result");
  Monstey_Check(h.Get(h.record + 1, 1) == 0xA6,
                "flags sampled before hash write");
  ++Monstey_cases;
  // Payload aliases frame count. Read writes low32 zero; live reload stops
  // after first payload despite cached advance3.
  Monstey_Fixture j;
  j.Script(3, 0);
  j.Put(j.cursor, j.frame - 16, 8);
  j.steps[5].value = 0;
  Monstey_Check(j.Run() == 769 && j.calls.size() == 6,
                "payload aliases live header count");
  Monstey_Check(j.Get(j.cursor, 8) == j.frame - 16 + 24,
                "cached cursor advance with payload alias");
  ++Monstey_cases;
  Monstey_groups.push_back("frame stream flags and payload aliases");
}
void Monstey_RelocationAliases() {
  Monstey_Fixture f(0x20000);
  f.Script(0, 1, 0xC4, 0, 0);
  f.pool = f.base + 0xFFFF;
  f.Put(f.pool, 0x4241, 3);
  f.Put(f.frame + 0x50, f.pool, 8);
  f.Put(f.cursor, f.descriptors + 12, 8);
  f.Descriptor(0, 4, 2, 0);
  const auto initial = f.Get(f.descriptors + 12, 8);
  const auto untouched = f.Get(f.descriptors + 20, 8);
  Monstey_Check(f.Run() == 769, "descriptor count alias result");
  Monstey_Check(f.Get(f.descriptors + 12, 8) == initial + f.pool &&
                    f.Get(f.descriptors + 20, 8) == untouched,
                "descriptor count reload stops after one live relocation");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Script(0, 1);
  g.pool += 3;
  g.Put(g.pool + 0x80, 0x4241, 3);
  g.Put(g.frame + 0x50, g.pool, 8);
  g.Put(g.cursor, g.frame + 0x38, 8);
  for (unsigned n = 0; n < 4; ++n)
    g.Descriptor(n, n == 0 ? 4 : 3, 1, 0);
  Monstey_Check(
      g.Run() == 769 && g.Get(g.record, 1) == 4,
      "relocation updates cached descriptor count after entered loop");
  ++Monstey_cases;
  Monstey_Fixture h;
  h.Script(0, 1);
  h.Descriptor(0, 4, 1, 0);
  h.Put(h.words, std::numeric_limits<std::uint64_t>::max() - h.pool + 7, 8);
  Monstey_Check(h.Run() == 769 && h.Get(h.words, 8) == 6,
                "full64 wrapping pool addition");
  ++Monstey_cases;
  Monstey_Fixture j;
  j.Script(1, 3);
  j.Descriptor(0, 5, 1, 0);
  j.Descriptor(1, 3, 0, 0);
  j.Descriptor(2, 14, 1, 0);
  Monstey_Check(
      j.Run() == 769 && j.Get(j.words, 8) == 0x10203040ULL + j.pool * 2,
      "ordered in-place repeated relocation without premature semanticfixup");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "live descriptor counts aliases repeated and wrapping relocation");
}
void Monstey_CountExtents() {
  Monstey_Fixture f(0x100000);
  f.Script(65535, 0);
  Monstey_Check(f.Run() == 769 && f.calls.size() == 65540,
                "unsigned16 payload extent");
  Monstey_Check(f.Get(f.words + 65534ULL * 8, 8) == 0x1021303EULL,
                "last unsigned16 payload");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Script(0, 255);
  for (unsigned n = 0; n < 255; ++n)
    g.Descriptor(n, 3, 0, 0);
  Monstey_Check(g.Run() == 769 && g.Get(g.record, 1) == 255,
                "unsigned8 descriptor extent");
  ++Monstey_cases;
  Monstey_groups.push_back("maximum unsigned serialized count extents");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::runtime_error("Usage REPORT NONCE");
    Monstey_Leaf();
    Monstey_Basic();
    Monstey_HeaderStatuses();
    Monstey_PayloadFailure();
    Monstey_LivePayload();
    Monstey_TargetReload();
    Monstey_FaultPrefixes();
    Monstey_TypedWrappers();
    Monstey_OwnedReader();
    Monstey_ProviderReplacement();
    Monstey_AliasCases();
    Monstey_RelocationAliases();
    Monstey_CountExtents();
    std::ofstream out(argv[1], std::ios::binary);
    if (!out)
      throw std::runtime_error("report unavailable");
    out << "{\n  \"status\":\"PASS\",\n  \"nonce\":\"" << argv[2]
        << "\",\n  \"groups\":" << Monstey_groups.size()
        << ",\n  \"cases\":" << Monstey_cases
        << ",\n  \"checks\":" << Monstey_checks
        << ",\n  \"owned_faults\":" << Monstey_faults
        << ",\n  \"original_code_executed\":false,\n  "
           "\"synthetic_only\":true,\n  \"group_names\":[";
    for (std::size_t k = 0; k < Monstey_groups.size(); ++k) {
      if (k)
        out << ',';
      out << '"' << Monstey_groups[k] << '"';
    }
    out << "]\n}\n";
    if (!out)
      throw std::runtime_error("report write failed");
    std::cout << "PASS " << Monstey_groups.size() << " groups " << Monstey_cases
              << " cases " << Monstey_checks << " checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
