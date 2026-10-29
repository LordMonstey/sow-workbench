#include "Monstey_FearConstraintReader.hpp"
#include <cstddef>
#include <string>
#include <utility>
namespace Monstey {
Monstey_FearConstraintBoundaryFault::Monstey_FearConstraintBoundaryFault(const char* name)
 :std::runtime_error(std::string("Consumed FEAR constraint boundary unavailable: ")+name) {}
namespace {
std::span<std::uint8_t> Monstey_ConstraintRange(Monstey_FearDbMutableMemory memory,
 std::uint32_t at,std::uint32_t width,bool writing) {
 const auto offset=static_cast<std::uint64_t>(at)-memory.base;
 if(at<memory.base||static_cast<std::uint64_t>(at)+width>0x100000000ull||
    offset>memory.bytes.size()||width>memory.bytes.size()-static_cast<std::size_t>(offset))
  throw Monstey_FearDbFault(at,width,writing);
 return memory.bytes.subspan(static_cast<std::size_t>(offset),width);
}
std::uint32_t Monstey_ConstraintRead(Monstey_FearDbMutableMemory memory,std::uint32_t at,std::uint32_t width=4) {
 const auto bytes=Monstey_ConstraintRange(memory,at,width,false);std::uint32_t value=0;
 for(std::uint32_t i=0;i<width;++i)value|=static_cast<std::uint32_t>(bytes[i])<<(i*8u);
 return value;
}
void Monstey_ConstraintWrite(Monstey_FearDbMutableMemory memory,std::uint32_t at,std::uint32_t value,std::uint32_t width=4) {
 auto bytes=Monstey_ConstraintRange(memory,at,width,true);
 for(std::uint32_t i=0;i<width;++i)bytes[i]=static_cast<std::uint8_t>(value>>(i*8u));
}
template<class Function> Function Monstey_ConstraintBoundary(const Function& function,const char* name) {
 auto copy=function;if(!copy)throw Monstey_FearConstraintBoundaryFault(name);return copy;
}
}
bool Monstey_FearReadConstraints(Monstey_FearDbMutableMemory& memory,std::uint32_t frame,
 Monstey_FearConstraintServices& services) {
 const auto stream=Monstey_ConstraintRead(memory,frame+0x48u);
 auto table=Monstey_ConstraintRead(memory,stream);
 auto target=Monstey_ConstraintRead(memory,table+0x1Cu);
 const auto total=Monstey_ConstraintBoundary(services.get_length,"GetLen")({target,stream});
 // The first GetPos vtable identity is sampled BEFORE the two cached-length stores.
 table=Monstey_ConstraintRead(memory,stream);
 const auto total_low=static_cast<std::uint32_t>(total),total_high=static_cast<std::uint32_t>(total>>32u);
 Monstey_ConstraintWrite(memory,frame+0x20u,total_low);
 Monstey_ConstraintWrite(memory,frame+0x24u,total_high);
 target=Monstey_ConstraintRead(memory,table+0x18u);
 auto position=Monstey_ConstraintBoundary(services.get_position,"GetPos")({target,stream});
 if(static_cast<std::uint32_t>(position)==total_low&&static_cast<std::uint32_t>(position>>32u)==total_high)return true;
 for(;;) {
  table=Monstey_ConstraintRead(memory,stream);
  Monstey_ConstraintWrite(memory,frame+0x10u,0);
  target=Monstey_ConstraintRead(memory,table+4u);
  auto status=Monstey_ConstraintBoundary(services.read,"Read")({target,stream,frame+0x10u,4});
  if(status!=0)return false;
  const auto requested=Monstey_ConstraintRead(memory,frame+0x10u)+1u;
  const auto allocation=Monstey_ConstraintBoundary(services.new_array,"new[]")(requested);
  // Native reloads the length after allocation, then caches both REP counts.
  const auto zero_bytes=Monstey_ConstraintRead(memory,frame+0x10u)+1u;
  auto cursor=allocation;
  for(std::uint32_t i=0;i<(zero_bytes>>2u);++i,cursor+=4u)Monstey_ConstraintWrite(memory,cursor,0);
  for(std::uint32_t i=0;i<(zero_bytes&3u);++i,++cursor)Monstey_ConstraintWrite(memory,cursor,0,1);
  const auto name_bytes=Monstey_ConstraintRead(memory,frame+0x10u);
  table=Monstey_ConstraintRead(memory,stream);
  target=Monstey_ConstraintRead(memory,table+4u);
  status=Monstey_ConstraintBoundary(services.read,"Read")({target,stream,allocation,name_bytes});
  if(status!=0) {
   Monstey_ConstraintBoundary(services.delete_array,"delete[]")(allocation);return false;
  }
  target=Monstey_ConstraintRead(memory,0x1000B064u);
  Monstey_ConstraintBoundary(services.string_default,"string default")({target,frame+0x28u});
  target=Monstey_ConstraintRead(memory,0x1000B068u);
  Monstey_ConstraintBoundary(services.string_assign_cstr,"string assign")({target,frame+0x28u},allocation);
  Monstey_ConstraintBoundary(services.delete_array,"delete[]")(allocation);
  table=Monstey_ConstraintRead(memory,stream);
  Monstey_ConstraintWrite(memory,frame+0x48u,0,1);
  target=Monstey_ConstraintRead(memory,table+4u);
  status=Monstey_ConstraintBoundary(services.read,"Read")({target,stream,frame+0x48u,1});
  if(status==0) {
   table=Monstey_ConstraintRead(memory,stream);
   Monstey_ConstraintWrite(memory,frame+0x1Cu,0);
   target=Monstey_ConstraintRead(memory,table+4u);
   status=Monstey_ConstraintBoundary(services.read,"Read")({target,stream,frame+0x1Cu,4});
  }
  if(status==0) {
   table=Monstey_ConstraintRead(memory,stream);
   Monstey_ConstraintWrite(memory,frame+0x14u,0);
   target=Monstey_ConstraintRead(memory,table+4u);
   status=Monstey_ConstraintBoundary(services.read,"Read")({target,stream,frame+0x14u,4});
  }
  if(status==0) {
   table=Monstey_ConstraintRead(memory,stream);
   Monstey_ConstraintWrite(memory,frame+0x18u,0);
   target=Monstey_ConstraintRead(memory,table+4u);
   status=Monstey_ConstraintBoundary(services.read,"Read")({target,stream,frame+0x18u,4});
  }
  if(status!=0) {
   target=Monstey_ConstraintRead(memory,0x1000B074u);
   Monstey_ConstraintBoundary(services.string_destroy,"string destroy")({target,frame+0x28u});return false;
  }
  const auto type=Monstey_ConstraintRead(memory,frame+0x48u,1);
  const auto second=Monstey_ConstraintRead(memory,frame+0x14u);
  const auto third=Monstey_ConstraintRead(memory,frame+0x18u);
  const auto map=Monstey_ConstraintRead(memory,frame+0x4Cu);
  const auto value=Monstey_ConstraintBoundary(services.map_value,"map subscript")(map,frame+0x28u);
  Monstey_ConstraintWrite(memory,value,type);
  Monstey_ConstraintWrite(memory,value+4u,second);
  Monstey_ConstraintWrite(memory,value+8u,third);
  target=Monstey_ConstraintRead(memory,0x1000B074u);
  Monstey_ConstraintBoundary(services.string_destroy,"string destroy")({target,frame+0x28u});
  table=Monstey_ConstraintRead(memory,stream);
  target=Monstey_ConstraintRead(memory,table+0x18u);
  position=Monstey_ConstraintBoundary(services.get_position,"GetPos")({target,stream});
  if(Monstey_ConstraintRead(memory,frame+0x20u)!=static_cast<std::uint32_t>(position))continue;
  if(Monstey_ConstraintRead(memory,frame+0x24u)!=static_cast<std::uint32_t>(position>>32u))continue;
  return true;
 }
}
} //namespace Monstey
