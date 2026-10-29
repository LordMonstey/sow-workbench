#pragma once
#include "Monstey_Gadb.hpp"
namespace Monstey {
// Native LTFX8 envelope qualified across all 436 local original FX inputs.
Monstey_Bytes Monstey_FxDefaultPrefix();
// Prefix provenance is explicit: parse a real LTFX8 source and qualify its captured snapshot.
Monstey_Bytes Monstey_FxQualifiedPrefix(const std::filesystem::path &source,
                                        std::string_view expectedSha256 = {});
// Creates one independent physical element without requiring FxGlobalProps or linking KeyList.
Monstey_Json Monstey_FxCreateElement(Monstey_Gadb &database, const Monstey_Json &arguments);
// Replays native first-record binding predicates and checks every later raw record layout.
// Reports compatibility; never modifies the engine cache or executes the original binary.
Monstey_Json Monstey_FxVerifyNativeBindings(Monstey_Gadb &database);
// All native records, editor layout, transactions, projects and encoding remain in the same Gadb instance.
Monstey_Json Monstey_FxWorkflowCall(Monstey_Gadb &database, std::string_view operation,
                                    const Monstey_Json &arguments = Monstey_Json::object());
} // namespace Monstey