#include "Monstey_GadbAudioNextKey.hpp"
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
constexpr U64 B=0x200000000ull,W=B+0x100,AC=B+0x200,KC=B+0x280;
constexpr U64 AF=B+0x400,KF=B+0x1000,AR=B+0x2000,KR=B+0x2100;
constexpr U64 AT=B+0x4000,KT=B+0x6000,AV=B+0x8000,KV=B+0x9000;
constexpr U64 AO=B+0xa000,AG=B+0xa100,AGF=B+0xb000;
constexpr U64 KO=B+0xc000,KG=B+0xc100,KGF=B+0xd000;
constexpr U64 AA=B+0xe000,KA=B+0xf000,Bad=B+0x100000;
constexpr U64 EndValue=0xf234567890abcdefull;
U64 checks=0,cases=0,groups=0;
void Check(bool v,const char* what){++checks;if(!v)throw std::runtime_error(what);}
struct Arena{
 std::vector<std::uint8_t> data=std::vector<std::uint8_t>(0x20000);
 void Put(U64 ea,U64 v,std::size_t n){
  if(ea<B||ea-B+n>data.size())throw std::runtime_error("Fixture range");
  for(std::size_t i=0;i<n;++i)data[static_cast<std::size_t>(ea-B)+i]=static_cast<std::uint8_t>(v>>(8*i));
 }
 Monstey_GadbBinderMemory Memory(){return{B,data};}
 U64 Run(){return Monstey_GadbReadAudioNextKey(Memory(),W,AC,KC);}
 Arena(){
  Put(AC,AF,8);Put(KC,KF,8);Put(W,AR,8);
  Put(AF+20*72+32,0x80000000,4);Put(KF+32,0xf0000000,4);
  for(auto r:{AR,KR}){Put(r,1,1);Put(r+1,1,1);}
  Put(AR+8,AT,8);Put(KR+8,KT,8);Put(AR+16,AV,8);Put(KR+16,KV,8);
  Put(AT,0x80000000,4);Put(KT,0xf0000000,4);
  for(auto t:{AT,KT}){Put(t+10,10,1);Put(t+12,1,2);Put(t+14,3,2);}
  Put(AV+24,KR,8);Put(KV+24,EndValue,8);
 }
 void State(U32 a,U32 k){Put(AF+20*72+48,a,4);Put(KF+48,k,4);}
};
void Fault(Arena& a,U64 expected,U64 w=W,U64 ac=AC,U64 kc=KC,std::size_t len=0){
 ++cases;try{
  auto memory=a.Memory();if(len)memory.bytes=memory.bytes.first(len);
  (void)Monstey_GadbReadAudioNextKey(memory,w,ac,kc);
  throw std::runtime_error("Expected first consumed fault absent");
 }catch(const Monstey_GadbBinderFault& e){
  Check(e.kind==Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable,"Wrong fault kind");
  Check(e.address==expected,"Wrong first consumed address");
 }
}
void SlowSearch(){
 ++groups;
 for(U32 n=1;n<=255;++n)for(bool adirect:{false,true})for(bool kdirect:{false,true}){
  Arena a;const auto selected=n/2;const U32 physical=n-1-selected;
  a.Put(AR,n,1);a.Put(AR+1,adirect?1:0,1);a.Put(KR+1,kdirect?1:0,1);
  a.Put(AF+20*72+32,0x80000000u+selected*0x100u,4);
  for(U32 i=0;i<n;++i){
   const auto row=AT+i*16ull;
   a.Put(row+(adirect?0:4),0x80000000u+i*0x100u,4);
   a.Put(row+9,n-1-i,1);a.Put(row+10,2,1);a.Put(row+12,0,2);
  }
  const auto match=AT+(adirect?selected:physical)*16ull;
  a.Put(match+10,0xca,1);a.Put(match+12,1,2);a.Put(match+14,3,2);
  if(!kdirect){a.Put(KT+4,0xf0000000,4);a.Put(KT+9,1,1);
   a.Put(KT+16+10,0x8a,1);a.Put(KT+16+12,1,2);a.Put(KT+16+14,3,2);}
  ++cases;Check(a.Run()==EndValue,"Unsigned direct/indirect nonzero-child search");
 }
}
void CacheAndSignedStates(){
 ++groups;Arena a;
 const std::array<U32,6> states{0x80000000u,UINT32_MAX,0,1,2,0x7fffffffu};
 for(U32 type=0;type<256;++type)for(U32 sa:states)for(U32 sk:states){
  a.State(sa,sk);a.Put(AT+10,type,1);a.Put(KT+10,type,1);
  const bool cacheA=sa==2||sa==0x7fffffffu,cacheK=sk==2||sk==0x7fffffffu;
  const bool typed=(type&63u)==10;
  const U64 expected=(cacheA||typed)&&(cacheK||typed)?EndValue:0;
  ++cases;Check(a.Run()==expected,"Signed state or cache type bypass");
 }
 a.State(2,2);a.Put(AR,0,1);a.Put(KR,0,1);
 a.Put(AR+1,0xff,1);a.Put(KR+1,0xff,1);
 a.Put(AF+20*72+32,0,4);a.Put(KF+32,0,4);
 ++cases;Check(a.Run()==EndValue,"Cache must bypass recordcount/flags/hash/type");
 a.Put(AF+20*72+64,255,1);a.Put(KF+64,255,1);
 a.Put(AT+255*16+12,1,2);a.Put(AT+255*16+14,0,2);
 a.Put(KT+255*16+12,1,2);a.Put(KT+255*16+14,0,2);
 a.Put(AV,KR,8);a.Put(KV,EndValue,8);
 ++cases;Check(a.Run()==EndValue,"Full U8 physicalcache255");
}
void FixedAndDynamicGenerated(){
 ++groups;
 const std::array<U32,19> types{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,255};
 for(U32 type:types)for(U32 flags:{0u,0x80u,0x200u,0x280u})
 for(U32 count:{0u,1u,2u,UINT32_MAX})for(U32 mode:{0u,1u,2u})for(bool first:{false,true}){
  Arena a;
  const auto record=first?AR:KR,owner=first?AO:KO,cls=first?AG:KG;
  const auto fields=first?AGF:KGF,values=first?AV:KV,array=first?AA:KA;
  a.Put(record+1,17,1);a.Put(record+24,owner,8);a.Put(owner+32,cls,8);
  a.Put(cls,fields,8);a.Put(cls+8,2,4);
  a.Put(fields+32,123,4);a.Put(fields+36,type,4);
  a.Put(fields+40,count,4);a.Put(fields+44,flags,4);
  a.Put(fields+72+32,first?0x80000000:0xf0000000,4);
  a.Put(fields+72+40,mode==0?0:UINT32_MAX,4);
  U32 size=0;
  if(!(flags&0x280u)){
   if(count!=1||type==9)size=16;
   else if(type==8)size=12;
   else if(type==1)size=5;
   else if(type>=2&&type<=17)size=8;
  }
  const U32 offset=(size+7u)&~7u;
  const auto value=first?KR:EndValue;
  if(mode==0)a.Put(values+offset,value,8);
  else {a.Put(values+offset,array,8);a.Put(values+offset+8,mode==1?1:0,4);a.Put(array,value,8);}
  ++cases;
  if(mode<2)Check(a.Run()==EndValue,"Generated prefix layout/fixed/dynamic child");
  else try{(void)a.Run();throw std::runtime_error("Expected dynamic empty assertion");}
  catch(const Monstey_GadbScalarFault& e){
   Check(e.kind==Monstey_GadbScalarFaultKind::NativeDynamicIndexAssertion,"Wrong assertion kind");
   Check(e.address==values+offset+8,"Wrong generated assertion storage");
  }
 }
}
void NullAndReadEpochs(){
 ++groups;Arena a;
 Fault(a,Bad,W,Bad,Bad);Fault(a,Bad,Bad,AC,Bad);
 a.Put(AC,Bad,8);a.Put(W,0,8);
 Fault(a,Bad+20*72+48);
 a=Arena{};a.Put(W,0,8);
 ++cases;Check(a.Run()==0,"Null parent still visits usable nextclass");
 Fault(a,Bad,W,AC,Bad);
 a.Put(KC,Bad,8);Fault(a,Bad+48);
 a=Arena{};a.State(2,0);a.Put(W,Bad,8);
 Fault(a,AF+20*72+64,W,AC,KC,0x400+20*72+52);
 a=Arena{};a.State(2,0);a.Put(AT+12,0,2);a.Put(AR+16,Bad,8);
 ++cases;Check(a.Run()==0,"Cached zero count skips value pointer");
 a=Arena{};a.Put(AR+8,Bad,8);a.Put(AR,0,1);
 ++cases;Check(a.Run()==0,"Slow zero recordcount reads Qidentity but not rows");
 a.Put(AR,1,1);a.Put(AC,Bad,8);
 Fault(a,Bad+20*72+48);
 a=Arena{};a.Put(AF+20*72+32,1,4);a.Put(AT+10,0xff,1);a.Put(AR+16,Bad,8);
 ++cases;Check(a.Run()==0,"Missing hash skips type and value");
}
void AliasesAndZeroResult(){
 ++groups;Arena a;const auto preimage=a.data;
 ++cases;Check(a.Run()==EndValue,"Baseline chain");
 Check(a.data==preimage,"No writes in normal chain");
 a.Put(KV+24,0,8);
 ++cases;Check(a.Run()==0,"Final zero result is valid");
 a=Arena{};a.Put(AV+24,0,8);a.Put(KR+8,Bad,8);
 ++cases;Check(a.Run()==0,"Null child skips childrecord memory");
 a=Arena{};a.Put(AV+24,Bad,8);Fault(a,Bad+1);
 a=Arena{};a.Put(AV+24,AR,8);a.Put(KF+32,0x80000000,4);
 ++cases;Check(a.Run()==AR,"A self-child alias is an ordinary two-stage raw chain");
 const auto after=a.data;(void)a.Run();Check(a.data==after,"Aliased chain did not mutate");
}
}
int main(int argc,char**argv){
 try{
  if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("Expected fresh output path");
  SlowSearch();CacheAndSignedStates();FixedAndDynamicGenerated();NullAndReadEpochs();AliasesAndZeroResult();
  std::ofstream out(argv[1],std::ios::binary);if(!out)throw std::runtime_error("Output failure");
  out<<"{\"schema\":\"Monstey.NextKeyIndependentTests/1\",\"groups\":"<<groups
     <<",\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"passed\":true}\n";
  out.close();if(!out)throw std::runtime_error("Output write failure");
  std::cout<<"PASS "<<groups<<" groups "<<cases<<" cases "<<checks<<" checks\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
