#include "Monstey_Runtime.hpp"
#include "Monstey_RuntimeArguments.hpp"
#include "Monstey_GadbClassCache.hpp"
#include "Monstey_GadbBanks.hpp"
#include "Monstey_GadbClassDeclarationApi.hpp"
#include "Monstey_RuntimeNativeDeclarations.hpp"
#include <limits>

namespace Monstey
{
using namespace Monstey_RuntimeDetail;

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryDatabaseTrace(std::string_view op, const Monstey_Json &a)
{
    if (op == "database_cache_trace")
        return Monstey_GadbClassCacheTrace(a.at("input"),
                                           static_cast<std::size_t>(a.value("step_limit", 100000).as_uint()));
    return std::nullopt;
}

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryDatabase(std::string_view op, const Monstey_Json &a,
                                                                 const Monstey_Arguments &arguments)
{
    using J = Monstey_Json;
    const auto offset = arguments.offset;
    const auto limit = arguments.limit;
    const auto &query = arguments.query;
    if (op == "database_new")
    {
        auto candidate = Monstey_Gadb::New(Monstey_Path(a, "schema_path", schema_));
        const auto metadata = candidate->Describe();
        const auto schemaPath = Monstey_Text(metadata, "schema_path");
        if (!schemaPath.empty())
            settings_->Protect(std::filesystem::path(Monstey_Wide(schemaPath)));
        database_ = std::move(candidate);
        ++generation_;
        return database_->Describe();
    }
    if (op == "database_create_category")
        return Database().CreateCategory(Monstey_Text(a, "schema_name"), Monstey_Text(a, "name"),
                                         Monstey_Text(a, "path"));
    if (op == "database_create_record")
    {
        const auto category = a.at("category_index").as_uint();
        if (category > std::numeric_limits<std::uint32_t>::max())
            throw Monstey_Error("category_out_of_range", "Category exceeds uint32");
        return Database().CreateRecord(static_cast<std::uint32_t>(category), Monstey_Text(a, "name"),
                                       a.value("anonymous", false).as_bool());
    }
    if (op == "database_add_field")
        return Database().AddField(Monstey_Text(a, "record"), Monstey_Text(a, "field"),
                                   static_cast<std::uint8_t>(a.at("native_type").as_uint()), a.at("value"),
                                   a.value("metadata", J::object()));
    if (op == "database_delete_field")
        return Database().DeleteField(Monstey_Text(a, "record"), Monstey_Text(a, "field"));
    if (op == "database_move_field")
        return Database().MoveField(Monstey_Text(a, "record"), Monstey_Text(a, "field"),
                                    static_cast<std::size_t>(a.at("ordinal").as_uint()));
    if (op == "database_project_fromopened" && Monstey_Text(Database().Describe(), "source").empty() &&
        Monstey_Text(a, "schema_path").empty())
    {
        Database().Revert();
        ++generation_;
        return Database().Describe();
    }
    if (op == "open_database" || op == "database_project_new" || op == "database_project_fromopened" ||
        op == "database_load_project")
    {
        std::unique_ptr<Monstey_Gadb> candidate;
        if (op == "database_load_project")
            candidate = Monstey_Gadb::LoadProject(Monstey_Path(a, "path"));
        else
        {
            std::filesystem::path path;
            if (op == "database_project_fromopened")
            {
                const auto source = Monstey_Text(Database().Describe(), "source");
                if (!source.empty())
                    path = std::filesystem::path(Monstey_Wide(source));
            }
            else
                path = Monstey_Path(a, op == "database_project_new" ? "source" : "path");
            const auto schema = Monstey_Path(a, op == "open_database" ? "schema" : "schema_path", schema_);
            candidate = path.empty() ? Monstey_Gadb::New(schema) : std::make_unique<Monstey_Gadb>(path, schema);
        }
        if (candidate->Describe().at("format").as_string() != "GADB17")
            throw Monstey_Error("wrong_format", "Expected GADB17, not LTFX8");
        if (!Monstey_Text(candidate->Describe(), "source").empty())
            settings_->Protect(Monstey_Path(candidate->Describe(), "source"));
        if (const auto meta = candidate->Describe(); meta.contains("schema_path") &&
                                                     meta.at("schema_path").is_string() &&
                                                     !meta.at("schema_path").as_string().empty())
            settings_->Protect(Monstey_Path(meta, "schema_path"));
        database_ = std::move(candidate);
        ++generation_;
        return database_->Describe();
    }
    if (op == "categories")
        return Database().Categories(query);
    if (op == "records")
    {
        const auto category = a.at("category_index").as_uint();
        if (category > std::numeric_limits<std::uint32_t>::max())
            throw Monstey_Error("category_out_of_range", "Category exceeds uint32");
        return Database().Records(static_cast<std::uint32_t>(category), query, offset, limit);
    }
    if (op == "record")
        return Database().Record(Monstey_Text(a, "reference"));
    if (op == "database_incoming")
        return Database().IncomingReferences(Monstey_Text(a, "record"), offset, limit);
    if (op == "database_metadata")
        return Database().Metadata(Monstey_Text(a, "record"));
    if (op == "database_atom_uses")
        return Database().AtomUses(Monstey_Text(a, "record"), offset, limit);
    if (op == "database_edit")
        return Database().SetField(Monstey_Text(a, "record"), Monstey_Text(a, "field"), a.at("value"));
    if (op == "database_clone")
        return Database().CloneRecord(Monstey_Text(a, "source"), Monstey_Text(a, "new_name"));
    if (op == "database_rename")
        return Database().RenameRecord(Monstey_Text(a, "record"), Monstey_Text(a, "new_name"));
    if (op == "database_delete")
    {
        auto replacement = a.value("replacement", nullptr);
        if (a.value("allow_null", false).as_bool())
        {
            if (!replacement.is_null())
                throw Monstey_Error("conflicting_policy", "Choose replacement or allow_null");
            replacement = "0xffffffff";
        }
        return Database().DeleteRecord(Monstey_Text(a, "record"), replacement);
    }
    if (op == "database_undo")
        return Database().Undo();
    if (op == "database_redo")
        return Database().Redo();
    if (op == "database_revert")
        return Database().Revert();
    if (op == "database_changes")
        return Database().Changes();
    if (op == "database_save_project")
        return Database().SaveProject(Monstey_Path(a, "path"));
    if (op == "database_build")
        return Monstey_BuildWithNativeDeclarations(Database(), a, *settings_, false);
    return std::nullopt;
}

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryNativeDatabase(std::string_view op, const Monstey_Json &a)
{
    if (op == "database_native_banks")
        return Monstey_GadbBankInspect(
            Monstey_SelectArguments(a, {"path", "expected_sha256", "bank_index", "query", "offset", "limit"}));
    if (op == "database_native_class")
        return Monstey_GadbNativeClassInspect(
            Monstey_SelectArguments(a, {"path", "expected_sha256", "class_pointer", "offset", "limit"}));
    return std::nullopt;
}
} // namespace Monstey
