#pragma once
#include "Monstey_ModelPreview.hpp"
namespace Monstey {
// Offline host bind-geometry BMP output; native materials and animation are not rendered.
Monstey_Json Monstey_ModelPreviewCatalog();
Monstey_Json Monstey_ModelPreviewRenderFile(const Monstey_Json& arguments);
Monstey_Json Monstey_ModelPreviewCall(std::string_view operation,
    const Monstey_Json& arguments = Monstey_Json::object());
} // namespace Monstey
