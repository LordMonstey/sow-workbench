#include "Monstey_FearUnpackedCleanup.hpp"
#include <string>
namespace Monstey {
Monstey_FearUnpackedCleanupBoundaryFault::Monstey_FearUnpackedCleanupBoundaryFault(const char* name)
 :std::runtime_error(std::string("Consumed FEAR cleanup boundary unavailable: ")+name) {}
namespace {
std::span<std::uint8_t> Monstey_CleanupRange(Monstey_FearDbMutableMemory memory,std::uint32_t address,bool writing) {
 const auto offset=static_cast<std::uint64_t>(address)-memory.base;
 if(address<memory.base||static_cast<std::uint64_t>(address)+4u>0x100000000ull||
    offset>memory.bytes.size()||4u>memory.bytes.size()-static_cast<std::size_t>(offset))
  throw Monstey_FearDbFault(address,4,writing);
 return memory.bytes.subspan(static_cast<std::size_t>(offset),4);
}
std::uint32_t Monstey_CleanupRead(Monstey_FearDbMutableMemory memory,std::uint32_t address) {
 const auto bytes=Monstey_CleanupRange(memory,address,false);
 return static_cast<std::uint32_t>(bytes[0])|(static_cast<std::uint32_t>(bytes[1])<<8u)|
  (static_cast<std::uint32_t>(bytes[2])<<16u)|(static_cast<std::uint32_t>(bytes[3])<<24u);
}
void Monstey_CleanupWrite(Monstey_FearDbMutableMemory memory,std::uint32_t address,std::uint32_t value) {
 auto bytes=Monstey_CleanupRange(memory,address,true);
 for(std::uint32_t i=0;i<4;++i)bytes[i]=static_cast<std::uint8_t>(value>>(i*8u));
}
}
void Monstey_FearUnpackedCleanup(Monstey_FearDbMutableMemory& memory,std::uint32_t map,
 std::uint32_t frame,bool parsed,const Monstey_FearUnpackedCleanupServices& services) {
 const auto head=Monstey_CleanupRead(memory,map+4u);
 const auto first=Monstey_CleanupRead(memory,head);
 Monstey_CleanupWrite(memory,frame-4u,head);
 Monstey_CleanupWrite(memory,frame-8u,first);
 Monstey_CleanupWrite(memory,frame-12u,frame+4u);
 if(!services.erase_range)throw Monstey_FearUnpackedCleanupBoundaryFault("erase_range");
 // The shared holder pins this invocation only. A replaced target must retire
 // before the following native head read, not at the wrapper's final return.
 (void)services.erase_range(memory,{parsed,parsed?0x100061E2u:0x100061B2u,map,
  Monstey_CleanupRead(memory,frame-12u),Monstey_CleanupRead(memory,frame-8u),Monstey_CleanupRead(memory,frame-4u)});
 const auto selected=Monstey_CleanupRead(memory,map+4u);
 Monstey_CleanupWrite(memory,frame-4u,selected);
 if(!services.delete_scalar)throw Monstey_FearUnpackedCleanupBoundaryFault("delete_scalar");
 services.delete_scalar(memory,{parsed?0x100061EBu:0x100061BBu,Monstey_CleanupRead(memory,frame-4u)});
 Monstey_CleanupWrite(memory,map+4u,0);
 Monstey_CleanupWrite(memory,map+8u,0);
}
}
