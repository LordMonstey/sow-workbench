#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
namespace Monstey {
// An exclusively borrowed, caller-owned byte image. All embedded native
// addresses are unsigned virtual identities relative to base, never invoked.
// Overlapping class/field/category/record/code/string ranges intentionally alias.
struct Monstey_GadbBinderMemory {
  std::uint64_t base = 0;
  std::span<std::uint8_t> bytes;
};
enum class Monstey_GadbBinderFaultKind {
  ConsumedRangeUnavailable,
  UnsupportedIndexCallback,
  NativeParentCorrespondenceAssertion,
  OwnedStepLimit
};
class Monstey_GadbBinderFault final : public std::runtime_error {
public:
  Monstey_GadbBinderFaultKind kind;
  std::uint64_t address;
  Monstey_GadbBinderFault(Monstey_GadbBinderFaultKind kind,
                          std::uint64_t address, const char *message);
};
enum class Monstey_GadbBinderEventKind {
  InitializeEnter, CacheWrite, InitializeComplete, CategoryBound, LoopSamples,
  StateSkipped, MarkerSkipped, IndexCallback, SuccessCall, FallbackCall,
  StateWrite, ParentPropagation
};
struct Monstey_GadbBinderEvent {
  Monstey_GadbBinderEventKind kind;
  std::uint64_t subject = 0, other = 0;
  std::uint32_t value = 0, auxiliary = 0;
};
struct Monstey_GadbBinderTrace {
  std::uint64_t consumed_operations = 0;
  std::vector<Monstey_GadbBinderEvent> events;
};
// Concrete owned translation of 79E2A0, including its complete cold body.
// Parent-first lazy initialization, live field-count/table reloads, compact U8
// writes and marker/null-row skips. Does not construct/reset binding states.
// A finite owned step budget is diagnostic only, not a native iteration limit.
void Monstey_GadbInitializeSelectedClass(
    Monstey_GadbBinderMemory memory, std::uint64_t selected_class,
    Monstey_GadbBinderTrace &trace, std::uint64_t owned_step_budget = 1000000);
// Concrete 196CE8/16202FC helpers. The U32 index is the original declaration
// index, including markers. Source and ancestor memory may alias; each native
// read/store is performed sequentially. A consumed fault preserves prior writes.
void Monstey_GadbPropagateSelectedField(
    Monstey_GadbBinderMemory memory, std::uint64_t selected_class,
    std::uint64_t field, std::uint32_t declaration_index, bool success,
    Monstey_GadbBinderTrace &trace, std::uint64_t owned_step_budget = 1000000);
// The actual preselection count gate at 196AC4/CB. This must run BEFORE an
// external class-name comparison, once; its result is not silently rechecked.
bool Monstey_GadbCategoryHasBindingRecords(
    Monstey_GadbBinderMemory memory, std::uint64_t category,
    Monstey_GadbBinderTrace &trace, std::uint64_t owned_step_budget = 1000000);
// Concrete selected-class/category portion of 196A34, entry 196B19. Name
// selection is external and is NOT guessed. Its earlier count gate is NOT
// repeated after comparison. A nonnull class is initialized, stored at
// category+20, and its first
// named record (otherwise first anonymous) drives the real ordered field loop.
// Native class/record field-count and class-mask sampling epochs are preserved.
// Only exact constant callback bodies B0 xx C3, 32/30/33 C0 C3 and B8 imm32 C3
// are consumed by the existing reflector; opaque callbacks stop at their call.
// No callback is executed and no generic/fake callback result can be supplied.
// Raw addresses, strings, opaque bytes and state history come entirely from the
// caller. There is no default name comparator, memory snapshot or zero-state
// assumption. This does not claim a whole bank enumerator or runtime load.
void Monstey_GadbBindSelectedCategory(
    Monstey_GadbBinderMemory memory, std::uint64_t selected_class,
    std::uint64_t category, Monstey_GadbBinderTrace &trace,
    std::uint64_t owned_step_budget = 1000000);
} // namespace Monstey
