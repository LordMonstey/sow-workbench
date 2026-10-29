#include "Monstey_FearDbPacked.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using Monstey_Word = std::uint32_t;
std::uint64_t Monstey_Groups = 0, Monstey_Cases = 0, Monstey_Checks = 0,
              Monstey_Bytes = 0, Monstey_WrittenBytes = 0, Monstey_Faults = 0;
void Monstey_Need(bool value, const char *label) {
  ++Monstey_Checks;
  if (!value)
    throw std::runtime_error(label);
}
struct Monstey_Attribute {
  Monstey_Word name = 0, type = 0, usage = 0;
  std::vector<Monstey_Word> words;
};
struct Monstey_Record {
  Monstey_Word name = 0;
  std::vector<Monstey_Attribute> attributes;
};
struct Monstey_Category {
  Monstey_Word name = 0;
  std::vector<Monstey_Record> records;
};
struct Monstey_Model {
  std::vector<std::uint8_t> pool{0x61, 0, 0xff, 0x71, 0};
  std::vector<Monstey_Category> categories;
  std::optional<std::array<Monstey_Word, 4>> advertised;
  std::array<Monstey_Word, 4> Totals() const {
    std::array<Monstey_Word, 4> totals{
        static_cast<Monstey_Word>(categories.size()), 0, 0, 0};
    for (const auto &c : categories) {
      totals[1] += static_cast<Monstey_Word>(c.records.size());
      for (const auto &r : c.records) {
        totals[2] += static_cast<Monstey_Word>(r.attributes.size());
        for (const auto &a : r.attributes)
          totals[3] += static_cast<Monstey_Word>(a.words.size());
      }
    }
    return totals;
  }
  std::vector<std::uint8_t> Wire() const {
    std::vector<std::uint8_t> out;
    auto add = [&](Monstey_Word x) {
      for (unsigned i = 0; i < 4; ++i)
        out.push_back(static_cast<std::uint8_t>(x >> (8 * i)));
    };
    add(0x42444147);
    add(3);
    add(static_cast<Monstey_Word>(pool.size()));
    const auto totals = advertised.value_or(Totals());
    for (auto n : totals)
      add(n);
    out.insert(out.end(), pool.begin(), pool.end());
    add(static_cast<Monstey_Word>(categories.size()));
    for (const auto &c : categories) {
      add(c.name);
      add(static_cast<Monstey_Word>(c.records.size()));
      for (const auto &r : c.records) {
        add(r.name);
        add(static_cast<Monstey_Word>(r.attributes.size()));
        for (const auto &a : r.attributes) {
          add(a.name);
          add(a.type);
          add(a.usage);
          add(static_cast<Monstey_Word>(a.words.size()));
          for (auto w : a.words)
            add(w);
        }
      }
    }
    return out;
  }
  std::vector<std::size_t> Requests() const {
    std::vector<std::size_t> out{28, pool.size(), 4};
    for (const auto &c : categories) {
      out.push_back(8);
      for (const auto &r : c.records) {
        out.push_back(8);
        for (const auto &a : r.attributes) {
          out.push_back(16);
          for (std::size_t i = 0; i < a.words.size(); ++i)
            out.push_back(4);
        }
      }
    }
    return out;
  }
};
struct Monstey_Expected {
  std::vector<std::uint8_t> bytes, written;
  std::uint32_t size = 0, used = 0, database = 0;
  std::vector<std::uint32_t> categories;
  std::vector<std::vector<std::uint32_t>> records;
  std::vector<std::vector<std::vector<std::uint32_t>>> attributes, values;
};
std::size_t Monstey_NameLength(std::span<const std::uint8_t> name) {
  const auto found = std::find(name.begin(), name.end(), std::uint8_t{0});
  if (found == name.end())
    throw std::runtime_error("reference name requires a terminator");
  return static_cast<std::size_t>(found - name.begin()) + 1;
}
Monstey_Expected Monstey_Reference(const Monstey_Model &m,
                                   std::span<const std::uint8_t> name,
                                   Monstey_Word base) {
  Monstey_Expected e;
  const auto totals = m.advertised.value_or(m.Totals());
  const auto length = Monstey_NameLength(name);
  // Independent wide closed form, reduced modulo32 only at the end.
  const auto wide = std::uint64_t(m.pool.size()) + 20 +
                    16 * (std::uint64_t(totals[0]) + totals[1] + totals[2]) +
                    4 * std::uint64_t(totals[3]) + length;
  e.size = static_cast<Monstey_Word>(wide);
  e.bytes.resize(e.size);
  e.written.resize(e.size);
  e.database = base + static_cast<Monstey_Word>(m.pool.size());
  std::uint32_t offset =
      static_cast<std::uint32_t>(m.pool.size() + 20 + length);
  const auto cat_offset = offset;
  offset += static_cast<Monstey_Word>(16 * m.categories.size());
  e.categories.resize(m.categories.size());
  e.records.resize(m.categories.size());
  e.attributes.resize(m.categories.size());
  e.values.resize(m.categories.size());
  for (std::size_t c = 0; c < m.categories.size(); ++c) {
    e.categories[c] = base + cat_offset + static_cast<Monstey_Word>(16 * c);
    const auto &category = m.categories[c];
    e.records[c].resize(category.records.size());
    e.attributes[c].resize(category.records.size());
    e.values[c].resize(category.records.size());
    const auto rec_offset = offset;
    offset += static_cast<Monstey_Word>(16 * category.records.size());
    for (std::size_t r = 0; r < category.records.size(); ++r) {
      e.records[c][r] = base + rec_offset + static_cast<Monstey_Word>(16 * r);
      const auto &record = category.records[r];
      e.attributes[c][r].resize(record.attributes.size());
      e.values[c][r].resize(record.attributes.size());
      const auto attr_offset = offset;
      offset += static_cast<Monstey_Word>(16 * record.attributes.size());
      for (std::size_t a = 0; a < record.attributes.size(); ++a) {
        e.attributes[c][r][a] =
            base + attr_offset + static_cast<Monstey_Word>(16 * a);
        e.values[c][r][a] = base + offset;
        offset +=
            static_cast<Monstey_Word>(4 * record.attributes[a].words.size());
      }
    }
  }
  e.used = offset;
  auto put = [&](Monstey_Word address, Monstey_Word value, unsigned width) {
    const auto at = static_cast<Monstey_Word>(address - base);
    if (at > e.bytes.size() || width > e.bytes.size() - at)
      throw std::runtime_error("independent valid fixture underallocated");
    for (unsigned i = 0; i < width; ++i) {
      e.bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
      e.written[at + i] = 1;
    }
  };
  for (std::size_t i = 0; i < m.pool.size(); ++i) {
    e.bytes[i] = m.pool[i];
    e.written[i] = 1;
  }
  for (std::size_t i = 0; i < length; ++i)
    put(e.database + 20 + static_cast<Monstey_Word>(i), name[i], 1);
  put(e.database, base, 4);
  put(e.database + 4, e.database + 20, 4);
  put(e.database + 8, static_cast<Monstey_Word>(m.categories.size()), 4);
  put(e.database + 12, base + cat_offset, 4);
  put(e.database + 16, 0, 4);
  auto node = [&](Monstey_Word address, Monstey_Word text, Monstey_Word parent,
                  Monstey_Word count, Monstey_Word first) {
    put(address, text, 4);
    put(address + 4, parent, 4);
    put(address + 8, count, 4);
    put(address + 12, first, 4);
  };
  for (std::size_t c = 0; c < m.categories.size(); ++c) {
    const auto &category = m.categories[c];
    const auto ch = e.categories[c];
    // The empty-array identity is still its reserved current cursor. Derive it
    // from preceding model extents, independently of production image reads.
    std::uint32_t cursor = static_cast<Monstey_Word>(
        m.pool.size() + 20 + length + 16 * m.categories.size());
    for (std::size_t k = 0; k < c; ++k) {
      const auto &prior = m.categories[k];
      cursor += static_cast<Monstey_Word>(16 * prior.records.size());
      for (const auto &r : prior.records) {
        cursor += static_cast<Monstey_Word>(16 * r.attributes.size());
        for (const auto &a : r.attributes)
          cursor += static_cast<Monstey_Word>(4 * a.words.size());
      }
    }
    const auto first = base + cursor;
    node(ch, base + category.name, e.database,
         static_cast<Monstey_Word>(category.records.size()), first);
    cursor += static_cast<Monstey_Word>(16 * category.records.size());
    for (std::size_t r = 0; r < category.records.size(); ++r) {
      const auto &record = category.records[r];
      const auto rh = e.records[c][r];
      node(rh, base + record.name, ch,
           static_cast<Monstey_Word>(record.attributes.size()), base + cursor);
      cursor += static_cast<Monstey_Word>(16 * record.attributes.size());
      for (std::size_t a = 0; a < record.attributes.size(); ++a) {
        const auto &attribute = record.attributes[a];
        const auto ah = e.attributes[c][r][a], vh = e.values[c][r][a];
        put(ah, base + attribute.name, 4);
        put(ah + 4, rh, 4);
        put(ah + 8, static_cast<std::uint16_t>(attribute.words.size()), 2);
        put(ah + 10, static_cast<std::uint8_t>(attribute.type), 1);
        put(ah + 11, static_cast<std::uint8_t>(attribute.usage), 1);
        put(ah + 12, vh, 4);
        for (std::size_t i = 0; i < attribute.words.size(); ++i) {
          auto word = attribute.words[i];
          switch (attribute.type) {
          case 4:
          case 5:
          case 6:
          case 7:
          case 8:
            word += base;
            break;
          default:
            break;
          }
          if (static_cast<std::uint8_t>(attribute.type) == 9 &&
              i < static_cast<std::uint16_t>(attribute.words.size())) {
            const auto ci = word / 65536u, ri = word % 65536u;
            word = ci < e.records.size() && ri < e.records[ci].size()
                       ? e.records[ci][ri]
                       : 0;
          }
          put(vh + static_cast<Monstey_Word>(4 * i), word, 4);
        }
        cursor += static_cast<Monstey_Word>(4 * attribute.words.size());
      }
    }
  }
  return e;
}
const std::array<std::uint8_t, 7> Monstey_Name{'n', 'a',  'm', 'e',
                                               0,   0xff, 0x11};
void Monstey_CheckModel(const Monstey_Model &m,
                        std::span<const std::uint8_t> name = Monstey_Name,
                        Monstey_Word base = 0x30000001, std::size_t prefix = 0,
                        std::uint64_t initial_calls = 0) {
  ++Monstey_Cases;
  auto wire = m.Wire();
  const auto payload = wire.size();
  wire.insert(wire.begin(), prefix, 0x5a);
  wire.insert(wire.end(), {0xa1, 0xb2, 0xc3});
  const auto before = wire;
  const auto expected = Monstey_Reference(m, name, base);
  Monstey_FearDbPackedInput input{wire, prefix, initial_calls};
  const auto loaded = Monstey_FearDbLoadPacked(input, name, base);
  Monstey_Need(loaded.status == Monstey_FearDbPackedStatus::loaded &&
                   loaded.image.has_value(),
               "valid owned native-layout load");
  Monstey_Need(loaded.allocation_bytes == expected.size &&
                   loaded.image->allocation.size() == expected.size,
               "wide modulo32 allocation equation");
  Monstey_Need(loaded.image->base == base &&
                   loaded.image->database == expected.database &&
                   loaded.image->used_bytes == expected.used,
               "native32 identities and traversal final cursor");
  Monstey_Need(
      input.position == prefix + payload &&
          input.read_calls == initial_calls + m.Requests().size(),
      "exact span read boundaries, zero-size calls and trailing bytes");
  Monstey_Need(wire == before, "borrowed serialized bytes unchanged");
  Monstey_Need(loaded.image->allocation == expected.bytes,
               "independent complete layout and owned zero-placeholder bytes");
  Monstey_Bytes += expected.bytes.size();
  bool native_equal = true;
  for (std::size_t i = 0; i < expected.written.size(); ++i)
    if (expected.written[i]) {
      ++Monstey_WrittenBytes;
      native_equal =
          native_equal && loaded.image->allocation[i] == expected.bytes[i];
    }
  Monstey_Need(
      native_equal,
      "native written-byte projection independent of allocator placeholders");
  Monstey_Need(Monstey_FearDbChildCount(loaded.image->Memory(),
                                        loaded.image->database) ==
                   m.categories.size(),
               "accepted primitive reads new root graph");
}
void Monstey_EmptyAndNames() {
  ++Monstey_Groups;
  for (std::size_t pool = 0; pool < 17; ++pool)
    for (std::size_t len = 0; len < 9; ++len) {
      Monstey_Model m;
      m.pool.resize(pool, 0xa5);
      std::vector<std::uint8_t> name(len + 4, 0x73);
      name[len] = 0;
      Monstey_CheckModel(m, name, 0x10000001, 1 + (len % 4));
    }
  Monstey_Model m;
  m.pool = {0, 1, 2};
  Monstey_CheckModel(m, Monstey_Name, 0x20000001, 3, UINT64_MAX - 1);
}
void Monstey_AllFullTypes() {
  ++Monstey_Groups;
  std::vector<Monstey_Word> types;
  for (Monstey_Word t = 0; t < 768; ++t)
    types.push_back(t);
  for (auto t : std::array<Monstey_Word, 8>{0x80000004, 0xffffff08, 0x7fffffff,
                                            0xffffffff, 0x10004, 0x80000109,
                                            0x10000009, 0x40000004})
    types.push_back(t);
  for (auto t : types) {
    Monstey_Model m;
    m.categories = {
        Monstey_Category{
            0xffffffff,
            {Monstey_Record{0x80000000,
                            {Monstey_Attribute{
                                0xfffffffe,
                                t,
                                0xfedcba98,
                                {0, 1, 0xffffffff, 0x80000000, 0x00010000}}}}}},
        Monstey_Category{2, {Monstey_Record{0, {}}}}};
    Monstey_CheckModel(m);
  }
}
void Monstey_PhysicalCounts() {
  ++Monstey_Groups;
  for (auto count : std::array<Monstey_Word, 4>{65535, 65536, 65537, 131073})
    for (auto type : std::array<Monstey_Word, 3>{3, 9, 0x109}) {
      Monstey_Model m;
      Monstey_Attribute a{0, type, 0xabcd1234, {}};
      a.words.resize(count);
      for (Monstey_Word i = 0; i < count; ++i)
        a.words[i] = std::array<Monstey_Word, 5>{0, 0x10000, 0xffff, 0xffffffff,
                                                 0x10001}[i % 5];
      m.categories = {
          Monstey_Category{0, {Monstey_Record{1, {std::move(a)}}}},
          Monstey_Category{2, {Monstey_Record{3, {}}, Monstey_Record{4, {}}}}};
      Monstey_CheckModel(m);
    }
}
void Monstey_RandomModels() {
  ++Monstey_Groups;
  std::mt19937 rng(0x5041434b);
  constexpr std::array<Monstey_Word, 16> types = {
      0, 1, 2,  3,     4,     5,          6,          7,
      8, 9, 10, 0x109, 0x104, 0xffffff04, 0x80000009, 0xffffffff};
  for (unsigned trial = 0; trial < 1024; ++trial) {
    Monstey_Model m;
    m.pool.resize(rng() % 21);
    for (auto &b : m.pool)
      b = static_cast<std::uint8_t>(rng());
    m.categories.resize(rng() % 5);
    for (auto &c : m.categories) {
      c.name = rng();
      c.records.resize(rng() % 4);
      for (auto &r : c.records) {
        r.name = rng();
        r.attributes.resize(rng() % 6);
        for (auto &a : r.attributes) {
          a.name = rng();
          a.type = types[rng() % types.size()];
          a.usage = rng();
          a.words.resize(rng() % 9);
          for (auto &w : a.words)
            w = (rng() % 3) == 0 ? rng() : ((rng() % 6) << 16) | (rng() % 6);
        }
      }
    }
    if ((trial % 4) == 0) {
      auto totals = m.Totals();
      totals[trial % 3] += 1;
      totals[3] += 3;
      m.advertised = totals;
    }
    Monstey_CheckModel(m, Monstey_Name, 0x01020300 + trial * 0x10000,
                       trial % 5);
  }
}
void Monstey_Cuts() {
  ++Monstey_Groups;
  for (std::size_t pool : {0u, 1u, 7u}) {
    Monstey_Model m;
    m.pool.resize(pool, 0x5c);
    m.categories = {Monstey_Category{
        0,
        {Monstey_Record{0,
                        {Monstey_Attribute{0, 4, 0, {0, 1, 2}},
                         Monstey_Attribute{0, 9, 0, {0, 0xffffffff}}}}}}};
    const auto raw = m.Wire();
    const auto sizes = m.Requests();
    for (std::size_t cut = 0; cut < raw.size(); ++cut) {
      ++Monstey_Cases;
      auto wire = raw;
      wire.insert(wire.begin(), 3, 0xcc);
      Monstey_FearDbPackedInput input{std::span(wire).first(3 + cut), 3, 17};
      const auto loaded =
          Monstey_FearDbLoadPacked(input, Monstey_Name, 0x20000000);
      std::size_t consumed = 0, request = 0;
      while (request < sizes.size() && sizes[request] <= cut - consumed) {
        consumed += sizes[request];
        ++request;
      }
      const auto state =
          request == 0   ? Monstey_FearDbPackedStatus::header_read_failed
          : request == 1 ? Monstey_FearDbPackedStatus::pool_read_failed
                         : Monstey_FearDbPackedStatus::graph_read_failed;
      Monstey_Need(
          loaded.status == state && !loaded.image,
          "first failed complete Read determines stage and drops allocation");
      Monstey_Need(input.position == 3 + consumed &&
                       input.read_calls == 18 + request,
                   "short-read cursor and exact request prefix");
      Monstey_Need(
          loaded.allocation_bytes ==
              (request == 0
                   ? 0
                   : Monstey_Reference(m, Monstey_Name, 0x20000000).size),
          "allocation result only after successful header validation");
    }
  }
}
void Monstey_HeadersAndAdapter() {
  ++Monstey_Groups;
  Monstey_Model m;
  const auto good = m.Wire();
  for (unsigned field = 0; field < 2; ++field)
    for (unsigned bit = 0; bit < 32; ++bit) {
      ++Monstey_Cases;
      auto bytes = good;
      bytes[field * 4 + bit / 8] ^= static_cast<std::uint8_t>(1u << (bit % 8));
      Monstey_FearDbPackedInput in{bytes};
      const auto r = Monstey_FearDbLoadPacked(in, {}, 0);
      Monstey_Need(r.status == Monstey_FearDbPackedStatus::invalid_header &&
                       !r.image && r.allocation_bytes == 0,
                   "invalid header precedes name/base consumption");
      Monstey_Need(in.position == 28 && in.read_calls == 1,
                   "invalid header single read");
    }
  ++Monstey_Cases;
  Monstey_FearDbPackedInput badpos{good, good.size() + 1, UINT64_MAX};
  const auto r = Monstey_FearDbLoadPacked(badpos, {}, 0);
  Monstey_Need(r.status == Monstey_FearDbPackedStatus::header_read_failed &&
                   badpos.position == good.size() + 1 && badpos.read_calls == 0,
               "out-of-span cursor unchanged and diagnostic count wraps");
  for (unsigned kind = 0; kind < 3; ++kind) {
    ++Monstey_Cases;
    Monstey_FearDbPackedInput in{good};
    bool fail = false;
    try {
      if (kind == 0)
        (void)Monstey_FearDbLoadPacked(in, std::array<std::uint8_t, 3>{1, 2, 3},
                                       0x1000);
      else
        (void)Monstey_FearDbLoadPacked(in, Monstey_Name,
                                       kind == 1 ? 0 : 0xfffffff0);
    } catch (const std::invalid_argument &) {
      fail = true;
    }
    Monstey_Need(fail && in.position == 28 && in.read_calls == 1,
                 "owned name/base precondition order after header before pool");
  }
}
void Monstey_ModuloBudgets() {
  ++Monstey_Groups;
  for (auto nodes :
       std::array<Monstey_Word, 4>{0, 0x10000000, 0x80000000, 0xf0000000})
    for (auto values :
         std::array<Monstey_Word, 4>{0, 0x40000000, 0x80000000, 0xc0000000})
      for (std::size_t pool : {0u, 3u}) {
        Monstey_Model m;
        m.pool.resize(pool, 0x93);
        m.advertised = std::array<Monstey_Word, 4>{nodes, 0, 0, values};
        const std::array<std::uint8_t, 1> empty{0};
        Monstey_CheckModel(
            m, empty, static_cast<Monstey_Word>(0x100000000ull - (pool + 21)));
      }
}
void Monstey_SourceNameAlias() {
  ++Monstey_Groups;
  Monstey_Model m;
  m.pool = {0x41, 0x42, 0, 0x90, 0x91};
  auto bytes = m.Wire();
  const auto snapshot = bytes;
  auto name = std::span<const std::uint8_t>(bytes).subspan(28, 5);
  const auto expected = Monstey_Reference(m, name, 0x76543210);
  Monstey_FearDbPackedInput input{bytes};
  ++Monstey_Cases;
  const auto result = Monstey_FearDbLoadPacked(input, name, 0x76543210);
  Monstey_Need(result.image && result.image->allocation == expected.bytes,
               "caller-name span may borrow immutable pool input bytes");
  Monstey_Need(bytes == snapshot, "source-name alias is not mutated");
}
void Monstey_ConsumedFaultOrder() {
  ++Monstey_Groups;
  for (unsigned depth = 0; depth < 4; ++depth) {
    Monstey_Model m;
    m.pool.clear();
    m.categories = {Monstey_Category{0, {}}};
    if (depth >= 1)
      m.categories[0].records.push_back(Monstey_Record{0, {}});
    if (depth >= 2)
      m.categories[0].records[0].attributes.push_back(
          Monstey_Attribute{0, 3, 0, {}});
    if (depth >= 3)
      m.categories[0].records[0].attributes[0].words.push_back(0x12345678);
    m.advertised = std::array<Monstey_Word, 4>{0, 0, 0, 0};
    const std::array<std::uint8_t, 1> empty{0};
    auto wire = m.Wire();
    Monstey_FearDbPackedInput in{wire};
    ++Monstey_Cases;
    ++Monstey_Faults;
    bool seen = false;
    try {
      (void)Monstey_FearDbLoadPacked(in, empty, 0x10000000);
    } catch (const Monstey_FearDbFault &e) {
      seen = true;
      Monstey_Need(e.writing && e.width == 4 &&
                       e.address == 0x10000015 + depth * 16,
                   "first leaf write consumes exact unavailable identity after "
                   "child reads");
    }
    Monstey_Need(
        seen && in.position == wire.size() &&
            in.read_calls == m.Requests().size(),
        "node initialization remains after successful descendant reads");
  }
  Monstey_Model m;
  m.pool.clear();
  m.advertised = std::array<Monstey_Word, 4>{0, 0, 0, 0x3ffffffa};
  auto bytes = m.Wire();
  Monstey_FearDbPackedInput in{bytes};
  const std::array<std::uint8_t, 4> name{'a', 'b', 'c', 0};
  ++Monstey_Cases;
  ++Monstey_Faults;
  bool seen = false;
  try {
    (void)Monstey_FearDbLoadPacked(in, name, 0x10000000);
  } catch (const Monstey_FearDbFault &e) {
    seen = true;
    Monstey_Need(e.writing && e.width == 1 && e.address == 0x10000014,
                 "wrapped zero budget consumes caller-name store first");
  }
  Monstey_Need(seen && in.position == 32 && in.read_calls == 3,
               "zero budget does not fabricate early header rejection");
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Usage: FEAR_PACKED_INDEPENDENT NEW_JSON");
    Monstey_EmptyAndNames();
    Monstey_AllFullTypes();
    Monstey_PhysicalCounts();
    Monstey_RandomModels();
    Monstey_Cuts();
    Monstey_HeadersAndAdapter();
    Monstey_ModuloBudgets();
    Monstey_SourceNameAlias();
    Monstey_ConsumedFaultOrder();
    const auto text =
        "{\"status\":\"PASS\",\"groups\":" + std::to_string(Monstey_Groups) +
        ",\"cases\":" + std::to_string(Monstey_Cases) +
        ",\"checks\":" + std::to_string(Monstey_Checks) +
        ",\"owned_bytes_compared\":" + std::to_string(Monstey_Bytes) +
        ",\"native_written_bytes_compared\":" +
        std::to_string(Monstey_WrittenBytes) +
        ",\"consumed_fault_cases\":" + std::to_string(Monstey_Faults) + "}\n";
    std::ofstream out(argv[1], std::ios::binary);
    out << text;
    if (!out)
      throw std::runtime_error("write independent report");
    std::cout << text;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}