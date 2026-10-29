#include "Monstey_FearUnpackedLine.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
namespace {
using U=std::uint32_t;using B=std::uint8_t;using Mem=Monstey::Monstey_FearDbMutableMemory;using Dispatch=Monstey::Monstey_FearUnpackedLineDispatch;
std::uint64_t cases=0,checks=0;
void Need(bool p,const char*s){++checks;if(!p)throw std::runtime_error(s);}
struct Abort{};
struct Arena {std::vector<B> bytes=std::vector<B>(0x1400,0xCC);Mem memory{0,bytes};static constexpr U args=0x80,scratch=0x800,input=0x1000;
 void W(U at,U v){for(unsigned k=0;k<4;++k){bytes[at+k]=static_cast<B>(v);v>>=8;}}
 Arena(){W(args,input);W(args+4,4);W(args+8,0x1234);W(args+12,0x5678);bytes[input]='a';bytes[input+1]=10;bytes[input+2]='b';bytes[input+3]=10;}
 B Run(const Dispatch&d){return Monstey::Monstey_FearUnpackedFrameLines(memory,args,scratch,d);}
};
void State(){
 {Arena a;std::vector<U> seen;Dispatch d=[n=0u,&seen](Mem&,U,U,U)mutable{seen.push_back(++n);return n==1?0u:UINT32_MAX;};Need(a.Run(d)==1&&seen==std::vector<U>({1,2}),"mutable value-captured dispatch state persists");++cases;}
 {Arena a;Dispatch d;auto token=std::make_shared<int>(4);std::weak_ptr<int> weak=token;bool alive=false;unsigned second=0;
  d=[&,token](Mem&,U,U,U){auto observer=weak;auto*target=&d;auto*flag=&alive;auto*count=&second;*target=[count](Mem&,U,U,U){++*count;return 0u;};*flag=!observer.expired();return 0u;};token.reset();Need(a.Run(d)==1&&alive&&weak.expired()&&second==1,"dispatch self-replacement retains active target until return");++cases;}
 {Arena a;Dispatch d;auto token=std::make_shared<int>(5);std::weak_ptr<int> weak=token;bool alive=false;
  d=[&,token](Mem&,U,U,U){auto observer=weak;auto*target=&d;auto*flag=&alive;*target={};*flag=!observer.expired();return 0u;};token.reset();bool caught=false;try{(void)a.Run(d);}catch(const Monstey::Monstey_FearUnpackedLineBoundaryFault&e){caught=e.callback_identity==0x1234&&e.context_identity==0x5678&&e.line_identity==Arena::scratch;}Need(caught&&alive&&weak.expired()&&a.bytes[Arena::scratch]=='b'&&a.bytes[Arena::scratch+1]==0,"self-clear faults only at next consumed dispatch and retains writes");++cases;}
 {Arena a;Dispatch d;auto token=std::make_shared<int>(6);std::weak_ptr<int> weak=token;bool alive=false;
  d=[&,token](Mem&,U,U,U)->U{auto observer=weak;auto*target=&d;auto*flag=&alive;*target={};*flag=!observer.expired();throw Abort{};};token.reset();bool caught=false;try{(void)a.Run(d);}catch(const Abort&){caught=true;}Need(caught&&alive&&weak.expired()&&a.bytes[Arena::scratch]=='a'&&a.bytes[Arena::scratch+1]==0,"self-clear then throw retains active target and first terminator");++cases;}
 {Arena outer,inner;inner.W(Arena::args+4,2);Dispatch d;auto token=std::make_shared<int>(7);std::weak_ptr<int> weak=token;bool inside=false,afterNested=false;unsigned oldCalls=0,newCalls=0;
  d=[&,token](Mem&,U,U,U)->U{++oldCalls;auto observer=weak;auto*target=&d;auto*nested=&inner;auto*entry=&inside;auto*after=&afterNested;auto*newCount=&newCalls;if(*entry){*target=[newCount](Mem&,U,U,U){++*newCount;return 0u;};Need(!observer.expired(),"nested active target alive after replacement");return 0;}*entry=true;Need(nested->Run(*target)==1,"nested line invocation");*after=!observer.expired();return 0;};token.reset();Need(outer.Run(d)==1&&afterNested&&weak.expired()&&oldCalls==2&&newCalls==1,"outer active target retained after nested replacement");++cases;}
}
void Views(){
 {Arena old,next;old.W(Arena::args+4,2);next.bytes[Arena::scratch-1]=0;next.W(Arena::args,0x1100);next.W(Arena::args+8,0x9999);next.W(Arena::args+12,0xAAAA);next.bytes[0x1100]='q';next.bytes[0x1101]=10;next.bytes[0x1102]='Z';next.bytes[0x1103]=10;std::vector<std::array<U,3>> observed;
  Dispatch d=[&](Mem&m,U callback,U line,U context){observed.push_back({callback,context,m.bytes[line]});if(observed.size()==1)m=next.memory;return 0u;};Need(old.Run(d)==1,"rebound view AL");Need(observed==std::vector<std::array<U,3>>({{0x1234,0x5678,'a'},{0x9999,0xAAAA,'Z'}}),"immediate length and next pointer/identities use replaced view");Need(old.memory.bytes.data()==next.bytes.data()&&old.bytes[Arena::scratch]=='a'&&next.bytes[Arena::scratch]=='Z',"caller view replacement and disjoint backing prefixes retained");++cases;}
 {Arena old,next;next.W(Arena::args+4,0);unsigned calls=0;Dispatch d=[&](Mem&m,U,U,U){++calls;m=next.memory;return UINT32_MAX;};Need(old.Run(d)==1&&calls==1&&old.memory.bytes.data()==next.bytes.data(),"shorter rebound length consumed immediately");++cases;}
 {Arena old,next;Dispatch d=[&](Mem&m,U,U,U){m={0,std::span(next.bytes).first(0x86)};return 0u;};bool caught=false;try{(void)old.Run(d);}catch(const Monstey::Monstey_FearDbFault&e){caught=e.address==0x84&&e.width==4&&!e.writing;}Need(caught&&old.memory.bytes.size()==0x86&&old.bytes[Arena::scratch+1]==0,"replacement view exposes consumed length fault after terminator");++cases;}
 {Arena old,next;Dispatch d=[&](Mem&m,U,U,U)->U{m=next.memory;throw Abort{};};bool caught=false;try{(void)old.Run(d);}catch(const Abort&){caught=true;}Need(caught&&old.memory.bytes.data()==next.bytes.data()&&old.bytes[Arena::scratch]=='a'&&old.bytes[Arena::scratch+1]==0,"view replacement survives dispatch exception");++cases;}
 {Arena old,next;Dispatch d=[&](Mem&m,U,U,U){m={0x100,std::span(next.bytes).subspan(0x100)};return 0u;};bool caught=false;try{(void)old.Run(d);}catch(const Monstey::Monstey_FearDbFault&e){caught=e.address==0x84&&e.width==4&&!e.writing;}Need(caught&&old.memory.base==0x100,"rebound base is consumed on the immediate length read");++cases;}
}
}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("new report required");State();Views();std::ofstream out(argv[1],std::ios::binary);out<<"{\"status\":\"PASS\",\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"mutable_target_state_and_lifetime\":true,\"view_rebinding_propagates\":true,\"native_original_entrypoint_invoked\":false,\"original_instruction_bytes_interpreted\":false}\n";std::cout<<"PASS "<<cases<<" targeted state/lifetime/view cases "<<checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const Abort&){std::cerr<<"unexpected test abort\n";return 1;}}
