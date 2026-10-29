#include "Monstey_FearUnpackedValidate.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>
namespace {
using U=std::uint32_t;using U64=std::uint64_t;using Mem=Monstey::Monstey_FearDbMutableMemory;using Phase=Monstey::Monstey_FearValidateX87Phase;
std::uint64_t groups=0,cases=0,checks=0,status_cases=0,profile_cases=0;
void Need(bool p,const char*s){++checks;if(!p)throw std::runtime_error(s);}
struct Abort{};
struct Event{unsigned kind;U site,target,object,a,b,c;U64 bits;};
struct Arena {
 static constexpr U base=0x10000000u;U constraint=base+0x1000,string=base+0x2000,frame=base+0x3000;
 std::vector<std::uint8_t>bytes=std::vector<std::uint8_t>(0x10000,0xCC),changed;Mem memory{base,bytes};
 Monstey::Monstey_FearUnpackedValidateServices services;U int_bits=0;U64 float_bits=0;U lower_status=0,upper_status=0;bool forced_status=false,primary_live=false,log=true;unsigned mode=0,lower_calls=0,upper_calls=0,pop_calls=0,parse_calls=0,cstr_calls=0;std::vector<Event>events;
 Arena(U type=2){W(constraint,type);W(constraint+4,0xBF800000);W(constraint+8,0x3F800000);W(string+0x18,15);W(string+4,0x00312E31);W(0x1000B070,0xE000B070);W(0x1000B0BC,0xE000B0BC);W(0x1000B0C0,0xE000B0C0);Bind();}
 U R(U p)const{if(p<memory.base||U64(p)+4>0x100000000ull||U64(p)-memory.base>memory.bytes.size()||4>memory.bytes.size()-static_cast<std::size_t>(U64(p)-memory.base))throw Monstey::Monstey_FearDbFault(p,4,false);U v=0;for(unsigned k=4;k;--k)v=v*256+memory.bytes[p-memory.base+k-1];return v;}
 void W(U p,U v){if(p<memory.base||U64(p)+4>0x100000000ull||U64(p)-memory.base>memory.bytes.size()||4>memory.bytes.size()-static_cast<std::size_t>(U64(p)-memory.base))throw Monstey::Monstey_FearDbFault(p,4,true);for(unsigned k=0;k<4;++k){memory.bytes[p-memory.base+k]=static_cast<std::uint8_t>(v);v>>=8;}}
 void Record(Event e){if(log)events.push_back(e);}
 void Bind(){
  services.c_str=[this,n=0u](Mem&m,auto c)mutable{Need(&m==&memory,"c_str receives actual mutable view");++cstr_calls;Record({1,c.native_site,c.target,c.string_object,++n,0,0,0});const U source=Monstey::Monstey_FearUnpackedValidateStringData(m,c.string_object);if(mode==1){W(0x1000B0C0,0xA0C01234);W(0x1000B0BC,0xA0BC1234);}if(mode==8)throw Abort{};return source;};
  services.strtol=[this,n=0u](Mem&m,auto c)mutable{Need(&m==&memory,"strtol receives actual mutable view");Need(c.source==R(frame-12)&&c.end_pointer==R(frame-8)&&c.base==R(frame-4),"strtol receives actual ordered outgoing cells");++parse_calls;Record({2,c.native_site,c.target,c.source,c.end_pointer,c.base,++n,0});
   if(mode==2){W(constraint+4,0xFFFFFFFEu);W(constraint+8,2);}if(mode==3){changed=bytes;m={base,std::span(changed).first(constraint-base+8)};}if(mode==9)throw Abort{};return int_bits;};
  services.atof_binary64=[this,n=0u](Mem&m,auto c)mutable{Need(&m==&memory,"atof receives actual mutable view");++parse_calls;primary_live=true;Record({3,c.native_site,c.target,c.source,++n,0,0,float_bits});if(mode==2){W(constraint+4,0xFF800000);W(constraint+8,0x7F800000);}if(mode==4){changed=bytes;m={base,changed};W(constraint+4,0xFF800000);}if(mode==9)throw Abort{};return float_bits;};
  services.compare_status=[this,n=0u](Mem&m,auto c)mutable{Need(&m==&memory,"compare receives actual view");Need(primary_live,"primary value remains live until its declared retirement");Need(c.load_site==(c.phase==Phase::lower_duplicate_compare?0x10003F5Au:0x10003F6Bu),"exact x87 load instruction identity");Need(c.value_binary64_bits==float_bits,"original binary64 return operand is preserved");Record({4,c.compare_site,c.status_site,c.bound_address,c.bound_binary32_bits,static_cast<U>(c.phase),++n,c.value_binary64_bits});
   if(c.phase==Phase::lower_duplicate_compare){++lower_calls;if(mode==5)W(constraint+8,0x7F800000);if(mode==10)throw Abort{};return static_cast<std::uint16_t>(forced_status?lower_status:Monstey::Monstey_FearUnpackedValidateMaskedCompare(c.value_binary64_bits,c.bound_binary32_bits));}
   ++upper_calls;primary_live=false;if(mode==11)throw Abort{};return static_cast<std::uint16_t>(forced_status?upper_status:Monstey::Monstey_FearUnpackedValidateMaskedCompare(c.value_binary64_bits,c.bound_binary32_bits));};
  services.retire_primary=[this](Mem&m,U site,U64 bits){Need(&m==&memory&&site==0x10003F7C&&bits==float_bits&&primary_live,"early FSTP retirement tuple");Record({5,site,0,0,0,0,0,bits});++pop_calls;primary_live=false;if(mode==12)throw Abort{};};
 }
 std::uint8_t Run(){return Monstey::Monstey_FearUnpackedValidate(memory,constraint,string,frame,services);}
 void Reset(){events.clear();lower_calls=upper_calls=pop_calls=parse_calls=cstr_calls=0;primary_live=false;}
};
std::int64_t Signed(U x){return x&0x80000000u?std::int64_t(x)-0x100000000ll:std::int64_t(x);}
void Integers(){++groups;const std::array<U,8> values{0,1,2,UINT32_MAX,0xFFFFFFFEu,0x80000000u,0x80000001u,0x7FFFFFFFu};Arena a(3);a.log=false;
 for(U value:values)for(U lower:values)for(U upper:values){a.Reset();a.int_bits=value;a.W(a.constraint+4,lower);a.W(a.constraint+8,upper);const bool expected=Signed(value)>=Signed(lower)&&Signed(value)<=Signed(upper);Need(a.Run()==(expected?1:0),"native signed32 range predicate");Need(a.parse_calls==1&&a.cstr_calls==1&&a.lower_calls==0&&a.pop_calls==0,"integer consumes only c_str/strtol");++cases;}
 {Arena b(3);b.mode=2;b.int_bits=0;b.W(b.constraint+4,10);b.W(b.constraint+8,20);Need(b.Run()==1,"integer bounds read after parser effects");Need(b.events[0].site==0x10003F2A&&b.events[1].site==0x10003F35&&b.events[1].object==b.string+4&&b.events[1].a==0&&b.events[1].b==0,"actual source/end0/base0 and sites");++cases;}
 for(bool lowerFails:{true,false}){Arena b(3);b.mode=3;b.int_bits=lowerFails?0u:2u;b.W(b.constraint+4,1);b.W(b.constraint+8,3);bool fault=false;U result=9;try{result=b.Run();}catch(const Monstey::Monstey_FearDbFault&e){fault=e.address==b.constraint+8&&e.width==4&&!e.writing;}Need(lowerFails?(!fault&&result==0):fault,"upper bound consumed only after signed lower success");Need(b.memory.bytes.data()==b.changed.data(),"parser view replacement retained");++cases;}
 for(U type:std::array<U,8>{0,1,4,5,9,UINT32_MAX,0x80000000u,0xFFFFFFFEu}){Arena b(type);b.services={};b.frame=0;b.memory.bytes=b.memory.bytes.first(b.constraint-Arena::base+4);Need(b.Run()==1&&b.events.empty(),"other type bypasses all services and frame");++cases;}
}
void StatusMasks(){++groups;Arena a(2);a.log=false;a.forced_status=true;
 for(U lower=0;lower<256;++lower)for(U upper=0;upper<256;++upper){a.Reset();a.lower_status=(lower<<8)|0xA5;a.upper_status=(upper<<8)|0x5A;const bool pass=((lower&1u)!=0)==((lower&4u)!=0);const U expected=pass&&((upper&0x41u)!=0)?1u:0u;Need(a.Run()==expected,"exact AH05 parity and AH41 zero masks");Need(a.lower_calls==1&&a.upper_calls==(pass?1u:0u)&&a.pop_calls==(pass?0u:1u)&&!a.primary_live,"exact primary retain/retire path");++cases;++status_cases;}
}
std::uint16_t HostStatus(U64 value,U bound){static_assert(sizeof(double)==8&&sizeof(float)==4&&std::numeric_limits<double>::is_iec559&&std::numeric_limits<float>::is_iec559);const double v=std::bit_cast<double>(value),b=static_cast<double>(std::bit_cast<float>(bound));if(std::isnan(v)||std::isnan(b))return 0x4501;if(v<b)return 0x100;if(v==b)return 0x4000;return 0;}
void MaskedProfile(){++groups;
 const std::array<U64,19> values{0,0x8000000000000000ull,1,0x8000000000000001ull,0x000FFFFFFFFFFFFFull,0x0010000000000000ull,0x3FF0000000000000ull,0xBFF0000000000000ull,0x3FF0000000000001ull,0x3FEFFFFFFFFFFFFFull,0x7FEFFFFFFFFFFFFFull,0xFFEFFFFFFFFFFFFFull,0x7FF0000000000000ull,0xFFF0000000000000ull,0x7FF8000000000001ull,0x7FF0000000000001ull,0xFFF8000000000001ull,0x36A0000000000000ull,0x36A0000000000001ull};
 const std::array<U,12> bounds{0,0x80000000u,0x3F800000u,0xBF800000u,0x7F800000u,0xFF800000u,0x7FC00000u,0x7F800001u,1,0x007FFFFFu,0x00800000u,0x7F7FFFFFu};
 for(U64 value:values)for(U bound:bounds){Need(Monstey::Monstey_FearUnpackedValidateMaskedCompare(value,bound)==HostStatus(value,bound),"integer-bit masked compare equals declared IEEE64/F32 profile");++cases;++profile_cases;}
 Arena a(2);a.log=false;
 for(U64 value:values)for(U lower:bounds)for(U upper:bounds){a.Reset();a.float_bits=value;a.W(a.constraint+4,lower);a.W(a.constraint+8,upper);const auto lo=HostStatus(value,lower),hi=HostStatus(value,upper);const bool lowPass=(lo&0x500u)==0||(lo&0x500u)==0x500u;const U expected=lowPass&&(hi&0x4100u)?1u:0u;Need(a.Run()==expected&&!a.primary_live,"masked profile whole-validator including NaN/unordered and signed zero");++cases;++profile_cases;}
 // Returning FSTP32 instead of the proven QWORD source would incorrectly accept.
 {Arena b(2);b.float_bits=0x3FF0000000000001ull;b.W(b.constraint+4,0);b.W(b.constraint+8,0x3F800000);Need(b.Run()==0,"value above F32 upper remains binary64 before comparison");++cases;}
}
void EpochsAndAliases(){++groups;
 {Arena a(2);a.float_bits=0x4000000000000000ull;a.mode=2;Need(a.Run()==1,"float bounds read after atof effects");Need(a.events[0].site==0x10003F4D&&a.events[1].site==0x10003F54,"float native callsites");Need(a.events[2].site==0x10003F5F&&a.events[2].target==0x10003F64&&a.events[2].object==a.constraint+4&&a.events[3].site==0x10003F70&&a.events[3].target==0x10003F72,"both comparison/status epochs and bound addresses");++cases;}
 {Arena a(2);a.float_bits=0x4000000000000000ull;a.mode=5;Need(a.Run()==1,"upper bound read after lower effect");Need(a.events.back().a==0x7F800000,"upper service receives newly read bits");++cases;}
 for(U type:{2u,3u}){Arena a(type);a.mode=1;Need(a.Run()<=1,"c_str target mutation run");Need(a.events[1].target==(type==2?0xA0BC1234u:0xA0C01234u),"parser target selected after c_str effects");++cases;}
 {Arena a(2);a.frame=0x1000B0C0;Need(a.Run()<=1,"atof slot/push alias");Need(a.events[1].target==a.string+4,"atof target sampled after source push overwrites IAT");++cases;}
 {Arena a(3);a.frame=0x1000B0C4;a.int_bits=0;a.W(a.constraint+4,0);a.W(a.constraint+8,0);Need(a.Run()==1,"strtol slot/push alias");Need(a.events[1].target==0&&a.events[1].a==0&&a.events[1].b==0,"strtol target follows base/end pushes");++cases;}
 {Arena a(3);a.constraint=a.frame-12;a.W(a.constraint,3);a.W(a.constraint+4,1);a.W(a.constraint+8,2);a.int_bits=0;Need(a.Run()==1&&a.R(a.constraint+4)==0&&a.R(a.constraint+8)==0,"known pushes overwrite aliased integer bounds before read");++cases;}
 {Arena a(2);a.constraint=a.frame-8;a.W(a.constraint,2);a.W(a.constraint+4,0xBF800000);a.W(a.constraint+8,0x3F800000);a.forced_status=true;a.lower_status=0;a.upper_status=0x4000;Need(a.Run()==1&&a.events[2].a==a.string+4,"float source push can overwrite lower bound");++cases;}
 {Arena a(2);a.mode=4;a.float_bits=0;Need(a.Run()==1&&a.memory.bytes.data()==a.changed.data(),"atof rebind applies to bound reads");++cases;}
 for(U capacity:std::array<U,5>{0,15,16,UINT32_MAX,0x80000000u}){Arena a;a.W(a.string+0x18,capacity);a.W(a.string+4,0xDEADBEEF);Need(Monstey::Monstey_FearUnpackedValidateStringData(a.memory,a.string)==(capacity<16?a.string+4:0xDEADBEEFu),"exact unsigned raw28 c_str data selection");++cases;}
}
struct Action {Arena*arena;U address,value;~Action(){arena->W(address,value);}};
void TargetLifetime(){++groups;
 {Arena a(3);auto life=std::make_shared<Action>();life->arena=&a;life->address=a.constraint+4;life->value=0;a.W(a.constraint+4,1);a.W(a.constraint+8,2);a.int_bits=0;auto*owner=&a;
  a.services.strtol=[owner,life](Mem&,auto)->U{auto*target=owner;target->services.strtol=[](Mem&,auto){return 0u;};return 0;};life.reset();Need(a.Run()==1,"retired strtol target effects precede lower-bound read");++cases;}
 {Arena a(2);auto life=std::make_shared<Action>();life->arena=&a;life->address=a.constraint+4;life->value=0;a.W(a.constraint+4,0x3F800000);a.float_bits=0;auto*owner=&a;
  a.services.atof_binary64=[owner,life](Mem&,auto)->U64{auto*target=owner;target->primary_live=true;target->services.atof_binary64=[](Mem&,auto){return U64(0);};return 0;};life.reset();Need(a.Run()==1,"retired atof target effects precede FLD lower bound");++cases;}
 {Arena a(2);a.float_bits=0x4000000000000000ull;auto original=a.services.compare_status;auto life=std::make_shared<Action>();life->arena=&a;life->address=a.constraint+8;life->value=0x7F800000;auto*owner=&a;
  a.services.compare_status=[owner,original,life](Mem&m,auto c){auto*target=owner;auto next=original;target->services.compare_status=next;return next(m,c);};life.reset();Need(a.Run()==1,"retired lower compare target effects precede upper bound read");++cases;}
 {Arena a(2);a.float_bits=0xC000000000000000ull;auto token=std::make_shared<int>(3);std::weak_ptr<int> weak=token;bool alive=false;auto*owner=&a;
  a.services.retire_primary=[owner,token,&weak,&alive](Mem&,U site,U64 bits){auto*target=owner;auto observer=weak;auto*flag=&alive;Need(site==0x10003F7C&&bits==target->float_bits,"retire exact native tuple");target->primary_live=false;target->services.retire_primary={};*flag=!observer.expired();};token.reset();Need(a.Run()==0&&alive&&weak.expired()&&!a.primary_live,"FSTP target self-clear survives active invocation then retires");++cases;}
 {Arena a(3);std::vector<U>seen;a.services.strtol=[&seen,n=0u](Mem&,auto)mutable{seen.push_back(++n);return 0u;};a.W(a.constraint+4,0);a.W(a.constraint+8,0);Need(a.Run()==1&&a.Run()==1&&seen==std::vector<U>({1,2}),"mutable value-captured parse state persists");++cases;}
}
void Failures(){++groups;
 for(unsigned mode:{8u,9u,10u,11u,12u}){Arena a(mode==8?3u:2u);a.mode=mode;if(mode==12)a.float_bits=0xC000000000000000ull;bool caught=false;try{(void)a.Run();}catch(const Abort&){caught=true;}Need(caught,"supplied primitive exception propagates");Need(mode!=9||a.R(a.frame-4)==a.string+4,"parse exception retains outgoing source");Need(mode!=10||a.upper_calls==0,"lower failure exception consumes no upper group");++cases;}
 {Arena a(3);a.services.c_str={};bool fault=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearUnpackedValidateBoundaryFault&e){fault=e.native_site==0x10003F2A&&e.target==0xE000B070;}Need(fault&&a.R(a.frame-4)==0xCCCCCCCC,"missing c_str before any argument push");++cases;}
 {Arena a(3);a.services.strtol={};bool fault=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearUnpackedValidateBoundaryFault&e){fault=e.native_site==0x10003F35&&e.target==0xE000B0C0;}Need(fault&&a.R(a.frame-4)==0&&a.R(a.frame-8)==0&&a.R(a.frame-12)==a.string+4,"missing strtol after actual push prefix");++cases;}
 {Arena a(2);a.services.atof_binary64={};bool fault=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearUnpackedValidateBoundaryFault&e){fault=e.native_site==0x10003F54;}Need(fault&&a.R(a.frame-4)==a.string+4,"missing atof after source push");++cases;}
 {Arena a(2);a.services.compare_status={};bool fault=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearUnpackedValidateBoundaryFault&e){fault=e.native_site==0x10003F5F;}Need(fault&&a.primary_live&&a.pop_calls==0,"missing compare retains supplied primary state");++cases;}
 {Arena a(2);a.float_bits=0xC000000000000000ull;a.services.retire_primary={};bool fault=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearUnpackedValidateBoundaryFault&e){fault=e.native_site==0x10003F7C;}Need(fault&&a.primary_live&&a.upper_calls==0,"missing early retire is consumed,not silently dropped");++cases;}
 {Arena a(2);a.float_bits=0x4000000000000000ull;a.forced_status=true;a.lower_status=0;auto original=a.services.compare_status;a.services.compare_status=[&a,original](Mem&m,auto c){const auto r=original(m,c);if(c.phase==Phase::lower_duplicate_compare)m.bytes=m.bytes.first(a.constraint-Arena::base+8);return r;};bool fault=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearDbFault&e){fault=e.address==a.constraint+8&&!e.writing&&e.width==4;}Need(fault&&a.primary_live&&a.pop_calls==0,"late upper read fault adds no invented primary cleanup");++cases;}
 for(bool lowerFails:{true,false}){Arena a(2);a.float_bits=lowerFails?0xC000000000000000ull:0;auto original=a.services.atof_binary64;a.services.atof_binary64=[&a,original](Mem&m,auto c){const auto bits=original(m,c);m.bytes=m.bytes.first(a.constraint-Arena::base+8);return bits;};bool fault=false;U result=9;try{result=a.Run();}catch(const Monstey::Monstey_FearDbFault&e){fault=e.address==a.constraint+8&&!e.writing;}Need(lowerFails?(!fault&&result==0&&a.pop_calls==1):(fault&&a.primary_live),"float upper operand unavailable only on its consumed branch");++cases;}
 {Arena a(3);a.frame=0x1000B0C4;a.memory.bytes=a.memory.bytes.first(0xB0C2);bool fault=false;try{(void)a.Run();}catch(const Monstey::Monstey_FearDbFault&e){fault=e.address==a.frame-4&&e.writing&&e.width==4;}Need(fault,"whole outgoing DWORD faults at its consumed store");++cases;}
}
}
int main(int argc,char**argv){try{if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("new report required");Integers();StatusMasks();MaskedProfile();EpochsAndAliases();TargetLifetime();Failures();std::ofstream out(argv[1],std::ios::binary);out<<"{\"status\":\"PASS\",\"groups\":"<<groups<<",\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"all_AH_pairs\":"<<status_cases<<",\"masked_profile_cases\":"<<profile_cases<<",\"native_original_entrypoint_invoked\":false,\"original_instruction_bytes_interpreted\":false,\"physical_CRT_x87_ABI_qualified\":false}\n";std::cout<<"PASS "<<cases<<" cases "<<checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const Abort&){std::cerr<<"unexpected primitive abort\n";return 1;}}
