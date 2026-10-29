#pragma once

#include "Monstey_GadbManagerParent.hpp"
#include "Monstey_BackendReference.hpp"

namespace Monstey_ParentReview {

Monstey::Monstey_GadbManagerParentResult Monstey_ReferenceCreateManagerBackendForManager(
    Monstey::Monstey_GadbBinderMemory memory,
    std::uint64_t manager, std::uint32_t flags,
    std::uint64_t entry_r8, std::uint64_t entry_r9_backing,
    std::uint64_t body_rsp,
    const Monstey::Monstey_GadbManagerEnterProjectionServices &enter,
    const Monstey::Monstey_GadbManagerLeaveProjectionServices &leave,
    const Monstey_BackendReview::Monstey_Services &backend,
    Monstey::Monstey_GadbManagerBackendBindings bindings = {});

} // namespace Monstey_ParentReview
