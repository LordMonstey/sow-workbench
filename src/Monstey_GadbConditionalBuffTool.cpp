#include "Monstey_Gadb.hpp"
#include "Monstey_GadbCachedValue.hpp"
#include <array>
#include <iostream>
#include <map>
using namespace Monstey;
int wmain(int argc, wchar_t** argv) {
    try {
        Monstey_Require(argc == 6, "Usage: Absence audit.json audit_sha256 database database_sha256 NEW-report.json");
        const std::filesystem::path auditPath(argv[1]), databasePath(argv[3]), output(argv[5]);
        Monstey_RequireOfflineOutput(output, std::array{auditPath, databasePath});
        Monstey_Require(!std::filesystem::exists(Monstey_WindowsFilePath(output)), "Output already exists");
        const auto auditBytes = Monstey_ReadFile(auditPath, 128ull * 1024 * 1024);
        const auto auditHash = Monstey_HashSha256(auditBytes);
        Monstey_Require(auditHash == Monstey_Utf8(argv[2]), "Audit SHA256 mismatch");
        const auto audit = Monstey_Json::Parse(std::string(auditBytes.begin(), auditBytes.end()));
        const auto databaseHash = Monstey_HashSha256(databasePath);
        Monstey_Require(databaseHash == Monstey_Utf8(argv[4]) &&
                        audit.at("database_sha256").as_string() == databaseHash, "Database SHA256 mismatch");
        Monstey_Gadb database(databasePath);
        const auto categories = database.Categories();
        std::uint32_t categoryIndex = 0;
        std::uint64_t matches = 0;
        for (const auto& category : categories.at("rows").as_array()) {
            if (category.at("name").as_string() == "Combat.Buff") {
                categoryIndex = static_cast<std::uint32_t>(category.at("index").as_uint()); ++matches;
            }
        }
        Monstey_Require(matches == 1, "Expected exactly one Combat.Buff category");
        std::map<std::string, Monstey_Json, std::less<>> records;
        for (std::size_t offset = 0;;) {
            const auto page = database.DescriptorLayouts(categoryIndex, offset, 10000);
            Monstey_Require(page.at("published_order_provenance").as_string() == "native_source" &&
                            !page.at("values_decoded").as_bool(), "Expected unedited native descriptor layouts");
            for (const auto& record : page.at("rows").as_array())
                records.emplace(record.at("reference").as_string(), record);
            offset += page.at("rows").size();
            if (offset >= page.at("total").as_uint()) break;
            Monstey_Require(!page.at("rows").empty(), "Pagination made no progress");
        }
        auto report = Monstey_GadbConditionalBuffAbsence(audit);
        for (const auto& witness : report.at("rows").as_array()) {
            const auto& record = records.at(witness.at("record").as_string());
            const auto& field = record.at("fields").at(4);
            Monstey_Require(field.at("hash").as_string() == "0x5b57a121" &&
                            field.at("native_ordinal").as_uint() == 5 &&
                            field.at("native_type").as_uint() == 11 && field.at("count").as_uint() == 0,
                            "Actual descriptor does not match selected absence witness");
        }
        Monstey_Require(Monstey_HashSha256(databasePath) == databaseHash &&
                        Monstey_HashSha256(auditPath) == auditHash, "Input changed before publication");
        report["audit_sha256"] = auditHash;
        report["actual_descriptor_rows_checked"] = report.at("rows").size();
        report["actual_category_records_seen"] = records.size();
        report["values_decoded"] = false;
        report["inputs_unchanged"] = true;
        Monstey_WriteNewText(output, report.dump(2) + "\n");
        std::cout << "PASS selected_absences=" << report.at("qualified_selected_absences").as_uint()
                  << " original_contracts=" << audit.at("totals").at("unqualified_value_contracts").as_uint() << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
