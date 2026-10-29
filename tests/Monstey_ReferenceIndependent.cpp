#include "Monstey_GadbReferenceWord.hpp"
#include "Monstey_GadbScalarFloat.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using U64=std::uint64_t;using U32=std::uint32_t;
U64 checks=0,cases=0,groups=0,rangeFailures=0,dynamicFailures=0;
void Require(bool ok,const char*why){++checks;if(!ok)throw std::runtime_error(why);}
struct State {
 static constexpr U64 base=0x400000,record=base+0x20,key=base+0x80,def=base+0xc0,
  table=base+0x200,values=base+0x1400,owner=base+0x2100,cl=base+0x2200,
  decls=base+0x2300,array=base+0x4000,slot=base+0x5000;
 static constexpr U32 hash=0x80012233;
 static constexpr U64 fallback=0xfa42000012345678ull,word=0x819283746576abceull;
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x7000,0x5c);
 void Set(U64 address,U64 value,std::size_t n){Require(address>=base&&address-base+n<=bytes.size(),"fixture range");for(std::size_t i=0;i<n;++i)bytes[static_cast<std::size_t>(address-base)+i]=static_cast<std::uint8_t>(value>>(i*8));}
 Monstey_GadbBinderMemory View(){return{base,bytes};}
 U64 Read(U32 index=0,U64 fallbackAddress=def){return Monstey_GadbReadReferenceWord(View(),record,key,index,fallbackAddress);}
 U64 Generated(U32 index=0,U64 fallbackAddress=def){return Monstey_GadbReadGeneratedReferenceWord(View(),record,key,index,fallbackAddress);}
 void Decl(U32 i,U32 h,U32 type,U32 count,U32 flags){const auto row=decls+static_cast<U64>(i)*72;Set(row+32,h,4);Set(row+36,type,4);Set(row+40,count,4);Set(row+44,flags,4);}
 void Init(bool generated=false,bool indirect=false){
  Set(record,1,1);Set(record+1,(generated?0x10u:0u)|(indirect?0u:1u),1);
  Set(record+8,table,8);Set(record+16,values,8);Set(record+24,owner,8);
  Set(key,hash,4);Set(def,fallback,8);Set(table+(indirect?4:0),hash,4);
  Set(table+9,0,1);Set(table+10,0xca,1);Set(table+12,16,2);Set(table+14,3,2);
  Set(values+3*8,word,8);Set(owner+32,cl,8);Set(cl,decls,8);Set(cl+8,1,4);
  Decl(0,hash,10,1,0);Set(values,word,8);
 }
};
void ExpectFault(auto call,U64 address){
 ++cases;++rangeFailures;bool caught=false;
 try{(void)call();}catch(const Monstey_GadbBinderFault&e){caught=true;Require(e.kind==Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable,"range kind");Require(e.address==address,"first consumed address");}
 Require(caught,"expected owned fault");
}
void WidthAndBounds(){
 ++groups;State f;f.Init();
 for(U32 type=0;type<256;++type)for(U32 n:{0u,1u,17u,65535u})
  for(U32 i:{0u,1u,2u,16u,17u,65534u,65535u,0x80000000u,UINT32_MAX}){
   f.Set(f.table+10,type,1);f.Set(f.table+12,n,2);const U32 first=65535;
   f.Set(f.table+14,first,2);const U32 element=first+i;
   f.Set(f.record+16,f.slot-8ull*element,8);f.Set(f.slot,f.word,8);
   ++cases;Require(f.Read(i)==(((type&63u)==10&&i<n)?f.word:f.fallback),"type/count/index/full Qword");
  }
 for(U64 raw:{0ull,1ull,0x8000000000000000ull,UINT64_MAX}){
  f.Init();f.Set(f.values+3*8,raw,8);++cases;Require(f.Read(0,0)==raw,"raw success must not use invalid default");
 }
 f.Init();++cases;Require(Monstey_GadbReadReferenceWord(f.View(),0,0,UINT32_MAX,f.def)==f.fallback,"null record ignores field/index");
 ExpectFault([&]{return Monstey_GadbReadReferenceWord(f.View(),0,0,0,f.base+f.bytes.size()-4);},f.base+f.bytes.size()-4);
}
void SearchAndFallback(){
 ++groups;
 for(bool indirect:{false,true})for(U32 n=1;n<=255;++n)for(U32 pos:{0u,n/2,n-1}){
  State f;f.Init(false,indirect);f.Set(f.record,n,1);
  for(U32 j=0;j<n;++j){f.Set(f.table+16ull*j+(indirect?4:0),j*0x1010101u,4);if(indirect)f.Set(f.table+16ull*j+9,(j+19)%256,1);}
  f.Set(f.key,pos*0x1010101u,4);const U32 physical=indirect?(pos+19)%256:pos;
  f.Set(f.table+16ull*physical+10,10,1);f.Set(f.table+16ull*physical+12,16,2);f.Set(f.table+16ull*physical+14,3,2);
  ++cases;Require(f.Read()==f.word,"direct/indirect lookup composed");
 }
 for(U32 mode=0;mode<5;++mode){State f;f.Init();if(mode==0)f.Set(f.record,0,1);if(mode==1)f.Set(f.key,1,4);if(mode==2)f.Set(f.table+10,2,1);if(mode==3)f.Set(f.table+12,0,2);if(mode==4)f.Set(f.record+8,0,8);
  if(mode<4){++cases;Require(f.Read()==f.fallback,"fallback gate");ExpectFault([&]{return f.Read(0,0);},0);}else ExpectFault([&]{return f.Read();},0);
 }
}
U32 Size(U32 type,U32 count,U32 flags){
 static constexpr std::array<U32,18> widths={0,5,8,8,8,8,8,8,12,16,8,8,8,8,8,8,8,8};
 if((flags&0x280u)!=0)return 0;
 return count==1?(type<widths.size()?widths[type]:0):16;
}
void GeneratedStorage(){
 ++groups;
 for(bool indirect:{false,true})for(U32 type=0;type<24;++type)
  for(U32 count:{0u,1u,2u,UINT32_MAX})for(U32 flags:{0u,0x80u,0x200u,0x400u})for(U32 i:{0u,1u,3u,7u}){
   State f;f.Init(true,indirect);f.Set(f.cl+8,3,4);
   f.Decl(0,f.hash-1,type,count,flags);f.Decl(1,f.hash-2,1,1,0);
   f.Decl(2,f.hash,10,0,0);const U32 prefix=((Size(type,count,flags)+7u)/8u*8u)+8u;
   f.Set(f.values+prefix+8ull*i,f.word,8);const auto before=f.bytes;
   ++cases;Require(f.Read(i,0)==f.word,"generated prefix size/fixed count0");
   Require(f.bytes==before,"generated borrowed source modified");
  }
 for(U32 type:{8u,9u,10u,UINT32_MAX})for(U32 i:{0u,1u,7u,0x80000000u,UINT32_MAX}){
  State f;f.Init(true);f.Decl(0,f.hash,type,1,0);U32 element=i;if(type==8||type==9)element+=element;
  f.Set(f.record+16,f.slot-8ull*element,8);f.Set(f.slot,f.word,8);++cases;
  Require(f.Generated(i,0)==f.word,"generated fullU32/doubled index");
 }
 for(U32 index:{0u,1u,7u})for(U32 size:{0u,1u,8u,0x80000000u,UINT32_MAX}){
  State f;f.Init(true);f.Decl(0,f.hash,10,UINT32_MAX,0);f.Set(f.values,f.array,8);f.Set(f.values+8,size,4);f.Set(f.array+8ull*index,f.word,8);
  ++cases;if(index<size)Require(f.Generated(index,0)==f.word,"dynamic unsigned count/index");
  else {bool caught=false;try{(void)f.Generated(index,0);}catch(const Monstey_GadbScalarFault&e){caught=true;++dynamicFailures;Require(e.kind==Monstey_GadbScalarFaultKind::NativeDynamicIndexAssertion&&e.address==f.values+8,"dynamic assertion");}Require(caught,"dynamic assertion missing");}
 }
}
void GeneratedFallbackAndEpochs(){
 ++groups;
 for(U32 mode=0;mode<5;++mode){State f;f.Init(true);
  if(mode==0)f.Set(f.record+24,0,8);if(mode==1)f.Set(f.owner+32,0,8);
  if(mode==2)f.Set(f.cl+8,0,4);if(mode==3)f.Decl(0,f.hash+1,10,1,0);
  if(mode==4)f.Set(f.values,0,8);
  ++cases;Require(f.Generated()==(mode==4?0:f.fallback),"generated unavailable versus zero");
  if(mode<2){++cases;Require(Monstey_GadbReadGeneratedReferenceWord(f.View(),f.record,0,0,f.def)==f.fallback,"class unavailable must ignore field");}
 }
 State f;f.Init(true);f.Set(f.cl+8,0,4);
 ExpectFault([&]{return Monstey_GadbReadGeneratedReferenceWord(f.View(),f.record,0,0,f.def);},0);
 f.Init(true);f.Set(f.record+24,0,8);ExpectFault([&]{return f.Generated(0,0);},0);
 f.Init(true);f.Set(f.owner+32,1,8);ExpectFault([&]{return f.Generated();},9);
 f.Init(true);f.Set(f.cl+8,2,4);f.Decl(0,1,1,1,0);f.Decl(1,f.hash,10,UINT32_MAX,0);
 f.Set(f.values+8,0,8);f.Set(f.values+16,0,4);bool caught=false;++cases;
 try{(void)f.Generated(0,0);}catch(const Monstey_GadbScalarFault&e){caught=true;++dynamicFailures;Require(e.address==f.values+16,"dynamic check before array/fallback");}Require(caught,"dynamic zero gate");
}
struct ReadRow{U64 address;std::size_t n;};
void Frontier(State&f,const std::vector<ReadRow>&reads,bool generated,U32 index,U64 expected){
 const auto before=f.bytes;
 for(std::size_t end=0;end<=f.bytes.size();++end){bool bad=false;U64 address=0;
  for(const auto&r:reads)if(r.address<f.base||r.address-f.base>end||r.n>end-static_cast<std::size_t>(r.address-f.base)){bad=true;address=r.address;break;}
  const auto call=[&]{auto view=Monstey_GadbBinderMemory{f.base,std::span(f.bytes).first(end)};return generated?Monstey_GadbReadGeneratedReferenceWord(view,f.record,f.key,index,f.def):Monstey_GadbReadReferenceWord(view,f.record,f.key,index,f.def);};
  if(bad)ExpectFault(call,address);else{++cases;Require(call()==expected,"complete frontier value");}
 }
 Require(f.bytes==before,"frontier wrote borrowed arena");
}
void ReadFrontiers(){
 ++groups;State f;f.Init();
 Frontier(f,{{f.record+1,1},{f.record,1},{f.record+8,8},{f.key,4},{f.table,4},{f.table+10,1},{f.table+12,2},{f.record+1,1},{f.table+14,2},{f.record+16,8},{f.values+3*8,8}},false,0,f.word);
 f.Init(false,true);
 Frontier(f,{{f.record+1,1},{f.record,1},{f.record+8,8},{f.table+4,4},{f.key,4},{f.table+9,1},{f.table+10,1},{f.table+12,2},{f.record+1,1},{f.table+14,2},{f.record+16,8},{f.values+3*8,8}},false,0,f.word);
 f.Init(true);
 Frontier(f,{{f.record+24,8},{f.owner+32,8},{f.owner+32,8},{f.key,4},{f.cl+8,4},{f.cl,8},{f.decls+32,4},{f.cl+8,4},{f.cl,8},{f.decls+32,4},{f.decls+36,4},{f.record+16,8},{f.decls+40,4},{f.values,8}},true,0,f.word);
 f.Init(true);f.Set(f.owner+32,0,8);
 Frontier(f,{{f.record+24,8},{f.owner+32,8},{f.def,8}},true,0,f.fallback);
}
void AliasAndLazyDefault(){
 ++groups;State f;f.Init();f.Set(f.record+16,f.key-3*8,8);
 const auto before=f.bytes;++cases;Require(f.Read(0,0)==(0x5c5c5c5c00000000ull|f.hash),"raw value borrows field bytes");Require(f.bytes==before,"raw alias modified");
 f.Init(true);f.Set(f.record+16,f.def,8);++cases;Require(f.Generated(0,0)==f.fallback,"generated value aliases default without default dereference");
 f.Init();f.Set(f.def,0,8);f.Set(f.table+12,0,2);++cases;Require(f.Read()==0,"fallback zero valid");
 f.Init();const auto tail=f.base+f.bytes.size()-2;f.Set(tail,0,1);f.Set(tail+1,1,1);
 ExpectFault([&]{return Monstey_GadbReadReferenceWord(f.View(),tail,f.key,0,f.def);},tail+8);
 f.Init(true);f.Set(f.owner+32,0,8);ExpectFault([&]{return f.Generated(0,f.base+f.bytes.size()-7);},f.base+f.bytes.size()-7);
}
}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("Usage: REFERENCE_INDEPENDENT NEW_REPORT");
 WidthAndBounds();SearchAndFallback();GeneratedStorage();GeneratedFallbackAndEpochs();ReadFrontiers();AliasAndLazyDefault();
 const std::string text="{\"status\":\"PASS\",\"groups\":"+std::to_string(groups)+",\"cases\":"+std::to_string(cases)+",\"checks\":"+std::to_string(checks)+",\"range_failures\":"+std::to_string(rangeFailures)+",\"dynamic_failures\":"+std::to_string(dynamicFailures)+"}\n";
 std::ofstream out(argv[1],std::ios::binary);out<<text;Require(static_cast<bool>(out),"write output");std::cout<<text;return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
