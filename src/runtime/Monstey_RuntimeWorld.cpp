#include "Monstey_Runtime.hpp"
#include "Monstey_RuntimeArguments.hpp"
#include "Monstey_World.hpp"
#include "Monstey_WdscAuxiliary.hpp"

namespace Monstey
{
using namespace Monstey_RuntimeDetail;

Monstey_Json Monstey_Runtime::WorldCall(std::string_view name, const Monstey_Json &arguments)
{
    auto values = arguments;
    values.as_object().erase("expected_generation");
    values.as_object().erase("expected_revision");
    const auto config = settings_->Describe();
    if ((name == "sow_world_asset_extract" || name == "sow_blender_prepare") && !values.contains("game_root") &&
        config.contains("game_root") && config.at("game_root").is_string() &&
        !config.at("game_root").as_string().empty())
        values["game_root"] = config.at("game_root");
    if (name == "sow_blender_build" && !values.contains("blender") && config.contains("blender") &&
        config.at("blender").is_string() && !config.at("blender").as_string().empty())
        values["blender"] = config.at("blender");
    return Monstey_WorldCall(name, values);
}

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryWorld(std::string_view op, const Monstey_Json &a)
{
    if (op == "world_auxiliary_inspect")
        return Monstey_WdscAuxiliaryInspectFile(Monstey_Path(a, "source"));
    if (op == "world_catalog")
        return Monstey_WorldCatalog();
    if (op == "world_call")
        return WorldCall(Monstey_Text(a, "name"), a.at("arguments"));
    return std::nullopt;
}
} // namespace Monstey
