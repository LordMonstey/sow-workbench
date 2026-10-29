#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
namespace Monstey_CategoryReview {
using Monstey_U64=std::uint64_t; using Monstey_U32=std::uint32_t; using Monstey_U8=std::uint8_t;
struct Monstey_Machine { Monstey_U64 rax; Monstey_U64 rdx; };
struct Monstey_ReferenceMemoryFault : std::runtime_error { Monstey_U64 address; unsigned width; bool writing; Monstey_ReferenceMemoryFault(Monstey_U64 a,unsigned w,bool v):std::runtime_error("reference consumed range"),address(a),width(w),writing(v){} };
struct Monstey_Access { bool write; Monstey_U64 address; unsigned width; Monstey_U64 value; };
class Monstey_Memory {
 std::vector<Monstey_U8> Monstey_bytes;
public:
 Monstey_U64 base=0x10000000ull; std::vector<Monstey_Access> accesses; std::size_t faultAt=std::numeric_limits<std::size_t>::max();
 explicit Monstey_Memory(std::size_t size=0x10000):Monstey_bytes(size,0xA5){}
 Monstey_U64 Read(Monstey_U64 address,unsigned width);
 void Write(Monstey_U64 address,Monstey_U64 value,unsigned width);
 std::span<const Monstey_U8> Bytes() const { return Monstey_bytes; }
 void ClearTrace(){accesses.clear();}
};
struct Monstey_ReadCall { Monstey_U64 stream,destination,count,target; unsigned ordinal; };
using Monstey_Read=std::function<Monstey_Machine(Monstey_Memory&,const Monstey_ReadCall&)>;
extern const std::array<Monstey_U8,256> Monstey_Map;
Monstey_Machine Monstey_Hash(Monstey_Memory&,Monstey_U64 string,Monstey_U32 seed,Monstey_U64 incomingRdx);
// Independent assembly reference for RDX only; it reproduces consumed memory effects,
// but makes no physical saved-register-slot ABI or raw RAX claim for record bodies.
struct Monstey_RecordExit { Monstey_U64 rdx; bool payloadFailure; unsigned reads; unsigned descriptors; };
Monstey_RecordExit Monstey_RecordRdx(Monstey_Memory&,bool named,Monstey_U64 parent,Monstey_U64 originalRdx,Monstey_U64 record,Monstey_U64 stream,Monstey_U64 frame,const Monstey_Read&);
Monstey_U64 Monstey_Lookup(Monstey_Memory&,Monstey_U64 object,Monstey_U64 key);
struct Monstey_ChildCall { bool named; Monstey_U64 parent,originalRdx,record,stream,frame; unsigned ordinal; };
struct Monstey_SelectorCall { Monstey_U64 object,bank,second,target; };
// Scripted dependencies below are ONLY a caller-protocol reference fixture. The
// complete selector and children are not qualified by these test hooks.
struct Monstey_CategoryDependencies {
 Monstey_Read read;
 std::function<Monstey_Machine(Monstey_Memory&,const Monstey_SelectorCall&)> selector;
 std::function<Monstey_Machine(Monstey_Memory&,const Monstey_ChildCall&)> child;
};
Monstey_Machine Monstey_Category(Monstey_Memory&,Monstey_U64 originalRcx,Monstey_U64 bank,Monstey_U64 category,Monstey_U64 lookup,Monstey_U64 F,Monstey_CategoryDependencies&);
}
