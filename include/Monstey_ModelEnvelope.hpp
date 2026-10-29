#pragma once
#include "Monstey_Core.hpp"
namespace Monstey {
// Hash-qualified SKEL17 sphere authoring; decoded bind geometry is covered.
// Existing donor padding stays; animation and cloth coverage remain unverified.
Monstey_Json Monstey_ModelEnvelopeCatalog();
Monstey_Json Monstey_ModelEnvelopeInspect(const Monstey_Json &arguments);
Monstey_Json Monstey_ModelEnvelopeBuild(const Monstey_Json &arguments);
} // namespace Monstey