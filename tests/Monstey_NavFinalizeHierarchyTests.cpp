#include "Monstey_Core.hpp"
#include "Monstey_Havok.hpp"
#include "Monstey_NavCompactOptimizerCore.hpp"
#include "Monstey_NavFinalizeHierarchy.hpp"
#include "Monstey_NavigationBoundaryQuery.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <functional>
#include <immintrin.h>
#include <iostream>
#include <random>
using namespace Monstey;
namespace {
using H = Monstey_NavHierarchy;
using B = Monstey_NavigationAabb;
unsigned groups = 0, rejections = 0;
std::uint64_t trees = 0, queries = 0, rows = 0;
void Check(bool yes, const char *why) { Monstey_Require(yes, why); }
B Box(float x, float y = 0, float width = 1, float w = 0) {
  return {{x, y, 0, w}, {x + width, y + 1, 1, w + 3}};
}
B Join(const B &a, const B &b) {
  B r{};
  for (unsigned i = 0; i < 4; ++i) {
    r.minimum[i] = a.minimum[i] < b.minimum[i] ? a.minimum[i] : b.minimum[i];
    r.maximum[i] = a.maximum[i] > b.maximum[i] ? a.maximum[i] : b.maximum[i];
  }
  return r;
}
float Area(const B &b) {
  const float x = b.maximum[0] - b.minimum[0], y = b.maximum[1] - b.minimum[1],
              z = b.maximum[2] - b.minimum[2];
  const float xy = y * x, yz = z * y, zx = x * z;
  const float first = yz + xy;
  return first + zx;
}
std::uint32_t Leaf(H &t, B b, std::uint32_t payload) {
  t.nodes.push_back({b, 0, 0, payload});
  ++t.leaf_count;
  return static_cast<std::uint32_t>(t.nodes.size() - 1);
}
std::uint32_t Branch(H &t, std::uint32_t a, std::uint32_t b) {
  const auto id = static_cast<std::uint32_t>(t.nodes.size());
  t.nodes.push_back({Join(t.nodes[a].bounds, t.nodes[b].bounds), 0, a, b});
  t.nodes[a].parent = id;
  t.nodes[b].parent = id;
  return id;
}
H Fixture(unsigned variant) {
  H t;
  t.nodes.resize(1);
  Leaf(t, Box(0), 0);
  Leaf(t, Box(100), 1);
  Leaf(t, Box(variant == 0 ? 1.0f : 101.0f), 2);
  Leaf(t, Box(variant == 0 ? 101.0f : 1.0f), 3);
  Leaf(t, Box(300), 4);
  Branch(t, 1, 2);
  Branch(t, 3, 4);
  Branch(t, 6, 7);
  t.root = Branch(t, 5, 8);
  return t;
}
void Same(const H &a, const H &b) {
  Check(a.root == b.root && a.leaf_count == b.leaf_count &&
            a.nodes.size() == b.nodes.size(),
        "Whole hierarchy identity differs");
  for (std::size_t n = 0; n < a.nodes.size(); ++n) {
    const auto &x = a.nodes[n];
    const auto &y = b.nodes[n];
    Check(x.parent == y.parent && x.child0 == y.child0 &&
              x.child1_or_payload == y.child1_or_payload,
          "Native topology or parent identity differs from independent "
          "reference");
    for (unsigned i = 0; i < 4; ++i) {
      Check(std::bit_cast<std::uint32_t>(x.bounds.minimum[i]) ==
                    std::bit_cast<std::uint32_t>(y.bounds.minimum[i]) &&
                std::bit_cast<std::uint32_t>(x.bounds.maximum[i]) ==
                    std::bit_cast<std::uint32_t>(y.bounds.maximum[i]),
            "All-lane bitwise bounds differ from independent reference");
      ++rows;
    }
  }
}
// Independent recursive walk, scalar component bounding and explicit
// alternatives.
H Reference(H tree, bool enabled = true) {
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
void Validate(const H &t) {
  const auto p = Monstey_NavPreorderHierarchy(t);
  std::vector<std::uint32_t> payloads;
  for (std::size_t n = 1; n < p.nodes.size(); ++n) {
    const auto &x = p.nodes[n];
    if (!x.child0)
      payloads.push_back(x.child1_or_payload);
    else
      for (const auto id : {x.child0, x.child1_or_payload}) {
        const auto &c = p.nodes[id];
        Check(c.parent == n, "Final tree parent mismatch");
        for (unsigned a = 0; a < 3; ++a)
          Check(c.bounds.minimum[a] >= x.bounds.minimum[a] &&
                    c.bounds.maximum[a] <= x.bounds.maximum[a],
                "Final child escaped native bounds");
      }
  }
  std::sort(payloads.begin(), payloads.end());
  Check(payloads.size() == t.leaf_count, "Final payload count");
  for (std::size_t n = 0; n < payloads.size(); ++n)
    Check(payloads[n] == n, "Final payload lost or duplicated");
}
bool Intersects(const B &a, const B &b) {
  for (unsigned n = 0; n < 3; ++n)
    if (a.minimum[n] > b.maximum[n] || b.minimum[n] > a.maximum[n])
      return false;
  return true;
}
void Cover(const H &t, const std::vector<B> &boxes, std::mt19937 &rng,
           unsigned count) {
  const auto p = Monstey_NavPackPreorder(Monstey_NavPreorderHierarchy(t));
  std::uniform_real_distribution<float> value(-125, 125), extent(.1f, 30);
  for (unsigned n = 0; n < count; ++n) {
    B q{};
    for (unsigned a = 0; a < 3; ++a) {
      q.minimum[a] = value(rng);
      q.maximum[a] = q.minimum[a] + extent(rng);
    }
    const auto hits =
        Monstey_NavigationCompactTreeQuery({p.nodes, p.bounds}, q);
    std::vector<bool> seen(boxes.size());
    for (const auto id : hits) {
      Check(id < boxes.size() && !seen[id],
            "Final compact query payload invalid or repeated");
      seen[id] = true;
    }
    for (std::size_t id = 0; id < boxes.size(); ++id)
      if (Intersects(q, boxes[id]))
        Check(seen[id], "Final compact query omitted actual intersection");
    ++queries;
  }
}
Monstey_Bytes Records(Monstey_Havok &g, std::size_t mesh,
                      std::string_view field, std::size_t stride) {
  Monstey_Bytes out;
  for (const auto id : g.Node(g.Field(mesh, field)).elements) {
    const auto &bytes = g.Node(id).original_storage;
    Check(bytes.size() == stride, "Actual record stride");
    out.insert(out.end(), bytes.begin(), bytes.end());
  }
  return out;
}
void Bad(const H &t) {
  bool rejected = false;
  try {
    (void)Monstey_NavFinalizeHierarchy(t);
  } catch (const std::exception &) {
    rejected = true;
  }
  Check(rejected, "Invalid owned hierarchy was accepted");
  ++rejections;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Check(argc == 2 || argc == 3, "NEW_REPORT [SOURCE_HKAI]");
    for (unsigned choice = 0; choice < 2; ++choice) {
      const auto before = Fixture(choice);
      const auto result = Monstey_NavFinalizeHierarchy(before);
      Same(result, Reference(before));
      Check(result.nodes[6].child1_or_payload == (choice == 0 ? 3u : 4u) &&
                result.nodes[7].child0 == 2 &&
                result.nodes[7].child1_or_payload == (choice == 0 ? 4u : 3u),
            "Native alternative permutation differs");
      Check(result.nodes[9].child0 == 5 &&
                result.nodes[9].child1_or_payload == 8,
            "Root wrongly included in ordering");
      Validate(result);
      ++trees;
    }
    ++groups;
    {
      auto t = Fixture(0);
      t.nodes[1].bounds = Box(0, 0);
      t.nodes[2].bounds = Box(10, 10);
      t.nodes[3].bounds = Box(0, 10);
      t.nodes[4].bounds = Box(10, 0);
      for (unsigned id = 6; id <= 9; ++id)
        t.nodes[id].bounds =
            Join(t.nodes[t.nodes[id].child0].bounds,
                 t.nodes[t.nodes[id].child1_or_payload].bounds);
      const auto r = Monstey_NavFinalizeHierarchy(t);
      Same(r, Reference(t));
      Check(r.nodes[6].child1_or_payload == 4 && r.nodes[7].child0 == 2 &&
                r.nodes[7].child1_or_payload == 3,
            "Equal-cost alternatives must select second");
      ++trees;
    }
    ++groups;
    {
      const auto t = Fixture(0);
      Same(t, Monstey_NavFinalizeHierarchy(t, false));
      ++trees;
    }
    ++groups;
    {
      H t;
      t.nodes.resize(1);
      Leaf(t, Box(0), 0);
      Leaf(t, Box(3, 0, 4), 1);
      Leaf(t, Box(20), 2);
      Branch(t, 1, 2);
      t.root = Branch(t, 3, 4);
      const auto r = Monstey_NavFinalizeHierarchy(t);
      Same(r, Reference(t));
      Check(r.nodes[4].child0 == 2 && r.nodes[4].child1_or_payload == 1,
            "Descendant area normalization missing");
      Check(r.nodes[5].child0 == 3 && r.nodes[5].child1_or_payload == 4,
            "Root area order must not be normalized");
      Same(t, Monstey_NavFinalizeHierarchy(t, false));
      ++trees;
    }
    ++groups;
    {
      const B box = Box(1);
      const std::array<B, 3> boxes{box, box, box};
      const auto core =
          Monstey_NavOptimizeHierarchyCore(Monstey_NavInsertHierarchy(boxes));
      Same(core, Monstey_NavFinalizeHierarchy(core));
      Check(core.root == 5 && core.nodes[5].child0 == 2 &&
                core.nodes[5].child1_or_payload == 3 &&
                core.nodes[3].child0 == 4 &&
                core.nodes[3].child1_or_payload == 1,
            "Native equal-three tie recipe differs");
      ++trees;
    }
    ++groups;
    {
      for (unsigned count = 0; count <= 2; ++count) {
        std::vector<B> boxes(count, Box(0));
        const auto t = Monstey_NavInsertHierarchy(boxes);
        Same(t, Monstey_NavFinalizeHierarchy(t));
        ++trees;
      }
    }
    ++groups;
    {
      auto t = Fixture(0);
      t.nodes[9].parent = 8;
      Bad(t);
      t = Fixture(0);
      t.nodes[8].child0 = 4000;
      Bad(t);
      t = Fixture(0);
      t.nodes[6].child0 = 8;
      Bad(t);
      t = Fixture(0);
      t.leaf_count = 6;
      Bad(t);
      t = Fixture(0);
      t.nodes[1].bounds.minimum[0] = INFINITY;
      Bad(t);
      t = Fixture(0);
      t.nodes[1].bounds.maximum[2] = -1;
      Bad(t);
    }
    ++groups;
    {
      auto t = Fixture(0);
      t.nodes[1].bounds.minimum[3] = std::bit_cast<float>(0x80000000u);
      t.nodes[2].bounds.minimum[3] = 0;
      t.nodes[3].bounds.minimum[3] = std::bit_cast<float>(0x7fc12345u);
      t.nodes[4].bounds.maximum[3] = std::bit_cast<float>(0xffc54321u);
      for (unsigned id = 6; id <= 9; ++id)
        t.nodes[id].bounds =
            Join(t.nodes[t.nodes[id].child0].bounds,
                 t.nodes[t.nodes[id].child1_or_payload].bounds);
      Same(Monstey_NavFinalizeHierarchy(t), Reference(t));
      ++trees;
    }
    ++groups;
    {
      const auto originalCsr = _mm_getcsr();
      struct Monstey_CsrRestore {
        unsigned value;
        ~Monstey_CsrRestore() { _mm_setcsr(value); }
      } restore{originalCsr};
      unsigned modes = 0;
      for (unsigned rounding = 0; rounding < 4; ++rounding)
        for (unsigned flush = 0; flush < 2; ++flush)
          for (unsigned denormals = 0; denormals < 2; ++denormals) {
            _mm_setcsr((originalCsr & ~0xe040u) | (rounding << 13) |
                       (flush << 15) | (denormals << 6));
            auto t = Fixture(0);
            t.nodes[1].bounds = Box(.1f, .2f, .3f);
            t.nodes[2].bounds = Box(10.1f, 10.2f, .3f);
            t.nodes[3].bounds = Box(.1f, 10.2f, .3f);
            t.nodes[4].bounds = Box(10.1f, .2f, .3f);
            for (unsigned id = 6; id <= 9; ++id)
              t.nodes[id].bounds =
                  Join(t.nodes[t.nodes[id].child0].bounds,
                       t.nodes[t.nodes[id].child1_or_payload].bounds);
            const auto beforeMode = _mm_getcsr() & ~0x3fu;
            Same(Monstey_NavFinalizeHierarchy(t), Reference(t));
            Check((_mm_getcsr() & ~0x3fu) == beforeMode,
                  "Finalizer changed FP control mode");
            ++modes;
            ++trees;
          }
      Check(modes == 16, "FP control cases were skipped");
      ++groups;
    }
    std::mt19937 rng(0xa76a16);
    std::uniform_real_distribution<float> value(-100, 100), extent(.01f, 25);
    for (unsigned count :
         {3u, 4u, 5u, 8u, 16u, 17u, 18u, 31u, 32u, 33u, 64u, 97u, 152u})
      for (unsigned repeat = 0; repeat < 16; ++repeat) {
        std::vector<B> boxes;
        for (unsigned n = 0; n < count; ++n) {
          B b{};
          for (unsigned a = 0; a < 4; ++a) {
            b.minimum[a] = value(rng);
            b.maximum[a] = b.minimum[a] + extent(rng);
          }
          boxes.push_back(b);
        }
        const auto inserted = Monstey_NavInsertHierarchy(boxes);
        const auto core = Monstey_NavOptimizeHierarchyCore(inserted);
        const auto result = Monstey_NavFinalizeHierarchy(core);
        Same(result, Reference(core));
        Validate(result);
        Cover(result, boxes, rng, 64);
        ++trees;
      }
    groups += 3;
    Monstey_Json actual;
    if (argc == 3) {
      const std::filesystem::path path = argv[2];
      const auto bytes = Monstey_ReadFile(path);
      const auto hash = Monstey_HashSha256(bytes);
      Check(hash == "b9fb6380798b09110a8db133ea6139e17db88ec5cad123fc6aa1fe1b03"
                    "8008ee",
            "Actual Seregost hash");
      Check(Monstey_U32(bytes, 0) == 0x49414b48 && Monstey_U32(bytes, 4) == 3 &&
                Monstey_U32(bytes, 12) == bytes.size() - 20,
            "Actual envelope");
      Monstey_Havok g(std::span(bytes).subspan(16, bytes.size() - 20));
      const auto meshes = g.Objects("hkaiNavMesh");
      Check(meshes.size() == 1, "Actual mesh count");
      const auto faces = Records(g, meshes[0], "faces", 16),
                 edges = Records(g, meshes[0], "edges", 20),
                 vertices = Records(g, meshes[0], "vertices", 16);
      const auto original =
          Monstey_NavInsertFaceHierarchy(faces, edges, vertices);
      const auto core = Monstey_NavOptimizeHierarchyCore(original);
      const auto result = Monstey_NavFinalizeHierarchy(core);
      Same(result, Reference(core));
      Validate(result);
      const auto p =
          Monstey_NavPackPreorder(Monstey_NavPreorderHierarchy(result));
      std::vector<B> boxes(result.leaf_count);
      for (const auto &n : original.nodes)
        if (!n.child0 && n.child1_or_payload < boxes.size())
          boxes[n.child1_or_payload] = n.bounds;
      for (const auto &b : boxes) {
        const auto hits =
            Monstey_NavigationCompactTreeQuery({p.nodes, p.bounds}, b);
        for (std::size_t id = 0; id < boxes.size(); ++id)
          if (Intersects(b, boxes[id]))
            Check(std::find(hits.begin(), hits.end(),
                            static_cast<std::uint32_t>(id)) != hits.end(),
                  "Actual finalized query omission");
        ++queries;
      }
      Check(Monstey_HashSha256(path) == hash, "Actual source changed");
      actual = Monstey_Json::object(
          {{"source_sha256", hash},
           {"faces", faces.size() / 16},
           {"edges", edges.size() / 20},
           {"vertices", vertices.size() / 16},
           {"nodes", p.nodes.size() / 6},
           {"final_compact_sha256", Monstey_HashSha256(p.nodes)},
           {"root", result.root},
           {"runtime_verified", false}});
      ++groups;
      ++trees;
    }
    Check(groups == (argc == 3 ? 13u : 12u) &&
              trees == (argc == 3 ? 235u : 234u) &&
              queries == (argc == 3 ? 13464u : 13312u) && rejections == 6,
          "Mandatory coverage counters differ");
    Monstey_WriteNewText(
        argv[1],
        Monstey_Json::object({{"schema", "Monstey.NavFinalizerTests/1"},
                              {"status", "PASS"},
                              {"groups", groups},
                              {"rejection_cases", rejections},
                              {"hierarchies", trees},
                              {"conservative_queries", queries},
                              {"bitwise_bound_lanes", rows},
                              {"actual", actual},
                              {"original_code_executed", false},
                              {"game_process_accessed", false}})
                .dump(2) +
            "\n");
    std::cout << "PASS " << groups << " finalizer groups; trees=" << trees
              << " queries=" << queries << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
