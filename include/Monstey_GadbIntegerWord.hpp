#pragma once
#include "Monstey_GadbRecordLookup.hpp"
#include <cstdint>
namespace Monstey {
// Raw EAX payload, not a signed conversion or a Boolean. These functions read
// exclusively borrowed, stable owned memory; no original code is executed.
// defaultAddress is a lazily consumed address of FOUR bytes, not a value.
// A returned zero is valid. field is the declaration's raw72+32 interior.
std::uint32_t Monstey_GadbReadIntegerWord(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field, std::uint32_t index, std::uint64_t defaultAddress);
// 176CEEC. On success the accepted5DA158 helper consumes an entire QWORD,
// then this wrapper returns its low32. Fallback consumes only a DWORD.
std::uint32_t Monstey_GadbReadGeneratedIntegerWord(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field, std::uint32_t index, std::uint64_t defaultAddress);
// Complete311E8C leaf. The physical byte is supplied by the caller; the leaf
// checks count before offset/value and does not inspect type or record flags.
std::uint32_t Monstey_GadbReadCachedIntegerWord(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint8_t physical_cache, std::uint32_t index,
    std::uint64_t defaultAddress);
} // namespace Monstey
