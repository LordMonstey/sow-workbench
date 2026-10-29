#include "Monstey_Gadb.hpp"
#include "Monstey_GadbAudioActionWord.hpp"
#include "Monstey_GadbClassDeclaration.hpp"
#include <array>
#include <iostream>
#include <map>
using namespace Monstey;
namespace {
using Monstey_InspectAddressWord = std::uint64_t;
Monstey_InspectAddressWord Monstey_Address(const Monstey_Json &x) {
  return std::stoull(x.as_string(), nullptr, 16);
}
void Monstey_Put(std::vector<std::uint8_t> &b, Monstey_InspectAddressWord a, Monstey_InspectAddressWord v, std::size_t w) {
  for (std::size_t i = 0; i < w; ++i)
    b.at(static_cast<std::size_t>(a) + i) =
        static_cast<std::uint8_t>(v >> (i * 8));
}
void Monstey_Raw(std::vector<std::uint8_t> &b, Monstey_InspectAddressWord at, const Monstey_Json &field) {
  const auto &hex = field.at("raw72_hex").as_string();
  Monstey_Require(hex.size() == 144, "Expected raw72 declaration");
  for (std::size_t i = 0; i < 72; ++i)
    b.at(static_cast<std::size_t>(at) + i) = static_cast<std::uint8_t>(
        std::stoul(hex.substr(i * 2, 2), nullptr, 16));
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Require(argc == 8, "Usage: AudioCountInspect selected_json "
                               "selected_sha PE PE_sha DB DB_sha NEW_report");
    const std::filesystem::path selectedPath(argv[1]), imagePath(argv[3]),
        databasePath(argv[5]), output(argv[7]);
    const std::array inputPaths{selectedPath, imagePath, databasePath};
    Monstey_RequireOfflineOutput(output, inputPaths);
    Monstey_Require(!std::filesystem::exists(output), "New output required");
    const auto selectedBytes = Monstey_ReadFile(selectedPath),
               image = Monstey_ReadFile(imagePath);
    const auto selectedSha = Monstey_HashSha256(selectedBytes),
               imageSha = Monstey_HashSha256(image),
               databaseSha = Monstey_HashSha256(databasePath);
    Monstey_Require(selectedSha == Monstey_Utf8(argv[2]) &&
                        imageSha == Monstey_Utf8(argv[4]) &&
                        databaseSha == Monstey_Utf8(argv[6]),
                    "Pinned input differs");
    Monstey_Require(imageSha == "6b8db778135c082c934463b6524e6b220478fb6987f082"
                                "ab37ca48a870acb455" ||
                        imageSha == "ed0d95cabb033cd66aaa99d699374881846c3cb03e"
                                    "c345f1123d82d45e0bdaef",
                    "Native source profile is unqualified");
    const auto selected = Monstey_Json::Parse(
        std::string(selectedBytes.begin(), selectedBytes.end()));
    const std::array<Monstey_InspectAddressWord, 4> ptrs{0x14264c660, 0x14264c560, 0x14264c520,
                                  0x14264c5a0};
    const auto declarations =
        Monstey_GadbInspectClassPointers(image, ptrs, imageSha);
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
      Monstey_Require(page.at("rows").size() == 1 &&
                          page.at("rows").at(0).at("reference") == ref,
                      "Source record ordinal differs");
      return records.emplace(s, page.at("rows").at(0)).first->second;
    };
    Monstey_Json::Array rows;
    std::size_t witnesses = 0;
    for (const auto &sourceRow : selected.at("rows").as_array()) {
      const auto &issue = sourceRow.at("original_contract");
      if (issue.at("category") != Monstey_Json("AudioHook.Action"))
        continue;
      const auto fieldName = issue.at("field").as_string();
      if (fieldName != "SoundSwitch" && fieldName != "SoundRTPC" &&
          fieldName != "VariableOperation")
        continue;
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
          Monstey_Json::object({{"record", issue.at("record")},
                                {"field", fieldName},
                                {"source_count", 0},
                                {"declaration_count", 1},
                                {"classification", "unknown_value_contract"},
                                {"native_cache_history_assumed", false}});
      const auto *c = classes.at(0x14264c660);
      const auto nativeIndex =
          static_cast<std::size_t>(issue.at("native_ordinal").as_uint());
      if (c->at("field_storage_status") !=
          Monstey_Json("COMPLETE_FILE_BACKED")) {
        row["consumer_witness"] =
            "DECLARATIONS_UNAVAILABLE_IN_THIS_FILE_PROFILE";
        rows.push_back(std::move(row));
        continue;
      }
      const auto &field = c->at("fields").at(nativeIndex);
      Monstey_Require(
          field.at("count_u32") == Monstey_Json(1) &&
              field.at("name").at("value") == Monstey_Json(fieldName) &&
              field.at("captured_cache_byte_u8").as_uint() == physical,
          "Native declaration mismatch");
      Monstey_Json::Array paths;
      const auto run = [&](Monstey_GadbAudioActionPath path, Monstey_InspectAddressWord nestedPointer,
                           std::size_t nestedIndex) {
        const auto *child = classes.at(nestedPointer);
        Monstey_Require(child->at("field_storage_status") ==
                            Monstey_Json("COMPLETE_FILE_BACKED"),
                        "Nested declaration unavailable");
        const auto &nested = child->at("fields").at(nestedIndex);
        Monstey_Require(
            field.at("captured_binding_state_u32").as_uint() >= 2 &&
                nested.at("captured_binding_state_u32").as_uint() >= 2,
            "This selected witness requires the captured cached-state profile");
        std::vector<std::uint8_t> bytes(0x7000);
        Monstey_GadbAudioActionClasses ids{0x100, 0x180, 0x200, 0x280,
                                           0x141db10e0};
        Monstey_Put(bytes, ids.action, 0x1000, 8);
        Monstey_Put(bytes, ids.set_switch, 0x2000, 8);
        Monstey_Put(bytes, ids.set_rtpc, 0x2000, 8);
        Monstey_Put(bytes, ids.variable_operation, 0x2000, 8);
        Monstey_Put(bytes, 0x300, 0x400, 8);
        Monstey_Raw(bytes, 0x1000 + nativeIndex * 72, field);
        Monstey_Raw(bytes, 0x2000 + nestedIndex * 72, nested);
        Monstey_Put(bytes, 0x408, 0x5000, 8);
        Monstey_Put(bytes, 0x410, UINT64_MAX, 8);
        Monstey_Put(bytes, 0x5000 + physical * 16 + 12, 0, 2);
        const auto before = bytes;
        const auto word =
            Monstey_GadbReadAudioActionWord({0, bytes}, 0x300, path, ids);
        const auto expected = path == Monstey_GadbAudioActionPath::Variable
                                  ? 0
                                  : ids.empty_string;
        Monstey_Require(word == expected && bytes == before,
                        "Owned absence witness differs");
        paths.push_back(Monstey_Json::object(
            {{"getter",
              path == Monstey_GadbAudioActionPath::SwitchName    ? "D1ADC0"
              : path == Monstey_GadbAudioActionPath::SwitchValue ? "D1B3B0"
              : path == Monstey_GadbAudioActionPath::RtpcName    ? "D18ED0"
                                                                 : "D1C030"},
             {"result_is_literal_empty_identity", word == ids.empty_string},
             {"result_is_zero", word == 0},
             {"value_buffer_unavailable_but_not_consumed", true},
             {"borrowed_bytes_unchanged", true}}));
      };
      if (fieldName == "SoundSwitch") {
        run(Monstey_GadbAudioActionPath::SwitchName, 0x14264c560, 0);
        run(Monstey_GadbAudioActionPath::SwitchValue, 0x14264c560, 2);
      } else if (fieldName == "SoundRTPC")
        run(Monstey_GadbAudioActionPath::RtpcName, 0x14264c520, 0);
      else
        run(Monstey_GadbAudioActionPath::Variable, 0x14264c5a0, 0);
      row["consumer_witness"] =
          "OWNED_CACHED_ZERO_COUNT_INSTANCE_WITH_CAPTURED_DECLARATIONS";
      row["paths"] = std::move(paths);
      ++witnesses;
      rows.push_back(std::move(row));
    }
    Monstey_Require(rows.size() == 19, "Expected19 selected audio contracts");
    Monstey_Require(Monstey_HashSha256(selectedPath) == selectedSha &&
                        Monstey_HashSha256(imagePath) == imageSha &&
                        Monstey_HashSha256(databasePath) == databaseSha,
                    "Source changed");
    const auto report = Monstey_Json::object(
        {{"schema", "Monstey.AudioSelectedCountConsumers/1"},
         {"passed", true},
         {"selected_rows", rows.size()},
         {"synthetic_cached_witnesses", witnesses},
         {"source_records_read", records.size()},
         {"selected_json_sha256", selectedSha},
         {"native_image_sha256", imageSha},
         {"database_sha256", databaseSha},
         {"source_unchanged", true},
         {"native_runtime_state_reconstructed", false},
         {"original_code_executed", false},
         {"remaining39_complete", false},
         {"original_unknown120_preserved", true},
         {"rows", std::move(rows)}});
    Monstey_WriteNewText(output, report.dump(2) + "\n");
    std::cout << "PASS19 rows " << witnesses << " owned witnesses\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
