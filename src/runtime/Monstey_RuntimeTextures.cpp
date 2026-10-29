#include "Monstey_Runtime.hpp"
#include "Monstey_RuntimeArguments.hpp"
#include "Monstey_TextureApi.hpp"

namespace Monstey
{
using namespace Monstey_RuntimeDetail;

std::optional<Monstey_Json> Monstey_Runtime::Monstey_TryTextures(std::string_view op, const Monstey_Json &a)
{
    if (op == "texture_inspect" || op == "texture_preview_render" || op == "texture_extract" ||
        op == "texture_roundtrip")
    {
        auto values = a;
        values.as_object().erase("expected_generation");
        values.as_object().erase("expected_revision");
        settings_->Protect(Monstey_Path(values, "source"));
        return Monstey_TextureCall(op, values);
    }
    return std::nullopt;
}
} // namespace Monstey
