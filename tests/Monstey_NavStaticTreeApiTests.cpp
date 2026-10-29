#include "Monstey_SowCatalogProjection.hpp"
#include "Monstey_NavCompactCooker.hpp"
#include "Monstey_NavStaticTreeApi.hpp"
#include "Monstey_NavTreeFixture.hpp"
#include "Monstey_Protocol.hpp"
#include "Monstey_Runtime.hpp"
#include "Monstey_World.hpp"
#include <algorithm>
#include <bit>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
unsigned groups = 0, negatives = 0, pages = 0;
std::size_t maxResult = 0, maxEnvelope = 0;
void Need(bool b, const char *s) { Monstey_Require(b, s); }
void Bad(const std::function<void()> &f) {
  bool bad = false;
  try {
    f();
  } catch (const std::exception &) {
    bad = true;
  }
  Need(bad, "Expected rejection");
  ++negatives;
}
Monstey_Bytes Records(const Monstey_Havok &g, std::size_t id,
                      std::string_view field) {
  Monstey_Bytes b;
  for (const auto n : g.Node(g.Field(id, field)).elements) {
    const auto &x = g.Node(n).original_storage;
    b.insert(b.end(), x.begin(), x.end());
  }
  return b;
}
std::vector<std::size_t> Meshes(const Monstey_Bytes &bytes) {
  Monstey_Havok g(std::span(bytes).subspan(16, bytes.size() - 20));
  return g.Objects("hkaiNavMesh");
}
std::string Hex(std::span<const std::uint8_t> b) {
  constexpr char digits[] = "0123456789abcdef";
  std::string s;
  for (const auto v : b) {
    s += digits[v >> 4];
    s += digits[v & 15];
  }
  return s;
}
void Page(const Monstey_Bytes &source, const J &args = J::object()) {
  const auto saved = source;
  const auto result = Monstey_NavStaticTreeFromBytes(source, args);
  Monstey_Havok g(std::span(source).subspan(16, source.size() - 20));
  const auto mesh = static_cast<std::size_t>(result.at("mesh_node").as_uint());
  const auto f = Records(g, mesh, "faces"), e = Records(g, mesh, "edges"),
             v = Records(g, mesh, "vertices");
  const auto tree = Monstey_NavCookCompactMesh(f, e, v);
  Need(source == saved &&
           result.at("source_sha256").as_string() == Monstey_HashSha256(source),
       "Owned source changed");
  Need(result.at("tree_sha256").as_string() == Monstey_HashSha256(tree.nodes) &&
           result.at("total_nodes").as_uint() == tree.nodes.size() / 6,
       "Adapter did not use accepted cooker");
  const auto begin = std::min<std::uint64_t>(result.at("offset").as_uint(),
                                             tree.nodes.size() / 6),
             limit = result.at("limit").as_uint(),
             count =
                 std::min<std::uint64_t>(limit, tree.nodes.size() / 6 - begin);
  Need(result.at("nodes_hex").size() == count, "Page count");
  for (std::size_t i = 0; i < count; ++i)
    Need(result.at("nodes_hex")[i].as_string() ==
             Hex(std::span(tree.nodes)
                     .subspan((static_cast<std::size_t>(begin) + i) * 6, 6)),
         "Page order/raw bytes");
  for (unsigned i = 0; i < 4; ++i) {
    Need(result.at("root_bounds_f32_bits").at("minimum")[i].as_uint() ==
             std::bit_cast<std::uint32_t>(tree.bounds.minimum[i]),
         "Minimum bits");
    Need(result.at("root_bounds_f32_bits").at("maximum")[i].as_uint() ==
             std::bit_cast<std::uint32_t>(tree.bounds.maximum[i]),
         "Maximum bits");
  }
  Need(result.at("read_only").as_bool() &&
           !result.at("runtime_verified").as_bool() &&
           !result.at("native_scratch_W_provenance_qualified").as_bool(),
       "Qualification scope");
  Need(J::Parse(result.dump()) == result, "Result is not valid JSON");
  Need(result.dump().size() < 30u * 1024, "Result bound");
  maxResult = std::max(maxResult, result.dump().size());
  ++pages;
}
J Call(Monstey_Runtime &r, const char *op, const J &a = J::object()) {
  return r.Call(J::object({{"id", 7}, {"operation", op}, {"arguments", a}}));
}
J Good(Monstey_Runtime &r, const char *op, const J &a = J::object()) {
  const auto x = Call(r, op, a);
  Need(x.at("ok").as_bool(), "Runtime call failed");
  return x;
}
void Same(const J &a, const J &b) {
  for (const auto key : {"generation", "revision"})
    Need(a.at(key) == b.at(key), "Read operation changed session counters");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Need(argc == 3 || argc == 4, "NEW_DIR BASELINE224 [ACTUAL_HKAI]");
    const std::filesystem::path out = argv[1];
    Need(!std::filesystem::exists(out), "Fresh test output required");
    std::filesystem::create_directories(out);
    const auto empty = Monstey_NavTreeFixture::Make(0),
               one = Monstey_NavTreeFixture::Make(1),
               large = Monstey_NavTreeFixture::Make(300),
               opaque = Monstey_NavTreeFixture::Make(3, 1, true),
               multiple = Monstey_NavTreeFixture::Make(1, 2);
    Page(empty);
    const auto er = Monstey_NavStaticTreeFromBytes(empty);
    Need(er.at("total_nodes").as_uint() == 0 &&
             er.at("root_bounds_f32_bits").at("minimum")[0].as_uint() ==
                 0x7f7fffeeu &&
             er.at("root_bounds_f32_bits").at("maximum")[0].as_uint() ==
                 0xff7fffeeu,
         "Exact empty tree bounds");
    ++groups;
    Page(one);
    Page(large, J::object({{"limit", 512}}));
    Page(large, J::object({{"offset", 512}, {"limit", 512}}));
    Page(large, J::object({{"offset", 599}}));
    Page(large, J::object({{"offset", INT64_MAX}}));
    Page(opaque);
    ++groups;
    const auto ids = Meshes(multiple);
    Need(ids.size() == 2, "Multi fixture");
    Bad([&] { (void)Monstey_NavStaticTreeFromBytes(multiple); });
    for (const auto id : ids)
      Page(multiple, J::object({{"mesh_node", id}}));
    Bad([&] {
      (void)Monstey_NavStaticTreeFromBytes(multiple,
                                           J::object({{"mesh_node", 999999}}));
    });
    ++groups;
    const auto badArgs = J::array(
        {J::object({{"limit", 0}}), J::object({{"limit", 513}}),
         J::object({{"offset", -1}}), J::object({{"offset", true}}),
         J::object({{"mesh_node", -1}}), J::object({{"mesh_node", "1"}}),
         J::object({{"output", "ignored"}}), J::object({{"limit", 1.5}})});
    unsigned rejected = 0;
    for (const auto &a : badArgs.as_array()) {
      Bad([&] { (void)Monstey_NavStaticTreeFromBytes(one, a); });
      ++rejected;
    }
    Need(rejected == 8, "Negative cases skipped");
    ++groups;
    {
      auto broken = one;
      Monstey_PutU32(broken, 8, 0);
      Bad([&] { (void)Monstey_NavStaticTreeFromBytes(broken); });
      broken = one;
      broken.pop_back();
      Bad([&] { (void)Monstey_NavStaticTreeFromBytes(broken); });
    }
    ++groups;
    Bad([&] {
      (void)Monstey_NavStaticTreeFromBytes(
          Monstey_NavTreeFixture::Make(1, 1, false, true));
    });
    for (unsigned variant = 0; variant < 4; ++variant) {
      Monstey_Havok graph(std::span(one).subspan(16, one.size() - 20));
      const auto mesh = graph.Objects("hkaiNavMesh").at(0);
      auto value = graph.Plain(mesh);
      value["edges"] = J::array();
      value["vertices"] = J::array();
      value["faces"][0u]["startEdgeIndex"] =
          variant == 3 ? INT32_MIN : INT32_MAX;
      value["faces"][0u]["numEdges"] = variant == 0 ? 0 : variant == 2 ? 1 : -1;
      graph.Replace(mesh, value);
      const auto source = Monstey_NavFixture::Envelope(graph.Encode());
      if (variant == 3)
        Bad([&] { (void)Monstey_NavStaticTreeFromBytes(source); });
      else {
        Page(source);
        Need(Monstey_NavStaticTreeFromBytes(source)
                     .at("total_nodes")
                     .as_uint() == 1,
             "Native unused face lost its payload node");
      }
    }
    ++groups;
    const auto path = out / L"synthetic.hkai";
    Monstey_WriteNewFile(path, large);
    const auto sha = Monstey_HashSha256(large);
    const auto args = J::object({{"path", Monstey_Utf8(path.wstring())},
                                 {"expected_sha256", sha},
                                 {"limit", 512}});
    const auto file = Monstey_NavStaticTreeInspect(args);
    Need(file.at("source_unchanged").as_bool() &&
             Monstey_HashSha256(path) == sha,
         "File was changed");
    auto bad = args;
    bad["expected_sha256"] = std::string(64, '0');
    Bad([&] { (void)Monstey_NavStaticTreeInspect(bad); });
    bad = args;
    bad["path"] = std::string("bad\0path", 8);
    Bad([&] { (void)Monstey_NavStaticTreeInspect(bad); });
    bad = args;
    bad["limit"] = 513;
    bad["path"] = "absent.hkai";
    Bad([&] { (void)Monstey_NavStaticTreeInspect(bad); });
    ++groups;
    const auto catalog = Monstey_Runtime::Catalog();
    auto old = Monstey_HistoricalCatalogWithoutAtomUses(catalog);
    auto &array = old.as_array();
    const auto explicitClass = std::find_if(array.begin(), array.end(), [](const auto &x) {
      return x.at("name") == J("database_native_class");
    });
    Need(explicitClass != array.end(), "Missing additive native-class descriptor");
    array.erase(explicitClass);
    const auto op = std::find_if(array.begin(), array.end(), [](const auto &x) {
      return x.at("name") == J("sow_world_navigation_static_tree");
    });
    Need(catalog.size() == 227 && op != array.end(),
         "Catalog227 operation absent");
    const auto descriptor = *op;
    array.erase(op);
    Need(old == J::Parse(Monstey_ReadText(argv[2])),
         "Old224 descriptors changed");
    Need(descriptor.at("read_only").as_bool() &&
             descriptor.at("inputSchema")
                     .at("properties")
                     .at("limit")
                     .at("maximum")
                     .as_uint() == 512 &&
             descriptor.at("inputSchema")
                     .at("properties")
                     .at("mesh_node")
                     .at("type") == J("integer"),
         "New schema");
    ++groups;
    auto world = Monstey_WorldCatalog();
    auto &worldRows = world.as_array();
    const auto addedWorld =
        std::find_if(worldRows.begin(), worldRows.end(), [](const auto &row) {
          return row.at("name") == J("sow_world_navigation_static_tree");
        });
    Need(worldRows.size() == 41 && addedWorld != worldRows.end() &&
             addedWorld->at("available").as_bool() &&
             addedWorld->at("annotations").at("readOnlyHint").as_bool(),
         "World form cannot discover new read-only command");
    worldRows.erase(addedWorld);
    Need(world == J::Parse(Monstey_ReadText(
                      std::filesystem::path(argv[2]).parent_path() /
                      L"world40.json")),
         "Old40 World form descriptors changed");
    const auto capabilities =
        Monstey_WorldCall("sow_capabilities", J::object());
    Need(capabilities.at("tools").as_uint() == 41 &&
             capabilities.at("complete_operations").as_uint() == 39 &&
             capabilities.at("partial_operations").as_uint() == 2 &&
             capabilities.at("pending_operations").as_uint() == 0,
         "World capability counts");
    Monstey_Runtime runtime({}, out / L"absent-settings.json");
    const auto before = Good(runtime, "describe");
    const auto reply = Good(runtime, "sow_world_navigation_static_tree", args);
    Same(before, reply);
    Need(reply.at("result") == file, "Runtime result differs");
    unsigned runtimeBad = 0;
    for (unsigned test = 0; test < 6; ++test) {
      auto a = args;
      if (test == 0)
        a["limit"] = 513;
      if (test == 1)
        a["mesh_node"] = true;
      if (test == 2)
        a["output"] = "forbidden";
      if (test == 3)
        a["expected_sha256"] = std::string(64, '0');
      if (test == 4)
        a["expected_generation"] = 99;
      if (test == 5)
        a["expected_revision"] = 99;
      const auto r = Call(runtime, "sow_world_navigation_static_tree", a);
      Need(!r.at("ok").as_bool(), "Runtime negative accepted");
      Same(before, r);
      ++runtimeBad;
    }
    Need(runtimeBad == 6, "Runtime rejection cases skipped");
    ++groups;
    const auto wrapperBefore = Good(runtime, "describe");
    const auto wrapped =
        Good(runtime, "world_call",
             J::object({{"name", "sow_world_navigation_static_tree"},
                        {"arguments", args}}));
    Need(wrapped.at("generation") == wrapperBefore.at("generation") &&
             wrapped.at("revision").as_uint() ==
                 wrapperBefore.at("revision").as_uint() + 1,
         "Legacy world_call wrapper behavior changed");
    (void)Good(runtime, "database_new");
    const auto edited = Good(runtime, "describe"),
               changes = Good(runtime, "database_changes");
    Same(edited, Good(runtime, "sow_world_navigation_static_tree", args));
    Need(Good(runtime, "describe").at("result") == edited.at("result") &&
             Good(runtime, "database_changes").at("result") ==
                 changes.at("result"),
         "Existing database state changed");
    ++groups;
    Monstey_Mcp mcp(runtime);
    (void)mcp.Handle(
        J::object({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}}));
    const auto listed = mcp.Handle(
        J::object({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}}));
    Need(listed.has_value() && listed->at("result").at("tools").size() == 227,
         "MCP catalog");
    const auto response = mcp.Handle(J::object(
        {{"jsonrpc", "2.0"},
         {"id", 3},
         {"method", "tools/call"},
         {"params", J::object({{"name", "sow_world_navigation_static_tree"},
                               {"arguments", args}})}}));
    Need(response.has_value() &&
             !response->at("result").at("isError").as_bool(),
         "MCP call failed");
    const auto native = J::Parse(
        response->at("result").at("content")[0u].at("text").as_string());
    Same(edited, native);
    Need(native.at("result") == file && response->dump().size() < 65536,
         "MCP result/envelope");
    maxEnvelope = response->dump().size();
    ++groups;
    J actual;
    if (argc == 4) {
      const std::filesystem::path source = argv[3];
      const auto bytes = Monstey_ReadFile(source);
      Need(Monstey_HashSha256(bytes) == "b9fb6380798b09110a8db133ea6139e17db88e"
                                        "c5cad123fc6aa1fe1b038008ee",
           "Actual source pin");
      Page(bytes, J::object({{"limit", 512}}));
      const auto a = J::object({{"path", Monstey_Utf8(source.wstring())},
                                {"expected_sha256", Monstey_HashSha256(bytes)},
                                {"limit", 512}});
      actual =
          Good(runtime, "sow_world_navigation_static_tree", a).at("result");
      Need(actual.at("tree_sha256").as_string() ==
                   "6140792f4bdeca13ae3078ebb86fb26fb1935430a01bec464a0144d3940"
                   "b3ad3" &&
               actual.at("total_nodes").as_uint() == 303 &&
               actual.at("faces").as_uint() == 152 &&
               actual.at("edges").as_uint() == 456 &&
               actual.at("vertices").as_uint() == 142,
           "Actual cooker output differs");
      Need(Monstey_HashSha256(source) == Monstey_HashSha256(bytes),
           "Actual source changed");
      ++groups;
    }
    Need(!std::filesystem::exists(out / L"absent-settings.json") &&
             Monstey_HashSha256(path) == sha,
         "Read-only source/settings invariant");
    Need(groups == (argc == 4 ? 12u : 11u) &&
             pages == (argc == 4 ? 13u : 12u) && negatives == 17,
         "Mandatory coverage differs");
    Monstey_WriteNewText(
        out / L"report.json",
        J::object({{"schema", "Monstey.NavStaticTreeApiTests/1"},
                   {"status", "PASS"},
                   {"groups", groups},
                   {"page_cases", pages},
                   {"negative_cases", negatives},
                   {"runtime_negative_cases", runtimeBad},
                   {"catalog_operations", 227},
                   {"old_descriptors_exact", 224},
                   {"maximum_result_bytes", maxResult},
                   {"maximum_MCP_envelope_bytes", maxEnvelope},
                   {"actual", actual},
                   {"game_process_accessed", false},
                   {"original_code_executed", false},
                   {"read_only_verified", true}})
                .dump(2) +
            "\n");
    std::cout << "PASS " << groups << " static-tree API groups; pages=" << pages
              << " result=" << maxResult << " envelope=" << maxEnvelope << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
