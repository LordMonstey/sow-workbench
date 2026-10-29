#include "Monstey_SowCatalogProjection.hpp"
#include "Monstey_GadbBanks.hpp"
#include "Monstey_Protocol.hpp"
#include "Monstey_Runtime.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
unsigned Monstey_RejectionCases = 0;
unsigned Monstey_RuntimeRejectionCases = 0;
void Monstey_Check(bool value, const char *message) {
  Monstey_Require(value, message);
}
J Monstey_Request(const char *op, J args = J::object()) {
  return J::object({{"id", 71}, {"operation", op}, {"arguments", args}});
}
J Monstey_Good(Monstey_Runtime &r, const char *op, J args = J::object()) {
  auto reply = r.Call(Monstey_Request(op, args));
  Monstey_Require(reply.at("ok").as_bool(), reply.dump());
  return reply;
}
void Monstey_Counters(const J &a, const J &b) {
  Monstey_Check(a.at("generation") == b.at("generation") &&
                    a.at("revision") == b.at("revision"),
                "Read-only operation changed session counters");
}
void Monstey_Reject(const std::function<void()> &f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::exception &) {
    rejected = true;
  }
  Monstey_Check(rejected, "Invalid input accepted");
  ++Monstey_RejectionCases;
}
J Monstey_Synthetic() {
  auto banks = J::array();
  for (unsigned b = 0; b < 6; ++b) {
    auto classes = J::array();
    for (unsigned i = 0; i < (b == 5 ? 0u : 3u); ++i)
      classes.as_array().push_back(J::object(
          {{"array_index", i},
           {"first_exact_byte_name_index", i == 2 ? 0u : i},
           {"full_name", i == 1 ? "Curve.Float" : "WorldUI.StorePredicate"},
           {"short_name_at_class_plus_10", i == 1 ? "Float" : "StorePredicate"},
           {"captured_file_initialized_byte_plus_3c", b == 0 ? 1 : 0},
           {"descriptor_qword_plus_30", "0x12345678"},
           {"introspection_status", "UNKNOWN"},
           {"opaque_class_pointer_30_role", "PRESENT_UNQUALIFIED"},
           {"private_blob", std::string(70000, 'x')}}));
    banks.as_array().push_back(J::object(
        {{"bank_index", b},
         {"name", "Bank" + std::to_string(b)},
         {"returned_count", classes.size()},
         {"exact_byte_duplicate_full_names",
          b == 5 ? J::array()
                 : J::array({J::object({{"first", 0}, {"second", 2}})})},
         {"classes", classes},
         {"private_bank_blob", "not exported"}}));
  }
  return J::object(
      {{"schema", "Monstey.GadbBankRegistration/1"},
       {"banks", banks},
       {"class_pointer_assignments", 15},
       {"native_nonnull_name_comparator", "UNKNOWN"},
       {"actual_prior_initialization_and_binding_effects", "UNKNOWN"},
       {"runtime_BSS_contents_verified", false},
       {"private_root_blob", std::string(70000, 'x')}});
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Check(argc == 3 || argc == 5,
                  "Need fresh output, source image, expected SHA256 "
                  "and baseline223 catalog");
    const bool withActual = argc == 5;
    const std::filesystem::path out = argv[1],
                                source = withActual
                                             ? std::filesystem::path(argv[2])
                                             : out / L"missing.exe";
    Monstey_Check(!std::filesystem::exists(out), "Fresh output required");
    std::filesystem::create_directories(out);
    const auto expected =
        withActual ? Monstey_Utf8(argv[3]) : std::string(64, '0');
    const auto path = Monstey_Utf8(source.wstring());
    const auto args =
        J::object({{"path", path}, {"expected_sha256", expected}});
    auto groups = J::array();
    auto test = [&](const char *name, const std::function<void()> &body) {
      try {
        body();
      } catch (const std::exception &e) {
        throw Monstey_Error("test", std::string(name) + ": " + e.what());
      }
      groups.as_array().push_back(name);
    };
    const auto synthetic = Monstey_Synthetic();
    test("six_bank_summary_has_no_class_payload", [&] {
      const auto page = Monstey_GadbBankPage(synthetic, J::object());
      Monstey_Check(page.at("mode") == J("bank_summary") &&
                        page.at("banks").size() == 6 &&
                        page.at("rows").empty() &&
                        page.at("next_offset").is_null() &&
                        page.at("bank_index").is_null(),
                    "Summary wrong");
      Monstey_Check(!page.at("metadata").contains("private_root_blob") &&
                        !page.at("banks")[0u].contains("private_bank_blob") &&
                        page.dump().size() < 65536,
                    "Summary leaked payload");
    });
    test("global_page_retains_bank_and_native_array_indices", [&] {
      auto page = Monstey_GadbBankPage(
          synthetic, J::object({{"offset", 2}, {"limit", 4}}));
      Monstey_Check(page.at("total").as_uint() == 15 &&
                        page.at("rows").size() == 4 &&
                        page.at("next_offset").as_uint() == 6,
                    "Page count wrong");
      const auto &rows = page.at("rows");
      Monstey_Check(rows[0u].at("bank_index").as_uint() == 0 &&
                        rows[0u].at("array_index").as_uint() == 2 &&
                        rows[1u].at("bank_index").as_uint() == 1 &&
                        rows[1u].at("array_index").as_uint() == 0,
                    "Native indices renumbered");
      Monstey_Check(rows[0u].at("first_exact_byte_name_index").as_uint() == 0 &&
                        !rows[0u].contains("private_blob"),
                    "Duplicate precedence lost or payload leaked");
    });
    test("exact_bank_selection_and_empty_bank", [&] {
      for (unsigned b = 0; b < 6; ++b) {
        const auto p =
            Monstey_GadbBankPage(synthetic, J::object({{"bank_index", b}}));
        Monstey_Check(p.at("total").as_uint() == (b == 5 ? 0u : 3u) &&
                          p.at("next_offset").is_null(),
                      "Bank selection wrong");
        for (const auto &row : p.at("rows").as_array())
          Monstey_Check(row.at("bank_index").as_uint() == b,
                        "Cross-bank fallback");
      }
    });
    test("search_is_display_only_and_preserves_duplicate_precedence", [&] {
      const auto p = Monstey_GadbBankPage(
          synthetic,
          J::object({{"bank_index", 2}, {"query", "STOREpredicate"}}));
      Monstey_Check(
          p.at("total").as_uint() == 2 &&
              p.at("rows")[1u].at("array_index").as_uint() == 2 &&
              p.at("rows")[1u].at("first_exact_byte_name_index").as_uint() == 0,
          "Search changed native indices");
      Monstey_Check(
          p.at("metadata").at("native_nonnull_name_comparator") ==
                  J("UNKNOWN") &&
              p.at("query_comparison").as_string().find("display filter") !=
                  std::string::npos,
          "Search overclaims native equivalence");
    });
    test("short_name_search_and_miss", [&] {
      auto p = Monstey_GadbBankPage(synthetic, J::object({{"query", "fLoAt"}}));
      Monstey_Check(p.at("total").as_uint() == 5 && p.at("rows").size() == 5,
                    "Short-name match failed");
      p = Monstey_GadbBankPage(synthetic, J::object({{"query", "not found"}}));
      Monstey_Check(p.at("total").as_uint() == 0 && p.at("rows").empty() &&
                        p.at("next_offset").is_null(),
                    "Miss failed");
    });
    test("offset_and_limit_boundaries_no_overflow", [&] {
      for (const std::uint64_t offset :
           {0ull, 14ull, 15ull, 16ull, static_cast<std::uint64_t>(INT64_MAX)}) {
        const auto p = Monstey_GadbBankPage(
            synthetic, J::object({{"offset", offset}, {"limit", 1}}));
        Monstey_Check(p.at("rows").size() == (offset < 15 ? 1u : 0u),
                      "Offset wrong");
        if (offset >= 14)
          Monstey_Check(p.at("next_offset").is_null(), "Terminal page loops");
      }
      const auto p = Monstey_GadbBankPage(
          synthetic, J::object({{"query", ""}, {"limit", 32}}));
      Monstey_Check(p.at("total").as_uint() == 15,
                    "Empty query did not select page");
    });
    test("reject_invalid_types_bounds_and_foreign_arguments", [&] {
      const auto Monstey_OwnedRejectionCases1 =
          J::array({J::object({{"bank_index", 6}}),
                    J::object({{"bank_index", -1}}), J::object({{"limit", 0}}),
                    J::object({{"limit", 33}}), J::object({{"limit", true}}),
                    J::object({{"offset", 1.5}}), J::object({{"offset", -1}}),
                    J::object({{"query", std::string(513, 'x')}}),
                    J::object({{"query", nullptr}}),
                    J::object({{"output", "forbidden"}})});
      for (const auto &bad : Monstey_OwnedRejectionCases1.as_array())
        Monstey_Reject([&] { (void)Monstey_GadbBankPage(synthetic, bad); });
      Monstey_Reject(
          [&] { (void)Monstey_GadbBankPage(J::object(), J::object()); });
    });
    test("returned_metadata_is_owned_and_unknown_states_preserved", [&] {
      const auto before = synthetic;
      auto p = Monstey_GadbBankPage(synthetic, J::object({{"bank_index", 0}}));
      Monstey_Check(p.at("rows")[0u].at("opaque_class_pointer_30_role") ==
                            J("PRESENT_UNQUALIFIED") &&
                        !p.at("runtime_verified").as_bool(),
                    "Unknown state upgraded");
      p["rows"][0u]["array_index"] = 999;
      p["metadata"]["runtime_BSS_contents_verified"] = true;
      Monstey_Check(
          synthetic == before &&
              Monstey_GadbBankPage(synthetic, J::object({{"bank_index", 0}}))
                      .at("rows")[0u]
                      .at("array_index")
                      .as_uint() == 0,
          "Result aliases source");
    });
    test("file_validation_precedes_source_read", [&] {
      const auto Monstey_OwnedRejectionCases2 =
          J::array({J::object({{"limit", 0}}), J::object({{"bank_index", 6}}),
                    J::object({{"query", true}})});
      for (const auto &bad : Monstey_OwnedRejectionCases2.as_array()) {
        auto a = bad;
        a["path"] = "not-present.exe";
        a["expected_sha256"] = expected;
        Monstey_Reject([&] { (void)Monstey_GadbBankInspect(a); });
      }
      auto a = args;
      a["expected_sha256"] = std::string(64, 'A');
      Monstey_Reject([&] { (void)Monstey_GadbBankInspect(a); });
      a = args;
      a["expected_sha256"] = std::string(64, '0');
      Monstey_Reject([&] { (void)Monstey_GadbBankInspect(a); });
    });
    J actual;
    if (withActual)
      test("self_contained_actual_reader_has_six_banks_3151_raw_records", [&] {
        const auto bytes = Monstey_ReadFile(source, 256ull * 1024 * 1024);
        Monstey_Check(Monstey_HashSha256(bytes) == expected,
                      "Source identity mismatch");
        actual = Monstey_GadbBankRegistration(bytes);
        Monstey_Check(actual.at("banks").size() == 6 &&
                          actual.at("class_pointer_assignments").as_uint() ==
                              3151 &&
                          !actual.at("semantic_map_required").as_bool(),
                      "Actual reader counts wrong or private map required");
      });
    std::size_t maximumPageBytes = 0;
    if (withActual)
      test("all_actual_classes_page_in_exact_order_below_transport_cap", [&] {
        std::size_t count = 0;
        for (const auto &bank : actual.at("banks").as_array()) {
          std::uint64_t offset = 0;
          do {
            const auto p = Monstey_GadbBankPage(
                actual, J::object({{"bank_index", bank.at("bank_index")},
                                   {"offset", offset}}));
            maximumPageBytes = std::max(maximumPageBytes, p.dump().size());
            Monstey_Check(p.dump().size() + 1024 < 65536,
                          "Page exceeds protocol envelope budget");
            for (const auto &row : p.at("rows").as_array()) {
              const auto &native =
                  bank.at("classes")[static_cast<std::size_t>(offset++)];
              for (const auto &[key, value] : native.as_object())
                if (row.contains(key))
                  Monstey_Check(row.at(key) == value,
                                "Page changed native metadata");
              ++count;
            }
            if (p.at("next_offset").is_null())
              break;
            Monstey_Check(p.at("next_offset").as_uint() == offset,
                          "Paging does not progress");
          } while (true);
        }
        Monstey_Check(count == 3151, "Missing actual class records");
      });
    test("catalog224_is_one_additive_readonly_operation", [&] {
      auto catalog = Monstey_HistoricalCatalogWithoutAtomUses(Monstey_Runtime::Catalog());
      const auto baseline = J::Parse(Monstey_ReadText(argv[argc - 1]));
      Monstey_Check(baseline.size() == 223 && catalog.size() == 226,
                    "Catalog size changed unexpectedly");
      auto &rows = catalog.as_array();
      const auto explicitClass = std::find_if(rows.begin(), rows.end(), [](const auto &x) {
        return x.at("name") == J("database_native_class");
      });
      Monstey_Check(explicitClass != rows.end(), "Missing additive native-class descriptor");
      rows.erase(explicitClass);
      const auto tree = std::find_if(rows.begin(), rows.end(), [](const auto &x) {
        return x.at("name") == J("sow_world_navigation_static_tree");
      });
      Monstey_Check(tree != rows.end(), "Missing additive static-tree descriptor");
      rows.erase(tree);
      const auto it = std::find_if(rows.begin(), rows.end(), [](const auto &x) {
        return x.at("name") == J("database_native_banks");
      });
      Monstey_Check(it != rows.end(), "Descriptor absent");
      const auto &schema = it->at("inputSchema");
      Monstey_Check(
          it->at("read_only").as_bool() &&
              schema.at("required") == J::array({"path", "expected_sha256"}) &&
              schema.at("properties").at("bank_index").at("maximum").as_int() ==
                  5 &&
              schema.at("properties").at("limit").at("maximum").as_int() ==
                  32 &&
              schema.at("properties").at("expected_sha256").at("pattern") ==
                  J("^[0-9a-f]{64}$") &&
              !schema.at("additionalProperties").as_bool(),
          "Descriptor contract wrong");
      rows.erase(it);
      Monstey_Check(catalog == baseline,
                    "One of existing223 descriptors changed");
    });
    Monstey_Runtime runtime({}, out / L"absent-settings.json");
    if (withActual)
      test("runtime_actual_summary_readonly_without_database", [&] {
        const auto before = Monstey_Good(runtime, "describe");
        const auto reply = Monstey_Good(runtime, "database_native_banks", args);
        Monstey_Counters(before, reply);
        Monstey_Check(reply.at("result").at("mode") == J("bank_summary") &&
                          reply.at("result").at("source_unchanged").as_bool() &&
                          !reply.at("result").contains("source_path"),
                      "Runtime summary wrong");
        Monstey_Check(
            before.at("result") ==
                    Monstey_Good(runtime, "describe").at("result") &&
                !std::filesystem::exists(out / L"absent-settings.json"),
            "Runtime changed state or settings");
      });
    test("runtime_schema_stale_and_hash_errors_preserve_counters", [&] {
      const auto before = Monstey_Good(runtime, "describe");
      for (unsigned i = 0; i < (withActual ? 8u : 7u); ++i) {
        auto a = args;
        const char *code = "invalid_argument_type";
        if (i == 0) {
          a.as_object().erase("path");
          code = "missing_argument";
        }
        if (i == 1) {
          a["limit"] = 33;
          code = "argument_out_of_range";
        }
        if (i == 2)
          a["query"] = true;
        if (i == 3) {
          a["bank_index"] = 6;
          code = "argument_out_of_range";
        }
        if (i == 4) {
          a["output"] = "not-created";
          code = "unknown_argument";
        }
        if (i == 5) {
          a["expected_generation"] = 1;
          code = "stale_session";
        }
        if (i == 6) {
          a["expected_revision"] = 1;
          code = "stale_revision";
        }
        if (i == 7) {
          a["expected_sha256"] = std::string(64, '0');
          code = "source_hash_mismatch";
        }
        const auto reply =
            runtime.Call(Monstey_Request("database_native_banks", a));
        Monstey_Check(!reply.at("ok").as_bool() &&
                          reply.at("error").at("code") == J(code),
                      reply.dump().c_str());
        Monstey_Counters(before, reply);
        ++Monstey_RuntimeRejectionCases;
      }
    });
    if (withActual)
      test("native_mcp_class_filter_and_empty_bank", [&] {
        Monstey_Mcp m(runtime);
        (void)m.Handle(J::object(
            {{"jsonrpc", "2.0"}, {"id", 1}, {"method", "initialize"}}));
        for (unsigned b : {0u, 5u}) {
          auto a = args;
          a["bank_index"] = b;
          if (b == 0)
            a["query"] = "WorldUI.StorePredicate";
          const auto response = m.Handle(J::object(
              {{"jsonrpc", "2.0"},
               {"id", 2},
               {"method", "tools/call"},
               {"params", J::object({{"name", "database_native_banks"},
                                     {"arguments", a}})}}));
          Monstey_Check(response.has_value() &&
                            !response->at("result").at("isError").as_bool(),
                        "MCP operation failed");
          const auto reply = J::Parse(
              response->at("result").at("content")[0u].at("text").as_string());
          const auto &p = reply.at("result");
          Monstey_Check(reply.at("ok").as_bool() &&
                            p.at("total").as_uint() == (b == 0 ? 2u : 0u),
                        "MCP class result wrong");
          if (b == 0)
            Monstey_Check(
                p.at("rows")[0u].at("array_index").as_uint() == 2018 &&
                    p.at("rows")[1u].at("array_index").as_uint() == 2052 &&
                    p.at("rows")[1u]
                            .at("first_exact_byte_name_index")
                            .as_uint() == 2018,
                "MCP duplicate precedence lost");
        }
      });
    if (withActual)
      test("existing_editor_project_preserved", [&] {
        (void)Monstey_Good(runtime, "database_new");
        const auto before = Monstey_Good(runtime, "describe"),
                   changes = Monstey_Good(runtime, "database_changes");
        auto a = args;
        a["bank_index"] = 5;
        a["expected_generation"] = before.at("generation");
        a["expected_revision"] = before.at("revision");
        const auto reply = Monstey_Good(runtime, "database_native_banks", a);
        Monstey_Counters(before, reply);
        Monstey_Check(
            before.at("result") ==
                    Monstey_Good(runtime, "describe").at("result") &&
                changes.at("result") ==
                    Monstey_Good(runtime, "database_changes").at("result"),
            "Read changed editor project");
      });
    if (withActual)
      test("source_identity_unchanged_and_no_output_created", [&] {
        Monstey_Check(
            Monstey_HashSha256(source) == expected &&
                !std::filesystem::exists(out / L"absent-settings.json") &&
                !std::filesystem::exists(out / L"not-created"),
            "Read wrote state");
      });
    Monstey_Check(Monstey_RejectionCases == 16,
                  "Direct rejection cases skipped");
    Monstey_Check(Monstey_RuntimeRejectionCases == (withActual ? 8u : 7u),
                  "Runtime rejection cases skipped");
    Monstey_WriteNewText(
        out / L"report.json",
        J::object({{"schema", "Monstey.GadbBankApiTests/1"},
                   {"status", "PASS"},
                   {"groups", groups},
                   {"count", groups.size()},
                   {"direct_rejection_cases", Monstey_RejectionCases},
                   {"runtime_rejection_cases", Monstey_RuntimeRejectionCases},
                   {"actual_classes", withActual ? 3151 : 0},
                   {"maximum_page_bytes", maximumPageBytes},
                   {"catalog_operations", 227},
                   {"previous_descriptors_unchanged", 223},
                   {"runtime_verified", false},
                   {"original_calls", false},
                   {"source_sha256", expected}})
                .dump(2) +
            "\n");
    std::cout << "PASS " << groups.size() << " bank API groups, maximum page "
              << maximumPageBytes << " bytes\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}