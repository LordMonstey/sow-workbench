#pragma once
#include "Monstey_Json.hpp"
namespace Monstey::Monstey_RuntimeDetail
{
Monstey_Json Monstey_ArgSchema(std::string_view name);
void Monstey_Validate(const Monstey_Json &arguments, const Monstey_Json &schema);
} // namespace Monstey::Monstey_RuntimeDetail
