#include "Monstey_TriangleCollision.hpp"
#include "Monstey_Physics.hpp"
#include "Monstey_World.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <functional>
#include <map>
#include <numeric>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
using V = std::array<double, 3>;
using Bounds = std::array<V, 2>;
constexpr std::uint32_t Monstey_TPhysics = 0xb9e81cf3;
constexpr std::array<std::uint32_t, 3> Monstey_TMasks{2097151, 2097151,
                                                      4194303};
const std::array<float, 3> Monstey_TCoeff{std::bit_cast<float>(0x35000004u),
                                          std::bit_cast<float>(0x35000004u),
                                          std::bit_cast<float>(0x34800002u)};
const float Monstey_TTreeCoeff = std::bit_cast<float>(0x3b90fdbcu),
            Monstey_TMaxFloat = std::bit_cast<float>(0x7f7fffeeu);
double Monstey_TF(double x) { return static_cast<float>(x); }
void Monstey_TKeys(const J &x, std::initializer_list<std::string_view> keys) {
  Monstey_Require(x.is_object(), "Triangle specification must be object");
  for (const auto &[k, v] : x.as_object()) {
    (void)v;
    Monstey_Require(std::find(keys.begin(), keys.end(), k) != keys.end(),
                    "Unknown triangle field " + k);
  }
}
V Monstey_TVec(const J &j, bool f32 = true) {
  Monstey_Require(j.is_array() && j.size() == 3,
                  "Triangle vector requires three coordinates");
  V v{};
  for (std::size_t i = 0; i < 3; ++i) {
    Monstey_Require(j.at(i).is_number(), "Triangle coordinate must be number");
    v[i] = j.at(i).as_number();
    Monstey_Require(std::isfinite(v[i]) &&
                        std::isfinite(static_cast<float>(v[i])),
                    "Triangle coordinate exceeds finite float32");
    if (f32)
      v[i] = Monstey_TF(v[i]);
  }
  return v;
}
V Monstey_TPoint(const J &j) {
  Monstey_Require(j.is_array() && j.size() >= 3, "Native vector is truncated");
  return {j.at(0).as_number(), j.at(1).as_number(), j.at(2).as_number()};
}
J Monstey_TJson(const V &v) { return J::array({v[0], v[1], v[2]}); }
J Monstey_TVector4(const V &v, double w) {
  return J::array({v[0], v[1], v[2], w});
}
J Monstey_TDomain(const Bounds &b, double radius) {
  return J::object({{"min", Monstey_TVector4(b[0], -radius)},
                    {"max", Monstey_TVector4(b[1], radius)}});
}
Bounds Monstey_TGetDomain(const J &j) {
  return {Monstey_TPoint(j.at("min")), Monstey_TPoint(j.at("max"))};
}
V Monstey_TSub(const V &a, const V &b) {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
V Monstey_TCross(const V &a, const V &b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}
double Monstey_TDot(const V &a, const V &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
double Monstey_TDistance(const V &a, const V &b) {
  const auto d = Monstey_TSub(a, b);
  return std::sqrt(Monstey_TDot(d, d));
}
V Monstey_TNormal(const std::array<V, 3> &v) {
  return Monstey_TCross(Monstey_TSub(v[1], v[0]), Monstey_TSub(v[2], v[0]));
}
Bounds Monstey_TBounds(const std::vector<V> &v) {
  Monstey_Require(!v.empty(), "Empty collision bounds");
  Bounds b{v[0], v[0]};
  for (const auto &p : v)
    for (std::size_t i = 0; i < 3; ++i) {
      b[0][i] = std::min(b[0][i], p[i]);
      b[1][i] = std::max(b[1][i], p[i]);
    }
  return b;
}
Bounds Monstey_TUnion(const std::vector<Bounds> &rows) {
  Monstey_Require(!rows.empty(), "Empty bounds union");
  auto b = rows[0];
  for (const auto &r : rows)
    for (std::size_t i = 0; i < 3; ++i) {
      b[0][i] = std::min(b[0][i], r[0][i]);
      b[1][i] = std::max(b[1][i], r[1][i]);
    }
  return b;
}
bool Monstey_TContains(const Bounds &a, const Bounds &b,
                       double tolerance = 2e-5) {
  for (std::size_t i = 0; i < 3; ++i)
    if (a[0][i] > b[0][i] + tolerance || a[1][i] < b[1][i] - tolerance)
      return false;
  return true;
}
double Monstey_TOutward(double x, bool upper) {
  auto f = static_cast<float>(x);
  Monstey_Require(std::isfinite(f), "Collision domain outside float32");
  if ((upper && f < x) || (!upper && f > x))
    f = std::nextafterf(f, upper ? INFINITY : -INFINITY);
  return f;
}
std::uint64_t Monstey_TWord(const J &j) {
  if (j.is_integer())
    return std::bit_cast<std::uint64_t>(j.as_int());
  Monstey_Require(j.is_string() && j.as_string().starts_with("0x"),
                  "Native shared64 word invalid");
  std::uint64_t word = 0;
  const auto &s = j.as_string();
  const auto p = std::from_chars(s.data() + 2, s.data() + s.size(), word, 16);
  Monstey_Require(p.ec == std::errc() && p.ptr == s.data() + s.size(),
                  "Native shared64 hex invalid");
  return word;
}
std::vector<V> Monstey_TDecodeShared(const J &packed, const Bounds &domain) {
  std::vector<V> out;
  for (const auto &value : packed.as_array()) {
    const auto bits = Monstey_TWord(value);
    const std::array<std::uint64_t, 3> q{
        bits & Monstey_TMasks[0], (bits >> 21) & Monstey_TMasks[1], bits >> 42};
    V v{};
    for (std::size_t i = 0; i < 3; ++i) {
      const auto step = Monstey_TF(Monstey_TF(domain[1][i] - domain[0][i]) *
                                   Monstey_TCoeff[i]);
      v[i] = Monstey_TF(Monstey_TF(double(q[i]) * step) + domain[0][i]);
    }
    out.push_back(v);
  }
  return out;
}
struct Monstey_TQuantized {
  J words = J::array();
  std::vector<V> decoded;
  double error = 0;
};
Monstey_TQuantized Monstey_TQuantize(const std::vector<V> &vertices,
                                     const Bounds &domain, double budget) {
  V steps{};
  for (std::size_t i = 0; i < 3; ++i) {
    steps[i] =
        Monstey_TF(Monstey_TF(domain[1][i] - domain[0][i]) * Monstey_TCoeff[i]);
    Monstey_Require(std::isfinite(steps[i]) && steps[i] > 0,
                    "Degenerate quantization domain");
  }
  Monstey_TQuantized result;
  for (const auto &v : vertices) {
    std::array<std::uint64_t, 3> q{};
    for (std::size_t i = 0; i < 3; ++i) {
      const auto x = (v[i] - domain[0][i]) / steps[i], base = std::floor(x),
                 fraction = x - base;
      const auto rounded =
          base + (fraction > 0.5 || (fraction == 0.5 && std::fmod(base, 2) != 0)
                      ? 1
                      : 0);
      q[i] = static_cast<std::uint64_t>(
          std::clamp(rounded, 0.0, double(Monstey_TMasks[i])));
    }
    const auto word = q[0] | (q[1] << 21) | (q[2] << 42);
    result.words.as_array().push_back(std::bit_cast<std::int64_t>(word));
  }
  result.decoded = Monstey_TDecodeShared(result.words, domain);
  for (std::size_t i = 0; i < vertices.size(); ++i)
    result.error = std::max(
        result.error, Monstey_TDistance(vertices[i], result.decoded[i]) * 100);
  Monstey_Require(result.error <= budget,
                  "Shared64 quantization exceeds centimetre error budget");
  return result;
}
std::pair<J, Bounds> Monstey_TQuantizedBounds(const Bounds &parent,
                                              const Bounds &child) {
  J code = J::array();
  Bounds decoded{};
  for (std::size_t i = 0; i < 3; ++i) {
    const auto step = Monstey_TF(Monstey_TF(parent[1][i] - parent[0][i]) *
                                 Monstey_TTreeCoeff);
    int a = -1, b = -1;
    for (int n = 0; n < 16; ++n) {
      if (Monstey_TF(parent[0][i] + Monstey_TF(double(n * n) * step)) <=
          child[0][i])
        a = n;
      if (Monstey_TF(parent[1][i] - Monstey_TF(double(n * n) * step)) >=
          child[1][i])
        b = n;
    }
    Monstey_Require(a >= 0 && b >= 0,
                    "Child bounds lie outside parent quantization domain");
    code.as_array().push_back((a << 4) | b);
    decoded[0][i] = Monstey_TF(parent[0][i] + Monstey_TF(double(a * a) * step));
    decoded[1][i] = Monstey_TF(parent[1][i] - Monstey_TF(double(b * b) * step));
  }
  return {code, decoded};
}
struct Monstey_TBinary {
  J nodes = J::array();
  std::map<std::size_t, std::size_t> leaves;
};
Monstey_TBinary Monstey_TBuildBinary(const std::vector<Bounds> &bounds,
                                     const Bounds &domain, unsigned width) {
  Monstey_Require(!bounds.empty(), "Cannot build empty BVH");
  Monstey_TBinary result;
  std::function<void(std::vector<std::size_t>, const Bounds &, bool)> emit =
      [&](std::vector<std::size_t> ids, const Bounds &parent, bool root) {
        const auto at = result.nodes.size();
        std::vector<Bounds> part;
        for (const auto i : ids)
          part.push_back(bounds[i]);
        const auto actual = Monstey_TUnion(part);
        const auto q = root ? std::pair<J, Bounds>{J::array({0, 0, 0}), domain}
                            : Monstey_TQuantizedBounds(parent, actual);
        result.nodes.as_array().push_back(J::object({{"xyz", q.first}}));
        if (ids.size() == 1) {
          const auto key = ids[0];
          result.leaves[key] = at;
          if (width == 4)
            result.nodes[at]["data"] = key << 1;
          else {
            result.nodes[at]["hiData"] = key >> 8;
            result.nodes[at]["loData"] = key & 255;
          }
        } else {
          std::size_t axis = 0;
          for (std::size_t i = 1; i < 3; ++i)
            if (actual[1][i] - actual[0][i] > actual[1][axis] - actual[0][axis])
              axis = i;
          std::stable_sort(ids.begin(), ids.end(), [&](auto a, auto b) {
            return bounds[a][0][axis] + bounds[a][1][axis] <
                   bounds[b][0][axis] + bounds[b][1][axis];
          });
          const auto split = ids.size() / 2;
          emit(std::vector<std::size_t>(ids.begin(),
                                        ids.begin() +
                                            static_cast<std::ptrdiff_t>(split)),
               q.second, false);
          const auto right = result.nodes.size();
          emit(std::vector<std::size_t>(
                   ids.begin() + static_cast<std::ptrdiff_t>(split), ids.end()),
               q.second, false);
          const auto offset = right - at;
          Monstey_Require(offset % 2 == 0,
                          "Native BVH right child requires even offset");
          if (width == 4) {
            Monstey_Require(offset <= 254,
                            "Section child offset exceeds8 bits");
            result.nodes[at]["data"] = offset | 1;
          } else {
            Monstey_Require(offset / 2 < 32768,
                            "Primary child offset exceeds15 bits");
            result.nodes[at]["hiData"] = 128 | (offset / 2 >> 8);
            result.nodes[at]["loData"] = (offset / 2) & 255;
          }
        }
      };
  std::vector<std::size_t> ids(bounds.size());
  std::iota(ids.begin(), ids.end(), 0);
  emit(ids, domain, true);
  return result;
}
J Monstey_TBuildSimd(const std::vector<Bounds> &bounds,
                     const std::vector<std::uint32_t> &keys) {
  auto empty = []() {
    J j = J::object();
    for (const auto name : {"lx", "ly", "lz"})
      j[name] = J::array({Monstey_TMaxFloat, Monstey_TMaxFloat,
                          Monstey_TMaxFloat, Monstey_TMaxFloat});
    for (const auto name : {"hx", "hy", "hz"})
      j[name] = J::array({-Monstey_TMaxFloat, -Monstey_TMaxFloat,
                          -Monstey_TMaxFloat, -Monstey_TMaxFloat});
    j["data"] = J::array({0, 0, 0, 0});
    return j;
  };
  J nodes = J::array({empty()});
  std::function<std::size_t(std::vector<std::size_t>)> emit =
      [&](std::vector<std::size_t> ids) {
        const auto at = nodes.size();
        nodes.as_array().push_back(empty());
        std::vector<Bounds> part;
        for (const auto i : ids)
          part.push_back(bounds[i]);
        const auto actual = Monstey_TUnion(part);
        std::size_t axis = 0;
        for (std::size_t i = 1; i < 3; ++i)
          if (actual[1][i] - actual[0][i] > actual[1][axis] - actual[0][axis])
            axis = i;
        std::stable_sort(ids.begin(), ids.end(), [&](auto a, auto b) {
          return bounds[a][0][axis] + bounds[a][1][axis] <
                 bounds[b][0][axis] + bounds[b][1][axis];
        });
        const auto group = (ids.size() + 3) / 4;
        std::size_t lane = 0;
        for (std::size_t first = 0; first < ids.size();
             first += group, ++lane) {
          const auto end = std::min(ids.size(), first + group);
          std::vector<std::size_t> subset(
              ids.begin() + static_cast<std::ptrdiff_t>(first),
              ids.begin() + static_cast<std::ptrdiff_t>(end));
          std::vector<Bounds> sub;
          for (const auto i : subset)
            sub.push_back(bounds[i]);
          const auto b = Monstey_TUnion(sub);
          for (std::size_t d = 0; d < 3; ++d) {
            nodes[at][std::string("l") + "xyz"[d]][lane] = b[0][d];
            nodes[at][std::string("h") + "xyz"[d]][lane] = b[1][d];
          }
          const auto code = subset.size() == 1
                                ? (std::size_t(keys[subset[0]]) << 1) | 1
                                : emit(subset) << 1;
          nodes[at]["data"][lane] = code;
        }
        return at;
      };
  std::vector<std::size_t> ids(bounds.size());
  std::iota(ids.begin(), ids.end(), 0);
  emit(ids);
  return nodes;
}
} // namespace
J Monstey_DecodeCompressedMesh(const J &mesh) {
  const auto &tree = mesh.at("data").at("meshTree");
  const auto domain = Monstey_TGetDomain(tree.at("domain"));
  const auto shared = Monstey_TDecodeShared(tree.at("sharedVertices"), domain);
  J decoded = J::array();
  std::set<std::uint32_t> seen;
  const auto &sections = tree.at("sections").as_array();
  Monstey_Require(sections.size() <= 32768,
                  "Compressed section count exceeds native key space");
  for (std::size_t sid = 0; sid < sections.size(); ++sid) {
    const auto &sec = sections[sid];
    const auto pd = sec.at("primitives").at("data").as_uint(), count = pd & 255,
               start = pd >> 8,
               sd = sec.at("sharedVertices").at("data").as_uint(),
               packed = sd & 255, sharedStart = sd >> 8;
    Monstey_Require(count > 0 && start <= tree.at("primitives").size() &&
                        count <= tree.at("primitives").size() - start,
                    "Primitive range outside native array");
    for (std::size_t j = 0; j < count; ++j) {
      const auto &prim = tree.at("primitives")
                             .at(static_cast<std::size_t>(start + j))
                             .at("indices");
      Monstey_Require(prim.size() == 4,
                      "Native primitive requires four indices");
      std::array<V, 4> points{};
      for (std::size_t lane = 0; lane < 4; ++lane) {
        const auto index = prim.at(lane).as_uint();
        if (index < packed) {
          const auto word =
              tree.at("packedVertices")
                  .at(static_cast<std::size_t>(
                      sec.at("firstPackedVertex").as_uint() + index))
                  .as_uint();
          const std::array<std::uint64_t, 3> q{word & 2047, (word >> 11) & 2047,
                                               (word >> 22) & 1023};
          for (std::size_t d = 0; d < 3; ++d)
            points[lane][d] = Monstey_TF(
                sec.at("codecParms").at(d).as_number() +
                Monstey_TF(double(q[d]) *
                           sec.at("codecParms").at(d + 3).as_number()));
        } else {
          const auto pointer =
              tree.at("sharedVerticesIndex")
                  .at(static_cast<std::size_t>(sharedStart + index - packed))
                  .as_uint() +
              sec.at("page").as_uint() * 65536;
          Monstey_Require(pointer < shared.size(),
                          "Shared vertex outside native array");
          points[lane] = shared[static_cast<std::size_t>(pointer)];
        }
      }
      const auto key = static_cast<std::uint32_t>((sid << 8) | (j << 1));
      Monstey_Require(prim.at(1) != prim.at(3),
                      "Convex primitive outside triangle decoder");
      const auto runs = sec.at("dataRuns").at("data").as_uint(),
                 runStart = runs >> 8, runCount = runs & 255;
      Monstey_Require(runStart <= tree.at("primitiveDataRuns").size() &&
                          runCount <=
                              tree.at("primitiveDataRuns").size() - runStart,
                      "Material run range invalid");
      std::optional<J> tag;
      for (std::size_t ri = 0; ri < runCount; ++ri) {
        const auto &r = tree.at("primitiveDataRuns")
                            .at(static_cast<std::size_t>(runStart + ri));
        const auto first = r.at("index").as_uint(), n = r.at("count").as_uint();
        if (first <= j && j - first < n) {
          Monstey_Require(!tag, "Ambiguous primitive material run");
          tag = r.at("value").at("data");
        }
      }
      Monstey_Require(tag.has_value(), "Primitive material run missing");
      auto add = [&](std::uint32_t k, const V &a, const V &b, const V &c) {
        Monstey_Require(seen.insert(k).second, "Duplicate native triangle key");
        decoded.as_array().push_back(J::object(
            {{"key", k},
             {"points_m",
              J::array({Monstey_TJson(a), Monstey_TJson(b), Monstey_TJson(c)})},
             {"material", *tag}}));
      };
      add(key, points[0], points[1], points[2]);
      if (prim.at(2) != prim.at(3))
        add(key | 1, points[0], points[2], points[3]);
    }
  }
  return J::object({{"triangles", decoded},
                    {"triangle_count", decoded.size()},
                    {"independent_native_codec", true}});
}
J Monstey_ValidateCompressedMesh(const J &mesh) {
  const auto &tree = mesh.at("data").at("meshTree");
  const auto decoded = Monstey_DecodeCompressedMesh(mesh);
  const auto &rows = decoded.at("triangles").as_array();
  Monstey_Require(!rows.empty(), "Empty compressed mesh");
  std::map<std::size_t, Bounds> primitiveBounds;
  std::uint32_t maximum = 0;
  for (const auto &row : rows) {
    const auto key = static_cast<std::uint32_t>(row.at("key").as_uint()),
               base = key & ~1u;
    std::vector<V> points;
    for (const auto &p : row.at("points_m").as_array())
      points.push_back(Monstey_TVec(p, false));
    const auto b = Monstey_TBounds(points);
    if (primitiveBounds.contains(base))
      primitiveBounds[base] = Monstey_TUnion({primitiveBounds[base], b});
    else
      primitiveBounds.emplace(base, b);
    maximum = std::max(maximum, key);
  }
  auto decodeBounds = [](const Bounds &parent, const J &xyz) {
    Monstey_Require(xyz.size() == 3, "BVH XYZ byte count invalid");
    Bounds b{};
    for (std::size_t d = 0; d < 3; ++d) {
      const auto byte = xyz.at(d).as_uint();
      Monstey_Require(byte <= 255, "BVH bound codec byte exceeds8bits");
      const auto scale = Monstey_TF(Monstey_TF(parent[1][d] - parent[0][d]) *
                                    Monstey_TTreeCoeff);
      b[0][d] = Monstey_TF(
          parent[0][d] + Monstey_TF(double((byte >> 4) * (byte >> 4)) * scale));
      b[1][d] = Monstey_TF(
          parent[1][d] - Monstey_TF(double((byte & 15) * (byte & 15)) * scale));
    }
    return b;
  };
  auto walk = [&](const J &nodes, const Bounds &domain, unsigned width,
                  const std::map<std::size_t, Bounds> &expected) {
    std::set<std::size_t> visited;
    std::map<std::size_t, std::size_t> leaves;
    std::function<void(std::size_t, const Bounds &, std::size_t)> visit =
        [&](std::size_t i, const Bounds &b, std::size_t depth) {
          Monstey_Require(i < nodes.size() && depth <= 64 &&
                              visited.insert(i).second,
                          "BVH cycle/depth/index invalid");
          const auto &n = nodes.at(i);
          const auto value = width == 4 ? n.at("data").as_uint()
                                        : (n.at("hiData").as_uint() << 8) |
                                              n.at("loData").as_uint();
          const bool internal =
              width == 4 ? (value & 1) != 0 : (value & 32768) != 0;
          const auto right = width == 4 ? i + (value & 254)
                                        : i + 2 * (value & 32767),
                     key = width == 4 ? value >> 1 : value;
          if (internal) {
            Monstey_Require(i + 1 < nodes.size() && right < nodes.size(),
                            "BVH child outside node array");
            visit(i + 1, decodeBounds(b, nodes.at(i + 1).at("xyz")), depth + 1);
            visit(static_cast<std::size_t>(right),
                  decodeBounds(
                      b, nodes.at(static_cast<std::size_t>(right)).at("xyz")),
                  depth + 1);
          } else {
            const auto it = expected.find(static_cast<std::size_t>(key));
            Monstey_Require(it != expected.end() &&
                                !leaves.contains(static_cast<std::size_t>(key)),
                            "Unknown/duplicate BVH leaf");
            Monstey_Require(Monstey_TContains(b, it->second),
                            "BVH leaf clips physical geometry");
            leaves[static_cast<std::size_t>(key)] = i;
          }
        };
    visit(0, domain, 0);
    Monstey_Require(visited.size() == nodes.size() &&
                        leaves.size() == expected.size(),
                    "BVH unreachable nodes or missing leaves");
    return leaves;
  };
  std::map<std::size_t, Bounds> sectionExpected;
  const auto &sections = tree.at("sections").as_array();
  for (std::size_t sid = 0; sid < sections.size(); ++sid) {
    std::map<std::size_t, Bounds> bounds;
    std::vector<Bounds> all;
    for (const auto &[key, b] : primitiveBounds)
      if (key >> 8 == sid) {
        bounds[(key >> 1) & 127] = b;
        all.push_back(b);
      }
    Monstey_Require(!all.empty(), "Empty native BVH section");
    walk(sections[sid].at("nodes"),
         Monstey_TGetDomain(sections[sid].at("domain")), 4, bounds);
    sectionExpected[sid] = Monstey_TUnion(all);
  }
  const auto leaves =
      walk(tree.at("nodes"), Monstey_TGetDomain(tree.at("domain")), 5,
           sectionExpected);
  for (std::size_t i = 0; i < sections.size(); ++i)
    Monstey_Require(sections[i].at("leafIndex").as_uint() == leaves.at(i),
                    "Native section leafIndex mismatch");
  const auto &nodes = mesh.at("data").at("simdTree").at("nodes");
  std::set<std::size_t> visited, keys;
  std::function<void(std::size_t, std::optional<Bounds>, std::size_t)> simd =
      [&](std::size_t i, std::optional<Bounds> parent, std::size_t depth) {
        Monstey_Require(i > 0 && i < nodes.size() && depth <= 64 &&
                            visited.insert(i).second,
                        "SIMD cycle/depth/index invalid");
        const auto &n = nodes.at(i);
        Monstey_Require(n.at("data").size() == 4,
                        "SIMD node requires four lanes");
        for (std::size_t lane = 0; lane < 4; ++lane) {
          Bounds b{};
          for (std::size_t d = 0; d < 3; ++d) {
            b[0][d] = n.at(std::string("l") + "xyz"[d]).at(lane).as_number();
            b[1][d] = n.at(std::string("h") + "xyz"[d]).at(lane).as_number();
            Monstey_Require(std::isfinite(b[0][d]) && std::isfinite(b[1][d]),
                            "Non-finite SIMD bound");
          }
          const auto code = n.at("data").at(lane).as_uint();
          if (b[0][0] > b[1][0] || b[0][1] > b[1][1] || b[0][2] > b[1][2]) {
            Monstey_Require(code == 0, "Inactive SIMD lane has nonzero link");
            continue;
          }
          if (parent)
            Monstey_Require(Monstey_TContains(*parent, b),
                            "SIMD parent clips child");
          if (code & 1) {
            const auto key = static_cast<std::size_t>(code >> 1);
            Monstey_Require(primitiveBounds.contains(key) &&
                                keys.insert(key).second,
                            "SIMD leaf duplicate/invalid");
            Monstey_Require(Monstey_TContains(b, primitiveBounds.at(key)),
                            "SIMD leaf clips triangle");
          } else
            simd(static_cast<std::size_t>(code >> 1), b, depth + 1);
        }
      };
  simd(1, {}, 0);
  Monstey_Require(visited.size() == nodes.size() - 1 &&
                      keys.size() == primitiveBounds.size(),
                  "SIMD missing/unreachable geometry");
  Monstey_Require(tree.at("maxKeyValue").as_uint() == maximum &&
                      tree.at("numPrimitiveKeys").as_uint() == rows.size(),
                  "Native primitive key maximum/count mismatch");
  const auto bits = tree.at("bitsPerKey").as_uint();
  Monstey_Require(bits < 32 && bits == mesh.at("numShapeKeyBits").as_uint() &&
                      (std::uint64_t(1) << bits) > maximum,
                  "Native shape key bit capacity invalid");
  const auto &storage = mesh.at("triangleIsInterior").at("storage");
  Monstey_Require(storage.at("numBits").as_uint() > maximum &&
                      storage.at("words").size() * 32 >=
                          storage.at("numBits").as_uint(),
                  "Interior bitset capacity invalid");
  return J::object({{"status", "PASS"},
                    {"triangles", rows.size()},
                    {"sections", sections.size()},
                    {"material_runs_resolve", rows.size()},
                    {"binary_and_simd_exact_coverage", true},
                    {"bounds_conservative", true},
                    {"independent_native_codec", true}});
}
namespace {
struct Monstey_TProfile {
  B shape, tag;
  Monstey_Havok graph;
  std::size_t mesh, compound;
  Monstey_TProfile(B s) : shape(std::move(s)), tag(Extract(shape)), graph(tag) {
    const auto meshes = graph.Objects("hknpCompressedMeshShape"),
               compounds = graph.Objects("hknpCompoundShape");
    Monstey_Require(meshes.size() == 1 && compounds.size() == 1,
                    "Profile requires one compressed mesh and compound");
    mesh = meshes[0];
    compound = compounds[0];
    const auto m = graph.Plain(mesh), c = graph.Plain(compound);
    const auto &t = m.at("data").at("meshTree");
    Monstey_Require(c.at("instances").at("elements").size() == 1 &&
                        t.at("sections").size() == 1 &&
                        t.at("primitives").size() == 1,
                    "Expected minimal one-primitive native profile");
    Monstey_Require(m.at("data").at("connectivity") ==
                        J::object({{"headers", J::array()},
                                   {"localLinks", J::array()},
                                   {"globalLinks", J::array()}}),
                    "Profile has extra native connectivity");
    Monstey_Require(m.at("materialTable").is_null() &&
                        c.at("materialTable").is_null() &&
                        t.at("primitiveDataRuns").size() == 1,
                    "Profile material table/run is not qualified");
  }
  static B Extract(const B &s) {
    Monstey_Require(s.size() >= 28 && Monstey_U32(s, 0) == 12 &&
                        Monstey_U32(s, 12) == 0xffffd00d &&
                        Monstey_U32(s, 16) == s.size() - 24 &&
                        Monstey_U32(s, s.size() - 4) == 0xffffdead,
                    "Profile must be exact native type12 TAG0 envelope");
    return B(s.begin() + 20, s.end() - 4);
  }
};
B Monstey_TPhysicsRaw(std::span<const std::uint8_t> source) {
  if (source.size() >= 4 && Monstey_U32(source, 0) == 0x524a424f)
    return Monstey_ObjresReadChunk(source, Monstey_TPhysics);
  return B(source.begin(), source.end());
}
Monstey_TProfile Monstey_TLoadProfile(std::span<const std::uint8_t> source,
                                      const J &spec) {
  Monstey_TKeys(spec, {"source_sha256", "shape_index", "route"});
  Monstey_Require(spec.contains("source_sha256") &&
                      spec.at("source_sha256").as_string() ==
                          Monstey_HashSha256(source),
                  "Triangle profile source SHA mismatch");
  std::vector<std::size_t> route;
  if (spec.contains("route"))
    for (const auto &i : spec.at("route").as_array()) {
      Monstey_Require(i.is_integer() && i.as_uint() <= UINT32_MAX,
                      "Profile route index invalid");
      route.push_back(static_cast<std::size_t>(i.as_uint()));
    }
  Monstey_Require(route.size() <= 64, "Profile route exceeds nesting bound");
  Monstey_Physics physics(Monstey_TPhysicsRaw(source));
  return Monstey_TProfile(physics.ShapeBytes(
      static_cast<std::size_t>(spec.at("shape_index").as_uint()), route));
}
} // namespace
Monstey_TriangleResult Monstey_TriangleCompleteProfile(
    std::span<const std::uint8_t> profileSource, const J &profile,
    std::span<const std::uint8_t> donorSource, const J &donor) {
  auto select = [](std::span<const std::uint8_t> raw, const J &spec) {
    Monstey_TKeys(spec, {"source_sha256", "shape_index", "route"});
    Monstey_Require(spec.at("source_sha256").as_string() ==
                        Monstey_HashSha256(raw),
                    "THSH native source hash mismatch");
    std::vector<std::size_t> route;
    if (spec.contains("route"))
      for (const auto &i : spec.at("route").as_array())
        route.push_back(static_cast<std::size_t>(i.as_uint()));
    Monstey_Require(route.size() <= 64, "THSH profile route too deep");
    Monstey_Physics physics(Monstey_TPhysicsRaw(raw));
    return std::pair<B, std::vector<std::size_t>>{
        physics.ShapeBytes(
            static_cast<std::size_t>(spec.at("shape_index").as_uint()), route),
        route};
  };
  const auto src = select(profileSource, profile),
             dst = select(donorSource, donor);
  const auto tag =
      Monstey_Havok::CompleteTypeHashes(Monstey_TProfile::Extract(src.first),
                                        Monstey_TProfile::Extract(dst.first));
  B replacement;
  Monstey_AppendU32(replacement, 12);
  replacement.insert(replacement.end(), src.first.begin() + 4,
                     src.first.begin() + 12);
  Monstey_AppendU32(replacement, 0xffffd00d);
  Monstey_AppendU32(replacement, static_cast<std::uint32_t>(tag.size()));
  replacement.insert(replacement.end(), tag.begin(), tag.end());
  Monstey_AppendU32(replacement, 0xffffdead);
  Monstey_Physics physics(Monstey_TPhysicsRaw(profileSource));
  physics.ReplaceShape(
      static_cast<std::size_t>(profile.at("shape_index").as_uint()),
      replacement, src.second);
  auto result =
      profileSource.size() >= 4 && Monstey_U32(profileSource, 0) == 0x524a424f
          ? Monstey_ObjresReplaceChunk(profileSource, Monstey_TPhysics,
                                       physics.Encode())
          : physics.Encode();
  return {result,
          J::object({{"status", "NATIVE_TRIANGLE_PROFILE_THSH_COMPLETED"},
                     {"source_sha256", Monstey_HashSha256(profileSource)},
                     {"donor_sha256", Monstey_HashSha256(donorSource)},
                     {"output_sha256", Monstey_HashSha256(result)},
                     {"native_data_and_item_ptch_byte_exact", true},
                     {"native_type_layout_and_sdk_byte_exact", true},
                     {"no_hashes_guessed", true},
                     {"runtime_verified", false}})};
}
Monstey_TriangleResult
Monstey_TriangleCook(std::span<const std::uint8_t> profileSource,
                     const J &profile, const J &input, const J &triangles,
                     double budget, const J &tags) {
  Monstey_Require(std::isfinite(budget) && budget > 0 && budget <= 10,
                  "Maximum triangle error budget must be in (0,10] cm");
  Monstey_Require(input.is_array() && input.size() >= 3 &&
                      input.size() <= 65535,
                  "Expected3..65535 collision vertices");
  Monstey_Require(triangles.is_array() && !triangles.empty() &&
                      triangles.size() <= 65536,
                  "Expected1..65536 collision triangles");
  Monstey_Require(tags.is_null() ||
                      (tags.is_array() && tags.size() == triangles.size()),
                  "Expected one native surface tag per triangle");
  if (!tags.is_null())
    for (const auto &t : tags.as_array())
      Monstey_Require(t.is_integer(), "Surface tags must be integers");
  std::vector<V> vertices, original;
  for (const auto &p : input.as_array()) {
    original.push_back(Monstey_TVec(p, false));
    auto v = Monstey_TVec(p);
    for (auto &x : v)
      x = Monstey_TF(x * .01);
    vertices.push_back(v);
  }
  std::vector<std::array<std::size_t, 3>> faces;
  for (const auto &f : triangles.as_array()) {
    Monstey_Require(f.is_array() && f.size() == 3,
                    "Triangle requires three indices");
    std::array<std::size_t, 3> face{};
    for (std::size_t i = 0; i < 3; ++i) {
      Monstey_Require(f.at(i).is_integer() &&
                          f.at(i).as_uint() < vertices.size(),
                      "Triangle vertex index outside table");
      face[i] = static_cast<std::size_t>(f.at(i).as_uint());
    }
    Monstey_Require(face[0] != face[1] && face[1] != face[2] &&
                        face[2] != face[0],
                    "Triangle repeats vertex");
    const auto n = Monstey_TNormal(
        {vertices[face[0]], vertices[face[1]], vertices[face[2]]});
    Monstey_Require(Monstey_TDot(n, n) > 1e-16,
                    "Degenerate collision triangle");
    faces.push_back(face);
  }
  auto native = Monstey_TLoadProfile(profileSource, profile);
  auto mesh = native.graph.Plain(native.mesh),
       compound = native.graph.Plain(native.compound);
  auto &tree = mesh["data"]["meshTree"];
  const auto radius = mesh.at("convexRadius").as_number();
  Monstey_Require(std::isfinite(radius) && radius >= 0,
                  "Invalid native collision convex radius");
  const auto bounds = Monstey_TBounds(vertices);
  const auto padding = radius + budget * .01 + .0001;
  Bounds domain{};
  for (std::size_t i = 0; i < 3; ++i) {
    domain[0][i] = Monstey_TOutward(bounds[0][i] - padding, false);
    domain[1][i] = Monstey_TOutward(bounds[1][i] + padding, true);
  }
  auto quantized = Monstey_TQuantize(vertices, domain, budget);
  double totalError = 0;
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    auto cm = quantized.decoded[i];
    for (auto &x : cm)
      x *= 100;
    totalError = std::max(totalError, Monstey_TDistance(original[i], cm));
  }
  Monstey_Require(totalError <= budget,
                  "Total centimetre-to-native error exceeds budget");
  for (const auto &face : faces) {
    const auto a = Monstey_TNormal(
                   {vertices[face[0]], vertices[face[1]], vertices[face[2]]}),
               b = Monstey_TNormal({quantized.decoded[face[0]],
                                    quantized.decoded[face[1]],
                                    quantized.decoded[face[2]]});
    Monstey_Require(Monstey_TDot(a, b) > 0,
                    "Quantization collapses or flips triangle winding");
  }
  J sharedIndices = J::array(), primitives = J::array(), runs = J::array(),
    sections = J::array();
  std::vector<Bounds> sectionBounds, triangleBounds;
  std::vector<std::uint32_t> keys;
  const auto sectionTemplate = tree.at("sections").at(0),
             defaultTag =
                 tree.at("primitiveDataRuns").at(0).at("value").at("data");
  for (std::size_t first = 0; first < faces.size(); first += 64) {
    const auto count = std::min(std::size_t(64), faces.size() - first);
    std::vector<std::size_t> local;
    std::map<std::size_t, std::size_t> mapping;
    for (std::size_t j = 0; j < count; ++j)
      for (const auto i : faces[first + j])
        if (!mapping.contains(i)) {
          mapping[i] = local.size();
          local.push_back(i);
        }
    const auto sharedStart = sharedIndices.size(),
               primitiveStart = primitives.size();
    for (const auto i : local)
      sharedIndices.as_array().push_back(i);
    std::vector<Bounds> part;
    for (std::size_t j = 0; j < count; ++j) {
      const auto &face = faces[first + j];
      const auto a = mapping.at(face[0]), b = mapping.at(face[1]),
                 c = mapping.at(face[2]);
      primitives.as_array().push_back(
          J::object({{"indices", J::array({a, b, c, c})}}));
      auto pb = Monstey_TBounds({quantized.decoded[face[0]],
                                 quantized.decoded[face[1]],
                                 quantized.decoded[face[2]]});
      for (std::size_t d = 0; d < 3; ++d) {
        pb[0][d] = Monstey_TOutward(pb[0][d] - radius, false);
        pb[1][d] = Monstey_TOutward(pb[1][d] + radius, true);
      }
      part.push_back(pb);
      triangleBounds.push_back(pb);
      keys.push_back(
          static_cast<std::uint32_t>((sections.size() << 8) | (j << 1)));
    }
    const auto sd = Monstey_TUnion(part);
    auto bvh = Monstey_TBuildBinary(part, sd, 4);
    J sectionRuns = J::array();
    for (std::size_t j = 0; j < count; ++j) {
      const auto value = tags.is_null() ? defaultTag : tags.at(first + j);
      if (!sectionRuns.empty() &&
          sectionRuns.at(sectionRuns.size() - 1).at("value").at("data") ==
              value) {
        auto &row = sectionRuns[sectionRuns.size() - 1];
        row["count"] = row.at("count").as_uint() + 1;
      } else
        sectionRuns.as_array().push_back(
            J::object({{"value", J::object({{"data", value}})},
                       {"index", j},
                       {"count", 1}}));
    }
    auto sec = sectionTemplate;
    sec["nodes"] = bvh.nodes;
    sec["domain"] = Monstey_TDomain(sd, radius);
    sec["codecParms"] = J::array({0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    sec["firstPackedVertex"] = 0;
    sec["sharedVertices"] = J::object({{"data", sharedStart << 8}});
    sec["primitives"] = J::object({{"data", (primitiveStart << 8) | count}});
    sec["dataRuns"] =
        J::object({{"data", (runs.size() << 8) | sectionRuns.size()}});
    sec["numPackedVertices"] = 0;
    sec["numSharedIndices"] = local.size();
    sec["leafIndex"] = 0;
    sec["page"] = 0;
    sec["flags"] = 0;
    sec["layerData"] = 0;
    sec["unusedData"] = 0;
    for (const auto &r : sectionRuns.as_array())
      runs.as_array().push_back(r);
    sections.as_array().push_back(sec);
    sectionBounds.push_back(sd);
  }
  auto treeBounds = sectionBounds;
  treeBounds.push_back(domain);
  Monstey_Require(Monstey_TUnion(treeBounds) == domain,
                  "Decoded vertices exceed global quantization domain");
  auto primary = Monstey_TBuildBinary(sectionBounds, domain, 5);
  for (std::size_t i = 0; i < sections.size(); ++i)
    sections[i]["leafIndex"] = primary.leaves.at(i);
  const auto maxKey = *std::max_element(keys.begin(), keys.end());
  const auto bits = std::max(1, std::bit_width(maxKey + 1));
  tree["nodes"] = primary.nodes;
  tree["domain"] = Monstey_TDomain(domain, radius);
  tree["numPrimitiveKeys"] = faces.size();
  tree["bitsPerKey"] = bits;
  tree["maxKeyValue"] = maxKey;
  tree["primitiveStoresIsFlatConvex"] = 255;
  tree["sections"] = sections;
  tree["primitives"] = primitives;
  tree["sharedVerticesIndex"] = sharedIndices;
  tree["packedVertices"] = J::array();
  const auto dataId =
      native.graph.Dereference(native.graph.Field(native.mesh, "data"));
  const auto treeId =
      native.graph.Dereference(native.graph.Field(dataId, "meshTree"));
  const auto sharedId = native.graph.Field(treeId, "sharedVertices");
  const auto &arrayType = native.graph.Types()[native.graph.EffectiveType(
      native.graph.Node(sharedId).type)];
  const auto &wordType =
      native.graph.Types()[native.graph.EffectiveType(arrayType.pointer)];
  if (!(wordType.subtype & 0x200))
    for (auto &word : quantized.words.as_array())
      if (word.as_int() < 0)
        word = Monstey_Hex(Monstey_TWord(word), 16);
  tree["sharedVertices"] = quantized.words;
  tree["primitiveDataRuns"] = runs;
  mesh["numShapeKeyBits"] = bits;
  J words = J::array();
  for (std::size_t i = 0; i < (std::size_t(maxKey) + 32) / 32; ++i)
    words.as_array().push_back(0);
  mesh["triangleIsInterior"]["storage"] =
      J::object({{"words", words}, {"numBits", maxKey + 1}});
  mesh["data"]["simdTree"]["nodes"] = Monstey_TBuildSimd(triangleBounds, keys);
  Monstey_Require(
      mesh.at("numTriangles").as_uint() == 0 &&
          mesh.at("numConvexShapes").as_uint() == 0,
      "Native informational triangle counters differ from qualified profile");
  native.graph.Replace(native.mesh, mesh);
  auto cb = compound.at("aabb");
  for (std::size_t i = 0; i < 3; ++i) {
    cb["min"][i] = domain[0][i];
    cb["max"][i] = domain[1][i];
  }
  native.graph.Replace(native.graph.Field(native.compound, "aabb"), cb);
  native.graph.Replace(
      native.graph.Field(native.compound, "estimatedNumShapeKeys"),
      faces.size() * 2);
  const auto bvhField =
      native.graph.Field(native.compound, "boundingVolumeData");
  auto bvh = native.graph.Plain(bvhField);
  Monstey_Require(bvh.at("aabbTree").at("nodes").size() > 1,
                  "Compound BVH profile lacks active node");
  bvh["aabbTree"]["nodes"][1]["aabb"] = cb;
  native.graph.Replace(bvhField, bvh);
  const auto tag = native.graph.Encode();
  Monstey_Havok reread(tag);
  const auto rereadMeshes = reread.Objects("hknpCompressedMeshShape");
  Monstey_Require(rereadMeshes.size() == 1 &&
                      reread.Plain(rereadMeshes[0]) ==
                          native.graph.Plain(native.mesh),
                  "Compressed native geometry readback differs");
  const auto actual = reread.Plain(rereadMeshes[0]);
  const auto spatial = Monstey_ValidateCompressedMesh(actual);
  B shape;
  Monstey_AppendU32(shape, 12);
  shape.insert(shape.end(), native.shape.begin() + 4,
               native.shape.begin() + 12);
  Monstey_AppendU32(shape, 0xffffd00d);
  Monstey_AppendU32(shape, static_cast<std::uint32_t>(tag.size()));
  shape.insert(shape.end(), tag.begin(), tag.end());
  Monstey_AppendU32(shape, 0xffffdead);
  std::size_t sectionNodes = 0;
  for (const auto &s : sections.as_array())
    sectionNodes += s.at("nodes").size();
  auto report = J::object(
      {{"schema", "Monstey_WorldTriangleCollision_1"},
       {"vertices", vertices.size()},
       {"triangles", faces.size()},
       {"sections", sections.size()},
       {"bits_per_key", bits},
       {"max_key", maxKey},
       {"native_material_data", defaultTag},
       {"convex_radius_m", radius},
       {"max_quantization_error_cm", totalError},
       {"requested_error_cm", budget},
       {"shared_vertex_codec", "21/21/22 bits, native float32 decode"},
       {"bounds_m",
        J::array({Monstey_TJson(domain[0]), Monstey_TJson(domain[1])})},
       {"shape_bytes", shape.size()},
       {"shape_sha256", Monstey_HashSha256(shape)},
       {"tag0_bytes", tag.size()},
       {"primary_nodes", primary.nodes.size()},
       {"section_nodes", sectionNodes},
       {"simd_nodes", actual.at("data").at("simdTree").at("nodes").size()},
       {"native_type_and_sdk_exact", true},
       {"type_hash_coverage", reread.TypeHashCoverage()},
       {"item_ptch_audit", reread.Audit()},
       {"spatial_audit", spatial},
       {"geometry_generated", true},
       {"boxes_approximated", false},
       {"implementation", "C++20"},
       {"python_runtime", false},
       {"runtime_verified", false}});
  return {std::move(shape), std::move(report)};
}
namespace {
std::array<double, 4> Monstey_TQuaternion(const J &j) {
  Monstey_Require(j.is_array() && j.size() == 4,
                  "Placement quaternion needs four coordinates");
  std::array<double, 4> q{};
  double norm = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    q[i] = Monstey_TF(j.at(i).as_number());
    Monstey_Require(std::isfinite(q[i]), "Placement quaternion is not finite");
    norm += q[i] * q[i];
  }
  Monstey_Require(std::abs(norm - 1) < .002,
                  "Placement quaternion must be normalized");
  return q;
}
V Monstey_TRotate(const V &v, const std::array<double, 4> &q) {
  const V a{q[0], q[1], q[2]};
  const auto t = Monstey_TCross(a, v), u = Monstey_TCross(a, t);
  return {v[0] + 2 * (q[3] * t[0] + u[0]), v[1] + 2 * (q[3] * t[1] + u[1]),
          v[2] + 2 * (q[3] * t[2] + u[2])};
}
} // namespace
Monstey_TriangleResult
Monstey_TriangleCollision(std::span<const std::uint8_t> source, const J &spec,
                          std::span<const std::uint8_t> profile) {
  Monstey_TKeys(spec,
                {"schema", "source_sha256", "owner", "profile", "vertices_cm",
                 "triangles", "max_error_cm", "placement"});
  Monstey_Require(
      spec.value("schema", "Monstey_WorldTriangleCollision_1").as_string() ==
          "Monstey_WorldTriangleCollision_1",
      "Unsupported triangle collision schema");
  const auto sha = Monstey_HashSha256(source);
  Monstey_Require(spec.contains("source_sha256") &&
                      spec.at("source_sha256").as_string() == sha,
                  "Triangle source SHA mismatch");
  Monstey_Require(source.size() >= 4 && Monstey_U32(source, 0) == 0x524a424f,
                  "Complete OBJRES required for collision authoring");
  const auto placement = spec.value("placement", J::object());
  Monstey_TKeys(placement, {"position_cm", "quaternion_xyzw"});
  const auto pp =
      Monstey_TVec(placement.value("position_cm", J::array({0, 0, 0})));
  const auto pq = Monstey_TQuaternion(
      placement.value("quaternion_xyzw", J::array({0, 0, 0, 1})));
  J vertices = J::array();
  for (const auto &v : spec.at("vertices_cm").as_array()) {
    auto rotated = Monstey_TRotate(Monstey_TVec(v), pq);
    for (std::size_t i = 0; i < 3; ++i)
      rotated[i] += pp[i];
    vertices.as_array().push_back(Monstey_TJson(rotated));
  }
  auto cooked = Monstey_TriangleCook(
      profile, spec.at("profile"), vertices, spec.at("triangles"),
      spec.value("max_error_cm", .02).as_number());
  Monstey_Physics physics(Monstey_ObjresReadChunk(source, Monstey_TPhysics));
  const auto before = physics.Describe();
  Monstey_Require(before.at("parse_limitation").is_null() &&
                      before.at("definitions").size() ==
                          before.at("shape_count").as_uint(),
                  "Qualified parsed collision definitions required");
  const auto owners = Monstey_ObjresOwners(source, false);
  std::map<std::string, std::size_t> counts;
  for (const auto &o : owners.as_array())
    ++counts[o.at("name_hash").as_string()];
  for (const auto &m : before.at("mapping_rows").as_array())
    Monstey_Require(counts[m.at("owner_hash").as_string()] == 1,
                    "Physical mapping has missing or ambiguous owner");
  const auto name = spec.value("owner", "PhysicsBSP").as_string();
  std::optional<J> owner;
  for (const auto &o : owners.as_array())
    if (o.at("name").as_string() == name) {
      Monstey_Require(!owner, "Global collision owner name is ambiguous");
      owner = o;
    }
  Monstey_Require(owner &&
                      owner->at("class_name").as_string() == "WorldCollision",
                  "Native WorldCollision owner required");
  Monstey_Require(owner->at("position") == J::array({0.0, 0.0, 0.0}) &&
                      owner->at("quaternion") == J::array({0.0, 0.0, 0.0, 1.0}),
                  "Global WorldCollision owner must have identity transform");
  const auto index = physics.DefinitionFor(Monstey_NativeHash(name));
  const auto &definition = before.at("definitions").at(index);
  Monstey_Require(definition.at("flags").as_uint() == 1 &&
                      definition.at("prefab").as_string().empty() &&
                      definition.at("identity").is_null(),
                  "Native global collision definition route is not qualified");
  for (const auto &v : definition.at("vectors_raw").as_array())
    Monstey_Require(v.is_number() && v.as_number() == 0,
                    "Global collision definition vectors must be zero");
  B identity;
  for (float f : {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f})
    Monstey_AppendU32(identity, std::bit_cast<std::uint32_t>(f));
  Monstey_Require(physics.RigidTransformBytes(index) == identity,
                  "Global collision rigid transform must be identity");
  const auto original = physics.ShapeBytes(index);
  const bool preserved = Monstey_U32(original, 0) != 0;
  B compound;
  Monstey_AppendU32(compound, 6);
  Monstey_AppendU32(compound, preserved ? 2 : 1);
  if (preserved)
    compound.insert(compound.end(), original.begin(), original.end());
  compound.insert(compound.end(), cooked.bytes.begin(), cooked.bytes.end());
  physics.ReplaceShape(index, compound);
  auto output =
      Monstey_ObjresReplaceChunk(source, Monstey_TPhysics, physics.Encode());
  Monstey_Physics checked(Monstey_ObjresReadChunk(output, Monstey_TPhysics));
  Monstey_Require(
      checked.Encode() == physics.Encode() &&
          checked.Describe().at("mapping_rows") == before.at("mapping_rows") &&
          checked.Describe().at("definitions") == before.at("definitions") &&
          Monstey_ObjresOwners(output, false) == owners,
      "Native global routing/owners changed during triangle authoring");
  Monstey_Physics unmodified(Monstey_ObjresReadChunk(source, Monstey_TPhysics));
  for (std::size_t i = 0; i < before.at("shape_count").as_uint(); ++i)
    if (i != index)
      Monstey_Require(checked.ShapeBytes(i) == unmodified.ShapeBytes(i),
                      "Unrelated physical shape changed");
  if (preserved)
    Monstey_Require(checked.ShapeBytes(index, {0}) == original,
                    "Previous global collision changed");
  auto report = J::object(
      {{"schema", "Monstey_WorldTriangleCollision_1"},
       {"status", "AUTHORED_TRIANGLE_COLLISION_RUNTIME_PENDING"},
       {"source_sha256", sha},
       {"profile_sha256", Monstey_HashSha256(profile)},
       {"owner", name},
       {"definition_index", index},
       {"cooked", cooked.report},
       {"previous_global_collision_preserved", preserved},
       {"output", J::object({{"bytes", output.size()},
                             {"sha256", Monstey_HashSha256(output)}})},
       {"spatial_registration",
        "Native PhysicsBSP world-space WorldCollision route, shape-built AABB "
        "and native broadphase queue"},
       {"global_definition_byte_exact", true},
       {"navigation_generated", false},
       {"implementation", "C++20"},
       {"python_runtime", false},
       {"runtime_verified", false}});
  return {std::move(output), std::move(report)};
}
} // namespace Monstey
