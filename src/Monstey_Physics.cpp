#include "Monstey_Physics.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>

namespace Monstey {
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
struct Monstey_PUnsupported {
  std::uint32_t type;
};
struct Monstey_PNode {
  std::uint32_t type{};
  std::size_t offset{};
  std::shared_ptr<const B> raw;
  std::vector<Monstey_PNode> children;
};
struct Monstey_PRecord {
  std::shared_ptr<const B> transform;
  Monstey_PNode node;
};
struct Monstey_PDefinition {
  std::shared_ptr<const B> raw;
  std::uint32_t flags{}, identity{}, scalar{};
  std::string prefab;
  std::array<std::uint32_t, 6> vectors{};
};
struct Monstey_PMapping {
  std::uint32_t hash;
  std::uint16_t index;
};
struct Monstey_PState {
  std::vector<Monstey_PMapping> mappings;
  std::vector<Monstey_PDefinition> definitions;
  std::vector<Monstey_PRecord> shapes;
  J edits = J::array();
};
std::shared_ptr<const B> Monstey_PSlice(std::span<const std::uint8_t> bytes,
                                        std::size_t offset, std::size_t count) {
  Monstey_CheckRange(bytes, offset, count);
  return std::make_shared<const B>(
      bytes.begin() + static_cast<std::ptrdiff_t>(offset),
      bytes.begin() + static_cast<std::ptrdiff_t>(offset + count));
}
Monstey_PNode Monstey_ParsePhysicalNode(std::span<const std::uint8_t> bytes,
                                        std::size_t &at, unsigned depth) {
  Monstey_Require(depth <= 64, "Native shape nesting exceeds 64");
  const auto begin = at;
  const auto type = Monstey_U32(bytes, at);
  at += 4;
  Monstey_PNode node;
  node.type = type;
  node.offset = begin;
  switch (type) {
  case 0:
    node.raw = Monstey_PSlice(bytes, begin, 4);
    return node;
  case 2:
  case 3: {
    const auto length = type == 2 ? 24u : 12u;
    Monstey_CheckRange(bytes, at, length);
    at += length;
    node.raw = Monstey_PSlice(bytes, begin, at - begin);
    return node;
  }
  case 5:
    Monstey_CheckRange(bytes, at, 28);
    at += 28;
    node.raw = Monstey_PSlice(bytes, begin, 32);
    node.children.push_back(Monstey_ParsePhysicalNode(bytes, at, depth + 1));
    return node;
  case 6: {
    const auto count = Monstey_U32(bytes, at);
    at += 4;
    Monstey_Require(count <= (bytes.size() - at) / 4,
                    "Compound shape count exceeds payload");
    node.raw = Monstey_PSlice(bytes, begin, 8);
    node.children.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
      node.children.push_back(Monstey_ParsePhysicalNode(bytes, at, depth + 1));
    return node;
  }
  case 12: {
    Monstey_CheckRange(bytes, at, 16);
    const auto marker = Monstey_U32(bytes, at + 8),
               size = Monstey_U32(bytes, at + 12);
    Monstey_Require(marker == 0xffffd00d,
                    "Native TAG0 shape begin marker is invalid");
    Monstey_CheckRange(bytes, at + 16, static_cast<std::size_t>(size) + 4);
    Monstey_Require(size >= 8, "Native TAG0 shape is truncated");
    const auto tag = at + 16;
    const std::uint32_t length = (std::uint32_t(bytes[tag]) << 24) |
                                 (std::uint32_t(bytes[tag + 1]) << 16) |
                                 (std::uint32_t(bytes[tag + 2]) << 8) |
                                 bytes[tag + 3];
    Monstey_Require((length & 0x3fffffff) == size && bytes[tag + 4] == 'T' &&
                        bytes[tag + 5] == 'A' && bytes[tag + 6] == 'G' &&
                        bytes[tag + 7] == '0',
                    "Native TAG0 shape extent/magic mismatch");
    at += 16 + static_cast<std::size_t>(size);
    Monstey_Require(Monstey_U32(bytes, at) == 0xffffdead,
                    "Native TAG0 shape end marker is invalid");
    at += 4;
    node.raw = Monstey_PSlice(bytes, begin, at - begin);
    return node;
  }
  default:
    throw Monstey_PUnsupported{type};
  }
}
void Monstey_EncodePhysicalNode(const Monstey_PNode &node, B &output) {
  output.insert(output.end(), node.raw->begin(), node.raw->end());
  for (const auto &child : node.children)
    Monstey_EncodePhysicalNode(child, output);
}
J Monstey_PFloat(std::uint32_t bits) {
  const auto value = std::bit_cast<float>(bits);
  return std::isfinite(value) ? J(double(value)) : J();
}
J Monstey_PFloatArray(std::span<const std::uint8_t> bytes, std::size_t offset,
                      std::size_t count) {
  auto values = J::array();
  for (std::size_t i = 0; i < count; ++i)
    values.as_array().push_back(
        Monstey_PFloat(Monstey_U32(bytes, offset + i * 4)));
  return values;
}
B Monstey_PVector(const J &value, std::size_t count, const char *label) {
  Monstey_Require(value.is_array() && value.size() == count,
                  std::string(label) + " has an invalid width");
  B out;
  out.reserve(count * 4);
  for (const auto &component : value.as_array()) {
    const auto number = component.as_number();
    Monstey_Require(std::isfinite(number) &&
                        std::abs(number) <= std::numeric_limits<float>::max(),
                    std::string(label) + " exceeds finite float32 range");
    const auto narrowed = static_cast<float>(number);
    Monstey_Require(std::isfinite(number) && std::isfinite(narrowed),
                    std::string(label) + " must contain finite float32 values");
    Monstey_AppendU32(out, std::bit_cast<std::uint32_t>(narrowed));
  }
  return out;
}
B Monstey_PTransform(const J &position, const J &quaternion) {
  auto bytes = Monstey_PVector(position, 3, "position_cm");
  auto rotation = Monstey_PVector(quaternion, 4, "quaternion_xyzw");
  double length = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    const auto v = std::bit_cast<float>(Monstey_U32(rotation, i * 4));
    length += double(v) * v;
  }
  Monstey_Require(std::abs(length - 1) < 0.002,
                  "Physical quaternion must be normalized");
  bytes.insert(bytes.end(), rotation.begin(), rotation.end());
  return bytes;
}
Monstey_PNode &Monstey_PShape(Monstey_PState &state, std::size_t shape,
                              const std::vector<std::size_t> &route) {
  Monstey_Require(shape < state.shapes.size(),
                  "Shape index is outside the native array");
  auto *node = &state.shapes[shape].node;
  for (const auto child : route) {
    Monstey_Require(child < node->children.size(),
                    "Shape route is outside the native compound");
    node = &node->children[child];
  }
  return *node;
}
std::uint16_t Monstey_PFind(const Monstey_PState &state, std::uint32_t hash) {
  std::optional<std::uint16_t> result;
  for (const auto &row : state.mappings)
    if (row.hash == hash) {
      Monstey_Require(!result.has_value(), "Physical owner hash is ambiguous");
      result = row.index;
    }
  Monstey_Require(result.has_value(), "Physical owner mapping was not found");
  return *result;
}
void Monstey_PRequireNew(const Monstey_PState &state, std::uint32_t hash) {
  Monstey_Require(
      std::none_of(state.mappings.begin(), state.mappings.end(),
                   [&](const auto &row) { return row.hash == hash; }),
      "New physical owner hash already exists");
}
void Monstey_PSort(Monstey_PState &state) {
  std::stable_sort(
      state.mappings.begin(), state.mappings.end(),
      [](const auto &a, const auto &b) { return a.hash < b.hash; });
}
} // namespace
struct Monstey_Physics::Monstey_Impl {
  std::shared_ptr<const B> original, shapeBlob;
  Monstey_PState state;
  std::uint32_t shapeCount{};
  std::string limitation;
  explicit Monstey_Impl(std::span<const std::uint8_t> bytes) {
    Monstey_Require(bytes.size() <= 512ull * 1024 * 1024,
                    "Physics payload exceeds 512 MiB");
    original = std::make_shared<const B>(bytes.begin(), bytes.end());
    Monstey_Require(bytes.size() >= 24, "Truncated WorldPhysics payload");
    const auto mappings = Monstey_U32(bytes, 0),
               definitions = Monstey_U32(bytes, 4);
    Monstey_Require(mappings <= (bytes.size() - 8) / 6,
                    "Physical mapping count exceeds payload");
    std::size_t at = 8 + std::size_t(mappings) * 6;
    state.mappings.reserve(mappings);
    for (std::uint32_t i = 0; i < mappings; ++i) {
      const auto hash = Monstey_U32(bytes, 8 + std::size_t(i) * 4);
      const auto index = Monstey_U16(bytes, 8 + std::size_t(mappings) * 4 +
                                                std::size_t(i) * 2);
      Monstey_Require(index < definitions,
                      "Physical mapping index exceeds definitions");
      state.mappings.push_back({hash, index});
    }
    Monstey_Require(definitions <= (bytes.size() - at) / 30,
                    "Physical definition count exceeds payload");
    state.definitions.reserve(definitions);
    for (std::uint32_t i = 0; i < definitions; ++i) {
      const auto begin = at;
      Monstey_PDefinition record;
      record.flags = Monstey_U32(bytes, at);
      const auto length = Monstey_U16(bytes, at + 4);
      at += 6;
      Monstey_CheckRange(bytes, at, length);
      for (std::size_t k = 0; k < length; ++k)
        Monstey_Require(bytes[at + k] < 128,
                        "Physical prefab path is not native ASCII");
      record.prefab.assign(reinterpret_cast<const char *>(bytes.data() + at),
                           length);
      at += length;
      if (length) {
        record.identity = Monstey_U32(bytes, at);
        record.scalar = Monstey_U32(bytes, at + 4);
        at += 8;
      }
      Monstey_CheckRange(bytes, at, 24);
      for (std::size_t k = 0; k < 6; ++k)
        record.vectors[k] = Monstey_U32(bytes, at + k * 4);
      at += 24;
      record.raw = Monstey_PSlice(bytes, begin, at - begin);
      state.definitions.push_back(std::move(record));
    }
    Monstey_CheckRange(bytes, at, 16);
    Monstey_Require(Monstey_U32(bytes, at) == 0xfafab0b0 &&
                        Monstey_U32(bytes, at + 4) == 2,
                    "Unsupported shape array envelope");
    shapeCount = Monstey_U32(bytes, at + 8);
    at += 12;
    Monstey_Require(Monstey_U32(bytes, bytes.size() - 4) == 0xfafabfbf,
                    "Missing native shape array trailer");
    Monstey_Require(at <= bytes.size() - 4, "Shape array starts after trailer");
    shapeBlob = Monstey_PSlice(bytes, at, bytes.size() - 4 - at);
    std::size_t cursor = 0;
    try {
      Monstey_Require(shapeCount <= shapeBlob->size() / 32,
                      "Shape count exceeds native array");
      state.shapes.reserve(shapeCount);
      for (std::uint32_t i = 0; i < shapeCount; ++i) {
        auto transform = Monstey_PSlice(*shapeBlob, cursor, 28);
        cursor += 28;
        auto node = Monstey_ParsePhysicalNode(*shapeBlob, cursor, 0);
        state.shapes.push_back({std::move(transform), std::move(node)});
      }
      Monstey_Require(cursor == shapeBlob->size(),
                      "Native shape array has trailing bytes");
    } catch (const Monstey_PUnsupported &unsupported) {
      state.shapes.clear();
      limitation =
          "Unsupported native shape type " + std::to_string(unsupported.type);
    }
  }
  void RequireParsed() const {
    if (!limitation.empty())
      throw Monstey_Error("physics_shape_not_qualified", limitation);
  }
  B Encode(const Monstey_PState &current) const {
    Monstey_Require(current.mappings.size() <= 0xffffffffull &&
                        current.definitions.size() <= 0xffffffffull,
                    "Physics table exceeds uint32");
    B output;
    Monstey_AppendU32(output,
                      static_cast<std::uint32_t>(current.mappings.size()));
    Monstey_AppendU32(output,
                      static_cast<std::uint32_t>(current.definitions.size()));
    for (const auto &row : current.mappings)
      Monstey_AppendU32(output, row.hash);
    for (const auto &row : current.mappings)
      Monstey_AppendU16(output, row.index);
    for (const auto &definition : current.definitions)
      output.insert(output.end(), definition.raw->begin(),
                    definition.raw->end());
    Monstey_AppendU32(output, 0xfafab0b0);
    Monstey_AppendU32(output, 2);
    Monstey_AppendU32(output, limitation.empty() ? static_cast<std::uint32_t>(
                                                       current.shapes.size())
                                                 : shapeCount);
    if (limitation.empty())
      for (const auto &shape : current.shapes) {
        output.insert(output.end(), shape.transform->begin(),
                      shape.transform->end());
        Monstey_EncodePhysicalNode(shape.node, output);
      }
    else
      output.insert(output.end(), shapeBlob->begin(), shapeBlob->end());
    Monstey_AppendU32(output, 0xfafabfbf);
    return output;
  }
  void Commit(Monstey_PState next, J edit) {
    next.edits.as_array().push_back(std::move(edit));
    const auto encoded = Encode(next);
    Monstey_Physics check(encoded);
    (void)check;
    state = std::move(next);
  }
};
Monstey_Physics::Monstey_Physics(std::span<const std::uint8_t> bytes)
    : impl_(std::make_unique<Monstey_Impl>(bytes)) {
  Monstey_Require(impl_->Encode(impl_->state) == *impl_->original,
                  "Physics identity serialization is not byte exact");
}
Monstey_Physics::~Monstey_Physics() = default;
Monstey_Physics::Monstey_Physics(Monstey_Physics &&) noexcept = default;
Monstey_Physics &
Monstey_Physics::operator=(Monstey_Physics &&) noexcept = default;
B Monstey_Physics::Encode() const { return impl_->Encode(impl_->state); }
std::uint16_t Monstey_Physics::DefinitionFor(std::uint32_t owner) const {
  return Monstey_PFind(impl_->state, owner);
}
void Monstey_Physics::RenameMapping(std::uint32_t before, std::uint32_t after) {
  auto next = impl_->state;
  (void)Monstey_PFind(next, before);
  Monstey_PRequireNew(next, after);
  for (auto &row : next.mappings)
    if (row.hash == before)
      row.hash = after;
  Monstey_PSort(next);
  impl_->Commit(std::move(next),
                J::object({{"operation", "rename_owner_mapping"},
                           {"before", Monstey_Hex(before)},
                           {"after", Monstey_Hex(after)}}));
}
std::uint16_t Monstey_Physics::CloneMapping(std::uint32_t donor,
                                            std::uint32_t target) {
  return ImportMapping(*this, donor, target);
}
std::uint16_t Monstey_Physics::ImportMapping(const Monstey_Physics &donor,
                                             std::uint32_t from,
                                             std::uint32_t to) {
  impl_->RequireParsed();
  donor.impl_->RequireParsed();
  auto next = impl_->state;
  Monstey_Require(next.definitions.size() == next.shapes.size() &&
                      donor.impl_->state.definitions.size() ==
                          donor.impl_->state.shapes.size(),
                  "Physical owner import needs one shape per definition");
  Monstey_Require(next.definitions.size() < 65536,
                  "Physical definition ordinal exceeds uint16");
  Monstey_PRequireNew(next, to);
  const auto source = donor.DefinitionFor(from);
  const auto index = static_cast<std::uint16_t>(next.definitions.size());
  next.definitions.push_back(donor.impl_->state.definitions[source]);
  next.shapes.push_back(donor.impl_->state.shapes[source]);
  next.mappings.push_back({to, index});
  Monstey_PSort(next);
  impl_->Commit(std::move(next),
                J::object({{"operation", "import_physical_owner_mapping"},
                           {"source", Monstey_Hex(from)},
                           {"target", Monstey_Hex(to)},
                           {"new_definition", index},
                           {"opaque_definition_links_preserved", true}}));
  return index;
}
void Monstey_Physics::SetTransform(
    std::size_t shape, const J &position, const J &quaternion,
    const std::optional<std::vector<std::size_t>> &route) {
  impl_->RequireParsed();
  const auto transform = Monstey_PTransform(position, quaternion);
  auto next = impl_->state;
  auto &node =
      Monstey_PShape(next, shape, route.value_or(std::vector<std::size_t>{}));
  if (route) {
    Monstey_Require(node.type == 5,
                    "Nested transform requires a native type5 wrapper");
    auto raw = *node.raw;
    std::copy(transform.begin(), transform.end(), raw.begin() + 4);
    node.raw = std::make_shared<const B>(std::move(raw));
  } else
    next.shapes[shape].transform = std::make_shared<const B>(transform);
  impl_->Commit(std::move(next),
                J::object({{"operation", "set_transform"},
                           {"shape", static_cast<std::uint64_t>(shape)},
                           {"nested", route.has_value()}}));
}
void Monstey_Physics::ResizeBox(std::size_t shape, const J &dimensions,
                                const std::vector<std::size_t> &route) {
  impl_->RequireParsed();
  const auto size = Monstey_PVector(dimensions, 3, "half_extents_cm");
  for (std::size_t i = 0; i < 3; ++i)
    Monstey_Require(std::bit_cast<float>(Monstey_U32(size, i * 4)) > 0,
                    "Box half extents must be positive");
  auto next = impl_->state;
  auto &node = Monstey_PShape(next, shape, route);
  Monstey_Require(node.type == 2, "Box resize requires a native type2 donor");
  auto raw = *node.raw;
  std::copy(size.begin(), size.end(), raw.begin() + 16);
  node.raw = std::make_shared<const B>(std::move(raw));
  impl_->Commit(std::move(next),
                J::object({{"operation", "resize_box"},
                           {"shape", static_cast<std::uint64_t>(shape)},
                           {"definition_bounds_regenerated", false}}));
}
B Monstey_Physics::ShapeBytes(std::size_t shape,
                              const std::vector<std::size_t> &route) const {
  impl_->RequireParsed();
  Monstey_Require(shape < impl_->state.shapes.size(),
                  "Shape index is outside the native array");
  const auto *node = &impl_->state.shapes[shape].node;
  for (const auto child : route) {
    Monstey_Require(child < node->children.size(),
                    "Shape route is outside the native compound");
    node = &node->children[child];
  }
  B bytes;
  Monstey_EncodePhysicalNode(*node, bytes);
  return bytes;
}
B Monstey_Physics::RigidTransformBytes(std::size_t shape) const {
  impl_->RequireParsed();
  Monstey_Require(shape < impl_->state.shapes.size(),
                  "Shape index is outside the native array");
  return *impl_->state.shapes[shape].transform;
}
void Monstey_Physics::ReplaceShape(std::size_t shape,
                                   std::span<const std::uint8_t> bytes,
                                   const std::vector<std::size_t> &route) {
  impl_->RequireParsed();
  Monstey_Require(bytes.size() <= 512ull * 1024 * 1024,
                  "Replacement shape exceeds 512 MiB");
  std::size_t at = 0;
  Monstey_PNode replacement;
  try {
    replacement = Monstey_ParsePhysicalNode(bytes, at, 0);
  } catch (const Monstey_PUnsupported &unsupported) {
    throw Monstey_Error("unsupported_physical_shape",
                        "Replacement shape kind is not qualified: " +
                            std::to_string(unsupported.type));
  }
  Monstey_Require(at == bytes.size(), "Replacement shape has trailing data");
  auto next = impl_->state;
  Monstey_PShape(next, shape, route) = std::move(replacement);
  impl_->Commit(std::move(next),
                J::object({{"operation", "replace_physical_shape"},
                           {"shape", static_cast<std::uint64_t>(shape)},
                           {"shape_sha256", Monstey_HashSha256(bytes)}}));
}
J Monstey_Physics::Describe(std::size_t offset, std::size_t limit) const {
  Monstey_Require(limit >= 1 && limit <= 500,
                  "Physics page limit must be 1..500");
  const auto &p = *impl_;
  auto definitions = J::array(), mappings = J::array(), shapes = J::array();
  for (std::size_t i = 0; i < p.state.definitions.size(); ++i) {
    const auto &d = p.state.definitions[i];
    auto values = J::array(), bits = J::array();
    for (auto value : d.vectors) {
      values.as_array().push_back(Monstey_PFloat(value));
      bits.as_array().push_back(Monstey_Hex(value));
    }
    definitions.as_array().push_back(J::object(
        {{"index", static_cast<std::uint64_t>(i)},
         {"flags", d.flags},
         {"prefab", d.prefab},
         {"identity", d.prefab.empty() ? J() : J(d.identity)},
         {"scalar_raw", d.prefab.empty() ? J() : Monstey_PFloat(d.scalar)},
         {"vectors_raw", values},
         {"vector_bits", bits},
         {"raw_sha256", Monstey_HashSha256(*d.raw)}}));
  }
  for (const auto &row : p.state.mappings)
    mappings.as_array().push_back(J::object(
        {{"owner_hash", Monstey_Hex(row.hash)}, {"definition", row.index}}));
  std::size_t total = 0;
  std::vector<std::size_t> route;
  const auto walk = [&](auto &&self, std::size_t index,
                        const Monstey_PNode &node) -> void {
    if (total >= offset && shapes.size() < limit) {
      auto path = J::array();
      for (auto item : route)
        path.as_array().push_back(static_cast<std::uint64_t>(item));
      B raw;
      Monstey_EncodePhysicalNode(node, raw);
      auto row = J::object({{"index", static_cast<std::uint64_t>(index)},
                            {"route", path},
                            {"type", node.type},
                            {"bytes", static_cast<std::uint64_t>(raw.size())},
                            {"sha256", Monstey_HashSha256(raw)}});
      if (node.type == 2) {
        row["physical_scalars_raw"] = Monstey_PFloatArray(*node.raw, 4, 3);
        row["half_extents_cm"] = Monstey_PFloatArray(*node.raw, 16, 3);
      }
      shapes.as_array().push_back(std::move(row));
    }
    ++total;
    for (std::size_t i = 0; i < node.children.size(); ++i) {
      route.push_back(i);
      self(self, index, node.children[i]);
      route.pop_back();
    }
  };
  for (std::size_t i = 0; i < p.state.shapes.size(); ++i)
    walk(walk, i, p.state.shapes[i].node);
  bool sorted = true;
  for (std::size_t i = 1; i < p.state.mappings.size(); ++i)
    sorted = sorted && p.state.mappings[i - 1].hash < p.state.mappings[i].hash;
  return J::object(
      {{"format", "ObjRes.WorldPhysics"},
       {"source_sha256", Monstey_HashSha256(*p.original)},
       {"object_mappings", static_cast<std::uint64_t>(p.state.mappings.size())},
       {"mapping_rows", mappings},
       {"mappings_strict_hash_order", sorted},
       {"definitions", definitions},
       {"shape_count", p.limitation.empty()
                           ? static_cast<std::uint64_t>(p.state.shapes.size())
                           : p.shapeCount},
       {"shapes", shapes},
       {"total_shape_nodes", static_cast<std::uint64_t>(total)},
       {"offset", static_cast<std::uint64_t>(offset)},
       {"limit", static_cast<std::uint64_t>(limit)},
       {"parse_limitation", p.limitation.empty() ? J() : J(p.limitation)},
       {"no_op_byte_exact", p.state.edits.empty() && Encode() == *p.original},
       {"edits", p.state.edits},
       {"collision_parameters_inherited", true},
       {"physical_definition_vectors_preserved", true},
       {"owner_objects_edited", false},
       {"navigation_generated", false},
       {"runtime_verified", false}});
}
} // namespace Monstey
