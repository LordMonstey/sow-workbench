#include "Monstey_AnimationPreview.hpp"
#include "Monstey_AnimationHierarchy.hpp"
#include "Monstey_SkelDefaultPose.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace Monstey
{
namespace
{
using Monstey_Pose = Monstey_AnimationPreviewPose;
using Monstey_Q = std::array<float, 4>;
using Monstey_V = std::array<float, 3>;
constexpr std::uint32_t Monstey_Pos = 0x00ce66fc, Monstey_Rotation = 0x27436396;
void Monstey_APRequire(bool value, const char *message)
{
    if (!value)
        throw Monstey_Error("animation_preview", message);
}
std::uint32_t Monstey_APHash(const Monstey_Json &value)
{
    std::string_view s = value.as_string();
    if (s.starts_with("0x"))
        s.remove_prefix(2);
    Monstey_APRequire(s.size() == 8, "Expected an eight-digit native hash");
    std::uint32_t result = 0;
    for (char c : s)
    {
        unsigned n = c >= '0' && c <= '9'   ? unsigned(c - '0')
                     : c >= 'a' && c <= 'f' ? unsigned(c - 'a' + 10)
                     : c >= 'A' && c <= 'F' ? unsigned(c - 'A' + 10)
                                            : 16;
        Monstey_APRequire(n < 16, "Invalid native hash digit");
        result = (result << 4) | n;
    }
    return result;
}
// Frozen scalar ordering from the qualified raw SKEL transform helpers.
// No quaternion normalization or matrix inverse is substituted here.
Monstey_V Monstey_APRotate(const Monstey_Q &q, const Monstey_V &t)
{
    const float z = t[2], y = t[1], yx = q[0] * y, qy = q[1], qw = q[3], yy = y * qy;
    const float a = (float)((float)(t[0] * qw) + (float)(qy * z)) - (float)(q[2] * y);
    const float b = (float)((float)(y * qw) - (float)(q[0] * z)) + (float)(q[2] * t[0]);
    const float c = (float)(yx + (float)(qw * z)) - (float)(t[0] * qy);
    const float ny = -qy, nx = -q[0], nz = -q[2];
    const float d = (float)((float)-(float)(q[0] * t[0]) - yy) - (float)(q[2] * z);
    return {(float)((float)((float)(nx * d) + (float)(qw * a)) + (float)(nz * b)) - (float)(ny * c),
            (float)((float)((float)(ny * d) - (float)(nz * a)) + (float)(qw * b)) + (float)(nx * c),
            (float)((float)((float)(nz * d) + (float)(ny * a)) - (float)(nx * b)) + (float)(qw * c)};
}
// Qualified basic-hierarchy F32 arithmetic from 0x1401B173C.
// The existing preview ABI exposes XYZ and quaternion only. Native position W
// is omitted here; 0x14034F540 masks input W before every descendant rotation,
// so it cannot affect the exposed XYZ/quaternion arithmetic.
// Keep the existing sampled-root host policy. Controller motion/reset is separate.
Monstey_Pose Monstey_APCompose(const Monstey_Pose &parent, const Monstey_Pose &child)
{
    Monstey_HierarchyPoseWords parent_words{}, child_words{};
    for (unsigned k = 0; k < 3; ++k)
    {
        parent_words[k] = std::bit_cast<std::uint32_t>(parent.position_cm[k]);
        child_words[k] = std::bit_cast<std::uint32_t>(child.position_cm[k]);
    }
    for (unsigned k = 0; k < 4; ++k)
    {
        parent_words[k + 4] = std::bit_cast<std::uint32_t>(parent.quaternion_xyzw[k]);
        child_words[k + 4] = std::bit_cast<std::uint32_t>(child.quaternion_xyzw[k]);
    }
    const auto native = Monstey_HierarchyComposeF32(parent_words, child_words);
    Monstey_Pose result;
    for (unsigned k = 0; k < 3; ++k)
        result.position_cm[k] = std::bit_cast<float>(native[k]);
    for (unsigned k = 0; k < 4; ++k)
        result.quaternion_xyzw[k] = std::bit_cast<float>(native[k + 4]);
    return result;
}
// Literal selected SIMD shuffle/multiply/add grouping, expressed as scalar F32.
// Multiply(a,b) yields b*a. This grouping is qualified for the bank stage;
// the separate parent/local accumulation above retains its explicit scope.
Monstey_Q Monstey_APBankAdd(const Monstey_Q &a, const Monstey_Q &b)
{
    Monstey_Q result{};
    for (unsigned k = 0; k < 4; ++k)
        result[k] = a[k] + b[k];
    return result;
}
Monstey_Q Monstey_APBankMul(const Monstey_Q &a, const Monstey_Q &b)
{
    Monstey_Q result{};
    for (unsigned k = 0; k < 4; ++k)
        result[k] = a[k] * b[k];
    return result;
}
Monstey_Q Monstey_APBankShuffle(const Monstey_Q &a, unsigned mask)
{
    Monstey_Q result{};
    for (unsigned k = 0; k < 4; ++k)
        result[k] = a[(mask >> (2 * k)) & 3u];
    return result;
}
Monstey_Q Monstey_APBankMultiply(const Monstey_Q &a, const Monstey_Q &b, bool z_first = false)
{
    constexpr Monstey_Q sign1{1, -1, 1, -1}, sign2{1, 1, -1, -1}, sign3{-1, 1, 1, -1};
    const auto reverse = Monstey_APBankShuffle(a, 0x1b), middle = Monstey_APBankShuffle(reverse, 0xb1);
    const auto x = Monstey_APBankMul(sign1, Monstey_APBankMul(Monstey_APBankShuffle(b, 0), reverse));
    const auto y = Monstey_APBankMul(Monstey_APBankMul(Monstey_APBankShuffle(b, 0x55), middle), sign2);
    const auto z = Monstey_APBankMul(
        Monstey_APBankMul(Monstey_APBankShuffle(middle, 0x1b), Monstey_APBankShuffle(b, 0xaa)), sign3);
    const auto w = Monstey_APBankMul(Monstey_APBankShuffle(b, 0xff), a);
    return Monstey_APBankAdd(z_first ? Monstey_APBankAdd(z, y) : Monstey_APBankAdd(y, z),
                             Monstey_APBankAdd(w, x));
}
Monstey_Pose Monstey_APBankCompose(const Monstey_Pose &current, const Monstey_Pose &inverse)
{
    Monstey_Pose result;
    result.quaternion_xyzw = Monstey_APBankMultiply(inverse.quaternion_xyzw, current.quaternion_xyzw, true);
    constexpr Monstey_Q conjugate{-1, -1, -1, 1};
    const Monstey_Q value{inverse.position_cm[0], inverse.position_cm[1], inverse.position_cm[2], 0};
    const auto inner =
        Monstey_APBankMultiply(Monstey_APBankMul(conjugate, current.quaternion_xyzw), value, true);
    const auto rotated = Monstey_APBankMultiply(inner, current.quaternion_xyzw);
    for (unsigned k = 0; k < 3; ++k)
        result.position_cm[k] = rotated[k] + current.position_cm[k];
    return result;
}
void Monstey_APFinite(const Monstey_Pose &pose)
{
    for (float v : pose.position_cm)
        Monstey_APRequire(std::isfinite(v), "Nonfinite evaluated position");
    double norm = 0;
    for (float v : pose.quaternion_xyzw)
    {
        Monstey_APRequire(std::isfinite(v), "Nonfinite evaluated quaternion");
        norm += double(v) * v;
    }
    Monstey_APRequire(std::isfinite(norm) && norm > 1e-24, "Degenerate evaluated quaternion");
}
std::array<double, 16> Monstey_APMatrix(const Monstey_Pose &pose)
{
    const auto x = Monstey_APRotate(pose.quaternion_xyzw, {1, 0, 0});
    const auto y = Monstey_APRotate(pose.quaternion_xyzw, {0, 1, 0});
    const auto z = Monstey_APRotate(pose.quaternion_xyzw, {0, 0, 1});
    return {x[0], y[0], z[0], double(pose.position_cm[0]) / 100,
            x[1], y[1], z[1], double(pose.position_cm[1]) / 100,
            x[2], y[2], z[2], double(pose.position_cm[2]) / 100,
            0,    0,    0,    1};
}
struct Monstey_APDestination
{
    int bone = -1;
    std::uint32_t type = 0, hash = 0;
};
void Monstey_APOverlay(const Monstey_Json &sample, const std::vector<Monstey_APDestination> &destinations,
                       std::vector<Monstey_Pose> &pose)
{
    Monstey_APRequire(sample["ignored_codecs"].empty() && sample["complete_pose_codecs"].as_bool(),
                      "The selected pose contains an unsupported or ignored codec");
    for (const auto &track : sample["tracks"].as_array())
    {
        const auto index = track["channel_index"].as_uint();
        Monstey_APRequire(index < destinations.size(), "Sample channel is outside the prepared shared table");
        const auto &d = destinations[static_cast<std::size_t>(index)];
        Monstey_APRequire(Monstey_APHash(track["type_hash"]) == d.type &&
                              Monstey_APHash(track["bone_hash"]) == d.hash,
                          "Sample identity differs from its prepared shared-table identity");
        if (d.bone < 0 || (d.type != Monstey_Pos && d.type != Monstey_Rotation))
            continue;
        const auto &row = track["values"];
        Monstey_APRequire(row.size() == 1, "Expected exactly one evaluated tick");
        if (row[0].is_null())
            continue; // A segment gap retains the explicit normal default.
        const auto &v = row[0];
        const unsigned width = d.type == Monstey_Pos ? 3u : 4u;
        Monstey_APRequire(v.is_array() && v.size() == width, "Invalid sampled pose component width");
        for (unsigned k = 0; k < width; ++k)
        {
            const auto value = v[k].as_number();
            const auto f = static_cast<float>(value);
            Monstey_APRequire(std::isfinite(value) && std::isfinite(f), "Nonfinite sampled pose component");
            if (d.type == Monstey_Pos)
                pose[static_cast<std::size_t>(d.bone)].position_cm[k] = f;
            else
                pose[static_cast<std::size_t>(d.bone)].quaternion_xyzw[k] = f;
        }
    }
}
Monstey_Pose Monstey_APWords(const std::array<std::uint32_t, 7> &words)
{
    Monstey_Pose p;
    for (unsigned k = 0; k < 3; ++k)
        p.position_cm[k] = std::bit_cast<float>(words[k]);
    for (unsigned k = 0; k < 4; ++k)
        p.quaternion_xyzw[k] = std::bit_cast<float>(words[k + 3]);
    Monstey_APFinite(p);
    return p;
}
struct Monstey_APFrameBuffer
{
    Monstey_ModelScene scene;
    std::vector<Monstey_Pose> local, global, bank;
    double tick = 0;
};
} // namespace
struct Monstey_AnimationPreviewBinding::Monstey_State
{
    Monstey_ModelScene source;
    Monstey_SkelDefaultPose skeleton;
    Monstey_Anix library;
    std::string animation_hash, clip;
    double first_tick = 0, last_tick = 0;
    std::vector<Monstey_APDestination> destinations;
    std::vector<Monstey_Pose> normal_default, inverse;
    std::size_t mapped_position = 0, mapped_rotation = 0, disabled_position = 0;
    Monstey_State(Monstey_ModelScene s, Monstey_SkelDefaultPose sk, std::span<const std::uint8_t> anix)
        : source(std::move(s)), skeleton(std::move(sk)), library(anix),
          animation_hash(Monstey_HashSha256(anix))
    {
    }
};
struct Monstey_AnimationPreviewFrame::Monstey_State
{
    std::shared_ptr<const Monstey_AnimationPreviewBinding::Monstey_State> owner;
    std::array<Monstey_APFrameBuffer, 2> buffers;
    unsigned published = 0;
    std::uint64_t revision = 0;
};
Monstey_AnimationPreviewFrame::Monstey_AnimationPreviewFrame(std::unique_ptr<Monstey_State> s)
    : state_(std::move(s))
{
}
Monstey_AnimationPreviewFrame::Monstey_AnimationPreviewFrame(Monstey_AnimationPreviewFrame &&) noexcept =
    default;
Monstey_AnimationPreviewFrame &
Monstey_AnimationPreviewFrame::operator=(Monstey_AnimationPreviewFrame &&) noexcept = default;
Monstey_AnimationPreviewFrame::~Monstey_AnimationPreviewFrame() = default;
const Monstey_ModelScene &Monstey_AnimationPreviewFrame::Scene() const
{
    Monstey_APRequire(bool(state_), "Moved-from preview frame");
    return state_->buffers[state_->published].scene;
}
std::span<const Monstey_Pose> Monstey_AnimationPreviewFrame::LocalPose() const
{
    (void)Scene();
    return state_->buffers[state_->published].local;
}
std::span<const Monstey_Pose> Monstey_AnimationPreviewFrame::GlobalPose() const
{
    (void)Scene();
    return state_->buffers[state_->published].global;
}
std::span<const Monstey_Pose> Monstey_AnimationPreviewFrame::SkinTransforms() const
{
    (void)Scene();
    return state_->buffers[state_->published].bank;
}
double Monstey_AnimationPreviewFrame::Tick() const
{
    (void)Scene();
    return state_->buffers[state_->published].tick;
}
std::uint64_t Monstey_AnimationPreviewFrame::Revision() const
{
    (void)Scene();
    return state_->revision;
}
Monstey_Json Monstey_AnimationPreviewFrame::Inspect() const
{
    (void)Scene();
    return Monstey_Json::object({{"tick_ms", Tick()},
                                 {"revision", Revision()},
                                 {"cpu_geometry_only", true},
                                 {"native_gpu_materials", false},
                                 {"native_ik_cloth", false},
                                 {"directions_deformed", false},
                                 {"root_motion_policy", "sampled_local_pose_only_no_actor_motion"}});
}
Monstey_AnimationPreviewBinding::Monstey_AnimationPreviewBinding(std::shared_ptr<const Monstey_State> s)
    : state_(std::move(s))
{
}
Monstey_AnimationPreviewBinding Monstey_AnimationPreviewBinding::Prepare(Monstey_ModelScene source,
                                                                         std::span<const std::uint8_t> skel,
                                                                         std::span<const std::uint8_t> anix,
                                                                         std::string clip)
{
    Monstey_APRequire(!skel.empty() && !source.skeleton_sha256.empty() &&
                          Monstey_HashSha256(skel) == source.skeleton_sha256,
                      "An explicit matching SKEL snapshot is required");
    Monstey_APRequire(!source.native_baseline.empty() &&
                          Monstey_HashSha256(source.native_baseline) == source.baseline_sha256 &&
                          source.source_sha256 == source.baseline_sha256,
                      "Model native baseline identity mismatch");
    auto skeleton = Monstey_SkelDefaultPoseRead(skel);
    const auto count = skeleton.nodes.size();
    Monstey_APRequire(source.bones.size() == count && source.hk_to_source == skeleton.hk_to_source &&
                          source.source_to_hk == skeleton.source_to_hk,
                      "Model/SKEL source-to-runtime permutation mismatch");
    std::map<std::uint32_t, std::size_t> hashes;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto &a = source.bones[i];
        const auto &b = skeleton.nodes[i];
        Monstey_APRequire(a.hk_index == i && a.source_index == b.source_index && a.parent_hk == b.parent_hk &&
                              a.name == b.name && a.native_id == b.name_hash &&
                              a.native_id == Monstey_NativeHash(b.name),
                          "Model/SKEL exact native identity or hierarchy mismatch");
        Monstey_APRequire(hashes.emplace(b.name_hash, i).second, "Duplicate SKEL name hash is ambiguous");
    }
    Monstey_APRequire(!source.parts.empty(), "Animation preview requires model geometry");
    std::size_t total_vertices = 0;
    for (const auto &p : source.parts)
    {
        Monstey_APRequire(p.channels.contains("POSITION") && p.channels.contains("JOINTS") &&
                              p.channels.contains("WEIGHTS_U8"),
                          "Animated geometry requires positions and native skin channels");
        const auto &pos = p.channels.at("POSITION"), &joints = p.channels.at("JOINTS"),
                   &weights = p.channels.at("WEIGHTS_U8");
        Monstey_APRequire(pos.width == 3 && !pos.values.empty() && pos.values.size() % 3 == 0,
                          "Invalid source position channel");
        const auto vertices = pos.values.size() / 3;
        total_vertices += vertices;
        Monstey_APRequire(total_vertices <= 16000000, "Animated vertex budget exceeded");
        Monstey_APRequire(joints.width == 4 && weights.width == 4 && joints.values.size() == vertices * 4 &&
                              weights.values.size() == vertices * 4,
                          "Invalid native skin channel extents");
        for (const auto &[name, c] : p.channels)
        {
            Monstey_APRequire(c.width > 0 && c.values.size() == vertices * c.width,
                              "Mismatched source channel extent");
            if (name == "POSITION" || name == "JOINTS" || name == "WEIGHTS_U8")
                for (double v : c.values)
                    Monstey_APRequire(std::isfinite(v), "Nonfinite source position or skin value");
            if (name == "NORMAL" || name == "TANGENT" || name == "BITANGENT")
                Monstey_APRequire(c.width == 3 || c.width == 4, "Invalid direction channel width");
        }
        for (std::size_t v = 0; v < vertices; ++v)
        {
            double sum = 0;
            for (unsigned k = 0; k < 4; ++k)
            {
                const auto w = weights.values[v * 4 + k], j = joints.values[v * 4 + k];
                Monstey_APRequire(w >= 0 && w <= 255 && std::floor(w) == w && j >= 0 && j <= 255 &&
                                      std::floor(j) == j,
                                  "Skin lanes must contain native U8 values");
                if (w > 0)
                    Monstey_APRequire(j < count, "Nonzero-weight joint is outside the explicit skeleton");
                sum += w;
            }
            Monstey_APRequire(sum > 0, "Vertex has no nonzero skin influence");
        }
        Monstey_APRequire(p.indices.size() % 3 == 0, "Invalid source triangle topology");
        for (auto index : p.indices)
            Monstey_APRequire(index < vertices, "Source index exceeds vertex count");
    }
    auto state = std::make_shared<Monstey_State>(std::move(source), std::move(skeleton), anix);
    state->clip = std::move(clip);
    const auto description = state->library.Describe();
    const auto &channels = description["channels"];
    std::set<std::pair<std::uint32_t, std::uint32_t>> unique;
    for (std::size_t i = 0; i < channels.size(); ++i)
    {
        const auto &row = channels[i];
        Monstey_APRequire(row["index"].as_uint() == i, "Shared ANIX ordinal mismatch");
        const auto type = Monstey_APHash(row["type_hash"]), hash = Monstey_APHash(row["bone_hash"]);
        Monstey_APRequire(unique.emplace(type, hash).second,
                          "Duplicate shared channel identity is ambiguous");
        if (i)
            Monstey_APRequire(std::pair(state->destinations.back().type, state->destinations.back().hash) <
                                  std::pair(type, hash),
                              "Shared channels are not strictly sorted by unsigned type and bone hash");
        Monstey_APDestination d{-1, type, hash};
        const auto bone = hashes.find(hash);
        if (bone != hashes.end())
        {
            if (type == Monstey_Rotation)
            {
                d.bone = static_cast<int>(bone->second);
                ++state->mapped_rotation;
            }
            else if (type == Monstey_Pos)
            {
                if ((state->skeleton.nodes[bone->second].raw_flag_byte & 1u) == 0)
                {
                    d.bone = static_cast<int>(bone->second);
                    ++state->mapped_position;
                }
                else
                    ++state->disabled_position;
            }
        }
        state->destinations.push_back(d);
    }
    const Monstey_Json *bind = nullptr, *selected = nullptr;
    for (const auto &row : description["clips"].as_array())
    {
        if (row["name"].as_string() == "bind")
            bind = &row;
        if (row["name"].as_string() == state->clip)
            selected = &row;
    }
    Monstey_APRequire(bind && selected, "The selected ordinary clip and explicit bind clip must exist");
    for (const auto *row : {bind, selected})
    {
        Monstey_APRequire((*row)["flags"].as_uint() == 0, "Only ordinary flags=0 clips are qualified");
        Monstey_APRequire(!(*row)["segments"].empty(), "A structural-only clip cannot be previewed");
    }
    state->first_tick = (*selected)["segments"][0]["first_tick"].as_number();
    state->last_tick = (*selected)["segments"][(*selected)["segments"].size() - 1]["last_tick"].as_number();
    for (const auto &node : state->skeleton.nodes)
    {
        state->normal_default.push_back(Monstey_APWords(node.local_words));
        state->inverse.push_back(Monstey_APWords(node.inverse_words));
    }
    const std::array<double, 1> bind_tick{(*bind)["segments"][0]["first_tick"].as_number()};
    Monstey_APOverlay(state->library.Sample("bind", bind_tick, false), state->destinations,
                      state->normal_default);
    for (const auto &p : state->normal_default)
        Monstey_APFinite(p);
    // Probe selected clip support before returning an immutable prepared binding.
    const std::array<double, 1> first{state->first_tick};
    auto probe = state->normal_default;
    Monstey_APOverlay(state->library.Sample(state->clip, first, false), state->destinations, probe);
    return Monstey_AnimationPreviewBinding(std::move(state));
}
Monstey_AnimationPreviewFrame Monstey_AnimationPreviewBinding::CreateFrame() const
{
    Monstey_APRequire(bool(state_), "Moved-from animation binding");
    auto frame = std::make_unique<Monstey_AnimationPreviewFrame::Monstey_State>();
    frame->owner = state_;
    for (auto &buffer : frame->buffers)
    {
        buffer.scene = state_->source;
        buffer.local.resize(state_->skeleton.nodes.size());
        buffer.global.resize(state_->skeleton.nodes.size());
        buffer.bank.resize(state_->skeleton.nodes.size());
    }
    Monstey_AnimationPreviewFrame result(std::move(frame));
    Evaluate(state_->first_tick, result);
    return result;
}
void Monstey_AnimationPreviewBinding::Evaluate(double milliseconds,
                                               Monstey_AnimationPreviewFrame &frame) const
{
    Monstey_APRequire(bool(state_) && bool(frame.state_) && frame.state_->owner == state_,
                      "Frame does not belong to this immutable animation binding");
    Monstey_APRequire(std::isfinite(milliseconds) && milliseconds >= state_->first_tick &&
                          milliseconds <= state_->last_tick,
                      "Preview tick must be finite and inside the selected clip range");
    const auto candidate = frame.state_->published ^ 1u;
    auto &buffer = frame.state_->buffers[candidate];
    std::copy(state_->normal_default.begin(), state_->normal_default.end(), buffer.local.begin());
    const std::array<double, 1> tick{milliseconds};
    Monstey_APOverlay(state_->library.Sample(state_->clip, tick, false), state_->destinations, buffer.local);
    for (std::size_t i = 0; i < buffer.local.size(); ++i)
    {
        Monstey_APFinite(buffer.local[i]);
        const auto parent = state_->skeleton.nodes[i].parent_hk;
        buffer.global[i] =
            parent < 0 ? buffer.local[i]
                       : Monstey_APCompose(buffer.global[static_cast<std::size_t>(parent)], buffer.local[i]);
        Monstey_APFinite(buffer.global[i]);
        buffer.bank[i] = Monstey_APBankCompose(buffer.global[i], state_->inverse[i]);
        Monstey_APFinite(buffer.bank[i]);
        buffer.scene.bones[i].global_bind_metres = Monstey_APMatrix(buffer.global[i]);
    }
    for (std::size_t pi = 0; pi < state_->source.parts.size(); ++pi)
    {
        const auto &source = state_->source.parts[pi];
        auto &part = buffer.scene.parts[pi];
        const auto &joints = source.channels.at("JOINTS").values,
                   &weights = source.channels.at("WEIGHTS_U8").values;
        const auto vertices = source.channels.at("POSITION").values.size() / 3;
        const auto &in = source.channels.at("POSITION");
        auto &out = part.channels.at("POSITION");
        for (std::size_t v = 0; v < vertices; ++v)
        {
            Monstey_V input{};
            for (unsigned k = 0; k < 3; ++k)
            {
                input[k] = static_cast<float>(in.values[v * 3 + k]);
                Monstey_APRequire(std::isfinite(input[k]), "Source position exceeds finite F32 range");
            }
            std::array<double, 3> sum{};
            double weight_sum = 0;
            for (unsigned lane = 0; lane < 4; ++lane)
            {
                const auto w = weights[v * 4 + lane];
                if (w == 0)
                    continue;
                const auto &bank = buffer.bank[static_cast<std::size_t>(joints[v * 4 + lane])];
                const auto transformed = Monstey_APRotate(bank.quaternion_xyzw, input);
                for (unsigned k = 0; k < 3; ++k)
                    sum[k] += w * (double(transformed[k]) + bank.position_cm[k]);
                weight_sum += w;
            }
            for (unsigned k = 0; k < 3; ++k)
            {
                const auto value = sum[k] / weight_sum;
                Monstey_APRequire(std::isfinite(value) && std::abs(value) <= 1e38,
                                  "Skinning produced a nonfinite or unbounded vertex");
                out.values[v * 3 + k] = value;
            }
        }
        // NORMAL/TANGENT/BITANGENT storage is copied exactly. Some native rows
        // contain opaque/nonfinite words; their shader interpretation is unqualified.
    }
    buffer.tick = milliseconds;
    frame.state_->published = candidate;
    ++frame.state_->revision;
}
const Monstey_ModelScene &Monstey_AnimationPreviewBinding::Source() const
{
    Monstey_APRequire(bool(state_), "Moved-from animation binding");
    return state_->source;
}
Monstey_Json Monstey_AnimationPreviewBinding::Inspect() const
{
    (void)Source();
    return Monstey_Json::object(
        {{"format", "ordinary_same_rig_animation_preview"},
         {"model_sha256", state_->source.source_sha256},
         {"skel_sha256", state_->skeleton.source_sha256},
         {"anix_sha256", state_->animation_hash},
         {"clip", state_->clip},
         {"first_tick_ms", state_->first_tick},
         {"last_tick_ms", state_->last_tick},
         {"bones", static_cast<std::uint64_t>(state_->skeleton.nodes.size())},
         {"mapped_shared_position", static_cast<std::uint64_t>(state_->mapped_position)},
         {"mapped_shared_rotation", static_cast<std::uint64_t>(state_->mapped_rotation)},
         {"disabled_shared_position", static_cast<std::uint64_t>(state_->disabled_position)},
         {"same_rig_exact_identity", true},
         {"cpu_geometry_only", true},
         {"native_gpu_materials", false},
         {"native_ik_cloth", false},
         {"directions_deformed", false},
         {"retargeting", false},
         {"additive_policy", false},
         {"root_motion_policy", "sampled_local_pose_only_no_actor_motion"},
         {"direction_policy", "native_direction_words_preserved_unchanged"},
         {"skin_bank_policy", "selected_183B90_and_34F540_F32_grouping"},
         {"global_hierarchy_native_fp_order_verified", false}});
}
} // namespace Monstey
