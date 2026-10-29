#include "Monstey_GadbManagerParent.hpp"
#include <limits>

namespace Monstey
{
namespace
{
std::size_t Monstey_ParentOffset(Monstey_GadbBinderMemory memory, std::uint64_t address, bool writing)
{
    if (address < memory.base || address > std::numeric_limits<std::uint64_t>::max() - 7u ||
        address - memory.base > memory.bytes.size() ||
        8u > memory.bytes.size() - static_cast<std::size_t>(address - memory.base))
        throw Monstey_GadbBufferedMemoryFault(address, 8, writing);
    return static_cast<std::size_t>(address - memory.base);
}

std::uint64_t Monstey_ParentRead64(Monstey_GadbBinderMemory memory, std::uint64_t address)
{
    const auto offset = Monstey_ParentOffset(memory, address, false);
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i)
        value |= std::uint64_t(memory.bytes[offset + i]) << (i * 8u);
    return value;
}

void Monstey_ParentWrite64(Monstey_GadbBinderMemory memory, std::uint64_t address, std::uint64_t value)
{
    const auto offset = Monstey_ParentOffset(memory, address, true);
    for (std::size_t i = 0; i < 8; ++i)
        memory.bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
}

std::uint64_t Monstey_ParentSelectorZero(std::uint64_t table) noexcept
{
    constexpr std::uint64_t index = 0;
    static_assert(index < 15);
    return table + index * 8u;
}
} // namespace

Monstey_GadbManagerParentResult Monstey_GadbCreateManagerBackendForManager(
    Monstey_GadbBinderMemory memory, std::uint64_t manager, std::uint32_t flags, std::uint64_t entry_r8,
    std::uint64_t entry_r9_backing, std::uint64_t body_rsp,
    const Monstey_GadbManagerEnterProjectionServices &enter_services,
    const Monstey_GadbManagerLeaveProjectionServices &leave_services,
    const Monstey_GadbManagerBackendServices &backend_services, Monstey_GadbManagerBackendBindings bindings)
{
    const auto table = manager + 0x190u;
    const auto lock = manager + 0x208u;
    const auto guard = body_rsp + 0x30u;
    Monstey_GadbManagerPrimitiveBindings primitive_bindings;
    primitive_bindings.enter_cell = bindings.imports.enter_cs;
    primitive_bindings.leave_cell = bindings.imports.leave_cs;
    const auto entered = Monstey_GadbConstructManagerGuardProjected(
        memory, guard, lock, {body_rsp + 0x28u, lock}, entry_r8, entry_r9_backing, primitive_bindings, enter_services);
    std::uint64_t selected = 0;
    if (Monstey_ParentRead64(memory, table) != 0)
        static_cast<void>(Monstey_GadbManagerFreeTrapReturn(memory, body_rsp - 8u, entered.raw, entered.post_enter_r8,
                                                            entered.post_enter_r9));

    const std::uint32_t mode = flags & 0xC0000000u;
    Monstey_GadbBufferedResult current{mode, 0xC0000000u};
    if (mode == 0)
        current = Monstey_GadbManagerFreeTrapReturn(memory, body_rsp - 8u, current, entered.post_enter_r8,
                                                    entered.post_enter_r9);

    if (mode == 0x40000000u)
        current = Monstey_GadbCreateManagerBackend40(memory, std::uint64_t(flags), entry_r9_backing,
                                                     Monstey_ParentRead64(memory, body_rsp + 0x50u), body_rsp - 0x400u,
                                                     backend_services, bindings);
    else if (mode == 0x80000000u)
        current = Monstey_GadbCreateManagerBackend80(memory, std::uint64_t(flags), entry_r9_backing,
                                                     Monstey_ParentRead64(memory, body_rsp + 0x50u), body_rsp - 0x400u,
                                                     backend_services, bindings);
    else if (mode == 0xC0000000u)
        current = Monstey_GadbCreateManagerBackendC0(memory, std::uint64_t(flags), entry_r9_backing,
                                                     Monstey_ParentRead64(memory, body_rsp + 0x50u), body_rsp - 0x400u,
                                                     backend_services, bindings);

    if (mode != 0 && current.rax != 0)
    {
        const auto backend = current.rax;
        current.rdx = 0;
        current.rax = Monstey_ParentSelectorZero(table);
        Monstey_ParentWrite64(memory, current.rax, backend);
        current.rax = Monstey_ParentSelectorZero(table);
        Monstey_ParentWrite64(memory, backend + 0x3C0u, current.rax);
        Monstey_ParentWrite64(memory, backend + 0x3C8u, selected);
        selected = backend;
    }
    const auto left =
        Monstey_GadbDestroyManagerGuardProjected(memory, guard, current, primitive_bindings, leave_services);
    return {{selected, left.raw.rdx}, left.post_leave_r8, left.post_leave_r9};
}
} // namespace Monstey
