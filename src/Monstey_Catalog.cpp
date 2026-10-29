#include <windows.h>

#include "Monstey_Catalog.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace Monstey
{
namespace
{
using J = Monstey_Json;
using B = Monstey_Bytes;
std::string Monstey_CLower(std::string text)
{
    for (auto &c : text)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + 32);
    return text;
}
std::filesystem::path Monstey_CPath(const std::filesystem::path &input)
{
    Monstey_Require(!input.empty() && input.wstring().find(L'\0') == std::wstring::npos,
                    "Path must be nonempty without NUL");
    return std::filesystem::weakly_canonical(std::filesystem::absolute(input));
}
std::string Monstey_CText(const std::filesystem::path &p)
{
    return Monstey_Utf8(p.wstring());
}
std::wstring Monstey_CKey(const std::filesystem::path &p)
{
    auto text = Monstey_CPath(p).wstring();
    for (auto &c : text)
        c = static_cast<wchar_t>(std::towlower(c));
    return text;
}
bool Monstey_CWithin(const std::filesystem::path &path, const std::filesystem::path &directory)
{
    const auto p = Monstey_CKey(path), d = Monstey_CKey(directory);
    return p == d || (p.size() > d.size() && p.starts_with(d) && (p[d.size()] == L'\\' || p[d.size()] == L'/'));
}
struct Monstey_CStamp
{
    std::uint64_t volume{}, identity{}, size{}, time{};
    bool operator==(const Monstey_CStamp &) const = default;
    J Json() const
    {
        return J::array({volume, identity, size, time});
    }
};
Monstey_CStamp Monstey_CStat(const std::filesystem::path &path)
{
    const auto handle =
        CreateFileW(Monstey_WindowsFilePath(path).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Monstey_Require(handle != INVALID_HANDLE_VALUE, "Cannot read source file identity");
    BY_HANDLE_FILE_INFORMATION info{};
    const auto ok = GetFileInformationByHandle(handle, &info);
    CloseHandle(handle);
    Monstey_Require(ok != FALSE && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0,
                    "Cannot fingerprint source file");
    return {info.dwVolumeSerialNumber, (static_cast<std::uint64_t>(info.nFileIndexHigh) << 32) | info.nFileIndexLow,
            (static_cast<std::uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow,
            (static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime) << 32) |
                info.ftLastWriteTime.dwLowDateTime};
}
std::filesystem::path Monstey_CNonce(const std::filesystem::path &parent, const wchar_t *extension)
{
    static std::atomic<std::uint64_t> count{};
    return parent / (L".Monstey-Catalog-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                     std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(++count) + extension);
}
B Monstey_CRead(const std::filesystem::path &path, std::uint64_t start, std::uint64_t count,
                std::uint64_t limit = 64ull * 1024 * 1024)
{
    Monstey_Require(count <= limit && start <= std::numeric_limits<std::uint64_t>::max() - count,
                    "Bounded metadata range exceeds limit");
    const auto size = std::filesystem::file_size(path);
    Monstey_Require(start <= size && count <= size - start, "Metadata range exceeds file");
    std::ifstream file(Monstey_WindowsFilePath(path), std::ios::binary);
    Monstey_Require(file.good() && start <= static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()),
                    "Cannot open metadata source");
    file.seekg(static_cast<std::streamoff>(start));
    B result(static_cast<std::size_t>(count));
    if (count)
        file.read(reinterpret_cast<char *>(result.data()), static_cast<std::streamsize>(count));
    Monstey_Require(static_cast<std::uint64_t>(file.gcount()) == count || count == 0, "Truncated metadata read");
    return result;
}
std::string Monstey_CIndexHash(const std::filesystem::path &path, std::uint64_t size)
{
    return Monstey_HashSha256(Monstey_CRead(path, 0, size, 256ull * 1024 * 1024));
}
void Monstey_CRequireKeys(const J &a, std::initializer_list<std::string_view> names)
{
    Monstey_Require(a.is_object(), "Arguments must be a JSON object");
    for (const auto &[name, value] : a.as_object())
    {
        (void)value;
        Monstey_Require(std::find(names.begin(), names.end(), name) != names.end(), "Unknown argument: " + name);
    }
}
bool Monstey_CBool(const J &a, std::string_view key, bool fallback)
{
    if (!a.contains(key))
        return fallback;
    Monstey_Require(a.at(key).is_bool(), std::string(key) + " must be a Boolean");
    return a.at(key).as_bool();
}
std::string Monstey_CArg(const J &a, std::string_view key, std::string fallback = {})
{
    if (!a.contains(key) || a.at(key).is_null())
        return fallback;
    Monstey_Require(a.at(key).is_string(), std::string(key) + " must be a string");
    return a.at(key).as_string();
}
// Linear wildcard matching with an explicit star retry point; brackets support
// native fnmatch-style ranges/negation without regex or locale transformations.
bool Monstey_CGlob(std::string_view pattern, std::string_view name)
{
    auto token = [&](std::size_t &p, char c) {
        if (pattern[p] == '?')
        {
            ++p;
            return true;
        }
        if (pattern[p] != '[')
            return pattern[p++] == c;
        const auto end = pattern.find(']', p + 1);
        if (end == pattern.npos)
            return pattern[p++] == c;
        std::size_t at = p + 1;
        const bool negate = at < end && (pattern[at] == '!' || pattern[at] == '^');
        if (negate)
            ++at;
        bool match = false;
        while (at < end)
        {
            const auto first = pattern[at++];
            if (at + 1 < end && pattern[at] == '-')
            {
                ++at;
                const auto last = pattern[at++];
                match = match || (c >= first && c <= last);
            }
            else
                match = match || c == first;
        }
        p = end + 1;
        return negate ? !match : match;
    };
    std::size_t p = 0, n = 0, star = pattern.npos, retry = 0;
    while (n < name.size())
    {
        if (p < pattern.size() && pattern[p] == '*')
        {
            star = ++p;
            retry = n;
        }
        else if (p < pattern.size())
        {
            auto next = p;
            if (token(next, name[n]))
            {
                p = next;
                ++n;
                continue;
            }
            if (star == pattern.npos)
                return false;
            p = star;
            n = ++retry;
        }
        else
        {
            if (star == pattern.npos)
                return false;
            p = star;
            n = ++retry;
        }
    }
    while (p < pattern.size() && pattern[p] == '*')
        ++p;
    return p == pattern.size();
}
bool Monstey_CMatches(const std::filesystem::path &path, const std::string &filter)
{
    if (filter.empty())
        return true;
    const auto name = Monstey_CLower(Monstey_CText(path.filename()));
    return filter.find_first_of("*?[") == filter.npos ? name.find(filter) != name.npos : Monstey_CGlob(filter, name);
}
struct Monstey_CMount
{
    std::filesystem::path source;
    std::string kind;
    std::uint64_t priority{};
};
struct Monstey_CMounts
{
    std::filesystem::path root, config;
    B configBytes;
    Monstey_CStamp configStamp;
    std::string configHash;
    std::vector<Monstey_CMount> mounts;
    J missing = J::array();
};
Monstey_CMounts Monstey_CLoadMounts(const std::filesystem::path &directory)
{
    Monstey_CMounts out;
    out.root = Monstey_CPath(directory);
    Monstey_Require(std::filesystem::is_directory(out.root), "Game directory does not exist");
    out.config = out.root / L"x64/default.archcfg";
    Monstey_Require(std::filesystem::is_regular_file(out.config), "Game directory must contain x64/default.archcfg");
    out.configStamp = Monstey_CStat(out.config);
    out.configBytes = Monstey_ReadFile(out.config, 1024 * 1024);
    out.configHash = Monstey_HashSha256(out.configBytes);
    std::string text(out.configBytes.begin(), out.configBytes.end());
    if (text.starts_with("\xef\xbb\xbf"))
        text.erase(0, 3);
    (void)Monstey_Wide(text);
    std::size_t start = 0;
    std::uint64_t line = 0;
    while (start <= text.size())
    {
        const auto end = text.find('\n', start);
        auto value = text.substr(start, end == text.npos ? text.size() - start : end - start);
        ++line;
        const auto comment = value.find(';');
        if (comment != value.npos)
            value.resize(comment);
        const auto first = value.find_first_not_of(" \t\r\n");
        if (first != value.npos)
        {
            value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
            while (!value.empty() && value.front() == '"')
                value.erase(value.begin());
            while (!value.empty() && value.back() == '"')
                value.pop_back();
            if (value.empty())
            {
                if (end == text.npos)
                    break;
                start = end + 1;
                continue;
            }
            std::replace(value.begin(), value.end(), '/', static_cast<char>('\\'));
            const auto path = Monstey_CPath(out.config.parent_path() / std::filesystem::path(Monstey_Wide(value)));
            if (std::filesystem::is_directory(path))
                out.mounts.push_back({path, "loose", line});
            else if (std::filesystem::is_regular_file(path) &&
                     (Monstey_CLower(Monstey_CText(path.extension())) == ".arch06" ||
                      Monstey_CLower(Monstey_CText(path.extension())) == ".arch05"))
                out.mounts.push_back({path, "archive", line});
            else
                out.missing.as_array().push_back(J::object({{"line", line}, {"source", Monstey_CText(path)}}));
        }
        if (end == text.npos)
            break;
        start = end + 1;
    }
    Monstey_Require(Monstey_CStat(out.config) == out.configStamp, "Archive configuration changed during reading");
    return out;
}
struct Monstey_CLoose
{
    std::string path;
    std::filesystem::path file;
    Monstey_CStamp stamp;
    bool operator==(const Monstey_CLoose &) const = default;
};
std::vector<Monstey_CLoose> Monstey_CLooseFiles(const std::filesystem::path &directory)
{
    std::vector<Monstey_CLoose> rows;
    std::set<std::string> paths;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(directory))
    {
        const auto attributes = GetFileAttributesW(entry.path().c_str());
        Monstey_Require(attributes != INVALID_FILE_ATTRIBUTES, "Cannot enumerate mounted loose path");
        Monstey_Require((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0,
                        "Mounted loose paths cannot be reparse points");
        if (!entry.is_regular_file())
            continue;
        auto relative = Monstey_ArchiveVirtual(Monstey_CText(entry.path().lexically_relative(directory)));
        Monstey_Require(paths.insert(relative).second, "Duplicate case-insensitive loose resource path");
        rows.push_back({relative, entry.path(), Monstey_CStat(entry.path())});
    }
    std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.path < b.path; });
    return rows;
}
class Monstey_CStage
{
  public:
    explicit Monstey_CStage(const std::filesystem::path &parent) : path_(Monstey_CNonce(parent, L".partial"))
    {
        handle_ = CreateFileW(Monstey_WindowsFilePath(path_).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        Monstey_Require(handle_ != INVALID_HANDLE_VALUE, "Cannot create catalog staging file");
        buffer_.reserve(1024 * 1024);
    }
    ~Monstey_CStage()
    {
        if (handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(handle_);
        if (!published_)
        {
            std::error_code error;
            std::filesystem::remove(Monstey_WindowsFilePath(path_), error);
        }
    }
    void Add(std::string_view text)
    {
        buffer_.insert(buffer_.end(), text.begin(), text.end());
        if (buffer_.size() >= 1024 * 1024)
            Flush();
    }
    void Finish()
    {
        Flush();
        Monstey_Require(FlushFileBuffers(handle_) != FALSE, "Cannot flush catalog staging output");
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    const std::filesystem::path &Path() const
    {
        return path_;
    }
    void Publish(const std::filesystem::path &target)
    {
        Monstey_Require(handle_ == INVALID_HANDLE_VALUE, "Finish catalog before publication");
        Monstey_Require(MoveFileExW(Monstey_WindowsFilePath(path_).c_str(), Monstey_WindowsFilePath(target).c_str(), MOVEFILE_WRITE_THROUGH) != FALSE,
                        "Catalog output exists or cannot be published");
        published_ = true;
    }

  private:
    std::filesystem::path path_;
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    B buffer_;
    bool published_ = false;
    void Flush()
    {
        std::size_t at = 0;
        while (at < buffer_.size())
        {
            const auto count = static_cast<DWORD>(std::min<std::size_t>(buffer_.size() - at, 1024 * 1024));
            DWORD written = 0;
            Monstey_Require(WriteFile(handle_, buffer_.data() + at, count, &written, nullptr) != FALSE && written != 0,
                            "Partial catalog staging write");
            at += written;
        }
        buffer_.clear();
    }
};
template <class Callback> void Monstey_CLines(const std::filesystem::path &path, Callback callback)
{
    std::ifstream file(Monstey_WindowsFilePath(path), std::ios::binary);
    Monstey_Require(file.good(), "Cannot open JSONL catalog");
    std::vector<char> buffer(1024 * 1024 + 1);
    std::uint64_t number = 0;
    while (file.good())
    {
        file.getline(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = file.gcount();
        if (count == 0 && file.eof())
            break;
        Monstey_Require(!file.bad() && (!file.fail() || file.eof()), "Catalog row exceeds 1 MiB or input read failed");
        auto length = static_cast<std::size_t>(count);
        if (!file.eof() && length)
            --length;
        if (length && buffer[length - 1] == '\r')
            --length;
        Monstey_Require(length != 0, "Empty catalog JSONL row");
        callback(J::Parse(std::string_view(buffer.data(), length)), number++);
    }
    Monstey_Require(file.eof(), "Catalog stream did not end cleanly");
}
std::filesystem::path Monstey_CSummary(const std::filesystem::path &output)
{
    return output.parent_path() / (output.stem().wstring() + L"-summary.json");
}
} // namespace
namespace
{
std::filesystem::path Monstey_CExecutable()
{
    std::vector<wchar_t> path(32768);
    const auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    Monstey_Require(size && size < path.size(), "Cannot resolve native executable path");
    return std::filesystem::path(std::wstring(path.data(), size));
}
void Monstey_CSettingsCheck(const std::filesystem::path &path, bool existed, const std::string &hash)
{
    Monstey_Require(std::filesystem::exists(path) == existed,
                    "Settings changed after loading; reopen their source before saving");
    if (existed)
        Monstey_Require(std::filesystem::is_regular_file(path) && Monstey_HashSha256(path) == hash,
                        "Settings content changed after loading; reopen before saving");
}
void Monstey_CSettingsWrite(const std::filesystem::path &path, const std::string &text, bool existed,
                            const std::string &oldHash)
{
    Monstey_CSettingsCheck(path, existed, oldHash);
    std::filesystem::create_directories(Monstey_WindowsFilePath(path.parent_path()));
    const auto stage = Monstey_CNonce(path.parent_path(), L".settings.json"),
               backup = Monstey_CNonce(path.parent_path(), L".backup.json");
    bool published = false;
    const auto newHash =
        Monstey_HashSha256(std::span(reinterpret_cast<const std::uint8_t *>(text.data()), text.size()));
    try
    {
        Monstey_WriteNewText(stage, text);
        Monstey_CSettingsCheck(path, existed, oldHash);
        if (existed)
        {
            Monstey_Require(ReplaceFileW(path.c_str(), stage.c_str(), backup.c_str(), 0, nullptr, nullptr) != FALSE,
                            "Cannot atomically replace settings; original source retained");
        }
        else
            Monstey_Require(MoveFileExW(Monstey_WindowsFilePath(stage).c_str(), Monstey_WindowsFilePath(path).c_str(), MOVEFILE_WRITE_THROUGH) != FALSE,
                            "Settings destination exists or cannot be created");
        published = true;
        Monstey_Require(Monstey_HashSha256(path) == newHash, "Settings publication readback differs");
        if (existed)
        {
            Monstey_Require(std::filesystem::is_regular_file(backup) && Monstey_HashSha256(backup) == oldHash,
                            "Settings recovery backup differs");
            Monstey_Require(DeleteFileW(backup.c_str()) != FALSE,
                            "Cannot remove completed settings transaction backup");
        }
    }
    catch (...)
    {
        std::error_code error;
        std::filesystem::remove(stage, error);
        if (published && std::filesystem::is_regular_file(path) && Monstey_HashSha256(path) == newHash)
        {
            if (existed && std::filesystem::is_regular_file(backup) && Monstey_HashSha256(backup) == oldHash)
                (void)ReplaceFileW(path.c_str(), backup.c_str(), nullptr, 0, nullptr, nullptr);
            else if (!existed)
                std::filesystem::remove(path, error);
        }
        else if (!std::filesystem::exists(path) && existed && std::filesystem::is_regular_file(backup) &&
                 Monstey_HashSha256(backup) == oldHash)
            (void)MoveFileExW(Monstey_WindowsFilePath(backup).c_str(), Monstey_WindowsFilePath(path).c_str(), MOVEFILE_WRITE_THROUGH);
        throw;
    }
}
} // namespace
struct Monstey_Settings::Monstey_Impl
{
    mutable std::mutex mutex;
    std::filesystem::path path;
    std::vector<std::filesystem::path> protectedSources;
    J current = J::object({{"schema", "Monstey.WorkbenchSettings/1"},
                           {"game_root", nullptr},
                           {"blender", nullptr},
                           {"converter_root", nullptr}}),
      saved = nullptr;
    std::string hash, error;
    bool existed = false;
    explicit Monstey_Impl(const std::filesystem::path &input, std::vector<std::filesystem::path> sources)
        : path(Monstey_CPath(input.empty() ? Monstey_CExecutable().parent_path() / L"settings.json" : input)),
          protectedSources(std::move(sources))
    {
        existed = std::filesystem::exists(path);
        if (!existed)
            return;
        try
        {
            const auto raw = Monstey_ReadFile(path, 1024 * 1024);
            hash = Monstey_HashSha256(raw);
            const auto value = J::Parse(std::string(raw.begin(), raw.end()));
            Monstey_Require(value.is_object() && value.contains("schema") && value.at("schema").is_string() &&
                                value.at("schema").as_string() == "Monstey.WorkbenchSettings/1",
                            "Unsupported Workbench settings format");
            for (const auto key : {"game_root", "blender", "converter_root"})
                if (value.contains(key))
                {
                    Monstey_Require(value.at(key).is_null() || value.at(key).is_string(), "Invalid settings path type");
                    if (value.at(key).is_string())
                        Monstey_Require(value.at(key).as_string().find('\0') == std::string::npos &&
                                            Monstey_Wide(value.at(key).as_string()).size() < 32768,
                                        "Invalid settings path encoding or length");
                }
            current = value;
            for (const auto key : {"game_root", "blender", "converter_root"})
                if (!current.contains(key))
                    current[key] = nullptr;
            saved = current;
        }
        catch (const std::exception &e)
        {
            error = e.what();
        }
    }
    J Describe() const
    {
        J out = current;
        out["config_path"] = Monstey_CText(path);
        out["configuration_error"] = error.empty() ? J() : J(error);
        out["settings_source_sha256"] = hash.empty() ? J() : J(hash);
        out["settings_saved"] = saved == current;
        out["converter_available"] = true;
        out["converter_implementation"] = "builtin:C++20";
        out["converter_root_policy"] = "Legacy configuration metadata only; no Python program is executed";
        out["world_api"] = "builtin:C++20";
        out["implementation"] = "C++20";
        out["python_runtime"] = false;
        out["runtime_verified"] = false;
        const auto available = [&](const char *key, bool directory) {
            if (!current.contains(key) || current.at(key).is_null() || current.at(key).as_string().empty())
                return false;
            const auto p = std::filesystem::path(Monstey_Wide(current.at(key).as_string()));
            return directory ? std::filesystem::is_directory(p) : std::filesystem::is_regular_file(p);
        };
        out["blender_available"] = available("blender", false);
        out["converter_root_available"] = available("converter_root", true);
        out["game_available"] =
            available("game_root", true) &&
            std::filesystem::is_regular_file(std::filesystem::path(Monstey_Wide(current.at("game_root").as_string())) /
                                             L"x64/default.archcfg");
        const auto executable = Monstey_CExecutable();
        out["native_component"] =
            J::object({{"path", Monstey_CText(executable)}, {"sha256", Monstey_HashSha256(executable)}});
        out["operations"] = J::array({"world_catalog", "world_call", "model_inspect", "model_convert",
                                      "model_export_glb", "model_roundtrip", "archive_catalog"});
        return out;
    }
};
Monstey_Settings::Monstey_Settings(const std::filesystem::path &path, std::vector<std::filesystem::path> sources)
    : impl_(std::make_unique<Monstey_Impl>(path, std::move(sources)))
{
}
Monstey_Settings::~Monstey_Settings() = default;
Monstey_Settings::Monstey_Settings(Monstey_Settings &&) noexcept = default;
Monstey_Settings &Monstey_Settings::operator=(Monstey_Settings &&) noexcept = default;
J Monstey_Settings::Describe() const
{
    std::lock_guard lock(impl_->mutex);
    return impl_->Describe();
}
void Monstey_Settings::Protect(const std::filesystem::path &source)
{
    if (source.empty())
        return;
    std::lock_guard lock(impl_->mutex);
    impl_->protectedSources.push_back(Monstey_CPath(source));
}
J Monstey_Settings::Configure(const J &arguments)
{
    auto &p = *impl_;
    std::lock_guard lock(p.mutex);
    Monstey_CRequireKeys(
        arguments, {"game_root", "blender", "converter_root", "save", "expected_generation", "expected_revision"});
    const bool save = Monstey_CBool(arguments, "save", true);
    J candidate = p.current;
    for (const auto key : {"game_root", "blender", "converter_root"})
        if (arguments.contains(key) && !arguments.at(key).is_null())
        {
            Monstey_Require(arguments.at(key).is_string(), std::string(key) + " must be a path string or null");
            if (!arguments.at(key).as_string().empty())
                candidate[key] =
                    Monstey_CText(Monstey_CPath(std::filesystem::path(Monstey_Wide(arguments.at(key).as_string()))));
        }
    if (!candidate.at("game_root").is_null())
        Monstey_Require(
            std::filesystem::is_regular_file(
                std::filesystem::path(Monstey_Wide(candidate.at("game_root").as_string())) / L"x64/default.archcfg"),
            "Game folder must contain x64/default.archcfg");
    if (!candidate.at("blender").is_null())
        Monstey_Require(
            std::filesystem::is_regular_file(std::filesystem::path(Monstey_Wide(candidate.at("blender").as_string()))),
            "Blender executable does not exist");
    if (!candidate.at("converter_root").is_null())
        Monstey_Require(std::filesystem::is_directory(
                            std::filesystem::path(Monstey_Wide(candidate.at("converter_root").as_string()))),
                        "Legacy converter metadata folder does not exist");
    if (save)
    {
        Monstey_Require(!p.existed || !p.hash.empty(),
                        "Existing settings could not be source-qualified; choose a new settings path");
        Monstey_Require(Monstey_CLower(Monstey_CText(p.path.extension())) == ".json",
                        "Settings filename must end with .json");
        Monstey_RequireOfflineOutput(p.path, p.protectedSources);
        for (const auto *document : {&p.current, &candidate})
            if (document->contains("game_root") && !document->at("game_root").is_null())
                Monstey_Require(!Monstey_CWithin(
                                    p.path, std::filesystem::path(Monstey_Wide(document->at("game_root").as_string()))),
                                "Save settings outside the selected game directory");
        const auto text = candidate.dump(2) + "\n";
        Monstey_CSettingsWrite(p.path, text, p.existed, p.hash);
        p.existed = true;
        p.hash = Monstey_HashSha256(p.path);
        p.saved = candidate;
    }
    p.current = std::move(candidate);
    p.error.clear();
    auto out = p.Describe();
    out["saved"] = save;
    return out;
}
J Monstey_ArchiveMounts(const std::filesystem::path &gameRoot)
{
    const auto mounts = Monstey_CLoadMounts(gameRoot);
    J rows = J::array();
    for (const auto &mount : mounts.mounts)
        rows.as_array().push_back(
            J::object({{"source", Monstey_CText(mount.source)}, {"kind", mount.kind}, {"priority", mount.priority}}));
    return J::object({{"game_root", Monstey_CText(mounts.root)},
                      {"config", Monstey_CText(mounts.config)},
                      {"config_sha256", mounts.configHash},
                      {"config_stamp", mounts.configStamp.Json()},
                      {"mounts", rows},
                      {"missing_mounts", mounts.missing},
                      {"mount_order", "Physical config line numbers; later lines override earlier direct resources"},
                      {"game_written", false}});
}
namespace
{
struct Monstey_CChild
{
    std::string path;
    std::uint64_t offset{}, size{};
};
class Monstey_CMetadata
{
  public:
    Monstey_CMetadata(const std::filesystem::path &source, std::shared_ptr<const Monstey_Archive> archive = {},
                      std::optional<Monstey_ArchiveEntry> entry = std::nullopt, const std::filesystem::path &dll = {})
        : path_(source)
    {
        if (archive)
        {
            Monstey_Require(entry.has_value(), "Archive parent entry required");
            range_ = std::make_unique<Monstey_ArchiveRange>(std::move(archive), *entry, dll);
            size_ = range_->Size();
        }
        else
        {
            size_ = std::filesystem::file_size(Monstey_WindowsFilePath(source));
            loose_.open(Monstey_WindowsFilePath(source), std::ios::binary);
            Monstey_Require(loose_.good(), "Cannot open loose EMBB metadata source");
        }
    }
    B Read(std::uint64_t start, std::uint64_t count)
    {
        Monstey_Require(count <= 64ull * 1024 * 1024 && start <= size_ && count <= size_ - start,
                        "EMBB metadata range exceeds bounds");
        auto bytes = Raw(start, count);
        Monstey_Require(fingerprint_.size() + bytes.size() + 16 <= 80ull * 1024 * 1024,
                        "EMBB metadata fingerprint exceeds 80 MiB budget");
        Monstey_AppendU64(fingerprint_, start);
        Monstey_AppendU64(fingerprint_, count);
        fingerprint_.insert(fingerprint_.end(), bytes.begin(), bytes.end());
        metadataBytes_ += count;
        return bytes;
    }
    std::pair<std::uint32_t, std::vector<Monstey_CChild>> Children()
    {
        const auto header = Read(0, 16);
        Monstey_Require(std::string_view(reinterpret_cast<const char *>(header.data()), 4) == "EMBB",
                        "Expected EMBB metadata header");
        const auto version = Monstey_U32(header, 4), names = Monstey_U32(header, 8), count = Monstey_U32(header, 12);
        Monstey_Require((version == 1 || version == 5) && names <= 64u * 1024 * 1024 && count <= 100000,
                        "Unsupported EMBB version or metadata counts");
        const auto length = static_cast<std::uint64_t>(names) + static_cast<std::uint64_t>(count) * 4;
        Monstey_Require(length <= 64ull * 1024 * 1024 && 16 + length <= size_, "EMBB name/offset table exceeds parent");
        const auto table = Read(16, length);
        std::vector<Monstey_CChild> rows;
        rows.reserve(count);
        std::set<std::string> seen;
        std::uint64_t cursor = 16 + length;
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto nameOffset =
                Monstey_U32(table, static_cast<std::size_t>(names) + static_cast<std::size_t>(i) * 4);
            Monstey_Require(nameOffset < names, "EMBB name offset outside pool");
            auto end = static_cast<std::size_t>(nameOffset);
            const auto maximum = std::min<std::size_t>(names, static_cast<std::size_t>(nameOffset) + 32768);
            while (end < maximum && table[end])
                ++end;
            Monstey_Require(end < maximum, "EMBB resource name is unterminated or exceeds 32767 bytes");
            const auto raw = std::string(reinterpret_cast<const char *>(table.data() + nameOffset), end - nameOffset);
            (void)Monstey_Wide(raw);
            auto path = Monstey_ArchiveVirtual(raw);
            (void)Monstey_PathHash(path);
            Monstey_Require(seen.insert(path).second, "Duplicate case-insensitive EMBB resource path");
            Monstey_Require(cursor <= size_ && 4 <= size_ - cursor, "Missing EMBB child size header");
            const auto size = Monstey_U32(Read(cursor, 4), 0);
            cursor += 4;
            Monstey_Require(size <= size_ - cursor, "EMBB child payload exceeds parent");
            rows.push_back({std::move(path), cursor, size});
            cursor += size;
        }
        Monstey_Require(cursor == size_, "Unexpected EMBB trailing bytes");
        return {version, std::move(rows)};
    }
    B Payload(std::uint64_t start, std::uint64_t count)
    {
        Monstey_Require(count <= 1024 * 1024 && start <= size_ && count <= size_ - start,
                        "Child extraction range exceeds parent or 1 MiB chunk budget");
        return Raw(start, count);
    }
    std::string Digest() const
    {
        return Monstey_HashSha256(fingerprint_);
    }
    J Stats() const
    {
        J result = J::object(
            {{"metadata_bytes_read", metadataBytes_}, {"parent_bytes", size_}, {"payload_materialized", false}});
        if (range_)
        {
            const auto stats = range_->Stats();
            result["chunks"] = stats.chunks;
            result["decoded_chunk_loads"] = stats.decoded_chunk_loads;
            result["cache_hits"] = stats.cache_hits;
            result["cached_chunks"] = stats.cached_chunks;
            result["stored_window_reads"] = stats.metadata_window_reads;
            result["stored_metadata_bytes_read"] = stats.metadata_bytes_read;
            result["stored_window_peak"] = stats.metadata_window_peak;
        }
        return result;
    }

  private:
    std::filesystem::path path_;
    B Raw(std::uint64_t start, std::uint64_t count)
    {
        if (range_)
            return range_->Read(start, count);
        Monstey_Require(start <= static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()),
                        "Loose metadata offset exceeds stream limit");
        loose_.clear();
        loose_.seekg(static_cast<std::streamoff>(start));
        B out(static_cast<std::size_t>(count));
        if (count)
            loose_.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(count));
        Monstey_Require(count == 0 || static_cast<std::uint64_t>(loose_.gcount()) == count,
                        "Truncated loose EMBB metadata read");
        return out;
    }
    std::ifstream loose_;
    std::unique_ptr<Monstey_ArchiveRange> range_;
    std::uint64_t size_{}, metadataBytes_{};
    B fingerprint_;
};
struct Monstey_CParent
{
    std::filesystem::path source;
    std::shared_ptr<const Monstey_Archive> archive;
    std::optional<Monstey_ArchiveEntry> entry;
    std::string digest, bundle;
};
struct Monstey_CFingerprint
{
    std::filesystem::path path;
    Monstey_CStamp stamp;
    std::uint64_t bytes{}, priority{};
    std::string hash;
};
struct Monstey_CWinner
{
    std::uint64_t row{}, priority{};
    std::string source, kind;
};
std::string Monstey_CVirtualRow(const J &row)
{
    Monstey_Require(row.is_object() && row.contains("path") && row.at("path").is_string(), "Catalog row needs a path");
    const auto path = Monstey_ArchiveVirtual(row.at("path").as_string());
    (void)Monstey_PathHash(path);
    if (row.contains("resource_path"))
        Monstey_Require(row.at("resource_path").is_string() &&
                            Monstey_ArchiveVirtual(row.at("resource_path").as_string()) == path,
                        "Catalog resource aliases disagree");
    if (row.contains("path_hash"))
        Monstey_Require(row.at("path_hash").is_integer() && row.at("path_hash").as_uint() == Monstey_PathHash(path),
                        "Catalog native path hash mismatch");
    return path;
}
void Monstey_CVerifyConfig(const Monstey_CMounts &mounts)
{
    Monstey_Require(Monstey_CStat(mounts.config) == mounts.configStamp &&
                        Monstey_ReadFile(mounts.config, 1024 * 1024) == mounts.configBytes,
                    "Archive configuration changed during catalog generation");
}
} // namespace
J Monstey_ArchiveCatalog(const std::filesystem::path &gameRoot, const std::filesystem::path &destination,
                         const J &options)
{
    Monstey_CRequireKeys(options, {"include_embedded", "oodle_dll", "archive_filter"});
    const auto embedded = Monstey_CBool(options, "include_embedded", false);
    const auto filter = Monstey_CLower(Monstey_CArg(options, "archive_filter"));
    Monstey_Require(filter.size() <= 512, "Archive filter exceeds 512 bytes");
    const auto dllText = Monstey_CArg(options, "oodle_dll");
    const auto dll =
        dllText.empty() ? std::filesystem::path{} : Monstey_CPath(std::filesystem::path(Monstey_Wide(dllText)));
    if (!dll.empty())
        Monstey_Require(std::filesystem::is_regular_file(dll), "Selected Oodle DLL does not exist");
    const auto mounts = Monstey_CLoadMounts(gameRoot);
    const auto output = Monstey_CPath(destination), summaryPath = Monstey_CSummary(output);
    Monstey_Require(Monstey_CLower(Monstey_CText(output.extension())) == ".jsonl",
                    "Catalog output must have a .jsonl extension");
    Monstey_RequireOfflineOutput(output);
    Monstey_RequireOfflineOutput(summaryPath);
    Monstey_Require(!Monstey_CWithin(output, mounts.root),
                    "Catalog output must stay outside the selected game directory");
    Monstey_Require(!std::filesystem::exists(output) && !std::filesystem::exists(summaryPath),
                    "Catalog or summary exists; choose new output names");
    std::vector<Monstey_CMount> selected;
    for (const auto &mount : mounts.mounts)
        if (mount.kind == "loose" || Monstey_CMatches(mount.source, filter))
        {
            if (mount.kind == "loose")
                Monstey_Require(!Monstey_CWithin(output, mount.source),
                                "Catalog cannot be inside a mounted loose resource directory");
            selected.push_back(mount);
        }
    if (!filter.empty())
        Monstey_Require(
            std::any_of(selected.begin(), selected.end(), [](const auto &row) { return row.kind == "archive"; }),
            "No mounted archive matches the filter");
    Monstey_Require(!selected.empty(), "No available selected mounted archives or loose directories");
    J missing = J::array();
    for (const auto &row : mounts.missing.as_array())
        if (Monstey_CMatches(std::filesystem::path(Monstey_Wide(row.at("source").as_string())), filter))
            missing.as_array().push_back(row);
    std::filesystem::create_directories(Monstey_WindowsFilePath(output.parent_path()));
    Monstey_CStage rawRows(output.parent_path());
    std::unordered_map<std::string, Monstey_CWinner> winners;
    std::vector<Monstey_CFingerprint> fingerprints;
    std::vector<std::pair<std::filesystem::path, std::vector<Monstey_CLoose>>> looseSnapshots;
    std::vector<Monstey_CParent> parents;
    J sources = J::array(), failures = J::array();
    std::uint64_t rowCount = 0, directCount = 0, looseCount = 0, embeddedCount = 0;
    auto emit = [&](J row) {
        const auto path = Monstey_CVirtualRow(row);
        row["path"] = path;
        row["resource_path"] = path;
        row["path_hash"] = Monstey_PathHash(path);
        row["path_hash_hex"] = Monstey_Hex(Monstey_PathHash(path));
        row["catalog_row"] = rowCount;
        const auto current = rowCount++;
        const auto kind = row.at("kind").as_string();
        if (kind == "archive")
            ++directCount;
        else if (kind == "loose")
            ++looseCount;
        else if (kind == "embedded")
            ++embeddedCount;
        if (kind != "embedded")
        {
            const auto priority = row.at("priority").as_uint();
            const auto found = winners.find(path);
            if (found == winners.end() || found->second.priority < priority)
                winners[path] = {current, priority};
        }
        try
        {
            rawRows.Add(row.dump());
            rawRows.Add("\n");
        }
        catch (const std::exception &error)
        {
            throw Monstey_Error("catalog_write_failed", error.what());
        }
        return current;
    };
    auto parent = [&](J row, const std::filesystem::path &source, std::shared_ptr<const Monstey_Archive> archive = {},
                      std::optional<Monstey_ArchiveEntry> entry = std::nullopt) {
        try
        {
            Monstey_CMetadata view(source, archive, entry, dll);
            auto [version, children] = view.Children();
            row["embedded_index_status"] = "complete";
            row["embedded_version"] = version;
            row["embedded_child_count"] = children.size();
            row["embedded_metadata_sha256"] = view.Digest();
            row["embedded_ranges"] = view.Stats();
            const auto parentRow = emit(row);
            for (const auto &child : children)
            {
                emit(J::object({{"path", child.path},
                                {"bundle", row.at("resource_path")},
                                {"archive", row.at("archive")},
                                {"archive_name", row.value("archive_name", nullptr)},
                                {"priority", row.at("priority")},
                                {"kind", "embedded"},
                                {"size", child.size},
                                {"bytes", child.size},
                                {"offset", child.offset},
                                {"offset_space", "decoded_parent"},
                                {"parent_entry_offset", row.value("offset", nullptr)},
                                {"parent_file", row.value("file", nullptr)},
                                {"parent_kind", row.at("kind")},
                                {"parent_catalog_row", parentRow}}));
                if (child.path.ends_with(".embb"))
                    failures.as_array().push_back(
                        J::object({{"kind", "nested_embedded"},
                                   {"archive", row.at("archive")},
                                   {"bundle", row.at("resource_path")},
                                   {"resource", child.path},
                                   {"error", "Nested EMBB metadata remains parent-scoped and is not expanded"}}));
            }
            parents.push_back(
                {source, std::move(archive), std::move(entry), view.Digest(), row.at("resource_path").as_string()});
        }
        catch (const Monstey_Error &error)
        {
            if (error.code == "catalog_write_failed")
                throw;
            row["embedded_index_status"] = "failed";
            row["embedded_error"] = error.what();
            emit(row);
            failures.as_array().push_back(J::object({{"kind", "embedded"},
                                                     {"archive", row.at("archive")},
                                                     {"bundle", row.at("resource_path")},
                                                     {"error", error.what()}}));
        }
        catch (const std::exception &error)
        {
            row["embedded_index_status"] = "failed";
            row["embedded_error"] = error.what();
            emit(row);
            failures.as_array().push_back(J::object({{"kind", "embedded"},
                                                     {"archive", row.at("archive")},
                                                     {"bundle", row.at("resource_path")},
                                                     {"error", error.what()}}));
        }
    };
    for (const auto &mount : selected)
    {
        if (mount.kind == "archive")
        {
            try
            {
                const auto stamp = Monstey_CStat(mount.source);
                auto archive = std::make_shared<Monstey_Archive>(mount.source);
                const auto indexHash = Monstey_CIndexHash(mount.source, archive->IndexLength());
                Monstey_Require(Monstey_CStat(mount.source) == stamp, "Archive changed during index reading");
                fingerprints.push_back({mount.source, stamp, archive->IndexLength(), mount.priority, indexHash});
                sources.as_array().push_back(J::object({{"path", Monstey_CText(mount.source)},
                                                        {"kind", "archive"},
                                                        {"stamp", stamp.Json()},
                                                        {"index_bytes", archive->IndexLength()},
                                                        {"index_sha256", indexHash},
                                                        {"priority", mount.priority}}));
                for (const auto &entry : archive->Entries())
                {
                    J row = J::object({{"path", entry.path},
                                       {"resource_path", entry.path},
                                       {"archive", Monstey_CText(mount.source)},
                                       {"archive_name", Monstey_CText(mount.source.filename())},
                                       {"version", archive->Version()},
                                       {"endian", archive->BigEndian() ? ">" : "<"},
                                       {"priority", mount.priority},
                                       {"kind", "archive"},
                                       {"offset", entry.offset},
                                       {"offset_space", "archive"},
                                       {"size", entry.size},
                                       {"bytes", entry.size},
                                       {"compressed_size", entry.stored_size},
                                       {"stored_size", entry.stored_size},
                                       {"flags", entry.flags}});
                    if (embedded && entry.path.ends_with(".embb"))
                        parent(row, mount.source, archive, entry);
                    else
                        emit(row);
                }
            }
            catch (const Monstey_Error &error)
            {
                if (error.code == "catalog_write_failed")
                    throw;
                failures.as_array().push_back(J::object(
                    {{"kind", "archive"}, {"archive", Monstey_CText(mount.source)}, {"error", error.what()}}));
            }
            catch (const std::exception &error)
            {
                failures.as_array().push_back(J::object(
                    {{"kind", "archive"}, {"archive", Monstey_CText(mount.source)}, {"error", error.what()}}));
            }
        }
        else
        {
            try
            {
                auto files = Monstey_CLooseFiles(mount.source);
                looseSnapshots.push_back({mount.source, files});
                sources.as_array().push_back(J::object({{"path", Monstey_CText(mount.source)},
                                                        {"kind", "loose"},
                                                        {"files", files.size()},
                                                        {"priority", mount.priority}}));
                for (const auto &file : files)
                {
                    J row = J::object({{"path", file.path},
                                       {"resource_path", file.path},
                                       {"archive", nullptr},
                                       {"file", Monstey_CText(file.file)},
                                       {"priority", mount.priority},
                                       {"kind", "loose"},
                                       {"size", file.stamp.size},
                                       {"bytes", file.stamp.size},
                                       {"offset", 0},
                                       {"offset_space", "loose_file"},
                                       {"source_stamp", file.stamp.Json()}});
                    if (embedded && file.path.ends_with(".embb"))
                        parent(row, file.file);
                    else
                        emit(row);
                }
            }
            catch (const Monstey_Error &error)
            {
                if (error.code == "catalog_write_failed")
                    throw;
                failures.as_array().push_back(
                    J::object({{"kind", "loose"}, {"source", Monstey_CText(mount.source)}, {"error", error.what()}}));
            }
            catch (const std::exception &error)
            {
                failures.as_array().push_back(
                    J::object({{"kind", "loose"}, {"source", Monstey_CText(mount.source)}, {"error", error.what()}}));
            }
        }
    }
    rawRows.Finish();
    auto verifySources = [&] {
        Monstey_CVerifyConfig(mounts);
        for (const auto &f : fingerprints)
            Monstey_Require(Monstey_CStat(f.path) == f.stamp && Monstey_CIndexHash(f.path, f.bytes) == f.hash,
                            "Archive index changed during catalog generation");
        for (const auto &[path, snapshot] : looseSnapshots)
            Monstey_Require(Monstey_CLooseFiles(path) == snapshot,
                            "Mounted loose metadata changed during catalog generation");
        for (const auto &record : parents)
        {
            Monstey_CMetadata view(record.source, record.archive, record.entry, dll);
            view.Children();
            Monstey_Require(view.Digest() == record.digest, "EMBB metadata changed during catalog generation");
        }
    };
    verifySources();
    Monstey_CStage finalRows(output.parent_path());
    std::uint64_t winningRows = 0;
    Monstey_CLines(rawRows.Path(), [&](J row, std::uint64_t index) {
        Monstey_Require(row.at("catalog_row").as_uint() == index, "Staged catalog sequence differs");
        const bool child = row.at("kind").as_string() == "embedded";
        const auto &key = child ? row.at("bundle").as_string() : row.at("path").as_string();
        const auto winner = winners.find(key);
        Monstey_Require(winner != winners.end(), "Catalog winner parent is absent");
        const auto expected = child ? row.at("parent_catalog_row").as_uint() : index;
        const bool winning = winner->second.row == expected;
        row["winning"] = winning;
        row["winning_priority"] = winner->second.priority;
        row["winner_scope"] = child ? "native_parent" : "mount_path";
        if (winning)
            ++winningRows;
        finalRows.Add(row.dump());
        finalRows.Add("\n");
    });
    finalRows.Finish();
    const auto digest = Monstey_HashSha256(finalRows.Path());
    const bool complete = failures.empty() && missing.empty();
    J result = J::object({{"schema", "Monstey.NativeArchiveCatalog/1"},
                          {"status", complete ? "SOW_ASSET_CATALOG_COMPLETE" : "SOW_ASSET_CATALOG_PARTIAL"},
                          {"complete", complete},
                          {"output", Monstey_CText(output)},
                          {"summary_path", Monstey_CText(summaryPath)},
                          {"sha256", digest},
                          {"bytes", std::filesystem::file_size(finalRows.Path())},
                          {"rows", rowCount},
                          {"winning_rows", winningRows},
                          {"counts", J::object({{"direct", directCount},
                                                {"loose", looseCount},
                                                {"embedded", embeddedCount},
                                                {"parents_checked", parents.size()}})},
                          {"game_root", Monstey_CText(mounts.root)},
                          {"config", J::object({{"path", Monstey_CText(mounts.config)},
                                                {"sha256", mounts.configHash},
                                                {"stamp", mounts.configStamp.Json()}})},
                          {"sources", sources},
                          {"failed", failures},
                          {"failed_count", failures.size()},
                          {"missing_mounts", missing},
                          {"include_embedded", embedded},
                          {"embedded_depth", embedded ? 1 : 0},
                          {"archive_filter", filter},
                          {"fingerprint_policy", "Source identity/size/mtime plus archive index SHA256 and replayed "
                                                 "EMBB metadata reads; loose metadata is re-enumerated"},
                          {"payload_hashes_computed", false},
                          {"payloads_copied", false},
                          {"mount_order_resolved", true},
                          {"variant_policy", "All variants retained. Direct paths use last mounted source; embedded "
                                             "children resolve only within their native parent"},
                          {"implementation", "C++20"},
                          {"python_runtime", false},
                          {"game_written", false},
                          {"runtime_verified", false}});
    bool published = false;
    try
    {
        verifySources();
        finalRows.Publish(output);
        published = true;
        Monstey_Require(Monstey_HashSha256(output) == digest, "Published catalog hash differs");
        Monstey_WriteNewText(summaryPath, result.dump(2) + "\n");
    }
    catch (...)
    {
        if (published && std::filesystem::is_regular_file(output) && Monstey_HashSha256(output) == digest)
        {
            std::error_code error;
            std::filesystem::remove(output, error);
        }
        throw;
    }
    return result;
}

J Monstey_ReadArchiveCatalog(const std::filesystem::path &source, const J &options)
{
    Monstey_CRequireKeys(options, {"query", "path", "bundle", "kind", "winning_only", "offset", "limit",
                                   "expected_sha256", "verify_mounts"});
    const auto catalog = Monstey_CPath(source);
    const auto stamp = Monstey_CStat(catalog);
    const auto digest = Monstey_HashSha256(catalog);
    const auto expected = Monstey_CLower(Monstey_CArg(options, "expected_sha256"));
    Monstey_Require(expected.empty() || expected == digest, "Catalog SHA256 precondition differs");
    const auto summaryPath = Monstey_CSummary(catalog);
    J summary = nullptr;
    std::optional<Monstey_CStamp> summaryStamp;
    std::string summaryHash;
    if (std::filesystem::is_regular_file(summaryPath))
    {
        summaryStamp = Monstey_CStat(summaryPath);
        const auto bytes = Monstey_ReadFile(summaryPath, 32ull * 1024 * 1024);
        summaryHash = Monstey_HashSha256(bytes);
        summary = J::Parse(std::string(bytes.begin(), bytes.end()));
        Monstey_Require(summary.is_object() && summary.contains("sha256") && summary.at("sha256").as_string() == digest,
                        "Catalog summary SHA256 does not match its JSONL source");
    }
    const bool verifyMounts = Monstey_CBool(options, "verify_mounts", false),
               winningOnly = Monstey_CBool(options, "winning_only", false);
    const auto query = Monstey_CLower(Monstey_CArg(options, "query")), exactText = Monstey_CArg(options, "path"),
               bundleText = Monstey_CArg(options, "bundle"), kindFilter = Monstey_CArg(options, "kind");
    Monstey_Require(query.size() <= 512, "Catalog query exceeds 512 bytes");
    const auto exact = exactText.empty() ? std::string{} : Monstey_ArchiveVirtual(exactText),
               bundle = bundleText.empty() ? std::string{} : Monstey_ArchiveVirtual(bundleText);
    const auto offset = options.value("offset", 0).as_uint(), limit = options.value("limit", 250).as_uint();
    Monstey_Require(limit >= 1 && limit <= 500, "Catalog page size must be 1..500");
    if (!kindFilter.empty())
        Monstey_Require(kindFilter == "archive" || kindFilter == "loose" || kindFilter == "embedded",
                        "Unknown catalog resource kind");
    if (verifyMounts)
    {
        Monstey_Require(summary.is_object() && summary.contains("config") && summary.contains("sources"),
                        "Native mount verification requires the source-qualified summary");
        const auto &config = summary.at("config");
        const auto path = std::filesystem::path(Monstey_Wide(config.at("path").as_string()));
        Monstey_Require(Monstey_HashSha256(path) == config.at("sha256").as_string(),
                        "Archive config changed since catalog generation");
        for (const auto &native : summary.at("sources").as_array())
        {
            const auto p = std::filesystem::path(Monstey_Wide(native.at("path").as_string()));
            if (native.at("kind").as_string() == "archive")
                Monstey_Require(Monstey_CStat(p).Json() == native.at("stamp") &&
                                    Monstey_CIndexHash(p, native.at("index_bytes").as_uint()) ==
                                        native.at("index_sha256").as_string(),
                                "Archive source changed since catalog generation");
            else
                Monstey_Require(Monstey_CLooseFiles(p).size() == native.at("files").as_uint(),
                                "Loose mount file set changed since catalog generation");
        }
        if (summary.contains("missing_mounts"))
            for (const auto &missing : summary.at("missing_mounts").as_array())
                Monstey_Require(
                    !std::filesystem::exists(std::filesystem::path(Monstey_Wide(missing.at("source").as_string()))),
                    "Previously missing mount now exists; regenerate the catalog");
    }
    std::unordered_map<std::string, Monstey_CWinner> winners;
    std::uint64_t sourceRows = 0;
    Monstey_CLines(catalog, [&](const J &row, std::uint64_t index) {
        const auto path = Monstey_CVirtualRow(row);
        Monstey_Require(row.contains("priority") && row.at("priority").is_integer(),
                        "Catalog variant needs a native mount priority");
        const auto priority = row.at("priority").as_uint();
        const auto kind = row.at("kind").as_string();
        Monstey_Require(kind == "archive" || kind == "loose" || kind == "embedded", "Unknown catalog variant kind");
        if (row.contains("catalog_row"))
            Monstey_Require(row.at("catalog_row").as_uint() == index,
                            "Catalog sequence differs from physical JSONL order");
        if (kind != "embedded")
        {
            const auto found = winners.find(path);
            Monstey_Require(found == winners.end() || found->second.priority != priority,
                            "Duplicate direct resource at one mount priority");
            const auto physical = kind == "archive" ? row.at("archive").as_string() : row.at("file").as_string();
            if (found == winners.end() || found->second.priority < priority)
                winners[path] = {index, priority, physical, kind};
            if (verifyMounts && kind == "loose")
                Monstey_Require(row.contains("source_stamp") &&
                                    Monstey_CStat(std::filesystem::path(Monstey_Wide(physical))).Json() ==
                                        row.at("source_stamp"),
                                "Loose resource changed since catalog generation");
        }
        ++sourceRows;
    });
    if (summary.is_object() && summary.contains("rows"))
        Monstey_Require(summary.at("rows").as_uint() == sourceRows, "Catalog summary row count differs");
    J rows = J::array();
    std::uint64_t total = 0;
    Monstey_CLines(catalog, [&](J row, std::uint64_t index) {
        const auto path = Monstey_CVirtualRow(row);
        const bool child = row.at("kind").as_string() == "embedded";
        const auto parent = child ? Monstey_ArchiveVirtual(row.at("bundle").as_string()) : std::string{};
        const auto found = winners.find(child ? parent : path);
        Monstey_Require(found != winners.end(), "Embedded catalog parent is absent");
        bool winning = false;
        if (!child)
            winning = found->second.row == index;
        else if (row.contains("parent_catalog_row"))
        {
            Monstey_Require(row.at("parent_catalog_row").as_uint() < index,
                            "Embedded parent must precede its child row");
            winning = found->second.row == row.at("parent_catalog_row").as_uint();
        }
        else
        {
            const auto physical = row.at("parent_kind").as_string() == "archive" ? row.at("archive").as_string()
                                                                                 : row.at("parent_file").as_string();
            winning = found->second.priority == row.at("priority").as_uint() && found->second.source == physical;
        }
        if (row.contains("winning"))
            Monstey_Require(row.at("winning").is_bool() && row.at("winning").as_bool() == winning,
                            "Stored catalog winning order differs from native priorities");
        if (winningOnly && !winning)
            return;
        if (!exact.empty() && path != exact)
            return;
        if (!bundle.empty() && (!child || parent != bundle))
            return;
        if (!kindFilter.empty() && row.at("kind").as_string() != kindFilter)
            return;
        if (!query.empty() && Monstey_CLower(path + " " + parent).find(query) == std::string::npos)
            return;
        const auto current = total++;
        if (current < offset || rows.size() >= limit)
            return;
        row["path_hash"] = Monstey_PathHash(path);
        row["winning"] = winning;
        row["winner_scope"] = child ? "native_parent" : "mount_path";
        rows.as_array().push_back(std::move(row));
    });
    Monstey_Require(Monstey_CStat(catalog) == stamp && Monstey_HashSha256(catalog) == digest,
                    "Catalog changed during query");
    if (summaryStamp)
        Monstey_Require(Monstey_CStat(summaryPath) == *summaryStamp && Monstey_HashSha256(summaryPath) == summaryHash,
                        "Catalog summary changed during query");
    return J::object({{"source", Monstey_CText(catalog)},
                      {"sha256", digest},
                      {"summary_path", summaryStamp ? J(Monstey_CText(summaryPath)) : J()},
                      {"summary_verified", summaryStamp.has_value()},
                      {"native_inputs_revalidated", verifyMounts},
                      {"native_verification_scope", "Config bytes, archive index bytes and loose file "
                                                    "identities/size/mtime; embedded payloads are not re-decoded"},
                      {"complete", summary.is_object() ? summary.value("complete", nullptr) : J()},
                      {"rows", rows},
                      {"total", total},
                      {"source_rows", sourceRows},
                      {"offset", offset},
                      {"limit", limit},
                      {"winning_only", winningOnly},
                      {"implementation", "C++20"},
                      {"python_runtime", false},
                      {"runtime_verified", false}});
}
J Monstey_InspectEmbeddedResource(const std::filesystem::path &input,
    std::string_view parentBundle,const std::filesystem::path &oodleDll) {
    const auto source=Monstey_CPath(input);
    const auto stamp=Monstey_CStat(source);
    const auto bundle=parentBundle.empty()?std::string{}:Monstey_ArchiveVirtual(std::string(parentBundle));
    std::shared_ptr<const Monstey_Archive> archive;
    std::optional<Monstey_ArchiveEntry> entry;
    std::string indexHash;
    if(!bundle.empty()) {
        archive=std::make_shared<Monstey_Archive>(source);
        const auto *found=archive->Find(bundle);
        Monstey_Require(found!=nullptr,"Native EMBB parent not found");
        entry=*found;indexHash=Monstey_CIndexHash(source,archive->IndexLength());
    }
    Monstey_CMetadata view(source,archive,entry,oodleDll);
    const auto [version,children]=view.Children();
    const auto metadataHash=view.Digest();
    auto rows=J::array();
    for(const auto &child:children) {
        const auto hash=Monstey_HashSha256Chunks(child.size,[&](std::uint64_t at,std::uint64_t count) {
            return view.Payload(child.offset+at,count);
        });
        rows.as_array().push_back(J::object({{"path",child.path},{"bytes",child.size},
            {"offset",child.offset},{"sha256",hash}}));
    }
    const auto parentHash=Monstey_HashSha256Chunks(view.Stats().at("parent_bytes").as_uint(),
        [&](std::uint64_t at,std::uint64_t count){return view.Payload(at,count);});
    Monstey_CMetadata replay(source,archive,entry,oodleDll);replay.Children();
    Monstey_Require(replay.Digest()==metadataHash&&Monstey_CStat(source)==stamp,
                    "Native EMBB source changed during inspection");
    if(archive) {
        archive->CheckUnchanged();
        Monstey_Require(Monstey_CIndexHash(source,archive->IndexLength())==indexHash,
                        "Native archive index changed during EMBB inspection");
    }
    return J::object({{"format","EMBB"},{"version",version},{"resources",rows},
        {"bytes",view.Stats().at("parent_bytes")},{"sha256",parentHash},{"source",Monstey_CText(source)},
        {"embedded_metadata_sha256",metadataHash},{"metadata_ranges",view.Stats()},
        {"parent_materialized",false},{"source_unchanged",true},{"runtime_verified",false}});
}
J Monstey_ExtractEmbeddedResource(const std::filesystem::path &input, std::string_view parentBundle,
                                  std::string_view childResource, const std::filesystem::path &destination,
                                  const std::filesystem::path &oodleDll)
{
    const auto source = Monstey_CPath(input), output = Monstey_CPath(destination);
    std::array<std::filesystem::path, 1> inputs{source};
    Monstey_RequireOfflineOutput(output, inputs);
    Monstey_Require(!std::filesystem::exists(output), "Embedded extraction output already exists");
    const auto stamp = Monstey_CStat(source);
    const auto bundle = parentBundle.empty() ? std::string{} : Monstey_ArchiveVirtual(std::string(parentBundle));
    const auto resource = Monstey_ArchiveVirtual(std::string(childResource));
    (void)Monstey_PathHash(resource);
    std::shared_ptr<const Monstey_Archive> archive;
    std::optional<Monstey_ArchiveEntry> entry;
    std::string indexHash;
    if (!bundle.empty())
    {
        archive = std::make_shared<Monstey_Archive>(source);
        const auto *found = archive->Find(bundle);
        Monstey_Require(found != nullptr, "Native EMBB parent not found in archive");
        entry = *found;
        indexHash = Monstey_CIndexHash(source, archive->IndexLength());
    }
    Monstey_CMetadata view(source, archive, entry, oodleDll);
    const auto [version, children] = view.Children();
    const auto found =
        std::find_if(children.begin(), children.end(), [&](const auto &child) { return child.path == resource; });
    Monstey_Require(found != children.end(), "Requested child is absent from its native EMBB parent");
    const auto metadataHash = view.Digest();
    const auto metadataStats = view.Stats();
    std::filesystem::create_directories(Monstey_WindowsFilePath(output.parent_path()));
    Monstey_CStage stage(output.parent_path());
    std::uint64_t cursor = 0;
    while (cursor < found->size)
    {
        const auto count = std::min<std::uint64_t>(1024 * 1024, found->size - cursor);
        const auto bytes = view.Payload(found->offset + cursor, count);
        stage.Add(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
        cursor += count;
    }
    stage.Finish();
    Monstey_Require(Monstey_CStat(source) == stamp, "Native embedded source changed during child extraction");
    if (archive)
        Monstey_Require(Monstey_CIndexHash(source, archive->IndexLength()) == indexHash,
                        "Native archive index changed during extraction");
    Monstey_CMetadata verify(source, archive, entry, oodleDll);
    verify.Children();
    Monstey_Require(verify.Digest() == metadataHash, "Native EMBB metadata changed during child extraction");
    const auto digest = Monstey_HashSha256(stage.Path());
    bool published = false;
    try
    {
        stage.Publish(output);
        published = true;
        Monstey_Require(std::filesystem::file_size(Monstey_WindowsFilePath(output)) == found->size && Monstey_HashSha256(output) == digest,
                        "Extracted child readback differs");
        Monstey_Require(Monstey_CStat(source) == stamp, "Native source changed at child publication");
    }
    catch (...)
    {
        if (published && std::filesystem::is_regular_file(output) && Monstey_HashSha256(output) == digest)
        {
            std::error_code error;
            std::filesystem::remove(output, error);
        }
        throw;
    }
    return J::object(
        {{"source", Monstey_CText(source)},
         {"source_stamp", stamp.Json()},
         {"archive_index_sha256", indexHash.empty() ? J() : J(indexHash)},
         {"bundle", bundle.empty() ? J() : J(bundle)},
         {"resource", resource},
         {"path_hash", Monstey_PathHash(resource)},
         {"embedded_version", version},
         {"embedded_metadata_sha256", metadataHash},
         {"metadata_ranges", metadataStats},
         {"child_offset", found->offset},
         {"bytes", found->size},
         {"output", Monstey_CText(output)},
         {"sha256", digest},
         {"parent_copied", false},
         {"source_verification_scope",
          "File identity/size/mtime, archive index and EMBB metadata; the full parent payload is not hashed"},
         {"source_unchanged", true},
         {"implementation", "C++20"},
         {"python_runtime", false},
         {"game_written", false},
         {"runtime_verified", false}});
}
J Monstey_ReadEmbeddedResourceRange(const std::filesystem::path &input, std::string_view parentBundle,
                                    std::string_view childResource, std::uint64_t offset, std::uint64_t count,
                                    const std::filesystem::path &oodleDll)
{
    Monstey_Require(count <= 1024 * 1024, "Embedded inspection range exceeds 1 MiB");
    const auto source = Monstey_CPath(input);
    const auto stamp = Monstey_CStat(source);
    const auto bundle = parentBundle.empty() ? std::string{} : Monstey_ArchiveVirtual(std::string(parentBundle));
    const auto resource = Monstey_ArchiveVirtual(std::string(childResource));
    std::shared_ptr<const Monstey_Archive> archive;
    std::optional<Monstey_ArchiveEntry> entry;
    std::string indexHash;
    if (!bundle.empty())
    {
        archive = std::make_shared<Monstey_Archive>(source);
        const auto *found = archive->Find(bundle);
        Monstey_Require(found != nullptr, "Native EMBB parent absent from archive");
        entry = *found;
        indexHash = Monstey_CIndexHash(source, archive->IndexLength());
    }
    Monstey_CMetadata view(source, archive, entry, oodleDll);
    const auto [version, children] = view.Children();
    const auto child =
        std::find_if(children.begin(), children.end(), [&](const auto &value) { return value.path == resource; });
    Monstey_Require(child != children.end(), "Requested child absent from native EMBB parent");
    Monstey_Require(offset <= child->size && count <= child->size - offset, "Inspection range exceeds selected child");
    const auto metadataHash = view.Digest();
    const auto bytes = view.Payload(child->offset + offset, count);
    Monstey_Require(Monstey_CStat(source) == stamp, "Native source changed during child inspection");
    if (archive)
        Monstey_Require(Monstey_CIndexHash(source, archive->IndexLength()) == indexHash,
                        "Native archive index changed during inspection");
    Monstey_CMetadata replay(source, archive, entry, oodleDll);
    replay.Children();
    Monstey_Require(replay.Digest() == metadataHash, "Native embedded metadata changed during inspection");
    constexpr char hex[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(bytes.size() * 2);
    for (auto byte : bytes)
    {
        encoded.push_back(hex[byte >> 4]);
        encoded.push_back(hex[byte & 15]);
    }
    J words = J::array();
    for (std::size_t i = 0; i + 4 <= bytes.size(); i += 4)
        words.as_array().push_back(Monstey_U32(bytes, i));
    return J::object({{"source", Monstey_CText(source)},
                      {"source_stamp", stamp.Json()},
                      {"archive_index_sha256", indexHash.empty() ? J() : J(indexHash)},
                      {"bundle", bundle.empty() ? J() : J(bundle)},
                      {"resource", resource},
                      {"resource_size", child->size},
                      {"range_offset", offset},
                      {"bytes", count},
                      {"bytes_hex", encoded},
                      {"u32_little_endian", words},
                      {"range_sha256", Monstey_HashSha256(bytes)},
                      {"embedded_version", version},
                      {"embedded_metadata_sha256", metadataHash},
                      {"metadata_ranges", view.Stats()},
                      {"parent_copied", false},
                      {"source_unchanged", true},
                      {"source_verification_scope",
                       "File identity/size/mtime, archive index and EMBB metadata; parent payload is not fully hashed"},
                      {"python_runtime", false},
                      {"game_written", false}});
}
} // namespace Monstey
