#pragma once
#include "Monstey_Havok.hpp"
#include <map>
#include <bit>
#include <algorithm>
namespace Monstey_NavFixture {
using namespace Monstey;using B=Monstey_Bytes;using J=Monstey_Json;
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
B Fixture(bool missing_active_hash = false) {
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
  auto encoded=g.Encode();
  if(missing_active_hash) {
    const auto section=Section("THSH",th);
    const auto it=std::search(encoded.begin(),encoded.end(),section.begin(),section.end());
    Monstey_Require(it!=encoded.end(),"Synthetic THSH section not found");
    const auto at=static_cast<std::size_t>(it-encoded.begin())+9+5*27;
    Monstey_Require(encoded.at(at)==28 && Monstey_U32(encoded,at+1)==0x5555001cu,
        "Synthetic active THSH identity differs");
    Monstey_PutU32(encoded,at+1,0);
  }
  return Envelope(encoded);
}
}
