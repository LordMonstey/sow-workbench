#include "Monstey_ParentSession.hpp"

namespace Monstey_ParentDifferential
{
void Monstey_Session::Monstey_InstallReference(Monstey_Primitive k, unsigned generation)
{
    using namespace Monstey_BackendReview;
    const auto memory = [](Monstey_Image m) { return Monstey::Monstey_GadbBinderMemory{m.base, m.bytes}; };
    switch (k)
    {
    case Monstey_Primitive::Fill:
        ref_backend.fill = [this, generation, memory, serial = 0u](Monstey_Image m, const Monstey_Fill &c) mutable
        {
            auto w = Monstey_ParentDifferential::Monstey_Site(c.site); Monstey_Add(w, {c.destination, c.count, c.value});
            return Monstey_Call(memory(m), Monstey_Primitive::Fill, ++serial, generation, std::move(w)).raw.rdx;
        }; break;
    case Monstey_Primitive::Allocate:
        ref_backend.allocate = [this, generation, memory, serial = 0u](Monstey_Image m, const Monstey_Allocation &c) mutable
        {
            auto w = Monstey_ParentDifferential::Monstey_Site(c.site); Monstey_Add(w, {c.address, c.size, c.type, c.protection});
            const auto r = Monstey_Call(memory(m), Monstey_Primitive::Allocate, ++serial, generation, std::move(w));
            return Monstey_Raw{r.raw.rax, r.raw.rdx};
        }; break;
    case Monstey_Primitive::SystemInfo:
        ref_backend.info = [this, generation, memory, serial = 0u](Monstey_Image m, const Monstey_Info &c) mutable
        {
            auto w = Monstey_ParentDifferential::Monstey_Site(c.site); Monstey_Add(w, {c.output});
            (void)Monstey_Call(memory(m), Monstey_Primitive::SystemInfo, ++serial, generation, std::move(w));
        }; break;
    case Monstey_Primitive::Sleep:
        ref_backend.sleep = [this, generation, memory, serial = 0u](Monstey_Image m, const Monstey_Sleep &c) mutable
        {
            auto w = Monstey_ParentDifferential::Monstey_Site(c.site); Monstey_Add(w, {c.milliseconds, c.alertable});
            (void)Monstey_Call(memory(m), Monstey_Primitive::Sleep, ++serial, generation, std::move(w));
        }; break;
    case Monstey_Primitive::Random:
        ref_backend.random = [this, generation, memory, serial = 0u](Monstey_Image m, const Monstey_BackendReview::Monstey_Site &c) mutable
        {
            return static_cast<Monstey_U32>(Monstey_Call(memory(m), Monstey_Primitive::Random, ++serial, generation,
                Monstey_ParentDifferential::Monstey_Site(c)).raw.rax);
        }; break;
    case Monstey_Primitive::Initialize:
        ref_backend.initialize = [this, generation, memory, serial = 0u](Monstey_Image m, const Monstey_Cs &c) mutable
        {
            auto w = Monstey_ParentDifferential::Monstey_Site(c.site); Monstey_Add(w, {c.lock, c.spin});
            return Monstey_Call(memory(m), Monstey_Primitive::Initialize, ++serial, generation, std::move(w)).raw.rdx;
        }; break;
    case Monstey_Primitive::BackendEnter:
        ref_backend.enter = [this, generation, memory, serial = 0u](Monstey_Image m, const Monstey_Sync &c) mutable
        {
            auto w = Monstey_ParentDifferential::Monstey_Site(c.site); Monstey_Add(w, {c.lock});
            (void)Monstey_Call(memory(m), Monstey_Primitive::BackendEnter, ++serial, generation, std::move(w));
        }; break;
    case Monstey_Primitive::BackendLeave:
        ref_backend.leave = [this, generation, memory, serial = 0u](Monstey_Image m, const Monstey_Sync &c) mutable
        {
            auto w = Monstey_ParentDifferential::Monstey_Site(c.site); Monstey_Add(w, {c.lock});
            return Monstey_Call(memory(m), Monstey_Primitive::BackendLeave, ++serial, generation, std::move(w)).raw.rdx;
        }; break;
    case Monstey_Primitive::Exchange:
        ref_backend.exchange = [this, generation, memory, serial = 0u](Monstey_Image m, const Monstey_Exchange &c) mutable
        {
            return static_cast<Monstey_U32>(Monstey_Call(memory(m), Monstey_Primitive::Exchange, ++serial, generation,
                {c.pc, c.address, c.value}).raw.rax);
        }; break;
    case Monstey_Primitive::CompareExchange:
        ref_backend.lower.cas = [this, generation, serial = 0u](Monstey_PlacementReview::Monstey_Image m,
            const Monstey_PlacementReview::Monstey_Call &c) mutable
        {
            return static_cast<Monstey_U32>(Monstey_Call({m.base, m.bytes}, Monstey_Primitive::CompareExchange,
                ++serial, generation, {c.pc, c.address, c.comparand, c.desired}).raw.rax);
        }; break;
    default: throw std::logic_error("invalid reference primitive binding");
    }
}
void Monstey_Session::Monstey_ClearReference(Monstey_Primitive k)
{
    switch (k)
    {
    case Monstey_Primitive::Fill: ref_backend.fill = {}; break;
    case Monstey_Primitive::Allocate: ref_backend.allocate = {}; break;
    case Monstey_Primitive::SystemInfo: ref_backend.info = {}; break;
    case Monstey_Primitive::Sleep: ref_backend.sleep = {}; break;
    case Monstey_Primitive::Random: ref_backend.random = {}; break;
    case Monstey_Primitive::Initialize: ref_backend.initialize = {}; break;
    case Monstey_Primitive::BackendEnter: ref_backend.enter = {}; break;
    case Monstey_Primitive::BackendLeave: ref_backend.leave = {}; break;
    case Monstey_Primitive::Exchange: ref_backend.exchange = {}; break;
    case Monstey_Primitive::CompareExchange: ref_backend.lower.cas = {}; break;
    default: throw std::logic_error("invalid reference clear");
    }
}
}
