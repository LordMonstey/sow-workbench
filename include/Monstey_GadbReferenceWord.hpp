#pragma once
#include "Monstey_GadbRecordLookup.hpp"
#include <cstdint>
namespace Monstey {
// Owned-memory translation of 16CE2E8. The fallback is a borrowed address,
// consumed lazily as a full QWORD only on the native fallback paths.
// A returned word is not dereferenced, resolved, validated as a link, or called.
// Input memory must remain exclusively borrowed and stable for the call.
std::uint64_t Monstey_GadbReadGeneratedReferenceWord(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field, std::uint32_t index, std::uint64_t defaultAddress);
// Complete 68BB08, including cold14111F3E4. field is raw72+32, not a row start.
// Lookup uses the real low-six-bit type10 and unsigned descriptor count.
// Raw and generated zero words are successful values, not missing records.
// This interface omits the native unused third argument; it is not an ABI shim.
std::uint64_t Monstey_GadbReadReferenceWord(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field, std::uint32_t index, std::uint64_t defaultAddress);
} // namespace Monstey
