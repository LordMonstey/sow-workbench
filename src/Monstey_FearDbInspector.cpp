#include "Monstey_FearDbInspector.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
namespace Monstey {
namespace {
using Monstey_InspectorU32 = std::uint32_t;
using Monstey_InspectorU64 = std::uint64_t;
Monstey_InspectorU32 Monstey_ReadOwned(const Monstey_FearDbPackedImage& image, Monstey_InspectorU64 at, unsigned width = 4) {
    if (at < image.base || at + width > 0x100000000ull || at - image.base > image.used_bytes ||
        width > image.used_bytes - (at - image.base) || at - image.base + width > image.allocation.size())
        throw std::runtime_error("Inspector consumed range is outside written owned image");
    Monstey_InspectorU32 value = 0;
    for (unsigned i = 0; i < width; ++i)
        value |= Monstey_InspectorU32(image.allocation[static_cast<std::size_t>(at - image.base) + i]) << (i * 8);
    return value;
}
Monstey_Json Monstey_Identity(const Monstey_FearDbPackedImage& image, Monstey_InspectorU32 address) {
    auto out = Monstey_Json::object({{"native32", Monstey_Hex(address)}, {"address_space", "owned_native32_diagnostic"}});
    if (address >= image.base && Monstey_InspectorU64(address) - image.base < image.allocation.size()) {
        out["allocation_offset"] = address - image.base;
        out["pointer_region"] = address < image.database ? "serialized_pool" :
            Monstey_InspectorU64(address) - image.base < image.used_bytes ? "runtime_graph_or_diagnostic_caller_name" : "owned_unwritten_tail";
    } else { out["allocation_offset"] = nullptr; out["pointer_region"] = "outside_owned_allocation"; }
    return out;
}
std::string Monstey_ByteHex(std::span<const std::uint8_t> bytes) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (auto byte : bytes) { out += hex[byte >> 4]; out += hex[byte & 15]; }
    return out;
}
Monstey_Json Monstey_PoolString(const Monstey_FearDbPackedImage& image, Monstey_InspectorU32 address,
                               Monstey_InspectorU32 cap, bool wide = false) {
    auto out = Monstey_Json::object({{"identity", Monstey_Identity(image, address)},
        {"encoding", wide ? "raw_utf16le_code_units_no_locale_conversion" : "raw_bytes_no_locale_conversion"},
        {"provenance", "serialized_pool_bytes_in_owned_image"}});
    const auto width = wide ? 2u : 1u;
    const auto pool = Monstey_InspectorU64(image.database) - image.base;
    if (address < image.base || Monstey_InspectorU64(address) - image.base >= pool) {
        out["status"] = address >= image.base && Monstey_InspectorU64(address) - image.base < image.allocation.size() ?
            "outside_pool_not_decoded" : "unavailable_outside_owned_allocation";
        return out;
    }
    const auto start = static_cast<std::size_t>(address - image.base);
    const auto available = static_cast<std::size_t>(pool) - start;
    const auto maximum = std::min<std::size_t>(available, cap - cap % width);
    std::size_t count = 0;
    bool terminated = false;
    while (count + width <= maximum) {
        if (image.allocation[start + count] == 0 && (!wide || image.allocation[start + count + 1] == 0)) {
            terminated = true;
            break;
        }
        count += width;
    }
    const auto bytes = std::span(image.allocation).subspan(start, count);
    out["bytes_hex"] = Monstey_ByteHex(bytes);
    out["emitted_bytes"] = static_cast<Monstey_InspectorU64>(count);
    out["terminator_observed"] = terminated;
    // An incomplete final code unit is a pool boundary, not an imposed display cap.
    out["status"] = terminated ? "terminated" : cap < available ? "truncated_at_display_limit" : "unavailable_terminator_in_pool";
    if (!wide) {
        std::string display;
        for (auto byte : bytes) {
            if (byte >= 32 && byte <= 126 && byte != '\\') display += static_cast<char>(byte);
            else if (byte == '\\') display += "\\\\";
            else { display += "\\x"; display += Monstey_ByteHex(std::span(&byte, 1)); }
        }
        out["display_ascii_escaped"] = std::move(display);
    }
    return out;
}
Monstey_Json Monstey_Float(Monstey_InspectorU32 bits) {
    const auto value = std::bit_cast<float>(bits);
    auto out = Monstey_Json::object({{"bits_u32", bits}, {"bits_hex", Monstey_Hex(bits)}});
    if (std::isfinite(value)) {
        out["value"] = static_cast<double>(value);
        out["classification"] = value == 0.0f && std::signbit(value) ? "negative_zero" : "finite";
    } else {
        out["value"] = nullptr;
        out["classification"] = std::isnan(value) ? "nan" : std::signbit(value) ? "negative_infinity" : "positive_infinity";
    }
    return out;
}
Monstey_Json Monstey_Node(const Monstey_FearDbPackedImage& image, Monstey_InspectorU32 handle,
                          Monstey_InspectorU32 ordinal, Monstey_InspectorU32 cap) {
    return Monstey_Json::object({{"ordinal", ordinal}, {"identity", Monstey_Identity(image, handle)},
        {"name", Monstey_PoolString(image, Monstey_FearDbNodeName(image.Memory(), handle), cap)}});
}
struct Monstey_RecordRef { Monstey_InspectorU32 identity, category, record; };
Monstey_Json Monstey_Value(const Monstey_FearDbPackedImage& image, Monstey_InspectorU32 attribute,
                          Monstey_InspectorU32 index, Monstey_InspectorU32 cap, const std::vector<Monstey_RecordRef>& records) {
    const auto memory = image.Memory();
    const auto type = Monstey_FearDbAttributeType(memory, attribute);
    const auto values = Monstey_ReadOwned(image, Monstey_InspectorU64(attribute) + 12);
    const auto word = Monstey_ReadOwned(image, Monstey_InspectorU64(values) + Monstey_InspectorU64(index) * 4);
    auto out = Monstey_Json::object({{"ordinal", index}, {"runtime_word_u32", word},
        {"runtime_word_hex", Monstey_Hex(word)}, {"word_provenance", "owned_runtime_word_after_loader_relocation_and_link_fixup"},
        {"original_packed_word", "not_retained_by_accepted_loader"}, {"runtime_type_u8", type}});
    if (type == 1) {
        out["kind"] = "bool_byte";
        out["byte_u8"] = static_cast<unsigned>(Monstey_FearDbBoolByte(memory, attribute, index, 0));
        out["normalized_bool"] = "not_claimed_original_returns_AL";
    } else if (type == 2) {
        out["kind"] = "float32_payload";
        out["float32"] = Monstey_Float(Monstey_FearDbFloat32Bits(memory, attribute, index, 0));
    } else if (type == 3) {
        out["kind"] = "int32_bits";
        out["signed_value"] = static_cast<std::int64_t>(std::bit_cast<std::int32_t>(Monstey_FearDbInt32Bits(memory, attribute, index, 0)));
    } else if (type == 4 || type == 5) {
        const auto identity = type == 4 ? Monstey_FearDbStringIdentity(memory, attribute, index, 0) :
                                          Monstey_FearDbWStringIdentity(memory, attribute, index, 0);
        out["kind"] = type == 4 ? "string_identity" : "wstring_identity";
        out["identity_provenance"] = "accepted_typed_getter_on_runtime_u8_type_and_u16_count";
        out["string"] = Monstey_PoolString(image, identity, cap, type == 5);
    } else if (type >= 6 && type <= 8) {
        const auto count = type - 4u;
        out["kind"] = "vector_pool_identity";
        out["identity"] = Monstey_Identity(image, word);
        out["identity_provenance"] = "runtime_value_word_selected_by_stored_type_and_count";
        if (word < image.base || Monstey_InspectorU64(word) + count * 4u > image.database) {
            out["status"] = word >= image.base && word < image.database ? "pool_span_crosses_boundary_not_decoded" :
                word >= image.base && Monstey_InspectorU64(word) - image.base < image.allocation.size() ?
                "outside_pool_not_decoded" : "unavailable_outside_owned_allocation";
        } else {
            out["status"] = "bounded_pool_components";
            auto components = Monstey_Json::array();
            for (Monstey_InspectorU32 i = 0; i < count; ++i)
                components.as_array().push_back(Monstey_Float(Monstey_ReadOwned(image, Monstey_InspectorU64(word) + i * 4u)));
            out["components"] = std::move(components);
        }
    } else if (type == 9) {
        const auto identity = Monstey_FearDbRecordIdentity(memory, attribute, index, 0);
        out["kind"] = "record_identity";
        out["identity"] = Monstey_Identity(image, identity);
        out["identity_provenance"] = "accepted_typed_getter_after_loader_packed_link_fixup";
        const auto found = std::find_if(records.begin(), records.end(), [identity](const auto& record) { return record.identity == identity; });
        if (found != records.end()) {
            out["status"] = "resolved_runtime_record";
            out["target_category_ordinal"] = found->category;
            out["target_record_ordinal"] = found->record;
        } else out["status"] = identity == 0 ? "null_after_loader_fixup_original_target_unavailable" : "unavailable_runtime_record_identity";
    } else {
        out["kind"] = "unknown_runtime_type";
        out["status"] = "raw_runtime_word_preserved_typed_interpretation_unavailable";
    }
    return out;
}
}
Monstey_Json Monstey_FearInspectPackedImage(const Monstey_FearDbPackedImage& image,
                                          const Monstey_FearInspectorOptions& options) {
    if (options.limit == 0 || options.limit > 10000 || options.string_bytes < 2 || options.string_bytes > 1048576)
        throw std::invalid_argument("limit must be 1..10000 and string-bytes 2..1048576");
    if ((options.record && !options.category) || (options.attribute && !options.record))
        throw std::invalid_argument("record requires category; attribute requires record");
    if (options.summary && (options.category || options.record || options.attribute))
        throw std::invalid_argument("summary cannot be combined with ordinal filters");
    const auto memory = image.Memory();
    const auto categories = Monstey_FearDbChildCount(memory, image.database);
    std::vector<Monstey_RecordRef> records;
    Monstey_InspectorU64 attribute_count = 0, value_count = 0;
    for (Monstey_InspectorU32 c = 0; c < categories; ++c) {
        const auto category = Monstey_FearDbChildByIndex(memory, image.database, c);
        const auto count = Monstey_FearDbChildCount(memory, category);
        for (Monstey_InspectorU32 r = 0; r < count; ++r) {
            const auto record = Monstey_FearDbChildByIndex(memory, category, r);
            records.push_back({record, c, r});
            const auto attributes = Monstey_FearDbChildCount(memory, record);
            attribute_count += attributes;
            for (Monstey_InspectorU32 a = 0; a < attributes; ++a)
                value_count += Monstey_FearDbValueCount(memory, Monstey_FearDbAttributeByIndex(memory, record, a));
        }
    }
    auto out = Monstey_Json::object({{"format", "legacy_FEAR_GADB3_packed_FEARDATABASE"},
        {"scope", "read_only_inspection_not_graph_export_or_writer"},
        {"totals", Monstey_Json::object({{"categories", categories}, {"records", static_cast<Monstey_InspectorU64>(records.size())},
            {"attributes", attribute_count}, {"runtime_visible_value_words", value_count}})},
        {"count_contract", "node_counts_u32_attribute_value_count_u16_type_u8_usage_u8"},
        {"physical_attribute_count_type_usage", "full_packed_fields_not_retained_by_accepted_loader"},
        {"original_native_calling_ABI", "unclaimed"}});
    if (options.summary) { out["listing"] = "summary"; return out; }
    Monstey_InspectorU32 parent = image.database, count = categories;
    const char* listing = "categories";
    if (options.category) {
        if (*options.category >= count) throw std::out_of_range("category ordinal out of range");
        parent = Monstey_FearDbChildByIndex(memory, parent, *options.category);
        out["selected_category"] = Monstey_Node(image, parent, *options.category, options.string_bytes);
        count = Monstey_FearDbChildCount(memory, parent); listing = "records";
    }
    if (options.record) {
        if (*options.record >= count) throw std::out_of_range("record ordinal out of range");
        parent = Monstey_FearDbChildByIndex(memory, parent, *options.record);
        out["selected_record"] = Monstey_Node(image, parent, *options.record, options.string_bytes);
        count = Monstey_FearDbChildCount(memory, parent); listing = "attributes";
    }
    if (options.attribute) {
        if (*options.attribute >= count) throw std::out_of_range("attribute ordinal out of range");
        parent = Monstey_FearDbAttributeByIndex(memory, parent, *options.attribute);
        auto attribute = Monstey_Node(image, parent, *options.attribute, options.string_bytes);
        attribute["runtime_type_u8"] = Monstey_FearDbAttributeType(memory, parent);
        attribute["runtime_usage_u8"] = Monstey_FearDbAttributeUsage(memory, parent);
        count = Monstey_FearDbValueCount(memory, parent);
        attribute["runtime_value_count_u16"] = count;
        out["selected_attribute"] = std::move(attribute); listing = "values";
    }
    const auto begin = std::min(options.offset, count);
    const auto end = static_cast<Monstey_InspectorU32>(std::min<Monstey_InspectorU64>(count, Monstey_InspectorU64(begin) + options.limit));
    auto items = Monstey_Json::array();
    for (Monstey_InspectorU32 i = begin; i < end; ++i) {
        if (options.attribute) items.as_array().push_back(Monstey_Value(image, parent, i, options.string_bytes, records));
        else {
            const auto child = options.record ? Monstey_FearDbAttributeByIndex(memory, parent, i) : Monstey_FearDbChildByIndex(memory, parent, i);
            auto node = Monstey_Node(image, child, i, options.string_bytes);
            if (options.record) {
                node["runtime_value_count_u16"] = Monstey_FearDbValueCount(memory, child);
                node["runtime_type_u8"] = Monstey_FearDbAttributeType(memory, child);
                node["runtime_usage_u8"] = Monstey_FearDbAttributeUsage(memory, child);
            } else node["child_count_u32"] = Monstey_FearDbChildCount(memory, child);
            items.as_array().push_back(std::move(node));
        }
    }
    out["listing"] = listing;
    out["items"] = std::move(items);
    out["page"] = Monstey_Json::object({{"offset_requested", options.offset}, {"offset_emitted", begin},
        {"limit", options.limit}, {"total", count}, {"emitted", end - begin}, {"has_more", end < count},
        {"next_offset", end < count ? Monstey_Json(end) : Monstey_Json(nullptr)}});
    return out;
}
}
