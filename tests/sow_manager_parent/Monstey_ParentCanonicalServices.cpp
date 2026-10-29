#include "Monstey_ParentSession.hpp"

namespace Monstey_ParentDifferential
{
void Monstey_Session::Monstey_InstallCanonical(Monstey_Primitive k, unsigned generation)
{
    using namespace Monstey;
    switch (k)
    {
    case Monstey_Primitive::Fill:
        backend.fill = [this, generation, serial = 0u](Monstey_GadbBinderMemory m, const Monstey_GadbManagerBackendFillCall &c) mutable
        {
            auto w = Monstey_Site(c.site); Monstey_Add(w, {c.destination, c.count, c.value});
            return Monstey_GadbManagerBackendRdxResult{Monstey_Call(m, Monstey_Primitive::Fill, ++serial, generation, std::move(w)).raw.rdx};
        }; break;
    case Monstey_Primitive::Allocate:
        backend.allocate = [this, generation, serial = 0u](Monstey_GadbBinderMemory m, const Monstey_GadbManagerBackendAllocateCall &c) mutable
        {
            auto w = Monstey_Site(c.site); Monstey_Add(w, {c.address, c.size, c.allocation_type, c.protection});
            return Monstey_Call(m, Monstey_Primitive::Allocate, ++serial, generation, std::move(w)).raw;
        }; break;
    case Monstey_Primitive::SystemInfo:
        backend.system_info = [this, generation, serial = 0u](Monstey_GadbBinderMemory m, const Monstey_GadbManagerBackendSystemInfoCall &c) mutable
        {
            auto w = Monstey_Site(c.site); Monstey_Add(w, {c.output});
            (void)Monstey_Call(m, Monstey_Primitive::SystemInfo, ++serial, generation, std::move(w));
        }; break;
    case Monstey_Primitive::Sleep:
        backend.sleep = [this, generation, serial = 0u](Monstey_GadbBinderMemory m, const Monstey_GadbManagerBackendSleepCall &c) mutable
        {
            auto w = Monstey_Site(c.site); Monstey_Add(w, {c.milliseconds, c.alertable});
            (void)Monstey_Call(m, Monstey_Primitive::Sleep, ++serial, generation, std::move(w));
        }; break;
    case Monstey_Primitive::Random:
        backend.random = [this, generation, serial = 0u](Monstey_GadbBinderMemory m, const Monstey_GadbManagerBackendImportSite &c) mutable
        {
            return static_cast<Monstey_U32>(Monstey_Call(m, Monstey_Primitive::Random, ++serial, generation, Monstey_Site(c)).raw.rax);
        }; break;
    case Monstey_Primitive::Initialize:
        backend.initialize_cs = [this, generation, serial = 0u](Monstey_GadbBinderMemory m, const Monstey_GadbManagerBackendInitializeCsCall &c) mutable
        {
            auto w = Monstey_Site(c.site); Monstey_Add(w, {c.critical_section, c.spin});
            return Monstey_GadbManagerBackendRdxResult{Monstey_Call(m, Monstey_Primitive::Initialize, ++serial, generation, std::move(w)).raw.rdx};
        }; break;
    case Monstey_Primitive::BackendEnter:
        backend.enter_cs = [this, generation, serial = 0u](Monstey_GadbBinderMemory m, const Monstey_GadbManagerBackendSyncCall &c) mutable
        {
            auto w = Monstey_Site(c.site); Monstey_Add(w, {c.critical_section});
            (void)Monstey_Call(m, Monstey_Primitive::BackendEnter, ++serial, generation, std::move(w));
        }; break;
    case Monstey_Primitive::BackendLeave:
        backend.leave_cs = [this, generation, serial = 0u](Monstey_GadbBinderMemory m, const Monstey_GadbManagerBackendSyncCall &c) mutable
        {
            auto w = Monstey_Site(c.site); Monstey_Add(w, {c.critical_section});
            return Monstey_GadbManagerBackendRdxResult{Monstey_Call(m, Monstey_Primitive::BackendLeave, ++serial, generation, std::move(w)).raw.rdx};
        }; break;
    case Monstey_Primitive::Exchange:
        backend.exchange32 = [this, generation, serial = 0u](Monstey_GadbBinderMemory m, const Monstey_GadbManagerBackendExchange32Call &c) mutable
        {
            return static_cast<Monstey_U32>(Monstey_Call(m, Monstey_Primitive::Exchange, ++serial, generation, {c.native_site, c.address, c.value}).raw.rax);
        }; break;
    case Monstey_Primitive::CompareExchange:
        backend.placement.compare_exchange32 = [this, generation, serial = 0u](Monstey_GadbBinderMemory m, const Monstey_GadbManagerCompareExchange32Call &c) mutable
        {
            return static_cast<Monstey_U32>(Monstey_Call(m, Monstey_Primitive::CompareExchange, ++serial, generation, {c.native_site, c.address, c.comparand, c.desired}).raw.rax);
        }; break;
    default: throw std::logic_error("invalid canonical primitive binding");
    }
}
void Monstey_Session::Monstey_ClearCanonical(Monstey_Primitive k)
{
    switch (k)
    {
    case Monstey_Primitive::Fill: backend.fill = {}; break;
    case Monstey_Primitive::Allocate: backend.allocate = {}; break;
    case Monstey_Primitive::SystemInfo: backend.system_info = {}; break;
    case Monstey_Primitive::Sleep: backend.sleep = {}; break;
    case Monstey_Primitive::Random: backend.random = {}; break;
    case Monstey_Primitive::Initialize: backend.initialize_cs = {}; break;
    case Monstey_Primitive::BackendEnter: backend.enter_cs = {}; break;
    case Monstey_Primitive::BackendLeave: backend.leave_cs = {}; break;
    case Monstey_Primitive::Exchange: backend.exchange32 = {}; break;
    case Monstey_Primitive::CompareExchange: backend.placement.compare_exchange32 = {}; break;
    default: throw std::logic_error("invalid canonical clear");
    }
}
}
