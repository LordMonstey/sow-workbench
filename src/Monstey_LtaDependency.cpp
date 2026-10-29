#include "Monstey_LtaDependency.hpp"
#include <array>
namespace Monstey {
Monstey_LtaDecodeObservation Monstey_ObserveLtaCompressed(std::span<const std::uint8_t>input,std::uint32_t budget){
 Monstey_LtaDecodeObservation r;std::array<std::uint8_t,4096>ring;std::array<bool,4096>known{};std::uint32_t cursor=1;
 auto bit=[&](std::uint32_t&out){if(r.consumed_bits/8>=input.size())return false;out=(input[static_cast<std::size_t>(r.consumed_bits/8)]>>(r.consumed_bits%8))&1u;++r.consumed_bits;return true;};
 auto field=[&](unsigned width,std::uint32_t&out){out=0;for(unsigned i=0;i<width;++i){std::uint32_t b=0;if(!bit(b))return false;out=(out<<1)|b;}return true;};
 std::uint32_t header=0;if(!field(32,header)){r.status="truncated_header";return r;}if(header!=0){r.status="unsupported_nonzero_header";return r;}
 auto append=[&](std::uint8_t value){if(r.decoded.size()>=budget){r.status="output_budget_exhausted";return false;}ring[cursor]=value;known[cursor]=true;cursor=(cursor+1u)&4095u;r.decoded.push_back(value);return true;};
 for(;;){std::uint32_t tag=0;if(!bit(tag)){r.status="missing_end_marker";return r;}if(tag!=0){std::uint32_t value=0;if(!field(8,value)){r.status="truncated_literal";return r;}if(!append(static_cast<std::uint8_t>(value)))return r;++r.literals;}else{std::uint32_t index=0,length=0;if(!field(12,index)){r.status="truncated_span_index";return r;}if(index==0){r.status="end_marker";return r;}if(!field(4,length)){r.status="truncated_span_length";return r;}length+=2u;++r.spans;for(std::uint32_t n=0;n<length;++n){if(!known[index]){r.status="unavailable_initial_window";r.unavailable_window_index=index;return r;}if(!append(ring[index]))return r;index=(index+1u)&4095u;++r.span_bytes;}}}
}
}
