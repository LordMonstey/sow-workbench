#pragma once
#include "Monstey_GadbRecordLookup.hpp"
namespace Monstey {
// 16CE290 with the caller's default Qword already copied into private owned
// storage. Returns the raw string pointer identity, without reading its bytes.
// Generated storage is consumed through the concrete5DA158 translation.
std::uint64_t Monstey_GadbReadGeneratedStringPointer(
    Monstey_GadbBinderMemory, std::uint64_t record, std::uint64_t field,
    std::uint32_t index, std::uint64_t defaultValue);
// Exact115A54C result: scalar type4, U16 bounds, raw8-byte value stride or
// generated record route. `field` is raw72+32; `defaultValue` is an address
// identity supplied by value, not a pointer to an arena-held default word.
// No string bytes, unrelated returned addresses or unconsumed rows are read.
std::uint64_t Monstey_GadbReadStringPointer(
    Monstey_GadbBinderMemory, std::uint64_t record, std::uint64_t field,
    std::uint32_t index, std::uint64_t defaultValue);
} // namespace Monstey
