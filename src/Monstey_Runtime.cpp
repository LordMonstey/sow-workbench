#include "Monstey_Runtime.hpp"
#include "runtime/Monstey_RuntimeArguments.hpp"
#include "runtime/Monstey_RuntimeRegistry.hpp"
#include <algorithm>
#include <utility>

namespace Monstey
{
using namespace Monstey_RuntimeDetail;

Monstey_Runtime::Monstey_Runtime(std::filesystem::path schema) : Monstey_Runtime(std::move(schema), {}) {}
Monstey_Runtime::Monstey_Runtime(std::filesystem::path schema, std::filesystem::path settings)
    : schema_(std::move(schema)), settings_(std::make_unique<Monstey_Settings>(settings))
{
    settings_->Protect(schema_);
}
Monstey_Json Monstey_Runtime::Call(const Monstey_Json &request)
{
    std::lock_guard lock(mutex_);
    using J = Monstey_Json;
    auto reply = J::object();
    try
    {
        if (!request.is_object())
            throw Monstey_Error("invalid_request", "Request must be an object");
        if (request.contains("id"))
            reply["id"] = request.at("id");
        const auto operation = Monstey_Text(request, "operation");
        static const auto catalog = Catalog();
        const auto &definitions = catalog.as_array();
        const auto definition = std::find_if(definitions.begin(), definitions.end(),
                                             [&](const auto &row) { return operation == row.at("name").as_string(); });
        if (definition == definitions.end())
            throw Monstey_Error("unknown_operation", "Unknown native operation: " + operation);
        reply["operation"] = operation;
        const auto arguments = request.value("arguments", J::object());
        Monstey_Validate(arguments, definition->at("inputSchema"));
        if (arguments.contains("expected_generation") && arguments.at("expected_generation").as_uint() != generation_)
            throw Monstey_Error("stale_session", "The source session changed; reopen the record properties");
        if (arguments.contains("expected_revision") && arguments.at("expected_revision").as_uint() != revision_)
            throw Monstey_Error("stale_revision", "The project changed; refresh its effective values");
        auto result = Execute(operation, arguments);
        if (!definition->at("read_only").as_bool())
            ++revision_;
        reply["ok"] = true;
        reply["result"] = std::move(result);
    }
    catch (const Monstey_Error &error)
    {
        reply["ok"] = false;
        reply["error"] = J::object({{"code", error.code}, {"message", error.what()}, {"details", J::object()}});
    }
    catch (const std::exception &error)
    {
        reply["ok"] = false;
        reply["error"] = J::object({{"code", "native_error"}, {"message", error.what()}, {"details", J::object()}});
    }
    reply["api_version"] = "0.3.0-dev";
    reply["runtime_verified"] = false;
    reply["generation"] = generation_;
    reply["revision"] = revision_;
    return reply;
}

Monstey_Json Monstey_Runtime::Reply(std::string_view request)
{
    const auto failure = [](const char *code, const char *message)
    {
        return Monstey_Json::object(
            {{"ok", false}, {"error", Monstey_Json::object({{"code", code}, {"message", message}})}});
    };
    if (request.size() > 64u * 1024)
        return failure("request_too_large", "JSON requests are limited to 64 KiB");
    Monstey_Json parsed;
    try
    {
        parsed = Monstey_Json::Parse(request);
    }
    catch (const std::exception &error)
    {
        return failure("invalid_json", error.what());
    }
    return Call(parsed);
}

Monstey_Json Monstey_Runtime::Execute(std::string_view op, const Monstey_Json &a)
{
    if (auto result = Monstey_TryDatabaseTrace(op, a))
        return std::move(*result);
    if (auto result = Monstey_TryTextures(op, a))
        return std::move(*result);
    if (auto result = Monstey_TryMaterial(op, a))
        return std::move(*result);
    if (auto result = Monstey_TryAnimation(op, a))
        return std::move(*result);
    if (auto result = Monstey_TryModelViews(op, a))
        return std::move(*result);
    if (auto result = Monstey_TryEffectTools(op, a))
        return std::move(*result);
    if (op.starts_with("sow_"))
        return WorldCall(op, a);
    const Monstey_Arguments arguments(a);
    if (auto result = Monstey_TrySettings(op, a))
        return std::move(*result);
    if (auto result = Monstey_TryArchiveCatalog(op, a))
        return std::move(*result);
    if (auto result = Monstey_TryModels(op, a))
        return std::move(*result);
    if (auto result = Monstey_TryWorld(op, a))
        return std::move(*result);
    if (auto result = Monstey_TrySession(op))
        return std::move(*result);
    if (auto result = Monstey_TryDatabase(op, a, arguments))
        return std::move(*result);
    if (auto result = Monstey_TryArchiveProject(op, a, arguments))
        return std::move(*result);
    if (auto result = Monstey_TryStrings(op, a, arguments))
        return std::move(*result);
    if (auto result = Monstey_TryNativeDatabase(op, a))
        return std::move(*result);
    if (auto result = Monstey_TryEffects(op, a, arguments))
        return std::move(*result);
    throw Monstey_Error("unknown_operation", "Unknown native operation");
}
} // namespace Monstey
