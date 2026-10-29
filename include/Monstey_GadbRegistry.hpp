#pragma once
#include "Monstey_FxReflection.hpp"
namespace Monstey {
Monstey_Json Monstey_GadbDiscoverNativeDeclarations(std::span<const std::uint8_t> image,
    std::string_view expectedImageSha256);
}
