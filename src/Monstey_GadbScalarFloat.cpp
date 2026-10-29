#include "Monstey_GadbScalarFloat.hpp"
namespace Monstey {
namespace {
using Monstey_U64 = std::uint64_t;
using Monstey_U32 = std::uint32_t;
class Monstey_ScalarMemory {
    Monstey_GadbBinderMemory memory_;
    std::size_t Offset(Monstey_U64 address, std::size_t width) const {
        if (address < memory_.base ||
            address - memory_.base > memory_.bytes.size() ||
            width > memory_.bytes.size() -
                        static_cast<std::size_t>(address - memory_.base))
            throw Monstey_GadbBinderFault(
                Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable,
                address, "Consumed scalar getter range unavailable");
        return static_cast<std::size_t>(address - memory_.base);
    }
public:
    explicit Monstey_ScalarMemory(Monstey_GadbBinderMemory memory)
        : memory_(memory) {}
    Monstey_U64 Read(Monstey_U64 address, std::size_t width) const {
        const auto offset = Offset(address, width);
        Monstey_U64 value = 0;
        for (std::size_t i = 0; i < width; ++i)
            value |= static_cast<Monstey_U64>(memory_.bytes[offset + i]) << (8 * i);
        return value;
    }
    void WriteWord(Monstey_U64 address, Monstey_U64 value) const {
        const auto offset = Offset(address, 8);
        for (std::size_t i = 0; i < 8; ++i)
            memory_.bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
};
}
Monstey_GadbScalarFault::Monstey_GadbScalarFault(
    Monstey_GadbScalarFaultKind fault, std::uint64_t where)
    : std::runtime_error("Native generated-array index assertion: deliberate null write"),
      kind(fault), address(where) {}

std::uint32_t Monstey_GadbGeneratedFieldSize(
    std::uint32_t type, std::uint32_t count, std::uint32_t flags) {
    if ((flags & 0x280u) != 0)
        return 0;
    if (count != 1 || type == 9)
        return 16;
    if (type == 8)
        return 12;
    if (type == 1)
        return 5;
    if (type >= 2 && type <= 17)
        return 8;
    return 0;
}
std::uint64_t Monstey_GadbFindGeneratedField(
    Monstey_GadbBinderMemory memory, std::uint64_t class_object,
    std::uint32_t hash) {
    const Monstey_ScalarMemory source(memory);
    const auto count = static_cast<Monstey_U32>(source.Read(class_object + 8, 4));
    if (count == 0)
        return 0;
    const auto table = source.Read(class_object, 8);
    for (Monstey_U32 i = 0; i < count; ++i) {
        const auto field = table + static_cast<Monstey_U64>(i) * 72;
        if (source.Read(field + 32, 4) == hash)
            return field;
    }
    return 0;
}
std::uint32_t Monstey_GadbGeneratedFieldOffset(
    Monstey_GadbBinderMemory memory, std::uint64_t class_object,
    std::uint32_t hash) {
    const Monstey_ScalarMemory source(memory);
    const auto count = static_cast<Monstey_U32>(source.Read(class_object + 8, 4));
    if (count == 0)
        return UINT32_MAX;
    const auto table = source.Read(class_object, 8);
    Monstey_U32 offset = 0;
    for (Monstey_U32 i = 0; i < count; ++i) {
        const auto field = table + static_cast<Monstey_U64>(i) * 72;
        if (source.Read(field + 32, 4) == hash)
            return offset;
        // Native register-read order is flags, count, then type.
        const auto flags = static_cast<Monstey_U32>(source.Read(field + 44, 4));
        const auto declared_count =
            static_cast<Monstey_U32>(source.Read(field + 40, 4));
        const auto type = static_cast<Monstey_U32>(source.Read(field + 36, 4));
        const auto size = Monstey_GadbGeneratedFieldSize(type, declared_count, flags);
        offset = (offset + 7u + size) & 0xfffffff8u;
    }
    return UINT32_MAX;
}
Monstey_GadbGeneratedWordResult Monstey_GadbEvaluateGeneratedWord(
    Monstey_GadbBinderMemory memory, std::uint64_t class_object,
    std::uint64_t field, std::uint64_t record, std::uint32_t index) {
    const Monstey_ScalarMemory source(memory);
    const auto hash = static_cast<Monstey_U32>(source.Read(field, 4));
    const auto declaration = Monstey_GadbFindGeneratedField(memory, class_object, hash);
    if (declaration == 0)
        return {};
    const auto offset = Monstey_GadbGeneratedFieldOffset(memory, class_object, hash);
    const auto type = static_cast<Monstey_U32>(source.Read(declaration + 36, 4));
    const auto storage = static_cast<Monstey_U64>(offset) + source.Read(record + 16, 8);
    if (static_cast<Monstey_U32>(type - 8u) <= 1u)
        index += index;
    const auto count =
        static_cast<Monstey_U32>(source.Read(declaration + 40, 4));
    if (count != UINT32_MAX)
        return {true, source.Read(storage + static_cast<Monstey_U64>(index) * 8, 8)};
    if (index >= source.Read(storage + 8, 4))
        throw Monstey_GadbScalarFault(
            Monstey_GadbScalarFaultKind::NativeDynamicIndexAssertion, storage + 8);
    const auto values = source.Read(storage, 8);
    return {true, source.Read(values + static_cast<Monstey_U64>(index) * 8, 8)};
}
std::uint8_t Monstey_GadbFetchGeneratedWord(
    Monstey_GadbBinderMemory memory, std::uint64_t class_object,
    std::uint64_t field, std::uint64_t record, std::uint32_t index,
    std::uint64_t outputAddress) {
    const auto value = Monstey_GadbEvaluateGeneratedWord(
        memory, class_object, field, record, index);
    if (!value.found)
        return 0;
    Monstey_ScalarMemory(memory).WriteWord(outputAddress, value.value);
    return 1;
}
std::uint32_t Monstey_GadbReadGeneratedFloatBits(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field, std::uint32_t index, std::uint64_t defaultAddress) {
    const Monstey_ScalarMemory source(memory);
    if (record != 0) {
        const auto owner = source.Read(record + 24, 8);
        if (owner != 0 && source.Read(owner + 32, 8) != 0) {
            // 5DA0FC rereads owner+32 when preparing the call.
            const auto declaration_class = source.Read(owner + 32, 8);
            const auto result = Monstey_GadbEvaluateGeneratedWord(
                memory, declaration_class, field, record, index);
            if (result.found)
                return static_cast<Monstey_U32>(result.value);
        }
    }
    return static_cast<Monstey_U32>(source.Read(defaultAddress, 4));
}
std::uint32_t Monstey_GadbReadScalarFloatBits(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field, std::uint32_t index, std::uint64_t defaultAddress) {
    const Monstey_ScalarMemory source(memory);
    if (record == 0)
        return static_cast<Monstey_U32>(source.Read(defaultAddress, 4));
    const auto descriptor = Monstey_GadbFindRecordDescriptor(memory, record, field);
    if (descriptor == 0 ||
        (source.Read(descriptor + 10, 1) & 0x3fu) != 2 ||
        index >= source.Read(descriptor + 12, 2))
        return static_cast<Monstey_U32>(source.Read(defaultAddress, 4));
    if ((source.Read(record + 1, 1) & 0x10u) != 0)
        return Monstey_GadbReadGeneratedFloatBits(
            memory, record, field, index, defaultAddress);
    const auto word = static_cast<Monstey_U32>(source.Read(descriptor + 14, 2));
    const auto values = source.Read(record + 16, 8);
    const auto element = static_cast<Monstey_U32>(word + index);
    return static_cast<Monstey_U32>(
        source.Read(values + static_cast<Monstey_U64>(element) * 8, 4));
}
}