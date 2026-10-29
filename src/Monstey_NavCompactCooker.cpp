#include "Monstey_NavCompactCooker.hpp"
#include "Monstey_NavCompactOptimizerCore.hpp"
#include "Monstey_NavFinalizeHierarchy.hpp"
#include <utility>
namespace Monstey {
namespace {
Monstey_NavCompactTreeOwned Monstey_FinishOwnedCook(Monstey_NavHierarchy hierarchy){
  // A74B2A..A74B4C return before finalization when root0 or <=2 leaves.
  if(hierarchy.leaf_count>2&&hierarchy.root!=0){
    hierarchy=Monstey_NavOptimizeHierarchyCore(std::move(hierarchy));
    hierarchy=Monstey_NavFinalizeHierarchy(std::move(hierarchy),true);
  }
  return Monstey_NavPackPreorder(Monstey_NavPreorderHierarchy(hierarchy));
}
}
Monstey_NavCompactTreeOwned Monstey_NavCookCompactMesh(
    std::span<const std::uint8_t> faces,std::span<const std::uint8_t> edges,
    std::span<const std::uint8_t> vertices){
  return Monstey_FinishOwnedCook(Monstey_NavInsertFaceHierarchy(faces,edges,vertices));
}
Monstey_NavCompactTreeOwned Monstey_NavCookCompactBounds(
    std::span<const Monstey_NavigationAabb> bounds){
  return Monstey_FinishOwnedCook(Monstey_NavInsertHierarchy(bounds));
}
}
