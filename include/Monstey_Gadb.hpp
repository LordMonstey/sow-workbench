#pragma once
#include "Monstey_Core.hpp"
#include <functional>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
namespace Monstey
{
class Monstey_GadbClassCache;
class Monstey_Gadb
{
  public:
    explicit Monstey_Gadb(const std::filesystem::path &source, const std::filesystem::path &schema = {});
    // A logical empty GADB17 source; no temporary file or template record is used.
    static std::unique_ptr<Monstey_Gadb> New(const std::filesystem::path &schema = {});
    // A new empty FX database uses an envelope qualified from a native LTFX8 source.
    static std::unique_ptr<Monstey_Gadb> NewFx(std::span<const std::uint8_t> qualifiedPrefix,
                                               const std::filesystem::path &schema = {});
    static std::unique_ptr<Monstey_Gadb> NewFx(std::span<const std::uint8_t> qualifiedPrefix,
                                               const Monstey_Json &qualifiedSchema);
    static std::unique_ptr<Monstey_Gadb> WithSchema(const std::filesystem::path &source,
                                                    const Monstey_Json &qualifiedSchema);
    ~Monstey_Gadb();
    Monstey_Gadb(Monstey_Gadb &&) noexcept;
    Monstey_Gadb &operator=(Monstey_Gadb &&) noexcept;
    Monstey_Gadb(const Monstey_Gadb &) = delete;
    Monstey_Gadb &operator=(const Monstey_Gadb &) = delete;
    Monstey_Json Describe() const;
    Monstey_Json Schema() const;
    Monstey_Json ProjectMetadata() const;
    Monstey_Json SetProjectMetadata(const Monstey_Json &metadata);
    // Bounded UI pages: {rows:[...],total,offset,limit}.
    Monstey_Json Categories(std::string_view query = {}) const;
    Monstey_Json Records(std::uint32_t category, std::string_view query = {}, std::size_t offset = 0,
                         std::size_t limit = 250) const;
    // Raw descriptor snapshots only: no value/atom decode or final writer planning.
    // Clean file-backed rows retain native source order; pending edits remain project snapshots.
    Monstey_Json DescriptorLayouts(std::uint32_t category, std::size_t offset = 0,
                                   std::size_t limit = 250) const;
    Monstey_Json FxRecords(std::string_view query = {}, std::size_t offset = 0, std::size_t limit = 250) const;
    Monstey_Json Dependencies() const;
    // Effective typed type10/type11 owners, including pending edits; deleted owners are excluded.
    Monstey_Json IncomingReferences(std::string_view selector, std::size_t offset = 0,
                                     std::size_t limit = 250) const;
    // Read-only bounded source atom uses; pending typed values have no serialized address.
    Monstey_Json AtomUses(std::string_view selector, std::size_t offset = 0,
                          std::size_t limit = 250) const;
    Monstey_Json Record(std::string_view selector) const;
    Monstey_Json Metadata(std::string_view selector) const;
    Monstey_Json SetField(std::string_view selector, std::string_view field, const Monstey_Json &value);
    // Category indices/references stay stable inside the project. Build publishes their remap.
    Monstey_Json CreateCategory(std::string_view schemaName, std::string_view name = {}, std::string_view path = {});
    Monstey_Json CreateRecord(std::uint32_t category, std::string_view name = {}, bool anonymous = false);
    // metadata uses native schema keys (array=-1 is variable). native_ordinal qualifies the
    // immutable UInt8 descriptor+8 identity; otherwise a free stable editor identity is assigned.
    Monstey_Json AddField(std::string_view record, std::string_view name, std::uint8_t nativeType,
                          const Monstey_Json &value, const Monstey_Json &metadata = Monstey_Json::object());
    Monstey_Json DeleteField(std::string_view record, std::string_view field);
    Monstey_Json MoveField(std::string_view record, std::string_view field, std::size_t newOrdinal);
    Monstey_Json CloneRecord(std::string_view source, std::string_view newName);
    Monstey_Json RenameRecord(std::string_view selector, std::string_view newName);
    // Null replacement rejects incoming references; "0xffffffff" nullifies them.
    Monstey_Json DeleteRecord(std::string_view selector, const Monstey_Json &replacement = nullptr);
    // Groups in-memory mutations into one undo step; exceptions restore state and history.
    Monstey_Json Transaction(const std::function<void(Monstey_Gadb &)> &action);
    Monstey_Json Undo();
    Monstey_Json Redo();
    Monstey_Json Revert();
    Monstey_Json Changes() const;
    Monstey_Json SaveProject(const std::filesystem::path &output) const;
    static std::unique_ptr<Monstey_Gadb> LoadProject(const std::filesystem::path &path);
    Monstey_Json Build(const std::filesystem::path &output) const;
    // Opt-in final writer-plan qualification. Proved unsafe slots reject before publication;
    // UNKNOWN remains explicit in the report and is not an engine-compatibility claim.
    Monstey_Json Build(const std::filesystem::path &output,
                       const Monstey_GadbClassCache &declarations) const;
    std::uint32_t Resolve(std::string_view selector) const;

  private:
    Monstey_Json BuildImpl(const std::filesystem::path &output,
                           const Monstey_GadbClassCache *declarations) const;
    struct Monstey_Impl;
    explicit Monstey_Gadb(std::unique_ptr<Monstey_Impl> impl);
    std::unique_ptr<Monstey_Impl> impl_;
};
int Monstey_RunGadbTests(const std::filesystem::path &directory, const std::filesystem::path &nativeDatabase = {},
                         const std::filesystem::path &schema = {}, const std::filesystem::path &nativeFx = {});
} // namespace Monstey
