#pragma once
#include "Monstey_GadbManagerLeaveProjection.hpp"
namespace Monstey {
Monstey_GadbManagerLeaveProjectionResult Monstey_ReferenceLeaveManagerCriticalSectionProjected(
    Monstey_GadbBinderMemory, std::uint64_t lock, Monstey_GadbBufferedResult incoming,
    const Monstey_GadbManagerPrimitiveBindings&,
    const Monstey_GadbManagerLeaveProjectionServices&);
Monstey_GadbManagerLeaveProjectionResult Monstey_ReferenceDestroyManagerGuardProjected(
    Monstey_GadbBinderMemory, std::uint64_t guard, Monstey_GadbBufferedResult incoming,
    const Monstey_GadbManagerPrimitiveBindings&,
    const Monstey_GadbManagerLeaveProjectionServices&);
} // namespace Monstey
