#include "Monstey_Gadb.hpp"
#include "Monstey_GadbClassCache.hpp"
#include <array>
#include <chrono>
#include <iostream>

using namespace Monstey;
int wmain(int argc, wchar_t **argv) {
  try {
    // Only the new five-argument form selects trace; all valid legacy
    // six-argument calls remain unchanged.
    if (argc == 5 && std::wstring_view(argv[1]) == L"trace") {
      const std::filesystem::path inputPath(argv[2]), outputPath(argv[4]);
      Monstey_RequireOfflineOutput(outputPath, std::array{inputPath});
      Monstey_Require(
          !std::filesystem::exists(Monstey_WindowsFilePath(outputPath)),
          "Report output already exists");
      const auto bytes = Monstey_ReadFile(inputPath, 32ull * 1024 * 1024);
      const auto hash = Monstey_HashSha256(bytes);
      Monstey_Require(hash == Monstey_Utf8(argv[3]),
                      "Trace input SHA256 mismatch");
      const auto input =
          Monstey_Json::Parse(std::string(bytes.begin(), bytes.end()));
      auto result = Monstey_GadbClassCacheTrace(input);
      result["input_file_sha256"] = hash;
      Monstey_Require(Monstey_HashSha256(inputPath) == hash,
                      "Trace input changed before publication");
      Monstey_WriteNewText(outputPath, result.dump(2) + "\n");
      std::cout << "trace_status=" << result.at("status").as_string() << '\n';
      return 0;
    }
    Monstey_Require(argc == 6,
                    "Usage: GadbClassCacheTool declarations.json "
                    "expected_sha256 database expected_sha256 new_report.json");
    const std::filesystem::path declarationsPath(argv[1]),
        databasePath(argv[3]), outputPath(argv[5]);
    Monstey_RequireOfflineOutput(outputPath,
                                 std::array{declarationsPath, databasePath});
    Monstey_Require(
        !std::filesystem::exists(Monstey_WindowsFilePath(outputPath)),
        "Report output already exists");
    const auto started = std::chrono::steady_clock::now();
    const auto declarationBytes =
        Monstey_ReadFile(declarationsPath, 128 * 1024 * 1024);
    const auto declarationHash = Monstey_HashSha256(declarationBytes);
    Monstey_Require(declarationHash == Monstey_Utf8(argv[2]),
                    "Declaration SHA256 mismatch");
    const auto declarations = Monstey_Json::Parse(
        std::string(declarationBytes.begin(), declarationBytes.end()));
    const Monstey_GadbClassCache checker(declarations);
    const Monstey_Gadb database(databasePath);
    const auto description = database.Describe();
    const auto databaseHash = description.at("source_sha256").as_string();
    Monstey_Require(databaseHash == Monstey_Utf8(argv[4]),
                    "Database SHA256 mismatch");
    Monstey_Require(!description.at("native_dirty").as_bool(),
                    "Audit requires a clean file-backed source");
    const auto categories = database.Categories();
    Monstey_Require(categories.at("rows").size() <= 4096,
                    "Native declaration checker category limit exceeded");
    auto bindingCounts = Monstey_Json::object();
    for (const auto &category : categories.at("rows").as_array()) {
      const auto &name = category.at("name").as_string();
      bindingCounts[name] = bindingCounts.contains(name)
                                ? bindingCounts.at(name).as_uint() + 1
                                : 1;
    }
    auto categoryResults = Monstey_Json::array(),
         diagnostics = Monstey_Json::array();
    auto totals = Monstey_Json::object();
    constexpr std::array counters{
        "qualified_categories",    "unknown_categories",
        "unsafe_categories",       "records_checked",
        "descriptor_rows_checked", "field_predicates_checked",
        "cached_slot_hazards",     "unqualified_value_contracts",
        "diagnostics_total"};
    for (const auto key : counters)
      totals[key] = 0;
    for (const auto &category : categories.at("rows").as_array()) {
      auto records = Monstey_Json::array();
      const auto index = category.at("index").as_uint();
      Monstey_Require(index <= UINT32_MAX,
                      "Category index exceeds native width");
      for (std::size_t offset = 0;;) {
        auto page = database.DescriptorLayouts(
            static_cast<std::uint32_t>(index), offset, 10000);
        Monstey_Require(page.at("published_order_provenance").as_string() ==
                                "native_source" &&
                            !page.at("values_decoded").as_bool(),
                        "Descriptor audit requires final native source order "
                        "without value decoding");
        const auto count = page.at("rows").size();
        for (auto &record : page["rows"].as_array())
          records.as_array().push_back(std::move(record));
        offset += count;
        if (offset >= page.at("total").as_uint())
          break;
        Monstey_Require(count != 0, "Descriptor pagination did not advance");
      }
      const auto result = checker.Check(Monstey_Json::object(
          {{"format", "Monstey_GadbClassCacheLayout"},
           {"version", 1},
           {"source_sha256", databaseHash},
           {"binding_name_counts", bindingCounts},
           {"categories", Monstey_Json::array({Monstey_Json::object(
                              {{"name", category.at("name")},
                               {"schema", category.at("schema")},
                               {"records", records}})})}}));
      for (const auto &row : result.at("categories").as_array())
        categoryResults.as_array().push_back(row);
      for (const auto &row : result.at("diagnostics").as_array())
        if (diagnostics.size() < 256)
          diagnostics.as_array().push_back(row);
      for (const auto key : counters)
        totals[key] = totals.at(key).as_uint() + result.at(key).as_uint();
      if (categoryResults.size() % 100 == 0)
        std::cout << "checked_categories=" << categoryResults.size()
                  << " descriptors="
                  << totals.at("descriptor_rows_checked").as_uint()
                  << std::endl;
    }
    Monstey_Require(Monstey_HashSha256(databasePath) == databaseHash &&
                        Monstey_HashSha256(declarationsPath) == declarationHash,
                    "Audit source changed before report publication");
    auto report = Monstey_Json::object(
        {{"format", "Monstey_GadbClassCacheAudit"},
         {"version", 1},
         {"status", totals.at("unsafe_categories").as_uint()    ? "unsafe"
                    : totals.at("unknown_categories").as_uint() ? "unknown"
                                                                : "qualified"},
         {"scope", "selected_direct_first_record_binder_and_cached_physical_"
                   "descriptor_layout"},
         {"database", Monstey_Utf8(databasePath.wstring())},
         {"database_sha256", databaseHash},
         {"declarations", Monstey_Utf8(declarationsPath.wstring())},
         {"declarations_sha256", declarationHash},
         {"native_image_sha256", declarations.value("source_sha256", nullptr)},
         {"category_count", categoryResults.size()},
         {"categories", categoryResults},
         {"diagnostics", diagnostics},
         {"diagnostics_truncated",
          totals.at("diagnostics_total").as_uint() > diagnostics.size()},
         {"totals", totals},
         {"layout_stage", "native_file_backed_source"},
         {"values_decoded", false},
         {"assumes_unbound_field_cache", true},
         {"shared_ancestor_state_simulated", false},
         {"runtime_verified", false},
         {"engine_executed", false},
         {"game_or_source_modified", false},
         {"original_inputs_unchanged", true},
         {"elapsed_ms", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - started)
                            .count()}});
    const auto text = report.dump(2) + "\n";
    Monstey_WriteNewText(outputPath, text);
    std::cout << "status=" << report.at("status").as_string()
              << " categories=" << categoryResults.size()
              << " hazards=" << totals.at("cached_slot_hazards").as_uint()
              << '\n';
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}