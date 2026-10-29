#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>
namespace Monstey {
// Supplied native address identities only. These integers are never converted
// to host pointers, dereferenced, resolved to strings or called by this module.
using Monstey_GadbNonnullPointerComparator =
    std::function<std::int32_t(std::uint64_t, std::uint64_t)>;
std::int32_t Monstey_GadbCompareNamePointers(
    std::uint64_t left, std::uint64_t right,
    const Monstey_GadbNonnullPointerComparator& external);
bool Monstey_GadbNamePointersEqual(
    std::uint64_t left, std::uint64_t right,
    const Monstey_GadbNonnullPointerComparator& external);
struct Monstey_GadbPointerClassName {
    std::uint64_t identity = 0;
    std::uint64_t full_name_pointer = 0; // Native class+20, not label+10.
};
struct Monstey_GadbPointerBankNames {
    std::uint64_t name_pointer = 0;
    std::vector<Monstey_GadbPointerClassName> classes;
};
struct Monstey_GadbPointerCategory {
    std::uint64_t name_pointer = 0; // Native category+8; sampled per class visit.
    std::uint32_t record_count = 0; // Native category+28.
    std::uint32_t anonymous_count = 0; // Native category+40.
};
enum class Monstey_GadbPointerSelectionStatus {
    BankNotFound, CategorySkipped, ClassNotFound, ClassFound
};
struct Monstey_GadbPointerSelection {
    Monstey_GadbPointerSelectionStatus status =
        Monstey_GadbPointerSelectionStatus::BankNotFound;
    std::optional<std::uint32_t> bank_index;
    std::optional<std::uint32_t> class_index;
    std::optional<std::uint64_t> class_identity;
    std::uint32_t bank_comparisons = 0;
    std::uint32_t class_comparisons = 0;
};
// Pre-initializer selection only: six supplied ordered banks, one category.
// The enumerated class count is sampled once before that class search. Later
// callback mutations may affect consumed rows/identity; growth does not extend
// the sampled count. Shrinkage rejects at the first consumed missing row.
// A null class identity rejects before its name is consumed; a late matched
// identity reread may be zero, which the later native binder skips.
// Category gates are read after bank comparison; its right-name field is
// reloaded on each class visit. Rows are caller-supplied name-field views, not
// a model resolving every alias of arbitrary native class objects.
// No lazy enumerator, class initializer, binder, locale or load history is run.
Monstey_GadbPointerSelection Monstey_GadbSelectPointerClass(
    std::span<const Monstey_GadbPointerBankNames> banks,
    std::uint64_t bank_name_pointer,
    const Monstey_GadbPointerCategory& category,
    const Monstey_GadbNonnullPointerComparator& external);
// Snapshot convenience: fixes category fields at entry. Use the borrowed
// category overload when comparator callbacks may mutate that descriptor.
Monstey_GadbPointerSelection Monstey_GadbSelectPointerClass(
    std::span<const Monstey_GadbPointerBankNames> banks,
    std::uint64_t bank_name_pointer, std::uint64_t category_name_pointer,
    std::uint32_t record_count, std::uint32_t anonymous_count,
    const Monstey_GadbNonnullPointerComparator& external);
} // namespace Monstey