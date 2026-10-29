#include "Monstey_GadbValueFixup.hpp"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using Monstey_Address=std::uint64_t;using Monstey_Word=std::uint32_t;
std::uint64_t Monstey_Groups=0,Monstey_Cases=0,Monstey_Checks=0,Monstey_Faults=0;
void Monstey_Need(bool b,const char*t){++Monstey_Checks;if(!b)throw std::runtime_error(t);}
struct Monstey_Arena{
 Monstey_Address base=0;std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x6000);
 Monstey_GadbBinderMemory Memory(){return{base,bytes};}
 void Put(Monstey_Address a,Monstey_Address v,std::size_t n=8){for(std::size_t i=0;i<n;++i)bytes.at(static_cast<std::size_t>(a-base+i))=static_cast<std::uint8_t>(v>>(8*i));}
 Monstey_Address Get(Monstey_Address a,std::size_t n=8)const{Monstey_Address v=0;for(std::size_t i=0;i<n;++i)v|=Monstey_Address(bytes.at(static_cast<std::size_t>(a-base+i)))<<(8*i);return v;}
 void Descriptor(unsigned flags,unsigned count,unsigned first=0){Put(base+0x100a,flags,1);Put(base+0x100c,count,2);Put(base+0x100e,first,2);}
 void Context(){Put(base+0x1100+24,base+0x1200);Put(base+0x1200+24,base+0x1300);Put(base+0x1300+72,1,4);Put(base+0x1300+80,base+0x1800);}
};
constexpr char Monstey_MapHex[]=
"0000000000000000000000000000000000000000000000000000000000000000"
"253a333c3d3e403242434128362737341b1c1d1e1f2021222324313038293935"
"3b0102030405060708090a0b0c0d0e0f101112131415161718191a2c2a2d3f26"
"440102030405060708090a0b0c0d0e0f101112131415161718191a2e2b2f4500"
"0000000000000000000000000000000000000000000000000000000000000000"
"0000000000000000000000000000000000000000000000000000000000000000"
"0000000000000000000000000000000000000000000000000000000000000000"
"0000000000000000000000000000000000000000000000000000000000000000";
Monstey_Word Monstey_Map(std::uint8_t c){auto digit=[](char x){return static_cast<unsigned>(x<='9'?x-'0':x-'a'+10);};return digit(Monstey_MapHex[c*2])*16u+digit(Monstey_MapHex[c*2+1]);}
Monstey_Word Monstey_Polynomial(const std::vector<std::uint8_t>&s,Monstey_Word seed){std::size_t n=0;while(n<s.size()&&s[n])++n;auto power=[](std::size_t e){Monstey_Word x=1;for(std::size_t i=0;i<e;++i)x*=919u;return x;};Monstey_Word result=seed*power(n);for(std::size_t i=0;i<n;++i)result+=Monstey_Map(s[i])*power(n-i-1);return result;}
void Monstey_Hashes(){++Monstey_Groups;for(unsigned c=0;c<256;++c)for(auto seed:std::array<Monstey_Word,5>{0,1,0xffffffff,0x80000000,0x12345678}){++Monstey_Cases;Monstey_Arena a;const std::vector<std::uint8_t>s{static_cast<std::uint8_t>(c),0};a.bytes[0x3000]=s[0];Monstey_Need(Monstey_GadbHashCString(a.Memory(),0x3000,seed)==Monstey_Polynomial(s,seed),"Single-byte hash polynomial");}
 std::mt19937 random(0x4baf6c);for(unsigned trial=0;trial<200;++trial){++Monstey_Cases;Monstey_Arena a;const auto n=trial%80;std::vector<std::uint8_t>s;for(unsigned i=0;i<n;++i){const auto c=static_cast<std::uint8_t>(1+random()%255);s.push_back(c);a.bytes[0x3000+i]=c;}s.push_back(0);const auto seed=static_cast<Monstey_Word>(random());Monstey_Need(Monstey_GadbHashCString(a.Memory(),0x3000,seed)==Monstey_Polynomial(s,seed),"Whole hash independent polynomial");}
}
void Monstey_Noop(){++Monstey_Groups;for(unsigned flags=0;flags<256;++flags){const auto type=flags&63u;if(type==5||type==10||type==11)continue;++Monstey_Cases;Monstey_Arena a;a.bytes.resize(11);a.Put(10,flags,1);const auto before=a.bytes;const auto got=Monstey_GadbFixupValueDescriptor(a.Memory(),0,UINT64_MAX,UINT64_MAX);Monstey_Need(got==type&&a.bytes==before,"No-op type consumed unused metadata");}}
void Monstey_Enums(){++Monstey_Groups;for(unsigned flag=5;flag<256;flag+=64){++Monstey_Cases;Monstey_Arena a;a.Descriptor(flag,2,3);a.Put(0x3000+24,0x4000);a.Put(0x3000+32,0x4010);a.Put(0x4000,'A',1);a.Put(0x4010,'b',1);auto expected=a;expected.Put(0x3000+24,1);expected.Put(0x3000+32,2);const auto got=Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,UINT64_MAX,0x3000);Monstey_Need(got==2&&a.bytes==expected.bytes,"Enum hash Qword zero extension/type preservation");}
 {++Monstey_Cases;Monstey_Arena a;a.Descriptor(0xc5,0,65535);const auto before=a.bytes;Monstey_Need(Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,UINT64_MAX,UINT64_MAX)==5&&a.bytes==before,"Zero enum return/no value read");}
 {++Monstey_Cases;Monstey_Arena a;a.base=0x100000000ull;a.Descriptor(5,1);a.Put(a.base+0x3000,a.base+0x4000);a.Put(a.base+0x4000,'z',1);Monstey_Need(Monstey_GadbFixupValueDescriptor(a.Memory(),a.base+0x1000,0,a.base+0x3000)==1&&a.Get(a.base+0x3000)==26,"String pointer truncated to32bits");}
}
Monstey_Address Monstey_Resolved(unsigned category,unsigned ordinal,unsigned anon,unsigned named,Monstey_Address ap,Monstey_Address np){if(category>=1)return 0;if(ordinal<anon){const auto p=ap+Monstey_Address(ordinal)*32;if(p)return p;}const auto index=static_cast<Monstey_Word>(ordinal-anon);return index<named?np+Monstey_Address(index)*40:0;}
void Monstey_References(){++Monstey_Groups;for(unsigned flags:std::array<unsigned,4>{10,11,0x8b,0xcb})for(unsigned anon=0;anon<4;++anon)for(unsigned named=0;named<4;++named)for(unsigned cat=0;cat<2;++cat)for(unsigned ordinal=0;ordinal<9;++ordinal){++Monstey_Cases;Monstey_Arena a;a.Context();a.Descriptor(flags,1,2);a.Put(0x1800+64,anon,4);a.Put(0x1800+40,named,4);a.Put(0x1800+72,0x5000);a.Put(0x1800+56,0x4000);a.Put(0x3010,0xfedcba9800000000ull|(cat<<20)|ordinal);auto expected=a;expected.Put(0x100a,(flags&0xc0u)|10u,1);expected.Put(0x3010,Monstey_Resolved(cat,ordinal,anon,named,0x5000,0x4000));const auto got=Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0x1100,0x3000);Monstey_Need(got==1&&a.bytes==expected.bytes,"Reference count/bounds/category-order/stride/flags");}
 {++Monstey_Cases;Monstey_Arena a;a.Context();a.Descriptor(11,1);a.Put(0x1800+64,2,4);a.Put(0x1800+72,0);a.Put(0x1800+40,0xffffffff,4);a.Put(0x1800+56,0x9000);a.Put(0x3000,0);const auto want=0x9000ull+40ull*0xfffffffeu;Monstey_Need(Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0x1100,0x3000)==1&&a.Get(0x3000)==want,"Anonymous zero candidate must take cold named fallback");}
 {++Monstey_Cases;Monstey_Arena a;a.Context();a.Descriptor(10,1);a.Put(0x1800+64,1,4);a.Put(0x1800+72,0x5000);a.Put(0x3000,0);Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0x1100,0x3000);Monstey_Need(a.Get(0x3000)==0x5000,"Packed zero is not universally null");}
 {++Monstey_Cases;Monstey_Arena a;a.base=0x100000000ull;a.Context();a.Descriptor(0xcb,0,4);const auto category=a.base+0x1200;Monstey_Need(Monstey_GadbFixupValueDescriptor(a.Memory(),a.base+0x1000,a.base+0x1100,UINT64_MAX)==category&&a.Get(a.base+0x100a,1)==0xca,"Zero-ref fullRAX backlink/type store");}
 {++Monstey_Cases;Monstey_Arena a;a.bytes.resize(0x90000);a.Context();a.Descriptor(10,2,65535);a.Put(0x1800+64,2,4);a.Put(0x1800+72,0x5000);a.Put(0x3000+65535ull*8,0);a.Put(0x3000+65536ull*8,1);Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0x1100,0x3000);Monstey_Need(a.Get(0x3000+65535ull*8)==0x5000&&a.Get(0x3000+65536ull*8)==0x5020,"Value index must grow32bits beyond65535");}
 {++Monstey_Cases;Monstey_Arena a;a.Context();a.Descriptor(11,1);a.Put(0x1300+72,2,4);a.Put(0x1800+88+64,0,4);a.Put(0x1800+88+40,2,4);a.Put(0x1800+88+56,0x4300);a.Put(0x3000,(1u<<20)|1u);Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0x1100,0x3000);Monstey_Need(a.Get(0x3000)==0x4328,"Second category uses88B and named40B strides");}
 {++Monstey_Cases;Monstey_Arena a;a.Context();a.Descriptor(11,2);a.Put(0x1800+64,2,4);a.Put(0x1800+72,UINT64_MAX-31u);a.Put(0x1800+40,10000,4);a.Put(0x1800+56,0x4000);Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0x1100,0x1300+72);Monstey_Need(a.Get(0x1300+72)==0&&a.Get(0x1300+80)==0,"DB category count must reload after aliased first store");}
 {++Monstey_Cases;Monstey_Arena a;a.Context();a.Descriptor(10,2);a.Put(0x1800+64,0,4);a.Put(0x1800+40,0x2000,4);a.Put(0x1800+56,0x2800ull-40ull*0x1800u);a.Put(0x1300+88,1);a.Put(0x2800+64,2,4);a.Put(0x2800+72,0x5000);Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0x1100,0x1300+80);Monstey_Need(a.Get(0x1300+80)==0x2800&&a.Get(0x1300+88)==0x5020,"DB category table pointer must reload after aliased store");}
}
void Monstey_Aliases(){++Monstey_Groups;
 {++Monstey_Cases;Monstey_Arena a;a.Descriptor(5,2);a.Put(2,0,1);const auto got=Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0,0x100c);Monstey_Need(got==0&&a.Get(0x100c)==0,"Enum write must reload aliased shrinking count");}
 {++Monstey_Cases;Monstey_Arena a;a.Descriptor(5,1);a.Put(1,'b',1);a.Put(0x1014,0x50);a.Put(0x50,'c',1);const auto got=Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0,0x100c);Monstey_Need(got==2&&a.Get(0x100c)==2&&a.Get(0x1014)==3,"Enum write must extend live count");}
 {++Monstey_Cases;Monstey_Arena a;a.Context();a.Descriptor(11,2);a.Put(0x1300+72,0,4);a.Put(0x1014,0x7777);const auto got=Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0x1100,0x100c);Monstey_Need(got==0&&a.Get(0x100c)==0&&a.Get(0x100a,1)==10&&a.Get(0x1014)==0x7777,"Reference live count shrink/type prefix");}
 {++Monstey_Cases;Monstey_Arena a;a.Descriptor(5,2);a.Put(0x1061,0x1061);a.Put(0x1069,0x1061);Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0,0x1061);Monstey_Need(a.Get(0x1061)==919&&a.Get(0x1069)==0,"Earlier word store must affect later aliased string");}
 {++Monstey_Cases;Monstey_Arena a;a.bytes.resize(0x20000);a.Descriptor(5,1,1);a.Put(0x10001,'b',1);a.Put(0x1014,0x50);a.Put(0x50,'c',1);const auto got=Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0,0x1004);Monstey_Need(got==2&&a.Get(0x100c)==2&&a.Get(0x1014)==3,"Initial offset remains cached after aliased count/offset write");}
}
template<class Monstey_Call>void Monstey_Fault(Monstey_Call fn,Monstey_Address address,std::size_t width,bool writing){++Monstey_Cases;bool caught=false;try{fn();}catch(const Monstey_GadbValueFixupFault&e){caught=true;++Monstey_Faults;Monstey_Need(e.address==address&&e.width==width&&e.writing==writing,"Fixup first-fault epoch/range");}Monstey_Need(caught,"Missing consumed fixup fault");}
void Monstey_Failures(){++Monstey_Groups;
 {Monstey_Arena a;a.bytes.resize(10);Monstey_Fault([&]{Monstey_GadbFixupValueDescriptor(a.Memory(),0,0,0);},10,1,false);}
 {Monstey_Arena a;a.bytes.resize(14);a.Put(10,5,1);Monstey_Fault([&]{Monstey_GadbFixupValueDescriptor(a.Memory(),0,0,0);},14,2,false);}
 {Monstey_Arena a;a.Descriptor(0xcb,0);Monstey_Fault([&]{Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0xffff0000,0);},0xffff0018,8,false);Monstey_Need(a.Get(0x100a,1)==0xca,"Type normalization must precede backlink fault");}
 {Monstey_Arena a;a.Descriptor(5,2);a.Put(0x3000,0x4000);a.Put(0x4000,'a',1);a.Put(0x3008,0xffff0000);Monstey_Fault([&]{Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0,0x3000);},0xffff0000,1,false);Monstey_Need(a.Get(0x3000)==1&&a.Get(0x3008)==0xffff0000,"Earlier enum write lost on second hash fault");}
 {Monstey_Arena a;a.bytes.resize(0x3001);a.Put(0x3000,'a',1);Monstey_Fault([&]{Monstey_GadbHashCString(a.Memory(),0x3000);},0x3001,1,false);}
 {Monstey_Arena a;a.Context();a.Descriptor(11,1);a.Put(0x1300+72,0,4);a.bytes.resize(0x3004);a.Put(0x3000,0x77777777,4);Monstey_Fault([&]{Monstey_GadbFixupValueDescriptor(a.Memory(),0x1000,0x1100,0x3000);},0x3000,8,true);Monstey_Need(a.Get(0x100a,1)==10&&a.Get(0x3000,4)==0x77777777,"Store fault altered unavailable word/prefix");}
}
}
int main(int argc,char**argv){try{if(argc!=2)throw std::runtime_error("NEW_REPORT required");Monstey_Hashes();Monstey_Noop();Monstey_Enums();Monstey_References();Monstey_Aliases();Monstey_Failures();std::ofstream out(argv[1],std::ios::binary);if(!out)throw std::runtime_error("Report unavailable");out<<"{\"status\":\"PASS\",\"groups\":"<<Monstey_Groups<<",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"consumed_faults\":"<<Monstey_Faults<<"}\n";if(!out)throw std::runtime_error("Report write failed");std::cout<<"PASS fixup "<<Monstey_Cases<<" cases "<<Monstey_Checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
