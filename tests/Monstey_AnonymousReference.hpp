#pragma once
#include "Monstey_GadbPoolRelocation.hpp"
#include <cstdint>
#include <limits>
#include <vector>
namespace Monstey_AnonymousReference {
struct Monstey_Access {std::uint64_t address;std::size_t width;bool writing;};
class Monstey_Bytes {
 Monstey::Monstey_GadbBinderMemory& memory;
 std::vector<Monstey_Access>* trace;
 std::size_t at(std::uint64_t address,std::size_t width,bool write)const{
  if(address<memory.base||width==0||address>std::numeric_limits<std::uint64_t>::max()-(width-1)||address-memory.base>memory.bytes.size()||width>memory.bytes.size()-static_cast<std::size_t>(address-memory.base))
   throw Monstey::Monstey_GadbPoolRelocationFault(address,width,write);
  return static_cast<std::size_t>(address-memory.base);
 }
public:
 explicit Monstey_Bytes(Monstey::Monstey_GadbBinderMemory& m,std::vector<Monstey_Access>* accesses=nullptr):memory(m),trace(accesses){}
 std::uint64_t read(std::uint64_t address,std::size_t width)const{
  const auto offset=at(address,width,false);std::uint64_t value=0;
  for(std::size_t i=0;i<width;++i)value|=static_cast<std::uint64_t>(memory.bytes[offset+i])<<(8*i);
  if(trace)trace->push_back({address,width,false});return value;
 }
 void write(std::uint64_t address,std::size_t width,std::uint64_t value)const{
  const auto offset=at(address,width,true);for(std::size_t i=0;i<width;++i)memory.bytes[offset+i]=static_cast<std::uint8_t>(value>>(8*i));
  if(trace)trace->push_back({address,width,true});
 }
};
// Derived only from SoW141766594..1417666E6. Q=entryRSP-48h.
// Typed read wrappers and type predicate are the already accepted named-family dependencies.
inline std::uint64_t Monstey_AnonymousLoad(Monstey::Monstey_GadbBinderMemory& memory,
 std::uint64_t category,std::uint64_t originalRdx,std::uint64_t record,std::uint64_t stream,
 std::uint64_t frame,const Monstey::Monstey_GadbPoolReadService& read,std::vector<Monstey_Access>* trace=nullptr){
 Monstey_Bytes bytes(memory,trace);
 bytes.write(frame+0x58,8,originalRdx);
 Monstey::Monstey_GadbPoolReadU16(memory,stream,frame+0x20,read);
 Monstey::Monstey_GadbPoolReadU8(memory,stream,frame+0x58,read);
 Monstey::Monstey_GadbPoolReadU32(memory,stream,frame+0x24,read);
 Monstey::Monstey_GadbPoolReadU8(memory,stream,frame+0x80,read);
 const auto initialCount=bytes.read(frame+0x20,2);
 const auto cursor=bytes.read(frame+0x88,8);
 std::uint64_t rax=initialCount;
 const auto words=bytes.read(cursor,8);bytes.write(cursor,8,words+initialCount*8);
 std::uint32_t index=0;
 if(initialCount!=0)do{
  rax=index;const auto slot=words+rax*8;bytes.write(slot,8,0);
  const auto table=bytes.read(stream,8),target=bytes.read(table+8,8);
  if(!read)throw Monstey::Monstey_GadbPoolReadBoundaryFault(0x14176662B,target);
  rax=read(memory,{stream,target,slot,4,0x14176662B});
  if(static_cast<std::uint32_t>(rax)!=0)return rax&0xFFFFFFFFFFFFFF00ull;
  rax=bytes.read(frame+0x20,2);++index;
 }while(index<rax);
 const auto firstDescriptor=bytes.read(frame+0x24,4);
 auto descriptorCount=static_cast<std::uint8_t>(bytes.read(frame+0x58,1));
 const auto descriptors=(firstDescriptor<<4)+bytes.read(frame+0x78,8);
 index=0;
 if(descriptorCount!=0){
  const auto pool=bytes.read(frame+0x70,8);
  do{
   const auto descriptor=descriptors+static_cast<std::uint64_t>(index)*16;
   const auto type=static_cast<std::uint32_t>(bytes.read(descriptor+0xA,1))&0x3F;
   rax=Monstey::Monstey_GadbPoolRelativeTypeRaw(type,rax);
   if((rax&0xFF)!=0){
    std::uint32_t value=0;rax=bytes.read(descriptor+0xE,2);
    const auto start=words+rax*8;
    if(bytes.read(descriptor+0xC,2)!=0){
     do{
      rax=value;++value;const auto slot=start+rax*8;
      const auto old=bytes.read(slot,8);bytes.write(slot,8,old+pool);
      rax=bytes.read(descriptor+0xC,2);
     }while(value<rax);
     descriptorCount=static_cast<std::uint8_t>(bytes.read(frame+0x58,1));
    }
   }
   ++index;
  }while(index<descriptorCount);
 }
 const auto flags=bytes.read(frame+0x80,1);
 rax=(rax&0xFFFFFFFFFFFFFF00ull)|1;
 bytes.write(record+1,1,flags);bytes.write(record,1,descriptorCount);
 bytes.write(record+8,8,descriptors);bytes.write(record+0x18,8,category);bytes.write(record+0x10,8,words);
 return rax;
}
} // namespace Monstey_AnonymousReference
