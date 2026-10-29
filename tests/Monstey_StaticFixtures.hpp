#pragma once
#include "Monstey_Core.hpp"
#include <array>
#include <bit>
#include <algorithm>
#include <iostream>
namespace Monstey_StaticFixture {
using namespace Monstey;
using B=Monstey_Bytes;using J=Monstey_Json;
void Check(bool x,const char *m){Monstey_Require(x,m);}
void A32(B &b, std::uint32_t n) {
    for (unsigned j = 0; j < 4; ++j)
        b.push_back(static_cast<std::uint8_t>(n >> (j * 8)));
}
void A16(B &b, std::uint16_t n) {
    b.push_back(static_cast<std::uint8_t>(n));
    b.push_back(static_cast<std::uint8_t>(n >> 8));
}
void Float(B &b, float f) { A32(b, std::bit_cast<std::uint32_t>(f)); }
std::uint32_t String(B &b, const std::string &s) {
    auto n = static_cast<std::uint32_t>(b.size());
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
    return n;
}
B ObjectFixture(const std::string &class_name = "Light") {
    B pool;
    auto schema = String(pool, "ObjectDB"), empty = String(pool, ""), record = String(pool, "ObjectRec");
    while (pool.size() % 4)
        pool.push_back(0);
    B db{'G', 'A', 'D', 'B'};
    for (auto n : {17u, static_cast<std::uint32_t>(pool.size()), 1u, 1u, 0u, 1u, 1u, 0u, 0u, 44u})
        A32(db, n);
    A16(db, 0);
    db.insert(db.end(), pool.begin(), pool.end());
    auto hash = Monstey_NativeHash("Enabled");
    A32(db, hash);
    A32(db, hash);
    db.insert(db.end(), {0, 0, 1});
    A16(db, 1);
    A16(db, 0);
    A32(db, 0);
    A32(db, 1);
    for (auto n : {schema, empty, empty, 0u, 1u, 0u})
        A32(db, n);
    A32(db, record);
    A32(db, 0x00010001);
    A32(db, 0x08000000);
    A32(db, 1);
    A32(db, 0);
    B classes, names;
    String(classes, class_name);
    String(names, "NativeLight");
    B obj{'O', 'B', 'J', 'L'};
    for (auto n : {5u, 1u, 1u, static_cast<std::uint32_t>(classes.size()),
                   static_cast<std::uint32_t>(names.size()), 0u})
        A32(obj, n);
    obj.insert(obj.end(), classes.begin(), classes.end());
    obj.insert(obj.end(), names.begin(), names.end());
    A32(obj, 0x1234);
    A32(obj, 1);
    A32(obj, 0);
    A32(obj, 0);
    A16(obj, 65535);
    A32(obj, 0);
    for (auto f : {10.f, 20.f, 30.f, 0.f, 0.f, 0.f, 1.f})
        Float(obj, f);
    A32(obj, 0xdeadbeef);
    A32(obj, 0xabcdef01);
    B out{'O', 'B', 'J', 'R'};
    A32(out, 13);
    A32(out, 0x17);
    A32(out, 3);
    for (auto pair : {std::pair{1u, db}, std::pair{2u, obj}, std::pair{77u, B{1, 2, 3, 4, 5}}}) {
        A32(out, pair.first);
        A32(out, static_cast<std::uint32_t>(pair.second.size()));
        out.insert(out.end(), pair.second.begin(), pair.second.end());
    }
    return out;
}
B MeshFixture(unsigned aliases = 1) {
    B out{'M', 'E', 'S', 'H'};
    for (auto n : {5u, 1u, aliases, 1u, 2u, 80u, 3u})
        A32(out, n);
    out.insert(out.end(), {0xab, 0xcd, 0xef});
    A32(out, aliases);
    A32(out, 72);
    A32(out, 6);
    for (auto p : {std::array{0.f, 0.f, 0.f}, std::array{1.f, 0.f, 0.f}, std::array{0.f, 1.f, 0.f}}) {
        for (auto f : p)
            Float(out, f);
        for (auto f : {-0.f, 0.f, 1.f})
            Float(out, f);
    }
    for (auto index : {0u, 1u, 2u})
        A16(out, static_cast<std::uint16_t>(index));
    A32(out, 1);
    A32(out, 2);
    out.insert(out.end(), {0, 2, 0, 0, 0, 2, 1, 0});
    A32(out, aliases);
    for (unsigned draw = 0; draw < aliases; ++draw) {
        std::array<std::uint32_t, 13> words{};
        words[0] = 0x10 + draw;
        words[1] = 3;
        words[5] = 1;
        words[6] = 7;
        words[10] = 72;
        for (auto n : words)
            A32(out, n);
    }
    return out;
}
B NativeSceneFixture() {
    B out;
    for (auto word : {2u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u})
        A32(out, word);
    for (unsigned i = 0; i < 2; ++i) {
        A32(out, i ? 0x102u : 0x101u);
        A32(out, 0);
        A32(out, 1);
        for (auto value : {-10.f, -20.f, -30.f, 10.f, 20.f, 30.f})
            Float(out, value);
        for (auto word : {0x11223344u, 0x55667788u, 0u, i ? 1u : 0u,
                          i ? UINT32_MAX : 0u, 0u})
            A32(out, word);
        if (i)
            A32(out, 0);
    }
    for (auto word : {1u, 3u, 0u, UINT32_MAX, UINT32_MAX, 0u})
        A32(out, word);
    Check(out.size() == 200, "Independent scene fixture extent differs");
    return out;
}
B RenderFixture() {
    B strings;
    auto source = String(strings, "worlds/source.wldmsh");
    std::vector<std::uint32_t> mats;
    for (unsigned j = 0; j < 8; ++j)
        mats.push_back(String(strings, "materials/m" + std::to_string(j) + ".mat"));
    B out{'W', 'D', 'S', 'C'};
    for (auto n : {9u, 1u, 1u, 0u, 1u, 1u, 8u, 0u, 0u, static_cast<std::uint32_t>(strings.size())})
        A32(out, n);
    out.insert(out.end(), strings.begin(), strings.end());
    A32(out, 8);
    A32(out, source);
    A32(out, 0);
    for (auto offset : mats)
        A32(out, offset);
    A32(out, 1);
    A32(out, 0);
    A32(out, 0);
    for (auto f : {0.f, 0.f, 0.f, 1.f, 1.f, 0.f})
        Float(out, f);
    A32(out, 0xdeadbeef);
    A16(out, 0);
    A16(out, 0x1234);
    const auto scene = NativeSceneFixture();
    out.insert(out.end(), scene.begin(), scene.end());
    return out;
}
}
