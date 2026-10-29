#include "Monstey_Core.hpp"
#include "Monstey_FearUnpackedValidate.hpp"
#include "Monstey_ValidateIndependentReference.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using U = std::uint32_t;
using Q = std::uint64_t;
using M = Monstey_FearDbMutableMemory;
using S = Monstey_FearUnpackedValidateServices;
using P = Monstey_FearValidateX87Phase;
Q checks = 0, cases = 0;
std::string current;
void Check(bool v, const char *text) {
    ++checks;
    if (!v)
        throw std::runtime_error(text);
}
std::size_t Offset(const M &m, U a, bool write) {
    if (a < m.base || Q(a) + 4 > 0x100000000ull || Q(a) - m.base > m.bytes.size() ||
        4 > m.bytes.size() - std::size_t(Q(a) - m.base))
        throw Monstey_FearDbFault(a, 4, write);
    return a - m.base;
}
U Read(const M &m, U a) {
    auto k = Offset(m, a, false);
    return U(m.bytes[k]) | (U(m.bytes[k + 1]) << 8) | (U(m.bytes[k + 2]) << 16) |
           (U(m.bytes[k + 3]) << 24);
}
void Write(M &m, U a, U v) {
    auto k = Offset(m, a, true);
    for (U j = 0; j < 4; ++j)
        m.bytes[k + j] = static_cast<std::uint8_t>(v >> (8 * j));
}
struct E {
    U kind = 0, site = 0, target = 0, a = 0, b = 0, c = 0, d = 0;
    Q value = 0;
    bool operator==(const E &) const = default;
};
struct F {
    static constexpr U base = 0x1000B000u, C = base + 0x100u, T = base + 0x200u, FP = base + 0x800u,
                       source = base + 0x500u;
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(4096, 0xA5), other;
    M m{base, bytes};
    S s;
    U frame = FP, constraint = C, string = T, parsed = 5;
    Q value = 0x3FF0000000000000ull;
    std::uint16_t lower = 0, upper = 0x4000;
    std::vector<E> events;
    std::function<void(F &, const E &)> hook;
    void Event(E e) {
        events.push_back(e);
        if (hook)
            hook(*this, e);
    }
    explicit F(U type = 3) {
        Write(m, C, type);
        Write(m, C + 4, 0);
        Write(m, C + 8, 10);
        Write(m, 0x1000B070u, 0xC570u);
        Write(m, 0x1000B0C0u, 0xC7C0u);
        Write(m, 0x1000B0BCu, 0xC7BCu);
        Write(m, T + 0x18u, 15);
        s.c_str = [this](M &, Monstey_FearValidateCstrCall c) {
            Event({1, c.native_site, c.target, c.string_object});
            return source;
        };
        s.strtol = [this](M &, Monstey_FearValidateStrtolCall c) {
            Event({2, c.native_site, c.target, c.source, c.end_pointer, c.base});
            return parsed;
        };
        s.atof_binary64 = [this](M &, Monstey_FearValidateAtofCall c) {
            Event({3, c.native_site, c.target, c.source});
            return value;
        };
        s.compare_status = [this](M &, Monstey_FearValidateCompareCall c) {
            Event({c.phase == P::lower_duplicate_compare ? 4u : 5u, c.compare_site, c.status_site,
                   c.bound_address, c.bound_binary32_bits, c.load_site, 0, c.value_binary64_bits});
            return c.phase == P::lower_duplicate_compare ? lower : upper;
        };
        s.retire_primary = [this](M &, U site, Q v) { Event({6, site, 0, 0, 0, 0, 0, v}); };
    }
};
std::uint8_t Reference(F &f) {
    auto &m = f.m;
    auto &s = f.s;
    const U type = Read(m, f.constraint);
    if (type != 2 && type != 3)
        return 1;
    const U ctarget = Read(m, 0x1000B070u);
    if (!s.c_str)
        throw Monstey_FearUnpackedValidateBoundaryFault(
            "c_str", type == 2 ? 0x10003F4Du : 0x10003F2Au, ctarget);
    const U source = s.c_str(m, {type == 2 ? 0x10003F4Du : 0x10003F2Au, ctarget, f.string});
    if (type == 3) {
        Write(m, f.frame - 4, 0);
        Write(m, f.frame - 8, 0);
        Write(m, f.frame - 12, source);
        U target = Read(m, 0x1000B0C0u);
        if (!s.strtol)
            throw Monstey_FearUnpackedValidateBoundaryFault("strtol", 0x10003F35u, target);
        U arg = Read(m, f.frame - 12), end = Read(m, f.frame - 8), base = Read(m, f.frame - 4);
        U result = s.strtol(m, {0x10003F35u, target, arg, end, base});
        const auto signedResult = std::bit_cast<std::int32_t>(result),
                   lo = std::bit_cast<std::int32_t>(Read(m, f.constraint + 4));
        if (signedResult < lo)
            return 0;
        return signedResult <= std::bit_cast<std::int32_t>(Read(m, f.constraint + 8)) ? 1u : 0u;
    }
    Write(m, f.frame - 4, source);
    const U target = Read(m, 0x1000B0BCu);
    if (!s.atof_binary64)
        throw Monstey_FearUnpackedValidateBoundaryFault("atof", 0x10003F54u, target);
    const Q value = s.atof_binary64(m, {0x10003F54u, target, Read(m, f.frame - 4)});
    U lo = Read(m, f.constraint + 4);
    if (!s.compare_status)
        throw Monstey_FearUnpackedValidateBoundaryFault("compare", 0x10003F5Fu);
    auto lower = s.compare_status(m, {P::lower_duplicate_compare, 0x10003F5Au, 0x10003F5Fu,
                                      0x10003F64u, f.constraint + 4, value, lo});
    if (!Monstey_ValidateReference::LowerPass(lower)) {
        if (!s.retire_primary)
            throw Monstey_FearUnpackedValidateBoundaryFault("retire", 0x10003F7Cu);
        s.retire_primary(m, 0x10003F7Cu, value);
        return 0;
    }
    U hi = Read(m, f.constraint + 8);
    if (!s.compare_status)
        throw Monstey_FearUnpackedValidateBoundaryFault("compare", 0x10003F70u);
    auto upper = s.compare_status(m, {P::upper_final_compare, 0x10003F6Bu, 0x10003F70u, 0x10003F72u,
                                      f.constraint + 8, value, hi});
    return Monstey_ValidateReference::UpperPass(upper) ? 1u : 0u;
}
struct Outcome {
    U kind = 0, value = 0, address = 0, width = 0, site = 0, target = 0;
    bool write = false;
    bool operator==(const Outcome &) const = default;
};
Outcome Run(F &f, bool reference) {
    try {
        return {0, reference
                       ? Reference(f)
                       : Monstey_FearUnpackedValidate(f.m, f.constraint, f.string, f.frame, f.s)};
    } catch (const Monstey_FearDbFault &e) {
        return {1, 0, e.address, e.width, 0, 0, e.writing};
    } catch (const Monstey_FearUnpackedValidateBoundaryFault &e) {
        return {2, 0, 0, 0, e.native_site, e.target};
    }
}
void Pair(const std::function<void(F &)> &setup, U type = 3) {
    F expected(type), actual(type);
    setup(expected);
    setup(actual);
    const auto a = Run(expected, true), b = Run(actual, false);
    Check(a == b, "native control/fault result differs");
    Check(expected.events == actual.events, "complete primitive argument/order trace differs");
    Check(expected.bytes == actual.bytes && expected.other == actual.other,
          "complete arena/prefix differs");
    Check(expected.m.base == actual.m.base && expected.m.bytes.size() == actual.m.bytes.size(),
          "view metadata differs");
    ++cases;
}
Q state = 0x123456789ABCDEFull;
Q Random() {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}
struct Effect {
    M *memory;
    U address, value;
    U *retired;
    ~Effect() {
        Write(*memory, address, value);
        ++*retired;
    }
};
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("NEW_REPORT required");
        std::vector<std::string> groups;
        auto G = [&](const char *s) {
            current = s;
            groups.push_back(s);
        };
        G("signed_integer_branch_and_whole_arena_reference");
        const std::array<U, 9> ints{0,           1,           2,           0x7FFFFFFEu, 0x7FFFFFFFu,
                                    0x80000000u, 0x80000001u, 0xFFFFFFFEu, UINT32_MAX};
        for (U p : ints)
            for (U lo : ints)
                for (U hi : ints)
                    Pair([=](F &f) {
                        f.parsed = p;
                        Write(f.m, f.C + 4, lo);
                        Write(f.m, f.C + 8, hi);
                    });
        for (U i = 0; i < 2048; ++i) {
            const U p = U(Random()), lo = U(Random()), hi = U(Random());
            Pair([=](F &f) {
                f.parsed = p;
                Write(f.m, f.C + 4, lo);
                Write(f.m, f.C + 8, hi);
            });
        }
        G("all_status_AH_pairs_and_primary_retirement");
        for (U lo = 0; lo < 256; ++lo)
            for (U hi = 0; hi < 256; ++hi)
                Pair(
                    [=](F &f) {
                        f.lower = std::uint16_t((lo << 8) | 0xA5u);
                        f.upper = std::uint16_t((hi << 8) | 0x5Au);
                    },
                    2);
        G("numeric_masked_profile_separate_FP_reference_all_MXCSR");
        std::vector<Q> doubles{0,
                               0x8000000000000000ull,
                               1,
                               0x8000000000000001ull,
                               0x000FFFFFFFFFFFFFull,
                               0x0010000000000000ull,
                               0x3FF0000000000000ull,
                               0xBFF0000000000000ull,
                               0x7FEFFFFFFFFFFFFFull,
                               0xFFEFFFFFFFFFFFFFull,
                               0x7FF0000000000000ull,
                               0xFFF0000000000000ull,
                               0x7FF0000000000001ull,
                               0xFFF123456789ABCDull,
                               0x7FF8000000000001ull,
                               0x36A0000000000000ull,
                               0xB6A0000000000000ull,
                               0x36A0000000000001ull,
                               0x369FFFFFFFFFFFFFull,
                               0xB6A0000000000001ull,
                               0xB69FFFFFFFFFFFFFull};
        std::vector<U> floats{0,           0x80000000u, 1,           0x80000001u, 0x007FFFFFu,
                              0x00800000u, 0x3F800000u, 0xBF800000u, 0x7F7FFFFFu, 0xFF7FFFFFu,
                              0x7F800000u, 0xFF800000u, 0x7F800001u, 0xFFC01234u};
        for (U i = 0; i < 80; ++i) {
            doubles.push_back(Random());
            floats.push_back(U(Random()));
        }
        const U oldcsr = _mm_getcsr();
        for (U flags = 0; flags < 16; ++flags) {
            const U csr = 0x1F80u | ((flags & 3u) << 13) | ((flags & 4u) ? 0x40u : 0u) |
                          ((flags & 8u) ? 0x8000u : 0u);
            _mm_setcsr(csr);
            for (Q d : doubles)
                for (U b : floats) {
                    const auto expected = Monstey_ValidateReference::NormalMasked(d, b);
                    Check(Monstey_FearUnpackedValidateMaskedCompare(d, b) == expected,
                          "normal-masked bits differ from separate finite FP reference");
                    Check(_mm_getcsr() == csr, "masked helper changed caller MXCSR");
                    ++cases;
                }
        }
        _mm_setcsr(oldcsr);
        G("late_parse_bounds_type_cache_and_upper_reload");
        Pair([](F &f) {
            f.hook = [](F &x, const E &e) {
                if (e.kind == 1)
                    Write(x.m, x.C, 2);
                if (e.kind == 2) {
                    Write(x.m, x.C + 4, 4);
                    Write(x.m, x.C + 8, 5);
                }
            };
        });
        Pair(
            [](F &f) {
                f.hook = [](F &x, const E &e) {
                    if (e.kind == 3)
                        Write(x.m, x.C + 4, 0xBF800000u);
                    if (e.kind == 4)
                        Write(x.m, x.C + 8, 0x3F800000u);
                };
            },
            2);
        Pair(
            [](F &f) {
                f.hook = [](F &x, const E &e) {
                    if (e.kind == 3) {
                        x.other = x.bytes;
                        x.m = {x.base, x.other};
                        Write(x.m, x.C + 4, 0xDEAD0101u);
                    }
                    if (e.kind == 4)
                        Write(x.m, x.C + 8, 0xBEEF0202u);
                };
            },
            2);
        Pair([](F &f) {
            f.hook = [](F &x, const E &e) {
                if (e.kind == 1) {
                    x.other = x.bytes;
                    x.m = {x.base, x.other};
                    Write(x.m, 0x1000B0C0u, 0xABCD1234u);
                }
            };
        });
        G("raw_argument_aliases_and_partial_store_prefixes");
        Pair([](F &f) { f.frame = 0x1000B0CCu; });
        Pair([](F &f) { f.frame = 0x1000B0C0u; }, 2);
        Pair([](F &f) { f.frame = f.C + 8; });
        Pair([](F &f) { f.frame = f.C + 8; }, 2);
        Pair([](F &f) { f.frame = f.base + 8; });
        Pair([](F &f) { f.frame = f.base; }, 2);
        Pair([](F &f) { f.s.strtol = {}; });
        Pair([](F &f) { f.s.atof_binary64 = {}; }, 2);
        Pair([](F &f) { f.s.c_str = {}; });
        Pair([](F &f) { f.s.c_str = {}; }, 2);
        Pair(
            [](F &f) {
                f.lower = 0x0100;
                f.s.retire_primary = {};
            },
            2);
        G("lazy_bounds_and_fault_before_missing_service");
        Pair([](F &f) {
            f.frame = f.base + 0x40;
            f.parsed = 1;
            Write(f.m, f.C + 4, 2);
            f.m.bytes = f.m.bytes.first(f.C + 8 - f.base);
        });
        Pair([](F &f) {
            f.frame = f.base + 0x40;
            f.parsed = 2;
            Write(f.m, f.C + 4, 2);
            f.m.bytes = f.m.bytes.first(f.C + 8 - f.base);
        });
        Pair(
            [](F &f) {
                f.frame = f.base + 0x40;
                f.lower = 0x0100;
                f.m.bytes = f.m.bytes.first(f.C + 8 - f.base);
            },
            2);
        Pair(
            [](F &f) {
                f.frame = f.base + 0x40;
                f.lower = 0;
                f.m.bytes = f.m.bytes.first(f.C + 8 - f.base);
            },
            2);
        Pair(
            [](F &f) {
                f.frame = f.base + 0x40;
                f.s.compare_status = {};
                f.m.bytes = f.m.bytes.first(f.C + 4 - f.base);
            },
            2);
        Pair([](F &f) {
            f.m.base += 0x74u;
            f.m.bytes = f.m.bytes.subspan(0x74u);
        });
        Pair([](F &f) { f.constraint = f.base - 1; });
        for (U type : std::array<U, 7>{0, 1, 4, 5, UINT32_MAX, 0x80000000u, 0xFFFFFFFEu})
            Pair(
                [](F &f) {
                    f.s = {};
                    f.m.bytes = f.m.bytes.first(f.C + 4 - f.base);
                },
                type);
        G("raw_cstr_unsigned_capacity_lazy_heap_identity");
        for (U capacity : std::array<U, 6>{0, 1, 15, 16, 0x80000000u, UINT32_MAX}) {
            F f;
            Write(f.m, f.T + 0x18, capacity);
            Write(f.m, f.T + 4, 0x1234ABCDu);
            Check(Monstey_FearUnpackedValidateStringData(f.m, f.T) ==
                      (capacity >= 16 ? 0x1234ABCDu : f.T + 4),
                  "unsigned cstr capacity selection");
            ++cases;
        }
        {
            F f;
            const U t = UINT32_MAX - 3u;
            f.m = {t + 0x18u, std::span<std::uint8_t>(f.bytes).first(4)};
            Write(f.m, t + 0x18u, 15);
            Check(Monstey_FearUnpackedValidateStringData(f.m, t) == 0,
                  "inline identity wraps without heap read");
            ++cases;
        }
        G("mutable_targets_and_call_scoped_destructor_retirement");
        {
            F f;
            U retired = 0;
            auto effect = std::shared_ptr<Effect>(new Effect{&f.m, f.C + 4, 6, &retired});
            f.s.c_str = [&f, effect](M &, Monstey_FearValidateCstrCall) {
                f.s.c_str = {};
                return f.source;
            };
            effect.reset();
            Check(Monstey_FearUnpackedValidate(f.m, f.C, f.T, f.frame, f.s) == 0 && retired == 1,
                  "cstr old target must retire before signed lower read");
            ++cases;
        }
        {
            F f(2);
            U retired = 0;
            auto effect = std::shared_ptr<Effect>(new Effect{&f.m, f.C + 8, 0xAABBCCDDu, &retired});
            f.s.compare_status = [&f, effect](M &, Monstey_FearValidateCompareCall c) {
                if (c.phase == P::lower_duplicate_compare) {
                    f.s.compare_status = [&f](M &, Monstey_FearValidateCompareCall q) {
                        Check(q.bound_binary32_bits == 0xAABBCCDDu,
                              "lower target retirement before upper bound load");
                        return f.upper;
                    };
                    return std::uint16_t{0};
                }
                return std::uint16_t{0x4000};
            };
            effect.reset();
            Check(Monstey_FearUnpackedValidate(f.m, f.C, f.T, f.frame, f.s) == 1 && retired == 1,
                  "lower replacement active lifetime");
            ++cases;
        }
        {
            F f(2);
            U called = 0;
            f.s.compare_status = [n = U{0}, &called](M &,
                                                     Monstey_FearValidateCompareCall c) mutable {
                ++n;
                ++called;
                Check(n == (c.phase == P::lower_duplicate_compare ? 1u : 2u),
                      "mutable status target copied per call");
                return std::uint16_t{0x4000};
            };
            Check(Monstey_FearUnpackedValidate(f.m, f.C, f.T, f.frame, f.s) == 1 && called == 2,
                  "persistent lower-upper target");
            ++cases;
        }
        {
            F f;
            U retired = 0;
            auto effect =
                std::shared_ptr<Effect>(new Effect{&f.m, 0x1000B0C0u, 0xAABBCCDDu, &retired});
            f.s.c_str = [&f, effect](M &, Monstey_FearValidateCstrCall) {
                f.s.c_str = {};
                return f.source;
            };
            effect.reset();
            Check(Monstey_FearUnpackedValidate(f.m, f.C, f.T, f.frame, f.s) == 1 && retired == 1 &&
                      f.events.at(0).target == 0xAABBCCDDu,
                  "retirement precedes numeric IAT sampling");
            ++cases;
        }
        auto names = Monstey_Json::array();
        for (const auto &n : groups)
            names.as_array().push_back(n);
        Monstey_WriteNewText(argv[1], Monstey_Json::object({{"status", "PASS"},
                                                            {"groups", groups.size()},
                                                            {"cases", cases},
                                                            {"checks", checks},
                                                            {"group_names", names},
                                                            {"owner_reference_reused", false},
                                                            {"original_executed", false},
                                                            {"raw_instruction_interpreter", false},
                                                            {"physical_CRT_x87_qualified", false}})
                                              .dump(2) +
                                          "\n");
        std::cout << "PASS " << groups.size() << " groups " << cases << " cases " << checks
                  << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << current << ": " << e.what() << '\n';
        return 1;
    }
}
