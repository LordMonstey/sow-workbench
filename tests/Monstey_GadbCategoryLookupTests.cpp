#include "Monstey_GadbCategoryLookup.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>
namespace {
using Monstey_U64=std::uint64_t;using Monstey_U32=std::uint32_t;
unsigned Monstey_cases=0,Monstey_checks=0,Monstey_faults=0;
void Monstey_Check(bool value,const char* text){++Monstey_checks;if(!value)throw std::runtime_error(text);}
struct Monstey_ReferenceFault{Monstey_U64 at;unsigned width;};
struct Monstey_Fixture{
 std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x10000,0xA7);
 Monstey_U64 base=0x10000000,object=base+0x100,key=base+0x300,array=base+0x1000;
 Monstey::Monstey_GadbBinderMemory View(){return {base,bytes};}
 void Put(Monstey_U64 a,Monstey_U64 v,unsigned n){if(a<base||a-base+n>bytes.size())throw std::runtime_error("fixture range");for(unsigned i=0;i<n;++i)bytes[static_cast<std::size_t>(a-base)+i]=static_cast<std::uint8_t>(v>>(i*8u));}
 void Setup(const std::vector<Monstey_U32>& hashes,Monstey_U32 requested){Put(object+0x48,hashes.size(),4);Put(object+0x50,array,8);Put(key,requested,4);for(std::size_t i=0;i<hashes.size();++i)Put(array+i*88u+0x50,hashes[i],4);}
};
Monstey_U64 Monstey_Load(const Monstey_Fixture& f,Monstey_U64 a,unsigned n){if(a<f.base||a>std::numeric_limits<Monstey_U64>::max()-(n-1)||a-f.base>f.bytes.size()||n>f.bytes.size()-static_cast<std::size_t>(a-f.base))throw Monstey_ReferenceFault{a,n};Monstey_U64 value=0;for(unsigned i=0;i<n;++i)value+=Monstey_U64(f.bytes[static_cast<std::size_t>(a-f.base)+i])*(Monstey_U64(1)<<(i*8u));return value;}
std::int64_t Monstey_Signed32(Monstey_U32 v){return v<0x80000000u?std::int64_t(v):std::int64_t(v)-0x100000000ll;}
Monstey::Monstey_GadbBufferedResult Monstey_Reference(const Monstey_Fixture& f){
 Monstey_U32 high=static_cast<Monstey_U32>(Monstey_Load(f,f.object+72,4))-1u;
 const auto table=Monstey_Load(f,f.object+80,8);Monstey_U32 low=0;Monstey_U64 d=f.key;
 if(Monstey_Signed32(high)<0)return {0,d};
 for(unsigned fixtureBudget=0;fixtureBudget<128;++fixtureBudget){
  const auto signedSum=Monstey_Signed32(static_cast<Monstey_U32>(low+high));
  const auto mid=signedSum<0?-((-signedSum)/2):signedSum/2;
  d=static_cast<Monstey_U64>(mid*88);
  const auto candidate=Monstey_Load(f,table+d+80,4);
  const auto wanted=Monstey_Load(f,f.key,4);
  if(wanted==candidate)return {table+d,d};
  if(wanted<candidate)high=static_cast<Monstey_U32>(mid-1);else low=static_cast<Monstey_U32>(mid+1);
  if(Monstey_Signed32(low)>Monstey_Signed32(high))return {0,d};
 }
 throw std::runtime_error("independent test reference budget");
}
Monstey::Monstey_GadbBufferedResult Monstey_Differential(Monstey_Fixture& f){
 const auto before=f.bytes;Monstey::Monstey_GadbBufferedResult expected{};bool referenceFault=false;Monstey_ReferenceFault fault{};
 try{expected=Monstey_Reference(f);}catch(const Monstey_ReferenceFault& e){referenceFault=true;fault=e;}
 Monstey::Monstey_GadbBufferedResult actual{};bool actualFault=false;
 try{actual=Monstey::Monstey_GadbFindCategoryByHash(f.View(),f.object,f.key);}catch(const Monstey::Monstey_GadbCategoryLookupFault& e){actualFault=true;Monstey_Check(referenceFault&&e.address==fault.at&&e.width==fault.width,"exact first consumed fault");++Monstey_faults;}
 Monstey_Check(actualFault==referenceFault,"same native reference fault status");
 if(!actualFault)Monstey_Check(actual.rax==expected.rax&&actual.rdx==expected.rdx,"raw RAX and RDX equal independent reference");
 Monstey_Check(f.bytes==before,"lookup leaves complete owned image unchanged");++Monstey_cases;return actual;
}
void Monstey_SortedAndAbsent(){
 for(unsigned n=0;n<=64;++n)for(unsigned key=0;key<=260;key+=2){Monstey_Fixture f;std::vector<Monstey_U32> values;for(unsigned i=0;i<n;++i)values.push_back(i*4+1);f.Setup(values,key);Monstey_Differential(f);}
 for(unsigned n=1;n<=64;++n){Monstey_Fixture f;std::vector<Monstey_U32> values;for(unsigned i=0;i<n;++i)values.push_back(i*4+1);f.Setup(values,(n/2)*4+1);const auto r=Monstey_Differential(f);Monstey_Check(r.rax==f.array+(n/2)*88u,"known exact unique matching row");}
}
void Monstey_DuplicatesUnsortedAliases(){
 for(unsigned n=1;n<=31;++n){Monstey_Fixture f;f.Setup(std::vector<Monstey_U32>(n,77),77);const auto r=Monstey_Differential(f);Monstey_Check(r.rax==f.array+((n-1u)/2u)*88u,"duplicates return first native midpoint not lower bound");}
 {Monstey_Fixture f;f.Setup({30,10,20},30);const auto r=Monstey_Differential(f);Monstey_Check(r.rax==0&&r.rdx==176,"unsorted input is searched without schema rejection");}
 {Monstey_Fixture f;f.Setup({1,3,5,7,9},0);f.key=f.array+2*88+0x50;const auto r=Monstey_Differential(f);Monstey_Check(r.rax==f.array+176,"key aliases row hash");}
 {Monstey_Fixture f;++f.object;f.key+=3;f.array+=1;f.Setup({2,4,6},4);Monstey_Differential(f);}
 {Monstey_Fixture f;f.base=0;f.object=0x100;f.key=0x300;f.array=0;f.Setup({9},9);const auto r=Monstey_Differential(f);Monstey_Check(r.rax==0&&r.rdx==0,"matching numeric row zero not rejected");}
 for(Monstey_U32 k:{0u,1u,0x7FFFFFFFu,0x80000000u,0xFFFFFFFEu,0xFFFFFFFFu}){Monstey_Fixture f;f.Setup({0,0x7FFFFFFFu,0x80000000u,0xFFFFFFFFu},k);Monstey_Differential(f);}
}
void Monstey_SignedCounts(){
 for(Monstey_U32 n:{0u,0x80000001u,0xFFFFFFFEu,0xFFFFFFFFu}){Monstey_Fixture f;f.Put(f.object+0x48,n,4);f.Put(f.object+0x50,0xFFFFFFFFFFFFFFF0ull,8);f.key=0xFEDCBA9876543210ull;const auto r=Monstey_Differential(f);Monstey_Check(r.rax==0&&r.rdx==f.key,"initial SF skip retains entire incoming RDX and lazy key");}
 for(Monstey_U32 n:{0x7FFFFFFFu,0x80000000u}){Monstey_Fixture f;const Monstey_U64 offset=0x3FFFFFFFull*88;f.array=f.base+0x1000-offset-0x50;f.Put(f.object+0x48,n,4);f.Put(f.object+0x50,f.array,8);f.Put(f.base+0x1000,7,4);f.Put(f.key,7,4);const auto r=Monstey_Differential(f);Monstey_Check(r.rax==f.base+0xFB0&&r.rdx==offset,"huge count match with bounded span and U64 wrapped table identity");}
 {Monstey_Fixture f;const Monstey_U64 firstOffset=0x3FFFFFFFull*88;f.array=f.base+0x1000-firstOffset-0x50;f.Put(f.object+0x48,0x80000000u,4);f.Put(f.object+0x50,f.array,8);f.Put(f.base+0x1000,1,4);f.Put(f.key,2,4);const auto secondOffset=static_cast<Monstey_U64>(-std::int64_t(0x20000000))*88;const auto expected=f.array+secondOffset+0x50;bool caught=false;try{Monstey::Monstey_GadbFindCategoryByHash(f.View(),f.object,f.key);}catch(const Monstey::Monstey_GadbCategoryLookupFault& e){caught=true;Monstey_Check(e.address==expected&&e.width==4,"overflow midpoint divides signed odd sum toward zero before IMUL");}Monstey_Check(caught,"negative wrapped midpoint consumes explicit fault address");Monstey_Differential(f);}
 {Monstey_Fixture f;f.base=0;f.object=std::numeric_limits<Monstey_U64>::max()-0x47;f.key=0xFEDCBA9876543210ull;f.Put(0,0,4);f.Put(8,0xAABB,8);const auto r=Monstey_Differential(f);Monstey_Check(r.rdx==f.key,"only consumed object fields checked after U64 address wrapping");}
}
void Monstey_FaultOrder(){
 for(unsigned n=0;n<0x58;++n){Monstey_Fixture f;f.Setup({},0);f.bytes.resize(0x100+n);Monstey_Differential(f);}
 {Monstey_Fixture f;f.Setup({1},1);f.array=0xFFFFFFFFFFFFFF00ull;f.Put(f.object+0x50,f.array,8);f.key=0xEEEEEEEEEEEEEEEEull;bool caught=false;try{Monstey::Monstey_GadbFindCategoryByHash(f.View(),f.object,f.key);}catch(const Monstey::Monstey_GadbCategoryLookupFault& e){caught=true;Monstey_Check(e.address==f.array+0x50,"hash row consumed before invalid key");}Monstey_Check(caught,"both invalid must fault");Monstey_Differential(f);}
 {Monstey_Fixture f;f.Setup({1},1);f.key=0xEEEEEEEEEEEEEEEEull;Monstey_Differential(f);}
 {Monstey_Fixture f;f.Setup({1},1);f.key=f.base+f.bytes.size()-2;Monstey_Differential(f);}
 {Monstey_Fixture f;f.Setup({1},1);f.array=std::numeric_limits<Monstey_U64>::max()-0x51;f.Put(f.object+0x50,f.array,8);Monstey_Differential(f);}
 {Monstey_Fixture f;f.Setup({},0);f.bytes.resize(0x154);Monstey_Differential(f);}
}
}
int main(){try{Monstey_SortedAndAbsent();Monstey_DuplicatesUnsortedAliases();Monstey_SignedCounts();Monstey_FaultOrder();std::cout<<"{\"status\":\"PASS\",\"groups\":4,\"cases\":"<<Monstey_cases<<",\"checks\":"<<Monstey_checks<<",\"faults\":"<<Monstey_faults<<",\"native_original_entrypoint_invoked\":false,\"original_image_loaded\":false,\"original_instruction_bytes_interpreted\":false}\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
