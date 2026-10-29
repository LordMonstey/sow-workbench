#include "Monstey_Model.hpp"
#include "Monstey_ModelSockets.hpp"
#include <bit>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
using namespace Monstey;
namespace {
using J = Monstey_Json;
J Monstey_TestSchema() {
  auto f = [](const char *name, unsigned type, const J &value, J array = 1) {
    return J::object({{"name", name},
                      {"type_code", type},
                      {"array", array},
                      {"default", value}});
  };
  auto links = f("SimpleModelSocketOffsets", 11, nullptr, -1);
  links["target"] = "ModelSocketOverride";
  return J::object(
      {{"categories",
        J::object(
            {{"Model",
              J::object({{"fields",
                          J::array({f("ModelFile", 4, "characters/test.skel"),
                                    links})}})},
             {"ModelSocketOverride",
              J::object(
                  {{"struct", true},
                   {"fields",
                    J::array({f("SocketName", 4, nullptr),
                              f("ParentNode", 4, nullptr),
                              f("PosOffsetFromParent", 8, J::array({0, 0, 0})),
                              f("RotOffsetFromParent", 9,
                                J::array({0, 0, 0, 1}))})}})}})}});
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
J Monstey_List(Monstey_Gadb &db, const char *model) {
  return Monstey_ModelSocketsCall(db, "model_sockets_list",
                                  J::object({{"model", model}}));
}
J Monstey_Create(Monstey_Gadb &db, const char *model,
                 const char *socket = "Socket_Test") {
  return Monstey_ModelSocketsCall(
      db, "model_socket_create",
      J::object({{"model", model},
                 {"socket_name", socket},
                 {"parent_node", "Arm"},
                 {"position_cm", J::array({1.25, -2.5, 3.75})},
                 {"rotation_xyzw", J::array({0.0, 0.0, 0.0, -1.0})}}));
}
void Monstey_Check(bool b, const char *message) { Monstey_Require(b, message); }
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    const auto base = argc > 1 ? std::filesystem::path(argv[1])
                               : std::filesystem::temp_directory_path();
    const auto out =
        base /
        (L"ModelSockets_" +
         std::to_wstring(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(out);
    J tests = J::array();
    auto test = [&](const char *name, auto f) {
      std::cout << name << std::endl;
      f();
      tests.as_array().push_back(name);
    };
    auto reject = [&](auto f) {
      bool bad = false;
      try {
        f();
      } catch (const Monstey_Error &) {
        bad = true;
      }
      Monstey_Check(bad, "Expected socket operation failure");
    };
    const auto schema = out / L"schema.json", source = out / L"fixture.gamedb",
               skel = out / L"fixture.skel";
    Monstey_WriteNewText(schema, Monstey_TestSchema().dump(2));
    Monstey_WriteNewFile(skel, Monstey_TestSkel());
    auto seed = Monstey_Gadb::New(schema);
    const auto ci =
        static_cast<std::uint32_t>(seed->CreateCategory("Model", "Model")
                                       .at("category")
                                       .at("index")
                                       .as_uint());
    for (const char *name : {"Alpha", "Beta", "Gamma"}) {
      auto ref =
          seed->CreateRecord(ci, name).at("record").at("reference").as_string();
      seed->AddField(ref, "ModelFile", 4, "characters/test.skel");
      seed->AddField(
          ref, "SimpleModelSocketOffsets", 11, J::array(),
          J::object({{"array", -1}, {"target", "ModelSocketOverride"}}));
    }
    seed->Build(source);
    auto db = std::make_unique<Monstey_Gadb>(source, schema);
    const auto sourceHash = Monstey_HashSha256(source);
    std::string first, copy;
    test("catalog_exposes_exact_five_native_routes", [&] {
      auto c = Monstey_ModelSocketsCatalog();
      Monstey_Check(c.size() == 5, "Socket catalog count mismatch");
    });
    test("empty_model_links_read_without_struct_template", [&] {
      auto r = Monstey_List(*db, "Alpha");
      Monstey_Check(r.at("total").as_uint() == 0,
                    "New fixture model is not empty");
    });
    test(
        "create_new_anonymous_override_and_native_type11_link_atomically", [&] {
          auto r = Monstey_Create(*db, "Alpha");
          first = r.at("result").at("reference").as_string();
          auto rows = Monstey_List(*db, "Alpha").at("rows");
          Monstey_Check(
              rows.size() == 1 && rows[0].at("anonymous").as_bool(),
              "Creation cloned instead of creating an anonymous native struct");
          Monstey_Check(
              rows[0].at("position_cm") == J::array({1.25, -2.5, 3.75}) &&
                  rows[0].at("rotation_xyzw") ==
                      J::array({0.0, 0.0, 0.0, -1.0}),
              "Native socket units/quaternion were converted or normalized");
        });
    test("single_undo_redo_covers_creation_fields_and_link", [&] {
      db->Undo();
      Monstey_Check(Monstey_List(*db, "Alpha").at("total").as_uint() == 0,
                    "Creation was split across undo entries");
      db->Redo();
      Monstey_Check(Monstey_List(*db, "Alpha").at("total").as_uint() == 1,
                    "Redo lost socket graph");
    });
    test("shared_copy_preserves_physical_reference", [&] {
      auto r = Monstey_ModelSocketsCall(*db, "model_socket_copy_to_model",
                                        J::object({{"source_model", "Alpha"},
                                                   {"source_index", 0},
                                                   {"target_model", "Beta"},
                                                   {"policy", "shared"}}));
      Monstey_Check(r.at("result").at("reference").as_string() == first,
                    "Shared copy cloned the physical override");
    });
    test("incoming_usage_uses_native_reference_slots", [&] {
      auto r = Monstey_List(*db, "Beta").at("rows")[0];
      Monstey_Check(r.at("incoming_references").at("total").as_uint() == 2,
                    "Shared native reference users were not counted");
    });
    test("shared_edit_changes_all_linked_models", [&] {
      Monstey_ModelSocketsCall(
          *db, "model_socket_edit",
          J::object({{"model", "Beta"},
                     {"index", 0},
                     {"policy", "shared"},
                     {"position_cm", J::array({4.0, 5.0, 6.0})}}));
      Monstey_Check(
          Monstey_List(*db, "Alpha").at("rows")[0].at("position_cm") ==
              J::array({4.0, 5.0, 6.0}),
          "Shared socket edit did not update other model");
    });
    test("independent_edit_detaches_selected_slot_and_retains_unknown_fields",
         [&] {
           db->AddField(first, "UnrecognizedNativeField", 3, 123456);
           auto r = Monstey_ModelSocketsCall(
               *db, "model_socket_edit",
               J::object({{"model", "Beta"},
                          {"index", 0},
                          {"policy", "independent"},
                          {"socket_name", "Socket_Private"},
                          {"position_cm", J::array({7.0, 8.0, 9.0})}}));
           copy = r.at("result").at("reference").as_string();
           Monstey_Check(
               copy != first &&
                   Monstey_List(*db, "Alpha").at("rows")[0].at("position_cm") ==
                       J::array({4.0, 5.0, 6.0}),
               "Independent edit changed the shared source");
           auto meta = db->Metadata(copy);
           bool found = false;
           for (const auto &field : meta.at("fields").as_array())
             if (field.at("hash").as_string() ==
                 Monstey_Hex(Monstey_NativeHash("UnrecognizedNativeField"))) {
               found = true;
               Monstey_Check(field.at("value").as_int() == 123456,
                             "Unrecognized native field was lost");
             }
           Monstey_Check(found, "Clone omitted unrecognized native field");
         });
    test("independent_copy_supports_explicit_target_parent_transform_changes",
         [&] {
           auto r = Monstey_ModelSocketsCall(
               *db, "model_socket_copy_to_model",
               J::object({{"source_model", "Alpha"},
                          {"source_index", 0},
                          {"target_model", "Gamma"},
                          {"policy", "independent"},
                          {"parent_node", "Root"},
                          {"socket_name", "Socket_Gamma"},
                          {"rotation_xyzw", J::array({0, 0, 1, 0})}}));
           Monstey_Check(
               r.at("result").at("reference").as_string() != first &&
                   Monstey_List(*db, "Gamma").at("rows")[0].at("parent_node") ==
                       J("Root"),
               "Independent copy did not apply explicit target values");
         });
    test("copy_policy_is_required_and_shared_copy_cannot_mutate_donor", [&] {
      auto before = db->Changes();
      reject([&] {
        Monstey_ModelSocketsCall(*db, "model_socket_copy_to_model",
                                 J::object({{"source_model", "Alpha"},
                                            {"source_index", 0},
                                            {"target_model", "Gamma"}}));
      });
      reject([&] {
        Monstey_ModelSocketsCall(*db, "model_socket_copy_to_model",
                                 J::object({{"source_model", "Alpha"},
                                            {"source_index", 0},
                                            {"target_model", "Gamma"},
                                            {"policy", "shared"},
                                            {"parent_node", "Root"}}));
      });
      Monstey_Check(db->Changes() == before,
                    "Failed shared-copy policy changed graph/history");
    });
    test("skeleton_parent_validation_and_distinct_socket_label", [&] {
      auto r = Monstey_ModelSocketsCall(
          *db, "model_socket_create",
          J::object({{"model", "Gamma"},
                     {"socket_name", "Label_Not_A_Bone"},
                     {"parent_node", "Arm"},
                     {"skeleton", Monstey_Utf8(skel.wstring())},
                     {"skeleton_resource", "characters/test.skel"},
                     {"expected_skeleton_sha256", Monstey_HashSha256(skel)}}));
      auto v = r.at("result").at("skeleton_validation");
      Monstey_Check(v.at("parent_node_verified").as_bool() &&
                        !v.at("socket_name_in_bone_table").as_bool() &&
                        v.at("model_resource_path_verified").as_bool(),
                    "Socket label was incorrectly required to be a bone");
    });
    test("skeleton_hash_parent_and_resource_failures_roll_back", [&] {
      for (const auto key : {"hash", "parent", "resource"}) {
        auto a = J::object({{"model", "Gamma"},
                            {"socket_name", "Rejected"},
                            {"parent_node", "Arm"},
                            {"skeleton", Monstey_Utf8(skel.wstring())}});
        if (std::string_view(key) == "hash")
          a["expected_skeleton_sha256"] = std::string(64, '0');
        if (std::string_view(key) == "parent")
          a["parent_node"] = "Not_A_Node";
        if (std::string_view(key) == "parent")
          a["require_parent_node"] = true;
        if (std::string_view(key) == "resource")
          a["skeleton_resource"] = "characters/wrong.skel";
        auto before = db->Changes();
        reject(
            [&] { Monstey_ModelSocketsCall(*db, "model_socket_create", a); });
        Monstey_Check(db->Changes() == before,
                      "Failed SKEL validation left records, links or history");
      }
    });
    test("model_list_can_validate_supplied_native_skeleton", [&] {
      auto r = Monstey_ModelSocketsCall(
          *db, "model_sockets_list",
          J::object({{"model", "Alpha"},
                     {"skeleton", Monstey_Utf8(skel.wstring())}}));
      Monstey_Check(r.at("rows")[0]
                        .at("skeleton_validation")
                        .at("parent_node_verified")
                        .as_bool(),
                    "Read-only socket validation was ignored");
    });
    test("name_float_and_quaternion_constraints_are_transactional", [&] {
      for (unsigned scenario = 0; scenario < 5; ++scenario) {
        auto a = J::object({{"model", "Alpha"},
                            {"socket_name", "Bad"},
                            {"parent_node", "Arm"}});
        if (scenario == 0)
          a["socket_name"] = "";
        if (scenario == 1)
          a["parent_node"] = std::string("A\0B", 3);
        if (scenario == 2)
          a["position_cm"] = J::array({1, 2});
        if (scenario == 3)
          a["position_cm"] =
              J::array({std::numeric_limits<double>::max(), 0, 0});
        if (scenario == 4)
          a["rotation_xyzw"] = J::array({0, 0, 0, 2});
        const auto before = db->Changes();
        reject(
            [&] { Monstey_ModelSocketsCall(*db, "model_socket_create", a); });
        Monstey_Check(before == db->Changes(),
                      "Invalid native socket values left changes");
      }
    });
    test("index_insert_preserves_existing_slot_order", [&] {
      auto r = Monstey_ModelSocketsCall(*db, "model_socket_create",
                                        J::object({{"model", "Alpha"},
                                                   {"socket_name", "Inserted"},
                                                   {"parent_node", "Root"},
                                                   {"index", 0}}));
      auto rows = Monstey_List(*db, "Alpha").at("rows");
      Monstey_Check(rows.size() == 2 &&
                        rows[0].at("reference") ==
                            r.at("result").at("reference") &&
                        rows[1].at("reference") == J(first),
                    "Socket insertion reordered existing slots");
    });
    test("remove_unlinks_and_retains_physical_record", [&] {
      auto r = Monstey_ModelSocketsCall(
          *db, "model_socket_remove",
          J::object({{"model", "Alpha"}, {"index", 0}}));
      Monstey_Check(
          !r.at("result").at("record_deleted").as_bool() &&
              !db->Metadata(r.at("result").at("reference").as_string())
                   .at("pending_delete")
                   .as_bool(),
          "Unlink deleted a physical record");
    });
    test("orphan_delete_rejects_shared_reference_and_rolls_back", [&] {
      Monstey_ModelSocketsCall(*db, "model_socket_copy_to_model",
                               J::object({{"source_model", "Alpha"},
                                          {"source_index", 0},
                                          {"target_model", "Gamma"},
                                          {"policy", "shared"}}));
      auto before = db->Changes();
      reject([&] {
        Monstey_ModelSocketsCall(
            *db, "model_socket_remove",
            J::object(
                {{"model", "Alpha"}, {"index", 0}, {"delete_orphan", true}}));
      });
      Monstey_Check(before == db->Changes(),
                    "Failed shared deletion removed selected model link");
    });
    test("orphan_delete_and_library_paging_use_active_records", [&] {
      auto r = Monstey_ModelSocketsCall(
          *db, "model_socket_remove",
          J::object(
              {{"model", "Beta"}, {"index", 0}, {"delete_orphan", true}}));
      Monstey_Check(r.at("result").at("record_deleted").as_bool(),
                    "Unreferenced private override was not deleted");
      auto a = Monstey_ModelSocketsCall(*db, "model_sockets_list",
                                        J::object({{"include_usage", false}}));
      auto b = Monstey_ModelSocketsCall(
          *db, "model_sockets_list",
          J::object({{"include_usage", false}, {"offset", 1}, {"limit", 1}}));
      Monstey_Check(b.at("total") == a.at("total") &&
                        b.at("rows").size() == 1 &&
                        b.at("rows")[0].at("reference") ==
                            a.at("rows")[1].at("reference"),
                    "Library active-record pagination differs after deletion");
    });
    test("unknown_arguments_operations_and_wrong_source_hash_reject", [&] {
      reject([&] { Monstey_ModelSocketsCall(*db, "unknown", J::object()); });
      reject([&] {
        Monstey_ModelSocketsCall(*db, "model_sockets_list",
                                 J::object({{"unused", true}}));
      });
      reject([&] {
        Monstey_ModelSocketsCall(
            *db, "model_sockets_list",
            J::object({{"expected_database_sha256", std::string(64, '0')}}));
      });
      reject([&] {
        Monstey_ModelSocketsCall(
            *db, "model_socket_remove",
            J::object({{"model", "Alpha"}, {"index", 9999}}));
      });
    });
    test("project_reopen_native_build_and_reference_remap_are_identical", [&] {
      const auto project = out / L"project.json",
                 firstFile = out / L"candidate.fmt.gamedb",
                 secondFile = out / L"project-reopen.gamedb";
      db->SaveProject(project);
      const auto report = db->Build(firstFile);
      auto restored = Monstey_Gadb::LoadProject(project);
      restored->Build(secondFile);
      Monstey_Check(Monstey_ReadFile(firstFile) ==
                            Monstey_ReadFile(secondFile) &&
                        report.at("warnings").empty(),
                    "Socket project roundtrip or native remap differs");
      Monstey_Gadb reopened(firstFile, schema);
      auto list = Monstey_List(reopened, "Alpha");
      Monstey_Check(list.at("rows")[0].at("socket_name") == J("Socket_Test"),
                    "Native output lost model-to-socket link");
    });
    test("null_slots_can_be_unlinked_but_not_edited", [&] {
      const auto meta = db->Metadata("Alpha");
      auto links = J::array({nullptr, first});
      db->SetField("Alpha", "SimpleModelSocketOffsets", links);
      auto before = db->Changes();
      reject([&] {
        Monstey_ModelSocketsCall(*db, "model_socket_edit",
                                 J::object({{"model", "Alpha"},
                                            {"index", 0},
                                            {"policy", "shared"},
                                            {"socket_name", "NoPhysicalRow"}}));
      });
      Monstey_Check(before == db->Changes(), "Null-slot edit changed source");
      auto r = Monstey_ModelSocketsCall(
          *db, "model_socket_remove",
          J::object(
              {{"model", "Alpha"}, {"index", 0}, {"delete_orphan", true}}));
      Monstey_Check(
          r.at("result").at("reference").is_null() &&
              Monstey_List(*db, "Alpha").at("rows")[0].at("reference") ==
                  J(first),
          "Removing null slot lost physical neighbor");
    });
    test("native_list_capacity_failure_rolls_back_new_record_and_fields", [&] {
      J full = J::array();
      full.as_array().resize(65535);
      db->SetField("Alpha", "SimpleModelSocketOffsets", full);
      const auto before = db->Changes();
      reject([&] { Monstey_Create(*db, "Alpha", "OverCapacity"); });
      Monstey_Check(db->Changes() == before,
                    "Capacity failure left a partial socket record");
      db->Undo();
      Monstey_Check(Monstey_List(*db, "Alpha").at("rows")[0].at("reference") ==
                        J(first),
                    "Capacity failure damaged undo history");
    });
    test("input_database_bytes_are_unchanged", [&] {
      Monstey_Check(Monstey_HashSha256(source) == sourceHash,
                    "Fixture source was overwritten");
    });
    test("source_bytes_changed_with_same_size_and_mtime_are_rejected", [&] {
      const auto path = out / L"stale-input.gamedb";
      auto bytes = Monstey_ReadFile(source);
      Monstey_WriteNewFile(path, bytes);
      Monstey_Gadb stale(path, schema);
      auto stamp = std::filesystem::last_write_time(path);
      bytes.back() ^= 1;
      {
        std::ofstream f(Monstey_WindowsFilePath(path),
                        std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char *>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
        Monstey_Check(f.good(),
                      "Cannot prepare synthetic same-stamp source mutation");
      }
      std::filesystem::last_write_time(path, stamp);
      reject([&] {
        Monstey_ModelSocketsCall(stale, "model_sockets_list", J::object());
      });
    });
    J native = J::object();
    if (argc > 3) {
      const auto actualSchema = J::Parse(Monstey_ReadText(argv[3]));
      auto actual = Monstey_Gadb::WithSchema(argv[2], actualSchema);
      const auto originalHash = Monstey_HashSha256(argv[2]);
      test("actual_ten_native_rows_and_five_original_models", [&] {
        native["library"] =
            Monstey_ModelSocketsCall(*actual, "model_sockets_list",
                                     J::object({{"include_usage", false}}));
        Monstey_Check(native.at("library").at("total").as_uint() == 10,
                      "Expected ten original native socket structs");
        for (const char *model :
             {"Smp_Orc_Stk_Body_Scar", "Smp_Orc_Slim_Body_Scar",
              "Smp_Orc_Slim_Body_Ratbag", "Smp_Orc_Slim_Body001",
              "Smp_Orc_Stk_Body001V2"}) {
          auto rows = Monstey_List(*actual, model);
          Monstey_Check(rows.at("total").as_uint() >= 1 &&
                            rows.at("total").as_uint() <= 2,
                        "Original Orc socket route differs");
          native["models"][model] = rows;
        }
      });
      test("actual_independent_copy_candidate_and_project_roundtrip", [&] {
        Monstey_ModelSocketsCall(
            *actual, "model_socket_copy_to_model",
            J::object({{"source_model", "Smp_Orc_Slim_Body001"},
                       {"source_index", 0},
                       {"target_model", "Smp_Orc_Slim_Body_Ratbag"},
                       {"policy", "independent"},
                       {"socket_name", "Monstey_Native_Test"}}));
        const auto candidate = out / L"actual-sockets.gamedb",
                   project = out / L"actual-project.json",
                   reproduced = out / L"actual-project-reopened.gamedb";
        actual->SaveProject(project);
        native["candidate"] = actual->Build(candidate);
        auto restored = Monstey_Gadb::LoadProject(project);
        restored->Build(reproduced);
        Monstey_Check(Monstey_ReadFile(candidate) ==
                              Monstey_ReadFile(reproduced) &&
                          native.at("candidate").at("warnings").empty(),
                      "Actual socket candidate failed persistence/ref remap");
      });
      if (argc > 4)
        test("actual_skin_skeleton_membership_keeps_native_external_labels",
             [&] {
               auto args = J::object(
                   {{"model", "Smp_Orc_Slim_Body_Ratbag"},
                    {"socket_name", "Monstey_Validated"},
                    {"parent_node", "L_Sheath_1"},
                    {"skeleton",
                     Monstey_Utf8(std::filesystem::path(argv[4]).wstring())},
                    {"expected_skeleton_sha256", Monstey_HashSha256(argv[4])},
                    {"skeleton_resource",
                     "characters/modelcharacters/orcs/bodies/body001/"
                     "orc_slim_body001.skel"}});
               auto result = Monstey_ModelSocketsCall(
                   *actual, "model_socket_create", args);
               const auto &validation =
                   result.at("result").at("skeleton_validation");
               Monstey_Check(
                   validation.at("provided").as_bool() &&
                       !validation.at("parent_node_verified").as_bool() &&
                       !validation.at("full_attachment_rig_verified")
                            .as_bool() &&
                       validation.at("model_resource_path_verified").as_bool(),
                   "Partial skin skeleton was misreported as the complete "
                   "native attachment rig");
               const auto before = actual->Changes();
               args["require_parent_node"] = true;
               reject([&] {
                 Monstey_ModelSocketsCall(*actual, "model_socket_create", args);
               });
               Monstey_Check(actual->Changes() == before,
                             "Explicit complete-rig validation failure left a "
                             "partial socket");
               native["skin_skeleton_validation"] = validation;
             });
      test("actual_source_database_remains_unchanged", [&] {
        Monstey_Check(Monstey_HashSha256(argv[2]) == originalHash,
                      "Actual database source changed");
      });
    }
    const auto report = J::object({{"status", "PASS"},
                                   {"groups", tests.size()},
                                   {"tests", tests},
                                   {"native", native},
                                   {"game_written", false},
                                   {"runtime_verified", false}});
    Monstey_WriteNewText(out / L"model-sockets-tests.json", report.dump(2));
    std::cout << "PASS " << tests.size() << " groups "
              << Monstey_Utf8(out.wstring()) << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}