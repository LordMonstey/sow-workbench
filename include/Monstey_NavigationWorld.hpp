#pragma once
#include "Monstey_Navigation.hpp"
namespace Monstey {
Monstey_Json Monstey_NavigationWorldCall(std::string_view operation,
                                         const Monstey_Json &arguments);
}
