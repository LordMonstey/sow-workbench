#pragma once
#include "Monstey_FearDbPrimitives.hpp"
#include "Monstey_FearDbCallable.hpp"
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
namespace Monstey {
// Dispatches only a consumed original callback identity; no original address is
// invoked. Return bits are ignored exactly by10009D60. The callback may mutate
// the borrowed owned image, including input, scratch and argument DWORDs, and
// rebind the caller-owned memory view. Later operands consume the updated view.
// Shared targets retain persistent mutable state and active lifetime through
// self-replacement/clear/reentrancy/exception; no std::function closure is cloned.
using Monstey_FearUnpackedLineDispatch=Monstey_FearDbCallable<std::uint32_t(
 Monstey_FearDbMutableMemory&,std::uint32_t callback_identity,
 std::uint32_t line_identity,std::uint32_t context_identity)>;
class Monstey_FearUnpackedLineBoundaryFault final:public std::runtime_error {
public:
 std::uint32_t callback_identity,line_identity,context_identity;
 Monstey_FearUnpackedLineBoundaryFault(std::uint32_t,std::uint32_t,std::uint32_t);
};
// Owned representation of complete10009D60. argument_words contains four U32s:
// input pointer,+4 byte length,+8 callback identity,+C callback context. These
// fields are sampled at their native epochs rather than captured up front.
// line_scratch names1024 bytes; its preceding byte is the comment flag. Native
// explicit initialization/store order is retained. No early whole-span check,
// padding or alias restriction. Backing for each consumed operand is supplied;
// callbacks may mutate or replace the view through the same memory reference.
// Returns only native AL:0 on1024th stored byte,1 on loop completion. Original
// private frame/callee register, hardware-fault and calling ABI remain separate.
std::uint8_t Monstey_FearUnpackedFrameLines(Monstey_FearDbMutableMemory&,
 std::uint32_t argument_words,std::uint32_t line_scratch,
 const Monstey_FearUnpackedLineDispatch&);
// Complete3A70 owned byte-string content sequence. References may alias any
// other argument. Writes occur attribute,record,category and later operations
// reread live input. U32 positions/npos arithmetic match the original. Standard
// owned C++ exceptions propagate with earlier writes retained. Original MSVCP71
// object/capacity/allocator ABI and its incidental EAX value are not claimed.
void Monstey_FearUnpackedSplitKey(const std::string& input,std::string& category,
 std::string& record,std::string& attribute);
}
