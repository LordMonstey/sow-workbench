#include "Monstey_ModelEnvelope.hpp"
#include "Monstey_Model.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <exception>
#include <limits>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
void Monstey_ERequire(bool condition, const std::string &message) {
  if (!condition)
    throw Monstey_Error("model_envelope", message);
}
struct Monstey_EnvelopeSnapshot {
  std::filesystem::path path;
  std::string hash;
  Monstey_Bytes bytes;
};
struct Monstey_EnvelopePrepared {
  std::array<Monstey_EnvelopeSnapshot, 3> inputs;
  Monstey_Bytes output;
  J report;
};
std::filesystem::path Monstey_EPath(const J &arguments, const char *key) {
  Monstey_ERequire(arguments.contains(key) && arguments.at(key).is_string() &&
                       !arguments.at(key).as_string().empty(),
                   std::string("Missing path ") + key);
  return std::filesystem::path(Monstey_Wide(arguments.at(key).as_string()));
}
std::string Monstey_EHash(const J &arguments, const char *key) {
  Monstey_ERequire(arguments.contains(key) && arguments.at(key).is_string(),
                   std::string("Missing SHA256 precondition ") + key);
  auto value = arguments.at(key).as_string();
  Monstey_ERequire(value.size() == 64 &&
                       std::all_of(value.begin(), value.end(),
                                   [](unsigned char c) {
                                     return (c >= '0' && c <= '9') ||
                                            (c >= 'a' && c <= 'f') ||
                                            (c >= 'A' && c <= 'F');
                                   }),
                   std::string("Invalid SHA256 precondition ") + key);
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}
void Monstey_EArguments(const J &arguments, bool build) {
  Monstey_ERequire(arguments.is_object(), "Envelope arguments need an object");
  const std::set<std::string> keys = {"baseline_mesh",
                                      "encoded_mesh",
                                      "skeleton",
                                      "expected_baseline_sha256",
                                      "expected_encoded_sha256",
                                      "expected_skeleton_sha256"};
  for (const auto &[key, value] : arguments.as_object()) {
    (void)value;
    Monstey_ERequire(keys.contains(key) ||
                         (build && (key == "output" || key == "report")),
                     "Unknown envelope argument " + key);
  }
}
void Monstey_EUnchanged(const std::array<Monstey_EnvelopeSnapshot, 3> &inputs) {
  for (const auto &input : inputs)
    if (Monstey_HashSha256(input.path) != input.hash)
      throw Monstey_Error("source_changed",
                          "Envelope input changed after snapshot: " +
                              Monstey_Utf8(input.path.wstring()));
}
void Monstey_ESameRig(const Monstey_ModelScene &baseline,
                      const Monstey_ModelScene &encoded) {
  Monstey_ERequire(!baseline.bones.empty() &&
                       baseline.bones.size() == encoded.bones.size() &&
                       baseline.hk_to_source == encoded.hk_to_source &&
                       baseline.source_to_hk == encoded.source_to_hk,
                   "Baseline/encoded mesh rig count or permutation differs");
  for (std::size_t i = 0; i < baseline.bones.size(); ++i) {
    const auto &a = baseline.bones[i];
    const auto &b = encoded.bones[i];
    Monstey_ERequire(
        a.hk_index == b.hk_index && a.source_index == b.source_index &&
            a.parent_hk == b.parent_hk && a.native_id == b.native_id &&
            a.name == b.name && a.global_bind_metres == b.global_bind_metres,
        "Baseline/encoded mesh bind rig differs");
  }
}
struct Monstey_EnvelopeExtent {
  double maximum = 0;
  std::uint64_t vertices = 0, outside = 0;
};
Monstey_EnvelopeExtent Monstey_EExtent(const Monstey_ModelScene &scene,
                                       double radius) {
  Monstey_EnvelopeExtent extent;
  for (const auto &part : scene.parts) {
    const auto found = part.channels.find("POSITION");
    Monstey_ERequire(found != part.channels.end() && found->second.width == 3 &&
                         found->second.values.size() % 3 == 0,
                     "Invalid decoded POSITION channel");
    const auto &values = found->second.values;
    for (std::size_t i = 0; i < values.size(); i += 3) {
      const auto distance = std::hypot(values[i], values[i + 1], values[i + 2]);
      Monstey_ERequire(std::isfinite(distance),
                       "Envelope needs finite decoded positions");
      extent.maximum = std::max(extent.maximum, distance);
      ++extent.vertices;
      if (distance > radius)
        ++extent.outside;
    }
  }
  Monstey_ERequire(extent.vertices > 0,
                   "Envelope needs nonempty encoded geometry");
  return extent;
}
J Monstey_EExtentJson(const Monstey_EnvelopeExtent &extent) {
  return J::object({{"vertices", extent.vertices},
                    {"maximum_origin_distance_cm", extent.maximum},
                    {"positions_outside_source_radius", extent.outside}});
}
Monstey_EnvelopePrepared Monstey_EPrepare(const J &arguments, bool build) {
  Monstey_EArguments(arguments, build);
  Monstey_EnvelopePrepared state;
  const std::array<const char *, 3> pathKeys = {"baseline_mesh", "encoded_mesh",
                                                "skeleton"};
  const std::array<const char *, 3> hashKeys = {"expected_baseline_sha256",
                                                "expected_encoded_sha256",
                                                "expected_skeleton_sha256"};
  for (std::size_t i = 0; i < state.inputs.size(); ++i) {
    auto &input = state.inputs[i];
    input.path = Monstey_EPath(arguments, pathKeys[i]);
    input.hash = Monstey_EHash(arguments, hashKeys[i]);
    input.bytes = Monstey_ReadFile(input.path);
    if (Monstey_HashSha256(input.bytes) != input.hash)
      throw Monstey_Error("source_hash_mismatch",
                          std::string("Envelope SHA256 differs: ") +
                              pathKeys[i]);
  }
  const auto &skel = state.inputs[2];
  Monstey_ERequire(Monstey_U32(skel.bytes, 0) == 0x4c454b53 &&
                       Monstey_U32(skel.bytes, 4) == 17,
                   "Envelope requires SKEL17");
  const auto sourceBits = Monstey_U32(skel.bytes, 68);
  const auto oldRadius = std::bit_cast<float>(sourceBits);
  Monstey_ERequire(std::isfinite(oldRadius) && oldRadius >= 0,
                   "SKEL radius must be finite and nonnegative");
  // Shared native codecs validate hierarchy/bind data against the explicit
  // source skeleton. Returned hashes also catch a transient decoder-read race.
  const auto baseline =
      Monstey_ModelReadSceneBytes(state.inputs[0].bytes, skel.path);
  const auto encoded =
      Monstey_ModelReadSceneBytes(state.inputs[1].bytes, skel.path);
  Monstey_ERequire(baseline.skeleton_sha256 == skel.hash &&
                       encoded.skeleton_sha256 == skel.hash,
                   "Skeleton changed during bind validation");
  Monstey_ESameRig(baseline, encoded);
  const auto before = Monstey_EExtent(baseline, oldRadius),
             after = Monstey_EExtent(encoded, oldRadius);
  const auto padding =
      std::max(0.0, static_cast<double>(oldRadius) - before.maximum);
  double required =
      std::max(static_cast<double>(oldRadius), after.maximum + padding);
  // Keep exact identity for unchanged/shrinking geometry despite cancellation.
  if (after.maximum <= before.maximum && after.maximum <= oldRadius)
    required = oldRadius;
  Monstey_ERequire(std::isfinite(required) &&
                       required <= std::numeric_limits<float>::max(),
                   "Required native radius is outside finite float32 range");
  float radius = oldRadius;
  if (required > oldRadius) {
    radius = static_cast<float>(required);
    if (static_cast<double>(radius) < required)
      radius = std::nextafter(radius, std::numeric_limits<float>::infinity());
  }
  Monstey_ERequire(std::isfinite(radius) && radius >= oldRadius &&
                       radius >= required && radius >= after.maximum,
                   "Conservative float32 radius cannot be represented");
  state.output = skel.bytes;
  const auto outputBits = std::bit_cast<std::uint32_t>(radius);
  Monstey_PutU32(state.output, 68, outputBits);
  Monstey_ERequire(std::equal(skel.bytes.begin(), skel.bytes.begin() + 68,
                              state.output.begin()) &&
                       std::equal(skel.bytes.begin() + 72, skel.bytes.end(),
                                  state.output.begin() + 72),
                   "Envelope changed opaque skeleton bytes");
  (void)Monstey_ModelInspectSkeleton(
      state.output, J::object({{"table_limit", 0}, {"row_limit", 0}}));
  J inputReport = J::object();
  for (std::size_t i = 0; i < state.inputs.size(); ++i)
    inputReport[pathKeys[i]] = J::object(
        {{"path", Monstey_Utf8(state.inputs[i].path.wstring())},
         {"sha256", state.inputs[i].hash},
         {"bytes", static_cast<std::uint64_t>(state.inputs[i].bytes.size())}});
  state.report = J::object(
      {{"format", "SKEL17"},
       {"operation", build ? "model_envelope_build" : "model_envelope_inspect"},
       {"inputs", inputReport},
       {"native_local_center_cm", J::array({0, 0, 0})},
       {"baseline", Monstey_EExtentJson(before)},
       {"encoded", Monstey_EExtentJson(after)},
       {"radius_offset", 68},
       {"source_radius_cm", oldRadius},
       {"source_radius_float32_bits", Monstey_Hex(sourceBits)},
       {"donor_bind_padding_cm", padding},
       {"required_radius_cm", required},
       {"output_radius_cm", radius},
       {"output_radius_float32_bits", Monstey_Hex(outputBits)},
       {"changed", sourceBits != outputBits},
       {"candidate_skeleton_sha256", Monstey_HashSha256(state.output)},
       {"rig_verified", true},
       {"bind_geometry_covered", true},
       {"donor_bind_padding_retained", true},
       {"native_radius_consumer_qualified", true},
       {"all_animation_poses_verified", false},
       {"cloth_deformation_verified", false},
       {"other_skeleton_bytes_preserved", true},
       {"mesh_metadata_modified", false},
       {"in_game_verified", false}});
  Monstey_EUnchanged(state.inputs);
  return state;
}
J Monstey_ESchema(bool build) {
  auto path = [] {
    return J::object({{"type", "string"}, {"format", "path"}});
  };
  auto hash = [] {
    return J::object({{"type", "string"}, {"pattern", "^[a-fA-F0-9]{64}$"}});
  };
  J properties = J::object({{"baseline_mesh", path()},
                            {"encoded_mesh", path()},
                            {"skeleton", path()},
                            {"expected_baseline_sha256", hash()},
                            {"expected_encoded_sha256", hash()},
                            {"expected_skeleton_sha256", hash()}});
  auto required = J::array(
      {"baseline_mesh", "encoded_mesh", "skeleton", "expected_baseline_sha256",
       "expected_encoded_sha256", "expected_skeleton_sha256"});
  if (build) {
    properties["output"] = path();
    properties["report"] = path();
    required.as_array().emplace_back("output");
  }
  return J::object({{"type", "object"},
                    {"properties", properties},
                    {"required", required},
                    {"additionalProperties", false}});
}
bool Monstey_ERollback(const std::filesystem::path &path,
                       const std::string &expected) noexcept {
  try {
    const auto native = Monstey_WindowsFilePath(path);
    if (!std::filesystem::exists(native))
      return true;
    if (Monstey_HashSha256(path) != expected)
      return false;
    std::error_code error;
    std::filesystem::remove(native, error);
    return !error && !std::filesystem::exists(native);
  } catch (...) {
    return false;
  }
}
} // namespace
J Monstey_ModelEnvelopeCatalog() {
  return J::array(
      {J::object({{"name", "model_envelope_inspect"},
                  {"description",
                   "Inspect SKEL17 bind-geometry sphere growth against "
                   "explicit hashed native mesh/skeleton inputs."},
                  {"inputSchema", Monstey_ESchema(false)}}),
       J::object({{"name", "model_envelope_build"},
                  {"description", "Author a new SKEL17 sphere retaining donor "
                                  "bind padding; preserve every other byte. "
                                  "Animation/cloth coverage stays unverified."},
                  {"inputSchema", Monstey_ESchema(true)}})});
}
J Monstey_ModelEnvelopeInspect(const J &arguments) {
  return Monstey_EPrepare(arguments, false).report;
}
J Monstey_ModelEnvelopeBuild(const J &arguments) {
  auto state = Monstey_EPrepare(arguments, true);
  const auto output = Monstey_EPath(arguments, "output");
  const auto report = arguments.contains("report")
                          ? Monstey_EPath(arguments, "report")
                          : std::filesystem::path{};
  std::array<std::filesystem::path, 3> sources;
  for (std::size_t i = 0; i < sources.size(); ++i)
    sources[i] = state.inputs[i].path;
  Monstey_RequireOfflineOutput(output, sources);
  Monstey_ERequire(!std::filesystem::exists(Monstey_WindowsFilePath(output)),
                   "Skeleton output already exists");
  if (!report.empty()) {
    const std::array<std::filesystem::path, 4> protectedPaths = {
        sources[0], sources[1], sources[2], output};
    Monstey_RequireOfflineOutput(report, protectedPaths);
    Monstey_ERequire(!std::filesystem::exists(Monstey_WindowsFilePath(report)),
                     "Envelope report already exists");
  }
  state.report["output"] = Monstey_Utf8(output.wstring());
  const auto outputHash = Monstey_HashSha256(state.output);
  const auto reportText = state.report.dump(2) + "\n";
  const auto reportHash = Monstey_HashSha256(
      std::span(reinterpret_cast<const std::uint8_t *>(reportText.data()),
                reportText.size()));
  bool outputWritten = false, reportWritten = false;
  try {
    Monstey_EUnchanged(state.inputs);
    Monstey_WriteNewFile(output, state.output);
    outputWritten = true;
    Monstey_ERequire(Monstey_ReadFile(output) == state.output,
                     "Published skeleton failed exact readback");
    Monstey_EUnchanged(state.inputs);
    if (!report.empty()) {
      Monstey_WriteNewText(report, reportText);
      reportWritten = true;
    }
    Monstey_EUnchanged(state.inputs);
  } catch (...) {
    const auto original = std::current_exception();
    const bool reportClean =
        !reportWritten || Monstey_ERollback(report, reportHash);
    const bool outputClean =
        !outputWritten || Monstey_ERollback(output, outputHash);
    if (!reportClean || !outputClean) {
      try {
        std::rethrow_exception(original);
      } catch (const Monstey_Error &e) {
        throw Monstey_Error(
            e.code, std::string(e.what()) +
                        "; owned envelope output/report rollback was "
                        "incomplete; externally changed files were preserved");
      } catch (const std::exception &e) {
        throw Monstey_Error(
            "model_envelope_rollback",
            std::string(e.what()) +
                "; owned envelope output/report rollback was incomplete; "
                "externally changed files were preserved");
      } catch (...) {
        throw Monstey_Error("model_envelope_rollback",
                            "Envelope publication failed and owned "
                            "output/report rollback was incomplete");
      }
    }
    std::rethrow_exception(original);
  }
  return state.report;
}
} // namespace Monstey