#include "Monstey_Havok.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
using namespace Monstey;
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
int passed = 0;
void Check(bool ok, std::string_view what) {
  Monstey_Require(ok, what);
  ++passed;
  std::cout << "PASS " << what << '\n';
}
void Reject(const std::function<void()> &fn, std::string_view what) {
  try {
    fn();
  } catch (const std::exception &) {
    Check(true, what);
    return;
  }
  throw std::runtime_error("Expected rejection: " + std::string(what));
}
void BE(B &b, std::uint32_t n) {
  b.push_back(std::uint8_t(n >> 24));
  b.push_back(std::uint8_t(n >> 16));
  b.push_back(std::uint8_t(n >> 8));
  b.push_back(std::uint8_t(n));
}
void Pack(B &b, std::uint32_t n) {
  if (n < 0x80)
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
B Sec(std::string_view n, B p, bool flag = true) {
  while (p.size() % 4)
    p.push_back(0);
  B b;
  BE(b, static_cast<std::uint32_t>(p.size() + 8) | (flag ? 0x40000000u : 0));
  b.insert(b.end(), n.begin(), n.end());
  b.insert(b.end(), p.begin(), p.end());
  return b;
}
void Add(B &b, const B &x) { b.insert(b.end(), x.begin(), x.end()); }
B Text(const std::vector<std::string> &s) {
  B b;
  for (const auto &x : s) {
    b.insert(b.end(), x.begin(), x.end());
    b.push_back(0);
  }
  return b;
}
B Fixture(bool missingHash = false, bool cycle = false) {
  std::vector<std::string> names{
      "char",       "int32",       "float",       "vector4", "rootPtr",
      "floatArray", "hkStringPtr", "Root",        "bool",    "hkHalf16",
      "uint64",     "Opaque",      "rootPtrArray"};
  B tn;
  Pack(tn, 14);
  for (std::uint32_t i = 0; i < 13; ++i) {
    Pack(tn, i);
    Pack(tn, 0);
  }
  B tb;
  auto typ = [&](std::uint32_t i, std::uint32_t subtype, std::uint32_t bytes,
                 std::uint32_t align, std::uint32_t ptr = 0) {
    Pack(tb, i);
    Pack(tb, 0);
    Pack(tb, 9 | (ptr ? 2 : 0));
    Pack(tb, subtype);
    if (ptr)
      Pack(tb, ptr);
    Pack(tb, bytes);
    Pack(tb, align);
  };
  typ(1, 0x2004, 1, 1);
  typ(2, 0x8204, 4, 4);
  typ(3, 5, 4, 4);
  typ(4, 0x428, 16, 16, 3);
  typ(5, 6, 8, 8, 8);
  typ(6, 8, 16, 8, 3);
  typ(7, 3, 8, 8);
  Pack(tb, 8);
  Pack(tb, 0);
  Pack(tb, 41);
  Pack(tb, 7);
  Pack(tb, 112);
  Pack(tb, 16);
  Pack(tb, 10);
  const std::uint32_t offsets[]{0, 16, 32, 40, 56, 64, 66, 72, 80, 88},
      types[]{2, 4, 5, 6, 7, 9, 10, 11, 12, 13};
  for (std::uint32_t i = 0; i < 10; ++i) {
    Pack(tb, i);
    Pack(tb, 0);
    Pack(tb, offsets[i]);
    Pack(tb, types[i]);
  }
  typ(9, 0x2002, 1, 1);
  typ(10, 5, 2, 2);
  typ(11, 0x10004, 8, 8);
  typ(12, 0x99, 4, 4);
  typ(13, 8, 16, 8, 5);
  B th;
  Pack(th, missingHash ? 12 : 13);
  for (std::uint32_t i = 1; i <= 13; ++i)
    if (!(missingHash && i == 5)) {
      Pack(th, i);
      Monstey_AppendU32(th, 0x12340000 + i);
    }
  B type;
  Add(type, Sec("TPTR", {}));
  Add(type, Sec("TSTR", Text(names)));
  Add(type, Sec("TNA1", tn));
  Add(type, Sec("FSTR", Text({"x", "v", "p", "a", "s", "b", "h", "u", "opaque",
                              "ptrs"})));
  Add(type, Sec("TBOD", tb));
  Add(type, Sec("THSH", th));
  Add(type, Sec("TPAD", {}));
  B data(272, 0);
  Monstey_PutU32(data, 0, 17);
  for (std::size_t i = 0; i < 4; ++i)
    Monstey_PutU32(data, 16 + i * 4,
                   std::bit_cast<std::uint32_t>(float(i + 1)));
  Monstey_PutU32(data, 32, 5);
  Monstey_PutU32(data, 40, 2);
  Monstey_PutU32(data, 56, 3);
  data[64] = 1;
  Monstey_PutU16(data, 66, 0x3f80);
  Monstey_PutU64(data, 72, 0xf123456789abcdefull);
  Monstey_PutU32(data, 80, 0xaabbccdd);
  Monstey_PutU32(data, 88, 4);
  for (std::size_t i = 0; i < 4; ++i)
    Monstey_PutU32(data, 112 + i * 4,
                   std::bit_cast<std::uint32_t>(float(i + 10)));
  const std::string str = "hello";
  std::copy(str.begin(), str.end(), data.begin() + 128);
  Monstey_PutU32(data, 144, 5);
  Monstey_PutU32(data, 152, 5);
  Monstey_PutU32(data, 160, 42);
  if (cycle)
    Monstey_PutU32(data, 192, 1);
  B item(12);
  auto it = [&](std::uint32_t t, std::uint32_t off, std::uint32_t count,
                bool ptr) {
    Monstey_AppendU32(item, t | (ptr ? 0x10000000u : 0x20000000u));
    Monstey_AppendU32(item, off);
    Monstey_AppendU32(item, count);
  };
  it(8, 0, 1, true);
  it(3, 112, 4, false);
  it(1, 128, 6, false);
  it(5, 144, 2, false);
  it(8, 160, 1, true);
  B patch;
  auto pt = [&](std::uint32_t ti, std::initializer_list<std::uint32_t> off) {
    Monstey_AppendU32(patch, ti);
    Monstey_AppendU32(patch, static_cast<std::uint32_t>(off.size()));
    for (auto x : off)
      Monstey_AppendU32(patch, x);
  };
  if (cycle)
    pt(5, {32, 144, 152, 192});
  else
    pt(5, {32, 144, 152});
  pt(6, {40});
  pt(7, {56});
  pt(13, {88});
  B ind;
  Add(ind, Sec("ITEM", item));
  Add(ind, Sec("PTCH", patch));
  Add(ind, Sec("TEST", {1, 2, 3, 4}));
  B root;
  Add(root, Sec("SDKV", B{'2', '0', '1', '6', '0', '1', '0', '0'}));
  Add(root, Sec("DATA", data));
  Add(root, Sec("TYPE", type, false));
  Add(root, Sec("EXTR", {9, 8, 7, 6}, false));
  Add(root, Sec("INDX", ind, false));
  return Sec("TAG0", root, false);
}
void Synthetic(const std::filesystem::path &out) {
  const auto raw = Fixture();
  Monstey_Havok g(raw);
  Check(g.Encode() == raw, "No-op source byte exact");
  Check(g.Plain(g.Root()).at("x").as_int() == 17, "Typed scalar decode");
  Check(g.Plain(g.Field(g.Root(), "u")).as_string() == "0xf123456789abcdef",
        "Unsigned 64-bit exact");
  Check(g.Plain(g.Field(g.Root(), "h")).as_number() == 1,
        "Havok high-word half decoding");
  const auto ptr = g.Field(g.Root(), "p"), array = g.Field(g.Root(), "ptrs");
  Check(g.Node(g.Node(array).elements[0]).target == g.Node(ptr).target &&
            g.Node(g.Node(array).elements[1]).target == g.Node(ptr).target,
        "Shared object identity through pointer arrays");
  Check(g.Audit().at("status").as_string() == "PASS",
        "ITEM/PTCH and THSH validation");
  const auto child = g.Dereference(ptr);
  g.Node(g.Field(child, "a")).elements =
      g.Node(g.Field(g.Root(), "a")).elements;
  const auto shared = g.Encode(true);
  Check(Monstey_Havok(shared).Canonical() == g.Canonical(),
        "Shared native array storage alias survives rebuild");
  const auto rebuilt = g.Encode(true);
  Monstey_Havok r(rebuilt);
  Check(r.Canonical() == g.Canonical(),
        "Forced aligned graph semantic and alias roundtrip");
  Check(r.Original() != B{}, "Rebuilt TAG0 container");
  g.Replace(g.Field(g.Root(), "x"), -9);
  g.Replace(g.Field(g.Root(), "s"), "Changed native string");
  g.Replace(g.Field(g.Root(), "a"), J::array({2.5, 3.5, 4.5, 5.5, 6.5}));
  g.Replace(g.Field(g.Root(), "v"), J::array({0.5, 1.5, 2.5, 3.5}));
  const auto edited = g.Encode();
  Monstey_Havok e(edited);
  Check(e.Plain(e.Root()).at("x").as_int() == -9 &&
            e.Plain(e.Root()).at("a").size() == 5,
        "Scalar/vector/string/resized-array authoring");
  Check(e.Node(e.Field(e.Root(), "opaque")).original_storage ==
            g.Node(g.Field(g.Root(), "opaque")).original_storage,
        "Unknown native subtype storage preserved");
  Monstey_WriteNewFile(out / L"synthetic-edit.tag0", edited);
  const auto clone = g.Clone(g.Dereference(ptr));
  Check(g.Plain(clone).at("x").as_int() == 42, "Typed deep clone");
  g.Node(ptr).target = clone;
  Check(Monstey_Havok(g.Encode()).Canonical() == g.Canonical(),
        "Authored pointer target and clone serialization");
  Reject([&] { g.Replace(g.Field(g.Root(), "x"), std::uint64_t(0xffffffff)); },
         "Signed range rejected");
  Reject([&] { g.Replace(g.Field(g.Root(), "v"), J::array({1, 2})); },
         "Tuple size rejected");
  Reject([&] { g.Replace(g.Field(g.Root(), "h"), 1.0001); },
         "Lossy truncated-half rejected");
  Monstey_Havok cyc(Fixture(false, true));
  Check(Monstey_Havok(cyc.Encode(true)).Canonical() == cyc.Canonical(),
        "Cyclic native pointer graph finite roundtrip");
  Reject([&] { cyc.Plain(cyc.Root()); },
         "Cyclic flattening rejected explicitly");
  Monstey_Havok missing(Fixture(true));
  Check(missing.TypeHashCoverage().at("status").as_string() == "FAIL",
        "Missing active THSH detected");
  Reject([&] { missing.Encode(true); },
         "Native build with missing active THSH rejected");
  const auto completed =
      Monstey_Havok::CompleteTypeHashes(Fixture(true), Fixture());
  Check(Monstey_Havok(completed).TypeHashCoverage().at("status").as_string() ==
            "PASS",
        "Exact-layout donor completes native THSH");
  auto truncated = raw;
  truncated.pop_back();
  Reject([&] { Monstey_Havok bad(truncated); }, "Truncated root bounded");
  auto bad = raw;
  bad[0] = 0x7f;
  Reject([&] { Monstey_Havok rejected(bad); }, "Oversized container bounded");
}
void Native(const std::filesystem::path &source,
            const std::filesystem::path &out) {
  const auto raw = Monstey_ReadFile(source);
  Monstey_Require(raw.size() >= 28 && Monstey_U32(raw, 0) == 0x49414b48 &&
                      Monstey_U32(raw, 4) == 3 &&
                      Monstey_U32(raw, 8) == 0xffffd00d &&
                      Monstey_U32(raw, 12) == raw.size() - 20 &&
                      Monstey_U32(raw, raw.size() - 4) == 0xffffdead,
                  "Invalid HKAI3 envelope");
  const auto tag =
      std::span<const std::uint8_t>(raw).subspan(16, raw.size() - 20);
  Monstey_Havok g(tag);
  Check(g.Encode() == B(tag.begin(), tag.end()),
        "Real HKAI3 TAG0 no-op byte exact");
  const auto rebuilt = g.Encode(true);
  Monstey_Havok r(rebuilt);
  Check(r.Canonical() == g.Canonical(),
        "Real HKAI3 graph reserialized with aliases intact");
  auto meshes = g.Objects("hkaiNavMesh");
  Check(!meshes.empty(), "Real native navigation types discovered");
  Monstey_WriteNewFile(out / L"native-rebuilt.tag0", rebuilt);
  Monstey_WriteNewText(out / L"native-tag-report.json", g.Describe().dump(2));
  Check(Monstey_HashSha256(source) == Monstey_HashSha256(raw),
        "Native source unchanged");
}
void NativePhysics(const std::filesystem::path &source,
                   const std::filesystem::path &out) {
  const auto raw = Monstey_ReadFile(source);
  std::size_t found = 0;
  for (std::size_t at = 4; at + 4 <= raw.size(); ++at) {
    if (raw[at] != 'T' || raw[at + 1] != 'A' || raw[at + 2] != 'G' ||
        raw[at + 3] != '0')
      continue;
    const auto begin = at - 4;
    const auto n = ((std::uint32_t(raw[begin]) << 24) |
                    (std::uint32_t(raw[begin + 1]) << 16) |
                    (std::uint32_t(raw[begin + 2]) << 8) | raw[begin + 3]) &
                   0x3fffffffu;
    if (n < 8 || n > raw.size() - begin)
      continue;
    const auto slice = std::span<const std::uint8_t>(raw).subspan(begin, n);
    Monstey_Havok graph(slice);
    if (graph.Objects("hknpCompressedMeshShape").empty())
      continue;
    Check(graph.Encode() == B(slice.begin(), slice.end()),
          "Real type12 compressed-collision graph no-op exact");
    auto report = graph.Describe();
    report["physical_source"] = Monstey_Utf8(source.wstring());
    report["physical_source_sha256"] = Monstey_HashSha256(raw);
    report["tag_offset"] = begin;
    if (graph.TypeHashCoverage().at("status").as_string() == "PASS") {
      const auto rebuilt = graph.Encode(true);
      Check(Monstey_Havok(rebuilt).Canonical() == graph.Canonical(),
            "Real type12 collision semantic and alias rebuild");
      Monstey_WriteNewFile(out / L"native-collision-rebuilt.tag0", rebuilt);
      report["forced_rebuild_verified"] = true;
    } else
      report["forced_rebuild_verified"] = false;
    Monstey_WriteNewText(out / L"native-collision-report.json", report.dump(2));
    ++found;
    break;
  }
  Check(found == 1,
        "Actual compressed-collision native TYPE profile discovered");
  Check(Monstey_HashSha256(source) == Monstey_HashSha256(raw),
        "Native physics source unchanged");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    const std::filesystem::path base = argc > 1 ? argv[1] : L"havok-tests";
    const auto out =
        base / std::to_wstring(
                   std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(out);
    Synthetic(out);
    if (argc > 2)
      Native(argv[2], out);
    if (argc > 3)
      NativePhysics(argv[3], out);
    std::cout << "PASS " << passed << " native Havok groups\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
