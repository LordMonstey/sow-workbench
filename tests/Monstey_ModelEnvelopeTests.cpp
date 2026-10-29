#include "Monstey_Model.hpp"
#include "Monstey_ModelEnvelope.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
using namespace Monstey;
namespace {
void Monstey_AddFloat(Monstey_Bytes &b, float f) {
  Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(f));
}
void Monstey_WriteTest(const std::filesystem::path &path,
                       const Monstey_Bytes &data) {
  if (std::filesystem::exists(path))
    std::filesystem::remove(path);
  Monstey_WriteNewFile(path, data);
}
Monstey_Bytes Monstey_MeshFixture(bool skinned = true) {
  Monstey_Bytes vertices;
  const std::uint16_t positions[3][4] = {
      {0, 0, 0, 0x3c00}, {0x5640, 0, 0, 0x3c00}, {0, 0x5640, 0, 0x3c00}};
  for (const auto &p : positions) {
    for (const auto v : p)
      Monstey_AppendU16(vertices, v);
    if (skinned) {
      for (const auto v : {63u, 128u, 64u, 0u})
        vertices.push_back(static_cast<std::uint8_t>(v));
      for (const auto v : {2u, 1u, 0u, 1u})
        vertices.push_back(static_cast<std::uint8_t>(v));
    }
  }
  for (unsigned i = 0; i < 3; ++i) {
    Monstey_AppendU16(vertices, i == 1 ? 32767 : 0);
    Monstey_AppendU16(vertices, i == 2 ? 32767 : 0);
    for (const auto v :
         {0u, 0u, 0x3c00u, 0u, 0x3c00u, 0u, 0u, 0x3c00u, 0u, 0x3c00u, 0u, 0u})
      Monstey_AppendU16(vertices, static_cast<std::uint16_t>(v));
    for (const auto v : {11u + i, 33u + i, 77u + i, 255u})
      vertices.push_back(static_cast<std::uint8_t>(v));
  }
  Monstey_Bytes faces;
  for (const auto v : {0u, 1u, 2u})
    Monstey_AppendU16(faces, static_cast<std::uint16_t>(v));
  const Monstey_Bytes palette =
      skinned ? Monstey_Bytes{0, 2, 3} : Monstey_Bytes{};
  Monstey_Bytes desc;
  for (const auto v : {0xebaec3fau, 3u, 2u, 0u, 1u, 2u, 0u, 1u, 0xffffffffu})
    Monstey_AppendU32(desc, v);
  for (const auto size : {static_cast<std::uint32_t>(vertices.size()),
                          static_cast<std::uint32_t>(faces.size()),
                          static_cast<std::uint32_t>(palette.size())})
    for (unsigned c = 0; c < 2; ++c)
      Monstey_AppendU32(desc, size);
  std::array<std::uint32_t, 18> h{};
  h[0] = 0x1a1541bc;
  h[2] = 3;
  h[3] = 2;
  h[4] = 2;
  h[5] = 1;
  h[9] = 1;
  h[10] = skinned ? 8 : 6;
  h[16] = 4;
  h[17] = 160;
  Monstey_Bytes info;
  for (const auto v : h)
    Monstey_AppendU32(info, v);
  info.insert(info.end(), 48, 0x22);
  for (unsigned c = 0; c < 2; ++c) {
    for (const auto v : {static_cast<std::uint32_t>(vertices.size()),
                         static_cast<std::uint32_t>(faces.size()),
                         static_cast<std::uint32_t>(palette.size()), 1u})
      Monstey_AppendU32(info, v);
    for (unsigned i = 0; i < 16; ++i)
      Monstey_AppendU16(info, i % 3 ? 1 : 0);
  }
  for (unsigned c = 0; c < 2; ++c)
    for (const auto v :
         {0u, 0u, 3u, 0u, 3u, 0u, static_cast<std::uint32_t>(palette.size()),
          0x11223344u + c, 0x55667788u, 0u})
      Monstey_AppendU32(info, v);
  Monstey_AppendU32(info, 0x8e3e068e);
  Monstey_AppendU32(info, 0x37d749a6);
  Monstey_AppendU32(info, skinned ? 8 : 6);
  for (const auto &a :
       std::array<std::array<std::uint8_t, 4>, 8>{{{{0, 12, 0, 0}},
                                                   {{0, 4, 6, 0}},
                                                   {{0, 4, 7, 0}},
                                                   {{1, 9, 4, 0}},
                                                   {{1, 12, 1, 0}},
                                                   {{1, 12, 2, 0}},
                                                   {{1, 12, 3, 0}},
                                                   {{1, 4, 5, 0}}}})
    if (skinned || (a[2] != 6 && a[2] != 7))
      info.insert(info.end(), a.begin(), a.end());
  Monstey_AppendU32(info, 0x93d9a424);
  const int parents[] = {-1, 0, 1, 0};
  const unsigned children[] = {2, 1, 0, 0};
  const float positionsCm[4][3] = {
      {0, 0, 0}, {0, 50, 0}, {10, 100, 0}, {-40, 0, 0}};
  for (unsigned i = 0; i < 4; ++i) {
    Monstey_AppendU32(info, 0x1000 + i);
    Monstey_AppendU16(info, static_cast<std::uint16_t>(parents[i]));
    Monstey_AppendU16(info, static_cast<std::uint16_t>(children[i]));
    for (const auto v : positionsCm[i])
      Monstey_AddFloat(info, v);
    for (const auto v : {0.0f, 0.0f, 0.0f, 1.0f, 1.0f})
      Monstey_AddFloat(info, v);
  }
  Monstey_Bytes data;
  Monstey_AppendU32(data, 0x95dbdb69);
  for (unsigned i = 0; i < 2; ++i)
    data.insert(data.end(), vertices.begin(), vertices.end());
  for (unsigned i = 0; i < 2; ++i)
    data.insert(data.end(), faces.begin(), faces.end());
  for (unsigned i = 0; i < 2; ++i)
    data.insert(data.end(), palette.begin(), palette.end());
  Monstey_Bytes out;
  for (const auto v :
       {0x48534d4du, 17u, static_cast<std::uint32_t>(desc.size()),
        static_cast<std::uint32_t>(info.size()),
        static_cast<std::uint32_t>(data.size())})
    Monstey_AppendU32(out, v);
  out.insert(out.end(), desc.begin(), desc.end());
  out.insert(out.end(), info.begin(), info.end());
  out.insert(out.end(), data.begin(), data.end());
  return out;
}
Monstey_Bytes Monstey_SkelFixture() {
  const std::string names("Root\0Arm\0Finger\0Leg\0", 20);
  Monstey_Bytes b(96, 0);
  Monstey_PutU32(b, 0, 0x4c454b53);
  Monstey_PutU32(b, 4, 17);
  Monstey_PutU32(b, 12, static_cast<std::uint32_t>(names.size()));
  Monstey_PutU32(b, 40, 4);
  Monstey_AppendU32(b, 0xdcdcff00);
  b.insert(b.end(), names.begin(), names.end());
  const unsigned offsets[] = {0, 5, 9, 16}, children[] = {2, 1, 0, 0};
  const float t[4][3] = {{0, 0, 0}, {0, 50, 0}, {10, 100, 0}, {-40, 0, 0}};
  for (unsigned i = 0; i < 4; ++i) {
    Monstey_AppendU32(b, offsets[i]);
    b.push_back(1);
    for (const auto v : t[i])
      Monstey_AddFloat(b, v);
    for (const auto v : {0.0f, 0.0f, 0.0f, 1.0f})
      Monstey_AddFloat(b, v);
    Monstey_AppendU32(b, children[i]);
  }
  return b;
}
using J = Monstey_Json;
J Monstey_Arguments(const std::filesystem::path &baseline,
                    const std::filesystem::path &mesh,
                    const std::filesystem::path &skel) {
  return J::object({{"baseline_mesh", Monstey_Utf8(baseline.wstring())},
                    {"encoded_mesh", Monstey_Utf8(mesh.wstring())},
                    {"skeleton", Monstey_Utf8(skel.wstring())},
                    {"expected_baseline_sha256", Monstey_HashSha256(baseline)},
                    {"expected_encoded_sha256", Monstey_HashSha256(mesh)},
                    {"expected_skeleton_sha256", Monstey_HashSha256(skel)}});
}
void Monstey_Failure(const std::function<void()> &action,
                     const std::string &code = {}) {
  bool failed = false;
  try {
    action();
  } catch (const Monstey_Error &e) {
    failed = true;
    Monstey_Require(code.empty() || code == e.code,
                    "Wrong native error: " + e.code);
  } catch (const std::exception &) {
    failed = true;
    Monstey_Require(code.empty(), "Expected a typed native error");
  }
  Monstey_Require(failed, "Expected failure did not occur");
}
void Monstey_OnlyRadius(const Monstey_Bytes &source,
                        const Monstey_Bytes &result) {
  Monstey_Require(source.size() == result.size(),
                  "Envelope changed SKEL length");
  for (std::size_t i = 0; i < source.size(); ++i)
    if (i < 68 || i >= 72)
      Monstey_Require(source[i] == result[i], "Envelope changed another byte");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Require(argc == 2 || argc == 4,
                    "Use output-directory [original.mesh original.skel]");
    const auto directory =
        std::filesystem::path(argv[1]) /
        (L"Envelope_" +
         std::to_wstring(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    J checks = J::array();
    auto test = [&](const char *name, const std::function<void()> &action) {
      action();
      checks.as_array().push_back(name);
    };
    const auto baselinePath = directory / "baseline.mesh",
               encodedPath = directory / "encoded.mesh",
               skelPath = directory / "source.skel";
    const auto baseline = Monstey_MeshFixture();
    auto skeleton = Monstey_SkelFixture();
    Monstey_PutU32(skeleton, 68, std::bit_cast<std::uint32_t>(160.0f));
    // Deliberate opaque NaN and tail payload must remain byte exact.
    Monstey_PutU32(skeleton, 76, 0x7fc0abcdu);
    skeleton.insert(skeleton.end(), {0xde, 0xad, 0xbe, 0xef, 0x80, 0});
    Monstey_WriteTest(baselinePath, baseline);
    Monstey_WriteTest(skelPath, skeleton);
    auto scene = Monstey_ModelReadScene(baselinePath, skelPath);
    for (auto &part : scene.parts) {
      auto &p = part.channels.at("POSITION").values;
      p[3] = 300;
      p[7] = 301;
      p[8] = 1;
    }
    (void)Monstey_ModelWriteScene(scene, baselinePath, encodedPath);
    const auto args = Monstey_Arguments(baselinePath, encodedPath, skelPath);
    const auto identityArgs =
        Monstey_Arguments(baselinePath, baselinePath, skelPath);
    test("exact typed catalog", [&] {
      const auto catalog = Monstey_ModelEnvelopeCatalog();
      Monstey_Require(
          catalog.size() == 2 &&
              catalog[0u].at("inputSchema").at("required").size() == 6 &&
              catalog[1u].at("inputSchema").at("required").size() == 7,
          "Catalog schema differs");
    });
    test("independent analytic geometry and donor padding", [&] {
      const auto r = Monstey_ModelEnvelopeInspect(args);
      Monstey_Require(
          r.at("baseline").at("maximum_origin_distance_cm").as_number() ==
                  100 &&
              std::abs(
                  r.at("encoded").at("maximum_origin_distance_cm").as_number() -
                  std::sqrt(301.0 * 301.0 + 1)) < 1e-10 &&
              r.at("donor_bind_padding_cm").as_number() == 60,
          "Analytic extent/padding differs");
    });
    test("conservative float32 rounding", [&] {
      const auto r = Monstey_ModelEnvelopeInspect(args);
      const auto required = std::sqrt(301.0 * 301.0 + 1) + 60;
      float expected = static_cast<float>(required);
      if (expected < required)
        expected =
            std::nextafter(expected, std::numeric_limits<float>::infinity());
      Monstey_Require(r.at("output_radius_cm").as_number() == expected &&
                          r.at("output_radius_cm").as_number() >= required,
                      "Radius was rounded down or over-expanded");
    });
    test("explicit limited coverage", [&] {
      const auto r = Monstey_ModelEnvelopeInspect(args);
      Monstey_Require(r.at("rig_verified").as_bool() &&
                          r.at("bind_geometry_covered").as_bool() &&
                          !r.at("all_animation_poses_verified").as_bool() &&
                          !r.at("cloth_deformation_verified").as_bool() &&
                          !r.at("mesh_metadata_modified").as_bool(),
                      "Coverage was overstated");
    });
    test("unchanged native SKEL byte identity", [&] {
      auto a = identityArgs;
      a["output"] = Monstey_Utf8((directory / "identity.skel").wstring());
      const auto r = Monstey_ModelEnvelopeBuild(a);
      Monstey_Require(!r.at("changed").as_bool() &&
                          Monstey_ReadFile(directory / "identity.skel") ==
                              skeleton,
                      "No-op changed native bytes");
    });
    test("new-only growth and coupled receipt", [&] {
      auto a = args;
      a["output"] = Monstey_Utf8((directory / "grown.skel").wstring());
      a["report"] = Monstey_Utf8((directory / "grown.json").wstring());
      const auto r = Monstey_ModelEnvelopeBuild(a);
      const auto b = Monstey_ReadFile(directory / "grown.skel");
      Monstey_OnlyRadius(skeleton, b);
      Monstey_Require(
          r.at("changed").as_bool() &&
              J::Parse(Monstey_ReadText(directory / "grown.json")) == r,
          "Build/receipt differs");
    });
    test("source files preserved", [&] {
      Monstey_Require(Monstey_ReadFile(baselinePath) == baseline &&
                          Monstey_ReadFile(skelPath) == skeleton &&
                          Monstey_HashSha256(encodedPath) ==
                              args.at("expected_encoded_sha256").as_string(),
                      "Source mutated");
    });
    for (const auto *key :
         {"expected_baseline_sha256", "expected_encoded_sha256",
          "expected_skeleton_sha256"})
      test(key, [&] {
        auto a = args;
        a[key] = std::string(64, '0');
        Monstey_Failure([&] { (void)Monstey_ModelEnvelopeInspect(a); },
                        "source_hash_mismatch");
      });
    test("required SHA precondition", [&] {
      auto a = args;
      a.as_object().erase("expected_skeleton_sha256");
      Monstey_Failure([&] { (void)Monstey_ModelEnvelopeInspect(a); });
    });
    test("invalid hash syntax", [&] {
      auto a = args;
      a["expected_encoded_sha256"] = "not-a-hash";
      Monstey_Failure([&] { (void)Monstey_ModelEnvelopeInspect(a); });
    });
    test("unknown argument rejected", [&] {
      auto a = args;
      a["guess_all_poses"] = true;
      Monstey_Failure([&] { (void)Monstey_ModelEnvelopeInspect(a); });
    });
    test("empty explicit path rejected", [&] {
      auto a = args;
      a["skeleton"] = "";
      Monstey_Failure([&] { (void)Monstey_ModelEnvelopeInspect(a); });
    });
    auto badSkel = [&](const char *name, Monstey_Bytes bytes) {
      const auto path = directory / name;
      Monstey_WriteTest(path, bytes);
      auto a = Monstey_Arguments(baselinePath, encodedPath, path);
      Monstey_Failure([&] { (void)Monstey_ModelEnvelopeInspect(a); });
    };
    test("truncated source skeleton",
         [&] { badSkel("truncated.skel", Monstey_Bytes(68, 0)); });
    test("wrong SKEL version", [&] {
      auto b = skeleton;
      Monstey_PutU32(b, 4, 16);
      badSkel("wrong-version.skel", b);
    });
    test("negative radius", [&] {
      auto b = skeleton;
      Monstey_PutU32(b, 68, std::bit_cast<std::uint32_t>(-1.0f));
      badSkel("negative.skel", b);
    });
    test("NaN radius", [&] {
      auto b = skeleton;
      Monstey_PutU32(b, 68, 0x7fc01234);
      badSkel("nan.skel", b);
    });
    test("infinite radius", [&] {
      auto b = skeleton;
      Monstey_PutU32(b, 68, 0x7f800000);
      badSkel("infinite.skel", b);
    });
    test("mismatched skeleton bone count", [&] {
      auto b = skeleton;
      Monstey_PutU32(b, 40, 3);
      badSkel("bone-count.skel", b);
    });
    test("mismatched encoded native bone ID", [&] {
      auto b = Monstey_ReadFile(encodedPath);
      const Monstey_Bytes marker{0x24, 0xa4, 0xd9, 0x93};
      const auto at =
          std::search(b.begin(), b.end(), marker.begin(), marker.end());
      Monstey_Require(at != b.end(), "Test native bone marker absent");
      Monstey_PutU32(b, static_cast<std::size_t>(at - b.begin()) + 4,
                     0x10101010);
      const auto p = directory / "wrong-id.mesh";
      Monstey_WriteTest(p, b);
      Monstey_Failure([&] {
        (void)Monstey_ModelEnvelopeInspect(
            Monstey_Arguments(baselinePath, p, skelPath));
      });
    });
    test("nonfinite encoded position", [&] {
      auto b = Monstey_ReadFile(encodedPath);
      const auto data = 20ull + Monstey_U32(b, 8) + Monstey_U32(b, 12) + 4;
      Monstey_PutU16(b, static_cast<std::size_t>(data), 0x7c00);
      const auto p = directory / "infinite-position.mesh";
      Monstey_WriteTest(p, b);
      Monstey_Failure([&] {
        (void)Monstey_ModelEnvelopeInspect(
            Monstey_Arguments(baselinePath, p, skelPath));
      });
    });
    test("source output alias rejected", [&] {
      auto a = args;
      a["output"] = Monstey_Utf8(skelPath.wstring());
      Monstey_Failure([&] { (void)Monstey_ModelEnvelopeBuild(a); });
      Monstey_Require(Monstey_ReadFile(skelPath) == skeleton,
                      "Alias changed source");
    });
    test("report output alias rejected", [&] {
      auto a = args;
      a["output"] = Monstey_Utf8((directory / "alias.skel").wstring());
      a["report"] = a["output"];
      Monstey_Failure([&] { (void)Monstey_ModelEnvelopeBuild(a); });
      Monstey_Require(!std::filesystem::exists(directory / "alias.skel"),
                      "Alias published output");
    });
    test("existing output rejected", [&] {
      auto a = args;
      a["output"] = Monstey_Utf8((directory / "grown.skel").wstring());
      const auto old = Monstey_HashSha256(directory / "grown.skel");
      Monstey_Failure([&] { (void)Monstey_ModelEnvelopeBuild(a); });
      Monstey_Require(Monstey_HashSha256(directory / "grown.skel") == old,
                      "Existing output changed");
    });
    test("existing report rejects before publication", [&] {
      auto a = args;
      a["output"] = Monstey_Utf8((directory / "preflight.skel").wstring());
      a["report"] = Monstey_Utf8((directory / "grown.json").wstring());
      Monstey_Failure([&] { (void)Monstey_ModelEnvelopeBuild(a); });
      Monstey_Require(!std::filesystem::exists(directory / "preflight.skel"),
                      "Report preflight published skeleton");
    });
    test("failed report publication rolls back owned skeleton", [&] {
      const auto parent = directory / "report-parent-is-file";
      Monstey_WriteNewText(parent, "preserve");
      auto a = args;
      a["output"] = Monstey_Utf8((directory / "rollback.skel").wstring());
      a["report"] = Monstey_Utf8((parent / "receipt.json").wstring());
      Monstey_Failure([&] { (void)Monstey_ModelEnvelopeBuild(a); });
      Monstey_Require(!std::filesystem::exists(directory / "rollback.skel") &&
                          Monstey_ReadText(parent) == "preserve",
                      "Coupled publication rollback failed");
    });
    test("zero donor radius repairs coverage without invented padding", [&] {
      auto b = skeleton;
      Monstey_PutU32(b, 68, 0);
      const auto p = directory / "zero.skel";
      Monstey_WriteTest(p, b);
      const auto r = Monstey_ModelEnvelopeInspect(
          Monstey_Arguments(baselinePath, baselinePath, p));
      Monstey_Require(r.at("donor_bind_padding_cm").as_number() == 0 &&
                          r.at("output_radius_cm").as_number() == 100,
                      "Zero radius repair differs");
    });
    test("shrunken geometry retains donor sphere", [&] {
      auto s = Monstey_ModelReadScene(baselinePath, skelPath);
      for (auto &part : s.parts)
        for (auto &p : part.channels.at("POSITION").values)
          p /= 2;
      const auto p = directory / "small.mesh";
      (void)Monstey_ModelWriteScene(s, baselinePath, p);
      const auto r = Monstey_ModelEnvelopeInspect(
          Monstey_Arguments(baselinePath, p, skelPath));
      Monstey_Require(!r.at("changed").as_bool() &&
                          r.at("output_radius_cm").as_number() == 160,
                      "Shrinking geometry shrank donor sphere");
    });
    test("actual changed source rejects stale snapshot before publication",
         [&] {
           const auto changed = directory / "changed-source.mesh";
           auto bytes = Monstey_ReadFile(encodedPath);
           Monstey_WriteTest(changed, bytes);
           auto a = Monstey_Arguments(baselinePath, changed, skelPath);
           bytes.back() ^= 1;
           Monstey_WriteTest(changed, bytes);
           a["output"] =
               Monstey_Utf8((directory / "stale-source.skel").wstring());
           Monstey_Failure([&] { (void)Monstey_ModelEnvelopeBuild(a); },
                           "source_hash_mismatch");
           Monstey_Require(
               !std::filesystem::exists(directory / "stale-source.skel") &&
                   Monstey_ReadFile(changed) == bytes,
               "Changed source was overwritten or stale output published");
         });
    test("all native containers contribute to the sphere", [&] {
      auto s = Monstey_ModelReadScene(baselinePath, skelPath);
      Monstey_Require(s.parts.size() == 2, "Fixture container count changed");
      auto &values = s.parts.back().channels.at("POSITION").values;
      values[3] = 500;
      const auto mesh = directory / "last-container.mesh";
      (void)Monstey_ModelWriteScene(s, baselinePath, mesh);
      const auto r = Monstey_ModelEnvelopeInspect(
          Monstey_Arguments(baselinePath, mesh, skelPath));
      Monstey_Require(
          r.at("encoded").at("vertices").as_uint() == 6 &&
              r.at("encoded").at("maximum_origin_distance_cm").as_number() ==
                  500 &&
              r.at("output_radius_cm").as_number() == 560,
          "A later container was omitted");
    });
    test("case-insensitive SHA precondition", [&] {
      auto a = args;
      auto hash = a.at("expected_skeleton_sha256").as_string();
      std::transform(
          hash.begin(), hash.end(), hash.begin(),
          [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
      a["expected_skeleton_sha256"] = hash;
      Monstey_Require(
          Monstey_ModelEnvelopeInspect(a).at("rig_verified").as_bool(),
          "Uppercase SHA was rejected");
    });
    test("maximum finite donor sphere preserves identity", [&] {
      auto bytes = skeleton;
      Monstey_PutU32(bytes, 68, 0x7f7fffffu);
      const auto path = directory / "maximum.skel";
      Monstey_WriteTest(path, bytes);
      const auto r = Monstey_ModelEnvelopeInspect(
          Monstey_Arguments(baselinePath, baselinePath, path));
      Monstey_Require(!r.at("changed").as_bool() &&
                          r.at("output_radius_cm").as_number() ==
                              std::numeric_limits<float>::max(),
                      "Finite donor radius was overflowed");
    });
    J native = nullptr;
    if (argc == 4) {
      const std::filesystem::path mesh = argv[2], skel = argv[3];
      const auto meshHash = Monstey_HashSha256(mesh),
                 skelHash = Monstey_HashSha256(skel);
      auto originalScene = Monstey_ModelReadScene(mesh, skel);
      const auto bytes = Monstey_ReadFile(mesh);
      const auto info = 20 + Monstey_U32(bytes, 8);
      std::array<double, 3> center{};
      for (unsigned k = 0; k < 3; ++k)
        center[k] = std::bit_cast<float>(Monstey_U32(bytes, info + 48 + k * 4));
      for (auto &part : originalScene.parts) {
        auto &values = part.channels.at("POSITION").values;
        for (std::size_t i = 0; i < values.size(); ++i)
          values[i] = center[i % 3] + 3 * (values[i] - center[i % 3]);
      }
      const auto enlarged = directory / "native-3x.mesh";
      (void)Monstey_ModelWriteScene(originalScene, mesh, enlarged);
      auto a = Monstey_Arguments(mesh, enlarged, skel);
      a["output"] = Monstey_Utf8((directory / "native-grown.skel").wstring());
      a["report"] = Monstey_Utf8((directory / "native-grown.json").wstring());
      test("original pair 3x envelope authoring", [&] {
        native = Monstey_ModelEnvelopeBuild(a);
        Monstey_OnlyRadius(Monstey_ReadFile(skel),
                           Monstey_ReadFile(directory / "native-grown.skel"));
        Monstey_Require(
            native.at("output_radius_cm").as_number() >=
                    native.at("required_radius_cm").as_number() &&
                !native.at("all_animation_poses_verified").as_bool(),
            "Native envelope bound differs");
      });
      test("original pair inputs unchanged", [&] {
        Monstey_Require(Monstey_HashSha256(mesh) == meshHash &&
                            Monstey_HashSha256(skel) == skelHash,
                        "Native original inputs changed");
      });
    }
    const auto result =
        J::object({{"passed", static_cast<std::uint64_t>(checks.size())},
                   {"checks", checks},
                   {"native", native},
                   {"output_directory", Monstey_Utf8(directory.wstring())}});
    Monstey_WriteNewText(directory / "envelope-tests.json",
                         result.dump(2) + "\n");
    std::cout << result.dump(2) << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}