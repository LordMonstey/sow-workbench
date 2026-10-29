#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace Monstey {
struct Monstey_FearConstraintRow { std::string key;std::uint32_t type,word4,word8; };
struct Monstey_FearConstraintTable {
 bool complete=false;std::uint64_t consumed=0,read_calls=0,records=0;
 std::vector<Monstey_FearConstraintRow> rows;
};
// Concrete read-only owned-span profile around the exact control-flow reader.
// Short reads zero the requested owned destination and retain the input position.
// This is the span adapter policy, not every original ILTInStream implementation.
// Names use the first NUL, duplicate keys overwrite, ordering compares unsigned
// bytes then lengths. Physical MSVCP71 objects/tree/allocator/exception ABI is not
// reconstructed by the logical container binding. No original DLL is loaded.
Monstey_FearConstraintTable Monstey_FearDecodeConstraints(std::span<const std::uint8_t> input);
}
