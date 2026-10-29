#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>
namespace Monstey_LtaReference {
struct Result{unsigned kind=0;std::uint64_t bits=0,literals=0,spans=0,span_bytes=0;std::uint32_t unknown=0;std::vector<std::uint8_t>bytes;};
inline Result Decode(std::span<const std::uint8_t>input){
 Result r;std::array<std::uint8_t,4096>ring{};std::array<bool,4096>known{};std::uint32_t cursor=1;
 auto read=[&](unsigned count,std::uint32_t&out){out=0;for(unsigned n=0;n<count;++n){if(r.bits/8>=input.size())return false;const auto bit=(input[static_cast<std::size_t>(r.bits/8)]>>(r.bits%8))&1u;++r.bits;out=out*2u+bit;}return true;};
 std::uint32_t value=0;if(!read(32,value)){r.kind=1;return r;}if(value!=0){r.kind=2;return r;}
 for(;;){std::uint32_t flag=0;if(!read(1,flag)){r.kind=3;return r;}
  if(flag){if(!read(8,value)){r.kind=4;return r;}r.bytes.push_back(static_cast<std::uint8_t>(value));ring[cursor]=static_cast<std::uint8_t>(value);known[cursor]=true;cursor=(cursor+1u)%4096u;++r.literals;}
  else{std::uint32_t source=0;if(!read(12,source)){r.kind=5;return r;}if(source==0)return r;if(!read(4,value)){r.kind=6;return r;}++r.spans;const auto count=value+2u;
   for(std::uint32_t n=0;n<count;++n){if(!known[source]){r.kind=7;r.unknown=source;return r;}const auto byte=ring[source];ring[cursor]=byte;known[cursor]=true;r.bytes.push_back(byte);source=(source+1u)%4096u;cursor=(cursor+1u)%4096u;++r.span_bytes;}
  }
 }
}
struct Builder{std::vector<std::uint8_t>data;std::uint64_t bits=0;void Field(std::uint32_t x,unsigned count){for(unsigned n=0;n<count;++n){if(bits%8==0)data.push_back(0);if((x>>(count-1u-n))&1u)data.back()|=static_cast<std::uint8_t>(1u<<(bits%8));++bits;}}Builder(){Field(0,32);}void Literal(std::uint8_t x){Field(1,1);Field(x,8);}void Span(std::uint32_t source,std::uint32_t length){Field(0,1);Field(source,12);Field(length-2u,4);}void End(){Field(0,1);Field(0,12);}};
}
