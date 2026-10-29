#include "Monstey_AnimationPreviewApi.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <limits>
namespace Monstey
{
namespace
{
using J = Monstey_Json;
void Monstey_AnimationPreviewApiRequire(bool valid, const std::string &message)
{
    if (!valid)
        throw Monstey_Error("animation_preview", message);
}
J Monstey_AnimationPreviewApiSchema()
{
    const auto path = J::object({{"type", "string"}, {"format", "path"}, {"minLength", 1}});
    const auto hash = J::object({{"type", "string"}, {"pattern", "^[a-fA-F0-9]{64}$"}});
    const auto boolean = J::object({{"type", "boolean"}});
    const auto dimension = J::object({{"type", "integer"}, {"minimum", 32}, {"maximum", 4096}});
    auto properties = J::object({{"source", path},
                                 {"output", path},
                                 {"skeleton", path},
                                 {"animation", path},
                                 {"clip", J::object({{"type", "string"}, {"minLength", 1}})},
                                 {"tick_ms", J::object({{"type", "number"}})},
                                 {"expected_source_sha256", hash},
                                 {"expected_skeleton_sha256", hash},
                                 {"expected_animation_sha256", hash},
                                 {"width", dimension},
                                 {"height", dimension},
                                 {"container", J::object({{"type", "integer"}, {"minimum", 0}})}});
    for (const auto key : {"all_containers", "mesh", "bones", "bones_in_front", "wireframe", "vertex_colors"})
        properties[key] = boolean;
    for (const auto key : {"yaw_degrees", "pitch_degrees"})
        properties[key] = J::object({{"type", "number"}});
    properties["zoom"] = J::object({{"type", "number"}, {"minimum", 0.05}, {"maximum", 20}});
    for (const auto key : {"pan_x", "pan_y"})
        properties[key] = J::object({{"type", "number"}, {"minimum", -8}, {"maximum", 8}});
    return J::object(
        {{"type", "object"},
         {"properties", properties},
         {"required", J::array({"source", "skeleton", "animation", "clip", "tick_ms", "output"})},
         {"additionalProperties", false}});
}
std::filesystem::path Monstey_AnimationPreviewApiPath(const J &a, const char *key)
{
    Monstey_AnimationPreviewApiRequire(a.contains(key) && a.at(key).is_string() &&
                                           !a.at(key).as_string().empty(),
                                       std::string("Missing or invalid preview path ") + key);
    const auto &value = a.at(key).as_string();
    Monstey_AnimationPreviewApiRequire(value.find('\0') == std::string::npos,
                                       std::string("NUL in preview path ") + key);
    return std::filesystem::path(Monstey_Wide(value));
}
std::string Monstey_AnimationPreviewApiExpected(const J &a, const char *key)
{
    if (!a.contains(key))
        return {};
    Monstey_AnimationPreviewApiRequire(a.at(key).is_string(),
                                       std::string("Invalid preview SHA256 type ") + key);
    auto value = a.at(key).as_string();
    Monstey_AnimationPreviewApiRequire(value.size() == 64 && std::all_of(value.begin(), value.end(),
                                                                         [](unsigned char c)
                                                                         {
                                                                             return (c >= '0' && c <= '9') ||
                                                                                    (c >= 'a' && c <= 'f') ||
                                                                                    (c >= 'A' && c <= 'F');
                                                                         }),
                                       std::string("Invalid preview SHA256 precondition ") + key);
    for (auto &c : value)
        if (c >= 'A' && c <= 'F')
            c = static_cast<char>(c - 'A' + 'a');
    return value;
}
Monstey_ModelPreviewOptions Monstey_AnimationPreviewApiOptions(const J &a)
{
    Monstey_ModelPreviewOptions o;
    const auto dimension = [&](const char *key, std::uint32_t fallback)
    {
        if (!a.contains(key))
            return fallback;
        Monstey_AnimationPreviewApiRequire(a.at(key).is_integer(),
                                           std::string("Expected integer preview option ") + key);
        const auto v = a.at(key).as_int();
        Monstey_AnimationPreviewApiRequire(v >= 32 && v <= 4096,
                                           std::string("Preview dimension outside 32..4096: ") + key);
        return static_cast<std::uint32_t>(v);
    };
    const auto boolean = [&](const char *key, bool fallback)
    {
        if (!a.contains(key))
            return fallback;
        Monstey_AnimationPreviewApiRequire(a.at(key).is_bool(),
                                           std::string("Expected Boolean preview option ") + key);
        return a.at(key).as_bool();
    };
    const auto number = [&](const char *key, double fallback)
    {
        if (!a.contains(key))
            return fallback;
        Monstey_AnimationPreviewApiRequire(a.at(key).is_number(),
                                           std::string("Expected numeric preview option ") + key);
        const auto v = a.at(key).as_number();
        Monstey_AnimationPreviewApiRequire(std::isfinite(v), std::string("Nonfinite preview option ") + key);
        return v;
    };
    o.width = dimension("width", o.width);
    o.height = dimension("height", o.height);
    Monstey_AnimationPreviewApiRequire(static_cast<std::uint64_t>(o.width) * o.height <= 8388608,
                                       "Preview pixel budget exceeds 8388608");
    if (a.contains("container"))
    {
        Monstey_AnimationPreviewApiRequire(a.at("container").is_integer() && a.at("container").as_int() >= 0,
                                           "Invalid preview container");
        const auto v = a.at("container").as_uint();
        Monstey_AnimationPreviewApiRequire(v <= std::numeric_limits<std::size_t>::max(),
                                           "Preview container overflows native size");
        o.container = static_cast<std::size_t>(v);
    }
    o.all_containers = boolean("all_containers", o.all_containers);
    o.mesh = boolean("mesh", o.mesh);
    o.bones = boolean("bones", o.bones);
    o.bones_in_front = boolean("bones_in_front", o.bones_in_front);
    o.wireframe = boolean("wireframe", o.wireframe);
    o.vertex_colors = boolean("vertex_colors", o.vertex_colors);
    o.yaw_degrees = number("yaw_degrees", o.yaw_degrees);
    o.pitch_degrees = number("pitch_degrees", o.pitch_degrees);
    o.zoom = number("zoom", o.zoom);
    o.pan_x = number("pan_x", o.pan_x);
    o.pan_y = number("pan_y", o.pan_y);
    Monstey_AnimationPreviewApiRequire(o.zoom >= 0.05 && o.zoom <= 20, "Preview zoom outside 0.05..20");
    Monstey_AnimationPreviewApiRequire(std::abs(o.pan_x) <= 8 && std::abs(o.pan_y) <= 8,
                                       "Preview pan outside -8..8");
    return o;
}
J Monstey_AnimationPreviewApiOptionsJson(const Monstey_ModelPreviewOptions &o)
{
    return J::object({{"width", o.width},
                      {"height", o.height},
                      {"container", static_cast<std::uint64_t>(o.container)},
                      {"all_containers", o.all_containers},
                      {"mesh", o.mesh},
                      {"bones", o.bones},
                      {"bones_in_front", o.bones_in_front},
                      {"wireframe", o.wireframe},
                      {"vertex_colors", o.vertex_colors},
                      {"yaw_degrees", o.yaw_degrees},
                      {"pitch_degrees", o.pitch_degrees},
                      {"zoom", o.zoom},
                      {"pan_x", o.pan_x},
                      {"pan_y", o.pan_y}});
}
struct Monstey_AnimationPreviewApiSnapshot
{
    std::filesystem::path path;
    std::string hash;
    std::uint64_t bytes = 0;
    bool expected = false;
};
void Monstey_AnimationPreviewApiUnchanged(std::span<const Monstey_AnimationPreviewApiSnapshot> inputs)
{
    for (const auto &input : inputs)
        if (Monstey_HashSha256(input.path) != input.hash)
            throw Monstey_Error("source_changed", "Preview input changed after snapshot: " +
                                                      Monstey_Utf8(input.path.wstring()));
}
// Called only after atomic no-replace publication. Never remove by pathname:
// a check-then-delete rollback can delete a competing replacement.
void Monstey_AnimationPreviewApiVerifyPublished(
    const std::filesystem::path &output, std::span<const std::uint8_t> bitmap,
    std::span<const Monstey_AnimationPreviewApiSnapshot> snapshots)
{
    try
    {
        const auto readback = Monstey_ReadFile(output);
        Monstey_AnimationPreviewApiRequire(readback.size() == bitmap.size() &&
                                               std::equal(readback.begin(), readback.end(), bitmap.begin()),
                                           "Animation preview bitmap failed exact readback");
        Monstey_AnimationPreviewApiUnchanged(snapshots);
    }
    catch (const Monstey_Error &e)
    {
        throw Monstey_Error(
            e.code, std::string(e.what()) +
                        "; post-publication verification failed; destination was not removed or restored: " +
                        Monstey_Utf8(output.wstring()));
    }
    catch (const std::exception &e)
    {
        throw Monstey_Error(
            "animation_preview_publication",
            std::string(e.what()) +
                "; post-publication verification failed; destination was not removed or restored: " +
                Monstey_Utf8(output.wstring()));
    }
}
} // namespace
J Monstey_AnimationPreviewCatalog()
{
    return J::array({J::object(
        {{"name", "model_animation_preview_render"},
         {"read_only", false},
         {"description", "Render a new offline BMP of one ordinary same-rig ANIX pose with the shared C++ "
                         "CPU geometry preview. Explicit SKEL and exact native identities are required. "
                         "Native controller, GPU materials, IK and cloth are not rendered."},
         {"inputSchema", Monstey_AnimationPreviewApiSchema()}})});
}
J Monstey_AnimationPreviewRenderFile(const J &a)
{
    Monstey_AnimationPreviewApiRequire(a.is_object(), "Animation preview arguments must be an object");
    const auto schema = Monstey_AnimationPreviewApiSchema();
    for (const auto &[key, value] : a.as_object())
    {
        (void)value;
        Monstey_AnimationPreviewApiRequire(schema.at("properties").contains(key),
                                           "Unknown animation preview argument " + key);
    }
    const auto source = Monstey_AnimationPreviewApiPath(a, "source");
    const auto skeleton = Monstey_AnimationPreviewApiPath(a, "skeleton");
    const auto animation = Monstey_AnimationPreviewApiPath(a, "animation");
    const auto output = Monstey_AnimationPreviewApiPath(a, "output");
    Monstey_AnimationPreviewApiRequire(a.contains("clip") && a.at("clip").is_string() &&
                                           !a.at("clip").as_string().empty() &&
                                           a.at("clip").as_string().find('\0') == std::string::npos,
                                       "Missing or invalid animation preview clip");
    const auto clip = a.at("clip").as_string();
    Monstey_AnimationPreviewApiRequire(a.contains("tick_ms") && a.at("tick_ms").is_number(),
                                       "Animation preview tick_ms must be a finite number");
    const auto tick = a.at("tick_ms").as_number();
    Monstey_AnimationPreviewApiRequire(std::isfinite(tick), "Animation preview tick_ms must be finite");
    const std::array<std::string, 3> expected{
        Monstey_AnimationPreviewApiExpected(a, "expected_source_sha256"),
        Monstey_AnimationPreviewApiExpected(a, "expected_skeleton_sha256"),
        Monstey_AnimationPreviewApiExpected(a, "expected_animation_sha256")};
    const auto options = Monstey_AnimationPreviewApiOptions(a);
    const std::array<std::filesystem::path, 3> protected_inputs{source, skeleton, animation};
    Monstey_RequireOfflineOutput(output, protected_inputs);
    if (std::filesystem::exists(Monstey_WindowsFilePath(output)))
        throw Monstey_Error("output_exists", "Animation preview output already exists");
    for (const auto &path : protected_inputs)
        Monstey_AnimationPreviewApiRequire(std::filesystem::is_regular_file(Monstey_WindowsFilePath(path)),
                                           "Animation preview input must be a regular file");
    std::array<Monstey_Bytes, 3> bytes;
    std::vector<Monstey_AnimationPreviewApiSnapshot> snapshots;
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        bytes[i] = Monstey_ReadFile(protected_inputs[i]);
        const auto hash = Monstey_HashSha256(bytes[i]);
        if (!expected[i].empty() && expected[i] != hash)
            throw Monstey_Error("source_hash_mismatch", "Animation preview input SHA256 differs: " +
                                                            Monstey_Utf8(protected_inputs[i].wstring()));
        snapshots.push_back(
            {protected_inputs[i], hash, static_cast<std::uint64_t>(bytes[i].size()), !expected[i].empty()});
    }
    Monstey_AnimationPreviewApiUnchanged(snapshots);
    auto source_scene = Monstey_ModelReadSceneBytes(bytes[0], skeleton);
    Monstey_AnimationPreviewApiRequire(source_scene.source_sha256 == snapshots[0].hash,
                                       "Animation preview decoder source identity differs");
    if (source_scene.skeleton_sha256 != snapshots[1].hash)
        throw Monstey_Error("source_changed", "Skeleton changed while decoding animation preview");
    const auto binding =
        Monstey_AnimationPreviewBinding::Prepare(std::move(source_scene), bytes[1], bytes[2], clip);
    for (auto &b : bytes)
        Monstey_Bytes{}.swap(b);
    auto pose = binding.CreateFrame();
    binding.Evaluate(tick, pose);
    auto frame = Monstey_ModelPreviewRender(pose.Scene(), options);
    frame.report["host_bind_geometry_only"] = false;
    frame.report["animation_sample_evaluated"] = true;
    const auto bitmap = Monstey_ModelPreviewBitmap(frame);
    const auto output_hash = Monstey_HashSha256(bitmap);
    auto inputs = J::object();
    constexpr std::array<const char *, 3> names{"source", "skeleton", "animation"};
    for (std::size_t i = 0; i < snapshots.size(); ++i)
        inputs[names[i]] = J::object({{"path", Monstey_Utf8(snapshots[i].path.wstring())},
                                      {"sha256", snapshots[i].hash},
                                      {"bytes", snapshots[i].bytes},
                                      {"expected_hash_guard", snapshots[i].expected}});
    auto result = J::object({{"schema", "Monstey.AnimationPreviewRenderFile/1"},
                             {"operation", "model_animation_preview_render"},
                             {"inputs", inputs},
                             {"clip", clip},
                             {"tick_ms", tick},
                             {"viewport", Monstey_AnimationPreviewApiOptionsJson(options)},
                             {"binding", binding.Inspect()},
                             {"pose", pose.Inspect()},
                             {"preview", frame.report},
                             {"output", Monstey_Utf8(output.wstring())},
                             {"output_format", "BMP"},
                             {"output_sha256", output_hash},
                             {"output_bytes", static_cast<std::uint64_t>(bitmap.size())},
                             {"source_unchanged", true},
                             {"host_ordinary_cpu_geometry", true},
                             {"host_bind_geometry_only", false},
                             {"animation_sample_evaluated", true},
                             {"native_controller_verified", false},
                             {"native_gpu_materials", false},
                             {"native_ik_cloth", false},
                             {"continuous_animation_playback", false}});
    Monstey_AnimationPreviewApiUnchanged(snapshots);
    Monstey_WriteNewFile(output, bitmap);
    Monstey_AnimationPreviewApiVerifyPublished(output, bitmap, snapshots);
    return result;
}
J Monstey_AnimationPreviewCall(std::string_view operation, const J &arguments)
{
    if (operation != "model_animation_preview_render")
        throw Monstey_Error("unknown_operation", "Unknown native preview operation");
    return Monstey_AnimationPreviewRenderFile(arguments);
}
} // namespace Monstey
