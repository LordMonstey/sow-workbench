#pragma once
#include "Monstey_Archive.hpp"
#include <memory>
#include <vector>
namespace Monstey
{
// Physical line numbers from x64/default.archcfg are native priorities: later wins.
Monstey_Json Monstey_ArchiveMounts(const std::filesystem::path &game_root);
// options: include_embedded(bool), oodle_dll(path|null), archive_filter(string).
// JSONL retains every variant. Embedded winner scope is its native EMBB parent.
Monstey_Json Monstey_ArchiveCatalog(const std::filesystem::path &game_root, const std::filesystem::path &output,
                                    const Monstey_Json &options = Monstey_Json::object());
// query/path/bundle/kind, winning_only, offset, limit, expected_sha256, verify_mounts.
Monstey_Json Monstey_ReadArchiveCatalog(const std::filesystem::path &catalog,
                                        const Monstey_Json &options = Monstey_Json::object());
// Empty bundle means parent is a loose EMBB; otherwise parent is an LTAR archive.
// Only the selected child is streamed to a new offline file.
Monstey_Json Monstey_ExtractEmbeddedResource(const std::filesystem::path &parent, std::string_view bundle,
                                             std::string_view resource, const std::filesystem::path &output,
                                             const std::filesystem::path &oodle_dll = {});
// Inspect and hash native EMBB children in bounded decoded ranges; no parent copy.
Monstey_Json Monstey_InspectEmbeddedResource(const std::filesystem::path &parent,
    std::string_view bundle={},const std::filesystem::path &oodle_dll={});
// Read-only prefix/range inspection. Requests are bounded to 1 MiB.
Monstey_Json Monstey_ReadEmbeddedResourceRange(const std::filesystem::path &parent, std::string_view bundle,
                                               std::string_view resource, std::uint64_t offset, std::uint64_t count,
                                               const std::filesystem::path &oodle_dll = {});
class Monstey_Settings
{
  public:
    explicit Monstey_Settings(const std::filesystem::path &path = {},
                              std::vector<std::filesystem::path> protected_sources = {});
    ~Monstey_Settings();
    Monstey_Settings(Monstey_Settings &&) noexcept;
    Monstey_Settings &operator=(Monstey_Settings &&) noexcept;
    Monstey_Settings(const Monstey_Settings &) = delete;
    Monstey_Settings &operator=(const Monstey_Settings &) = delete;
    Monstey_Json Describe() const;
    Monstey_Json Configure(const Monstey_Json &arguments);
    void Protect(const std::filesystem::path &source);

  private:
    struct Monstey_Impl;
    std::unique_ptr<Monstey_Impl> impl_;
};
} // namespace Monstey
