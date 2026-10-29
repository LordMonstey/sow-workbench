#include <windows.h>

#include "Monstey_Catalog.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>
using namespace Monstey;
namespace
{
using J = Monstey_Json;
using B = Monstey_Bytes;
std::uint64_t Monstey_TestCounter = 0;
unsigned Monstey_TestPassed = 0;
void Monstey_Check(bool ok, const std::string &message)
{
    if (!ok)
        throw std::runtime_error(message);
}
void Monstey_Reject(const std::function<void()> &operation)
{
    bool rejected = false;
    try
    {
        operation();
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    Monstey_Check(rejected, "Expected rejection");
}
void Monstey_Case(const char *name, const std::function<void()> &operation)
{
    operation();
    ++Monstey_TestPassed;
    std::cout << "PASS " << name << '\n';
}
std::string Monstey_PathString(const std::filesystem::path &path)
{
    return Monstey_Utf8(std::filesystem::absolute(path).wstring());
}
void Monstey_Update(const std::filesystem::path &path, const std::string &text)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    Monstey_Check(stream.good(), "Fixture update failed");
}
B Monstey_Embb(const std::vector<std::pair<std::string, B>> &files, std::uint32_t version = 5)
{
    B names;
    std::vector<std::uint32_t> offsets;
    for (const auto &[name, payload] : files)
    {
        (void)payload;
        offsets.push_back(static_cast<std::uint32_t>(names.size()));
        std::string path = name;
        std::replace(path.begin(), path.end(), '/', '\\');
        names.insert(names.end(), path.begin(), path.end());
        names.push_back(0);
    }
    B bytes = {'E', 'M', 'B', 'B'};
    Monstey_AppendU32(bytes, version);
    Monstey_AppendU32(bytes, static_cast<std::uint32_t>(names.size()));
    Monstey_AppendU32(bytes, static_cast<std::uint32_t>(files.size()));
    bytes.insert(bytes.end(), names.begin(), names.end());
    for (auto pointer : offsets)
        Monstey_AppendU32(bytes, pointer);
    for (const auto &[name, payload] : files)
    {
        (void)name;
        Monstey_AppendU32(bytes, static_cast<std::uint32_t>(payload.size()));
        bytes.insert(bytes.end(), payload.begin(), payload.end());
    }
    return bytes;
}
struct Monstey_Fixture
{
    std::filesystem::path root, game, config;
    explicit Monstey_Fixture(const std::filesystem::path &parent)
        : root(parent / (L"case-" + std::to_wstring(++Monstey_TestCounter))), game(root / L"game"),
          config(game / L"x64/default.archcfg")
    {
        std::filesystem::create_directories(game / L"x64");
        Monstey_WriteNewText(config, "");
    }
    std::filesystem::path Put(const std::string &name, const B &bytes)
    {
        const auto p = root / std::filesystem::path(Monstey_Wide(name));
        Monstey_WriteNewFile(p, bytes);
        return p;
    }
    std::filesystem::path Archive(const std::string &name, const std::vector<std::pair<std::string, B>> &entries)
    {
        Monstey_ArchiveProject project;
        for (const auto &[path, bytes] : entries)
        {
            const auto input = Put("input-" + std::to_string(++Monstey_TestCounter) + ".bin", bytes);
            project.AddFile(path, input);
        }
        const auto output = game / std::filesystem::path(Monstey_Wide(name));
        project.Build(output, false);
        return output;
    }
    void Config(const std::string &text)
    {
        Monstey_Update(config, text);
    }
    std::filesystem::path Output(const std::string &name = "catalog.jsonl") const
    {
        return root / std::filesystem::path(Monstey_Wide(name));
    }
};
B Monstey_ManyArchive(std::uint32_t count)
{
    B names(1);
    std::vector<std::uint32_t> pointers;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const auto name = "item" + std::to_string(i) + ".bin";
        pointers.push_back(static_cast<std::uint32_t>(names.size()));
        names.insert(names.end(), name.begin(), name.end());
        names.push_back(0);
    }
    const auto extent = 48 + static_cast<std::uint64_t>(names.size()) + 30ull * count + 16;
    B out = {'L', 'T', 'A', 'R'};
    for (auto word : {4u, static_cast<std::uint32_t>(names.size()), 1u, count, 0u, 0u, 0u, 0u, 0u, 0u, 0u})
        Monstey_AppendU32(out, word);
    out.insert(out.end(), names.begin(), names.end());
    for (auto pointer : pointers)
    {
        Monstey_AppendU32(out, pointer);
        Monstey_AppendU64(out, extent);
        Monstey_AppendU64(out, 12);
        Monstey_AppendU64(out, 1);
        out.push_back(1);
        out.push_back(0);
    }
    for (auto word : {0u, 0xffffffffu, 0xffffffffu, count})
        Monstey_AppendU32(out, word);
    Monstey_AppendU32(out, 1);
    Monstey_AppendU32(out, 1);
    out.push_back('x');
    out.insert(out.end(), 3, 0);
    return out;
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        if (argc >= 8 && std::wstring_view(argv[1]) == L"--read-embedded-range")
        {
            const auto result = Monstey_ReadEmbeddedResourceRange(argv[2], Monstey_Utf8(argv[3]), Monstey_Utf8(argv[4]),
                                                                  J::Parse(Monstey_Utf8(argv[5])).as_uint(),
                                                                  J::Parse(Monstey_Utf8(argv[6])).as_uint(), argv[7]);
            if (argc > 8)
                Monstey_WriteNewText(argv[8], result.dump(2) + "\n");
            std::cout << result.dump() << '\n';
            return 0;
        }
        if (argc >= 7 && std::wstring_view(argv[1]) == L"--extract-embedded")
        {
            const auto result = Monstey_ExtractEmbeddedResource(argv[2], Monstey_Utf8(argv[3]), Monstey_Utf8(argv[4]),
                                                                argv[5], argv[6]);
            if (argc > 7)
                Monstey_WriteNewText(argv[7], result.dump(2) + "\n");
            std::cout << result.dump() << '\n';
            return 0;
        }
        const auto parent = argc > 1 ? std::filesystem::path(argv[1])
                                     : std::filesystem::temp_directory_path() / L"Monstey-Catalog-Tests";
        const auto root =
            parent / (L"run-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        std::filesystem::create_directories(root);
        Monstey_Case("physical mount lines and later loose source win", [&] {
            Monstey_Fixture f(root);
            const auto base = f.Archive("base.arch06", {{"same.bin", {'a'}}, {"base.bin", {'b'}}});
            const auto patch = f.Archive("patch.arch06", {{"same.bin", {'p'}}});
            std::filesystem::create_directories(f.game / L"x64/Live");
            Monstey_WriteNewText(f.game / L"x64/Live/same.bin", "live");
            f.Config("\"..\\base.arch06\" ; base\n; comment\n\n..\\patch.arch06\nLive\n");
            const auto before = Monstey_HashSha256(base), patchBefore = Monstey_HashSha256(patch);
            const auto mounts = Monstey_ArchiveMounts(f.game);
            Monstey_Check(mounts.at("mounts").at(1).at("priority").as_int() == 4, "Physical config line priority lost");
            const auto report = Monstey_ArchiveCatalog(f.game, f.Output());
            Monstey_Check(report.at("complete").as_bool() && report.at("rows").as_int() == 4,
                          "Native mount catalog count");
            const auto variants = Monstey_ReadArchiveCatalog(f.Output(), J::object({{"path", "SAME.BIN"}}));
            Monstey_Check(variants.at("rows").size() == 3, "All source variants not retained");
            const auto winner = Monstey_ReadArchiveCatalog(
                f.Output(), J::object({{"path", "same.bin"}, {"winning_only", true}, {"verify_mounts", true}}));
            Monstey_Check(winner.at("rows").size() == 1 && winner.at("rows").at(0).at("kind").as_string() == "loose",
                          "Later loose mount does not win");
            Monstey_Check(winner.at("rows").at(0).at("path_hash").as_uint() == Monstey_PathHash("same.bin"),
                          "Wrong native path hash");
            Monstey_Check(Monstey_HashSha256(base) == before && Monstey_HashSha256(patch) == patchBefore,
                          "Input archive changed");
        });
        Monstey_Case("default catalog never decodes embedded parent payloads", [&] {
            Monstey_Fixture f(root);
            f.Archive("base.arch06", {{"bundles/bad.embb", {'b', 'a', 'd'}}});
            f.Config("..\\base.arch06\n");
            const auto report = Monstey_ArchiveCatalog(f.game, f.Output());
            Monstey_Check(report.at("complete").as_bool() && report.at("counts").at("parents_checked").as_int() == 0,
                          "Direct-only indexing touched embedded payload");
        });
        Monstey_Case("bounded EMBB ranges retain children and source provenance", [&] {
            Monstey_Fixture f(root);
            const auto raw =
                Monstey_Embb({{"assets/large.bin", B(2 * 1024 * 1024, 'X')}, {"assets/small.bin", {'e', 'n', 'd'}}});
            f.Archive("base.arch06", {{"bundles/test.embb", raw}});
            f.Config("..\\base.arch06\n");
            const auto report = Monstey_ArchiveCatalog(f.game, f.Output(), J::object({{"include_embedded", true}}));
            Monstey_Check(report.at("complete").as_bool() && report.at("counts").at("embedded").as_int() == 2,
                          "EMBB metadata incomplete");
            const auto rows = Monstey_ReadArchiveCatalog(f.Output()).at("rows");
            const auto &parentRow = rows.at(0);
            Monstey_Check(parentRow.at("embedded_ranges").at("metadata_bytes_read").as_uint() < 4096,
                          "Child payload was read as metadata");
            Monstey_Check(parentRow.at("embedded_ranges").at("decoded_chunk_loads").as_uint() <
                              parentRow.at("embedded_ranges").at("chunks").as_uint(),
                          "Entire embedded parent was decoded");
            Monstey_Check(rows.at(1).at("bundle").as_string() == "bundles/test.embb" &&
                              rows.at(1).at("offset_space").as_string() == "decoded_parent",
                          "EMBB parent association lost");
        });
        Monstey_Case("embedded winning is parent scoped, including absent replacement children", [&] {
            Monstey_Fixture f(root);
            f.Archive("base.arch06",
                      {{"bundles/test.embb", Monstey_Embb({{"shared.bin", {'a'}}, {"old-only.bin", {'o'}}})},
                       {"bundles/other.embb", Monstey_Embb({{"shared.bin", {'b'}}})}});
            f.Archive("patch.arch06", {{"bundles/test.embb", Monstey_Embb({{"shared.bin", {'p'}}})}});
            f.Config("..\\base.arch06\n..\\patch.arch06\n");
            Monstey_ArchiveCatalog(f.game, f.Output(), J::object({{"include_embedded", true}}));
            const auto shared =
                Monstey_ReadArchiveCatalog(f.Output(), J::object({{"path", "shared.bin"}, {"winning_only", true}}));
            Monstey_Check(shared.at("rows").size() == 2, "Children from different native parents collapsed");
            const auto removed =
                Monstey_ReadArchiveCatalog(f.Output(), J::object({{"path", "old-only.bin"}, {"winning_only", true}}));
            Monstey_Check(removed.at("rows").empty(), "Child from shadowed parent incorrectly wins");
        });
        Monstey_Case("malformed child extent produces explicit partial catalog", [&] {
            Monstey_Fixture f(root);
            auto bytes = Monstey_Embb({{"a.bin", {'a'}}});
            const auto offset = 16 + Monstey_U32(bytes, 8) + 4;
            Monstey_PutU32(bytes, offset, 0xffffffffu);
            f.Archive("base.arch06", {{"bad.embb", bytes}});
            f.Config("..\\base.arch06\n");
            const auto report = Monstey_ArchiveCatalog(f.game, f.Output(), J::object({{"include_embedded", true}}));
            Monstey_Check(!report.at("complete").as_bool() && report.at("failed_count").as_int() == 1 &&
                              report.at("rows").as_int() == 1,
                          "Malformed parent hidden as complete");
            Monstey_Check(
                Monstey_ReadArchiveCatalog(f.Output()).at("rows").at(0).at("embedded_index_status").as_string() ==
                    "failed",
                "Partial parent reason absent");
        });
        Monstey_Case("unsafe duplicate and unknown-version EMBB inputs rejected per parent", [&] {
            for (const auto &bytes :
                 {Monstey_Embb({{"../escape.bin", {'x'}}}), Monstey_Embb({{"same.bin", {'x'}}, {"SAME.BIN", {'y'}}}),
                  Monstey_Embb({{"ok.bin", {'x'}}}, 6)})
            {
                Monstey_Fixture f(root);
                f.Archive("base.arch06", {{"bad.embb", bytes}});
                f.Config("..\\base.arch06\n");
                const auto report = Monstey_ArchiveCatalog(f.game, f.Output(), J::object({{"include_embedded", true}}));
                Monstey_Check(!report.at("complete").as_bool() && report.at("counts").at("embedded").as_int() == 0,
                              "Unsafe metadata accepted");
            }
        });
        Monstey_Case("unsafe archive chunks cannot be indexed as complete embedded metadata", [&] {
            Monstey_Fixture f(root);
            const auto original = f.Archive("base.arch06", {{"test.embb", Monstey_Embb({{"x.bin", {'x'}}})}});
            const auto archive = Monstey_Archive(original);
            auto bytes = Monstey_ReadFile(original);
            Monstey_PutU32(bytes, static_cast<std::size_t>(archive.Entries().at(0).offset + 4), 65537);
            Monstey_WriteNewFile(f.game / L"bad.arch06", bytes);
            f.Config("..\\bad.arch06\n");
            const auto report = Monstey_ArchiveCatalog(f.game, f.Output(), J::object({{"include_embedded", true}}));
            Monstey_Check(!report.at("complete").as_bool(), "Invalid chunk raw length accepted");
        });
        Monstey_Case("nested embedded metadata stays explicit and unexpanded", [&] {
            Monstey_Fixture f(root);
            f.Archive("base.arch06",
                      {{"outer.embb", Monstey_Embb({{"nested.embb", Monstey_Embb({{"inside.bin", {'i'}}})}})}});
            f.Config("..\\base.arch06\n");
            const auto report = Monstey_ArchiveCatalog(f.game, f.Output(), J::object({{"include_embedded", true}}));
            Monstey_Check(!report.at("complete").as_bool() &&
                              report.at("failed").at(0).at("kind").as_string() == "nested_embedded",
                          "Nested metadata limitation hidden");
            Monstey_Check(
                Monstey_ReadArchiveCatalog(f.Output(), J::object({{"path", "inside.bin"}})).at("rows").empty(),
                "Nested child invented");
        });
        Monstey_Case("loose EMBB metadata and external mount output protection", [&] {
            Monstey_Fixture f(root);
            const auto loose = f.root / L"external";
            std::filesystem::create_directories(loose);
            Monstey_WriteNewFile(loose / L"test.embb", Monstey_Embb({{"child.bin", {'c'}}}));
            f.Config("..\\..\\external\n");
            const auto report = Monstey_ArchiveCatalog(f.game, f.Output(), J::object({{"include_embedded", true}}));
            Monstey_Check(report.at("complete").as_bool() && report.at("counts").at("embedded").as_int() == 1,
                          "Loose EMBB child missing");
            Monstey_Reject([&] { Monstey_ArchiveCatalog(f.game, loose / L"forbidden.jsonl"); });
            Monstey_Check(!std::filesystem::exists(loose / L"forbidden.jsonl"),
                          "Catalog written inside external resource mount");
        });
        Monstey_Case("archive substring, glob and bracket filters preserve priorities", [&] {
            Monstey_Fixture f(root);
            f.Archive("base.arch06", {{"x.bin", {'x'}}});
            f.Config("..\\base.arch06\n..\\missing.arch06\n");
            for (const auto &filter : {"base.arch06", "bas*.arch06", "b[!x]se.arch0?"})
            {
                const auto output = f.Output("filtered-" + std::to_string(++Monstey_TestCounter) + ".jsonl");
                const auto report = Monstey_ArchiveCatalog(f.game, output, J::object({{"archive_filter", filter}}));
                Monstey_Check(report.at("complete").as_bool() && report.at("rows").as_int() == 1,
                              "Filter selected wrong mount");
            }
            Monstey_Reject(
                [&] { Monstey_ArchiveCatalog(f.game, f.Output(), J::object({{"archive_filter", "no-match*"}})); });
            const auto partial = Monstey_ArchiveCatalog(f.game, f.Output("missing.jsonl"));
            Monstey_Check(!partial.at("complete").as_bool() && partial.at("missing_mounts").size() == 1,
                          "Missing mount hidden");
        });
        Monstey_Case("catalog output source and existing files protected", [&] {
            Monstey_Fixture f(root);
            f.Archive("base.arch06", {{"x.bin", {'x'}}});
            f.Config("..\\base.arch06\n");
            Monstey_Reject([&] { Monstey_ArchiveCatalog(f.game, f.game / L"forbidden.jsonl"); });
            Monstey_WriteNewText(f.Output(), "keep");
            Monstey_Reject([&] { Monstey_ArchiveCatalog(f.game, f.Output()); });
            Monstey_Check(Monstey_ReadText(f.Output()) == "keep", "Existing catalog overwritten");
        });
        Monstey_Case("reusable query checksum and mount freshness checks", [&] {
            Monstey_Fixture f(root);
            f.Archive("base.arch06", {{"x.bin", {'x'}}});
            f.Config("..\\base.arch06\n");
            const auto report = Monstey_ArchiveCatalog(f.game, f.Output());
            Monstey_Check(Monstey_ReadArchiveCatalog(f.Output(), J::object({{"query", "X.BIN"},
                                                                            {"expected_sha256", report.at("sha256")},
                                                                            {"verify_mounts", true}}))
                                  .at("total")
                                  .as_int() == 1,
                          "Saved catalog query failed");
            f.Config("..\\base.arch06\n; changed\n");
            Monstey_Reject([&] { Monstey_ReadArchiveCatalog(f.Output(), J::object({{"verify_mounts", true}})); });
            Monstey_Check(Monstey_ReadArchiveCatalog(f.Output()).at("rows").size() == 1,
                          "Historical catalog cannot be read explicitly");
            auto stream = std::ofstream(f.Output(), std::ios::binary | std::ios::app);
            stream << "{}\n";
            stream.close();
            Monstey_Reject([&] { Monstey_ReadArchiveCatalog(f.Output()); });
        });
        Monstey_Case("JSONL row bounds and native path hashes checked", [&] {
            Monstey_Fixture f(root);
            const auto row = J::object(
                {{"path", "x.bin"}, {"priority", 1}, {"kind", "loose"}, {"file", "fixture"}, {"path_hash", 123}});
            Monstey_WriteNewText(f.Output(), row.dump() + "\n");
            Monstey_Reject([&] { Monstey_ReadArchiveCatalog(f.Output()); });
            Monstey_WriteNewText(f.Output("oversized.jsonl"), std::string(1024 * 1024 + 4, 'x'));
            Monstey_Reject([&] { Monstey_ReadArchiveCatalog(f.Output("oversized.jsonl")); });
        });
        Monstey_Case("legacy settings metadata and runtime-only configure", [&] {
            Monstey_Fixture f(root);
            const auto legacy = f.root / L"legacy-converter";
            std::filesystem::create_directories(legacy);
            const auto blender = f.Put("blender-fake.exe", {'n', 'o', 't', '-', 'e', 'x', 'e'});
            Monstey_Settings settings(f.root / L"settings.json");
            const auto result = settings.Configure(J::object({{"game_root", Monstey_PathString(f.game)},
                                                              {"blender", Monstey_PathString(blender)},
                                                              {"converter_root", Monstey_PathString(legacy)},
                                                              {"save", false}}));
            Monstey_Check(result.at("converter_available").as_bool() && !result.at("python_runtime").as_bool() &&
                              result.at("blender_available").as_bool(),
                          "Native components missing");
            Monstey_Check(!std::filesystem::exists(f.root / L"settings.json"),
                          "Runtime-only configuration wrote settings");
            Monstey_Check(!std::filesystem::exists(legacy / L"scripts/engine_entry.py"),
                          "Private Python prerequisite introduced");
        });
        Monstey_Case("settings new save atomic update and source-compatible reload", [&] {
            Monstey_Fixture f(root);
            const auto path = f.root / L"settings.json";
            Monstey_Settings settings(path);
            settings.Configure(J::object({{"game_root", Monstey_PathString(f.game)}}));
            const auto before = Monstey_HashSha256(path);
            const auto blender = f.Put("blender.exe", {'x'});
            settings.Configure(J::object({{"blender", Monstey_PathString(blender)}}));
            Monstey_Check(Monstey_HashSha256(path) != before, "Settings did not update");
            Monstey_Settings loaded(path);
            Monstey_Check(loaded.Describe().at("settings_saved").as_bool() &&
                              loaded.Describe().at("game_root") == settings.Describe().at("game_root"),
                          "Native settings reload differs");
            Monstey_Check(J::Parse(Monstey_ReadText(path)).at("schema").as_string() == "Monstey.WorkbenchSettings/1",
                          "Legacy schema changed");
        });
        Monstey_Case("settings invalid changes leave runtime and disk unchanged", [&] {
            Monstey_Fixture f(root);
            Monstey_Settings settings(f.root / L"settings.json");
            settings.Configure(J::object({{"game_root", Monstey_PathString(f.game)}}));
            const auto before = settings.Describe();
            const auto hash = Monstey_HashSha256(f.root / L"settings.json");
            Monstey_Reject(
                [&] { settings.Configure(J::object({{"blender", Monstey_PathString(f.root / L"missing.exe")}})); });
            Monstey_Reject([&] { settings.Configure(J::object({{"save", 1}})); });
            Monstey_Reject([&] { settings.Configure(J::object({{"script", "do-not-run"}})); });
            Monstey_Check(before == settings.Describe() && Monstey_HashSha256(f.root / L"settings.json") == hash,
                          "Rejected settings changed state");
        });
        Monstey_Case("settings external changes rejected without overwrite", [&] {
            Monstey_Fixture f(root);
            const auto path = f.root / L"settings.json";
            Monstey_Settings settings(path);
            settings.Configure(J::object({{"game_root", Monstey_PathString(f.game)}}));
            Monstey_Update(path, "{\"schema\":\"Monstey.WorkbenchSettings/1\",\"external\":true}");
            const auto changed = Monstey_HashSha256(path);
            Monstey_Reject([&] { settings.Configure(J::object()); });
            Monstey_Check(Monstey_HashSha256(path) == changed, "External settings overwritten");
        });
        Monstey_Case("settings locked target preserves original transaction", [&] {
            Monstey_Fixture f(root);
            const auto path = f.root / L"settings.json";
            Monstey_Settings settings(path);
            settings.Configure(J::object({{"game_root", Monstey_PathString(f.game)}}));
            const auto old = Monstey_HashSha256(path);
            const auto handle =
                CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            Monstey_Check(handle != INVALID_HANDLE_VALUE, "Cannot lock fixture settings");
            const auto blender = f.Put("blender.exe", {'x'});
            Monstey_Reject([&] { settings.Configure(J::object({{"blender", Monstey_PathString(blender)}})); });
            CloseHandle(handle);
            Monstey_Check(Monstey_HashSha256(path) == old && settings.Describe().at("blender").is_null(),
                          "Failed publication changed settings");
        });
        Monstey_Case("settings source and game destinations protected", [&] {
            Monstey_Fixture f(root);
            const auto path = f.root / L"schema.json";
            Monstey_WriteNewText(path, "{\"schema\":\"Monstey.WorkbenchSettings/1\"}");
            const auto before = Monstey_HashSha256(path);
            Monstey_Settings settings(path, {path});
            Monstey_Reject([&] { settings.Configure(J::object({{"game_root", Monstey_PathString(f.game)}})); });
            Monstey_Check(Monstey_HashSha256(path) == before, "Selected schema overwritten");
            Monstey_Settings inside(f.game / L"settings.json");
            Monstey_Reject([&] { inside.Configure(J::object({{"game_root", Monstey_PathString(f.game)}})); });
            Monstey_Check(!std::filesystem::exists(f.game / L"settings.json"), "Settings written inside selected game");
        });
        Monstey_Case("malformed settings are visible and do not block runtime-only recovery", [&] {
            Monstey_Fixture f(root);
            const auto path = f.root / L"settings.json";
            Monstey_WriteNewText(path, "bad");
            Monstey_Settings settings(path);
            Monstey_Check(settings.Describe().at("configuration_error").is_string(), "Bad configuration error hidden");
            settings.Configure(J::object({{"game_root", Monstey_PathString(f.game)}, {"save", false}}));
            Monstey_Check(settings.Describe().at("configuration_error").is_null() && Monstey_ReadText(path) == "bad",
                          "Runtime-only recovery overwrote original");
        });
        Monstey_Case("same-stamp archive index mutation is caught before publication", [&] {
            Monstey_Fixture f(root);
            const auto source = f.game / L"many.arch06";
            Monstey_WriteNewFile(source, Monstey_ManyArchive(12000));
            f.Config("..\\many.arch06\n");
            const auto oldTime = std::filesystem::last_write_time(source);
            std::atomic<bool> stop = false, changed = false;
            std::thread mutate([&] {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
                while (!stop && std::chrono::steady_clock::now() < deadline)
                {
                    for (const auto &entry : std::filesystem::directory_iterator(f.root))
                    {
                        std::error_code error;
                        if (entry.path().filename().wstring().starts_with(L".Monstey-Catalog-") &&
                            entry.file_size(error) > 0 && !error)
                        {
                            std::fstream file(source, std::ios::binary | std::ios::in | std::ios::out);
                            file.seekp(49);
                            file.put('j');
                            file.flush();
                            file.close();
                            std::filesystem::last_write_time(source, oldTime);
                            changed = true;
                            return;
                        }
                    }
                    std::this_thread::yield();
                }
            });
            bool rejected = false;
            try
            {
                Monstey_ArchiveCatalog(f.game, f.Output());
            }
            catch (const std::exception &)
            {
                rejected = true;
            }
            stop = true;
            mutate.join();
            Monstey_Check(changed && rejected, "Source-CAS mutation was not observed/rejected");
            Monstey_Check(!std::filesystem::exists(f.Output()), "Changed source left a published catalog");
        });
        Monstey_Case("bounded selected embedded extraction reuses its native parent", [&] {
            Monstey_Fixture f(root);
            const auto parent =
                Monstey_Embb({{"large.bin", B(2 * 1024 * 1024, 'X')}, {"selected.bin", {'e', 'n', 'd'}}});
            const auto archive = f.Archive("base.arch06", {{"bundles/test.embb", parent}});
            const auto before = Monstey_HashSha256(archive);
            const auto output = f.root / L"child.bin";
            const auto result = Monstey_ExtractEmbeddedResource(archive, "bundles/test.embb", "SELECTED.BIN", output);
            Monstey_Check(Monstey_ReadText(output) == "end" && result.at("bytes").as_uint() == 3 &&
                              !result.at("parent_copied").as_bool(),
                          "Selected child extraction differs");
            Monstey_Check(Monstey_HashSha256(archive) == before && result.at("source_unchanged").as_bool(),
                          "Embedded extraction changed parent");
            Monstey_Reject(
                [&] { Monstey_ExtractEmbeddedResource(archive, "bundles/test.embb", "selected.bin", output); });
            Monstey_Check(Monstey_ReadText(output) == "end", "Existing child output overwritten");
            Monstey_Reject([&] {
                Monstey_ExtractEmbeddedResource(archive, "bundles/test.embb", "missing.bin", f.root / L"missing.bin");
            });
            Monstey_Check(!std::filesystem::exists(f.root / L"missing.bin"), "Missing child left output");
        });
        Monstey_Case("loose embedded child streams and game output stays protected", [&] {
            Monstey_Fixture f(root);
            const auto parent = f.Put("loose.embb", Monstey_Embb({{"child.bin", B(1024 * 1024 + 7, 'Y')}}));
            const auto result = Monstey_ExtractEmbeddedResource(parent, "", "child.bin", f.root / L"child.bin");
            Monstey_Check(Monstey_ReadFile(f.root / L"child.bin") == B(1024 * 1024 + 7, 'Y') &&
                              result.at("bundle").is_null(),
                          "Loose child extraction differs");
            Monstey_WriteNewText(f.game / L"ShadowOfWar.exe", "");
            Monstey_Reject(
                [&] { Monstey_ExtractEmbeddedResource(parent, "", "child.bin", f.game / L"must-not-exist.bin"); });
        });
        Monstey_Case("bounded embedded child range preserves byte offsets", [&] {
            Monstey_Fixture f(root);
            const auto parent =
                f.Put("range.embb", Monstey_Embb({{"child.bin", {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'}}}));
            const auto result = Monstey_ReadEmbeddedResourceRange(parent, "", "child.bin", 2, 4);
            Monstey_Check(result.at("bytes_hex").as_string() == "63646566" &&
                              result.at("u32_little_endian").at(0).as_uint() == 0x66656463u,
                          "Bounded child bytes/words differ");
            Monstey_Reject([&] { Monstey_ReadEmbeddedResourceRange(parent, "", "child.bin", 7, 2); });
            Monstey_Reject([&] { Monstey_ReadEmbeddedResourceRange(parent, "", "child.bin", 0, 1024 * 1024 + 1); });
        });
        Monstey_Case("empty quoted mount lines stay empty", [&] {
            Monstey_Fixture f(root);
            f.Config("\"\"\n; empty\n\n");
            const auto mounts = Monstey_ArchiveMounts(f.game);
            Monstey_Check(mounts.at("mounts").empty(), "Empty quotes mounted the config directory");
        });
        Monstey_Case("oversized settings remain bounded and cannot be overwritten", [&] {
            Monstey_Fixture f(root);
            const auto path = f.root / L"settings.json";
            Monstey_WriteNewText(path, std::string(1024 * 1024 + 1, 'x'));
            const auto before = Monstey_HashSha256(path);
            Monstey_Settings settings(path);
            Monstey_Check(settings.Describe().at("configuration_error").is_string(), "Oversized settings accepted");
            Monstey_Reject([&] { settings.Configure(J::object({{"game_root", Monstey_PathString(f.game)}})); });
            Monstey_Check(Monstey_HashSha256(path) == before, "Unqualified oversized source overwritten");
        });
        if (argc > 2)
        {
            const auto game = std::filesystem::path(argv[2]);
            Monstey_Case("actual mounted archive index and loose metadata are read-only", [&] {
                const auto config = game / L"x64/default.archcfg";
                const auto before = Monstey_HashSha256(config);
                const auto report = Monstey_ArchiveCatalog(game, root / L"native.jsonl",
                                                           J::object({{"archive_filter", "Monstey_SowSDK.arch06"}}));
                Monstey_Check(report.at("rows").as_uint() > 0 && !report.at("game_written").as_bool() &&
                                  !report.at("payloads_copied").as_bool(),
                              "Native catalog proof missing");
                Monstey_Check(Monstey_HashSha256(config) == before, "Native config changed");
                const auto query = Monstey_ReadArchiveCatalog(
                    root / L"native.jsonl", J::object({{"limit", 5}, {"winning_only", true}, {"verify_mounts", true}}));
                Monstey_Check(!query.at("rows").empty(), "Native catalog cannot be queried");
                std::cout << report.dump() << '\n';
            });
        }
        if (argc > 2)
        {
            const auto game = std::filesystem::path(argv[2]);
            Monstey_Case("actual native EMBB5 metadata indexes without copying child payloads", [&] {
                const auto output = root / L"native-embedded.jsonl";
                const auto dll = game / L"x64/oo2core_5_win64.dll";
                const auto result = Monstey_ArchiveCatalog(game, output,
                                                           J::object({{"archive_filter", "Monstey_SowSDK.arch06"},
                                                                      {"include_embedded", true},
                                                                      {"oodle_dll", Monstey_PathString(dll)}}));
                Monstey_Check(result.at("counts").at("parents_checked").as_uint() > 0 &&
                                  result.at("counts").at("embedded").as_uint() > 0,
                              "No real EMBB metadata qualified");
                const auto rows = Monstey_ReadArchiveCatalog(output, J::object({{"kind", "embedded"}, {"limit", 5}}));
                Monstey_Check(!rows.at("rows").empty(), "Native embedded catalog cannot be queried");
                Monstey_Check(!result.at("payloads_copied").as_bool() &&
                                  !result.at("payload_hashes_computed").as_bool(),
                              "Native payloads materialized");
                std::cout << result.dump() << '\n';
            });
        }
        std::cout << "CATALOG_RESULT " << Monstey_TestPassed << " groups passed; " << root.string() << '\n';
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
