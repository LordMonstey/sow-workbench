#include "Monstey_GuardProjectionReference.hpp"
#include <array>
#include <limits>
namespace Monstey {
namespace {
std::span<std::uint8_t> Monstey_ReferenceWord(Monstey_GadbBinderMemory image,
    std::uint64_t address, bool writing) {
  constexpr std::uint64_t width = 8;
  if (address < image.base || address > std::numeric_limits<std::uint64_t>::max() - 7)
    throw Monstey_GadbBufferedMemoryFault(address, width, writing);
  const auto offset = address - image.base;
  if (offset > image.bytes.size() || image.bytes.size() - static_cast<std::size_t>(offset) < width)
    throw Monstey_GadbBufferedMemoryFault(address, width, writing);
  return image.bytes.subspan(static_cast<std::size_t>(offset), static_cast<std::size_t>(width));
}
}
Monstey_GadbManagerGuardProjectedResult Monstey_ReferenceConstructManagerGuardProjected(
    Monstey_GadbBinderMemory image, std::uint64_t guard, std::uint64_t lock,
    Monstey_GadbBufferedResult incoming, std::uint64_t incoming_r8,
    std::uint64_t incoming_r9, const Monstey_GadbManagerPrimitiveBindings& bindings,
    const Monstey_GadbManagerEnterProjectionServices& services) {
  incoming.rdx = lock;
  auto destination = Monstey_ReferenceWord(image, guard, true);
  for (std::size_t i = 0; i != 8; ++i)
    destination[i] = static_cast<std::uint8_t>(lock >> (i * 8));
  const auto source = Monstey_ReferenceWord(image, bindings.enter_cell, false);
  std::uint64_t target = 0;
  for (std::size_t i = 0; i != 8; ++i)
    target |= std::uint64_t(source[i]) << (i * 8);
  if (!services.enter)
    throw Monstey_GadbManagerPrimitiveBoundaryFault(
        Monstey_GadbManagerPrimitiveBoundaryKind::missing_sync_service,
        0x14114BA9FULL, bindings.enter_cell, 8, bindings.enter_cell, target, lock);
  const auto observed = services.enter(image,
      {{Monstey_GadbManagerSyncOperation::enter, 0x14114BA9FULL,
        bindings.enter_cell, target, lock, incoming}, incoming_r8, incoming_r9});
  return {{guard, observed.rdx}, observed.r8, observed.r9};
}
}
