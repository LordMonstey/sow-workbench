#pragma once
#include "Monstey_Core.hpp"
namespace Monstey {
// Bounded projection of one exact class identity from the frozen explicit-
// pointer report. Parents remain pointer links; their fields are separate
// calls. Complete field rows are paged by serialized bytes, never truncated
// silently.
Monstey_Json Monstey_GadbNativeClassPage(const Monstey_Json &report,
                                         const Monstey_Json &arguments);
// Read-only file adapter: exact supported identity, before/after hash checks,
// explicit class pointer, then bounded field-page projection. No editor state.
Monstey_Json Monstey_GadbNativeClassInspect(const Monstey_Json &arguments);
} // namespace Monstey
