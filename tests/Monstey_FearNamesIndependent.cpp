#include "Monstey_FearDbNames.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using Monstey_Word=std::uint32_t;
std::uint64_t Monstey_Groups=0,Monstey_Cases=0,Monstey_Checks=0,Monstey_Faults=0;
void Monstey_Need(bool ok,const char* text){++Monstey_Checks;if(!ok)throw std::runtime_error(text);}
struct Monstey_Arena{
 Monstey_Word base=0x1000;std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(0x4000);
 Monstey_FearDbMemory Memory()const{return{base,bytes};}
 void Put(Monstey_Word at,Monstey_Word value,unsigned width=4){for(unsigned i=0;i<width;++i)bytes.at(static_cast<std::size_t>(at-base+i))=static_cast<std::uint8_t>(value>>(8*i));}
 void Node(Monstey_Word node,Monstey_Word count,Monstey_Word table){Put(node+8,count);Put(node+12,table);}
};
constexpr Monstey_Word Monstey_Label(Monstey_Word rank){return 0x50000000u+rank*0x101u;}
void Monstey_LinearUnique(){++Monstey_Groups;
 for(Monstey_Word n=0;n<=192;++n){Monstey_Arena a;for(Monstey_Word i=0;i<n;++i)a.Put(0x1800+i*16,Monstey_Label(2*(i+1)));const auto before=a.bytes;
  for(Monstey_Word rank=0;rank<=2*n+1;++rank){++Monstey_Cases;const auto query=Monstey_Label(rank);std::uint32_t calls=0;
   const Monstey_FearDbNameCompare comparator=[&](auto q,auto c){++calls;Monstey_Need(q==query&&c>=Monstey_Label(2)&&c<=Monstey_Label(2*n),"Comparator raw identity order");return q<c?INT32_MIN:q>c?INT32_MAX:0;};
   Monstey_Word expected=0;for(Monstey_Word i=0;i<n;++i)if(rank==2*(i+1)){expected=0x1800+i*16;break;}
   const auto got=Monstey_FearDbBinaryNameSearch(a.Memory(),0x1800,n,query,comparator);Monstey_Need(got==expected,"Linear unique-name oracle mismatch");Monstey_Need((n==0)==(calls==0),"Empty table/comparator consumption");Monstey_Need(a.bytes==before,"Read-only search wrote memory");
  }
 }
}
void Monstey_DuplicateMidpoint(){++Monstey_Groups;for(Monstey_Word n=1;n<=257;++n){++Monstey_Cases;Monstey_Arena a;for(Monstey_Word i=0;i<n;++i)a.Put(0x1800+i*16,0xabcdef01);unsigned calls=0;const auto got=Monstey_FearDbBinaryNameSearch(a.Memory(),0x1800,n,0xf0000001,[&](auto q,auto c){++calls;Monstey_Need(q==0xf0000001&&c==0xabcdef01,"Duplicate raw identities");return 0;});Monstey_Need(got==0x1800+((n-1)/2)*16&&calls==1,"Duplicate search is not current midpoint");}}
void Monstey_WideMath(){++Monstey_Groups;const Monstey_FearDbMemory empty{0,{}};
 for(auto count:std::array<Monstey_Word,5>{0,0x80000001,0x80010000,0xfffffffe,0xffffffff}){++Monstey_Cases;Monstey_Need(Monstey_FearDbBinaryNameSearch(empty,0xffffffff,count,0x1234,{})==0,"Signed high gate consumed row");}
 for(auto count:std::array<Monstey_Word,5>{1,2,9,0x7fffffff,0x80000000}){++Monstey_Cases;Monstey_Arena a;a.Put(a.base,0);const auto mid=(count-1)/2;const auto children=a.base-mid*16;Monstey_Need(Monstey_FearDbBinaryNameSearch(a.Memory(),children,count,0,{})==a.base,"First large-count row arithmetic");}
 for(auto first:std::array<std::int32_t,4>{1,256,65536,INT32_MAX})for(auto second:std::array<std::int32_t,2>{0,INT32_MIN}){++Monstey_Cases;Monstey_Arena a;a.Put(a.base,0x77700001);a.Put(a.base+16,0x77700002);std::vector<Monstey_Word> seen;
  const auto got=Monstey_FearDbBinaryNameSearch(a.Memory(),a.base+16,0x80000000,0xaaaa0001,[&](auto,auto c){seen.push_back(c);if(seen.size()>2)throw std::runtime_error("Signed overflow search continued");return seen.size()==1?first:second;});
  Monstey_Need(got==(second==0?a.base+16:0)&&seen==std::vector<Monstey_Word>{0x77700001,0x77700002},"CDQ signed midpoint continuation/termination");
 }
}
template<class Monstey_Call>void Monstey_ReadFault(Monstey_Call call,Monstey_Word at,unsigned width){++Monstey_Cases;bool caught=false;try{call();}catch(const Monstey_FearDbFault&e){caught=true;++Monstey_Faults;Monstey_Need(e.address==at&&e.width==width&&!e.writing,"Wrong first consumed read");}Monstey_Need(caught,"Missing consumed read fault");}
void Monstey_GuardsAndFaults(){++Monstey_Groups;const Monstey_FearDbMemory none{0,{}};
 ++Monstey_Cases;Monstey_Need(Monstey_FearDbGuardedChildName(none,0,0xffffffff,{})==0,"Null parent did not short-circuit query");
 ++Monstey_Cases;Monstey_Need(Monstey_FearDbGuardedChildName(none,0xffffffff,0,{})==0,"Null query did not short-circuit parent");
 Monstey_ReadFault([&]{(void)Monstey_FearDbGuardedChildName(none,0xffff0000,0x1234,{});},0x1234,1);
 Monstey_Arena a;a.Put(0x1020,0,1);++Monstey_Cases;Monstey_Need(Monstey_FearDbGuardedChildName(a.Memory(),0xffff0000,0x1020,{})==0,"Empty query consumed bad parent");
 Monstey_ReadFault([&]{(void)Monstey_FearDbRawChildrenName(none,0x1234,0,{});},0x1240,4);
 const Monstey_FearDbMemory countOnly{0x1208,std::span<const std::uint8_t>(a.bytes).subspan(0x208,4)};
 Monstey_ReadFault([&]{(void)Monstey_FearDbRawChildrenName(countOnly,0x1200,0,{});},0x120c,4);
 const Monstey_FearDbMemory pointerOnly{0x120c,std::span<const std::uint8_t>(a.bytes).subspan(0x20c,4)};
 Monstey_ReadFault([&]{(void)Monstey_FearDbRawChildrenName(pointerOnly,0x1200,0,{});},0x1208,4);
 Monstey_ReadFault([&]{(void)Monstey_FearDbBinaryNameSearch(none,0x1800,1,0,{});},0x1800,4);
 a.Node(0x1200,1,0x1800);a.Put(0x1800,0);++Monstey_Cases;Monstey_Need(Monstey_FearDbRawChildrenName(a.Memory(),0x1200,0,{})==0x1800,"Raw null/null needs comparator");
 ++Monstey_Cases;Monstey_Need(Monstey_FearDbRawChildrenName(a.Memory(),0x1200,0xf0000001,{})==0,"Raw null candidate needs comparator");
 a.Put(0x1800,0x50000000);++Monstey_Cases;Monstey_Need(Monstey_FearDbRawChildrenName(a.Memory(),0x1200,0,{})==0,"Null query/nonzero candidate needs comparator");
 ++Monstey_Cases;unsigned calls=0;Monstey_Need(Monstey_FearDbRawChildrenName(a.Memory(),0x1200,0xf0000001,[&](auto q,auto c){++calls;Monstey_Need(q==0xf0000001&&c==0x50000000,"Unmapped raw names changed");return 0;})==0x1800&&calls==1,"Raw search scanned unmapped name bytes");
}
void Monstey_LiveEpochs(){++Monstey_Groups;
 for(bool guarded:std::array<bool,2>{false,true}){++Monstey_Cases;Monstey_Arena a;a.Put(0x1020,0x55,1);a.Node(0x1200,5,0x1800);for(Monstey_Word i=0;i<5;++i)a.Put(0x1800+i*16,0x60000000+i);std::vector<Monstey_Word>seen;
  const auto cmp=[&](auto q,auto c){Monstey_Need(q==0x1020,"Query identity reloaded");seen.push_back(c);if(seen.size()==1){a.Node(0x1200,0,0xffffffff);a.Put(0x1800,0x77770000);a.Put(0x1020,0,1);return -1;}return 0;};
  const auto got=guarded?Monstey_FearDbGuardedChildName(a.Memory(),0x1200,0x1020,cmp):Monstey_FearDbRawChildrenName(a.Memory(),0x1200,0x1020,cmp);
  Monstey_Need(got==0x1800&&seen==std::vector<Monstey_Word>{0x60000002,0x77770000},"Metadata snapshot/live row/query guard epochs");
 }
 ++Monstey_Cases;Monstey_Arena a;a.Put(0x1020,0x55,1);a.Node(0x1200,1,0x1800);a.Put(0x1800,0x88880000);a.Node(0x1800,0,0xffffffff);a.Put(0x1900,0x99990000);std::vector<Monstey_Word> seen;
 const auto got=Monstey_FearDbRecordByNames(a.Memory(),0x1200,0x1020,0xf0000001,[&](auto,auto c){seen.push_back(c);if(seen.size()==1)a.Node(0x1800,1,0x1900);return 0;});
 Monstey_Need(got==0x1900&&seen==std::vector<Monstey_Word>{0x88880000,0x99990000},"Wrapper prefetched later child metadata");
}
void Monstey_WrapperMapping(){++Monstey_Groups;Monstey_Arena a;a.Put(0x1020,0x7f,1);a.Put(0x1021,0,1);a.Put(0x1022,0x7e,1);a.Node(0x1200,1,0x1800);a.Put(0x1800,0x1020);a.Node(0x1800,1,0x1900);a.Put(0x1900,0);a.Node(0x1900,1,0x1a00);a.Put(0x1a00,0);unsigned calls=0;const auto cmp=[&](auto,auto){++calls;return 0;};
 ++Monstey_Cases;Monstey_Need(Monstey_FearDbRecordByNames(a.Memory(),0x1200,0x1020,0,cmp)==0x1900&&calls==1,"Record wrapper final null is raw");
 ++Monstey_Cases;Monstey_Need(Monstey_FearDbGuardedChildName(a.Memory(),0x1800,0,cmp)==0,"Direct null record unexpectedly raw");
 ++Monstey_Cases;Monstey_Need(Monstey_FearDbAttributeInDatabaseByNames(a.Memory(),0x1200,0x1020,0,0,cmp)==0x1a00,"Database attribute arity/null mapping");
 ++Monstey_Cases;Monstey_Need(Monstey_FearDbAttributeByNames(a.Memory(),0x1800,0,0,cmp)==0,"Category attribute bypassed guarded record");
 a.Put(0x1900,0x1022);a.Put(0x1a00,0x1021);calls=0;
 ++Monstey_Cases;Monstey_Need(Monstey_FearDbAttributeByNames(a.Memory(),0x1800,0x1022,0x1021,cmp)==0x1a00&&calls==2,"Category attribute final empty is raw");
 ++Monstey_Cases;Monstey_Need(Monstey_FearDbGuardedChildName(a.Memory(),0x1900,0x1021,cmp)==0,"Direct empty attribute unexpectedly raw");
 a.Put(0x1900,0x1021);calls=0;++Monstey_Cases;Monstey_Need(Monstey_FearDbRecordByNames(a.Memory(),0x1200,0x1020,0x1021,cmp)==0x1900&&calls==2,"Record wrapper final empty is raw");
}
void Monstey_ExternalBoundary(){++Monstey_Groups;Monstey_Arena a;a.Put(0x1800,0x55667788);++Monstey_Cases;bool missing=false;try{(void)Monstey_FearDbBinaryNameSearch(a.Memory(),0x1800,1,0x12345678,{});}catch(const std::invalid_argument&){missing=true;}Monstey_Need(missing,"Missing consumed comparator silently defaulted");
 ++Monstey_Cases;unsigned calls=0;bool propagated=false;try{(void)Monstey_FearDbBinaryNameSearch(a.Memory(),0x1800,1,0x12345678,[&](auto,auto)->std::int32_t{++calls;a.Put(0x1800,0xcafebabe);throw std::runtime_error("external sentinel");});}catch(const std::runtime_error&e){propagated=std::string(e.what())=="external sentinel";}Monstey_Need(propagated&&calls==1,"External failure retried/swallowed");
 ++Monstey_Cases;Monstey_Need(Monstey_FearDbBinaryNameSearch(a.Memory(),0x1800,1,0x12345678,[&](auto,auto c){Monstey_Need(c==0xcafebabe,"External prefix write lost");return 0;})==0x1800,"External side effects not preserved");
}
void Monstey_WrappedParent(){++Monstey_Groups;std::array<std::uint8_t,8>bytes{};bytes[4]=0xff;const Monstey_FearDbMemory m{0,bytes};++Monstey_Cases;Monstey_Need(Monstey_FearDbRawChildrenName(m,0xfffffff8,0,{})==0,"Parent native32 offsets did not wrap");
 const Monstey_FearDbMemory pointerOnly{4,std::span<const std::uint8_t>(bytes).subspan(4,4)};Monstey_ReadFault([&]{(void)Monstey_FearDbRawChildrenName(pointerOnly,0xfffffff8,0,{});},0,4);
 const Monstey_FearDbMemory high{0xfffffffe,std::span<const std::uint8_t>(bytes).first(4)};Monstey_ReadFault([&]{(void)Monstey_FearDbBinaryNameSearch(high,0xfffffffe,1,0,{});},0xfffffffe,4);
}
}
int main(int argc,char**argv){try{if(argc!=2)throw std::invalid_argument("NEW_REPORT required");Monstey_LinearUnique();Monstey_DuplicateMidpoint();Monstey_WideMath();Monstey_GuardsAndFaults();Monstey_LiveEpochs();Monstey_WrapperMapping();Monstey_ExternalBoundary();Monstey_WrappedParent();std::ofstream out(argv[1],std::ios::binary);if(!out)throw std::runtime_error("Cannot create report");out<<"{\"status\":\"PASS\",\"groups\":"<<Monstey_Groups<<",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"consumed_faults\":"<<Monstey_Faults<<",\"historical_comparator_claimed\":false}\n";if(!out)throw std::runtime_error("Report write failure");std::cout<<"PASS names independent "<<Monstey_Cases<<" cases "<<Monstey_Checks<<" checks\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
