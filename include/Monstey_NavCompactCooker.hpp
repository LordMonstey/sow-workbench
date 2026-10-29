#pragma once
#include "Monstey_NavCompactCookerStages.hpp"
namespace Monstey {
// Successful owned-input 47AB10/BF7FD0/BACF10 constructor profile.
// Uses native bins32, bottom-up threshold16 and finalization flag1.
// Input records: faces16, edges20, vertices16, in native local units.
// Returns owned six-byte records and root bounds for the compact-tree consumer.
// Ordinary bounds require finite ordered XYZ; exact four-lane native unused-face
// sentinels are also accepted when the native partition path succeeds. No faces
// are dropped. Native no-candidate/no-pair branches remain diagnosed errors.
// Empty meshes produce zero nodes and exact native sentinel root bounds.
Monstey_NavCompactTreeOwned Monstey_NavCookCompactMesh(
    std::span<const std::uint8_t> faces,std::span<const std::uint8_t> edges,
    std::span<const std::uint8_t> vertices);
Monstey_NavCompactTreeOwned Monstey_NavCookCompactBounds(
    std::span<const Monstey_NavigationAabb> bounds);
}
