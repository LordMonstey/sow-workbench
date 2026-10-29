#include "Monstey_ParentSession.hpp"
#include <algorithm>
#include <limits>

namespace Monstey_ParentDifferential
{
namespace
{
std::size_t Monstey_Offset(Monstey::Monstey_GadbBinderMemory memory, Monstey_U64 address, Monstey_U64 width, bool writing)
{
    if (address < memory.base || address - memory.base > memory.bytes.size() ||
        width > memory.bytes.size() - static_cast<std::size_t>(address - memory.base))
        throw Monstey::Monstey_GadbBufferedMemoryFault(address, static_cast<std::size_t>(width), writing);
    return static_cast<std::size_t>(address - memory.base);
}
Monstey_Raw4 Monstey_Raw(const Monstey::Monstey_GadbManagerParentResult &v)
{
    return {v.raw.rax, v.raw.rdx, v.post_leave_r8, v.post_leave_r9};
}
}
Monstey_U64 Monstey_Read(Monstey::Monstey_GadbBinderMemory m, Monstey_U64 a, unsigned width)
{
    const auto p = Monstey_Offset(m, a, width, false);
    Monstey_U64 value = 0;
    for (unsigned i = 0; i < width; ++i)
        value |= static_cast<Monstey_U64>(m.bytes[p + i]) << (8 * i);
    return value;
}
void Monstey_Write(Monstey::Monstey_GadbBinderMemory m, Monstey_U64 a, Monstey_U64 value, unsigned width)
{
    const auto p = Monstey_Offset(m, a, width, true);
    for (unsigned i = 0; i < width; ++i)
        m.bytes[p + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
void Monstey_Seed(Monstey_Case &c, Monstey_U64 a, Monstey_U64 value, unsigned width)
{
    if (a >= c.base && a - c.base <= c.bytes.size() && width <= c.bytes.size() - (a - c.base))
        Monstey_Write({c.base, c.bytes}, a, value, width);
}
Monstey_ProviderFault::Monstey_ProviderFault(std::shared_ptr<const Monstey_Token> t, Monstey_U64 v)
    : std::runtime_error("owned_parent_provider"), token(std::move(t)), value(v)
{
}
Monstey_Retire::Monstey_Retire(std::shared_ptr<Monstey_SharedState> s, Monstey_U64 a, Monstey_U64 v, unsigned g)
    : state(std::move(s)), address(a), value(v), generation(g)
{
}
Monstey_Retire::~Monstey_Retire()
{
    if (!state->active)
        return;
    try
    {
        Monstey_Write({state->base, state->bytes}, address, value, 8);
        state->events.push_back({Monstey_Primitive::GuardEnter, 2, {generation, address, value}, state->bytes});
    }
    catch (...)
    {
        state->observer_failed = true;
    }
}
Monstey_Session::Monstey_Session(const Monstey_Case &c, bool ref)
    : scenario(c), state(std::make_shared<Monstey_SharedState>(Monstey_SharedState{c.base, c.bytes, {}, true, false})),
      reference(ref), caller_bindings(c.bindings)
{
    for (unsigned i = 0; i < counts.size(); ++i)
    {
        const auto k = static_cast<Monstey_Primitive>(i);
        if (std::find(c.missing.begin(), c.missing.end(), k) == c.missing.end())
            Monstey_Install(k);
    }
}
Monstey_Session::~Monstey_Session()
{
    state->active = false;
}
Monstey::Monstey_GadbBinderMemory Monstey_Session::Monstey_Memory()
{
    return {state->base, state->bytes};
}
Monstey::Monstey_GadbManagerParentResult Monstey_Session::Monstey_Execute(const Monstey_Request &r)
{
    if (reference)
        return Monstey_ParentReview::Monstey_ReferenceCreateManagerBackendForManager(
            Monstey_Memory(), r.manager, r.flags, r.entry_r8, r.backing, r.body, enter, leave, ref_backend, caller_bindings);
    return Monstey::Monstey_GadbCreateManagerBackendForManager(
        Monstey_Memory(), r.manager, r.flags, r.entry_r8, r.backing, r.body, enter, leave, backend, caller_bindings);
}
void Monstey_Session::Monstey_Install(Monstey_Primitive k)
{
    const auto generation = ++generations[static_cast<unsigned>(k)];
    if (k == Monstey_Primitive::GuardEnter)
    {
        std::shared_ptr<Monstey_Retire> lifetime;
        if (scenario.retire_enter)
            lifetime = std::make_shared<Monstey_Retire>(state, scenario.retire_address, scenario.retire_value, generation);
        enter.enter = [this, lifetime, generation, serial = 0u](Monstey::Monstey_GadbBinderMemory m,
            const Monstey::Monstey_GadbManagerEnterProjectionCall &c) mutable
        {
            (void)lifetime;
            const auto &b = c.base;
            auto r = Monstey_Call(m, Monstey_Primitive::GuardEnter, ++serial, generation,
                {static_cast<Monstey_U64>(b.operation), b.native_site, b.slot_identity, b.target_identity,
                 b.critical_section, b.incoming.rax, b.incoming.rdx, c.incoming_r8, c.incoming_r9});
            return Monstey::Monstey_GadbManagerEnterProjectionResult{r.raw.rdx, r.r8, r.r9};
        };
    }
    else if (k == Monstey_Primitive::GuardLeave)
    {
        leave.leave = [this, generation, serial = 0u](Monstey::Monstey_GadbBinderMemory m,
            const Monstey::Monstey_GadbManagerSyncCall &c) mutable
        {
            auto r = Monstey_Call(m, Monstey_Primitive::GuardLeave, ++serial, generation,
                {static_cast<Monstey_U64>(c.operation), c.native_site, c.slot_identity, c.target_identity,
                 c.critical_section, c.incoming.rax, c.incoming.rdx});
            return Monstey::Monstey_GadbManagerLeaveProjectionResult{r.raw, r.r8, r.r9};
        };
    }
    else if (reference)
        Monstey_InstallReference(k, generation);
    else
        Monstey_InstallCanonical(k, generation);
}
void Monstey_Session::Monstey_Clear(Monstey_Primitive k)
{
    if (k == Monstey_Primitive::GuardEnter) enter.enter = {};
    else if (k == Monstey_Primitive::GuardLeave) leave.leave = {};
    else if (reference) Monstey_ClearReference(k);
    else Monstey_ClearCanonical(k);
}
void Monstey_Session::Monstey_Effects(Monstey_Primitive k, unsigned occurrence, bool after)
{
    for (const auto &e : scenario.effects)
    {
        if (e.primitive != k || e.occurrence != occurrence || e.after_default != after)
            continue;
        switch (e.action)
        {
        case Monstey_Action::Write32: Monstey_Write(Monstey_Memory(), e.address, e.value, 4); break;
        case Monstey_Action::Write64: Monstey_Write(Monstey_Memory(), e.address, e.value, 8); break;
        case Monstey_Action::Throw: throw Monstey_ProviderFault(scenario.token, e.value);
        case Monstey_Action::ClearCurrent: Monstey_Clear(k); break;
        case Monstey_Action::ReplaceCurrent: Monstey_Install(k); break;
        case Monstey_Action::ReplaceLeave: Monstey_Install(Monstey_Primitive::GuardLeave); break;
        case Monstey_Action::Reenter:
            if (!scenario.nested)
                throw std::logic_error("reentry request missing");
            {
                const auto r = Monstey_Raw(Monstey_Execute(*scenario.nested));
                state->events.push_back({k, 3, {r[0], r[1], r[2], r[3]}, state->bytes});
            }
            break;
        }
    }
}
Monstey_PrimitiveResult Monstey_Session::Monstey_Call(Monstey::Monstey_GadbBinderMemory m,
    Monstey_Primitive k, unsigned serial, unsigned generation, std::vector<Monstey_U64> words)
{
    const auto occurrence = ++counts[static_cast<unsigned>(k)];
    if (occurrence > 128)
        throw std::logic_error("owned primitive script budget exceeded");
    words.insert(words.begin(), {serial, generation});
    state->events.push_back({k, 0, words, state->bytes});
    Monstey_Effects(k, occurrence, false);
    Monstey_PrimitiveResult result{{0, 0}, 0, 0};
    switch (k)
    {
    case Monstey_Primitive::GuardEnter:
        result = {{0, scenario.enter_out.rdx}, scenario.enter_out.r8, scenario.enter_out.r9}; break;
    case Monstey_Primitive::GuardLeave:
        result = {scenario.leave_out.raw, scenario.leave_out.post_leave_r8, scenario.leave_out.post_leave_r9}; break;
    case Monstey_Primitive::Fill:
        {
            const auto destination = words.at(7), count = words.at(8), value = words.at(9);
            const auto offset = Monstey_Offset(m, destination, count, true);
            std::fill_n(m.bytes.begin() + static_cast<std::ptrdiff_t>(offset), static_cast<std::size_t>(count),
                static_cast<std::uint8_t>(value));
            result.raw.rdx = scenario.fill_rdx;
        }
        break;
    case Monstey_Primitive::Allocate: result.raw = {scenario.allocate_pointer, scenario.allocate_rdx}; break;
    case Monstey_Primitive::SystemInfo:
        for (unsigned i = 0; i < 48; ++i)
            Monstey_Write(m, words.at(7) + i, 0x5a, 1);
        Monstey_Write(m, words.at(7) + 4, 0x1000, 4);
        Monstey_Write(m, words.at(7) + 0x28, 0x10000, 4);
        break;
    case Monstey_Primitive::Sleep: break;
    case Monstey_Primitive::Random: result.raw.rax = scenario.random_eax; break;
    case Monstey_Primitive::Initialize: result.raw.rdx = scenario.initialize_rdx; break;
    case Monstey_Primitive::BackendEnter: break;
    case Monstey_Primitive::BackendLeave: result.raw.rdx = scenario.backend_leave_rdx; break;
    case Monstey_Primitive::Exchange:
        result.raw.rax = Monstey_Read(m, words.at(3), 4);
        Monstey_Write(m, words.at(3), words.at(4), 4);
        break;
    case Monstey_Primitive::CompareExchange:
        result.raw.rax = Monstey_Read(m, words.at(3), 4);
        if (result.raw.rax == words.at(4))
            Monstey_Write(m, words.at(3), words.at(5), 4);
        break;
    }
    Monstey_Effects(k, occurrence, true);
    state->events.push_back({k, 1, words, state->bytes});
    return result;
}
std::vector<Monstey_U64> Monstey_Site(const Monstey::Monstey_GadbManagerBackendImportSite &s)
{
    return {s.native_site, s.call_site, s.body_rsp, s.cell_identity, s.target_identity};
}
std::vector<Monstey_U64> Monstey_Site(const Monstey_BackendReview::Monstey_Site &s)
{
    return {s.pc, s.call, s.body, s.cell, s.target};
}
void Monstey_Add(std::vector<Monstey_U64> &v, std::initializer_list<Monstey_U64> a)
{
    v.insert(v.end(), a.begin(), a.end());
}
}
