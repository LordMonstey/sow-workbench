#include "Monstey_Physics.hpp"
#include "Monstey_TriangleCollision.hpp"
#include "Monstey_World.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <random>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
int passed = 0;
void Check(bool value, std::string_view name) {
  Monstey_Require(value, name);
  ++passed;
  std::cout << "PASS " << name << '\n';
}
void Reject(const std::function<void()> &fn, std::string_view name) {
  try {
    fn();
  } catch (const std::exception &) {
    Check(true, name);
    return;
  }
  throw std::runtime_error("Expected rejection " + std::string(name));
}
J Vector4(double x, double y, double z, double w) {
  return J::array({x, y, z, w});
}
J Mesh() {
  const auto domain = J::object(
      {{"min", Vector4(-1, -1, -1, 0)}, {"max", Vector4(2, 2, 2, 0)}});
  auto empty = []() {
    J j = J::object();
    for (const auto k : {"lx", "ly", "lz"})
      j[k] = J::array({1e30, 1e30, 1e30, 1e30});
    for (const auto k : {"hx", "hy", "hz"})
      j[k] = J::array({-1e30, -1e30, -1e30, -1e30});
    j["data"] = J::array({0, 0, 0, 0});
    return j;
  };
  auto simd = empty();
  for (const auto k : {"lx", "ly", "lz"})
    simd[k][0] = -1;
  for (const auto k : {"hx", "hy", "hz"})
    simd[k][0] = 2;
  simd["data"][0] = 1;
  const std::uint64_t q2 = std::uint64_t(4194303) << 42;
  auto tree = J::object(
      {{"domain", domain},
       {"sharedVertices", J::array({std::int64_t(0), std::int64_t(2097151),
                                    std::bit_cast<std::int64_t>(q2)})},
       {"sharedVerticesIndex", J::array({0, 1, 2})},
       {"packedVertices", J::array()},
       {"primitives",
        J::array({J::object({{"indices", J::array({0, 1, 2, 2})}})})},
       {"primitiveDataRuns",
        J::array({J::object({{"value", J::object({{"data", 7}})},
                             {"index", 0},
                             {"count", 1}})})},
       {"sections",
        J::array({J::object(
            {{"domain", domain},
             {"primitives", J::object({{"data", 1}})},
             {"sharedVertices", J::object({{"data", 0}})},
             {"firstPackedVertex", 0},
             {"page", 0},
             {"codecParms", J::array({0, 0, 0, 0, 0, 0})},
             {"dataRuns", J::object({{"data", 1}})},
             {"nodes", J::array({J::object(
                           {{"xyz", J::array({0, 0, 0})}, {"data", 0}})})},
             {"leafIndex", 0}})})},
       {"nodes",
        J::array({J::object(
            {{"xyz", J::array({0, 0, 0})}, {"hiData", 0}, {"loData", 0}})})},
       {"maxKeyValue", 0},
       {"numPrimitiveKeys", 1},
       {"bitsPerKey", 1}});
  return J::object(
      {{"data",
        J::object(
            {{"meshTree", tree},
             {"simdTree", J::object({{"nodes", J::array({empty(), simd})}})}})},
       {"triangleIsInterior",
        J::object({{"storage",
                    J::object({{"numBits", 1}, {"words", J::array({0})}})}})},
       {"numShapeKeyBits", 1},
       {"convexRadius", .01}});
}
void Synthetic() {
  const auto m = Mesh();
  const auto decoded = Monstey_DecodeCompressedMesh(m);
  Check(decoded.at("triangle_count").as_uint() == 1 &&
            decoded.at("triangles").at(0).at("material").as_int() == 7,
        "Original shared64 triangle/material decoder");
  Check(Monstey_ValidateCompressedMesh(m).at("status").as_string() == "PASS",
        "Independent primary/section/SIMD coverage validation");
  auto bad = m;
  bad["data"]["meshTree"]["nodes"][0]["loData"] = 1;
  Reject([&] { Monstey_ValidateCompressedMesh(bad); },
         "Unknown primary BVH leaf rejected");
  bad = m;
  bad["data"]["simdTree"]["nodes"][1]["data"][0] = 2;
  Reject([&] { Monstey_ValidateCompressedMesh(bad); }, "SIMD cycle rejected");
  bad = m;
  bad["data"]["meshTree"]["primitiveDataRuns"][0]["index"] = 2;
  Reject([&] { Monstey_DecodeCompressedMesh(bad); },
         "Missing primitive material rejected");
  bad = m;
  bad["data"]["meshTree"]["primitives"][0]["indices"][0] = 999;
  Reject([&] { Monstey_DecodeCompressedMesh(bad); },
         "Shared vertex range checked");
  bad = m;
  bad["triangleIsInterior"]["storage"]["numBits"] = 0;
  Reject([&] { Monstey_ValidateCompressedMesh(bad); },
         "Interior bit capacity checked");
  bad = m;
  bad["data"]["meshTree"]["bitsPerKey"] = 0;
  Reject([&] { Monstey_ValidateCompressedMesh(bad); },
         "Native shape key capacity checked");
  bad = m;
  bad["data"]["simdTree"]["nodes"][1]["lx"][0] = 0;
  Reject([&] { Monstey_ValidateCompressedMesh(bad); },
         "SIMD geometry clipping rejected");
  const auto vertices = J::array({J::array({0, 0, 0}), J::array({0, 0, 100}),
                                  J::array({100, 0, 100})}),
             triangles = J::array({J::array({0, 1, 2})});
  Reject([&] { Monstey_TriangleCook({}, J::object(), vertices, triangles, 0); },
         "Zero error budget rejected before profile access");
  Reject(
      [&] {
        Monstey_TriangleCook({}, J::object(), vertices,
                             J::array({J::array({0, 0, 1})}));
      },
      "Repeated vertex rejected before profile access");
  Reject(
      [&] {
        Monstey_TriangleCook({}, J::object(), vertices,
                             J::array({J::array({0, 1, 999})}));
      },
      "Triangle index bounds checked before profile access");
  Reject(
      [&] {
        Monstey_TriangleCook({}, J::object(),
                             J::array({J::array({0, 0, 0}), J::array({1, 1, 1}),
                                       J::array({2, 2, 2})}),
                             triangles);
      },
      "Zero-area triangle rejected before profile access");
}
void Queries(const J &mesh, const J &decoded) {
  using Bounds = std::array<std::array<double, 3>, 2>;
  auto intersects = [](const Bounds &a, const Bounds &b) {
    for (std::size_t i = 0; i < 3; ++i)
      if (a[0][i] > b[1][i] || a[1][i] < b[0][i])
        return false;
    return true;
  };
  const auto &nodes = mesh.at("data").at("simdTree").at("nodes");
  std::mt19937 rng(7721);
  std::uniform_real_distribution<double> x(-1, 14), y(-.5, .5), z(-1, 6),
      radius(.01, .5);
  std::size_t total = 0, maximum = 0;
  for (std::size_t test = 0; test < 200; ++test) {
    const std::array<double, 3> c{x(rng), y(rng), z(rng)},
        r{radius(rng), radius(rng), radius(rng)};
    Bounds query{};
    for (std::size_t i = 0; i < 3; ++i) {
      query[0][i] = c[i] - r[i];
      query[1][i] = c[i] + r[i];
    }
    std::set<std::size_t> expected, got;
    for (const auto &face : decoded.at("triangles").as_array()) {
      Bounds b{};
      for (std::size_t axis = 0; axis < 3; ++axis) {
        b[0][axis] = b[1][axis] =
            face.at("points_m").at(0).at(axis).as_number();
        for (const auto &v : face.at("points_m").as_array()) {
          b[0][axis] = std::min(b[0][axis], v.at(axis).as_number());
          b[1][axis] = std::max(b[1][axis], v.at(axis).as_number());
        }
      }
      if (intersects(query, b))
        expected.insert(static_cast<std::size_t>(face.at("key").as_uint()));
    }
    std::vector<std::size_t> stack{1};
    std::set<std::size_t> seen;
    while (!stack.empty()) {
      const auto at = stack.back();
      stack.pop_back();
      Monstey_Require(at < nodes.size() && seen.insert(at).second,
                      "Query encountered invalid SIMD graph");
      const auto &node = nodes.at(at);
      for (std::size_t lane = 0; lane < 4; ++lane) {
        Bounds b{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
          b[0][axis] =
              node.at(std::string("l") + "xyz"[axis]).at(lane).as_number();
          b[1][axis] =
              node.at(std::string("h") + "xyz"[axis]).at(lane).as_number();
        }
        if (b[0][0] > b[1][0] || b[0][1] > b[1][1] || b[0][2] > b[1][2] ||
            !intersects(query, b))
          continue;
        const auto value = node.at("data").at(lane).as_uint();
        if (value & 1)
          got.insert(static_cast<std::size_t>(value >> 1));
        else
          stack.push_back(static_cast<std::size_t>(value >> 1));
      }
    }
    for (const auto key : expected)
      Monstey_Require(got.contains(key), "SIMD query missed physical triangle");
    total += got.size();
    maximum = std::max(maximum, got.size());
  }
  Check(true,
        "Two hundred independent SIMD queries miss no physical triangles");
  Check(maximum < 130 && total < 200 * 32,
        "SIMD traversal prunes geometry rather than returning every face");
}
void Native(const std::filesystem::path &profilePath,
            const std::filesystem::path &sourcePath,
            const std::filesystem::path &out,
            const std::filesystem::path &snow) {
  const auto originalProfile = Monstey_ReadFile(profilePath);
  auto profileRaw = originalProfile;
  auto profile = J::object({{"source_sha256", Monstey_HashSha256(profileRaw)},
                            {"shape_index", 0},
                            {"route", J::array({0, 0})}});
  J vertices = J::array(), triangles = J::array(), tags = J::array();
  for (std::size_t z = 0; z <= 5; ++z)
    for (std::size_t x = 0; x <= 13; ++x)
      vertices.as_array().push_back(J::array({x * 100, 0, z * 100}));
  for (std::size_t z = 0; z < 5; ++z)
    for (std::size_t x = 0; x < 13; ++x) {
      const auto a = z * 14 + x, b = a + 14, c = b + 1, d = a + 1;
      triangles.as_array().push_back(J::array({a, b, c}));
      triangles.as_array().push_back(J::array({a, c, d}));
      tags.as_array().push_back((a & 1) ? 7 : 8);
      tags.as_array().push_back((a & 1) ? 8 : 7);
    }
  Reject(
      [&] {
        Monstey_TriangleCook(profileRaw, profile, vertices, triangles, .02,
                             tags);
      },
      "Uncompleted minimal profile rejects newly-active missing THSH");
  Monstey_Require(!snow.empty(),
                  "Native tests require explicit Snow TYPE donor source");
  const auto donorRaw = Monstey_ReadFile(snow);
  Monstey_Physics donorPhysics(Monstey_ObjresReadChunk(donorRaw, 0xb9e81cf3));
  const auto donorRows = donorPhysics.Describe(0, 500).at("shapes");
  bool prepared = false;
  std::string lastError;
  std::size_t attempted = 0;
  for (const auto &row : donorRows.as_array())
    if (row.at("type").as_uint() == 12) {
      if (++attempted > 32)
        break;
      const auto donorSpec =
          J::object({{"source_sha256", Monstey_HashSha256(donorRaw)},
                     {"shape_index", row.at("index")},
                     {"route", row.at("route")}});
      try {
        auto candidate = Monstey_TriangleCompleteProfile(profileRaw, profile,
                                                         donorRaw, donorSpec);
        auto candidateSpec = profile;
        candidateSpec["source_sha256"] = Monstey_HashSha256(candidate.bytes);
        (void)Monstey_TriangleCook(candidate.bytes, candidateSpec, vertices,
                                   triangles, .02, tags);
        profileRaw = std::move(candidate.bytes);
        profile = std::move(candidateSpec);
        prepared = true;
        Monstey_WriteNewFile(out / L"native-qualified-profile.objres",
                             profileRaw);
        candidate.report["donor_profile"] = donorSpec;
        Monstey_WriteNewText(out / L"native-profile-thsh-report.json",
                             candidate.report.dump(2));
        break;
      } catch (const std::exception &e) {
        lastError = e.what();
      }
    }
  Monstey_Require(prepared,
                  "No exact-layout native hash donor qualified: " + lastError);
  Check(
      prepared,
      "Exact-layout native THSH donor supplies missing shared64 registry hash");
  Monstey_Physics originalPhysics(
      Monstey_ObjresReadChunk(originalProfile, 0xb9e81cf3));
  const auto originalShape = originalPhysics.ShapeBytes(0, {0, 0});
  Monstey_Havok originalGraph(std::span<const std::uint8_t>(originalShape)
                                  .subspan(20, originalShape.size() - 24));
  const auto originalMesh = originalGraph.Plain(
      originalGraph.Objects("hknpCompressedMeshShape").at(0));
  Check(
      Monstey_ValidateCompressedMesh(originalMesh).at("triangles").as_uint() ==
          2,
      "Independent packed32 decoder accepts actual native minimal quad");
  const auto cooked =
      Monstey_TriangleCook(profileRaw, profile, vertices, triangles, .02, tags);
  Check(cooked.report.at("triangles").as_uint() == 130 &&
            cooked.report.at("sections").as_uint() == 3,
        "Actual native profile cooks130triangles in three sections");
  Check(cooked.report.at("max_quantization_error_cm").as_number() <= .02 &&
            cooked.report.at("spatial_audit").at("status").as_string() ==
                "PASS",
        "Native BVHs and quantization budget independently verified");
  Check(cooked.report.at("type_hash_coverage").at("status").as_string() ==
            "PASS",
        "Actual authored TYPE hashes qualified");
  Monstey_Havok graph(std::span<const std::uint8_t>(cooked.bytes)
                          .subspan(20, cooked.bytes.size() - 24));
  const auto plain =
      graph.Plain(graph.Objects("hknpCompressedMeshShape").at(0));
  const auto decoded = Monstey_DecodeCompressedMesh(plain);
  Check(decoded.at("triangle_count").as_uint() == 130,
        "Native cooker readback covers all130triangles");
  for (std::size_t i = 0; i < 130; ++i)
    Monstey_Require(decoded.at("triangles").at(i).at("material") == tags.at(i),
                    "Authored material tag changed");
  Check(true, "Authored per-triangle material tags preserved");
  double orderingError = 0;
  for (std::size_t fi = 0; fi < triangles.size(); ++fi)
    for (std::size_t vi = 0; vi < 3; ++vi) {
      const auto index =
          static_cast<std::size_t>(triangles.at(fi).at(vi).as_uint());
      double square = 0;
      for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto difference =
            vertices.at(index).at(axis).as_number() - decoded.at("triangles")
                                                              .at(fi)
                                                              .at("points_m")
                                                              .at(vi)
                                                              .at(axis)
                                                              .as_number() *
                                                          100;
        square += difference * difference;
      }
      orderingError = std::max(orderingError, std::sqrt(square));
    }
  Check(orderingError <= .02, "Independent decoded face positions, ordering "
                              "and winding preserve source triangles");
  Queries(plain, decoded);
  Monstey_WriteNewFile(out / L"native-cooked.shape", cooked.bytes);
  Monstey_WriteNewText(out / L"native-cooked-report.json",
                       cooked.report.dump(2));
  Reject(
      [&] {
        Monstey_TriangleCook(profileRaw, profile, vertices, triangles, 1e-12);
      },
      "Impossible quantization budget rejected");
  auto bad = profile;
  bad["source_sha256"] = std::string(64, '0');
  Reject([&] { Monstey_TriangleCook(profileRaw, bad, vertices, triangles); },
         "Profile source hash enforced");
  const auto source = Monstey_ReadFile(sourcePath);
  auto spec = J::object({{"source_sha256", Monstey_HashSha256(source)},
                         {"profile", profile},
                         {"vertices_cm", vertices},
                         {"triangles", triangles},
                         {"max_error_cm", .02}});
  const auto authored = Monstey_TriangleCollision(source, spec, profileRaw);
  Check(authored.report.at("status").as_string() ==
            "AUTHORED_TRIANGLE_COLLISION_RUNTIME_PENDING",
        "Actual qualified global OBJRES gains native triangle collision");
  Monstey_WriteNewFile(out / L"native-global.objres", authored.bytes);
  Monstey_WriteNewText(out / L"native-global-report.json",
                       authored.report.dump(2));
  const auto appended = Monstey_TriangleCollision(
      authored.bytes,
      J::object({{"source_sha256", Monstey_HashSha256(authored.bytes)},
                 {"profile", profile},
                 {"vertices_cm", vertices},
                 {"triangles", triangles}}),
      profileRaw);
  Check(appended.report.at("previous_global_collision_preserved").as_bool(),
        "Existing global collision retained on another addition");
  if (!snow.empty()) {
    const auto raw = Monstey_ReadFile(snow);
    spec["source_sha256"] = Monstey_HashSha256(raw);
    Reject([&] { Monstey_TriangleCollision(raw, spec, profileRaw); },
           "Unqualified Snow global definition vectors remain rejected");
  }
  Check(Monstey_HashSha256(profilePath) ==
                Monstey_HashSha256(originalProfile) &&
            Monstey_HashSha256(sourcePath) == Monstey_HashSha256(source),
        "Both native source assets unchanged");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    const auto out =
        (argc > 1 ? std::filesystem::path(argv[1])
                  : std::filesystem::path(L"triangle-tests")) /
        std::to_wstring(
            std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(out);
    Synthetic();
    if (argc > 3)
      Native(argv[2], argv[3], out,
             argc > 4 ? std::filesystem::path(argv[4])
                      : std::filesystem::path());
    std::cout << "PASS " << passed << " native triangle collision groups\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
