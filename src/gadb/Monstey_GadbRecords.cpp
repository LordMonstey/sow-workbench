#include "Monstey_GadbPrivate.hpp"

namespace Monstey
{
using namespace Monstey_GadbPrivate;

const Monstey_RecordData &Monstey_Gadb::Monstey_Impl::Native(std::uint32_t ref) const
{
    const auto ci = ref >> 20, ri = ref & 0xfffffu;
    Monstey_Require(ci < categories.size() && ri < categories[ci].records.size(), "Reference outside database");
    return categories[ci].records[ri];
}

const Monstey_Clone *Monstey_Gadb::Monstey_Impl::Clone(std::uint32_t ref) const
{
    for (const auto &c : state.clones)
        if (c.reference == ref)
            return &c;
    return nullptr;
}

std::size_t Monstey_Gadb::Monstey_Impl::CategoryCount() const
{
    return categories.size() + state.categories.size();
}

std::string Monstey_Gadb::Monstey_Impl::CategoryText(std::uint32_t ci, int which) const
{
    Monstey_Require(ci < CategoryCount(), "Category outside database");
    if (ci < categories.size())
    {
        const auto &c = categories[ci];
        return String(which == 0 ? c.schema : which == 1 ? c.name : c.path);
    }
    const auto &c = state.categories[ci - categories.size()];
    return which == 0 ? c.schema : which == 1 ? c.name : c.path;
}

const Monstey_NewRecord *Monstey_Gadb::Monstey_Impl::Created(std::uint32_t ref) const
{
    for (const auto &r : state.records)
        if (r.reference == ref)
            return &r;
    return nullptr;
}

const Monstey_RecordData &Monstey_Gadb::Monstey_Impl::Base(std::uint32_t ref) const
{
    if (const auto i = state.layouts.find(ref); i != state.layouts.end())
        return i->second;
    if (const auto c = Clone(ref))
        return c->snapshot;
    if (const auto c = Created(ref))
        return c->record;
    return Native(ref);
}

std::vector<std::uint32_t> Monstey_Gadb::Monstey_Impl::References(std::uint32_t ci) const
{
    Monstey_Require(ci < CategoryCount(), "Category outside database");
    std::vector<std::uint32_t> refs;
    if (ci < categories.size())
        for (const auto &r : categories[ci].records)
            refs.push_back(r.reference);
    for (const auto &r : state.records)
        if ((r.reference >> 20) == ci)
            refs.push_back(r.reference);
    for (const auto &r : state.clones)
        if ((r.reference >> 20) == ci)
            refs.push_back(r.reference);
    std::sort(refs.begin(), refs.end());
    return refs;
}

std::uint32_t Monstey_Gadb::Monstey_Impl::NextReference(std::uint32_t ci) const
{
    const auto refs = References(ci);
    const auto ordinal = refs.empty() ? 0u : (refs.back() & 0xfffffu) + 1;
    Monstey_Require(ordinal < 0x100000, "Native record ordinal overflow");
    return ci << 20 | ordinal;
}

std::uint32_t Monstey_Gadb::Monstey_Impl::Select(std::string_view selector) const
{
    for (const auto &c : state.clones)
        if (c.id == selector)
            return c.reference;
    if (selector.starts_with("0x") || selector.starts_with("0X"))
    {
        const auto ref = Monstey_ParseRef(selector);
        Base(ref);
        return ref;
    }
    const auto pos = selector.rfind(':');
    const auto cat = pos == std::string_view::npos ? std::string_view{} : selector.substr(0, pos);
    const auto name = pos == std::string_view::npos ? selector : selector.substr(pos + 1);
    const auto hash = Monstey_NativeHash(name);
    std::optional<std::uint32_t> found;
    auto match = [&](std::uint32_t ref)
    {
        if (Base(ref).named && Monstey_KeyEqual(Name(ref), name))
        {
            Monstey_Require(!found || *found == ref, "Ambiguous record name; prefix its category");
            found = ref;
        }
    };
    for (std::uint32_t ci = 0; ci < CategoryCount(); ++ci)
    {
        if (!cat.empty() && !Monstey_KeyEqual(CategoryText(ci, 1), cat) && !Monstey_KeyEqual(CategoryText(ci, 0), cat))
            continue;
        if (ci < categories.size())
        {
            const auto &c = categories[ci];
            const auto first = c.records.begin() + c.anonymous;
            const auto hit = std::lower_bound(first, c.records.end(), hash, [&](const auto &r, std::uint32_t h)
                                              { return Monstey_NativeHash(String(r.name)) < h; });
            if (hit != c.records.end() && Monstey_NativeHash(String(hit->name)) == hash)
                match(hit->reference);
        }
        for (const auto &[ref, renamed] : state.renames)
        {
            (void)renamed;
            if ((ref >> 20) == ci)
                match(ref);
        }
        for (const auto &r : state.records)
            if ((r.reference >> 20) == ci)
                match(r.reference);
        for (const auto &r : state.clones)
            if ((r.reference >> 20) == ci)
                match(r.reference);
    }
    Monstey_Require(found.has_value(), "Record not found: " + std::string(selector));
    return *found;
}

std::vector<std::uint32_t> Monstey_Gadb::Monstey_Impl::Words(const Monstey_RecordData &r) const
{
    if (r.payload)
        return r.payload->words;
    std::vector<std::uint32_t> out;
    out.reserve(r.packed & 65535u);
    for (std::size_t i = 0; i < (r.packed & 65535u); ++i)
        out.push_back(Monstey_U32(data, r.values + i * 4));
    return out;
}

std::vector<Monstey_Descriptor> Monstey_Gadb::Monstey_Impl::Descriptors(const Monstey_RecordData &r) const
{
    if (r.payload)
        return r.payload->descriptors;
    const auto start = ((r.flags & 65535u) << 8) | (r.packed >> 24), count = (r.packed >> 16) & 255u;
    std::vector<Monstey_Descriptor> ds;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto p = descriptorBase + (start + i) * 15;
        ds.push_back({Monstey_U32(data, p), Monstey_U32(data, p + 4), data[p + 8], data[p + 9], data[p + 10],
                      Monstey_U16(data, p + 11), Monstey_U16(data, p + 13)});
    }
    std::vector<std::uint32_t> hashes;
    for (const auto &d : ds)
        hashes.push_back(d.hash);
    std::sort(hashes.begin(), hashes.end());
    for (std::size_t i = 0; i < ds.size(); ++i)
    {
        const auto &d = ds[i];
        Monstey_Require(d.sorted == hashes[i] && d.index < ds.size() && ds[d.index].hash == d.sorted,
                        "Invalid descriptor sorted lookup");
        if (d.type == 1)
            Monstey_Require(d.count == 1 && d.offset < (r.packed & 65535u) * 32u, "Invalid packed boolean layout");
        else
            Monstey_Require(static_cast<std::uint32_t>(d.offset) + d.count <= (r.packed & 65535u),
                            "Field exceeds record values");
    }
    return ds;
}

const J &Monstey_Gadb::Monstey_Impl::SchemaField(std::uint32_t ref, std::uint32_t hash) const
{
    static const J empty = J::object();
    const auto &r = Base(ref);
    if (r.payload)
        if (const auto hit = r.payload->metadata.find(hash); hit != r.payload->metadata.end())
            return hit->second;
    const auto ci = ref >> 20;
    if (ci < schemaFields.size())
    {
        const auto hit = schemaFields[ci].find(hash);
        if (hit != schemaFields[ci].end())
            return *hit->second;
    }
    if (schema.contains("categories"))
        for (const auto label : {CategoryText(ci, 1), CategoryText(ci, 0)})
            if (schema.at("categories").contains(label))
            {
                const auto &cat = schema.at("categories").at(label);
                if (cat.contains("fields"))
                    for (const auto &f : cat.at("fields").as_array())
                        if (f.contains("name") && Monstey_NativeHash(f.at("name").as_string()) == hash)
                            return f;
            }
    return empty;
}

Monstey_Payload Monstey_Gadb::Monstey_Impl::Payload(std::uint32_t ref) const
{
    const auto &r = Base(ref);
    if (r.payload)
        return *r.payload;
    Monstey_Payload result;
    result.words = Words(r);
    result.descriptors = Descriptors(r);
    return result;
}

void Monstey_Gadb::Monstey_Impl::DescriptorOrder(Monstey_Payload &p)
{
    Monstey_Require(p.descriptors.size() <= 255 && p.words.size() <= 65535,
                    "Native record descriptor/value limit exceeded");
    std::vector<std::pair<std::uint32_t, std::uint8_t>> sorted;
    std::set<std::uint8_t> identities;
    for (std::size_t i = 0; i < p.descriptors.size(); ++i)
    {
        // +8 is the immutable native field identity, not this physical row index.
        Monstey_Require(identities.insert(p.descriptors[i].ordinal).second, "Duplicate native field ordinal");
        sorted.emplace_back(p.descriptors[i].hash, static_cast<std::uint8_t>(i));
    }
    std::sort(sorted.begin(), sorted.end());
    for (std::size_t i = 0; i < sorted.size(); ++i)
    {
        Monstey_Require(i == 0 || sorted[i - 1].first < sorted[i].first, "Duplicate field hash");
        p.descriptors[i].sorted = sorted[i].first;
        p.descriptors[i].index = sorted[i].second;
    }
}

void Monstey_Gadb::Monstey_Impl::Install(std::uint32_t ref, Monstey_Payload payload)
{
    auto r = Base(ref);
    r.payload = std::make_shared<const Monstey_Payload>(std::move(payload));
    r.packed = static_cast<std::uint32_t>(r.payload->words.size()) |
               static_cast<std::uint32_t>(r.payload->descriptors.size()) << 16;
    state.layouts[ref] = std::move(r);
}

std::string Monstey_Gadb::Monstey_Impl::Name(std::uint32_t ref) const
{
    if (const auto it = state.renames.find(ref); it != state.renames.end())
        return it->second;
    if (const auto c = Clone(ref))
        return c->id;
    if (const auto c = Created(ref))
        return c->name;
    const auto &r = Native(ref);
    return r.named ? String(r.name) : std::string{};
}

const std::map<std::uint32_t, J> &Monstey_Gadb::Monstey_Impl::Edits(std::uint32_t ref) const
{
    if (const auto c = Clone(ref))
        return c->edits;
    static const std::map<std::uint32_t, J> empty;
    const auto it = state.edits.find(ref);
    return it == state.edits.end() ? empty : it->second;
}

J Monstey_Gadb::Monstey_Impl::Metadata(std::uint32_t ref) const
{
    const auto &r = Base(ref);
    J rows = J::array();
    const auto &changes = Edits(ref);
    for (const auto &d : Descriptors(r))
    {
        const auto &f = SchemaField(ref, d.hash);
        const auto name = f.contains("name")
                              ? f.at("name").as_string()
                              : (fieldNames.contains(d.hash) ? fieldNames.at(d.hash) : Monstey_Hex(d.hash));
        const bool arr = Array(d, f), editable = !f.empty() && Monstey_Type(d.type) != "unknown" &&
                                                 !(d.type == 1 && arr) && (d.count != 0 || Variable(d, f));
        const auto old = r.payload && r.payload->introduced.contains(d.hash) ? J() : Decode(r, d, arr);
        const auto edit = changes.find(d.hash);
        J row = J::object({{"name", name},
                           {"hash", Monstey_Hex(d.hash)},
                           {"type", d.type},
                           {"native_type", d.type},
                           {"native_ordinal", d.ordinal},
                           {"physical_index", static_cast<std::uint64_t>(rows.size())},
                           {"type_name", Monstey_Type(d.type)},
                           {"count", d.count},
                           {"array", arr},
                           {"variable_array", Variable(d, f)},
                           {"editable", editable},
                           {"reason", editable ? "" : "Unknown schema field or unsupported native layout"},
                           {"value", edit == changes.end() ? old : edit->second},
                           {"original_value", old},
                           {"dirty", edit != changes.end()}});
        for (const auto key :
             {"min", "max", "enum", "enum_values", "target", "constraint", "default", "help", "description"})
            if (f.contains(key))
                row[key] = f.at(key);
        rows.as_array().push_back(row);
    }
    return J::object({{"reference", Monstey_Hex(ref)},
                      {"selector", Clone(ref) ? J(Clone(ref)->id) : J(Monstey_Hex(ref))},
                      {"name", Monstey_DisplayUtf8(Name(ref))},
                      {"name_hash", r.named || Clone(ref) ? J(Monstey_Hex(Monstey_NativeHash(Name(ref)))) : J()},
                      {"category", CategoryText(ref >> 20, 1)},
                      {"category_index", ref >> 20},
                      {"clone", Clone(ref) != nullptr},
                      {"anonymous", !r.named},
                      {"pending_create", Created(ref) != nullptr},
                      {"pending_delete", state.deletes.contains(ref)},
                      {"fields", rows}});
}

} // namespace Monstey
