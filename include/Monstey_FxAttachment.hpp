#pragma once
#include "Monstey_FxLibrary.hpp"
namespace Monstey {
// Qualified native BaseFX owner-frame attachment; not a local key-to-key link.
Monstey_Json Monstey_FxAttachmentCatalog();
Monstey_Json Monstey_FxAttachmentCall(
    Monstey_Gadb &database, std::string_view operation,
    const Monstey_Json &arguments = Monstey_Json::object());
} // namespace Monstey
