#include "Monstey_Navigation.hpp"
#include "Monstey_World.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <tuple>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
using V = std::array<double, 3>;
constexpr std::uint32_t Monstey_NoEdge = 0xffffffffu;
void Monstey_NavKeys(const J &spec,
                     std::initializer_list<std::string_view> keys) {
  Monstey_Require(spec.is_object(), "Navigation specification must be object");
  for (const auto &[key, value] : spec.as_object()) {
    (void)value;
    Monstey_Require(std::find(keys.begin(), keys.end(), key) != keys.end(),
                    "Unknown navigation field " + key);
  }
}
V Monstey_NavVector(const J &x) {
  Monstey_Require(x.is_array() && x.size() == 3,
                  "Expected three navigation coordinates");
  V out{};
  for (std::size_t i = 0; i < 3; ++i) {
    Monstey_Require(x.at(i).is_number(),
                    "Navigation coordinate is not numeric");
    const auto f = static_cast<float>(x.at(i).as_number());
    Monstey_Require(std::isfinite(f),
                    "Navigation coordinate outside finite float32");
    out[i] = f;
  }
  return out;
}
J Monstey_NavJson(const V &v) { return J::array({v[0], v[1], v[2]}); }
double Monstey_Dot(const V &a, const V &b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
V Monstey_Sub(const V &a, const V &b) {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
V Monstey_Add(const V &a, const V &b) {
  return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}
V Monstey_Mul(const V &a, double k) { return {a[0] * k, a[1] * k, a[2] * k}; }
double Monstey_Length(const V &a) { return std::sqrt(Monstey_Dot(a, a)); }
V Monstey_Cross(const V &a, const V &b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}
V Monstey_Point(const J &x) {
  Monstey_Require(x.is_array() && x.size() >= 3,
                  "Navigation vertex size invalid");
  return {x.at(0).as_number(), x.at(1).as_number(), x.at(2).as_number()};
}
std::size_t Monstey_NavIndex(const J &value, std::size_t limit) {
  Monstey_Require(value.is_integer(), "Navigation index is not integer");
  const auto n = value.as_uint();
  Monstey_Require(n < limit, "Navigation index outside native array");
  return static_cast<std::size_t>(n);
}
J Monstey_NavValidateMesh(const J &m, double weld) {
  const auto &faces = m.at("faces").as_array();
  const auto &edges = m.at("edges").as_array();
  const auto &vertices = m.at("vertices").as_array();
  std::vector<std::size_t> owner(edges.size(),
                                 std::numeric_limits<std::size_t>::max());
  for (const auto &v : vertices) {
    Monstey_Require(v.is_array() && v.size() == 4,
                    "Native nav vertex must be vector4");
    for (const auto &x : v.as_array())
      Monstey_Require(x.is_number() && std::isfinite(x.as_number()),
                      "Non-finite native nav vertex");
  }
  for (std::size_t fi = 0; fi < faces.size(); ++fi) {
    const auto &f = faces[fi];
    const auto start = f.at("startEdgeIndex").as_uint(),
               count = f.at("numEdges").as_uint();
    Monstey_Require(count >= 3 && count <= 0x7fffu &&
                        start <= edges.size() &&
                        count <= edges.size() - start,
                    "Face edge range invalid");
    for (std::size_t j = 0; j < count; ++j) {
      const auto ei = static_cast<std::size_t>(start + j);
      Monstey_Require(owner[ei] == std::numeric_limits<std::size_t>::max(),
                      "Edge belongs to multiple faces");
      owner[ei] = fi;
      Monstey_Require(
          edges[ei].at("b") ==
              edges[static_cast<std::size_t>(start + (j + 1) % count)].at("a"),
          "Face edge loop is open");
    }
  }
  std::size_t local = 0, external = 0;
  for (std::size_t i = 0; i < edges.size(); ++i) {
    const auto &e = edges[i];
    const auto a = Monstey_NavIndex(e.at("a"), vertices.size()),
               b = Monstey_NavIndex(e.at("b"), vertices.size());
    const auto oe = e.at("oppositeEdge").as_uint(),
               of = e.at("oppositeFace").as_uint();
    if (oe == Monstey_NoEdge && of == Monstey_NoEdge)
      continue;
    if (oe < edges.size() && of < faces.size()) {
      const auto &o = edges[static_cast<std::size_t>(oe)];
      Monstey_Require(owner[i] != std::numeric_limits<std::size_t>::max() &&
                          o.at("oppositeEdge").as_uint() == i &&
                          o.at("oppositeFace").as_uint() == owner[i] &&
                          owner[static_cast<std::size_t>(oe)] == of,
                      "Asymmetric local nav adjacency");
      const auto oa = Monstey_NavIndex(o.at("a"), vertices.size()),
                 ob = Monstey_NavIndex(o.at("b"), vertices.size());
      Monstey_Require(
          std::max(Monstey_Length(Monstey_Sub(Monstey_Point(vertices[a]),
                                              Monstey_Point(vertices[ob]))),
                   Monstey_Length(Monstey_Sub(Monstey_Point(vertices[b]),
                                              Monstey_Point(vertices[oa])))) <=
              weld,
          "Native seam exceeds weld threshold");
      ++local;
    } else
      ++external;
  }
  const auto fs = m.at("faceDataStriding").as_uint(),
             es = m.at("edgeDataStriding").as_uint();
  Monstey_Require(!fs || m.at("faceData").size() == faces.size() * fs,
                  "Face data stride mismatch");
  Monstey_Require(!es || m.at("edgeData").size() == edges.size() * es,
                  "Edge data stride mismatch");
  for (const auto &v : vertices)
    for (std::size_t axis = 0; axis < 3; ++axis)
      Monstey_Require(m.at("aabb").at("min").at(axis).as_number() <=
                              v.at(axis).as_number() &&
                          m.at("aabb").at("max").at(axis).as_number() >=
                              v.at(axis).as_number(),
                      "Nav vertex outside AABB");
  std::size_t caches = 0;
  const auto &dataset = m.at("clearanceCacheSeedingDataSet");
  if (!dataset.is_null())
    for (const auto &row : dataset.at("cacheDatas").as_array()) {
      ++caches;
      const auto &cache = row.at("initialCache");
      if (!cache.is_null()) {
        Monstey_Require(cache.at("faceOffsets").size() == faces.size(),
                        "Clearance face cache count mismatch");
        Monstey_Require(cache.at("vertexClearances").size() == vertices.size(),
                        "Clearance vertex cache count mismatch");
      }
    }
  return J::object({{"faces", faces.size()},
                    {"edges", edges.size()},
                    {"vertices", vertices.size()},
                    {"local_directed_adjacencies", local},
                    {"external_links_not_revalidated", external},
                    {"streaming_sets", m.at("streamingSets").size()},
                    {"clearance_caches", caches},
                    {"clearance_values_preserved_not_recomputed", true}});
}
struct Monstey_NavDocument {
  B original, owners;
  Monstey_Havok graph;
  double weld = 0;
  J changes = J::array();
  static std::span<const std::uint8_t> Tag(std::span<const std::uint8_t> b) {
    Monstey_Require(b.size() >= 28 && Monstey_U32(b, 0) == 0x49414b48 &&
                        Monstey_U32(b, 4) == 3 &&
                        Monstey_U32(b, 8) == 0xffffd00d &&
                        Monstey_U32(b, 12) == b.size() - 20 &&
                        Monstey_U32(b, b.size() - 4) == 0xffffdead,
                    "Invalid HKAI3 envelope");
    return b.subspan(16, b.size() - 20);
  }
  Monstey_NavDocument(std::span<const std::uint8_t> b,
                      std::span<const std::uint8_t> o = {})
      : original(b.begin(), b.end()), owners(o.begin(), o.end()),
        graph(Tag(b)) {
    const auto settings = One("hkaiNavMeshGenerationSettings");
    weld = graph.Plain(graph.Field(settings, "weldThreshold")).as_number();
    Monstey_Require(std::isfinite(weld) && weld > 0,
                    "Invalid native navigation weld threshold");
    Validate();
  }
  std::size_t One(std::string_view name) const {
    const auto rows = graph.Objects(name);
    Monstey_Require(rows.size() == 1,
                    "Expected one native " + std::string(name));
    return rows[0];
  }
  std::size_t Pieces() const {
    return graph.Field(One("CHavokAIDynamicNavMeshPieceArray"),
                       "DynamicNavMeshPieces");
  }
  std::size_t Strings() const {
    return graph.Field(One("CHkStringTable"), "Data");
  }
  std::size_t Piece(std::size_t i) const {
    Monstey_Require(i < graph.Node(Pieces()).elements.size(),
                    "Dynamic piece index outside array");
    return graph.Dereference(graph.Node(Pieces()).elements[i]);
  }
  std::string Name(std::size_t p) const {
    const auto i = graph.Plain(graph.Field(p, "nStringIndexName")).as_uint();
    const auto &strings = graph.Node(Strings()).elements;
    Monstey_Require(i < strings.size(),
                    "Piece string index outside native table");
    const auto name =
        graph.Plain(strings[static_cast<std::size_t>(i)]).as_string();
    Monstey_Require(Monstey_NativeHash(name) ==
                        graph.Plain(graph.Field(p, "uNameHash")).as_uint(),
                    "Dynamic navigation name/hash mismatch");
    return name;
  }
  J Owner(std::string_view name) const {
    Monstey_Require(!owners.empty(),
                    "Matching OBJRES is required for navigation owner edits");
    std::optional<J> found;
    const auto ownerRows = Monstey_ObjresOwners(owners, false);
    for (const auto &o : ownerRows.as_array())
      if (o.at("name").as_string() == name) {
        Monstey_Require(!found, "Ambiguous native navigation owner");
        found = o;
      }
    Monstey_Require(found.has_value() && found->at("class_name").as_string() ==
                                             "AINavMeshDynamicNavMeshPiece",
                    "Native navigation owner missing or wrong class");
    return *found;
  }
  J Validate() const {
    J results = J::array(), names = J::array();
    for (const auto mesh : graph.Objects("hkaiNavMesh"))
      results.as_array().push_back(
          Monstey_NavValidateMesh(graph.Plain(mesh), weld));
    std::set<std::string> seen;
    for (std::size_t i = 0; i < graph.Node(Pieces()).elements.size(); ++i) {
      const auto piece = Piece(i);
      const auto name = Name(piece);
      Monstey_Require(seen.insert(name).second,
                      "Duplicate dynamic navigation owner");
      names.as_array().push_back(name);
      const auto v = graph.Plain(graph.Field(piece, "vWorldPosition"));
      Monstey_Require(v.size() == 4, "Native piece anchor is not vector4");
      for (const auto &x : v.as_array())
        Monstey_Require(x.is_number() && std::isfinite(x.as_number()),
                        "Invalid dynamic nav anchor");
      if (!owners.empty())
        Owner(name);
    }
    return J::object({{"meshes", results},
                      {"pieces", names.size()},
                      {"owners", names},
                      {"owner_context_verified", !owners.empty()}});
  }
  void Editable(std::size_t p) const {
    const auto name = Name(p);
    Owner(name);
    Monstey_Require(graph.Node(graph.Field(p, "Points")).elements.empty(),
                    "Piece Points relocation contract is not qualified");
    const auto mesh = graph.Dereference(graph.Field(p, "hkaiNavMesh"));
    const auto plain = graph.Plain(mesh);
    Monstey_Require(plain.at("streamingSets").empty() &&
                        Monstey_NavValidateMesh(plain, weld)
                                .at("external_links_not_revalidated")
                                .as_uint() == 0,
                    "External nav links require coordinated section editing");
    const auto user = graph.Plain(One("CHavokAIUserEdgeDataArray"));
    for (const auto key :
         {"UserEdges", "ContextActionInfo", "aContextActionInfoOffsets",
          "aContextActionOwnerHashes", "aContextActionOwnerBeginIdsOffset",
          "aOwnedContextActionConnectionIds"})
      Monstey_Require(!user.contains(key) || user.at(key).empty(),
                      "Context/user links require explicit native remapping");
  }
  B Encode() const {
    const auto tag = graph.Encode();
    if (tag == B(Tag(original).begin(), Tag(original).end()))
      return original;
    B out(original.begin(), original.begin() + 12);
    Monstey_AppendU32(out, static_cast<std::uint32_t>(tag.size()));
    out.insert(out.end(), tag.begin(), tag.end());
    out.insert(out.end(), original.end() - 4, original.end());
    return out;
  }
  J Report() const {
    return J::object(
        {{"format", "HKAI3"},
         {"source_sha256", Monstey_HashSha256(original)},
         {"structure", Validate()},
         {"edits", changes},
         {"no_op_byte_exact", changes.empty() && Encode() == original},
         {"native_clearance_preserved", true},
         {"units",
          J::object({{"owner", "centimetres"}, {"nav_anchor", "metres"}})},
         {"arbitrary_baking", false},
         {"new_neighbor_connections", false},
         {"collision_generated", false},
         {"implementation", "C++20"},
         {"python_runtime", false},
         {"runtime_verified", false}});
  }
};
} // namespace
J Monstey_NavigationInspect(std::span<const std::uint8_t> source,
                            std::span<const std::uint8_t> owner) {
  return Monstey_NavDocument(source, owner).Report();
}
Monstey_NavigationResult
Monstey_NavigationApply(std::span<const std::uint8_t> source,
                        std::span<const std::uint8_t> owner, const J &spec) {
  Monstey_NavKeys(spec, {"source_sha256", "owner_source_sha256",
                         "translate_pieces", "clone_pieces"});
  if (spec.contains("source_sha256"))
    Monstey_Require(spec.at("source_sha256").as_string() ==
                        Monstey_HashSha256(source),
                    "HKAI source hash mismatch");
  if (spec.contains("owner_source_sha256"))
    Monstey_Require(spec.at("owner_source_sha256").as_string() ==
                        Monstey_HashSha256(owner),
                    "OBJRES source hash mismatch");
  Monstey_NavDocument nav(source, owner);
  auto translate = [&](std::size_t i, const V &delta) {
    const auto p = nav.Piece(i);
    nav.Editable(p);
    const auto name = nav.Name(p);
    const auto mesh = nav.graph.Dereference(nav.graph.Field(p, "hkaiNavMesh"));
    const auto canonical = nav.graph.Canonical(mesh);
    auto anchor = nav.graph.Plain(nav.graph.Field(p, "vWorldPosition"));
    const auto old = anchor;
    for (std::size_t a = 0; a < 3; ++a)
      anchor[a] = static_cast<double>(
          static_cast<float>(anchor.at(a).as_number() + delta[a] / 100));
    nav.graph.Replace(nav.graph.Field(p, "vWorldPosition"), anchor);
    const auto row = nav.Owner(name);
    const auto position =
        Monstey_Add(Monstey_NavVector(row.at("position")), delta);
    nav.owners =
        Monstey_ObjresApply(
            nav.owners,
            J::object({{"updates",
                        J::array({J::object(
                            {{"name", name},
                             {"position", Monstey_NavJson(position)}})})}}))
            .first;
    Monstey_Require(nav.graph.Canonical(mesh) == canonical,
                    "Navigation relocation changed topology/cache");
    nav.changes.as_array().push_back(
        J::object({{"operation", "translate_piece"},
                   {"index", i},
                   {"owner", name},
                   {"delta_cm", Monstey_NavJson(delta)},
                   {"old_anchor_m", old},
                   {"new_anchor_m", anchor},
                   {"topology_and_cache_unchanged", true},
                   {"neighbor_connections_created", false}}));
  };
  if (spec.contains("translate_pieces"))
    for (const auto &entry : spec.at("translate_pieces").as_array()) {
      Monstey_NavKeys(entry, {"index", "delta_cm"});
      translate(Monstey_NavIndex(entry.at("index"),
                                 nav.graph.Node(nav.Pieces()).elements.size()),
                Monstey_NavVector(entry.at("delta_cm")));
    }
  if (spec.contains("clone_pieces"))
    for (const auto &entry : spec.at("clone_pieces").as_array()) {
      Monstey_NavKeys(entry, {"index", "new_name", "delta_cm"});
      const auto i = Monstey_NavIndex(
                     entry.at("index"),
                     nav.graph.Node(nav.Pieces()).elements.size()),
                 p = nav.Piece(i);
      nav.Editable(p);
      const auto name = nav.Name(p);
      const auto newName = entry.at("new_name").as_string();
      Monstey_Require(!newName.empty() &&
                          newName.find('\0') == std::string::npos,
                      "Invalid cloned navigation owner name");
      const auto hash = Monstey_NativeHash(newName);
      const auto existingOwners = Monstey_ObjresOwners(nav.owners, false);
      for (const auto &row : existingOwners.as_array())
        Monstey_Require(Monstey_NativeHash(row.at("name").as_string()) != hash,
                        "New navigation owner collides with existing hash");
      const auto oldOwner = nav.Owner(name);
      Monstey_Require(oldOwner.at("prefix_offset").as_uint() == 65535,
                      "Prefixed owner clone requires explicit prefix remap");
      const auto clone =
                     nav.graph.Clone(nav.graph.Node(nav.Pieces()).elements[i]),
                 newPiece = nav.graph.Dereference(clone);
      const auto strings = nav.Strings();
      for (const auto s : nav.graph.Node(strings).elements)
        Monstey_Require(nav.graph.Plain(s).as_string() != newName,
                        "Duplicate navigation string");
      const auto stringType =
          nav.graph
              .Types()[nav.graph.EffectiveType(nav.graph.Node(strings).type)]
              .pointer;
      const auto si = nav.graph.Node(strings).elements.size(),
                 sn = nav.graph.New(stringType, newName);
      nav.graph.Node(strings).elements.push_back(sn);
      nav.graph.Replace(nav.graph.Field(newPiece, "nStringIndexName"), si);
      nav.graph.Replace(nav.graph.Field(newPiece, "uNameHash"), hash);
      nav.graph.Node(nav.Pieces()).elements.push_back(clone);
      nav.owners =
          Monstey_ObjresApply(
              nav.owners,
              J::object(
                  {{"clones", J::array({J::object({{"source_name", name},
                                                   {"name", newName}})})}}))
              .first;
      const auto ni = nav.graph.Node(nav.Pieces()).elements.size() - 1;
      translate(ni, Monstey_NavVector(entry.at("delta_cm")));
      nav.changes.as_array().push_back(
          J::object({{"operation", "clone_piece"},
                     {"source_index", i},
                     {"new_index", ni},
                     {"source_owner", name},
                     {"new_owner", newName},
                     {"native_clearance_cloned", true}}));
    }
  auto result =
      Monstey_NavigationResult{nav.Encode(), nav.owners, nav.Report()};
  Monstey_NavDocument checked(result.navigation, result.owner);
  result.report["item_ptch_audit"] = checked.graph.Audit();
  return result;
}
namespace {
J Monstey_NavProfile(const J &spec) {
  Monstey_NavKeys(spec,
                  {"name", "radius_cm", "height_cm", "max_slope_degrees"});
  Monstey_Require(spec.contains("radius_cm") && spec.contains("height_cm"),
                  "Explicit actor radius and height are required");
  const auto radius = spec.at("radius_cm").as_number(),
             height = spec.at("height_cm").as_number(),
             slope = spec.value("max_slope_degrees", 40).as_number();
  Monstey_Require(std::isfinite(radius) && std::isfinite(height) &&
                      std::isfinite(slope) && radius > 0 && height > 0 &&
                      slope >= 0 && slope < 89,
                  "Invalid actor profile");
  return J::object({{"name", spec.value("name", "explicit_actor")},
                    {"radius_cm", radius},
                    {"height_cm", height},
                    {"max_slope_degrees", slope}});
}
J Monstey_NavConnectivity(const J &m) {
  const auto &faces = m.at("faces").as_array();
  const auto &edges = m.at("edges").as_array();
  std::vector<std::set<std::size_t>> adj(faces.size());
  for (std::size_t fi = 0; fi < faces.size(); ++fi) {
    const auto start = faces[fi].at("startEdgeIndex").as_uint(),
               count = faces[fi].at("numEdges").as_uint();
    for (std::size_t j = 0; j < count; ++j) {
      const auto of = edges[static_cast<std::size_t>(start + j)]
                          .at("oppositeFace")
                          .as_uint();
      if (of < faces.size())
        adj[fi].insert(static_cast<std::size_t>(of));
    }
  }
  J components = J::array(), sizes = J::array();
  std::set<std::size_t> remaining;
  for (std::size_t i = 0; i < faces.size(); ++i)
    remaining.insert(i);
  while (!remaining.empty()) {
    std::set<std::size_t> part;
    std::vector<std::size_t> stack{*remaining.begin()};
    while (!stack.empty()) {
      const auto a = stack.back();
      stack.pop_back();
      if (!part.insert(a).second)
        continue;
      stack.insert(stack.end(), adj[a].begin(), adj[a].end());
    }
    J row = J::array();
    for (const auto i : part) {
      row.as_array().push_back(i);
      remaining.erase(i);
    }
    sizes.as_array().push_back(row.size());
    components.as_array().push_back(row);
  }
  return J::object({{"component_count", components.size()},
                    {"component_sizes", sizes},
                    {"components", components}});
}
double Monstey_SegmentDistance(const V &p, const V &a, const V &b) {
  const auto d = Monstey_Sub(b, a);
  const auto den = Monstey_Dot(d, d);
  const auto t =
      den == 0 ? 0
               : std::clamp(Monstey_Dot(Monstey_Sub(p, a), d) / den, 0.0, 1.0);
  return Monstey_Length(Monstey_Sub(p, Monstey_Add(a, Monstey_Mul(d, t))));
}
double Monstey_Orient(const V &a, const V &b, const V &c) {
  return (b[0] - a[0]) * (c[2] - a[2]) - (b[2] - a[2]) * (c[0] - a[0]);
}
double Monstey_SegmentXZ(V a, V b, V c, V d) {
  a[1] = b[1] = c[1] = d[1] = 0;
  if (Monstey_Orient(a, b, c) * Monstey_Orient(a, b, d) < 0 &&
      Monstey_Orient(c, d, a) * Monstey_Orient(c, d, b) < 0)
    return 0;
  return std::min(
      {Monstey_SegmentDistance(a, c, d), Monstey_SegmentDistance(b, c, d),
       Monstey_SegmentDistance(c, a, b), Monstey_SegmentDistance(d, a, b)});
}
J Monstey_NavRoute(const J &m, const J &route, const J &profile,
                   double headroom) {
  const auto &vertices = m.at("vertices").as_array();
  const auto &edges = m.at("edges").as_array();
  std::vector<V> points;
  for (const auto &p : route.as_array())
    points.push_back(Monstey_NavVector(p));
  Monstey_Require(points.size() >= 2 && points.size() <= 100000,
                  "Route needs 2..100000 points");
  std::vector<std::vector<V>> faces;
  for (const auto &face : m.at("faces").as_array()) {
    std::vector<V> poly;
    const auto start = face.at("startEdgeIndex").as_uint(),
               n = face.at("numEdges").as_uint();
    for (std::size_t i = 0; i < n; ++i)
      poly.push_back(Monstey_Mul(
          Monstey_Point(vertices[static_cast<std::size_t>(
              edges[static_cast<std::size_t>(start + i)].at("a").as_uint())]),
          100));
    faces.push_back(poly);
  }
  auto support = [&](const V &p) {
    for (const auto &poly : faces)
      for (std::size_t i = 1; i + 1 < poly.size(); ++i) {
        const auto &a = poly[0], &b = poly[i], &c = poly[i + 1];
        const auto den = Monstey_Orient(a, b, c);
        if (std::abs(den) < 1e-9)
          continue;
        const auto u = Monstey_Orient(p, b, c) / den,
                   v = Monstey_Orient(a, p, c) / den, w = 1 - u - v;
        if (std::min({u, v, w}) >= -1e-7 &&
            std::abs(u * a[1] + v * b[1] + w * c[1] - p[1]) < 0.1)
          return true;
      }
    return false;
  };
  const auto radius = profile.at("radius_cm").as_number(),
             spacing = std::max(1.0, std::min(10.0, radius / 4));
  double minimum = std::numeric_limits<double>::infinity();
  std::size_t samples = 0;
  bool supported = true;
  for (std::size_t i = 0; i + 1 < points.size(); ++i) {
    const auto &a = points[i], &b = points[i + 1];
    for (const auto &e : edges)
      if (e.at("oppositeFace").as_uint() == Monstey_NoEdge) {
        const auto
            c = Monstey_Mul(
                Monstey_Point(
                    vertices[static_cast<std::size_t>(e.at("a").as_uint())]),
                100),
            d = Monstey_Mul(
                Monstey_Point(
                    vertices[static_cast<std::size_t>(e.at("b").as_uint())]),
                100);
        minimum = std::min(minimum, Monstey_SegmentXZ(a, b, c, d));
      }
    const auto steps =
        std::max(1.0, std::ceil(Monstey_Length(Monstey_Sub(b, a)) / spacing));
    Monstey_Require(steps <= 1000000 && samples + steps <= 4000000,
                    "Route sampling exceeds bound");
    for (std::size_t j = 0; j <= static_cast<std::size_t>(steps); ++j) {
      supported = support(Monstey_Add(
                      a, Monstey_Mul(Monstey_Sub(b, a),
                                     static_cast<double>(j) / steps))) &&
                  supported;
      ++samples;
    }
  }
  const auto height = profile.at("height_cm").as_number();
  const bool heightOk = headroom >= height;
  return J::object(
      {{"pass_", supported && heightOk && std::isfinite(minimum) &&
                     minimum + 1e-5 >= radius},
       {"geometric_support", supported},
       {"min_boundary_distance_cm", std::isfinite(minimum) ? J(minimum) : J()},
       {"radius_cm", radius},
       {"headroom_cm", headroom},
       {"height_cm", height},
       {"height_ok", heightOk},
       {"samples", samples},
       {"headroom_source", "author_declared_not_runtime_collision_query"},
       {"runtime_verified", false}});
}
J Monstey_NavSurface(const J &m, std::size_t oldFaces, double headroom,
                     double weld) {
  struct Triangle {
    std::size_t face;
    std::array<V, 3> v;
    double lo, hi, zlo, zhi;
  };
  std::vector<Triangle> triangles;
  const auto &vertices = m.at("vertices").as_array();
  const auto &edges = m.at("edges").as_array();
  const auto &faces = m.at("faces").as_array();
  for (std::size_t fi = 0; fi < faces.size(); ++fi) {
    const auto start = faces[fi].at("startEdgeIndex").as_uint(),
               count = faces[fi].at("numEdges").as_uint();
    const auto first = Monstey_Point(vertices[static_cast<std::size_t>(
        edges[static_cast<std::size_t>(start)].at("a").as_uint())]);
    for (std::size_t i = 1; i + 1 < count; ++i) {
      std::array<V, 3> v{
          first,
          Monstey_Point(vertices[static_cast<std::size_t>(
              edges[static_cast<std::size_t>(start + i)].at("a").as_uint())]),
          Monstey_Point(vertices[static_cast<std::size_t>(
              edges[static_cast<std::size_t>(start + i + 1)]
                  .at("a")
                  .as_uint())])};
      triangles.push_back({fi, v, std::min({v[0][0], v[1][0], v[2][0]}),
                           std::max({v[0][0], v[1][0], v[2][0]}),
                           std::min({v[0][2], v[1][2], v[2][2]}),
                           std::max({v[0][2], v[1][2], v[2][2]})});
    }
  }
  std::sort(triangles.begin(), triangles.end(),
            [](const auto &a, const auto &b) { return a.lo < b.lo; });
  std::vector<std::size_t> active;
  std::size_t checked = 0, candidates = 0;
  auto height = [](const auto &tri, const V &p) {
    const auto den = Monstey_Orient(tri[0], tri[1], tri[2]);
    Monstey_Require(std::abs(den) > 1e-12, "Vertical/degenerate nav floor");
    const auto u = Monstey_Orient(p, tri[1], tri[2]) / den,
               v = Monstey_Orient(tri[0], p, tri[2]) / den;
    return u * tri[0][1] + v * tri[1][1] + (1 - u - v) * tri[2][1];
  };
  for (std::size_t ti = 0; ti < triangles.size(); ++ti) {
    const auto &row = triangles[ti];
    active.erase(std::remove_if(
                     active.begin(), active.end(),
                     [&](auto i) { return triangles[i].hi < row.lo - 1e-10; }),
                 active.end());
    for (const auto oi : active) {
      const auto &other = triangles[oi];
      if ((row.face < oldFaces && other.face < oldFaces) ||
          row.face == other.face || row.zhi < other.zlo || other.zhi < row.zlo)
        continue;
      Monstey_Require(++candidates <= 20000000,
                      "Navigation overlap workload exceeds bound");
      std::vector<V> poly(row.v.begin(), row.v.end());
      const auto sign =
          Monstey_Orient(other.v[0], other.v[1], other.v[2]) > 0 ? 1.0 : -1.0;
      for (std::size_t edge = 0; edge < 3 && !poly.empty(); ++edge) {
        const auto &a = other.v[edge], &b = other.v[(edge + 1) % 3];
        std::vector<V> clipped;
        auto prev = poly.back();
        auto fp = Monstey_Orient(a, b, prev) * sign;
        for (const auto &current : poly) {
          const auto fc = Monstey_Orient(a, b, current) * sign;
          if ((fc >= -1e-12) != (fp >= -1e-12))
            clipped.push_back(Monstey_Add(
                prev, Monstey_Mul(Monstey_Sub(current, prev), fp / (fp - fc))));
          if (fc >= -1e-12)
            clipped.push_back(current);
          prev = current;
          fp = fc;
        }
        poly = std::move(clipped);
      }
      if (poly.size() < 3)
        continue;
      double area = 0;
      for (std::size_t i = 0; i < poly.size(); ++i)
        area += poly[i][0] * poly[(i + 1) % poly.size()][2] -
                poly[(i + 1) % poly.size()][0] * poly[i][2];
      if (std::abs(area) / 2 < 1e-10)
        continue;
      ++checked;
      double minimum = std::numeric_limits<double>::infinity(),
             maximum = -minimum, absmin = minimum;
      for (const auto &p : poly) {
        const auto d = height(row.v, p) - height(other.v, p);
        minimum = std::min(minimum, d);
        maximum = std::max(maximum, d);
        absmin = std::min(absmin, std::abs(d));
      }
      Monstey_Require(!(minimum <= 1e-7 && maximum >= -1e-7),
                      "Overlapping/intersecting walkable floor faces");
      Monstey_Require(absmin + 1e-7 >= headroom,
                      "Stacked floors contradict actor headroom");
    }
    active.push_back(ti);
  }
  std::set<std::size_t> oldVertices;
  for (std::size_t i = 0; i < oldFaces; ++i) {
    const auto start = faces[i].at("startEdgeIndex").as_uint(),
               count = faces[i].at("numEdges").as_uint();
    for (std::size_t j = 0; j < count; ++j)
      oldVertices.insert(static_cast<std::size_t>(
          edges[static_cast<std::size_t>(start + j)].at("a").as_uint()));
  }
  std::vector<std::pair<double, std::size_t>> ordered;
  for (std::size_t i = 0; i < vertices.size(); ++i)
    ordered.emplace_back(vertices[i].at(0).as_number(), i);
  std::sort(ordered.begin(), ordered.end());
  std::size_t junctions = 0;
  for (std::size_t fi = 0; fi < faces.size(); ++fi) {
    const auto start = faces[fi].at("startEdgeIndex").as_uint(),
               count = faces[fi].at("numEdges").as_uint();
    for (std::size_t j = 0; j < count; ++j) {
      const auto &e = edges[static_cast<std::size_t>(start + j)];
      const auto ia = static_cast<std::size_t>(e.at("a").as_uint()),
                 ib = static_cast<std::size_t>(e.at("b").as_uint());
      const auto a = Monstey_Point(vertices[ia]),
                 b = Monstey_Point(vertices[ib]);
      auto it = std::lower_bound(
          ordered.begin(), ordered.end(),
          std::pair<double, std::size_t>{std::min(a[0], b[0]) - weld, 0});
      for (; it != ordered.end() && it->first <= std::max(a[0], b[0]) + weld;
           ++it) {
        const auto vi = it->second;
        if ((fi < oldFaces && oldVertices.contains(vi)) || vi == ia || vi == ib)
          continue;
        const auto v = Monstey_Point(vertices[vi]);
        if (std::min(Monstey_Length(Monstey_Sub(v, a)),
                     Monstey_Length(Monstey_Sub(v, b))) <= weld)
          continue;
        if (v[2] >= std::min(a[2], b[2]) - weld &&
            v[2] <= std::max(a[2], b[2]) + weld &&
            v[1] >= std::min(a[1], b[1]) - weld &&
            v[1] <= std::max(a[1], b[1]) + weld) {
          Monstey_Require(++junctions <= 20000000,
                          "Navigation junction workload exceeds bound");
          Monstey_Require(
              Monstey_SegmentDistance(v, a, b) > weld,
              "Nonconforming T junction: split input edge at vertex");
        }
      }
    }
  }
  return J::object({{"projected_overlap_pairs_checked", checked},
                    {"new_edge_vertex_checks", junctions},
                    {"overlapping_floors", false},
                    {"new_t_junctions", false}});
}
J Monstey_NavLazyCache(Monstey_Havok &g, std::size_t mesh) {
  const auto plain = g.Plain(mesh);
  const auto holder = g.Field(mesh, "clearanceCacheSeedingDataSet");
  Monstey_Require(g.Node(holder).target.has_value(),
                  "Native clearance cache donor required");
  const auto dataset = g.Dereference(holder),
             rows = g.Field(dataset, "cacheDatas");
  Monstey_Require(!g.Node(rows).elements.empty(),
                  "No native clearance categories");
  J report = J::array();
  const auto rowIds = g.Node(rows).elements;
  for (const auto row : rowIds) {
    const auto cache = g.Dereference(g.Field(row, "initialCache"));
    const auto ceiling =
        g.Plain(g.Field(cache, "clearanceCeiling")).as_number();
    Monstey_Require(std::isfinite(ceiling) && ceiling > 0,
                    "Invalid native clearance ceiling");
    J faces = J::array(), mcp = J::array(), vertices = J::array();
    for (std::size_t i = 0; i < plain.at("faces").size(); ++i)
      faces.as_array().push_back(Monstey_NoEdge);
    for (std::size_t i = 0; i < plain.at("edges").size(); ++i)
      mcp.as_array().push_back(
          J::object({{"interpolant", 0}, {"clearance", 255}}));
    for (std::size_t i = 0; i < plain.at("vertices").size(); ++i)
      vertices.as_array().push_back(255);
    g.Replace(g.Field(cache, "faceOffsets"), faces);
    g.Replace(g.Field(cache, "edgePairClearances"), J::array());
    g.Replace(g.Field(cache, "mcpData"), mcp);
    g.Replace(g.Field(cache, "vertexClearances"), vertices);
    g.Replace(g.Field(cache, "unusedEdgePairElements"), 0);
    g.Replace(g.Field(cache, "uncalculatedFacesLowerBound"), 0);
    report.as_array().push_back(
        J::object({{"id", g.Plain(g.Field(row, "id"))},
                   {"info", g.Plain(g.Field(row, "info"))},
                   {"infoMask", g.Plain(g.Field(row, "infoMask"))},
                   {"ceiling_m", ceiling},
                   {"faces", faces.size()},
                   {"edges", mcp.size()},
                   {"vertices", vertices.size()}}));
  }
  return J::object({{"mode", "native_lazy_recompute"},
                    {"cache_categories", report},
                    {"stale_values_retained", false},
                    {"initializer_rva", "0x5A3AA0"},
                    {"consumer_rva", "0x3BE878"},
                    {"engine_execution_verified", false}});
}
} // namespace
Monstey_NavigationResult
Monstey_NavigationBuild(std::span<const std::uint8_t> source, const J &spec,
                        std::span<const std::uint8_t> owner,
                        std::span<const std::uint8_t> cacheTemplate) {
  Monstey_NavKeys(spec, {"source_sha256", "mesh_index", "mode", "vertices_cm",
                         "triangles", "actor", "headroom_cm", "weld_cm",
                         "require_connection", "routes", "input_space",
                         "cache_template_sha256"});
  if (spec.contains("source_sha256"))
    Monstey_Require(spec.at("source_sha256").as_string() ==
                        Monstey_HashSha256(source),
                    "Navigation source SHA mismatch");
  Monstey_NavDocument nav(source, owner);
  const auto meshes = nav.graph.Objects("hkaiNavMesh");
  const auto mi = Monstey_NavIndex(spec.value("mesh_index", 0), meshes.size()),
             mesh = meshes[mi];
  bool completedTypeHashes = false;
  const auto cacheHolderBefore =
      nav.graph.Field(mesh, "clearanceCacheSeedingDataSet");
  if (!nav.graph.Node(cacheHolderBefore).target && !cacheTemplate.empty()) {
    if (spec.contains("cache_template_sha256"))
      Monstey_Require(spec.at("cache_template_sha256").as_string() ==
                          Monstey_HashSha256(cacheTemplate),
                      "Clearance donor hash mismatch");
    const auto completed = Monstey_Havok::CompleteTypeHashes(
        nav.graph.Original(), Monstey_NavDocument::Tag(cacheTemplate));
    completedTypeHashes = completed != nav.graph.Original();
    nav.graph = Monstey_Havok(completed);
  }
  const auto original = nav.graph.Plain(mesh);
  std::vector<J> unchanged;
  for (const auto m : meshes)
    unchanged.push_back(nav.graph.Canonical(m));
  const auto mode = spec.value("mode", "append").as_string(),
             space = spec.value("input_space", "mesh_local").as_string();
  Monstey_Require(mode == "append" || mode == "replace",
                  "Mode must be append or replace");
  Monstey_Require(space == "mesh_local" || space == "world",
                  "Input space must be mesh_local or world");
  Monstey_Require(
      original.at("streamingSets").empty() &&
          Monstey_NavValidateMesh(original, nav.weld)
                  .at("external_links_not_revalidated")
                  .as_uint() == 0,
      "External packed nav adjacency requires coordinated section edits");
  for (const auto &f : original.at("faces").as_array())
    Monstey_Require(f.at("numUserEdges").as_uint() == 0,
                    "User-edge ownership requires coordinated update");
  const auto user = nav.graph.Plain(nav.One("CHavokAIUserEdgeDataArray"));
  for (const auto key :
       {"UserEdges", "ContextActionInfo", "aContextActionInfoOffsets",
        "aContextActionOwnerHashes", "aContextActionOwnerBeginIdsOffset",
        "aOwnedContextActionConnectionIds"})
    Monstey_Require(!user.contains(key) || user.at(key).empty(),
                    "Context/user links require remapping");
  const auto profile = Monstey_NavProfile(spec.at("actor"));
  const auto headroom = spec.value("headroom_cm", 0).as_number(),
             weld = spec.value("weld_cm", 0.01).as_number();
  Monstey_Require(std::isfinite(headroom) &&
                      headroom >= profile.at("height_cm").as_number(),
                  "Declared headroom below actor height");
  Monstey_Require(std::isfinite(weld) && weld > 0 && weld <= nav.weld * 100,
                  "Weld exceeds native threshold");
  Monstey_Require(spec.at("vertices_cm").size() >= 3 &&
                      spec.at("vertices_cm").size() <= 1000000 &&
                      spec.at("triangles").size() > 0 &&
                      spec.at("triangles").size() <= 1000000,
                  "Geometry count outside supported bounds");
  V anchor{};
  J pieceName;
  for (std::size_t i = 0; i < nav.graph.Node(nav.Pieces()).elements.size();
       ++i) {
    const auto p = nav.Piece(i);
    if (nav.graph.Dereference(nav.graph.Field(p, "hkaiNavMesh")) == mesh) {
      anchor =
          Monstey_Point(nav.graph.Plain(nav.graph.Field(p, "vWorldPosition")));
      pieceName = nav.Name(p);
      Monstey_Require(
          nav.graph.Node(nav.graph.Field(p, "Points")).elements.empty(),
          "Dynamic Points spatial contract requires preservation");
      break;
    }
  }
  J plain = original;
  if (mode == "replace")
    for (const auto key :
         {"vertices", "faces", "edges", "faceData", "edgeData"})
      plain[key] = J::array();
  const auto oldFaceCount = plain.at("faces").size(),
             oldEdgeCount = plain.at("edges").size(),
             oldVertexCount = plain.at("vertices").size();
  auto &vertices = plain["vertices"].as_array();
  const auto quant = weld / 100;
  using Cell = std::array<std::int64_t, 3>;
  std::map<Cell, std::vector<std::size_t>> buckets;
  auto cell = [&](const V &v) {
    Cell c{};
    for (std::size_t i = 0; i < 3; ++i) {
      const auto q = std::floor(v[i] / quant);
      Monstey_Require(std::isfinite(q) && std::abs(q) < 9e18,
                      "Weld cell exceeds int64");
      c[i] = static_cast<std::int64_t>(q);
    }
    return c;
  };
  for (std::size_t i = 0; i < vertices.size(); ++i)
    buckets[cell(Monstey_Point(vertices[i]))].push_back(i);
  std::vector<std::size_t> mapping;
  for (const auto &input : spec.at("vertices_cm").as_array()) {
    auto v = Monstey_NavVector(input);
    if (space == "world")
      v = Monstey_Sub(v, Monstey_Mul(anchor, 100));
    for (auto &x : v)
      x = static_cast<float>(x / 100);
    const auto c = cell(v);
    std::optional<std::pair<double, std::size_t>> best;
    for (int x = -1; x <= 1; ++x)
      for (int y = -1; y <= 1; ++y)
        for (int z = -1; z <= 1; ++z) {
          const auto it = buckets.find({c[0] + x, c[1] + y, c[2] + z});
          if (it != buckets.end())
            for (const auto i : it->second) {
              const auto distance =
                  Monstey_Length(Monstey_Sub(v, Monstey_Point(vertices[i])));
              const auto candidate = std::make_pair(distance, i);
              if (distance <= quant && (!best || candidate < *best))
                best = candidate;
            }
        }
    if (best)
      mapping.push_back(best->second);
    else {
      const auto i = vertices.size();
      vertices.push_back(J::array({v[0], v[1], v[2], 0.0}));
      buckets[c].push_back(i);
      mapping.push_back(i);
    }
  }
  std::vector<std::array<std::size_t, 3>> added;
  std::set<std::array<std::size_t, 3>> seen;
  std::size_t steep = 0;
  for (const auto &tri : spec.at("triangles").as_array()) {
    Monstey_Require(tri.is_array() && tri.size() == 3,
                    "Triangle must have three indices");
    std::array<std::size_t, 3> ids{
        mapping[Monstey_NavIndex(tri.at(0), mapping.size())],
        mapping[Monstey_NavIndex(tri.at(1), mapping.size())],
        mapping[Monstey_NavIndex(tri.at(2), mapping.size())]};
    Monstey_Require(ids[0] != ids[1] && ids[1] != ids[2] && ids[2] != ids[0],
                    "Degenerate welded triangle");
    const auto a = Monstey_Point(vertices[ids[0]]),
               b = Monstey_Point(vertices[ids[1]]),
               c = Monstey_Point(vertices[ids[2]]);
    const auto normal = Monstey_Cross(Monstey_Sub(b, a), Monstey_Sub(c, a));
    const auto length = Monstey_Length(normal);
    Monstey_Require(length > 1e-10 && normal[1] > 0,
                    "Walkable face must have nonzero area and upward winding");
    if (normal[1] / length <
        std::cos(profile.at("max_slope_degrees").as_number() *
                 3.14159265358979323846 / 180)) {
      ++steep;
      continue;
    }
    auto sorted = ids;
    std::sort(sorted.begin(), sorted.end());
    Monstey_Require(seen.insert(sorted).second, "Duplicate triangle");
    added.push_back(ids);
  }
  Monstey_Require(!added.empty(), "No walkable triangles remain");
  using Edge = std::pair<std::size_t, std::size_t>;
  std::map<Edge, std::pair<std::size_t, std::size_t>> directed;
  std::map<Edge, std::size_t> undirected;
  auto &faces = plain["faces"].as_array();
  auto &edges = plain["edges"].as_array();
  for (std::size_t fi = 0; fi < faces.size(); ++fi) {
    const auto start = faces[fi].at("startEdgeIndex").as_uint(),
               count = faces[fi].at("numEdges").as_uint();
    for (std::size_t j = 0; j < count; ++j) {
      const auto ei = static_cast<std::size_t>(start + j);
      const auto a = static_cast<std::size_t>(edges[ei].at("a").as_uint()),
                 b = static_cast<std::size_t>(edges[ei].at("b").as_uint());
      Monstey_Require(
          directed
              .emplace(Edge{a, b}, std::pair<std::size_t, std::size_t>{ei, fi})
              .second,
          "Source has duplicate directed edge");
      ++undirected[std::minmax(a, b)];
    }
  }
  J joins = J::array();
  for (const auto &ids : added) {
    const auto fi = faces.size(), start = edges.size();
    faces.push_back(J::object({{"startEdgeIndex", start},
                               {"startUserEdgeIndex", -1},
                               {"numEdges", 3},
                               {"numUserEdges", 0},
                               {"clusterIndex", -1},
                               {"padding", 0xcdcd}}));
    for (std::size_t i = 0; i < plain.at("faceDataStriding").as_uint(); ++i)
      plain["faceData"].as_array().push_back(-1);
    for (std::size_t j = 0; j < 3; ++j) {
      const auto a = ids[j], b = ids[(j + 1) % 3], ei = edges.size();
      const Edge key = std::minmax(a, b);
      Monstey_Require(!directed.contains({a, b}) && undirected[key] < 2,
                      "Overlapping/same-winding or non-manifold edge");
      auto edge = J::object({{"a", a},
                             {"b", b},
                             {"oppositeEdge", Monstey_NoEdge},
                             {"oppositeFace", Monstey_NoEdge},
                             {"flags", 4},
                             {"paddingByte", 0},
                             {"userEdgeCost", 0.0}});
      if (const auto it = directed.find({b, a}); it != directed.end()) {
        const auto [oe, of] = it->second;
        Monstey_Require(edges[oe].at("oppositeFace").as_uint() ==
                            Monstey_NoEdge,
                        "Cannot stitch into occupied edge");
        edge["oppositeEdge"] = oe;
        edge["oppositeFace"] = of;
        edges[oe]["oppositeEdge"] = ei;
        edges[oe]["oppositeFace"] = fi;
        if (of < oldFaceCount)
          joins.as_array().push_back(J::object(
              {{"old_face", of},
               {"old_edge", oe},
               {"new_face", fi},
               {"new_edge", ei},
               {"width_cm",
                Monstey_Length(Monstey_Sub(Monstey_Point(vertices[a]),
                                           Monstey_Point(vertices[b]))) *
                    100}}));
      }
      edges.push_back(edge);
      for (std::size_t k = 0; k < plain.at("edgeDataStriding").as_uint(); ++k)
        plain["edgeData"].as_array().push_back(0);
      directed[{a, b}] = {ei, fi};
      ++undirected[key];
    }
  }
  Monstey_Require(mode != "append" ||
                      !spec.value("require_connection", true).as_bool() ||
                      !joins.empty(),
                  "New floor disconnected from original mesh");
  Monstey_Require(faces.size() < 0x400000 && edges.size() < 0x400000,
                  "Packed local index limit exceeded");
  const auto surface =
      Monstey_NavSurface(plain, oldFaceCount,
                         profile.at("height_cm").as_number() / 100, weld / 100);
  J lo = J::array(), hi = J::array();
  for (std::size_t i = 0; i < 3; ++i) {
    double mn = std::numeric_limits<double>::infinity(), mx = -mn;
    for (const auto &v : vertices) {
      mn = std::min(mn, v.at(i).as_number());
      mx = std::max(mx, v.at(i).as_number());
    }
    lo.as_array().push_back(mn - 2);
    hi.as_array().push_back(mx + 2);
  }
  lo.as_array().push_back(-2.0);
  hi.as_array().push_back(2.0);
  for (const auto key : {"faces", "edges", "vertices", "faceData", "edgeData"})
    nav.graph.Replace(nav.graph.Field(mesh, key), plain.at(key));
  nav.graph.Replace(nav.graph.Field(mesh, "aabb"),
                    J::object({{"min", lo}, {"max", hi}}));
  nav.graph.Replace(nav.graph.Field(mesh, "erosionRadius"), 0.0);
  std::string provenance = Monstey_HashSha256(source);
  const auto holder = nav.graph.Field(mesh, "clearanceCacheSeedingDataSet");
  if (!nav.graph.Node(holder).target) {
    Monstey_Require(!cacheTemplate.empty(),
                    "Empty template requires native clearance category donor");
    provenance = Monstey_HashSha256(cacheTemplate);
    if (spec.contains("cache_template_sha256"))
      Monstey_Require(spec.at("cache_template_sha256").as_string() ==
                          provenance,
                      "Clearance donor hash mismatch");
    Monstey_NavDocument donor(cacheTemplate);
    std::optional<std::size_t> native;
    for (const auto dm : donor.graph.Objects("hkaiNavMesh"))
      if (donor.graph
              .Node(donor.graph.Field(dm, "clearanceCacheSeedingDataSet"))
              .target) {
        native = dm;
        break;
      }
    Monstey_Require(native.has_value(),
                    "Clearance donor has no native categories");
    nav.graph.Replace(holder, donor.graph.Plain(donor.graph.Field(
                                  *native, "clearanceCacheSeedingDataSet")));
  }
  auto cache = Monstey_NavLazyCache(nav.graph, mesh);
  cache["category_source_sha256"] = provenance;
  const auto finalPlain = nav.graph.Plain(mesh);
  auto validation = Monstey_NavValidateMesh(finalPlain, nav.weld);
  validation["clearance_values_preserved_not_recomputed"] = false;
  validation["clearance_state"] = "native_lazy_initialization";
  J routes = J::array();
  if (spec.contains("routes"))
    for (const auto &route : spec.at("routes").as_array()) {
      Monstey_NavKeys(route, {"name", "points_cm", "require_pass"});
      auto points = route.at("points_cm");
      if (space == "world")
        for (auto &p : points.as_array())
          p = Monstey_NavJson(
              Monstey_Sub(Monstey_NavVector(p), Monstey_Mul(anchor, 100)));
      auto result = Monstey_NavRoute(finalPlain, points, profile, headroom);
      result["name"] = route.value("name", "route");
      result["input_space"] = space;
      if (route.value("require_pass", true).as_bool())
        Monstey_Require(result.at("pass_").as_bool(),
                        "Route cannot be traversed by declared actor: " +
                            result.dump());
      routes.as_array().push_back(result);
    }
  for (std::size_t i = 0; i < meshes.size(); ++i)
    if (i != mi)
      Monstey_Require(nav.graph.Canonical(meshes[i]) == unchanged[i],
                      "Unrelated native navmesh changed");
  auto data = nav.Encode();
  Monstey_NavDocument checked(data, owner);
  auto report = J::object(
      {{"status", "NAVIGATION_GRAPH_AUTHORED_RUNTIME_PENDING"},
       {"source_sha256", Monstey_HashSha256(source)},
       {"output_sha256", Monstey_HashSha256(data)},
       {"mesh_index", mi},
       {"owner", pieceName},
       {"world_anchor_m", Monstey_NavJson(anchor)},
       {"input_space", space},
       {"mode", mode},
       {"actor", profile},
       {"headroom_cm", headroom},
       {"units", J::object({{"input", "centimetres_Y_up"},
                            {"serialized", "metres_Y_up"}})},
       {"counts", J::object({{"old_faces", oldFaceCount},
                             {"new_faces", faces.size()},
                             {"old_edges", oldEdgeCount},
                             {"new_edges", edges.size()},
                             {"old_vertices", oldVertexCount},
                             {"new_vertices", vertices.size()},
                             {"rejected_steep_triangles", steep}})},
       {"seams", joins},
       {"connectivity", Monstey_NavConnectivity(finalPlain)},
       {"clearance", cache},
       {"validation", validation},
       {"surface_validation", surface},
       {"routes", routes},
       {"item_ptch_audit", checked.graph.Audit()},
       {"type_hashes_completed_from_exact_layout_donor", completedTypeHashes},
       {"unrelated_meshes_preserved", meshes.size() - 1},
       {"external_streaming_connections", false},
       {"collision_generated", false},
       {"implementation", "C++20"},
       {"python_runtime", false},
       {"runtime_verified", false}});
  return {std::move(data), B(owner.begin(), owner.end()), std::move(report)};
}
} // namespace Monstey
