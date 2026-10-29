// SoW API proposal only: eight concrete reset/tail/CAS bodies, 416 native bytes.
#pragma once
#include "Monstey_GadbBufferedStream.hpp"
#include "Monstey_FearDbCallable.hpp"
#include <cstdint>
#include <stdexcept>
namespace Monstey {
struct Monstey_GadbManagerPlacementCells {
  std::uint64_t tail_40 = 0x14277D208ULL;
  std::uint64_t tail_80 = 0x14277CDF8ULL;
  std::uint64_t tail_c0 = 0x14277D618ULL;
  std::uint64_t order_table = 0x14202EE28ULL;
};
// raw is exactly leaf RAX/RDX. remaining is the concrete outgoing R8.
// RCX remains object; R9 remains the caller's incoming value, never a default.
struct Monstey_GadbManagerTailResult {
  Monstey_GadbBufferedResult raw;
  std::uint64_t remaining;
};
// raw is exactly zeroextended status RAX and preserved expected-cell RDX.
// desired_register is the FULL outgoing R8, not merely its consumed low U32.
// target_identity is outgoing R9, established by the concrete leaf MOV R9,RCX.
struct Monstey_GadbManagerCasResult {
  Monstey_GadbBufferedResult raw;
  std::uint64_t desired_register;
  std::uint64_t target_identity;
};
struct Monstey_GadbManagerCompareExchange32Call {
  std::uint64_t native_site;
  std::uint64_t address;
  std::uint32_t comparand;
  std::uint32_t desired;
};
class Monstey_GadbManagerCompareExchange32Fault final
    : public std::runtime_error {
public:
  static constexpr std::uint32_t width = 4;
  std::uint64_t native_site, address;
  std::uint32_t comparand, desired;
  Monstey_GadbManagerCompareExchange32Fault(
      std::uint64_t native_site, std::uint64_t address,
      std::uint32_t comparand, std::uint32_t desired);
};
struct Monstey_GadbManagerPlacementServices {
  // Only a consumed 32-bit CMPXCHG. On normal return, report the observed old
  // U32 and commit desired iff old==comparand. Concrete code derives status
  // and owns the later ordinary expected-cell store on mismatch.
  Monstey_FearDbCallable<std::uint32_t(
      Monstey_GadbBinderMemory,
      const Monstey_GadbManagerCompareExchange32Call &)> compare_exchange32;
};
// Numeric identities in one stable, exclusively borrowed owned image. All
// address/value arithmetic is modulo U64 before each consumed range access.
// Cells are fixed identities for one operation, with native defaults or an
// explicit owned remapping; their contents are read LIVE at the native epoch.
// No alignment/null/disjointness/count guard or service preflight is added.
// A missing service faults only at the consumed opcode, after earlier reads.
// Direct invocation of the current const service field preserves target
// replacement/retirement semantics of the existing shared Callable.
// No physical saved-CPU/return-word/SEH/LOCK/concurrency/OS ABI is qualified.

// 1411755CC / 141173A14 / 141176B24, respectively. Incoming RAX is dead.
// Every iteration stores [H+18] before [H+10]. Raw RAX=object+238h;
// raw RDX=incoming_rdx. Concrete RCX=object, R8=32 and preserved R9 are
// composition facts; the parent overwrites R8 and RDX before their next use.
Monstey_GadbBufferedResult Monstey_GadbResetManagerBackend40(
    Monstey_GadbBinderMemory, std::uint64_t object,
    std::uint64_t incoming_rdx);
Monstey_GadbBufferedResult Monstey_GadbResetManagerBackend80(
    Monstey_GadbBinderMemory, std::uint64_t object,
    std::uint64_t incoming_rdx);
Monstey_GadbBufferedResult Monstey_GadbResetManagerBackendC0(
    Monstey_GadbBinderMemory, std::uint64_t object,
    std::uint64_t incoming_rdx);

// 1404875C0 / 141173B60 / 141176C70. Five ordered stores surround a late
// variant-global read. raw RAX=that live read; raw RDX=aligned start;
// remaining=outgoing R8. RCX=object and R9 is preserved by actual instructions.
Monstey_GadbManagerTailResult Monstey_GadbInitializeManagerBackendTail40(
    Monstey_GadbBinderMemory, std::uint64_t object, std::uint64_t start,
    std::uint64_t span, Monstey_GadbManagerPlacementCells = {});
Monstey_GadbManagerTailResult Monstey_GadbInitializeManagerBackendTail80(
    Monstey_GadbBinderMemory, std::uint64_t object, std::uint64_t start,
    std::uint64_t span, Monstey_GadbManagerPlacementCells = {});
Monstey_GadbManagerTailResult Monstey_GadbInitializeManagerBackendTailC0(
    Monstey_GadbBinderMemory, std::uint64_t object, std::uint64_t start,
    std::uint64_t span, Monstey_GadbManagerPlacementCells = {});

// 14117357C. body_rsp F=entry RSP-38h. Known W32[F+20h]=5 precedes
// the concrete child at E=F-8. Produces desired_register=00000000FFFFFFFF.
Monstey_GadbManagerCasResult Monstey_GadbCompareExchangeManagerState(
    Monstey_GadbBinderMemory, std::uint64_t target,
    std::uint64_t expected_cell, std::uint64_t body_rsp,
    const Monstey_GadbManagerPlacementServices &,
    Monstey_GadbManagerPlacementCells = {});
// 14059D5B8. Leaf body_rsp=entry RSP. Read DWORD[body_rsp+28h] twice
// on the <=5 path; read the live table only there. Only low U32 of
// desired_register is consumed; the full input value is preserved in result.
Monstey_GadbManagerCasResult Monstey_GadbCompareExchangeManagerWord(
    Monstey_GadbBinderMemory, std::uint64_t target,
    std::uint64_t expected_cell, std::uint64_t desired_register,
    std::uint64_t body_rsp, const Monstey_GadbManagerPlacementServices &,
    Monstey_GadbManagerPlacementCells = {});
} // namespace Monstey
