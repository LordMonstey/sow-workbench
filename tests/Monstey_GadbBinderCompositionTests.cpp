#include "Monstey_GadbBinderComposition.hpp"
#include "Monstey_GadbClassCache.hpp"
#include "Monstey_Core.hpp"
#include "Monstey_GadbHashTableReference.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <string>
using namespace Monstey;
namespace {
using U64=std::uint64_t; using U32=std::uint32_t; using J=Monstey_Json;
std::uint64_t cases=0,checks=0;unsigned groups=0;
void Check(bool p,const char *why){++checks;if(!p)throw std::runtime_error(why);}
struct Image {
 U64 base=0x100000;std::vector<std::uint8_t>b;
 explicit Image(std::size_t n=65536):b(n,0xa5){}
 U64 A(U64 o)const{return base+o;}
 void W(U64 a,U64 v,std::size_t n){const auto o=static_cast<std::size_t>(a-base);if(a<base||o>b.size()||n>b.size()-o)throw std::runtime_error("Bad test store");for(std::size_t i=0;i<n;++i)b[o+i]=static_cast<std::uint8_t>(v>>(8*i));}
 U64 R(U64 a,std::size_t n)const{U64 v=0;const auto o=static_cast<std::size_t>(a-base);if(a<base||o>b.size()||n>b.size()-o)throw std::runtime_error("Bad test load");for(std::size_t i=0;i<n;++i)v|=static_cast<U64>(b[o+i])<<(8*i);return v;}
 void Text(U64 a,std::string_view s){for(const unsigned char c:s){W(a,c,1);++a;}W(a,0,1);}
 void Cl(U64 cl,U64 table,U32 count,U64 parent=0,U32 flags=0,std::uint8_t initialized=0){W(cl,table,8);W(cl+8,count,4);W(cl+0x28,parent,8);W(cl+0x38,flags,4);W(cl+0x3c,initialized,1);}
 void Field(U64 f,U64 name,U32 hash,U32 type,U32 count,U32 flags,U32 state,U64 code,std::uint8_t cache){W(f,name,8);W(f+0x20,hash,4);W(f+0x24,type,4);W(f+0x28,count,4);W(f+0x2c,flags,4);W(f+0x30,state,4);W(f+0x38,code,8);W(f+0x40,cache,1);}
 void Desc(U64 d,U32 hash,std::uint8_t identity,std::uint8_t type,std::uint16_t count){W(d,hash,4);W(d+8,identity,1);W(d+0xa,type,1);W(d+0xc,count,2);}
 void Category(U64 cat,U32 named,U64 first,U32 anonymous=0,U64 anon=0){W(cat+0x28,named,4);W(cat+0x38,first,8);W(cat+0x40,anonymous,4);W(cat+0x48,anon,8);}
 Monstey_GadbBinderMemory View(){return {base,b};}
};
using EK=Monstey_GadbBinderEventKind;using FK=Monstey_GadbBinderFaultKind;
std::size_t Events(const Monstey_GadbBinderTrace&t,EK k){return static_cast<std::size_t>(std::count_if(t.events.begin(),t.events.end(),[&](const auto&e){return e.kind==k;}));}
template<class F> void Fault(F&&f,FK k,U64 a){bool got=false;try{f();}catch(const Monstey_GadbBinderFault&e){got=true;Check(e.kind==k,"Wrong consumed fault kind");Check(e.address==a,"Wrong native consumed fault address");}Check(got,"Expected consumed fault missing");}
void Initializers(){
 ++groups;
 for(U32 flags: {0u,4u,0x80000000u})for(U32 init:{0u,1u,255u}){
  Image m;const auto cl=m.A(0x100),par=m.A(0x180),tab=m.A(0x1000),pt=m.A(0x2000);m.Cl(cl,tab,6,par,flags,static_cast<std::uint8_t>(init));m.Cl(par,pt,3);
  for(U32 i=0;i<6;++i)m.Field(tab+72*i,0,10+i,2,1,i==1?0x80u:i==4?0x200u:0,0x80000000u+i,0,0xde);
  for(U32 i=0;i<3;++i)m.Field(pt+72*i,0,10+i,2,1,i==1?0x280u:0,9+i,0,0xbc);
  auto expected=m; if(init==0){expected.W(par+0x3c,1,1);expected.W(pt+0x40,0,1);expected.W(pt+144+0x40,1,1);if(!(flags&4)){U32 slot=0;for(U32 i=0;i<6;++i)if(i!=1&&i!=4)expected.W(tab+72*i+0x40,slot++,1);}expected.W(cl+0x3c,1,1);}
  Monstey_GadbBinderTrace t;Monstey_GadbInitializeSelectedClass(m.View(),cl,t);Check(m.b==expected.b,"Parent-first initializer bytes/state diverge");++cases;
 }
 {Image m(131072);const auto cl=m.A(0x100),tab=m.A(0x1000);m.Cl(cl,tab,300);for(U32 i=0;i<300;++i){m.W(tab+72*i+0x2c,0,4);m.W(tab+72*i+0x30,0x11223344,4);}auto expected=m;for(U32 i=0;i<300;++i)expected.W(tab+72*i+0x40,i&255,1);expected.W(cl+0x3c,1,1);Monstey_GadbBinderTrace t;Monstey_GadbInitializeSelectedClass(m.View(),cl,t);Check(m.b==expected.b,"Compact counter U8 wrapping differs");Check(Events(t,EK::CacheWrite)==300,"Initializer count narrowed to U8");++cases;}
 // Live class-count reload: first cache byte aliases its class count low byte.
 {Image m;const auto cl=m.A(0x200),tab=cl-0x38;m.W(tab+0x2c,0,4);m.Cl(cl,tab,3);auto expected=m;expected.W(cl+8,0,1);expected.W(cl+0x3c,1,1);Monstey_GadbBinderTrace t;Monstey_GadbInitializeSelectedClass(m.View(),cl,t);Check(m.b==expected.b,"Initializer cached a live native count");Check(Events(t,EK::CacheWrite)==1,"Initializer failed to stop at changed count");++cases;}
 {Image m;auto cl=m.A(0x100);m.Cl(cl,0,0);m.W(cl+0x30,0xffffffffffffffffull,8);Monstey_GadbBinderTrace t;Monstey_GadbInitializeSelectedClass(m.View(),cl,t);Check(m.R(cl+0x3c,1)==1,"Opaque class+30 incorrectly consumed");++cases;}
 {Image m;auto cl=m.A(0x100);m.Cl(cl,0,0,cl);Monstey_GadbBinderTrace t;Fault([&]{Monstey_GadbInitializeSelectedClass(m.View(),cl,t,15);},FK::OwnedStepLimit,cl+0x28);Check(m.R(cl+0x3c,1)==0,"Cycle was fabricated initialized");++cases;}
}
struct Basic {
 Image m;U64 cl,tab,cat,rec,desc,name,code;U32 hash;
 Basic():cl(m.A(0x100)),tab(m.A(0x1000)),cat(m.A(0x300)),rec(m.A(0x500)),desc(m.A(0x6000)),name(m.A(0x4000)),code(m.A(0x5000)),hash(Monstey_NativeHash("Alpha")){
  m.Text(name,"Alpha");m.W(code,0xb0,1);m.W(code+1,0,1);m.W(code+2,0xc3,1);m.Cl(cl,tab,1);m.Field(tab,name,hash,2,1,0,0,code,0xbe);m.Category(cat,1,rec);m.W(rec,1,1);m.W(rec+8,desc,8);m.Desc(desc,hash,0,2,1);
 }
};
void PredicateMatrix(){
 ++groups;
 // Analytic full-output oracle. Each expectation changes only proved native stores.
 for(U32 state:{0u,1u,2u,0x80000000u})for(U32 marker:{0u,0x80u,0x200u,0x280u})for(U32 type:{2u,11u,0x10000002u})for(U32 stored:{2u,10u,0x82u})for(U32 count:{1u,2u,0xffffffffu})for(U32 identity:{0u,1u})for(U32 hashbad:{0u,1u})for(U32 flag:{0u,4u}){
  Basic f;f.m.Cl(f.cl,f.tab,1,0,flag);f.m.Field(f.tab,f.name,f.hash,type,count,marker,state,f.code,0);f.m.Desc(f.desc,f.hash+hashbad,static_cast<std::uint8_t>(identity),static_cast<std::uint8_t>(stored),1);
  auto expected=f.m;expected.W(f.cl+0x3c,1,1);expected.W(f.cat+0x20,f.cl,8);if(!(marker&0x280)&&!(flag&4))expected.W(f.tab+0x40,0,1);
  const bool visited=state!=1&&marker==0;
  if(visited){const bool pass=identity==0&&!hashbad&&(type==(stored&63)||(type==11&&(stored&63)==10))&&(count==1||count==0xffffffffu)&&flag==0;expected.W(f.tab+0x30,pass?(state==0?2:state):1,4);}
  Monstey_GadbBinderTrace t;Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);Check(f.m.b==expected.b,"Analytic binder predicate full-byte state differs");Check(Events(t,EK::IndexCallback)==(visited?1u:0u),"Native callback gate/order differs");++cases;
 }
}
void LiteralCallbacks(){
 ++groups;
 for(U32 form=0;form<5;++form)for(U32 v=0;v<256;++v){Basic f;const bool zero=form>=2;f.m.W(f.code,form==0?0xb0:form==1?0xb8:form==2?0x32:form==3?0x30:0x33,1);if(form<2){f.m.W(f.code+1,form==1?0xabcd0000u|v:v,form==1?4:1);f.m.W(f.code+(form==1?5:2),0xc3,1);}else{f.m.W(f.code+1,0xc0,1);f.m.W(f.code+2,0xc3,1);}f.m.W(f.desc+8,zero?0:v,1);Monstey_GadbBinderTrace t;Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);Check(f.m.R(f.tab+0x30,4)==2,"Concrete literal callback AL differs");++cases;}
 {Basic f;f.m.W(f.code,0x90,1);Monstey_GadbBinderTrace t;Fault([&]{Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);},FK::UnsupportedIndexCallback,f.code);Check(f.m.R(f.cl+0x3c,1)==1&&f.m.R(f.cat+0x20,8)==f.cl&&f.m.R(f.tab+0x30,4)==0,"Opaque callback lost earlier real writes");++cases;}
 // False type/count must still consume an in-range callback.
 {Basic f;f.m.W(f.code,0x90,1);f.m.W(f.tab+0x24,99,4);f.m.W(f.tab+0x28,99,4);Monstey_GadbBinderTrace t;Fault([&]{Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);},FK::UnsupportedIndexCallback,f.code);++cases;}
 for(U32 gate=0;gate<3;++gate){Basic f;f.m.W(f.code,0x90,1);if(gate==0)f.m.W(f.tab+0x30,1,4);if(gate==1)f.m.W(f.tab+0x2c,0x80,4);if(gate==2)f.m.W(f.rec,0,1);f.m.W(f.tab,0xffffffffffffffffull,8);Monstey_GadbBinderTrace t;Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);Check(Events(t,EK::IndexCallback)==0,"Skipped path consumed opaque callback/name");++cases;}
}
void RecordSelection(){
 ++groups;
 {Basic f;f.m.Category(f.cat,0,0,0,0);Monstey_GadbBinderTrace t;auto expected=f.m;Check(!Monstey_GadbCategoryHasBindingRecords(f.m.View(),f.cat,t),"Empty preselection category passed");Check(f.m.b==expected.b&&t.events.empty(),"Empty category gate mutated storage");Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);Check(f.m.R(f.cat+0x20,8)==f.cl&&f.m.R(f.cl+0x3c,1)==1,"Selected entry repeated the earlier category-count gate");++cases;}
 {Basic f;f.m.W(f.cat+0x38,0,8);f.m.W(f.cat+0x40,1,4);f.m.W(f.cat+0x48,f.rec,8);Monstey_GadbBinderTrace t;Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);Check(f.m.R(f.tab+0x30,4)==0&&Events(t,EK::LoopSamples)==0,"Null first named record searched anonymous/later records");++cases;}
 {Basic f;f.m.Category(f.cat,0,0,1,f.rec);Monstey_GadbBinderTrace t;Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);Check(f.m.R(f.tab+0x30,4)==2,"First anonymous path not selected");++cases;}
 {Basic f;f.m.W(f.cl+8,0,4);f.m.W(f.cat+0x38,0xffffffffffffffffull,8);Monstey_GadbBinderTrace t;Fault([&]{Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);},FK::ConsumedRangeUnavailable,0xffffffffffffffffull);Check(f.m.R(f.cat+0x20,8)==f.cl,"Zero fields wrongly skipped record header sampling");++cases;}
 {Basic f;f.m.W(f.tab, f.m.A(f.m.b.size()-1),8);f.m.W(f.m.A(f.m.b.size()-1),0,1);f.m.Desc(f.desc,0,0,2,1);Monstey_GadbBinderTrace t;Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);Check(f.m.R(f.tab+0x30,4)==2,"Native C-string NUL termination read trailing memory");++cases;}
}
void SavedAndLiveAliases(){
 ++groups;
 // Descriptor count aliases first field state: fallback changes 0 -> 1, but
 // both visits must use the count sampled before the first field.
 {Basic f;f.m.Cl(f.cl,f.tab,2,0,0,1);f.m.Field(f.tab,f.name,f.hash,2,1,0,0,f.desc,0);f.m.Field(f.tab+72,f.name,f.hash,2,1,0,0,f.code,0);f.m.Category(f.cat,1,f.tab+0x30);f.m.W(f.tab+0x38,f.desc,8);Monstey_GadbBinderTrace t;Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);Check(f.m.R(f.tab+0x30,4)==1&&f.m.R(f.tab+72+0x30,4)==1,"Descriptor count reloaded after first helper");Check(Events(t,EK::IndexCallback)==0,"Saved zero descriptor count lost");++cases;}
 // The first field state aliases class flags: raw4 -> fallback1. Saved mask4
 // must still reject the otherwise compatible second field.
 {Basic f;const auto first=f.cl+8;f.m.Field(first,2,0,2,1,0,4,f.code,255);f.m.Field(first+72,f.name,f.hash,2,1,0,0,f.code,0);f.m.Cl(f.cl,first,2,0,4,1);f.m.W(first+0x40,255,1);Monstey_GadbBinderTrace t;Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);Check(f.m.R(f.cl+0x38,4)==1&&f.m.R(first+72+0x30,4)==1,"Class mask sampled after live flag alias mutation");++cases;}
 // First state aliases class count2 -> 1. The saved two-row bound still visits
 // the second record. The live field table base itself is reloaded each visit.
 {Basic f;const auto first=f.cl-0x28;f.m.Field(first,0,0,2,1,0,2,0,255);f.m.Field(first+72,f.name,f.hash,2,1,0,0,f.code,0);f.m.Cl(f.cl,first,2,0,0,1);f.m.W(first+0x40,255,1);Monstey_GadbBinderTrace t;Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);Check(f.m.R(f.cl+8,4)==1&&f.m.R(first+72+0x30,4)==2,"Field loop bound was reread after state mutation");++cases;}
 {Basic f;const auto first=f.cl-0x30;f.m.Field(first,0,0,2,1,0,0,0,255);f.m.Cl(f.cl,first,2,0,0,1);f.m.W(first+0x40,255,1);Monstey_GadbBinderTrace t;Fault([&]{Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);},FK::ConsumedRangeUnavailable,1+72+0x30);Check(f.m.R(f.cl,8)==1,"Earlier alias write lost before live table failure");++cases;}
}

void ExplicitHistoryAndLazyReads(){
 ++groups;
 {Basic f;Monstey_GadbBinderTrace t;Check(Monstey_GadbCategoryHasBindingRecords(f.m.View(),f.cat,t),"Preselection count unexpectedly empty");f.m.W(f.cat+0x28,0,4);f.m.W(f.cat+0x40,0,4);Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);Check(f.m.R(f.cat+0x20,8)==f.cl&&f.m.R(f.cl+0x3c,1)==1&&Events(t,EK::LoopSamples)==0,"Selected entry repeated preselection after supplied count change");++cases;}

 // Explicitly supplied sequential operations; no actual load chronology inferred.
 for(U32 initial:{0u,2u,1u,0xffffffffu}){
  Basic f;f.m.W(f.tab+0x30,initial,4);Monstey_GadbBinderTrace t;
  Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);
  Check(f.m.R(f.tab+0x30,4)==(initial==0?2:initial),"Success altered an existing nonzero state");
  f.m.W(f.desc+0xc,0,2);Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);
  Check(f.m.R(f.tab+0x30,4)==1,"Mismatch failed to persist fallback1");
  f.m.W(f.desc+0xc,1,2);const auto before=Events(t,EK::IndexCallback);
  Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);
  Check(f.m.R(f.tab+0x30,4)==1&&Events(t,EK::IndexCallback)==before,"Later layout resurrected fallback state1");++cases;
 }
 {Basic f;f.m.Cl(f.cl,0,1);Monstey_GadbBinderTrace t;Monstey_GadbInitializeSelectedClass(f.m.View(),f.cl,t);Check(f.m.R(f.cl+0x3c,1)==1&&Events(t,EK::CacheWrite)==0,"Initializer did not skip literal null row0");++cases;}
 for(const bool success:{false,true}){
  Basic f;f.m.W(f.tab+0x30,success?2:1,4);auto expected=f.m;Monstey_GadbBinderTrace t;
  Monstey_GadbPropagateSelectedField(f.m.View(),~U64{0},f.tab,0,success,t);
  Check(f.m.b==expected.b,"Stopped helper consumed invalid class or changed bytes");++cases;
 }
 {const U64 cat=0x20000;std::array<std::uint8_t,4>b{1,0,0,0};Monstey_GadbBinderTrace t;
  Check(Monstey_GadbCategoryHasBindingRecords({cat+0x28,b},cat,t),"Nonzero named count read unavailable anonymous count");
  Check(t.consumed_operations==1,"Preselection short-circuit consumed extra storage");b[0]=0;
  Fault([&]{(void)Monstey_GadbCategoryHasBindingRecords({cat+0x28,b},cat,t);},FK::ConsumedRangeUnavailable,cat+0x40);++cases;
 }
 // First state aliases the real next callback code, not a synthetic callback.
 {Basic f;const auto first=f.code-0x30;f.m.Field(first,f.name,f.hash,2,1,0,0,f.code,255);f.m.Field(first+72,f.name,f.hash,2,1,0,0,f.code,0);f.m.Cl(f.cl,first,2,0,0,1);f.m.W(f.code,0xb0,1);f.m.W(f.code+1,0,1);f.m.W(f.code+2,0xc3,1);f.m.W(first+0x40,255,1);Monstey_GadbBinderTrace t;
  Fault([&]{Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);},FK::UnsupportedIndexCallback,f.code);
  Check(f.m.R(f.code,4)==1,"Earlier state write did not alias next code bytes");++cases;
 }
}

void NativeHashWords(){
 ++groups;
 for(U32 middle=0;middle<256;++middle)for(bool prefix:{false,true}){
  Basic f;std::string name=prefix?"x":"";name.push_back(static_cast<char>(middle));name+="z";name.push_back(0);name+="ignored";f.m.Text(f.name,name);U32 expected=0;
  for(const unsigned char c:name){if(c==0)break;const auto term=static_cast<std::int8_t>(Monstey_GadbHashReference[c]);expected=expected*0x397u+static_cast<U32>(static_cast<std::int32_t>(term));}
  f.m.W(f.desc,expected,4);Monstey_GadbBinderTrace t;Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);
  Check(f.m.R(f.tab+0x30,4)==2,"Native NUL/sign-table hash differs from complete paired literal");++cases;
 }
}

void CorrespondenceOrder(){
 ++groups;
 for(const U64 member:{0x20ull,0x24ull,0x2cull,0x28ull,0x40ull})for(const U32 delta:{0x80u,0x80000000u}){if(member==0x40&&delta>255)continue;
  Basic f;const auto p=f.m.A(0x180),pt=f.m.A(0x2000);f.m.Cl(f.cl,f.tab,1,p,0,1);f.m.Cl(p,pt,1,0,0,1);f.m.Field(pt,0,f.hash,2,1,0,0,0,0);f.m.W(pt+member,f.m.R(pt+member,member==0x40?1:4)^delta,member==0x40?1:4);f.m.W(f.tab+0x40,0,1);Monstey_GadbBinderTrace t;
  Fault([&]{Monstey_GadbPropagateSelectedField(f.m.View(),f.cl,f.tab,0,true,t);},FK::NativeParentCorrespondenceAssertion,pt+member);
  Check(f.m.R(f.tab+0x30,4)==2&&f.m.R(pt+0x30,4)==0,"Parent correspondence stop rewound prior source state");++cases;
 }
 // Source and ancestor rows overlap at non-row-aligned raw addresses.
 {Basic f;const auto p=f.m.A(0x180),pf=f.tab+16;f.m.Cl(f.cl,f.tab,1,p,0,1);f.m.Cl(p,pf,1,0,0,1);
  f.m.W(f.tab+0x20,2,4);f.m.W(f.tab+0x24,2,4);f.m.W(f.tab+0x28,1,4);f.m.W(f.tab+0x2c,0,4);f.m.W(f.tab+0x30,0,4);
  f.m.W(pf+0x24,2,4);f.m.W(pf+0x28,1,4);f.m.W(pf+0x2c,0,4);f.m.W(f.tab+0x40,0,4);f.m.W(pf+0x40,0,1);
  Monstey_GadbBinderTrace t;Monstey_GadbPropagateSelectedField(f.m.View(),f.cl,f.tab,0,true,t);
  Check(f.m.R(f.tab+0x30,4)==2&&f.m.R(pf+0x30,4)==2,"Overlapping ancestor propagation was narrowed or copied");++cases;
 }
}

void HelperReference(){
 ++groups;std::mt19937 rng(0x92753618);
 for(U32 n=0;n<4096;++n){Image m;J classes=J::object(),fields=J::object();const U32 depth=1+rng()%6,index=rng()%5;const bool success=(rng()&1)!=0;const U64 first=m.A(0x1000)+72*index;const U64 selected=m.A(0x100);std::vector<U64> native;
  for(U32 d=0;d<depth;++d){const auto cl=m.A(0x100+64*d),tab=m.A(0x1000+0x400*d),f=tab+72*index;const U32 count=(rng()%5==0)?index:index+1,state=std::array<U32,4>{0,1,2,0xffffffffu}[rng()%4],hash=(d&&rng()%7==0)?77:123;const std::string cid="c"+std::to_string(d),fid="f"+std::to_string(d);m.Cl(cl,tab,count,d+1<depth?m.A(0x100+64*(d+1)):0,0,1);m.Field(f,0,hash,2,1,0,state,0,3);native.push_back(f);J fs=J::array();for(U32 q=0;q<count;++q)fs.as_array().push_back(q==index?J(fid):J());classes[cid]=J::object({{"parent",d+1<depth?J("c"+std::to_string(d+1)):J()},{"field_count",count},{"fields",fs}});fields[fid]=J::object({{"hash",hash},{"type",2},{"count",1},{"flags",0},{"cache",3},{"state",state}});}
  const J event=J::object({{"operation",success?"success":"fallback"},{"class","c0"},{"field","f0"},{"declaration_index",index}});const J input=J::object({{"format","Monstey_GadbCachePropagationTrace"},{"version",1},{"classes",classes},{"fields",fields},{"events",J::array({event})}});const auto ref=Monstey_GadbClassCacheTrace(input);auto expected=m;for(U32 d=0;d<depth;++d)expected.W(native[d]+0x30,ref["final_fields"]["f"+std::to_string(d)]["state"].as_uint(),4);bool assertion=false;Monstey_GadbBinderTrace t;try{Monstey_GadbPropagateSelectedField(m.View(),selected,first,index,success,t);}catch(const Monstey_GadbBinderFault&e){Check(e.kind==FK::NativeParentCorrespondenceAssertion,"Unexpected helper reference fault");assertion=true;}Check(assertion==(ref["status"].as_string()=="would_assert"),"Actual helper assertion ordering differs from accepted reference");Check(m.b==expected.b,"Actual helper writes differ from accepted explicit trace");++cases;
 }
}
void ParentIndexAndFailures(){
 ++groups;
 {Basic f;const auto p=f.m.A(0x180),pt=f.m.A(0x2000);f.m.Cl(p,pt,2);f.m.Cl(f.cl,f.tab,3,p);for(U32 i=0;i<3;++i){f.m.Field(f.tab+72*i,f.name,f.hash,2,1,i==1?0x280u:0,0,f.code,0);if(i<2)f.m.Field(pt+72*i,0,f.hash,2,1,i==1?0x280u:0,0,0,0);}f.m.W(f.rec,2,1);f.m.Desc(f.desc+16,f.hash,0,2,1);Monstey_GadbBinderTrace t;Monstey_GadbBindSelectedCategory(f.m.View(),f.cl,f.cat,t);Check(f.m.R(pt+0x30,4)==2&&f.m.R(pt+72+0x30,4)==0,"Propagation used compact index instead of original declaration index");Check(f.m.R(f.tab+144+0x30,4)==2,"Post-marker ordinary field failed");++cases;}
 {Basic f;const auto p=f.m.A(0x180),pt=f.m.A(0x2000);f.m.Cl(f.cl,f.tab,1,p,0,1);f.m.Cl(p,pt,1,0,0,1);f.m.Field(pt,0,99,2,1,0,0,0,0);Monstey_GadbBinderTrace t;Fault([&]{Monstey_GadbPropagateSelectedField(f.m.View(),f.cl,f.tab,0,true,t);},FK::NativeParentCorrespondenceAssertion,pt+0x20);Check(f.m.R(f.tab+0x30,4)==2&&f.m.R(pt+0x30,4)==0,"Assertion rolled back native prior state write");++cases;}
 // A shared source/parent row writes once and stops on the changed state.
 {Basic f;const auto p=f.m.A(0x180);f.m.Cl(f.cl,f.tab,1,p,0,1);f.m.Cl(p,f.tab,1,0,0,1);Monstey_GadbBinderTrace t;Monstey_GadbPropagateSelectedField(f.m.View(),f.cl,f.tab,0,true,t);Check(Events(t,EK::StateWrite)==1&&Events(t,EK::ParentPropagation)==1,"Shared parent field alias sampled from a stale copy");++cases;}
 // Parent pointer is read after source state write, not prefetched.
 {Basic f;const auto field=f.cl-8;f.m.W(field+0x30,0,4);f.m.W(f.cl+0x28,0,8);Monstey_GadbBinderTrace t;Fault([&]{Monstey_GadbPropagateSelectedField(f.m.View(),f.cl,field,0,true,t);},FK::ConsumedRangeUnavailable,2+8);Check(f.m.R(f.cl+0x28,8)==2,"Parent pointer alias write not retained");++cases;}
}
}
int main(int argc,char**argv){try{Initializers();PredicateMatrix();LiteralCallbacks();RecordSelection();SavedAndLiveAliases();HelperReference();ParentIndexAndFailures();ExplicitHistoryAndLazyReads();CorrespondenceOrder();NativeHashWords();const auto report=J::object({{"schema","Monstey.GadbBinderCompositionTests/1"},{"status","PASS"},{"groups",groups},{"cases",cases},{"checks",checks},{"original_engine_calls",false},{"runtime_history_qualified",false},{"external_comparator_supplied",false}});if(argc==2)Monstey_WriteNewText(std::filesystem::path(argv[1]),report.dump(2));std::cout<<report.dump()<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"case "<<cases<<": "<<e.what()<<'\n';return 1;}}
