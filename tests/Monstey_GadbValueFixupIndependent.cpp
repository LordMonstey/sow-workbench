#include "Monstey_GadbValueFixup.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
namespace {
using Monstey_U64=std::uint64_t;using Monstey_U32=std::uint32_t;
struct Monstey_IndependentFault {Monstey_U64 at;std::size_t width;bool write;};
struct Monstey_Arena {
 Monstey_U64 base=0;std::vector<std::uint8_t> data=std::vector<std::uint8_t>(0x2400);
 Monstey_U64 Read(Monstey_U64 a,std::size_t n)const {
  if(a<base||a-base>data.size()||n>data.size()-static_cast<std::size_t>(a-base))throw Monstey_IndependentFault{a,n,false};
  Monstey_U64 v=0;for(std::size_t i=n;i!=0;--i)v=(v<<8)|data[static_cast<std::size_t>(a-base)+i-1];return v;
 }
 void Write(Monstey_U64 a,Monstey_U64 v,std::size_t n) {
  if(a<base||a-base>data.size()||n>data.size()-static_cast<std::size_t>(a-base))throw Monstey_IndependentFault{a,n,true};
  for(std::size_t i=0;i<n;++i){data[static_cast<std::size_t>(a-base)+i]=static_cast<std::uint8_t>(v&255);v>>=8;}
 }
 void Put(std::size_t o,Monstey_U64 v,std::size_t n=8){Write(base+o,v,n);}
 Monstey::Monstey_GadbBinderMemory Memory(){return{base,data};}
 void Setup(unsigned type,unsigned count,unsigned first=0) {
  Put(0x10a,type,1);Put(0x10c,count,2);Put(0x10e,first,2);
  Put(0x218,base+0x280);Put(0x298,base+0x300);Put(0x348,3,4);Put(0x350,base+0x400);
  for(unsigned c=0;c<3;++c){Put(0x400+88*c+64,3,4);Put(0x400+88*c+72,base+0x1800+32*c);Put(0x400+88*c+40,4,4);Put(0x400+88*c+56,base+0x1900+40*c);}
  for(unsigned i=0;i<32;++i){Put(0x1000+8*i,base+0x800+i*8);Put(0x800+i*8,'A'+i%26,1);}
 }
};
struct Monstey_Stats {Monstey_U64 groups=0,cases=0,checks=0,faults=0;};
Monstey_Stats Monstey_Counts;
void Monstey_Require(bool b,const char* s){++Monstey_Counts.checks;if(!b)throw std::runtime_error(s);}
Monstey_U32 Monstey_Translation(std::uint8_t c) {
 if(c>='A'&&c<='Z')return c-'A'+1u;if(c>='a'&&c<='z')return c-'a'+1u;if(c>='0'&&c<='9')return c-'0'+27u;
 switch(c) {
 case ' ':return 37;case '!':return 58;case '"':return 51;case '#':return 60;case '$':return 61;
 case '%':return 62;case '&':return 64;case '\'':return 50;case '(':return 66;case ')':return 67;
 case '*':return 65;case '+':return 40;case ',':return 54;case '-':return 39;case '.':return 55;
 case '/':return 52;case ':':return 49;case ';':return 48;case '<':return 56;case '=':return 41;
 case '>':return 57;case '?':return 53;case '@':return 59;case '[':return 44;case '\\':return 42;
 case ']':return 45;case '^':return 63;case '_':return 38;case 0x60:return 68;case '{':return 46;
 case '|':return 43;case '}':return 47;case '~':return 69;default:return 0;
 }
}
Monstey_U32 Monstey_Polynomial(Monstey_Arena& a,Monstey_U64 p,Monstey_U32 seed) {
 std::vector<std::uint8_t> s;for(;;++p){const auto c=static_cast<std::uint8_t>(a.Read(p,1));if(!c)break;s.push_back(c);}
 Monstey_U32 sum=0,power=1;for(auto i=s.rbegin();i!=s.rend();++i){sum+=Monstey_Translation(*i)*power;power*=919u;}return sum+seed*power;
}
// Literal register/label semantics from the complete native CFG, independent of
// the candidate's structured resolver. Only owned bytes are read or changed.
Monstey_U64 Monstey_Reference(Monstey_Arena& m,Monstey_U64 d,Monstey_U64 record,Monstey_U64 words) {
 Monstey_U64 rax=0,rdx=record,rbp=0,r9=0;
 Monstey_U32 ecx=static_cast<Monstey_U32>(m.Read(d+10,1)),edi=0,r8d=0,r10d=0,ebp=0,r11d=0;
 rax=ecx&63u;if(rax==5)goto Monstey_Enum;if(rax<=9||rax>11)return rax;
 r8d=static_cast<Monstey_U32>(m.Read(d+14,2));ecx=(ecx&0xCAu)|10u;
 m.Write(d+10,ecx,1);rax=m.Read(rdx+24,8);rbp=m.Read(rax+24,8);if(m.Read(d+12,2)==0)return rax;r10d=0u-r8d;
Monstey_RefLoop:
 ecx=static_cast<Monstey_U32>(m.Read(words+Monstey_U64(r8d)*8,4));rdx=0;rax=ecx>>20;ecx&=0xFFFFFu;
 if(rax>=m.Read(rbp+72,4))goto Monstey_RefWrite;
 r9=m.Read(rbp+80,8)+rax*88;
 if(ecx<m.Read(r9+64,4)){rdx=Monstey_U64(ecx)*32+m.Read(r9+72,8);if(rdx!=0)goto Monstey_RefWrite;}
 ecx-=static_cast<Monstey_U32>(m.Read(r9+64,4));if(ecx>=m.Read(r9+40,4))goto Monstey_RefWrite;
 rax=m.Read(r9+56,8);rdx=rax+Monstey_U64(ecx)*40;
Monstey_RefWrite:
 m.Write(words+Monstey_U64(r8d)*8,rdx,8);++r8d;rax=m.Read(d+12,2);
 if(static_cast<Monstey_U32>(r10d+r8d)<rax)goto Monstey_RefLoop;return rax;
Monstey_Enum:
 ebp=static_cast<Monstey_U32>(m.Read(d+14,2));if(m.Read(d+12,2)==0)return rax;
Monstey_EnumLoop:
 r11d=edi+ebp;rax=m.Read(words+Monstey_U64(r11d)*8,8);r10d=Monstey_Polynomial(m,rax,0);++edi;
 m.Write(words+Monstey_U64(r11d)*8,r10d,8);rax=m.Read(d+12,2);if(edi<rax)goto Monstey_EnumLoop;return rax;
}
struct Monstey_Result {bool fault=false;Monstey_U64 value=0,address=0;std::size_t width=0;bool write=false;};
void Monstey_Compare(Monstey_Arena input,Monstey_U64 d,Monstey_U64 record,Monstey_U64 words) {
 ++Monstey_Counts.cases;auto expected=input,actual=input;Monstey_Result e,a;
 try{e.value=Monstey_Reference(expected,d,record,words);}catch(const Monstey_IndependentFault& f){e={true,0,f.at,f.width,f.write};}
 try{a.value=Monstey::Monstey_GadbFixupValueDescriptor(actual.Memory(),d,record,words);}
 catch(const Monstey::Monstey_GadbValueFixupFault& f){a={true,0,f.address,f.width,f.writing};}
 Monstey_Require(e.fault==a.fault,"fault disposition mismatch");
 Monstey_Require(!e.fault?e.value==a.value:e.address==a.address&&e.width==a.width&&e.write==a.write,"rawRAX/first consumed fault mismatch");
 Monstey_Require(expected.data==actual.data,"whole-memory/fault-prefix mismatch");if(e.fault)++Monstey_Counts.faults;
}
void Monstey_HashCases() {
 ++Monstey_Counts.groups;Monstey_Arena a;
 for(unsigned c=0;c<256;++c)for(auto seed:std::array<Monstey_U32,6>{0,1,919,0x7fffffff,0x80000000,0xffffffff}){
  a.Put(0x1000,c,1);a.Put(0x1001,0,1);++Monstey_Counts.cases;
  Monstey_Require(Monstey::Monstey_GadbHashCString(a.Memory(),0x1000,seed)==Monstey_Polynomial(a,0x1000,seed),"single-byte map/polynomial mismatch");
 }
 std::mt19937 rng(0x423E70);
 for(unsigned n=0;n<1024;++n){const auto len=n%97;for(unsigned i=0;i<len;++i)a.Put(0x1000+i,1+rng()%255,1);a.Put(0x1000+len,0,1);const auto seed=static_cast<Monstey_U32>(rng());++Monstey_Counts.cases;
  Monstey_Require(Monstey::Monstey_GadbHashCString(a.Memory(),0x1000,seed)==Monstey_Polynomial(a,0x1000,seed),"reverse-power hash mismatch");
 }
}
void Monstey_TypeAndBounds() {
 ++Monstey_Counts.groups;
 for(unsigned flags=0;flags<256;++flags){Monstey_Arena a;a.Setup(flags,0);Monstey_Compare(a,0x100,0x200,0x1000);}
 for(unsigned type:std::array<unsigned,8>{10,11,0x4a,0x4b,0x8a,0x8b,0xca,0xcb})
 for(unsigned cats:std::array<unsigned,4>{0,1,2,3})for(unsigned ci:std::array<unsigned,5>{0,1,2,3,4095})
 for(unsigned anon:std::array<unsigned,4>{0,1,3,0x100000})for(unsigned named:std::array<unsigned,3>{0,1,4})
 for(unsigned ordinal:std::array<unsigned,8>{0,1,2,3,4,6,7,0xfffff}){
  Monstey_Arena a;a.Setup(type,1);a.Put(0x348,cats,4);
  for(unsigned c=0;c<3;++c){a.Put(0x400+c*88+64,anon,4);a.Put(0x400+c*88+40,named,4);}
  a.Put(0x1000,0xfedcba9800000000ull|(ci<<20)|ordinal);Monstey_Compare(a,0x100,0x200,0x1000);
 }
}
void Monstey_RandomAndCold() {
 ++Monstey_Counts.groups;std::mt19937 rng(0x4BAF6C);
 for(unsigned trial=0;trial<2048;++trial){
  Monstey_Arena a;a.base=(trial&1)?0x1234567800000000ull:0;a.Setup((trial&1)?0xcb:10,1+rng()%12,trial%3);
  for(unsigned i=0;i<16;++i)a.Put(0x1000+8*i,Monstey_U64(rng())<<32|(rng()%4u<<20)|rng()%9u);
  Monstey_Compare(a,a.base+0x100,a.base+0x200,a.base+0x1000);
 }
 for(unsigned anon:std::array<unsigned,4>{1,2,3,0x100000})for(unsigned ordinal=0;ordinal<4;++ordinal){
  Monstey_Arena a;a.Setup(11,1);a.Put(0x440,anon,4);a.Put(0x448,Monstey_U64{0}-Monstey_U64(ordinal)*32);a.Put(0x428,0xffffffff,4);a.Put(0x1000,ordinal);Monstey_Compare(a,0x100,0x200,0x1000);
 }
}
void Monstey_AliasCases() {
 ++Monstey_Counts.groups;
 const std::array<std::size_t,14> points{0x100,0x102,0x104,0x108,0x10a,0x10c,0x10e,0x118,0x218,0x298,0x348,0x350,0x428,0x440};
 for(unsigned type:std::array<unsigned,3>{5,10,11})for(auto p:points)for(int delta=-7;delta<=7;++delta) {
  Monstey_Arena a;a.Setup(type,2);const auto words=static_cast<Monstey_U64>(static_cast<std::int64_t>(p)+delta);
  for(unsigned i=0;i<3;++i)a.Put(static_cast<std::size_t>(words)+8*i,type==5?0u:static_cast<Monstey_U64>(i));
  Monstey_Compare(a,0x100,0x200,words);
 }
 for(unsigned shift=0;shift<24;++shift){Monstey_Arena a;a.Setup(5,3);const auto words=0x1000+shift;for(unsigned i=0;i<3;++i)a.Put(words+8*i,words);Monstey_Compare(a,0x100,0x200,words);}
}
void Monstey_LiveStateCases() {
 ++Monstey_Counts.groups;
 // A store into descriptor+0C shortens or extends the already-entered loop.
 {Monstey_Arena a;a.Setup(10,2);a.Put(0x448,0x10000u-64u);a.Put(0x114,0xDEADBEEF);
  Monstey_Compare(a,0x100,0x200,0x10c);}
 {Monstey_Arena a;a.Setup(11,1);a.Put(0x448,Monstey_U64{3}-32);a.Put(0x114,0);a.Put(0x11c,1);
  Monstey_Compare(a,0x100,0x200,0x10c);}
 {Monstey_Arena a;a.Setup(5,1);a.Put(1,'b',1);a.Put(0x114,0x800);
  Monstey_Compare(a,0x100,0x200,0x10c);}
 {Monstey_Arena a;a.Setup(5,2);a.Put(0x114,0xBAD0);
  Monstey_Compare(a,0x100,0x200,0x10c);}
 // The first offset remains captured after a result store changes its bytes.
 {Monstey_Arena a;a.data.resize(0x12000);a.Setup(5,1,1);a.Put(0x10001,'b',1);a.Put(0x114,0x800);
  Monstey_Compare(a,0x100,0x200,0x104);}
 // Aliased DB count becomes zero, so the next word must not use old metadata.
 {Monstey_Arena a;a.Setup(10,2);a.Put(0x428,2000,4);a.Put(0x438,0);
  Monstey_Compare(a,0x100,0x200,0x348);}
 // The first result replaces the category table pointer consumed next iteration.
 {Monstey_Arena a;a.Setup(10,2);a.Put(0x440,0,4);a.Put(0x428,2048,4);
  a.Put(0x438,Monstey_U64{0x600}-Monstey_U64{0x400}*40);a.Put(0x358,1);
  a.Put(0x640,3,4);a.Put(0x648,0x1800);
  Monstey_Compare(a,0x100,0x200,0x350);}
 // A zero reference count still consumes both backlinks after normalizing type.
 {Monstey_Arena a;a.Setup(0xCB,0);a.Put(0x218,0x80000000);
  Monstey_Compare(a,0x100,0x200,0x1000);}
 // An earlier enum result changes bytes of a later string identity target.
 for(unsigned shift=0;shift<64;++shift){Monstey_Arena a;a.Setup(5,3);const auto words=0x1040u+shift;
  for(unsigned i=0;i<3;++i)a.Put(words+8*i,words);
  Monstey_Compare(a,0x100,0x200,words);}
}

void Monstey_EnumHighHashesAndHashFaults() {
 ++Monstey_Counts.groups;
 for(unsigned length=1;length<=64;++length) {
  Monstey_Arena a;a.Setup(5,2,1);
  for(unsigned j=0;j<length;++j){a.Put(0x800+j,'A'+j%26,1);a.Put(0x900+j,'z'-j%26,1);}
  a.Put(0x800+length,0,1);a.Put(0x900+length,0,1);
  a.Put(0x1008,0x800);a.Put(0x1010,0x900);Monstey_Compare(a,0x100,0x200,0x1000);
 }
 for(unsigned flags:std::array<unsigned,4>{5,0x45,0x85,0xC5})
 for(auto base:std::array<Monstey_U64,2>{0x100000000ull,0x1234567800000000ull}) {
  Monstey_Arena a;a.base=base;a.Setup(flags,2,3);
  a.Put(0x1018,base+0x800);a.Put(0x1020,base+0x808);
  Monstey_Compare(a,base+0x100,~Monstey_U64{0},base+0x1000);
 }

 for(unsigned scenario=0;scenario<2;++scenario) {
  Monstey_Arena a;a.data.resize(3);a.base=0x800000000ull;
  a.Put(0,'A',1);a.Put(1,'B',1);a.Put(2,'C',1);
  const auto p=scenario==0?0ull:a.base;
  Monstey_Result e,actual;
  try{e.value=Monstey_Polynomial(a,p,0xFEDCBA98);}
  catch(const Monstey_IndependentFault& f){e={true,0,f.at,f.width,f.write};}
  try{actual.value=Monstey::Monstey_GadbHashCString(a.Memory(),p,0xFEDCBA98);}
  catch(const Monstey::Monstey_GadbValueFixupFault& f){actual={true,0,f.address,f.width,f.writing};}
  ++Monstey_Counts.cases;++Monstey_Counts.faults;
  Monstey_Require(e.fault&&actual.fault&&e.address==actual.address&&e.width==actual.width&&!actual.write,
   "hash null identity/missing terminator consumed-fault mismatch");
 }
}

void Monstey_FaultCuts() {
 ++Monstey_Counts.groups;
 for(unsigned type:std::array<unsigned,4>{5,10,11,0xca}) {
  Monstey_Arena original;original.Setup(type,2);if(type!=5){original.Put(0x1000,0);original.Put(0x1008,0x100001);}
  for(std::size_t cut=0;cut<=0x1020;++cut){auto a=original;a.data.resize(cut);Monstey_Compare(a,0x100,0x200,0x1000);}
 }
}
void Monstey_HighIndexAndLoads() {
 ++Monstey_Counts.groups;
 for(unsigned type:std::array<unsigned,3>{5,10,11}){Monstey_Arena a;a.data.resize(0x83000);a.Setup(type,2,65535);a.Put(0x1000+65535ull*8,type==5?0x800u:0u);a.Put(0x1000+65536ull*8,type==5?0x808u:1u);Monstey_Compare(a,0x100,0x200,0x1000);}
 {Monstey_Arena a;a.Setup(11,1);a.Put(0x348,0,4);a.data.resize(0x1004);a.Put(0x1000,0xA5A5A5A5,4);Monstey_Compare(a,0x100,0x200,0x1000);}
 for(unsigned raw=0;raw<256;++raw)if((raw&63)!=5&&(raw&63)!=10&&(raw&63)!=11){Monstey_Arena a;a.data.resize(11);a.Put(10,raw,1);Monstey_Compare(a,0,~Monstey_U64{0},~Monstey_U64{0});}
}
}
int main(int argc,char** argv) {
 try {
  if(argc!=2||std::filesystem::exists(argv[1]))throw std::runtime_error("New independent report path required");
  Monstey_HashCases();Monstey_TypeAndBounds();Monstey_RandomAndCold();Monstey_AliasCases();Monstey_LiveStateCases();Monstey_EnumHighHashesAndHashFaults();Monstey_FaultCuts();Monstey_HighIndexAndLoads();
  std::ofstream report(argv[1],std::ios::binary);if(!report)throw std::runtime_error("Report output unavailable");
  report<<"{\"status\":\"PASS\",\"groups\":"<<Monstey_Counts.groups<<",\"cases\":"<<Monstey_Counts.cases<<",\"checks\":"<<Monstey_Counts.checks<<",\"fault_cases\":"<<Monstey_Counts.faults<<"}\n";
  std::cout<<"PASS "<<Monstey_Counts.cases<<" cases "<<Monstey_Counts.checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
 catch(const Monstey_IndependentFault&){std::cerr<<"Unexpected fixture construction fault\n";return 1;}
}
