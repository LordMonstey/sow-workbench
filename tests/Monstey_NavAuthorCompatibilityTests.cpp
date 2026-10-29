#include "Monstey_NavFixture.hpp"
#include "Monstey_Navigation.hpp"
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
int checks = 0;
void Check(bool ok, std::string_view name) {
  Monstey_Require(ok, name);
  ++checks;
  std::cout << "PASS " << name << '\n';
}
B UnsignedFace(std::uint32_t count) {
  const auto bytes = Monstey_NavFixture::Fixture();
  Monstey_Havok g(std::span(bytes).subspan(16, bytes.size() - 20));
  const auto mesh = g.Objects("hkaiNavMesh").at(0);
  auto value = g.Plain(mesh);
  value["faces"] = J::array({J::object({{"startEdgeIndex", 0},
                                        {"startUserEdgeIndex", -1},
                                        {"numEdges", count},
                                        {"numUserEdges", 0},
                                        {"clusterIndex", -1},
                                        {"padding", 0}})});
  value["vertices"] = J::array({J::array({0, 0, 0, 0})});
  value["edges"] = J::array();
  value["edgeData"] = J::array();
  for (std::uint32_t i = 0; i < count; ++i) {
    value["edges"].as_array().push_back(
        J::object({{"a", 0},
                   {"b", 0},
                   {"oppositeEdge", 0xffffffffu},
                   {"oppositeFace", 0xffffffffu},
                   {"flags", 4},
                   {"paddingByte", 0},
                   {"userEdgeCost", 0.0}}));
    value["edgeData"].as_array().push_back(0);
  }
  value["faceData"] = J::array({-1});
  auto &cache =
      value["clearanceCacheSeedingDataSet"]["cacheDatas"][0]["initialCache"];
  cache["faceOffsets"] = J::array({0xffffffffu});
  cache["vertexClearances"] = J::array({255});
  cache["mcpData"] = J::array();
  for (std::uint32_t i = 0; i < count; ++i)
    cache["mcpData"].as_array().push_back(
        J::object({{"interpolant", 0}, {"clearance", 255}}));
  g.Replace(mesh, value);
  return Monstey_NavFixture::Envelope(g.Encode());
}
bool Accepts(const B &b) {
  try {
    (void)Monstey_NavigationInspect(b);
    return true;
  } catch (const std::exception &) {
    return false;
  }
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Require(
        argc >= 2,
        "Usage: NavAuthorCompatibilityTests baseline|candidate [actual.hkai]");
    const bool baseline = std::wstring_view(argv[1]) == L"baseline";
    Check(Accepts(UnsignedFace(3)),
          "Existing minimum three-edge acceptance preserved");
    Check(Accepts(UnsignedFace(32767)),
          "Positive S16 maximum retains existing structural acceptance, not "
          "geometric qualification");
    Check(Accepts(UnsignedFace(32768)) == baseline,
          "Alternate unsigned metadata 0x8000 follows explicit native S16 "
          "contract");
    Check(Accepts(UnsignedFace(65535)) == baseline,
          "Alternate unsigned metadata 0xffff follows explicit native S16 "
          "contract");
    if (argc >= 3) {
      const auto source = Monstey_ReadFile(argv[2]);
      const auto hash = Monstey_HashSha256(source);
      Check(hash == "b9fb6380798b09110a8db133ea6139e17db88ec5cad123fc6aa1fe1b03"
                    "8008ee",
            "Actual authored Seregost source identity pinned");
      const auto inspected = Monstey_NavigationInspect(source);
      Check(inspected.at("no_op_byte_exact").as_bool(),
            "Actual native HKAI no-op unchanged");
      const auto tag = std::span(source).subspan(16, source.size() - 20);
      Monstey_Havok g(tag);
      const auto mesh = g.Objects("hkaiNavMesh").at(0);
      const auto faces = g.Field(mesh, "faces");
      const auto face = g.Node(faces).elements.at(0);
      const auto field = g.Field(face, "numEdges");
      for (const int count : {-32768, -1, 0, 1, 2}) {
        g.Replace(field, count);
        const auto altered = Monstey_NavFixture::Envelope(g.Encode());
        Check(!Accepts(altered), "Actual signed metadata rejects non-positive "
                                 "or undersized face count");
      }
      Check(Monstey_HashSha256(std::filesystem::path(argv[2])) == hash,
            "Actual source unchanged after owned graph corruption tests");
    }
    std::cout << "PASS " << checks << " author compatibility groups\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}