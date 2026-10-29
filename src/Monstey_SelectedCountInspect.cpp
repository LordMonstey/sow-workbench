#include "Monstey_Gadb.hpp"
#include "Monstey_GadbClassDeclaration.hpp"
#include "Monstey_GadbSelectedCountWord.hpp"
#include <array>
#include <iostream>
#include <map>
using namespace Monstey;
namespace {
using Monstey_InspectAddressWord = std::uint64_t;
Monstey_InspectAddressWord Monstey_Address(const Monstey_Json &v) {
  return std::stoull(v.as_string(), nullptr, 16);
}
void Monstey_Put(std::vector<std::uint8_t> &b, Monstey_InspectAddressWord at, Monstey_InspectAddressWord v,
                 std::size_t width) {
  for (std::size_t i = 0; i < width; ++i)
    b.at(static_cast<std::size_t>(at) + i) =
        static_cast<std::uint8_t>(v >> (8 * i));
}
Monstey_GadbSelectedCountField Monstey_Select(const std::string &category,
                                              const std::string &field) {
  if (category == "Quest.Quest" && field == "Predicates")
    return Monstey_GadbSelectedCountField::QuestPredicates;
  if (category == "AI.NameEvent" && field == "CustomDetailWidgetData")
    return Monstey_GadbSelectedCountField::NameEventCustomDetailWidgetData;
  Monstey_Require(category == "Presentation.Element",
                  "Unexpected selected category");
  if (field == "AdvanceOnItem")
    return Monstey_GadbSelectedCountField::PresentationAdvanceOnItem;
  if (field == "Dissolve")
    return Monstey_GadbSelectedCountField::PresentationDissolve;
  if (field == "AOE")
    return Monstey_GadbSelectedCountField::PresentationAOE;
  if (field == "ActionsOnStart")
    return Monstey_GadbSelectedCountField::PresentationActionsOnStart;
  if (field == "ActionsOnEnd")
    return Monstey_GadbSelectedCountField::PresentationActionsOnEnd;
  throw std::runtime_error("Unexpected selected field");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Require(argc == 8, "Usage: SelectedCountInspect input input_sha PE "
                               "PE_sha DB DB_sha NEW_report");
    const std::filesystem::path inputPath(argv[1]), imagePath(argv[3]),
        databasePath(argv[5]), output(argv[7]);
    const std::array inputPaths{inputPath, imagePath, databasePath};
    Monstey_RequireOfflineOutput(output, inputPaths);
    Monstey_Require(!std::filesystem::exists(output), "New output required");
    const auto inputBytes = Monstey_ReadFile(inputPath),
               image = Monstey_ReadFile(imagePath);
    const auto inputSha = Monstey_HashSha256(inputBytes),
               imageSha = Monstey_HashSha256(image),
               databaseSha = Monstey_HashSha256(databasePath);
    Monstey_Require(inputSha == Monstey_Utf8(argv[2]) &&
                        imageSha == Monstey_Utf8(argv[4]) &&
                        databaseSha == Monstey_Utf8(argv[6]),
                    "Pinned input differs");
    Monstey_Require(imageSha == "6b8db778135c082c934463b6524e6b220478fb6987f082"
                                "ab37ca48a870acb455" ||
                        imageSha == "ed0d95cabb033cd66aaa99d699374881846c3cb03e"
                                    "c345f1123d82d45e0bdaef",
                    "Native PE profile is unqualified");
    const auto selected =
        Monstey_Json::Parse(std::string(inputBytes.begin(), inputBytes.end()));
    const std::array<Monstey_InspectAddressWord, 3> pointers{0x142645620, 0x14266baf0,
                                              0x142648260};
    const auto declarations =
        Monstey_GadbInspectClassPointers(image, pointers, imageSha);
    std::map<Monstey_InspectAddressWord, const Monstey_Json *> classes;
    for (const auto &c : declarations.at("classes").as_array())
      classes.emplace(Monstey_Address(c.at("class_pointer")), &c);
    Monstey_Gadb db(databasePath);
    std::map<std::string, Monstey_Json, std::less<>> records;
    const auto get = [&](const Monstey_Json &ref) -> const Monstey_Json & {
      const auto &s = ref.as_string();
      if (const auto found = records.find(s); found != records.end())
        return found->second;
      const auto packed = Monstey_Address(ref);
      const auto page =
          db.DescriptorLayouts(static_cast<std::uint32_t>(packed >> 20),
                               static_cast<std::size_t>(packed & 0xfffff), 1);
      Monstey_Require(page.at("published_order_provenance") ==
                              Monstey_Json("native_source") &&
                          page.at("rows").size() == 1 &&
                          page.at("rows").at(0).at("reference") == ref,
                      "Exact selected source ordinal differs");
      return records.emplace(s, page.at("rows").at(0)).first->second;
    };
    Monstey_Json::Array rows;
    std::size_t witnesses = 0;
    for (const auto &sourceRow : selected.at("rows").as_array()) {
      const auto &issue = sourceRow.at("original_contract");
      const auto &category = issue.at("category").as_string();
      const auto &fieldName = issue.at("field").as_string();
      const auto path = Monstey_Select(category, fieldName);
      const auto &current = get(issue.at("record"));
      const auto &first = get(issue.at("first_record"));
      const auto physical =
          static_cast<std::size_t>(issue.at("physical_index").as_uint());
      for (const auto &pair :
           {std::pair{&current, &issue.at("actual_descriptor")},
            std::pair{&first, &issue.at("first_descriptor")}}) {
        const auto &f = pair.first->at("fields").at(physical);
        for (const auto &key :
             {"hash", "count", "native_type", "native_ordinal"})
          Monstey_Require(f.at(key) == pair.second->at(key),
                          "Pinned source descriptor differs");
      }
      auto row =
          Monstey_Json::object({{"category", category},
                                {"field", fieldName},
                                {"record", issue.at("record")},
                                {"source_count", 0},
                                {"declaration_count", 1},
                                {"classification", "unknown_value_contract"},
                                {"runtime_history_assumed", false}});
      const auto *c =
          classes.at(Monstey_Address(sourceRow.at("native_class_pointer")));
      if (c->at("field_storage_status") !=
          Monstey_Json("COMPLETE_FILE_BACKED")) {
        row["witness"] = "NATIVE_DECLARATIONS_UNAVAILABLE_IN_FILE_PROFILE";
        rows.push_back(std::move(row));
        continue;
      }
      const auto nativeIndex =
          static_cast<std::size_t>(issue.at("native_ordinal").as_uint());
      const auto &f = c->at("fields").at(nativeIndex);
      Monstey_Require(f.at("name").at("value") == Monstey_Json(fieldName) &&
                          f.at("count_u32") == Monstey_Json(1) &&
                          f.at("name_hash_u32") ==
                              issue.at("actual_field_hash") &&
                          f.at("captured_cache_byte_u8").as_uint() == physical,
                      "Selected declaration identity mismatch");
      Monstey_Require(
          f.at("captured_binding_state_u32").as_uint() >= 2,
          "Selected owned witness requires captured cached declaration state");
      std::vector<std::uint8_t> b(0x8000);
      Monstey_GadbSelectedCountClasses ids{0x100, 0x180, 0x200};
      Monstey_Put(b, ids.presentation, 0x1000, 8);
      Monstey_Put(b, ids.name_event, 0x1000, 8);
      Monstey_Put(b, ids.quest, 0x1000, 8);
      const auto &hex = f.at("raw72_hex").as_string();
      Monstey_Require(hex.size() == 144, "Expected72 declaration bytes");
      for (std::size_t i = 0; i < 72; ++i)
        b.at(0x1000 + nativeIndex * 72 + i) = static_cast<std::uint8_t>(
            std::stoul(hex.substr(i * 2, 2), nullptr, 16));
      Monstey_Put(b, 0x408, 0x5000, 8);
      Monstey_Put(b, 0x410, UINT64_MAX, 8);
      Monstey_Put(b, 0x5000 + physical * 16 + 12, 0, 2);
      const auto before = b;
      const auto value =
          Monstey_GadbReadSelectedCountWord({0, b}, 0x400, path, ids);
      Monstey_Require(value == 0 && before == b,
                      "Qualified owned zero-count witness differs");
      row["witness"] = "OWNED_ZERO_COUNT_INSTANCE_WITH_CAPTURED_DECLARATION";
      row["value_storage_unavailable_but_not_read"] = true;
      row["returned_raw64_zero"] = true;
      row["borrowed_bytes_unchanged"] = true;
      ++witnesses;
      rows.push_back(std::move(row));
    }
    Monstey_Require(rows.size() == 20, "Expected20 selected rows");
    Monstey_Require(Monstey_HashSha256(inputPath) == inputSha &&
                        Monstey_HashSha256(imagePath) == imageSha &&
                        Monstey_HashSha256(databasePath) == databaseSha,
                    "Sources changed");
    const auto report = Monstey_Json::object(
        {{"schema", "Monstey.SelectedCountSourceInspection/1"},
         {"passed", true},
         {"selected_rows", rows.size()},
         {"owned_cached_absence_witnesses", witnesses},
         {"distinct_source_records_read", records.size()},
         {"input_sha256", inputSha},
         {"native_PE_sha256", imageSha},
         {"database_sha256", databaseSha},
         {"original_unknown120_unchanged", true},
         {"native_runtime_history_reconstructed", false},
         {"original_code_executed", false},
         {"source_unchanged", true},
         {"rows", std::move(rows)}});
    Monstey_WriteNewText(output, report.dump(2) + "\n");
    std::cout << "PASS20 rows " << witnesses << " owned witnesses\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
