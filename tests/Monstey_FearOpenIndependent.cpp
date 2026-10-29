#include "Monstey_FearDbOpen.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using Monstey_Word=std::uint32_t;
std::uint64_t Monstey_Groups=0,Monstey_Cases=0,Monstey_Checks=0,Monstey_Faults=0;
void Monstey_Need(bool ok,const char*s){++Monstey_Checks;if(!ok)throw std::runtime_error(s);}
void Monstey_Put(Monstey_FearDbPackedImage&i,Monstey_Word at,Monstey_Word x){for(unsigned n=0;n<4;++n)i.allocation.at(at-i.base+n)=static_cast<std::uint8_t>(x>>(8*n));}
Monstey_Word Monstey_Get(const Monstey_FearDbPackedImage&i,Monstey_Word at){Monstey_Word x=0;for(unsigned n=0;n<4;++n)x|=Monstey_Word(i.allocation.at(at-i.base+n))<<(8*n);return x;}
Monstey_FearDbPackedImage Monstey_Image(unsigned index){Monstey_FearDbPackedImage i;i.base=0x100000u+index*0x100u;i.database=i.base+16;i.used_bytes=64;i.allocation.resize(64,0xa5);Monstey_Put(i,i.database,i.base);Monstey_Put(i,i.database+4,index%5==4?0u:0x60000000u+index);Monstey_Put(i,i.database+16,0xffffffffu-index);return i;}
bool Monstey_Same(const Monstey_FearDbResources&a,const Monstey_FearDbResources&b){if(a.cache!=b.cache||a.allocations.size()!=b.allocations.size())return false;for(std::size_t n=0;n<a.allocations.size();++n){const auto&x=a.allocations[n];const auto&y=b.allocations[n];if(x.base!=y.base||x.database!=y.database||x.used_bytes!=y.used_bytes||x.allocation!=y.allocation)return false;}return true;}
Monstey_FearDbResources Monstey_Base(){Monstey_FearDbResources r;r.allocations={Monstey_Image(0),Monstey_Image(1),Monstey_Image(2)};r.cache.reserve(16);return r;}
constexpr Monstey_Word Monstey_Query=0x70000000;const std::array<std::uint8_t,1> Monstey_File{'Q'};
Monstey_FearDbMemory Monstey_Filename(){return{Monstey_Query,Monstey_File};}
void Monstey_CacheOracle(){++Monstey_Groups;
 for(unsigned n=0;n<=64;++n)for(unsigned target=0;target<=n;++target){++Monstey_Cases;Monstey_FearDbResources r;for(unsigned j=0;j<n;++j)r.allocations.push_back(Monstey_Image(j));for(unsigned j=0;j<2*n+1&&n;++j)r.cache.push_back(r.allocations[(j*7+n/2)%n].database);auto expected=r;std::vector<Monstey_Word> actual,trace;Monstey_Word wanted=0;
  for(const auto db:expected.cache){auto&im=expected.allocations[(db-0x100010)/0x100];const auto name=Monstey_Get(im,db+4);if(!name)continue;trace.push_back(name);if(target<n&&name==0x60000000u+target){wanted=db;Monstey_Put(im,db+16,Monstey_Get(im,db+16)+1u);break;}}
  const auto got=Monstey_FearDbOpenExisting(r,Monstey_Filename(),Monstey_Query,[&](auto q,auto name){Monstey_Need(q==Monstey_Query,"Comparator argument identity order");actual.push_back(name);return target<n&&name==0x60000000u+target?0:256;});
  Monstey_Need(got==wanted&&trace==actual,"Ordered first equal cache reference");Monstey_Need(Monstey_Same(r,expected),"Whole cache/resource state differs from linear oracle");
 }
}
void Monstey_ResultWords(){++Monstey_Groups;for(auto v:std::array<std::int32_t,9>{0,1,-1,256,-256,65536,-65536,INT32_MIN,INT32_MAX}){++Monstey_Cases;auto r=Monstey_Base();r.cache={r.allocations[0].database,r.allocations[1].database};unsigned calls=0;const auto got=Monstey_FearDbOpenExisting(r,Monstey_Filename(),Monstey_Query,[&](auto,auto){return ++calls==1?v:0;});const auto selected=v==0?0u:1u;Monstey_Need(got==r.allocations[selected].database&&calls==(v==0?1u:2u),"OpenExisting needs full EAX zero only");Monstey_Need(Monstey_Get(r.allocations[selected],got+16)==0xffffffffu-selected+1u,"Selected reference did not wrap");}}
template<class Monstey_Call>void Monstey_Fault(Monstey_Call call,Monstey_Word at,unsigned width){++Monstey_Cases;bool caught=false;try{call();}catch(const Monstey_FearDbFault&e){caught=true;++Monstey_Faults;Monstey_Need(e.address==at&&e.width==width&&!e.writing,"Wrong consumed Open read");}Monstey_Need(caught,"Expected Open range fault missing");}
void Monstey_LiveCache(){++Monstey_Groups;
 {++Monstey_Cases;auto r=Monstey_Base();r.cache={r.allocations[0].database};const auto second=r.allocations[1].database;const auto got=Monstey_FearDbOpenExisting(r,Monstey_Filename(),Monstey_Query,[&](auto,auto){r.cache[0]=second;return 0;});Monstey_Need(got==second&&Monstey_Get(r.allocations[0],r.allocations[0].database+16)==0xffffffff&&Monstey_Get(r.allocations[1],second+16)==0xffffffff,"Matching row was not reloaded before retain");}
 {++Monstey_Cases;auto r=Monstey_Base();r.cache={r.allocations[0].database};unsigned calls=0;const auto got=Monstey_FearDbOpenExisting(r,Monstey_Filename(),Monstey_Query,[&](auto,auto){if(++calls==1){r.cache.push_back(r.allocations[1].database);return 1;}return 0;});Monstey_Need(got==r.allocations[1].database&&calls==2,"Miss did not reload grown live cache end");}
 {++Monstey_Cases;auto r=Monstey_Base();r.cache={r.allocations[0].database,r.allocations[1].database};unsigned calls=0;const auto got=Monstey_FearDbOpenExisting(r,Monstey_Filename(),Monstey_Query,[&](auto,auto){++calls;r.cache.resize(1);return -1;});Monstey_Need(got==0&&calls==1,"End equal to advanced cursor should stop");}
 {++Monstey_Cases;auto r=Monstey_Base();r.cache={r.allocations[0].database,r.allocations[1].database,r.allocations[2].database};unsigned calls=0;bool caught=false;try{(void)Monstey_FearDbOpenExisting(r,Monstey_Filename(),Monstey_Query,[&](auto,auto){if(++calls==2)r.cache.resize(1);return 1;});}catch(const std::out_of_range&){caught=true;}Monstey_Need(caught&&calls==2&&r.cache.size()==1,"End shrink behind cursor fabricated a miss");}
 {auto r=Monstey_Base();r.cache={r.allocations[0].database};Monstey_Fault([&]{(void)Monstey_FearDbOpenExisting(r,Monstey_Filename(),Monstey_Query,[&](auto,auto){r.cache[0]=0;return 0;});},16,4);Monstey_Need(r.cache[0]==0,"Callback prefix lost before retain fault");}
 {++Monstey_Cases;auto r=Monstey_Base();r.cache={r.allocations[0].database};bool caught=false;try{(void)Monstey_FearDbOpenExisting(r,Monstey_Filename(),Monstey_Query,[&](auto,auto){r.cache.reserve(r.cache.capacity()+1);return 0;});}catch(const std::invalid_argument&){caught=true;}Monstey_Need(caught&&Monstey_Get(r.allocations[0],r.allocations[0].database+16)==0xffffffff,"Moved cache backing silently retained stale row");}
}
void Monstey_NewGuards(){++Monstey_Groups;
 {++Monstey_Cases;auto r=Monstey_Base();r.cache={r.allocations[0].database};Monstey_FearDbPackedInput ignored{{},999,123};const auto got=Monstey_FearDbOpenNew(r,Monstey_Filename(),Monstey_Query,ignored,0,[](auto,auto){return 0;});Monstey_Need(got==r.allocations[0].database&&ignored.position==999&&ignored.read_calls==123,"Hit consumes stream/allocation/filename terminator");}
 {++Monstey_Cases;auto r=Monstey_Base();r.cache={0};Monstey_FearDbPackedInput ignored{{},999,123};const auto got=Monstey_FearDbOpenNew(r,{0,{}},0,ignored,0,{});Monstey_Need(got==0&&ignored.position==999&&ignored.read_calls==123,"Null filename consumed cache/stream");}
 {++Monstey_Cases;auto r=Monstey_Base();r.cache={r.allocations[0].database};std::array<std::uint8_t,1> name{'Q'};Monstey_FearDbPackedInput ignored{{},999,123};unsigned calls=0;const auto got=Monstey_FearDbOpenNew(r,{Monstey_Query,name},Monstey_Query,ignored,0,[&](auto,auto){++calls;name[0]=0;return 1;});Monstey_Need(got==0&&calls==1&&ignored.position==999&&ignored.read_calls==123,"Miss did not recheck live filename byte");}
 {++Monstey_Cases;auto r=Monstey_Base();r.cache={r.allocations[0].database,r.allocations[1].database};std::array<std::uint8_t,1> name{'Q'};unsigned calls=0;const auto got=Monstey_FearDbOpenExisting(r,{Monstey_Query,name},Monstey_Query,[&](auto,auto){name[0]=0;return ++calls==1?1:0;});Monstey_Need(got==r.allocations[1].database&&calls==2,"Existing loop rechecked filename after callback");}
}
void Monstey_Add(std::vector<std::uint8_t>&b,Monstey_Word x){for(unsigned n=0;n<4;++n)b.push_back(static_cast<std::uint8_t>(x>>(8*n)));}
std::vector<std::uint8_t> Monstey_Wire(std::size_t prefix,Monstey_Word pool,Monstey_Word spare=1){std::vector<std::uint8_t>b(prefix,0xcd);for(auto x:std::array<Monstey_Word,7>{0x42444147,3,pool,spare,0,0,0})Monstey_Add(b,x);b.resize(prefix+28+pool,0x61);Monstey_Add(b,0);return b;}
std::size_t Monstey_Extent(std::uint64_t value){for(unsigned i=0;i<8;++i)if(((value>>(8*i))&255u)==0)return i+1;throw std::runtime_error("Synthetic name lacks NUL");}
void Monstey_PublicAliases(){++Monstey_Groups;
 for(bool counter:std::array<bool,2>{false,true})for(auto initial:std::array<std::uint64_t,7>{1,2,224,225,227,255,65503})for(auto pool:std::array<Monstey_Word,6>{0,1,5,227,228,256}){++Monstey_Cases;const auto prefix=counter?0:static_cast<std::size_t>(initial);auto wire=Monstey_Wire(prefix,pool);const auto original=wire;Monstey_FearDbPackedInput input{wire,prefix,counter?initial:0};const auto*field=counter?reinterpret_cast<const std::uint8_t*>(&input.read_calls):reinterpret_cast<const std::uint8_t*>(&input.position);Monstey_FearDbResources r;
  const auto got=Monstey_FearDbOpenNew(r,{Monstey_Query,std::span<const std::uint8_t>(field,8)},Monstey_Query,input,0x30000000,{});const auto first=Monstey_Extent(counter?initial+1:prefix+28),last=Monstey_Extent(counter?initial+3:prefix+32+pool);const auto bits=counter?initial+3:prefix+32+pool;
  Monstey_Need(r.allocations.size()==1&&r.cache==std::vector<Monstey_Word>{got}&&got==0x30000000+pool,"Public alias failed real adoption");const auto&i=r.allocations[0];Monstey_Need(i.allocation.size()==pool+36+first&&i.used_bytes==pool+20+last&&Monstey_Get(i,got+12)==0x30000000+i.used_bytes,"Public alias allocation/graph epochs");Monstey_Need(Monstey_Get(i,got+16)==1&&Monstey_Get(i,got+4)==got+20,"Append/retain/copy order");for(std::size_t n=0;n<last;++n)Monstey_Need(i.allocation.at(pool+20+n)==static_cast<std::uint8_t>(bits>>(8*n)),"Copied public filename payload");
  Monstey_Need(input.position==prefix+32+pool&&input.read_calls==(counter?initial:0)+3&&wire==original,"Caller-owned input progress/source");const auto release=Monstey_FearDbReleaseDatabase(r,got);Monstey_Need(release.kind==Monstey_FearDbReleaseKind::freed&&r.allocations.empty()&&r.cache.empty(),"Composed public lifetime cleanup");
 }
}
void Monstey_LoadFailures(){++Monstey_Groups;
 for(unsigned kind=0;kind<5;++kind){++Monstey_Cases;auto r=Monstey_Base();const auto before=r;auto wire=Monstey_Wire(0,7);if(kind==0)wire.resize(27);if(kind==1)wire[0]=0;if(kind==2)wire.resize(34);if(kind==3)wire.resize(37);Monstey_FearDbPackedInput input{wire};std::array<std::uint8_t,2> name{'x',0};const auto filename=kind==4?Monstey_Filename():Monstey_FearDbMemory{Monstey_Query,name};bool caught=false;Monstey_Word got=0;try{got=Monstey_FearDbOpenNew(r,filename,Monstey_Query,input,0x30000000,{});}catch(const std::invalid_argument&){caught=true;}
  Monstey_Need(Monstey_Same(r,before)&&got==0,"Failed load adopted resources/cache");Monstey_Need(caught==(kind==4),"Header versus filename extent failure order");const std::array<std::size_t,5> positions{0,28,28,35,28};const std::array<std::uint64_t,5> calls{1,1,2,3,1};Monstey_Need(input.position==positions[kind]&&input.read_calls==calls[kind],"Failed load rewound or consumed later input");
 }
 {++Monstey_Cases;auto r=Monstey_Base();const auto before=r;auto wire=Monstey_Wire(0,0);Monstey_FearDbPackedInput input{wire};std::array<std::uint8_t,2> name{'x',0};bool caught=false;try{(void)Monstey_FearDbOpenNew(r,{Monstey_Query,name},Monstey_Query,input,r.allocations[0].base,{});}catch(const std::invalid_argument&){caught=true;}Monstey_Need(caught&&input.position==32&&input.read_calls==3&&Monstey_Same(r,before),"Overlap must reject after load before adoption");}
}
void Monstey_ConsumedBoundary(){++Monstey_Groups;
 {auto r=Monstey_Base();Monstey_Fault([&]{(void)Monstey_FearDbOpenExisting(r,{0,{}},Monstey_Query,{});},Monstey_Query,1);}
 {auto r=Monstey_Base();r.cache={0};Monstey_Fault([&]{(void)Monstey_FearDbOpenExisting(r,Monstey_Filename(),Monstey_Query,{});},4,4);}
 {++Monstey_Cases;auto r=Monstey_Base();r.cache={r.allocations[0].database};const auto before=r;bool caught=false;try{(void)Monstey_FearDbOpenExisting(r,Monstey_Filename(),Monstey_Query,{});}catch(const std::invalid_argument&){caught=true;}Monstey_Need(caught&&Monstey_Same(r,before),"Missing consumed comparator changed state");}
 {++Monstey_Cases;auto r=Monstey_Base();r.cache={r.allocations[0].database};Monstey_FearDbPackedInput input{{},999,123};bool caught=false;try{(void)Monstey_FearDbOpenNew(r,Monstey_Filename(),Monstey_Query,input,0,[&](auto,auto)->std::int32_t{r.cache.push_back(r.allocations[1].database);throw std::runtime_error("external sentinel");});}catch(const std::runtime_error&e){caught=std::string(e.what())=="external sentinel";}Monstey_Need(caught&&r.cache.size()==2&&input.position==999&&input.read_calls==123,"External failure swallowed or loader used");}
}
}
int main(int argc,char**argv){try{if(argc!=2)throw std::runtime_error("NEW_REPORT required");Monstey_CacheOracle();Monstey_ResultWords();Monstey_LiveCache();Monstey_NewGuards();Monstey_PublicAliases();Monstey_LoadFailures();Monstey_ConsumedBoundary();std::ofstream out(argv[1],std::ios::binary);if(!out)throw std::runtime_error("Cannot create report");out<<"{\"status\":\"PASS\",\"groups\":"<<Monstey_Groups<<",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"consumed_faults\":"<<Monstey_Faults<<",\"historical_comparator_claimed\":false}\n";if(!out)throw std::runtime_error("Report write failed");std::cout<<"PASS Open independent "<<Monstey_Cases<<" cases "<<Monstey_Checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
