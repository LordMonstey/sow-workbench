#pragma once
#include "Monstey_FearDbCallable.hpp"
#include <cstdint>
#include <span>
#include <optional>
#include <stdexcept>
#include <vector>
namespace Monstey_ManagerPrimitiveReview {
using Monstey_U64=std::uint64_t;using Monstey_U32=std::uint32_t;
struct Monstey_Raw{Monstey_U64 a,d;bool operator==(const Monstey_Raw&)const=default;};
enum class Monstey_FaultKind{memory,missing_gs,missing_sync};
struct Monstey_Fault:std::runtime_error{Monstey_FaultKind kind;Monstey_U64 pc,address,slot,target,argument;unsigned width;bool writing;Monstey_Fault(Monstey_FaultKind,Monstey_U64 pc,Monstey_U64 address,unsigned width,bool writing=false,Monstey_U64 slot=0,Monstey_U64 target=0,Monstey_U64 argument=0);};
enum class Monstey_EventKind{read,write,sync};
struct Monstey_Event{Monstey_EventKind kind;Monstey_U64 pc,address,value;unsigned width;bool operator==(const Monstey_Event&)const=default;};
struct Monstey_Image{Monstey_U64 base;std::span<std::uint8_t> bytes;std::vector<Monstey_Event>*events=nullptr;Monstey_U64 Read(Monstey_U64,unsigned,Monstey_U64 pc)const;void Write(Monstey_U64,Monstey_U64,unsigned,Monstey_U64 pc)const;};
struct Monstey_Bindings{std::optional<Monstey_U64>gs;Monstey_U64 enter=0x141D922C0,leave=0x141D922B8;};
enum class Monstey_Role{enter,leave};
struct Monstey_Call{Monstey_Role role;Monstey_U64 pc,slot,target,argument;Monstey_Raw incoming;bool operator==(const Monstey_Call&)const=default;};
struct Monstey_Services{Monstey::Monstey_FearDbCallable<Monstey_Raw(Monstey_Image,const Monstey_Call&)>sync;};
Monstey_Raw Monstey_RefConstruct(Monstey_Image,Monstey_U64 guard,Monstey_U64 lock,Monstey_Raw,const Monstey_Bindings&,const Monstey_Services&);
Monstey_Raw Monstey_RefLeave(Monstey_Image,Monstey_U64 lock,Monstey_Raw,const Monstey_Bindings&,const Monstey_Services&);
Monstey_Raw Monstey_RefDestroy(Monstey_Image,Monstey_U64 guard,Monstey_Raw,const Monstey_Bindings&,const Monstey_Services&);
Monstey_Raw Monstey_RefIncrement(Monstey_Image,const Monstey_Bindings&);
Monstey_Raw Monstey_RefDecrement(Monstey_Image,const Monstey_Bindings&);
Monstey_Raw Monstey_RefCold(Monstey_Image,Monstey_Raw);
}
