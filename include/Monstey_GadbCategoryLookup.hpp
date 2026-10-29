#pragma once
#include "Monstey_GadbBufferedStream.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
namespace Monstey {
class Monstey_GadbCategoryLookupFault final : public std::runtime_error {
public:
 std::uint64_t address;
 std::size_t width;
 Monstey_GadbCategoryLookupFault(std::uint64_t, std::size_t);
};
// Concrete logical read-only translation of14115B4D4 (88B). The exclusively
// borrowed owned view is stable for this operation. Addresses are numeric
// identities, never host callbacks. Object+48 is U32 count; object+50 is array;
// rows are58h bytes, hash at+50h; hash_pointer is a live U32 key identity.
// No sortedness, uniqueness, lower-bound or schema assertion is added. Native
// U32 count/bounds/midpoint arithmetic and signed bounds are retained. Raw RDX
// is input hash_pointer when no iteration runs, otherwise last signed64 row
// offset. Raw RAX is matching row identity or0. Faults preserve the image.
// Physical prologue savedRBX/return storage is outside this logical profile.
Monstey_GadbBufferedResult Monstey_GadbFindCategoryByHash(
 Monstey_GadbBinderMemory, std::uint64_t object, std::uint64_t hash_pointer);
} // namespace Monstey
