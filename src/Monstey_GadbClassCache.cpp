#include "Monstey_GadbClassCache.hpp"
#include <algorithm>
#include <charconv>
#include <climits>
#include <limits>
#include <map>
#include <optional>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
bool Monstey_CacheHex(std::string_view text, std::size_t size) {
  return text.size() == size &&
         std::all_of(text.begin(), text.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F');
         });
}
std::uint64_t Monstey_CacheUnsigned(const J &value, std::uint64_t maximum) {
  std::uint64_t result = 0;
  if (value.is_integer())
    result = value.as_uint();
  else {
    const auto &text = value.as_string();
    Monstey_Require(text.starts_with("0x") && text.size() > 2 &&
                        text.size() <= 18,
                    "Expected bounded native hex integer");
    const auto parsed =
        std::from_chars(text.data() + 2, text.data() + text.size(), result, 16);
    Monstey_Require(parsed.ec == std::errc{} &&
                        parsed.ptr == text.data() + text.size(),
                    "Invalid native hex integer");
  }
  Monstey_Require(result <= maximum, "Native integer exceeds its field width");
  return result;
}
std::uint32_t Monstey_CacheHash(const J &value) {
  return static_cast<std::uint32_t>(Monstey_CacheUnsigned(value, UINT32_MAX));
}
unsigned Monstey_CacheCallback(const J &field) {
  Monstey_Require(field.at("native_ordinal_source").as_string() ==
                      "native_index_callback",
                  "Immutable callback identity is not qualified");
  Monstey_Require(Monstey_CacheUnsigned(field.at("native_index_callback_ea"),
                                        UINT64_MAX) != 0,
                  "Missing native callback address provenance");
  const auto &text = field.at("native_index_callback_hex").as_string();
  Monstey_Require(text.size() >= 6 && text.size() <= 128 &&
                      text.size() % 2 == 0 &&
                      Monstey_CacheHex(text, text.size()),
                  "Invalid callback byte evidence");
  Monstey_Bytes bytes;
  for (std::size_t i = 0; i < text.size(); i += 2) {
    unsigned byte = 0;
    const auto parsed =
        std::from_chars(text.data() + i, text.data() + i + 2, byte, 16);
    Monstey_Require(parsed.ec == std::errc{}, "Invalid callback byte");
    bytes.push_back(static_cast<std::uint8_t>(byte));
  }
  unsigned value = UINT_MAX;
  if (bytes.size() >= 3 && bytes[0] == 0xb0 && bytes[2] == 0xc3)
    value = bytes[1];
  else if (bytes.size() >= 3 &&
           (bytes[0] == 0x32 || bytes[0] == 0x30 || bytes[0] == 0x33) &&
           bytes[1] == 0xc0 && bytes[2] == 0xc3)
    value = 0;
  else if (bytes.size() >= 6 && bytes[0] == 0xb8 && bytes[5] == 0xc3)
    value = Monstey_U32(bytes, 1);
  Monstey_Require(value <= 255 && field.at("native_ordinal").as_uint() == value,
                  "Unsupported or inconsistent native callback identity");
  return value;
}
struct Monstey_CacheField {
  std::string name;
  std::uint32_t hash = 0, type = 0, flags = 0;
  std::int64_t count = 0;
  unsigned identity = 0, index = 0;
};
struct Monstey_CacheClass {
  bool qualified = false;
  std::uint64_t flags = 0;
  std::string parent;
  std::vector<Monstey_CacheField> fields;
  J reasons = J::array(), entry;
};
Monstey_CacheClass Monstey_CacheDeclaration(const J &reflection,
                                            std::string_view name) {
  Monstey_CacheClass result;
  try {
    Monstey_Require(
        Monstey_CacheHex(reflection.at("source_sha256").as_string(), 64),
        "Missing native image SHA256 provenance");
    const auto &category = reflection.at("categories").at(name);
    Monstey_Require(category.at("name").as_string() == name,
                    "Native category name differs from declaration key");
    result.parent = category.value("parent", "").as_string();
    result.flags = Monstey_CacheUnsigned(category.at("flags"), UINT64_MAX);
    const auto count = category.at("field_count").as_uint();
    Monstey_Require(count <= 256 && category.at("fields").is_array() &&
                        count == category.at("fields").size() &&
                        category.at("unreadable_fields").as_uint() == 0,
                    "Complete unflattened reflection table is required");
    unsigned entries = 0;
    for (const auto &entry : reflection.at("entries").as_array())
      if (entry.at("name").as_string() == name) {
        ++entries;
        result.entry = entry;
      }
    Monstey_Require(entries == 1,
                    "Exactly one native category entry is required");
    Monstey_Require(
        Monstey_CacheUnsigned(result.entry.at("entry_ea"), UINT64_MAX) != 0 &&
            Monstey_CacheHex(result.entry.at("entry_sha256").as_string(), 64) &&
            result.entry.at("count").as_uint() == count &&
            Monstey_CacheUnsigned(result.entry.at("flags"), UINT64_MAX) ==
                result.flags,
        "Native entry provenance disagrees with declaration");
    Monstey_Require(count == 0 ||
                        Monstey_CacheUnsigned(result.entry.at("fields_ea"),
                                              UINT64_MAX) != 0,
                    "Nonempty native field table requires provenance");
    // Parent cache sharing is native entry evidence, not an optional display
    // label.
    const auto parentAddress =
        Monstey_CacheUnsigned(result.entry.at("parent_ea"), UINT64_MAX);
    if (parentAddress == 0)
      Monstey_Require(result.parent.empty(),
                      "Symbolic parent contradicts null native parent");
    else {
      const auto symbolicParent = result.parent;
      std::string nativeParent;
      unsigned matches = 0;
      for (const auto &entry : reflection.at("entries").as_array())
        if (Monstey_CacheUnsigned(entry.at("entry_ea"), UINT64_MAX) ==
            parentAddress) {
          nativeParent = entry.at("name").as_string();
          ++matches;
        }
      Monstey_Require(matches == 1 && !nativeParent.empty(),
                      "Native parent association is missing or ambiguous");
      // Retain the actual association even when the symbolic label is absent,
      // so the parent's own shared-state boundary cannot disappear.
      result.parent = nativeParent;
      Monstey_Require(
          symbolicParent == nativeParent,
          "Symbolic parent disagrees with native parent association");
    }
    unsigned physical = 0;
    std::set<std::uint32_t> hashes;
    for (const auto &field : category.at("fields").as_array()) {
      const auto flags = static_cast<std::uint32_t>(
          Monstey_CacheUnsigned(field.at("flags"), UINT32_MAX));
      if (flags & 0x280u)
        continue;
      Monstey_CacheField out;
      out.name = field.at("name").as_string();
      out.hash = Monstey_CacheHash(field.at("hash"));
      Monstey_Require(!out.name.empty() &&
                          Monstey_NativeHash(out.name) == out.hash &&
                          hashes.insert(out.hash).second,
                      "Duplicate or invalid consumed native field hash");
      Monstey_Require(
          Monstey_CacheUnsigned(field.at("native_descriptor_ea"), UINT64_MAX) !=
                  0 &&
              Monstey_CacheHex(field.at("native_descriptor_sha256").as_string(),
                               64),
          "Missing native field descriptor provenance");
      out.type = static_cast<std::uint32_t>(
          Monstey_CacheUnsigned(field.at("type_code"), 255));
      Monstey_Require(out.type > 0, "Native type zero is not qualified");
      out.count = field.at("array").as_int();
      Monstey_Require(out.count >= -1 && out.count <= 65535,
                      "Native field count is outside descriptor grammar");
      out.flags = flags;
      out.identity = Monstey_CacheCallback(field);
      out.index = physical++;
      result.fields.push_back(std::move(out));
    }
    result.qualified = true;
  } catch (const std::exception &e) {
    result.reasons.as_array().push_back(e.what());
  }
  return result;
}
J Monstey_CachePredicates(const Monstey_CacheField &field, const J &record,
                          bool categoryFlag4) {
  const auto &fields = record.at("fields");
  const bool present = field.index < fields.size();
  bool hash = false, identity = false, type = false, count = false;
  if (present) {
    const auto &d = fields[field.index];
    hash = Monstey_CacheHash(d.at("hash")) == field.hash;
    identity = d.at("native_ordinal").as_uint() == field.identity;
    const auto stored = d.at("native_type").as_uint() & 0x3fu;
    type = stored == field.type || (field.type == 11 && stored == 10);
    count = field.count == -1 ||
            d.at("count").as_uint() == static_cast<std::uint64_t>(field.count);
  }
  return J::object({{"present", present},
                    {"hash_match", hash},
                    {"identity_match", identity},
                    {"type_match", type},
                    {"count_match", count},
                    {"category_flag4", categoryFlag4},
                    {"pass", present && hash && identity && type && count &&
                                 !categoryFlag4}});
}
// Copy descriptor metadata only; no value, atom or runtime pointer is read.
J Monstey_CacheDiagnosticDescriptor(const J &record, unsigned index) {
  const auto &fields = record.at("fields");
  if (index >= fields.size())
    return nullptr;
  const auto &descriptor = fields[index];
  return J::object(
      {{"hash", descriptor.at("hash")},
       {"native_ordinal", descriptor.at("native_ordinal")},
       {"native_type", descriptor.at("native_type")},
       {"effective_type", descriptor.at("native_type").as_uint() & 0x3fu},
       {"count", descriptor.at("count")}});
}
J Monstey_CacheDiagnosticMismatches(const J &predicates) {
  auto reasons = J::array();
  if (!predicates.at("present").as_bool()) {
    reasons.as_array().push_back("missing_descriptor");
    return reasons;
  }
  for (const auto &[predicate, reason] :
       {std::pair{"hash_match", "field_hash"},
        std::pair{"identity_match", "native_ordinal"},
        std::pair{"type_match", "effective_type"},
        std::pair{"count_match", "count"}})
    if (!predicates.at(predicate).as_bool())
      reasons.as_array().push_back(reason);
  return reasons;
}
bool Monstey_CacheValidateRecords(const J &records) {
  Monstey_Require(records.is_array() && records.size() <= 0x100000,
                  "Invalid native record list");
  bool named = false, rawNamesQualified = true;
  std::optional<std::uint32_t> previous;
  for (const auto &record : records.as_array()) {
    const bool current = record.at("named").as_bool();
    Monstey_Require(!named || current,
                    "Native anonymous prefix is not preserved");
    named |= current;
    if (current) {
      if (!record.contains("name_hash") || record["name_hash"].is_null())
        rawNamesQualified = false;
      else {
        const auto hash = Monstey_CacheHash(record["name_hash"]);
        Monstey_Require(!previous || *previous < hash,
                        "Input is not final strict native named hash order");
        previous = hash;
      }
    }
    Monstey_Require(record.at("reference").is_string(),
                    "Record diagnostic reference is missing");
    const auto &fields = record.at("fields");
    Monstey_Require(fields.is_array() && fields.size() <= 255,
                    "Record descriptors exceed native UInt8 count");
    for (const auto &field : fields.as_array()) {
      (void)Monstey_CacheHash(field.at("hash"));
      Monstey_Require(field.at("native_ordinal").as_uint() <= 255 &&
                          field.at("native_type").as_uint() <= 255 &&
                          field.at("count").as_uint() <= 65535,
                      "Descriptor metadata exceeds native widths");
    }
  }
  return rawNamesQualified;
}
} // namespace
struct Monstey_GadbClassCache::Monstey_Impl {
  std::map<std::string, Monstey_CacheClass> classes;
  std::set<std::string> parents;
  J sourceSha;
};
Monstey_GadbClassCache::Monstey_GadbClassCache(const J &reflection) {
  Monstey_Require(reflection.is_object(),
                  "Native reflection must be an object");
  auto data = std::make_unique<Monstey_Impl>();
  data->sourceSha = reflection.value("source_sha256", J(nullptr));
  if (reflection.contains("categories") && reflection["categories"].is_object())
    for (const auto &[name, unused] : reflection["categories"].as_object()) {
      (void)unused;
      auto declaration = Monstey_CacheDeclaration(reflection, name);
      if (!declaration.parent.empty())
        data->parents.insert(declaration.parent);
      data->classes.emplace(name, std::move(declaration));
    }
  impl_ = std::move(data);
}
Monstey_GadbClassCache::~Monstey_GadbClassCache() = default;
Monstey_Json Monstey_GadbClassCache::Check(const J &layout,
                                           std::size_t diagnosticLimit) const {
  Monstey_Require(
      layout.at("format").as_string() == "Monstey_GadbClassCacheLayout" &&
          layout.at("version").as_uint() == 1 &&
          layout.at("categories").is_array() &&
          layout.at("categories").size() <= 4096 && diagnosticLimit <= 100000,
      "Invalid class-cache input contract");
  const auto &classes = impl_->classes;
  const auto &parents = impl_->parents;
  std::map<std::string, unsigned> bindings;
  for (const auto &category : layout["categories"].as_array())
    ++bindings[category.at("name").as_string()];
  if (layout.contains("binding_name_counts"))
    for (const auto &[name, count] :
         layout["binding_name_counts"].as_object()) {
      Monstey_Require(count.as_uint() >= bindings[name] &&
                          count.as_uint() <= 4096,
                      "Invalid complete binding-name context");
      bindings[name] = static_cast<unsigned>(count.as_uint());
    }
  auto rows = J::array(), diagnostics = J::array();
  std::uint64_t unknown = 0, unsafe = 0, qualified = 0, recordsChecked = 0,
                descriptorRows = 0, predicatesChecked = 0, hazards = 0,
                diagnosticCount = 0, unqualifiedContracts = 0;
  for (const auto &category : layout["categories"].as_array()) {
    const auto name = category.at("name").as_string();
    const auto &records = category.at("records");
    const bool rawNamesQualified = Monstey_CacheValidateRecords(records);
    recordsChecked += records.size();
    for (const auto &record : records.as_array())
      descriptorRows += record["fields"].size();
    const auto found = classes.find(name);
    auto row = J::object(
        {{"name", name},
         {"schema", category.value("schema", name)},
         {"records", records.size()},
         {"binding_string_source", "native_category_header_second_string"},
         {"status", "unknown"},
         {"declaration_qualified",
          found != classes.end() && found->second.qualified},
         {"hazards", 0},
         {"fields", J::array()},
         {"reasons", J::array()}});
    if (!rawNamesQualified) {
      row["reasons"].as_array().push_back(
          "Missing native raw-name hash provenance; display names are not "
          "hashed");
      ++unknown;
      rows.as_array().push_back(std::move(row));
      continue;
    }
    if (found == classes.end() || !found->second.qualified) {
      row["reasons"] = found == classes.end()
                           ? J::array({"No qualified native declaration"})
                           : found->second.reasons;
      ++unknown;
      rows.as_array().push_back(std::move(row));
      continue;
    }
    const auto &declaration = found->second;
    row["native_entry"] = declaration.entry;
    const bool shared = !declaration.parent.empty() || parents.contains(name) ||
                        bindings.at(name) > 1;
    row["shared_ancestor_cache_state_qualified"] = !shared;
    if (shared)
      row["reasons"].as_array().push_back(
          "Parent/descendant or duplicate-name shared cache state and "
          "propagation order are not simulated");
    if (records.empty()) {
      row["status"] = shared ? "unknown" : "qualified";
      row["first_record"] = nullptr;
      row["not_applicable_empty_category"] = true;
      if (shared)
        ++unknown;
      else
        ++qualified;
      rows.as_array().push_back(std::move(row));
      continue;
    }
    const auto named = std::find_if(
        records.as_array().begin(), records.as_array().end(),
        [](const auto &record) { return record["named"].as_bool(); });
    const auto first =
        named != records.as_array().end() ? named : records.as_array().begin();
    row["first_record"] = (*first)["reference"];
    row["first_selection"] =
        named != records.as_array().end() ? "first_named" : "first_anonymous";
    const bool flag4 = (declaration.flags & 4u) != 0;
    std::uint64_t localHazards = 0, localUnknownContracts = 0;
    for (const auto &field : declaration.fields) {
      const auto initial = Monstey_CachePredicates(field, *first, flag4);
      ++predicatesChecked;
      auto fieldRow = J::object(
          {{"field", field.name},
           {"native_ordinal", field.identity},
           {"physical_index", field.index},
           {"first_record_predicates", initial},
           {"direct_initial_mode",
            initial["pass"].as_bool() ? "cached_index" : "hash_fallback"},
           {"conflicting_records", 0}});
      std::uint64_t failures = 0, unsafeSlots = 0, unknownContracts = 0;
      for (const auto &record : records.as_array()) {
        const auto next = Monstey_CachePredicates(field, record, flag4);
        ++predicatesChecked;
        if (!initial["pass"].as_bool() || next["pass"].as_bool())
          continue;
        ++failures;
        const bool slotMismatch = !next["present"].as_bool() ||
                                  !next["hash_match"].as_bool() ||
                                  !next["type_match"].as_bool();
        if (slotMismatch) {
          ++unsafeSlots;
          ++localHazards;
          ++hazards;
        } else {
          // The cached getter reads the record's own count/offset. Repeating
          // the binder's first-record count or identity predicate is not proof
          // of a bad later access.
          ++unknownContracts;
          ++localUnknownContracts;
          ++unqualifiedContracts;
        }
        ++diagnosticCount;
        if (diagnostics.size() < diagnosticLimit)
          diagnostics.as_array().push_back(J::object(
              {{"category", name},
               {"first_record", (*first)["reference"]},
               {"record", record["reference"]},
               {"field", field.name},
               {"physical_index", field.index},
               {"native_ordinal", field.identity},
               {"first", initial},
               {"actual", next},
               {"declaration",
                J::object({{"hash", Monstey_Hex(field.hash)},
                           {"native_ordinal", field.identity},
                           {"native_type", field.type},
                           {"count", field.count},
                           {"count_wildcard", field.count == -1}})},
               {"first_descriptor",
                Monstey_CacheDiagnosticDescriptor(*first, field.index)},
               {"actual_descriptor",
                Monstey_CacheDiagnosticDescriptor(record, field.index)},
               {"mismatches", Monstey_CacheDiagnosticMismatches(next)},
               {"classification",
                slotMismatch ? "unsafe_slot" : "unknown_value_contract"},
               {"native_descriptor_index_bounds_checked", false},
               {"native_element_index_bounds_checked", true},
               {"actual_field_hash", next["present"].as_bool()
                                         ? record["fields"][field.index]["hash"]
                                         : J(nullptr)}}));
      }
      fieldRow["conflicting_records"] = failures;
      fieldRow["unsafe_slot_records"] = unsafeSlots;
      fieldRow["unqualified_value_records"] = unknownContracts;
      row["fields"].as_array().push_back(std::move(fieldRow));
    }
    row["hazards"] = localHazards;
    row["unqualified_value_contracts"] = localUnknownContracts;
    row["status"] = localHazards                        ? "unsafe"
                    : (shared || localUnknownContracts) ? "unknown"
                                                        : "qualified";
    if (localHazards)
      ++unsafe;
    else if (shared || localUnknownContracts)
      ++unknown;
    else
      ++qualified;
    rows.as_array().push_back(std::move(row));
  }
  return J::object(
      {{"format", "Monstey_GadbClassCacheCheck"},
       {"version", 1},
       {"status", unsafe    ? "unsafe"
                  : unknown ? "unknown"
                            : "qualified"},
       {"scope",
        "proved_direct_first_record_binder_and_cached_descriptor_layout"},
       {"assumes_unbound_field_cache", true},
       {"shared_ancestor_state_simulated", false},
       {"engine_executed", false},
       {"gameplay_qualified", false},
       {"reflection_source_sha256", impl_->sourceSha},
       {"layout_source_sha256", layout.value("source_sha256", J(nullptr))},
       {"category_count", rows.size()},
       {"qualified_categories", qualified},
       {"unknown_categories", unknown},
       {"unsafe_categories", unsafe},
       {"records_checked", recordsChecked},
       {"descriptor_rows_checked", descriptorRows},
       {"field_predicates_checked", predicatesChecked},
       {"cached_slot_hazards", hazards},
       {"unqualified_value_contracts", unqualifiedContracts},
       {"later_predicate_conflicts", diagnosticCount},
       {"diagnostics_total", diagnosticCount},
       {"diagnostics_truncated", diagnosticCount > diagnostics.size()},
       {"diagnostics", diagnostics},
       {"categories", rows}});
}
Monstey_Json Monstey_GadbClassCacheCheck(const J &reflection, const J &layout,
                                         std::size_t diagnosticLimit) {
  return Monstey_GadbClassCache(reflection).Check(layout, diagnosticLimit);
}
} // namespace Monstey
