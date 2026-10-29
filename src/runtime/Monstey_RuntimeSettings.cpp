#include "Monstey_Runtime.hpp"
#include "Monstey_RuntimeArguments.hpp"

namespace Monstey
{
using namespace Monstey_RuntimeDetail;

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TrySettings(std::string_view op, const Monstey_Json &a)
{
    using J = Monstey_Json;
    if (op == "configure")
        return settings_->Configure(a);
    if (op == "components")
        return J::object({{"implementation", "C++20"},
                          {"python_runtime", false},
                          {"configuration", settings_->Describe()},
                          {"model_converter", "builtin:MMSH17/SKEL17/GLB2/BLEND"},
                          {"world_tools", "builtin:C++20"},
                          {"native_blender_model_adapter_complete", true}});
    return std::nullopt;
}
} // namespace Monstey
