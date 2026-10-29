#pragma once
#include "Monstey_FearDbCallable.hpp" // shared generic U2097 lifetime utility only
#include "Monstey_GadbBinderComposition.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
namespace Monstey {
class Monstey_GadbPoolRelocationFault final : public std::runtime_error {
public:
  std::uint64_t address;
  std::size_t width;
  bool writing;
  Monstey_GadbPoolRelocationFault(std::uint64_t, std::size_t, bool);
};
class Monstey_GadbPoolReadBoundaryFault final : public std::runtime_error {
public:
  std::uint64_t native_site, target;
  Monstey_GadbPoolReadBoundaryFault(std::uint64_t, std::uint64_t);
};
struct Monstey_GadbPoolReadCall {
  std::uint64_t stream = 0, target = 0, destination = 0;
  std::uint32_t byte_count = 0;
  std::uint64_t native_site = 0;
};
// The numeric original target is observed, never called. Stateful providers
// keep one persistent target and per-call lifetime through U2097, including
// replacement.
using Monstey_GadbPoolReadService = Monstey_FearDbCallable<std::uint64_t(
    Monstey_GadbBinderMemory &, const Monstey_GadbPoolReadCall &)>;
// Complete leaf4BA364..4BA382. Input is rawECX, without a hidden low6 mask.
std::uint8_t Monstey_GadbIsPoolRelativeType(std::uint32_t raw_ecx);
// Leaf writes only AL; all other incoming RAX bits are retained.
std::uint64_t Monstey_GadbPoolRelativeTypeRaw(std::uint32_t raw_ecx,
                                              std::uint64_t incoming_rax);
// Complete115FD0C/2C/4C: reread stream/vtable+8, invoke only owned primitive,
// ignore primitive status and return the supplied stream identity. No prezero.
std::uint64_t Monstey_GadbPoolReadU8(Monstey_GadbBinderMemory &,
                                     std::uint64_t stream,
                                     std::uint64_t destination,
                                     const Monstey_GadbPoolReadService &);
std::uint64_t Monstey_GadbPoolReadU16(Monstey_GadbBinderMemory &,
                                      std::uint64_t stream,
                                      std::uint64_t destination,
                                      const Monstey_GadbPoolReadService &);
std::uint64_t Monstey_GadbPoolReadU32(Monstey_GadbBinderMemory &,
                                      std::uint64_t stream,
                                      std::uint64_t destination,
                                      const Monstey_GadbPoolReadService &);
// Complete named-record loader4BA1F0..4BA363 over exclusively borrowed raw64
// identities. Frame is native RBP=entryRSP-28h. Caller supplies poolQword+50,
// descriptorBaseQword+58,cursorAddressQword+68. The known originalRDX home
// store atframe+38 is retained before header reads; unknown savedCPU/return
// words are supplied bytes, never fabricated. Header
// destinations:-0C,-10,+38,-8,+60. Aliases, partial read/fault prefixes,
// low32status test, live count reloads and full64pool addition follow native
// epochs. Returns rawRAX, not only a bool. Hash423E70 is concretely reused; its
// existing ValueFixupFault may propagate. No semantic4BAF6Cfixup, allocator,
// disk writer or original physical ABI is added.
std::uint64_t Monstey_GadbLoadPoolRelativeRecord(
    Monstey_GadbBinderMemory &, std::uint64_t category,
    std::uint64_t original_rdx, std::uint64_t record, std::uint64_t stream,
    std::uint64_t frame_rbp, const Monstey_GadbPoolReadService &);
// Optional supplied-memory partial-read profile, not original stream ABI.
// Copies current source bytes in order, retaining position/prefix on a consumed
// fault. Short reads return the caller-declared raw value; success returns0.
struct Monstey_GadbPoolOwnedMemoryRead {
  std::uint64_t source = 0, length = 0, position = 0, short_read_result = 1;
  std::uint64_t Monstey_Read(Monstey_GadbBinderMemory &,
                             const Monstey_GadbPoolReadCall &);
};
} // namespace Monstey