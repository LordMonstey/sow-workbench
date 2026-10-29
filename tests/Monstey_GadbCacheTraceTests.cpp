#include "Monstey_GadbClassCache.hpp"
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
J Monstey_Field(std::uint32_t state = 0) {
  return J::object({{"hash", "0x89abcdef"},
                    {"type", 11},
                    {"count", "0xffffffff"},
                    {"flags", 6},
                    {"cache", 7},
                    {"state", state},
                    {"native_ordinal", 12}});
}
J Monstey_Event(const char *op = "success") {
  return J::object({{"operation", op},
                    {"class", "C"},
                    {"field", "c"},
                    {"declaration_index", 0}});
}
J Monstey_Input() {
  return J::object(
      {{"format", "Monstey_GadbCachePropagationTrace"},
       {"version", 1},
       {"classes",
        J::object({{"C", J::object({{"parent", "P"},
                                    {"field_count", 1},
                                    {"fields", J::array({"c"})}})},
                   {"P", J::object({{"parent", nullptr},
                                    {"field_count", 1},
                                    {"fields", J::array({"p"})}})}})},
       {"fields", J::object({{"c", Monstey_Field()}, {"p", Monstey_Field()}})},
       {"events", J::array({Monstey_Event()})}});
}
void Monstey_Check(bool b, const char *m) { Monstey_Require(b, m); }
void Monstey_Status(const J &r, const char *s) {
  Monstey_Check(r.at("status").as_string() == s, "Unexpected trace status");
}
std::uint64_t Monstey_State(const J &r, const char *id) {
  return r.at("final_fields").at(id).at("state").as_uint();
}
void Monstey_Reject(const std::function<void()> &fn) {
  bool bad = false;
  try {
    fn();
  } catch (const std::exception &) {
    bad = true;
  }
  Monstey_Check(bad, "Malformed input unexpectedly accepted");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Require(argc == 2, "Pass NEW native trace test report");
    J passed = J::array();
    const auto Test = [&](const char *n, const auto &fn) {
      try {
        fn();
      } catch (const std::exception &e) {
        throw Monstey_Error("test", std::string(n) + ": " + e.what());
      }
      passed.as_array().push_back(n);
    };
    Test("success_zero_propagates_to_zero_parent", [] {
      auto i = Monstey_Input();
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "complete");
      Monstey_Check(Monstey_State(r, "c") == 2 && Monstey_State(r, "p") == 2 &&
                        r.at("state_writes").size() == 2,
                    "Success did not propagate exact state2");
    });
    Test("success_nonzero_source_returns_before_parent_read", [] {
      for (auto state : {1u, 2u, 3u, 0x7fffffffu, 0x80000000u, 0xffffffffu}) {
        auto i = Monstey_Input();
        i["fields"]["c"]["state"] = state;
        i["events"][0u]["class"] = nullptr;
        auto r = Monstey_GadbClassCacheTrace(i);
        Monstey_Status(r, "complete");
        Monstey_Check(
            r.at("state_writes").empty() && Monstey_State(r, "c") == state,
            "Nonzero source was rewritten or null class dereferenced");
      }
    });
    Test("fallback_changes_zero_two_and_signed_high_bits", [] {
      for (auto state : {0u, 2u, 3u, 0x80000000u, 0xffffffffu}) {
        auto i = Monstey_Input();
        i["fields"]["c"]["state"] = state;
        i["events"] = J::array({Monstey_Event("fallback")});
        auto r = Monstey_GadbClassCacheTrace(i);
        Monstey_Status(r, "complete");
        Monstey_Check(Monstey_State(r, "c") == 1 && Monstey_State(r, "p") == 1,
                      "Fallback incorrectly uses signed state ordering");
      }
    });
    Test("fallback_one_stops_without_class", [] {
      auto i = Monstey_Input();
      i["fields"]["c"]["state"] = 1;
      i["events"] = J::array({Monstey_Event("fallback")});
      i["events"][0u].as_object().erase("class");
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "complete");
      Monstey_Check(r.at("state_writes").empty(),
                    "Fallback state1 was rewritten");
    });
    Test("fallback_is_sticky_across_explicit_later_success", [] {
      auto i = Monstey_Input();
      i["events"] =
          J::array({Monstey_Event("fallback"), Monstey_Event("success")});
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "complete");
      Monstey_Check(Monstey_State(r, "c") == 1 && Monstey_State(r, "p") == 1 &&
                        r.at("state_writes").size() == 2,
                    "Later success revived fallback");
    });
    Test("success_parent_one_or_two_stops_after_source_write", [] {
      for (auto state : {1u, 2u}) {
        auto i = Monstey_Input();
        i["fields"]["p"]["state"] = state;
        auto r = Monstey_GadbClassCacheTrace(i);
        Monstey_Status(r, "complete");
        Monstey_Check(Monstey_State(r, "c") == 2 &&
                          Monstey_State(r, "p") == state &&
                          r.at("state_writes").size() == 1,
                      "Parent stop state changed");
      }
    });
    Test("parent_tuple_is_checked_before_parent_stop_state", [] {
      auto i = Monstey_Input();
      i["fields"]["p"]["state"] = 1;
      i["fields"]["p"]["hash"] = 4;
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "would_assert");
      Monstey_Check(Monstey_State(r, "c") == 2 && Monstey_State(r, "p") == 1,
                    "Prior write or parent state corrupted");
    });
    Test("all_five_exact_correspondence_mismatches_assert", [] {
      for (auto key : {"hash", "type", "flags", "count", "cache"}) {
        auto i = Monstey_Input();
        i["fields"]["p"][key] = 3;
        i["events"].as_array().push_back(Monstey_Event("fallback"));
        auto r = Monstey_GadbClassCacheTrace(i);
        Monstey_Status(r, "would_assert");
        Monstey_Check(
            Monstey_State(r, "c") == 2 && Monstey_State(r, "p") == 0 &&
                r.at("events_started").as_uint() == 1,
            "Assert did not preserve partial write/stop later events");
      }
    });
    Test("fallback_mismatch_preserves_its_prior_write", [] {
      auto i = Monstey_Input();
      i["fields"]["c"]["state"] = 2;
      i["fields"]["p"]["cache"] = 8;
      i["events"] = J::array({Monstey_Event("fallback")});
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "would_assert");
      Monstey_Check(Monstey_State(r, "c") == 1 && Monstey_State(r, "p") == 0,
                    "Fallback partial state lost");
    });
    Test("comparison_order_does_not_read_missing_later_property", [] {
      auto i = Monstey_Input();
      i["fields"]["p"]["hash"] = 3;
      i["fields"]["p"].as_object().erase("cache");
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "would_assert");
      Monstey_Check(r.at("steps")[0u].at("correspondence").size() == 1,
                    "Compared later properties after first mismatch");
    });
    Test("missing_binding_state_is_unknown_not_zero", [] {
      auto i = Monstey_Input();
      i["fields"]["c"].as_object().erase("state");
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "unknown");
      Monstey_Check(r.at("state_writes").empty() &&
                        !r.at("final_fields").at("c").contains("state"),
                    "Missing state was defaulted");
    });
    Test("missing_needed_cache_is_unknown_after_source_write", [] {
      auto i = Monstey_Input();
      i["fields"]["p"].as_object().erase("cache");
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "unknown");
      Monstey_Check(Monstey_State(r, "c") == 2 && Monstey_State(r, "p") == 0,
                    "Unknown comparison lost partial source write");
    });
    Test("unused_missing_cache_does_not_invent_a_read", [] {
      auto i = Monstey_Input();
      i["fields"]["c"]["state"] = 2;
      i["fields"]["c"].as_object().erase("cache");
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "complete");
      Monstey_Check(!r.at("final_fields").at("c").contains("cache"),
                    "Unused cache was fabricated");
    });
    Test("missing_parent_pointer_unknown_after_source_write", [] {
      auto i = Monstey_Input();
      i["classes"]["C"].as_object().erase("parent");
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "unknown");
      Monstey_Check(Monstey_State(r, "c") == 2,
                    "Unknown parent rolled back native source write");
    });
    Test("null_parent_returns_without_index_or_metadata", [] {
      auto i = Monstey_Input();
      i["classes"]["C"]["parent"] = nullptr;
      i["fields"]["c"] = J::object({{"state", 0}});
      i["events"][0u].as_object().erase("declaration_index");
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "complete");
      Monstey_Check(Monstey_State(r, "c") == 2, "Null parent path changed");
    });
    Test("parent_count_boundary_precedes_null_table", [] {
      for (auto idx : {0u, 1u, UINT32_MAX}) {
        auto i = Monstey_Input();
        i["classes"]["P"]["field_count"] = idx;
        i["classes"]["P"]["fields"] = nullptr;
        i["events"][0u]["declaration_index"] = idx;
        auto r = Monstey_GadbClassCacheTrace(i);
        Monstey_Status(r, "complete");
        Monstey_Check(Monstey_State(r, "c") == 2 && Monstey_State(r, "p") == 0,
                      "Parent count exit dereferenced table");
      }
    });
    Test("original_index_does_not_become_compact_cache", [] {
      auto i = Monstey_Input();
      i["classes"]["P"]["field_count"] = 2;
      i["classes"]["P"]["fields"] = J::array({"wrong", "p"});
      i["fields"]["wrong"] = Monstey_Field();
      i["fields"]["wrong"]["hash"] = 4;
      i["events"][0u]["declaration_index"] = 1;
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "complete");
      Monstey_Check(Monstey_State(r, "p") == 2 &&
                        Monstey_State(r, "wrong") == 0,
                    "Parent selected compact/cache or wrong original index");
    });
    Test("success_cycle_terminates_on_already_written_state", [] {
      auto i = Monstey_Input();
      i["classes"]["P"]["parent"] = "C";
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "complete");
      Monstey_Check(r.at("steps").size() == 3 &&
                        r.at("state_writes").size() == 2,
                    "Cycle did not follow native state stop");
    });
    Test("fallback_cycle_terminates_on_sticky_state", [] {
      auto i = Monstey_Input();
      i["classes"]["P"]["parent"] = "C";
      i["events"] = J::array({Monstey_Event("fallback")});
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "complete");
      Monstey_Check(r.at("steps").size() == 3 &&
                        r.at("state_writes").size() == 2,
                    "Fallback cycle not stopped natively");
    });
    Test("aliased_parent_field_preserves_single_owned_state", [] {
      auto i = Monstey_Input();
      i["classes"]["P"]["fields"] = J::array({"c"});
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "complete");
      Monstey_Check(r.at("state_writes").size() == 1 &&
                        r.at("steps").size() == 2,
                    "Alias was copied into independent state");
    });
    Test("explicit_null_field_dereference_path_has_no_write", [] {
      auto i = Monstey_Input();
      i["events"][0u]["field"] = nullptr;
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "null_dereference_path");
      Monstey_Check(r.at("state_writes").empty(),
                    "Null source field was written");
    });
    Test("explicit_null_class_preserves_prior_source_write", [] {
      auto i = Monstey_Input();
      i["events"][0u]["class"] = nullptr;
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "null_dereference_path");
      Monstey_Check(Monstey_State(r, "c") == 2,
                    "Write before null class read lost");
    });
    Test("explicit_null_parent_table_differs_from_missing_row", [] {
      auto i = Monstey_Input();
      i["classes"]["P"]["fields"] = nullptr;
      Monstey_Status(Monstey_GadbClassCacheTrace(i), "null_dereference_path");
      i = Monstey_Input();
      i["classes"]["P"]["fields"] = J::array({nullptr});
      Monstey_Status(Monstey_GadbClassCacheTrace(i), "unknown");
    });
    Test("missing_parent_table_is_unknown_after_write", [] {
      auto i = Monstey_Input();
      i["classes"]["P"].as_object().erase("fields");
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "unknown");
      Monstey_Check(Monstey_State(r, "c") == 2,
                    "Unknown table lost prior write");
    });
    Test("missing_class_and_parent_nodes_stay_unknown", [] {
      auto i = Monstey_Input();
      i["events"][0u]["class"] = "missing";
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "unknown");
      Monstey_Check(Monstey_State(r, "c") == 2, "Unknown class write lost");
      i = Monstey_Input();
      i["classes"]["C"]["parent"] = "missing";
      Monstey_Status(Monstey_GadbClassCacheTrace(i), "unknown");
    });
    Test("step_budget_preserves_exact_prefix", [] {
      auto i = Monstey_Input();
      auto r = Monstey_GadbClassCacheTrace(i, 0);
      Monstey_Status(r, "unknown");
      Monstey_Check(r.at("state_writes").empty(), "Zero budget wrote state");
      r = Monstey_GadbClassCacheTrace(i, 1);
      Monstey_Status(r, "unknown");
      Monstey_Check(Monstey_State(r, "c") == 2 && Monstey_State(r, "p") == 0,
                    "Budget prefix was rolled back or continued");
    });
    Test("immutable_callback_identity_not_compared_as_cache", [] {
      auto i = Monstey_Input();
      i["fields"]["c"]["native_ordinal"] = 99;
      i["fields"]["p"]["native_ordinal"] = 1;
      Monstey_Status(Monstey_GadbClassCacheTrace(i), "complete");
    });
    Test("explicit_helper_does_not_invent_outer_marker_skip", [] {
      auto i = Monstey_Input();
      i["fields"]["c"]["flags"] = 0x280;
      i["fields"]["p"]["flags"] = 0x280;
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "complete");
      Monstey_Check(r.at("state_writes").size() == 2,
                    "Outer binder marker gate was invented inside helper");
    });
    Test("missing_event_history_is_unknown_and_empty_is_explicit", [] {
      auto i = Monstey_Input();
      i.as_object().erase("events");
      Monstey_Status(Monstey_GadbClassCacheTrace(i), "unknown");
      i["events"] = J::array();
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "complete");
      Monstey_Check(r.at("state_writes").empty(),
                    "Empty event list inferred calls");
    });
    Test("missing_operation_is_unknown_without_writes", [] {
      auto i = Monstey_Input();
      i["events"][0u].as_object().erase("operation");
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "unknown");
      Monstey_Check(r.at("state_writes").empty(), "Unknown outcome guessed");
    });
    Test("all_input_and_result_ownership_is_independent", [] {
      auto i = Monstey_Input(), copy = i;
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Check(i == copy, "Input mutated");
      r["final_fields"]["c"]["hash"] = 7;
      Monstey_Check(i == copy, "Output aliases input");
      auto a = Monstey_GadbClassCacheTrace(i),
           b = Monstey_GadbClassCacheTrace(i);
      Monstey_Check(a == b, "Repeated trace retains hidden mutable state");
    });
    Test("malformed_widths_types_and_events_reject_immutably", [] {
      const auto Monstey_OwnedRejectionCases1 = J::array({-1, true, 1.5, static_cast<std::uint64_t>(UINT32_MAX) + 1,
                     "0x100000000"}); for (const auto &v : Monstey_OwnedRejectionCases1.as_array()) {
        auto i = Monstey_Input();
        i["fields"]["c"]["state"] = v;
        const auto c = i;
        Monstey_Reject([&] { (void)Monstey_GadbClassCacheTrace(i); });
        Monstey_Check(i == c, "Failure changed input");
      }
      auto i = Monstey_Input();
      i["fields"]["c"]["cache"] = 256;
      Monstey_Reject([&] { (void)Monstey_GadbClassCacheTrace(i); });
      i = Monstey_Input();
      i["events"][0u]["operation"] = "initialize";
      Monstey_Reject([&] { (void)Monstey_GadbClassCacheTrace(i); });
      i = Monstey_Input();
      i["force_fallback"] = true;
      Monstey_Reject([&] { (void)Monstey_GadbClassCacheTrace(i); });
      Monstey_Reject(
          [&] { (void)Monstey_GadbClassCacheTrace(Monstey_Input(), 1000001); });
    });
    Test("cycle_reporting_marks_only_actual_revisit", [] {
      auto i = Monstey_Input();
      i["classes"]["P"]["parent"] = "C";
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Check(
          !r.at("steps")[0u].at("revisited_class_field_pair").as_bool() &&
              !r.at("steps")[1u].at("revisited_class_field_pair").as_bool() &&
              r.at("steps")[2u].at("revisited_class_field_pair").as_bool(),
          "Cycle report marked an unvisited parent");
    });
    Test("hex_high_state_stops_without_narrowing", [] {
      auto i = Monstey_Input();
      i["fields"]["c"]["state"] = "0xffffffff";
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "complete");
      Monstey_Check(r.at("final_fields").at("c").at("state").as_string() ==
                            "0xffffffff" &&
                        r.at("state_writes").empty(),
                    "Raw hex state was coerced");
    });
    Test("grandparent_assert_retains_all_prior_writes", [] {
      auto i = Monstey_Input();
      i["classes"]["P"]["parent"] = "G";
      i["classes"]["G"] = J::object({{"parent", nullptr},
                                     {"field_count", 1},
                                     {"fields", J::array({"g"})}});
      i["fields"]["g"] = Monstey_Field();
      i["fields"]["g"]["flags"] = 3;
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "would_assert");
      Monstey_Check(Monstey_State(r, "c") == 2 && Monstey_State(r, "p") == 2 &&
                        Monstey_State(r, "g") == 0 &&
                        r.at("state_writes").size() == 2,
                    "Grandparent assertion rolled back prefix or wrote the "
                    "unvisited state");
    });
    Test("missing_parent_state_unknown_after_metadata_and_prior_write", [] {
      auto i = Monstey_Input();
      i["fields"]["p"].as_object().erase("state");
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "unknown");
      Monstey_Check(Monstey_State(r, "c") == 2 &&
                        !r.at("final_fields").at("p").contains("state"),
                    "Parent state was guessed or source write rolled back");
    });
    Test("null_table_nonzero_derived_address_stays_unknown", [] {
      auto i = Monstey_Input();
      i["classes"]["P"]["field_count"] = 2;
      i["classes"]["P"]["fields"] = nullptr;
      i["events"][0u]["declaration_index"] = 1;
      auto r = Monstey_GadbClassCacheTrace(i);
      Monstey_Status(r, "unknown");
      Monstey_Check(
          Monstey_State(r, "c") == 2 &&
              r.at("steps")[0u].at("native_row_byte_offset").as_uint() == 72,
          "A nonzero null-base derived address was guessed unmapped");
    });
    Monstey_WriteNewText(argv[1],
                         J::object({{"status", "PASS"},
                                    {"groups", passed},
                                    {"count", passed.size()},
                                    {"original_calls", false},
                                    {"runtime_history_replayed", false}})
                                 .dump(2) +
                             "\n");
    std::cout << "PASS " << passed.size()
              << " source-qualified propagation groups\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
