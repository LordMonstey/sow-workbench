#pragma once
#include "Monstey_GadbManagerGuardProjection.hpp"
namespace Monstey {
Monstey_GadbManagerGuardProjectedResult Monstey_ReferenceConstructManagerGuardProjected(
    Monstey_GadbBinderMemory, std::uint64_t guard, std::uint64_t lock,
    Monstey_GadbBufferedResult incoming, std::uint64_t incoming_r8,
    std::uint64_t incoming_r9, const Monstey_GadbManagerPrimitiveBindings&,
    const Monstey_GadbManagerEnterProjectionServices&);
}
