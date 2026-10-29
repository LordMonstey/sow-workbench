#include "Monstey_FearUnpackedValidate.hpp"
#include <bit>
#include <string>
namespace Monstey {
Monstey_FearUnpackedValidateBoundaryFault::Monstey_FearUnpackedValidateBoundaryFault(const char*name,std::uint32_t site,std::uint32_t targetValue):std::runtime_error(std::string("Consumed unpacked numeric validation boundary: ")+name),native_site(site),target(targetValue){}
namespace {
using U=std::uint32_t;using U64=std::uint64_t;using Mem=Monstey_FearDbMutableMemory;using S=Monstey_FearUnpackedValidateServices;using Phase=Monstey_FearValidateX87Phase;
std::size_t Monstey_ValidateOffset(Mem m,U at,bool writing){if(at<m.base||U64(at)+4u>0x100000000ull||U64(at)-m.base>m.bytes.size()||4u>m.bytes.size()-static_cast<std::size_t>(U64(at)-m.base))throw Monstey_FearDbFault(at,4,writing);return at-m.base;}
U Monstey_ValidateRead(Mem m,U at){const auto p=Monstey_ValidateOffset(m,at,false);U v=0;for(unsigned n=0;n<4;++n)v|=U(m.bytes[p+n])<<(n*8);return v;}
void Monstey_ValidateWrite(Mem m,U at,U value){const auto p=Monstey_ValidateOffset(m,at,true);for(unsigned n=0;n<4;++n)m.bytes[p+n]=static_cast<std::uint8_t>(value>>(n*8));}
U Monstey_ValidateCstr(Mem&m,U string,U site,const S&s){const U target=Monstey_ValidateRead(m,0x1000B070u);if(!s.c_str)throw Monstey_FearUnpackedValidateBoundaryFault("c_str",site,target);return s.c_str(m,{site,target,string});}
std::uint16_t Monstey_ValidateCompare(Mem&m,U address,U load,U compare,U status,Phase phase,U64 value,const S&s){const U bound=Monstey_ValidateRead(m,address);if(!s.compare_status)throw Monstey_FearUnpackedValidateBoundaryFault("x87_compare_status",compare);return s.compare_status(m,{phase,load,compare,status,address,value,bound});}
U64 Monstey_ValidateF32ToF64(U x){const U64 sign=U64(x&0x80000000u)<<32;const U exponent=(x>>23)&255u,fraction=x&0x7FFFFFu;if(exponent==255)return sign|0x7FF0000000000000ull|(U64(fraction)<<29);if(exponent)return sign|(U64(exponent+896u)<<52)|(U64(fraction)<<29);if(!fraction)return sign;const unsigned top=31u-std::countl_zero(fraction);return sign|(U64(top+874u)<<52)|(U64(fraction^(1u<<top))<<(52u-top));}
bool Monstey_ValidateNaN(U64 x){return (x&0x7FF0000000000000ull)==0x7FF0000000000000ull&&(x&0x000FFFFFFFFFFFFFull)!=0;}
}
std::uint8_t Monstey_FearUnpackedValidate(Mem&m,U constraint,U string,U frame,const S&s){
 const U type=Monstey_ValidateRead(m,constraint);
 if(type==3){const U source=Monstey_ValidateCstr(m,string,0x10003F2Au,s);Monstey_ValidateWrite(m,frame-4u,0);Monstey_ValidateWrite(m,frame-8u,0);Monstey_ValidateWrite(m,frame-12u,source);
  const U target=Monstey_ValidateRead(m,0x1000B0C0u);if(!s.strtol)throw Monstey_FearUnpackedValidateBoundaryFault("strtol",0x10003F35u,target);
  const U parsed=s.strtol(m,{0x10003F35u,target,Monstey_ValidateRead(m,frame-12u),Monstey_ValidateRead(m,frame-8u),Monstey_ValidateRead(m,frame-4u)});
  const U lower=Monstey_ValidateRead(m,constraint+4u);if(std::bit_cast<std::int32_t>(parsed)<std::bit_cast<std::int32_t>(lower))return 0;
  const U upper=Monstey_ValidateRead(m,constraint+8u);return std::bit_cast<std::int32_t>(parsed)<=std::bit_cast<std::int32_t>(upper)?1u:0u;
 }
 if(type!=2)return 1;
 const U source=Monstey_ValidateCstr(m,string,0x10003F4Du,s);Monstey_ValidateWrite(m,frame-4u,source);const U target=Monstey_ValidateRead(m,0x1000B0BCu);if(!s.atof_binary64)throw Monstey_FearUnpackedValidateBoundaryFault("atof_binary64",0x10003F54u,target);
 const U64 value=s.atof_binary64(m,{0x10003F54u,target,Monstey_ValidateRead(m,frame-4u)});
 const auto lower=Monstey_ValidateCompare(m,constraint+4u,0x10003F5Au,0x10003F5Fu,0x10003F64u,Phase::lower_duplicate_compare,value,s);
 const unsigned masked=(lower>>8)&5u;
 if((std::popcount(masked)&1)!=0){if(!s.retire_primary)throw Monstey_FearUnpackedValidateBoundaryFault("fstp_st0",0x10003F7Cu);s.retire_primary(m,0x10003F7Cu,value);return 0;}
 const auto upper=Monstey_ValidateCompare(m,constraint+8u,0x10003F6Bu,0x10003F70u,0x10003F72u,Phase::upper_final_compare,value,s);
 return ((upper>>8)&0x41u)!=0?1u:0u;
}
std::uint32_t Monstey_FearUnpackedValidateStringData(Mem&m,U string){const U capacity=Monstey_ValidateRead(m,string+0x18u);return capacity>=16?Monstey_ValidateRead(m,string+4u):string+4u;}
std::uint16_t Monstey_FearUnpackedValidateMaskedCompare(U64 value,U bound){const U64 other=Monstey_ValidateF32ToF64(bound);if(Monstey_ValidateNaN(value)||Monstey_ValidateNaN(other))return 0x4501;
 const U64 vm=value&0x7FFFFFFFFFFFFFFFull,om=other&0x7FFFFFFFFFFFFFFFull;if((vm==0&&om==0)||value==other)return 0x4000;
 const bool negative=(value>>63)!=0,otherNegative=(other>>63)!=0;const bool less=negative!=otherNegative?negative:(negative?vm>om:vm<om);return less?0x0100u:0u;
}
}
