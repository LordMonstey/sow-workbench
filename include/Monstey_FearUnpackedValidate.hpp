#pragma once
#include "Monstey_FearDbCallable.hpp"
#include "Monstey_FearDbPrimitives.hpp"
#include <cstdint>
#include <stdexcept>
namespace Monstey {
struct Monstey_FearValidateCstrCall {std::uint32_t native_site,target,string_object;};
struct Monstey_FearValidateStrtolCall {std::uint32_t native_site,target,source,end_pointer,base;};
struct Monstey_FearValidateAtofCall {std::uint32_t native_site,target,source;};
enum class Monstey_FearValidateX87Phase {lower_duplicate_compare,upper_final_compare};
struct Monstey_FearValidateCompareCall {
 Monstey_FearValidateX87Phase phase;
 std::uint32_t load_site,compare_site,status_site,bound_address;
 std::uint64_t value_binary64_bits;
 std::uint32_t bound_binary32_bits;
};
class Monstey_FearUnpackedValidateBoundaryFault final:public std::runtime_error {
public:
 std::uint32_t native_site,target;
 Monstey_FearUnpackedValidateBoundaryFault(const char*,std::uint32_t,std::uint32_t=0);
};
struct Monstey_FearUnpackedValidateServices {
 Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory&,Monstey_FearValidateCstrCall)> c_str;
 // EAX native32 bits, not host long. Actual source/end/base argument cells are
 // consumed after known pushes; source/errno/locale/parser effects stay supplied.
 Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory&,Monstey_FearValidateStrtolCall)> strtol;
 // Supplied MSVCR71 atof normal return always FLD QWORD[result+10] at7C354E6C.
 // Return its binary64 load-source bits. CRT parsing/errno/locale and physical
 // x87 return/exception/history ABI remain an explicit consumed boundary.
 Monstey_FearDbCallable<std::uint64_t(Monstey_FearDbMutableMemory&,Monstey_FearValidateAtofCall)> atof_binary64;
 // Narrow x87 instruction/status group. Lower: FLD32/FLD ST1/FCOMPP/FNSTSW,
 // leaving the primary value live. Upper: FLD32/FXCH ST1/FCOMPP/FNSTSW,
 // retiring the primary value. Return the status-word snapshot, not a bool.
 Monstey_FearDbCallable<std::uint16_t(Monstey_FearDbMutableMemory&,Monstey_FearValidateCompareCall)> compare_status;
 // Consumed FSTP ST0 at3F7C only after failed lower comparison. Never silently
 // omitted; effects/exception/active-target lifetime precede the AL0 return.
 Monstey_FearDbCallable<void(Monstey_FearDbMutableMemory&,std::uint32_t native_site,
  std::uint64_t value_binary64_bits)> retire_primary;
};
// Complete10003F20..10003F81(97B) owned protocol; constraint_value is native ESI,
// value_string is ECX at entry, frame is the entry ESP identity. The5EC0 bridge
// uses validatorFrame=callbackF-4. Known argument stores reach frame-0Ch.
// Type3 performs signed native32 comparisons after strtol(source,end0,base0).
// Type2 uses exact AH&05 parity/JNP and AH&41 zero/JE branching, not guessed
// host floating comparisons. Lower/upper bounds are read only when consumed.
// Memory may be mutated/rebound; numeric identities remain cached as observed.
// Services use shared2097 persistent targets with call-scoped active lifetime.
// Return qualifies only AL. Callback5EC0 ignores AL but still consumes all effects.
// No eager validation/zero padding/SEH rollback or original calling ABI is added.
std::uint8_t Monstey_FearUnpackedValidate(Monstey_FearDbMutableMemory&,
 std::uint32_t constraint_value,std::uint32_t value_string,std::uint32_t frame,
 const Monstey_FearUnpackedValidateServices&);
// Exact selected supplied c_str field branch at7C3A44B3..44C1: raw28B string
// capacity+18 unsigned>=16 selects DWORD+4; otherwise inline identity+4.
std::uint32_t Monstey_FearUnpackedValidateStringData(Monstey_FearDbMutableMemory&,
 std::uint32_t string_object);
// Ordinary normal-masked profile: exact binary64-vs-binary32 comparison using
// integer bits, with C0/C2/C3 plus masked NaN-invalid bit. Other ambient status,
// x87 control/exception/pending-state and actual CRT history are NOT qualified.
// It performs no host floating calculation and does not parse a CString.
std::uint16_t Monstey_FearUnpackedValidateMaskedCompare(std::uint64_t,std::uint32_t);
}
