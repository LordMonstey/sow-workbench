#include "Monstey_Runtime.hpp"
#include "Monstey_RuntimeRegistry.hpp"
#include "Monstey_RuntimeOperations.hpp"
#include "Monstey_World.hpp"
#include "Monstey_ModelSockets.hpp"
#include "Monstey_ModelEnvelope.hpp"
#include "Monstey_ModelPreviewApi.hpp"
#include "Monstey_AnimationPreviewApi.hpp"
#include "Monstey_FxNested.hpp"
#include "Monstey_AnimatedExtras.hpp"
#include "Monstey_FxAttachment.hpp"
#include "Monstey_TextureApi.hpp"
#include <array>

namespace Monstey
{
namespace Monstey_RuntimeDetail
{
Monstey_Json Monstey_ArgSchema(std::string_view name)
{
    using J = Monstey_Json;
    if (name == "include_embedded" || name == "winning_only" || name == "verify_mounts" || name == "save" ||
        name == "anonymous" || name == "recursive" || name == "keep_overrides")
        return J::object({{"type", "boolean"}});
    if (name == "expected_native_declarations_sha256")
        return J::object({{"type", "string"}, {"minLength", 64}, {"maxLength", 64}, {"pattern", "^[0-9a-f]{64}$"}});
    if (name == "inspection")
        return J::object(
            {{"type", "object"},
             {"additionalProperties", false},
             {"properties",
              J::object({{"table_offset", J::object({{"type", "integer"}, {"minimum", 0}})},
                         {"table_limit", J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", 4096}})},
                         {"row_offset", J::object({{"type", "integer"}, {"minimum", 0}})},
                         {"row_limit", J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", 10000}})}})}});
    if (name == "category_entries")
        return J::object({{"type", "array"},
                          {"minItems", 1},
                          {"maxItems", 4096},
                          {"items", J::object({{"type", J::array({"string", "integer"})}})}});
    if (name == "group" || name == "parent" || name == "prefab")
        return J::object({{"type", "string"}});
    if (name == "bundle")
        return J::object({{"type", "string"}});
    if (name == "archive_filter")
        return J::object({{"type", "string"}, {"maxLength", 512}});
    if (name == "game_root" || name == "blender" || name == "converter_root")
        return J::object({{"type", J::array({"string", "null"})}});
    if (name == "options" || name == "metadata" || name == "values" || name == "input")
        return J::object({{"type", "object"}});
    if (name == "preflight")
        return J::object({{"type", "boolean"}});
    if (name == "triangle_band")
        return J::object({{"type", "integer"},
                          {"minimum", 0},
                          {"maximum", 16},
                          {"description", "Assign every supplied face to this native triangle "
                                          "band. Omit to retain qualified per-face membership."}});
    if (name == "native_type")
        return J::object({{"type", "integer"}, {"minimum", 1}, {"maximum", 255}});
    if (name == "ordinal")
        return J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", 254}});
    if (name == "duration_ms")
        return J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", 2147483647}});
    if (name == "index" || name == "to")
        return J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", 65535}});
    if (name == "container" || name == "primitive" || name == "bone")
        return J::object({{"type", "integer"}, {"minimum", 0}});
    if (name == "scale" || name == "rotate_x" || name == "rotate_y" || name == "rotate_z" || name == "start" ||
        name == "end" || name == "time")
        return J::object({{"type", "number"}});
    if (name == "arguments")
        return J::object({{"type", "object"}});
    if (name == "value")
        return J::object();
    if (name == "key" || name == "replacement" || name == "old" || name == "new")
        return J::object({{"type", J::array({"string", "integer", "null"})}});
    if (name == "text")
        return J::object({{"type", "string"}});
    if (name == "include_patchlist" || name == "allow_null")
        return J::object({{"type", "boolean"}});
    if (name == "bank_index")
        return J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", 5}});
    if (name == "offset" || name == "category_index" || name == "expected_generation" || name == "expected_revision")
        return J::object({{"type", "integer"}, {"minimum", 0}});
    if (name == "step_limit")
        return J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", 1000000}});
    if (name == "limit")
        return J::object({{"type", "integer"}, {"minimum", 1}, {"maximum", 500}});
    if (name == "query")
        return J::object({{"type", "string"}, {"maxLength", 512}});
    if (name == "schema" || name == "schema_path" || name == "field_names" || name == "oodle_dll" || name == "source")
        return J::object({{"type", J::array({"string", "null"})}});
    return J::object({{"type", "string"}, {"minLength", 1}});
}
} // namespace Monstey_RuntimeDetail
namespace
{
using namespace Monstey_RuntimeDetail;
Monstey_Json Monstey_Descriptor(const Monstey_Operation &op)
{
    using J = Monstey_Json;
    auto properties = J::object();
    auto required = J::array();
    std::string_view fields = op.fields;
    while (!fields.empty())
    {
        const auto comma = fields.find(',');
        auto name = fields.substr(0, comma);
        const bool need = name.ends_with('!');
        if (need)
            name.remove_suffix(1);
        auto fieldSchema = Monstey_ArgSchema(name);
        if (std::string_view(op.name) == "database_native_banks" && name == "limit")
            fieldSchema = J::object({{"type", "integer"}, {"minimum", 1}, {"maximum", 32}});
        if (std::string_view(op.name) == "database_native_banks" && name == "expected_sha256")
            fieldSchema =
                J::object({{"type", "string"}, {"minLength", 64}, {"maxLength", 64}, {"pattern", "^[0-9a-f]{64}$"}});
        if (std::string_view(op.name) == "database_native_class")
        {
            if (name == "class_pointer")
                fieldSchema = J::object({{"type", J::array({"string", "integer"})},
                                         {"minimum", 1},
                                         {"minLength", 1},
                                         {"maxLength", 18},
                                         {"pattern", "^(0[xX])?[0-9a-fA-F]{1,16}$"}});
            if (name == "offset")
                fieldSchema = J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", 4096}});
            if (name == "limit")
                fieldSchema = J::object({{"type", "integer"}, {"minimum", 1}, {"maximum", 32}});
            if (name == "expected_sha256")
                fieldSchema = J::object(
                    {{"type", "string"}, {"minLength", 64}, {"maxLength", 64}, {"pattern", "^[0-9a-f]{64}$"}});
            if (name == "path")
                fieldSchema = J::object({{"type", "string"}, {"minLength", 1}, {"maxLength", 32768}});
        }
        const bool categoryPath = std::string_view(op.name) == "database_create_category" && name == "path";
        if (categoryPath || ((std::string_view(op.name) == "database_create_category" ||
                              std::string_view(op.name) == "database_create_record") &&
                             name == "name"))
            fieldSchema = J::object({{"type", "string"}});
        if (std::string_view(op.name) == "fx_duplicate_element" && name == "source")
            fieldSchema = J::object({{"type", "string"}, {"minLength", 1}});
        constexpr std::string_view pathFields = "|path|output|destination|schema|schema_path|field_names|source_dir|"
                                                "oodle_dll|template|skeleton|mapping|blender|blend_template|blend|"
                                                "output_blend|output_mesh|native_declarations|";
        if ((!categoryPath && pathFields.find("|" + std::string(name) + "|") != pathFields.npos) ||
            (name == "source" && std::string_view(op.name) != "database_clone" &&
             std::string_view(op.name) != "fx_duplicate_element"))
            fieldSchema["format"] = "path";
        if (name == "game_root" || name == "converter_root")
            fieldSchema["format"] = "directory";
        properties[std::string(name)] = std::move(fieldSchema);
        if (need)
            required.as_array().push_back(std::string(name));
        if (comma == fields.npos)
            break;
        fields.remove_prefix(comma + 1);
    }
    properties["expected_generation"] = Monstey_ArgSchema("expected_generation");
    properties["expected_revision"] = Monstey_ArgSchema("expected_revision");
    auto inputSchema = J::object(
        {{"type", "object"}, {"properties", properties}, {"required", required}, {"additionalProperties", false}});
    if (std::string_view(op.name) == "database_build" || std::string_view(op.name) == "fx_build")
        inputSchema["dependentRequired"] =
            J::object({{"native_declarations", J::array({"expected_native_declarations_sha256"})},
                       {"expected_native_declarations_sha256", J::array({"native_declarations"})}});
    return J::object({{"name", op.name},
                      {"description", op.description},
                      {"read_only", op.read},
                      {"implemented", true},
                      {"implementation", "C++20"},
                      {"inputSchema", std::move(inputSchema)},
                      {"annotations", J::object({{"readOnlyHint", op.read}, {"openWorldHint", false}})}});
}
} // namespace
Monstey_Json Monstey_Runtime::Catalog()
{
    using namespace Monstey_RuntimeDetail;
    using J = Monstey_Json;
    auto result = J::array();
    for (const auto &operation : Monstey_RuntimeOperations())
        result.as_array().push_back(Monstey_Descriptor(operation));
    const auto world = Monstey_WorldCatalog();
    for (auto descriptor : world.as_array())
    {
        descriptor["read_only"] = descriptor.at("annotations").at("readOnlyHint");
        descriptor["implementation"] = "C++20";
        auto &properties = descriptor["inputSchema"]["properties"];
        properties["expected_generation"] = Monstey_ArgSchema("expected_generation");
        properties["expected_revision"] = Monstey_ArgSchema("expected_revision");
        result.as_array().push_back(std::move(descriptor));
    }
    const std::array<J, 10> additionalCatalogs{Monstey_ModelSocketsCatalog(), Monstey_ModelEnvelopeCatalog(),
                                               Monstey_ModelPreviewCatalog(), Monstey_AnimationPreviewCatalog(),
                                               Monstey_FxNestedCatalog(),     Monstey_AnimatedExtrasCatalog(),
                                               Monstey_AnimationCatalog(),    Monstey_FxAttachmentCatalog(),
                                               Monstey_MaterialCatalog(),     Monstey_TextureCatalog()};
    for (const auto &domain : additionalCatalogs)
        for (auto descriptor : domain.as_array())
        {
            const auto read = descriptor.contains("read_only")
                                  ? descriptor.at("read_only").as_bool()
                                  : descriptor.at("name").as_string() == "model_envelope_inspect";
            descriptor["read_only"] = read;
            descriptor["implemented"] = true;
            descriptor["implementation"] = "C++20";
            descriptor["annotations"] = J::object({{"readOnlyHint", read}, {"openWorldHint", false}});
            auto &properties = descriptor["inputSchema"]["properties"];
            properties["expected_generation"] = Monstey_ArgSchema("expected_generation");
            properties["expected_revision"] = Monstey_ArgSchema("expected_revision");
            if (properties.contains("skeleton") && !properties["skeleton"].contains("format"))
                properties["skeleton"]["format"] = "path";
            result.as_array().push_back(std::move(descriptor));
        }
    return result;
}
} // namespace Monstey
