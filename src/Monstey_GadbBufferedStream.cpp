#include "Monstey_GadbBufferedStream.hpp"
#include <limits>
namespace Monstey {
Monstey_GadbBufferedMemoryFault::Monstey_GadbBufferedMemoryFault(
    std::uint64_t at, std::size_t n, bool w)
    : std::runtime_error("Consumed SoW buffered-stream range unavailable"),
      address(at), width(n), writing(w) {}
Monstey_GadbBufferedImportFault::Monstey_GadbBufferedImportFault(
    Monstey_GadbBufferedImport k, std::uint64_t site, std::uint64_t cell,
    std::uint64_t value)
    : std::runtime_error("Unprovided SoW buffered-stream external primitive"),
      kind(k), native_site(site), slot(cell), target(value) {}
Monstey_GadbBufferedDispatchFault::Monstey_GadbBufferedDispatchFault(
    std::uint64_t site, std::uint64_t cell, std::uint64_t value,
    std::uint64_t wanted)
    : std::runtime_error(
          "Unsupported SoW buffered-stream concrete virtual target"),
      native_site(site), slot(cell), target(value), expected(wanted) {}
namespace {
using Monstey_BufferedAddress = std::uint64_t;
std::size_t Monstey_BufferedOffset(Monstey_GadbBinderMemory m,
                                   Monstey_BufferedAddress at, std::size_t n,
                                   bool w) {
  if (at < m.base ||
      at > std::numeric_limits<Monstey_BufferedAddress>::max() - (n - 1) ||
      at - m.base > m.bytes.size() ||
      n > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw Monstey_GadbBufferedMemoryFault(at, n, w);
  return static_cast<std::size_t>(at - m.base);
}
Monstey_BufferedAddress Monstey_BufferedRead(Monstey_GadbBinderMemory m,
                                             Monstey_BufferedAddress at,
                                             std::size_t n) {
  const auto p = Monstey_BufferedOffset(m, at, n, false);
  Monstey_BufferedAddress v = 0;
  for (std::size_t k = 0; k < n; ++k)
    v |= Monstey_BufferedAddress(m.bytes[p + k]) << (k * 8u);
  return v;
}
void Monstey_BufferedWrite(Monstey_GadbBinderMemory m,
                           Monstey_BufferedAddress at,
                           Monstey_BufferedAddress v, std::size_t n) {
  const auto p = Monstey_BufferedOffset(m, at, n, true);
  for (std::size_t k = 0; k < n; ++k)
    m.bytes[p + k] = static_cast<std::uint8_t>(v >> (k * 8u));
}
std::uint32_t Monstey_BufferedRead32(Monstey_GadbBinderMemory m,
                                     Monstey_BufferedAddress at) {
  return static_cast<std::uint32_t>(Monstey_BufferedRead(m, at, 4));
}
void Monstey_BufferedAndZero(Monstey_GadbBinderMemory m,
                             Monstey_BufferedAddress at, std::size_t n) {
  (void)Monstey_BufferedRead(m, at, n);
  Monstey_BufferedWrite(m, at, 0, n);
}
Monstey_GadbBufferedResult
Monstey_BufferedFill(Monstey_GadbBinderMemory m, std::uint64_t destination,
                     std::uint32_t count, std::uint64_t site,
                     const Monstey_GadbBufferedServices &s,
                     Monstey_GadbBufferedCells cells) {
  const auto target = Monstey_BufferedRead(m, cells.fill, 8);
  if (!s.fill)
    throw Monstey_GadbBufferedImportFault(Monstey_GadbBufferedImport::fill,
                                          site, cells.fill, target);
  return s.fill(m, {cells.fill, target, destination, count, 0, site});
}
Monstey_GadbBufferedResult Monstey_BufferedAL(Monstey_GadbBufferedResult result,
                                              bool yes) {
  result.rax =
      (result.rax & ~std::uint64_t(255)) | static_cast<std::uint64_t>(yes);
  return result;
}
} // namespace
Monstey_GadbBufferedResult
Monstey_GadbBufferedCopyCached(Monstey_GadbBinderMemory m, std::uint64_t inner,
                               std::uint64_t destination, std::uint32_t count,
                               const Monstey_GadbBufferedServices &s,
                               Monstey_GadbBufferedCells cells) {
  const auto position = Monstey_BufferedRead32(m, inner + 0x18u);
  const auto end_sum = std::uint32_t(position + count);
  if (end_sum > Monstey_BufferedRead32(m, inner + 0x1Cu))
    return {std::uint64_t(end_sum) & ~std::uint64_t(255), destination};
  const auto source =
      std::uint64_t(position) + Monstey_BufferedRead(m, inner + 0x10u, 8);
  const auto target = Monstey_BufferedRead(m, cells.copy, 8);
  if (!s.copy)
    throw Monstey_GadbBufferedImportFault(Monstey_GadbBufferedImport::copy,
                                          0x141204B3FULL, cells.copy, target);
  auto result = s.copy(
      m, {cells.copy, target, destination, source, count, 0x141204B3FULL});
  const auto updated =
      std::uint32_t(Monstey_BufferedRead32(m, inner + 0x18u) + count);
  Monstey_BufferedWrite(m, inner + 0x18u, updated, 4);
  result.rax = Monstey_BufferedRead32(m, inner + 0x18u);
  if (result.rax == Monstey_BufferedRead32(m, inner + 0x1Cu)) {
    Monstey_BufferedAndZero(m, inner + 0x1Cu, 4);
    Monstey_BufferedAndZero(m, inner + 0x18u, 4);
  }
  return Monstey_BufferedAL(result, true);
}
Monstey_GadbBufferedResult Monstey_GadbBufferedDirectRead(
    Monstey_GadbBinderMemory m, std::uint64_t inner, std::uint64_t destination,
    std::uint32_t count, std::uint64_t out_count, std::uint64_t frame,
    std::uint64_t incoming_rax, const Monstey_GadbBufferedServices &s,
    Monstey_GadbBufferedCells cells) {
  Monstey_BufferedAndZero(m, out_count, 4);
  const auto handle = Monstey_BufferedRead(m, inner + 8u, 8);
  std::uint32_t remaining = count;
  std::uint64_t current = destination;
  Monstey_GadbBufferedResult result{incoming_rax, destination};
  if (remaining != 0) {
    do {
      const auto chunk = remaining < 0x7D0000u ? remaining : 0x7D0000u;
      Monstey_BufferedAndZero(m, frame + 0x20u, 8);
      const auto target = Monstey_BufferedRead(m, cells.read_file, 8);
      if (!s.read_file)
        throw Monstey_GadbBufferedImportFault(
            Monstey_GadbBufferedImport::read_file, 0x141204D34ULL,
            cells.read_file, target);
      result =
          s.read_file(m, {cells.read_file, target, handle, current, chunk,
                          frame + 0x50u, 0, frame + 0x20u, 0x141204D34ULL});
      if (static_cast<std::uint32_t>(result.rax) != 1u)
        return Monstey_BufferedAL(result, false);
      const auto got = Monstey_BufferedRead32(m, frame + 0x50u);
      if (got == 0)
        return Monstey_BufferedAL(result, true);
      const auto total =
          std::uint32_t(Monstey_BufferedRead32(m, out_count) + got);
      Monstey_BufferedWrite(m, out_count, total, 4);
      current += got;
      result.rax = got;
      remaining = std::uint32_t(remaining - got);
    } while (remaining != 0);
  }
  return Monstey_BufferedAL(result, true);
}
Monstey_GadbBufferedResult
Monstey_GadbBufferedRefill(Monstey_GadbBinderMemory m, std::uint64_t inner,
                           std::uint64_t frame, std::uint64_t incoming_rax,
                           const Monstey_GadbBufferedServices &s,
                           Monstey_GadbBufferedCells cells) {
  const auto destination = Monstey_BufferedRead(m, inner + 0x10u, 8);
  auto result = Monstey_GadbBufferedDirectRead(m, inner, destination, 0x1000u,
                                               inner + 0x1Cu, frame - 0x40u,
                                               incoming_rax, s, cells);
  if (static_cast<std::uint8_t>(result.rax) != 0) {
    Monstey_BufferedAndZero(m, inner + 0x18u, 4);
    result = Monstey_BufferedAL(result, true);
  }
  return result;
}
Monstey_GadbBufferedResult Monstey_GadbBufferedReadInner(
    Monstey_GadbBinderMemory m, std::uint64_t inner, std::uint64_t destination,
    std::uint32_t count, std::uint64_t frame, std::uint64_t incoming_rax,
    const Monstey_GadbBufferedServices &s, Monstey_GadbBufferedCells cells) {
  const auto end = Monstey_BufferedRead32(m, inner + 0x1Cu);
  const auto available =
      std::uint32_t(end - Monstey_BufferedRead32(m, inner + 0x18u));
  Monstey_GadbBufferedResult result{incoming_rax, destination};
  if (available >= count) {
    result =
        Monstey_GadbBufferedCopyCached(m, inner, destination, count, s, cells);
    return Monstey_BufferedAL(result, true);
  }
  bool failed = false;
  if (available != 0) {
    result = Monstey_GadbBufferedCopyCached(m, inner, destination, available, s,
                                            cells);
    failed = static_cast<std::uint8_t>(result.rax) == 0;
  }
  if (!failed) {
    const auto remaining = std::uint32_t(count - available);
    if (remaining >= 0x800u) {
      Monstey_BufferedAndZero(m, frame + 0x50u, 4);
      result = Monstey_GadbBufferedDirectRead(
          m, inner, destination + available, remaining, frame + 0x50u,
          frame - 0x40u, result.rax, s, cells);
      failed = static_cast<std::uint8_t>(result.rax) == 0;
      if (!failed)
        failed = Monstey_BufferedRead32(m, frame + 0x50u) != remaining;
    } else {
      result = Monstey_GadbBufferedRefill(m, inner, frame - 0x30u, result.rax,
                                          s, cells);
      failed = static_cast<std::uint8_t>(result.rax) == 0;
      if (!failed)
        failed = Monstey_BufferedRead32(m, inner + 0x1Cu) < remaining;
      if (!failed) {
        result = Monstey_GadbBufferedCopyCached(
            m, inner, destination + available, remaining, s, cells);
        failed = static_cast<std::uint8_t>(result.rax) == 0;
      }
    }
  }
  if (failed) {
    result =
        Monstey_BufferedFill(m, destination, count, 0x141204C77ULL, s, cells);
    return Monstey_BufferedAL(result, false);
  }
  return Monstey_BufferedAL(result, true);
}
Monstey_GadbBufferedResult Monstey_GadbBufferedRead(
    Monstey_GadbBinderMemory m, std::uint64_t outer, std::uint64_t destination,
    std::uint32_t count, std::uint64_t frame,
    const Monstey_GadbBufferedServices &s, Monstey_GadbBufferedCells cells) {
  Monstey_GadbBufferedResult result;
  bool failed = Monstey_BufferedRead(m, outer + 0x28u, 1) != 0;
  if (!failed) {
    const auto inner = outer + 8u;
    const auto table = Monstey_BufferedRead(m, inner, 8);
    const auto target = Monstey_BufferedRead(m, table + 0x18u, 8);
    if (target != 0x141204C2CULL)
      throw Monstey_GadbBufferedDispatchFault(0x141B50E16ULL, table + 0x18u,
                                              target, 0x141204C2CULL);
    result = Monstey_GadbBufferedReadInner(m, inner, destination, count,
                                           frame - 0x40u, table, s, cells);
    failed = static_cast<std::uint8_t>(result.rax) == 0;
  }
  if (failed) {
    Monstey_BufferedWrite(m, outer + 0x28u, 1, 1);
    result =
        Monstey_BufferedFill(m, destination, count, 0x141B50E29ULL, s, cells);
  }
  result.rax = static_cast<std::uint64_t>(
      Monstey_BufferedRead(m, outer + 0x28u, 1) != 0);
  return result;
}
} // namespace Monstey
