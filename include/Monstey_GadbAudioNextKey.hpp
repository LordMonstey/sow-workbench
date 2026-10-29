#pragma once
#include "Monstey_GadbScalarFloat.hpp"
#include <cstdint>
namespace Monstey {
// Complete owned-memory translation of140D17C80. action_wrapper points to a
// Qword record identity. action_class and nextkey_class point to Class64
// descriptors (native identities14264C660/14264C4E0); their Q0 fields tables
// provide declaration20 and declaration0 respectively. Embedded addresses are
// numeric identities in the exclusively borrowed arena, never invoked.
//
// Both cached and slow direct/indirect/generated stages are implemented. The
// first raw word is consumed as the child record; the final raw Qword is returned
// without resolving, dereferencing or interpreting it. Normal paths write no
// bytes. Unavailable consumed ranges raise BinderFault; the native deliberate
// null-write assertion for an empty dynamic array raises ScalarFault instead.
// Concurrent mutation/native faults/allocator/runtime ABI are outside this API.
std::uint64_t Monstey_GadbReadAudioNextKey(
    Monstey_GadbBinderMemory memory, std::uint64_t action_wrapper,
    std::uint64_t action_class, std::uint64_t nextkey_class);
} // namespace Monstey
