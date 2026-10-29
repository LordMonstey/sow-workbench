#include "Monstey_LtaDependency.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {std::uint64_t Monstey_checks=0,Monstey_cases=0;void Monstey_Check(bool x){++Monstey_checks;if(!x)throw std::runtime_error("native-derived LTA grammar mismatch");}
struct Monstey_Bits{std::vector<std::uint8_t>b;std::uint64_t bits=0;void Bit(unsigned x){if(bits%8==0)b.push_back(0);if(x)b.back()|=static_cast<std::uint8_t>(1u<<(bits%8));++bits;}void Field(unsigned n,std::uint32_t x){for(unsigned i=n;i>0;--i)Bit((x>>(i-1))&1u);}Monstey_Bits(){Field(32,0);}void Literal(std::uint8_t x){Bit(1);Field(8,x);}void Span(std::uint32_t p,std::uint32_t n){Bit(0);Field(12,p);if(p!=0)Field(4,n-2);}void End(){Span(0,0);}};
}
int main(int argc,char**argv){try{if(argc!=2)throw std::runtime_error("report");for(unsigned n=0;n<=256;++n){Monstey_Bits in;std::vector<std::uint8_t>expected;for(unsigned i=0;i<n;++i){in.Literal(static_cast<std::uint8_t>(i));expected.push_back(static_cast<std::uint8_t>(i));}in.End();auto r=Monstey::Monstey_ObserveLtaCompressed(in.b);Monstey_Check(r.status=="end_marker"&&r.decoded==expected&&r.literals==n&&r.spans==0&&r.consumed_bits==in.bits);++Monstey_cases;}
 for(unsigned length=2;length<=17;++length){Monstey_Bits in;in.Literal('A');in.Span(1,length);in.End();const auto r=Monstey::Monstey_ObserveLtaCompressed(in.b);Monstey_Check(r.status=="end_marker"&&r.decoded==std::vector<std::uint8_t>(length+1,'A')&&r.span_bytes==length);++Monstey_cases;}
 {Monstey_Bits in;std::vector<std::uint8_t>expected;for(unsigned i=0;i<4094;++i){const auto c=static_cast<std::uint8_t>(i);in.Literal(c);expected.push_back(c);}in.Literal('x');expected.push_back('x');in.Literal('y');expected.push_back('y');in.Span(4095,2);in.End();expected.push_back('x');expected.push_back('y');const auto r=Monstey::Monstey_ObserveLtaCompressed(in.b);Monstey_Check(r.status=="end_marker"&&r.decoded==expected);++Monstey_cases;}
 {Monstey_Bits in;in.Span(1024,3);in.End();const auto r=Monstey::Monstey_ObserveLtaCompressed(in.b);Monstey_Check(r.status=="unavailable_initial_window"&&r.unavailable_window_index==1024&&r.decoded.empty());++Monstey_cases;}
 {Monstey_Bits in;in.Literal('A');in.Span(1,17);in.End();const auto r=Monstey::Monstey_ObserveLtaCompressed(in.b,7);Monstey_Check(r.status=="output_budget_exhausted"&&r.decoded.size()==7&&r.span_bytes==6);++Monstey_cases;}
 for(unsigned n=0;n<4;++n){const std::vector<std::uint8_t>b(n);Monstey_Check(Monstey::Monstey_ObserveLtaCompressed(b).status=="truncated_header");++Monstey_cases;}
 {Monstey_Bits in;in.b[0]=1;Monstey_Check(Monstey::Monstey_ObserveLtaCompressed(in.b).status=="unsupported_nonzero_header");++Monstey_cases;}
 {Monstey_Bits in;Monstey_Check(Monstey::Monstey_ObserveLtaCompressed(in.b).status=="missing_end_marker");++Monstey_cases;}
 std::ofstream o(argv[1]);o<<"{\"status\":\"PASS\",\"groups\":5,\"cases\":"<<Monstey_cases<<",\"checks\":"<<Monstey_checks<<",\"original_files_required\":false}\n";std::cout<<"PASS "<<Monstey_cases<<" cases "<<Monstey_checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
