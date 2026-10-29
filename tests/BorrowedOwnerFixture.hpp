#include "Monstey_Core.hpp"
#include "Monstey_FearUnpackedOpen.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using U = std::uint32_t;
using Mem = Monstey_FearDbMutableMemory;
using Op = Monstey_FearUnpackedStringOp;
std::uint64_t checks = 0, cases = 0;
std::string current_group;
void Check(bool v, const char *text) {
    ++checks;
    if (!v)
        throw std::runtime_error(text);
}
U R(const Mem &m, U at, U width = 4) {
    Check(at >= m.base && std::uint64_t(at) - m.base + width <= m.bytes.size(),
          "fixture read range");
    U n = 0;
    for (U i = 0; i < width; ++i)
        n |= U(m.bytes[at - m.base + i]) << (8 * i);
    return n;
}
void W(Mem &m, U at, U n, U width = 4) {
    Check(at >= m.base && std::uint64_t(at) - m.base + width <= m.bytes.size(),
          "fixture write range");
    for (U i = 0; i < width; ++i)
        m.bytes[at - m.base + i] = static_cast<std::uint8_t>(n >> (8 * i));
}
std::string Cstr(const Mem &m, U at) {
    std::string s;
    for (;;) {
        U c = R(m, at++, 1);
        if (!c)
            return s;
        s += static_cast<char>(c);
    }
}
void Append(std::vector<std::uint8_t> &b, U n, U width = 4) {
    for (U i = 0; i < width; ++i)
        b.push_back(static_cast<std::uint8_t>(n >> (8 * i)));
}
struct Row {
    std::string key;
    U type = 3, word4 = 0, word8 = 0;
};
std::vector<std::uint8_t> ConstraintBytes(const std::vector<Row> &rows) {
    std::vector<std::uint8_t> b;
    for (const auto &r : rows) {
        Append(b, static_cast<U>(r.key.size()));
        for (unsigned char c : r.key)
            b.push_back(c);
        Append(b, r.type, 1);
        Append(b, 0xBEEF1234);
        Append(b, r.word4);
        Append(b, r.word8);
    }
    return b;
}
struct Allocation {
    U address, bytes, site;
    std::string kind;
    bool released = false;
};
struct Event {
    std::string kind;
    U site = 0, a = 0, b = 0, c = 0;
};
struct Stream {
    std::vector<std::uint8_t> bytes;
    U position = 0, read_calls = 0;
};
struct Fixture {
    static constexpr U base = 0x10000000u, frame = base + 0x20000u, wrapper = frame + 0x58u,
                       textStream = base + 0x1000u, constraintStream = base + 0x1100u,
                       textVtable = base + 0x1200u, constraintVtable = base + 0x1240u;
    std::vector<std::uint8_t> image = std::vector<std::uint8_t>(0x200000, 0xA5), replacement;
    Mem memory{base, image};
    Monstey_FearUnpackedOpenServices services;
    Monstey_FearUnpackedCallbackLogicalMaps maps;
    std::map<U, Stream> streams;
    std::map<std::pair<U, std::string>, U> constraintValues;
    std::vector<Allocation> allocations;
    std::vector<Event> events;
    U next = base + 0x80000u, nextNode = base + 0x60000u, nextString = base + 0x120000u,
      lastNode = 0, validations = 0;
    std::function<void(Mem &, Monstey_FearUnpackedCallbackStringCall)> stringHook;
    std::function<void(Mem &, Monstey_FearUnpackedDeleteCall)> deleteHook;
    std::function<void(Mem &, Monstey_FearUnpackedEraseCall)> eraseHook;
    std::function<U(Mem &, Monstey_FearConstraintReadCall)> textReadOverride;
    bool failConstraintRead = false, failOutputAllocation = false, nullEmptyRoot = false;
    std::uint64_t textLengthHigh = 0;
    U B() const { return ((frame - 0x18u) & 0xFFFFFFF8u) - 0x410u; }
    U Allocate(U bytes, U site, const std::string &kind) {
        const U address = next;
        next += (bytes + 15u) & ~15u;
        Check(next < base + 0x110000u, "fixture ordinary allocation capacity");
        allocations.push_back({address, bytes, site, kind, false});
        events.push_back({"allocate", site, address, bytes});
        return address;
    }
    void Release(U address, U site, const char *kind) {
        events.push_back({kind, site, address});
        for (auto &x : allocations)
            if (x.address == address) {
                x.released = true;
                return;
            }
        if (address != 0)
            throw std::runtime_error("Unknown fixture deletion");
    }
    std::string String(U object) {
        const U data = maps.strings.Invoke(memory, Op::c_str, object),
                length = R(memory, object + 0x14u);
        std::string s;
        for (U i = 0; i < length; ++i)
            s += static_cast<char>(R(memory, data + i, 1));
        return s;
    }
    U ReadStream(Monstey_FearConstraintReadCall call, bool text) {
        auto &stream = streams.at(call.stream);
        ++stream.read_calls;
        events.push_back({text ? "read-text" : "read-constraints", 0, call.destination, call.bytes,
                          call.target});
        if (!text && failConstraintRead)
            return 1;
        if (call.bytes > stream.bytes.size() - stream.position)
            return 1;
        for (U i = 0; i < call.bytes; ++i)
            W(memory, call.destination + i, stream.bytes[stream.position + i], 1);
        stream.position += call.bytes;
        return 0;
    }
    Fixture(std::vector<Row> rows, std::string text) {
        streams[constraintStream].bytes = ConstraintBytes(rows);
        streams[textStream].bytes.assign(text.begin(), text.end());
        W(memory, textStream, textVtable);
        W(memory, constraintStream, constraintVtable);
        for (U p : std::array<U, 2>{textVtable, constraintVtable}) {
            W(memory, p + 4u, p + 0xA004u);
            W(memory, p + 0x18u, p + 0xA018u);
            W(memory, p + 0x1Cu, p + 0xA01Cu);
        }
        for (U slot :
             std::array<U, 10>{0x1000B044u, 0x1000B040u, 0x1000B050u, 0x1000B054u, 0x1000B064u,
                               0x1000B068u, 0x1000B06Cu, 0x1000B070u, 0x1000B074u, 0x1000B078u})
            W(memory, slot, slot + 0x10000u);
        W(memory, 0x1000B048u, 0x1000D080u);
        W(memory, 0x1000D080u, UINT32_MAX);
        W(memory, 0x1000B1E0u, 0, 1);
        W(memory, 0x1000D078u, 0x1000D100u);
        W(memory, 0x1000D100u, 0, 1);
        W(memory, wrapper + 4u, textStream);
        W(memory, wrapper + 8u, constraintStream);
        W(memory, B() - 0x10u, 0xD0C0B0A0u);
        W(memory, B() - 0xE8u, 0x1234ABCDu);
        maps.strings.allocate = [this](Mem, U bytes) {
            U address = nextString;
            nextString += (bytes + 15u) & ~15u;
            Check(nextString < base + 0x1F0000u, "logical string capacity");
            return address;
        };
        maps.allocate = [this](Mem, U bytes) {
            U address = nextNode;
            nextNode += (bytes + 15u) & ~15u;
            lastNode = address;
            return address;
        };
        services.new_sentinel = [this](Mem &m, bool parsed, U map, U site) {
            U bytes = parsed ? 0x50u : 0x38u;
            const U address = Allocate(bytes, site, parsed ? "parsed-head" : "constraint-head");
            W(m, address, 0);
            W(m, address + 4u, 0);
            W(m, address + 8u, 0);
            W(m, address + (parsed ? 0x4Cu : 0x34u), 1, 1);
            W(m, address + (parsed ? 0x4Du : 0x35u), 0, 1);
            events.push_back({"sentinel", site, map, address, parsed ? 1u : 0u});
            return address;
        };
        services.constraints.get_length = [this](Monstey_FearConstraintCall c) {
            events.push_back({"constraint-length", 0, c.object, c.target});
            return std::uint64_t(streams.at(c.object).bytes.size());
        };
        services.constraints.get_position = [this](Monstey_FearConstraintCall c) {
            return std::uint64_t(streams.at(c.object).position);
        };
        services.constraints.read = [this](Monstey_FearConstraintReadCall c) {
            return ReadStream(c, false);
        };
        services.constraints.new_array = [this](U bytes) {
            return Allocate(bytes, 0, "constraint-name");
        };
        services.constraints.delete_array = [this](U address) {
            Release(address, 0, "delete-constraint-name");
        };
        services.constraints.string_default = [this](Monstey_FearConstraintCall c) {
            maps.strings.Invoke(memory, Op::construct, c.object);
        };
        services.constraints.string_assign_cstr = [this](Monstey_FearConstraintCall c, U source) {
            maps.strings.Invoke(memory, Op::construct_cstring, c.object, source);
        };
        services.constraints.string_destroy = [this](Monstey_FearConstraintCall c) {
            maps.strings.Invoke(memory, Op::destroy, c.object);
        };
        services.constraints.map_value = [this](U map, U key) {
            const auto k = std::make_pair(map, String(key));
            auto found = constraintValues.find(k);
            if (found != constraintValues.end())
                return found->second;
            maps.PutConstraint(memory, map, key, 0xA5A5A5A5u, 0xA5A5A5A5u, 0xA5A5A5A5u);
            const U value = lastNode + 0x28u;
            constraintValues.emplace(k, value);
            return value;
        };
        services.get_length = [this](Mem &, Monstey_FearConstraintCall c) {
            events.push_back({"text-length", 0x1000627Cu, c.object, c.target});
            return textLengthHigh | std::uint64_t(streams.at(c.object).bytes.size());
        };
        services.read = [this](Mem &m, Monstey_FearConstraintReadCall c) {
            if (textReadOverride)
                return textReadOverride(m, c);
            return ReadStream(c, true);
        };
        services.new_array = [this](Mem &, U site, U bytes) {
            if (site == 0x100063D2u && failOutputAllocation) {
                events.push_back({"null-output", site, 0, bytes});
                return 0u;
            }
            return Allocate(bytes, site, site == 0x100062BAu ? "text" : "output");
        };
        services.new_scalar = [this](Mem &, U site, U bytes) {
            if (nullEmptyRoot) {
                events.push_back({"null-empty", site, 0, bytes});
                return 0u;
            }
            return Allocate(bytes, site, "empty-root");
        };
        services.delete_array = [this](Mem &, Monstey_FearUnpackedDeleteCall c) {
            Release(c.address, c.native_site, "delete-array");
        };
        services.cleanup.delete_scalar = [this](Mem &m, Monstey_FearUnpackedDeleteCall c) {
            Release(c.address, c.native_site, "delete-scalar");
            if (deleteHook)
                deleteHook(m, c);
        };
        services.cleanup.erase_range = [this](Mem &m, Monstey_FearUnpackedEraseCall c) {
            events.push_back({c.parsed ? "erase-parsed" : "erase-constraints", c.native_site, c.map,
                              c.first, c.last});
            const U head = R(m, c.map + 4u), nil = c.parsed ? 0x4Du : 0x35u;
            std::vector<U> todo{R(m, head + 4u)};
            while (!todo.empty()) {
                U node = todo.back();
                todo.pop_back();
                if (R(m, node + nil, 1) != 0)
                    continue;
                todo.push_back(R(m, node));
                todo.push_back(R(m, node + 8u));
                maps.strings.Invoke(m, Op::destroy, node + 0x0Cu);
                if (c.parsed)
                    maps.strings.Invoke(m, Op::destroy, node + 0x2Cu);
            }
            W(m, head, head);
            W(m, head + 4u, head);
            W(m, head + 8u, head);
            W(m, c.map + 8u, 0);
            W(m, c.hidden_result, c.last);
            if (eraseHook)
                eraseHook(m, c);
            return c.hidden_result;
        };
        services.callback.string = [this](Mem &m, Monstey_FearUnpackedCallbackStringCall c) {
            U result = maps.strings.Invoke(m, c.op, c.receiver, c.a, c.b, c.c);
            if (stringHook)
                stringHook(m, c);
            return result;
        };
        services.callback.validate = [this](Mem &, U, U) {
            ++validations;
            return std::uint8_t{0};
        };
        services.callback.insert = [this](Mem &m, U map, U hidden, U hint, U pair) {
            Check(R(m, pair + 0x3Cu) == 0x1234ABCDu, "nested opaque supplied to insert");
            return maps.InsertParsed(m, map, hidden, hint, pair);
        };
        services.materializer.string = [this](Mem m, Op op, U self, U a, U b, U c) {
            return maps.strings.Invoke(m, op, self, a, b, c);
        };
        services.materializer.npos_pointer_cell = 0x1000B048u;
        services.materializer.empty_cstring = 0x1000B1E0u;
        services.materializer.root_name_cell = 0x1000D078u;
        services.materializer.strncpy = Monstey_FearUnpackedForwardStrncpy;
        services.materializer.atoi_bits = [](Mem m, U source) {
            const auto token = Cstr(m, source);
            if (token == "11")
                return 11u;
            if (token == "22")
                return 22u;
            if (token == "33")
                return 33u;
            throw std::runtime_error("Unconfigured fixture numeric result");
        };
    }
    U Run() { return Monstey_FearOpenUnpacked(memory, wrapper, services); }
    std::size_t Count(const std::string &kind, U site = 0) const {
        std::size_t n = 0;
        for (const auto &e : events)
            if (e.kind == kind && (site == 0 || e.site == site))
                ++n;
        return n;
    }
    const Allocation &Kind(const std::string &kind) const {
        for (const auto &a : allocations)
            if (a.kind == kind)
                return a;
        throw std::runtime_error("Missing allocation kind");
    }
};
} // namespace
