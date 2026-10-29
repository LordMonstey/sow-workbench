#include "Monstey_NavFinalizeHierarchy.hpp"
#include "Monstey_Core.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <immintrin.h>
#include <utility>
namespace Monstey {
namespace {
using H = Monstey_NavHierarchy;
using B = Monstey_NavigationAabb;
using V = Monstey_NavigationVector4;
__m128 Monstey_Load(const V &value) { return _mm_loadu_ps(value.data()); }
V Monstey_Store(__m128 value) {
  V result{};
  _mm_storeu_ps(result.data(), value);
  return result;
}
B Monstey_Union(const B &left, const B &right) {
  return {Monstey_Store(_mm_min_ps(Monstey_Load(left.minimum),
                                   Monstey_Load(right.minimum))),
          Monstey_Store(_mm_max_ps(Monstey_Load(left.maximum),
                                   Monstey_Load(right.maximum)))};
}
float Monstey_Area(const B &box) {
  const auto d =
      _mm_sub_ps(Monstey_Load(box.maximum), Monstey_Load(box.minimum));
  const auto p = _mm_mul_ps(_mm_shuffle_ps(d, d, 0xc9), d);
  return _mm_cvtss_f32(_mm_add_ps(
      _mm_add_ps(_mm_shuffle_ps(p, p, 0x55), _mm_shuffle_ps(p, p, 0)),
      _mm_shuffle_ps(p, p, 0xaa)));
}
float Monstey_Add(float left, float right) {
  return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(left), _mm_set_ss(right)));
}
bool Monstey_ExactUnusedBounds(const B& bounds) noexcept {
  for (unsigned axis = 0; axis < 4; ++axis)
    if (std::bit_cast<std::uint32_t>(bounds.minimum[axis]) != 0x7f7fffeeu ||
        std::bit_cast<std::uint32_t>(bounds.maximum[axis]) != 0xff7fffeeu)
      return false;
  return true;
}
void Monstey_Validate(const H &tree) {
  Monstey_Require(
      tree.root < tree.nodes.size() && tree.nodes[tree.root].parent == 0,
      "Native finalizer requires an owned whole root with parent zero");
  std::vector<bool> seen(tree.nodes.size());
  std::vector<std::uint32_t> pending{tree.root};
  std::size_t leaves = 0, internal = 0;
  while (!pending.empty()) {
    const auto id = pending.back();
    pending.pop_back();
    Monstey_Require(id > 0 && id < tree.nodes.size() && !seen[id],
                    "Native finalizer child outside owned tree or cyclic");
    seen[id] = true;
    const auto &node = tree.nodes[id];
    if (!Monstey_ExactUnusedBounds(node.bounds))
    for (unsigned axis = 0; axis < 3; ++axis)
      Monstey_Require(std::isfinite(node.bounds.minimum[axis]) &&
                          std::isfinite(node.bounds.maximum[axis]) &&
                          node.bounds.minimum[axis] <=
                              node.bounds.maximum[axis],
                      "Native finalizer requires finite ordered XYZ bounds");
    if (node.child0) {
      ++internal;
      for (const auto child : {node.child0, node.child1_or_payload}) {
        Monstey_Require(child > 0 && child < tree.nodes.size() &&
                            tree.nodes[child].parent == id,
                        "Native finalizer child-parent identity mismatch");
        pending.push_back(child);
      }
    } else
      ++leaves;
  }
  Monstey_Require(leaves == tree.leaf_count && internal + 1 == leaves,
                  "Native finalizer complete hierarchy count differs");
}
std::uint32_t Monstey_Next(const H &tree, std::uint32_t id) {
  if (tree.nodes[id].child0)
    return tree.nodes[id].child0;
  auto previous = id, parent = tree.nodes[id].parent;
  while (parent != tree.root &&
         tree.nodes[parent].child1_or_payload == previous) {
    previous = parent;
    parent = tree.nodes[parent].parent;
  }
  if (parent == tree.root && tree.nodes[parent].child1_or_payload == previous)
    return 0;
  return tree.nodes[parent].child1_or_payload;
}
bool Monstey_Rotate(H &tree, std::uint32_t id) {
  const auto &node = tree.nodes[id];
  if (!node.child0)
    return false;
  const auto left = node.child0, right = node.child1_or_payload;
  auto &a = tree.nodes[left];
  auto &b = tree.nodes[right];
  if (!a.child0 || !b.child0)
    return false;
  const std::array<std::uint32_t, 4> children{a.child0, a.child1_or_payload,
                                              b.child0, b.child1_or_payload};
  const auto &a0 = tree.nodes[children[0]].bounds;
  const auto &a1 = tree.nodes[children[1]].bounds;
  const auto &b0 = tree.nodes[children[2]].bounds;
  const auto &b1 = tree.nodes[children[3]].bounds;
  const std::array<B, 4> bounds{Monstey_Union(a0, b0), Monstey_Union(a1, b1),
                                Monstey_Union(a0, b1), Monstey_Union(a1, b0)};
  const auto oldCost =
      Monstey_Add(Monstey_Area(a.bounds), Monstey_Area(b.bounds));
  const auto firstCost =
      Monstey_Add(Monstey_Area(bounds[0]), Monstey_Area(bounds[1]));
  const auto secondCost =
      Monstey_Add(Monstey_Area(bounds[2]), Monstey_Area(bounds[3]));
  // A76CC8..CEA: equal alternatives select candidate1; only strict improvement
  // rotates.
  const unsigned choice = secondCost > firstCost ? 0u : 1u;
  if (!(oldCost > (choice == 0 ? firstCost : secondCost)))
    return false;
  // Data141E4AFF0: exact native [2,1,3] / [3,1,2] child permutation.
  constexpr std::array<std::array<unsigned, 3>, 2> permutation{
      {{{2, 1, 3}}, {{3, 1, 2}}}};
  a.child1_or_payload = children[permutation[choice][0]];
  b.child0 = children[permutation[choice][1]];
  b.child1_or_payload = children[permutation[choice][2]];
  tree.nodes[a.child1_or_payload].parent = left;
  tree.nodes[b.child0].parent = right;
  tree.nodes[b.child1_or_payload].parent = right;
  a.bounds = bounds[choice * 2];
  b.bounds = bounds[choice * 2 + 1];
  return true;
}
} // namespace
H Monstey_NavFinalizeHierarchy(H tree, bool native_finalize) {
  if (tree.root == 0 || tree.leaf_count <= 2)
    return tree;
  Monstey_Validate(tree);
  if (native_finalize) {
    bool changed;
    do {
      changed = false;
      for (auto id = tree.nodes[tree.root].child0; id != 0;
           id = Monstey_Next(tree, id))
        changed = Monstey_Rotate(tree, id) || changed;
    } while (changed);
    for (auto id = tree.nodes[tree.root].child0; id != 0;
         id = Monstey_Next(tree, id)) {
      auto &node = tree.nodes[id];
      if (node.child0 &&
          Monstey_Area(tree.nodes[node.child1_or_payload].bounds) >
              Monstey_Area(tree.nodes[node.child0].bounds))
        std::swap(node.child0, node.child1_or_payload);
    }
  }
  // A76FC8..A77056: selected original parent is zero, so publish this same new
  // root. The unselected subroot parent-slot replacement / ancestor-refit route
  // is not claimed.
  tree.nodes[tree.root].parent = 0;
  return tree;
}
} // namespace Monstey
