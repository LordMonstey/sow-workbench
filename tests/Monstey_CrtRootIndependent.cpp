#include "Monstey_FearCrtComparison.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
namespace {
using U8=std::uint8_t;using U32=std::uint32_t;using S32=std::int32_t;
std::uint64_t Monstey_Cases=0,Monstey_Checks=0;
void Monstey_Check(bool value,const char* label){++Monstey_Checks;if(!value)throw std::runtime_error(label);}
int Monstey_Lower(unsigned c){return c>=65&&c<=90?static_cast<int>(c+32):static_cast<int>(c);}
int Monstey_Expected(const std::array<U8,3>& a,const std::array<U8,3>& b){for(std::size_t i=0;i<a.size();++i){const auto x=Monstey_Lower(a[i]),y=Monstey_Lower(b[i]);if(x!=y)return x<y?-1:1;if(x==0)return 0;}throw std::runtime_error("missing terminator in independent fixture");}
void Monstey_Ascii(){
 for(unsigned prefix=0;prefix<4;++prefix)for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b){
  std::array<U8,3> left{static_cast<U8>(a),0,0},right{static_cast<U8>(b),0,0};
  if(prefix){left={static_cast<U8>(prefix==1?'A':prefix==2?0xE9:'!'),static_cast<U8>(a),0};right={static_cast<U8>(prefix==1?'a':prefix==2?0xE9:'!'),static_cast<U8>(b),0};}
  const std::array<Monstey::Monstey_CrtRegion,2> regions{{{0x1000,left},{0x2000,right}}};
  Monstey_Check(Monstey::Monstey_CrtStricmpC({regions},0x1000,0x2000)==Monstey_Expected(left,right),"independent ASCII unsigned normalized result");++Monstey_Cases;
 }
}
void Monstey_Folds(){
 constexpr U32 base=0x3000,table=0x3100;
 std::array<U8,0x400> data{};
 auto put=[&](U32 at,U32 value,U32 width){for(U32 i=0;i<width;++i)data.at(at-base+i)=static_cast<U8>(value>>(8*i));};
 for(U32 mode:std::array<U32,6>{0,1,2,0xFFFFFFFFu,0x80000000u,0x7FFFFFFFu})for(U32 gate:{0u,1u})for(U32 classification:{0u,1u,0x80u,0x81u})for(bool dbcs:{false,true})for(S32 count:{-1,0,1,2})for(U32 c=0;c<256;++c){
  data.fill(0);put(base+0x14,0x409,4);put(base+0x24,gate,4);put(base+0x28,mode,4);put(base+0x48,table,4);put(base+4,932,4);put(table+c*2,classification,2);put(table+1,dbcs?0x80u:0u,1);
  std::vector<Monstey::Monstey_CrtRegion> regions;
  // Every consumed WORD/DWORD is split into single-byte views of one allocation.
  for(U32 i=0;i<data.size();++i)regions.push_back({base+i,std::span<const U8>(data).subspan(i,1)});
  if((c&1u)!=0)std::reverse(regions.begin(),regions.end());
  unsigned scratch_calls=0,map_calls=0;
  Monstey::Monstey_CrtBoundaries boundaries;
  boundaries.initial_map_scratch=[&](auto site,std::uint64_t iteration){++scratch_calls;Monstey_Check(site==Monstey::Monstey_CrtCallSite::Direct&&iteration==17,"map scratch identity");return std::array<U8,3>{0xD4,0xE5,0xF6};};
  boundaries.lc_map_string_a=[&](const auto& q,auto output){++map_calls;Monstey_Check(q.locale_id==0x409&&q.code_page==932&&q.flags==0x100&&q.strict==1&&q.source_count==(dbcs?2u:1u)&&q.destination_capacity==3,"independent map fields");Monstey_Check(q.source== (dbcs?std::array<U8,3>{0,static_cast<U8>(c),0}:std::array<U8,3>{static_cast<U8>(c),0,0}),"independent map source");Monstey_Check(output[0]==0xD4&&output[1]==0xE5&&output[2]==0xF6,"initial stack scratch");output[0]=0xA7;return count;};
  const bool ascii=gate!=0&&c<=0x7F;
  // c==0 shares table+1: eligibility remains classification bit0 regardless of its high byte.
  const bool mapped=!ascii&&(classification&1)!=0;
  const S32 expected=ascii?Monstey_Lower(c):!mapped||count==0?static_cast<S32>(c):count==1?0xA7:0xA7E5;
  Monstey_Check(Monstey::Monstey_CrtFoldByte({regions},base,static_cast<U8>(c),boundaries,Monstey::Monstey_CrtCallSite::Direct,17)==expected,"independent fold result");
  Monstey_Check(scratch_calls==static_cast<unsigned>(mapped)&&map_calls==static_cast<unsigned>(mapped),"consumed map callback count");++Monstey_Cases;
 }
}
}
int main(){try{Monstey_Ascii();Monstey_Folds();std::cout<<"{\"status\":\"PASS\",\"groups\":2,\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
