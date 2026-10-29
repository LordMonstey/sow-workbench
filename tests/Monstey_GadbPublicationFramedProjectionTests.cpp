#include "Monstey_GadbPublicationFramed.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace Monstey;
enum class Monstey_Entry {
    Monstey_Publish, Monstey_Copy, Monstey_Assign,
    Monstey_ReleasePair, Monstey_Primary, Monstey_Secondary
};
enum class Monstey_FaultKind {
    Monstey_None, Monstey_Range, Monstey_Boundary,
    Monstey_Provider, Monstey_Unexpected, Monstey_Any
};
enum class Monstey_EventKind { Monstey_Atomic, Monstey_Virtual, Monstey_Trap };
struct Monstey_Event {
    Monstey_EventKind kind = Monstey_EventKind::Monstey_Atomic;
    std::array<std::uint64_t, 12> words{};
    bool operator==(const Monstey_Event&) const = default;
};
struct Monstey_Fault {
    Monstey_FaultKind kind = Monstey_FaultKind::Monstey_None;
    std::array<std::uint64_t, 12> words{};
    std::string text;
    bool operator==(const Monstey_Fault&) const = default;
};
struct Monstey_Outcome {
    bool returned = false;
    Monstey_GadbBufferedResult raw{0, 0};
    Monstey_Fault fault;
};
struct Monstey_ExpectedRange {
    std::uint64_t address;
    std::size_t width;
    bool writing;
};
struct Monstey_Postcondition {
    std::uint64_t address;
    std::size_t width;
    std::uint64_t value;
};
struct Monstey_Case {
    std::string name;
    Monstey_Entry entry = Monstey_Entry::Monstey_Publish;
    std::vector<std::uint8_t> image;
    std::size_t view_size = 0;
    std::uint64_t base = 0, database = 0, body = 0, first = 0, second = 0;
    std::uint64_t third = 0, bank = 0, effect = 0, replacement_table = 0;
    std::uint32_t h8 = 0xA1567823U, h9 = 0xB9876504U;
    Monstey_GadbBufferedResult incoming{0xF123456789ABCDE0ULL, 0xD0FEDCBA98765432ULL};
    bool atomic_bound = true, virtual_bound = true, trap_bound = true;
    std::size_t throw_atomic = 0, throw_virtual = 0;
    bool atomic_throw_after = false, virtual_throw_after = false;
    bool stamp_virtual = false, stamp_trap = false, trap_throws = false;
    bool retarget_primary = false, mutate_late_homes = false;
    Monstey_FaultKind expected = Monstey_FaultKind::Monstey_None;
    std::optional<Monstey_ExpectedRange> expected_range;
    std::optional<Monstey_GadbPublicationBoundaryKind> expected_boundary;
    std::optional<std::array<std::size_t, 3>> expected_counts;
    std::optional<Monstey_GadbBufferedResult> expected_raw;
    std::vector<Monstey_Postcondition> postconditions;
};
struct Monstey_State {
    std::vector<Monstey_Event> events;
    std::size_t atomic_count = 0, virtual_count = 0, trap_count = 0;
    std::size_t atomic_commits = 0;
};
struct Monstey_Report {
    std::size_t cases = 0, checks = 0;
    std::array<std::size_t, 6> entries{};
    std::array<std::size_t, 3> events{};
    std::vector<std::pair<std::string, std::string>> failures;
};
class Monstey_ProviderException final : public std::runtime_error {
public:
    std::uint64_t stage, token;
    Monstey_ProviderException(std::uint64_t stage_value, std::uint64_t token_value)
        : std::runtime_error("Monstey projection provider exception"),
          stage(stage_value), token(token_value) {}
};
void Monstey_CheckRange(Monstey_GadbBinderMemory memory, std::uint64_t address,
                        std::size_t width, bool writing) {
    if (address < memory.base) {
        throw Monstey_GadbBufferedMemoryFault(address, width, writing);
    }
    const auto delta = address - memory.base;
    if (delta > static_cast<std::uint64_t>(memory.bytes.size())) {
        throw Monstey_GadbBufferedMemoryFault(address, width, writing);
    }
    const auto offset = static_cast<std::size_t>(delta);
    if (width > memory.bytes.size() - offset) {
        throw Monstey_GadbBufferedMemoryFault(address, width, writing);
    }
}
std::uint64_t Monstey_Read(Monstey_GadbBinderMemory memory,
                          std::uint64_t address, std::size_t width) {
    Monstey_CheckRange(memory, address, width, false);
    const auto offset = static_cast<std::size_t>(address - memory.base);
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < width; ++index) {
        value |= static_cast<std::uint64_t>(memory.bytes[offset + index])
            << (index * 8U);
    }
    return value;
}
void Monstey_Write(Monstey_GadbBinderMemory memory, std::uint64_t address,
                   std::size_t width, std::uint64_t value) {
    Monstey_CheckRange(memory, address, width, true);
    const auto offset = static_cast<std::size_t>(address - memory.base);
    for (std::size_t index = 0; index < width; ++index) {
        memory.bytes[offset + index] =
            static_cast<std::uint8_t>(value >> (index * 8U));
    }
}
void Monstey_Store(Monstey_Case& test, std::uint64_t address,
                   std::size_t width, std::uint64_t value) {
    Monstey_Write({test.base, std::span<std::uint8_t>(test.image)},
                  address, width, value);
}
void Monstey_Clear(Monstey_Case& test, std::uint64_t address, std::size_t width) {
    Monstey_CheckRange({test.base, std::span<std::uint8_t>(test.image)},
                       address, width, true);
    const auto offset = static_cast<std::size_t>(address - test.base);
    std::fill_n(test.image.begin() + static_cast<std::ptrdiff_t>(offset),
                width, static_cast<std::uint8_t>(0));
}
Monstey_Case Monstey_Seed(std::uint64_t base = 0x100000000ULL) {
    Monstey_Case test;
    test.base = base;
    test.image.resize(0x8000);
    test.view_size = test.image.size();
    for (std::size_t index = 0; index < test.image.size(); ++index) {
        test.image[index] = static_cast<std::uint8_t>((index * 37U + 11U) & 0xFFU);
    }
    test.database = base + 0x1000;
    test.body = base + 0x2000;
    test.first = base + 0x3000;
    test.second = 0x1122334455667788ULL;
    test.third = base + 0x4000;
    test.bank = 0x0123456789ABCDEFULL;
    test.effect = base + 0x7000;
    test.replacement_table = base + 0x6100;
    for (const auto& area : std::array<std::pair<std::uint64_t, std::size_t>, 10>{{
             {test.database, 0x88}, {test.body, 0xB0},
             {base + 0x3000, 0x20}, {base + 0x3500, 0x20},
             {base + 0x4000, 0x20}, {base + 0x4100, 0x20},
             {base + 0x5000, 0x10}, {base + 0x5100, 0x10},
             {base + 0x6000, 0x20}, {base + 0x6100, 0x20}}}) {
        Monstey_Clear(test, area.first, area.second);
    }
    Monstey_Store(test, test.body + 0x60, 4, 0x12345678U);
    Monstey_Store(test, test.body + 0x68, 4, 0x9ABCDEF0U);
    Monstey_Store(test, test.body + 0x70, 8, base + 0x3100);
    Monstey_Store(test, test.body + 0x78, 8, base + 0x3000);
    Monstey_Store(test, test.body + 0x80, 8, base + 0x3200);
    Monstey_Store(test, test.body + 0x88, 8, base + 0x3300);
    Monstey_Store(test, test.body + 0x90, 4, 0x81726354U);
    Monstey_Store(test, test.body + 0x98, 4, 0xFEDCBA98U);
    Monstey_Store(test, test.body + 0xA0, 8, base + 0x3400);
    Monstey_Store(test, test.body + 0xA8, 8, base + 0x5000);
    Monstey_Store(test, base + 0x3000, 8, 0xE011223344556677ULL);
    Monstey_Store(test, base + 0x3008, 8, base + 0x4000);
    for (const std::uint64_t control : {base + 0x4000, base + 0x4100}) {
        Monstey_Store(test, control, 8, base + 0x6000);
        Monstey_Store(test, control + 8, 4, 2);
        Monstey_Store(test, control + 12, 4, 2);
    }
    Monstey_Store(test, base + 0x6000, 8, 0x140AABBCCULL);
    Monstey_Store(test, base + 0x6008, 8, 0x140DDEEFFULL);
    Monstey_Store(test, base + 0x6100, 8, 0x141234567ULL);
    Monstey_Store(test, base + 0x6108, 8, 0);
    return test;
}
Monstey_Case Monstey_Helper(Monstey_Entry entry) {
    auto test = Monstey_Seed();
    test.entry = entry;
    switch (entry) {
    case Monstey_Entry::Monstey_Copy:
        test.first = test.database + 0x58;
        test.second = test.base + 0x3000;
        break;
    case Monstey_Entry::Monstey_Assign:
        test.first = test.database + 0x58;
        test.second = 0xA010203040506070ULL;
        test.third = test.base + 0x4000;
        break;
    case Monstey_Entry::Monstey_ReleasePair:
        test.first = test.base + 0x3000;
        break;
    case Monstey_Entry::Monstey_Primary:
    case Monstey_Entry::Monstey_Secondary:
        test.first = test.base + 0x4000;
        break;
    case Monstey_Entry::Monstey_Publish:
        break;
    }
    return test;
}
std::uint32_t Monstey_AtomicProvider(Monstey_GadbBinderMemory memory,
    const Monstey_GadbPublicationAtomicCall& call, Monstey_State& state,
    const Monstey_Case& test) {
    ++state.atomic_count;
    Monstey_Event event;
    event.kind = Monstey_EventKind::Monstey_Atomic;
    event.words = {call.native_site, call.address, call.addend,
        static_cast<std::uint64_t>(call.operation), 0, 0, 0, 0, 0, 0, 0, 0};
    state.events.push_back(event);
    if (test.throw_atomic == state.atomic_count && !test.atomic_throw_after) {
        throw Monstey_ProviderException(1, static_cast<std::uint64_t>(state.atomic_count));
    }
    const auto old = static_cast<std::uint32_t>(Monstey_Read(memory, call.address, 4));
    state.events.back().words[4] = old;
    Monstey_Write(memory, call.address, 4,
        static_cast<std::uint32_t>(old + call.addend));
    ++state.atomic_commits;
    if (test.throw_atomic == state.atomic_count && test.atomic_throw_after) {
        throw Monstey_ProviderException(2, static_cast<std::uint64_t>(state.atomic_count));
    }
    return old;
}
Monstey_GadbBufferedResult Monstey_VirtualProvider(Monstey_GadbBinderMemory memory,
    const Monstey_GadbPublicationVirtualCall& call, Monstey_State& state,
    const Monstey_Case& test) {
    ++state.virtual_count;
    Monstey_Event event;
    event.kind = Monstey_EventKind::Monstey_Virtual;
    event.words = {call.native_site, call.this_identity, call.vtable_identity,
        call.slot_identity, call.target_identity, call.incoming.rax,
        call.incoming.rdx, 0, 0, 0, 0, 0};
    state.events.push_back(event);
    if (test.throw_virtual == state.virtual_count && !test.virtual_throw_after) {
        throw Monstey_ProviderException(3, static_cast<std::uint64_t>(state.virtual_count));
    }
    if (test.stamp_virtual) {
        Monstey_Write(memory, test.effect, 8,
            0xE115000000000000ULL | static_cast<std::uint64_t>(state.virtual_count));
    }
    if (test.retarget_primary && call.native_site == 0x1403DD6FBULL) {
        Monstey_Write(memory, call.this_identity, 8, test.replacement_table);
        Monstey_Write(memory, call.this_identity + 12, 4, 1);
    }
    if (test.mutate_late_homes && state.virtual_count == 1) {
        Monstey_Write(memory, test.body + 0x68, 4, 0xA10B20C3U);
        Monstey_Write(memory, test.body + 0x78, 8, test.base + 0x3500);
        Monstey_Write(memory, test.body + 0x80, 8, test.base + 0x3600);
        Monstey_Write(memory, test.body + 0x88, 8, test.base + 0x3700);
        Monstey_Write(memory, test.body + 0x90, 4, 0xC0987654U);
        Monstey_Write(memory, test.body + 0x98, 4, 0xB0123456U);
        Monstey_Write(memory, test.body + 0xA0, 8, test.base + 0x3800);
        Monstey_Write(memory, test.body + 0xA8, 8, test.base + 0x5100);
    }
    if (test.throw_virtual == state.virtual_count && test.virtual_throw_after) {
        throw Monstey_ProviderException(4, static_cast<std::uint64_t>(state.virtual_count));
    }
    return {0xA177000000000000ULL ^ call.target_identity
                ^ static_cast<std::uint64_t>(state.virtual_count),
            0xD299000000000000ULL ^ call.incoming.rdx ^ call.this_identity
                ^ static_cast<std::uint64_t>(state.virtual_count)};
}
void Monstey_TrapProvider(Monstey_GadbBinderMemory memory,
    const Monstey_GadbPublicationTrapCall& call, Monstey_State& state,
    const Monstey_Case& test) {
    ++state.trap_count;
    Monstey_Event event;
    event.kind = Monstey_EventKind::Monstey_Trap;
    event.words = {call.call_site, call.trap_entry, call.zero_store_site,
        call.message_identity, call.address, call.value,
        call.incoming.rax, call.incoming.rdx, 0, 0, 0, 0};
    state.events.push_back(event);
    if (test.stamp_trap) {
        Monstey_Write(memory, test.effect, 8, 0x7A4F000000000001ULL);
    }
    if (test.trap_throws) {
        throw Monstey_ProviderException(5, static_cast<std::uint64_t>(state.trap_count));
    }
}
void Monstey_InstallGeneric(const Monstey_Case& test, Monstey_State& state,
                           Monstey_GadbPublicationServices& services) {
    if (test.atomic_bound) {
        services.atomic32 = [&test, &state](Monstey_GadbBinderMemory memory,
            const Monstey_GadbPublicationAtomicCall& call) {
            return Monstey_AtomicProvider(memory, call, state, test);
        };
    }
    if (test.virtual_bound) {
        services.virtual_call = [&test, &state](Monstey_GadbBinderMemory memory,
            const Monstey_GadbPublicationVirtualCall& call) {
            return Monstey_VirtualProvider(memory, call, state, test);
        };
    }
    if (test.trap_bound) {
        services.trap = [&test, &state](Monstey_GadbBinderMemory memory,
            const Monstey_GadbPublicationTrapCall& call) {
            Monstey_TrapProvider(memory, call, state, test);
        };
    }
}
void Monstey_InstallFramed(const Monstey_Case& test, Monstey_State& state,
                          Monstey_GadbPublicationFramedServices& services) {
    if (test.atomic_bound) {
        services.atomic32 = [&test, &state](Monstey_GadbBinderMemory memory,
            const Monstey_GadbPublicationAtomicCall& call) {
            return Monstey_AtomicProvider(memory, call, state, test);
        };
    }
    if (test.virtual_bound) {
        services.virtual_call = [&test, &state](Monstey_GadbBinderMemory memory,
            const Monstey_GadbPublicationFramedVirtualCall& call) {
            // Projection consumes the generic DTO only, never caller_body_rsp.
            return Monstey_VirtualProvider(memory, call.call, state, test);
        };
    }
    if (test.trap_bound) {
        services.trap = [&test, &state](Monstey_GadbBinderMemory memory,
            const Monstey_GadbPublicationTrapCall& call) {
            Monstey_TrapProvider(memory, call, state, test);
        };
    }
}

Monstey_GadbBufferedResult Monstey_InvokeGeneric(Monstey_GadbBinderMemory memory,
    const Monstey_Case& test, const Monstey_GadbPublicationServices& services) {
    switch (test.entry) {
    case Monstey_Entry::Monstey_Publish:
        return Monstey_GadbPublishDatabaseStorage(memory, test.database,
            test.h8, test.h9, test.bank, test.body, services);
    case Monstey_Entry::Monstey_Copy:
        return Monstey_GadbCopyPublicationPair(memory, test.first, test.second,
            test.incoming, services);
    case Monstey_Entry::Monstey_Assign:
        return Monstey_GadbAssignPublicationPair(memory, test.first, test.second,
            test.third, test.incoming, services);
    case Monstey_Entry::Monstey_ReleasePair:
        return Monstey_GadbReleasePublicationPair(memory, test.first,
            test.incoming, services);
    case Monstey_Entry::Monstey_Primary:
        return Monstey_GadbReleasePublicationPrimary(memory, test.first,
            test.incoming, services);
    case Monstey_Entry::Monstey_Secondary:
        return Monstey_GadbReleasePublicationSecondary(memory, test.first,
            test.incoming, services);
    }
    throw std::logic_error("Monstey invalid projection entry");
}
Monstey_GadbBufferedResult Monstey_InvokeFramed(Monstey_GadbBinderMemory memory,
    const Monstey_Case& test, const Monstey_GadbPublicationFramedServices& services) {
    switch (test.entry) {
    case Monstey_Entry::Monstey_Publish:
        return Monstey_GadbPublishDatabaseStorageFramed(memory, test.database,
            test.h8, test.h9, test.bank, test.body, services);
    case Monstey_Entry::Monstey_Copy:
        return Monstey_GadbCopyPublicationPairFramed(memory, test.first, test.second,
            test.body, test.incoming, services);
    case Monstey_Entry::Monstey_Assign:
        return Monstey_GadbAssignPublicationPairFramed(memory, test.first, test.second,
            test.third, test.body, test.incoming, services);
    case Monstey_Entry::Monstey_ReleasePair:
        return Monstey_GadbReleasePublicationPairFramed(memory, test.first,
            test.body, test.incoming, services);
    case Monstey_Entry::Monstey_Primary:
        return Monstey_GadbReleasePublicationPrimaryFramed(memory, test.first,
            test.body, test.incoming, services);
    case Monstey_Entry::Monstey_Secondary:
        return Monstey_GadbReleasePublicationSecondaryFramed(memory, test.first,
            test.body, test.incoming, services);
    }
    throw std::logic_error("Monstey invalid framed projection entry");
}
template<class Monstey_Function>
Monstey_Outcome Monstey_Capture(Monstey_Function&& function) {
    Monstey_Outcome outcome;
    try {
        outcome.raw = std::forward<Monstey_Function>(function)();
        outcome.returned = true;
    } catch (const Monstey_GadbBufferedMemoryFault& fault) {
        outcome.fault.kind = Monstey_FaultKind::Monstey_Range;
        outcome.fault.words[0] = fault.address;
        outcome.fault.words[1] = static_cast<std::uint64_t>(fault.width);
        outcome.fault.words[2] = fault.writing ? 1U : 0U;
        outcome.fault.text = fault.what();
    } catch (const Monstey_GadbPublicationBoundaryFault& fault) {
        outcome.fault.kind = Monstey_FaultKind::Monstey_Boundary;
        outcome.fault.words = {static_cast<std::uint64_t>(fault.kind),
            fault.native_site, fault.call_site, fault.address, fault.width,
            fault.slot, fault.target, fault.message_identity, 0, 0, 0, 0};
        outcome.fault.text = fault.what();
    } catch (const Monstey_ProviderException& fault) {
        outcome.fault.kind = Monstey_FaultKind::Monstey_Provider;
        outcome.fault.words[0] = fault.stage;
        outcome.fault.words[1] = fault.token;
        outcome.fault.text = fault.what();
    } catch (const std::exception& fault) {
        outcome.fault.kind = Monstey_FaultKind::Monstey_Unexpected;
        outcome.fault.text = fault.what();
    } catch (...) {
        outcome.fault.kind = Monstey_FaultKind::Monstey_Unexpected;
        outcome.fault.text = "non-standard exception";
    }
    return outcome;
}
void Monstey_Check(Monstey_Report& report, const std::string& name,
                   bool passed, const char* check) {
    ++report.checks;
    if (!passed) {
        report.failures.emplace_back(name, check);
    }
}
void Monstey_Run(const Monstey_Case& test, Monstey_Report& report) {
    ++report.cases;
    ++report.entries[static_cast<std::size_t>(test.entry)];
    auto generic_image = test.image;
    auto framed_image = test.image;
    Monstey_GadbBinderMemory generic_memory{
        test.base, std::span<std::uint8_t>(generic_image).first(test.view_size)};
    Monstey_GadbBinderMemory framed_memory{
        test.base, std::span<std::uint8_t>(framed_image).first(test.view_size)};
    Monstey_State generic_state, framed_state;
    Monstey_GadbPublicationServices generic_services;
    Monstey_GadbPublicationFramedServices framed_services;
    Monstey_InstallGeneric(test, generic_state, generic_services);
    Monstey_InstallFramed(test, framed_state, framed_services);
    const auto generic = Monstey_Capture([&] {
        return Monstey_InvokeGeneric(generic_memory, test, generic_services);
    });
    const auto framed = Monstey_Capture([&] {
        return Monstey_InvokeFramed(framed_memory, test, framed_services);
    });
    Monstey_Check(report, test.name, generic.returned == framed.returned,
                  "return_or_exception");
    Monstey_Check(report, test.name, generic.fault == framed.fault,
                  "first_fault_all_metadata_and_exception_payload");
    Monstey_Check(report, test.name, generic_image == framed_image,
                  "full_backing_bytes_including_committed_prefix_and_unexposed_tail");
    Monstey_Check(report, test.name, generic_state.events == framed_state.events,
                  "ordered_atomic_virtual_trap_events_with_incoming_raw_state");
    Monstey_Check(report, test.name,
        generic_state.atomic_commits == framed_state.atomic_commits,
        "atomic_committed_prefix");
    Monstey_Check(report, test.name,
        generic.fault.kind != Monstey_FaultKind::Monstey_Unexpected,
        "baseline_reached_declared_return_or_fault");
    if (generic.returned && framed.returned) {
        Monstey_Check(report, test.name, generic.raw.rax == framed.raw.rax, "raw_RAX");
        Monstey_Check(report, test.name, generic.raw.rdx == framed.raw.rdx, "raw_RDX");
    }
    if (test.expected != Monstey_FaultKind::Monstey_Any) {
        Monstey_Check(report, test.name, generic.fault.kind == test.expected,
                      "fixture_expected_baseline_outcome");
    }
    if (test.expected_range) {
        const auto& range = *test.expected_range;
        Monstey_Check(report, test.name,
            generic.fault.kind == Monstey_FaultKind::Monstey_Range
                && generic.fault.words[0] == range.address
                && generic.fault.words[1] == static_cast<std::uint64_t>(range.width)
                && generic.fault.words[2] == (range.writing ? 1U : 0U),
            "fixture_exact_first_consumed_range");
    }
    if (test.expected_boundary) {
        Monstey_Check(report, test.name,
            generic.fault.kind == Monstey_FaultKind::Monstey_Boundary
                && generic.fault.words[0] ==
                    static_cast<std::uint64_t>(*test.expected_boundary),
            "fixture_exact_boundary_kind");
    }
    const std::array<std::size_t, 3> counts{
        generic_state.atomic_count, generic_state.virtual_count, generic_state.trap_count};
    if (test.expected_counts) {
        Monstey_Check(report, test.name, counts == *test.expected_counts,
                      "fixture_exact_consumed_event_counts");
    }
    for (std::size_t index = 0; index < counts.size(); ++index) {
        report.events[index] += counts[index];
    }
    if (test.expected_raw && generic.returned) {
        Monstey_Check(report, test.name,
            generic.raw.rax == test.expected_raw->rax
                && generic.raw.rdx == test.expected_raw->rdx,
            "fixture_explicit_raw_return");
    }
    for (const auto& post : test.postconditions) {
        const Monstey_GadbBinderMemory complete{
            test.base, std::span<std::uint8_t>(generic_image)};
        Monstey_Check(report, test.name,
            Monstey_Read(complete, post.address, post.width) == post.value,
            "fixture_committed_prefix_bytes");
    }
    if (test.retarget_primary && generic.returned) {
        bool observed = false;
        for (const auto& event : generic_state.events) {
            observed = observed || (event.kind == Monstey_EventKind::Monstey_Virtual
                && event.words[0] == 0x1403DD720ULL
                && event.words[2] == test.replacement_table
                && event.words[3] == test.replacement_table + 8
                && event.words[4] == 0);
        }
        Monstey_Check(report, test.name, observed,
                      "live_reloaded_secondary_vptr_and_zero_target_consumed");
    }
}
void Monstey_Add(std::vector<Monstey_Case>& cases, Monstey_Case test,
                 const std::string& name) {
    test.name = name;
    cases.push_back(std::move(test));
}

std::vector<Monstey_Case> Monstey_Cases() {
    std::vector<Monstey_Case> cases;
    auto test = Monstey_Seed();
    test.expected_counts = std::array<std::size_t, 3>{2, 0, 0};
    Monstey_Add(cases, test, "publish_ordinary_counted_copy_and_final_release");
    test = Monstey_Seed();
    Monstey_Store(test, test.database + 0x58, 8, 0xAA55CC33EE119977ULL);
    Monstey_Store(test, test.database + 0x60, 8, test.base + 0x4100);
    Monstey_Store(test, test.base + 0x4108, 4, 1);
    Monstey_Store(test, test.base + 0x410C, 4, 1);
    test.expected_counts = std::array<std::size_t, 3>{4, 2, 0};
    Monstey_Add(cases, test, "publish_old_pool_primary_and_secondary_virtuals");
    test.mutate_late_homes = true;
    test.expected_counts = std::array<std::size_t, 3>{3, 2, 0};
    test.postconditions = {{test.database + 0x08, 8, test.base + 0x3200},
        {test.database + 0x10, 8, test.base + 0x3700},
        {test.database + 0x30, 4, 0xC0987654U},
        {test.database + 0x34, 4, 0xB0123456U},
        {test.database + 0x40, 8, test.base + 0x3800},
        {test.database + 0x4C, 4, 0xA10B20C3U}};
    Monstey_Add(cases, test, "publish_virtual_effect_changes_late_homes_and_pair");

    for (const auto entry : {Monstey_Entry::Monstey_Copy,
             Monstey_Entry::Monstey_Assign, Monstey_Entry::Monstey_ReleasePair}) {
        test = Monstey_Helper(entry);
        if (entry != Monstey_Entry::Monstey_Assign) {
            Monstey_Store(test, test.base + 0x3008, 8, 0);
        }
        test.atomic_bound = false;
        test.virtual_bound = false;
        test.trap_bound = false;
        test.body = std::numeric_limits<std::uint64_t>::max();
        test.expected_counts = std::array<std::size_t, 3>{0, 0, 0};
        if (entry == Monstey_Entry::Monstey_ReleasePair) {
            test.expected_raw = test.incoming;
        } else if (entry == Monstey_Entry::Monstey_Copy) {
            test.expected_raw = Monstey_GadbBufferedResult{
                test.first, 0xE011223344556677ULL};
        } else {
            test.third = 0;
            test.expected_raw = Monstey_GadbBufferedResult{
                test.incoming.rax, test.second};
        }
        Monstey_Add(cases, test, "null_control_lazy_services_and_unused_frame_entry_"
            + std::to_string(static_cast<unsigned int>(entry)));
    }
    for (const std::uint32_t old : {0U, 1U, 2U, 0xFFFFFFFFU}) {
        for (const auto entry : {Monstey_Entry::Monstey_Primary,
                 Monstey_Entry::Monstey_Secondary}) {
            test = Monstey_Helper(entry);
            const auto counter = test.first
                + (entry == Monstey_Entry::Monstey_Primary ? 8ULL : 12ULL);
            Monstey_Store(test, counter, 4, old);
            Monstey_Store(test, test.first + 12, 4,
                entry == Monstey_Entry::Monstey_Primary ? 1U : old);
            const auto virtuals = old == 1
                ? (entry == Monstey_Entry::Monstey_Primary ? 2U : 1U) : 0U;
            const auto atomics = old == 1
                && entry == Monstey_Entry::Monstey_Primary ? 2U : 1U;
            test.expected_counts = std::array<std::size_t, 3>{atomics, virtuals, 0};
            test.postconditions.push_back({counter, 4,
                static_cast<std::uint32_t>(old - 1U)});
            if (old != 1) {
                test.expected_raw = Monstey_GadbBufferedResult{old, test.incoming.rdx};
            }
            test.body = old == 0 ? 3ULL
                : std::numeric_limits<std::uint64_t>::max() - old;
            Monstey_Add(cases, test, "direct_release_old_" + std::to_string(old)
                + "_entry_" + std::to_string(static_cast<unsigned int>(entry)));
        }
        test = Monstey_Helper(Monstey_Entry::Monstey_Copy);
        Monstey_Store(test, test.base + 0x4008, 4, old);
        test.body = static_cast<std::uint64_t>(old);
        test.expected_counts = std::array<std::size_t, 3>{1, 0, 0};
        test.postconditions.push_back({test.base + 0x4008, 4,
            static_cast<std::uint32_t>(old + 1U)});
        Monstey_Add(cases, test, "copy_increment_modular_old_" + std::to_string(old));

        test = Monstey_Helper(Monstey_Entry::Monstey_Assign);
        Monstey_Store(test, test.first + 8, 8, test.base + 0x4100);
        Monstey_Store(test, test.base + 0x4108, 4, old);
        Monstey_Store(test, test.base + 0x410C, 4, 1);
        test.expected_counts = old == 1
            ? std::array<std::size_t, 3>{2, 2, 0}
            : std::array<std::size_t, 3>{1, 0, 0};
        test.postconditions = {{test.first, 8, test.second},
            {test.first + 8, 8, test.third},
            {test.base + 0x4108, 4, static_cast<std::uint32_t>(old - 1U)}};
        Monstey_Add(cases, test,
            "assign_cached_pair_after_release_old_" + std::to_string(old));

        test = Monstey_Helper(Monstey_Entry::Monstey_ReleasePair);
        Monstey_Store(test, test.base + 0x4008, 4, old);
        Monstey_Store(test, test.base + 0x400C, 4, 1);
        test.expected_counts = old == 1
            ? std::array<std::size_t, 3>{2, 2, 0}
            : std::array<std::size_t, 3>{1, 0, 0};
        test.postconditions = {{test.first, 8, 0xE011223344556677ULL},
            {test.first + 8, 8, test.base + 0x4000}};
        Monstey_Add(cases, test, "release_pair_preserves_fields_old_" + std::to_string(old));
    }
    test = Monstey_Helper(Monstey_Entry::Monstey_Copy);
    test.first = test.second;
    test.expected_counts = std::array<std::size_t, 3>{0, 0, 0};
    test.postconditions = {{test.base + 0x4008, 4, 2}};
    Monstey_Add(cases, test, "copy_self_alias_destination_zeroed_before_source_reads");
    test = Monstey_Helper(Monstey_Entry::Monstey_Copy);
    test.first = test.second + 8;
    test.expected_counts = std::array<std::size_t, 3>{0, 0, 0};
    Monstey_Add(cases, test, "copy_destination_source_plus8_zeroes_source_control");
    test = Monstey_Helper(Monstey_Entry::Monstey_Copy);
    test.first = test.base + 0x3000;
    test.second = test.first + 8;
    Monstey_Store(test, test.first + 8, 8, 0);
    Monstey_Store(test, test.first + 16, 8, test.base + 0x4000);
    test.expected_counts = std::array<std::size_t, 3>{1, 0, 0};
    Monstey_Add(cases, test, "copy_source_destination_plus8_overlap");
    test = Monstey_Helper(Monstey_Entry::Monstey_Copy);
    Monstey_Store(test, test.second, 8, 0x1122334400000001ULL);
    Monstey_Store(test, test.second + 8, 8, test.second - 8);
    test.expected_counts = std::array<std::size_t, 3>{1, 0, 0};
    test.postconditions = {{test.first, 8, 0x1122334400000001ULL}};
    Monstey_Add(cases, test, "copy_counter_aliases_source_data_cached_before_increment");
    test = Monstey_Helper(Monstey_Entry::Monstey_Assign);
    test.first = test.base + 0x4108;
    Monstey_Store(test, test.first + 8, 8, 0);
    test.expected_counts = std::array<std::size_t, 3>{0, 0, 0};
    test.postconditions = {{test.first, 8, test.second},
        {test.first + 8, 8, test.third}};
    Monstey_Add(cases, test, "assign_ordinary_data_store_aliases_counter_bytes");

    for (const auto entry : {Monstey_Entry::Monstey_Copy,
             Monstey_Entry::Monstey_Assign, Monstey_Entry::Monstey_ReleasePair}) {
        test = Monstey_Helper(entry);
        test.first = test.body + 0x20;
        if (entry == Monstey_Entry::Monstey_ReleasePair) {
            Monstey_Store(test, test.first, 8, 0xA1B2C3D4E5F67890ULL);
            Monstey_Store(test, test.first + 8, 8, test.base + 0x4000);
        }
        Monstey_Add(cases, test, "direct_pair_overlays_body_frame_entry_"
            + std::to_string(static_cast<unsigned int>(entry)));
    }
    test = Monstey_Helper(Monstey_Entry::Monstey_Copy);
    test.second = test.body + 0x20;
    Monstey_Store(test, test.second, 8, 0xA1B2C3D4E5F67890ULL);
    Monstey_Store(test, test.second + 8, 8, test.base + 0x4000);
    Monstey_Add(cases, test, "copy_source_pair_overlays_body_frame");
    test = Monstey_Seed();
    Monstey_Store(test, test.body + 0x78, 8, test.body + 0x20);
    Monstey_Store(test, test.body + 0x20, 8, 0x11223344ULL);
    Monstey_Store(test, test.body + 0x28, 8, test.base + 0x4000);
    test.expected_counts = std::array<std::size_t, 3>{0, 0, 0};
    Monstey_Add(cases, test, "publish_incoming_pair_is_actual_temporary_frame_pair");
    test = Monstey_Seed();
    Monstey_Store(test, test.body + 0x78, 8, test.database + 0x58);
    Monstey_Store(test, test.database + 0x58, 8, 0xC0123456789ABCDEULL);
    Monstey_Store(test, test.database + 0x60, 8, test.base + 0x4100);
    Monstey_Store(test, test.base + 0x4108, 4, 1);
    Monstey_Store(test, test.base + 0x410C, 4, 1);
    test.expected_counts = std::array<std::size_t, 3>{4, 2, 0};
    Monstey_Add(cases, test, "publish_incoming_pair_aliases_database_pool_pair");
    for (const std::uint64_t displacement : {0ULL, 0x10ULL, 0x20ULL, 0x28ULL,
             0x48ULL, 0x60ULL, 0x80ULL}) {
        test = Monstey_Seed();
        test.database = test.body - displacement;
        test.expected = Monstey_FaultKind::Monstey_Any;
        Monstey_Add(cases, test,
            "publish_database_frame_overlap_" + std::to_string(displacement));
    }
    test = Monstey_Helper(Monstey_Entry::Monstey_Primary);
    Monstey_Store(test, test.first + 8, 4, 1);
    Monstey_Store(test, test.first + 12, 4, 1);
    test.retarget_primary = true;
    test.expected_counts = std::array<std::size_t, 3>{2, 2, 0};
    Monstey_Add(cases, test, "primary_callback_retargets_live_secondary_slot_to_zero");
    for (const auto entry : {Monstey_Entry::Monstey_Primary,
             Monstey_Entry::Monstey_Secondary}) {
        test = Monstey_Helper(entry);
        Monstey_Store(test, test.first + 8, 4, 1);
        Monstey_Store(test, test.first + 12, 4, 1);
        Monstey_Store(test, test.base + 0x6000, 8, 0);
        Monstey_Store(test, test.base + 0x6008, 8, 0);
        test.body = 7;
        test.expected_counts = entry == Monstey_Entry::Monstey_Primary
            ? std::array<std::size_t, 3>{2, 2, 0}
            : std::array<std::size_t, 3>{1, 1, 0};
        Monstey_Add(cases, test, "zero_virtual_target_is_consumed_entry_"
            + std::to_string(static_cast<unsigned int>(entry)));
    }
    test = Monstey_Helper(Monstey_Entry::Monstey_Primary);
    test.first = test.base + 0x4001;
    test.body = 0x31;
    Monstey_Clear(test, test.first, 0x18);
    Monstey_Store(test, test.first, 8, test.base + 0x6001);
    Monstey_Store(test, test.first + 8, 4, 1);
    Monstey_Store(test, test.first + 12, 4, 1);
    Monstey_Store(test, test.base + 0x6001, 8, 0x171234567ULL);
    Monstey_Store(test, test.base + 0x6009, 8, 0);
    test.expected_counts = std::array<std::size_t, 3>{2, 2, 0};
    Monstey_Add(cases, test, "unaligned_numeric_control_vtable_and_body_are_not_preflighted");

    test = Monstey_Helper(Monstey_Entry::Monstey_Copy);
    test.second = test.base - 8;
    Monstey_Store(test, test.base, 8, test.base + 0x4000);
    test.expected = Monstey_FaultKind::Monstey_Range;
    test.expected_range = Monstey_ExpectedRange{test.second, 8, false};
    test.expected_counts = std::array<std::size_t, 3>{0, 0, 0};
    test.postconditions = {{test.base + 0x4008, 4, 2}};
    Monstey_Add(cases, test, "copy_source_data_fault_after_destination_zero_before_increment");
    test = Monstey_Helper(Monstey_Entry::Monstey_Copy);
    test.first = test.base + test.image.size() - 8;
    test.expected = Monstey_FaultKind::Monstey_Range;
    test.expected_range = Monstey_ExpectedRange{test.first + 8, 8, false};
    test.expected_counts = std::array<std::size_t, 3>{0, 0, 0};
    test.postconditions = {{test.base + 0x4008, 4, 2}};
    Monstey_Add(cases, test, "copy_destination_AND_fault_before_source_reads_and_increment");
    test = Monstey_Helper(Monstey_Entry::Monstey_Assign);
    test.first = test.base - 8;
    Monstey_Store(test, test.base, 8, 0);
    test.expected = Monstey_FaultKind::Monstey_Range;
    test.expected_range = Monstey_ExpectedRange{test.first, 8, true};
    test.expected_counts = std::array<std::size_t, 3>{0, 0, 0};
    Monstey_Add(cases, test, "assign_exact_data_write_fault_after_valid_old_control_read");
    test = Monstey_Helper(Monstey_Entry::Monstey_ReleasePair);
    test.first = test.base - 8;
    Monstey_Store(test, test.base, 8, test.base + 0x4000);
    test.expected_counts = std::array<std::size_t, 3>{1, 0, 0};
    Monstey_Add(cases, test, "release_pair_only_control_range_required_data_unavailable");
    test = Monstey_Helper(Monstey_Entry::Monstey_Primary);
    test.first = test.base - 8;
    Monstey_Store(test, test.base, 4, 1);
    test.expected = Monstey_FaultKind::Monstey_Range;
    test.expected_range = Monstey_ExpectedRange{test.first, 8, false};
    test.expected_counts = std::array<std::size_t, 3>{1, 0, 0};
    test.postconditions = {{test.base, 4, 0}};
    Monstey_Add(cases, test, "primary_vptr_first_fault_preserves_atomic_decrement");
    for (const auto entry : {Monstey_Entry::Monstey_Primary,
             Monstey_Entry::Monstey_Secondary}) {
        test = Monstey_Helper(entry);
        Monstey_Store(test, test.first + 8, 4, 1);
        Monstey_Store(test, test.first + 12, 4, 1);
        const auto table = test.base + test.image.size()
            - (entry == Monstey_Entry::Monstey_Primary ? 4ULL : 8ULL);
        Monstey_Store(test, test.first, 8, table);
        test.virtual_bound = false;
        test.expected = Monstey_FaultKind::Monstey_Range;
        test.expected_range = Monstey_ExpectedRange{
            table + (entry == Monstey_Entry::Monstey_Primary ? 0ULL : 8ULL), 8, false};
        test.expected_counts = std::array<std::size_t, 3>{1, 0, 0};
        Monstey_Add(cases, test, "live_target_range_fault_precedes_missing_virtual_entry_"
            + std::to_string(static_cast<unsigned int>(entry)));
    }
    for (const auto entry : {Monstey_Entry::Monstey_Copy,
             Monstey_Entry::Monstey_Primary, Monstey_Entry::Monstey_Secondary}) {
        test = Monstey_Helper(entry);
        test.atomic_bound = false;
        test.expected = Monstey_FaultKind::Monstey_Boundary;
        test.expected_boundary = Monstey_GadbPublicationBoundaryKind::missing_atomic32;
        test.expected_counts = std::array<std::size_t, 3>{0, 0, 0};
        Monstey_Add(cases, test, "missing_atomic_at_consumed_boundary_entry_"
            + std::to_string(static_cast<unsigned int>(entry)));
    }
    for (const auto entry : {Monstey_Entry::Monstey_Primary,
             Monstey_Entry::Monstey_Secondary}) {
        test = Monstey_Helper(entry);
        Monstey_Store(test, test.first + 8, 4, 1);
        Monstey_Store(test, test.first + 12, 4, 1);
        test.virtual_bound = false;
        test.expected = Monstey_FaultKind::Monstey_Boundary;
        test.expected_boundary = Monstey_GadbPublicationBoundaryKind::missing_virtual_call;
        test.expected_counts = std::array<std::size_t, 3>{1, 0, 0};
        Monstey_Add(cases, test, "missing_virtual_preserves_live_slot_target_metadata_entry_"
            + std::to_string(static_cast<unsigned int>(entry)));
    }
    test = Monstey_Helper(Monstey_Entry::Monstey_Copy);
    test.throw_atomic = 1;
    test.atomic_throw_after = true;
    test.expected = Monstey_FaultKind::Monstey_Provider;
    test.expected_counts = std::array<std::size_t, 3>{1, 0, 0};
    test.postconditions = {{test.base + 0x4008, 4, 3}};
    Monstey_Add(cases, test, "atomic_provider_partial_exception_keeps_increment");
    test = Monstey_Helper(Monstey_Entry::Monstey_Primary);
    test.throw_atomic = 1;
    test.expected = Monstey_FaultKind::Monstey_Provider;
    test.expected_counts = std::array<std::size_t, 3>{1, 0, 0};
    test.postconditions = {{test.first + 8, 4, 2}};
    Monstey_Add(cases, test, "atomic_provider_exception_before_commit");
    for (const std::size_t invocation : {1U, 2U}) {
        test = Monstey_Helper(Monstey_Entry::Monstey_Primary);
        Monstey_Store(test, test.first + 8, 4, 1);
        Monstey_Store(test, test.first + 12, 4, 1);
        test.stamp_virtual = true;
        test.throw_virtual = invocation;
        test.virtual_throw_after = true;
        test.expected = Monstey_FaultKind::Monstey_Provider;
        test.expected_counts = std::array<std::size_t, 3>{invocation, invocation, 0};
        test.postconditions = {{test.effect, 8,
            0xE115000000000000ULL | static_cast<std::uint64_t>(invocation)},
            {test.first + 8, 4, 0}};
        Monstey_Add(cases, test, "virtual_partial_exception_committed_prefix_"
            + std::to_string(invocation));
    }
    for (const bool equal_owner : {false, true}) {
        for (const unsigned int mode : {0U, 1U, 2U}) {
            test = Monstey_Seed();
            Monstey_Store(test, test.database + 0x20, 8, 0xABCD1234567890EFULL);
            if (equal_owner) {
                Monstey_Store(test, test.body + 0xA8, 8, test.database + 0x20);
            }
            test.trap_bound = mode != 0;
            test.trap_throws = mode == 2;
            test.stamp_trap = mode == 2;
            test.expected = mode == 2 ? Monstey_FaultKind::Monstey_Provider
                : Monstey_FaultKind::Monstey_Boundary;
            if (mode != 2) {
                test.expected_boundary = mode == 0
                    ? Monstey_GadbPublicationBoundaryKind::invariant_trap
                    : Monstey_GadbPublicationBoundaryKind::trap_returned;
            }
            test.expected_counts = std::array<std::size_t, 3>{
                equal_owner ? 2U : 1U, 0, mode == 0 ? 0U : 1U};
            if (!equal_owner) {
                test.postconditions = {{test.base + 0x5000, 8, 0}};
            }
            if (mode == 2) {
                test.postconditions.push_back({test.effect, 8, 0x7A4F000000000001ULL});
            }
            Monstey_Add(cases, test,
                std::string(equal_owner ? "publish_final_owner_trap_"
                                        : "publish_old_owner_trap_")
                    + std::to_string(mode));
        }
    }
    for (const std::size_t cutoff : {
             0U, 1U, 0x1000U, 0x1007U, 0x1020U, 0x1048U, 0x1058U,
             0x1060U, 0x1067U, 0x2020U, 0x2027U, 0x2028U, 0x202FU,
             0x2060U, 0x2063U, 0x2064U, 0x2068U, 0x206BU, 0x2070U,
             0x2077U, 0x2078U, 0x207FU, 0x2080U, 0x2087U, 0x2088U,
             0x208FU, 0x2090U, 0x2093U, 0x2098U, 0x209BU, 0x20A0U,
             0x20A7U, 0x20A8U, 0x20AFU, 0x3000U, 0x3007U, 0x3008U,
             0x300FU, 0x4008U, 0x400BU, 0x5000U, 0x5007U}) {
        test = Monstey_Seed();
        test.view_size = cutoff;
        test.expected = Monstey_FaultKind::Monstey_Range;
        Monstey_Add(cases, test, "publish_consumed_range_prefix_cutoff_"
            + std::to_string(cutoff));
    }
    for (const std::uint64_t body : {0ULL, 1ULL, 0x2FULL, 0x30ULL, 0x5FULL,
             0x60ULL, 0x8FULL, 0x90ULL, 0xFFFFFFFFFFFFFFFFULL}) {
        test = Monstey_Helper(Monstey_Entry::Monstey_Primary);
        test.body = body;
        Monstey_Store(test, test.first + 8, 4, 1);
        Monstey_Store(test, test.first + 12, 4, 1);
        test.expected_counts = std::array<std::size_t, 3>{2, 2, 0};
        Monstey_Add(cases, test, "direct_body_unsigned_wrap_no_projection_preflight_"
            + std::to_string(body));
    }
    test = Monstey_Seed(0);
    test.body = std::numeric_limits<std::uint64_t>::max() - 0x40ULL;
    test.expected = Monstey_FaultKind::Monstey_Range;
    Monstey_Add(cases, test, "publish_numeric_body_wrap_first_consumed_fault");
    return cases;
}
std::string Monstey_Json(const std::string& input) {
    std::string result = "\"";
    for (const char value : input) {
        switch (value) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            result += static_cast<unsigned char>(value) < 0x20U ? '?' : value;
            break;
        }
    }
    result += '"';
    return result;
}
void Monstey_Print(const Monstey_Report& report) {
    std::cout << "{\"schema\":\"Monstey.PublicationFramedProjection/1\","
        "\"reference\":\"actual_accepted_generic_v001_library\","
        "\"provider_ignores_frame\":true,\"cases\":" << report.cases
        << ",\"checks\":" << report.checks
        << ",\"pass\":" << (report.failures.empty() ? "true" : "false")
        << ",\"failed\":" << report.failures.size()
        << ",\"entry_cases\":[";
    for (std::size_t index = 0; index < report.entries.size(); ++index) {
        if (index != 0) { std::cout << ','; }
        std::cout << report.entries[index];
    }
    std::cout << "],\"event_counts\":["
        << report.events[0] << ',' << report.events[1] << ',' << report.events[2]
        << "],\"failures\":[";
    for (std::size_t index = 0; index < report.failures.size(); ++index) {
        if (index != 0) { std::cout << ','; }
        std::cout << "{\"case\":" << Monstey_Json(report.failures[index].first)
            << ",\"check\":" << Monstey_Json(report.failures[index].second) << '}';
    }
    std::cout << "]}\n";
}
} // namespace

int main() {
    Monstey_Report report;
    try {
        for (const auto& test : Monstey_Cases()) {
            Monstey_Run(test, report);
        }
        for (std::size_t index = 0; index < report.entries.size(); ++index) {
            Monstey_Check(report, "all_six_actual_entries", report.entries[index] != 0,
                           "actual_generic_and_framed_function_linked_and_invoked");
        }
    } catch (const std::exception& fault) {
        report.failures.emplace_back("harness_fatal", fault.what());
    } catch (...) {
        report.failures.emplace_back("harness_fatal", "non-standard exception");
    }
    Monstey_Print(report);
    return report.failures.empty() ? 0 : 1;
}
