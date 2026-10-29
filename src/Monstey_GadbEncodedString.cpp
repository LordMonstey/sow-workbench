#include "Monstey_GadbEncodedString.hpp"
#include <limits>
namespace Monstey {
Monstey_GadbEncodedMemoryFault::Monstey_GadbEncodedMemoryFault(std::uint64_t at,
                                                               std::size_t n,
                                                               bool w)
    : std::runtime_error("Consumed SoW encoded-string range unavailable"),
      address(at), width(n), writing(w) {}
Monstey_GadbEncodedTargetFault::Monstey_GadbEncodedTargetFault(
    std::uint64_t site, std::uint64_t object, std::uint64_t value,
    std::uint64_t wanted)
    : std::runtime_error("Unsupported concrete SoW encoded-string Read target"),
      native_site(site), stream(object), target(value), expected(wanted) {}
namespace {
std::size_t Monstey_EncodedOffset(Monstey_GadbBinderMemory m, std::uint64_t at,
                                  std::size_t n, bool w) {
  if (at < m.base || at > std::numeric_limits<std::uint64_t>::max() - (n - 1) ||
      at - m.base > m.bytes.size() ||
      n > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw Monstey_GadbEncodedMemoryFault(at, n, w);
  return static_cast<std::size_t>(at - m.base);
}
std::uint64_t Monstey_EncodedRead(Monstey_GadbBinderMemory m, std::uint64_t at,
                                  std::size_t n) {
  const auto p = Monstey_EncodedOffset(m, at, n, false);
  std::uint64_t v = 0;
  for (std::size_t k = 0; k < n; ++k)
    v |= std::uint64_t(m.bytes[p + k]) << (k * 8u);
  return v;
}
void Monstey_EncodedWriteByte(Monstey_GadbBinderMemory m, std::uint64_t at) {
  const auto p = Monstey_EncodedOffset(m, at, 1, true);
  m.bytes[p] = 0;
}
Monstey_GadbBufferedResult Monstey_EncodedTypedResult(
    Monstey_GadbBinderMemory m, std::uint64_t stream, std::uint64_t destination,
    std::uint32_t bytes, std::uint64_t frame, std::uint64_t site,
    const Monstey_GadbBufferedServices &s, Monstey_GadbBufferedCells cells) {
  const auto table = Monstey_EncodedRead(m, stream, 8);
  const auto target = Monstey_EncodedRead(m, table + 8u, 8);
  if (target != 0x141B50DE8ULL)
    throw Monstey_GadbEncodedTargetFault(site, stream, target, 0x141B50DE8ULL);
  auto result = Monstey_GadbBufferedRead(m, stream, destination, bytes,
                                         frame - 0x30u, s, cells);
  result.rax = stream;
  return result;
}
} // namespace
Monstey_GadbBufferedResult
Monstey_GadbEncodedReadU16(Monstey_GadbBinderMemory m, std::uint64_t stream,
                           std::uint64_t destination, std::uint64_t frame,
                           const Monstey_GadbBufferedServices &s,
                           Monstey_GadbBufferedCells cells) {
  return Monstey_EncodedTypedResult(m, stream, destination, 2, frame,
                                    0x14115FD3EULL, s, cells);
}
Monstey_GadbBufferedResult
Monstey_GadbEncodedReadByte(Monstey_GadbBinderMemory m, std::uint64_t stream,
                            std::uint64_t destination, std::uint64_t frame,
                            const Monstey_GadbBufferedServices &s,
                            Monstey_GadbBufferedCells cells) {
  return Monstey_EncodedTypedResult(m, stream, destination, 1, frame,
                                    0x14115FCFEULL, s, cells);
}
Monstey_GadbBufferedResult Monstey_GadbReadEncodedString(
    Monstey_GadbBinderMemory m, std::uint64_t stream, std::uint64_t destination,
    std::uint32_t capacity, std::uint64_t frame,
    const Monstey_GadbBufferedServices &s, Monstey_GadbBufferedCells cells) {
  (void)Monstey_GadbEncodedReadU16(m, stream, frame + 0x40u, frame - 0x30u, s,
                                   cells);
  auto length =
      static_cast<std::uint16_t>(Monstey_EncodedRead(m, frame + 0x40u, 2));
  Monstey_GadbBufferedResult result{std::uint32_t(std::uint32_t(length) + 1u),
                                    length};
  if (result.rax > capacity)
    return result;
  std::uint32_t index = 0;
  if (length != 0) {
    do {
      result = Monstey_GadbEncodedReadByte(m, stream,
                                           destination + std::uint64_t(index),
                                           frame - 0x30u, s, cells);
      length =
          static_cast<std::uint16_t>(Monstey_EncodedRead(m, frame + 0x40u, 2));
      ++index;
    } while (index < length);
  }
  result.rax = length;
  Monstey_EncodedWriteByte(m, destination + std::uint64_t(length));
  return result;
}
} // namespace Monstey
