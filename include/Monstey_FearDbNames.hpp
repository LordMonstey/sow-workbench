#pragma once
#include "Monstey_FearDbPrimitives.hpp"
#include "Monstey_FearDbCallable.hpp"
namespace Monstey {
// The original target is MSVCR71!_stricmp. This explicit boundary supplies its
// signed EAX result for raw32 {query,candidate} identities. No locale, character
// decoding, ASCII default or historical CRT body is invented. It is required
// only at an actually consumed nonnull comparison. Borrowed bytes stay stable;
// the callable may change their contents between native reads.
// Copies share the existing target and its mutable state; replacement publishes
// a new target. Active calls retain their old target through return/exception.
//7020 consumes the current holder for each comparison. OpenExisting samples a
// shared target once at6A6D; a caller may replace its holder during that lookup.
// This is an owned C++ boundary, not original CRT or binary C++ ABI equivalence.
using Monstey_FearDbNameCompare=Monstey_FearDbCallable<std::int32_t(std::uint32_t,std::uint32_t)>;
std::uint32_t Monstey_FearDbBinaryNameSearch(Monstey_FearDbMemory,std::uint32_t children,std::uint32_t count,std::uint32_t query,const Monstey_FearDbNameCompare&); //7020
//7090 reads children+C before count+8, snapshots both, and applies no null/empty
//query filter. Raw Binary accepts null queries and null row-name identities.
std::uint32_t Monstey_FearDbRawChildrenName(Monstey_FearDbMemory,std::uint32_t parent,std::uint32_t query,const Monstey_FearDbNameCompare&);
//6620 is shared by direct GetCategory(DB,name), GetRecord(category,name), and
//GetAttribute(record,name). It checks parent, query, then only query's first byte.
std::uint32_t Monstey_FearDbGuardedChildName(Monstey_FearDbMemory,std::uint32_t parent,std::uint32_t query,const Monstey_FearDbNameCompare&);
// Known original service mapping, not arbitrary overridden virtual methods:
//13/+34 -> category7 then raw record lookup; final name has no guard.
std::uint32_t Monstey_FearDbRecordByNames(Monstey_FearDbMemory,std::uint32_t database,std::uint32_t category_name,std::uint32_t record_name,const Monstey_FearDbNameCompare&);
//20/+50 -> guarded record14 then raw attribute lookup.
std::uint32_t Monstey_FearDbAttributeByNames(Monstey_FearDbMemory,std::uint32_t category,std::uint32_t record_name,std::uint32_t attribute_name,const Monstey_FearDbNameCompare&);
//21/+54 -> record13 then raw attribute lookup.
std::uint32_t Monstey_FearDbAttributeInDatabaseByNames(Monstey_FearDbMemory,std::uint32_t database,std::uint32_t category_name,std::uint32_t record_name,std::uint32_t attribute_name,const Monstey_FearDbNameCompare&);
} // namespace Monstey
