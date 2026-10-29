#include "Monstey_World.hpp"
#include "Monstey_WorldComposition.hpp"
#include "Monstey_WorldEdit.hpp"
#include <algorithm>
#include <array>
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
void U(B &b, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(static_cast<std::uint8_t>(n >> (8 * i)));
}
void H(B &b, std::uint16_t n) {
    b.push_back(static_cast<std::uint8_t>(n));
    b.push_back(static_cast<std::uint8_t>(n >> 8));
}
void F(B &b, float f) { U(b, std::bit_cast<std::uint32_t>(f)); }
std::uint32_t S(B &b, const std::string &s) {
    auto p = static_cast<std::uint32_t>(b.size());
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
    return p;
}
std::string P(const std::filesystem::path &p) { return Monstey_Utf8(p.wstring()); }
J Desc(const std::filesystem::path &p, const B &b) {
    return J::object(
        {{"path", P(p)}, {"bytes", static_cast<std::uint64_t>(b.size())}, {"sha256", Monstey_HashSha256(b)}});
}
B Object() {
    B pool;
    auto schema = S(pool, "ObjectDB"), empty = S(pool, ""), record = S(pool, "ObjectRec");
    while (pool.size() % 4)
        pool.push_back(0);
    B db{'G', 'A', 'D', 'B'};
    for (auto n : {17u, static_cast<std::uint32_t>(pool.size()), 1u, 1u, 0u, 1u, 1u, 0u, 0u, 44u})
        U(db, n);
    H(db, 0);
    db.insert(db.end(), pool.begin(), pool.end());
    U(db, Monstey_NativeHash("Enabled"));
    U(db, Monstey_NativeHash("Enabled"));
    db.insert(db.end(), {0, 0, 1});
    H(db, 1);
    H(db, 0);
    for (auto n : {0u, 1u, schema, empty, empty, 0u, 1u, 0u, record, 0x00010001u, 0x08000000u, 1u, 0u})
        U(db, n);
    B classes, names;
    S(classes, "Light");
    S(names, "NativeLight");
    B obj{'O', 'B', 'J', 'L'};
    for (auto n : {5u, 1u, 1u, static_cast<std::uint32_t>(classes.size()),
                   static_cast<std::uint32_t>(names.size()), 0u})
        U(obj, n);
    obj.insert(obj.end(), classes.begin(), classes.end());
    obj.insert(obj.end(), names.begin(), names.end());
    for (auto n : {1u, 1u, 0u, 0u})
        U(obj, n);
    H(obj, 65535);
    U(obj, 0);
    for (auto f : {10.f, 20.f, 30.f, 0.f, 0.f, 0.f, 1.f})
        F(obj, f);
    U(obj, 0xdeadbeef);
    U(obj, 0xabcdef01);
    B out{'O', 'B', 'J', 'R'};
    U(out, 13);
    U(out, 0);
    U(out, 2);
    for (auto &e : {std::pair{1u, db}, std::pair{2u, obj}}) {
        U(out, e.first);
        U(out, static_cast<std::uint32_t>(e.second.size()));
        out.insert(out.end(), e.second.begin(), e.second.end());
    }
    return out;
}
B Instance() {
    B names;
    S(names, "prefabs/test.inst");
    while (names.size() % 8)
        names.push_back(0);
    B out;
    for (auto n : {0x42465250u, 8u, Monstey_PathHash("worlds/test.inst"), 1u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u,
                   1u, static_cast<std::uint32_t>(names.size())})
        U(out, n);
    B d(44);
    Monstey_PutU32(d, 0, 0x1234);
    Monstey_PutU32(d, 16, 0xffffffffu);
    Monstey_PutU32(d, 36, 1);
    out.insert(out.end(), d.begin(), d.end());
    for (unsigned i = 0; i < 5; ++i)
        U(out, 0);
    U(out, 0);
    U(out, 0x5678);
    U(out, 0);
    F(out, 1000);
    for (auto f : {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f})
        F(out, f);
    U(out, 0);
    U(out, 0);
    out.insert(out.end(), names.begin(), names.end());
    return out;
}
B Mesh() {
    B out{'M', 'E', 'S', 'H'};
    for (auto n : {5u, 1u, 1u, 1u, 1u, 44u, 0u, 1u, 36u, 6u})
        U(out, n);
    for (auto f : {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f})
        F(out, f);
    for (auto n : {0, 1, 2})
        H(out, static_cast<std::uint16_t>(n));
    U(out, 1);
    U(out, 1);
    out.insert(out.end(), {0, 2, 0, 0});
    U(out, 1);
    std::array<std::uint32_t, 13> w{};
    w[1] = 3;
    w[2] = 3;
    w[5] = 1;
    w[10] = 36;
    for (auto n : w)
        U(out, n);
    return out;
}
B Binding() {
    B names;
    S(names, "worlds/test.inst");
    while (names.size() % 4)
        names.push_back(0);
    B out{'W', 'D', 'S', 'C'};
    for (auto n : {9u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 0u, static_cast<std::uint32_t>(names.size())})
        U(out, n);
    out.insert(out.end(), names.begin(), names.end());
    for (auto f : {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f})
        F(out, f);
    for (auto n : {0x1111u, 0x2222u, 0x1234u, 0x3333u, 0u, 0u, 0u, 1u, 0u, 1u, 0u})
        U(out, n);
    out.resize(out.size() + 52);
    return out;
}
} // namespace
int main(int argc, char **argv) {
    try {
        auto base =
            argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1])) : std::filesystem::temp_directory_path();
        std::filesystem::create_directories(Monstey_WindowsFilePath(base));
        auto dir = base / ("Monstey_CppWorldEditTests_" +
                           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(Monstey_WindowsFilePath(dir));
        auto put = [&](const char *n, const B &b) {
            auto p = dir / n;
            Monstey_WriteNewFile(p, b);
            return Desc(p, b);
        };
        auto obj = Object(), inst = Instance(), mesh = Mesh(), binding = Binding();
        auto od = put("source.objres", obj), id = put("source.inst", inst), md = put("source.wldmsh", mesh),
             wd = put("source.wldclnt", binding);
        Monstey_Bundle bundle{"Bundles/Types:WorldQuest",
                              {{"worlds/test.objres", static_cast<std::uint32_t>(obj.size())},
                               {"worlds/test.inst", static_cast<std::uint32_t>(inst.size())},
                               {"worlds/test.wldmsh", static_cast<std::uint32_t>(mesh.size())},
                               {"worlds/test.wldclnt", static_cast<std::uint32_t>(binding.size())},
                               {"worlds/unchanged.bin", 3}}};
        auto bd = put("source.bndl", Monstey_BuildBundle(bundle)),
             ed = put("source.embb", Monstey_BuildEmbedded({{"worlds/test.objres", obj},
                                                            {"worlds/test.inst", inst},
                                                            {"worlds/test.wldmsh", mesh},
                                                            {"worlds/test.wldclnt", binding},
                                                            {"worlds/unchanged.bin", B{9, 8, 7}}}));
        auto source = [](const char *backend, const char *path, const J &descriptor, const J &spec) {
            return J::object(
                {{"backend", backend}, {"resource_path", path}, {"source", descriptor}, {"spec", spec}});
        };
        auto plan = J::object(
            {{"schema", "Monstey_WorldEdit_1"},
             {"sources",
              J::array(
                  {source(
                       "OBJRES", "worlds/test.objres", od,
                       J::object({{"source_sha256", od.at("sha256")},
                                  {"updates", J::array({J::object({{"name", "NativeLight"},
                                                                   {"position", J::array({1, 2, 3})}})})}})),
                   source("OBJRES", "worlds/test.objres", od,
                          J::object({{"source_sha256", od.at("sha256")},
                                     {"updates", J::array({J::object(
                                                     {{"name", "NativeLight"},
                                                      {"properties", J::object({{"Enabled", false}})}})})}})),
                   source("INST", "worlds/test.inst", id,
                          J::object(
                              {{"source_sha256", id.at("sha256")},
                               {"updates",
                                J::array({J::object({{"index", 0}, {"position", J::array({50, 0, 0})}})})}})),
                   source("MESH5", "worlds/test.wldmsh", md,
                          J::object({{"source_sha256", md.at("sha256")},
                                     {"updates",
                                      J::array({J::object({{"group_index", 0},
                                                           {"draw_index", 0},
                                                           {"translation", J::array({5, 0, 0})}})})}}))})},
             {"bindings", J::array({J::object({{"inst_resource_path", "worlds/test.inst"},
                                               {"resource_path", "worlds/test.wldclnt"},
                                               {"owner_key", 0x1234},
                                               {"donor", wd}})})},
             {"bundles",
              J::array({J::object({{"resource_path", "worlds/test.bndl"},
                                   {"source", bd},
                                   {"resources", J::array({"worlds/test.objres", "worlds/test.inst",
                                                           "worlds/test.wldmsh", "worlds/test.wldclnt"})},
                                   {"embb_resource_path", "worlds/test.embb"},
                                   {"embb_source", ed}})})}});
        Case("native multi-stage world edits compose bindings and bundle children", [&] {
            auto result = Monstey_WorldEditCall(
                "sow_worldedit_compile", J::object({{"plan", plan}, {"output", P(dir / "component")}}));
            Check(result.at("operations").size() == 6 &&
                      result.at("all_requested_bindings_built").as_bool() &&
                      result.at("pending_dependencies").size() == 1,
                  "world edit operations/dependencies differ");
            auto output = dir / "component/assets/worlds";
            auto owners = Monstey_ObjresOwners(Monstey_ReadFile(output / "test.objres"));
            Check(
                owners[0].at("position")[0].as_number() == 1 &&
                    !owners[0].at("property_values").at(Monstey_Hex(Monstey_NativeHash("Enabled"))).as_bool(),
                "second stage discarded earlier object edit");
            auto child = Monstey_ReadEmbedded(Monstey_ReadFile(output / "test.embb"));
            Check(child.back().bytes == B({9, 8, 7}) &&
                      child[0].bytes == Monstey_ReadFile(output / "test.objres"),
                  "embedded unrelated/changed bytes differ");
            auto w = Monstey_ReadFile(output / "test.wldclnt");
            Check(std::bit_cast<float>(Monstey_U32(w, 44 + Monstey_U32(w, 40))) == 51,
                  "binding used stale pre-transform bounds");
            auto b = Monstey_ReadBundle(Monstey_ReadFile(output / "test.bndl"));
            for (auto &row : b.resources)
                if (row.resource == "worlds/test.objres")
                    Check(row.size_hint == Monstey_ReadFile(output / "test.objres").size(),
                          "bundle size hint stale");
        });
        Case("validation-only world edit writes no component", [&] {
            auto result = Monstey_WorldEditCall("sow_worldedit_compile", J::object({{"plan", plan}}));
            Check(result.at("status").as_string() == "OFFLINE_WORLD_EDIT_VALIDATED" &&
                      result.at("resources").size() == 6,
                  "validation result differs");
        });
        Case("late invalid stage publishes no earlier successful candidate", [&] {
            auto bad = plan;
            bad["sources"][1]["spec"]["updates"][0]["properties"]["Enabled"] = 123;
            Reject([&] {
                Monstey_WorldEditCall("sow_worldedit_compile",
                                      J::object({{"plan", bad}, {"output", P(dir / "invalid")}}));
            });
            Check(!std::filesystem::exists(Monstey_WindowsFilePath(dir / "invalid")),
                  "failed plan published earlier stage");
        });
        Case("native empty WDSC retains orphan slots without omitting another owner", [&] {
            auto empty = plan;
            empty["sources"] = J::array({source(
                "INST", "worlds/test.inst", id,
                J::object({{"deletes",
                            J::array({J::object({{"index", 0}, {"owner_definition_key", 0x1234}})})}}))});
            empty["bundles"][0]["resources"] = J::array({"worlds/test.inst", "worlds/test.wldclnt"});
            auto result = Monstey_WorldEditCall("sow_worldedit_compile",
                                                J::object({{"plan", empty}, {"output", P(dir / "empty")}}));
            auto bytes = Monstey_ReadFile(dir / "empty/assets/worlds/test.wldclnt");
            Check(bytes.size() == 96 && result.at("pending_dependencies").empty(),
                  "empty owner binding not coherent");
        });
        Case("explicit new resource is added to both native dependency tables", [&] {
            auto added =
                J::object({{"schema", "Monstey_WorldEdit_1"},
                           {"sources", J::array({source("OBJRES", "worlds/new.objres", od, J::object())})},
                           {"bundles", plan.at("bundles")}});
            added["bundles"][0]["resources"] = J::array({"worlds/new.objres"});
            added["bundles"][0]["additions"] = J::array({"worlds/new.objres"});
            Monstey_WorldEditCall("sow_worldedit_compile",
                                  J::object({{"plan", added}, {"output", P(dir / "added")}}));
            auto b = Monstey_ReadBundle(Monstey_ReadFile(dir / "added/assets/worlds/test.bndl"));
            auto e = Monstey_ReadEmbedded(Monstey_ReadFile(dir / "added/assets/worlds/test.embb"));
            Check(b.resources.back().resource == "worlds/new.objres" &&
                      e.back().resource == "worlds/new.objres" && e.back().bytes == obj,
                  "new candidate was not added natively");
        });
        Case("undeclared bundle additions reject before output", [&] {
            auto bad = plan;
            bad["bundles"][0]["resources"] = J::array({"worlds/missing.objres"});
            Reject([&] {
                Monstey_WorldEditCall("sow_worldedit_compile",
                                      J::object({{"plan", bad}, {"output", P(dir / "missing")}}));
            });
            Check(!std::filesystem::exists(Monstey_WindowsFilePath(dir / "missing")),
                  "invalid bundle published");
        });
        Case("Blender v1 and v2 plans use the same native path", [&] {
            auto old = J::object(
                {{"schema", "Monstey_WorldBlenderEdits_1"},
                 {"objres",
                  J::array({J::object(
                      {{"source", od},
                       {"resource_path", "worlds/test.objres"},
                       {"updates", J::array({J::object({{"name", "NativeLight"},
                                                        {"position", J::array({7, 8, 9})}})})}})})}});
            auto result = Monstey_WorldEditCall("sow_worldedit_compile", J::object({{"plan", old}}));
            Check(result.at("operations").size() == 1, "v1 normalization lost edit");
            auto v2 = plan;
            v2["schema"] = "Monstey_WorldBlenderEdits_2";
            v2["scene_sha256"] = "metadata";
            Check(Monstey_WorldEditCall("sow_worldedit_compile", J::object({{"plan", v2}}))
                          .at("operations")
                          .size() == 6,
                  "v2 native route differs");
        });
        Case("deep Unicode component publication uses extended physical paths", [&] {
            auto deep = dir / std::filesystem::path(L"Unicode-\u00e9preuve");
            for (unsigned i = 0; i < 8; ++i)
                deep /= L"long-native-component-folder";
            auto single =
                J::object({{"schema", "Monstey_WorldEdit_1"},
                           {"sources", J::array({source("OBJRES", "worlds/test.objres", od, J::object())})}});
            Monstey_WorldEditCall("sow_worldedit_compile",
                                  J::object({{"plan", single}, {"output", P(deep)}}));
            Check(Monstey_ReadFile(deep / "assets/worlds/test.objres") == obj, "deep component bytes differ");
        });
        Case("existing output and all originals remain immutable", [&] {
            auto before = Monstey_HashSha256(dir / "component/resource-manifest.json");
            Reject([&] {
                Monstey_WorldEditCall("sow_worldedit_compile",
                                      J::object({{"plan", plan}, {"output", P(dir / "component")}}));
            });
            Check(Monstey_HashSha256(dir / "component/resource-manifest.json") == before &&
                      Monstey_ReadFile(std::filesystem::path(Monstey_Wide(od.at("path").as_string()))) == obj,
                  "existing output/source changed");
        });
        if (argc > 2) {
            auto root = std::filesystem::path(Monstey_Wide(argv[2]));
            Case("actual native physics and navigation use the same in-memory composition path", [&] {
                auto physics =
                    root / "sow-port/world-objects/fixtures-v001/source/worlds/retail/snowmtn/snowmtn.objres";
                auto section = root / "sow-port/world-edit/private-section-v001/source/worlds/retail/"
                                      "minasithil/sectionsandlayers";
                auto hkai = section / "t4prefallprops.hkai", owner = section / "t4prefallprops.objres";
                auto physical_raw = Monstey_ReadFile(physics), nav_raw = Monstey_ReadFile(hkai),
                     owner_raw = Monstey_ReadFile(owner);
                auto physical_desc = Desc(physics, physical_raw), nav_desc = Desc(hkai, nav_raw),
                     owner_desc = Desc(owner, owner_raw);
                auto nav_source = source(
                    "NAVIGATION", "worlds/retail/minasithil/sectionsandlayers/t4prefallprops.hkai", nav_desc,
                    J::object({{"source_sha256", nav_desc.at("sha256")},
                               {"owner_source_sha256", owner_desc.at("sha256")}}));
                nav_source["owner_resource_path"] =
                    "worlds/retail/minasithil/sectionsandlayers/t4prefallprops.objres";
                nav_source["owner_source"] = owner_desc;
                auto native = J::object(
                    {{"schema", "Monstey_WorldEdit_1"},
                     {"sources",
                      J::array({source("PHYSICS", "worlds/retail/snowmtn/snowmtn.objres", physical_desc,
                                       J::object({{"source_sha256", physical_desc.at("sha256")}})),
                                nav_source})}});
                auto report = Monstey_WorldEditCall("sow_worldedit_compile", J::object({{"plan", native}}));
                Check(report.at("operations").size() == 2 && report.at("resources").size() == 3 &&
                          report.at("pending_dependencies").empty(),
                      "native physics/navigation composition changed identity or lost owners");
                for (auto &row : report.at("resources").as_array())
                    Check(!row.at("changed").as_bool(), "native no-op component modified a source");
                Check(Monstey_HashSha256(physics) == physical_desc.at("sha256").as_string() &&
                          Monstey_HashSha256(hkai) == nav_desc.at("sha256").as_string() &&
                          Monstey_HashSha256(owner) == owner_desc.at("sha256").as_string(),
                      "native compositor mutated originals");
            });
        }
        std::cout << "RESULT " << passed << " world edit groups passed; outputs " << dir.string() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
