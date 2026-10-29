#pragma once
#include "Monstey_Core.hpp"
namespace Monstey {
// Read-only qualification using the same native LTFX/GADB schema as the builder.
Monstey_Json Monstey_WorldLightInspect(const std::filesystem::path &source,
    std::string_view resource, std::span<const std::uint8_t> owner,
    std::string_view owner_name);
Monstey_Json Monstey_WorldLightingCatalog();
Monstey_Json Monstey_WorldLightingCall(std::string_view operation, const Monstey_Json &arguments);
} // namespace Monstey
