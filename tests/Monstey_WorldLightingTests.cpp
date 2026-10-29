#include "Monstey_Gadb.hpp"
#include "Monstey_WorldLighting.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <iostream>
using namespace Monstey;
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
unsigned passed{};
void Check(bool x, const char *m) {
    if (!x)
        throw std::runtime_error(m);
}
template <class F> void Case(const char *n, F f) {
    f();
    ++passed;
    std::cout << "PASS " << n << '\n';
}
template <class F> void Reject(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    Check(rejected, "expected rejection");
}
std::string P(const std::filesystem::path &p) { return Monstey_Utf8(p.wstring()); }
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
J EchoSchema() {
    return J::object(
        {{"categories",
          J::object(
              {{"FxGlobalProps",
                J::object({{"fields", J::array({J::object({{"name", "KeyList"}, {"array", -1}})})}})}})}});
}
const J &FieldValue(const J &metadata, const char *name) {
    auto hash = Monstey_Hex(Monstey_NativeHash(name));
    for (auto &field : metadata.at("fields").as_array())
        if (field.at("hash").as_string() == hash)
            return field.at("value");
    throw std::runtime_error("field missing");
}
} // namespace
int main(int argc, char **argv) {
    try {
        auto base =
            argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1])) : std::filesystem::temp_directory_path();
        std::filesystem::create_directories(base);
        auto dir = base / ("Monstey_CppWorldLightingTests_" +
                           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(dir);
        auto put = [&](const char *name, const B &raw) {
            auto p = dir / name;
            Monstey_WriteNewFile(p, raw);
            return p;
        };
        auto point = put("point.fxp", Fixture()), spot = put("spot.fxp", Fixture(true));
        auto args = [&](const std::filesystem::path &source, const char *name) {
            return J::object(
                {{"source", P(source)},
                 {"output", P(dir / name)},
                 {"spec", J::object({{"source_sha256", Monstey_HashSha256(source)},
                                     {"resource_path", std::string("fx/monstey/test/") + name + ".fxp"},
                                     {"radius_cm", 500},
                                     {"color_rgb", J::array({0.3, 0, 0.5})},
                                     {"intensity", 7},
                                     {"period_seconds", 3}})}});
        };
        Case("Point native recipe isolates one key and preserves packed policy fields", [&] {
            auto result = Monstey_WorldLightingCall("sow_world_light_build", args(point, "point"));
            Check(result.at("type").as_string() == "Point" &&
                      result.at("color_argb").as_string() == "0xff4c0080",
                  "native packed ARGB rounding differs");
            auto target =
                std::filesystem::path(Monstey_Wide(result.at("resources")[0].at("path").as_string()));
            auto db = Monstey_Gadb::WithSchema(target, EchoSchema());
            auto global = db->Metadata("0x00000000"), light = db->Metadata("0x00100000");
            Check(FieldValue(global, "KeyList") == J::array({"0x00100000"}) &&
                      FieldValue(light, "Radius").at("knots")[1].at("value").as_number() == 500,
                  "active key/curve wrong");
            Check(!FieldValue(light, "RelativeLight").as_bool() &&
                      FieldValue(light, "ShadowFlag").as_bool() &&
                      FieldValue(light, "ShadowBias").as_number() == 0.125,
                  "unmodified native policy or packed bool changed");
        });
        Case("Spot preserves native type and writes real FOV curves", [&] {
            auto a = args(spot, "spot");
            a["spec"]["type"] = "Spot";
            a["spec"]["fov_degrees"] = 90;
            auto result = Monstey_WorldLightingCall("sow_world_light_build", a);
            auto db = Monstey_Gadb::WithSchema(
                std::filesystem::path(Monstey_Wide(result.at("resources")[0].at("path").as_string())),
                EchoSchema());
            Check(FieldValue(db->Metadata("0x00100000"), "FOVx").at("knots")[0].at("value").as_number() == 90,
                  "native spot FOV not authored");
        });
        Case("stale donor rejects before output", [&] {
            auto a = args(point, "stale");
            a["spec"]["source_sha256"] = "wrong";
            Reject([&] { Monstey_WorldLightingCall("sow_world_light_build", a); });
            Check(!std::filesystem::exists(dir / "stale"), "stale source published");
        });
        Case("RGB range and radius contracts reject before output", [&] {
            auto a = args(point, "invalid");
            a["spec"]["color_rgb"] = J::array({1, 2, 3});
            Reject([&] { Monstey_WorldLightingCall("sow_world_light_build", a); });
            a["spec"]["color_rgb"] = J::array({1, 0, 0});
            a["spec"]["inner_radius_cm"] = 501;
            Reject([&] { Monstey_WorldLightingCall("sow_world_light_build", a); });
        });
        Case("Point cannot be silently changed into a Spot enum", [&] {
            auto a = args(point, "wrong-type");
            a["spec"]["type"] = "Spot";
            Reject([&] { Monstey_WorldLightingCall("sow_world_light_build", a); });
            Check(!std::filesystem::exists(dir / "wrong-type"), "unqualified type change published");
        });
        Case("deep Unicode light component publishes native curves", [&] {
            auto deep = dir / std::filesystem::path(L"light-\u00e9preuve");
            for (unsigned i = 0; i < 8; ++i)
                deep /= L"long-native-component-folder";
            auto a = args(point, "deep");
            a["output"] = P(deep);
            auto result = Monstey_WorldLightingCall("sow_world_light_build", a);
            auto file = std::filesystem::path(Monstey_Wide(result.at("resources")[0].at("path").as_string()));
            Check(Monstey_HashSha256(file) == result.at("resources")[0].at("sha256").as_string(),
                  "deep native light publication hash differs");
        });
        Case("deep failed GADB sidecar publication leaves no candidate or stage", [&] {
            auto deep = dir / std::filesystem::path(L"light-rollback-\u00e9preuve");
            for (unsigned i = 0; i < 8; ++i)
                deep /= L"long-native-component-folder";
            auto a = args(point, "rollback");
            a["output"] = P(deep);
            a["spec"]["resource_path"] = "fx/monstey/" + std::string(248, 'n') + ".fxp";
            Reject([&] { Monstey_WorldLightingCall("sow_world_light_build", a); });
            Check(!std::filesystem::exists(Monstey_WindowsFilePath(deep)),
                  "failed deep native light left candidate, sidecar or directory");
        });
        Case("existing output never overwritten", [&] {
            auto before = Monstey_HashSha256(dir / "point/resource-manifest.json");
            Reject([&] { Monstey_WorldLightingCall("sow_world_light_build", args(point, "point")); });
            Check(Monstey_HashSha256(dir / "point/resource-manifest.json") == before, "manifest overwritten");
        });
        Case("all synthetic donors remain byte exact", [&] {
            Check(Monstey_ReadFile(point) == Fixture() && Monstey_ReadFile(spot) == Fixture(true),
                  "source modified");
        });
        if (argc > 2) {
            auto root = std::filesystem::path(Monstey_Wide(argv[2]));
            for (auto item : {std::pair{"Point", "sow-port/crimson-bow/storm-v001/source/fx/characters/"
                                                 "ambientlife/collectibles/arrow_collectible.fxp"},
                              std::pair{"Spot", "sow-port/mounts-expansion/alduin-fx/source/fx/characters/"
                                                "enemy/fellbeast/firestream/streampoint_01.fxp"}}) {
                auto source = root / item.second;
                Case(item.first, [&] {
                    auto before = Monstey_HashSha256(source);
                    auto label = std::string("native_") + item.first;
                    auto a = args(source, label.c_str());
                    a["spec"]["type"] = item.first;
                    auto result = Monstey_WorldLightingCall("sow_world_light_build", a);
                    Check(result.at("type").as_string() == item.first &&
                              result.at("active_key_list").size() == 1 &&
                              Monstey_HashSha256(source) == before,
                          "native light proof/source differs");
                    std::cout << "NATIVE_LIGHT " << item.first << " " << before << '\n';
                });
            }
        }
        std::cout << "RESULT " << passed << " lighting groups passed; outputs " << dir.string() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
