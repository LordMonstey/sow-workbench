#include "Monstey_GadbStringPointer.hpp"
#include "Monstey_GadbScalarFloat.hpp"
#include <cstring>
namespace Monstey {
namespace {
template <class T>
T Monstey_StringPointerRead(Monstey_GadbBinderMemory memory,
                           std::uint64_t address) {
  if (address < memory.base || address - memory.base > memory.bytes.size() ||
      sizeof(T) > memory.bytes.size() - static_cast<std::size_t>(address - memory.base))
    throw Monstey_GadbBinderFault(
        Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable, address,
        "String pointer getter consumed an unavailable owned range");
  T result;
  std::memcpy(&result, memory.bytes.data() +
                           static_cast<std::size_t>(address - memory.base),
              sizeof(result));
  return result;
}
} // namespace
std::uint64_t Monstey_GadbReadGeneratedStringPointer(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field, std::uint32_t index, std::uint64_t defaultValue) {
  if (!record)
    return defaultValue;
  const auto category = Monstey_StringPointerRead<std::uint64_t>(memory, record + 24);
  if (!category)
    return defaultValue;
  const auto classObject = Monstey_StringPointerRead<std::uint64_t>(memory, category + 32);
  if (!classObject)
    return defaultValue;
  const auto result = Monstey_GadbEvaluateGeneratedWord(
      memory, classObject, field, record, index);
  return result.found ? result.value : defaultValue;
}
std::uint64_t Monstey_GadbReadStringPointer(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field, std::uint32_t index, std::uint64_t defaultValue) {
  if (!record)
    return defaultValue;
  const auto descriptor = Monstey_GadbFindRecordDescriptor(memory, record, field);
  if (!descriptor)
    return defaultValue;
  if ((Monstey_StringPointerRead<std::uint8_t>(memory, descriptor + 10) & 0x3fu) != 4u)
    return defaultValue;
  if (index >= Monstey_StringPointerRead<std::uint16_t>(memory, descriptor + 12))
    return defaultValue;
  if ((Monstey_StringPointerRead<std::uint8_t>(memory, record + 1) & 0x10u) != 0)
    return Monstey_GadbReadGeneratedStringPointer(memory, record, field, index,
                                                 defaultValue);
  const auto first = Monstey_StringPointerRead<std::uint16_t>(memory, descriptor + 14);
  const auto values = Monstey_StringPointerRead<std::uint64_t>(memory, record + 16);
  const auto word = static_cast<std::uint32_t>(first + index);
  return Monstey_StringPointerRead<std::uint64_t>(memory, values + 8ull * word);
}
} // namespace Monstey
