#include "Monstey_MaterialProject.hpp"
#include "Monstey_Runtime.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
void Monstey_Test(bool ok, const char *msg) {
  if (!ok)
    throw Monstey_Error("test_failure", msg);
}
void Monstey_RawWrite(const std::filesystem::path &p, const B &b) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f.write(reinterpret_cast<const char *>(b.data()),
          static_cast<std::streamsize>(b.size()));
  Monstey_Test(bool(f), "write failed");
}
B Monstey_Fixture(unsigned version) {
  B b(30, 0);
  Monstey_PutU32(b, 0, 0x494d544c);
  Monstey_PutU32(b, 4, version);
  Monstey_PutU32(b, 8, 0x12345678);
  Monstey_PutU32(b, 12, 48);
  Monstey_PutU16(b, 16, 3);
  Monstey_PutU16(b, 18, 2);
  Monstey_PutU32(b, 20, 0xaabbccdd);
  Monstey_PutU32(b, 24, 0x01020304);
  Monstey_PutU16(b, 28, 4);
  b.insert(b.end(), {'f', 'x', 'o', 0});
  B v(48, 0x70);
  Monstey_PutU32(v, 0, 0x3f800000);
  Monstey_PutU32(v, 4, 0xffc01234);
  Monstey_PutU32(v, 8, 0x11223344);
  v[16] = 't';
  v[17] = 'e';
  v[18] = 'x';
  v[19] = 0;
  Monstey_PutU32(v, 24, 1);
  v[32] = 's';
  v[33] = 'u';
  v[34] = 'r';
  v[35] = 'f';
  v[36] = 0;
  b.insert(b.end(), v.begin(), v.end());
  if (version == 7) {
    Monstey_AppendU32(b, 3);
    b.insert(b.end(), {0x91, 0x92, 0x93});
  }
  auto prop = [&](unsigned type, B name, unsigned offset) {
    Monstey_AppendU16(b, static_cast<std::uint16_t>(type));
    b.push_back(0x42);
    b.push_back(0x21);
    Monstey_AppendU16(b, static_cast<std::uint16_t>(name.size()));
    b.insert(b.end(), name.begin(), name.end());
    Monstey_AppendU32(b, offset);
    if (version == 7)
      Monstey_AppendU16(b, 0x789a);
  };
  prop(5, {'s', 'c', 'a', 'l', 'a', 'r'}, 0);
  prop(5, {0xff, 'n', 0, 'z'}, 4);
  prop(4, {'w', 'o', 'r', 'd'}, 8);
  prop(1, {'t', 'e', 'x'}, 16);
  prop(6, {'b', 'o', 'o', 'l'}, 24);
  b.insert(b.end(), {0xd1, 0xd2, 0xd3});
  return b;
}
J Monstey_Set(double value) {
  return J::object(
      {{"op", "set"}, {"table", "a"}, {"index", 0}, {"value", value}});
}
template <class F> void Monstey_Fails(F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::exception &) {
    failed = true;
  }
  Monstey_Test(failed, "expected failure");
}
std::string Monstey_Path(const std::filesystem::path &p) {
  return Monstey_Utf8(p.wstring());
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Test(argc == 3, "tests need output directory and old catalog JSON");
    const std::filesystem::path root = argv[1];
    Monstey_Test(!std::filesystem::exists(root), "test root must be new");
    std::filesystem::create_directories(root);
    auto groups = J::array();
    auto pass = [&](const char *s) { groups.as_array().push_back(s); };
    for (unsigned version : {7u, 8u}) {
      const auto prefix = std::string("v") + std::to_string(version);
      const auto dir = root / prefix;
      std::filesystem::create_directories(dir);
      const auto source = dir / L"source.mat";
      const auto bytes = Monstey_Fixture(version);
      Monstey_WriteNewFile(source, bytes);
      const auto sha = Monstey_HashSha256(bytes);
      Monstey_MaterialProject p(source, sha);
      auto d = p.Describe();
      Monstey_Test(!d["dirty"].as_bool() &&
                       d["material"]["entries"].size() == 5,
                   "open metadata");
      Monstey_Test(
          d["material"]["entries"][1]["raw_name_hex"].as_string() ==
                  "ff6e007a" &&
              d["material"]["entries"][1]["value_bytes_hex"].as_string() ==
                  "3412c0ff",
          "raw metadata");
      p.Build(dir / L"noop.mat");
      Monstey_Test(Monstey_ReadFile(dir / L"noop.mat") == bytes, "noop exact");
      pass((prefix + " open/raw/noop").c_str());
      const auto old = p.Describe();
      Monstey_Fails([&] {
        p.Edit(J::array({Monstey_Set(2.0), J::object({{"op", "set"},
                                                      {"table", "b"},
                                                      {"index", 99},
                                                      {"value", true}})}));
      });
      Monstey_Test(p.Describe() == old, "batch rollback");
      pass((prefix + " atomic batch rollback").c_str());
      p.Edit(J::array({Monstey_Set(2.0)}));
      const auto hash2 = p.Describe()["effective_sha256"];
      p.Edit(J::array({Monstey_Set(3.0)}));
      const auto hash3 = p.Describe()["effective_sha256"];
      p.Undo();
      Monstey_Test(p.Describe()["effective_sha256"] == hash2 &&
                       p.Describe()["can_redo"].as_bool(),
                   "undo");
      p.Redo();
      Monstey_Test(p.Describe()["effective_sha256"] == hash3, "redo");
      p.Revert();
      Monstey_Test(!p.Describe()["modified"].as_bool(), "revert");
      p.Undo();
      Monstey_Test(p.Describe()["effective_sha256"] == hash3, "undo revert");
      pass((prefix + " undo redo revert").c_str());
      p.Undo();
      const auto project = dir / L"journal.json";
      p.SaveProject(project);
      auto versioned = J::Parse(Monstey_ReadText(project));
      Monstey_Test(versioned.at("version") == J(2),
                   "First integrated COW journal must identify version2");
      versioned["version"] = 1;
      const auto experimental = dir / L"experimental-v1.json";
      Monstey_WriteNewText(experimental, versioned.dump());
      Monstey_Fails(
          [&] { Monstey_MaterialProject::LoadProject(experimental); });
      pass(
          (prefix + " reject experimental allocator journal version1").c_str());
      auto loaded = Monstey_MaterialProject::LoadProject(project);
      Monstey_Test(loaded->Describe()["effective_sha256"] == hash2 &&
                       loaded->Describe()["can_redo"].as_bool(),
                   "redo persisted");
      loaded->Redo();
      Monstey_Test(loaded->Describe()["effective_sha256"] == hash3,
                   "replay redo");
      pass((prefix + " project full redo replay").c_str());
      p.Edit(J::array({Monstey_Set(4.0)}));
      Monstey_Test(!p.Describe()["can_redo"].as_bool(), "branch truncate");
      const auto state = p.Describe();
      Monstey_Fails([&] { p.Redo(); });
      Monstey_Test(p.Describe() == state, "failed redo preserved");
      pass((prefix + " branch and redo failure").c_str());
      auto bad = J::Parse(Monstey_ReadText(project));
      bad["steps"][1]["after_sha256"] = std::string(64, '0');
      const auto badPath = dir / L"bad-redo.json";
      Monstey_WriteNewText(badPath, bad.dump());
      Monstey_Fails([&] { Monstey_MaterialProject::LoadProject(badPath); });
      bad = J::Parse(Monstey_ReadText(project));
      bad["native_baseline_sha256"] = std::string(64, '0');
      Monstey_WriteNewText(dir / L"bad-base.json", bad.dump());
      Monstey_Fails([&] {
        Monstey_MaterialProject::LoadProject(dir / L"bad-base.json");
      });
      pass((prefix + " reject corrupt replay and baseline").c_str());
      const auto pathsState = p.Describe();
      Monstey_Fails([&] { p.Build(source); });
      Monstey_Fails([&] { p.SaveProject(source); });
      Monstey_Fails([&] { loaded->Build(project); });
      Monstey_WriteNewText(dir / L"sentinel.mat", "sentinel");
      Monstey_Fails([&] { p.Build(dir / L"sentinel.mat"); });
      Monstey_Test(Monstey_ReadText(dir / L"sentinel.mat") == "sentinel" &&
                       p.Describe() == pathsState,
                   "path protection");
      pass((prefix + " existing/source/project output protection").c_str());
      p.Build(dir / L"edited.mat");
      const auto built =
          Monstey_LtmiRead(Monstey_ReadFile(dir / L"edited.mat"));
      Monstey_Test(built.shader_id == 0x12345678 &&
                       built.unknown_20 == 0xaabbccdd &&
                       built.trailer == B({0xd1, 0xd2, 0xd3}),
                   "headers/trailer retained");
      Monstey_Test(Monstey_LtmiValue(built, built.tables[0][1]) ==
                       B({0x34, 0x12, 0xc0, 0xff}),
                   "nan retained");
      pass((prefix + " changed build exact preservation").c_str());
      const auto ops = dir / L"operations.json";
      Monstey_WriteNewText(ops, J::array({Monstey_Set(5.0)}).dump());
      p.EditFile(ops, Monstey_HashSha256(ops));
      Monstey_Fails([&] { p.EditFile(ops, std::string(64, '0')); });
      const auto compact = p.Changes(J::object(
          {{"include_operations", false}, {"offset", 1}, {"limit", 1}}));
      Monstey_Test(compact["steps"].size() == 1 &&
                       !compact["steps"][0].contains("operations"),
                   "paged history");
      pass((prefix + " qualified operation file and history").c_str());
      const auto pj = Monstey_ReadFile(project);
      auto changed = pj;
      changed.push_back(' ');
      Monstey_RawWrite(project, changed);
      Monstey_Fails([&] { loaded->Describe(); });
      Monstey_Fails([&] { loaded->Build(dir / L"project-changed.mat"); });
      Monstey_Test(!std::filesystem::exists(dir / L"project-changed.mat"),
                   "changed project no publish");
      Monstey_RawWrite(project, pj);
      pass((prefix + " loaded project source guard").c_str());
      auto changedSource = bytes;
      changedSource.back() ^= 1;
      Monstey_RawWrite(source, changedSource);
      Monstey_Fails([&] { p.Edit(J::array({Monstey_Set(6.0)})); });
      Monstey_Fails([&] { p.Undo(); });
      Monstey_Fails([&] { p.Redo(); });
      Monstey_Fails([&] { p.Revert(); });
      Monstey_Fails([&] { p.Build(dir / L"changed-source.mat"); });
      Monstey_Fails([&] { Monstey_MaterialProject::LoadProject(project); });
      Monstey_Test(!std::filesystem::exists(dir / L"changed-source.mat"),
                   "changed source no publish");
      Monstey_RawWrite(source, bytes);
      Monstey_Test(Monstey_HashSha256(source) == sha,
                   "source restored fixture only");
      pass((prefix + " current source guard all mutation paths").c_str());
    }
    auto direct = Monstey_LtmiRead(Monstey_Fixture(8));
    const auto before = Monstey_LtmiBuild(direct);
    Monstey_Fails([&] {
      Monstey_LtmiApplyOperations(
          direct, J::array({Monstey_Set(2.0),
                            J::object({{"op", "unknown"}, {"table", "a"}})}));
    });
    Monstey_Test(Monstey_LtmiBuild(direct) == before, "shared parser atomic");
    Monstey_LtmiApplyOperations(
        direct,
        J::array(
            {J::object({{"op", "add"},
                        {"table", "b"},
                        {"name", "Added"},
                        {"type_u16", 5},
                        {"value", 7.0}}),
             J::object({{"op", "rename"},
                        {"table", "b"},
                        {"index", 2},
                        {"raw_name_hex", "ff00aa"}}),
             J::object({{"op", "set"},
                        {"table", "b"},
                        {"index", 2},
                        {"bytes_hex", "3412c0ff"}}),
             J::object({{"op", "remove"}, {"table", "b"}, {"index", 0}})}));
    Monstey_Test(direct.tables[1].size() == 2 &&
                     direct.tables[1][1].name == B({0xff, 0, 0xaa}),
                 "shared CRUD");
    pass("shared parser atomic and exact CRUD");
    Monstey_Runtime runtime({}, root / L"settings.json");
    auto call = [&](const char *op, J args = J::object()) {
      return runtime.Call(J::object({{"operation", op}, {"arguments", args}}));
    };
    auto ok = [&](const char *op, J args = J::object()) {
      auto reply = call(op, args);
      if (!reply["ok"].as_bool())
        throw Monstey_Error("runtime_failure", reply.dump());
      return reply;
    };
    auto oldCatalog = J::Parse(Monstey_ReadText(argv[2]));
    const auto catalog = Monstey_Runtime::Catalog();
    Monstey_Test(oldCatalog.size() == 208 && catalog.size() == 227,
                 "catalog size");
    for (const auto &row : oldCatalog.as_array()) {
      const auto it =
          std::find_if(catalog.as_array().begin(), catalog.as_array().end(),
                       [&](const auto &v) { return v["name"] == row["name"]; });
      auto expected = row;
      if (row["name"] == J("database_build") || row["name"] == J("fx_build")) {
        auto &schema = expected["inputSchema"];
        schema["properties"]["native_declarations"] = J::object(
            {{"type", "string"}, {"format", "path"}, {"minLength", 1}});
        schema["properties"]["expected_native_declarations_sha256"] =
            J::object({{"type", "string"},
                       {"minLength", 64},
                       {"maxLength", 64},
                       {"pattern", "^[0-9a-f]{64}$"}});
        schema["dependentRequired"] =
            J::object({{"native_declarations",
                        J::array({"expected_native_declarations_sha256"})},
                       {"expected_native_declarations_sha256",
                        J::array({"native_declarations"})}});
      }
      Monstey_Test(it != catalog.as_array().end() && *it == expected,
                   "catalog changed beyond the explicit paired native "
                   "declaration build arguments");
    }
    pass("Runtime preserves all old descriptors except the exact paired "
         "build-profile extension");
    const auto source = root / L"v8/source.mat";
    auto opened =
        ok("material_open", J::object({{"path", Monstey_Path(source)}}));
    const auto gen = opened["generation"], rev = opened["revision"];
    auto stale = call("material_edit",
                      J::object({{"operations", J::array({Monstey_Set(9.0)})},
                                 {"expected_generation", gen.as_uint() + 1},
                                 {"expected_revision", rev}}));
    Monstey_Test(!stale["ok"].as_bool() &&
                     stale["error"]["code"].as_string() == "stale_session" &&
                     stale["revision"] == rev,
                 "generation guard");
    stale = call("material_edit",
                 J::object({{"operations", J::array({Monstey_Set(9.0)})},
                            {"expected_generation", gen},
                            {"expected_revision", rev.as_uint() + 1}}));
    Monstey_Test(!stale["ok"].as_bool() &&
                     stale["error"]["code"].as_string() == "stale_revision",
                 "revision guard");
    pass("Runtime stale guards and failure revision stability");
    auto edited = ok("material_edit",
                     J::object({{"operations", J::array({Monstey_Set(9.0)})},
                                {"expected_generation", gen},
                                {"expected_revision", rev}}));
    Monstey_Test(edited["generation"] == gen &&
                     edited["revision"].as_uint() == rev.as_uint() + 1,
                 "edit revision");
    const auto editRev = edited["revision"];
    auto desc = ok("material_describe");
    Monstey_Test(desc["revision"] == editRev &&
                     desc["result"]["dirty"].as_bool(),
                 "readonly revision");
    auto badOpen = call("material_open",
                        J::object({{"path", Monstey_Path(source)},
                                   {"expected_sha256", std::string(64, '0')}}));
    Monstey_Test(!badOpen["ok"].as_bool() &&
                     ok("material_describe")["result"]["dirty"].as_bool(),
                 "bad open retains old session");
    pass("Runtime open/edit/read generation and revision contract");
    ok("material_save_project",
       J::object({{"path", Monstey_Path(root / L"runtime.json")}}));
    ok("material_build",
       J::object({{"output", Monstey_Path(root / L"runtime.mat")}}));
    ok("material_undo");
    ok("material_redo");
    ok("material_revert");
    ok("material_changes", J::object({{"include_operations", false}}));
    auto load = ok("material_load_project",
                   J::object({{"path", Monstey_Path(root / L"runtime.json")}}));
    Monstey_Test(load["generation"].as_uint() == gen.as_uint() + 1,
                 "load generation");
    pass("Runtime all ten operations execute");
    Monstey_Runtime protectedRuntime({}, source);
    auto protectedCall = [&](const char *op, J args) {
      return protectedRuntime.Call(
          J::object({{"operation", op}, {"arguments", args}}));
    };
    Monstey_Test(
        protectedCall("material_open",
                      J::object({{"path", Monstey_Path(source)}}))["ok"]
            .as_bool(),
        "protected source open");
    const auto sourceHash = Monstey_HashSha256(source);
    const auto configured =
        protectedCall("configure", J::object({{"save", true}}));
    Monstey_Test(!configured["ok"].as_bool() &&
                     Monstey_HashSha256(source) == sourceHash,
                 "settings cannot overwrite material source");
    pass("Runtime settings source protection");
    const auto report =
        J::object({{"schema", "Monstey.MaterialProjectTests/1"},
                   {"groups", groups},
                   {"passed", static_cast<std::uint64_t>(groups.size())},
                   {"failed", 0},
                   {"product_runtime", "C++20"},
                   {"game_written", false},
                   {"original_program_executed", false}});
    Monstey_WriteNewText(root / L"report.json", report.dump(2) + "\n");
    std::cout << groups.size() << " material project groups PASS\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}