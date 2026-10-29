#pragma once
#include "Monstey_FearCrtComparison.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
namespace Monstey_Independent {
using Monstey_U8=std::uint8_t;
using Monstey_U32=std::uint32_t;
using Monstey_I32=std::int32_t;
constexpr Monstey_U32 Monstey_Base=0x1000,Monstey_Left=0x1000,Monstey_Right=0x2000;
constexpr Monstey_U32 Monstey_Locale=0x3000,Monstey_Table=0x4002,Monstey_Ptd=0x5000;
constexpr Monstey_U32 Monstey_Global=0x7C38EF44;
struct Monstey_Case {
 std::string name;
 Monstey_U32 address,width;
 bool top;
 Monstey_U8 input;
 Monstey_I32 expected;
};
inline const std::array<Monstey_Case,8> Monstey_Cases{{
 {"global",Monstey_Global,4,true,0,0},
 {"ptd_locale",Monstey_Ptd+0x64,4,true,0,0},
 {"locale_gate",Monstey_Locale+0x14,4,true,0,0},
 {"ascii_gate",Monstey_Locale+0x24,4,false,'A',97},
 {"table_mode",Monstey_Locale+0x28,4,false,0xE9,233},
 {"table_pointer",Monstey_Locale+0x48,4,false,0xE9,233},
 {"classification_word",Monstey_Table+0xE9*2,2,false,0xE9,233},
 {"code_page",Monstey_Locale+4,4,false,0xE9,233}
}};
struct Monstey_Fixture {
 std::vector<Monstey_U8> bytes=std::vector<Monstey_U8>(0x6000);
 std::array<Monstey_U8,4> global{};
 std::vector<std::string> events;
 explicit Monstey_Fixture(const Monstey_Case& c) {
  Monstey_Put(Monstey_Left,'A',1);Monstey_Put(Monstey_Right,'a',1);
  Monstey_Put(Monstey_Ptd+0x64,Monstey_Locale,4);
  Monstey_Put(Monstey_Global,Monstey_Locale,4);
  Monstey_Put(Monstey_Locale+4,1252,4);Monstey_Put(Monstey_Locale+0x48,Monstey_Table,4);
  if(!c.top) {
   Monstey_Put(Monstey_Locale+0x14,0x409,4);
   Monstey_Put(Monstey_Locale+0x24,c.name=="ascii_gate"?1u:0u,4);
   Monstey_Put(Monstey_Locale+0x28,2,4);
   if(c.name=="code_page")Monstey_Put(Monstey_Table+0xE9*2,1,2);
  }
 }
 void Monstey_Put(Monstey_U32 at,Monstey_U32 value,Monstey_U32 width) {
  for(Monstey_U32 i=0;i<width;++i) {
   const auto address=at+i; auto b=static_cast<Monstey_U8>(value>>(8*i));
   if(address>=Monstey_Global&&address<Monstey_Global+4)global[address-Monstey_Global]=b;
   else bytes.at(address-Monstey_Base)=b;
  }
 }
 std::vector<Monstey::Monstey_CrtRegion> Monstey_Regions(const Monstey_Case& c,Monstey_U32 split)const {
  std::vector<Monstey::Monstey_CrtRegion> r;
  const auto append=[&r,&c,split](Monstey_U32 base,std::span<const Monstey_U8> data) {
   if(split!=0&&c.address>=base&&static_cast<std::uint64_t>(c.address)+split<static_cast<std::uint64_t>(base)+data.size()) {
    const auto n=static_cast<std::size_t>(c.address-base+split);
    r.push_back({base,data.first(n)});r.push_back({base+static_cast<Monstey_U32>(n),data.subspan(n)});
   } else r.push_back({base,data});
  };
  append(Monstey_Base,bytes);append(Monstey_Global,global);return r;
 }
 Monstey::Monstey_CrtBoundaries Monstey_Boundaries() {
  return {
   [this] {events.push_back("ptd");return Monstey_Ptd;},
   [this] {events.push_back("refresh");return Monstey_Locale;},
   [this](auto,auto) {events.push_back("scratch");return std::array<Monstey_U8,3>{0xA1,0xB2,0xC3};},
   [this](const auto& request,auto) {
    if(request.locale_id!=0x409||request.code_page!=1252||request.flags!=0x100||
       request.source_count!=1||request.destination_capacity!=3||request.strict!=1||request.source[0]!=0xE9)
     throw std::runtime_error("map request differs");
    events.push_back("map");return Monstey_I32{0};
   }
  };
 }
 Monstey_I32 Monstey_Run(const Monstey_Case& c,std::span<const Monstey::Monstey_CrtRegion> regions) {
  auto b=Monstey_Boundaries();
  return c.top?Monstey::Monstey_CrtStricmp({regions},Monstey_Left,Monstey_Right,b):
   Monstey::Monstey_CrtFoldByte({regions},Monstey_Locale,c.input,b);
 }
};
}
