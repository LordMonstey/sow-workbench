// SoW owned publication contract: six concrete bodies, 463 native bytes.
#pragma once
#include "Monstey_GadbBufferedStream.hpp"
#include "Monstey_FearDbCallable.hpp"
#include <cstdint>
namespace Monstey {
enum class Monstey_GadbPublicationAtomicOperation {
 increment32, fetch_add32
};
struct Monstey_GadbPublicationAtomicCall {
 std::uint64_t native_site;
 std::uint64_t address;
 std::uint32_t addend;
 Monstey_GadbPublicationAtomicOperation operation;
};
struct Monstey_GadbPublicationVirtualCall {
 std::uint64_t native_site;
 std::uint64_t this_identity;
 std::uint64_t vtable_identity;
 std::uint64_t slot_identity;
 std::uint64_t target_identity;
 Monstey_GadbBufferedResult incoming;
};
struct Monstey_GadbPublicationTrapCall {
 std::uint64_t call_site;
 std::uint64_t trap_entry;
 std::uint64_t zero_store_site;
 std::uint64_t message_identity;
 std::uint64_t address;
 std::uint32_t value;
 Monstey_GadbBufferedResult incoming;
};
enum class Monstey_GadbPublicationBoundaryKind {
 missing_atomic32, missing_virtual_call, invariant_trap, trap_returned
};
class Monstey_GadbPublicationBoundaryFault final : public std::runtime_error {
public:
 Monstey_GadbPublicationBoundaryKind kind;
 std::uint64_t native_site, call_site, address, slot, target, message_identity;
 std::uint32_t width;
 Monstey_GadbPublicationBoundaryFault(Monstey_GadbPublicationBoundaryKind,
  std::uint64_t native_site,std::uint64_t call_site,std::uint64_t address,
  std::uint32_t width,std::uint64_t slot,std::uint64_t target,
  std::uint64_t message_identity);
};
struct Monstey_GadbPublicationServices {
 // Return pre-operation U32 and commit exactly the indicated modular update.
 // increment32 ignores the returned U32; fetch_add32 uses it as old EAX.
 Monstey_FearDbCallable<std::uint32_t(
  Monstey_GadbBinderMemory,const Monstey_GadbPublicationAtomicCall&)> atomic32;
 // Only a consumed live vtable slot+0/+8, never an entire release helper.
 Monstey_FearDbCallable<Monstey_GadbBufferedResult(
  Monstey_GadbBinderMemory,const Monstey_GadbPublicationVirtualCall&)> virtual_call;
 // Nonreturning zero-store trap boundary. A normal callback return must cause
 // a typed TrapReturned fault, never continue the native syntactic back-jump.
 Monstey_FearDbCallable<void(
  Monstey_GadbBinderMemory,const Monstey_GadbPublicationTrapCall&)> trap;
};
// Main4BAD00+36cold. Q=entryRSP-38h; Q+20 is an actual16B temporary pair.
// Entry RAX is dead before a call; entry EDX/R8D are exactly header_h8/h9.
// The final CMP reads the invariant and NEVER replaces result.rax.
Monstey_GadbBufferedResult Monstey_GadbPublishDatabaseStorage(
 Monstey_GadbBinderMemory,std::uint64_t database,
 std::uint32_t header_h8,std::uint32_t header_h9,std::uint64_t bank_name,
 std::uint64_t body_rsp,const Monstey_GadbPublicationServices&);
// Complete4BAE88. Explicit source sets incoming RDX at entry; no default raw
// values are fabricated. Return RAX=destination; retain assign helper RDX.
Monstey_GadbBufferedResult Monstey_GadbCopyPublicationPair(
 Monstey_GadbBinderMemory,std::uint64_t destination,std::uint64_t source,
 Monstey_GadbBufferedResult incoming,const Monstey_GadbPublicationServices&);
// Complete761674. Explicit new_data/new_control are native RDX/R8 arguments.
// If old control is null, incoming RAX and new_data RDX remain unchanged.
Monstey_GadbBufferedResult Monstey_GadbAssignPublicationPair(
 Monstey_GadbBinderMemory,std::uint64_t destination,std::uint64_t new_data,
 std::uint64_t new_control,Monstey_GadbBufferedResult incoming,
 const Monstey_GadbPublicationServices&);
// Complete115E04C. Null control returns the incoming RAX/RDX unchanged;
// pair fields are neither cleared nor abstractly moved.
Monstey_GadbBufferedResult Monstey_GadbReleasePublicationPair(
 Monstey_GadbBinderMemory,std::uint64_t pair,
 Monstey_GadbBufferedResult incoming,const Monstey_GadbPublicationServices&);
// Complete3DD6DC/3DD70C. XADD overwrites RAX with zeroextended oldU32,
// preserves RDX, and old==1 alone consumes a live dynamic virtual call.
Monstey_GadbBufferedResult Monstey_GadbReleasePublicationPrimary(
 Monstey_GadbBinderMemory,std::uint64_t control,
 Monstey_GadbBufferedResult incoming,const Monstey_GadbPublicationServices&);
Monstey_GadbBufferedResult Monstey_GadbReleasePublicationSecondary(
 Monstey_GadbBinderMemory,std::uint64_t control,
 Monstey_GadbBufferedResult incoming,const Monstey_GadbPublicationServices&);
} // namespace Monstey
