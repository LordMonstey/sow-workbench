#include "Monstey_PhysicsWorld.hpp"
#include "Monstey_Collision.hpp"
#include "Monstey_TriangleCollision.hpp"
#include "Monstey_World.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
constexpr std::uint32_t Monstey_PhysicsChunk = 0xb9e81cf3;
struct Monstey_PSource {
  std::filesystem::path path;
  std::string hash;
};
bool Monstey_PObjres(std::span<const std::uint8_t> bytes) {
  return bytes.size() >= 4 && bytes[0] == 'O' && bytes[1] == 'B' &&
         bytes[2] == 'J' && bytes[3] == 'R';
}
std::filesystem::path Monstey_PPath(const J &value) {
  const auto &text = value.as_string();
  Monstey_Require(!text.empty() && text.find('\0') == text.npos,
                  "Physics path must be nonempty without NUL");
  return std::filesystem::path(Monstey_Wide(text));
}
std::string Monstey_PName(const J &value) {
  const auto &text = value.as_string();
  Monstey_Require(!text.empty() && text.find('\0') == text.npos &&
                      text.size() < 65535,
                  "Physical owner name is invalid");
  (void)Monstey_Wide(text);
  return text;
}
void Monstey_PKeys(const J &object,
                   std::initializer_list<std::string_view> allowed) {
  Monstey_Require(object.is_object(), "Physics operation must be an object");
  for (const auto &[key, value] : object.as_object()) {
    (void)value;
    Monstey_Require(std::find(allowed.begin(), allowed.end(), key) !=
                        allowed.end(),
                    "Unknown physics operation field: " + key);
  }
}
std::uint32_t Monstey_PHash(const J &value) {
  const auto &text = value.as_string();
  std::uint32_t result{};
  Monstey_Require(text.starts_with("0x") && text.size() == 10,
                  "Expected a native uint32 hash");
  const auto parsed =
      std::from_chars(text.data() + 2, text.data() + text.size(), result, 16);
  Monstey_Require(parsed.ec == std::errc{} &&
                      parsed.ptr == text.data() + text.size(),
                  "Invalid native hash");
  return result;
}
std::vector<std::size_t> Monstey_PRoute(const J &value) {
  std::vector<std::size_t> route;
  for (const auto &item : value.as_array()) {
    const auto index = item.as_uint();
    Monstey_Require(index <= std::numeric_limits<std::size_t>::max(),
                    "Shape route exceeds size_t");
    route.push_back(static_cast<std::size_t>(index));
  }
  return route;
}
J Monstey_POwner(const B &objres, const std::string &name,
                 bool properties = false) {
  const auto rows = Monstey_ObjresOwners(objres, properties);
  std::optional<J> selected;
  for (const auto &row : rows.as_array())
    if (row.at("name").as_string() == name) {
      Monstey_Require(!selected.has_value(),
                      "Physical owner name is ambiguous");
      selected = row;
    }
  Monstey_Require(selected.has_value(),
                  "Physical owner was not found: " + name);
  return *selected;
}
J Monstey_POwners(const B &objres, const J &mappings, bool require) {
  const auto rows = Monstey_ObjresOwners(objres, false);
  std::unordered_map<std::uint32_t, std::size_t> counts;
  for (const auto &owner : rows.as_array())
    ++counts[Monstey_PHash(owner.at("name_hash"))];
  std::size_t missing = 0, ambiguous = 0;
  for (const auto &row : mappings.as_array()) {
    const auto count = counts[Monstey_PHash(row.at("owner_hash"))];
    missing += count == 0;
    ambiguous += count > 1;
  }
  if (require)
    Monstey_Require(
        missing == 0 && ambiguous == 0,
        "Physical owner mappings are missing or hash-ambiguous in OBJRES");
  return J::object(
      {{"object_count", static_cast<std::uint64_t>(rows.size())},
       {"missing_mappings", static_cast<std::uint64_t>(missing)},
       {"ambiguous_mappings", static_cast<std::uint64_t>(ambiguous)},
       {"verified", missing == 0 && ambiguous == 0}});
}
B Monstey_PWithout(const B &objres) {
  return Monstey_ObjresReplaceChunk(objres, Monstey_PhysicsChunk, {});
}
J Monstey_PPublish(const std::vector<Monstey_PSource> &inputs, const J &a,
                   const B &bytes, J report) {
  const auto output = Monstey_PPath(a.at("output"));
  std::vector<std::filesystem::path> protectedPaths;
  for (const auto &source : inputs)
    protectedPaths.push_back(source.path);
  Monstey_RequireOfflineOutput(output, protectedPaths);
  const auto hash = Monstey_HashSha256(bytes);
  std::optional<std::filesystem::path> reportPath;
  if (a.contains("report")) {
    reportPath = Monstey_PPath(a.at("report"));
    auto all = protectedPaths;
    all.push_back(output);
    Monstey_RequireOfflineOutput(*reportPath, all);
    Monstey_Require(!std::filesystem::exists(*reportPath),
                    "Physics report already exists");
  }
  const auto unchanged = [&] {
    for (const auto &source : inputs)
      Monstey_Require(Monstey_HashSha256(source.path) == source.hash,
                      "Physics input changed during the edit");
  };
  unchanged();
  bool made = false, madeReport = false;
  std::string reportText, reportHash;
  report["output"] = Monstey_Utf8(output.wstring());
  report["bytes"] = static_cast<std::uint64_t>(bytes.size());
  report["sha256"] = hash;
  report["source_sha256"] = inputs.front().hash;
  report["game_written"] = false;
  report["runtime_verified"] = false;
  try {
    Monstey_WriteNewFile(output, bytes);
    made = true;
    Monstey_Require(Monstey_HashSha256(output) == hash,
                    "Physics output readback differs");
    unchanged();
    if (reportPath) {
      reportText = report.dump(2) + "\n";
      reportHash = Monstey_HashSha256(
          std::span(reinterpret_cast<const std::uint8_t *>(reportText.data()),
                    reportText.size()));
      Monstey_WriteNewText(*reportPath, reportText);
      madeReport = true;
    }
    unchanged();
  } catch (...) {
    if (madeReport) {
      std::error_code ec;
      try {
        if (Monstey_HashSha256(*reportPath) == reportHash)
          std::filesystem::remove(*reportPath, ec);
      } catch (...) {
      }
    }
    if (made) {
      try {
        if (Monstey_HashSha256(output) == hash) {
          std::error_code ec;
          std::filesystem::remove(output, ec);
        }
      } catch (...) {
      }
    }
    throw;
  }
  return report;
}
} // namespace
std::pair<B, J>
Monstey_PhysicsWorldApply(std::span<const std::uint8_t> input, const J &spec,
                          const Monstey_PhysicsDonorReader &donorReader,
                          std::size_t offset, std::size_t limit) {
  const B original(input.begin(), input.end());
  const auto originalHash = Monstey_HashSha256(original);
  const bool wrapped = Monstey_PObjres(original);
  B context = wrapped ? original : B{};
  Monstey_Physics document(
      wrapped ? Monstey_ObjresReadChunk(original, Monstey_PhysicsChunk)
              : original);
  Monstey_PKeys(spec, {"source_sha256", "rename_owners", "clone_owners",
                       "import_owners", "transforms", "boxes"});
  if (spec.contains("source_sha256"))
    Monstey_Require(spec.at("source_sha256").as_string() == originalHash,
                    "Physics edit spec source SHA256 mismatch");

  auto ownerEdits = J::array();
  if (wrapped)
    (void)Monstey_POwners(context, document.Describe().at("mapping_rows"),
                          true);
  const auto operations = [&](const char *key) -> J {
    const auto value = spec.value(key, J::array());
    Monstey_Require(value.is_array(), std::string(key) + " must be an array");
    return value;
  };
  const auto rename_owners = operations("rename_owners");
  for (const auto &entry : rename_owners.as_array()) {
    Monstey_Require(wrapped, "Physical owner rename requires a whole OBJRES");
    Monstey_PKeys(entry, {"old_name", "new_name"});
    const auto before = Monstey_PName(entry.at("old_name")),
               after = Monstey_PName(entry.at("new_name"));
    (void)Monstey_POwner(context, before);
    document.RenameMapping(Monstey_NativeHash(before),
                           Monstey_NativeHash(after));
    auto changed = Monstey_ObjresApply(
        Monstey_PWithout(context),
        J::object(
            {{"updates", J::array({J::object(
                             {{"name", before}, {"rename_to", after}})})}}));
    context = Monstey_ObjresReplaceChunk(changed.first, Monstey_PhysicsChunk,
                                         document.Encode());
    ownerEdits.as_array().push_back(changed.second);
  }
  const auto clone_owners = operations("clone_owners");
  for (const auto &entry : clone_owners.as_array()) {
    Monstey_Require(wrapped, "Physical owner clone requires a whole OBJRES");
    Monstey_PKeys(entry, {"name", "new_name", "delta_cm"});
    const auto name = Monstey_PName(entry.at("name")),
               newName = Monstey_PName(entry.at("new_name"));
    const auto owner = Monstey_POwner(context, name);
    const auto &delta = entry.at("delta_cm");
    Monstey_Require(delta.is_array() && delta.size() == 3,
                    "Owner clone delta must have three values");
    auto position = J::array();
    for (std::size_t k = 0; k < 3; ++k) {
      const double value =
          owner.at("position")[k].as_number() + delta[k].as_number();
      Monstey_Require(std::isfinite(value),
                      "Owner clone position is not finite");
      position.as_array().push_back(value);
    }
    document.CloneMapping(Monstey_NativeHash(name),
                          Monstey_NativeHash(newName));
    auto changed = Monstey_ObjresApply(
        Monstey_PWithout(context),
        J::object(
            {{"clones", J::array({J::object({{"source_name", name},
                                             {"name", newName},
                                             {"position", position}})})}}));
    context = Monstey_ObjresReplaceChunk(changed.first, Monstey_PhysicsChunk,
                                         document.Encode());
    ownerEdits.as_array().push_back(changed.second);
  }
  const auto import_owners = operations("import_owners");
  for (const auto &entry : import_owners.as_array()) {
    Monstey_Require(wrapped, "Physical owner import requires a whole OBJRES");
    Monstey_PKeys(entry, {"donor", "donor_name", "new_name", "position_cm",
                          "quaternion_xyzw", "properties", "group_index"});
    const auto path = Monstey_PPath(entry.at("donor"));
    const auto raw = donorReader ? donorReader(path) : Monstey_ReadFile(path);
    Monstey_Require(Monstey_PObjres(raw),
                    "Physical donor must be a whole OBJRES");

    Monstey_Physics donor(Monstey_ObjresReadChunk(raw, Monstey_PhysicsChunk));
    const auto donorName = Monstey_PName(entry.at("donor_name")),
               newName = Monstey_PName(entry.at("new_name"));
    (void)Monstey_POwner(raw, donorName);
    // Validate normalized float32 placement before changing either graph.

    const auto &quaternion = entry.at("quaternion_xyzw");
    Monstey_Require(quaternion.is_array() && quaternion.size() == 4,
                    "Imported quaternion width");
    double norm = 0;
    for (const auto &value : quaternion.as_array()) {
      const auto f = value.as_number();
      Monstey_Require(std::isfinite(f), "Imported quaternion must be finite");
      norm += f * f;
    }
    Monstey_Require(std::abs(norm - 1) < 0.002,
                    "Imported quaternion must be normalized");
    const auto group = entry.value("group_index", 0).as_uint();
    Monstey_Require(group <= 0xffffffffull, "Target group exceeds uint32");
    document.ImportMapping(donor, Monstey_NativeHash(donorName),
                           Monstey_NativeHash(newName));
    auto changed = Monstey_ObjresImportOwner(
        Monstey_PWithout(context), Monstey_PWithout(raw), donorName, newName,
        entry.at("position_cm"), quaternion, J::object(),
        static_cast<std::uint32_t>(group));
    if (entry.contains("properties") && !entry.at("properties").is_null()) {
      const auto &properties = entry.at("properties");
      Monstey_Require(properties.is_object(),
                      "Imported properties must be an object or null");
      const auto imported = Monstey_POwner(changed.first, newName, true);
      auto remove = J::array();
      std::unordered_set<std::uint32_t> explicitFields;
      for (const auto &[key, value] : properties.as_object()) {
        (void)value;
        explicitFields.insert(key.starts_with("0x") ? Monstey_PHash(key)
                                                    : Monstey_NativeHash(key));
      }
      for (const auto &[key, value] :
           imported.at("property_values").as_object()) {
        (void)value;
        if (!explicitFields.contains(Monstey_PHash(key)))
          remove.as_array().push_back(key);
      }
      auto replace = Monstey_ObjresApply(
          changed.first,
          J::object({{"updates", J::array({J::object(
                                     {{"name", newName},
                                      {"properties", properties},
                                      {"remove_properties", remove}})})}}));
      changed.first = std::move(replace.first);
    }
    context = Monstey_ObjresReplaceChunk(changed.first, Monstey_PhysicsChunk,
                                         document.Encode());
    ownerEdits.as_array().push_back(changed.second);
  }
  const auto transforms = operations("transforms");
  for (const auto &entry : transforms.as_array()) {
    Monstey_PKeys(entry, {"index", "position_cm", "quaternion_xyzw", "route"});
    std::optional<std::vector<std::size_t>> route;
    if (entry.contains("route") && !entry.at("route").is_null())
      route = Monstey_PRoute(entry.at("route"));
    document.SetTransform(static_cast<std::size_t>(entry.at("index").as_uint()),
                          entry.at("position_cm"), entry.at("quaternion_xyzw"),
                          route);
  }
  const auto boxes = operations("boxes");
  for (const auto &entry : boxes.as_array()) {
    Monstey_PKeys(entry, {"index", "half_extents_cm", "route"});
    document.ResizeBox(static_cast<std::size_t>(entry.at("index").as_uint()),
                       entry.at("half_extents_cm"),
                       Monstey_PRoute(entry.value("route", J::array())));
  }
  auto encoded = wrapped ? Monstey_ObjresReplaceChunk(
                               context, Monstey_PhysicsChunk, document.Encode())
                         : document.Encode();
  Monstey_Physics checked(
      wrapped ? Monstey_ObjresReadChunk(encoded, Monstey_PhysicsChunk)
              : encoded);
  auto report = document.Describe(offset, limit);
  if (wrapped)
    report["owners"] =
        Monstey_POwners(encoded, checked.Describe().at("mapping_rows"), true);
  report["owner_changes"] = ownerEdits;
  report["owner_objects_edited"] = !ownerEdits.empty();
  report["source_sha256"] = originalHash;
  return {std::move(encoded), std::move(report)};
}

J Monstey_PhysicsWorldCall(std::string_view operation, const J &a) {
  Monstey_Require(operation == "sow_world_physics_inspect" ||
                      operation == "sow_world_physics_apply" ||
                      operation == "sow_world_collision_boxes_build" ||
                      operation == "sow_world_collision_triangles_build",
                  "Unknown physics operation");
  if (operation == "sow_world_physics_inspect")
    Monstey_PKeys(a, {"source", "offset", "limit", "expected_sha256"});
  else if (operation == "sow_world_physics_apply")
    Monstey_PKeys(a, {"source", "output", "spec", "report", "offset", "limit",
                      "expected_sha256"});
  else if (operation == "sow_world_collision_triangles_build")
    Monstey_PKeys(a,
                  {"source", "profile_source", "spec", "output", "report",
                   "expected_sha256", "profile_donor_source", "profile_donor"});
  else
    Monstey_PKeys(a, {"source", "profile_source", "output", "spec", "report",
                      "expected_sha256"});
  const auto source = Monstey_PPath(a.at("source"));
  const auto original = Monstey_ReadFile(source);
  const auto originalHash = Monstey_HashSha256(original);
  if (a.contains("expected_sha256"))
    Monstey_Require(a.at("expected_sha256").as_string() == originalHash,
                    "Physics source SHA256 mismatch");
  if (operation == "sow_world_collision_boxes_build") {
    const auto profilePath = Monstey_PPath(a.at("profile_source"));
    const auto profile = Monstey_ReadFile(profilePath);
    auto result = Monstey_CollisionBoxes(original, a.at("spec"), profile);
    return Monstey_PPublish(
        {{source, originalHash}, {profilePath, Monstey_HashSha256(profile)}}, a,
        result.bytes, std::move(result.report));
  }
  if (operation == "sow_world_collision_triangles_build") {
    const auto profilePath = Monstey_PPath(a.at("profile_source"));
    const auto profile = Monstey_ReadFile(profilePath);
    B prepared = profile;
    auto spec = a.at("spec");
    J preparation;
    std::vector<Monstey_PSource> inputs{
        {source, originalHash}, {profilePath, Monstey_HashSha256(profile)}};
    Monstey_Require(
        a.contains("profile_donor_source") == a.contains("profile_donor"),
        "Triangle profile donor path and selector must be supplied together");
    if (a.contains("profile_donor_source")) {
      const auto donorPath = Monstey_PPath(a.at("profile_donor_source"));
      const auto donor = Monstey_ReadFile(donorPath);
      inputs.push_back({donorPath, Monstey_HashSha256(donor)});
      auto result = Monstey_TriangleCompleteProfile(
          profile, spec.at("profile"), donor, a.at("profile_donor"));
      prepared = std::move(result.bytes);
      preparation = std::move(result.report);
      spec["profile"]["source_sha256"] = Monstey_HashSha256(prepared);
    }
    auto result = Monstey_TriangleCollision(original, spec, prepared);
    result.report["profile_preparation"] = preparation;
    result.report["profile_input_sha256"] = Monstey_HashSha256(profile);
    result.report["prepared_profile_sha256"] = Monstey_HashSha256(prepared);
    return Monstey_PPublish(inputs, a, result.bytes, std::move(result.report));
  }
  const bool wrapped = Monstey_PObjres(original);
  B context = wrapped ? original : B{};
  Monstey_Physics document(
      wrapped ? Monstey_ObjresReadChunk(original, Monstey_PhysicsChunk)
              : original);
  const auto offset = static_cast<std::size_t>(a.value("offset", 0).as_uint()),
             limit = static_cast<std::size_t>(a.value("limit", 250).as_uint());
  if (operation == "sow_world_physics_inspect") {
    auto report = document.Describe(offset, limit);
    report["source"] = Monstey_Utf8(source.wstring());
    report["source_sha256"] = originalHash;
    if (wrapped)
      report["owners"] =
          Monstey_POwners(context, report.at("mapping_rows"), false);
    return report;
  }
  std::vector<Monstey_PSource> inputs{{source, originalHash}};
  auto result = Monstey_PhysicsWorldApply(
      original, a.at("spec"),
      [&](const std::filesystem::path &path) {
        auto raw = Monstey_ReadFile(path);
        inputs.push_back({path, Monstey_HashSha256(raw)});
        return raw;
      },
      offset, limit);
  return Monstey_PPublish(inputs, a, result.first, std::move(result.second));
}
} // namespace Monstey
