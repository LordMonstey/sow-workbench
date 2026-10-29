#include "Monstey_GadbStringPointer.hpp"
#include "Monstey_GadbScalarFloat.hpp"
#include "Monstey_Core.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <random>
using namespace Monstey;
namespace {
using U64=std::uint64_t;using U32=std::uint32_t;
U64 cases=0,checks=0,groups=0;
void Check(bool v,const char* m){++checks;Monstey_Require(v,m);}
struct RefFault{int kind;U64 at;};
struct Oracle {
 U64 base;std::span<const std::uint8_t> bytes;
 U64 Read(U64 a,std::size_t n) const {
  if(bytes.size()<n||a<base||a-base>bytes.size()-n)throw RefFault{1,a};
  U64 v=0;for(std::size_t i=0;i<n;++i)v|=U64(bytes[static_cast<std::size_t>(a-base)+i])<<(8*i);return v;
 }
 U64 Search(U64 table,U64 field,int low,int high,bool direct,U32 sampled=0) const {
  if(high<low)return 0;
  const auto mid=low+(high-low)/2;
  const auto row=table+16ull*static_cast<unsigned>(mid);
  const auto candidate=static_cast<U32>(Read(row+(direct?0:4),4));
  const auto wanted=direct?sampled:static_cast<U32>(Read(field,4));
  if(wanted==candidate)return direct?row:table+16ull*Read(row+9,1);
  return wanted<candidate?Search(table,field,low,mid-1,direct,sampled):Search(table,field,mid+1,high,direct,sampled);
 }
 U64 Lookup(U64 record,U64 field) const {
  const auto flag=Read(record+1,1),count=Read(record,1),table=Read(record+8,8);
  if(!count)return 0;
  const bool direct=(flag&1)!=0;
  const auto hash=direct?static_cast<U32>(Read(field,4)):0;
  return Search(table,field,0,static_cast<int>(count)-1,direct,hash);
 }
 U64 Generated(U64 record,U64 field,U32 index,U64 fallback) const {
  if(!record)return fallback;
  const auto owner=Read(record+24,8);if(!owner)return fallback;
  const auto klass=Read(owner+32,8);if(!klass)return fallback;
  const auto wanted=static_cast<U32>(Read(field,4));
  const auto n=static_cast<U32>(Read(klass+8,4));
  if(!n)return fallback;
  const auto table=Read(klass,8);
  U64 match=0;for(U32 i=0;i<n;++i){const auto f=table+72ull*i;if(Read(f+32,4)==wanted){match=f;break;}}
  if(!match)return fallback;
  const auto m=static_cast<U32>(Read(klass+8,4));if(!m)return fallback;
  const auto t=Read(klass,8);U32 offset=0;
  static constexpr std::array<U32,18> sizes{0,5,8,8,8,8,8,8,12,16,8,8,8,8,8,8,8,8};
  bool offsetFound=false;
  for(U32 i=0;i<m;++i){
   const auto f=t+72ull*i;if(Read(f+32,4)==wanted){offsetFound=true;break;}
   const auto flags=Read(f+44,4),count=Read(f+40,4),type=Read(f+36,4);
   const U32 size=(flags&0x280)?0:count!=1?16:type<sizes.size()?sizes[static_cast<std::size_t>(type)]:0;
   offset=(offset+7u+size)&~U32(7);
  }
  if(!offsetFound)offset=UINT32_MAX;
  const auto type=static_cast<U32>(Read(match+36,4));
  const auto storage=Read(record+16,8)+offset;
  if(type==8||type==9)index=static_cast<U32>(index*2u);
  if(Read(match+40,4)!=UINT32_MAX)return Read(storage+8ull*index,8);
  if(index>=Read(storage+8,4))throw RefFault{2,storage+8};
  return Read(Read(storage,8)+8ull*index,8);
 }
 U64 String(U64 record,U64 field,U32 index,U64 fallback) const {
  if(!record)return fallback;
  const auto d=Lookup(record,field);if(!d)return fallback;
  if((Read(d+10,1)&63)!=4)return fallback;
  if(index>=Read(d+12,2))return fallback;
  if(Read(record+1,1)&16)return Generated(record,field,index,fallback);
  const auto first=Read(d+14,2),values=Read(record+16,8);
  const auto element=static_cast<U32>(static_cast<U32>(first)+index);
  return Read(values+8ull*element,8);
 }
};
struct Result{int kind=0;U64 value=0;bool operator==(const Result&)const=default;};
template<class F>Result Reference(F f){try{return{0,f()};}catch(const RefFault& e){return{e.kind,e.at};}}
template<class F>Result Actual(F f){try{return{0,f()};}catch(const Monstey_GadbBinderFault& e){Check(e.kind==Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable,"Unexpected binder fault kind");return{1,e.address};}catch(const Monstey_GadbScalarFault& e){Check(e.kind==Monstey_GadbScalarFaultKind::NativeDynamicIndexAssertion,"Unexpected scalar fault kind");return{2,e.address};}}
struct Arena{
 U64 base=0x400000000ull;std::vector<std::uint8_t>b=std::vector<std::uint8_t>(8192,0xa5);
 U64 A(std::size_t p)const{return base+p;}
 void Put(std::size_t p,U64 v,std::size_t n){for(std::size_t i=0;i<n;++i)b.at(p+i)=static_cast<std::uint8_t>(v>>(8*i));}
 Monstey_GadbBinderMemory M(std::size_t size){return{base,std::span(b).first(size)};}
 Oracle O(std::size_t size)const{return{base,std::span(b).first(size)};}
 void Seed(){Put(32,1,1);Put(33,1,1);Put(40,A(512),8);Put(48,A(5120),8);Put(56,A(192),8);Put(128,0xf1234567,4);Put(512,0xf1234567,4);Put(522,4,1);Put(524,2,2);Put(526,0,2);Put(5120,0x123456789abcdef0ull,8);Put(5128,0xfff8000000001234ull,8);Put(224,A(256),8);Put(256,A(4096),8);Put(264,2,4);Put(4128,1,4);Put(4132,1,4);Put(4136,1,4);Put(4140,0,4);Put(4200,0xf1234567,4);Put(4204,4,4);Put(4208,2,4);Put(4212,0,4);}
};
void Compare(Arena& a,int operation,U64 record,U64 field,U32 index=0,U64 fallback=0,std::size_t size=8192){
 auto before=a.b;const auto m=a.M(size);const auto o=a.O(size);
 const auto expected=Reference([&]{return operation==0?o.Lookup(record,field):operation==1?o.String(record,field,index,fallback):o.Generated(record,field,index,fallback);});
 const auto got=Actual([&]{return operation==0?Monstey_GadbFindRecordDescriptor(m,record,field):operation==1?Monstey_GadbReadStringPointer(m,record,field,index,fallback):Monstey_GadbReadGeneratedStringPointer(m,record,field,index,fallback);});
 Check(got==expected,"Independent native lookup/string result or first fault differs");Check(before==a.b,"Read-only getter mutated borrowed input/default bytes");++cases;
}
void LookupRandom(){std::mt19937_64 gen(0x25180c251bd0ull);
 for(unsigned sample=0;sample<8192;++sample){Arena a;const auto count=static_cast<unsigned>(gen()%256),form=sample&1u;a.Put(32,count,1);a.Put(33,0xe0|form,1);a.Put(40,a.A(512),8);std::vector<U32> keys;
  for(unsigned i=0;i<count;++i)keys.push_back(static_cast<U32>(gen()));std::sort(keys.begin(),keys.end());
  if(count>2&&sample%3==0)keys[count/2]=keys[count/2-1];
  for(unsigned i=0;i<count;++i){a.Put(512+16*i+(form?0:4),keys[i],4);a.Put(512+16*i+9,gen()%256,1);}
  const auto wanted=count&&sample%2?keys[gen()%count]:static_cast<U32>(gen());a.Put(128,wanted,4);
  Compare(a,0,a.A(32),a.A(128));if(count)Compare(a,0,a.A(32),a.A(512+16*(count/2)+(form?0:4)));
 }++groups;
}
void FaultAndAlias(){
 for(unsigned form=0;form<2;++form)for(unsigned count:{0u,1u,2u,255u})for(std::size_t size=0;size<600;++size){Arena a;a.Seed();a.Put(32,count,1);a.Put(33,form,1);Compare(a,0,a.A(32),a.A(128),0,0,size);}++groups;
 for(unsigned form=0;form<2;++form){Arena a;a.Seed();a.Put(33,form,1);a.Put(40,0xfffffffffffffff0ull,8);Compare(a,0,a.A(32),0xabadbeefull);a.Put(32,0,1);Compare(a,0,a.A(32),0xabadbeefull);}
 Arena z;z.base=0;z.Seed();for(U64 rec:{0ull,UINT64_MAX,UINT64_MAX-1,UINT64_MAX-7,UINT64_MAX-8})Compare(z,0,rec,0xfffffffffffffff0ull);++groups;
}
void RawStrings(){
 for(unsigned type=0;type<256;++type)for(U32 index:{0u,1u,2u,65534u,65535u,65536u,0x80000000u,UINT32_MAX}){Arena a;a.Seed();a.Put(522,type,1);Compare(a,1,a.A(32),a.A(128),index,0x8000000000000001ull);}++groups;
 for(unsigned count:{0u,1u,2u,127u,128u,255u,256u,65534u,65535u})for(U32 index:{0u,1u,2u,65534u,65535u,65536u,0x80000000u,UINT32_MAX})for(unsigned first:{0u,1u,65535u}){Arena a;a.Seed();a.Put(524,count,2);a.Put(526,first,2);Compare(a,1,a.A(32),a.A(128),index,UINT64_MAX);}++groups;
 for(std::size_t size=0;size<5400;++size){Arena a;a.Seed();Compare(a,1,a.A(32),a.A(128),0,0x7ff0000000000123ull,size);}++groups;
 for(unsigned form=0;form<2;++form){Arena a;a.Seed();a.Put(33,form,1);if(!form){a.Put(516,0xf1234567u,4);a.Put(521,0,1);}for(auto word:{0ull,UINT64_MAX,0x0000000141e733c0ull,0xfff8000000001234ull}){a.Put(5120,word,8);Compare(a,1,a.A(32),a.A(128),0,word^1);}}
 Arena a;a.Seed();for(auto field:{a.A(512),a.A(128)})Compare(a,1,a.A(32),field,0,a.A(5120));Compare(a,1,0,UINT64_MAX,UINT32_MAX,UINT64_MAX,0);++groups;
}
void RemappedAndZero(){
 for(unsigned generated=0;generated<2;++generated){
  Arena a;a.Seed();a.Put(32,2,1);a.Put(33,generated?0x10:0,1);
  a.Put(516,0x123u,4);a.Put(532,0xf1234567u,4);a.Put(537,0,1);
  for(U32 index:{0u,1u,2u,UINT32_MAX})Compare(a,1,a.A(32),a.A(128),index,0xdeadbeefcafef00dull);
  a.Put(537,255,1);Compare(a,1,a.A(32),a.A(128),0,17,3000);
 }
 Arena wrongType;wrongType.Seed();wrongType.Put(522,2,1);
 Compare(wrongType,1,wrongType.A(32),wrongType.A(128),0,UINT64_MAX,523);
 Arena zero;zero.Seed();zero.Put(33,0x11,1);zero.Put(5128,0,8);
 Compare(zero,1,zero.A(32),zero.A(128),0,UINT64_MAX);
 Compare(zero,2,zero.A(32),zero.A(128),0,UINT64_MAX);
 Arena alias;alias.Seed();alias.Put(512,static_cast<U32>(alias.A(5120)),4);
 Compare(alias,1,alias.A(32),alias.A(48),0,0x7ff8000000000017ull);
 for(unsigned absent=0;absent<2;++absent){Arena a;a.Seed();if(absent==0)a.Put(56,0,8);else a.Put(224,0,8);Compare(a,2,a.A(32),UINT64_MAX,0,UINT64_MAX);}
 ++groups;
}
void Generated(){
 for(U32 type:{1u,4u,8u,9u,17u})for(U32 declared:{0u,1u,2u,UINT32_MAX})for(U32 index:{0u,1u,2u,0x80000000u,0x80000001u,UINT32_MAX}){Arena a;a.Seed();a.Put(33,0x11,1);a.Put(4204,type,4);a.Put(4208,declared,4);a.Put(5128,a.A(6144),8);a.Put(5136,2,4);a.Put(6144,0xffffffffffffffffull,8);a.Put(6152,0x8000000000000001ull,8);Compare(a,2,a.A(32),a.A(128),index,0x7ff8000000001234ull);Compare(a,1,a.A(32),a.A(128),index,0x7ff8000000001234ull);}++groups;
 for(std::size_t size=0;size<5200;++size){Arena a;a.Seed();a.Put(33,0x11,1);Compare(a,1,a.A(32),a.A(128),0,0xabcdef,size);}++groups;
 for(unsigned absent=0;absent<5;++absent){Arena a;a.Seed();a.Put(33,0x11,1);if(absent==0)a.Put(56,0,8);if(absent==1)a.Put(224,0,8);if(absent==2)a.Put(264,0,4);if(absent==3)a.Put(4200,3,4);if(absent==4)a.Put(128,0xdeadbeef,4);Compare(a,2,a.A(32),a.A(128),0,UINT64_MAX);}
 // Generated field hash is consumed before a zero field-count fallback.
 Arena a;a.Seed();a.Put(264,0,4);Compare(a,2,a.A(32),UINT64_MAX,0,17);Compare(a,2,0,UINT64_MAX,UINT32_MAX,0xffffffffffffffffull,0);++groups;
}
}
int wmain(int argc,wchar_t**argv){try{Monstey_Require(argc==2,"Need new report");LookupRandom();FaultAndAlias();RawStrings();RemappedAndZero();Generated();Monstey_WriteNewText(argv[1],Monstey_Json::object({{"status","PASS"},{"groups",groups},{"cases",cases},{"checks",checks},{"independent_recursive_lookup",true},{"independent_literal_generated_reference",true},{"first_fault_and_whole_input_checked",true},{"original_programs_executed",false}}).dump(2)+"\n");std::cout<<"PASS "<<groups<<" groups "<<cases<<" cases "<<checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
