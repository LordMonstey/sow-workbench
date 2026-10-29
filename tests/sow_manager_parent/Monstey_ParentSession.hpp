#pragma once
#include "Monstey_ParentDifferential.hpp"
#include <stdexcept>

namespace Monstey_ParentDifferential
{
struct Monstey_ProviderFault final : std::runtime_error
{
    std::shared_ptr<const Monstey_Token> token;
    Monstey_U64 value;
    Monstey_ProviderFault(std::shared_ptr<const Monstey_Token> t, Monstey_U64 v);
};
struct Monstey_SharedState
{
    Monstey_U64 base;
    Monstey_Bytes bytes;
    std::vector<Monstey_Event> events;
    bool active = true, observer_failed = false;
};
struct Monstey_Retire
{
    std::shared_ptr<Monstey_SharedState> state;
    Monstey_U64 address, value;
    unsigned generation;
    Monstey_Retire(std::shared_ptr<Monstey_SharedState>, Monstey_U64, Monstey_U64, unsigned);
    Monstey_Retire(const Monstey_Retire &) = delete;
    ~Monstey_Retire();
};
struct Monstey_PrimitiveResult
{
    Monstey::Monstey_GadbBufferedResult raw;
    Monstey_U64 r8, r9;
};
class Monstey_Session
{
  public:
    const Monstey_Case &scenario;
    std::shared_ptr<Monstey_SharedState> state;
    bool reference;
    Monstey::Monstey_GadbManagerEnterProjectionServices enter;
    Monstey::Monstey_GadbManagerLeaveProjectionServices leave;
    Monstey::Monstey_GadbManagerBackendServices backend;
    Monstey_BackendReview::Monstey_Services ref_backend;
    Monstey::Monstey_GadbManagerBackendBindings caller_bindings;
    std::array<unsigned, 12> counts{}, generations{};
    explicit Monstey_Session(const Monstey_Case &, bool);
    ~Monstey_Session();
    Monstey::Monstey_GadbBinderMemory Monstey_Memory();
    Monstey::Monstey_GadbManagerParentResult Monstey_Execute(const Monstey_Request &);
    Monstey_PrimitiveResult Monstey_Call(Monstey::Monstey_GadbBinderMemory,
        Monstey_Primitive, unsigned serial, unsigned generation, std::vector<Monstey_U64>);
    void Monstey_Install(Monstey_Primitive);
    void Monstey_InstallCanonical(Monstey_Primitive, unsigned);
    void Monstey_InstallReference(Monstey_Primitive, unsigned);
    void Monstey_Clear(Monstey_Primitive);
    void Monstey_ClearCanonical(Monstey_Primitive);
    void Monstey_ClearReference(Monstey_Primitive);
    void Monstey_Effects(Monstey_Primitive, unsigned, bool);
};
std::vector<Monstey_U64> Monstey_Site(const Monstey::Monstey_GadbManagerBackendImportSite &);
std::vector<Monstey_U64> Monstey_Site(const Monstey_BackendReview::Monstey_Site &);
void Monstey_Add(std::vector<Monstey_U64> &, std::initializer_list<Monstey_U64>);
}
