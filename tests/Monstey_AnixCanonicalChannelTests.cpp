#include "Monstey_Anix.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iostream>
#include <numeric>
using namespace Monstey;
namespace {
using Monstey_CanonicalJson = Monstey_Json;
using Monstey_CanonicalBytes = Monstey_Bytes;
using Monstey_CanonicalPair = std::pair<std::uint32_t, std::uint32_t>; // type, bone
constexpr std::uint32_t Monstey_CanonicalPos = 0x00ce66fc, Monstey_CanonicalRot = 0x27436396, Monstey_CanonicalEvent = 0x210c8f5a;
Monstey_CanonicalJson Monstey_Track(unsigned index, Monstey_CanonicalJson first, Monstey_CanonicalJson last) {
    return Monstey_CanonicalJson::object({{"channel_index", index}, {"values", Monstey_CanonicalJson::array({first, last})}});
}
Monstey_CanonicalJson Monstey_Scene() {
    const auto channels = Monstey_CanonicalJson::array({
        Monstey_CanonicalJson::object({{"type_hash", Monstey_Hex(Monstey_CanonicalPos)}, {"bone_hash", "00000020"}}),
        Monstey_CanonicalJson::object({{"type_hash", Monstey_Hex(Monstey_CanonicalRot)}, {"bone_hash", "00000020"}}),
        Monstey_CanonicalJson::object({{"type_hash", Monstey_Hex(Monstey_CanonicalPos)}, {"bone_hash", "00000010"}}),
        Monstey_CanonicalJson::object({{"type_hash", Monstey_Hex(Monstey_CanonicalRot)}, {"bone_hash", "00000010"}}),
        Monstey_CanonicalJson::object({{"type_hash", Monstey_Hex(Monstey_CanonicalEvent)}, {"bone_hash", "00000020"}})});
    const auto make_clip = [](const char *name, bool moving) {
        return Monstey_CanonicalJson::object({{"name", name}, {"duration_ticks", 1000}, {"ticks", Monstey_CanonicalJson::array({0, 1000})},
            {"tracks", Monstey_CanonicalJson::array({
                Monstey_Track(0, Monstey_CanonicalJson::array({0, 1, 2}), moving ? Monstey_CanonicalJson::array({10, 11, 12}) : Monstey_CanonicalJson::array({0, 1, 2})),
                Monstey_Track(1, Monstey_CanonicalJson::array({0, 0, 0, 1}), Monstey_CanonicalJson::array({0, 0, 0, 1})),
                Monstey_Track(2, Monstey_CanonicalJson::array({20, 21, 22}), moving ? Monstey_CanonicalJson::array({30, 31, 32}) : Monstey_CanonicalJson::array({20, 21, 22})),
                Monstey_Track(3, Monstey_CanonicalJson::array({0, 0, 1, 0}), Monstey_CanonicalJson::array({0, 0, 1, 0}))})},
            {"event_channel_index", 4},
            {"events", moving ? Monstey_CanonicalJson::array({Monstey_CanonicalJson::object({{"tick", 500}, {"text", "CANONICAL_EVENT"}})}) : Monstey_CanonicalJson::array()}});
    };
    return Monstey_CanonicalJson::object({{"library_path", "synthetic/canonical.anix"}, {"channels", channels},
        {"clips", Monstey_CanonicalJson::array({make_clip("bind", false), make_clip("walk", true)})}});
}
std::vector<Monstey_CanonicalPair> Monstey_ReadPairs(const Monstey_CanonicalBytes &bytes) {
    const auto count = Monstey_U32(bytes, 16), payload = Monstey_U32(bytes, 24);
    std::size_t cursor = Monstey_U32(bytes, 20);
    std::vector<Monstey_CanonicalPair> pairs;
    while (pairs.size() < count) {
        Monstey_CheckRange(bytes, cursor, 8);
        const auto kind = Monstey_U32(bytes, cursor), number = Monstey_U32(bytes, cursor + 4);
        cursor += 8;
        Monstey_Require(number > 0 && number <= count - pairs.size() && cursor + 4ull * number <= payload,
                         "Invalid shared group in test output");
        for (std::size_t i = 0; i < number; ++i)
            pairs.emplace_back(kind, Monstey_U32(bytes, cursor + 4 * i));
        cursor += 4ull * number;
    }
    return pairs;
}
std::size_t Monstey_NativeLowerBound(std::span<const Monstey_CanonicalPair> pairs, Monstey_CanonicalPair target) {
    std::size_t first = 0, count = pairs.size();
    while (count) {
        const auto half = count / 2, middle = first + half;
        if (pairs[middle] < target) {
            first = middle + 1;
            count -= half + 1;
        } else {
            count = half;
        }
    }
    return first;
}
void Monstey_RequireIdentityMatches(const Monstey_CanonicalBytes &bytes) {
    const auto pairs = Monstey_ReadPairs(bytes);
    Monstey_Require(std::is_sorted(pairs.begin(), pairs.end()), "Authored shared pairs are not canonical");
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        const auto found = Monstey_NativeLowerBound(pairs, pairs[i]);
        Monstey_Require(found < pairs.size() && pairs[found] == pairs[i], "Native tuple search misses an authored identity");
    }
}
const Monstey_CanonicalJson &Monstey_TrackAt(const Monstey_CanonicalJson &scene, std::size_t channel) {
    for (const auto &track : scene["tracks"].as_array())
        if (track["channel_index"].as_uint() == channel)
            return track;
    throw std::runtime_error("Expected track is absent");
}
bool Monstey_NumericEqual(const Monstey_CanonicalJson &left, const Monstey_CanonicalJson &right) {
    if (left.is_array() && right.is_array()) {
        if (left.size() != right.size())
            return false;
        for (std::size_t i = 0; i < left.size(); ++i)
            if (!Monstey_NumericEqual(left[i], right[i]))
                return false;
        return true;
    }
    return left.is_number() && right.is_number() ? left.as_number() == right.as_number() : left == right;
}
void Monstey_Reject(const std::function<void()> &action) {
    bool rejected = false;
    try {action();} catch (const Monstey_Error &) {rejected = true;}
    Monstey_Require(rejected, "Invalid semantic index accepted");
}
Monstey_CanonicalJson Monstey_Permute(const Monstey_CanonicalJson &input, std::span<const std::size_t> order) {
    auto scene = input;
    Monstey_CanonicalJson channels = Monstey_CanonicalJson::array();
    std::vector<std::size_t> remap(order.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        channels.as_array().push_back(input["channels"][order[i]]);
        remap[order[i]] = i;
    }
    scene["channels"] = channels;
    for (auto &clip : scene["clips"].as_array()) {
        for (auto &track : clip["tracks"].as_array())
            track["channel_index"] = remap[static_cast<std::size_t>(track["channel_index"].as_uint())];
        clip["event_channel_index"] = remap[static_cast<std::size_t>(clip["event_channel_index"].as_uint())];
    }
    return scene;
}
}
int wmain(int argc, wchar_t **argv) {
    try {
        Monstey_Require(argc == 2 || argc == 3, "Usage: Monstey_AnixCanonicalChannelTests output-base [old-authored.anix]");
        const auto out = std::filesystem::path(argv[1]) / ("Canonical_" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(out);
        Monstey_CanonicalJson tests = Monstey_CanonicalJson::array();
        unsigned failures = 0;
        const auto test = [&](const char *name, const std::function<void()> &action) {
            try {action();tests.as_array().push_back(Monstey_CanonicalJson::object({{"name", name}, {"passed", true}}));}
            catch (const std::exception &e) {++failures;tests.as_array().push_back(Monstey_CanonicalJson::object({
                {"name", name}, {"passed", false}, {"message", e.what()}}));}
        };
        const auto input = Monstey_Scene(), before = input;
        const auto library = Monstey_Anix::Create(input);
        const auto bytes = library.Build();
        const std::vector<Monstey_CanonicalPair> expected{{Monstey_CanonicalPos, 0x10}, {Monstey_CanonicalPos, 0x20}, {Monstey_CanonicalEvent, 0x20}, {Monstey_CanonicalRot, 0x10}, {Monstey_CanonicalRot, 0x20}};
        test("canonical_shared_groups_and_native_search_find_every_identity", [&] {
            Monstey_Require(Monstey_ReadPairs(bytes) == expected, "Native shared order differs from independent expected vector");
            Monstey_RequireIdentityMatches(bytes);
            Monstey_Require(input == before, "Caller semantic scene changed");
        });
        test("coordinated_pose_and_event_index_remap_preserves_values", [&] {
            const auto clip = library.ExportClip("walk");
            Monstey_Require(clip["event_channel_index"].as_uint() == 2 &&
                clip["events"] == input["clips"][1]["events"], "Monstey_CanonicalEvent identity or content changed");
            Monstey_Require(Monstey_NumericEqual(Monstey_TrackAt(clip, 0)["values"], input["clips"][1]["tracks"][2]["values"]),
                             "Arm position values attached to a different identity");
            Monstey_Require(Monstey_NumericEqual(Monstey_TrackAt(clip, 1)["values"], input["clips"][1]["tracks"][0]["values"]),
                             "Root position values attached to a different identity");
            const std::array<double, 1> tick{500};
            const auto sampled = library.Sample("walk", tick, false);
            Monstey_Require(Monstey_NumericEqual(Monstey_TrackAt(sampled, 0)["values"][0], Monstey_CanonicalJson::array({25, 26, 27})) &&
                Monstey_NumericEqual(Monstey_TrackAt(sampled, 1)["values"][0], Monstey_CanonicalJson::array({5, 6, 7})), "Numeric trajectory identity changed");
        });
        std::size_t permutations = 0;
        test("all_120_declaration_permutations_build_identical_native_bytes", [&] {
            std::array<std::size_t, 5> order{0, 1, 2, 3, 4};
            do {
                const auto permuted = Monstey_Permute(input, order);
                const auto actual = Monstey_Anix::Create(permuted).Build();
                Monstey_Require(actual == bytes, "Equivalent channel declaration permutation changes native output");
                Monstey_RequireIdentityMatches(actual);
                ++permutations;
            } while (std::next_permutation(order.begin(), order.end()));
            Monstey_Require(permutations == 120, "Incomplete permutation coverage");
        });
        test("reopen_and_native_noop_preserve_canonical_bytes", [&] {
            const Monstey_Anix reopened(bytes);
            Monstey_Require(reopened.Build() == bytes &&
                reopened.ExportClip("walk") == library.ExportClip("walk"), "Canonical reopen changes data");
            Monstey_WriteNewFile(out / "canonical.anix", bytes);
        });
        test("unsigned_bone_order_and_unused_identities_preserved", [&] {
            auto scene = input;
            scene["channels"].as_array().push_back(Monstey_CanonicalJson::object({{"type_hash", Monstey_Hex(Monstey_CanonicalPos)}, {"bone_hash", "80000000"}}));
            scene["channels"].as_array().push_back(Monstey_CanonicalJson::object({{"type_hash", Monstey_Hex(Monstey_CanonicalPos)}, {"bone_hash", "7fffffff"}}));
            const auto result = Monstey_Anix::Create(scene).Build();
            const auto pairs = Monstey_ReadPairs(result);
            Monstey_Require(pairs.size() == 7 && pairs[2] == Monstey_CanonicalPair(Monstey_CanonicalPos, 0x7fffffffu) &&
                pairs[3] == Monstey_CanonicalPair(Monstey_CanonicalPos, 0x80000000u), "Bone comparison is not unsigned or unused identity lost");
        });
        test("new_authoring_rejects_duplicate_shared_identities", [&] {
            auto scene = input;
            scene["channels"].as_array().push_back(scene["channels"][0]);
            Monstey_Reject([&] {(void)Monstey_Anix::Create(scene);});
        });
        test("invalid_track_and_event_indices_reject_without_mutating_input", [&] {
            auto scene = input;
            scene["clips"][1]["tracks"][0]["channel_index"] = 5;
            Monstey_Reject([&] {(void)Monstey_Anix::Create(scene);});
            scene = input; scene["clips"][1]["event_channel_index"] = 5;
            Monstey_Reject([&] {(void)Monstey_Anix::Create(scene);});
            scene = input; scene["clips"][1]["tracks"][0]["channel_index"] = -1;
            Monstey_Reject([&] {(void)Monstey_Anix::Create(scene);});
            Monstey_Require(input == before, "Rejected authoring changed caller scene");
        });
        test("existing_semantic_duplicate_track_rejection_remains", [&] {
            auto scene = input;
            scene["clips"][1]["tracks"].as_array().push_back(scene["clips"][1]["tracks"][0]);
            Monstey_Reject([&] {(void)Monstey_Anix::Create(scene);});
        });
        if (argc == 3)
            test("old_authored_native_input_remains_byte_exact_without_implicit_repair", [&] {
                const auto original = Monstey_ReadFile(argv[2]);
                const auto hash = Monstey_HashSha256(original);
                const Monstey_Anix existing(original);
                Monstey_Require(existing.Build() == original && Monstey_HashSha256(argv[2]) == hash,
                                 "Native no-op silently repaired or altered an existing source");
                const auto pairs = Monstey_ReadPairs(original);
                Monstey_Require(!std::is_sorted(pairs.begin(), pairs.end()), "Expected old unsorted control fixture");
                std::size_t missed = 0;
                for (const auto pair : pairs) {
                    const auto found = Monstey_NativeLowerBound(pairs, pair);
                    missed += found == pairs.size() || pairs[found] != pair;
                }
                Monstey_Require(pairs.size() == 5 && missed == 5, "Old fixture did not reproduce the native lookup defect");
            });
        const auto report = Monstey_CanonicalJson::object({{"schema", "Monstey.AnixCanonicalSharedChannels/1"}, {"tests", tests},
            {"failures", failures}, {"equivalent_scene_permutations", permutations},
            {"native_engine_executed", false}, {"original_source_written", false}});
        Monstey_WriteNewText(out / "report.json", report.dump(2) + "\n");
        std::cout << "CanonicalTests=" << tests.size() << " Failures=" << failures << " Permutations=" << permutations << '\n';
        return failures ? 1 : 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}