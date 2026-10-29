#include "Monstey_Runtime.hpp"
#include "Monstey_RuntimeArguments.hpp"
#include "Monstey_FxAttachment.hpp"
#include "Monstey_FxLibrary.hpp"
#include "Monstey_FxNested.hpp"
#include "Monstey_FxReflection.hpp"
#include "Monstey_FxWorkflow.hpp"
#include "Monstey_RuntimeNativeDeclarations.hpp"
#include <charconv>

namespace Monstey
{
using namespace Monstey_RuntimeDetail;

namespace
{
std::filesystem::path Monstey_DefaultEffectSchema(const std::filesystem::path &schema)
{
    if (schema.empty())
        return {};
    const auto candidate = schema.parent_path() / L"fx-schema.json";
    return std::filesystem::exists(Monstey_WindowsFilePath(candidate)) ? candidate : schema;
}
} // namespace

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryEffectTools(std::string_view op, const Monstey_Json &a)
{
    if (op == "fx_verify_bindings")
        return Monstey_FxVerifyNativeBindings(Effects());
    if (op.starts_with("fx_attachment_"))
    {
        auto values = a;
        values.as_object().erase("expected_generation");
        values.as_object().erase("expected_revision");
        return Monstey_FxAttachmentCall(Effects(), op, values);
    }
    if (op.starts_with("fx_nested_"))
    {
        auto values = a;
        values.as_object().erase("expected_generation");
        values.as_object().erase("expected_revision");
        return Monstey_FxNestedCall(Effects(), op, values);
    }
    if (op == "fx_library" || op.starts_with("fx_library_") || op.starts_with("fx_prefab_"))
    {
        auto values = a;
        values.as_object().erase("expected_generation");
        values.as_object().erase("expected_revision");
        return Monstey_FxLibraryCall(Effects(), op, values);
    }
    return std::nullopt;
}

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryEffects(std::string_view op, const Monstey_Json &a,
                                                                const Monstey_Arguments &arguments)
{
    using J = Monstey_Json;
    const auto offset = arguments.offset;
    const auto limit = arguments.limit;
    const auto &query = arguments.query;
    if (op == "fx_reflect_schema")
    {
        const auto source = Monstey_Path(a, "source");
        const auto image = Monstey_ReadFile(source);
        const auto &rows = a.at("category_entries").as_array();
        if (rows.empty() || rows.size() > 4096)
            throw Monstey_Error("invalid_arguments", "Select one to4096 native class addresses");
        std::vector<std::uint64_t> entries;
        entries.reserve(rows.size());
        for (const auto &row : rows)
        {
            if (row.is_integer())
            {
                entries.push_back(row.as_uint());
                continue;
            }
            if (!row.is_string())
                throw Monstey_Error("invalid_argument_type",
                                    "Native class addresses require integers or hexadecimal strings");
            std::string_view text = row.as_string();
            if (text.starts_with("0x") || text.starts_with("0X"))
                text.remove_prefix(2);
            std::uint64_t value{};
            const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);
            if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
                throw Monstey_Error("invalid_arguments", "Native class address is not hexadecimal");
            entries.push_back(value);
        }
        auto result = Monstey_FxReflectCategories(image, entries, Monstey_Text(a, "expected_sha256"));
        if (Monstey_HashSha256(source) != Monstey_HashSha256(image))
            throw Monstey_Error("source_hash_mismatch", "Native reflection source changed during inspection");
        result["source"] = Monstey_Utf8(source.wstring());
        result["source_unchanged"] = true;
        return result;
    }
    if (op == "fx_new")
    {
        const auto schema = Monstey_Path(
            a, "schema_path",
            Monstey_Path(a, "schema", Monstey_Path(a, "field_names", Monstey_DefaultEffectSchema(schema_))));
        const auto source = Monstey_Path(a, "template");
        if (source.empty() && a.contains("expected_sha256"))
            throw Monstey_Error("invalid_arguments", "expected_sha256 requires an explicit effect template");
        const auto prefix = source.empty() ? Monstey_FxDefaultPrefix()
                                           : Monstey_FxQualifiedPrefix(source, Monstey_Text(a, "expected_sha256"));
        auto candidate = Monstey_Gadb::NewFx(prefix, schema);
        if (a.contains("name"))
        {
            auto creation = J::object({{"name", a.at("name")}});
            for (const char *field : {"duration_ms", "values"})
                if (a.contains(field))
                    creation[field] = a.at(field);
            Monstey_FxWorkflowCall(*candidate, "fx_create_effect", creation);
        }
        else if (a.contains("duration_ms") || a.contains("values"))
            throw Monstey_Error("invalid_arguments", "Initial effect properties require a resource name");
        if (!schema.empty())
            settings_->Protect(schema);
        if (!source.empty())
            settings_->Protect(source);
        fx_ = std::move(candidate);
        ++generation_;
        return Monstey_FxWorkflowCall(*fx_, "fx_workflow");
    }
    if (op == "fx_open" || op == "fx_load_project")
    {
        const auto schema =
            Monstey_Path(a, "schema", Monstey_Path(a, "field_names", Monstey_DefaultEffectSchema(schema_)));
        auto candidate = op == "fx_open" ? std::make_unique<Monstey_Gadb>(Monstey_Path(a, "path"), schema)
                                         : Monstey_Gadb::LoadProject(Monstey_Path(a, "path"));
        if (candidate->Describe().at("format").as_string() != "LTFX8/GADB17")
            throw Monstey_Error("wrong_format", "Expected LTFX8");
        if (!Monstey_Text(candidate->Describe(), "source").empty())
            settings_->Protect(Monstey_Path(candidate->Describe(), "source"));
        if (const auto meta = candidate->Describe(); meta.contains("schema_path") &&
                                                     meta.at("schema_path").is_string() &&
                                                     !meta.at("schema_path").as_string().empty())
            settings_->Protect(Monstey_Path(meta, "schema_path"));
        fx_ = std::move(candidate);
        ++generation_;
        return fx_->Describe();
    }
    if (op == "fx_set" || op == "fx_delete_element" || op == "fx_delete_effect" || op == "fx_build" ||
        op == "fx_save_project")
    {
        if (op == "fx_build")
        {
            (void)Monstey_Path(a, "output");
            return Monstey_BuildWithNativeDeclarations(Effects(), a, *settings_, true);
        }
        else if (op == "fx_save_project")
            (void)Monstey_Path(a, "path");
        auto values = a;
        values.as_object().erase("expected_generation");
        values.as_object().erase("expected_revision");
        return Monstey_FxLibraryNativeCall(Effects(), op, values);
    }
    if (op == "fx_records")
        return Effects().FxRecords(query, offset, limit);
    if (op == "fx_record")
        return Effects().Metadata(Monstey_Text(a, "record"));
    if (op == "fx_dependencies")
        return Effects().Dependencies();
    if (op == "fx_undo")
        return Effects().Undo();
    if (op == "fx_redo")
        return Effects().Redo();
    if (op == "fx_revert")
        return Effects().Revert();
    if (op == "fx_changes")
        return Effects().Changes();
    if (op.starts_with("fx_"))
        return Monstey_FxWorkflowCall(Effects(), op, a);
    return std::nullopt;
}
} // namespace Monstey
