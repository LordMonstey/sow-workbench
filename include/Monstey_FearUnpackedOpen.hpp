#pragma once
#include "Monstey_FearConstraintReader.hpp"
#include "Monstey_FearUnpackedCallback.hpp"
#include "Monstey_FearUnpackedCleanup.hpp"
#include "Monstey_FearUnpackedLine.hpp"
#include "Monstey_FearUnpackedMaterialize.hpp"
#include <cstdint>
#include <stdexcept>
namespace Monstey {
struct Monstey_FearUnpackedOpenServices {
    Monstey_FearConstraintServices constraints;
    Monstey_FearUnpackedCallbackServices callback;
    Monstey_FearUnpackedImports materializer;
    Monstey_FearUnpackedCleanupServices cleanup;
    // Narrow4210/4340 sentinel constructor boundary. Returned storage is initialized
    // according to the supplied primitive profile; caller publication stays concrete.
    Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory &, bool parsed,
                                         std::uint32_t map, std::uint32_t native_site)>
        new_sentinel;
    Monstey_FearDbCallable<std::uint64_t(Monstey_FearDbMutableMemory &, Monstey_FearConstraintCall)>
        get_length;
    Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory &,
                                         Monstey_FearConstraintReadCall)>
        read;
    Monstey_FearDbCallable<std::uint32_t(Monstey_FearDbMutableMemory &, std::uint32_t native_site,
                                         std::uint32_t bytes)>
        new_array, new_scalar;
    Monstey_FearDbCallable<void(Monstey_FearDbMutableMemory &, Monstey_FearUnpackedDeleteCall)>
        delete_array;
};
class Monstey_FearUnpackedOpenBoundaryFault final : public std::runtime_error {
  public:
    std::uint32_t native_site, target;
    Monstey_FearUnpackedOpenBoundaryFault(const char *, std::uint32_t, std::uint32_t = 0);
};
// Complete6200..642F. F=A-8 after native sub3C/pushEBX/EBP and ESI/EDI saves.
// Caller input words:F+50 text stream,F+54 constraints stream. Other consumed
// frames/opaque bytes must exist as supplied; no broad initialization is added.
// Concrete constraints/line/callback/sizing/materializer/cleanup bodies are called.
// Stream, allocator, sentinel ctor and narrow tree erase remain explicit services.
// Memory view replacement is observed after primitive and concrete stage calls;
// materializer-v00234d75 retains its declared stable-within-that-body memory contract.
std::uint32_t Monstey_FearOpenUnpackedBody(Monstey_FearDbMutableMemory &, std::uint32_t frame,
                                           Monstey_FearUnpackedOpenServices &);
// Complete6430 wrapper. W is entryESP; input wordsW+4(text),W+8(constraints).
// The two outgoing argument stores are retained; bodyF=W-58h. A nonzero returned
// root has its live refcount+10h incremented with U32 wrapping after all cleanup.
std::uint32_t Monstey_FearOpenUnpacked(Monstey_FearDbMutableMemory &, std::uint32_t wrapper_frame,
                                       Monstey_FearUnpackedOpenServices &);
} // namespace Monstey
