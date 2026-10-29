#include "Monstey_GadbBanks.hpp"
#include <algorithm>
#include <array>
#include <limits>
namespace Monstey {
namespace {
using J = Monstey_Json;
constexpr std::size_t Monstey_BankPageMaximum = 32;
std::string Monstey_BankSearchKey(std::string_view text) {
  std::string result(text);
  for (auto &c : result)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + ('a' - 'A'));
  return result;
}
void Monstey_BankArguments(const J &args, bool file) {
  Monstey_Require(args.is_object(),
                  "Bank inspection arguments must be an object");
  for (const auto &[key, unused] : args.as_object()) {
    (void)unused;
    const bool page = key == "bank_index" || key == "query" ||
                      key == "offset" || key == "limit";
    if (!page && !(file && (key == "path" || key == "expected_sha256")))
      throw Monstey_Error("invalid_arguments",
                          "Unknown bank inspection argument: " + key);
  }
}
std::uint64_t Monstey_BankUnsigned(const J &args, std::string_view key,
                                   std::uint64_t fallback,
                                   std::uint64_t maximum) {
  if (!args.contains(key))
    return fallback;
  if (!args.at(key).is_integer())
    throw Monstey_Error("invalid_argument_type",
                        "Bank page indices and limits must be integers");
  const auto value = args.at(key).as_uint();
  if (value > maximum)
    throw Monstey_Error("invalid_arguments",
                        "Bank page index or limit exceeds its bound");
  return value;
}
J Monstey_BankPick(const J &source,
                   std::initializer_list<std::string_view> keys) {
  auto result = J::object();
  for (const auto key : keys)
    if (source.contains(key))
      result[std::string(key)] = source.at(key);
  return result;
}
} // namespace
J Monstey_GadbBankPage(const J &report, const J &args) {
  Monstey_BankArguments(args, false);
  Monstey_Require(
      report.at("schema").as_string() == "Monstey.GadbBankRegistration/1" &&
          report.at("banks").is_array() && report.at("banks").size() == 6,
      "Expected native bank registration report");
  const bool selected = args.contains("bank_index");
  const auto index = Monstey_BankUnsigned(args, "bank_index", 0, 5);
  const auto offset = Monstey_BankUnsigned(
      args, "offset", 0, std::numeric_limits<std::uint64_t>::max());
  const auto limit = Monstey_BankUnsigned(
      args, "limit", Monstey_BankPageMaximum, Monstey_BankPageMaximum);
  if (limit == 0)
    throw Monstey_Error("invalid_arguments",
                        "Bank page limit must be positive");
  const auto query = args.value("query", J("")).as_string();
  if (query.size() > 512)
    throw Monstey_Error("invalid_arguments",
                        "Bank query exceeds 512 UTF8 bytes");
  const bool page = selected || args.contains("query") ||
                    args.contains("offset") || args.contains("limit");
  const auto needle = Monstey_BankSearchKey(query);
  auto metadata = Monstey_BankPick(
      report, {"schema",
               "status",
               "PE_sha256",
               "semantic_map_sha256",
               "semantic_map_required",
               "compiled_profile_sha256",
               "native_anchor_check_model",
               "class_pointer_assignments",
               "unique_class_pointers",
               "unique_exact_byte_full_names",
               "native_code_anchor_checks",
               "binder_class_name_source",
               "native_bank_and_class_selection",
               "native_nonnull_name_comparator",
               "ASCII_fold_is_only_a_diagnostic_not_native_equivalence",
               "actual_prior_initialization_and_binding_effects",
               "runtime_BSS_contents_verified",
               "original_code_executed",
               "game_memory_accessed",
               "runtime_verified",
               "checker_promoted"});
  J summaries = J::array(), rows = J::array();
  std::uint64_t total = 0;
  for (const auto &bank : report.at("banks").as_array()) {
    auto summary = Monstey_BankPick(
        bank, {"bank_index", "name", "entry_address", "entry_sha256",
               "enumerator", "returned_count", "returned_array",
               "array_values_source", "runtime_array_contents_verified"});
    summary["exact_duplicate_name_pairs"] =
        bank.at("exact_byte_duplicate_full_names").size();
    summaries.as_array().push_back(std::move(summary));
    if (!page || (selected && bank.at("bank_index").as_uint() != index))
      continue;
    for (const auto &row : bank.at("classes").as_array()) {
      if (!needle.empty() &&
          Monstey_BankSearchKey(row.at("full_name").as_string()).find(needle) ==
              std::string::npos &&
          Monstey_BankSearchKey(
              row.at("short_name_at_class_plus_10").as_string())
                  .find(needle) == std::string::npos)
        continue;
      if (total >= offset && rows.size() < limit) {
        auto copy =
            Monstey_BankPick(row, {"array_index",
                                   "class_pointer",
                                   "class_pointer_source",
                                   "bank_slot",
                                   "full_name",
                                   "full_name_pointer_at_class_plus_20",
                                   "short_name_at_class_plus_10",
                                   "field_table",
                                   "field_count_u32",
                                   "descriptor_qword_plus_08",
                                   "descriptor_qword_plus_30",
                                   "descriptor_qword_plus_38",
                                   "parent_pointer_at_class_plus_28",
                                   "class_flags_u32_plus_38",
                                   "captured_file_initialized_byte_plus_3c",
                                   "descriptor_sha256",
                                   "introspection_status",
                                   "bank_membership_status",
                                   "unsupported_by_legacy_zero_30_shape",
                                   "opaque_class_pointer_30_role",
                                   "opaque_class_pointer_30_head_sha256",
                                   "parent_full_name",
                                   "first_exact_byte_name_index"});
        copy["bank_index"] = bank.at("bank_index");
        copy["bank_name"] = bank.at("name");
        rows.as_array().push_back(std::move(copy));
      }
      ++total;
    }
  }
  const auto next = offset < total && rows.size() < total - offset
                        ? J(offset + rows.size())
                        : J(nullptr);
  return J::object(
      {{"format", "Monstey.GadbBankInspection/1"},
       {"metadata", metadata},
       {"mode", page ? "class_page" : "bank_summary"},
       {"banks", summaries},
       {"bank_index", selected ? J(index) : J(nullptr)},
       {"query", query},
       {"query_comparison", "ASCII-insensitive display filter; native name "
                            "comparison remains unqualified"},
       {"offset", offset},
       {"limit", limit},
       {"total", total},
       {"next_offset", next},
       {"rows", rows},
       {"raw_native_indices_preserved", true},
       {"read_only", true},
       {"runtime_verified", false}});
}
J Monstey_GadbBankInspect(const J &args) {
  Monstey_BankArguments(args, true);
  auto pageArgs =
      Monstey_BankPick(args, {"bank_index", "query", "offset", "limit"});
  // Validate bounds before reading a potentially large image.
  (void)Monstey_BankUnsigned(pageArgs, "bank_index", 0, 5);
  (void)Monstey_BankUnsigned(pageArgs, "offset", 0,
                             std::numeric_limits<std::uint64_t>::max());
  if (Monstey_BankUnsigned(pageArgs, "limit", Monstey_BankPageMaximum,
                           Monstey_BankPageMaximum) == 0)
    throw Monstey_Error("invalid_arguments",
                        "Bank page limit must be positive");
  if (pageArgs.contains("query") &&
      (!pageArgs.at("query").is_string() ||
       pageArgs.at("query").as_string().size() > 512))
    throw Monstey_Error("invalid_arguments",
                        "Bank query must be a bounded string");
  const auto &expected = args.at("expected_sha256").as_string();
  if (expected.size() != 64 ||
      !std::all_of(expected.begin(), expected.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
      }))
    throw Monstey_Error(
        "invalid_arguments",
        "Expected SHA256 must be 64 lowercase hexadecimal characters");
  const auto &text = args.at("path").as_string();
  if (text.empty())
    throw Monstey_Error("invalid_arguments", "Select a source PE path");
  const std::filesystem::path source(Monstey_Wide(text));
  const auto image = Monstey_ReadFile(source, 256ull * 1024 * 1024);
  if (Monstey_HashSha256(image) != expected)
    throw Monstey_Error("source_hash_mismatch", "Source PE identity differs");
  auto result =
      Monstey_GadbBankPage(Monstey_GadbBankRegistration(image), pageArgs);
  if (Monstey_HashSha256(source) != expected)
    throw Monstey_Error("source_hash_mismatch",
                        "Source changed during bank inspection");
  result["source_file_name"] = Monstey_Utf8(source.filename().wstring());
  result["source_unchanged"] = true;
  return result;
}
} // namespace Monstey