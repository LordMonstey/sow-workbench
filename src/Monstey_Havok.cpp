#include "Monstey_Havok.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <tuple>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
constexpr std::size_t Monstey_MaxNodes = 4000000,
                      Monstey_MaxData = 256u * 1024 * 1024,
                      Monstey_MaxDepth = 512;
struct Monstey_TagSection {
  std::string name;
  B raw;
  std::size_t offset = 0;
};
std::uint32_t Monstey_BE(std::span<const std::uint8_t> b, std::size_t at) {
  Monstey_CheckRange(b, at, 4);
  return (std::uint32_t(b[at]) << 24) | (std::uint32_t(b[at + 1]) << 16) |
         (std::uint32_t(b[at + 2]) << 8) | b[at + 3];
}
void Monstey_AppendBE(B &b, std::uint32_t n) {
  b.push_back(std::uint8_t(n >> 24));
  b.push_back(std::uint8_t(n >> 16));
  b.push_back(std::uint8_t(n >> 8));
  b.push_back(std::uint8_t(n));
}
std::vector<Monstey_TagSection>
Monstey_Sections(std::span<const std::uint8_t> b, std::string_view sig) {
  Monstey_Require(
      b.size() >= 8 &&
          std::string_view(reinterpret_cast<const char *>(b.data() + 4), 4) ==
              sig,
      "Invalid Havok section signature");
  Monstey_Require((Monstey_BE(b, 0) & 0x3fffffff) == b.size(),
                  "Havok section extent mismatch");
  std::vector<Monstey_TagSection> out;
  std::set<std::string> names;
  for (std::size_t at = 8; at < b.size();) {
    Monstey_CheckRange(b, at, 8);
    const auto n = Monstey_BE(b, at) & 0x3fffffff;
    Monstey_Require(n >= 8 && n <= b.size() - at,
                    "Havok child outside container");
    std::string name(reinterpret_cast<const char *>(b.data() + at + 4), 4);
    Monstey_Require(std::all_of(name.begin(), name.end(),
                                [](char c) { return c >= 32 && c <= 126; }),
                    "Non-ASCII Havok section");
    Monstey_Require(names.insert(name).second, "Duplicate Havok section");
    out.push_back({name,
                   B(b.begin() + static_cast<std::ptrdiff_t>(at),
                     b.begin() + static_cast<std::ptrdiff_t>(at + n)),
                   at});
    at += n;
  }
  return out;
}
const Monstey_TagSection &
Monstey_Section(const std::vector<Monstey_TagSection> &p,
                std::string_view name) {
  const auto it = std::find_if(p.begin(), p.end(),
                               [&](const auto &x) { return x.name == name; });
  Monstey_Require(it != p.end(), "Missing Havok section " + std::string(name));
  return *it;
}
B Monstey_MakeSection(std::string_view name, B payload,
                      std::uint32_t flags = 0x40000000) {
  while (payload.size() % 4)
    payload.push_back(0);
  Monstey_Require(payload.size() + 8 < 0x40000000,
                  "Havok section size overflow");
  B b;
  Monstey_AppendBE(b, flags | static_cast<std::uint32_t>(payload.size() + 8));
  b.insert(b.end(), name.begin(), name.end());
  b.insert(b.end(), payload.begin(), payload.end());
  return b;
}
B Monstey_Recontainer(const B &original,
                      const std::vector<Monstey_TagSection> &parts,
                      const std::map<std::string, B> &replace) {
  B body;
  for (const auto &p : parts) {
    const auto it = replace.find(p.name);
    const auto &x = it == replace.end() ? p.raw : it->second;
    body.insert(body.end(), x.begin(), x.end());
  }
  return Monstey_MakeSection(
      std::string_view(reinterpret_cast<const char *>(original.data() + 4), 4),
      std::move(body), Monstey_BE(original, 0) & 0xc0000000);
}
std::uint64_t Monstey_HavokUnsigned(const J &value) {
  if (value.is_integer())
    return value.as_uint();
  Monstey_Require(value.is_string() && value.as_string().starts_with("0x") &&
                      value.as_string().size() == 18,
                  "Unsigned64 requires integer or exact 0x hex string");
  std::uint64_t result = 0;
  const auto &s = value.as_string();
  const auto parsed =
      std::from_chars(s.data() + 2, s.data() + s.size(), result, 16);
  Monstey_Require(parsed.ec == std::errc() && parsed.ptr == s.data() + s.size(),
                  "Invalid unsigned64 hex value");
  return result;
}
struct Monstey_PackedReader {
  std::span<const std::uint8_t> bytes;
  std::size_t at = 8;
  std::uint32_t Read() {
    Monstey_CheckRange(bytes, at, 1);
    const auto x = bytes[at++];
    if (!(x & 0x80))
      return x;
    if (!(x & 0x40)) {
      Monstey_CheckRange(bytes, at, 1);
      return ((std::uint32_t(x) << 8) | bytes[at++]) & 0x3fff;
    }
    if (!(x & 0x20)) {
      Monstey_CheckRange(bytes, at, 2);
      const auto n = ((std::uint32_t(x) << 16) |
                      (std::uint32_t(bytes[at]) << 8) | bytes[at + 1]) &
                     0x1fffff;
      at += 2;
      return n;
    }
    Monstey_CheckRange(bytes, at, 3);
    const auto n =
        ((std::uint32_t(x) << 24) | (std::uint32_t(bytes[at]) << 16) |
         (std::uint32_t(bytes[at + 1]) << 8) | bytes[at + 2]) &
        0x7ffffff;
    at += 3;
    return n;
  }
};
std::vector<std::string> Monstey_Strings(const B &bytes) {
  std::vector<std::string> out;
  std::size_t start = 8;
  for (std::size_t i = 8; i < bytes.size(); ++i)
    if (bytes[i] == 0) {
      out.emplace_back(reinterpret_cast<const char *>(bytes.data() + start),
                       i - start);
      start = i + 1;
    }
  Monstey_Require(start == bytes.size(),
                  "Unterminated Havok dictionary string");
  return out;
}
std::uint32_t Monstey_IntBytes(std::uint32_t flags) {
  if (flags & 0x2000)
    return 1;
  if (flags & 0x4000)
    return 2;
  if (flags & 0x8000)
    return 4;
  if (flags & 0x10000)
    return 8;
  throw Monstey_Error("HAVOK_INTEGER_TYPE", "Integer lacks width flag");
}
std::uint64_t Monstey_Unsigned(std::span<const std::uint8_t> b, std::size_t at,
                               std::size_t n) {
  Monstey_CheckRange(b, at, n);
  std::uint64_t v = 0;
  for (std::size_t i = 0; i < n; ++i)
    v |= std::uint64_t(b[at + i]) << (i * 8);
  return v;
}
void Monstey_PutInteger(B &b, std::size_t at, std::size_t n, std::uint64_t v) {
  Monstey_CheckRange(b, at, n);
  for (std::size_t i = 0; i < n; ++i)
    b[at + i] = std::uint8_t(v >> (i * 8));
}
} // namespace
struct Monstey_Havok::Impl {
  B source, data;
  std::vector<Monstey_TagSection> sections, typeSections, indexSections;
  std::vector<Monstey_HavokType> types;
  struct Item {
    std::uint32_t word = 0, offset = 0, count = 0;
    std::vector<std::size_t> nodes;
    bool loaded = false;
  };
  std::vector<Item> items;
  std::vector<Monstey_HavokNode> nodes;
  std::map<std::pair<std::uint32_t, std::size_t>, std::size_t> physical;
  std::size_t root = 0;
  J initial;
  std::string sdk;
  std::set<std::size_t> reading, read;
  std::uint32_t Effective(std::uint32_t type) const {
    for (std::size_t depth = 0; depth <= types.size(); ++depth) {
      Monstey_Require(type > 0 && type < types.size(),
                      "Havok type outside dictionary");
      if (types[type].flags & 1)
        return type;
      type = types[type].parent;
    }
    throw Monstey_Error("HAVOK_TYPE_CYCLE", "Type inheritance cycle");
  }
  std::vector<Monstey_HavokMember> Members(std::uint32_t type) const {
    std::vector<std::uint32_t> chain;
    std::set<std::uint32_t> seen;
    while (type) {
      Monstey_Require(type < types.size() && seen.insert(type).second,
                      "Invalid/cyclic inheritance");
      chain.push_back(type);
      type = types[type].parent;
    }
    std::vector<Monstey_HavokMember> out;
    std::set<std::string> names;
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
      for (const auto &m : types[*it].members) {
        Monstey_Require(names.insert(m.name).second,
                        "Duplicate inherited member");
        out.push_back(m);
      }
    return out;
  }
  explicit Impl(std::span<const std::uint8_t> bytes)
      : source(bytes.begin(), bytes.end()) {
    Monstey_Require(source.size() <= Monstey_MaxData,
                    "Havok exceeds 256 MiB input");
    sections = Monstey_Sections(source, "TAG0");
    const auto &s = Monstey_Section(sections, "SDKV").raw;
    Monstey_Require(s.size() == 16, "Invalid SDKV length");
    sdk.assign(reinterpret_cast<const char *>(s.data() + 8), 8);
    Monstey_Require(sdk == "20160100" || sdk == "20160200" || sdk == "20150100",
                    "Unsupported SDK");
    const auto &d = Monstey_Section(sections, "DATA").raw;
    data.assign(d.begin() + 8, d.end());
    typeSections =
        Monstey_Sections(Monstey_Section(sections, "TYPE").raw, "TYPE");
    ParseTypes();
    indexSections =
        Monstey_Sections(Monstey_Section(sections, "INDX").raw, "INDX");
    const auto &ib = Monstey_Section(indexSections, "ITEM").raw;
    Monstey_Require((ib.size() - 8) % 12 == 0 &&
                        (ib.size() - 8) / 12 <= Monstey_MaxNodes,
                    "Invalid ITEM extent");
    for (std::size_t at = 8; at < ib.size(); at += 12) {
      Item item;
      item.word = Monstey_U32(ib, at);
      item.offset = Monstey_U32(ib, at + 4);
      item.count = Monstey_U32(ib, at + 8);
      if (item.word) {
        const auto ti = item.word & 0xffffff;
        Monstey_Require(ti > 0 && ti < types.size(),
                        "ITEM type outside dictionary");
        Monstey_Require((item.word & 0xf0000000) == 0x10000000 ||
                            (item.word & 0xf0000000) == 0x20000000,
                        "Unknown ITEM category");
        const auto &t = types[Effective(ti)];
        Monstey_Require(
            item.count <= Monstey_MaxNodes && item.offset <= data.size() &&
                (t.bytes == 0
                     ? item.count <= 1
                     : item.count <= (data.size() - item.offset) / t.bytes),
            "ITEM outside DATA");
      } else
        Monstey_Require(item.offset == 0 && item.count == 0,
                        "Invalid null ITEM");
      items.push_back(std::move(item));
    }
    Monstey_Require(items.size() > 1 && items[0].word == 0 && items[1].word &&
                        items[1].count == 1,
                    "Root ITEM must contain one object");
    LoadItem(1, 0);
    root = items[1].nodes[0];
    AuditRaw();
    initial = Canonical(root);
  }
  void ParseTypes() {
    const auto strings =
        Monstey_Strings(Monstey_Section(typeSections, "TSTR").raw);
    const auto fields =
        Monstey_Strings(Monstey_Section(typeSections, "FSTR").raw);
    const auto &names =
        Monstey_Section(
            typeSections,
            std::any_of(typeSections.begin(), typeSections.end(),
                        [](const auto &p) { return p.name == "TNA1"; })
                ? "TNA1"
                : "TNAM")
            .raw;
    Monstey_PackedReader n{names};
    const auto count = n.Read();
    Monstey_Require(count >= 2 && count <= 65536, "Type count outside bound");
    types.resize(count);
    for (std::uint32_t i = 1; i < count; ++i) {
      const auto name = n.Read();
      Monstey_Require(name < strings.size(), "Type name outside TSTR");
      types[i].name = strings[name];
      const auto nt = n.Read();
      Monstey_Require(nt <= 4096, "Too many templates");
      for (std::uint32_t j = 0; j < nt; ++j) {
        const auto sn = n.Read(), v = n.Read();
        Monstey_Require(sn < strings.size(), "Template name invalid");
        if (!strings[sn].empty() && strings[sn][0] == 't')
          Monstey_Require(v < count, "Template type invalid");
        types[i].templates.emplace_back(strings[sn], v);
      }
    }
    const auto &body =
        Monstey_Section(
            typeSections,
            std::any_of(typeSections.begin(), typeSections.end(),
                        [](const auto &p) { return p.name == "TBDY"; })
                ? "TBDY"
                : "TBOD")
            .raw;
    Monstey_PackedReader b{body};
    std::set<std::uint32_t> defined;
    while (b.at < body.size()) {
      const auto ti = b.Read();
      if (!ti)
        continue;
      Monstey_Require(ti < count && defined.insert(ti).second,
                      "Invalid/duplicate type body");
      auto &t = types[ti];
      t.parent = b.Read();
      t.flags = b.Read();
      Monstey_Require(t.parent < count && (t.flags & ~0x7fu) == 0,
                      "Unsupported type flags/parent");
      if (t.flags & 1)
        t.subtype = b.Read();
      if ((t.flags & 2) && (t.subtype & 0xf) >= 6)
        t.pointer = b.Read();
      if (t.flags & 4)
        t.version = b.Read();
      if (t.flags & 8) {
        t.bytes = b.Read();
        t.alignment = b.Read();
        Monstey_Require(
            t.bytes <= Monstey_MaxData && t.alignment <= 4096 &&
                (t.alignment == 0 || (t.alignment & (t.alignment - 1)) == 0),
            "Invalid layout/alignment");
      }
      if (t.flags & 16)
        t.abstract_value = b.Read();
      if (t.flags & 32) {
        const auto nm = b.Read();
        Monstey_Require(nm <= 65536, "Too many members");
        for (std::uint32_t j = 0; j < nm; ++j) {
          const auto name = b.Read(), flags = b.Read(), offset = b.Read(),
                     mt = b.Read();
          Monstey_Require(name < fields.size() && mt > 0 && mt < count,
                          "Member dictionary reference invalid");
          t.members.push_back({fields[name], flags, offset, mt});
        }
      }
      if (t.flags & 64) {
        const auto ni = b.Read();
        Monstey_Require(ni <= 65536, "Too many interfaces");
        for (std::uint32_t j = 0; j < ni; ++j) {
          const auto type = b.Read(), off = b.Read();
          Monstey_Require(type > 0 && type < count, "Interface type invalid");
          t.interfaces.emplace_back(type, off);
        }
      }
      Monstey_Require(t.pointer < count, "Pointer type invalid");
    }
    const auto &hs = Monstey_Section(typeSections, "THSH").raw;
    Monstey_PackedReader h{hs};
    const auto nh = h.Read();
    Monstey_Require(nh < count, "Invalid THSH count");
    std::set<std::uint32_t> hashes;
    for (std::uint32_t i = 0; i < nh; ++i) {
      const auto ti = h.Read();
      Monstey_Require(ti > 0 && ti < count && hashes.insert(ti).second,
                      "Invalid/duplicate THSH type");
      types[ti].hash = Monstey_U32(hs, h.at);
      h.at += 4;
    }
    Monstey_CheckRange(hs, h.at, 0);
    Monstey_Require(std::all_of(hs.begin() + static_cast<std::ptrdiff_t>(h.at),
                                hs.end(), [](auto c) { return c == 0; }),
                    "THSH trailing nonzero bytes");
    for (std::uint32_t i = 1; i < count; ++i) {
      const auto &t = types[Effective(i)];
      if ((t.subtype & 0xff) == 7)
        for (const auto &m : Members(Effective(i)))
          Monstey_Require(m.offset <= t.bytes &&
                              types[Effective(m.type)].bytes <=
                                  t.bytes - m.offset,
                          "Class member outside layout");
    }
  }
  void LoadItem(std::size_t index, std::size_t depth) {
    Monstey_Require(index < items.size(), "Pointer outside ITEM");
    if (!index || items[index].loaded)
      return;
    auto &item = items[index];
    Monstey_Require(item.word != 0, "Nonzero pointer names null ITEM");
    item.loaded = true;
    const auto ti = item.word & 0xffffff, stride = types[Effective(ti)].bytes;
    item.nodes.reserve(item.count);
    for (std::uint32_t i = 0; i < item.count; ++i)
      item.nodes.push_back(Reserve(ti, item.offset + std::size_t(i) * stride));
    const auto ids = item.nodes;
    for (std::uint32_t i = 0; i < item.count; ++i)
      ReadNode(ids[i], item.offset + std::size_t(i) * stride, depth + 1);
  }
  std::size_t Reserve(std::uint32_t ti, std::size_t offset) {
    const auto key = std::make_pair(ti, offset);
    if (const auto it = physical.find(key); it != physical.end())
      return it->second;
    Monstey_Require(nodes.size() < Monstey_MaxNodes,
                    "Graph node limit exceeded");
    const auto id = nodes.size();
    nodes.emplace_back();
    nodes.back().type = ti;
    physical.emplace(key, id);
    return id;
  }
  void ReadNode(std::size_t id, std::size_t offset, std::size_t depth) {
    if (read.contains(id) || reading.contains(id))
      return;
    Monstey_Require(depth <= Monstey_MaxDepth, "Graph nesting exceeds bound");
    reading.insert(id);
    const auto ti = nodes[id].type;
    const auto &t = types[Effective(ti)];
    Monstey_CheckRange(data, offset, t.bytes);
    nodes[id].original_storage.assign(
        data.begin() + static_cast<std::ptrdiff_t>(offset),
        data.begin() + static_cast<std::ptrdiff_t>(offset + t.bytes));
    const auto kind = t.subtype & 0xff;
    if (kind == 2 || kind == 4) {
      const auto n = Monstey_IntBytes(t.subtype);
      Monstey_Require(n <= t.bytes, "Integer width exceeds type size");
      const auto v = Monstey_Unsigned(data, offset, n);
      if (kind == 2)
        nodes[id].scalar = v != 0;
      else if (t.subtype & 0x200) {
        const auto sv = n == 8 ? std::bit_cast<std::int64_t>(v)
                               : static_cast<std::int64_t>(
                                     v ^ (std::uint64_t(1) << (n * 8 - 1))) -
                                     static_cast<std::int64_t>(std::uint64_t(1)
                                                               << (n * 8 - 1));
        nodes[id].scalar = sv;
      } else
        nodes[id].scalar =
            v > std::uint64_t(INT64_MAX) ? J(Monstey_Hex(v, 16)) : J(v);
    } else if (kind == 5) {
      Monstey_Require(t.bytes >= 4 || (t.name == "hkHalf16" && t.bytes == 2),
                      "Unknown floating layout");
      const auto bits = t.bytes == 2
                            ? std::uint32_t(Monstey_U16(data, offset)) << 16
                            : Monstey_U32(data, offset);
      const auto f = std::bit_cast<float>(bits);
      nodes[id].scalar = std::isfinite(f) ? J(static_cast<double>(f))
                                          : J::object({{"float32_bits", bits}});
    } else if (kind == 3 || kind == 6 || kind == 8) {
      Monstey_Require(t.bytes >= 4, "Item pointer field too small");
      const auto index = Monstey_U32(data, offset);
      LoadItem(index, depth + 1);
      if (kind == 6) {
        if (index) {
          Monstey_Require(items[index].nodes.size() == 1,
                          "Object pointer references multiple values");
          nodes[id].target = items[index].nodes[0];
        }
      } else if (kind == 8) {
        if (index)
          nodes[id].elements = items[index].nodes;
      } else {
        std::string s;
        if (index) {
          const auto &item = items[index];
          Monstey_Require(item.count > 0 && item.count <= 1024 * 1024,
                          "String length outside bound");
          Monstey_CheckRange(data, item.offset, item.count);
          Monstey_Require(data[item.offset + item.count - 1] == 0,
                          "String lacks terminator");
          s.assign(reinterpret_cast<const char *>(data.data() + item.offset),
                   item.count - 1);
          Monstey_Require(s.find('\0') == std::string::npos,
                          "String has interior NUL");
        }
        nodes[id].scalar = std::move(s);
      }
    } else if (kind == 7) {
      for (const auto &m : Members(Effective(ti))) {
        const auto child = Reserve(m.type, offset + m.offset);
        nodes[id].fields.emplace_back(m.name, child);
        ReadNode(child, offset + m.offset, depth + 1);
      }
    } else if (kind == 0x28) {
      Monstey_Require(t.pointer > 0, "Tuple lacks element type");
      const auto count = t.subtype >> 8,
                 stride = types[Effective(t.pointer)].bytes;
      Monstey_Require(count <= Monstey_MaxNodes &&
                          std::uint64_t(count) * stride <= t.bytes,
                      "Tuple outside layout");
      for (std::uint32_t i = 0; i < count; ++i) {
        const auto child = Reserve(t.pointer, offset + std::size_t(i) * stride);
        nodes[id].elements.push_back(child);
        ReadNode(child, offset + std::size_t(i) * stride, depth + 1);
      }
    }
    reading.erase(id);
    read.insert(id);
  }
  J Canonical(std::size_t start) const {
    J rows = J::array();
    std::map<std::size_t, std::size_t> seen;
    std::function<J(std::size_t, std::size_t)> visit =
        [&](std::size_t id, std::size_t depth) -> J {
      Monstey_Require(id < nodes.size() && depth <= Monstey_MaxDepth,
                      "Invalid/deep node relationship");
      if (const auto it = seen.find(id); it != seen.end())
        return J::object({{"ref", it->second}});
      const auto index = rows.size();
      seen.emplace(id, index);
      rows.as_array().emplace_back();
      const auto &n = nodes[id];
      const auto kind = types[Effective(n.type)].subtype & 0xff;
      J value;
      if (kind == 6)
        value = n.target ? visit(*n.target, depth + 1) : J();
      else if (kind == 7) {
        value = J::array();
        for (const auto &f : n.fields)
          value.as_array().push_back(
              J::array({f.first, visit(f.second, depth + 1)}));
      } else if (kind == 8 || kind == 0x28) {
        value = J::array();
        for (const auto child : n.elements)
          value.as_array().push_back(visit(child, depth + 1));
      } else if (kind == 5)
        value =
            n.scalar.is_object()
                ? n.scalar
                : J::object({{"float32_bits",
                              std::bit_cast<std::uint32_t>(
                                  static_cast<float>(n.scalar.as_number()))}});
      else if (kind == 2 || kind == 3 || kind == 4)
        value = n.scalar;
      else {
        std::string hex;
        constexpr char digits[] = "0123456789abcdef";
        for (auto b : n.original_storage) {
          hex += digits[b >> 4];
          hex += digits[b & 15];
        }
        value = hex;
      }
      rows[index] = J::array({n.type, types[n.type].name, value});
      return J::object({{"ref", index}});
    };
    visit(start, 0);
    return rows;
  }
  J Plain(std::size_t start) const {
    std::set<std::size_t> active;
    std::function<J(std::size_t, std::size_t)> visit =
        [&](std::size_t id, std::size_t depth) -> J {
      Monstey_Require(id < nodes.size() && depth <= Monstey_MaxDepth &&
                          active.insert(id).second,
                      "Cyclic/deep graph cannot be flattened; use Canonical");
      const auto &n = nodes[id];
      const auto kind = types[Effective(n.type)].subtype & 0xff;
      J value;
      if (kind == 6)
        value = n.target ? visit(*n.target, depth + 1) : J();
      else if (kind == 7) {
        value = J::object();
        for (const auto &f : n.fields)
          value[f.first] = visit(f.second, depth + 1);
      } else if (kind == 8 || kind == 0x28) {
        value = J::array();
        for (const auto child : n.elements)
          value.as_array().push_back(visit(child, depth + 1));
      } else
        value = n.scalar;
      active.erase(id);
      return value;
    };
    return visit(start, 0);
  }
  void AuditRaw() const {
    const auto &pb = Monstey_Section(indexSections, "PTCH").raw;
    std::set<std::uint32_t> seen, offsets;
    for (std::size_t at = 8; at < pb.size();) {
      Monstey_CheckRange(pb, at, 8);
      const auto ti = Monstey_U32(pb, at), n = Monstey_U32(pb, at + 4);
      at += 8;
      Monstey_Require(ti > 0 && ti < types.size() && seen.insert(ti).second &&
                          n <= (pb.size() - at) / 4,
                      "Invalid PTCH group");
      const auto kind = types[Effective(ti)].subtype & 0xff;
      Monstey_Require(kind == 3 || kind == 6 || kind == 8,
                      "PTCH type is not pointer/storage");
      for (std::uint32_t j = 0; j < n; ++j) {
        const auto off = Monstey_U32(pb, at);
        at += 4;
        Monstey_CheckRange(data, off, 4);
        Monstey_Require(offsets.insert(off).second &&
                            Monstey_U32(data, off) > 0 &&
                            Monstey_U32(data, off) < items.size(),
                        "Invalid/duplicate relocation");
      }
    }
  }
};
Monstey_Havok::Monstey_Havok(std::span<const std::uint8_t> s)
    : impl_(std::make_unique<Impl>(s)) {}
Monstey_Havok::Monstey_Havok(const std::filesystem::path &s)
    : Monstey_Havok(Monstey_ReadFile(s, Monstey_MaxData)) {}
Monstey_Havok::~Monstey_Havok() = default;
Monstey_Havok::Monstey_Havok(Monstey_Havok &&) noexcept = default;
Monstey_Havok &Monstey_Havok::operator=(Monstey_Havok &&) noexcept = default;
const std::vector<Monstey_HavokType> &Monstey_Havok::Types() const {
  return impl_->types;
}
std::uint32_t Monstey_Havok::EffectiveType(std::uint32_t t) const {
  return impl_->Effective(t);
}
std::vector<Monstey_HavokMember> Monstey_Havok::Members(std::uint32_t t) const {
  return impl_->Members(t);
}
std::size_t Monstey_Havok::Root() const { return impl_->root; }
std::size_t Monstey_Havok::NodeCount() const { return impl_->nodes.size(); }
const Monstey_HavokNode &Monstey_Havok::Node(std::size_t id) const {
  Monstey_Require(id < NodeCount(), "Node outside graph");
  return impl_->nodes[id];
}
Monstey_HavokNode &Monstey_Havok::Node(std::size_t id) {
  Monstey_Require(id < NodeCount(), "Node outside graph");
  return impl_->nodes[id];
}
std::size_t Monstey_Havok::Field(std::size_t id, std::string_view name) const {
  for (const auto &f : Node(id).fields)
    if (f.first == name)
      return f.second;
  throw Monstey_Error("HAVOK_FIELD",
                      "Unknown native member " + std::string(name));
}
std::size_t Monstey_Havok::Dereference(std::size_t id) const {
  std::set<std::size_t> seen;
  while (Node(id).target) {
    Monstey_Require(seen.insert(id).second, "Cyclic pointer-only chain");
    id = *Node(id).target;
  }
  return id;
}
std::vector<std::size_t> Monstey_Havok::Objects(std::string_view name) const {
  std::vector<std::size_t> out;
  std::set<std::size_t> seen;
  std::vector<std::size_t> stack{Root()};
  while (!stack.empty()) {
    const auto id = stack.back();
    stack.pop_back();
    if (!seen.insert(id).second)
      continue;
    const auto &n = Node(id);
    if (Types()[n.type].name == name)
      out.push_back(id);
    if (n.target)
      stack.push_back(*n.target);
    for (auto it = n.fields.rbegin(); it != n.fields.rend(); ++it)
      stack.push_back(it->second);
    for (auto it = n.elements.rbegin(); it != n.elements.rend(); ++it)
      stack.push_back(*it);
  }
  return out;
}
std::size_t Monstey_Havok::New(std::uint32_t type, const J &plain) {
  Monstey_Require(type > 0 && type < Types().size() &&
                      NodeCount() < Monstey_MaxNodes,
                  "New node type/count invalid");
  const auto id = NodeCount();
  impl_->nodes.emplace_back();
  Node(id).type = type;
  Replace(id, plain);
  return id;
}
void Monstey_Havok::Replace(std::size_t id, const J &plain) {
  const auto ti = Node(id).type;
  const auto &t = Types()[EffectiveType(ti)];
  const auto kind = t.subtype & 0xff;
  if (kind == 2) {
    Monstey_Require(plain.is_bool(), "Expected Boolean");
    Node(id).scalar = plain;
  } else if (kind == 4) {
    Monstey_Require(plain.is_integer() ||
                        (!(t.subtype & 0x200) &&
                         Monstey_IntBytes(t.subtype) == 8 && plain.is_string()),
                    "Expected integer or unsigned64 hex");
    const auto w = Monstey_IntBytes(t.subtype);
    if (t.subtype & 0x200) {
      const auto v = plain.as_int();
      if (w < 8)
        Monstey_Require(v >= -(std::int64_t(1) << (w * 8 - 1)) &&
                            v < (std::int64_t(1) << (w * 8 - 1)),
                        "Signed integer outside range");
    } else {
      const auto v = Monstey_HavokUnsigned(plain);
      if (w < 8)
        Monstey_Require(v < (std::uint64_t(1) << (w * 8)),
                        "Unsigned integer outside range");
    }
    Node(id).scalar = plain;
  } else if (kind == 5) {
    Monstey_Require(plain.is_number() && std::isfinite(plain.as_number()),
                    "Expected finite float");
    const auto f = static_cast<float>(plain.as_number());
    Monstey_Require(std::isfinite(f), "Float32 range exceeded");
    if (t.bytes == 2)
      Monstey_Require(t.name == "hkHalf16" &&
                          (std::bit_cast<std::uint32_t>(f) & 0xffff) == 0,
                      "hkHalf16 requires exact truncated float32");
    Node(id).scalar = static_cast<double>(f);
  } else if (kind == 3) {
    Monstey_Require(plain.is_string() &&
                        plain.as_string().size() < 1024 * 1024 &&
                        plain.as_string().find('\0') == std::string::npos,
                    "Invalid string");
    Node(id).scalar = plain;
  } else if (kind == 6) {
    if (plain.is_null())
      Node(id).target.reset();
    else {
      Monstey_Require(t.pointer > 0, "Pointer lacks target type");
      if (Node(id).target)
        Replace(*Node(id).target, plain);
      else {
        const auto target = New(t.pointer, plain);
        Node(id).target = target;
      }
    }
  } else if (kind == 7) {
    Monstey_Require(plain.is_object(), "Expected class object");
    const auto members = Members(EffectiveType(ti));
    Monstey_Require(plain.size() == members.size(),
                    "Class requires every native field");
    for (const auto &m : members) {
      Monstey_Require(plain.contains(m.name), "Missing class member");
      const auto found =
          std::find_if(Node(id).fields.begin(), Node(id).fields.end(),
                       [&](const auto &f) { return f.first == m.name; });
      if (found != Node(id).fields.end()) {
        const auto child = found->second;
        Replace(child, plain.at(m.name));
      } else {
        const auto child = New(m.type, plain.at(m.name));
        Node(id).fields.emplace_back(m.name, child);
      }
    }
  } else if (kind == 8 || kind == 0x28) {
    Monstey_Require(plain.is_array() && plain.size() <= Monstey_MaxNodes &&
                        t.pointer > 0,
                    "Invalid array");
    if (kind == 0x28)
      Monstey_Require(plain.size() == t.subtype >> 8, "Tuple size mismatch");
    const auto detached = Clone(id);
    const auto old = Node(detached).elements;
    std::vector<std::size_t> next;
    for (std::size_t i = 0; i < plain.size(); ++i) {
      if (i < old.size()) {
        Replace(old[i], plain.at(i));
        next.push_back(old[i]);
      } else
        next.push_back(New(t.pointer, plain.at(i)));
    }
    Node(id).elements = std::move(next);
  } else
    Monstey_Require(plain.is_null() &&
                        (t.bytes == 0 || !Node(id).original_storage.empty()),
                    "Opaque subtype cannot be synthesized");
}
std::size_t Monstey_Havok::Clone(std::size_t id) {
  std::map<std::size_t, std::size_t> remap;
  std::function<std::size_t(std::size_t)> clone = [&](std::size_t src) {
    if (const auto it = remap.find(src); it != remap.end())
      return it->second;
    const auto copy = Node(src);
    Monstey_Require(NodeCount() < Monstey_MaxNodes,
                    "Clone node bound exceeded");
    const auto dst = NodeCount();
    impl_->nodes.push_back(copy);
    remap[src] = dst;
    if (copy.target) {
      const auto child = clone(*copy.target);
      Node(dst).target = child;
    }
    for (std::size_t i = 0; i < copy.elements.size(); ++i) {
      const auto child = clone(copy.elements[i]);
      Node(dst).elements[i] = child;
    }
    for (std::size_t i = 0; i < copy.fields.size(); ++i) {
      const auto child = clone(copy.fields[i].second);
      Node(dst).fields[i].second = child;
    }
    return dst;
  };
  return clone(id);
}
J Monstey_Havok::Plain(std::size_t id) const { return impl_->Plain(id); }
J Monstey_Havok::Canonical(std::optional<std::size_t> r) const {
  return impl_->Canonical(r.value_or(Root()));
}
const B &Monstey_Havok::Original() const { return impl_->source; }
J Monstey_Havok::TypeHashCoverage() const {
  J missing = J::array(), rows = J::array();
  std::set<std::uint32_t> active;
  for (std::size_t i = 1; i < impl_->items.size(); ++i) {
    const auto &item = impl_->items[i];
    if (!item.word)
      continue;
    const auto ti = item.word & 0xffffff;
    active.insert(ti);
    rows.as_array().push_back(J::object({{"item", i},
                                         {"type", ti},
                                         {"name", Types()[ti].name},
                                         {"hash", Types()[ti].hash},
                                         {"offset", item.offset},
                                         {"count", item.count}}));
  }
  const auto &p = Monstey_Section(impl_->indexSections, "PTCH").raw;
  for (std::size_t at = 8; at < p.size();) {
    const auto ti = Monstey_U32(p, at), count = Monstey_U32(p, at + 4);
    at += 8 + std::size_t(count) * 4;
    if (count)
      active.insert(ti);
  }
  for (const auto ti : active)
    if (!Types()[ti].hash)
      missing.as_array().push_back(
          J::object({{"type", ti}, {"name", Types()[ti].name}}));
  return J::object({{"status", missing.empty() ? "PASS" : "FAIL"},
                    {"referenced_types", active.size()},
                    {"items", rows},
                    {"missing_hashes", missing},
                    {"runtime_verified", false}});
}
J Monstey_Havok::Audit() const {
  impl_->AuditRaw();
  return J::object({{"status", "PASS"},
                    {"item_count", impl_->items.size()},
                    {"reachable_nodes", NodeCount()},
                    {"type_hash_coverage", TypeHashCoverage()},
                    {"runtime_verified", false}});
}
J Monstey_Havok::Describe() const {
  J ts = J::array(), ss = J::array();
  for (std::size_t i = 1; i < Types().size(); ++i) {
    const auto &t = Types()[i];
    ts.as_array().push_back(
        J::object({{"index", i},
                   {"name", t.name},
                   {"parent", t.parent},
                   {"flags", t.flags},
                   {"subtype", t.subtype},
                   {"effective", EffectiveType(static_cast<std::uint32_t>(i))},
                   {"bytes", t.bytes},
                   {"alignment", t.alignment},
                   {"hash", t.hash},
                   {"members", t.members.size()}}));
  }
  for (const auto &s : impl_->sections)
    ss.as_array().push_back(J::object({{"name", s.name},
                                       {"offset", s.offset},
                                       {"bytes", s.raw.size()},
                                       {"flags", Monstey_BE(s.raw, 0) >> 30}}));
  return J::object({{"format", "TAG0"},
                    {"sdk", impl_->sdk},
                    {"source_sha256", Monstey_HashSha256(Original())},
                    {"source_bytes", Original().size()},
                    {"types", ts},
                    {"sections", ss},
                    {"root", Root()},
                    {"nodes", NodeCount()},
                    {"audit", Audit()},
                    {"no_op_byte_exact", Canonical() == impl_->initial},
                    {"implementation", "C++20"},
                    {"python_runtime", false},
                    {"runtime_verified", false}});
}
B Monstey_Havok::Encode(bool force) const {
  if (!force && Canonical() == impl_->initial)
    return Original();
  struct Item {
    std::uint32_t type = 0;
    bool pointer = false;
    std::vector<std::size_t> ids;
    B text;
    std::size_t offset = 0;
  };
  std::vector<Item> items(1);
  std::map<std::pair<char, std::size_t>, std::uint32_t> identities;
  std::map<std::pair<std::uint32_t, std::vector<std::size_t>>, std::uint32_t>
      arrayIdentities;
  std::map<std::uint32_t, std::set<std::uint32_t>> patches;
  B data;
  auto index = [&](std::size_t id, bool root) -> std::uint32_t {
    const auto &n = Node(id);
    const auto &t = Types()[EffectiveType(n.type)];
    const auto k = t.subtype & 0xff;
    Item item;
    char storage = 'p';
    std::size_t identity = id;
    if (root) {
      item.type = n.type;
      item.pointer = true;
      item.ids = {id};
    } else if (k == 6) {
      if (!n.target)
        return 0;
      identity = *n.target;
      item.type = Node(identity).type;
      item.pointer = true;
      item.ids = {identity};
    } else if (k == 8) {
      if (n.elements.empty())
        return 0;
      storage = 'a';
      item.type = t.pointer;
      item.ids = n.elements;
    } else if (k == 3) {
      if (n.scalar.as_string().empty())
        return 0;
      storage = 's';
      const auto found =
          std::find_if(Types().begin() + 1, Types().end(),
                       [](const auto &x) { return x.name == "char"; });
      Monstey_Require(found != Types().end(), "TYPE lacks char storage");
      item.type = static_cast<std::uint32_t>(found - Types().begin());
      item.text.assign(n.scalar.as_string().begin(),
                       n.scalar.as_string().end());
      item.text.push_back(0);
    } else
      throw Monstey_Error("HAVOK_STORAGE", "Unsupported ITEM storage request");
    const auto arrayKey = std::make_pair(item.type, item.ids);
    if (storage == 'a')
      if (const auto it = arrayIdentities.find(arrayKey);
          it != arrayIdentities.end())
        return it->second;
    const auto key = std::make_pair(storage, identity);
    if (const auto it = identities.find(key); it != identities.end())
      return it->second;
    Monstey_Require(items.size() < 0xffffff, "ITEM count exceeds index space");
    const auto result = static_cast<std::uint32_t>(items.size());
    identities.emplace(key, result);
    if (storage == 'a')
      arrayIdentities.emplace(arrayKey, result);
    items.push_back(std::move(item));
    return result;
  };
  index(Root(), true);
  std::function<void(std::size_t, std::size_t, std::size_t)> write =
      [&](std::size_t id, std::size_t off, std::size_t depth) {
        Monstey_Require(depth <= Monstey_MaxDepth,
                        "Writer nesting exceeds bound");
        const auto n = Node(id);
        const auto et = EffectiveType(n.type);
        const auto &t = Types()[et];
        Monstey_CheckRange(data, off, t.bytes);
        if (n.original_storage.size() == t.bytes)
          std::copy(n.original_storage.begin(), n.original_storage.end(),
                    data.begin() + static_cast<std::ptrdiff_t>(off));
        const auto k = t.subtype & 0xff;
        if (k == 2 || k == 4) {
          const auto width = Monstey_IntBytes(t.subtype);
          const auto v = k == 2 ? std::uint64_t(n.scalar.as_bool())
                         : (t.subtype & 0x200)
                             ? std::bit_cast<std::uint64_t>(n.scalar.as_int())
                             : Monstey_HavokUnsigned(n.scalar);
          Monstey_PutInteger(data, off, width, v);
        } else if (k == 5) {
          const auto bits = n.scalar.is_object()
                                ? static_cast<std::uint32_t>(
                                      n.scalar.at("float32_bits").as_uint())
                                : std::bit_cast<std::uint32_t>(
                                      static_cast<float>(n.scalar.as_number()));
          if (t.bytes == 2) {
            Monstey_Require((bits & 0xffff) == 0, "Lossy hkHalf16 prohibited");
            Monstey_PutU16(data, off, static_cast<std::uint16_t>(bits >> 16));
          } else
            Monstey_PutU32(data, off, bits);
        } else if (k == 3 || k == 6 || k == 8) {
          const auto item = index(id, false);
          Monstey_PutU32(data, off, item);
          if (item) {
            Monstey_Require(off <= 0xffffffff, "PTCH offset overflow");
            patches[et].insert(static_cast<std::uint32_t>(off));
          }
        } else if (k == 7) {
          const auto ms = Members(et);
          Monstey_Require(n.fields.size() == ms.size(),
                          "Class count differs from TYPE");
          for (const auto &m : ms) {
            const auto child = Field(id, m.name);
            Monstey_Require(Node(child).type == m.type,
                            "Class child differs from TYPE");
            write(child, off + m.offset, depth + 1);
          }
        } else if (k == 0x28) {
          Monstey_Require(n.elements.size() == t.subtype >> 8,
                          "Tuple length differs from TYPE");
          const auto stride = Types()[EffectiveType(t.pointer)].bytes;
          for (std::size_t i = 0; i < n.elements.size(); ++i) {
            Monstey_Require(Node(n.elements[i]).type == t.pointer,
                            "Tuple type differs from TYPE");
            write(n.elements[i], off + i * stride, depth + 1);
          }
        } else
          Monstey_Require(n.original_storage.size() == t.bytes,
                          "Unknown subtype lacks preserved storage");
      };
  for (std::size_t i = 1; i < items.size(); ++i) {
    const auto itemType = items[i].type;
    const auto &t = Types()[EffectiveType(itemType)];
    const auto alignment = std::max(2u, t.alignment);
    Monstey_Require((alignment & (alignment - 1)) == 0,
                    "Invalid item alignment");
    while (data.size() % alignment)
      data.push_back(0);
    const auto off = data.size();
    items[i].offset = off;
    const auto ids = items[i].ids;
    const auto text = items[i].text;
    const auto count = text.empty() ? ids.size() : text.size();
    Monstey_Require(t.bytes > 0 &&
                        count <= (Monstey_MaxData - data.size()) / t.bytes,
                    "DATA size limit exceeded");
    data.resize(data.size() + t.bytes * count);
    if (!text.empty()) {
      Monstey_Require(t.bytes == 1, "Native char is not one byte");
      std::copy(text.begin(), text.end(),
                data.begin() + static_cast<std::ptrdiff_t>(off));
    } else
      for (std::size_t j = 0; j < ids.size(); ++j) {
        Monstey_Require(EffectiveType(Node(ids[j]).type) ==
                            EffectiveType(itemType),
                        "Array type differs from storage");
        write(ids[j], off + j * t.bytes, 0);
      }
  }
  while (data.size() % 16)
    data.push_back(0);
  B ib(12), pb;
  for (std::size_t i = 1; i < items.size(); ++i) {
    const auto &item = items[i];
    Monstey_AppendU32(ib, item.type | (item.pointer ? 0x10000000 : 0x20000000));
    Monstey_AppendU32(ib, static_cast<std::uint32_t>(item.offset));
    Monstey_AppendU32(ib, static_cast<std::uint32_t>(item.text.empty()
                                                         ? item.ids.size()
                                                         : item.text.size()));
  }
  for (const auto &[type, offs] : patches) {
    Monstey_AppendU32(pb, type);
    Monstey_AppendU32(pb, static_cast<std::uint32_t>(offs.size()));
    for (const auto off : offs)
      Monstey_AppendU32(pb, off);
  }
  const auto newIndex = Monstey_Recontainer(
      Monstey_Section(impl_->sections, "INDX").raw, impl_->indexSections,
      {{"ITEM", Monstey_MakeSection("ITEM", std::move(ib))},
       {"PTCH", Monstey_MakeSection("PTCH", std::move(pb))}});
  auto encoded = Monstey_Recontainer(
      Original(), impl_->sections,
      {{"DATA", Monstey_MakeSection("DATA", std::move(data))},
       {"INDX", newIndex}});
  Monstey_Havok parsed(encoded);
  Monstey_Require(parsed.Canonical() == Canonical(),
                  "Native semantic/alias readback mismatch");
  Monstey_Require(parsed.TypeHashCoverage().at("status").as_string() == "PASS",
                  "TYPE lacks active THSH hashes; supply exact-layout donor");
  return encoded;
}
B Monstey_Havok::CompleteTypeHashes(std::span<const std::uint8_t> source,
                                    std::span<const std::uint8_t> donor) {
  Monstey_Havok src(source), dst(donor);
  const auto a = Monstey_Sections(source, "TAG0"),
             b = Monstey_Sections(donor, "TAG0");
  Monstey_Require(Monstey_Section(a, "SDKV").raw ==
                      Monstey_Section(b, "SDKV").raw,
                  "THSH donor SDK differs");
  const auto ta = Monstey_Sections(Monstey_Section(a, "TYPE").raw, "TYPE"),
             tb = Monstey_Sections(Monstey_Section(b, "TYPE").raw, "TYPE");
  Monstey_Require(ta.size() == tb.size() &&
                      src.Types().size() == dst.Types().size(),
                  "THSH donor dictionary differs");
  for (const auto &s : ta)
    if (s.name != "THSH")
      Monstey_Require(s.raw == Monstey_Section(tb, s.name).raw,
                      "THSH donor layout differs");
  for (std::size_t i = 1; i < src.Types().size(); ++i)
    if (src.Types()[i].hash)
      Monstey_Require(src.Types()[i].hash == dst.Types()[i].hash,
                      "THSH donor changes existing hash");
  const auto nt =
      Monstey_Recontainer(Monstey_Section(a, "TYPE").raw, ta,
                          {{"THSH", Monstey_Section(tb, "THSH").raw}});
  auto result =
      Monstey_Recontainer(B(source.begin(), source.end()), a, {{"TYPE", nt}});
  Monstey_Havok check(result);
  Monstey_Require(check.TypeHashCoverage().at("status").as_string() == "PASS",
                  "Donor still lacks active hashes");
  return result;
}
} // namespace Monstey
