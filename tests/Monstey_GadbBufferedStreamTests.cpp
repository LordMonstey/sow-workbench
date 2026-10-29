#include "Monstey_GadbBufferedStream.hpp"
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
using Monstey_TestMemory = Monstey::Monstey_GadbBinderMemory;
using Monstey_TestResult = Monstey::Monstey_GadbBufferedResult;
std::uint64_t Monstey_cases = 0, Monstey_checks = 0, Monstey_faults = 0;
std::vector<std::string> Monstey_groups;
void Monstey_Check(bool ok, const char *why) {
  ++Monstey_checks;
  if (!ok)
    throw std::runtime_error(why);
}
std::uint64_t Monstey_Get(Monstey_TestMemory m, std::uint64_t at,
                          std::size_t n) {
  if (at < m.base || at - m.base > m.bytes.size() ||
      n > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw std::runtime_error("test primitive consumed read unavailable");
  std::uint64_t v = 0;
  for (std::size_t k = 0; k < n; ++k)
    v |= std::uint64_t(m.bytes[static_cast<std::size_t>(at - m.base) + k])
         << (8u * k);
  return v;
}
void Monstey_Put(Monstey_TestMemory m, std::uint64_t at, std::uint64_t v,
                 std::size_t n) {
  if (at < m.base || at - m.base > m.bytes.size() ||
      n > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw std::runtime_error("test primitive consumed write unavailable");
  for (std::size_t k = 0; k < n; ++k)
    m.bytes[static_cast<std::size_t>(at - m.base) + k] =
        static_cast<std::uint8_t>(v >> (8u * k));
}
struct Monstey_Event {
  char kind;
  std::uint64_t site, target, dest, src, count, out, stack;
};
struct Monstey_IO {
  std::uint64_t rax = 1, rdx = 0xD100, reported = 0;
  std::uint32_t write_count_bytes = 4;
  bool full = false, write_data = true;
};
struct Monstey_Fixture {
  std::uint64_t base = 0x10000000000ULL;
  std::vector<std::uint8_t> bytes;
  Monstey_TestMemory m;
  std::uint64_t outer, inner, table, buffer, dest, frame, out_count;
  Monstey::Monstey_GadbBufferedCells cells;
  Monstey::Monstey_GadbBufferedServices services;
  std::vector<Monstey_Event> events;
  std::vector<Monstey_IO> io;
  std::size_t io_calls = 0;
  std::uint64_t transferred = 0;
  std::function<void(Monstey_Fixture &,
                     const Monstey::Monstey_GadbBufferedReadFileCall &)>
      io_hook;
  Monstey_Fixture(std::size_t size = 0x20000)
      : bytes(size, 0xCC), m{base, bytes}, outer(base + 0x1000),
        inner(outer + 8), table(base + 0x2000), buffer(base + 0x3000),
        dest(base + 0x5000), frame(base + 0x10000), out_count(base + 0x180),
        cells{base + 0x100, base + 0x108, base + 0x110} {
    Put(inner, table, 8);
    Put(table + 0x18, 0x141204C2CULL, 8);
    Put(inner + 8, 0x123456789ABCDEULL, 8);
    Put(inner + 0x10, buffer, 8);
    Put(inner + 0x18, 0, 4);
    Put(inner + 0x1C, 0, 4);
    Put(outer + 0x28, 0, 1);
    Put(cells.copy, 0xCAFE1001, 8);
    Put(cells.fill, 0xCAFE1002, 8);
    Put(cells.read_file, 0xCAFE1003, 8);
    for (unsigned k = 0; k < 4096; ++k)
      Put(buffer + k, 0x20u + (k % 0x60u), 1);
    services.copy = [this](auto memory, const auto &call) {
      events.push_back({'C', call.native_site, call.target, call.destination,
                        call.source, call.count, 0, 0});
      for (std::uint64_t k = 0; k < call.count; ++k)
        Monstey_Put(memory, call.destination + k,
                    Monstey_Get(memory, call.source + k, 1), 1);
      return Monstey_TestResult{0x12345678AA55AA55ULL, 0xC001};
    };
    services.fill = [this](auto memory, const auto &call) {
      Monstey_Check(call.value == 0, "fill value must be native zero");
      events.push_back({'F', call.native_site, call.target, call.destination, 0,
                        call.count, 0, 0});
      for (std::uint64_t k = 0; k < call.count; ++k)
        Monstey_Put(memory, call.destination + k, 0, 1);
      return Monstey_TestResult{0xF0E0D0C0B0A099AAULL, 0xF001};
    };
    services.read_file = [this, index = std::size_t(0)](
                             auto memory, const auto &call) mutable {
      Monstey_Check(index == io_calls, "ReadFile mutable target state reset");
      Monstey_Check(index < io.size(), "unexpected OS read");
      const auto step = io[index++];
      ++io_calls;
      Monstey_Check(call.overlapped == 0 &&
                        Monstey_Get(memory, call.stack_argument_address, 8) ==
                            0,
                    "known null overlapped stack store");
      events.push_back({'R', call.native_site, call.target, call.destination,
                        call.handle, call.count, call.bytes_read_address,
                        call.stack_argument_address});
      const auto got = step.full ? std::uint64_t(call.count) : step.reported;
      if (step.write_data)
        for (std::uint64_t k = 0; k < got; ++k)
          Monstey_Put(memory, call.destination + k,
                      0x80u + ((transferred + k) % 0x60u), 1);
      transferred += got;
      if (step.write_count_bytes)
        Monstey_Put(memory, call.bytes_read_address, got,
                    step.write_count_bytes);
      if (io_hook)
        io_hook(*this, call);
      return Monstey_TestResult{step.rax, step.rdx};
    };
  }
  void Put(std::uint64_t at, std::uint64_t v, std::size_t n) {
    Monstey_Put(m, at, v, n);
  }
  std::uint64_t Get(std::uint64_t at, std::size_t n) const {
    return Monstey_Get(m, at, n);
  }
  Monstey_TestResult Read(std::uint32_t count) {
    return Monstey::Monstey_GadbBufferedRead(m, outer, dest, count, frame,
                                             services, cells);
  }
  Monstey_TestResult Direct(std::uint32_t count,
                            std::uint64_t incoming = 0xBA9876543210EEFFULL) {
    return Monstey::Monstey_GadbBufferedDirectRead(
        m, inner, dest, count, out_count, frame, incoming, services, cells);
  }
};
void Monstey_CachedMatrix() {
  for (std::uint32_t pos = 0; pos < 16; ++pos)
    for (std::uint32_t end = 0; end < 16; ++end)
      for (std::uint32_t count = 0; count < 17; ++count) {
        Monstey_Fixture f;
        f.Put(f.inner + 0x18, pos, 4);
        f.Put(f.inner + 0x1C, end, 4);
        auto expected = f.bytes;
        Monstey_TestMemory e{f.base, expected};
        const auto sum = std::uint32_t(pos + count);
        const bool pass = sum <= end;
        if (pass) {
          for (std::uint32_t k = 0; k < count; ++k)
            Monstey_Put(e, f.dest + k, Monstey_Get(e, f.buffer + pos + k, 1),
                        1);
          Monstey_Put(e, f.inner + 0x18, sum == end ? 0 : sum, 4);
          if (sum == end)
            Monstey_Put(e, f.inner + 0x1C, 0, 4);
        }
        const auto result = Monstey::Monstey_GadbBufferedCopyCached(
            f.m, f.inner, f.dest, count, f.services, f.cells);
        const auto raw = (std::uint64_t(sum) & ~255ULL) | std::uint64_t(pass);
        Monstey_Check(result.rax == raw &&
                          result.rdx == (pass ? 0xC001ULL : f.dest),
                      "cached raw result/register");
        Monstey_Check(f.bytes == expected, "cached complete arena");
        Monstey_Check(f.events.size() == std::size_t(pass),
                      "cached primitive count including zero");
        ++Monstey_cases;
      }
  Monstey_groups.push_back(
      "independent cached algebra matrix full arenas rawRAX/RDX");
}
void Monstey_OuterBasics() {
  Monstey_Fixture f;
  f.Put(f.inner + 0x1C, 8, 4);
  const auto result = f.Read(3);
  Monstey_Check(result.rax == 0 && result.rdx == 0xC001 &&
                    f.Get(f.inner + 0x18, 4) == 3,
                "outer cached success");
  Monstey_Check(f.events.size() == 1 && f.events[0].kind == 'C',
                "outer uses concrete cached path");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Put(g.inner + 0x18, 10, 4);
  g.Put(g.inner + 0x1C, 1, 4);
  const auto before = g.bytes;
  const auto wrapped = g.Read(1);
  Monstey_Check(wrapped.rax == 0 && wrapped.rdx == g.dest &&
                    g.bytes == before && g.events.empty(),
                "native fast branch forces AL1 after failed cache guard");
  ++Monstey_cases;
  Monstey_Fixture h;
  h.Put(h.outer + 0x28, 1, 1);
  h.Put(h.inner, 0, 8);
  const auto error = h.Read(0);
  Monstey_Check(error.rax == 1 && error.rdx == 0xF001 && h.events.size() == 1 &&
                    h.events[0].kind == 'F' && h.events[0].count == 0,
                "sticky zero read still fills and skips vtable");
  ++Monstey_cases;
  Monstey_Fixture j;
  const auto zero = j.Read(0);
  Monstey_Check(zero.rax == 0 && zero.rdx == 0xC001 && j.events.size() == 1 &&
                    j.events[0].kind == 'C' && j.events[0].count == 0,
                "zero healthy read still consumes copy");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "outer concrete dispatch sticky zero and forcedAL paths");
}
void Monstey_DirectStatus() {
  for (auto status : {0ULL, 2ULL, 0xFFFFFFFFULL, 0xABCDEF0000000001ULL,
                      0xABCDEF0000000002ULL}) {
    Monstey_Fixture f;
    f.io.push_back({status, 0xD123, 3, 4, false, true});
    const auto result = f.Direct(3);
    const bool pass = static_cast<std::uint32_t>(status) == 1;
    const auto expected = pass ? 1ULL : (status & ~255ULL);
    Monstey_Check(result.rax == expected && result.rdx == 0xD123,
                  "ReadFile exact EAX1 and raw return");
    Monstey_Check(f.Get(f.out_count, 4) == (pass ? 3 : 0),
                  "failed OS count not accumulated");
    Monstey_Check(f.Get(f.out_count + 4, 4) == 0xCCCCCCCC &&
                      f.Get(f.frame + 0x54, 4) == 0xCCCCCCCC,
                  "DWORD output/private count padding must remain supplied");
    Monstey_Check(f.Get(f.dest, 1) == 0x80,
                  "direct failure does not roll back prefix");
    ++Monstey_cases;
  }
  Monstey_Fixture f;
  const auto initial = 0xFEEDEECCDDCCBB99ULL;
  const auto zero = f.Direct(0, initial);
  Monstey_Check(zero.rax == ((initial & ~255ULL) | 1) && zero.rdx == f.dest &&
                    f.events.empty() && f.Get(f.out_count, 4) == 0,
                "zero direct retains incomingRAX and destinationRDX");
  Monstey_Check(f.Get(f.out_count + 4, 4) == 0xCCCCCCCC,
                "zero direct count resets only DWORD");
  Monstey_Check(f.Get(f.frame + 0x20, 8) == 0xCCCCCCCCCCCCCCCCULL,
                "zero direct must not store overlapped");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.io = {{0xABCDEF0000000001ULL, 0xD124, 0, 4, false, false}};
  const auto eof = g.Direct(9);
  Monstey_Check(eof.rax == 0xABCDEF0000000001ULL && eof.rdx == 0xD124 &&
                    g.Get(g.out_count, 4) == 0,
                "successful EOF raw RAX");
  ++Monstey_cases;
  Monstey_groups.push_back("ReadFile exact status zero EOF and raw registers");
}
void Monstey_PartialChunks() {
  Monstey_Fixture f;
  f.io = {{1, 0xD101, 2, 4, false, true},
          {1, 0xD102, 3, 4, false, true},
          {1, 0xD103, 0, 4, false, false}};
  const auto partial = f.Direct(9);
  Monstey_Check(partial.rax == 1 && partial.rdx == 0xD103 &&
                    f.Get(f.out_count, 4) == 5,
                "partial chunk accumulation before EOF");
  Monstey_Check(f.events.size() == 3 && f.events[0].count == 9 &&
                    f.events[1].count == 7 && f.events[2].count == 4,
                "remaining count updates");
  Monstey_Check(f.events[2].dest == f.dest + 5,
                "destination advances only successful reported bytes");
  ++Monstey_cases;
  Monstey_Fixture g(0x810000);
  g.io = {{1, 0xD111, 0, 4, true, true}, {1, 0xD112, 0, 4, true, true}};
  g.io_hook = [](auto &x, const auto &) {
    if (x.io_calls == 1) {
      x.Put(x.cells.read_file, 0xCAFE2003, 8);
      x.Put(x.inner + 8, 0x7777, 8);
    }
  };
  const auto large = g.Direct(0x7D0000u + 17u);
  Monstey_Check(large.rax == 1 && large.rdx == 0xD112 &&
                    g.Get(g.out_count, 4) == 0x7D0011,
                "large chunk total/result");
  Monstey_Check(g.events.size() == 2 && g.events[0].count == 0x7D0000 &&
                    g.events[1].count == 17,
                "native chunk cap");
  Monstey_Check(g.events[1].target == 0xCAFE2003 &&
                    g.events[1].src == 0x123456789ABCDEULL,
                "live IAT target and cached handle");
  ++Monstey_cases;
  Monstey_Fixture h;
  h.io = {{1, 0xD120, 5, 4, false, true}};
  h.io_hook = [](auto &x, const auto &) { x.Put(x.out_count, 0xFFFFFFFE, 4); };
  const auto wrap = h.Direct(5);
  Monstey_Check(wrap.rax == 1 && h.Get(h.out_count, 4) == 3,
                "live U32 outcount wrapping");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "partial OS counts native chunk limit live target cached handle wrap");
}
void Monstey_RefillThreshold() {
  for (auto count : {1u, 17u, 0x7FFu, 0x800u, 0x801u}) {
    Monstey_Fixture f;
    f.io = {{1, 0xD201, 0, 4, true, true}};
    const auto result = f.Read(count);
    Monstey_Check(result.rax == 0, "refill/direct outer result");
    Monstey_Check(f.io_calls == 1, "unexpected extra I/O");
    if (count < 0x800) {
      Monstey_Check(f.events[0].kind == 'R' && f.events[0].count == 0x1000 &&
                        f.events[0].dest == f.buffer,
                    "refill size/buffer");
      Monstey_Check(f.events[0].out == f.frame - 0x60 &&
                        f.events[0].stack == f.frame - 0x90,
                    "refill nested frames");
      Monstey_Check(f.Get(f.inner + 0x18, 4) == count &&
                        f.Get(f.inner + 0x1C, 4) == 0x1000,
                    "refill live cache state");
    } else {
      Monstey_Check(f.events[0].count == count && f.events[0].dest == f.dest,
                    "direct threshold route");
      Monstey_Check(f.events[0].out == f.frame - 0x30 &&
                        f.events[0].stack == f.frame - 0x60,
                    "direct nested count/overlapped frames");
      Monstey_Check(f.Get(f.frame + 0x10, 4) == count,
                    "direct parent count local");
    }
    ++Monstey_cases;
  }
  Monstey_groups.push_back(
      "0x800 threshold 0x1000 refill and exact nested frame writes");
}
void Monstey_DoubleFill() {
  Monstey_Fixture f;
  f.Put(f.inner + 0x18, 2, 4);
  f.Put(f.inner + 0x1C, 4, 4);
  f.io = {{1, 0xD301, 3, 4, false, true}, {1, 0xD302, 0, 4, false, false}};
  const auto result = f.Read(17);
  Monstey_Check(result.rax == 1 && result.rdx == 0xF001 &&
                    f.Get(f.outer + 0x28, 1) == 1,
                "short refill sticky error");
  Monstey_Check(f.events.size() == 5 && f.events[0].kind == 'C' &&
                    f.events[1].kind == 'R' && f.events[2].kind == 'R' &&
                    f.events[3].site == 0x141204C77ULL &&
                    f.events[4].site == 0x141B50E29ULL,
                "both fills/order preserved");
  for (unsigned k = 0; k < 17; ++k)
    Monstey_Check(f.Get(f.dest + k, 1) == 0,
                  "full original destination cleared after prefix");
  Monstey_Check(f.Get(f.inner + 0x1C, 4) == 3 && f.Get(f.inner + 0x18, 4) == 0,
                "no rollback of refilled cache");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Put(g.outer + 0x28, 1, 1);
  g.dest = g.outer + 0x28;
  const auto alias = g.Read(1);
  Monstey_Check(alias.rax == 0 && alias.rdx == 0xF001 &&
                    g.Get(g.outer + 0x28, 1) == 0,
                "sticky must be reread after alias fill");
  ++Monstey_cases;
  Monstey_Fixture h;
  h.dest = h.outer + 0x18;
  h.io = {{0, 0xD303, 0, 4, false, false}};
  const auto twice = h.Read(17);
  Monstey_Check(twice.rax == 0 && h.events.size() == 3 &&
                    h.events[1].kind == 'F' && h.events[2].kind == 'F',
                "double-fill aliases retain late sticky epoch");
  Monstey_Check(h.Get(h.outer + 0x18, 8) == 0 && h.Get(h.outer + 0x28, 1) == 0,
                "alias fills affect actual object fields");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "partial prefix double fills sticky reread and aliases");
}
void Monstey_LiveCopyAlias() {
  Monstey_Fixture f;
  f.Put(f.inner + 0x18, 0, 4);
  f.Put(f.inner + 0x1C, 8, 4);
  f.Put(f.buffer, 5, 4);
  f.Put(f.buffer + 4, 20, 4);
  const auto result = Monstey::Monstey_GadbBufferedCopyCached(
      f.m, f.inner, f.inner + 0x18, 8, f.services, f.cells);
  Monstey_Check(result.rax == 1 && result.rdx == 0xC001 &&
                    f.Get(f.inner + 0x18, 4) == 13 &&
                    f.Get(f.inner + 0x1C, 4) == 20,
                "cache updates use current metadata after copy");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.io = {{1, 0xD400, 3, 4, false, true}, {1, 0xD401, 0, 4, false, false}};
  const auto over = g.Direct(1);
  Monstey_Check(
      over.rax == 1 && g.Get(g.out_count, 4) == 3 && g.events.size() == 2 &&
          g.events[1].count == 0x7D0000 && g.events[1].dest == g.dest + 3,
      "external over-report uses native unsigned remaining arithmetic");
  ++Monstey_cases;
  Monstey_Fixture h;
  h.Put(h.frame + 0x50, 0x100, 4);
  h.io = {{1, 0xD402, 2, 1, false, true}, {2, 0xD403, 0, 0, false, false}};
  const auto partial = h.Direct(0x102);
  Monstey_Check(partial.rax == 0x101 && h.Get(h.out_count, 4) == 0x102,
                "unwritten private bytes preserved rather than zeroed");
  Monstey_Check(h.events.size() == 1,
                "partial output count completed exact native request");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "live metadata copy aliases raw malformed external counts private bytes");
}
void Monstey_Faults() {
  Monstey_Fixture f;
  const auto invalid = f.base + f.bytes.size();
  try {
    (void)Monstey::Monstey_GadbBufferedDirectRead(
        f.m, invalid, f.dest, 0, invalid, f.frame, 0x55, f.services, f.cells);
    throw std::runtime_error("outcount fault absent");
  } catch (const Monstey::Monstey_GadbBufferedMemoryFault &e) {
    Monstey_Check(e.address == invalid && e.width == 4 && !e.writing,
                  "outcount RMW consumed read first");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture g;
  try {
    (void)Monstey::Monstey_GadbBufferedDirectRead(g.m, invalid, g.dest, 0,
                                                  g.out_count, g.frame, 0x55,
                                                  g.services, g.cells);
    throw std::runtime_error("handle fault absent");
  } catch (const Monstey::Monstey_GadbBufferedMemoryFault &e) {
    Monstey_Check(e.address == invalid + 8 && g.Get(g.out_count, 4) == 0,
                  "zero direct still consumes handle after outcount reset");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture h;
  h.services.read_file = {};
  try {
    (void)h.Direct(1);
    throw std::runtime_error("import fault absent");
  } catch (const Monstey::Monstey_GadbBufferedImportFault &e) {
    Monstey_Check(e.native_site == 0x141204D34ULL && e.target == 0xCAFE1003 &&
                      h.Get(h.out_count, 4) == 0 &&
                      h.Get(h.frame + 0x20, 8) == 0,
                  "known prefix before missing OS primitive");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture j;
  j.services.read_file = {};
  j.cells.read_file = invalid;
  try {
    (void)j.Direct(1);
    throw std::runtime_error("IAT read fault absent");
  } catch (const Monstey::Monstey_GadbBufferedMemoryFault &e) {
    Monstey_Check(e.address == invalid && !e.writing &&
                      j.Get(j.frame + 0x20, 8) == 0,
                  "IAT consumed before provider availability");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture k;
  k.Put(k.table + 0x18, 0xDEAD, 8);
  try {
    (void)k.Read(1);
    throw std::runtime_error("dispatch fault absent");
  } catch (const Monstey::Monstey_GadbBufferedDispatchFault &e) {
    Monstey_Check(e.native_site == 0x141B50E16ULL && e.target == 0xDEAD &&
                      k.events.empty() && k.Get(k.outer + 0x28, 1) == 0,
                  "concrete inner target gate at consumed call");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture n;
  n.frame = n.cells.read_file - 0x20;
  n.services.read_file = [&](auto, const auto &call) -> Monstey_TestResult {
    n.events.push_back({'R', call.native_site, call.target, call.destination,
                        call.handle, call.count, call.bytes_read_address,
                        call.stack_argument_address});
    throw Monstey::Monstey_GadbBufferedImportFault(
        Monstey::Monstey_GadbBufferedImport::read_file, call.native_site,
        call.slot, call.target);
  };
  try {
    (void)n.Direct(1);
    throw std::runtime_error("explicit primitive target rejection absent");
  } catch (const Monstey::Monstey_GadbBufferedImportFault &e) {
    Monstey_Check(e.target == 0 && n.events.size() == 1 &&
                      n.events[0].target == 0,
                  "overlapped store alias changes import target before "
                  "explicitly supplied rejection");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_groups.push_back("consumed fault order partial known stack stores "
                           "dynamic target and aliases");
}
void Monstey_StateReplacement() {
  Monstey_Fixture f;
  f.io = {{1, 0xD601, 2, 4, false, true}, {1, 0xD602, 2, 4, false, true}};
  auto original = f.services.read_file;
  unsigned first = 0, second = 0;
  f.services.read_file = [&](auto m, const auto &call) {
    ++first;
    auto result = original(m, call);
    f.services.read_file = [&](auto mm, const auto &cc) {
      ++second;
      return original(mm, cc);
    };
    return result;
  };
  const auto result = f.Direct(4);
  Monstey_Check(result.rax == 1 && result.rdx == 0xD602 && first == 1 &&
                    second == 1,
                "self-replacement snapshots persistent target lifetime");
  ++Monstey_cases;
  Monstey_groups.push_back("provider state and self replacement");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::runtime_error("Usage REPORT NONCE");
    Monstey_CachedMatrix();
    Monstey_OuterBasics();
    Monstey_DirectStatus();
    Monstey_PartialChunks();
    Monstey_RefillThreshold();
    Monstey_DoubleFill();
    Monstey_LiveCopyAlias();
    Monstey_Faults();
    Monstey_StateReplacement();
    std::ofstream out(argv[1], std::ios::binary);
    if (!out)
      throw std::runtime_error("report unavailable");
    out << "{\n\"status\":\"PASS\",\n\"nonce\":\"" << argv[2]
        << "\",\n\"groups\":" << Monstey_groups.size()
        << ",\n\"cases\":" << Monstey_cases
        << ",\n\"checks\":" << Monstey_checks
        << ",\n\"faults\":" << Monstey_faults
        << ",\n\"original_code_executed\":false,\n\"synthetic_only\":true\n}\n";
    if (!out)
      throw std::runtime_error("report failed");
    std::cout << "PASS " << Monstey_groups.size() << " groups " << Monstey_cases
              << " cases " << Monstey_checks << " checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
