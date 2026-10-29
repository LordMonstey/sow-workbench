#include "Monstey_Navigation.hpp"
#include "Monstey_NavigationWorld.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
int passed = 0;
void Check(bool ok, std::string_view s) {
  Monstey_Require(ok, s);
  ++passed;
  std::cout << "PASS " << s << '\n';
}
void Reject(const std::function<void()> &fn, std::string_view s) {
  try {
    fn();
  } catch (const std::exception &) {
    Check(true, s);
    return;
  }
  throw std::runtime_error("Expected rejection " + std::string(s));
}
void Pack(B &b, std::uint32_t n) {
  if (n < 128)
    b.push_back(std::uint8_t(n));
  else if (n < 0x4000) {
    b.push_back(std::uint8_t(0x80 | (n >> 8)));
    b.push_back(std::uint8_t(n));
  } else if (n < 0x200000) {
    b.push_back(std::uint8_t(0xc0 | (n >> 16)));
    b.push_back(std::uint8_t(n >> 8));
    b.push_back(std::uint8_t(n));
  } else {
    b.push_back(std::uint8_t(0xe0 | (n >> 24)));
    b.push_back(std::uint8_t(n >> 16));
    b.push_back(std::uint8_t(n >> 8));
    b.push_back(std::uint8_t(n));
  }
}
B Section(std::string_view name, B p, bool flag = true) {
  while (p.size() % 4)
    p.push_back(0);
  const auto n =
      static_cast<std::uint32_t>(p.size() + 8) | (flag ? 0x40000000u : 0);
  B b{std::uint8_t(n >> 24), std::uint8_t(n >> 16), std::uint8_t(n >> 8),
      std::uint8_t(n)};
  b.insert(b.end(), name.begin(), name.end());
  b.insert(b.end(), p.begin(), p.end());
  return b;
}
void Add(B &b, const B &x) { b.insert(b.end(), x.begin(), x.end()); }
B Text(const std::vector<std::string> &strings) {
  B b;
  for (const auto &s : strings) {
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
  }
  return b;
}
B Envelope(const B &tag) {
  B b{'H', 'K', 'A', 'I'};
  Monstey_AppendU32(b, 3);
  Monstey_AppendU32(b, 0xffffd00d);
  Monstey_AppendU32(b, static_cast<std::uint32_t>(tag.size()));
  Add(b, tag);
  Monstey_AppendU32(b, 0xffffdead);
  return b;
}
struct Type {
  std::string name;
  std::uint32_t kind = 0, bytes = 0, align = 0, ptr = 0;
  std::vector<std::pair<std::string, std::uint32_t>> fields;
};
B Fixture() {
  std::vector<Type> t(37);
  auto scalar = [&](std::size_t i, std::string name, std::uint32_t kind,
                    std::uint32_t bytes, std::uint32_t align) {
    t[i] = {name, kind, bytes, align, 0, {}};
  };
  auto array = [&](std::size_t i, std::string name, std::uint32_t ptr) {
    t[i] = {name, 8, 16, 8, ptr, {}};
  };
  auto pointer = [&](std::size_t i, std::string name, std::uint32_t ptr) {
    t[i] = {name, 6, 8, 8, ptr, {}};
  };
  auto cls =
      [&](std::size_t i, std::string name,
          std::initializer_list<std::pair<std::string, std::uint32_t>> fields) {
        t[i] = {name, 7, 0, 8, 0, fields};
      };
  scalar(1, "float", 5, 4, 4);
  scalar(2, "uint32", 0x8004, 4, 4);
  scalar(3, "int32", 0x8204, 4, 4);
  scalar(4, "uint16", 0x4004, 2, 2);
  scalar(5, "char", 0x2004, 1, 1);
  t[6] = {"hkVector4", 0x428, 16, 16, 1, {}};
  cls(7, "hkAabb", {{"min", 6}, {"max", 6}});
  cls(8, "Face",
      {{"startEdgeIndex", 3},
       {"startUserEdgeIndex", 3},
       {"numEdges", 4},
       {"numUserEdges", 4},
       {"clusterIndex", 3},
       {"padding", 4}});
  cls(9, "Edge",
      {{"a", 2},
       {"b", 2},
       {"oppositeEdge", 2},
       {"oppositeFace", 2},
       {"flags", 5},
       {"paddingByte", 5},
       {"userEdgeCost", 1}});
  array(10, "faces", 8);
  array(11, "edges", 9);
  array(12, "vertices", 6);
  array(13, "intArray", 3);
  array(14, "byteArray", 5);
  array(15, "uintArray", 2);
  scalar(16, "hkStringPtr", 3, 8, 8);
  array(17, "stringArray", 16);
  cls(18, "CHkStringTable", {{"Data", 17}});
  cls(19, "hkaiNavMeshGenerationSettings", {{"weldThreshold", 1}});
  cls(20, "hkaiNavMesh",
      {{"faces", 10},
       {"edges", 11},
       {"vertices", 12},
       {"faceData", 13},
       {"edgeData", 13},
       {"faceDataStriding", 2},
       {"edgeDataStriding", 2},
       {"aabb", 7},
       {"streamingSets", 15},
       {"clearanceCacheSeedingDataSet", 36},
       {"erosionRadius", 1}});
  pointer(21, "meshPtr", 20);
  array(22, "meshArray", 21);
  cls(23, "DynamicPiece",
      {{"hkaiNavMesh", 21},
       {"vWorldPosition", 6},
       {"Points", 15},
       {"nStringIndexName", 2},
       {"uNameHash", 2}});
  pointer(24, "piecePtr", 23);
  array(25, "pieceArray", 24);
  cls(26, "CHavokAIDynamicNavMeshPieceArray", {{"DynamicNavMeshPieces", 25}});
  cls(27, "CHavokAIUserEdgeDataArray",
      {{"UserEdges", 15},
       {"ContextActionInfo", 15},
       {"aContextActionInfoOffsets", 15},
       {"aContextActionOwnerHashes", 15},
       {"aContextActionOwnerBeginIdsOffset", 15},
       {"aOwnedContextActionConnectionIds", 15}});
  cls(28, "Root",
      {{"settings", 19},
       {"strings", 18},
       {"pieces", 26},
       {"user", 27},
       {"meshes", 22}});
  cls(29, "Cache",
      {{"clearanceCeiling", 1},
       {"faceOffsets", 15},
       {"edgePairClearances", 14},
       {"mcpData", 31},
       {"vertexClearances", 14},
       {"unusedEdgePairElements", 2},
       {"uncalculatedFacesLowerBound", 2}});
  cls(30, "Mcp", {{"interpolant", 5}, {"clearance", 5}});
  array(31, "mcpArray", 30);
  pointer(32, "cachePtr", 29);
  cls(33, "CacheRow",
      {{"id", 2}, {"info", 2}, {"infoMask", 2}, {"initialCache", 32}});
  array(34, "cacheRows", 33);
  cls(35, "CacheDataset", {{"cacheDatas", 34}});
  pointer(36, "datasetPtr", 35);
  std::vector<std::map<std::string, std::uint32_t>> offsets(t.size());
  std::function<void(std::size_t)> layout = [&](std::size_t i) {
    if (t[i].bytes)
      return;
    std::uint32_t at = 0;
    for (const auto &[name, ti] : t[i].fields) {
      layout(ti);
      const auto align = t[ti].align;
      at = (at + align - 1) / align * align;
      offsets[i][name] = at;
      at += t[ti].bytes;
      t[i].align = std::max(t[i].align, align);
    }
    t[i].bytes = (at + t[i].align - 1) / t[i].align * t[i].align;
  };
  for (std::size_t i = 1; i < t.size(); ++i)
    layout(i);
  std::vector<std::string> names, fields;
  std::map<std::string, std::uint32_t> fieldIndex;
  for (std::size_t i = 1; i < t.size(); ++i) {
    names.push_back(t[i].name);
    for (const auto &[name, ti] : t[i].fields) {
      (void)ti;
      if (!fieldIndex.contains(name)) {
        fieldIndex[name] = static_cast<std::uint32_t>(fields.size());
        fields.push_back(name);
      }
    }
  }
  B tn, tb, th;
  Pack(tn, static_cast<std::uint32_t>(t.size()));
  Pack(th, static_cast<std::uint32_t>(t.size() - 1));
  for (std::uint32_t i = 1; i < t.size(); ++i) {
    Pack(tn, i - 1);
    Pack(tn, 0);
    const auto &x = t[i];
    Pack(tb, i);
    Pack(tb, 0);
    Pack(tb, 9 | (x.ptr ? 2 : 0) | (!x.fields.empty() ? 32 : 0));
    Pack(tb, x.kind);
    if (x.ptr)
      Pack(tb, x.ptr);
    Pack(tb, x.bytes);
    Pack(tb, x.align);
    if (!x.fields.empty()) {
      Pack(tb, static_cast<std::uint32_t>(x.fields.size()));
      for (const auto &[name, ti] : x.fields) {
        Pack(tb, fieldIndex[name]);
        Pack(tb, 0);
        Pack(tb, offsets[i][name]);
        Pack(tb, ti);
      }
    }
    Pack(th, i);
    Monstey_AppendU32(th, 0x55550000 + i);
  }
  B type;
  Add(type, Section("TPTR", {}));
  Add(type, Section("TSTR", Text(names)));
  Add(type, Section("TNA1", tn));
  Add(type, Section("FSTR", Text(fields)));
  Add(type, Section("TBOD", tb));
  Add(type, Section("THSH", th));
  Add(type, Section("TPAD", {}));
  B data(t[28].bytes);
  Monstey_PutU32(data, offsets[28]["settings"],
                 std::bit_cast<std::uint32_t>(0.01f));
  B item(12);
  Monstey_AppendU32(item, 0x10000000 | 28);
  Monstey_AppendU32(item, 0);
  Monstey_AppendU32(item, 1);
  B index;
  Add(index, Section("ITEM", item));
  Add(index, Section("PTCH", {}));
  B tag;
  Add(tag, Section("SDKV", B{'2', '0', '1', '6', '0', '1', '0', '0'}));
  Add(tag, Section("DATA", data));
  Add(tag, Section("TYPE", type, false));
  Add(tag, Section("INDX", index, false));
  Monstey_Havok g(Section("TAG0", tag, false));
  J mesh = J::object(
      {{"faces", J::array()},
       {"edges", J::array()},
       {"vertices", J::array()},
       {"faceData", J::array()},
       {"edgeData", J::array()},
       {"faceDataStriding", 1},
       {"edgeDataStriding", 1},
       {"aabb", J::object({{"min", J::array({-2, -2, -2, -2})},
                           {"max", J::array({2, 2, 2, 2})}})},
       {"streamingSets", J::array()},
       {"clearanceCacheSeedingDataSet",
        J::object(
            {{"cacheDatas",
              J::array({J::object(
                  {{"id", 1},
                   {"info", 2},
                   {"infoMask", 3},
                   {"initialCache",
                    J::object({{"clearanceCeiling", 10.0},
                               {"faceOffsets", J::array()},
                               {"edgePairClearances", J::array()},
                               {"mcpData", J::array()},
                               {"vertexClearances", J::array()},
                               {"unusedEdgePairElements", 0},
                               {"uncalculatedFacesLowerBound", 0}})}})})}})},
       {"erosionRadius", 0.0}});
  const auto id = g.New(20, mesh), ptr = g.New(21, J());
  g.Node(ptr).target = id;
  g.Node(g.Field(g.Root(), "meshes")).elements.push_back(ptr);
  return Envelope(g.Encode());
}
J Room() {
  J vertices = J::array(), triangles = J::array();
  const double xs[]{-400, -150, 150, 400}, zs[]{-500, -10, 10, 500};
  for (auto z : zs)
    for (auto x : xs)
      vertices.as_array().push_back(J::array({x, 0.0, z}));
  for (std::size_t zi = 0; zi < 3; ++zi)
    for (std::size_t xi = 0; xi < 3; ++xi) {
      if (zi == 1 && xi != 1)
        continue;
      const auto a = zi * 4 + xi, b = a + 4, c = b + 1, d = a + 1;
      triangles.as_array().push_back(J::array({a, b, c}));
      triangles.as_array().push_back(J::array({a, c, d}));
    }
  return J::object(
      {{"mode", "replace"},
       {"vertices_cm", vertices},
       {"triangles", triangles},
       {"actor", J::object({{"radius_cm", 130},
                            {"height_cm", 310},
                            {"max_slope_degrees", 40}})},
       {"headroom_cm", 380},
       {"routes", J::array({J::object(
                      {{"name", "Kathnox"},
                       {"points_cm", J::array({J::array({0, 0, -300}),
                                               J::array({0, 0, 300})})}})})}});
}
void Files(const B &source, const std::filesystem::path &out) {
  const auto src = out / L"source.hkai", target = out / L"file-room.hkai",
             receipt = out / L"file-room-report.json";
  Monstey_WriteNewFile(src, source);
  const auto path = [](const std::filesystem::path &p) {
    return Monstey_Utf8(p.wstring());
  };
  const auto inspect = Monstey_NavigationWorldCall(
      "sow_world_navigation_inspect", J::object({{"source", path(src)}}));
  Check(inspect.at("source_unchanged").as_bool(),
        "Native navigation file inspect preserves source");
  auto spec = Room();
  spec["source_sha256"] = Monstey_HashSha256(source);
  auto args = J::object({{"source", path(src)},
                         {"spec", spec},
                         {"output", path(target)},
                         {"report", path(receipt)}});
  const auto built =
      Monstey_NavigationWorldCall("sow_world_navigation_build", args);
  Check(built.at("output_sha256").as_string() == Monstey_HashSha256(target) &&
            std::filesystem::exists(receipt),
        "New-only file build and report readback");
  Reject(
      [&] { Monstey_NavigationWorldCall("sow_world_navigation_build", args); },
      "Existing native navigation outputs rejected");
  args["output"] = path(src);
  args["report"] = path(out / L"protected-report.json");
  Reject(
      [&] { Monstey_NavigationWorldCall("sow_world_navigation_build", args); },
      "Output cannot replace navigation input");
  args["output"] = path(out / L"duplicate.hkai");
  args["report"] = args.at("output");
  Reject(
      [&] { Monstey_NavigationWorldCall("sow_world_navigation_build", args); },
      "Duplicate navigation/report outputs rejected");
  Monstey_WriteNewText(out / L"blocked-parent", "preexisting file");
  args["output"] = path(out / L"rolled-back.hkai");
  args["report"] = path(out / L"blocked-parent" / L"report.json");
  Reject(
      [&] { Monstey_NavigationWorldCall("sow_world_navigation_build", args); },
      "Later report publication failure is propagated");
  Check(!std::filesystem::exists(out / L"rolled-back.hkai") &&
            Monstey_HashSha256(src) == Monstey_HashSha256(source),
        "Earlier navigation output rolled back and source unchanged");
}
void Synthetic(const std::filesystem::path &out) {
  const auto source = Fixture();
  Files(source, out);
  const auto inspect = Monstey_NavigationInspect(source);
  Check(inspect.at("no_op_byte_exact").as_bool(),
        "Synthetic HKAI3 no-op exact");
  auto spec = Room();
  const auto built = Monstey_NavigationBuild(source, spec);
  Check(built.report.at("counts").at("new_faces").as_uint() == 14,
        "New walkable room has fourteen triangles");
  Check(built.report.at("connectivity").at("component_count").as_uint() == 1,
        "Room native adjacency is connected");
  Check(built.report.at("routes").at(0).at("pass_").as_bool() &&
            built.report.at("routes")
                    .at(0)
                    .at("min_boundary_distance_cm")
                    .as_number() == 150,
        "Kathnox geometric route has 150cm clearance");
  Check(built.report.at("clearance").at("mode").as_string() ==
            "native_lazy_recompute",
        "Exact native lazy-clearance sentinels authored");
  Check(Monstey_NavigationInspect(built.navigation)
                .at("structure")
                .at("meshes")
                .at(0)
                .at("vertices")
                .as_uint() == 16,
        "Authored graph rereads with typed metadata");
  Monstey_WriteNewFile(out / L"synthetic-room.hkai", built.navigation);
  auto bad = spec;
  bad["headroom_cm"] = 200;
  Reject([&] { Monstey_NavigationBuild(source, bad); },
         "Insufficient headroom rejected");
  bad = spec;
  bad["triangles"] = J::array({J::array({0, 0, 1})});
  Reject([&] { Monstey_NavigationBuild(source, bad); },
         "Degenerate triangle rejected");
  bad = spec;
  bad["triangles"] = J::array({J::array({0, 5, 4})});
  Reject([&] { Monstey_NavigationBuild(source, bad); },
         "Wrong winding rejected");
  bad = spec;
  bad["triangles"].as_array().push_back(spec.at("triangles").at(0));
  Reject([&] { Monstey_NavigationBuild(source, bad); },
         "Duplicate face rejected");
  bad = spec;
  bad["actor"]["radius_cm"] = 151;
  Reject([&] { Monstey_NavigationBuild(source, bad); },
         "Oversized actor route rejected");
  bad = spec;
  bad["vertices_cm"].as_array().push_back(J::array({-400, 0, -250}));
  Reject([&] { Monstey_NavigationBuild(source, bad); },
         "Nonconforming T junction rejected");
  bad = spec;
  bad["vertices_cm"].as_array().push_back(J::array({-350, 0, -450}));
  bad["vertices_cm"].as_array().push_back(J::array({-350, 0, -200}));
  bad["vertices_cm"].as_array().push_back(J::array({-200, 0, -200}));
  bad["triangles"].as_array().push_back(J::array({16, 17, 18}));
  Reject([&] { Monstey_NavigationBuild(source, bad); },
         "Overlapping nonshared floor rejected");
  bad = spec;
  bad["source_sha256"] = std::string(64, '0');
  Reject([&] { Monstey_NavigationBuild(source, bad); },
         "Wrong source hash rejected");
  bad = spec;
  bad["weld_cm"] = 100;
  Reject([&] { Monstey_NavigationBuild(source, bad); },
         "Excessive weld rejected");
  Reject([&] { Monstey_NavigationInspect(B(28)); },
         "Malformed HKAI3 envelope bounded");
  Check(source == Fixture(), "Original synthetic source preserved");
}
void Native(const std::filesystem::path &folder,
            const std::filesystem::path &out) {
  const auto empty = Monstey_ReadFile(folder / L"keepinterior.hkai"),
             donor = Monstey_ReadFile(folder / L"keeplarge.hkai");
  const auto built = Monstey_NavigationBuild(empty, Room(), {}, donor);
  Check(built.report.at("counts").at("new_faces").as_uint() == 14 &&
            built.report.at("routes").at(0).at("pass_").as_bool(),
        "Real native empty HKAI3 authored Kathnox room");
  Check(built.report.at("item_ptch_audit").at("status").as_string() == "PASS",
        "Real authored ITEM/PTCH readback qualified");
  Monstey_WriteNewFile(out / L"native-room.hkai", built.navigation);
  Monstey_WriteNewText(out / L"native-room-report.json", built.report.dump(2));
  Reject([&] { Monstey_NavigationBuild(empty, Room()); },
         "Native empty template requires actual category donor");
  const auto inspected = Monstey_NavigationInspect(donor);
  Check(inspected.at("structure").at("meshes").size() == 16,
        "Real keeplarge sixteen native meshes inspect");
  Check(Monstey_HashSha256(folder / L"keepinterior.hkai") ==
                Monstey_HashSha256(empty) &&
            Monstey_HashSha256(folder / L"keeplarge.hkai") ==
                Monstey_HashSha256(donor),
        "Both native navigation sources unchanged");
}
void NativeConnected(const std::filesystem::path &folder,
                     const std::filesystem::path &specPath,
                     const std::filesystem::path &ownerPath,
                     const std::filesystem::path &out) {
  const auto source = Monstey_ReadFile(folder / L"keeplarge.hkai");
  const auto spec = J::Parse(Monstey_ReadText(specPath));
  const auto built = Monstey_NavigationBuild(source, spec);
  Check(
      built.report.at("counts").at("new_faces").as_uint() == 24 &&
          built.report.at("seams").size() == 1,
      "Native piece extended by sixteen authored triangles and one exact seam");
  Check(built.report.at("connectivity").at("component_count").as_uint() == 1 &&
            built.report.at("unrelated_meshes_preserved").as_uint() == 15,
        "Connected native graph preserves fifteen unrelated meshes");
  Check(built.report.at("routes").at(0).at("pass_").as_bool(),
        "Native attached room route is traversable geometrically");
  Monstey_WriteNewFile(out / L"native-connected.hkai", built.navigation);
  Monstey_WriteNewText(out / L"native-connected-report.json",
                       built.report.dump(2));
  const auto owners = Monstey_ReadFile(ownerPath);
  const auto inspect = Monstey_NavigationInspect(source, owners);
  Check(inspect.at("structure").at("owner_context_verified").as_bool(),
        "Real OBJRES dynamic navigation ownership verified");
  const auto translated = Monstey_NavigationApply(
      source, owners,
      J::object(
          {{"translate_pieces",
            J::array({J::object(
                {{"index", 0}, {"delta_cm", J::array({100, 0, -50})}})})}}));
  Check(translated.report.at("edits").size() == 1,
        "Native piece and OBJRES translated together");
  Monstey_WriteNewFile(out / L"native-translated.hkai", translated.navigation);
  Monstey_WriteNewFile(out / L"native-translated.objres", translated.owner);
  const auto cloned = Monstey_NavigationApply(
      source, owners,
      J::object(
          {{"clone_pieces",
            J::array({J::object({{"index", 0},
                                 {"new_name", "Monstey_Test_NavClone"},
                                 {"delta_cm", J::array({300, 0, 0})}})})}}));
  Check(cloned.report.at("structure").at("pieces").as_uint() ==
            inspect.at("structure").at("pieces").as_uint() + 1,
        "Native piece and matching owner clone authored");
  Monstey_WriteNewFile(out / L"native-cloned.hkai", cloned.navigation);
  Monstey_WriteNewFile(out / L"native-cloned.objres", cloned.owner);
  Monstey_WriteNewText(out / L"native-cloned-report.json",
                       cloned.report.dump(2));
  Check(Monstey_HashSha256(ownerPath) == Monstey_HashSha256(owners),
        "Native owner source unchanged");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    const auto out =
        (argc > 1 ? std::filesystem::path(argv[1])
                  : std::filesystem::path(L"navigation-tests")) /
        std::to_wstring(
            std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(out);
    Synthetic(out);
    if (argc > 2)
      Native(argv[2], out);
    if (argc > 4)
      NativeConnected(argv[2], argv[3], argv[4], out);
    std::cout << "PASS " << passed << " native navigation groups\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
