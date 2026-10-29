#include "Monstey_Strings.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>

using namespace Monstey;
namespace {
unsigned tests{};
void Check(bool value, const char *text) {
    if (!value)
        throw std::runtime_error(text);
}
template <class F> void Case(const char *name, F &&f) {
    f();
    ++tests;
    std::cout << "PASS " << name << '\n';
}
template <class F> void Reject(F &&f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    Check(rejected, "expected rejection");
}
void A32(Monstey_Bytes &b, std::uint32_t n) {
    for (unsigned j = 0; j < 4; ++j)
        b.push_back(static_cast<std::uint8_t>(n >> (8 * j)));
}
void A16(Monstey_Bytes &b, std::uint16_t n) {
    b.push_back(static_cast<std::uint8_t>(n));
    b.push_back(static_cast<std::uint8_t>(n >> 8));
}
Monstey_Bytes Fixture() {
    // Independent header, two sorted hashes and explicit UTF16 code units.
    constexpr std::uint16_t first[] = {'O', 'r', 'i', 'g', 'i', 'n', 'a', 'l',
                                       ' ', 's', 'w', 'o', 'r', 'd', 0};
    constexpr std::uint16_t second[] = {0x00c9, 'p', 0x00e9, 'e', ' ', 0xd83d, 0xdde1, 0};
    Monstey_Bytes b{'S', 'K', 'D', 'B'};
    for (auto n : {9u, 28u, 2u, 0u, 25u, 0u})
        A32(b, n);
    A32(b, 1);
    A32(b, 0);
    A32(b, 2);
    A32(b, 15);
    for (auto c : first)
        A16(b, c);
    for (auto c : second)
        A16(b, c);
    A16(b, 'x');
    A16(b, 0);
    return b;
}
std::string Text(const Monstey_StringsProject &p, const Monstey_Json &key) {
    auto selector = key.is_string() ? key.as_string() : Monstey_Hex(key.as_uint());
    auto rows = p.Entries(selector).at("items");
    Check(rows.size() == 1, "key not found uniquely");
    return rows[0].at("text").as_string();
}
} // namespace
int main(int argc, char **argv) {
    try {
        auto base =
            argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1])) : std::filesystem::temp_directory_path();
        std::filesystem::create_directories(base);
        auto dir = base / ("Monstey_CppStringsTests_" +
                           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(dir);
        auto put = [&](const std::string &name, const Monstey_Bytes &bytes) {
            auto p = dir / name;
            Monstey_WriteNewFile(p, bytes);
            return p;
        };
        auto raw = Fixture();
        auto source = put("source.strdb", raw);
        auto before = Monstey_HashSha256(source);
        Monstey_StringsProject project(source);
        Case("independent SKDB9 hashes and UTF16 fixture", [&] {
            auto page = project.Entries();
            Check(page.at("total").as_uint() == 2 &&
                      page.at("items")[0].at("hash").as_string() == "0x00000001",
                  "fixture count");
            Check(Text(project, 1) == "Original sword" && Text(project, 2) == "\xc3\x89p\xc3\xa9"
                                                                              "e \xf0\x9f\x97\xa1",
                  "UTF16 decode");
            Check(!project.Describe().at("original_key_names_stored").as_bool(), "invented symbolic names");
        });
        Case("Unicode case-folded search and paging", [&] {
            Check(project.Entries("\xc3\xa9p\xc3\xa9"
                                  "e")
                          .at("total")
                          .as_uint() == 1,
                  "Unicode case fold");
            Check(project.Entries({}, 999).at("items").empty(), "paging overflow");
            Reject([&] { (void)project.Entries({}, 0, 0); });
            Reject([&] { (void)project.Entries({}, 0, 501); });
        });
        Case("no-op build byte exact", [&] {
            auto out = dir / "identity.strdb";
            auto proof = project.Build(out);
            Check(proof.at("no_op_byte_exact").as_bool() && Monstey_ReadFile(out) == raw,
                  "no-op bytes changed");
        });
        Case("add Unicode text and uint32 hash zero", [&] {
            project.Set("Monstey_NewName", "\xc3\x89nergie \xce\xa9 \xf0\x9f\x94\xa5");
            project.Set(0, "");
            auto out = dir / "unicode.strdb";
            auto report = project.Build(out);
            Monstey_StringsProject reread(out);
            Check(Text(reread, 0).empty(), "zero hash or empty text lost");
            Check(Text(reread, Monstey_Hex(Monstey_PathHash("Monstey_NewName"))) ==
                      "\xc3\x89nergie \xce\xa9 \xf0\x9f\x94\xa5",
                  "Unicode roundtrip");
            Check(report.at("original_text_pool_prefix_byte_exact").as_bool(), "prefix proof");
        });
        Case("original pool prefix and unchanged offsets preserved", [&] {
            auto output = Monstey_ReadFile(dir / "unicode.strdb");
            auto pool = 28 + Monstey_U32(output, 12) * 8;
            Check(std::equal(raw.begin() + 44, raw.end(), output.begin() + pool), "pool prefix differs");
            for (std::uint32_t j = 0; j < Monstey_U32(output, 12); ++j)
                if (Monstey_U32(output, 28 + 8ull * j) == 2)
                    Check(Monstey_U32(output, 32 + 8ull * j) == 15, "unchanged offset moved");
        });
        Case("native known collision rejected atomically", [&] {
            Check(Monstey_PathHash("A") == 1 && Monstey_PathHash("Collision_7K8AJ02") == 1,
                  "collision fixture");
            project.Set("A", "known key");
            auto changes = project.Changes();
            Reject([&] { project.Set("Collision_7K8AJ02", "collision"); });
            Check(changes == project.Changes(), "collision changed project");
            project.Set("a", "same case key");
            Check(Text(project, 1) == "same case key", "equivalent key rejected");
        });
        Case("rename transfers text and rejects occupied target", [&] {
            Reject([&] { project.Rename("A", "B"); });
            project.Rename("A", "RenamedKey");
            Check(project.Entries("0x00000001").at("items").empty() &&
                      Text(project, "RenamedKey") == "same case key",
                  "rename failure");
        });
        Case("remove, undo, redo and revert", [&] {
            project.Remove("RenamedKey");
            Check(project.Entries("RenamedKey").at("items").empty(), "remove failed");
            project.Undo();
            Check(Text(project, "RenamedKey") == "same case key", "undo failed");
            project.Redo();
            Check(project.Entries("RenamedKey").at("items").empty(), "redo failed");
            project.Revert();
            Check(!project.Describe().at("dirty").as_bool(), "revert failed");
            project.Undo();
            Check(project.Describe().at("dirty").as_bool(), "revert undo failed");
        });
        Case("save/load source and new-table projects", [&] {
            project.Set("Saved_Key", "Saved \xce\xa9");
            auto saved = dir / "source.sowstrproj.json";
            project.SaveProject(saved);
            auto loaded = Monstey_StringsProject::LoadProject(saved);
            Check(Text(*loaded, "Saved_Key") == "Saved \xce\xa9", "source project restore");
            loaded->Build(dir / "restored.strdb");
            Monstey_StringsProject fresh;
            fresh.Set("Fresh", "New");
            fresh.SaveProject(dir / "fresh.sowstrproj.json");
            auto fresh_loaded = Monstey_StringsProject::LoadProject(dir / "fresh.sowstrproj.json");
            Check(Text(*fresh_loaded, "Fresh") == "New", "fresh project restore");
        });
        Case("reject negative, boolean and oversized hash keys", [&] {
            for (auto key : {Monstey_Json(-1), Monstey_Json(true), Monstey_Json(std::uint64_t{0x100000000}),
                             Monstey_Json("0xzz"), Monstey_Json("0x"), Monstey_Json("\xc3\xa9")})
                Reject([&] { project.Set(key, "bad"); });
        });
        Case("reject NUL and malformed UTF8 text", [&] {
            Reject([&] { project.Set("Bad", std::string("head\0tail", 9)); });
            Reject([&] { project.Set("Bad", std::string("\xed\xa0\x80", 3)); });
            Reject([&] { project.Set("Bad", std::string("\xf0\x80\x80\x80", 4)); });
        });
        Case("shared UTF16 offsets and empty string", [&] {
            auto same = raw;
            Monstey_PutU32(same, 40, 0);
            auto path = put("shared.strdb", same);
            Monstey_StringsProject alias(path);
            Check(Text(alias, 1) == Text(alias, 2), "shared offsets failed");
            auto report = alias.Build(dir / "shared-noop.strdb");
            Check(report.at("no_op_byte_exact").as_bool(), "shared no-op changed");
        });
        Case("missing UTF16 terminator rejected", [&] {
            auto bad = raw;
            Monstey_PutU32(bad, 40, 24);
            bad[bad.size() - 2] = 'q';
            Reject([&] { Monstey_StringsProject invalid(put("unterminated.strdb", bad)); });
        });
        Case("unpaired UTF16 high surrogate rejected", [&] {
            auto bad = raw;
            bad[44] = 0;
            bad[45] = 0xd8;
            Reject([&] { Monstey_StringsProject invalid(put("highsurrogate.strdb", bad)); });
        });
        Case("offset into surrogate pair rejected", [&] {
            auto bad = raw;
            Monstey_PutU32(bad, 40, 21);
            Reject([&] { Monstey_StringsProject invalid(put("lowoffset.strdb", bad)); });
        });
        Case("duplicate and unsorted hash rows rejected", [&] {
            auto bad = raw;
            Monstey_PutU32(bad, 36, 1);
            Reject([&] { Monstey_StringsProject invalid(put("duplicate.strdb", bad)); });
            bad = raw;
            Monstey_PutU32(bad, 28, 3);
            Reject([&] { Monstey_StringsProject invalid(put("unsorted.strdb", bad)); });
        });
        Case("truncated counts and outside offsets rejected", [&] {
            auto bad = raw;
            Monstey_PutU32(bad, 20, 0xffffffff);
            Reject([&] { Monstey_StringsProject invalid(put("badcount.strdb", bad)); });
            bad = raw;
            Monstey_PutU32(bad, 40, 25);
            Reject([&] { Monstey_StringsProject invalid(put("badoffset.strdb", bad)); });
        });
        Case("new empty table and empty-text roundtrip", [&] {
            Monstey_StringsProject empty;
            auto proof = empty.Build(dir / "empty.strdb");
            Check(proof.at("entries").as_uint() == 0 && Monstey_ReadFile(dir / "empty.strdb").size() == 28,
                  "empty build");
            empty.Set(0, "");
            empty.Build(dir / "emptytext.strdb");
            Monstey_StringsProject loaded(dir / "emptytext.strdb");
            Check(Text(loaded, 0).empty(), "empty-text build");
        });
        Case("no-op symbolic annotations do not alter native bytes", [&] {
            Monstey_StringsProject annotations(source);
            annotations.Set("A", "Original sword");
            Check(!annotations.Describe().at("dirty").as_bool() &&
                      annotations.Describe().at("annotations_dirty").as_bool(),
                  "annotation dirty classification");
            annotations.Build(dir / "annotations.strdb");
            Check(Monstey_ReadFile(dir / "annotations.strdb") == raw, "annotation changed native bytes");
        });
        Case("existing output and source cannot be overwritten", [&] {
            Reject([&] { project.Build(source); });
            Check(Monstey_HashSha256(source) == before, "source overwritten");
            auto output = put("existing.strdb", {'k', 'e', 'e', 'p'});
            Reject([&] { project.Build(output); });
            Check(Monstey_ReadFile(output) == Monstey_Bytes({'k', 'e', 'e', 'p'}), "existing output changed");
        });
        Case("source compare-and-swap and loaded project provenance", [&] {
            auto changing = put("changing.strdb", raw);
            Monstey_StringsProject stale(changing);
            stale.SaveProject(dir / "stale.sowstrproj.json");
            auto changed = raw;
            changed[44] = 'X';
            std::ofstream f(changing, std::ios::binary | std::ios::trunc);
            f.write(reinterpret_cast<const char *>(changed.data()),
                    static_cast<std::streamsize>(changed.size()));
            f.close();
            Reject([&] { stale.Set("New", "bad"); });
            Reject([&] { stale.Build(dir / "stale.strdb"); });
            Reject([&] { (void)Monstey_StringsProject::LoadProject(dir / "stale.sowstrproj.json"); });
        });
        Case("project annotation hash tampering rejected", [&] {
            auto saved = Monstey_Json::Parse(Monstey_ReadText(dir / "source.sowstrproj.json"));
            saved["changes"]["keys"]["0x00000001"] = "WrongKey";
            Monstey_WriteNewText(dir / "tampered.sowstrproj.json", saved.dump());
            Reject([&] { (void)Monstey_StringsProject::LoadProject(dir / "tampered.sowstrproj.json"); });
        });
        Case("game directory output prohibited", [&] {
            auto game = dir / "fake-game";
            std::filesystem::create_directory(game);
            Monstey_WriteNewFile(game / "ShadowOfWar.exe", Monstey_Bytes{'m'});
            Reject([&] { project.Build(game / "forbidden.strdb"); });
            Check(!std::filesystem::exists(game / "forbidden.strdb"), "game file created");
        });
        if (argc > 2) {
            auto native = std::filesystem::path(Monstey_Wide(argv[2]));
            Case("native SKDB9 no-op and one-row Unicode edit", [&] {
                auto native_before = Monstey_HashSha256(native);
                Monstey_StringsProject real(native);
                auto description = real.Describe();
                Check(description.at("entries").as_uint() > 50000, "native table unexpectedly small");
                auto first = real.Entries({}, 0, 1).at("items")[0].at("hash");
                auto identity = real.Build(dir / "native-noop.strdb");
                Check(identity.at("no_op_byte_exact").as_bool() &&
                          Monstey_HashSha256(dir / "native-noop.strdb") == native_before,
                      "native no-op mismatch");
                real.Set(first, "Monstey native Unicode \xc3\x89 \xce\xa9 \xf0\x9f\x94\xa5");
                auto changed = real.Build(dir / "native-edited.strdb");
                Check(changed.at("unchanged_entries_verified").as_uint() + 1 ==
                          description.at("entries").as_uint(),
                      "native unchanged count mismatch");
                Monstey_StringsProject check(dir / "native-edited.strdb");
                Check(Text(check, first) == "Monstey native Unicode \xc3\x89 \xce\xa9 \xf0\x9f\x94\xa5",
                      "native Unicode edit failed");
                Check(Monstey_HashSha256(native) == native_before, "native source changed");
                std::cout << "NATIVE " << description.at("entries").as_uint() << " entries " << native_before
                          << '\n';
            });
        }
        Check(Monstey_HashSha256(source) == before, "fixture source modified");
        std::cout << "RESULT " << tests << " tests passed; synthetic/native readback outputs " << dir.string()
                  << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
