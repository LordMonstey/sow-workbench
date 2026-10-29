#pragma once
#include "Monstey_GadbManagerBackend.hpp"
#include "Monstey_GadbManagerFree.hpp"
#include "Monstey_GadbManagerGuardProjection.hpp"
#include "Monstey_GadbManagerLeaveProjection.hpp"
#include <cstdint>
namespace Monstey
{
struct Monstey_GadbManagerParentResult
{
    Monstey_GadbBufferedResult raw;
    std::uint64_t post_leave_r8;
    std::uint64_t post_leave_r9;
};
Monstey_GadbManagerParentResult Monstey_GadbCreateManagerBackendForManager(
    Monstey_GadbBinderMemory, std::uint64_t manager, std::uint32_t flags, std::uint64_t entry_r8,
    std::uint64_t entry_r9_backing, std::uint64_t body_rsp, const Monstey_GadbManagerEnterProjectionServices &,
    const Monstey_GadbManagerLeaveProjectionServices &, const Monstey_GadbManagerBackendServices &,
    Monstey_GadbManagerBackendBindings = {});
} // namespace Monstey
