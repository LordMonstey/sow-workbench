#include "Monstey_CrtRegionalFixtures.hpp"
#include <bit>
#include <iostream>
#include <limits>
namespace Monstey_Independent {
struct Monstey_Stats {std::uint64_t cases=0,checks=0;std::uint32_t groups=0;};
void Monstey_Require(bool ok,const std::string& what,Monstey_Stats& s) {
 ++s.checks;if(!ok)throw std::runtime_error(what);
}
std::vector<Monstey::Monstey_CrtRegion> Monstey_Coverage(const Monstey_Fixture& f,const Monstey_Case& c,
 bool bytewise,bool reversed,Monstey_U32 hole=0xFFFFFFFFu) {
 std::vector<Monstey::Monstey_CrtRegion> r;
 auto append=[&](Monstey_U32 base,std::span<const Monstey_U8> data) {
  const auto end=static_cast<std::uint64_t>(base)+data.size();
  if(c.address<base||static_cast<std::uint64_t>(c.address)+c.width>end) {r.push_back({base,data});return;}
  const auto prefix=static_cast<std::size_t>(c.address-base);
  if(prefix)r.push_back({base,data.first(prefix)});
  for(Monstey_U32 i=0;i<c.width;) {
   if(i==hole){++i;continue;}
   const auto start=i;
   ++i;
   if(!bytewise)while(i<c.width&&i!=hole)++i;
   r.push_back({c.address+start,data.subspan(prefix+start,i-start)});
  }
  const auto suffix=prefix+c.width;
  if(suffix<data.size())r.push_back({c.address+c.width,data.subspan(suffix)});
 };
 append(Monstey_Base,f.bytes);append(Monstey_Global,f.global);
 if(reversed)std::reverse(r.begin(),r.end());
 return r;
}
void Monstey_AdjacentAndHoles(Monstey_Stats& s) {
 ++s.groups;
 for(const auto& c:Monstey_Cases)for(bool reversed:{false,true}) {
  Monstey_Fixture f(c),baseline(c);
  const auto all=baseline.Monstey_Regions(c,0);
  const auto expected=baseline.Monstey_Run(c,all);
  const auto fragmented=Monstey_Coverage(f,c,true,reversed);
  Monstey_Require(f.Monstey_Run(c,fragmented)==expected&&f.events==baseline.events,"byte partition/callback order "+c.name,s);
  ++s.cases;
  for(Monstey_U32 hole=0;hole<c.width;++hole) {
   Monstey_Fixture missing(c);
   const auto regions=Monstey_Coverage(missing,c,true,reversed,hole);
   bool failed=false;
   try{(void)missing.Monstey_Run(c,regions);}
   catch(const Monstey::Monstey_CrtFault& e) {
    failed=true;Monstey_Require(e.address==c.address&&e.width==c.width,"whole operand fault coordinates "+c.name,s);
   }
   Monstey_Require(failed,"true hole accepted "+c.name,s);
   const std::vector<std::string> prefix=c.top?std::vector<std::string>{"ptd"}:std::vector<std::string>{};
   Monstey_Require(missing.events==prefix,"hole occurred after a later callback "+c.name,s);
   ++s.cases;
  }
 }
}
void Monstey_Overflow(Monstey_Stats& s) {
 ++s.groups;
 std::array<Monstey_U8,4> zero{};
 std::array<Monstey::Monstey_CrtRegion,2> crossing{{{0xFFFFFFFEu,std::span<const Monstey_U8>(zero).first(2)},{0,std::span<const Monstey_U8>(zero).last(2)}}};
 Monstey::Monstey_CrtBoundaries none;
 bool fault=false;try{(void)Monstey::Monstey_CrtFoldByte({crossing},0xFFFFFFEAu,'A',none);}
 catch(const Monstey::Monstey_CrtFault& e){fault=true;Monstey_Require(e.address==0xFFFFFFFEu&&e.width==4,"overflow fault coordinates",s);}
 Monstey_Require(fault,"operand wrapped across address limit",s);++s.cases;
 for(const auto pair:std::array<std::array<Monstey_U32,2>,2>{{{0xFFFFFFE8u,0xFFFFFFFCu},{0xFFFFFFF0u,4u}}}) {
  std::array<Monstey::Monstey_CrtRegion,2> regions{{{pair[1],std::span<const Monstey_U8>(zero).first(2)},{pair[1]+2,std::span<const Monstey_U8>(zero).last(2)}}};
  Monstey_Require(Monstey::Monstey_CrtFoldByte({regions},pair[0],'A',none)==97,"valid boundary/effective-address wrap",s);++s.cases;
 }
 std::array<Monstey_U8,1> left{{'A'}},nul{{0}};std::array<Monstey_U8,2> right{{'a',0}};
 std::array<Monstey::Monstey_CrtRegion,3> strings{{{0xFFFFFFFFu,left},{0,nul},{0x2000,right}}};
 Monstey_Require(Monstey::Monstey_CrtStricmpC({strings},0xFFFFFFFFu,0x2000)==0,"separate byte increments wrap",s);++s.cases;
}
void Monstey_CallbackAliases(Monstey_Stats& s) {
 ++s.groups;
 Monstey_Fixture f(Monstey_Cases[0]);
 f.Monstey_Put(Monstey_Ptd+0x64,0xDEADBEEF,4);
 auto regions=f.Monstey_Regions(Monstey_Cases[1],2);
 auto b=f.Monstey_Boundaries();
 b.get_ptd=[&] {f.events.push_back("ptd");f.Monstey_Put(Monstey_Ptd+0x64,Monstey_Locale,4);return Monstey_Ptd;};
 Monstey_Require(Monstey::Monstey_CrtStricmp({regions},Monstey_Left,Monstey_Right,b)==0,"PTD callback alias sampled too early",s);
 Monstey_Require(f.events==std::vector<std::string>{"ptd"},"unexpected PTD route boundary",s);++s.cases;
 Monstey_Fixture m(Monstey_Cases[7]);
 m.Monstey_Put(Monstey_Left,'A',1);m.Monstey_Put(Monstey_Right,'B',1);
 m.Monstey_Put(Monstey_Table+'A'*2,1,2);m.Monstey_Put(Monstey_Table+'B'*2,1,2);
 regions=m.Monstey_Regions(Monstey_Cases[7],2);b=m.Monstey_Boundaries();
 b.lc_map_string_a=[&](const auto& q,auto output) {
  m.events.push_back(q.call_site==Monstey::Monstey_CrtCallSite::Left?"map-left":"map-right");
  if(q.call_site==Monstey::Monstey_CrtCallSite::Left)m.Monstey_Put(Monstey_Right,'A',1);
  output[0]=q.source[0];return Monstey_I32{1};
 };
 Monstey_Require(Monstey::Monstey_CrtStricmp({regions},Monstey_Left,Monstey_Right,b)==0,"left fold does not precede right byte reload",s);
 Monstey_Require(m.events==std::vector<std::string>{"ptd","scratch","map-left","scratch","map-right"},"fold callbacks order",s);++s.cases;
}
void Monstey_FullRegionPrecedence(Monstey_Stats& s) {
 ++s.groups;
 // Retain prior complete-region precedence even for conflicting borrowed views.
 // This is a legacy owned-API observation, not a consistent native memory fixture.
 Monstey_Fixture f(Monstey_Cases[0]);
 auto regions=f.Monstey_Regions(Monstey_Cases[0],0);
 std::array<Monstey_U8,2> conflicting{{0xEF,0xBE}};
 regions.insert(regions.begin(),{Monstey_Global,conflicting});
 auto b=f.Monstey_Boundaries();
 Monstey_Require(Monstey::Monstey_CrtStricmp({regions},Monstey_Left,Monstey_Right,b)==0,"complete-region fast-path precedence changed",s);
 Monstey_Require(f.events==std::vector<std::string>{"ptd"},"partial view replaced complete slot",s);++s.cases;
}
}
int main() {
 try {
  Monstey_Independent::Monstey_Stats s;
  Monstey_Independent::Monstey_AdjacentAndHoles(s);
  Monstey_Independent::Monstey_Overflow(s);
  Monstey_Independent::Monstey_CallbackAliases(s);
  Monstey_Independent::Monstey_FullRegionPrecedence(s);
  std::cout<<"{\"status\":\"PASS\",\"groups\":"<<s.groups<<",\"cases\":"<<s.cases<<",\"checks\":"<<s.checks<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
