#include "Monstey_NavCompactCookerStages.hpp"
#include "Monstey_Core.hpp"
#include "Monstey_StaticNavFaceBounds.hpp"
#include <bit>
#include <climits>
#include <immintrin.h>
namespace Monstey {
namespace {
using B = Monstey_NavigationAabb;
using V = Monstey_NavigationVector4;
__m128 Monstey_CookerLoad(const V &v) { return _mm_loadu_ps(v.data()); }
V Monstey_CookerStore(__m128 v) {
  V out{};
  _mm_storeu_ps(out.data(), v);
  return out;
}
__m128 Monstey_CookerSum3(__m128 v) {
  return _mm_add_ps(
      _mm_add_ps(_mm_shuffle_ps(v, v, 0x55), _mm_shuffle_ps(v, v, 0)),
      _mm_shuffle_ps(v, v, 0xaa));
}
B Monstey_CookerUnion(const B &a, const B &b) {
  return {Monstey_CookerStore(_mm_min_ps(Monstey_CookerLoad(a.minimum),
                                         Monstey_CookerLoad(b.minimum))),
          Monstey_CookerStore(_mm_max_ps(Monstey_CookerLoad(a.maximum),
                                         Monstey_CookerLoad(b.maximum)))};
}
float Monstey_CookerDescentCost(const B &child, const B &added) {
  auto difference = _mm_sub_ps(_mm_add_ps(Monstey_CookerLoad(child.maximum),
                                          Monstey_CookerLoad(child.minimum)),
                               _mm_add_ps(Monstey_CookerLoad(added.maximum),
                                          Monstey_CookerLoad(added.minimum)));
  auto extent = _mm_add_ps(_mm_sub_ps(Monstey_CookerLoad(child.maximum),
                                      Monstey_CookerLoad(child.minimum)),
                           _mm_sub_ps(Monstey_CookerLoad(added.maximum),
                                      Monstey_CookerLoad(added.minimum)));
  difference = _mm_mul_ps(difference, difference);
  return _mm_cvtss_f32(
      _mm_mul_ps(Monstey_CookerSum3(difference), Monstey_CookerSum3(extent)));
}
void Monstey_CookerIndex(const Monstey_NavHierarchy &tree,
                         std::uint32_t index) {
  Monstey_Require(index > 0 && index < tree.nodes.size(),
                  "Native hierarchy node outside owned records");
}
} // namespace
B Monstey_NavCompactDecode(const B &parent,
                           const std::array<std::uint8_t, 4> &bytes) noexcept {
  const auto extent =
      _mm_mul_ps(_mm_sub_ps(Monstey_CookerLoad(parent.maximum),
                            Monstey_CookerLoad(parent.minimum)),
                 _mm_set1_ps(std::bit_cast<float>(std::uint32_t{0x3b90fdbc})));
  const auto upper = _mm_cvtepi32_ps(_mm_set_epi32(
      bytes[3] >> 4, bytes[2] >> 4, bytes[1] >> 4, bytes[0] >> 4));
  const auto lower = _mm_cvtepi32_ps(
      _mm_set_epi32(0, bytes[2] & 15, bytes[1] & 15, bytes[0] & 15));
  return {Monstey_CookerStore(
              _mm_add_ps(_mm_mul_ps(_mm_mul_ps(upper, upper), extent),
                         Monstey_CookerLoad(parent.minimum))),
          Monstey_CookerStore(
              _mm_sub_ps(Monstey_CookerLoad(parent.maximum),
                         _mm_mul_ps(_mm_mul_ps(lower, lower), extent)))};
}
std::array<std::uint8_t, 3>
Monstey_NavCompactQuantize(const B &parent, const B &child) noexcept {
  std::array<std::uint8_t, 4> bytes{};
  for (unsigned axis = 0; axis < 3; ++axis) {
    while ((bytes[axis] & 0xf0u) < 0xf0u) {
      const auto previous = bytes[axis];
      bytes[axis] = static_cast<std::uint8_t>(previous + 0x10u);
      const auto decoded = Monstey_NavCompactDecode(parent, bytes);
      if (!(child.minimum[axis] >= decoded.minimum[axis])) {
        bytes[axis] = previous;
        break;
      }
    }
    while ((bytes[axis] & 15u) < 15u) {
      const auto previous = bytes[axis];
      bytes[axis] = static_cast<std::uint8_t>(previous + 1u);
      const auto decoded = Monstey_NavCompactDecode(parent, bytes);
      if (child.maximum[axis] > decoded.maximum[axis]) {
        bytes[axis] = previous;
        break;
      }
    }
  }
  return {bytes[0], bytes[1], bytes[2]};
}
Monstey_NavHierarchy Monstey_NavInsertHierarchy(std::span<const B> bounds) {
  Monstey_Require(bounds.size() <= static_cast<std::size_t>((INT_MAX - 1) / 2),
                  "Native hierarchy node indices exceed signed grammar");
  Monstey_NavHierarchy tree;
  tree.nodes.reserve(bounds.size() * 2 + 1);
  tree.nodes.push_back({});
  for (std::size_t face = 0; face < bounds.size(); ++face) {
    const auto leaf = static_cast<std::uint32_t>(tree.nodes.size());
    tree.nodes.push_back(
        {bounds[face], 0, 0, static_cast<std::uint32_t>(face)});
    if (tree.root == 0) {
      tree.root = leaf;
    } else {
      const auto parent = static_cast<std::uint32_t>(tree.nodes.size());
      tree.nodes.push_back({});
      auto at = tree.root;
      while (tree.nodes[at].child0 != 0) {
        tree.nodes[at].bounds =
            Monstey_CookerUnion(tree.nodes[at].bounds, bounds[face]);
        const auto left = tree.nodes[at].child0,
                   right = tree.nodes[at].child1_or_payload;
        Monstey_CookerIndex(tree, left);
        Monstey_CookerIndex(tree, right);
        const float cost0 = Monstey_CookerDescentCost(tree.nodes[left].bounds,
                                                      bounds[face]),
                    cost1 = Monstey_CookerDescentCost(tree.nodes[right].bounds,
                                                      bounds[face]);
        at = cost0 > cost1 ? right : left;
      }
      const auto oldParent = tree.nodes[at].parent;
      if (oldParent == 0)
        tree.root = parent;
      else {
        Monstey_CookerIndex(tree, oldParent);
        if (tree.nodes[oldParent].child1_or_payload == at)
          tree.nodes[oldParent].child1_or_payload = parent;
        else
          tree.nodes[oldParent].child0 = parent;
      }
      tree.nodes[parent] = {
          Monstey_CookerUnion(tree.nodes[at].bounds, bounds[face]), oldParent,
          at, leaf};
      tree.nodes[at].parent = parent;
      tree.nodes[leaf].parent = parent;
    }
    ++tree.leaf_count;
  }
  return tree;
}
Monstey_NavHierarchy
Monstey_NavInsertFaceHierarchy(std::span<const std::uint8_t> faces,
                               std::span<const std::uint8_t> edges,
                               std::span<const std::uint8_t> vertices) {
  Monstey_Require(faces.size() % 16 == 0 &&
                      faces.size() / 16 <=
                          static_cast<std::size_t>((INT_MAX - 1) / 2),
                  "Native face hierarchy count/stride outside owned grammar");
  std::vector<B> bounds;
  bounds.reserve(faces.size() / 16);
  for (std::size_t i = 0; i < faces.size() / 16; ++i) {
    const auto box = Monstey_StaticNavFaceBounds(faces, edges, vertices,
                                                 static_cast<std::uint32_t>(i));
    bounds.push_back({box.minimum, box.maximum});
  }
  return Monstey_NavInsertHierarchy(bounds);
}
Monstey_NavHierarchy
Monstey_NavPreorderHierarchy(const Monstey_NavHierarchy &source) {
  Monstey_NavHierarchy out;
  out.nodes.push_back({});
  out.leaf_count = source.leaf_count;
  if (source.root == 0) {
    Monstey_Require(source.leaf_count == 0,
                    "Empty native root contradicts leaf count");
    return out;
  }
  struct Work {
    std::uint32_t source, parent;
    bool right;
  };
  std::vector<Work> stack{{source.root, 0, false}};
  std::vector<bool> seen(source.nodes.size());
  std::uint32_t leaves = 0;
  while (!stack.empty()) {
    const auto work = stack.back();
    stack.pop_back();
    Monstey_CookerIndex(source, work.source);
    Monstey_Require(!seen[work.source],
                    "Native hierarchy contains duplicate/cyclic child");
    seen[work.source] = true;
    const auto &old = source.nodes[work.source];
    const auto index = static_cast<std::uint32_t>(out.nodes.size());
    out.nodes.push_back({old.bounds, work.parent, 0, old.child1_or_payload});
    if (work.parent) {
      if (work.right)
        out.nodes[work.parent].child1_or_payload = index;
      else
        out.nodes[work.parent].child0 = index;
    } else
      out.root = index;
    if (old.child0) {
      Monstey_CookerIndex(source, old.child1_or_payload);
      stack.push_back({old.child1_or_payload, index, true});
      stack.push_back({old.child0, index, false});
    } else
      ++leaves;
  }
  Monstey_Require(leaves == source.leaf_count &&
                      out.nodes.size() == static_cast<std::size_t>(leaves) * 2,
                  "Native hierarchy leaf/node metadata is inconsistent");
  return out;
}
Monstey_NavCompactTreeOwned
Monstey_NavPackPreorder(const Monstey_NavHierarchy &tree) {
  Monstey_NavCompactTreeOwned out;
  // BACF29..BACF5C returns before reading the root when the leaf count is zero.
  if (tree.leaf_count == 0) {
    const float high = std::bit_cast<float>(std::uint32_t{0x7f7fffee});
    const float low = std::bit_cast<float>(std::uint32_t{0xff7fffee});
    out.bounds = {{high, high, high, high}, {low, low, low, low}};
    return out;
  }
  Monstey_Require(
      tree.root == 1 &&
          tree.nodes.size() == static_cast<std::size_t>(tree.leaf_count) * 2,
      "Native compact packing requires a nonempty complete preorder tree");
  out.bounds = tree.nodes[1].bounds;
  out.nodes.resize((tree.nodes.size() - 1) * 6);
  std::vector<B> decoded(tree.nodes.size());
  decoded[0] = out.bounds;
  for (std::size_t i = 1; i < tree.nodes.size(); ++i) {
    const auto &node = tree.nodes[i];
    Monstey_Require(node.parent < i,
                    "Native compact preorder parent does not precede child");
    const auto encoded =
        Monstey_NavCompactQuantize(decoded[node.parent], node.bounds);
    auto *target = out.nodes.data() + (i - 1) * 6;
    for (unsigned axis = 0; axis < 3; ++axis)
      target[axis] = encoded[axis];
    std::uint32_t payload = node.child1_or_payload;
    std::uint8_t tag = 0;
    if (node.child0) {
      Monstey_Require(node.child0 == i + 1 && payload > i + 1 &&
                          payload < tree.nodes.size() &&
                          ((payload - i) & 1u) == 0,
                      "Native compact binary preorder child layout differs");
      payload = (payload - static_cast<std::uint32_t>(i)) / 2;
      tag = 0x80;
    }
    target[3] = static_cast<std::uint8_t>(((payload >> 16) & 0x7fu) | tag);
    target[4] = static_cast<std::uint8_t>(payload);
    target[5] = static_cast<std::uint8_t>(payload >> 8);
    decoded[i] = Monstey_NavCompactDecode(
        decoded[node.parent], {target[0], target[1], target[2], target[3]});
  }
  return out;
}
} // namespace Monstey
