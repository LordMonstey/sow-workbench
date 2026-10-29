#include "Monstey_FearUnpackedOwned.hpp"
#include "Monstey_FearUnpackedOpen.hpp"
#include "Monstey_FearUnpackedValidate.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <immintrin.h>
#include <limits>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
namespace Monstey {
Monstey_FearUnpackedOwnedFault::Monstey_FearUnpackedOwnedFault(std::string c, std::string message,
                                                               Monstey_Json d)
    : std::runtime_error(std::move(message)), code(std::move(c)), details(std::move(d)) {}
namespace {
using Monstey_UnpackedU32 = std::uint32_t;
using Monstey_UnpackedU64 = std::uint64_t;
using Monstey_Memory = Monstey_FearDbMutableMemory;
using Monstey_StringOp = Monstey_FearUnpackedStringOp;
constexpr Monstey_UnpackedU32 Monstey_Base = 0x10000000u, Monstey_Frame = Monstey_Base + 0x20000u, Monstey_Wrapper = Monstey_Frame + 0x58u,
            Monstey_TextStream = Monstey_Base + 0x1000u, Monstey_ConstraintStream = Monstey_Base + 0x1100u;
constexpr Monstey_UnpackedU32 Monstey_LineB = ((Monstey_Frame - 0x18u) & 0xFFFFFFF8u) - 0x410u, Monstey_CallbackFrame = Monstey_LineB - 0x9Cu,
            Monstey_Reserve = 0x30000u;
std::size_t Monstey_Range(Monstey_Memory m, Monstey_UnpackedU32 at, Monstey_UnpackedU32 n, bool writing) {
    if (at < m.base || Monstey_UnpackedU64(at) + n > 0x100000000ull || Monstey_UnpackedU64(at) - m.base > m.bytes.size() ||
        n > m.bytes.size() - std::size_t(Monstey_UnpackedU64(at) - m.base))
        throw Monstey_FearDbFault(at, n, writing);
    return at - m.base;
}
Monstey_UnpackedU32 Monstey_Read(Monstey_Memory m, Monstey_UnpackedU32 at, Monstey_UnpackedU32 n = 4) {
    auto p = Monstey_Range(m, at, n, false);
    Monstey_UnpackedU32 v = 0;
    for (Monstey_UnpackedU32 i = 0; i < n; ++i)
        v |= Monstey_UnpackedU32(m.bytes[p + i]) << (8 * i);
    return v;
}
void Monstey_Write(Monstey_Memory m, Monstey_UnpackedU32 at, Monstey_UnpackedU32 v, Monstey_UnpackedU32 n = 4) {
    auto p = Monstey_Range(m, at, n, true);
    for (Monstey_UnpackedU32 i = 0; i < n; ++i)
        m.bytes[p + i] = std::uint8_t(v >> (8 * i));
}
[[noreturn]] void Monstey_Fail(const char *code, const char *message,
                       Monstey_Json d = Monstey_Json::object()) {
    throw Monstey_FearUnpackedOwnedFault(code, message, std::move(d));
}
Monstey_UnpackedU32 Monstey_Budget(Monstey_UnpackedU32 bytes) {
    if (bytes < 256u * 1024u || bytes > 1024u * 1024u * 1024u)
        Monstey_Fail("unpacked_arena_budget_invalid", "Arena budget must be256KiB..1GiB");
    return bytes;
}
struct Monstey_CLocale {
    _locale_t value = nullptr;
    Monstey_CLocale() {
        value = _create_locale(LC_ALL, "C");
        if (!value)
            Monstey_Fail("unpacked_numeric_locale_failed", "Owned C-locale initialization failed");
    }
    ~Monstey_CLocale() { _free_locale(value); }
    Monstey_CLocale(const Monstey_CLocale &) = delete;
};
struct Monstey_Mxcsr {
    Monstey_UnpackedU32 saved;
    Monstey_Mxcsr() : saved(_mm_getcsr()) { _mm_setcsr(0x1F80u); }
    ~Monstey_Mxcsr() { _mm_setcsr(saved); }
};
struct Monstey_Allocation {
    Monstey_UnpackedU32 address, bytes, site;
    bool released = false;
};
struct Monstey_Stream {
    std::span<const std::uint8_t> bytes;
    std::size_t position = 0;
    Monstey_UnpackedU64 reads = 0;
};
class Monstey_Binding {
  public:
    Monstey_FearUnpackedOwnedOptions options;
    std::vector<std::uint8_t> arena;
    Monstey_Memory memory;
    Monstey_CLocale locale;
    Monstey_FearUnpackedOpenServices services;
    Monstey_FearUnpackedValidateServices validator;
    Monstey_FearUnpackedCallbackLogicalMaps maps;
    Monstey_Stream text, constraints;
    std::vector<Monstey_Allocation> allocations;
    std::unordered_map<Monstey_UnpackedU32, std::size_t> allocation_index;
    std::map<std::pair<Monstey_UnpackedU32, std::string>, Monstey_UnpackedU32> constraint_index;
    Monstey_UnpackedU32 used = Monstey_Reserve, last_node = 0;
    Monstey_UnpackedU64 native_deletes = 0, validation_calls = 0, validation_false = 0, numeric_calls = 0;
    bool primary_live = false;
    Monstey_UnpackedU64 primary_bits = 0;
    Monstey_Binding(std::span<const std::uint8_t> t, std::span<const std::uint8_t> c,
                    const Monstey_FearUnpackedOwnedOptions &o)
        : options(o), arena(Monstey_Budget(o.arena_budget_bytes), 0xCD), memory{Monstey_Base, arena}, text{t},
          constraints{c} {
        if (t.size() > UINT32_MAX || c.size() > UINT32_MAX)
            Monstey_Fail("unpacked_input_extent", "Each native span must fit uint32");
        Monstey_Write(memory, Monstey_Wrapper + 4u, Monstey_TextStream);
        Monstey_Write(memory, Monstey_Wrapper + 8u, Monstey_ConstraintStream);
        for (Monstey_UnpackedU32 stream : std::array<Monstey_UnpackedU32, 2>{Monstey_TextStream, Monstey_ConstraintStream}) {
            Monstey_UnpackedU32 table = stream + 0x200u;
            Monstey_Write(memory, stream, table);
            Monstey_Write(memory, table + 4u, 0x4D530004u);
            Monstey_Write(memory, table + 0x18u, 0x4D530018u);
            Monstey_Write(memory, table + 0x1Cu, 0x4D53001Cu);
        }
        for (Monstey_UnpackedU32 slot : std::array<Monstey_UnpackedU32, 12>{0x1000B040u, 0x1000B044u, 0x1000B050u, 0x1000B054u,
                                        0x1000B064u, 0x1000B068u, 0x1000B06Cu, 0x1000B070u,
                                        0x1000B074u, 0x1000B078u, 0x1000B0BCu, 0x1000B0C0u})
            Monstey_Write(memory, slot, 0x4D530000u + (slot - Monstey_Base));
        Monstey_Write(memory, 0x1000B048u, 0x1000D080u);
        Monstey_Write(memory, 0x1000D080u, UINT32_MAX);
        Monstey_Write(memory, 0x1000B1E0u, 0, 1);
        Monstey_Write(memory, 0x1000D078u, 0x1000D100u);
        Monstey_Write(memory, 0x1000D100u, 0, 1);
        Monstey_Write(memory, Monstey_LineB - 0x10u, options.callback_opaque_word);
        Monstey_Write(memory, Monstey_LineB - 0xE8u, options.insert_opaque_word);
        Monstey_Bind();
    }
    Monstey_UnpackedU32 Monstey_Allocate(Monstey_UnpackedU32 bytes, Monstey_UnpackedU32 site) {
        const Monstey_UnpackedU64 size = std::max<Monstey_UnpackedU64>(bytes, 1u), aligned = (size + 15u) & ~Monstey_UnpackedU64(15u);
        if (aligned > arena.size() - used)
            Monstey_Fail("unpacked_arena_exhausted", "Owned native32 arena budget exhausted",
                 Monstey_Json::object({{"budget_bytes", options.arena_budget_bytes},
                                       {"used_bytes", used},
                                       {"requested_bytes", bytes},
                                       {"native_site", Monstey_Hex(site)}}));
        Monstey_UnpackedU32 address = Monstey_Base + used;
        used += static_cast<Monstey_UnpackedU32>(aligned);
        allocation_index.emplace(address, allocations.size());
        allocations.push_back({address, bytes, site});
        return address;
    }
    void Monstey_Release(Monstey_UnpackedU32 address) {
        if (address == 0)
            return;
        auto it = allocation_index.find(address);
        if (it == allocation_index.end())
            Monstey_Fail("unpacked_unknown_allocation", "Delete consumed an unknown owned allocation");
        auto &a = allocations[it->second];
        if (a.released)
            Monstey_Fail("unpacked_double_release", "Delete consumed an already released owned allocation");
        a.released = true;
        ++native_deletes;
    }
    Monstey_Stream &Monstey_GetStream(Monstey_UnpackedU32 object) {
        if (object == Monstey_TextStream)
            return text;
        if (object == Monstey_ConstraintStream)
            return constraints;
        Monstey_Fail("unpacked_stream_identity", "Unknown owned stream identity");
    }
    Monstey_UnpackedU32 Monstey_ReadStream(Monstey_FearConstraintReadCall c) {
        auto &s = Monstey_GetStream(c.stream);
        ++s.reads;
        if (c.bytes > s.bytes.size() - s.position)
            return 1;
        const auto out = Monstey_Range(memory, c.destination, c.bytes, true);
        std::copy_n(s.bytes.begin() + static_cast<std::ptrdiff_t>(s.position), c.bytes,
                    arena.begin() + static_cast<std::ptrdiff_t>(out));
        s.position += c.bytes;
        return 0;
    }
    std::string Monstey_CString(Monstey_Memory m, Monstey_UnpackedU32 source) {
        if (source == 0)
            Monstey_Fail("unpacked_numeric_null_source", "Consumed null C-string identity");
        std::string out;
        for (Monstey_UnpackedU32 n = 0;; ++n) {
            const auto b = Monstey_Read(m, source + n, 1);
            if (b == 0)
                return out;
            out.push_back(static_cast<char>(b));
        }
    }
    std::string Monstey_String(Monstey_Memory m, Monstey_UnpackedU32 object) {
        Monstey_UnpackedU32 at = Monstey_FearUnpackedValidateStringData(m, object), count = Monstey_Read(m, object + 0x14u);
        auto offset = Monstey_Range(m, at, count, false);
        return std::string(reinterpret_cast<const char *>(m.bytes.data() + offset), count);
    }
    Monstey_UnpackedU64 Monstey_ParseDouble(Monstey_Memory m, Monstey_UnpackedU32 source) {
        ++numeric_calls;
        auto token = Monstey_CString(m, source);
        const double value = _atof_l(token.c_str(), locale.value);
        return std::bit_cast<Monstey_UnpackedU64>(value);
    }
    void Monstey_Erase(Monstey_Memory m, const Monstey_FearUnpackedEraseCall &c) {
        const Monstey_UnpackedU32 head = Monstey_Read(m, c.map + 4u), nil = c.parsed ? 0x4Du : 0x35u;
        if (c.last != head || c.first != Monstey_Read(m, head))
            Monstey_Fail("unpacked_erase_profile",
                 "Owned erase requires the whole map range produced by Open");
        std::vector<Monstey_UnpackedU32> pending{Monstey_Read(m, head + 4u)};
        std::size_t visits = 0;
        while (!pending.empty()) {
            Monstey_UnpackedU32 node = pending.back();
            pending.pop_back();
            if (Monstey_Read(m, node + nil, 1) != 0)
                continue;
            if (++visits > allocations.size())
                Monstey_Fail("unpacked_map_cycle", "Owned logical map traversal exceeded allocation count");
            pending.push_back(Monstey_Read(m, node));
            pending.push_back(Monstey_Read(m, node + 8u));
            maps.strings.Invoke(m, Monstey_StringOp::destroy, node + 0x0Cu);
            if (c.parsed)
                maps.strings.Invoke(m, Monstey_StringOp::destroy, node + 0x2Cu);
            Monstey_Release(node);
        }
        Monstey_Write(m, head, head);
        Monstey_Write(m, head + 4u, head);
        Monstey_Write(m, head + 8u, head);
        Monstey_Write(m, c.map + 8u, 0);
        Monstey_Write(m, c.hidden_result, c.last);
    }
    void Monstey_Bind() {
        maps.strings.allocate = [this](Monstey_Memory, Monstey_UnpackedU32 n) { return Monstey_Allocate(n, 0); };
        maps.allocate = [this](Monstey_Memory, Monstey_UnpackedU32 n) {
            last_node = Monstey_Allocate(n, 0);
            return last_node;
        };
        services.new_sentinel = [this](Monstey_Memory &m, bool parsed, Monstey_UnpackedU32, Monstey_UnpackedU32 site) {
            Monstey_UnpackedU32 p = Monstey_Allocate(parsed ? 0x50u : 0x38u, site);
            Monstey_Write(m, p, 0);
            Monstey_Write(m, p + 4u, 0);
            Monstey_Write(m, p + 8u, 0);
            Monstey_Write(m, p + (parsed ? 0x4Cu : 0x34u), 1, 1);
            Monstey_Write(m, p + (parsed ? 0x4Du : 0x35u), 0, 1);
            return p;
        };
        services.constraints.get_length = [this](Monstey_FearConstraintCall c) {
            return Monstey_UnpackedU64(Monstey_GetStream(c.object).bytes.size());
        };
        services.constraints.get_position = [this](Monstey_FearConstraintCall c) {
            return Monstey_UnpackedU64(Monstey_GetStream(c.object).position);
        };
        services.constraints.read = [this](Monstey_FearConstraintReadCall c) {
            return Monstey_ReadStream(c);
        };
        services.constraints.new_array = [this](Monstey_UnpackedU32 n) { return Monstey_Allocate(n, 0x10006020u); };
        services.constraints.delete_array = [this](Monstey_UnpackedU32 p) { Monstey_Release(p); };
        services.constraints.string_default = [this](Monstey_FearConstraintCall c) {
            maps.strings.Invoke(memory, Monstey_StringOp::construct, c.object);
        };
        services.constraints.string_assign_cstr = [this](Monstey_FearConstraintCall c, Monstey_UnpackedU32 p) {
            maps.strings.Invoke(memory, Monstey_StringOp::construct_cstring, c.object, p);
        };
        services.constraints.string_destroy = [this](Monstey_FearConstraintCall c) {
            maps.strings.Invoke(memory, Monstey_StringOp::destroy, c.object);
        };
        services.constraints.map_value = [this](Monstey_UnpackedU32 map, Monstey_UnpackedU32 key) {
            auto index = std::make_pair(map, Monstey_String(memory, key));
            auto found = constraint_index.find(index);
            if (found != constraint_index.end())
                return found->second;
            maps.PutConstraint(memory, map, key, 0xCDCDCDCDu, 0xCDCDCDCDu, 0xCDCDCDCDu);
            const Monstey_UnpackedU32 result = last_node + 0x28u;
            constraint_index.emplace(std::move(index), result);
            return result;
        };
        services.get_length = [this](Monstey_Memory &, Monstey_FearConstraintCall c) {
            return Monstey_UnpackedU64(Monstey_GetStream(c.object).bytes.size());
        };
        services.read = [this](Monstey_Memory &, Monstey_FearConstraintReadCall c) { return Monstey_ReadStream(c); };
        services.new_array = [this](Monstey_Memory &, Monstey_UnpackedU32 site, Monstey_UnpackedU32 n) { return Monstey_Allocate(n, site); };
        services.new_scalar = [this](Monstey_Memory &, Monstey_UnpackedU32 site, Monstey_UnpackedU32 n) { return Monstey_Allocate(n, site); };
        services.delete_array = [this](Monstey_Memory &, Monstey_FearUnpackedDeleteCall c) {
            Monstey_Release(c.address);
        };
        services.cleanup.delete_scalar = [this](Monstey_Memory &, Monstey_FearUnpackedDeleteCall c) {
            Monstey_Release(c.address);
        };
        services.cleanup.erase_range = [this](Monstey_Memory &m, Monstey_FearUnpackedEraseCall c) {
            Monstey_Erase(m, c);
            return c.hidden_result;
        };
        services.callback.string = [this](Monstey_Memory &m, Monstey_FearUnpackedCallbackStringCall c) {
            return maps.strings.Invoke(m, c.op, c.receiver, c.a, c.b, c.c);
        };
        services.callback.insert = [this](Monstey_Memory &m, Monstey_UnpackedU32 map, Monstey_UnpackedU32 hidden, Monstey_UnpackedU32 hint, Monstey_UnpackedU32 pair) {
            return maps.InsertParsed(m, map, hidden, hint, pair);
        };
        validator.c_str = [](Monstey_Memory &m, Monstey_FearValidateCstrCall c) {
            return Monstey_FearUnpackedValidateStringData(m, c.string_object);
        };
        validator.strtol = [this](Monstey_Memory &m, Monstey_FearValidateStrtolCall c) {
            if (c.end_pointer != 0 || c.base != 0)
                Monstey_Fail("unpacked_numeric_profile", "Validator strtol requires native end0/base0");
            ++numeric_calls;
            auto token = Monstey_CString(m, c.source);
            static_assert(sizeof(long) == 4);
            return static_cast<Monstey_UnpackedU32>(_strtol_l(token.c_str(), nullptr, 0, locale.value));
        };
        validator.atof_binary64 = [this](Monstey_Memory &m, Monstey_FearValidateAtofCall c) {
            if (primary_live)
                Monstey_Fail("unpacked_x87_profile_state", "Logical primary already live");
            primary_bits = Monstey_ParseDouble(m, c.source);
            primary_live = true;
            return primary_bits;
        };
        validator.compare_status = [this](Monstey_Memory &, Monstey_FearValidateCompareCall c) {
            if (!primary_live || primary_bits != c.value_binary64_bits)
                Monstey_Fail("unpacked_x87_profile_state", "Logical primary value mismatch");
            const auto status = Monstey_FearUnpackedValidateMaskedCompare(c.value_binary64_bits,
                                                                          c.bound_binary32_bits);
            if (c.phase == Monstey_FearValidateX87Phase::upper_final_compare)
                primary_live = false;
            return status;
        };
        validator.retire_primary = [this](Monstey_Memory &, Monstey_UnpackedU32, Monstey_UnpackedU64 value) {
            if (!primary_live || value != primary_bits)
                Monstey_Fail("unpacked_x87_profile_state", "Logical primary retirement mismatch");
            primary_live = false;
        };
        services.callback.validate = [this](Monstey_Memory &m, Monstey_UnpackedU32 constraint, Monstey_UnpackedU32 value) {
            ++validation_calls;
            auto result =
                Monstey_FearUnpackedValidate(m, constraint, value, Monstey_CallbackFrame - 4u, validator);
            if (primary_live)
                Monstey_Fail("unpacked_x87_profile_state", "Validator left a logical primary live");
            if (!result)
                ++validation_false;
            return result;
        };
        services.materializer.string = [this](Monstey_Memory m, Monstey_StringOp op, Monstey_UnpackedU32 receiver, Monstey_UnpackedU32 a, Monstey_UnpackedU32 b, Monstey_UnpackedU32 c) {
            return maps.strings.Invoke(m, op, receiver, a, b, c);
        };
        services.materializer.npos_pointer_cell = 0x1000B048u;
        services.materializer.empty_cstring = 0x1000B1E0u;
        services.materializer.root_name_cell = 0x1000D078u;
        services.materializer.strncpy = Monstey_FearUnpackedForwardStrncpy;
        services.materializer.atoi_bits = [this](Monstey_Memory m, Monstey_UnpackedU32 source) {
            ++numeric_calls;
            auto token = Monstey_CString(m, source);
            return static_cast<Monstey_UnpackedU32>(_atoi_l(token.c_str(), locale.value));
        };
        services.materializer.atof_fstp32_bits = [this](Monstey_Memory m, Monstey_UnpackedU32 source) {
            const auto bits = Monstey_ParseDouble(m, source);
            const float value = static_cast<float>(std::bit_cast<double>(bits));
            return std::bit_cast<Monstey_UnpackedU32>(value);
        };
// Fixed numeric-only formats below consume correctly typed float pointers;
// no character buffer conversion is permitted by this owned adapter.
#pragma warning(push)
#pragma warning(disable : 4996)
        services.materializer.sscanf = [this](Monstey_Memory m, Monstey_UnpackedU32 source, Monstey_UnpackedU32 dimensions, std::array<Monstey_UnpackedU32, 4> out) {
            ++numeric_calls;
            if (dimensions < 2 || dimensions > 4)
                Monstey_Fail("unpacked_numeric_dimensions", "Vector dimensions must be2..4");
            auto token = Monstey_CString(m, source);
            std::array<float, 4> values{};
            int converted = 0;
            if (dimensions == 2)
                converted = _sscanf_l(token.c_str(), "%f,%f", locale.value, &values[0], &values[1]);
            else if (dimensions == 3)
                converted = _sscanf_l(token.c_str(), "%f,%f,%f", locale.value, &values[0],
                                      &values[1], &values[2]);
            else
                converted = _sscanf_l(token.c_str(), "%f,%f,%f,%f", locale.value, &values[0],
                                      &values[1], &values[2], &values[3]);
            for (int n = 0; n < converted; ++n)
                Monstey_Write(m, out[static_cast<std::size_t>(n)],
                      std::bit_cast<Monstey_UnpackedU32>(values[static_cast<std::size_t>(n)]));
            return static_cast<Monstey_UnpackedU32>(converted);
        };
#pragma warning(pop)
    }
    Monstey_Json Monstey_Stats() const {
        return Monstey_Json::object(
            {{"arena_budget_bytes", options.arena_budget_bytes},
             {"arena_used_bytes", used},
             {"arena_reserved_frame_bytes", Monstey_Reserve},
             {"allocation_count", allocations.size()},
             {"explicit_delete_calls", native_deletes},
             {"text_read_calls", text.reads},
             {"constraint_read_calls", constraints.reads},
             {"text_consumed_bytes", text.position},
             {"constraint_consumed_bytes", constraints.position},
             {"validation_calls", validation_calls},
             {"validation_false_results_ignored_by_native_callback", validation_false},
             {"numeric_calls", numeric_calls}});
    }
};
} // namespace
Monstey_Json Monstey_FearUnpackedTextProfile(std::span<const std::uint8_t> text) {
    const char *signature = "none";
    if (text.size() >= 4 && text[0] == 0xFF && text[1] == 0xFE && text[2] == 0 && text[3] == 0)
        signature = "utf32le_bom";
    else if (text.size() >= 4 && text[0] == 0 && text[1] == 0 && text[2] == 0xFE && text[3] == 0xFF)
        signature = "utf32be_bom";
    else if (text.size() >= 2 && text[0] == 0xFF && text[1] == 0xFE)
        signature = "utf16le_bom";
    else if (text.size() >= 2 && text[0] == 0xFE && text[1] == 0xFF)
        signature = "utf16be_bom";
    else if (text.size() >= 3 && text[0] == 0xEF && text[1] == 0xBB && text[2] == 0xBF)
        signature = "utf8_bom";
    return Monstey_Json::object(
        {{"source_profile", "explicit_8bit_override_lines_not_GDBEdit_s_metadata"},
         {"bom_signature", signature},
         {"contains_NUL", std::find(text.begin(), text.end(), std::uint8_t{0}) != text.end()},
         {"decoder_applied", false},
         {"bytes_passed_literally", true},
         {"notice", "Signatures/NULs are not decoded; an empty graph does not certify "
                    "editor-source format compatibility."}});
}
Monstey_FearUnpackedOwnedImage
Monstey_FearLoadUnpackedOwned(std::span<const std::uint8_t> text,
                              std::span<const std::uint8_t> constraints,
                              const Monstey_FearUnpackedOwnedOptions &options) {
    Monstey_Mxcsr mxcsr;
    std::unique_ptr<Monstey_Binding> binding;
    try {
        binding = std::make_unique<Monstey_Binding>(text, constraints, options);
        auto &b = *binding;
        const Monstey_UnpackedU32 root = Monstey_FearOpenUnpacked(b.memory, Monstey_Wrapper, b.services);
        if (!root)
            Monstey_Fail("unpacked_open_returned_null", "Reconstructed OpenUnpacked returned null",
                 b.Monstey_Stats());
        const auto it = b.allocation_index.find(root);
        if (it == b.allocation_index.end())
            Monstey_Fail("unpacked_root_identity", "Returned root is outside owned allocation identities");
        const Monstey_UnpackedU32 bytes = b.allocations[it->second].bytes;
        Monstey_UnpackedU32 poolBegin = 0, poolEnd = 0;
        if (bytes > 20u) {
            poolEnd = Monstey_Read(b.memory, Monstey_Frame + 0x48u);
            const Monstey_UnpackedU32 poolBytes = Monstey_Read(b.memory, Monstey_Frame + 0x44u);
            poolBegin = poolEnd - poolBytes;
            if (poolBegin < root || Monstey_UnpackedU64(poolEnd) > Monstey_UnpackedU64(root) + bytes)
                Monstey_Fail("unpacked_pool_extent", "Materializer pool extent is outside root allocation");
        }
        Monstey_FearUnpackedOwnedImage out;
        out.arena_base = Monstey_Base;
        out.arena_used = b.used;
        out.root = root;
        out.root_allocation_bytes = bytes;
        out.pool_begin = poolBegin;
        out.pool_end = poolEnd;
        if (bytes > 20u)
            out.table_counts = {Monstey_Read(b.memory, Monstey_Frame + 0x34u), Monstey_Read(b.memory, Monstey_Frame + 0x38u),
                                Monstey_Read(b.memory, Monstey_Frame + 0x3Cu)};
        out.profile = Monstey_Json::object(
            {{"profile", "owned_Monstey_FEAR_unpacked_MSVC2026_C_locale_v1"},
             {"scope", "read_only_reconstructed_pipeline_with_explicit_owned_primitive_profiles"},
             {"numeric_parser",
              "owned _atoi_l/_atof_l/_strtol_l/_sscanf_l with dedicated C locale"},
             {"float_conversion",
              "owned_x64_default_MXCSR_round_nearest_FZ_DAZ_off;caller_MXCSR_restored"},
             {"validator_status",
              "integer_bit_normal_masked_F32_F64_profile;logical_primary_retirement"},
             {"physical_original_CRT_x87_ABI_history_qualified", false},
             {"logical_strings_maps", "frozen_owned_raw28B_strings_and_ordered_binary_tree;not_"
                                      "original_STL_capacity_or_balancing"},
             {"allocator", "bounded_monotonic_native32_arena;explicit_deletes_mark_release;all_"
                           "host_storage_RAII"},
             {"budget_scope", "arena_only;input_spans_and_host_control_containers_are_separate"},
             {"unwritten_arena_byte", 0xCDu},
             {"callback_opaque_word", options.callback_opaque_word},
             {"insert_opaque_word", options.insert_opaque_word},
             {"opaque_origin",
              "explicit_owned_profile_values_not_historical_samples_or_zero_fallback"},
             {"stream_profile",
              "bounded_span;zero_success;short_read_leaves_destination_and_position_unchanged"},
             {"input_text_byte_profile", Monstey_FearUnpackedTextProfile(text)},
             {"input_text_sha256", Monstey_HashSha256(text)},
             {"input_constraints_sha256", Monstey_HashSha256(constraints)},
             {"sizing_context_after_materialization",
              Monstey_Json::array({Monstey_Read(b.memory, Monstey_Frame + 0x34u), Monstey_Read(b.memory, Monstey_Frame + 0x38u),
                                   Monstey_Read(b.memory, Monstey_Frame + 0x3Cu), Monstey_Read(b.memory, Monstey_Frame + 0x40u),
                                   Monstey_Read(b.memory, Monstey_Frame + 0x44u), Monstey_Read(b.memory, Monstey_Frame + 0x48u)})},
             {"statistics", b.Monstey_Stats()},
             {"original_image_loaded_or_executed", false},
             {"writer_or_cross_source_conversion", false}});
        out.arena = std::move(b.arena);
        return out;
    } catch (const Monstey_FearUnpackedOwnedFault &) {
        throw;
    } catch (const Monstey_FearDbFault &e) {
        auto d = binding ? binding->Monstey_Stats() : Monstey_Json::object();
        d["address"] = Monstey_Hex(e.address);
        d["width"] = e.width;
        d["writing"] = e.writing;
        throw Monstey_FearUnpackedOwnedFault("unpacked_consumed_range", e.what(), std::move(d));
    } catch (const std::bad_alloc &) {
        throw Monstey_FearUnpackedOwnedFault(
            "unpacked_host_allocation_failed",
            "Host allocation failed within the declared owned profile");
    } catch (const std::exception &e) {
        auto d = binding ? binding->Monstey_Stats() : Monstey_Json::object();
        throw Monstey_FearUnpackedOwnedFault("unpacked_primitive_failure", e.what(), std::move(d));
    }
}
} // namespace Monstey
