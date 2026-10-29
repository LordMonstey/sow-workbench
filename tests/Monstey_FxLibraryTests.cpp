#include "Monstey_FxLibrary.hpp"
#include <chrono>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
J Monstey_Schema() {
    const auto f = [](std::string name, unsigned type, J value, int array = 1) {
        return J::object({{"name", name}, {"type_code", type}, {"array", array}, {"default", value}});
    };
    const auto props = J::array({f("KeyName", 4, nullptr), f("StartTime", 2, 0.0), f("EndTime", 2, 1.0),
                                 f("Strength", 2, 1.0), f("Curve", 13, nullptr, -1), f("Asset", 4, ""),
                                 f("Target", 10, nullptr)});
    return J::object(
        {{"categories",
          J::object({{"FxGlobalProps",
                      J::object({{"fields", J::array({f("FxName", 4, nullptr), f("ResourceLenMS", 3, 0),
                                                      f("KeyList", 10, nullptr, -1)})}})},
                     {"ClientFX.Template", J::object({{"fields", props}})},
                     {"ClientFX.Other", J::object({{"fields", props}})}})}});
}
J Monstey_Value(Monstey_Gadb &db, std::string_view record, std::string_view field) {
    const auto metadata = db.Metadata(record);
    for (const auto &f : metadata["fields"].as_array())
        if (f["hash"].as_string() == Monstey_Hex(Monstey_NativeHash(field)))
            return f["value"];
    throw Monstey_Error("test", "Property missing");
}
J Monstey_Values() { return J::object({{"Curve", J::object({{"constant", 2.5}})}}); }
J Monstey_Lib(Monstey_Gadb &db) { return Monstey_FxLibraryCall(db, "fx_library")["library"]; }
struct Monstey_LibraryFixture {
    std::unique_ptr<Monstey_Gadb> db;
    std::string prefab, template_record, child;
};
Monstey_LibraryFixture Monstey_IntegratedFixture(const std::filesystem::path &schema) {
    Monstey_LibraryFixture f;
    f.db = Monstey_Gadb::NewFx(Monstey_FxDefaultPrefix(), schema);
    Monstey_FxWorkflowCall(
        *f.db, "fx_create_effect",
        J::object({{"name", "resources/fx/synthetic/integration.fxp"}, {"duration_ms", 5000}}));
    const auto prefab = Monstey_FxLibraryCall(*f.db, "fx_prefab_create",
                                              J::object({{"name", "Integration template"},
                                                         {"class_name", "ClientFX.Template"},
                                                         {"values", Monstey_Values()}}));
    f.prefab = prefab["result"]["prefab"].as_string();
    f.template_record = prefab["result"]["record"].as_string();
    f.child = Monstey_FxLibraryCall(*f.db, "fx_prefab_add_child",
                                    J::object({{"prefab", f.prefab}, {"time", 2.0}}))["result"]["record"]
                  .as_string();
    return f;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        const auto out =
            (argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(L"fx-library-tests")) /
            (L"FxLibrary_" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(out);
        const auto schema = out / L"schema.json";
        Monstey_WriteNewText(schema, Monstey_Schema().dump(2));
        auto db = Monstey_Gadb::NewFx(Monstey_FxDefaultPrefix(), schema);
        auto report = J::object({{"status", "PASS"},
                                 {"python_product_runtime", false},
                                 {"native_parent_attachment_claimed", false},
                                 {"cases", J::array()}});
        const auto test = [&](std::string_view name, const std::function<void()> &action) {
            try {
                action();
            } catch (const std::exception &e) {
                throw Monstey_Error("test", std::string(name) + ": " + e.what());
            }
            report["cases"].as_array().push_back(std::string(name));
        };
        const auto reject = [&](const std::function<void()> &action) {
            bool fail = false;
            try {
                action();
            } catch (const std::exception &) {
                fail = true;
            }
            Monstey_Require(fail, "Expected transactional prefab rejection");
        };
        const auto call = [&](std::string_view op, const J &a = J::object()) {
            return Monstey_FxLibraryCall(*db, op, a);
        };
        std::string group, prefab, record, child, derived, derivedRecord, source, fromKey;
        test("metadata_only_groups_preserve_empty_native_bytes", [&] {
            db->Build(out / L"before-group.fxp");
            group =
                call("fx_library_group_create", J::object({{"name", "Spectral library"}}))["result"]["group"]
                    .as_string();
            db->Build(out / L"after-group.fxp");
            Monstey_Require(Monstey_ReadFile(out / L"before-group.fxp") ==
                                Monstey_ReadFile(out / L"after-group.fxp"),
                            "Group invented native records");
        });
        test("new_component_prefab_requires_no_global_and_is_inactive", [&] {
            const auto r = call("fx_prefab_create", J::object({{"name", "Root template"},
                                                               {"class_name", "ClientFX.Template"},
                                                               {"values", Monstey_Values()},
                                                               {"group", group}}));
            prefab = r["result"]["prefab"].as_string();
            record = r["result"]["record"].as_string();
            Monstey_Require(!Monstey_FxWorkflowCall(*db, "fx_workflow")["effect_exists"].as_bool() &&
                                db->Describe()["clone_count"].as_uint() == 0,
                            "Template invented global/clone");
        });
        test("missing_prefab_defaults_roll_back_metadata_and_records", [&] {
            const auto before = db->Changes();
            reject([&] {
                call("fx_prefab_create",
                     J::object({{"name", "Missing values"}, {"class_name", "ClientFX.Template"}}));
            });
            Monstey_Require(db->Changes() == before, "Failed prefab left partial state");
        });
        test("prefab_from_key_materializes_independent_properties_and_unknown_fields", [&] {
            source =
                Monstey_FxCreateElement(*db, J::object({{"name", "Source key"},
                                                        {"class_name", "ClientFX.Template"},
                                                        {"values", Monstey_Values()}}))["result"]["selector"]
                    .as_string();
            db->AddField(source, "FutureProperty", 3, 77, J::object({{"array", 1}}));
            const auto r =
                call("fx_prefab_from_key", J::object({{"name", "Copied component"}, {"record", source}}));
            fromKey = r["result"]["prefab"].as_string();
            const auto copied = r["result"]["record"].as_string();
            db->SetField(source, "Strength", 99.0);
            Monstey_Require(Monstey_Value(*db, copied, "Strength").as_number() == 1.0 &&
                                Monstey_Value(*db, copied, "FutureProperty").as_int() == 77,
                            "Prefab aliases source or drops unknown field");
        });
        test("derived_child_key_has_fresh_identity_and_original_cursor_timing", [&] {
            Monstey_FxWorkflowCall(
                *db, "fx_create_effect",
                J::object({{"name", "resources/fx/synthetic/library.fxp"}, {"duration_ms", 20000}}));
            child = call("fx_prefab_add_child",
                         J::object(
                             {{"prefab", prefab}, {"name", "Child key"}, {"time", 10.0}}))["result"]["record"]
                        .as_string();
            Monstey_Require(db->Resolve(child) != db->Resolve(record) &&
                                Monstey_Value(*db, child, "StartTime").as_number() == 9 &&
                                Monstey_Value(*db, child, "EndTime").as_number() == 11,
                            "Child key did not retain original cursor-centred creation");
        });
        test("prefab_updates_propagate_without_overwriting_key_identity_or_timing", [&] {
            call("fx_prefab_override",
                 J::object({{"record", record}, {"field", "Strength"}, {"value", 7.0}}));
            Monstey_Require(Monstey_Value(*db, child, "Strength").as_number() == 7 &&
                                Monstey_Value(*db, child, "KeyName").as_string() == "Child key" &&
                                Monstey_Value(*db, child, "StartTime").as_number() == 9,
                            "Inheritance clobbered local key identity/time");
        });
        test("explicit_overrides_and_inherit_reset_match_property_set_semantics", [&] {
            call("fx_prefab_override",
                 J::object({{"record", child}, {"field", "Strength"}, {"value", 11.0}}));
            call("fx_prefab_override",
                 J::object({{"record", record}, {"field", "Strength"}, {"value", 9.0}}));
            Monstey_Require(Monstey_Value(*db, child, "Strength").as_number() == 11,
                            "Parent overwrote explicit instance override");
            call("fx_prefab_inherit", J::object({{"record", child}, {"field", "Strength"}}));
            Monstey_Require(Monstey_Value(*db, child, "Strength").as_number() == 9,
                            "Inherit did not restore parent property");
        });
        test("direct_typed_editor_changes_are_adopted_as_local_overrides", [&] {
            db->SetField(child, "Strength", 13.0);
            call("fx_prefab_override",
                 J::object({{"record", record}, {"field", "Strength"}, {"value", 10.0}}));
            Monstey_Require(Monstey_Value(*db, child, "Strength").as_number() == 13,
                            "Prefab sync discarded direct editor change");
            call("fx_prefab_inherit", J::object({{"record", child}, {"field", "Strength"}}));
        });
        test("new_prefab_defaults_remain_inheritable_but_authored_values_remain_local", [&] {
            const auto r = call("fx_prefab_create", J::object({{"name", "Derived template"},
                                                               {"class_name", "ClientFX.Template"},
                                                               {"values", Monstey_Values()}}));
            derived = r["result"]["prefab"].as_string();
            derivedRecord = r["result"]["record"].as_string();
            call("fx_prefab_parent", J::object({{"prefab", derived}, {"parent", prefab}}));
            Monstey_Require(Monstey_Value(*db, derivedRecord, "Strength").as_number() == 10,
                            "Unmodified class default shadowed parent");
            call("fx_prefab_override",
                 J::object({{"record", record}, {"field", "Strength"}, {"value", 12.0}}));
            Monstey_Require(Monstey_Value(*db, derivedRecord, "Strength").as_number() == 12,
                            "Derived prefab did not follow parent");
        });
        test("assigning_prefab_preserves_existing_explicit_values_unless_requested", [&] {
            call("fx_prefab_assign", J::object({{"record", source}, {"prefab", prefab}}));
            Monstey_Require(Monstey_Value(*db, source, "Strength").as_number() == 99,
                            "Assign discarded existing explicit override");
            call("fx_prefab_assign",
                 J::object({{"record", source}, {"prefab", prefab}, {"keep_overrides", false}}));
            Monstey_Require(Monstey_Value(*db, source, "Strength").as_number() == 12,
                            "Explicit full inheritance was not applied");
        });
        test("native_component_type_mismatch_and_parent_cycles_are_atomic", [&] {
            const auto other =
                Monstey_FxCreateElement(*db, J::object({{"name", "Other type"},
                                                        {"class_name", "ClientFX.Other"},
                                                        {"values", Monstey_Values()}}))["result"]["selector"]
                    .as_string();
            const auto before = db->Changes();
            reject([&] { call("fx_prefab_assign", J::object({{"record", other}, {"prefab", prefab}})); });
            reject([&] { call("fx_prefab_parent", J::object({{"prefab", prefab}, {"parent", derived}})); });
            Monstey_Require(db->Changes() == before, "Invalid inheritance left partial native edits");
        });
        test("stale_or_spoofed_component_type_metadata_rejects_without_native_edits", [&] {
            const auto before = db->Changes();
            auto metadata = db->ProjectMetadata();
            metadata["fx_library"]["prefabs"][derived]["class_name"] = "ClientFX.Other";
            db->SetProjectMetadata(metadata);
            const auto staged = db->Changes();
            reject([&] { call("fx_prefab_sync"); });
            Monstey_Require(db->Changes() == staged,
                            "Rejected type metadata changed native component fields");
            db->Undo();
            Monstey_Require(db->Changes() == before, "Metadata rollback did not restore original type owner");
        });
        test("detach_materializes_values_and_breaks_only_editor_inheritance", [&] {
            const auto value = Monstey_Value(*db, child, "Strength");
            call("fx_prefab_detach", J::object({{"record", child}}));
            call("fx_prefab_override",
                 J::object({{"record", record}, {"field", "Strength"}, {"value", 14.0}}));
            Monstey_Require(Monstey_Value(*db, child, "Strength") == value &&
                                !Monstey_Lib(*db)["instances"].contains(Monstey_Hex(db->Resolve(child))),
                            "Detached native properties changed");
        });
        test("inheritance_change_and_propagation_are_one_undo_step", [&] {
            const auto before = db->Changes();
            call("fx_prefab_override",
                 J::object({{"record", record}, {"field", "Strength"}, {"value", 15.0}}));
            db->Undo();
            Monstey_Require(db->Changes() == before, "Propagation required multiple undo steps");
            db->Redo();
        });
        test("group_moves_are_metadata_only_and_reject_parent_cycles", [&] {
            const auto nested = call("fx_library_group_create",
                                     J::object({{"name", "Nested"}, {"parent", group}}))["result"]["group"]
                                    .as_string();
            const auto before = db->Changes();
            reject([&] { call("fx_library_group_move", J::object({{"group", group}, {"parent", nested}})); });
            Monstey_Require(db->Changes() == before, "Group cycle partially changed metadata");
            call("fx_library_group_move", J::object({{"group", nested}, {"parent", ""}}));
            Monstey_Require(Monstey_Lib(*db)["groups"][nested]["parent"].as_string().empty(),
                            "Group move failed");
        });
        test("active_prefab_drives_original_add_child_command", [&] {
            call("fx_prefab_set_active", J::object({{"prefab", derived}}));
            const auto key =
                call("fx_prefab_add_child",
                     J::object({{"name", "Active prefab child"}, {"time", 5.0}}))["result"]["record"]
                    .as_string();
            Monstey_Require(Monstey_Value(*db, key, "StartTime").as_number() == 4.0 &&
                                Monstey_Lib(*db)["instances"][key]["prefab"].as_string() == derived,
                            "Add-child ignored active prefab");
        });
        test("deleting_prefab_materializes_direct_children_and_preserves_live_keys", [&] {
            const auto inherited = Monstey_Value(*db, derivedRecord, "Strength");
            const auto sourceValue = Monstey_Value(*db, source, "Strength");
            call("fx_prefab_delete", J::object({{"prefab", prefab}}));
            const auto state = Monstey_Lib(*db);
            Monstey_Require(!state["prefabs"].contains(prefab) &&
                                state["prefabs"][derived]["parent"].as_string().empty() &&
                                Monstey_Value(*db, derivedRecord, "Strength") == inherited &&
                                Monstey_Value(*db, source, "Strength") == sourceValue &&
                                !state["instances"].contains(Monstey_Hex(db->Resolve(source))),
                            "Prefab deletion left dangling ownership or lost effective properties");
        });
        test("recursive_group_delete_keeps_materialized_instances_and_unknown_fields", [&] {
            const auto scope =
                call("fx_library_group_create", J::object({{"name", "Delete scope"}}))["result"]["group"]
                    .as_string();
            const auto nested =
                call("fx_library_group_create",
                     J::object({{"name", "Delete nested"}, {"parent", scope}}))["result"]["group"]
                    .as_string();
            const auto duplicate =
                call("fx_prefab_duplicate", J::object({{"prefab", fromKey},
                                                       {"name", "Independent duplicate"},
                                                       {"group", nested}}))["result"]["prefab"]
                    .as_string();
            const auto key = call("fx_prefab_add_child",
                                  J::object({{"prefab", duplicate}, {"time", 8.0}}))["result"]["record"]
                                 .as_string();
            Monstey_Require(Monstey_Value(*db, key, "FutureProperty").as_int() == 77,
                            "Derived unknown property disappeared");
            const auto before = db->Changes();
            reject([&] {
                call("fx_library_group_delete", J::object({{"group", scope}, {"recursive", false}}));
            });
            Monstey_Require(db->Changes() == before, "Rejected nonempty group deletion changed state");
            call("fx_library_group_delete", J::object({{"group", scope}}));
            const auto state = Monstey_Lib(*db);
            Monstey_Require(!state["groups"].contains(scope) && !state["groups"].contains(nested) &&
                                !state["prefabs"].contains(duplicate) &&
                                Monstey_Value(*db, key, "FutureProperty").as_int() == 77 &&
                                !state["instances"].contains(key),
                            "Recursive delete lost materialized key state");
        });
        test("project_and_native_build_readback_preserve_materialized_templates_and_keys", [&] {
            db->SaveProject(out / L"library-project.json");
            auto loaded = Monstey_Gadb::LoadProject(out / L"library-project.json");
            Monstey_Require(Monstey_Lib(*loaded) == Monstey_Lib(*db),
                            "Project lost prefab ownership/overrides");
            loaded->Build(out / L"library-built.fxp");
            Monstey_Gadb native(out / L"library-built.fxp", schema);
            Monstey_Require(Monstey_FxWorkflowCall(native, "fx_workflow")["active_keys"].size() ==
                                Monstey_FxWorkflowCall(*db, "fx_workflow")["active_keys"].size(),
                            "Native template records became active or key disappeared");
            report["native_built_sha256"] = Monstey_HashSha256(out / L"library-built.fxp");
        });
        test("generic_typed_template_edit_propagates_and_undoes_as_one_operation", [&] {
            auto f = Monstey_IntegratedFixture(schema);
            const auto before = f.db->Changes();
            const auto result = Monstey_FxLibraryNativeCall(
                *f.db, "fx_set",
                J::object({{"record", f.template_record}, {"field", "Strength"}, {"value", 22.0}}));
            Monstey_Require(result["field"].as_string() == "Strength" && result["value"].as_number() == 22 &&
                                Monstey_Value(*f.db, f.child, "Strength").as_number() == 22,
                            "Native edit result shape or materialized propagation changed");
            f.db->Undo();
            Monstey_Require(f.db->Changes() == before,
                            "Generic edit and propagation required multiple undo steps");
        });
        test("generic_instance_edits_become_local_overrides", [&] {
            auto f = Monstey_IntegratedFixture(schema);
            Monstey_FxLibraryNativeCall(
                *f.db, "fx_set", J::object({{"record", f.child}, {"field", "Strength"}, {"value", 33.0}}));
            Monstey_FxLibraryNativeCall(
                *f.db, "fx_set",
                J::object({{"record", f.template_record}, {"field", "Strength"}, {"value", 44.0}}));
            Monstey_Require(Monstey_Value(*f.db, f.child, "Strength").as_number() == 33,
                            "Generic typed edit was not adopted as local override");
        });
        test("generic_template_and_instance_deletion_leave_no_library_dangling_records", [&] {
            auto f = Monstey_IntegratedFixture(schema);
            const auto value = Monstey_Value(*f.db, f.child, "Strength");
            Monstey_FxLibraryNativeCall(*f.db, "fx_delete_element",
                                        J::object({{"record", f.template_record}}));
            Monstey_FxLibraryCall(*f.db, "fx_prefab_sync");
            Monstey_Require(Monstey_Lib(*f.db)["prefabs"].empty() &&
                                Monstey_Lib(*f.db)["instances"].empty() &&
                                Monstey_Value(*f.db, f.child, "Strength") == value,
                            "Generic template deletion lost live values or retained stale ownership");
            Monstey_FxLibraryNativeCall(*f.db, "fx_delete_element", J::object({{"record", f.child}}));
            Monstey_FxLibraryCall(*f.db, "fx_prefab_sync");
            Monstey_FxLibraryNativeCall(
                *f.db, "fx_build",
                J::object({{"output", Monstey_Utf8((out / L"generic-delete.fxp").wstring())}}));
        });
        test("generic_effect_deletion_cleans_active_template_ownership_and_retains_inactive_library", [&] {
            auto f = Monstey_IntegratedFixture(schema);
            const auto survivor =
                Monstey_FxLibraryCall(
                    *f.db, "fx_prefab_duplicate",
                    J::object({{"prefab", f.prefab}, {"name", "Inactive survivor"}}))["result"]["prefab"]
                    .as_string();
            Monstey_FxWorkflowCall(*f.db, "fx_link_key", J::object({{"record", f.template_record}}));
            Monstey_FxLibraryNativeCall(*f.db, "fx_delete_effect");
            Monstey_FxLibraryCall(*f.db, "fx_prefab_sync");
            const auto state = Monstey_Lib(*f.db);
            Monstey_Require(!Monstey_FxWorkflowCall(*f.db, "fx_workflow")["effect_exists"].as_bool() &&
                                state["instances"].empty() && !state["prefabs"].contains(f.prefab) &&
                                state["prefabs"].contains(survivor),
                            "Effect deletion retained stale prefab ownership");
            const auto project = out / L"deleted-effect-library.json";
            Monstey_FxLibraryNativeCall(*f.db, "fx_save_project",
                                        J::object({{"path", Monstey_Utf8(project.wstring())}}));
            auto loaded = Monstey_Gadb::LoadProject(project);
            Monstey_FxLibraryCall(*loaded, "fx_prefab_sync");
            loaded->Build(out / L"deleted-effect-library.fxp");
        });
        test("build_and_project_writers_sync_direct_editor_changes_and_preserve_result_fields", [&] {
            auto f = Monstey_IntegratedFixture(schema);
            f.db->SetField(f.template_record, "Strength", 55.0);
            const auto project = out / L"integrated-writer.json";
            const auto saved = Monstey_FxLibraryNativeCall(
                *f.db, "fx_save_project", J::object({{"path", Monstey_Utf8(project.wstring())}}));
            Monstey_Require(saved.contains("project_path") && saved.contains("source_sha256") &&
                                Monstey_Value(*f.db, f.child, "Strength").as_number() == 55,
                            "Project writer lost its result contract or materialized values");
            f.db->SetField(f.template_record, "Strength", 66.0);
            const auto native = out / L"integrated-writer.fxp";
            const auto built = Monstey_FxLibraryNativeCall(
                *f.db, "fx_build", J::object({{"output", Monstey_Utf8(native.wstring())}}));
            Monstey_Require(built.contains("path") &&
                                Monstey_Value(*f.db, f.child, "Strength").as_number() == 66,
                            "Build writer lost output contract or failed to sync inheritance");
        });
        test("from_key_and_detach_capture_latest_effective_inherited_values", [&] {
            auto f = Monstey_IntegratedFixture(schema);
            f.db->SetField(f.template_record, "Strength", 77.0);
            const auto copy =
                Monstey_FxLibraryCall(*f.db, "fx_prefab_from_key",
                                      J::object({{"record", f.child},
                                                 {"name", "Current effective snapshot"}}))["result"]["record"]
                    .as_string();
            Monstey_Require(Monstey_Value(*f.db, copy, "Strength").as_number() == 77,
                            "From-key copied stale materialized values");
            f.db->SetField(f.template_record, "Strength", 88.0);
            Monstey_FxLibraryCall(*f.db, "fx_prefab_detach", J::object({{"record", f.child}}));
            Monstey_Require(Monstey_Value(*f.db, f.child, "Strength").as_number() == 88,
                            "Detach preserved stale inherited values");
        });
        if (argc > 3)
            test("native_component_prefab_child_override_project_and_compiled_readback", [&] {
                const auto nativePath = std::filesystem::path(argv[2]),
                           nativeSchema = std::filesystem::path(argv[3]);
                const auto sourceHash = Monstey_HashSha256(nativePath);
                Monstey_Gadb real(nativePath, nativeSchema);
                const auto before = Monstey_FxWorkflowCall(real, "fx_workflow");
                Monstey_Require(!before["active_keys"].empty(), "Native effect has no source component");
                const auto original = before["active_keys"][0u]["selector"].as_string();
                Monstey_Require(real.Metadata(original)["category"].as_string() == "ClientFX.DynaLight",
                                "Native qualification expects the pinned DynaLight source");
                const auto templ = Monstey_FxLibraryCall(
                    real, "fx_prefab_from_key",
                    J::object({{"name", "Native light template"}, {"record", original}}));
                const auto prefabId = templ["result"]["prefab"].as_string(),
                           templateRef = templ["result"]["record"].as_string();
                Monstey_FxWorkflowCall(real, "fx_set_duration", J::object({{"duration_ms", 5000}}));
                const auto created = Monstey_FxLibraryCall(
                    real, "fx_prefab_add_child",
                    J::object({{"prefab", prefabId}, {"time", 3.0}, {"name", "Native derived child"}}));
                const auto derivedKey = created["result"]["record"].as_string();
                Monstey_FxLibraryCall(real, "fx_prefab_override",
                                      J::object({{"record", templateRef},
                                                 {"field", "Radius"},
                                                 {"value", J::object({{"constant", 7.5}})}}));
                const auto inheritedRadius = Monstey_Value(real, derivedKey, "Radius");
                for (const auto &knot : inheritedRadius["knots"].as_array())
                    Monstey_Require(knot["value"].as_number() == 7.5,
                                    "Native radius override did not propagate");
                Monstey_FxLibraryNativeCall(
                    real, "fx_save_project",
                    J::object({{"path", Monstey_Utf8((out / L"native-library-project.json").wstring())}}));
                auto loaded = Monstey_Gadb::LoadProject(out / L"native-library-project.json");
                Monstey_Require(Monstey_Lib(*loaded) == Monstey_Lib(real),
                                "Native project lost prefab overrides");
                const auto target = out / L"native-library-built.fxp";
                const auto directTarget = out / L"native-library-direct.fxp";
                real.Build(directTarget);
                loaded->Build(target);
                Monstey_Require(Monstey_ReadFile(directTarget) == Monstey_ReadFile(target),
                                "Native prefab project replay changed compiled bytes");
                report["native_project_rebuild_byte_exact"] = true;
                Monstey_Gadb readback(target, nativeSchema);
                const auto active = Monstey_FxWorkflowCall(readback, "fx_workflow")["active_keys"];
                bool found = false;
                for (const auto &key : active.as_array())
                    if (Monstey_Value(readback, key["selector"].as_string(), "KeyName").as_string() ==
                        "Native derived child") {
                        const auto selector = key["selector"].as_string();
                        found = Monstey_Value(readback, selector, "StartTime").as_number() == 2.0 &&
                                Monstey_Value(readback, selector, "EndTime").as_number() == 4.0;
                        const auto compiledRadius = Monstey_Value(readback, selector, "Radius");
                        for (const auto &knot : compiledRadius["knots"].as_array())
                            found &= knot["value"].as_number() == 7.5;
                    }
                Monstey_Require(
                    found && active.size() == before["active_keys"].size() + 1 &&
                        Monstey_HashSha256(nativePath) == sourceHash,
                    "Native prefab compilation lost the child/materialized values or changed source");
                report["native_source_sha256"] = sourceHash;
                report["native_built_sha256"] = Monstey_HashSha256(target);
                report["game_runtime_verified"] = false;
            });
        report["passed"] = static_cast<std::uint64_t>(report["cases"].size());
        Monstey_WriteNewText(out / L"fx-library-tests.json", report.dump(2));
        std::cout << "Native FX library tests: " << report["passed"].as_uint() << " passed\n"
                  << Monstey_Utf8(out.wstring()) << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
