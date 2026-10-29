#include "Monstey_GadbSelectedCountWord.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using U64=std::uint64_t;using U32=std::uint32_t;
U64 groups=0,cases=0,checks=0,rangeCases=0,dynamicCases=0;
void Need(bool ok,const char*why){++checks;if(!ok)throw std::runtime_error(why);}
constexpr std::array<Monstey_GadbSelectedCountField,7> Fields={
 Monstey_GadbSelectedCountField::PresentationAdvanceOnItem,Monstey_GadbSelectedCountField::PresentationDissolve,
 Monstey_GadbSelectedCountField::PresentationAOE,Monstey_GadbSelectedCountField::PresentationActionsOnStart,
 Monstey_GadbSelectedCountField::PresentationActionsOnEnd,Monstey_GadbSelectedCountField::NameEventCustomDetailWidgetData,
 Monstey_GadbSelectedCountField::QuestPredicates};
constexpr std::array<U32,7> Ordinal={87,117,118,119,120,37,73};
struct Input {
 static constexpr U64 base=0x10000000,p=base+0x10,n=base+0x20,q=base+0x30,
  pt=base+0x1000,nt=base+0x4000,qt=base+0x7000,record=base+0xa000,
  descriptors=base+0xb000,values=base+0xc000,owner=base+0xd000,
  generated=base+0xd100,declarations=base+0xe000,array=base+0xf000,slot=base+0x11000;
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x13000,0x6b);
 Monstey_GadbSelectedCountClasses classes{p,n,q};
 Monstey_GadbBinderMemory View(){return{base,bytes};}
 void Set(U64 a,U64 v,std::size_t w){Need(a>=base&&a-base+w<=bytes.size(),"fixture range");for(std::size_t i=0;i<w;++i)bytes[static_cast<std::size_t>(a-base)+i]=static_cast<std::uint8_t>(v>>(8*i));}
 U64 Table(U32 i)const{return i<5?pt:i==5?nt:qt;}
 U64 Field(U32 i)const{return Table(i)+72ull*Ordinal[i];}
 U32 Hash(U32 i)const{return 0x80001100u+i;}
 U64 Answer(U32 i)const{return 0xabcd000012340000ull+i*0x100000001ull;}
 U64 Run(U32 i,U64 rec=record){return Monstey_GadbReadSelectedCountWord(View(),rec,Fields[i],classes);}
 void Init(){
  Set(p,pt,8);Set(n,nt,8);Set(q,qt,8);Set(record,7,1);Set(record+1,1,1);
  Set(record+8,descriptors,8);Set(record+16,values,8);Set(record+24,owner,8);
  for(U32 i=0;i<7;++i){Set(Field(i)+32,Hash(i),4);Set(Field(i)+48,2,4);Set(Field(i)+64,7+i,1);
   Set(descriptors+16ull*i,Hash(i),4);Set(descriptors+16ull*i+10,0xca,1);Set(descriptors+16ull*i+12,1,2);Set(descriptors+16ull*i+14,11+i,2);
   Set(descriptors+16ull*(7+i)+10,0,1);Set(descriptors+16ull*(7+i)+12,1,2);Set(descriptors+16ull*(7+i)+14,11+i,2);
   Set(values+8ull*(11+i),Answer(i),8);
  }
  Set(owner+32,generated,8);Set(generated,declarations,8);Set(generated+8,1,4);
 }
 void Decl(U32 index,U32 hash,U32 type,U32 count,U32 flags){const U64 row=declarations+72ull*index;Set(row+32,hash,4);Set(row+36,type,4);Set(row+40,count,4);Set(row+44,flags,4);}
};
void Range(auto call,U64 address){++cases;++rangeCases;bool caught=false;try{(void)call();}catch(const Monstey_GadbBinderFault&e){caught=true;Need(e.kind==Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable,"fault kind");Need(e.address==address,"fault read order/address");}Need(caught,"missing range fault");}
void FieldAndState(){
 ++groups;for(U32 i=0;i<7;++i)for(auto state:{INT32_MIN,-1,0,1,2,3,INT32_MAX}){
  Input f;f.Init();f.Set(f.Field(i)+48,std::bit_cast<U32>(state),4);
  if(state<2)f.Set(f.descriptors+16ull*(7+i)+12,0,2);else f.Set(f.descriptors+16ull*i+10,0,1);
  const auto before=f.bytes;++cases;Need(f.Run(i)==f.Answer(i),"field/global/ordinal/signed route");Need(f.bytes==before,"borrowed bytes modified");
 }
 for(U32 i=0;i<7;++i){Input f;f.Init();f.Set(f.Field(i)+48,0,4);f.Set(f.record+1,0,1);
  for(U32 j=0;j<7;++j){f.Set(f.descriptors+16ull*j+4,f.Hash(j),4);f.Set(f.descriptors+16ull*j+9,40+j,1);
   f.Set(f.descriptors+16ull*(40+j)+10,10,1);f.Set(f.descriptors+16ull*(40+j)+12,1,2);f.Set(f.descriptors+16ull*(40+j)+14,11+j,2);}
  ++cases;Need(f.Run(i)==f.Answer(i),"indirect physical mapping per field");
 }
}
void CachedPrivateZero(){
 ++groups;for(U32 i=0;i<7;++i)for(U32 physical=0;physical<256;++physical){
  Input f;f.Init();f.Set(f.Field(i)+64,physical,1);f.Set(f.descriptors+16ull*physical+12,65535,2);
  f.Set(f.descriptors+16ull*physical+14,65535,2);f.Set(f.record+16,f.slot-8ull*65535,8);f.Set(f.slot,f.Answer(i),8);
  f.Set(f.record,0,1);f.Set(f.record+1,0x10,1);++cases;
  Need(f.Run(i)==f.Answer(i),"cached byte/count/type/flags bypass and U16 offset");
 }
 for(U32 i=0;i<7;++i){Input f;f.Init();++cases;Need(f.Run(i,0)==0,"cached null privatezero");
  const auto end=static_cast<std::size_t>(f.Field(i)+52-f.base);
  Range([&]{return Monstey_GadbReadSelectedCountWord({f.base,std::span(f.bytes).first(end)},0,Fields[i],f.classes);},f.Field(i)+64);
  f.Set(f.Field(i)+48,1,4);++cases;
  Need(Monstey_GadbReadSelectedCountWord({f.base,std::span(f.bytes).first(end)},0,Fields[i],f.classes)==0,"slow null must ignore cache byte");
  f.Init();f.Set(f.descriptors+16ull*(7+i)+12,0,2);f.Set(f.record+16,0,8);++cases;Need(f.Run(i)==0,"cached countzero privatezero");
 }
 Input f;f.Init();f.classes.presentation=1;Range([&]{return f.Run(0,0);},1);
}
void SlowTypeAndLookup(){
 ++groups;for(U32 i=0;i<7;++i)for(U32 type=0;type<256;++type){Input f;f.Init();f.Set(f.Field(i)+48,0,4);f.Set(f.descriptors+16ull*i+10,type,1);++cases;
  Need(f.Run(i)==((type&63u)==10?f.Answer(i):0),"masked type10/private zero");}
 for(bool indirect:{false,true})for(U32 count=1;count<=255;++count)for(U32 pos:{0u,count/2,count-1}){
  Input f;f.Init();f.Set(f.Field(0)+48,0,4);f.Set(f.record,count,1);f.Set(f.record+1,indirect?0:1,1);
  for(U32 j=0;j<count;++j){f.Set(f.descriptors+16ull*j+(indirect?4:0),j*0x1010101u,4);if(indirect)f.Set(f.descriptors+16ull*j+9,(j*37+9)%256,1);}
  f.Set(f.Field(0)+32,pos*0x1010101u,4);U32 physical=indirect?(pos*37+9)%256:pos;
  f.Set(f.descriptors+16ull*physical+10,10,1);f.Set(f.descriptors+16ull*physical+12,1,2);f.Set(f.descriptors+16ull*physical+14,11,2);
  ++cases;Need(f.Run(0)==f.Answer(0),"unsigned sorted count/indirect remap");
 }
 for(U32 i=0;i<7;++i)for(U32 gate=0;gate<4;++gate){Input f;f.Init();f.Set(f.Field(i)+48,0,4);
  if(gate==0)f.Set(f.record,0,1);if(gate==1)f.Set(f.Field(i)+32,0xffffffff,4);
  if(gate==2)f.Set(f.descriptors+16ull*i+12,0,2);if(gate==3)f.Set(f.record+24,0,8);
  if(gate==3)f.Set(f.record+1,0x11,1);++cases;Need(f.Run(i)==0,"slow privatezero gate");
 }
}
U32 Cost(U32 t,U32 n,U32 flags){constexpr std::array<U32,18> widths={0,5,8,8,8,8,8,8,12,16,8,8,8,8,8,8,8,8};return flags&0x280u?0:((n==1?(t<widths.size()?widths[t]:0):16)+7)/8*8;}
void GeneratedPrivateZero(){
 ++groups;for(U32 i=0;i<7;++i)for(U32 t:{0u,1u,2u,8u,9u,10u,17u,18u,UINT32_MAX})for(U32 n:{0u,1u,2u,UINT32_MAX})for(U32 flags:{0u,0x80u,0x200u,0x40u}){
  Input f;f.Init();f.Set(f.Field(i)+48,0,4);f.Set(f.record+1,0x11,1);f.Set(f.generated+8,2,4);
  f.Decl(0,f.Hash(i)-1,t,n,flags);f.Decl(1,f.Hash(i),t,0,0);
  f.Set(f.values+Cost(t,n,flags),f.Answer(i),8);++cases;Need(f.Run(i)==f.Answer(i),"generated size/marker/fixedcount0");
 }
 for(U32 i=0;i<7;++i)for(U32 count:{0u,1u,0x80000000u,UINT32_MAX}){Input f;f.Init();f.Set(f.Field(i)+48,0,4);f.Set(f.record+1,0x11,1);
  f.Decl(0,f.Hash(i),10,UINT32_MAX,0);f.Set(f.values,f.array,8);f.Set(f.values+8,count,4);f.Set(f.array,f.Answer(i),8);++cases;
  if(count!=0)Need(f.Run(i)==f.Answer(i),"dynamic unsigned first word");else{bool caught=false;try{(void)f.Run(i);}catch(const Monstey_GadbScalarFault&e){caught=true;++dynamicCases;Need(e.address==f.values+8,"dynamic fault address");}Need(caught,"empty dynamic fault missing");}
 }
 for(U32 i=0;i<7;++i)for(U32 gate=0;gate<4;++gate){Input f;f.Init();f.Set(f.Field(i)+48,0,4);f.Set(f.record+1,0x11,1);f.Decl(0,f.Hash(i),10,1,0);
  if(gate==0)f.Set(f.record+24,0,8);if(gate==1)f.Set(f.owner+32,0,8);if(gate==2)f.Set(f.generated+8,0,4);if(gate==3)f.Decl(0,f.Hash(i)+1,10,1,0);
  ++cases;Need(f.Run(i)==0,"generated missing privatezero");
 }
}
struct ReadRow{U64 a;std::size_t n;};
void Frontier(Input&f,U32 selected,U64 record,const std::vector<ReadRow>&reads,U64 answer){const auto before=f.bytes;
 for(std::size_t end=0;end<=f.bytes.size();++end){bool bad=false;U64 first=0;
  for(const auto&r:reads)if(r.a<f.base||r.a-f.base>end||r.n>end-static_cast<std::size_t>(r.a-f.base)){bad=true;first=r.a;break;}
  const auto call=[&]{return Monstey_GadbReadSelectedCountWord({f.base,std::span(f.bytes).first(end)},record,Fields[selected],f.classes);};
  if(bad)Range(call,first);else{++cases;Need(call()==answer,"read frontier answer");}}
 Need(f.bytes==before,"frontier memory written");}
void ReadOrder(){
 ++groups;Input f;f.Init();const auto field=f.Field(0);
 Frontier(f,0,0,{{f.p,8},{field+48,4},{f.p,8},{field+64,1}},0);
 Frontier(f,0,f.record,{{f.p,8},{field+48,4},{f.p,8},{field+64,1},{f.record+8,8},{f.descriptors+7*16+12,2},{f.descriptors+7*16+14,2},{f.record+16,8},{f.values+11*8,8}},f.Answer(0));
 f.Init();f.Set(field+48,0,4);f.Set(f.record,1,1);
 Frontier(f,0,f.record,{{f.p,8},{field+48,4},{f.p,8},{f.record+1,1},{f.record,1},{f.record+8,8},{field+32,4},{f.descriptors,4},{f.descriptors+10,1},{f.descriptors+12,2},{f.record+1,1},{f.descriptors+14,2},{f.record+16,8},{f.values+11*8,8}},f.Answer(0));
 f.Init();f.Set(field+48,0,4);f.Set(f.record,1,1);f.Set(f.record+1,0,1);f.Set(f.descriptors+4,f.Hash(0),4);f.Set(f.descriptors+9,0,1);
 Frontier(f,0,f.record,{{f.p,8},{field+48,4},{f.p,8},{f.record+1,1},{f.record,1},{f.record+8,8},{f.descriptors+4,4},{field+32,4},{f.descriptors+9,1},{f.descriptors+10,1},{f.descriptors+12,2},{f.record+1,1},{f.descriptors+14,2},{f.record+16,8},{f.values+11*8,8}},f.Answer(0));
 f.Init();f.Set(field+48,0,4);f.Set(f.record,1,1);f.Set(f.record+1,0x11,1);f.Decl(0,f.Hash(0),10,1,0);f.Set(f.values,f.Answer(0),8);
 Frontier(f,0,f.record,{{f.p,8},{field+48,4},{f.p,8},{f.record+1,1},{f.record,1},{f.record+8,8},{field+32,4},{f.descriptors,4},{f.descriptors+10,1},{f.descriptors+12,2},{f.record+1,1},{f.record+24,8},{f.owner+32,8},{f.owner+32,8},{field+32,4},{f.generated+8,4},{f.generated,8},{f.declarations+32,4},{f.generated+8,4},{f.generated,8},{f.declarations+32,4},{f.declarations+36,4},{f.record+16,8},{f.declarations+40,4},{f.values,8}},f.Answer(0));
}
void AliasAndFailures(){
 ++groups;for(U32 i=0;i<7;++i){Input f;f.Init();f.Set(f.record+16,f.Field(i)-8ull*(11+i),8);f.Set(f.Field(i),f.Answer(i),8);
  ++cases;Need(f.Run(i)==f.Answer(i),"cached values borrow metadata");
  f.Init();f.Set(f.Field(i)+48,0,4);f.Set(f.record,1,1);f.Set(f.descriptors,f.Hash(i),4);f.Set(f.descriptors+10,10,1);f.Set(f.descriptors+12,1,2);f.Set(f.descriptors+14,0,2);f.Set(f.record+16,f.slot,8);f.Set(f.slot,0,8);
  ++cases;Need(f.Run(i)==0,"zero raw result valid");
 }
 Input f;f.Init();f.Set(f.Field(0)+48,0,4);f.Set(f.record+1,0,1);f.Set(f.record,1,1);f.Set(f.record+8,1,8);
 // Indirect candidate precedes wanted hash; missing candidate wins even if
 // the selected hash would be inaccessible in a differently based arena.
 Range([&]{return f.Run(0);},5);
 bool bad=false;++cases;try{(void)Monstey_GadbReadSelectedCountWord(f.View(),0,static_cast<Monstey_GadbSelectedCountField>(99),f.classes);}catch(const std::invalid_argument&){bad=true;}Need(bad,"invalid owned selector accepted");
}
}
int main(int argc,char**argv){try{if(argc!=2)throw std::runtime_error("Usage: SELECTED_INDEPENDENT NEW_REPORT");FieldAndState();CachedPrivateZero();SlowTypeAndLookup();GeneratedPrivateZero();ReadOrder();AliasAndFailures();
 const std::string text="{\"status\":\"PASS\",\"groups\":"+std::to_string(groups)+",\"cases\":"+std::to_string(cases)+",\"checks\":"+std::to_string(checks)+",\"range_cases\":"+std::to_string(rangeCases)+",\"dynamic_cases\":"+std::to_string(dynamicCases)+"}\n";
 std::ofstream out(argv[1],std::ios::binary);out<<text;Need(static_cast<bool>(out),"write report");std::cout<<text;return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
