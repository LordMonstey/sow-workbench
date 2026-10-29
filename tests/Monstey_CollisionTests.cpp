#include "Monstey_Collision.hpp"
#include "Monstey_World.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <iostream>
using namespace Monstey;
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
constexpr std::uint32_t Kind = 0xb9e81cf3;
void Check(bool v, const char *s) {
  if (!v)
    throw std::runtime_error(s);
}
template <class F> void Reject(F &&f) {
  bool failed = false;
  try {
    f();
  } catch (const std::exception &) {
    failed = true;
  }
  Check(failed, "Expected rejection");
}
B PhysicsFixture(const std::string &owner) {
  B b;
  for (auto n : {1u, 1u, Monstey_NativeHash(owner)})
    Monstey_AppendU32(b, n);
  Monstey_AppendU16(b, 0);
  Monstey_AppendU32(b, 1);
  Monstey_AppendU16(b, 0);
  for (unsigned i = 0; i < 6; ++i)
    Monstey_AppendU32(b, 0);
  for (auto n : {0xfafab0b0u, 2u, 1u})
    Monstey_AppendU32(b, n);
  for (float f : {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f})
    Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(f));
  Monstey_AppendU32(b, 0);
  Monstey_AppendU32(b, 0xfafabfbf);
  return b;
}
B Box() {
  B b;
  Monstey_AppendU32(b, 2);
  for (float f : {0.125f, 0.75f, 0.f, 1.f, 2.f, 3.f})
    Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(f));
  return b;
}
void A32(B &b, std::uint32_t n) {
  for (unsigned j = 0; j < 4; ++j)
    b.push_back(static_cast<std::uint8_t>(n >> (j * 8)));
}
void A16(B &b, std::uint16_t n) {
  b.push_back(static_cast<std::uint8_t>(n));
  b.push_back(static_cast<std::uint8_t>(n >> 8));
}
void Float(B &b, float f) { A32(b, std::bit_cast<std::uint32_t>(f)); }
std::uint32_t String(B &b, const std::string &s) {
  auto n = static_cast<std::uint32_t>(b.size());
  b.insert(b.end(), s.begin(), s.end());
  b.push_back(0);
  return n;
}
B ObjectFixture(const std::string &owner = "PhysicsBSP",
                bool qualifiedClass = true) {
  B pool;
  auto schema = String(pool, "ObjectDB"), empty = String(pool, ""),
       record = String(pool, "ObjectRec");
  while (pool.size() % 4)
    pool.push_back(0);
  B db{'G', 'A', 'D', 'B'};
  for (auto n : {17u, static_cast<std::uint32_t>(pool.size()), 1u, 1u, 0u, 1u,
                 1u, 0u, 0u, 44u})
    A32(db, n);
  A16(db, 0);
  db.insert(db.end(), pool.begin(), pool.end());
  auto hash = Monstey_NativeHash("Enabled");
  A32(db, hash);
  A32(db, hash);
  db.insert(db.end(), {0, 0, 1});
  A16(db, 1);
  A16(db, 0);
  A32(db, 0);
  A32(db, 1);
  for (auto n : {schema, empty, empty, 0u, 1u, 0u})
    A32(db, n);
  A32(db, record);
  A32(db, 0x00010001);
  A32(db, 0x08000000);
  A32(db, 1);
  A32(db, 0);
  B classes, names;
  String(classes, qualifiedClass ? "WorldCollision" : "Light");
  String(names, owner);
  B obj{'O', 'B', 'J', 'L'};
  for (auto n : {5u, 1u, 1u, static_cast<std::uint32_t>(classes.size()),
                 static_cast<std::uint32_t>(names.size()), 0u})
    A32(obj, n);
  obj.insert(obj.end(), classes.begin(), classes.end());
  obj.insert(obj.end(), names.begin(), names.end());
  A32(obj, 0x1234);
  A32(obj, 1);
  A32(obj, 0);
  A32(obj, 0);
  A16(obj, 65535);
  A32(obj, 0);
  for (auto f : {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f})
    Float(obj, f);
  A32(obj, 0xdeadbeef);
  A32(obj, 0xabcdef01);
  B out{'O', 'B', 'J', 'R'};
  A32(out, 13);
  A32(out, 0x17);
  A32(out, 4);
  for (auto pair :
       {std::pair{1u, db}, std::pair{2u, obj}, std::pair{77u, B{1, 2, 3, 4, 5}},
        std::pair{0xb9e81cf3u, PhysicsFixture(owner)}}) {
    A32(out, pair.first);
    A32(out, static_cast<std::uint32_t>(pair.second.size()));
    out.insert(out.end(), pair.second.begin(), pair.second.end());
  }
  return out;
}

J Spec(const B &source, const B &profile) {
  return J::object(
      {{"source_sha256", Monstey_HashSha256(source)},
       {"profile", J::object({{"source_sha256", Monstey_HashSha256(profile)},
                              {"shape_index", 0}})},
       {"boxes", J::array({J::object(
                     {{"name", "floor"},
                      {"center_cm", J::array({0, -10, 0})},
                      {"half_extents_cm", J::array({300, 10, 300})}})})}});
}
} // namespace
int main(int argc, char **argv) {
  try {
    const auto dir =
        (argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1]))
                  : std::filesystem::temp_directory_path()) /
        ("collision-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const auto source = ObjectFixture();
    Monstey_Physics profileDoc(PhysicsFixture("Profile"));
    profileDoc.ReplaceShape(0, Box());
    const auto profile = profileDoc.Encode();
    const auto originalHash = Monstey_HashSha256(source);
    unsigned passed = 0;
    const auto test = [&](const char *name, auto &&f) {
      f();
      ++passed;
      std::cout << "PASS " << name << '\n';
    };
    test("new native world collision preserves all other chunks and routing",
         [&] {
           auto result =
               Monstey_CollisionBoxes(source, Spec(source, profile), profile);
           Check(result.report.at("geometry_generated").as_bool(),
                 "No authored geometry");
           for (auto kind : {1u, 2u, 77u})
             Check(Monstey_ObjresReadChunk(result.bytes, kind) ==
                       Monstey_ObjresReadChunk(source, kind),
                   "Unrelated chunk changed");
           Monstey_Physics checked(Monstey_ObjresReadChunk(result.bytes, Kind));
           Check(checked.ShapeBytes(0, {0, 0})[0] == 2,
                 "Missing compound transform and box");
           const auto box = checked.ShapeBytes(0, {0, 0});
           Check(
               std::equal(box.begin() + 4, box.begin() + 12, Box().begin() + 4),
               "Profile scalars changed");
           Check(Monstey_U32(box, 12) == 0,
                 "Discarded native third scalar differs");
           Check(result.report.at("bounds_cm") ==
                     J::array({J::array({-300.0, -20.0, -300.0}),
                               J::array({300.0, 0.0, 300.0})}),
                 "World-space AABB differs");
           Monstey_WriteNewFile(dir / "room.objres", result.bytes);
           Monstey_WriteNewText(dir / "room-report.json",
                                result.report.dump(2));
         });
    test("placement composes rotations and native float world centers", [&] {
      auto spec = Spec(source, profile);
      spec["placement"] =
          J::object({{"position_cm", J::array({100, 200, 300})},
                     {"quaternion_xyzw", J::array({0, 0, 1, 0})}});
      spec["boxes"][0]["center_cm"] = J::array({10, 0, 0});
      spec["boxes"][0]["half_extents_cm"] = J::array({2, 4, 6});
      auto result = Monstey_CollisionBoxes(source, spec, profile);
      Check(result.report.at("boxes")[0].at("world_center_cm") ==
                J::array({90.0, 200.0, 300.0}),
            "World center composition wrong");
      Check(result.report.at("bounds_cm") ==
                J::array({J::array({88.0, 196.0, 294.0}),
                          J::array({92.0, 204.0, 306.0})}),
            "Rotated bounds differ");
    });
    test("existing collision remains exact first child", [&] {
      Monstey_Physics physics(Monstey_ObjresReadChunk(source, Kind));
      physics.ReplaceShape(0, Box());
      const auto original = physics.ShapeBytes(0);
      const auto existing =
          Monstey_ObjresReplaceChunk(source, Kind, physics.Encode());
      auto result =
          Monstey_CollisionBoxes(existing, Spec(existing, profile), profile);
      Monstey_Physics checked(Monstey_ObjresReadChunk(result.bytes, Kind));
      Check(checked.ShapeBytes(0, {0}) == original, "Old global shape changed");
      Check(result.report.at("old_collision_preserved").as_bool(),
            "Old collision omitted");
    });
    test("source and profile hashes are mandatory", [&] {
      auto spec = Spec(source, profile);
      spec["source_sha256"] = std::string(64, '0');
      Reject([&] { Monstey_CollisionBoxes(source, spec, profile); });
      spec = Spec(source, profile);
      spec["profile"]["source_sha256"] = std::string(64, '0');
      Reject([&] { Monstey_CollisionBoxes(source, spec, profile); });
    });
    test("invalid dimensions and duplicate names reject", [&] {
      auto spec = Spec(source, profile);
      spec["boxes"][0]["half_extents_cm"] = J::array({0.49, 1, 1});
      Reject([&] { Monstey_CollisionBoxes(source, spec, profile); });
      spec = Spec(source, profile);
      spec["boxes"].as_array().push_back(spec["boxes"][0]);
      Reject([&] { Monstey_CollisionBoxes(source, spec, profile); });
      spec = Spec(source, profile);
      spec["placement"] =
          J::object({{"quaternion_xyzw", J::array({1, 0, 0, 1})}});
      Reject([&] { Monstey_CollisionBoxes(source, spec, profile); });
    });
    test("unqualified owner class and definition rejected", [&] {
      auto changed = ObjectFixture("PhysicsBSP", false);
      Reject([&] {
        Monstey_CollisionBoxes(changed, Spec(changed, profile), profile);
      });
      auto raw = Monstey_ObjresReadChunk(source, Kind);
      Monstey_PutU32(raw, 14, 3);
      changed = Monstey_ObjresReplaceChunk(source, Kind, raw);
      Reject([&] {
        Monstey_CollisionBoxes(changed, Spec(changed, profile), profile);
      });
    });
    test("replacement parsing and failure rollback", [&] {
      Monstey_Physics physics(PhysicsFixture("Test"));
      const auto old = physics.Encode();
      auto malformed = Box();
      malformed.push_back(0);
      Reject([&] { physics.ReplaceShape(0, malformed); });
      B unknown;
      Monstey_AppendU32(unknown, 999);
      Reject([&] { physics.ReplaceShape(0, unknown); });
      Reject([&] { physics.ReplaceShape(1, Box()); });
      Check(physics.Encode() == old,
            "Rejected shape replacement altered source");
    });
    if (argc > 2) {
      const auto nativePath = std::filesystem::path(Monstey_Wide(argv[2]));
      const auto native = Monstey_ReadFile(nativePath);
      const auto profileInput =
          argc > 3
              ? Monstey_ReadFile(std::filesystem::path(Monstey_Wide(argv[3])))
              : native;
      const auto payload = profileInput.size() >= 4 &&
                                   std::equal(profileInput.begin(),
                                              profileInput.begin() + 4, "OBJR")
                               ? Monstey_ObjresReadChunk(profileInput, Kind)
                               : profileInput;
      Monstey_Physics nativePhysics(payload);
      const auto report = nativePhysics.Describe(0, 500);
      const auto rows = report.at("shapes").as_array();
      const auto found =
          std::find_if(rows.begin(), rows.end(), [](const J &row) {
            return row.at("type").as_uint() == 12;
          });
      Check(found != rows.end(), "No real native type12 profile");
      test("actual native PhysicsBSP adds geometry with inherited type12 "
           "profile",
           [&] {
             auto spec = Spec(native, profileInput);
             spec["profile"]["shape_index"] = found->at("index");
             spec["profile"]["route"] = found->at("route");
             auto result = Monstey_CollisionBoxes(native, spec, profileInput);
             const auto oldPayload = Monstey_ObjresReadChunk(native, Kind);
             Monstey_Physics oldPhysics(oldPayload);
             const auto ownerIndex =
                 oldPhysics.DefinitionFor(Monstey_NativeHash("PhysicsBSP"));
             Check(result.report.at("old_collision_preserved").as_bool() ==
                       (Monstey_U32(oldPhysics.ShapeBytes(ownerIndex), 0) != 0),
                   "Existing collision presence differs from native source");
             auto secondSpec = spec;
             secondSpec["source_sha256"] = Monstey_HashSha256(result.bytes);
             secondSpec["boxes"][0]["name"] = "second_floor";
             secondSpec["boxes"][0]["center_cm"] = J::array({700, -10, 0});
             auto second =
                 Monstey_CollisionBoxes(result.bytes, secondSpec, profileInput);
             Monstey_Physics firstPhysics(
                 Monstey_ObjresReadChunk(result.bytes, Kind));
             Monstey_Physics secondPhysics(
                 Monstey_ObjresReadChunk(second.bytes, Kind));
             Check(second.report.at("old_collision_preserved").as_bool() &&
                       secondPhysics.ShapeBytes(ownerIndex, {0}) ==
                           firstPhysics.ShapeBytes(ownerIndex),
                   "Successive native authoring lost the entire old geometry");
             Check(Monstey_HashSha256(nativePath) == Monstey_HashSha256(native),
                   "Native source modified");
             Monstey_WriteNewFile(dir / "native-boxes.objres", result.bytes);
             Monstey_WriteNewText(dir / "native-boxes-report.json",
                                  result.report.dump(2));
           });
    }
    if (argc > 4) {
      const auto unqualified =
          Monstey_ReadFile(std::filesystem::path(Monstey_Wide(argv[4])));
      test("native nonzero definition vectors require separate qualification",
           [&] {
             Reject([&] {
               Monstey_CollisionBoxes(unqualified, Spec(unqualified, profile),
                                      profile);
             });
           });
    }
    Check(Monstey_HashSha256(source) == originalHash,
          "Synthetic source modified");
    std::cout << passed << " collision authoring groups passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}