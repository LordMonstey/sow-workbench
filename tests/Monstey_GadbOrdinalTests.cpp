#include "Monstey_FxWorkflow.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
using namespace Monstey;
namespace {
using J = Monstey_Json;
J Monstey_Field(std::string name, unsigned type, J value, unsigned flags = 0) {
    return J::object({{"name", name},
                      {"type_code", type},
                      {"default", value},
                      {"array", 1},
                      {"flags", flags},
                      {"ordinal", 99}});
}
J Monstey_Schema() {
    const auto fields =
        J::array({Monstey_Field("KeyName", 4, ""), Monstey_Field("StartTime", 2, 0.0),
                  Monstey_Field("BeginGroup", 4, nullptr, 0x80000080u), Monstey_Field("EndTime", 2, 1.0),
                  Monstey_Field("Value", 3, 8), Monstey_Field("EndGroup", 4, nullptr, 0x200u)});
    return J::object(
        {{"categories",
          J::object(
              {{"ClientFX.NativeLayout", J::object({{"fields", fields},
                                                    {"field_count", 6},
                                                    {"unreadable_fields", 0},
                                                    {"flags", "0x100000000"}})},
               {"Generic", J::object({{"fields", J::array({J::object({{"name", "Bound"},
                                                                      {"type_code", 3},
                                                                      {"native_ordinal", 17}})})}})}})}});
}
std::string Monstey_Record(Monstey_Gadb &db, std::string_view label = "Generic") {
    const auto category = db.CreateCategory(label)["category"]["index"].as_uint();
    return db
        .CreateRecord(static_cast<std::uint32_t>(category), "Monstey_OrdinalRecord")["record"]["selector"]
        .as_string();
}
J Monstey_Identities(Monstey_Gadb &db, std::string_view ref) {
    auto result = J::object();
    const auto metadata = db.Metadata(ref);
    for (const auto &field : metadata["fields"].as_array())
        result[field["name"].as_string()] = field["native_ordinal"];
    return result;
}
std::string Monstey_Element(Monstey_Gadb &db, std::string_view name) {
    return Monstey_FxCreateElement(db,
                                   J::object({{"name", std::string(name)},
                                              {"class_name", "ClientFX.NativeLayout"}}))["result"]["selector"]
        .as_string();
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        const auto out =
            (argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(L"gadb-ordinal-tests")) /
            (L"GadbOrdinal_" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(out);
        const auto schema = out / L"schema.json";
        Monstey_WriteNewText(schema, Monstey_Schema().dump(2));
        auto report = J::object({{"status", "PASS"},
                                 {"engine_executed", false},
                                 {"originals_modified", false},
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
            bool bad = false;
            try {
                action();
            } catch (const std::exception &) {
                bad = true;
            }
            Monstey_Require(bad, "Expected ordinal validation failure");
        };
        test("legacy_fresh_generic_append_identity_and_move_are_separate", [&] {
            auto db = Monstey_Gadb::New(schema);
            const auto ref = Monstey_Record(*db);
            db->AddField(ref, "A", 3, 1);
            db->AddField(ref, "B", 3, 2);
            db->AddField(ref, "C", 3, 3);
            const auto before = Monstey_Identities(*db, ref);
            Monstey_Require(before == J::object({{"A", 0}, {"B", 1}, {"C", 2}}), "Legacy append changed");
            db->MoveField(ref, "C", 0);
            Monstey_Require(Monstey_Identities(*db, ref) == before &&
                                db->Metadata(ref)["fields"][0u]["name"].as_string() == "C",
                            "Move changed stable identity");
        });
        test("sparse_identities_survive_move_delete_add_and_undo", [&] {
            auto db = Monstey_Gadb::New(schema);
            const auto ref = Monstey_Record(*db);
            db->AddField(ref, "A", 3, 1, J::object({{"native_ordinal", 2}}));
            db->AddField(ref, "B", 3, 2, J::object({{"native_ordinal", 8}}));
            db->AddField(ref, "C", 3, 3, J::object({{"native_ordinal", 31}}));
            const auto before = db->Changes();
            db->MoveField(ref, "C", 0);
            Monstey_Require(Monstey_Identities(*db, ref) == J::object({{"A", 2}, {"B", 8}, {"C", 31}}),
                            "Sparse identity changed");
            db->Undo();
            Monstey_Require(db->Changes() == before, "Undo lost identity/layout");
            db->Redo();
            db->DeleteField(ref, "B");
            db->AddField(ref, "D", 3, 4);
            Monstey_Require(Monstey_Identities(*db, ref) == J::object({{"A", 2}, {"C", 31}, {"D", 0}}),
                            "Delete/add rewrote surviving identities");
        });
        test("qualified_identity_conflict_duplicates_and_bad_uint8_are_atomic", [&] {
            auto db = Monstey_Gadb::New(schema);
            const auto ref = Monstey_Record(*db);
            db->AddField(ref, "Bound", 3, 1);
            Monstey_Require(Monstey_Identities(*db, ref)["Bound"].as_uint() == 17, "Schema identity ignored");
            const auto before = db->Changes();
            reject([&] { db->AddField(ref, "Duplicate", 3, 2, J::object({{"native_ordinal", 17}})); });
            for (const auto value : {J(-1), J(256), J(nullptr), J(2.5)})
                reject([&] { db->AddField(ref, "Invalid", 3, 2, J::object({{"native_ordinal", value}})); });
            Monstey_Require(db->Changes() == before, "Rejected identity changed state");
            auto other = Monstey_Gadb::New(schema);
            const auto row = Monstey_Record(*other);
            reject([&] { other->AddField(row, "Bound", 3, 2, J::object({{"native_ordinal", 3}})); });
        });
        test("sparse_project_and_build_reread_are_byte_exact", [&] {
            auto db = Monstey_Gadb::New(schema);
            const auto ref = Monstey_Record(*db);
            db->AddField(ref, "A", 3, 12, J::object({{"native_ordinal", 8}}));
            db->AddField(ref, "B", 4, "value", J::object({{"native_ordinal", 31}}));
            db->MoveField(ref, "B", 0);
            const auto project = out / L"sparse-project.json";
            db->SaveProject(project);
            auto restored = Monstey_Gadb::LoadProject(project);
            Monstey_Require(restored->Changes() == db->Changes(), "Sparse project identities changed");
            const auto one = out / L"sparse-a.gamedb", two = out / L"sparse-b.gamedb";
            db->Build(one);
            restored->Build(two);
            Monstey_Require(Monstey_ReadFile(one) == Monstey_ReadFile(two), "Project build differs");
            Monstey_Gadb parsed(one, schema);
            const auto rows = parsed.Records(0)["rows"];
            const auto found = rows[0u]["native_reference"].as_string();
            const auto metadata = parsed.Metadata(found);
            std::map<std::string, unsigned> identities;
            for (const auto &field : metadata["fields"].as_array())
                identities[field["hash"].as_string()] =
                    static_cast<unsigned>(field["native_ordinal"].as_uint());
            Monstey_Require(identities.at(Monstey_Hex(Monstey_NativeHash("A"))) == 8 &&
                                identities.at(Monstey_Hex(Monstey_NativeHash("B"))) == 31,
                            "Binary descriptor+8 changed");
            const auto identity = out / L"sparse-noop.gamedb";
            parsed.Build(identity);
            Monstey_Require(Monstey_ReadFile(one) == Monstey_ReadFile(identity), "No-op bytes changed");
        });
        test("marker_factory_omits_both_marker_kinds_and_ignores_loaded_cache", [&] {
            auto db = Monstey_Gadb::NewFx(Monstey_FxDefaultPrefix(), schema);
            const auto ref = Monstey_Element(*db, "First");
            Monstey_Require(Monstey_Identities(*db, ref) ==
                                J::object({{"KeyName", 0}, {"StartTime", 1}, {"EndTime", 3}, {"Value", 4}}),
                            "Factory materialized marker/cache ordinals");
            const auto result = Monstey_FxVerifyNativeBindings(*db);
            Monstey_Require(result["all_records_compatible"].as_bool() &&
                                result["verified_records"].as_uint() == 1,
                            "Canonical factory failed native binding predicates");
            const auto path = out / L"marker-native.fxp";
            db->Build(path);
            Monstey_Gadb parsed(path, schema);
            Monstey_Require(Monstey_FxVerifyNativeBindings(parsed)["all_records_compatible"].as_bool(),
                            "Built factory lost native binding");
        });
        test("first_native_record_does_not_hide_incompatible_later_layout", [&] {
            auto db = Monstey_Gadb::NewFx(Monstey_FxDefaultPrefix(), schema);
            const auto first = Monstey_Element(*db, "First"), later = Monstey_Element(*db, "Later");
            const auto stable = Monstey_Identities(*db, later);
            db->MoveField(later, "Value", 0);
            const auto result = Monstey_FxVerifyNativeBindings(*db);
            Monstey_Require(!result["all_records_compatible"].as_bool() &&
                                result["later_record_cache_conflicts"].as_uint() > 0 &&
                                result["categories"][0u]["records"][0u]["compatible"].as_bool() &&
                                Monstey_Identities(*db, later) == stable,
                            "Later layout was incorrectly accepted");
            db->Undo();
            db->MoveField(first, "Value", 0);
            const auto badFirst = Monstey_FxVerifyNativeBindings(*db);
            Monstey_Require(!badFirst["all_records_compatible"].as_bool() &&
                                badFirst["categories"][0u]["records"][1u]["compatible"].as_bool(),
                            "First-record fallback was mistaken for universal compatibility");
        });
        test("first_record_selection_uses_native_clone_hash_tail_not_friendly_id", [&] {
            auto db = Monstey_Gadb::NewFx(Monstey_FxDefaultPrefix(), schema);
            const auto source = Monstey_Element(*db, "Anonymous source");
            const auto first = db->CloneRecord(source, "Monstey_Z_First")["clone"]["reference"].as_string();
            const auto later = db->CloneRecord(source, "Monstey_A_Second")["clone"]["reference"].as_string();
            db->MoveField(later, "Value", 0);
            const auto proof = Monstey_FxVerifyNativeBindings(*db);
            Monstey_Require(proof["categories"][0u]["first_record"].as_string() == first &&
                                proof["later_record_cache_conflicts"].as_uint() > 0,
                            "Friendly clone ID changed native first-record qualification");
        });
        test("identity_255_and_descriptor_limit_have_no_duplicate_or_wrap", [&] {
            auto db = Monstey_Gadb::New(schema);
            const auto ref = Monstey_Record(*db);
            db->AddField(ref, "Last", 3, 1, J::object({{"native_ordinal", 255}}));
            for (unsigned i = 0; i < 254; ++i)
                db->AddField(ref, "F" + std::to_string(i), 3, i);
            const auto before = db->Changes();
            reject([&] { db->AddField(ref, "Overflow", 3, 1); });
            db->MoveField(ref, "Last", 254);
            Monstey_Require(Monstey_Identities(*db, ref)["Last"].as_uint() == 255 && before != db->Changes(),
                            "UInt8 identity wrapped during move");
        });
        if (argc > 5) {
            const auto nativeSchema = std::filesystem::path(argv[5]);
            const std::array names{"ClientFX.DynaLight", "ClientFX.Create", "ClientFX.DebrisSystem"};
            const std::array<unsigned, 3> counts{38, 32, 75};
            auto nativeReports = J::array();
            for (std::size_t i = 0; i < names.size(); ++i)
                test(std::string("actual_factory_binding_") + names[i], [&] {
                    const auto source = std::filesystem::path(argv[i + 2]);
                    const auto sha = Monstey_HashSha256(source);
                    Monstey_Gadb original(source, nativeSchema);
                    const auto rows = original.FxRecords(names[i])["rows"];
                    Monstey_Require(!rows.empty(), "Actual source class absent");
                    const auto native = original.Metadata(rows[0u]["reference"].as_string());
                    auto fresh = Monstey_Gadb::NewFx(Monstey_FxDefaultPrefix(), nativeSchema);
                    auto values = J::object();
                    for (const auto &field : native["fields"].as_array())
                        values[field["name"].as_string()] = field["value"];
                    const auto result = Monstey_FxCreateElement(
                        *fresh,
                        J::object({{"name", "Native layout"}, {"class_name", names[i]}, {"values", values}}));
                    const auto ref = result["result"]["selector"].as_string();
                    Monstey_Require(fresh->Metadata(ref)["fields"].size() == counts[i],
                                    "Native descriptor count differs");
                    const auto freshProof = Monstey_FxVerifyNativeBindings(*fresh);
                    Monstey_Require(freshProof["all_records_compatible"].as_bool(),
                                    "Fresh native binding fails");
                    const auto path = out / (L"native-" + std::to_wstring(i) + L".fxp");
                    fresh->Build(path);
                    Monstey_Gadb parsed(path, nativeSchema);
                    const auto readback = Monstey_FxVerifyNativeBindings(parsed);
                    Monstey_Require(readback["all_records_compatible"].as_bool(),
                                    "Compiled native binding fails");
                    const auto identity = out / (L"identity-" + std::to_wstring(i) + L".fxp");
                    original.Build(identity);
                    Monstey_Require(Monstey_ReadFile(identity) == Monstey_ReadFile(source) &&
                                        Monstey_HashSha256(source) == sha,
                                    "Original/native no-op bytes changed");
                    nativeReports.as_array().push_back(
                        J::object({{"class", names[i]},
                                   {"source_sha256", sha},
                                   {"authored_sha256", Monstey_HashSha256(path)},
                                   {"descriptors", counts[i]},
                                   {"original_binding", Monstey_FxVerifyNativeBindings(original)},
                                   {"readback_binding", readback}}));
                });
            report["native"] = nativeReports;
        }
        report["passed"] = static_cast<std::uint64_t>(report["cases"].size());
        Monstey_WriteNewText(out / L"gadb-ordinal-tests.json", report.dump(2));
        std::cout << "Native GADB ordinal tests: " << report["passed"].as_uint() << " passed\n"
                  << Monstey_Utf8(out.wstring()) << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}