#include "Monstey_GadbReferenceWord.hpp"
#include "Monstey_GadbScalarFloat.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using U64 = std::uint64_t;
using U32 = std::uint32_t;
constexpr U64 Base = 0x100000000ull;
constexpr U64 Record = Base + 0x100, Field = Base + 0x300;
constexpr U64 Table = Base + 0x400, Declaration = Base + 0x600;
constexpr U64 Owner = Base + 0x800, Class = Base + 0x900;
constexpr U64 Values = Base + 0x1000, Default = Base + 0x2000;
constexpr U64 Bad = Base + 0x10000000, Fallback = 0xfedcba9876543210ull;
std::uint64_t Checks = 0, Cases = 0, Groups = 0;
void Check(bool condition, const char* message) {
    ++Checks;
    if (!condition) throw std::runtime_error(message);
}
struct Fixture {
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x3000);
    void Put(U64 address, U64 value, std::size_t width) {
        if (address < Base || address - Base + width > bytes.size())
            throw std::runtime_error("Test fixture write outside arena");
        for (std::size_t i = 0; i < width; ++i)
            bytes[static_cast<std::size_t>(address - Base) + i] =
                static_cast<std::uint8_t>(value >> (8 * i));
    }
    Monstey_GadbBinderMemory Memory() { return {Base, bytes}; }
    Fixture() {
        Put(Record, 1, 1); Put(Record + 1, 1, 1);
        Put(Record + 8, Table, 8); Put(Record + 16, Values, 8);
        Put(Record + 24, Owner, 8); Put(Owner + 32, Class, 8);
        Put(Field, 0x91234567, 4);
        Put(Table, 0x91234567, 4); Put(Table + 4, 0x91234567, 4);
        Put(Table + 9, 0, 1); Put(Table + 10, 10, 1);
        Put(Table + 12, 3, 2); Put(Table + 14, 2, 2);
        Put(Class, Declaration, 8); Put(Class + 8, 1, 4);
        Put(Declaration + 32, 0x91234567, 4);
        Put(Declaration + 36, 10, 4); Put(Declaration + 40, 3, 4);
        for (U32 i = 0; i < 16; ++i)
            Put(Values + i * 8ull, 0x8000000000000000ull + i, 8);
        Put(Default, Fallback, 8);
    }
};
U64 Read(Fixture& f, U64 record = Record, U64 field = Field,
         U32 index = 0, U64 fallback = Default) {
    return Monstey_GadbReadReferenceWord(f.Memory(), record, field, index, fallback);
}
void ExpectFault(Fixture& f, U64 expected, U64 record, U64 field,
                 U32 index, U64 fallback, std::size_t length = 0) {
    ++Cases;
    try {
        const auto size = length == 0 ? f.bytes.size() : length;
        (void)Monstey_GadbReadReferenceWord(
            {Base, std::span<std::uint8_t>(f.bytes.data(), size)},
            record, field, index, fallback);
        throw std::runtime_error("Expected consumed-range fault was absent");
    } catch (const Monstey_GadbBinderFault& e) {
        Check(e.kind == Monstey_GadbBinderFaultKind::ConsumedRangeUnavailable,
              "Wrong owned fault kind");
        Check(e.address == expected, "Wrong first consumed fault address");
    }
}
void NullAndLazyDefault() {
    ++Groups;
    Fixture f;
    ++Cases; Check(Read(f, 0, Bad, UINT32_MAX) == Fallback, "Null record fallback");
    ++Cases; Check(Read(f, Record, Field, 0, Bad) == 0x8000000000000002ull,
                   "Valid raw value must not touch fallback");
    f.Put(Values + 16, 0, 8);
    ++Cases; Check(Read(f, Record, Field, 0, Bad) == 0, "Zero is a valid reference word");
    f.Put(Record, 0, 1);
    ++Cases; Check(Read(f, Record, Bad) == Fallback, "Empty record skips field hash");
    ExpectFault(f, Bad, 0, Bad, UINT32_MAX, Bad);
    ExpectFault(f, Bad, Record, Bad, 0, Bad);
}
void DescriptorMatrix() {
    ++Groups;
    Fixture f;
    const std::array<U32, 7> indices{0, 1, 2, 3, 65535, 0x80000000u, UINT32_MAX};
    for (U32 type = 0; type < 256; ++type) {
        f.Put(Table + 10, type, 1);
        for (U32 flags : {0u, 1u, 16u, 17u}) {
            f.Put(Record + 1, flags, 1);
            for (const auto index : indices) {
                ++Cases;
                const bool succeeds = (type & 63u) == 10 && index < 3;
                const U64 expected = !succeeds ? Fallback :
                    0x8000000000000000ull + index + ((flags & 16u) ? 0u : 2u);
                Check(Read(f, Record, Field, index) == expected,
                      "Type/flags/count/index matrix mismatch");
            }
        }
    }
}
void RawCountBoundaries() {
    ++Groups;
    Fixture f; f.bytes.resize(0x120000);
    const U64 values = Base + 0x10000;
    f.Put(Record + 16, values, 8);
    f.Put(Table + 14, 65535, 2);
    for (U32 count : {0u, 1u, 2u, 32767u, 32768u, 65535u}) {
        f.Put(Table + 12, count, 2);
        const std::array<U32, 5> indices{0, 1, count == 0 ? 0u : count - 1,
                                       count, UINT32_MAX};
        for (U32 flags : {0u, 1u}) {
            f.Put(Record + 1, flags, 1);
            for (const auto index : indices) {
                ++Cases;
                const U64 expected = 0xface000000000000ull + index;
                if (index < count)
                    f.Put(values + 8ull * (65535u + index), expected, 8);
                Check(Read(f, Record, Field, index) ==
                          (index < count ? expected : Fallback),
                      "U16 count / U32 index / first-word addition mismatch");
            }
        }
    }
}
void GeneratedBranches() {
    ++Groups;
    Fixture f; f.Put(Record + 1, 17, 1);
    for (U64 payload : {0ull, 1ull, 0xffffffffffffffffull,
                       0x8000000000000000ull, 0x00000000ffffffffull,
                       0x7ff8000012345678ull}) {
        f.Put(Values, payload, 8);
        ++Cases; Check(Read(f, Record, Field, 0, Bad) == payload,
                       "Generated result must retain all 64 bits");
    }
    f.Put(Record + 24, 0, 8);
    ++Cases; Check(Read(f) == Fallback, "Missing owner fallback");
    f.Put(Record + 24, Owner, 8); f.Put(Owner + 32, 0, 8);
    ++Cases; Check(Read(f) == Fallback, "Missing class fallback");
    f.Put(Owner + 32, Class, 8); f.Put(Class + 8, 0, 4);
    ++Cases; Check(Read(f) == Fallback, "Empty class fallback");
    f.Put(Class + 8, 1, 4); f.Put(Declaration + 32, 1, 4);
    ++Cases; Check(Read(f) == Fallback, "Missing generated hash fallback");
    f.Put(Declaration + 32, 0x91234567, 4);
    for (U32 type : {8u, 9u, 10u}) {
        f.Put(Declaration + 36, type, 4); f.Put(Declaration + 40, 0, 4);
        for (U32 index : {0u, 1u, 2u}) {
            ++Cases;
            const auto effective = type == 10 ? index : index * 2;
            Check(Read(f, Record, Field, index, Bad) ==
                      (effective == 0 ? 0x7ff8000012345678ull :
                       0x8000000000000000ull + effective),
                  "Generated declaration type/count must use native helper");
        }
    }
}
void GeneratedDynamic() {
    ++Groups;
    Fixture f; f.Put(Record + 1, 17, 1); f.Put(Table + 12, 20, 2);
    f.Put(Declaration + 40, UINT32_MAX, 4);
    const U64 array = Base + 0x1800;
    f.Put(Values, array, 8);
    for (U32 i = 0; i < 12; ++i)
        f.Put(array + 8ull * i, 0x1234000000000000ull + i, 8);
    for (U32 type : {8u, 9u, 10u}) {
        f.Put(Declaration + 36, type, 4);
        for (U32 count = 0; count < 5; ++count) {
            f.Put(Values + 8, count, 4);
            for (U32 index = 0; index < 6; ++index) {
                ++Cases;
                const U32 effective = type == 10 ? index : index * 2;
                if (effective < count)
                    Check(Read(f, Record, Field, index, Bad) ==
                              0x1234000000000000ull + effective,
                          "Generated dynamic index mismatch");
                else {
                    try {
                        (void)Read(f, Record, Field, index, Bad);
                        throw std::runtime_error("Dynamic assertion was absent");
                    } catch (const Monstey_GadbScalarFault& e) {
                        Check(e.kind == Monstey_GadbScalarFaultKind::
                                  NativeDynamicIndexAssertion,
                              "Wrong generated dynamic fault kind");
                        Check(e.address == Values + 8,
                              "Dynamic assertion must precede array/fallback read");
                    }
                }
            }
        }
    }
    f.Put(Values, Bad, 8); f.Put(Values + 8, 0, 4);
    ++Cases;
    try {
        (void)Read(f, Record, Field, 0, Bad);
        throw std::runtime_error("Empty dynamic assertion absent");
    } catch (const Monstey_GadbScalarFault& e) {
        Check(e.address == Values + 8, "Empty dynamic read touched array pointer");
    }
}
void ConsumedFaults() {
    ++Groups;
    Fixture f;
    ExpectFault(f, Bad + 1, Bad, Bad, 0, Bad);
    ExpectFault(f, Bad, Record, Bad, 0, Bad);
    f.Put(Record + 8, Bad, 8);
    ExpectFault(f, Bad, Record, Field, 0, Default);
    f = Fixture{}; f.Put(Table + 10, 4, 1);
    ++Cases;
    Check(Monstey_GadbReadReferenceWord(
              {Base, std::span<std::uint8_t>(f.bytes.data(), 0x40b)},
              Record, Field, 0, Base) == 0,
          "Type mismatch must not consume count or first-word");
    f.Put(Table + 10, 10, 1);
    ExpectFault(f, Table + 12, Record, Field, 0, Base, 0x40c);
    f.Put(Table + 12, 0, 2);
    ++Cases;
    Check(Monstey_GadbReadReferenceWord(
              {Base, std::span<std::uint8_t>(f.bytes.data(), 0x40e)},
              Record, Field, 0, Base) == 0,
          "Count miss must not consume first-word");
    f.Put(Table + 12, 1, 2);
    ExpectFault(f, Table + 14, Record, Field, 0, Base, 0x40e);
    f = Fixture{}; f.Put(Record + 16, Bad, 8);
    ExpectFault(f, Bad + 16, Record, Field, 0, Default);
    f = Fixture{}; f.Put(Record + 1, 17, 1);
    f.Put(Record + 24, Bad, 8);
    ExpectFault(f, Bad + 32, Record, Field, 0, Default);
    f = Fixture{}; f.Put(Record + 1, 17, 1);
    f.Put(Owner + 32, Bad, 8);
    ExpectFault(f, Bad + 8, Record, Field, 0, Default);
    f = Fixture{}; f.Put(Record + 1, 17, 1);
    f.Put(Record + 24, 0, 8);
    ExpectFault(f, Bad, Record, Field, 0, Bad);
    f = Fixture{}; f.Put(Record + 1, 17, 1);
    f.Put(Declaration + 32, 1, 4);
    ExpectFault(f, Bad, Record, Field, 0, Bad);
    f = Fixture{};
    ++Cases;
    Check(Monstey_GadbReadGeneratedReferenceWord(f.Memory(), 0, Bad, UINT32_MAX,
                                                Default) == Fallback,
          "Generated null record must skip field");
    f.Put(Record + 24, 0, 8);
    ++Cases;
    Check(Monstey_GadbReadGeneratedReferenceWord(f.Memory(), Record, Bad,
                                                UINT32_MAX, Default) == Fallback,
          "Generated null owner must skip field");
    f.Put(Record + 24, Owner, 8); f.Put(Class + 8, 0, 4);
    ++Cases;
    try {
        (void)Monstey_GadbReadGeneratedReferenceWord(f.Memory(), Record, Bad,
                                                    0, Default);
        throw std::runtime_error("Generated hash must be read before empty class");
    } catch (const Monstey_GadbBinderFault& e) {
        Check(e.address == Bad, "Wrong generated hash-before-class failure");
    }
}
struct Access { U64 address; std::size_t width; };
void Prefix(bool generated) {
    ++Groups;
    Fixture f; f.Put(Record + 1, generated ? 17 : 1, 1);
    std::vector<Access> accesses{
        {Record+1,1},{Record,1},{Record+8,8},{Field,4},{Table,4},
        {Table+10,1},{Table+12,2},{Record+1,1}};
    if (generated) {
        const std::vector<Access> tail{
            {Record+24,8},{Owner+32,8},{Owner+32,8},
            {Field,4},{Class+8,4},{Class,8},{Declaration+32,4},
            {Class+8,4},{Class,8},{Declaration+32,4},{Declaration+36,4},
            {Record+16,8},{Declaration+40,4},{Values,8}};
        accesses.insert(accesses.end(), tail.begin(), tail.end());
    } else {
        accesses.insert(accesses.end(),
                        {{Table+14,2},{Record+16,8},{Values+16,8}});
    }
    const auto preimage = f.bytes;
    for (std::size_t length = 0; length <= 0x2200; ++length) {
        ++Cases;
        const auto first = std::find_if(accesses.begin(), accesses.end(),
            [length](const Access& a) {
                return a.address - Base + a.width > length;
            });
        try {
            const auto result = Monstey_GadbReadReferenceWord(
                {Base, std::span<std::uint8_t>(f.bytes.data(), length)},
                Record, Field, 0, Bad);
            Check(first == accesses.end(), "Prefix missed expected first fault");
            Check(result == 0x8000000000000000ull + (generated ? 0 : 2),
                  "Prefix changed returned value");
        } catch (const Monstey_GadbBinderFault& e) {
            Check(first != accesses.end(), "Unexpected prefix fault");
            Check(e.address == first->address, "Prefix first read epoch mismatch");
        }
    }
    Check(f.bytes == preimage, "Reference getter mutated borrowed memory");
}
void Aliases() {
    ++Groups;
    Fixture f;
    for (const U64 fallback : {Record, Field, Table, Values, Default, Bad}) {
        ++Cases;
        Check(Read(f, Record, Field, 1, fallback) == 0x8000000000000003ull,
              "Unused aliased fallback changed successful raw result");
    }
    f.Put(Table + 10, 4, 1);
    ++Cases;
    Check(Read(f, Record, Field, 0, Values) == 0x8000000000000000ull,
          "Fallback alias was not read as an entire QWORD");
    const auto before = f.bytes;
    (void)Read(f, Record, Field, 0, Record);
    ++Cases; Check(f.bytes == before, "Fallback alias path wrote memory");
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 2 || std::filesystem::exists(argv[1]))
            throw std::runtime_error("Usage: tests NEW_REPORT.json");
        NullAndLazyDefault(); DescriptorMatrix(); RawCountBoundaries();
        GeneratedBranches(); GeneratedDynamic(); ConsumedFaults();
        Prefix(false); Prefix(true); Aliases();
        std::ofstream out(argv[1], std::ios::binary);
        if (!out) throw std::runtime_error("Could not create test report");
        out << "{\"schema\":\"Monstey.GadbReferenceWordTests/1\","
            << "\"groups\":" << Groups << ",\"cases\":" << Cases
            << ",\"checks\":" << Checks << ",\"passed\":true,"
            << "\"original_code_executed\":false,\"game_accessed\":false}\n";
        out.close();
        if (!out) throw std::runtime_error("Test report write failed");
        std::cout << "PASS groups=" << Groups << " cases=" << Cases
                  << " checks=" << Checks << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
