// API proposal only. No product implementation before independent reference freeze.
#pragma once
#include "Monstey_GadbBufferedStream.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
namespace Monstey {
struct Monstey_GadbManagerPrimitiveBindings {
  std::optional<std::uint64_t> gs58_cell_identity;
  std::uint64_t enter_cell = 0x141D922C0ULL;
  std::uint64_t leave_cell = 0x141D922B8ULL;
};
enum class Monstey_GadbManagerSyncOperation { enter, leave };
struct Monstey_GadbManagerSyncCall {
  Monstey_GadbManagerSyncOperation operation;
  std::uint64_t native_site, slot_identity, target_identity;
  std::uint64_t critical_section;
  Monstey_GadbBufferedResult incoming;
};
struct Monstey_GadbManagerPrimitiveServices {
  // One actual import primitive at the consumed instruction. VOID WINAPI does
  // not define raw RAX/RDX; handler returns both explicitly. Target may be zero.
  Monstey_FearDbCallable<Monstey_GadbBufferedResult(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerSyncCall&)> sync;
};
enum class Monstey_GadbManagerPrimitiveBoundaryKind {
  missing_gs58_binding, missing_sync_service
};
class Monstey_GadbManagerPrimitiveBoundaryFault final : public std::runtime_error {
public:
  Monstey_GadbManagerPrimitiveBoundaryKind kind;
  std::uint64_t native_site, address;
  std::size_t width;
  std::uint64_t slot_identity, target_identity, argument;
  Monstey_GadbManagerPrimitiveBoundaryFault(
      Monstey_GadbManagerPrimitiveBoundaryKind, std::uint64_t native_site,
      std::uint64_t address, std::size_t width, std::uint64_t slot_identity,
      std::uint64_t target_identity, std::uint64_t argument);
};
// 114BA90: store lock in guard before reading Enter cell; normal RAX=guard.
Monstey_GadbBufferedResult Monstey_GadbConstructManagerGuard(
    Monstey_GadbBinderMemory, std::uint64_t guard, std::uint64_t lock,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbManagerPrimitiveBindings&,
    const Monstey_GadbManagerPrimitiveServices&);
// 114BA88: indirect tail through live Leave cell, no own raw-state change.
Monstey_GadbBufferedResult Monstey_GadbLeaveManagerCriticalSection(
    Monstey_GadbBinderMemory, std::uint64_t lock,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbManagerPrimitiveBindings&,
    const Monstey_GadbManagerPrimitiveServices&);
// 17DE90: reread live Qword[guard], then real leave forwarder; no clear/free.
Monstey_GadbBufferedResult Monstey_GadbDestroyManagerGuard(
    Monstey_GadbBinderMemory, std::uint64_t guard,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbManagerPrimitiveBindings&,
    const Monstey_GadbManagerPrimitiveServices&);
// 6AAFDC: RAX=TLSarray, RDX=28h, INC DWORD[TLSbase+2Ch] modulo32.
Monstey_GadbBufferedResult Monstey_GadbIncrementManagerTls(
    Monstey_GadbBinderMemory, const Monstey_GadbManagerPrimitiveBindings&);
// 6AACFC: RAX=zext32(decremented count), RDX=28h; zero tails11224A4.
Monstey_GadbBufferedResult Monstey_GadbDecrementManagerTls(
    Monstey_GadbBinderMemory, const Monstey_GadbManagerPrimitiveBindings&);
// 11224A4: literal read/AND0/write DWORD[0], RET preserves incoming raw state.
// If numeric0 is in the owned image it is a real modeled RMW; otherwise the
// first ordinary read faults. No forced null guard/noreturn service is added.
Monstey_GadbBufferedResult Monstey_GadbManagerNullAndReturn(
    Monstey_GadbBinderMemory, Monstey_GadbBufferedResult incoming);
} // namespace Monstey
