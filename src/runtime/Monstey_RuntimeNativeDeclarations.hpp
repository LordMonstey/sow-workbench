#pragma once
#include "Monstey_Runtime.hpp"
namespace Monstey::Monstey_RuntimeDetail
{
Monstey_Json Monstey_BuildWithNativeDeclarations(Monstey_Gadb &database, const Monstey_Json &arguments,
                                                 Monstey_Settings &settings, bool effects);
} // namespace Monstey::Monstey_RuntimeDetail
