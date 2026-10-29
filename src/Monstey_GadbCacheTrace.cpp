#include "Monstey_GadbClassCache.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
void Monstey_TraceKeys(const J &v,
                       std::initializer_list<std::string_view> allowed) {
  Monstey_Require(v.is_object(), "Trace node must be an object");
  for (const auto &[k, x] : v.as_object()) {
    (void)x;
    Monstey_Require(std::find(allowed.begin(), allowed.end(), k) !=
                        allowed.end(),
                    "Unknown trace input field: " + k);
  }
}
std::uint32_t Monstey_TraceU32(const J &v) {
  std::uint64_t n = 0;
  if (v.is_integer())
    n = v.as_uint();
  else {
    Monstey_Require(v.is_string(),
                    "Trace word must be an unsigned integer or hex string");
    const auto &t = v.as_string();
    Monstey_Require(t.starts_with("0x") && t.size() > 2 && t.size() <= 10,
                    "Trace word hex exceeds U32");
    const auto r = std::from_chars(t.data() + 2, t.data() + t.size(), n, 16);
    Monstey_Require(r.ec == std::errc{} && r.ptr == t.data() + t.size(),
                    "Invalid trace word hex");
  }
  Monstey_Require(n <= UINT32_MAX, "Trace word exceeds U32");
  return static_cast<std::uint32_t>(n);
}
void Monstey_TraceReference(const J &v) {
  Monstey_Require(v.is_null() || (v.is_string() && !v.as_string().empty() &&
                                  v.as_string().size() <= 256),
                  "Trace reference must be null or a nonempty bounded ID");
}
void Monstey_TraceValidate(const J &in) {
  Monstey_TraceKeys(
      in, {"format", "version", "classes", "fields", "events", "provenance"});
  Monstey_Require(in.at("format").as_string() ==
                          "Monstey_GadbCachePropagationTrace" &&
                      in.at("version").as_uint() == 1,
                  "Unknown propagation trace format/version");
  if (in.contains("fields") && !in.at("fields").is_null()) {
    Monstey_Require(in.at("fields").is_object() &&
                        in.at("fields").size() <= 65536,
                    "Trace field graph exceeds65536 nodes");
    for (const auto &[id, f] : in.at("fields").as_object()) {
      Monstey_TraceReference(J(id));
      Monstey_TraceKeys(f, {"hash", "type", "count", "flags", "cache", "state",
                            "native_ordinal", "provenance"});
      for (const auto &[k, v] : f.as_object())
        if (k != "provenance" && !v.is_null()) {
          const auto n = Monstey_TraceU32(v);
          if (k == "cache" || k == "native_ordinal")
            Monstey_Require(n <= 255, "Trace cache/identity exceeds U8");
        }
    }
  }
  if (in.contains("classes") && !in.at("classes").is_null()) {
    Monstey_Require(in.at("classes").is_object() &&
                        in.at("classes").size() <= 4096,
                    "Trace class graph exceeds4096 nodes");
    std::size_t suppliedRows = 0;
    for (const auto &[id, c] : in.at("classes").as_object()) {
      Monstey_TraceReference(J(id));
      Monstey_TraceKeys(c, {"parent", "field_count", "fields", "provenance"});
      if (c.contains("parent"))
        Monstey_TraceReference(c.at("parent"));
      if (c.contains("field_count") && !c.at("field_count").is_null())
        (void)Monstey_TraceU32(c.at("field_count"));
      if (c.contains("fields") && !c.at("fields").is_null()) {
        Monstey_Require(c.at("fields").is_array() &&
                            c.at("fields").size() <= 65536,
                        "Trace table exceeds65536 supplied rows");
        suppliedRows += c.at("fields").size();
        Monstey_Require(suppliedRows <= 1048576,
                        "Trace supplied field-table references exceed1048576");
        for (const auto &f : c.at("fields").as_array())
          Monstey_TraceReference(f);
      }
    }
  }
  if (in.contains("events") && !in.at("events").is_null()) {
    Monstey_Require(in.at("events").is_array() &&
                        in.at("events").size() <= 100000,
                    "Trace event list exceeds100000");
    for (const auto &e : in.at("events").as_array()) {
      Monstey_TraceKeys(e, {"operation", "class", "field", "declaration_index",
                            "provenance"});
      if (e.contains("operation") && !e.at("operation").is_null())
        Monstey_Require(
            e.at("operation").as_string() == "success" ||
                e.at("operation").as_string() == "fallback",
            "Only explicit success/fallback helper events are supported");
      for (auto key : {"class", "field"})
        if (e.contains(key))
          Monstey_TraceReference(e.at(key));
      if (e.contains("declaration_index") &&
          !e.at("declaration_index").is_null())
        (void)Monstey_TraceU32(e.at("declaration_index"));
    }
  }
}
struct Monstey_TraceRun {
  const J &in;
  J fields, steps = J::array(), writes = J::array(), events = J::array();
  std::size_t limit;
  std::string status = "complete", reason;
  bool terminal = false;
  std::size_t completed = 0;
  Monstey_TraceRun(const J &i, std::size_t n)
      : in(i), fields(i.contains("fields") && !i.at("fields").is_null()
                          ? i.at("fields")
                          : J::object()),
        limit(n) {}
  void Stop(J &row, std::string s, std::string why) {
    status = std::move(s);
    reason = std::move(why);
    terminal = true;
    row["stop"] = reason;
    row["status"] = status;
  }
  bool Have(const J &obj, std::string_view key) const {
    return obj.contains(key) && !obj.at(key).is_null();
  }
  void Event(std::size_t eventIndex, const J &e) {
    J er = J::object({{"event", eventIndex},
                      {"writes_before", writes.size()},
                      {"steps_before", steps.size()}});
    if (!Have(e, "operation")) {
      Stop(er, "unknown", "operation_not_supplied");
      events.as_array().push_back(er);
      return;
    }
    const bool success = e.at("operation").as_string() == "success";
    J classRef = e.value("class", J()), fieldRef = e.value("field", J());
    bool classSupplied = e.contains("class");
    std::set<std::pair<std::string, std::string>> seen;
    for (;;) {
      if (steps.size() >= limit) {
        Stop(er, "unknown", "step_limit_reached_before_next_native_state_read");
        break;
      }
      J row =
          J::object({{"event", eventIndex},
                     {"operation", success ? "success" : "fallback"},
                     {"class", classRef},
                     {"class_argument_supplied", classSupplied},
                     {"field", fieldRef},
                     {"native_site", success ? "0x140196CEE" : "0x141620302"}});
      if (!e.contains("field")) {
        Stop(row, "unknown", "source_field_argument_not_supplied");
        steps.as_array().push_back(row);
        break;
      }
      if (fieldRef.is_null()) {
        Stop(row, "null_dereference_path", "explicit_null_field_state_read");
        steps.as_array().push_back(row);
        break;
      }
      if (classRef.is_string())
        row["revisited_class_field_pair"] =
            !seen.emplace(classRef.as_string(), fieldRef.as_string()).second;
      const auto fieldID = fieldRef.as_string();
      if (!fields.contains(fieldID)) {
        Stop(row, "unknown", "field_node_not_supplied");
        steps.as_array().push_back(row);
        break;
      }
      auto &f = fields[fieldID];
      if (!Have(f, "state")) {
        Stop(row, "unknown", "binding_state_not_supplied");
        steps.as_array().push_back(row);
        break;
      }
      const auto before = Monstey_TraceU32(f.at("state"));
      row["state_before"] = before;
      if ((success && before != 0) || (!success && before == 1)) {
        row["status"] = "returned";
        row["stop"] = success ? "success_stops_on_nonzero_state"
                              : "fallback_stops_on_state1";
        steps.as_array().push_back(row);
        break;
      }
      const auto after = success ? 2u : 1u;
      f["state"] = after;
      row["state_after"] = after;
      writes.as_array().push_back(J::object(
          {{"event", eventIndex},
           {"field", fieldID},
           {"before", before},
           {"after", after},
           {"native_site", success ? "0x14109E6BA" : "0x141620308"}}));
      if (!classSupplied) {
        Stop(row, "unknown", "class_argument_not_supplied_after_state_write");
        steps.as_array().push_back(row);
        break;
      }
      if (classRef.is_null()) {
        Stop(row, "null_dereference_path",
             "explicit_null_class_parent_read_after_state_write");
        steps.as_array().push_back(row);
        break;
      }
      const auto classID = classRef.as_string();
      if (!in.at("classes").contains(classID)) {
        Stop(row, "unknown", "class_node_not_supplied_after_state_write");
        steps.as_array().push_back(row);
        break;
      }
      const auto &c = in.at("classes").at(classID);
      if (!c.contains("parent")) {
        Stop(row, "unknown", "parent_pointer_not_supplied_after_state_write");
        steps.as_array().push_back(row);
        break;
      }
      const auto parent = c.at("parent");
      row["parent"] = parent;
      if (parent.is_null()) {
        row["status"] = "returned";
        row["stop"] = "null_parent_return";
        steps.as_array().push_back(row);
        break;
      }
      if (!in.at("classes").contains(parent.as_string())) {
        Stop(row, "unknown", "parent_class_node_not_supplied");
        steps.as_array().push_back(row);
        break;
      }
      const auto &pc = in.at("classes").at(parent.as_string());
      if (!Have(pc, "field_count")) {
        Stop(row, "unknown", "parent_field_count_not_supplied");
        steps.as_array().push_back(row);
        break;
      }
      if (!Have(e, "declaration_index")) {
        Stop(row, "unknown", "original_declaration_index_not_supplied");
        steps.as_array().push_back(row);
        break;
      }
      const auto index = Monstey_TraceU32(e.at("declaration_index")),
                 count = Monstey_TraceU32(pc.at("field_count"));
      row["declaration_index"] = index;
      row["parent_field_count"] = count;
      if (index >= count) {
        row["status"] = "returned";
        row["stop"] = "parent_count_boundary_return";
        steps.as_array().push_back(row);
        break;
      }
      if (!pc.contains("fields")) {
        Stop(row, "unknown", "parent_field_table_not_supplied");
        steps.as_array().push_back(row);
        break;
      }
      if (pc.at("fields").is_null()) {
        Stop(row, index == 0 ? "null_dereference_path" : "unknown",
             index == 0
                 ? "explicit_null_parent_field_table_read"
                 : "null_table_nonzero_index_derived_address_not_supplied");
        row["native_row_byte_offset"] = 72ull * index;
        steps.as_array().push_back(row);
        break;
      }
      const auto &table = pc.at("fields");
      if (index >= table.size() || table[index].is_null()) {
        Stop(row, "unknown", "selected_parent_row_not_supplied");
        steps.as_array().push_back(row);
        break;
      }
      const auto parentFieldID = table[index].as_string();
      row["selected_parent_field"] = parentFieldID;
      if (!fields.contains(parentFieldID)) {
        Stop(row, "unknown", "selected_parent_field_node_not_supplied");
        steps.as_array().push_back(row);
        break;
      }
      const auto &pf = fields.at(parentFieldID);
      row["correspondence"] = J::array();
      bool matched = true;
      for (const auto key : {"hash", "type", "flags", "count", "cache"}) {
        if (!Have(f, key) || !Have(pf, key)) {
          Stop(row, "unknown",
               std::string("parent_correspondence_") + key + "_not_supplied");
          matched = false;
          break;
        }
        const auto src = Monstey_TraceU32(f.at(key)),
                   dst = Monstey_TraceU32(pf.at(key));
        row["correspondence"].as_array().push_back(
            J::object({{"property", key},
                       {"source", src},
                       {"parent", dst},
                       {"equal", src == dst}}));
        if (src != dst) {
          Stop(row, "would_assert",
               std::string("native_parent_") + key +
                   "_mismatch_assertion_path");
          row["native_assert_site"] = success ? "0x14109E710" : "0x141620356";
          matched = false;
          break;
        }
      }
      if (!matched) {
        steps.as_array().push_back(row);
        break;
      }
      row["status"] = "propagate";
      steps.as_array().push_back(row);
      classRef = parent;
      fieldRef = parentFieldID;
      classSupplied = true;
    }
    er["writes_after"] = writes.size();
    er["steps_after"] = steps.size();
    er["status"] = terminal ? status : "returned";
    if (terminal)
      er["stop"] = reason;
    else
      ++completed;
    events.as_array().push_back(er);
  }
  J Run() {
    if (!in.contains("classes") || in.at("classes").is_null() ||
        !in.contains("fields") || in.at("fields").is_null()) {
      status = "unknown";
      reason = "owned_class_or_field_graph_not_supplied";
    } else if (!in.contains("events") || in.at("events").is_null()) {
      status = "unknown";
      reason = "explicit_event_order_not_supplied";
    } else
      for (std::size_t i = 0; i < in.at("events").size(); ++i) {
        Event(i, in.at("events")[i]);
        if (terminal)
          break;
      }
    return J::object(
        {{"format", "Monstey_GadbCachePropagationTraceResult"},
         {"version", 1},
         {"status", status},
         {"stop_reason", reason.empty() ? J() : J(reason)},
         {"scope",
          "conditional_explicit_owned_input_success_fallback_helpers_only"},
         {"native_code_evidence_sha256",
          "9842437cc38c953ed343e02cca5f639841d2bc1f249d9c7d6ffe0394dabd1410"},
         {"events_supplied", in.contains("events") && in.at("events").is_array()
                                 ? J(in.at("events").size())
                                 : J()},
         {"events_started", events.size()},
         {"events_completed", completed},
         {"events", events},
         {"steps", steps},
         {"state_writes", writes},
         {"final_fields", fields},
         {"step_limit", limit},
         {"input_modified", false},
         {"engine_executed", false},
         {"actual_binding_history_known", false},
         {"checker_statuses_changed", false},
         {"constructor_or_compaction_simulated", false},
         {"caller_provenance", in.value("provenance", J())}});
  }
};
} // namespace
Monstey_Json Monstey_GadbClassCacheTrace(const Monstey_Json &input,
                                         std::size_t stepLimit) {
  Monstey_Require(stepLimit <= 1000000, "Trace step limit exceeds1000000");
  Monstey_TraceValidate(input);
  return Monstey_TraceRun(input, stepLimit).Run();
}
} // namespace Monstey
