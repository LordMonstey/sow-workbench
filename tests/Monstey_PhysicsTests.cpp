#include "Monstey_Physics.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <iostream>
#include <limits>

using namespace Monstey;
using B = Monstey_Bytes;
using J = Monstey_Json;
namespace {
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
template <class F> void Reject(F &&call) {
  bool failed = false;
  try {
    call();
  } catch (const std::exception &) {
    failed = true;
  }
  Check(failed, "Expected a validation failure");
}
void Float(B &bytes, float value) {
  Monstey_AppendU32(bytes, std::bit_cast<std::uint32_t>(value));
}
B Transform() {
  B bytes;
  for (float value : {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f})
    Float(bytes, value);
  return bytes;
}
B Box() {
  B bytes;
  Monstey_AppendU32(bytes, 2);
  Monstey_AppendU32(bytes, 0xffc01234);
  Float(bytes, -2.75f);
  Float(bytes, .125f);
  for (float value : {1.f, 2.f, 3.f})
    Float(bytes, value);
  return bytes;
}
B Null() {
  B bytes;
  Monstey_AppendU32(bytes, 0);
  return bytes;
}
B Wrapped(B child) {
  B bytes;
  Monstey_AppendU32(bytes, 5);
  auto transform = Transform();
  bytes.insert(bytes.end(), transform.begin(), transform.end());
  bytes.insert(bytes.end(), child.begin(), child.end());
  return bytes;
}
B Compound(std::initializer_list<B> children) {
  B bytes;
  Monstey_AppendU32(bytes, 6);
  Monstey_AppendU32(bytes, static_cast<std::uint32_t>(children.size()));
  for (const auto &child : children)
    bytes.insert(bytes.end(), child.begin(), child.end());
  return bytes;
}
B Tag() {
  B bytes;
  for (auto word : {12u, 0x11223344u, 0x55667788u, 0xffffd00du, 8u})
    Monstey_AppendU32(bytes, word);
  bytes.insert(bytes.end(), {0x40, 0, 0, 8, 'T', 'A', 'G', '0'});
  Monstey_AppendU32(bytes, 0xffffdead);
  return bytes;
}
B Definition(bool prefab) {
  B bytes;
  Monstey_AppendU32(bytes, 0xa5a51234);
  const std::string name = prefab ? "worlds/test.prefab" : "";
  Monstey_AppendU16(bytes, static_cast<std::uint16_t>(name.size()));
  bytes.insert(bytes.end(), name.begin(), name.end());
  if (prefab) {
    Monstey_AppendU32(bytes, 0xdeadbeef);
    Monstey_AppendU32(bytes, 0x7fc12345);
  }
  for (auto word : {0x3f800000u, 0x80000000u, 0xffc00000u, 0x40000000u,
                    0x40400000u, 0x40800000u})
    Monstey_AppendU32(bytes, word);
  return bytes;
}
B Fixture(const std::vector<B> &shapes, bool prefab = false) {
  B bytes;
  Monstey_AppendU32(bytes, static_cast<std::uint32_t>(shapes.size()));
  Monstey_AppendU32(bytes, static_cast<std::uint32_t>(shapes.size()));
  for (std::size_t i = 0; i < shapes.size(); ++i)
    Monstey_AppendU32(bytes, static_cast<std::uint32_t>(10 + i));
  for (std::size_t i = 0; i < shapes.size(); ++i)
    Monstey_AppendU16(bytes, static_cast<std::uint16_t>(i));
  for (std::size_t i = 0; i < shapes.size(); ++i) {
    const auto def = Definition(prefab);
    bytes.insert(bytes.end(), def.begin(), def.end());
  }
  for (auto word : {0xfafab0b0u, 2u, static_cast<std::uint32_t>(shapes.size())})
    Monstey_AppendU32(bytes, word);
  for (const auto &shape : shapes) {
    const auto transform = Transform();
    bytes.insert(bytes.end(), transform.begin(), transform.end());
    bytes.insert(bytes.end(), shape.begin(), shape.end());
  }
  Monstey_AppendU32(bytes, 0xfafabfbf);
  return bytes;
}
} // namespace
int main(int argc, char **argv) {
  try {
    const auto base = argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1]))
                               : std::filesystem::temp_directory_path();
    const auto dir =
        base /
        ("physics-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    unsigned passed{};
    auto test = [&](const char *label, auto &&call) {
      call();
      ++passed;
      std::cout << "PASS " << label << '\n';
    };
    test("all qualified shapes and exact nonfinite bit preservation", [&] {
      B sphere;
      Monstey_AppendU32(sphere, 3);
      Float(sphere, 1);
      Float(sphere, 2);
      Float(sphere, 3);
      const auto raw = Fixture({Null(), Box(), sphere, Wrapped(Box()),
                                Compound({Null(), Wrapped(Box())}), Tag()},
                               true);
      Monstey_Physics doc(raw);
      Check(doc.Encode() == raw, "Identity changed raw bits");
      const auto report = doc.Describe();
      Check(report.at("shape_count").as_uint() == 6 &&
                report.at("total_shape_nodes").as_uint() == 10,
            "Recursive shapes missing");
      Check(report.at("definitions")[0].at("scalar_raw").is_null(),
            "Nonfinite inherited scalar must not become invented numeric data");
      Check(report.at("definitions")[0].at("vector_bits")[1].as_string() ==
                "0x80000000",
            "Negative zero was lost");
    });
    test("empty native tables", [&] {
      const auto raw = Fixture({});
      Monstey_Physics doc(raw);
      Check(doc.Encode() == raw &&
                doc.Describe().at("shape_count").as_uint() == 0,
            "Empty tables changed");
    });
    test("resize preserves inherited parameters and definition vectors", [&] {
      const auto raw = Fixture({Box()}, true);
      Monstey_Physics doc(raw);
      const auto original = doc.Describe();
      doc.ResizeBox(0, J::array({8, 9, 10}));
      const auto report = doc.Describe();
      Check(report.at("definitions") == original.at("definitions"),
            "Physical definition changed");
      Check(report.at("shapes")[0].at("physical_scalars_raw") ==
                original.at("shapes")[0].at("physical_scalars_raw"),
            "Inherited scalar bytes changed");
      Check(report.at("shapes")[0].at("half_extents_cm")[2].as_number() == 10,
            "Box size was not encoded");
      Monstey_Physics reread(doc.Encode());
      Check(reread.Describe()
                    .at("shapes")[0]
                    .at("half_extents_cm")[0]
                    .as_number() == 8,
            "Box readback failed");
    });
    test("outer rigid transform only changes its seven floats", [&] {
      const auto raw = Fixture({Box()});
      Monstey_Physics doc(raw);
      const auto before = doc.Describe();
      doc.SetTransform(0, J::array({100, 200, 300}), J::array({0, 0, 0, 1}));
      Check(doc.Describe().at("shapes") == before.at("shapes"),
            "Outer transform changed shape geometry");
      const auto out = doc.Encode();
      const auto rigid = out.size() - 4 - Box().size() - 28;
      Check(std::bit_cast<float>(Monstey_U32(out, rigid)) == 100,
            "Rigid position not applied");
      Check(std::equal(raw.end() - 32, raw.end(), out.end() - 32),
            "Geometry/trailer changed");
    });
    test("nested transform and compound route", [&] {
      const auto raw = Fixture({Compound({Null(), Wrapped(Box())})});
      Monstey_Physics doc(raw);
      doc.SetTransform(0, J::array({1, 2, 3}), J::array({0, 0, 0, 1}),
                       std::vector<std::size_t>{1});
      doc.ResizeBox(0, J::array({4, 5, 6}), {1, 0});
      const auto rows = doc.Describe().at("shapes");
      Check(rows.size() == 4 &&
                rows[3].at("half_extents_cm")[1].as_number() == 5,
            "Nested edit failed");
    });
    test("failed edits are transactional", [&] {
      Monstey_Physics doc(Fixture({Box()}));
      const auto before = doc.Encode();
      const auto state = doc.Describe();
      Reject([&] { doc.ResizeBox(0, J::array({0, 1, 1})); });
      Reject([&] { doc.ResizeBox(5, J::array({1, 1, 1})); });
      Reject([&] {
        doc.ResizeBox(0, J::array({std::numeric_limits<double>::max(), 1, 1}));
      });
      Reject([&] {
        doc.SetTransform(0, J::array({1, 2, 3}), J::array({1, 1, 1, 1}));
      });
      Reject([&] {
        doc.SetTransform(0, J::array({1, 2, 3}), J::array({0, 0, 0, 1}),
                         std::vector<std::size_t>{});
      });
      Check(doc.Encode() == before && doc.Describe() == state,
            "Rejected edit mutated state");
    });
    test("rename sorted mapping without geometry mutation", [&] {
      Monstey_Physics doc(Fixture({Box(), Null()}));
      const auto before = doc.Describe();
      doc.RenameMapping(10, 20);
      Check(doc.DefinitionFor(20) == 0 && doc.DefinitionFor(11) == 1,
            "Mapping rename wrong");
      Check(doc.Describe().at("shapes") == before.at("shapes") &&
                doc.Describe().at("definitions") == before.at("definitions"),
            "Mapping rename changed shape data");
      Check(doc.Describe().at("mappings_strict_hash_order").as_bool(),
            "Mapping sort lost");
    });
    test("clone retains native shape and raw parameter bytes", [&] {
      Monstey_Physics doc(Fixture({Wrapped(Box())}, true));
      const auto old = doc.Describe();
      Check(doc.CloneMapping(10, 99) == 1, "Clone ordinal incorrect");
      const auto now = doc.Describe();
      Check(now.at("definitions")[0].at("raw_sha256") ==
                now.at("definitions")[1].at("raw_sha256"),
            "Cloned definition changed");
      Check(now.at("shapes")[0].at("sha256") ==
                now.at("shapes")[2].at("sha256"),
            "Cloned shape changed");
      Check(now.at("definitions")[0] == old.at("definitions")[0],
            "Original definition changed");
    });
    test("cross-file import and duplicate rollback", [&] {
      Monstey_Physics target(Fixture({})), donor(Fixture({Box()}, true));
      const auto before = donor.Encode();
      Check(target.ImportMapping(donor, 10, 123) == 0,
            "Imported definition ordinal");
      Check(target.DefinitionFor(123) == 0, "Imported mapping absent");
      const auto saved = target.Encode();
      Reject([&] { target.ImportMapping(donor, 10, 123); });
      Reject([&] { target.CloneMapping(999, 124); });
      Check(target.Encode() == saved && donor.Encode() == before,
            "Import failure modified source/target");
    });
    test("unknown shape blob remains byte exact and edits are disabled", [&] {
      B shape;
      for (auto word : {999u, 0x11223344u, 0xaabbccddu})
        Monstey_AppendU32(shape, word);
      const auto raw = Fixture({shape});
      Monstey_Physics doc(raw);
      Check(doc.Encode() == raw &&
                !doc.Describe().at("parse_limitation").is_null(),
            "Unknown raw shape was lost");
      Reject([&] { doc.ResizeBox(0, J::array({1, 1, 1})); });
      Reject([&] {
        doc.SetTransform(0, J::array({1, 2, 3}), J::array({0, 0, 0, 1}));
      });
      Reject([&] { doc.CloneMapping(10, 11); });
      doc.RenameMapping(10, 11);
      Check(doc.DefinitionFor(11) == 0,
            "Safe mapping edit unnecessarily disabled");
      const auto encoded = doc.Encode();
      Check(std::equal(raw.end() - 44, raw.end(), encoded.end() - 44),
            "Unknown shape blob changed after mapping edit");
    });
    test("native TAG0 boundaries and wrapper markers", [&] {
      auto raw = Fixture({Tag()});
      auto tag = Tag();
      const auto start = raw.size() - 4 - tag.size();
      Monstey_PutU32(raw, start + 12, 0);
      Reject([&] { Monstey_Physics invalid(raw); });
      raw = Fixture({Tag()});
      Monstey_PutU32(raw, raw.size() - 8, 0);
      Reject([&] { Monstey_Physics invalid(raw); });
      raw = Fixture({Tag()});
      raw[raw.size() - 13] = 'X';
      Reject([&] { Monstey_Physics invalid(raw); });
    });
    test("nesting depth is bounded", [&] {
      auto shape = Null();
      for (unsigned i = 0; i < 64; ++i)
        shape = Wrapped(std::move(shape));
      Monstey_Physics valid(Fixture({shape}));
      shape = Wrapped(std::move(shape));
      Reject([&] { Monstey_Physics invalid(Fixture({shape})); });
    });
    test("mapping and known-shape truncation bounds", [&] {
      const auto valid = Fixture({Box()});
      for (std::size_t size = 0; size < valid.size(); ++size)
        Reject([&] { Monstey_Physics invalid(std::span(valid).first(size)); });
      auto raw = valid;
      Monstey_PutU32(raw, 0, 0xffffffff);
      Reject([&] { Monstey_Physics invalid(raw); });
      raw = valid;
      Monstey_PutU16(raw, 12, 2);
      Reject([&] { Monstey_Physics invalid(raw); });
    });
    test("paged shape report does not alter definitions or data", [&] {
      Monstey_Physics doc(Fixture({Compound({Null(), Wrapped(Box())})}));
      const auto full = doc.Describe();
      const auto page = doc.Describe(2, 1);
      Check(page.at("shapes").size() == 1 &&
                page.at("total_shape_nodes").as_uint() == 4,
            "Page count wrong");
      Check(page.at("shapes")[0] == full.at("shapes")[2], "Page row differs");
      Reject([&] { (void)doc.Describe(0, 0); });
      Reject([&] { (void)doc.Describe(0, 501); });
    });
    if (argc > 2) {
      const auto source = std::filesystem::path(Monstey_Wide(argv[2]));
      const auto raw = Monstey_ReadFile(source);
      test("actual native payload identity", [&] {
        Monstey_Physics doc(raw);
        Check(doc.Encode() == raw, "Real native physical payload changed");
        Monstey_WriteNewText(dir / "native-physics.json",
                             doc.Describe().dump(2));
      });
      test("actual native transform preserves every shape and definition", [&] {
        Monstey_Physics doc(raw);
        const auto before = doc.Describe();
        Check(before.at("parse_limitation").is_null() &&
                  before.at("shape_count").as_uint() > 0,
              "Native source has no qualified shape");
        doc.SetTransform(0, J::array({100, 200, 300}), J::array({0, 0, 0, 1}));
        const auto shifted = doc.Encode();
        Check(shifted != raw, "Actual rigid transform did not change payload");
        Check(doc.Describe().at("definitions") == before.at("definitions") &&
                  doc.Describe().at("shapes") == before.at("shapes"),
              "Actual transform changed shape/definition bytes");
        Monstey_WriteNewFile(dir / "native-physics-transform.bin", shifted);
      });
      test("actual native clone retains all old definition bytes", [&] {
        Monstey_Physics doc(raw);
        const auto before = doc.Describe();
        const auto oldHash = static_cast<std::uint32_t>(std::stoull(
            before.at("mapping_rows")[0].at("owner_hash").as_string(), nullptr,
            16));
        auto newHash = Monstey_NativeHash("Monstey_PhysicsNative_Probe");
        const auto mappings = before.at("mapping_rows").as_array();
        while (
            std::any_of(mappings.begin(), mappings.end(), [&](const auto &row) {
              return row.at("owner_hash").as_string() == Monstey_Hex(newHash);
            }))
          ++newHash;
        doc.CloneMapping(oldHash, newHash);
        const auto cloned = doc.Describe();
        Check(cloned.at("shape_count").as_uint() ==
                  before.at("shape_count").as_uint() + 1,
              "Native clone shape missing");
        for (std::size_t i = 0; i < before.at("definitions").size(); ++i)
          Check(cloned.at("definitions")[i] == before.at("definitions")[i],
                "Native clone changed an old definition");
        Check(Monstey_ReadFile(source) == raw, "Native source file changed");
        Monstey_WriteNewFile(dir / "native-physics-clone.bin", doc.Encode());
        Monstey_WriteNewText(dir / "native-physics-clone.json", cloned.dump(2));
      });
    }
    Monstey_WriteNewText(
        dir / "physics-tests.json",
        J::object(
            {{"passed", passed}, {"failed", 0}, {"implementation", "C++20"}})
            .dump(2));
    std::cout << passed << " physics test groups passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
