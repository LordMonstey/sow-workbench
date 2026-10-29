#include "Monstey_WorldAssets.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <windows.h>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
std::string Monstey_ALower(std::string value) {
  for (auto &c : value)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + 32);
  return value;
}
std::string Monstey_AText(const std::filesystem::path &p) {
  return Monstey_Utf8(p.wstring());
}
std::filesystem::path Monstey_APath(const J &value) {
  const auto &text = value.as_string();
  Monstey_Require(!text.empty() && text.find('\0') == text.npos,
                  "Asset path must be nonempty without NUL");
  return std::filesystem::weakly_canonical(
      std::filesystem::absolute(std::filesystem::path(Monstey_Wide(text))));
}
void Monstey_AKeys(const J &value,
                   std::initializer_list<std::string_view> keys) {
  Monstey_Require(value.is_object(), "Asset arguments must be an object");
  for (const auto &[key, v] : value.as_object()) {
    (void)v;
    Monstey_Require(std::find(keys.begin(), keys.end(), key) != keys.end(),
                    "Unknown asset argument: " + key);
  }
}
bool Monstey_AWithin(const std::filesystem::path &p,
                     const std::filesystem::path &root) {
  auto a = Monstey_ALower(Monstey_AText(std::filesystem::weakly_canonical(p))),
       b = Monstey_ALower(
           Monstey_AText(std::filesystem::weakly_canonical(root)));
  std::replace(a.begin(), a.end(), '\\', '/');
  std::replace(b.begin(), b.end(), '\\', '/');
  return a == b ||
         (a.size() > b.size() && a.starts_with(b) && a[b.size()] == '/');
}
struct Monstey_AStamp {
  std::uint64_t volume{}, identity{}, bytes{}, time{};
  bool operator==(const Monstey_AStamp &) const = default;
  J Json() const { return J::array({volume, identity, bytes, time}); }
};
Monstey_AStamp Monstey_AStat(const std::filesystem::path &p) {
  const auto native = Monstey_WindowsFilePath(p);
  HANDLE h =
      CreateFileW(native.c_str(), FILE_READ_ATTRIBUTES,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  Monstey_Require(h != INVALID_HANDLE_VALUE, "Cannot fingerprint asset input");
  BY_HANDLE_FILE_INFORMATION info{};
  const auto ok = GetFileInformationByHandle(h, &info);
  CloseHandle(h);
  Monstey_Require(ok != FALSE &&
                      (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0,
                  "Asset input is not a regular file");
  return {info.dwVolumeSerialNumber,
          (std::uint64_t(info.nFileIndexHigh) << 32) | info.nFileIndexLow,
          (std::uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow,
          (std::uint64_t(info.ftLastWriteTime.dwHighDateTime) << 32) |
              info.ftLastWriteTime.dwLowDateTime};
}
J Monstey_ASearch(const J &a) {
  Monstey_AKeys(a, {"catalog", "path_contains", "suffix", "bundle_contains",
                    "offset", "limit", "manifests"});
  const auto offset = a.value("offset", 0).as_uint(),
             limit = a.value("limit", 100).as_uint();
  Monstey_Require(limit >= 1 && limit <= 500,
                  "Asset search limit must be 1..500");
  const auto query = Monstey_ALower(a.value("path_contains", "").as_string()),
             suffix = Monstey_ALower(a.value("suffix", "").as_string()),
             bundle =
                 Monstey_ALower(a.value("bundle_contains", "").as_string());
  const auto catalog =
      a.contains("catalog")
          ? Monstey_APath(a.at("catalog"))
          : std::filesystem::absolute(L"reports/sow-asset-catalog.jsonl");
  const auto manifests = a.value("manifests", J::array());
  Monstey_Require(manifests.is_array(), "Asset manifests must be an array");
  J items = J::array(), inputs = J::array();
  std::uint64_t total = 0;
  const auto visit = [&](J row, const std::filesystem::path &origin) {
    Monstey_Require(row.is_object(), "Asset catalog row must be an object");
    const auto path = Monstey_ArchiveVirtual(
        row.value("resource_path", row.value("path", "")).as_string());
    const auto parent = row.contains("bundle") && row.at("bundle").is_string()
                            ? Monstey_ALower(row.at("bundle").as_string())
                            : std::string{};
    if (path.find(query) == path.npos ||
        (!suffix.empty() && !path.ends_with(suffix)) ||
        parent.find(bundle) == parent.npos)
      return;
    if (total >= offset && items.size() < limit) {
      row["resource_path"] = path;
      row["catalog_origin"] = Monstey_AText(origin);
      items.as_array().push_back(std::move(row));
    }
    Monstey_Require(total != std::numeric_limits<std::uint64_t>::max(),
                    "Asset result count overflows");
    ++total;
  };
  const auto snapshot = [&](const std::filesystem::path &path) {
    return J::object({{"path", Monstey_AText(path)},
                      {"sha256", Monstey_HashSha256(path)},
                      {"stamp", Monstey_AStat(path).Json()}});
  };
  if (std::filesystem::is_regular_file(Monstey_WindowsFilePath(catalog))) {
    inputs.as_array().push_back(snapshot(catalog));
    std::ifstream stream(Monstey_WindowsFilePath(catalog), std::ios::binary);
    Monstey_Require(stream.good(), "Cannot open asset catalog");
    std::string line;
    bool first = true;
    for (char c{}; stream.get(c);) {
      if (c != '\n') {
        Monstey_Require(line.size() < 16ull * 1024 * 1024,
                        "Asset catalog row exceeds 16 MiB");
        line.push_back(c);
        continue;
      }
      if (first && line.starts_with("\xef\xbb\xbf"))
        line.erase(0, 3);
      first = false;
      if (line.find_first_not_of(" \t\r") != line.npos)
        visit(J::Parse(line), catalog);
      line.clear();
    }
    Monstey_Require(stream.eof(), "Asset catalog read failed");
    if (first && line.starts_with("\xef\xbb\xbf"))
      line.erase(0, 3);
    if (line.find_first_not_of(" \t\r") != line.npos)
      visit(J::Parse(line), catalog);
  } else
    Monstey_Require(
        !manifests.empty(),
        "Asset catalog is absent and no component manifest was supplied");
  for (const auto &entry : manifests.as_array()) {
    const auto path = Monstey_APath(entry);
    inputs.as_array().push_back(snapshot(path));
    const auto data = J::Parse(Monstey_ReadText(path, 64ull * 1024 * 1024));
    for (const auto &row : data.at("resources").as_array())
      visit(row, path);
  }
  for (const auto &input : inputs.as_array()) {
    const auto p = Monstey_APath(input.at("path"));
    Monstey_Require(Monstey_AStat(p).Json() == input.at("stamp") &&
                        Monstey_HashSha256(p) == input.at("sha256").as_string(),
                    "Asset catalog or manifest changed during search");
  }
  const auto next = offset + static_cast<std::uint64_t>(items.size());
  return J::object(
      {{"total", total},
       {"offset", offset},
       {"limit", limit},
       {"items", items},
       {"next_offset", next < total ? J(next) : J()},
       {"inputs", inputs},
       {"resolution", "Catalog lists variants; extraction resolves current "
                      "mount order and explicit parent"},
       {"implementation_language", "C++20"},
       {"runtime_verified", false}});
}
struct Monstey_AResource {
  std::string path, kind;
  std::filesystem::path source;
  std::shared_ptr<Monstey_Archive> archive;
  std::optional<Monstey_ArchiveEntry> entry;
  Monstey_AStamp stamp;
  std::uint64_t priority{}, size{};
  J Provenance() const {
    J p = J::object({{"kind", kind},
                     {"source", Monstey_AText(source)},
                     {"priority_line", priority},
                     {"source_stamp", stamp.Json()}});
    if (entry)
      p["archive_entry"] = J::object({{"resource_path", entry->path},
                                      {"offset", entry->offset},
                                      {"size", entry->size},
                                      {"stored_size", entry->stored_size},
                                      {"flags", entry->flags}});
    return p;
  }
};
struct Monstey_ARequest {
  std::string path, parent;
  std::filesystem::path target;
  Monstey_AResource source;
  bool embedded{};
  std::uint64_t size{};
  J probe;
};
void Monstey_ARollback(
    const std::filesystem::path &root,
    const std::vector<std::filesystem::path> &planned,
    const std::vector<std::pair<std::filesystem::path, std::string>>
        &published) noexcept {
  for (auto it = published.rbegin(); it != published.rend(); ++it)
    try {
      if (Monstey_HashSha256(it->first) == it->second) {
        std::error_code error;
        std::filesystem::remove(Monstey_WindowsFilePath(it->first), error);
      }
    } catch (...) {
    }
  try {
    std::set<std::filesystem::path> directories;
    for (const auto &path : planned) {
      auto parent = path.parent_path();
      while (!parent.empty() && Monstey_AWithin(parent, root)) {
        directories.insert(parent);
        if (parent == root)
          break;
        parent = parent.parent_path();
      }
    }
    std::vector<std::filesystem::path> ordered(directories.begin(),
                                               directories.end());
    std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
      return a.wstring().size() > b.wstring().size();
    });
    for (const auto &path : ordered)
      try {
        std::error_code error;
        std::filesystem::remove(Monstey_WindowsFilePath(path), error);
      } catch (...) {
      }
  } catch (...) {
  }
}
J Monstey_AExtract(const J &a) {
  Monstey_AKeys(
      a, {"requests", "output", "max_total_bytes", "game_root", "oodle_dll"});
  const auto &requested = a.at("requests");
  Monstey_Require(requested.is_array() && !requested.empty(),
                  "Select explicit asset requests");
  const auto budget = a.value("max_total_bytes", 268435456).as_uint();
  Monstey_Require(budget > 0, "Extraction budget must be positive");
  std::filesystem::path game;
  if (a.contains("game_root"))
    game = Monstey_APath(a.at("game_root"));
  else {
    std::wstring value(32768, L'\0');
    const auto n = GetEnvironmentVariableW(L"MONSTEY_SOW_GAME", value.data(),
                                           static_cast<DWORD>(value.size()));
    Monstey_Require(
        n > 0 && n < value.size(),
        "Configure the game folder or supply game_root for extraction");
    value.resize(n);
    game = Monstey_APath(Monstey_AText(value));
  }
  const auto output = Monstey_APath(a.at("output"));
  Monstey_RequireOfflineOutput(output);
  Monstey_Require(!Monstey_AWithin(output, game),
                  "Extract assets outside the game directory");
  Monstey_Require(!std::filesystem::exists(Monstey_WindowsFilePath(output)),
                  "Extraction requires a new output directory");
  const auto dll =
      a.contains("oodle_dll")
          ? Monstey_APath(a.at("oodle_dll"))
          : (std::filesystem::is_regular_file(game / L"x64/oo2core_5_win64.dll")
                 ? game / L"x64/oo2core_5_win64.dll"
                 : std::filesystem::path{});
  std::vector<Monstey_ARequest> requests;
  std::set<std::string> wanted, names;
  for (const auto &row : requested.as_array()) {
    Monstey_AKeys(row, {"resource_path", "bundle"});
    auto path = Monstey_ArchiveVirtual(row.at("resource_path").as_string());
    (void)Monstey_PathHash(path);
    Monstey_Require(names.insert(path).second,
                    "Duplicate asset resource selection");
    const auto parentText = row.value("bundle", "").as_string();
    const auto parent =
        parentText.empty() ? std::string{} : Monstey_ArchiveVirtual(parentText);
    Monstey_Require(parent.empty() || parent.ends_with(".embb"),
                    "Explicit asset parent must be an EMBB resource");
    wanted.insert(path);
    if (!parent.empty())
      wanted.insert(parent);
    requests.push_back({path, parent, {}, {}, false, 0, {}});
  }
  const auto mounts = Monstey_ArchiveMounts(game);
  const auto config = Monstey_APath(mounts.at("config"));
  const auto configHash = mounts.at("config_sha256").as_string();
  std::map<std::string, Monstey_AResource> effective;
  std::vector<std::shared_ptr<Monstey_Archive>> archives;
  for (const auto &mount : mounts.at("mounts").as_array()) {
    const auto source = Monstey_APath(mount.at("source"));
    const auto priority = mount.at("priority").as_uint();
    if (mount.at("kind").as_string() == "archive") {
      const auto stamp = Monstey_AStat(source);
      auto archive = std::make_shared<Monstey_Archive>(source);
      Monstey_Require(Monstey_AStat(source) == stamp,
                      "Archive identity changed during index reading");
      archives.push_back(archive);
      for (const auto &path : wanted)
        if (const auto *entry = archive->Find(path)) {
          auto old = effective.find(path);
          if (old == effective.end() || old->second.priority < priority)
            effective[path] = {path,     "ordered_direct_resource",
                               source,   archive,
                               *entry,   stamp,
                               priority, entry->size};
        }
      archive->CheckUnchanged();
    } else {
      for (const auto &path : wanted) {
        const auto file = source / std::filesystem::path(Monstey_Wide(path));
        if (!std::filesystem::is_regular_file(Monstey_WindowsFilePath(file)))
          continue;
        Monstey_Require(Monstey_AWithin(file, source),
                        "Loose asset source escaped its mount");
        auto old = effective.find(path);
        if (old == effective.end() || old->second.priority < priority) {
          const auto stamp = Monstey_AStat(file);
          effective[path] = {path,         "ordered_loose_resource",
                             file,         {},
                             std::nullopt, stamp,
                             priority,     stamp.bytes};
        }
      }
    }
  }
  std::uint64_t total = 0;
  for (auto &request : requests) {
    auto found = effective.find(request.path);
    if (found != effective.end()) {
      request.source = found->second;
      request.size = request.source.size;
    } else {
      Monstey_Require(
          !request.parent.empty(),
          "Asset has no direct resource and no explicit EMBB parent: " +
              request.path);
      found = effective.find(request.parent);
      Monstey_Require(found != effective.end(),
                      "Explicit native EMBB parent is unavailable");
      request.source = found->second;
      request.embedded = true;
      request.probe = Monstey_ReadEmbeddedResourceRange(
          request.source.source,
          request.source.archive ? request.parent : std::string{}, request.path,
          0, 0, dll);
      request.size = request.probe.at("resource_size").as_uint();
    }
    Monstey_Require(request.size <= budget - total,
                    "Selected asset extraction exceeds byte budget");
    total += request.size;
    request.target =
        output / L"assets" / std::filesystem::path(Monstey_Wide(request.path));
    Monstey_Require(Monstey_AWithin(request.target, output),
                    "Asset output escaped extraction directory");
  }
  const auto unchanged = [&] {
    Monstey_Require(Monstey_HashSha256(config) == configHash,
                    "Archive configuration changed during extraction");
    for (const auto &archive : archives)
      archive->CheckUnchanged();
    for (const auto &request : requests)
      Monstey_Require(Monstey_AStat(request.source.source) ==
                          request.source.stamp,
                      "Asset source changed during extraction");
  };
  unchanged();
  const auto manifest = output / L"resource-manifest.json";
  std::vector<std::filesystem::path> planned{manifest};
  for (const auto &request : requests)
    planned.push_back(request.target);
  std::filesystem::create_directories(
      Monstey_WindowsFilePath(output.parent_path()));
  Monstey_Require(CreateDirectoryW(Monstey_WindowsFilePath(output).c_str(),
                                   nullptr) != FALSE,
                  "Cannot claim new extraction directory");
  std::vector<std::pair<std::filesystem::path, std::string>> published;
  published.reserve(requests.size() + 1);
  auto resources = J::array();
  try {
    for (const auto &request : requests) {
      unchanged();
      J provenance = request.source.Provenance();
      if (request.embedded) {
        auto result = Monstey_ExtractEmbeddedResource(
            request.source.source,
            request.source.archive ? request.parent : std::string{},
            request.path, request.target, dll);
        published.emplace_back(request.target, result.at("sha256").as_string());
        provenance["kind"] = "explicit_embedded_child";
        provenance["parent"] = request.parent;
        provenance["extraction"] = result;
        Monstey_Require(result.at("bytes").as_uint() == request.size &&
                            result.at("embedded_metadata_sha256") ==
                                request.probe.at("embedded_metadata_sha256"),
                        "Native EMBB metadata changed after selection");
      } else if (request.source.archive)
        request.source.archive->Extract(request.path, request.target, dll);
      else {
        Monstey_ArchiveProject project;
        project.AddFile(request.path, request.source.source);
        project.Extract(request.path, request.target);
      }
      const auto hash = Monstey_HashSha256(request.target);
      if (!request.embedded)
        published.emplace_back(request.target, hash);
      Monstey_Require(std::filesystem::file_size(Monstey_WindowsFilePath(
                          request.target)) == request.size,
                      "Extracted asset size differs");
      resources.as_array().push_back(
          J::object({{"resource_path", request.path},
                     {"path", Monstey_AText(request.target)},
                     {"bytes", request.size},
                     {"sha256", hash},
                     {"provenance", provenance}}));
      unchanged();
    }
    auto report = J::object(
        {{"status", "TARGETED_WORLD_ASSETS_EXTRACTED"},
         {"resources", resources},
         {"total_bytes", total},
         {"archive_configuration",
          J::object({{"path", Monstey_AText(config)}, {"sha256", configHash}})},
         {"archive_order_resolved", true},
         {"unrequested_parent_copied", false},
         {"source_unchanged", true},
         {"implementation_language", "C++20"},
         {"game_written", false},
         {"runtime_verified", false}});
    const auto text = report.dump(2) + "\n";
    Monstey_WriteNewText(manifest, text);
    published.emplace_back(manifest, Monstey_HashSha256(manifest));
    unchanged();
    return report;
  } catch (...) {
    Monstey_ARollback(output, planned, published);
    throw;
  }
}
J Monstey_ASchema(J::Object properties,
                  std::initializer_list<const char *> required = {}) {
  J r = J::array();
  for (const auto *key : required)
    r.as_array().push_back(key);
  return J::object({{"type", "object"},
                    {"properties", std::move(properties)},
                    {"required", r},
                    {"additionalProperties", false}});
}
} // namespace
J Monstey_WorldAssetsCatalog() {
  const auto text = J::object({{"type", "string"}}),
             path = J::object(
                 {{"type", "string"}, {"minLength", 1}, {"format", "path"}});
  const auto integer = J::object({{"type", "integer"}, {"minimum", 0}});
  const auto manifests = J::object({{"type", "array"}, {"items", path}});
  const auto request = Monstey_ASchema(
      {{"resource_path", text}, {"bundle", text}}, {"resource_path"});
  const auto requests =
      J::object({{"type", "array"}, {"minItems", 1}, {"items", request}});
  auto search = Monstey_ASchema(
      {{"catalog", path},
       {"path_contains", text},
       {"suffix", text},
       {"bundle_contains", text},
       {"offset", integer},
       {"limit",
        J::object({{"type", "integer"}, {"minimum", 1}, {"maximum", 500}})},
       {"manifests", manifests}});
  auto extract = Monstey_ASchema(
      {{"requests", requests},
       {"output", path},
       {"game_root", path},
       {"oodle_dll", path},
       {"max_total_bytes", J::object({{"type", "integer"}, {"minimum", 1}})}},
      {"requests", "output"});
  return J::array(
      {J::object({{"name", "sow_world_asset_search"}, {"inputSchema", search}}),
       J::object(
           {{"name", "sow_world_asset_extract"}, {"inputSchema", extract}})});
}
J Monstey_WorldAssetsCall(std::string_view operation, const J &arguments) {
  if (operation == "sow_world_asset_search")
    return Monstey_ASearch(arguments);
  if (operation == "sow_world_asset_extract")
    return Monstey_AExtract(arguments);
  throw Monstey_Error("unknown_asset_operation",
                      "Unknown world asset operation");
}
} // namespace Monstey