#pragma once
#include "Monstey_FearDbCallable.hpp"
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
namespace Monstey_PlacementReview {
using Monstey_U64=std::uint64_t;using Monstey_U32=std::uint32_t;
struct Monstey_Raw{Monstey_U64 a,d;bool operator==(const Monstey_Raw&)const=default;};
struct Monstey_TailResult{Monstey_Raw raw;Monstey_U64 remaining;bool operator==(const Monstey_TailResult&)const=default;};
struct Monstey_CasResult{Monstey_Raw raw;Monstey_U64 desired,target;bool operator==(const Monstey_CasResult&)const=default;};
enum class Monstey_FaultKind{memory,missing_cas};
struct Monstey_Fault:std::runtime_error{Monstey_FaultKind kind;Monstey_U64 pc,address;unsigned width;bool writing;Monstey_U32 comparand,desired;Monstey_Fault(Monstey_FaultKind,Monstey_U64 pc,Monstey_U64 address,unsigned width,bool writing=false,Monstey_U32 comparand=0,Monstey_U32 desired=0);};
struct Monstey_Access{bool writing;Monstey_U64 pc,address,value;unsigned width;bool operator==(const Monstey_Access&)const=default;};
struct Monstey_Image{Monstey_U64 base;std::span<std::uint8_t>bytes;std::vector<Monstey_Access>*passive=nullptr;Monstey_U64 Read(Monstey_U64,unsigned,Monstey_U64)const;void Write(Monstey_U64,Monstey_U64,unsigned,Monstey_U64)const;};
struct Monstey_Cells{Monstey_U64 g40=0x14277D208,g80=0x14277CDF8,gc0=0x14277D618,table=0x14202EE28;};
struct Monstey_Call{Monstey_U64 pc,address;Monstey_U32 comparand,desired;bool operator==(const Monstey_Call&)const=default;};
struct Monstey_Services{Monstey::Monstey_FearDbCallable<Monstey_U32(Monstey_Image,const Monstey_Call&)>cas;};
Monstey_Raw Monstey_RefReset40(Monstey_Image,Monstey_U64 object,Monstey_U64 incoming_d);
Monstey_Raw Monstey_RefReset80(Monstey_Image,Monstey_U64 object,Monstey_U64 incoming_d);
Monstey_Raw Monstey_RefResetC0(Monstey_Image,Monstey_U64 object,Monstey_U64 incoming_d);
Monstey_TailResult Monstey_RefTail40(Monstey_Image,Monstey_U64 object,Monstey_U64 start,Monstey_U64 span,Monstey_Cells={});
Monstey_TailResult Monstey_RefTail80(Monstey_Image,Monstey_U64 object,Monstey_U64 start,Monstey_U64 span,Monstey_Cells={});
Monstey_TailResult Monstey_RefTailC0(Monstey_Image,Monstey_U64 object,Monstey_U64 start,Monstey_U64 span,Monstey_Cells={});
Monstey_CasResult Monstey_RefWord(Monstey_Image,Monstey_U64 target,Monstey_U64 expected,Monstey_U64 desired,Monstey_U64 bodyE,const Monstey_Services&,Monstey_Cells={});
Monstey_CasResult Monstey_RefState(Monstey_Image,Monstey_U64 target,Monstey_U64 expected,Monstey_U64 bodyF,const Monstey_Services&,Monstey_Cells={});
}
