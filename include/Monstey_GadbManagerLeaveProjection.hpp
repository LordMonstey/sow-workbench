// ADDITIVE API PROPOSAL ONLY. No product/reference implementation or build.
// Existing M1 types, six symbols, exports and JSON contracts stay unchanged.
#pragma once
#include "Monstey_GadbManagerPrimitives.hpp"

namespace Monstey {
// Exactly the declared normal Leave outputs passed through both known leaves.
// Raw RAX is retained: neither 17DE90 nor 114BA88 overwrites it.
// Full64 R8/R9 are the new outputs consumed by 1172728@1172800.
struct Monstey_GadbManagerLeaveProjectionResult {
  Monstey_GadbBufferedResult raw;
  std::uint64_t post_leave_r8;
  std::uint64_t post_leave_r9;
};

struct Monstey_GadbManagerLeaveProjectionServices {
  // Existing M1 DTO: role leave/site 0x14114BA88, live cell/target, actual
  // lock, and unchanged incoming Raw2. No incoming.rdx=lock assignment.
  // This output projection does not expose forwarded incoming R8/R9 or
  // promise arbitrary physical-register dependence of the OS primitive.
  // Invoke this current field directly after the actual live-cell read.
  Monstey_FearDbCallable<Monstey_GadbManagerLeaveProjectionResult(
      Monstey_GadbBinderMemory, const Monstey_GadbManagerSyncCall&)> leave;
};

// Concrete 7B [14114BA88,14114BA8F), excluding the following CC padding.
// Read live bindings.leave_cell, then current leave field. Pass every declared
// output unchanged. Target0 is allowed. Reuse exact existing M1 ordinary and
// missing_sync_service faults; no preflight, register defaults or rollback.
Monstey_GadbManagerLeaveProjectionResult
Monstey_GadbLeaveManagerCriticalSectionProjected(
    Monstey_GadbBinderMemory, std::uint64_t lock,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbManagerPrimitiveBindings&,
    const Monstey_GadbManagerLeaveProjectionServices&);

// Concrete 8B [14017DE90,14017DE98): read current Qword[guard] first,
// then the concrete projected Leave forwarder in the SAME native tail frame.
// No guard clear/free/store or own post-primitive instruction is invented.
Monstey_GadbManagerLeaveProjectionResult Monstey_GadbDestroyManagerGuardProjected(
    Monstey_GadbBinderMemory, std::uint64_t guard,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbManagerPrimitiveBindings&,
    const Monstey_GadbManagerLeaveProjectionServices&);
} // namespace Monstey
