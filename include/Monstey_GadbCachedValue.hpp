#pragma once
#include "Monstey_Core.hpp"
#include <cstdint>
#include <span>
namespace Monstey {
// Owned storage for the native cached QWORD getter. This does not bind a field,
// interpret a link, or turn a serialized reference into a process pointer.
struct Monstey_GadbCachedRecordView {
    std::span<const std::uint8_t> descriptors;
    std::span<const std::uint64_t> values;
};
// 30E7A4/30E7B4 -> 30E7D0 -> 1447E6F50, with the 1447E7023 tail.
// A null record and an out-of-count element return the raw supplied fallback.
// Descriptor-index bounds are an owned-storage check, not a native count check.
// A zero count consumes only bytes +12/+13, never offset +14 or the values.
std::uint64_t Monstey_GadbReadCachedQword(
    const Monstey_GadbCachedRecordView* record, std::uint8_t physical_index,
    std::uint32_t element_index, std::uint64_t fallback);
// Additive evidence for one field and its two statically identified callers.
// Original checker counters/classifications are preserved, not requalified.
Monstey_Json Monstey_GadbConditionalBuffAbsence(const Monstey_Json& audit);
}
