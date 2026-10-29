#include "Monstey_FearDbPrimitives.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using Monstey_Word=std::uint32_t;
struct Monstey_Trap {Monstey_Word at,width;bool write;};
struct Monstey_Image {
 Monstey_Word base;std::vector<std::uint8_t> bytes;std::size_t available;
 std::size_t Check(Monstey_Word a,Monstey_Word n,bool write)const{
  const std::uint64_t first=a,last=first+n;
  if(first<base||last>0x100000000ull||first-base>available||n>available-static_cast<std::size_t>(first-base))throw Monstey_Trap{a,n,write};
  return static_cast<std::size_t>(first-base);
 }
 Monstey_Word Get(Monstey_Word a,Monstey_Word n)const{
  const auto p=Check(a,n,false);Monstey_Word z=0;
  for(Monstey_Word k=0;k<n;++k)z|=Monstey_Word(bytes[p+k])<<(8*k);return z;
 }
 void Put(Monstey_Word a,Monstey_Word z){
  const auto p=Check(a,4,true);for(unsigned k=0;k<4;++k)bytes[p+k]=static_cast<std::uint8_t>(z>>(8*k));
 }
};
struct Monstey_Result{Monstey_Word value=0,at=0,width=0;bool fault=false,write=false;};
// Each arm follows the consumed register/memory epochs in the original x86
// bodies. The Float arm stops before native FLD; original x87 state is excluded.
Monstey_Word Monstey_OriginalWords(Monstey_Image &m,unsigned api,Monstey_Word h,Monstey_Word index,Monstey_Word fallback,Monstey_Word output){
 if(api==0)return h==0?0:m.Get(h,4);
 if(api==1)return h==0?0:m.Get(h+4u,4);
 if(api==2)return h==0?0:m.Get(h+8u,4);
 if(api==3){if(!h)return 0;const auto count=m.Get(h+8u,4);if(index>=count)return 0;const auto p=m.Get(h+12u,4);return p+(index<<4);}
 if(api==4){if(!h)return 0;const auto p=m.Get(h+12u,4);return p+(index<<4);}
 if(api==5){if(!h)return 0xffffffffu;const auto p=m.Get(h+4u,4);const auto begin=m.Get(p+12u,4);const auto delta=h-begin;return (delta>>4)|((delta&0x80000000u)?0xf0000000u:0u);}
 if(api==6)return h==0?0:m.Get(h+8u,2);
 if(api==7)return h==0?0:m.Get(h+10u,1);
 if(api==8)return h==0?0:m.Get(h+11u,1);
 constexpr std::array<Monstey_Word,9> kinds{3,2,4,5,9,1,6,7,8};
 const auto kind=kinds.at(api-9);bool matched=false;
 if(h){const auto stored=m.Get(h+10u,1);if(stored==kind){const auto count=m.Get(h+8u,2);matched=index<count;}}
 if(api<15){
  if(!matched)return api==14?fallback&0xffu:fallback;
  const auto p=m.Get(h+12u,4);const auto effective=p+(index<<2);
  return m.Get(effective,api==14?1u:4u);
 }
 Monstey_Word src=fallback;
 if(matched){const auto p=m.Get(h+12u,4);src=m.Get(p+(index<<2),4);}
 const unsigned words=api-13;
 for(unsigned k=0;k<words;++k){const auto value=m.Get(src+4u*k,4);m.Put(output+4u*k,value);}
 return output;
}
Monstey_Word Monstey_Product(Monstey_Image&m,unsigned api,Monstey_Word h,Monstey_Word i,Monstey_Word f,Monstey_Word o){
 const Monstey::Monstey_FearDbMemory r{m.base,std::span<const std::uint8_t>(m.bytes).first(m.available)};
 const Monstey::Monstey_FearDbMutableMemory w{m.base,std::span<std::uint8_t>(m.bytes).first(m.available)};
 switch(api){
 case 0:return Monstey::Monstey_FearDbNodeName(r,h);
 case 1:return Monstey::Monstey_FearDbNodeParentOrDatabaseName(r,h);
 case 2:return Monstey::Monstey_FearDbChildCount(r,h);
 case 3:return Monstey::Monstey_FearDbChildByIndex(r,h,i);
 case 4:return Monstey::Monstey_FearDbAttributeByIndex(r,h,i);
 case 5:return Monstey::Monstey_FearDbHandleIndex(r,h);
 case 6:return Monstey::Monstey_FearDbValueCount(r,h);
 case 7:return Monstey::Monstey_FearDbAttributeType(r,h);
 case 8:return Monstey::Monstey_FearDbAttributeUsage(r,h);
 case 9:return Monstey::Monstey_FearDbInt32Bits(r,h,i,f);
 case 10:return Monstey::Monstey_FearDbFloat32Bits(r,h,i,f);
 case 11:return Monstey::Monstey_FearDbStringIdentity(r,h,i,f);
 case 12:return Monstey::Monstey_FearDbWStringIdentity(r,h,i,f);
 case 13:return Monstey::Monstey_FearDbRecordIdentity(r,h,i,f);
 case 14:return Monstey::Monstey_FearDbBoolByte(r,h,i,static_cast<std::uint8_t>(f));
 case 15:return Monstey::Monstey_FearDbVector2(w,h,i,f,o);
 case 16:return Monstey::Monstey_FearDbVector3(w,h,i,f,o);
 case 17:return Monstey::Monstey_FearDbVector4(w,h,i,f,o);
 default:throw std::logic_error("Unknown fixture API");
 }
}
std::uint64_t Monstey_Cases=0,Monstey_Checks=0;
void Monstey_Run(const Monstey_Image&seed,unsigned api,Monstey_Word h,Monstey_Word i,Monstey_Word f,Monstey_Word o){
 auto a=seed,b=seed;Monstey_Result expected,actual;
 try{expected.value=Monstey_OriginalWords(a,api,h,i,f,o);}catch(const Monstey_Trap&t){expected={0,t.at,t.width,true,t.write};}
 try{actual.value=Monstey_Product(b,api,h,i,f,o);}catch(const Monstey::Monstey_FearDbFault&t){actual={0,t.address,t.width,true,t.writing};}
 ++Monstey_Cases;
 const bool same=expected.value==actual.value&&expected.at==actual.at&&expected.width==actual.width&&expected.fault==actual.fault&&expected.write==actual.write;
 if(!same)throw std::runtime_error("Behavior/fault differs at case "+std::to_string(Monstey_Cases)+" api "+std::to_string(api));
 ++Monstey_Checks;
 if(a.bytes!=b.bytes)throw std::runtime_error("Memory/partial-write differs at case "+std::to_string(Monstey_Cases));
 ++Monstey_Checks;
}
Monstey_Image Monstey_Seed(){
 Monstey_Image m{0x1000,std::vector<std::uint8_t>(512),512};Monstey_Word rng=0x6349d11u;
 for(auto&x:m.bytes){rng=rng*1664525u+1013904223u;x=static_cast<std::uint8_t>(rng>>24);}
 m.Put(0x1040,0x1190);m.Put(0x1044,0x1080);m.Put(0x1048,0x07030002u);m.Put(0x104c,0x10c0);m.Put(0x108c,0x1030);
 m.Put(0x10c0,0x1100);m.Put(0x10c4,0);m.Put(0x10c8,0xffffffffu);
 m.Put(0x1100,0x80000000u);m.Put(0x1104,0x7fc12345u);m.Put(0x1108,0x00ffffffu);m.Put(0x110c,0x01020304u);
 return m;
}
void Monstey_SetType(Monstey_Image&m,unsigned type){m.bytes[0x4a]=static_cast<std::uint8_t>(type);}
}
int main(){try{
 // Group1: null operands never consume borrowed memory, except vector fallback.
 auto seed=Monstey_Seed();
 for(unsigned api=0;api<18;++api)for(auto index:std::array<Monstey_Word,5>{0,1,0xffff,0x80000000u,0xffffffffu}){
  auto m=seed;m.available=0;Monstey_Run(m,api,0,index,0x1110,0x1180);
 }
 // Group2: byte type comparison, U16 count, full U32 index and raw values.
 for(unsigned type=0;type<256;++type)for(auto count:std::array<Monstey_Word,5>{0,1,2,255,65535})for(auto index:std::array<Monstey_Word,7>{0,1,2,254,65534,0x80000000u,0xffffffffu}){
  auto m=seed;Monstey_SetType(m,type);m.bytes[0x48]=static_cast<std::uint8_t>(count);m.bytes[0x49]=static_cast<std::uint8_t>(count>>8);
  for(unsigned api=9;api<18;++api)Monstey_Run(m,api,0x1040,index,0x1100,0x1180);
 }
 // Group3: every truncation point, including destination writes after prior words.
 for(std::size_t end=0;end<=512;++end)for(unsigned api=0;api<18;++api){
  auto m=seed;m.available=end;if(api>=9)Monstey_SetType(m,std::array<unsigned,9>{3,2,4,5,9,1,6,7,8}[api-9]);
  Monstey_Run(m,api,0x1040,0,0x1110,0x1180);
 }
 // Group4: successful/default vector copies, all byte alignments and overlaps.
 for(unsigned api=15;api<18;++api)for(unsigned src=0;src<32;++src)for(unsigned dst=0;dst<32;++dst)for(unsigned mode=0;mode<3;++mode){
  auto m=seed;const auto source=0x1100u+src,output=0x1100u+dst;
  Monstey_SetType(m,api-9);m.Put(0x10c0,source);
  Monstey_Run(m,api,mode==0?0u:0x1040u,mode==2?65535u:0u,source,output);
 }
 // Group5: wrapping shift/add for indexed handles and signed SAR of subtraction.
 constexpr std::array<Monstey_Word,8> values{0,1,15,16,0x7fffffffu,0x80000000u,0xfffffff0u,0xffffffffu};
 for(auto first:values)for(auto index:values){
  auto m=seed;m.Put(0x1048,0xffffffffu);m.Put(0x104c,first);m.Put(0x108c,first);
  Monstey_Run(m,3,0x1040,index,0,0);Monstey_Run(m,4,0x1040,index,0,0);Monstey_Run(m,5,0x1040,index,0,0);
 }
 // Group6: low AL byte only and four-byte stride, including terminal one-byte read.
 for(unsigned low=0;low<256;++low){auto m=seed;Monstey_SetType(m,1);m.Put(0x10c0,0xaabb0000u|low);m.available=0xc1;Monstey_Run(m,14,0x1040,0,0xff,0);}
 // Group7: invalid and wrapped effective identities fault at first consumed range.
 for(auto h:values)for(unsigned api=0;api<18;++api)Monstey_Run(seed,api,h,0xffffffffu,0xffffffffu,0xffffffffu);
 // Group8: bit payloads and rawzero pointers must not turn into lazy defaults.
 for(auto value:std::array<Monstey_Word,9>{0,1,0xffffffffu,0x80000000u,0x7f800000u,0xff800000u,0x7fc00001u,0x7fa12345u,0x00800000u})for(unsigned api=9;api<15;++api){
  auto m=seed;Monstey_SetType(m,std::array<unsigned,6>{3,2,4,5,9,1}[api-9]);m.Put(0x10c0,value);Monstey_Run(m,api,0x1040,0,0xfefefefeu,0);
 }
 std::cout<<"PASS fear_reader_independent groups=8 cases="<<Monstey_Cases<<" checks="<<Monstey_Checks<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}