#pragma once
#include "Monstey_Core.hpp"
namespace Monstey {
Monstey_Json Monstey_WorldEditCatalog();
Monstey_Json Monstey_WorldEditCall(std::string_view operation, const Monstey_Json &arguments);
} // namespace Monstey
