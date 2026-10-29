#pragma once
#include "Monstey_NavigationBounds.hpp"
#include <cstdint>
#include <span>
#include <vector>
namespace Monstey {
struct Monstey_NavHierarchyNode {
  Monstey_NavigationAabb bounds;
  std::uint32_t parent=0;
  std::uint32_t child0=0;
  std::uint32_t child1_or_payload=0;
};
struct Monstey_NavHierarchy {
  std::vector<Monstey_NavHierarchyNode> nodes; // Reserved index0, native node identities.
  std::uint32_t root=0;
  std::uint32_t leaf_count=0;
};
struct Monstey_NavCompactTreeOwned {
  std::vector<std::uint8_t> nodes; // Six-byte native compact records.
  Monstey_NavigationAabb bounds;
};
// Complete BFDC90 conservative nibble search; does not assume containment.
// Only XYZ bytes are encoded. The fourth node byte is tag/payload, not geometry.
std::array<std::uint8_t,3> Monstey_NavCompactQuantize(
    const Monstey_NavigationAabb& parent,const Monstey_NavigationAabb& child) noexcept;
// Complete BFDDA0 decode for an explicit fourth tag/payload byte.
Monstey_NavigationAabb Monstey_NavCompactDecode(
    const Monstey_NavigationAabb& parent,const std::array<std::uint8_t,4>& bytes) noexcept;
// Complete successful owned-input BF7280 insertion stage, before A74AB0 optimization.
Monstey_NavHierarchy Monstey_NavInsertHierarchy(
    std::span<const Monstey_NavigationAabb> face_bounds);
Monstey_NavHierarchy Monstey_NavInsertFaceHierarchy(
    std::span<const std::uint8_t> faces,std::span<const std::uint8_t> edges,
    std::span<const std::uint8_t> vertices);
// Complete A72860 valid-tree left-first preorder reindexing stage.
Monstey_NavHierarchy Monstey_NavPreorderHierarchy(const Monstey_NavHierarchy& source);
// Complete BACF10 compact packing after the native optimizer/preorder stages.
// Empty packing returns the exact native sentinel bounds and zero nodes.
// This function does NOT run A74AB0.
Monstey_NavCompactTreeOwned Monstey_NavPackPreorder(const Monstey_NavHierarchy& preorder);
}
