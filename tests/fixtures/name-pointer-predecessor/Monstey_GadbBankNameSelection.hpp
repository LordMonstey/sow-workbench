#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace Monstey {
// Owned narrow name bytes. Null is distinct from an empty nonnull string.
using Monstey_GadbOwnedName = std::optional<std::string>;
// Required external boundary from 17ADFC. No implicit byte/case/Unicode policy.
// Views borrow the caller-owned strings for the duration of the call. Their
// bytes are passed unchanged, including embedded zeroes; a concrete external
// C-string comparator decides how it interprets those bytes.
using Monstey_GadbNonnullComparator =
    std::function<std::int32_t(std::string_view, std::string_view)>;
std::int32_t
Monstey_GadbNativeNameCompare(const Monstey_GadbOwnedName &left,
                              const Monstey_GadbOwnedName &right,
                              const Monstey_GadbNonnullComparator &external);
bool Monstey_GadbNativeNameEqual(const Monstey_GadbOwnedName &left,
                                 const Monstey_GadbOwnedName &right,
                                 const Monstey_GadbNonnullComparator &external);
struct Monstey_GadbOwnedClassName {
  Monstey_GadbOwnedName full_name; // Native class QWORD+20, not short name+10.
  std::uint64_t identity = 0; // Opaque value; never dereferenced or invoked.
};
struct Monstey_GadbOwnedBankNames {
  Monstey_GadbOwnedName name;
  std::vector<Monstey_GadbOwnedClassName> classes; // Enumerator order retained.
};
enum class Monstey_GadbNameSelectionStatus {
  BankNotFound,
  CategorySkipped,
  ClassNotFound,
  ClassFound
};
struct Monstey_GadbNameSelection {
  Monstey_GadbNameSelectionStatus status =
      Monstey_GadbNameSelectionStatus::BankNotFound;
  std::optional<std::uint32_t> bank_index;
  std::optional<std::uint32_t> class_index;
  std::optional<std::uint64_t> class_identity;
  std::uint32_t bank_comparisons = 0;
  std::uint32_t class_comparisons = 0;
};
// Qualified selection portion of 196A34 only. Exactly six native table rows.
// record_count and anonymous_count are sampled category DWORDs+28/+40. A zero
// pair skips class comparison, after bank selection. No initializer, callback
// field binding, BSS mutation, current state or live locale is reconstructed.
// The required external comparator is invoked in native argument/order; its
// return is tested only for equality to zero. First matched bank is final even
// when its class lookup misses. Result values own their scalar state.
Monstey_GadbNameSelection
Monstey_GadbNativeSelectClass(std::span<const Monstey_GadbOwnedBankNames> banks,
                              const Monstey_GadbOwnedName &bank_name,
                              const Monstey_GadbOwnedName &category_name,
                              std::uint32_t record_count,
                              std::uint32_t anonymous_count,
                              const Monstey_GadbNonnullComparator &external);
} // namespace Monstey
