#pragma once
#include "Monstey_GadbRecordLookup.hpp"
#include <cstdint>
namespace Monstey {
enum class Monstey_GadbScalarFaultKind { NativeDynamicIndexAssertion };
class Monstey_GadbScalarFault final : public std::runtime_error {
public:
    Monstey_GadbScalarFaultKind kind;
    std::uint64_t address;
    Monstey_GadbScalarFault(Monstey_GadbScalarFaultKind, std::uint64_t);
};
struct Monstey_GadbGeneratedWordResult {
    bool found = false;
    std::uint64_t value = 0;
};
// 251CAC: literal generated-storage size, including zero/unknown types,
// marker flags and non-single counts. This is not serialized GADB size.
std::uint32_t Monstey_GadbGeneratedFieldSize(
    std::uint32_t type, std::uint32_t count, std::uint32_t flags);
// 251BA8 and 251C24. Class count and table are sampled once per call.
// Hashes and byte offsets are unsigned; offset accumulation wraps at32 bits.
std::uint64_t Monstey_GadbFindGeneratedField(
    Monstey_GadbBinderMemory, std::uint64_t class_object, std::uint32_t hash);
std::uint32_t Monstey_GadbGeneratedFieldOffset(
    Monstey_GadbBinderMemory, std::uint64_t class_object, std::uint32_t hash);
// 5DA158's read before its output store. The field argument points at raw72+32,
// not the declaration start. No original function or callback is invoked.
Monstey_GadbGeneratedWordResult Monstey_GadbEvaluateGeneratedWord(
    Monstey_GadbBinderMemory, std::uint64_t class_object,
    std::uint64_t field, std::uint64_t record, std::uint32_t index);
// Exact caller-visible output store of5DA158, including output/input aliasing.
// Not-found leaves output untouched and does not consume outputAddress.
std::uint8_t Monstey_GadbFetchGeneratedWord(
    Monstey_GadbBinderMemory, std::uint64_t class_object,
    std::uint64_t field, std::uint64_t record, std::uint32_t index,
    std::uint64_t outputAddress);
// 5DA0FC and115B43C. Return raw IEEE754 payload bits without evaluating a host
// float, preserving NaNs and signed zero. Defaults are read only when consumed.
// The native private output temporary is represented by an owned C++ result.
// Borrowed input memory must be stable/exclusive throughout the call.
std::uint32_t Monstey_GadbReadGeneratedFloatBits(
    Monstey_GadbBinderMemory, std::uint64_t record, std::uint64_t field,
    std::uint32_t index, std::uint64_t defaultAddress);
std::uint32_t Monstey_GadbReadScalarFloatBits(
    Monstey_GadbBinderMemory, std::uint64_t record, std::uint64_t field,
    std::uint32_t index, std::uint64_t defaultAddress);
}