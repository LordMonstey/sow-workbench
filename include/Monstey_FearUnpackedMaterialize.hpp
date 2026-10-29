#pragma once
#include "Monstey_FearDbCallable.hpp"
#include "Monstey_FearDbPrimitives.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
namespace Monstey {
enum class Monstey_FearUnpackedStringOp {
    construct,
    copy,
    destroy,
    assign,
    clear,
    c_str,
    subscript,
    append,
    find,
    rfind,
    substr,
    unequal,
    construct_cstring,
    less
};
// Only original consumed primitives are supplied; the native sizing, conversion,
// key/iterator/constructor and link bodies below are concrete owned C++.
// Strings/returned c_str/subscript identities are native32 owned addresses.
// Callable targets persist across calls and are retained across self-replacement.
// Copies retain the same target identity; concurrent mutation is not qualified.
// All callbacks may mutate the same stable borrowed image. No eager validation
// or original function pointer invocation. Exceptions retain preceding effects.
struct Monstey_FearUnpackedImports {
    Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory, Monstey_FearUnpackedStringOp,
                                         std::uint32_t receiver, std::uint32_t a, std::uint32_t b,
                                         std::uint32_t c)>
        string;
    Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory, std::uint32_t source)>
        atoi_bits;
    // This explicit boundary includes original atof+x87 FSTP32 result selection.
    Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory, std::uint32_t source)>
        atof_fstp32_bits;
    Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory, std::uint32_t source,
                                         std::uint32_t dimensions,
                                         std::array<std::uint32_t, 4> destinations)>
        sscanf;
    Monstey_FearDbCallable<void(Monstey_FearDbMutableMemory, std::uint32_t destination,
                                std::uint32_t source, std::uint32_t count)>
        strncpy;
    std::uint32_t npos_pointer_cell = 0, empty_cstring = 0, root_name_cell = 0;
};
class Monstey_FearUnpackedBoundaryFault final : public std::runtime_error {
  public:
    std::string boundary;
    explicit Monstey_FearUnpackedBoundaryFault(std::string);
};
// 4790. Frame is the native loop-local base after conditional EBP/EDI saves.
// Caller supplies map argument DWORD at frame+BC. The original private CPU stack
// ABI/saved registers/return-address bytes are NOT synthesized. Consumed locals
// and outgoing argument/string objects are real borrowed storage and may alias.
// Empty-map path uses the same physical locals, even though native ESP differs.
void Monstey_FearUnpackedSize(Monstey_FearDbMutableMemory, std::uint32_t context,
                              std::uint32_t frame, const Monstey_FearUnpackedImports &);
// 4A30. Frame is entryESP-12C; caller supplies root/context/map/allocation DWORDs
// at +130/+134/+138/+13C. Outgoing args/temp strings consume storage below frame
// (down to frame-60h). Cursor is a borrowed DWORD, not captured upfront.
// Context/args/trees/cursor/output/local aliases and late reads are retained.
std::uint8_t Monstey_FearUnpackedMaterialize(Monstey_FearDbMutableMemory,
                                             std::uint32_t cursor_address, std::uint32_t frame,
                                             const Monstey_FearUnpackedImports &);
// Concrete accepted-family6FF0/6F80/6F20 traversal; live counts, type9 only.
void Monstey_FearUnpackedFixLinks(Monstey_FearDbMutableMemory, std::uint32_t root);
// Concrete allocation arithmetic only. It neither allocates nor zeroes storage.
std::uint32_t Monstey_FearUnpackedAllocationBytes(Monstey_FearDbMemory, std::uint32_t context);
// Usable logical owned string binding. It uses selected 28B raw objects with
// size+14/capacity+18 and inline bytes+4 when capacity<=15. This is an explicit
// owned content/allocator profile, not original MSVCP capacity/exception/ABI.
// allocate is invoked only by this adapter when content exceeds15; returned
// addresses refer to the same caller-owned stable memory. It never executes SDK.
// construct_cstring a is a consumed native C-string identity; less compares
// receiver and a as owned unsigned-byte lexical contents. These two primitives
// support the separately reconstructed5EC0 caller, not this body's own imports.
// Source content is selected at each primitive's call epoch; native32 fields
// remain live. Returned c_str addresses can alias all other borrowed memory.
struct Monstey_FearUnpackedLogicalStrings {
    Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory, std::uint32_t)> allocate;
    std::uint32_t Invoke(Monstey_FearDbMutableMemory, Monstey_FearUnpackedStringOp,
                         std::uint32_t receiver, std::uint32_t a = 0, std::uint32_t b = 0,
                         std::uint32_t c = 0) const;
};
// Explicit ordinary owned forward-byte strncpy profile (including zero padding).
// Overlapping/CRT-specific implementation behavior is not silently generalized.
void Monstey_FearUnpackedForwardStrncpy(Monstey_FearDbMutableMemory, std::uint32_t destination,
                                        std::uint32_t source, std::uint32_t count);
} // namespace Monstey
