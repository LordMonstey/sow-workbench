#include "Monstey_FearUnpackedCleanup.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
namespace {
using U=std::uint32_t;using Mem=Monstey::Monstey_FearDbMutableMemory;using Services=Monstey::Monstey_FearUnpackedCleanupServices;
struct Event {unsigned kind;U head,count,address;};
struct Arena {std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x800,0xCC);Mem memory{0,bytes};std::vector<Event>events;static constexpr U map=0x100,frame=0x400,head=0x200,next=0x220;
 U Read(U p)const{if(std::uint64_t(p)+4>memory.bytes.size())throw std::runtime_error("test read");U v=0;for(unsigned k=4;k;--k)v=v*256+memory.bytes[p+k-1];return v;}
 void Write(U p,U v){if(std::uint64_t(p)+4>memory.bytes.size())throw std::runtime_error("test write");for(unsigned k=0;k<4;++k){memory.bytes[p+k]=static_cast<std::uint8_t>(v);v>>=8;}}
 Arena(){Write(map+4,head);Write(map+8,7);Write(head,0x280);Write(next,0x2A0);}
 void Log(unsigned kind,U address=0){events.push_back({kind,Read(map+4),Read(map+8),address});}
};
struct Sentinel {Arena*arena;unsigned kind;~Sentinel(){arena->Log(kind);arena->Write(Arena::map+4,Arena::next);}};
void Reference(Arena&a,bool parsed,const Services&s){const auto head=a.Read(Arena::map+4),first=a.Read(head);a.Write(Arena::frame-4,head);a.Write(Arena::frame-8,first);a.Write(Arena::frame-12,Arena::frame+4);
 // Retention bounded to the consumed call expression, before the next native operand.
 if(!s.erase_range)throw std::runtime_error("reference erase unavailable");(void)s.erase_range(a.memory,{parsed,parsed?0x100061E2u:0x100061B2u,Arena::map,a.Read(Arena::frame-12),a.Read(Arena::frame-8),a.Read(Arena::frame-4)});
 const U selected=a.Read(Arena::map+4);a.Write(Arena::frame-4,selected);if(!s.delete_scalar)throw std::runtime_error("reference delete unavailable");s.delete_scalar(a.memory,{parsed?0x100061EBu:0x100061BBu,a.Read(Arena::frame-4)});a.Write(Arena::map+4,0);a.Write(Arena::map+8,0);
}
struct Result {U head,count,deleted;bool destructor_saw_zero;std::vector<Event>events;};
Result Run(bool product,bool eraseTarget,bool parsed){Arena a;Services s;U deleted=0;bool destructor_saw_zero=false;
 s.erase_range=[&](Mem&,auto c){a.Log(1,c.native_site);return 0u;};s.delete_scalar=[&](Mem&,auto c){deleted=c.address;a.Log(3,c.address);};
 auto token=std::make_shared<Sentinel>();token->arena=&a;token->kind=eraseTarget?2u:4u;
 if(eraseTarget)s.erase_range=[&s,&a,token](Mem&,auto c){auto*services=&s;auto*arena=&a;const auto site=c.native_site;services->erase_range=[arena](Mem&,auto next){arena->Log(5,next.native_site);return 0u;};arena->Log(1,site);return 0u;};
 else s.delete_scalar=[&s,&a,&deleted,token](Mem&,auto c){auto*services=&s;auto*arena=&a;auto*selected=&deleted;const auto address=c.address;services->delete_scalar=[arena](Mem&,auto next){arena->Log(6,next.address);};*selected=address;arena->Log(3,address);};
 token.reset();if(product)Monstey::Monstey_FearUnpackedCleanup(a.memory,Arena::map,Arena::frame,parsed,s);else Reference(a,parsed,s);
 for(const auto&e:a.events)if((e.kind==2||e.kind==4)&&e.head==0&&e.count==0)destructor_saw_zero=true;
 return{a.Read(Arena::map+4),a.Read(Arena::map+8),deleted,destructor_saw_zero,a.events};
}
void Emit(std::ostream&out,const Result&r){out<<"{\"head\":"<<r.head<<",\"count\":"<<r.count<<",\"deleted\":"<<r.deleted<<",\"old_target_destructor_observed_both_zero\":"<<(r.destructor_saw_zero?"true":"false")<<",\"events\":[";bool first=true;for(const auto&e:r.events){if(!first)out<<',';first=false;out<<"{\"kind\":"<<e.kind<<",\"head\":"<<e.head<<",\"count\":"<<e.count<<",\"address_or_site\":"<<e.address<<'}';}out<<"]}";}
}

namespace {
struct EpochFault{U address,width;bool writing;};struct EpochAbort{};
struct EpochEvent{unsigned kind;bool parsed;U site,map,hidden,first,last,address,serial;bool operator==(const EpochEvent&)const=default;};
struct EpochArena{
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x800,0xCC),changed;Mem memory{0,bytes};U map=0x100,frame=0x400,head=0x200;unsigned mode=0;std::vector<EpochEvent> events;
 EpochArena(unsigned layout,unsigned mutation):mode(mutation){if(layout==1)map=frame-8;if(layout==2)map=frame-12;if(layout==3)head=frame-4;if(layout==4)head=frame-8;if(layout==5)map=0x1F8;W(head,0x280);W(0x220,0x2A0);W(map+4,head);W(map+8,7);}
 std::size_t At(U p,bool w)const{if(p<memory.base||std::uint64_t(p)+4>0x100000000ull||std::uint64_t(p)-memory.base>memory.bytes.size()||4>memory.bytes.size()-static_cast<std::size_t>(std::uint64_t(p)-memory.base))throw EpochFault{p,4,w};return p-memory.base;}
 U R(U p)const{const auto at=At(p,false);U v=0;for(unsigned k=4;k;--k)v=v*256+memory.bytes[at+k-1];return v;}
 void W(U p,U v){const auto at=At(p,true);for(unsigned k=0;k<4;++k){memory.bytes[at+k]=static_cast<std::uint8_t>(v);v>>=8;}}
 Services Bind(bool erase=true,bool destroy=true){Services s;
  if(erase)s.erase_range=[this,n=0u](Mem&m,auto c)mutable{if(&m!=&memory)throw std::runtime_error("erase must receive actual view reference");events.push_back({1,c.parsed,c.native_site,c.map,c.hidden_result,c.first,c.last,0,++n});
   if(mode==1)W(map+4,0x220);if(mode==2)W(c.hidden_result,0xABCDEF01);if(mode==3)W(frame-4,0xDEADBEEF);if(mode==5)throw EpochAbort{};
   if(mode==7){changed.assign(memory.bytes.begin(),memory.bytes.end());m={0,changed};W(map+4,0x220);}
   if(mode==9){changed=bytes;m={0,std::span(changed).first(map+6)};}
   return UINT32_MAX;};
  if(destroy)s.delete_scalar=[this,n=0u](Mem&m,auto c)mutable{if(&m!=&memory)throw std::runtime_error("delete must receive actual view reference");events.push_back({2,false,c.native_site,0,0,0,0,c.address,++n});if(mode==4){W(map+4,0xDDDD);W(map+8,0xEEEE);}if(mode==6)throw EpochAbort{};
   if(mode==8){changed=bytes;m={0,std::span(changed).first(map+10)};}
   if(mode==10){changed=bytes;const U base=map+8;m={base,std::span(changed).subspan(base)};}
  };return s;
 }
};
void EpochReference(EpochArena&a,bool parsed,const Services&s){const U h=a.R(a.map+4),first=a.R(h);a.W(a.frame-4,h);a.W(a.frame-8,first);a.W(a.frame-12,a.frame+4);if(!s.erase_range)throw Monstey::Monstey_FearUnpackedCleanupBoundaryFault("erase_range");
 (void)s.erase_range(a.memory,{parsed,parsed?0x100061E2u:0x100061B2u,a.map,a.R(a.frame-12),a.R(a.frame-8),a.R(a.frame-4)});
 const U current=a.R(a.map+4);a.W(a.frame-4,current);if(!s.delete_scalar)throw Monstey::Monstey_FearUnpackedCleanupBoundaryFault("delete_scalar");s.delete_scalar(a.memory,{parsed?0x100061EBu:0x100061BBu,a.R(a.frame-4)});a.W(a.map+4,0);a.W(a.map+8,0);
}
struct EpochResult{unsigned kind=0;U address=0,width=0;bool writing=false;std::string message;bool operator==(const EpochResult&)const=default;};
template<class F> EpochResult Capture(F f){try{f();return{};}catch(const EpochFault&e){return{1,e.address,e.width,e.writing,{}};}catch(const Monstey::Monstey_FearDbFault&e){return{1,e.address,e.width,e.writing,{}};}catch(const EpochAbort&){return{2,0,0,false,{}};}catch(const Monstey::Monstey_FearUnpackedCleanupBoundaryFault&e){return{3,0,0,false,e.what()};}}
std::uint64_t EpochCases=0,EpochChecks=0,EpochFaults=0;
void Check(bool ok,const char*text){++EpochChecks;if(!ok)throw std::runtime_error(text);}
void Compare(unsigned layout,unsigned mode,bool parsed,std::size_t cut=0x800,bool erase=true,bool destroy=true){EpochArena expected(layout,mode),actual(layout,mode);expected.memory.bytes=expected.memory.bytes.first(cut);actual.memory.bytes=actual.memory.bytes.first(cut);auto e=expected.Bind(erase,destroy),p=actual.Bind(erase,destroy);
 const auto er=Capture([&]{EpochReference(expected,parsed,e);}),pr=Capture([&]{Monstey::Monstey_FearUnpackedCleanup(actual.memory,actual.map,actual.frame,parsed,p);});
 Check(er==pr,"native cleanup result/consumed fault prefix");Check(expected.bytes==actual.bytes&&expected.changed==actual.changed,"native cleanup all backing and retained writes");Check(expected.events==actual.events,"native cleanup argument/read/target epochs and sites");Check(expected.memory.base==actual.memory.base&&expected.memory.bytes.size()==actual.memory.bytes.size(),"native cleanup replacement view propagates");++EpochCases;if(er.kind)++EpochFaults;
}
void Ordinary(){for(bool parsed:{false,true}){
 for(unsigned layout=0;layout<6;++layout)for(unsigned mode=0;mode<=10;++mode)Compare(layout,mode,parsed);
 for(std::size_t cut=0;cut<=0x410;++cut)Compare(0,0,parsed,cut);
 Compare(0,0,parsed,0x800,false,true);Compare(0,0,parsed,0x800,true,false);
 {EpochArena expected(0,0),actual(0,0);auto e=expected.Bind(),p=actual.Bind();for(unsigned iteration=0;iteration<2;++iteration){expected.W(expected.map+4,expected.head);actual.W(actual.map+4,actual.head);EpochReference(expected,parsed,e);Monstey::Monstey_FearUnpackedCleanup(actual.memory,actual.map,actual.frame,parsed,p);}Check(expected.events==actual.events&&actual.events[2].serial==2&&actual.events[3].serial==2,"mutable target state persists across invocations");++EpochCases;}
 {EpochArena expected(0,0),actual(0,0);for(auto*a:{&expected,&actual}){a->memory={0xFFFFFFFCu,std::span(a->bytes).first(8)};a->map=0xFFFFFFF8u;a->frame=0;a->W(0xFFFFFFFCu,0xFFFFFFFCu);}auto e=expected.Bind(),p=actual.Bind();const auto er=Capture([&]{EpochReference(expected,parsed,e);}),pr=Capture([&]{Monstey::Monstey_FearUnpackedCleanup(actual.memory,actual.map,actual.frame,parsed,p);});Check(er==pr&&er.kind==1&&er.address==0xFFFFFFF8u&&er.writing,"native32 wrap retains first outgoing store before next fault");Check(expected.bytes==actual.bytes,"native32 wrap prefix");++EpochCases;}
 }}
}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("new report path required");Ordinary();std::cout<<"ORDINARY_PASS "<<EpochCases<<" cases "<<EpochChecks<<" checks\n";
 for(bool parsed:{false,true})for(bool eraseTarget:{true,false}){const auto actual=Run(true,eraseTarget,parsed),expected=Run(false,eraseTarget,parsed);Check(actual.head==expected.head&&actual.count==expected.count&&actual.deleted==expected.deleted&&actual.destructor_saw_zero==expected.destructor_saw_zero,"D001 active target must release before next native operand/final zero stores");Check(actual.events.size()==expected.events.size(),"D001 lifetime phase count");for(std::size_t k=0;k<actual.events.size();++k)Check(actual.events[k].kind==expected.events[k].kind&&actual.events[k].head==expected.events[k].head&&actual.events[k].count==expected.events[k].count&&actual.events[k].address==expected.events[k].address,"D001 exact target-release and owned-write epochs");++EpochCases;}
 std::ofstream out(argv[1],std::ios::binary);out<<"{\"status\":\"PASS\",\"cases\":"<<EpochCases<<",\"checks\":"<<EpochChecks<<",\"fault_abort_boundary_cases\":"<<EpochFaults<<",\"D001_scoped_target_cases\":4,\"native_original_entrypoint_invoked\":false,\"original_instruction_bytes_interpreted\":false}\n";std::cout<<"PASS "<<EpochCases<<" cases "<<EpochChecks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const EpochFault&){std::cerr<<"bad independent fixture range\n";return 1;}}
