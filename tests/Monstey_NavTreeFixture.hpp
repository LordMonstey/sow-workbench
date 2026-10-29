#pragma once
#include "Monstey_NavFixture.hpp"
#include <tuple>
namespace Monstey_NavTreeFixture {
using namespace Monstey;
using J = Monstey_Json;
using B = Monstey_Bytes;
using namespace Monstey_NavFixture;
struct Monstey_Type {
  std::string name;
  std::uint32_t kind, bytes, alignment, pointer, version;
  std::vector<std::tuple<std::string, std::uint32_t, std::uint32_t>> fields;
};
B Make(unsigned faces = 1, unsigned meshes = 1, bool opaque_w = false,
       bool wrong_width = false) {
  std::vector<Monstey_Type> t(17);
  t[1] = {"float", 5, 4, 4, 0, 0, {}};
  t[2] = {"int", wrong_width ? 0x4204u : 0x8204u, 4, 4, 0, 0, {}};
  t[3] = {"hkInt16", 0x4204, 2, 2, 0, 0, {}};
  t[4] = {"hkUint32", 0x8004, 4, 4, 0, 0, {}};
  t[5] = {"hkUint8", 0x2004, 1, 1, 0, 0, {}};
  t[6] = {"hkHalf16", 5, 2, 2, 0, 0, {}};
  t[7] = {"hkVector4", 0x428, 16, 16, 1, 0, {}};
  t[8] = {"hkaiNavMesh::Face",
          7,
          16,
          4,
          0,
          6,
          {{"startEdgeIndex", 0, 2},
           {"startUserEdgeIndex", 4, 2},
           {"numEdges", 8, 3},
           {"numUserEdges", 10, 3},
           {"clusterIndex", 12, 3},
           {"padding", 14, 3}}};
  t[9] = {"hkaiNavMesh::Edge",
          7,
          20,
          4,
          0,
          8,
          {{"a", 0, 2},
           {"b", 4, 2},
           {"oppositeEdge", 8, 4},
           {"oppositeFace", 12, 4},
           {"flags", 16, 5},
           {"paddingByte", 17, 5},
           {"userEdgeCost", 18, 6}}};
  t[10] = {"FaceArray", 8, 16, 8, 8, 0, {}};
  t[11] = {"EdgeArray", 8, 16, 8, 9, 0, {}};
  t[12] = {"VertexArray", 8, 16, 8, 7, 0, {}};
  t[13] = {"hkaiNavMesh",
           7,
           208,
           16,
           0,
           16,
           {{"faces", 24, 10}, {"edges", 40, 11}, {"vertices", 56, 12}}};
  t[14] = {"MeshPointer", 6, 8, 8, 13, 0, {}};
  t[15] = {"MeshArray", 8, 16, 8, 14, 0, {}};
  t[16] = {"SyntheticRoot", 7, 16, 8, 0, 0, {{"meshes", 0, 15}}};
  std::vector<std::string> names, fields;
  std::map<std::string, std::uint32_t> ids;
  for (std::size_t i = 1; i < t.size(); ++i) {
    names.push_back(t[i].name);
    for (const auto &[name, offset, type] : t[i].fields) {
      (void)offset;
      (void)type;
      if (!ids.contains(name)) {
        ids[name] = static_cast<std::uint32_t>(fields.size());
        fields.push_back(name);
      }
    }
  }
  B tn, tb, th;
  Pack(tn, static_cast<std::uint32_t>(t.size()));
  Pack(th, static_cast<std::uint32_t>(t.size() - 1));
  for (std::uint32_t i = 1; i < t.size(); ++i) {
    const auto &x = t[i];
    Pack(tn, i - 1);
    Pack(tn, 0);
    Pack(tb, i);
    Pack(tb, 0);
    Pack(tb, 9 | (x.pointer ? 2u : 0u) | (x.version ? 4u : 0u) |
                 (x.fields.empty() ? 0u : 32u));
    Pack(tb, x.kind);
    if (x.pointer)
      Pack(tb, x.pointer);
    if (x.version)
      Pack(tb, x.version);
    Pack(tb, x.bytes);
    Pack(tb, x.alignment);
    if (!x.fields.empty()) {
      Pack(tb, static_cast<std::uint32_t>(x.fields.size()));
      for (const auto &[name, offset, type] : x.fields) {
        Pack(tb, ids.at(name));
        Pack(tb, 0);
        Pack(tb, offset);
        Pack(tb, type);
      }
    }
    Pack(th, i);
    Monstey_AppendU32(th, 0x5a000000u + i);
  }
  B types;
  Add(types, Section("TPTR", {}));
  Add(types, Section("TSTR", Text(names)));
  Add(types, Section("TNA1", tn));
  Add(types, Section("FSTR", Text(fields)));
  Add(types, Section("TBOD", tb));
  Add(types, Section("THSH", th));
  Add(types, Section("TPAD", {}));
  B item(12);
  Monstey_AppendU32(item, 0x10000010);
  Monstey_AppendU32(item, 0);
  Monstey_AppendU32(item, 1);
  B index;
  Add(index, Section("ITEM", item));
  Add(index, Section("PTCH", {}));
  B tag;
  Add(tag, Section("SDKV", B{'2', '0', '1', '6', '0', '1', '0', '0'}));
  Add(tag, Section("DATA", B(16)));
  Add(tag, Section("TYPE", types, false));
  Add(tag, Section("INDX", index, false));
  Monstey_Havok g(Section("TAG0", tag, false));
  for (unsigned mi = 0; mi < meshes; ++mi) {
    J fs = J::array(), es = J::array(), vs = J::array();
    for (unsigned fi = 0; fi < faces; ++fi) {
      fs.as_array().push_back(J::object({{"startEdgeIndex", fi * 3},
                                         {"startUserEdgeIndex", -1},
                                         {"numEdges", 3},
                                         {"numUserEdges", 0},
                                         {"clusterIndex", -1},
                                         {"padding", 17}}));
      const auto x = static_cast<float>((fi % 32) * 2 + mi * 1000),
                 y = static_cast<float>((fi / 32) * 2);
      vs.as_array().push_back(J::array({x, y, 0, 0}));
      vs.as_array().push_back(J::array({x + 1, y, 0, 0}));
      vs.as_array().push_back(J::array({x, y + 1, 0, 0}));
      for (unsigned e = 0; e < 3; ++e)
        es.as_array().push_back(J::object({{"a", fi * 3 + e},
                                           {"b", fi * 3 + (e + 1) % 3},
                                           {"oppositeEdge", 0xffffffffu},
                                           {"oppositeFace", 0xffffffffu},
                                           {"flags", 4},
                                           {"paddingByte", 0xab},
                                           {"userEdgeCost", 0.0}}));
    }
    const auto mesh = g.New(
                   13,
                   J::object({{"faces", fs}, {"edges", es}, {"vertices", vs}})),
               ptr = g.New(14, J());
    g.Node(ptr).target = mesh;
    g.Node(g.Field(g.Root(), "meshes")).elements.push_back(ptr);
    if (opaque_w) {
      const auto array = g.Field(mesh, "vertices");
      unsigned at = 0;
      for (const auto vertex : g.Node(array).elements) {
        auto &scalar = g.Node(g.Node(vertex).elements.at(3)).scalar;
        scalar =
            J::object({{"float32_bits", at++ % 2 ? 0x80000000u : 0x7fc12345u}});
      }
    }
  }
  return Envelope(g.Encode());
}
} // namespace Monstey_NavTreeFixture
