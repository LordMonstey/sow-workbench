#include "Monstey_GadbBankSelectorRegisters.hpp"
namespace Monstey {
Monstey_GadbBankSelectorMemoryFault::Monstey_GadbBankSelectorMemoryFault(
    std::uint64_t a, std::size_t n, bool w)
    : std::runtime_error("Consumed SoW bank-selector range unavailable"),
      address(a), width(n), writing(w) {}
Monstey_GadbBankSelectorOverlayFault::Monstey_GadbBankSelectorOverlayFault(
    std::uint64_t a, std::size_t n, bool w, std::uint64_t id)
    : std::runtime_error("Raw selector access overlaps a typed guard/epoch"),
      address(a), width(n), writing(w), overlay_identity(id) {}
Monstey_GadbBankSelectorImportFault::Monstey_GadbBankSelectorImportFault(
    std::uint64_t s, std::uint64_t t, std::uint64_t site)
    : std::runtime_error("Nonnull raw comparator must be supplied"), slot(s),
      target(t), native_site(site) {}
namespace {
class Monstey_SelectorMemory {
  Monstey_GadbBinderMemory memory_;
  const Monstey_GadbBankActivation &activation_;
  std::size_t Offset(std::uint64_t a, std::size_t n, bool w) const {
    const auto overlaps = [a, n](std::uint64_t id) {
      return a >= id ? a - id < 4 : id - a < n;
    };
    if (overlaps(activation_.global_epoch.identity))
      throw Monstey_GadbBankSelectorOverlayFault(
          a, n, w, activation_.global_epoch.identity);
    for (const auto *word : activation_.guards)
      if (word && overlaps(word->identity))
        throw Monstey_GadbBankSelectorOverlayFault(a, n, w, word->identity);
    if (a < memory_.base || a - memory_.base > memory_.bytes.size() ||
        n > memory_.bytes.size() - static_cast<std::size_t>(a - memory_.base))
      throw Monstey_GadbBankSelectorMemoryFault(a, n, w);
    return static_cast<std::size_t>(a - memory_.base);
  }

public:
  explicit Monstey_SelectorMemory(const Monstey_GadbBankActivation &a)
      : memory_(a.memory), activation_(a) {}
  std::uint64_t Read(std::uint64_t a, std::size_t n) const {
    const auto off = Offset(a, n, false);
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < n; ++i)
      v |= static_cast<std::uint64_t>(memory_.bytes[off + i]) << (i * 8);
    return v;
  }
  void Write(std::uint64_t a, std::uint64_t v, std::size_t n) const {
    const auto off = Offset(a, n, true);
    for (std::size_t i = 0; i < n; ++i)
      memory_.bytes[off + i] = static_cast<std::uint8_t>(v >> (i * 8));
  }
};
Monstey_GadbBufferedResult Monstey_SelectorCompare(
    const Monstey_SelectorMemory &memory, Monstey_GadbBinderMemory image,
    std::uint64_t left, std::uint64_t right, std::uint64_t site,
    const Monstey_GadbBankSelectorServices &services, std::uint64_t cell) {
  if (left == 0)
    return {right == 0 ? 0ULL : 0xFFFFFFFFULL, 0ULL - right};
  if (right == 0)
    return {1, 0};
  const auto target = memory.Read(cell, 8);
  if (!services.compare_nonnull)
    throw Monstey_GadbBankSelectorImportFault(cell, target, site);
  return services.compare_nonnull(
      image, {cell, target, left, right, site, 0x14017AE06ULL});
}
} // namespace
Monstey_GadbBufferedResult Monstey_GadbFindParserBankClassRegisters(
    Monstey_GadbBankActivation &activation, std::uint64_t bank_name,
    std::uint64_t category_name, std::uint64_t q,
    const Monstey_GadbBankSelectorServices &services,
    std::uint64_t compare_cell) {
  const Monstey_SelectorMemory memory(activation);
  const auto image = activation.memory;
  auto cursor = activation.bank_table + 8;
  Monstey_GadbBufferedResult result{0, bank_name};
  for (std::uint32_t bank = 0; bank < 6; ++bank, cursor += 24) {
    const auto name = memory.Read(cursor - 8, 8);
    result = Monstey_SelectorCompare(memory, image, name, bank_name,
                                     0x1404BA9A2ULL, services, compare_cell);
    if (static_cast<std::uint32_t>(result.rax) != 0)
      continue;
    (void)memory.Read(q + 0x68, 4); // AND DWORD0 is read-modify-write.
    memory.Write(q + 0x68, 0, 4);
    (void)memory.Read(q + 0x20, 8); // Second clear before enum target load.
    memory.Write(q + 0x20, 0, 8);
    const auto target = memory.Read(cursor, 8);
    const auto enumeration =
        Monstey_GadbEnumerateNativeBank(activation, target);
    memory.Write(q + 0x68, enumeration.count, 4);
    memory.Write(q + 0x20, enumeration.array, 8);
    // Guarded result.rdx is dead before Compare supplies its real replacement.
    // The only empty enum preserves its known inputRDX=outputArray.
    if (target == 0x14482A370ULL)
      result.rdx = q + 0x20;
    std::uint32_t index = 0;
    if (memory.Read(q + 0x68, 4) != 0) {
      do {
        const auto array = memory.Read(q + 0x20, 8);
        const auto object =
            memory.Read(array + static_cast<std::uint64_t>(index) * 8, 8);
        const auto class_name = memory.Read(object + 0x20, 8);
        result =
            Monstey_SelectorCompare(memory, image, class_name, category_name,
                                    0x1404BA9ECULL, services, compare_cell);
        if (static_cast<std::uint32_t>(result.rax) == 0) {
          const auto live_array = memory.Read(q + 0x20, 8);
          result.rax = memory.Read(
              live_array + static_cast<std::uint64_t>(index) * 8, 8);
          return result;
        }
        ++index;
      } while (index < memory.Read(q + 0x68, 4));
    }
    // Native increments its cursor then immediately tests matchedBPL: no later
    // bank/table read occurs, including when this first matched bank was empty.
    result.rax = 0;
    return result;
  }
  result.rax = 0;
  return result;
}
} // namespace Monstey
