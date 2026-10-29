#pragma once
#include "Monstey_GadbBankSelectorRegisters.hpp"
#include "Monstey_GadbCategoryLookup.hpp"
#include "Monstey_GadbConcreteRecords.hpp"
namespace Monstey {
class Monstey_GadbCategoryDispatchFault final : public std::runtime_error {
public:
  std::uint64_t native_site, slot, target, expected;
  Monstey_GadbCategoryDispatchFault(std::uint64_t, std::uint64_t, std::uint64_t,
                                    std::uint64_t);
};
// Complete4BA07C+1410ED6CA owned effect profile (484B). F=frame_rbp is
// entryRSP-28h. Live stack arguments: F+50h stream, +58h selector object,
// +60h scalarU32, +68h pool, +70h descriptors, +78h record-cursor address,
// +80h value-cursor address. Known savedRCX atF+30h is stored on entry.
// Child argument stores F-38h/-48h/-50h remain observable; F-40h flags is
// deliberately not initialized. Supplied savedCPU/return/home words retain
// the dependencies' logical owned profile, not physical ABI emulation.
// The stable activation memory is exclusively borrowed, except synchronous
// primitive service effects. Recipes/typed overlays/view stay stable; all
// ordinary ranges consumed by this composition must avoid typed epoch words.
// Actual BufferedRead548/Named/Anonymous/Selector/Lookup/Hash are invoked.
// Only external copy/fill/ReadFile and rawnonnull comparison remain services.
// Live virtual targets are checked at their native consumed epochs. Original
// addresses are never invoked as host code. No allocation, early value fixup,
// serializer, full loader, game publication or historical load state is added.
Monstey_GadbBufferedResult Monstey_GadbLoadCategoryFromBuffered(
    Monstey_GadbBankActivation &, std::uint64_t parent, std::uint64_t bank_name,
    std::uint64_t category, std::uint64_t optional_lookup,
    std::uint64_t frame_rbp, const Monstey_GadbBankSelectorServices &,
    const Monstey_GadbBufferedServices &, Monstey_GadbBufferedCells = {},
    std::uint64_t compare_cell = 0x141D935B0ULL);
} // namespace Monstey
