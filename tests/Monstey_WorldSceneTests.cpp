#include "Monstey_WorldScene.hpp"
#include <algorithm>
#include <bit>
#include <cfloat>
#include <cmath>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
Monstey_WorldScene Monstey_SceneFixture() {
    Monstey_WorldScene scene;
    scene.header = {2, 1, 0, 1, 1, 0, 0, 1, 1, 44, 0, 1, 0};
    Monstey_WorldSceneGeometry geometry;
    geometry.words = {std::bit_cast<std::uint32_t>(100.f), 1, 3, 3, 1};
    for (float f : {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f})
        Monstey_AppendU32(geometry.vertices, std::bit_cast<std::uint32_t>(f));
    for (auto i : {0, 1, 2})
        Monstey_AppendU16(geometry.indices, static_cast<std::uint16_t>(i));
    Monstey_AppendU16(geometry.extras, 0xbeef);
    scene.geometry.push_back(geometry);
    Monstey_WorldSceneNode leaf, root;
    leaf.words[0] = 0x101;
    root.words[0] = 0x102;
    for (auto *node : {&leaf, &root}) {
        node->words[2] = 0x12340001;
        node->words[9] = 0x11223344;
        node->words[10] = 0x55667788;
        node->words[14] = 0xa1b2c300;
        const float bounds[] = {-10, -20, -30, 10, 20, 30};
        for (unsigned i = 0; i < 6; ++i)
            node->words[3 + i] = std::bit_cast<std::uint32_t>(bounds[i]);
    }
    leaf.words[13] = 0;
    root.words[13] = UINT32_MAX;
    root.geometry = {0};
    root.words[11] = 1;
    root.children = {0};
    root.words[12] = 1;
    scene.nodes = {leaf, root};
    scene.bsp_nodes.push_back({{1, 3, 0, UINT32_MAX, UINT32_MAX}, {0}});
    return scene;
}
// Independent stream-order reconstruction of the native routines, not production parser reuse.
J Monstey_NativeSceneReplica(std::span<const std::uint8_t> bytes) {
    std::size_t cursor = 0;
    const auto word = [&] {
        const auto x = Monstey_U32(bytes, cursor);
        cursor += 4;
        return x;
    };
    std::array<std::uint32_t, 13> header;
    for (auto &x : header)
        x = word();
    for (std::uint32_t g = 0; g < header[8]; ++g) {
        cursor += 8;
        const auto v = word(), i = word(), e = word();
        const auto payload = 12ull * v + 2ull * i + 2ull * e;
        Monstey_CheckRange(bytes, cursor, static_cast<std::size_t>(payload));
        cursor += static_cast<std::size_t>(payload);
    }
    auto nodes = J::array(), bsp = J::array();
    std::array<float, 6> registry = {FLT_MAX, FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (std::uint32_t n = 0; n < header[0]; ++n) {
        const auto at = cursor;
        Monstey_CheckRange(bytes, at, 60);
        const auto flags = Monstey_U32(bytes, at + 8), refs = Monstey_U32(bytes, at + 44),
                   children = Monstey_U32(bytes, at + 48), group = Monstey_U32(bytes, at + 52),
                   planes = Monstey_U32(bytes, at + 56) & 255;
        auto bounds = J::array();
        for (unsigned k = 0; k < 6; ++k) {
            const auto value = std::bit_cast<float>(Monstey_U32(bytes, at + 12 + 4 * k));
            bounds.as_array().push_back(value);
            registry[k] = k < 3 ? std::min(registry[k], value) : std::max(registry[k], value);
        }
        cursor += 60 + 16ull * planes;
        Monstey_CheckRange(bytes, cursor, 4ull * (refs + children));
        cursor += 4ull * refs;
        auto child = J::array(), groupList = J::array();
        for (std::uint32_t i = 0; i < children; ++i)
            child.as_array().push_back(word());
        if (group != UINT32_MAX)
            groupList.as_array().push_back(0x100000ull + 72ull * group);
        nodes.as_array().push_back(
            J::object({{"group", group},
                       {"group_offset", group == UINT32_MAX ? J(nullptr) : J(72ull * group)},
                       {"runtime_flags113", 3u | ((flags & 1) ? 4u : 0u)},
                       {"runtime_category114", (flags >> 1) & 255},
                       {"bounds", bounds},
                       {"plane_count", planes},
                       {"geometry_ref_count", refs},
                       {"group_list80", groupList},
                       {"children", child}}));
    }
    for (std::uint32_t c = 0; c < header[3]; ++c) {
        const auto count = word(), axis = word(), split = word(), left = word(), right = word();
        auto refs = J::array();
        for (std::uint32_t n = 0; n < count; ++n)
            refs.as_array().push_back(word());
        bsp.as_array().push_back(J::object({{"axis32", axis},
                                            {"split_bits", split},
                                            {"left", left},
                                            {"right", right},
                                            {"node_refs", refs}}));
    }
    Monstey_Require(cursor == bytes.size(), "Independent native stream replica did not consume exact extent");
    auto bounds = J::array();
    for (const auto value : registry)
        bounds.as_array().push_back(value);
    return J::object({{"primary", nodes}, {"bsp", bsp}, {"registry_bounds", bounds}});
}
J Monstey_NativeMinimalVisibilityReplica(const J &runtime, const std::array<float, 6> &query) {
    auto submitted = J::array();
    Monstey_Require(runtime["bsp"].size() == 1 && runtime["bsp"][0u]["axis32"].as_uint() > 2,
                    "Visibility replica supports the qualified native single BSP leaf only");
    const auto intersects = [&](const J &bounds) {
        for (unsigned k = 0; k < 3; ++k)
            if (bounds[k].as_number() > query[k + 3] || bounds[k + 3].as_number() < query[k])
                return false;
        return true;
    };
    // 6AD540/6AD5A0 registry admission uses primary bounds, independent of optional geometry.
    Monstey_Require(intersects(runtime["registry_bounds"]), "Query misses native registry bounds");
    // 1CC744 selects first BSP; 1CC5A4 iterates its primary refs and sees axis>2 as leaf.
    for (const auto &reference : runtime["bsp"][0u]["node_refs"].as_array()) {
        const auto &node = runtime["primary"][static_cast<std::size_t>(reference.as_uint())];
        if (!intersects(node["bounds"]))
            continue;
        // 1CC68C returns true directly when no primary clipping planes exist.
        Monstey_Require(node["plane_count"].as_uint() == 0,
                        "Visibility replica needs the qualified zero-plane route");
        if ((node["runtime_flags113"].as_uint() & 1) == 0 || node["runtime_category114"].as_uint() >= 5)
            continue;
        // 2BDF44 -> 2BD648 invokes the group-list80 consumer before unrelated geometry work.
        for (const auto &pointer : node["group_list80"].as_array())
            submitted.as_array().push_back((pointer.as_uint() - 0x100000ull) / 72);
    }
    return submitted;
}
void Monstey_Write(const std::filesystem::path &path, const Monstey_Bytes &bytes) {
    if (std::filesystem::exists(path))
        std::filesystem::remove(path);
    Monstey_WriteNewFile(path, bytes);
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        const std::filesystem::path out = argc > 1 ? argv[1] : L"world-scene-tests";
        std::filesystem::create_directories(out);
        auto report =
            J::object({{"status", "PASS"}, {"game_runtime_verified", false}, {"cases", J::array()}});
        const auto test = [&](std::string_view name, const std::function<void()> &f) {
            try {
                f();
            } catch (const std::exception &e) {
                throw Monstey_Error("test", std::string(name) + ": " + e.what());
            }
            report["cases"].as_array().push_back(std::string(name));
        };
        const auto reject = [&](const std::function<void()> &f) {
            bool rejected = false;
            try {
                f();
            } catch (const std::exception &) {
                rejected = true;
            }
            Monstey_Require(rejected, "Invalid scene was accepted");
        };
        const auto donor = Monstey_SceneFixture();
        const auto bytes = donor.Encode(1);
        test("packed_native_scene_roundtrip_preserves_every_opaque_word", [&] {
            const auto decoded = Monstey_WorldScene::Read(bytes, 1);
            Monstey_Require(decoded.Encode(1) == bytes && decoded.nodes[0].words[14] == 0xa1b2c300,
                            "Packed scene roundtrip changed raw words");
        });
        test("independent_native_stream_replica_observes_group_and_child_offsets", [&] {
            const auto runtime = Monstey_NativeSceneReplica(bytes);
            Monstey_Require(runtime["primary"][0u]["group_offset"].as_uint() == 0 &&
                                runtime["primary"][1u]["group_offset"].is_null() &&
                                runtime["primary"][1u]["children"][0u].as_uint() == 0 &&
                                runtime["primary"][0u]["runtime_flags113"].as_uint() == 7,
                            "Native stream group/child/flag association differs");
        });
        test("donor_qualified_root_leaf_authoring_creates_real_group_routing", [&] {
            const std::array<float, 6> bounds = {0, 0, 0, 100, 200, 300};
            const auto authored = Monstey_WorldSceneRouteSingleGroup(donor, bounds);
            const auto raw = authored.Encode(1);
            Monstey_Require(raw.size() == 200 && authored.geometry.empty() &&
                                authored.nodes[1].children[0] == 0 && authored.bsp_nodes[0].nodes[0] == 0 &&
                                authored.nodes[0].words[13] == 0,
                            "Minimal native scene omitted root/BSP/group leaf links");
            for (unsigned n = 0; n < 2; ++n)
                for (unsigned i : {0u, 1u, 2u, 9u, 10u, 14u})
                    Monstey_Require(authored.nodes[n].words[i] == donor.nodes[n].words[i],
                                    "Opaque prototype state changed");
            (void)Monstey_NativeSceneReplica(raw);
            Monstey_Write(out / L"single-group-scene.bin", raw);
        });
        test("native_bsp_leaf_visibility_submits_group_without_optional_geometry", [&] {
            const auto authored = Monstey_WorldSceneRouteSingleGroup(donor, {0, 0, 0, 100, 200, 300});
            const auto runtime = Monstey_NativeSceneReplica(authored.Encode(1));
            const auto oldEmpty = Monstey_NativeSceneReplica(Monstey_Bytes(52, 0));
            Monstey_Require(
                oldEmpty["registry_bounds"][0u].as_number() > oldEmpty["registry_bounds"][3u].as_number() &&
                    runtime["registry_bounds"][0u].as_number() <= runtime["registry_bounds"][3u].as_number(),
                "Native zero-scene admission failure or finite-node repair was masked");
            const auto submitted = Monstey_NativeMinimalVisibilityReplica(runtime, {10, 20, 30, 11, 21, 31});
            Monstey_Require(
                submitted.size() == 1 && submitted[0u].as_uint() == 0 &&
                    runtime["primary"][1u]["geometry_ref_count"].as_uint() == 0,
                "Native minimal BSP route did not submit the group independently of geometry refs");
            auto invalid = donor;
            invalid.bsp_nodes[0].words[1] = 0;
            reject([&] { Monstey_WorldSceneRouteSingleGroup(invalid, {0, 0, 0, 1, 1, 1}); });
        });
        test("shared_node_publication_word_is_preserved_for_read_but_not_synthesized", [&] {
            auto shared = donor;
            shared.nodes[0].words[1] = 123;
            const auto raw = shared.Encode(1);
            Monstey_Require(Monstey_WorldScene::Read(raw, 1).Encode(1) == raw,
                            "Read-only preservation changed opaque shared-node metadata");
            reject([&] { Monstey_WorldSceneRouteSingleGroup(shared, {0, 0, 0, 1, 1, 1}); });
        });
        test("node_cycles_multiple_parents_and_invalid_groups_are_rejected", [&] {
            auto bad = donor;
            bad.nodes[0].children = {1};
            bad.nodes[0].words[12] = 1;
            reject([&] { bad.Validate(1); });
            bad = donor;
            bad.nodes[1].children = {0, 0};
            bad.nodes[1].words[12] = 2;
            reject([&] { bad.Validate(1); });
            bad = donor;
            bad.nodes[0].words[13] = 1;
            reject([&] { bad.Validate(1); });
        });
        test("native_reserved_allocation_and_payload_counters_are_checked", [&] {
            for (auto i : {2u, 5u, 6u, 7u, 9u, 10u, 12u}) {
                auto bad = donor;
                ++bad.header[i];
                reject([&] { bad.Validate(1); });
            }
            auto bad = donor;
            bad.nodes[1].geometry = {1};
            reject([&] { bad.Validate(1); });
        });
        test("truncated_and_unqualified_extended_scene_inputs_are_rejected", [&] {
            for (std::size_t size : {0u, 51u, 52u, 100u})
                reject([&] { Monstey_WorldScene::Read(std::span(bytes).first(size), 1); });
            auto extended = bytes;
            extended.push_back(0);
            reject([&] { Monstey_WorldScene::Read(extended, 1); });
        });
        test("empty_native_scene_is_preserved_without_faking_routes", [&] {
            const Monstey_Bytes empty(52, 0);
            const auto scene = Monstey_WorldScene::Read(empty, 1);
            Monstey_Require(scene.nodes.empty() && scene.Encode(1) == empty,
                            "Empty scene fabricated a routing graph");
            reject([&] { Monstey_WorldSceneRouteSingleGroup(scene, {0, 0, 0, 1, 1, 1}); });
        });
        test("nonfinite_inverted_bounds_and_unqualified_prototypes_are_rejected", [&] {
            reject([&] { Monstey_WorldSceneRouteSingleGroup(donor, {1, 0, 0, 0, 1, 1}); });
            auto bad = donor;
            bad.nodes[0].words[3] = 0x7fc01234;
            reject([&] { bad.Validate(1); });
            bad = donor;
            bad.header[4] = 2;
            reject([&] { Monstey_WorldSceneRouteSingleGroup(bad, {0, 0, 0, 1, 1, 1}); });
        });
        if (argc > 2)
            test("original_native_scene_tail_reconstructs_byte_exactly", [&] {
                const auto raw = Monstey_ReadFile(std::filesystem::path(argv[2]));
                const auto offset = argc > 3 ? std::stoull(argv[3]) : 396976ull;
                Monstey_CheckRange(raw, static_cast<std::size_t>(offset), 52);
                const auto tail = std::span(raw).subspan(static_cast<std::size_t>(offset));
                const auto scene = Monstey_WorldScene::Read(tail, 1);
                const auto encoded = scene.Encode(1);
                Monstey_Require(std::equal(encoded.begin(), encoded.end(), tail.begin(), tail.end()),
                                "Original native scene bytes changed");
                report["native_scene"] = scene.Inspect(1);
                report["native_source_sha256"] = Monstey_HashSha256(raw);
                report["scene_offset"] = offset;
                (void)Monstey_NativeSceneReplica(tail);
                const auto authored = Monstey_WorldSceneRouteSingleGroup(scene, {0, 0, 0, 100, 200, 300});
                Monstey_Write(out / L"native-prototype-single-group-scene.bin", authored.Encode(1));
                const auto visible = Monstey_NativeMinimalVisibilityReplica(
                    Monstey_NativeSceneReplica(authored.Encode(1)), {10, 20, 30, 11, 21, 31});
                Monstey_Require(visible.size() == 1 && visible[0u].as_uint() == 0,
                                "Original prototype minimal visibility route failed");
                report["minimal_route_submitted_groups"] = visible;
            });
        report["passed"] = static_cast<std::uint64_t>(report["cases"].size());
        const auto text = report.dump(2);
        Monstey_Write(out / L"world-scene-tests.json", Monstey_Bytes(text.begin(), text.end()));
        std::cout << "Native world scene tests: " << report["passed"].as_uint() << " passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}