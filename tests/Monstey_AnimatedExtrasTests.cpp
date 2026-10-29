#include "Monstey_AnimatedExtras.hpp"
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
J Monstey_TestSchema() {
  auto field = [](const char *name, unsigned type, J value, const char *target = "",
                  int array = 1) {
    auto f = J::object({{"name", name}, {"type_code", type}, {"array", array}, {"default", value}});
    if (*target)
      f["target"] = target;
    return f;
  };
  return J::object(
      {{"categories",
        J::object(
            {{"Character.Models",
              J::object({{"fields", J::array({field("AttachedAnimExtras", 10, nullptr,
                                                    "AnimatedExtra.Object", -1),
                                              field("ModelRecord", 10, nullptr, "Model")})}})},
             {"Model",
              J::object({{"fields", J::array({field("ModelFile", 4, "characters/test.skel")})}})},
             {"Client.ClientFXSequence", J::object({{"fields", J::array()}})},
             {"BaseTypes.FloatRange", J::object({{"fields", J::array()}})},
             {"AnimatedExtra.Object",
              J::object(
                  {{"fields",
                    J::array({field("Model", 10, nullptr, "Model"),
                              field("BaseAnimation", 4, nullptr),
                              field("OwnerAttachNode", 4, nullptr), field("StartAttached", 1, true),
                              field("StartVisible", 1, true),
                              field("PersistentFX", 10, nullptr, "Client.ClientFXSequence"),
                              field("DissolvePitchRange", 10, nullptr, "BaseTypes.FloatRange"),
                              field("DissolveParamRange", 7, J::array({0, 1}))})}})}})}});
}
Monstey_Bytes Monstey_TestSkel() {
  const std::string names("Root\0Arm\0", 9);
  Monstey_Bytes b(96, 0);
  Monstey_PutU32(b, 0, 0x4c454b53);
  Monstey_PutU32(b, 4, 17);
  Monstey_PutU32(b, 12, static_cast<std::uint32_t>(names.size()));
  Monstey_PutU32(b, 40, 2);
  Monstey_AppendU32(b, 0xdcdcff00);
  b.insert(b.end(), names.begin(), names.end());
  for (unsigned i = 0; i < 2; ++i) {
    Monstey_AppendU32(b, i ? 5 : 0);
    b.push_back(1);
    for (const float f : {0.f, i ? 100.f : 0.f, 0.f, 0.f, 0.f, 0.f, 1.f})
      Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(f));
    Monstey_AppendU32(b, i ? 0 : 1);
  }
  return b;
}
J Monstey_List(Monstey_Gadb &db, const char *owner) {
  return Monstey_AnimatedExtrasCall(db, "animated_extras_list",
                                    J::object({{"owner", owner}, {"include_usage", false}}));
}
J Monstey_Create(Monstey_Gadb &db, const char *owner, const char *name = "") {
  return Monstey_AnimatedExtrasCall(db, "animated_extra_create",
                                    J::object({{"owner", owner},
                                               {"model", "Body"},
                                               {"name", name},
                                               {"base_animation", ""},
                                               {"owner_attach_node", "Arm"},
                                               {"start_visible", false}}));
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    const auto base =
        argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path();
    const auto out =
        base / (L"AnimatedExtras_" +
                std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(out);
    auto groups = J::array();
    auto test = [&](const char *name, auto f) {
      std::cout << name << std::endl;
      f();
      groups.as_array().push_back(name);
    };
    auto reject = [&](auto f) {
      bool bad = false;
      try {
        f();
      } catch (const Monstey_Error &) {
        bad = true;
      }
      Monstey_Require(bad, "Expected failure");
    };
    const auto schema = out / L"schema.json", source = out / L"fixture.gamedb",
               skel = out / L"fixture.skel";
    Monstey_WriteNewText(schema, Monstey_TestSchema().dump(2));
    Monstey_WriteNewFile(skel, Monstey_TestSkel());
    auto seed = Monstey_Gadb::New(schema);
    const auto modelCat = static_cast<std::uint32_t>(
        seed->CreateCategory("Model", "Model").at("category").at("index").as_uint());
    const auto body = seed->CreateRecord(modelCat, "Body").at("record").at("reference").as_string();
    seed->AddField(body, "ModelFile", 4, "characters/test.skel");
    const auto ownerCat =
        static_cast<std::uint32_t>(seed->CreateCategory("Character.Models", "Character.Models")
                                       .at("category")
                                       .at("index")
                                       .as_uint());
    for (const char *name : {"Alpha", "Beta", "Gamma"}) {
      const auto ref = seed->CreateRecord(ownerCat, name).at("record").at("reference").as_string();
      seed->AddField(ref, "AttachedAnimExtras", 10, J::array(),
                     J::object({{"array", -1}, {"target", "AnimatedExtra.Object"}}));
      seed->AddField(ref, "ModelRecord", 10, body, J::object({{"target", "Model"}}));
    }
    seed->Build(source);
    auto db = std::make_unique<Monstey_Gadb>(source, schema);
    const auto hash = Monstey_HashSha256(source);
    std::string first, independent;
    test("exact_five_typed_catalog_operations",
         [&] { Monstey_Require(Monstey_AnimatedExtrasCatalog().size() == 5, "Catalog mismatch"); });
    test("native_list_empty_before_category_exists", [&] {
      Monstey_Require(Monstey_List(*db, "Alpha").at("total").as_uint() == 0, "Not empty");
    });
    test("create_anonymous_native_extra_with_empty_animation_and_hidden_startup", [&] {
      const auto r = Monstey_Create(*db, "Alpha");
      first = r.at("result").at("reference").as_string();
      const auto row = Monstey_List(*db, "Alpha").at("rows")[0];
      Monstey_Require(row.at("anonymous").as_bool() && row.at("base_animation") == J("") &&
                          !row.at("start_visible").as_bool(),
                      "Native empty/hidden semantics changed");
    });
    test("one_undo_redo_covers_extra_fields_and_owner_link", [&] {
      db->Undo();
      Monstey_Require(Monstey_List(*db, "Alpha").at("total").as_uint() == 0, "Undo not atomic");
      db->Redo();
      Monstey_Require(Monstey_List(*db, "Alpha").at("total").as_uint() == 1, "Redo lost graph");
    });
    test("shared_copy_retains_physical_reference", [&] {
      auto r = Monstey_AnimatedExtrasCall(*db, "animated_extra_copy_to_owner",
                                          J::object({{"source_owner", "Alpha"},
                                                     {"source_index", 0},
                                                     {"target_owner", "Beta"},
                                                     {"policy", "shared"}}));
      Monstey_Require(r.at("result").at("reference") == J(first), "Shared link cloned");
    });
    test("shared_edit_changes_all_owners", [&] {
      Monstey_AnimatedExtrasCall(
          *db, "animated_extra_edit",
          J::object(
              {{"owner", "Beta"}, {"index", 0}, {"policy", "shared"}, {"start_attached", false}}));
      Monstey_Require(!Monstey_List(*db, "Alpha").at("rows")[0].at("start_attached").as_bool(),
                      "Shared edit failed");
    });
    test("independent_edit_preserves_unknown_field_and_null_references", [&] {
      db->AddField(first, "UnknownAttachmentPayload", 3, 5318008);
      const auto r = Monstey_AnimatedExtrasCall(*db, "animated_extra_edit",
                                                J::object({{"owner", "Beta"},
                                                           {"index", 0},
                                                           {"policy", "independent"},
                                                           {"owner_attach_node", "Root"}}));
      independent = r.at("result").at("reference").as_string();
      bool found = false;
      const auto meta = db->Metadata(independent);
      for (const auto &f : meta.at("fields").as_array())
        if (f.at("hash").as_string() == Monstey_Hex(Monstey_NativeHash("UnknownAttachmentPayload")))
          found = f.at("value").as_int() == 5318008;
      Monstey_Require(found && independent != first &&
                          r.at("result").at("extra").at("persistent_fx").is_null() &&
                          Monstey_List(*db, "Alpha").at("rows")[0].at("owner_attach_node") ==
                              J("Arm"),
                      "Independent clone corrupted unknown/native values");
    });
    test("copy_policy_required_shared_copy_cannot_edit", [&] {
      const auto before = db->Changes();
      reject([&] {
        Monstey_AnimatedExtrasCall(
            *db, "animated_extra_copy_to_owner",
            J::object({{"source_owner", "Alpha"}, {"source_index", 0}, {"target_owner", "Gamma"}}));
      });
      reject([&] {
        Monstey_AnimatedExtrasCall(*db, "animated_extra_copy_to_owner",
                                   J::object({{"source_owner", "Alpha"},
                                              {"source_index", 0},
                                              {"target_owner", "Gamma"},
                                              {"policy", "shared"},
                                              {"start_visible", true}}));
      });
      Monstey_Require(db->Changes() == before, "Rejected copy changed graph");
    });
    test("duplicate_slots_and_insertion_order_preserved", [&] {
      db->SetField("Alpha", "AttachedAnimExtras", J::array({first, nullptr, first}));
      auto r = Monstey_AnimatedExtrasCall(*db, "animated_extra_copy_to_owner",
                                          J::object({{"source_owner", "Beta"},
                                                     {"source_index", 0},
                                                     {"target_owner", "Alpha"},
                                                     {"policy", "shared"},
                                                     {"index", 1}}));
      const auto rows = Monstey_List(*db, "Alpha").at("rows");
      Monstey_Require(rows.size() == 4 && rows[0].at("reference") == J(first) &&
                          rows[1].at("reference") == J(independent) &&
                          rows[2].at("reference").is_null() && rows[3].at("reference") == J(first),
                      "Slot order or duplicates changed");
    });
    test("incoming_usage_tracks_duplicate_slots", [&] {
      Monstey_Require(db->IncomingReferences(first).at("total").as_uint() == 2,
                      "Duplicate usage not counted");
    });
    test("stale_slot_compare_rejects_without_changes", [&] {
      const auto before = db->Changes();
      reject([&] {
        Monstey_AnimatedExtrasCall(*db, "animated_extra_edit",
                                   J::object({{"owner", "Alpha"},
                                              {"index", 0},
                                              {"expected_reference", independent},
                                              {"policy", "shared"},
                                              {"start_visible", true}}));
      });
      Monstey_Require(before == db->Changes(), "Stale slot changed graph");
    });
    test("shared_orphan_deletion_rolls_back_unlink", [&] {
      const auto before = db->Changes();
      reject([&] {
        Monstey_AnimatedExtrasCall(
            *db, "animated_extra_remove",
            J::object({{"owner", "Alpha"}, {"index", 0}, {"delete_orphan", true}}));
      });
      Monstey_Require(before == db->Changes(), "Shared delete unlinked partial graph");
    });
    test("null_slot_unlinks_but_cannot_be_edited", [&] {
      const auto before = db->Changes();
      reject([&] {
        Monstey_AnimatedExtrasCall(
            *db, "animated_extra_edit",
            J::object({{"owner", "Alpha"}, {"index", 2}, {"policy", "shared"}}));
      });
      Monstey_Require(before == db->Changes(), "Null edit changed graph");
      Monstey_AnimatedExtrasCall(
          *db, "animated_extra_remove",
          J::object({{"owner", "Alpha"}, {"index", 2}, {"delete_orphan", true}}));
    });
    test("new_named_extra_and_orphan_delete", [&] {
      auto r = Monstey_Create(*db, "Gamma", "NamedExtra");
      Monstey_Require(!r.at("result").at("extra").at("anonymous").as_bool(),
                      "Named creation became anonymous");
      auto d = Monstey_AnimatedExtrasCall(
          *db, "animated_extra_remove",
          J::object({{"owner", "Gamma"}, {"index", 0}, {"delete_orphan", true}}));
      Monstey_Require(d.at("result").at("record_deleted").as_bool(), "Orphan not deleted");
    });
    test("schema_defaults_and_reference_targets_are_enforced", [&] {
      const auto before = db->Changes();
      reject([&] {
        Monstey_AnimatedExtrasCall(*db, "animated_extra_create",
                                   J::object({{"owner", "Alpha"},
                                              {"model", "Beta"},
                                              {"base_animation", ""},
                                              {"owner_attach_node", "Arm"}}));
      });
      reject([&] {
        Monstey_AnimatedExtrasCall(*db, "animated_extra_edit",
                                   J::object({{"owner", "Alpha"},
                                              {"index", 0},
                                              {"policy", "shared"},
                                              {"persistent_fx", "Body"}}));
      });
      Monstey_Require(before == db->Changes(), "Wrong targets changed graph");
    });
    test("rig_membership_exact_and_partial_rig_are_distinguished", [&] {
      auto a = J::object({{"owner", "Gamma"},
                          {"model", "Body"},
                          {"base_animation", ""},
                          {"owner_attach_node", "Arm"},
                          {"skeleton", Monstey_Utf8(skel.wstring())},
                          {"skeleton_resource", "characters/test.skel"},
                          {"expected_skeleton_sha256", Monstey_HashSha256(skel)}});
      const auto r = Monstey_AnimatedExtrasCall(*db, "animated_extra_create", a);
      Monstey_Require(
          r.at("result").at("skeleton_validation").at("owner_attach_node_verified").as_bool(),
          "Exact supplied node not found");
      a["owner_attach_node"] = "External_Attachment_Node";
      const auto p = Monstey_AnimatedExtrasCall(*db, "animated_extra_create", a);
      Monstey_Require(
          !p.at("result").at("skeleton_validation").at("owner_attach_node_verified").as_bool() &&
              !p.at("result")
                   .at("skeleton_validation")
                   .at("full_attachment_rig_verified")
                   .as_bool(),
          "Partial rig overclaimed");
      const auto before = db->Changes();
      a["require_attach_node"] = true;
      reject([&] { Monstey_AnimatedExtrasCall(*db, "animated_extra_create", a); });
      Monstey_Require(before == db->Changes(), "Explicit missing node left changes");
    });
    test("bad_rig_hash_and_logical_resource_roll_back", [&] {
      auto a = J::object({{"owner", "Alpha"},
                          {"model", "Body"},
                          {"base_animation", ""},
                          {"owner_attach_node", "Arm"},
                          {"skeleton", Monstey_Utf8(skel.wstring())}});
      const auto before = db->Changes();
      a["expected_skeleton_sha256"] = std::string(64, '0');
      reject([&] { Monstey_AnimatedExtrasCall(*db, "animated_extra_create", a); });
      a.as_object().erase("expected_skeleton_sha256");
      a["skeleton_resource"] = "wrong.skel";
      reject([&] { Monstey_AnimatedExtrasCall(*db, "animated_extra_create", a); });
      Monstey_Require(before == db->Changes(), "Rig error left partial graph");
    });
    test("typed_constraints_and_invalid_arguments_are_atomic", [&] {
      for (const auto patch :
           {J::object({{"start_visible", 1}}), J::object({{"dissolve_param_range", J::array({1})}}),
            J::object({{"base_animation", std::string("a\0b", 3)}}),
            J::object({{"extra", true}})}) {
        auto a = J::object({{"owner", "Alpha"}, {"index", 0}, {"policy", "shared"}});
        for (const auto &[k, v] : patch.as_object())
          a[k] = v;
        const auto before = db->Changes();
        reject([&] { Monstey_AnimatedExtrasCall(*db, "animated_extra_edit", a); });
        Monstey_Require(before == db->Changes(), "Bad typed argument mutated graph");
      }
    });
    test("library_search_paging_retains_physical_owner_indices", [&] {
      const auto all = Monstey_AnimatedExtrasCall(*db, "animated_extras_list",
                                                  J::object({{"include_usage", false}}));
      const auto page = Monstey_AnimatedExtrasCall(
          *db, "animated_extras_list",
          J::object({{"offset", 1}, {"limit", 1}, {"include_usage", false}}));
      Monstey_Require(page.at("total") == all.at("total") && page.at("rows").size() == 1 &&
                          page.at("rows")[0].at("reference") == all.at("rows")[1].at("reference"),
                      "Library paging differs");
      const auto search = Monstey_AnimatedExtrasCall(
          *db, "animated_extras_list",
          J::object({{"owner", "Alpha"}, {"query", "Root"}, {"include_usage", false}}));
      Monstey_Require(search.at("total").as_uint() == 1 &&
                          search.at("rows")[0].at("index").as_uint() == 1,
                      "Filtered index changed");
    });
    test(
        "usage_pages_and_filtered_owner_rows_match_complete_golden_with_nulls_and_duplicates", [&] {
          db->SetField("Alpha", "AttachedAnimExtras",
                       J::array({first, nullptr, independent, first}));
          const auto all =
              Monstey_AnimatedExtrasCall(*db, "animated_extras_list",
                                         J::object({{"owner", "Alpha"}, {"include_usage", true}}));
          const auto page = Monstey_AnimatedExtrasCall(
              *db, "animated_extras_list",
              J::object(
                  {{"owner", "Alpha"}, {"include_usage", true}, {"offset", 2}, {"limit", 1}}));
          const auto nullPage = Monstey_AnimatedExtrasCall(
              *db, "animated_extras_list",
              J::object(
                  {{"owner", "Alpha"}, {"include_usage", true}, {"offset", 1}, {"limit", 1}}));
          const auto filtered = Monstey_AnimatedExtrasCall(
              *db, "animated_extras_list",
              J::object(
                  {{"owner", "Alpha"}, {"include_usage", true}, {"query", "Root"}, {"limit", 1}}));
          Monstey_Require(
              all["total"].as_uint() == 4 && page["total"] == all["total"] &&
                  page["rows"].size() == 1 && page["rows"][0] == all["rows"][2] &&
                  page["rows"][0]["incoming_references"]["total"].as_uint() == 2 &&
                  nullPage["rows"][0] == all["rows"][1] &&
                  !nullPage["rows"][0].contains("incoming_references") &&
                  filtered["total"].as_uint() == 1 && filtered["rows"][0] == all["rows"][2],
              "Paged usage, null slots, filtered indices, totals, or complete row payload changed");
          const auto library = Monstey_AnimatedExtrasCall(*db, "animated_extras_list",
                                                          J::object({{"include_usage", true}}));
          const auto libraryPage = Monstey_AnimatedExtrasCall(
              *db, "animated_extras_list",
              J::object({{"include_usage", true}, {"offset", 1}, {"limit", 1}}));
          Monstey_Require(libraryPage["total"] == library["total"] &&
                              libraryPage["rows"][0] == library["rows"][1],
                          "Library usage page differs from complete rows");
          db->Undo();
        });
    test("native_candidate_project_replay_byte_identity_zero_warnings", [&] {
      const auto project = out / L"project.json", a = out / L"candidate.gamedb",
                 b = out / L"reopened.gamedb";
      db->SaveProject(project);
      const auto report = db->Build(a);
      auto restored = Monstey_Gadb::LoadProject(project);
      restored->Build(b);
      Monstey_Require(Monstey_ReadFile(a) == Monstey_ReadFile(b) && report.at("warnings").empty(),
                      "Project replay or native build differs");
      Monstey_Gadb native(a, schema);
      Monstey_Require(Monstey_List(native, "Alpha").at("total").as_uint() == 3,
                      "Native owner links lost");
    });
    test("capacity_failure_rolls_back_fresh_extra_and_history", [&] {
      J full = J::array();
      full.as_array().resize(65535);
      db->SetField("Alpha", "AttachedAnimExtras", full);
      const auto before = db->Changes();
      reject([&] { Monstey_Create(*db, "Alpha"); });
      Monstey_Require(before == db->Changes(), "Capacity error left extra");
      db->Undo();
    });
    test("source_and_schema_identity_guards_reject", [&] {
      for (const auto key : {"expected_database_sha256", "expected_schema_sha256"})
        reject([&] {
          Monstey_AnimatedExtrasCall(*db, "animated_extras_list",
                                     J::object({{key, std::string(64, '0')}}));
        });
      Monstey_Require(Monstey_HashSha256(source) == hash, "Source overwritten");
    });
    test("same_size_same_stamp_source_mutation_is_rejected", [&] {
      const auto p = out / L"stale.gamedb";
      auto b = Monstey_ReadFile(source);
      Monstey_WriteNewFile(p, b);
      Monstey_Gadb stale(p, schema);
      const auto stamp = std::filesystem::last_write_time(p);
      b.back() ^= 1;
      {
        std::ofstream f(Monstey_WindowsFilePath(p), std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char *>(b.data()), static_cast<std::streamsize>(b.size()));
        Monstey_Require(f.good(), "Mutation write failed");
      }
      std::filesystem::last_write_time(p, stamp);
      reject([&] { Monstey_AnimatedExtrasCall(stale, "animated_extras_list"); });
    });
    test("same_size_same_stamp_schema_mutation_is_rejected", [&] {
      const auto p = out / L"stale-schema.json";
      auto b = Monstey_ReadFile(schema);
      Monstey_WriteNewFile(p, b);
      Monstey_Gadb stale(source, p);
      const auto stamp = std::filesystem::last_write_time(p);
      b.back() ^= 1;
      {
        std::ofstream f(Monstey_WindowsFilePath(p), std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char *>(b.data()), static_cast<std::streamsize>(b.size()));
        Monstey_Require(f.good(), "Schema mutation write failed");
      }
      std::filesystem::last_write_time(p, stamp);
      reject([&] { Monstey_AnimatedExtrasCall(stale, "animated_extras_list"); });
    });
    test("qualified_schema_shape_and_native_class_cannot_be_spoofed_by_display_name", [&] {
      auto badSchema = Monstey_TestSchema();
      badSchema["categories"]["Character.Models"]["fields"][0]["type_code"] = 11;
      auto bad = Monstey_Gadb::WithSchema(source, badSchema);
      reject([&] { Monstey_AnimatedExtrasCall(*bad, "animated_extras_list"); });
      auto spoof = Monstey_Gadb::New(schema);
      const auto cat = static_cast<std::uint32_t>(
          spoof->CreateCategory("Model", "Character.Models").at("category").at("index").as_uint());
      spoof->CreateRecord(cat, "Spoof");
      reject([&] { Monstey_List(*spoof, "Spoof"); });
    });
    test("owner_rig_options_require_skeleton_even_when_empty", [&] {
      reject([&] {
        Monstey_AnimatedExtrasCall(*db, "animated_extras_list",
                                   J::object({{"owner", "Gamma"}, {"require_attach_node", true}}));
      });
    });
    J native = J::object();
    if (argc > 3) {
      auto actual = std::make_unique<Monstey_Gadb>(argv[2], argv[3]);
      const auto sourceHash = Monstey_HashSha256(argv[2]);
      native["source_sha256"] = sourceHash;
      test("actual_four_Baranor_extras_preserve_original_order_empty_animation_hidden_flags", [&] {
        const auto r = Monstey_List(*actual, "0x85100075");
        native["original"] = r;
        Monstey_Require(r.at("total").as_uint() == 4, "Expected four Baranor extras");
        const auto &rows = r.at("rows");
        Monstey_Require(rows[0].at("reference") == J("0x16200001") &&
                            rows[1].at("reference") == J("0x16200003") &&
                            rows[2].at("reference") == J("0x16200002") &&
                            rows[3].at("reference") == J("0x16200000") &&
                            rows[1].at("base_animation") == J("") &&
                            !rows[0].at("start_visible").as_bool(),
                        "Native Baranor values differ");
      });
      test("actual_fresh_extra_uses_qualified_eight_fields_and_atomic_undo", [&] {
        const auto before = actual->Changes();
        auto r = Monstey_AnimatedExtrasCall(*actual, "animated_extra_create",
                                            J::object({{"owner", "0x85100075"},
                                                       {"model", "0x75900928"},
                                                       {"name", "Monstey_Qualification_Fresh"},
                                                       {"base_animation", ""},
                                                       {"owner_attach_node", "LeftHand"},
                                                       {"start_visible", false}}));
        Monstey_Require(r.at("result").at("extra").at("fields").size() == 8 &&
                            r.at("result").at("extra").at("persistent_fx").is_null(),
                        "Fresh native descriptors/defaults differ");
        const auto file = out / L"actual-fresh.gamedb";
        native["fresh_candidate"] = actual->Build(file);
        Monstey_Require(native.at("fresh_candidate").at("warnings").empty(),
                        "Fresh native candidate warnings");
        Monstey_Gadb reload(file, argv[3]);
        Monstey_Require(Monstey_List(reload, "0x85100075").at("rows").size() == 5,
                        "Fresh native owner link lost");
        actual->Undo();
        Monstey_Require(actual->Changes() == before, "Fresh actual undo not atomic");
      });
      test("actual_independent_copy_edit_and_project_byte_identity", [&] {
        auto r = Monstey_AnimatedExtrasCall(*actual, "animated_extra_copy_to_owner",
                                            J::object({{"source_owner", "0x85100075"},
                                                       {"source_index", 1},
                                                       {"target_owner", "0x85100075"},
                                                       {"policy", "independent"},
                                                       {"start_visible", true}}));
        native["copy"] = r;
        Monstey_AnimatedExtrasCall(*actual, "animated_extra_edit",
                                   J::object({{"owner", "0x85100075"},
                                              {"index", 4},
                                              {"policy", "shared"},
                                              {"owner_attach_node", "LeftHand"}}));
        const auto project = out / L"actual-project.json", a = out / L"actual-candidate.gamedb",
                   b = out / L"actual-reopened.gamedb";
        actual->SaveProject(project);
        native["candidate"] = actual->Build(a);
        auto replay = Monstey_Gadb::LoadProject(project);
        replay->Build(b);
        Monstey_Require(Monstey_ReadFile(a) == Monstey_ReadFile(b) &&
                            native.at("candidate").at("warnings").empty(),
                        "Actual candidate persistence differs");
        native["candidate_sha256"] = Monstey_HashSha256(a);
        Monstey_Gadb reload(a, argv[3]);
        const auto rows = Monstey_List(reload, "0x85100075").at("rows");
        Monstey_Require(rows.size() == 5 && rows[4].at("start_visible").as_bool() &&
                            rows[4].at("base_animation") == J("") &&
                            rows[4].at("owner_attach_node") == J("LeftHand"),
                        "Actual clone fields lost");
      });
      if (argc > 4)
        test("actual_supplied_native_skel_membership_does_not_claim_complete_attachment_rig", [&] {
          const auto path = std::filesystem::path(argv[4]);
          native["supplied_rig"] = Monstey_AnimatedExtrasCall(
              *actual, "animated_extras_list",
              J::object({{"owner", "0x85100075"},
                         {"skeleton", Monstey_Utf8(path.wstring())},
                         {"expected_skeleton_sha256", Monstey_HashSha256(path)},
                         {"include_usage", false}}));
          for (const auto &row : native.at("supplied_rig").at("rows").as_array()) {
            const auto &validation = row.at("skeleton_validation");
            Monstey_Require(validation.at("provided").as_bool() &&
                                validation.at("bone_count").as_uint() > 0 &&
                                !validation.at("full_attachment_rig_verified").as_bool() &&
                                !validation.at("animation_library_verified").as_bool(),
                            "Supplied skin rig was overqualified");
          }
        });
      test("actual_source_bytes_unchanged", [&] {
        Monstey_Require(Monstey_HashSha256(argv[2]) == sourceHash, "Actual DB changed");
      });
    }
    const auto report = J::object({{"status", "PASS"},
                                   {"groups", groups.size()},
                                   {"tests", groups},
                                   {"native", native},
                                   {"game_written", false},
                                   {"runtime_verified", false},
                                   {"animation_merge_verified", false}});
    Monstey_WriteNewText(out / L"animated-extras-tests.json", report.dump(2));
    std::cout << "PASS " << groups.size() << " groups " << Monstey_Utf8(out.wstring()) << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
