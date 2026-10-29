#include "Monstey_AssetInspect.hpp"
#include "Monstey_LtmiMaterial.hpp"
#include <chrono>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
void Monstey_Check(bool x, const char *why) {
  if (!x)
    throw Monstey_Error("asset_test", why);
}
template <class F> void Monstey_Reject(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::exception &) {
    rejected = true;
  }
  Monstey_Check(rejected, "Expected asset rejection");
}
struct Monstey_Row {
  unsigned type, flags;
  B name;
  std::uint32_t offset;
};
B Monstey_Fixture(unsigned version, const B &values,
                  const std::vector<Monstey_Row> &rows, unsigned a = 1) {
  B b(30, 0);
  Monstey_PutU32(b, 0, 0x494d544c);
  Monstey_PutU32(b, 4, version);
  Monstey_PutU32(b, 8, 0x10203040);
  Monstey_PutU32(b, 12, static_cast<std::uint32_t>(values.size()));
  Monstey_PutU16(b, 16, static_cast<std::uint16_t>(a));
  Monstey_PutU16(b, 18, static_cast<std::uint16_t>(rows.size() - a));
  Monstey_PutU16(b, 28, 4);
  b.insert(b.end(), {'f', 'x', 'o', 0});
  b.insert(b.end(), values.begin(), values.end());
  if (version == 7) {
    Monstey_AppendU32(b, 3);
    b.insert(b.end(), {0x11, 0x22, 0x33});
  }
  for (const auto &r : rows) {
    Monstey_AppendU16(b, static_cast<std::uint16_t>(r.type));
    b.push_back(static_cast<std::uint8_t>(r.flags));
    b.push_back(static_cast<std::uint8_t>(r.flags >> 8));
    Monstey_AppendU16(b, static_cast<std::uint16_t>(r.name.size()));
    b.insert(b.end(), r.name.begin(), r.name.end());
    Monstey_AppendU32(b, r.offset);
    if (version == 7)
      Monstey_AppendU16(b, 0x9876);
  }
  b.insert(b.end(), {0x91, 0x92});
  return b;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Check(argc == 3, "Need corpus JSON and NEW output dir");
    const auto root =
        std::filesystem::path(argv[2]) /
        ("run-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    Monstey_Check(!std::filesystem::exists(root), "Output root must be new");
    std::filesystem::create_directories(root);
    auto groups = J::array();
    auto test = [&](const char *name, auto f) {
      f();
      groups.as_array().push_back(name);
    };
    test("stored flags independent native low type and full U32 kind", [&] {
      B values(4);
      Monstey_PutU32(values, 0, 0x3f000000);
      const auto bytes = Monstey_Fixture(8, values, {{5, 0xabcd, {'F'}, 0}});
      const auto r = Monstey_MaterialInspect(bytes);
      const auto e = r["entries"][0];
      Monstey_Check(
          r["properties"]["F"].as_number() == 0.5 &&
              e["kind"].as_uint() == 0xabcd0005 && e["type"] == e["kind"] &&
              e["type_u16"].as_uint() == 5 && e["flag_a"].as_uint() == 0xcd &&
              e["flag_b"].as_uint() == 0xab,
          "flag/type split");
    });
    test("runtime U16 offset wrap with stored U32 diagnostics", [&] {
      B v(4);
      Monstey_PutU32(v, 0, 0x3f800000);
      const auto r = Monstey_MaterialInspect(
          Monstey_Fixture(8, v, {{5, 0, {'F'}, 0x12340000}}));
      const auto e = r["entries"][0];
      Monstey_Check(e["value"].as_number() == 1 &&
                        e["runtime_offset_u16"].as_uint() == 0 &&
                        e["value_offset"].as_uint() == 34 &&
                        e["stored_value_offset"].as_uint() == 34 + 0x12340000 &&
                        e["offset_truncated_by_native"].as_bool(),
                    "U16 wrap");
    });
    test("native boolean uses all four bytes", [&] {
      B v(4);
      Monstey_PutU32(v, 0, 0x100);
      const auto r =
          Monstey_MaterialInspect(Monstey_Fixture(8, v, {{6, 0, {'B'}, 0}}));
      Monstey_Check(r["properties"]["B"].as_bool(), "Bool32 high byte ignored");
    });
    test("version7 legacy blob row words metadata end trailer", [&] {
      B v(4);
      Monstey_PutU32(v, 0, 7);
      const auto b = Monstey_Fixture(7, v, {{4, 0, {'W'}, 0}});
      const auto r = Monstey_MaterialInspect(b);
      Monstey_Check(r["properties"]["W"].as_uint() == 7 &&
                        r["native_layout"]["legacy_blob_bytes"].as_uint() ==
                            3 &&
                        r["entries"][0]["legacy_word"].as_uint() == 0x9876 &&
                        r["metadata_end"].as_uint() == b.size() - 2 &&
                        r["trailing_bytes"].as_uint() == 2,
                    "version7 layout");
    });
    test("NaN payload exact in old value and new raw diagnostics", [&] {
      B v(4);
      Monstey_PutU32(v, 0, 0xffc01234);
      const auto r =
          Monstey_MaterialInspect(Monstey_Fixture(8, v, {{5, 0, {'N'}, 0}}));
      Monstey_Check(
          r["properties"]["N"]["float32_bits"].as_uint() == 0xffc01234 &&
              r["entries"][0]["value_bytes_hex"].as_string() == "3412c0ff",
          "NaN bits");
    });
    test("raw14 remains opaque eight-byte storage", [&] {
      B v{1, 2, 3, 4, 5, 6, 7, 8};
      const auto r =
          Monstey_MaterialInspect(Monstey_Fixture(8, v, {{14, 0, {'P'}, 0}}));
      const auto e = r["entries"][0];
      Monstey_Check(e["value"]["unsupported_type"].as_uint() == 14 &&
                        e["value_bytes_hex"].as_string() ==
                            "0102030405060708" &&
                        !e["authored_value_qualified"].as_bool() &&
                        e["native_copy_bytes"].as_uint() == 8,
                    "Opaque14 semantics");
    });
    test("type8 optional bounded observed surface string", [&] {
      const auto r = Monstey_MaterialInspect(
          Monstey_Fixture(8, B{'s', 'u', 'r', 'f', 0}, {{8, 0, {'S'}, 0}}));
      const auto e = r["entries"][0];
      Monstey_Check(e["value"].as_string() == "surf" &&
                        e["native_zero_copy"].as_bool() &&
                        !e["string_consumer_verified"].as_bool(),
                    "Observed type8");
    });
    test("unbounded type8 has null observation without forced NUL", [&] {
      const auto r = Monstey_MaterialInspect(
          Monstey_Fixture(8, B{0xff, 0xfe, 0xfd}, {{8, 0, {'S'}, 0}}));
      Monstey_Check(r["properties"]["S"].is_null() &&
                        r["entries"][0]["value_bytes_hex"].as_string().empty(),
                    "Unbounded type8");
    });
    test("zero-copy type8 out of blob is diagnostic only", [&] {
      const auto r = Monstey_MaterialInspect(
          Monstey_Fixture(8, B{}, {{8, 0, {'S'}, 0xffff}}));
      Monstey_Check(r["properties"]["S"].is_null() &&
                        r["entries"][0]["runtime_offset_u16"].as_uint() ==
                            65535,
                    "Out-of-blob zero copy");
    });
    test("duplicate display names keep all rows and last map value", [&] {
      B v(8);
      Monstey_PutU32(v, 0, 1);
      Monstey_PutU32(v, 4, 2);
      const auto r = Monstey_MaterialInspect(
          Monstey_Fixture(8, v, {{4, 0, {'D', 0}, 0}, {4, 0, {'D'}, 4}}));
      Monstey_Check(r["properties"]["D"].as_uint() == 2 &&
                        r["entries"].size() == 2 &&
                        r["duplicate_display_name_count"].as_uint() == 1 &&
                        r["properties_duplicate_name_policy"].as_string() ==
                            "last_entry_wins_in_table_a_then_b_order",
                    "Duplicate policy");
    });
    test("qualified string cannot read into metadata", [&] {
      Monstey_Reject([&] {
        Monstey_MaterialInspect(Monstey_Fixture(8, B{'x'}, {{1, 0, {'T'}, 0}}));
      });
    });
    test("unknown low type preserves raw metadata without invented value", [&] {
      const auto r = Monstey_MaterialInspect(Monstey_Fixture(
          8, B{}, {{0x1234, 0xffee, {0xff, 0, 'X'}, 0xffffffff}}));
      const auto e = r["entries"][0];
      Monstey_Check(e["raw_name_hex"].as_string() == "ff0058" &&
                        e["kind"].as_uint() == 0xffee1234 &&
                        e["value"]["unsupported_type"].as_uint() ==
                            0xffee1234 &&
                        !e["raw_storage_editable"].as_bool(),
                    "Unknown profile");
    });
    test("unsupported versions and loader count cap reject", [&] {
      Monstey_Reject(
          [&] { Monstey_MaterialInspect(Monstey_Fixture(9, B{}, {}, 0)); });
      auto b = Monstey_Fixture(8, B{}, {}, 0);
      Monstey_PutU16(b, 16, 129);
      Monstey_Reject([&] { Monstey_MaterialInspect(b); });
    });
    const auto manifest = J::Parse(Monstey_ReadText(argv[1]));
    auto rows = J::array();
    for (const auto &item : manifest.as_array()) {
      const std::filesystem::path path = Monstey_Wide(item.as_string());
      const auto b = Monstey_ReadFile(path);
      const auto before = Monstey_HashSha256(b);
      const auto r = Monstey_AssetInspect(path);
      const auto codec = Monstey_LtmiDescribe(Monstey_LtmiRead(b));
      const auto m = r["material"];
      Monstey_Check(m["native_layout"] == codec, "Asset/native codec mismatch");
      Monstey_Check(m["entries"].size() == codec["entries"].size() &&
                        m["source_sha256"].as_string() == before,
                    "Asset snapshot differs");
      for (const auto &e : m["entries"].as_array())
        Monstey_Check(e.contains("kind") && e.contains("name") &&
                          e.contains("table") && e.contains("value_offset") &&
                          e.contains("value") && e.contains("raw_name_hex") &&
                          e.contains("value_bytes_hex"),
                      "Public entry keys lost");
      Monstey_Check(Monstey_HashSha256(path) == before, "Asset source changed");
      rows.as_array().push_back(J::object({{"source", item},
                                           {"sha256", before},
                                           {"codec_exact", true},
                                           {"source_unchanged", true}}));
    }
    const auto report = J::object(
        {{"schema", "Monstey.LtmiAssetTests/1"},
         {"status", "PASS"},
         {"synthetic_groups", groups},
         {"synthetic_passed", static_cast<std::uint64_t>(groups.size())},
         {"actual_materials", static_cast<std::uint64_t>(rows.size())},
         {"actual_rows", rows},
         {"game_written", false},
         {"original_program_executed", false},
         {"runtime_material_rendering_verified", false}});
    Monstey_WriteNewText(root / L"report.json", report.dump(2) + "\n");
    std::cout << groups.size() << " synthetic groups and " << rows.size()
              << " actual LTMI asset inspections PASS\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}