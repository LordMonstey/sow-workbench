#include "Monstey_FearCrtComparison.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
std::uint64_t Monstey_Checks = 0, Monstey_Cases = 0;
void Monstey_Check(bool ok, const char* message) {
 ++Monstey_Checks;
 if (!ok) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
}
constexpr std::uint32_t Monstey_Base = 0x1000, Monstey_Ptd = 0x1100, Monstey_Locale = 0x1300;
constexpr std::uint32_t Monstey_Left = 0x1600, Monstey_Right = 0x1700, Monstey_Table = 0x3000;
struct Monstey_Fixture {
 std::vector<std::uint8_t> storage = std::vector<std::uint8_t>(0x1000);
 std::vector<std::uint8_t> table = std::vector<std::uint8_t>(512);
 std::array<std::uint8_t, 4> global{};
 std::array<Monstey_CrtRegion, 3> regions{};
 Monstey_CrtBoundaries callbacks;
 std::uint32_t ptd_calls=0, refresh_calls=0, scratch_calls=0, map_calls=0;
 std::vector<Monstey_CrtMapRequest> requests;
 Monstey_Fixture() {
  put(Monstey_Ptd + 0x64, Monstey_Locale);
  put(Monstey_Locale + 0x48, Monstey_Table);
  put(Monstey_Locale + 0x04, 1252);
  set_global(Monstey_Locale);
  callbacks.get_ptd = [&] { ++ptd_calls; return Monstey_Ptd; };
  callbacks.refresh_locale = [&] { ++refresh_calls; return Monstey_Locale; };
  callbacks.initial_map_scratch = [&](auto, auto) { ++scratch_calls; return std::array<std::uint8_t, 3>{0xA1,0xB2,0xC3}; };
  callbacks.lc_map_string_a = [&](const auto& request, auto output) {
   ++map_calls; requests.push_back(request);
   output[0] = static_cast<std::uint8_t>(request.source[request.source_count - 1] + 32u);
   return 1;
  };
 }
 void put(std::uint32_t at, std::uint32_t value) {
  for (unsigned i=0;i<4;++i) storage[at-Monstey_Base+i]=static_cast<std::uint8_t>(value>>(i*8));
 }
 void set_global(std::uint32_t value) { for(unsigned i=0;i<4;++i) global[i]=static_cast<std::uint8_t>(value>>(8*i)); }
 void text(std::uint32_t at, std::initializer_list<std::uint8_t> bytes) {
  std::copy(bytes.begin(),bytes.end(),storage.begin()+at-Monstey_Base);
 }
 Monstey_CrtMemory memory() {
  regions={Monstey_CrtRegion{Monstey_Base,storage},Monstey_CrtRegion{Monstey_Table,table},Monstey_CrtRegion{0x7C38EF44u,global}};
  return {regions};
 }
 std::int32_t compare() { return Monstey_CrtStricmp(memory(),Monstey_Left,Monstey_Right,callbacks); }
 std::int32_t fold(std::uint8_t c) { return Monstey_CrtFoldByte(memory(),Monstey_Locale,c,callbacks); }
};
std::uint32_t Monstey_ReferenceLower(std::uint32_t c) {
 const auto distance = static_cast<std::uint8_t>(c - 65u);
 return distance < 26u ? c + 32u : c;
}
void Monstey_TestC() {
 Monstey_Fixture f;
 for(unsigned a=0;a<256;++a) for(unsigned b=0;b<256;++b) {
  ++Monstey_Cases;
  f.text(Monstey_Left,{static_cast<std::uint8_t>(a),0}); f.text(Monstey_Right,{static_cast<std::uint8_t>(b),0});
  const auto left=Monstey_ReferenceLower(a),right=Monstey_ReferenceLower(b);
  const int expected = left==right?0:left<right?-1:1;
  Monstey_Check(Monstey_CrtStricmpC(f.memory(),Monstey_Left,Monstey_Right)==expected,"C pair normalized unsigned return");
 }
 ++Monstey_Cases;
 f.text(Monstey_Left,{'A','b','C',0,0xCC}); f.text(Monstey_Right,{'a','B','c',0,0xDD});
 Monstey_Check(f.compare()==0,"C multi pair terminates at equal NUL");
 Monstey_Check(f.ptd_calls==1&&f.refresh_calls==0&&f.map_calls==0&&f.scratch_calls==0,"C route consumed only PTD");
 f.text(Monstey_Left,{'a',0});f.text(Monstey_Right,{'A','z',0});
 Monstey_Check(f.compare()==-1,"case equal pair continues into prefix difference");
}
void Monstey_TestFoldGates() {
 for(auto mode:std::array<std::uint32_t,5>{0,1,2,0xFFFFFFFFu,0x80000000u})
 for(unsigned gate=0;gate<3;++gate) for(unsigned c=0;c<256;++c) {
  ++Monstey_Cases; Monstey_Fixture f;
  f.put(Monstey_Locale+0x14,gate?0x407:0); f.put(Monstey_Locale+0x24,gate==2?1:0); f.put(Monstey_Locale+0x28,mode);
  // All classification clear: non-ASCII route must return unchanged.
  const bool ascii=gate==0||(gate==2&&c<=127);
  Monstey_Check(f.fold(static_cast<std::uint8_t>(c))==static_cast<std::int32_t>(ascii?Monstey_ReferenceLower(c):c),"fold locale gates and unsigned byte");
  Monstey_Check(f.map_calls==0&&f.scratch_calls==0,"fold gates avoid unavailable mapping");
 }
 for(unsigned c=0;c<256;++c) {
  ++Monstey_Cases; Monstey_Fixture f;
  f.put(Monstey_Locale+0x14,0x407); f.put(Monstey_Locale+0x28,2); f.table[c*2]=2;
  Monstey_Check(f.fold(static_cast<std::uint8_t>(c))==static_cast<int>(c),"classification exact mask1 not nonzero");
 }
}
void Monstey_TestMap() {
 for(unsigned lead=0;lead<2;++lead) for(unsigned c=0;c<256;++c) {
  ++Monstey_Cases; Monstey_Fixture f;
  f.put(Monstey_Locale+0x14,0x407); f.table[c*2]=1; f.table[1]=lead?0x80:0;
  Monstey_Check(f.fold(static_cast<std::uint8_t>(c))==static_cast<int>(static_cast<std::uint8_t>(c+32)),"map result count1");
  Monstey_Check(f.map_calls==1&&f.scratch_calls==1,"flagged byte maps exactly once");
  const auto& r=f.requests.at(0);
  Monstey_Check(r.locale_id==0x407&&r.flags==0x100&&r.destination_capacity==3&&r.code_page==1252&&r.strict==1,"map exact native parameters");
  Monstey_Check(r.call_site==Monstey_CrtCallSite::Direct&&r.iteration==0,"map direct explicit frame");
  Monstey_Check(r.source_count==(lead?2u:1u)&&r.source[0]==(lead?0u:c)&&r.source[lead?1u:0u]==c,"map source low byte and zero high-byte lead path");
 }
 for(const auto count:std::array<std::int32_t,7>{0,1,2,3,-1,INT32_MIN,INT32_MAX}) {
  ++Monstey_Cases; Monstey_Fixture f;
  f.put(Monstey_Locale+0x14,0x407); f.table['X'*2]=1;
  f.callbacks.lc_map_string_a=[&](const auto&,auto output) {output[0]=0x12;return count;};
  Monstey_Check(f.fold('X')==(count==0?'X':count==1?0x12:0x12B2),"zero fallback, one byte, other count big endian and explicit partial scratch");
 }
 ++Monstey_Cases; Monstey_Fixture f; f.put(Monstey_Locale+0x14,0x407); f.table['X'*2]=1;
 f.callbacks.lc_map_string_a=[](const auto&,auto){return 2;};
 f.callbacks.initial_map_scratch=[](auto site,auto iteration){
  return site==Monstey_CrtCallSite::Left?std::array<std::uint8_t,3>{0x12,static_cast<std::uint8_t>(0x34+iteration),0xFE}:std::array<std::uint8_t,3>{0x56,0x78,0xFD};
 };
 f.text(Monstey_Left,{'X',0});f.text(Monstey_Right,{'X',0});
 Monstey_Check(f.compare()==0x1234-0x5678,"same raw bytes fold separately with distinct frame seeds and exact difference");
}
void Monstey_TestLocaleLoop() {
 ++Monstey_Cases; Monstey_Fixture f; f.put(Monstey_Locale+0x14,0x407);
 f.text(Monstey_Left,{'A','B',0});f.text(Monstey_Right,{'a','b',0}); f.table['A'*2]=f.table['B'*2]=1;
 Monstey_Check(f.compare()==0,"non C multiple pairs");
 Monstey_Check(f.requests.size()==2&&f.requests[0].call_site==Monstey_CrtCallSite::Left&&f.requests[1].iteration==1,"iteration and frame forwarded");
 ++Monstey_Cases; Monstey_Fixture g; g.put(Monstey_Locale+0x14,0x407);
 g.text(Monstey_Left,{'A',0});g.text(Monstey_Right,{'B',0});g.table['A'*2]=1;
 g.callbacks.lc_map_string_a=[&](const auto&,auto output){g.storage[Monstey_Right-Monstey_Base]='a';output[0]='a';return 1;};
 Monstey_Check(g.compare()==0,"left folding callback precedes right byte load");
 ++Monstey_Cases; Monstey_Fixture h; h.put(Monstey_Locale+0x14,0x407);
 h.text(Monstey_Left,{'X','Y',0}); h.text(Monstey_Right,{'X','Z',0});h.table['X'*2]=1;
 h.callbacks.lc_map_string_a=[](const auto&,auto output){output[0]=0;return 1;};
 Monstey_Check(h.compare()==0,"mapped zero terminates despite nonzero raw input");
 ++Monstey_Cases; Monstey_Fixture k; k.put(Monstey_Locale+0x14,0x407);
 k.text(Monstey_Left,{0,'a',0});k.text(Monstey_Right,{0,'z',0});k.table[0]=1;
 k.callbacks.lc_map_string_a=[](const auto&,auto output){output[0]=0x7F;return 1;};
 Monstey_Check(k.compare()=='a'-'z',"mapped nonzero NUL continues and return is unnormalized");
 ++Monstey_Cases; Monstey_Fixture r; r.set_global(0xDEADBEEF);
 Monstey_Check(r.compare()==0&&r.ptd_calls==1&&r.refresh_calls==1,"refresh only mismatched pointers");
 ++Monstey_Cases; Monstey_Fixture s; s.put(Monstey_Ptd+0x64,0xDEAD);s.set_global(0xBEEF);
 Monstey_Check(s.compare()==0&&s.refresh_calls==1,"refreshed locale identity replaces invalid PTD locale");
}
void Monstey_TestFaults() {
 ++Monstey_Cases; Monstey_Fixture f;
 try { (void)Monstey_CrtStricmpC(f.memory(),0xF000,0xE000);Monstey_Check(false,"missing C fault"); }
 catch(const Monstey_CrtFault& x){Monstey_Check(x.address==0xE000&&x.width==1,"C right read first");}
 f.put(Monstey_Locale+0x14,0x407);
 try { (void)Monstey_CrtStricmp(f.memory(),0xF000,0xE000,f.callbacks);Monstey_Check(false,"missing locale fault"); }
 catch(const Monstey_CrtFault& x){Monstey_Check(x.address==0xF000&&x.width==1,"locale left read first");}
 for(const auto mode:std::array<std::uint32_t,5>{0,1,2,0xFFFFFFFFu,0x80000000u}) {
  ++Monstey_Cases; Monstey_Fixture t;t.table.resize(511);t.put(Monstey_Locale+0x14,0x407);t.put(Monstey_Locale+0x28,mode);
  try { Monstey_Check(t.fold(255)==255&&mode!=2,"one-byte table read for signed mode <=1"); }
  catch(const Monstey_CrtFault& x){Monstey_Check(mode==2&&x.address==Monstey_Table+510&&x.width==2,"classifier WORD read for signed mode>1");}
 }
 ++Monstey_Cases;Monstey_Fixture g;g.callbacks.get_ptd={};
 try{(void)g.compare();Monstey_Check(false,"missing getptd boundary");}catch(const Monstey_CrtBoundaryFault& x){Monstey_Check(x.boundary==Monstey_CrtBoundary::GetPtd,"explicit unavailable PTD");}
 g.callbacks.get_ptd=[] {return Monstey_Ptd;};g.set_global(0xFAFA);g.callbacks.refresh_locale={};
 try{(void)g.compare();Monstey_Check(false,"missing refresh boundary");}catch(const Monstey_CrtBoundaryFault& x){Monstey_Check(x.boundary==Monstey_CrtBoundary::RefreshLocale,"explicit unavailable refresh");}
 g.put(Monstey_Locale+0x14,0x407);g.table['X'*2]=1;g.callbacks.initial_map_scratch={};
 try{(void)g.fold('X');Monstey_Check(false,"missing scratch boundary");}catch(const Monstey_CrtBoundaryFault& x){Monstey_Check(x.boundary==Monstey_CrtBoundary::InitialMapScratch,"explicit unavailable private map scratch");}
 g.callbacks.initial_map_scratch=[](auto,auto){return std::array<std::uint8_t,3>{1,2,3};};g.callbacks.lc_map_string_a={};
 try{(void)g.fold('X');Monstey_Check(false,"missing map boundary");}catch(const Monstey_CrtBoundaryFault& x){Monstey_Check(x.boundary==Monstey_CrtBoundary::LcMapStringA,"explicit unavailable map callback");}
 // A skipped ASCII branch may not dereference the unavailable classification pointer.
 ++Monstey_Cases;Monstey_Fixture a;a.put(Monstey_Locale+0x14,0);a.put(Monstey_Locale+0x48,0xBAD);
 Monstey_Check(a.fold('A')=='a',"C locale avoids all table and mapper dependencies");
 a.put(Monstey_Locale+0x14,0x407);a.put(Monstey_Locale+0x24,1);
 Monstey_Check(a.fold('Z')=='z',"ASCII shortcut avoids table dependencies");
}
}
int main(){
 Monstey_TestC();Monstey_TestFoldGates();Monstey_TestMap();Monstey_TestLocaleLoop();Monstey_TestFaults();
 std::cout<<"{\"suite\":\"owned-crt-synthetic\",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"status\":\"PASS\"}\n";
}
