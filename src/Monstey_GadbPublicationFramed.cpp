#include "Monstey_GadbPublicationFramed.hpp"
#include <limits>
namespace Monstey {
namespace {
std::size_t Monstey_FramedPublicationOffset(Monstey_GadbBinderMemory m,
    std::uint64_t at, std::size_t n, bool writing) {
  if (at < m.base || at > std::numeric_limits<std::uint64_t>::max() - (n - 1) ||
      at - m.base > m.bytes.size() ||
      n > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw Monstey_GadbBufferedMemoryFault(at, n, writing);
  return static_cast<std::size_t>(at - m.base);
}
std::uint64_t Monstey_FramedPublicationRead(Monstey_GadbBinderMemory m,
    std::uint64_t at, std::size_t n) {
  const auto offset = Monstey_FramedPublicationOffset(m, at, n, false);
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < n; ++i)
    value |= std::uint64_t(m.bytes[offset + i]) << (i * 8u);
  return value;
}
void Monstey_FramedPublicationWrite(Monstey_GadbBinderMemory m,
    std::uint64_t at, std::uint64_t value, std::size_t n) {
  const auto offset = Monstey_FramedPublicationOffset(m, at, n, true);
  for (std::size_t i = 0; i < n; ++i)
    m.bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
}
void Monstey_FramedPublicationAndZero(Monstey_GadbBinderMemory m,
    std::uint64_t at) {
  (void)Monstey_FramedPublicationRead(m, at, 8);
  Monstey_FramedPublicationWrite(m, at, 0, 8);
}
std::uint32_t Monstey_FramedPublicationAtomic(Monstey_GadbBinderMemory m,
    std::uint64_t site, std::uint64_t address, std::uint32_t addend,
    Monstey_GadbPublicationAtomicOperation operation,
    const Monstey_GadbPublicationFramedServices &services) {
  if (!services.atomic32)
    throw Monstey_GadbPublicationBoundaryFault(
        Monstey_GadbPublicationBoundaryKind::missing_atomic32,
        site, site, address, 4, 0, 0, 0);
  return services.atomic32(m, {site, address, addend, operation});
}
Monstey_GadbBufferedResult Monstey_FramedPublicationVirtual(
    Monstey_GadbBinderMemory m, std::uint64_t control, std::uint64_t site,
    std::uint64_t slot_offset, std::uint64_t body_rsp,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationFramedServices &services) {
  incoming.rax = Monstey_FramedPublicationRead(m, control, 8);
  const auto slot = incoming.rax + slot_offset;
  const auto target = Monstey_FramedPublicationRead(m, slot, 8);
  if (!services.virtual_call)
    throw Monstey_GadbPublicationBoundaryFault(
        Monstey_GadbPublicationBoundaryKind::missing_virtual_call,
        site, site, control, 8, slot, target, 0);
  return services.virtual_call(
      m, {{site, control, incoming.rax, slot, target, incoming}, body_rsp});
}
[[noreturn]] void Monstey_FramedPublicationTrap(Monstey_GadbBinderMemory m,
    std::uint64_t caller, Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationFramedServices &services) {
  if (!services.trap)
    throw Monstey_GadbPublicationBoundaryFault(
        Monstey_GadbPublicationBoundaryKind::invariant_trap,
        0x140819113ULL, caller, 0, 4, 0, 0x140819104ULL, 0x141F65A38ULL);
  services.trap(m, {caller, 0x140819104ULL, 0x140819113ULL,
                    0x141F65A38ULL, 0, 0, incoming});
  throw Monstey_GadbPublicationBoundaryFault(
      Monstey_GadbPublicationBoundaryKind::trap_returned,
      0x140819113ULL, caller, 0, 4, 0, 0x140819104ULL, 0x141F65A38ULL);
}
} // namespace
Monstey_GadbBufferedResult Monstey_GadbReleasePublicationSecondaryFramed(
    Monstey_GadbBinderMemory m, std::uint64_t control, std::uint64_t body_rsp,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationFramedServices &services) {
  incoming.rax = Monstey_FramedPublicationAtomic(
      m, 0x1403DD713ULL, control + 0xC, 0xFFFFFFFFu,
      Monstey_GadbPublicationAtomicOperation::fetch_add32, services);
  if (incoming.rax == 1)
    incoming = Monstey_FramedPublicationVirtual(m, control, 0x1403DD720ULL,
                                               8, body_rsp, incoming, services);
  return incoming;
}
Monstey_GadbBufferedResult Monstey_GadbReleasePublicationPrimaryFramed(
    Monstey_GadbBinderMemory m, std::uint64_t control, std::uint64_t body_rsp,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationFramedServices &services) {
  incoming.rax = Monstey_FramedPublicationAtomic(
      m, 0x1403DD6E8ULL, control + 8, 0xFFFFFFFFu,
      Monstey_GadbPublicationAtomicOperation::fetch_add32, services);
  if (incoming.rax == 1) {
    incoming = Monstey_FramedPublicationVirtual(m, control, 0x1403DD6FBULL,
                                               0, body_rsp, incoming, services);
    incoming = Monstey_GadbReleasePublicationSecondaryFramed(
        m, control, body_rsp, incoming, services);
  }
  return incoming;
}
Monstey_GadbBufferedResult Monstey_GadbReleasePublicationPairFramed(
    Monstey_GadbBinderMemory m, std::uint64_t pair, std::uint64_t body_rsp,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationFramedServices &services) {
  const auto control = Monstey_FramedPublicationRead(m, pair + 8, 8);
  if (control != 0)
    incoming = Monstey_GadbReleasePublicationPrimaryFramed(
        m, control, body_rsp - 0x30, incoming, services);
  return incoming;
}
Monstey_GadbBufferedResult Monstey_GadbAssignPublicationPairFramed(
    Monstey_GadbBinderMemory m, std::uint64_t destination,
    std::uint64_t new_data, std::uint64_t new_control, std::uint64_t body_rsp,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationFramedServices &services) {
  incoming.rdx = new_data;
  const auto old_control = Monstey_FramedPublicationRead(m, destination + 8, 8);
  if (old_control != 0)
    incoming = Monstey_GadbReleasePublicationPrimaryFramed(
        m, old_control, body_rsp - 0x30, incoming, services);
  Monstey_FramedPublicationWrite(m, destination, new_data, 8);
  Monstey_FramedPublicationWrite(m, destination + 8, new_control, 8);
  return incoming;
}
Monstey_GadbBufferedResult Monstey_GadbCopyPublicationPairFramed(
    Monstey_GadbBinderMemory m, std::uint64_t destination, std::uint64_t source,
    std::uint64_t body_rsp, Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationFramedServices &services) {
  incoming.rdx = source;
  Monstey_FramedPublicationAndZero(m, destination);
  Monstey_FramedPublicationAndZero(m, destination + 8);
  const auto control = Monstey_FramedPublicationRead(m, source + 8, 8);
  incoming.rdx = Monstey_FramedPublicationRead(m, source, 8);
  if (control != 0)
    (void)Monstey_FramedPublicationAtomic(
        m, 0x1404BAEA6ULL, control + 8, 1,
        Monstey_GadbPublicationAtomicOperation::increment32, services);
  incoming = Monstey_GadbAssignPublicationPairFramed(
      m, destination, incoming.rdx, control, body_rsp - 0x30, incoming, services);
  incoming.rax = destination;
  return incoming;
}
Monstey_GadbBufferedResult Monstey_GadbPublishDatabaseStorageFramed(
    Monstey_GadbBinderMemory m, std::uint64_t database,
    std::uint32_t header_h8, std::uint32_t header_h9, std::uint64_t bank_name,
    std::uint64_t body_rsp, const Monstey_GadbPublicationFramedServices &services) {
  Monstey_GadbBufferedResult result{
      Monstey_FramedPublicationRead(m, body_rsp + 0x60, 4), header_h8};
  Monstey_FramedPublicationWrite(m, database + 0x48, result.rax, 4);
  result.rax = Monstey_FramedPublicationRead(m, body_rsp + 0x70, 8);
  Monstey_FramedPublicationWrite(m, database + 0x50, result.rax, 8);
  result.rax = Monstey_FramedPublicationRead(m, body_rsp + 0x80, 8);
  Monstey_FramedPublicationWrite(m, database + 0x28, header_h8, 4);
  result.rdx = Monstey_FramedPublicationRead(m, body_rsp + 0x78, 8);
  Monstey_FramedPublicationWrite(m, database + 8, result.rax, 8);
  Monstey_FramedPublicationWrite(m, database, bank_name, 8);
  Monstey_FramedPublicationWrite(m, database + 0x2C, header_h9, 4);
  result = Monstey_GadbCopyPublicationPairFramed(
      m, body_rsp + 0x20, result.rdx, body_rsp - 0x30, result, services);
  auto old = Monstey_FramedPublicationRead(m, database + 0x60, 8);
  result.rdx = Monstey_FramedPublicationRead(m, result.rax + 8, 8);
  Monstey_FramedPublicationWrite(m, result.rax + 8, old, 8);
  Monstey_FramedPublicationWrite(m, database + 0x60, result.rdx, 8);
  old = Monstey_FramedPublicationRead(m, database + 0x58, 8);
  result.rdx = Monstey_FramedPublicationRead(m, result.rax, 8);
  Monstey_FramedPublicationWrite(m, result.rax, old, 8);
  Monstey_FramedPublicationWrite(m, database + 0x58, result.rdx, 8);
  result = Monstey_GadbReleasePublicationPairFramed(
      m, body_rsp + 0x20, body_rsp - 0x30, result, services);
  result.rax = Monstey_FramedPublicationRead(m, body_rsp + 0x88, 8);
  const auto destination_owner = database + 0x20;
  Monstey_FramedPublicationWrite(m, database + 0x10, result.rax, 8);
  result.rax = Monstey_FramedPublicationRead(m, body_rsp + 0x90, 4);
  Monstey_FramedPublicationWrite(m, database + 0x30, result.rax, 4);
  result.rax = Monstey_FramedPublicationRead(m, body_rsp + 0x98, 4);
  Monstey_FramedPublicationWrite(m, database + 0x34, result.rax, 4);
  result.rax = Monstey_FramedPublicationRead(m, body_rsp + 0xA0, 8);
  Monstey_FramedPublicationWrite(m, database + 0x40, result.rax, 8);
  result.rax = Monstey_FramedPublicationRead(m, body_rsp + 0x68, 4);
  Monstey_FramedPublicationWrite(m, database + 0x4C, result.rax, 4);
  const auto source_owner = Monstey_FramedPublicationRead(m, body_rsp + 0xA8, 8);
  if (destination_owner != source_owner) {
    result.rdx = Monstey_FramedPublicationRead(m, source_owner, 8);
    Monstey_FramedPublicationAndZero(m, source_owner);
    result.rax = Monstey_FramedPublicationRead(m, destination_owner, 8);
    Monstey_FramedPublicationWrite(m, destination_owner, result.rdx, 8);
    if (result.rax != 0)
      Monstey_FramedPublicationTrap(m, 0x1410ED7D7ULL, result, services);
  }
  const auto final_pair = Monstey_FramedPublicationRead(m, body_rsp + 0x78, 8);
  result = Monstey_GadbReleasePublicationPairFramed(
      m, final_pair, body_rsp - 0x30, result, services);
  if (Monstey_FramedPublicationRead(m, source_owner, 8) != 0)
    Monstey_FramedPublicationTrap(m, 0x1410ED7E9ULL, result, services);
  return result;
}
} // namespace Monstey
