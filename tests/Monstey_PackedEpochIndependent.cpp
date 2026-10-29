#include "Monstey_FearDbPacked.hpp"
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
std::uint64_t Monstey_Groups=0,Monstey_Cases=0,Monstey_Checks=0,Monstey_Compared=0;
void Monstey_Need(bool ok,const char* text){++Monstey_Checks;if(!ok)throw std::runtime_error(text);}
void Monstey_Add(std::vector<std::uint8_t>&b,std::uint32_t x){for(unsigned n=0;n<4;++n)b.push_back(static_cast<std::uint8_t>(x>>(8*n)));}
void Monstey_Store(std::vector<std::uint8_t>&b,std::size_t at,std::uint32_t x){for(unsigned n=0;n<4;++n)b.at(at+n)=static_cast<std::uint8_t>(x>>(8*n));}
std::vector<std::uint8_t> Monstey_Name(std::uint64_t bits){std::vector<std::uint8_t>b;for(unsigned n=0;n<8;++n){const auto x=static_cast<std::uint8_t>(bits>>(8*n));b.push_back(x);if(!x)return b;}throw std::runtime_error("Reference scalar has no NUL");}
std::vector<std::uint8_t> Monstey_Wire(std::size_t prefix,std::uint32_t pool,std::uint32_t roots,std::uint32_t spare){
 std::vector<std::uint8_t>b(prefix,0xcc);for(const auto x:std::array<std::uint32_t,7>{0x42444147,3,pool,roots+spare,0,0,0})Monstey_Add(b,x);
 for(std::uint32_t i=0;i<pool;++i)b.push_back(static_cast<std::uint8_t>(0x61u+i%21u));Monstey_Add(b,roots);
 for(std::uint32_t i=0;i<roots;++i){Monstey_Add(b,0);Monstey_Add(b,0);}return b;
}
void Monstey_Success(std::size_t prefix,std::uint32_t pool,std::uint32_t roots,bool counter,std::uint64_t initialCalls){
 ++Monstey_Cases;auto wire=Monstey_Wire(prefix,pool,roots,1);Monstey_FearDbPackedInput input{wire,prefix,initialCalls};
 const auto first=Monstey_Name(counter?initialCalls+1:prefix+28);const auto graph=Monstey_Name(counter?initialCalls+3:prefix+32+pool);
 const std::uint32_t base=0x30000000u,allocated=pool+20u+(roots+1u)*16u+static_cast<std::uint32_t>(first.size());
 const std::uint32_t used=pool+20u+static_cast<std::uint32_t>(graph.size())+roots*16u;
 const auto name=counter?std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(&input.read_calls),sizeof(input.read_calls)):std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(&input.position),sizeof(input.position));
 const auto got=Monstey_FearDbLoadPacked(input,name,base);Monstey_Need(got.status==Monstey_FearDbPackedStatus::loaded&&got.image.has_value(),"Expected loaded alias image");
 const auto&i=*got.image;Monstey_Need(got.allocation_bytes==allocated&&i.allocation.size()==allocated,"Allocation epoch changed");
 Monstey_Need(i.base==base&&i.database==base+pool&&i.used_bytes==used,"Graph extent epoch changed");
 Monstey_Need(input.position==prefix+32+pool+8u*roots&&input.read_calls==initialCalls+3+roots,"Read sequence/cursor changed");
 std::vector<std::uint8_t>expected(allocated);std::copy_n(wire.begin()+static_cast<std::ptrdiff_t>(prefix+28),pool,expected.begin());
 const auto nameAt=pool+20u,catAt=nameAt+static_cast<std::uint32_t>(graph.size());std::copy(graph.begin(),graph.end(),expected.begin()+nameAt);
 Monstey_Store(expected,pool,base);Monstey_Store(expected,pool+4u,base+nameAt);Monstey_Store(expected,pool+8u,roots);Monstey_Store(expected,pool+12u,base+catAt);Monstey_Store(expected,pool+16u,0);
 for(std::uint32_t c=0;c<roots;++c){const auto at=catAt+c*16u;Monstey_Store(expected,at,base);Monstey_Store(expected,at+4u,base+pool);Monstey_Store(expected,at+8u,0);Monstey_Store(expected,at+12u,base+used);}
 Monstey_Need(i.allocation==expected,"Full raw aliased graph layout mismatch");Monstey_Compared+=expected.size();
}
void Monstey_Failures(){
 ++Monstey_Groups;
 {++Monstey_Cases;std::vector<std::uint8_t>w(27);Monstey_FearDbPackedInput in{w};const auto r=Monstey_FearDbLoadPacked(in,{},0);Monstey_Need(r.status==Monstey_FearDbPackedStatus::header_read_failed&&!r.image&&r.allocation_bytes==0,"Header fail consumes name/base");Monstey_Need(in.position==0&&in.read_calls==1,"Header short-read cursor");}
 {++Monstey_Cases;auto w=Monstey_Wire(0,0,0,0);w[0]=0;Monstey_FearDbPackedInput in{w};const auto r=Monstey_FearDbLoadPacked(in,{},0);Monstey_Need(r.status==Monstey_FearDbPackedStatus::invalid_header&&!r.image&&r.allocation_bytes==0,"Invalid header consumes name/base");Monstey_Need(in.position==28&&in.read_calls==1,"Invalid header cursor");}
 {++Monstey_Cases;auto w=Monstey_Wire(0,228,0,1);w.resize(255);Monstey_FearDbPackedInput in{w};const auto n=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(&in.position),sizeof(in.position));const auto r=Monstey_FearDbLoadPacked(in,n,0x30000000);Monstey_Need(r.status==Monstey_FearDbPackedStatus::pool_read_failed&&!r.image&&r.allocation_bytes==266,"Pool fail status/initial allocation");Monstey_Need(in.position==28&&in.read_calls==2,"Pool short-read cursor");}
 {++Monstey_Cases;auto w=Monstey_Wire(0,228,0,1);w.resize(258);Monstey_FearDbPackedInput in{w};const auto n=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(&in.position)+1,1);const auto r=Monstey_FearDbLoadPacked(in,n,0x30000000);Monstey_Need(r.status==Monstey_FearDbPackedStatus::graph_read_failed&&!r.image&&r.allocation_bytes==265,"Failed count must precede second scan");Monstey_Need(in.position==256&&in.read_calls==3,"Count short-read cursor");}
 {++Monstey_Cases;auto w=Monstey_Wire(0,228,0,1);Monstey_FearDbPackedInput in{w};const auto n=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(&in.position)+1,1);bool caught=false;try{(void)Monstey_FearDbLoadPacked(in,n,0x30000000);}catch(const std::invalid_argument&){caught=true;}Monstey_Need(caught&&in.position==260&&in.read_calls==3,"Second unterminated scan wrong epoch");}
 {++Monstey_Cases;auto w=Monstey_Wire(0,228,0,0);Monstey_FearDbPackedInput in{w};const auto n=std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(&in.position),sizeof(in.position));bool caught=false;try{(void)Monstey_FearDbLoadPacked(in,n,0x30000000);}catch(const Monstey_FearDbFault&e){caught=true;Monstey_Need(e.address==0x300000fau&&e.width==1&&e.writing,"Graph range fault not first copied byte");}Monstey_Need(caught&&in.position==260&&in.read_calls==3,"No reallocation on graph-name growth");}
}
}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::invalid_argument("NEW_REPORT required");
 ++Monstey_Groups;Monstey_Success(0,228,0,false,0);
 ++Monstey_Groups;for(auto prefix:std::array<std::size_t,6>{0,1,224,225,228,65504})for(auto pool:std::array<std::uint32_t,18>{0,1,3,7,20,100,224,225,226,227,228,229,230,255,256,257,65504,65508})for(std::uint32_t roots=0;roots<2;++roots)Monstey_Success(prefix,pool,roots,false,0);
 ++Monstey_Groups;for(auto initial:std::array<std::uint64_t,8>{0,1,252,253,254,255,65534,65535})for(auto pool:std::array<std::uint32_t,4>{0,7,228,256})for(std::uint32_t roots=0;roots<2;++roots)Monstey_Success(0,pool,roots,true,initial);
 Monstey_Failures();
 std::ofstream out(argv[1],std::ios::binary);if(!out)throw std::runtime_error("New report unavailable");out<<"{\"status\":\"PASS\",\"groups\":"<<Monstey_Groups<<",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"owned_bytes_compared\":"<<Monstey_Compared<<"}\n";if(!out)throw std::runtime_error("Report write failed");std::cout<<"PASS packed epoch independent "<<Monstey_Cases<<" cases\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
