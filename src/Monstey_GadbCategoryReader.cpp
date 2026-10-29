#include "Monstey_GadbCategoryReader.hpp"
#include <limits>
namespace Monstey {
Monstey_GadbCategoryDispatchFault::Monstey_GadbCategoryDispatchFault(
    std::uint64_t site, std::uint64_t s, std::uint64_t t, std::uint64_t e)
    : std::runtime_error("Unsupported consumed SoW category virtual target"),
      native_site(site), slot(s), target(t), expected(e) {}
namespace {
class Monstey_CategoryMemory {
  Monstey_GadbBinderMemory memory_;
  std::size_t Offset(std::uint64_t a, std::size_t n, bool w) const {
    if (a < memory_.base ||
        a > std::numeric_limits<std::uint64_t>::max() - (n - 1) ||
        a - memory_.base > memory_.bytes.size() ||
        n > memory_.bytes.size() - static_cast<std::size_t>(a - memory_.base))
      throw Monstey_GadbPoolRelocationFault(a, n, w);
    return static_cast<std::size_t>(a - memory_.base);
  }

public:
  explicit Monstey_CategoryMemory(Monstey_GadbBinderMemory m) : memory_(m) {}
  std::uint64_t Read(std::uint64_t a, std::size_t n) const {
    const auto p = Offset(a, n, false);
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < n; ++i)
      v |= static_cast<std::uint64_t>(memory_.bytes[p + i]) << (i * 8);
    return v;
  }
  void Write(std::uint64_t a, std::uint64_t v, std::size_t n) const {
    const auto p = Offset(a, n, true);
    for (std::size_t i = 0; i < n; ++i)
      memory_.bytes[p + i] = static_cast<std::uint8_t>(v >> (i * 8));
  }
};
Monstey_GadbBufferedResult
Monstey_CategoryClearAL(Monstey_GadbBufferedResult r) {
  r.rax &= ~std::uint64_t{255};
  return r;
}
} // namespace
Monstey_GadbBufferedResult Monstey_GadbLoadCategoryFromBuffered(
    Monstey_GadbBankActivation &activation, std::uint64_t parent,
    std::uint64_t bank_name, std::uint64_t category, std::uint64_t lookup,
    std::uint64_t f, const Monstey_GadbBankSelectorServices &selector_services,
    const Monstey_GadbBufferedServices &buffer_services,
    Monstey_GadbBufferedCells cells, std::uint64_t compare_cell) {
  const auto image = activation.memory;
  const Monstey_CategoryMemory m(image);
  m.Write(f + 0x30, parent, 8);
  const auto stream = m.Read(f + 0x50, 8);
  const auto stream_table = m.Read(stream, 8);
  const auto stream_target = m.Read(stream_table + 8, 8);
  if (stream_target != 0x141B50DE8ULL)
    throw Monstey_GadbCategoryDispatchFault(0x1404BA0BCULL, stream_table + 8,
                                            stream_target, 0x141B50DE8ULL);
  auto result = Monstey_GadbBufferedRead(image, stream, f - 0x18, 24, f - 0xA0,
                                         buffer_services, cells);
  if (static_cast<std::uint32_t>(result.rax) != 0)
    return Monstey_CategoryClearAL(result);
  const auto record_cursor = m.Read(f + 0x78, 8);
  const auto initial_anonymous =
      static_cast<std::uint32_t>(m.Read(f - 0x0C, 4));
  const auto pool = m.Read(f + 0x68, 8);
  const auto third_offset = static_cast<std::uint32_t>(m.Read(f - 0x10, 4));
  const auto anonymous_base = m.Read(record_cursor, 8);
  const auto third_name = pool + third_offset;
  const auto second_offset = static_cast<std::uint32_t>(m.Read(f - 0x14, 4));
  const auto first_offset = static_cast<std::uint32_t>(m.Read(f - 0x18, 4));
  const auto second_name = pool + second_offset;
  const auto first_name = pool + first_offset;
  const auto anonymous_extent =
      static_cast<std::uint32_t>(initial_anonymous << 5);
  m.Write(f - 0x20, third_name, 8);
  m.Write(record_cursor, anonymous_base + anonymous_extent, 8);
  const auto selector = m.Read(f + 0x58, 8);
  m.Write(f - 0x30, anonymous_base, 8);
  m.Write(f - 0x28, second_name, 8);
  const auto selector_table = m.Read(selector, 8);
  const auto selector_target = m.Read(selector_table, 8);
  if (selector_target != 0x1404BA96CULL)
    throw Monstey_GadbCategoryDispatchFault(0x1404BA10BULL, selector_table,
                                            selector_target, 0x1404BA96CULL);
  result = Monstey_GadbFindParserBankClassRegisters(
      activation, bank_name, second_name, f - 0xC0, selector_services,
      compare_cell);
  auto anonymous_count = static_cast<std::uint32_t>(m.Read(f - 0x0C, 4));
  std::uint32_t index = 0;
  if (anonymous_count != 0)
    do {
      const auto value_cursor = m.Read(f + 0x80, 8);
      m.Write(f - 0x38, value_cursor, 8);
      const auto descriptors = m.Read(f + 0x70, 8);
      m.Write(f - 0x48, descriptors, 8);
      const auto record =
          m.Read(f - 0x30, 8) + (static_cast<std::uint64_t>(index) << 5);
      m.Write(f - 0x50, pool, 8);
      result = Monstey_GadbLoadAnonymousRecordFromBuffered(
          image, category, result.rdx, record, stream, f - 0xC0,
          buffer_services, cells);
      if (static_cast<std::uint8_t>(result.rax) == 0)
        return Monstey_CategoryClearAL(result);
      anonymous_count = static_cast<std::uint32_t>(m.Read(f - 0x0C, 4));
      ++index;
    } while (index < anonymous_count);
  std::uint32_t lookup_extra = 0;
  if (lookup != 0) {
    result = Monstey_GadbHashCStringRegisters(image, first_name, 0, result.rdx);
    m.Write(f + 0x50, result.rax, 4);
    result = Monstey_GadbFindCategoryByHash(image, lookup, f + 0x50);
    if (result.rax != 0)
      lookup_extra = static_cast<std::uint32_t>(m.Read(result.rax + 0x28, 4));
  }
  auto named_count = static_cast<std::uint32_t>(m.Read(f - 8, 4));
  const auto named_cursor = m.Read(f + 0x78, 8);
  auto named_extent = static_cast<std::uint32_t>(named_count + lookup_extra);
  index = 0;
  const auto named_base = m.Read(named_cursor, 8);
  named_extent = static_cast<std::uint32_t>(named_extent * 5u);
  named_extent = static_cast<std::uint32_t>(named_extent << 3);
  m.Write(named_cursor, named_base + named_extent, 8);
  if (named_count != 0) {
    do {
      const auto value_cursor = m.Read(f + 0x80, 8);
      m.Write(f - 0x38, value_cursor, 8);
      const auto record = named_base + static_cast<std::uint64_t>(index) * 40;
      const auto descriptors = m.Read(f + 0x70, 8);
      m.Write(f - 0x48, descriptors, 8);
      m.Write(f - 0x50, pool, 8);
      result = Monstey_GadbLoadNamedRecordFromBuffered(
          image, category, named_count, record, stream, f - 0xA0,
          buffer_services, cells);
      if (static_cast<std::uint8_t>(result.rax) == 0)
        return Monstey_CategoryClearAL(result);
      named_count = static_cast<std::uint32_t>(m.Read(f - 8, 4));
      ++index;
    } while (index < named_count);
    anonymous_count = static_cast<std::uint32_t>(m.Read(f - 0x0C, 4));
  }
  const auto second = m.Read(f - 0x28, 8);
  const auto saved_parent = m.Read(f + 0x30, 8);
  m.Write(category + 8, second, 8);
  const auto third = m.Read(f - 0x20, 8);
  m.Write(category + 0x10, third, 8);
  const auto saved_anonymous = m.Read(f - 0x30, 8);
  m.Write(category + 0x28, named_count, 4);
  const auto late_scalar = static_cast<std::uint32_t>(m.Read(f + 0x60, 4));
  m.Write(category + 0x18, saved_parent, 8);
  m.Write(category + 0x48, saved_anonymous, 8);
  m.Write(category, first_name, 8);
  m.Write(category + 0x38, named_base, 8);
  m.Write(category + 0x40, anonymous_count, 4);
  m.Write(category + 0x2C, late_scalar, 4);
  result = Monstey_GadbHashCStringRegisters(image, first_name, 0, late_scalar);
  (void)m.Read(category + 0x20, 8);
  m.Write(category + 0x20, 0, 8);
  m.Write(category + 0x50, result.rax, 4);
  result.rax = (result.rax & ~std::uint64_t{255}) | 1;
  m.Write(category + 0x30, 0, 1);
  return result;
}
} // namespace Monstey
