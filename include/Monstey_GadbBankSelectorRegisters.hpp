#pragma once
#include "Monstey_FearDbCallable.hpp"
#include "Monstey_GadbBankActivation.hpp"
#include "Monstey_GadbBufferedStream.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
namespace Monstey {
class Monstey_GadbBankSelectorMemoryFault final : public std::runtime_error {
public:
  std::uint64_t address;
  std::size_t width;
  bool writing;
  Monstey_GadbBankSelectorMemoryFault(std::uint64_t, std::size_t, bool);
};
class Monstey_GadbBankSelectorOverlayFault final : public std::runtime_error {
public:
  std::uint64_t address;
  std::size_t width;
  bool writing;
  std::uint64_t overlay_identity;
  Monstey_GadbBankSelectorOverlayFault(std::uint64_t, std::size_t, bool,
                                       std::uint64_t);
};
struct Monstey_GadbBankSelectorCompareCall {
  std::uint64_t slot, target, left, right, native_site, tail_site;
};
class Monstey_GadbBankSelectorImportFault final : public std::runtime_error {
public:
  std::uint64_t slot, target, native_site;
  Monstey_GadbBankSelectorImportFault(std::uint64_t, std::uint64_t,
                                      std::uint64_t);
};
struct Monstey_GadbBankSelectorServices {
  Monstey_FearDbCallable<Monstey_GadbBufferedResult(
      Monstey_GadbBinderMemory, const Monstey_GadbBankSelectorCompareCall &)>
      compare_nonnull;
};
// Exact4BA96C body/17ADFC null branches. Q=bank_body_rsp=entryRSP-48h.
// CountU32 atQ+68h and arrayU64 atQ+20h are live ordinary bytes; AND0
// reads precede their writes. Native saved-register/return/home bytes and
// enum-private stack are not synthesized. Actual accepted enumeration is used.
// Memory/base/span, recipes and typed-overlay identities stay stable. Ordinary
// bytes are exclusively borrowed except synchronous provider effects. Providers
// may not byte-access guard/global overlays. Overlay checks precede range
// checks. Nonnull comparison, even equal pointers, reads IAT target live then
// consumes the sole external service. It provides rawRAX/RDX, not inferred
// CRT/locale/ABI. A remapped compare_cell declares an owned binding; native
// identity is default. Five guarded constant counts are positive: their opaque
// volatileRDX is dead before first class compare. Empty14482A370 preserves
// knownRDX=Q+20h. No normally returned register is fabricated. Faults preserve
// prior mutations.
Monstey_GadbBufferedResult Monstey_GadbFindParserBankClassRegisters(
    Monstey_GadbBankActivation &, std::uint64_t bank_name,
    std::uint64_t category_second_string, std::uint64_t bank_body_rsp,
    const Monstey_GadbBankSelectorServices &,
    std::uint64_t compare_cell = 0x141D935B0ULL);
} // namespace Monstey
