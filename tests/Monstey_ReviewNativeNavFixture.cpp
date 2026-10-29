#include "Monstey_ReviewNativeNavFixture.hpp"
#include <array>
#include <bit>
#include <map>
namespace Monstey {
namespace {
using B = Monstey_Bytes;
struct Monstey_Field {
  std::string name;
  std::uint32_t offset, type;
};
struct Monstey_Type {
  std::string name;
  std::uint32_t kind, bytes, align, pointer = 0, version = 0;
  std::vector<Monstey_Field> fields;
};
void Monstey_Pack(B &out, std::uint32_t v) {
  if (v < 128)
    out.push_back(static_cast<std::uint8_t>(v));
  else if (v < 0x4000) {
    out.push_back(static_cast<std::uint8_t>(0x80 | (v >> 8)));
    out.push_back(static_cast<std::uint8_t>(v));
  } else if (v < 0x200000) {
    out.push_back(static_cast<std::uint8_t>(0xc0 | (v >> 16)));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
  } else {
    out.push_back(static_cast<std::uint8_t>(0xe0 | (v >> 24)));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
  }
}
void Monstey_Add(B &to, const B &from) {
  to.insert(to.end(), from.begin(), from.end());
}
B Monstey_Section(std::string_view name, B payload, bool flag = true) {
  while (payload.size() % 4)
    payload.push_back(0);
  const auto length = static_cast<std::uint32_t>(payload.size() + 8) |
                      (flag ? 0x40000000u : 0u);
  B out{static_cast<std::uint8_t>(length >> 24),
        static_cast<std::uint8_t>(length >> 16),
        static_cast<std::uint8_t>(length >> 8),
        static_cast<std::uint8_t>(length)};
  out.insert(out.end(), name.begin(), name.end());
  Monstey_Add(out, payload);
  return out;
}
B Monstey_Text(const std::vector<std::string> &text) {
  B out;
  for (const auto &s : text) {
    out.insert(out.end(), s.begin(), s.end());
    out.push_back(0);
  }
  return out;
}
void Monstey_Align(B &bytes, std::size_t n) {
  while (bytes.size() % n)
    bytes.push_back(0);
}
} // namespace
B Monstey_ReviewNativeNavFixture(unsigned count, unsigned meshes,
                                 std::uint32_t w_bits, unsigned variant) {
  Monstey_Require(meshes >= 1 && meshes <= 2 && count <= 512,
                  "Independent synthetic fixture bounds");
  std::vector<Monstey_Type> types{
      {"", 0, 0, 0},
      {"float", 5, 4, 4},
      {"hkInt32", 0x8204, 4, 4},
      {"hkInt16", 0x4204, 2, 2},
      {"hkUint32", 0x8004, 4, 4},
      {"hkUint16", 0x4004, 2, 2},
      {"hkUint8", 0x2004, 1, 1},
      {"hkVector4", 0x428, 16, 16, 1},
      {"hkaiNavMesh::Face",
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
        {"padding", 14, 5}}},
      {"hkaiNavMesh::Edge",
       7,
       20,
       4,
       0,
       8,
       {{"a", 0, 2},
        {"b", 4, 2},
        {"oppositeEdge", 8, 4},
        {"oppositeFace", 12, 4},
        {"flags", 16, 6},
        {"paddingByte", 17, 6},
        {"userEdgeCost", 18, 10}}},
      {"hkHalf16", 5, 2, 2},
      {"Faces", 8, 16, 8, 8},
      {"Edges", 8, 16, 8, 9},
      {"Vertices", 8, 16, 8, 7},
      {"hkaiNavMesh",
       7,
       208,
       16,
       0,
       16,
       {{"faces", 24, 11}, {"edges", 40, 12}, {"vertices", 56, 13}}},
      {"MeshPointer", 6, 8, 8, 14},
      {"MeshPointers", 8, 16, 8, 15},
      {"Root", 7, 16, 8, 0, 0, {{"meshes", 0, 16}}}};
  if (variant == 1)
    types[8].fields[0].offset = 4;
  if (variant == 2)
    types[8].bytes = 20;
  if (variant == 3)
    types[7].kind = 0x828;
  if (variant == 4)
    types[8].fields[2].type = 5;
  if (variant == 5)
    types[14].fields[0].offset = 16;
  if (variant == 6)
    types[2].kind = 0x4204;
  if (variant == 8)
    types[14].version = 99;
  if (variant == 9)
    types[14].name = "NotANavMesh";
  std::vector<std::string> names, fields;
  std::map<std::string, std::uint32_t> fieldIds;
  for (std::size_t i = 1; i < types.size(); ++i) {
    names.push_back(types[i].name);
    for (const auto &f : types[i].fields)
      if (!fieldIds.contains(f.name)) {
        fieldIds.emplace(f.name, static_cast<std::uint32_t>(fields.size()));
        fields.push_back(f.name);
      }
  }
  B tna, tbod, thsh;
  Monstey_Pack(tna, static_cast<std::uint32_t>(types.size()));
  Monstey_Pack(thsh, static_cast<std::uint32_t>(types.size() - 1));
  for (std::uint32_t i = 1; i < types.size(); ++i) {
    const auto &t = types[i];
    Monstey_Pack(tna, i - 1);
    Monstey_Pack(tna, 0);
    Monstey_Pack(tbod, i);
    Monstey_Pack(tbod, 0);
    const bool cls = t.kind == 7;
    Monstey_Pack(tbod, 9u | (t.pointer ? 2u : 0u) | (cls ? 36u : 0u));
    Monstey_Pack(tbod, t.kind);
    if (t.pointer)
      Monstey_Pack(tbod, t.pointer);
    if (cls)
      Monstey_Pack(tbod, t.version);
    Monstey_Pack(tbod, t.bytes);
    Monstey_Pack(tbod, t.align);
    if (cls) {
      Monstey_Pack(tbod, static_cast<std::uint32_t>(t.fields.size()));
      for (const auto &f : t.fields) {
        Monstey_Pack(tbod, fieldIds.at(f.name));
        Monstey_Pack(tbod, 0);
        Monstey_Pack(tbod, f.offset);
        Monstey_Pack(tbod, f.type);
      }
    }
    Monstey_Pack(thsh, i);
    Monstey_AppendU32(thsh, 0xc0010000u + i);
  }
  B type;
  Monstey_Add(type, Monstey_Section("TPTR", {}));
  Monstey_Add(type, Monstey_Section("TSTR", Monstey_Text(names)));
  Monstey_Add(type, Monstey_Section("TNA1", tna));
  Monstey_Add(type, Monstey_Section("FSTR", Monstey_Text(fields)));
  Monstey_Add(type, Monstey_Section("TBOD", tbod));
  Monstey_Add(type, Monstey_Section("THSH", thsh));
  Monstey_Add(type, Monstey_Section("TPAD", {}));
  struct Monstey_Item {
    std::uint32_t type, offset, count, flag;
  };
  std::vector<Monstey_Item> items{
      {0, 0, 0, 0}, {17, 0, 1, 0x10000000u}, {15, 16, meshes, 0x20000000u}};
  B data(16 + meshes * 8);
  Monstey_PutU32(data, 0, 2);
  const auto AddItem = [&](std::uint32_t ti, std::size_t off,
                           std::uint32_t number,
                           std::uint32_t flag = 0x20000000u) {
    items.push_back({ti, static_cast<std::uint32_t>(off), number, flag});
    return static_cast<std::uint32_t>(items.size() - 1);
  };
  for (unsigned mesh = 0; mesh < meshes; ++mesh) {
    Monstey_Align(data, 16);
    const auto mo = data.size();
    data.resize(mo + 208);
    const auto mi = AddItem(14, mo, 1, 0x10000000u);
    Monstey_PutU32(data, 16 + mesh * 8, mi);
    Monstey_Align(data, 4);
    const auto fo = data.size();
    data.resize(fo + count * types[8].bytes);
    const auto fi = AddItem(8, fo, count);
    Monstey_Align(data, 4);
    const auto eo = data.size();
    data.resize(eo + count * 60);
    const auto ei = AddItem(9, eo, count * 3);
    Monstey_Align(data, 16);
    const auto vo = data.size();
    data.resize(vo + count * 48);
    const auto vi = AddItem(7, vo, count * 3);
    Monstey_PutU32(data, mo + types[14].fields[0].offset, fi);
    Monstey_PutU32(data, mo + 40, ei);
    Monstey_PutU32(data, mo + 56, vi);
    for (unsigned face = 0; face < count; ++face) {
      const auto fs = fo + face * types[8].bytes;
      Monstey_PutU32(data, fs + types[8].fields[0].offset, face * 3);
      Monstey_PutU16(data, fs + 8, 3);
      Monstey_PutU32(data, fs + 4, 0xffffffff);
      Monstey_PutU16(data, fs + 12, 0xffff);
      Monstey_PutU16(data, fs + 14, 0xcdcd);
      const float x = static_cast<float>(face * 2 + mesh * 100);
      const std::array<std::array<float, 3>, 3> p{
          {{x, 0, 0}, {x + 1, 0, 0}, {x, 0, 1}}};
      for (unsigned v = 0; v < 3; ++v) {
        const auto edge = eo + (face * 3 + v) * 20,
                   vertex = vo + (face * 3 + v) * 16;
        Monstey_PutU32(data, edge, face * 3 + v);
        Monstey_PutU32(data, edge + 4, face * 3 + (v + 1) % 3);
        Monstey_PutU32(data, edge + 8, 0xffffffff);
        Monstey_PutU32(data, edge + 12, 0xffffffff);
        for (unsigned axis = 0; axis < 3; ++axis)
          Monstey_PutU32(data, vertex + axis * 4,
                         std::bit_cast<std::uint32_t>(p[v][axis]));
        Monstey_PutU32(data, vertex + 12, w_bits);
      }
    }
  }
  B item;
  for (const auto &i : items) {
    Monstey_AppendU32(item, i.type | i.flag);
    Monstey_AppendU32(item, i.offset);
    Monstey_AppendU32(item, i.count);
  }
  B index;
  Monstey_Add(index, Monstey_Section("ITEM", item));
  Monstey_Add(index, Monstey_Section("PTCH", {}));
  B tag;
  Monstey_Add(
      tag, Monstey_Section("SDKV", B{'2', '0', '1', '6', '0', '1', '0', '0'}));
  Monstey_Add(tag, Monstey_Section("DATA", data));
  Monstey_Add(tag, Monstey_Section("TYPE", type, false));
  Monstey_Add(tag, Monstey_Section("INDX", index, false));
  const auto encoded = Monstey_Section("TAG0", tag, false);
  B out{'H', 'K', 'A', 'I'};
  Monstey_AppendU32(out, 3);
  Monstey_AppendU32(out, 0xffffd00d);
  Monstey_AppendU32(out, static_cast<std::uint32_t>(encoded.size()));
  Monstey_Add(out, encoded);
  Monstey_AppendU32(out, 0xffffdead);
  return out;
}
} // namespace Monstey
