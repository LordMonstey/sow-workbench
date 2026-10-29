#include "Monstey_GadbPrivate.hpp"

namespace Monstey
{
using namespace Monstey_GadbPrivate;

J Monstey_Gadb::SetField(std::string_view selector, std::string_view field, const J &value)
{
    auto &p = *impl_;
    p.Unchanged();
    const auto ref = p.Select(selector);
    Monstey_Require(!p.state.deletes.contains(ref), "Cannot edit a deleted record");
    const auto hash = field.starts_with("0x") ? Monstey_ParseRef(field) : Monstey_NativeHash(field);
    const auto ds = p.Descriptors(p.Base(ref));
    const auto found = std::find_if(ds.begin(), ds.end(), [&](const auto &d) { return d.hash == hash; });
    Monstey_Require(found != ds.end(), "Field does not exist");
    const auto &f = p.SchemaField(ref, hash);
    Monstey_Require(!f.empty() && Monstey_Type(found->type) != "unknown" && !(found->type == 1 && p.Array(*found, f)),
                    "Field has no qualified native schema/type");
    const auto normalized = p.Normalize(ref, *found, f, value);
    const auto before = p.Metadata(ref);
    for (const auto &row : before.at("fields").as_array())
        if (row.at("hash").as_string() == Monstey_Hex(hash) && row.at("value") == normalized &&
            (!p.Base(ref).payload || !p.Base(ref).payload->introduced.contains(hash) || p.Edits(ref).contains(hash)))
            return Describe();
    p.Commit();
    if (auto clone = const_cast<Monstey_Clone *>(p.Clone(ref)))
        clone->edits[hash] = normalized;
    else
        p.state.edits[ref][hash] = normalized;
    auto report = Describe();
    report["record"] = std::string(selector);
    report["field"] = std::string(field);
    report["value"] = normalized;
    return report;
}

J Monstey_Gadb::CreateCategory(std::string_view schemaName, std::string_view name, std::string_view path)
{
    auto &p = *impl_;
    p.Unchanged();
    Monstey_Require(!schemaName.empty() && schemaName.size() <= 512 && schemaName.find('\0') == std::string_view::npos,
                    "Category schema needs 1..512 characters without NUL");
    const std::string label = name.empty() ? std::string(schemaName) : std::string(name);
    Monstey_Require(label.size() <= 512 && label.find('\0') == std::string::npos && path.size() <= 4096 &&
                        path.find('\0') == std::string_view::npos,
                    "Invalid category name/path");
    Monstey_Wide(std::string(schemaName));
    Monstey_Wide(label);
    Monstey_Wide(std::string(path));
    Monstey_Require(p.CategoryCount() < 4096, "Native category reference limit exceeded");
    const auto hash = Monstey_NativeHash(schemaName);
    for (std::uint32_t ci = 0; ci < p.CategoryCount(); ++ci)
        Monstey_Require(Monstey_NativeHash(p.CategoryText(ci, 0)) != hash, "Category schema hash already exists");
    p.Commit();
    p.state.categories.push_back({std::string(schemaName), label, std::string(path)});
    auto out = Describe();
    const auto rows = Categories();
    out["category"] = rows.at("rows").at(rows.at("rows").size() - 1);
    return out;
}

J Monstey_Gadb::CreateRecord(std::uint32_t category, std::string_view name, bool anonymous)
{
    auto &p = *impl_;
    p.Unchanged();
    Monstey_Require(category < p.CategoryCount(), "Category outside database");
    Monstey_Require(anonymous ? name.empty() : !name.empty(), "Named records need a name; anonymous records have none");
    Monstey_Require(name.size() <= 512 && name.find('\0') == std::string_view::npos, "Invalid native record name");
    Monstey_Wide(std::string(name));
    if (!anonymous)
        for (const auto ref : p.References(category))
            if (p.Base(ref).named)
                Monstey_Require(Monstey_NativeHash(p.Name(ref)) != Monstey_NativeHash(name),
                                "Record name hash already exists");
    const auto ref = p.NextReference(category);
    Monstey_NewRecord record;
    record.reference = ref;
    record.name = std::string(name);
    record.record.reference = ref;
    record.record.named = !anonymous;
    record.record.payload = std::make_shared<const Monstey_Payload>();
    p.Commit();
    p.state.records.push_back(std::move(record));
    auto out = Describe();
    out["record"] = p.Metadata(ref);
    return out;
}

J Monstey_Gadb::AddField(std::string_view selector, std::string_view name, std::uint8_t nativeType, const J &value,
                         const J &metadata)
{
    auto &p = *impl_;
    p.Unchanged();
    const auto ref = p.Select(selector);
    Monstey_Require(!p.state.deletes.contains(ref), "Cannot edit deleted record");
    Monstey_Require(!name.empty() && name.size() <= 512 && name.find('\0') == std::string_view::npos,
                    "Invalid field name");
    Monstey_Wide(std::string(name));
    Monstey_Require(Monstey_Type(nativeType) != "unknown" && metadata.is_object(),
                    "Unsupported native type or metadata");
    auto payload = p.Payload(ref);
    const auto hash = Monstey_NativeHash(name);
    for (const auto &d : payload.descriptors)
        Monstey_Require(d.hash != hash, "Field hash already exists");
    Monstey_Require(payload.descriptors.size() < 255, "Native descriptor count overflow");
    J f = p.SchemaField(ref, hash);
    if (f.contains("type_code"))
        Monstey_Require(f.at("type_code").as_uint() == nativeType, "Field type conflicts with qualified schema");
    if (f.contains("native_type"))
        Monstey_Require(f.at("native_type").as_uint() == nativeType, "Field type conflicts with qualified schema");
    if (f.empty())
        f = J::object();
    if (f.contains("native_ordinal") && metadata.contains("native_ordinal"))
        Monstey_Require(f.at("native_ordinal") == metadata.at("native_ordinal"),
                        "Field ordinal conflicts with qualified schema");
    for (const auto &[key, v] : metadata.as_object())
        f[key] = v;
    f["name"] = std::string(name);
    f["native_type"] = nativeType;
    if (!f.contains("array"))
        f["array"] = nativeType == 11 ? -1 : 1;
    const auto array = f.at("array").as_int();
    Monstey_Require(array >= -1 && array <= 65535, "Invalid native array length");
    const bool curve = nativeType == 12 || nativeType == 13 || nativeType == 15 || nativeType == 16;
    Monstey_Require(nativeType != 1 || (array >= 0 && array <= 1 && !f.value("reflected_array", J(false)).as_bool()),
                    "Packed bool arrays are unsupported");
    std::size_t count = 1;
    if (!curve && (nativeType == 11 || array == -1))
    {
        Monstey_Require(value.is_array(), "Variable field requires an array");
        count = value.size();
    }
    else if (!curve && array > 0)
        count = static_cast<std::size_t>(array);
    Monstey_Require(count <= 65535, "Field count overflow");
    Monstey_Descriptor d;
    d.hash = hash;
    std::set<std::uint8_t> identities;
    for (const auto &old : payload.descriptors)
        Monstey_Require(identities.insert(old.ordinal).second, "Duplicate existing native field ordinal");
    if (f.contains("native_ordinal"))
    {
        Monstey_Require(f.at("native_ordinal").is_integer() && f.at("native_ordinal").as_int() >= 0 &&
                            f.at("native_ordinal").as_uint() <= 255,
                        "Native field ordinal must be UInt8");
        d.ordinal = static_cast<std::uint8_t>(f.at("native_ordinal").as_uint());
        Monstey_Require(!identities.contains(d.ordinal), "Native field ordinal already exists");
    }
    else
    {
        unsigned identity = 0;
        while (identity <= 255 && identities.contains(static_cast<std::uint8_t>(identity)))
            ++identity;
        Monstey_Require(identity <= 255, "Native field ordinal space exhausted");
        d.ordinal = static_cast<std::uint8_t>(identity);
    }
    d.type = nativeType;
    d.count = static_cast<std::uint16_t>(count);
    const auto normalized = p.Normalize(ref, d, f, value);
    if (nativeType == 1)
    {
        Monstey_Require(payload.words.size() < 65535, "Native record value count overflow");
        for (auto &old : payload.descriptors)
        {
            const auto moved = static_cast<std::uint32_t>(old.offset) + (old.type == 1 ? 32u : 1u);
            Monstey_Require(moved <= 65535, "Packed boolean/word offset overflow");
            old.offset = static_cast<std::uint16_t>(moved);
        }
        payload.words.insert(payload.words.begin(), 0);
        d.offset = 0;
    }
    else
    {
        Monstey_Require(count <= 65535 - payload.words.size(), "Native record value count overflow");
        d.offset = static_cast<std::uint16_t>(payload.words.size());
        payload.words.resize(payload.words.size() + count, 0);
    }
    payload.descriptors.push_back(d);
    payload.metadata[hash] = f;
    payload.introduced.insert(hash);
    p.DescriptorOrder(payload);
    p.Commit();
    p.Install(ref, std::move(payload));
    if (auto clone = const_cast<Monstey_Clone *>(p.Clone(ref)))
        clone->edits[hash] = normalized;
    else
        p.state.edits[ref][hash] = normalized;
    auto out = Describe();
    out["record"] = std::string(selector);
    out["metadata"] = p.Metadata(ref);
    return out;
}

J Monstey_Gadb::DeleteField(std::string_view selector, std::string_view field)
{
    auto &p = *impl_;
    p.Unchanged();
    const auto ref = p.Select(selector);
    Monstey_Require(!p.state.deletes.contains(ref), "Cannot edit deleted record");
    const auto hash = field.starts_with("0x") ? Monstey_ParseRef(field) : Monstey_NativeHash(field);
    auto payload = p.Payload(ref);
    const auto hit = std::find_if(payload.descriptors.begin(), payload.descriptors.end(),
                                  [&](const auto &d) { return d.hash == hash; });
    Monstey_Require(hit != payload.descriptors.end(), "Field does not exist");
    payload.descriptors.erase(hit);
    payload.metadata.erase(hash);
    payload.introduced.erase(hash);
    p.DescriptorOrder(payload);
    p.Commit();
    p.Install(ref, std::move(payload));
    if (auto c = const_cast<Monstey_Clone *>(p.Clone(ref)))
        c->edits.erase(hash);
    else
        p.state.edits[ref].erase(hash);
    auto out = Describe();
    out["record"] = std::string(selector);
    out["metadata"] = p.Metadata(ref);
    return out;
}

J Monstey_Gadb::MoveField(std::string_view selector, std::string_view field, std::size_t newOrdinal)
{
    auto &p = *impl_;
    p.Unchanged();
    const auto ref = p.Select(selector);
    Monstey_Require(!p.state.deletes.contains(ref), "Cannot edit deleted record");
    const auto hash = field.starts_with("0x") ? Monstey_ParseRef(field) : Monstey_NativeHash(field);
    auto payload = p.Payload(ref);
    const auto hit = std::find_if(payload.descriptors.begin(), payload.descriptors.end(),
                                  [&](const auto &d) { return d.hash == hash; });
    Monstey_Require(hit != payload.descriptors.end() && newOrdinal < payload.descriptors.size(),
                    "Invalid field or destination ordinal");
    if (static_cast<std::size_t>(hit - payload.descriptors.begin()) != newOrdinal)
    {
        const auto d = *hit;
        payload.descriptors.erase(hit);
        payload.descriptors.insert(payload.descriptors.begin() + static_cast<std::ptrdiff_t>(newOrdinal), d);
        p.DescriptorOrder(payload);
        p.Commit();
        p.Install(ref, std::move(payload));
    }
    auto out = Describe();
    out["record"] = std::string(selector);
    out["metadata"] = p.Metadata(ref);
    return out;
}

J Monstey_Gadb::CloneRecord(std::string_view source, std::string_view newName)
{
    auto &p = *impl_;
    p.Unchanged();
    const auto ref = p.Select(source);
    Monstey_Require(!p.state.deletes.contains(ref), "Cannot clone deleted record");
    Monstey_Require(!newName.empty() && newName.size() <= 120, "Clone name needs 1..120 ASCII characters");
    std::string id(newName);
    for (const auto c : id)
        Monstey_Require((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
                            c == '.' || c == '-',
                        "Clone name needs ASCII identifier characters");
    if (!id.starts_with("Monstey_"))
        id = "Monstey_" + id;
    for (std::uint32_t ci = 0; ci < p.CategoryCount(); ++ci)
        for (const auto r : p.References(ci))
            Monstey_Require(p.Name(r) != id, "Clone id already exists");
    const auto next = p.NextReference(ref >> 20);
    Monstey_Clone clone;
    clone.id = id;
    clone.reference = next;
    clone.source = ref;
    clone.edits = p.Edits(ref);
    clone.snapshot = p.Base(ref);
    clone.snapshot.named = true;
    p.Commit();
    p.state.clones.push_back(std::move(clone));
    auto out = Describe();
    out["clone"] =
        J::object({{"id", id}, {"selector", id}, {"reference", Monstey_Hex(next)}, {"source", std::string(source)}});
    return out;
}

J Monstey_Gadb::RenameRecord(std::string_view selector, std::string_view newName)
{
    auto &p = *impl_;
    p.Unchanged();
    const auto ref = p.Select(selector);
    Monstey_Require(p.Base(ref).named || p.Clone(ref), "Anonymous records do not have names");
    Monstey_Require(!newName.empty() && newName.size() <= 512 && newName.find('\0') == std::string_view::npos,
                    "Invalid native record name");
    Monstey_Wide(std::string(newName));
    for (const auto other : p.References(ref >> 20))
        if (other != ref && p.Base(other).named)
            Monstey_Require(Monstey_NativeHash(p.Name(other)) != Monstey_NativeHash(newName),
                            "A record already has this native name hash");
    p.Commit();
    p.state.renames[ref] = std::string(newName);
    return Describe();
}

J Monstey_Gadb::DeleteRecord(std::string_view selector, const J &replacement)
{
    auto &p = *impl_;
    p.Unchanged();
    const auto ref = p.Select(selector);
    if (!replacement.is_null())
    {
        const auto to = p.ValueRef(replacement);
        Monstey_Require(to != ref, "Deleted record cannot replace itself");
    }
    p.Commit();
    p.state.deletes[ref] = replacement;
    return Describe();
}

} // namespace Monstey
