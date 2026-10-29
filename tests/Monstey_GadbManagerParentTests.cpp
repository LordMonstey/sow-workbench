#include "Monstey_GadbManagerParent.hpp"
#include "Monstey_Core.hpp"
#include "Monstey_Json.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <vector>

namespace
{
using namespace Monstey;
using Monstey_U64 = std::uint64_t;
using Monstey_U32 = std::uint32_t;
using Monstey_J = Monstey_Json;
std::size_t Monstey_Checks = 0, Monstey_Images = 0, Monstey_ImageBytes = 0;
std::size_t Monstey_Normals = 0, Monstey_Faults = 0;
Monstey_J Monstey_Cases = Monstey_J::array();
Monstey_J Monstey_NormalWords = Monstey_J::array();
Monstey_J Monstey_FaultSignatures = Monstey_J::array();
std::string Monstey_CurrentCase;

void Monstey_Check(bool value, const char *message)
{
    ++Monstey_Checks;
    if (!value)
        throw std::runtime_error(message);
}

std::string Monstey_Word(Monstey_U64 value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}

void Monstey_RecordNormal(const Monstey_GadbManagerParentResult &result)
{
    ++Monstey_Normals;
    Monstey_NormalWords.as_array().push_back(Monstey_J::object({{"case", Monstey_CurrentCase},
                                                                {"rax", Monstey_Word(result.raw.rax)},
                                                                {"rdx", Monstey_Word(result.raw.rdx)},
                                                                {"r8", Monstey_Word(result.post_leave_r8)},
                                                                {"r9", Monstey_Word(result.post_leave_r9)}}));
}

void Monstey_RecordFault(Monstey_J signature)
{
    ++Monstey_Faults;
    signature["case"] = Monstey_CurrentCase;
    Monstey_FaultSignatures.as_array().push_back(std::move(signature));
}

void Monstey_Put(std::vector<std::uint8_t> &image, Monstey_U64 base, Monstey_U64 address, Monstey_U64 value,
                 std::size_t width = 8)
{
    if (address < base || address - base > image.size() || width > image.size() - std::size_t(address - base))
        throw std::runtime_error("Owner fixture write outside its stable image");
    for (std::size_t i = 0; i < width; ++i)
        image[std::size_t(address - base) + i] = static_cast<std::uint8_t>(value >> (8u * i));
}

Monstey_U64 Monstey_Get(const std::vector<std::uint8_t> &image, Monstey_U64 base, Monstey_U64 address,
                        std::size_t width = 8)
{
    if (address < base || address - base > image.size() || width > image.size() - std::size_t(address - base))
        throw std::runtime_error("Owner fixture read outside its stable image");
    Monstey_U64 value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= Monstey_U64(image[std::size_t(address - base) + i]) << (8u * i);
    return value;
}

struct Monstey_Event
{
    std::string kind;
    Monstey_GadbManagerBackendImportSite site{};
    Monstey_U64 a = 0, b = 0;
};

struct Monstey_Fixture
{
    Monstey_U64 base = 0, manager = 0x4000, frame = 0x8000, backing = 0x10000;
    Monstey_U64 x8 = 0xE8123456789ABCDEULL, x9 = backing, count = 0x2000;
    Monstey_U32 flags = 0x40000010u;
    std::size_t viewBegin = 0, viewLength = 0x30000;
    std::vector<std::uint8_t> image = std::vector<std::uint8_t>(0x30000, 0xA5);
    Monstey_GadbManagerBackendBindings bindings;
    Monstey_GadbManagerBackendServices backend;
    Monstey_GadbManagerEnterProjectionServices enter;
    Monstey_GadbManagerLeaveProjectionServices leave;
    Monstey_GadbManagerEnterProjectionResult entered{0xD2123456789ABCDEULL, 0xE8123456789ABCDFULL,
                                                     0xE9123456789ABCDEULL};
    Monstey_GadbManagerLeaveProjectionResult left{
        {0xF1123456789ABCDEULL, 0xF2123456789ABCDEULL}, 0xF8123456789ABCDEULL, 0xF9123456789ABCDEULL};
    std::vector<Monstey_GadbManagerEnterProjectionCall> enterCalls;
    std::vector<Monstey_GadbManagerSyncCall> leaveCalls;
    std::vector<Monstey_Event> events;
    std::function<void(std::string_view)> hook;
    Monstey_U64 allocation = 0, allocationRdx = 0xB2123456789ABCDEULL;
    Monstey_U64 fillRdx = 0xA2123456789ABCDEULL, backendLeaveRdx = 0xB3123456789ABCDEULL;

    Monstey_GadbBinderMemory Memory()
    {
        return {base + viewBegin, std::span(image).subspan(viewBegin, viewLength)};
    }
    Monstey_U64 Table() const
    {
        return manager + 0x190u;
    }
    Monstey_U64 Guard() const
    {
        return frame + 0x30u;
    }
    Monstey_U64 Lock() const
    {
        return manager + 0x208u;
    }
    void Put(Monstey_U64 a, Monstey_U64 v, std::size_t n = 8)
    {
        Monstey_Put(image, base, a, v, n);
    }
    Monstey_U64 Get(Monstey_U64 a, std::size_t n = 8) const
    {
        return Monstey_Get(image, base, a, n);
    }
    Monstey_GadbManagerBackendModeCells &Cells(unsigned mode)
    {
        return mode == 0 ? bindings.mode_40 : mode == 1 ? bindings.mode_80 : bindings.mode_c0;
    }
    Monstey_U64 Tail(unsigned mode) const
    {
        return mode == 0   ? bindings.placement.tail_40
               : mode == 1 ? bindings.placement.tail_80
                           : bindings.placement.tail_c0;
    }
    void Event(std::string kind, Monstey_GadbManagerBackendImportSite site = {}, Monstey_U64 a = 0, Monstey_U64 b = 0)
    {
        events.push_back({kind, site, a, b});
        if (hook)
            hook(kind);
    }
    void RecordEnter(const Monstey_GadbManagerEnterProjectionCall &call)
    {
        enterCalls.push_back(call);
        Event("Enter");
    }
    void RecordLeave(const Monstey_GadbManagerSyncCall &call)
    {
        leaveCalls.push_back(call);
        Event("Leave");
    }
    Monstey_Fixture()
    {
        bindings.imports = {0x100, 0x108, 0x110, 0x118, 0x120, 0x128, 0x130, 0x138};
        bindings.placement = {0x1800, 0x1808, 0x1810, 0x1900};
        bindings.mode_40 = {0x1000, 0x1008, 0x1010, 0x1018, 0x1020, 0x1028, 0x1030, 0x1060, 0x1070};
        bindings.mode_80 = {0x1100, 0x1108, 0x1110, 0x1118, 0x1120, 0x1128, 0x1130, 0x1160, 0x1170};
        bindings.mode_c0 = {0x1200, 0x1208, 0x1210, 0x1218, 0x1220, 0x1228, 0x1230, 0x1260, 0x1270};
        for (unsigned i = 0; i < 8; ++i)
            Put(0x100u + 8u * i, 0x770000u + i);
        for (unsigned i = 0; i < 3; ++i)
        {
            Put(Cells(i).cookie, 0x12345678);
            Put(Cells(i).page, 0x1000);
            Put(Cells(i).granularity, 0x1000);
            Put(Cells(i).flags, 7, 4);
            Put(Cells(i).state, 1, 4);
            Put(Tail(i), 0xDDAABBCC11223344ULL);
        }
        Put(Table(), 0);
        Put(frame + 0x50u, count);
        enter.enter = [this](Monstey_GadbBinderMemory, const Monstey_GadbManagerEnterProjectionCall &call)
        {
            RecordEnter(call);
            return entered;
        };
        leave.leave = [this](Monstey_GadbBinderMemory, const Monstey_GadbManagerSyncCall &call)
        {
            RecordLeave(call);
            return left;
        };
        backend.fill = [this](Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendFillCall &call)
        {
            Event("Fill", call.site, call.destination, call.count);
            Monstey_Check(call.value == 0, "Actual lower fill value differs");
            for (Monstey_U64 i = 0; i < call.count; ++i)
                Put(call.destination + i, 0, 1);
            return Monstey_GadbManagerBackendRdxResult{fillRdx};
        };
        backend.allocate = [this](Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendAllocateCall &call)
        {
            Event("Allocate", call.site, call.address, call.size);
            Monstey_Check(call.address == 0 && call.allocation_type == 0x3000u,
                          "Actual lower allocation arguments differ");
            return Monstey_GadbBufferedResult{allocation, allocationRdx};
        };
        backend.initialize_cs = [this](Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendInitializeCsCall &call)
        {
            Event("InitializeCs", call.site, call.critical_section, call.spin);
            Monstey_Check(call.spin == 0x80000FA0u, "Actual lower CS spin differs");
            return Monstey_GadbManagerBackendRdxResult{0x8765432101234567ULL};
        };
        backend.enter_cs = [this](Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendSyncCall &call)
        { Event("BackendEnter", call.site, call.critical_section); };
        backend.leave_cs = [this](Monstey_GadbBinderMemory, const Monstey_GadbManagerBackendSyncCall &call)
        {
            Event("BackendLeave", call.site, call.critical_section);
            return Monstey_GadbManagerBackendRdxResult{backendLeaveRdx};
        };
    }
    Monstey_GadbManagerParentResult Run()
    {
        return Monstey_GadbCreateManagerBackendForManager(Memory(), manager, flags, x8, x9, frame, enter, leave,
                                                          backend, bindings);
    }
};

void Monstey_Image(const Monstey_Fixture &fixture, const std::vector<std::uint8_t> &expected)
{
    ++Monstey_Images;
    Monstey_ImageBytes += expected.size();
    Monstey_Check(fixture.image == expected, "Full owned image mismatch");
}

void Monstey_ExpectedGuard(const Monstey_Fixture &f, std::vector<std::uint8_t> &expected)
{
    Monstey_Put(expected, f.base, f.Guard(), f.Lock());
}

void Monstey_ExpectedTrap(const Monstey_Fixture &f, std::vector<std::uint8_t> &expected, Monstey_U64 rdx)
{
    Monstey_Put(expected, f.base, f.frame + 8u, rdx);
    Monstey_Put(expected, f.base, f.frame + 0x10u, f.entered.r8);
    Monstey_Put(expected, f.base, f.frame + 0x18u, f.entered.r9);
    Monstey_Put(expected, f.base, 0, 0, 4);
}

Monstey_U64 Monstey_ExpectedPlacement(const Monstey_Fixture &f, std::vector<std::uint8_t> &expected, unsigned mode,
                                      Monstey_U64 backing, Monstey_U64 span)
{
    const auto object = (backing + 23u) & ~Monstey_U64{7};
    const auto put = [&](Monstey_U64 a, Monstey_U64 v, std::size_t n = 8) { Monstey_Put(expected, f.base, a, v, n); };
    const auto get = [&](Monstey_U64 a, std::size_t n = 8) { return Monstey_Get(expected, f.base, a, n); };
    for (Monstey_U64 i = 0; i < 0x3D8u; ++i)
        put(object + i, 0, 1);
    const auto &cells = mode == 0 ? f.bindings.mode_40 : mode == 1 ? f.bindings.mode_80 : f.bindings.mode_c0;
    put(object - 8u, 0x3DB);
    put(object + 0x18u, backing);
    put(object + 0x3A0u, backing);
    put(object + 0x360u, span);
    put(object + 0x358u, span);
    put(object + 0x3A8u, span);
    put(object + 0x38u, ~Monstey_U64{0});
    put(object + 0x40u, get(cells.cookie));
    put(object + 0x370u, get(cells.flags, 4) | 4u, 4);
    for (unsigned i = 0; i < 32; ++i)
    {
        const auto header = object + 72u + 16u * i;
        put(header + 24u, header);
        put(header + 16u, header);
    }
    const auto segment = object + 0x3C8u;
    const auto remaining = span - (segment - backing) - 0x48u;
    put(object + 0x28u, segment);
    put(object + 0x10u, remaining);
    put(segment + 8u, remaining | 1u);
    put(backing + span - 0x40u, 0x48);
    put(object + 0x30u, get(f.Tail(mode)));
    put(object + 0x370u, get(object + 0x370u, 4) | 2u | f.flags, 4);
    put(object + 0x3B8u, 8, 4);
    return object;
}

void Monstey_CheckResult(const Monstey_Fixture &f, const Monstey_GadbManagerParentResult &result, Monstey_U64 selected,
                         Monstey_GadbBufferedResult incoming, const std::vector<std::uint8_t> &expected)
{
    Monstey_RecordNormal(result);
    Monstey_Check(result.raw.rax == selected && result.raw.rdx == f.left.raw.rdx &&
                      result.post_leave_r8 == f.left.post_leave_r8 && result.post_leave_r9 == f.left.post_leave_r9,
                  "Selected backend or actual full64 Leave egress differs");
    Monstey_Check(f.enterCalls.size() == std::size_t{1} && f.leaveCalls.size() == std::size_t{1},
                  "Guard/cleanup primitive count differs");
    const auto &enter = f.enterCalls.front();
    Monstey_Check(enter.base.native_site == 0x14114BA9FULL && enter.base.slot_identity == f.bindings.imports.enter_cs &&
                      enter.base.critical_section == f.Lock() && enter.base.incoming.rax == f.frame + 0x28u &&
                      enter.base.incoming.rdx == f.Lock() && enter.incoming_r8 == f.x8 && enter.incoming_r9 == f.x9,
                  "Actual Enter DTO input widths/frame/cells differ");
    const auto &leave = f.leaveCalls.front();
    Monstey_Check(leave.native_site == 0x14114BA88ULL && leave.slot_identity == f.bindings.imports.leave_cs &&
                      leave.critical_section == Monstey_Get(expected, f.base, f.Guard()) &&
                      leave.incoming.rax == incoming.rax && leave.incoming.rdx == incoming.rdx,
                  "Actual Leave DTO lock, cells or incoming Raw differs");
    Monstey_Image(f, expected);
}

void Monstey_Case(const std::string &id, std::initializer_list<const char *> criteria,
                  const std::function<void()> &body)
{
    Monstey_CurrentCase = id;
    const auto before = Monstey_Checks;
    body();
    Monstey_J names = Monstey_J::array();
    for (const auto *name : criteria)
        names.as_array().push_back(name);
    Monstey_Cases.as_array().push_back(
        Monstey_J::object({{"id", id}, {"criteria", names}, {"checks", Monstey_Checks - before}}));
}

void Monstey_MemoryFault(Monstey_Fixture &f, Monstey_U64 address, std::size_t width, bool writing,
                         const std::vector<std::uint8_t> &expected)
{
    bool caught = false;
    try
    {
        static_cast<void>(f.Run());
    }
    catch (const Monstey_GadbBufferedMemoryFault &fault)
    {
        caught = true;
        Monstey_RecordFault(Monstey_J::object({{"kind", "memory"},
                                               {"address", Monstey_Word(fault.address)},
                                               {"width", fault.width},
                                               {"writing", fault.writing}}));
        Monstey_Check(fault.address == address && fault.width == width && fault.writing == writing,
                      "Exact ordinary memory fault tuple differs");
    }
    Monstey_Check(caught, "Expected actual memory fault was not reached");
    Monstey_Check(f.leaveCalls.empty(), "Fault invented cleanup or normal return");
    Monstey_Image(f, expected);
}

void Monstey_Routes()
{
    const std::array<Monstey_U32, 4> masks{0u, 0x40000000u, 0x80000000u, 0xC0000000u};
    const std::array<Monstey_U64, 3> fillSites{0x141175756ULL, 0x141173BDEULL, 0x141176CEEULL};
    for (unsigned mode = 0; mode < masks.size(); ++mode)
        for (unsigned occupied = 0; occupied < 2; ++occupied)
            for (unsigned success = 0; success < (mode == 0 ? 1u : 2u); ++success)
                Monstey_Case(
                    "route_" + std::to_string(mode) + "_" + std::to_string(occupied) + "_" + std::to_string(success),
                    {"PP02", "PP03", "PP05", "PP06", "PP08", "PP09", "PP10", "PP11", "PP12", "PP13"},
                    [&]
                    {
                        Monstey_Fixture f;
                        f.flags = masks[mode] | 0x00300010u;
                        f.count = success ? 0x2000u : 0x420u;
                        f.Put(f.frame + 0x50u, f.count);
                        f.Put(f.Table(), occupied ? 0xBEEF : 0);
                        auto expected = f.image;
                        Monstey_ExpectedGuard(f, expected);
                        if (occupied)
                            Monstey_ExpectedTrap(f, expected, f.entered.rdx);
                        if (mode == 0)
                            Monstey_ExpectedTrap(f, expected, 0xC0000000u);
                        Monstey_U64 selected = 0;
                        if (success)
                        {
                            selected = Monstey_ExpectedPlacement(f, expected, mode - 1u, f.x9, f.count);
                            Monstey_Put(expected, f.base, f.Table(), selected);
                            Monstey_Put(expected, f.base, selected + 0x3C0u, f.Table());
                            Monstey_Put(expected, f.base, selected + 0x3C8u, 0);
                        }
                        const auto result = f.Run();
                        const Monstey_GadbBufferedResult incoming = mode == 0
                                                                        ? Monstey_GadbBufferedResult{0, 0xC0000000u}
                                                                    : success ? Monstey_GadbBufferedResult{f.Table(), 0}
                                                                              : Monstey_GadbBufferedResult{0, f.flags};
                        Monstey_CheckResult(f, result, selected, incoming, expected);
                        Monstey_Check(f.events.size() == (success ? std::size_t{4} : std::size_t{2}),
                                      "Concrete path primitive order/count differs");
                        if (success)
                        {
                            Monstey_Check(f.events[1].kind == "Fill" && f.events[2].kind == "InitializeCs" &&
                                              f.events[1].site.call_site == fillSites[mode - 1u] &&
                                              f.events[1].site.native_site == 0x1408C4EE0ULL &&
                                              f.events[1].site.body_rsp == f.frame - 0x438u &&
                                              f.events[2].site.body_rsp == f.frame - 0x430u &&
                                              f.events[1].a == selected && f.events[1].b == 0x3D8u,
                                          "Actual constructor/placement/thunk frame or mode differs");
                        }
                    });
}

void Monstey_LiveInputs()
{
    for (unsigned mode = 0; mode < 3; ++mode)
        for (unsigned success = 0; success < 2; ++success)
            for (unsigned alias = 0; alias < 2; ++alias)
                Monstey_Case("late_fifth_" + std::to_string(mode) + "_" + std::to_string(success) + "_" +
                                 std::to_string(alias),
                             {"PP02", "PP03", "PP07", "PP08", "PP13", "PP14"},
                             [&]
                             {
                                 Monstey_Fixture f;
                                 f.flags = std::array<Monstey_U32, 3>{0x40000015u, 0x80000015u, 0xC0000015u}[mode];
                                 const Monstey_U64 late = success ? 0x2000u : 0x420u;
                                 f.Put(f.frame + 0x50u, success ? 0x420u : 0x2000u);
                                 if (alias)
                                     f.bindings.imports.enter_cs = f.frame + 0x50u;
                                 f.hook = [&](std::string_view event)
                                 {
                                     if (event == "Enter")
                                         f.Put(f.frame + 0x50u, late);
                                 };
                                 auto expected = f.image;
                                 Monstey_ExpectedGuard(f, expected);
                                 Monstey_Put(expected, f.base, f.frame + 0x50u, late);
                                 Monstey_U64 selected = 0;
                                 if (success)
                                 {
                                     selected = Monstey_ExpectedPlacement(f, expected, mode, f.x9, late);
                                     Monstey_Put(expected, f.base, f.Table(), selected);
                                     Monstey_Put(expected, f.base, selected + 0x3C0u, f.Table());
                                     Monstey_Put(expected, f.base, selected + 0x3C8u, 0);
                                 }
                                 const auto result = f.Run();
                                 Monstey_CheckResult(f, result, selected,
                                                     success ? Monstey_GadbBufferedResult{f.Table(), 0}
                                                             : Monstey_GadbBufferedResult{0, f.flags},
                                                     expected);
                                 Monstey_Check(f.enterCalls[0].incoming_r9 != f.entered.r9,
                                               "Backing witness accidentally equals post-Enter R9");
                                 if (alias)
                                     Monstey_Check(f.enterCalls[0].base.target_identity == (success ? 0x420u : 0x2000u),
                                                   "Enter must read cell before changing the aliased fifth word");
                             });
    for (unsigned before = 0; before < 2; ++before)
        Monstey_Case("live_slot_after_enter_" + std::to_string(before), {"PP04", "PP05", "PP13"},
                     [&]
                     {
                         Monstey_Fixture f;
                         f.Put(f.frame + 0x50u, 0x420);
                         f.Put(f.Table(), before);
                         f.hook = [&](std::string_view event)
                         {
                             if (event == "Enter")
                                 f.Put(f.Table(), 1u - before);
                         };
                         auto expected = f.image;
                         Monstey_ExpectedGuard(f, expected);
                         Monstey_Put(expected, f.base, f.Table(), 1u - before);
                         if (!before)
                             Monstey_ExpectedTrap(f, expected, f.entered.rdx);
                         Monstey_CheckResult(f, f.Run(), 0, {0, f.flags}, expected);
                     });
    Monstey_Case("mode_zero_does_not_read_fifth", {"PP02", "PP05", "PP07", "PP13"},
                 []
                 {
                     Monstey_Fixture f;
                     f.flags = 0x00300015u;
                     f.viewLength = static_cast<std::size_t>(f.frame + 0x38u);
                     auto expected = f.image;
                     Monstey_ExpectedGuard(f, expected);
                     Monstey_ExpectedTrap(f, expected, 0xC0000000u);
                     Monstey_CheckResult(f, f.Run(), 0, {0, 0xC0000000u}, expected);
                 });
    for (unsigned mode = 0; mode < 3; ++mode)
        Monstey_Case("real_initializer_rdx_" + std::to_string(mode), {"PP03", "PP08", "PP11", "PP12", "PP13"},
                     [&]
                     {
                         Monstey_Fixture f;
                         f.flags = std::array<Monstey_U32, 3>{0x40000001u, 0x80000001u, 0xC0000001u}[mode];
                         f.Put(f.frame + 0x50u, 0x420);
                         f.Put(f.Cells(mode).cookie, 0);
                         f.hook = [&](std::string_view event)
                         {
                             if (event == "BackendEnter")
                                 f.Put(f.Cells(mode).cookie, 1);
                         };
                         auto expected = f.image;
                         Monstey_ExpectedGuard(f, expected);
                         Monstey_Put(expected, f.base, f.Cells(mode).cookie, 1);
                         Monstey_CheckResult(f, f.Run(), 0, {0, f.backendLeaveRdx}, expected);
                         Monstey_Check(f.events.size() == std::size_t{4} && f.events[1].kind == "BackendEnter" &&
                                           f.events[2].kind == "BackendLeave" &&
                                           f.events[1].site.cell_identity == f.enterCalls[0].base.slot_identity &&
                                           f.events[2].site.cell_identity == f.leaveCalls[0].slot_identity,
                                       "Guard and BK must share identities while invoking distinct actual primitives");
                     });
}

void Monstey_FaultPrefixes()
{
    for (unsigned overflow = 0; overflow < 2; ++overflow)
        Monstey_Case("guard_full_operand_" + std::to_string(overflow), {"PP02", "PP03", "PP13"},
                     [&]
                     {
                         Monstey_Fixture f;
                         f.frame = overflow ? std::numeric_limits<Monstey_U64>::max() - 0x33u : 0x2FFCDu;
                         const auto expected = f.image;
                         Monstey_MemoryFault(f, f.Guard(), 8, true, expected);
                         Monstey_Check(f.enterCalls.empty(), "Guard fault must precede Enter");
                     });
    Monstey_Case("table_first_read_after_enter", {"PP04", "PP13"},
                 []
                 {
                     Monstey_Fixture f;
                     f.manager = 0x2FFF9u - 0x190u;
                     f.hook = [&](std::string_view event)
                     {
                         if (event == "Enter")
                             f.Put(0x2300, 0xCAFE);
                     };
                     auto expected = f.image;
                     Monstey_ExpectedGuard(f, expected);
                     Monstey_Put(expected, f.base, 0x2300, 0xCAFE);
                     Monstey_MemoryFault(f, 0x2FFF9, 8, false, expected);
                     Monstey_Check(f.enterCalls.size() == std::size_t{1}, "Slot fault lost preceding Enter");
                 });
    for (unsigned enter = 0; enter < 2; ++enter)
        Monstey_Case("import_read_before_missing_" + std::to_string(enter), {"PP03", "PP11", "PP13"},
                     [&]
                     {
                         Monstey_Fixture f;
                         f.flags = 0;
                         auto expected = f.image;
                         Monstey_ExpectedGuard(f, expected);
                         if (enter)
                         {
                             f.bindings.imports.enter_cs = 0x2FFF9;
                             f.enter.enter = {};
                         }
                         else
                         {
                             f.bindings.imports.leave_cs = 0x2FFF9;
                             f.leave.leave = {};
                             Monstey_ExpectedTrap(f, expected, 0xC0000000u);
                         }
                         Monstey_MemoryFault(f, 0x2FFF9, 8, false, expected);
                     });
    for (unsigned occupied = 0; occupied < 2; ++occupied)
        Monstey_Case("trap_first_home_" + std::to_string(occupied), {"PP05", "PP13"},
                     [&]
                     {
                         Monstey_Fixture f;
                         f.flags = 0;
                         f.manager = f.frame + 0x3000u;
                         f.bindings.imports.enter_cs = f.frame + 0x100u;
                         f.Put(f.Table(), occupied);
                         f.Put(f.bindings.imports.enter_cs, 0x7777);
                         f.viewBegin = static_cast<std::size_t>(f.frame + 0x10u);
                         f.viewLength = f.image.size() - f.viewBegin;
                         auto expected = f.image;
                         Monstey_ExpectedGuard(f, expected);
                         Monstey_MemoryFault(f, f.frame + 8u, 8, true, expected);
                     });
    for (unsigned occupied = 0; occupied < 2; ++occupied)
        Monstey_Case("trap_direct_zero_store_" + std::to_string(occupied), {"PP05", "PP13"},
                     [&]
                     {
                         Monstey_Fixture f;
                         f.flags = 0;
                         f.Put(f.Table(), occupied);
                         f.viewBegin = 1;
                         f.viewLength = f.image.size() - 1;
                         auto expected = f.image;
                         Monstey_ExpectedGuard(f, expected);
                         Monstey_Put(expected, f.base, f.frame + 8u,
                                     occupied ? f.entered.rdx : Monstey_U64{0xC0000000});
                         Monstey_Put(expected, f.base, f.frame + 0x10u, f.entered.r8);
                         Monstey_Put(expected, f.base, f.frame + 0x18u, f.entered.r9);
                         Monstey_MemoryFault(f, 0, 4, true, expected);
                     });
    for (unsigned mode = 0; mode < 3; ++mode)
        Monstey_Case("late_fifth_full_operand_" + std::to_string(mode), {"PP07", "PP13"},
                     [&]
                     {
                         Monstey_Fixture f;
                         f.flags = std::array<Monstey_U32, 3>{0x40000000u, 0x80000000u, 0xC0000000u}[mode];
                         f.viewLength = static_cast<std::size_t>(f.frame + 0x57u);
                         auto expected = f.image;
                         Monstey_ExpectedGuard(f, expected);
                         Monstey_MemoryFault(f, f.frame + 0x50u, 8, false, expected);
                     });
    for (unsigned enter = 0; enter < 2; ++enter)
        for (unsigned zero = 0; zero < 2; ++zero)
            Monstey_Case("missing_sync_" + std::to_string(enter) + "_" + std::to_string(zero), {"PP03", "PP11", "PP13"},
                         [&]
                         {
                             Monstey_Fixture f;
                             f.flags = 0;
                             const auto cell = enter ? f.bindings.imports.enter_cs : f.bindings.imports.leave_cs;
                             f.Put(cell, zero ? 0 : 0xCC88776655443322ULL);
                             if (enter)
                                 f.enter.enter = {};
                             else
                                 f.leave.leave = {};
                             auto expected = f.image;
                             Monstey_ExpectedGuard(f, expected);
                             if (!enter)
                                 Monstey_ExpectedTrap(f, expected, 0xC0000000u);
                             bool caught = false;
                             try
                             {
                                 static_cast<void>(f.Run());
                             }
                             catch (const Monstey_GadbManagerPrimitiveBoundaryFault &fault)
                             {
                                 caught = true;
                                 Monstey_RecordFault(Monstey_J::object({{"kind", "missing_sync"},
                                                                        {"site", Monstey_Word(fault.native_site)},
                                                                        {"slot", Monstey_Word(fault.slot_identity)},
                                                                        {"target", Monstey_Word(fault.target_identity)},
                                                                        {"argument", Monstey_Word(fault.argument)}}));
                                 Monstey_Check(fault.kind ==
                                                       Monstey_GadbManagerPrimitiveBoundaryKind::missing_sync_service &&
                                                   fault.native_site == (enter ? 0x14114BA9FULL : 0x14114BA88ULL) &&
                                                   fault.address == cell && fault.width == std::size_t{8} &&
                                                   fault.slot_identity == cell &&
                                                   fault.target_identity == (zero ? 0 : 0xCC88776655443322ULL) &&
                                                   fault.argument == f.Lock(),
                                               "Missing current primitive fault fields differ");
                             }
                             Monstey_Check(caught && f.leaveCalls.empty(),
                                           "Missing primitive was preflighted or cleanup invented");
                             Monstey_Image(f, expected);
                         });
    for (unsigned mode = 0; mode < 3; ++mode)
        Monstey_Case(
            "real_BK_missing_fill_" + std::to_string(mode), {"PP07", "PP08", "PP13"},
            [&]
            {
                Monstey_Fixture f;
                f.flags = std::array<Monstey_U32, 3>{0x40000011u, 0x80000011u, 0xC0000011u}[mode];
                f.x9 = 0;
                f.backend.fill = {};
                auto expected = f.image;
                Monstey_ExpectedGuard(f, expected);
                bool caught = false;
                try
                {
                    static_cast<void>(f.Run());
                }
                catch (const Monstey_GadbManagerBackendImportFault &fault)
                {
                    caught = true;
                    Monstey_RecordFault(Monstey_J::object({{"kind", "BK_import"},
                                                           {"site", Monstey_Word(fault.site.native_site)},
                                                           {"call_site", Monstey_Word(fault.site.call_site)},
                                                           {"body", Monstey_Word(fault.site.body_rsp)}}));
                    const std::array<Monstey_U64, 3> sites{0x141175FBBULL, 0x141174DEFULL, 0x141177EFFULL};
                    Monstey_Check(fault.kind == Monstey_GadbManagerBackendImport::fill &&
                                      fault.site.native_site == 0x1408C4EE0ULL && fault.site.call_site == sites[mode] &&
                                      fault.site.body_rsp == f.frame - 0x408u &&
                                      fault.site.cell_identity == f.bindings.imports.fill,
                                  "Parent did not enter the actual concrete BK frame/site");
                }
                Monstey_Check(caught && f.leaveCalls.empty(), "Real BK fault was replaced by callback/null result");
                Monstey_Image(f, expected);
            });
    for (unsigned mode = 0; mode < 3; ++mode)
        Monstey_Case("allocation_null_high_rdx_" + std::to_string(mode), {"PP07", "PP08", "PP11", "PP13"},
                     [&]
                     {
                         Monstey_Fixture f;
                         f.flags = std::array<Monstey_U32, 3>{0x40000015u, 0x80000015u, 0xC0000015u}[mode];
                         f.x9 = 0;
                         auto expected = f.image;
                         Monstey_ExpectedGuard(f, expected);
                         const auto descriptor = f.frame - 0x3E0u;
                         for (Monstey_U64 i = 0; i < 0x3D0u; ++i)
                             Monstey_Put(expected, f.base, descriptor + i, 0, 1);
                         Monstey_Put(expected, f.base, descriptor + 0x370u, f.flags, 4);
                         Monstey_CheckResult(f, f.Run(), 0, {0, f.allocationRdx}, expected);
                         Monstey_Check(f.events[1].kind == "Fill" && f.events[1].a == descriptor &&
                                           f.events[2].kind == "Allocate" && f.events[2].b == 0x1000u,
                                       "Real root/factory allocation path was not exercised");
                     });
}

void Monstey_Aliases()
{
    for (unsigned kind = 0; kind < 6; ++kind)
        Monstey_Case("installation_alias_" + std::to_string(kind), {"PP09", "PP10", "PP11", "PP13", "PP14"},
                     [&]
                     {
                         Monstey_Fixture f;
                         const auto object = f.backing + 0x10u;
                         if (kind < 3)
                         {
                             const auto table = object + (kind == 1 ? 0x3C8u : kind == 2 ? 0x3C1u : 0x3C0u);
                             f.manager = table - 0x190u;
                             f.Put(f.Table(), 0);
                         }
                         if (kind == 3)
                         {
                             f.frame = object + 0x390u;
                             f.Put(f.frame + 0x50u, f.count);
                         }
                         if (kind == 4)
                             f.bindings.imports.leave_cs = object + 0x3C0u;
                         if (kind == 5)
                         {
                             f.manager = f.frame - 0x140u;
                             f.Put(f.Table(), f.count);
                         }
                         auto expected = f.image;
                         Monstey_ExpectedGuard(f, expected);
                         if (kind == 5)
                             Monstey_ExpectedTrap(f, expected, f.entered.rdx);
                         const auto selected = Monstey_ExpectedPlacement(f, expected, 0, f.x9, f.count);
                         Monstey_Put(expected, f.base, f.Table(), selected);
                         Monstey_Put(expected, f.base, selected + 0x3C0u, f.Table());
                         Monstey_Put(expected, f.base, selected + 0x3C8u, 0);
                         Monstey_CheckResult(f, f.Run(), selected, {f.Table(), 0}, expected);
                         if (kind == 4)
                             Monstey_Check(f.leaveCalls[0].target_identity == f.Table(),
                                           "Leave cell did not observe installation write");
                     });
    for (unsigned cell = 0; cell < 2; ++cell)
        Monstey_Case("guard_alias_import_" + std::to_string(cell), {"PP03", "PP11", "PP13", "PP14"},
                     [&]
                     {
                         Monstey_Fixture f;
                         f.Put(f.frame + 0x50u, 0x420);
                         if (cell)
                             f.bindings.imports.leave_cs = f.Guard();
                         else
                             f.bindings.imports.enter_cs = f.Guard();
                         auto expected = f.image;
                         Monstey_ExpectedGuard(f, expected);
                         Monstey_CheckResult(f, f.Run(), 0, {0, f.flags}, expected);
                         Monstey_Check((cell ? f.leaveCalls[0].target_identity
                                             : f.enterCalls[0].base.target_identity) == f.Lock(),
                                       "Import cell was read before actual guard alias write");
                     });
    Monstey_Case("guard_equals_slot", {"PP04", "PP05", "PP11", "PP14"},
                 []
                 {
                     Monstey_Fixture f;
                     f.manager = f.frame - 0x160u;
                     f.Put(f.Table(), 0);
                     f.Put(f.frame + 0x50u, 0x420);
                     auto expected = f.image;
                     Monstey_ExpectedGuard(f, expected);
                     Monstey_ExpectedTrap(f, expected, f.entered.rdx);
                     Monstey_CheckResult(f, f.Run(), 0, {0, f.flags}, expected);
                 });
    Monstey_Case("binding_snapshot_contents_live", {"PP03", "PP11", "PP14"},
                 []
                 {
                     Monstey_Fixture f;
                     f.Put(f.frame + 0x50u, 0x420);
                     const auto originalLeave = f.bindings.imports.leave_cs;
                     f.hook = [&](std::string_view event)
                     {
                         if (event == "Enter")
                         {
                             f.bindings.imports.leave_cs = 0x2400;
                             f.Put(originalLeave, 0);
                             f.Put(0x2400, 0xAAAA);
                         }
                     };
                     auto expected = f.image;
                     Monstey_ExpectedGuard(f, expected);
                     Monstey_Put(expected, f.base, originalLeave, 0);
                     Monstey_Put(expected, f.base, 0x2400, 0xAAAA);
                     const auto result = f.Run();
                     Monstey_RecordNormal(result);
                     Monstey_Check(result.raw.rdx == f.left.raw.rdx && f.leaveCalls.size() == std::size_t{1} &&
                                       f.leaveCalls[0].slot_identity == originalLeave &&
                                       f.leaveCalls[0].target_identity == 0,
                                   "Bindings identities were not copied once or live target0 was rejected");
                     Monstey_Image(f, expected);
                 });
    Monstey_Case("manager_address_wrap", {"PP02", "PP05", "PP11", "PP14"},
                 []
                 {
                     Monstey_Fixture f;
                     f.manager = std::numeric_limits<Monstey_U64>::max() - 0x18Fu;
                     f.flags = 0;
                     f.Put(0, 0);
                     auto expected = f.image;
                     Monstey_ExpectedGuard(f, expected);
                     Monstey_ExpectedTrap(f, expected, 0xC0000000u);
                     Monstey_CheckResult(f, f.Run(), 0, {0, 0xC0000000u}, expected);
                     Monstey_Check(f.Table() == 0 && f.Lock() == 0x78u, "Modular address witness is not wrapped");
                 });
}

struct Monstey_SharedFaultToken
{
    Monstey_U64 value;
    std::string label;
};
struct Monstey_ProviderFault : std::runtime_error
{
    std::shared_ptr<Monstey_SharedFaultToken> token;
    explicit Monstey_ProviderFault(std::shared_ptr<Monstey_SharedFaultToken> value)
        : std::runtime_error("Owned primitive fault"), token(std::move(value))
    {
    }
};
struct Monstey_RetirementToken
{
    std::function<void()> retire;
    ~Monstey_RetirementToken()
    {
        if (retire)
            retire();
    }
};

void Monstey_ProviderFaults()
{
    for (unsigned mode = 0; mode < 3; ++mode)
        for (const auto *stage : {"Enter", "Fill", "Leave"})
            Monstey_Case("typed_partial_" + std::to_string(mode) + "_" + stage, {"PP08", "PP10", "PP13", "PP16"},
                         [&]
                         {
                             Monstey_Fixture f;
                             f.flags = std::array<Monstey_U32, 3>{0x40000010u, 0x80000010u, 0xC0000010u}[mode];
                             auto token = std::make_shared<Monstey_SharedFaultToken>(
                                 Monstey_SharedFaultToken{0xFEDCBA9876543210ULL, "actual-primitive-owned-token"});
                             f.hook = [&](std::string_view event)
                             {
                                 if (event == stage)
                                 {
                                     f.Put(0x2300, token->value);
                                     throw Monstey_ProviderFault(token);
                                 }
                             };
                             auto expected = f.image;
                             Monstey_ExpectedGuard(f, expected);
                             if (std::string_view(stage) == "Leave")
                             {
                                 const auto object = Monstey_ExpectedPlacement(f, expected, mode, f.x9, f.count);
                                 Monstey_Put(expected, f.base, f.Table(), object);
                                 Monstey_Put(expected, f.base, object + 0x3C0u, f.Table());
                                 Monstey_Put(expected, f.base, object + 0x3C8u, 0);
                             }
                             Monstey_Put(expected, f.base, 0x2300, token->value);
                             bool caught = false;
                             try
                             {
                                 static_cast<void>(f.Run());
                             }
                             catch (const Monstey_ProviderFault &fault)
                             {
                                 caught = true;
                                 Monstey_RecordFault(
                                     Monstey_J::object({{"kind", "owned_provider_token"},
                                                        {"token_value", Monstey_Word(fault.token->value)},
                                                        {"token_label", fault.token->label},
                                                        {"identity_matches", fault.token.get() == token.get()}}));
                                 Monstey_Check(fault.token.get() == token.get() &&
                                                   fault.token->value == 0xFEDCBA9876543210ULL &&
                                                   fault.token->label == "actual-primitive-owned-token",
                                               "Typed shared-token identity/value lost");
                             }
                             Monstey_Check(caught && f.events.back().kind == stage,
                                           "Provider fault changed dynamic type or invented cleanup");
                             Monstey_Image(f, expected);
                         });
}

void Monstey_CurrentHolders()
{
    Monstey_Case(
        "enter_clear_retirement_and_missing_reentry", {"PP04", "PP13", "PP15", "PP16"},
        []
        {
            Monstey_Fixture f;
            f.Put(f.frame + 0x50u, 0x420);
            auto retired = std::make_shared<bool>(false);
            auto token = std::make_shared<Monstey_RetirementToken>();
            token->retire = [&, retired]
            {
                *retired = true;
                f.Put(f.Table(), 1);
            };
            const auto nested = f.frame + 0x1000u;
            f.enter.enter =
                [&, token, retired](Monstey_GadbBinderMemory memory, const Monstey_GadbManagerEnterProjectionCall &call)
            {
                f.RecordEnter(call);
                f.enter.enter = {};
                Monstey_Check(!*retired, "Active Enter holder was retired during invocation");
                bool missing = false;
                try
                {
                    static_cast<void>(Monstey_GadbCreateManagerBackendForManager(
                        memory, f.manager, 0, f.x8, f.x9, nested, f.enter, f.leave, f.backend, f.bindings));
                }
                catch (const Monstey_GadbManagerPrimitiveBoundaryFault &fault)
                {
                    missing = true;
                    Monstey_RecordFault(Monstey_J::object({{"kind", "nested_missing_sync"},
                                                           {"site", Monstey_Word(fault.native_site)},
                                                           {"slot", Monstey_Word(fault.slot_identity)}}));
                    Monstey_Check(fault.kind == Monstey_GadbManagerPrimitiveBoundaryKind::missing_sync_service &&
                                      fault.native_site == 0x14114BA9FULL,
                                  "Nested self-clear must consume missing current Enter");
                }
                Monstey_Check(missing && !*retired, "Nested call copied an old holder or retired active callback");
                return f.entered;
            };
            f.leave.leave = [&, retired](Monstey_GadbBinderMemory, const Monstey_GadbManagerSyncCall &call)
            {
                Monstey_Check(*retired, "Retirement must precede observable subsequent parent slot read");
                f.RecordLeave(call);
                return f.left;
            };
            token.reset();
            auto expected = f.image;
            Monstey_ExpectedGuard(f, expected);
            Monstey_Put(expected, f.base, nested + 0x30u, f.Lock());
            Monstey_Put(expected, f.base, f.Table(), 1);
            Monstey_ExpectedTrap(f, expected, f.entered.rdx);
            Monstey_CheckResult(f, f.Run(), 0, {0, f.flags}, expected);
            Monstey_Check(*retired && !f.enter.enter, "Cleared Enter was not retired by caller return");
        });
    Monstey_Case(
        "enter_replacement_actual_nested_parent", {"PP03", "PP12", "PP14", "PP15"},
        []
        {
            Monstey_Fixture f;
            f.flags = 0;
            const auto nested = f.frame + 0x1000u;
            const Monstey_GadbManagerEnterProjectionResult replacement{0xC2123456789ABCDEULL, 0xC8123456789ABCDEULL,
                                                                       0xC9123456789ABCDEULL};
            f.enter.enter = [&](Monstey_GadbBinderMemory memory, const Monstey_GadbManagerEnterProjectionCall &call)
            {
                f.RecordEnter(call);
                f.enter.enter =
                    [&, replacement](Monstey_GadbBinderMemory, const Monstey_GadbManagerEnterProjectionCall &next)
                {
                    f.RecordEnter(next);
                    return replacement;
                };
                const auto result = Monstey_GadbCreateManagerBackendForManager(memory, f.manager, 0, f.x8, f.x9, nested,
                                                                               f.enter, f.leave, f.backend, f.bindings);
                Monstey_Check(result.raw.rax == 0 && result.raw.rdx == f.left.raw.rdx &&
                                  result.post_leave_r8 == f.left.post_leave_r8 &&
                                  result.post_leave_r9 == f.left.post_leave_r9,
                              "Actual nested parent return differs");
                return f.entered;
            };
            auto expected = f.image;
            Monstey_ExpectedGuard(f, expected);
            Monstey_ExpectedTrap(f, expected, 0xC0000000u);
            Monstey_Put(expected, f.base, nested + 0x30u, f.Lock());
            Monstey_Put(expected, f.base, nested + 8u, 0xC0000000u);
            Monstey_Put(expected, f.base, nested + 0x10u, replacement.r8);
            Monstey_Put(expected, f.base, nested + 0x18u, replacement.r9);
            const auto result = f.Run();
            Monstey_RecordNormal(result);
            Monstey_Check(result.raw.rax == 0 && result.raw.rdx == f.left.raw.rdx &&
                              result.post_leave_r8 == f.left.post_leave_r8 &&
                              result.post_leave_r9 == f.left.post_leave_r9 && f.enterCalls.size() == std::size_t{2} &&
                              f.leaveCalls.size() == std::size_t{2},
                          "Nested parent did not use current persistent Enter/Leave fields");
            Monstey_Check(f.enterCalls[1].base.incoming.rax == nested + 0x28u, "Reentry frame witness not real");
            Monstey_Image(f, expected);
        });
    Monstey_Case("leave_current_replaced_during_enter", {"PP11", "PP12", "PP15"},
                 []
                 {
                     Monstey_Fixture f;
                     f.Put(f.frame + 0x50u, 0x420);
                     const Monstey_GadbManagerLeaveProjectionResult replacement{
                         {0x9111111111111111ULL, 0x9222222222222222ULL}, 0x9888888888888888ULL, 0x9999999999999999ULL};
                     f.hook = [&](std::string_view event)
                     {
                         if (event == "Enter")
                             f.leave.leave =
                                 [&, replacement](Monstey_GadbBinderMemory, const Monstey_GadbManagerSyncCall &call)
                             {
                                 f.RecordLeave(call);
                                 return replacement;
                             };
                     };
                     auto expected = f.image;
                     Monstey_ExpectedGuard(f, expected);
                     const auto result = f.Run();
                     Monstey_RecordNormal(result);
                     Monstey_Check(result.raw.rax == 0 && result.raw.rdx == replacement.raw.rdx &&
                                       result.post_leave_r8 == replacement.post_leave_r8 &&
                                       result.post_leave_r9 == replacement.post_leave_r9,
                                   "Parent cached Leave or fabricated its output words");
                     Monstey_Image(f, expected);
                 });
    Monstey_Case("leave_clear_retired_by_caller_return", {"PP11", "PP12", "PP15"},
                 []
                 {
                     Monstey_Fixture f;
                     f.Put(f.frame + 0x50u, 0x420);
                     auto retired = std::make_shared<bool>(false);
                     auto token = std::make_shared<Monstey_RetirementToken>();
                     token->retire = [&, retired]
                     {
                         *retired = true;
                         f.Put(0x2300, 0xAABBCCDD);
                     };
                     f.leave.leave =
                         [&, token, retired](Monstey_GadbBinderMemory, const Monstey_GadbManagerSyncCall &call)
                     {
                         f.RecordLeave(call);
                         f.leave.leave = {};
                         Monstey_Check(!*retired, "Active Leave target was destroyed during invocation");
                         return f.left;
                     };
                     token.reset();
                     auto expected = f.image;
                     Monstey_ExpectedGuard(f, expected);
                     Monstey_Put(expected, f.base, 0x2300, 0xAABBCCDD);
                     Monstey_CheckResult(f, f.Run(), 0, {0, f.flags}, expected);
                     Monstey_Check(*retired && !f.leave.leave, "Cleared Leave must be retired by caller return");
                 });
}

void Monstey_WrapAndWidths()
{
    for (const auto frame : std::array<Monstey_U64, 2>{0x200u, Monstey_U64{0} - 0x30u})
        Monstey_Case("numeric_frame_" + Monstey_Word(frame), {"PP02", "PP03", "PP07", "PP08", "PP11"},
                     [&]
                     {
                         Monstey_Fixture f;
                         f.frame = frame;
                         f.Put(f.frame + 0x50u, 0x420);
                         auto expected = f.image;
                         Monstey_ExpectedGuard(f, expected);
                         Monstey_CheckResult(f, f.Run(), 0, {0, f.flags}, expected);
                     });
    Monstey_Case("full64_backing_high_bits_not_truncated", {"PP02", "PP03", "PP07", "PP08"},
                 []
                 {
                     Monstey_Fixture f;
                     f.x9 = 0x8000000000000000ULL;
                     f.Put(f.frame + 0x50u, 0x420);
                     auto expected = f.image;
                     Monstey_ExpectedGuard(f, expected);
                     Monstey_CheckResult(f, f.Run(), 0, {0, f.flags}, expected);
                     Monstey_Check(f.events.size() == std::size_t{2},
                                   "High nonzero backing was truncated into allocation route");
                 });
    Monstey_Case(
        "wrapped_BK_frame_consumed_at_fill_primitive", {"PP02", "PP08", "PP13", "PP16"},
        []
        {
            Monstey_Fixture f;
            f.frame = 0x200;
            f.x9 = 0;
            f.Put(f.frame + 0x50u, 0x2000);
            auto token = std::make_shared<Monstey_SharedFaultToken>(
                Monstey_SharedFaultToken{0x123456789ABCDEF0ULL, "wrapped-frame"});
            f.backend.fill =
                [&, token](Monstey_GadbBinderMemory,
                           const Monstey_GadbManagerBackendFillCall &call) -> Monstey_GadbManagerBackendRdxResult
            {
                Monstey_Check(call.destination == f.frame - 0x3E0u && call.site.body_rsp == f.frame - 0x408u &&
                                  call.count == 0x3D0u,
                              "Modular BK body/frame was not forwarded to actual primitive");
                throw Monstey_ProviderFault(token);
            };
            auto expected = f.image;
            Monstey_ExpectedGuard(f, expected);
            bool caught = false;
            try
            {
                static_cast<void>(f.Run());
            }
            catch (const Monstey_ProviderFault &fault)
            {
                caught = true;
                Monstey_Check(fault.token == token, "Wrapped primitive fault token changed");
                Monstey_RecordFault(
                    Monstey_J::object({{"kind", "wrapped_BK_primitive"}, {"identity_matches", fault.token == token}}));
            }
            Monstey_Check(caught && f.leaveCalls.empty(), "Wrapped frame was preflighted or cleanup fabricated");
            Monstey_Image(f, expected);
        });
    Monstey_Case("zero_enter_and_leave_targets_dispatch", {"PP03", "PP11", "PP13", "PP14"},
                 []
                 {
                     Monstey_Fixture f;
                     f.flags = 0;
                     f.Put(f.bindings.imports.enter_cs, 0);
                     f.Put(f.bindings.imports.leave_cs, 0);
                     auto expected = f.image;
                     Monstey_ExpectedGuard(f, expected);
                     Monstey_ExpectedTrap(f, expected, 0xC0000000u);
                     Monstey_CheckResult(f, f.Run(), 0, {0, 0xC0000000u}, expected);
                     Monstey_Check(f.enterCalls[0].base.target_identity == 0 && f.leaveCalls[0].target_identity == 0,
                                   "Mapped target0 was not dispatched as an ordinary identity");
                 });
}

void Monstey_FutureConsumer()
{
    Monstey_Case("actual_four_word_future_trap_witness", {"PP12", "PP17"},
                 []
                 {
                     Monstey_Fixture f;
                     f.flags = 0;
                     auto expected = f.image;
                     Monstey_ExpectedGuard(f, expected);
                     Monstey_ExpectedTrap(f, expected, 0xC0000000u);
                     const auto result = f.Run();
                     Monstey_CheckResult(f, result, 0, {0, 0xC0000000u}, expected);
                     const auto outer = f.frame + 0x30u;
                     f.Put(f.manager + 0x10u, result.raw.rax);
                     const auto trap = Monstey_GadbManagerFreeTrapReturn(f.Memory(), outer - 8u, result.raw,
                                                                         result.post_leave_r8, result.post_leave_r9);
                     Monstey_Put(expected, f.base, f.manager + 0x10u, 0);
                     Monstey_Put(expected, f.base, outer + 8u, f.left.raw.rdx);
                     Monstey_Put(expected, f.base, outer + 0x10u, f.left.post_leave_r8);
                     Monstey_Put(expected, f.base, outer + 0x18u, f.left.post_leave_r9);
                     Monstey_Put(expected, f.base, 0, 0, 4);
                     Monstey_Check(trap.rax == 0 && trap.rdx == f.left.raw.rdx,
                                   "Known consumer did not receive actual post-Leave Raw words");
                     Monstey_Image(f, expected);
                 });
}
} // namespace

int wmain(int argc, wchar_t **argv)
{
    using namespace Monstey;
    if (argc != 2)
    {
        std::cerr << "Need NEW_REPORT\n";
        return 2;
    }
    const std::filesystem::path report = argv[1];
    try
    {
        Monstey_Routes();
        Monstey_LiveInputs();
        Monstey_FaultPrefixes();
        Monstey_Aliases();
        Monstey_ProviderFaults();
        Monstey_CurrentHolders();
        Monstey_WrapAndWidths();
        Monstey_FutureConsumer();
        Monstey_WriteNewText(
            report,
            Monstey_Json::object(
                {{"status", "PASS"},
                 {"cases", Monstey_Cases},
                 {"case_count", Monstey_Cases.size()},
                 {"checks", Monstey_Checks},
                 {"whole_image_checks", Monstey_Images},
                 {"whole_image_bytes", Monstey_ImageBytes},
                 {"normal_parent_observations", Monstey_Normals},
                 {"expected_fault_observations", Monstey_Faults},
                 {"normal_raw4", Monstey_NormalWords},
                 {"fault_signatures", Monstey_FaultSignatures},
                 {"PP18_PP19", "separate immutable header/source/compile/reference-timing artifacts"},
                 {"PP20", "observable compiled controls pending; not claimed by positive IDs"},
                 {"late_range_faults", "unreachable stable-span positions documented separately, not synthesized"},
                 {"physical_CPU_OS_or_higher_parent_qualified", false},
                 {"reference_CPP_or_harness_read", false}})
                    .dump(2) +
                "\n");
        std::cout << "PASS " << Monstey_Cases.size() << " cases " << Monstey_Checks << " checks\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        const auto failure = Monstey_Json::object({{"status", "FAIL"},
                                                   {"failed_case", Monstey_CurrentCase},
                                                   {"error", error.what()},
                                                   {"completed_cases", Monstey_Cases},
                                                   {"checks", Monstey_Checks},
                                                   {"normal_parent_observations", Monstey_Normals},
                                                   {"expected_fault_observations", Monstey_Faults},
                                                   {"normal_raw4", Monstey_NormalWords},
                                                   {"fault_signatures", Monstey_FaultSignatures}});
        try
        {
            Monstey_WriteNewText(report, failure.dump(2) + "\n");
        }
        catch (...)
        {
        }
        std::cerr << Monstey_CurrentCase << ": " << error.what() << '\n';
        return 1;
    }
}
