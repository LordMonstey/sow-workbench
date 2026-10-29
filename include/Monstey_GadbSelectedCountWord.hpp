#pragma once
#include "Monstey_GadbScalarFloat.hpp"
#include <cstdint>
namespace Monstey {
enum class Monstey_GadbSelectedCountField {
  PresentationAdvanceOnItem,
  PresentationDissolve,
  PresentationAOE,
  PresentationActionsOnStart,
  PresentationActionsOnEnd,
  NameEventCustomDetailWidgetData,
  QuestPredicates
};
struct Monstey_GadbSelectedCountClasses {
  std::uint64_t presentation = 0x142645620ull;
  std::uint64_t name_event = 0x14266baf0ull;
  std::uint64_t quest = 0x142648260ull;
};
// Seven concrete native wrappers with an explicitly zeroed private QWORD.
// record is the runtime record identity itself, not a first-QWORD wrapper.
// Gate and cache/field helper each reread the selected class-table QWORD.
// Cached wrappers consume the cache byte even when record is zero.
// The private zero fallback is owned here; no synthetic borrowed-memory slot
// is added and the generic reference getter's lazy-address API is unchanged.
// Returns raw64, never a resolved host record or original engine pointer.
std::uint64_t Monstey_GadbReadSelectedCountWord(
    Monstey_GadbBinderMemory memory, std::uint64_t record,
    Monstey_GadbSelectedCountField field,
    const Monstey_GadbSelectedCountClasses &classes = {});
} // namespace Monstey
