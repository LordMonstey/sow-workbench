#include "Monstey_GadbAudioActionWord.hpp"
#include <bit>
#include <cstddef>
#include <stdexcept>
namespace Monstey {
namespace {
using Monstey_U64 = std::uint64_t;
using Monstey_U32 = std::uint32_t;
class Monstey_AudioMemory {
  Monstey_GadbBinderMemory memory_;

public:
  explicit Monstey_AudioMemory(Monstey_GadbBinderMemory memory)
      : memory_(memory) {}
  Monstey_U64 Read(Monstey_U64 address, std::size_t width) const {
    if (address < memory_.base ||
        address - memory_.base > memory_.bytes.size() ||
        width > memory_.bytes.size() -
                    static_cast<std::size_t>(address - memory_.base))
      throw Monstey_GadbBinderFault(
          Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable, address,
          "Consumed audio action getter range unavailable");
    const auto offset = static_cast<std::size_t>(address - memory_.base);
    Monstey_U64 value = 0;
    for (std::size_t i = 0; i < width; ++i)
      value |= static_cast<Monstey_U64>(memory_.bytes[offset + i]) << (8 * i);
    return value;
  }
};
Monstey_U64 Monstey_AudioDescriptor(const Monstey_AudioMemory &source,
                                    Monstey_U64 record, Monstey_U64 field,
                                    Monstey_U64 table, std::uint8_t flags) {
  const auto count = static_cast<std::int32_t>(source.Read(record, 1));
  if (count == 0)
    return 0;
  // All four complete inline bodies preload this hash before the loop,
  // including the indirect branch. 251BD0 has a different read order.
  const auto wanted = static_cast<Monstey_U32>(source.Read(field + 32, 4));
  std::int32_t low = 0;
  std::int32_t high = count - 1;
  while (low <= high) {
    const auto middle = (low + high) / 2;
    const auto row = table + static_cast<Monstey_U64>(middle) * 16;
    const auto candidate = static_cast<Monstey_U32>(
        source.Read(row + ((flags & 1u) != 0 ? 0u : 4u), 4));
    if (wanted == candidate) {
      if ((flags & 1u) != 0)
        return row;
      return table + source.Read(row + 9, 1) * 16;
    }
    if (wanted < candidate)
      high = middle - 1;
    else
      low = middle + 1;
  }
  return 0;
}
Monstey_U64 Monstey_AudioGeneratedZero(const Monstey_AudioMemory &source,
                                       Monstey_U64 record, Monstey_U64 field,
                                       Monstey_U64 fallback) {
  const auto owner = source.Read(record + 24, 8);
  if (owner == 0)
    return fallback;
  const auto class_object = source.Read(owner + 32, 8);
  if (class_object == 0)
    return fallback;
  const auto count = static_cast<Monstey_U32>(source.Read(class_object + 8, 4));
  const auto wanted = static_cast<Monstey_U32>(source.Read(field + 32, 4));
  if (count == 0)
    return fallback;
  const auto table = source.Read(class_object, 8);
  Monstey_U64 selected = 0;
  bool found = false;
  for (Monstey_U32 i = 0; i < count; ++i) {
    const auto candidate = table + static_cast<Monstey_U64>(i) * 72;
    if (source.Read(candidate + 32, 4) == wanted) {
      selected = candidate;
      found = true;
      break;
    }
  }
  if (!found)
    return fallback;
  Monstey_U32 offset = 0;
  for (Monstey_U32 i = 0; i < count; ++i) {
    const auto candidate = table + static_cast<Monstey_U64>(i) * 72;
    if (source.Read(candidate + 32, 4) == wanted)
      break;
    // Inline native bodies read all three operands even for flags&280.
    const auto flags = static_cast<Monstey_U32>(source.Read(candidate + 44, 4));
    const auto declared_count =
        static_cast<Monstey_U32>(source.Read(candidate + 40, 4));
    const auto type = static_cast<Monstey_U32>(source.Read(candidate + 36, 4));
    offset = (offset + 7u +
              Monstey_GadbGeneratedFieldSize(type, declared_count, flags)) &
             0xfffffff8u;
  }
  const auto storage =
      source.Read(record + 16, 8) + static_cast<Monstey_U64>(offset);
  if (source.Read(selected + 40, 4) != UINT32_MAX)
    return source.Read(storage, 8);
  if (source.Read(storage + 8, 4) == 0)
    throw Monstey_GadbScalarFault(
        Monstey_GadbScalarFaultKind::NativeDynamicIndexAssertion, storage + 8);
  return source.Read(source.Read(storage, 8), 8);
}
Monstey_U64 Monstey_AudioStage(const Monstey_AudioMemory &source,
                               Monstey_U64 record, Monstey_U64 field,
                               std::uint8_t type, Monstey_U64 fallback) {
  const auto state = std::bit_cast<std::int32_t>(
      static_cast<Monstey_U32>(source.Read(field + 48, 4)));
  if (state >= 2) {
    if (record == 0)
      return fallback;
    const auto physical = source.Read(field + 64, 1);
    const auto table = source.Read(record + 8, 8);
    const auto descriptor = table + physical * 16;
    if (source.Read(descriptor + 12, 2) == 0)
      return fallback;
    const auto word = source.Read(descriptor + 14, 2);
    return source.Read(source.Read(record + 16, 8) + word * 8, 8);
  }
  if (record == 0)
    return fallback;
  const auto flags = static_cast<std::uint8_t>(source.Read(record + 1, 1));
  const auto table = source.Read(record + 8, 8);
  const auto descriptor =
      Monstey_AudioDescriptor(source, record, field, table, flags);
  if (descriptor == 0 || (source.Read(descriptor + 10, 1) & 0x3fu) != type ||
      source.Read(descriptor + 12, 2) == 0)
    return fallback;
  if ((flags & 0x10u) != 0)
    return Monstey_AudioGeneratedZero(source, record, field, fallback);
  const auto word = source.Read(descriptor + 14, 2);
  return source.Read(source.Read(record + 16, 8) + word * 8, 8);
}
} // namespace
std::uint64_t
Monstey_GadbReadAudioActionWord(Monstey_GadbBinderMemory memory,
                                std::uint64_t wrapper,
                                Monstey_GadbAudioActionPath path,
                                const Monstey_GadbAudioActionClasses &classes) {
  std::uint64_t ordinal = 0;
  std::uint64_t child_class = 0;
  std::uint64_t child_ordinal = 0;
  std::uint8_t child_type = 4;
  std::uint64_t fallback = classes.empty_string;
  switch (path) {
  case Monstey_GadbAudioActionPath::SwitchName:
    ordinal = 9;
    child_class = classes.set_switch;
    break;
  case Monstey_GadbAudioActionPath::SwitchValue:
    ordinal = 9;
    child_class = classes.set_switch;
    child_ordinal = 2;
    break;
  case Monstey_GadbAudioActionPath::RtpcName:
    ordinal = 10;
    child_class = classes.set_rtpc;
    break;
  case Monstey_GadbAudioActionPath::Variable:
    ordinal = 21;
    child_class = classes.variable_operation;
    child_type = 10;
    fallback = 0;
    break;
  default:
    throw std::invalid_argument("Unknown audio action getter path");
  }
  const Monstey_AudioMemory source(memory);
  const auto action_fields = source.Read(classes.action, 8);
  const auto record = source.Read(wrapper, 8);
  const auto child =
      Monstey_AudioStage(source, record, action_fields + ordinal * 72, 10, 0);
  const auto child_fields = source.Read(child_class, 8);
  return Monstey_AudioStage(source, child, child_fields + child_ordinal * 72,
                            child_type, fallback);
}
} // namespace Monstey
