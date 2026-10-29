#pragma once
#include "Monstey_Gadb.hpp"
namespace Monstey {
Monstey_Json Monstey_AnimatedExtrasCatalog();
Monstey_Json Monstey_AnimatedExtrasCall(Monstey_Gadb &database, std::string_view operation,
                                        const Monstey_Json &arguments = Monstey_Json::object());
} // namespace Monstey
