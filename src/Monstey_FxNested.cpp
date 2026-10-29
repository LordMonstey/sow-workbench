#include "Monstey_FxNested.hpp"
#include <algorithm>
#include <array>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
void Monstey_NRequire(bool value, std::string_view message) {
  if (!value)
    throw Monstey_Error("fx_nested", std::string(message));
}
bool Monstey_NSpace(unsigned char c) {
  return c == ' ' || (c >= '\t' && c <= '\r');
}
const J &Monstey_NField(const J &record, std::string_view name, unsigned type) {
  const auto hash = Monstey_Hex(Monstey_NativeHash(name));
  for (const auto &field : record["fields"].as_array())
    if (field["hash"].as_string() == hash) {
      Monstey_NRequire(field["native_type"].as_uint() == type &&
                           !field["value"].is_array(),
                       "CreateFX property has an invalid native type or shape");
      return field["value"];
    }
  throw Monstey_Error("fx_nested",
                      "Missing native CreateFX property: " + std::string(name));
}
J Monstey_NRecord(Monstey_Gadb &db, std::string_view selector) {
  auto record = db.Metadata(selector);
  Monstey_NRequire(!record.value("pending_delete", false).as_bool(),
                   "CreateFX record was deleted");
  std::string schemaName;
  const auto categories = db.Categories();
  for (const auto &category : categories["rows"].as_array())
    if (category["index"] == record["category_index"])
      schemaName = category["schema"].as_string();
  Monstey_NRequire(
      schemaName == "ClientFX.Create",
      "Nested effect operations require the qualified ClientFX.Create class");
  record["category_schema"] = schemaName;
  return record;
}
J Monstey_NInspect(Monstey_Gadb &db, std::string_view selector) {
  const auto record = Monstey_NRecord(db, selector);
  auto slots = J::array(), warnings = J::array();
  std::size_t selected = 0, resources = 0;
  for (unsigned i = 0; i < 16; ++i) {
    const auto field = "Effect" + std::to_string(i);
    const auto &value = Monstey_NField(record, field, 4);
    Monstey_NRequire(value.is_null() || value.is_string(),
                     "Nested effect property is not native text");
    const auto raw = value.is_null() ? std::string{} : value.as_string();
    const bool eligible = !raw.empty() && raw.front() != '\0';
    if (eligible)
      ++selected;
    auto entry = J::object({{"slot", i},
                            {"field", field},
                            {"raw", value},
                            {"selected_by_native_random_choice", eligible}});
    try {
      const auto names = Monstey_FxEffectNames(raw);
      entry["effects"] = names;
      entry["valid"] = true;
      resources += names.size();
      if (eligible && names.empty())
        warnings.as_array().push_back("Nonempty " + field +
                                      " selects an empty effect group");
    } catch (const Monstey_Error &error) {
      entry["valid"] = false;
      entry["error"] = error.what();
      warnings.as_array().push_back("Invalid " + field + ": " + error.what());
    }
    slots.as_array().push_back(std::move(entry));
  }
  const auto &count = Monstey_NField(record, "NumToCreate", 3);
  Monstey_NRequire(count.is_integer(), "Native NumToCreate is not Int32");
  const auto authored = count.as_int();
  Monstey_NRequire(authored >= INT32_MIN && authored <= INT32_MAX,
                   "Native NumToCreate exceeds Int32");
  const auto nativeCount =
      static_cast<unsigned>(static_cast<std::uint32_t>(authored) & 255u);
  if (authored < 0 || authored > 255)
    warnings.as_array().push_back(
        "Native loader stores only the low eight bits of NumToCreate");
  if (!selected)
    warnings.as_array().push_back(
        "No nonempty nested effect group is selectable");
  for (auto &slot : slots.as_array())
    slot["nominal_selection_share"] =
        slot["selected_by_native_random_choice"].as_bool()
            ? J(1.0 / static_cast<double>(selected))
            : J(0.0);
  const auto &loop = Monstey_NField(record, "Loop", 1);
  Monstey_NRequire(loop.is_bool(), "Native Loop is not Boolean");
  return J::object(
      {{"record", record["reference"]},
       {"selector", record["selector"]},
       {"class_name", record["category_schema"]},
       {"slots", slots},
       {"nonempty_groups", static_cast<std::uint64_t>(selected)},
       {"effect_references", static_cast<std::uint64_t>(resources)},
       {"authored_num_to_create", authored},
       {"native_num_to_create", nativeCount},
       {"loop", loop},
       {"create_context", Monstey_NField(record, "CreateContext", 4)},
       {"selection", "one random nonempty slot per iteration; every effect in "
                     "that slot is requested"},
       {"warnings", warnings},
       {"native_consumer_qualified", true},
       {"in_game_verified", false}});
}
J Monstey_NSource(Monstey_Gadb &db, const J &args) {
  const auto description = db.Describe();
  if (!description["source"].as_string().empty()) {
    const auto path =
        std::filesystem::path(Monstey_Wide(description["source"].as_string()));
    if (Monstey_HashSha256(path) != description["source_sha256"].as_string())
      throw Monstey_Error("source_hash_mismatch",
                          "Native effect source bytes changed");
  }
  for (const auto &[key, field] :
       std::array<std::pair<const char *, const char *>, 2>{
           {{"expected_database_sha256", "source_sha256"},
            {"expected_schema_sha256", "schema_sha256"}}})
    if (args.contains(key))
      Monstey_NRequire(args[key] == description[field],
                       "Nested effect source/schema hash changed");
  return description;
}
J Monstey_NObject(J properties, J required = J::array()) {
  return J::object({{"type", "object"},
                    {"properties", properties},
                    {"required", required},
                    {"additionalProperties", false}});
}
} // namespace
Monstey_Json Monstey_FxEffectNames(std::string_view text) {
  auto result = J::array();
  std::string token;
  auto emit = [&] {
    if (!token.empty()) {
      result.as_array().push_back(token);
      token.clear();
    }
  };
  for (const unsigned char c : text) {
    if (c == 0)
      break;
    if (c == ';') {
      emit();
      continue;
    }
    if (Monstey_NSpace(c))
      continue;
    Monstey_NRequire(token.size() < 259,
                     "Effect name exceeds the native 259-byte token bound");
    token.push_back(static_cast<char>(c));
  }
  emit();
  return result;
}
Monstey_Json Monstey_FxNestedCatalog() {
  const auto text = J::object({{"type", "string"}, {"minLength", 1}});
  const auto hash =
      J::object({{"type", "string"}, {"minLength", 64}, {"maxLength", 64}});
  auto base = J::object({{"record", text},
                         {"expected_database_sha256", hash},
                         {"expected_schema_sha256", hash}});
  auto result = J::array();
  auto add = [&](const char *name, const char *description, const J &properties,
                 const J &required, bool readOnly) {
    result.as_array().push_back(
        J::object({{"name", name},
                   {"description", description},
                   {"read_only", readOnly},
                   {"inputSchema", Monstey_NObject(properties, required)}}));
  };
  add("fx_nested_inspect",
      "Inspect native CreateFX alternatives, grouped dependencies and "
      "effective spawn count.",
      base, J::array({"record"}), true);
  auto slot = base;
  slot["slot"] =
      J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", 15}});
  slot["effects"] =
      J::object({{"type", "array"}, {"maxItems", 65535}, {"items", text}});
  add("fx_nested_set_slot",
      "Set one native Effect0..15 group; an empty list disables the slot.",
      slot, J::array({"record", "slot", "effects"}), false);
  auto options = base;
  options["num_to_create"] =
      J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", 255}});
  options["loop"] = J::object({{"type", "boolean"}});
  add("fx_nested_set_options",
      "Set the native byte-sized creation count and Loop flag without "
      "truncation.",
      options, J::array({"record"}), false);
  return result;
}
Monstey_Json Monstey_FxNestedCall(Monstey_Gadb &db, std::string_view op,
                                  const J &args) {
  Monstey_NRequire(args.is_object(),
                   "Nested effect arguments must be an object");
  Monstey_NRequire(db.Describe()["format"].as_string() == "LTFX8/GADB17",
                   "Nested effects require LTFX8");
  const auto catalog = Monstey_FxNestedCatalog();
  const J *definition = nullptr;
  for (const auto &candidate : catalog.as_array())
    if (candidate["name"].as_string() == op)
      definition = &candidate;
  Monstey_NRequire(definition != nullptr, "Unknown nested effect operation");
  for (const auto &[key, value] : args.as_object()) {
    (void)value;
    Monstey_NRequire((*definition)["inputSchema"]["properties"].contains(key),
                     "Unknown nested effect argument");
  }
  Monstey_NRequire(args.contains("record") && args["record"].is_string(),
                   "Select a native CreateFX record");
  const auto description = Monstey_NSource(db, args);
  const auto record =
      Monstey_NRecord(db, args["record"].as_string())["reference"].as_string();
  if (op == "fx_nested_inspect") {
    auto result = Monstey_NInspect(db, record);
    (void)Monstey_NSource(db, args);
    result["source_sha256"] = description["source_sha256"];
    result["schema_sha256"] = description["schema_sha256"];
    return result;
  }
  auto state = db.Transaction([&](Monstey_Gadb &) {
    if (op == "fx_nested_set_slot") {
      Monstey_NRequire(
          args.contains("slot") && args["slot"].is_integer() &&
              args["slot"].as_uint() < 16 && args.contains("effects") &&
              args["effects"].is_array() && args["effects"].size() <= 65535,
          "Nested effect slot/effect list is invalid");
      std::string encoded;
      for (const auto &item : args["effects"].as_array()) {
        Monstey_NRequire(item.is_string() && !item.as_string().empty(),
                         "An effect name is empty or not text");
        const auto &name = item.as_string();
        Monstey_NRequire(
            name.size() <= 259 && name.find('\0') == std::string::npos &&
                name.find(';') == std::string::npos &&
                std::none_of(name.begin(), name.end(),
                             [](unsigned char c) { return Monstey_NSpace(c); }),
            "Effect name contains a native delimiter/whitespace or exceeds 259 "
            "bytes");
        if (!encoded.empty())
          encoded.push_back(';');
        encoded += name;
      }
      Monstey_NRequire(
          encoded.size() <= 65535,
          "Encoded nested effect list exceeds the editor string limit");
      const auto field = "Effect" + std::to_string(args["slot"].as_uint());
      Monstey_NField(db.Metadata(record), field, 4);
      Monstey_FxLibraryNativeCall(
          db, "fx_set",
          J::object(
              {{"record", record}, {"field", field}, {"value", encoded}}));
    } else {
      Monstey_NRequire(args.contains("num_to_create") || args.contains("loop"),
                       "Select a creation option to change");
      if (args.contains("num_to_create")) {
        Monstey_NRequire(args["num_to_create"].is_integer() &&
                             args["num_to_create"].as_int() >= 0 &&
                             args["num_to_create"].as_int() <= 255,
                         "NumToCreate must fit the native 0..255 byte");
        Monstey_NField(db.Metadata(record), "NumToCreate", 3);
        Monstey_FxLibraryNativeCall(
            db, "fx_set",
            J::object({{"record", record},
                       {"field", "NumToCreate"},
                       {"value", args["num_to_create"]}}));
      }
      if (args.contains("loop")) {
        Monstey_NRequire(args["loop"].is_bool(), "Loop must be Boolean");
        Monstey_NField(db.Metadata(record), "Loop", 1);
        Monstey_FxLibraryNativeCall(db, "fx_set",
                                    J::object({{"record", record},
                                               {"field", "Loop"},
                                               {"value", args["loop"]}}));
      }
    }
    (void)Monstey_NInspect(db, record);
    (void)Monstey_NSource(db, args);
  });
  state["operation"] = std::string(op);
  state["result"] = Monstey_NInspect(db, record);
  return state;
}
} // namespace Monstey
