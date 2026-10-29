#include "Monstey_GadbClassDeclaration.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = std::vector<std::uint8_t>;
constexpr std::uint64_t Base = 0x140000000, Va = Base + 0x1000;
std::uint64_t checks = 0, groups = 0;
void Check(bool b, const char *m) {
  ++checks;
  Monstey_Require(b, m);
}
void W(B &b, std::size_t p, std::uint64_t v, unsigned n) {
  Check(p <= b.size() && n <= b.size() - p, "Test write bounds");
  for (unsigned i = 0; i < n; ++i)
    b[p + i] = static_cast<std::uint8_t>(v >> (8 * i));
}
void Q(B &b, std::uint64_t a, std::uint64_t v, unsigned n = 8) {
  W(b, 0x200 + static_cast<std::size_t>(a - Va), v, n);
}
void Text(B &b, std::uint64_t a, const char *s) {
  const auto n = std::strlen(s) + 1;
  std::copy_n(reinterpret_cast<const std::uint8_t *>(s), n,
              b.begin() + 0x200 + static_cast<std::size_t>(a - Va));
}
B Fixture() {
  B b(0x4200);
  W(b, 0, 0x5a4d, 2);
  W(b, 0x3c, 0x80, 4);
  W(b, 0x80, 0x4550, 4);
  W(b, 0x84, 0x8664, 2);
  W(b, 0x86, 1, 2);
  W(b, 0x94, 0xf0, 2);
  W(b, 0x98, 0x20b, 2);
  W(b, 0x98 + 24, Base, 8);
  const auto p = 0x188u;
  W(b, p + 8, 0x5000, 4);
  W(b, p + 12, 0x1000, 4);
  W(b, p + 16, 0x4000, 4);
  W(b, p + 20, 0x200, 4);
  Text(b, Va + 0x100, "Duplicate");
  Text(b, Va + 0x140, "Field");
  Text(b, Va + 0x180, "f32");
  for (const auto c : {Va, Va + 0x40}) {
    Q(b, c, Va + 0x200 + (c - Va) * 4);
    Q(b, c + 8, 2, 4);
    Q(b, c + 12, 0xffffffff, 4);
    Q(b, c + 16, Va + 0x100);
    Q(b, c + 24, Va + 0x180);
    Q(b, c + 32, Va + 0x100);
    Q(b, c + 48, 0xfffffffffffffff0ull);
    Q(b, c + 56, 0x80000004, 4);
    Q(b, c + 60, 0xab, 1);
    for (unsigned i = 0; i < 2; ++i) {
      const auto f = Va + 0x200 + (c - Va) * 4 + 72ull * i;
      Q(b, f, Va + 0x140);
      Q(b, f + 8, Va + 0x180);
      Q(b, f + 16, 0x7fa1234580000000ull);
      Q(b, f + 24, 0xffc7654300000001ull);
      Q(b, f + 32, Monstey_NativeHash("Field"), 4);
      Q(b, f + 36, 2, 4);
      Q(b, f + 40, 0xffffffff, 4);
      Q(b, f + 44, 0x40, 4);
      Q(b, f + 48, 0xffffffffu - i, 4);
      Q(b, f + 56, 0xffff000012340000ull + i);
      Q(b, f + 64, 0xd0 + i, 1);
      Q(b, f + 65, 0x11223344556677, 7);
    }
  }
  return b;
}
J Inspect(const B &b, std::initializer_list<std::uint64_t> p) {
  const std::vector<std::uint64_t> a(p);
  return Monstey_GadbInspectClassPointers(b, a);
}
const J &Row(const J &j, std::uint64_t p) {
  for (const auto &r : j.at("classes").as_array())
    if (r.at("class_pointer").as_string() == Monstey_Hex(p))
      return r;
  throw std::runtime_error("Missing test identity");
}
template <class F> void Throws(F f) {
  bool yes = false;
  try {
    f();
  } catch (const std::exception &) {
    yes = true;
  }
  Check(yes, "Expected rejection absent");
}
std::string Hex(std::span<const std::uint8_t> v) {
  static constexpr char d[] = "0123456789abcdef";
  std::string s;
  for (auto x : v) {
    s += d[x >> 4];
    s += d[x & 15];
  }
  return s;
}
std::span<const std::uint8_t> IndependentAt(const B &b, std::uint64_t a,
                                            std::size_t n) {
  const auto nt = Monstey_U32(b, 0x3c);
  const auto opt = static_cast<std::size_t>(nt) + 24;
  const auto base = Monstey_U64(b, opt + 24);
  Check(a >= base, "Independent address below base");
  const auto rva = a - base;
  const auto table = opt + Monstey_U16(b, static_cast<std::size_t>(nt) + 20);
  const auto count = Monstey_U16(b, static_cast<std::size_t>(nt) + 6);
  for (unsigned i = 0; i < count; ++i) {
    const auto p = table + 40ull * i;
    const auto first = Monstey_U32(b, p + 12);
    const auto size = Monstey_U32(b, p + 16);
    const auto raw = Monstey_U32(b, p + 20);
    if (rva >= first && rva - first < size && n <= size - (rva - first)) {
      const auto at = raw + rva - first;
      Check(at <= b.size() && n <= b.size() - at,
            "Independent file range overflow");
      return std::span<const std::uint8_t>(b).subspan(
          static_cast<std::size_t>(at), n);
    }
  }
  throw std::runtime_error("Independent virtual-only range");
}
void Actual(const std::filesystem::path &pe,
            const std::filesystem::path &report, std::string_view expected,
            bool backed) {
  const auto b = Monstey_ReadFile(pe);
  Check(Monstey_HashSha256(b) == expected, "Actual identity mismatch");
  const auto text = Monstey_ReadFile(report);
  const auto j = J::Parse(std::string_view(
      reinterpret_cast<const char *>(text.data()), text.size()));
  Check(j.at("classes").size() == 39 &&
            j.at("static_bank_memberships").size() == 34,
        "Actual class identities missing");
  std::set<std::string> all;
  std::map<std::string, unsigned> names;
  std::uint64_t fields = 0, nonzero = 0;
  for (const auto &r : j.at("classes").as_array()) {
    const auto p = std::stoull(r.at("class_pointer").as_string(), nullptr, 0);
    const auto raw = IndependentAt(b, p, 64);
    Check(r.at("raw64_hex").as_string() == Hex(raw),
          "Actual raw class disagreement");
    Check(all.insert(r.at("class_pointer").as_string()).second,
          "Actual class pointer duplicated");
    Check(r.at("flags_u32").as_uint() == Monstey_U32(raw, 56) &&
              r.at("captured_initialized_u8").as_uint() == raw[60],
          "Actual flags/state disagreement");
    const auto q30 = Monstey_U64(raw, 48);
    if (q30) {
      ++nonzero;
      ++names[r.at("full_name").at("value").as_string()];
    }
    Check(r.at("opaque_class_pointer_30").as_string() == Monstey_Hex(q30),
          "Actual opaque target disagreement");
    const auto count = Monstey_U32(raw, 8);
    const auto table = Monstey_U64(raw, 0);
    bool readable = true;
    try {
      (void)IndependentAt(b, table, static_cast<std::size_t>(count) * 72);
    } catch (const std::exception &) {
      readable = false;
    }
    Check(readable == backed, "Actual field storage expectation differs");
    Check(r.at("fields").size() == (backed ? count : 0),
          "Actual row count disagreement");
    for (const auto &f : r.at("fields").as_array()) {
      const auto index = f.at("declaration_index").as_uint();
      const auto bytes = IndependentAt(b, table + 72 * index, 72);
      ++fields;
      Check(f.at("raw72_hex").as_string() == Hex(bytes),
            "Actual raw field disagreement");
      Check(f.at("captured_binding_state_u32").as_uint() ==
                    Monstey_U32(bytes, 48) &&
                f.at("captured_cache_byte_u8").as_uint() == bytes[64],
            "Actual cache state disagreement");
      Check(f.at("name_hash_matches").as_bool(),
            "Actual field name/hash failure");
    }
  }
  Check(nonzero == 34 && names.size() == 17,
        "Actual duplicate identities changed");
  for (const auto &[n, k] : names) {
    (void)n;
    Check(k == 2, "Actual duplicate pair collapsed");
  }
  Check(fields == (backed ? 598u : 0u),
        "Actual total field count disagreement");
  Check(Monstey_HashSha256(pe) == expected, "Actual input changed");
  ++groups;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Require(argc == 2 || argc == 6,
                    "Usage: tests NEW_REPORT [snapshotPE snapshotReport "
                    "installedPE installedReport]");
    const std::filesystem::path out = argv[1];
    auto b = Fixture();
    const auto original = b;
    auto j = Inspect(b, {Va});
    const auto &r = Row(j, Va);
    Check(r.at("opaque_class_pointer_30").as_string() == "0xfffffffffffffff0",
          "Nonzero +30 target was interpreted/rejected");
    Check(r.at("field_count_u32").as_uint() == 2,
          "Qword count consumed reserved padding");
    Check(r.at("fields").size() == 2, "Field duplicates dropped");
    Check(r.at("captured_initialized_u8").as_uint() == 0xab &&
              r.at("flags_u32").as_uint() == 0x80000004,
          "Flags and initialized byte conflated");
    Check(b == original, "Read-only source changed");
    ++groups;
    j = Inspect(b, {Va, Va + 0x40, Va});
    Check(j.at("classes").size() == 2 &&
              j.at("requested_class_pointers").size() == 3,
          "Duplicate names or repeated input identities lost");
    Check(Row(j, Va).at("raw64_hex") != Row(j, Va + 0x40).at("raw64_hex"),
          "Distinct class descriptors merged");
    ++groups;
    Q(b, Va + 40, Va + 0x40);
    j = Inspect(b, {Va});
    Check(j.at("classes").size() == 2, "Parent pointer chain lost");
    Check(Row(j, Va).at("parent_pointer").as_string() == Monstey_Hex(Va + 0x40),
          "Parent identity changed");
    ++groups;
    Q(b, Va + 0x40 + 40, Va);
    Throws([&] { (void)Inspect(b, {Va}); });
    ++groups;
    b = original;
    Q(b, Va, Va + 0x4500);
    j = Inspect(b, {Va});
    Check(Row(j, Va).at("field_storage_status").as_string() ==
                  "UNAVAILABLE_FILE_RANGE" &&
              Row(j, Va).at("fields").size() == 0,
          "Virtual tail zero-filled or falsely decoded");
    ++groups;
    b = original;
    Q(b, Va, Va + 0x3fc0);
    j = Inspect(b, {Va});
    Check(Row(j, Va).at("fields").size() == 0,
          "Cross-boundary partial table returned");
    ++groups;
    b = original;
    Q(b, Va + 8, 0, 4);
    Q(b, Va, UINT64_MAX);
    j = Inspect(b, {Va});
    Check(Row(j, Va).at("field_storage_status").as_string() ==
              "COMPLETE_FILE_BACKED",
          "Empty table eagerly consumed");
    ++groups;
    b = original;
    Q(b, Va, UINT64_MAX - 5);
    Throws([&] { (void)Inspect(b, {Va}); });
    b = original;
    Q(b, Va + 8, 4097, 4);
    Throws([&] { (void)Inspect(b, {Va}); });
    b = original;
    Throws([&] {
      (void)Monstey_GadbInspectClassPointers(b, std::array{Va}, "bad");
    });
    Throws([&] { (void)Inspect(b, {}); });
    Throws([&] { (void)Inspect(b, {0}); });
    ++groups;
    Q(b, Va + 0x200, Va + 0x4500);
    j = Inspect(b, {Va});
    Check(Row(j, Va).at("fields")[0].at("name").at("status").as_string() ==
              "UNAVAILABLE_OR_INVALID",
          "Unavailable field name falsely decoded");
    Check(Row(j, Va).at("fields").size() == 2,
          "Unavailable name dropped raw descriptor");
    ++groups;
    b = original;
    Q(b, Va + 0x100, 0xff, 1);
    j = Inspect(b, {Va});
    Check(Row(j, Va).at("full_name").at("status").as_string() ==
              "UNAVAILABLE_OR_INVALID",
          "Invalid UTF8 escaped into JSON");
    ++groups;
    b = original;
    j = Inspect(b, {Va});
    const auto &f = Row(j, Va).at("fields")[0];
    Check(f.at("default16_hex").as_string() ==
              "000000804523a17f010000004365c7ff",
          "Raw nonfinite/signed-zero defaults changed");
    Check(f.at("index_callback_pointer").as_string() == "0xffff000012340000",
          "Unknown field callback read or rewritten");
    Check(f.at("raw72_hex").as_string().ends_with("d077665544332211"),
          "Opaque field suffix lost");
    ++groups;
    if (argc == 6) {
      Actual(argv[2], argv[3],
             "6b8db778135c082c934463b6524e6b220478fb6987f082ab37ca48a870acb455",
             true);
      Actual(argv[4], argv[5],
             "ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef",
             false);
    }
    Monstey_WriteNewText(out, J::object({{"pass", true},
                                         {"groups", groups},
                                         {"assertions", checks},
                                         {"actual_profiles", argc == 6 ? 2 : 0},
                                         {"synthetic_only", argc == 2}})
                                      .dump(2) +
                                  "\n");
    std::cout << "PASS " << groups << " groups " << checks << " assertions\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
