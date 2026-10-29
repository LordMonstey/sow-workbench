#include "Monstey_FearUnpackedLine.hpp"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <random>
#include <vector>
namespace {
bool Monstey_FailAllocation=false;
std::size_t Monstey_AllocationBudget=0;
}
void* operator new(std::size_t n) {
 if(Monstey_FailAllocation){if(Monstey_AllocationBudget==0)throw std::bad_alloc();--Monstey_AllocationBudget;}
 if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace {
using Monstey_U32=std::uint32_t;
using Monstey_Byte=std::uint8_t;
struct Monstey_RefFault{Monstey_U32 address,width;bool write;};
struct Monstey_Abort{};
struct Monstey_Arena {
 Monstey_U32 base=0;std::vector<Monstey_Byte> bytes=std::vector<Monstey_Byte>(0x1000,0xCC);
 Monstey_U32 args=0x100,scratch=0x800,input=0x300;
 Monstey_U32 Read(Monstey_U32 at,Monstey_U32 width)const{
  if(at<base||std::uint64_t(at)+width>0x100000000ull||std::uint64_t(at)-base>bytes.size()||width>bytes.size()-static_cast<std::size_t>(at-base))throw Monstey_RefFault{at,width,false};
  Monstey_U32 v=0;for(auto n=width;n;--n)v=(v<<8)|bytes[static_cast<std::size_t>(at-base)+n-1];return v;
 }
 void Write(Monstey_U32 at,Monstey_U32 v,Monstey_U32 width){
  if(at<base||std::uint64_t(at)+width>0x100000000ull||std::uint64_t(at)-base>bytes.size()||width>bytes.size()-static_cast<std::size_t>(at-base))throw Monstey_RefFault{at,width,true};
  for(Monstey_U32 n=0;n<width;++n){bytes[static_cast<std::size_t>(at-base)+n]=static_cast<Monstey_Byte>(v);v>>=8;}
 }
 void Init(std::span<const Monstey_Byte> data){
  for(std::size_t i=0;i<data.size();++i)Write(input+static_cast<Monstey_U32>(i),data[i],1);
  Write(args,input,4);Write(args+4,static_cast<Monstey_U32>(data.size()),4);Write(args+8,0x1234,4);Write(args+12,0x5678,4);
 }
 Monstey::Monstey_FearDbMutableMemory Memory(){return{base,bytes};}
};
struct Monstey_Event {
 Monstey_U32 callback,context;std::vector<Monstey_Byte> line,tail;
 bool operator==(const Monstey_Event&)const=default;
};
struct Monstey_Callback {
 bool enabled=true;unsigned mode=0;std::vector<Monstey_Event> events;
 void Apply(Monstey_Arena& a,Monstey_U32 callback,Monstey_U32 line,Monstey_U32 context) {
  Monstey_Event e{callback,context,{},{}};
  for(Monstey_U32 i=0;i<1024;++i){const auto c=static_cast<Monstey_Byte>(a.Read(line+i,1));if(!c)break;e.line.push_back(c);}
  for(Monstey_U32 i=0;i<8;++i)e.tail.push_back(static_cast<Monstey_Byte>(a.Read(line+i,1)));
  events.push_back(std::move(e));
  if(events.size()!=1)return;
  if(mode==1)a.Write(a.input+2,'Z',1);
  if(mode==2){a.Write(a.args,0x500,4);a.Write(0x502,'Y',1);a.Write(0x503,10,1);}
  if(mode==3)a.Write(a.args+4,0,4);
  if(mode==4)a.Write(a.args+4,6,4);
  if(mode==5){a.Write(a.args+8,0x9999,4);a.Write(a.args+12,0x4444,4);}
  if(mode==6)a.Write(line-1,5,1);
  if(mode==7)a.Write(line+2,'Z',1);
  if(mode==8){a.Write(a.input+2,'Q',1);throw Monstey_Abort{};}
 }
};
// Independent state machine follows original data/register epochs, including the
// final length sentinel and explicit prologue store widths. No native code runs.
Monstey_Byte Monstey_Reference(Monstey_Arena& a,Monstey_Callback& cb){
 a.Write(a.scratch,0,1);
 for(Monstey_U32 p=1;p<=1017;p+=4)a.Write(a.scratch+p,0,4);
 a.Write(a.scratch+1021,0,2);a.Write(a.scratch+1023,0,1);a.Write(a.scratch-1,0,1);
 Monstey_U32 cursor=0,used=0;
 do{
  auto bound=a.Read(a.args+4,4);Monstey_U32 advance=1;bool boundary=cursor==bound;Monstey_Byte c=0;
  if(!boundary){c=static_cast<Monstey_Byte>(a.Read(a.Read(a.args,4)+cursor,1));boundary=c==0;
   if(c==10)boundary=true;
   if(c==13){if(cursor+1==bound)boundary=true;else if(a.Read(a.Read(a.args,4)+cursor+1,1)==10){boundary=true;advance=2;}}}
  if(boundary){
   if(a.Read(a.scratch-1,1)!=0)a.Write(a.scratch-1,0,1);
   else if(used){
    a.Write(a.scratch+used,0,1);const auto context=a.Read(a.args+12,4),callback=a.Read(a.args+8,4);
    if(!cb.enabled)throw Monstey::Monstey_FearUnpackedLineBoundaryFault(callback,a.scratch,context);
    cb.Apply(a,callback,a.scratch,context);bound=a.Read(a.args+4,4);
   }
   used=0;cursor+=advance;
  }else if(used==0&&c==';'){a.Write(a.scratch-1,1,1);++cursor;}
  else{a.Write(a.scratch+used,c,1);++used;if(used==1024)return 0;++cursor;}
  if(cursor>bound)return 1;
 }while(true);
}
struct Monstey_Result {unsigned kind=0;Monstey_U32 value=0,address=0,width=0;bool write=false;};
struct Monstey_Stats {std::uint64_t groups=0,cases=0,checks=0,faults=0;};
Monstey_Stats Monstey_Counts;
void Monstey_Need(bool b,const char* text){++Monstey_Counts.checks;if(!b)throw std::runtime_error(text);}
void Monstey_Compare(Monstey_Arena input,unsigned mode=0,bool enabled=true) {
 ++Monstey_Counts.cases;auto expected=input,actual=input;Monstey_Callback e{enabled,mode,{}},p{enabled,mode,{}};Monstey_Result er,pr;
 try{er.value=Monstey_Reference(expected,e);}catch(const Monstey_RefFault& f){er={1,0,f.address,f.width,f.write};}
 catch(const Monstey::Monstey_FearUnpackedLineBoundaryFault& f){er={2,f.callback_identity,f.context_identity,f.line_identity,false};}
 catch(const Monstey_Abort&){er.kind=3;}
 Monstey::Monstey_FearUnpackedLineDispatch dispatch;
 if(enabled)dispatch=[&](auto memory,auto callback,auto line,auto context){
  Monstey_Need(memory.bytes.data()==actual.bytes.data()&&memory.base==actual.base,"callback must borrow live backing");
  p.Apply(actual,callback,line,context);return Monstey_U32{0};
 };
 auto actual_memory=actual.Memory();
 try{pr.value=Monstey::Monstey_FearUnpackedFrameLines(actual_memory,actual.args,actual.scratch,dispatch);}
 catch(const Monstey::Monstey_FearDbFault& f){pr={1,0,f.address,f.width,f.writing};}
 catch(const Monstey::Monstey_FearUnpackedLineBoundaryFault& f){pr={2,f.callback_identity,f.context_identity,f.line_identity,false};}
 catch(const Monstey_Abort&){pr.kind=3;}
 Monstey_Need(er.kind==pr.kind&&er.value==pr.value&&er.address==pr.address&&er.width==pr.width&&er.write==pr.write,"return/fault boundary mismatch");
 Monstey_Need(expected.bytes==actual.bytes,"line whole-memory/prefix mismatch");Monstey_Need(e.events==p.events,"callback/order/live input mismatch");
 if(er.kind)++Monstey_Counts.faults;
}
void Monstey_EnumeratedLines(){
 ++Monstey_Counts.groups;
 constexpr std::array<Monstey_Byte,6> alphabet{'a',';',13,10,0,'/'};
 std::uint64_t combinations=1;
 for(unsigned length=0;length<=6;++length){for(std::uint64_t value=0;value<combinations;++value){auto x=value;std::vector<Monstey_Byte> text(length);for(auto&c:text){c=alphabet[x%alphabet.size()];x/=alphabet.size();}Monstey_Arena a;a.Init(text);Monstey_Compare(a);}combinations*=alphabet.size();}
 for(unsigned c=0;c<256;++c){const std::array<Monstey_Byte,3> text{static_cast<Monstey_Byte>(c),13,'x'};Monstey_Arena a;a.Init(text);Monstey_Compare(a);}
}
void Monstey_EdgesAndMutations(){
 ++Monstey_Counts.groups;
 for(unsigned n:std::array<unsigned,8>{0,1,1022,1023,1024,1025,2048,3072}) {
  Monstey_Arena a;a.input=0xC00;a.bytes.resize(0xC00+n+4);std::vector<Monstey_Byte> text(n,'a');a.Init(text);Monstey_Compare(a);
 }
 const std::array<Monstey_Byte,6> text{'a',10,'b',10,'c',10};
 for(unsigned mode=0;mode<=8;++mode){Monstey_Arena a;a.Init(text);if(mode==4)a.Write(a.args+4,2,4);Monstey_Compare(a,mode);}
 {Monstey_Arena a;a.Init(text);a.Write(a.args+4,1,4);Monstey_Compare(a,4);}
 {Monstey_Arena a;const std::array<Monstey_Byte,6> t{'a','b','c',10,'q',10};a.Init(t);Monstey_Compare(a,7);}
 for(bool empty:{false,true}){Monstey_Arena a;std::vector<Monstey_Byte> t=empty?std::vector<Monstey_Byte>{}:std::vector<Monstey_Byte>{'x'};a.Init(t);Monstey_Compare(a,0,false);}
 for(auto pointer:std::array<Monstey_U32,3>{0x7FF,0x801,0x810}){Monstey_Arena a;a.Init(text);a.Write(a.args,pointer,4);Monstey_Compare(a);}
 for(auto args:std::array<Monstey_U32,3>{0x7F8,0x7FC,0x810}){Monstey_Arena a;a.args=args;a.Init(text);Monstey_Compare(a);}
 {Monstey_Arena a;a.Init({});a.Write(a.args+4,0xFFFFFFFF,4);Monstey_Compare(a);}
}
void Monstey_FaultCases(){
 ++Monstey_Counts.groups;const std::array<Monstey_Byte,4> t{'a',10,'b',10};
 for(std::size_t cut=0;cut<=0xC10;++cut){Monstey_Arena a;a.Init(t);a.bytes.resize(cut);Monstey_Compare(a);}
 for(std::size_t cut=0xE00;cut<=0xE10;++cut){Monstey_Arena a;a.args=0xE00;a.Init(t);a.bytes.resize(cut);Monstey_Compare(a);}
 {Monstey_Arena a;a.input=0xFF0;std::vector<Monstey_Byte> text(16,'x');a.Init(text);a.Write(a.args+4,32,4);Monstey_Compare(a);}
}
void Monstey_BoundaryInitCases(){
 ++Monstey_Counts.groups;const std::array<Monstey_Byte,4> t{'a',10,'b',10};
 {Monstey_Arena a;a.base=0x800;a.args=0xC00;a.scratch=0x800;a.input=0xD00;a.bytes.resize(0x1000);a.Init(t);Monstey_Compare(a);}
 {Monstey_Arena a;a.base=0xFFFFFBFFu;a.scratch=0xFFFFFC00u;a.bytes.resize(1025);Monstey_Compare(a);}
 {Monstey_Arena a;a.base=0xFFFFFFFEu;a.scratch=0xFFFFFFFEu;a.bytes.resize(4);Monstey_Compare(a);}
}
std::uint32_t Monstey_Find(const std::string& text,std::uint32_t from){
 std::uint32_t found=0xFFFFFFFFu;for(std::size_t i=0;i<text.size()&&i<=from;++i)if(text[i]=='/')found=static_cast<std::uint32_t>(i);return found;
}
std::string Monstey_Slice(const std::string& s,std::uint32_t at,std::uint32_t count){
 if(at>s.size())throw std::out_of_range("Reference string range");const auto n=std::min<std::size_t>(s.size()-at,count);
 return std::string(s.begin()+at,s.begin()+at+n);
}
void Monstey_KeyReference(const std::string& key,std::string& category,std::string& record,std::string& attribute){
 const auto a=Monstey_Find(key,0xFFFFFFFFu);attribute=Monstey_Slice(key,a+1u,0xFFFFFFFFu);
 const auto b=Monstey_Find(key,a-1u);record=Monstey_Slice(key,b+1u,a-b-1u);category=Monstey_Slice(key,0,b);
}
std::vector<std::array<unsigned,4>> Monstey_Partitions(){
 std::vector<std::array<unsigned,4>> out;
 for(unsigned b=0;b<=1;++b)for(unsigned c=0;c<=2;++c)for(unsigned d=0;d<=3;++d){
  if(c>1+std::max(0u,b)||d>1+std::max(b,c))continue;out.push_back({0,b,c,d});
 }return out;
}
void Monstey_KeyCases(){
 ++Monstey_Counts.groups;const auto partitions=Monstey_Partitions();Monstey_Need(partitions.size()==15,"all Bell4 alias partitions");
 const std::array<char,3> alphabet{'a','/',0};std::uint64_t ways=1;
 for(unsigned length=0;length<=7;++length){for(std::uint64_t v=0;v<ways;++v){auto x=v;std::string key(length,'x');for(auto&c:key){c=alphabet[x%3];x/=3;}
  for(const auto& p:partitions){std::array<std::string,4> expected{key,"C","R","A"},actual=expected;
   Monstey_KeyReference(expected[p[0]],expected[p[1]],expected[p[2]],expected[p[3]]);
   Monstey::Monstey_FearUnpackedSplitKey(actual[p[0]],actual[p[1]],actual[p[2]],actual[p[3]]);
   ++Monstey_Counts.cases;Monstey_Need(expected==actual,"key whole-object alias/slice mismatch");
  }}ways*=3;}
}
void Monstey_KeyAllocationFaults(){
 ++Monstey_Counts.groups;
 const auto c=std::string(100,'c'),r=std::string(100,'r'),a=std::string(100,'a'),key=c+"/"+r+"/"+a;
 for(std::size_t budget:std::array<std::size_t,3>{0,2,4}){
  std::string cat="OLD-C",rec="OLD-R",attr="OLD-A";bool failed=false;
  Monstey_AllocationBudget=budget;Monstey_FailAllocation=true;
  try{Monstey::Monstey_FearUnpackedSplitKey(key,cat,rec,attr);}catch(const std::bad_alloc&){failed=true;}
  Monstey_FailAllocation=false;++Monstey_Counts.cases;
  Monstey_Need(failed,"owned string failure injection must reach allocation");
  Monstey_Need(attr==(budget>=2?a:"OLD-A")&&rec==(budget>=4?r:"OLD-R")&&cat=="OLD-C","earlier string writes retained on exception");
 }
}
}
int main(int argc,char**argv){
 try{
  if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("New report path required");
  Monstey_EnumeratedLines();Monstey_EdgesAndMutations();Monstey_FaultCases();Monstey_BoundaryInitCases();Monstey_KeyCases();Monstey_KeyAllocationFaults();
  std::ofstream out(argv[1],std::ios::binary);if(!out)throw std::runtime_error("Report unavailable");
  out<<"{\"status\":\"PASS\",\"groups\":"<<Monstey_Counts.groups<<",\"cases\":"<<Monstey_Counts.cases<<",\"checks\":"<<Monstey_Counts.checks<<",\"fault_cases\":"<<Monstey_Counts.faults<<"}\n";
  std::cout<<"PASS "<<Monstey_Counts.cases<<" cases "<<Monstey_Counts.checks<<" checks\n";return 0;
 }catch(const std::exception& e){Monstey_FailAllocation=false;std::cerr<<e.what()<<'\n';return 1;}
 catch(const Monstey_RefFault&){Monstey_FailAllocation=false;std::cerr<<"Test fixture escaped owned range\n";return 1;}
}
