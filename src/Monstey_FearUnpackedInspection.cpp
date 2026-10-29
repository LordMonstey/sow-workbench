#include "Monstey_FearUnpackedOwned.hpp"
#include <algorithm>
#include <bit>
#include <immintrin.h>
#include <vector>
namespace Monstey {
namespace {
using Monstey_UnpackedU32 = std::uint32_t;
using Monstey_UnpackedU64 = std::uint64_t;
using Monstey_Image = Monstey_FearUnpackedOwnedImage;
struct Monstey_QueryMxcsr {
    Monstey_UnpackedU32 saved;
    Monstey_QueryMxcsr() : saved(_mm_getcsr()) { _mm_setcsr(0x1F80u); }
    ~Monstey_QueryMxcsr() { _mm_setcsr(saved); }
};
[[noreturn]] void Monstey_Bad(const char *code, const char *what) {
    throw Monstey_FearUnpackedOwnedFault(code, what);
}
Monstey_UnpackedU32 Monstey_Raw(const Monstey_Image &i, Monstey_UnpackedU64 at, Monstey_UnpackedU32 width = 4) {
    if (at < i.arena_base || at + width > 0x100000000ull || at - i.arena_base > i.arena_used ||
        width > Monstey_UnpackedU64(i.arena_used) - (at - i.arena_base) || at - i.arena_base + width > i.arena.size())
        Monstey_Bad("unpacked_inspection_range",
            "Inspection consumed a range outside the allocated arena prefix");
    Monstey_UnpackedU32 value = 0;
    for (Monstey_UnpackedU32 k = 0; k < width; ++k)
        value |= Monstey_UnpackedU32(i.arena[static_cast<std::size_t>(at - i.arena_base) + k]) << (k * 8);
    return value;
}
std::string Monstey_InspectionHex(std::span<const std::uint8_t> b) {
    constexpr char h[] = "0123456789abcdef";
    std::string out;
    out.reserve(b.size() * 2);
    for (auto x : b) {
        out += h[x >> 4];
        out += h[x & 15];
    }
    return out;
}
Monstey_Json Monstey_Identity(const Monstey_Image &i, Monstey_UnpackedU32 address) {
    auto out = Monstey_Json::object(
        {{"native32", Monstey_Hex(address)}, {"address_space", "owned_unpacked_native32"}});
    if (address >= i.arena_base && Monstey_UnpackedU64(address) - i.arena_base < i.arena_used) {
        out["arena_offset"] = address - i.arena_base;
        out["region"] = address >= i.pool_begin && address < i.pool_end
                            ? "unpacked_names_vector_pool"
                        : address >= i.root && Monstey_UnpackedU64(address) < Monstey_UnpackedU64(i.root) + i.root_allocation_bytes
                            ? "materialized_root_allocation"
                            : "owned_frames_maps_or_retained_primitive_storage";
    } else
        out["region"] = "outside_allocated_arena_prefix";
    return out;
}
Monstey_Json Monstey_Text(const Monstey_Image &i, Monstey_UnpackedU32 address, Monstey_UnpackedU32 cap, bool wide = false) {
    auto out = Monstey_Json::object(
        {{"identity", Monstey_Identity(i, address)},
         {"encoding",
          wide ? "raw_utf16le_code_units_no_locale_conversion" : "raw_bytes_no_locale_conversion"},
         {"provenance", "materialized_unpacked_pool_after_tables_and_value_slots"}});
    if (address < i.pool_begin || address >= i.pool_end || !i.pool_begin) {
        out["status"] = "unavailable_outside_unpacked_pool";
        return out;
    }
    const Monstey_UnpackedU32 unit = wide ? 2u : 1u;
    const Monstey_UnpackedU64 available = Monstey_UnpackedU64(i.pool_end) - address;
    const Monstey_UnpackedU64 maximum = std::min<Monstey_UnpackedU64>(available, cap - cap % unit);
    Monstey_UnpackedU64 n = 0;
    bool terminated = false;
    while (n + unit <= maximum) {
        if (Monstey_Raw(i, Monstey_UnpackedU64(address) + n, 1) == 0 && (!wide || Monstey_Raw(i, Monstey_UnpackedU64(address) + n + 1, 1) == 0)) {
            terminated = true;
            break;
        }
        n += unit;
    }
    const auto bytes = std::span(i.arena).subspan(static_cast<std::size_t>(address - i.arena_base),
                                                  static_cast<std::size_t>(n));
    out["bytes_hex"] = Monstey_InspectionHex(bytes);
    out["emitted_bytes"] = n;
    out["terminator_observed"] = terminated;
    out["status"] = terminated        ? "terminated"
                    : cap < available ? "truncated_at_display_limit"
                                      : "unavailable_terminator_in_unpacked_pool";
    if (!wide) {
        std::string display;
        for (auto b : bytes) {
            if (b >= 32 && b <= 126 && b != '\\')
                display += static_cast<char>(b);
            else if (b == '\\')
                display += "\\\\";
            else {
                display += "\\x";
                display += Monstey_InspectionHex(std::span(&b, 1));
            }
        }
        out["display_ascii_escaped"] = std::move(display);
    }
    return out;
}
Monstey_Json Monstey_Float(Monstey_UnpackedU32 bits) {
    const Monstey_UnpackedU32 exp = bits & 0x7F800000u, frac = bits & 0x7FFFFFu;
    auto out = Monstey_Json::object({{"bits_u32", bits},
                                     {"bits_hex", Monstey_Hex(bits)},
                                     {"matches_arena_poison_pattern", bits == 0xCDCDCDCDu}});
    if (exp == 0x7F800000u) {
        out["value"] = nullptr;
        out["classification"] = frac           ? "nan"
                                : (bits >> 31) ? "negative_infinity"
                                               : "positive_infinity";
    } else {
        out["value"] = static_cast<double>(std::bit_cast<float>(bits));
        out["classification"] = (bits & 0x7FFFFFFFu) == 0
                                    ? (bits >> 31) ? "negative_zero" : "positive_zero"
                                : exp == 0 ? "subnormal"
                                           : "finite";
    }
    return out;
}
Monstey_UnpackedU32 Monstey_TableBase(const Monstey_Image &i, unsigned type) {
    Monstey_UnpackedU64 address = Monstey_UnpackedU64(i.root) + 20u;
    for (unsigned n = 0; n < type; ++n)
        address += Monstey_UnpackedU64(i.table_counts[n]) * 16u;
    if (address > UINT32_MAX)
        Monstey_Bad("unpacked_graph_layout", "Declared table base overflow");
    return static_cast<Monstey_UnpackedU32>(address);
}
void Monstey_Node(const Monstey_Image &i, Monstey_UnpackedU32 address, unsigned type) {
    const Monstey_UnpackedU32 base = Monstey_TableBase(i, type);
    const Monstey_UnpackedU64 bytes = Monstey_UnpackedU64(i.table_counts[type]) * 16u;
    if (address < base || Monstey_UnpackedU64(address) - base >= bytes || (address - base) % 16u != 0 ||
        Monstey_UnpackedU64(address) + 16u > Monstey_UnpackedU64(i.root) + i.root_allocation_bytes)
        Monstey_Bad("unpacked_graph_layout",
            "Published node is outside its actually allocated native table");
}
void Monstey_ChildTable(const Monstey_Image &i, Monstey_UnpackedU32 parent, unsigned childType) {
    const auto m = i.Memory();
    const Monstey_UnpackedU32 count = Monstey_FearDbChildCount(m, parent);
    if (!count)
        return;
    const Monstey_UnpackedU32 address = Monstey_Raw(i, Monstey_UnpackedU64(parent) + 12u), base = Monstey_TableBase(i, childType);
    const Monstey_UnpackedU64 bytes = Monstey_UnpackedU64(i.table_counts[childType]) * 16u;
    if (address < base || Monstey_UnpackedU64(address) - base > bytes || (address - base) % 16u != 0 ||
        Monstey_UnpackedU64(count) * 16u > bytes - (Monstey_UnpackedU64(address) - base))
        Monstey_Bad("unpacked_graph_layout", "Published child range exceeds its actually allocated native "
                                     "table; raw reconstruction is not repaired");
}
Monstey_Json Monstey_Named(const Monstey_Image &i, Monstey_UnpackedU32 node, Monstey_UnpackedU32 ordinal, Monstey_UnpackedU32 cap) {
    return Monstey_Json::object({{"ordinal", ordinal},
                                 {"identity", Monstey_Identity(i, node)},
                                 {"name", Monstey_Text(i, Monstey_FearDbNodeName(i.Memory(), node), cap)}});
}
struct Monstey_Ref {
    Monstey_UnpackedU32 identity, category, record;
};
Monstey_Json Monstey_Value(const Monstey_Image &i, Monstey_UnpackedU32 attribute, Monstey_UnpackedU32 index, Monstey_UnpackedU32 cap, const std::vector<Monstey_Ref> &records) {
    auto memory = i.Memory();
    const Monstey_UnpackedU32 values = Monstey_Raw(i, Monstey_UnpackedU64(attribute) + 12u);
    const Monstey_UnpackedU64 at = Monstey_UnpackedU64(values) + Monstey_UnpackedU64(index) * 4u;
    const Monstey_UnpackedU32 valueBase = Monstey_TableBase(i, 2) + i.table_counts[2] * 16u;
    if (values < valueBase || at + 4u > i.pool_begin)
        Monstey_Bad("unpacked_value_range", "Published value cell is outside native reserved value slots");
    const Monstey_UnpackedU32 word = Monstey_Raw(i, at), type = Monstey_FearDbAttributeType(memory, attribute);
    auto out = Monstey_Json::object(
        {{"ordinal", index},
         {"runtime_word_u32", word},
         {"runtime_word_hex", Monstey_Hex(word)},
         {"runtime_type_u8", type},
         {"word_provenance",
          "concrete_unpacked_materializer_output_with_declared_owned_primitives"},
         {"matches_arena_poison_pattern", word == 0xCDCDCDCDu}});
    if (type == 1) {
        out["kind"] = "bool_byte";
        out["byte_u8"] = Monstey_UnpackedU32(Monstey_FearDbBoolByte(memory, attribute, index, 0));
    } else if (type == 2) {
        out["kind"] = "float32_payload";
        out["float32"] = Monstey_Float(Monstey_FearDbFloat32Bits(memory, attribute, index, 0));
    } else if (type == 3) {
        out["kind"] = "int32_bits";
        out["signed_value"] = static_cast<std::int64_t>(
            std::bit_cast<std::int32_t>(Monstey_FearDbInt32Bits(memory, attribute, index, 0)));
    } else if (type == 4 || type == 5) {
        out["kind"] = type == 4 ? "string_identity" : "wstring_identity";
        const Monstey_UnpackedU32 address = type == 4 ? Monstey_FearDbStringIdentity(memory, attribute, index, 0)
                                    : Monstey_FearDbWStringIdentity(memory, attribute, index, 0);
        out["string"] = Monstey_Text(i, address, cap, type == 5);
    } else if (type >= 6 && type <= 8) {
        out["kind"] = "vector_pool_identity";
        out["identity"] = Monstey_Identity(i, word);
        const Monstey_UnpackedU32 components = type - 4u;
        if (word < i.pool_begin || Monstey_UnpackedU64(word) + components * 4u > i.pool_end)
            out["status"] = "unavailable_outside_unpacked_pool";
        else {
            out["status"] = "bounded_unpacked_pool_components";
            auto a = Monstey_Json::array();
            for (Monstey_UnpackedU32 n = 0; n < components; ++n)
                a.as_array().push_back(Monstey_Float(Monstey_Raw(i, Monstey_UnpackedU64(word) + n * 4u)));
            out["components"] = std::move(a);
        }
    } else if (type == 9) {
        out["kind"] = "record_identity";
        const Monstey_UnpackedU32 identity = Monstey_FearDbRecordIdentity(memory, attribute, index, 0);
        out["identity"] = Monstey_Identity(i, identity);
        auto a = Monstey_Json::array();
        for (const auto &r : records)
            if (r.identity == identity)
                a.as_array().push_back(
                    Monstey_Json::object({{"category", r.category}, {"record", r.record}}));
        out["matching_traversed_ordinals"] = std::move(a);
        out["status"] = identity ? "runtime_identity_as_published" : "null_runtime_identity";
    } else {
        out["kind"] = "unknown_runtime_type";
        out["status"] = "raw_word_only";
    }
    return out;
}
} // namespace
Monstey_Json Monstey_FearInspectUnpackedOwned(const Monstey_Image &i,
                                              const Monstey_FearInspectorOptions &o) {
    Monstey_QueryMxcsr mxcsr;
    if (o.limit == 0 || o.limit > 10000 || o.string_bytes < 2 || o.string_bytes > 1048576 ||
        (o.record && !o.category) || (o.attribute && !o.record) ||
        (o.summary && (o.category || o.record || o.attribute)))
        Monstey_Bad("unpacked_query_arguments", "Invalid ordinal hierarchy or display bounds");
    if (i.arena_used > i.arena.size() || i.root < i.arena_base ||
        Monstey_UnpackedU64(i.root) + i.root_allocation_bytes > Monstey_UnpackedU64(i.arena_base) + i.arena_used ||
        i.root_allocation_bytes < 20)
        Monstey_Bad("unpacked_image_extent", "Invalid owned root allocation extent");
    const Monstey_UnpackedU64 tablesEnd =
        Monstey_UnpackedU64(i.root) + 20u + 16u * (Monstey_UnpackedU64(i.table_counts[0]) + i.table_counts[1] + i.table_counts[2]);
    if (tablesEnd > Monstey_UnpackedU64(i.root) + i.root_allocation_bytes ||
        (i.table_counts[0] &&
         (i.pool_begin < tablesEnd || i.pool_end != Monstey_UnpackedU64(i.root) + i.root_allocation_bytes ||
          i.pool_begin > i.pool_end)))
        Monstey_Bad("unpacked_graph_layout", "Declared tables or pool exceed root allocation");
    const auto m = i.Memory();
    const Monstey_UnpackedU32 categories = Monstey_FearDbChildCount(m, i.root);
    if (categories != i.table_counts[0])
        Monstey_Bad("unpacked_graph_layout",
            "Published root count differs from its allocated category table");
    Monstey_ChildTable(i, i.root, 0);
    std::vector<Monstey_Ref> records;
    Monstey_UnpackedU64 attributes = 0, values = 0;
    for (Monstey_UnpackedU32 c = 0; c < categories; ++c) {
        const Monstey_UnpackedU32 cat = Monstey_FearDbChildByIndex(m, i.root, c);
        Monstey_Node(i, cat, 0);
        Monstey_ChildTable(i, cat, 1);
        const Monstey_UnpackedU32 count = Monstey_FearDbChildCount(m, cat);
        for (Monstey_UnpackedU32 r = 0; r < count; ++r) {
            const Monstey_UnpackedU32 record = Monstey_FearDbChildByIndex(m, cat, r);
            Monstey_Node(i, record, 1);
            records.push_back({record, c, r});
            Monstey_ChildTable(i, record, 2);
            const Monstey_UnpackedU32 n = Monstey_FearDbChildCount(m, record);
            attributes += n;
            for (Monstey_UnpackedU32 a = 0; a < n; ++a) {
                const Monstey_UnpackedU32 attr = Monstey_FearDbAttributeByIndex(m, record, a);
                Monstey_Node(i, attr, 2);
                values += Monstey_FearDbValueCount(m, attr);
            }
        }
    }
    auto out = Monstey_Json::object(
        {{"format", "legacy_FEAR_unpacked_text_plus_DBConstraints"},
         {"scope", "read_only_owned_reconstruction_not_writer_or_graph_export"},
         {"profile", i.profile},
         {"root", Monstey_Identity(i, i.root)},
         {"root_allocation_bytes", i.root_allocation_bytes},
         {"unpacked_pool_begin", Monstey_Identity(i, i.pool_begin)},
         {"unpacked_pool_end_exclusive", Monstey_Hex(i.pool_end)},
         {"allocated_table_counts",
          Monstey_Json::array({i.table_counts[0], i.table_counts[1], i.table_counts[2]})},
         {"totals", Monstey_Json::object({{"categories", categories},
                                          {"records", records.size()},
                                          {"attributes", attributes},
                                          {"runtime_visible_value_words", values}})},
         {"count_contract",
          "native_U32_nodes_U16_value_count_U8_type_usage;native_cursor_parent_quirks_retained"}});
    if (o.summary) {
        out["listing"] = "summary";
        return out;
    }
    Monstey_UnpackedU32 parent = i.root, count = categories;
    const char *listing = "categories";
    if (o.category) {
        if (*o.category >= count)
            Monstey_Bad("unpacked_category_ordinal", "Category ordinal out of range");
        parent = Monstey_FearDbChildByIndex(m, parent, *o.category);
        out["selected_category"] = Monstey_Named(i, parent, *o.category, o.string_bytes);
        count = Monstey_FearDbChildCount(m, parent);
        listing = "records";
    }
    if (o.record) {
        if (*o.record >= count)
            Monstey_Bad("unpacked_record_ordinal", "Record ordinal out of range");
        parent = Monstey_FearDbChildByIndex(m, parent, *o.record);
        out["selected_record"] = Monstey_Named(i, parent, *o.record, o.string_bytes);
        count = Monstey_FearDbChildCount(m, parent);
        listing = "attributes";
    }
    if (o.attribute) {
        if (*o.attribute >= count)
            Monstey_Bad("unpacked_attribute_ordinal", "Attribute ordinal out of range");
        parent = Monstey_FearDbAttributeByIndex(m, parent, *o.attribute);
        auto a = Monstey_Named(i, parent, *o.attribute, o.string_bytes);
        a["runtime_type_u8"] = Monstey_FearDbAttributeType(m, parent);
        a["runtime_usage_u8"] = Monstey_FearDbAttributeUsage(m, parent);
        count = Monstey_FearDbValueCount(m, parent);
        a["runtime_value_count_u16"] = count;
        out["selected_attribute"] = std::move(a);
        listing = "values";
    }
    const Monstey_UnpackedU32 begin = std::min(o.offset, count),
            end = static_cast<Monstey_UnpackedU32>(std::min<Monstey_UnpackedU64>(count, Monstey_UnpackedU64(begin) + o.limit));
    auto items = Monstey_Json::array();
    for (Monstey_UnpackedU32 n = begin; n < end; ++n) {
        if (o.attribute)
            items.as_array().push_back(Monstey_Value(i, parent, n, o.string_bytes, records));
        else {
            const Monstey_UnpackedU32 child = o.record ? Monstey_FearDbAttributeByIndex(m, parent, n)
                                     : Monstey_FearDbChildByIndex(m, parent, n);
            auto a = Monstey_Named(i, child, n, o.string_bytes);
            if (o.record) {
                a["runtime_type_u8"] = Monstey_FearDbAttributeType(m, child);
                a["runtime_usage_u8"] = Monstey_FearDbAttributeUsage(m, child);
                a["runtime_value_count_u16"] = Monstey_FearDbValueCount(m, child);
            } else
                a["child_count_u32"] = Monstey_FearDbChildCount(m, child);
            items.as_array().push_back(std::move(a));
        }
    }
    out["listing"] = listing;
    out["items"] = std::move(items);
    out["page"] = Monstey_Json::object(
        {{"offset_requested", o.offset},
         {"offset_emitted", begin},
         {"limit", o.limit},
         {"total", count},
         {"emitted", end - begin},
         {"has_more", end < count},
         {"next_offset", end < count ? Monstey_Json(end) : Monstey_Json(nullptr)}});
    return out;
}
} // namespace Monstey
