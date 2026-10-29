#include "Monstey_FearDbResources.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using Monstey_Word=std::uint32_t;
using Monstey_State=Monstey::Monstey_FearDbResources;
struct Monstey_ExpectedFault{Monstey_Word address,width;bool write;};
struct Monstey_Failure{Monstey_Word address=0,width=0;bool fault=false,write=false;};
std::span<std::uint8_t> Monstey_RefRange(Monstey_State&s,Monstey_Word a,bool write){
 for(auto&im:s.allocations){
  const std::uint64_t begin=im.base,end=begin+im.allocation.size(),at=a;
  if(at>=begin&&at+4<=0x100000000ull&&at+4<=end)return std::span(im.allocation).subspan(static_cast<std::size_t>(at-begin),4);
 }
 throw Monstey_ExpectedFault{a,4,write};
}
Monstey_Word Monstey_RefRead(Monstey_State&s,Monstey_Word a){auto b=Monstey_RefRange(s,a,false);return Monstey_Word(b[0])|(Monstey_Word(b[1])<<8)|(Monstey_Word(b[2])<<16)|(Monstey_Word(b[3])<<24);}
void Monstey_RefStore(Monstey_State&s,Monstey_Word a,Monstey_Word v){auto b=Monstey_RefRange(s,a,true);for(unsigned i=0;i<4;++i)b[i]=static_cast<std::uint8_t>(v>>(8*i));}
void Monstey_RefEffect(Monstey_State&s,unsigned api,Monstey_Word db){
 if(api==2&&db==0)return;
 if(api==1)s.cache.push_back(db);
 const auto at=db+16u;
 const auto before=Monstey_RefRead(s,at);
 const auto after=api==2?before-1u:before+1u;
 Monstey_RefStore(s,at,after);
 if(api!=2||after!=0)return;
 for(std::size_t i=0;i<s.cache.size();++i)if(s.cache[i]==db){
  for(std::size_t j=i+1;j<s.cache.size();++j)s.cache[j-1]=s.cache[j];
  s.cache.pop_back();break;
 }
 const auto allocation=Monstey_RefRead(s,db);
 if(allocation==0)return;
 for(std::size_t i=0;i<s.allocations.size();++i)if(s.allocations[i].base==allocation){s.allocations.erase(s.allocations.begin()+static_cast<std::ptrdiff_t>(i));return;}
 throw Monstey_ExpectedFault{allocation,0,true};
}
bool Monstey_Equal(const Monstey_State&a,const Monstey_State&b){
 if(a.cache!=b.cache||a.allocations.size()!=b.allocations.size())return false;
 for(std::size_t i=0;i<a.allocations.size();++i){const auto&x=a.allocations[i];const auto&y=b.allocations[i];if(x.base!=y.base||x.database!=y.database||x.used_bytes!=y.used_bytes||x.allocation!=y.allocation)return false;}
 return true;
}
std::uint64_t Monstey_Cases=0,Monstey_Checks=0;
void Monstey_Compare(Monstey_State&a,Monstey_State&b,unsigned api,Monstey_Word db){
 Monstey_Failure want,got;
 try{Monstey_RefEffect(a,api,db);}catch(const Monstey_ExpectedFault&f){want={f.address,f.width,true,f.write};}
 try{
  if(api==0)Monstey::Monstey_FearDbRetainSelected(b,db);
  else if(api==1)Monstey::Monstey_FearDbRegisterLoaded(b,db);
  else (void)Monstey::Monstey_FearDbReleaseDatabase(b,db);
 }catch(const Monstey::Monstey_FearDbFault&f){got={f.address,f.width,true,f.writing};}
 ++Monstey_Cases;
 if(want.fault!=got.fault||want.address!=got.address||want.width!=got.width||want.write!=got.write)throw std::runtime_error("Fault differs at case "+std::to_string(Monstey_Cases));
 ++Monstey_Checks;
 if(!Monstey_Equal(a,b))throw std::runtime_error("State/mutation order differs at case "+std::to_string(Monstey_Cases));
 ++Monstey_Checks;
}
Monstey_State Monstey_Seed(Monstey_Word count,Monstey_Word freeTarget=0x10000){
 Monstey_State s;
 for(auto base:std::array<Monstey_Word,2>{0x10000,0x20000}){
  Monstey::Monstey_FearDbPackedImage im;im.base=base;im.database=base+8;im.used_bytes=32;im.allocation.resize(40,0x7b);s.allocations.push_back(std::move(im));
 }
 Monstey_RefStore(s,0x10008,freeTarget);Monstey_RefStore(s,0x10018,count);return s;
}
void Monstey_One(const Monstey_State&seed,unsigned api,Monstey_Word db){auto a=seed,b=seed;Monstey_Compare(a,b,api,db);}
}
int main(){try{
 constexpr std::array<Monstey_Word,7> counts{0,1,2,3,0x7fffffffu,0x80000000u,0xffffffffu};
 // Group1: duplicate positions, cache misses and full signed-bit counters.
 for(unsigned length=0;length<=10;++length)for(unsigned mask=0;mask<(1u<<length);++mask)for(auto count:counts){
  auto s=Monstey_Seed(count);for(unsigned i=0;i<length;++i)s.cache.push_back(mask&(1u<<i)?0x10008u:0x20008u+i*16u);
  for(unsigned api=0;api<3;++api)Monstey_One(s,api,0x10008);
 }
 // Group2: free uses stored allocation, even when different from owner or cache absent.
 for(auto target:std::array<Monstey_Word,5>{0,0x10000,0x20000,0x20001,0xffffffffu})for(unsigned mode=0;mode<4;++mode){
  auto s=Monstey_Seed(1,target);if(mode&1)s.cache={0x20008,0x10008,0x20018};if(mode&2)s.cache.push_back(0x10008);
  Monstey_One(s,2,0x10008);
 }
 // Group3: every allocation prefix and fault after prior append/decrement/erase.
 for(std::size_t size=0;size<=40;++size)for(auto count:counts)for(unsigned api=0;api<3;++api){
  auto s=Monstey_Seed(count);s.cache={0x20008,0x10008,0x10008};s.allocations[0].allocation.resize(size);Monstey_One(s,api,0x10008);
 }
 // Group4: counter storage can be present while the earlier header/free target is unavailable.
 for(auto count:counts)for(unsigned api=0;api<3;++api){
  Monstey_State s;Monstey::Monstey_FearDbPackedImage im;im.base=0x10018;im.database=0x10008;im.allocation.resize(4);s.allocations.push_back(std::move(im));Monstey_RefStore(s,0x10018,count);s.cache={0x10008,0x20008,0x10008};Monstey_One(s,api,0x10008);
 }
 // Group5: full lifecycle sequences and stale duplicate rows after free.
 for(auto count:counts)for(unsigned mask=0;mask<16;++mask){
  auto a=Monstey_Seed(count),b=a;for(unsigned i=0;i<4;++i)a.cache.push_back(mask&(1u<<i)?0x10008:0x20008);b=a;
  for(unsigned api:std::array<unsigned,7>{1,0,2,2,2,1,2})Monstey_Compare(a,b,api,0x10008);
 }
 // Group6: native32 effective-address wrapping and null-handle asymmetry.
 for(auto db:std::array<Monstey_Word,8>{0,1,0xfffffff0u,0xfffffff8u,0xffffffffu,0x10007,0x10008,0x20008})for(unsigned api=0;api<3;++api){
  auto s=Monstey_Seed(1);Monstey_One(s,api,db);
 }
 std::cout<<"PASS resource_independent groups=6 cases="<<Monstey_Cases<<" checks="<<Monstey_Checks<<"\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}