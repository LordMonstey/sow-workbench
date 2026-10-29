#pragma once
#include "Monstey_Catalog.hpp"
namespace Monstey {
Monstey_Json Monstey_WorldAssetsCatalog();
Monstey_Json Monstey_WorldAssetsCall(std::string_view operation,
                                     const Monstey_Json &arguments);
} // namespace Monstey