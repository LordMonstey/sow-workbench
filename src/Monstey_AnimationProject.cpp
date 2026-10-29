#include "Monstey_AnimationProject.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <exception>
#include <map>
#include <set>
#include <type_traits>
namespace Monstey {
namespace {
using J = Monstey_Json;
void Monstey_PRequire(bool ok, std::string_view message) {
  if (!ok)
    throw Monstey_Error("animation_project", std::string(message));
}
std::string Monstey_PPath(const std::filesystem::path &path) {
  return Monstey_Utf8(
      std::filesystem::absolute(path).lexically_normal().wstring());
}
struct Monstey_PInput {
  Monstey_Bytes bytes;
  std::string sha;
};
Monstey_PInput Monstey_PRead(const std::filesystem::path &path,
                             std::string_view expected = {}) {
  Monstey_PRequire(!path.empty(), "Animation source path must be explicit");
  auto bytes = Monstey_ReadFile(path);
  auto sha = Monstey_HashSha256(bytes);
  if ((!expected.empty() && sha != expected) || Monstey_HashSha256(path) != sha)
    throw Monstey_Error("source_hash_mismatch",
                        "Animation input bytes changed or hash differs");
  return {std::move(bytes), std::move(sha)};
}
J Monstey_PJson(const Monstey_Bytes &bytes) {
  auto text = std::string_view(reinterpret_cast<const char *>(bytes.data()),
                               bytes.size());
  if (text.starts_with("\xef\xbb\xbf"))
    text.remove_prefix(3);
  (void)Monstey_Wide(text);
  return J::Parse(text);
}
std::uint32_t Monstey_PHash(const J &value) {
  if (value.is_integer()) {
    Monstey_PRequire(value.as_uint() <= UINT32_MAX,
                     "Animation channel hash exceeds UInt32");
    return static_cast<std::uint32_t>(value.as_uint());
  }
  auto text = value.as_string();
  if (text.starts_with("0x") || text.starts_with("0X"))
    text.erase(0, 2);
  std::uint32_t result{};
  const auto parsed =
      std::from_chars(text.data(), text.data() + text.size(), result, 16);
  Monstey_PRequire(!text.empty() && parsed.ec == std::errc{} &&
                       parsed.ptr == text.data() + text.size(),
                   "Invalid animation channel hash");
  return result;
}
std::string Monstey_PFold(std::string_view value) {
  std::string result(value);
  for (auto &c : result)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + 32);
  return result;
}
Monstey_Bytes Monstey_PText(const J &value) {
  const auto text = value.dump(2) + "\n";
  return Monstey_Bytes(text.begin(), text.end());
}
J Monstey_PClipRow(const J &clip) {
  auto row = clip;
  row["segment_count"] = static_cast<std::uint64_t>(row["segments"].size());
  row.as_object().erase("segments");
  return row;
}
} // namespace
void Monstey_AnimationPublishOffline(
    const std::filesystem::path &output, const Monstey_Bytes &bytes,
    std::span<const std::filesystem::path> sources,
    const std::function<void()> &check) {
  Monstey_RequireOfflineOutput(output, sources);
  check();
  bool created = false;
  const auto sha = Monstey_HashSha256(bytes);
  try {
    Monstey_WriteNewFile(output, bytes);
    created = true;
    Monstey_PRequire(Monstey_HashSha256(output) == sha,
                     "Published animation output readback differs");
    check();
  } catch (...) {
    const auto original = std::current_exception();
    bool clean = !created;
    if (created) {
      try {
        if (!std::filesystem::exists(Monstey_WindowsFilePath(output)))
          clean = true;
        else if (Monstey_HashSha256(output) == sha) {
          std::error_code error;
          const bool removed =
              std::filesystem::remove(Monstey_WindowsFilePath(output), error);
          clean = !error && (removed || !std::filesystem::exists(
                                            Monstey_WindowsFilePath(output)));
        }
      } catch (...) {
        clean = false;
      }
    }
    if (!clean) {
      try {
        std::rethrow_exception(original);
      } catch (const Monstey_Error &error) {
        throw Monstey_Error(
            error.code, std::string(error.what()) +
                            "; owned animation output rollback was incomplete; "
                            "externally changed files were preserved");
      } catch (const std::exception &error) {
        throw Monstey_Error(
            "animation_rollback",
            std::string(error.what()) +
                "; owned animation output rollback was incomplete; externally "
                "changed files were preserved");
      } catch (...) {
        throw Monstey_Error("animation_rollback",
                            "Animation publication failed and owned output "
                            "rollback was incomplete");
      }
    }
    std::rethrow_exception(original);
  }
}
struct Monstey_AnimationProject::Monstey_Impl {
  std::filesystem::path source, project;
  std::string sourceKind, sha, baseHash, currentHash;
  Monstey_Anix base, current;
  std::unique_ptr<J> metadata;
  std::vector<std::unique_ptr<J>> steps;
  std::size_t cursor{};
  Monstey_Impl(const std::filesystem::path &path, Monstey_PInput input,
               bool semantic)
      : source(std::filesystem::absolute(path).lexically_normal()),
        sourceKind(semantic ? "semantic" : "native"), sha(std::move(input.sha)),
        base(semantic ? Monstey_Anix::Create(Monstey_PJson(input.bytes))
                      : Monstey_Anix(input.bytes)),
        current(base), metadata(std::make_unique<J>(current.Describe())) {
    baseHash = Monstey_HashSha256(base.Build());
    currentHash = baseHash;
    Check();
  }
  void Check() const {
    if (Monstey_HashSha256(source) != sha)
      throw Monstey_Error("source_hash_mismatch",
                          "Animation base source changed; reopen it");
  }
  Monstey_Anix Replay(std::size_t count) const {
    Monstey_Anix next(base);
    for (std::size_t i = 0; i < count; ++i) {
      if ((*steps[i])["kind"].as_string() == "revert")
        next = base;
      else
        next.Edit((*steps[i])["operations"]);
    }
    return next;
  }
  void Install(Monstey_Anix next, J step) {
    Monstey_PRequire(cursor < 10000,
                     "Animation project history exceeds 10000 steps");
    const auto bytes = next.Build();
    auto nextMetadata = std::make_unique<J>(next.Describe());
    auto nextHash = Monstey_HashSha256(bytes);
    // MSVC map move construction may allocate its sentinel. Construct the
    // owned journal entry and reserve storage before changing the redo branch.
    auto nextStep = std::make_unique<J>(std::move(step));
    static_assert(std::is_nothrow_move_assignable_v<std::unique_ptr<J>> &&
                  std::is_nothrow_move_assignable_v<Monstey_Anix>);
    steps.reserve(cursor + 1);
    Check();
    steps.resize(cursor);
    steps.push_back(std::move(nextStep));
    current = std::move(next);
    metadata = std::move(nextMetadata);
    currentHash = std::move(nextHash);
    cursor = steps.size();
  }
  std::vector<std::filesystem::path> Sources() const {
    auto result = std::vector<std::filesystem::path>{source};
    if (!project.empty())
      result.push_back(project);
    return result;
  }
};
Monstey_AnimationProject::Monstey_AnimationProject(
    const std::filesystem::path &source, std::string_view expected,
    bool semantic)
    : impl_(std::make_unique<Monstey_Impl>(
          source, Monstey_PRead(source, expected), semantic)) {}
Monstey_AnimationProject::~Monstey_AnimationProject() = default;
J Monstey_AnimationProject::Describe() const {
  const auto &p = *impl_;
  p.Check();
  auto result = *p.metadata;
  result.as_object().erase("clips");
  result.as_object().erase("channels");
  result["source"] = Monstey_PPath(p.source);
  result["source_kind"] = p.sourceKind;
  result["source_sha256"] = p.sha;
  result["native_baseline_sha256"] = p.baseHash;
  result["effective_sha256"] = p.currentHash;
  result["dirty"] = p.currentHash != p.baseHash;
  result["can_undo"] = p.cursor > 0;
  result["can_redo"] = p.cursor < p.steps.size();
  result["undo_steps"] = p.cursor;
  result["redo_steps"] = p.steps.size() - p.cursor;
  result["implementation"] = "C++20";
  result["python_runtime"] = false;
  return result;
}
J Monstey_AnimationProject::Clips(std::string_view query, std::size_t offset,
                                  std::size_t limit) const {
  const auto &p = *impl_;
  p.Check();
  Monstey_PRequire(limit <= 10000, "Animation page exceeds 10000 clips");
  const auto filter = Monstey_PFold(query);
  auto rows = J::array();
  std::size_t total = 0;
  for (const auto &clip : (*p.metadata)["clips"].as_array()) {
    if (!filter.empty() &&
        Monstey_PFold(clip["name"].as_string()).find(filter) ==
            std::string::npos)
      continue;
    const auto ordinal = total++;
    if (ordinal >= offset && rows.size() < limit)
      rows.as_array().push_back(Monstey_PClipRow(clip));
  }
  p.Check();
  return J::object(
      {{"rows", rows}, {"total", total}, {"offset", offset}, {"limit", limit}});
}
J Monstey_AnimationProject::Clip(std::string_view name) const {
  const auto &p = *impl_;
  p.Check();
  const auto folded = Monstey_PFold(name);
  for (const auto &clip : (*p.metadata)["clips"].as_array())
    if (Monstey_PFold(clip["name"].as_string()) == folded) {
      auto result = clip;
      const auto events = p.current.EventInfo(name);
      for (const auto &[key, value] : events.as_object())
        result[key == "event_channel_choices" ? "event_channels" : key] = value;
      p.Check();
      return result;
    }
  throw Monstey_Error("animation_clip_not_found",
                      "Animation clip does not exist");
}
J Monstey_AnimationProject::Sample(std::string_view name, const J &times,
                                   bool fallback) const {
  const auto &p = *impl_;
  p.Check();
  Monstey_PRequire(times.is_array() && !times.empty() && times.size() <= 1024,
                   "Sample requires 1..1024 explicit times");
  std::vector<double> ticks;
  for (const auto &time : times.as_array())
    ticks.push_back(time.as_number());
  auto result = p.current.Sample(name, ticks, fallback);
  p.Check();
  return result;
}
J Monstey_AnimationProject::Edit(const J &operations) {
  auto &p = *impl_;
  p.Check();
  Monstey_Anix next(p.current);
  next.Edit(operations);
  p.Install(std::move(next),
            J::object({{"kind", "edit"}, {"operations", operations}}));
  return Describe();
}
J Monstey_AnimationProject::ImportClip(const std::filesystem::path &file,
                                       std::string_view clipName,
                                       std::string_view name,
                                       std::string_view expected) {
  auto &p = *impl_;
  p.Check();
  const auto input = Monstey_PRead(file, expected);
  const auto scene = Monstey_PJson(input.bytes);
  Monstey_PRequire(scene["format"].as_string() == "Monstey_ANIX_Semantic_v1",
                   "Import requires a full semantic animation scene");
  const J *found = nullptr;
  for (const auto &clip : scene["clips"].as_array())
    if (Monstey_PFold(clip["name"].as_string()) == Monstey_PFold(clipName)) {
      Monstey_PRequire(!found, "Ambiguous imported clip name");
      found = &clip;
    }
  Monstey_PRequire(found != nullptr,
                   "Imported semantic scene has no requested clip");
  auto clip = *found;
  const auto &sourceChannels = scene["channels"];
  const auto &targetChannels = (*p.metadata)["channels"];
  for (auto &track : clip["tracks"].as_array()) {
    const auto sourceIndex = track["channel_index"].as_uint();
    Monstey_PRequire(sourceIndex < sourceChannels.size(),
                     "Imported source channel index is invalid");
    const auto &sourceChannel =
        sourceChannels[static_cast<std::size_t>(sourceIndex)];
    std::size_t match = SIZE_MAX;
    for (std::size_t i = 0; i < targetChannels.size(); ++i)
      if (Monstey_PHash(sourceChannel["type_hash"]) ==
              Monstey_PHash(targetChannels[i]["type_hash"]) &&
          Monstey_PHash(sourceChannel["bone_hash"]) ==
              Monstey_PHash(targetChannels[i]["bone_hash"])) {
        Monstey_PRequire(match == SIZE_MAX,
                         "Target animation channel mapping is ambiguous");
        match = i;
      }
    Monstey_PRequire(match != SIZE_MAX,
                     "Imported channel has no matching target type/bone hash");
    track["channel_index"] = static_cast<std::uint64_t>(match);
  }
  if (clip.contains("event_channel_index") ||
      (clip.contains("events") && !clip["events"].empty())) {
    std::size_t sourceEvent = SIZE_MAX;
    if (clip.contains("event_channel_index"))
      sourceEvent =
          static_cast<std::size_t>(clip["event_channel_index"].as_uint());
    else
      for (std::size_t i = 0; i < sourceChannels.size(); ++i)
        if (Monstey_PHash(sourceChannels[i]["type_hash"]) == 0x210c8f5au) {
          Monstey_PRequire(sourceEvent == SIZE_MAX,
                           "Source event channel is ambiguous; export its "
                           "explicit identity");
          sourceEvent = i;
        }
    Monstey_PRequire(
        sourceEvent < sourceChannels.size() &&
            Monstey_PHash(sourceChannels[sourceEvent]["type_hash"]) ==
                0x210c8f5au,
        "Imported event channel is absent or has the wrong native type");
    std::size_t targetEvent = SIZE_MAX;
    for (std::size_t i = 0; i < targetChannels.size(); ++i)
      if (Monstey_PHash(targetChannels[i]["type_hash"]) == 0x210c8f5au &&
          Monstey_PHash(targetChannels[i]["bone_hash"]) ==
              Monstey_PHash(sourceChannels[sourceEvent]["bone_hash"])) {
        Monstey_PRequire(targetEvent == SIZE_MAX,
                         "Target event channel identity is ambiguous");
        targetEvent = i;
      }
    Monstey_PRequire(
        targetEvent != SIZE_MAX,
        "Imported event channel has no matching target type/bone hash");
    clip["event_channel_index"] = static_cast<std::uint64_t>(targetEvent);
  }
  if (!name.empty())
    clip["name"] = name;
  Monstey_PRequire(Monstey_HashSha256(file) == input.sha,
                   "Imported semantic source changed while reading");
  return Edit(J::array({J::object({{"op", "import"}, {"clip", clip}})}));
}
J Monstey_AnimationProject::Undo() {
  auto &p = *impl_;
  p.Check();
  Monstey_PRequire(p.cursor > 0, "No animation edit to undo");
  auto next = p.Replay(p.cursor - 1);
  auto meta = std::make_unique<J>(next.Describe());
  auto hash = Monstey_HashSha256(next.Build());
  p.Check();
  p.current = std::move(next);
  p.metadata = std::move(meta);
  p.currentHash = std::move(hash);
  --p.cursor;
  return Describe();
}
J Monstey_AnimationProject::Redo() {
  auto &p = *impl_;
  p.Check();
  Monstey_PRequire(p.cursor < p.steps.size(), "No animation edit to redo");
  auto next = p.Replay(p.cursor + 1);
  auto meta = std::make_unique<J>(next.Describe());
  auto hash = Monstey_HashSha256(next.Build());
  p.Check();
  p.current = std::move(next);
  p.metadata = std::move(meta);
  p.currentHash = std::move(hash);
  ++p.cursor;
  return Describe();
}
J Monstey_AnimationProject::Revert() {
  auto &p = *impl_;
  p.Check();
  p.Install(p.base, J::object({{"kind", "revert"}}));
  return Describe();
}
J Monstey_AnimationProject::Changes() const { return Changes(J::object()); }
J Monstey_AnimationProject::Changes(const J &options) const {
  const auto &p = *impl_;
  p.Check();
  Monstey_PRequire(options.is_object(), "History options must be an object");
  for (const auto &entry : options.as_object())
    Monstey_PRequire(entry.first == "include_operations" ||
                         entry.first == "offset" || entry.first == "limit",
                     "Unknown history view option");
  const auto offset = options.value("offset", 0u).as_uint();
  const auto limit = options.value("limit", 10000u).as_uint();
  const bool include = options.value("include_operations", true).as_bool();
  Monstey_PRequire(offset <= p.cursor && limit > 0 && limit <= 10000,
                   "History page is outside its native journal limits");
  const auto count = std::min<std::uint64_t>(p.cursor - offset, limit);
  auto steps = J::array();
  for (std::size_t i = static_cast<std::size_t>(offset); i < offset + count;
       ++i) {
    const auto &step = *p.steps[i];
    if (include)
      steps.as_array().push_back(step);
    else {
      std::set<std::string> kinds, clips;
      std::uint64_t operations = 0;
      J first = nullptr;
      if (step.contains("operations"))
        for (const auto &operation : step["operations"].as_array()) {
          ++operations;
          const auto kind = operation.at("op").as_string();
          kinds.insert(kind);
          const auto name = kind == "import"
                                ? operation.at("clip").at("name").as_string()
                            : kind == "held" ? operation.at("name").as_string()
                                             : operation.at("clip").as_string();
          clips.insert(name);
          if (first.is_null())
            first = name;
          if (kind == "rename" || kind == "duplicate")
            clips.insert(operation.at("name").as_string());
        }
      auto names = J::array();
      for (const auto &name : kinds)
        names.as_array().push_back(name);
      steps.as_array().push_back(
          J::object({{"kind", step.at("kind")},
                     {"operation_count", operations},
                     {"operation_kinds", names},
                     {"affected_clip_count", clips.size()},
                     {"first_clip", first}}));
    }
  }
  auto result = Describe();
  result["steps"] = std::move(steps);
  if (!options.empty()) {
    result["total_steps"] = p.cursor;
    result["offset"] = offset;
    result["limit"] = limit;
    result["returned_steps"] = count;
    result["has_more"] = offset + count < p.cursor;
    result["operations_included"] = include;
  }
  return result;
}
J Monstey_AnimationProject::Export(const std::filesystem::path &output,
                                   std::string_view clip) const {
  const auto &p = *impl_;
  p.Check();
  J scene;
  if (clip.empty())
    scene = p.current.Export();
  else {
    auto channels = J::array();
    for (const auto &channel : (*p.metadata)["channels"].as_array())
      channels.as_array().push_back(
          J::object({{"type_hash", channel["type_hash"]},
                     {"bone_hash", channel["bone_hash"]}}));
    scene = J::object({{"format", "Monstey_ANIX_Semantic_v1"},
                       {"library_path", (*p.metadata)["library_path"]},
                       {"channels", channels},
                       {"clips", J::array({p.current.ExportClip(clip)})},
                       {"source_sha256", (*p.metadata)["source_sha256"]},
                       {"runtime_verified", false}});
  }
  const auto bytes = Monstey_PText(scene);
  const auto sources = p.Sources();
  Monstey_AnimationPublishOffline(output, bytes, sources, [&] { p.Check(); });
  auto unsupported = J::array();
  for (const auto &row : scene["clips"].as_array())
    if (!row["semantic_reimport_supported"].as_bool())
      unsupported.as_array().push_back(row["name"]);
  return J::object({{"path", Monstey_PPath(output)},
                    {"sha256", Monstey_HashSha256(bytes)},
                    {"bytes", bytes.size()},
                    {"clips", scene["clips"].size()},
                    {"non_reimportable_clips", unsupported},
                    {"runtime_verified", false}});
}
J Monstey_AnimationProject::SaveProject(
    const std::filesystem::path &output) const {
  const auto &p = *impl_;
  const auto changes = Changes();
  const auto doc = J::object({{"format", "MONSTEY_CPP_ANIX_PROJECT"},
                              {"version", 1},
                              {"source", Monstey_PPath(p.source)},
                              {"source_kind", p.sourceKind},
                              {"source_sha256", p.sha},
                              {"native_baseline_sha256", p.baseHash},
                              {"effective_sha256", p.currentHash},
                              {"steps", changes["steps"]}});
  const auto bytes = Monstey_PText(doc);
  const auto sources = p.Sources();
  Monstey_AnimationPublishOffline(output, bytes, sources, [&] { p.Check(); });
  return J::object({{"path", Monstey_PPath(output)},
                    {"sha256", Monstey_HashSha256(bytes)},
                    {"bytes", bytes.size()},
                    {"source_qualified", true}});
}
std::unique_ptr<Monstey_AnimationProject>
Monstey_AnimationProject::LoadProject(const std::filesystem::path &path) {
  const auto input = Monstey_PRead(path);
  const auto doc = Monstey_PJson(input.bytes);
  Monstey_PRequire(doc["format"].as_string() == "MONSTEY_CPP_ANIX_PROJECT" &&
                       doc["version"].as_uint() == 1,
                   "Unsupported native animation project");
  const auto kind = doc["source_kind"].as_string();
  Monstey_PRequire(kind == "native" || kind == "semantic",
                   "Invalid animation project source kind");
  auto result = std::make_unique<Monstey_AnimationProject>(
      std::filesystem::path(Monstey_Wide(doc["source"].as_string())),
      doc["source_sha256"].as_string(), kind == "semantic");
  auto &p = *result->impl_;
  Monstey_PRequire(p.baseHash == doc["native_baseline_sha256"].as_string(),
                   "Animation project baseline differs");
  Monstey_PRequire(doc["steps"].is_array() && doc["steps"].size() <= 10000,
                   "Animation project step count invalid");
  for (const auto &step : doc["steps"].as_array()) {
    const auto stepKind = step["kind"].as_string();
    if (stepKind == "revert")
      result->Revert();
    else {
      Monstey_PRequire(stepKind == "edit", "Unknown animation project step");
      result->Edit(step["operations"]);
    }
  }
  Monstey_PRequire(p.currentHash == doc["effective_sha256"].as_string() &&
                       Monstey_HashSha256(path) == input.sha,
                   "Animation project replay or input hash differs");
  p.project = std::filesystem::absolute(path).lexically_normal();
  return result;
}
J Monstey_AnimationProject::Build(const std::filesystem::path &output) const {
  const auto &p = *impl_;
  p.Check();
  const auto bytes = p.current.Build();
  Monstey_PRequire(
      Monstey_HashSha256(bytes) == p.currentHash,
      "Native animation build differs from its verified session state");
  Monstey_Anix checked(bytes);
  auto metadata = checked.Describe();
  Monstey_PRequire(metadata["directory_footer_consistent"].as_bool(),
                   "Animation build footer is inconsistent");
  const auto sources = p.Sources();
  Monstey_AnimationPublishOffline(output, bytes, sources, [&] { p.Check(); });
  return J::object({{"status", "MONSTEY_ANIX44_BUILD_VALIDATED"},
                    {"path", Monstey_PPath(output)},
                    {"sha256", Monstey_HashSha256(bytes)},
                    {"bytes", bytes.size()},
                    {"clips", metadata["clip_count"]},
                    {"directory_footer_consistent", true},
                    {"no_op_byte_exact", p.currentHash == p.baseHash},
                    {"source_sha256", p.sha},
                    {"runtime_verified", false},
                    {"game_written", false}});
}
J Monstey_AnimationCatalog() {
  const auto text = J::object({{"type", "string"}, {"minLength", 1}});
  const auto path =
      J::object({{"type", "string"}, {"format", "path"}, {"minLength", 1}});
  const auto hash =
      J::object({{"type", "string"}, {"minLength", 64}, {"maxLength", 64}});
  const auto integer = J::object({{"type", "integer"}, {"minimum", 0}});
  auto result = J::array();
  auto add = [&](const char *name, const char *description, bool read,
                 J properties, J required = J::array()) {
    result.as_array().push_back(J::object(
        {{"name", name},
         {"description", description},
         {"read_only", read},
         {"inputSchema", J::object({{"type", "object"},
                                    {"properties", properties},
                                    {"required", required},
                                    {"additionalProperties", false}})}}));
  };
  add("animation_open", "Open a source-qualified native ANIX44 library.", false,
      J::object({{"path", path}, {"expected_sha256", hash}}),
      J::array({"path"}));
  add("animation_new",
      "Create a native library from a semantic animation scene file.", false,
      J::object({{"scene_path", path}, {"expected_sha256", hash}}),
      J::array({"scene_path"}));
  add("animation_describe",
      "Read compact animation source, codec and history metadata.", true,
      J::object());
  add("animation_clips", "Search and page the effective native clip directory.",
      true,
      J::object({{"query", J::object({{"type", "string"}})},
                 {"offset", integer},
                 {"limit", J::object({{"type", "integer"},
                                      {"minimum", 1},
                                      {"maximum", 500}})}}));
  add("animation_clip",
      "Inspect native clip segments, codecs and preserved extras.", true,
      J::object({{"clip", text}}), J::array({"clip"}));
  add("animation_sample",
      "Sample explicit millisecond times in native parent-local channels.",
      true,
      J::object(
          {{"clip", text},
           {"ticks", J::object({{"type", "array"},
                                {"minItems", 1},
                                {"maxItems", 1024},
                                {"items", J::object({{"type", "number"}})}})},
           {"bind_fallback", J::object({{"type", "boolean"}})}}),
      J::array({"clip", "ticks"}));
  add("animation_edit",
      "Apply one transactional batch of native animation edits.", false,
      J::object({{"operations",
                  J::object({{"type", "array"},
                             {"minItems", 1},
                             {"maxItems", 10000},
                             {"items", J::object({{"type", "object"}})}})},
                 {"operations_path", path},
                 {"expected_sha256", hash}}));
  add("animation_import_clip",
      "Import one semantic clip, mapping shared channels by type and bone "
      "hashes.",
      false,
      J::object({{"scene_path", path},
                 {"clip", text},
                 {"name", text},
                 {"expected_sha256", hash}}),
      J::array({"scene_path", "clip"}));
  add("animation_export",
      "Export semantic animation data and unsupported-channel diagnostics to a "
      "new file.",
      false, J::object({{"output", path}, {"clip", text}}),
      J::array({"output"}));
  for (const auto name : {"animation_undo", "animation_redo",
                          "animation_revert", "animation_changes"}) {
    auto arguments = J::object();
    if (std::string_view(name) == "animation_changes")
      arguments =
          J::object({{"include_operations", J::object({{"type", "boolean"}})},
                     {"offset", integer},
                     {"limit", J::object({{"type", "integer"},
                                          {"minimum", 1},
                                          {"maximum", 10000}})}});
    add(name, "Animation project history and effective changes.",
        std::string_view(name) == "animation_changes", arguments);
  }
  add("animation_save_project", "Save source-qualified native animation edits.",
      false, J::object({{"path", path}}), J::array({"path"}));
  add("animation_load_project",
      "Reload and verify source-qualified native animation edits.", false,
      J::object({{"path", path}}), J::array({"path"}));
  add("animation_build", "Build, reparse and publish a new native ANIX44 file.",
      false, J::object({{"output", path}}), J::array({"output"}));
  add("animation_skeleton_inspect",
      "Inspect native SKEL animation library registration tables.", true,
      J::object({{"source", path}, {"expected_sha256", hash}}),
      J::array({"source"}));
  add("animation_skeleton_register",
      "Register an ANIX resource in the qualified native SKEL17 profile.",
      false,
      J::object({{"source", path},
                 {"resource", text},
                 {"output", path},
                 {"expected_sha256", hash}}),
      J::array({"source", "resource", "output", "expected_sha256"}));
  return result;
}
J Monstey_AnimationSkeletonCall(std::string_view op, const J &args) {
  const auto source =
      std::filesystem::path(Monstey_Wide(args["source"].as_string()));
  const auto input =
      Monstey_PRead(source, args.value("expected_sha256", "").as_string());
  if (op == "animation_skeleton_inspect") {
    auto report = Monstey_AnixSkeletonInspect(input.bytes);
    Monstey_PRequire(Monstey_HashSha256(source) == input.sha,
                     "Skeleton source changed");
    report["source_sha256"] = input.sha;
    return report;
  }
  Monstey_PRequire(op == "animation_skeleton_register",
                   "Unknown skeleton animation operation");
  const auto result =
      Monstey_AnixSkeletonRegister(input.bytes, args["resource"].as_string());
  const auto output =
      std::filesystem::path(Monstey_Wide(args["output"].as_string()));
  const std::array<std::filesystem::path, 1> sources{source};
  Monstey_AnimationPublishOffline(output, result.bytes, sources, [&] {
    Monstey_PRequire(Monstey_HashSha256(source) == input.sha,
                     "Skeleton source changed");
  });
  auto report = result.report;
  report["path"] = Monstey_PPath(output);
  report["sha256"] = Monstey_HashSha256(result.bytes);
  report["source_sha256"] = input.sha;
  return report;
}
} // namespace Monstey
