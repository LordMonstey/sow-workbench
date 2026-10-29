#include "Monstey_Core.hpp"
#include "Monstey_NavCompactCooker.hpp"
#include "Monstey_NavCompactOptimizerCore.hpp"
#include "Monstey_NavFinalizeHierarchy.hpp"
#include "Monstey_StaticNavFaceBounds.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <stdexcept>
#include <xmmintrin.h>
namespace {
using namespace Monstey;
using H = Monstey_NavHierarchy;
using B = Monstey_NavigationAabb;
std::uint64_t cases = 0;
void Check(bool yes, const char *why) {
  if (!yes)
    throw std::runtime_error(why);
}
float Add(float a, float b) {
  volatile float r = a + b;
  return r;
}
float Sub(float a, float b) {
  volatile float r = a - b;
  return r;
}
float Mul(float a, float b) {
  volatile float r = a * b;
  return r;
}
float Div(float a, float b) {
  volatile float r = a / b;
  return r;
}
float Min(float a, float b) { return a < b ? a : b; }
float Max(float a, float b) { return a > b ? a : b; }
B Empty() {
  const auto q = std::bit_cast<float>(0x7f7fffeeu);
  return {{q, q, q, q}, {-q, -q, -q, -q}};
}
B Join(const B &a, const B &b) {
  B c{};
  for (unsigned n = 0; n < 4; ++n) {
    c.minimum[n] = Min(a.minimum[n], b.minimum[n]);
    c.maximum[n] = Max(a.maximum[n], b.maximum[n]);
  }
  return c;
}
float Area(const B &a) {
  std::array<float, 3> d{};
  for (unsigned n = 0; n < 3; ++n)
    d[n] = Sub(a.maximum[n], a.minimum[n]);
  return Add(Add(Mul(d[1], d[2]), Mul(d[0], d[1])), Mul(d[2], d[0]));
}
float Center(const B &a, unsigned axis) {
  return Mul(Add(a.maximum[axis], a.minimum[axis]), .5f);
}
void SortIds(std::vector<std::uint32_t> &ids, std::ptrdiff_t lo,
             std::ptrdiff_t hi, unsigned axis, const H &h) {
  if (lo >= hi)
    return;
  const float key = Center(
      h.nodes[ids[static_cast<std::size_t>((lo + hi) / 2)]].bounds, axis);
  auto i = lo, j = hi;
  while (i <= j) {
    while (Center(h.nodes[ids[static_cast<std::size_t>(i)]].bounds, axis) < key)
      ++i;
    while (Center(h.nodes[ids[static_cast<std::size_t>(j)]].bounds, axis) > key)
      --j;
    if (i <= j) {
      std::swap(ids[static_cast<std::size_t>(i)],
                ids[static_cast<std::size_t>(j)]);
      ++i;
      --j;
    }
  }
  if (lo < j)
    SortIds(ids, lo, j, axis, h);
  if (i < hi)
    SortIds(ids, i, hi, axis, h);
}
H Make(std::span<const B> boxes, bool skew = false) {
  H h;
  h.nodes.emplace_back();
  for (std::size_t n = 0; n < boxes.size(); ++n)
    h.nodes.push_back({boxes[n], 0, 0, static_cast<std::uint32_t>(n)});
  h.leaf_count = static_cast<std::uint32_t>(boxes.size());
  if (boxes.empty())
    return h;
  std::function<std::uint32_t(std::size_t, std::size_t)> build =
      [&](std::size_t lo, std::size_t hi) -> std::uint32_t {
    if (hi - lo == 1)
      return static_cast<std::uint32_t>(lo + 1);
    const auto middle = skew ? lo + 1 : (lo + (hi - lo) / 2);
    const auto a = build(lo, middle), b = build(middle, hi);
    const auto id = static_cast<std::uint32_t>(h.nodes.size());
    h.nodes.push_back({Join(h.nodes[a].bounds, h.nodes[b].bounds), 0, a, b});
    h.nodes[a].parent = id;
    h.nodes[b].parent = id;
    return id;
  };
  h.root = build(0, boxes.size());
  return h;
}
struct Oracle {
  H h;
  std::vector<std::uint32_t> free;
  std::uint32_t Alloc() {
    Check(!free.empty(), "Oracle free-list empty");
    auto id = free.back();
    free.pop_back();
    return id;
  }
  std::uint32_t Rebuild(std::vector<std::uint32_t> ids, bool initial) {
    if (ids.size() <= 16 && !initial) {
      while (ids.size() > 1) {
        float best = std::bit_cast<float>(0x7f7fffeeu);
        std::size_t ai = 0, bi = 0;
        B bounds{};
        bool found = false;
        for (std::size_t a = 0; a < ids.size(); ++a)
          for (std::size_t b = a + 1; b < ids.size(); ++b) {
            const auto u = Join(h.nodes[ids[b]].bounds, h.nodes[ids[a]].bounds);
            const auto score = Area(u);
            if (score < best) {
              found = true;
              best = score;
              ai = a;
              bi = b;
              bounds = u;
            }
          }
        Check(found, "Oracle no merge");
        const auto a = ids[ai], b = ids[bi], p = Alloc();
        h.nodes[p] = {bounds, 0, a, b};
        h.nodes[a].parent = p;
        h.nodes[b].parent = p;
        ids[bi] = ids.back();
        ids.pop_back();
        ids[ai] = p;
      }
      return ids[0];
    }
    B box = Empty();
    for (auto id : ids)
      box = Join(box, h.nodes[id].bounds);
    const auto p = Alloc();
    h.nodes[p].bounds = box;
    std::array<std::array<std::vector<std::uint32_t>, 32>, 3> groups;
    std::array<std::array<B, 32>, 3> bounds;
    for (auto &axis : bounds)
      axis.fill(Empty());
    for (auto id : ids)
      for (unsigned axis = 0; axis < 3; ++axis) {
        const auto extent = Sub(box.maximum[axis], box.minimum[axis]);
        const auto reciprocal = extent == 0 ? 0 : Div(1, extent);
        const auto scale = Mul(reciprocal, 32);
        auto value =
            Add(Mul(Sub(Center(h.nodes[id].bounds, axis), box.minimum[axis]),
                    scale),
                .5f);
        value = Min(Max(value, 0), 31);
        const auto at = static_cast<std::size_t>(value);
        groups[axis][at].push_back(id);
        bounds[axis][at] = Join(bounds[axis][at], h.nodes[id].bounds);
      }
    // Independent O(3*32^2) closed prefix/suffix expression, rather than
    // sharing the production two-ended update loop or its aliased cost
    // accumulators.
    float best = std::bit_cast<float>(0x7f7fffeeu);
    unsigned chosenAxis = 0, chosenBin = 0;
    bool found = false;
    for (unsigned axis = 0; axis < 3; ++axis)
      for (unsigned at = 0; at < 32; ++at) {
        if (groups[axis][at].empty())
          continue;
        B left = Empty(), right = Empty();
        std::uint32_t lc = 0, rc = 0;
        for (unsigned q = 0; q <= at; ++q) {
          left = Join(left, bounds[axis][q]);
          lc += static_cast<std::uint32_t>(groups[axis][q].size());
        }
        for (unsigned q = 32; q-- > at;) {
          right = Join(right, bounds[axis][q]);
          rc += static_cast<std::uint32_t>(groups[axis][q].size());
        }
        const auto score = Add(Mul(Area(left), static_cast<float>(lc)),
                               Mul(Area(right), static_cast<float>(rc)));
        if (!(score >= best)) {
          found = true;
          best = score;
          chosenAxis = axis;
          chosenBin = at;
        }
      }
    Check(found, "Oracle no bin");
    std::vector<std::uint32_t> left, right;
    for (unsigned q = 0; q < 32; ++q)
      if (q < chosenBin)
        left.insert(left.end(), groups[chosenAxis][q].begin(),
                    groups[chosenAxis][q].end());
      else if (q > chosenBin)
        right.insert(right.end(), groups[chosenAxis][q].begin(),
                     groups[chosenAxis][q].end());
    auto middle = groups[chosenAxis][chosenBin];
    if (left.empty() && !right.empty())
      left.insert(left.end(), middle.begin(), middle.end());
    else if (right.empty() && !left.empty())
      right.insert(right.end(), middle.begin(), middle.end());
    else {
      SortIds(middle, 0, static_cast<std::ptrdiff_t>(middle.size()) - 1,
              chosenAxis, h);
      const auto split =
          middle.begin() + static_cast<std::ptrdiff_t>(middle.size() / 2);
      left.insert(left.end(), middle.begin(), split);
      right.insert(right.end(), split, middle.end());
    }
    Check(!left.empty() && !right.empty(), "Oracle empty partition");
    const auto r = Rebuild(std::move(right), false);
    const auto l = Rebuild(std::move(left), false);
    h.nodes[p].child0 = l;
    h.nodes[p].child1_or_payload = r;
    h.nodes[l].parent = p;
    h.nodes[r].parent = p;
    return p;
  }
  H Run() {
    if (h.root == 0 || h.leaf_count <= 2)
      return h;
    std::vector<std::uint32_t> leaves;
    std::function<void(std::uint32_t)> visit = [&](auto id) {
      if (!h.nodes[id].child0) {
        leaves.push_back(id);
        return;
      }
      free.push_back(id);
      visit(h.nodes[id].child0);
      visit(h.nodes[id].child1_or_payload);
    };
    visit(h.root);
    h.root = Rebuild(std::move(leaves), true);
    h.nodes[h.root].parent = 0;
    Check(free.empty(), "Oracle unused free ID");
    return h;
  }
};
bool Equal(const H &a, const H &b) {
  if (a.root != b.root || a.leaf_count != b.leaf_count ||
      a.nodes.size() != b.nodes.size())
    return false;
  for (std::size_t n = 0; n < a.nodes.size(); ++n) {
    const auto &x = a.nodes[n];
    const auto &y = b.nodes[n];
    if (x.parent != y.parent || x.child0 != y.child0 ||
        x.child1_or_payload != y.child1_or_payload)
      return false;
    for (unsigned q = 0; q < 4; ++q)
      if (std::bit_cast<std::uint32_t>(x.bounds.minimum[q]) !=
              std::bit_cast<std::uint32_t>(y.bounds.minimum[q]) ||
          std::bit_cast<std::uint32_t>(x.bounds.maximum[q]) !=
              std::bit_cast<std::uint32_t>(y.bounds.maximum[q]))
        return false;
  }
  return true;
}
void Compare(const H &input) {
  const auto want = Oracle{input, {}}.Run();
  const auto got = Monstey_NavOptimizeHierarchyCore(input);
  Check(Equal(got, want), "Independent core topology/bounds/ID oracle differs");
  ++cases;
}
B Box(float x, float y, float z, float extent) {
  return {{x, y, z, 0}, {Add(x, extent), Add(y, extent), Add(z, extent), 0}};
}
} // namespace

namespace {
H Monstey_FinalOracle(H tree, bool enabled = true) {
  if (!tree.root || tree.leaf_count <= 2 || !enabled)
    return tree;
  bool changed;
  do {
    changed = false;
    std::function<void(std::uint32_t)> visit = [&](std::uint32_t id) {
      auto &n = tree.nodes[id];
      if (!n.child0)
        return;
      auto &a = tree.nodes[n.child0];
      auto &b = tree.nodes[n.child1_or_payload];
      if (a.child0 && b.child0) {
        const auto p = a.child0, q = a.child1_or_payload, r = b.child0,
                   s = b.child1_or_payload;
        const auto firstA = Join(tree.nodes[p].bounds, tree.nodes[r].bounds),
                   firstB = Join(tree.nodes[q].bounds, tree.nodes[s].bounds);
        const auto secondA = Join(tree.nodes[p].bounds, tree.nodes[s].bounds),
                   secondB = Join(tree.nodes[q].bounds, tree.nodes[r].bounds);
        const float c0 = Area(firstA) + Area(firstB),
                    c1 = Area(secondA) + Area(secondB),
                    old = Area(a.bounds) + Area(b.bounds);
        if (c1 > c0) {
          if (old > c0) {
            a.child1_or_payload = r;
            b.child0 = q;
            b.child1_or_payload = s;
            a.bounds = firstA;
            b.bounds = firstB;
            changed = true;
          }
        } else if (old > c1) {
          a.child1_or_payload = s;
          b.child0 = q;
          b.child1_or_payload = r;
          a.bounds = secondA;
          b.bounds = secondB;
          changed = true;
        }
        tree.nodes[a.child0].parent = n.child0;
        tree.nodes[a.child1_or_payload].parent = n.child0;
        tree.nodes[b.child0].parent = n.child1_or_payload;
        tree.nodes[b.child1_or_payload].parent = n.child1_or_payload;
      }
      visit(n.child0);
      visit(n.child1_or_payload);
    };
    const auto &r = tree.nodes[tree.root];
    visit(r.child0);
    visit(r.child1_or_payload);
  } while (changed);
  std::function<void(std::uint32_t)> order = [&](std::uint32_t id) {
    auto &n = tree.nodes[id];
    if (!n.child0)
      return;
    if (Area(tree.nodes[n.child1_or_payload].bounds) >
        Area(tree.nodes[n.child0].bounds))
      std::swap(n.child0, n.child1_or_payload);
    order(n.child0);
    order(n.child1_or_payload);
  };
  const auto &r = tree.nodes[tree.root];
  order(r.child0);
  order(r.child1_or_payload);
  return tree;
}

} // namespace

namespace Monstey {
Monstey_NavHierarchy
    Monstey_NavOptimizeHierarchyCoreBaseline(Monstey_NavHierarchy);
Monstey_NavHierarchy Monstey_NavFinalizeHierarchyBaseline(Monstey_NavHierarchy,
                                                          bool);
Monstey_StaticNavFaceAabb Monstey_StaticNavFaceBoundsBaseline(
    std::span<const std::uint8_t>, std::span<const std::uint8_t>,
    std::span<const std::uint8_t>, std::uint32_t, const std::array<float, 4> &);
} // namespace Monstey
namespace {
std::uint64_t Monstey_Count = 0, Monstey_Skips = 0, Monstey_Entered = 0,
              Monstey_Safe = 0, Monstey_UnsafeBin = 0, Monstey_UnsafePair = 0,
              Monstey_SafeMultiple = 0;
void Monstey_Need(bool ok, const char *text) {
  Check(ok, text);
  ++Monstey_Count;
}
bool Monstey_Throws(const std::function<void()> &fn) {
  try {
    fn();
  } catch (const std::exception &) {
    return true;
  }
  return false;
}
bool Monstey_ExactSentinel(const B &b) {
  for (unsigned a = 0; a < 4; ++a)
    if (std::bit_cast<std::uint32_t>(b.minimum[a]) != 0x7f7fffee ||
        std::bit_cast<std::uint32_t>(b.maximum[a]) != 0xff7fffee)
      return false;
  return true;
}
bool Monstey_EnteredReference(std::int32_t first, std::int16_t count) {
  std::int64_t end =
      static_cast<std::int64_t>(first) + static_cast<std::int64_t>(count);
  if (end > std::numeric_limits<std::int32_t>::max())
    end -= 4294967296ll;
  if (end < std::numeric_limits<std::int32_t>::min())
    end += 4294967296ll;
  return static_cast<std::int64_t>(first) < end;
}
void Monstey_Payloads(const Monstey_NavCompactTreeOwned &t, std::size_t faces) {
  Monstey_Need(t.nodes.size() == (faces ? faces * 2 - 1 : 0) * 6,
               "Unused face was removed from compact node count");
  std::vector<std::uint32_t> found;
  for (std::size_t i = 0; i < t.nodes.size(); i += 6)
    if ((t.nodes[i + 3] & 0x80) == 0)
      found.push_back(
          (static_cast<std::uint32_t>(t.nodes[i + 3] & 0x7f) << 16) |
          t.nodes[i + 4] | (static_cast<std::uint32_t>(t.nodes[i + 5]) << 8));
  std::sort(found.begin(), found.end());
  Monstey_Need(found.size() == faces, "Unused face payload disappeared");
  for (std::size_t i = 0; i < found.size(); ++i)
    Monstey_Need(found[i] == i, "Unused face payload was renumbered");
}
void Monstey_BoundsSame(const B &a, const B &b) {
  for (unsigned n = 0; n < 4; ++n)
    Monstey_Need(std::bit_cast<std::uint32_t>(a.minimum[n]) ==
                         std::bit_cast<std::uint32_t>(b.minimum[n]) &&
                     std::bit_cast<std::uint32_t>(a.maximum[n]) ==
                         std::bit_cast<std::uint32_t>(b.maximum[n]),
                 "Raw root bounds changed");
}
bool Monstey_CoreCase(const std::vector<B> &bounds) {
  const auto input = Monstey_NavInsertHierarchy(bounds);
  std::optional<H> expected;
  std::string failure;
  try {
    expected = Oracle{input, {}}.Run();
  } catch (const std::exception &e) {
    failure = e.what();
    Monstey_Need(failure == "Oracle no merge" || failure == "Oracle no bin",
                 "Unexpected reference failure");
  }
  if (!expected) {
    std::string actual;
    try {
      (void)Monstey_NavOptimizeHierarchyCore(input);
    } catch (const std::exception &e) {
      actual = e.what();
    }
    if (failure == "Oracle no bin") {
      Monstey_Need(actual.find("bin split") != std::string::npos,
                   "Native no-bin route was replaced by unrelated validation "
                   "or fabricated output");
      ++Monstey_UnsafeBin;
    } else {
      Monstey_Need(actual.find("pair merge") != std::string::npos,
                   "Native no-pair route was replaced by unrelated validation "
                   "or fabricated output");
      ++Monstey_UnsafePair;
    }
    return false;
  }
  const auto actual = Monstey_NavOptimizeHierarchyCore(input);
  Monstey_Need(Equal(actual, *expected),
               "Exact core hierarchy differs from sealed independent oracle");
  const auto finalExpected = Monstey_FinalOracle(*expected),
             finalActual = Monstey_NavFinalizeHierarchy(actual, true);
  Monstey_Need(Equal(finalExpected, finalActual),
               "Finalizer changes sentinel topology or raw bounds");
  const auto tree = Monstey_NavCookCompactBounds(bounds),
             packed = Monstey_NavPackPreorder(
                 Monstey_NavPreorderHierarchy(finalExpected));
  Monstey_Need(
      tree.nodes == packed.nodes,
      "Full compact output differs from independent core/finalizer references");
  Monstey_BoundsSame(tree.bounds, packed.bounds);
  Monstey_Payloads(tree, bounds.size());
  ++Monstey_Safe;
  const auto empties =
      std::count_if(bounds.begin(), bounds.end(), Monstey_ExactSentinel);
  if (empties >= 2)
    ++Monstey_SafeMultiple;
  return true;
}
struct Monstey_MeshBytes {
  Monstey_Bytes faces, edges, vertices;
};
Monstey_MeshBytes Monstey_MeshFromBounds(const std::vector<B> &bounds,
                                         std::int32_t ignoredFirst,
                                         std::int16_t ignoredCount) {
  Monstey_MeshBytes out;
  out.faces.resize(bounds.size() * 16);
  out.edges.resize(bounds.size() * 60);
  out.vertices.resize(bounds.size() * 48);
  for (std::size_t i = 0; i < bounds.size(); ++i) {
    if (Monstey_ExactSentinel(bounds[i])) {
      Monstey_PutU32(out.faces, i * 16,
                     std::bit_cast<std::uint32_t>(ignoredFirst));
      Monstey_PutU16(out.faces, i * 16 + 8,
                     std::bit_cast<std::uint16_t>(ignoredCount));
      continue;
    }
    Monstey_PutU32(out.faces, i * 16, static_cast<std::uint32_t>(i * 3));
    Monstey_PutU16(out.faces, i * 16 + 8, 3);
    const auto &b = bounds[i];
    const std::array<std::array<float, 4>, 3> points{
        b.minimum, b.maximum,
        std::array<float, 4>{b.minimum[0], b.maximum[1], b.maximum[2],
                             b.minimum[3]}};
    for (unsigned n = 0; n < 3; ++n) {
      Monstey_PutU32(out.edges, (i * 3 + n) * 20,
                     static_cast<std::uint32_t>(i * 3 + n));
      for (unsigned axis = 0; axis < 4; ++axis)
        Monstey_PutU32(out.vertices, (i * 3 + n) * 16 + axis * 4,
                       std::bit_cast<std::uint32_t>(points[n][axis]));
    }
  }
  return out;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    using namespace Monstey;
    Monstey_Need(argc == 2, "NEW_REPORT required");
    unsigned groups = 0;
    const auto empty = Empty();
    // Independent 64-bit mathematical wrap oracle covers EVERY signed16 count.
    const std::array<std::int32_t, 8> firsts{
        std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::min() + 32767,
        -32768,
        -1,
        0,
        1,
        std::numeric_limits<std::int32_t>::max() - 32767,
        std::numeric_limits<std::int32_t>::max()};
    Monstey_Bytes face(16);
    for (const auto first : firsts)
      for (std::uint32_t raw = 0; raw < 65536; ++raw) {
        const auto count =
            std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(raw));
        Monstey_PutU32(face, 0, std::bit_cast<std::uint32_t>(first));
        Monstey_PutU16(face, 8, static_cast<std::uint16_t>(raw));
        if (Monstey_EnteredReference(first, count)) {
          Monstey_Need(
              Monstey_Throws(
                  [&] { (void)Monstey_StaticNavFaceBounds(face, {}, {}, 0); }),
              "Entered out-of-owned-range loop was fabricated as unused");
          ++Monstey_Entered;
        } else {
          const auto b = Monstey_StaticNavFaceBounds(face, {}, {}, 0);
          Monstey_Need(b.vertex_iterations == 0 && b.first_edge == first &&
                           b.edge_count == count &&
                           Monstey_ExactSentinel({b.minimum, b.maximum}),
                       "Skipped native signed-end range did not retain exact "
                       "sentinel/no-read metadata");
          ++Monstey_Skips;
        }
      }
    Monstey_Need(Monstey_Skips + Monstey_Entered == 524288 &&
                     Monstey_Skips > 0 && Monstey_Entered > 0,
                 "Exhaustive native loop coverage incomplete");
    ++groups;
    Monstey_Bytes edges(32768 * 20), vertices(16);
    const std::array<float, 4> point{1, 2, 3, 4};
    for (unsigned n = 0; n < 4; ++n)
      Monstey_PutU32(vertices, n * 4, std::bit_cast<std::uint32_t>(point[n]));
    for (std::int16_t count : {std::int16_t{1}, std::int16_t{2},
                               std::int16_t{3}, std::int16_t{32767}})
      for (std::int32_t first : {0, 1}) {
        Monstey_PutU32(face, 0, std::bit_cast<std::uint32_t>(first));
        Monstey_PutU16(face, 8, std::bit_cast<std::uint16_t>(count));
        const auto b = Monstey_StaticNavFaceBounds(face, edges, vertices, 0);
        Monstey_Need(b.vertex_iterations == static_cast<std::uint32_t>(count) &&
                         b.minimum == point && b.maximum == point,
                     "Ordinary positive loop range regressed");
      }
    ++groups;
    const std::array<std::pair<std::int32_t, std::int16_t>, 4> skipped{
        {{std::numeric_limits<std::int32_t>::min(), std::int16_t{0}},
         {0, std::int16_t{-1}},
         {std::numeric_limits<std::int32_t>::max(), std::int16_t{-32768}},
         {std::numeric_limits<std::int32_t>::max(), std::int16_t{1}}}};
    for (std::size_t n = 1; n <= 2; ++n)
      for (const auto [first, count] : skipped) {
        const auto mesh =
            Monstey_MeshFromBounds(std::vector<B>(n, empty), first, count);
        const auto tree = Monstey_NavCookCompactMesh(mesh.faces, {}, {});
        Monstey_Payloads(tree, n);
        Monstey_Need(Monstey_ExactSentinel(tree.bounds),
                     "Unused singleton/pair root is not the native sentinel");
        if (n == 1)
          Monstey_Need(tree.nodes == Monstey_Bytes({255, 255, 255, 0, 0, 0}),
                       "Known unused singleton bytes differ");
        else
          Monstey_Need(tree.nodes ==
                           Monstey_Bytes({255, 255, 255, 128, 1, 0, 15, 15, 15,
                                          0, 0, 0, 15, 15, 15, 0, 1, 0}),
                       "Known unused pair bytes differ");
      }
    ++groups;
    const std::array<B, 3> valid{B{{0, 0, 0, 0}, {1, 0, 1, 0}},
                                 B{{-1, -1, -1, 0}, {1, 1, 1, 0}},
                                 B{{-11, 0, -11, 0}, {-10, 0, -10, 0}}};
    // BF7469 UCOMISS / SETA selects child0 for unordered descent costs.
    const auto nanInput = Monstey_NavInsertHierarchy(
        std::array{empty, empty, valid[1], valid[1]});
    const auto nanPreorder = Monstey_NavPreorderHierarchy(nanInput);
    std::vector<std::uint32_t> nanPayloads;
    for (std::size_t i = 1; i < nanPreorder.nodes.size(); ++i)
      if (!nanPreorder.nodes[i].child0)
        nanPayloads.push_back(nanPreorder.nodes[i].child1_or_payload);
    Monstey_Need(nanInput.root == 3 && nanInput.nodes[3].child0 == 5 &&
                     nanInput.nodes[5].child0 == 7 &&
                     nanPayloads == std::vector<std::uint32_t>({0, 3, 2, 1}),
                 "Native unordered descent must choose child0");
    ++groups;
    unsigned rawMixed = 0;
    for (unsigned n : {3u, 4u, 7u, 17u, 33u})
      for (const auto &box : valid)
        for (unsigned pos : {0u, n / 2, n - 1})
          for (const auto [first, count] : skipped) {
            std::vector<B> bounds(n, box);
            bounds[pos] = empty;
            Monstey_Need(
                Monstey_CoreCase(bounds),
                "One-unused native mixed profile was arbitrarily narrowed");
            const auto mesh = Monstey_MeshFromBounds(bounds, first, count);
            const auto got = Monstey_NavCookCompactMesh(mesh.faces, mesh.edges,
                                                        mesh.vertices),
                       want = Monstey_NavCookCompactBounds(bounds);
            Monstey_Need(got.nodes == want.nodes,
                         "Raw skipped face differs from exact sentinel input");
            Monstey_BoundsSame(got.bounds, want.bounds);
            ++rawMixed;
          }
    Monstey_Need(rawMixed == 180, "Raw mixed fixture coverage incomplete");
    ++groups;
    unsigned masks = 0;
    for (unsigned geometry = 0; geometry < 2; ++geometry)
      for (unsigned n = 3; n <= (geometry == 0 ? 7u : 9u); ++n)
        for (unsigned mask = 0; mask < (1u << n); ++mask) {
          std::vector<B> bounds(n, valid[geometry]);
          for (unsigned i = 0; i < n; ++i)
            if (mask & (1u << i))
              bounds[i] = empty;
          (void)Monstey_CoreCase(bounds);
          ++masks;
        }
    Monstey_Need(masks == 1264 && Monstey_SafeMultiple > 0 &&
                     Monstey_UnsafeBin > 0 && Monstey_UnsafePair > 0,
                 "Mixed sentinel safe/unsafe coverage incomplete");
    ++groups;
    std::mt19937 rng(0x3cd5a74u);
    unsigned random = 0;
    for (unsigned trial = 0; trial < 256; ++trial) {
      const unsigned n = 3 + rng() % 62;
      std::vector<B> bounds;
      for (unsigned i = 0; i < n; ++i)
        bounds.push_back(
            rng() % 3 == 0
                ? empty
                : Box(static_cast<float>(static_cast<int>(rng() % 65) - 32) *
                          .25f,
                      static_cast<float>(rng() % 17) * .25f,
                      static_cast<float>(rng() % 13) * .25f, .5f));
      (void)Monstey_CoreCase(bounds);
      ++random;
    }
    Monstey_Need(random == 256, "Random mixed fixture coverage incomplete");
    ++groups;
    for (unsigned mode = 0; mode < 4; ++mode) {
      auto bad = empty;
      if (mode == 0)
        bad.minimum[0] = std::nextafter(bad.minimum[0], 0.0f);
      if (mode == 1)
        bad.maximum[2] = std::nextafter(bad.maximum[2], 0.0f);
      if (mode == 2)
        bad.minimum[3] = bad.maximum[3] = 0;
      if (mode == 3)
        bad = {{2, 0, 2, 0}, {1, 0, 1, 0}};
      const auto h =
          Monstey_NavInsertHierarchy(std::array{bad, valid[0], valid[0]});
      Monstey_Need(
          Monstey_Throws([&] { (void)Monstey_NavOptimizeHierarchyCore(h); }),
          "Core allowed a nonexact inverted sentinel");
      Monstey_Need(
          Monstey_Throws([&] { (void)Monstey_NavFinalizeHierarchy(h, true); }),
          "Finalizer allowed a nonexact inverted sentinel");
    }
    ++groups;
    for (unsigned trial = 0; trial < 96; ++trial) {
      std::vector<B> bounds;
      for (unsigned i = 0; i < 3 + rng() % 35; ++i)
        bounds.push_back(Box(static_cast<float>(rng() % 17),
                             static_cast<float>(rng() % 7),
                             static_cast<float>(rng() % 11), 1));
      const auto input = Monstey_NavInsertHierarchy(bounds);
      const auto a = Monstey_NavOptimizeHierarchyCore(input),
                 b = Monstey_NavOptimizeHierarchyCoreBaseline(input);
      Monstey_Need(Equal(a, b) &&
                       Equal(Monstey_NavFinalizeHierarchy(a, true),
                             Monstey_NavFinalizeHierarchyBaseline(b, true)),
                   "Old ordinary native output changed");
    }
    Compare(Make(std::array{valid[0], valid[0], valid[0]}));
    ++groups;
    Monstey_PutU32(face, 0, 0x7fffffffu);
    Monstey_PutU16(face, 8, 1);
    Monstey_Need(Monstey_Throws([&] {
                   (void)Monstey_StaticNavFaceBoundsBaseline(face, {}, {}, 0,
                                                             {});
                 }),
                 "Original positive-overflow helper counterexample absent");
    Monstey_PutU32(face, 0, 0x80000000u);
    Monstey_PutU16(face, 8, 0xffffu);
    Monstey_Need(
        Monstey_StaticNavFaceBoundsBaseline(face, {}, {}, 0, {})
                    .vertex_iterations == 0 &&
            Monstey_Throws(
                [&] { (void)Monstey_StaticNavFaceBounds(face, {}, {}, 0); }),
        "Original negative-underflow false-empty counterexample absent");
    ++groups;
    Monstey_Need(groups == 10 && Monstey_Safe > 0 && Monstey_SafeMultiple > 0,
                 "Mandatory independent group coverage incomplete");
    Monstey_WriteNewText(
        argv[1],
        Monstey_Json::object(
            {{"status", "PASS"},
             {"groups", groups},
             {"assertions", Monstey_Count},
             {"wrapped_loop_cases", Monstey_Skips + Monstey_Entered},
             {"skipped_zero_read_cases", Monstey_Skips},
             {"entered_rejected_cases", Monstey_Entered},
             {"raw_mixed_profiles", rawMixed},
             {"enumerated_mixed_profiles", masks},
             {"random_mixed_profiles", random},
             {"safe_hierarchies", Monstey_Safe},
             {"safe_multiple_unused_hierarchies", Monstey_SafeMultiple},
             {"unsafe_no_bin", Monstey_UnsafeBin},
             {"unsafe_no_pair", Monstey_UnsafePair},
             {"ordinary_preimage_cases", 96},
             {"no_face_dropping_or_renumbering", true},
             {"game_or_original_function_executed", false}})
                .dump(2) +
            "\n");
    std::cout << "PASS " << groups << " independent unused-face groups; loop="
              << Monstey_Skips + Monstey_Entered << " mixed=" << masks
              << " safe=" << Monstey_Safe
              << " multiple=" << Monstey_SafeMultiple
              << " checks=" << Monstey_Count << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
