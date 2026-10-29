#include "Monstey_Runtime.hpp"
#include "Monstey_RuntimeArguments.hpp"
#include <array>

namespace Monstey
{
using namespace Monstey_RuntimeDetail;

namespace
{
std::string Monstey_Lower(std::string value)
{
    for (char &c : value)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return value;
}
} // namespace

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryArchiveCatalog(std::string_view op, const Monstey_Json &a)
{
    const auto gameRoot = [&]
    {
        auto path = Monstey_Path(a, "game_root", Monstey_Path(settings_->Describe(), "game_root"));
        if (path.empty())
            throw Monstey_Error("game_not_configured", "Select the local game folder first");
        return path;
    };
    if (op == "archive_mounts")
        return Monstey_ArchiveMounts(gameRoot());
    if (op == "archive_catalog")
    {
        const auto output = Monstey_Path(a, "output");
        const auto report = Monstey_ArchiveCatalog(
            gameRoot(), output, Monstey_SelectArguments(a, {"include_embedded", "oodle_dll", "archive_filter"}));
        settings_->Protect(output);
        return report;
    }
    if (op == "archive_catalog_read")
    {
        const auto path = Monstey_Path(a, "path");
        const auto report = Monstey_ReadArchiveCatalog(
            path, Monstey_SelectArguments(a, {"query", "bundle", "kind", "winning_only", "offset", "limit",
                                              "expected_sha256", "verify_mounts"}));
        settings_->Protect(path);
        return report;
    }
    if (op == "archive_extract_embedded")
    {
        const auto parent = Monstey_Path(a, "source");
        settings_->Protect(parent);
        return Monstey_ExtractEmbeddedResource(parent, Monstey_Text(a, "bundle"), Monstey_Text(a, "virtual"),
                                               Monstey_Path(a, "destination"), Monstey_Path(a, "oodle_dll"));
    }
    return std::nullopt;
}

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryArchiveProject(std::string_view op, const Monstey_Json &a,
                                                                       const Monstey_Arguments &arguments)
{
    using J = Monstey_Json;
    const auto offset = arguments.offset;
    const auto limit = arguments.limit;
    const auto &query = arguments.query;
    if (op == "open_archive" || op == "archive_project_new" || op == "archive_project_fromopened" ||
        op == "archive_load_project")
    {
        std::unique_ptr<Monstey_ArchiveProject> candidate;
        if (op == "open_archive")
            candidate = std::make_unique<Monstey_ArchiveProject>(Monstey_Path(a, "path"));
        else if (op == "archive_load_project")
            candidate = std::make_unique<Monstey_ArchiveProject>(Monstey_ArchiveProject::Load(Monstey_Path(a, "path")));
        else if (op == "archive_project_fromopened")
        {
            const auto source = Archive().Source();
            if (!source)
                throw Monstey_Error("archive_has_no_source", "The empty archive project has no original source");
            candidate = std::make_unique<Monstey_ArchiveProject>(*source);
        }
        else
            candidate = std::make_unique<Monstey_ArchiveProject>();
        if (const auto source = candidate->Source())
            settings_->Protect(*source);
        archive_ = std::move(candidate);
        ++generation_;
        return ArchiveDescribe();
    }
    if (op == "archive_entries")
    {
        auto rows = J::array();
        std::size_t total = 0;
        const auto filter = Monstey_Lower(query);
        for (const auto &entry : Archive().Entries())
        {
            if (Monstey_Lower(entry.path).find(filter) == std::string::npos)
                continue;
            if (total >= offset && rows.size() < limit)
                rows.as_array().push_back(J::object({{"path", entry.path},
                                                     {"size", entry.size},
                                                     {"stored_size", entry.stored_size},
                                                     {"offset", entry.offset},
                                                     {"flags", entry.flags}}));
            ++total;
        }
        auto folders = J::array();
        for (const auto &folder : Archive().Folders())
            folders.as_array().push_back(folder);
        return J::object({{"rows", rows},
                          {"total", static_cast<std::uint64_t>(total)},
                          {"offset", static_cast<std::uint64_t>(offset)},
                          {"limit", static_cast<std::uint64_t>(limit)},
                          {"folders", folders}});
    }
    if (op == "extract")
    {
        const auto output = Monstey_Path(a, "destination");
        Archive().Extract(Monstey_Text(a, "virtual"), output, Monstey_Path(a, "oodle_dll"));
        return J::object({{"output", Monstey_Utf8(output.wstring())},
                          {"sha256", Monstey_HashSha256(output)},
                          {"bytes", std::filesystem::file_size(output)}});
    }
    if (op == "archive_add")
        Archive().AddFile(Monstey_Text(a, "virtual"), Monstey_Path(a, "source"));
    else if (op == "archive_add_directory")
        Archive().AddDirectory(Monstey_Text(a, "virtual"), Monstey_Path(a, "source_dir"));
    else if (op == "archive_replace")
        Archive().Replace(Monstey_Text(a, "virtual"), Monstey_Path(a, "source"));
    else if (op == "archive_create_folder")
        Archive().CreateFolder(Monstey_Text(a, "virtual"));
    else if (op == "archive_remove")
        Archive().Remove(Monstey_Text(a, "virtual"));
    else if (op == "archive_rename")
        Archive().Rename(Monstey_Text(a, "old"), Monstey_Text(a, "new"));
    else if (op == "archive_undo")
        (void)Archive().Undo();
    else if (op == "archive_redo")
        (void)Archive().Redo();
    else if (op == "archive_revert")
        Archive().Revert();
    else if (op == "archive_changes")
        return ArchiveDescribe();
    else if (op == "archive_save_project")
    {
        Archive().Save(Monstey_Path(a, "path"));
        return ArchiveDescribe();
    }
    else if (op == "archive_build")
    {
        const auto report = Archive().Build(Monstey_Path(a, "output"), a.value("include_patchlist", true).as_bool(),
                                            Monstey_Path(a, "oodle_dll"));
        return J::object({{"output", Monstey_Utf8(report.output.wstring())},
                          {"bytes", report.bytes},
                          {"sha256", report.sha256},
                          {"files", static_cast<std::uint64_t>(report.files)},
                          {"folders", static_cast<std::uint64_t>(report.folders)},
                          {"readback_verified", report.readback_verified}});
    }
    if (op.starts_with("archive_"))
        return ArchiveDescribe();
    return std::nullopt;
}
} // namespace Monstey
