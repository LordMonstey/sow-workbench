#pragma once
#include "Monstey_FearDbPrimitives.hpp"
#include <cstdint>
#include "Monstey_FearDbCallable.hpp"
#include <stdexcept>
namespace Monstey {
struct Monstey_FearConstraintCall {
 std::uint32_t target, object;
};
struct Monstey_FearConstraintReadCall {
 std::uint32_t target, stream, destination, bytes;
};
class Monstey_FearConstraintBoundaryFault : public std::runtime_error {
public:
 explicit Monstey_FearConstraintBoundaryFault(const char* boundary);
};
// Supplied call effects; only consumed callbacks are required. Targets and object
// identities are native32 numbers into caller-owned storage, never executable pointers.
// Copies retain the configured target identity/state; invocation retains an active
// target through self-replacement/reset. No mutable closure is cloned per call.
// Services may mutate/replace the owned memory view at call boundaries. No borrowed
// byte span survives a call. Original x86/STL/allocator/exception ABI is not inferred.
struct Monstey_FearConstraintServices {
 Monstey_FearDbCallable<std::uint64_t(Monstey_FearConstraintCall)> get_length;
 Monstey_FearDbCallable<std::uint64_t(Monstey_FearConstraintCall)> get_position;
 Monstey_FearDbCallable<std::uint32_t(Monstey_FearConstraintReadCall)> read;
 Monstey_FearDbCallable<std::uint32_t(std::uint32_t)> new_array;
 Monstey_FearDbCallable<void(std::uint32_t)> delete_array;
 Monstey_FearDbCallable<void(Monstey_FearConstraintCall)> string_default;
 Monstey_FearDbCallable<void(Monstey_FearConstraintCall,std::uint32_t)> string_assign_cstr;
 Monstey_FearDbCallable<void(Monstey_FearConstraintCall)> string_destroy;
 Monstey_FearDbCallable<std::uint32_t(std::uint32_t,std::uint32_t)> map_value;
};
// Complete10006020..100061A0 protocol. Frame is S after sub34/push4-registers.
// Consumed initial slots:S+48(stream identity),S+4C(map identity). Native locals:
// +10name length,+14second word,+18third word,+1Cdiscarded first word,
// +20/+24cached length,+28string object. Type byte reuses incoming slot+48;
// the original stream identity stays cached. Other frame bytes are not initialized.
// Fixed preferred-profile IAT slots1000B064/B068/B074 are read at each call site.
// Return qualifies the declared bool/AL, not the unspecified upper EAX or ABI frame.
// Original outgoing push/prologue effects and external callee internals are outside
// this owned protocol; no physical stack/host-pointer equivalence is claimed.
bool Monstey_FearReadConstraints(Monstey_FearDbMutableMemory& memory,
 std::uint32_t frame,Monstey_FearConstraintServices& services);
} // namespace Monstey
