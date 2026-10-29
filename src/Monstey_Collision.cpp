#include "Monstey_Collision.hpp"
#include "Monstey_World.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
using V = std::array<double, 3>;
using Q = std::array<double, 4>;
constexpr std::uint32_t Kind = 0xb9e81cf3;
void Monstey_CKeys(const J &value,
                   std::initializer_list<std::string_view> keys) {
  Monstey_Require(value.is_object(),
                  "Collision specification must be an object");
  for (const auto &[key, v] : value.as_object()) {
    (void)v;
    Monstey_Require(std::find(keys.begin(), keys.end(), key) != keys.end(),
                    "Unknown collision field: " + key);
  }
}
template <std::size_t N> std::array<double, N> Monstey_CVector(const J &value) {
  Monstey_Require(value.is_array() && value.size() == N,
                  "Collision vector width mismatch");
  std::array<double, N> out{};
  for (std::size_t i = 0; i < N; ++i) {
    const auto d = value[i].as_number();
    Monstey_Require(std::isfinite(d) &&
                        std::abs(d) <= std::numeric_limits<float>::max(),
                    "Collision value exceeds finite float32");
    out[i] = static_cast<double>(static_cast<float>(d));
  }
  return out;
}
Q Monstey_CQuaternion(const J &value) {
  auto q = Monstey_CVector<4>(value);
  double length = 0;
  for (auto v : q)
    length += v * v;
  length = std::sqrt(length);
  Monstey_Require(std::abs(length - 1.0) <= 0.0001,
                  "Collision quaternion must be normalized");
  for (auto &v : q)
    v /= length;
  return q;
}
Q Monstey_CMultiply(const Q &a, const Q &b) {
  Q result{a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
           a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
           a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
           a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
  double norm = 0;
  for (auto v : result)
    norm += v * v;
  norm = std::sqrt(norm);
  Monstey_Require(std::isfinite(norm) && norm > 0,
                  "Collision rotation composition is invalid");
  for (auto &v : result)
    v /= norm;
  return result;
}
V Monstey_CRotate(const V &p, const Q &q) {
  const V t{2 * (q[1] * p[2] - q[2] * p[1]), 2 * (q[2] * p[0] - q[0] * p[2]),
            2 * (q[0] * p[1] - q[1] * p[0])};
  return {p[0] + q[3] * t[0] + q[1] * t[2] - q[2] * t[1],
          p[1] + q[3] * t[1] + q[2] * t[0] - q[0] * t[2],
          p[2] + q[3] * t[2] + q[0] * t[1] - q[1] * t[0]};
}
template <std::size_t N> J Monstey_CJson(const std::array<double, N> &value) {
  auto out = J::array();
  for (auto v : value)
    out.as_array().push_back(v);
  return out;
}
void Monstey_CFloat(B &bytes, double value) {
  Monstey_Require(std::isfinite(value) &&
                      std::abs(value) <= std::numeric_limits<float>::max(),
                  "Collision result exceeds finite float32");
  Monstey_AppendU32(bytes,
                    std::bit_cast<std::uint32_t>(static_cast<float>(value)));
}
bool Monstey_CIdentity(const J &p, const J &q) {
  return Monstey_CVector<3>(p) == V{0, 0, 0} &&
         Monstey_CVector<4>(q) == Q{0, 0, 0, 1};
}
B Monstey_CPayload(std::span<const std::uint8_t> raw) {
  if (raw.size() >= 4 && std::equal(raw.begin(), raw.begin() + 4, "OBJR"))
    return Monstey_ObjresReadChunk(raw, Kind);
  return B(raw.begin(), raw.end());
}
} // namespace
Monstey_CollisionResult
Monstey_CollisionBoxes(std::span<const std::uint8_t> source, const J &spec,
                       std::span<const std::uint8_t> profile) {
  Monstey_CKeys(spec, {"schema", "source_sha256", "owner", "profile",
                       "placement", "boxes"});
  Monstey_Require(
      spec.value("schema", "Monstey_WorldCollisionAuthor_1").as_string() ==
          "Monstey_WorldCollisionAuthor_1",
      "Unsupported collision author schema");
  const auto sourceHash = Monstey_HashSha256(source),
             profileHash = Monstey_HashSha256(profile);
  Monstey_Require(spec.at("source_sha256").as_string() == sourceHash,
                  "Collision source SHA256 mismatch");
  const auto &profileSpec = spec.at("profile");
  Monstey_CKeys(profileSpec, {"source_sha256", "shape_index", "route"});
  Monstey_Require(profileSpec.at("source_sha256").as_string() == profileHash,
                  "Collision profile SHA256 mismatch");
  std::vector<std::size_t> route;
  const auto routeSpec = profileSpec.value("route", J::array());
  for (const auto &v : routeSpec.as_array()) {
    Monstey_Require(route.size() < 64, "Collision profile route exceeds 64");
    route.push_back(static_cast<std::size_t>(v.as_uint()));
  }
  Monstey_Physics donor(Monstey_CPayload(profile));
  const auto profileNode = donor.ShapeBytes(
      static_cast<std::size_t>(profileSpec.at("shape_index").as_uint()), route);
  const auto profileKind = Monstey_U32(profileNode, 0);
  Monstey_Require(profileKind == 2 || profileKind == 12,
                  "Collision profile must be native box2 or TAG0 type12");
  for (std::size_t at : {4u, 8u}) {
    const auto value = std::bit_cast<float>(Monstey_U32(profileNode, at));
    Monstey_Require(std::isfinite(value) && value >= 0,
                    "Collision profile scalars must be finite and nonnegative");
  }
  auto owners = Monstey_ObjresOwners(source, false);
  std::unordered_map<std::string, std::size_t> identities;
  for (const auto &row : owners.as_array())
    ++identities[row.at("name_hash").as_string()];
  const auto ownerName = spec.value("owner", "PhysicsBSP").as_string();
  std::optional<J> selected;
  for (const auto &row : owners.as_array())
    if (row.at("name").as_string() == ownerName) {
      Monstey_Require(!selected, "Collision owner name is ambiguous");
      selected = row;
    }
  Monstey_Require(selected.has_value(), "Collision owner was not found");
  Monstey_Require(selected->at("class_name").as_string() == "WorldCollision",
                  "Collision authoring requires native WorldCollision owner");
  Monstey_Require(
      Monstey_CIdentity(selected->at("position"), selected->at("quaternion")),
      "Global collision owner must retain native identity placement");
  Monstey_Physics physics(Monstey_ObjresReadChunk(source, Kind));
  const auto before = physics.Describe();
  Monstey_Require(
      before.at("parse_limitation").is_null() &&
          before.at("definitions").size() == before.at("shape_count").as_uint(),
      "Collision authoring requires one parsed shape per definition");
  const auto mappings = before.at("mapping_rows");
  for (const auto &row : mappings.as_array())
    Monstey_Require(identities[row.at("owner_hash").as_string()] == 1,
                    "Collision mappings require unique OBJRES owners");
  const auto index = physics.DefinitionFor(Monstey_NativeHash(ownerName));
  const auto definition = before.at("definitions")[index];
  Monstey_Require(definition.at("flags").as_uint() == 1 &&
                      definition.at("prefab").as_string().empty() &&
                      definition.at("identity").is_null(),
                  "Global collision definition route is not qualified");
  for (const auto &v : definition.at("vectors_raw").as_array())
    Monstey_Require(v.is_number() && v.as_number() == 0,
                    "Global collision definition vectors must remain zero");
  B identity;
  for (auto v : {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f})
    Monstey_CFloat(identity, v);
  Monstey_Require(physics.RigidTransformBytes(index) == identity,
                  "Global collision rigid transform must be native identity");
  const auto placement = spec.value("placement", J::object());
  Monstey_CKeys(placement, {"position_cm", "quaternion_xyzw"});
  const auto pp =
      Monstey_CVector<3>(placement.value("position_cm", J::array({0, 0, 0})));
  const auto pq = Monstey_CQuaternion(
      placement.value("quaternion_xyzw", J::array({0, 0, 0, 1})));
  const auto &boxes = spec.at("boxes");
  Monstey_Require(boxes.is_array() && !boxes.empty() && boxes.size() <= 4096,
                  "Expected 1..4096 collision boxes");
  const auto originalShape = physics.ShapeBytes(index);
  const bool preserved = Monstey_U32(originalShape, 0) != 0;
  B compound;
  Monstey_AppendU32(compound, 6);
  Monstey_AppendU32(
      compound, static_cast<std::uint32_t>(boxes.size() + (preserved ? 1 : 0)));
  if (preserved)
    compound.insert(compound.end(), originalShape.begin(), originalShape.end());
  auto records = J::array();
  std::unordered_set<std::string> names;
  V lower{}, upper{};
  bool first = true;
  for (const auto &row : boxes.as_array()) {
    Monstey_CKeys(row,
                  {"name", "center_cm", "half_extents_cm", "quaternion_xyzw"});
    const auto name = row.at("name").as_string();
    Monstey_Require(!name.empty() && name.size() < 256 &&
                        name.find('\0') == name.npos &&
                        names.insert(name).second,
                    "Collision box name is invalid or duplicated");
    (void)Monstey_Wide(name);
    const auto center = Monstey_CVector<3>(row.at("center_cm")),
               half = Monstey_CVector<3>(row.at("half_extents_cm"));
    for (auto v : half)
      Monstey_Require(v >= 0.5 && v <= 1e9,
                      "Box half extents must be 0.5..1e9 cm");
    const auto rotation = Monstey_CQuaternion(
        row.value("quaternion_xyzw", J::array({0, 0, 0, 1})));
    V global = Monstey_CRotate(center, pq);
    for (std::size_t k = 0; k < 3; ++k)
      global[k] += pp[k];
    global = Monstey_CVector<3>(Monstey_CJson(global));
    auto globalQ =
        Monstey_CVector<4>(Monstey_CJson(Monstey_CMultiply(pq, rotation)));
    B shape;
    Monstey_AppendU32(shape, 5);
    for (auto v : global)
      Monstey_CFloat(shape, v);
    for (auto v : globalQ)
      Monstey_CFloat(shape, v);
    Monstey_AppendU32(shape, 2);
    shape.insert(shape.end(), profileNode.begin() + 4,
                 profileNode.begin() + 12);
    Monstey_AppendU32(shape, 0);
    for (auto v : half)
      Monstey_CFloat(shape, v);
    auto corners = J::array();
    V boxMin{}, boxMax{};
    bool boxFirst = true;
    for (unsigned mask = 0; mask < 8; ++mask) {
      V point{};
      for (std::size_t k = 0; k < 3; ++k)
        point[k] = ((mask & (1u << k)) ? 1.0 : -1.0) * half[k];
      point = Monstey_CRotate(point, globalQ);
      for (std::size_t k = 0; k < 3; ++k)
        point[k] += global[k];
      for (std::size_t k = 0; k < 3; ++k) {
        Monstey_Require(std::isfinite(point[k]) &&
                            std::abs(point[k]) <=
                                std::numeric_limits<float>::max(),
                        "Collision AABB is not finite");
        if (boxFirst)
          boxMin[k] = boxMax[k] = point[k];
        else {
          boxMin[k] = std::min(boxMin[k], point[k]);
          boxMax[k] = std::max(boxMax[k], point[k]);
        }
        if (first)
          lower[k] = upper[k] = point[k];
        else {
          lower[k] = std::min(lower[k], point[k]);
          upper[k] = std::max(upper[k], point[k]);
        }
      }
      first = false;
      boxFirst = false;
      corners.as_array().push_back(Monstey_CJson(point));
    }
    compound.insert(compound.end(), shape.begin(), shape.end());
    records.as_array().push_back(
        J::object({{"name", name},
                   {"world_center_cm", Monstey_CJson(global)},
                   {"world_quaternion_xyzw", Monstey_CJson(globalQ)},
                   {"half_extents_cm", Monstey_CJson(half)},
                   {"world_aabb_cm",
                    J::array({Monstey_CJson(boxMin), Monstey_CJson(boxMax)})},
                   {"corners_cm", corners},
                   {"native_shape_sha256", Monstey_HashSha256(shape)}}));
  }
  physics.ReplaceShape(index, compound);
  const auto output =
      Monstey_ObjresReplaceChunk(source, Kind, physics.Encode());
  Monstey_Physics checked(Monstey_ObjresReadChunk(output, Kind));
  const auto after = checked.Describe();
  Monstey_Require(after.at("mapping_rows") == mappings &&
                      after.at("definitions") == before.at("definitions"),
                  "Collision authoring changed native physical routing");
  Monstey_Physics original(Monstey_ObjresReadChunk(source, Kind));
  for (std::size_t i = 0; i < before.at("shape_count").as_uint(); ++i)
    if (i != index)
      Monstey_Require(checked.ShapeBytes(i) == original.ShapeBytes(i),
                      "Collision authoring changed an unrelated shape");
  if (preserved)
    Monstey_Require(checked.ShapeBytes(index, {0}) == originalShape,
                    "Existing collision changed");
  return {
      output,
      J::object(
          {{"schema", "Monstey_WorldCollisionAuthor_1"},
           {"status", "AUTHORED_STATIC_BOX_COLLISION_RUNTIME_PENDING"},
           {"source_sha256", sourceHash},
           {"profile_sha256", profileHash},
           {"profile_shape_sha256", Monstey_HashSha256(profileNode)},
           {"owner", ownerName},
           {"definition_index", index},
           {"boxes", records},
           {"bounds_cm",
            J::array({Monstey_CJson(lower), Monstey_CJson(upper)})},
           {"geometry_generated", true},
           {"old_collision_preserved", preserved},
           {"existing_shape_sha256", Monstey_HashSha256(originalShape)},
           {"global_definition_byte_exact", true},
           {"native_primitive", "box2/transform5/compound6"},
           {"spatial_registration",
            J::object({{"route", "WorldCollision->333430->333510->333874->"
                                 "738E8C->1CD6FC->1CD8F4"},
                       {"definition_mode", "native global world-space flags1"},
                       {"in_game_broadphase_verified", false}})},
           {"navigation_generated", false},
           {"runtime_verified", false}})};
}
} // namespace Monstey