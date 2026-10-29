#pragma once
#include "Monstey_GadbPoolRelocation.hpp"
#include "Monstey_GadbBufferedStream.hpp"
namespace Monstey {
// Additive raw-register composition for the concrete SoW file-fallback stream.
// Addresses are stable owned identities, never invoked as host pointers.
// The consumed stream vtable+8 must select141B50DE8. Only copy/fill/ReadFile
// services are external. Typed wrappers and payload reads use their distinct
// native body-RSP frames. Existing scalar record APIs remain unchanged.
// Prologue saved-register/return bytes are supplied, not fabricated physicalABI.
Monstey_GadbBufferedResult Monstey_GadbLoadNamedRecordFromBuffered(
    Monstey_GadbBinderMemory, std::uint64_t category, std::uint64_t original_rdx,
    std::uint64_t record, std::uint64_t stream, std::uint64_t frame_rbp,
    const Monstey_GadbBufferedServices&, Monstey_GadbBufferedCells = {});
Monstey_GadbBufferedResult Monstey_GadbLoadAnonymousRecordFromBuffered(
    Monstey_GadbBinderMemory, std::uint64_t category, std::uint64_t original_rdx,
    std::uint64_t record, std::uint64_t stream, std::uint64_t body_rsp,
    const Monstey_GadbBufferedServices&, Monstey_GadbBufferedCells = {});
// Same immutable qualified map as423E70. Empty input preserves incomingRDX;
// nonempty input returns zero-extended EDX=signext8(map[last input byte]).
Monstey_GadbBufferedResult Monstey_GadbHashCStringRegisters(
    Monstey_GadbBinderMemory, std::uint64_t string_identity,
    std::uint32_t seed, std::uint64_t incoming_rdx);
} // namespace Monstey
