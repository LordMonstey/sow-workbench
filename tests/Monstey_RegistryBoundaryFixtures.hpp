#pragma once
#include "Monstey_GadbRegistry.hpp"
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using B=Monstey_Bytes;using J=Monstey_Json;
constexpr std::uint64_t base=0x140000000ull;
void U(B& b,std::size_t at,std::uint32_t n){Monstey_PutU32(b,at,n);}
void Q(B& b,std::size_t at,std::uint64_t n){Monstey_PutU64(b,512+at,n);}
std::uint64_t EA(std::size_t local){return base+0x1000+local;}
void T(B& b,std::size_t at,std::string_view text){std::copy(text.begin(),text.end(),b.begin()+static_cast<std::ptrdiff_t>(512+at));b.at(512+at+text.size())=0;}
B PE(std::uint32_t bytes=8192){B b(512ull+bytes);b[0]='M';b[1]='Z';U(b,60,128);U(b,128,0x4550);Monstey_PutU16(b,132,0x8664);Monstey_PutU16(b,134,1);Monstey_PutU16(b,148,240);Monstey_PutU16(b,152,0x20b);Monstey_PutU64(b,176,base);std::memcpy(b.data()+392,".data",5);for(const auto& [off,v]:std::vector<std::pair<std::size_t,std::uint32_t>>{{8,bytes},{12,0x1000},{16,bytes},{20,512},{36,0xe0000040}})U(b,392+off,v);return b;}
J Read(const B&b){return Monstey_GadbDiscoverNativeDeclarations(b,Monstey_HashSha256(b));}
B FieldPE(){auto b=PE();T(b,0x1000,"Parent");T(b,0x1020,"Test.Parent");T(b,0x1080,"Value");Q(b,0x100,EA(0x500));Q(b,0x108,1);Q(b,0x110,EA(0x1000));Q(b,0x120,EA(0x1020));Q(b,0x138,8);Q(b,0x500,EA(0x1080));U(b,512+0x510,0x3f800000);U(b,512+0x520,Monstey_NativeHash("Value"));U(b,512+0x524,2);U(b,512+0x528,1);Q(b,0x538,EA(0x800));b[512+0x800]=0x32;b[512+0x801]=0xc0;b[512+0x802]=0xc3;return b;}
B Chain(unsigned count,bool rootsFirst){auto b=PE(65536);for(unsigned i=0;i<count;++i){const auto value=rootsFirst?i:count-i-1;const auto name="Chain"+std::string(value<10?"00":value<100?"0":"")+std::to_string(value);const std::size_t at=0x100+64ull*i,text=0x6000+32ull*i;T(b,text,name);Q(b,at+16,EA(text));Q(b,at+32,EA(text));Q(b,at+40,i?EA(at-64):0);Q(b,at+56,8);}return b;}
std::uint64_t Depth(const J&r){std::uint64_t n=0;for(const auto &[name,c]:r.at("categories").as_object()){(void)name;if(c.contains("ancestors"))n=std::max(n,static_cast<std::uint64_t>(c.at("ancestors").size()));}return n;}
void Check(bool b,const char*m){Monstey_Require(b,m);}
}
