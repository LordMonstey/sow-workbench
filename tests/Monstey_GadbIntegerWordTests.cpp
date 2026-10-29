#include "Monstey_GadbIntegerWord.hpp"
#include "Monstey_GadbScalarFloat.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace Monstey;
namespace {
using U64=std::uint64_t;using U32=std::uint32_t;
constexpr U64 B=0x100000000ull,R=B+0x100,F=B+0x300,T=B+0x400;
constexpr U64 D=B+0x600,O=B+0x800,C=B+0x900,V=B+0x10000,Z=B+0x20000,A=B+0x18000;
constexpr U64 Bad=B+0x10000000;
constexpr U32 Default=0xa5c3f17bu;
U64 cases=0,checks=0,groups=0;
void Check(bool v,const char*why){++checks;if(!v)throw std::runtime_error(why);}
struct Fixture {
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x30000);
 void Put(U64 ea,U64 value,std::size_t width){
  if(ea<B||ea-B+width>bytes.size())throw std::runtime_error("Fixture write");
  for(std::size_t i=0;i<width;++i)bytes[static_cast<std::size_t>(ea-B)+i]=static_cast<std::uint8_t>(value>>(8*i));
 }
 Monstey_GadbBinderMemory Memory(){return{B,bytes};}
 Fixture(){
  Put(R,1,1);Put(R+1,1,1);Put(R+8,T,8);Put(R+16,V,8);Put(R+24,O,8);Put(O+32,C,8);
  Put(F,0x91234567,4);Put(T,0x91234567,4);Put(T+4,0x91234567,4);Put(T+9,0,1);
  Put(T+10,5,1);Put(T+12,3,2);Put(T+14,2,2);
  Put(C,D,8);Put(C+8,1,4);Put(D+32,0x91234567,4);Put(D+36,5,4);Put(D+40,3,4);
  Put(Z,0x1122334400000000ull+Default,8);
  for(U32 i=0;i<24;++i)Put(V+8ull*i,0xdeadbeef80000000ull+i,8);
 }
 U32 Read(U32 index=0,U64 fallback=Z){return Monstey_GadbReadIntegerWord(Memory(),R,F,index,fallback);}
 U32 Cached(std::uint8_t physical=0,U32 index=0,U64 fallback=Z){return Monstey_GadbReadCachedIntegerWord(Memory(),R,physical,index,fallback);}
};
void Basic(){
 ++groups;Fixture f;
 ++cases;Check(Monstey_GadbReadIntegerWord(f.Memory(),0,Bad,UINT32_MAX,Z)==Default,"Null generic default");
 ++cases;Check(Monstey_GadbReadGeneratedIntegerWord(f.Memory(),0,Bad,UINT32_MAX,Z)==Default,"Null generated default");
 ++cases;Check(Monstey_GadbReadCachedIntegerWord(f.Memory(),0,255,UINT32_MAX,Z)==Default,"Null cached default");
 for(U32 bits:{0u,1u,0x7fffffffu,0x80000000u,UINT32_MAX}){
  f.Put(V+16,0xfedcba9800000000ull+bits,8);
  ++cases;Check(f.Read(0,Bad)==bits,"Raw DWORD must preserve low32/zero without fallback");
  ++cases;Check(f.Cached(0,0,Bad)==bits,"Cached DWORD must preserve low32/zero");
 }
}
void TypeAndFlagMatrix(){
 ++groups;Fixture f;
 for(U32 type=0;type<256;++type)for(U32 flags:{0u,1u,16u,17u}){
  f.Put(T+10,type,1);f.Put(R+1,flags,1);
  for(U32 i:{0u,1u,2u,3u,65535u,0x80000000u,UINT32_MAX}){
   const U32 expected=(type&63u)==5&&i<3?0x80000000u+i+((flags&16u)?0u:2u):Default;
   ++cases;Check(f.Read(i)==expected,"Exact low6 type5 / unsigned count / generated flag");
  }
 }
}
void CachedBypass(){
 ++groups;Fixture f;f.Put(R,0,1);
 for(U32 p=0;p<256;++p)for(U32 type=0;type<256;++type)for(U32 flags:{0u,1u,16u,255u}){
  const auto row=T+16ull*p;
  f.Put(row+10,type,1);f.Put(row+12,1,2);f.Put(row+14,2,2);f.Put(R+1,flags,1);
  ++cases;Check(f.Cached(static_cast<std::uint8_t>(p),0,Bad)==0x80000002u,"Cache bypasses type,header count and generated flag");
 }
}
void CountAndOffset(){
 ++groups;Fixture f;f.bytes.resize(0x220000);
 f.Put(T+14,65535,2);
 for(U32 count:{0u,1u,2u,32767u,32768u,65535u}){
  f.Put(T+12,count,2);
  for(U32 index:{0u,1u,count==0?0u:count-1,count,0x80000000u,UINT32_MAX}){
   const U32 expected=index<count?0xf0000000u+index:Default;
   if(index<count)f.Put(V+8ull*(65535u+index),0xabcdef1200000000ull+expected,8);
   for(U32 flag:{0u,1u}){f.Put(R+1,flag,1);++cases;Check(f.Read(index)==expected,"Raw unsigned bounds and nontruncated first+index");}
   ++cases;Check(f.Cached(0,index)==expected,"Cached unsigned bounds and nontruncated first+index");
  }
 }
}
void Generated(){
 ++groups;Fixture f;f.Put(R+1,17,1);f.Put(T+12,20,2);
 for(U32 bits:{0u,1u,0x80000000u,UINT32_MAX}){
  f.Put(V,0xfedcba9800000000ull+bits,8);
  ++cases;Check(f.Read(0,Bad)==bits,"Generated lowDWORD/fullQWORD");
 }
 f.Put(R+24,0,8);++cases;Check(f.Read()==Default,"Generated null owner default");
 f.Put(R+24,O,8);f.Put(O+32,0,8);++cases;Check(f.Read()==Default,"Generated null class default");
 f.Put(O+32,C,8);f.Put(C+8,0,4);++cases;Check(f.Read()==Default,"Generated absent field default");
 f.Put(C+8,1,4);f.Put(D+32,1,4);++cases;Check(f.Read()==Default,"Generated nonmatching field default");
 f.Put(D+32,0x91234567,4);f.Put(D+40,UINT32_MAX,4);f.Put(V,A,8);
 for(U32 i=0;i<20;++i)f.Put(A+8ull*i,0x1234567800000000ull+i,8);
 for(U32 type:{5u,8u,9u})for(U32 count=0;count<5;++count)for(U32 i=0;i<7;++i){
  f.Put(D+36,type,4);f.Put(V+8,count,4);
  const U32 effective=type==5?i:2*i;++cases;
  if(effective<count)Check(f.Read(i,Bad)==effective,"Generated dynamic value/double-index");
  else try{(void)f.Read(i,Bad);throw std::runtime_error("Expected dynamic assertion");}
  catch(const Monstey_GadbScalarFault&e){Check(e.address==V+8,"Dynamic assertion before values/default");}
 }
}
void GeneratedDirect(){
 ++groups;Fixture f;
 f.Put(R,0,1);f.Put(R+1,0,1);f.Put(T+10,3,1);f.Put(T+12,0,2);
 for(U32 type:{5u,8u,9u}){
  f.Put(D+36,type,4);f.Put(D+40,0,4);
  const U32 expected=type==5?0x80000001u:0x80000002u;
  ++cases;Check(Monstey_GadbReadGeneratedIntegerWord(f.Memory(),R,F,1,Bad)==expected,
                "Direct generated entry must bypass physical descriptor/type/count");
 }
 f.Put(D+40,UINT32_MAX,4);f.Put(V,A,8);f.Put(V+8,1,4);f.Put(A,0x12345678deadbeefull,8);
 for(U32 type:{8u,9u}){
  f.Put(D+36,type,4);++cases;
  Check(Monstey_GadbReadGeneratedIntegerWord(f.Memory(),R,F,0x80000000u,Bad)==0xdeadbeefu,
        "Native generated doubled-index wraps atU32");
 }
 f.Put(D+36,5,4);++cases;
 try{(void)Monstey_GadbReadGeneratedIntegerWord(f.Memory(),R,F,0x80000000u,Bad);throw std::runtime_error("Expected direct dynamic assertion");}
 catch(const Monstey_GadbScalarFault&e){Check(e.address==V+8,"Direct generated unsigned assertion");}
}
enum class Mode{Generic,Generated,Cached};
U32 Call(Fixture&f,Mode mode,std::size_t len,U64 record,U64 field,U32 i,U64 fallback){
 auto mem=f.Memory();mem.bytes=mem.bytes.first(len);
 if(mode==Mode::Cached)return Monstey_GadbReadCachedIntegerWord(mem,record,0,i,fallback);
 if(mode==Mode::Generated)return Monstey_GadbReadGeneratedIntegerWord(mem,record,field,i,fallback);
 return Monstey_GadbReadIntegerWord(mem,record,field,i,fallback);
}
void Fault(Fixture&f,Mode mode,U64 expected,U64 record=R,U64 field=F,U32 index=0,U64 fallback=Z,std::size_t len=0){
 ++cases;
 try{(void)Call(f,mode,len?len:f.bytes.size(),record,field,index,fallback);throw std::runtime_error("Missing consumed fault");}
 catch(const Monstey_GadbBinderFault&e){
  Check(e.kind==Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable,"Fault kind");
  Check(e.address==expected,"Wrong first consumed fault");
 }
}
void WidthAndFailures(){
 ++groups;Fixture f;
 ++cases;Check(Call(f,Mode::Generic,0x10000+20,R,F,0,Bad)==0x80000002u,"Raw succeeds on only four available value bytes");
 ++cases;Check(Call(f,Mode::Cached,0x10000+20,R,F,0,Bad)==0x80000002u,"Cache succeeds on only four available value bytes");
 f.Put(R+1,17,1);Fault(f,Mode::Generic,V,R,F,0,Bad,0x10000+4);
 ++cases;Check(Call(f,Mode::Generic,0x20000+4,0,Bad,0,Z)==Default,"Fallback reads only four bytes");
 ++cases;Check(Call(f,Mode::Cached,0x20000+4,0,Bad,0,Z)==Default,"Cached fallback reads only four bytes");
 Fault(f,Mode::Generic,Bad+1,Bad,Bad,0,Z);
 Fault(f,Mode::Cached,Bad+8,Bad,Bad,0,Z);
 Fault(f,Mode::Generated,Bad+24,Bad,Bad,0,Z);
 f=Fixture{};f.Put(T+10,3,1);
 ++cases;Check(Call(f,Mode::Generic,0x40b,R,F,0,B)==0,"Type miss skips count/offset");
 f.Put(T+10,5,1);Fault(f,Mode::Generic,T+12,R,F,0,B,0x40c);
 f.Put(T+12,0,2);
 ++cases;Check(Call(f,Mode::Generic,0x40e,R,F,0,B)==0,"Count zero skips offset");
 ++cases;Check(Call(f,Mode::Cached,0x40e,R,F,0,B)==0,"Cached count zero skips offset");
 f.Put(T+12,1,2);Fault(f,Mode::Cached,T+14,R,F,0,B,0x40e);
 f=Fixture{};f.Put(R+16,Bad,8);Fault(f,Mode::Generic,Bad+16);
 Fault(f,Mode::Cached,Bad+16);
 f=Fixture{};f.Put(R+1,17,1);f.Put(R+24,Bad,8);Fault(f,Mode::Generic,Bad+32);
 f=Fixture{};f.Put(R+1,17,1);f.Put(O+32,Bad,8);Fault(f,Mode::Generic,Bad+8);
 f=Fixture{};f.Put(R,0,1);Fault(f,Mode::Generic,Bad,R,Bad,0,Bad);
 f=Fixture{};Fault(f,Mode::Generated,Bad,R,Bad);
}
struct Access{U64 ea;std::size_t bytes;};
void Prefix(Mode mode){
 ++groups;Fixture f;std::vector<Access> reads;
 U32 value=0x80000002u;
 if(mode==Mode::Cached)reads={{R+8,8},{T+12,2},{T+14,2},{R+16,8},{V+16,4}};
 else{
  reads={{R+1,1},{R,1},{R+8,8},{F,4},{T,4},{T+10,1},{T+12,2},{R+1,1}};
  if(mode==Mode::Generic)reads.insert(reads.end(),{{T+14,2},{R+16,8},{V+16,4}});
  else{
   f.Put(R+1,17,1);value=0x80000000u;
   reads.insert(reads.end(),{{R+24,8},{O+32,8},{O+32,8},{F,4},{C+8,4},{C,8},{D+32,4},
                            {C+8,4},{C,8},{D+32,4},{D+36,4},{R+16,8},{D+40,4},{V,8}});
  }
 }
 const auto before=f.bytes;
 for(std::size_t len=0;len<=0x10100;++len){
  const auto first=std::find_if(reads.begin(),reads.end(),[len](const Access&a){return a.ea-B+a.bytes>len;});++cases;
  try{
   // Generated mode in this frontier means the generic getter's generated route.
   const auto got=Call(f,mode==Mode::Generated?Mode::Generic:mode,len,R,F,0,Bad);
   Check(first==reads.end(),"Prefix missed expected first read");
   Check(got==value,"Prefix payload");
  }catch(const Monstey_GadbBinderFault&e){Check(first!=reads.end(),"Unexpected prefix fault");Check(e.address==first->ea,"Prefix first address");}
 }
 Check(f.bytes==before,"Getter wrote borrowed arena");
}
void Aliases(){
 ++groups;Fixture f;f.Put(T+12,0,2);
 ++cases;Check(f.Read(0,R)==0x101u,"Lazy default aliases record DWORD");
 ++cases;Check(f.Cached(0,0,T)==0x91234567u,"Cached fallback aliases descriptor DWORD");
 f=Fixture{};const auto before=f.bytes;
 for(U64 alias:{R,F,T,D,O,C,V,Z,Bad}){
  ++cases;Check(f.Read(0,alias)==0x80000002u,"Unused fallback alias");
 }
 Check(f.bytes==before,"Alias path mutated bytes");
}
}
int main(int argc,char**argv){
 try{
  if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("Expected new report path");
  Basic();TypeAndFlagMatrix();CachedBypass();CountAndOffset();Generated();GeneratedDirect();WidthAndFailures();
  Prefix(Mode::Generic);Prefix(Mode::Generated);Prefix(Mode::Cached);Aliases();
  std::ofstream out(argv[1],std::ios::binary);if(!out)throw std::runtime_error("Report create");
  out<<"{\"schema\":\"Monstey.IntegerGetterTests/1\",\"groups\":"<<groups<<",\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"passed\":true}\n";
  out.close();if(!out)throw std::runtime_error("Report write");
  std::cout<<"PASS "<<groups<<" groups "<<cases<<" cases "<<checks<<" checks\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
