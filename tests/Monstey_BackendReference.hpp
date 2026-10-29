#pragma once
#include "Monstey_PlacementReference.hpp"
#include <array>
namespace Monstey_BackendReview {
using Monstey_U64=std::uint64_t;
using Monstey_U32=std::uint32_t;
using Monstey_Image=Monstey_PlacementReview::Monstey_Image;
using Monstey_Raw=Monstey_PlacementReview::Monstey_Raw;
using Monstey_Tail=Monstey_PlacementReview::Monstey_TailResult;
enum class Monstey_Kind {fill,allocate,info,sleep,random,initialize,enter,leave};
struct Monstey_Site {Monstey_U64 pc,call,body,cell,target;bool operator==(const Monstey_Site&)const=default;};
struct Monstey_Fill {Monstey_Site site;Monstey_U64 destination,count;Monstey_U32 value;};
struct Monstey_Allocation {Monstey_Site site;Monstey_U64 address,size;Monstey_U32 type,protection;};
struct Monstey_Info {Monstey_Site site;Monstey_U64 output;};
struct Monstey_Sleep {Monstey_Site site;Monstey_U32 milliseconds,alertable;};
struct Monstey_Cs {Monstey_Site site;Monstey_U64 lock;Monstey_U32 spin;};
struct Monstey_Sync {Monstey_Site site;Monstey_U64 lock;};
struct Monstey_Exchange {Monstey_U64 pc,address;Monstey_U32 value;};
struct Monstey_ImportFault:std::runtime_error {Monstey_Kind kind;Monstey_Site site;Monstey_ImportFault(Monstey_Kind,Monstey_Site);};
struct Monstey_ExchangeFault:std::runtime_error {Monstey_U64 pc,address;Monstey_U32 value;unsigned width=4;Monstey_ExchangeFault(Monstey_Exchange);};
struct Monstey_Globals {Monstey_U64 cookie,page,granularity,auxiliary,flags,secondary,template_cs,state,main_cs;};
struct Monstey_Bindings {
 std::array<Monstey_U64,8> imports{0x141D92E50,0x141D92278,0x141D92290,0x141D92268,0x141D93758,0x141D92270,0x141D922C0,0x141D922B8};
 Monstey_PlacementReview::Monstey_Cells lower;
 std::array<Monstey_Globals,3> modes{{
  {0x14277D1E8,0x14277D1F0,0x14277D1F8,0x14277D200,0x14277D210,0x14277D590,0x14277D598,0x14277D5F0,0x142A41508},
  {0x14277CDD8,0x14277CDE0,0x14277CDE8,0x14277CDF0,0x14277CE00,0x14277D180,0x14277D188,0x14277D1E0,0x142A414E0},
  {0x14277D5F8,0x14277D600,0x14277D608,0x14277D610,0x14277D620,0x14277D9A0,0x14277D9A8,0x14277D5F4,0x142A41530}}};
};
struct Monstey_Services {
 Monstey::Monstey_FearDbCallable<Monstey_U64(Monstey_Image,const Monstey_Fill&)>fill;
 Monstey::Monstey_FearDbCallable<Monstey_Raw(Monstey_Image,const Monstey_Allocation&)>allocate;
 Monstey::Monstey_FearDbCallable<void(Monstey_Image,const Monstey_Info&)>info;
 Monstey::Monstey_FearDbCallable<void(Monstey_Image,const Monstey_Sleep&)>sleep;
 Monstey::Monstey_FearDbCallable<Monstey_U32(Monstey_Image,const Monstey_Site&)>random;
 Monstey::Monstey_FearDbCallable<Monstey_U64(Monstey_Image,const Monstey_Cs&)>initialize;
 Monstey::Monstey_FearDbCallable<void(Monstey_Image,const Monstey_Sync&)>enter;
 Monstey::Monstey_FearDbCallable<Monstey_U64(Monstey_Image,const Monstey_Sync&)>leave;
 Monstey::Monstey_FearDbCallable<Monstey_U32(Monstey_Image,const Monstey_Exchange&)>exchange;
 Monstey_PlacementReview::Monstey_Services lower;
};
#define MONSTEY_BACKEND_REFERENCE_DECL(T) \
Monstey_Raw Monstey_RefCreate##T(Monstey_Image,Monstey_U64 flags,Monstey_U64 backing,Monstey_U64 count,Monstey_U64 body,const Monstey_Services&,Monstey_Bindings={}); \
Monstey_Raw Monstey_RefAllocate##T(Monstey_Image,Monstey_U64 descriptor,Monstey_U64 body,Monstey_U64 incoming_d,const Monstey_Services&,Monstey_Bindings={}); \
Monstey_Raw Monstey_RefInit##T(Monstey_Image,Monstey_U64 body,const Monstey_Services&,Monstey_Bindings={}); \
Monstey_Tail Monstey_RefPlace##T(Monstey_Image,Monstey_U64 backing,Monstey_U64 span,Monstey_U64 body,const Monstey_Services&,Monstey_Bindings={});
MONSTEY_BACKEND_REFERENCE_DECL(40)
MONSTEY_BACKEND_REFERENCE_DECL(80)
MONSTEY_BACKEND_REFERENCE_DECL(C0)
#undef MONSTEY_BACKEND_REFERENCE_DECL
}
