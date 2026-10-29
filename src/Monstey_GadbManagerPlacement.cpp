#include "Monstey_GadbManagerPlacement.hpp"
#include <bit>
#include <limits>

namespace Monstey {
Monstey_GadbManagerCompareExchange32Fault::Monstey_GadbManagerCompareExchange32Fault(
    std::uint64_t Monstey_Site, std::uint64_t Monstey_Address,
    std::uint32_t Monstey_Comparand, std::uint32_t Monstey_Desired)
    : std::runtime_error("Unprovided SoW manager CMPXCHG32 primitive"),
      native_site(Monstey_Site), address(Monstey_Address),
      comparand(Monstey_Comparand), desired(Monstey_Desired) {}

namespace {
std::size_t Monstey_PlacementOffset(Monstey_GadbBinderMemory Monstey_Memory,
    std::uint64_t Monstey_Address, std::size_t Monstey_Width,
    bool Monstey_Writing) {
  if (Monstey_Address < Monstey_Memory.base ||
      Monstey_Address > std::numeric_limits<std::uint64_t>::max() - (Monstey_Width - 1) ||
      Monstey_Address - Monstey_Memory.base > Monstey_Memory.bytes.size() ||
      Monstey_Width > Monstey_Memory.bytes.size() -
          static_cast<std::size_t>(Monstey_Address - Monstey_Memory.base))
    throw Monstey_GadbBufferedMemoryFault(Monstey_Address, Monstey_Width, Monstey_Writing);
  return static_cast<std::size_t>(Monstey_Address - Monstey_Memory.base);
}
std::uint64_t Monstey_PlacementRead(Monstey_GadbBinderMemory Monstey_Memory,
    std::uint64_t Monstey_Address, std::size_t Monstey_Width) {
  const auto Monstey_Offset =
      Monstey_PlacementOffset(Monstey_Memory, Monstey_Address, Monstey_Width, false);
  std::uint64_t Monstey_Value = 0;
  for (std::size_t Monstey_Index = 0; Monstey_Index < Monstey_Width; ++Monstey_Index)
    Monstey_Value |= std::uint64_t(Monstey_Memory.bytes[Monstey_Offset + Monstey_Index])
        << (Monstey_Index * 8u);
  return Monstey_Value;
}
void Monstey_PlacementWrite(Monstey_GadbBinderMemory Monstey_Memory,
    std::uint64_t Monstey_Address, std::uint64_t Monstey_Value,
    std::size_t Monstey_Width) {
  const auto Monstey_Offset =
      Monstey_PlacementOffset(Monstey_Memory, Monstey_Address, Monstey_Width, true);
  for (std::size_t Monstey_Index = 0; Monstey_Index < Monstey_Width; ++Monstey_Index)
    Monstey_Memory.bytes[Monstey_Offset + Monstey_Index] =
        static_cast<std::uint8_t>(Monstey_Value >> (Monstey_Index * 8u));
}
} // namespace

Monstey_GadbBufferedResult Monstey_GadbResetManagerBackend40(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Object,
    std::uint64_t Monstey_IncomingRdx) {
  std::uint64_t Monstey_Header = 0;
  for (std::uint32_t Monstey_Index = 0; Monstey_Index < 32; ++Monstey_Index) {
    Monstey_Header = Monstey_Object +
        std::uint64_t(2u * Monstey_Index + 9u) * std::uint64_t{8};
    Monstey_PlacementWrite(Monstey_Memory, Monstey_Header + 0x18u, Monstey_Header, 8);
    Monstey_PlacementWrite(Monstey_Memory, Monstey_Header + 0x10u, Monstey_Header, 8);
  }
  return {Monstey_Header, Monstey_IncomingRdx};
}

Monstey_GadbBufferedResult Monstey_GadbResetManagerBackend80(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Object,
    std::uint64_t Monstey_IncomingRdx) {
  std::uint64_t Monstey_Header = 0;
  for (std::uint32_t Monstey_Index = 0; Monstey_Index < 32; ++Monstey_Index) {
    Monstey_Header = Monstey_Object +
        std::uint64_t(2u * Monstey_Index + 9u) * std::uint64_t{8};
    Monstey_PlacementWrite(Monstey_Memory, Monstey_Header + 0x18u, Monstey_Header, 8);
    Monstey_PlacementWrite(Monstey_Memory, Monstey_Header + 0x10u, Monstey_Header, 8);
  }
  return {Monstey_Header, Monstey_IncomingRdx};
}

Monstey_GadbBufferedResult Monstey_GadbResetManagerBackendC0(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Object,
    std::uint64_t Monstey_IncomingRdx) {
  std::uint64_t Monstey_Header = 0;
  for (std::uint32_t Monstey_Index = 0; Monstey_Index < 32; ++Monstey_Index) {
    Monstey_Header = Monstey_Object +
        std::uint64_t(2u * Monstey_Index + 9u) * std::uint64_t{8};
    Monstey_PlacementWrite(Monstey_Memory, Monstey_Header + 0x18u, Monstey_Header, 8);
    Monstey_PlacementWrite(Monstey_Memory, Monstey_Header + 0x10u, Monstey_Header, 8);
  }
  return {Monstey_Header, Monstey_IncomingRdx};
}

Monstey_GadbManagerTailResult Monstey_GadbInitializeManagerBackendTail40(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Object,
    std::uint64_t Monstey_Start, std::uint64_t Monstey_Span,
    Monstey_GadbManagerPlacementCells Monstey_Cells) {
  const auto Monstey_Residue = Monstey_Start & std::uint64_t{7};
  const auto Monstey_Pad = Monstey_Residue == 0 ? std::uint64_t{0}
      : (std::uint64_t{0} - Monstey_Residue) & std::uint64_t{7};
  const auto Monstey_Aligned = Monstey_Start + Monstey_Pad;
  const auto Monstey_Remaining = Monstey_Span - Monstey_Pad;
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Object + 0x28u, Monstey_Aligned, 8);
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Object + 0x10u, Monstey_Remaining, 8);
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Aligned + 8u, Monstey_Remaining | std::uint64_t{1}, 8);
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Aligned + Monstey_Remaining + 8u, 0x48, 8);
  const auto Monstey_Global = Monstey_PlacementRead(Monstey_Memory, Monstey_Cells.tail_40, 8);
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Object + 0x30u, Monstey_Global, 8);
  return {{Monstey_Global, Monstey_Aligned}, Monstey_Remaining};
}

Monstey_GadbManagerTailResult Monstey_GadbInitializeManagerBackendTail80(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Object,
    std::uint64_t Monstey_Start, std::uint64_t Monstey_Span,
    Monstey_GadbManagerPlacementCells Monstey_Cells) {
  const auto Monstey_Residue = Monstey_Start & std::uint64_t{7};
  const auto Monstey_Pad = Monstey_Residue == 0 ? std::uint64_t{0}
      : (std::uint64_t{0} - Monstey_Residue) & std::uint64_t{7};
  const auto Monstey_Aligned = Monstey_Start + Monstey_Pad;
  const auto Monstey_Remaining = Monstey_Span - Monstey_Pad;
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Object + 0x28u, Monstey_Aligned, 8);
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Object + 0x10u, Monstey_Remaining, 8);
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Aligned + 8u, Monstey_Remaining | std::uint64_t{1}, 8);
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Aligned + Monstey_Remaining + 8u, 0x48, 8);
  const auto Monstey_Global = Monstey_PlacementRead(Monstey_Memory, Monstey_Cells.tail_80, 8);
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Object + 0x30u, Monstey_Global, 8);
  return {{Monstey_Global, Monstey_Aligned}, Monstey_Remaining};
}

Monstey_GadbManagerTailResult Monstey_GadbInitializeManagerBackendTailC0(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Object,
    std::uint64_t Monstey_Start, std::uint64_t Monstey_Span,
    Monstey_GadbManagerPlacementCells Monstey_Cells) {
  const auto Monstey_Residue = Monstey_Start & std::uint64_t{7};
  const auto Monstey_Pad = Monstey_Residue == 0 ? std::uint64_t{0}
      : (std::uint64_t{0} - Monstey_Residue) & std::uint64_t{7};
  const auto Monstey_Aligned = Monstey_Start + Monstey_Pad;
  const auto Monstey_Remaining = Monstey_Span - Monstey_Pad;
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Object + 0x28u, Monstey_Aligned, 8);
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Object + 0x10u, Monstey_Remaining, 8);
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Aligned + 8u, Monstey_Remaining | std::uint64_t{1}, 8);
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Aligned + Monstey_Remaining + 8u, 0x48, 8);
  const auto Monstey_Global = Monstey_PlacementRead(Monstey_Memory, Monstey_Cells.tail_c0, 8);
  Monstey_PlacementWrite(Monstey_Memory, Monstey_Object + 0x30u, Monstey_Global, 8);
  return {{Monstey_Global, Monstey_Aligned}, Monstey_Remaining};
}

Monstey_GadbManagerCasResult Monstey_GadbCompareExchangeManagerState(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Target,
    std::uint64_t Monstey_ExpectedCell, std::uint64_t Monstey_BodyRsp,
    const Monstey_GadbManagerPlacementServices& Monstey_Services,
    Monstey_GadbManagerPlacementCells Monstey_Cells) {
  Monstey_PlacementWrite(Monstey_Memory, Monstey_BodyRsp + 0x20u, 5, 4);
  auto Monstey_Result = Monstey_GadbCompareExchangeManagerWord(
      Monstey_Memory, Monstey_Target, Monstey_ExpectedCell, 0xFFFFFFFFULL,
      Monstey_BodyRsp - std::uint64_t{8}, Monstey_Services, Monstey_Cells);
  Monstey_Result.raw.rax = std::uint32_t(Monstey_Result.raw.rax) != 0 ? 1u : 0u;
  return Monstey_Result;
}

Monstey_GadbManagerCasResult Monstey_GadbCompareExchangeManagerWord(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Target,
    std::uint64_t Monstey_ExpectedCell, std::uint64_t Monstey_DesiredRegister,
    std::uint64_t Monstey_BodyRsp,
    const Monstey_GadbManagerPlacementServices& Monstey_Services,
    Monstey_GadbManagerPlacementCells Monstey_Cells) {
  std::int32_t Monstey_Selected = 5;
  if (Monstey_PlacementRead(Monstey_Memory, Monstey_BodyRsp + 0x28u, 4) <= 5) {
    const auto Monstey_Selector = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(Monstey_PlacementRead(
            Monstey_Memory, Monstey_BodyRsp + 0x28u, 4)));
    const auto Monstey_TableAddress = Monstey_Cells.order_table +
        static_cast<std::uint64_t>(static_cast<std::int64_t>(Monstey_Selector)) *
            std::uint64_t{4};
    Monstey_Selected = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(
        Monstey_PlacementRead(Monstey_Memory, Monstey_TableAddress, 4)));
  }
  if (Monstey_Selected < 0 || Monstey_Selected > 5)
    return {{0, Monstey_ExpectedCell}, Monstey_DesiredRegister, Monstey_Target};
  const auto Monstey_Site = Monstey_Selected <= 3
      ? std::uint64_t{0x14059D608ULL} : std::uint64_t{0x14059D5F3ULL};
  const auto Monstey_Comparand = static_cast<std::uint32_t>(
      Monstey_PlacementRead(Monstey_Memory, Monstey_ExpectedCell, 4));
  const auto Monstey_Desired = static_cast<std::uint32_t>(Monstey_DesiredRegister);
  if (!Monstey_Services.compare_exchange32)
    throw Monstey_GadbManagerCompareExchange32Fault(
        Monstey_Site, Monstey_Target, Monstey_Comparand, Monstey_Desired);
  const auto Monstey_Observed = Monstey_Services.compare_exchange32(
      Monstey_Memory, {Monstey_Site, Monstey_Target, Monstey_Comparand, Monstey_Desired});
  const bool Monstey_Success = Monstey_Observed == Monstey_Comparand;
  if (!Monstey_Success)
    Monstey_PlacementWrite(Monstey_Memory, Monstey_ExpectedCell, Monstey_Observed, 4);
  return {{Monstey_Success ? std::uint64_t{1} : std::uint64_t{0}, Monstey_ExpectedCell},
          Monstey_DesiredRegister, Monstey_Target};
}
} // namespace Monstey
