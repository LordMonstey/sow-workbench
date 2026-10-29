#include "Monstey_AnimationProject.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
void Monstey_Check(bool condition, const char *message) { Monstey_Require(condition, message); }
void Monstey_Reject(const std::function<void()> &action) {
    bool rejected = false;
    try {
        action();
    } catch (const Monstey_Error &) {
        rejected = true;
    }
    Monstey_Check(rejected, "Expected native authoring rejection");
}
J Monstey_Track(unsigned channel, std::string_view codec, J values) {
    auto out = J::object({{"channel_index", channel}, {"values", std::move(values)}});
    if (!codec.empty()) {
        out["native_role"] = "auxiliary";
        out["native_codec"] = std::string(codec);
    }
    return out;
}
J Monstey_Clip(const char *name, bool moving) {
    const auto negzero = J::object({{"raw_u32", 0x80000000u}, {"float32", 123.0}});
    const auto nan = J::object({{"raw_u32", 0x7fc01234u}, {"float32", nullptr}});
    return J::object(
        {{"name", name},
         {"duration_ticks", 1000},
         {"ticks", J::array({0, 200, 1000})},
         {"event_channel_index", 2},
         {"events", moving ? J::array({J::object({{"tick", 333}, {"text", "AUX_HIT"}})}) : J::array()},
         {"tracks",
          J::array(
              {Monstey_Track(0, "",
                             J::array({J::array({0, 0, 0}), J::array({moving ? 10 : 0, 0, 0}),
                                       J::array({moving ? 30 : 0, 0, 0})})),
               Monstey_Track(
                   1, "", J::array({J::array({0, 0, 0, 1}), J::array({0, 0, 0, 1}), J::array({0, 0, 0, 1})})),
               Monstey_Track(3, "CAF4", moving ? J::array({0.0, 10.0, 30.0}) : J::array({0.0, 0.0, 0.0})),
               Monstey_Track(4, "CAF4",
                             moving ? J::array({100.0, 200.0, 400.0}) : J::array({100.0, 100.0, 100.0})),
               Monstey_Track(5, "USFT", J::array({negzero, negzero, negzero})),
               Monstey_Track(6, "USFT", J::array({nan, nan, nan})),
               Monstey_Track(7, "USBT", J::array({255, 255, 255}))})}});
}
J Monstey_Scene() {
    auto channels = J::array({J::object({{"type_hash", "00ce66fc"}, {"bone_hash", "88baa7e7"}}),
                              J::object({{"type_hash", "27436396"}, {"bone_hash", "88baa7e7"}}),
                              J::object({{"type_hash", "210c8f5a"}, {"bone_hash", "88baa7e7"}})});
    for (unsigned hash = 1; hash <= 5; ++hash)
        channels.as_array().push_back(J::object({{"type_hash", "5f1d8eed"}, {"bone_hash", Monstey_Hex(hash)}}));
    return J::object({{"format", "Monstey_ANIX_Semantic_v1"},
                      {"library_path", "characters/animationcharacters/synthetic/auxiliary.anix"},
                      {"channels", channels},
                      {"clips", J::array({Monstey_Clip("bind", false), Monstey_Clip("walk", true)})}});
}
const J &Monstey_Channel(const J &sample, std::uint64_t channel) {
    for (const auto &track : sample["tracks"].as_array())
        if (track["channel_index"].as_uint() == channel)
            return track;
    throw Monstey_Error("test", "Expected channel is missing");
}
void Monstey_CompareAuxiliary(const J &a, const J &b, double tolerance = 0) {
    std::size_t count = 0;
    for (const auto &track : a["tracks"].as_array()) {
        if (track.value("native_role", "").as_string() != "auxiliary")
            continue;
        const auto &other = Monstey_Channel(b, track["channel_index"].as_uint());
        Monstey_Check(other["native_codec"] == track["native_codec"], "Auxiliary codec identity changed");
        Monstey_Check(other["values"].size() == track["values"].size(), "Auxiliary value count changed");
        for (std::size_t i = 0; i < track["values"].size(); ++i) {
            const auto &x = track["values"][i], &y = other["values"][i];
            const auto codec = track["native_codec"].as_string();
            if (codec == "USFT")
                Monstey_Check(x["raw_u32"] == y["raw_u32"], "Raw auxiliary word changed");
            else if (codec == "USBT")
                Monstey_Check(x == y, "Auxiliary byte changed");
            else
                Monstey_Check(std::abs(x.as_number() - y.as_number()) <=
                                  tolerance * std::max(1.0, std::abs(x.as_number())),
                              "CAF4 authored numeric value differs");
        }
        ++count;
    }
    Monstey_Check(count > 0, "No auxiliary tracks were compared");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    auto out = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path();
    out /= "Auxiliary_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(out);
    J results = J::array();
    unsigned failed = 0;
    auto test = [&](const char *name, const std::function<void()> &action) {
        try {
            action();
            results.as_array().push_back(J::object({{"name", name}, {"passed", true}}));
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception &error) {
            ++failed;
            results.as_array().push_back(
                J::object({{"name", name}, {"passed", false}, {"error", error.what()}}));
            std::cout << "FAIL " << name << ": " << error.what() << '\n';
        }
    };
    test("CAF4 encoder exact fixed header keys padding and float rows", [&] {
        Monstey_AnixScalarKeys keys{{0, 200, 1000}, {{-0.0f, 100}, {10, 200}, {30, 400}}};
        const auto bytes = Monstey_AnixEncodeCaf4(keys);
        Monstey_Check(bytes.size() == 56 && Monstey_U32(bytes, 0) == 1 && Monstey_U32(bytes, 4) == 0 &&
                          Monstey_U32(bytes, 8) == 1000 && Monstey_U32(bytes, 12) == 3 &&
                          Monstey_U32(bytes, 16) == 2 && Monstey_U16(bytes, 20) == 0 &&
                          Monstey_U16(bytes, 22) == 200 && Monstey_U16(bytes, 24) == 800 &&
                          Monstey_U32(bytes, 32) == 0x80000000u,
                      "CAF4 encoder does not match its independent literal layout");
        const std::array<double, 5> ticks{0, 100, 200, 600, 1000};
        const auto decoded = Monstey_AnixDecodeCodec("CAF4", bytes, 2, {}, ticks);
        Monstey_Check(decoded[1] == J::array({5.0, 150.0}) && decoded[3] == J::array({20.0, 300.0}),
                      "CAF4 encoded interpolation differs from hand-computed values");
    });
    test("CAF4 encoder rejects malformed times dimensions and float values", [&] {
        const Monstey_AnixScalarKeys valid{{0, 1000}, {{0}, {1}}};
        for (unsigned kind = 0; kind < 7; ++kind) {
            auto keys = valid;
            if (kind == 0)
                keys.ticks[1] = 0;
            if (kind == 1)
                keys.ticks[1] = 65536;
            if (kind == 2)
                keys.values[1].clear();
            if (kind == 3)
                keys.values[1][0] = std::numeric_limits<float>::quiet_NaN();
            if (kind == 4)
                keys.values[0].clear();
            if (kind == 5)
                keys.ticks.clear();
            if (kind == 6) {
                keys.values[0][0] = -std::numeric_limits<float>::max();
                keys.values[1][0] = std::numeric_limits<float>::max();
            }
            Monstey_Reject([&] { (void)Monstey_AnixEncodeCaf4(keys); });
        }
    });
    test("USFT encoder preserves raw words including NaN infinities and negative zero", [&] {
        const std::array<std::uint32_t, 5> words{0x7fc01234u, 0x80000000u, 0xff800000u, 0xffffffffu,
                                                 0x3f800000u};
        const auto bytes = Monstey_AnixEncodeUsft(words);
        for (std::size_t i = 0; i < words.size(); ++i)
            Monstey_Check(Monstey_U16(bytes, i * 2) == 10 + i * 4 && Monstey_U32(bytes, 10 + i * 4) == words[i],
                          "USFT literal offset table or payload changed");
        const std::array<double, 1> ticks{0};
        const auto values = Monstey_AnixDecodeCodec("USFT", bytes, words.size(), {}, ticks);
        for (std::size_t i = 0; i < words.size(); ++i)
            Monstey_Check(values[0][i]["raw_u32"].as_uint() == words[i], "USFT exact payload mismatch");
    });
    test("USBT encoder retains every possible byte without bool normalization", [&] {
        B values(256);
        for (unsigned i = 0; i < 256; ++i)
            values[i] = static_cast<std::uint8_t>(i);
        const auto bytes = Monstey_AnixEncodeUsbt(values);
        const std::array<double, 1> ticks{0};
        const auto decoded = Monstey_AnixDecodeCodec("USBT", bytes, 256, {}, ticks);
        for (unsigned i = 0; i < 256; ++i)
            Monstey_Check(decoded[0][i].as_uint() == i && Monstey_U16(bytes, i * 2) == 512 + i,
                          "USBT literal offset table or byte mismatch");
    });
    test("Static encoders reject empty and overflowing u16 tables", [&] {
        Monstey_Reject([&] { (void)Monstey_AnixEncodeUsft({}); });
        Monstey_Reject([&] { (void)Monstey_AnixEncodeUsbt({}); });
        std::vector<std::uint32_t> words(10923);
        std::iota(words.begin(), words.end(), 0u);
        const B bytes(32768);
        Monstey_Reject([&] { (void)Monstey_AnixEncodeUsft(words); });
        Monstey_Reject([&] { (void)Monstey_AnixEncodeUsbt(bytes); });
    });
    test("Large repeated static tables use qualified native aliased offsets", [&] {
        const std::vector<std::uint32_t> words(10923, 0x7fc01234u);
        const B bytes(21846, 255);
        const auto wordBlock = Monstey_AnixEncodeUsft(words), byteBlock = Monstey_AnixEncodeUsbt(bytes);
        Monstey_Check(wordBlock.size() == words.size() * 2 + 4 && byteBlock.size() == bytes.size() * 2 + 1,
                      "Static aliases did not share their exact payload");
        Monstey_Check(Monstey_U16(wordBlock, 0) == Monstey_U16(wordBlock, (words.size() - 1) * 2) &&
                          Monstey_U16(byteBlock, 0) == Monstey_U16(byteBlock, (bytes.size() - 1) * 2),
                      "Static aliased offset identity changed");
    });
    const auto document = Monstey_Scene();
    B source;
    test("Create mixed pose events and all auxiliary codecs without discarded tracks", [&] {
        auto library = Monstey_Anix::Create(document);
        source = library.Build();
        const auto metadata = library.Describe();
        Monstey_Check(metadata["auxiliary_authoring_supported"].as_bool() &&
                          metadata["unsupported_authoring_codecs"].empty() &&
                          metadata["complete_channel_decode"].as_bool() &&
                          metadata["channel_count"].as_uint() == 8 &&
                          metadata["auxiliary_codec_blocks"].as_uint() == 6,
                      "Mixed auxiliary library capability/count mismatch");
        const std::array<double, 2> ticks{100, 600};
        const auto sampled = library.Sample("walk", ticks, false);
        Monstey_Check(sampled["tracks"].size() == 7 &&
                          Monstey_Channel(sampled, 3)["values"] == J::array({5.0, 20.0}) &&
                          Monstey_Channel(sampled, 4)["values"] == J::array({150.0, 300.0}) &&
                          Monstey_Channel(sampled, 5)["values"][0]["raw_u32"].as_uint() == 0x80000000u &&
                          Monstey_Channel(sampled, 6)["values"][0]["float32"].is_null() &&
                          Monstey_Channel(sampled, 7)["values"] == J::array({255, 255}),
                      "Mixed auxiliary sample value/identity mismatch");
        Monstey_WriteNewFile(out / "mixed.anix", source);
    });
    test("Description qualifies bound native channel types before reporting decode or authoring", [&] {
        auto raw = source;
        auto at = static_cast<std::size_t>(Monstey_U32(raw, 20));
        const auto payload = Monstey_U32(raw, 24);
        bool changed = false;
        while (at < payload) {
            const auto type = Monstey_U32(raw, at), count = Monstey_U32(raw, at + 4);
            if (type == 0x5f1d8eedu) {
                Monstey_PutU32(raw, at, 0x12345678u);
                changed = true;
                break;
            }
            at += 8 + 4 * count;
        }
        Monstey_Check(changed, "Auxiliary type group was absent");
        const Monstey_Anix library(raw);
        const auto metadata = library.Describe();
        Monstey_Check(!metadata["complete_channel_decode"].as_bool() &&
                          !metadata["auxiliary_authoring_supported"].as_bool() &&
                          metadata["unsupported_authoring_codecs"] == J::array({"CAF4", "USBT", "USFT"}) &&
                          library.Build() == raw,
                      "Unqualified binding was described as decoded/authorable");
        const std::array<double, 1> ticks{0};
        Monstey_Reject([&] { (void)library.Sample("walk", ticks, false); });
    });
    test("Semantic export JSON parse and reimport retain auxiliary targets and raw bits", [&] {
        Monstey_Anix library(source);
        auto clip = J::Parse(library.ExportClip("walk").dump());
        Monstey_Check(clip["semantic_reimport_supported"].as_bool(),
                      "Qualified auxiliary export is not reimportable");
        clip["name"] = "imported";
        library.Edit(J::array({J::object({{"op", "import"}, {"clip", clip}})}));
        const std::array<double, 5> ticks{0, 100, 200, 600, 1000};
        Monstey_CompareAuxiliary(library.Sample("walk", ticks, false),
                                 library.Sample("imported", ticks, false));
    });
    test("Held pose freezes all auxiliary primitives with bind fallback", [&] {
        Monstey_Anix library(source);
        const std::array<double, 1> at{600};
        auto reference = library.Sample("walk", at, true);
        library.Edit(J::array({J::object({{"op", "held"},
                                          {"source", "walk"},
                                          {"name", "held"},
                                          {"tick", 600},
                                          {"bind_fallback", true},
                                          {"duration_ticks", 2000}})}));
        for (double tick : {0.0, 700.0, 2000.0}) {
            const std::array<double, 1> time{tick};
            Monstey_CompareAuxiliary(reference, library.Sample("held", time, false));
        }
    });
    for (const auto *kind : {"rate", "length", "delay", "trim"}) {
        const std::string label =
            std::string("Auxiliary timeline ") + kind + " preserves mapped numeric/raw values";
        test(label.c_str(), [&] {
            Monstey_Anix library(source);
            auto operation = J::object({{"op", kind}, {"clip", "walk"}});
            std::vector<double> from, to;
            if (std::string_view(kind) == "rate") {
                operation["factor"] = 2.0;
                from = {0, 200, 600, 1000};
                to = {0, 100, 300, 500};
            }
            if (std::string_view(kind) == "length") {
                operation["duration_ticks"] = 2000;
                from = {0, 200, 600, 1000};
                to = {0, 400, 1200, 2000};
            }
            if (std::string_view(kind) == "delay") {
                operation["at_tick"] = 200;
                operation["duration_ticks"] = 500;
                from = {0, 200, 600, 1000};
                to = {0, 200, 1100, 1500};
            }
            if (std::string_view(kind) == "trim") {
                operation["start_tick"] = 200;
                operation["end_tick"] = 1000;
                from = {200, 600, 1000};
                to = {0, 400, 800};
            }
            const auto before = library.Sample("walk", from, false);
            library.Edit(J::array({operation}));
            Monstey_CompareAuxiliary(before, library.Sample("walk", to, false));
            Monstey_Anix reopened(library.Build());
            Monstey_CompareAuxiliary(before, reopened.Sample("walk", to, false));
        });
    }
    test("Long scalar gaps use valid u16 keys without dropping static auxiliary tracks", [&] {
        auto scene = document;
        for (auto &clip : scene["clips"].as_array()) {
            clip["duration_ticks"] = 200000;
            clip["ticks"] = J::array({0, 40000, 200000});
        }
        auto library = Monstey_Anix::Create(scene);
        const std::array<double, 3> ticks{0, 120000, 200000};
        const auto sampled = library.Sample("walk", ticks, false);
        Monstey_Check(Monstey_Channel(sampled, 3)["values"] == J::array({0.0, 20.0, 30.0}) &&
                          Monstey_Channel(sampled, 7)["values"] == J::array({255, 255, 255}),
                      "Long gap auxiliary interpolation mismatch");
    });
    test("Missing unknown or contradictory auxiliary codec metadata rejects transactionally", [&] {
        Monstey_Anix library(source);
        for (unsigned kind = 0; kind < 4; ++kind) {
            auto clip = library.ExportClip("walk");
            clip["name"] = "bad";
            auto &track = clip["tracks"][2];
            if (kind == 0)
                track.as_object().erase("native_codec");
            if (kind == 1)
                track["native_codec"] = "UAQT";
            if (kind == 2)
                track["native_role"] = "bone";
            if (kind == 3)
                track["values"][0] = nullptr;
            Monstey_Reject([&] { library.Edit(J::array({J::object({{"op", "import"}, {"clip", clip}})})); });
            Monstey_Check(library.Build() == source, "Rejected auxiliary import partially committed");
        }
    });
    test("Static raw tracks reject varying values and invalid byte/word widths", [&] {
        for (unsigned kind = 0; kind < 5; ++kind) {
            auto scene = document;
            auto &clip = scene["clips"][1];
            if (kind == 0)
                clip["tracks"][4]["values"][1]["raw_u32"] = 1;
            if (kind == 1)
                clip["tracks"][6]["values"][1] = 1;
            if (kind == 2)
                clip["tracks"][6]["values"][0] = 256;
            if (kind == 3)
                clip["tracks"][4]["values"][0]["raw_u32"] = 0x100000000ull;
            if (kind == 4)
                clip["tracks"][4]["values"][0] = 0;
            Monstey_Reject([&] { (void)Monstey_Anix::Create(scene); });
        }
    });
    test("Project import history undo redo export and deterministic replay retain auxiliaries", [&] {
        Monstey_AnimationProject project(out / "mixed.anix", Monstey_HashSha256(source));
        const auto exported = out / "selected.scene.json";
        const auto result = project.Export(exported, "walk");
        Monstey_Check(result["non_reimportable_clips"].empty(), "Auxiliary project export is not reimportable");
        project.ImportClip(exported, "walk", "project_copy", Monstey_HashSha256(exported));
        const auto hash = project.Describe()["effective_sha256"];
        project.Undo();
        project.Redo();
        Monstey_Check(project.Describe()["effective_sha256"] == hash,
                      "Auxiliary project undo/redo changed bytes");
        const auto path = out / "auxiliary.project.json";
        project.SaveProject(path);
        auto replay = Monstey_AnimationProject::LoadProject(path);
        Monstey_Check(replay->Describe()["effective_sha256"] == hash, "Auxiliary project replay changed bytes");
        const auto target = out / "replayed.anix";
        replay->Build(target);
        Monstey_Anix native(Monstey_ReadFile(target));
        const std::array<double, 3> ticks{0, 600, 1000};
        Monstey_CompareAuxiliary(native.Sample("walk", ticks, false),
                                 native.Sample("project_copy", ticks, false));
        Monstey_Check(Monstey_HashSha256(out / "mixed.anix") == Monstey_HashSha256(source),
                      "Project modified native source");
    });
    test("Large aliased native profiles advertise feasible semantic reimport", [&] {
        for (bool word : {true, false}) {
            const std::size_t count = word ? 10923 : 21846;
            auto scene = J::object({{"library_path", "characters/animationcharacters/synthetic/aliases.anix"},
                                    {"channels", J::array()},
                                    {"clips", J::array()}});
            auto clip = J::object({{"name", "bind"},
                                   {"duration_ticks", 1},
                                   {"ticks", J::array({0, 1})},
                                   {"tracks", J::array()},
                                   {"events", J::array()}});
            for (std::size_t i = 0; i < count; ++i) {
                scene["channels"].as_array().push_back(
                    J::object({{"type_hash", "5f1d8eed"}, {"bone_hash", Monstey_Hex(i + 1)}}));
                auto value = word ? J::object({{"raw_u32", 0x7fc01234u}}) : J(255);
                clip["tracks"].as_array().push_back(
                    Monstey_Track(static_cast<unsigned>(i), word ? "USFT" : "USBT", J::array({value, value})));
            }
            scene["clips"].as_array().push_back(std::move(clip));
            const auto library = Monstey_Anix::Create(scene);
            Monstey_Check(library.Describe()["auxiliary_authoring_supported"].as_bool() &&
                              library.ExportClip("bind")["semantic_reimport_supported"].as_bool(),
                          "Valid large aliased native profile is not reimportable");
        }
    });
    test("Remapped auxiliary targets and event identity survive project import", [&] {
        Monstey_AnimationProject project(out / "mixed.anix");
        auto scene = document;
        const std::array<std::size_t, 8> order{7, 5, 3, 1, 6, 4, 2, 0};
        std::array<std::size_t, 8> remap{};
        auto channels = J::array();
        for (std::size_t i = 0; i < order.size(); ++i) {
            channels.as_array().push_back(scene["channels"][order[i]]);
            remap[order[i]] = i;
        }
        scene["channels"] = std::move(channels);
        for (auto &clip : scene["clips"].as_array()) {
            for (auto &track : clip["tracks"].as_array())
                track["channel_index"] = remap[track["channel_index"].as_uint()];
            clip["event_channel_index"] = remap[clip["event_channel_index"].as_uint()];
        }
        const auto file = out / "remapped.scene.json";
        Monstey_WriteNewText(file, scene.dump());
        project.ImportClip(file, "walk", "remapped");
        const auto target = out / "remapped.anix";
        project.Build(target);
        const Monstey_Anix library(Monstey_ReadFile(target));
        const std::array<double, 3> ticks{0, 600, 1000};
        Monstey_CompareAuxiliary(library.Sample("walk", ticks, false),
                                 library.Sample("remapped", ticks, false));
        Monstey_Check(library.ExportClip("remapped")["event_channel_index"] == library.ExportClip("walk")["event_channel_index"],
                      "Remapped event identity changed");
    });
    std::uint64_t actualImported = 0, actualAuxiliaryValues = 0;
    double actualMaximumRelativeError = 0;
    auto actualImports = J::array();
    if (argc > 2) {
        const std::filesystem::path path(argv[2]);
        const auto raw = Monstey_ReadFile(path);
        const auto hash = Monstey_HashSha256(raw);
        test("Actual library every known auxiliary profile is authorable", [&] {
            Monstey_Anix library(raw);
            const auto metadata = library.Describe();
            Monstey_Check(metadata["auxiliary_authoring_supported"].as_bool() &&
                              metadata["unsupported_authoring_codecs"].empty() &&
                              metadata["complete_channel_decode"].as_bool(),
                          "Actual auxiliary authoring remains unqualified");
        });
        test("Actual CAF4 clip semantic reimport retains sampled curves and inherited raw tracks", [&] {
            Monstey_Anix library(raw);
            const auto clips = library.Describe()["clips"];
            for (const auto &row : clips.as_array()) {
                const auto &codecs = row["codecs"].as_array();
                if (std::find(codecs.begin(), codecs.end(), J("CAF4")) == codecs.end())
                    continue;
                auto clip = library.ExportClip(row["name"].as_string());
                if (!clip["semantic_reimport_supported"].as_bool())
                    continue;
                const auto original = clip["name"].as_string();
                const auto target = "Aux_RT_" + std::to_string(actualImported);
                clip["name"] = target;
                library.Edit(J::array({J::object({{"op", "import"}, {"clip", clip}})}));
                std::set<double> times;
                for (const auto &tick : clip["ticks"].as_array())
                    times.insert(tick.as_number());
                const auto originals = times;
                if (originals.size() > 1)
                    for (auto i = std::next(originals.begin()); i != originals.end(); ++i)
                        times.insert(std::floor((*std::prev(i) + *i) * 0.5));
                const std::vector<double> ticks(times.begin(), times.end());
                const auto expected = library.Sample(original, ticks, true);
                const auto authored = library.Sample(target, ticks, true);
                Monstey_CompareAuxiliary(expected, authored, 2e-5);
                for (const auto &track : expected["tracks"].as_array()) {
                    if (track.value("native_role", "").as_string() != "auxiliary")
                        continue;
                    const auto &other = Monstey_Channel(authored, track["channel_index"].as_uint());
                    actualAuxiliaryValues += track["values"].size();
                    if (track["native_codec"].as_string() == "CAF4")
                        for (std::size_t i = 0; i < track["values"].size(); ++i) {
                            const auto value = track["values"][i].as_number();
                            const auto error = std::abs(value - other["values"][i].as_number()) /
                                               std::max(1.0, std::abs(value));
                            actualMaximumRelativeError = std::max(actualMaximumRelativeError, error);
                        }
                }
                actualImports.as_array().push_back(
                    J::object({{"source", original}, {"target", target}, {"sample_ticks", ticks.size()}}));
                ++actualImported;
            }
            Monstey_Check(actualImported > 0, "No actual CAF4 clip was reimported");
            Monstey_WriteNewFile(out / "actual-authored.anix", library.Build());
            Monstey_Check(Monstey_HashSha256(path) == hash, "Actual authoring changed original source");
        });
        test("Actual held pose retains all forty auxiliary channels", [&] {
            Monstey_Anix library(raw);
            const auto info = library.ExportClip("OV_GetUp_Inf_A");
            const double tick = info["duration_ticks"].as_number() * 0.5;
            const std::array<double, 1> times{tick};
            const auto before = library.Sample("OV_GetUp_Inf_A", times, true);
            library.Edit(J::array({J::object({{"op", "held"},
                                              {"source", "OV_GetUp_Inf_A"},
                                              {"name", "Aux_Held_Actual"},
                                              {"tick", tick},
                                              {"bind_fallback", true},
                                              {"duration_ticks", 1000}})}));
            const std::array<double, 1> held{500};
            Monstey_CompareAuxiliary(before, library.Sample("Aux_Held_Actual", held, false));
        });
    }
    Monstey_WriteNewText(out / "auxiliary-authoring-tests.json",
                         J::object({{"passed", results.size() - failed},
                                    {"failed", failed},
                                    {"cases", results},
                                    {"actual_reimported_clips", actualImported},
                                    {"actual_imports", actualImports},
                                    {"actual_compared_auxiliary_values", actualAuxiliaryValues},
                                    {"actual_maximum_relative_scalar_error", actualMaximumRelativeError},
                                    {"actual_source_supplied", argc > 2},
                                    {"implementation", "C++20"},
                                    {"game_written", false},
                                    {"runtime_verified", false}})
                             .dump(2));
    std::cout << "RESULT " << results.size() - failed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
