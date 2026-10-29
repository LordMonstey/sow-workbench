#include "Monstey_Anix.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
using Q = Monstey_AnixQuaternion;
using V = Monstey_AnixVector;
void Monstey_UCheck(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}
void Monstey_UReject(const std::function<void()> &action) {
    try { action(); } catch (const Monstey_Error &) { return; }
    throw std::runtime_error("Expected a bounded validation error");
}
float Monstey_UFloat(std::uint32_t word) { return std::bit_cast<float>(word); }
std::uint32_t Monstey_UBits(const J &value) {
    return std::bit_cast<std::uint32_t>(static_cast<float>(value.as_number()));
}
template <std::size_t Width>
B Monstey_UFixture(const std::vector<std::uint32_t> &ticks,
                    const std::vector<std::array<float, Width>> &values, std::size_t offset = 3) {
    B bytes(offset, 0xa5);
    Monstey_PutU16(bytes, 0, static_cast<std::uint16_t>(offset));
    Monstey_AppendU32(bytes, static_cast<std::uint32_t>(ticks.size()));
    for (const auto tick : ticks) Monstey_AppendU32(bytes, tick);
    for (const auto &row : values)
        for (const float value : row) Monstey_AppendU32(bytes, std::bit_cast<std::uint32_t>(value));
    return bytes;
}
J Monstey_UDecode(std::string_view codec, const B &bytes, std::vector<double> ticks = {0, 50, 100},
                   std::size_t tracks = 1) {
    const std::array<std::uint32_t, 3> unrelated_lith{1, 2, 3};
    return Monstey_AnixDecodeCodec(codec, bytes, tracks, unrelated_lith, ticks);
}
J Monstey_UClip(std::uint32_t duration = 100, bool uncompressed = true) {
    auto rotation = J::object({{"channel_index", 0},
        {"values", J::array({J::array({0, 0, 0, 2}), J::array({0, 0, 0, 3})})}});
    auto position = J::object({{"channel_index", 1},
        {"values", J::array({J::array({-1, 2, 3}), J::array({9, 4, 5})})}});
    if (uncompressed) { rotation["native_codec"] = "UAQT"; position["native_codec"] = "UAVT"; }
    return J::object({{"name", "bind"}, {"duration_ticks", duration}, {"ticks", J::array({0, duration})},
        {"tracks", J::array({rotation, position})}});
}
Monstey_Anix Monstey_ULibrary(const J &clip) {
    return Monstey_Anix::Create(J::object({{"library_path", "synthetic/uncompressed.anix"},
        {"channels", J::array({J::object({{"type_hash", "27436396"}, {"bone_hash", "88baa7e7"}}),
            J::object({{"type_hash", "00ce66fc"}, {"bone_hash", "88baa7e7"}}),
            J::object({{"type_hash", "5f1d8eed"}, {"bone_hash", "12345678"}}),
            J::object({{"type_hash", "210c8f5a"}, {"bone_hash", "12345678"}})})},
        {"clips", J::array({clip})}}));
}
const J &Monstey_UTrackByCodec(const J &clip, std::string_view codec) {
    for (const auto &track : clip["tracks"].as_array())
        if (track["native_codec"].as_string() == codec)
            return track;
    throw std::runtime_error("Expected authored codec identity is absent");
}
const J &Monstey_UFindClip(const J &description, std::string_view name) {
    for (const auto &clip : description["clips"].as_array())
        if (clip["name"].as_string() == name) return clip;
    throw std::runtime_error("Missing described clip");
}
std::vector<std::string> Monstey_UNativeHashes(const J &description) {
    std::vector<std::string> hashes;
    for (const auto &clip : description["clips"].as_array())
        for (const auto &segment : clip["segments"].as_array())
            for (const auto &block : segment["blocks"].as_array())
                if (block["codec"].as_string() == "UAQT" || block["codec"].as_string() == "UAVT")
                    hashes.push_back(block["codec"].as_string() + ":" + block["sha256"].as_string());
    std::sort(hashes.begin(), hashes.end());
    return hashes;
}
std::vector<std::string> Monstey_UCodecs(const J &codecs) {
    std::vector<std::string> names;
    for (const auto &codec : codecs.as_array()) names.push_back(codec.as_string());
    std::sort(names.begin(), names.end());
    return names;
}
B Monstey_UExtractBlock(const B &bytes, std::size_t clip, std::size_t segment, std::string_view wanted) {
    const auto metadata = static_cast<std::size_t>(Monstey_U32(bytes, 32));
    const auto descriptor = metadata + Monstey_U32(bytes, metadata + 20 + 8 * clip);
    const auto codes = Monstey_U16(bytes, descriptor + 56), tracks = Monstey_U16(bytes, descriptor + 58);
    std::size_t code = codes;
    for (std::size_t i = 0; i < codes; ++i)
        if (std::string_view(reinterpret_cast<const char *>(bytes.data() + descriptor + 124 + 4 * i), 4) == wanted)
            code = i;
    Monstey_UCheck(code < codes, "Missing source block codec");
    const auto ids = (124 + 4 * static_cast<std::size_t>(codes) + tracks + 3) / 4 * 4;
    const auto directory = descriptor + (ids + 2 * tracks + 3) / 4 * 4;
    const auto begin = static_cast<std::size_t>(Monstey_U32(bytes, 24)) + Monstey_U32(bytes, directory + 16 * segment);
    const auto size = static_cast<std::size_t>(Monstey_U32(bytes, directory + 16 * segment + 4));
    const auto extras = Monstey_U32(bytes, begin);
    const auto offset = static_cast<std::size_t>(Monstey_U16(bytes, begin + 4 + 2 * code));
    std::size_t end = size;
    for (std::size_t i = 0; i < codes + extras; ++i) {
        const auto next = static_cast<std::size_t>(Monstey_U16(bytes, begin + 4 + 2 * i));
        if (next > offset) end = std::min(end, next);
    }
    Monstey_CheckRange(bytes, begin + offset, end - offset);
    return B(bytes.begin() + begin + offset, bytes.begin() + begin + end);
}
}
int wmain(int argc, wchar_t **argv) {
    if (argc < 2) return 2;
    const auto run = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const auto output = std::filesystem::path(argv[1]) / ("Uncompressed_" + run);
    std::filesystem::create_directories(output);
    J checks = J::array(), corpus = J::object(), authoring = J::object();
    std::size_t failures = 0;
    auto test = [&](std::string name, const std::function<void()> &action) {
        try { action(); checks.as_array().push_back(J::object({{"name", name}, {"pass", true}})); }
        catch (const std::exception &error) {
            ++failures;
            checks.as_array().push_back(J::object({{"name", name}, {"pass", false}, {"error", error.what()}}));
            std::cerr << name << ": " << error.what() << "\n";
        }
    };
    test("uavt-unaligned-record-absolute-time-native-lerp", [] {
        const auto bytes = Monstey_UFixture<3>({10, 30, 100}, {{1, 2, 3}, {5, 6, 7}, {12, 13, 14}});
        const auto rows = Monstey_UDecode("UAVT", bytes, {10, 20, 30, 65, 100, 500});
        const std::array<double, 6> expected{1, 3, 5, 8.5, 12, 12};
        for (std::size_t i = 0; i < expected.size(); ++i)
            Monstey_UCheck(rows[i][0][0].as_number() == expected[i], "Native absolute time or linear result differs");
    });
    test("uavt-aliases-unordered-offsets-independent-key-times", [] {
        const auto first = Monstey_UFixture<3>({0, 100}, {{0, 0, 0}, {10, 20, 30}}, 7);
        const auto second = Monstey_UFixture<3>({0, 25, 100}, {{100, 0, 0}, {200, 0, 0}, {500, 0, 0}}, 3);
        B bytes = first;
        const auto offset = static_cast<std::uint16_t>(bytes.size());
        bytes.insert(bytes.end(), second.begin() + 3, second.end());
        Monstey_PutU16(bytes, 0, offset); Monstey_PutU16(bytes, 2, 7); Monstey_PutU16(bytes, 4, offset);
        const auto rows = Monstey_UDecode("UAVT", bytes, {25, 100}, 3);
        Monstey_UCheck(rows[0][0][0].as_number() == 200 && rows[0][1][0].as_number() == 2.5 &&
            rows[0][2] == rows[0][0] && rows[1][1][2].as_number() == 30, "Offsets or native track times were conflated");
    });
    test("uaqt-captured-polynomial-bit-oracle-including-endpoints", [] {
        const float h = Monstey_UFloat(0x3f3504f3);
        const auto rows = Monstey_UDecode("UAQT", Monstey_UFixture<4>({0, 100}, {{0, 0, 0, 1}, {0, 0, h, h}}));
        const std::array<std::array<std::uint32_t, 4>, 3> expected{{
            {0, 0, 0, 0x3f7fffff}, {0, 0, 0x3ec3ef14, 0x3f6c835d}, {0, 0, 0x3f3504f2, 0x3f3504f2}}};
        for (std::size_t i = 0; i < expected.size(); ++i)
            for (std::size_t c = 0; c < 4; ++c)
                Monstey_UCheck(Monstey_UBits(rows[i][0][c]) == expected[i][c], "Captured float32 polynomial mismatch");
    });
    test("uaqt-strict-linear-threshold-and-no-normalization", [] {
        for (const auto [dot, expected] : {std::pair{0x3f7fff57u, 0x3f7fffd6u},
                                           std::pair{0x3f7fff58u, 0x3f7fffacu}}) {
            const auto rows = Monstey_UDecode("UAQT", Monstey_UFixture<4>({0, 100},
                {{1, 0, 0, 0}, {Monstey_UFloat(dot), 0, 0, 0}}), {50});
            Monstey_UCheck(Monstey_UBits(rows[0][0][0]) == expected, "Strict native threshold was approximated");
        }
        const auto rows = Monstey_UDecode("UAQT", Monstey_UFixture<4>({0, 100}, {{0, 0, 0, 2}, {0, 0, 0, 4}}), {50});
        Monstey_UCheck(rows[0][0][3].as_number() == 3, "Raw nonunit linear output was normalized");
    });
    test("uaqt-antipodal-zero-and-negative-zero-dot", [] {
        const auto antipodal = Monstey_UDecode("UAQT", Monstey_UFixture<4>({0, 100},
            {{0, 0, 0, 2}, {0, 0, 0, -2}}), {0, 50, 100});
        for (const auto &row : antipodal.as_array())
            Monstey_UCheck(row[0][3].as_number() == 2, "Shortest path sign is absent");
        const auto zero = Monstey_UDecode("UAQT", Monstey_UFixture<4>({0, 100}, {{0, 0, 0, 0}, {0, 0, 0, 0}}));
        for (const auto &component : zero[1][0].as_array())
            Monstey_UCheck(component.as_number() == 0, "Finite raw zero quaternion rejected");
        const float n = Monstey_UFloat(0x80000000);
        const auto negative_zero = Monstey_UDecode("UAQT", Monstey_UFixture<4>({0, 100}, {{n, n, n, n}, {1, 1, 1, 1}}), {50});
        for (const auto &component : negative_zero[0][0].as_array())
            Monstey_UCheck(Monstey_UBits(component) == 0x3f3504f3, "Negative zero dot took negative sign");
    });
    test("u32-before-first-wrap-last-pair-and-closed-interval", [] {
        const auto vector = Monstey_UDecode("UAVT", Monstey_UFixture<3>({100, 200, 400},
            {{-99, 0, 0}, {0, 0, 0}, {200, 0, 0}}), {0, 100, 200, 400, 0xffffffffu});
        const float alpha = static_cast<float>(std::uint32_t{0} - std::uint32_t{200}) / 200.f;
        Monstey_UCheck(vector[0][0][0].as_number() == static_cast<double>(200.f * alpha), "Before-first underflow was clamped");
        Monstey_UCheck(vector[2][0][0].as_number() == 0 && vector[4][0][0].as_number() == 200, "Closed interval/end policy differs");
        const float h = Monstey_UFloat(0x3f3504f3);
        const auto q = Monstey_UDecode("UAQT", Monstey_UFixture<4>({100, 200}, {{0, 0, 0, 1}, {0, 0, h, h}}), {0});
        Monstey_UCheck(Monstey_UBits(q[0][0][2]) == 0xbf41bdce && Monstey_UBits(q[0][0][3]) == 0x3ea0802c,
            "Huge-alpha native sine reduction differs");
    });
    test("uaqt-first-closed-interval-retains-shortest-path-sign", [] {
        const auto bytes = Monstey_UFixture<4>({0, 100, 200}, {{0, 0, 0, 1}, {0, 0, 0, -1}, {0, 0, 0, -1}});
        const auto rows = Monstey_UDecode("UAQT", bytes, {100, 101});
        Monstey_UCheck(rows[0][0][3].as_number() == 1 && rows[1][0][3].as_number() == -1,
            "An exact interior tick used the following interval instead of the first closed interval");
    });
    test("uncompressed-rejects-unqualified-query-time-forms", [] {
        const auto bytes = Monstey_UFixture<3>({0, 100}, {{0, 0, 0}, {1, 2, 3}});
        for (const double t : {-1., .5, 4294967296., std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()})
            Monstey_UReject([&] { Monstey_UDecode("UAVT", bytes, {t}); });
    });
    test("uncompressed-validates-empty-samples-and-malformed-tables", [] {
        const auto bytes = Monstey_UFixture<3>({0, 100}, {{0, 0, 0}, {1, 2, 3}});
        Monstey_UCheck(Monstey_UDecode("UAVT", bytes, {}).empty(), "Empty sample request added output");
        Monstey_UReject([&] { Monstey_UDecode("UAVT", bytes, {}, 0); });
        Monstey_UReject([&] { Monstey_UDecode("UAVT", bytes, {}, 32768); });
        for (const auto size : {0u, 1u, 6u, 15u, 38u}) {
            B truncated(bytes.begin(), bytes.begin() + size);
            Monstey_UReject([&] { Monstey_UDecode("UAVT", truncated, {}); });
        }
        for (const auto offset : {0u, 1u, 65535u}) {
            auto invalid = bytes; Monstey_PutU16(invalid, 0, static_cast<std::uint16_t>(offset));
            Monstey_UReject([&] { Monstey_UDecode("UAVT", invalid, {}); });
        }
        for (const auto count : {0u, 1u, 1000001u, 0xffffffffu}) {
            auto invalid = bytes; Monstey_PutU32(invalid, 3, count);
            Monstey_UReject([&] { Monstey_UDecode("UAVT", invalid, {}); });
        }
    });
    test("uncompressed-strict-times-and-finite-values-outputs", [] {
        for (const auto ticks : {std::vector<std::uint32_t>{1, 1}, {2, 1}})
            Monstey_UReject([&] { Monstey_UDecode("UAVT", Monstey_UFixture<3>(ticks, {{0, 0, 0}, {1, 2, 3}}), {}); });
        for (const auto word : {0x7f800000u, 0xff800000u, 0x7fc12345u})
            for (const auto codec : {"UAQT", "UAVT"}) {
                const auto bytes = codec == std::string_view("UAQT")
                    ? Monstey_UFixture<4>({0, 100}, {{0, 0, 0, 1}, {Monstey_UFloat(word), 0, 0, 1}})
                    : Monstey_UFixture<3>({0, 100}, {{0, 0, 0}, {Monstey_UFloat(word), 0, 0}});
                Monstey_UReject([&] { Monstey_UDecode(codec, bytes, {}); });
            }
        const float max = std::numeric_limits<float>::max();
        Monstey_UReject([&] { Monstey_UDecode("UAVT", Monstey_UFixture<3>({0, 100}, {{-max, 0, 0}, {max, 0, 0}}), {50}); });
        Monstey_UReject([&] { Monstey_UDecode("UAQT", Monstey_UFixture<4>({100, 101},
            {{max, max, max, max}, {max / 2, max / 2, max / 2, max / 2}}), {0}); });
    });
    test("uncompressed-writers-preserve-raw-bits-alias-and-u32-extents", [] {
        const float n = Monstey_UFloat(0x80000000), s = Monstey_UFloat(1);
        Monstey_AnixQuaternionKeys keys{{0, 0xffffffffu}, {{{n, s, 0, 2}, {n, s, 0, 2}}, {{0, 0, 0, 0}, {0, 0, 0, 0}}}};
        const auto bytes = Monstey_AnixEncodeUaqt(keys);
        Monstey_UCheck(Monstey_U16(bytes, 0) == 4 && Monstey_U16(bytes, 2) == 4 && bytes.size() == 48,
            "Exact duplicate track records were not shared");
        Monstey_UCheck(Monstey_U32(bytes, 12) == 0xffffffff && Monstey_U32(bytes, 16) == 0x80000000 &&
            Monstey_U32(bytes, 20) == 1 && Monstey_U32(bytes, 28) == 0x40000000, "Raw finite bits or u32 time were changed");
        (void)Monstey_UDecode("UAQT", bytes, {0, 4294967295.}, 2);
        const std::array<std::uint32_t, 2> ticks{0, 0xffffffffu};
        const auto vector = Monstey_AnixEncodeUavt(ticks, {{{n, s, 2}}, {{3, 4, 5}}});
        Monstey_UCheck(Monstey_U32(vector, 14) == 0x80000000 && Monstey_U32(vector, 18) == 1,
            "UAVT writer changed float bits");
    });
    test("uncompressed-writers-reject-invalid-inputs-and-u16-overflow", [] {
        Monstey_UReject([] { Monstey_AnixEncodeUaqt({}); });
        Monstey_UReject([] { Monstey_AnixEncodeUaqt({{0, 1}, {{{0, 0, 0, 0}}, {}}}); });
        Monstey_UReject([] { Monstey_AnixEncodeUaqt({{2, 1}, {{{0, 0, 0, 0}}, {{0, 0, 0, 0}}}}); });
        Monstey_UReject([] { Monstey_AnixEncodeUaqt({{0, 1}, {{{0, 0, 0, 0}}, {{0, 0, 0, std::numeric_limits<float>::infinity()}}}}); });
        Monstey_AnixQuaternionKeys keys;
        for (std::uint32_t i = 0; i < 3277; ++i) { keys.ticks.push_back(i); keys.values.push_back({Q{0, 0, 0, 1}}); }
        Monstey_UReject([&] { Monstey_AnixEncodeUaqt(keys); });
        keys = {{0, 1}, {std::vector<Q>(2000), std::vector<Q>(2000)}};
        for (std::size_t i = 0; i < 2000; ++i) keys.values[1][i][0] = static_cast<float>(i);
        Monstey_UReject([&] { Monstey_AnixEncodeUaqt(keys); });
    });
    test("explicit-native-import-preserves-raw-nonunit-root-and-defaults", [] {
        auto library = Monstey_ULibrary(Monstey_UClip());
        const auto exported = library.ExportClip("bind");
        Monstey_UCheck(Monstey_UCodecs(exported["source_codecs"]) == std::vector<std::string>{"UAQT", "UAVT"} &&
            Monstey_UTrackByCodec(exported, "UAQT")["channel_index"].as_uint() == 2 &&
            Monstey_UTrackByCodec(exported, "UAVT")["channel_index"].as_uint() == 0, "Native codecs or sample metadata missing");
        Monstey_UCheck(exported["root_start"][6].as_number() == 2 && exported["root_end"][6].as_number() == 3,
            "Root raw quaternion was normalized");
        const auto original = library.Build(); Monstey_Anix reopened(original);
        Monstey_UCheck(reopened.Build() == original, "No-op build changed uncompressed library bytes");
        auto copy = exported; copy["name"] = "copy";
        library.Edit(J::array({J::object({{"op", "import"}, {"clip", copy}})}));
        const std::array<double, 3> ticks{0, 50, 100};
        Monstey_UCheck(library.Sample("bind", ticks, false)["tracks"] == library.Sample("copy", ticks, false)["tracks"],
            "Semantic raw nonunit roundtrip changed samples");
        Monstey_UCheck(Monstey_UCodecs(Monstey_ULibrary(Monstey_UClip(100, false)).ExportClip("bind")["source_codecs"]) ==
            std::vector<std::string>{"AQ03", "UATR"}, "Existing authoring defaults changed");
    });
    test("native-metadata-descriptor-four-byte-profile-without-sixteen-byte-restriction", [] {
        const auto original = Monstey_ULibrary(Monstey_UClip()).Build();
        const auto metadata = static_cast<std::size_t>(Monstey_U32(original, 32));
        const auto offset = static_cast<std::size_t>(Monstey_U32(original, metadata + 20));
        const auto footer = Monstey_U32(original, metadata + 12);
        for (std::uint32_t shift = 1; shift <= 4; ++shift) {
            auto bytes = original;
            bytes.insert(bytes.begin() + metadata + offset, shift, 0);
            Monstey_PutU32(bytes, metadata + 20, static_cast<std::uint32_t>(offset + shift));
            Monstey_PutU32(bytes, metadata + 12, footer + shift);
            Monstey_PutU32(bytes, 36, Monstey_U32(bytes, 36) + shift);
            Monstey_PutU32(bytes, 12, static_cast<std::uint32_t>(bytes.size()));
            if (shift != 4) {
                Monstey_UReject([&] { (void)Monstey_Anix(bytes); });
                continue;
            }
            Monstey_Anix library(bytes);
            Monstey_UCheck(library.Build() == bytes, "Valid four-byte descriptor was changed on no-op");
            const std::array<double, 1> query{50};
            Monstey_UCheck(Monstey_UTrackByCodec(library.Sample("bind", query, false), "UAQT")["values"][0][3].as_number() == 2.5,
                "Four-byte descriptor changed native codec binding");
        }
    });
    test("raw-zero-import-and-contradictory-native-codec", [] {
        auto clip = Monstey_UClip(); clip["tracks"][0]["values"] = J::array({J::array({0, 0, 0, 0}), J::array({0, 0, 0, 0})});
        (void)Monstey_ULibrary(clip);
        clip["tracks"][0]["native_codec"] = "UAVT";
        Monstey_UReject([&] { Monstey_ULibrary(clip); });
    });
    test("native-long-u32-gaps-do-not-expand-without-delta-codecs", [] {
        const auto library = Monstey_ULibrary(Monstey_UClip(1000000));
        const auto clip = library.ExportClip("bind");
        Monstey_UCheck(clip["ticks"].size() == 2 && clip["ticks"][0].as_number() == 0 &&
            clip["ticks"][1].as_number() == 1000000, "Uncompressed times were needlessly expanded");
        Monstey_UCheck(Monstey_UFindClip(library.Describe(), "bind")["segments"].size() == 1, "Long native gap was split");
    });
    test("native-long-gap-expansion-uses-slerp-if-caf4-needs-keys", [] {
        auto clip = Monstey_UClip(200000);
        const float h = Monstey_UFloat(0x3f3504f3);
        clip["tracks"][0]["values"] = J::array({J::array({0, 0, 0, 1}), J::array({0, 0, h, h})});
        clip["tracks"].as_array().push_back(J::object({{"channel_index", 2}, {"native_codec", "CAF4"},
            {"values", J::array({0, 200})}}));
        const auto library = Monstey_ULibrary(clip);
        const auto bytes = library.Build();
        const auto block = Monstey_UExtractBlock(bytes, 0, 0, "UAQT");
        const auto at = static_cast<std::size_t>(Monstey_U16(block, 0));
        const auto count = static_cast<std::size_t>(Monstey_U32(block, at));
        Monstey_UCheck(count == 5 && Monstey_U32(block, at + 8) == 65535, "CAF4 did not expand long gap");
        const auto expected = Monstey_UDecode("UAQT", Monstey_UFixture<4>({0, 200000}, {{0, 0, 0, 1}, {0, 0, h, h}}), {65535});
        const auto data = at + 4 + 4 * count + 16;
        for (std::size_t c = 0; c < 4; ++c)
            Monstey_UCheck(Monstey_U32(block, data + 4 * c) == Monstey_UBits(expected[0][0][c]),
                "Inserted UAQT key used normalized linear interpolation");
    });
    test("native-many-tracks-split-and-preserve-sampled-values", [] {
        J channels = J::array(), tracks = J::array(), ticks = J::array();
        for (unsigned k = 0; k < 70; ++k) ticks.as_array().emplace_back(k * 10);
        for (unsigned tr = 0; tr < 80; ++tr) {
            channels.as_array().push_back(J::object({{"type_hash", "27436396"}, {"bone_hash", tr + 1}}));
            J values = J::array();
            for (unsigned k = 0; k < 70; ++k) values.as_array().push_back(J::array({tr, k, 0, 2}));
            tracks.as_array().push_back(J::object({{"channel_index", tr}, {"native_codec", "UAQT"}, {"values", values}}));
        }
        auto library = Monstey_Anix::Create(J::object({{"library_path", "synthetic/split.anix"}, {"channels", channels},
            {"clips", J::array({J::object({{"name", "bind"}, {"duration_ticks", 690}, {"ticks", ticks}, {"tracks", tracks}})})}}));
        const auto description = library.Describe();
        Monstey_UCheck(Monstey_UFindClip(description, "bind")["segments"].size() > 1, "64 KiB split was not exercised");
        for (const auto &segment : Monstey_UFindClip(description, "bind")["segments"].as_array())
            Monstey_UCheck(segment["bytes"].as_uint() <= 65535, "Oversized native segment escaped validation");
        const std::array<double, 2> query{0, 690};
        Monstey_UCheck(library.Sample("bind", query, false)["tracks"][79]["values"][1][1].as_number() == 69,
            "Split lost the final raw track value");
    });
    test("native-held-and-timeline-keep-codecs-and-unnormalized-values", [] {
        auto library = Monstey_ULibrary(Monstey_UClip());
        library.Edit(J::array({J::object({{"op", "held"}, {"source", "bind"}, {"name", "held"},
            {"tick", 50}, {"duration_ticks", 200}})}));
        auto held = library.ExportClip("held");
        Monstey_UCheck(Monstey_UCodecs(held["source_codecs"]) == std::vector<std::string>{"UAQT", "UAVT"} &&
            Monstey_UTrackByCodec(held, "UAQT")["values"][0][3].as_number() == 2.5, "Held raw quaternion was packed or normalized");
        library.Edit(J::array({J::object({{"op", "length"}, {"clip", "bind"}, {"duration_ticks", 200}})}));
        const std::array<double, 1> query{100};
        Monstey_UCheck(Monstey_UTrackByCodec(library.Sample("bind", query, false), "UAQT")["values"][0][3].as_number() == 2.5,
            "Timeline raw quaternion was packed or normalized");
        Monstey_UCheck(Monstey_UCodecs(library.ExportClip("bind")["source_codecs"]) == std::vector<std::string>{"UAQT", "UAVT"}, "Timeline changed native schemes");
    });
    if (argc >= 3) test("actual-corpus-native-blocks-numeric-oracle-and-byte-preservation", [&] {
        const auto evidence = std::filesystem::path(argv[2]);
        const auto manifest = J::Parse(Monstey_ReadText(evidence / "actual-layouts-and-probes.json"));
        std::map<std::string, B> files, blocks;
        std::map<std::string, std::size_t> widths;
        std::uint64_t libraries = 0, block_count = 0, key_count = 0, probes = 0, clips = 0;
        for (const auto &source : manifest["source_files"].as_array()) {
            const auto path = source["path"].as_string(); const auto bytes = Monstey_ReadFile(Monstey_Wide(path));
            Monstey_UCheck(Monstey_HashSha256(bytes) == source["sha256"].as_string(), "Original source hash changed");
            Monstey_Anix library(bytes); const auto before = library.Describe();
            Monstey_UCheck(library.Build() == bytes, "Original native library no-op was not byte exact");
            const auto hashes = Monstey_UNativeHashes(before);
            std::string renamed;
            for (const auto &clip : before["clips"].as_array())
                if (clip["name"].as_string() != "bind") { renamed = clip["name"].as_string(); break; }
            Monstey_UCheck(!renamed.empty(), "Actual source has no renameable clip");
            library.Edit(J::array({J::object({{"op", "rename"}, {"clip", renamed}, {"name", "UNCOMPRESSED_NATIVE_RENAME"}})}));
            Monstey_Anix reopened(library.Build());
            Monstey_UCheck(hashes == Monstey_UNativeHashes(reopened.Describe()), "Native source block changed during metadata edit");
            files.emplace(path, bytes); ++libraries;
        }
        for (const auto &clip : manifest["clips"].as_array()) {
            const auto path = clip["path"].as_string(), name = clip["clip"].as_string();
            Monstey_Anix library(files.at(path)); const auto exported = library.ExportClip(name);
            Monstey_UCheck(exported["complete_pose_codecs"].as_bool(), "Actual native pose stayed opaque");
            ++clips;
            for (const auto &segment : clip["segments"].as_array())
                for (const auto &code : segment["codecs"].as_array()) {
                    const auto codec = code["codec"].as_string();
                    const auto key = path + "|" + name + "|" + std::to_string(segment["segment"].as_uint()) + "|" + codec;
                    auto bytes = Monstey_UExtractBlock(files.at(path), static_cast<std::size_t>(clip["clip_index"].as_uint()),
                        static_cast<std::size_t>(segment["segment"].as_uint()), codec);
                    Monstey_UCheck(Monstey_HashSha256(bytes) == code["block_sha256"].as_string(), "Actual native block extraction/hash differs");
                    const auto tracks = static_cast<std::size_t>(code["track_count"].as_uint());
                    (void)Monstey_UDecode(codec, bytes, {}, tracks);
                    for (const auto &track : code["track_rows"].as_array()) key_count += track["key_count"].as_uint();
                    blocks.emplace(key, std::move(bytes)); widths.emplace(key, tracks); ++block_count;
                }
        }
        for (const auto &probe : manifest["probes"].as_array()) {
            const auto key = probe["path"].as_string() + "|" + probe["clip"].as_string() + "|" +
                std::to_string(probe["segment"].as_uint()) + "|" + probe["codec"].as_string();
            const auto decoded = Monstey_UDecode(probe["codec"].as_string(), blocks.at(key),
                {probe["sample_tick"].as_number()}, widths.at(key));
            const auto track = static_cast<std::size_t>(probe["track"].as_uint());
            for (std::size_t c = 0; c < probe["output"].size(); ++c)
                Monstey_UCheck(Monstey_UBits(decoded[0][track][c]) == Monstey_UBits(probe["output"][c]),
                    "Actual native float32 instruction oracle mismatch");
            ++probes;
        }
        Monstey_UCheck(libraries == 10 && clips == 26 && block_count == 140 && key_count == 202146 && probes == 416,
            "Pinned corpus coverage is incomplete");
        corpus = J::object({{"libraries", libraries}, {"clips", clips}, {"native_blocks", block_count},
            {"stored_keys_validated", key_count}, {"bit_exact_numeric_probes", probes},
            {"numeric_oracle", "captured native instruction float32 replica; original executable not run"},
            {"source_manifest_sha256", Monstey_HashSha256(evidence / "actual-layouts-and-probes.json")}});
    });
    if (argc >= 3) test("actual-26-clips-native-authoring-and-key-midpoint-comparison", [&] {
        const auto evidence = std::filesystem::path(argv[2]);
        const auto manifest = J::Parse(Monstey_ReadText(evidence / "actual-layouts-and-probes.json"));
        constexpr double quaternion_tolerance = 0.000004, vector_tolerance = 0.001;
        J rows = J::array();
        std::map<std::string, B> files;
        std::map<std::string, std::string> source_hashes;
        for (const auto &source : manifest["source_files"].as_array()) {
            const auto path = source["path"].as_string();
            auto bytes = Monstey_ReadFile(Monstey_Wide(path));
            const auto hash = Monstey_HashSha256(bytes);
            Monstey_UCheck(hash == source["sha256"].as_string(), "Actual authoring source hash differs from manifest");
            source_hashes.emplace(path, hash); files.emplace(path, std::move(bytes));
        }
        std::uint64_t passed = 0, limited = 0, failed = 0, compared_components = 0, key_queries = 0, midpoint_queries = 0;
        double quaternion_max = 0, raw_quaternion_max = 0, vector_max = 0;
        std::uint64_t sign_equivalent_samples = 0;
        for (const auto &clip : manifest["clips"].as_array()) {
            const auto path = clip["path"].as_string(), name = clip["clip"].as_string();
            J row = J::object({{"path", path}, {"clip", name}});
            try {
                const auto &original = files.at(path);
                row["source_sha256"] = source_hashes.at(path);
                Monstey_Anix library(original);
                const auto before = library.Describe();
                auto scene = library.ExportClip(name);
                row["duration_ticks"] = scene["duration_ticks"];
                row["first_tick"] = scene["ticks"][0];
                row["last_tick"] = scene["ticks"].as_array().back();
                row["source_codecs"] = scene["source_codecs"];
                for (const auto &track : scene["tracks"].as_array()) {
                    const auto type = track["type_hash"].as_string();
                    if (type == "0x27436396" || type == "0x00ce66fc")
                        Monstey_UCheck(track.contains("native_codec") &&
                            track["native_codec"].as_string() == (type == "0x27436396" ? "UAQT" : "UAVT"),
                            "Actual export lost the explicit native scheme");
                }
                scene["name"] = "UNCOMPRESSED_AUTHORING_COPY";
                if (!scene["segment_range_matches_duration"].as_bool()) {
                    Monstey_UCheck(!scene["semantic_reimport_supported"].as_bool(), "Range-mismatched source claims native authoring");
                    Monstey_UReject([&] { library.Edit(J::array({J::object({{"op", "import"}, {"clip", scene}})})); });
                    Monstey_UCheck(library.Build() == original, "Failed native import changed original bytes");
                    Monstey_UCheck(Monstey_HashSha256(Monstey_Wide(path)) == source_hashes.at(path),
                        "Range-limited native import modified an original source");
                    row["original_bytes_preserved"] = true;
                    row["status"] = "explicit_source_range_limit";
                    row["reason"] = "Semantic authoring requires keys from zero through the descriptor duration";
                    ++limited; rows.as_array().push_back(std::move(row)); continue;
                }
                Monstey_UCheck(scene["semantic_reimport_supported"].as_bool(),
                    scene.contains("semantic_reimport_error") ? scene["semantic_reimport_error"].dump() : "Qualified actual range cannot author");
                library.Edit(J::array({J::object({{"op", "import"}, {"clip", scene}})}));
                const auto built = library.Build(); Monstey_Anix reopened(built);
                const auto after = reopened.Describe();
                for (const auto &old_clip : before["clips"].as_array()) {
                    const auto &new_clip = Monstey_UFindClip(after, old_clip["name"].as_string());
                    Monstey_UCheck(old_clip["segments"].size() == new_clip["segments"].size(), "Copy changed an original segment count");
                    for (std::size_t s = 0; s < old_clip["segments"].size(); ++s)
                        Monstey_UCheck(old_clip["segments"][s]["blocks"] == new_clip["segments"][s]["blocks"],
                            "Semantic authoring changed an original block hash");
                }
                const auto copy = reopened.ExportClip("UNCOMPRESSED_AUTHORING_COPY");
                Monstey_UCheck(Monstey_UCodecs(copy["source_codecs"]) == Monstey_UCodecs(scene["source_codecs"]),
                    "Actual import changed native codecs");
                row["authored_codecs"] = copy["source_codecs"];
                std::set<std::uint32_t> keys, midpoints, queries;
                for (const auto &tick : scene["ticks"].as_array()) keys.insert(static_cast<std::uint32_t>(tick.as_number()));
                for (auto current = std::next(keys.begin()); current != keys.end(); ++current) {
                    const auto previous = *std::prev(current), next = *current;
                    const auto midpoint = previous + (next - previous) / 2;
                    if (midpoint > previous && midpoint < next) midpoints.insert(midpoint);
                }
                queries.insert(keys.begin(), keys.end()); queries.insert(midpoints.begin(), midpoints.end());
                const std::vector<double> ticks(queries.begin(), queries.end());
                double local_quaternion_max = 0, local_raw_quaternion_max = 0, local_vector_max = 0;
                std::uint64_t components = 0, local_sign_equivalent_samples = 0;
                for (std::size_t begin = 0; begin < ticks.size(); begin += 256) {
                    const auto chunk = std::span<const double>(ticks).subspan(begin, std::min<std::size_t>(256, ticks.size() - begin));
                    const auto a = reopened.Sample(name, chunk, false), b = reopened.Sample("UNCOMPRESSED_AUTHORING_COPY", chunk, false);
                    Monstey_UCheck(a["tracks"].size() == b["tracks"].size(), "Actual authored track count differs");
                    for (std::size_t tr = 0; tr < a["tracks"].size(); ++tr) {
                        const auto &left = a["tracks"][tr], &right = b["tracks"][tr];
                        Monstey_UCheck(left["channel_index"] == right["channel_index"], "Actual authored channel order differs");
                        if (!left.contains("native_codec")) continue;
                        const auto codec = left["native_codec"].as_string();
                        if (codec != "UAQT" && codec != "UAVT") continue;
                        Monstey_UCheck(right.contains("native_codec") && right["native_codec"] == left["native_codec"],
                            "Actual sampled native scheme was lost");
                        for (std::size_t k = 0; k < chunk.size(); ++k) {
                            double direct = 0, opposite = 0;
                            for (std::size_t c = 0; c < left["values"][k].size(); ++c) {
                                const double source = left["values"][k][c].as_number();
                                const double authored = right["values"][k][c].as_number();
                                direct = std::max(direct, std::abs(source - authored));
                                opposite = std::max(opposite, std::abs(source + authored));
                                ++components;
                            }
                            if (codec == "UAQT") {
                                const double equivalent = std::min(direct, opposite);
                                if (equivalent > local_quaternion_max)
                                    row["worst_quaternion_probe"] = J::object({{"tick", chunk[k]},
                                        {"channel_index", left["channel_index"]}, {"source", left["values"][k]},
                                        {"authored", right["values"][k]}, {"sign_equivalent_error", equivalent}});
                                local_quaternion_max = std::max(local_quaternion_max, equivalent);
                                local_raw_quaternion_max = std::max(local_raw_quaternion_max, direct);
                                if (opposite < direct) ++local_sign_equivalent_samples;
                            } else
                                local_vector_max = std::max(local_vector_max, direct);
                        }
                    }
                }
                quaternion_max = std::max(quaternion_max, local_quaternion_max);
                raw_quaternion_max = std::max(raw_quaternion_max, local_raw_quaternion_max);
                sign_equivalent_samples += local_sign_equivalent_samples;
                vector_max = std::max(vector_max, local_vector_max);
                compared_components += components; key_queries += keys.size(); midpoint_queries += midpoints.size();
                row["key_queries"] = static_cast<std::uint64_t>(keys.size());
                row["integer_midpoint_queries"] = static_cast<std::uint64_t>(midpoints.size());
                row["compared_components"] = components;
                row["max_quaternion_sign_equivalent_component_error"] = local_quaternion_max;
                row["max_quaternion_direct_component_error"] = local_raw_quaternion_max;
                row["quaternion_samples_using_opposite_sign"] = local_sign_equivalent_samples;
                row["max_vector_component_error_cm"] = local_vector_max;

                const auto authored_path = output / ("actual-native-copy-" + std::to_string(rows.size()) + ".anix");
                Monstey_WriteNewFile(authored_path, built);
                row["authored_path"] = Monstey_Utf8(authored_path.wstring());
                row["authored_sha256"] = Monstey_HashSha256(built);
                row["original_blocks_preserved"] = true;
                row["native_codec_retained"] = true;
                Monstey_UCheck(local_quaternion_max <= quaternion_tolerance && local_vector_max <= vector_tolerance,
                    "Actual authored native sample error exceeds the declared tolerance");
                Monstey_UCheck(Monstey_HashSha256(Monstey_Wide(path)) == source_hashes.at(path), "Actual authoring modified an original source");
                row["status"] = "pass"; ++passed;
            } catch (const std::exception &error) {
                row["status"] = "fail"; row["error"] = error.what(); ++failed;
            }
            rows.as_array().push_back(std::move(row));
        }
        authoring = J::object({{"clips", rows}, {"passed", passed}, {"explicit_source_range_limits", limited}, {"failed", failed},
            {"quaternion_sign_equivalent_component_tolerance", quaternion_tolerance}, {"vector_component_tolerance_cm", vector_tolerance},
            {"max_quaternion_sign_equivalent_component_error", quaternion_max},
            {"max_quaternion_direct_component_error", raw_quaternion_max},
            {"quaternion_samples_using_opposite_sign", sign_equivalent_samples}, {"max_vector_component_error_cm", vector_max},
            {"key_queries", key_queries}, {"integer_midpoint_queries", midpoint_queries}, {"compared_components", compared_components}});
        Monstey_UCheck(rows.size() == 26 && files.size() == 10 && passed == 22 && limited == 4 && failed == 0,
            "Actual native authoring coverage has failed or missing clips");
    });
    const auto report = J::object({{"schema", "Monstey.AnixUncompressedTests/1"},
        {"checks", checks}, {"failures", static_cast<std::uint64_t>(failures)}, {"corpus", corpus}, {"actual_authoring", authoring}});
    Monstey_WriteNewText(output / "report.json", report.dump(2) + "\n");
    std::cout << report.dump(2) << "\n";
    return failures ? 1 : 0;
}