#pragma once
#include "Monstey_FxWorkflow.hpp"
namespace Monstey {
class Monstey_GadbClassCache;
// Component prefabs are inactive native records plus explicit editor inheritance metadata.
// All property materialization/history/projects remain in the caller's shared Gadb.
Monstey_Json Monstey_FxLibraryCall(Monstey_Gadb &database, std::string_view operation,
                                   const Monstey_Json &arguments = Monstey_Json::object());
// Routes existing native edits/deletes/writers coherently when prefab metadata is present.
Monstey_Json Monstey_FxLibraryNativeCall(Monstey_Gadb &database, std::string_view operation,
                                         const Monstey_Json &arguments = Monstey_Json::object());
// Explicit opt-in writer guard, including prefab materialization; legacy overload is unchanged.
Monstey_Json Monstey_FxLibraryNativeCall(Monstey_Gadb &database, std::string_view operation,
                                         const Monstey_Json &arguments,
                                         const Monstey_GadbClassCache *declarations);
} // namespace Monstey
