#include "Monstey_GadbClassDeclarationApi.hpp"
#include "Monstey_GadbClassDeclaration.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
namespace Monstey {
namespace {
using J = Monstey_Json;
constexpr std::size_t Monstey_ClassPayloadMaximum = 28 * 1024;
constexpr std::uint64_t Monstey_ClassPageMaximum = 32;
constexpr std::array<std::string_view, 2> Monstey_ClassImages{
    "6b8db778135c082c934463b6524e6b220478fb6987f082ab37ca48a870acb455",
    "ed0d95cabb033cd66aaa99d699374881846c3cb03ec345f1123d82d45e0bdaef"};
void Monstey_ClassArguments(const J &a, bool file) {
  if (!a.is_object())
    throw Monstey_Error("invalid_arguments",
                        "Class arguments must be an object");
  for (const auto &[key, unused] : a.as_object()) {
    (void)unused;
    if (key != "class_pointer" && key != "offset" && key != "limit" &&
        !(file && (key == "path" || key == "expected_sha256")))
      throw Monstey_Error("invalid_arguments",
                          "Unknown native class argument: " + key);
  }
}
std::uint64_t Monstey_ClassPointer(const J &value) {
  std::uint64_t out = 0;
  if (value.is_integer())
    out = value.as_uint();
  else if (value.is_string()) {
    std::string_view text = value.as_string();
    if (text.starts_with("0x") || text.starts_with("0X"))
      text.remove_prefix(2);
    if (text.empty() || text.size() > 16)
      throw Monstey_Error("invalid_arguments",
                          "Class pointer is not bounded hexadecimal");
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), out, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
      throw Monstey_Error("invalid_arguments",
                          "Class pointer is not hexadecimal");
  } else
    throw Monstey_Error(
        "invalid_argument_type",
        "Class pointer must be an integer or hexadecimal string");
  if (!out)
    throw Monstey_Error("invalid_arguments", "Class pointer must be nonzero");
  return out;
}
std::uint64_t Monstey_ClassUnsigned(const J &a, std::string_view key,
                                    std::uint64_t fallback,
                                    std::uint64_t maximum) {
  if (!a.contains(key))
    return fallback;
  if (!a.at(key).is_integer())
    throw Monstey_Error("invalid_argument_type",
                        "Class offset/limit must be integers");
  const auto out = a.at(key).as_uint();
  if (out > maximum || (key == "limit" && out == 0))
    throw Monstey_Error("invalid_arguments",
                        "Class page index or limit exceeds its bound");
  return out;
}
J Monstey_ClassPick(const J &j, std::initializer_list<std::string_view> keys) {
  auto out = J::object();
  for (const auto key : keys)
    if (j.contains(key))
      out[std::string(key)] = j.at(key);
  return out;
}
std::size_t Monstey_ClassSerialized(J &result) {
  for (unsigned i = 0; i < 4; ++i) {
    const auto bytes = result.dump().size();
    if (result.at("serialized_bytes").as_uint() == bytes)
      return bytes;
    result["serialized_bytes"] = static_cast<std::uint64_t>(bytes);
  }
  return result.dump().size();
}
void Monstey_ClassPageState(J &result, std::uint64_t offset,
                            std::uint64_t total) {
  const auto count = static_cast<std::uint64_t>(result.at("rows").size());
  const bool more = offset < total && count < total - offset;
  result["returned"] = count;
  result["has_more"] = more;
  result["next_offset"] = more ? J(offset + count) : J(nullptr);
}
} // namespace
J Monstey_GadbNativeClassPage(const J &report, const J &a) {
  Monstey_ClassArguments(a, false);
  const auto selected = Monstey_ClassPointer(a.at("class_pointer"));
  const auto offset = Monstey_ClassUnsigned(a, "offset", 0, 4096);
  const auto limit =
      Monstey_ClassUnsigned(a, "limit", 16, Monstey_ClassPageMaximum);
  if (report.at("schema").as_string() !=
          "Monstey.GadbExplicitClassDeclarations/1" ||
      !report.at("classes").is_array())
    throw Monstey_Error("invalid_arguments",
                        "Expected explicit class declaration report");
  const J *row = nullptr;
  for (const auto &candidate : report.at("classes").as_array())
    if (Monstey_ClassPointer(candidate.at("class_pointer")) == selected) {
      row = &candidate;
      break;
    }
  if (!row)
    throw Monstey_Error("invalid_arguments",
                        "Selected class pointer is absent from report");
  const auto &fields = row->at("fields").as_array();
  const auto total = static_cast<std::uint64_t>(fields.size());
  auto metadata = Monstey_ClassPick(
      *row,
      {"class_pointer", "raw64_hex", "raw64_sha256", "field_table_pointer",
       "field_count_u32", "short_name", "description", "full_name",
       "parent_pointer", "opaque_class_pointer_30", "class_30_role",
       "flags_u32", "captured_initialized_u8", "field_storage_status",
       "field_storage_error"});
  auto result =
      J::object({{"format", "Monstey.GadbNativeClassInspection/1"},
                 {"source_sha256", report.at("source_sha256")},
                 {"class", metadata},
                 {"offset", offset},
                 {"limit", limit},
                 {"total", total},
                 {"rows", J::array()},
                 {"returned", 0},
                 {"has_more", false},
                 {"next_offset", nullptr},
                 {"serialized_bytes", 0},
                 {"maximum_serialized_bytes",
                  static_cast<std::uint64_t>(Monstey_ClassPayloadMaximum)},
                 {"identity", "EXACT_CLASS_POINTER"},
                 {"parent_fields_included", false},
                 {"class_name_selection", "NOT_PERFORMED"},
                 {"read_only", true},
                 {"runtime_verified", false},
                 {"original_code_executed", false}});
  for (const auto key : {"source_file_name", "source_unchanged"})
    if (report.contains(key))
      result[key] = report.at(key);
  Monstey_ClassPageState(result, offset, total);
  if (Monstey_ClassSerialized(result) > Monstey_ClassPayloadMaximum)
    throw Monstey_Error("native_class_response_limit",
                        "Class metadata exceeds the bounded response size");
  if (offset < total)
    for (std::uint64_t i = offset;
         i < total && result.at("rows").size() < limit; ++i) {
      result["rows"].as_array().push_back(fields[static_cast<std::size_t>(i)]);
      Monstey_ClassPageState(result, offset, total);
      if (Monstey_ClassSerialized(result) <= Monstey_ClassPayloadMaximum)
        continue;
      result["rows"].as_array().pop_back();
      Monstey_ClassPageState(result, offset, total);
      (void)Monstey_ClassSerialized(result);
      if (result.at("rows").size() == 0)
        throw Monstey_Error(
            "native_class_response_limit",
            "A complete field row exceeds the bounded response size");
      break;
    }
  return result;
}
J Monstey_GadbNativeClassInspect(const J &a) {
  Monstey_ClassArguments(a, true);
  const auto pointer = Monstey_ClassPointer(a.at("class_pointer"));
  (void)Monstey_ClassUnsigned(a, "offset", 0, 4096);
  (void)Monstey_ClassUnsigned(a, "limit", 16, Monstey_ClassPageMaximum);
  const auto &expected = a.at("expected_sha256").as_string();
  if (std::find(Monstey_ClassImages.begin(), Monstey_ClassImages.end(),
                expected) == Monstey_ClassImages.end())
    throw Monstey_Error(
        "unsupported_native_image",
        "Native class inspection requires an exact supported image SHA256");
  const auto &path = a.at("path").as_string();
  if (path.empty() || path.size() > 32768 || path.find('\0') != path.npos)
    throw Monstey_Error("invalid_path", "Native class source path must be "
                                        "nonempty, bounded and contain no NUL");
  const std::filesystem::path source(Monstey_Wide(path));
  const auto image = Monstey_ReadFile(source, 256ull * 1024 * 1024);
  if (Monstey_HashSha256(image) != expected)
    throw Monstey_Error("source_hash_mismatch",
                        "Native class source identity differs");
  auto report =
      Monstey_GadbInspectClassPointers(image, std::array{pointer}, expected);
  report["source_file_name"] = Monstey_Utf8(source.filename().wstring());
  report["source_unchanged"] = true;
  auto result = Monstey_GadbNativeClassPage(
      report, Monstey_ClassPick(a, {"class_pointer", "offset", "limit"}));
  if (Monstey_HashSha256(source) != expected)
    throw Monstey_Error("source_hash_mismatch",
                        "Source changed during native class inspection");
  return result;
}
} // namespace Monstey
