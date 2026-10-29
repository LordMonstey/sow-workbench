#pragma once
#include "Monstey_Havok.hpp"
#include <map>
#include <bit>
#include <algorithm>
namespace Monstey_LightFixture {
using namespace Monstey;using B=Monstey_Bytes;using J=Monstey_Json;
struct Field {
    std::string name;
    std::uint8_t type;
    std::uint16_t count, offset;
};
struct Row {
    std::string category;
    std::vector<std::uint32_t> words;
    std::vector<Field> fields;
};
B Fixture(bool spot = false) {
    B pool(4);
    auto atom = [&](const B &bytes) {
        auto n = static_cast<std::uint32_t>(pool.size());
        pool.insert(pool.end(), bytes.begin(), bytes.end());
        while (pool.size() % 4)
            pool.push_back(0);
        return n;
    };
    auto text = [&](const std::string &s) {
        B b(s.begin(), s.end());
        b.push_back(0);
        return atom(b);
    };
    auto curve = [&](std::uint8_t kind) {
        B b;
        Monstey_AppendU32(b, 2);
        for (auto time : {0.f, 1.f}) {
            Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(time));
            if (kind == 12)
                Monstey_AppendU32(b, 0xffffffffu);
            else
                for (unsigned i = 0; i < (kind == 16 ? 3u : 1u); ++i)
                    Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(5.f));
        }
        return atom(b);
    };
    auto fcurve = curve(13), vcurve = curve(16), ccurve = curve(12), name = text("fx\\test\\original.fxp"),
         type = text(spot ? "Spot" : "Point");
    Row global{"FxGlobalProps", {}, {}}, light{"ClientFX.DynaLight", {}, {}}, sound{"ClientFX.Sound", {}, {}};
    auto field = [](Row &r, const std::string &label, std::uint8_t kind, std::uint32_t word) {
        auto offset = r.words.size();
        r.words.push_back(word);
        r.fields.push_back({label, kind, 1, static_cast<std::uint16_t>(kind == 1 ? offset * 32 : offset)});
    };
    field(global, "FXName", 4, name);
    field(global, "ResourceLenMS", 3, 1000);
    field(global, "MaxUpdateDistance", 2, std::bit_cast<std::uint32_t>(5000.f));
    field(global, "PauseWhenNotVisible", 1, 1);
    global.fields.push_back({"KeyList", 10, 2, static_cast<std::uint16_t>(global.words.size())});
    global.words.push_back(0x00100000);
    global.words.push_back(0x00200000);
    field(light, "Type", 5, type);
    field(light, "StartTime", 2, 0);
    field(light, "EndTime", 2, std::bit_cast<std::uint32_t>(1.f));
    for (auto n : {"Radius", "InnerRadius", "Intensity", "FlickerScale", "FOVx", "FOVY"})
        field(light, n, 13, fcurve);
    field(light, "COLOR", 12, ccurve);
    field(light, "TranslucentColor", 3, 0xffffffffu);
    field(light, "SpecularColor", 3, 0xffffffffu);
    for (auto n : {"Offset", "RotationOffset", "Rotation", "RotationIntegral"})
        field(light, n, 16, vcurve);
    auto bool_word = light.words.size();
    field(light, "RelativeLight", 1, 3);
    light.fields.push_back({"ShadowFlag", 1, 1, static_cast<std::uint16_t>(bool_word * 32 + 1)});
    field(light, "FadeStartDist", 2, std::bit_cast<std::uint32_t>(200.f));
    field(light, "FadeEndDist", 2, std::bit_cast<std::uint32_t>(1000.f));
    field(light, "ShadowBias", 2, std::bit_cast<std::uint32_t>(0.125f));
    field(light, "Unrecognized", 99, 0xdeadbeefu);
    std::vector<Row> rows{global, light, sound};
    B descriptors, registry;
    std::uint32_t words{};
    for (auto &r : rows) {
        auto c = text(r.category), empty = text("");
        for (auto v : {c, c, empty, 1u, 0u, Monstey_NativeHash(r.category)})
            Monstey_AppendU32(registry, v);
        auto first = static_cast<std::uint32_t>(descriptors.size() / 15);
        std::vector<std::size_t> order;
        for (std::size_t i = 0; i < r.fields.size(); ++i)
            order.push_back(i);
        std::sort(order.begin(), order.end(), [&](auto a, auto b) {
            return Monstey_NativeHash(r.fields[a].name) < Monstey_NativeHash(r.fields[b].name);
        });
        for (std::size_t i = 0; i < r.fields.size(); ++i) {
            auto &f = r.fields[i];
            Monstey_AppendU32(descriptors, Monstey_NativeHash(f.name));
            Monstey_AppendU32(descriptors, Monstey_NativeHash(r.fields[order[i]].name));
            descriptors.insert(descriptors.end(),
                               {static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(order[i]), f.type});
            Monstey_AppendU16(descriptors, f.count);
            Monstey_AppendU16(descriptors, f.offset);
        }
        Monstey_AppendU32(registry, ((first & 255) << 24) |
                                        (static_cast<std::uint32_t>(r.fields.size()) << 16) |
                                        static_cast<std::uint32_t>(r.words.size()));
        Monstey_AppendU32(registry, first >> 8);
        for (auto word : r.words)
            Monstey_AppendU32(registry, word);
        words += static_cast<std::uint32_t>(r.words.size());
    }
    B db{'G', 'A', 'D', 'B'};
    for (auto n : {17u, static_cast<std::uint32_t>(pool.size()), 3u, 0u, 3u, words,
                   static_cast<std::uint32_t>(descriptors.size() / 15), 0u, 0u, 44u})
        Monstey_AppendU32(db, n);
    Monstey_AppendU16(db, 0);
    db.insert(db.end(), pool.begin(), pool.end());
    db.insert(db.end(), descriptors.begin(), descriptors.end());
    Monstey_AppendU32(db, 0);
    Monstey_AppendU32(db, 3);
    db.insert(db.end(), registry.begin(), registry.end());
    Monstey_AppendU32(db, 0);
    B out{'L', 'T', 'F', 'X', 8, 0, 0, 0};
    out.insert(out.end(), db.begin(), db.end());
    return out;
}
}
