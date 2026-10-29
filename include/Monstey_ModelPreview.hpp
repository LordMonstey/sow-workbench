#pragma once
#include "Monstey_Model.hpp"
namespace Monstey
{
// Host-side orthographic inspection. This is not a game material or animation renderer.
struct Monstey_ModelPreviewOptions
{
    std::uint32_t width = 900, height = 900;
    std::size_t container = 0;
    bool all_containers = false, mesh = true, bones = true;
    bool bones_in_front = true, wireframe = false, vertex_colors = false;
    double yaw_degrees = 0, pitch_degrees = 0, zoom = 1, pan_x = 0, pan_y = 0;
};
struct Monstey_ModelPreviewFrame
{
    std::uint32_t width = 0, height = 0;
    // Top-down BGRA words, alpha255. Ownership belongs to the returned frame.
    std::vector<std::uint32_t> pixels;
    Monstey_Json report;
};
Monstey_ModelPreviewFrame Monstey_ModelPreviewRender(const Monstey_ModelScene &scene,
                                                     const Monstey_ModelPreviewOptions &options = {});
// A stable reference keeps camera framing fixed while evaluated geometry moves.
Monstey_ModelPreviewFrame Monstey_ModelPreviewRenderFramed(const Monstey_ModelScene& scene,
    const Monstey_ModelScene& reference, const Monstey_ModelPreviewOptions& options = {});
Monstey_Bytes Monstey_ModelPreviewBitmap(const Monstey_ModelPreviewFrame &frame);
} // namespace Monstey