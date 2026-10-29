#include "Monstey_GadbRecordLookup.hpp"
#include <cstring>
namespace Monstey {
namespace {
template <class T>
T Monstey_RecordLookupRead(Monstey_GadbBinderMemory memory,
                          std::uint64_t address) {
  if (address < memory.base || address - memory.base > memory.bytes.size() ||
      sizeof(T) > memory.bytes.size() - static_cast<std::size_t>(address - memory.base))
    throw Monstey_GadbBinderFault(
        Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable, address,
        "Record descriptor lookup consumed an unavailable owned range");
  T result;
  std::memcpy(&result, memory.bytes.data() +
                           static_cast<std::size_t>(address - memory.base),
              sizeof(result));
  return result;
}
std::uint64_t Monstey_FindIndirectRecordDescriptor(
    Monstey_GadbBinderMemory memory, std::uint64_t table,
    std::uint64_t field, std::uint8_t count) {
  std::int32_t first = 0;
  std::int32_t last = static_cast<std::int32_t>(count) - 1;
  while (first <= last) {
    const auto middle = (first + last) / 2;
    const auto row = table + 16u * static_cast<std::uint64_t>(middle);
    const auto candidate = Monstey_RecordLookupRead<std::uint32_t>(memory, row + 4);
    const auto wanted = Monstey_RecordLookupRead<std::uint32_t>(memory, field);
    if (wanted < candidate)
      last = middle - 1;
    else if (wanted == candidate)
      return table + 16u * Monstey_RecordLookupRead<std::uint8_t>(memory, row + 9);
    else
      first = middle + 1;
  }
  return 0;
}
} // namespace
std::uint64_t Monstey_GadbFindRecordDescriptor(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    std::uint64_t field) {
  const auto flags = Monstey_RecordLookupRead<std::uint8_t>(memory, record + 1);
  const auto count = Monstey_RecordLookupRead<std::uint8_t>(memory, record);
  const auto table = Monstey_RecordLookupRead<std::uint64_t>(memory, record + 8);
  if ((flags & 1u) == 0)
    return Monstey_FindIndirectRecordDescriptor(memory, table, field, count);
  std::int32_t first = 0;
  std::int32_t last = static_cast<std::int32_t>(count) - 1;
  if (last < 0)
    return 0;
  const auto wanted = Monstey_RecordLookupRead<std::uint32_t>(memory, field);
  while (first <= last) {
    const auto middle = (first + last) / 2;
    const auto row = table + 16u * static_cast<std::uint64_t>(middle);
    const auto candidate = Monstey_RecordLookupRead<std::uint32_t>(memory, row);
    if (wanted == candidate)
      return row;
    if (wanted >= candidate)
      first = middle + 1;
    else
      last = middle - 1;
  }
  return 0;
}
} // namespace Monstey
