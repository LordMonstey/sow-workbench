#include "Monstey_GadbPoolRelocation.hpp"
#include "Monstey_GadbValueFixup.hpp"
#include <bit>
#include <limits>
namespace Monstey {
Monstey_GadbPoolRelocationFault::Monstey_GadbPoolRelocationFault(
    std::uint64_t at, std::size_t n, bool write)
    : std::runtime_error("Consumed SoW pool-relocation range unavailable"),
      address(at), width(n), writing(write) {}
Monstey_GadbPoolReadBoundaryFault::Monstey_GadbPoolReadBoundaryFault(
    std::uint64_t site, std::uint64_t value)
    : std::runtime_error("Unprovided owned SoW stream Read primitive"),
      native_site(site), target(value) {}
namespace {
using Monstey_PoolAddress = std::uint64_t;
std::size_t Monstey_PoolOffset(const Monstey_GadbBinderMemory &m,
                               Monstey_PoolAddress at, std::size_t width,
                               bool writing) {
  if (at < m.base || width == 0 ||
      at > std::numeric_limits<Monstey_PoolAddress>::max() - (width - 1) ||
      at - m.base > m.bytes.size() ||
      width > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw Monstey_GadbPoolRelocationFault(at, width, writing);
  return static_cast<std::size_t>(at - m.base);
}
Monstey_PoolAddress Monstey_PoolRead(const Monstey_GadbBinderMemory &m,
                                     Monstey_PoolAddress at,
                                     std::size_t width) {
  auto offset = Monstey_PoolOffset(m, at, width, false);
  Monstey_PoolAddress value = 0;
  for (std::size_t k = 0; k < width; ++k)
    value |= Monstey_PoolAddress(m.bytes[offset + k]) << (k * 8u);
  return value;
}
void Monstey_PoolWrite(Monstey_GadbBinderMemory &m, Monstey_PoolAddress at,
                       Monstey_PoolAddress value, std::size_t width) {
  auto offset = Monstey_PoolOffset(m, at, width, true);
  for (std::size_t k = 0; k < width; ++k)
    m.bytes[offset + k] = static_cast<std::uint8_t>(value >> (k * 8u));
}
Monstey_PoolAddress Monstey_PoolReadPrimitive(
    Monstey_GadbBinderMemory &m, Monstey_PoolAddress stream,
    Monstey_PoolAddress destination, std::uint32_t bytes,
    Monstey_PoolAddress site, const Monstey_GadbPoolReadService &read) {
  const auto table = Monstey_PoolRead(m, stream, 8);
  const auto target = Monstey_PoolRead(m, table + 8u, 8);
  if (!read)
    throw Monstey_GadbPoolReadBoundaryFault(site, target);
  return read(m, {stream, target, destination, bytes, site});
}
} // namespace
std::uint8_t Monstey_GadbIsPoolRelativeType(std::uint32_t raw) {
  const auto type = std::bit_cast<std::int32_t>(raw);
  return static_cast<std::uint8_t>((type >= 4 && type <= 9) ||
                                   (type >= 12 && type <= 16));
}
std::uint64_t Monstey_GadbPoolRelativeTypeRaw(std::uint32_t raw,
                                              std::uint64_t incoming) {
  return (incoming & ~std::uint64_t(255)) | Monstey_GadbIsPoolRelativeType(raw);
}
std::uint64_t Monstey_GadbPoolReadU8(Monstey_GadbBinderMemory &m,
                                     std::uint64_t stream,
                                     std::uint64_t destination,
                                     const Monstey_GadbPoolReadService &read) {
  (void)Monstey_PoolReadPrimitive(m, stream, destination, 1, 0x14115FD1Eu,
                                  read);
  return stream;
}
std::uint64_t Monstey_GadbPoolReadU16(Monstey_GadbBinderMemory &m,
                                      std::uint64_t stream,
                                      std::uint64_t destination,
                                      const Monstey_GadbPoolReadService &read) {
  (void)Monstey_PoolReadPrimitive(m, stream, destination, 2, 0x14115FD3Eu,
                                  read);
  return stream;
}
std::uint64_t Monstey_GadbPoolReadU32(Monstey_GadbBinderMemory &m,
                                      std::uint64_t stream,
                                      std::uint64_t destination,
                                      const Monstey_GadbPoolReadService &read) {
  (void)Monstey_PoolReadPrimitive(m, stream, destination, 4, 0x14115FD5Eu,
                                  read);
  return stream;
}
std::uint64_t Monstey_GadbLoadPoolRelativeRecord(
    Monstey_GadbBinderMemory &m, std::uint64_t category,
    std::uint64_t original_rdx, std::uint64_t record, std::uint64_t stream,
    std::uint64_t frame, const Monstey_GadbPoolReadService &read) {
  Monstey_PoolWrite(m, frame + 0x38u, original_rdx, 8);
  (void)Monstey_GadbPoolReadU32(m, stream, frame - 0x0Cu, read);
  (void)Monstey_GadbPoolReadU16(m, stream, frame - 0x10u, read);
  (void)Monstey_GadbPoolReadU8(m, stream, frame + 0x38u, read);
  (void)Monstey_GadbPoolReadU32(m, stream, frame - 8u, read);
  (void)Monstey_GadbPoolReadU8(m, stream, frame + 0x60u, read);
  const auto cursor_address = Monstey_PoolRead(m, frame + 0x68u, 8);
  const auto initial_count =
      static_cast<std::uint16_t>(Monstey_PoolRead(m, frame - 0x10u, 2));
  const auto name_offset =
      static_cast<std::uint32_t>(Monstey_PoolRead(m, frame - 0x0Cu, 4));
  const auto pool = Monstey_PoolRead(m, frame + 0x50u, 8);
  const auto words = Monstey_PoolRead(m, cursor_address, 8);
  const auto name = std::uint64_t(name_offset) + pool;
  Monstey_PoolWrite(m, cursor_address,
                    words + std::uint64_t(initial_count) * 8u, 8);
  std::uint32_t index = 0;
  if (initial_count != 0) {
    do {
      const auto slot = words + std::uint64_t(index) * 8u;
      Monstey_PoolWrite(m, slot, 0, 8);
      const auto result =
          Monstey_PoolReadPrimitive(m, stream, slot, 4, 0x1404BA297u, read);
      if (static_cast<std::uint32_t>(result) != 0)
        return result & ~std::uint64_t(255);
      ++index;
    } while (index < Monstey_PoolRead(m, frame - 0x10u, 2));
  }
  const auto first_descriptor =
      static_cast<std::uint32_t>(Monstey_PoolRead(m, frame - 8u, 4));
  auto descriptor_count =
      static_cast<std::uint8_t>(Monstey_PoolRead(m, frame + 0x38u, 1));
  const auto descriptors = (std::uint64_t(first_descriptor) << 4u) +
                           Monstey_PoolRead(m, frame + 0x58u, 8);
  index = 0;
  if (descriptor_count != 0) {
    do {
      const auto descriptor = descriptors + (std::uint64_t(index) << 4u);
      const auto type = static_cast<std::uint32_t>(
                            Monstey_PoolRead(m, descriptor + 0x0Au, 1)) &
                        0x3Fu;
      if (Monstey_GadbIsPoolRelativeType(type)) {
        const auto first_word = Monstey_PoolRead(m, descriptor + 0x0Eu, 2);
        const auto first = words + first_word * 8u;
        std::uint32_t value = 0;
        if (Monstey_PoolRead(m, descriptor + 0x0Cu, 2) != 0) {
          do {
            const auto slot = first + std::uint64_t(value++) * 8u;
            const auto offset = Monstey_PoolRead(m, slot, 8);
            Monstey_PoolWrite(m, slot, offset + pool, 8);
          } while (value < Monstey_PoolRead(m, descriptor + 0x0Cu, 2));
          descriptor_count =
              static_cast<std::uint8_t>(Monstey_PoolRead(m, frame + 0x38u, 1));
        }
      }
      ++index;
    } while (index < descriptor_count);
  }
  Monstey_PoolWrite(m, record + 8u, descriptors, 8);
  Monstey_PoolWrite(m, record + 0x20u, name, 8);
  Monstey_PoolWrite(m, record, descriptor_count, 1);
  Monstey_PoolWrite(m, record + 0x18u, category, 8);
  Monstey_PoolWrite(m, record + 0x10u, words, 8);
  const auto hash = Monstey_GadbHashCString({m.base, m.bytes}, name, 0);
  const auto flags =
      static_cast<std::uint8_t>(Monstey_PoolRead(m, frame + 0x60u, 1)) | 2u;
  Monstey_PoolWrite(m, record + 4u, hash, 4);
  Monstey_PoolWrite(m, record + 1u, flags, 1);
  return (std::uint64_t(hash) & ~std::uint64_t(255)) | 1u;
}
std::uint64_t Monstey_GadbPoolOwnedMemoryRead::Monstey_Read(
    Monstey_GadbBinderMemory &m, const Monstey_GadbPoolReadCall &call) {
  std::uint32_t copied = 0;
  while (copied < call.byte_count && position < length) {
    const auto value = Monstey_PoolRead(m, source + position, 1);
    Monstey_PoolWrite(m, call.destination + copied, value, 1);
    ++position;
    ++copied;
  }
  return copied == call.byte_count ? 0 : short_read_result;
}
} // namespace Monstey