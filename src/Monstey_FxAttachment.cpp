#include "Monstey_FxAttachment.hpp"
#include <algorithm>
#include <array>
#include <set>

namespace Monstey {
namespace {
using J = Monstey_Json;
struct Monstey_AttachmentMode {
  const char *token;
  unsigned flags;
  const char *position;
  const char *orientation;
};
constexpr std::array<Monstey_AttachmentMode, 8> Monstey_AttachmentModes{
    {{"Fixed", 0x000, "captured_frame_plus_rotated_offset",
      "captured_frame_times_rotation"},
     {"Follow", 0x008, "frame_plus_rotated_offset", "frame_times_rotation"},
     {"NodeAttach", 0x028, "named_frame_plus_rotated_offset",
      "frame_times_rotation"},
     {"SocketAttach", 0x028, "named_frame_plus_rotated_offset",
      "frame_times_rotation"},
     {"FollowPos", 0x009, "frame_plus_rotated_offset",
      "initial_context_times_rotation"},
     {"NodeAttachPos", 0x029, "named_frame_plus_rotated_offset",
      "initial_context_times_rotation"},
     {"FollowWSOffset", 0x108, "frame_plus_world_offset",
      "authored_rotation_or_identity"},
     {"NodeAttachWSOffset", 0x128, "named_frame_plus_world_offset",
      "authored_rotation_or_identity"}}};
void Monstey_ARequire(bool condition, std::string_view message) {
  if (!condition)
    throw Monstey_Error("fx_attachment", std::string(message));
}
std::string_view Monstey_ACString(std::string_view text) {
  return text.substr(0, text.find('\0'));
}
J Monstey_AModes() {
  auto modes = J::array();
  for (std::size_t i = 0; i < Monstey_AttachmentModes.size(); ++i)
    modes.as_array().push_back(
        J::object({{"token", Monstey_AttachmentModes[i].token},
                   {"index", static_cast<unsigned>(i)},
                   {"native_follow_flags", Monstey_AttachmentModes[i].flags}}));
  return modes;
}
J Monstey_ASource(Monstey_Gadb &db, const J &args) {
  const auto description = db.Describe();
  Monstey_ARequire(description["format"].as_string() == "LTFX8/GADB17",
                   "Owner attachment requires an LTFX8 database");
  const auto &source = description["source"].as_string();
  if (!source.empty() &&
      Monstey_HashSha256(std::filesystem::path(Monstey_Wide(source))) !=
          description["source_sha256"].as_string())
    throw Monstey_Error("source_hash_mismatch",
                        "Native effect source bytes changed");
  for (const auto &[argument, field] :
       std::array<std::pair<const char *, const char *>, 2>{
           {{"expected_database_sha256", "source_sha256"},
            {"expected_schema_sha256", "schema_sha256"}}})
    if (args.contains(argument))
      Monstey_ARequire(args[argument].is_string() &&
                           args[argument] == description[field],
                       "Owner attachment source/schema hash changed");
  return description;
}
J Monstey_ARecord(Monstey_Gadb &db, std::string_view selector) {
  auto record = db.Metadata(selector);
  Monstey_ARequire(!record.value("pending_delete", false).as_bool(),
                   "Owner attachment record was deleted");
  std::string categorySchema;
  const auto categories = db.Categories();
  for (const auto &category : categories["rows"].as_array())
    if (category["index"] == record["category_index"])
      categorySchema = category["schema"].as_string();
  const auto schema = db.Schema();
  std::set<std::string> visited;
  auto parent = categorySchema;
  bool base = false;
  while (!parent.empty()) {
    Monstey_ARequire(visited.insert(parent).second,
                     "Cyclic FX class inheritance");
    if (parent == "BaseFX") {
      base = true;
      break;
    }
    if (!schema.contains("categories") ||
        !schema["categories"].contains(parent))
      break;
    const auto &definition = schema["categories"][parent];
    if (!definition.contains("parent") || definition["parent"].is_null())
      break;
    Monstey_ARequire(definition["parent"].is_string(),
                     "Invalid FX class inheritance");
    parent = definition["parent"].as_string();
  }
  Monstey_ARequire(base,
                   "Owner attachment requires a BaseFX-derived native record");
  record["category_schema"] = categorySchema;
  return record;
}
const J &Monstey_AField(const J &record, std::uint32_t hash, unsigned type) {
  const J *found = nullptr;
  for (const auto &field : record["fields"].as_array())
    if (field["hash"].as_string() == Monstey_Hex(hash)) {
      Monstey_ARequire(found == nullptr,
                       "Owner attachment field identity is ambiguous");
      found = &field;
    }
  Monstey_ARequire(found != nullptr, "Missing native owner attachment field");
  Monstey_ARequire(
      (*found)["native_type"].as_uint() == type &&
          !(*found)["array"].as_bool() && (*found)["count"].as_uint() == 1 &&
          !(*found)["value"].is_array(),
      "Owner attachment field has an invalid native type or scalar shape");
  Monstey_ARequire((*found)["value"].is_null() || (*found)["value"].is_string(),
                   "Native owner attachment field is not text");
  return *found;
}
J Monstey_AInspect(Monstey_Gadb &db, std::string_view selector) {
  const auto record = Monstey_ARecord(db, selector);
  const auto &update = Monstey_AField(record, 0x23e7c5b3u, 5);
  const auto &attach = Monstey_AField(record, 0x1fcd6c38u, 4);
  const auto &raw = update["value"];
  bool matched = false;
  std::size_t modeIndex = 1;
  J tokenHash = nullptr;
  if (raw.is_string()) {
    const auto hash = Monstey_NativeHash(Monstey_ACString(raw.as_string()), 0);
    tokenHash = Monstey_Hex(hash);
    for (std::size_t i = 0; i < Monstey_AttachmentModes.size(); ++i)
      if (hash == Monstey_NativeHash(Monstey_AttachmentModes[i].token, 0)) {
        modeIndex = i;
        matched = true;
        break;
      }
  }
  const auto &mode = Monstey_AttachmentModes[modeIndex];
  const auto &name = attach["value"];
  const auto nativeName =
      name.is_null() ? std::string_view{} : Monstey_ACString(name.as_string());
  const auto hash = Monstey_NativeHash(nativeName, 0);
  const bool named = (mode.flags & 0x20u) != 0;
  auto warnings = J::array();
  if (!matched)
    warnings.as_array().push_back(
        "Unmatched UpdatePos uses the native Follow default");
  if (named)
    warnings.as_array().push_back(
        "Name resolution depends on the external effect context and can fall "
        "back to its frame");
  return J::object(
      {{"record", record["reference"]},
       {"selector", record["selector"]},
       {"class_name", record["category_schema"]},
       {"update_pos", raw},
       {"update_pos_native_hash", tokenHash},
       {"update_pos_native_match", matched},
       {"update_pos_uses_native_default", !matched},
       {"effective_update_pos", mode.token},
       {"native_mode_index", static_cast<unsigned>(modeIndex)},
       {"native_follow_flags", mode.flags},
       {"modes", Monstey_AModes()},
       {"attach_name", name},
       {"attach_name_native_hash", Monstey_Hex(hash)},
       {"attach_name_c_string", std::string(nativeName)},
       {"attach_name_is_null", name.is_null()},
       {"attach_name_is_empty", nativeName.empty()},
       {"attach_name_hash_is_zero", hash == 0},
       {"named_lookup_enabled_by_mode", named},
       {"name_lookup_has_nonzero_token",
        named && !nativeName.empty() && hash != 0},
       {"name_is_dormant", !named && !nativeName.empty()},
       {"empty_or_zero_name_uses_context_frame",
        named && (nativeName.empty() || hash == 0)},
       {"preexisting_context_attachment_can_bypass_name_lookup", true},
       {"external_owner_resolved", false},
       {"in_game_verified", false},
       {"local_key_parent_attachment", false},
       {"native_consumer_qualified", true},
       {"transform",
        J::object({{"position", mode.position},
                   {"orientation", mode.orientation},
                   {"retains_frame", (mode.flags & 0x8u) != 0},
                   {"position_only", (mode.flags & 0x1u) != 0},
                   {"world_space_offset", (mode.flags & 0x100u) != 0}})},
       {"fields",
        J::object(
            {{"update_pos",
              J::object({{"hash", update["hash"]},
                         {"physical_index", update["physical_index"]},
                         {"native_ordinal", update["native_ordinal"]}})},
             {"attach_name",
              J::object({{"hash", attach["hash"]},
                         {"physical_index", attach["physical_index"]},
                         {"native_ordinal", attach["native_ordinal"]}})}})},
       {"warnings", warnings}});
}
} // namespace
Monstey_Json Monstey_FxAttachmentCatalog() {
  const auto selector = J::object({{"type", "string"}, {"minLength", 1}});
  const auto hash =
      J::object({{"type", "string"}, {"minLength", 64}, {"maxLength", 64}});
  auto properties = J::object({{"record", selector},
                               {"expected_database_sha256", hash},
                               {"expected_schema_sha256", hash}});
  auto result = J::array();
  auto add = [&](const char *name, const char *description, bool readOnly,
                 const J &props) {
    result.as_array().push_back(J::object(
        {{"name", name},
         {"description", description},
         {"read_only", readOnly},
         {"inputSchema", J::object({{"type", "object"},
                                    {"properties", props},
                                    {"required", J::array({"record"})},
                                    {"additionalProperties", false}})}}));
  };
  add("fx_attachment_inspect",
      "Inspect native BaseFX owner-frame mode, name hash and fallback "
      "conditions; does not resolve a game owner.",
      true, properties);
  auto tokens = J::array();
  for (const auto &mode : Monstey_AttachmentModes)
    tokens.as_array().push_back(mode.token);
  properties["update_pos"] = J::object({{"type", "string"}, {"enum", tokens}});
  properties["attach_name"] =
      J::object({{"type", J::array({"string", "null"})}});
  add("fx_attachment_set",
      "Set native BaseFX UpdatePos and/or AttachName atomically, preserving "
      "prefab inheritance and dormant names. "
      "Null AttachName is normalized to empty text.",
      false, properties);
  return result;
}
Monstey_Json Monstey_FxAttachmentCall(Monstey_Gadb &db,
                                      std::string_view operation,
                                      const J &args) {
  Monstey_ARequire(args.is_object(),
                   "Owner attachment arguments must be an object");
  const auto catalog = Monstey_FxAttachmentCatalog();
  const J *definition = nullptr;
  for (const auto &entry : catalog.as_array())
    if (entry["name"].as_string() == operation)
      definition = &entry;
  Monstey_ARequire(definition != nullptr, "Unknown owner attachment operation");
  for (const auto &[key, value] : args.as_object()) {
    (void)value;
    Monstey_ARequire((*definition)["inputSchema"]["properties"].contains(key),
                     "Unknown owner attachment argument");
  }
  Monstey_ARequire(args.contains("record") && args["record"].is_string() &&
                       !args["record"].as_string().empty(),
                   "Select a native BaseFX record");
  const auto description = Monstey_ASource(db, args);
  const auto before = Monstey_AInspect(db, args["record"].as_string());
  const auto record = before["record"].as_string();
  if (operation == "fx_attachment_inspect") {
    auto result = before;
    (void)Monstey_ASource(db, args);
    result["source_sha256"] = description["source_sha256"];
    result["schema_sha256"] = description["schema_sha256"];
    return result;
  }
  Monstey_ARequire(args.contains("update_pos") || args.contains("attach_name"),
                   "Select an owner attachment field to change");
  if (args.contains("update_pos")) {
    Monstey_ARequire(args["update_pos"].is_string(),
                     "UpdatePos must be a canonical native enum token");
    Monstey_ARequire(std::any_of(Monstey_AttachmentModes.begin(),
                                 Monstey_AttachmentModes.end(),
                                 [&](const auto &mode) {
                                   return args["update_pos"].as_string() ==
                                          mode.token;
                                 }),
                     "Unknown or noncanonical native UpdatePos token");
  }
  if (args.contains("attach_name"))
    Monstey_ARequire(args["attach_name"].is_null() ||
                         args["attach_name"].is_string(),
                     "AttachName must be text or null");
  const bool normalizeNull =
      args.contains("attach_name") && args["attach_name"].is_null();
  J result;
  auto state = db.Transaction([&](Monstey_Gadb &) {
    (void)Monstey_ASource(db, args);
    if (args.contains("update_pos"))
      Monstey_FxLibraryNativeCall(db, "fx_set",
                                  J::object({{"record", record},
                                             {"field", "0x23e7c5b3"},
                                             {"value", args["update_pos"]}}));
    if (args.contains("attach_name"))
      Monstey_FxLibraryNativeCall(
          db, "fx_set",
          J::object({{"record", record},
                     {"field", "0x1fcd6c38"},
                     {"value", normalizeNull ? J("") : args["attach_name"]}}));
    result = Monstey_AInspect(db, record);
    (void)Monstey_ASource(db, args);
  });
  state["operation"] = std::string(operation);
  result["attach_name_null_normalized"] = normalizeNull;
  result["source_sha256"] = description["source_sha256"];
  result["schema_sha256"] = description["schema_sha256"];
  state["result"] = result;
  return state;
}
} // namespace Monstey
