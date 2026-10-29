#pragma once
#include "Monstey_NavCompactCookerStages.hpp"
namespace Monstey {
// Selected A74AB0 successful whole-root profile, A76A16..A77056.
// Native flag false skips BOTH local rotations and area-order normalization.
// The passes traverse descendants of the new root, excluding the root itself.
// Node identities, leaf payloads and the whole-root parent=0 contract are
// retained.
Monstey_NavHierarchy Monstey_NavFinalizeHierarchy(Monstey_NavHierarchy tree,
                                                  bool native_finalize = true);
} // namespace Monstey
