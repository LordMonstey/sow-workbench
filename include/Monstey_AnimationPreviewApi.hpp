#pragma once
#include "Monstey_AnimationPreview.hpp"
#include "Monstey_ModelPreview.hpp"
namespace Monstey
{
// Read-only inputs, new offline BMP output of one ordinary same-rig CPU pose.
Monstey_Json Monstey_AnimationPreviewCatalog();
Monstey_Json Monstey_AnimationPreviewRenderFile(const Monstey_Json &arguments);
Monstey_Json Monstey_AnimationPreviewCall(std::string_view operation,
                                          const Monstey_Json &arguments = Monstey_Json::object());
} // namespace Monstey
