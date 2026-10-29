#include "Monstey_ParentSession.hpp"
#include "Monstey_FreeReference.hpp"
#include <algorithm>

namespace Monstey_ParentDifferential
{
static_assert(static_cast<Monstey_U64>(Monstey::Monstey_GadbManagerBackendImport::fill) == static_cast<Monstey_U64>(Monstey_BackendReview::Monstey_Kind::fill));
static_assert(static_cast<Monstey_U64>(Monstey::Monstey_GadbManagerBackendImport::allocate) == static_cast<Monstey_U64>(Monstey_BackendReview::Monstey_Kind::allocate));
static_assert(static_cast<Monstey_U64>(Monstey::Monstey_GadbManagerBackendImport::system_info) == static_cast<Monstey_U64>(Monstey_BackendReview::Monstey_Kind::info));
static_assert(static_cast<Monstey_U64>(Monstey::Monstey_GadbManagerBackendImport::sleep) == static_cast<Monstey_U64>(Monstey_BackendReview::Monstey_Kind::sleep));
static_assert(static_cast<Monstey_U64>(Monstey::Monstey_GadbManagerBackendImport::random) == static_cast<Monstey_U64>(Monstey_BackendReview::Monstey_Kind::random));
static_assert(static_cast<Monstey_U64>(Monstey::Monstey_GadbManagerBackendImport::initialize_cs) == static_cast<Monstey_U64>(Monstey_BackendReview::Monstey_Kind::initialize));
static_assert(static_cast<Monstey_U64>(Monstey::Monstey_GadbManagerBackendImport::enter_cs) == static_cast<Monstey_U64>(Monstey_BackendReview::Monstey_Kind::enter));
static_assert(static_cast<Monstey_U64>(Monstey::Monstey_GadbManagerBackendImport::leave_cs) == static_cast<Monstey_U64>(Monstey_BackendReview::Monstey_Kind::leave));
namespace
{
Monstey_Fault Monstey_MemoryFault(Monstey_U64 address, Monstey_U64 width, bool writing)
{
    return {Monstey_FaultKind::Memory, {address, width, writing ? 1u : 0u}, {}, false};
}
Monstey_Fault Monstey_BackendFault(Monstey_U64 kind, std::vector<Monstey_U64> site)
{
    site.insert(site.begin(), kind);
    return {Monstey_FaultKind::BackendImport, std::move(site), {}, false};
}
Monstey_Outcome Monstey_Run(const Monstey_Case &c, bool reference)
{
    Monstey_Session s(c, reference);
    Monstey_Outcome result;
    try
    {
        for (const auto &request : c.requests)
        {
            const auto r = s.Monstey_Execute(request);
            result.results.push_back({r.raw.rax, r.raw.rdx, r.post_leave_r8, r.post_leave_r9});
            if (c.consume_higher)
            {
                Monstey_Write(s.Monstey_Memory(), request.manager + 0x10, r.raw.rax, 8);
                if (r.raw.rax == 0)
                {
                    const auto entry = request.body + 0x30 - 8;
                    if (reference)
                    {
                        const Monstey_FreeReview::Monstey_Trap trap{entry, {r.raw.rax, r.raw.rdx}, r.post_leave_r8, r.post_leave_r9};
                        (void)Monstey_FreeReview::Monstey_Run(0, {s.state->base, s.state->bytes, nullptr},
                            Monstey_FreeReview::Monstey_Operation::trap, trap, Monstey::Monstey_GadbManagerFreeServices{});
                    }
                    else
                        (void)Monstey::Monstey_GadbManagerFreeTrapReturn(s.Monstey_Memory(), entry, r.raw,
                            r.post_leave_r8, r.post_leave_r9);
                }
            }
        }
    }
    catch (const Monstey::Monstey_GadbBufferedMemoryFault &e)
    {
        result.fault = Monstey_MemoryFault(e.address, e.width, e.writing);
    }
    catch (const Monstey_PlacementReview::Monstey_Fault &e)
    {
        if (e.kind == Monstey_PlacementReview::Monstey_FaultKind::memory)
            result.fault = Monstey_MemoryFault(e.address, e.width, e.writing);
        else
            result.fault = Monstey_Fault{Monstey_FaultKind::CompareBinding,
                {e.pc, e.address, e.width, e.comparand, e.desired}, {}, false};
    }
    catch (const Monstey::Monstey_GadbManagerPrimitiveBoundaryFault &e)
    {
        result.fault = Monstey_Fault{Monstey_FaultKind::PrimitiveBinding,
            {static_cast<Monstey_U64>(e.kind), e.native_site, e.address, e.width,
             e.slot_identity, e.target_identity, e.argument}, {}, false};
    }
    catch (const Monstey::Monstey_GadbManagerBackendImportFault &e)
    {
        result.fault = Monstey_BackendFault(static_cast<Monstey_U64>(e.kind), Monstey_Site(e.site));
    }
    catch (const Monstey_BackendReview::Monstey_ImportFault &e)
    {
        result.fault = Monstey_BackendFault(static_cast<Monstey_U64>(e.kind), Monstey_Site(e.site));
    }
    catch (const Monstey::Monstey_GadbManagerBackendExchange32Fault &e)
    {
        result.fault = Monstey_Fault{Monstey_FaultKind::ExchangeBinding,
            {e.native_site, e.address, e.width, e.value}, {}, false};
    }
    catch (const Monstey_BackendReview::Monstey_ExchangeFault &e)
    {
        result.fault = Monstey_Fault{Monstey_FaultKind::ExchangeBinding,
            {e.pc, e.address, e.width, e.value}, {}, false};
    }
    catch (const Monstey::Monstey_GadbManagerCompareExchange32Fault &e)
    {
        result.fault = Monstey_Fault{Monstey_FaultKind::CompareBinding,
            {e.native_site, e.address, e.width, e.comparand, e.desired}, {}, false};
    }
    catch (const Monstey_ProviderFault &e)
    {
        result.fault = Monstey_Fault{Monstey_FaultKind::Provider,
            {e.token->identity, e.value}, e.what(), e.token == c.token};
    }
    catch (const std::exception &e)
    {
        result.fault = Monstey_Fault{Monstey_FaultKind::Unexpected, {}, e.what(), false};
        result.observer_failed = true;
    }
    catch (...)
    {
        result.fault = Monstey_Fault{Monstey_FaultKind::Unexpected, {}, "unknown exception", false};
        result.observer_failed = true;
    }
    result.image = s.state->bytes;
    result.events = s.state->events;
    result.observer_failed = result.observer_failed || s.state->observer_failed;
    return result;
}
}
Monstey_Outcome Monstey_RunProduct(const Monstey_Case &c)
{
    return Monstey_Run(c, false);
}
Monstey_Outcome Monstey_RunReference(const Monstey_Case &c)
{
    return Monstey_Run(c, true);
}
bool Monstey_Equal(const Monstey_Outcome &a, const Monstey_Outcome &b)
{
    return !a.observer_failed && !b.observer_failed && a.results == b.results && a.image == b.image &&
        a.events == b.events && a.fault == b.fault;
}
bool Monstey_MeetsLiteralExpectations(const Monstey_Case &c, const Monstey_Outcome &r)
{
    if (r.observer_failed)
        return false;
    if (c.expected_fault)
    {
        if (!r.fault || r.fault->kind != c.expected_fault->kind)
            return false;
        if (r.fault->kind == Monstey_FaultKind::Memory)
        {
            const auto &e = *c.expected_fault;
            if ((e.address && r.fault->words.at(0) != *e.address) ||
                (e.width && r.fault->words.at(1) != *e.width) ||
                (e.writing && (r.fault->words.at(2) != 0) != *e.writing))
                return false;
        }
        if (r.fault->kind == Monstey_FaultKind::Provider && !r.fault->supplied_token_preserved)
            return false;
    }
    else if (r.fault || r.results != c.expected_raw)
        return false;
    if (!r.events.empty() && r.events.front().primitive == Monstey_Primitive::GuardEnter)
    {
        const auto &w = r.events.front().words;
        const auto &q = c.requests.front();
        if (w.at(3) != 0x14114BA9FULL || w.at(6) != q.manager + 0x208 ||
            w.at(7) != q.body + 0x28 || w.at(8) != q.manager + 0x208 ||
            w.at(9) != q.entry_r8 || w.at(10) != q.backing)
            return false;
    }
    return true;
}
}
