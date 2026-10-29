#include "Monstey_GadbPrivate.hpp"

namespace Monstey
{
using namespace Monstey_GadbPrivate;

J Monstey_Gadb::AtomUses(std::string_view selector, std::size_t offset, std::size_t limit) const
{
    const auto &p = *impl_;
    p.Unchanged();
    Monstey_Require(limit >= 1 && limit <= 10000, "Atom-use limit must be 1..10000");
    Monstey_Require(offset <= static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()),
                    "Atom-use offset exceeds JSON integer range");
    const auto ref = p.Select(selector);
    const auto &record = p.Base(ref);
    const auto descriptors = p.Descriptors(record);
    const auto &edits = p.Edits(ref);
    std::optional<std::vector<std::uint32_t>> words;
    auto Monstey_Words = [&]() -> const std::vector<std::uint32_t> &
    {
        if (!words)
            words = p.Words(record);
        return *words;
    };
    auto Monstey_Introduced = [&](std::uint32_t hash)
    { return record.payload && record.payload->introduced.contains(hash); };
    std::size_t total = 0;
    for (const auto &d : descriptors)
        total += edits.contains(d.hash) || Monstey_Introduced(d.hash) || d.count == 0 || d.type == 1
                     ? 1
                     : static_cast<std::size_t>(d.count);
    const auto begin = std::min(offset, total);
    const auto end = begin + std::min(limit, total - begin);
    J rows = J::array();
    std::map<std::pair<std::uint32_t, std::uint8_t>, J> atoms;
    auto Monstey_Atom = [&](std::uint32_t w, std::uint8_t type) -> J
    {
        const auto key = std::make_pair(w, type);
        if (const auto hit = atoms.find(key); hit != atoms.end())
            return hit->second;
        J a = J::object({{"pool_offset", w},
                         {"source_file_offset", J()},
                         {"logical_image_offset", J()},
                         {"length_bytes", J()},
                         {"span_sha256", J()},
                         {"span_valid", false},
                         {"ownership", type == 68 ? "raw_0x40_flag_unqualified" : "unqualified"},
                         {"status", "atom_offset_outside_pool"}});
        std::size_t length = 0;
        bool valid = false;
        if (type == 4 || type == 5 || type == 68)
        {
            if (w < p.stringSize)
            {
                std::size_t at = w;
                while (at < p.stringSize && p.data[p.stringBase + at] != 0)
                    ++at;
                if (at == p.stringSize)
                    a["status"] = "unterminated_narrow_string";
                else
                {
                    length = at - w + 1;
                    valid = true;
                    a["unit_count"] = length;
                }
            }
        }
        else if (type == 6)
        {
            if (w % 2 != 0)
                a["status"] = "unaligned_utf16_offset";
            else if (w < p.stringSize)
            {
                std::size_t at = w;
                J units = J::array();
                while (at <= p.stringSize && 2 <= p.stringSize - at)
                {
                    const auto unit = Monstey_U16(p.data, p.stringBase + at);
                    if (units.size() < 64)
                        units.as_array().push_back(unit);
                    at += 2;
                    if (unit == 0)
                    {
                        length = at - w;
                        valid = true;
                        break;
                    }
                }
                if (!valid)
                    a["status"] = "unterminated_utf16_string";
                else
                {
                    a["utf16_units"] = units;
                    a["unit_count"] = length / 2;
                    a["utf16_units_truncated"] = length / 2 > units.size();
                }
            }
        }
        else if (type >= 7 && type <= 9)
        {
            const auto count = static_cast<std::size_t>(type - 5);
            if (w % 4 != 0)
                a["status"] = "unaligned_vector_offset";
            else if (w <= p.stringSize)
            {
                if (count > (p.stringSize - w) / 4)
                    a["status"] = "vector_exceeds_pool";
                else
                {
                    valid = true;
                    length = count * 4;
                    J bits = J::array();
                    for (std::size_t i = 0; i < count; ++i)
                        bits.as_array().push_back(Monstey_U32(p.data, p.stringBase + w + i * 4));
                    a["component_count"] = count;
                    a["raw_f32_bits"] = bits;
                }
            }
        }
        else
        {
            const std::size_t stride = type == 16 ? 16 : 8;
            a["curve_stride_bytes"] = stride;
            if (w % 4 != 0)
                a["status"] = "unaligned_curve_offset";
            else if (w <= p.stringSize)
            {
                if (4 > p.stringSize - w)
                    a["status"] = "curve_header_exceeds_pool";
                else
                {
                    const auto count = Monstey_U32(p.data, p.stringBase + w);
                    const bool bounds = count <= (p.stringSize - w - 4) / stride;
                    a["curve_count"] = count;
                    a["curve_bounds_valid"] = bounds;
                    a["curve_adapter_decode_allowed"] = count <= 65536;
                    if (!bounds)
                        a["status"] = "curve_count_exceeds_pool";
                    else
                    {
                        valid = true;
                        length = 4 + static_cast<std::size_t>(count) * stride;
                        J bits = J::array();
                        if (count <= 65536)
                            for (std::size_t i = 0; i < std::min<std::size_t>(count, 16); ++i)
                            {
                                const auto at = p.stringBase + w + 4 + i * stride;
                                bits.as_array().push_back(Monstey_U32(p.data, at));
                                if (type == 13 || type == 16)
                                    for (std::size_t k = 1; k < stride / 4; ++k)
                                        bits.as_array().push_back(Monstey_U32(p.data, at + k * 4));
                            }
                        a["raw_f32_bits"] = bits;
                        a["numeric_preview_truncated"] = count > 16;
                    }
                }
            }
        }
        if (valid)
        {
            a["span_valid"] = true;
            a["length_bytes"] = length;
            a["logical_image_offset"] = p.stringBase + w;
            if (!p.memorySource)
                a["source_file_offset"] = p.wrapper.size() + p.stringBase + w;
            a["span_sha256"] =
                Monstey_HashSha256(std::span<const std::uint8_t>(p.data).subspan(p.stringBase + w, length));
            a["status"] = a.contains("curve_count") && !a.at("curve_adapter_decode_allowed").as_bool()
                              ? "atom_span_valid_adapter_curve_limit_exceeded"
                              : "atom_span_valid";
        }
        atoms.emplace(key, a);
        return a;
    };
    std::size_t cursor = 0;
    for (std::size_t physical = 0; physical < descriptors.size(); ++physical)
    {
        const auto &d = descriptors[physical];
        const bool introduced = Monstey_Introduced(d.hash);
        const bool pending = introduced || edits.contains(d.hash);
        const auto count = pending || d.count == 0 || d.type == 1 ? std::size_t{1} : d.count;
        const auto first = std::max(begin, cursor), last = std::min(end, cursor + count);
        for (auto use = first; use < last; ++use)
        {
            J row = J::object({{"use_index", use},
                               {"source_sha256", p.sourceHash},
                               {"record_reference", Monstey_Hex(ref)},
                               {"snapshot_record_reference", Monstey_Hex(record.reference)},
                               {"field_hash", Monstey_Hex(d.hash)},
                               {"native_ordinal", d.ordinal},
                               {"physical_index", physical},
                               {"sorted_lookup_physical_index", d.index},
                               {"raw_type", d.type},
                               {"snapshot_count", d.count},
                               {"future_serialized_count", J()},
                               {"introduced", introduced},
                               {"pending_typed_edit", edits.contains(d.hash)},
                               {"element", J()},
                               {"raw_u32", J()},
                               {"record_word_index", J()},
                               {"source_word_file_offset", J()},
                               {"bit_offset", J()},
                               {"atom", J()},
                               {"kind", "pending_typed_value"},
                               {"status", "not_yet_serialized"},
                               {"provenance", introduced       ? "not_yet_serialized_introduced_field"
                                              : pending        ? "not_yet_serialized_typed_edit"
                                              : record.payload ? "owned_layout_snapshot"
                                                               : "original_snapshot"}});
            if (!pending && d.count == 0)
            {
                row["kind"] = "empty_field";
                row["status"] = "no_values";
            }
            else if (!pending)
            {
                const auto element = use - cursor;
                const auto wi = d.type == 1 ? static_cast<std::size_t>(d.offset / 32)
                                            : static_cast<std::size_t>(d.offset) + element;
                const auto &raw = Monstey_Words();
                Monstey_Require(wi < raw.size(), "Atom-use word outside record snapshot");
                const auto w = raw[wi];
                row["element"] = element;
                row["record_word_index"] = wi;
                row["raw_u32"] = w;
                if (!record.payload && !p.memorySource)
                    row["source_word_file_offset"] = p.wrapper.size() + record.values + wi * 4;
                if (d.type == 1)
                {
                    row["kind"] = "packed_boolean";
                    row["status"] = "packed_boolean";
                    row["bit_offset"] = d.offset;
                    row["boolean_value"] = (w & (1u << (d.offset % 32))) != 0;
                }
                else if (d.type == 2 || d.type == 3)
                {
                    row["kind"] = d.type == 2 ? "immediate_float32" : "immediate_int32";
                    row["status"] = "inline_value";
                }
                else if (d.type == 10 || d.type == 11)
                {
                    row["kind"] = d.type == 10 ? "record_reference" : "record_reference_list_element";
                    row["status"] = w == 0xffffffffu ? "reference_null" : "reference_word_unresolved";
                    row["reference_category"] = w == 0xffffffffu ? J() : J(w >> 20);
                    row["reference_ordinal"] = w == 0xffffffffu ? J() : J(w & 0xfffffu);
                }
                else if ((d.type >= 4 && d.type <= 9) || d.type == 12 || d.type == 13 || d.type == 15 || d.type == 16 ||
                         d.type == 68)
                {
                    row["kind"] = d.type == 4    ? "narrow_string"
                                  : d.type == 5  ? "enum_string"
                                  : d.type == 68 ? "narrow_string_like"
                                  : d.type == 6  ? "utf16"
                                  : d.type <= 9  ? "vector"
                                                 : "curve";
                    row["atom"] = Monstey_Atom(w, d.type);
                    row["status"] = row.at("atom").at("status");
                }
                else
                {
                    row["kind"] = "opaque_word";
                    row["status"] = "unknown_payload_layout";
                }
            }
            rows.as_array().push_back(std::move(row));
        }
        cursor += count;
    }
    struct Monstey_AtomSpan
    {
        std::size_t begin, end, use;
        std::uint8_t type;
    };
    std::vector<Monstey_AtomSpan> spans;
    for (const auto &row : rows.as_array())
        if (!row.at("atom").is_null() && row.at("atom").at("span_valid").as_bool())
        {
            const auto &a = row.at("atom");
            const auto start = static_cast<std::size_t>(a.at("pool_offset").as_uint());
            spans.push_back({start, start + static_cast<std::size_t>(a.at("length_bytes").as_uint()),
                             static_cast<std::size_t>(row.at("use_index").as_uint()),
                             static_cast<std::uint8_t>(row.at("raw_type").as_uint())});
        }
    std::sort(spans.begin(), spans.end(), [](const auto &a, const auto &b)
              { return std::tie(a.begin, a.end, a.use) < std::tie(b.begin, b.end, b.use); });
    J relations = J::array();
    bool truncated = false;
    for (std::size_t i = 0; i < spans.size() && !truncated; ++i)
        for (std::size_t k = i + 1; k < spans.size() && spans[k].begin < spans[i].end; ++k)
        {
            if (relations.size() == 128)
            {
                truncated = true;
                break;
            }
            const auto &a = spans[i], &b = spans[k];
            const auto narrow = [](std::uint8_t t) { return t == 4 || t == 5 || t == 68; };
            const bool suffix = a.end == b.end && ((narrow(a.type) && narrow(b.type)) || (a.type == 6 && b.type == 6));
            relations.as_array().push_back(J::object({{"left_use_index", a.use},
                                                      {"right_use_index", b.use},
                                                      {"relation", a.begin == b.begin && a.end == b.end ? "shared_span"
                                                                   : suffix ? "suffix_overlap"
                                                                            : "overlap"}}));
        }
    p.Unchanged();
    return J::object(
        {{"schema", "Monstey.GadbAtomUses/1"},
         {"rows", rows},
         {"total", total},
         {"offset", offset},
         {"limit", limit},
         {"reference", Monstey_Hex(ref)},
         {"snapshot_record_reference", Monstey_Hex(record.reference)},
         {"source_sha256", p.sourceHash},
         {"schema_sha256", p.schemaHash},
         {"source_kind", p.memorySource ? "logical_memory_source" : "file_snapshot"},
         {"layout_provenance", record.payload ? "owned_layout_snapshot" : "original_snapshot"},
         {"order", "physical_descriptor_then_element"},
         {"pool", J::object({{"bytes", p.stringSize},
                             {"logical_image_offset", p.stringBase},
                             {"source_file_offset", p.memorySource ? J() : J(p.wrapper.size() + p.stringBase)},
                             {"wrapper_bytes", p.wrapper.size()}})},
         {"span_relations", relations},
         {"span_relation_limit", 128},
         {"span_relations_truncated", truncated},
         {"span_relation_scope", "returned_page_only"},
         {"curve_adapter_knot_limit", 65536},
         {"numeric_preview_knot_limit", 16},
         {"utf16_preview_unit_limit", 64},
         {"publication_plan_computed", false},
         {"portable_graph", false},
         {"native_ownership_qualified", false}});
}

} // namespace Monstey
