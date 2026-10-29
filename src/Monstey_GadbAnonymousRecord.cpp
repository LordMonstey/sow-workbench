#include "Monstey_GadbAnonymousRecord.hpp"
#include <limits>
namespace Monstey {
namespace {
using Monstey_AnonymousAddress = std::uint64_t;
std::size_t Monstey_AnonymousOffset(const Monstey_GadbBinderMemory &m,
                                    Monstey_AnonymousAddress at,
                                    std::size_t width, bool writing) {
  if (at < m.base ||
      at > std::numeric_limits<Monstey_AnonymousAddress>::max() - (width - 1) ||
      at - m.base > m.bytes.size() ||
      width > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw Monstey_GadbPoolRelocationFault(at, width, writing);
  return static_cast<std::size_t>(at - m.base);
}
Monstey_AnonymousAddress
Monstey_AnonymousRead(const Monstey_GadbBinderMemory &m,
                      Monstey_AnonymousAddress at, std::size_t width) {
  const auto offset = Monstey_AnonymousOffset(m, at, width, false);
  Monstey_AnonymousAddress value = 0;
  for (std::size_t n = 0; n < width; ++n)
    value |= Monstey_AnonymousAddress(m.bytes[offset + n]) << (n * 8u);
  return value;
}
void Monstey_AnonymousWrite(Monstey_GadbBinderMemory &m,
                            Monstey_AnonymousAddress at,
                            Monstey_AnonymousAddress value, std::size_t width) {
  const auto offset = Monstey_AnonymousOffset(m, at, width, true);
  for (std::size_t n = 0; n < width; ++n)
    m.bytes[offset + n] = static_cast<std::uint8_t>(value >> (n * 8u));
}
Monstey_AnonymousAddress
Monstey_AnonymousPayloadRead(Monstey_GadbBinderMemory &m,
                             Monstey_AnonymousAddress stream,
                             Monstey_AnonymousAddress destination,
                             const Monstey_GadbPoolReadService &read) {
  const auto table = Monstey_AnonymousRead(m, stream, 8);
  const auto target = Monstey_AnonymousRead(m, table + 8u, 8);
  if (!read)
    throw Monstey_GadbPoolReadBoundaryFault(0x14176662BULL, target);
  return read(m, {stream, target, destination, 4, 0x14176662BULL});
}
} // namespace
std::uint64_t Monstey_GadbLoadAnonymousRecord(
    Monstey_GadbBinderMemory &m, std::uint64_t category,
    std::uint64_t original_rdx, std::uint64_t record, std::uint64_t stream,
    std::uint64_t frame, const Monstey_GadbPoolReadService &read) {
  Monstey_AnonymousWrite(m, frame + 0x58u, original_rdx, 8);
  (void)Monstey_GadbPoolReadU16(m, stream, frame + 0x20u, read);
  (void)Monstey_GadbPoolReadU8(m, stream, frame + 0x58u, read);
  (void)Monstey_GadbPoolReadU32(m, stream, frame + 0x24u, read);
  (void)Monstey_GadbPoolReadU8(m, stream, frame + 0x80u, read);
  const auto initial_count =
      static_cast<std::uint16_t>(Monstey_AnonymousRead(m, frame + 0x20u, 2));
  const auto cursor_address = Monstey_AnonymousRead(m, frame + 0x88u, 8);
  std::uint64_t result =
      initial_count; // 141766600 MOV EAX,R8D, not an invented boolean.
  const auto words = Monstey_AnonymousRead(m, cursor_address, 8);
  Monstey_AnonymousWrite(m, cursor_address,
                         words + std::uint64_t(initial_count) * 8u, 8);
  std::uint32_t index = 0;
  if (initial_count != 0) {
    do {
      const auto slot = words + std::uint64_t(index) * 8u;
      Monstey_AnonymousWrite(m, slot, 0, 8);
      result = Monstey_AnonymousPayloadRead(m, stream, slot, read);
      if (static_cast<std::uint32_t>(result) != 0)
        return result & ~std::uint64_t(255);
      result = Monstey_AnonymousRead(m, frame + 0x20u, 2);
      ++index;
    } while (index < result);
  }
  const auto first_descriptor =
      static_cast<std::uint32_t>(Monstey_AnonymousRead(m, frame + 0x24u, 4));
  auto descriptor_count =
      static_cast<std::uint8_t>(Monstey_AnonymousRead(m, frame + 0x58u, 1));
  const auto descriptors = (std::uint64_t(first_descriptor) << 4u) +
                           Monstey_AnonymousRead(m, frame + 0x78u, 8);
  index = 0;
  if (descriptor_count != 0) {
    const auto pool = Monstey_AnonymousRead(m, frame + 0x70u, 8);
    do {
      const auto descriptor = descriptors + (std::uint64_t(index) << 4u);
      const auto type = static_cast<std::uint32_t>(
                            Monstey_AnonymousRead(m, descriptor + 0x0Au, 1)) &
                        0x3Fu;
      result = Monstey_GadbPoolRelativeTypeRaw(type, result);
      if (static_cast<std::uint8_t>(result) != 0) {
        result = Monstey_AnonymousRead(m, descriptor + 0x0Eu, 2);
        const auto first = words + result * 8u;
        std::uint32_t value_index = 0;
        if (Monstey_AnonymousRead(m, descriptor + 0x0Cu, 2) != 0) {
          do {
            const auto slot = first + std::uint64_t(value_index++) * 8u;
            const auto value = Monstey_AnonymousRead(m, slot, 8);
            Monstey_AnonymousWrite(m, slot, value + pool, 8);
            result = Monstey_AnonymousRead(m, descriptor + 0x0Cu, 2);
          } while (value_index < result);
          descriptor_count = static_cast<std::uint8_t>(
              Monstey_AnonymousRead(m, frame + 0x58u, 1));
        }
      }
      ++index;
    } while (index < descriptor_count);
  }
  const auto flags = Monstey_AnonymousRead(m, frame + 0x80u, 1);
  result = (result & ~std::uint64_t(255)) | 1u;
  Monstey_AnonymousWrite(m, record + 1u, flags, 1);
  Monstey_AnonymousWrite(m, record, descriptor_count, 1);
  Monstey_AnonymousWrite(m, record + 8u, descriptors, 8);
  Monstey_AnonymousWrite(m, record + 0x18u, category, 8);
  Monstey_AnonymousWrite(m, record + 0x10u, words, 8);
  return result;
}
} // namespace Monstey
