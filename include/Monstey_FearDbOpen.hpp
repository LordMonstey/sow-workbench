#pragma once
#include "Monstey_FearDbResources.hpp"
#include "Monstey_FearDbNames.hpp"
namespace Monstey {
//6A50: filename_memory owns the borrowed filename bytes; resources owns live DB
//allocations/cache. Comparator is the same explicit MSVCR71!_stricmp boundary,
//required only for a visited nonnull name. No locale/default is substituted.
//The callee samples one shared target after the nonempty-cache gate at6A6D.
//That target and its mutable state remain fixed/alive through the entire lookup,
//even if the caller replaces or clears its holder during a comparison. A null
//sampled target faults only when a nonnull candidate is actually compared.
//Filename/cache backing remains stable during the call. Callbacks may mutate
//bytes, cache entries and its live count within capacity; each later native read
//observes them. Consumed stale backing/out-of-owned-cache entries are errors,
//not a fabricated cache miss. Arbitrary overrides/CRT/fault ABI are unqualified.
std::uint32_t Monstey_FearDbOpenExisting(Monstey_FearDbResources&,Monstey_FearDbMemory filename_memory,std::uint32_t filename,const Monstey_FearDbNameCompare&);
//6DE0 default-reader composition: existing lookup first, then filename null/empty
//recheck, actual packed loader, cache append and ref increment. A cache hit never
//consumes stream/allocation_base or the filename terminator. Misses require a
//fresh native32 allocation identity; overlapping live ranges are owned errors.
//Input remains caller-owned. Historical allocator failure/virtual service ABI
//and arbitrary overridden slot0 implementations are not claimed.
std::uint32_t Monstey_FearDbOpenNew(Monstey_FearDbResources&,Monstey_FearDbMemory filename_memory,std::uint32_t filename,Monstey_FearDbPackedInput&,std::uint32_t allocation_base,const Monstey_FearDbNameCompare&);
} // namespace Monstey
