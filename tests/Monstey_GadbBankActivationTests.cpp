#include "Monstey_GadbBankActivation.hpp"
#include "Monstey_Core.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <array>
#include <barrier>
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
namespace Monstey {
std::int32_t Monstey_GadbBankEpochAcquire(std::int32_t &) noexcept;
void Monstey_GadbBankEpochRelease(std::int32_t &, std::int32_t) noexcept;
}
using namespace Monstey;
namespace {
using U64=std::uint64_t;using U32=std::uint32_t;using S32=std::int32_t;
U64 groups=0,cases=0,checks=0,rangeFailures=0;
void Need(bool value,const char*why){++checks;if(!value)throw std::runtime_error(why);}
Monstey_GadbBankBackendObservation Backend(bool event){return{true,!event,!event,!event,event?0x1234ull:0ull};}
void Fault(auto call,U64 address){bool caught=false;++cases;++rangeFailures;try{call();}catch(const Monstey_GadbBinderFault&e){caught=true;Need(e.address==address&&e.kind==Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable,"first native consumed range");}Need(caught,"missing range fault");}
struct Fixture {
 static constexpr U64 base=0x140000000,banktable=0x1426397e0,A=base+0x1000,B=base+0x1100,
  nameA=0xa11,nameB=0xb11,db=base+0x2000,cats=base+0x3000,record=base+0x4000;
 static constexpr std::array<U64,6> ids={0x14162dc24,0x1416248b8,0x1404baa24,0x14161d674,0x14161d594,0x14482a370};
 static constexpr std::array<U64,6> guards={0x142aa2b68,0x142aa2b3c,0x142a58df0,0x142aa2b44,0x142aa2b4c,0};
 static constexpr std::array<U64,6> arrays={0x142a59ea0,0x142a58c70,0x142a58e00,0x142a590e0,0x142a59e10,0x142a59e08};
 static constexpr std::array<U32,6> counts={2687,24,50,375,15,0};
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x2b00000,0x5d);
 std::array<Monstey_GadbBankRecipe,6> recipes;
 std::array<std::unique_ptr<Monstey_GadbBankEpochWord>,5> owned;
 std::array<Monstey_GadbBankEpochWord*,5> words{};
 Monstey_GadbBankEpochWord global{0x1425ac000,INT32_MIN};S32 epoch=INT32_MIN;
 Monstey_GadbBankSynchronizer sync;Monstey_GadbNonnullPointerComparator compare;
 Fixture(bool event=false):sync(Backend(event)){
  compare=[](U64 x,U64 y)->S32{return x==y?0:1;};
  for(U32 i=0;i<6;++i){recipes[i]={ids[i],guards[i],arrays[i],counts[i],std::vector<U64>(counts[i],A)};
   if(counts[i]>1)recipes[i].constructor_classes[1]=B;
   Set(banktable+24ull*i,0x900+i,8);Set(banktable+24ull*i+8,ids[i],8);
   if(i<5){owned[i]=std::make_unique<Monstey_GadbBankEpochWord>(guards[i],0);words[i]=owned[i].get();}}
  for(U64 c:{A,B}){for(U32 n=0;n<64;++n)Set(c+n,0,1);Set(c+32,c==A?nameA:nameB,8);}
  Monstey_GadbInitializeDatabaseHeader(Memory(),db);Set(db+72,0,4);Set(db+80,cats,8);
  for(U32 n=0;n<40;++n)Set(record+n,0,1);
 }
 Monstey_GadbBinderMemory Memory(){return{base,bytes};}
 Monstey_GadbBankActivation Context(){return{Memory(),recipes,words,global,epoch,sync,banktable,compare};}
 void Set(U64 a,U64 v,std::size_t n){if(a<base||a-base+n>bytes.size())throw std::runtime_error("fixture range");for(std::size_t i=0;i<n;++i)bytes[static_cast<std::size_t>(a-base)+i]=static_cast<std::uint8_t>(v>>(8*i));}
 U64 Get(U64 a,std::size_t n){U64 v=0;for(std::size_t i=0;i<n;++i)v|=static_cast<U64>(bytes[static_cast<std::size_t>(a-base)+i])<<(8*i);return v;}
 void Category(U32 i,U64 name,U32 named,U32 anon){const auto c=cats+88ull*i;for(U32 j=0;j<88;++j)Set(c+j,0,1);Set(c+8,name,8);Set(c+32,0x777,8);Set(c+40,named,4);Set(c+56,record,8);Set(c+64,anon,4);Set(c+72,record,8);}
};
void OwnedPrimitiveCode(){++groups;
 const auto loadAddress=std::bit_cast<std::uintptr_t>(&Monstey_GadbBankEpochAcquire);
 const auto storeAddress=std::bit_cast<std::uintptr_t>(&Monstey_GadbBankEpochRelease);
 const auto*load=reinterpret_cast<const std::uint8_t*>(loadAddress);
 const auto*store=reinterpret_cast<const std::uint8_t*>(storeAddress);
 constexpr std::array<std::uint8_t,3> expectedLoad{0x8b,0x01,0xc3},expectedStore{0x89,0x11,0xc3};
 for(std::size_t i=0;i<3;++i){++cases;Need(load[i]==expectedLoad[i],"owned guard acquire must be exact plainMOV/RET");Need(store[i]==expectedStore[i],"owned guard release must be exact plainMOV/RET");}
}
void HeaderInitialization(){++groups;
 constexpr std::array<U32,17> offsets={0,8,16,24,32,40,48,56,64,72,80,88,96,104,112,120,128};
 for(std::size_t end=0;end<140;++end){std::vector<std::uint8_t>b(144,0x73),expected=b;bool fail=false;U64 bad=0;
  for(U32 at:offsets){const U32 width=at==56||at==128?4:8;if(at+width>end){fail=true;bad=at;break;}for(U32 j=0;j<width;++j)expected[at+j]=0;}
  ++cases;if(fail)Fault([&]{(void)Monstey_GadbInitializeDatabaseHeader({0,std::span(b).first(end)},0);},bad);else Need(Monstey_GadbInitializeDatabaseHeader({0,std::span(b).first(end)},0)==0,"header identity");
  Need(b==expected,"header exact store order/padding/fault prefix");}
}
void GuardSequential(){++groups;for(bool event:{false,true}){
 Monstey_GadbBankSynchronizer s(Backend(event));
 for(S32 initial:{INT32_MIN,-3,1,2,INT32_MAX})for(S32 globalValue:{INT32_MIN,-2,0,INT32_MAX}){
  Monstey_GadbBankEpochWord guard(0x10,initial),global(0x20,globalValue);S32 thread=33;++cases;s.Header(guard,global,thread);Need(guard.Load()==initial&&global.Load()==globalValue&&thread==globalValue,"existing header epoch");}
 for(S32 value:{INT32_MIN,-2,0,INT32_MAX}){Monstey_GadbBankEpochWord guard(0x10,0),global(0x20,value);S32 thread=123;++cases;s.Header(guard,global,thread);Need(guard.Load()==-1&&thread==123&&global.Load()==value,"header claim");s.Footer(guard,global,thread);
  const auto expected=std::bit_cast<S32>(std::bit_cast<U32>(value)+1u);Need(guard.Load()==expected&&global.Load()==expected&&thread==expected,"footer unsigned wrap/raw bits");}
 Need(s.Stats().wait_calls==0,"sequential unexpected wait");}
 for(U32 bits=0;bits<8;++bits)for(bool present:{false,true}){const bool all=bits==7;bool rejected=false;++cases;try{Monstey_GadbBankSynchronizer s({true,(bits&1)!=0,(bits&2)!=0,(bits&4)!=0,present?1ull:0ull});}catch(const Monstey_GadbBankFault&e){rejected=true;Need(e.kind==Monstey_GadbBankFaultKind::InvalidBackendObservation,"backend diagnostic");}Need(rejected==(present==all),"observed backend selection");}
}
void WaitAndPublication(){++groups;for(bool event:{false,true}){
 Monstey_GadbBankSynchronizer s(Backend(event));Monstey_GadbBankEpochWord guard(0x10,-1),global(0x20,INT32_MIN);
 std::array<S32,4> epochs{INT32_MIN,INT32_MIN,INT32_MIN,INT32_MIN};std::array<U64,4> seen{};std::vector<std::thread> threads;U64 publication=0;
 const auto began=std::chrono::steady_clock::now();
 for(std::size_t i=0;i<4;++i)threads.emplace_back([&,i]{s.Header(guard,global,epochs[i]);seen[i]=publication;});
 const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(4);while(s.Stats().wait_calls<8&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 const bool retried=s.Stats().wait_calls>=8;const auto waitingDuration=std::chrono::steady_clock::now()-began;publication=0xf123456789abcdef;S32 ownerEpoch=INT32_MIN;s.Footer(guard,global,ownerEpoch);for(auto&t:threads)t.join();++cases;
 Need(retried,"actual100ms timeout/recheck not observed");if(event)Need(waitingDuration>=std::chrono::milliseconds(80),"manual-event wait must actually exercise native100ms timeout");for(std::size_t i=0;i<4;++i)Need(epochs[i]==INT32_MIN+1&&seen[i]==publication,"real wait/wake publication");Need(s.Stats().wake_calls==1,"wake count");
 }
}
void EnumerationAndFault(){++groups;Fixture f;auto c=f.Context();
 for(U32 i=0;i<6;++i){++cases;const auto r=Monstey_GadbEnumerateNativeBank(c,f.ids[i]);Need(r.array==f.arrays[i]&&r.count==f.counts[i],"literal enumeration outputs");for(U32 j=0;j<f.counts[i];++j)Need(f.Get(r.array+8ull*j,8)==f.recipes[i].constructor_classes[j],"constructor constant order");}
 const auto wakes=f.sync.Stats().wake_calls;f.Set(f.arrays[0],f.B,8);++cases;Need(Monstey_GadbEnumerateNativeBank(c,f.ids[0]).count==2687&&f.Get(f.arrays[0],8)==f.B&&f.sync.Stats().wake_calls==wakes,"cached array must retain caller state");
 // Native signed fast test may bypass initialization for supplied unusual
 // sequential epochs. No concurrent constructor is active in this fixture.
 f.words[1]->Store(-1);f.epoch=0;f.Set(f.arrays[1],0x123,8);++cases;Need(Monstey_GadbEnumerateNativeBank(c,f.ids[1]).count==24&&f.Get(f.arrays[1],8)==0x123,"signed busy fast skip preserved");
 f.words[1]->Store(0);f.epoch=INT32_MIN;f.global.Store(INT32_MIN);auto shortContext=f.Context();shortContext.memory.bytes=std::span(f.bytes).first(static_cast<std::size_t>(f.arrays[1]+16-f.base));
 Fault([&]{(void)Monstey_GadbEnumerateNativeBank(shortContext,f.ids[1]);},f.arrays[1]+16);Need(f.words[1]->Load()==-1&&f.global.Load()==INT32_MIN&&f.epoch==INT32_MIN,"constructor failure must retain native claim");Need(f.Get(f.arrays[1],8)==f.A&&f.Get(f.arrays[1]+8,8)==f.B,"constructor fault prefix writes");
 Monstey_GadbBankEpochWord other(0x2222,0);S32 t=INT32_MIN;f.sync.Header(other,f.global,t);Need(other.Load()==-1,"lock not released before constructor failure");f.sync.Footer(other,f.global,t);
}
void ConcurrentEnumeration(){++groups;for(bool event:{false,true}){Fixture f(event);std::array<S32,8> epochs;epochs.fill(INT32_MIN);std::array<U64,8> first{};std::barrier start(8);std::vector<std::thread> threads;
 for(std::size_t i=0;i<8;++i)threads.emplace_back([&,i]{auto c=f.Context();Monstey_GadbBankActivation own{c.memory,c.recipes,c.guards,c.global_epoch,epochs[i],c.synchronization,c.bank_table,c.external_comparator};start.arrive_and_wait();const auto result=Monstey_GadbEnumerateNativeBank(own,f.ids[0]);first[i]=f.Get(result.array,8);});
 for(auto&t:threads)t.join();++cases;Need(f.sync.Stats().wake_calls==1&&f.global.Load()==INT32_MIN+1,"constructor must publish once");for(auto value:first)Need(value==f.A,"concurrent enum array publication");}}
void ParserAndBinder(){++groups;Fixture f;auto c=f.Context();++cases;Need(Monstey_GadbFindParserBankClass(c,0x900,f.nameA)==f.A,"parser first class");Need(f.Get(f.A+60,1)==0,"parser hook must not initialize class");
 f.Set(f.arrays[0],f.B,8);f.Set(f.B+32,f.nameA,8);++cases;Need(Monstey_GadbFindParserBankClass(c,0x900,f.nameA)==f.B,"parser cached array must remain live");
 f.Set(f.arrays[0],f.A,8);f.Set(f.B+32,f.nameB,8);const auto original=f.compare;U32 calls=0;f.compare=[&](U64 x,U64 y)->S32{++calls;if(x==f.nameA&&y==f.nameA){f.Set(f.arrays[0],0,8);return 0;}return original(x,y);};++cases;Need(Monstey_GadbFindParserBankClass(c,0x900,f.nameA)==0,"parser late class reread");Need(calls==2,"parser compare order");f.Set(f.arrays[0],f.A,8);f.compare=original;
 f.Set(f.banktable,0,8);f.Set(f.A+32,0,8);calls=0;f.compare=[&](U64 x,U64 y)->S32{++calls;return x==y?0:1;};++cases;Need(Monstey_GadbFindParserBankClass(c,0,0)==f.A&&calls==2,"parser direct comparator must consume null pointers");
 f.Set(f.banktable,0x900,8);f.Set(f.A+32,f.nameA,8);f.compare=original;f.Set(f.db+72,3,4);f.Category(0,f.nameA,1,0);f.Category(1,f.nameB,0,0);f.Category(2,f.nameB,0,1);Monstey_GadbBinderTrace trace;++cases;Monstey_GadbBindDatabaseBank(c,0x900,f.db,trace);Need(f.Get(f.cats+32,8)==f.A&&f.Get(f.cats+88+32,8)==0x777&&f.Get(f.cats+176+32,8)==f.B,"whole DB category selection");Need(f.Get(f.A+60,1)==1&&f.Get(f.B+60,1)==1,"selected initializer composition");
 f.Set(f.db+72,1,4);f.Set(f.db+80,f.base-32,8);f.Set(f.base+8,1,4);auto bounded=f.Context();bounded.memory.bytes=std::span(f.bytes).first(static_cast<std::size_t>(f.banktable+144-f.base));Monstey_GadbBinderTrace t;
 Fault([&]{Monstey_GadbBindDatabaseBank(bounded,0x900,f.db,t);},f.base-24);
}
void BorrowedEpochAndCallbackCases(){++groups;Fixture f;auto c=f.Context();
 bool rejected=false;++cases;try{f.sync.Header(f.global,f.global,f.epoch);}catch(const Monstey_GadbBankFault&e){rejected=e.kind==Monstey_GadbBankFaultKind::InvalidRecipe;}Need(rejected&&f.global.Load()==INT32_MIN,"aliased typed epoch backing");
 Monstey_GadbBankEpochWord duplicate(f.guards[0],99);std::array<Monstey_GadbBankEpochWord*,2> bad{f.words[0],&duplicate};auto invalid=f.Context();invalid.guards=bad;rejected=false;++cases;try{(void)Monstey_GadbEnumerateNativeBank(invalid,f.ids[0]);}catch(const Monstey_GadbBankFault&e){rejected=e.kind==Monstey_GadbBankFaultKind::InvalidRecipe;}Need(rejected&&f.words[0]->Load()==0,"ambiguous typed guard identity");
 const auto original=f.compare;++cases;Need(Monstey_GadbFindParserBankClass(c,0x900,0xdead)==0&&f.words[1]->Load()==0,"class miss must not fall through to later bank");
 f.Set(f.db+72,1,4);f.Category(0,f.nameB,1,0);U32 visited=0;f.compare=[&](U64 x,U64 y)->S32{if(x==f.nameA&&y==f.nameB&&visited++==0){f.Set(f.cats+8,f.nameA,8);return 1;}return original(x,y);};Monstey_GadbBinderTrace trace;++cases;Monstey_GadbBindDatabaseBank(c,0x900,f.db,trace);Need(f.Get(f.cats+32,8)==f.A,"category second-string must reload for each class");
 f.compare=original;f.Set(f.db+72,2,4);f.Category(0,f.nameA,1,0);f.Category(1,f.nameB,1,0);bool changed=false;f.compare=[&](U64 x,U64 y)->S32{if(!changed&&x==f.nameA&&y==f.nameA){f.Set(f.db+72,1,4);changed=true;}return original(x,y);};Monstey_GadbBinderTrace t;++cases;Monstey_GadbBindDatabaseBank(c,0x900,f.db,t);Need(f.Get(f.cats+88+32,8)==f.B,"database category count must remain captured");
 f.compare=original;f.Set(f.banktable+8,0xdead,8);rejected=false;++cases;try{(void)Monstey_GadbFindParserBankClass(c,0x900,f.nameA);}catch(const Monstey_GadbBankFault&e){rejected=e.kind==Monstey_GadbBankFaultKind::UnknownEnumerator&&e.identity==0xdead;}Need(rejected,"unsupported enumerator must stop at its call");
}
void ExactEpochFastPaths(){++groups;Fixture f;auto c=f.Context();
 for(U32 bank=0;bank<5;++bank)for(S32 guardValue:{INT32_MIN,-2,0,1,INT32_MAX})for(S32 threadValue:{INT32_MIN,-2,0,1,INT32_MAX}){
  f.words[bank]->Store(guardValue);f.global.Store(INT32_MIN);f.epoch=threadValue;f.Set(f.arrays[bank],0x777,8);const auto before=f.sync.Stats();
  const auto r=Monstey_GadbEnumerateNativeBank(c,f.ids[bank]);++cases;Need(r.array==f.arrays[bank]&&r.count==f.counts[bank],"fast path literal outputs");
  const bool enter=guardValue>threadValue;Need(f.sync.Stats().header_calls-before.header_calls==(enter?1u:0u),"native signed guard/thread fast comparison");
  if(enter&&guardValue==0){Need(f.Get(r.array,8)==f.A&&f.words[bank]->Load()==INT32_MIN+1&&f.epoch==INT32_MIN+1,"claimed constructor/epoch footer");Need(f.sync.Stats().wake_calls-before.wake_calls==1,"constructor wake");}
  else{Need(f.Get(r.array,8)==0x777&&f.words[bank]->Load()==guardValue,"fast/cached path must retain existing BSS");Need(f.epoch==(enter?INT32_MIN:threadValue),"cached header thread epoch");}
 }
 auto leaf=f.Context();leaf.memory={1,{}};leaf.guards={};const auto before=f.sync.Stats();++cases;const auto result=Monstey_GadbEnumerateNativeBank(leaf,f.ids[5]);Need(result.count==0&&result.array==f.arrays[5]&&f.sync.Stats().header_calls==before.header_calls,"empty bank leaf consumes no TLS/guard/array bytes");
}
void EmptyDatabaseAndLateInputs(){++groups;Fixture f;auto c=f.Context();Monstey_GadbBinderTrace trace;
 ++cases;Monstey_GadbBindDatabaseBank(c,0x900,f.db,trace);Need(f.words[0]->Load()==INT32_MIN+1&&f.Get(f.arrays[0],8)==f.A&&f.Get(f.A+60,1)==0,"empty DB still activates bank but no class");
 f.Set(f.banktable+8,0xdead,8);const auto wakes=f.sync.Stats().wake_calls;++cases;Monstey_GadbBindDatabaseBank(c,0x999,f.db,trace);Need(f.sync.Stats().wake_calls==wakes,"unmatched bank must not consume callback");f.Set(f.banktable+8,f.ids[0],8);
 f.Set(f.db+72,1,4);f.Category(0,0xdead,0,0);auto bounded=f.Context();bounded.memory.bytes=std::span(f.bytes).first(static_cast<std::size_t>(f.banktable+144-f.base));++cases;Monstey_GadbBindDatabaseBank(bounded,0x900,f.db,trace);Need(f.Get(f.cats+32,8)==0x777,"empty category must not read unavailable class array");
 f.Set(f.arrays[0],0,8);Fault([&]{(void)Monstey_GadbFindParserBankClass(c,0x900,f.nameA);},32);
 bool rejected=false;++cases;try{(void)Monstey_GadbActivationRecipes(std::array<std::uint8_t,4>{1,2,3,4});}catch(const std::exception&){rejected=true;}Need(rejected,"unsupported image cannot supply native recipes");
}
void ExplicitSequentialBindingHistory(){++groups;Fixture f;auto c=f.Context();const U64 field=f.base+0x6000,name=f.base+0x7000,callback=f.base+0x8000,descriptors=f.base+0x9000;
 for(U32 i=0;i<72;++i)f.Set(field+i,0,1);constexpr char text[]="Value";for(std::size_t i=0;i<sizeof(text);++i)f.Set(name+i,static_cast<std::uint8_t>(text[i]),1);
 f.Set(f.A,field,8);f.Set(f.A+8,1,4);f.Set(field,name,8);f.Set(field+32,Monstey_NativeHash("Value",0),4);f.Set(field+36,2,4);f.Set(field+40,1,4);f.Set(field+44,6,4);f.Set(field+56,callback,8);f.Set(field+64,200,1);
 f.Set(callback,0xb0,1);f.Set(callback+1,1,1);f.Set(callback+2,0xc3,1);for(U32 i=0;i<16;++i)f.Set(descriptors+i,0,1);f.Set(descriptors,Monstey_NativeHash("Value",0),4);f.Set(descriptors+8,1,1);f.Set(descriptors+10,2,1);f.Set(descriptors+12,1,2);
 f.Set(f.record,1,1);f.Set(f.record+8,descriptors,8);f.Set(f.db+72,1,4);f.Category(0,f.nameA,1,0);
 Monstey_GadbBinderTrace first;++cases;Monstey_GadbBindDatabaseBank(c,0x900,f.db,first);Need(f.Get(field+48,4)==2&&f.Get(field+64,1)==0&&f.Get(f.A+60,1)==1,"first explicit bind must initialize compact cache and succeed");
 f.Set(f.record,0,1);Monstey_GadbBinderTrace second;++cases;Monstey_GadbBindDatabaseBank(c,0x900,f.db,second);Need(f.Get(field+48,4)==1,"later missing descriptor must preserve fallback transition");
 f.Set(f.record,1,1);f.Set(field+64,99,1);f.Set(field,0,8);f.Set(field+56,0,8);Monstey_GadbBinderTrace third;++cases;Monstey_GadbBindDatabaseBank(c,0x900,f.db,third);Need(f.Get(field+48,4)==1&&f.Get(field+64,1)==99,"later bind must not reset prior field/class history");
}
void ResourceCleanup(){++groups;DWORD before=0,after=0;Need(GetProcessHandleCount(GetCurrentProcess(),&before)!=0,"own handle baseline");for(bool event:{false,true})for(U32 i=0;i<32;++i){Monstey_GadbBankSynchronizer s(Backend(event));Monstey_GadbBankEpochWord g(1,0),e(2,INT32_MIN);S32 thread=INT32_MIN;s.Header(g,e,thread);s.Footer(g,e,thread);++cases;}Need(GetProcessHandleCount(GetCurrentProcess(),&after)!=0,"own handle final");Need(before==after,"owned synchronization handle leak");}
}
int main(int argc,char**argv){try{if(argc!=2)throw std::runtime_error("Usage: BANK_ACTIVATION_TEST NEW_REPORT");OwnedPrimitiveCode();HeaderInitialization();GuardSequential();WaitAndPublication();EnumerationAndFault();ConcurrentEnumeration();ParserAndBinder();BorrowedEpochAndCallbackCases();ExactEpochFastPaths();EmptyDatabaseAndLateInputs();ExplicitSequentialBindingHistory();ResourceCleanup();const auto text="{\"status\":\"PASS\",\"groups\":"+std::to_string(groups)+",\"cases\":"+std::to_string(cases)+",\"checks\":"+std::to_string(checks)+",\"range_failures\":"+std::to_string(rangeFailures)+"}\n";std::ofstream out(argv[1],std::ios::binary);out<<text;Need(static_cast<bool>(out),"report write");std::cout<<text;return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
