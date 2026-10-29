#include "Monstey_ConstraintFixtures.hpp"
#include "Monstey_FearConstraintTable.hpp"
#include "Monstey_Core.hpp"
#include <cstring>
#include <iostream>
#include <random>
namespace {
using namespace Monstey_Test;using namespace Monstey;
U64 Cases=0,Checks=0,Groups=0;
void Need(bool good,const char* label){++Checks;if(!good)throw std::runtime_error(label);}
struct ByteLess{bool operator()(const std::string& x,const std::string& y)const{const auto n=std::min(x.size(),y.size());const auto result=std::memcmp(x.data(),y.data(),n);return result<0||(result==0&&x.size()<y.size());}};
Monstey_FearConstraintTable Expected(const std::vector<U8>& data){
 Monstey_FearConstraintTable result;std::size_t at=0;std::map<std::string,std::array<U32,3>,ByteLess> rows;
 auto read=[&](U32 width,U32& value){++result.read_calls;if(width>data.size()-at){value=0;return false;}value=0;for(U32 i=0;i<width;++i)value|=U32(data[at++])<<(8*i);return true;};
 bool ok=true;while(at!=data.size()){
  U32 length=0,kind=0,ignored=0,a=0,b=0;if(!read(4,length)){ok=false;break;}
  ++result.read_calls;if(length>data.size()-at){ok=false;break;}std::string key;bool ended=false;for(U32 i=0;i<length;++i){const auto c=data[at++];if(!c)ended=true;if(!ended)key.push_back(static_cast<char>(c));}
  if(!read(1,kind)||!read(4,ignored)||!read(4,a)||!read(4,b)){ok=false;break;}rows[key]={kind,a,b};++result.records;
 }
 result.complete=ok;result.consumed=at;for(const auto& [name,words]:rows)result.rows.push_back({name,words[0],words[1],words[2]});return result;
}
void Equal(const Monstey_FearConstraintTable& actual,const Monstey_FearConstraintTable& expected){
 Need(actual.complete==expected.complete&&actual.consumed==expected.consumed&&actual.read_calls==expected.read_calls&&actual.records==expected.records&&actual.rows.size()==expected.rows.size(),"table/cursor/read-count reference");
 for(std::size_t i=0;i<actual.rows.size();++i){const auto& a=actual.rows[i];const auto& b=expected.rows[i];Need(a.key==b.key&&a.type==b.type&&a.word4==b.word4&&a.word8==b.word8,"raw constraint row reference");}++Cases;
}
void Format(){++Groups;std::mt19937 random(0x6030);
 for(U32 sample=0;sample<768;++sample){std::vector<U8> bytes;const U32 n=sample%13;for(U32 i=0;i<n;++i){std::string key;const U32 size=random()%24;for(U32 j=0;j<size;++j)key.push_back(static_cast<char>(random()&255));if(i%3==0)key="Duplicate";Record(bytes,key,sample&255,random(),random(),random());}Equal(Monstey_FearDecodeConstraints(bytes),Expected(bytes));}
 std::vector<U8> data;Record(data,"A/B",0,11,0x7FC00001,0xFFFFFFFF);Record(data,std::string("A/B\0ignored",11),255,22,0x80000000,0xDEADBEEF);Record(data,std::string(1,static_cast<char>(0x80)),6,33,2,3);Record(data,std::string(1,static_cast<char>(0x7F)),1,44,5,6);
 for(std::size_t cut=0;cut<=data.size();++cut){const std::vector<U8> prefix(data.begin(),data.begin()+static_cast<std::ptrdiff_t>(cut));Equal(Monstey_FearDecodeConstraints(prefix),Expected(prefix));}
}
void Failure(){++Groups;
 for(U32 failed=1;failed<=6;++failed)for(U32 status:{1u,0x100u,0x80000000u,0xFFFFFFFFu}){
  Fixture f;Record(f.input,"Name",0xE9,0x11111111,0x22334455,0x66778899);
  f.on_read=[failed,status](auto& x,auto){return x.reads==failed?std::optional<U32>(status):std::nullopt;};Need(!f.Run()&&f.reads==failed&&f.map_calls==0,"read failure is full-EAX and stops before map");
  Need(f.alloc_calls==(failed>=2?1u:0u)&&f.free_calls==(failed>=2?1u:0u)&&f.ctor_calls==(failed>=3?1u:0u)&&f.dtor_calls==(failed>=3?1u:0u),"exact failure cleanup prefix");++Cases;
 }
 Fixture empty;empty.services.read={};empty.services.new_array={};empty.services.map_value={};Need(empty.Run()&&empty.reads==0,"unused boundaries not required");++Cases;
 Fixture missing;missing.input={1};missing.services.read={};bool fault=false;try{(void)missing.Run();}catch(const Monstey_FearConstraintBoundaryFault&){fault=true;}Need(fault&&missing.Get(Frame+0x10)==0,"missing consumed read boundary after native zero store");++Cases;
}
void Epochs(){++Groups;
 {Fixture f;f.total=U64{1}<<32;Need(!f.Run()&&f.reads==1,"GetLen/GetPos high32 comparison");++Cases;}
 {Fixture f;f.total=1;f.on_position=[](auto& x){x.Put(Frame+0x20,0);x.Put(Frame+0x24,0);};Need(!f.Run()&&f.reads==1,"initial comparison uses cached registers not mutated frame");++Cases;}
 {Fixture f;Record(f.input,"A",1,10,20,30);Record(f.input,"B",2,40,50,60);f.on_position=[](auto& x){if(x.pos_calls==2){x.Put(Frame+0x20,static_cast<U32>(x.position));x.Put(Frame+0x24,0);}};Need(f.Run()&&f.map_calls==1&&f.position<f.input.size(),"loop compares live cached length slots");++Cases;}
 {Fixture f;f.stream=Frame+0x20;f.Put(Frame+0x48,f.stream);f.Put(f.stream,Vtable);Need(f.Run()&&f.pos_calls==1,"first position VT is sampled before cached-length stores");++Cases;}
 {Fixture f;Record(f.input,"AAAA",3,9,7,8);f.on_alloc=[](auto& x){x.Put(Frame+0x10,2);};f.on_read=[](auto& x,auto c)->std::optional<U32>{if(x.reads==2){Need(c.bytes==2&&x.Get(x.last_allocation,1)==0&&x.Get(x.last_allocation+2,1)==0&&x.Get(x.last_allocation+3,1)==0xCD,"allocator length reload and zero bounds");throw std::runtime_error("stop-after-zero");}return{};};bool stopped=false;try{(void)f.Run();}catch(const std::runtime_error& e){stopped=std::string(e.what())=="stop-after-zero";}Need(stopped&&f.allocated_sizes==std::vector<U32>{5}&&f.free_calls==0,"allocator/read prefix");++Cases;}
 {Fixture f;Record(f.input,"AAAA",3,9,7,8);f.on_alloc=[](auto& x){x.last_allocation=Frame+0x10;};f.on_read=[](auto& x,auto c)->std::optional<U32>{if(x.reads==2){Need(c.bytes==0&&x.Get(Frame+0x10)==0&&x.Get(Frame+0x14,1)==0,"zeroing alias reload/cached REP tail");throw std::runtime_error("stop-after-alias");}return{};};bool stopped=false;try{(void)f.Run();}catch(const std::runtime_error& e){stopped=std::string(e.what())=="stop-after-alias";}Need(stopped,"post-zero aliased length observation");++Cases;}
 {Fixture f;Record(f.input,"Name",3,0x12345678,0xAABBCCDD,0xDDEEFF00);f.on_read=[](auto& x,auto)->std::optional<U32>{if(x.reads==3){x.map=0xABCDEF01;x.Put(Frame+0x4C,x.map);}return{};};f.on_map=[](auto& x){x.Put(Frame+0x48,0);x.Put(Frame+0x14,0);x.Put(Frame+0x18,0);};Need(f.Run()&&f.Get(f.value)==3&&f.Get(f.value+4)==0xAABBCCDD&&f.Get(f.value+8)==0xDDEEFF00,"map id reload and register-cached type/words");++Cases;}
 {Fixture f;Record(f.input,"Name",0xE9,0x11111111,0x22334455,0x66778899);f.on_map=[](auto& x){x.value=Frame+0x14;};Need(f.Run()&&f.Get(Frame+0x14)==0xE9&&f.Get(Frame+0x18)==0x22334455&&f.Get(Frame+0x1C)==0x66778899,"ordered alias stores use cached fields");++Cases;}
 {Fixture f;Record(f.input,"Name",0xE9,0x11111111,0x22334455,0x66778899);f.on_map=[](auto& x){x.value=0x1000B074;x.expected_destroy=0xE9;};Need(f.Run()&&f.destroy_targets==std::vector<U32>{0xE9},"destructor target loaded after value stores");++Cases;}
 {Fixture f;Record(f.input,"Name",1,2,3,4);f.on_ctor=[](auto& x){x.Put(x.last_allocation,'Z',1);};Need(f.Run()&&f.key=="Zame","constructor precedes C-string assignment read");++Cases;}
 {Fixture f;Record(f.input,"A",0x48,2,3,4);f.stream=Frame+0x48;f.map=0xA004;f.Put(Frame+0x48,f.stream);f.Put(Frame+0x4C,f.map);f.Put(Frame+0x60,0xA018);f.Put(Frame+0x64,0xA01C);Need(f.Run()&&f.Get(f.value)==0x48,"type vtable cached before zeroing the incoming stream slot");++Cases;}
 {Fixture f;Record(f.input,"A",1,2,3,4);U32 successor=0;f.on_read=[&successor](auto& x,auto)->std::optional<U32>{if(x.reads==1){const auto old=x.services.read;x.services.read=[old,&successor](auto c){++successor;return old(c);};x.Put(Vtable+4,0xA005);}return{};};Need(f.Run()&&successor==5&&f.requests[1].target==0xA005,"read callable and target are reloaded per invocation");++Cases;}
 {Fixture f;Record(f.input,"A",1,2,3,4);Need(f.Run()&&(f.Get(Frame+0x48)&0xFFFFFF00u)==(f.stream&0xFFFFFF00u),"type initialization preserves upper incoming-slot bytes");++Cases;}
 {Fixture f;f.memory.bytes=std::span<U8>(f.data).first(Frame+0x4C-Base);Need(f.Run(),"empty input does not consume map argument");++Cases;}
 {Fixture f;Record(f.input,"A",1,2,3,4);f.on_position=[](auto& x){if(x.pos_calls==2)++x.position;};Need(!f.Run()&&f.reads==7,"position beyond saved length is not EOF equality");++Cases;}
 {Fixture f;Record(f.input,"AAAA",3,9,7,8);std::vector<U8> prior;f.on_alloc=[&prior](auto& x){prior=std::move(x.data);x.data=prior;x.memory.bytes=x.data;x.Put(Frame+0x10,2);};f.on_read=[](auto& x,auto c)->std::optional<U32>{if(x.reads==2){Need(c.bytes==2&&x.Get(x.last_allocation+3,1)==0xCD,"new memory view consumed after allocator boundary");throw std::runtime_error("stop-after-view");}return{};};bool stopped=false;try{(void)f.Run();}catch(const std::runtime_error& e){stopped=std::string(e.what())=="stop-after-view";}Need(stopped&&prior.size()==f.data.size(),"retained old view makes replacement observable without dangling memory");++Cases;}

}
void Faults(){++Groups;
 {Fixture f;Record(f.input,"A",1,2,3,4);f.value=Base+static_cast<U32>(f.data.size())-4;bool fault=false;try{(void)f.Run();}catch(const Monstey_FearDbFault& e){fault=e.address==Base+f.data.size()&&e.width==4&&e.writing;}Need(fault&&f.Get(f.value)==1&&f.dtor_calls==0,"partial value stores before consumed range fault");++Cases;}
 {Fixture f;Word(f.input,0xFFFFFFFF);f.on_alloc=[](auto& x){x.last_allocation=0;};f.on_read=[](auto& x,auto c)->std::optional<U32>{if(x.reads==2){Need(c.bytes==0xFFFFFFFF&&c.destination==0,"wrapped allocation request preserved");return 1;}return{};};Need(!f.Run()&&f.allocated_sizes==std::vector<U32>{0}&&f.free_calls==1,"zero allocation has no invented prevalidation or zero-store");++Cases;}
 {Fixture f;Record(f.input,"A",1,2,3,4);f.services.string_assign_cstr=[](auto,auto){throw std::runtime_error("assignment-failure");};bool stopped=false;try{(void)f.Run();}catch(const std::runtime_error& e){stopped=std::string(e.what())=="assignment-failure";}Need(stopped&&f.free_calls==0&&f.dtor_calls==0&&f.ctor_calls==1,"external exception prefix not fabricated as native cleanup");++Cases;}
}
void Actual(const std::filesystem::path& path){const auto before=Monstey_HashSha256(path);Need(before=="6744df3128a6fee36a0e5d1be13e90d417432b2d5512ac8f1755e32fd01709da","explicit original constraint profile");const auto bytes=Monstey_ReadFile(path);Equal(Monstey_FearDecodeConstraints(bytes),Expected(bytes));const auto table=Monstey_FearDecodeConstraints(bytes);Need(table.complete&&table.records==547&&table.rows.size()==547&&table.read_calls==3282&&table.consumed==37922,"original aggregate profile");Need(Monstey_HashSha256(path)==before,"original source remained unchanged");}
}
int wmain(int argc,wchar_t** argv){try{if(argc==3&&std::wstring(argv[1])==L"--actual"){Actual(argv[2]);std::cout<<"{\"status\":\"PASS_ACTUAL_ALL_ROWS\",\"records\":547,\"names_values_persisted\":false}\n";return 0;}if(argc!=1)throw std::invalid_argument("Unexpected owner test arguments");Format();Failure();Epochs();Faults();std::cout<<"{\"status\":\"PASS\",\"groups\":"<<Groups<<",\"cases\":"<<Cases<<",\"checks\":"<<Checks<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
