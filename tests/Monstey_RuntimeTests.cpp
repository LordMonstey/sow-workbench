#include "Monstey_Blend.hpp"
#include "Monstey_Model.hpp"
#include "Monstey_Runtime.hpp"
#include "Monstey_World.hpp"
#include "Monstey_WorldComposition.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
#include <thread>

using namespace Monstey;
using J = Monstey_Json;
namespace {
void Check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
J Call(Monstey_Runtime &runtime, const char *operation, J args = J::object()) {
  const auto response =
      runtime.Call(J::object({{"operation", operation}, {"arguments", args}}));
  Check(response.at("ok").as_bool(), response.dump().c_str());
  return response;
}
void Error(Monstey_Runtime &runtime, const char *operation, J args,
           const char *code) {
  const auto response =
      runtime.Call(J::object({{"operation", operation}, {"arguments", args}}));
  Check(!response.at("ok").as_bool() &&
            response.at("error").at("code").as_string() == code,
        response.dump().c_str());
}
void Monstey_RuntimeStaleBandGlb(const std::filesystem::path &source,
                                 const std::filesystem::path &output) {
  auto bytes = Monstey_ReadFile(source);
  Check(bytes.size() >= 20 && Monstey_U32(bytes, 0) == 0x46546c67 &&
            Monstey_U32(bytes, 4) == 2 && Monstey_U32(bytes, 8) == bytes.size(),
        "Invalid generated GLB fixture");
  J document;
  std::size_t binary = 0, binarySize = 0;
  for (std::size_t at = 12; at < bytes.size();) {
    Check(at + 8 <= bytes.size(), "Truncated generated GLB chunk");
    const auto size = static_cast<std::size_t>(Monstey_U32(bytes, at));
    Check(size <= bytes.size() - at - 8, "Generated GLB chunk exceeds file");
    if (Monstey_U32(bytes, at + 4) == 0x4e4f534a)
      document = J::Parse(std::string_view(
          reinterpret_cast<const char *>(bytes.data() + at + 8), size));
    if (Monstey_U32(bytes, at + 4) == 0x004e4942) {
      binary = at + 8;
      binarySize = size;
    }
    at += 8 + size;
  }
  Check(binary != 0 && document.is_object(), "Generated GLB has no data");
  const auto &primitive = document.at("meshes").at(0).at("primitives").at(0);
  const auto &accessor =
      document.at("accessors")
          .at(static_cast<std::size_t>(primitive.at("indices").as_uint()));
  const auto &view =
      document.at("bufferViews")
          .at(static_cast<std::size_t>(accessor.at("bufferView").as_uint()));
  const auto type = accessor.at("componentType").as_uint();
  Check(accessor.at("count").as_uint() >= 3 && (type == 5123 || type == 5125),
        "Generated GLB fixture needs a 16-bit or 32-bit triangle");
  const std::size_t width = type == 5123 ? 2 : 4;
  const auto offset =
      static_cast<std::size_t>(view.value("byteOffset", 0u).as_uint() +
                               accessor.value("byteOffset", 0u).as_uint());
  const auto stride =
      static_cast<std::size_t>(view.value("byteStride", width).as_uint());
  Check(stride >= width && offset <= binarySize &&
            2 * stride + width <= binarySize - offset,
        "Generated GLB triangle exceeds binary chunk");
  const auto a = binary + offset + stride, b = a + stride;
  Check(!std::equal(bytes.begin() + a, bytes.begin() + a + width,
                    bytes.begin() + b),
        "Generated GLB triangle cannot demonstrate a topology change");
  for (std::size_t i = 0; i < width; ++i)
    std::swap(bytes[a + i], bytes[b + i]);
  Monstey_WriteNewFile(output, bytes);
}
void Monstey_RuntimeUnassignedBandBlend(const std::filesystem::path &source,
                                        const std::filesystem::path &output) {
  Monstey_Blend blend(source);
  const auto meshes = blend.Blocks("ME");
  for (const auto &row : meshes.at("rows").as_array()) {
    const auto mesh = static_cast<std::size_t>(row.at("index").as_uint());
    if (!blend.ReadField(mesh, "id.name").as_string().starts_with("MESoW_C"))
      continue;
    const auto address =
        blend.ReadField(mesh, "attribute_storage.dna_attributes")
            .at("pointer")
            .as_uint();
    const auto attributes = blend.BlockAtAddress(address, mesh);
    const auto count =
        blend.ReadField(mesh, "attribute_storage.dna_attributes_num").as_uint();
    for (std::size_t i = 0; i < count; ++i) {
      const auto nameAddress =
          blend.ReadField(attributes, "name", i).at("pointer").as_uint();
      const auto nameBytes =
          blend.BlockData(blend.BlockAtAddress(nameAddress, attributes));
      const std::string name(nameBytes.begin(),
                             std::find(nameBytes.begin(), nameBytes.end(), 0));
      if (name != "monstey_triangle_band")
        continue;
      const auto array = blend.BlockAtAddress(
          blend.ReadField(attributes, "data", i).at("pointer").as_uint(),
          attributes);
      const auto data = blend.BlockAtAddress(
          blend.ReadField(array, "data").at("pointer").as_uint(), array);
      auto bytes = blend.BlockData(data);
      Check(bytes.size() >= 4, "Generated band attribute has no face");
      Monstey_PutU32(bytes, 0, 0);
      blend.ReplaceData(data, std::move(bytes));
      (void)blend.Write(output);
      return;
    }
  }
  throw std::runtime_error("Generated Blender fixture has no native band");
}
void Monstey_RuntimeCheckBand(const std::filesystem::path &mesh,
                              std::uint8_t band) {
  const auto scene = Monstey_ModelReadScene(mesh);
  Check(!scene.parts.empty(), "Native model route wrote no geometry");
  for (const auto &part : scene.parts)
    Check(!part.triangle_bands.empty() &&
              part.triangle_bands.size() == part.indices.size() / 3 &&
              part.triangle_band_topology_sha256 ==
                  Monstey_ModelTriangleTopologyHash(part.indices) &&
              std::all_of(part.triangle_bands.begin(),
                          part.triangle_bands.end(),
                          [band](const auto item) { return item == band; }),
          "Native model route lost explicit face-band membership");
}
J Monstey_AnimationScene() {
  const auto clip = [](const char *name, int end) {
    return J::object(
        {{"name", name},
         {"duration_ticks", 1000},
         {"ticks", J::array({0, 1000})},
         {"events", J::array()},
         {"tracks", J::array({J::object(
                        {{"channel_index", 0},
                         {"values", J::array({J::array({0, 0, 0}),
                                              J::array({end, 0, 0})})}})})}});
  };
  return J::object(
      {{"format", "Monstey_ANIX_Semantic_v1"},
       {"library_path",
        "characters/animationcharacters/runtime/synthetic.anix"},
       {"channels", J::array({J::object({{"type_hash", "00ce66fc"},
                                         {"bone_hash", "88baa7e7"}})})},
       {"clips", J::array({clip("bind", 0), clip("walk", 20)})}});
}
} // namespace
int main(int argc, char **argv) {
  try {
    auto base = argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1]))
                         : std::filesystem::temp_directory_path();
    const auto directory =
        base /
        ("runtime-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    Monstey_Runtime runtime({}, directory / "settings.json");
    unsigned passed{};
    auto test = [&](const char *name, auto &&run) {
      run();
      ++passed;
      std::cout << "PASS " << name << '\n';
    };
    test("native catalog schemas", [&] {
      const auto catalog = Monstey_Runtime::Catalog();
      Check(catalog.size() >= 118,
            "missing native or original World operations");
      std::set<std::string> names;
      for (const auto &entry : catalog.as_array()) {
        Check(names.insert(entry.at("name").as_string()).second,
              "duplicate operation name");
        Check(entry.at("implementation").as_string() == "C++20" &&
                  entry.at("inputSchema").at("type").as_string() == "object" &&
                  entry.at("read_only").is_bool(),
              "invalid native schema");
        Check(entry.contains("status")
                  ? entry.at("implemented").as_bool() ==
                        (entry.at("status").as_string() == "complete")
                  : entry.at("implemented").as_bool(),
              "catalog misreports an incomplete operation");
      }
    });
    test("original World routes preserve schemas and native status", [&] {
      const auto catalog = Monstey_Runtime::Catalog();
      const auto world = Monstey_WorldCatalog();
      Check(world.size() == 41, "World registry count differs");
      for (const auto &original : world.as_array()) {
        const J *found = nullptr;
        for (const auto &entry : catalog.as_array())
          if (entry.at("name") == original.at("name"))
            found = &entry;
        Check(found != nullptr, "original World name is not directly callable");
        auto expected = original.at("inputSchema");
        for (const char *field : {"expected_generation", "expected_revision"})
          expected["properties"][field] =
              J::object({{"type", "integer"}, {"minimum", 0}});
        Check(found->at("inputSchema") == expected &&
                  found->at("implemented") == original.at("implemented") &&
                  found->at("available") == original.at("available") &&
                  found->at("read_only") ==
                      original.at("annotations").at("readOnlyHint"),
              "direct World descriptor diverges from its native handler");
      }
      const auto before = Call(runtime, "describe");
      const auto direct =
          Call(runtime, "sow_capabilities",
               J::object({{"expected_generation", before.at("generation")},
                          {"expected_revision", before.at("revision")}}));
      Check(direct.at("revision") == before.at("revision"),
            "read-only World route changed the session revision");
      const auto wrapped = Call(runtime, "world_call",
                                J::object({{"name", "sow_capabilities"},
                                           {"arguments", J::object()}}));
      Check(direct.at("result") == wrapped.at("result"),
            "direct and wrapped routes use different World implementations");
      Error(runtime, "sow_objres_inspect", J::object(), "missing_argument");
      Error(runtime, "sow_capabilities", J::object({{"unexpected", 1}}),
            "unknown_argument");
      Error(runtime, "sow_capabilities",
            J::object({{"expected_generation", 999}}), "stale_session");
    });
    test("World resource route strips only session guards", [&] {
      const auto source = directory / "fixture.bndl";
      Monstey_WriteNewFile(
          source,
          Monstey_BuildBundle(Monstey_Bundle{
              "Test", {{"models/demo.mesh", 0}, {"textures/demo.tex", 0}}}));
      const auto before = Call(runtime, "describe");
      const auto args =
          J::object({{"source", Monstey_Utf8(source.wstring())},
                     {"expected_generation", before.at("generation")},
                     {"expected_revision", before.at("revision")}});
      const auto direct = Call(runtime, "sow_asset_inspect", args);
      Check(direct.at("revision") == before.at("revision"),
            "read-only resource inspection mutated session");
      const auto wrapped =
          Call(runtime, "world_call",
               J::object({{"name", "sow_asset_inspect"}, {"arguments", args}}));
      Check(direct.at("result") == wrapped.at("result"),
            "native inspection received transport-only guard arguments");
    });
    test("no implicit source", [&] {
      Error(runtime, "categories", J::object(), "database_not_open");
      Error(runtime, "fx_records", J::object(), "fx_not_open");
      Error(runtime, "archive_entries", J::object(), "archive_not_open");
      Error(runtime, "strings_entries", J::object(), "strings_not_open");
    });
    test("argument type and bounds", [&] {
      Error(runtime, "records", J::object(), "missing_argument");
      Error(runtime, "categories", J::object({{"query", true}}),
            "invalid_argument_type");
      Error(runtime, "archive_entries", J::object({{"limit", 501}}),
            "argument_out_of_range");
      Error(runtime, "categories", J::object({{"unexpected", 1}}),
            "unknown_argument");
      Error(runtime, "python_execute", J::object(), "unknown_operation");
    });
    test("model triangle-band schemas are optional bounded native integers",
         [&] {
           const auto catalog = Monstey_Runtime::Catalog();
           for (const char *name : {"model_convert", "model_export_blend",
                                    "model_export_native"}) {
             const auto found = std::find_if(
                 catalog.as_array().begin(), catalog.as_array().end(),
                 [name](const auto &row) {
                   return row.at("name").as_string() == name;
                 });
             Check(found != catalog.as_array().end(), "Missing model route");
             const auto &schema = found->at("inputSchema");
             const auto &band = schema.at("properties").at("triangle_band");
             Check(band.at("type").as_string() == "integer" &&
                       band.at("minimum").as_int() == 0 &&
                       band.at("maximum").as_int() == 16 &&
                       !band.contains("default") &&
                       std::none_of(schema.at("required").as_array().begin(),
                                    schema.at("required").as_array().end(),
                                    [](const auto &field) {
                                      return field.as_string() ==
                                             "triangle_band";
                                    }),
                   "Model route changed optional band identity or bounds");
           }
         });
    const auto modelArguments = [](const char *operation,
                                   const std::filesystem::path &source,
                                   const std::filesystem::path &nativeTemplate,
                                   const std::filesystem::path &output) {
      if (std::string_view(operation) == "model_export_native")
        return J::object({{"blend", Monstey_Utf8(source.wstring())},
                          {"output_mesh", Monstey_Utf8(output.wstring())}});
      auto args = J::object({{"source", Monstey_Utf8(source.wstring())},
                             {"output", Monstey_Utf8(output.wstring())}});
      if (std::string_view(operation) == "model_convert") {
        args["template"] = Monstey_Utf8(nativeTemplate.wstring());
        args["binding"] = "native";
      }
      return args;
    };
    test("model triangle-band validation rejects before source access", [&] {
      const auto before = Call(runtime, "describe");
      const auto invalidValues = J::array({-1, 17, 0.5, "0", nullptr, true});
      for (const char *name :
           {"model_convert", "model_export_blend", "model_export_native"}) {
        const auto output = directory / (std::string(name) + "-invalid.mesh");
        const auto baseArgs =
            modelArguments(name, directory / "absent-source",
                           directory / "absent-template.mesh", output);
        for (const auto &value : invalidValues.as_array()) {
          auto args = baseArgs;
          args["triangle_band"] = value;
          Error(runtime, name, args,
                value.is_integer() ? "argument_out_of_range"
                                   : "invalid_argument_type");
          Check(!std::filesystem::exists(output),
                "Rejected band wrote a native output");
        }
      }
      const auto after = Call(runtime, "describe");
      Check(after.at("generation") == before.at("generation") &&
                after.at("revision") == before.at("revision"),
            "Rejected band changed the source session");
    });
    test("model triangle-band routes retain source-session guards", [&] {
      const auto before = Call(runtime, "describe");
      for (const char *name :
           {"model_convert", "model_export_blend", "model_export_native"}) {
        const auto output = directory / (std::string(name) + "-stale.mesh");
        auto args = modelArguments(name, directory / "absent-source",
                                   directory / "absent-template.mesh", output);
        args["triangle_band"] = 0;
        args["expected_generation"] = before.at("generation").as_uint() + 1;
        Error(runtime, name, args, "stale_session");
        args["expected_generation"] = before.at("generation");
        args["expected_revision"] = before.at("revision").as_uint() + 1;
        Error(runtime, name, args, "stale_revision");
        Check(!std::filesystem::exists(output),
              "Stale model route wrote a native output");
      }
      const auto after = Call(runtime, "describe");
      Check(after.at("generation") == before.at("generation") &&
                after.at("revision") == before.at("revision"),
            "Stale model route changed the source session");
    });
    test("bounded strict JSON requests", [&] {
      Check(
          !runtime
               .Reply("{\"operation\":\"describe\",\"operation\":\"catalog\"}")
               .at("ok")
               .as_bool(),
          "accepted duplicate");
      Check(runtime.Reply(std::string(65537, 'x'))
                    .at("error")
                    .at("code")
                    .as_string() == "request_too_large",
            "accepted oversized");
      Check(!runtime.Reply("[]").at("ok").as_bool(), "accepted array request");
    });
    test("string edits and stale generation", [&] {
      auto opened = Call(runtime, "strings_new");
      const auto generation = opened.at("generation");
      Call(runtime, "strings_set",
           J::object({{"key", "RuntimeName"},
                      {"text", "Native Unicode \xce\xa9"},
                      {"expected_generation", generation}}));
      const auto rows =
          Call(runtime, "strings_entries").at("result").at("rows");
      Check(rows.size() == 1 &&
                rows[0].at("text").as_string() == "Native Unicode \xce\xa9",
            "string edit missing");
      const auto revision = Call(runtime, "describe").at("revision");
      Call(runtime, "strings_set", J::object({{"key", 2}, {"text", "Second"}}));
      Error(runtime, "strings_remove",
            J::object({{"key", 2}, {"expected_revision", revision}}),
            "stale_revision");
      Call(runtime, "archive_project_new");
      Error(runtime, "strings_set",
            J::object({{"key", 3},
                       {"text", "stale"},
                       {"expected_generation", generation}}),
            "stale_session");
    });
    test("failed open keeps previous string project", [&] {
      const auto before = Call(runtime, "strings_entries").at("result");
      const auto failed = runtime.Call(J::object(
          {{"operation", "strings_new"},
           {"arguments",
            J::object(
                {{"source",
                  Monstey_Utf8((directory / "missing.strdb").wstring())}})}}));
      Check(!failed.at("ok").as_bool(), "missing file opened");
      Check(Call(runtime, "strings_entries").at("result") == before,
            "failed open replaced state");
    });
    test("string build project save and undo", [&] {
      Call(runtime, "strings_save_project",
           J::object({{"path",
                       Monstey_Utf8(
                           (directory / "strings.project.json").wstring())}}));
      Call(
          runtime, "strings_build",
          J::object({{"output",
                      Monstey_Utf8((directory / "strings.strdb").wstring())}}));
      Call(runtime, "strings_remove", J::object({{"key", 2}}));
      Call(runtime, "strings_undo");
      Check(
          Call(runtime, "strings_entries").at("result").at("total").as_uint() ==
              2,
          "undo failed");
      Call(runtime, "strings_load_project",
           J::object({{"path",
                       Monstey_Utf8(
                           (directory / "strings.project.json").wstring())}}));
      Check(
          Call(runtime, "strings_entries").at("result").at("total").as_uint() ==
              2,
          "project load failed");
    });
    test("effective archive extraction and build", [&] {
      const auto local = directory / "payload.bin";
      Monstey_WriteNewText(local, "native archive payload");
      Call(runtime, "archive_add",
           J::object({{"virtual", "folder/original.bin"},
                      {"source", Monstey_Utf8(local.wstring())}}));
      Call(runtime, "archive_rename",
           J::object({{"old", "folder/original.bin"},
                      {"new", "folder/renamed.bin"}}));
      const auto extract = directory / "extract.bin";
      Call(runtime, "extract",
           J::object({{"virtual", "folder/renamed.bin"},
                      {"destination", Monstey_Utf8(extract.wstring())}}));
      Check(Monstey_ReadText(extract) == "native archive payload",
            "effective extraction failed");
      Call(
          runtime, "archive_build",
          J::object({{"output",
                      Monstey_Utf8((directory / "native.arch06").wstring())}}));
      Call(runtime, "archive_save_project",
           J::object(
               {{"path",
                 Monstey_Utf8((directory / "archive.project").wstring())}}));
      Call(runtime, "archive_project_new");
      Call(runtime, "archive_load_project",
           J::object(
               {{"path",
                 Monstey_Utf8((directory / "archive.project").wstring())}}));
      Check(
          Call(runtime, "archive_entries").at("result").at("total").as_uint() ==
              1,
          "archive project lost");
    });
    test("serialized concurrent clients", [&] {
      bool good[4]{true, true, true, true};
      std::thread threads[4];
      for (unsigned i = 0; i < 4; ++i)
        threads[i] = std::thread([&, i] {
          for (unsigned k = 0; k < 5; ++k) {
            const auto reply = runtime.Call(J::object(
                {{"operation", "strings_set"},
                 {"arguments",
                  J::object({{"key", 100 + 10 * i + k},
                             {"text", "Concurrent native client"}})}}));
            if (!reply.at("ok").as_bool())
              good[i] = false;
          }
        });
      for (auto &thread : threads)
        thread.join();
      for (bool value : good)
        Check(value, "concurrent request failed");
      Check(
          Call(runtime, "strings_entries").at("result").at("total").as_uint() ==
              22,
          "lost concurrent update");
    });
    test("native settings and mounted asset catalog integration", [&] {
      const auto game = directory / "fake-game";
      std::filesystem::create_directories(game / "x64");
      std::filesystem::copy_file(directory / "native.arch06",
                                 game / "sample.arch06");
      Monstey_WriteNewText(game / "x64" / "default.archcfg",
                           "..\\sample.arch06\n");
      Call(runtime, "configure",
           J::object(
               {{"game_root", Monstey_Utf8(game.wstring())}, {"save", false}}));
      Check(!std::filesystem::exists(directory / "settings.json"),
            "Runtime-only settings wrote a file");
      const auto extracted = directory / "world-extract";
      const auto result =
          Call(runtime, "sow_world_asset_extract",
               J::object({{"output", Monstey_Utf8(extracted.wstring())},
                          {"requests",
                           J::array({J::object(
                               {{"resource_path", "folder/renamed.bin"}})})}}))
              .at("result");
      Check(result.at("resources").size() == 1 &&
                Monstey_ReadText(extracted / "assets/folder/renamed.bin") ==
                    "native archive payload",
            "direct World route lost configured game root or array arguments");
      const auto mounts = Call(runtime, "archive_mounts").at("result");
      Check(mounts.at("mounts").size() == 1,
            "Configured archive mount order missing");
      Call(runtime, "configure", J::object({{"save", true}}));
      Monstey_Runtime restored({}, directory / "settings.json");
      const auto config =
          Call(restored, "components").at("result").at("configuration");
      Check(config.at("settings_saved").as_bool() &&
                !config.at("python_runtime").as_bool(),
            "Native settings restore failed");
      const auto catalog = directory / "assets.jsonl";
      const auto built =
          Call(runtime, "archive_catalog",
               J::object({{"output", Monstey_Utf8(catalog.wstring())}}))
              .at("result");
      Check(built.at("rows").as_uint() > 0 &&
                !built.at("payloads_copied").as_bool(),
            "Catalog copied asset payloads");
      const auto read =
          Call(runtime, "archive_catalog_read",
               J::object({{"path", Monstey_Utf8(catalog.wstring())},
                          {"query", "renamed"},
                          {"winning_only", true}}))
              .at("result");
      Check(read.at("rows").size() == 1,
            "Catalog query or winning variant missing");
    });
    test("settings cannot replace an opened native source", [&] {
      const auto source = directory / "native.arch06";
      const auto before = Monstey_HashSha256(source);
      Monstey_Runtime protectedSession({}, source);
      Call(protectedSession, "open_archive",
           J::object({{"path", Monstey_Utf8(source.wstring())}}));
      const auto response = protectedSession.Call(
          J::object({{"operation", "configure"},
                     {"arguments", J::object({{"save", true}})}}));
      Check(!response.at("ok").as_bool(),
            "Settings overwrote an opened resource");
      Check(Monstey_HashSha256(source) == before,
            "Opened native source changed");
    });
    test(
        "independent database/category/record/attribute workflow through "
        "shared API",
        [&] {
          std::vector<std::string> names{"RuntimeOwnedAlpha",
                                         "RuntimeOwnedBeta"};
          std::sort(names.begin(), names.end(),
                    [](const auto &left, const auto &right) {
                      return Monstey_NativeHash(left) >
                             Monstey_NativeHash(right);
                    });
          auto fields = J::array(
              {J::object({{"name", "Power"}, {"type_code", 2}, {"array", 1}}),
               J::object({{"name", "Flag"}, {"type_code", 1}, {"array", 1}}),
               J::object(
                   {{"name", "Greeting"}, {"type_code", 6}, {"array", 1}}),
               J::object({{"name", "Link"},
                          {"type_code", 10},
                          {"array", 1},
                          {"target", names[1]}}),
               J::object(
                   {{"name", "Scratch"}, {"type_code", 3}, {"array", 1}})});
          const auto schema = directory / "independent-schema.json";
          Monstey_WriteNewText(
              schema,
              J::object(
                  {{"categories",
                    J::object(
                        {{names[0], J::object({{"fields", fields}})},
                         {names[1], J::object({{"fields", J::array()}})}})}})
                  .dump());
          const auto opened = Call(
              runtime, "database_new",
              J::object({{"schema_path", Monstey_Utf8(schema.wstring())}}));
          Check(opened.at("result").at("source").as_string().empty() &&
                    opened.at("result").at("category_count").as_uint() == 0,
                "New database still requires a file or a template record");
          const auto ownerCategory =
              Call(runtime, "database_create_category",
                   J::object(
                       {{"schema_name", names[0]}, {"path", "Editor/Logical"}}))
                  .at("result")
                  .at("category")
                  .at("index");
          const auto targetCategory =
              Call(runtime, "database_create_category",
                   J::object({{"schema_name", names[1]}}))
                  .at("result")
                  .at("category")
                  .at("index");
          const auto owner = Call(runtime, "database_create_record",
                                  J::object({{"category_index", ownerCategory},
                                             {"name", "Keeper"}}))
                                 .at("result")
                                 .at("record")
                                 .at("selector")
                                 .as_string();
          const auto target =
              Call(runtime, "database_create_record",
                   J::object({{"category_index", targetCategory},
                              {"anonymous", true}}))
                  .at("result")
                  .at("record")
                  .at("selector")
                  .as_string();
          const auto add = [&](const char *field, unsigned type,
                               const J &value) {
            Call(runtime, "database_add_field",
                 J::object({{"record", owner},
                            {"field", field},
                            {"native_type", type},
                            {"value", value}}));
          };
          add("Power", 2, 123.5);
          add("Flag", 1, true);
          add("Greeting", 6, "Pale \xce\xa9");
          add("Link", 10, target);
          const auto beforeIncoming = Call(runtime, "describe");
          const auto incoming =
              Call(runtime, "database_incoming",
                   J::object({{"record", target}, {"limit", 10}}));
          Check(incoming.at("result").at("total").as_uint() == 1 &&
                    incoming.at("result")
                            .at("rows")[0]
                            .at("record")
                            .as_string() == owner &&
                    incoming.at("revision") == beforeIncoming.at("revision"),
                "Incoming-reference route lost a typed owner or mutated the "
                "session");
          add("Scratch", 3, 7);
          Call(runtime, "database_move_field",
               J::object(
                   {{"record", owner}, {"field", "Flag"}, {"ordinal", 0}}));
          auto metadata =
              Call(runtime, "database_metadata", J::object({{"record", owner}}))
                  .at("result");
          Check(metadata.at("fields")[0].at("name").as_string() == "Flag",
                "Descriptor move was only a UI ordering change");
          Call(runtime, "database_delete_field",
               J::object({{"record", owner}, {"field", "Scratch"}}));
          Call(runtime, "database_undo");
          Check(
              Call(runtime, "database_metadata", J::object({{"record", owner}}))
                      .at("result")
                      .at("fields")
                      .size() == 5,
              "Structural undo lost a descriptor");
          Call(runtime, "database_redo");
          const auto project = directory / "independent.project.json";
          Call(runtime, "database_save_project",
               J::object({{"path", Monstey_Utf8(project.wstring())}}));
          const auto candidate = directory / "independent.gamedb";
          Call(runtime, "database_build",
               J::object({{"output", Monstey_Utf8(candidate.wstring())}}));
          Monstey_Gadb built(candidate, schema);
          const auto builtRecord = built.Metadata(names[0] + ":Keeper");
          const auto &rows = builtRecord.at("fields");
          bool power = false, flag = false, text = false, link = false;
          for (const auto &field : rows.as_array()) {
            const auto name = field.at("name").as_string();
            power = power ||
                    (name == "Power" && field.at("value").as_number() == 123.5);
            flag = flag || (name == "Flag" && field.at("value").as_bool());
            text = text || (name == "Greeting" &&
                            field.at("value").as_string() == "Pale \xce\xa9");
            link = link || (name == "Link" &&
                            field.at("value").as_string() == Monstey_Hex(0));
          }
          Check(rows.size() == 4 && power && flag && text && link,
                "Native category sorting or typed field/ref readback failed");
          Call(runtime, "database_new",
               J::object({{"schema_path", Monstey_Utf8(schema.wstring())}}));
          Call(runtime, "database_load_project",
               J::object({{"path", Monstey_Utf8(project.wstring())}}));
          const auto replay = directory / "independent-replayed.gamedb";
          Call(runtime, "database_build",
               J::object({{"output", Monstey_Utf8(replay.wstring())}}));
          Check(
              Monstey_ReadFile(candidate) == Monstey_ReadFile(replay),
              "Shared API project persistence lost authored layout/value data");
          Call(runtime, "database_project_fromopened");
          Check(Call(runtime, "describe")
                        .at("result")
                        .at("database")
                        .at("category_count")
                        .as_uint() == 0,
                "Restart from logical source did not reset its structural "
                "project");
          Call(runtime, "database_project_fromopened",
               J::object({{"schema_path", Monstey_Utf8(schema.wstring())}}));
        });
    test("structural API schemas keep category paths virtual and scalar types "
         "exact",
         [&] {
           const auto catalog = Monstey_Runtime::Catalog();
           std::set<std::string> expected{
               "database_new",           "database_create_category",
               "database_create_record", "database_add_field",
               "database_delete_field",  "database_move_field"};
           for (const auto &operation : catalog.as_array()) {
             const auto name = operation.at("name").as_string();
             expected.erase(name);
             const auto &properties =
                 operation.at("inputSchema").at("properties");
             if (name == "database_create_category")
               Check(!properties.at("path").contains("format"),
                     "Logical category path opens a filesystem picker");
             if (name == "database_create_record")
               Check(
                   properties.at("anonymous").at("type").as_string() ==
                       "boolean",
                   "Anonymous creation expects a string instead of a boolean");
           }
           Check(expected.empty(),
                 "Original GDBEdit structural APIs are not exposed");
           Error(runtime, "database_add_field",
                 J::object({{"record", "missing"},
                            {"field", "Power"},
                            {"native_type", 256},
                            {"value", 1}}),
                 "argument_out_of_range");
         });
    test("independent effect lifecycle and linked timeline through shared "
         "native API",
         [&] {
           const auto schema = directory / "effect-schema.json";
           const auto field = [](const char *name, unsigned type,
                                 const J &value, int array = 1) {
             return J::object({{"name", name},
                               {"type_code", type},
                               {"array", array},
                               {"default", value}});
           };
           Monstey_WriteNewText(
               schema,
               J::object(
                   {{"categories",
                     J::object(
                         {{"FxGlobalProps",
                           J::object({{"fields",
                                       J::array({field("FxName", 4, nullptr),
                                                 field("ResourceLenMS", 3, 0),
                                                 field("KeyList", 10, nullptr,
                                                       -1)})}})},
                          {"ClientFX.RuntimeSynthetic",
                           J::object({{"fields",
                                       J::array({field("KeyName", 4, nullptr),
                                                 field("StartTime", 2, 0.0),
                                                 field("EndTime", 2, 1.0),
                                                 field("Enabled", 1, true),
                                                 field("Asset", 4, ""),
                                                 field("Target", 10,
                                                       nullptr)})}})}})}})
                   .dump());
           const auto opened =
               Call(runtime, "fx_new",
                    J::object({{"schema_path", Monstey_Utf8(schema.wstring())},
                               {"name", "fx/runtime/independent"},
                               {"duration_ms", 2500}}));
           Check(opened.at("result").at("effect_exists").as_bool() &&
                     opened.at("result").at("source").as_string().empty(),
                 "New effect still requires a donor effect or a temporary "
                 "native file");
           const auto resourceSchema = Call(runtime, "fx_schema").at("result");
           Check(resourceSchema.at("classes").size() == 1,
                 "Qualified effect class catalog missing");
           const auto track =
               Call(runtime, "fx_create_track",
                    J::object({{"name", "Native timeline \xce\xa9"}}))
                   .at("result")
                   .at("result")
                   .at("id")
                   .as_string();
           const auto element =
               Call(runtime, "fx_create_element",
                    J::object({{"class_name", "ClientFX.RuntimeSynthetic"},
                               {"name", "First key"},
                               {"track", track},
                               {"start", .25},
                               {"end", 1.5}}))
                   .at("result")
                   .at("result")
                   .at("selector")
                   .as_string();
           Call(runtime, "fx_link_key",
                J::object({{"record", element}, {"track", track}}));
           auto view = Call(runtime, "fx_workflow").at("result");
           Check(view.at("active_keys").size() == 2 &&
                     view.at("layout").at("tracks").size() == 1 &&
                     view.at("clone_count").as_uint() == 0,
                 "Linked effect slots silently cloned or lost their physical "
                 "record");
           Call(runtime, "fx_retime_key",
                J::object({{"record", element}, {"start", .5}, {"end", 2.0}}));
           Call(runtime, "fx_undo");
           const auto retimed =
               Call(runtime, "fx_record", J::object({{"record", element}}))
                   .at("result");
           bool startRestored = false;
           for (const auto &row : retimed.at("fields").as_array())
             if (row.at("name").as_string() == "StartTime")
               startRestored = row.at("value").as_number() == .25;
           Check(startRestored, "Effect retiming was not one undo transaction");
           Call(runtime, "fx_redo");
           Call(runtime, "fx_unlink_key",
                J::object({{"track", track}, {"index", 0}}));
           const auto candidate = directory / "independent.fxp";
           const auto project = directory / "independent-fx.project.json";
           Call(runtime, "fx_save_project",
                J::object({{"path", Monstey_Utf8(project.wstring())}}));
           Call(runtime, "fx_build",
                J::object({{"output", Monstey_Utf8(candidate.wstring())}}));
           const auto native = Monstey_ReadFile(candidate);
           Check(native.size() > 8 && native[0] == 'L' && native[3] == 'X' &&
                     native[4] == 8,
                 "Independent effect did not build a native LTFX8 envelope");
           Call(runtime, "fx_new");
           Check(!Call(runtime, "fx_workflow")
                      .at("result")
                      .at("effect_exists")
                      .as_bool(),
                 "Logical empty FX retained a previous effect");
           Call(runtime, "fx_load_project",
                J::object({{"path", Monstey_Utf8(project.wstring())}}));
           const auto replay = directory / "independent-fx-replayed.fxp";
           Call(runtime, "fx_build",
                J::object({{"output", Monstey_Utf8(replay.wstring())}}));
           view = Call(runtime, "fx_workflow").at("result");
           Check(Monstey_ReadFile(replay) == native &&
                     view.at("active_keys").size() == 1 &&
                     view.at("layout").at("tracks")[0].at("name").as_string() ==
                         "Native timeline \xce\xa9",
                 "FX project replay lost native records, null reference or "
                 "editor timeline");
           Call(runtime, "fx_delete_element", J::object({{"record", element}}));
           Check(Call(runtime, "fx_workflow")
                     .at("result")
                     .at("active_keys")
                     .empty(),
                 "Physical deletion retained a dangling track link");
           Call(runtime, "fx_undo");
           Check(Call(runtime, "fx_workflow")
                         .at("result")
                         .at("active_keys")
                         .size() == 1,
                 "Physical key deletion was not restored with one undo");
           const auto before = Call(runtime, "describe");
           Error(runtime, "fx_new", J::object({{"duration_ms", 50}}),
                 "invalid_arguments");
           const auto after = Call(runtime, "describe");
           Check(
               after.at("generation") == before.at("generation") &&
                   after.at("result").at("effects") ==
                       before.at("result").at("effects"),
               "Failed independent effect replaced the active source session");
         });
    test("effect schemas expose numeric timing and logical record selectors",
         [&] {
           const auto catalog = Monstey_Runtime::Catalog();
           for (const auto &op : catalog.as_array()) {
             const auto name = op.at("name").as_string();
             const auto &properties = op.at("inputSchema").at("properties");
             if (name == "fx_duplicate_element")
               Check(!properties.at("source").contains("format"),
                     "Element duplication treats a native record selector as a "
                     "file");
             if (name == "fx_retime_key")
               Check(
                   properties.at("start").at("type").as_string() == "number" &&
                       properties.at("end").at("type").as_string() == "number",
                   "FX timeline expects strings instead of numeric seconds");
             if (name == "fx_create_element")
               Check(properties.at("values").at("type").as_string() == "object",
                     "FX typed initial properties have no object schema");
           }
           Error(runtime, "fx_set_duration",
                 J::object({{"duration_ms", 2147483648ull}}),
                 "argument_out_of_range");
           Error(runtime, "fx_create_track",
                 J::object({{"name", "Bad"}, {"index", -1}}),
                 "argument_out_of_range");
           Error(runtime, "fx_new",
                 J::object({{"expected_sha256", std::string(64, '0')}}),
                 "invalid_arguments");
         });
    test("native reflection has an independent read-only API and paged "
         "skeleton schemas",
         [&] {
           const auto catalog = Monstey_Runtime::Catalog();
           bool reflection = false, skeleton = false;
           for (const auto &op : catalog.as_array()) {
             const auto name = op.at("name").as_string();
             const auto &properties = op.at("inputSchema").at("properties");
             if (name == "fx_reflect_schema") {
               reflection =
                   op.at("read_only").as_bool() &&
                   properties.at("category_entries").at("type").as_string() ==
                       "array";
               Check(properties.at("source").at("format").as_string() == "path",
                     "Reflection source is not a local file picker");
             }
             if (name == "model_inspect")
               skeleton = properties.at("inspection")
                              .at("properties")
                              .at("row_limit")
                              .at("maximum")
                              .as_uint() == 10000;
           }
           Check(reflection && skeleton,
                 "Native class reflection or skeleton paging schema missing");
           const auto source = directory / "reflection-invalid-image.bin";
           Monstey_WriteNewFile(source, Monstey_Bytes{1, 2, 3, 4});
           const auto args =
               J::object({{"source", Monstey_Utf8(source.wstring())},
                          {"expected_sha256", Monstey_HashSha256(source)},
                          {"category_entries", J::array()}});
           Error(runtime, "fx_reflect_schema", args, "invalid_arguments");
           auto invalid = args;
           invalid["category_entries"] = J::array({true});
           Error(runtime, "fx_reflect_schema", invalid,
                 "invalid_argument_type");
           invalid["category_entries"] = J::array({"0xnot-an-address"});
           Error(runtime, "fx_reflect_schema", invalid, "invalid_arguments");
           invalid["category_entries"] = J::array({"0x140001000"});
           const auto failed = runtime.Call(J::object(
               {{"operation", "fx_reflect_schema"}, {"arguments", invalid}}));
           Check(!failed.at("ok").as_bool() &&
                     Monstey_ReadFile(source) == Monstey_Bytes({1, 2, 3, 4}),
                 "Malformed native reflection image was accepted or mutated");
         });
    test(
        "model socket operations share the native database session and guards",
        [&] {
          const auto schema = directory / "socket-schema.json";
          const auto scalar = [](const char *name, unsigned type) {
            return J::object(
                {{"name", name}, {"type_code", type}, {"array", 1}});
          };
          Monstey_WriteNewText(
              schema,
              J::object(
                  {{"categories",
                    J::object(
                        {{"Model",
                          J::object(
                              {{"fields",
                                J::array(
                                    {scalar("ModelFile", 4),
                                     J::object(
                                         {{"name", "SimpleModelSocketOffsets"},
                                          {"type_code", 11},
                                          {"array", -1},
                                          {"target",
                                           "ModelSocketOverride"}})})}})},
                         {"ModelSocketOverride",
                          J::object(
                              {{"fields",
                                J::array(
                                    {scalar("SocketName", 4),
                                     scalar("ParentNode", 4),
                                     scalar("PosOffsetFromParent", 8),
                                     scalar("RotOffsetFromParent", 9)})}})}})}})
                  .dump());
          Call(runtime, "database_new",
               J::object({{"schema_path", Monstey_Utf8(schema.wstring())}}));
          const auto modelCategory = Call(runtime, "database_create_category",
                                          J::object({{"schema_name", "Model"}}))
                                         .at("result")
                                         .at("category")
                                         .at("index");
          Call(runtime, "database_create_category",
               J::object({{"schema_name", "_StructuresModelSocketOverride"},
                          {"name", "ModelSocketOverride"}}));
          const auto createModel = [&](const char *name) {
            const auto selector =
                Call(runtime, "database_create_record",
                     J::object(
                         {{"category_index", modelCategory}, {"name", name}}))
                    .at("result")
                    .at("record")
                    .at("selector")
                    .as_string();
            Call(runtime, "database_add_field",
                 J::object({{"record", selector},
                            {"field", "ModelFile"},
                            {"native_type", 4},
                            {"value", "models/example.skel"}}));
            return selector;
          };
          const auto first = createModel("First"),
                     second = createModel("Second");
          Call(runtime, "model_socket_create",
               J::object({{"model", first},
                          {"socket_name", "L_sheath"},
                          {"parent_node", "L_Sheath_1"},
                          {"position_cm", J::array({1.0, 2.0, 3.0})},
                          {"rotation_xyzw", J::array({0.0, 0.0, 0.0, 1.0})}}));
          const auto before = Call(runtime, "describe");
          const auto listed =
              Call(runtime, "model_sockets_list",
                   J::object({{"model", first},
                              {"expected_generation", before.at("generation")},
                              {"expected_revision", before.at("revision")}}));
          Check(listed.at("result").at("total").as_uint() == 1 &&
                    listed.at("revision") == before.at("revision"),
                "Socket list rejected valid session guards or changed its "
                "source");
          Call(runtime, "model_socket_copy_to_model",
               J::object({{"source_model", first},
                          {"source_index", 0},
                          {"target_model", second},
                          {"policy", "shared"}}));
          Call(runtime, "model_socket_edit",
               J::object({{"model", second},
                          {"index", 0},
                          {"policy", "independent"},
                          {"socket_name", "Independent"}}));
          const auto source =
              Call(runtime, "model_sockets_list", J::object({{"model", first}}))
                  .at("result");
          const auto changed = Call(runtime, "model_sockets_list",
                                    J::object({{"model", second}}))
                                   .at("result");
          Check(source.at("rows")[0].at("reference") !=
                    changed.at("rows")[0].at("reference"),
                "Independent socket edit retained a shared native override");
          Error(runtime, "model_sockets_list",
                J::object({{"model", first}, {"expected_generation", 999999}}),
                "stale_session");
          const auto project = directory / "socket.project.json";
          Call(runtime, "database_save_project",
               J::object({{"path", Monstey_Utf8(project.wstring())}}));
          const auto candidate = directory / "socket.gamedb";
          Call(runtime, "database_build",
               J::object({{"output", Monstey_Utf8(candidate.wstring())}}));
          Call(runtime, "database_load_project",
               J::object({{"path", Monstey_Utf8(project.wstring())}}));
          const auto replay = directory / "socket-replayed.gamedb";
          Call(runtime, "database_build",
               J::object({{"output", Monstey_Utf8(replay.wstring())}}));
          Check(Monstey_ReadFile(candidate) == Monstey_ReadFile(replay),
                "Shared socket workflow did not persist through native project "
                "replay");
        });
    test(
        "component prefabs stay coherent with generic effect edits deletion "
        "and history",
        [&] {
          const auto project = directory / "independent-fx.project.json";
          Call(runtime, "fx_load_project",
               J::object({{"path", Monstey_Utf8(project.wstring())}}));
          const auto active = Call(runtime, "fx_workflow").at("result");
          const auto key =
              active.at("active_keys")[0].at("selector").as_string();
          const auto prefab =
              Call(runtime, "fx_prefab_from_key",
                   J::object({{"record", key}, {"name", "Library template"}}))
                  .at("result")
                  .at("result");
          const auto id = prefab.at("prefab").as_string(),
                     record = prefab.at("record").as_string();
          const auto child = Call(runtime, "fx_prefab_add_child",
                                  J::object({{"prefab", id},
                                             {"time", 3.0},
                                             {"name", "Derived component"}}))
                                 .at("result")
                                 .at("result")
                                 .at("record")
                                 .as_string();
          Call(runtime, "fx_set",
               J::object({{"record", record},
                          {"field", "Asset"},
                          {"value", "materials/native-red.mat"}}));
          const auto metadata =
              Call(runtime, "fx_record", J::object({{"record", child}}))
                  .at("result");
          bool inherited = false, ownTime = false;
          for (const auto &field : metadata.at("fields").as_array()) {
            if (field.at("name").as_string() == "Asset")
              inherited =
                  field.at("value").as_string() == "materials/native-red.mat";
            if (field.at("name").as_string() == "StartTime")
              ownTime = field.at("value").as_number() == 2.0;
          }
          Check(inherited && ownTime, "Generic typed template edit did not "
                                      "propagate or overwrote instance timing");
          Call(runtime, "fx_undo");
          Call(runtime, "fx_redo");
          Call(runtime, "fx_delete_element", J::object({{"record", record}}));
          auto library = Call(runtime, "fx_library").at("result").at("library");
          Check(
              library.at("prefabs").empty() && library.at("instances").empty(),
              "Generic template deletion retained stale prefab relationships");
          Call(runtime, "fx_prefab_sync");
          Call(runtime, "fx_build",
               J::object(
                   {{"output",
                     Monstey_Utf8(
                         (directory / "prefab-materialized.fxp").wstring())}}));
          Call(runtime, "fx_delete_effect");
          library = Call(runtime, "fx_library").at("result").at("library");
          Check(library.at("instances").empty(),
                "Effect deletion left dangling library instances");
          Call(runtime, "fx_undo");
          Check(Call(runtime, "fx_workflow")
                    .at("result")
                    .at("effect_exists")
                    .as_bool(),
                "Effect/library deletion was not one shared undo transaction");
        });
    test(
        "native nested effect groups preserve session guards and prefab "
        "integration",
        [&] {
          const auto schema = directory / "nested-schema.json";
          const auto scalar = [](const std::string &name, unsigned type,
                                 const J &value) {
            return J::object({{"name", name},
                              {"type_code", type},
                              {"array", 1},
                              {"default", value}});
          };
          auto fields = J::array(
              {scalar("KeyName", 4, ""), scalar("StartTime", 2, 0.0),
               scalar("EndTime", 2, 1.0), scalar("NumToCreate", 3, 1),
               scalar("Loop", 1, false), scalar("CreateContext", 4, "")});
          for (unsigned i = 0; i < 16; ++i)
            fields.as_array().push_back(
                scalar("Effect" + std::to_string(i), 4, ""));
          auto links = scalar("KeyList", 10, nullptr);
          links["array"] = -1;
          Monstey_WriteNewText(
              schema,
              J::object(
                  {{"categories",
                    J::object(
                        {{"FxGlobalProps",
                          J::object({{"fields",
                                      J::array({scalar("FxName", 4, ""),
                                                scalar("ResourceLenMS", 3, 0),
                                                links})}})},
                         {"ClientFX.Create",
                          J::object({{"fields", fields}})}})}})
                  .dump());
          Call(runtime, "fx_new",
               J::object({{"schema_path", Monstey_Utf8(schema.wstring())},
                          {"name", "fx/runtime/nested"},
                          {"duration_ms", 3000}}));
          const auto key = Call(runtime, "fx_create_element",
                                J::object({{"class_name", "ClientFX.Create"},
                                           {"name", "Nested spawner"}}))
                               .at("result")
                               .at("result")
                               .at("reference")
                               .as_string();
          Call(runtime, "fx_nested_set_slot",
               J::object(
                   {{"record", key},
                    {"slot", 15},
                    {"effects", J::array({"fx/first.fxp", "fx/second.fxp"})}}));
          const auto state = Call(runtime, "describe");
          const auto view =
              Call(runtime, "fx_nested_inspect",
                   J::object({{"record", key},
                              {"expected_generation", state.at("generation")},
                              {"expected_revision", state.at("revision")}}));
          Check(
              view.at("revision") == state.at("revision") &&
                  view.at("result").at("effect_references").as_uint() == 2,
              "Nested reader changed revision or flattened grouped resources");
          Error(runtime, "fx_nested_inspect",
                J::object({{"record", key}, {"expected_generation", 999999}}),
                "stale_session");
          Error(runtime, "fx_nested_set_options",
                J::object({{"record", key}, {"num_to_create", 256}}),
                "argument_out_of_range");
          Call(runtime, "fx_nested_set_options",
               J::object(
                   {{"record", key}, {"num_to_create", 200}, {"loop", true}}));
          Check(Call(runtime, "fx_nested_inspect", J::object({{"record", key}}))
                        .at("result")
                        .at("native_num_to_create")
                        .as_uint() == 200,
                "Nested native byte count was lost");
          Call(runtime, "fx_undo");
          const auto previous =
              Call(runtime, "fx_nested_inspect", J::object({{"record", key}}))
                  .at("result");
          Check(previous.at("native_num_to_create").as_uint() == 1 &&
                    !previous.at("loop").as_bool(),
                "Nested options did not use one history step");
        });
    test(
        "animated extras share native database history references and session "
        "guards",
        [&] {
          const auto schema = directory / "animated-extra-schema.json";
          const auto field = [](const char *name, unsigned type, const J &value,
                                const char *target = "", int array = 1) {
            auto result = J::object({{"name", name},
                                     {"type_code", type},
                                     {"array", array},
                                     {"default", value}});
            if (*target)
              result["target"] = target;
            return result;
          };
          Monstey_WriteNewText(
              schema,
              J::object(
                  {{"categories",
                    J::object(
                        {{"Character.Models",
                          J::object(
                              {{"fields", J::array({field(
                                              "AttachedAnimExtras", 10, nullptr,
                                              "AnimatedExtra.Object", -1)})}})},
                         {"Model",
                          J::object(
                              {{"fields",
                                J::array({field("ModelFile", 4,
                                                "characters/test.skel")})}})},
                         {"Client.ClientFXSequence",
                          J::object({{"fields", J::array()}})},
                         {"BaseTypes.FloatRange",
                          J::object({{"fields", J::array()}})},
                         {"AnimatedExtra.Object",
                          J::object(
                              {{"fields",
                                J::array(
                                    {field("Model", 10, nullptr, "Model"),
                                     field("BaseAnimation", 4, nullptr),
                                     field("OwnerAttachNode", 4, nullptr),
                                     field("StartAttached", 1, true),
                                     field("StartVisible", 1, false),
                                     field("PersistentFX", 10, nullptr,
                                           "Client.ClientFXSequence"),
                                     field("DissolvePitchRange", 10, nullptr,
                                           "BaseTypes.FloatRange"),
                                     field("DissolveParamRange", 7,
                                           J::array({0, 1}))})}})}})}})
                  .dump());
          Call(runtime, "database_new",
               J::object({{"schema_path", Monstey_Utf8(schema.wstring())}}));
          const auto ownerCategory =
              Call(runtime, "database_create_category",
                   J::object({{"schema_name", "Character.Models"}}))
                  .at("result")
                  .at("category")
                  .at("index");
          const auto modelCategory = Call(runtime, "database_create_category",
                                          J::object({{"schema_name", "Model"}}))
                                         .at("result")
                                         .at("category")
                                         .at("index");
          const auto model = Call(runtime, "database_create_record",
                                  J::object({{"category_index", modelCategory},
                                             {"name", "Body"}}))
                                 .at("result")
                                 .at("record")
                                 .at("reference");
          Call(runtime, "database_add_field",
               J::object({{"record", model},
                          {"field", "ModelFile"},
                          {"native_type", 4},
                          {"value", "characters/test.skel"}}));
          const auto createOwner = [&](const char *name) {
            return Call(runtime, "database_create_record",
                        J::object({{"category_index", ownerCategory},
                                   {"name", name}}))
                .at("result")
                .at("record")
                .at("reference");
          };
          const auto first = createOwner("First"),
                     second = createOwner("Second");
          Call(runtime, "animated_extra_create",
               J::object({{"owner", first},
                          {"model", model},
                          {"base_animation", ""},
                          {"owner_attach_node", "Arm"},
                          {"start_visible", false}}));
          const auto state = Call(runtime, "describe");
          const auto list =
              Call(runtime, "animated_extras_list",
                   J::object({{"owner", first},
                              {"include_usage", false},
                              {"expected_generation", state.at("generation")},
                              {"expected_revision", state.at("revision")}}));
          Check(list.at("revision") == state.at("revision") &&
                    list.at("result").at("total").as_uint() == 1,
                "Animated extra reader changed revision or rejected valid "
                "guards");
          const auto original = list.at("result").at("rows")[0].at("reference");
          Call(runtime, "animated_extra_copy_to_owner",
               J::object({{"source_owner", first},
                          {"source_index", 0},
                          {"target_owner", second},
                          {"policy", "shared"},
                          {"expected_reference", original}}));
          Call(runtime, "animated_extra_edit",
               J::object({{"owner", second},
                          {"index", 0},
                          {"policy", "independent"},
                          {"owner_attach_node", "Other"},
                          {"expected_reference", original}}));
          const auto copied =
              Call(runtime, "animated_extras_list",
                   J::object({{"owner", second}, {"include_usage", false}}))
                  .at("result")
                  .at("rows")[0]
                  .at("reference");
          Check(copied != original,
                "Animated extra independent edit retained shared source");
          Error(runtime, "animated_extras_list",
                J::object({{"owner", first}, {"expected_generation", 999999}}),
                "stale_session");
          Call(runtime, "animated_extra_remove",
               J::object({{"owner", second},
                          {"index", 0},
                          {"delete_orphan", true},
                          {"expected_reference", copied}}));
          Check(Call(runtime, "animated_extras_list",
                     J::object({{"owner", second}, {"include_usage", false}}))
                        .at("result")
                        .at("total")
                        .as_uint() == 0,
                "Animated extra unlink did not update native owner");
          Call(runtime, "database_undo");
          Check(Call(runtime, "animated_extras_list",
                     J::object({{"owner", second}, {"include_usage", false}}))
                        .at("result")
                        .at("rows")[0]
                        .at("reference") == copied,
                "Animated extra removal did not undo record and owner "
                "atomically");
        });

    test("animation catalog exposes all eighteen guarded native routes", [&] {
      const auto catalog = Monstey_Runtime::Catalog();
      const auto animations = Monstey_AnimationCatalog();
      for (const auto &expected : animations.as_array()) {
        const auto found =
            std::find_if(catalog.as_array().begin(), catalog.as_array().end(),
                         [&](const auto &entry) {
                           return entry.at("name") == expected.at("name");
                         });
        Check(found != catalog.as_array().end() &&
                  found->at("read_only") == expected.at("read_only") &&
                  found->at("inputSchema")
                      .at("properties")
                      .contains("expected_generation") &&
                  found->at("inputSchema")
                      .at("properties")
                      .contains("expected_revision"),
              "Animation route missing native schema/session guards");
      }
      Error(runtime, "animation_describe", J::object(), "animation_not_open");
      Error(runtime, "animation_sample",
            J::object({{"clip", "walk"}, {"ticks", J::array()}}),
            "animation_not_open");
      Error(runtime, "animation_skeleton_register",
            J::object({{"source", "synthetic.skel"},
                       {"resource", "characters/a.anix"},
                       {"output", "synthetic-new.skel"}}),
            "missing_argument");
    });
    const auto animationSource = directory / "native-animation.anix";
    Monstey_WriteNewFile(
        animationSource,
        Monstey_Anix::Create(Monstey_AnimationScene()).Build());
    test("animation reads and failed reopen retain the source session", [&] {
      const auto before = Call(runtime, "describe");
      const auto opened =
          Call(runtime, "animation_open",
               J::object(
                   {{"path", Monstey_Utf8(animationSource.wstring())},
                    {"expected_sha256", Monstey_HashSha256(animationSource)}}));
      Check(opened.at("generation").as_uint() ==
                before.at("generation").as_uint() + 1,
            "Animation open did not advance source generation");
      const auto state = Call(runtime, "animation_describe");
      const auto rows = Call(runtime, "animation_clips",
                             J::object({{"query", "WALK"}, {"limit", 1}}));
      const auto pose = Call(
          runtime, "animation_sample",
          J::object({{"clip", "walk"}, {"ticks", J::array({0, 500, 1000})}}));
      Check(rows.at("result").at("total").as_uint() == 1 &&
                pose.at("result").at("tracks").size() == 1 &&
                rows.at("revision") == state.at("revision") &&
                pose.at("revision") == state.at("revision"),
            "Animation reads changed revision or sampling data");
      Error(runtime, "animation_open",
            J::object({{"path", Monstey_Utf8(animationSource.wstring())},
                       {"expected_sha256", std::string(64, '0')}}),
            "source_hash_mismatch");
      const auto after = Call(runtime, "animation_describe");
      Check(after.at("generation") == state.at("generation") &&
                after.at("revision") == state.at("revision") &&
                after.at("result") == state.at("result"),
            "Failed animation open replaced the live session");
    });
    test(
        "animation batch file edits share atomic history and stale guards",
        [&] {
          const auto ops = J::array(
              {J::object(
                   {{"op", "duplicate"}, {"clip", "walk"}, {"name", "copy"}}),
               J::object(
                   {{"op", "rename"}, {"clip", "copy"}, {"name", "renamed"}})});
          const auto path = directory / "animation-ops.json";
          Monstey_WriteNewText(path, std::string("\xef\xbb\xbf") + ops.dump());
          auto before = Call(runtime, "animation_describe");
          const auto edited =
              Call(runtime, "animation_edit",
                   J::object({{"operations_path", Monstey_Utf8(path.wstring())},
                              {"expected_sha256", Monstey_HashSha256(path)},
                              {"expected_generation", before.at("generation")},
                              {"expected_revision", before.at("revision")}}));
          Check(edited.at("result").at("undo_steps").as_uint() == 1 &&
                    edited.at("result").at("clip_count").as_uint() == 3,
                "Animation file batch split the history");
          Error(runtime, "animation_edit",
                J::object({{"operations", ops},
                           {"expected_revision", before.at("revision")}}),
                "stale_revision");
          before = Call(runtime, "animation_changes");
          Error(runtime, "animation_edit", J::object(), "invalid_arguments");
          Error(runtime, "animation_edit",
                J::object({{"operations", ops},
                           {"operations_path", Monstey_Utf8(path.wstring())}}),
                "invalid_arguments");
          Error(runtime, "animation_edit",
                J::object({{"operations", ops},
                           {"expected_sha256", Monstey_HashSha256(path)}}),
                "invalid_arguments");
          const auto invalid =
              J::array({J::object({{"op", "duplicate"},
                                   {"clip", "walk"},
                                   {"name", "transient"}}),
                        J::object({{"op", "delete"}, {"clip", "bind"}})});
          const auto failed = runtime.Call(
              J::object({{"operation", "animation_edit"},
                         {"arguments", J::object({{"operations", invalid}})}}));
          const auto after = Call(runtime, "animation_changes");
          Check(!failed.at("ok").as_bool() &&
                    before.at("result") == after.at("result") &&
                    before.at("revision") == after.at("revision"),
                "Invalid animation batch committed state or revision");
          Call(runtime, "animation_undo");
          Check(!Call(runtime, "animation_describe")
                     .at("result")
                     .at("dirty")
                     .as_bool(),
                "Animation undo did not restore baseline");
          Call(runtime, "animation_redo");
        });
    test("animation project export import build and semantic new preserve "
         "native data",
         [&] {
           const auto project = directory / "animation.project.json";
           const auto first = directory / "animation-first.anix";
           const auto second = directory / "animation-replay.anix";
           const auto semantic = directory / "animation-semantic.json";
           Call(runtime, "animation_export",
                J::object({{"output", Monstey_Utf8(semantic.wstring())},
                           {"clip", "walk"}}));
           Call(runtime, "animation_import_clip",
                J::object({{"scene_path", Monstey_Utf8(semantic.wstring())},
                           {"clip", "walk"},
                           {"name", "imported"},
                           {"expected_sha256", Monstey_HashSha256(semantic)}}));
           Call(runtime, "animation_save_project",
                J::object({{"path", Monstey_Utf8(project.wstring())}}));
           Call(runtime, "animation_build",
                J::object({{"output", Monstey_Utf8(first.wstring())}}));
           const auto before = Call(runtime, "animation_describe");
           Call(runtime, "animation_revert");
           Check(!Call(runtime, "animation_describe")
                      .at("result")
                      .at("dirty")
                      .as_bool(),
                 "Animation runtime revert failed");
           Call(runtime, "animation_load_project",
                J::object({{"path", Monstey_Utf8(project.wstring())}}));
           Call(runtime, "animation_build",
                J::object({{"output", Monstey_Utf8(second.wstring())}}));
           Check(Monstey_ReadFile(first) == Monstey_ReadFile(second) &&
                     Call(runtime, "animation_clip",
                          J::object({{"clip", "imported"}}))
                             .at("result")
                             .at("duration_ticks")
                             .as_uint() == 1000,
                 "Animation runtime project replay differs");
           Error(runtime, "animation_describe",
                 J::object({{"expected_generation", before.at("generation")}}),
                 "stale_session");
           const auto sourceScene = directory / "animation-new.json";
           Monstey_WriteNewText(sourceScene, Monstey_AnimationScene().dump());
           Call(runtime, "animation_new",
                J::object(
                    {{"scene_path", Monstey_Utf8(sourceScene.wstring())}}));
           Check(Call(runtime, "animation_describe")
                         .at("result")
                         .at("source_kind")
                         .as_string() == "semantic",
                 "Semantic animation source was not opened");
         });
    test("imported animation scenes cannot be overwritten by settings "
         "persistence",
         [&] {
           const auto sourceScene =
               directory / "animation-settings-source.json";
           Monstey_WriteNewText(sourceScene, Monstey_AnimationScene().dump());
           const auto hash = Monstey_HashSha256(sourceScene);
           Monstey_Runtime samePath({}, sourceScene);
           Call(samePath, "animation_open",
                J::object({{"path", Monstey_Utf8(animationSource.wstring())}}));
           Call(samePath, "animation_import_clip",
                J::object({{"scene_path", Monstey_Utf8(sourceScene.wstring())},
                           {"clip", "walk"},
                           {"name", "copy"}}));
           Error(samePath, "configure", J::object({{"save", true}}),
                 "validation_failed");
           Check(Monstey_HashSha256(sourceScene) == hash,
                 "Settings persistence overwrote an imported animation scene");
         });

    test("native owner attachment routes share FX history and session guards",
         [&] {
           auto f = [](const char *name, unsigned type, J value) {
             return J::object({{"name", name},
                               {"type_code", type},
                               {"array", 1},
                               {"default", value}});
           };
           auto mode = f("UpdatePos", 5, "Follow");
           mode["enum_values"] = J::array(
               {"Fixed", "Follow", "NodeAttach", "SocketAttach", "FollowPos",
                "NodeAttachPos", "FollowWSOffset", "NodeAttachWSOffset"});
           auto links = f("KeyList", 10, nullptr);
           links["array"] = -1;
           links["target"] = "FxKey";
           const auto schema = directory / "owner-attachment-schema.json";
           auto baseFields =
               J::array({f("KeyName", 4, ""), f("StartTime", 2, 0.0),
                         f("EndTime", 2, 1.0), mode, f("AttachName", 4, "")});
           auto derivedFields = baseFields;
           derivedFields.as_array().push_back(
               f("FutureProperty", 4, "Keep original"));
           Monstey_WriteNewText(
               schema,
               J::object(
                   {{"reference_aliases", J::object({{"FxKey", "BaseFX"}})},
                    {"categories",
                     J::object({{"FxGlobalProps",
                                 J::object({{"fields",
                                             J::array({f("FxName", 4, ""),
                                                       f("ResourceLenMS", 3, 0),
                                                       links})}})},
                                {"BaseFX", J::object({{"fields", baseFields}})},
                                {"ClientFX.Model",
                                 J::object({{"parent", "BaseFX"},
                                            {"fields", derivedFields}})}})}})
                   .dump());
           Call(runtime, "fx_new",
                J::object({{"schema_path", Monstey_Utf8(schema.wstring())},
                           {"name", "fx/runtime/owner"},
                           {"duration_ms", 3000}}));
           const auto key = Call(runtime, "fx_create_element",
                                 J::object({{"class_name", "ClientFX.Model"},
                                            {"name", "Owned component"}}))
                                .at("result")
                                .at("result")
                                .at("reference");
           const auto before = Call(runtime, "describe");
           auto args =
               J::object({{"record", key},
                          {"expected_generation", before.at("generation")},
                          {"expected_revision", before.at("revision")}});
           const auto inspected = Call(runtime, "fx_attachment_inspect", args);
           Check(inspected.at("revision") == before.at("revision") &&
                     inspected.at("result")
                             .at("effective_update_pos")
                             .as_string() == "Follow" &&
                     !inspected.at("result")
                          .at("external_owner_resolved")
                          .as_bool(),
                 "Native owner inspection changed state or claimed an external "
                 "owner");
           args["update_pos"] = "NodeAttachPos";
           args["attach_name"] = "R_Weapon";
           const auto edited = Call(runtime, "fx_attachment_set", args);
           Check(edited.at("revision").as_uint() ==
                     before.at("revision").as_uint() + 1,
                 "Combined owner settings did not use one runtime revision");
           auto view = Call(runtime, "fx_attachment_inspect",
                            J::object({{"record", key}}));
           Check(view.at("result").at("native_follow_flags").as_uint() ==
                         0x29 &&
                     view.at("result").at("attach_name").as_string() ==
                         "R_Weapon",
                 "Native owner route lost declared flags or name");
           Error(runtime, "fx_attachment_set", args, "stale_revision");
           const auto stable = Call(runtime, "fx_attachment_inspect",
                                    J::object({{"record", key}}));
           auto bad = J::object(
               {{"operation", "fx_attachment_set"},
                {"arguments",
                 J::object({{"record", key},
                            {"update_pos", "Fixed"},
                            {"attach_name", std::string("A\0B", 3)}})}});
           const auto failed = runtime.Call(bad);
           const auto unchanged = Call(runtime, "fx_attachment_inspect",
                                       J::object({{"record", key}}));
           Check(!failed.at("ok").as_bool() &&
                     unchanged.at("revision") == stable.at("revision") &&
                     unchanged.at("result") == stable.at("result"),
                 "Rejected attachment update committed partial state");
           Call(runtime, "fx_undo");
           Check(Call(runtime, "fx_attachment_inspect",
                      J::object({{"record", key}}))
                         .at("result")
                         .at("update_pos")
                         .as_string() == "Follow",
                 "Combined owner settings split into multiple undo steps");
           Call(runtime, "fx_redo");
           const auto project = directory / "owner-attachment-project.json";
           const auto direct = directory / "owner-attachment-direct.fxp";
           const auto replay = directory / "owner-attachment-replay.fxp";
           Call(runtime, "fx_save_project",
                J::object({{"path", Monstey_Utf8(project.wstring())}}));
           Call(runtime, "fx_build",
                J::object({{"output", Monstey_Utf8(direct.wstring())}}));
           Call(runtime, "fx_load_project",
                J::object({{"path", Monstey_Utf8(project.wstring())}}));
           Call(runtime, "fx_build",
                J::object({{"output", Monstey_Utf8(replay.wstring())}}));
           Check(Monstey_ReadFile(direct) == Monstey_ReadFile(replay),
                 "Owner attachment project replay differs");
         });
    if (argc > 2) {
      Check(
          argc >= 4,
          "Supply synthetic.mesh and synthetic.skel together after outputDir");
      const std::filesystem::path fixtureMesh = Monstey_Wide(argv[2]),
                                  fixtureSkel = Monstey_Wide(argv[3]);
      const auto sourceHash = Monstey_HashSha256(fixtureMesh),
                 skeletonHash = Monstey_HashSha256(fixtureSkel);
      const auto modelDirectory = directory / "model-bands";
      std::filesystem::create_directories(modelDirectory);
      const auto banded = modelDirectory / "band-five.mesh",
                 glb = modelDirectory / "band-five.glb",
                 blend = modelDirectory / "band-five.blend",
                 staleGlb = modelDirectory / "stale-topology.glb",
                 unassignedBlend = modelDirectory / "unassigned-face.blend";
      std::string bandedHash, glbHash, blendHash;
      test("model runtime creates source-qualified synthetic band fixtures",
           [&] {
             const auto scene =
                 Monstey_ModelReadScene(fixtureMesh, fixtureSkel);
             (void)Monstey_ModelWriteScene(scene, fixtureMesh, banded,
                                           J::object({{"triangle_band", 5}}));
             Monstey_RuntimeCheckBand(banded, 5);
             Call(runtime, "model_export_glb",
                  J::object({{"source", Monstey_Utf8(banded.wstring())},
                             {"skeleton", Monstey_Utf8(fixtureSkel.wstring())},
                             {"output", Monstey_Utf8(glb.wstring())}}));
             Call(runtime, "model_import_blend",
                  J::object(
                      {{"source", Monstey_Utf8(banded.wstring())},
                       {"skeleton", Monstey_Utf8(fixtureSkel.wstring())},
                       {"output", Monstey_Utf8(blend.wstring())},
                       {"expected_source_sha256", Monstey_HashSha256(banded)},
                       {"expected_skeleton_sha256", skeletonHash}}));
             Monstey_RuntimeStaleBandGlb(glb, staleGlb);
             Monstey_RuntimeUnassignedBandBlend(blend, unassignedBlend);
             bandedHash = Monstey_HashSha256(banded);
             glbHash = Monstey_HashSha256(glb);
             blendHash = Monstey_HashSha256(blend);
           });
      for (const char *name :
           {"model_convert", "model_export_blend", "model_export_native"}) {
        const auto source =
            std::string_view(name) == "model_convert" ? glb : blend;
        const auto omissionName =
            std::string(name) + " omission preserves qualified face bands";
        test(omissionName.c_str(), [&] {
          const auto output =
              modelDirectory / (std::string(name) + "-preserve.mesh");
          auto args = modelArguments(name, source, banded, output);
          const auto reply = Call(runtime, name, args);
          Check(!reply.at("result").contains("triangle_band_policy") ||
                    reply.at("result").at("triangle_band_policy").as_string() !=
                        "explicit_uniform",
                "Omitted triangle band became an explicit override");
          Check(Monstey_ReadFile(output) == Monstey_ReadFile(banded),
                "Omitted band changed a pristine native roundtrip");
          Monstey_RuntimeCheckBand(output, 5);
        });
        const auto zeroName =
            std::string(name) + " explicit zero overrides qualified bands";
        test(zeroName.c_str(), [&] {
          const auto output =
              modelDirectory / (std::string(name) + "-zero.mesh");
          auto args = modelArguments(name, source, banded, output);
          args["triangle_band"] = 0;
          const auto reply = Call(runtime, name, args);
          Check(reply.at("result").at("triangle_band_policy").as_string() ==
                        "explicit_uniform" &&
                    reply.at("result").at("triangle_band").as_int() == 0,
                "Explicit zero disappeared from the native route");
          Monstey_RuntimeCheckBand(output, 0);
          Check(Monstey_ReadFile(output) != Monstey_ReadFile(banded),
                "Explicit zero did not replace source band five");
        });
      }
      test("model_convert resolves top-level and nested band precedence", [&] {
        auto args = modelArguments("model_convert", glb, banded,
                                   modelDirectory / "top-level-zero.mesh");
        args["options"] = J::object({{"triangle_band", 16}});
        args["triangle_band"] = 0;
        Call(runtime, "model_convert", args);
        Monstey_RuntimeCheckBand(modelDirectory / "top-level-zero.mesh", 0);
        args.as_object().erase("triangle_band");
        args["output"] =
            Monstey_Utf8((modelDirectory / "nested-sixteen.mesh").wstring());
        Call(runtime, "model_convert", args);
        Monstey_RuntimeCheckBand(modelDirectory / "nested-sixteen.mesh", 16);
      });
      for (const char *name :
           {"model_convert", "model_export_blend", "model_export_native"}) {
        const auto source = std::string_view(name) == "model_convert"
                                ? staleGlb
                                : unassignedBlend;
        const auto earlyName =
            std::string(name) + " forwards zero before semantic-band rejection";
        test(earlyName.c_str(), [&] {
          const auto rejected =
              modelDirectory / (std::string(name) + "-unqualified.mesh");
          auto args = modelArguments(name, source, banded, rejected);
          const auto before = Call(runtime, "describe");
          const auto failed = runtime.Call(
              J::object({{"operation", name}, {"arguments", args}}));
          Check(!failed.at("ok").as_bool() &&
                    !std::filesystem::exists(rejected),
                "Unqualified semantic face metadata was accepted implicitly");
          Check(Call(runtime, "describe").at("revision") ==
                    before.at("revision"),
                "Rejected semantic metadata changed the runtime revision");
          const auto output =
              modelDirectory / (std::string(name) + "-early-zero.mesh");
          if (std::string_view(name) == "model_export_native")
            args["output_mesh"] = Monstey_Utf8(output.wstring());
          else
            args["output"] = Monstey_Utf8(output.wstring());
          args["triangle_band"] = 0;
          Call(runtime, name, args);
          Monstey_RuntimeCheckBand(output, 0);
        });
      }
      test("triangle-band overrides preserve supplied source hashes", [&] {
        const auto before = Call(runtime, "describe");
        for (const char *name :
             {"model_convert", "model_export_blend", "model_export_native"}) {
          const auto source =
              std::string_view(name) == "model_convert" ? glb : blend;
          const auto output =
              modelDirectory / (std::string(name) + "-wrong-source.mesh");
          auto args = modelArguments(name, source, banded, output);
          args["triangle_band"] = 0;
          args[std::string_view(name) == "model_export_native"
                   ? "expected_sha256"
                   : "expected_source_sha256"] = std::string(64, '0');
          const auto failed = runtime.Call(
              J::object({{"operation", name}, {"arguments", args}}));
          Check(!failed.at("ok").as_bool() && !std::filesystem::exists(output),
                "Band override bypassed the expected source hash");
        }
        Check(Call(runtime, "describe").at("revision") == before.at("revision"),
              "Rejected model source hash changed the runtime revision");
      });
      test("triangle-band overrides preserve supplied template hashes", [&] {
        const auto before = Call(runtime, "describe");
        for (const char *name : {"model_convert", "model_export_blend"}) {
          const auto source =
              std::string_view(name) == "model_convert" ? glb : blend;
          const auto output =
              modelDirectory / (std::string(name) + "-wrong-template.mesh");
          auto args = modelArguments(name, source, banded, output);
          args["template"] = Monstey_Utf8(banded.wstring());
          args["triangle_band"] = 0;
          args["expected_source_sha256"] = Monstey_HashSha256(source);
          args["expected_template_sha256"] = std::string(64, '0');
          const auto failed = runtime.Call(
              J::object({{"operation", name}, {"arguments", args}}));
          Check(!failed.at("ok").as_bool() && !std::filesystem::exists(output),
                "Band override bypassed the expected template hash");
        }
        Check(Call(runtime, "describe").at("revision") == before.at("revision"),
              "Rejected model template hash changed the runtime revision");
      });
      test("model band fixture inputs remain unchanged", [&] {
        Check(Monstey_HashSha256(fixtureMesh) == sourceHash &&
                  Monstey_HashSha256(fixtureSkel) == skeletonHash &&
                  Monstey_HashSha256(banded) == bandedHash &&
                  Monstey_HashSha256(glb) == glbHash &&
                  Monstey_HashSha256(blend) == blendHash,
              "Runtime model tests changed supplied synthetic inputs");
        Monstey_RuntimeCheckBand(banded, 5);
      });
    }
    test("native identity description", [&] {
      const auto description = Call(runtime, "describe").at("result");
      Check(description.at("implementation").as_string() == "C++20" &&
                !description.at("python_runtime").as_bool(),
            "unexpected runtime");
    });
    Monstey_WriteNewText(
        directory / "runtime-tests.json",
        J::object(
            {{"passed", passed}, {"failed", 0}, {"implementation", "C++20"}})
            .dump(2));
    std::cout << passed << " tests passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
