#include "Monstey_Archive.hpp"
#include "Monstey_WorldComposition.hpp"
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
void U(B &b, std::uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}
void H(B &b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void F(B &b, float f) { U(b, std::bit_cast<std::uint32_t>(f)); }
std::uint32_t S(B &b, const std::string &s) {
    auto p = static_cast<std::uint32_t>(b.size());
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
    return p;
}
std::string P(const std::filesystem::path &p) { return Monstey_Utf8(p.wstring()); }
J Record(const std::filesystem::path &p, const B &b) {
    return J::object(
        {{"path", P(p)}, {"bytes", static_cast<std::uint64_t>(b.size())}, {"sha256", Monstey_HashSha256(b)}});
}
B EmptyObj() {
    B pool;
    auto schema = S(pool, "ObjectDB"), empty = S(pool, ""), record = S(pool, "ObjectRec");
    while (pool.size() % 4)
        pool.push_back(0);
    B db{'G', 'A', 'D', 'B'};
    for (auto n : {17u, static_cast<std::uint32_t>(pool.size()), 1u, 1u, 0u, 0u, 0u, 0u, 0u, 44u})
        U(db, n);
    H(db, 0);
    db.insert(db.end(), pool.begin(), pool.end());
    for (auto n : {0u, 1u, schema, empty, empty, 0u, 1u, 0u, record, 0u, 0x08000000u, 0u})
        U(db, n);
    B classes, names;
    std::vector<std::uint32_t> co, no;
    for (auto c : {"WorldInitializer", "WorldCollision", "MemoryReporter"})
        co.push_back(S(classes, c));
    for (auto n : {"Init", "Collision", "Memory"})
        no.push_back(S(names, n));
    B obj{'O', 'B', 'J', 'L'};
    for (auto n : {5u, 1u, 3u, static_cast<std::uint32_t>(classes.size()),
                   static_cast<std::uint32_t>(names.size()), 0u})
        U(obj, n);
    obj.insert(obj.end(), classes.begin(), classes.end());
    obj.insert(obj.end(), names.begin(), names.end());
    U(obj, 1);
    U(obj, 3);
    for (std::size_t i = 0; i < 3; ++i) {
        U(obj, co[i]);
        U(obj, no[i]);
        H(obj, 65535);
        U(obj, 0);
        for (auto f : {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f})
            F(obj, f);
        U(obj, 0);
        U(obj, 0);
    }
    B out{'O', 'B', 'J', 'R'};
    U(out, 13);
    U(out, 0);
    U(out, 2);
    for (auto &entry : {std::pair{1u, db}, std::pair{2u, obj}}) {
        U(out, entry.first);
        U(out, static_cast<std::uint32_t>(entry.second.size()));
        out.insert(out.end(), entry.second.begin(), entry.second.end());
    }
    return out;
}
B Inst(const std::string &stem, bool empty) {
    B names;
    if (!empty) {
        S(names, "prefabs/test.inst");
        while (names.size() % 8)
            names.push_back(0);
    }
    B out;
    for (auto n :
         {0x42465250u, 8u, Monstey_PathHash(stem + ".inst"), empty ? 0u : 1u, 0u, 0u, empty ? 0u : 1u, 0u, 0u,
          0u, 0u, 0u, empty ? 0u : 1u, static_cast<std::uint32_t>(names.size())})
        U(out, n);
    if (!empty) {
        B d(44);
        Monstey_PutU32(d, 0, 0x1234);
        Monstey_PutU32(d, 16, 0xffffffff);
        Monstey_PutU32(d, 36, 1);
        out.insert(out.end(), d.begin(), d.end());
    }
    for (unsigned i = 0; i < 5; ++i)
        U(out, 0);
    if (!empty) {
        U(out, 0);
        U(out, 0x5678);
        U(out, 0);
        F(out, 1000);
        for (auto f : {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f})
            F(out, f);
        U(out, 0);
        U(out, 0);
        out.insert(out.end(), names.begin(), names.end());
    }
    return out;
}
B Wdsc(const std::string &stem, bool empty) {
    B out{'W', 'D', 'S', 'C'};
    U(out, 9);
    if (empty) {
        out.resize(96);
        return out;
    }
    B names;
    auto path = stem + ".inst";
    std::replace(path.begin(), path.end(), '/', '\\');
    S(names, path);
    while (names.size() % 4)
        names.push_back(0);
    for (auto n : {1u, 0u, 1u, 1u, 0u, 0u, 0u, 0u, static_cast<std::uint32_t>(names.size())})
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
        std::filesystem::create_directories(base);
        auto dir = base / ("Monstey_CppWorldCompositionTests_" +
                           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(dir);
        auto put = [&](const std::string &name, const B &b) {
            auto p = dir / name;
            Monstey_WriteNewFile(p, b);
            return p;
        };
        auto json = [&](const std::string &name, const J &j) {
            auto p = dir / name;
            Monstey_WriteNewText(p, j.dump(2));
            return p;
        };
        Case("independent BNDL6 header and offsets", [&] {
            B b{'B', 'N', 'D', 'L'};
            for (auto n : {6u, 8u, 12u, 1u, 123u})
                U(b, n);
            b.insert(b.end(), {'a', '.', 'i', 'n', 's', 't', 0, 0, 'T', 0, 0, 0});
            U(b, 0);
            auto parsed = Monstey_ReadBundle(b);
            Check(parsed.kind == "T" && parsed.resources[0].resource == "a.inst" &&
                      parsed.resources[0].size_hint == 123,
                  "BNDL independent layout");
            Check(Monstey_BuildBundle(parsed) == b, "BNDL exact reconstruction");
        });
        Case("native bundle size and duplicate validation", [&] {
            auto b = Monstey_BuildBundle({"Bundles/Types:WorldQuest", {{"A/B.inst", 12}, {"a/c.objres", 0}}});
            auto q = Monstey_ReadBundle(b);
            Check(q.resources.size() == 2 && q.resources[0].resource == "a/b.inst",
                  "BNDL path normalization");
            auto bad = b;
            Monstey_PutU32(bad, 16, 0xffffffff);
            Reject([&] { Monstey_ReadBundle(bad); });
            Reject([&] { Monstey_BuildBundle({"T", {{"x", 1}, {"X", 2}}}); });
        });
        Case("independent EMBB5 child extents", [&] {
            B b{'E', 'M', 'B', 'B'};
            for (auto n : {5u, 4u, 1u})
                U(b, n);
            b.insert(b.end(), {'a', 0, 0, 0});
            U(b, 0);
            U(b, 3);
            b.insert(b.end(), {4, 5, 6});
            auto q = Monstey_ReadEmbedded(b);
            Check(q.size() == 1 && q[0].resource == "a" && q[0].bytes == B({4, 5, 6}),
                  "EMBB independent layout");
            Check(Monstey_BuildEmbedded(q) == b, "EMBB exact reconstruction");
        });
        Case("EMBB zero children empty payload and corruption", [&] {
            auto b = Monstey_BuildEmbedded({{"a", {}}, {"b", B{7}}});
            Check(Monstey_ReadEmbedded(b)[0].bytes.empty(), "empty child");
            Check(Monstey_ReadEmbedded(Monstey_BuildEmbedded({})).empty(), "zero children");
            b.pop_back();
            Reject([&] { Monstey_ReadEmbedded(b); });
            Reject([&] { Monstey_BuildEmbedded({{"a", {}}, {"A", {}}}); });
        });
        Case("BNDL updates preserve original dictionary and raw case", [&] {
            auto original = Monstey_BuildBundle({"T", {{"a", 10}, {"b", 20}}});
            original[28] = 'A';
            original[30] = 0x7f;
            Check(Monstey_RebuildBundle(original, {}) == original, "no-op BNDL rebuild changed raw bytes");
            auto changed = Monstey_RebuildBundle(original, {{"a", 123}});
            auto expected = original;
            Monstey_PutU32(expected, 20, 123);
            Check(changed == expected, "BNDL size update rewrote unrelated dictionary/kind/table bytes");
            auto added = Monstey_RebuildBundle(original, {{"b", 77}}, {{"c", 88}});
            auto report = Monstey_ReadBundle(added);
            Check(report.resources.size() == 3 && report.resources[1].size_hint == 77 &&
                      report.resources[2].resource == "c" && report.resources[2].size_hint == 88 &&
                      std::equal(original.begin() + 28, original.begin() + 36, added.begin() + 32),
                  "BNDL addition did not retain original dictionary prefix");
            Reject([&] { Monstey_RebuildBundle(original, {{"absent", 1}}); });
            Reject([&] { Monstey_RebuildBundle(original, {}, {{"a", 1}}); });
        });
        Case("EMBB replacements retain unrelated payloads and original name offsets", [&] {
            auto original = Monstey_BuildEmbedded({{"a", B{1, 2, 3}}, {"b", B{4, 5}}});
            original[16] = 'A';
            original[18] = 0x7f;
            Check(Monstey_RebuildEmbedded(original, {}) == original, "no-op EMBB rebuild changed raw bytes");
            auto changed = Monstey_RebuildEmbedded(original, {{"a", B{9}}}, {{"c", B{7, 8}}});
            auto report = Monstey_ReadEmbedded(changed);
            Check(report.size() == 3 && report[0].bytes == B({9}) && report[1].bytes == B({4, 5}) &&
                      report[2].resource == "c" && report[2].bytes == B({7, 8}) &&
                      std::equal(original.begin() + 16, original.begin() + 24, changed.begin() + 16),
                  "EMBB update altered unrelated payload or opaque dictionary prefix");
            Reject([&] { Monstey_RebuildEmbedded(original, {{"absent", B{}}}); });
            Reject([&] { Monstey_RebuildEmbedded(original, {}, {{"a", B{}}}); });
        });
        Case("normalized additions cannot collide across raw path spellings", [&] {
            auto b = Monstey_BuildBundle({"T", {}});
            Reject([&] { Monstey_RebuildBundle(b, {}, {{"A/B", 1}, {"a\\b", 2}}); });
            auto e = Monstey_BuildEmbedded({});
            Reject([&] { Monstey_RebuildEmbedded(e, {}, {{"A/B", B{}}, {"a\\b", B{}}}); });
        });
        std::string old = "worlds/retail/test/sectionsandlayers/empty",
                    stem = "worlds/retail/test/sectionsandlayers/monstey/new_section";
        B script{'S', 'C', 'R', 'I', 1, 0, 0, 0, 1};
        std::string lua =
            "-- empty native section\nlocal section_123 = {}\n_G.loadedresource = section_123\n";
        U(script, static_cast<std::uint32_t>(lua.size() + 1));
        script.insert(script.end(), lua.begin(), lua.end());
        script.push_back(0);
        B veg{'V', 'E', 'G', 'G'};
        U(veg, 1);
        H(veg, static_cast<std::uint16_t>(old.size() + 1));
        veg.insert(veg.end(), old.begin(), old.end());
        veg.push_back(0);
        veg.insert(veg.end(), {0x12, 0x34, 0x56});
        std::map<std::string, B> payloads{{old + ".objres", EmptyObj()},
                                          {old + ".inst", Inst(old, true)},
                                          {old + ".wldclnt", Wdsc(old, true)},
                                          {old + ".scripts", script},
                                          {old + ".veggrid", veg},
                                          {old + ".hkai", B{1, 2, 3}}};
        Monstey_Bundle bundle{"Bundles/Types:WorldQuest", {}};
        for (auto &[path, b] : payloads)
            bundle.resources.push_back({path, static_cast<std::uint32_t>(b.size())});
        bundle.resources.push_back({old + ".absent", 0});
        payloads.emplace(old + ".bndl", Monstey_BuildBundle(bundle));
        J::Array rows;
        unsigned file{};
        for (auto &[path, b] : payloads) {
            auto disk = put("template-" + std::to_string(file++), b);
            auto row = Record(disk, b);
            row["resource_path"] = path;
            rows.push_back(row);
        }
        auto template_file =
            json("source-manifest.json", J::object({{"template", old}, {"resources", rows}}));
        auto candidate = Inst(stem, false), binding = Wdsc(stem, false);
        auto inst_file = put("candidate.inst", candidate), wdsc_file = put("candidate.wldclnt", binding);
        auto proof_file = json("binding.json", J::object({{"inst_sha256", Monstey_HashSha256(candidate)}}));
        auto dep_bytes = B{8, 9, 10};
        auto dep_file = put("texture.bin", dep_bytes);
        auto dep = Record(dep_file, dep_bytes);
        dep["resource_path"] = "prefabs/test.tex";
        auto dependencies = json(
            "dependencies.json",
            J::object({{"resources", J::array({dep})},
                       {"external_dependencies",
                        J::array({J::object({{"resource_path", "prefabs/test.tex0"}, {"bytes", 100}})})}}));
        auto args = J::object({{"template", P(template_file)},
                               {"inst", P(inst_file)},
                               {"dependencies", P(dependencies)},
                               {"wldclnt", P(wdsc_file)},
                               {"binding_proof", P(proof_file)},
                               {"owner_key", 0x1234},
                               {"stem", stem},
                               {"output", P(dir / "component")},
                               {"position", J::array({0, 0, 0})}});
        Case("complete private native section composition", [&] {
            auto result = Monstey_WorldCompositionCall("sow_private_section_build", args);
            Check(result.at("ready_for_package").as_bool() && !result.at("game_written").as_bool(),
                  "private result");
            auto embb = Monstey_ReadEmbedded(Monstey_ReadFile(
                dir / "component/assets" / std::filesystem::path(Monstey_Wide(stem + ".embb"))));
            Check(embb.size() == 7, "private embedded closure count");
            auto final_bundle = Monstey_ReadBundle(Monstey_ReadFile(
                dir / "component/assets" / std::filesystem::path(Monstey_Wide(stem + ".bndl"))));
            Check(final_bundle.resources.size() == 9, "private dependency count");
            auto final_veg = Monstey_ReadFile(dir / "component/assets" /
                                              std::filesystem::path(Monstey_Wide(stem + ".veggrid")));
            Check(std::equal(final_veg.end() - 3, final_veg.end(), veg.end() - 3),
                  "VEGG opaque suffix changed");
            auto final_script = Monstey_ReadFile(dir / "component/assets" /
                                                 std::filesystem::path(Monstey_Wide(stem + ".scripts")));
            Check(std::string(final_script.begin(), final_script.end()).find("section_123") ==
                      std::string::npos,
                  "SCRI symbol was not retargeted");
        });
        Case("deep Unicode composition publishes exact native resources", [&] {
            auto deep = dir / std::filesystem::path(L"composition-\u00e9preuve");
            for (unsigned i = 0; i < 8; ++i)
                deep /= L"long-native-component-folder";
            auto a = args;
            a["output"] = P(deep);
            auto result = Monstey_WorldCompositionCall("sow_private_section_build", a);
            Check(result.at("ready_for_package").as_bool() &&
                      Monstey_ReadFile(deep / "assets" /
                                       std::filesystem::path(Monstey_Wide(stem + ".inst"))) == candidate,
                  "deep component identity differs");
        });
        Case("deep failed native file publication rolls back only new paths", [&] {
            auto deep = dir / std::filesystem::path(L"rollback-\u00e9preuve");
            for (unsigned i = 0; i < 8; ++i)
                deep /= L"long-native-component-folder";
            auto long_stem = std::string("worlds/retail/test/sectionsandlayers/") + std::string(250, 'n');
            auto long_inst = Inst(long_stem, false), long_binding = Wdsc(long_stem, false);
            auto long_inst_file = put("long-candidate.inst", long_inst);
            auto long_binding_file = put("long-candidate.wldclnt", long_binding);
            auto long_proof =
                json("long-binding.json", J::object({{"inst_sha256", Monstey_HashSha256(long_inst)}}));
            auto a = args;
            a["inst"] = P(long_inst_file);
            a["wldclnt"] = P(long_binding_file);
            a["binding_proof"] = P(long_proof);
            a["stem"] = long_stem;
            a["output"] = P(deep);
            Reject([&] { Monstey_WorldCompositionCall("sow_private_section_build", a); });
            Check(!std::filesystem::exists(Monstey_WindowsFilePath(deep)),
                  "failed deep composition left files or directories");
            Check(Monstey_ReadFile(long_inst_file) == long_inst &&
                      Monstey_ReadFile(long_binding_file) == long_binding,
                  "rollback changed a source");
        });
        Case("existing component never overwritten", [&] {
            Reject([&] { Monstey_WorldCompositionCall("sow_private_section_build", args); });
            Check(std::filesystem::exists(dir / "component/resource-manifest.json"),
                  "existing manifest damaged");
        });
        Case("source manifest checksum rejects before publication", [&] {
            auto bad = rows;
            bad[0]["sha256"] = "wrong";
            auto bad_template = json("bad-template.json", J::object({{"template", old}, {"resources", bad}}));
            auto a = args;
            a["template"] = P(bad_template);
            a["output"] = P(dir / "bad-checksum");
            Reject([&] { Monstey_WorldCompositionCall("sow_private_section_build", a); });
            Check(!std::filesystem::exists(dir / "bad-checksum"), "checksum failure published output");
        });
        Case("native WDSC leaf mismatch rejects before publication", [&] {
            auto bad = binding;
            Monstey_PutU32(bad, 44 + Monstey_U32(bad, 40), 0);
            auto bad_path = put("bad-binding.wldclnt", bad);
            auto a = args;
            a["wldclnt"] = P(bad_path);
            a["output"] = P(dir / "bad-binding");
            Reject([&] { Monstey_WorldCompositionCall("sow_private_section_build", a); });
            Check(!std::filesystem::exists(dir / "bad-binding"), "binding failure published output");
        });
        Case("active Lua logic is not silently retargeted", [&] {
            B active{'S', 'C', 'R', 'I', 1, 0, 0, 0, 1};
            std::string body = "local section_123 = {}\n_G.loadedresource = section_123\nprint('active')\n";
            U(active, static_cast<std::uint32_t>(body.size() + 1));
            active.insert(active.end(), body.begin(), body.end());
            active.push_back(0);
            auto disk = put("active.scripts", active);
            auto changed = rows;
            for (auto &row : changed)
                if (row.at("resource_path").as_string().ends_with(".scripts")) {
                    auto path = row.at("resource_path");
                    row = Record(disk, active);
                    row["resource_path"] = path;
                }
            auto manifest =
                json("active-manifest.json", J::object({{"template", old}, {"resources", changed}}));
            auto a = args;
            a["template"] = P(manifest);
            a["output"] = P(dir / "active-component");
            Reject([&] { Monstey_WorldCompositionCall("sow_private_section_build", a); });
            Check(!std::filesystem::exists(dir / "active-component"),
                  "active script retargeting published an invalid section");
        });
        if (argc > 2) {
            auto project = std::filesystem::path(Monstey_Wide(argv[2]));
            Case("real native private section and fourteen-dependency closure", [&] {
                auto world = project / "sow-port/world-edit";
                auto proof_path = world / "private-section-v002/component/proof.json";
                auto proof = J::Parse(Monstey_ReadText(proof_path));
                auto wdsc_manifest =
                    J::Parse(Monstey_ReadText(world / "private-section-v002/wdsc/resource-manifest.json"));
                auto instance_path =
                    std::filesystem::path(Monstey_Wide(proof.at("candidate_inst").at("path").as_string()));
                auto instance_before = Monstey_HashSha256(instance_path);
                auto a =
                    J::object({{"template", P(world / "private-section-v001/source-manifest.json")},
                               {"inst", P(instance_path)},
                               {"dependencies", P(world / "private-section-v002/dependencies.json")},
                               {"wldclnt", wdsc_manifest.at("resources")[0].at("path")},
                               {"binding_proof", P(world / "private-section-v002/wdsc/binding-proof.json")},
                               {"owner_key", proof.at("render_binding_validation").at("owner_key")},
                               {"stem", proof.at("new_stem")},
                               {"output", P(dir / "native-component")},
                               {"position", J::array({0, 0, 0})}});
                auto result = Monstey_WorldCompositionCall("sow_private_section_build", a);
                Check(result.at("resources").size() == 25 && result.at("external_dependencies").size() == 7,
                      "native private closure resources differ");
                auto stem = proof.at("new_stem").as_string();
                auto embb = Monstey_ReadEmbedded(Monstey_ReadFile(
                    dir / "native-component/assets" / std::filesystem::path(Monstey_Wide(stem + ".embb"))));
                Check(embb.size() == 23 && Monstey_HashSha256(instance_path) == instance_before,
                      "native private embed closure count or original instance changed");
                std::cout << "NATIVE_PRIVATE " << result.at("resources").size() << " resources "
                          << embb.size() << " embedded children " << instance_before << '\n';
            });
        }
        std::cout << "RESULT " << passed << " composition groups passed; outputs " << dir.string() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
