#pragma once
#include "Monstey_FxWorkflow.hpp"
namespace Monstey {
// Portable native graph capture; only the shared Gadb snapshot owns atom/descriptor encoding.
Monstey_Json Monstey_FxGraphCapture(Monstey_Gadb &database, const Monstey_Json &arguments);
// Clipboard copy/cut, original cursor-centred paste, and explicit timing-preserving graph import.
Monstey_Json Monstey_FxGraphCall(Monstey_Gadb &database, std::string_view operation,
                                 const Monstey_Json &arguments = Monstey_Json::object());
} // namespace Monstey
