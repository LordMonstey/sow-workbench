#pragma once
#include "Monstey_NavCompactCookerStages.hpp"
namespace Monstey {
// Selected BF7FD0 profile: A74AB0 bins32, bottom-up threshold16.
// Accepts finite ordered XYZ or exact four-lane native unused-face sentinels.
// Native no-candidate/no-pair branches remain errors. Does not run final rotations.
// Output retains native free-list node identity and the selected new root.
Monstey_NavHierarchy Monstey_NavOptimizeHierarchyCore(Monstey_NavHierarchy source);
}
