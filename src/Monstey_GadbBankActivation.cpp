#include "Monstey_GadbBankActivation.hpp"
#include "Monstey_GadbBanks.hpp"
#include <limits>
namespace Monstey {
namespace {
using Monstey_ActivationAddress = std::uint64_t;
struct Monstey_BankProfile {
    Monstey_ActivationAddress enumerator, guard, array;
    std::uint32_t count;
};
constexpr std::array<Monstey_BankProfile, 6> Monstey_BankProfiles{{
    {0x14162dc24, 0x142aa2b68, 0x142a59ea0, 2687},
    {0x1416248b8, 0x142aa2b3c, 0x142a58c70, 24},
    {0x1404baa24, 0x142a58df0, 0x142a58e00, 50},
    {0x14161d674, 0x142aa2b44, 0x142a590e0, 375},
    {0x14161d594, 0x142aa2b4c, 0x142a59e10, 15},
    {0x14482a370, 0, 0x142a59e08, 0}
}};
class Monstey_ActivationMemory {
    Monstey_GadbBinderMemory memory_;
    std::size_t Offset(Monstey_ActivationAddress address, std::size_t width) const {
        if (address < memory_.base || address - memory_.base > memory_.bytes.size() ||
            width > memory_.bytes.size() - static_cast<std::size_t>(address - memory_.base))
            throw Monstey_GadbBinderFault(
                Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable,
                address, "Consumed bank activation range unavailable");
        return static_cast<std::size_t>(address - memory_.base);
    }
public:
    explicit Monstey_ActivationMemory(Monstey_GadbBinderMemory memory) : memory_(memory) {}
    Monstey_ActivationAddress Read(Monstey_ActivationAddress address, std::size_t width) const {
        const auto offset = Offset(address, width); Monstey_ActivationAddress result = 0;
        for (std::size_t i = 0; i < width; ++i)
            result |= static_cast<Monstey_ActivationAddress>(memory_.bytes[offset + i]) << (8 * i);
        return result;
    }
    void Write(Monstey_ActivationAddress address, Monstey_ActivationAddress value, std::size_t width) const {
        const auto offset = Offset(address, width);
        for (std::size_t i = 0; i < width; ++i)
            memory_.bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
};
Monstey_ActivationAddress Monstey_ActivationHex(const Monstey_Json &value) {
    const auto &text = value.as_string(); std::size_t used = 0;
    const auto result = std::stoull(text, &used, 16);
    if (used != text.size()) throw std::invalid_argument("Invalid qualified recipe address");
    return result;
}
const Monstey_GadbBankRecipe &Monstey_ActivationRecipe(
    const Monstey_GadbBankActivation &input, Monstey_ActivationAddress enumerator) {
    const Monstey_BankProfile *profile = nullptr;
    for (const auto &candidate : Monstey_BankProfiles)
        if (candidate.enumerator == enumerator) { profile = &candidate; break; }
    if (!profile)
        throw Monstey_GadbBankFault(Monstey_GadbBankFaultKind::UnknownEnumerator,
            enumerator, "Only six exact native enumerator profiles qualify");
    for (const auto &recipe : input.recipes) {
        if (recipe.enumerator != enumerator) continue;
        if (recipe.guard_identity != profile->guard ||
            recipe.returned_array != profile->array ||
            recipe.returned_count != profile->count)
            throw Monstey_GadbBankFault(Monstey_GadbBankFaultKind::InvalidRecipe,
                enumerator, "Supplied guard/array/count differs from native profile");
        return recipe;
    }
    throw Monstey_GadbBankFault(Monstey_GadbBankFaultKind::UnknownEnumerator,
        enumerator, "Native enumerator is outside the supplied qualified recipes");
}
std::int32_t Monstey_ActivationCompare(
    Monstey_GadbBankActivation &input, Monstey_ActivationAddress left, Monstey_ActivationAddress right) {
    if (!input.external_comparator)
        throw std::invalid_argument("Native direct name comparator must be supplied");
    return input.external_comparator(left, right);
}
} // namespace
std::array<Monstey_GadbBankRecipe, 6> Monstey_GadbActivationRecipes(
    std::span<const std::uint8_t> image) {
    const auto report = Monstey_GadbBankRegistration(image);
    std::array<Monstey_GadbBankRecipe, 6> result;
    const auto &banks = report.at("banks").as_array();
    if (banks.size() != result.size()) throw std::invalid_argument("Expected six qualified banks");
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto &source = banks[i]; auto &recipe = result[i];
        recipe.enumerator = Monstey_ActivationHex(source.at("enumerator"));
        recipe.guard_identity = Monstey_BankProfiles[i].guard;
        recipe.returned_array = Monstey_ActivationHex(source.at("returned_array"));
        recipe.returned_count = static_cast<std::uint32_t>(source.at("returned_count").as_uint());
        for (const auto &row : source.at("classes").as_array())
            recipe.constructor_classes.push_back(Monstey_ActivationHex(row.at("class_pointer")));
        if (recipe.enumerator != Monstey_BankProfiles[i].enumerator ||
            recipe.returned_array != Monstey_BankProfiles[i].array ||
            recipe.returned_count != Monstey_BankProfiles[i].count ||
            recipe.constructor_classes.size() != recipe.returned_count)
            throw std::invalid_argument("Qualified constructor/count recipe differs");
    }
    return result;
}
Monstey_GadbBankEnumeration Monstey_GadbEnumerateNativeBank(
    Monstey_GadbBankActivation &input, std::uint64_t enumerator) {
    const auto &recipe = Monstey_ActivationRecipe(input, enumerator);
    if (recipe.guard_identity != 0) {
        Monstey_GadbBankEpochWord *guard = nullptr;
        for (auto *word : input.guards) {
            if (!word || word->identity != recipe.guard_identity) continue;
            if (guard && guard != word)
                throw Monstey_GadbBankFault(Monstey_GadbBankFaultKind::InvalidRecipe,
                    recipe.guard_identity, "Conflicting live objects for one guard identity");
            guard = word;
        }
        if (!guard)
            throw Monstey_GadbBankFault(Monstey_GadbBankFaultKind::MissingGuardWord,
                recipe.guard_identity, "A live typed guard word must be supplied");
        const auto thread_epoch = input.thread_epoch;
        if (guard->Load() > thread_epoch) {
            input.synchronization.Header(*guard, input.global_epoch, input.thread_epoch);
            if (guard->Load() == -1) {
                if (recipe.constructor_classes.size() != recipe.returned_count)
                    throw Monstey_GadbBankFault(Monstey_GadbBankFaultKind::InvalidRecipe,
                        enumerator, "Incomplete immutable native constructor constants");
                const Monstey_ActivationMemory memory(input.memory);
                for (std::size_t i = 0; i < recipe.constructor_classes.size(); ++i) {
                    const auto destination = recipe.returned_array + static_cast<Monstey_ActivationAddress>(i) * 8;
                    // Typed overlay addresses are not ordinary byte destinations.
                    const auto overlaps = [destination](const Monstey_GadbBankEpochWord &word) {
                        return word.identity - destination < 8 || destination - word.identity < 4;
                    };
                    if (overlaps(input.global_epoch))
                        throw Monstey_GadbBankFault(Monstey_GadbBankFaultKind::InvalidRecipe,
                            destination, "Constructor overlaps a typed epoch overlay");
                    for (auto *word : input.guards)
                        if (word && overlaps(*word))
                            throw Monstey_GadbBankFault(Monstey_GadbBankFaultKind::InvalidRecipe,
                                destination, "Constructor overlaps a typed guard overlay");
                    memory.Write(destination, recipe.constructor_classes[i], 8);
                }
                input.synchronization.Footer(*guard, input.global_epoch, input.thread_epoch);
            }
        }
    }
    return {recipe.returned_count, recipe.returned_array};
}
std::uint64_t Monstey_GadbFindParserBankClass(
    Monstey_GadbBankActivation &input, std::uint64_t bank_name,
    std::uint64_t category_name) {
    const Monstey_ActivationMemory memory(input.memory);
    for (std::uint32_t bank = 0; bank < 6; ++bank) {
        const auto slot = input.bank_table + static_cast<Monstey_ActivationAddress>(bank) * 24;
        if (Monstey_ActivationCompare(input, memory.Read(slot, 8), bank_name) != 0)
            continue;
        const auto enumeration = Monstey_GadbEnumerateNativeBank(input, memory.Read(slot + 8, 8));
        for (std::uint32_t index = 0; index < enumeration.count; ++index) {
            const auto class_slot = enumeration.array + static_cast<Monstey_ActivationAddress>(index) * 8;
            const auto class_object = memory.Read(class_slot, 8);
            const auto class_name = memory.Read(class_object + 32, 8);
            if (Monstey_ActivationCompare(input, class_name, category_name) == 0)
                return memory.Read(class_slot, 8);
        }
        return 0;
    }
    return 0;
}
void Monstey_GadbBindDatabaseBank(
    Monstey_GadbBankActivation &input, std::uint64_t bank_name,
    std::uint64_t database, Monstey_GadbBinderTrace &trace,
    std::uint64_t owned_step_budget) {
    const Monstey_ActivationMemory memory(input.memory);
    for (std::uint32_t bank = 0; bank < 6; ++bank) {
        const auto slot = input.bank_table + static_cast<Monstey_ActivationAddress>(bank) * 24;
        const auto name = memory.Read(slot, 8);
        if (!Monstey_GadbNamePointersEqual(name, bank_name, input.external_comparator))
            continue;
        const auto enumeration = Monstey_GadbEnumerateNativeBank(input, memory.Read(slot + 8, 8));
        //196AA5 gates the count, then196AAF samples it again for the loop.
        if (memory.Read(database + 72, 4) == 0) return;
        const auto category_count = memory.Read(database + 72, 4);
        for (Monstey_ActivationAddress index = 0; index < category_count; ++index) {
            const auto category = memory.Read(database + 80, 8) + index * 88;
            if (!Monstey_GadbCategoryHasBindingRecords(input.memory, category, trace, owned_step_budget))
                continue;
            for (std::uint32_t class_index = 0; class_index < enumeration.count; ++class_index) {
                const auto class_slot = enumeration.array + static_cast<Monstey_ActivationAddress>(class_index) * 8;
                //196AE9 consumes the category second-string before class slot/name.
                const auto category_name = memory.Read(category + 8, 8);
                const auto class_object = memory.Read(class_slot, 8);
                const auto class_name = memory.Read(class_object + 32, 8);
                if (!Monstey_GadbNamePointersEqual(class_name, category_name, input.external_comparator))
                    continue;
                const auto selected = memory.Read(class_slot, 8);
                if (selected != 0)
                    Monstey_GadbBindSelectedCategory(input.memory, selected, category, trace, owned_step_budget);
                break;
            }
        }
        return;
    }
}
std::uint64_t Monstey_GadbInitializeDatabaseHeader(
    Monstey_GadbBinderMemory input, std::uint64_t database) {
    const Monstey_ActivationMemory memory(input);
    for (std::uint64_t offset = 0; offset <= 48; offset += 8)
        memory.Write(database + offset, 0, 8);
    memory.Write(database + 56, 0, 4);
    for (std::uint64_t offset = 64; offset <= 120; offset += 8)
        memory.Write(database + offset, 0, 8);
    memory.Write(database + 128, 0, 4);
    return database;
}
} // namespace Monstey
