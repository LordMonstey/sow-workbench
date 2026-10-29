#pragma once
#include "Monstey_LtmiMaterial.hpp"
#include <memory>
namespace Monstey {
// Shared strict authoring grammar used by sessions, CLI and the native GUI.
void Monstey_LtmiApplyOperations(Monstey_LtmiMaterial &material,
                                 const Monstey_Json &operations);
Monstey_Json Monstey_LtmiOperationSchema();
class Monstey_MaterialProject {
public:
  explicit Monstey_MaterialProject(const std::filesystem::path &source,
                                   std::string_view expected_sha256 = {});
  ~Monstey_MaterialProject();
  Monstey_MaterialProject(const Monstey_MaterialProject &) = delete;
  Monstey_MaterialProject &operator=(const Monstey_MaterialProject &) = delete;
  Monstey_Json Describe() const;
  Monstey_Json Edit(const Monstey_Json &operations);
  Monstey_Json EditFile(const std::filesystem::path &path,
                        std::string_view expected_sha256 = {});
  Monstey_Json Undo();
  Monstey_Json Redo();
  Monstey_Json Revert();
  Monstey_Json
  Changes(const Monstey_Json &options = Monstey_Json::object()) const;
  Monstey_Json SaveProject(const std::filesystem::path &output) const;
  static std::unique_ptr<Monstey_MaterialProject>
  LoadProject(const std::filesystem::path &path,
              std::string_view expected_sha256 = {});
  Monstey_Json Build(const std::filesystem::path &output) const;

private:
  struct Monstey_Impl;
  std::unique_ptr<Monstey_Impl> impl_;
};
Monstey_Json Monstey_MaterialCatalog();
} // namespace Monstey