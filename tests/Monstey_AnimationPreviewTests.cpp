#include "Monstey_AnimationPreview.hpp"
#include "Monstey_ModelPreview.hpp"
#include "Monstey_SkelDefaultPose.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <set>
using namespace Monstey;
namespace
{
using J = Monstey_Json;
using B = Monstey_Bytes;
using P = Monstey_AnimationPreviewPose;
void Monstey_Test(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void Monstey_Reject(const std::function<void()> &f)
{
    bool caught = false;
    try
    {
        f();
    }
    catch (const Monstey_Error &)
    {
        caught = true;
    }
    Monstey_Test(caught, "Expected rejection did not occur");
}
void Monstey_AddFloat(B &b, float v) { Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(v)); }
B Monstey_FixtureSkel(bool noncommuting = false)
{
    const std::string names("Root\0Arm\0Finger\0Leg\0", 20);
    B b(96);
    Monstey_PutU32(b, 0, 0x4c454b53);
    Monstey_PutU32(b, 4, 17);
    Monstey_PutU32(b, 12, static_cast<std::uint32_t>(names.size()));
    Monstey_PutU32(b, 40, 4);
    Monstey_AppendU32(b, 0xdcdcff00);
    b.insert(b.end(), names.begin(), names.end());
    const unsigned offsets[]{0, 5, 9, 16}, children[]{2, 1, 0, 0}, flags[]{0, 1, 2, 3};
    const float translations[][3]{{0, 0, 0}, {0, 10, 0}, {3, 15, 2}, {-4, 0, 0}};
    const float c = std::sqrt(0.5f);
    const float rotations[][4]{
        {0, 0, c, c}, {0.5f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f}, {0, 0, c, c}};
    for (unsigned i = 0; i < 4; ++i)
    {
        Monstey_AppendU32(b, offsets[i]);
        b.push_back(static_cast<std::uint8_t>(flags[i]));
        for (float v : translations[i])
            Monstey_AddFloat(b, v);
        if (noncommuting)
            for (float v : rotations[i])
                Monstey_AddFloat(b, v);
        else
            for (float v : {0.f, 0.f, 0.f, 1.f})
                Monstey_AddFloat(b, v);
        Monstey_AppendU32(b, children[i]);
    }
    return b;
}
Monstey_ModelScene Monstey_FixtureScene(const B &skel)
{
    const auto raw = Monstey_SkelDefaultPoseRead(skel);
    Monstey_ModelScene s;
    s.native_baseline = {1, 2, 3, 4};
    s.source_sha256 = s.baseline_sha256 = Monstey_HashSha256(s.native_baseline);
    s.skeleton_sha256 = Monstey_HashSha256(skel);
    s.hk_to_source = raw.hk_to_source;
    s.source_to_hk = raw.source_to_hk;
    for (const auto &n : raw.nodes)
    {
        std::array<double, 16> matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        for (unsigned k = 0; k < 3; ++k)
            matrix[k * 4 + 3] = double(std::bit_cast<float>(n.global_words[k])) / 100;
        s.bones.push_back({n.hk_index, n.source_index, n.parent_hk, n.name_hash, n.name, matrix});
    }
    Monstey_ModelPart p;
    p.container = 0;
    p.part = 0;
    p.indices = {0, 1, 2};
    p.triangle_bands = {7};
    p.triangle_band_topology_sha256 = "synthetic";
    p.channels["POSITION"] = {3, {0, 10, 0, 3, 15, 2, -4, 0, 0}};
    p.channels["JOINTS"] = {4, {1, 255, 255, 255, 3, 1, 255, 255, 2, 255, 255, 255}};
    p.channels["WEIGHTS_U8"] = {4, {255, 0, 0, 0, 128, 127, 0, 0, 255, 0, 0, 0}};
    p.channels["NORMAL"] = {3, {0, 0, 1, 0, 0, 1, 0, 0, 1}};
    p.channels["TANGENT"] = {4, {1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, -1}};
    p.channels["BITANGENT"] = {3, {0, 1, 0, 0, 1, 0, 0, 1, 0}};
    p.channels["UV0"] = {2, {-0.0, 0.5, 0.25, 1, 0.75, 0}};
    p.channels["COLOR0"] = {4, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}};
    s.parts.push_back(std::move(p));
    return s;
}
J Monstey_FixtureTrack(unsigned channel, J a, J b)
{
    return J::object({{"channel_index", channel}, {"values", J::array({a, b})}});
}
B Monstey_FixtureLibrary(const B &skel, bool moving = true)
{
    const auto raw = Monstey_SkelDefaultPoseRead(skel);
    J channels = J::array(), bind = J::array(), walk = J::array();
    // Deliberately declare unsorted/interleaved identities. Create remaps them.
    for (std::size_t i = 0; i < raw.nodes.size(); ++i)
    {
        const auto &n = raw.nodes[i];
        channels.as_array().push_back(
            J::object({{"type_hash", "00ce66fc"}, {"bone_hash", Monstey_Hex(n.name_hash)}}));
        channels.as_array().push_back(
            J::object({{"type_hash", "27436396"}, {"bone_hash", Monstey_Hex(n.name_hash)}}));
        auto p = J::array(), q = J::array();
        for (unsigned k = 0; k < 3; ++k)
            p.as_array().emplace_back(double(std::bit_cast<float>(n.local_words[k])));
        for (unsigned k = 0; k < 4; ++k)
            q.as_array().emplace_back(double(std::bit_cast<float>(n.local_words[k + 3])));
        // Root/Finger enabled positions; Arm/Leg forbidden positions are poison.
        auto bindp = p;
        if (n.raw_flag_byte & 1u)
            bindp = J::array({9999, 8888, 7777});
        bind.as_array().push_back(Monstey_FixtureTrack(static_cast<unsigned>(2 * i), bindp, bindp));
        bind.as_array().push_back(Monstey_FixtureTrack(static_cast<unsigned>(2 * i + 1), q, q));
        if (moving && n.name == "Arm")
            walk.as_array().push_back(
                Monstey_FixtureTrack(static_cast<unsigned>(2 * i + 1), q, J::array({0, 0, 1, 0})));
        if (moving && n.name == "Finger")
        {
            auto end = p;
            end[0] = p[0].as_number() + 5;
            walk.as_array().push_back(Monstey_FixtureTrack(static_cast<unsigned>(2 * i), p, end));
        }
    }
    // Unmapped identity and ordinary missing tracks are intentional.
    channels.as_array().push_back(
        J::object({{"type_hash", "00ce66fc"}, {"bone_hash", Monstey_Hex(Monstey_NativeHash("Absent"))}}));
    walk.as_array().push_back(Monstey_FixtureTrack(static_cast<unsigned>(channels.size() - 1),
                                                   J::array({100, 200, 300}), J::array({400, 500, 600})));
    if (!moving)
        walk = bind;
    auto clip = [](const char *name, J tracks)
    {
        return J::object({{"name", name},
                          {"duration_ticks", 1000},
                          {"ticks", J::array({0, 1000})},
                          {"tracks", std::move(tracks)},
                          {"events", J::array()}});
    };
    return Monstey_Anix::Create(J::object({{"library_path", "synthetic/preview.anix"},
                                           {"channels", channels},
                                           {"clips", J::array({clip("bind", bind), clip("walk", walk)})}}))
        .Build();
}
std::string Monstey_FrameHash(const Monstey_AnimationPreviewFrame &f)
{
    B words;
    for (const auto &p : f.Scene().parts)
        for (const auto &[name, c] : p.channels)
        {
            for (char x : name)
                words.push_back(static_cast<std::uint8_t>(x));
            for (double v : c.values)
                Monstey_AppendU64(words, std::bit_cast<std::uint64_t>(v));
        }
    for (const auto &p : f.LocalPose())
    {
        for (float v : p.position_cm)
            Monstey_AddFloat(words, v);
        for (float v : p.quaternion_xyzw)
            Monstey_AddFloat(words, v);
    }
    for (const auto &p : f.GlobalPose())
    {
        for (float v : p.position_cm)
            Monstey_AddFloat(words, v);
        for (float v : p.quaternion_xyzw)
            Monstey_AddFloat(words, v);
    }
    return Monstey_HashSha256(words);
}
void Monstey_CheckUnrelated(const Monstey_ModelScene &baseline, const Monstey_ModelScene &frame)
{
    Monstey_Test(baseline.native_baseline == frame.native_baseline, "Native baseline changed");
    Monstey_Test(baseline.hk_to_source == frame.hk_to_source && baseline.source_to_hk == frame.source_to_hk,
                 "Permutation changed");
    for (std::size_t i = 0; i < baseline.parts.size(); ++i)
    {
        const auto &a = baseline.parts[i], &b = frame.parts[i];
        Monstey_Test(a.indices == b.indices && a.triangle_bands == b.triangle_bands &&
                         a.triangle_band_topology_sha256 == b.triangle_band_topology_sha256,
                     "Topology/band metadata changed");
        for (const auto &[name, c] : a.channels)
        {
            const auto &out = b.channels.at(name);
            Monstey_Test(c.width == out.width && c.values.size() == out.values.size(),
                         "Channel shape changed");
            if (name == "POSITION")
            {
                if (c.width == 4)
                    for (std::size_t v = 3; v < c.values.size(); v += 4)
                        Monstey_Test(std::bit_cast<std::uint64_t>(c.values[v]) ==
                                         std::bit_cast<std::uint64_t>(out.values[v]),
                                     "Fourth direction word changed");
            }
            else
                for (std::size_t v = 0; v < c.values.size(); ++v)
                    Monstey_Test(std::bit_cast<std::uint64_t>(c.values[v]) ==
                                     std::bit_cast<std::uint64_t>(out.values[v]),
                                 "Unrelated channel word changed");
        }
    }
}
std::array<double, 4> Monstey_OracleMultiply(std::array<double, 4> p, std::array<double, 4> c)
{
    return {p[3] * c[0] + p[0] * c[3] + p[1] * c[2] - p[2] * c[1],
            p[3] * c[1] - p[0] * c[2] + p[1] * c[3] + p[2] * c[0],
            p[3] * c[2] + p[0] * c[1] - p[1] * c[0] + p[2] * c[3],
            p[3] * c[3] - p[0] * c[0] - p[1] * c[1] - p[2] * c[2]};
}
std::array<double, 3> Monstey_OracleRotate(std::array<double, 4> q, std::array<double, 3> p)
{
    const auto temp = Monstey_OracleMultiply(q, {p[0], p[1], p[2], 0});
    const auto v = Monstey_OracleMultiply(temp, {-q[0], -q[1], -q[2], q[3]});
    return {v[0], v[1], v[2]};
}
void Monstey_OracleGlobal(const Monstey_AnimationPreviewFrame &f)
{
    std::vector<std::array<double, 4>> q(f.LocalPose().size());
    std::vector<std::array<double, 3>> t(q.size());
    for (std::size_t i = 0; i < q.size(); ++i)
    {
        const auto &p = f.LocalPose()[i];
        for (unsigned k = 0; k < 3; ++k)
            t[i][k] = p.position_cm[k];
        for (unsigned k = 0; k < 4; ++k)
            q[i][k] = p.quaternion_xyzw[k];
        const auto parent = f.Scene().bones[i].parent_hk;
        if (parent >= 0)
        {
            const auto pi = static_cast<std::size_t>(parent);
            t[i] = Monstey_OracleRotate(q[pi], t[i]);
            for (unsigned k = 0; k < 3; ++k)
                t[i][k] += t[pi][k];
            q[i] = Monstey_OracleMultiply(q[pi], q[i]);
        }
        for (unsigned k = 0; k < 3; ++k)
            Monstey_Test(std::abs(t[i][k] - f.GlobalPose()[i].position_cm[k]) < 0.001,
                         "Parent/local position disagrees with independent Hamilton oracle");
        for (unsigned k = 0; k < 4; ++k)
            Monstey_Test(std::abs(q[i][k] - f.GlobalPose()[i].quaternion_xyzw[k]) < 0.00001,
                         "Parent/local rotation disagrees with independent Hamilton oracle");
    }
}
J Monstey_SyntheticTests()
{
    unsigned groups = 0;
    auto skel = Monstey_FixtureSkel();
    auto baseline = Monstey_FixtureScene(skel);
    auto bytes = Monstey_FixtureLibrary(skel);
    const auto bytes_hash = Monstey_HashSha256(bytes), skel_hash = Monstey_HashSha256(skel);
    const auto binding = Monstey_AnimationPreviewBinding::Prepare(baseline, skel, bytes, "walk");
    const auto report = binding.Inspect();
    Monstey_Test(baseline.hk_to_source == std::vector<std::size_t>({0, 1, 3, 2}),
                 "Fixture is not a nontrivial sibling-block permutation");
    Monstey_Test(report["mapped_shared_position"].as_uint() == 2 &&
                     report["mapped_shared_rotation"].as_uint() == 4 &&
                     report["disabled_shared_position"].as_uint() == 2,
                 "Raw 0/1/2/3 flag polarity differs");
    ++groups;
    auto f = binding.CreateFrame();
    Monstey_Test(f.LocalPose()[1].position_cm == std::array<float, 3>({0, 10, 0}) &&
                     f.LocalPose()[2].position_cm == std::array<float, 3>({-4, 0, 0}),
                 "Disabled Pos poisoned normal defaults");
    ++groups;
    for (std::size_t i = 0; i < baseline.parts[0].channels.at("POSITION").values.size(); ++i)
        Monstey_Test(std::abs(baseline.parts[0].channels.at("POSITION").values[i] -
                              f.Scene().parts[0].channels.at("POSITION").values[i]) < 0.001,
                     "Synthetic bind invariant failed");
    ++groups;
    Monstey_CheckUnrelated(baseline, f.Scene());
    ++groups;
    const auto start = Monstey_FrameHash(f);
    std::set<std::string> moving;
    const double *storage0 = f.Scene().parts[0].channels.at("POSITION").values.data();
    const double *storage1 = nullptr;
    for (unsigned i = 1; i <= 20; ++i)
    {
        binding.Evaluate(i * 50, f);
        moving.insert(Monstey_FrameHash(f));
        Monstey_CheckUnrelated(baseline, f.Scene());
        Monstey_OracleGlobal(f);
        const auto *storage = f.Scene().parts[0].channels.at("POSITION").values.data();
        if (!storage1 && storage != storage0)
            storage1 = storage;
        Monstey_Test(storage == storage0 || storage == storage1, "Per-frame vertex storage was reallocated");
    }
    Monstey_Test(moving.size() > 15 && !moving.contains(start),
                 "Moving clip does not produce distinct poses");
    ++groups;
    Monstey_CheckUnrelated(baseline, f.Scene());
    Monstey_Test(!binding.Inspect()["directions_deformed"].as_bool(),
                 "Opaque direction storage was represented as deformed normals");
    ++groups;
    const auto stable = Monstey_FrameHash(f);
    const auto revision = f.Revision();
    const auto tick = f.Tick();
    for (double invalid :
         {-1.0, 1001.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
        Monstey_Reject([&] { binding.Evaluate(invalid, f); });
        Monstey_Test(Monstey_FrameHash(f) == stable && f.Revision() == revision && f.Tick() == tick,
                     "Invalid tick changed published output");
    }
    ++groups;
    const auto other = Monstey_AnimationPreviewBinding::Prepare(baseline, skel, bytes, "walk");
    Monstey_Reject([&] { other.Evaluate(0, f); });
    Monstey_Test(Monstey_FrameHash(f) == stable, "Foreign binding changed frame");
    ++groups;
    auto bad = baseline;
    bad.skeleton_sha256 = "bad";
    Monstey_Reject([&] { (void)Monstey_AnimationPreviewBinding::Prepare(bad, skel, bytes, "walk"); });
    ++groups;
    bad = baseline;
    bad.bones[1].native_id ^= 1;
    Monstey_Reject([&] { (void)Monstey_AnimationPreviewBinding::Prepare(bad, skel, bytes, "walk"); });
    ++groups;
    bad = baseline;
    std::swap(bad.hk_to_source[1], bad.hk_to_source[2]);
    Monstey_Reject([&] { (void)Monstey_AnimationPreviewBinding::Prepare(bad, skel, bytes, "walk"); });
    ++groups;
    auto duplicate = skel;
    const auto at = 100 + 20;
    Monstey_PutU32(duplicate, at + 37, 0);
    bad = Monstey_FixtureScene(duplicate);
    Monstey_Reject([&] { (void)Monstey_AnimationPreviewBinding::Prepare(bad, duplicate, bytes, "walk"); });
    ++groups;
    bad = baseline;
    bad.parts[0].channels.at("WEIGHTS_U8").values[0] = 0;
    Monstey_Reject([&] { (void)Monstey_AnimationPreviewBinding::Prepare(bad, skel, bytes, "walk"); });
    ++groups;
    bad = baseline;
    bad.parts[0].channels.at("JOINTS").values[0] = 255;
    Monstey_Reject([&] { (void)Monstey_AnimationPreviewBinding::Prepare(bad, skel, bytes, "walk"); });
    ++groups;
    bad = baseline;
    bad.parts[0].channels.at("POSITION").values[0] = std::numeric_limits<double>::infinity();
    Monstey_Reject([&] { (void)Monstey_AnimationPreviewBinding::Prepare(bad, skel, bytes, "walk"); });
    ++groups;
    auto flagged = bytes;
    const auto meta = Monstey_U32(flagged, 32);
    const auto descriptor = meta + Monstey_U32(flagged, meta + 20 + 8);
    Monstey_PutU16(flagged, descriptor + 50, 1); // Descriptor flag word is qualified at +50.
    bool flag_changed = false;
    try
    {
        Monstey_Anix l(flagged);
        const auto flagged_description = l.Describe();
        for (const auto &c : flagged_description["clips"].as_array())
            if (c["flags"].as_uint() != 0)
                flag_changed = true;
    }
    catch (const Monstey_Error &)
    {
    }
    Monstey_Test(flag_changed, "Synthetic clip flag mutation failed");
    {
        Monstey_Reject([&]
                       { (void)Monstey_AnimationPreviewBinding::Prepare(baseline, skel, flagged, "walk"); });
        ++groups;
    }
    auto duplicate_channels = bytes;
    const auto shared = Monstey_U32(duplicate_channels, 20);
    Monstey_Test(Monstey_U32(duplicate_channels, shared + 4) >= 2,
                 "Shared duplicate fixture needs two entries");
    Monstey_PutU32(duplicate_channels, shared + 12, Monstey_U32(duplicate_channels, shared + 8));
    Monstey_Reject(
        [&] { (void)Monstey_AnimationPreviewBinding::Prepare(baseline, skel, duplicate_channels, "walk"); });
    ++groups;
    auto unknown = bytes;
    const auto ud = Monstey_U32(unknown, 32) + Monstey_U32(unknown, Monstey_U32(unknown, 32) + 28);
    unknown[ud + 124] = 'X';
    unknown[ud + 125] = 'X';
    unknown[ud + 126] = 'X';
    unknown[ud + 127] = 'X';
    Monstey_Reject([&] { (void)Monstey_AnimationPreviewBinding::Prepare(baseline, skel, unknown, "walk"); });
    ++groups;
    auto zero_scene = Monstey_Anix(bytes).Export();
    bool changed_zero = false;
    for (auto &c : zero_scene["clips"].as_array())
        if (c["name"].as_string() == "walk")
            for (auto &t : c["tracks"].as_array())
                if (t["type_hash"].as_string() == Monstey_Hex(0x27436396) &&
                    t["bone_hash"].as_string() == Monstey_Hex(Monstey_NativeHash("Arm")))
                {
                    t["native_codec"] = "UAQT";
                    t["values"][t["values"].size() - 1] = J::array({0, 0, 0, 0});
                    changed_zero = true;
                }
    Monstey_Test(changed_zero, "Missing synthetic raw quaternion track");
    const auto zero_bytes = Monstey_Anix::Create(zero_scene).Build();
    const auto zero_binding = Monstey_AnimationPreviewBinding::Prepare(baseline, skel, zero_bytes, "walk");
    auto zf = zero_binding.CreateFrame();
    const auto zh = Monstey_FrameHash(zf);
    const auto zr = zf.Revision();
    Monstey_Reject([&] { zero_binding.Evaluate(1000, zf); });
    Monstey_Test(Monstey_FrameHash(zf) == zh && zf.Revision() == zr && zf.Tick() == 0,
                 "Failed active evaluation changed published buffers");
    zero_binding.Evaluate(0, zf);
    Monstey_Test(Monstey_FrameHash(zf) == zh, "Evaluation did not recover after rejected candidate");
    ++groups;
    const auto noncommuting = Monstey_FixtureSkel(true);
    const auto ncscene = Monstey_FixtureScene(noncommuting);
    const auto ncbytes = Monstey_FixtureLibrary(noncommuting);
    const auto nc = Monstey_AnimationPreviewBinding::Prepare(ncscene, noncommuting, ncbytes, "walk");
    auto nf = nc.CreateFrame();
    for (double t : {0.0, 250.0, 500.0, 750.0, 1000.0})
    {
        nc.Evaluate(t, nf);
        Monstey_OracleGlobal(nf);
    }
    ++groups;
    auto sparse_scene = Monstey_Anix(ncbytes).Export();
    for (auto &c : sparse_scene["clips"].as_array())
        if (c["name"].as_string() == "bind")
            std::erase_if(c["tracks"].as_array(),
                          [](const auto &t)
                          {
                              return t["bone_hash"].as_string() == Monstey_Hex(Monstey_NativeHash("Root")) ||
                                     t["bone_hash"].as_string() == Monstey_Hex(Monstey_NativeHash("Leg"));
                          });
    const auto sparse_bytes = Monstey_Anix::Create(sparse_scene).Build();
    const auto sb = Monstey_AnimationPreviewBinding::Prepare(ncscene, noncommuting, sparse_bytes, "walk");
    auto sf = sb.CreateFrame();
    const auto sr = Monstey_SkelDefaultPoseRead(noncommuting);
    for (std::size_t bone : {std::size_t{0}, std::size_t{2}})
    {
        for (unsigned k = 0; k < 3; ++k)
            Monstey_Test(std::bit_cast<std::uint32_t>(sf.LocalPose()[bone].position_cm[k]) ==
                             sr.nodes[bone].local_words[k],
                         "Missing position did not retain raw SKEL default");
        for (unsigned k = 0; k < 4; ++k)
            Monstey_Test(std::bit_cast<std::uint32_t>(sf.LocalPose()[bone].quaternion_xyzw[k]) ==
                             sr.nodes[bone].local_words[k + 3],
                         "Missing rotation did not retain raw SKEL default");
    }
    ++groups;
    auto raw_nonunit = skel;
    Monstey_PutU32(raw_nonunit, at + 5 + 6 * 4, std::bit_cast<std::uint32_t>(1.0001f));
    const auto ru = Monstey_SkelDefaultPoseRead(raw_nonunit);
    Monstey_Test(ru.nodes[0].global_words[6] == std::bit_cast<std::uint32_t>(1.0001f) &&
                     ru.nodes[0].inverse_words[6] == ru.nodes[0].global_words[6],
                 "Raw quaternion was normalized");
    ++groups;
    auto opaque = baseline;
    opaque.parts[0].channels.at("TANGENT").values[0] =
        std::bit_cast<double>(std::uint64_t{0x7ff8123456789abc});
    const auto ob = Monstey_AnimationPreviewBinding::Prepare(opaque, skel, bytes, "walk");
    auto of = ob.CreateFrame();
    ob.Evaluate(750, of);
    Monstey_CheckUnrelated(opaque, of.Scene());
    ++groups;
    Monstey_Test(Monstey_HashSha256(bytes) == bytes_hash && Monstey_HashSha256(skel) == skel_hash &&
                     binding.Source().native_baseline == baseline.native_baseline,
                 "Immutable source changed");
    ++groups;
    return J::object({{"status", "passed"},
                      {"groups", groups},
                      {"moving_unique_frames", static_cast<std::uint64_t>(moving.size())},
                      {"binding", report},
                      {"source_unchanged", true},
                      {"reusable_geometry_buffers", true}});
}
J Monstey_ActualTests(const std::filesystem::path &mesh, const std::filesystem::path &skel,
                      const std::filesystem::path &anix, std::string clip,
                      const std::filesystem::path &output)
{
    const auto mesh_hash = Monstey_HashSha256(mesh), skel_hash = Monstey_HashSha256(skel),
               anix_hash = Monstey_HashSha256(anix);
    const auto scene = Monstey_ModelReadScene(mesh, skel);
    const auto skel_bytes = Monstey_ReadFile(skel), anix_bytes = Monstey_ReadFile(anix);
    const auto binding =
        Monstey_AnimationPreviewBinding::Prepare(scene, skel_bytes, anix_bytes, std::move(clip));
    auto frame = binding.CreateFrame();
    const auto info = binding.Inspect();
    const auto first = info["first_tick_ms"].as_number(), last = info["last_tick_ms"].as_number();
    std::filesystem::create_directories(output);
    J samples = J::array();
    std::set<std::string> frames;
    double max_drift = 0;
    std::size_t vertices = 0;
    for (const auto &p : scene.parts)
        vertices += p.channels.at("POSITION").values.size() / 3;
    for (unsigned i = 0; i <= 4; ++i)
    {
        // Integer millisecond ticks are valid across every supported native codec.
        const double tick = std::floor(first + (last - first) * i / 4);
        binding.Evaluate(tick, frame);
        Monstey_OracleGlobal(frame);
        Monstey_CheckUnrelated(scene, frame.Scene());
        const auto hash = Monstey_FrameHash(frame);
        frames.insert(hash);
        double drift = 0;
        for (std::size_t p = 0; p < scene.parts.size(); ++p)
        {
            const auto &a = scene.parts[p].channels.at("POSITION").values,
                       &b = frame.Scene().parts[p].channels.at("POSITION").values;
            for (std::size_t k = 0; k < a.size(); ++k)
                drift = std::max(drift, std::abs(a[k] - b[k]));
        }
        max_drift = std::max(max_drift, drift);
        Monstey_ModelPreviewOptions view;
        view.width = 800;
        view.height = 800;
        view.bones = true;
        view.all_containers = false;
        const auto image = Monstey_ModelPreviewRender(frame.Scene(), view);
        const auto path = output / ("frame-" + std::to_string(i) + ".bmp");
        Monstey_WriteNewFile(path, Monstey_ModelPreviewBitmap(image));
        samples.as_array().push_back(J::object({{"tick_ms", tick},
                                                {"frame_sha256", hash},
                                                {"bmp_sha256", Monstey_HashSha256(path)},
                                                {"max_position_drift_cm", drift}}));
    }
    if (info["clip"].as_string() == "bind")
    {
        Monstey_Test(frames.size() == 1, "Actual bind clip changed across sampled ticks");
        Monstey_Test(max_drift < 0.02,
                     "Actual armor bind reconstruction exceeds the explicit 0.02cm fixture bound");
    }
    else
        Monstey_Test(frames.size() > 1, "Actual moving clip produced no changing evaluated pose");
    Monstey_Test(Monstey_HashSha256(mesh) == mesh_hash && Monstey_HashSha256(skel) == skel_hash &&
                     Monstey_HashSha256(anix) == anix_hash,
                 "Actual source changed");
    auto result = J::object({{"status", "passed"},
                             {"binding", info},
                             {"vertices", static_cast<std::uint64_t>(vertices)},
                             {"samples", samples},
                             {"unique_frames", static_cast<std::uint64_t>(frames.size())},
                             {"max_position_drift_cm", max_drift},
                             {"source_unchanged", true},
                             {"cpu_geometry_only", true}});
    Monstey_WriteNewText(output / "report.json", result.dump(2));
    return result;
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        if (argc == 1)
        {
            std::cout << Monstey_SyntheticTests().dump(2) << '\n';
            return 0;
        }
        if (argc == 7 && std::string_view(argv[1]) == "--actual")
        {
            std::cout << Monstey_ActualTests(argv[2], argv[3], argv[4], argv[5], argv[6]).dump(2) << '\n';
            return 0;
        }
        std::cerr << "Usage: tests [--actual mesh skel anix clip NEW-output-directory]\n";
        return 2;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
