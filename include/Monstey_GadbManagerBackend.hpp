// Proposal only: twelve actual SoW backend parent bodies, 2767 native bytes.
// Conditional dependency: qualified placement product matching frozen API004.
#pragma once
#include "Monstey_GadbBufferedStream.hpp"
#include "Monstey_GadbManagerPlacement.hpp"
#include "Monstey_FearDbCallable.hpp"
#include <cstdint>
#include <stdexcept>
namespace Monstey {
struct Monstey_GadbManagerBackendImportCells {
  std::uint64_t fill = 0x141D92E50ULL;
  std::uint64_t allocate = 0x141D92278ULL;
  std::uint64_t system_info = 0x141D92290ULL;
  std::uint64_t sleep = 0x141D92268ULL;
  std::uint64_t random = 0x141D93758ULL;
  std::uint64_t initialize_cs = 0x141D92270ULL;
  std::uint64_t enter_cs = 0x141D922C0ULL;
  std::uint64_t leave_cs = 0x141D922B8ULL;
};
struct Monstey_GadbManagerBackendModeCells {
  std::uint64_t cookie, page, granularity, auxiliary, flags;
  std::uint64_t secondary_flags, template_cs, state, main_cs;
};
struct Monstey_GadbManagerBackendBindings {
  Monstey_GadbManagerBackendImportCells imports;
  Monstey_GadbManagerPlacementCells placement;
  Monstey_GadbManagerBackendModeCells mode_40 {
      0x14277D1E8ULL, 0x14277D1F0ULL, 0x14277D1F8ULL,
      0x14277D200ULL, 0x14277D210ULL, 0x14277D590ULL,
      0x14277D598ULL, 0x14277D5F0ULL, 0x142A41508ULL};
  Monstey_GadbManagerBackendModeCells mode_80 {
      0x14277CDD8ULL, 0x14277CDE0ULL, 0x14277CDE8ULL,
      0x14277CDF0ULL, 0x14277CE00ULL, 0x14277D180ULL,
      0x14277D188ULL, 0x14277D1E0ULL, 0x142A414E0ULL};
  Monstey_GadbManagerBackendModeCells mode_c0 {
      0x14277D5F8ULL, 0x14277D600ULL, 0x14277D608ULL,
      0x14277D610ULL, 0x14277D620ULL, 0x14277D9A0ULL,
      0x14277D9A8ULL, 0x14277D5F4ULL, 0x142A41530ULL};
};
// At direct import CALL: native_site=call_site and body_rsp is THIS body.
// At fill tail JMP: native_site=1408C4EE0, call_site is its parent CALL,
// body_rsp is the six-byte thunk body/entry = supplied parent body_rsp-8.
// The last three fields are owned identity/diagnostic context, not CPU args.
struct Monstey_GadbManagerBackendImportSite {
  std::uint64_t native_site, call_site, body_rsp;
  std::uint64_t cell_identity, target_identity;
};
struct Monstey_GadbManagerBackendFillCall {
  Monstey_GadbManagerBackendImportSite site;
  std::uint64_t destination, count;
  std::uint32_t value;
};
struct Monstey_GadbManagerBackendAllocateCall {
  Monstey_GadbManagerBackendImportSite site;
  std::uint64_t address, size;
  std::uint32_t allocation_type, protection;
};
struct Monstey_GadbManagerBackendSystemInfoCall {
  Monstey_GadbManagerBackendImportSite site;
  std::uint64_t output;
  static constexpr std::uint32_t output_bytes = 48;
};
struct Monstey_GadbManagerBackendSleepCall {
  Monstey_GadbManagerBackendImportSite site;
  std::uint32_t milliseconds, alertable;
};
struct Monstey_GadbManagerBackendInitializeCsCall {
  Monstey_GadbManagerBackendImportSite site;
  std::uint64_t critical_section;
  std::uint32_t spin;
};
struct Monstey_GadbManagerBackendSyncCall {
  Monstey_GadbManagerBackendImportSite site;
  std::uint64_t critical_section;
};
struct Monstey_GadbManagerBackendExchange32Call {
  std::uint64_t native_site, address;
  std::uint32_t value;
};
struct Monstey_GadbManagerBackendRdxResult {
  std::uint64_t rdx;
};
enum class Monstey_GadbManagerBackendImport {
  fill, allocate, system_info, sleep, random, initialize_cs, enter_cs, leave_cs
};
class Monstey_GadbManagerBackendImportFault final : public std::runtime_error {
public:
  Monstey_GadbManagerBackendImport kind;
  Monstey_GadbManagerBackendImportSite site;
  Monstey_GadbManagerBackendImportFault(
      Monstey_GadbManagerBackendImport,
      const Monstey_GadbManagerBackendImportSite &);
};
class Monstey_GadbManagerBackendExchange32Fault final
    : public std::runtime_error {
public:
  static constexpr std::uint32_t width = 4;
  std::uint64_t native_site, address;
  std::uint32_t value;
  Monstey_GadbManagerBackendExchange32Fault(
      std::uint64_t native_site, std::uint64_t address, std::uint32_t value);
};
struct Monstey_GadbManagerBackendServices {
  // Fill's returned RAX is dead. Its real RDX survives a precise early
  // factory rejection, so the provider supplies that word explicitly.
  Monstey_FearDbCallable<Monstey_GadbManagerBackendRdxResult(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendFillCall &)>
      fill;
  // Allocation RAX is the pointer. On rejection, its RDX is returned raw.
  Monstey_FearDbCallable<Monstey_GadbBufferedResult(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendAllocateCall &)>
      allocate;
  // Concrete code owns the later live granularity/page reads and all globals.
  Monstey_FearDbCallable<void(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendSystemInfoCall &)>
      system_info;
  Monstey_FearDbCallable<void(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendSleepCall &)>
      sleep;
  // Exact EAX32 bit pattern only; native cookie arithmetic sign-extends it.
  Monstey_FearDbCallable<std::uint32_t(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendImportSite &)>
      random;
  // BOOL is ignored. RDX is supplied for the REAL frozen Reset incoming_rdx
  // argument in placement; main/template sites discard this dead word.
  Monstey_FearDbCallable<Monstey_GadbManagerBackendRdxResult(
      Monstey_GadbBinderMemory,
      const Monstey_GadbManagerBackendInitializeCsCall &)> initialize_cs;
  // These are the native direct imports, never guard114BA90/17DE90 adapters.
  Monstey_FearDbCallable<void(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendSyncCall &)>
      enter_cs;
  // All initializer exits execute MOV EAX,1 AFTER this returned RDX.
  Monstey_FearDbCallable<Monstey_GadbManagerBackendRdxResult(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendSyncCall &)>
      leave_cs;
  // Only consumed XCHG32[state],1. Return observed old U32 and commit1 once.
  Monstey_FearDbCallable<std::uint32_t(
      Monstey_GadbBinderMemory,
      const Monstey_GadbManagerBackendExchange32Call &)> exchange32;
  // Actual concrete wrapper/leaf CAS dependency, not a whole-initializer hook.
  Monstey_GadbManagerPlacementServices placement;
};
// Exclusively borrowed stable numeric image. Bindings are fixed identities
// copied by value per operation; native defaults or explicit owned remapping.
// Tail globals are shared with bindings.placement.tail_40/tail_80/tail_c0,
// never duplicated identities. Contents/cells are read LIVE when consumed.
// Services is a const reference; invoke each current Callable field directly.
// No service/range/null/alignment/disjointness/arithmetic preflight or rollback.
// No original OS/DLL execution, physical CPU/LOCK/concurrency ABI is promised.

// 141175F88 / 141174DBC / 141177ECC. B=entryRSP-3F8h.
// Low32(flags_register) is cached EDX; full entry RDX is preserved on size
// rejection without init. Incoming RCX/RAX are dead to this owned projection.
Monstey_GadbBufferedResult Monstey_GadbCreateManagerBackend40(
    Monstey_GadbBinderMemory, std::uint64_t flags_register,
    std::uint64_t backing, std::uint64_t byte_count, std::uint64_t body_rsp,
    const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});
Monstey_GadbBufferedResult Monstey_GadbCreateManagerBackend80(
    Monstey_GadbBinderMemory, std::uint64_t flags_register,
    std::uint64_t backing, std::uint64_t byte_count, std::uint64_t body_rsp,
    const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});
Monstey_GadbBufferedResult Monstey_GadbCreateManagerBackendC0(
    Monstey_GadbBinderMemory, std::uint64_t flags_register,
    std::uint64_t backing, std::uint64_t byte_count, std::uint64_t body_rsp,
    const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});

// 141176058 / 141174EA8 / 141177FB8. F=entryRSP-28h.
// Descriptor is native R8. Explicit incoming_rdx survives page rejection
// when init is skipped; entry RAX/RCX are dead before any own use.
Monstey_GadbBufferedResult Monstey_GadbAllocateManagerBackend40(
    Monstey_GadbBinderMemory, std::uint64_t descriptor, std::uint64_t body_rsp,
    std::uint64_t incoming_rdx, const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});
Monstey_GadbBufferedResult Monstey_GadbAllocateManagerBackend80(
    Monstey_GadbBinderMemory, std::uint64_t descriptor, std::uint64_t body_rsp,
    std::uint64_t incoming_rdx, const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});
Monstey_GadbBufferedResult Monstey_GadbAllocateManagerBackendC0(
    Monstey_GadbBinderMemory, std::uint64_t descriptor, std::uint64_t body_rsp,
    std::uint64_t incoming_rdx, const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});

// 1411755F0 / 141173A38 / 141176B48. I=entryRSP-58h.
// Real W32[I+60], GetSystemInfo[I+20..I+4F], concrete CAS atI-40,
// native state loop/XCHG/global stores. Every normal return is RAX1,
// RDX=the final direct Leave provider word; no inherited raw default.
Monstey_GadbBufferedResult Monstey_GadbInitializeManagerBackend40(
    Monstey_GadbBinderMemory, std::uint64_t body_rsp,
    const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});
Monstey_GadbBufferedResult Monstey_GadbInitializeManagerBackend80(
    Monstey_GadbBinderMemory, std::uint64_t body_rsp,
    const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});
Monstey_GadbBufferedResult Monstey_GadbInitializeManagerBackendC0(
    Monstey_GadbBinderMemory, std::uint64_t body_rsp,
    const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});

// 141175718 / 141173BA0 / 141176CB0. H=entryRSP-28h.
// Concrete fill/CS/header/object setup, real reset, LIVE header read, real tail.
// Final MOV RAX,O overrides only tail raw.rax; keep q RDX and remaining R8.
Monstey_GadbManagerTailResult Monstey_GadbPlaceManagerBackend40(
    Monstey_GadbBinderMemory, std::uint64_t backing, std::uint64_t span,
    std::uint64_t body_rsp, const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});
Monstey_GadbManagerTailResult Monstey_GadbPlaceManagerBackend80(
    Monstey_GadbBinderMemory, std::uint64_t backing, std::uint64_t span,
    std::uint64_t body_rsp, const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});
Monstey_GadbManagerTailResult Monstey_GadbPlaceManagerBackendC0(
    Monstey_GadbBinderMemory, std::uint64_t backing, std::uint64_t span,
    std::uint64_t body_rsp, const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});
} // namespace Monstey
