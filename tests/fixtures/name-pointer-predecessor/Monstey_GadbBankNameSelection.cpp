#include "Monstey_GadbBankNameSelection.hpp"
#include <limits>
#include <stdexcept>
namespace Monstey {
std::int32_t
Monstey_GadbNativeNameCompare(const Monstey_GadbOwnedName &left,
                              const Monstey_GadbOwnedName &right,
                              const Monstey_GadbNonnullComparator &external) {
  if (!left)
    return right ? -1 : 0;
  if (!right)
    return 1;
  if (!external)
    throw std::invalid_argument(
        "Native nonnull comparator is unqualified; supply it explicitly");
  return external(*left, *right);
}
bool Monstey_GadbNativeNameEqual(
    const Monstey_GadbOwnedName &left, const Monstey_GadbOwnedName &right,
    const Monstey_GadbNonnullComparator &external) {
  return Monstey_GadbNativeNameCompare(left, right, external) == 0;
}
Monstey_GadbNameSelection
Monstey_GadbNativeSelectClass(std::span<const Monstey_GadbOwnedBankNames> banks,
                              const Monstey_GadbOwnedName &bank_name,
                              const Monstey_GadbOwnedName &category_name,
                              std::uint32_t record_count,
                              std::uint32_t anonymous_count,
                              const Monstey_GadbNonnullComparator &external) {
  if (banks.size() != 6)
    throw std::invalid_argument("Native selection requires six ordered banks");
  Monstey_GadbNameSelection result;
  for (std::uint32_t bank = 0; bank < 6; ++bank) {
    ++result.bank_comparisons;
    if (!Monstey_GadbNativeNameEqual(banks[bank].name, bank_name, external))
      continue;
    result.bank_index = bank;
    if (record_count == 0 && anonymous_count == 0) {
      result.status = Monstey_GadbNameSelectionStatus::CategorySkipped;
      return result;
    }
    result.status = Monstey_GadbNameSelectionStatus::ClassNotFound;
    const auto &classes = banks[bank].classes;
    if (classes.size() > std::numeric_limits<std::uint32_t>::max())
      throw std::invalid_argument(
          "Owned class count exceeds native uint32 range");
    for (std::uint32_t index = 0; index < classes.size(); ++index) {
      ++result.class_comparisons;
      if (!Monstey_GadbNativeNameEqual(classes[index].full_name, category_name,
                                       external))
        continue;
      result.status = Monstey_GadbNameSelectionStatus::ClassFound;
      result.class_index = index;
      result.class_identity = classes[index].identity;
      return result;
    }
    return result;
  }
  return result;
}
} // namespace Monstey
