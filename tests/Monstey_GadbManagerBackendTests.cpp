#include "Monstey_GadbManagerBackend.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
using namespace Monstey;
namespace {
using Monstey_U64=std::uint64_t;
using Monstey_U32=std::uint32_t;
std::size_t Monstey_Cases=0,Monstey_Checks=0,Monstey_Exits=0,Monstey_Images=0;
void Monstey_Check(bool Monstey_V,const char *Monstey_M){++Monstey_Checks;if(!Monstey_V)throw std::runtime_error(Monstey_M);}
void Monstey_Put(std::vector<std::uint8_t>&Monstey_I,Monstey_U64 Monstey_B,Monstey_U64 Monstey_A,Monstey_U64 Monstey_V,std::size_t Monstey_N=8){
 if(Monstey_A<Monstey_B||Monstey_A-Monstey_B>Monstey_I.size()||Monstey_N>Monstey_I.size()-static_cast<std::size_t>(Monstey_A-Monstey_B))throw std::runtime_error("Test primitive range");
 for(std::size_t Monstey_J=0;Monstey_J<Monstey_N;++Monstey_J)Monstey_I[static_cast<std::size_t>(Monstey_A-Monstey_B)+Monstey_J]=static_cast<std::uint8_t>(Monstey_V>>(8u*Monstey_J));}
Monstey_U64 Monstey_Get(const std::vector<std::uint8_t>&Monstey_I,Monstey_U64 Monstey_B,Monstey_U64 Monstey_A,std::size_t Monstey_N=8){
 Monstey_U64 Monstey_V=0;for(std::size_t Monstey_J=0;Monstey_J<Monstey_N;++Monstey_J)Monstey_V|=Monstey_U64(Monstey_I.at(static_cast<std::size_t>(Monstey_A-Monstey_B)+Monstey_J))<<(8u*Monstey_J);return Monstey_V;}
struct Monstey_Event{unsigned kind;Monstey_GadbManagerBackendImportSite site;Monstey_U64 a,b,c,d;};
struct Monstey_Fixture{
 Monstey_U64 base,body,backing;unsigned mode;std::vector<std::uint8_t> image;
 Monstey_GadbManagerBackendBindings bindings;Monstey_GadbManagerBackendServices services;
 std::vector<Monstey_Event> events;std::function<void(unsigned)> hook;
 Monstey_U64 allocation,alloc_rdx=0xF123456789ABCDEFULL,fill_rdx=0xFEDCBA9876543210ULL,leave_rdx=0xAABBCCDDEEFF9911ULL;
 Monstey_U32 page=0x1000,granularity=0x1000,random=0xFFFFFFFDu;unsigned cas=0,exchanges=0;
 Monstey_GadbBinderMemory memory(){return{base,image};}
 Monstey_GadbManagerBackendModeCells&cells(){return mode==0?bindings.mode_40:mode==1?bindings.mode_80:bindings.mode_c0;}
 Monstey_U64&tail(){return mode==0?bindings.placement.tail_40:mode==1?bindings.placement.tail_80:bindings.placement.tail_c0;}
 void put(Monstey_U64 Monstey_A,Monstey_U64 Monstey_V,std::size_t Monstey_N=8){Monstey_Put(image,base,Monstey_A,Monstey_V,Monstey_N);}
 Monstey_U64 get(Monstey_U64 Monstey_A,std::size_t Monstey_N=8)const{return Monstey_Get(image,base,Monstey_A,Monstey_N);}
 void event(unsigned Monstey_K,const Monstey_GadbManagerBackendImportSite&Monstey_S,Monstey_U64 Monstey_A=0,Monstey_U64 Monstey_B=0,Monstey_U64 Monstey_C=0,Monstey_U64 Monstey_D=0){events.push_back({Monstey_K,Monstey_S,Monstey_A,Monstey_B,Monstey_C,Monstey_D});if(hook)hook(Monstey_K);}
 explicit Monstey_Fixture(unsigned Monstey_M,Monstey_U64 Monstey_Base=0):base(Monstey_Base),body(base+0x3000),backing(base+0x10000),mode(Monstey_M),image(0x30000,0xA5),allocation(backing){
 bindings.imports={base+0x100,base+0x108,base+0x110,base+0x118,base+0x120,base+0x128,base+0x130,base+0x138};
 bindings.placement={base+0x1800,base+0x1808,base+0x1810,base+0x1900};
 bindings.mode_40={base+0x1000,base+0x1008,base+0x1010,base+0x1018,base+0x1020,base+0x1028,base+0x1030,base+0x1060,base+0x1070};
 bindings.mode_80={base+0x1100,base+0x1108,base+0x1110,base+0x1118,base+0x1120,base+0x1128,base+0x1130,base+0x1160,base+0x1170};
 bindings.mode_c0={base+0x1200,base+0x1208,base+0x1210,base+0x1218,base+0x1220,base+0x1228,base+0x1230,base+0x1260,base+0x1270};
 for(unsigned Monstey_J=0;Monstey_J<8;++Monstey_J)put(base+0x100+8u*Monstey_J,0x770000+Monstey_J);
 for(unsigned Monstey_J=0;Monstey_J<6;++Monstey_J)put(base+0x1900+4u*Monstey_J,Monstey_J,4);
 put(cells().cookie,0x12345678);put(cells().page,page);put(cells().granularity,granularity);put(cells().flags,7,4);put(cells().state,1,4);put(tail(),0xDDAABBCC11223344ULL);
 services.fill=[this](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendFillCall&Monstey_C){event(0,Monstey_C.site,Monstey_C.destination,Monstey_C.count,Monstey_C.value);for(Monstey_U64 Monstey_J=0;Monstey_J<Monstey_C.count;++Monstey_J)put(Monstey_C.destination+Monstey_J,Monstey_C.value&255u,1);return Monstey_GadbManagerBackendRdxResult{fill_rdx};};
 services.allocate=[this](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendAllocateCall&Monstey_C){event(1,Monstey_C.site,Monstey_C.address,Monstey_C.size,Monstey_C.allocation_type,Monstey_C.protection);return Monstey_GadbBufferedResult{allocation,alloc_rdx};};
 services.system_info=[this](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendSystemInfoCall&Monstey_C){event(2,Monstey_C.site,Monstey_C.output,Monstey_C.output_bytes);for(unsigned Monstey_J=0;Monstey_J<48;++Monstey_J)put(Monstey_C.output+Monstey_J,0x70u+Monstey_J,1);put(Monstey_C.output+4,page,4);put(Monstey_C.output+40,granularity,4);};
 services.sleep=[this](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendSleepCall&Monstey_C){event(3,Monstey_C.site,Monstey_C.milliseconds,Monstey_C.alertable);put(cells().state,1,4);};
 services.random=[this](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendImportSite&Monstey_C){event(4,Monstey_C);return random;};
 services.initialize_cs=[this](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendInitializeCsCall&Monstey_C){event(5,Monstey_C.site,Monstey_C.critical_section,Monstey_C.spin);return Monstey_GadbManagerBackendRdxResult{0x8765432101234567ULL};};
 services.enter_cs=[this](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendSyncCall&Monstey_C){event(6,Monstey_C.site,Monstey_C.critical_section);};
 services.leave_cs=[this](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendSyncCall&Monstey_C){event(7,Monstey_C.site,Monstey_C.critical_section);return Monstey_GadbManagerBackendRdxResult{leave_rdx};};
 services.exchange32=[this](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendExchange32Call&Monstey_C){++exchanges;Monstey_Check(Monstey_C.value==1&&Monstey_C.address==cells().state,"XCHG target/value");const auto Monstey_Old=static_cast<Monstey_U32>(get(Monstey_C.address,4));put(Monstey_C.address,Monstey_C.value,4);return Monstey_Old;};
 services.placement.compare_exchange32=[this](Monstey_GadbBinderMemory,const Monstey_GadbManagerCompareExchange32Call&Monstey_C){++cas;Monstey_Check(Monstey_C.desired==0xFFFFFFFFu&&Monstey_C.native_site==0x14059D5F3ULL,"CAS desired/site");const auto Monstey_Old=static_cast<Monstey_U32>(get(Monstey_C.address,4));if(Monstey_Old==Monstey_C.comparand)put(Monstey_C.address,Monstey_C.desired,4);return Monstey_Old;};
 }
 Monstey_GadbBufferedResult root(Monstey_U64 Monstey_A,Monstey_U64 Monstey_N,Monstey_U64 Monstey_F=0x1234567800300010ULL){if(mode==0)return Monstey_GadbCreateManagerBackend40(memory(),Monstey_F,Monstey_A,Monstey_N,body,services,bindings);if(mode==1)return Monstey_GadbCreateManagerBackend80(memory(),Monstey_F,Monstey_A,Monstey_N,body,services,bindings);return Monstey_GadbCreateManagerBackendC0(memory(),Monstey_F,Monstey_A,Monstey_N,body,services,bindings);}
 Monstey_GadbBufferedResult factory(Monstey_U64 Monstey_D=0){if(mode==0)return Monstey_GadbAllocateManagerBackend40(memory(),Monstey_D,body,0x8877665544332211ULL,services,bindings);if(mode==1)return Monstey_GadbAllocateManagerBackend80(memory(),Monstey_D,body,0x8877665544332211ULL,services,bindings);return Monstey_GadbAllocateManagerBackendC0(memory(),Monstey_D,body,0x8877665544332211ULL,services,bindings);}
 Monstey_GadbBufferedResult init(){if(mode==0)return Monstey_GadbInitializeManagerBackend40(memory(),body,services,bindings);if(mode==1)return Monstey_GadbInitializeManagerBackend80(memory(),body,services,bindings);return Monstey_GadbInitializeManagerBackendC0(memory(),body,services,bindings);}
 Monstey_GadbManagerTailResult place(Monstey_U64 Monstey_A,Monstey_U64 Monstey_N){if(mode==0)return Monstey_GadbPlaceManagerBackend40(memory(),Monstey_A,Monstey_N,body,services,bindings);if(mode==1)return Monstey_GadbPlaceManagerBackend80(memory(),Monstey_A,Monstey_N,body,services,bindings);return Monstey_GadbPlaceManagerBackendC0(memory(),Monstey_A,Monstey_N,body,services,bindings);}
 void clear(unsigned Monstey_K){switch(Monstey_K){case 0:services.fill={};break;case 1:services.allocate={};break;case 2:services.system_info={};break;case 3:services.sleep={};break;case 4:services.random={};break;case 5:services.initialize_cs={};break;case 6:services.enter_cs={};break;default:services.leave_cs={};break;}}
};
struct Monstey_Sites{Monstey_U64 rootfill,placefill,alloc,sleep,maincs,enter,info,templatecs,random,leave,placecs,exchange;};
constexpr std::array<Monstey_Sites,3> Monstey_NativeSites{{
 {0x141175FBB,0x141175756,0x1411760D8,0x141175627,0x14117563B,0x141175653,0x14117566C,0x1411756D9,0x1411756DF,0x141175705,0x141175767,0x141175646},
 {0x141174DEF,0x141173BDE,0x141174F28,0x141173A6F,0x141173A83,0x141173A9B,0x141173AB4,0x141173B21,0x141173B27,0x141173B4D,0x141173BEF,0x141173A8E},
 {0x141177EFF,0x141176CEE,0x141178038,0x141176B7F,0x141176B93,0x141176BAB,0x141176BC4,0x141176C32,0x141176C38,0x141176C5E,0x141176CFF,0x141176B9E}}};
void Monstey_Image(Monstey_Fixture&Monstey_F,const std::vector<std::uint8_t>&Monstey_E){++Monstey_Images;Monstey_Check(Monstey_F.image==Monstey_E,"Full owned image mismatch");}
Monstey_U64 Monstey_ExpectedPlace(Monstey_Fixture&Monstey_F,std::vector<std::uint8_t>&Monstey_E,Monstey_U64 Monstey_A,Monstey_U64 Monstey_N,unsigned Monstey_Outer=0,Monstey_U32 Monstey_Flags=0){
 const auto Monstey_O=(Monstey_A+23u)&~Monstey_U64{7};
 auto Monstey_W=[&](Monstey_U64 Monstey_P,Monstey_U64 Monstey_V,std::size_t Monstey_NN=8){Monstey_Put(Monstey_E,Monstey_F.base,Monstey_P,Monstey_V,Monstey_NN);};
 auto Monstey_R=[&](Monstey_U64 Monstey_P,std::size_t Monstey_NN=8){return Monstey_Get(Monstey_E,Monstey_F.base,Monstey_P,Monstey_NN);};
 for(Monstey_U64 Monstey_J=0;Monstey_J<0x3D8;++Monstey_J)Monstey_W(Monstey_O+Monstey_J,0,1);
 Monstey_W(Monstey_O-8,0x3DB);Monstey_W(Monstey_O+0x18,Monstey_A);Monstey_W(Monstey_O+0x3A0,Monstey_A);
 Monstey_W(Monstey_O+0x360,Monstey_N);Monstey_W(Monstey_O+0x358,Monstey_N);Monstey_W(Monstey_O+0x3A8,Monstey_N);
 const auto Monstey_Cookie=Monstey_R(Monstey_F.cells().cookie);Monstey_W(Monstey_O+0x38,~Monstey_U64{0});Monstey_W(Monstey_O+0x40,Monstey_Cookie);
 const auto Monstey_Cached=Monstey_R(Monstey_F.cells().flags,4);Monstey_W(Monstey_O+0x3C0,0);Monstey_W(Monstey_O+0x3C8,0);Monstey_W(Monstey_O+0x370,Monstey_Cached|4u,4);
 for(unsigned Monstey_J=0;Monstey_J<32;++Monstey_J){const auto Monstey_H=Monstey_O+72u+16u*Monstey_J;Monstey_W(Monstey_H+24,Monstey_H);Monstey_W(Monstey_H+16,Monstey_H);}
 const auto Monstey_Q=Monstey_O+0x3C8u;const auto Monstey_Rem=Monstey_N-(Monstey_Q-Monstey_A)-0x48u;
 Monstey_W(Monstey_O+0x28,Monstey_Q);Monstey_W(Monstey_O+0x10,Monstey_Rem);Monstey_W(Monstey_Q+8,Monstey_Rem|1u);Monstey_W(Monstey_A+Monstey_N-0x40u,0x48);Monstey_W(Monstey_O+0x30,Monstey_R(Monstey_F.tail()));
 if(Monstey_Outer){Monstey_W(Monstey_O+0x370,Monstey_R(Monstey_O+0x370,4)|2u|Monstey_Flags,4);Monstey_W(Monstey_O+0x3B8,Monstey_Outer,4);}return Monstey_O;}
void Monstey_ExpectedInit(Monstey_Fixture&Monstey_F,std::vector<std::uint8_t>&Monstey_E,Monstey_U64 Monstey_B){
 auto Monstey_W=[&](Monstey_U64 Monstey_P,Monstey_U64 Monstey_V,std::size_t Monstey_N=8){Monstey_Put(Monstey_E,Monstey_F.base,Monstey_P,Monstey_V,Monstey_N);};
 for(unsigned Monstey_J=0;Monstey_J<48;++Monstey_J)Monstey_W(Monstey_B+0x20+Monstey_J,0x70u+Monstey_J,1);
 Monstey_W(Monstey_B+0x24,Monstey_F.page,4);Monstey_W(Monstey_B+0x48,Monstey_F.granularity,4);
 if((Monstey_F.page&(Monstey_F.page-1u))||(Monstey_F.granularity&(Monstey_F.granularity-1u)))Monstey_W(0,0,4);
 Monstey_W(Monstey_F.cells().granularity,Monstey_F.granularity);Monstey_W(Monstey_F.cells().auxiliary,Monstey_F.mode==2?0x400000u:~Monstey_U64{0});
 Monstey_W(Monstey_F.tail(),~Monstey_U64{0});Monstey_W(Monstey_F.cells().page,Monstey_F.page);Monstey_W(Monstey_F.cells().flags,7,4);Monstey_W(Monstey_F.cells().secondary_flags,7,4);
 const auto Monstey_R=Monstey_U64(Monstey_F.random)|(Monstey_F.random&0x80000000u?0xFFFFFFFF00000000ULL:0);
 Monstey_W(Monstey_F.cells().cookie,(((Monstey_R>>3u)<<3u)^Monstey_U64{0x55555550})|8u);}
void Monstey_Normal(unsigned Monstey_M){
 for(unsigned Monstey_J=0;Monstey_J<8;++Monstey_J){++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);auto Monstey_E=Monstey_F.image;const auto Monstey_A=Monstey_F.backing+Monstey_J;const auto Monstey_O=Monstey_ExpectedPlace(Monstey_F,Monstey_E,Monstey_A,0x2000);const auto Monstey_R=Monstey_F.place(Monstey_A,0x2000);Monstey_Check(Monstey_R.raw.rax==Monstey_O&&Monstey_R.raw.rdx==Monstey_O+0x3C8,"Placement O/q");Monstey_Check(Monstey_R.remaining==0x2000-(Monstey_O+0x3C8-Monstey_A)-0x48,"Placement remaining");Monstey_Check(Monstey_F.events[0].b==0x3D8&&Monstey_F.events[1].a==Monstey_O+0x378,"Placement DTO");Monstey_Check(Monstey_F.events[0].site.body_rsp==Monstey_F.body-8&&Monstey_F.events[0].site.native_site==0x1408C4EE0ULL,"Fill thunk frame");Monstey_Image(Monstey_F,Monstey_E);}Monstey_Exits+=2;
 for(unsigned Monstey_J=0;Monstey_J<3;++Monstey_J){++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);if(Monstey_J){Monstey_F.put(Monstey_F.cells().cookie,0);if(Monstey_J==2)Monstey_F.page=3;}auto Monstey_E=Monstey_F.image;if(Monstey_J)Monstey_ExpectedInit(Monstey_F,Monstey_E,Monstey_F.body);const auto Monstey_R=Monstey_F.init();Monstey_Check(Monstey_R.rax==1&&Monstey_R.rdx==Monstey_F.leave_rdx,"Init final words");Monstey_Check(Monstey_F.events.front().kind==6&&Monstey_F.events.back().kind==7,"Direct Enter/Leave");Monstey_Image(Monstey_F,Monstey_E);}Monstey_Exits+=3;
 for(unsigned Monstey_J=0;Monstey_J<5;++Monstey_J){++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);if(Monstey_J<2)Monstey_F.put(Monstey_F.cells().page,0u-Monstey_U64{0x420});if(Monstey_J==1){Monstey_F.put(Monstey_F.cells().cookie,0);Monstey_F.hook=[&](unsigned Monstey_K){if(Monstey_K==6)Monstey_F.put(Monstey_F.cells().cookie,1);};}if(Monstey_J==2)Monstey_F.allocation=0;if(Monstey_J==3)Monstey_F.allocation=~Monstey_U64{0};auto Monstey_E=Monstey_F.image;if(Monstey_J==1)Monstey_Put(Monstey_E,Monstey_F.base,Monstey_F.cells().cookie,1);Monstey_U64 Monstey_O=0;if(Monstey_J==4)Monstey_O=Monstey_ExpectedPlace(Monstey_F,Monstey_E,Monstey_F.backing,0x1000,1);const auto Monstey_R=Monstey_F.factory();const auto Monstey_D=Monstey_J==0?0x8877665544332211ULL:Monstey_J==1?Monstey_F.leave_rdx:Monstey_J<4?Monstey_F.alloc_rdx:Monstey_O+0x3C8;Monstey_Check(Monstey_R.rax==Monstey_O&&Monstey_R.rdx==Monstey_D,"Factory normal Raw");if(Monstey_J>=2)Monstey_Check(Monstey_F.events[0].a==0&&Monstey_F.events[0].b==0x1000&&Monstey_F.events[0].c==0x3000&&Monstey_F.events[0].d==4,"Allocation DTO");Monstey_Image(Monstey_F,Monstey_E);}Monstey_Exits+=5;
 for(unsigned Monstey_J=0;Monstey_J<7;++Monstey_J){++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);const Monstey_U64 Monstey_Flags=0x1234567800300010ULL;const auto Monstey_A=Monstey_J<2?Monstey_U64{0}:Monstey_F.backing;const auto Monstey_N=Monstey_J<4?Monstey_U64{0x420}:Monstey_J<6?~Monstey_U64{0}:Monstey_U64{0x2000};if(Monstey_J==0)Monstey_F.put(Monstey_F.cells().page,0u-Monstey_U64{0x420});if(Monstey_J==3||Monstey_J==5)Monstey_F.put(Monstey_F.cells().cookie,0);auto Monstey_E=Monstey_F.image;Monstey_U64 Monstey_O=0,Monstey_D=Monstey_Flags;
 if(Monstey_J<2){for(Monstey_U64 Monstey_I=0;Monstey_I<0x3D0;++Monstey_I)Monstey_Put(Monstey_E,Monstey_F.base,Monstey_F.body+0x20+Monstey_I,0,1);Monstey_Put(Monstey_E,Monstey_F.base,Monstey_F.body+0x390,static_cast<Monstey_U32>(Monstey_Flags),4);Monstey_D=Monstey_F.fill_rdx;if(Monstey_J==1){Monstey_O=Monstey_ExpectedPlace(Monstey_F,Monstey_E,Monstey_F.backing,0x1000,1,static_cast<Monstey_U32>(Monstey_Flags));Monstey_D=Monstey_O+0x3C8;}}
 else if(Monstey_J==3||Monstey_J==5){Monstey_ExpectedInit(Monstey_F,Monstey_E,Monstey_F.body-0x60);Monstey_D=Monstey_F.leave_rdx;}
 if(Monstey_J==6){Monstey_O=Monstey_ExpectedPlace(Monstey_F,Monstey_E,Monstey_A,Monstey_N,8,static_cast<Monstey_U32>(Monstey_Flags));Monstey_D=Monstey_O+0x3C8;}
 const auto Monstey_R=Monstey_F.root(Monstey_A,Monstey_N,Monstey_Flags);Monstey_Check(Monstey_R.rax==Monstey_O&&Monstey_R.rdx==Monstey_D,"Root full Raw/flags");if(Monstey_J==1)Monstey_Check(Monstey_F.events[2].site.body_rsp==Monstey_F.body-0x68,"Root factory placement frame");Monstey_Image(Monstey_F,Monstey_E);}Monstey_Exits+=7;
}
void Monstey_InvokeBoundary(Monstey_Fixture&Monstey_F,unsigned Monstey_K){
 if(Monstey_K==0||Monstey_K==5)static_cast<void>(Monstey_F.place(Monstey_F.backing,0x2000));
 else if(Monstey_K==1)static_cast<void>(Monstey_F.factory());
 else{if(Monstey_K==2||Monstey_K==4)Monstey_F.put(Monstey_F.cells().cookie,0);if(Monstey_K==3)Monstey_F.put(Monstey_F.cells().state,0xFFFFFFFFu,4);static_cast<void>(Monstey_F.init());}}
void Monstey_Boundaries(unsigned Monstey_M){
 const auto&Monstey_S=Monstey_NativeSites[Monstey_M];const std::array<Monstey_U64,8>Monstey_PCs{Monstey_S.placefill,Monstey_S.alloc,Monstey_S.info,Monstey_S.sleep,Monstey_S.random,Monstey_S.placecs,Monstey_S.enter,Monstey_S.leave};
 for(unsigned Monstey_K=0;Monstey_K<8;++Monstey_K){
 ++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.clear(Monstey_K);bool Monstey_Hit=false;
 try{Monstey_InvokeBoundary(Monstey_F,Monstey_K);}catch(const Monstey_GadbManagerBackendImportFault&Monstey_X){Monstey_Hit=true;Monstey_Check(static_cast<unsigned>(Monstey_X.kind)==Monstey_K,"Missing import kind");Monstey_Check(Monstey_X.site.call_site==Monstey_PCs[Monstey_K],"Missing callsite");Monstey_Check(Monstey_X.site.native_site==(Monstey_K==0?0x1408C4EE0ULL:Monstey_PCs[Monstey_K]),"Missing native site");Monstey_Check(Monstey_X.site.body_rsp==Monstey_F.body-(Monstey_K==0?8u:0u),"Missing exact body");Monstey_Check(Monstey_X.site.cell_identity==Monstey_F.base+0x100+8u*Monstey_K&&Monstey_X.site.target_identity==0x770000+Monstey_K,"Missing cell/target");}
 Monstey_Check(Monstey_Hit,"Typed import fault not reached");
 ++Monstey_Cases;Monstey_Fixture Monstey_Z(Monstey_M);Monstey_Z.put(Monstey_Z.base+0x100+8u*Monstey_K,0);Monstey_InvokeBoundary(Monstey_Z,Monstey_K);
 const auto Monstey_E=std::find_if(Monstey_Z.events.begin(),Monstey_Z.events.end(),[=](const Monstey_Event&Monstey_V){return Monstey_V.kind==Monstey_K;});
 Monstey_Check(Monstey_E!=Monstey_Z.events.end()&&Monstey_E->site.target_identity==0,"Zero target must dispatch");}
 for(unsigned Monstey_K=0;Monstey_K<8;++Monstey_K){++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);int Monstey_Token=42;
 Monstey_F.hook=[&](unsigned Monstey_E){if(Monstey_E==Monstey_K){Monstey_F.put(Monstey_F.base+0x2500,0xFACE1234,4);throw &Monstey_Token;}};
 bool Monstey_Hit=false;try{Monstey_InvokeBoundary(Monstey_F,Monstey_K);}catch(int*Monstey_X){Monstey_Hit=Monstey_X==&Monstey_Token;}
 Monstey_Check(Monstey_Hit&&Monstey_F.get(Monstey_F.base+0x2500,4)==0xFACE1234,"Provider exception identity/partial effect");Monstey_Check(Monstey_F.events.back().kind==Monstey_K,"No invented cleanup callback");}
}
void Monstey_AtomicAndFaults(unsigned Monstey_M){
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.put(Monstey_F.cells().state,0,4);auto Monstey_E=Monstey_F.image;
 Monstey_Put(Monstey_E,Monstey_F.base,Monstey_F.body+0x60,0,4);Monstey_Put(Monstey_E,Monstey_F.base,Monstey_F.body-0x20,5,4);Monstey_Put(Monstey_E,Monstey_F.base,Monstey_F.cells().state,1,4);
 const auto Monstey_R=Monstey_F.init();Monstey_Check(Monstey_R.rax==1&&Monstey_F.cas==1&&Monstey_F.exchanges==1,"Real CAS then XCHG once");
 Monstey_Check(Monstey_F.events[0].site.call_site==Monstey_NativeSites[Monstey_M].maincs,"Main CS site");Monstey_Image(Monstey_F,Monstey_E);}
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.put(Monstey_F.cells().state,0,4);Monstey_F.services.exchange32={};bool Monstey_Hit=false;
 try{static_cast<void>(Monstey_F.init());}catch(const Monstey_GadbManagerBackendExchange32Fault&Monstey_X){Monstey_Hit=Monstey_X.native_site==Monstey_NativeSites[Monstey_M].exchange&&Monstey_X.address==Monstey_F.cells().state&&Monstey_X.value==1&&Monstey_X.width==4;}
 Monstey_Check(Monstey_Hit&&Monstey_F.get(Monstey_F.cells().state,4)==0xFFFFFFFFu&&Monstey_F.events.size()==1,"Missing XCHG retained prefix");}
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.put(Monstey_F.cells().state,0,4);Monstey_F.services.placement.compare_exchange32={};bool Monstey_Hit=false;
 try{static_cast<void>(Monstey_F.init());}catch(const Monstey_GadbManagerCompareExchange32Fault&Monstey_X){Monstey_Hit=Monstey_X.native_site==0x14059D5F3ULL&&Monstey_X.address==Monstey_F.cells().state&&Monstey_X.comparand==0&&Monstey_X.desired==0xFFFFFFFFu;}
 Monstey_Check(Monstey_Hit&&Monstey_F.get(Monstey_F.body-0x20,4)==5&&Monstey_F.get(Monstey_F.body+0x60,4)==0&&Monstey_F.events.empty(),"Nested CAS missing prefix");}
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.cells().cookie=Monstey_F.base+Monstey_F.image.size()-7;auto Monstey_E=Monstey_F.image;bool Monstey_Hit=false;
 try{static_cast<void>(Monstey_F.factory());}catch(const Monstey_GadbBufferedMemoryFault&Monstey_X){Monstey_Hit=Monstey_X.address==Monstey_F.cells().cookie&&Monstey_X.width==8&&!Monstey_X.writing;}
 Monstey_Check(Monstey_Hit&&Monstey_F.events.empty(),"Fullwidth first cookie READ");Monstey_Image(Monstey_F,Monstey_E);}
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.bindings.imports.fill=~Monstey_U64{3};Monstey_F.services.fill={};bool Monstey_Hit=false;
 try{static_cast<void>(Monstey_F.place(Monstey_F.backing,0x2000));}catch(const Monstey_GadbBufferedMemoryFault&Monstey_X){Monstey_Hit=Monstey_X.address==~Monstey_U64{3}&&Monstey_X.width==8&&!Monstey_X.writing;}
 Monstey_Check(Monstey_Hit,"IAT range fault before absent service");}
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M,0x100000);Monstey_F.put(Monstey_F.cells().cookie,0);Monstey_F.page=3;bool Monstey_Hit=false;
 try{static_cast<void>(Monstey_F.init());}catch(const Monstey_GadbBufferedMemoryFault&Monstey_X){Monstey_Hit=Monstey_X.address==0&&Monstey_X.width==4&&!Monstey_X.writing;}
 Monstey_Check(Monstey_Hit&&Monstey_F.get(Monstey_F.cells().cookie)==0&&Monstey_F.events.back().kind==2,"Invalidbit literal0 READ first");}
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.services.fill=[](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendFillCall&){return Monstey_GadbManagerBackendRdxResult{0x123};};bool Monstey_Hit=false;
 try{static_cast<void>(Monstey_F.place(0x2FFF0,0x2000));}catch(const Monstey_GadbBufferedMemoryFault&Monstey_X){Monstey_Hit=Monstey_X.address==0x30018&&Monstey_X.width==8&&Monstey_X.writing;}
 Monstey_Check(Monstey_Hit&&Monstey_F.get(0x2FFF8)==0x3DB,"Header prefix before later WRITE fault");}
}
void Monstey_AliasesAndArithmetic(unsigned Monstey_M){
 for(unsigned Monstey_J=0;Monstey_J<5;++Monstey_J){++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);const auto Monstey_O=Monstey_F.backing+16;
 if(Monstey_J==0)Monstey_F.cells().cookie=Monstey_O+0x18;
 if(Monstey_J==1)Monstey_F.cells().flags=Monstey_O+0x370;
 if(Monstey_J==2)Monstey_F.tail()=Monstey_F.backing+0x2000-0x40;
 if(Monstey_J==3)Monstey_F.tail()=Monstey_O+0x10;
 if(Monstey_J==4)Monstey_F.bindings.imports.initialize_cs=Monstey_O+0x378;
 auto Monstey_E=Monstey_F.image;Monstey_ExpectedPlace(Monstey_F,Monstey_E,Monstey_F.backing,0x2000);static_cast<void>(Monstey_F.place(Monstey_F.backing,0x2000));Monstey_Image(Monstey_F,Monstey_E);
 if(Monstey_J==4)Monstey_Check(Monstey_F.events[1].site.target_identity==0,"Live CS IAT after fill alias");}
 for(const auto Monstey_G:std::array<Monstey_U64,5>{0,3,0x1000,0x8000000000000001ULL,~Monstey_U64{0}})for(unsigned Monstey_P=0;Monstey_P<3;++Monstey_P){
 ++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.put(Monstey_F.cells().granularity,Monstey_G);Monstey_F.allocation=0;
 const auto Monstey_D=Monstey_F.base+0x2000;Monstey_F.put(Monstey_D+0x370,Monstey_P==1?0x100000:Monstey_P==2?0x200000:0,4);static_cast<void>(Monstey_F.factory(Monstey_D));
 const auto Monstey_Span=(Monstey_G+Monstey_G-1u)&(0u-Monstey_G);Monstey_Check(Monstey_F.events[0].b==Monstey_Span&&Monstey_F.events[0].d==(Monstey_P==1?2u:4u),"Modular span/protection");}
 for(const auto Monstey_N:std::array<Monstey_U64,4>{0,1,0x10,0x420}){++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);auto Monstey_E=Monstey_F.image;const auto Monstey_O=Monstey_ExpectedPlace(Monstey_F,Monstey_E,Monstey_F.backing+3,Monstey_N);const auto Monstey_R=Monstey_F.place(Monstey_F.backing+3,Monstey_N);Monstey_Check(Monstey_R.raw.rax==Monstey_O,"Short modular span accepted");Monstey_Image(Monstey_F,Monstey_E);}
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.put(Monstey_F.cells().cookie,0);Monstey_F.page=0;Monstey_F.granularity=0;auto Monstey_E=Monstey_F.image;Monstey_ExpectedInit(Monstey_F,Monstey_E,Monstey_F.body);static_cast<void>(Monstey_F.init());Monstey_Image(Monstey_F,Monstey_E);}
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.put(Monstey_F.cells().cookie,0);Monstey_F.hook=[&](unsigned Monstey_K){if(Monstey_K==6)Monstey_F.put(Monstey_F.bindings.imports.leave_cs,0xCAFEBABE);};static_cast<void>(Monstey_F.init());Monstey_Check(Monstey_F.events.back().site.target_identity==0xCAFEBABE,"Late live Leave target");}
}
struct Monstey_LifetimeToken{bool*retired;~Monstey_LifetimeToken(){*retired=true;}};
void Monstey_Lifetime(unsigned Monstey_M){
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.put(Monstey_F.cells().state,0xFFFFFFFFu,4);bool Monstey_Retired=false;auto Monstey_T=std::make_shared<Monstey_LifetimeToken>();Monstey_T->retired=&Monstey_Retired;unsigned Monstey_Stage=0;
 Monstey_F.services.sleep=[&,Monstey_T](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendSleepCall&){++Monstey_Stage;Monstey_F.services.sleep=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendSleepCall&){Monstey_Check(Monstey_Retired,"Sleep old target retirement before next call");++Monstey_Stage;Monstey_F.put(Monstey_F.cells().state,1,4);};Monstey_Check(!Monstey_Retired,"Active Sleep retained");};Monstey_T.reset();static_cast<void>(Monstey_F.init());Monstey_Check(Monstey_Stage==2&&Monstey_Retired,"No retry limit/current Sleep replacement");}
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.put(Monstey_F.cells().cookie,0);bool Monstey_Retired=false;auto Monstey_T=std::make_shared<Monstey_LifetimeToken>();Monstey_T->retired=&Monstey_Retired;
 Monstey_F.services.random=[&,Monstey_T](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendImportSite&){Monstey_F.services.random={};Monstey_Check(!Monstey_Retired,"Active Rand retained");return Monstey_U32{0x80000001};};Monstey_T.reset();
 Monstey_F.services.leave_cs=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendSyncCall&){Monstey_Check(Monstey_Retired,"Rand retirement before Leave");return Monstey_GadbManagerBackendRdxResult{91};};
 const auto Monstey_R=Monstey_F.init();Monstey_Check(Monstey_R.rax==1&&Monstey_R.rdx==91&&Monstey_F.get(Monstey_F.cells().cookie)==0xFFFFFFFFD5555558ULL,"Rand signed32/Leave");}
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.put(Monstey_F.cells().state,0xFFFFFFFFu,4);unsigned Monstey_Reentry=0;
 Monstey_F.services.sleep=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendSleepCall&){++Monstey_Reentry;Monstey_F.put(Monstey_F.cells().state,1,4);Monstey_F.body+=0x100;const auto Monstey_R=Monstey_F.init();Monstey_Check(Monstey_R.rax==1,"Nested initializer");Monstey_F.body-=0x100;};
 static_cast<void>(Monstey_F.init());Monstey_Check(Monstey_Reentry==1&&Monstey_F.events.size()==4,"Reentrant Enter/Leave");Monstey_Check(Monstey_F.get(Monstey_F.body+0x60,4)==1,"Retry second live state local");}
 {++Monstey_Cases;Monstey_Fixture Monstey_F(Monstey_M);Monstey_F.put(Monstey_F.cells().state,0,4);unsigned Monstey_Calls=0;
 Monstey_F.services.placement.compare_exchange32=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerCompareExchange32Call&Monstey_C){++Monstey_Calls;Monstey_Check(Monstey_C.comparand==0,"CAS expected0");Monstey_F.services.placement.compare_exchange32=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerCompareExchange32Call&Monstey_D){++Monstey_Calls;Monstey_F.put(Monstey_D.address,Monstey_D.desired,4);return Monstey_D.comparand;};return Monstey_U32{2};};
 Monstey_F.services.sleep=[&](Monstey_GadbBinderMemory,const Monstey_GadbManagerBackendSleepCall&){Monstey_Check(Monstey_F.get(Monstey_F.body+0x60,4)==2,"CAS mismatch expected store before Sleep");Monstey_F.put(Monstey_F.cells().state,0,4);};
 static_cast<void>(Monstey_F.init());Monstey_Check(Monstey_Calls==2&&Monstey_F.exchanges==1,"Nested current CAS replacement");}
}
} // namespace
int main(){try{for(unsigned Monstey_M=0;Monstey_M<3;++Monstey_M){Monstey_Normal(Monstey_M);Monstey_Boundaries(Monstey_M);Monstey_AtomicAndFaults(Monstey_M);Monstey_AliasesAndArithmetic(Monstey_M);Monstey_Lifetime(Monstey_M);}Monstey_Check(Monstey_Exits==51,"All51normal exits");std::cout<<"{\"status\":\"PASS\",\"scope\":\"ACTUAL_BACKEND_OWNER_LIBRARY\",\"cases\":"<<Monstey_Cases<<",\"checks\":"<<Monstey_Checks<<",\"normal_exit_classes\":"<<Monstey_Exits<<",\"whole_images\":"<<Monstey_Images<<"}\n";return 0;}catch(const std::exception&Monstey_X){std::cerr<<"FAIL: "<<Monstey_X.what()<<"; cases="<<Monstey_Cases<<" checks="<<Monstey_Checks<<'\n';return 1;}catch(...){std::cerr<<"FAIL: unexpected provider exception\n";return 1;}}
