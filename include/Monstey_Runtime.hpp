#pragma once
#include "Monstey_AnimationProject.hpp"
#include "Monstey_Archive.hpp"
#include "Monstey_Catalog.hpp"
#include "Monstey_Gadb.hpp"
#include "Monstey_MaterialProject.hpp"
#include "Monstey_Strings.hpp"
#include <mutex>
#include <optional>

namespace Monstey {
// One serialized native editor session shared by the GUI, CLI and transports.
class Monstey_Runtime {
public:
  explicit Monstey_Runtime(std::filesystem::path schema = {});
  Monstey_Runtime(std::filesystem::path schema, std::filesystem::path settings);
  Monstey_Json Call(const Monstey_Json &request);
  Monstey_Json Reply(std::string_view request);
  static Monstey_Json Catalog();

private:
  std::mutex mutex_;
  std::filesystem::path schema_;
  std::unique_ptr<Monstey_Settings> settings_;
  std::unique_ptr<Monstey_Gadb> database_, fx_;
  std::unique_ptr<Monstey_ArchiveProject> archive_;
  std::unique_ptr<Monstey_StringsProject> strings_;
  std::unique_ptr<Monstey_AnimationProject> animation_;
  std::unique_ptr<Monstey_MaterialProject> material_;
  std::uint64_t generation_{}, revision_{};
  struct Monstey_Arguments;
  std::optional<Monstey_Json> Monstey_TryDatabaseTrace(std::string_view op,
                                                       const Monstey_Json &a);
  std::optional<Monstey_Json> Monstey_TryTextures(std::string_view op,
                                                  const Monstey_Json &a);
  std::optional<Monstey_Json> Monstey_TryMaterial(std::string_view op,
                                                  const Monstey_Json &a);
  std::optional<Monstey_Json> Monstey_TryAnimation(std::string_view op,
                                                   const Monstey_Json &a);
  std::optional<Monstey_Json> Monstey_TryModelViews(std::string_view op,
                                                    const Monstey_Json &a);
  std::optional<Monstey_Json> Monstey_TryEffectTools(std::string_view op,
                                                     const Monstey_Json &a);
  std::optional<Monstey_Json> Monstey_TrySettings(std::string_view op,
                                                  const Monstey_Json &a);
  std::optional<Monstey_Json> Monstey_TryArchiveCatalog(std::string_view op,
                                                        const Monstey_Json &a);
  std::optional<Monstey_Json> Monstey_TryModels(std::string_view op,
                                                const Monstey_Json &a);
  std::optional<Monstey_Json> Monstey_TryWorld(std::string_view op,
                                               const Monstey_Json &a);
  std::optional<Monstey_Json> Monstey_TrySession(std::string_view op);
  std::optional<Monstey_Json> Monstey_TryDatabase(std::string_view op,
                                                  const Monstey_Json &a,
                                                  const Monstey_Arguments &arguments);
  std::optional<Monstey_Json>
  Monstey_TryArchiveProject(std::string_view op, const Monstey_Json &a,
                            const Monstey_Arguments &arguments);
  std::optional<Monstey_Json> Monstey_TryStrings(std::string_view op,
                                                 const Monstey_Json &a,
                                                 const Monstey_Arguments &arguments);
  std::optional<Monstey_Json> Monstey_TryNativeDatabase(std::string_view op,
                                                        const Monstey_Json &a);
  std::optional<Monstey_Json> Monstey_TryEffects(std::string_view op,
                                                 const Monstey_Json &a,
                                                 const Monstey_Arguments &arguments);
  Monstey_Json Execute(std::string_view operation, const Monstey_Json &arguments);
  Monstey_Json WorldCall(std::string_view name, const Monstey_Json &arguments);
  Monstey_Json Describe() const;
  Monstey_Json ArchiveDescribe() const;
  Monstey_Gadb &Database();
  Monstey_Gadb &Effects();
  Monstey_ArchiveProject &Archive();
  Monstey_StringsProject &Strings();
  Monstey_AnimationProject &Animation();
  Monstey_MaterialProject &Material();
};
} // namespace Monstey
