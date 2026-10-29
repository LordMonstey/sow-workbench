#include "Monstey_FearConstraintReference.hpp"
#include "Monstey_FearConstraintTable.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>
using namespace Monstey;
using namespace Monstey_Independent;
namespace {
using Monstey_U32=std::uint32_t;using Monstey_U64=std::uint64_t;
constexpr Monstey_U32 Monstey_Base=0x10000000u,Monstey_Frame=Monstey_Base+0x100,
 Monstey_Stream=Monstey_Base+0x200,Monstey_Table=Monstey_Base+0x300,Monstey_Value=Monstey_Base+0x500;
enum class Monstey_Mode {
 Plain,AllocShort,AllocAlias,AllocRebase,LengthReplace,HeaderVtableChange,AssignImportChange,
 DefaultImportChange,MapCacheMutation,DestroyTargetAlias,MapCut2,MapCut6,MapCut10,
 TailCutHigh,TailCutSkipHigh,InitialPositionChangesCache,ReadVtableAliasesZero,
 CachedVtableBeforeLength,WrapName,PositionBeyondLength
};
struct Monstey_Row {std::vector<std::uint8_t> key;Monstey_U32 type,ignored,low,high;};
void Monstey_Append(std::vector<std::uint8_t>& b,Monstey_U32 x){for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(x>>(8*i)));}
std::vector<std::uint8_t> Monstey_Serialize(const std::vector<Monstey_Row>& rows){
 std::vector<std::uint8_t> b;
 for(const auto& r:rows){Monstey_Append(b,static_cast<Monstey_U32>(r.key.size()));b.insert(b.end(),r.key.begin(),r.key.end());b.push_back(static_cast<std::uint8_t>(r.type));Monstey_Append(b,r.ignored);Monstey_Append(b,r.low);Monstey_Append(b,r.high);}
 return b;
}
struct Monstey_Event {
 std::string kind;Monstey_U64 a=0,b=0,c=0,d=0;std::string text;
 bool operator==(const Monstey_Event&)const=default;
};
struct Monstey_Config {
 Monstey_Mode mode=Monstey_Mode::Plain;int fail_read=-1;int missing=-1;
 Monstey_U32 length_high=0,position_high=0;bool empty=false;
 std::size_t initial_cut=0xC000;
};
struct Monstey_Fixture {
 Monstey_Config cfg;std::vector<std::uint8_t> input;std::vector<std::vector<std::uint8_t>> images;
 std::size_t active=0;Monstey_FearDbMutableMemory view;
 std::vector<Monstey_Event> events;std::vector<std::string> keys;
 Monstey_U64 position=0;unsigned reads=0,positions=0,allocations=0,maps=0;bool array_live=false,string_live=false;std::string current_key;
 Monstey_Fixture(Monstey_Config c,std::vector<std::uint8_t> in):cfg(c),input(std::move(in)),images(1,std::vector<std::uint8_t>(0xC000,0xA5)){
  view={Monstey_Base,images[0]};Monstey_Memory m(view);
  m.Write(Monstey_Frame+0x48,Monstey_Stream);m.Write(Monstey_Frame+0x4C,Monstey_Base+0x400);
  m.Write(Monstey_Stream,Monstey_Table);m.Write(Monstey_Table+0x1C,0x1111);m.Write(Monstey_Table+0x18,0x2222);m.Write(Monstey_Table+4,0x3333);
  m.Write(0x1000B064u,0xD064);m.Write(0x1000B068u,0xD068);m.Write(0x1000B074u,0xD074);
  if(cfg.mode==Monstey_Mode::ReadVtableAliasesZero)m.Write(Monstey_Stream,Monstey_Frame+0xC);
  if(cfg.mode==Monstey_Mode::CachedVtableBeforeLength){m.Write(Monstey_Frame+0x48,Monstey_Frame+0x20);m.Write(Monstey_Frame+0x20,Monstey_Table);}
  if(cfg.mode==Monstey_Mode::WrapName){input.clear();Monstey_Append(input,0xFFFFFFFF);}
  if(cfg.empty)input.clear();
  view.bytes=std::span(images[0]).first(cfg.initial_cut);
 }
 void Replace(std::size_t count,bool copy=true){
  if(copy){images.push_back(images[active]);active=images.size()-1;}
  view={Monstey_Base,std::span(images[active]).first(count)};
 }
 Monstey_FearConstraintServices Services(){
  Monstey_FearConstraintServices s;
  s.get_length=[this](auto call){
   const auto n=(Monstey_U64(cfg.length_high)<<32)|input.size();
   events.push_back({"length",call.target,call.object,n});
   if(cfg.mode==Monstey_Mode::LengthReplace){Replace(0xC000);Monstey_Memory(view).Write(Monstey_Base+0x40,0x42424242);}
   return n;
  };
  s.get_position=[this](auto call){
   ++positions;auto p=(Monstey_U64(cfg.position_high)<<32)|position;
   if(cfg.mode==Monstey_Mode::PositionBeyondLength&&positions==1)p=input.size()+1;
   if(cfg.mode==Monstey_Mode::InitialPositionChangesCache&&positions==1){p=(Monstey_U64(cfg.length_high)<<32)|input.size();Monstey_Memory(view).Write(Monstey_Frame+0x20,0xBAD);}
   events.push_back({"position",call.target,call.object,p});
   if(positions>1&&(cfg.mode==Monstey_Mode::TailCutHigh||cfg.mode==Monstey_Mode::TailCutSkipHigh)){
    if(cfg.mode==Monstey_Mode::TailCutSkipHigh)Monstey_Memory(view).Write(Monstey_Frame+0x20,static_cast<Monstey_U32>(p)+1);
    Replace(0x124);
   }
   return p;
  };
  s.read=[this](auto call){
   ++reads;events.push_back({"read",call.target,call.stream,call.destination,call.bytes});
   Monstey_Memory m(view);
   if(cfg.mode==Monstey_Mode::WrapName&&reads==2)return Monstey_U32{0x80000000};
   if(static_cast<int>(reads)==cfg.fail_read){if(call.bytes)m.Write(call.destination,0xE1,1);return Monstey_U32{0x80000000};}
   if(call.bytes>input.size()-static_cast<std::size_t>(position)){
    for(Monstey_U32 i=0;i<call.bytes;++i)m.Write(call.destination+i,0,1);return Monstey_U32{1};
   }
   for(Monstey_U32 i=0;i<call.bytes;++i)m.Write(call.destination+i,input[static_cast<std::size_t>(position)+i],1);
   position+=call.bytes;
   if(cfg.mode==Monstey_Mode::HeaderVtableChange&&reads==1){m.Write(Monstey_Stream,Monstey_Base+0x700);m.Write(Monstey_Base+0x704,0xABCD);m.Write(Monstey_Base+0x718,0xBCDE);}
   return Monstey_U32{0};
  };
  s.new_array=[this](auto n){
   ++allocations;array_live=true;events.push_back({"new",n});Monstey_Memory m(view);
   if(cfg.mode==Monstey_Mode::AllocShort)m.Write(Monstey_Frame+0x10,1);
   if(cfg.mode==Monstey_Mode::AllocRebase)Replace(0xC000);
   return cfg.mode==Monstey_Mode::AllocAlias?Monstey_Frame+0x10:Monstey_Base+0x1000;
  };
  s.delete_array=[this](auto p){events.push_back({"delete",p});array_live=false;};
  s.string_default=[this](auto call){
   events.push_back({"string-default",call.target,call.object});string_live=true;current_key.clear();
   if(cfg.mode==Monstey_Mode::DefaultImportChange)Monstey_Memory(view).Write(0x1000B068u,0xAA68);
  };
  s.string_assign_cstr=[this](auto call,auto p){
   events.push_back({"string-assign",call.target,call.object,p});current_key.clear();Monstey_Memory m(view);
   for(;;++p){const auto c=m.Read(p,1);if(!c)break;current_key.push_back(static_cast<char>(c));}
   if(cfg.mode==Monstey_Mode::AssignImportChange)m.Write(0x1000B074u,0xAA74);
  };
  s.string_destroy=[this](auto call){events.push_back({"string-destroy",call.target,call.object});string_live=false;};
  s.map_value=[this](auto map,auto name){
   ++maps;events.push_back({"map",map,name,0,0,current_key});keys.push_back(current_key);
   if(cfg.mode==Monstey_Mode::MapCacheMutation){Monstey_Memory m(view);m.Write(Monstey_Frame+0x48,0xFF,1);m.Write(Monstey_Frame+0x14,0x123);m.Write(Monstey_Frame+0x18,0x456);}
   const auto value=cfg.mode==Monstey_Mode::DestroyTargetAlias?0x1000B06Cu:Monstey_Value+12u*(maps-1);
   if(cfg.mode==Monstey_Mode::MapCut2)Replace(0x502);
   if(cfg.mode==Monstey_Mode::MapCut6)Replace(0x506);
   if(cfg.mode==Monstey_Mode::MapCut10)Replace(0x50A);
   return value;
  };
  switch(cfg.missing){case 0:s.get_length={};break;case 1:s.get_position={};break;case 2:s.read={};break;case 3:s.new_array={};break;case 4:s.delete_array={};break;case 5:s.string_default={};break;case 6:s.string_assign_cstr={};break;case 7:s.string_destroy={};break;case 8:s.map_value={};break;default:break;}
  return s;
 }
};
struct Monstey_Outcome{unsigned kind=0;bool result=false;Monstey_U32 address=0,width=0;bool writing=false;};
struct Monstey_Stats{Monstey_U64 groups=0,cases=0,checks=0,faults=0;};
Monstey_Stats Monstey_Counts;
void Monstey_Check(bool ok,const char* why){++Monstey_Counts.checks;if(!ok)throw std::runtime_error(why);}
std::vector<Monstey_Row> Monstey_Rows(){
 return {{{'n','a','m','e'},7,0xDEADBEEF,0x80000000,0x3F800000},{{'x',0,'y'},0xFF,0x12345678,0x76543210,0xFEDCBA98}};
}
void Monstey_Compare(const Monstey_Config& cfg,const std::vector<std::uint8_t>& input){
 ++Monstey_Counts.cases;Monstey_Fixture e(cfg,input),a(cfg,input);auto es=e.Services(),as=a.Services();Monstey_Outcome expected,actual;
 try{expected.result=Monstey_ReferenceConstraints(e.view,Monstey_Frame,es);}
 catch(const Monstey_RefFault& f){expected={1,false,f.address,f.width,f.writing};}
 catch(const Monstey_RefBoundary&){expected.kind=2;}
 try{actual.result=Monstey_FearReadConstraints(a.view,Monstey_Frame,as);}
 catch(const Monstey_FearDbFault& f){actual={1,false,f.address,f.width,f.writing};}
 catch(const Monstey_FearConstraintBoundaryFault&){actual.kind=2;}
 Monstey_Check(expected.kind==actual.kind&&expected.result==actual.result&&expected.address==actual.address&&expected.width==actual.width&&expected.writing==actual.writing,"constraint result/first-fault mismatch");
 Monstey_Check(e.events==a.events,"service target/order/consumption mismatch");
 Monstey_Check(e.images==a.images&&e.active==a.active&&e.view.base==a.view.base&&e.view.bytes.size()==a.view.bytes.size(),"whole memory/replaced-view/failure prefix mismatch");
 Monstey_Check(e.position==a.position&&e.reads==a.reads&&e.maps==a.maps&&e.keys==a.keys&&e.array_live==a.array_live&&e.string_live==a.string_live,"stream/lifetime/logical prefix mismatch");
 if(expected.kind)++Monstey_Counts.faults;
}
void Monstey_ProtocolCases(){
 ++Monstey_Counts.groups;const auto input=Monstey_Serialize(Monstey_Rows());
 for(unsigned mode=0;mode<=static_cast<unsigned>(Monstey_Mode::PositionBeyondLength);++mode){Monstey_Config c;c.mode=static_cast<Monstey_Mode>(mode);Monstey_Compare(c,input);}
 for(int read=1;read<=14;++read){Monstey_Config c;c.fail_read=read;Monstey_Compare(c,input);}
 for(int missing=0;missing<9;++missing){Monstey_Config c;c.missing=missing;Monstey_Compare(c,input);}
 for(auto highs:std::array<std::array<Monstey_U32,2>,4>{{{0,0},{1,1},{1,0},{0,1}}}){Monstey_Config c;c.length_high=highs[0];c.position_high=highs[1];Monstey_Compare(c,input);}
 for(int missing=2;missing<9;++missing){Monstey_Config c;c.empty=true;c.missing=missing;Monstey_Compare(c,input);}
 {Monstey_Config c;c.mode=Monstey_Mode::AllocAlias;Monstey_Compare(c,Monstey_Serialize({{{},3,0xDEAD,0x1234,0x5678}}));}
}
void Monstey_FaultCuts(){
 ++Monstey_Counts.groups;const auto input=Monstey_Serialize(Monstey_Rows());
 for(std::size_t cut=0;cut<=0x600;++cut){Monstey_Config c;c.initial_cut=cut;Monstey_Compare(c,input);}
 for(std::size_t cut=0xFFEu;cut<=0x1008u;++cut){Monstey_Config c;c.initial_cut=cut;Monstey_Compare(c,input);}
 for(std::size_t cut=0xB060;cut<=0xB078;++cut){Monstey_Config c;c.initial_cut=cut;Monstey_Compare(c,input);}
}
void Monstey_WidthsAndTypes(){
 ++Monstey_Counts.groups;
 for(unsigned type=0;type<256;++type){auto r=Monstey_Rows();r.resize(1);r[0].type=type;Monstey_Compare({},Monstey_Serialize(r));}
 for(unsigned n=0;n<=32;++n){auto r=Monstey_Rows();r.resize(1);r[0].key.assign(n,'z');Monstey_Compare({},Monstey_Serialize(r));}
}
void Monstey_ConcreteTable(){
 ++Monstey_Counts.groups;
 std::mt19937 random(0x60020);
 for(unsigned trial=0;trial<128;++trial){
  std::vector<Monstey_Row> rows;
  for(unsigned i=0;i<trial%12;++i){Monstey_Row r{{},static_cast<Monstey_U32>(random()%256),static_cast<Monstey_U32>(random()),static_cast<Monstey_U32>(random()),static_cast<Monstey_U32>(random())};for(unsigned j=0;j<i%5;++j)r.key.push_back(static_cast<std::uint8_t>((i+j)%4==0?0:128+random()%128));rows.push_back(r);}
  auto bytes=Monstey_Serialize(rows);const auto got=Monstey_FearDecodeConstraints(bytes);
  std::map<std::vector<std::uint8_t>,Monstey_Row> expected;
  for(auto r:rows){const auto n=std::find(r.key.begin(),r.key.end(),0);r.key.erase(n,r.key.end());expected[r.key]=r;}
  ++Monstey_Counts.cases;Monstey_Check(got.complete&&got.consumed==bytes.size()&&got.records==rows.size()&&got.read_calls==rows.size()*6,"concrete span counts/consumption mismatch");
  Monstey_Check(got.rows.size()==expected.size(),"logical table duplicate policy mismatch");
  std::size_t i=0;for(const auto& [key,r]:expected){const auto& v=got.rows[i++];const std::vector<std::uint8_t> k(v.key.begin(),v.key.end());Monstey_Check(k==key&&v.type==r.type&&v.word4==r.low&&v.word8==r.high,"logical unsigned-key ordering/raw field mismatch");}
 }
}
}
int main(int argc,char**argv){
 try{
  if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("New report path required");
  Monstey_ProtocolCases();Monstey_FaultCuts();Monstey_WidthsAndTypes();Monstey_ConcreteTable();
  std::ofstream out(argv[1],std::ios::binary);if(!out)throw std::runtime_error("Report path unavailable");
  out<<"{\"status\":\"PASS\",\"groups\":"<<Monstey_Counts.groups<<",\"cases\":"<<Monstey_Counts.cases<<",\"checks\":"<<Monstey_Counts.checks<<",\"fault_cases\":"<<Monstey_Counts.faults<<"}\n";
  std::cout<<"PASS "<<Monstey_Counts.cases<<" cases "<<Monstey_Counts.checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
 catch(const Monstey_RefFault&){std::cerr<<"Unexpected independent fixture range\n";return 1;}
}
