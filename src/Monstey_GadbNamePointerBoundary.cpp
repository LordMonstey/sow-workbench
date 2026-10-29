#include "Monstey_GadbNamePointerBoundary.hpp"
#include <limits>
#include <stdexcept>
namespace Monstey {
std::int32_t Monstey_GadbCompareNamePointers(
    std::uint64_t left, std::uint64_t right,
    const Monstey_GadbNonnullPointerComparator& external) {
    if (left == 0) return right == 0 ? 0 : -1;
    if (right == 0) return 1;
    if (!external)
        throw std::invalid_argument("Native nonnull comparator must be supplied");
    return external(left, right);
}
bool Monstey_GadbNamePointersEqual(
    std::uint64_t left, std::uint64_t right,
    const Monstey_GadbNonnullPointerComparator& external) {
    return Monstey_GadbCompareNamePointers(left, right, external) == 0;
}
Monstey_GadbPointerSelection Monstey_GadbSelectPointerClass(
    std::span<const Monstey_GadbPointerBankNames> banks,
    std::uint64_t bank_name_pointer,
    const Monstey_GadbPointerCategory& category,
    const Monstey_GadbNonnullPointerComparator& external) {
    if (banks.size() != 6)
        throw std::invalid_argument("Native bank table has six ordered rows");
    Monstey_GadbPointerSelection result;
    for (std::uint32_t bank = 0; bank < 6; ++bank) {
        ++result.bank_comparisons;
        if (!Monstey_GadbNamePointersEqual(
                banks[bank].name_pointer, bank_name_pointer, external)) continue;
        result.bank_index = bank;
        if (category.record_count == 0 && category.anonymous_count == 0) {
            result.status = Monstey_GadbPointerSelectionStatus::CategorySkipped;
            return result;
        }
        result.status = Monstey_GadbPointerSelectionStatus::ClassNotFound;
        const auto& classes = banks[bank].classes;
        if (classes.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("Class count exceeds native U32");
        const auto class_count = static_cast<std::uint32_t>(classes.size());
        for (std::uint32_t index = 0; index < class_count; ++index) {
            if (index >= classes.size())
                throw std::out_of_range("Consumed class row was removed");
            if (classes[index].identity == 0)
                throw std::out_of_range("Consumed null native class identity");
            ++result.class_comparisons;
            const auto full_name = classes[index].full_name_pointer;
            if (!Monstey_GadbNamePointersEqual(
                    full_name, category.name_pointer, external)) continue;
            if (index >= classes.size())
                throw std::out_of_range("Matched class row was removed");
            result.status = Monstey_GadbPointerSelectionStatus::ClassFound;
            result.class_index = index;
            result.class_identity = classes[index].identity;
            return result;
        }
        return result;
    }
    return result;
}
Monstey_GadbPointerSelection Monstey_GadbSelectPointerClass(
    std::span<const Monstey_GadbPointerBankNames> banks,
    std::uint64_t bank_name_pointer, std::uint64_t category_name_pointer,
    std::uint32_t record_count, std::uint32_t anonymous_count,
    const Monstey_GadbNonnullPointerComparator& external) {
    const Monstey_GadbPointerCategory snapshot{
        category_name_pointer, record_count, anonymous_count};
    return Monstey_GadbSelectPointerClass(
        banks, bank_name_pointer, snapshot, external);
}
} // namespace Monstey