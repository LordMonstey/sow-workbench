#pragma once
#include "Monstey_ParentReference.hpp"
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Monstey_ParentDifferential
{
using Monstey_U64 = std::uint64_t;
using Monstey_U32 = std::uint32_t;
using Monstey_Bytes = std::vector<std::uint8_t>;
using Monstey_Raw4 = std::array<Monstey_U64, 4>;

enum class Monstey_Primitive
{
    GuardEnter, GuardLeave, Fill, Allocate, SystemInfo, Sleep, Random,
    Initialize, BackendEnter, BackendLeave, Exchange, CompareExchange
};
enum class Monstey_Action
{
    Write32, Write64, Throw, ClearCurrent, ReplaceCurrent, ReplaceLeave, Reenter
};
enum class Monstey_FaultKind
{
    Memory, PrimitiveBinding, BackendImport, ExchangeBinding, CompareBinding,
    Provider, Unexpected
};
struct Monstey_Request
{
    Monstey_U64 manager = 0, entry_r8 = 0, backing = 0, body = 0;
    Monstey_U32 flags = 0;
};
struct Monstey_Effect
{
    Monstey_Primitive primitive;
    unsigned occurrence;
    bool after_default;
    Monstey_Action action;
    Monstey_U64 address = 0, value = 0;
};
struct Monstey_Token
{
    Monstey_U64 identity;
};
struct Monstey_ExpectedFault
{
    Monstey_FaultKind kind;
    std::optional<Monstey_U64> address;
    std::optional<std::size_t> width;
    std::optional<bool> writing;
};
struct Monstey_Case
{
    std::string id;
    std::vector<std::string> criteria;
    Monstey_U64 base = 0;
    Monstey_Bytes bytes;
    std::vector<Monstey_Request> requests;
    Monstey::Monstey_GadbManagerBackendBindings bindings;
    std::vector<Monstey_Primitive> missing;
    std::vector<Monstey_Effect> effects;
    Monstey::Monstey_GadbManagerEnterProjectionResult enter_out{};
    Monstey::Monstey_GadbManagerLeaveProjectionResult leave_out{};
    Monstey_U64 allocate_pointer = 0, allocate_rdx = 0, fill_rdx = 0, initialize_rdx = 0;
    Monstey_U64 backend_leave_rdx = 0;
    Monstey_U32 random_eax = 0;
    std::shared_ptr<const Monstey_Token> token;
    bool consume_higher = false;
    bool retire_enter = false;
    Monstey_U64 retire_address = 0, retire_value = 0;
    std::optional<Monstey_Request> nested;
    std::optional<Monstey_ExpectedFault> expected_fault;
    std::vector<Monstey_Raw4> expected_raw;
};
struct Monstey_Event
{
    Monstey_Primitive primitive;
    unsigned phase;
    std::vector<Monstey_U64> words;
    Monstey_Bytes image;
    bool operator==(const Monstey_Event &) const = default;
};
struct Monstey_Fault
{
    Monstey_FaultKind kind = Monstey_FaultKind::Unexpected;
    std::vector<Monstey_U64> words;
    std::string label;
    bool supplied_token_preserved = false;
    bool operator==(const Monstey_Fault &) const = default;
};
struct Monstey_Outcome
{
    std::vector<Monstey_Raw4> results;
    Monstey_Bytes image;
    std::vector<Monstey_Event> events;
    std::optional<Monstey_Fault> fault;
    bool observer_failed = false;
};

Monstey_Outcome Monstey_RunProduct(const Monstey_Case &);
Monstey_Outcome Monstey_RunReference(const Monstey_Case &);
bool Monstey_Equal(const Monstey_Outcome &, const Monstey_Outcome &);
bool Monstey_MeetsLiteralExpectations(const Monstey_Case &, const Monstey_Outcome &);
std::vector<Monstey_Case> Monstey_Cases();
void Monstey_Seed(Monstey_Case &, Monstey_U64, Monstey_U64, unsigned);
Monstey_U64 Monstey_Read(Monstey::Monstey_GadbBinderMemory, Monstey_U64, unsigned);
void Monstey_Write(Monstey::Monstey_GadbBinderMemory, Monstey_U64, Monstey_U64, unsigned);
}
