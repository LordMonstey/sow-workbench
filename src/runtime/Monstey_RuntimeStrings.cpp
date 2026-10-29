#include "Monstey_Runtime.hpp"
#include "Monstey_RuntimeArguments.hpp"

namespace Monstey
{
using namespace Monstey_RuntimeDetail;

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryStrings(std::string_view op, const Monstey_Json &a,
                                                                const Monstey_Arguments &arguments)
{
    const auto offset = arguments.offset;
    const auto limit = arguments.limit;
    const auto &query = arguments.query;
    if (op == "strings_new" || op == "strings_load_project")
    {
        auto candidate = op == "strings_new" ? std::make_unique<Monstey_StringsProject>(Monstey_Path(a, "source"))
                                             : Monstey_StringsProject::LoadProject(Monstey_Path(a, "path"));
        if (!Monstey_Text(candidate->Describe(), "source").empty())
            settings_->Protect(Monstey_Path(candidate->Describe(), "source"));
        strings_ = std::move(candidate);
        ++generation_;
        return strings_->Describe();
    }
    if (op == "strings_entries")
        return Strings().Entries(query, offset, limit);
    if (op == "strings_set")
        return Strings().Set(a.at("key"), Monstey_Text(a, "text"));
    if (op == "strings_remove")
        return Strings().Remove(a.at("key"));
    if (op == "strings_rename")
        return Strings().Rename(a.at("old"), a.at("new"));
    if (op == "strings_undo")
        return Strings().Undo();
    if (op == "strings_redo")
        return Strings().Redo();
    if (op == "strings_revert")
        return Strings().Revert();
    if (op == "strings_changes")
        return Strings().Changes();
    if (op == "strings_save_project")
        return Strings().SaveProject(Monstey_Path(a, "path"));
    if (op == "strings_build")
        return Strings().Build(Monstey_Path(a, "output"));
    return std::nullopt;
}
} // namespace Monstey
