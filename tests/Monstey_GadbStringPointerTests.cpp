#include "Monstey_GadbStringPointer.hpp"
#include "Monstey_GadbScalarFloat.hpp"
#include "Monstey_Core.hpp"
#include "Monstey_Json.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <vector>
using namespace Monstey;
namespace {
std::uint64_t checks=0,cases=0;
void Need(bool b,const char* s){++checks;Monstey_Require(b,s);}
template<class T>void Put(std::vector<std::uint8_t>&b,std::size_t p,T v){std::memcpy(b.data()+p,&v,sizeof v);}
struct Arena{
 std::uint64_t base=0x100000000;std::vector<std::uint8_t>b=std::vector<std::uint8_t>(4096,0xa5);
 std::uint64_t A(std::size_t p)const{return base+p;}Monstey_GadbBinderMemory M(){return{base,b};}
 Arena(){Put(b,128,std::uint8_t{1});Put(b,129,std::uint8_t{1});Put(b,136,A(256));Put(b,144,A(1024));Put(b,152,A(384));Put(b,256,0x22222222u);Put(b,266,std::uint8_t{4});Put(b,268,std::uint16_t{2});Put(b,270,std::uint16_t{0});Put(b,64,0x22222222u);Put(b,416,A(512));Put(b,512,A(640));Put(b,520,2u);Put(b,672,0x11111111u);Put(b,676,4u);Put(b,680,1u);Put(b,684,0u);Put(b,744,0x22222222u);Put(b,748,4u);Put(b,752,2u);Put(b,756,0u);}
};
void Raw(){for(unsigned form=0;form<2;++form){Arena a;Put(a.b,129,static_cast<std::uint8_t>(form));if(!form){Put(a.b,260,0x22222222u);Put(a.b,265,std::uint8_t{0});}std::uint64_t i=0;for(auto word:{0ull,0xffffffffffffffffull,0x7ff8000000000001ull,0x0000000141e733c0ull,0x8000000000000000ull}){Put(a.b,1024,word);Put(a.b,1032,word^0x1122334455667788ull);auto before=a.b;Need(Monstey_GadbReadStringPointer(a.M(),a.A(128),a.A(64),0,123)==word,"Raw pointer bits/zero must be returned unchanged");Need(Monstey_GadbReadStringPointer(a.M(),a.A(128),a.A(64),1,123)==(word^0x1122334455667788ull),"Raw eight-byte value stride wrong");Need(a.b==before,"Getter wrote raw source bytes");++cases;++i;}Need(i==5,"Raw fixture cases missing");}}
void Default(){Arena a;const auto invalid=std::uint64_t{0xfffffffffffffff0};Need(Monstey_GadbReadStringPointer({a.base,{}},0,invalid,0,invalid)==invalid,"Null record consumed data or default address");++cases;
 for(unsigned type=0;type<256;++type){Put(a.b,266,static_cast<std::uint8_t>(type));Put(a.b,1024,0x123456789abcdef0ull);auto result=Monstey_GadbReadStringPointer(a.M(),a.A(128),a.A(64),0,99);Need(result==((type&63)==4?0x123456789abcdef0ull:99ull),"Type must be low six bits of descriptor byte10");++cases;}
 Put(a.b,266,std::uint8_t{4});for(auto index:{2u,65535u,0x80000000u,0xffffffffu}){Need(Monstey_GadbReadStringPointer(a.M(),a.A(128),a.A(64),index,0xdead)==0xdead,"Unsigned U16-count index fallback wrong");++cases;}
 Put(a.b,128,std::uint8_t{0});Need(Monstey_GadbReadStringPointer(a.M(),a.A(128),0,0,17)==17,"Empty descriptor record consumed field");++cases;}
void Generated(){Arena a;Put(a.b,129,std::uint8_t{0x11});Put(a.b,1032,0xffff0123456789abull);Put(a.b,1040,0x7ff0000000000001ull);auto old=a.b;Need(Monstey_GadbReadStringPointer(a.M(),a.A(128),a.A(64),0,0)==0xffff0123456789abull,"Generated fixed pointer word wrong");Need(Monstey_GadbReadStringPointer(a.M(),a.A(128),a.A(64),1,0)==0x7ff0000000000001ull,"Generated fixed array index/stride wrong");Need(a.b==old,"Generated getter mutated memory");cases+=2;
 Put(a.b,752,0xffffffffu);Put(a.b,1032,a.A(2048));Put(a.b,1040,2u);Put(a.b,2048,0x8888888888888888ull);Put(a.b,2056,0x9999999999999999ull);Need(Monstey_GadbReadStringPointer(a.M(),a.A(128),a.A(64),0,0)==0x8888888888888888ull,"Generated dynamic first pointer word wrong");Need(Monstey_GadbReadStringPointer(a.M(),a.A(128),a.A(64),1,0)==0x9999999999999999ull,"Generated dynamic second pointer word wrong");cases+=2;
 Put(a.b,1040,1u);bool hit=false;try{(void)Monstey_GadbReadStringPointer(a.M(),a.A(128),a.A(64),1,0);}catch(const Monstey_GadbScalarFault&e){hit=e.kind==Monstey_GadbScalarFaultKind::NativeDynamicIndexAssertion;}Need(hit,"Generated native dynamic bound assertion disappeared");++cases;
 for(unsigned absent=0;absent<3;++absent){Arena z;Put(z.b,129,std::uint8_t{0x11});if(absent==0)Put(z.b,152,std::uint64_t{0});else if(absent==1)Put(z.b,416,std::uint64_t{0});else Put(z.b,520,0u);Need(Monstey_GadbReadStringPointer(z.M(),z.A(128),z.A(64),0,0xabcdef)==0xabcdef,"Generated missing owner/class/field failed fallback");++cases;}}
void Faults(){Arena a;const auto fault=[&](Monstey_GadbBinderMemory m,std::uint64_t field,std::uint64_t at){bool caught=false;try{(void)Monstey_GadbReadStringPointer(m,a.A(128),field,0,123);}catch(const Monstey_GadbBinderFault&e){caught=e.address==at&&e.kind==Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable;}Need(caught,"String getter consumed-range fault/order wrong");++cases;};
 fault({a.base,std::span(a.b).first(266)},a.A(64),a.A(266));Put(a.b,266,std::uint8_t{2});Need(Monstey_GadbReadStringPointer({a.base,std::span(a.b).first(267)},a.A(128),a.A(64),0,55)==55,"Wrong type eagerly consumed count");++cases;Put(a.b,266,std::uint8_t{4});fault({a.base,std::span(a.b).first(268)},a.A(64),a.A(268));fault({a.base,std::span(a.b).first(1024)},a.A(64),a.A(1024));
 Put(a.b,129,std::uint8_t{0x11});Put(a.b,152,std::uint64_t{0});Need(Monstey_GadbReadStringPointer({a.base,std::span(a.b).first(272)},a.A(128),a.A(64),0,0xbad)==0xbad,"Missing generated owner consumed unavailable class");++cases;}
}
int wmain(int argc,wchar_t**argv){try{Monstey_Require(argc==2,"Expected NEW_REPORT");Raw();Default();Generated();Faults();auto r=Monstey_Json::object();r["status"]="PASS";r["groups"]=4;r["cases"]=static_cast<std::int64_t>(cases);r["checks"]=static_cast<std::int64_t>(checks);r["original_calls"]=false;Monstey_WriteNewText(argv[1],r.dump(2)+"\n");std::cout<<"PASS4groups "<<cases<<" cases "<<checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
