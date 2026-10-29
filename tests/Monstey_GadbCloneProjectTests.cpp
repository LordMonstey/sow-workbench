#include "Monstey_Gadb.hpp"
#include <chrono>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
void Check(bool condition, const char *message) {
    if (!condition) throw Monstey_Error("test", message);
}
void Write(const std::filesystem::path &path, const J &value) {
    Monstey_WriteNewText(path, value.dump(2));
}
}
int wmain(int argc, wchar_t **argv) {
    try {
        const auto base = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path();
        const auto out = base / (L"CloneProject_" +
            std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(out);
        auto report = J::object({{"status", "PASS"}, {"cases", J::array()},
                                  {"native_cpp", true}, {"game_written", false}});
        const auto test = [&](const char *name, const std::function<void()> &run) {
            run(); report["cases"].as_array().push_back(name);
        };
        const auto schema = out / L"schema.json";
        Write(schema, J::object({{"categories", J::object({{"Entity", J::object({
            {"fields", J::array({
                J::object({{"name", "Text"}, {"type_code", 4}, {"array", 1}}),
                J::object({{"name", "Vector"}, {"type_code", 8}, {"array", 1}}),
                J::object({{"name", "Number"}, {"type_code", 3}, {"array", 1}}),
                J::object({{"name", "Extra"}, {"type_code", 3}, {"array", 1}})
            })}})}})}}));
        auto seed = Monstey_Gadb::New(schema);
        const auto category = static_cast<std::uint32_t>(
            seed->CreateCategory("Entity")["category"]["index"].as_uint());
        for (const auto anonymous : {true, false}) {
            const auto record = seed->CreateRecord(category, anonymous ? "" : "Seed", anonymous)
                ["record"]["selector"].as_string();
            seed->AddField(record, "Text", 4, "Original");
            seed->AddField(record, "Vector", 8, J::array({1.0, 2.0, 3.0}));
            seed->AddField(record, "Number", 3, 17);
        }
        const auto native = out / L"native.gamedb";
        seed->Build(native);
        auto sparseBytes = Monstey_ReadFile(native);
        const auto descriptorBase = 46 + Monstey_U16(sparseBytes, 44) + Monstey_U32(sparseBytes, 8);
        const auto descriptorCount = Monstey_U32(sparseBytes, 28);
        for (std::size_t index = 0; index < descriptorCount; ++index)
            sparseBytes[descriptorBase + index * 15 + 8] =
                static_cast<std::uint8_t>(index % 3 == 0 ? 0 : index % 3 == 1 ? 5 : 11);
        const auto sparse = out / L"sparse-native.gamedb";
        Monstey_WriteNewFile(sparse, sparseBytes);
        const auto roundtrip = [&](const std::filesystem::path &source, const char *selector,
                                   const char *label) {
            Monstey_Gadb db(source, schema);
            const auto clone = db.CloneRecord(selector, label)["clone"]["id"].as_string();
            db.SetField(clone, "Text", std::string("Changed ") + label);
            const auto project = out / (std::string(label) + ".project.json");
            db.SaveProject(project);
            const auto doc = J::Parse(Monstey_ReadText(project));
            Check(!doc["changes"]["clones"][0]["snapshot_native_layout"].as_bool(),
                  "Native raw snapshot was classified as an authored layout");
            const auto before = out / (std::string(label) + "-before.gamedb");
            const auto after = out / (std::string(label) + "-after.gamedb");
            db.Build(before);
            auto replay = Monstey_Gadb::LoadProject(project);
            replay->Build(after);
            Check(Monstey_ReadFile(before) == Monstey_ReadFile(after),
                  "Native clone project replay changed descriptor pool or payload bytes");
            Check(Monstey_HashSha256(source) == db.Describe()["source_sha256"].as_string(),
                  "Source native database changed");
            return doc;
        };
        test("dense_native_named_clone_project_is_byte_exact", [&] {
            roundtrip(native, "Entity:Seed", "DenseNamed");
        });
        test("dense_native_anonymous_clone_project_is_byte_exact", [&] {
            roundtrip(native, "0x00000000", "DenseAnonymous");
        });
        J sparseDoc;
        test("sparse_native_ordinals_are_preserved_with_valid_sorted_lookup", [&] {
            sparseDoc = roundtrip(sparse, "Entity:Seed", "SparseNamed");
            const auto &descriptors = sparseDoc["changes"]["clones"][0]["snapshot"]["descriptors"];
            Check(descriptors[0][2].as_uint() == 0 && descriptors[1][2].as_uint() == 5 &&
                      descriptors[2][2].as_uint() == 11, "Fixture lost sparse native source ordinals");
        });
        test("sparse_native_anonymous_clone_project_is_byte_exact", [&] {
            roundtrip(sparse, "0x00000000", "SparseAnonymous");
        });
        test("legacy_version2_without_layout_marker_recovers_exact_native_snapshot", [&] {
            auto legacy = sparseDoc;
            legacy["changes"]["clones"][0].as_object().erase("snapshot_native_layout");
            const auto file = out / L"legacy.project.json";
            Write(file, legacy);
            auto replay = Monstey_Gadb::LoadProject(file);
            const auto candidate = out / L"legacy-replayed.gamedb";
            replay->Build(candidate);
            Check(Monstey_ReadFile(candidate) == Monstey_ReadFile(out / "SparseNamed-before.gamedb"),
                  "Legacy native snapshot inferred a new descriptor layout");
        });
        test("authored_clone_layout_stays_owned_after_source_layout_changes", [&] {
            Monstey_Gadb db(sparse, schema);
            db.AddField("Entity:Seed", "Extra", 3, 73);
            const auto clone = db.CloneRecord("Entity:Seed", "Authored")["clone"]["id"].as_string();
            db.DeleteField("Entity:Seed", "Extra");
            const auto project = out / L"authored.project.json";
            db.SaveProject(project);
            const auto doc = J::Parse(Monstey_ReadText(project));
            Check(doc["changes"]["clones"][0]["snapshot_native_layout"].as_bool(),
                  "Authored snapshot lost explicit layout ownership");
            const auto before = out / L"authored-before.gamedb";
            const auto after = out / L"authored-after.gamedb";
            db.Build(before);
            auto replay = Monstey_Gadb::LoadProject(project);
            replay->Build(after);
            Check(Monstey_ReadFile(before) == Monstey_ReadFile(after),
                  "Authored clone source mutation changed replay bytes");
            bool extra = false;
            const auto metadata = replay->Metadata(clone);
            for (const auto &field : metadata["fields"].as_array())
                if (field["name"].as_string() == "Extra")
                    extra = field["value"].as_int() == 73;
            Check(extra, "Authored clone lost its independent field snapshot");
        });
        test("invalid_sorted_lookup_still_rejects", [&] {
            auto invalid = sparseDoc;
            auto &hash = invalid["changes"]["clones"][0]["snapshot"]["descriptors"][0][1];
            hash = hash.as_uint() ^ 1u;
            const auto path = out / L"bad-lookup.project.json";
            Write(path, invalid);
            bool rejected = false;
            try { Monstey_Gadb::LoadProject(path); } catch (const std::exception &) {rejected = true;}
            Check(rejected, "Sparse ordinal support bypassed native sorted lookup validation");
        });
        test("native_marker_cannot_hide_changed_snapshot_words", [&] {
            auto invalid = sparseDoc;
            auto &word = invalid["changes"]["clones"][0]["snapshot"]["words"][0];
            word = word.as_uint() ^ 1u;
            const auto path = out / L"bad-native-snapshot.project.json";
            Write(path, invalid);
            bool rejected = false;
            try { Monstey_Gadb::LoadProject(path); } catch (const std::exception &) {rejected = true;}
            Check(rejected, "Immutable native snapshot marker accepted changed source words");
        });
        report["passed"] = report["cases"].size();
        Monstey_WriteNewText(out / L"clone-project-tests.json", report.dump(2));
        std::cout << report["passed"].as_uint() << " native clone-project groups passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}