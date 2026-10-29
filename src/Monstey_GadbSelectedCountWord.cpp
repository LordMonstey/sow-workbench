#include "Monstey_GadbSelectedCountWord.hpp"
#include <bit>
#include <cstddef>
#include <stdexcept>
namespace Monstey {
namespace {
using Monstey_U64 = std::uint64_t;
Monstey_U64 Monstey_SelectedRead(Monstey_GadbBinderMemory memory,
                                 Monstey_U64 address, std::size_t width) {
  if (address < memory.base || address - memory.base > memory.bytes.size() ||
      width >
          memory.bytes.size() - static_cast<std::size_t>(address - memory.base))
    throw Monstey_GadbBinderFault(
        Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable, address,
        "Selected count consumer range unavailable");
  const auto offset = static_cast<std::size_t>(address - memory.base);
  Monstey_U64 result = 0;
  for (std::size_t i = 0; i < width; ++i)
    result |= static_cast<Monstey_U64>(memory.bytes[offset + i]) << (8 * i);
  return result;
}
// Actual 30E7A4 ->30E7D0 ->447E6F50/447E7023 with caller's private zero.
Monstey_U64 Monstey_SelectedCachedZero(Monstey_GadbBinderMemory memory,
                                       Monstey_U64 record,
                                       std::uint8_t physical) {
  if (record == 0)
    return 0;
  const auto table = Monstey_SelectedRead(memory, record + 8, 8);
  const auto descriptor = table + static_cast<Monstey_U64>(physical) * 16;
  if (Monstey_SelectedRead(memory, descriptor + 12, 2) == 0)
    return 0;
  const auto first = Monstey_SelectedRead(memory, descriptor + 14, 2);
  const auto values = Monstey_SelectedRead(memory, record + 16, 8);
  return Monstey_SelectedRead(memory, values + 8 * first, 8);
}
// 16CEE38 ->68BB08 ->16CE2E8, selected private-zero caller composition.
// This is not an alternative generic default-by-value reference API.
Monstey_U64 Monstey_SelectedReferenceZero(Monstey_GadbBinderMemory memory,
                                          Monstey_U64 record,
                                          Monstey_U64 field) {
  if (record == 0)
    return 0;
  const auto descriptor =
      Monstey_GadbFindRecordDescriptor(memory, record, field);
  if (descriptor == 0 ||
      (Monstey_SelectedRead(memory, descriptor + 10, 1) & 0x3fu) != 10 ||
      Monstey_SelectedRead(memory, descriptor + 12, 2) == 0)
    return 0;
  if ((Monstey_SelectedRead(memory, record + 1, 1) & 0x10u) != 0) {
    const auto owner = Monstey_SelectedRead(memory, record + 24, 8);
    if (owner == 0 || Monstey_SelectedRead(memory, owner + 32, 8) == 0)
      return 0;
    const auto declarationClass = Monstey_SelectedRead(memory, owner + 32, 8);
    const auto result = Monstey_GadbEvaluateGeneratedWord(
        memory, declarationClass, field, record, 0);
    return result.found ? result.value : 0;
  }
  const auto first = Monstey_SelectedRead(memory, descriptor + 14, 2);
  const auto values = Monstey_SelectedRead(memory, record + 16, 8);
  return Monstey_SelectedRead(memory, values + first * 8, 8);
}
} // namespace
std::uint64_t Monstey_GadbReadSelectedCountWord(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    Monstey_GadbSelectedCountField selected,
    const Monstey_GadbSelectedCountClasses &classes) {
  std::uint64_t owner = 0, ordinal = 0;
  switch (selected) {
  case Monstey_GadbSelectedCountField::PresentationAdvanceOnItem:
    owner = classes.presentation;
    ordinal = 87;
    break;
  case Monstey_GadbSelectedCountField::PresentationDissolve:
    owner = classes.presentation;
    ordinal = 117;
    break;
  case Monstey_GadbSelectedCountField::PresentationAOE:
    owner = classes.presentation;
    ordinal = 118;
    break;
  case Monstey_GadbSelectedCountField::PresentationActionsOnStart:
    owner = classes.presentation;
    ordinal = 119;
    break;
  case Monstey_GadbSelectedCountField::PresentationActionsOnEnd:
    owner = classes.presentation;
    ordinal = 120;
    break;
  case Monstey_GadbSelectedCountField::NameEventCustomDetailWidgetData:
    owner = classes.name_event;
    ordinal = 37;
    break;
  case Monstey_GadbSelectedCountField::QuestPredicates:
    owner = classes.quest;
    ordinal = 73;
    break;
  default:
    throw std::invalid_argument("Unknown selected native count field");
  }
  const auto gateTable = Monstey_SelectedRead(memory, owner, 8);
  const auto state = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(
      Monstey_SelectedRead(memory, gateTable + ordinal * 72 + 48, 4)));
  // Native wrappers initialize their private fallback after the gate call.
  // The following helper is called regardless of a null record argument.
  const auto selectedTable = Monstey_SelectedRead(memory, owner, 8);
  if (state >= 2) {
    const auto physical = static_cast<std::uint8_t>(
        Monstey_SelectedRead(memory, selectedTable + ordinal * 72 + 64, 1));
    return Monstey_SelectedCachedZero(memory, record, physical);
  }
  return Monstey_SelectedReferenceZero(memory, record,
                                       selectedTable + ordinal * 72 + 32);
}
} // namespace Monstey
