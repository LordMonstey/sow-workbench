#include "Monstey_FearUnpackedCleanup.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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
int main(int argc,char**argv){try{
 if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("new report required");
 std::ofstream out(argv[1],std::ios::binary);out<<"{\"status\":\"PASS\",\"cases\":[";bool first=true;unsigned checks=0,cases=0;
 auto need=[&checks](bool value){++checks;if(!value)throw std::runtime_error("Consumed target retirement does not match call-scoped reference");};
 for(bool parsed:{false,true})for(bool eraseTarget:{true,false}){
  const auto actual=Run(true,eraseTarget,parsed),expected=Run(false,eraseTarget,parsed);
  need(actual.head==expected.head&&actual.count==expected.count);need(actual.deleted==expected.deleted);need(actual.destructor_saw_zero==expected.destructor_saw_zero);need(actual.events.size()==expected.events.size());
  for(std::size_t i=0;i<actual.events.size();++i){const auto&a=actual.events[i];const auto&e=expected.events[i];need(a.kind==e.kind&&a.head==e.head&&a.count==e.count&&a.address==e.address);}
  if(!first)out<<',';first=false;out<<"{\"parsed\":"<<(parsed?"true":"false")<<",\"erase_target\":"<<(eraseTarget?"true":"false")<<",\"actual\":";Emit(out,actual);out<<'}';++cases;
 }
 out<<"],\"case_count\":"<<cases<<",\"checks\":"<<checks<<",\"native_original_entrypoint_invoked\":false,\"original_instruction_bytes_interpreted\":false}\n";std::cout<<"PASS four call-scoped target retirement cases;checks="<<checks<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
