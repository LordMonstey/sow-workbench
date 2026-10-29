#pragma once
#include "Monstey_Core.hpp"
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>

namespace Monstey {
struct Monstey_ArchiveEntry {
    std::string path;
    std::uint64_t offset{}, stored_size{}, size{};
    std::uint32_t flags{};
};
struct Monstey_ArchiveBuildReport {
    std::filesystem::path output;
    std::uint64_t bytes{};
    std::size_t files{}, folders{};
    std::string sha256;
    bool readback_verified{};
};
// LTAR v3/v4 index with lazy bounded chunk decoding, including RATL big endian.
class Monstey_Archive {
  public:
    using Sink = std::function<void(std::span<const std::uint8_t>)>;
    explicit Monstey_Archive(const std::filesystem::path &path);
    const std::filesystem::path &Path() const noexcept { return path_; }
    unsigned Version() const noexcept { return version_; }
    std::uint64_t Length() const noexcept { return length_; }
    std::uint64_t IndexLength() const noexcept { return index_length_; }
    bool BigEndian() const noexcept { return big_endian_; }
    const std::vector<Monstey_ArchiveEntry> &Entries() const noexcept { return entries_; }
    const std::vector<std::string> &Folders() const noexcept { return folders_; }
    const Monstey_ArchiveEntry *Find(const std::string &virtual_path) const;
    void Stream(const Monstey_ArchiveEntry &entry, const Sink &sink,
                const std::filesystem::path &oodle_dll = {}) const;
    Monstey_Bytes Read(const std::string &virtual_path, const std::filesystem::path &oodle_dll = {},
                       std::uint64_t limit = 256ull * 1024 * 1024) const;
    void Extract(const std::string &virtual_path, const std::filesystem::path &output,
                 const std::filesystem::path &oodle_dll = {}) const;
    void CheckUnchanged() const;

  private:
    std::filesystem::path path_;
    std::filesystem::file_time_type time_;
    std::uint64_t length_{}, index_length_{};
    unsigned version_{};
    bool big_endian_{};
    std::vector<Monstey_ArchiveEntry> entries_;
    std::vector<std::string> folders_;
    std::map<std::string, std::size_t> by_path_;
};
struct Monstey_ArchiveRangeStats {
    std::uint64_t chunks{}, decoded_chunk_loads{}, cache_hits{}, cached_chunks{};
    std::uint64_t metadata_window_reads{}, metadata_bytes_read{}, metadata_window_peak{};
};
// Validated chunk metadata plus a four-chunk LRU, not a materialized parent asset.
class Monstey_ArchiveRange {
  public:
    Monstey_ArchiveRange(std::shared_ptr<const Monstey_Archive> archive, const Monstey_ArchiveEntry &entry,
                         const std::filesystem::path &oodle_dll = {});
    ~Monstey_ArchiveRange();
    Monstey_ArchiveRange(Monstey_ArchiveRange &&) noexcept;
    Monstey_ArchiveRange &operator=(Monstey_ArchiveRange &&) noexcept;
    Monstey_ArchiveRange(const Monstey_ArchiveRange &) = delete;
    Monstey_ArchiveRange &operator=(const Monstey_ArchiveRange &) = delete;
    std::uint64_t Size() const noexcept;
    Monstey_Bytes Read(std::uint64_t start, std::uint64_t count);
    Monstey_ArchiveRangeStats Stats() const;

  private:
    struct Monstey_Impl;
    std::unique_ptr<Monstey_Impl> impl_;
};
// Original payloads stay lazy; undo/redo snapshots contain metadata only.
class Monstey_ArchiveProject {
  public:
    Monstey_ArchiveProject();
    explicit Monstey_ArchiveProject(const std::filesystem::path &archive);
    std::vector<Monstey_ArchiveEntry> Entries() const;
    std::vector<std::string> Folders() const;
    std::optional<std::filesystem::path> Source() const;
    bool Dirty() const noexcept;
    bool CanUndo() const noexcept;
    bool CanRedo() const noexcept;
    void AddFile(const std::string &virtual_path, const std::filesystem::path &source);
    void AddDirectory(const std::string &virtual_directory, const std::filesystem::path &source);
    void Replace(const std::string &virtual_path, const std::filesystem::path &source);
    void Remove(const std::string &virtual_path);
    void Rename(const std::string &virtual_path, const std::string &destination);
    void CreateFolder(const std::string &virtual_path);
    void Extract(const std::string &virtual_path, const std::filesystem::path &output,
                 const std::filesystem::path &oodle_dll = {}) const;
    bool Undo();
    bool Redo();
    void Revert();
    void Save(const std::filesystem::path &output) const;
    static Monstey_ArchiveProject Load(const std::filesystem::path &source);
    Monstey_ArchiveBuildReport Build(const std::filesystem::path &output, bool include_patchlist = true,
                                     const std::filesystem::path &oodle_dll = {}) const;

  private:
    struct Item {
        std::string original;
        std::filesystem::path local;
        std::uint64_t size{};
        std::filesystem::file_time_type time{};
        std::string sha256;
        bool operator==(const Item &) const = default;
    };
    struct State {
        std::map<std::string, Item> files;
        std::set<std::string> folders{""};
        bool operator==(const State &) const = default;
    };
    std::shared_ptr<Monstey_Archive> original_;
    State initial_;
    std::vector<State> history_;
    std::size_t cursor_{};
    const State &Current() const;
    void Commit(State state);
    static Item Local(const std::filesystem::path &source);
    static void Parents(State &state, const std::string &path);
};
std::uint32_t Monstey_Crc32(std::span<const std::uint8_t> input);
// Strict single-member gzip, including FHCRC, CRC32 and ISIZE verification.
Monstey_Bytes Monstey_InflateGzip(std::span<const std::uint8_t> input,
                                  std::size_t maximum = 512ull * 1024 * 1024);
// Bounded RFC1951 decoder, also used by native ZIP/NPZ geometry interchange.
Monstey_Bytes Monstey_InflateRaw(std::span<const std::uint8_t> input, std::size_t decoded_size);
std::string Monstey_ArchiveVirtual(const std::string &path, bool allow_empty = false);
Monstey_Bytes Monstey_BuildArchivePatchList(const std::vector<std::string> &paths);
std::vector<std::string> Monstey_ReadArchivePatchList(std::span<const std::uint8_t> bytes);
} // namespace Monstey
