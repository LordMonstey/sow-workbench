#pragma once
#include "Monstey_GadbPoolRelocation.hpp"
namespace Monstey {
// Complete SoW x64 141766594..1417666E6, not a named-record adaptation.
// frame_rsp=Q=entryRSP-48h. Inputs at Q+70/Q+78/Q+88 are pool,
// descriptors and value-cursor address. Pool is consumed only if descriptor
// count is nonzero. Known originalRDX home store Q+58 precedes headers.
// Four typed headers2/1/4/1 write Q+20/Q+58/Q+24/Q+80, without prezero.
// Uses accepted typedRead/type predicate and narrow supplied owned Read.
// Flags remain verbatim, record padding is preserved, and return is rawRAX
// with AL forced at native epochs. Faults preserve all prior effects.
// Exclusive stable native backing profile; Memory& rebinding between Read
// calls is an owned binding extension. Unknown savedCPU/return words are
// supplied bytes, not fabricated physicalABI. No hash/writer/runtime loader.
std::uint64_t Monstey_GadbLoadAnonymousRecord(
    Monstey_GadbBinderMemory &, std::uint64_t category,
    std::uint64_t original_rdx, std::uint64_t record, std::uint64_t stream,
    std::uint64_t frame_rsp, const Monstey_GadbPoolReadService &);
} // namespace Monstey
