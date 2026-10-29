#include "Monstey_PhysicsWorld.hpp"
#include "Monstey_World.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <iostream>
using namespace Monstey;
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void Reject(F &&call) {
  bool rejected = false;
  try {
    call();
  } catch (const std::exception &) {
    rejected = true;
  }
  Check(rejected, "Expected a validation failure");
}
B PhysicsFixture(const std::string &owner) {
  B raw;
  for (auto n : {1u, 1u, Monstey_NativeHash(owner)})
    Monstey_AppendU32(raw, n);
  Monstey_AppendU16(raw, 0);
  Monstey_AppendU32(raw, 0x12345678);
  Monstey_AppendU16(raw, 0);
  for (float f : {-1.f, -2.f, -3.f, 1.f, 2.f, 3.f})
    Monstey_AppendU32(raw, std::bit_cast<std::uint32_t>(f));
  for (auto n : {0xfafab0b0u, 2u, 1u})
    Monstey_AppendU32(raw, n);
  for (float f : {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f})
    Monstey_AppendU32(raw, std::bit_cast<std::uint32_t>(f));
  Monstey_AppendU32(raw, 2);
  for (auto n : {0xffc01234u, 0x80000000u, 0x3f800000u, 0x41200000u,
                 0x41a00000u, 0x41f00000u, 0xfafabfbfu})
    Monstey_AppendU32(raw, n);
  return raw;
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
B ObjectFixture(const std::string &owner = "NativeLight") {
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
  String(classes, "Light");
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
  for (auto f : {10.f, 20.f, 30.f, 0.f, 0.f, 0.f, 1.f})
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

J Owner(const B &raw, const std::string &name) {
  const auto rows = Monstey_ObjresOwners(raw, true);
  for (const auto &row : rows.as_array())
    if (row.at("name").as_string() == name)
      return row;
  throw std::runtime_error("Owner missing");
}
std::string Text(const std::filesystem::path &p) {
  return Monstey_Utf8(p.wstring());
}
J Inspect(const std::filesystem::path &source) {
  return Monstey_PhysicsWorldCall("sow_world_physics_inspect",
                                  J::object({{"source", Text(source)}}));
}
J Apply(const std::filesystem::path &source,
        const std::filesystem::path &output, J spec) {
  return Monstey_PhysicsWorldCall("sow_world_physics_apply",
                                  J::object({{"source", Text(source)},
                                             {"output", Text(output)},
                                             {"spec", std::move(spec)}}));
}
} // namespace
int main(int argc, char **argv) {
  try {
    const auto base = argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1]))
                               : std::filesystem::temp_directory_path();
    const auto dir =
        base /
        ("physics-world-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    const auto source = dir / "source.objres", donor = dir / "donor.objres";
    const auto original = ObjectFixture(),
               donorBytes = ObjectFixture("DonorLight");
    Monstey_WriteNewFile(source, original);
    Monstey_WriteNewFile(donor, donorBytes);
    unsigned passed = 0;
    const auto test = [&](const char *name, auto &&f) {
      f();
      ++passed;
      std::cout << "PASS " << name << '\n';
    };
    test("in-memory editor chains owners and resolves donors without staging",
         [&] {
           auto cloned = Monstey_PhysicsWorldApply(
               original,
               J::object({{"clone_owners",
                           J::array({J::object(
                               {{"name", "NativeLight"},
                                {"new_name", "MemoryClone"},
                                {"delta_cm", J::array({1, 2, 3})}})})}}));
           unsigned reads = 0;
           auto imported = Monstey_PhysicsWorldApply(
               cloned.first,
               J::object(
                   {{"import_owners",
                     J::array({J::object(
                         {{"donor", "memory-donor.objres"},
                          {"donor_name", "DonorLight"},
                          {"new_name", "MemoryImport"},
                          {"position_cm", J::array({1, 2, 3})},
                          {"quaternion_xyzw", J::array({0, 0, 0, 1})}})})}}),
               [&](const std::filesystem::path &path) {
                 Check(path.filename() == L"memory-donor.objres",
                       "Donor resolver received wrong identity");
                 ++reads;
                 return donorBytes;
               });
           Check(reads == 1 &&
                     Monstey_ObjresOwners(imported.first, false).size() == 3,
                 "Chained in-memory graph lost owners");
           Check(imported.second.at("owners").at("verified").as_bool(),
                 "In-memory owner validation missing");
           Check(original == ObjectFixture(), "In-memory input changed");
         });
    test("wrapped identity preserves every OBJRES chunk", [&] {
      auto report = Apply(source, dir / "identity.objres", J::object());
      Check(Monstey_ReadFile(dir / "identity.objres") == original,
            "Identity changed OBJRES bytes");
      Check(report.at("owners").at("verified").as_bool(),
            "Owner graph not checked");
      Check(Inspect(source).at("mapping_rows").size() == 1, "Missing mapping");
    });
    test("rename changes matching owner and sorted native hash only", [&] {
      auto path = dir / "rename.objres";
      auto report = Apply(
          source, path,
          J::object({{"rename_owners",
                      J::array({J::object({{"old_name", "NativeLight"},
                                           {"new_name", "RenamedLight"}})})}}));
      auto changed = Monstey_ReadFile(path);
      Check(Owner(changed, "RenamedLight").at("prefix_offset").as_uint() ==
                65535,
            "Identity prefix changed");
      Check(Monstey_ObjresReadChunk(changed, 1) ==
                Monstey_ObjresReadChunk(original, 1),
            "Property graph changed");
      Check(Inspect(path).at("mapping_rows")[0].at("owner_hash").as_string() ==
                Monstey_Hex(Monstey_NativeHash("RenamedLight")),
            "Mapping not renamed");
      Check(report.at("owners").at("verified").as_bool(),
            "Renamed owner mismatch");
    });
    test("clone copies physical bytes and offsets only the new object", [&] {
      auto path = dir / "clone.objres";
      auto report =
          Apply(source, path,
                J::object({{"clone_owners",
                            J::array({J::object(
                                {{"name", "NativeLight"},
                                 {"new_name", "CloneLight"},
                                 {"delta_cm", J::array({5, -2, 10})}})})}}));
      auto changed = Monstey_ReadFile(path);
      Check(Owner(changed, "NativeLight").at("position") ==
                J::array({10.0, 20.0, 30.0}),
            "Original owner moved");
      Check(Owner(changed, "CloneLight").at("position") ==
                J::array({15.0, 18.0, 40.0}),
            "Clone placement differs");
      Check(report.at("shape_count").as_uint() == 2 &&
                report.at("mapping_rows").size() == 2,
            "Clone tables missing");
      Check(report.at("definitions")[0].at("raw_sha256") ==
                report.at("definitions")[1].at("raw_sha256"),
            "Definition copy changed");
      Check(report.at("shapes")[0].at("sha256") ==
                report.at("shapes")[1].at("sha256"),
            "Shape copy changed");
    });
    const auto importSpec = [&](J properties, bool explicitProperties) {
      auto row = J::object({{"donor", Text(donor)},
                            {"donor_name", "DonorLight"},
                            {"new_name", "ImportedLight"},
                            {"position_cm", J::array({40, 50, 60})},
                            {"quaternion_xyzw", J::array({0, 0, 0, 1})}});
      if (explicitProperties)
        row["properties"] = std::move(properties);
      return J::object({{"import_owners", J::array({row})}});
    };
    test("cross-file import retains donor properties and fresh identity", [&] {
      const auto path = dir / "import.objres";
      Apply(source, path, importSpec(J(), false));
      const auto raw = Monstey_ReadFile(path);
      const auto owner = Owner(raw, "ImportedLight");
      Check(owner.at("prefix_offset").as_uint() == 65535,
            "Imported prefix points into donor pool");
      Check(owner.at("position") == J::array({40.0, 50.0, 60.0}),
            "Imported position differs");
      Check(owner.at("property_values").size() == 1,
            "Default import lost donor properties");
      Check(Owner(raw, "NativeLight").at("property_values") ==
                Owner(original, "NativeLight").at("property_values"),
            "Target properties changed");
      Check(Inspect(path).at("owners").at("verified").as_bool(),
            "Imported owner not mapped");
    });
    test("explicit empty imported properties replace donor fields", [&] {
      auto path = dir / "empty-properties.objres";
      Apply(source, path, importSpec(J::object(), true));
      Check(Owner(Monstey_ReadFile(path), "ImportedLight")
                .at("property_values")
                .empty(),
            "Empty properties treated as overlay");
    });
    test("named imported properties match existing hashed field", [&] {
      auto path = dir / "named-properties.objres";
      Apply(source, path, importSpec(J::object({{"Enabled", false}}), true));
      const auto values =
          Owner(Monstey_ReadFile(path), "ImportedLight").at("property_values");
      Check(
          values.size() == 1 &&
              !values.at(Monstey_Hex(Monstey_NativeHash("Enabled"))).as_bool(),
          "Named override dropped or mistyped");
    });
    test("raw transform and box edit retain inherited scalar bits", [&] {
      auto rawPath = dir / "physics.raw";
      const auto raw = PhysicsFixture("NativeLight");
      Monstey_WriteNewFile(rawPath, raw);
      auto report = Apply(
          rawPath, dir / "edited.raw",
          J::object(
              {{"transforms",
                J::array({J::object(
                    {{"index", 0},
                     {"position_cm", J::array({7, 8, 9})},
                     {"quaternion_xyzw", J::array({0, 0, 0, 1})}})})},
               {"boxes", J::array({J::object(
                             {{"index", 0},
                              {"half_extents_cm", J::array({2, 3, 4})}})})}}));
      Check(report.at("shapes")[0].at("half_extents_cm") ==
                J::array({2.0, 3.0, 4.0}),
            "Box extent mismatch");
      auto result = Monstey_ReadFile(dir / "edited.raw");
      Check(Monstey_U32(result, 88) == 0xffc01234u,
            "Inherited NaN bits changed");
    });
    test("all owner operations reject raw context before publication", [&] {
      auto rawPath = dir / "raw-owner.physics";
      Monstey_WriteNewFile(rawPath, PhysicsFixture("NativeLight"));
      Reject([&] {
        Apply(
            rawPath, dir / "bad-owner.raw",
            J::object({{"rename_owners",
                        J::array({J::object({{"old_name", "NativeLight"},
                                             {"new_name", "NoContext"}})})}}));
      });
      Check(!std::filesystem::exists(dir / "bad-owner.raw"),
            "Rejected edit published output");
    });
    test("invalid owner placement and duplicates leave source unchanged", [&] {
      auto spec = importSpec(J(), false);
      spec["import_owners"][0]["quaternion_xyzw"] = J::array({0, 0, 0, 0});
      Reject([&] { Apply(source, dir / "bad-quat.objres", spec); });
      spec = importSpec(J(), false);
      spec["import_owners"][0]["new_name"] = "NativeLight";
      Reject([&] { Apply(source, dir / "duplicate.objres", spec); });
      Check(!std::filesystem::exists(dir / "bad-quat.objres") &&
                !std::filesystem::exists(dir / "duplicate.objres"),
            "Rejected imports published");
    });
    test("hash checks, unknown fields and output source protection", [&] {
      Reject([&] {
        Apply(source, dir / "bad-hash.objres",
              J::object({{"source_sha256", std::string(64, '0')}}));
      });
      Reject([&] {
        Apply(source, dir / "bad-key.objres",
              J::object({{"box_typo", J::array()}}));
      });
      Reject([&] { Apply(source, source, J::object()); });
      const auto existing = dir / "existing.objres";
      Monstey_WriteNewFile(existing, B{9});
      Reject([&] { Apply(source, existing, J::object()); });
      Check(Monstey_ReadFile(existing) == B{9}, "Existing output overwritten");
      Check(Monstey_ReadFile(source) == original &&
                Monstey_ReadFile(donor) == donorBytes,
            "Input modified");
    });
    test("output and report publish consistent hashes", [&] {
      const auto output = dir / "reported.objres",
                 reportPath = dir / "reported.json";
      auto report = Monstey_PhysicsWorldCall(
          "sow_world_physics_apply", J::object({{"source", Text(source)},
                                                {"output", Text(output)},
                                                {"spec", J::object()},
                                                {"report", Text(reportPath)}}));
      Check(report.at("sha256").as_string() == Monstey_HashSha256(output),
            "Output hash mismatch");
      Check(J::Parse(Monstey_ReadText(reportPath)) == report,
            "Report readback differs");
    });
    if (argc > 2) {
      const auto nativeSource = std::filesystem::path(Monstey_Wide(argv[2]));
      const auto nativeBytes = Monstey_ReadFile(nativeSource);
      const auto nativeHash = Monstey_HashSha256(nativeBytes);
      test("actual OBJRES physics inspection and identity", [&] {
        const auto path = dir / "native-identity.objres";
        auto report = Apply(nativeSource, path, J::object());
        Check(Monstey_ReadFile(path) == nativeBytes,
              "Native OBJRES identity differs");
        Check(report.at("owners").at("verified").as_bool(),
              "Native owner resolution failed");
        Check(report.at("mapping_rows").size() == 7080 &&
                  report.at("shape_count").as_uint() == 412,
              "Native source counts changed");
      });
      test("actual OBJRES owner clone preserves all old physical definitions",
           [&] {
             const auto before = Inspect(nativeSource),
                        owners = Monstey_ObjresOwners(nativeBytes, false);
             const auto wanted = before.at("mapping_rows")[0].at("owner_hash");
             std::string name;
             for (const auto &owner : owners.as_array())
               if (owner.at("name_hash") == wanted)
                 name = owner.at("name").as_string();
             Check(!name.empty(), "Could not resolve native physical owner");
             auto output = dir / "native-clone.objres";
             auto after = Apply(
                 nativeSource, output,
                 J::object({{"clone_owners",
                             J::array({J::object(
                                 {{"name", name},
                                  {"new_name", "NativeCppPhysicsOwnerClone"},
                                  {"delta_cm", J::array({0, 0, 20})}})})}}));
             Check(after.at("shape_count").as_uint() == 413 &&
                       after.at("mapping_rows").size() == 7081,
                   "Native clone counts differ");
             for (std::size_t i = 0; i < before.at("definitions").size(); ++i)
               Check(before.at("definitions")[i] == after.at("definitions")[i],
                     "Old native definition changed");
             Check(Monstey_HashSha256(nativeSource) == nativeHash,
                   "Native source modified");
           });
    }
    std::cout << passed << " physics world groups passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}