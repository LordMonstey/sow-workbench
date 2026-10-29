#include "Monstey_GadbAnonymousRecord.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
namespace {
std::uint64_t Monstey_checks = 0, Monstey_cases = 0, Monstey_faults = 0;
std::vector<std::string> Monstey_groups;
void Monstey_Check(bool ok, const char *why) {
  ++Monstey_checks;
  if (!ok)
    throw std::runtime_error(why);
}
void Monstey_Put(Monstey::Monstey_GadbBinderMemory &m, std::uint64_t at,
                 std::uint64_t value, std::size_t n) {
  if (at < m.base || at - m.base > m.bytes.size() ||
      n > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw std::runtime_error("bad test write");
  for (std::size_t k = 0; k < n; ++k)
    m.bytes[static_cast<std::size_t>(at - m.base) + k] =
        static_cast<std::uint8_t>(value >> (k * 8u));
}
std::uint64_t Monstey_Get(const Monstey::Monstey_GadbBinderMemory &m,
                          std::uint64_t at, std::size_t n) {
  if (at < m.base || at - m.base > m.bytes.size() ||
      n > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw std::runtime_error("bad test read");
  std::uint64_t v = 0;
  for (std::size_t k = 0; k < n; ++k)
    v |= std::uint64_t(m.bytes[static_cast<std::size_t>(at - m.base) + k])
         << (k * 8u);
  return v;
}
struct Monstey_Step {
  std::uint32_t width, copied;
  std::uint64_t value, status;
};
struct Monstey_Fixture {
  std::uint64_t base = 0x10000000000ULL;
  std::vector<std::uint8_t> bytes;
  Monstey::Monstey_GadbBinderMemory m;
  std::uint64_t frame, stream, table, record, pool, descs, words, cursor;
  std::vector<Monstey_Step> steps;
  std::vector<Monstey::Monstey_GadbPoolReadCall> events;
  std::function<void(Monstey_Fixture &, std::size_t,
                     const Monstey::Monstey_GadbPoolReadCall &)>
      hook;
  Monstey_Fixture()
      : bytes(0x100000, 0xCD), m{base, bytes}, frame(base + 0x800),
        stream(base + 0x100), table(base + 0x200), record(base + 0x300),
        pool(base + 0x1000), descs(base + 0x2000), words(base + 0x4000),
        cursor(base + 0x500) {
    Put(stream, table, 8);
    Put(table + 8, 0xFEDCBA0987654321ULL, 8);
    Put(frame + 0x70, pool, 8);
    Put(frame + 0x78, descs, 8);
    Put(frame + 0x88, cursor, 8);
    Put(cursor, words, 8);
  }
  void Put(std::uint64_t at, std::uint64_t v, std::size_t n) {
    Monstey_Put(m, at, v, n);
  }
  std::uint64_t Get(std::uint64_t at, std::size_t n) const {
    return Monstey_Get(m, at, n);
  }
  void Script(std::uint16_t values, std::uint8_t descriptors,
              std::uint8_t flags = 0xA0, std::uint32_t first = 0) {
    steps = {{2, 2, values, 0},
             {1, 1, descriptors, 0},
             {4, 4, first, 0},
             {1, 1, flags, 0}};
    for (std::uint32_t k = 0; k < values; ++k)
      steps.push_back({4, 4, 0x98760000u + k, 0});
  }
  void Descriptor(unsigned i, std::uint8_t type, std::uint16_t count,
                  std::uint16_t first) {
    const auto at = descs + std::uint64_t(i) * 16;
    Put(at + 10, type, 1);
    Put(at + 12, count, 2);
    Put(at + 14, first, 2);
  }
  Monstey::Monstey_GadbPoolReadService Service() {
    return [this, i = std::size_t(0)](
               auto &memory, const auto &call) mutable -> std::uint64_t {
      Monstey_Check(i == events.size(), "persistent provider state lost");
      Monstey_Check(i < steps.size(), "unexpected read");
      const auto s = steps[i];
      Monstey_Check(call.byte_count == s.width, "wrong read width");
      events.push_back(call);
      for (std::uint32_t k = 0; k < s.copied; ++k)
        Monstey_Put(memory, call.destination + k, s.value >> (k * 8u), 1);
      if (hook)
        hook(*this, i, call);
      ++i;
      return s.status;
    };
  }
  std::uint64_t Run(const Monstey::Monstey_GadbPoolReadService &service) {
    return Monstey::Monstey_GadbLoadAnonymousRecord(
        m, 0x123456789ABCDEF0ULL, 0x8877665544332211ULL, record, stream, frame,
        service);
  }
  std::uint64_t Run() {
    const auto service = Service();
    return Run(service);
  }
};
void Monstey_Basic() {
  for (unsigned raw = 0; raw < 256; ++raw) {
    Monstey_Fixture f;
    f.Script(2, 1, static_cast<std::uint8_t>(raw));
    f.Descriptor(0, static_cast<std::uint8_t>(raw), 2, 0);
    auto expected = f.bytes;
    Monstey::Monstey_GadbBinderMemory e{f.base, expected};
    Monstey_Put(e, f.frame + 0x58, 0x8877665544332211ULL, 8);
    Monstey_Put(e, f.frame + 0x20, 2, 2);
    Monstey_Put(e, f.frame + 0x58, 1, 1);
    Monstey_Put(e, f.frame + 0x24, 0, 4);
    Monstey_Put(e, f.frame + 0x80, raw, 1);
    Monstey_Put(e, f.cursor, f.words + 16, 8);
    const auto type = raw % 64;
    const bool relative =
        (type >= 4 && type <= 9) || (type >= 12 && type <= 16);
    for (unsigned k = 0; k < 2; ++k)
      Monstey_Put(e, f.words + k * 8,
                  0x98760000ULL + k + (relative ? f.pool : 0), 8);
    Monstey_Put(e, f.record + 1, raw, 1);
    Monstey_Put(e, f.record, 1, 1);
    Monstey_Put(e, f.record + 8, f.descs, 8);
    Monstey_Put(e, f.record + 24, 0x123456789ABCDEF0ULL, 8);
    Monstey_Put(e, f.record + 16, f.words, 8);
    Monstey_Check(f.Run() == 1, "raw simple success");
    Monstey_Check(f.bytes == expected,
                  "full raw arena or padding/flags mismatch");
    Monstey_Check(f.events.size() == 6, "anonymous headers count");
    const std::array<std::uint64_t, 6> sites = {0x14115FD3E, 0x14115FD1E,
                                                0x14115FD5E, 0x14115FD1E,
                                                0x14176662B, 0x14176662B};
    for (std::size_t k = 0; k < sites.size(); ++k)
      Monstey_Check(f.events[k].native_site == sites[k],
                    "wrong native read call site");
    ++Monstey_cases;
  }
  Monstey_groups.push_back(
      "all256 types flags complete arenas and preserved padding");
}
void Monstey_RawReturns() {
  for (const std::uint16_t count :
       {std::uint16_t(0), std::uint16_t(1), std::uint16_t(255),
        std::uint16_t(256), std::uint16_t(257), std::uint16_t(65535)}) {
    Monstey_Fixture f;
    f.Script(count, 0);
    f.Put(f.frame + 0x70, 0xFFFFFFFFFFFFFFFFULL, 8);
    Monstey_Check(f.Run() == ((std::uint64_t(count) & ~255ULL) | 1),
                  "no descriptor raw RAX or pool eager dereference");
    Monstey_Check(f.events.size() == std::size_t(count) + 4,
                  "maximum payload extent");
    Monstey_Check(f.Get(f.cursor, 8) == f.words + std::uint64_t(count) * 8,
                  "value cursor extent");
    ++Monstey_cases;
  }
  for (const std::uint16_t first :
       {std::uint16_t(0), std::uint16_t(255), std::uint16_t(256),
        std::uint16_t(0xA1B2), std::uint16_t(65535)}) {
    Monstey_Fixture f;
    f.Script(0, 2);
    f.Descriptor(0, 4, 0, first);
    f.Descriptor(1, 3, 65535, 65535);
    Monstey_Check(f.Run() == ((std::uint64_t(first) & ~255ULL) | 1),
                  "firstWord EAX then predicate AL retention");
    Monstey_Check(f.events.size() == 4,
                  "zero descriptor value count must not consume words");
    ++Monstey_cases;
  }
  Monstey_Fixture f;
  f.Script(0, 1);
  f.Descriptor(0, 14, 257, 0);
  for (unsigned k = 0; k < 257; ++k)
    f.Put(f.words + k * 8, 9, 8);
  Monstey_Check(f.Run() == 0x101, "last descriptor count retained above AL");
  Monstey_Check(f.Get(f.words + 256 * 8, 8) == f.pool + 9,
                "last relocated word");
  ++Monstey_cases;
  Monstey_groups.push_back("raw success EAX leaf AL and unsigned counts");
}
void Monstey_Failures() {
  for (unsigned stop = 0; stop < 3; ++stop)
    for (unsigned copied = 0; copied <= 4; ++copied) {
      Monstey_Fixture f;
      f.Script(3, 0);
      f.steps[4 + stop].copied = copied;
      f.steps[4 + stop].status = 0xABCD345600000155ULL;
      auto untouched = std::vector<std::uint8_t>(f.bytes.begin() + 0x300,
                                                 f.bytes.begin() + 0x320);
      f.hook = [](auto &x, std::size_t k, const auto &call) {
        if (k >= 4) {
          Monstey_Check(x.Get(x.cursor, 8) == x.words + 24,
                        "cursor not advanced before read");
          Monstey_Check(x.Get(call.destination + 4, 4) == 0,
                        "upper payload not zero before read");
        }
      };
      Monstey_Check(f.Run() == 0xABCD345600000100ULL, "raw failure RAX");
      Monstey_Check(f.events.size() == stop + 5, "read after payload failure");
      Monstey_Check(std::equal(untouched.begin(), untouched.end(),
                               f.bytes.begin() + 0x300),
                    "record written after failed payload");
      ++Monstey_cases;
    }
  Monstey_Fixture f;
  f.Script(1, 0);
  f.steps[4].status = 0xFFFFFFFF00000000ULL;
  Monstey_Check(f.Run() == 1, "payload must test EAX not RAX");
  ++Monstey_cases;
  Monstey_groups.push_back("partial payload status prefix and EAX only");
}
void Monstey_PartialHeaders() {
  Monstey_Fixture f;
  f.Script(0, 0, 0);
  f.Put(f.frame + 0x20, 0, 2);
  f.Put(f.frame + 0x24, 0, 4);
  f.Put(f.frame + 0x80, 0x81, 1);
  for (auto &s : f.steps) {
    s.status = 0xFFEEDDCC;
    s.copied = 0;
  }
  f.steps[1].copied = 1;
  f.steps[1].value = 0;
  Monstey_Check(f.Run() == 1, "header failure wrongly gated");
  Monstey_Check(f.Get(f.frame + 0x58, 8) == 0x8877665544332200ULL,
                "originalRDX home prefix");
  Monstey_Check(f.Get(f.record + 1, 1) == 0x81,
                "unwritten flag byte was initialized or ORed");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Script(0, 0);
  for (std::size_t k = 0; k < 4; ++k)
    g.steps[k].status = 0x12345678;
  Monstey_Check(g.Run() == 1, "header statuses not ignored");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "ignored partial header statuses and supplied private bytes");
}
void Monstey_LiveAliases() {
  Monstey_Fixture f;
  f.Script(3, 0);
  f.Put(f.cursor, f.frame + 0x20, 8);
  f.steps[4].value = 0;
  Monstey_Check(f.Run() == 1 && f.events.size() == 5,
                "payload count alias not reread");
  Monstey_Check(f.Get(f.cursor, 8) == f.frame + 0x20 + 24,
                "cached reservation lost");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Script(0, 1);
  g.pool = 3;
  g.Put(g.frame + 0x70, 3, 8);
  g.Put(g.cursor, g.frame + 0x58, 8);
  for (unsigned k = 0; k < 4; ++k)
    g.Descriptor(k, k == 0 ? 4 : 3, 1, 0);
  Monstey_Check(g.Run() == 1 && g.Get(g.record, 1) == 4,
                "outer count not refreshed after relocation");
  ++Monstey_cases;
  Monstey_Fixture h;
  h.Script(0, 1);
  h.pool = 65535;
  h.Put(h.frame + 0x70, h.pool, 8);
  h.Put(h.cursor, h.descs + 12, 8);
  h.Descriptor(0, 4, 2, 0);
  const auto old = h.Get(h.descs + 12, 8), next = h.Get(h.descs + 20, 8);
  Monstey_Check(h.Run() == 1 && h.Get(h.descs + 12, 8) == old + h.pool &&
                    h.Get(h.descs + 20, 8) == next,
                "inner count not reloaded after aliased add");
  ++Monstey_cases;
  Monstey_Fixture j;
  j.Script(0, 1);
  j.Descriptor(0, 4, 1, 0);
  j.Put(j.words, std::numeric_limits<std::uint64_t>::max() - j.pool + 6, 8);
  Monstey_Check(j.Run() == 1 && j.Get(j.words, 8) == 5, "64bit wrap addition");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "frame descriptor counts and full64 alias arithmetic");
}
void Monstey_FaultPrefixes() {
  Monstey_Fixture f;
  f.Script(0, 0, 0x80);
  f.record = f.base + f.bytes.size() - 2;
  try {
    (void)f.Run();
    throw std::runtime_error("record fault absent");
  } catch (const Monstey::Monstey_GadbPoolRelocationFault &e) {
    Monstey_Check(e.address == f.record + 8 && e.width == 8 && e.writing,
                  "record publication fault order");
    Monstey_Check(f.Get(f.record, 1) == 0 && f.Get(f.record + 1, 1) == 0x80,
                  "flags and count prefix not retained");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Script(1, 0);
  g.Put(g.cursor, g.base + g.bytes.size() - 4, 8);
  try {
    (void)g.Run();
    throw std::runtime_error("zero fault absent");
  } catch (const Monstey::Monstey_GadbPoolRelocationFault &e) {
    Monstey_Check(e.writing && e.width == 8 && g.events.size() == 4,
                  "zero before virtual payload access");
    Monstey_Check(g.Get(g.cursor, 8) == g.base + g.bytes.size() + 4,
                  "cursor failure prefix");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture h;
  h.Script(0, 1, 0, 0xFFFFFFFFu);
  try {
    (void)h.Run();
    throw std::runtime_error("descriptor scale fault absent");
  } catch (const Monstey::Monstey_GadbPoolRelocationFault &e) {
    Monstey_Check(e.address == h.descs + 0xFFFFFFFF0ULL + 10 && !e.writing,
                  "descriptor index must widen before scale");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture j;
  j.Script(0, 0);
  j.frame = j.stream - 0x58;
  try {
    (void)j.Run();
    throw std::runtime_error("home alias stream fault absent");
  } catch (const Monstey::Monstey_GadbPoolRelocationFault &e) {
    Monstey_Check(e.address == 0x8877665544332219ULL,
                  "home write precedes typed read");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_groups.push_back("consumed faults and exact prior stores");
}
void Monstey_ProviderEpochs() {
  Monstey_Fixture f;
  f.Script(1, 0);
  f.hook = [](auto &x, std::size_t k, const auto &call) {
    Monstey_Check(call.target == 0xFEDCBA0987654321ULL + k,
                  "stale stream target");
    x.Put(x.table + 8, call.target + 1, 8);
  };
  Monstey_Check(f.Run() == 1, "target result");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Script(0, 0);
  auto old = g.Service();
  Monstey::Monstey_GadbPoolReadService service;
  unsigned before = 0, after = 0;
  service = [&](auto &m, const auto &call) {
    ++before;
    const auto raw = old(m, call);
    service = [&](auto &mm, const auto &cc) {
      ++after;
      return old(mm, cc);
    };
    return raw;
  };
  Monstey_Check(g.Run(service) == 1 && before == 1 && after == 3,
                "provider target replacement did not persist");
  ++Monstey_cases;
  Monstey_Fixture h;
  h.Script(0, 0);
  auto replaced = h.bytes;
  h.hook = [&](auto &x, std::size_t k, const auto &) {
    if (k == 0) {
      replaced = x.bytes;
      x.m = {x.base, replaced};
    }
  };
  Monstey_Check(h.Run() == 1 &&
                    h.Get(h.record + 24, 8) == 0x123456789ABCDEF0ULL &&
                    h.bytes[0x300] == 0xCD,
                "owned view rebind extension");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "persistent state replacement target and view epochs");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::runtime_error("Usage REPORT NONCE");
    Monstey_Basic();
    Monstey_RawReturns();
    Monstey_Failures();
    Monstey_PartialHeaders();
    Monstey_LiveAliases();
    Monstey_FaultPrefixes();
    Monstey_ProviderEpochs();
    std::ofstream out(argv[1], std::ios::binary);
    if (!out)
      throw std::runtime_error("report path");
    out << "{\n\"status\":\"PASS\",\n\"nonce\":\"" << argv[2]
        << "\",\n\"groups\":" << Monstey_groups.size()
        << ",\n\"cases\":" << Monstey_cases
        << ",\n\"checks\":" << Monstey_checks
        << ",\n\"faults\":" << Monstey_faults
        << ",\n\"original_code_executed\":false,\n\"synthetic_only\":true\n}\n";
    if (!out)
      throw std::runtime_error("report write");
    std::cout << "PASS " << Monstey_groups.size() << " groups " << Monstey_cases
              << " cases " << Monstey_checks << " checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
