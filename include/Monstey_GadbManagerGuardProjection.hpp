// ADDITIVE API DRAFT ONLY. No product/reference implementation or compilation.
// Existing Monstey_GadbManagerPrimitives.hpp and its six symbols are unchanged.
#pragma once
#include "Monstey_GadbManagerPrimitives.hpp"

namespace Monstey {
// The actual Enter primitive call. base is the existing M1 DTO, always role
// enter/site 0x14114BA9F. base.incoming.rdx is lock after the own RDX assignment;
// incoming_r8/r9 are the values forwarded by the known guard before Enter.
struct Monstey_GadbManagerEnterProjectionCall {
  Monstey_GadbManagerSyncCall base;
  std::uint64_t incoming_r8;
  std::uint64_t incoming_r9;
};

// Exactly the primitive normal-return words consumed by the new continuation.
// Enter RAX is omitted: own MOV RAX,RBX at 0x14114BAA5 overwrites it with guard.
// These words are explicit declared primitive effects, never implicit zeros,
// preservation promises, or an observation of physical host registers.
struct Monstey_GadbManagerEnterProjectionResult {
  std::uint64_t rdx;
  std::uint64_t r8;
  std::uint64_t r9;
};

struct Monstey_GadbManagerEnterProjectionServices {
  Monstey_FearDbCallable<Monstey_GadbManagerEnterProjectionResult(
      Monstey_GadbBinderMemory,
      const Monstey_GadbManagerEnterProjectionCall&)> enter;
};

// Normal guard result: raw.rax=guard is produced by the known own instruction;
// raw.rdx and the two continuation words are the actual declared Enter outputs.
struct Monstey_GadbManagerGuardProjectedResult {
  Monstey_GadbBufferedResult raw;
  std::uint64_t post_enter_r8;
  std::uint64_t post_enter_r9;
};

// Additive concrete 0x14114BA90 variant, not a whole-guard provider.
// Preserve the accepted own order: RDX=lock; W64[guard]=lock; live Enter-cell
// read; actual current enter field; then own RAX=guard on normal return.
// Use existing M1 ordinary/boundary faults at the same consumed instruction.
// No preflight, disjointness/null guard, trap service, or rollback is added.
Monstey_GadbManagerGuardProjectedResult Monstey_GadbConstructManagerGuardProjected(
    Monstey_GadbBinderMemory,
    std::uint64_t guard, std::uint64_t lock,
    Monstey_GadbBufferedResult incoming,
    std::uint64_t incoming_r8, std::uint64_t incoming_r9,
    const Monstey_GadbManagerPrimitiveBindings&,
    const Monstey_GadbManagerEnterProjectionServices&);
} // namespace Monstey
