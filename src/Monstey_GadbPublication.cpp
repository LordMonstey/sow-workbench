#include "Monstey_GadbPublication.hpp"
#include <limits>
namespace Monstey {
Monstey_GadbPublicationBoundaryFault::Monstey_GadbPublicationBoundaryFault(
    Monstey_GadbPublicationBoundaryKind k, std::uint64_t site,
    std::uint64_t caller, std::uint64_t at, std::uint32_t n,
    std::uint64_t cell, std::uint64_t destination, std::uint64_t message)
    : std::runtime_error("Consumed SoW publication boundary unavailable or nonreturning"),
      kind(k), native_site(site), call_site(caller), address(at), slot(cell),
      target(destination), message_identity(message), width(n) {}
namespace {
std::size_t Monstey_PublicationOffset(Monstey_GadbBinderMemory m,
                                    std::uint64_t at, std::size_t n, bool writing) {
  if (at < m.base || at > std::numeric_limits<std::uint64_t>::max() - (n - 1) ||
      at - m.base > m.bytes.size() ||
      n > m.bytes.size() - static_cast<std::size_t>(at - m.base))
    throw Monstey_GadbBufferedMemoryFault(at, n, writing);
  return static_cast<std::size_t>(at - m.base);
}
std::uint64_t Monstey_PublicationRead(Monstey_GadbBinderMemory m,
                                    std::uint64_t at, std::size_t n) {
  const auto offset = Monstey_PublicationOffset(m, at, n, false);
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < n; ++i)
    value |= std::uint64_t(m.bytes[offset + i]) << (i * 8u);
  return value;
}
void Monstey_PublicationWrite(Monstey_GadbBinderMemory m, std::uint64_t at,
                              std::uint64_t value, std::size_t n) {
  const auto offset = Monstey_PublicationOffset(m, at, n, true);
  for (std::size_t i = 0; i < n; ++i)
    m.bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
}
void Monstey_PublicationAndZero(Monstey_GadbBinderMemory m, std::uint64_t at) {
  (void)Monstey_PublicationRead(m, at, 8);
  Monstey_PublicationWrite(m, at, 0, 8);
}
std::uint32_t Monstey_PublicationAtomic(
    Monstey_GadbBinderMemory m, std::uint64_t site, std::uint64_t address,
    std::uint32_t addend, Monstey_GadbPublicationAtomicOperation operation,
    const Monstey_GadbPublicationServices &services) {
  if (!services.atomic32)
    throw Monstey_GadbPublicationBoundaryFault(
        Monstey_GadbPublicationBoundaryKind::missing_atomic32,
        site, site, address, 4, 0, 0, 0);
  return services.atomic32(m, {site, address, addend, operation});
}
Monstey_GadbBufferedResult Monstey_PublicationVirtual(
    Monstey_GadbBinderMemory m, std::uint64_t control, std::uint64_t site,
    std::uint64_t slot_offset, Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationServices &services) {
  incoming.rax = Monstey_PublicationRead(m, control, 8);
  const auto slot = incoming.rax + slot_offset;
  const auto target = Monstey_PublicationRead(m, slot, 8);
  if (!services.virtual_call)
    throw Monstey_GadbPublicationBoundaryFault(
        Monstey_GadbPublicationBoundaryKind::missing_virtual_call,
        site, site, control, 8, slot, target, 0);
  return services.virtual_call(
      m, {site, control, incoming.rax, slot, target, incoming});
}
[[noreturn]] void Monstey_PublicationTrap(
    Monstey_GadbBinderMemory m, std::uint64_t caller,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationServices &services) {
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

Monstey_GadbBufferedResult Monstey_GadbReleasePublicationSecondary(
    Monstey_GadbBinderMemory m, std::uint64_t control,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationServices &services) {
  incoming.rax = Monstey_PublicationAtomic(
      m, 0x1403DD713ULL, control + 0xC, 0xFFFFFFFFu,
      Monstey_GadbPublicationAtomicOperation::fetch_add32, services);
  if (incoming.rax == 1)
    incoming = Monstey_PublicationVirtual(m, control, 0x1403DD720ULL, 8,
                                         incoming, services);
  return incoming;
}
Monstey_GadbBufferedResult Monstey_GadbReleasePublicationPrimary(
    Monstey_GadbBinderMemory m, std::uint64_t control,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationServices &services) {
  incoming.rax = Monstey_PublicationAtomic(
      m, 0x1403DD6E8ULL, control + 8, 0xFFFFFFFFu,
      Monstey_GadbPublicationAtomicOperation::fetch_add32, services);
  if (incoming.rax == 1) {
    incoming = Monstey_PublicationVirtual(m, control, 0x1403DD6FBULL, 0,
                                         incoming, services);
    incoming = Monstey_GadbReleasePublicationSecondary(m, control, incoming,
                                                      services);
  }
  return incoming;
}
Monstey_GadbBufferedResult Monstey_GadbReleasePublicationPair(
    Monstey_GadbBinderMemory m, std::uint64_t pair,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationServices &services) {
  const auto control = Monstey_PublicationRead(m, pair + 8, 8);
  if (control != 0)
    incoming = Monstey_GadbReleasePublicationPrimary(m, control, incoming, services);
  return incoming;
}
Monstey_GadbBufferedResult Monstey_GadbAssignPublicationPair(
    Monstey_GadbBinderMemory m, std::uint64_t destination,
    std::uint64_t new_data, std::uint64_t new_control,
    Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationServices &services) {
  incoming.rdx = new_data;
  const auto old_control = Monstey_PublicationRead(m, destination + 8, 8);
  if (old_control != 0)
    incoming = Monstey_GadbReleasePublicationPrimary(m, old_control, incoming,
                                                    services);
  Monstey_PublicationWrite(m, destination, new_data, 8);
  Monstey_PublicationWrite(m, destination + 8, new_control, 8);
  return incoming;
}
Monstey_GadbBufferedResult Monstey_GadbCopyPublicationPair(
    Monstey_GadbBinderMemory m, std::uint64_t destination,
    std::uint64_t source, Monstey_GadbBufferedResult incoming,
    const Monstey_GadbPublicationServices &services) {
  incoming.rdx = source;
  Monstey_PublicationAndZero(m, destination);
  Monstey_PublicationAndZero(m, destination + 8);
  const auto control = Monstey_PublicationRead(m, source + 8, 8);
  incoming.rdx = Monstey_PublicationRead(m, source, 8);
  if (control != 0)
    (void)Monstey_PublicationAtomic(
        m, 0x1404BAEA6ULL, control + 8, 1,
        Monstey_GadbPublicationAtomicOperation::increment32, services);
  incoming = Monstey_GadbAssignPublicationPair(m, destination, incoming.rdx,
                                               control, incoming, services);
  incoming.rax = destination;
  return incoming;
}
Monstey_GadbBufferedResult Monstey_GadbPublishDatabaseStorage(
    Monstey_GadbBinderMemory m, std::uint64_t database,
    std::uint32_t header_h8, std::uint32_t header_h9, std::uint64_t bank_name,
    std::uint64_t body_rsp, const Monstey_GadbPublicationServices &services) {
  Monstey_GadbBufferedResult result{
      Monstey_PublicationRead(m, body_rsp + 0x60, 4), header_h8};
  Monstey_PublicationWrite(m, database + 0x48, result.rax, 4);
  result.rax = Monstey_PublicationRead(m, body_rsp + 0x70, 8);
  Monstey_PublicationWrite(m, database + 0x50, result.rax, 8);
  result.rax = Monstey_PublicationRead(m, body_rsp + 0x80, 8);
  Monstey_PublicationWrite(m, database + 0x28, header_h8, 4);
  result.rdx = Monstey_PublicationRead(m, body_rsp + 0x78, 8);
  Monstey_PublicationWrite(m, database + 8, result.rax, 8);
  Monstey_PublicationWrite(m, database, bank_name, 8);
  Monstey_PublicationWrite(m, database + 0x2C, header_h9, 4);
  result = Monstey_GadbCopyPublicationPair(m, body_rsp + 0x20, result.rdx,
                                           result, services);
  auto old = Monstey_PublicationRead(m, database + 0x60, 8);
  result.rdx = Monstey_PublicationRead(m, result.rax + 8, 8);
  Monstey_PublicationWrite(m, result.rax + 8, old, 8);
  Monstey_PublicationWrite(m, database + 0x60, result.rdx, 8);
  old = Monstey_PublicationRead(m, database + 0x58, 8);
  result.rdx = Monstey_PublicationRead(m, result.rax, 8);
  Monstey_PublicationWrite(m, result.rax, old, 8);
  Monstey_PublicationWrite(m, database + 0x58, result.rdx, 8);
  result = Monstey_GadbReleasePublicationPair(m, body_rsp + 0x20, result,
                                              services);
  result.rax = Monstey_PublicationRead(m, body_rsp + 0x88, 8);
  const auto destination_owner = database + 0x20;
  Monstey_PublicationWrite(m, database + 0x10, result.rax, 8);
  result.rax = Monstey_PublicationRead(m, body_rsp + 0x90, 4);
  Monstey_PublicationWrite(m, database + 0x30, result.rax, 4);
  result.rax = Monstey_PublicationRead(m, body_rsp + 0x98, 4);
  Monstey_PublicationWrite(m, database + 0x34, result.rax, 4);
  result.rax = Monstey_PublicationRead(m, body_rsp + 0xA0, 8);
  Monstey_PublicationWrite(m, database + 0x40, result.rax, 8);
  result.rax = Monstey_PublicationRead(m, body_rsp + 0x68, 4);
  Monstey_PublicationWrite(m, database + 0x4C, result.rax, 4);
  const auto source_owner = Monstey_PublicationRead(m, body_rsp + 0xA8, 8);
  if (destination_owner != source_owner) {
    result.rdx = Monstey_PublicationRead(m, source_owner, 8);
    Monstey_PublicationAndZero(m, source_owner);
    result.rax = Monstey_PublicationRead(m, destination_owner, 8);
    Monstey_PublicationWrite(m, destination_owner, result.rdx, 8);
    if (result.rax != 0)
      Monstey_PublicationTrap(m, 0x1410ED7D7ULL, result, services);
  }
  const auto final_pair = Monstey_PublicationRead(m, body_rsp + 0x78, 8);
  result = Monstey_GadbReleasePublicationPair(m, final_pair, result, services);
  if (Monstey_PublicationRead(m, source_owner, 8) != 0)
    Monstey_PublicationTrap(m, 0x1410ED7E9ULL, result, services);
  return result;
}
} // namespace Monstey
