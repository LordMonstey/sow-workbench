#include "Monstey_MaterialProject.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <windows.h>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
void Monstey_Check(bool b, const char *s) {
  if (!b)
    throw Monstey_Error("ltmi_tool_invalid", s);
}
void Monstey_Keys(const J &value,
                  std::initializer_list<std::string_view> keys) {
  Monstey_Check(value.is_object(), "Expected JSON object");
  for (const auto &[key, unused] : value.as_object()) {
    (void)unused;
    Monstey_Check(std::find(keys.begin(), keys.end(), key) != keys.end(),
                  "Unknown LTMI edit field");
  }
}
std::string Monstey_LtmiExpectedHash(std::string value) {
  Monstey_Check(value.size() == 64,
                "Expected source SHA256 must contain64 hexadecimal digits");
  for (auto &c : value) {
    if (c >= 'A' && c <= 'F')
      c = static_cast<char>(c - 'A' + 'a');
    Monstey_Check((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'),
                  "Expected source SHA256 contains non-hexadecimal data");
  }
  return value;
}
void Monstey_OutputPair(const std::filesystem::path &output,
                        const std::filesystem::path &report) {
  auto normalize = [](const auto &p) {
    return Monstey_WindowsFilePath(
               std::filesystem::weakly_canonical(std::filesystem::absolute(p)))
        .lexically_normal();
  };
  const auto a = normalize(output), b = normalize(report);
  auto ancestor = [](const auto &parent, const auto &child) {
    auto p = parent.begin(), c = child.begin();
    for (; p != parent.end(); ++p, ++c) {
      if (c == child.end())
        return false;
      const auto x = p->native(), y = c->native();
      if (CompareStringOrdinal(x.c_str(), -1, y.c_str(), -1, TRUE) !=
          CSTR_EQUAL)
        return false;
    }
    return true;
  };
  Monstey_Check(!ancestor(a, b) && !ancestor(b, a),
                "Material and report paths must be distinct files with no "
                "mutual ancestry");
}
J Monstey_Inspect(const std::filesystem::path &source) {
  const auto bytes = Monstey_ReadFile(source);
  auto m = Monstey_LtmiRead(bytes);
  auto report = Monstey_LtmiDescribe(m);
  Monstey_Check(Monstey_HashSha256(source) == Monstey_HashSha256(bytes),
                "Material changed during inspection");
  report["path"] = Monstey_Utf8(source.wstring());
  report["source_unchanged"] = true;
  return report;
}
J Monstey_Census(const std::filesystem::path &list,
                 const std::filesystem::path &output) {
  Monstey_Check(!std::filesystem::exists(output), "Census output must be new");
  const auto manifest = J::Parse(Monstey_ReadText(list));
  Monstey_Check(manifest.is_array(),
                "Material input manifest must be an array");
  auto files = J::array(), errors = J::array(), types = J::object(),
       versions = J::object();
  std::uint64_t properties = 0, largest = 0;
  unsigned passed = 0;
  for (const auto &row : manifest.as_array()) {
    auto source = std::filesystem::path(Monstey_Wide(row.as_string()));
    const auto bytes = Monstey_ReadFile(source);
    const auto hash = Monstey_HashSha256(bytes);
    try {
      auto m = Monstey_LtmiRead(bytes);
      auto rebuilt = Monstey_LtmiBuild(m);
      Monstey_Check(rebuilt == bytes, "Material no-op changed bytes");
      auto key = std::to_string(m.version);
      versions[key] = versions.contains(key) ? versions[key].as_uint() + 1 : 1;
      std::uint64_t count = 0;
      for (const auto &table : m.tables)
        for (const auto &p : table) {
          key = std::to_string(p.type) + ":" + std::to_string(p.flag_a) + ":" +
                std::to_string(p.flag_b);
          types[key] = types.contains(key) ? types[key].as_uint() + 1 : 1;
          ++count;
        }
      properties += count;
      largest = std::max(largest, count);
      files.as_array().push_back(
          J::object({{"path", row},
                     {"sha256", hash},
                     {"bytes", static_cast<std::uint64_t>(bytes.size())},
                     {"version", m.version},
                     {"properties", count},
                     {"native_shader_name_truncated",
                      Monstey_LtmiDescribe(m)["native_shader_name_truncated"]},
                     {"no_op_byte_exact", true}}));
      ++passed;
    } catch (const std::exception &error) {
      errors.as_array().push_back(J::object(
          {{"path", row}, {"sha256", hash}, {"error", error.what()}}));
    }
    Monstey_Check(Monstey_HashSha256(source) == hash, "Census source changed");
  }
  auto report =
      J::object({{"schema", "Monstey.LtmiCensus/1"},
                 {"sources", static_cast<std::uint64_t>(manifest.size())},
                 {"passed", passed},
                 {"failed", static_cast<std::uint64_t>(errors.size())},
                 {"properties", properties},
                 {"largest_property_count", largest},
                 {"versions", versions},
                 {"type_flag_counts", types},
                 {"files", files},
                 {"errors", errors},
                 {"sources_unchanged", true},
                 {"game_written", false},
                 {"original_program_executed", false}});
  Monstey_WriteNewText(output, report.dump(2) + "\n");
  return report;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    if (argc == 4 && std::wstring_view(argv[1]) == L"inspect") {
      auto report = Monstey_Inspect(argv[2]);
      Monstey_WriteNewText(argv[3], report.dump(2) + "\n");
      std::cout << "LTMI inspection PASS\n";
      return 0;
    }
    if (argc == 4 && std::wstring_view(argv[1]) == L"census") {
      auto r = Monstey_Census(argv[2], argv[3]);
      std::cout << r["passed"].as_uint() << " materials passed; "
                << r["failed"].as_uint() << " outside profile\n";
      return r["failed"].as_uint() ? 1 : 0;
    }
    if (argc == 6 && std::wstring_view(argv[1]) == L"edit") {
      const std::filesystem::path source = argv[2], spec = argv[3],
                                  output = argv[4], reportPath = argv[5];
      const auto before = Monstey_ReadFile(source);
      const auto hash = Monstey_HashSha256(before);
      const auto specBytes = Monstey_ReadFile(spec);
      const auto specHash = Monstey_HashSha256(specBytes);
      auto document = Monstey_LtmiRead(before);
      auto edits = J::Parse(std::string(
          reinterpret_cast<const char *>(specBytes.data()), specBytes.size()));
      Monstey_Keys(edits, {"operations", "expected_source_sha256"});
      if (edits.contains("expected_source_sha256"))
        Monstey_Check(Monstey_LtmiExpectedHash(
                          edits["expected_source_sha256"].as_string()) == hash,
                      "Expected material SHA256 differs");
      const std::array<std::filesystem::path, 2> sources{source, spec};
      Monstey_RequireOfflineOutput(output, sources);
      Monstey_RequireOfflineOutput(reportPath, sources);
      Monstey_Check(!std::filesystem::exists(output) &&
                        !std::filesystem::exists(reportPath),
                    "Both edit output paths must be new");
      Monstey_OutputPair(output, reportPath);
      Monstey_LtmiApplyOperations(document, edits.at("operations"));
      const auto built = Monstey_LtmiBuild(document);
      auto report = Monstey_LtmiDescribe(document);
      Monstey_Check(Monstey_LtmiBuild(Monstey_LtmiRead(built)) == built,
                    "Edited material readback differs");
      report["built_readback_byte_exact"] = true;
      Monstey_Check(Monstey_HashSha256(source) == hash &&
                        Monstey_HashSha256(spec) == specHash,
                    "Edit source changed before publication");
      Monstey_WriteNewFile(output, built);
      Monstey_Check(Monstey_ReadFile(output) == built &&
                        Monstey_HashSha256(source) == hash &&
                        Monstey_HashSha256(spec) == specHash,
                    "Post-publication verification failed; destination was not "
                    "removed or restored");
      report["source_sha256"] = hash;
      report["output"] = Monstey_Utf8(output.wstring());
      report["output_sha256"] = Monstey_HashSha256(built);
      report["spec_sha256"] = specHash;
      report["source_unchanged"] = true;
      report["operations"] =
          static_cast<std::uint64_t>(edits["operations"].size());
      Monstey_WriteNewText(reportPath, report.dump(2) + "\n");
      std::cout << "LTMI edit/readback PASS\n";
      return 0;
    }
    std::cerr << "Usage: LtmiTool inspect input.mat NEW.json | census "
                 "manifest.json NEW.json | edit input.mat "
                 "spec.json NEW.mat NEW-report.json\n";
    return 2;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}