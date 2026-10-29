#pragma once
#include "Monstey_Core.hpp"
namespace Monstey
{
// Independent, bounded read-only evidence probe. No writer or runtime role is implied.
Monstey_Json Monstey_WdscAuxiliaryInspect(std::span<const std::uint8_t> source);
Monstey_Json Monstey_WdscAuxiliaryInspectFile(const std::filesystem::path &source);
} // namespace Monstey