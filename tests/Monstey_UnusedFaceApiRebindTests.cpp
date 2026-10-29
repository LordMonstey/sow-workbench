#include "Monstey_Havok.hpp"
#include "Monstey_NavStaticTreeApi.hpp"
#include "Monstey_Protocol.hpp"
#include "Monstey_ReviewNativeNavFixture.hpp"
#include "Monstey_Runtime.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <iostream>
#include <limits>
using namespace Monstey;
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
unsigned Monstey_Groups = 0, Monstey_Pages = 0, Monstey_Rejections = 0,
         Monstey_Checks = 0;
void Monstey_Check(bool ok, const char *why) {
  Monstey_Require(ok, why);
  ++Monstey_Checks;
}
B Monstey_Envelope(const B &tag) {
  B out{'H', 'K', 'A', 'I'};
  Monstey_AppendU32(out, 3);
  Monstey_AppendU32(out, 0xffffd00d);
  Monstey_AppendU32(out, static_cast<std::uint32_t>(tag.size()));
  out.insert(out.end(), tag.begin(), tag.end());
  Monstey_AppendU32(out, 0xffffdead);
  return out;
}
B Monstey_UnusedSource(unsigned faces, std::span<const unsigned> unused,
                       std::int32_t first, std::int16_t count, bool symmetric) {
  const auto initial = Monstey_ReviewNativeNavFixture(faces);
  Monstey_Havok graph(std::span(initial).subspan(16, initial.size() - 20));
  const auto mesh = graph.Objects("hkaiNavMesh").front();
  const auto ids = graph.Node(graph.Field(mesh, "faces")).elements;
  const auto vertices = graph.Node(graph.Field(mesh, "vertices")).elements;
  const std::array<std::array<float, 4>, 3> points =
      symmetric ? std::array<std::array<float, 4>, 3>{{{-1, -1, -1, 0},
                                                       {1, 1, 1, 0},
                                                       {-1, 1, 1, 0}}}
                : std::array<std::array<float, 4>, 3>{
                      {{0, 0, 0, 0}, {1, 0, 0, 0}, {0, 0, 1, 0}}};
  for (unsigned i = 0; i < faces; ++i) {
    const bool empty =
        std::find(unused.begin(), unused.end(), i) != unused.end();
    if (empty) {
      graph.Node(graph.Field(ids[i], "startEdgeIndex")).scalar = first;
      graph.Node(graph.Field(ids[i], "numEdges")).scalar =
          static_cast<int>(count);
    } else {
      for (unsigned p = 0; p < 3; ++p) {
        const auto lanes = graph.Node(vertices[i * 3 + p]).elements;
        for (unsigned axis = 0; axis < 4; ++axis)
          graph.Node(lanes[axis]).scalar = static_cast<double>(points[p][axis]);
      }
    }
  }
  return Monstey_Envelope(graph.Encode());
}
void Monstey_Payloads(const J &result, unsigned count) {
  Monstey_Check(result.at("faces").as_uint() == count &&
                    result.at("total_nodes").as_uint() == count * 2 - 1,
                "API dropped native unused faces");
  std::vector<unsigned> ids;
  for (const auto &row : result.at("nodes_hex").as_array()) {
    const auto &s = row.as_string();
    Monstey_Check(s.size() == 12, "Bad native node encoding");
    const auto byte = [&](unsigned p) {
      return static_cast<unsigned>(std::stoul(s.substr(p * 2, 2), nullptr, 16));
    };
    const auto tag = byte(3);
    if ((tag & 0x80u) == 0)
      ids.push_back(((tag & 0x7fu) << 16) | byte(4) | (byte(5) << 8));
  }
  std::sort(ids.begin(), ids.end());
  Monstey_Check(ids.size() == count, "API lost unused leaf payloads");
  for (unsigned i = 0; i < count; ++i)
    Monstey_Check(ids[i] == i, "API renumbered unused payloads");
}
J Monstey_Page(const B &bytes, unsigned count) {
  const auto before = bytes;
  const auto result =
      Monstey_NavStaticTreeFromBytes(bytes, J::object({{"limit", 512}}));
  Monstey_Check(before == bytes &&
                    result.at("source_sha256") == J(Monstey_HashSha256(bytes)),
                "Unused source changed");
  Monstey_Payloads(result, count);
  ++Monstey_Pages;
  return result;
}
void Monstey_Reject(const B &bytes, const char *expected) {
  const auto before = bytes;
  std::string error;
  try {
    (void)Monstey_NavStaticTreeFromBytes(bytes);
  } catch (const std::exception &e) {
    error = e.what();
  }
  Monstey_Check(error.find(expected) != std::string::npos && before == bytes,
                "Native invalid constructor boundary was misclassified or "
                "source changed");
  ++Monstey_Rejections;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Check(argc == 2, "NEW_DIRECTORY required");
    const std::filesystem::path out = argv[1];
    Monstey_Check(!std::filesystem::exists(out),
                  "Fresh output directory required");
    std::filesystem::create_directories(out);
    const std::array<unsigned, 1> one{0};
    const std::array<std::pair<std::int32_t, std::int16_t>, 7> profiles{
        {{-1, std::int16_t{0}},
         {std::numeric_limits<std::int32_t>::min(), std::int16_t{0}},
         {std::numeric_limits<std::int32_t>::max(), std::int16_t{0}},
         {0, std::int16_t{-1}},
         {0, std::int16_t{-32768}},
         {std::numeric_limits<std::int32_t>::max(), std::int16_t{-32768}},
         {std::numeric_limits<std::int32_t>::max(), std::int16_t{1}}}};
    for (const auto [first, count] : profiles)
      (void)Monstey_Page(Monstey_UnusedSource(3, one, first, count, true), 3);
    ++Monstey_Groups;
    const std::array<unsigned, 2> two{0, 1};
    const auto mixed = Monstey_UnusedSource(4, two, -1, 0, true);
    const auto mixedResult = Monstey_Page(mixed, 4);
    Monstey_Check(mixedResult.at("total_nodes") == J(7),
                  "Two-unused native-safe profile rejected");
    ++Monstey_Groups;
    const std::array<unsigned, 3> all{0, 1, 2};
    Monstey_Reject(Monstey_UnusedSource(3, all, -1, 0, true), "bin split");
    Monstey_Reject(Monstey_UnusedSource(4, two, -1, 0, false), "pair merge");
    Monstey_Reject(
        Monstey_UnusedSource(3, one, std::numeric_limits<std::int32_t>::min(),
                             -1, true),
        "first edge is negative");
    ++Monstey_Groups;
    const auto path = out / L"unused-mixed.hkai";
    Monstey_WriteNewFile(path, mixed);
    const auto sha = Monstey_HashSha256(mixed);
    const auto args = J::object({{"path", Monstey_Utf8(path.wstring())},
                                 {"expected_sha256", sha},
                                 {"limit", 512}});
    const auto read = Monstey_NavStaticTreeInspect(args);
    Monstey_Check(read.at("tree_sha256") == mixedResult.at("tree_sha256") &&
                      read.at("source_unchanged").as_bool(),
                  "File adapter did not bind new unused-face grammar");
    Monstey_Runtime runtime({}, out / L"absent-settings.json");
    const auto request =
        J::object({{"operation", "sow_world_navigation_static_tree"},
                   {"arguments", args}});
    const auto before = runtime.Call(
        J::object({{"operation", "describe"}, {"arguments", J::object()}}));
    const auto result = runtime.Call(request);
    Monstey_Check(
        result.at("ok").as_bool() &&
            result.at("generation") == before.at("generation") &&
            result.at("revision") == before.at("revision") &&
            result.at("result").at("tree_sha256") ==
                mixedResult.at("tree_sha256"),
        "World operation changed state or failed to bind new grammar");
    ++Monstey_Groups;
    Monstey_Mcp mcp(runtime);
    (void)mcp.Handle(
        J::object({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}}));
    const auto reply = mcp.Handle(J::object(
        {{"jsonrpc", "2.0"},
         {"id", 2},
         {"method", "tools/call"},
         {"params", J::object({{"name", "sow_world_navigation_static_tree"},
                               {"arguments", args}})}}));
    Monstey_Check(reply.has_value() &&
                      !reply->at("result").at("isError").as_bool(),
                  "MCP did not accept new safe unused-face grammar");
    const auto native =
        J::Parse(reply->at("result").at("content")[0u].at("text").as_string());
    Monstey_Check(native.at("generation") == before.at("generation") &&
                      native.at("revision") == before.at("revision") &&
                      native.at("result").at("tree_sha256") ==
                          mixedResult.at("tree_sha256"),
                  "MCP unused-face response changed counters or tree");
    ++Monstey_Groups;
    Monstey_Check(Monstey_Groups == 5 && Monstey_Pages == 8 &&
                      Monstey_Rejections == 3,
                  "Mandatory rebound domain coverage incomplete");
    Monstey_Check(Monstey_HashSha256(path) == sha &&
                      !std::filesystem::exists(out / L"absent-settings.json"),
                  "Unused source or settings changed");
    Monstey_WriteNewText(
        out / L"report.json",
        J::object({{"status", "PASS"},
                   {"groups", Monstey_Groups},
                   {"positive_page_cases", Monstey_Pages},
                   {"native_boundary_rejections", Monstey_Rejections},
                   {"assertions", Monstey_Checks},
                   {"mixed_source_sha256", sha},
                   {"mixed_tree_sha256", mixedResult.at("tree_sha256")},
                   {"mixed_nodes", 7},
                   {"source_and_session_unchanged", true},
                   {"API_source_unchanged", true},
                   {"game_or_original_program_called", false}})
                .dump(2) +
            "\n");
    std::cout << "PASS " << Monstey_Groups
              << " new unused-face API groups; pages=" << Monstey_Pages
              << " rejections=" << Monstey_Rejections
              << " checks=" << Monstey_Checks << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
