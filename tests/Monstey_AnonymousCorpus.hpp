#pragma once
#include "Monstey_AnonymousReference.hpp"
#include <algorithm>
#include <array>
#include <string>
namespace Monstey_AnonymousCorpus {
using Monstey::Monstey_GadbBinderMemory;using Monstey::Monstey_GadbPoolReadService;
struct Monstey_Arguments {std::uint64_t category,originalRdx,record,stream,frame;};
struct Monstey_ReadEvent {std::uint64_t stream,target,destination,site,result;std::uint32_t bytes,copied;bool operator==(const Monstey_ReadEvent&)const=default;};
struct Monstey_Plan {std::uint64_t result=0;std::uint32_t maximum=0xFFFFFFFFu;};
struct Monstey_Case {
 static constexpr std::uint64_t Base=0x3456000000;
 std::string name;std::vector<std::uint8_t> initial=std::vector<std::uint8_t>(0x20000,0xA5);
 Monstey_Arguments args{Base+0x1000,0xAABBCCDDEEFF00C7ull,Base+0x1200,Base+0x2000,Base+0x3000};
 std::uint64_t cursor=Base+0x4000,descriptors=Base+0x5000,words=Base+0x7000,pool=0x100000000ull,table=Base+0x2100;
 std::vector<std::uint8_t> input;std::vector<Monstey_Plan> plan;
 enum class Hook :std::uint8_t {None,ShrinkValues,ChangeDescriptors};Hook hook=Hook::None;
 bool hasExpected=false,noService=false;std::uint64_t expected=0;
 Monstey_GadbBinderMemory memory(){return {Base,initial};}
 void set(std::uint64_t at,std::size_t n,std::uint64_t value){auto m=memory();Monstey_AnonymousReference::Monstey_Bytes(m).write(at,n,value);}
 void append(std::uint64_t value,std::size_t width){for(std::size_t i=0;i<width;++i)input.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
 void seed(std::uint16_t values,std::uint8_t count,std::uint32_t first=0,std::uint8_t flags=0x80){
  set(args.stream,8,table);set(table+8,8,0x140A10000);set(cursor,8,words);
  set(args.frame+0x70,8,pool);set(args.frame+0x78,8,descriptors);set(args.frame+0x88,8,cursor);
  set(args.frame+0x20,2,0);set(args.frame+0x24,4,0);set(args.frame+0x80,1,0xE7);
  append(values,2);append(count,1);append(first,4);append(flags,1);
  for(std::uint32_t i=0;i<values;++i)append(0x02000000u+i,4);
 }
 void descriptor(std::uint32_t index,std::uint8_t type,std::uint16_t count,std::uint16_t first){const auto at=descriptors+16ull*index;set(at+0xA,1,type);set(at+0xC,2,count);set(at+0xE,2,first);}
};
struct Monstey_Outcome {
 std::vector<std::uint8_t> bytes;std::vector<Monstey_ReadEvent> events;std::vector<Monstey_AnonymousReference::Monstey_Access> accesses;
 std::uint64_t result=0,address=0,target=0;std::size_t width=0,position=0;std::uint8_t fault=0;bool writing=false;
};
using Monstey_Executor=std::uint64_t(*)(Monstey_GadbBinderMemory&,const Monstey_Arguments&,const Monstey_GadbPoolReadService&,std::vector<Monstey_AnonymousReference::Monstey_Access>*);
inline std::uint64_t Monstey_Reference(Monstey_GadbBinderMemory& memory,const Monstey_Arguments& a,const Monstey_GadbPoolReadService& reader,std::vector<Monstey_AnonymousReference::Monstey_Access>* accesses){return Monstey_AnonymousReference::Monstey_AnonymousLoad(memory,a.category,a.originalRdx,a.record,a.stream,a.frame,reader,accesses);}
inline Monstey_Outcome Monstey_Execute(const Monstey_Case& test,Monstey_Executor execute){
 Monstey_Outcome out;out.bytes=test.initial;Monstey_GadbBinderMemory memory{Monstey_Case::Base,out.bytes};std::size_t calls=0;
 Monstey_GadbPoolReadService reader([&](Monstey_GadbBinderMemory& m,const Monstey::Monstey_GadbPoolReadCall& call)->std::uint64_t{
  const auto ordinal=calls++;const auto plan=ordinal<test.plan.size()?test.plan[ordinal]:Monstey_Plan{};
  const auto available=test.input.size()-std::min(out.position,test.input.size());
  const auto count=static_cast<std::uint32_t>(std::min<std::size_t>({available,plan.maximum,call.byte_count}));
  Monstey_AnonymousReference::Monstey_Bytes bytes(m);
  for(std::uint32_t i=0;i<count;++i)bytes.write(call.destination+i,1,test.input[out.position+i]);out.position+=count;
  auto result=plan.result;if(count<call.byte_count&&ordinal>=test.plan.size())result=0x1001;
  if(test.hook==Monstey_Case::Hook::ShrinkValues&&ordinal==4)bytes.write(test.args.frame+0x20,2,1);
  if(test.hook==Monstey_Case::Hook::ChangeDescriptors&&ordinal==3)bytes.write(test.args.frame+0x78,8,test.descriptors+16);
  out.events.push_back({call.stream,call.target,call.destination,call.native_site,result,call.byte_count,count});return result;
 });
 if(test.noService)reader={};
 try{out.result=execute(memory,test.args,reader,&out.accesses);}
 catch(const Monstey::Monstey_GadbPoolRelocationFault& error){out.fault=1;out.address=error.address;out.width=error.width;out.writing=error.writing;}
 catch(const Monstey::Monstey_GadbPoolReadBoundaryFault& error){out.fault=2;out.address=error.native_site;out.target=error.target;}
 return out;
}
inline std::vector<Monstey_Case> Monstey_Cases(){
 std::vector<Monstey_Case> tests;
 for(const auto type:std::array<std::uint8_t,12>{0,3,4,5,9,10,11,12,14,16,0xC4,0x8E}){
  Monstey_Case t;t.name="type-"+std::to_string(type);t.seed(3,1);t.descriptor(0,type,3,0);t.hasExpected=true;t.expected=1;tests.push_back(std::move(t));
 }
 for(const auto first:std::array<std::uint16_t,3>{0,0x1234,0xFFFF}){
  Monstey_Case t;t.name="zero-selected-first-"+std::to_string(first);t.seed(0,1);t.descriptor(0,14,0,first);t.hasExpected=true;t.expected=(std::uint64_t(first)&~std::uint64_t(255))|1;tests.push_back(std::move(t));
 }
 for(const auto count:std::array<std::uint16_t,3>{0,256,513}){
  Monstey_Case t;t.name="no-descriptors-rax-"+std::to_string(count);t.seed(count,0,0xFFFFFFFE);t.set(t.args.frame+0x70,8,0xDEADC0DE);t.hasExpected=true;t.expected=(std::uint64_t(count)&~std::uint64_t(255))|1;tests.push_back(std::move(t));
 }
 {Monstey_Case t;t.name="selected-live-count-rax";t.seed(513,1);t.descriptor(0,14,513,0);t.hasExpected=true;t.expected=0x201;tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="zero-selected-then-unselected-rax";t.seed(0,2);t.descriptor(0,4,0,0xABCD);t.descriptor(1,3,0,0);t.hasExpected=true;t.expected=0xAB01;tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="descriptor-count255";t.seed(0,255);for(std::uint32_t i=0;i<255;++i)t.descriptor(i,3,0,0);t.hasExpected=true;t.expected=1;tests.push_back(std::move(t));}
 for(std::size_t ordinal=0;ordinal<7;++ordinal){Monstey_Case t;t.name="read-failure-"+std::to_string(ordinal);t.seed(3,0);t.plan.resize(ordinal+1);t.plan[ordinal].result=0x1234567800AABCFFull;t.hasExpected=true;t.expected=ordinal<4?1:0x1234567800AABC00ull;tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="payload-high32status";t.seed(1,0);t.plan.resize(5);t.plan[4].result=0xAABBCCDD00000000ull;t.hasExpected=true;t.expected=1;tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="partial-header-retains-byte";t.seed(0,0);t.set(t.args.frame+0x24,4,0xAB000000);t.plan.resize(3);t.plan[2]={7,3};tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="partial-payload-upperzero";t.seed(2,0);t.plan.resize(5);t.plan[4]={0,2};tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="callback-shrink-values";t.seed(3,0);t.hook=Monstey_Case::Hook::ShrinkValues;t.hasExpected=true;t.expected=1;tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="late-descriptor-base";t.seed(1,1);t.descriptor(0,3,1,0);t.descriptor(1,14,1,0);t.hook=Monstey_Case::Hook::ChangeDescriptors;tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="pool-cached-once";t.pool=7;t.words=t.args.frame+0x70;t.seed(0,1);t.descriptor(0,14,2,0);tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="descriptor-count-frame-alias";t.pool=0xFF;t.words=t.args.frame+0x58;t.seed(0,2);t.descriptor(0,4,1,0);t.descriptor(1,3,0,0);tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="flags-read-late";t.pool=1;t.words=t.args.frame+0x80;t.seed(0,1);t.descriptor(0,4,1,0);tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="value-count-frame-alias";t.words=t.args.frame+0x20;t.seed(3,0);t.input[8]=1;t.input[9]=0;t.input[10]=0;t.input[11]=0;tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="word-cursor-alias";t.cursor=t.words;t.seed(1,0);tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="stream-word-alias";t.args.stream=t.words;t.seed(1,0);tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="no-provided-primitive";t.seed(0,0);t.noService=true;tests.push_back(std::move(t));}
 {Monstey_Case t;t.name="pool-u64-wrap";t.pool=0xFFFFFFFFFFFFFFFEull;t.seed(3,1);t.descriptor(0,14,3,0);for(std::size_t i=0;i<12;++i)t.input[8+i]=0;t.input[12]=1;t.input[16]=2;tests.push_back(std::move(t));}
 for(const auto remaining:std::array<std::size_t,8>{1,2,3,7,8,15,24,32}){Monstey_Case t;t.name="publication-cut-"+std::to_string(remaining);t.seed(0,0);t.args.record=Monstey_Case::Base+t.initial.size()-remaining;tests.push_back(std::move(t));}
 for(const auto remaining:std::array<std::size_t,7>{0,1,7,8,15,16,24}){Monstey_Case t;t.name="word-cut-"+std::to_string(remaining);t.seed(3,0);t.initial.resize(0x7000+remaining);tests.push_back(std::move(t));}
 return tests;
}
} // namespace Monstey_AnonymousCorpus
