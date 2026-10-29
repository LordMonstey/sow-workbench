#include "Monstey_SowCatalogProjection.hpp"
#include "Monstey_Havok.hpp"
#include "Monstey_NavCompactCooker.hpp"
#include "Monstey_NavStaticTreeApi.hpp"
#include "Monstey_Protocol.hpp"
#include "Monstey_ReviewNativeNavFixture.hpp"
#include "Monstey_Runtime.hpp"
#include "Monstey_World.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
constexpr std::string_view Monstey_Operation =
    "sow_world_navigation_static_tree";
unsigned Monstey_Groups = 0, Monstey_Pages = 0, Monstey_Negatives = 0,
         Monstey_RuntimeNegatives = 0, Monstey_Checks = 0;
std::size_t Monstey_MaxPage = 0, Monstey_MaxMcp = 0;
void Monstey_Check(bool ok, const char *message) {
  Monstey_Require(ok, message);
  ++Monstey_Checks;
}
void Monstey_Reject(const std::function<void()> &fn) {
  bool failed = false;
  try {
    fn();
  } catch (const std::exception &) {
    failed = true;
  }
  Monstey_Check(failed, "Independent negative case accepted");
  ++Monstey_Negatives;
}
B Monstey_Records(const Monstey_Havok &g, std::size_t mesh,
                  std::string_view field) {
  B out;
  const auto array = g.Field(mesh, field);
  for (const auto id : g.Node(array).elements) {
    const auto &raw = g.Node(id).original_storage;
    out.insert(out.end(), raw.begin(), raw.end());
  }
  return out;
}
std::vector<std::size_t> Monstey_Meshes(const B &source) {
  Monstey_Havok g(std::span(source).subspan(16, source.size() - 20));
  return g.Objects("hkaiNavMesh");
}
std::string Monstey_HexBytes(std::span<const std::uint8_t> bytes) {
  constexpr char digits[] = "0123456789abcdef";
  std::string out;
  for (const auto b : bytes) {
    out += digits[b >> 4];
    out += digits[b & 15];
  }
  return out;
}
J Monstey_Page(const B &source, const J &args = J::object()) {
  const auto before = source;
  const auto got = Monstey_NavStaticTreeFromBytes(source, args);
  Monstey_Havok g(std::span(source).subspan(16, source.size() - 20));
  const auto mesh = static_cast<std::size_t>(got.at("mesh_node").as_uint());
  const auto faces = Monstey_Records(g, mesh, "faces"),
             edges = Monstey_Records(g, mesh, "edges"),
             vertices = Monstey_Records(g, mesh, "vertices");
  const auto expected = Monstey_NavCookCompactMesh(faces, edges, vertices);
  const auto total = expected.nodes.size() / 6;
  const auto offset = args.value("offset", 0).as_uint();
  const auto limit = args.value("limit", 256).as_uint();
  const auto begin = std::min<std::uint64_t>(offset, total);
  const auto count = std::min<std::uint64_t>(limit, total - begin);
  Monstey_Check(source == before &&
                    got.at("source_sha256") == J(Monstey_HashSha256(source)),
                "Owned input changed or source hash differs");
  Monstey_Check(got.at("faces").as_uint() == faces.size() / 16 &&
                    got.at("edges").as_uint() == edges.size() / 20 &&
                    got.at("vertices").as_uint() == vertices.size() / 16,
                "Record counts differ");
  for (const auto [name, raw] :
       std::array<std::pair<const char *, const B *>, 3>{
           {{"faces", &faces}, {"edges", &edges}, {"vertices", &vertices}}})
    Monstey_Check(got.at("source_record_sha256").at(name).as_string() ==
                      Monstey_HashSha256(*raw),
                  "Raw record digest differs");
  Monstey_Check(got.at("tree_sha256").as_string() ==
                        Monstey_HashSha256(expected.nodes) &&
                    got.at("total_nodes").as_uint() == total &&
                    got.at("total_bytes").as_uint() == expected.nodes.size(),
                "Accepted cooker output differs");
  Monstey_Check(got.at("node_stride_bytes") == J(6) &&
                    got.at("nodes_hex").size() == count &&
                    got.at("offset").as_uint() == offset &&
                    got.at("limit").as_uint() == limit,
                "Page dimensions differ");
  for (std::size_t i = 0; i < count; ++i) {
    const auto &text = got.at("nodes_hex")[i].as_string();
    Monstey_Check(
        text.size() == 12 &&
            text ==
                Monstey_HexBytes(
                    std::span(expected.nodes)
                        .subspan((static_cast<std::size_t>(begin) + i) * 6, 6)),
        "Native node page order/raw hex differs");
  }
  const auto next = count < total - begin ? J(offset + count) : J();
  Monstey_Check(got.at("next_offset") == next, "Next offset differs");
  for (unsigned axis = 0; axis < 4; ++axis) {
    Monstey_Check(
        got.at("root_bounds_f32_bits").at("minimum")[axis].as_uint() ==
            std::bit_cast<std::uint32_t>(expected.bounds.minimum[axis]),
        "Minimum raw bits differ");
    Monstey_Check(
        got.at("root_bounds_f32_bits").at("maximum")[axis].as_uint() ==
            std::bit_cast<std::uint32_t>(expected.bounds.maximum[axis]),
        "Maximum raw bits differ");
  }
  Monstey_Check(got.at("read_only").as_bool() &&
                    !got.at("runtime_verified").as_bool() &&
                    !got.at("native_scratch_W_provenance_qualified").as_bool(),
                "Qualification metadata differs");
  const auto text = got.dump();
  Monstey_Check(J::Parse(text) == got && text.size() < 30 * 1024,
                "Invalid/nonfinite or unbounded JSON page");
  Monstey_MaxPage = std::max(Monstey_MaxPage, text.size());
  ++Monstey_Pages;
  return got;
}
J Monstey_Call(Monstey_Runtime &runtime, std::string_view operation,
               const J &args = J::object()) {
  return runtime.Call(
      J::object({{"operation", operation}, {"arguments", args}}));
}
J Monstey_Good(Monstey_Runtime &runtime, std::string_view operation,
               const J &args = J::object()) {
  auto reply = Monstey_Call(runtime, operation, args);
  Monstey_Check(reply.at("ok").as_bool(), "Independent runtime call failed");
  return reply;
}
void Monstey_SameCounters(const J &a, const J &b) {
  Monstey_Check(a.at("generation") == b.at("generation") &&
                    a.at("revision") == b.at("revision"),
                "Read-only operation changed session counters");
}
std::map<std::string, std::string>
Monstey_Snapshot(const std::filesystem::path &root) {
  std::map<std::string, std::string> out;
  for (const auto &e : std::filesystem::recursive_directory_iterator(root))
    if (e.is_regular_file()) {
      const auto name =
          Monstey_Utf8(e.path().lexically_relative(root).wstring());
      out[name] =
          Monstey_HashSha256(e.path()) + ":" + std::to_string(e.file_size()) +
          ":" + std::to_string(e.last_write_time().time_since_epoch().count());
    }
  return out;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Check(argc == 3 || argc == 4,
                  "NEW_DIR INDEPENDENT_BASELINE [ACTUAL_HKAI]");
    const std::filesystem::path out = argv[1];
    Monstey_Check(!std::filesystem::exists(out),
                  "Fresh independent test directory required");
    std::filesystem::create_directories(out);
    const auto baseline = J::Parse(Monstey_ReadText(argv[2]));
    for (unsigned count : {0u, 1u, 3u, 17u})
      for (const auto w : {0u, 0x80000000u, 0x7fc12345u, 0xffc56789u}) {
        const auto source = Monstey_ReviewNativeNavFixture(count, 1, w);
        const auto result = Monstey_Page(source);
        Monstey_Check(result.at("mesh_type_version") == J(16),
                      "Native mesh version metadata differs");
      }
    ++Monstey_Groups;
    const auto big = Monstey_ReviewNativeNavFixture(257);
    std::uint64_t offset = 0;
    unsigned defaultPages = 0;
    do {
      const auto r = Monstey_Page(big, J::object({{"offset", offset}}));
      ++defaultPages;
      if (r.at("next_offset").is_null())
        break;
      offset = r.at("next_offset").as_uint();
    } while (defaultPages < 10);
    Monstey_Check(defaultPages == 3 && offset == 512,
                  "Default paging did not cover 513 nodes in three pages");
    const auto mid = Monstey_ReviewNativeNavFixture(17);
    for (unsigned i = 0; i < 33; ++i)
      (void)Monstey_Page(mid, J::object({{"offset", i}, {"limit", 1}}));
    (void)Monstey_Page(
        mid, J::object({{"offset", std::numeric_limits<std::int64_t>::max()},
                        {"limit", 512}}));
    (void)Monstey_Page(big, J::object({{"limit", 512}}));
    (void)Monstey_Page(big, J::object({{"offset", 512}, {"limit", 512}}));
    ++Monstey_Groups;
    const auto multi = Monstey_ReviewNativeNavFixture(3, 2);
    const auto meshIds = Monstey_Meshes(multi);
    Monstey_Check(meshIds.size() == 2, "Independent multi fixture differs");
    const auto a = Monstey_Page(multi, J::object({{"mesh_node", meshIds[0]}})),
               b = Monstey_Page(multi, J::object({{"mesh_node", meshIds[1]}}));
    Monstey_Check(a.at("tree_sha256") != b.at("tree_sha256") ||
                      a.at("root_bounds_f32_bits") !=
                          b.at("root_bounds_f32_bits"),
                  "Stable node selection did not select distinct geometry");
    (void)Monstey_Page(Monstey_ReviewNativeNavFixture(3, 1, 0, 5));
    const auto versioned =
        Monstey_Page(Monstey_ReviewNativeNavFixture(3, 1, 0, 8));
    Monstey_Check(versioned.at("mesh_type_version") == J(99),
                  "Structurally compatible mesh metadata lost");
    ++Monstey_Groups;
    const auto source = Monstey_ReviewNativeNavFixture(3);
    const std::vector<J> badValues{J(-1), J(.5),      J(true),    J("0"),
                                   J(),   J::array(), J::object()};
    for (const auto key : {"offset", "mesh_node"})
      for (const auto &value : badValues)
        Monstey_Reject([&] {
          (void)Monstey_NavStaticTreeFromBytes(source,
                                               J::object({{key, value}}));
        });
    const std::vector<J> badLimits{J(0),  J(513),     J(-1),
                                   J(.5), J(true),    J("1"),
                                   J(),   J::array(), J::object()};
    for (const auto &value : badLimits)
      Monstey_Reject([&] {
        (void)Monstey_NavStaticTreeFromBytes(source,
                                             J::object({{"limit", value}}));
      });
    for (const auto &value : std::vector<J>{J(), J::array(), J(true)})
      Monstey_Reject(
          [&] { (void)Monstey_NavStaticTreeFromBytes(source, value); });
    for (const auto key : {"unknown", "path", "expected_sha256"})
      Monstey_Reject([&] {
        (void)Monstey_NavStaticTreeFromBytes(source,
                                             J::object({{key, "value"}}));
      });
    for (const auto id : {std::size_t{0}, std::size_t{999999}, meshIds[0] + 1})
      Monstey_Reject([&] {
        (void)Monstey_NavStaticTreeFromBytes(multi,
                                             J::object({{"mesh_node", id}}));
      });
    Monstey_Reject([&] { (void)Monstey_NavStaticTreeFromBytes(multi); });
    ++Monstey_Groups;
    for (unsigned variant : {1u, 2u, 3u, 4u, 6u})
      for (unsigned count : {0u, 3u}) {
        const auto malformed =
            Monstey_ReviewNativeNavFixture(count, 1, 0, variant);
        Monstey_Reject(
            [&] { (void)Monstey_NavStaticTreeFromBytes(malformed); });
      }
    Monstey_Reject([&] {
      (void)Monstey_NavStaticTreeFromBytes(
          Monstey_ReviewNativeNavFixture(3, 1, 0, 9));
    });
    for (unsigned corrupt = 0; corrupt < 5; ++corrupt) {
      auto bytes = source;
      if (corrupt == 0)
        bytes[0] = 0;
      if (corrupt == 1)
        Monstey_PutU32(bytes, 4, 4);
      if (corrupt == 2)
        Monstey_PutU32(bytes, 8, 0);
      if (corrupt == 3)
        Monstey_PutU32(bytes, 12, 1);
      if (corrupt == 4)
        Monstey_PutU32(bytes, bytes.size() - 4, 0);
      Monstey_Reject([&] { (void)Monstey_NavStaticTreeFromBytes(bytes); });
    }
    Monstey_Check(Monstey_Negatives == 49, "Direct negative coverage skipped");
    ++Monstey_Groups;
    const auto path = out / L"review-\u00e9.hkai";
    Monstey_WriteNewFile(path, big);
    const auto sha = Monstey_HashSha256(big);
    J args = J::object({{"path", Monstey_Utf8(path.wstring())},
                        {"expected_sha256", sha},
                        {"limit", 512}});
    const auto file = Monstey_NavStaticTreeInspect(args);
    Monstey_Check(
        file.at("source_unchanged").as_bool() &&
            file.at("tree_sha256").as_string() ==
                Monstey_NavStaticTreeFromBytes(big, J::object({{"limit", 512}}))
                    .at("tree_sha256")
                    .as_string(),
        "Unicode file adapter identity differs");
    Monstey_Check(file.dump().find(Monstey_Utf8(path.wstring())) ==
                      std::string::npos,
                  "Source path leaked into result");
    for (unsigned bad = 0; bad < 11; ++bad) {
      auto changed = args;
      if (bad == 0)
        changed["expected_sha256"] = std::string(64, '0');
      if (bad == 1)
        changed["expected_sha256"] = std::string(63, 'a');
      if (bad == 2)
        changed["expected_sha256"] = std::string(64, 'A');
      if (bad == 3)
        changed["path"] = std::string("bad\0path", 8);
      if (bad == 4)
        changed["path"] = "";
      if (bad == 5)
        changed["expected_sha256"] = true;
      if (bad == 6)
        changed["path"] = true;
      if (bad == 7)
        changed["unknown"] = 1;
      if (bad == 8)
        changed.as_object().erase("path");
      if (bad == 9)
        changed.as_object().erase("expected_sha256");
      if (bad == 10)
        changed["path"] = Monstey_Utf8((out / L"missing.hkai").wstring());
      Monstey_Reject([&] { (void)Monstey_NavStaticTreeInspect(changed); });
    }
    Monstey_Check(Monstey_Negatives == 60, "File negative coverage skipped");
    ++Monstey_Groups;
    const auto catalog = Monstey_Runtime::Catalog(),
               world = Monstey_WorldCatalog();
    Monstey_Check(catalog.size() == 227 && world.size() == 41,
                  "Final catalog counts differ");
    auto oldCore = Monstey_HistoricalCatalogWithoutAtomUses(catalog), oldWorld = world;
    auto &coreRows = oldCore.as_array();
    const auto explicitClass = std::find_if(coreRows.begin(), coreRows.end(), [](const auto &x) {
      return x.at("name") == J("database_native_class");
    });
    Monstey_Check(explicitClass != coreRows.end(), "Missing additive native-class descriptor");
    coreRows.erase(explicitClass);
    const auto Remove = [&](J &rows) {
      auto &owned = rows.as_array();
      const auto it =
          std::find_if(owned.begin(), owned.end(), [](const auto &row) {
            return row.at("name").as_string() == Monstey_Operation;
          });
      Monstey_Check(it != owned.end(), "New operation missing from catalog");
      const auto descriptor = *it;
      owned.erase(it);
      return descriptor;
    };
    const auto descriptor = Remove(oldCore), worldDescriptor = Remove(oldWorld);
    Monstey_Check(oldCore == baseline.at("core") &&
                      oldWorld == baseline.at("world"),
                  "Old 224 or old 40 descriptors changed");
    Monstey_Check(descriptor.at("read_only").as_bool() &&
                      descriptor.at("annotations").at("readOnlyHint").as_bool(),
                  "New operation lacks read-only metadata");
    Monstey_Check(
        worldDescriptor.at("annotations").at("readOnlyHint").as_bool(),
        "World descriptor is not read-only");
    const auto &schema = descriptor.at("inputSchema");
    Monstey_Check(
        schema.at("properties").at("limit").at("minimum") == J(1) &&
            schema.at("properties").at("limit").at("maximum") == J(512) &&
            schema.at("properties").at("mesh_node").at("type") == J("integer"),
        "Typed form/schema limits differ");
    ++Monstey_Groups;
    Monstey_Runtime runtime({}, out / L"absent-settings.json");
    const auto before = Monstey_Good(runtime, "describe");
    const auto filesBefore = Monstey_Snapshot(out);
    const auto good = Monstey_Good(runtime, Monstey_Operation, args);
    Monstey_SameCounters(before, good);
    Monstey_Check(good.at("result") == file, "Runtime direct result differs");
    const auto state = Monstey_Good(runtime, "describe");
    Monstey_Check(state.at("result") == before.at("result"),
                  "Read-only call changed editor state");
    auto guarded = args;
    guarded["expected_generation"] = state.at("generation");
    guarded["expected_revision"] = state.at("revision");
    Monstey_SameCounters(state,
                         Monstey_Good(runtime, Monstey_Operation, guarded));
    ++Monstey_Groups;
    for (unsigned bad = 0; bad < 12; ++bad) {
      auto changed = args;
      if (bad == 0)
        changed["limit"] = 513;
      if (bad == 1)
        changed["limit"] = 0;
      if (bad == 2)
        changed["limit"] = .5;
      if (bad == 3)
        changed["offset"] = -1;
      if (bad == 4)
        changed["offset"] = true;
      if (bad == 5)
        changed["mesh_node"] = 999999;
      if (bad == 6)
        changed["expected_sha256"] = std::string(64, '0');
      if (bad == 7)
        changed["expected_generation"] = 99;
      if (bad == 8)
        changed["expected_revision"] = 99;
      if (bad == 9)
        changed["path"] = Monstey_Utf8((out / L"missing.hkai").wstring());
      if (bad == 10)
        changed["unknown"] = 1;
      if (bad == 11)
        changed["path"] = J();
      const auto reply = Monstey_Call(runtime, Monstey_Operation, changed);
      Monstey_Check(!reply.at("ok").as_bool(),
                    "Runtime rejection case accepted");
      Monstey_SameCounters(state, reply);
      ++Monstey_RuntimeNegatives;
    }
    Monstey_Check(Monstey_RuntimeNegatives == 12,
                  "Runtime negative coverage skipped");
    ++Monstey_Groups;
    (void)Monstey_Good(runtime, "database_new");
    const auto edited = Monstey_Good(runtime, "describe"),
               changes = Monstey_Good(runtime, "database_changes");
    Monstey_SameCounters(edited,
                         Monstey_Good(runtime, Monstey_Operation, args));
    Monstey_Check(Monstey_Good(runtime, "describe").at("result") ==
                          edited.at("result") &&
                      Monstey_Good(runtime, "database_changes").at("result") ==
                          changes.at("result"),
                  "Existing database session changed");
    ++Monstey_Groups;
    Monstey_Mcp mcp(runtime);
    (void)mcp.Handle(
        J::object({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}}));
    const auto listed = mcp.Handle(
        J::object({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}}));
    Monstey_Check(listed.has_value() &&
                      listed->at("result").at("tools").size() == 227,
                  "MCP catalog differs");
    const auto response =
        mcp.Handle(J::object({{"jsonrpc", "2.0"},
                              {"id", 3},
                              {"method", "tools/call"},
                              {"params", J::object({{"name", Monstey_Operation},
                                                    {"arguments", args}})}}));
    Monstey_Check(response.has_value() &&
                      !response->at("result").at("isError").as_bool(),
                  "MCP call failed");
    const auto native = J::Parse(
        response->at("result").at("content")[0u].at("text").as_string());
    Monstey_SameCounters(edited, native);
    Monstey_Check(native.at("result") == file, "MCP result differs");
    Monstey_MaxMcp = response->dump().size();
    Monstey_Check(Monstey_MaxMcp < 65536, "MCP page envelope unbounded");
    ++Monstey_Groups;
    Monstey_Runtime legacy({}, out / L"absent-legacy-settings.json");
    const auto legacyBefore = Monstey_Good(legacy, "describe");
    const auto wrapped = Monstey_Good(
        legacy, "world_call",
        J::object({{"name", Monstey_Operation}, {"arguments", args}}));
    Monstey_Check(wrapped.at("generation") == legacyBefore.at("generation") &&
                      wrapped.at("revision").as_uint() ==
                          legacyBefore.at("revision").as_uint() + 1,
                  "Legacy world_call revision contract changed");
    ++Monstey_Groups;
    J actual;
    if (argc == 4) {
      const std::filesystem::path actualPath = argv[3];
      const auto bytes = Monstey_ReadFile(actualPath);
      const auto hash = Monstey_HashSha256(bytes);
      Monstey_Check(hash == "b9fb6380798b09110a8db133ea6139e17db88ec5cad123fc6a"
                            "a1fe1b038008ee",
                    "Actual retained source differs");
      actual = Monstey_Page(bytes, J::object({{"limit", 512}}));
      Monstey_Check(actual.at("total_nodes") == J(303) &&
                        actual.at("tree_sha256") ==
                            J("6140792f4bdeca13ae3078ebb86fb26fb1935430a01bec46"
                              "4a0144d3940b3ad3"),
                    "Actual native tree differs");
      const auto actualReply =
          Monstey_Good(runtime, Monstey_Operation,
                       J::object({{"path", Monstey_Utf8(actualPath.wstring())},
                                  {"expected_sha256", hash},
                                  {"limit", 512}}));
      Monstey_SameCounters(edited, actualReply);
      Monstey_Check(actualReply.at("result").at("tree_sha256") ==
                            actual.at("tree_sha256") &&
                        Monstey_HashSha256(actualPath) == hash,
                    "Actual read-only API identity differs");
      ++Monstey_Groups;
    }
    Monstey_Check(Monstey_Pages == (argc == 4 ? 60u : 59u) &&
                      Monstey_Groups == (argc == 4 ? 13u : 12u) &&
                      Monstey_Negatives == 60 && Monstey_RuntimeNegatives == 12,
                  "Mandatory independent coverage differs");
    Monstey_Check(Monstey_Snapshot(out) == filesBefore &&
                      !std::filesystem::exists(out / L"absent-settings.json") &&
                      Monstey_HashSha256(path) == sha,
                  "Read-only filesystem/source invariant failed");
    Monstey_WriteNewText(
        out / L"review.json",
        J::object({{"status", "PASS"},
                   {"groups", Monstey_Groups},
                   {"page_cases", Monstey_Pages},
                   {"negative_cases", Monstey_Negatives},
                   {"runtime_negative_cases", Monstey_RuntimeNegatives},
                   {"checks", Monstey_Checks},
                   {"old_core_descriptors_exact", 224},
                   {"old_world_descriptors_exact", 40},
                   {"catalog_operations", 227},
                   {"world_operations", 41},
                   {"maximum_page_bytes", Monstey_MaxPage},
                   {"maximum_MCP_bytes", Monstey_MaxMcp},
                   {"source_and_session_unchanged", true},
                   {"raw_float_W_preserved", true},
                   {"actual", actual},
                   {"game_process_accessed", false},
                   {"original_engine_called", false}})
                .dump(2) +
            "\n");
    std::cout << "PASS " << Monstey_Groups
              << " independent API groups; pages=" << Monstey_Pages
              << " negatives=" << Monstey_Negatives
              << " runtime=" << Monstey_RuntimeNegatives
              << " checks=" << Monstey_Checks << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
