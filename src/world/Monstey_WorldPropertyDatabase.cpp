#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

Monstey_WPropertyDb::Monstey_WPropertyDb(B input) : raw(std::move(input))
{
    Monstey_Require(raw.size() >= 46 && std::memcmp(raw.data(), "GADB", 4) == 0 && Monstey_U32(raw, 4) == 17 &&
                        Monstey_U32(raw, 12) == 1 && Monstey_U32(raw, 20) == 0 && Monstey_U32(raw, 40) == 44,
                    "Expected qualified single-category ObjectDB GADB17");
    pool_start = 46ull + Monstey_U16(raw, 44);
    auto size = Monstey_U32(raw, 8);
    Monstey_CheckRange(raw, pool_start, size);
    pool_end = pool_start + size;
    pool.assign(raw.begin() + static_cast<std::ptrdiff_t>(pool_start),
                raw.begin() + static_cast<std::ptrdiff_t>(pool_end));
    descriptors = Monstey_U32(raw, 28);
    descriptor_end = pool_end + 15ull * descriptors;
    Monstey_CheckRange(raw, descriptor_end, 32);
    Monstey_Require(Monstey_U32(raw, descriptor_end) == 0 && Monstey_U32(raw, descriptor_end + 4) == 1,
                    "ObjectDB registry prefix differs");
    category_start = descriptor_end + 8;
    Monstey_Require(Monstey_WName(pool, Monstey_U32(raw, category_start)) == "ObjectDB" &&
                        Monstey_WName(pool, Monstey_U32(raw, category_start + 4)).empty() &&
                        Monstey_U32(raw, category_start + 12) == 0,
                    "Not the native ObjectDB schema");
    auto count = Monstey_U32(raw, category_start + 16);
    Monstey_Require(count == Monstey_U32(raw, 16) && count <= 65535, "ObjectDB record count differs");
    std::size_t at = category_start + 24;
    std::uint64_t values{};
    for (std::uint32_t j = 0; j < count; ++j)
    {
        Monstey_CheckRange(raw, at, 12);
        Record r;
        r.at = at;
        r.name = Monstey_U32(raw, at);
        r.packed = Monstey_U32(raw, at + 4);
        r.flags = Monstey_U32(raw, at + 8);
        Monstey_Require((r.flags & 0xffff0000u) == 0x08000000u && Monstey_WName(pool, r.name) == "ObjectRec",
                        "Unqualified ObjectDB record header");
        auto n = r.packed & 65535;
        Monstey_CheckRange(raw, at + 12, 4ull * n);
        for (std::uint32_t k = 0; k < n; ++k)
            r.words.push_back(Monstey_U32(raw, at + 12 + 4ull * k));
        r.end = at + 12 + 4ull * n;
        values += n;
        at = r.end;
        auto first = ((r.flags & 65535) << 8) | (r.packed >> 24), nf = (r.packed >> 16) & 255;
        Monstey_Require(first <= descriptors && nf <= descriptors - first, "ObjectDB field descriptors outside table");
        std::vector<std::uint32_t> hashes;
        for (std::uint32_t k = 0; k < nf; ++k)
        {
            auto p = pool_end + 15ull * (first + k);
            Desc d{Monstey_U32(raw, p),      Monstey_U32(raw, p + 4), raw[p + 8], raw[p + 9], raw[p + 10],
                   Monstey_U16(raw, p + 11), Monstey_U16(raw, p + 13)};
            Monstey_Require(d.type == 1 ? (d.count == 1 && d.offset < r.words.size() * 32)
                                        : static_cast<std::size_t>(d.offset) + d.count <= r.words.size(),
                            "ObjectDB field outside value words");
            r.fields.push_back(d);
            hashes.push_back(d.hash);
        }
        std::sort(hashes.begin(), hashes.end());
        Monstey_Require(std::adjacent_find(hashes.begin(), hashes.end()) == hashes.end(),
                        "Duplicate property field hash");
        for (std::size_t k = 0; k < r.fields.size(); ++k)
            Monstey_Require(r.fields[k].sorted == hashes[k] && r.fields[k].lookup < r.fields.size() &&
                                r.fields[r.fields[k].lookup].hash == r.fields[k].sorted,
                            "Invalid ObjectDB descriptor lookup");
        records.push_back(std::move(r));
    }
    Monstey_Require(values == Monstey_U32(raw, 24) && at + 4 == raw.size() && Monstey_U32(raw, at) == 0,
                    "ObjectDB values or trailer differ");
    for (std::uint32_t j = 0; j < records.size(); ++j)
        (void)Fields(j);
}

J Monstey_WPropertyDb::Value(const Record &r, const Desc &d) const
{
    if (d.type == 1)
        return (r.words[d.offset / 32] & (1u << (d.offset % 32))) != 0;
    J::Array values;
    for (std::size_t j = 0; j < d.count; ++j)
    {
        auto w = r.words[d.offset + j];
        J v;
        switch (d.type)
        {
        case 2:
        {
            auto f = std::bit_cast<float>(w);
            v = std::isfinite(f) ? J(static_cast<double>(f)) : J::object({{"raw", Monstey_Hex(w)}});
            break;
        }
        case 3:
            v = static_cast<std::int64_t>(static_cast<std::int32_t>(w));
            break;
        case 4:
        case 5:
        case 68:
            v = Monstey_WName(pool, w);
            break;
        case 6:
        {
            Monstey_Require(w % 2 == 0 && w < pool.size(), "Unaligned or outside UTF16 property atom");
            std::wstring text;
            std::size_t cursor = w;
            while (cursor + 2 <= pool.size() && Monstey_U16(pool, cursor) != 0)
            {
                text.push_back(static_cast<wchar_t>(Monstey_U16(pool, cursor)));
                cursor += 2;
            }
            Monstey_Require(cursor + 2 <= pool.size(), "Unterminated UTF16 property atom");
            v = Monstey_Utf8(text);
            break;
        }
        case 7:
        case 8:
        case 9:
            Monstey_Require(w % 4 == 0, "Unaligned property vector");
            v = Monstey_WFloats(pool, w, d.type - 5);
            break;
        case 10:
        case 11:
            v = w == 0xffffffffu ? J{}
                                 : J::object({{"reference", Monstey_Hex(w)}, {"unresolved", w >= records.size()}});
            break;
        default:
            v = J::object({{"raw", Monstey_Hex(w)}, {"undecoded_type", d.type}});
        }
        values.push_back(std::move(v));
    }
    return d.count == 1 ? values[0] : J(values);
}

std::pair<B, std::uint32_t> Monstey_WPropertyDb::Overlay(std::optional<std::uint32_t> source, const J &properties,
                                                         const J &types, const J &removals) const
{
    Monstey_Require(!records.empty() && records.size() < 65535,
                    "ObjectDB authoring requires a template record and free ordinal");
    Monstey_Require(properties.is_object() && types.is_object() && removals.is_array(),
                    "Invalid typed property overlay");
    auto atoms = pool;
    std::vector<std::uint32_t> words;
    std::vector<Desc> fields;
    if (source)
    {
        Monstey_Require(*source < records.size(), "Property source ordinal outside ObjectDB");
        words = records[*source].words;
        fields = records[*source].fields;
    }
    auto field_hash = [](const std::string &name)
    {
        if (name.starts_with("0x"))
            return Monstey_WUint(name);
        return Monstey_NativeHash(name);
    };
    std::set<std::uint32_t> removed, edited;
    for (auto &value : removals.as_array())
    {
        auto hash = field_hash(value.as_string());
        Monstey_Require(removed.insert(hash).second &&
                            std::any_of(fields.begin(), fields.end(), [&](auto &d) { return d.hash == hash; }),
                        "Removed property absent or duplicated");
    }
    std::erase_if(fields, [&](auto &d) { return removed.contains(d.hash); });
    auto atom = [&](const B &bytes)
    {
        Monstey_Require(atoms.size() % 4 == 0 && atoms.size() + bytes.size() + 3 <= 512ull * 1024 * 1024,
                        "Unaligned or excessive ObjectDB atom pool");
        auto pointer = static_cast<std::uint32_t>(atoms.size());
        atoms.insert(atoms.end(), bytes.begin(), bytes.end());
        while (atoms.size() % 4)
            atoms.push_back(0);
        return pointer;
    };
    for (auto &[name, value] : properties.as_object())
    {
        auto hash = field_hash(name);
        Monstey_Require(edited.insert(hash).second && !removed.contains(hash) && types.contains(name),
                        "Duplicate, removed or untyped property");
        auto kind = Monstey_WUint(types.at(name));
        Monstey_Require(kind == 1 || kind == 2 || kind == 3 || kind == 4 || kind == 5 || kind == 6 || kind == 7 ||
                            kind == 8 || kind == 9 || kind == 10 || kind == 11 || kind == 68,
                        "Property native type not qualified");
        auto old = std::find_if(fields.begin(), fields.end(), [&](auto &d) { return d.hash == hash; });
        if (old != fields.end())
            Monstey_Require(old->type == kind, "Existing property type cannot change");
        std::vector<J> values;
        if (kind == 1)
            values.push_back(value);
        else if (kind == 7 || kind == 8 || kind == 9)
        {
            if (value.is_array() && !value.empty() && value[0].is_array())
                values = value.as_array();
            else
                values.push_back(value);
        }
        else if (value.is_array())
            values = value.as_array();
        else
            values.push_back(value);
        Monstey_Require(values.size() <= 65535 && words.size() <= 65535, "Property values exceed native uint16 limits");
        auto offset = words.size();
        if (kind == 1)
        {
            Monstey_Require(values.size() == 1 && value.is_bool() && offset * 32 <= 65535,
                            "Packed bool property outside native layout");
            words.push_back(value.as_bool() ? 1u : 0u);
            offset *= 32;
        }
        else
            for (auto &v : values)
            {
                std::uint32_t word{};
                if (kind == 2)
                    word = std::bit_cast<std::uint32_t>(Monstey_WFloat(v));
                else if (kind == 3)
                {
                    Monstey_Require(v.is_integer() && v.as_int() >= -0x80000000ll && v.as_int() <= 0xffffffffll,
                                    "Property integer outside DWORD");
                    word = static_cast<std::uint32_t>(v.as_int());
                }
                else if (kind == 4 || kind == 5 || kind == 68)
                {
                    auto text = v.as_string();
                    Monstey_Require(text.find('\0') == std::string::npos, "Property string contains NUL");
                    (void)Monstey_Wide(text);
                    B bytes(text.begin(), text.end());
                    bytes.push_back(0);
                    word = atom(bytes);
                }
                else if (kind == 6)
                {
                    auto text = v.as_string();
                    Monstey_Require(text.find('\0') == std::string::npos, "Property text contains NUL");
                    B bytes;
                    for (auto c : Monstey_Wide(text))
                        Monstey_AppendU16(bytes, static_cast<std::uint16_t>(c));
                    Monstey_AppendU16(bytes, 0);
                    word = atom(bytes);
                }
                else if (kind == 7 || kind == 8 || kind == 9)
                {
                    B bytes;
                    for (auto f : Monstey_WVector(v, kind - 5))
                        Monstey_AppendU32(bytes, std::bit_cast<std::uint32_t>(f));
                    word = atom(bytes);
                }
                else
                {
                    word = v.is_null() ? 0xffffffffu : Monstey_WUint(v);
                    Monstey_Require(word == 0xffffffffu || word < records.size(),
                                    "New property reference does not resolve in ObjectDB");
                }
                words.push_back(word);
            }
        Desc d{hash,
               0,
               0,
               0,
               static_cast<std::uint8_t>(kind),
               static_cast<std::uint16_t>(values.size()),
               static_cast<std::uint16_t>(offset)};
        if (old == fields.end())
            fields.push_back(d);
        else
            *old = d;
    }
    Monstey_Require(fields.size() <= 255 && words.size() <= 65535 &&
                        std::uint64_t(descriptors) + fields.size() <= 0x1000000ull,
                    "Appended ObjectDB record exceeds native capacities");
    std::vector<std::size_t> sorted;
    for (std::size_t j = 0; j < fields.size(); ++j)
    {
        fields[j].index = static_cast<std::uint8_t>(j);
        sorted.push_back(j);
    }
    std::sort(sorted.begin(), sorted.end(), [&](auto x, auto y) { return fields[x].hash < fields[y].hash; });
    for (std::size_t j = 0; j < fields.size(); ++j)
    {
        fields[j].sorted = fields[sorted[j]].hash;
        fields[j].lookup = static_cast<std::uint8_t>(sorted[j]);
    }
    B out(raw.begin(), raw.begin() + static_cast<std::ptrdiff_t>(pool_start));
    Monstey_PutU32(out, 8, static_cast<std::uint32_t>(atoms.size()));
    Monstey_PutU32(out, 16, static_cast<std::uint32_t>(records.size() + 1));
    Monstey_PutU32(out, 24, Monstey_U32(raw, 24) + static_cast<std::uint32_t>(words.size()));
    Monstey_PutU32(out, 28, descriptors + static_cast<std::uint32_t>(fields.size()));
    out.insert(out.end(), atoms.begin(), atoms.end());
    out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(pool_end),
               raw.begin() + static_cast<std::ptrdiff_t>(descriptor_end));
    for (auto &d : fields)
    {
        Monstey_AppendU32(out, d.hash);
        Monstey_AppendU32(out, d.sorted);
        out.push_back(d.index);
        out.push_back(d.lookup);
        out.push_back(d.type);
        Monstey_AppendU16(out, d.count);
        Monstey_AppendU16(out, d.offset);
    }
    out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(descriptor_end),
               raw.begin() + static_cast<std::ptrdiff_t>(category_start));
    B category(raw.begin() + static_cast<std::ptrdiff_t>(category_start),
               raw.begin() + static_cast<std::ptrdiff_t>(category_start + 24));
    Monstey_PutU32(category, 16, static_cast<std::uint32_t>(records.size() + 1));
    out.insert(out.end(), category.begin(), category.end());
    out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(category_start + 24), raw.end() - 4);
    Monstey_AppendU32(out, records[0].name);
    Monstey_AppendU32(out, ((descriptors & 255u) << 24) | (static_cast<std::uint32_t>(fields.size()) << 16) |
                               static_cast<std::uint32_t>(words.size()));
    Monstey_AppendU32(out, 0x08000000u | (descriptors >> 8));
    for (auto word : words)
        Monstey_AppendU32(out, word);
    Monstey_AppendU32(out, 0);
    Monstey_WPropertyDb check(out);
    Monstey_Require(std::equal(pool.begin(), pool.end(), check.pool.begin()), "Original ObjectDB atom prefix changed");
    for (std::size_t j = 0; j < records.size(); ++j)
        Monstey_Require(std::equal(raw.begin() + static_cast<std::ptrdiff_t>(records[j].at),
                                   raw.begin() + static_cast<std::ptrdiff_t>(records[j].end),
                                   check.raw.begin() + static_cast<std::ptrdiff_t>(check.records[j].at)),
                        "Unrelated ObjectDB record changed");
    return {out, static_cast<std::uint32_t>(records.size())};
}

std::pair<B, J> Monstey_WPropertyDb::ImportClosure(const Monstey_WPropertyDb &donor, std::uint32_t root) const
{
    Monstey_Require(root < donor.records.size() && !records.empty(), "Imported ObjectDB root is absent");
    std::map<std::uint32_t, std::uint32_t> mapping;
    std::vector<std::uint32_t> queue;
    auto discover = [&](std::uint32_t ordinal)
    {
        Monstey_Require(ordinal < donor.records.size(), "Donor property reference is unresolved");
        if (!mapping.contains(ordinal))
        {
            Monstey_Require(records.size() + queue.size() < 65535,
                            "Imported ObjectDB closure exceeds native record capacity");
            mapping.emplace(ordinal, static_cast<std::uint32_t>(records.size() + queue.size()));
            queue.push_back(ordinal);
        }
    };
    discover(root);
    std::uint64_t added_descriptors{}, added_words{};
    for (std::size_t cursor = 0; cursor < queue.size(); ++cursor)
    {
        auto &record = donor.records[queue[cursor]];
        added_descriptors += record.fields.size();
        added_words += record.words.size();
        for (auto &field : record.fields)
        {
            Monstey_Require(field.type >= 1 && (field.type <= 11 || field.type == 68),
                            "Imported ObjectDB closure has an unqualified field type");
            if (field.type == 10 || field.type == 11)
                for (std::size_t k = 0; k < field.count; ++k)
                {
                    auto reference = record.words[field.offset + k];
                    if (reference != 0xffffffffu)
                        discover(reference);
                }
        }
    }
    Monstey_Require(std::uint64_t(descriptors) + added_descriptors <= 0x1000000ull &&
                        std::uint64_t(Monstey_U32(raw, 24)) + added_words <= 0xffffffffull,
                    "Imported ObjectDB descriptors or values exceed native capacity");
    B atoms = pool;
    while (atoms.size() % 4)
        atoms.push_back(0);
    auto atom_base = atoms.size();
    Monstey_Require(atom_base + donor.pool.size() + 3 <= 512ull * 1024 * 1024,
                    "Imported ObjectDB atom pool exceeds bounded extent");
    atoms.insert(atoms.end(), donor.pool.begin(), donor.pool.end());
    while (atoms.size() % 4)
        atoms.push_back(0);
    B descriptor_bytes, record_bytes;
    auto next_descriptor = descriptors;
    for (auto ordinal : queue)
    {
        auto &record = donor.records[ordinal];
        auto words = record.words;
        std::map<std::size_t, std::uint32_t> relocated;
        for (auto &field : record.fields)
        {
            if (field.type == 1 || field.type == 2 || field.type == 3)
                continue;
            for (std::size_t k = 0; k < field.count; ++k)
            {
                auto offset = static_cast<std::size_t>(field.offset) + k;
                auto old = record.words[offset], value = old;
                if (field.type == 10 || field.type == 11)
                    value = old == 0xffffffffu ? old : mapping.at(old);
                else
                {
                    Monstey_Require(old < donor.pool.size() && atom_base + old <= 0xffffffffull,
                                    "Imported property atom outside donor pool");
                    value = static_cast<std::uint32_t>(atom_base + old);
                }
                auto [it, inserted] = relocated.emplace(offset, value);
                Monstey_Require(inserted || it->second == value,
                                "Imported overlapping property words require contradictory relocation");
                words[offset] = value;
            }
        }
        for (auto &field : record.fields)
        {
            Monstey_AppendU32(descriptor_bytes, field.hash);
            Monstey_AppendU32(descriptor_bytes, field.sorted);
            descriptor_bytes.insert(descriptor_bytes.end(), {field.index, field.lookup, field.type});
            Monstey_AppendU16(descriptor_bytes, field.count);
            Monstey_AppendU16(descriptor_bytes, field.offset);
        }
        Monstey_AppendU32(record_bytes, records[0].name);
        Monstey_AppendU32(record_bytes, ((next_descriptor & 255u) << 24) |
                                            (static_cast<std::uint32_t>(record.fields.size()) << 16) |
                                            static_cast<std::uint32_t>(words.size()));
        Monstey_AppendU32(record_bytes, 0x08000000u | (next_descriptor >> 8));
        for (auto word : words)
            Monstey_AppendU32(record_bytes, word);
        next_descriptor += static_cast<std::uint32_t>(record.fields.size());
    }
    B out(raw.begin(), raw.begin() + static_cast<std::ptrdiff_t>(pool_start));
    auto total = static_cast<std::uint32_t>(records.size() + queue.size());
    Monstey_PutU32(out, 8, static_cast<std::uint32_t>(atoms.size()));
    Monstey_PutU32(out, 16, total);
    Monstey_PutU32(out, 24, Monstey_U32(raw, 24) + static_cast<std::uint32_t>(added_words));
    Monstey_PutU32(out, 28, next_descriptor);
    out.insert(out.end(), atoms.begin(), atoms.end());
    out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(pool_end),
               raw.begin() + static_cast<std::ptrdiff_t>(descriptor_end));
    out.insert(out.end(), descriptor_bytes.begin(), descriptor_bytes.end());
    out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(descriptor_end),
               raw.begin() + static_cast<std::ptrdiff_t>(category_start));
    B category(raw.begin() + static_cast<std::ptrdiff_t>(category_start),
               raw.begin() + static_cast<std::ptrdiff_t>(category_start + 24));
    Monstey_PutU32(category, 16, total);
    out.insert(out.end(), category.begin(), category.end());
    out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(category_start + 24), raw.end() - 4);
    out.insert(out.end(), record_bytes.begin(), record_bytes.end());
    Monstey_AppendU32(out, 0);
    Monstey_WPropertyDb checked(out);
    Monstey_Require(std::equal(pool.begin(), pool.end(), checked.pool.begin()),
                    "Imported closure changed original target atom pool");
    for (std::size_t j = 0; j < records.size(); ++j)
        Monstey_Require(std::equal(raw.begin() + static_cast<std::ptrdiff_t>(records[j].at),
                                   raw.begin() + static_cast<std::ptrdiff_t>(records[j].end),
                                   checked.raw.begin() + static_cast<std::ptrdiff_t>(checked.records[j].at)),
                        "Imported closure changed an existing target record");
    J::Array proof;
    for (auto old : queue)
        proof.push_back(J::object({{"source_ordinal", old}, {"target_ordinal", mapping.at(old)}}));
    return {out, J::object({{"root_ordinal", mapping.at(root)},
                            {"records", proof},
                            {"atom_pool_base", static_cast<std::uint64_t>(atom_base)},
                            {"cyclic_and_forward_references_remapped", true}})};
}

J Monstey_WPropertyDb::Fields(std::uint32_t ref) const
{
    Monstey_Require(ref < records.size(), "Object property reference outside ObjectDB");
    auto &r = records[ref];
    J::Array fields;
    for (auto &d : r.fields)
        fields.push_back(J::object({{"hash", Monstey_Hex(d.hash)},
                                    {"name", Monstey_Hex(d.hash)},
                                    {"type", d.type},
                                    {"count", d.count},
                                    {"value", Value(r, d)}}));
    return fields;
}

} // namespace Monstey_WorldPrivate

} // namespace Monstey
