#include "Monstey_Core.hpp"
#include "Monstey_FearUnpackedMaterialize.hpp"
#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using U = std::uint32_t;
using Op = Monstey_FearUnpackedStringOp;
using Mem = Monstey_FearDbMutableMemory;
std::uint64_t checks = 0, cases = 0;
void Check(bool b, const char *s) {
    ++checks;
    if (!b)
        throw std::runtime_error(s);
}
U Read(Mem m, U a, U n = 4) {
    Check(a >= m.base && std::uint64_t(a) - m.base + n <= m.bytes.size(), "test read range");
    U v = 0;
    for (U j = 0; j < n; ++j)
        v |= U(m.bytes[a - m.base + j]) << (8 * j);
    return v;
}
void Put(Mem m, U a, U v, U n = 4) {
    Check(a >= m.base && std::uint64_t(a) - m.base + n <= m.bytes.size(), "test write range");
    for (U j = 0; j < n; ++j)
        m.bytes[a - m.base + j] = static_cast<std::uint8_t>(v >> (8 * j));
}
std::string CString(Mem m, U a) {
    std::string s;
    while (Read(m, a, 1)) {
        s.push_back(static_cast<char>(Read(m, a++, 1)));
    }
    return s;
}
struct Row {
    std::string key, value;
    U type = 3, opaque = 0x76543210;
};
struct F {
    static constexpr U base = 0x1000, context = 0x1800, map = 0x2000, header = 0x2080,
                       first = 0x2100, sf = 0x5000, mf = 0x6000, out = 0x10000, cursor = 0x1900;
    std::vector<std::uint8_t> bytes = std::vector<std::uint8_t>(0x20000, 0xA5);
    Monstey_FearUnpackedLogicalStrings logical;
    Monstey_FearUnpackedImports imports;
    U heap = 0x8000;
    std::function<void(Mem, Op, U, U, U, U)> after;
    std::function<U(Mem, U)> integer;
    std::function<U(Mem, U, U, std::array<U, 4>)> scan;
    std::vector<std::string> log;
    U strcalls = 0, numcalls = 0;
    Mem M() { return {base, bytes}; }
    F(const std::vector<Row> &rows) {
        auto m = M();
        logical.allocate = [this](Mem x, U n) {
            U at = heap;
            heap += (n + 15u) & ~15u;
            Check(heap < 0xE000, "fixture heap");
            for (U k = 0; k < n; ++k)
                Put(x, at + k, 0xD4, 1);
            return at;
        };
        imports.string = [this](Mem x, Op op, U self, U a, U b, U c) {
            ++strcalls;
            U v = logical.Invoke(x, op, self, a, b, c);
            if (after)
                after(x, op, self, a, b, c);
            return v;
        };
        imports.atoi_bits = [this](Mem x, U source) {
            ++numcalls;
            if (integer)
                return integer(x, source);
            const std::map<std::string, U> known = {
                {"11", 11}, {"22", 22}, {"33", 33}, {"yes", UINT32_MAX}, {"neg", 0x80000000}};
            return known.at(CString(x, source));
        };
        imports.atof_fstp32_bits = [this](Mem x, U source) {
            ++numcalls;
            Check(CString(x, source) == "nan", "float supplied token");
            return 0x7FC12345;
        };
        imports.sscanf = [this](Mem x, U s, U dims, std::array<U, 4> d) {
            ++numcalls;
            if (scan)
                return scan(x, s, dims, d);
            Check(CString(x, s) == "vec", "vector supplied token");
            for (U k = 0; k < dims; ++k)
                Put(x, d[k], 0x3F000000u + k);
            return 0u;
        };
        imports.strncpy = Monstey_FearUnpackedForwardStrncpy;
        imports.npos_pointer_cell = 0x1700;
        imports.root_name_cell = 0x1704;
        imports.empty_cstring = 0x1720;
        Put(m, 0x1700, 0x1710);
        Put(m, 0x1710, UINT32_MAX);
        Put(m, 0x1704, 0x1730);
        Put(m, 0x1720, 0, 1);
        Put(m, 0x1730, 0, 1);
        Put(m, map + 4, header);
        Put(m, header, rows.empty() ? header : first);
        Put(m, header + 4, rows.empty() ? header : first);
        Put(m, header + 8, rows.empty() ? header : first + U(rows.size() - 1) * 0x80u);
        Put(m, header + 0x4D, 1, 1);
        for (std::size_t j = 0; j < rows.size(); ++j) {
            U node = first + static_cast<U>(j) * 0x80u;
            Put(m, node, header);
            Put(m, node + 4, j ? node - 0x80u : header);
            Put(m, node + 8, j + 1 < rows.size() ? node + 0x80u : header);
            Put(m, node + 0x4D, 0, 1);
            Put(m, node + 0x28, rows[j].type);
            Put(m, node + 0x48, rows[j].opaque);
            Bind(node + 0x0C, rows[j].key);
            Bind(node + 0x2C, rows[j].value);
        }
        for (U j = 0; j < 6; ++j)
            Put(m, context + 4 * j, 0);
        Put(m, sf + 0xBC, map);
        Put(m, mf + 0x130, out);
        Put(m, mf + 0x134, context);
        Put(m, mf + 0x138, map);
        Put(m, mf + 0x13C, out);
        Put(m, cursor, out + 20);
    }
    void Bind(U object, const std::string &text) {
        auto m = M();
        logical.Invoke(m, Op::construct, object);
        if (text.empty())
            return; // Fill via a temporary raw source object without calling a parser.
        U data = object + 4, capacity = 15;
        if (text.size() > 15) {
            data = logical.allocate(m, static_cast<U>(text.size() + 1));
            Put(m, object + 4, data);
            capacity = static_cast<U>(text.size());
        }
        for (std::size_t j = 0; j < text.size(); ++j)
            Put(m, data + static_cast<U>(j), static_cast<std::uint8_t>(text[j]), 1);
        Put(m, data + static_cast<U>(text.size()), 0, 1);
        Put(m, object + 0x14, static_cast<U>(text.size()));
        Put(m, object + 0x18, capacity);
    }
    void Size() { Monstey_FearUnpackedSize(M(), context, sf, imports); }
    U Prepare() {
        Size();
        const U n = Monstey_FearUnpackedAllocationBytes({base, bytes}, context);
        Put(M(), context + 20, out + n);
        return n;
    }
    void Run() {
        Check(Monstey_FearUnpackedMaterialize(M(), cursor, mf, imports) == 1, "materialize result");
    }
};
void Group(std::vector<std::string> &names, const char *name) { names.emplace_back(name); }
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        if (argc != 2)
            throw std::runtime_error("NEW_REPORT required");
        std::vector<std::string> groups;
        {
            Group(groups, "two_attributes_multivalue_address_and_opaque");
            F f(std::vector<Row>{{"C/r/a", "11;22", 3, 0x01234567},
                                 {"C/r/b", "33;", 3, 0xFEDCBA98}});
            U n = f.Prepare();
            Check(n == 116, "allocation116");
            Check(CString(f.M(), F::first + 0x2C + 4) == "11;22;", "live semicolon append");
            f.Run();
            auto m = f.M();
            constexpr U attr = F::out + 52, word = F::out + 84;
            Check(Read(m, word) == 11 && Read(m, word + 4) == 33 && Read(m, word + 8) == 0xA5A5A5A5,
                  "native multivalue cell overlap");
            Check(Read(m, attr + 8, 2) == 2 && Read(m, attr + 24, 2) == 1, "value counts");
            Check(Read(m, attr + 12) == word && Read(m, attr + 28) == word + 4,
                  "cursor per attribute");
            Check(Read(m, F::context + 12) == 6 && Read(m, F::cursor) == F::out + 116,
                  "count and reserve cursor");
            Check(Read(m, F::mf + 0x9C) == 0xFEDCBA98, "opaque raw copy");
            Check(Read(m, F::out) == F::out && Read(m, F::out + 8) == 1 &&
                      Read(m, F::out + 12) == F::out + 20 && Read(m, F::out + 16) == 0,
                  "root layout");
            ++cases;
        }
        {
            Group(groups, "two_record_parent_formula_erratum");
            F f(std::vector<Row>{{"C/a/a", "11;"}, {"C/b/b", "33;"}});
            f.Prepare();
            f.Run();
            auto m = f.M();
            constexpr U records = F::out + 36, attr = F::out + 68;
            Check(Read(m, records + 8) == 1 && Read(m, records + 24) == 1, "two records counts");
            Check(Read(m, attr + 4) == records && Read(m, attr + 20) == records,
                  "parent omits current-category record ordinal");
            Check(Read(m, F::out + 20 + 8) == 2, "final category records plus1");
            Check(Read(m, F::mf + 0x1C) == 1, "final records increment not stored");
            ++cases;
        }
        {
            Group(groups, "category_transitions_and_long_keys");
            F f(std::vector<Row>{{"CategoryA/recordX/a", "11;"}, {"CategoryB/recordY/b", "33;"}});
            f.Prepare();
            f.Run();
            auto m = f.M();
            Check(Read(m, F::out + 8) == 2, "two categories");
            U cats = F::out + 20, records = cats + 32, attrs = records + 32;
            Check(Read(m, cats + 8) == 1 && Read(m, cats + 24) == 1, "category child counts");
            Check(Read(m, attrs + 4) == records && Read(m, attrs + 20) == records + 16,
                  "parent category start");
            Check(CString(m, Read(m, cats)) == "CategoryA" &&
                      CString(m, Read(m, cats + 16)) == "CategoryB",
                  "category names");
            ++cases;
        }
        {
            Group(groups, "all_types_partial_vectors_and_skip_paths");
            std::vector<Row> rows;
            const std::array<std::string, 11> tokens = {"yes;",  "nan;",  "neg;",  "hi;",
                                                        "wide;", "vec;",  "vec;",  "vec;",
                                                        "ref;",  "zero;", "other;"};
            const std::array<U, 11> types = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0, UINT32_MAX};
            for (U k = 0; k < types.size(); ++k)
                rows.push_back(
                    {"C/r/" + std::string(1, static_cast<char>('a' + k)), tokens[k], types[k]});
            F f(rows);
            f.Prepare();
            f.Run();
            auto m = f.M();
            U attr = F::out + 52, words = attr + 16u * 11u;
            Check(Read(m, words) == 1 && Read(m, words + 4) == 0x7FC12345 &&
                      Read(m, words + 8) == 0x80000000,
                  "numeric return bits");
            Check(CString(m, Read(m, words + 12)) == "hi", "string pool value");
            for (U k = 0; k < 11; ++k) {
                const bool skip =
                    types[k] == 5 || types[k] == 9 || types[k] == 0 || types[k] == UINT32_MAX;
                Check(Read(m, attr + 16 * k + 8, 2) == (skip ? 0u : 1u), "typed count");
                Check(Read(m, attr + 16 * k + 10, 1) == (types[k] & 255u) &&
                          Read(m, attr + 16 * k + 11, 1) == 0,
                      "type/usage narrowing");
                if (skip)
                    Check(Read(m, words + 4 * k) == 0xA5A5A5A5, "skipped value seed");
            }
            for (U k = 5; k <= 7; ++k) {
                U p = Read(m, words + 4 * k);
                for (U j = 0; j < k - 3; ++j)
                    Check(Read(m, p + j * 4) == 0x3F000000u + j, "vector callback stores");
            }
            Check(f.numcalls == 6, "only consumed numeric imports");
            ++cases;
        }
        {
            Group(groups, "u32_context_wrap_and_allocation_math");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            auto m = f.M();
            for (U k = 0; k < 4; ++k)
                Put(m, F::context + k * 4, UINT32_MAX);
            Put(m, F::context + 16, 0xFFFFFFF0);
            f.Size();
            Check(Read(m, F::context) == 0 && Read(m, F::context + 4) == 0 &&
                      Read(m, F::context + 8) == 0 && Read(m, F::context + 12) == 0,
                  "context wrap");
            Check(Read(m, F::context + 16) == 0xFFFFFFFA, "pool budget wrap");
            Check(Monstey_FearUnpackedAllocationBytes({F::base, f.bytes}, F::context) == 14,
                  "allocation wrap");
            ++cases;
        }
        {
            Group(groups, "cursor_alias_context_sequential_reservations");
            F f(std::vector<Row>{});
            auto m = f.M();
            for (U k = 0; k < 5; ++k)
                Put(m, F::context + 4 * k, k + 1);
            f.imports.string = [](Mem, Op, U, U, U, U) -> U {
                throw std::runtime_error("stop after reservation");
            };
            bool stopped = false;
            try {
                Monstey_FearUnpackedMaterialize(m, F::context + 4, F::mf, f.imports);
            } catch (const std::runtime_error &) {
                stopped = true;
            }
            Check(stopped, "reservation stop");
            Check(Read(m, F::mf + 0x3C) == 2 && Read(m, F::mf + 0x24) == 18 &&
                      Read(m, F::mf + 0x40) == 306,
                  "sequential alias bases");
            Check(Read(m, F::context + 4) == 375, "cursor final375");
            ++cases;
        }
        {
            Group(groups, "cursor_wrap_without_eager_range_validation");
            F f(std::vector<Row>{});
            auto m = f.M();
            Put(m, F::context, 2);
            Put(m, F::context + 4, 0);
            Put(m, F::context + 8, 0);
            Put(m, F::context + 12, 0);
            Put(m, F::context + 16, 0);
            Put(m, F::cursor, 0xFFFFFFF0);
            f.imports.string = [](Mem, Op, U, U, U, U) -> U {
                throw std::runtime_error("reserved");
            };
            try {
                f.Run();
            } catch (const std::runtime_error &) {
            }
            Check(Read(m, F::cursor) == 0x10 && Read(m, F::mf + 0x3C) == 0xFFFFFFF0,
                  "cursor wraps before later consumption");
            ++cases;
        }
        {
            Group(groups, "atoi_mutates_live_value_cursor");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            f.Prepare();
            f.integer = [](Mem m, U) {
                Put(m, F::mf + 0x14, 0x1A000);
                return 0xDEADBEEF;
            };
            f.Run();
            auto m = f.M();
            Check(Read(m, 0x1A000) == 0xDEADBEEF && Read(m, F::out + 52 + 12) == 0x1A000,
                  "cursor selected after callback");
            Check(Read(m, F::out + 68) == 0xA5A5A5A5, "old cell untouched");
            ++cases;
        }
        {
            Group(groups, "missing_numeric_boundary_prefix");
            F f(std::vector<Row>{{"C/r/a", "11;", 3, 0xAABBCCDD}});
            f.Prepare();
            f.imports.atoi_bits = {};
            bool fault = false;
            try {
                f.Run();
            } catch (const Monstey_FearUnpackedBoundaryFault &e) {
                fault = e.boundary == "atoi_bits";
            }
            Check(fault && Read(f.M(), F::context + 12) == 2, "numeric gate after token increment");
            Check(Read(f.M(), F::mf + 0x9C) == 0xAABBCCDD && Read(f.M(), F::out) == 0xA5A5A5A5,
                  "prefix opaque/no root publication");
            ++cases;
        }
        {
            Group(groups, "sscanf_partial_failure_and_offset_epoch");
            F f(std::vector<Row>{{"C/r/a", "vec;", 8}});
            f.Prepare();
            U destination = 0;
            f.scan = [&](Mem m, U, U dims, std::array<U, 4> d) -> U {
                Check(dims == 4, "dims4");
                destination = d[0];
                Put(m, d[0], 0x12345678);
                throw std::runtime_error("supplied scan fault");
            };
            bool fault = false;
            try {
                f.Run();
            } catch (const std::runtime_error &) {
                fault = true;
            }
            Check(fault && destination != 0 && Read(f.M(), destination) == 0x12345678 &&
                      Read(f.M(), destination + 4) == 0xA5A5A5A5,
                  "scan partial writes");
            Check(Read(f.M(), F::mf + 0x10) == 0 && Read(f.M(), F::context + 12) == 2,
                  "offset not published before scan returns");
            ++cases;
        }
        {
            Group(groups, "substring_mutates_type_and_context_argument");
            F f(std::vector<Row>{{"C/r/a", "other;", 9}});
            f.Prepare();
            auto m = f.M();
            constexpr U second = 0x1B00;
            for (U k = 0; k < 6; ++k)
                Put(m, second + k * 4, Read(m, F::context + k * 4));
            f.after = [](Mem x, Op op, U self, U a, U, U) {
                if (op == Op::substr && self == F::mf + 0x80 && a == F::mf + 0x44) {
                    Put(x, F::mf + 0x7C, 3);
                    Put(x, F::mf + 0x134, second);
                }
            };
            f.integer = [](Mem, U) { return 77u; };
            f.Run();
            Check(Read(f.M(), second + 12) == 2 && Read(f.M(), F::context + 12) == 1,
                  "late context selection");
            Check(Read(f.M(), F::out + 52 + 10, 1) == 3 && Read(f.M(), F::out + 68) == 77,
                  "late type selection");
            ++cases;
        }
        {
            Group(groups, "empty_value_owned_subscript_fault_prefix");
            F f(std::vector<Row>{{"C/r/a", ""}});
            bool fault = false;
            try {
                f.Size();
            } catch (const std::out_of_range &) {
                fault = true;
            }
            Check(fault && Read(f.M(), F::context) == 1 && Read(f.M(), F::context + 4) == 1 &&
                      Read(f.M(), F::context + 8) == 1 && Read(f.M(), F::context + 12) == 0,
                  "last-byte boundary not normalized");
            ++cases;
        }
        {
            Group(groups, "empty_map_size_and_materializer_first_key_epoch");
            F f(std::vector<Row>{});
            f.Size();
            for (U k = 0; k < 5; ++k)
                Check(Read(f.M(), F::context + 4 * k) == 0, "empty sizing");
            f.imports.string = [](Mem, Op op, U, U, U, U) -> U {
                if (op == Op::rfind)
                    throw Monstey_FearUnpackedBoundaryFault("first-key");
                return 0;
            };
            bool first = false;
            try {
                f.Run();
            } catch (const Monstey_FearUnpackedBoundaryFault &e) {
                first = e.boundary == "first-key";
            }
            Check(first, "key consumed before own empty check");
            ++cases;
        }
        {
            Group(groups, "attribute_constructor_partial_write_prefix");
            F f(std::vector<Row>{{"C/r/a", "wide;", 5}});
            f.Prepare();
            auto full = f.M();
            U pool = F::out + 72;
            Put(full, F::context + 20, pool);
            const auto limited = Mem{full.base, full.bytes.first(F::out + 62 - F::base)};
            bool fault = false;
            try {
                Monstey_FearUnpackedMaterialize(limited, F::cursor, F::mf, f.imports);
            } catch (const Monstey_FearDbFault &e) {
                fault = e.writing && e.address == F::out + 62 && e.width == 1;
            }
            Check(fault, "constructor type-byte fault");
            Check(Read(full, F::out + 52) == pool && Read(full, F::out + 56) == 0xA5A5A5A5,
                  "name before type,parent later");
            ++cases;
        }
        {
            Group(groups, "standalone_link_fixup_live_reference_and_null");
            F f(std::vector<Row>{});
            auto m = f.M();
            U root = 0x11000, cat = 0x11100, record = 0x11200, attr = 0x11300, words = 0x11400;
            Put(m, root + 8, 1);
            Put(m, root + 12, cat);
            Put(m, cat + 4, root);
            Put(m, cat + 8, 1);
            Put(m, cat + 12, record);
            Put(m, record + 4, cat);
            Put(m, record + 8, 1);
            Put(m, record + 12, attr);
            Put(m, attr + 4, record);
            Put(m, attr + 8, 3, 2);
            Put(m, attr + 10, 9, 1);
            Put(m, attr + 12, words);
            Put(m, words, 0);
            Put(m, words + 4, 0x10000);
            Put(m, words + 8, 1);
            Monstey_FearUnpackedFixLinks(m, root);
            Check(Read(m, words) == record && Read(m, words + 4) == 0 && Read(m, words + 8) == 0,
                  "link ordinal bounds");
            ++cases;
        }
        {
            Group(groups, "scalar_bit_matrix_and_all_reserved_prefixes");
            for (U k = 0; k < 128; ++k) {
                F f(std::vector<Row>{{"C/r/a", "11;"}});
                f.Prepare();
                const U bits = k * 0x9E3779B9u;
                f.integer = [bits](Mem, U) { return bits; };
                f.Run();
                Check(Read(f.M(), F::out + 68) == bits, "raw scalar matrix");
                ++cases;
            }
        }
        {
            Group(groups, "name_copy_mutates_pool_offset_before_constructor");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            f.Prepare();
            bool once = false;
            f.imports.strncpy = [&](Mem m, U d, U source, U n) {
                Monstey_FearUnpackedForwardStrncpy(m, d, source, n);
                if (!once) {
                    once = true;
                    Put(m, F::mf + 0x10, 5);
                }
            };
            f.Run();
            Check(Read(f.M(), F::out + 52) == F::out + 72 + 5, "helper name selected after copy");
            Check(Read(f.M(), F::mf + 0x10) == 11, "offset resampled after imported copy");
            ++cases;
        }
        {
            Group(groups, "numeric_mutates_delimiter_before_next_token");
            F f(std::vector<Row>{{"C/r/a", "11;22;"}});
            f.Prepare();
            f.integer = [](Mem m, U) {
                Put(m, F::mf + 0x30, 5);
                return 11u;
            };
            f.Run();
            Check(Read(f.M(), F::out + 52 + 8, 2) == 1 && Read(f.M(), F::context + 12) == 3,
                  "next-token delimiter late load");
            Check(Read(f.M(), F::out + 72) == 0xA5A5A5A5, "skipped second token cell");
            ++cases;
        }
        {
            Group(groups, "sizing_type_uses_live_iterator_after_substring");
            F f(std::vector<Row>{{"C/r/a", "abcde;", 4}, {"C/r/b", "33;", 3}});
            f.after = [](Mem m, Op op, U self, U a, U, U) {
                if (op == Op::substr && self == F::first + 0x2C && a == F::sf + 0x9C)
                    Put(m, F::sf + 0x0C, F::first + 0x80);
            };
            f.Size();
            Check(Read(f.M(), F::context + 8) == 1 && Read(f.M(), F::context + 12) == 1,
                  "live iterator affects successor");
            Check(Read(f.M(), F::context + 16) == 10, "live type changes token budget");
            ++cases;
        }
        {
            Group(groups, "wide_raw_type_narrows_only_after_dispatch");
            F f(std::vector<Row>{{"C/r/a", "other;", 0x101}, {"C/r/b", "other;", 0x109}});
            f.Prepare();
            f.Run();
            Check(Read(f.M(), F::out + 52 + 10, 1) == 1 && Read(f.M(), F::out + 68 + 10, 1) == 9,
                  "low8 type header");
            Check(Read(f.M(), F::out + 52 + 8, 2) == 0 && Read(f.M(), F::out + 68 + 8, 2) == 0 &&
                      f.numcalls == 0,
                  "raw type dispatch before narrow");
            ++cases;
        }
        {
            Group(groups, "actual_count65537_low16_and_full_writes");
            F f(std::vector<Row>{{"C/r/a", "11;", 3}});
            f.bytes.resize(0x300000, 0xA5);
            auto m = f.M();
            constexpr U count = 65537, data = 0x100000;
            Put(m, F::first + 0x2C + 4, data);
            Put(m, F::first + 0x2C + 0x14, count * 3u);
            Put(m, F::first + 0x2C + 0x18, count * 3u);
            for (U n = 0; n < count; ++n) {
                Put(m, data + 3 * n, '1', 1);
                Put(m, data + 3 * n + 1, '1', 1);
                Put(m, data + 3 * n + 2, ';', 1);
            }
            Put(m, data + count * 3u, 0, 1);
            f.heap = 0x140000;
            f.logical.allocate = [&](Mem, U n) {
                U at = f.heap;
                f.heap += n + 16u;
                return at;
            };
            for (U k = 0; k < 3; ++k)
                Put(m, F::context + 4 * k, 1);
            Put(m, F::context + 12, count);
            Put(m, F::context + 16, count * 4u + 6u);
            Put(m, F::context + 20, F::out + 20 + 48 + count * 8u + 6u);
            auto ordinary = f.imports.string;
            f.imports.string = [&](Mem x, Op op, U self, U a, U b, U c) -> U {
                if (self == F::mf + 0x80 && (op == Op::find || op == Op::substr)) {
                    U d = Read(x, self + 4), length = Read(x, self + 0x14);
                    if (op == Op::find) {
                        for (U n = b; n < length; ++n)
                            if (Read(x, d + n, 1) == a)
                                return n;
                        return UINT32_MAX;
                    }
                    Check(b <= length, "large substring position");
                    std::string t;
                    for (U n = 0; n < std::min(c, length - b); ++n)
                        t.push_back(static_cast<char>(Read(x, d + b + n, 1)));
                    f.Bind(a, t);
                    return a;
                }
                return ordinary(x, op, self, a, b, c);
            };
            U constructorCount = 0, constructorCalls = 0;
            f.after = [&](Mem x, Op op, U self, U, U, U) {
                if (op == Op::c_str && self == F::mf - 0x38u && ++constructorCalls == 2)
                    constructorCount = Read(x, F::mf - 0x50u);
            };
            f.integer = [](Mem, U) { return 9u; };
            f.Run();
            Check(constructorCount == count, "full U32 count PUSH precedes U16 narrowing");
            constexpr U words = F::out + 68;
            Check(Read(m, F::out + 52 + 8, 2) == 1, "real 65537 count narrows low16");
            Check(Read(m, words) == 9 && Read(m, words + (count - 1u) * 4u) == 9,
                  "full physical token cells written");
            Check(Read(m, F::context + 12) == count * 2u && Read(m, F::mf + 0x14) == words + 4u,
                  "full count and cursor retained");
            ++cases;
        }
        {
            Group(groups, "shared_callback_cstring_constructor_and_less");
            F f(std::vector<Row>{});
            auto m = f.M();
            Put(m, 0x1A00, 'z', 1);
            Put(m, 0x1A01, 0, 1);
            f.logical.Invoke(m, Op::construct_cstring, 0x1B00, 0x1A00);
            Check(CString(m, f.logical.Invoke(m, Op::c_str, 0x1B00)) == "z", "cstring primitive");
            f.Bind(0x1C00, "a");
            Check(f.logical.Invoke(m, Op::less, 0x1C00, 0x1B00) == 1 &&
                      f.logical.Invoke(m, Op::less, 0x1B00, 0x1C00) == 0,
                  "owned lexical less");
            ++cases;
        }
        {
            Group(groups, "value_captured_string_and_numeric_provider_state");
            F f(std::vector<Row>{{"C/r/a", "11;"}, {"C/r/b", "33;"}});
            auto ordinary = f.imports.string;
            f.imports.string = [ordinary, count = U{0}](Mem m, Op op, U self, U a, U b,
                                                        U c) mutable {
                Put(m, 0x1D00, ++count);
                return ordinary(m, op, self, a, b, c);
            };
            f.imports.atoi_bits = [count = U{0}](Mem, U) mutable { return ++count; };
            f.Prepare();
            f.Run();
            Check(Read(f.M(), 0x1D00) == f.strcalls && f.strcalls > 30,
                  "persistent string provider target state");
            Check(Read(f.M(), F::out + 84) == 1 && Read(f.M(), F::out + 88) == 2,
                  "persistent numeric provider target state");
            ++cases;
        }
        {
            Group(groups, "numeric_provider_self_replacement_lifetime");
            F f(std::vector<Row>{{"C/r/a", "11;"}, {"C/r/b", "33;"}});
            f.Prepare();
            auto lifetime = std::make_shared<U>(10);
            const std::weak_ptr<U> weak = lifetime;
            f.imports.atoi_bits = [&f, weak, state = std::move(lifetime)](Mem, U) {
                f.imports.atoi_bits = [](Mem, U) { return 33u; };
                Check(!weak.expired(), "active numeric target retained after replacement");
                return ++*state;
            };
            f.Run();
            Check(weak.expired(), "old numeric target released after return");
            Check(Read(f.M(), F::out + 84) == 11 && Read(f.M(), F::out + 88) == 33,
                  "subsequent call selects replacement target");
            ++cases;
        }
        {
            Group(groups, "shared_callable_nested_replacement_and_void");
            Monstey_FearDbCallable<U(U)> provider;
            auto first = std::make_shared<U>(1);
            auto second = std::make_shared<U>(2);
            const std::weak_ptr<U> firstWeak = first, secondWeak = second;
            provider = [&provider, firstWeak, secondWeak, state = std::move(first),
                        next = std::move(second)](U) mutable {
                provider = [&provider, secondWeak, state = std::move(next)](U) {
                    provider = [](U n) { return n + 40u; };
                    Check(!secondWeak.expired(), "nested active target retained");
                    Check(*state == 2, "nested target state alive");
                    return provider(2) + 1u;
                };
                Check(!firstWeak.expired() && *state == 1, "outer active target retained");
                const auto result = provider(1);
                Check(!firstWeak.expired(), "outer target survives nested return");
                return result + 1u;
            };
            Check(provider(0) == 44, "nested replacement return order");
            Check(firstWeak.expired() && secondWeak.expired(), "replaced targets released");
            Check(provider(3) == 43, "latest target remains selected");
            Monstey_FearDbCallable<void()> empty;
            bool completed = false;
            empty = [&] {
                empty = nullptr;
                completed = true;
            };
            empty();
            Check(completed && !empty, "void self-clear target lifetime");
            ++cases;
        }
        {
            Group(groups, "sizing_counter_iterator_alias_read_epoch");
            F f(std::vector<Row>{{"C/r/a", "abcde;", 4}});
            auto m = f.M();
            for (U n = 0; n < 6; ++n)
                Put(m, F::sf + 4u * n, 0);
            f.after = [](Mem, Op op, U self, U a, U b, U) {
                if (op == Op::find && self == F::first + 0x2Cu && a == 59 && b == 6)
                    throw std::runtime_error("alias-prefix-stop");
            };
            bool stopped = false;
            try {
                Monstey_FearUnpackedSize(m, F::sf, F::sf, f.imports);
            } catch (const std::runtime_error &e) {
                stopped = std::string(e.what()) == "alias-prefix-stop";
            }
            Check(stopped, "alias prefix reaches next find");
            Check(Read(m, F::sf + 0x0Cu) == F::first + 1u,
                  "counter write changes aliased iterator");
            Check(Read(m, F::sf + 0x10u) == 12,
                  "iterator sampled before counter store for type budget");
            ++cases;
        }
        {
            Group(groups, "first_npos_compare_precedes_delimiter_store");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            f.Prepare();
            Put(f.M(), 0x1700, F::mf + 0x30u);
            f.Run();
            Check(Read(f.M(), F::out + 52u + 8u, 2) == 1,
                  "first npos sampled before delimiter store");
            Check(Read(f.M(), F::out + 68u) == 11, "aliased npos first token converted");
            ++cases;
        }
        {
            Group(groups, "delimiter_reloaded_after_token_destructor");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            f.Prepare();
            f.after = [](Mem m, Op op, U self, U a, U b, U c) {
                if (op == Op::destroy && self == F::mf + 0x44u)
                    Put(m, F::mf + 0x30u, 0);
                if (op == Op::substr && self == F::mf + 0x80u && a == F::mf + 0x44u && b == 3 &&
                    c == 0xFFFFFFFDu)
                    throw std::runtime_error("destructor-prefix-stop");
            };
            bool stopped = false;
            try {
                f.Run();
            } catch (const std::runtime_error &e) {
                stopped = std::string(e.what()) == "destructor-prefix-stop";
            }
            Check(stopped, "destructor delimiter late reload");
            Check(Read(f.M(), F::context + 12u) == 2 && Read(f.M(), F::out + 68u) == 11,
                  "destructor mutation preserves preceding conversion");
            ++cases;
        }
        {
            Group(groups, "attribute_constructor_late_raw_argument_reads");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            f.Prepare();
            f.integer = [](Mem m, U) {
                Put(m, F::mf + 0x40u, F::mf - 0x54u);
                return 11u;
            };
            U checkpoints = 0;
            f.after = [&](Mem, Op op, U self, U, U, U) {
                if (op == Op::c_str && self == F::mf - 0x38u && ++checkpoints == 2)
                    throw std::runtime_error("attribute-ctor-stop");
            };
            bool stopped = false;
            try {
                f.Run();
            } catch (const std::runtime_error &e) {
                stopped = std::string(e.what()) == "attribute-ctor-stop";
            }
            const U out = F::mf - 0x54u;
            Check(stopped, "attribute ctor checkpoint");
            Check(Read(f.M(), out + 10u, 1) == 3 && Read(f.M(), out + 11u, 1) == 0x48,
                  "attribute type retained through aliased usage selection");
            Check(Read(f.M(), out + 12u) == 0x48030044u,
                  "attribute values sampled after overlapping type and usage stores");
            Check(Read(f.M(), out + 8u, 2) == 1 && Read(f.M(), out + 4u) == F::out + 36u,
                  "attribute remaining metadata");
            ++cases;
        }
        {
            Group(groups, "node_constructor_parent_sampled_after_name_store");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            f.Prepare();
            U checkpoints = 0;
            f.after = [&](Mem m, Op op, U self, U, U, U) {
                if (op == Op::destroy && self == F::mf + 0x80u)
                    Put(m, F::mf + 0x24u, F::mf - 0x4Cu);
                if (op == Op::c_str && self == F::mf - 0x30u && ++checkpoints == 2)
                    throw std::runtime_error("node-ctor-stop");
            };
            bool stopped = false;
            try {
                f.Run();
            } catch (const std::runtime_error &e) {
                stopped = std::string(e.what()) == "node-ctor-stop";
            }
            const U out = F::mf - 0x4Cu;
            Check(stopped, "node ctor checkpoint");
            Check(Read(f.M(), out) == F::out + 74u && Read(f.M(), out + 4u) == Read(f.M(), out),
                  "node parent sampled after name overwrites argument");
            Check(Read(f.M(), out + 8u) == 1 && Read(f.M(), out + 12u) == F::out + 52u,
                  "node count and children epochs");
            ++cases;
        }
        {
            Group(groups, "root_constructor_allocation_sampled_after_count_store");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            f.Prepare();
            const U root = F::mf - 12u;
            Put(f.M(), F::mf + 0x130u, root);
            Put(f.M(), F::mf + 0x13Cu, 0x12345678u);
            Put(f.M(), 0x1704, F::out + 20u);
            f.Run();
            Check(Read(f.M(), root) == 1,
                  "root allocation sampled after count overwrites argument");
            Check(Read(f.M(), root + 4u) == F::out + 20u && Read(f.M(), root + 8u) == 1 &&
                      Read(f.M(), root + 12u) == F::out + 20u && Read(f.M(), root + 16u) == 0,
                  "root aliased constructor completes postpass");
            ++cases;
        }
        {
            Group(groups, "constructor_push_fault_preserves_name_copy_prefix");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            f.Prepare();
            constexpr U frame = F::base + 0x48u;
            for (U k = 0; k < 4; ++k)
                Put(f.M(), frame + 0x130u + 4u * k, Read(f.M(), F::mf + 0x130u + 4u * k));
            bool fault = false;
            try {
                (void)Monstey_FearUnpackedMaterialize(f.M(), F::cursor, frame, f.imports);
            } catch (const Monstey_FearDbFault &e) {
                fault = e.writing && e.address == F::base - 4u && e.width == 4;
            }
            Check(fault, "first inner constructor PUSH fault epoch");
            Check(Read(f.M(), F::out + 68u) == 11 && CString(f.M(), F::out + 72u) == "a",
                  "value and name copy precede ctor PUSH fault");
            Check(Read(f.M(), F::out + 52u) == 0xA5A5A5A5u,
                  "constructor output untouched before failing PUSH");
            ++cases;
        }
        {
            Group(groups, "constructor_push_full_words_before_header_narrowing");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            f.Prepare();
            U checkpoints = 0, rawType = 0, rawUsage = 0, rawCount = 0;
            f.after = [&](Mem m, Op op, U self, U, U, U) {
                if (op == Op::copy && self == F::mf - 0x38u) {
                    Put(m, F::mf - 0x14u, 0x55667788u);
                    Put(m, F::mf - 0x10u, 0x11223344u);
                    Put(m, F::mf - 12u, 0x10001u);
                }
                if (op == Op::c_str && self == F::mf - 0x38u && ++checkpoints == 2) {
                    rawType = Read(m, F::mf - 0x58u);
                    rawUsage = Read(m, F::mf - 0x54u);
                    rawCount = Read(m, F::mf - 0x50u);
                }
            };
            f.Run();
            Check(rawType == 0x55667788u && rawUsage == 0x11223344u && rawCount == 0x10001u,
                  "constructor PUSH words retain high bits");
            Check(Read(f.M(), F::out + 52u + 10u, 1) == 0x88 &&
                      Read(f.M(), F::out + 52u + 11u, 1) == 0x44 &&
                      Read(f.M(), F::out + 52u + 8u, 2) == 1,
                  "constructor header narrows on consumed reads");
            ++cases;
        }
        {
            Group(groups, "root_name_global_sampled_after_allocation_push");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            f.Prepare();
            f.imports.root_name_cell = F::mf - 4u;
            Put(f.M(), F::mf + 0x13Cu, 0xAABBCCDDu);
            f.Run();
            Check(Read(f.M(), F::out + 4u) == 0xAABBCCDDu,
                  "root name global sees preceding allocation PUSH");
            ++cases;
        }
        {
            Group(groups, "node_name_offset_sampled_before_count_push");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            f.Prepare();
            U checkpoints = 0;
            f.after = [&](Mem m, Op op, U self, U, U, U) {
                if (op == Op::copy && self == F::mf - 0x30u) {
                    Put(m, F::mf - 4u, F::mf - 0x48u);
                    Put(m, F::mf - 0x48u, 7);
                }
                if (op == Op::c_str && self == F::mf - 0x30u && ++checkpoints == 2)
                    throw std::runtime_error("node-offset-stop");
            };
            bool stopped = false;
            try {
                f.Run();
            } catch (const std::runtime_error &e) {
                stopped = std::string(e.what()) == "node-offset-stop";
            }
            Check(stopped && Read(f.M(), F::out + 36u) == F::out + 79u,
                  "node name offset read before count PUSH overwrites it");
            Check(Read(f.M(), F::mf - 0x48u) == 1, "node count PUSH updates aliased offset word");
            ++cases;
        }
        {
            Group(groups, "attribute_name_offset_sampled_after_values_push");
            F f(std::vector<Row>{{"C/r/a", "11;"}});
            f.Prepare();
            U checkpoints = 0;
            f.after = [&](Mem m, Op op, U self, U, U, U) {
                if (op == Op::copy && self == F::mf - 0x38u) {
                    Put(m, F::mf - 4u, F::mf - 0x4Cu);
                    Put(m, F::mf - 0x4Cu, 7);
                }
                if (op == Op::c_str && self == F::mf - 0x38u && ++checkpoints == 2)
                    throw std::runtime_error("attribute-offset-stop");
            };
            bool stopped = false;
            try {
                f.Run();
            } catch (const std::runtime_error &e) {
                stopped = std::string(e.what()) == "attribute-offset-stop";
            }
            Check(stopped && Read(f.M(), F::out + 52u) == 0x2008Cu,
                  "attribute name offset sees preceding values PUSH");
            Check(Read(f.M(), F::mf - 0x4Cu) == F::out + 68u,
                  "attribute values PUSH updates aliased offset word");
            ++cases;
        }
        auto arr = Monstey_Json::array();
        for (const auto &g : groups)
            arr.as_array().push_back(g);
        Monstey_WriteNewText(
            argv[1], Monstey_Json::object(
                         {{"status", "PASS"},
                          {"groups", groups.size()},
                          {"cases", cases},
                          {"checks", checks},
                          {"group_names", arr},
                          {"original_executed", false},
                          {"numeric_results",
                           "explicit supplied fixture bits/writes, no host CRT equivalence"},
                          {"physical_MSVCP_x87_and_machine_stack_ABI_claimed", false}})
                             .dump(2) +
                         "\n");
        std::cout << "PASS " << groups.size() << " groups " << cases << " cases " << checks
                  << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
