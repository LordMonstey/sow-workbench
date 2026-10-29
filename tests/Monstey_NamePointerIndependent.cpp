#include "Monstey_GadbNamePointerBoundary.hpp"
#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Monstey;
namespace {
using U64=std::uint64_t;using U32=std::uint32_t;
U64 Monstey_Checks=0,Monstey_Cases=0,Monstey_Groups=0;
void Monstey_Check(bool value,const char* text){++Monstey_Checks;if(!value)throw std::runtime_error(text);}
auto Monstey_Banks(){
    std::array<Monstey_GadbPointerBankNames,6> banks{};
    for(U32 i=0;i<6;++i)banks[i].name_pointer=100+i;
    return banks;
}
std::int32_t Monstey_Policy(U64 a,U64 b){return a==b?0:0x100;}
template<class E,class F>void Monstey_Throws(F f){
 bool yes=false;try{f();}catch(const E&){yes=true;}Monstey_Check(yes,"required exception boundary");
}
void Monstey_Compare(){
 ++Monstey_Groups;U64 calls=0,left=0,right=0;
 for(U64 a:{0ull,1ull,0x100000001ull,UINT64_MAX})
  for(U64 b:{0ull,1ull,0x100000001ull,UINT64_MAX})
   for(U32 raw:{0u,1u,0x100u,0x10000u,0x80000000u,0xffffffffu,0x7fffffffu}){
    ++Monstey_Cases;calls=0;
    const auto result=std::bit_cast<std::int32_t>(raw);
    const auto policy=[&](U64 x,U64 y){++calls;left=x;right=y;return result;};
    const auto expected=a==0?(b==0?0:-1):(b==0?1:result);
    Monstey_Check(Monstey_GadbCompareNamePointers(a,b,policy)==expected,"full EAX/null order");
    Monstey_Check(calls==(a&&b?1u:0u),"null suppresses external call");
    if(a&&b)Monstey_Check(left==a&&right==b,"external argument identity/order");
    calls=0;
    Monstey_Check(Monstey_GadbNamePointersEqual(a,b,policy)==(expected==0),"full EAX equality");
    Monstey_Check(calls==(a&&b?1u:0u),"equality callback count");
   }
 Monstey_Check(!Monstey_GadbNamePointersEqual(5,5,[](U64,U64){return 0x100;}),"no identity equality shortcut");
 Monstey_Check(Monstey_GadbCompareNamePointers(0,0,{})==0,"null equality no policy");
 Monstey_Check(Monstey_GadbCompareNamePointers(0,1,{})==-1,"null low no policy");
 Monstey_Check(Monstey_GadbCompareNamePointers(1,0,{})==1,"null high no policy");
 Monstey_Throws<std::invalid_argument>([]{Monstey_GadbCompareNamePointers(1,1,{});});
 Monstey_Throws<std::logic_error>([]{Monstey_GadbCompareNamePointers(2,3,[](U64,U64)->std::int32_t{throw std::logic_error("external");});});
}
void Monstey_Search(){
 ++Monstey_Groups;
 for(U32 bank=0;bank<=6;++bank)for(U32 count=0;count<=24;++count)
  for(U32 hit=0;hit<=count;++hit)for(U32 gate=0;gate<3;++gate){
   ++Monstey_Cases;auto banks=Monstey_Banks();U32 calls=0;
   for(U32 b=0;b<6;++b)for(U32 i=0;i<count;++i)
    banks[b].classes.push_back({10000+static_cast<U64>(b)*100+i,1000+i});
   if(bank<6&&hit<count){
    banks[bank].classes[hit].full_name_pointer=900;
    if(hit+1<count)banks[bank].classes[hit+1].full_name_pointer=900;
   }
   Monstey_GadbPointerCategory category{900,gate==1?1u:0u,gate==2?UINT32_MAX:0u};
   const auto result=Monstey_GadbSelectPointerClass(banks,100+bank,category,[&](U64 a,U64 b){++calls;return Monstey_Policy(a,b);});
   Monstey_Check(result.bank_comparisons==(bank<6?bank+1:6),"bank count");
   if(bank==6){
    Monstey_Check(result.status==Monstey_GadbPointerSelectionStatus::BankNotFound&&!result.bank_index&&!result.class_index&&!result.class_identity,"missing bank");
    Monstey_Check(result.class_comparisons==0&&calls==6,"missing bank no classes");
    continue;
   }
   Monstey_Check(result.bank_index==bank,"first bank index");
   if(gate==0){
    Monstey_Check(result.status==Monstey_GadbPointerSelectionStatus::CategorySkipped,"empty category");
    Monstey_Check(result.class_comparisons==0&&!result.class_index&&!result.class_identity,"empty skips class");
   }else if(hit==count){
    Monstey_Check(result.status==Monstey_GadbPointerSelectionStatus::ClassNotFound&&!result.class_identity&&!result.class_index,"missing first-bank class");
    Monstey_Check(result.class_comparisons==count,"all class rows");
   }else{
    Monstey_Check(result.status==Monstey_GadbPointerSelectionStatus::ClassFound&&result.class_index==hit,"first duplicate class");
    Monstey_Check(result.class_identity==10000+static_cast<U64>(bank)*100+hit,"matched identity");
    Monstey_Check(result.class_comparisons==hit+1,"class comparison count");
   }
   Monstey_Check(calls==result.bank_comparisons+result.class_comparisons,"nonnull callback count");
  }
 auto banks=Monstey_Banks();banks[0].name_pointer=7;banks[1].name_pointer=7;
 banks[0].classes.push_back({1,8});banks[1].classes.push_back({2,9});
 const auto r=Monstey_GadbSelectPointerClass(banks,7,{9,1,0},Monstey_Policy);
 Monstey_Check(r.status==Monstey_GadbPointerSelectionStatus::ClassNotFound&&r.bank_index==0u,"class miss no later bank fallback");
}
void Monstey_Epochs(){
 ++Monstey_Groups;
 {auto b=Monstey_Banks();b[0].classes={{1,10},{2,20}};Monstey_GadbPointerCategory c{30,1,0};
  const auto r=Monstey_GadbSelectPointerClass(b,100,c,[&](U64 l,U64 q){if(l==10){Monstey_Check(q==30,"initial query");c.name_pointer=20;return 1;}return Monstey_Policy(l,q);});
  Monstey_Check(r.class_identity==2ull&&r.class_comparisons==2,"query pointer reloaded per row");++Monstey_Cases;}
 {auto b=Monstey_Banks();b[0].classes={{1,10},{2,20}};Monstey_GadbPointerCategory c{20,1,0};
  const auto r=Monstey_GadbSelectPointerClass(b,100,c,[&](U64 l,U64 q){if(l==10){c.record_count=0;c.anonymous_count=0;}return Monstey_Policy(l,q);});
  Monstey_Check(r.class_identity==2ull,"count gate not rechecked during search");++Monstey_Cases;}
 for(bool gate_after:{false,true}){
  auto b=Monstey_Banks();b[0].classes={{9,7}};Monstey_GadbPointerCategory c{7,gate_after?0u:1u,0};
  const auto r=Monstey_GadbSelectPointerClass(b,100,c,[&](U64 l,U64 q){if(l==100)c.record_count=gate_after?1u:0u;return Monstey_Policy(l,q);});
  Monstey_Check(r.status==(gate_after?Monstey_GadbPointerSelectionStatus::ClassFound:Monstey_GadbPointerSelectionStatus::CategorySkipped),"count gate after bank comparison");++Monstey_Cases;
 }
 {auto b=Monstey_Banks();b[0].classes={{1,10}};
  const auto r=Monstey_GadbSelectPointerClass(b,100,{20,1,0},[&](U64 l,U64 q){if(l==10)b[0].classes.push_back({2,20});return Monstey_Policy(l,q);});
  Monstey_Check(r.status==Monstey_GadbPointerSelectionStatus::ClassNotFound&&r.class_comparisons==1,"enumerator count frozen");++Monstey_Cases;}
 {auto b=Monstey_Banks();b[0].classes={{1,10},{2,20}};
  Monstey_Throws<std::out_of_range>([&]{Monstey_GadbSelectPointerClass(b,100,{20,1,0},[&](U64 l,U64 q){if(l==10)b[0].classes.clear();return Monstey_Policy(l,q);});});++Monstey_Cases;}
 {auto b=Monstey_Banks();b[0].classes={{1,10}};
  Monstey_Throws<std::out_of_range>([&]{Monstey_GadbSelectPointerClass(b,100,{10,1,0},[&](U64 l,U64 q){if(l==10)b[0].classes.clear();return Monstey_Policy(l,q);});});++Monstey_Cases;}
 for(U64 late:{0ull,0xfedcba9876543210ull}){
  auto b=Monstey_Banks();b[0].classes={{1,10}};
  const auto r=Monstey_GadbSelectPointerClass(b,100,{10,1,0},[&](U64 l,U64 q){if(l==10)b[0].classes[0].identity=late;return Monstey_Policy(l,q);});
  Monstey_Check(r.status==Monstey_GadbPointerSelectionStatus::ClassFound&&r.class_identity==late,"late matched identity reload");++Monstey_Cases;
 }
 {auto b=Monstey_Banks();b[0].classes={{0,0}};U32 calls=0;
  Monstey_Throws<std::out_of_range>([&]{Monstey_GadbSelectPointerClass(b,100,{0,1,0},[&](U64 a,U64 z){++calls;return Monstey_Policy(a,z);});});
  Monstey_Check(calls==1,"null class rejects before name compare");++Monstey_Cases;}
 {auto b=Monstey_Banks();b[0].classes={{1,10},{2,20}};U64 original_query=30;
  const auto r=Monstey_GadbSelectPointerClass(b,100,original_query,1,0,[&](U64 l,U64 q){if(l==10)original_query=20;return Monstey_Policy(l,q);});
  Monstey_Check(original_query==20&&r.status==Monstey_GadbPointerSelectionStatus::ClassNotFound,"snapshot convenience explicit");++Monstey_Cases;}
}
void Monstey_Errors(){
 ++Monstey_Groups;
 auto b=Monstey_Banks();
 for(std::size_t n=0;n<6;++n){++Monstey_Cases;Monstey_Throws<std::invalid_argument>([&]{Monstey_GadbSelectPointerClass(std::span(b).first(n),0,{0,0,0},{});});}
 std::array<Monstey_GadbPointerBankNames,7> seven{};
 Monstey_Throws<std::invalid_argument>([&]{Monstey_GadbSelectPointerClass(seven,0,{0,0,0},{});});
 Monstey_Throws<std::invalid_argument>([&]{Monstey_GadbSelectPointerClass(b,100,{0,0,0},{});});
 b[0].name_pointer=0;b[0].classes={{1,0}};
 const auto null=Monstey_GadbSelectPointerClass(b,0,{0,1,0},{});
 Monstey_Check(null.status==Monstey_GadbPointerSelectionStatus::ClassFound&&null.class_identity==1ull,"all null names need no policy");
 auto throw_banks=Monstey_Banks();throw_banks[0].classes={{1,10}};Monstey_GadbPointerCategory c{10,1,0};
 Monstey_Throws<std::logic_error>([&]{Monstey_GadbSelectPointerClass(throw_banks,100,c,[&](U64 l,U64 q){if(l==10){c.name_pointer=99;throw std::logic_error("policy");}return Monstey_Policy(l,q);});});
 Monstey_Check(c.name_pointer==99,"external side effects retained on exception");
 Monstey_Cases+=4;
}
}
int main(){try{Monstey_Compare();Monstey_Search();Monstey_Epochs();Monstey_Errors();
 std::cout<<"PASS independent_pointer groups="<<Monstey_Groups<<" cases="<<Monstey_Cases<<" checks="<<Monstey_Checks<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL at "<<Monstey_Checks<<": "<<e.what()<<"\n";return 1;}}