#include "Monstey_Archive.hpp"
#include "Monstey_Core.hpp"
#include "Monstey_WorldComposition.hpp"
#include "Monstey_WorldEdit.hpp"
#include <algorithm>
#include <iostream>

using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
std::string Monstey_TextPath(const std::filesystem::path &path) {
  return Monstey_Utf8(path.wstring());
}
J Monstey_Put(const std::filesystem::path &root, const char *name,
              const B &bytes) {
  const auto path = root / Monstey_Wide(name);
  Monstey_WriteNewFile(path, bytes);
  return J::object({{"path", Monstey_TextPath(path)},
                    {"bytes", static_cast<std::uint64_t>(bytes.size())},
                    {"sha256", Monstey_HashSha256(bytes)}});
}
J Monstey_Generated(const char *path, const J &source) {
  return J::object({{"backend", "DEPENDENCY"},
                    {"dependency_kind", "OPAQUE"},
                    {"resource_path", path},
                    {"candidate", source}});
}
B Monstey_Child(const std::filesystem::path &path, const char *resource) {
  const auto children = Monstey_ReadEmbedded(Monstey_ReadFile(path));
  const auto child =
      std::find_if(children.begin(), children.end(), [&](const auto &value) {
        return value.resource == resource;
      });
  Monstey_Require(child != children.end(), "Embedded child is absent");
  return child->bytes;
}
void Monstey_FinalEvidence(const J &result, const char *resource,
                           const B &finalBytes) {
  const auto &rows = result.at("component_evidence").as_array();
  const auto row =
      std::find_if(rows.begin(), rows.end(), [&](const auto &value) {
        return value.contains("resource_path") &&
               value.at("resource_path").as_string() == resource;
      });
  Monstey_Require(row != rows.end() &&
                      row->at("package_residency_valid").as_bool() &&
                      row->at("candidate_sha256").as_string() ==
                          Monstey_HashSha256(finalBytes),
                  "Final component evidence differs from published bytes");
}
J Monstey_Row(const char *path, const J &source, J resources,
              const char *embeddedPath, const J &embeddedSource) {
  return J::object({{"resource_path", path},
                    {"source", source},
                    {"resources", std::move(resources)},
                    {"embb_resource_path", embeddedPath},
                    {"embb_source", embeddedSource}});
}
} // namespace

int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Require(argc == 2, "NEW-output-directory required");
    const std::filesystem::path out = argv[1];
    Monstey_Require(!std::filesystem::exists(out), "Output exists");
    std::filesystem::create_directories(out);
    const B old{1}, fresh{2, 3, 4};
    const auto inner = Monstey_BuildBundle(
        {"Bundles/Types:WorldQuest", {{"worlds/old.bin", 1}}});
    const auto innerBytes = Monstey_BuildEmbedded({{"worlds/old.bin", old}});
    const auto innerSource = Monstey_Put(out, "inner.bndl", inner);
    const auto innerEmbedded = Monstey_Put(out, "inner.embb", innerBytes);
    const auto outerSource = Monstey_Put(
        out, "outer.bndl",
        Monstey_BuildBundle({"Bundles/Types:WorldQuest",
                             {{"worlds/inner.bndl",
                               static_cast<std::uint32_t>(inner.size())}}}));
    const auto outerEmbedded =
        Monstey_Put(out, "outer.embb",
                    Monstey_BuildEmbedded({{"worlds/inner.bndl", inner}}));
    const auto freshDesc = Monstey_Put(out, "new.bin", fresh);
    const auto generated =
        J::array({Monstey_Generated("worlds/inner.bndl", innerSource),
                  Monstey_Generated("worlds/new.bin", freshDesc)});
    const auto outerRow = Monstey_Row("worlds/outer.bndl", outerSource,
                                      J::array({"worlds/inner.bndl"}),
                                      "worlds/outer.embb", outerEmbedded);
    auto innerRow = Monstey_Row("worlds/inner.bndl", innerSource,
                                J::array({"worlds/new.bin"}),
                                "worlds/inner.embb", innerEmbedded);
    innerRow["additions"] = J::array({"worlds/new.bin"});
    J::Array cases;
    for (unsigned order = 0; order < 3; ++order) {
      const auto label = order == 0   ? "outer-first"
                         : order == 1 ? "inner-first"
                                      : "implicit-inner-source";
      const auto plan =
          J::object({{"schema", "Monstey_WorldEdit_1"},
                     {"generated_resources",
                      order == 2 ? J::array({generated[1]}) : generated},
                     {"bundles", order == 1 ? J::array({innerRow, outerRow})
                                            : J::array({outerRow, innerRow})}});
      const auto dest = out / Monstey_Wide(label);
      const auto result = Monstey_WorldEditCall(
          "sow_worldedit_compile",
          J::object({{"plan", plan}, {"output", Monstey_TextPath(dest)}}));
      const auto finalInner =
          Monstey_ReadFile(dest / L"assets/worlds/inner.bndl");
      Monstey_Require(Monstey_Child(dest / L"assets/worlds/outer.embb",
                                    "worlds/inner.bndl") == finalInner,
                      "Final nested BNDL differs from embedded bytes");
      if (order != 2)
        Monstey_FinalEvidence(result, "worlds/inner.bndl", finalInner);
      Monstey_Require(std::filesystem::exists(dest / L"resource-manifest.json"),
                      "Completion marker absent");
      cases.push_back(J::object(
          {{"case", label},
           {"status", "PASS"},
           {"final_candidate_sha256", Monstey_HashSha256(finalInner)}}));
    }
    // A physical EMBB payload must follow the final inner EMBB, regardless of
    // plan order.
    const auto physicalOuterSource = Monstey_Put(
        out, "physical-outer.bndl",
        Monstey_BuildBundle({"Bundles/Types:WorldQuest",
                             {{"worlds/inner.embb", static_cast<std::uint32_t>(
                                                        innerBytes.size())}}}));
    const auto physicalOuterEmbedded =
        Monstey_Put(out, "physical-outer.embb",
                    Monstey_BuildEmbedded({{"worlds/inner.embb", innerBytes}}));
    const auto physicalOuterRow =
        Monstey_Row("worlds/physical-outer.bndl", physicalOuterSource,
                    J::array({"worlds/inner.embb"}),
                    "worlds/physical-outer.embb", physicalOuterEmbedded);
    for (unsigned order = 0; order < 2; ++order) {
      const auto label =
          order == 0 ? "physical-outer-first" : "physical-inner-first";
      const auto plan = J::object(
          {{"schema", "Monstey_WorldEdit_1"},
           {"generated_resources",
            J::array({Monstey_Generated("worlds/inner.embb", innerEmbedded),
                      generated[1]})},
           {"bundles", order == 0 ? J::array({physicalOuterRow, innerRow})
                                  : J::array({innerRow, physicalOuterRow})}});
      const auto dest = out / Monstey_Wide(label);
      const auto result = Monstey_WorldEditCall(
          "sow_worldedit_compile",
          J::object({{"plan", plan}, {"output", Monstey_TextPath(dest)}}));
      const auto finalInner =
          Monstey_ReadFile(dest / L"assets/worlds/inner.embb");
      Monstey_Require(
          finalInner != innerBytes &&
              Monstey_Child(dest / L"assets/worlds/physical-outer.embb",
                            "worlds/inner.embb") == finalInner &&
              Monstey_Child(dest / L"assets/worlds/inner.embb",
                            "worlds/new.bin") == fresh,
          "Final physical nesting differs");
      Monstey_FinalEvidence(result, "worlds/inner.embb", finalInner);
      const auto bundle = Monstey_ReadBundle(
          Monstey_ReadFile(dest / L"assets/worlds/physical-outer.bndl"));
      Monstey_Require(bundle.resources.size() == 1 &&
                          bundle.resources[0].size_hint == finalInner.size(),
                      "Final physical child size hint differs");
      cases.push_back(J::object({{"case", label}, {"status", "PASS"}}));
    }
    // Finite source files can request an impossible physical self-expansion
    // cycle.
    const auto cycleASource =
        Monstey_Put(out, "cycle-a.bndl",
                    Monstey_BuildBundle({"Bundles/Types:WorldQuest",
                                         {{"worlds/cycle-b.embb", 1}}}));
    const auto cycleBSource =
        Monstey_Put(out, "cycle-b.bndl",
                    Monstey_BuildBundle({"Bundles/Types:WorldQuest",
                                         {{"worlds/cycle-a.embb", 1}}}));
    const auto cycleAEmbedded =
        Monstey_Put(out, "cycle-a.embb",
                    Monstey_BuildEmbedded({{"worlds/cycle-b.embb", old}}));
    const auto cycleBEmbedded =
        Monstey_Put(out, "cycle-b.embb",
                    Monstey_BuildEmbedded({{"worlds/cycle-a.embb", old}}));
    auto cycleA =
        Monstey_Row("worlds/cycle-a.bndl", cycleASource,
                    J::array({"worlds/cycle-b.embb", "worlds/new.bin"}),
                    "worlds/cycle-a.embb", cycleAEmbedded);
    cycleA["additions"] = J::array({"worlds/new.bin"});
    const auto cycleB = Monstey_Row("worlds/cycle-b.bndl", cycleBSource,
                                    J::array({"worlds/cycle-a.embb"}),
                                    "worlds/cycle-b.embb", cycleBEmbedded);
    const auto cycleDest = out / L"physical-cycle";
    bool rejected = false;
    try {
      (void)Monstey_WorldEditCall(
          "sow_worldedit_compile",
          J::object(
              {{"plan",
                J::object({{"schema", "Monstey_WorldEdit_1"},
                           {"generated_resources", J::array({generated[1]})},
                           {"bundles", J::array({cycleA, cycleB})}})},
               {"output", Monstey_TextPath(cycleDest)}}));
    } catch (const std::exception &error) {
      rejected = std::string(error.what()).find("physical byte-copy cycle") !=
                 std::string::npos;
    }
    Monstey_Require(rejected && !std::filesystem::exists(
                                    cycleDest / L"resource-manifest.json"),
                    "Physical cycle was not rejected before publication");
    cases.push_back(
        J::object({{"case", "physical-cycle-rejected-before-publication"},
                   {"status", "PASS"}}));
    const auto result = J::object({{"status", "PASS"},
                                   {"cases", cases},
                                   {"runtime_verified", false},
                                   {"game_or_codec_executed", false}});
    Monstey_WriteNewText(out / L"report.json", result.dump(2));
    std::cout << result.dump(2) << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}