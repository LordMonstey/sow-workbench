#include "Monstey_GadbEncodedString.hpp"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
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
class Monstey_PrimitiveFault final : public std::runtime_error {
public:
  std::uint64_t address;
  bool writing;
  Monstey_PrimitiveFault(std::uint64_t at, bool w)
      : std::runtime_error("owned primitive range/explicit fault"), address(at),
        writing(w) {}
};
std::uint64_t Monstey_Get(Monstey_TestMemory m, std::uint64_t at,
                          std::size_t n) {
  if (at < m.base || at - m.base > m.bytes.size() ||
      n > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw Monstey_PrimitiveFault(at, false);
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
    throw Monstey_PrimitiveFault(at, true);
  for (std::size_t k = 0; k < n; ++k)
    m.bytes[static_cast<std::size_t>(at - m.base) + k] =
        static_cast<std::uint8_t>(v >> (8u * k));
}
struct Monstey_Event {
  char kind;
  std::uint64_t destination, count, bytes_read, stack;
};
struct Monstey_Fixture {
  std::uint64_t base = 0x10000000000ULL;
  std::vector<std::uint8_t> bytes;
  Monstey_TestMemory memory;
  std::uint64_t outer, outer_table, inner_table, buffer, destination, frame;
  Monstey::Monstey_GadbBufferedCells cells;
  Monstey::Monstey_GadbBufferedServices services;
  std::vector<std::uint8_t> input;
  std::size_t input_position = 0, copy_calls = 0, fill_calls = 0, io_calls = 0;
  std::vector<Monstey_Event> events;
  std::function<void(Monstey_Fixture &,
                     const Monstey::Monstey_GadbBufferedCopyCall &)>
      copy_hook;
  bool partial_fill_fault = false;
  Monstey_Fixture()
      : bytes(0x40000, 0xCC), memory{base, bytes}, outer(base + 0x1000),
        outer_table(base + 0x1100), inner_table(base + 0x1200),
        buffer(base + 0x4000), destination(base + 0x8000),
        frame(base + 0x20000), cells{base + 0x100, base + 0x108, base + 0x110} {
    Put(outer, outer_table, 8);
    Put(outer_table + 8, 0x141B50DE8ULL, 8);
    Put(outer + 8, inner_table, 8);
    Put(inner_table + 0x18, 0x141204C2CULL, 8);
    Put(outer + 0x10, 0x1234, 8);
    Put(outer + 0x18, buffer, 8);
    Put(outer + 0x20, 0, 4);
    Put(outer + 0x24, 0, 4);
    Put(outer + 0x28, 0, 1);
    Put(cells.copy, 0xC011, 8);
    Put(cells.fill, 0xF011, 8);
    Put(cells.read_file, 0xD011, 8);
    services.copy = [this](auto m, const auto &call) {
      ++copy_calls;
      events.push_back({'C', call.destination, call.count, 0, 0});
      for (std::uint64_t k = 0; k < call.count; ++k)
        Monstey_Put(m, call.destination + k, Monstey_Get(m, call.source + k, 1),
                    1);
      if (copy_hook)
        copy_hook(*this, call);
      return Monstey_TestResult{0xDEADFACE, 0xC0000000ULL + copy_calls};
    };
    services.fill = [this](auto m, const auto &call) {
      ++fill_calls;
      events.push_back({'F', call.destination, call.count, 0, 0});
      for (std::uint64_t k = 0; k < call.count; ++k) {
        Monstey_Put(m, call.destination + k, 0, 1);
        if (partial_fill_fault)
          throw Monstey_PrimitiveFault(call.destination + k, true);
      }
      return Monstey_TestResult{0xFEEDBEEFAA, 0xF0000000ULL + fill_calls};
    };
    services.read_file = [this, index = std::size_t(0)](
                             auto m, const auto &call) mutable {
      Monstey_Check(index++ == io_calls, "ReadFile state cloned");
      ++io_calls;
      events.push_back({'R', call.destination, call.count,
                        call.bytes_read_address, call.stack_argument_address});
      const auto got =
          std::min<std::size_t>(call.count, input.size() - input_position);
      for (std::size_t k = 0; k < got; ++k)
        Monstey_Put(m, call.destination + k, input[input_position + k], 1);
      input_position += got;
      Monstey_Put(m, call.bytes_read_address, got, 4);
      return Monstey_TestResult{1, 0xD0000000ULL + io_calls};
    };
  }
  void Put(std::uint64_t at, std::uint64_t v, std::size_t n) {
    Monstey_Put(memory, at, v, n);
  }
  std::uint64_t Get(std::uint64_t at, std::size_t n) const {
    return Monstey_Get(memory, at, n);
  }
  void Input(std::uint16_t length, const std::vector<std::uint8_t> &payload) {
    input = {static_cast<std::uint8_t>(length),
             static_cast<std::uint8_t>(length >> 8)};
    input.insert(input.end(), payload.begin(), payload.end());
  }
  Monstey_TestResult Run(std::uint32_t capacity) {
    return Monstey::Monstey_GadbReadEncodedString(
        memory, outer, destination, capacity, frame, services, cells);
  }
};
std::vector<std::uint8_t> Monstey_Payload(std::size_t n) {
  std::vector<std::uint8_t> p(n);
  for (std::size_t k = 0; k < n; ++k)
    p[k] = static_cast<std::uint8_t>(k * 37u);
  return p;
}
void Monstey_CapacityMatrix() {
  for (std::uint16_t n = 0; n <= 32; ++n)
    for (std::uint32_t cap = 0; cap <= std::uint32_t(n) + 2; ++cap) {
      Monstey_Fixture f;
      const auto p = Monstey_Payload(n);
      f.Input(n, p);
      const auto result = f.Run(cap);
      const bool fits = cap >= std::uint32_t(n) + 1;
      Monstey_Check(result.rax == (fits ? n : std::uint32_t(n) + 1) &&
                        result.rdx ==
                            (fits ? (n ? 0xC0000000ULL + n + 1 : 0) : n),
                    "encoded raw result/capacity");
      Monstey_Check(f.copy_calls == (fits ? std::size_t(n) + 1 : 1) &&
                        f.fill_calls == 0,
                    "typed error/status or oversize character consumption");
      for (std::uint32_t k = 0; k < std::uint32_t(n) + 2; ++k) {
        const auto want = fits ? (k < n ? p[k] : (k == n ? 0 : 0xCC)) : 0xCC;
        Monstey_Check(f.Get(f.destination + k, 1) == std::uint64_t(want),
                      "raw payload/NUL/no write oversize");
      }
      Monstey_Check(f.Get(f.frame + 0x40, 2) == n &&
                        f.Get(f.frame + 0x42, 2) == 0xCCCC,
                    "private length only U16 initialized");
      Monstey_Check(f.events[0].kind == 'R' &&
                        f.events[0].bytes_read == f.frame - 0xC0 &&
                        f.events[0].stack == f.frame - 0xF0,
                    "encoded to typed to buffered exact frames");
      ++Monstey_cases;
    }
  Monstey_groups.push_back(
      "capacity0/1/exact/oversize unsigned matrix rawbytes NUL and frames");
}
void Monstey_Maximum() {
  for (const std::uint16_t n :
       {std::uint16_t(255), std::uint16_t(256), std::uint16_t(4094),
        std::uint16_t(4095), std::uint16_t(65535)}) {
    Monstey_Fixture f;
    auto p = Monstey_Payload(n);
    f.Input(n, p);
    const auto result = f.Run(std::uint32_t(n) + 1);
    Monstey_Check(result.rax == n && result.rdx == 0xC0000000ULL + n + 1,
                  "maximum U16 length/result");
    for (std::uint32_t k = 0; k < n; ++k)
      Monstey_Check(f.Get(f.destination + k, 1) == p[k], "large payload bytes");
    Monstey_Check(f.Get(f.destination + n, 1) == 0 &&
                      f.copy_calls == std::size_t(n) + 1,
                  "maximum NUL and typed calls");
    ++Monstey_cases;
  }
  Monstey_Fixture f;
  f.Input(65535, Monstey_Payload(65535));
  f.destination = f.base + f.bytes.size();
  const auto over = f.Run(65535);
  Monstey_Check(over.rax == 65536 && over.rdx == 65535 && f.copy_calls == 1,
                "length+1 must not truncate U16 and invalid dest unused");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "U16max rawbyte payload cache boundaries and 65536 oversize result");
}
void Monstey_Wrappers() {
  for (bool word : {false, true}) {
    Monstey_Fixture f;
    f.input = {0x34, 0x12};
    const auto result =
        word ? Monstey::Monstey_GadbEncodedReadU16(f.memory, f.outer,
                                                   f.destination, f.frame,
                                                   f.services, f.cells)
             : Monstey::Monstey_GadbEncodedReadByte(f.memory, f.outer,
                                                    f.destination, f.frame,
                                                    f.services, f.cells);
    Monstey_Check(
        result.rax == f.outer && result.rdx == 0xC0000001ULL,
        "raw typed wrapper must preserve Read RDX and force streamRAX");
    Monstey_Check(f.Get(f.destination, word ? 2 : 1) == (word ? 0x1234 : 0x34),
                  "typed width");
    ++Monstey_cases;
    Monstey_Fixture g;
    g.Put(g.outer + 0x28, 1, 1);
    const auto failed =
        word ? Monstey::Monstey_GadbEncodedReadU16(g.memory, g.outer,
                                                   g.destination, g.frame,
                                                   g.services, g.cells)
             : Monstey::Monstey_GadbEncodedReadByte(g.memory, g.outer,
                                                    g.destination, g.frame,
                                                    g.services, g.cells);
    Monstey_Check(failed.rax == g.outer && failed.rdx == 0xF0000001ULL &&
                      g.fill_calls == 1,
                  "typed wrapper ignores Read status but retains exact RDX");
    ++Monstey_cases;
  }
  Monstey_groups.push_back(
      "additive raw U16/U8 wrappers real BufferedRead composition");
}
void Monstey_LengthAliases() {
  Monstey_Fixture f;
  f.Input(3, {'A', 'B', 'C'});
  f.copy_hook = [](auto &x, const auto &call) {
    if (call.destination == x.destination)
      x.Put(x.frame + 0x40, 1, 2);
  };
  const auto shrink = f.Run(4);
  Monstey_Check(shrink.rax == 1 && shrink.rdx == 0xC0000002ULL &&
                    f.Get(f.destination, 2) == 0x41 && f.copy_calls == 2,
                "live shrunk length with last RDX");
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Input(1, {'A', 'B', 'C'});
  g.copy_hook = [](auto &x, const auto &call) {
    if (call.destination == x.destination)
      x.Put(x.frame + 0x40, 3, 2);
  };
  const auto expand = g.Run(2);
  Monstey_Check(expand.rax == 3 && expand.rdx == 0xC0000004ULL &&
                    g.copy_calls == 4,
                "capacity must not be rechecked after live length expands");
  Monstey_Check(g.Get(g.destination, 4) == 0x434241, "expanded source and NUL");
  ++Monstey_cases;
  Monstey_Fixture h;
  h.Input(1, {0});
  h.destination = h.frame + 0x40;
  const auto zero = h.Run(2);
  Monstey_Check(zero.rax == 0 && zero.rdx == 0xC0000002ULL && h.copy_calls == 2,
                "length becomes zero after nonempty path must retain last RDX");
  ++Monstey_cases;
  Monstey_Fixture j;
  j.Input(2, {1, 2});
  j.destination = j.outer + 0x28;
  const auto sticky = j.Run(3);
  Monstey_Check(
      sticky.rax == 2 && sticky.rdx == 0xF0000001ULL && j.fill_calls == 1 &&
          j.Get(j.destination, 3) == 1,
      "typed statuses ignored with real sticky state after destination alias");
  ++Monstey_cases;
  Monstey_groups.push_back(
      "live shrink grow zero and sticky aliases without capacity repair");
}
void Monstey_PartialReads() {
  Monstey_Fixture f;
  f.Input(4, {});
  const auto eof = f.Run(5);
  Monstey_Check(eof.rax == 4 && eof.rdx == 0xF0000005ULL && f.fill_calls == 5 &&
                    f.copy_calls == 1,
                "partial body EOF follows actual doublefill then sticky fills");
  for (unsigned k = 0; k < 5; ++k)
    Monstey_Check(f.Get(f.destination + k, 1) == 0,
                  "partial body output zero and final NUL");
  ++Monstey_cases;
  for (auto cap : {0u, 1u}) {
    Monstey_Fixture g;
    g.input = {2};
    const auto header = g.Run(cap);
    Monstey_Check(
        header.rax == (cap ? 0 : 1) && header.rdx == 0 && g.fill_calls == 2,
        "short header actual BufferedFill, no fabricated length fallback");
    Monstey_Check(g.Get(g.destination, 1) == (cap ? 0 : 0xCC),
                  "short header capacity NUL gate");
    ++Monstey_cases;
  }
  Monstey_Fixture h;
  h.input = {2};
  h.Put(h.frame + 0x40, 0xBEEF, 2);
  h.partial_fill_fault = true;
  try {
    (void)h.Run(8);
    throw std::runtime_error("partial external fill fault absent");
  } catch (const Monstey_PrimitiveFault &e) {
    Monstey_Check(
        e.writing && h.Get(h.frame + 0x40, 2) == 0xBE00,
        "unwritten private header byte must stay supplied on partial fault");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_groups.push_back(
      "real partial IO EOF sticky statuses and partial primitive fault prefix");
}
void Monstey_Faults() {
  Monstey_Fixture f;
  f.Input(0, {});
  f.destination = f.base + f.bytes.size();
  try {
    (void)f.Run(1);
    throw std::runtime_error("NUL fault absent");
  } catch (const Monstey::Monstey_GadbEncodedMemoryFault &e) {
    Monstey_Check(e.address == f.destination && e.width == 1 && e.writing &&
                      f.copy_calls == 1,
                  "empty NUL store consumed only after header");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture g;
  g.Input(2, {'A', 'B'});
  g.destination = g.base + g.bytes.size() - 2;
  try {
    (void)g.Run(3);
    throw std::runtime_error("final NUL fault absent");
  } catch (const Monstey::Monstey_GadbEncodedMemoryFault &e) {
    Monstey_Check(e.address == g.destination + 2 && e.writing &&
                      g.Get(g.destination, 2) == 0x4241 && g.copy_calls == 3,
                  "NUL fault preserves two byte writes and count");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture h;
  h.Input(2, {'A', 'B'});
  h.destination = h.base + h.bytes.size();
  const auto unused = h.Run(2);
  Monstey_Check(unused.rax == 3 && unused.rdx == 2 && h.copy_calls == 1,
                "oversize must not validate/write destination");
  ++Monstey_cases;
  Monstey_Fixture j;
  j.Input(2, {'A', 'B'});
  j.Put(j.outer_table + 8, 0xDEAD, 8);
  j.Put(j.outer + 0x28, 1, 1);
  try {
    (void)j.Run(3);
    throw std::runtime_error("outer target fault absent");
  } catch (const Monstey::Monstey_GadbEncodedTargetFault &e) {
    Monstey_Check(e.native_site == 0x14115FD3EULL && e.target == 0xDEAD &&
                      j.events.empty(),
                  "typed outer target gate precedes sticky fill");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture k;
  k.Input(2, {'A', 'B'});
  k.copy_hook = [](auto &x, const auto &call) {
    if (call.destination == x.frame + 0x40)
      x.Put(x.outer_table + 8, 0xBADD, 8);
  };
  try {
    (void)k.Run(3);
    throw std::runtime_error("live byte target fault absent");
  } catch (const Monstey::Monstey_GadbEncodedTargetFault &e) {
    Monstey_Check(e.native_site == 0x14115FCFEULL && e.target == 0xBADD &&
                      k.copy_calls == 1 && k.Get(k.frame + 0x40, 2) == 2,
                  "byte target reread after header prefix");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture n;
  n.Input(2, {'A', 'B'});
  n.Put(n.outer, n.base + n.bytes.size(), 8);
  try {
    (void)n.Run(3);
    throw std::runtime_error("vtable range fault absent");
  } catch (const Monstey::Monstey_GadbEncodedMemoryFault &e) {
    Monstey_Check(!e.writing && e.address == n.base + n.bytes.size() + 8 &&
                      n.events.empty(),
                  "vtable range first fault");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_Fixture p;
  p.Input(2, {0xAA, 'B'});
  p.destination = p.outer;
  try {
    (void)p.Run(3);
    throw std::runtime_error("destination alias vtable fault absent");
  } catch (const Monstey::Monstey_GadbEncodedTargetFault &e) {
    Monstey_Check(
        e.native_site == 0x14115FCFEULL && p.Get(p.outer, 1) == 0xAA &&
            p.copy_calls == 2,
        "destination first byte corrupts next target with no rollback");
    ++Monstey_faults;
  }
  ++Monstey_cases;
  Monstey_groups.push_back(
      "target/load/NUL faults live vtable aliases and exact output prefix");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 3)
      throw std::runtime_error("Usage REPORT NONCE");
    Monstey_CapacityMatrix();
    Monstey_Maximum();
    Monstey_Wrappers();
    Monstey_LengthAliases();
    Monstey_PartialReads();
    Monstey_Faults();
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
