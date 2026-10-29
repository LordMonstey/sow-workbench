#pragma once
#include "Monstey_Anix.hpp"
namespace Monstey {
// Source-qualified native animation editor; all output files are new offline
// candidates.
class Monstey_AnimationProject {
public:
  explicit Monstey_AnimationProject(const std::filesystem::path &source,
                                    std::string_view expected_sha256 = {},
                                    bool semantic_source = false);
  ~Monstey_AnimationProject();
  Monstey_AnimationProject(const Monstey_AnimationProject &) = delete;
  Monstey_AnimationProject &
  operator=(const Monstey_AnimationProject &) = delete;
  Monstey_Json Describe() const;
  Monstey_Json Clips(std::string_view query = {}, std::size_t offset = 0,
                     std::size_t limit = 250) const;
  Monstey_Json Clip(std::string_view name) const;
  Monstey_Json Sample(std::string_view name, const Monstey_Json &ticks,
                      bool bind_fallback = true) const;
  Monstey_Json Edit(const Monstey_Json &operations);
  Monstey_Json ImportClip(const std::filesystem::path &semantic_scene,
                          std::string_view clip, std::string_view name = {},
                          std::string_view expected_sha256 = {});
  Monstey_Json Undo();
  Monstey_Json Redo();
  Monstey_Json Revert();
  Monstey_Json Changes() const;
  // Optional compact/paged view; the no-argument journal contract is unchanged.
  Monstey_Json Changes(const Monstey_Json &options) const;
  Monstey_Json Export(const std::filesystem::path &output,
                      std::string_view clip = {}) const;
  Monstey_Json SaveProject(const std::filesystem::path &output) const;
  static std::unique_ptr<Monstey_AnimationProject>
  LoadProject(const std::filesystem::path &path);
  Monstey_Json Build(const std::filesystem::path &output) const;

private:
  struct Monstey_Impl;
  std::unique_ptr<Monstey_Impl> impl_;
};
// Shared native file publisher; pre/post source checks and hash-owned rollback.
void Monstey_AnimationPublishOffline(
    const std::filesystem::path &output, const Monstey_Bytes &bytes,
    std::span<const std::filesystem::path> sources,
    const std::function<void()> &check);
Monstey_Json Monstey_AnimationCatalog();
Monstey_Json Monstey_AnimationSkeletonCall(std::string_view operation,
                                           const Monstey_Json &arguments);
} // namespace Monstey
