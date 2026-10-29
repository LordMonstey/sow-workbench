#include "Monstey_ParentReference.hpp"
#include "Monstey_FreeReference.hpp"
#include "Monstey_GuardProjectionReference.hpp"
#include "Monstey_LeaveProjectionReference.hpp"

#include <limits>
#include <span>

namespace Monstey_ParentReview {
namespace {
using Monstey_U64 = std::uint64_t;
using Monstey_Memory = Monstey::Monstey_GadbBinderMemory;
using Monstey_Raw = Monstey::Monstey_GadbBufferedResult;

std::span<std::uint8_t> Monstey_Word(
    Monstey_Memory memory, Monstey_U64 address, bool writing) {
    if (address < memory.base || address > std::numeric_limits<Monstey_U64>::max() - 7)
        throw Monstey::Monstey_GadbBufferedMemoryFault(address, 8, writing);
    const auto offset = address - memory.base;
    if (offset > memory.bytes.size() ||
        memory.bytes.size() - static_cast<std::size_t>(offset) < 8)
        throw Monstey::Monstey_GadbBufferedMemoryFault(address, 8, writing);
    return memory.bytes.subspan(static_cast<std::size_t>(offset), 8);
}

Monstey_U64 Monstey_ReadWord(Monstey_Memory memory, Monstey_U64 address) {
    const auto bytes = Monstey_Word(memory, address, false);
    Monstey_U64 value = 0;
    for (std::size_t byte = 0; byte < 8; ++byte)
        value |= Monstey_U64(bytes[byte]) << (byte * 8);
    return value;
}

void Monstey_WriteWord(Monstey_Memory memory, Monstey_U64 address, Monstey_U64 value) {
    auto bytes = Monstey_Word(memory, address, true);
    for (std::size_t byte = 0; byte < 8; ++byte)
        bytes[byte] = static_cast<std::uint8_t>(value >> (byte * 8));
}

Monstey_BackendReview::Monstey_Globals Monstey_ModeBindings(
    const Monstey::Monstey_GadbManagerBackendModeCells &cells) {
    return {cells.cookie, cells.page, cells.granularity, cells.auxiliary, cells.flags,
            cells.secondary_flags, cells.template_cs, cells.state, cells.main_cs};
}

Monstey_BackendReview::Monstey_Bindings Monstey_BackendBindings(
    const Monstey::Monstey_GadbManagerBackendBindings &bindings) {
    Monstey_BackendReview::Monstey_Bindings result;
    const auto &imports = bindings.imports;
    result.imports = {imports.fill, imports.allocate, imports.system_info, imports.sleep,
                      imports.random, imports.initialize_cs, imports.enter_cs, imports.leave_cs};
    const auto &placement = bindings.placement;
    result.lower = {placement.tail_40, placement.tail_80, placement.tail_c0,
                    placement.order_table};
    result.modes = {Monstey_ModeBindings(bindings.mode_40),
                    Monstey_ModeBindings(bindings.mode_80),
                    Monstey_ModeBindings(bindings.mode_c0)};
    return result;
}

Monstey_Raw Monstey_Trap(Monstey_Memory memory, Monstey_U64 entry,
                         Monstey_Raw incoming, Monstey_U64 r8, Monstey_U64 r9) {
    const Monstey_FreeReview::Monstey_Services unused_services;
    const auto result = Monstey_FreeReview::Monstey_Run(
        0, {memory.base, memory.bytes, nullptr}, Monstey_FreeReview::Monstey_Operation::trap,
        Monstey_FreeReview::Monstey_Trap{entry, {incoming.rax, incoming.rdx}, r8, r9},
        unused_services);
    return {result.a, result.d};
}
} // namespace

Monstey::Monstey_GadbManagerParentResult Monstey_ReferenceCreateManagerBackendForManager(
    Monstey::Monstey_GadbBinderMemory memory,
    std::uint64_t manager, std::uint32_t flags,
    std::uint64_t entry_r8, std::uint64_t entry_r9_backing,
    std::uint64_t body_rsp,
    const Monstey::Monstey_GadbManagerEnterProjectionServices &enter,
    const Monstey::Monstey_GadbManagerLeaveProjectionServices &leave,
    const Monstey_BackendReview::Monstey_Services &backend,
    Monstey::Monstey_GadbManagerBackendBindings bindings) {
    const auto table = manager + 0x190ULL;
    const auto lock = manager + 0x208ULL;
    const auto guard = body_rsp + 0x30ULL;
    const Monstey::Monstey_GadbManagerPrimitiveBindings sync_bindings{
        std::nullopt, bindings.imports.enter_cs, bindings.imports.leave_cs};
    const auto lower_bindings = Monstey_BackendBindings(bindings);
    const auto entered = Monstey::Monstey_ReferenceConstructManagerGuardProjected(
        memory, guard, lock, {body_rsp + 0x28ULL, lock},
        entry_r8, entry_r9_backing, sync_bindings, enter);
    auto raw = entered.raw;
    Monstey_U64 selected = 0;
    if (Monstey_ReadWord(memory, table) != 0)
        raw = Monstey_Trap(memory, body_rsp - 8, raw,
                           entered.post_enter_r8, entered.post_enter_r9);

    const auto mode = flags & 0xC0000000u;
    raw = {mode, 0xC0000000ULL};
    if (mode == 0)
        raw = Monstey_Trap(memory, body_rsp - 8, raw,
                           entered.post_enter_r8, entered.post_enter_r9);

    const Monstey_PlacementReview::Monstey_Image image{memory.base, memory.bytes, nullptr};
    if (mode == 0x40000000u) {
        const auto count = Monstey_ReadWord(memory, body_rsp + 0x50ULL);
        const auto created = Monstey_BackendReview::Monstey_RefCreate40(
            image, Monstey_U64(flags), entry_r9_backing, count, body_rsp - 0x400ULL,
            backend, lower_bindings);
        raw = {created.a, created.d};
    } else if (mode == 0x80000000u) {
        const auto count = Monstey_ReadWord(memory, body_rsp + 0x50ULL);
        const auto created = Monstey_BackendReview::Monstey_RefCreate80(
            image, Monstey_U64(flags), entry_r9_backing, count, body_rsp - 0x400ULL,
            backend, lower_bindings);
        raw = {created.a, created.d};
    } else if (mode == 0xC0000000u) {
        const auto count = Monstey_ReadWord(memory, body_rsp + 0x50ULL);
        const auto created = Monstey_BackendReview::Monstey_RefCreateC0(
            image, Monstey_U64(flags), entry_r9_backing, count, body_rsp - 0x400ULL,
            backend, lower_bindings);
        raw = {created.a, created.d};
    }

    if (mode != 0 && raw.rax != 0) {
        const auto object = raw.rax;
        raw.rdx = 0;
        raw.rax = table;
        Monstey_WriteWord(memory, raw.rax, object);
        raw.rax = table;
        Monstey_WriteWord(memory, object + 0x3C0ULL, raw.rax);
        Monstey_WriteWord(memory, object + 0x3C8ULL, selected);
        selected = object;
    }
    const auto departed = Monstey::Monstey_ReferenceDestroyManagerGuardProjected(
        memory, guard, raw, sync_bindings, leave);
    return {{selected, departed.raw.rdx}, departed.post_leave_r8, departed.post_leave_r9};
}

} // namespace Monstey_ParentReview
