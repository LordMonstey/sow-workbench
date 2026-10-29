#include "Monstey_FxNested.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
J Monstey_Schema() {
  auto f = [](const char *name, unsigned type, J value) {
    return J::object({{"name", name},
                      {"type_code", type},
                      {"array", 1},
                      {"default", value}});
  };
  auto fields = J::array({f("KeyName", 4, ""), f("StartTime", 2, 0.0),
                          f("EndTime", 2, 1.0), f("NumToCreate", 3, 1),
                          f("Loop", 1, false), f("CreateContext", 4, "")});
  for (unsigned i = 0; i < 16; ++i)
    fields.as_array().push_back(
        f(("Effect" + std::to_string(i)).c_str(), 4, ""));
  fields.as_array().push_back(f("UnknownPreserved", 4, "native opaque text"));
  auto links = f("KeyList", 10, nullptr);
  links["array"] = -1;
  links["target"] = "FxKey";
  return J::object(
      {{"reference_aliases", J::object({{"FxKey", "BaseFX"}})},
       {"categories",
        J::object({{"FxGlobalProps",
                    J::object({{"fields",
                                J::array({f("FxName", 4, ""),
                                          f("ResourceLenMS", 3, 0), links})}})},
                   {"BaseFX", J::object({{"fields", J::array()}})},
                   {"ClientFX.Create",
                    J::object({{"parent", "BaseFX"}, {"fields", fields}})}})}});
}
J Monstey_Inspect(Monstey_Gadb &db, const std::string &key) {
  return Monstey_FxNestedCall(db, "fx_nested_inspect",
                              J::object({{"record", key}}));
}
void Monstey_Check(bool ok, const char *message) {
  Monstey_Require(ok, message);
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    const auto out =
        std::filesystem::path(argc > 1 ? argv[1] : L".") /
        (L"Nested_" +
         std::to_wstring(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(out);
    J cases = J::array();
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
      Monstey_Check(failed, "Expected native nested rejection");
    };
    test("native_grammar_strips_ascii_whitespace_and_empty_semicolon_tokens",
         [&] {
           Monstey_Check(Monstey_FxEffectNames(
                             "  fx/First .fxp ;\tfx/Second.fxp;;\r\n") ==
                             J::array({"fx/First.fxp", "fx/Second.fxp"}),
                         "Native tokenizer mismatch");
         });
    test("commas_are_literal_effect_name_bytes_and_duplicates_survive", [&] {
      Monstey_Check(Monstey_FxEffectNames("A,B;A;A") ==
                        J::array({"A,B", "A", "A"}),
                    "Tokenizer changed native grouping");
    });
    test("native_c_string_terminates_at_first_nul", [&] {
      Monstey_Check(Monstey_FxEffectNames(std::string("A\0;B", 5)) ==
                        J::array({"A"}),
                    "Embedded native terminator ignored");
    });
    test("native_259_byte_limit_accepts_and_260_rejects", [&] {
      Monstey_Check(
          Monstey_FxEffectNames(std::string(259, 'x'))[0].as_string().size() ==
              259,
          "Token bound too small");
      reject([&] { Monstey_FxEffectNames(std::string(260, 'x')); });
    });
    const auto schema = out / L"schema.json";
    Monstey_WriteNewText(schema, Monstey_Schema().dump(2));
    auto db = Monstey_Gadb::NewFx(Monstey_FxDefaultPrefix(), schema);
    Monstey_FxWorkflowCall(
        *db, "fx_create_effect",
        J::object({{"name", "fx/native/nested"}, {"duration_ms", 3000}}));
    const auto key =
        Monstey_FxWorkflowCall(
            *db, "fx_create_element",
            J::object({{"class_name", "ClientFX.Create"},
                       {"name", "Spawner"}}))["result"]["reference"]
            .as_string();
    test("independent_native_create_has_sixteen_disabled_slots", [&] {
      auto v = Monstey_Inspect(*db, key);
      Monstey_Check(v["slots"].size() == 16 &&
                        v["nonempty_groups"].as_uint() == 0 &&
                        v["native_num_to_create"].as_uint() == 1,
                    "New Create shape invalid");
    });
    test("grouped_slot_set_preserves_case_slashes_order_and_duplicates", [&] {
      Monstey_FxNestedCall(
          *db, "fx_nested_set_slot",
          J::object(
              {{"record", key},
               {"slot", 15},
               {"effects", J::array({"FX/A.fxp", "fx\\B.fxp", "FX/A.fxp"})}}));
      auto v = Monstey_Inspect(*db, key);
      Monstey_Check(v["slots"][15]["effects"] ==
                        J::array({"FX/A.fxp", "fx\\B.fxp", "FX/A.fxp"}),
                    "Slot authoring normalized native names");
    });
    test("nonempty_slots_are_random_alternatives_with_all_members", [&] {
      Monstey_FxNestedCall(*db, "fx_nested_set_slot",
                           J::object({{"record", key},
                                      {"slot", 0},
                                      {"effects", J::array({"fx/C.fxp"})}}));
      auto v = Monstey_Inspect(*db, key);
      Monstey_Check(v["nonempty_groups"].as_uint() == 2 &&
                        v["effect_references"].as_uint() == 4 &&
                        v["slots"][15]["nominal_selection_share"].as_number() ==
                            0.5,
                    "Native alternatives were flattened");
    });
    test("empty_list_disables_only_selected_slot", [&] {
      Monstey_FxNestedCall(
          *db, "fx_nested_set_slot",
          J::object({{"record", key}, {"slot", 0}, {"effects", J::array()}}));
      Monstey_Check(Monstey_Inspect(*db, key)["nonempty_groups"].as_uint() == 1,
                    "Disable changed other slots");
    });
    test("count_and_loop_are_one_undo_transaction", [&] {
      auto before = Monstey_Inspect(*db, key);
      Monstey_FxNestedCall(
          *db, "fx_nested_set_options",
          J::object({{"record", key}, {"num_to_create", 255}, {"loop", true}}));
      Monstey_Check(
          Monstey_Inspect(*db, key)["native_num_to_create"].as_uint() == 255,
          "Count setter mismatch");
      db->Undo();
      Monstey_Check(Monstey_Inspect(*db, key) == before,
                    "Options split history");
      db->Redo();
      Monstey_Check(Monstey_Inspect(*db, key)["loop"].as_bool(),
                    "Options redo failed");
    });
    test("out_of_byte_range_count_and_invalid_loop_roll_back_atomically", [&] {
      auto before = db->Changes();
      reject([&] {
        Monstey_FxNestedCall(
            *db, "fx_nested_set_options",
            J::object({{"record", key}, {"num_to_create", 256}}));
      });
      reject([&] {
        Monstey_FxNestedCall(
            *db, "fx_nested_set_options",
            J::object({{"record", key}, {"num_to_create", 5}, {"loop", 1}}));
      });
      Monstey_Check(db->Changes() == before, "Bad option partially committed");
    });
    test("effect_names_reject_native_whitespace_delimiter_nul_overflow_without_"
         "mutation",
         [&] {
           auto before = db->Changes();
           for (const auto &name : std::vector<std::string>{
                    "", "A B", "A;B", std::string("A\0B", 3),
                    std::string(260, 'x')})
             reject([&] {
               Monstey_FxNestedCall(*db, "fx_nested_set_slot",
                                    J::object({{"record", key},
                                               {"slot", 2},
                                               {"effects", J::array({name})}}));
             });
           Monstey_Check(db->Changes() == before,
                         "Invalid path mutated source");
         });
    test("invalid_slot_unknown_argument_class_and_stale_hash_reject", [&] {
      reject([&] {
        Monstey_FxNestedCall(
            *db, "fx_nested_set_slot",
            J::object(
                {{"record", key}, {"slot", 16}, {"effects", J::array()}}));
      });
      reject([&] {
        Monstey_FxNestedCall(*db, "fx_nested_inspect",
                             J::object({{"record", key}, {"unexpected", 0}}));
      });
      reject([&] {
        Monstey_FxNestedCall(
            *db, "fx_nested_inspect",
            J::object({{"record", key},
                       {"expected_schema_sha256", std::string(64, '0')}}));
      });
      auto global = Monstey_FxWorkflowCall(*db, "fx_workflow")["global_record"]
                        .as_string();
      reject([&] { Monstey_Inspect(*db, global); });
    });
    test("raw_native_low_byte_count_and_empty_group_hazards_are_reported", [&] {
      db->SetField(key, "NumToCreate", 257);
      db->SetField(key, "Effect0", " ; \t ");
      auto v = Monstey_Inspect(*db, key);
      Monstey_Check(v["native_num_to_create"].as_uint() == 1 &&
                        v["nonempty_groups"].as_uint() == 2 &&
                        v["warnings"].size() >= 2,
                    "Native truncation/empty group hidden");
      db->Undo();
      db->Undo();
    });
    test("raw_overlong_token_is_reported_without_crashing_reader", [&] {
      db->SetField(key, "Effect0", std::string(260, 'x'));
      auto v = Monstey_Inspect(*db, key);
      Monstey_Check(!v["slots"][0]["valid"].as_bool(),
                    "Unsafe native token accepted");
      db->Undo();
    });
    test("prefab_typed_set_propagates_and_instance_override_is_preserved", [&] {
      const auto prefab = Monstey_FxLibraryCall(
          *db, "fx_prefab_from_key",
          J::object({{"record", key}, {"name", "Spawn template"}}))["result"];
      const auto child = Monstey_FxLibraryCall(
                             *db, "fx_prefab_add_child",
                             J::object({{"prefab", prefab["prefab"]},
                                        {"time", 2.0},
                                        {"name", "Child"}}))["result"]["record"]
                             .as_string();
      Monstey_FxNestedCall(*db, "fx_nested_set_options",
                           J::object({{"record", prefab["record"]},
                                      {"num_to_create", 7},
                                      {"loop", false}}));
      Monstey_Check(
          Monstey_Inspect(*db, child)["native_num_to_create"].as_uint() == 7,
          "Template did not propagate");
      Monstey_FxNestedCall(
          *db, "fx_nested_set_options",
          J::object({{"record", child}, {"num_to_create", 9}}));
      Monstey_FxNestedCall(
          *db, "fx_nested_set_options",
          J::object({{"record", prefab["record"]}, {"num_to_create", 8}}));
      Monstey_Check(
          Monstey_Inspect(*db, child)["native_num_to_create"].as_uint() == 9,
          "Local override lost");
    });
    test("project_replay_and_native_compiled_readback_are_byte_exact", [&] {
      const auto a = out / L"synthetic.fxp", p = out / L"project.json",
                 b = out / L"synthetic-replay.fxp";
      db->Build(a);
      db->SaveProject(p);
      auto replay = Monstey_Gadb::LoadProject(p);
      replay->Build(b);
      Monstey_Check(Monstey_ReadFile(a) == Monstey_ReadFile(b),
                    "Nested project changed native bytes");
      Monstey_Gadb read(a, schema);
      Monstey_Check(read.FxRecords()["rows"].size() >= 3,
                    "Compiled nested records lost");
    });
    test("same_size_same_timestamp_source_mutation_rejects_read_and_edit", [&] {
      const auto file = out / L"stale-source.fxp";
      db->Build(file);
      Monstey_Gadb stale(file, schema);
      const auto timestamp = std::filesystem::last_write_time(file);
      auto bytes = Monstey_ReadFile(file);
      bytes.back() ^= 1;
      {
        std::ofstream changed(file, std::ios::binary | std::ios::trunc);
        changed.write(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()));
        Monstey_Check(changed.good(), "Could not mutate owned stale fixture");
      }
      std::filesystem::last_write_time(file, timestamp);
      const auto categories = stale.Categories();
      std::string selected;
      for (const auto &category : categories["rows"].as_array())
        if (category["schema"].as_string() == "ClientFX.Create") {
          const auto records = stale.Records(
              static_cast<std::uint32_t>(category["index"].as_uint()));
          selected = records["rows"][0]["native_reference"].as_string();
        }
      const auto before = stale.Changes();
      reject([&] { Monstey_Inspect(stale, selected); });
      reject([&] {
        Monstey_FxNestedCall(
            stale, "fx_nested_set_options",
            J::object({{"record", selected}, {"num_to_create", 3}}));
      });
      Monstey_Check(stale.Changes() == before,
                    "Stale source committed an edit");
    });
    J native = nullptr;
    if (argc > 3) {
      const std::filesystem::path source(argv[2]), nativeSchema(argv[3]);
      Monstey_Gadb real(source, nativeSchema);
      const auto hash = Monstey_HashSha256(source);
      std::string create;
      const auto nativeCategories = real.Categories();
      for (const auto &c : nativeCategories["rows"].as_array())
        if (c["schema"].as_string() == "ClientFX.Create") {
          auto records =
              real.Records(static_cast<std::uint32_t>(c["index"].as_uint()));
          if (!records["rows"].empty())
            create = records["rows"][0]["native_reference"].as_string();
        }
      Monstey_Check(!create.empty(), "Native fixture has no Create component");
      test("actual_native_create_inspection_retains_sixteen_slots_and_consumer_"
           "report",
           [&] {
             native = Monstey_Inspect(real, create);
             Monstey_Check(native["slots"].size() == 16,
                           "Native slot count mismatch");
           });
      test("actual_native_clone_edit_project_replay_preserves_original_and_"
           "zero_warnings",
           [&] {
             const auto copied =
                 real.CloneRecord(
                         create,
                         "Monstey_NestedNativeClone")["clone"]["reference"]
                     .as_string();
             Monstey_FxNestedCall(
                 real, "fx_nested_set_slot",
                 J::object(
                     {{"record", copied},
                      {"slot", 15},
                      {"effects", J::array({"fx/one.fxp", "fx/two.fxp"})}}));
             Monstey_FxNestedCall(real, "fx_nested_set_options",
                                  J::object({{"record", copied},
                                             {"num_to_create", 200},
                                             {"loop", true}}));
             const auto a = out / L"native.fxp",
                        p = out / L"native-project.json",
                        b = out / L"native-replay.fxp";
             const auto report = real.Build(a);
             real.SaveProject(p);
             auto replay = Monstey_Gadb::LoadProject(p);
             replay->Build(b);
             Monstey_Check(report["warnings"].empty() &&
                               Monstey_ReadFile(a) == Monstey_ReadFile(b) &&
                               Monstey_HashSha256(source) == hash,
                           "Native nested replay/warnings/source failed");
             native["source_sha256"] = hash;
             native["built_sha256"] = Monstey_HashSha256(a);
           });
    }
    J corpus = nullptr;
    if (argc > 4) {
      test("actual_corpus_all_create_components_follow_native_group_grammar",
           [&] {
             std::size_t files = 0, components = 0, effectNames = 0,
                         warningCount = 0;
             std::set<std::string> hashes;
             for (const auto &entry :
                  std::filesystem::recursive_directory_iterator(argv[4])) {
               if (!entry.is_regular_file() ||
                   entry.path().extension() != L".fxp")
                 continue;
               const auto hash = Monstey_HashSha256(entry.path());
               Monstey_Gadb database(entry.path(),
                                     std::filesystem::path(argv[3]));
               const auto categories = database.Categories();
               for (const auto &category : categories["rows"].as_array()) {
                 if (category["schema"].as_string() != "ClientFX.Create")
                   continue;
                 const auto records = database.Records(
                     static_cast<std::uint32_t>(category["index"].as_uint()),
                     {}, 0, 10000);
                 Monstey_Check(records["rows"].size() ==
                                   records["total"].as_uint(),
                               "Native Create corpus page is incomplete");
                 for (const auto &record : records["rows"].as_array()) {
                   const auto view = Monstey_Inspect(
                       database, record["native_reference"].as_string());
                   for (const auto &slot : view["slots"].as_array())
                     Monstey_Check(
                         slot["valid"].as_bool(),
                         "Native corpus contains an unsafe effect token");
                   effectNames += static_cast<std::size_t>(
                       view["effect_references"].as_uint());
                   warningCount += view["warnings"].size();
                   ++components;
                 }
               }
               Monstey_Check(Monstey_HashSha256(entry.path()) == hash,
                             "Native corpus source changed");
               hashes.insert(hash);
               ++files;
             }
             Monstey_Check(files > 0 && components > 0,
                           "Native corpus has no qualified Create components");
             corpus = J::object({{"files", files},
                                 {"unique_hashes", hashes.size()},
                                 {"create_components", components},
                                 {"effect_names", effectNames},
                                 {"warnings", warningCount},
                                 {"all_sources_unchanged", true}});
           });
    }
    const auto result = J::object({{"status", "PASS"},
                                   {"groups", cases.size()},
                                   {"cases", cases},
                                   {"native", native},
                                   {"corpus", corpus},
                                   {"python_product_runtime", false},
                                   {"game_written", false},
                                   {"in_game_verified", false}});
    Monstey_WriteNewText(out / L"nested-tests.json", result.dump(2));
    std::cout << result.dump() << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
