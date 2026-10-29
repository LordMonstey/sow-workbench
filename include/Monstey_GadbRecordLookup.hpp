#pragma once
#include "Monstey_GadbBinderComposition.hpp"
namespace Monstey {
// Exact owned-memory translation of 25180C -> 251BD0. `field` is the
// declaration's hash/cache interior (raw72 field row +32), not its row start.
// All addresses are unsigned virtual identities; no original code is invoked.
// Record byte+0 is the live U8 count. Flagbyte+1 bit0 selects direct hash-at0
// rows; otherwise sorted hash-at4 rows return the U8+9 remapped16-byte row.
// A returned descriptor address is not eagerly dereferenced or range-checked:
// native lookup can return an unreadable remapped row to a later consumer.
// Only actually consumed ranges fault, using Monstey_GadbBinderFault.
std::uint64_t Monstey_GadbFindRecordDescriptor(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field);
} // namespace Monstey
