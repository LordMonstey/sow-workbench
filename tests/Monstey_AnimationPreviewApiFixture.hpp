#pragma once
#include "Monstey_AnimationPreviewApi.hpp"
#include <bit>
namespace
{
using namespace Monstey;
void Monstey_AddFloat(Monstey_Bytes &b, float f) { Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(f)); }
Monstey_Bytes Monstey_MeshFixture(bool skinned = true)
{
    Monstey_Bytes vertices;
    const std::uint16_t positions[3][4] = {{0, 0, 0, 0x3c00}, {0x5640, 0, 0, 0x3c00}, {0, 0x5640, 0, 0x3c00}};
    for (const auto &p : positions)
    {
        for (const auto v : p)
            Monstey_AppendU16(vertices, v);
        if (skinned)
        {
            for (const auto v : {63u, 128u, 64u, 0u})
                vertices.push_back(static_cast<std::uint8_t>(v));
            for (const auto v : {2u, 1u, 0u, 1u})
                vertices.push_back(static_cast<std::uint8_t>(v));
        }
    }
    for (unsigned i = 0; i < 3; ++i)
    {
        Monstey_AppendU16(vertices, i == 1 ? 32767 : 0);
        Monstey_AppendU16(vertices, i == 2 ? 32767 : 0);
        for (const auto v : {0u, 0u, 0x3c00u, 0u, 0x3c00u, 0u, 0u, 0x3c00u, 0u, 0x3c00u, 0u, 0u})
            Monstey_AppendU16(vertices, static_cast<std::uint16_t>(v));
        for (const auto v : {11u + i, 33u + i, 77u + i, 255u})
            vertices.push_back(static_cast<std::uint8_t>(v));
    }
    Monstey_Bytes faces;
    for (const auto v : {0u, 1u, 2u})
        Monstey_AppendU16(faces, static_cast<std::uint16_t>(v));
    const Monstey_Bytes palette = skinned ? Monstey_Bytes{0, 2, 3} : Monstey_Bytes{};
    Monstey_Bytes desc;
    for (const auto v : {0xebaec3fau, 3u, 2u, 0u, 1u, 2u, 0u, 1u, 0xffffffffu})
        Monstey_AppendU32(desc, v);
    for (const auto size :
         {static_cast<std::uint32_t>(vertices.size()), static_cast<std::uint32_t>(faces.size()),
          static_cast<std::uint32_t>(palette.size())})
        for (unsigned c = 0; c < 2; ++c)
            Monstey_AppendU32(desc, size);
    std::array<std::uint32_t, 18> h{};
    h[0] = 0x1a1541bc;
    h[2] = 3;
    h[3] = 2;
    h[4] = 2;
    h[5] = 1;
    h[9] = 1;
    h[10] = skinned ? 8 : 6;
    h[16] = 4;
    h[17] = 160;
    Monstey_Bytes info;
    for (const auto v : h)
        Monstey_AppendU32(info, v);
    info.insert(info.end(), 48, 0x22);
    for (unsigned c = 0; c < 2; ++c)
    {
        for (const auto v :
             {static_cast<std::uint32_t>(vertices.size()), static_cast<std::uint32_t>(faces.size()),
              static_cast<std::uint32_t>(palette.size()), 1u})
            Monstey_AppendU32(info, v);
        for (unsigned i = 0; i < 16; ++i)
            Monstey_AppendU16(info, i % 3 ? 1 : 0);
    }
    for (unsigned c = 0; c < 2; ++c)
        for (const auto v : {0u, 0u, 3u, 0u, 3u, 0u, static_cast<std::uint32_t>(palette.size()),
                             0x11223344u + c, 0x55667788u, 0u})
            Monstey_AppendU32(info, v);
    Monstey_AppendU32(info, 0x8e3e068e);
    Monstey_AppendU32(info, 0x37d749a6);
    Monstey_AppendU32(info, skinned ? 8 : 6);
    for (const auto &a : std::array<std::array<std::uint8_t, 4>, 8>{{{{0, 12, 0, 0}},
                                                                     {{0, 4, 6, 0}},
                                                                     {{0, 4, 7, 0}},
                                                                     {{1, 9, 4, 0}},
                                                                     {{1, 12, 1, 0}},
                                                                     {{1, 12, 2, 0}},
                                                                     {{1, 12, 3, 0}},
                                                                     {{1, 4, 5, 0}}}})
        if (skinned || (a[2] != 6 && a[2] != 7))
            info.insert(info.end(), a.begin(), a.end());
    Monstey_AppendU32(info, 0x93d9a424);
    const int parents[] = {-1, 0, 1, 0};
    const unsigned children[] = {2, 1, 0, 0};
    const float positionsCm[4][3] = {{0, 0, 0}, {0, 50, 0}, {10, 100, 0}, {-40, 0, 0}};
    for (unsigned i = 0; i < 4; ++i)
    {
        const std::array<const char *, 4> names{"Root", "Arm", "Finger", "Leg"};
        Monstey_AppendU32(info, Monstey_NativeHash(names[i]));
        Monstey_AppendU16(info, static_cast<std::uint16_t>(parents[i]));
        Monstey_AppendU16(info, static_cast<std::uint16_t>(children[i]));
        for (const auto v : positionsCm[i])
            Monstey_AddFloat(info, v);
        for (const auto v : {0.0f, 0.0f, 0.0f, 1.0f, 1.0f})
            Monstey_AddFloat(info, v);
    }
    Monstey_Bytes data;
    Monstey_AppendU32(data, 0x95dbdb69);
    for (unsigned i = 0; i < 2; ++i)
        data.insert(data.end(), vertices.begin(), vertices.end());
    for (unsigned i = 0; i < 2; ++i)
        data.insert(data.end(), faces.begin(), faces.end());
    for (unsigned i = 0; i < 2; ++i)
        data.insert(data.end(), palette.begin(), palette.end());
    Monstey_Bytes out;
    for (const auto v : {0x48534d4du, 17u, static_cast<std::uint32_t>(desc.size()),
                         static_cast<std::uint32_t>(info.size()), static_cast<std::uint32_t>(data.size())})
        Monstey_AppendU32(out, v);
    out.insert(out.end(), desc.begin(), desc.end());
    out.insert(out.end(), info.begin(), info.end());
    out.insert(out.end(), data.begin(), data.end());
    return out;
}
Monstey_Bytes Monstey_SkelFixture()
{
    const std::string names("Root\0Arm\0Finger\0Leg\0", 20);
    Monstey_Bytes b(96, 0);
    Monstey_PutU32(b, 0, 0x4c454b53);
    Monstey_PutU32(b, 4, 17);
    Monstey_PutU32(b, 12, static_cast<std::uint32_t>(names.size()));
    Monstey_PutU32(b, 40, 4);
    Monstey_AppendU32(b, 0xdcdcff00);
    b.insert(b.end(), names.begin(), names.end());
    const unsigned offsets[] = {0, 5, 9, 16}, children[] = {2, 1, 0, 0};
    const float t[4][3] = {{0, 0, 0}, {0, 50, 0}, {10, 100, 0}, {-40, 0, 0}};
    for (unsigned i = 0; i < 4; ++i)
    {
        Monstey_AppendU32(b, offsets[i]);
        b.push_back(static_cast<std::uint8_t>(i % 2));
        for (const auto v : t[i])
            Monstey_AddFloat(b, v);
        for (const auto v : {0.0f, 0.0f, 0.0f, 1.0f})
            Monstey_AddFloat(b, v);
        Monstey_AppendU32(b, children[i]);
    }
    return b;
}

} // namespace

#include "Monstey_SkelDefaultPose.hpp"
namespace
{
Monstey_Bytes Monstey_AnimationFixture(const Monstey_Bytes &skel, bool raw = false)
{
    using J = Monstey::Monstey_Json;
    const auto skeleton = Monstey::Monstey_SkelDefaultPoseRead(skel);
    J channels = J::array(), bind = J::array(), walk = J::array();
    for (std::size_t i = 0; i < skeleton.nodes.size(); ++i)
    {
        const auto &n = skeleton.nodes[i];
        channels.as_array().push_back(
            J::object({{"type_hash", "00ce66fc"}, {"bone_hash", Monstey_Hex(n.name_hash)}}));
        channels.as_array().push_back(
            J::object({{"type_hash", "27436396"}, {"bone_hash", Monstey_Hex(n.name_hash)}}));
        auto p = J::array(), q = J::array();
        for (unsigned k = 0; k < 3; ++k)
            p.as_array().emplace_back(double(std::bit_cast<float>(n.local_words[k])));
        for (unsigned k = 0; k < 4; ++k)
            q.as_array().emplace_back(double(std::bit_cast<float>(n.local_words[k + 3])));
        bind.as_array().push_back(
            J::object({{"channel_index", static_cast<unsigned>(2 * i)}, {"values", J::array({p, p})}}));
        bind.as_array().push_back(
            J::object({{"channel_index", static_cast<unsigned>(2 * i + 1)}, {"values", J::array({q, q})}}));
        if (n.name == "Arm")
        {
            auto t = J::object(
                {{"channel_index", static_cast<unsigned>(2 * i + 1)},
                 {"values", J::array({q, J::array({0, 0, 0.7071067811865476, 0.7071067811865476})})}});
            if (raw)
                t["native_codec"] = "UAQT";
            walk.as_array().push_back(std::move(t));
        }
    }
    const auto clip = [](const char *name, J tracks)
    {
        return J::object({{"name", name},
                          {"duration_ticks", 1000},
                          {"ticks", J::array({0, 1000})},
                          {"tracks", std::move(tracks)},
                          {"events", J::array()}});
    };
    return Monstey::Monstey_Anix::Create(
               J::object({{"library_path", "synthetic/api.anix"},
                          {"channels", channels},
                          {"clips", J::array({clip("bind", bind), clip("walk", walk)})}}))
        .Build();
}
} // namespace
