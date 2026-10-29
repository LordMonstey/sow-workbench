#pragma once
#include "Monstey_GadbCachedValue.hpp"
namespace Monstey {
enum class Monstey_NextKeyProbeGate {
    ReturnedZero,
    NamedActionLookupRequired,
    NonzeroNextKeyRequiresOwnedChild
};
struct Monstey_NextKeyProbeResult {
    Monstey_NextKeyProbeGate gate;
    // Not an observed value when gate is NamedActionLookupRequired.
    std::uint64_t raw_next_key;
    bool descriptor_count_consumed;
};
// Selected D17C80 route. State is the signed native field+30 snapshot;
// physical_cache is the caller-supplied field+40 byte, not its declaration ordinal.
// Null action or a cached zero next-key returns zero through BOTH nested-null
// branches. A nonnull slow action lookup and a nonzero child remain explicit
// gates: this probe neither resolves names nor converts a raw word to a pointer.
Monstey_NextKeyProbeResult Monstey_GadbProbeNextKeyAbsence(
    const Monstey_GadbCachedRecordView* action, std::int32_t field_state,
    std::uint8_t physical_cache);
// Metadata-only projection. It retains original classifications/totals, excludes
// the separately handled ConditionalBuff rows, and joins exact class pointers
// and declaration identities from the accepted read-only class inspector.
Monstey_Json Monstey_GadbInspectRemainingCountContracts(
    const Monstey_Json& audit, const Monstey_Json& declarations);
} // namespace Monstey
