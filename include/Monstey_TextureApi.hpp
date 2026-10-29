#pragma once
#include "Monstey_TextureResource.hpp"
namespace Monstey {
Monstey_Json Monstey_TextureCatalog();
Monstey_Json Monstey_TextureCall(std::string_view operation,
                                 const Monstey_Json &arguments);
} // namespace Monstey
