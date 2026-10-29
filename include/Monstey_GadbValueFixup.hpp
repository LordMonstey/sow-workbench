#pragma once
#include "Monstey_GadbBinderComposition.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
namespace Monstey {
class Monstey_GadbValueFixupFault final : public std::runtime_error {
public:
 std::uint64_t address;
 std::size_t width;
 bool writing;
 Monstey_GadbValueFixupFault(std::uint64_t,std::size_t,bool);
};
// Complete423E70, using the exact immutable256-byte map qualified in both PE
// profiles. Reads the borrowed C string byte-by-byte through its first NUL.
// No slash normalization, text decoding or host C-string dereference occurs.
std::uint32_t Monstey_GadbHashCString(Monstey_GadbBinderMemory memory,
 std::uint64_t string_identity,std::uint32_t seed=0);
// Complete4BAF6C +cold1410ED846. Values are already-relocated runtime Qwords;
// this does not implement serialized-pool relocation or allocate a database.
// Descriptor+0A is type/flags; +0C countU16; +0E first-wordU16. Record+18 links
// category, category+18 links database. Raw ranges intentionally may alias.
// Writes and live count reloads follow native order. The returned rawRAX is
// retained even where callers ignore it: no-op type, zero-count type5, backlink
// identity on zero-count references, or the last reloaded count after a loop.
// Only supplied owned memory is written. Consumed-range failure retains earlier
// stores; original fault/service ABI, allocator and real load history are outside
// this component. Backing is stable and exclusively borrowed during the call.
std::uint64_t Monstey_GadbFixupValueDescriptor(Monstey_GadbBinderMemory memory,
 std::uint64_t descriptor,std::uint64_t record,std::uint64_t value_words);
} // namespace Monstey
