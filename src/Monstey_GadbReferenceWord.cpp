#include "Monstey_GadbReferenceWord.hpp"
#include "Monstey_GadbScalarFloat.hpp"
#include <cstddef>
namespace Monstey {
namespace {
std::uint64_t Monstey_ReferenceRead(
    Monstey_GadbBinderMemory memory, std::uint64_t address, std::size_t width) {
    if (address < memory.base ||
        address - memory.base > memory.bytes.size() ||
        width > memory.bytes.size() -
            static_cast<std::size_t>(address - memory.base))
        throw Monstey_GadbBinderFault(
            Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable, address,
            "Reference getter consumed an unavailable owned range");
    const auto offset = static_cast<std::size_t>(address - memory.base);
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint64_t>(memory.bytes[offset + i]) << (8 * i);
    return value;
}
} // namespace
std::uint64_t Monstey_GadbReadGeneratedReferenceWord(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field, std::uint32_t index, std::uint64_t defaultAddress) {
    if (record != 0) {
        const auto owner = Monstey_ReferenceRead(memory, record + 24, 8);
        if (owner != 0 && Monstey_ReferenceRead(memory, owner + 32, 8) != 0) {
            // The native helper reloads this class pointer for the actual call.
            const auto declarationClass =
                Monstey_ReferenceRead(memory, owner + 32, 8);
            const auto result = Monstey_GadbEvaluateGeneratedWord(
                memory, declarationClass, field, record, index);
            if (result.found)
                return result.value;
        }
    }
    return Monstey_ReferenceRead(memory, defaultAddress, 8);
}
std::uint64_t Monstey_GadbReadReferenceWord(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field, std::uint32_t index, std::uint64_t defaultAddress) {
    if (record != 0) {
        const auto descriptor =
            Monstey_GadbFindRecordDescriptor(memory, record, field);
        if (descriptor != 0 &&
            (Monstey_ReferenceRead(memory, descriptor + 10, 1) & 0x3fu) == 10 &&
            index < Monstey_ReferenceRead(memory, descriptor + 12, 2)) {
            if ((Monstey_ReferenceRead(memory, record + 1, 1) & 0x10u) != 0)
                return Monstey_GadbReadGeneratedReferenceWord(
                    memory, record, field, index, defaultAddress);
            const auto first = static_cast<std::uint32_t>(
                Monstey_ReferenceRead(memory, descriptor + 14, 2));
            const auto values = Monstey_ReferenceRead(memory, record + 16, 8);
            const auto word = static_cast<std::uint32_t>(first + index);
            return Monstey_ReferenceRead(memory, values + 8ull * word, 8);
        }
    }
    return Monstey_ReferenceRead(memory, defaultAddress, 8);
}
} // namespace Monstey
