#include "Monstey_GadbManagerPrimitives.hpp"
#include <limits>

namespace Monstey {
Monstey_GadbManagerPrimitiveBoundaryFault::Monstey_GadbManagerPrimitiveBoundaryFault(
    Monstey_GadbManagerPrimitiveBoundaryKind Monstey_Kind,
    std::uint64_t Monstey_Site, std::uint64_t Monstey_Address,
    std::size_t Monstey_Width, std::uint64_t Monstey_Slot,
    std::uint64_t Monstey_Target, std::uint64_t Monstey_Argument)
    : std::runtime_error("Unprovided SoW manager primitive boundary"),
      kind(Monstey_Kind), native_site(Monstey_Site), address(Monstey_Address),
      width(Monstey_Width), slot_identity(Monstey_Slot),
      target_identity(Monstey_Target), argument(Monstey_Argument) {}

namespace {
std::size_t Monstey_ManagerOffset(Monstey_GadbBinderMemory Monstey_Memory,
    std::uint64_t Monstey_Address, std::size_t Monstey_Width,
    bool Monstey_Writing) {
  if (Monstey_Address < Monstey_Memory.base ||
      Monstey_Address > std::numeric_limits<std::uint64_t>::max() -
          (Monstey_Width - 1) ||
      Monstey_Address - Monstey_Memory.base > Monstey_Memory.bytes.size() ||
      Monstey_Width > Monstey_Memory.bytes.size() -
          static_cast<std::size_t>(Monstey_Address - Monstey_Memory.base))
    throw Monstey_GadbBufferedMemoryFault(
        Monstey_Address, Monstey_Width, Monstey_Writing);
  return static_cast<std::size_t>(Monstey_Address - Monstey_Memory.base);
}

std::uint64_t Monstey_ManagerRead(Monstey_GadbBinderMemory Monstey_Memory,
    std::uint64_t Monstey_Address, std::size_t Monstey_Width) {
  const auto Monstey_Offset =
      Monstey_ManagerOffset(Monstey_Memory, Monstey_Address, Monstey_Width, false);
  std::uint64_t Monstey_Value = 0;
  for (std::size_t Monstey_Index = 0; Monstey_Index < Monstey_Width; ++Monstey_Index)
    Monstey_Value |= std::uint64_t(Monstey_Memory.bytes[Monstey_Offset + Monstey_Index])
        << (Monstey_Index * 8u);
  return Monstey_Value;
}

void Monstey_ManagerWrite(Monstey_GadbBinderMemory Monstey_Memory,
    std::uint64_t Monstey_Address, std::uint64_t Monstey_Value,
    std::size_t Monstey_Width) {
  const auto Monstey_Offset =
      Monstey_ManagerOffset(Monstey_Memory, Monstey_Address, Monstey_Width, true);
  for (std::size_t Monstey_Index = 0; Monstey_Index < Monstey_Width; ++Monstey_Index)
    Monstey_Memory.bytes[Monstey_Offset + Monstey_Index] =
        static_cast<std::uint8_t>(Monstey_Value >> (Monstey_Index * 8u));
}

std::uint64_t Monstey_ManagerTlsArray(Monstey_GadbBinderMemory Monstey_Memory,
    const Monstey_GadbManagerPrimitiveBindings& Monstey_Bindings,
    std::uint64_t Monstey_Site) {
  if (!Monstey_Bindings.gs58_cell_identity.has_value())
    throw Monstey_GadbManagerPrimitiveBoundaryFault(
        Monstey_GadbManagerPrimitiveBoundaryKind::missing_gs58_binding,
        Monstey_Site, 0x58, 8, 0, 0, 0);
  return Monstey_ManagerRead(
      Monstey_Memory, *Monstey_Bindings.gs58_cell_identity, 8);
}
} // namespace

Monstey_GadbBufferedResult Monstey_GadbConstructManagerGuard(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Guard,
    std::uint64_t Monstey_Lock, Monstey_GadbBufferedResult Monstey_Incoming,
    const Monstey_GadbManagerPrimitiveBindings& Monstey_Bindings,
    const Monstey_GadbManagerPrimitiveServices& Monstey_Services) {
  Monstey_Incoming.rdx = Monstey_Lock;
  Monstey_ManagerWrite(Monstey_Memory, Monstey_Guard, Monstey_Lock, 8);
  const auto Monstey_Target =
      Monstey_ManagerRead(Monstey_Memory, Monstey_Bindings.enter_cell, 8);
  if (!Monstey_Services.sync)
    throw Monstey_GadbManagerPrimitiveBoundaryFault(
        Monstey_GadbManagerPrimitiveBoundaryKind::missing_sync_service,
        0x14114BA9FULL, Monstey_Bindings.enter_cell, 8,
        Monstey_Bindings.enter_cell, Monstey_Target, Monstey_Lock);
  auto Monstey_Result = Monstey_Services.sync(Monstey_Memory,
      {Monstey_GadbManagerSyncOperation::enter, 0x14114BA9FULL,
       Monstey_Bindings.enter_cell, Monstey_Target, Monstey_Lock,
       Monstey_Incoming});
  Monstey_Result.rax = Monstey_Guard;
  return Monstey_Result;
}

Monstey_GadbBufferedResult Monstey_GadbLeaveManagerCriticalSection(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Lock,
    Monstey_GadbBufferedResult Monstey_Incoming,
    const Monstey_GadbManagerPrimitiveBindings& Monstey_Bindings,
    const Monstey_GadbManagerPrimitiveServices& Monstey_Services) {
  const auto Monstey_Target =
      Monstey_ManagerRead(Monstey_Memory, Monstey_Bindings.leave_cell, 8);
  if (!Monstey_Services.sync)
    throw Monstey_GadbManagerPrimitiveBoundaryFault(
        Monstey_GadbManagerPrimitiveBoundaryKind::missing_sync_service,
        0x14114BA88ULL, Monstey_Bindings.leave_cell, 8,
        Monstey_Bindings.leave_cell, Monstey_Target, Monstey_Lock);
  return Monstey_Services.sync(Monstey_Memory,
      {Monstey_GadbManagerSyncOperation::leave, 0x14114BA88ULL,
       Monstey_Bindings.leave_cell, Monstey_Target, Monstey_Lock,
       Monstey_Incoming});
}

Monstey_GadbBufferedResult Monstey_GadbDestroyManagerGuard(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Guard,
    Monstey_GadbBufferedResult Monstey_Incoming,
    const Monstey_GadbManagerPrimitiveBindings& Monstey_Bindings,
    const Monstey_GadbManagerPrimitiveServices& Monstey_Services) {
  const auto Monstey_Lock = Monstey_ManagerRead(Monstey_Memory, Monstey_Guard, 8);
  return Monstey_GadbLeaveManagerCriticalSection(
      Monstey_Memory, Monstey_Lock, Monstey_Incoming,
      Monstey_Bindings, Monstey_Services);
}

Monstey_GadbBufferedResult Monstey_GadbIncrementManagerTls(
    Monstey_GadbBinderMemory Monstey_Memory,
    const Monstey_GadbManagerPrimitiveBindings& Monstey_Bindings) {
  const auto Monstey_Array =
      Monstey_ManagerTlsArray(Monstey_Memory, Monstey_Bindings, 0x1406AAFDCULL);
  const auto Monstey_Base = Monstey_ManagerRead(Monstey_Memory, Monstey_Array, 8);
  const auto Monstey_CountAddress = Monstey_Base + std::uint64_t{0x2C};
  const auto Monstey_Old =
      static_cast<std::uint32_t>(Monstey_ManagerRead(Monstey_Memory, Monstey_CountAddress, 4));
  const auto Monstey_Next = std::uint32_t(Monstey_Old + std::uint32_t{1});
  Monstey_ManagerWrite(Monstey_Memory, Monstey_CountAddress, Monstey_Next, 4);
  return {Monstey_Array, 0x28};
}

Monstey_GadbBufferedResult Monstey_GadbDecrementManagerTls(
    Monstey_GadbBinderMemory Monstey_Memory,
    const Monstey_GadbManagerPrimitiveBindings& Monstey_Bindings) {
  const auto Monstey_Array =
      Monstey_ManagerTlsArray(Monstey_Memory, Monstey_Bindings, 0x1406AACFCULL);
  const auto Monstey_Base = Monstey_ManagerRead(Monstey_Memory, Monstey_Array, 8);
  const auto Monstey_CountAddress = Monstey_Base + std::uint64_t{0x2C};
  const auto Monstey_Old =
      static_cast<std::uint32_t>(Monstey_ManagerRead(Monstey_Memory, Monstey_CountAddress, 4));
  if (Monstey_Old == 0)
    return Monstey_GadbManagerNullAndReturn(Monstey_Memory, {0, 0x28});
  const auto Monstey_Next = std::uint32_t(Monstey_Old - std::uint32_t{1});
  Monstey_ManagerWrite(Monstey_Memory, Monstey_CountAddress, Monstey_Next, 4);
  return {Monstey_Next, 0x28};
}

Monstey_GadbBufferedResult Monstey_GadbManagerNullAndReturn(
    Monstey_GadbBinderMemory Monstey_Memory,
    Monstey_GadbBufferedResult Monstey_Incoming) {
  (void)Monstey_ManagerRead(Monstey_Memory, 0, 4);
  Monstey_ManagerWrite(Monstey_Memory, 0, 0, 4);
  return Monstey_Incoming;
}
} // namespace Monstey
