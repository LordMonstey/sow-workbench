#include "Monstey_GadbManagerBackend.hpp"
#include <bit>
#include <limits>

namespace Monstey {
Monstey_GadbManagerBackendImportFault::Monstey_GadbManagerBackendImportFault(
    Monstey_GadbManagerBackendImport Monstey_Kind,
    const Monstey_GadbManagerBackendImportSite &Monstey_Site)
    : std::runtime_error("Unprovided SoW backend import primitive"),
      kind(Monstey_Kind), site(Monstey_Site) {}
Monstey_GadbManagerBackendExchange32Fault::Monstey_GadbManagerBackendExchange32Fault(
    std::uint64_t Monstey_Site, std::uint64_t Monstey_Address,
    std::uint32_t Monstey_Value)
    : std::runtime_error("Unprovided SoW backend XCHG32 primitive"),
      native_site(Monstey_Site), address(Monstey_Address), value(Monstey_Value) {}
namespace {
using Monstey_U64 = std::uint64_t;
using Monstey_U32 = std::uint32_t;
std::size_t Monstey_BackendOffset(Monstey_GadbBinderMemory Monstey_Memory,
    Monstey_U64 Monstey_Address, std::size_t Monstey_Width, bool Monstey_Writing) {
  if (Monstey_Address < Monstey_Memory.base ||
      Monstey_Address > std::numeric_limits<Monstey_U64>::max() - (Monstey_Width - 1) ||
      Monstey_Address - Monstey_Memory.base > Monstey_Memory.bytes.size() ||
      Monstey_Width > Monstey_Memory.bytes.size() -
          static_cast<std::size_t>(Monstey_Address - Monstey_Memory.base))
    throw Monstey_GadbBufferedMemoryFault(Monstey_Address, Monstey_Width, Monstey_Writing);
  return static_cast<std::size_t>(Monstey_Address - Monstey_Memory.base);
}
Monstey_U64 Monstey_BackendRead(Monstey_GadbBinderMemory Monstey_Memory,
    Monstey_U64 Monstey_Address, std::size_t Monstey_Width) {
  const auto Monstey_Offset = Monstey_BackendOffset(Monstey_Memory, Monstey_Address, Monstey_Width, false);
  Monstey_U64 Monstey_Value = 0;
  for (std::size_t Monstey_Index = 0; Monstey_Index < Monstey_Width; ++Monstey_Index)
    Monstey_Value |= Monstey_U64(Monstey_Memory.bytes[Monstey_Offset + Monstey_Index]) << (8u * Monstey_Index);
  return Monstey_Value;
}
void Monstey_BackendWrite(Monstey_GadbBinderMemory Monstey_Memory,
    Monstey_U64 Monstey_Address, Monstey_U64 Monstey_Value, std::size_t Monstey_Width) {
  const auto Monstey_Offset = Monstey_BackendOffset(Monstey_Memory, Monstey_Address, Monstey_Width, true);
  for (std::size_t Monstey_Index = 0; Monstey_Index < Monstey_Width; ++Monstey_Index)
    Monstey_Memory.bytes[Monstey_Offset + Monstey_Index] = static_cast<std::uint8_t>(Monstey_Value >> (8u * Monstey_Index));
}
void Monstey_BackendOr(Monstey_GadbBinderMemory Monstey_Memory,
    Monstey_U64 Monstey_Address, Monstey_U64 Monstey_Value, std::size_t Monstey_Width) {
  const auto Monstey_Old = Monstey_BackendRead(Monstey_Memory, Monstey_Address, Monstey_Width);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Address, Monstey_Old | Monstey_Value, Monstey_Width);
}
void Monstey_BackendAndZero(Monstey_GadbBinderMemory Monstey_Memory,
    Monstey_U64 Monstey_Address, std::size_t Monstey_Width) {
  static_cast<void>(Monstey_BackendRead(Monstey_Memory, Monstey_Address, Monstey_Width));
  Monstey_BackendWrite(Monstey_Memory, Monstey_Address, 0, Monstey_Width);
}
Monstey_GadbManagerBackendImportSite Monstey_BackendSite(
    Monstey_GadbBinderMemory Monstey_Memory, Monstey_U64 Monstey_PC,
    Monstey_U64 Monstey_Body, Monstey_U64 Monstey_Cell, bool Monstey_Fill = false) {
  const auto Monstey_Target = Monstey_BackendRead(Monstey_Memory, Monstey_Cell, 8);
  return {Monstey_Fill ? 0x1408C4EE0ULL : Monstey_PC, Monstey_PC,
      Monstey_Fill ? Monstey_Body - 8u : Monstey_Body, Monstey_Cell, Monstey_Target};
}
} // namespace
// Actual 141175f88: complete root body, numeric B supplied by caller.
Monstey_GadbBufferedResult Monstey_GadbCreateManagerBackend40(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_FlagsRegister,
    std::uint64_t Monstey_Backing, std::uint64_t Monstey_ByteCount,
    std::uint64_t Monstey_Body, const Monstey_GadbManagerBackendServices &Monstey_Services,
    Monstey_GadbManagerBackendBindings Monstey_Bindings) {
  const auto Monstey_Flags = static_cast<Monstey_U32>(Monstey_FlagsRegister);
  const auto Monstey_Mode = Monstey_Bindings.mode_40;
  if (Monstey_Backing == 0) {
    const auto Monstey_Descriptor = Monstey_Body + 0x20u;
    const auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141175FBBULL,
        Monstey_Body, Monstey_Bindings.imports.fill, true);
    if (!Monstey_Services.fill)
      throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::fill, Monstey_Site);
    const auto Monstey_Fill = Monstey_Services.fill(Monstey_Memory,
        {Monstey_Site, Monstey_Descriptor, 0x3D0u, 0});
    Monstey_BackendOr(Monstey_Memory, Monstey_Descriptor + 0x370u, Monstey_Flags, 4);
    const auto Monstey_Result = Monstey_GadbAllocateManagerBackend40(Monstey_Memory,
        Monstey_Descriptor, Monstey_Body - 0x30u, Monstey_Fill.rdx, Monstey_Services, Monstey_Bindings);
    if (Monstey_Result.rax != 0)
      Monstey_BackendOr(Monstey_Memory, Monstey_Result.rax + 0x370u, Monstey_Flags, 4);
    return Monstey_Result;
  }
  auto Monstey_Rdx = Monstey_FlagsRegister;
  if (Monstey_BackendRead(Monstey_Memory, Monstey_Mode.cookie, 8) == 0)
    Monstey_Rdx = Monstey_GadbInitializeManagerBackend40(Monstey_Memory,
        Monstey_Body - 0x60u, Monstey_Services, Monstey_Bindings).rdx;
  if (Monstey_ByteCount <= 0x420u)
    return {0, Monstey_Rdx};
  const auto Monstey_Page = Monstey_BackendRead(Monstey_Memory, Monstey_Mode.page, 8);
  const auto Monstey_Limit = Monstey_U64{0} - 0x420u - Monstey_Page;
  if (Monstey_ByteCount >= Monstey_Limit)
    return {0, Monstey_Rdx};
  const auto Monstey_Result = Monstey_GadbPlaceManagerBackend40(Monstey_Memory,
      Monstey_Backing, Monstey_ByteCount, Monstey_Body - 0x30u, Monstey_Services, Monstey_Bindings);
  Monstey_BackendOr(Monstey_Memory, Monstey_Result.raw.rax + 0x370u, 2, 4);
  Monstey_BackendOr(Monstey_Memory, Monstey_Result.raw.rax + 0x370u, Monstey_Flags, 4);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Result.raw.rax + 0x3B8u, 8, 4);
  return Monstey_Result.raw;
}

// Actual 141176058: complete allocation factory, numeric F supplied.
Monstey_GadbBufferedResult Monstey_GadbAllocateManagerBackend40(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Descriptor,
    std::uint64_t Monstey_Body, std::uint64_t Monstey_IncomingRdx,
    const Monstey_GadbManagerBackendServices &Monstey_Services,
    Monstey_GadbManagerBackendBindings Monstey_Bindings) {
  const auto Monstey_Mode = Monstey_Bindings.mode_40;
  auto Monstey_Rdx = Monstey_IncomingRdx;
  if (Monstey_BackendRead(Monstey_Memory, Monstey_Mode.cookie, 8) == 0)
    Monstey_Rdx = Monstey_GadbInitializeManagerBackend40(Monstey_Memory,
        Monstey_Body - 0x60u, Monstey_Services, Monstey_Bindings).rdx;
  if (Monstey_BackendRead(Monstey_Memory, Monstey_Mode.page, 8) == Monstey_U64{0} - 0x420u)
    return {0, Monstey_Rdx};
  const auto Monstey_Granularity = Monstey_BackendRead(Monstey_Memory, Monstey_Mode.granularity, 8);
  const auto Monstey_Span = (Monstey_Granularity * Monstey_U64{2} - 1u) & ~(Monstey_Granularity - 1u);
  Monstey_U32 Monstey_Mask = 0;
  if (Monstey_Descriptor != 0)
    Monstey_Mask = static_cast<Monstey_U32>(Monstey_BackendRead(Monstey_Memory, Monstey_Descriptor + 0x370u, 4)) & 0x300000u;
  const auto Monstey_Selected = Monstey_Mask != 0 ? Monstey_Mask : Monstey_U32{0x300000};
  const Monstey_U32 Monstey_Protection = (Monstey_Selected & 0x200000u) != 0 ? 4u : 2u;
  const auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x1411760D8ULL,
      Monstey_Body, Monstey_Bindings.imports.allocate);
  if (!Monstey_Services.allocate)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::allocate, Monstey_Site);
  const auto Monstey_Allocation = Monstey_Services.allocate(Monstey_Memory,
      {Monstey_Site, 0, Monstey_Span, 0x3000u, Monstey_Protection});
  if (Monstey_Allocation.rax - 1u > std::numeric_limits<Monstey_U64>::max() - 2u)
    return {0, Monstey_Allocation.rdx};
  const auto Monstey_Result = Monstey_GadbPlaceManagerBackend40(Monstey_Memory,
      Monstey_Allocation.rax, Monstey_Span, Monstey_Body - 0x30u, Monstey_Services, Monstey_Bindings);
  Monstey_BackendOr(Monstey_Memory, Monstey_Result.raw.rax + 0x370u, 2, 4);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Result.raw.rax + 0x3B8u, 1, 4);
  return Monstey_Result.raw;
}

// Actual 141175718: complete concrete placement; lower reset/tail are qualified Data114 bodies.
Monstey_GadbManagerTailResult Monstey_GadbPlaceManagerBackend40(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Backing,
    std::uint64_t Monstey_Span, std::uint64_t Monstey_Body,
    const Monstey_GadbManagerBackendServices &Monstey_Services,
    Monstey_GadbManagerBackendBindings Monstey_Bindings) {
  const auto Monstey_Mode = Monstey_Bindings.mode_40;
  const auto Monstey_Pad = (Monstey_U64{0} - (Monstey_Backing & 7u)) & 7u;
  const auto Monstey_Object = Monstey_Backing + 0x10u + Monstey_Pad;
  auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141175756ULL,
      Monstey_Body, Monstey_Bindings.imports.fill, true);
  if (!Monstey_Services.fill)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::fill, Monstey_Site);
  static_cast<void>(Monstey_Services.fill(Monstey_Memory, {Monstey_Site, Monstey_Object, 0x3D8u, 0}));
  Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141175767ULL,
      Monstey_Body, Monstey_Bindings.imports.initialize_cs);
  if (!Monstey_Services.initialize_cs)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::initialize_cs, Monstey_Site);
  const auto Monstey_Cs = Monstey_Services.initialize_cs(Monstey_Memory,
      {Monstey_Site, Monstey_Object + 0x378u, 0x80000FA0u});
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object - 8u, 0x3DBu, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x18u, Monstey_Backing, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x3A0u, Monstey_Backing, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x360u, Monstey_Span, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x358u, Monstey_Span, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x3A8u, Monstey_Span, 8);
  const auto Monstey_Cookie = Monstey_BackendRead(Monstey_Memory, Monstey_Mode.cookie, 8);
  Monstey_BackendOr(Monstey_Memory, Monstey_Object + 0x38u, std::numeric_limits<Monstey_U64>::max(), 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x40u, Monstey_Cookie, 8);
  const auto Monstey_Flags = Monstey_BackendRead(Monstey_Memory, Monstey_Mode.flags, 4);
  Monstey_BackendAndZero(Monstey_Memory, Monstey_Object + 0x3C0u, 8);
  Monstey_BackendAndZero(Monstey_Memory, Monstey_Object + 0x3C8u, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x370u, Monstey_Flags, 4);
  Monstey_BackendOr(Monstey_Memory, Monstey_Object + 0x370u, 4, 4);
  static_cast<void>(Monstey_GadbResetManagerBackend40(Monstey_Memory, Monstey_Object, Monstey_Cs.rdx));
  const auto Monstey_Header = Monstey_BackendRead(Monstey_Memory, Monstey_Object - 8u, 8);
  const auto Monstey_Start = (Monstey_Header & ~Monstey_U64{7}) - 0x10u + Monstey_Object;
  const auto Monstey_Remaining = Monstey_Span - 0x48u + (Monstey_Backing - Monstey_Start);
  auto Monstey_Result = Monstey_GadbInitializeManagerBackendTail40(Monstey_Memory,
      Monstey_Object, Monstey_Start, Monstey_Remaining, Monstey_Bindings.placement);
  Monstey_Result.raw.rax = Monstey_Object;
  return Monstey_Result;
}

// Actual 1411755f0: complete native state loop, known locals and global setup.
Monstey_GadbBufferedResult Monstey_GadbInitializeManagerBackend40(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Body,
    const Monstey_GadbManagerBackendServices &Monstey_Services,
    Monstey_GadbManagerBackendBindings Monstey_Bindings) {
  const auto Monstey_Mode = Monstey_Bindings.mode_40;
  auto Monstey_State = static_cast<Monstey_U32>(Monstey_BackendRead(Monstey_Memory, Monstey_Mode.state, 4));
  if (std::bit_cast<std::int32_t>(Monstey_State) <= 0) {
    for (;;) {
      Monstey_State = static_cast<Monstey_U32>(Monstey_BackendRead(Monstey_Memory, Monstey_Mode.state, 4));
      Monstey_BackendWrite(Monstey_Memory, Monstey_Body + 0x60u, Monstey_State, 4);
      if (std::bit_cast<std::int32_t>(Monstey_State) > 0)
        break;
      if (Monstey_State == 0) {
        const auto Monstey_Cas = Monstey_GadbCompareExchangeManagerState(Monstey_Memory,
            Monstey_Mode.state, Monstey_Body + 0x60u, Monstey_Body - 0x40u,
            Monstey_Services.placement, Monstey_Bindings.placement);
        if ((Monstey_Cas.raw.rax & 0xFFu) != 0) {
          const auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x14117563BULL,
              Monstey_Body, Monstey_Bindings.imports.initialize_cs);
          if (!Monstey_Services.initialize_cs)
            throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::initialize_cs, Monstey_Site);
          static_cast<void>(Monstey_Services.initialize_cs(Monstey_Memory,
              {Monstey_Site, Monstey_Mode.main_cs, 0x80000FA0u}));
          if (!Monstey_Services.exchange32)
            throw Monstey_GadbManagerBackendExchange32Fault(0x141175646ULL, Monstey_Mode.state, 1);
          static_cast<void>(Monstey_Services.exchange32(Monstey_Memory, {0x141175646ULL, Monstey_Mode.state, 1}));
          break;
        }
      }
      const auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141175627ULL,
          Monstey_Body, Monstey_Bindings.imports.sleep);
      if (!Monstey_Services.sleep)
        throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::sleep, Monstey_Site);
      Monstey_Services.sleep(Monstey_Memory, {Monstey_Site, 0, 0});
    }
  }
  auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141175653ULL,
      Monstey_Body, Monstey_Bindings.imports.enter_cs);
  if (!Monstey_Services.enter_cs)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::enter_cs, Monstey_Site);
  Monstey_Services.enter_cs(Monstey_Memory, {Monstey_Site, Monstey_Mode.main_cs});
  if (Monstey_BackendRead(Monstey_Memory, Monstey_Mode.cookie, 8) == 0) {
    Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x14117566CULL,
        Monstey_Body, Monstey_Bindings.imports.system_info);
    if (!Monstey_Services.system_info)
      throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::system_info, Monstey_Site);
    Monstey_Services.system_info(Monstey_Memory, {Monstey_Site, Monstey_Body + 0x20u});
    const auto Monstey_Granularity = Monstey_BackendRead(Monstey_Memory, Monstey_Body + 0x48u, 4);
    const auto Monstey_Page = Monstey_BackendRead(Monstey_Memory, Monstey_Body + 0x24u, 4);
    if ((Monstey_Granularity & (Monstey_Granularity - 1u)) != 0 ||
        (Monstey_Page & (Monstey_Page - 1u)) != 0)
      Monstey_BackendAndZero(Monstey_Memory, 0, 4);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.granularity, Monstey_Granularity, 8);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.auxiliary, std::numeric_limits<Monstey_U64>::max(), 8);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Bindings.placement.tail_40, std::numeric_limits<Monstey_U64>::max(), 8);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.page, Monstey_Page, 8);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.flags, 7, 4);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.secondary_flags, 7, 4);
    Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x1411756D9ULL,
        Monstey_Body, Monstey_Bindings.imports.initialize_cs);
    if (!Monstey_Services.initialize_cs)
      throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::initialize_cs, Monstey_Site);
    static_cast<void>(Monstey_Services.initialize_cs(Monstey_Memory,
        {Monstey_Site, Monstey_Mode.template_cs, 0x80000FA0u}));
    Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x1411756DFULL,
        Monstey_Body, Monstey_Bindings.imports.random);
    if (!Monstey_Services.random)
      throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::random, Monstey_Site);
    const auto Monstey_Random = Monstey_Services.random(Monstey_Memory, Monstey_Site);
    const auto Monstey_Signed = static_cast<Monstey_U64>(static_cast<std::int64_t>(std::bit_cast<std::int32_t>(Monstey_Random)));
    const auto Monstey_Cookie = ((Monstey_Signed & ~Monstey_U64{7}) ^ 0x55555550u) | 8u;
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.cookie, Monstey_Cookie, 8);
  }
  Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141175705ULL,
      Monstey_Body, Monstey_Bindings.imports.leave_cs);
  if (!Monstey_Services.leave_cs)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::leave_cs, Monstey_Site);
  const auto Monstey_Leave = Monstey_Services.leave_cs(Monstey_Memory, {Monstey_Site, Monstey_Mode.main_cs});
  return {1, Monstey_Leave.rdx};
}

// Actual 141174dbc: complete root body, numeric B supplied by caller.
Monstey_GadbBufferedResult Monstey_GadbCreateManagerBackend80(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_FlagsRegister,
    std::uint64_t Monstey_Backing, std::uint64_t Monstey_ByteCount,
    std::uint64_t Monstey_Body, const Monstey_GadbManagerBackendServices &Monstey_Services,
    Monstey_GadbManagerBackendBindings Monstey_Bindings) {
  const auto Monstey_Flags = static_cast<Monstey_U32>(Monstey_FlagsRegister);
  const auto Monstey_Mode = Monstey_Bindings.mode_80;
  if (Monstey_Backing == 0) {
    const auto Monstey_Descriptor = Monstey_Body + 0x20u;
    const auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141174DEFULL,
        Monstey_Body, Monstey_Bindings.imports.fill, true);
    if (!Monstey_Services.fill)
      throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::fill, Monstey_Site);
    const auto Monstey_Fill = Monstey_Services.fill(Monstey_Memory,
        {Monstey_Site, Monstey_Descriptor, 0x3D0u, 0});
    Monstey_BackendOr(Monstey_Memory, Monstey_Descriptor + 0x370u, Monstey_Flags, 4);
    const auto Monstey_Result = Monstey_GadbAllocateManagerBackend80(Monstey_Memory,
        Monstey_Descriptor, Monstey_Body - 0x30u, Monstey_Fill.rdx, Monstey_Services, Monstey_Bindings);
    if (Monstey_Result.rax != 0)
      Monstey_BackendOr(Monstey_Memory, Monstey_Result.rax + 0x370u, Monstey_Flags, 4);
    return Monstey_Result;
  }
  auto Monstey_Rdx = Monstey_FlagsRegister;
  if (Monstey_BackendRead(Monstey_Memory, Monstey_Mode.cookie, 8) == 0)
    Monstey_Rdx = Monstey_GadbInitializeManagerBackend80(Monstey_Memory,
        Monstey_Body - 0x60u, Monstey_Services, Monstey_Bindings).rdx;
  if (Monstey_ByteCount <= 0x420u)
    return {0, Monstey_Rdx};
  const auto Monstey_Page = Monstey_BackendRead(Monstey_Memory, Monstey_Mode.page, 8);
  const auto Monstey_Limit = Monstey_U64{0} - 0x420u - Monstey_Page;
  if (Monstey_ByteCount >= Monstey_Limit)
    return {0, Monstey_Rdx};
  const auto Monstey_Result = Monstey_GadbPlaceManagerBackend80(Monstey_Memory,
      Monstey_Backing, Monstey_ByteCount, Monstey_Body - 0x30u, Monstey_Services, Monstey_Bindings);
  Monstey_BackendOr(Monstey_Memory, Monstey_Result.raw.rax + 0x370u, 2, 4);
  Monstey_BackendOr(Monstey_Memory, Monstey_Result.raw.rax + 0x370u, Monstey_Flags, 4);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Result.raw.rax + 0x3B8u, 8, 4);
  return Monstey_Result.raw;
}

// Actual 141174ea8: complete allocation factory, numeric F supplied.
Monstey_GadbBufferedResult Monstey_GadbAllocateManagerBackend80(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Descriptor,
    std::uint64_t Monstey_Body, std::uint64_t Monstey_IncomingRdx,
    const Monstey_GadbManagerBackendServices &Monstey_Services,
    Monstey_GadbManagerBackendBindings Monstey_Bindings) {
  const auto Monstey_Mode = Monstey_Bindings.mode_80;
  auto Monstey_Rdx = Monstey_IncomingRdx;
  if (Monstey_BackendRead(Monstey_Memory, Monstey_Mode.cookie, 8) == 0)
    Monstey_Rdx = Monstey_GadbInitializeManagerBackend80(Monstey_Memory,
        Monstey_Body - 0x60u, Monstey_Services, Monstey_Bindings).rdx;
  if (Monstey_BackendRead(Monstey_Memory, Monstey_Mode.page, 8) == Monstey_U64{0} - 0x420u)
    return {0, Monstey_Rdx};
  const auto Monstey_Granularity = Monstey_BackendRead(Monstey_Memory, Monstey_Mode.granularity, 8);
  const auto Monstey_Span = (Monstey_Granularity * Monstey_U64{2} - 1u) & ~(Monstey_Granularity - 1u);
  Monstey_U32 Monstey_Mask = 0;
  if (Monstey_Descriptor != 0)
    Monstey_Mask = static_cast<Monstey_U32>(Monstey_BackendRead(Monstey_Memory, Monstey_Descriptor + 0x370u, 4)) & 0x300000u;
  const auto Monstey_Selected = Monstey_Mask != 0 ? Monstey_Mask : Monstey_U32{0x300000};
  const Monstey_U32 Monstey_Protection = (Monstey_Selected & 0x200000u) != 0 ? 4u : 2u;
  const auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141174F28ULL,
      Monstey_Body, Monstey_Bindings.imports.allocate);
  if (!Monstey_Services.allocate)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::allocate, Monstey_Site);
  const auto Monstey_Allocation = Monstey_Services.allocate(Monstey_Memory,
      {Monstey_Site, 0, Monstey_Span, 0x3000u, Monstey_Protection});
  if (Monstey_Allocation.rax - 1u > std::numeric_limits<Monstey_U64>::max() - 2u)
    return {0, Monstey_Allocation.rdx};
  const auto Monstey_Result = Monstey_GadbPlaceManagerBackend80(Monstey_Memory,
      Monstey_Allocation.rax, Monstey_Span, Monstey_Body - 0x30u, Monstey_Services, Monstey_Bindings);
  Monstey_BackendOr(Monstey_Memory, Monstey_Result.raw.rax + 0x370u, 2, 4);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Result.raw.rax + 0x3B8u, 1, 4);
  return Monstey_Result.raw;
}

// Actual 141173ba0: complete concrete placement; lower reset/tail are qualified Data114 bodies.
Monstey_GadbManagerTailResult Monstey_GadbPlaceManagerBackend80(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Backing,
    std::uint64_t Monstey_Span, std::uint64_t Monstey_Body,
    const Monstey_GadbManagerBackendServices &Monstey_Services,
    Monstey_GadbManagerBackendBindings Monstey_Bindings) {
  const auto Monstey_Mode = Monstey_Bindings.mode_80;
  const auto Monstey_Pad = (Monstey_U64{0} - (Monstey_Backing & 7u)) & 7u;
  const auto Monstey_Object = Monstey_Backing + 0x10u + Monstey_Pad;
  auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141173BDEULL,
      Monstey_Body, Monstey_Bindings.imports.fill, true);
  if (!Monstey_Services.fill)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::fill, Monstey_Site);
  static_cast<void>(Monstey_Services.fill(Monstey_Memory, {Monstey_Site, Monstey_Object, 0x3D8u, 0}));
  Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141173BEFULL,
      Monstey_Body, Monstey_Bindings.imports.initialize_cs);
  if (!Monstey_Services.initialize_cs)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::initialize_cs, Monstey_Site);
  const auto Monstey_Cs = Monstey_Services.initialize_cs(Monstey_Memory,
      {Monstey_Site, Monstey_Object + 0x378u, 0x80000FA0u});
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object - 8u, 0x3DBu, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x18u, Monstey_Backing, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x3A0u, Monstey_Backing, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x360u, Monstey_Span, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x358u, Monstey_Span, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x3A8u, Monstey_Span, 8);
  const auto Monstey_Cookie = Monstey_BackendRead(Monstey_Memory, Monstey_Mode.cookie, 8);
  Monstey_BackendOr(Monstey_Memory, Monstey_Object + 0x38u, std::numeric_limits<Monstey_U64>::max(), 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x40u, Monstey_Cookie, 8);
  const auto Monstey_Flags = Monstey_BackendRead(Monstey_Memory, Monstey_Mode.flags, 4);
  Monstey_BackendAndZero(Monstey_Memory, Monstey_Object + 0x3C0u, 8);
  Monstey_BackendAndZero(Monstey_Memory, Monstey_Object + 0x3C8u, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x370u, Monstey_Flags, 4);
  Monstey_BackendOr(Monstey_Memory, Monstey_Object + 0x370u, 4, 4);
  static_cast<void>(Monstey_GadbResetManagerBackend80(Monstey_Memory, Monstey_Object, Monstey_Cs.rdx));
  const auto Monstey_Header = Monstey_BackendRead(Monstey_Memory, Monstey_Object - 8u, 8);
  const auto Monstey_Start = (Monstey_Header & ~Monstey_U64{7}) - 0x10u + Monstey_Object;
  const auto Monstey_Remaining = Monstey_Span - 0x48u + (Monstey_Backing - Monstey_Start);
  auto Monstey_Result = Monstey_GadbInitializeManagerBackendTail80(Monstey_Memory,
      Monstey_Object, Monstey_Start, Monstey_Remaining, Monstey_Bindings.placement);
  Monstey_Result.raw.rax = Monstey_Object;
  return Monstey_Result;
}

// Actual 141173a38: complete native state loop, known locals and global setup.
Monstey_GadbBufferedResult Monstey_GadbInitializeManagerBackend80(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Body,
    const Monstey_GadbManagerBackendServices &Monstey_Services,
    Monstey_GadbManagerBackendBindings Monstey_Bindings) {
  const auto Monstey_Mode = Monstey_Bindings.mode_80;
  auto Monstey_State = static_cast<Monstey_U32>(Monstey_BackendRead(Monstey_Memory, Monstey_Mode.state, 4));
  if (std::bit_cast<std::int32_t>(Monstey_State) <= 0) {
    for (;;) {
      Monstey_State = static_cast<Monstey_U32>(Monstey_BackendRead(Monstey_Memory, Monstey_Mode.state, 4));
      Monstey_BackendWrite(Monstey_Memory, Monstey_Body + 0x60u, Monstey_State, 4);
      if (std::bit_cast<std::int32_t>(Monstey_State) > 0)
        break;
      if (Monstey_State == 0) {
        const auto Monstey_Cas = Monstey_GadbCompareExchangeManagerState(Monstey_Memory,
            Monstey_Mode.state, Monstey_Body + 0x60u, Monstey_Body - 0x40u,
            Monstey_Services.placement, Monstey_Bindings.placement);
        if ((Monstey_Cas.raw.rax & 0xFFu) != 0) {
          const auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141173A83ULL,
              Monstey_Body, Monstey_Bindings.imports.initialize_cs);
          if (!Monstey_Services.initialize_cs)
            throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::initialize_cs, Monstey_Site);
          static_cast<void>(Monstey_Services.initialize_cs(Monstey_Memory,
              {Monstey_Site, Monstey_Mode.main_cs, 0x80000FA0u}));
          if (!Monstey_Services.exchange32)
            throw Monstey_GadbManagerBackendExchange32Fault(0x141173A8EULL, Monstey_Mode.state, 1);
          static_cast<void>(Monstey_Services.exchange32(Monstey_Memory, {0x141173A8EULL, Monstey_Mode.state, 1}));
          break;
        }
      }
      const auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141173A6FULL,
          Monstey_Body, Monstey_Bindings.imports.sleep);
      if (!Monstey_Services.sleep)
        throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::sleep, Monstey_Site);
      Monstey_Services.sleep(Monstey_Memory, {Monstey_Site, 0, 0});
    }
  }
  auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141173A9BULL,
      Monstey_Body, Monstey_Bindings.imports.enter_cs);
  if (!Monstey_Services.enter_cs)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::enter_cs, Monstey_Site);
  Monstey_Services.enter_cs(Monstey_Memory, {Monstey_Site, Monstey_Mode.main_cs});
  if (Monstey_BackendRead(Monstey_Memory, Monstey_Mode.cookie, 8) == 0) {
    Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141173AB4ULL,
        Monstey_Body, Monstey_Bindings.imports.system_info);
    if (!Monstey_Services.system_info)
      throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::system_info, Monstey_Site);
    Monstey_Services.system_info(Monstey_Memory, {Monstey_Site, Monstey_Body + 0x20u});
    const auto Monstey_Granularity = Monstey_BackendRead(Monstey_Memory, Monstey_Body + 0x48u, 4);
    const auto Monstey_Page = Monstey_BackendRead(Monstey_Memory, Monstey_Body + 0x24u, 4);
    if ((Monstey_Granularity & (Monstey_Granularity - 1u)) != 0 ||
        (Monstey_Page & (Monstey_Page - 1u)) != 0)
      Monstey_BackendAndZero(Monstey_Memory, 0, 4);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.granularity, Monstey_Granularity, 8);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.auxiliary, std::numeric_limits<Monstey_U64>::max(), 8);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Bindings.placement.tail_80, std::numeric_limits<Monstey_U64>::max(), 8);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.page, Monstey_Page, 8);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.flags, 7, 4);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.secondary_flags, 7, 4);
    Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141173B21ULL,
        Monstey_Body, Monstey_Bindings.imports.initialize_cs);
    if (!Monstey_Services.initialize_cs)
      throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::initialize_cs, Monstey_Site);
    static_cast<void>(Monstey_Services.initialize_cs(Monstey_Memory,
        {Monstey_Site, Monstey_Mode.template_cs, 0x80000FA0u}));
    Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141173B27ULL,
        Monstey_Body, Monstey_Bindings.imports.random);
    if (!Monstey_Services.random)
      throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::random, Monstey_Site);
    const auto Monstey_Random = Monstey_Services.random(Monstey_Memory, Monstey_Site);
    const auto Monstey_Signed = static_cast<Monstey_U64>(static_cast<std::int64_t>(std::bit_cast<std::int32_t>(Monstey_Random)));
    const auto Monstey_Cookie = ((Monstey_Signed & ~Monstey_U64{7}) ^ 0x55555550u) | 8u;
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.cookie, Monstey_Cookie, 8);
  }
  Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141173B4DULL,
      Monstey_Body, Monstey_Bindings.imports.leave_cs);
  if (!Monstey_Services.leave_cs)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::leave_cs, Monstey_Site);
  const auto Monstey_Leave = Monstey_Services.leave_cs(Monstey_Memory, {Monstey_Site, Monstey_Mode.main_cs});
  return {1, Monstey_Leave.rdx};
}

// Actual 141177ecc: complete root body, numeric B supplied by caller.
Monstey_GadbBufferedResult Monstey_GadbCreateManagerBackendC0(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_FlagsRegister,
    std::uint64_t Monstey_Backing, std::uint64_t Monstey_ByteCount,
    std::uint64_t Monstey_Body, const Monstey_GadbManagerBackendServices &Monstey_Services,
    Monstey_GadbManagerBackendBindings Monstey_Bindings) {
  const auto Monstey_Flags = static_cast<Monstey_U32>(Monstey_FlagsRegister);
  const auto Monstey_Mode = Monstey_Bindings.mode_c0;
  if (Monstey_Backing == 0) {
    const auto Monstey_Descriptor = Monstey_Body + 0x20u;
    const auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141177EFFULL,
        Monstey_Body, Monstey_Bindings.imports.fill, true);
    if (!Monstey_Services.fill)
      throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::fill, Monstey_Site);
    const auto Monstey_Fill = Monstey_Services.fill(Monstey_Memory,
        {Monstey_Site, Monstey_Descriptor, 0x3D0u, 0});
    Monstey_BackendOr(Monstey_Memory, Monstey_Descriptor + 0x370u, Monstey_Flags, 4);
    const auto Monstey_Result = Monstey_GadbAllocateManagerBackendC0(Monstey_Memory,
        Monstey_Descriptor, Monstey_Body - 0x30u, Monstey_Fill.rdx, Monstey_Services, Monstey_Bindings);
    if (Monstey_Result.rax != 0)
      Monstey_BackendOr(Monstey_Memory, Monstey_Result.rax + 0x370u, Monstey_Flags, 4);
    return Monstey_Result;
  }
  auto Monstey_Rdx = Monstey_FlagsRegister;
  if (Monstey_BackendRead(Monstey_Memory, Monstey_Mode.cookie, 8) == 0)
    Monstey_Rdx = Monstey_GadbInitializeManagerBackendC0(Monstey_Memory,
        Monstey_Body - 0x60u, Monstey_Services, Monstey_Bindings).rdx;
  if (Monstey_ByteCount <= 0x420u)
    return {0, Monstey_Rdx};
  const auto Monstey_Page = Monstey_BackendRead(Monstey_Memory, Monstey_Mode.page, 8);
  const auto Monstey_Limit = Monstey_U64{0} - 0x420u - Monstey_Page;
  if (Monstey_ByteCount >= Monstey_Limit)
    return {0, Monstey_Rdx};
  const auto Monstey_Result = Monstey_GadbPlaceManagerBackendC0(Monstey_Memory,
      Monstey_Backing, Monstey_ByteCount, Monstey_Body - 0x30u, Monstey_Services, Monstey_Bindings);
  Monstey_BackendOr(Monstey_Memory, Monstey_Result.raw.rax + 0x370u, 2, 4);
  Monstey_BackendOr(Monstey_Memory, Monstey_Result.raw.rax + 0x370u, Monstey_Flags, 4);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Result.raw.rax + 0x3B8u, 8, 4);
  return Monstey_Result.raw;
}

// Actual 141177fb8: complete allocation factory, numeric F supplied.
Monstey_GadbBufferedResult Monstey_GadbAllocateManagerBackendC0(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Descriptor,
    std::uint64_t Monstey_Body, std::uint64_t Monstey_IncomingRdx,
    const Monstey_GadbManagerBackendServices &Monstey_Services,
    Monstey_GadbManagerBackendBindings Monstey_Bindings) {
  const auto Monstey_Mode = Monstey_Bindings.mode_c0;
  auto Monstey_Rdx = Monstey_IncomingRdx;
  if (Monstey_BackendRead(Monstey_Memory, Monstey_Mode.cookie, 8) == 0)
    Monstey_Rdx = Monstey_GadbInitializeManagerBackendC0(Monstey_Memory,
        Monstey_Body - 0x60u, Monstey_Services, Monstey_Bindings).rdx;
  if (Monstey_BackendRead(Monstey_Memory, Monstey_Mode.page, 8) == Monstey_U64{0} - 0x420u)
    return {0, Monstey_Rdx};
  const auto Monstey_Granularity = Monstey_BackendRead(Monstey_Memory, Monstey_Mode.granularity, 8);
  const auto Monstey_Span = (Monstey_Granularity * Monstey_U64{2} - 1u) & ~(Monstey_Granularity - 1u);
  Monstey_U32 Monstey_Mask = 0;
  if (Monstey_Descriptor != 0)
    Monstey_Mask = static_cast<Monstey_U32>(Monstey_BackendRead(Monstey_Memory, Monstey_Descriptor + 0x370u, 4)) & 0x300000u;
  const auto Monstey_Selected = Monstey_Mask != 0 ? Monstey_Mask : Monstey_U32{0x300000};
  const Monstey_U32 Monstey_Protection = (Monstey_Selected & 0x200000u) != 0 ? 4u : 2u;
  const auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141178038ULL,
      Monstey_Body, Monstey_Bindings.imports.allocate);
  if (!Monstey_Services.allocate)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::allocate, Monstey_Site);
  const auto Monstey_Allocation = Monstey_Services.allocate(Monstey_Memory,
      {Monstey_Site, 0, Monstey_Span, 0x3000u, Monstey_Protection});
  if (Monstey_Allocation.rax - 1u > std::numeric_limits<Monstey_U64>::max() - 2u)
    return {0, Monstey_Allocation.rdx};
  const auto Monstey_Result = Monstey_GadbPlaceManagerBackendC0(Monstey_Memory,
      Monstey_Allocation.rax, Monstey_Span, Monstey_Body - 0x30u, Monstey_Services, Monstey_Bindings);
  Monstey_BackendOr(Monstey_Memory, Monstey_Result.raw.rax + 0x370u, 2, 4);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Result.raw.rax + 0x3B8u, 1, 4);
  return Monstey_Result.raw;
}

// Actual 141176cb0: complete concrete placement; lower reset/tail are qualified Data114 bodies.
Monstey_GadbManagerTailResult Monstey_GadbPlaceManagerBackendC0(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Backing,
    std::uint64_t Monstey_Span, std::uint64_t Monstey_Body,
    const Monstey_GadbManagerBackendServices &Monstey_Services,
    Monstey_GadbManagerBackendBindings Monstey_Bindings) {
  const auto Monstey_Mode = Monstey_Bindings.mode_c0;
  const auto Monstey_Pad = (Monstey_U64{0} - (Monstey_Backing & 7u)) & 7u;
  const auto Monstey_Object = Monstey_Backing + 0x10u + Monstey_Pad;
  auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141176CEEULL,
      Monstey_Body, Monstey_Bindings.imports.fill, true);
  if (!Monstey_Services.fill)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::fill, Monstey_Site);
  static_cast<void>(Monstey_Services.fill(Monstey_Memory, {Monstey_Site, Monstey_Object, 0x3D8u, 0}));
  Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141176CFFULL,
      Monstey_Body, Monstey_Bindings.imports.initialize_cs);
  if (!Monstey_Services.initialize_cs)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::initialize_cs, Monstey_Site);
  const auto Monstey_Cs = Monstey_Services.initialize_cs(Monstey_Memory,
      {Monstey_Site, Monstey_Object + 0x378u, 0x80000FA0u});
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object - 8u, 0x3DBu, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x18u, Monstey_Backing, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x3A0u, Monstey_Backing, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x360u, Monstey_Span, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x358u, Monstey_Span, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x3A8u, Monstey_Span, 8);
  const auto Monstey_Cookie = Monstey_BackendRead(Monstey_Memory, Monstey_Mode.cookie, 8);
  Monstey_BackendOr(Monstey_Memory, Monstey_Object + 0x38u, std::numeric_limits<Monstey_U64>::max(), 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x40u, Monstey_Cookie, 8);
  const auto Monstey_Flags = Monstey_BackendRead(Monstey_Memory, Monstey_Mode.flags, 4);
  Monstey_BackendAndZero(Monstey_Memory, Monstey_Object + 0x3C0u, 8);
  Monstey_BackendAndZero(Monstey_Memory, Monstey_Object + 0x3C8u, 8);
  Monstey_BackendWrite(Monstey_Memory, Monstey_Object + 0x370u, Monstey_Flags, 4);
  Monstey_BackendOr(Monstey_Memory, Monstey_Object + 0x370u, 4, 4);
  static_cast<void>(Monstey_GadbResetManagerBackendC0(Monstey_Memory, Monstey_Object, Monstey_Cs.rdx));
  const auto Monstey_Header = Monstey_BackendRead(Monstey_Memory, Monstey_Object - 8u, 8);
  const auto Monstey_Start = (Monstey_Header & ~Monstey_U64{7}) - 0x10u + Monstey_Object;
  const auto Monstey_Remaining = Monstey_Span - 0x48u + (Monstey_Backing - Monstey_Start);
  auto Monstey_Result = Monstey_GadbInitializeManagerBackendTailC0(Monstey_Memory,
      Monstey_Object, Monstey_Start, Monstey_Remaining, Monstey_Bindings.placement);
  Monstey_Result.raw.rax = Monstey_Object;
  return Monstey_Result;
}

// Actual 141176b48: complete native state loop, known locals and global setup.
Monstey_GadbBufferedResult Monstey_GadbInitializeManagerBackendC0(
    Monstey_GadbBinderMemory Monstey_Memory, std::uint64_t Monstey_Body,
    const Monstey_GadbManagerBackendServices &Monstey_Services,
    Monstey_GadbManagerBackendBindings Monstey_Bindings) {
  const auto Monstey_Mode = Monstey_Bindings.mode_c0;
  auto Monstey_State = static_cast<Monstey_U32>(Monstey_BackendRead(Monstey_Memory, Monstey_Mode.state, 4));
  if (std::bit_cast<std::int32_t>(Monstey_State) <= 0) {
    for (;;) {
      Monstey_State = static_cast<Monstey_U32>(Monstey_BackendRead(Monstey_Memory, Monstey_Mode.state, 4));
      Monstey_BackendWrite(Monstey_Memory, Monstey_Body + 0x60u, Monstey_State, 4);
      if (std::bit_cast<std::int32_t>(Monstey_State) > 0)
        break;
      if (Monstey_State == 0) {
        const auto Monstey_Cas = Monstey_GadbCompareExchangeManagerState(Monstey_Memory,
            Monstey_Mode.state, Monstey_Body + 0x60u, Monstey_Body - 0x40u,
            Monstey_Services.placement, Monstey_Bindings.placement);
        if ((Monstey_Cas.raw.rax & 0xFFu) != 0) {
          const auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141176B93ULL,
              Monstey_Body, Monstey_Bindings.imports.initialize_cs);
          if (!Monstey_Services.initialize_cs)
            throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::initialize_cs, Monstey_Site);
          static_cast<void>(Monstey_Services.initialize_cs(Monstey_Memory,
              {Monstey_Site, Monstey_Mode.main_cs, 0x80000FA0u}));
          if (!Monstey_Services.exchange32)
            throw Monstey_GadbManagerBackendExchange32Fault(0x141176B9EULL, Monstey_Mode.state, 1);
          static_cast<void>(Monstey_Services.exchange32(Monstey_Memory, {0x141176B9EULL, Monstey_Mode.state, 1}));
          break;
        }
      }
      const auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141176B7FULL,
          Monstey_Body, Monstey_Bindings.imports.sleep);
      if (!Monstey_Services.sleep)
        throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::sleep, Monstey_Site);
      Monstey_Services.sleep(Monstey_Memory, {Monstey_Site, 0, 0});
    }
  }
  auto Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141176BABULL,
      Monstey_Body, Monstey_Bindings.imports.enter_cs);
  if (!Monstey_Services.enter_cs)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::enter_cs, Monstey_Site);
  Monstey_Services.enter_cs(Monstey_Memory, {Monstey_Site, Monstey_Mode.main_cs});
  if (Monstey_BackendRead(Monstey_Memory, Monstey_Mode.cookie, 8) == 0) {
    Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141176BC4ULL,
        Monstey_Body, Monstey_Bindings.imports.system_info);
    if (!Monstey_Services.system_info)
      throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::system_info, Monstey_Site);
    Monstey_Services.system_info(Monstey_Memory, {Monstey_Site, Monstey_Body + 0x20u});
    const auto Monstey_Granularity = Monstey_BackendRead(Monstey_Memory, Monstey_Body + 0x48u, 4);
    const auto Monstey_Page = Monstey_BackendRead(Monstey_Memory, Monstey_Body + 0x24u, 4);
    if ((Monstey_Granularity & (Monstey_Granularity - 1u)) != 0 ||
        (Monstey_Page & (Monstey_Page - 1u)) != 0)
      Monstey_BackendAndZero(Monstey_Memory, 0, 4);
    Monstey_BackendOr(Monstey_Memory, Monstey_Bindings.placement.tail_c0, std::numeric_limits<Monstey_U64>::max(), 8);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.granularity, Monstey_Granularity, 8);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.flags, 7, 4);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.secondary_flags, 7, 4);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.page, Monstey_Page, 8);
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.auxiliary, 0x400000u, 8);
    Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141176C32ULL,
        Monstey_Body, Monstey_Bindings.imports.initialize_cs);
    if (!Monstey_Services.initialize_cs)
      throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::initialize_cs, Monstey_Site);
    static_cast<void>(Monstey_Services.initialize_cs(Monstey_Memory,
        {Monstey_Site, Monstey_Mode.template_cs, 0x80000FA0u}));
    Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141176C38ULL,
        Monstey_Body, Monstey_Bindings.imports.random);
    if (!Monstey_Services.random)
      throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::random, Monstey_Site);
    const auto Monstey_Random = Monstey_Services.random(Monstey_Memory, Monstey_Site);
    const auto Monstey_Signed = static_cast<Monstey_U64>(static_cast<std::int64_t>(std::bit_cast<std::int32_t>(Monstey_Random)));
    const auto Monstey_Cookie = ((Monstey_Signed & ~Monstey_U64{7}) ^ 0x55555550u) | 8u;
    Monstey_BackendWrite(Monstey_Memory, Monstey_Mode.cookie, Monstey_Cookie, 8);
  }
  Monstey_Site = Monstey_BackendSite(Monstey_Memory, 0x141176C5EULL,
      Monstey_Body, Monstey_Bindings.imports.leave_cs);
  if (!Monstey_Services.leave_cs)
    throw Monstey_GadbManagerBackendImportFault(Monstey_GadbManagerBackendImport::leave_cs, Monstey_Site);
  const auto Monstey_Leave = Monstey_Services.leave_cs(Monstey_Memory, {Monstey_Site, Monstey_Mode.main_cs});
  return {1, Monstey_Leave.rdx};
}

} // namespace Monstey
