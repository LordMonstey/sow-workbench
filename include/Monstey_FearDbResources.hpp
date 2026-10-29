#pragma once
#include "Monstey_FearDbPacked.hpp"
#include <vector>
namespace Monstey {
// Ownership registry for live, disjoint native32 allocations. The native cache is
// a separate ordered pointer array: duplicates and uncached live allocations are
// preserved. This is owned storage, not an original manager/vtable ABI.
struct Monstey_FearDbResources {
 std::vector<Monstey_FearDbPackedImage> allocations;
 std::vector<std::uint32_t> cache;
};
enum class Monstey_FearDbReleaseKind { ignored_null,retained,freed,null_allocation };
struct Monstey_FearDbReleaseObservation {
 Monstey_FearDbReleaseKind kind=Monstey_FearDbReleaseKind::ignored_null;
 std::uint32_t counter_after=0,allocation_base=0;
 std::size_t removed_cache_index=static_cast<std::size_t>(-1);
};
// Selected matched-cache ref increment at6A9C, independent of unresolved name
// comparison. No selection/comparator is invented by this helper.
void Monstey_FearDbRetainSelected(Monstey_FearDbResources&,std::uint32_t database);
// Successful 6D90 append then6E26 increment. Does not implement cache lookup,
// vector allocator failure ABI or the complete OpenNew method.
void Monstey_FearDbRegisterLoaded(Monstey_FearDbResources&,std::uint32_t database);
//6D40 release: wrapping decrement, first cache identity erase (stable memmove),
//then delete[] allocation read from database+0 even if cache had no matching row.
//Observation is owned diagnostic data, not the native void return ABI.
//Consumed unavailable ranges/free targets throw after prior mutations; native
//invalid-pointer/double-free behavior and CRT allocator internals are not emulated.
Monstey_FearDbReleaseObservation Monstey_FearDbReleaseDatabase(Monstey_FearDbResources&,std::uint32_t database);
} // namespace Monstey
