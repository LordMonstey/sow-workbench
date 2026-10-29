#include "Monstey_SowCatalogProjection.hpp"
#include "Monstey_GadbClassDeclarationApi.hpp"
#include "Monstey_Protocol.hpp"
#include "Monstey_Runtime.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
std::uint64_t checks = 0, groups = 0, rejections = 0, pages = 0, maxBytes = 0;
void Check(bool v, const char *m) {
  ++checks;
  Monstey_Require(v, m);
}
template <class F> void Reject(F f, std::string_view code = {}) {
  bool rejected = false;
  std::string actual;
  try {
    f();
  } catch (const Monstey_Error &e) {
    rejected = true;
    actual = e.code;
  } catch (const std::exception &) {
    rejected = true;
  }
  Check(rejected, "Invalid class API input accepted");
  if (!code.empty())
    Check(actual == code, "Class API rejection code differs");
  ++rejections;
}
J Field(unsigned i, std::size_t text = 0, char ch = 'x') {
  return J::object(
      {{"declaration_index", i},
       {"field_pointer", Monstey_Hex(0x140002000ull + 72 * i)},
       {"raw72_hex", std::string(140, '0') + "abcd"},
       {"raw72_sha256", std::string(64, 'a')},
       {"name",
        J::object({{"status", "FILE_BACKED"},
                   {"value", text ? std::string(text, ch) : "Duplicate"}})},
       {"constraint",
        J::object({{"status", "FILE_BACKED"},
                   {"value", text ? std::string(text, ch) : "raw"}})},
       {"default16_hex", "000000804523a17f010000004365c7ff"},
       {"captured_cache_byte_u8", i & 255},
       {"captured_binding_state_u32", 0xffffffffu - i},
       {"flags_u32", 64}});
}
J Class(std::uint64_t p, unsigned n) {
  auto fields = J::array();
  for (unsigned i = 0; i < n; ++i)
    fields.as_array().push_back(Field(i));
  return J::object(
      {{"class_pointer", Monstey_Hex(p)},
       {"raw64_hex", std::string(126, '0') + (p == 1 ? "ab" : "cd")},
       {"raw64_sha256", std::string(64, 'b')},
       {"field_table_pointer", "0x140002000"},
       {"field_count_u32", n},
       {"short_name",
        J::object({{"status", "FILE_BACKED"}, {"value", "Same"}})},
       {"description",
        J::object({{"status", "FILE_BACKED"}, {"value", "raw"}})},
       {"full_name",
        J::object({{"status", "FILE_BACKED"}, {"value", "Duplicate.Name"}})},
       {"parent_pointer", "0x3"},
       {"opaque_class_pointer_30", "0xffff000012340000"},
       {"class_30_role", "NOT_INFERRED"},
       {"flags_u32", 0x80000004u},
       {"captured_initialized_u8", 0xab},
       {"field_storage_status", "COMPLETE_FILE_BACKED"},
       {"fields", fields},
       {"private_blob", std::string(70000, 'z')}});
}
J Report(unsigned n = 100) {
  return J::object(
      {{"schema", "Monstey.GadbExplicitClassDeclarations/1"},
       {"source_sha256", std::string(64, 'c')},
       {"classes", J::array({Class(1, n), Class(2, 4), Class(3, 5)})},
       {"source_file_name", "fixture.exe"},
       {"source_unchanged", true}});
}
J Args(J pointer = J(1), unsigned limit = 16) {
  return J::object({{"class_pointer", pointer}, {"limit", limit}});
}
void Bounded(const J &p) {
  ++pages;
  const auto size = p.dump().size();
  maxBytes = std::max(maxBytes, static_cast<std::uint64_t>(size));
  Check(size <= 28 * 1024, "Class page exceeds byte bound");
  Check(p.at("serialized_bytes").as_uint() == size,
        "Serialized byte counter differs");
  Check(p.at("returned").as_uint() == p.at("rows").size(),
        "Returned row count differs");
}
J Request(const char *op, J a = J::object()) {
  return J::object({{"id", 17}, {"operation", op}, {"arguments", a}});
}
J Good(Monstey_Runtime &r, const char *op, J a = J::object()) {
  auto p = r.Call(Request(op, a));
  Check(p.at("ok").as_bool(), "Runtime call failed");
  return p;
}
void Counters(const J &a, const J &b) {
  Check(a.at("generation") == b.at("generation") &&
            a.at("revision") == b.at("revision"),
        "Read operation changed session counters");
}
void Actual(Monstey_Runtime &runtime, const std::filesystem::path &pe,
            const std::filesystem::path &report, std::string_view sha) {
  const auto reference = J::Parse(Monstey_ReadText(report));
  Check(reference.at("classes").size() == 39,
        "Actual reference lost identities");
  const auto before = Good(runtime, "describe"),
             changes = Good(runtime, "database_changes");
  for (const auto &row : reference.at("classes").as_array()) {
    auto a = J::object({{"path", Monstey_Utf8(pe.wstring())},
                        {"expected_sha256", sha},
                        {"class_pointer", row.at("class_pointer")},
                        {"limit", 1}});
    const auto reply = Good(runtime, "database_native_class", a);
    Counters(before, reply);
    const auto &p = reply.at("result");
    Bounded(p);
    Check(p.at("class").at("class_pointer") == row.at("class_pointer") &&
              p.at("class").at("raw64_hex") == row.at("raw64_hex"),
          "Actual class identity/raw64 differs");
    Check(p.at("class").at("opaque_class_pointer_30") ==
              row.at("opaque_class_pointer_30"),
          "Actual +30 state changed");
    Check(p.at("class").at("field_storage_status") ==
              row.at("field_storage_status"),
          "Actual storage classification changed");
    Check(p.at("total").as_uint() == row.at("fields").size(),
          "Actual available row count differs");
    if (row.at("fields").size())
      Check(p.at("rows")[0u] == row.at("fields")[0u],
            "Actual raw72 row changed");
    else
      Check(p.at("rows").size() == 0 && !p.at("has_more").as_bool(),
            "Virtual-only table fabricated rows");
    Check(p.at("source_unchanged").as_bool() && !p.contains("source_path"),
          "Actual source status/path wrong");
  }
  auto a = J::object({{"path", Monstey_Utf8(pe.wstring())},
                      {"expected_sha256", sha},
                      {"class_pointer", "0x14263c050"},
                      {"limit", 32}});
  Monstey_Mcp m(runtime);
  (void)m.Handle(
      J::object({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}}));
  const auto response = m.Handle(J::object(
      {{"jsonrpc", "2.0"},
       {"id", 2},
       {"method", "tools/call"},
       {"params",
        J::object({{"name", "database_native_class"}, {"arguments", a}})}}));
  Check(response.has_value() && !response->at("result").at("isError").as_bool(),
        "Actual MCP class inspection failed");
  Check(response->dump().size() <= 64 * 1024, "MCP envelope exceeds bound");
  const auto decoded =
      J::Parse(response->at("result").at("content")[0u].at("text").as_string());
  Counters(before, decoded);
  Bounded(decoded.at("result"));
  Check(Good(runtime, "describe").at("result") == before.at("result") &&
            Good(runtime, "database_changes").at("result") ==
                changes.at("result"),
        "Class inspection changed current project");
  Check(Monstey_HashSha256(pe) == sha, "Actual source changed");
  ++groups;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Check(argc == 3 || argc == 7, "Need NEW_OUT baseline225 [snapshot "
                                  "snapshotReport installed installedReport]");
    const std::filesystem::path out = argv[1];
    Check(!std::filesystem::exists(out), "Fresh test output required");
    std::filesystem::create_directories(out);
    auto report = Report();
    auto p = Monstey_GadbNativeClassPage(report, Args());
    Bounded(p);
    Check(p.at("class").at("class_pointer") == J(Monstey_Hex(1)) &&
              !p.at("class").contains("private_blob"),
          "Class metadata identity/whitelist wrong");
    Check(!p.at("parent_fields_included").as_bool() &&
              p.at("class").at("parent_pointer") == J("0x3"),
          "Parent link/fields wrong");
    ++groups;
    auto same =
        Monstey_GadbNativeClassPage(report, Args(J("0X0000000000000001")));
    Check(same == p, "Hex/integer pointer selection differs");
    auto sibling = Monstey_GadbNativeClassPage(report, Args(J("2")));
    Check(sibling.at("class").at("raw64_hex") !=
                  p.at("class").at("raw64_hex") &&
              sibling.at("total").as_uint() == 4,
          "Duplicate class names merged");
    ++groups;
    std::uint64_t offset = 0;
    J assembled = J::array();
    do {
      auto a = Args(1, 32);
      a["offset"] = offset;
      auto page = Monstey_GadbNativeClassPage(report, a);
      Bounded(page);
      for (const auto &row : page.at("rows").as_array())
        assembled.as_array().push_back(row);
      if (!page.at("has_more").as_bool())
        break;
      Check(page.at("next_offset").as_uint() > offset,
            "Paging did not progress");
      offset = page.at("next_offset").as_uint();
    } while (true);
    Check(assembled == report.at("classes")[0u].at("fields"),
          "Ordered duplicate-hash/raw rows lost");
    ++groups;
    for (const auto end : {100u, 101u, 4096u}) {
      auto a = Args();
      a["offset"] = end;
      auto page = Monstey_GadbNativeClassPage(report, a);
      Bounded(page);
      Check(page.at("rows").size() == 0 && !page.at("has_more").as_bool() &&
                page.at("next_offset").is_null(),
            "Past-end page incorrect");
    }
    ++groups;
    auto dense = Report(20);
    for (unsigned i = 0; i < 20; ++i)
      dense["classes"][0u]["fields"][i] = Field(i, 4095);
    offset = 0;
    std::uint64_t total = 0;
    do {
      auto a = Args(1, 32);
      a["offset"] = offset;
      auto page = Monstey_GadbNativeClassPage(dense, a);
      Bounded(page);
      Check(page.at("rows").size() > 0 && page.at("rows").size() < 32,
            "Byte-adaptive page failed");
      for (const auto &row : page.at("rows").as_array()) {
        Check(row == dense.at("classes")[0u].at(
                         "fields")[static_cast<std::size_t>(total)],
              "Byte paging changed a complete row");
        ++total;
      }
      if (!page.at("has_more").as_bool())
        break;
      offset = page.at("next_offset").as_uint();
    } while (true);
    Check(total == 20, "Byte paging silently dropped rows");
    ++groups;
    auto huge = Report(1);
    huge["classes"][0u]["description"] = J::object(
        {{"status", "FILE_BACKED"}, {"value", std::string(4095, '\1')}});
    huge["classes"][0u]["full_name"] = J::object(
        {{"status", "FILE_BACKED"}, {"value", std::string(4095, '\1')}});
    Reject([&] { (void)Monstey_GadbNativeClassPage(huge, Args()); },
           "native_class_response_limit");
    huge = Report(1);
    huge["classes"][0u]["fields"][0u] = Field(0, 4095, '\1');
    Reject([&] { (void)Monstey_GadbNativeClassPage(huge, Args()); },
           "native_class_response_limit");
    ++groups;
    auto absent = Report(1);
    absent["classes"][0u]["fields"] = J::array();
    absent["classes"][0u]["field_count_u32"] = 16;
    absent["classes"][0u]["field_storage_status"] = "UNAVAILABLE_FILE_RANGE";
    auto q = Monstey_GadbNativeClassPage(absent, Args());
    Bounded(q);
    Check(q.at("total").as_uint() == 0 &&
              q.at("class").at("field_count_u32").as_uint() == 16 &&
              !q.at("has_more").as_bool(),
          "Unavailable table fabricated or hid declaration count");
    ++groups;
    const auto invalidPointers =
        J::array({0, -1, true, nullptr, J::array(), "", "0x", "xyz", "-1", "+1",
                  " 1", "0x10000000000000000"});
    for (const auto &v : invalidPointers.as_array())
      Reject([&] { (void)Monstey_GadbNativeClassPage(report, Args(v)); });
    ++groups;
    const auto badLimits = J::array({0, 33, -1, true, "1", 1.5});
    for (const auto &v : badLimits.as_array()) {
      auto a = Args();
      a["limit"] = v;
      Reject([&] { (void)Monstey_GadbNativeClassPage(report, a); });
    }
    const auto badOffsets = J::array({-1, 4097, true, 1.5});
    for (const auto &v : badOffsets.as_array()) {
      auto a = Args();
      a["offset"] = v;
      Reject([&] { (void)Monstey_GadbNativeClassPage(report, a); });
    }
    auto unknown = Args();
    unknown["bogus"] = 1;
    Reject([&] { (void)Monstey_GadbNativeClassPage(report, unknown); });
    Reject([&] { (void)Monstey_GadbNativeClassPage(report, Args(4)); });
    ++groups;
    auto fileArgs =
        J::object({{"class_pointer", 1},
                   {"path", Monstey_Utf8((out / L"missing.exe").wstring())},
                   {"expected_sha256", std::string(64, '0')}});
    Reject([&] { (void)Monstey_GadbNativeClassInspect(fileArgs); },
           "unsupported_native_image");
    fileArgs["expected_sha256"] =
        "ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef";
    fileArgs["path"] = std::string("a\0b", 3);
    Reject([&] { (void)Monstey_GadbNativeClassInspect(fileArgs); },
           "invalid_path");
    Monstey_WriteNewFile(out / L"changed.exe",
                         std::array<std::uint8_t, 4>{1, 2, 3, 4});
    fileArgs["path"] = Monstey_Utf8((out / L"changed.exe").wstring());
    Reject([&] { (void)Monstey_GadbNativeClassInspect(fileArgs); },
           "source_hash_mismatch");
    ++groups;
    auto catalog = Monstey_HistoricalCatalogWithoutAtomUses(Monstey_Runtime::Catalog());
    auto baseline = J::Parse(Monstey_ReadText(argv[2]));
    Check(baseline.size() == 225 && catalog.size() == 226,
          "Catalog count wrong");
    auto &ops = catalog.as_array();
    auto it = std::find_if(ops.begin(), ops.end(), [](const auto &v) {
      return v.at("name") == J("database_native_class");
    });
    Check(it != ops.end(), "New operation absent");
    const auto descriptor = *it;
    Check(descriptor.at("read_only").as_bool() &&
              descriptor.at("annotations").at("readOnlyHint").as_bool(),
          "New operation is not readonly");
    Check(descriptor.at("inputSchema").at("required") ==
              J::array({"path", "expected_sha256", "class_pointer"}),
          "Required class API inputs wrong");
    Check(descriptor.at("inputSchema")
                  .at("properties")
                  .at("class_pointer")
                  .at("type") == J::array({"string", "integer"}),
          "Pointer schema type wrong");
    ops.erase(it);
    Check(catalog == baseline, "An old225 operation descriptor changed");
    ++groups;
    Monstey_Runtime runtime({}, out / L"absent-settings.json");
    (void)Good(runtime, "database_new");
    const auto before = Good(runtime, "describe"),
               changes = Good(runtime, "database_changes");
    for (unsigned i = 0; i < 7; ++i) {
      auto a = fileArgs;
      if (i == 0)
        a.as_object().erase("class_pointer");
      if (i == 1)
        a["class_pointer"] = true;
      if (i == 2)
        a["limit"] = 33;
      if (i == 3)
        a["offset"] = 4097;
      if (i == 4)
        a["unknown"] = 0;
      if (i == 5)
        a["expected_generation"] = before.at("generation").as_uint() + 1;
      if (i == 6)
        a["expected_revision"] = before.at("revision").as_uint() + 1;
      const auto reply = runtime.Call(Request("database_native_class", a));
      Check(!reply.at("ok").as_bool(), "Runtime invalid request passed");
      Counters(before, reply);
    }
    Check(Good(runtime, "describe").at("result") == before.at("result") &&
              Good(runtime, "database_changes").at("result") ==
                  changes.at("result"),
          "Failed read changed active project");
    Check(!std::filesystem::exists(out / L"absent-settings.json"),
          "Read wrote settings");
    ++groups;
    Monstey_Mcp m(runtime);
    (void)m.Handle(
        J::object({{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}}));
    const auto list = m.Handle(
        J::object({{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}}));
    Check(list.has_value(), "MCP tools missing");
    const auto &tools = list->at("result").at("tools");
    auto tool = std::find_if(
        tools.as_array().begin(), tools.as_array().end(), [](const auto &v) {
          return v.at("name") == J("database_native_class");
        });
    Check(tool != tools.as_array().end() &&
              tool->at("annotations").at("readOnlyHint").as_bool(),
          "MCP metadata wrong");
    ++groups;
    if (argc == 7) {
      Actual(
          runtime, argv[3], argv[4],
          "6b8db778135c082c934463b6524e6b220478fb6987f082ab37ca48a870acb455");
      Actual(
          runtime, argv[5], argv[6],
          "ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef");
    }
    Check(!std::filesystem::exists(out / L"absent-settings.json"),
          "Actual read wrote settings");
    Monstey_WriteNewText(out / L"report.json",
                         J::object({{"pass", true},
                                    {"groups", groups},
                                    {"assertions", checks},
                                    {"direct_rejections", rejections},
                                    {"pages", pages},
                                    {"maximum_page_bytes", maxBytes},
                                    {"catalog_operations", 227},
                                    {"old225_exact", true},
                                    {"actual_profiles", argc == 7 ? 2 : 0}})
                                 .dump(2) +
                             "\n");
    std::cout << "PASS " << groups << " groups " << checks << " assertions "
              << pages << " pages\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
