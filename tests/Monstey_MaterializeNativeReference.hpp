#pragma once
#include "Monstey_FearUnpackedMaterialize.hpp"
#include <cstdint>
namespace Monstey_IndependentMaterialize {
struct Monstey_RefFault {std::uint32_t address,width;bool writing;};
class Monstey_RefMemory {
 Monstey::Monstey_FearDbMutableMemory memory;
 std::size_t Monstey_Offset(std::uint32_t at,std::uint32_t width,bool write)const {
  const auto delta=static_cast<std::uint64_t>(at)-memory.base;
  if(at<memory.base||std::uint64_t(at)+width>0x100000000ull||delta>memory.bytes.size()||width>memory.bytes.size()-static_cast<std::size_t>(delta))throw Monstey_RefFault{at,width,write};
  return static_cast<std::size_t>(delta);
 }
public:
 explicit Monstey_RefMemory(Monstey::Monstey_FearDbMutableMemory value):memory(value){}
 std::uint32_t Monstey_Read(std::uint32_t at,std::uint32_t width=4)const {
  const auto delta=Monstey_Offset(at,width,false);std::uint32_t result=0;
  for(std::uint32_t i=0;i<width;++i)result|=std::uint32_t(memory.bytes[delta+i])<<(8u*i);return result;
 }
 void Monstey_Write(std::uint32_t at,std::uint32_t value,std::uint32_t width=4)const {
  const auto delta=Monstey_Offset(at,width,true);for(std::uint32_t i=0;i<width;++i)memory.bytes[delta+i]=static_cast<std::uint8_t>(value>>(8u*i));
 }
};
// Literal load/store epochs from6EA0,6EC0,6EF0. Args points to the
// original function entry argument word0; object/argument storage may alias.
inline void Monstey_RefNode16(const Monstey_RefMemory& m,std::uint32_t object,std::uint32_t args) {
 auto a=m.Monstey_Read(args+4),d=m.Monstey_Read(args+12);m.Monstey_Write(object,a);
 a=m.Monstey_Read(args+16);m.Monstey_Write(object+8,d);d=m.Monstey_Read(args+8);
 m.Monstey_Write(object+12,a);m.Monstey_Write(object+4,d);
}
inline void Monstey_RefRoot20(const Monstey_RefMemory& m,std::uint32_t object,std::uint32_t args) {
 auto a=m.Monstey_Read(args+4),d=m.Monstey_Read(args+8);m.Monstey_Write(object+4,a);
 a=m.Monstey_Read(args+12);m.Monstey_Write(object+8,d);d=m.Monstey_Read(args+16);
 m.Monstey_Write(object+12,a);m.Monstey_Write(object,d);m.Monstey_Write(object+16,0);
}
inline void Monstey_RefAttribute16(const Monstey_RefMemory& m,std::uint32_t object,std::uint32_t args) {
 auto a=m.Monstey_Read(args+4),d=m.Monstey_Read(args+12,1);m.Monstey_Write(object,a);
 a=m.Monstey_Read(args+16,1);m.Monstey_Write(object+10,d,1);d=m.Monstey_Read(args+20,2);
 m.Monstey_Write(object+11,a,1);a=m.Monstey_Read(args+24);m.Monstey_Write(object+8,d,2);
 d=m.Monstey_Read(args+8);m.Monstey_Write(object+12,a);m.Monstey_Write(object+4,d);
}
inline void Monstey_RefFixAttribute(const Monstey_RefMemory& m,std::uint32_t attribute) {
 if(m.Monstey_Read(attribute+10,1)!=9)return;
 const auto record=m.Monstey_Read(attribute+4),category=m.Monstey_Read(record+4),root=m.Monstey_Read(category+4);
 if(m.Monstey_Read(attribute+8,2)==0)return;
 std::uint32_t index=0;
 do {
  const auto cells=m.Monstey_Read(attribute+12),cell=cells+index*4u,key=m.Monstey_Read(cell);
  const auto category_index=key>>16u,record_index=key&0xFFFFu;std::uint32_t target=0;
  if(category_index<m.Monstey_Read(root+8)) {
   const auto selected=m.Monstey_Read(root+12)+(category_index<<4u);
   if(record_index<m.Monstey_Read(selected+8))target=m.Monstey_Read(selected+12)+(record_index<<4u);
  }
  m.Monstey_Write(cell,target);++index;
 }while(index<m.Monstey_Read(attribute+8,2));
}
inline void Monstey_RefFixCategory(const Monstey_RefMemory& m,std::uint32_t category) {
 if(m.Monstey_Read(category+8)==0)return;std::uint32_t index=0,offset=0;
 do {
  const auto record=m.Monstey_Read(category+12)+offset;
  if(m.Monstey_Read(record+8)!=0) {
   std::uint32_t attribute_index=0,attribute_offset=0;
   do {
    const auto attribute=m.Monstey_Read(record+12)+attribute_offset;Monstey_RefFixAttribute(m,attribute);
    ++attribute_index;attribute_offset+=16;
   }while(attribute_index<m.Monstey_Read(record+8));
  }
  ++index;offset+=16;
 }while(index<m.Monstey_Read(category+8));
}
inline void Monstey_RefFixLinks(Monstey::Monstey_FearDbMutableMemory memory,std::uint32_t root) {
 const Monstey_RefMemory m(memory);if(m.Monstey_Read(root+8)==0)return;
 std::uint32_t index=0,offset=0;
 do {const auto category=m.Monstey_Read(root+12)+offset;Monstey_RefFixCategory(m,category);++index;offset+=16;}while(index<m.Monstey_Read(root+8));
}
}