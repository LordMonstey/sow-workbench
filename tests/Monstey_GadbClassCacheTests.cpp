#include "Monstey_FxWorkflow.hpp"
#include "Monstey_GadbClassCache.hpp"
#include <algorithm>
#include <filesystem>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
const std::string Monstey_TestSha(64, 'a');
J Field(const char *name, unsigned type, unsigned identity,
        unsigned flags = 0) {
  char code[17]{};
  std::snprintf(code, sizeof(code), "b0%02xc3cccccccccc", identity);
  return J::object({{"name", name},
                    {"hash", Monstey_Hex(Monstey_NativeHash(name))},
                    {"type_code", type},
                    {"array", 1},
                    {"flags", flags},
                    {"native_ordinal", identity},
                    {"native_ordinal_source", "native_index_callback"},
                    {"native_index_callback_ea", "0x140010000"},
                    {"native_index_callback_hex", code},
                    {"native_descriptor_ea", "0x142000000"},
                    {"native_descriptor_sha256", Monstey_TestSha},
                    {"ordinal", 99},
                    {"cached_ordinal", 99}});
}
J Reflection() {
  return J::object(
      {{"source_sha256", Monstey_TestSha},
       {"entries", J::array({J::object({{"name", "Native"},
                                        {"entry_ea", "0x142100000"},
                                        {"entry_sha256", Monstey_TestSha},
                                        {"fields_ea", "0x142000000"},
                                        {"count", 6},
                                        {"flags", "0x100000000"},
                                        {"parent_ea", "0x0"}})})},
       {"categories",
        J::object(
            {{"Native",
              J::object(
                  {{"name", "Native"},
                   {"flags", "0x100000000"},
                   {"field_count", 6},
                   {"unreadable_fields", 0},
                   {"fields",
                    J::array({Field("KeyName", 4, 0), Field("Start", 2, 1),
                              Field("Group", 4, 2, 0x80), Field("End", 2, 3),
                              Field("Value", 3, 4),
                              Field("Group", 4, 5, 0x200)})}})}})}});
}
J Descriptor(const char *name, unsigned type, unsigned identity,
             unsigned count = 1) {
  return J::object({{"hash", Monstey_Hex(Monstey_NativeHash(name))},
                    {"native_type", type},
                    {"native_ordinal", identity},
                    {"count", count}});
}
J Record(const char *name, const char *reference, bool named = true) {
  return J::object(
      {{"name", name},
       {"name_hash",
        named ? J(Monstey_Hex(Monstey_NativeHash(name))) : J(nullptr)},
       {"reference", reference},
       {"named", named},
       {"fields",
        J::array({Descriptor("KeyName", 4, 0), Descriptor("Start", 2, 1),
                  Descriptor("End", 2, 3), Descriptor("Value", 3, 4)})}});
}
J Layout() {
  auto records = J::array({Record("A", "first"), Record("B", "later")});
  std::sort(records.as_array().begin(), records.as_array().end(),
            [](const auto &a, const auto &b) {
              return Monstey_NativeHash(a["name"].as_string()) <
                     Monstey_NativeHash(b["name"].as_string());
            });
  return J::object(
      {{"format", "Monstey_GadbClassCacheLayout"},
       {"version", 1},
       {"source_sha256", Monstey_TestSha},
       {"categories", J::array({J::object({{"name", "Native"},
                                           {"schema", "NativeSchema"},
                                           {"records", records}})})}});
}
void Move(J &record) {
  auto value = record["fields"].as_array().back();
  record["fields"].as_array().pop_back();
  record["fields"].as_array().insert(record["fields"].as_array().begin(),
                                     value);
}
void Need(bool value, const char *message) { Monstey_Require(value, message); }
J Capture(const Monstey_Gadb &db) {
  Need(!db.Describe()["native_dirty"].as_bool(),
       "Capture requires actual published native record names/order");
  auto layout = J::object({{"format", "Monstey_GadbClassCacheLayout"},
                           {"version", 1},
                           {"source_sha256", db.Describe()["source_sha256"]},
                           {"categories", J::array()}});
  const auto categories = db.Categories();
  for (const auto &category : categories["rows"].as_array()) {
    auto records = J::array();
    for (std::size_t offset = 0;;) {
      const auto page =
          db.Records(static_cast<std::uint32_t>(category["index"].as_uint()),
                     {}, offset, 10000);
      for (const auto &row : page["rows"].as_array()) {
        const auto metadata = db.Metadata(row["native_reference"].as_string());
        auto fields = J::array();
        for (const auto &d : metadata["fields"].as_array())
          fields.as_array().push_back(
              J::object({{"hash", d["hash"]},
                         {"native_ordinal", d["native_ordinal"]},
                         {"native_type", d["native_type"]},
                         {"count", d["count"]}}));
        Need(std::all_of(
                 row["name"].as_string().begin(), row["name"].as_string().end(),
                 [](char c) { return static_cast<unsigned char>(c) < 128; }),
             "Synthetic fixture capture requires proved ASCII names");
        records.as_array().push_back(J::object(
            {{"name_hash",
              row["named"].as_bool()
                  ? J(Monstey_Hex(Monstey_NativeHash(row["name"].as_string())))
                  : J(nullptr)},
             {"name", row["name"]},
             {"reference", row["native_reference"]},
             {"named", row["named"]},
             {"fields", fields}}));
      }
      offset += page["rows"].size();
      if (offset >= page["total"].as_uint())
        break;
    }
    layout["categories"].as_array().push_back(
        J::object({{"name", category["name"]},
                   {"schema", category["schema"]},
                   {"records", records}}));
  }
  return layout;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Need(argc >= 3,
         "Pass a new results directory and the published fixture directory");
    const std::filesystem::path out(argv[1]);
    Need(!std::filesystem::exists(out), "Results directory already exists");
    std::filesystem::create_directories(out);
    auto report = J::object({{"status", "PASS"},
                             {"engine_executed", false},
                             {"groups", J::array()}});
    const auto test = [&](const char *name,
                          const std::function<void()> &action) {
      try {
        action();
      } catch (const std::exception &e) {
        throw Monstey_Error("test", std::string(name) + ": " + e.what());
      }
      report["groups"].as_array().push_back(name);
    };
    const auto reject = [](const std::function<void()> &action) {
      bool rejected = false;
      try {
        action();
      } catch (const std::exception &) {
        rejected = true;
      }
      Need(rejected, "Expected malformed input rejection");
    };
    test("complete_markers_skipped_identity_gaps_not_mutable_cache", [] {
      const auto r = Monstey_GadbClassCacheCheck(Reflection(), Layout());
      Need(r["status"].as_string() == "qualified" &&
               r["cached_slot_hazards"].as_uint() == 0 &&
               r["categories"][0u]["fields"].size() == 4,
           "Wrong control qualification");
    });
    test("later_reorder_reports_four_cached_slot_hazards", [] {
      auto l = Layout();
      Move(l["categories"][0u]["records"][1u]);
      const auto r = Monstey_GadbClassCacheCheck(Reflection(), l);
      Need(r["status"].as_string() == "unsafe" &&
               r["cached_slot_hazards"].as_uint() == 4,
           "Reorder was not distinguished");
    });
    test("later_delete_reports_out_of_descriptor_array", [] {
      auto l = Layout();
      l["categories"][0u]["records"][1u]["fields"].as_array().pop_back();
      const auto r = Monstey_GadbClassCacheCheck(Reflection(), l);
      Need(r["cached_slot_hazards"].as_uint() == 1 &&
               !r["diagnostics"][0u]["actual"]["present"].as_bool(),
           "Missing cached slot not detected");
    });
    test("first_reorder_is_direct_fallback_not_later_cache_failure", [] {
      auto l = Layout();
      Move(l["categories"][0u]["records"][0u]);
      const auto r = Monstey_GadbClassCacheCheck(Reflection(), l);
      Need(r["status"].as_string() == "qualified" &&
               r["cached_slot_hazards"].as_uint() == 0 &&
               r["categories"][0u]["fields"][0u]["direct_initial_mode"]
                       .as_string() == "hash_fallback",
           "Fallback conflated with unsafe slot");
    });
    test("first_named_takes_precedence_over_anonymous_prefix", [] {
      auto l = Layout();
      auto a = Record("", "anon", false);
      Move(a);
      l["categories"][0u]["records"].as_array().insert(
          l["categories"][0u]["records"].as_array().begin(), a);
      const auto r = Monstey_GadbClassCacheCheck(Reflection(), l);
      Need(r["cached_slot_hazards"].as_uint() == 4 &&
               r["categories"][0u]["first_selection"].as_string() ==
                   "first_named" &&
               r["categories"][0u]["first_record"].as_string() != "anon",
           "Anonymous incorrectly selected");
    });
    test("anonymous_only_uses_first_anonymous", [] {
      auto l = Layout();
      for (auto &row : l["categories"][0u]["records"].as_array())
        row["named"] = false;
      const auto r = Monstey_GadbClassCacheCheck(Reflection(), l);
      Need(r["categories"][0u]["first_selection"].as_string() ==
               "first_anonymous",
           "Anonymous selection wrong");
    });
    test("variable_count_and_type11_accepts_type10", [] {
      auto f = Reflection();
      f["categories"]["Native"]["fields"][4u]["type_code"] = 11;
      f["categories"]["Native"]["fields"][4u]["array"] = -1;
      auto l = Layout();
      for (auto &row : l["categories"][0u]["records"].as_array())
        row["fields"][3u]["native_type"] = 10;
      l["categories"][0u]["records"][1u]["fields"][3u]["count"] = 65535;
      Need(Monstey_GadbClassCacheCheck(f, l)["cached_slot_hazards"].as_uint() ==
               0,
           "Native list/count predicates wrong");
    });
    test("fixed_count_mismatch_and_masked_string_type", [] {
      auto l = Layout();
      l["categories"][0u]["records"][1u]["fields"][3u]["count"] = 2;
      l["categories"][0u]["records"][1u]["fields"][0u]["native_type"] = 68;
      const auto r = Monstey_GadbClassCacheCheck(Reflection(), l);
      Need(r["cached_slot_hazards"].as_uint() == 0 &&
               r["unqualified_value_contracts"].as_uint() == 1 &&
               r["status"].as_string() == "unknown" &&
               r["unknown_categories"].as_uint() == 1 &&
               !r["diagnostics"][0u]["actual"]["count_match"].as_bool(),
           "Count-only contract drift was confused with a wrong physical slot");
    });
    test("category_flag4_selects_direct_fallback", [] {
      auto f = Reflection();
      f["categories"]["Native"]["flags"] = 4;
      f["entries"][0u]["flags"] = 4;
      auto l = Layout();
      Move(l["categories"][0u]["records"][1u]);
      Need(Monstey_GadbClassCacheCheck(f, l)["cached_slot_hazards"].as_uint() ==
               0,
           "Flag4 fallback ignored");
    });
    test("mutable_ordinal_cannot_substitute_callback_identity", [] {
      auto f = Reflection();
      f["categories"]["Native"]["fields"][0u].as_object().erase(
          "native_ordinal_source");
      Need(Monstey_GadbClassCacheCheck(f, Layout())["status"].as_string() ==
               "unknown",
           "Unqualified cache ordinal was trusted");
    });
    test("callback_identity_mismatch_and_unsupported_code_are_unknown", [] {
      for (const auto code : {"b009c3cccccccccc", "e900000000cccccc"}) {
        auto f = Reflection();
        f["categories"]["Native"]["fields"][0u]["native_index_callback_hex"] =
            code;
        Need(Monstey_GadbClassCacheCheck(f, Layout())["status"].as_string() ==
                 "unknown",
             "Bad callback was trusted");
      }
    });
    test("incomplete_duplicate_or_unreadable_reflection_is_unknown", [] {
      for (unsigned mode = 0; mode < 3; ++mode) {
        auto f = Reflection();
        if (mode == 0)
          f["categories"]["Native"]["fields"].as_array().pop_back();
        if (mode == 1)
          f["categories"]["Native"]["fields"][1u] =
              f["categories"]["Native"]["fields"][0u];
        if (mode == 2)
          f["categories"]["Native"]["unreadable_fields"] = 1;
        Need(Monstey_GadbClassCacheCheck(f, Layout())["status"].as_string() ==
                 "unknown",
             "Incomplete table was trusted");
      }
    });
    test("missing_or_ambiguous_entry_and_bad_image_pin_are_unknown", [] {
      for (unsigned mode = 0; mode < 3; ++mode) {
        auto f = Reflection();
        if (mode == 0)
          f["entries"] = J::array();
        if (mode == 1)
          f["entries"].as_array().push_back(f["entries"][0u]);
        if (mode == 2)
          f["source_sha256"] = "bad";
        Need(Monstey_GadbClassCacheCheck(f, Layout())["status"].as_string() ==
                 "unknown",
             "Missing provenance was trusted");
      }
    });
    test("every_unknown_category_and_empty_category_is_reported", [] {
      auto l = Layout();
      auto empty = l["categories"][0u];
      empty["records"] = J::array();
      l["categories"].as_array().push_back(
          J::object({{"name", "Unknown"}, {"records", J::array()}}));
      l["categories"][0u] = empty;
      const auto r = Monstey_GadbClassCacheCheck(Reflection(), l);
      Need(r["category_count"].as_uint() == 2 &&
               r["qualified_categories"].as_uint() == 1 &&
               r["unknown_categories"].as_uint() == 1,
           "Coverage omitted category");
    });
    test("ancestor_descendant_and_duplicate_binding_states_stay_unknown", [] {
      auto f = Reflection();
      f["categories"]["Native"]["parent"] = "Base";
      Need(Monstey_GadbClassCacheCheck(f, Layout())["status"].as_string() ==
               "unknown",
           "Parent state guessed");
      auto l = Layout();
      l["categories"].as_array().push_back(l["categories"][0u]);
      Need(Monstey_GadbClassCacheCheck(Reflection(), l)["unknown_categories"]
                   .as_uint() == 2,
           "Duplicate shared binding state guessed");
    });
    test("named_native_order_and_anonymous_prefix_are_validated", [&] {
      auto l = Layout();
      std::reverse(l["categories"][0u]["records"].as_array().begin(),
                   l["categories"][0u]["records"].as_array().end());
      reject([&] { (void)Monstey_GadbClassCacheCheck(Reflection(), l); });
      l = Layout();
      l["categories"][0u]["records"][1u]["named"] = false;
      reject([&] { (void)Monstey_GadbClassCacheCheck(Reflection(), l); });
    });
    test("bounded_diagnostics_do_not_truncate_coverage", [] {
      auto l = Layout();
      Move(l["categories"][0u]["records"][1u]);
      for (const std::size_t limit : {0u, 1u, 3u}) {
        const auto r = Monstey_GadbClassCacheCheck(Reflection(), l, limit);
        Need(r["diagnostics"].size() == limit &&
                 r["diagnostics_total"].as_uint() == 4 &&
                 r["records_checked"].as_uint() == 2 &&
                 r["descriptor_rows_checked"].as_uint() == 8 &&
                 r["diagnostics_truncated"].as_bool(),
             "Bounded details lost coverage");
      }
    });
    test("descriptor_uint8_uint16_widths_are_checked", [&] {
      for (const auto key : {"native_type", "native_ordinal", "count"}) {
        auto l = Layout();
        l["categories"][0u]["records"][0u]["fields"][0u][key] = 65536;
        reject([&] { (void)Monstey_GadbClassCacheCheck(Reflection(), l); });
      }
    });
    test("prepared_declarations_reuse_without_carrying_binding_results", [] {
      const Monstey_GadbClassCache prepared(Reflection());
      auto bad = Layout();
      Move(bad["categories"][0u]["records"][1u]);
      Need(prepared.Check(bad)["cached_slot_hazards"].as_uint() == 4,
           "Prepared bad case lost hazards");
      Need(prepared.Check(Layout())["cached_slot_hazards"].as_uint() == 0,
           "Previous audit contaminated immutable declaration context");
    });
    test("prepared_declarations_own_provenance_after_input_mutation", [] {
      auto source = Reflection();
      const Monstey_GadbClassCache prepared(source);
      source["source_sha256"] = "changed";
      source["categories"]["Native"]["fields"] = J::array();
      const auto r = prepared.Check(Layout());
      Need(r["status"].as_string() == "qualified" &&
               r["reflection_source_sha256"].as_string() == Monstey_TestSha,
           "Prepared declarations retained borrowed JSON");
    });
    test("streamed_global_duplicate_binding_context_is_not_lost", [&] {
      auto l = Layout();
      l["binding_name_counts"] = J::object({{"Native", 2}});
      const auto r = Monstey_GadbClassCache(Reflection()).Check(l);
      Need(r["unknown_categories"].as_uint() == 1,
           "Streaming lost shared binding context");
      l["binding_name_counts"]["Native"] = 0;
      reject([&] { (void)Monstey_GadbClassCacheCheck(Reflection(), l); });
    });
    test("unsafe_prefix_does_not_hide_later_unknown_category", [] {
      auto l = Layout();
      Move(l["categories"][0u]["records"][1u]);
      l["categories"].as_array().push_back(
          J::object({{"name", "Unknown"}, {"records", J::array()}}));
      const auto r = Monstey_GadbClassCacheCheck(Reflection(), l, 0);
      Need(r["unsafe_categories"].as_uint() == 1 &&
               r["unknown_categories"].as_uint() == 1 &&
               r["category_count"].as_uint() == 2 &&
               r["cached_slot_hazards"].as_uint() == 4,
           "Unsafe prefix stopped full category coverage");
    });
    test("missing_raw_name_hash_is_unknown_without_hashing_display", [] {
      auto l = Layout();
      l["categories"][0u]["records"][0u].as_object().erase("name_hash");
      const auto r = Monstey_GadbClassCacheCheck(Reflection(), l);
      Need(r["status"].as_string() == "unknown" &&
               r["field_predicates_checked"].as_uint() == 0,
           "Missing raw hash silently substituted");
    });
    test("display_name_bytes_do_not_control_native_named_order", [] {
      auto l = Layout();
      l["categories"][0u]["records"][0u]["name"] = "\\xff display differs";
      l["categories"][0u]["records"][1u]["name"] = "display z";
      Need(Monstey_GadbClassCacheCheck(Reflection(), l)["status"].as_string() ==
               "qualified",
           "Display name was rehashed instead of pinned native identity");
    });
    test("later_identity_only_drift_is_not_a_proved_wrong_slot", [] {
      auto layout = Layout();
      layout["categories"][0u]["records"][1u]["fields"][3u]["native_ordinal"] =
          9;
      const auto report = Monstey_GadbClassCacheCheck(Reflection(), layout);
      Need(report["cached_slot_hazards"].as_uint() == 0 &&
               report["unqualified_value_contracts"].as_uint() == 1 &&
               report["status"].as_string() == "unknown" &&
               report["diagnostics"][0u]["classification"].as_string() ==
                   "unknown_value_contract",
           "Later immutable-identity drift was confused with wrong physical "
           "data");
    });
    test("native_parent_pointer_requires_complete_coherent_association", [] {
      auto reflection = Reflection();
      auto parentCategory = reflection["categories"]["Native"];
      parentCategory["name"] = "Parent";
      reflection["categories"]["Parent"] = parentCategory;
      auto parentEntry = reflection["entries"][0u];
      parentEntry["name"] = "Parent";
      parentEntry["entry_ea"] = "0x142300000";
      reflection["entries"].as_array().push_back(parentEntry);
      reflection["entries"][0u]["parent_ea"] = "0x142300000";
      auto layout = Layout();
      auto parentLayout = layout["categories"][0u];
      parentLayout["name"] = "Parent";
      layout["categories"].as_array().push_back(parentLayout);
      const auto missing = Monstey_GadbClassCacheCheck(reflection, layout);
      Need(missing["qualified_categories"].as_uint() == 0 &&
               missing["unknown_categories"].as_uint() == 2 &&
               !missing["categories"][0u]["declaration_qualified"].as_bool() &&
               !missing["categories"][1u]
                       ["shared_ancestor_cache_state_qualified"]
                           .as_bool(),
           "Missing symbolic parent erased child or descendant cache boundary");
      reflection["categories"]["Native"]["parent"] = "Wrong";
      Need(Monstey_GadbClassCacheCheck(reflection,
                                       layout)["qualified_categories"]
                   .as_uint() == 0,
           "Contradictory symbolic parent qualified");
      reflection["categories"]["Native"]["parent"] = "Parent";
      const auto coherent = Monstey_GadbClassCacheCheck(reflection, layout);
      Need(coherent["categories"][0u]["declaration_qualified"].as_bool() &&
               coherent["unknown_categories"].as_uint() == 2,
           "Coherent native parent failed or shared state was simulated");
    });
    test("absent_unresolved_and_ambiguous_native_parent_stay_unknown", [] {
      auto reflection = Reflection();
      reflection["entries"][0u].as_object().erase("parent_ea");
      Need(Monstey_GadbClassCacheCheck(reflection, Layout())["status"]
                   .as_string() == "unknown",
           "Missing parent provenance qualified");
      reflection["entries"][0u]["parent_ea"] = "0x142300000";
      Need(Monstey_GadbClassCacheCheck(reflection, Layout())["status"]
                   .as_string() == "unknown",
           "Unresolved parent provenance qualified");
      auto entry = reflection["entries"][0u];
      entry["name"] = "Parent";
      entry["entry_ea"] = "0x142300000";
      reflection["entries"].as_array().push_back(entry);
      entry["name"] = "OtherParent";
      reflection["entries"].as_array().push_back(entry);
      reflection["categories"]["Native"]["parent"] = "Parent";
      Need(Monstey_GadbClassCacheCheck(reflection, Layout())["status"]
                   .as_string() == "unknown",
           "Ambiguous native parent qualified");
      reflection = Reflection();
      reflection["categories"]["Native"]["parent"] = "Parent";
      Need(Monstey_GadbClassCacheCheck(reflection, Layout())["status"]
                   .as_string() == "unknown",
           "Nonempty symbolic parent contradicted native root");
    });
    test("diagnostic_shows_fixed_count_and_owned_descriptor_metadata", [] {
      auto layout = Layout();
      layout["categories"][0u]["records"][1u]["fields"][3u]["count"] = 2;
      const auto result = Monstey_GadbClassCacheCheck(Reflection(), layout);
      const auto &diagnostic = result["diagnostics"][0u];
      Need(result["status"].as_string() == "unknown" &&
               diagnostic["declaration"]["count"].as_int() == 1 &&
               !diagnostic["declaration"]["count_wildcard"].as_bool() &&
               diagnostic["first_descriptor"]["count"].as_uint() == 1 &&
               diagnostic["actual_descriptor"]["count"].as_uint() == 2 &&
               diagnostic["mismatches"] == J::array({"count"}),
           "Count diagnostic does not show the compared metadata");
      layout["categories"][0u]["records"][1u]["fields"][3u]["count"] = 9;
      Need(diagnostic["actual_descriptor"]["count"].as_uint() == 2,
           "Returned diagnostics alias mutable input");
    });
    test("diagnostic_preserves_raw_type_flags_and_effective_type", [] {
      auto layout = Layout();
      auto &field = layout["categories"][0u]["records"][1u]["fields"][3u];
      field["native_type"] = 0xc3;
      field["count"] = 2;
      const auto result = Monstey_GadbClassCacheCheck(Reflection(), layout);
      const auto &diagnostic = result["diagnostics"][0u];
      Need(
          diagnostic["actual"]["type_match"].as_bool() &&
              diagnostic["actual_descriptor"]["native_type"].as_uint() ==
                  0xc3 &&
              diagnostic["actual_descriptor"]["effective_type"].as_uint() ==
                  3 &&
              diagnostic["mismatches"] == J::array({"count"}),
          "Raw descriptor type flags were discarded or called a type mismatch");
    });
    test("diagnostic_missing_slot_is_null_not_zero_descriptor", [] {
      auto layout = Layout();
      layout["categories"][0u]["records"][1u]["fields"].as_array().pop_back();
      const auto result = Monstey_GadbClassCacheCheck(Reflection(), layout);
      const auto &diagnostic = result["diagnostics"][0u];
      Need(diagnostic["classification"].as_string() == "unsafe_slot" &&
               diagnostic["actual_descriptor"].is_null() &&
               diagnostic["first_descriptor"]["count"].as_uint() == 1 &&
               diagnostic["mismatches"] == J::array({"missing_descriptor"}),
           "Missing physical descriptor was fabricated or reasons cascaded");
    });
    test("diagnostic_reordered_slots_show_both_field_hashes", [] {
      auto layout = Layout();
      Move(layout["categories"][0u]["records"][1u]);
      const auto result = Monstey_GadbClassCacheCheck(Reflection(), layout);
      Need(result["diagnostics"].size() == 4, "Expected four reordered slots");
      for (const auto &diagnostic : result["diagnostics"].as_array()) {
        const auto &reasons = diagnostic["mismatches"].as_array();
        Need(diagnostic["declaration"]["hash"] ==
                     diagnostic["first_descriptor"]["hash"] &&
                 diagnostic["actual_descriptor"]["hash"] ==
                     diagnostic["actual_field_hash"] &&
                 diagnostic["actual_descriptor"]["hash"] !=
                     diagnostic["declaration"]["hash"] &&
                 std::find(reasons.begin(), reasons.end(), J("field_hash")) !=
                     reasons.end(),
             "Reorder metadata does not identify the substituted native field");
      }
    });
    test("diagnostic_identity_only_difference_stays_unknown", [] {
      auto layout = Layout();
      layout["categories"][0u]["records"][1u]["fields"][0u]["native_ordinal"] =
          200;
      const auto result = Monstey_GadbClassCacheCheck(Reflection(), layout);
      const auto &diagnostic = result["diagnostics"][0u];
      Need(result["cached_slot_hazards"].as_uint() == 0 &&
               diagnostic["classification"].as_string() ==
                   "unknown_value_contract" &&
               diagnostic["declaration"]["native_ordinal"].as_uint() == 0 &&
               diagnostic["actual_descriptor"]["native_ordinal"].as_uint() ==
                   200 &&
               diagnostic["mismatches"] == J::array({"native_ordinal"}),
           "Identity metadata changed qualification or mismatch meaning");
    });
    test("diagnostic_zero_count_is_metadata_not_a_new_hazard", [] {
      auto layout = Layout();
      layout["categories"][0u]["records"][1u]["fields"][3u]["count"] = 0;
      const auto result = Monstey_GadbClassCacheCheck(Reflection(), layout);
      Need(result["status"].as_string() == "unknown" &&
               result["cached_slot_hazards"].as_uint() == 0 &&
               result["diagnostics"][0u]["actual_descriptor"]["count"]
                       .as_uint() == 0,
           "Zero-count value contract was invented as a proved unsafe slot");
    });
    test("diagnostic_wildcard_and_type11_coercion_are_explicit", [] {
      auto reflection = Reflection();
      reflection["categories"]["Native"]["fields"][4u]["type_code"] = 11;
      reflection["categories"]["Native"]["fields"][4u]["array"] = -1;
      auto layout = Layout();
      for (auto &record : layout["categories"][0u]["records"].as_array())
        record["fields"][3u]["native_type"] = 10;
      auto &actual = layout["categories"][0u]["records"][1u]["fields"][3u];
      actual["native_ordinal"] = 200;
      actual["count"] = 0;
      const auto result = Monstey_GadbClassCacheCheck(reflection, layout);
      const auto &diagnostic = result["diagnostics"][0u];
      Need(diagnostic["declaration"]["count"].as_int() == -1 &&
               diagnostic["declaration"]["count_wildcard"].as_bool() &&
               diagnostic["declaration"]["native_type"].as_uint() == 11 &&
               diagnostic["actual_descriptor"]["effective_type"].as_uint() ==
                   10 &&
               diagnostic["actual"]["type_match"].as_bool() &&
               diagnostic["actual"]["count_match"].as_bool() &&
               diagnostic["mismatches"] == J::array({"native_ordinal"}),
           "Diagnostic conflates wildcard cardinality or native type11/type10 "
           "coercion");
    });
    test("diagnostic_limit_preserves_complete_coverage_with_extra_details", [] {
      auto layout = Layout();
      Move(layout["categories"][0u]["records"][1u]);
      for (std::size_t limit = 0; limit <= 1; ++limit) {
        const auto result =
            Monstey_GadbClassCacheCheck(Reflection(), layout, limit);
        Need(result["diagnostics"].size() == limit &&
                 result["diagnostics_total"].as_uint() == 4 &&
                 result["cached_slot_hazards"].as_uint() == 4 &&
                 result["diagnostics_truncated"].as_bool(),
             "Detailed diagnostics changed the bounded coverage contract");
      }
    });
    test("published_real_writer_counterexamples_checked_without_engine", [&] {
      const std::filesystem::path cases(argv[2]);
      auto f = Reflection();
      auto declaration = f["categories"]["Native"];
      declaration["name"] = "ClientFX.NativeLayout";
      declaration["fields"][1u]["name"] = "StartTime";
      declaration["fields"][1u]["hash"] =
          Monstey_Hex(Monstey_NativeHash("StartTime"));
      declaration["fields"][3u]["name"] = "EndTime";
      declaration["fields"][3u]["hash"] =
          Monstey_Hex(Monstey_NativeHash("EndTime"));
      f["categories"] = J::object({{"ClientFX.NativeLayout", declaration}});
      f["entries"][0u]["name"] = "ClientFX.NativeLayout";
      for (const auto name :
           {"control", "later-reorder", "later-delete", "first-reorder"}) {
        Monstey_Gadb db(cases / (std::string(name) + ".fxp"),
                        cases / L"synthetic-schema.json");
        const auto layout = Capture(db);
        const auto r = Monstey_GadbClassCacheCheck(f, layout);
        const std::string which(name);
        const auto expected = which == "later-reorder"  ? 4u
                              : which == "later-delete" ? 1u
                                                        : 0u;
        Need(r["cached_slot_hazards"].as_uint() == expected,
             "Published fixture classification differs");
        Monstey_WriteNewText(out / (which + ".json"), r.dump(2) + "\n");
      }
    });
    report["group_count"] = report["groups"].size();
    Monstey_WriteNewText(out / L"report.json", report.dump(2) + "\n");
    std::cout << report.dump(2) << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
