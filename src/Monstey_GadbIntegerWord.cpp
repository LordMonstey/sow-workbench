#include "Monstey_GadbIntegerWord.hpp"
#include "Monstey_GadbScalarFloat.hpp"
#include <cstddef>
namespace Monstey {
namespace {
std::uint64_t Monstey_IntegerRead(Monstey_GadbBinderMemory memory,
                                std::uint64_t address, std::size_t width) {
    if (address < memory.base || address - memory.base > memory.bytes.size() ||
        width > memory.bytes.size() -
                    static_cast<std::size_t>(address - memory.base))
        throw Monstey_GadbBinderFault(
            Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable, address,
            "Integer getter consumed an unavailable owned range");
    const auto offset = static_cast<std::size_t>(address - memory.base);
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint64_t>(memory.bytes[offset + i]) << (8 * i);
    return value;
}
} // namespace
std::uint32_t Monstey_GadbReadGeneratedIntegerWord(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field, std::uint32_t index, std::uint64_t defaultAddress) {
    if (record != 0) {
        const auto owner = Monstey_IntegerRead(memory, record + 24, 8);
        if (owner != 0 && Monstey_IntegerRead(memory, owner + 32, 8) != 0) {
            const auto declarationClass = Monstey_IntegerRead(memory, owner + 32, 8);
            const auto result = Monstey_GadbEvaluateGeneratedWord(
                memory, declarationClass, field, record, index);
            if (result.found)
                return static_cast<std::uint32_t>(result.value);
        }
    }
    return static_cast<std::uint32_t>(Monstey_IntegerRead(memory, defaultAddress, 4));
}
std::uint32_t Monstey_GadbReadIntegerWord(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field, std::uint32_t index, std::uint64_t defaultAddress) {
    if (record != 0) {
        const auto descriptor = Monstey_GadbFindRecordDescriptor(memory, record, field);
        if (descriptor != 0 &&
            (Monstey_IntegerRead(memory, descriptor + 10, 1) & 0x3fu) == 5 &&
            index < Monstey_IntegerRead(memory, descriptor + 12, 2)) {
            if ((Monstey_IntegerRead(memory, record + 1, 1) & 0x10u) != 0)
                return Monstey_GadbReadGeneratedIntegerWord(
                    memory, record, field, index, defaultAddress);
            const auto first = static_cast<std::uint32_t>(
                Monstey_IntegerRead(memory, descriptor + 14, 2));
            const auto values = Monstey_IntegerRead(memory, record + 16, 8);
            const auto word = static_cast<std::uint32_t>(first + index);
            return static_cast<std::uint32_t>(
                Monstey_IntegerRead(memory, values + 8ull * word, 4));
        }
    }
    return static_cast<std::uint32_t>(Monstey_IntegerRead(memory, defaultAddress, 4));
}
std::uint32_t Monstey_GadbReadCachedIntegerWord(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint8_t physical_cache, std::uint32_t index,
    std::uint64_t defaultAddress) {
    if (record != 0) {
        const auto descriptors = Monstey_IntegerRead(memory, record + 8, 8);
        const auto descriptor = descriptors + 16ull * physical_cache;
        if (index < Monstey_IntegerRead(memory, descriptor + 12, 2)) {
            const auto first = static_cast<std::uint32_t>(
                Monstey_IntegerRead(memory, descriptor + 14, 2));
            const auto values = Monstey_IntegerRead(memory, record + 16, 8);
            const auto word = static_cast<std::uint32_t>(first + index);
            return static_cast<std::uint32_t>(
                Monstey_IntegerRead(memory, values + 8ull * word, 4));
        }
    }
    return static_cast<std::uint32_t>(Monstey_IntegerRead(memory, defaultAddress, 4));
}
} // namespace Monstey
