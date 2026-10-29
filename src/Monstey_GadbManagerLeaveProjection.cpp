#include "Monstey_GadbManagerLeaveProjection.hpp"
#include <limits>

namespace Monstey {
namespace {
std::uint64_t Monstey_LeaveProjectionRead64(
    Monstey_GadbBinderMemory Monstey_Memory,
    std::uint64_t Monstey_Address) {
  if (Monstey_Address < Monstey_Memory.base ||
      Monstey_Address > std::numeric_limits<std::uint64_t>::max() - 7u ||
      Monstey_Address - Monstey_Memory.base > Monstey_Memory.bytes.size() ||
      8u > Monstey_Memory.bytes.size() -
          static_cast<std::size_t>(Monstey_Address - Monstey_Memory.base))
    throw Monstey_GadbBufferedMemoryFault(Monstey_Address, 8, false);

  const auto Monstey_Offset =
      static_cast<std::size_t>(Monstey_Address - Monstey_Memory.base);
  std::uint64_t Monstey_Value = 0;
  for (std::size_t Monstey_Index = 0; Monstey_Index < 8; ++Monstey_Index)
    Monstey_Value |= std::uint64_t(Monstey_Memory.bytes[
        Monstey_Offset + Monstey_Index]) << (Monstey_Index * 8u);
  return Monstey_Value;
}
} // namespace

Monstey_GadbManagerLeaveProjectionResult
Monstey_GadbLeaveManagerCriticalSectionProjected(
    Monstey_GadbBinderMemory Monstey_Memory,
    std::uint64_t Monstey_Lock,
    Monstey_GadbBufferedResult Monstey_Incoming,
    const Monstey_GadbManagerPrimitiveBindings& Monstey_Bindings,
    const Monstey_GadbManagerLeaveProjectionServices& Monstey_Services) {
  const auto Monstey_Target = Monstey_LeaveProjectionRead64(
      Monstey_Memory, Monstey_Bindings.leave_cell);
  if (!Monstey_Services.leave)
    throw Monstey_GadbManagerPrimitiveBoundaryFault(
        Monstey_GadbManagerPrimitiveBoundaryKind::missing_sync_service,
        0x14114BA88ULL, Monstey_Bindings.leave_cell, 8,
        Monstey_Bindings.leave_cell, Monstey_Target, Monstey_Lock);

  return Monstey_Services.leave(Monstey_Memory,
      {Monstey_GadbManagerSyncOperation::leave, 0x14114BA88ULL,
       Monstey_Bindings.leave_cell, Monstey_Target, Monstey_Lock,
       Monstey_Incoming});
}

Monstey_GadbManagerLeaveProjectionResult Monstey_GadbDestroyManagerGuardProjected(
    Monstey_GadbBinderMemory Monstey_Memory,
    std::uint64_t Monstey_Guard,
    Monstey_GadbBufferedResult Monstey_Incoming,
    const Monstey_GadbManagerPrimitiveBindings& Monstey_Bindings,
    const Monstey_GadbManagerLeaveProjectionServices& Monstey_Services) {
  const auto Monstey_Lock =
      Monstey_LeaveProjectionRead64(Monstey_Memory, Monstey_Guard);
  return Monstey_GadbLeaveManagerCriticalSectionProjected(
      Monstey_Memory, Monstey_Lock, Monstey_Incoming,
      Monstey_Bindings, Monstey_Services);
}
} // namespace Monstey
