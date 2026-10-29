#pragma once
#include "Monstey_Core.hpp"
#include <memory>

namespace Monstey {
// Native SKDB9 stores uint32 hashes, UTF16 code-unit offsets and text, not key names.
class Monstey_StringsProject {
  public:
    explicit Monstey_StringsProject(const std::filesystem::path &source = {});
    ~Monstey_StringsProject();
    Monstey_StringsProject(Monstey_StringsProject &&) noexcept;
    Monstey_StringsProject &operator=(Monstey_StringsProject &&) noexcept;
    Monstey_StringsProject(const Monstey_StringsProject &) = delete;
    Monstey_StringsProject &operator=(const Monstey_StringsProject &) = delete;
    Monstey_Json Describe() const;
    Monstey_Json Entries(std::string_view query = {}, std::size_t offset = 0, std::size_t limit = 250) const;
    Monstey_Json Set(const Monstey_Json &key, std::string_view text);
    Monstey_Json Remove(const Monstey_Json &key);
    Monstey_Json Rename(const Monstey_Json &before, const Monstey_Json &after);
    Monstey_Json Undo();
    Monstey_Json Redo();
    Monstey_Json Revert();
    Monstey_Json Changes() const;
    Monstey_Json SaveProject(const std::filesystem::path &output) const;
    static std::unique_ptr<Monstey_StringsProject> LoadProject(const std::filesystem::path &source);
    Monstey_Json Build(const std::filesystem::path &output) const;

  private:
    struct Monstey_Impl;
    std::unique_ptr<Monstey_Impl> impl_;
};
} // namespace Monstey
