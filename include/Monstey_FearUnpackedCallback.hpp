#pragma once
#include "Monstey_FearUnpackedMaterialize.hpp"
#include "Monstey_FearDbCallable.hpp"
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
namespace Monstey {
// Numeric identities and targets are supplied owned data, never callable SDK addresses.
// Service holders share persistent mutable target state and retain the active target
// through self-replacement, reentrant calls and exceptions; targets are not cloned.
struct Monstey_FearUnpackedCallbackStringCall {
 Monstey_FearUnpackedStringOp op;
 std::uint32_t target,receiver,a,b,c,native_site;
};
class Monstey_FearUnpackedCallbackBoundaryFault final:public std::runtime_error {
public:
 std::uint32_t native_site,target;
 Monstey_FearUnpackedCallbackBoundaryFault(const char*,std::uint32_t,std::uint32_t=0);
};
struct Monstey_FearUnpackedCallbackServices {
 // Same primitive argument meanings as materializer imports. Additional consumed
 // operations: construct_cstring(receiver,a=C string), less(receiver=left,a=right).
 // Each record carries the live selected IAT target; lower_bound caches its less
 // target once on nonempty entry, exactly as the original selected helper.
 Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory&,
  Monstey_FearUnpackedCallbackStringCall)> string;
 // Exact consumed 3F20 boundary. Its AL is ignored; writes and exceptions are not.
 Monstey_FearDbCallable<std::uint8_t(Monstey_FearDbMutableMemory&,
  std::uint32_t constraint_value,std::uint32_t value_string)> validate;
 // Narrow physical tree insertion5890 boundary after concrete5C10 preparation.
 // Pair contains key28,typeDWORD,string28,opaqueDWORD. Return is the actual
 // hidden-result identity; its first DWORD supplies the selected node identity.
 Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory&,std::uint32_t map,
  std::uint32_t hidden_result,std::uint32_t hint,std::uint32_t pair)> insert;
 std::uint32_t npos_pointer_cell=0x1000B048u;
};
// Concrete4040/3F90 selected lower_bound. key_argument_cell is read only on the
// nonempty path. Constraint nil offset35h; parsed-value nil offset4Dh.
std::uint32_t Monstey_FearUnpackedCallbackLowerBound(Monstey_FearDbMutableMemory&,
 std::uint32_t map,std::uint32_t key_argument_cell,bool parsed,
 const Monstey_FearUnpackedCallbackServices&);
// Complete5C10 wrapper with explicit G local frame; incoming key DWORD atG+74h.
// G+2Ch is consumed opaque private state on insertion, never initialized here.
std::uint32_t Monstey_FearUnpackedCallbackParsedValue(Monstey_FearDbMutableMemory&,
 std::uint32_t map,std::uint32_t frame,const Monstey_FearUnpackedCallbackServices&);
// Complete5EC0..601C owned protocol. F is stable native local ESP after sub80h
// and four register saves. line/context argument DWORD cells are explicit and
// consumed at5EC6 and5EDC respectively. Normal physical placement isF+94/F+98.
// Locals: keyF+18,lineF+34,valueF+50,typeF+6C,parsedStringF+70,opaqueF+8C.
// Nested5C10 uses G=F-78h; selected helper outgoing arguments reach F-98h.
// Known outgoing argument stores below frames are real
// owned writes. CPU saved-register/return-address/SEH ABI is not synthesized.
// Original uninitialized private words F+8C and G+2C require supplied storage;
// no all-zero fallback, eager frame precheck, hidden cleanup or rollback exists.
// Services may mutate/rebind the owned view; every later operand remains live.
// Only native AL0/1 is qualified. Exceptions retain preceding writes and calls.
std::uint8_t Monstey_FearUnpackedLineCallback(Monstey_FearDbMutableMemory&,
 std::uint32_t frame,std::uint32_t line_argument_cell,std::uint32_t context_argument_cell,
 const Monstey_FearUnpackedCallbackServices&);
// Explicit logical raw-tree binding useful for ordinary OpenUnpacked composition.
// It uses the observed28B string/node fields and borrowed arena allocation, not
// original red-black balancing, allocator/capacity/lifetime or exception ABI.
struct Monstey_FearUnpackedCallbackLogicalMaps {
 Monstey_FearUnpackedLogicalStrings strings;
 Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory,std::uint32_t)> allocate;
 void Initialize(Monstey_FearDbMutableMemory,std::uint32_t map,bool parsed) const;
 std::uint32_t InsertParsed(Monstey_FearDbMutableMemory&,std::uint32_t map,
  std::uint32_t hidden_result,std::uint32_t hint,std::uint32_t pair) const;
 // Ordinary logical constraint row binding; existing exact keys overwrite values.
 void PutConstraint(Monstey_FearDbMutableMemory,std::uint32_t map,
  std::uint32_t key_string,std::uint32_t type,std::uint32_t word4,std::uint32_t word8) const;
};
}
