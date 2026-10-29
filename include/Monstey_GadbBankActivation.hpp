#pragma once
#include "Monstey_GadbBankSynchronization.hpp"
#include "Monstey_GadbBinderComposition.hpp"
#include "Monstey_GadbNamePointerBoundary.hpp"
#include <array>
#include <span>
#include <vector>
namespace Monstey {
// Immutable constructor constants, not captured live BSS. They must remain
// stable throughout activation; class/array bytes in memory may legitimately
// differ on a cached epoch path and are never replaced by this recipe then.
struct Monstey_GadbBankRecipe {
    std::uint64_t enumerator = 0, guard_identity = 0, returned_array = 0;
    std::uint32_t returned_count = 0;
    std::vector<std::uint64_t> constructor_classes;
};
// Uses the accepted self-contained C++ PE reader. No original constructor runs.
// Guard identities are qualified native constants, not inferred live values.
std::array<Monstey_GadbBankRecipe, 6> Monstey_GadbActivationRecipes(
    std::span<const std::uint8_t> qualifiedImage);
struct Monstey_GadbBankActivation {
    Monstey_GadbBinderMemory memory;
    std::span<const Monstey_GadbBankRecipe> recipes;
    std::span<Monstey_GadbBankEpochWord *const> guards;
    Monstey_GadbBankEpochWord &global_epoch;
    std::int32_t &thread_epoch;
    Monstey_GadbBankSynchronizer &synchronization;
    std::uint64_t bank_table;
    const Monstey_GadbNonnullPointerComparator &external_comparator;
};
// Guard/global identities are authoritative typed overlays outside the byte
// arena's ordinary-access contract. Raw byte writes/reads to those identities
// are forbidden, including comparator effects. No recipe destination overlaps
// them in the actual profile. Private native output/home-stack aliases are not
// modeled. Existing raw class/category/array aliases are retained sequentially.
// Selection/binding requires exclusive byte-arena access except its synchronous
// comparator effects. Shared enumeration arrays require the coherent native
// guard/epoch publication protocol; no byte reader may race a constructor.
struct Monstey_GadbBankEnumeration {
    std::uint32_t count;
    std::uint64_t array;
};
Monstey_GadbBankEnumeration Monstey_GadbEnumerateNativeBank(
    Monstey_GadbBankActivation &, std::uint64_t enumerator);
// Concrete parser helper4BA96C. Direct17ADFC nonnull comparator boundary is
// retained even for zero input pointers; no17ADE8 null shortcut is substituted.
// First matched bank wins, including a class miss/empty bank. Final class slot
// is reread after comparison. Returned raw identity is not invoked/resolved.
std::uint64_t Monstey_GadbFindParserBankClass(
    Monstey_GadbBankActivation &, std::uint64_t bank_name,
    std::uint64_t category_second_string);
// Complete196A34 outer bank/category/class selection feeding the accepted
// initializer/field binder. Its original register return is not part of the
// caller-used contract. No parser/allocator/patch/locale/history is fabricated.
void Monstey_GadbBindDatabaseBank(
    Monstey_GadbBankActivation &, std::uint64_t bank_name,
    std::uint64_t database, Monstey_GadbBinderTrace &,
    std::uint64_t owned_step_budget = 1000000);
// Complete leaf784C98: sequential runtime DB-header initialization, preserving
// its unwritten padding at60..63 and132..135. Does not reset Class64/field state.
std::uint64_t Monstey_GadbInitializeDatabaseHeader(
    Monstey_GadbBinderMemory, std::uint64_t database);
} // namespace Monstey
