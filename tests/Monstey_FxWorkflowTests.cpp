#include "Monstey_FxWorkflow.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
J Monstey_Field(std::string name, unsigned type, J value, int array = 1) {
    return J::object({{"name", name}, {"type_code", type}, {"array", array}, {"default", value}});
}
J Monstey_FixtureSchema() {
    auto global =
        J::array({Monstey_Field("FxName", 4, nullptr), Monstey_Field("ResourceLenMS", 3, 0),
                  Monstey_Field("KeyList", 10, nullptr, -1), Monstey_Field("MaxUpdateDistance", 2, 0.0),
                  Monstey_Field("PauseWhenNotVisible", 1, true)});
    auto element =
        J::array({Monstey_Field("KeyName", 4, nullptr), Monstey_Field("StartTime", 2, 0.0),
                  Monstey_Field("EndTime", 2, 1.0), Monstey_Field("FloatCurve", 13, nullptr, -1),
                  Monstey_Field("ColorCurve", 12, nullptr, -1), Monstey_Field("IntCurve", 15, nullptr, -1),
                  Monstey_Field("VectorCurve", 16, nullptr, -1), Monstey_Field("Asset", 4, ""),
                  Monstey_Field("Enabled", 1, true), Monstey_Field("Target", 10, nullptr)});
    return J::object({{"categories", J::object({{"FxGlobalProps", J::object({{"fields", global}})},
                                                {"ClientFX.Synthetic", J::object({{"fields", element}})}})}});
}
J Monstey_InitialValues() {
    return J::object({{"FloatCurve", J::object({{"constant", 2.5}})},
                      {"ColorCurve", J::object({{"constant", 0xff123456u}})},
                      {"IntCurve", J::object({{"constant", 7}})},
                      {"VectorCurve", J::object({{"constant", J::array({1.0, 2.0, 3.0})}})}});
}
void Monstey_Write(const std::filesystem::path &path, const Monstey_Bytes &bytes) {
    Monstey_WriteNewFile(path, bytes);
}
void Monstey_WriteJson(const std::filesystem::path &path, const J &value) {
    const auto s = value.dump(2);
    Monstey_Write(path, Monstey_Bytes(s.begin(), s.end()));
}
J Monstey_Value(Monstey_Gadb &db, std::string_view selector, std::string_view name) {
    const auto meta = db.Metadata(selector);
    for (const auto &f : meta["fields"].as_array())
        if (f["hash"].as_string() == Monstey_Hex(Monstey_NativeHash(name)))
            return f["value"];
    throw Monstey_Error("test", "Field missing: " + std::string(name));
}
J Monstey_View(Monstey_Gadb &db) { return Monstey_FxWorkflowCall(db, "fx_workflow"); }
std::string Monstey_Ref(const J &report) { return report["result"]["selector"].as_string(); }
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        const auto base =
            argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(L"fx-workflow-tests");
        const auto out =
            base /
            (L"FxWorkflow_" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(out);
        auto report = J::object({{"status", "PASS"},
                                 {"python_runtime", false},
                                 {"game_runtime_verified", false},
                                 {"cases", J::array()}});
        const auto test = [&](std::string_view name, const std::function<void()> &f) {
            try {
                f();
            } catch (const std::exception &e) {
                throw Monstey_Error("test", std::string(name) + ": " + e.what());
            }
            report["cases"].as_array().push_back(std::string(name));
        };
        const auto reject = [&](const std::function<void()> &f) {
            bool bad = false;
            try {
                f();
            } catch (const std::exception &) {
                bad = true;
            }
            Monstey_Require(bad, "Expected transactional FX failure");
        };
        const auto schema = out / L"schema.json";
        Monstey_WriteJson(schema, Monstey_FixtureSchema());
        const auto prefix = Monstey_FxDefaultPrefix();
        auto empty = Monstey_Gadb::NewFx(prefix, schema);
        const auto prefixSource = out / L"prefix-template.fxp";
        empty->Build(prefixSource);
        test("qualified_prefix_reads_native_envelope_and_checks_snapshot_hash", [&] {
            Monstey_Require(Monstey_FxQualifiedPrefix(prefixSource, Monstey_HashSha256(prefixSource)) ==
                                prefix,
                            "Qualified LTFX prefix changed");
            reject([&] { Monstey_FxQualifiedPrefix(prefixSource, std::string(64, '0')); });
            const auto bad = out / L"bad-prefix.fxp";
            Monstey_Write(bad, Monstey_Bytes{'L', 'T', 'F', 'X', 9, 0, 0, 0});
            reject([&] { Monstey_FxQualifiedPrefix(bad); });
        });
        test("native_schema_required_values_exclude_editor_group_markers", [&] {
            auto fixture = Monstey_FixtureSchema();
            auto &definition = fixture["categories"]["ClientFX.Synthetic"];
            auto &fields = definition["fields"].as_array();
            for (auto &field : fields) {
                field["flags"] = 0;
                field["ordinal"] = 77;
            }
            auto begin = Monstey_Field("BeginGroup", 4, nullptr);
            begin["flags"] = 0x80000080u;
            auto end = Monstey_Field("BeginGroup", 4, nullptr);
            end["flags"] = 0x200u;
            fields.insert(fields.begin() + 3, begin);
            fields.push_back(end);
            definition["field_count"] = fields.size();
            definition["flags"] = "0x100000000";
            const auto path = out / L"marker-schema.json";
            Monstey_WriteJson(path, fixture);
            auto database = Monstey_Gadb::NewFx(prefix, path);
            const auto requested = Monstey_FxWorkflowCall(*database, "fx_schema",
                                                          J::object({{"class_name", "ClientFX.Synthetic"}}));
            Monstey_Require(requested["native_fields"].size() == 10 &&
                                requested["required_initial_values"].size() == 4,
                            "Editor-only marker appeared in required native values");
            const auto created =
                Monstey_FxCreateElement(*database, J::object({{"class_name", "ClientFX.Synthetic"},
                                                              {"name", "Canonical markers"},
                                                              {"values", Monstey_InitialValues()}}));
            const auto ref = created["result"]["selector"].as_string();
            const auto metadata = database->Metadata(ref);
            Monstey_Require(metadata["fields"].size() == 10 &&
                                metadata["fields"][3u]["native_ordinal"].as_uint() == 4 &&
                                Monstey_FxVerifyNativeBindings(*database)["all_records_compatible"].as_bool(),
                            "Marker gaps/physical order did not match native reflection");
        });
        test("independent_inactive_element_needs_no_global_or_keylist", [&] {
            auto library = Monstey_Gadb::NewFx(prefix, schema);
            const auto before = library->Changes();
            const auto created =
                Monstey_FxCreateElement(*library, J::object({{"class_name", "ClientFX.Synthetic"},
                                                             {"name", "Inactive template"},
                                                             {"values", Monstey_InitialValues()}}));
            const auto ref = created["result"]["selector"].as_string();
            Monstey_Require(!Monstey_View(*library)["effect_exists"].as_bool() &&
                                Monstey_View(*library)["unlinked_elements"].size() == 1 &&
                                Monstey_Value(*library, ref, "KeyName").as_string() == "Inactive template",
                            "Independent creation invented a global or an active key");
            const auto target = out / L"inactive-template.fxp";
            library->Build(target);
            Monstey_Gadb readback(target, schema);
            Monstey_Require(!Monstey_View(readback)["effect_exists"].as_bool() &&
                                Monstey_View(readback)["unlinked_elements"].size() == 1,
                            "Independent inactive element failed native readback");
            library->Undo();
            Monstey_Require(library->Changes() == before, "Independent creation is not one undo step");
        });
        test("failed_inactive_element_creation_restores_shared_state", [&] {
            auto library = Monstey_Gadb::NewFx(prefix, schema);
            const auto before = library->Changes();
            reject([&] {
                Monstey_FxCreateElement(*library, J::object({{"class_name", "ClientFX.Synthetic"},
                                                             {"name", "Missing explicit curve defaults"}}));
            });
            Monstey_Require(library->Changes() == before, "Failed inactive template left a partial record");
        });
        auto db = Monstey_Gadb::NewFx(Monstey_FxQualifiedPrefix(prefixSource), schema);
        const auto createEffect = [&](std::string name) {
            return Monstey_FxWorkflowCall(*db, "fx_create_effect",
                                          J::object({{"name", name}, {"duration_ms", 2000}}));
        };
        std::string global, alpha, beta, clone, trackA, trackB;
        test("independent_effect_creation_uses_empty_source_not_record_clone", [&] {
            const auto r = createEffect("resources/fx/synthetic/independent.fxp");
            global = r["result"]["global_record"].as_string();
            Monstey_Require(db->Describe()["clone_count"].as_uint() == 0 &&
                                Monstey_View(*db)["active_keys"].empty() &&
                                Monstey_Value(*db, global, "ResourceLenMS").as_int() == 2000,
                            "Independent effect did not use source-owned records");
            db->Undo();
            Monstey_Require(!Monstey_View(*db)["effect_exists"].as_bool(),
                            "One undo did not remove complete effect transaction");
            db->Redo();
            Monstey_Require(Monstey_View(*db)["effect_exists"].as_bool(),
                            "Redo did not restore effect graph");
        });
        test("native_class_schema_reports_required_explicit_property_defaults", [&] {
            const auto r = Monstey_FxWorkflowCall(*db, "fx_schema");
            Monstey_Require(r["classes"].size() == 1 &&
                                r["classes"][0u]["required_initial_values"].size() == 4,
                            "Class defaults were invented or omitted");
        });
        test("one_unqualified_class_does_not_block_qualified_schema_browser", [&] {
            auto fixture = Monstey_FixtureSchema();
            fixture["categories"]["ClientFX.Incomplete"] =
                J::object({{"field_count", 2},
                           {"unreadable_fields", 0},
                           {"fields", J::array({Monstey_Field("Known", 3, 0)})}});
            const auto path = out / L"incomplete-class-schema.json";
            Monstey_WriteJson(path, fixture);
            auto database = Monstey_Gadb::NewFx(prefix, path);
            const auto before = database->Changes();
            const auto listed = Monstey_FxWorkflowCall(*database, "fx_schema");
            Monstey_Require(listed["qualified_class_count"].as_uint() == 1 &&
                                listed["unqualified_class_count"].as_uint() == 1 &&
                                listed["total_class_count"].as_uint() == 2 &&
                                listed["classes"][0u]["class_name"].as_string() == "ClientFX.Synthetic" &&
                                listed["classes"][0u]["creation_qualified"].as_bool() &&
                                !listed["unqualified_classes"][0u]["creation_qualified"].as_bool() &&
                                !listed["unqualified_classes"][0u]["creation_diagnostic"].as_string().empty(),
                            "One incomplete class blocked or silently disappeared from the browser");
            reject([&] {
                Monstey_FxCreateElement(
                    *database, J::object({{"class_name", "ClientFX.Incomplete"}, {"name", "Must reject"}}));
            });
            Monstey_Require(database->Changes() == before, "Rejected unqualified creation mutated state");
        });
        test("binding_verifier_reports_incomplete_category_and_continues_qualified_records", [&] {
            auto fixture = Monstey_FixtureSchema();
            fixture["categories"]["ClientFX.Complete"] = J::object(
                {{"field_count", 3},
                 {"fields", J::array({Monstey_Field("KeyName", 4, ""), Monstey_Field("StartTime", 2, 0.0),
                                      Monstey_Field("EndTime", 2, 1.0)})}});
            for (auto &field : fixture["categories"]["ClientFX.Complete"]["fields"].as_array())
                field["flags"] = 0;
            fixture["categories"]["ClientFX.Incomplete"] =
                J::object({{"field_count", 2}, {"fields", J::array({Monstey_Field("Known", 3, 0)})}});
            const auto path = out / L"mixed-binding-schema.json";
            Monstey_WriteJson(path, fixture);
            auto database = Monstey_Gadb::NewFx(prefix, path);
            Monstey_FxCreateElement(
                *database, J::object({{"class_name", "ClientFX.Complete"}, {"name", "Qualified record"}}));
            const auto category =
                database->CreateCategory("ClientFX.Incomplete")["category"]["index"].as_uint();
            const auto record =
                database->CreateRecord(static_cast<std::uint32_t>(category), {}, true)["record"]["selector"]
                    .as_string();
            database->AddField(record, "Known", 3, 2);
            const auto before = database->Changes();
            const auto verified = Monstey_FxVerifyNativeBindings(*database);
            Monstey_Require(
                !verified["all_records_compatible"].as_bool() &&
                    !verified["qualification_complete"].as_bool() &&
                    verified["verified_records"].as_uint() == 1 &&
                    verified["unqualified_records"].as_uint() == 1 &&
                    verified["total_records"].as_uint() == 2 && verified["categories"].size() == 1 &&
                    verified["categories"][0u]["records"][0u]["compatible"].as_bool() &&
                    verified["unqualified_categories"].size() == 1 &&
                    verified["unqualified_categories"][0u]["category"].as_string() == "ClientFX.Incomplete" &&
                    database->Changes() == before,
                "Incomplete class aborted verification or changed native state");
        });
        test("missing_curve_defaults_roll_back_categories_records_and_history", [&] {
            const auto before = db->Changes();
            reject([&] {
                Monstey_FxWorkflowCall(
                    *db, "fx_create_element",
                    J::object({{"class_name", "ClientFX.Synthetic"}, {"name", "Missing"}}));
            });
            Monstey_Require(db->Changes() == before && Monstey_View(*db)["active_keys"].empty(),
                            "Failed creation left a partial native key graph");
        });
        test("independent_element_materializes_all_four_native_curve_types", [&] {
            const auto r = Monstey_FxWorkflowCall(*db, "fx_create_element",
                                                  J::object({{"class_name", "ClientFX.Synthetic"},
                                                             {"name", "Alpha"},
                                                             {"start", 0.0},
                                                             {"end", 1.0},
                                                             {"values", Monstey_InitialValues()}}));
            alpha = Monstey_Ref(r);
            const auto v = Monstey_View(*db);
            trackA = v["layout"]["tracks"][0u]["id"].as_string();
            Monstey_Require(db->Describe()["clone_count"].as_uint() == 0 && v["active_keys"].size() == 1 &&
                                Monstey_Value(*db, alpha, "Enabled").as_bool(),
                            "Element creation cloned or lost typed defaults");
            for (const auto name : {"FloatCurve", "ColorCurve", "IntCurve", "VectorCurve"})
                Monstey_Require(!Monstey_Value(*db, alpha, name).is_null(), "Native curve missing");
        });
        test("editor_only_track_creation_preserves_native_binary_identity", [&] {
            const auto a = out / L"before-track.fxp", b = out / L"after-track.fxp";
            db->Build(a);
            db->SetProjectMetadata(J::object({{"other_tool", J::object({{"keep", 42}})},
                                              {"fx_workflow", db->ProjectMetadata()["fx_workflow"]}}));
            const auto r =
                Monstey_FxWorkflowCall(*db, "fx_create_track", J::object({{"name", "Second track"}}));
            trackB = r["result"]["id"].as_string();
            db->Build(b);
            Monstey_Require(Monstey_ReadFile(a) == Monstey_ReadFile(b) &&
                                db->ProjectMetadata()["other_tool"]["keep"].as_int() == 42,
                            "Track metadata changed native payload or another tool's metadata");
        });
        test("linked_track_slots_share_native_record_without_cloning", [&] {
            Monstey_FxWorkflowCall(*db, "fx_link_key", J::object({{"record", alpha}, {"track", trackB}}));
            const auto v = Monstey_View(*db);
            Monstey_Require(v["active_keys"].size() == 2 &&
                                v["active_keys"][0u]["reference"] == v["active_keys"][1u]["reference"] &&
                                db->Describe()["clone_count"].as_uint() == 0,
                            "Key links silently created a second record");
        });
        test("retime_updates_shared_key_and_is_one_undo_transaction", [&] {
            Monstey_FxWorkflowCall(*db, "fx_retime_key",
                                   J::object({{"record", alpha}, {"start", 0.25}, {"end", 1.5}}));
            Monstey_Require(Monstey_Value(*db, alpha, "StartTime").as_number() == .25 &&
                                Monstey_View(*db)["active_keys"].size() == 2,
                            "Linked key retime failed");
            db->Undo();
            Monstey_Require(Monstey_Value(*db, alpha, "StartTime").as_number() == 0 &&
                                Monstey_Value(*db, alpha, "EndTime").as_number() == 1,
                            "Retiming required multiple undo steps");
            db->Redo();
            const auto before = db->Changes();
            reject([&] {
                Monstey_FxWorkflowCall(*db, "fx_retime_key",
                                       J::object({{"record", alpha}, {"start", 2.0}, {"end", 1.0}}));
            });
            Monstey_Require(db->Changes() == before, "Invalid retime was not rolled back");
        });
        test("unlink_and_delete_track_keep_effect_owned_elements", [&] {
            Monstey_FxWorkflowCall(*db, "fx_unlink_key", J::object({{"track", trackA}, {"index", 0}}));
            Monstey_Require(Monstey_View(*db)["active_keys"].size() == 1,
                            "Unlink removed wrong number of slots");
            Monstey_FxWorkflowCall(*db, "fx_delete_track", J::object({{"track", trackB}}));
            Monstey_Require(Monstey_View(*db)["active_keys"].empty() &&
                                Monstey_View(*db)["unlinked_elements"].size() == 1 &&
                                db->Resolve(alpha) < UINT32_MAX,
                            "Deleting track deleted its native element");
        });
        test("duplicate_is_intentional_independent_record_and_preserves_unknown_fields", [&] {
            db->AddField(alpha, "UnknownNativeValue", 3, 0x1234567, J::object({{"array", 1}}));
            const auto r = Monstey_FxWorkflowCall(
                *db, "fx_duplicate_element",
                J::object({{"source", alpha}, {"name", "AlphaCopy"}, {"track", trackA}}));
            clone = Monstey_Ref(r);
            Monstey_Require(db->Resolve(clone) != db->Resolve(alpha) &&
                                Monstey_Value(*db, clone, "UnknownNativeValue").as_int() == 0x1234567,
                            "Duplicate shared storage or lost unknown attributes");
            Monstey_FxWorkflowCall(*db, "fx_rename_element",
                                   J::object({{"record", clone}, {"name", "Copy renamed"}}));
            Monstey_Require(Monstey_Value(*db, alpha, "KeyName").as_string() == "Alpha",
                            "Renaming duplicate changed original");
        });
        test("track_and_key_reordering_flatten_actual_native_keylist", [&] {
            const auto r = Monstey_FxWorkflowCall(*db, "fx_create_element",
                                                  J::object({{"class_name", "ClientFX.Synthetic"},
                                                             {"name", "Beta"},
                                                             {"values", Monstey_InitialValues()},
                                                             {"track", trackA}}));
            beta = Monstey_Ref(r);
            Monstey_FxWorkflowCall(*db, "fx_move_key",
                                   J::object({{"track", trackA}, {"index", 1}, {"to", 0}}));
            auto keys = Monstey_Value(*db, global, "KeyList");
            Monstey_Require(db->Resolve(keys[0u].as_string()) == db->Resolve(beta),
                            "Key reorder did not update native list");
            const auto tr = Monstey_FxWorkflowCall(*db, "fx_create_track",
                                                   J::object({{"name", "Alias track"}, {"index", 0}}));
            trackB = tr["result"]["id"].as_string();
            Monstey_FxWorkflowCall(*db, "fx_link_key", J::object({{"record", alpha}, {"track", trackB}}));
            Monstey_FxWorkflowCall(*db, "fx_move_track", J::object({{"track", trackB}, {"index", 1}}));
            keys = Monstey_Value(*db, global, "KeyList");
            Monstey_Require(db->Resolve(keys[2u].as_string()) == db->Resolve(alpha),
                            "Track reorder did not flatten in native order");
        });
        test("physical_delete_handles_all_links_and_typed_incoming_references", [&] {
            db->SetField(beta, "Target", alpha);
            const auto before = db->Changes();
            Monstey_FxWorkflowCall(*db, "fx_delete_element", J::object({{"record", alpha}}));
            reject([&] { db->Build(out / "unresolved-delete.fxp"); });
            Monstey_Require(!std::filesystem::exists(out / "unresolved-delete.fxp"),
                            "Unresolved typed reference was published");
            db->Undo();
            Monstey_Require(db->Changes() == before, "Undo did not restore physical delete and track links");
            Monstey_FxWorkflowCall(*db, "fx_delete_element",
                                   J::object({{"record", alpha}, {"replacement", clone}}));
            const auto keys = Monstey_Value(*db, global, "KeyList");
            Monstey_Require(
                std::none_of(keys.as_array().begin(), keys.as_array().end(),
                             [&](const auto &v) { return !v.is_null() && v.as_string() == alpha; }),
                "Deleted key remained linked");
        });
        test("native_element_fields_add_remove_and_reorder_share_gadb_codec", [&] {
            Monstey_FxWorkflowCall(
                *db, "fx_add_field",
                J::object({{"record", beta}, {"field", "Extra"}, {"native_type", 3}, {"value", 99}}));
            Monstey_FxWorkflowCall(*db, "fx_move_field",
                                   J::object({{"record", beta}, {"field", "Extra"}, {"ordinal", 0}}));
            Monstey_Require(db->Metadata(beta)["fields"][0u]["name"].as_string() == "Extra",
                            "FX field order did not change");
            Monstey_FxWorkflowCall(*db, "fx_delete_field", J::object({{"record", beta}, {"field", "Extra"}}));
        });
        test("outside_native_keylist_edits_require_explicit_layout_reconciliation", [&] {
            auto keys = Monstey_Value(*db, global, "KeyList");
            std::reverse(keys.as_array().begin(), keys.as_array().end());
            db->SetField(global, "KeyList", keys);
            Monstey_Require(!Monstey_View(*db)["layout_synchronized"].as_bool(),
                            "Outside native order edit was hidden");
            reject([&] {
                Monstey_FxWorkflowCall(*db, "fx_create_track", J::object({{"name", "Would overwrite"}}));
            });
            Monstey_FxWorkflowCall(*db, "fx_reconcile_layout");
            Monstey_Require(Monstey_View(*db)["layout_synchronized"].as_bool(),
                            "Explicit reconciliation failed");
        });
        test("project_roundtrip_preserves_envelope_tracks_links_unknown_fields_and_history", [&] {
            const auto project = out / L"workflow-project.json";
            db->SaveProject(project);
            auto loaded = Monstey_Gadb::LoadProject(project);
            Monstey_Require(loaded->Describe()["format"].as_string() == "LTFX8/GADB17" &&
                                loaded->ProjectMetadata() == db->ProjectMetadata() &&
                                Monstey_View(*loaded)["active_keys"].size() ==
                                    Monstey_View(*db)["active_keys"].size(),
                            "Project lost native/track state");
            const auto compiled = out / L"workflow-built.fxp";
            loaded->Build(compiled);
            Monstey_Gadb reopened(compiled, schema);
            Monstey_Require(Monstey_View(reopened)["active_keys"].size() ==
                                Monstey_View(*db)["active_keys"].size(),
                            "Native built readback lost KeyList graph");
            report["synthetic_built_sha256"] = Monstey_HashSha256(compiled);
        });
        test("duplicate_native_field_hashes_reject_without_partial_creation", [&] {
            const auto before = db->Changes();
            auto values = Monstey_InitialValues();
            values["Asset"] = "one";
            values["asset"] = "two";
            reject([&] {
                Monstey_FxWorkflowCall(*db, "fx_create_element",
                                       J::object({{"class_name", "ClientFX.Synthetic"},
                                                  {"name", "Ambiguous property"},
                                                  {"values", values}}));
            });
            Monstey_Require(db->Changes() == before, "Rejected native hash collision left partial key state");
        });
        test("duplicate_consumed_schema_hashes_reject_including_mixed_marker_orders", [&] {
            for (unsigned variant = 0; variant < 3; ++variant) {
                auto fixture = Monstey_FixtureSchema();
                auto &definition = fixture["categories"]["ClientFX.Synthetic"];
                auto &fields = definition["fields"].as_array();
                for (auto &field : fields)
                    field["flags"] = 0;
                auto duplicate = fields[7];
                if (variant == 1)
                    duplicate["flags"] = 0x80;
                if (variant == 2)
                    fields[7]["flags"] = 0x80;
                fields.push_back(duplicate);
                definition["field_count"] = fields.size();
                const auto path = out / (L"duplicate-schema-" + std::to_wstring(variant) + L".json");
                Monstey_WriteJson(path, fixture);
                auto database = Monstey_Gadb::NewFx(prefix, path);
                const auto before = database->Changes();
                reject([&] {
                    Monstey_FxCreateElement(*database, J::object({{"class_name", "ClientFX.Synthetic"},
                                                                  {"name", "Invalid consumed duplicate"},
                                                                  {"values", Monstey_InitialValues()}}));
                });
                Monstey_Require(database->Changes() == before,
                                "Rejected duplicate consumed field left a partial record");
            }
        });
        test("negative_preroll_is_preserved_and_duration_validation_rolls_back", [&] {
            const auto before = db->Changes();
            Monstey_FxWorkflowCall(*db, "fx_retime_key",
                                   J::object({{"record", beta}, {"start", -0.125}, {"end", .25}}));
            Monstey_Require(Monstey_Value(*db, beta, "StartTime").as_number() == -.125,
                            "Valid native negative preroll was clamped");
            db->Undo();
            reject([&] { Monstey_FxWorkflowCall(*db, "fx_set_duration", J::object({{"duration_ms", -1}})); });
            Monstey_Require(db->Changes() == before, "Rejected duration altered shared state");
        });
        test("effect_deletion_uses_explicit_reference_policy_and_is_one_undo_step", [&] {
            const auto before = db->Changes();
            Monstey_FxWorkflowCall(*db, "fx_delete_effect", J::object({{"replacement", "0xffffffff"}}));
            Monstey_Require(!Monstey_View(*db)["effect_exists"].as_bool(), "Deleted effect remained active");
            const auto target = out / L"deleted-effect.fxp";
            db->Build(target);
            Monstey_Gadb deleted(target, schema);
            Monstey_Require(!Monstey_View(deleted)["effect_exists"].as_bool(),
                            "Deleted effect was restored during native encoding");
            db->Undo();
            Monstey_Require(db->Changes() == before, "Effect graph deletion required multiple undo steps");
        });
        test("effect_rename_and_explicit_duplicate_publish_source_qualified_copy", [&] {
            Monstey_FxWorkflowCall(*db, "fx_rename_effect",
                                   J::object({{"name", "resources/fx/synthetic/renamed.fxp"}}));
            const auto target = out / L"explicit-effect-copy.fxp";
            Monstey_FxWorkflowCall(*db, "fx_duplicate_effect",
                                   J::object({{"name", "resources/fx/synthetic/copy.fxp"},
                                              {"output", Monstey_Utf8(target.wstring())}}));
            Monstey_Gadb copy(target, schema);
            Monstey_Require(Monstey_View(copy)["active_keys"].size() ==
                                Monstey_View(*db)["active_keys"].size(),
                            "Whole effect duplication lost graph");
        });
        if (argc > 3)
            test("actual_full_schema_browse_qualifies_all_classes_and_preserves_sprite_marker_ordinals", [&] {
                auto database = Monstey_Gadb::NewFx(prefix, std::filesystem::path(argv[3]));
                const auto listed = Monstey_FxWorkflowCall(*database, "fx_schema");
                Monstey_Require(listed["total_class_count"].as_uint() == 34 &&
                                    listed["qualified_class_count"].as_uint() == 34 &&
                                    listed["unqualified_class_count"].as_uint() == 0 &&
                                    listed["unqualified_classes"].empty(),
                                "Actual schema browser qualification counts changed");
                for (const auto name : {"ClientFX.DynaLight", "ClientFX.Model", "ClientFX.Sprite",
                                        "ClientFX.FlareSprite"}) {
                    const auto found = std::find_if(listed["classes"].as_array().begin(),
                                                    listed["classes"].as_array().end(), [&](const J &row) {
                                                        return row["class_name"].as_string() == name &&
                                                               row["creation_qualified"].as_bool();
                                                    });
                    Monstey_Require(found != listed["classes"].as_array().end(),
                                    "Qualified native class disappeared from the creation picker");
                    const auto selected =
                        Monstey_FxWorkflowCall(*database, "fx_schema", J::object({{"class_name", name}}));
                    Monstey_Require(selected["creation_qualified"].as_bool() &&
                                        !selected["native_fields"].empty(),
                                    "Qualified class selection failed");
                    if (std::string_view(name) == "ClientFX.Sprite" ||
                        std::string_view(name) == "ClientFX.FlareSprite") {
                        const bool flare = std::string_view(name) == "ClientFX.FlareSprite";
                        const auto fullCount = flare ? 58u : 48u, nativeCount = flare ? 44u : 38u;
                        const auto &definition = selected["definition"];
                        Monstey_Require(definition["field_count"].as_uint() == fullCount &&
                                            definition["fields"].size() == fullCount &&
                                            selected["native_fields"].size() == nativeCount,
                                        "Sprite full/native descriptor counts changed");
                        for (const auto ordinal : {18u, 40u}) {
                            const auto &marker = definition["fields"][ordinal];
                            Monstey_Require(marker["name"].as_string() == "EndSpriteRendering" &&
                                                marker["hash"].as_string() == "0x96bf116c" &&
                                                marker["native_ordinal"].as_uint() == ordinal &&
                                                marker["flags"].as_uint() == 0x40000080u,
                                            "Sprite repeated editor marker lost its native identity");
                        }
                        for (const auto &field : selected["native_fields"].as_array())
                            Monstey_Require(field["name"].as_string() != "EndSpriteRendering",
                                            "Editor marker became a consumed native descriptor");
                    }
                }
                report["native_schema_browser"] = listed;
            });
        if (argc > 3)
            test("actual_native_fx_prefix_graph_edit_and_compiled_readback", [&] {
                const auto native = std::filesystem::path(argv[2]),
                           nativeSchema = std::filesystem::path(argv[3]);
                const auto before = Monstey_HashSha256(native);
                const auto qualified = Monstey_FxQualifiedPrefix(native, before);
                Monstey_Require(qualified == prefix,
                                "Actual native LTFX envelope differs from qualified synthetic protocol");
                Monstey_Gadb real(native, nativeSchema);
                const auto view = Monstey_View(real);
                Monstey_Require(!view["active_keys"].empty(),
                                "Actual native effect has no keys for graph qualification");
                const auto source = view["active_keys"][0u]["selector"].as_string();
                const auto copied = Monstey_FxWorkflowCall(
                    real, "fx_duplicate_element",
                    J::object({{"source", source}, {"name", "Monstey_FxWorkflow_Copy"}}));
                const auto created = Monstey_Ref(copied);
                Monstey_FxWorkflowCall(real, "fx_retime_key",
                                       J::object({{"record", created}, {"start", 0.125}, {"end", 0.5}}));
                const auto target = out / L"native-graph-edited.fxp";
                real.Build(target);
                Monstey_Gadb checked(target, nativeSchema);
                Monstey_Require(Monstey_View(checked)["active_keys"].size() ==
                                        view["active_keys"].size() + 1 &&
                                    Monstey_HashSha256(native) == before,
                                "Actual graph edit/readback changed source or lost element");
                auto fresh = Monstey_Gadb::NewFx(qualified, nativeSchema);
                Monstey_FxWorkflowCall(*fresh, "fx_create_effect",
                                       J::object({{"name", "resources/fx/synthetic/new-native-envelope.fxp"},
                                                  {"duration_ms", 1000}}));
                const auto sourceMetadata = real.Metadata(source);
                const auto className = sourceMetadata["category"].as_string();
                const auto definition = Monstey_FxWorkflowCall(
                    *fresh, "fx_schema", J::object({{"class_name", className}}))["definition"];
                auto values = J::object();
                auto explicitEmptyStrings = J::array();
                for (const auto &field : definition["fields"].as_array()) {
                    for (const auto &original : sourceMetadata["fields"].as_array())
                        if (original["hash"].as_string() ==
                            Monstey_Hex(Monstey_NativeHash(field["name"].as_string())))
                            values[field["name"].as_string()] = original["value"];
                    if (!values.contains(field["name"].as_string()) && field["type_code"].as_uint() == 4 &&
                        (!field.contains("default") || field["default"].is_null())) {
                        // Test author explicitly accepts an empty optional string; the engine supplies no
                        // default.
                        values[field["name"].as_string()] = "";
                        explicitEmptyStrings.as_array().push_back(field["name"]);
                    }
                }
                report["native_new_class_test_author_empty_strings"] = explicitEmptyStrings;
                Monstey_FxWorkflowCall(*fresh, "fx_create_element",
                                       J::object({{"class_name", className},
                                                  {"name", "Independent native class"},
                                                  {"start", 0.0},
                                                  {"end", 0.75},
                                                  {"values", values}}));
                Monstey_Require(fresh->Describe()["clone_count"].as_uint() == 0,
                                "Independent native class creation used a donor clone");
                const auto independent = out / L"new-native-envelope.fxp";
                fresh->Build(independent);
                Monstey_Gadb independentReadback(independent, nativeSchema);
                Monstey_Require(
                    Monstey_View(independentReadback)["active_keys"].size() == 1 &&
                        Monstey_Value(
                            independentReadback,
                            Monstey_View(independentReadback)["active_keys"][0u]["selector"].as_string(),
                            "EndTime")
                                .as_number() == .75,
                    "Independent native class graph lost typed values on readback");
                const auto nativeBinding = Monstey_FxVerifyNativeBindings(independentReadback);
                Monstey_Require(nativeBinding["all_records_compatible"].as_bool(),
                                "Fresh actual class fails native compact binding predicates");
                report["native_new_binding"] = nativeBinding;
                report["native_new_class"] = className;
                report["native_new_class_source_properties_explicit"] = values.size();
                report["native_new_sha256"] = Monstey_HashSha256(independent);
                report["native_source_sha256"] = before;
                report["native_edited_sha256"] = Monstey_HashSha256(target);
            });
        report["passed"] = static_cast<std::uint64_t>(report["cases"].size());
        report["output"] = Monstey_Utf8(out.wstring());
        Monstey_WriteJson(out / L"fx-workflow-tests.json", report);
        std::cout << "Native FX workflow tests: " << report["passed"].as_uint() << " passed\n"
                  << Monstey_Utf8(out.wstring()) << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}