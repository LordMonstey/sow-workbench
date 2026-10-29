#include "Monstey_LeaveProjectionReference.hpp"
#include <limits>

namespace Monstey {
namespace {
std::uint64_t Monstey_ReadLeaveQword(
    Monstey_GadbBinderMemory memory, std::uint64_t address) {
  if (address < memory.base ||
      address > std::numeric_limits<std::uint64_t>::max() - 7 ||
      address - memory.base > memory.bytes.size() ||
      memory.bytes.size() - static_cast<std::size_t>(address - memory.base) < 8)
    throw Monstey_GadbBufferedMemoryFault(address, 8, false);
  const auto offset = static_cast<std::size_t>(address - memory.base);
  std::uint64_t value = 0;
  for (std::size_t byte = 0; byte < 8; ++byte)
    value |= static_cast<std::uint64_t>(memory.bytes[offset + byte]) << (byte * 8);
  return value;
}
}
Monstey_GadbManagerLeaveProjectionResult Monstey_ReferenceLeaveManagerCriticalSectionProjected(
    Monstey_GadbBinderMemory memory, std::uint64_t lock,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbManagerPrimitiveBindings& bindings,
    const Monstey_GadbManagerLeaveProjectionServices& services) {
  const auto target = Monstey_ReadLeaveQword(memory, bindings.leave_cell);
  if (!services.leave)
    throw Monstey_GadbManagerPrimitiveBoundaryFault(
        Monstey_GadbManagerPrimitiveBoundaryKind::missing_sync_service,
        0x14114BA88ULL, bindings.leave_cell, 8,
        bindings.leave_cell, target, lock);
  return services.leave(memory,
      {Monstey_GadbManagerSyncOperation::leave, 0x14114BA88ULL,
       bindings.leave_cell, target, lock, incoming});
}
Monstey_GadbManagerLeaveProjectionResult Monstey_ReferenceDestroyManagerGuardProjected(
    Monstey_GadbBinderMemory memory, std::uint64_t guard,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbManagerPrimitiveBindings& bindings,
    const Monstey_GadbManagerLeaveProjectionServices& services) {
  const auto lock = Monstey_ReadLeaveQword(memory, guard);
  return Monstey_ReferenceLeaveManagerCriticalSectionProjected(
      memory, lock, incoming, bindings, services);
}
}
