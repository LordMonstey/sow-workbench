#include "Monstey_GadbValueFixup.hpp"
#include <array>
#include <bit>
namespace Monstey {
Monstey_GadbValueFixupFault::Monstey_GadbValueFixupFault(std::uint64_t a,std::size_t w,bool store)
 :std::runtime_error("Consumed value-fixup range unavailable"),address(a),width(w),writing(store){}
namespace {
using Monstey_Address=std::uint64_t;
using Monstey_Word=std::uint32_t;
// Native signed-byte translation table141DB50D0; independently pinned on bothPEs.
constexpr std::array<std::uint8_t,256> Monstey_CharacterMap={
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x25,0x3a,0x33,0x3c,0x3d,0x3e,0x40,0x32,0x42,0x43,0x41,0x28,0x36,0x27,0x37,0x34,
 0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x31,0x30,0x38,0x29,0x39,0x35,
 0x3b,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x2c,0x2a,0x2d,0x3f,0x26,
 0x44,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x2e,0x2b,0x2f,0x45,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
class Monstey_FixupMemory {
 Monstey_GadbBinderMemory memory_;
 std::size_t Offset(Monstey_Address address,std::size_t width,bool writing)const{
  if(address<memory_.base||address-memory_.base>memory_.bytes.size()||width>memory_.bytes.size()-static_cast<std::size_t>(address-memory_.base))
   throw Monstey_GadbValueFixupFault(address,width,writing);
  return static_cast<std::size_t>(address-memory_.base);
 }
public:
 explicit Monstey_FixupMemory(Monstey_GadbBinderMemory memory):memory_(memory){}
 Monstey_Address Read(Monstey_Address address,std::size_t width)const{
  const auto at=Offset(address,width,false);Monstey_Address value=0;
  for(std::size_t n=0;n<width;++n)value|=static_cast<Monstey_Address>(memory_.bytes[at+n])<<(8*n);
  return value;
 }
 void Write(Monstey_Address address,Monstey_Address value,std::size_t width)const{
  const auto at=Offset(address,width,true);
  for(std::size_t n=0;n<width;++n)memory_.bytes[at+n]=static_cast<std::uint8_t>(value>>(8*n));
 }
};
}
std::uint32_t Monstey_GadbHashCString(Monstey_GadbBinderMemory memory,std::uint64_t string_identity,std::uint32_t seed){
 const Monstey_FixupMemory bytes(memory);auto value=seed;
 auto byte=static_cast<std::uint8_t>(bytes.Read(string_identity,1));
 while(byte!=0){
  ++string_identity;
  const auto translated=std::bit_cast<std::int8_t>(Monstey_CharacterMap[byte]);
  value=value*0x397u+static_cast<Monstey_Word>(static_cast<std::int32_t>(translated));
  byte=static_cast<std::uint8_t>(bytes.Read(string_identity,1));
 }
 return value;
}
std::uint64_t Monstey_GadbFixupValueDescriptor(Monstey_GadbBinderMemory memory,std::uint64_t descriptor,std::uint64_t record,std::uint64_t value_words){
 const Monstey_FixupMemory bytes(memory);const auto flags=static_cast<std::uint8_t>(bytes.Read(descriptor+10u,1));
 Monstey_Address result=flags&0x3fu;
 if(result==5){
  const auto first=static_cast<Monstey_Word>(bytes.Read(descriptor+14u,2));Monstey_Word ordinal=0;
  if(bytes.Read(descriptor+12u,2)==0)return result;
  do{
   const Monstey_Word index=ordinal+first;
   const auto string_identity=bytes.Read(value_words+static_cast<Monstey_Address>(index)*8u,8);
   const auto hash=Monstey_GadbHashCString(memory,string_identity,0);
   ++ordinal;
   bytes.Write(value_words+static_cast<Monstey_Address>(index)*8u,hash,8);
   result=bytes.Read(descriptor+12u,2);
  }while(ordinal<result);
  return result;
 }
 if(result<=9||result>11)return result;
 auto index=static_cast<Monstey_Word>(bytes.Read(descriptor+14u,2));const auto negative_first=Monstey_Word{0}-index;
 bytes.Write(descriptor+10u,static_cast<std::uint8_t>((flags&0xcau)|0x0au),1);
 result=bytes.Read(record+24u,8);
 const auto database=bytes.Read(result+24u,8);
 if(bytes.Read(descriptor+12u,2)==0)return result;
 do{
  const auto packed=static_cast<Monstey_Word>(bytes.Read(value_words+static_cast<Monstey_Address>(index)*8u,4));
  Monstey_Address resolved=0;Monstey_Word ordinal=packed&0xfffffu;const auto category_index=packed>>20;
  if(category_index<bytes.Read(database+72u,4)){
   const auto category=bytes.Read(database+80u,8)+static_cast<Monstey_Address>(category_index)*88u;
   if(ordinal<bytes.Read(category+64u,4)){
    resolved=bytes.Read(category+72u,8)+static_cast<Monstey_Address>(ordinal)*32u;
   }
   if(resolved==0){
    ordinal-=static_cast<Monstey_Word>(bytes.Read(category+64u,4));
    if(ordinal<bytes.Read(category+40u,4))
     resolved=bytes.Read(category+56u,8)+static_cast<Monstey_Address>(ordinal)*40u;
   }
  }
  bytes.Write(value_words+static_cast<Monstey_Address>(index)*8u,resolved,8);
  ++index;
  result=bytes.Read(descriptor+12u,2);
 }while(static_cast<Monstey_Word>(negative_first+index)<result);
 return result;
}
} // namespace Monstey
