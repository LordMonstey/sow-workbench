#pragma once
#include "Monstey_Gadb.hpp"
namespace Monstey {
// Native GADB Model.SimpleModelSocketOffsets -> ModelSocketOverride workflow.
// Mutations use the caller's session and one shared undo transaction. No model/container is rebuilt here.
Monstey_Json Monstey_ModelSocketsCall(Monstey_Gadb &database, std::string_view operation,
                                      const Monstey_Json &arguments = Monstey_Json::object());
Monstey_Json Monstey_ModelSocketsCatalog();
} // namespace Monstey