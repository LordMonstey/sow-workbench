#include "Monstey_GadbAudioNextKey.hpp"
#include <bit>
namespace Monstey {
namespace {
using Monstey_U64 = std::uint64_t;
using Monstey_U32 = std::uint32_t;
class Monstey_NextKeyMemory {
    Monstey_GadbBinderMemory memory_;
public:
    explicit Monstey_NextKeyMemory(Monstey_GadbBinderMemory memory) : memory_(memory) {}
    Monstey_U64 Read(Monstey_U64 address, std::size_t width) const {
        if (address < memory_.base || address - memory_.base > memory_.bytes.size() ||
            width > memory_.bytes.size() - static_cast<std::size_t>(address - memory_.base))
            throw Monstey_GadbBinderFault(
                Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable, address,
                "Consumed AudioHookNextKey range unavailable");
        const auto offset = static_cast<std::size_t>(address - memory_.base);
        Monstey_U64 value = 0;
        for (std::size_t i = 0; i < width; ++i)
            value |= static_cast<Monstey_U64>(memory_.bytes[offset + i]) << (i * 8);
        return value;
    }
};
Monstey_U64 Monstey_NextKeyGenerated(const Monstey_NextKeyMemory &source,
                                    Monstey_U64 record, Monstey_U64 wanted_address) {
    const auto category = source.Read(record + 24, 8);
    if (category == 0) return 0;
    const auto declaration_class = source.Read(category + 32, 8);
    if (declaration_class == 0) return 0;
    const auto count = static_cast<Monstey_U32>(source.Read(declaration_class + 8, 4));
    const auto wanted = static_cast<Monstey_U32>(source.Read(wanted_address, 4));
    if (count == 0) return 0;
    const auto table = source.Read(declaration_class, 8);
    Monstey_U64 declaration = 0;
    bool found = false;
    for (Monstey_U32 i = 0; i < count; ++i) {
        const auto row = table + static_cast<Monstey_U64>(i) * 72;
        if (source.Read(row + 32, 4) == wanted) {
            declaration = row;
            found = true;
            break;
        }
    }
    if (!found) return 0;
    // D17E54/D18104 retain the class count/table from the first scan.
    Monstey_U32 offset = 0;
    for (Monstey_U32 i = 0; ; ++i) {
        if (i >= count) { offset = UINT32_MAX; break; }
        const auto row = table + static_cast<Monstey_U64>(i) * 72;
        if (source.Read(row + 32, 4) == wanted) break;
        // Native flags, count, type reads are unconditional, even on markers.
        const auto flags = static_cast<Monstey_U32>(source.Read(row + 44, 4));
        const auto declared_count = static_cast<Monstey_U32>(source.Read(row + 40, 4));
        const auto type = static_cast<Monstey_U32>(source.Read(row + 36, 4));
        const auto size = Monstey_GadbGeneratedFieldSize(type, declared_count, flags);
        offset = (offset + 7u + size) & 0xfffffff8u;
    }
    auto storage = source.Read(record + 16, 8) + static_cast<Monstey_U64>(offset);
    if (source.Read(declaration + 40, 4) == UINT32_MAX) {
        if (source.Read(storage + 8, 4) == 0)
            throw Monstey_GadbScalarFault(
                Monstey_GadbScalarFaultKind::NativeDynamicIndexAssertion, storage + 8);
        storage = source.Read(storage, 8);
    }
    return source.Read(storage, 8);
}
Monstey_U64 Monstey_NextKeySlow(const Monstey_NextKeyMemory &source,
                              Monstey_U64 record, Monstey_U64 wanted_address) {
    if (record == 0) return 0;
    const auto flags = source.Read(record + 1, 1);
    const auto descriptors = source.Read(record + 8, 8);
    const auto count = static_cast<std::int32_t>(source.Read(record, 1));
    if (count == 0) return 0;
    const auto wanted = static_cast<Monstey_U32>(source.Read(wanted_address, 4));
    std::int32_t low = 0, high = count - 1;
    Monstey_U64 descriptor = 0;
    while (low <= high) {
        const auto middle = (low + high) / 2;
        const auto row = descriptors + static_cast<Monstey_U64>(middle) * 16;
        const auto candidate = static_cast<Monstey_U32>(
            source.Read(row + ((flags & 1u) != 0 ? 0u : 4u), 4));
        if (wanted == candidate) {
            descriptor = (flags & 1u) != 0
                ? row : descriptors + source.Read(row + 9, 1) * 16;
            break;
        }
        if (wanted < candidate) high = middle - 1;
        else low = middle + 1;
    }
    if (descriptor == 0) return 0;
    if ((source.Read(descriptor + 10, 1) & 0x3fu) != 10) return 0;
    if (source.Read(descriptor + 12, 2) == 0) return 0;
    if ((flags & 0x10u) != 0)
        return Monstey_NextKeyGenerated(source, record, wanted_address);
    const auto word = source.Read(descriptor + 14, 2);
    const auto values = source.Read(record + 16, 8);
    return source.Read(values + word * 8, 8);
}
Monstey_U64 Monstey_NextKeyStage(const Monstey_NextKeyMemory &source,
                               Monstey_U64 record, Monstey_U64 field) {
    const auto state = std::bit_cast<std::int32_t>(
        static_cast<Monstey_U32>(source.Read(field + 48, 4)));
    if (state < 2) return Monstey_NextKeySlow(source, record, field + 32);
    if (record == 0) return 0;
    const auto physical = source.Read(field + 64, 1);
    const auto descriptors = source.Read(record + 8, 8);
    const auto descriptor = descriptors + physical * 16;
    if (source.Read(descriptor + 12, 2) == 0) return 0;
    const auto word = source.Read(descriptor + 14, 2);
    const auto values = source.Read(record + 16, 8);
    return source.Read(values + word * 8, 8);
}
} // namespace
std::uint64_t Monstey_GadbReadAudioNextKey(
    Monstey_GadbBinderMemory memory, std::uint64_t action_wrapper,
    std::uint64_t action_class, std::uint64_t nextkey_class) {
    const Monstey_NextKeyMemory source(memory);
    const auto action_fields = source.Read(action_class, 8);
    const auto action_record = source.Read(action_wrapper, 8);
    const auto child = Monstey_NextKeyStage(source, action_record, action_fields + 20 * 72);
    const auto key_fields = source.Read(nextkey_class, 8);
    return Monstey_NextKeyStage(source, child, key_fields);
}
} // namespace Monstey
