#include "Monstey_ParentDifferential.hpp"
#include <limits>

namespace Monstey_ParentDifferential
{
namespace
{
void Monstey_Expect(Monstey_Case &c, Monstey_U64 selected)
{
    c.expected_fault.reset();
    c.expected_raw = {{selected, c.leave_out.raw.rdx, c.leave_out.post_leave_r8, c.leave_out.post_leave_r9}};
}
void Monstey_FaultExpected(Monstey_Case &c, Monstey_FaultKind kind,
    std::optional<Monstey_U64> address = {}, std::optional<std::size_t> width = {}, std::optional<bool> writing = {})
{
    c.expected_raw.clear();
    c.expected_fault = Monstey_ExpectedFault{kind, address, width, writing};
}
void Monstey_SeedMode(Monstey_Case &c, const Monstey::Monstey_GadbManagerBackendModeCells &m)
{
    Monstey_Seed(c, m.cookie, 1, 8);
    Monstey_Seed(c, m.page, 0x1000, 8);
    Monstey_Seed(c, m.granularity, 0x10000, 8);
    Monstey_Seed(c, m.auxiliary, 0, 8);
    Monstey_Seed(c, m.flags, 0, 4);
    Monstey_Seed(c, m.secondary_flags, 0, 4);
    Monstey_Seed(c, m.state, 2, 4);
}
Monstey_Case Monstey_Base(std::string id, Monstey_U32 mode = 0x40000000u,
    bool occupied = false, bool success = true, Monstey_U64 base = 0)
{
    Monstey_Case c;
    c.id = std::move(id); c.criteria = {"PP02", "PP03", "PP06", "PP08", "PP09", "PP11", "PP12"};
    c.base = base; c.bytes.assign(0x30000, 0xa5);
    c.requests = {{base + 0x4000, 0xcafebabedeadbeefULL, base + 0x10000, base + 0x8000, mode}};
    c.enter_out = {0x0102030405060708ULL, 0x89abcdef12345678ULL, 0xfedcba9876543210ULL};
    c.leave_out = {{0xabcdcccc87654321ULL, 0x1122334455667788ULL}, 0x99aabbccddeeff00ULL, 0x1020304050607080ULL};
    c.fill_rdx = 0x8877665544332211ULL; c.allocate_rdx = 0x11220000aabbccddULL;
    c.initialize_rdx = 0xf0000000aaaabbbbULL; c.backend_leave_rdx = 0xe000000012345678ULL;
    c.random_eax = 0x7fffffff;
    c.token = std::make_shared<const Monstey_Token>(Monstey_Token{0xa17bca11});
    c.bindings.imports = {base + 0x100, base + 0x108, base + 0x110, base + 0x118,
        base + 0x120, base + 0x128, base + 0x130, base + 0x138};
    c.bindings.mode_40 = {base + 0x200, base + 0x208, base + 0x210, base + 0x218,
        base + 0x220, base + 0x228, base + 0x240, base + 0x230, base + 0x280};
    c.bindings.mode_80 = {base + 0x300, base + 0x308, base + 0x310, base + 0x318,
        base + 0x320, base + 0x328, base + 0x340, base + 0x330, base + 0x380};
    c.bindings.mode_c0 = {base + 0x400, base + 0x408, base + 0x410, base + 0x418,
        base + 0x420, base + 0x428, base + 0x440, base + 0x430, base + 0x480};
    c.bindings.placement = {base + 0x500, base + 0x508, base + 0x510, base + 0x540};
    for (unsigned i = 0; i < 8; ++i)
        Monstey_Seed(c, base + 0x100 + 8 * i, 0xf000100000000000ULL + i, 8);
    Monstey_SeedMode(c, c.bindings.mode_40); Monstey_SeedMode(c, c.bindings.mode_80); Monstey_SeedMode(c, c.bindings.mode_c0);
    for (unsigned i = 0; i < 6; ++i) Monstey_Seed(c, base + 0x540 + 4 * i, i, 4);
    Monstey_Seed(c, base + 0x500, 0x55667788, 8);
    Monstey_Seed(c, base + 0x508, 0x66778899, 8);
    Monstey_Seed(c, base + 0x510, 0x778899aa, 8);
    Monstey_Seed(c, c.requests[0].manager + 0x190, occupied ? 1u : 0u, 8);
    Monstey_Seed(c, c.requests[0].body + 0x50, success ? 0x4000u : 0x420u, 8);
    Monstey_Expect(c, mode && success ? base + 0x10010 : 0);
    return c;
}
void Monstey_InitializePath(Monstey_Case &c, Monstey_U32 state)
{
    c.criteria.push_back("PP08");
    Monstey_Seed(c, c.bindings.mode_40.cookie, 0, 8);
    Monstey_Seed(c, c.bindings.mode_40.state, state, 4);
}
}
std::vector<Monstey_Case> Monstey_Cases()
{
    std::vector<Monstey_Case> cases;
    for (unsigned occupied = 0; occupied < 2; ++occupied)
    {
        cases.push_back(Monstey_Base("mode0.slot" + std::to_string(occupied), 0, occupied != 0, false));
        for (const auto mode : {0x40000000u, 0x80000000u, 0xc0000000u})
            for (unsigned success = 0; success < 2; ++success)
                cases.push_back(Monstey_Base("mode" + std::to_string(mode) + ".slot" + std::to_string(occupied) +
                    ".success" + std::to_string(success), mode, occupied != 0, success != 0));
    }
    for (const auto mode : {0x40000000u, 0x80000000u, 0xc0000000u})
    {
        auto c = Monstey_Base("full_flags." + std::to_string(mode), mode | 0x00300007u);
        cases.push_back(std::move(c));
    }
    for (unsigned unaligned : {1u, 3u, 7u})
    {
        auto c = Monstey_Base("unaligned_backing." + std::to_string(unaligned));
        c.requests[0].backing += unaligned; Monstey_Expect(c, 0x10018); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("high64_image", 0x80000000u, false, true, 0x8000000000000000ULL);
        cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("manager_zero"); c.requests[0].manager = 0;
        Monstey_Seed(c, 0x190, 0, 8); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("manager_T_wrap_to_zero"); c.requests[0].manager = std::numeric_limits<Monstey_U64>::max() - 0x18f;
        Monstey_Seed(c, 0, 0, 8); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("body_wrap_lazy_BK_frame", 0x40000000u, false, false);
        c.requests[0].body = std::numeric_limits<Monstey_U64>::max() - 0x1f;
        Monstey_Seed(c, c.requests[0].body + 0x50, 0x420, 8); cases.push_back(std::move(c));
    }
    for (const auto kind : {Monstey_Primitive::GuardEnter, Monstey_Primitive::GuardLeave,
        Monstey_Primitive::Fill, Monstey_Primitive::Allocate, Monstey_Primitive::Initialize})
    {
        auto c = Monstey_Base("missing." + std::to_string(static_cast<unsigned>(kind)));
        if (kind == Monstey_Primitive::Allocate) c.requests[0].backing = 0;
        c.missing.push_back(kind);
        Monstey_FaultExpected(c, kind == Monstey_Primitive::GuardEnter || kind == Monstey_Primitive::GuardLeave
            ? Monstey_FaultKind::PrimitiveBinding : Monstey_FaultKind::BackendImport);
        cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("guard_first_full_operand_fault"); c.bytes.resize(0x8037);
        Monstey_FaultExpected(c, Monstey_FaultKind::Memory, 0x8030, 8, true); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("cell_read_before_empty_binding"); c.bindings.imports.enter_cs = 0x50000;
        c.missing.push_back(Monstey_Primitive::GuardEnter);
        Monstey_FaultExpected(c, Monstey_FaultKind::Memory, 0x50000, 8, false); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("T_read_after_guard_prefix"); c.requests[0].manager = 0x50000;
        Monstey_FaultExpected(c, Monstey_FaultKind::Memory, 0x50190, 8, false); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("fifth_full_operand_fault"); c.bytes.resize(0x8057);
        Monstey_FaultExpected(c, Monstey_FaultKind::Memory, 0x8050, 8, false); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("mode0_no_fifth_read", 0, false, false); c.bytes.resize(0x8038);
        cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("unmapped_zero_is_direct_write", 0, false, false, 0x1000);
        Monstey_FaultExpected(c, Monstey_FaultKind::Memory, 0, 4, true); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("first_trap_home_below_view", 0x40000000u, true, true, 0x4009);
        c.requests[0].body = 0x4000;
        Monstey_FaultExpected(c, Monstey_FaultKind::Memory, 0x4008, 8, true); cases.push_back(std::move(c));
    }
    for (unsigned occupied : {0u, 1u})
    {
        auto c = Monstey_Base("Enter_mutates_T." + std::to_string(occupied), 0x40000000u, occupied != 0);
        c.effects.push_back({Monstey_Primitive::GuardEnter, 1, true, Monstey_Action::Write64,
            c.requests[0].manager + 0x190, occupied ? 0u : 1u});
        c.criteria.push_back("PP04"); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("Enter_changes_live_G");
        c.effects.push_back({Monstey_Primitive::GuardEnter, 1, true, Monstey_Action::Write64,
            c.requests[0].body + 0x30, 0x8899aabbccddeeffULL});
        c.criteria.push_back("PP11"); cases.push_back(std::move(c));
    }
    for (unsigned late : {0u, 1u})
    {
        auto c = Monstey_Base("late_fifth." + std::to_string(late), 0x40000000u, false, late == 0);
        c.effects.push_back({Monstey_Primitive::GuardEnter, 1, true, Monstey_Action::Write64, 0x8050, late ? 0x4000u : 0x420u});
        Monstey_Expect(c, late ? 0x10010 : 0); cases.push_back(std::move(c));
    }
    for (const auto offset : {0x3c0u, 0x3c8u, 0x3c1u})
    {
        auto c = Monstey_Base("T_overlaps_P_header." + std::to_string(offset));
        c.requests[0].manager = 0x10010 + offset - 0x190;
        Monstey_Seed(c, c.requests[0].manager + 0x190, 0, 8); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("BK_header_zeroes_live_guard"); c.requests[0].backing = 0x8030 - 0x3c8 - 0x10;
        Monstey_Expect(c, c.requests[0].backing + 0x10); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("BK_region_overlaps_consumed_fifth"); c.requests[0].backing = 0x8030;
        Monstey_Expect(c, 0x8040); cases.push_back(std::move(c));
    }
    for (unsigned cell : {0u, 1u})
    {
        auto c = Monstey_Base("guard_aliases_import_cell." + std::to_string(cell));
        if (cell == 0) c.bindings.imports.enter_cs = 0x8030;
        else c.bindings.imports.leave_cs = 0x8030;
        cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("fifth_is_initialize_cell"); c.bindings.imports.initialize_cs = 0x8050;
        cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("zero_targets_are_called");
        Monstey_Seed(c, c.bindings.imports.enter_cs, 0, 8); Monstey_Seed(c, c.bindings.imports.leave_cs, 0, 8);
        cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("leave_cell_late_contents");
        c.effects.push_back({Monstey_Primitive::GuardEnter, 1, true, Monstey_Action::Write64, c.bindings.imports.leave_cs, 0x76543210fedcba98ULL});
        cases.push_back(std::move(c));
    }
    for (const auto primitive : {Monstey_Primitive::GuardEnter, Monstey_Primitive::Fill, Monstey_Primitive::GuardLeave})
    {
        auto c = Monstey_Base("typed_partial_throw." + std::to_string(static_cast<unsigned>(primitive)));
        c.effects.push_back({primitive, 1, false, Monstey_Action::Write64, 0x6000, 0x9876543210abcdefULL});
        c.effects.push_back({primitive, 1, false, Monstey_Action::Throw, 0, 0xa1b2c3d4});
        Monstey_FaultExpected(c, Monstey_FaultKind::Provider); cases.push_back(std::move(c));
    }
    for (const auto action : {Monstey_Action::ClearCurrent, Monstey_Action::ReplaceCurrent, Monstey_Action::ReplaceLeave})
    {
        auto c = Monstey_Base("current_target_action." + std::to_string(static_cast<unsigned>(action)));
        c.effects.push_back({Monstey_Primitive::GuardEnter, 1, true, action, 0, 0});
        cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("enter_retirement_changes_fifth"); c.retire_enter = true; c.retire_address = 0x8050; c.retire_value = 0x420;
        c.effects.push_back({Monstey_Primitive::GuardEnter, 1, true, Monstey_Action::ClearCurrent, 0, 0});
        Monstey_Expect(c, 0); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("real_reentry_persistent_target");
        c.nested = Monstey_Request{0x6000, 0x12340000abcdef99ULL, 0x18000, 0xa000, 0x80000000u};
        Monstey_Seed(c, 0x6190, 0, 8); Monstey_Seed(c, 0xa050, 0x420, 8);
        c.effects.push_back({Monstey_Primitive::GuardEnter, 1, true, Monstey_Action::Reenter, 0, 0});
        cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("two_entries_same_value_captured_provider", 0x40000000u, false, false);
        c.requests.push_back(c.requests.front()); c.expected_raw.push_back(c.expected_raw.front()); cases.push_back(std::move(c));
    }
    for (Monstey_U32 state : {0u, 2u})
    {
        auto c = Monstey_Base("real_initializer_state." + std::to_string(state));
        Monstey_InitializePath(c, state); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("real_initializer_sleep_progress"); Monstey_InitializePath(c, 0xffffffffu);
        c.effects.push_back({Monstey_Primitive::Sleep, 2, true, Monstey_Action::Write32, c.bindings.mode_40.state, 2});
        cases.push_back(std::move(c));
    }
    for (const auto primitive : {Monstey_Primitive::SystemInfo, Monstey_Primitive::Random,
        Monstey_Primitive::BackendEnter, Monstey_Primitive::BackendLeave,
        Monstey_Primitive::CompareExchange, Monstey_Primitive::Exchange, Monstey_Primitive::Sleep})
    {
        auto c = Monstey_Base("initializer_missing." + std::to_string(static_cast<unsigned>(primitive)));
        Monstey_InitializePath(c, primitive == Monstey_Primitive::Sleep ? 0xffffffffu :
            primitive == Monstey_Primitive::CompareExchange || primitive == Monstey_Primitive::Exchange ? 0u : 2u);
        c.missing.push_back(primitive);
        Monstey_FaultExpected(c, primitive == Monstey_Primitive::CompareExchange ? Monstey_FaultKind::CompareBinding :
            primitive == Monstey_Primitive::Exchange ? Monstey_FaultKind::ExchangeBinding : Monstey_FaultKind::BackendImport);
        cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("bounded_higher_raw_consumer", 0, false, false);
        c.consume_higher = true; c.criteria.push_back("PP17"); cases.push_back(std::move(c));
    }
    for (unsigned home = 0; home < 3; ++home)
    {
        auto c = Monstey_Base("bounded_higher_home_fault." + std::to_string(home), 0, false, false);
        c.consume_higher = true; c.criteria.push_back("PP17");
        const Monstey_U64 address = 0x8038 + 8 * home; c.bytes.resize(static_cast<std::size_t>(address + 7));
        Monstey_FaultExpected(c, Monstey_FaultKind::Memory, address, 8, true); cases.push_back(std::move(c));
    }
    for (const auto pointer : {Monstey_U64{0}, std::numeric_limits<Monstey_U64>::max(), Monstey_U64{0x10000}})
    {
        auto c = Monstey_Base("real_allocate_return." + std::to_string(pointer));
        c.requests[0].backing = 0; c.allocate_pointer = pointer;
        Monstey_Expect(c, pointer == 0x10000 ? 0x10010 : 0); cases.push_back(std::move(c));
    }
    for (unsigned k = static_cast<unsigned>(Monstey_Primitive::Fill);
        k <= static_cast<unsigned>(Monstey_Primitive::CompareExchange); ++k)
    {
        auto c = Monstey_Base("unused_missing_backend." + std::to_string(k), 0x40000000u, false, false);
        c.missing.push_back(static_cast<Monstey_Primitive>(k)); cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("unused_allocate_cell_unmapped", 0x40000000u, false, false);
        c.bindings.imports.allocate = 0x50000; cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("mode0_low_nonmode_flags", 0x00300007u, false, false);
        cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("body_zero_lazy_BK_frame", 0x40000000u, false, false);
        c.requests[0].body = 0; Monstey_Seed(c, 0x50, 0x420, 8); cases.push_back(std::move(c));
    }
    for (const auto action : {Monstey_Action::ReplaceCurrent, Monstey_Action::ClearCurrent})
    {
        auto c = Monstey_Base("real_initializer_current_target." + std::to_string(static_cast<unsigned>(action)));
        Monstey_InitializePath(c, 0);
        c.effects.push_back({Monstey_Primitive::Initialize, 1, true, action, 0, 0});
        if (action == Monstey_Action::ClearCurrent) Monstey_FaultExpected(c, Monstey_FaultKind::BackendImport);
        cases.push_back(std::move(c));
    }
    {
        auto c = Monstey_Base("enter_retirement_changes_T_before_read", 0x40000000u, false, true, 0x1000);
        c.retire_enter = true; c.retire_address = c.requests[0].manager + 0x190; c.retire_value = 1;
        c.effects.push_back({Monstey_Primitive::GuardEnter, 1, true, Monstey_Action::ClearCurrent, 0, 0});
        Monstey_FaultExpected(c, Monstey_FaultKind::Memory, 0, 4, true); cases.push_back(std::move(c));
    }
    for (const auto value : {Monstey_U64{0x8000000000000001ULL}, Monstey_U64{0xffffffffffffffffULL}})
    {
        auto c = Monstey_Base("all_full64_leave_words." + std::to_string(value));
        c.leave_out.raw = {value ^ 0x1111, value}; c.leave_out.post_leave_r8 = value ^ 0x2222;
        c.leave_out.post_leave_r9 = value ^ 0x4444; Monstey_Expect(c, 0x10010); cases.push_back(std::move(c));
    }
    for (auto &c : cases)
    {
        if (c.id.find("late_fifth") != std::string::npos || c.id.find("fifth") != std::string::npos) c.criteria.push_back("PP07");
        if (c.id.find("home") != std::string::npos || c.id.find("mode0") != std::string::npos || c.id.find("zero") != std::string::npos) c.criteria.push_back("PP05");
        if (c.id.find("T_") != std::string::npos || c.id.find("header") != std::string::npos) { c.criteria.push_back("PP04"); c.criteria.push_back("PP10"); }
        if (c.id.find("alias") != std::string::npos || c.id.find("overlap") != std::string::npos) c.criteria.push_back("PP14");
        if (c.id.find("missing") != std::string::npos || c.id.find("fault") != std::string::npos) c.criteria.push_back("PP13");
        if (c.id.find("current_target") != std::string::npos || c.id.find("retirement") != std::string::npos || c.id.find("reentry") != std::string::npos || c.id.find("two_entries") != std::string::npos) c.criteria.push_back("PP15");
        if (c.id.find("throw") != std::string::npos) c.criteria.push_back("PP16");
    }
    return cases;
}
}
