#pragma once
#include "Monstey_FxLibrary.hpp"
namespace Monstey {
// Native CreateFX Effect0..15 grammar; analysis never executes an effect.
Monstey_Json Monstey_FxEffectNames(std::string_view text);
Monstey_Json Monstey_FxNestedCatalog();
Monstey_Json
Monstey_FxNestedCall(Monstey_Gadb &database, std::string_view operation,
                     const Monstey_Json &arguments = Monstey_Json::object());
} // namespace Monstey
