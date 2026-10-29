#include "Monstey_PhysicsWorld.hpp"
#include "Monstey_World.hpp"
#include <chrono>
#include <iostream>
using namespace Monstey;
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
void Check(bool v, const char *s) {
  if (!v)
    throw std::runtime_error(s);
}
template <class F> void Reject(F &&f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::exception &) {
    rejected = true;
  }
  Check(rejected, "Expected rejection");
}
std::string Text(const std::filesystem::path &p) {
  return Monstey_Utf8(p.wstring());
}
J Profile(const std::filesystem::path &p, J route) {
  return J::object({{"source_sha256", Monstey_HashSha256(p)},
                    {"shape_index", 0},
                    {"route", route}});
}
} // namespace
int main(int argc, char **argv) {
  try {
    const auto dir =
        (argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1]))
                  : std::filesystem::temp_directory_path()) /
        ("triangle-api-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    unsigned passed = 0;
    const auto test = [&](const char *s, auto &&f) {
      f();
      ++passed;
      std::cout << "PASS " << s << '\n';
    };
    const auto source = dir / L"synthetic.bin";
    Monstey_WriteNewFile(source, B{1, 2, 3});
    test("source hash precondition rejects before profile access or output",
         [&] {
           Reject([&] {
             Monstey_PhysicsWorldCall(
                 "sow_world_collision_triangles_build",
                 J::object(
                     {{"source", Text(source)},
                      {"profile_source", Text(dir / L"absent-profile.objres")},
                      {"spec", J::object()},
                      {"output", Text(dir / L"failed" / L"no.objres")},
                      {"expected_sha256", std::string(64, '0')}}));
           });
           Check(!std::filesystem::exists(dir / L"failed"),
                 "Rejected source hash created outputs");
         });
    test("unknown triangle API fields reject", [&] {
      Reject([&] {
        Monstey_PhysicsWorldCall(
            "sow_world_collision_triangles_build",
            J::object({{"source", Text(source)},
                       {"profile_source", Text(source)},
                       {"spec", J::object()},
                       {"output", Text(dir / L"bad.objres")},
                       {"donor_guess", Text(source)}}));
      });
      Check(!std::filesystem::exists(dir / L"bad.objres"),
            "Unknown field published");
    });
    test("triangle schemas retain explicit donor and strict request keys", [&] {
      const auto rows = Monstey_WorldCatalog();
      bool found = false;
      for (const auto &row : rows.as_array())
        if (row.at("name").as_string() ==
            "sow_world_collision_triangles_build") {
          const auto schema = row.at("inputSchema");
          Check(!schema.at("additionalProperties").as_bool(),
                "Triangle top-level schema is open");
          Check(schema.at("properties").contains("profile_donor_source") &&
                    schema.at("properties").contains("profile_donor"),
                "Explicit donor schema missing");
          found = true;
        }
      Check(found, "Original triangle operation missing");
    });
    if (argc > 4) {
      const auto native = std::filesystem::path(Monstey_Wide(argv[2])),
                 profile = std::filesystem::path(Monstey_Wide(argv[3])),
                 donor = std::filesystem::path(Monstey_Wide(argv[4]));
      const auto sourceHash = Monstey_HashSha256(native),
                 profileHash = Monstey_HashSha256(profile),
                 donorHash = Monstey_HashSha256(donor);
      auto spec = J::object(
          {{"source_sha256", sourceHash},
           {"profile", Profile(profile, J::array({0, 0}))},
           {"vertices_cm",
            J::array({J::array({0, 0, 0}), J::array({0, 0, 100}),
                      J::array({100, 0, 100}), J::array({100, 0, 0})})},
           {"triangles", J::array({J::array({0, 1, 2}), J::array({0, 2, 3})})},
           {"max_error_cm", 0.02}});
      const auto args = [&](const std::filesystem::path &output) {
        return J::object({{"source", Text(native)},
                          {"profile_source", Text(profile)},
                          {"output", Text(output)},
                          {"spec", spec},
                          {"profile_donor_source", Text(donor)},
                          {"profile_donor", Profile(donor, J::array({1, 0}))}});
      };
      test("native World API prepares explicit hashes and publishes real "
           "triangle collision",
           [&] {
             const auto output = dir / L"native-triangles.objres",
                        reportPath = dir / L"native-triangles.json";
             auto a = args(output);
             a["report"] = Text(reportPath);
             auto r =
                 Monstey_WorldCall("sow_world_collision_triangles_build", a);
             Check(r.at("cooked").at("triangles").as_uint() == 2,
                   "Native triangle count differs");
             Check(!r.at("profile_preparation").is_null() &&
                       r.at("prepared_profile_sha256") !=
                           r.at("profile_input_sha256"),
                   "Explicit THSH preparation missing");
             Check(Monstey_HashSha256(output) == r.at("sha256").as_string(),
                   "Published triangle hash differs");
             Check(J::Parse(Monstey_ReadText(reportPath)) == r,
                   "Triangle report differs");
           });
      test("incomplete profile fails without explicit donor", [&] {
        auto a = args(dir / L"incomplete.objres");
        a.as_object().erase("profile_donor");
        a.as_object().erase("profile_donor_source");
        Reject([&] {
          Monstey_WorldCall("sow_world_collision_triangles_build", a);
        });
        Check(!std::filesystem::exists(dir / L"incomplete.objres"),
              "Unqualified profile published");
      });
      test("donor pair and source snapshot are mandatory", [&] {
        auto a = args(dir / L"bad-donor.objres");
        a["profile_donor"]["source_sha256"] = std::string(64, '0');
        Reject([&] {
          Monstey_WorldCall("sow_world_collision_triangles_build", a);
        });
        a = args(dir / L"unpaired.objres");
        a.as_object().erase("profile_donor");
        Reject([&] {
          Monstey_WorldCall("sow_world_collision_triangles_build", a);
        });
        Check(!std::filesystem::exists(dir / L"bad-donor.objres") &&
                  !std::filesystem::exists(dir / L"unpaired.objres"),
              "Invalid donor published");
      });
      test("all three source files are protected at output and report "
           "publication",
           [&] {
             auto a = args(dir / L"protected.objres");
             a["report"] = Text(donor);
             Reject([&] {
               Monstey_WorldCall("sow_world_collision_triangles_build", a);
             });
             Check(!std::filesystem::exists(dir / L"protected.objres"),
                   "Report source collision published data");
             Reject([&] {
               Monstey_WorldCall("sow_world_collision_triangles_build",
                                 args(native));
             });
             Check(Monstey_HashSha256(native) == sourceHash &&
                       Monstey_HashSha256(profile) == profileHash &&
                       Monstey_HashSha256(donor) == donorHash,
                   "Native authoring source changed");
           });
    }
    std::cout << passed << " native triangle API groups passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}