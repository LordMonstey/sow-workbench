#pragma once
#include "Monstey_Core.hpp"
namespace Monstey {
Monstey_Json Monstey_WorldBlenderCatalog();
Monstey_Json Monstey_WorldBlenderCall(std::string_view operation, const Monstey_Json &arguments);
} // namespace Monstey