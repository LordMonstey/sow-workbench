#include "Monstey_Gadb.hpp"
#include <chrono>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
void Check(bool condition, const char *message) {
    if (!condition) throw Monstey_Error("test", message);
}
}
int wmain(int argc, wchar_t **argv) {
    try {
        const auto base = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path();
        const auto out = base / (L"Incoming_" +
            std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(out);
        auto report = J::object({{"status", "PASS"}, {"cases", J::array()},
                                  {"native_cpp", true}, {"game_written", false}});
        const auto test = [&](const char *name, const std::function<void()> &run) {
            run(); report["cases"].as_array().push_back(name);
        };
        const auto schema = out / L"schema.json";
        Monstey_WriteNewText(schema, J::object({{"categories", J::object({
            {"Target", J::object({{"fields", J::array()}})},
            {"Owner", J::object({{"fields", J::array({
                J::object({{"name", "Single"}, {"type_code", 10}, {"array", 1}, {"target", "Target"}}),
                J::object({{"name", "Many"}, {"type_code", 11}, {"array", -1}, {"target", "Target"}}),
                J::object({{"name", "Fixed"}, {"type_code", 10}, {"array", 3}, {"target", "Target"}}),
                J::object({{"name", "Text"}, {"type_code", 6}, {"array", 1}})
            })}})}
        })}}).dump());
        auto db = Monstey_Gadb::New(schema);
        const auto targetCategory = db->CreateCategory("Target")["category"]["index"].as_uint();
        const auto ownerCategory = db->CreateCategory("Owner")["category"]["index"].as_uint();
        const auto first = db->CreateRecord(static_cast<std::uint32_t>(targetCategory), "First")
            ["record"]["selector"].as_string();
        const auto second = db->CreateRecord(static_cast<std::uint32_t>(targetCategory), "Second")
            ["record"]["selector"].as_string();
        const auto owner = db->CreateRecord(static_cast<std::uint32_t>(ownerCategory), "Owner")
            ["record"]["selector"].as_string();
        db->AddField(owner, "Single", 10, first);
        db->AddField(owner, "Many", 11, J::array({first, nullptr, first, second}));
        db->AddField(owner, "Fixed", 10, J::array({second, first, nullptr}));
        db->AddField(owner, "Text", 6, "Unicode \xce\xa9 does not participate");
        test("counts_scalar_variable_fixed_duplicate_and_null_reference_slots", [&] {
            const auto incoming = db->IncomingReferences(first, 0, 100);
            Check(incoming["total"].as_uint() == 4 && incoming["rows"].size() == 4,
                  "Typed incoming count differs");
            unsigned scalar = 0, variable = 0, fixed = 0;
            for (const auto &row : incoming["rows"].as_array()) {
                Check(row["record"].as_string() == owner &&
                          row["category"].as_string() == "Owner" &&
                          row["category_schema"].as_string() == "Owner",
                      "Incoming owner identity differs");
                if (row["field"].as_string() == "Single") {
                    Check(row["index"].is_null(), "Scalar reference gained an array ordinal");
                    ++scalar;
                } else if (row["field"].as_string() == "Many") {
                    Check(row["index"].as_uint() == 0 || row["index"].as_uint() == 2,
                          "Duplicate list ordinal lost");
                    ++variable;
                } else if (row["field"].as_string() == "Fixed") {
                    Check(row["index"].as_uint() == 1, "Fixed list ordinal lost");
                    ++fixed;
                } else Check(false, "Non-reference field reported as incoming");
            }
            Check(scalar == 1 && variable == 2 && fixed == 1, "Incoming shape classification differs");
        });
        test("paging_and_count_only_keep_complete_native_total", [&] {
            const auto page = db->IncomingReferences(first, 1, 2);
            Check(page["total"].as_uint() == 4 && page["rows"].size() == 2 &&
                      page["offset"].as_uint() == 1, "Incoming paging truncated total");
            Check(db->IncomingReferences(first, 20, 2)["rows"].empty(), "Out of range page is not empty");
            const auto count = db->IncomingReferences(first, 0, 0);
            Check(count["total"].as_uint() == 4 && count["rows"].empty(), "Count-only page failed");
        });
        test("effective_pending_edits_and_history_are_used", [&] {
            db->SetField(owner, "Single", second);
            db->SetField(owner, "Many", J::array({second, first}));
            Check(db->IncomingReferences(first)["total"].as_uint() == 2,
                  "Incoming reader used original words instead of pending typed edits");
            db->Undo();
            Check(db->IncomingReferences(first)["total"].as_uint() == 3, "Incoming undo differs");
            db->Redo();
            Check(db->IncomingReferences(first)["total"].as_uint() == 2, "Incoming redo differs");
        });
        std::string clone;
        test("independent_clones_are_real_incoming_owners", [&] {
            clone = db->CloneRecord(owner, "IncomingCopy")["clone"]["id"].as_string();
            const auto incoming = db->IncomingReferences(first);
            Check(incoming["total"].as_uint() == 4, "Clone references missing");
            bool seen = false;
            for (const auto &row : incoming["rows"].as_array())
                seen = seen || row["record"].as_string() == clone;
            Check(seen, "Clone selector was replaced by source owner identity");
        });
        test("deleted_owners_are_excluded_and_undo_restores_them", [&] {
            db->DeleteRecord(clone);
            Check(db->IncomingReferences(first)["total"].as_uint() == 2,
                  "Deleted owner still contributes incoming references");
            db->Undo();
            Check(db->IncomingReferences(first)["total"].as_uint() == 4,
                  "Undo did not restore incoming clone owner");
        });
        const auto native = out / L"source.gamedb";
        J build;
        test("query_is_read_only_and_build_retains_sorted_reference_mapping", [&] {
            const auto before = db->Changes();
            db->IncomingReferences(first);
            Check(db->Changes() == before, "Incoming query mutated project/history");
            build = db->Build(native);
            Check(build["warnings"].empty(), "Incoming test native build warned");
        });
        test("unknown_schema_reference_fields_keep_hash_and_native_type", [&] {
            Monstey_Gadb raw(native);
            const auto mapped = build["reference_map"].contains(first) ?
                build["reference_map"][first].as_string() : first;
            const auto incoming = raw.IncomingReferences(mapped, 0, 100);
            Check(incoming["total"].as_uint() == 4, "Unschematized native references disappeared");
            for (const auto &row : incoming["rows"].as_array())
                Check(row["field"] == row["field_hash"] &&
                          (row["native_type"].as_uint() == 10 || row["native_type"].as_uint() == 11),
                      "Unknown field was assigned a fabricated source name/type");
            Check(Monstey_HashSha256(native) == incoming["source_sha256"].as_string(),
                  "Native source changed during incoming inspection");
        });
        test("invalid_selector_and_page_bound_reject_without_mutation", [&] {
            const auto before = db->Changes();
            bool badSelector = false, badPage = false;
            try { db->IncomingReferences("Missing:Record"); } catch (const std::exception &) {badSelector = true;}
            try { db->IncomingReferences(first, 0, 10001); } catch (const std::exception &) {badPage = true;}
            Check(badSelector && badPage && db->Changes() == before, "Invalid query changed state");
        });
        report["passed"] = report["cases"].size();
        Monstey_WriteNewText(out / L"incoming-tests.json", report.dump(2));
        std::cout << report["passed"].as_uint() << " native incoming-reference groups passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}