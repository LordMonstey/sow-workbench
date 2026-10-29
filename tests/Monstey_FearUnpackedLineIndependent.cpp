#include "Monstey_FearUnpackedLine.hpp"
#include "Monstey_LineReviewBytes.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <span>
#include <string>
#include <vector>
namespace {bool Monstey_Fail=false;std::size_t Monstey_Budget=0;}
void* operator new(std::size_t n){if(Monstey_Fail){if(!Monstey_Budget)throw std::bad_alloc();--Monstey_Budget;}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
namespace {
using Monstey_U32=std::uint32_t;using Monstey_Byte=std::uint8_t;
std::uint64_t Monstey_Cases=0,Monstey_Checks=0,Monstey_FaultCases=0,Monstey_Steps=0,Monstey_KeyCases=0,Monstey_AllocationCases=0;
std::array<bool,208> Monstey_Visited{};
void Monstey_Need(bool value,const char*label){++Monstey_Checks;if(!value)throw std::runtime_error(label);}
std::vector<Monstey_Byte> Monstey_Hex(std::string_view s){std::vector<Monstey_Byte> out;auto digit=[](char c){return c<='9'?c-'0':c-'a'+10;};for(std::size_t i=0;i<s.size();i+=2)out.push_back(static_cast<Monstey_Byte>(digit(s[i])*16+digit(s[i+1])));return out;}
struct Monstey_Fault{Monstey_U32 at,width;bool write;};struct Monstey_Abort{};
struct Monstey_Arena {
 Monstey_U32 base=0,args=0x80,scratch=0x600,input=0x1000;
 std::vector<Monstey_Byte> data=std::vector<Monstey_Byte>(0x2000,0xB7);
 std::size_t Check(Monstey_U32 p,unsigned n,bool write)const{
  if(p<base||std::uint64_t(p)+n>0x100000000ull||std::uint64_t(p)-base>data.size()||n>data.size()-static_cast<std::size_t>(std::uint64_t(p)-base))throw Monstey_Fault{p,n,write};return static_cast<std::size_t>(p-base);
 }
 Monstey_U32 Read(Monstey_U32 p,unsigned n=4)const{auto at=Check(p,n,false);Monstey_U32 v=0;for(unsigned k=n;k;--k)v=v*256+data[at+k-1];return v;}
 void Write(Monstey_U32 p,Monstey_U32 v,unsigned n=4){auto at=Check(p,n,true);for(unsigned k=0;k<n;++k){data[at+k]=static_cast<Monstey_Byte>(v);v>>=8;}}
 void Init(std::span<const Monstey_Byte> t){for(std::size_t i=0;i<t.size();++i)Write(input+static_cast<Monstey_U32>(i),t[i],1);Write(args,input);Write(args+4,static_cast<Monstey_U32>(t.size()));Write(args+8,0xF0040031);Write(args+12,0xC2080043);}
};
struct Monstey_Event {Monstey_U32 callback,line,context;std::array<Monstey_Byte,1024> bytes;bool operator==(const Monstey_Event&)const=default;};
struct Monstey_Callback {
 unsigned mode=0;bool enabled=true;std::vector<Monstey_Event> events;
 Monstey_U32 Apply(Monstey_Arena&a,Monstey_U32 callback,Monstey_U32 line,Monstey_U32 context){
  Monstey_Event e{callback,line,context,{}};for(unsigned i=0;i<1024;++i)e.bytes[i]=static_cast<Monstey_Byte>(a.Read(line+i,1));events.push_back(e);
  if(events.size()==1)switch(mode){
   case 1:a.Write(a.input+2,'Z',1);break;
   case 2:a.Write(a.args,a.input+0x100);a.Write(a.input+0x102,'P',1);a.Write(a.input+0x103,10,1);a.Write(a.input+0x104,0,1);break;
   case 3:a.Write(a.args+4,0);break;
   case 4:a.Write(a.args+4,6);break;
   case 5:a.Write(a.args+8,0x42);a.Write(a.args+12,0x99);break;
   case 6:a.Write(line-1,0x80,1);break;
   case 7:a.Write(line+12,0x77,1);break;
   case 8:a.Write(a.input+2,'T',1);throw Monstey_Abort{};
   case 9:a.Write(a.args,0xFFFFFFFDu);break;
   case 10:a.Write(a.args+4,1);break;
   case 11:a.Write(line,0x5A,1);a.Write(a.args+8,0);a.Write(a.args+12,0xAABBCCDD);break;
   case 12:a.Write(a.args+4,5);a.Write(a.input+2,13,1);a.Write(a.input+3,'r',1);a.Write(a.input+4,13,1);break;
   default:break;
  }
  return (events.size()&1)?0:0xFFFFFFFFu;
 }
};
// Owned bounded instruction-data interpreter. Register/control operands are private;
// only observed argument/scratch/input operands map to the supplied native32 arena.
// Prologue/epilogue stack traffic and callback ABI are explicit excluded boundaries.
Monstey_Byte Monstey_Oracle(Monstey_Arena&a,Monstey_Callback&cb){
 static const auto code=Monstey_Hex(Monstey_LineReview::Monstey_LineHex);
 unsigned pc=0x0F;Monstey_U32 eax=0,ebx=0,ecx=0,edx=0,esi=0,edi=0,context=0;bool equal=false,below=false;
 auto cmp=[&](Monstey_U32 x,Monstey_U32 y){equal=x==y;below=x<y;};
 auto al=[&](Monstey_U32 v){eax=(eax&0xFFFFFF00u)|(v&255);};
 for(unsigned budget=0;budget<1000000;++budget){
  const auto found=std::find_if(Monstey_LineReview::Monstey_Instructions.begin(),Monstey_LineReview::Monstey_Instructions.end(),[&](const auto&i){return i.offset==pc;});
  if(found==Monstey_LineReview::Monstey_Instructions.end())throw std::runtime_error("Oracle PC outside selected instructions");
  const auto literal=Monstey_Hex(found->hex);if(!std::equal(literal.begin(),literal.end(),code.begin()+pc))throw std::runtime_error("Oracle instruction pin mismatch");
  const auto next=pc+static_cast<unsigned>(literal.size());Monstey_Visited[pc]=true;++Monstey_Steps;
  auto branch=[&](bool condition){return condition?static_cast<unsigned>(static_cast<int>(next)+static_cast<std::int8_t>(code[next-1])):next;};
  switch(pc){
   case 0x0F:ebx=0;break;case 0x11:eax=0;break;
   case 0x13:a.Write(a.scratch,ebx,1);break;case 0x17:ecx=255;break;
   case 0x1C:edi=a.scratch+1;break;
   case 0x20:while(ecx){a.Write(edi,eax,4);edi+=4;--ecx;}break;
   case 0x22:a.Write(edi,eax,2);edi+=2;break;case 0x24:a.Write(edi,eax,1);++edi;break;
   case 0x25:ecx=0;break;case 0x27:a.Write(a.scratch-1,ebx,1);break;case 0x2B:edi=0;break;case 0x2D:break;
   case 0x30:edx=a.Read(a.args+4);break;case 0x33:esi=0;break;case 0x35:cmp(edi,edx);break;
   case 0x37:pc=branch(equal);continue;case 0x39:eax=a.Read(a.args);break;
   case 0x3C:al(a.Read(edi+eax,1));break;case 0x3F:cmp(eax&255,ebx&255);break;
   case 0x41:pc=branch(equal);continue;case 0x43:cmp(eax&255,13);break;
   case 0x45:pc=branch(!equal);continue;case 0x47:esi=edi+1;break;case 0x4A:cmp(esi,edx);break;
   case 0x4C:pc=branch(equal);continue;case 0x4E:esi=a.Read(a.args);break;
   case 0x51:cmp(a.Read(edi+esi+1,1),10);break;case 0x56:pc=branch(!equal);continue;
   case 0x58:esi=2;break;case 0x5D:cmp(a.Read(a.scratch-1,1),ebx&255);break;
   case 0x61:pc=branch(equal);continue;case 0x63:a.Write(a.scratch-1,ebx,1);break;
   case 0x67:pc=branch(true);continue;case 0x69:cmp(eax&255,10);break;
   case 0x6B:pc=branch(!equal);continue;case 0x6D:esi=1;break;case 0x72:pc=branch(true);continue;
   case 0x74:cmp(ecx,ebx);break;case 0x76:pc=branch(!equal);continue;
   case 0x78:cmp(eax&255,59);break;case 0x7A:pc=branch(!equal);continue;
   case 0x7C:a.Write(a.scratch-1,1,1);break;case 0x81:pc=branch(true);continue;
   case 0x83:a.Write(a.scratch+ecx,eax,1);break;case 0x87:++ecx;break;
   case 0x88:cmp(ecx,1024);break;case 0x8E:pc=branch(!equal);continue;
   case 0x90:return 0;
   case 0x99:cmp(ecx,ebx);break;case 0x9B:pc=branch(equal);continue;
   case 0x9D:a.Write(a.scratch+ecx,ebx,1);break;case 0xA1:ecx=a.Read(a.args+12);break;
   case 0xA4:context=ecx;break;case 0xA5:edx=a.scratch;break;case 0xA9:break;
   case 0xAA:{const auto callback=a.Read(a.args+8);if(!cb.enabled)throw Monstey::Monstey_FearUnpackedLineBoundaryFault(callback,edx,context);eax=cb.Apply(a,callback,edx,context);break;}
   case 0xAD:edx=a.Read(a.args+4);break;case 0xB0:break;case 0xB3:ecx=0;break;
   case 0xB5:cmp(esi,1);break;case 0xB8:pc=branch(below||equal);continue;
   case 0xBA:edi=edi+esi-1;break;case 0xBE:++edi;break;case 0xBF:cmp(edi,edx);break;
   case 0xC1:{if(below||equal){const auto displacement=std::uint32_t(code[pc+2])|(std::uint32_t(code[pc+3])<<8)|(std::uint32_t(code[pc+4])<<16)|(std::uint32_t(code[pc+5])<<24);pc=next+displacement;}else pc=next;continue;}
   case 0xC7:return 1;
   default:throw std::runtime_error("Oracle encountered excluded instruction");
  }pc=next;
 }
 throw std::runtime_error("Oracle case exceeded bounded instruction budget");
}
struct Monstey_Result {unsigned kind=0;Monstey_U32 value=0,at=0,width=0;bool write=false;bool operator==(const Monstey_Result&)const=default;};
template<class F> Monstey_Result Monstey_Capture(F f){try{return {0,f(),0,0,false};}catch(const Monstey_Fault&e){return{1,0,e.at,e.width,e.write};}catch(const Monstey::Monstey_FearDbFault&e){return{1,0,e.address,e.width,e.writing};}catch(const Monstey::Monstey_FearUnpackedLineBoundaryFault&e){return{2,e.callback_identity,e.line_identity,e.context_identity,false};}catch(const Monstey_Abort&){return{3,0,0,0,false};}}
void Monstey_Compare(Monstey_Arena arena,unsigned mode=0,bool enabled=true){
 ++Monstey_Cases;auto expected=arena,actual=arena;Monstey_Callback e{mode,enabled,{}},p{mode,enabled,{}};
 auto er=Monstey_Capture([&]{return Monstey_Oracle(expected,e);});
 Monstey::Monstey_FearUnpackedLineDispatch dispatch;
 if(enabled)dispatch=[&](auto memory,auto callback,auto line,auto context){Monstey_Need(memory.base==actual.base&&memory.bytes.data()==actual.data.data(),"callback live backing");return p.Apply(actual,callback,line,context);};
 Monstey::Monstey_FearDbMutableMemory actual_memory{actual.base,actual.data};
 auto pr=Monstey_Capture([&]{return Monstey::Monstey_FearUnpackedFrameLines(actual_memory,actual.args,actual.scratch,dispatch);});
 Monstey_Need(er==pr,"line result or consumed operand fault");Monstey_Need(expected.data==actual.data,"line whole memory and retained prefix");Monstey_Need(e.events==p.events,"line callback epochs and residual bytes");if(er.kind)++Monstey_FaultCases;
}
void Monstey_Lines(){
 const std::array<Monstey_Byte,7> alphabet{'x',13,10,0,';',0x80,0xFF};std::uint64_t ways=1;
 for(unsigned len=0;len<=5;++len){for(std::uint64_t n=0;n<ways;++n){auto k=n;std::vector<Monstey_Byte> text(len);for(auto&c:text){c=alphabet[k%alphabet.size()];k/=alphabet.size();}Monstey_Arena a;a.Init(text);Monstey_Compare(a);}ways*=alphabet.size();}
 for(unsigned c=0;c<256;++c){Monstey_Arena a;const std::array<Monstey_Byte,5> t{'X',static_cast<Monstey_Byte>(c),13,'Q',13};a.Init(t);Monstey_Compare(a);}
 const std::array<Monstey_Byte,6> t{'a',10,'b',10,'c',10};
 for(unsigned mode=0;mode<=12;++mode){Monstey_Arena a;a.Init(t);Monstey_Compare(a,mode);}
 {Monstey_Arena a;a.Init(t);a.Write(a.args+4,1);Monstey_Compare(a,4);}
 for(unsigned len:{0u,1u,2u,1022u,1023u,1024u,1025u,2048u})for(Monstey_Byte first:{Monstey_Byte('a'),Monstey_Byte(';')}){Monstey_Arena a;std::vector<Monstey_Byte> text(len,'Q');if(len)text[0]=first;a.data.resize(0x1000+len+4);a.Init(text);Monstey_Compare(a);}
 for(unsigned size=0;size<=0xA10;++size){Monstey_Arena a;a.Init(t);a.data.resize(size);Monstey_Compare(a);}
 for(unsigned size=0x1800;size<=0x1810;++size){Monstey_Arena a;a.args=0x1800;a.Init(t);a.data.resize(size);Monstey_Compare(a);}
 for(auto ptr:{0x5FFu,0x600u,0x601u,0x620u,0xFFFFF000u}){Monstey_Arena a;a.Init(t);a.Write(a.args,ptr);Monstey_Compare(a);}
 for(auto arg:{0x5F0u,0x5F8u,0x5FCu,0x600u,0x610u,0x9F8u,0x9FCu}){Monstey_Arena a;a.args=arg;a.Init(t);Monstey_Compare(a);}
 for(auto scratch:{0u,1u,0x40u,0x80u,0x1000u}){Monstey_Arena a;a.scratch=scratch;a.Init(t);Monstey_Compare(a);}
 {Monstey_Arena a;a.Init(t);a.Write(a.args+4,0xFFFFFFFFu);Monstey_Compare(a);}
 for(bool empty:{false,true}){Monstey_Arena a;std::vector<Monstey_Byte> text(empty?0:1,'x');a.Init(text);Monstey_Compare(a,0,false);}
 {Monstey_Arena a;a.base=0xFFFFFBFFu;a.scratch=0xFFFFFC00u;a.data.resize(1025);Monstey_Compare(a);}
 {Monstey_Arena a;a.base=0xFFFFFFFDu;a.scratch=0xFFFFFFFEu;a.data.resize(4);Monstey_Compare(a);}
 {Monstey_Arena a;a.base=0x600;a.scratch=0x600;a.args=0xB00;a.input=0xC00;a.data.resize(0x1000);a.Init(t);Monstey_Compare(a);}
}
Monstey_U32 Monstey_Search(const std::string&s,Monstey_U32 limit){if(s.empty())return UINT32_MAX;auto k=std::min<std::uint64_t>(limit,s.size()-1);for(;;){if(s[static_cast<std::size_t>(k)]=='/')return static_cast<Monstey_U32>(k);if(!k)break;--k;}return UINT32_MAX;}
std::string Monstey_Slice(const std::string&s,Monstey_U32 pos,Monstey_U32 count){if(pos>s.size())throw std::out_of_range("owned byte substring");const auto len=std::min<std::size_t>(count,s.size()-pos);return std::string(s.data()+pos,len);}
void Monstey_KeyOracle(const std::string&s,std::string&c,std::string&r,std::string&a){
 const auto slash=Monstey_Search(s,UINT32_MAX);{const auto t=Monstey_Slice(s,slash+1u,UINT32_MAX);a=t;}
 const auto prior=Monstey_Search(s,slash-1u);{const auto t=Monstey_Slice(s,prior+1u,slash-prior-1u);r=t;}
 {const auto t=Monstey_Slice(s,0,prior);c=t;}
}
std::vector<std::array<unsigned,4>> Monstey_Partitions(){std::vector<std::array<unsigned,4>> p;for(unsigned i=0;i<=1;++i)for(unsigned j=0;j<=i+1;++j)for(unsigned k=0;k<=std::max(i,j)+1;++k)p.push_back({0,i,j,k});return p;}
template<class F> unsigned Monstey_KeyRun(F f){try{f();return 0;}catch(const std::out_of_range&){return 1;}catch(const std::bad_alloc&){return 2;}}
void Monstey_Keys(){const auto partitions=Monstey_Partitions();Monstey_Need(partitions.size()==15,"Bell4 alias partitions");
 const std::array<char,4> alphabet{'/','v',0,char(-15)};std::uint64_t ways=1;
 for(unsigned length=0;length<=5;++length){for(std::uint64_t n=0;n<ways;++n){auto x=n;std::string key(length,' ');for(auto&c:key){c=alphabet[x%4];x/=4;}
  for(auto p:partitions){std::array<std::string,4> e{key,"category-old","record-old","attribute-old"},a=e;const auto er=Monstey_KeyRun([&]{Monstey_KeyOracle(e[p[0]],e[p[1]],e[p[2]],e[p[3]]);});const auto ar=Monstey_KeyRun([&]{Monstey::Monstey_FearUnpackedSplitKey(a[p[0]],a[p[1]],a[p[2]],a[p[3]]);});++Monstey_Cases;++Monstey_KeyCases;Monstey_Need(er==ar,"key exception boundary");Monstey_Need(e==a,"key output order, live alias and U32 wrap");}
 }ways*=4;}
 const auto category=std::string(97,'C'),record=std::string(151,'R'),attribute=std::string(223,'A'),key=category+"/"+record+"/"+attribute;
 for(std::size_t budget=0;budget<=6;++budget){std::string c="oldC",r="oldR",a="oldA";Monstey_Budget=budget;Monstey_Fail=true;const auto result=Monstey_KeyRun([&]{Monstey::Monstey_FearUnpackedSplitKey(key,c,r,a);});Monstey_Fail=false;++Monstey_Cases;++Monstey_AllocationCases;
  Monstey_Need(result==(budget<6?2u:0u),"owned allocation injection reaches intended stage");Monstey_Need(a==(budget>=2?attribute:"oldA")&&r==(budget>=4?record:"oldR")&&c==(budget>=6?category:"oldC"),"key exception retains attribute-record-category prefix");}
 for(auto p:partitions)for(std::size_t budget=0;budget<=8;++budget){std::array<std::string,4> e{key,"C","R","A"},a=e;Monstey_Budget=budget;Monstey_Fail=true;const auto er=Monstey_KeyRun([&]{Monstey_KeyOracle(e[p[0]],e[p[1]],e[p[2]],e[p[3]]);});Monstey_Fail=false;Monstey_Budget=budget;Monstey_Fail=true;const auto ar=Monstey_KeyRun([&]{Monstey::Monstey_FearUnpackedSplitKey(a[p[0]],a[p[1]],a[p[2]],a[p[3]]);});Monstey_Fail=false;++Monstey_Cases;++Monstey_AllocationCases;Monstey_Need(er==ar&&e==a,"key aliased allocation/exception prefix");}
}
}
int main(int argc,char**argv){try{
 if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("fresh report path required");Monstey_Lines();Monstey_Keys();
 const auto visited=std::count(Monstey_Visited.begin(),Monstey_Visited.end(),true);std::ofstream out(argv[1],std::ios::binary);if(!out)throw std::runtime_error("report open");
 out<<"{\"status\":\"PASS\",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"line_fault_cases\":"<<Monstey_FaultCases<<",\"key_cases\":"<<Monstey_KeyCases<<",\"owned_allocation_cases\":"<<Monstey_AllocationCases<<",\"key_alias_partitions\":15,\"interpreted_steps\":"<<Monstey_Steps<<",\"interpreted_instruction_sites\":"<<visited<<",\"original_instruction_bytes_interpreted\":true,\"native_original_entrypoint_invoked\":false,\"physical_stack_and_import_ABI_qualified\":false}\n";
 std::cout<<"PASS "<<Monstey_Cases<<" cases "<<Monstey_Checks<<" checks "<<visited<<" mapped instruction sites\n";return 0;
 }catch(const std::exception&e){Monstey_Fail=false;std::cerr<<e.what()<<'\n';return 1;}catch(const Monstey_Fault&){Monstey_Fail=false;std::cerr<<"review fixture setup fault\n";return 1;}}
