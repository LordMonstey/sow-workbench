#pragma once
#include "Monstey_GadbScalarFloat.hpp"
#include <cstdint>
namespace Monstey {
enum class Monstey_GadbAudioActionPath {
  SwitchName,
  SwitchValue,
  RtpcName,
  Variable
};
// These are virtual identities in the caller-owned runtime image, not host
// pointers or serialized GADB record links. Native defaults pin both PEs.
struct Monstey_GadbAudioActionClasses {
  std::uint64_t action = 0x14264c660ull;
  std::uint64_t set_switch = 0x14264c560ull;
  std::uint64_t set_rtpc = 0x14264c520ull;
  std::uint64_t variable_operation = 0x14264c5a0ull;
  std::uint64_t empty_string = 0x141db10e0ull;
};
// D1ADC0 / D1B3B0 / D18ED0 / D1C030, respectively. wrapper is the
// address of the native first-QWORD record holder. The two stages preserve
// inline flags/table/hash sampling and lazy descriptor/count/value reads.
// Returns raw string identity for the first three paths, raw reference word
// for Variable. A stored zero is returned unchanged. No string is read and no
// record link is resolved, original entry point called, or borrowed byte
// written.
std::uint64_t Monstey_GadbReadAudioActionWord(
    Monstey_GadbBinderMemory memory, std::uint64_t wrapper,
    Monstey_GadbAudioActionPath path,
    const Monstey_GadbAudioActionClasses &classes = {});
} // namespace Monstey
