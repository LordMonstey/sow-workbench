#include "Monstey_NavigationWorld.hpp"
#include <algorithm>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
void Monstey_NWKeys(const J &a, std::initializer_list<std::string_view> keys) {
  Monstey_Require(a.is_object(), "Navigation arguments must be object");
  for (const auto &[key, value] : a.as_object()) {
    (void)value;
    Monstey_Require(std::find(keys.begin(), keys.end(), key) != keys.end(),
                    "Unknown navigation argument " + key);
  }
}
std::filesystem::path Monstey_NWPath(const J &x) {
  Monstey_Require(x.is_string() && !x.as_string().empty() &&
                      x.as_string().find('\0') == std::string::npos,
                  "Expected explicit navigation path");
  return std::filesystem::absolute(
             std::filesystem::path(Monstey_Wide(x.as_string())))
      .lexically_normal();
}
std::wstring Monstey_NWKey(const std::filesystem::path &p) {
  auto key = std::filesystem::weakly_canonical(p).wstring();
  for (auto &c : key)
    if (c >= L'A' && c <= L'Z')
      c = wchar_t(c + 32);
  return key;
}
std::filesystem::path Monstey_NWLong(const std::filesystem::path &p) {
  auto w = std::filesystem::absolute(p).wstring();
  if (w.starts_with(L"\\\\?\\"))
    return p;
  if (w.starts_with(L"\\\\"))
    return std::filesystem::path(L"\\\\?\\UNC\\" + w.substr(2));
  return std::filesystem::path(L"\\\\?\\" + w);
}
struct Monstey_NWSource {
  std::filesystem::path path;
  B bytes;
  std::string sha;
};
void Monstey_NWCheckSources(const std::vector<Monstey_NWSource> &sources) {
  for (const auto &s : sources)
    Monstey_Require(Monstey_HashSha256(s.path) == s.sha,
                    "Navigation input changed during operation: " +
                        Monstey_Utf8(s.path.wstring()));
}
} // namespace
J Monstey_NavigationWorldCall(std::string_view op, const J &a) {
  Monstey_Require(op == "sow_world_navigation_inspect" ||
                      op == "sow_world_navigation_apply" ||
                      op == "sow_world_navigation_build",
                  "Unknown native navigation operation");
  const bool inspect = op == "sow_world_navigation_inspect",
             apply = op == "sow_world_navigation_apply";
  if (inspect)
    Monstey_NWKeys(a, {"source", "owner"});
  else if (apply)
    Monstey_NWKeys(
        a, {"source", "owner", "spec", "output", "owner_output", "report"});
  else
    Monstey_NWKeys(a, {"source", "owner", "owner_expected_sha256",
                       "cache_template", "spec", "output", "report"});
  std::vector<Monstey_NWSource> sources;
  auto read = [&](const J &arg) {
    const auto path = Monstey_NWPath(arg);
    const auto bytes = Monstey_ReadFile(path, 256u * 1024 * 1024);
    sources.push_back({path, bytes, Monstey_HashSha256(bytes)});
    return sources.size() - 1;
  };
  const auto si = read(a.at("source"));
  std::optional<std::size_t> oi, ci;
  if (a.contains("owner") && !a.at("owner").is_null())
    oi = read(a.at("owner"));
  if (a.contains("cache_template") && !a.at("cache_template").is_null())
    ci = read(a.at("cache_template"));
  if (!inspect && !apply && oi) {
    Monstey_Require(a.contains("owner_expected_sha256") &&
                        a.at("owner_expected_sha256").as_string() ==
                            sources[*oi].sha,
                    "Navigation owner requires matching owner_expected_sha256");
  }
  if (ci)
    Monstey_Require(a.at("spec").contains("cache_template_sha256") &&
                        a.at("spec").at("cache_template_sha256").as_string() ==
                            sources[*ci].sha,
                    "Navigation cache template requires matching "
                    "spec.cache_template_sha256");
  const auto &source = sources[si].bytes;
  const auto owner = oi ? std::span<const std::uint8_t>(sources[*oi].bytes)
                        : std::span<const std::uint8_t>();
  if (inspect) {
    auto report = Monstey_NavigationInspect(source, owner);
    Monstey_NWCheckSources(sources);
    report["source"] = Monstey_Utf8(sources[si].path.wstring());
    if (oi) {
      report["owner_source"] = Monstey_Utf8(sources[*oi].path.wstring());
      report["owner_source_sha256"] = sources[*oi].sha;
    }
    report["source_unchanged"] = true;
    return report;
  }
  Monstey_Require(!apply || oi.has_value(),
                  "Navigation apply requires matching owner OBJRES");
  std::vector<std::filesystem::path> protectedPaths;
  for (const auto &s : sources)
    protectedPaths.push_back(s.path);
  std::vector<std::pair<std::filesystem::path, B>> outputs;
  const auto navOutput = Monstey_NWPath(a.at("output"));
  std::optional<std::filesystem::path> ownerOutput, reportOutput;
  if (apply)
    ownerOutput = Monstey_NWPath(a.at("owner_output"));
  if (a.contains("report") && !a.at("report").is_null())
    reportOutput = Monstey_NWPath(a.at("report"));
  std::set<std::wstring> keys;
  auto validate = [&](const std::filesystem::path &p) {
    Monstey_Require(keys.insert(Monstey_NWKey(p)).second,
                    "Navigation/owner/report outputs must be distinct");
    Monstey_RequireOfflineOutput(p, protectedPaths);
    Monstey_Require(!std::filesystem::exists(Monstey_NWLong(p)),
                    "Navigation output already exists");
  };
  validate(navOutput);
  if (ownerOutput)
    validate(*ownerOutput);
  if (reportOutput)
    validate(*reportOutput);
  auto result = apply
                    ? Monstey_NavigationApply(source, owner, a.at("spec"))
                    : Monstey_NavigationBuild(
                          source, a.at("spec"), owner,
                          ci ? std::span<const std::uint8_t>(sources[*ci].bytes)
                             : std::span<const std::uint8_t>());
  auto &report = result.report;
  report["source"] = Monstey_Utf8(sources[si].path.wstring());
  report["output"] = Monstey_Utf8(navOutput.wstring());
  report["output_sha256"] = Monstey_HashSha256(result.navigation);
  report["bytes"] = result.navigation.size();
  report["game_written"] = false;
  report["source_unchanged"] = true;
  report["same_stem_resource_binding_required"] = apply;
  J provenance = J::array();
  for (const auto &s : sources)
    provenance.as_array().push_back(
        J::object({{"path", Monstey_Utf8(s.path.wstring())},
                   {"sha256", s.sha},
                   {"bytes", s.bytes.size()}}));
  report["inputs"] = provenance;
  outputs.emplace_back(navOutput, std::move(result.navigation));
  if (ownerOutput) {
    report["owner_output"] = Monstey_Utf8(ownerOutput->wstring());
    report["owner_output_sha256"] = Monstey_HashSha256(result.owner);
    outputs.emplace_back(*ownerOutput, std::move(result.owner));
  }
  if (reportOutput) {
    report["report"] = Monstey_Utf8(reportOutput->wstring());
    const auto text = report.dump(2) + "\n";
    outputs.emplace_back(*reportOutput, B(text.begin(), text.end()));
  }
  std::vector<std::pair<std::filesystem::path, std::string>> created;
  try {
    Monstey_NWCheckSources(sources);
    for (const auto &[path, bytes] : outputs) {
      Monstey_WriteNewFile(path, bytes);
      const auto hash = Monstey_HashSha256(bytes);
      created.emplace_back(path, hash);
      Monstey_Require(Monstey_HashSha256(path) == hash,
                      "Navigation output readback SHA mismatch");
    }
    Monstey_NWCheckSources(sources);
  } catch (...) {
    for (auto it = created.rbegin(); it != created.rend(); ++it) {
      try {
        if (Monstey_HashSha256(it->first) == it->second) {
          std::error_code ec;
          std::filesystem::remove(Monstey_NWLong(it->first), ec);
        }
      } catch (const std::exception &) {
      }
    }
    throw;
  }
  return report;
}
} // namespace Monstey
