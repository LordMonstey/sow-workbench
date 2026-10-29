#include "Monstey_Runtime.hpp"
#include "Monstey_RuntimeArguments.hpp"
#include "Monstey_World.hpp"

namespace Monstey
{
using namespace Monstey_RuntimeDetail;

Monstey_Gadb &Monstey_Runtime::Database()
{
    if (!database_)
        throw Monstey_Error("database_not_open", "Select a database first");
    return *database_;
}

Monstey_Gadb &Monstey_Runtime::Effects()
{
    if (!fx_)
        throw Monstey_Error("fx_not_open", "Select an effect first");
    return *fx_;
}

Monstey_ArchiveProject &Monstey_Runtime::Archive()
{
    if (!archive_)
        throw Monstey_Error("archive_not_open", "Select an archive first");
    return *archive_;
}

Monstey_StringsProject &Monstey_Runtime::Strings()
{
    if (!strings_)
        throw Monstey_Error("strings_not_open", "Select a string table first");
    return *strings_;
}

Monstey_AnimationProject &Monstey_Runtime::Animation()
{
    if (!animation_)
        throw Monstey_Error("animation_not_open", "Select a native animation library first");
    return *animation_;
}

Monstey_MaterialProject &Monstey_Runtime::Material()
{
    if (!material_)
        throw Monstey_Error("material_not_open", "Select an LTMI material first");
    return *material_;
}

Monstey_Json Monstey_Runtime::ArchiveDescribe() const
{
    if (!archive_)
        return nullptr;
    const auto source = archive_->Source();
    return Monstey_Json::object(
        {{"source", source ? Monstey_Json(Monstey_Utf8(source->wstring())) : Monstey_Json(nullptr)},
         {"files", static_cast<std::uint64_t>(archive_->Entries().size())},
         {"folders", static_cast<std::uint64_t>(archive_->Folders().size())},
         {"dirty", archive_->Dirty()},
         {"can_undo", archive_->CanUndo()},
         {"can_redo", archive_->CanRedo()}});
}

Monstey_Json Monstey_Runtime::Describe() const
{
    auto remaining = Monstey_Json::array();
    const auto world = Monstey_WorldCatalog();
    for (const auto &operation : world.as_array())
        if (!operation.at("implemented").as_bool())
            remaining.as_array().push_back(operation.at("name"));
    return Monstey_Json::object({{"name", "Monstey SoW Workbench"},
                                 {"version", "0.3.0-dev"},
                                 {"implementation", "C++20"},
                                 {"python_runtime", false},
                                 {"runtime_verified", false},
                                 {"configuration", settings_->Describe()},
                                 {"database", database_ ? database_->Describe() : Monstey_Json(nullptr)},
                                 {"archive", ArchiveDescribe()},
                                 {"strings", strings_ ? strings_->Describe() : Monstey_Json(nullptr)},
                                 {"effects", fx_ ? fx_->Describe() : Monstey_Json(nullptr)},
                                 {"animations", animation_ ? animation_->Describe() : Monstey_Json(nullptr)},
                                 {"materials", material_ ? material_->Describe() : Monstey_Json(nullptr)},
                                 {"generation", generation_},
                                 {"revision", revision_},
                                 {"migration_incomplete", remaining}});
}

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TrySession(std::string_view op)
{
    if (op == "describe")
        return Describe();
    if (op == "catalog")
        return Catalog();
    return std::nullopt;
}
} // namespace Monstey
