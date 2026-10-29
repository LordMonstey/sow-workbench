#include "Monstey_GadbRecordLookup.hpp"
#include "Monstey_Core.hpp"
#include "Monstey_Json.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
using namespace Monstey;
namespace {
std::uint64_t checks=0,cases=0;
void Need(bool ok,const char* why){++checks;Monstey_Require(ok,why);}
template<class T> void Put(std::vector<std::uint8_t>& b,std::size_t p,T v){std::memcpy(b.data()+p,&v,sizeof(v));}
struct Arena {std::vector<std::uint8_t>b=std::vector<std::uint8_t>(8192,0xa5);std::uint64_t base=0x100000000;Monstey_GadbBinderMemory Memory(){return{base,b};}};
void Exhaustive(){
 for(unsigned form=0;form<2;++form)for(unsigned count=0;count<=255;++count){
  Arena a;Put(a.b,0,static_cast<std::uint8_t>(count));Put(a.b,1,static_cast<std::uint8_t>(0xa0|form));Put(a.b,8,a.base+256);
  std::vector<std::uint32_t> keys;
  for(unsigned i=0;i<count;++i){auto key=static_cast<std::uint32_t>((static_cast<std::uint64_t>(i)*0xffffffffu)/std::max(1u,count-1));keys.push_back(key);Put(a.b,256+16*i+(form?0:4),key);Put(a.b,256+16*i+9,static_cast<std::uint8_t>(255-i));}
  const auto input=a.b;
  std::vector<std::uint32_t> queries=keys;queries.insert(queries.end(),{0u,1u,0x7fffffffu,0x80000000u,0xfffffffeu,0xffffffffu});
  for(auto q:queries){Put(a.b,64,q);auto it=std::find(keys.begin(),keys.end(),q);std::uint64_t expected=0;if(it!=keys.end()){auto i=static_cast<unsigned>(it-keys.begin());expected=a.base+256+16*(form?i:255-i);}auto got=Monstey_GadbFindRecordDescriptor(a.Memory(),a.base,a.base+64);Need(got==expected,"Exhaustive raw lookup result mismatch");++cases;}
  auto restored=a.b;std::copy(input.begin()+64,input.begin()+68,restored.begin()+64);Need(restored==input,"Lookup mutated borrowed arena");
 }
}
void Duplicates(){
 for(unsigned form=0;form<2;++form)for(unsigned count:{2u,3u,4u,5u,255u}){
  Arena a;Put(a.b,0,static_cast<std::uint8_t>(count));Put(a.b,1,static_cast<std::uint8_t>(form));Put(a.b,8,a.base+256);Put(a.b,64,0xf1234567u);
  for(unsigned i=0;i<count;++i){Put(a.b,256+16*i+(form?0:4),0xf1234567u);Put(a.b,256+16*i+9,static_cast<std::uint8_t>(i+1));}
  auto mid=(count-1)/2;auto expected=a.base+256+16*(form?mid:mid+1);Need(Monstey_GadbFindRecordDescriptor(a.Memory(),a.base,a.base+64)==expected,"Duplicate hash must retain first reached middle, not lower_bound");++cases;
 }
}
void Consumption(){
 for(unsigned form=0;form<2;++form){Arena a;Put(a.b,0,std::uint8_t{0});Put(a.b,1,static_cast<std::uint8_t>(form));Put(a.b,8,std::uint64_t{0xfffffffffffffff0});Need(Monstey_GadbFindRecordDescriptor(a.Memory(),a.base,0)==0,"Empty lookup must not consume field/table rows");++cases;}
 Arena a;Put(a.b,0,std::uint8_t{1});Put(a.b,1,std::uint8_t{0});Put(a.b,8,a.base+256);Put(a.b,260,42u);Put(a.b,265,std::uint8_t{255});Put(a.b,64,42u);
 auto small=Monstey_GadbBinderMemory{a.base,std::span(a.b).first(280)};Need(Monstey_GadbFindRecordDescriptor(small,a.base,a.base+64)==a.base+256+255*16,"Returned remap must not be eagerly dereferenced");++cases;
 // Same row can supply the field's hash and the lookup bytes.
 Put(a.b,1,std::uint8_t{1});Put(a.b,256,0x80000000u);Need(Monstey_GadbFindRecordDescriptor(a.Memory(),a.base,a.base+256)==a.base+256,"Borrowed field/table alias mismatch");++cases;
 const auto fault=[&](Monstey_GadbBinderMemory m,std::uint64_t rec,std::uint64_t field,std::uint64_t address){bool caught=false;try{(void)Monstey_GadbFindRecordDescriptor(m,rec,field);}catch(const Monstey_GadbBinderFault&e){caught=e.kind==Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable&&e.address==address;}Need(caught,"Consumed range fault/order mismatch");++cases;};
 fault({a.base,{}},a.base,a.base+64,a.base+1);
 fault({a.base,std::span(a.b).first(2)},a.base,a.base+64,a.base+8);
 Put(a.b,0,std::uint8_t{0});fault({a.base,std::span(a.b).first(8)},a.base,0,a.base+8);
 Put(a.b,0,std::uint8_t{1});Put(a.b,1,std::uint8_t{1});fault({a.base,std::span(a.b).first(128)},a.base,0,0);
 Put(a.b,1,std::uint8_t{0});fault({a.base,std::span(a.b).first(128)},a.base,0,a.base+260);
 // U64 address arithmetic at the native boundary wraps before range checking.
 Arena z;z.base=0;Put(z.b,0,std::uint8_t{0});fault(z.Memory(),std::numeric_limits<std::uint64_t>::max(),0,std::numeric_limits<std::uint64_t>::max());
}
}
int wmain(int argc,wchar_t**argv){try{Monstey_Require(argc==2,"Expected NEW_REPORT");Exhaustive();Duplicates();Consumption();auto r=Monstey_Json::object();r["status"]="PASS";r["groups"]=3;r["cases"]=static_cast<std::int64_t>(cases);r["checks"]=static_cast<std::int64_t>(checks);r["original_calls"]=false;Monstey_WriteNewText(argv[1],r.dump(2)+"\n");std::cout<<"PASS 3 groups "<<cases<<" cases "<<checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
