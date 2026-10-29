#include "Monstey_GadbRemainingContracts.hpp"
#include <array>
#include <map>
#include <set>
namespace Monstey {
Monstey_NextKeyProbeResult Monstey_GadbProbeNextKeyAbsence(
    const Monstey_GadbCachedRecordView* action, std::int32_t field_state,
    std::uint8_t physical_cache) {
    if (!action) return {Monstey_NextKeyProbeGate::ReturnedZero, 0, false};
    if (field_state < 2)
        return {Monstey_NextKeyProbeGate::NamedActionLookupRequired, 0, false};
    const auto next = Monstey_GadbReadCachedQword(action, physical_cache, 0, 0);
    if (next == 0) return {Monstey_NextKeyProbeGate::ReturnedZero, 0, true};
    return {Monstey_NextKeyProbeGate::NonzeroNextKeyRequiresOwnedChild, next, true};
}
Monstey_Json Monstey_GadbInspectRemainingCountContracts(
    const Monstey_Json& audit, const Monstey_Json& declarations) {
    using J = Monstey_Json;
    Monstey_Require(audit.at("format") == J("Monstey_GadbClassCacheAudit") &&
        audit.at("version").as_uint() == 1 && !audit.at("diagnostics_truncated").as_bool(),
        "Complete native binding audit v1 required");
    Monstey_Require(declarations.at("schema") == J("Monstey.GadbExplicitClassDeclarations/1"),
        "Explicit-pointer native declaration report required");
    std::map<std::string, const J*, std::less<>> native;
    for (const auto& c : declarations.at("classes").as_array())
        Monstey_Require(native.emplace(c.at("class_pointer").as_string(), &c).second,
            "Duplicate exact native class identity in supplied report");
    std::set<std::string> neededCategories;
    for (const auto& issue : audit.at("diagnostics").as_array())
        if (!(issue.at("category") == J("Combat.Buff") && issue.at("field") == J("ConditionalBuff")))
            neededCategories.insert(issue.at("category").as_string());
    std::map<std::string, const J*, std::less<>> categories;
    for (const auto& c : audit.at("categories").as_array())
        if (neededCategories.contains(c.at("name").as_string()))
            Monstey_Require(categories.emplace(c.at("name").as_string(), &c).second,
                "Ambiguous selected category identity in supplied audit");
    J rows = J::array(), groups = J::array();
    std::map<std::pair<std::string, std::string>, std::size_t> grouped;
    std::uint64_t excluded = 0, selected = 0;
    for (const auto& issue : audit.at("diagnostics").as_array()) {
        const auto category = issue.at("category").as_string();
        const auto fieldName = issue.at("field").as_string();
        if (category == "Combat.Buff" && fieldName == "ConditionalBuff") { ++excluded; continue; }
        const auto& c = *categories.at(category);
        const auto pointer = c.at("native_entry").at("entry_ea").as_string();
        const auto n = native.find(pointer);
        const J* declaration = nullptr;
        if (n != native.end()) {
            for (const auto& f : n->second->at("fields").as_array()) {
                if (f.at("declaration_index") == issue.at("native_ordinal")) {
                    Monstey_Require(declaration == nullptr, "Duplicate native declaration ordinal");
                    declaration = &f;
                }
            }
        }
        bool identity = false;
        J nativeField = nullptr;
        if (declaration) {
            nativeField = *declaration;
            identity = declaration->at("name").at("status") == J("FILE_BACKED") &&
                declaration->at("name").at("value") == issue.at("field") &&
                declaration->at("name_hash_u32") == issue.at("declaration").at("hash") &&
                declaration->at("type_u32") == issue.at("declaration").at("native_type") &&
                declaration->at("count_u32") == issue.at("declaration").at("count");
        }
        const bool selectedIdentity = identity && category == "AudioHook.Action" &&
            fieldName == "AudioHookNextKey" && pointer == "0x14264c660" &&
            issue.at("physical_index").as_uint() == 20 && issue.at("native_ordinal").as_uint() == 20 &&
            issue.at("classification") == J("unknown_value_contract") &&
            issue.at("mismatches").size() == 1 && issue.at("mismatches").at(0) == J("count") &&
            issue.at("declaration").at("hash") == J("0x223477a3") &&
            issue.at("declaration").at("native_type").as_uint() == 11 &&
            issue.at("declaration").at("count").as_uint() == 1 &&
            declaration->at("field_pointer") == J("0x142863da0") &&
            declaration->at("index_callback_pointer") == J("0x141689c10") &&
            issue.at("first_descriptor").at("hash") == J("0x223477a3") &&
            issue.at("actual_descriptor").at("hash") == J("0x223477a3") &&
            issue.at("first_descriptor").at("count").as_uint() == 1 &&
            issue.at("actual_descriptor").at("count").as_uint() == 0 &&
            issue.at("first_descriptor").at("native_type").as_uint() == 11 &&
            issue.at("actual_descriptor").at("native_type").as_uint() == 11 &&
            issue.at("first_descriptor").at("native_ordinal").as_uint() == 20 &&
            issue.at("actual_descriptor").at("native_ordinal").as_uint() == 20;
        J consumer = J::object({{"status", "NATIVE_CONSUMER_NOT_QUALIFIED"},
            {"native_cache_state_or_load_history_assumed", false}});
        if (selectedIdentity) {
            std::array<std::uint8_t, 334> descriptorPrefix{};
            const Monstey_GadbCachedRecordView owned{descriptorPrefix, {}};
            const auto proof = Monstey_GadbProbeNextKeyAbsence(&owned, 2, 20);
            Monstey_Require(proof.gate == Monstey_NextKeyProbeGate::ReturnedZero,
                "Selected cached absence probe differs");
            consumer = J::object({{"status", "SELECTED_CACHED_ABSENCE_RETURN_ZERO"},
                {"getter", "0x140d17c80"}, {"getter_complete_bytes", 1532},
                {"field_state_offset", "0x5d0"}, {"physical_cache_offset", "0x5e0"},
                {"nested_class_pointer", "0x14264c4e0"},
                {"nested_null_return", "0x140d18262"}, {"returned_raw_qword", "0x0000000000000000"},
                {"selected_caller_call", "0x140d2093b"}, {"selected_caller_null_branch", "0x140d20946"},
                {"selected_caller_continuation", "0x140d20e05"},
                {"offset_or_value_consumed", false}, {"named_lookup_executed", false},
                {"required_runtime_gate", "signed Action field state >=2, correct physical cache20, valid native class tables"},
                {"probe_state2_is_explicit_owned_input", true},
                {"native_cache_state_or_load_history_assumed", false},
                {"all_dynamic_consumers_qualified", false}});
            ++selected;
        }
        const auto storage = n == native.end() ? J("CLASS_NOT_SUPPLIED") :
            n->second->value("field_storage_status", "STATUS_NOT_SUPPLIED");
        const auto evidenceGate = n == native.end() ? "CLASS_NOT_SUPPLIED" : !declaration ?
            "SELECTED_ORDINAL_NOT_FILE_BACKED" : !identity ? "IMMUTABLE_DECLARATION_MISMATCH" : "MATCHED";
        J output = J::object({{"original_contract", issue}, {"schema", c.at("schema")},
            {"native_class_pointer", pointer}, {"native_field_storage_status", storage},
            {"native_declaration_evidence_gate", evidenceGate}, {"native_declaration", nativeField},
            {"native_declaration_identity_matches", identity}, {"consumer", consumer}});
        rows.as_array().push_back(std::move(output));
        const auto key = std::make_pair(category, fieldName);
        auto [g, inserted] = grouped.emplace(key, groups.size());
        if (inserted) groups.as_array().push_back(J::object({{"category", category}, {"field", fieldName},
            {"native_ordinal", issue.at("native_ordinal")}, {"physical_index", issue.at("physical_index")},
            {"native_type", issue.at("declaration").at("native_type")},
            {"declaration_count", issue.at("declaration").at("count")}, {"first_count", issue.at("first_descriptor").at("count")},
            {"current_count", issue.at("actual_descriptor").at("count")}, {"rows", 0u}}));
        auto& group = groups.at(g->second);
        group["rows"] = group.at("rows").as_uint() + 1;
    }
    return J::object({{"format", "Monstey.GadbRemainingCountContracts/1"},
        {"status", audit.at("status")}, {"original_totals", audit.at("totals")},
        {"database_sha256", audit.at("database_sha256")},
        {"original_diagnostic_count", audit.at("diagnostics").size()},
        {"excluded_ConditionalBuff_rows", excluded}, {"remaining_rows", rows.size()},
        {"field_groups", groups}, {"rows", rows}, {"selected_cached_absence_rows", selected},
        {"other_remaining_rows", rows.size() - selected},
        {"selected_native_code_profile", J::object({{"getter", "0x140d17c80"},
            {"complete_bytes", 1532}, {"index_callback", "0x141689c10"}, {"callback_AL", 20},
            {"paired_code_identity_qualified_separately", true},
            {"nonnull_slow_action_lookup", "NOT_TRANSLATED"},
            {"nonnull_child_pointer_resolution", "NOT_PERFORMED"},
            {"captured_file_backing_is_not_live_cache_history", true}})},
        {"class_name_selection_performed", false}, {"values_decoded", false},
        {"original_checker_changed", false}, {"runtime_verified", false}});
}
} // namespace Monstey
