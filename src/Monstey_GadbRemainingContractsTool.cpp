#include "Monstey_GadbRemainingContracts.hpp"
#include "Monstey_Gadb.hpp"
#include "Monstey_GadbClassDeclaration.hpp"
#include <array>
#include <iostream>
#include <map>
#include <set>
using namespace Monstey;
int wmain(int argc, wchar_t** argv) {
    try {
        Monstey_Require(argc == 6 || argc == 8,
            "Usage: Remaining audit audit_sha PE PE_sha NEW-report [database database_sha]");
        const std::filesystem::path auditPath(argv[1]), imagePath(argv[3]), output(argv[5]);
        std::vector<std::filesystem::path> inputs{auditPath, imagePath};
        if (argc == 8) inputs.emplace_back(argv[6]);
        Monstey_RequireOfflineOutput(output, inputs);
        Monstey_Require(!std::filesystem::exists(output), "New output required");
        const auto auditBytes = Monstey_ReadFile(auditPath, 128ull * 1024 * 1024);
        const auto image = Monstey_ReadFile(imagePath);
        const auto auditSha = Monstey_HashSha256(auditBytes), imageSha = Monstey_HashSha256(image);
        Monstey_Require(auditSha == Monstey_Utf8(argv[2]) && imageSha == Monstey_Utf8(argv[4]), "Pinned input differs");
        Monstey_Require(imageSha == "6b8db778135c082c934463b6524e6b220478fb6987f082ab37ca48a870acb455" ||
            imageSha == "ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef",
            "Selected consumer semantics require a byte-qualified native image");
        const auto audit = Monstey_Json::Parse(std::string(auditBytes.begin(), auditBytes.end()));
        std::set<std::string> selectedNames;
        for (const auto& issue : audit.at("diagnostics").as_array())
            if (!(issue.at("category") == Monstey_Json("Combat.Buff") && issue.at("field") == Monstey_Json("ConditionalBuff")))
                selectedNames.insert(issue.at("category").as_string());
        std::vector<std::uint64_t> pointers;
        for (const auto& category : audit.at("categories").as_array())
            if (selectedNames.contains(category.at("name").as_string()))
                pointers.push_back(std::stoull(category.at("native_entry").at("entry_ea").as_string(), nullptr, 16));
        const auto native = Monstey_GadbInspectClassPointers(image, pointers, imageSha);
        auto report = Monstey_GadbInspectRemainingCountContracts(audit, native);
        if (argc == 8) {
            const std::filesystem::path databasePath(argv[6]);
            const auto databaseSha = Monstey_HashSha256(databasePath);
            Monstey_Require(databaseSha == Monstey_Utf8(argv[7]) && databaseSha == audit.at("database_sha256").as_string(), "Pinned database differs");
            Monstey_Gadb database(databasePath);
            std::map<std::string, Monstey_Json, std::less<>> records;
            const auto get = [&](const std::string& reference) -> const Monstey_Json& {
                if (const auto found = records.find(reference); found != records.end()) return found->second;
                const auto packed = std::stoul(reference, nullptr, 16);
                const auto page = database.DescriptorLayouts(static_cast<std::uint32_t>(packed >> 20), packed & 0xfffff, 1);
                Monstey_Require(page.at("published_order_provenance") == Monstey_Json("native_source") &&
                    page.at("rows").size() == 1 && page.at("rows").at(0).at("reference").as_string() == reference,
                    "Exact source ordinal did not return the selected record");
                return records.emplace(reference, page.at("rows").at(0)).first->second;
            };
            for (auto& outputRow : report.at("rows").as_array()) {
                const auto& issue = outputRow.at("original_contract");
                const auto& current = get(issue.at("record").as_string());
                const auto& first = get(issue.at("first_record").as_string());
                const auto physical = issue.at("physical_index").as_uint();
                for (const auto& pair : {std::pair{&current, &issue.at("actual_descriptor")}, std::pair{&first, &issue.at("first_descriptor")}}) {
                    const auto& f = pair.first->at("fields").at(static_cast<std::size_t>(physical));
                    Monstey_Require(f.at("hash") == pair.second->at("hash") && f.at("count") == pair.second->at("count") &&
                        f.at("native_type") == pair.second->at("native_type") && f.at("native_ordinal") == pair.second->at("native_ordinal"),
                        "Selected actual descriptor no longer matches pinned audit");
                }
                outputRow["actual_source_record"] = Monstey_Json::object({{"reference", current.at("reference")},
                    {"name", current.value("name", nullptr)}, {"source_descriptor_checked", true}});
            }
            Monstey_Require(Monstey_HashSha256(databasePath) == databaseSha, "Database changed during selected reads");
            report["actual_distinct_records_read"] = records.size();
            report["actual_descriptor_contracts_checked"] = report.at("rows").size();
        }
        Monstey_Require(Monstey_HashSha256(auditPath) == auditSha && Monstey_HashSha256(imagePath) == imageSha,
            "Input changed before publication");
        report["audit_sha256"] = auditSha; report["native_image_sha256"] = imageSha;
        report["original_audit_native_image_sha256"] = audit.at("native_image_sha256");
        report["native_declaration_profile_differs_from_audit"] = imageSha != audit.at("native_image_sha256").as_string();
        report["source_unchanged"] = true;
        Monstey_WriteNewText(output, report.dump(2) + "\n");
        std::cout << "PASS remaining=" << report.at("remaining_rows").as_uint() << " selected="
            << report.at("selected_cached_absence_rows").as_uint() << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
