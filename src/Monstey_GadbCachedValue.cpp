#include "Monstey_GadbCachedValue.hpp"
#include <array>
namespace Monstey {
std::uint64_t Monstey_GadbReadCachedQword(
    const Monstey_GadbCachedRecordView* record, std::uint8_t physical_index,
    std::uint32_t element_index, std::uint64_t fallback) {
    if (!record) return fallback;
    const auto descriptor = static_cast<std::size_t>(physical_index) * 16;
    const auto count = Monstey_U16(record->descriptors, descriptor + 12);
    if (element_index >= count) return fallback;
    const auto offset = Monstey_U16(record->descriptors, descriptor + 14);
    const auto word = static_cast<std::uint32_t>(offset) + element_index;
    Monstey_Require(word < record->values.size(), "Cached QWORD outside owned value storage");
    return record->values[word];
}
Monstey_Json Monstey_GadbConditionalBuffAbsence(const Monstey_Json& audit) {
    Monstey_Require(audit.at("format").as_string() == "Monstey_GadbClassCacheAudit" &&
                    audit.at("version").as_uint() == 1, "Expected native binding audit v1");
    Monstey_Require(!audit.at("diagnostics_truncated").as_bool(),
                    "Complete diagnostics are required for absence counts");
    auto rows = Monstey_Json::array();
    std::uint64_t selected = 0, qualified = 0;
    for (const auto& row : audit.at("diagnostics").as_array()) {
        if (row.at("category").as_string() != "Combat.Buff" ||
            row.at("field").as_string() != "ConditionalBuff") continue;
        ++selected;
        const auto& first = row.at("first_descriptor");
        const auto& actual = row.at("actual_descriptor");
        const auto& declared = row.at("declaration");
        const bool exact = row.at("classification").as_string() == "unknown_value_contract" &&
            row.at("physical_index").as_uint() == 4 &&
            row.at("native_ordinal").as_uint() == 5 &&
            row.at("mismatches").size() == 1 && row.at("mismatches").at(0).as_string() == "count" &&
            first.at("hash").as_string() == "0x5b57a121" && first.at("count").as_uint() == 1 &&
            first.at("native_type").as_uint() == 11 && first.at("native_ordinal").as_uint() == 5 &&
            actual.at("hash").as_string() == "0x5b57a121" && actual.at("count").as_uint() == 0 &&
            actual.at("native_type").as_uint() == 11 && actual.at("native_ordinal").as_uint() == 5 &&
            declared.at("hash").as_string() == "0x5b57a121" && declared.at("count").as_uint() == 1 &&
            declared.at("native_type").as_uint() == 11 && declared.at("native_ordinal").as_uint() == 5;
        auto witness = Monstey_Json::object({{"record", row.at("record")},
            {"qualified_selected_absence", exact}, {"original_classification", row.at("classification")}});
        if (exact) {
            // Deliberately no bytes at descriptor+14 and no value storage. This
            // witness cannot silently obtain a value from an absent element.
            std::array<std::uint8_t, 78> bytes{};
            Monstey_GadbCachedRecordView owned{bytes, {}};
            Monstey_Require(Monstey_GadbReadCachedQword(&owned, 4, 0, 0) == 0,
                            "Native zero-count fallback witness differs");
            Monstey_Require(Monstey_GadbReadCachedQword(nullptr, 255, UINT32_MAX, 0) == 0,
                            "Native null child getter witness differs");
            witness["cached_qword"] = "0x0000000000000000";
            witness["offset_or_value_read"] = false;
            witness["conditional_work_entered_by_two_callers"] = false;
            ++qualified;
        }
        rows.as_array().push_back(std::move(witness));
    }
    Monstey_Require(audit.at("totals").at("unqualified_value_contracts").as_uint() >= qualified,
                    "Selected absences exceed source contract counter");
    return Monstey_Json::object({
        {"format", "Monstey_GadbConditionalBuffAbsence"}, {"version", 1},
        {"database_sha256", audit.at("database_sha256")},
        {"scope", "selected_direct_cached_zero_count_and_two_native_null_consumers"},
        {"requires_correct_physical_field_cache", true},
        {"field_state_or_locale_simulated", false}, {"named_lookup_qualified", false},
        {"all_gameplay_consumers_claimed", false}, {"runtime_verified", false},
        {"original_checker_status", audit.at("status")},
        {"original_checker_totals", audit.at("totals")},
        {"original_classifications_preserved", true},
        {"selected_count_diagnostics", selected}, {"qualified_selected_absences", qualified},
        {"other_or_unqualified_contracts", audit.at("totals").at("unqualified_value_contracts").as_uint() - qualified},
        {"rows", rows}});
}
}
