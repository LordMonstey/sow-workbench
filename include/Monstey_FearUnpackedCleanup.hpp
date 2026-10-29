#pragma once
#include "Monstey_FearDbCallable.hpp"
#include "Monstey_FearDbPrimitives.hpp"
#include <cstdint>
#include <stdexcept>
namespace Monstey {
struct Monstey_FearUnpackedEraseCall {
 bool parsed;
 std::uint32_t native_site,map,hidden_result,first,last;
};
struct Monstey_FearUnpackedDeleteCall {std::uint32_t native_site,address;};
struct Monstey_FearUnpackedCleanupServices {
 Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory&,Monstey_FearUnpackedEraseCall)> erase_range;
 Monstey_FearDbCallable<void(Monstey_FearDbMutableMemory&,Monstey_FearUnpackedDeleteCall)> delete_scalar;
};
class Monstey_FearUnpackedCleanupBoundaryFault final:public std::runtime_error {
public:
 explicit Monstey_FearUnpackedCleanupBoundaryFault(const char* name);
};
// Complete 61A0 (constraints) and 61D0 (parsed values) wrappers. Frame is S after
// native pushECX/pushESI, entryESP-8. Map identity stays cached; head reloads after
// erase. Known outgoing argument stores at S-4/-8/-C and local hidden result S+4
// are owned memory, retaining consumed overlap/fault order. Other prologue,
// saved-register, return-address and original callee/private CRT ABI is separate.
// No original function pointers are invoked. Only consumed services are required.
// Rebinding owned memory at a callback is supported; no borrowed span survives it.
void Monstey_FearUnpackedCleanup(Monstey_FearDbMutableMemory&,std::uint32_t map,
 std::uint32_t frame,bool parsed,const Monstey_FearUnpackedCleanupServices&);
}
