#include "Monstey_Anix.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
void Monstey_RequireTest(bool value, std::string_view message) {
  if (!value)
    throw std::runtime_error(std::string(message));
}
void Monstey_Reject(const std::function<void()> &action) {
  try {
    action();
  } catch (const Monstey_Error &) {
    return;
  }
  throw std::runtime_error("Expected a bounded validation error");
}
B Monstey_Av01Fixture(std::size_t tracks = 1,
                      std::vector<std::uint32_t> ticks = {0, 1000}) {
  B bytes(16);
  Monstey_PutU32(bytes, 0, ticks.front());
  Monstey_PutU32(bytes, 4, ticks.back());
  Monstey_PutU32(bytes, 8, static_cast<std::uint32_t>(ticks.size()));
  Monstey_PutU32(bytes, 12, 0xa55a5aa5);
  for (std::size_t i = 0; i < ticks.size(); ++i)
    Monstey_AppendU16(
        bytes, i ? static_cast<std::uint16_t>(ticks[i] - ticks[i - 1]) : 0);
  bytes.resize((bytes.size() + 15) / 16 * 16, 0);
  const auto stride = (tracks + 3) / 4 * 4;
  for (std::size_t k = 0; k < ticks.size(); ++k)
    for (std::size_t tr = 0; tr < stride; ++tr)
      Monstey_AppendU32(bytes, tr >= tracks
                                   ? 0xffffffffu
                                   : (k ? (1022u << 2) | (511u << 12) : 0u));
  return bytes;
}
J Monstey_Decode(const B &bytes, std::size_t tracks = 1,
                 std::vector<double> ticks = {0, 500, 1000}) {
  return Monstey_AnixDecodeCodec("AV01", bytes, tracks, {}, ticks);
}
B Monstey_Av01Library(bool opaque_extra = false, bool interior_key = false) {
  const auto channels = J::array(
      {J::object({{"type_hash", "00ce66fc"}, {"bone_hash", "12345678"}}),
       J::object({{"type_hash", "210c8f5a"}, {"bone_hash", "12345678"}})});
  const auto clip = J::object(
      {{"name", "bind"},
       {"duration_ticks", 1000},
       {"ticks", J::array({0, 1000})},
       {"tracks",
        J::array({J::object({{"channel_index", 0},
                             {"values", J::array({J::array({0, 0, 0}),
                                                  J::array({1, 2, 3})})}})})}});
  const auto original =
      Monstey_Anix::Create(J::object({{"library_path", "synthetic/av01.anix"},
                                      {"channels", channels},
                                      {"clips", J::array({clip})}}))
          .Build();
  const auto payload = Monstey_U32(original, 24),
             oldmeta = Monstey_U32(original, 32);
  B segment(opaque_extra ? 32 : 16);
  Monstey_PutU32(segment, 0, opaque_extra ? 2 : 1);
  Monstey_PutU16(segment, 4, static_cast<std::uint16_t>(segment.size()));
  Monstey_PutU32(segment, opaque_extra ? 12 : 8, 0x2b99dbc1);
  if (opaque_extra)
    Monstey_PutU32(segment, 16, 0x1234abcd);
  const auto block = interior_key ? Monstey_Av01Fixture(1, {0, 333, 1000})
                                  : Monstey_Av01Fixture();
  segment.insert(segment.end(), block.begin(), block.end());
  Monstey_PutU16(segment, 6, static_cast<std::uint16_t>(segment.size()));
  for (const auto word : {3u, 0u, 0u, 0u, 777u, 1000u})
    Monstey_AppendU32(segment, word);
  if (opaque_extra) {
    Monstey_PutU16(segment, 8, static_cast<std::uint16_t>(segment.size()));
    Monstey_AppendU32(segment, 0xdeadbeef);
  }
  segment.resize((segment.size() + 15) / 16 * 16, 0);
  B meta(original.begin() + oldmeta, original.end());
  const auto descriptor = Monstey_U32(meta, 20);
  Monstey_RequireTest(Monstey_U16(meta, descriptor + 56) == 1,
                      "Fixture must use one codec");
  std::copy_n("AV01", 4, meta.begin() + descriptor + 124);
  const std::size_t tracks_end = descriptor + 124 + 4 + 1;
  const auto ids = (tracks_end + 3) / 4 * 4;
  const auto segments = (ids + 2 + 3) / 4 * 4;
  Monstey_PutU32(meta, segments, 0);
  Monstey_PutU32(meta, segments + 4,
                 static_cast<std::uint32_t>(segment.size()));
  B bytes(original.begin(), original.begin() + payload);
  bytes.insert(bytes.end(), segment.begin(), segment.end());
  Monstey_PutU32(bytes, 28, static_cast<std::uint32_t>(segment.size()));
  Monstey_PutU32(bytes, 32, static_cast<std::uint32_t>(bytes.size()));
  bytes.insert(bytes.end(), meta.begin(), meta.end());
  Monstey_PutU32(bytes, 12, static_cast<std::uint32_t>(bytes.size()));
  return bytes;
}
std::vector<J> Monstey_BlockHashes(const J &metadata, std::string_view name) {
  for (const auto &clip : metadata["clips"].as_array())
    if (clip["name"].as_string() == name) {
      std::vector<J> hashes;
      for (const auto &segment : clip["segments"].as_array())
        hashes.push_back(segment["blocks"]);
      return hashes;
    }
  throw std::runtime_error("Missing described clip");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  if (argc < 2)
    return 2;
  const auto run = std::to_string(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  const std::filesystem::path output =
      std::filesystem::path(argv[1]) / ("Av01_" + run);
  std::filesystem::create_directories(output);
  J checks = J::array();
  std::size_t failures = 0;
  auto test = [&](std::string name, const std::function<void()> &action) {
    try {
      action();
      checks.as_array().push_back(J::object({{"name", name}, {"pass", true}}));
    } catch (const std::exception &error) {
      ++failures;
      checks.as_array().push_back(J::object(
          {{"name", name}, {"pass", false}, {"error", error.what()}}));
      std::cerr << name << ": " << error.what() << "\n";
    }
  };
  test("av01-native-vector-values-and-clamp", [] {
    const auto value = Monstey_Decode(Monstey_Av01Fixture(), 1,
                                      {-50, 0, 250, 500, 1000, 2000});
    const std::array<double, 6> expected{-256, -256, -128, 0, 256, 256};
    for (std::size_t i = 0; i < expected.size(); ++i) {
      Monstey_RequireTest(value[i][0][0].as_number() == expected[i],
                          "Unexpected packed X");
      Monstey_RequireTest(value[i][0][1].as_number() == (expected[i] / 2 - 128),
                          "Unexpected packed Y");
      Monstey_RequireTest(value[i][0][2].as_number() == -256,
                          "Unexpected packed Z");
    }
  });
  test("av01-five-tracks-use-padded-row-stride", [] {
    const auto value = Monstey_Decode(Monstey_Av01Fixture(5), 5);
    Monstey_RequireTest(value.size() == 3 && value[1].size() == 5 &&
                            value[1][4][0].as_number() == 0,
                        "Fifth track did not use stride8");
  });
  test("av01-independent-delta-time-table", [] {
    const auto value = Monstey_Decode(Monstey_Av01Fixture(1, {50, 80, 1000}), 1,
                                      {50, 65, 80, 1000});
    Monstey_RequireTest(value[1][0][0].as_number() == 0 &&
                            value[2][0][0].as_number() == 256,
                        "AV01 used LITH or uniform key spacing");
  });
  test("av01-opaque-word12-is-not-a-stride", [] {
    auto bytes = Monstey_Av01Fixture();
    const auto before = Monstey_Decode(bytes);
    Monstey_PutU32(bytes, 12, 0xffffffff);
    Monstey_RequireTest(Monstey_Decode(bytes) == before,
                        "Opaque word12 changed decode");
  });
  test("av01-padding-words-are-not-extra-tracks", [] {
    auto bytes = Monstey_Av01Fixture();
    const auto before = Monstey_Decode(bytes);
    for (std::size_t i = 36; i < 48; ++i)
      bytes[i] = static_cast<std::uint8_t>(i);
    Monstey_RequireTest(Monstey_Decode(bytes) == before,
                        "Unused row words changed decode");
  });
  test("av01-zero-samples-still-validates-layout", [] {
    Monstey_RequireTest(Monstey_Decode(Monstey_Av01Fixture(), 1, {}).empty(),
                        "Empty sample list produced rows");
    Monstey_Reject([] { Monstey_Decode(B(3), 1, {}); });
  });
  test("av01-truncated-header",
       [] { Monstey_Reject([] { Monstey_Decode(B(15)); }); });
  test("av01-invalid-track-count", [] {
    Monstey_Reject([] { Monstey_Decode(Monstey_Av01Fixture(), 0); });
    Monstey_Reject([] { Monstey_Decode(Monstey_Av01Fixture(), 65536); });
  });
  test("av01-key-count-limits", [] {
    auto bytes = Monstey_Av01Fixture();
    for (auto count : {0u, 1u, 1000001u, 0xffffffffu}) {
      Monstey_PutU32(bytes, 8, count);
      Monstey_Reject([&] { Monstey_Decode(bytes); });
    }
  });
  test("av01-rejects-truncated-deltas", [] {
    auto bytes = Monstey_Av01Fixture();
    Monstey_PutU32(bytes, 8, 100);
    Monstey_Reject([&] { Monstey_Decode(bytes); });
  });
  test("av01-rejects-first-nonzero-delta", [] {
    auto bytes = Monstey_Av01Fixture();
    Monstey_PutU16(bytes, 16, 1);
    Monstey_Reject([&] { Monstey_Decode(bytes); });
  });
  test("av01-rejects-repeated-key-time", [] {
    auto bytes = Monstey_Av01Fixture();
    Monstey_PutU16(bytes, 18, 0);
    Monstey_Reject([&] { Monstey_Decode(bytes); });
  });
  test("av01-rejects-inconsistent-end-tick", [] {
    auto bytes = Monstey_Av01Fixture();
    Monstey_PutU32(bytes, 4, 999);
    Monstey_Reject([&] { Monstey_Decode(bytes); });
  });
  test("av01-rejects-u32-time-overflow", [] {
    auto bytes = Monstey_Av01Fixture();
    Monstey_PutU32(bytes, 0, 0xfffffff0);
    Monstey_Reject([&] { Monstey_Decode(bytes); });
  });
  test("av01-rejects-truncated-padded-rows", [] {
    auto bytes = Monstey_Av01Fixture();
    bytes.pop_back();
    Monstey_Reject([&] { Monstey_Decode(bytes); });
  });
  test("av01-rejects-nonfinite-sample-time", [] {
    Monstey_Reject([] {
      Monstey_Decode(Monstey_Av01Fixture(), 1,
                     {std::numeric_limits<double>::infinity()});
    });
    Monstey_Reject([] {
      Monstey_Decode(Monstey_Av01Fixture(), 1,
                     {std::numeric_limits<double>::quiet_NaN()});
    });
  });
  test("av01-original-library-noop-is-byte-exact", [] {
    const auto bytes = Monstey_Av01Library();
    Monstey_Anix library(bytes);
    Monstey_RequireTest(library.Build() == bytes,
                        "AV01 no-op changed source bytes");
    Monstey_RequireTest(library.Describe()["opaque_codecs"].empty(),
                        "AV01 stayed opaque");
  });
  test("av01-semantic-export-includes-native-key-times", [] {
    Monstey_Anix library(Monstey_Av01Library(false, true));
    const auto clip = library.ExportClip("bind");
    Monstey_RequireTest(clip["semantic_reimport_supported"].as_bool(),
                        "AV01 semantic export remained unsupported");
    const std::array<double, 4> expected{0, 333, 777, 1000};
    Monstey_RequireTest(clip["ticks"].size() == expected.size(),
                        "Native key union count changed");
    for (std::size_t i = 0; i < expected.size(); ++i)
      Monstey_RequireTest(clip["ticks"][i].as_number() == expected[i],
                          "Native or LITH interior key is absent");
  });
  test("av01-semantic-import-uses-qualified-uatr", [] {
    Monstey_Anix library(Monstey_Av01Library());
    auto clip = library.ExportClip("bind");
    clip["name"] = "copy";
    library.Edit(J::array({J::object({{"op", "import"}, {"clip", clip}})}));
    const std::array<double, 3> times{0, 500, 1000};
    const auto a = library.Sample("bind", times, false),
               b = library.Sample("copy", times, false);
    Monstey_RequireTest(a["tracks"] == b["tracks"],
                        "UATR semantic import changed AV01 values");
    Monstey_Anix reopened(library.Build());
    Monstey_RequireTest(reopened.ExportClip("copy")["source_codecs"] ==
                            J::array({"UATR"}),
                        "Importer invented an AV01 writer");
  });
  test("av01-held-pose-and-length-edit", [] {
    Monstey_Anix library(Monstey_Av01Library());
    library.Edit(J::array({J::object({{"op", "held"},
                                      {"source", "bind"},
                                      {"name", "held"},
                                      {"tick", 500},
                                      {"duration_ticks", 200}})}));
    const std::array<double, 2> ends{0, 200};
    const auto held = library.Sample("held", ends, false);
    Monstey_RequireTest(held["tracks"][0]["values"][0] ==
                            held["tracks"][0]["values"][1],
                        "Held AV01 pose varied");
    library.Edit(J::array({J::object(
        {{"op", "length"}, {"clip", "bind"}, {"duration_ticks", 2000}})}));
    const std::array<double, 1> middle{1000};
    Monstey_RequireTest(
        library.Sample("bind", middle, false)["tracks"][0]["values"][0][0]
                .as_number() == 0,
        "Scaled timeline lost AV01 midpoint");
  });
  test("av01-event-rebuild-preserves-block-and-alignment", [] {
    Monstey_Anix library(Monstey_Av01Library(true));
    const auto before = Monstey_BlockHashes(library.Describe(), "bind");
    library.Edit(J::array({J::object(
        {{"op", "events"},
         {"clip", "bind"},
         {"events", J::array({J::object(
                        {{"tick", 500}, {"text", "SYNTH_AV01_EVENT"}})})}})}));
    Monstey_Anix reopened(library.Build());
    const auto after = Monstey_BlockHashes(reopened.Describe(), "bind");
    Monstey_RequireTest(after[0][0] == before[0][0],
                        "Original AV01 block changed on event rebuild");
    Monstey_RequireTest(reopened.EventInfo("bind")["events"].size() == 1,
                        "Rebuilt event is absent");
  });
  test("av01-unknown-channel-binding-remains-explicit", [] {
    auto bytes = Monstey_Av01Library();
    Monstey_PutU32(bytes, Monstey_U32(bytes, 20), 0x1234abcd);
    Monstey_Anix library(bytes);
    const std::array<double, 1> t{0};
    Monstey_RequireTest(library.Describe()["opaque_codecs"] ==
                            J::array({"AV01"}),
                        "Unqualified AV01 binding claimed complete");
    Monstey_Reject([&] { library.Sample("bind", t, false); });
  });
  if (argc > 2) {
    test("actual-av01-all-blocks-match-independent-float32-oracle", [&] {
      const auto manifest = J::Parse(Monstey_ReadText(argv[2]));
      std::uint64_t values = 0;
      for (const auto &block : manifest["blocks"].as_array()) {
        const auto bytes =
            Monstey_ReadFile(Monstey_Wide(block["path"].as_string()));
        Monstey_RequireTest(Monstey_HashSha256(bytes) ==
                                block["sha256"].as_string(),
                            "Actual block identity changed");
        std::vector<double> ticks;
        for (const auto &tick : block["sample_ticks"].as_array())
          ticks.push_back(tick.as_number());
        const auto samples = Monstey_AnixDecodeCodec(
            "AV01", bytes, block["track_count"].as_uint(), {}, ticks);
        for (std::size_t s = 0; s < samples.size(); ++s)
          for (std::size_t tr = 0; tr < samples[s].size(); ++tr)
            for (std::size_t c = 0; c < 3; ++c) {
              const auto bits = std::bit_cast<std::uint32_t>(
                  static_cast<float>(samples[s][tr][c].as_number()));
              Monstey_RequireTest(
                  bits == block["expected_bits"][s][tr][c].as_uint(),
                  "AV01 Float32 numeric oracle mismatch");
              ++values;
            }
      }
      Monstey_WriteNewText(output / "actual-oracle.json",
                           J::object({{"values", values},
                                      {"blocks", manifest["blocks"].size()},
                                      {"exact_float32_bits", true}})
                               .dump(2));
    });
    test(
        "actual-av01-libraries-semantic-import-and-original-block-preservation",
        [&] {
          const auto manifest = J::Parse(Monstey_ReadText(argv[2]));
          std::uint64_t clips = 0, imported = 0, held = 0, range_limited = 0,
                        compared_components = 0;
          double maximum_error = 0;
          J source_receipts = J::array();
          for (const auto &source : manifest["sources"].as_array()) {
            const std::filesystem::path path(
                Monstey_Wide(source["path"].as_string()));
            const auto bytes = Monstey_ReadFile(path);
            Monstey_RequireTest(Monstey_HashSha256(bytes) ==
                                    source["sha256"].as_string(),
                                "Original library identity changed");
            Monstey_Anix library(bytes);
            Monstey_RequireTest(library.Build() == bytes,
                                "Actual no-op changed bytes");
            const auto before = library.Describe();
            for (const auto &name : source["clips"].as_array()) {
              auto scene = library.ExportClip(name.as_string());
              const auto &eligible =
                  source["semantic_eligible_clips"].as_array();
              const bool range_eligible =
                  std::find(eligible.begin(), eligible.end(), name) !=
                  eligible.end();
              Monstey_RequireTest(
                  scene["semantic_reimport_supported"].as_bool() ==
                      range_eligible,
                  "Actual semantic support differs from independently parsed "
                  "clip/segment ranges");
              if (!range_eligible) {
                Monstey_RequireTest(
                    scene["complete_channel_decode"].as_bool() &&
                        !scene["segment_range_matches_duration"].as_bool(),
                    "Native range limitation lost its explicit diagnostic");
                ++range_limited;
                continue;
              }
              std::vector<double> probes;
              for (const auto &tick : scene["ticks"].as_array())
                probes.push_back(tick.as_number());
              const auto keys = probes;
              for (std::size_t i = 1; i < keys.size(); ++i)
                probes.push_back(std::floor((keys[i - 1] + keys[i]) / 2));
              std::sort(probes.begin(), probes.end());
              probes.erase(std::unique(probes.begin(), probes.end()),
                           probes.end());
              const auto original_samples =
                  library.Sample(name.as_string(), probes, false);
              scene["name"] = "Monstey_Av01Copy_" + std::to_string(clips++);
              library.Edit(
                  J::array({J::object({{"op", "import"}, {"clip", scene}})}));
              ++imported;
              const auto authored_samples =
                  library.Sample(scene["name"].as_string(), probes, false);
              for (const auto &channel :
                   source["clip_channels"][name.as_string()].as_array()) {
                auto find_track = [&](const J &pose) -> const J & {
                  const auto &tracks = pose["tracks"].as_array();
                  const auto found = std::find_if(
                      tracks.begin(), tracks.end(), [&](const auto &track) {
                        return track["channel_index"].as_uint() ==
                               channel.as_uint();
                      });
                  Monstey_RequireTest(found != tracks.end(),
                                      "Native AV01 channel disappeared");
                  return *found;
                };
                const auto &a = find_track(original_samples);
                const auto &z = find_track(authored_samples);
                for (std::size_t sample = 0; sample < probes.size(); ++sample)
                  for (std::size_t c = 0; c < 3; ++c) {
                    const double error =
                        std::abs(a["values"][sample][c].as_number() -
                                 z["values"][sample][c].as_number());
                    maximum_error = std::max(maximum_error, error);
                    Monstey_RequireTest(error <= 0.0001,
                                        "Reencoded UATR differs from AV01 "
                                        "source beyond 0.0001 cm");
                    ++compared_components;
                  }
              }
            }
            const auto rebuilt = library.Build();
            Monstey_Anix reopened(rebuilt);
            const auto after = reopened.Describe();
            for (const auto &clip : before["clips"].as_array())
              Monstey_RequireTest(
                  Monstey_BlockHashes(before, clip["name"].as_string()) ==
                      Monstey_BlockHashes(after, clip["name"].as_string()),
                  "An original codec block changed after import");
            if (!source["clips"].empty()) {
              library.Edit(J::array({J::object(
                  {{"op", "held"},
                   {"source", source["clips"][0]},
                   {"name", "Monstey_Av01Held"},
                   {"tick", source["clip_ranges"]
                                  [source["clips"][0].as_string()]["first"]},
                   {"duration_ticks", 1000}})}));
              ++held;
              (void)Monstey_Anix(library.Build());
            }
            Monstey_RequireTest(Monstey_HashSha256(path) ==
                                    source["sha256"].as_string(),
                                "Input library changed");
            source_receipts.as_array().push_back(
                J::object({{"path", source["path"]},
                           {"sha256", source["sha256"]},
                           {"imported_clips", source["clips"].size()},
                           {"source_unchanged", true},
                           {"noop_byte_exact", true}}));
          }
          Monstey_WriteNewText(
              output / "actual-authoring.json",
              J::object({{"sources", source_receipts},
                         {"imported_clips", imported},
                         {"held_clips", held},
                         {"range_limited_clips", range_limited},
                         {"compared_components", compared_components},
                         {"maximum_error_cm", maximum_error},
                         {"original_blocks_preserved", true},
                         {"runtime_verified", false}})
                  .dump(2));
        });
  }
  const auto report = J::object({{"schema", "Monstey.AnixAv01Tests/1"},
                                 {"groups", checks.size()},
                                 {"failures", failures},
                                 {"checks", checks},
                                 {"runtime_verified", false}});
  Monstey_WriteNewText(output / "av01-tests.json", report.dump(2));
  std::cout << checks.size() << " groups, " << failures << " failures; "
            << Monstey_Utf8(output.wstring()) << "\n";
  return failures ? 1 : 0;
}
