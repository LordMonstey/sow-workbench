#include "Monstey_NavStaticTreeApi.hpp"
#include "Monstey_Havok.hpp"
#include "Monstey_NavCompactCooker.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>
namespace Monstey {
namespace {
using J = Monstey_Json;
constexpr std::uint64_t Monstey_StaticTreePageMaximum = 512;
void Monstey_StaticTreeKeys(const J &args, bool file) {
  Monstey_Require(args.is_object(), "Static-tree arguments must be an object");
  for (const auto &[key, value] : args.as_object()) {
    (void)value;
    if (key != "mesh_node" && key != "offset" && key != "limit" &&
        !(file && (key == "path" || key == "expected_sha256")))
      throw Monstey_Error("invalid_arguments",
                          "Unknown static-tree argument: " + key);
  }
}
std::uint64_t Monstey_StaticTreeUnsigned(const J &a, std::string_view key,
                                         std::uint64_t fallback,
                                         std::uint64_t maximum) {
  if (!a.contains(key))
    return fallback;
  if (!a.at(key).is_integer())
    throw Monstey_Error("invalid_argument_type",
                        "Static-tree node and page values must be integers");
  const auto v = a.at(key).as_uint();
  if (v > maximum)
    throw Monstey_Error("argument_out_of_range",
                        "Static-tree node or page value exceeds its bound");
  return v;
}
void Monstey_StaticTreePageArguments(const J &a) {
  (void)Monstey_StaticTreeUnsigned(a, "mesh_node", 0,
                                   std::numeric_limits<std::uint64_t>::max());
  (void)Monstey_StaticTreeUnsigned(a, "offset", 0,
                                   std::numeric_limits<std::uint64_t>::max());
  if (Monstey_StaticTreeUnsigned(a, "limit", 256,
                                 Monstey_StaticTreePageMaximum) == 0)
    throw Monstey_Error("argument_out_of_range",
                        "Static-tree page limit must be positive");
}
void Monstey_StaticTreeField(const Monstey_Havok &g, std::uint32_t type,
                             std::string_view name, std::uint32_t offset,
                             std::uint32_t bytes, bool signed_value) {
  const auto members = g.Members(type);
  const auto found =
      std::find_if(members.begin(), members.end(),
                   [&](const auto &m) { return m.name == name; });
  Monstey_Require(found != members.end() && found->offset == offset,
                  "Unsupported compact-tree record member layout");
  const auto &t = g.Types().at(g.EffectiveType(found->type));
  Monstey_Require((t.subtype & 0xfu) == 4 && t.bytes == bytes &&
                      (t.subtype & 0x1e000u) ==
                          (bytes == 4 ? 0x8000u : 0x4000u) &&
                      ((t.subtype & 0x200u) != 0) == signed_value,
                  "Unsupported compact-tree integer member encoding");
}
Monstey_Bytes Monstey_StaticTreeRecords(const Monstey_Havok &g,
                                        std::size_t mesh,
                                        std::string_view field,
                                        std::size_t stride) {
  const auto array = g.Field(mesh, field);
  const auto &node = g.Node(array);
  const auto &a = g.Types().at(g.EffectiveType(node.type));
  Monstey_Require((a.subtype & 0xfu) == 8 && !(a.subtype & 0x20u) &&
                      a.bytes == 16 && a.pointer,
                  "Expected native hkArray record field");
  const auto type = g.EffectiveType(a.pointer);
  const auto &t = g.Types().at(type);
  Monstey_Require(t.bytes == stride,
                  "Unsupported compact-tree native record stride");
  if (field == "faces") {
    Monstey_Require((t.subtype & 0xfu) == 7, "Expected face record class");
    Monstey_StaticTreeField(g, type, "startEdgeIndex", 0, 4, true);
    Monstey_StaticTreeField(g, type, "numEdges", 8, 2, true);
  } else if (field == "edges") {
    Monstey_Require((t.subtype & 0xfu) == 7, "Expected edge record class");
    Monstey_StaticTreeField(g, type, "a", 0, 4, true);
    Monstey_StaticTreeField(g, type, "b", 4, 4, true);
  } else {
    Monstey_Require((t.subtype & 0xfu) == 8 && (t.subtype & 0x20u) &&
                        (t.subtype >> 8) == 4 && t.pointer,
                    "Expected four-lane native vertex vector");
    const auto &scalar = g.Types().at(g.EffectiveType(t.pointer));
    Monstey_Require((scalar.subtype & 0xfu) == 5 && scalar.bytes == 4,
                    "Expected four raw float32 vertex lanes");
  }
  Monstey_Require(node.elements.size() <=
                      std::numeric_limits<std::size_t>::max() / stride,
                  "Native record byte count overflow");
  Monstey_Bytes result;
  result.reserve(node.elements.size() * stride);
  for (const auto id : node.elements) {
    const auto &record = g.Node(id);
    Monstey_Require(g.EffectiveType(record.type) == type &&
                        record.original_storage.size() == stride,
                    "Native record lacks complete original storage");
    result.insert(result.end(), record.original_storage.begin(),
                  record.original_storage.end());
  }
  return result;
}
J Monstey_StaticTreeBits(const Monstey_NavigationVector4 &v) {
  J result = J::array();
  for (const auto value : v)
    result.as_array().push_back(std::bit_cast<std::uint32_t>(value));
  return result;
}
std::string Monstey_StaticTreeHex(std::span<const std::uint8_t> bytes) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(bytes.size() * 2);
  for (const auto byte : bytes) {
    result.push_back(digits[byte >> 4]);
    result.push_back(digits[byte & 15]);
  }
  return result;
}
} // namespace
J Monstey_NavStaticTreeFromBytes(std::span<const std::uint8_t> source,
                                 const J &page) {
  Monstey_StaticTreeKeys(page, false);
  Monstey_StaticTreePageArguments(page);
  Monstey_Require(source.size() >= 28 && Monstey_U32(source, 0) == 0x49414b48 &&
                      Monstey_U32(source, 4) == 3 &&
                      Monstey_U32(source, 8) == 0xffffd00d &&
                      Monstey_U32(source, 12) == source.size() - 20 &&
                      Monstey_U32(source, source.size() - 4) == 0xffffdead,
                  "Invalid HKAI3 envelope");
  Monstey_Havok graph(source.subspan(16, source.size() - 20));
  const auto meshes = graph.Objects("hkaiNavMesh");
  Monstey_Require(!meshes.empty(), "HKAI3 contains no hkaiNavMesh object");
  std::size_t selected = meshes.front();
  if (page.contains("mesh_node")) {
    const auto wanted = Monstey_StaticTreeUnsigned(
        page, "mesh_node", 0, std::numeric_limits<std::uint64_t>::max());
    const auto found = std::find(meshes.begin(), meshes.end(), wanted);
    if (found == meshes.end())
      throw Monstey_Error(
          "invalid_mesh_node",
          "mesh_node is not a hkaiNavMesh object in this source");
    selected = *found;
  } else if (meshes.size() != 1) {
    std::string message =
        "Multiple hkaiNavMesh objects; choose mesh_node. First node IDs:";
    for (std::size_t i = 0; i < std::min<std::size_t>(meshes.size(), 32); ++i)
      message += ' ' + std::to_string(meshes[i]);
    throw Monstey_Error("ambiguous_mesh", message);
  }
  const auto faces = Monstey_StaticTreeRecords(graph, selected, "faces", 16);
  const auto edges = Monstey_StaticTreeRecords(graph, selected, "edges", 20);
  const auto vertices =
      Monstey_StaticTreeRecords(graph, selected, "vertices", 16);
  const auto tree = Monstey_NavCookCompactMesh(faces, edges, vertices);
  const auto total = tree.nodes.size() / 6;
  const auto offset = Monstey_StaticTreeUnsigned(
      page, "offset", 0, std::numeric_limits<std::uint64_t>::max());
  const auto limit = Monstey_StaticTreeUnsigned(page, "limit", 256,
                                                Monstey_StaticTreePageMaximum);
  J nodes = J::array();
  const auto begin =
      static_cast<std::size_t>(std::min<std::uint64_t>(offset, total));
  const auto count =
      std::min<std::size_t>(static_cast<std::size_t>(limit), total - begin);
  for (std::size_t i = 0; i < count; ++i)
    nodes.as_array().push_back(Monstey_StaticTreeHex(
        std::span(tree.nodes).subspan((begin + i) * 6, 6)));
  const auto next = count < total - begin ? J(offset + count) : J(nullptr);
  auto result = J::object(
      {{"schema", "Monstey.NavigationStaticTree/1"},
       {"source_sha256", Monstey_HashSha256(source)},
       {"source_bytes", source.size()},
       {"source_format", "HKAI3"},
       {"mesh_node", selected},
       {"mesh_count", meshes.size()},
       {"mesh_type_version",
        graph.Types()
            .at(graph.EffectiveType(graph.Node(selected).type))
            .version},
       {"faces", faces.size() / 16},
       {"edges", edges.size() / 20},
       {"vertices", vertices.size() / 16},
       {"source_record_strides",
        J::object({{"faces", 16}, {"edges", 20}, {"vertices", 16}})},
       {"source_record_sha256",
        J::object({{"faces", Monstey_HashSha256(faces)},
                   {"edges", Monstey_HashSha256(edges)},
                   {"vertices", Monstey_HashSha256(vertices)}})},
       {"profile", "whole-root bins32 threshold16 finalization1"},
       {"node_stride_bytes", 6},
       {"total_nodes", total},
       {"total_bytes", tree.nodes.size()},
       {"tree_sha256", Monstey_HashSha256(tree.nodes)},
       {"root_bounds_f32_bits",
        J::object({{"minimum", Monstey_StaticTreeBits(tree.bounds.minimum)},
                   {"maximum", Monstey_StaticTreeBits(tree.bounds.maximum)}})},
       {"offset", offset},
       {"limit", limit},
       {"next_offset", next},
       {"nodes_hex", nodes},
       {"node_order",
        "native compact preorder; first page node index is offset"},
       {"root_bounds_encoding",
        "raw float32 words; W is opaque, not a JSON float"},
       {"native_scratch_W_provenance_qualified", false},
       {"native_FP_status_emulated", false},
       {"read_only", true},
       {"runtime_verified", false}});
  Monstey_Require(
      result.dump().size() < 30u * 1024,
      "Bounded compact-tree page exceeded its fixed response budget");
  return result;
}
J Monstey_NavStaticTreeInspect(const J &arguments) {
  Monstey_StaticTreeKeys(arguments, true);
  J page = J::object();
  for (const auto key : {"mesh_node", "offset", "limit"})
    if (arguments.contains(key))
      page[key] = arguments.at(key);
  Monstey_StaticTreePageArguments(page);
  const auto &expected = arguments.at("expected_sha256").as_string();
  if (expected.size() != 64 ||
      !std::all_of(expected.begin(), expected.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
      }))
    throw Monstey_Error(
        "invalid_arguments",
        "Expected SHA256 must be 64 lowercase hexadecimal characters");
  const auto &text = arguments.at("path").as_string();
  if (text.empty() || text.find('\0') != std::string::npos)
    throw Monstey_Error("invalid_path",
                        "Source path must be nonempty and contain no NUL");
  const std::filesystem::path path(Monstey_Wide(text));
  const auto source = Monstey_ReadFile(path, 256ull * 1024 * 1024);
  if (Monstey_HashSha256(source) != expected)
    throw Monstey_Error("source_hash_mismatch", "Source HKAI identity differs");
  auto result = Monstey_NavStaticTreeFromBytes(source, page);
  if (Monstey_HashSha256(path) != expected)
    throw Monstey_Error("source_hash_mismatch",
                        "Source changed during compact-tree inspection");
  result["source_unchanged"] = true;
  return result;
}
} // namespace Monstey
