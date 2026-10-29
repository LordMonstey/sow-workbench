#pragma once
#include "Monstey_FearDbCallable.hpp"
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>
namespace Monstey_PublicationReview {
using Monstey_U64=std::uint64_t;using Monstey_U32=std::uint32_t;
struct Monstey_Raw {Monstey_U64 a,d;bool operator==(const Monstey_Raw&)const=default;};
enum class Monstey_EventKind{read,write,atomic,virtual_call,trap};
struct Monstey_Event {Monstey_EventKind kind;Monstey_U64 pc,address,value,extra;unsigned width;Monstey_Raw raw;bool operator==(const Monstey_Event&)const=default;};
enum class Monstey_FaultKind{range,missing_atomic,missing_virtual,invariant_trap,trap_returned,probe};
struct Monstey_Fault:std::runtime_error {Monstey_FaultKind kind;Monstey_U64 pc,address,slot,target,call_site,message;unsigned width;bool writing;Monstey_Fault(Monstey_FaultKind,Monstey_U64 pc,Monstey_U64 address,unsigned width,bool writing=false,Monstey_U64 slot=0,Monstey_U64 target=0,Monstey_U64 call_site=0,Monstey_U64 message=0);};
struct Monstey_Observer {std::vector<Monstey_Event> events;std::size_t fail_before=std::numeric_limits<std::size_t>::max();void Record(Monstey_Event);};
struct Monstey_View {
 Monstey_U64 base;std::span<std::uint8_t> bytes;Monstey_Observer* observer=nullptr;
 Monstey_U64 Load(Monstey_U64 address,unsigned width,Monstey_U64 pc)const;
 void Store(Monstey_U64 address,Monstey_U64 value,unsigned width,Monstey_U64 pc)const;
 void Zero(Monstey_U64 address,Monstey_U64 pc)const;
 void Event(Monstey_Event)const;
};
enum class Monstey_AtomicOp{increment,fetch_add};
struct Monstey_AtomicCall{Monstey_U64 pc,address;Monstey_U32 delta;Monstey_AtomicOp operation;};
struct Monstey_VirtualCall{Monstey_U64 pc,control,vtable,slot,target;Monstey_Raw incoming;};
struct Monstey_TrapCall{Monstey_U64 call_site,entry,store_site,message,address;Monstey_U32 value;Monstey_Raw incoming;};
struct Monstey_Services {
 Monstey::Monstey_FearDbCallable<Monstey_U32(Monstey_View,const Monstey_AtomicCall&)> atomic;
 Monstey::Monstey_FearDbCallable<Monstey_Raw(Monstey_View,const Monstey_VirtualCall&)> virtual_call;
 Monstey::Monstey_FearDbCallable<void(Monstey_View,const Monstey_TrapCall&)> trap;
};
Monstey_Raw Monstey_RefSecondary(Monstey_View,Monstey_U64 control,Monstey_Raw,const Monstey_Services&);
Monstey_Raw Monstey_RefPrimary(Monstey_View,Monstey_U64 control,Monstey_Raw,const Monstey_Services&);
Monstey_Raw Monstey_RefReleasePair(Monstey_View,Monstey_U64 pair,Monstey_Raw,const Monstey_Services&);
Monstey_Raw Monstey_RefAssign(Monstey_View,Monstey_U64 destination,Monstey_U64 data,Monstey_U64 control,Monstey_Raw,const Monstey_Services&);
Monstey_Raw Monstey_RefCopy(Monstey_View,Monstey_U64 destination,Monstey_U64 source,Monstey_Raw,const Monstey_Services&);
Monstey_Raw Monstey_RefPublish(Monstey_View,Monstey_U64 database,Monstey_U32 h8,Monstey_U32 h9,Monstey_U64 bank,Monstey_U64 bodyRsp,const Monstey_Services&);
}
