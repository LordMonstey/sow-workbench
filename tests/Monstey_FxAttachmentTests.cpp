#include "Monstey_FxAttachment.hpp"
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
using namespace Monstey;
namespace {
using J = Monstey_Json;
void Monstey_Check(bool ok, const char *message) {
  Monstey_Require(ok, message);
}
J Monstey_Schema() {
  auto f = [](const char *name, unsigned type, J value) {
    return J::object({{"name", name},
                      {"type_code", type},
                      {"array", 1},
                      {"default", value}});
  };
  auto links = f("KeyList", 10, nullptr);
  links["array"] = -1;
  links["target"] = "FxKey";
  return J::object(
      {{"reference_aliases", J::object({{"FxKey", "BaseFX"}})},
       {"categories",
        J::object(
            {{"FxGlobalProps",
              J::object(
                  {{"fields", J::array({f("FxName", 4, ""),
                                        f("ResourceLenMS", 3, 0), links})}})},
             {"BaseFX",
              J::object(
                  {{"fields",
                    J::array(
                        {f("KeyName", 4, ""), f("StartTime", 2, 0.0),
                         f("EndTime", 2, 1.0),
                         f("UpdatePos", 5, "Follow"), f("AttachName", 4, ""),
                         f("Offset", 16,
                           J::object(
                               {{"constant", J::array({1.5, -2.0, 7.25})}})),
                         f("UnknownPreserved", 4, "native opaque text")})}})},
             {"ClientFX.Test",
              J::object({{"parent", "BaseFX"}, {"fields", J::array()}})},
             {"Other",
              J::object({{"fields", J::array({f("UpdatePos", 5, "Follow"),
                                              f("AttachName", 4, "")})}})}})}});
}
std::pair<std::unique_ptr<Monstey_Gadb>, std::string>
Monstey_Fixture(const J &schema = Monstey_Schema()) {
  auto flattened = schema;
  flattened["categories"]["ClientFX.Test"]["fields"] =
      schema["categories"]["BaseFX"]["fields"];
  auto db = Monstey_Gadb::NewFx(Monstey_FxDefaultPrefix(), flattened);
  Monstey_FxWorkflowCall(
      *db, "fx_create_effect",
      J::object({{"name", "fx/native/attachment"}, {"duration_ms", 3000}}));
  auto key = Monstey_FxWorkflowCall(
                 *db, "fx_create_element",
                 J::object({{"class_name", "ClientFX.Test"},
                            {"name", "Attached"}}))["result"]["reference"]
                 .as_string();
  return {std::move(db), key};
}
J Monstey_Inspect(Monstey_Gadb &db, const std::string &key) {
  return Monstey_FxAttachmentCall(db, "fx_attachment_inspect",
                                  J::object({{"record", key}}));
}
J Monstey_Set(Monstey_Gadb &db, const std::string &key, const J &mode,
              const J &name) {
  return Monstey_FxAttachmentCall(db, "fx_attachment_set",
                                  J::object({{"record", key},
                                             {"update_pos", mode},
                                             {"attach_name", name}}))["result"];
}
J Monstey_Layout(const J &record) {
  auto out = J::array();
  for (const auto &field : record["fields"].as_array())
    out.as_array().push_back(
        J::array({field["hash"], field["native_type"], field["native_ordinal"],
                  field["physical_index"], field["array"], field["count"]}));
  return out;
}
J Monstey_Value(const J &record, const char *name) {
  for (const auto &field : record["fields"].as_array())
    if (field["hash"].as_string() == Monstey_Hex(Monstey_NativeHash(name)))
      return field["value"];
  throw Monstey_Error("test", "Missing test field");
}
void Monstey_Overwrite(const std::filesystem::path &path,
                       const Monstey_Bytes &data) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  Monstey_Check(file.is_open(), "Cannot open test-owned file");
  file.write(reinterpret_cast<const char *>(data.data()),
             static_cast<std::streamsize>(data.size()));
  Monstey_Check(file.good(), "Cannot write test-owned file");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    const auto out =
        std::filesystem::path(argc > 1 ? argv[1] : L".") /
        (L"Attachment_" +
         std::to_wstring(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(out);
    J cases = J::array(), proof = J::object();
    auto test = [&](const char *name, auto action) {
      action();
      cases.as_array().push_back(name);
      std::cout << name << std::endl;
    };
    auto reject = [](auto action) {
      bool failed = false;
      try {
        action();
      } catch (const Monstey_Error &) {
        failed = true;
      }
      Monstey_Check(failed, "Expected native attachment rejection");
    };
    const auto schemaFile = out / L"schema.json";
    auto diskSchema = Monstey_Schema();
    diskSchema["categories"]["ClientFX.Test"]["fields"] =
        diskSchema["categories"]["BaseFX"]["fields"];
    Monstey_WriteNewText(schemaFile, diskSchema.dump(2));
    auto [db, key] = Monstey_Fixture();
    test("catalog_two_operations_and_native_eight_enum_tokens", [&] {
      auto cat = Monstey_FxAttachmentCatalog();
      Monstey_Check(
          cat.size() == 2 &&
              cat[1]["inputSchema"]["properties"]["update_pos"]["enum"]
                      .size() == 8,
          "Catalog shape mismatch");
    });
    test("all_eight_modes_exact_bits_and_distinct_node_socket_labels", [&] {
      const std::array<unsigned, 8> flags{0, 8,    0x28,  0x28,
                                          9, 0x29, 0x108, 0x128};
      const auto modes = Monstey_Inspect(*db, key)["modes"];
      for (std::size_t i = 0; i < modes.size(); ++i) {
        auto v = Monstey_Set(*db, key, modes[i]["token"], "Weapon_Tip");
        if (i == 0)
          Monstey_Check(v["transform"]["position"] ==
                            J("captured_frame_plus_rotated_offset"),
                        "Fixed must retain animated local offset");
        Monstey_Check(v["native_follow_flags"].as_uint() == flags[i] &&
                          v["native_mode_index"].as_uint() == i &&
                          v["effective_update_pos"] == modes[i]["token"] &&
                          v["named_lookup_enabled_by_mode"].as_bool() ==
                              ((flags[i] & 0x20u) != 0),
                      "Native follow mode mismatch");
      }
    });
    test("native_enum_reader_uses_case_folded_hash_without_normalizing_source",
         [&] {
           db->SetField(key, "UpdatePos", "nOdEaTtAcH");
           auto v = Monstey_Inspect(*db, key);
           Monstey_Check(v["update_pos"].as_string() == "nOdEaTtAcH" &&
                             v["effective_update_pos"].as_string() ==
                                 "NodeAttach" &&
                             v["update_pos_native_match"].as_bool(),
                         "Native enum hashing mismatch");
           reject([&] { Monstey_Set(*db, key, "nodeattach", "Head"); });
         });
    test("unmatched_native_enum_uses_follow_default_without_mutation", [&] {
      db->SetField(key, "UpdatePos", "native unknown token");
      const auto before = db->Changes();
      auto v = Monstey_Inspect(*db, key);
      Monstey_Check(v["effective_update_pos"].as_string() == "Follow" &&
                        v["native_mode_index"].as_uint() == 1 &&
                        v["native_follow_flags"].as_uint() == 8 &&
                        v["update_pos_uses_native_default"].as_bool() &&
                        db->Changes() == before,
                    "Native enum default mismatch");
    });
    test("empty_name_falls_back_and_literal_null_is_hashed_normally", [&] {
      auto empty = Monstey_Set(*db, key, "NodeAttach", "");
      auto literal = Monstey_Set(*db, key, "NodeAttach", "null");
      Monstey_Check(
          empty["empty_or_zero_name_uses_context_frame"].as_bool() &&
              empty["attach_name_native_hash"].as_string() == "0x00000000" &&
              !literal["empty_or_zero_name_uses_context_frame"].as_bool() &&
              literal["attach_name_native_hash"].as_string() ==
                  Monstey_Hex(Monstey_NativeHash("null", 0)),
          "Native empty/literal name mismatch");
    });
    test("nonempty_native_hash_zero_name_uses_fallback_without_ascii_"
         "restriction",
         [&] {
           const std::string zeroName = "\xc3\xa9";
           Monstey_Check(Monstey_NativeHash(zeroName) == 0,
                         "Synthetic hash-zero name changed");
           const auto v = Monstey_Set(*db, key, "NodeAttach", zeroName);
           Monstey_Check(
               v["attach_name"].as_string() == zeroName &&
                   !v["attach_name_is_empty"].as_bool() &&
                   v["attach_name_hash_is_zero"].as_bool() &&
                   v["empty_or_zero_name_uses_context_frame"].as_bool() &&
                   !v["name_lookup_has_nonzero_token"].as_bool(),
               "Nonempty hash-zero fallback was lost");
         });
    test("null_input_explicitly_normalizes_to_empty_text_with_result_flag",
         [&] {
           auto v = Monstey_Set(*db, key, "SocketAttach", nullptr);
           Monstey_Check(v["attach_name"].is_string() &&
                             v["attach_name"].as_string().empty() &&
                             v["attach_name_null_normalized"].as_bool(),
                         "Null normalization was hidden");
         });
    test("name_bytes_preserve_case_slashes_spaces_unicode_and_more_than_259_"
         "bytes",
         [&] {
           const std::string name =
               std::string(300, 'x') + " /Head\\Node \xc3\xa9";
           auto v = Monstey_Set(*db, key, "NodeAttach", name);
           Monstey_Check(v["attach_name"].as_string() == name &&
                             v["attach_name_native_hash"].as_string() ==
                                 Monstey_Hex(Monstey_NativeHash(name)),
                         "Name was path-normalized/truncated");
           Monstey_Check(Monstey_NativeHash("A/B") !=
                             Monstey_NativeHash("A\\B"),
                         "Test hash is not native");
         });
    test("choosing_unnamed_mode_preserves_dormant_name", [&] {
      Monstey_Set(*db, key, "NodeAttach", "Weapon_Tip");
      auto v = Monstey_FxAttachmentCall(
          *db, "fx_attachment_set",
          J::object({{"record", key}, {"update_pos", "Follow"}}))["result"];
      Monstey_Check(v["attach_name"].as_string() == "Weapon_Tip" &&
                        v["name_is_dormant"].as_bool(),
                    "Dormant name was cleared");
    });
    test("two_fields_one_history_step_and_layout_curves_unrelated_preserved",
         [&] {
           auto before = db->Metadata(key),
                beforeInspect = Monstey_Inspect(*db, key);
           Monstey_Set(*db, key, "NodeAttachPos", "Head");
           auto after = db->Metadata(key);
           Monstey_Check(Monstey_Layout(before) == Monstey_Layout(after) &&
                             Monstey_Value(before, "Offset") ==
                                 Monstey_Value(after, "Offset") &&
                             Monstey_Value(before, "UnknownPreserved") ==
                                 Monstey_Value(after, "UnknownPreserved"),
                         "Physical layout or unrelated values changed");
           db->Undo();
           Monstey_Check(Monstey_Inspect(*db, key) == beforeInspect,
                         "Two-field write split history");
           db->Redo();
           Monstey_Check(
               Monstey_Inspect(*db, key)["effective_update_pos"].as_string() ==
                   "NodeAttachPos",
               "Attachment redo failed");
         });
    test("late_name_nul_utf8_failure_rolls_back_earlier_mode_and_history", [&] {
      const auto before = db->Changes(), metadata = db->Metadata(key),
                 project = db->ProjectMetadata();
      for (const auto &invalid :
           {std::string("A\0B", 3), std::string("\xff", 1)})
        reject([&] { Monstey_Set(*db, key, "Fixed", invalid); });
      Monstey_Check(db->Changes() == before && db->Metadata(key) == metadata &&
                        db->ProjectMetadata() == project,
                    "Rejected batch partially mutated");
    });
    test("empty_batch_unknown_argument_and_stale_hashes_reject", [&] {
      const auto before = db->Changes();
      reject([&] {
        Monstey_FxAttachmentCall(*db, "fx_attachment_set",
                                 J::object({{"record", key}}));
      });
      reject([&] {
        Monstey_FxAttachmentCall(*db, "fx_attachment_inspect",
                                 J::object({{"record", key}, {"field", "x"}}));
      });
      for (const auto *field :
           {"expected_database_sha256", "expected_schema_sha256"})
        reject([&] {
          Monstey_FxAttachmentCall(*db, "fx_attachment_set",
                                   J::object({{"record", key},
                                              {"update_pos", "Fixed"},
                                              {field, std::string(64, '0')}}));
        });
      Monstey_Check(db->Changes() == before,
                    "Invalid request mutated database");
    });
    test("valid_hash_guards_accept_current_snapshot", [&] {
      auto desc = db->Describe();
      auto v = Monstey_FxAttachmentCall(
          *db, "fx_attachment_inspect",
          J::object({{"record", key},
                     {"expected_database_sha256", desc["source_sha256"]},
                     {"expected_schema_sha256", desc["schema_sha256"]}}));
      Monstey_Check(v["source_sha256"] == desc["source_sha256"],
                    "Guarded source lost provenance");
    });
    test("ordinary_gadb_and_non_base_class_reject", [&] {
      auto normal = Monstey_Gadb::New(schemaFile);
      reject([&] {
        Monstey_FxAttachmentCall(*normal, "fx_attachment_inspect",
                                 J::object({{"record", key}}));
      });
      const auto created = db->CreateCategory("Other");
      const auto category = created["category"]["index"].as_uint();
      auto other = db->CreateRecord(static_cast<std::uint32_t>(category),
                                    "NotBase")["record"]["reference"]
                       .as_string();
      reject([&] { Monstey_Inspect(*db, other); });
    });
    test("missing_native_field_wrong_type_and_array_shape_reject", [&] {
      {
        auto [bad, ref] = Monstey_Fixture();
        bad->DeleteField(ref, "AttachName");
        reject([&] { Monstey_Inspect(*bad, ref); });
      }
      {
        auto schema = Monstey_Schema();
        schema["categories"]["BaseFX"]["fields"][3]["type_code"] = 4;
        auto [bad, ref] = Monstey_Fixture(schema);
        reject([&] { Monstey_Inspect(*bad, ref); });
      }
      {
        auto schema = Monstey_Schema();
        auto &field = schema["categories"]["BaseFX"]["fields"][4];
        field["array"] = 2;
        field["default"] = J::array({"A", "B"});
        auto [bad, ref] = Monstey_Fixture(schema);
        reject([&] { Monstey_Inspect(*bad, ref); });
      }
    });
    test("prefab_propagation_and_instance_local_override_are_preserved", [&] {
      auto prefab = Monstey_FxLibraryCall(
          *db, "fx_prefab_from_key",
          J::object({{"record", key}, {"name", "Owner template"}}))["result"];
      auto child = Monstey_FxLibraryCall(
                       *db, "fx_prefab_add_child",
                       J::object({{"prefab", prefab["prefab"]},
                                  {"time", 2.0},
                                  {"name", "Child"}}))["result"]["record"]
                       .as_string();
      auto parent = prefab["record"].as_string();
      Monstey_Set(*db, parent, "SocketAttach", "Head");
      Monstey_Check(
          Monstey_Inspect(*db, child)["attach_name"].as_string() == "Head" &&
              Monstey_Inspect(*db, child)["effective_update_pos"].as_string() ==
                  "SocketAttach",
          "Prefab attachment did not propagate");
      Monstey_FxAttachmentCall(
          *db, "fx_attachment_set",
          J::object({{"record", child}, {"attach_name", "Torso"}}));
      Monstey_Set(*db, parent, "NodeAttachPos", "RightHand");
      Monstey_Check(
          Monstey_Inspect(*db, child)["attach_name"].as_string() == "Torso" &&
              Monstey_Inspect(*db, child)["effective_update_pos"].as_string() ==
                  "NodeAttachPos",
          "Prefab local override lost");
      const auto before = db->Changes(), meta = db->ProjectMetadata(),
                 childBefore = db->Metadata(child);
      reject(
          [&] { Monstey_Set(*db, parent, "Fixed", std::string("A\0B", 3)); });
      Monstey_Check(db->Changes() == before && db->ProjectMetadata() == meta &&
                        db->Metadata(child) == childBefore,
                    "Prefab failure leaked changes");
    });
    test("project_native_build_replay_and_reopen_are_byte_exact", [&] {
      const auto a = out / L"synthetic.fxp", project = out / L"project.json",
                 b = out / L"replay.fxp";
      db->Build(a);
      db->SaveProject(project);
      auto replay = Monstey_Gadb::LoadProject(project);
      replay->Build(b);
      Monstey_Check(Monstey_ReadFile(a) == Monstey_ReadFile(b),
                    "Attachment project changed native bytes");
      Monstey_Gadb reopened(a, schemaFile);
      Monstey_Check(Monstey_Inspect(reopened, key)["attach_name"] ==
                        Monstey_Inspect(*db, key)["attach_name"],
                    "Native attachment readback mismatch");
    });
    test("same_size_restored_timestamp_source_tamper_rejects_without_mutation",
         [&] {
           const auto path = out / L"source-guard.fxp";
           db->Build(path);
           Monstey_Gadb guarded(path, schemaFile);
           const auto before = guarded.Changes();
           const auto stamp = std::filesystem::last_write_time(path);
           auto bytes = Monstey_ReadFile(path);
           bytes.back() ^= 1;
           Monstey_Overwrite(path, bytes);
           std::filesystem::last_write_time(path, stamp);
           reject([&] {
             Monstey_FxAttachmentCall(
                 guarded, "fx_attachment_set",
                 J::object({{"record", key}, {"update_pos", "Fixed"}}));
           });
           Monstey_Check(guarded.Changes() == before,
                         "Source tamper mutated project");
         });
    if (argc > 3) {
      const auto nativeSchema = std::filesystem::path(argv[2]),
                 corpusPath = std::filesystem::path(argv[3]);
      const auto corpus = J::Parse(Monstey_ReadText(corpusPath));
      std::map<std::string, std::vector<J>> grouped;
      for (const auto &row : corpus["rows"].as_array())
        grouped[row["path"].as_string()].push_back(row);
      std::map<std::string, std::string> sourceHashes;
      for (const auto &entry : corpus["sources"].as_array()) {
        const auto path = entry["path"].as_string();
        sourceHashes[path] = entry["sha256"].as_string();
        grouped.try_emplace(path);
      }
      auto modes = J::object();
      std::size_t count = 0;
      test("actual_326_files_2512_records_source_hashes_modes_and_names_match",
           [&] {
             for (const auto &[path, rows] : grouped) {
               const auto source = std::filesystem::path(Monstey_Wide(path));
               const auto hash = Monstey_HashSha256(source);
               Monstey_Check(hash == sourceHashes.at(path),
                             "Corpus source changed");
               Monstey_Gadb actual(source, nativeSchema);
               for (const auto &row : rows) {
                 const auto v =
                     Monstey_Inspect(actual, row["record"].as_string());
                 Monstey_Check(v["update_pos"] == row["update_pos"] &&
                                   v["attach_name"] == row["attach_name"] &&
                                   v["update_pos_native_match"].as_bool() &&
                                   !v["external_owner_resolved"].as_bool() &&
                                   !v["in_game_verified"].as_bool(),
                               "Native corpus semantic mismatch");
                 const auto mode = v["effective_update_pos"].as_string();
                 modes[mode] =
                     modes.contains(mode) ? modes[mode].as_uint() + 1 : 1u;
                 ++count;
               }
               Monstey_Check(Monstey_HashSha256(source) == hash &&
                                 !actual.Describe()["dirty"].as_bool(),
                             "Native source changed during inspect");
             }
             Monstey_Check(grouped.size() == 326 && count == 2512 &&
                               modes == corpus["modes"],
                           "Native corpus coverage incomplete");
           });
      proof["native_corpus"] =
          J::object({{"files", static_cast<std::uint64_t>(grouped.size())},
                     {"records", static_cast<std::uint64_t>(count)},
                     {"modes", modes},
                     {"all_source_sha256_unchanged", true},
                     {"corpus_sha256", Monstey_HashSha256(corpusPath)}});
      const auto &first = corpus["rows"][0];
      test("actual_edit_preserves_all_other_fields_and_source_native_build_"
           "project",
           [&] {
             const auto source =
                 std::filesystem::path(Monstey_Wide(first["path"].as_string()));
             Monstey_Gadb actual(source, nativeSchema);
             const auto ref = first["record"].as_string(),
                        hash = Monstey_HashSha256(source);
             const auto before = actual.Metadata(ref);
             Monstey_Set(actual, ref, "NodeAttachWSOffset", "Head");
             const auto after = actual.Metadata(ref);
             Monstey_Check(Monstey_Layout(before) == Monstey_Layout(after),
                           "Actual layout changed");
             for (std::size_t i = 0; i < before["fields"].size(); ++i)
               if (before["fields"][i]["hash"].as_string() != "0x23e7c5b3" &&
                   before["fields"][i]["hash"].as_string() != "0x1fcd6c38")
                 Monstey_Check(before["fields"][i]["value"] ==
                                   after["fields"][i]["value"],
                               "Actual unrelated field changed");
             const auto a = out / L"actual-edited.fxp",
                        p = out / L"actual-project.json",
                        b = out / L"actual-replay.fxp";
             actual.Build(a);
             actual.SaveProject(p);
             auto replay = Monstey_Gadb::LoadProject(p);
             replay->Build(b);
             Monstey_Check(Monstey_ReadFile(a) == Monstey_ReadFile(b) &&
                               Monstey_HashSha256(source) == hash,
                           "Actual project/native bytes diverged");
             Monstey_Gadb reopened(a, nativeSchema);
             Monstey_Check(
                 Monstey_Inspect(reopened, ref)["attach_name"].as_string() ==
                     "Head",
                 "Actual attachment build readback failed");
             proof["actual_edit_source_sha256"] = hash;
           });
    }
    proof["schema"] = "Monstey.NativeFxOwnerApiQualification/1";
    proof["passed_groups"] = static_cast<std::uint64_t>(cases.size());
    proof["cases"] = cases;
    proof["originals_executed"] = false;
    proof["in_game_verified"] = false;
    proof["schema_modified"] = false;
    proof["game_modified"] = false;
    Monstey_WriteNewText(out / L"fx-attachment-tests.json", proof.dump(2));
    std::cout << "PASS " << cases.size()
              << " groups: " << Monstey_Utf8(out.wstring()) << std::endl;
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << std::endl;
    return 1;
  }
}
