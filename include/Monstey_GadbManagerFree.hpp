// Proposal only: 22 concrete FREE bodies (7179B) plus reused literal trap27B.
#pragma once
#include "Monstey_GadbManagerBackend.hpp"
#include "Monstey_GadbManagerPlacement.hpp"
#include "Monstey_GadbBufferedStream.hpp"
#include "Monstey_FearDbCallable.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <variant>
namespace Monstey {
struct Monstey_GadbManagerFreeBindings {
  // One canonical nested binding. All free and BK imports/mode globals and
  // M2 tails/CAS use these SAME identities under an explicit owned remapping.
  Monstey_GadbManagerBackendBindings backend;
  std::uint64_t query_cell = 0x141D92260ULL;
  std::uint64_t release_cell = 0x141D92280ULL;
};
struct Monstey_GadbManagerFreeContext {
  std::uint64_t body_rsp;       // Dispatcher body = entry RSP - 28h.
  std::uint64_t incoming_rax;   // Required: preserved on payload==0.
  std::uint64_t incoming_r9;    // Required: consumed by mode0 trap home store.
};
struct Monstey_GadbManagerFreeEntry {
  std::uint64_t entry_rsp;      // A thunk has no additional frame.
  std::uint64_t incoming_rax;
};
struct Monstey_GadbManagerFreeBody {
  std::uint64_t body_rsp;       // Backend body = entry RSP - 28h.
  std::uint64_t incoming_rax;   // Direct payload0 return, before frame data use.
};
enum class Monstey_GadbManagerFreeSyncOperation { enter, leave };
struct Monstey_GadbManagerFreeSyncCall {
  Monstey_GadbManagerFreeSyncOperation operation;
  Monstey_GadbManagerBackendImportSite site;
  std::uint64_t critical_section;
};
struct Monstey_GadbManagerFreeQueryCall {
  Monstey_GadbManagerBackendImportSite site;
  std::uint64_t address, output, length;
};
struct Monstey_GadbManagerFreeReleaseCall {
  Monstey_GadbManagerBackendImportSite site;
  std::uint64_t address, size;
  std::uint32_t free_type;
};
struct Monstey_GadbManagerFreeBoolRdxResult {
  std::uint32_t eax;            // Exact BOOL32 pattern; consumed as !=0.
  std::uint64_t rdx;            // Real outgoing RDX; no VOID preservation rule.
};
enum class Monstey_GadbManagerFreeImport {
  query, release, enter_cs, leave_cs, system_info, sleep, random, initialize_cs
};
using Monstey_GadbManagerFreeImportArguments = std::variant<
    Monstey_GadbManagerFreeQueryCall,
    Monstey_GadbManagerFreeReleaseCall,
    Monstey_GadbManagerFreeSyncCall,
    Monstey_GadbManagerBackendSystemInfoCall,
    Monstey_GadbManagerBackendSleepCall,
    Monstey_GadbManagerBackendImportSite,
    Monstey_GadbManagerBackendInitializeCsCall>;
class Monstey_GadbManagerFreeImportFault final : public std::runtime_error {
public:
  static constexpr std::size_t slot_width = 8;
  Monstey_GadbManagerFreeImport kind;
  Monstey_GadbManagerBackendImportSite site;
  Monstey_GadbManagerFreeImportArguments arguments;
  Monstey_GadbManagerFreeImportFault(
      Monstey_GadbManagerFreeImport,
      const Monstey_GadbManagerBackendImportSite &,
      Monstey_GadbManagerFreeImportArguments);
};
// Logical diagnostic at the consumed unsigned DIV opcode only. EDX was
// explicitly zeroed by known code: high dividend=0 is a fact, not CPU default.
// No host divide-by-zero/Win64 exception ABI or preflight is claimed.
class Monstey_GadbManagerFreeDivideFault final : public std::runtime_error {
public:
  static constexpr std::uint64_t dividend_high = 0;
  std::uint64_t native_site, body_rsp, dividend_low, divisor;
  Monstey_GadbManagerFreeDivideFault(
      std::uint64_t native_site, std::uint64_t body_rsp,
      std::uint64_t dividend_low, std::uint64_t divisor);
};
struct Monstey_GadbManagerFreeServices {
  // SIZE_T64 result plus the actually surviving RDX. Query's 48-byte output
  // is borrowed frame memory and is never silently initialized or completed.
  Monstey_FearDbCallable<Monstey_GadbBufferedResult(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerFreeQueryCall &)> query;
  Monstey_FearDbCallable<Monstey_GadbManagerFreeBoolRdxResult(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerFreeReleaseCall &)> release;
  // One current shared target for all Free/BK Enter/Leave sites. Enter's
  // returned RAX is dead; never fabricate a replacement word in an adapter.
  // Free consumes both Leave words; BK init keeps RDX then forces EAX1.
  Monstey_FearDbCallable<Monstey_GadbBufferedResult(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerFreeSyncCall &)> sync;
  // Exact consumed subset needed by the THREE REAL BK initializer functions.
  Monstey_FearDbCallable<void(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendSystemInfoCall &)> system_info;
  Monstey_FearDbCallable<void(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendSleepCall &)> sleep;
  Monstey_FearDbCallable<std::uint32_t(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendImportSite &)> random;
  // Existing BK signature retained; returned RDX is dead at init's main and
  // template sites. No invented replacement word is passed into its library.
  Monstey_FearDbCallable<Monstey_GadbManagerBackendRdxResult(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendInitializeCsCall &)> initialize_cs;
  Monstey_FearDbCallable<std::uint32_t(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendExchange32Call &)> exchange32;
  Monstey_GadbManagerPlacementServices placement;
};
// All identities refer to one stable exclusively borrowed numeric image.
// Binding identities are copied once; their bytes/targets are sampled LIVE.
// No service, address, alignment, null, overlap or ownership preflight.
// Current shared Callable targets are invoked at each consumed call. BK init
// adapters delegate to these CURRENT fields, never copied provider snapshots.
// Saved CPU spills/home/return-address ABI effects are outside this owned
// profile, as in accepted BK. Explicit Query locals and trap stores ARE owned.
// Raw result is RAX/RDX only; omitted physical R8/R9 are not defaulted to zero.
Monstey_GadbBufferedResult Monstey_GadbFreeManager(
    Monstey_GadbBinderMemory, std::uint64_t manager, std::uint64_t payload,
    const Monstey_GadbManagerFreeContext &,
    const Monstey_GadbManagerFreeServices &,
    Monstey_GadbManagerFreeBindings = {});

Monstey_GadbBufferedResult Monstey_GadbFreeManagerBackend40Thunk(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t payload,
    const Monstey_GadbManagerFreeEntry &,
    const Monstey_GadbManagerFreeServices &, Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbFreeManagerBackend40(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t payload,
    const Monstey_GadbManagerFreeBody &,
    const Monstey_GadbManagerFreeServices &, Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbUnmapManagerRegion40(
    Monstey_GadbBinderMemory, std::uint64_t address, std::uint64_t length,
    std::uint64_t body_rsp, const Monstey_GadbManagerFreeServices &,
    Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbTrimManagerBackend40(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t body_rsp,
    std::uint64_t incoming_rdx, const Monstey_GadbManagerFreeServices &,
    Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbReleaseManagerSegments40(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t body_rsp,
    std::uint64_t incoming_rdx, const Monstey_GadbManagerFreeServices &,
    Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbFindManagerSegment40(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t address);
Monstey_GadbBufferedResult Monstey_GadbHasManagerSegmentLink40(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t segment);

Monstey_GadbBufferedResult Monstey_GadbFreeManagerBackend80Thunk(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t payload,
    const Monstey_GadbManagerFreeEntry &,
    const Monstey_GadbManagerFreeServices &, Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbFreeManagerBackend80(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t payload,
    const Monstey_GadbManagerFreeBody &,
    const Monstey_GadbManagerFreeServices &, Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbUnmapManagerRegion80(
    Monstey_GadbBinderMemory, std::uint64_t address, std::uint64_t length,
    std::uint64_t body_rsp, const Monstey_GadbManagerFreeServices &,
    Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbTrimManagerBackend80(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t body_rsp,
    std::uint64_t incoming_rdx, const Monstey_GadbManagerFreeServices &,
    Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbReleaseManagerSegments80(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t body_rsp,
    std::uint64_t incoming_rdx, const Monstey_GadbManagerFreeServices &,
    Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbFindManagerSegment80(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t address);
Monstey_GadbBufferedResult Monstey_GadbHasManagerSegmentLink80(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t segment);

Monstey_GadbBufferedResult Monstey_GadbFreeManagerBackendC0Thunk(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t payload,
    const Monstey_GadbManagerFreeEntry &,
    const Monstey_GadbManagerFreeServices &, Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbFreeManagerBackendC0(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t payload,
    const Monstey_GadbManagerFreeBody &,
    const Monstey_GadbManagerFreeServices &, Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbUnmapManagerRegionC0(
    Monstey_GadbBinderMemory, std::uint64_t address, std::uint64_t length,
    std::uint64_t body_rsp, const Monstey_GadbManagerFreeServices &,
    Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbTrimManagerBackendC0(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t body_rsp,
    std::uint64_t incoming_rdx, const Monstey_GadbManagerFreeServices &,
    Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbReleaseManagerSegmentsC0(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t body_rsp,
    std::uint64_t incoming_rdx, const Monstey_GadbManagerFreeServices &,
    Monstey_GadbManagerFreeBindings = {});
Monstey_GadbBufferedResult Monstey_GadbFindManagerSegmentC0(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t address);
Monstey_GadbBufferedResult Monstey_GadbHasManagerSegmentLinkC0(
    Monstey_GadbBinderMemory, std::uint64_t backend, std::uint64_t segment);

// Actual 819104, 27 reused native bytes. Three ordered homes then DIRECT
// W32[0]=0 (not RMW). If all stores return, preserve incoming Raw exactly.
// No whole-trap provider, message dereference or forced noreturn is added.
Monstey_GadbBufferedResult Monstey_GadbManagerFreeTrapReturn(
    Monstey_GadbBinderMemory, std::uint64_t entry_rsp,
    Monstey_GadbBufferedResult incoming, std::uint64_t incoming_r8,
    std::uint64_t incoming_r9);
} // namespace Monstey
