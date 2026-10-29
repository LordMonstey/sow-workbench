#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

std::vector<Monstey_WObjectGroup> Monstey_WObjectGroups(const Monstey_WObjres &doc)
{
    auto &bytes = doc.chunks[doc.obj_chunk].bytes;
    std::size_t at = 28 + doc.classes.size() + doc.names.size() + doc.extra.size();
    std::vector<Monstey_WObjectGroup> groups;
    for (std::uint32_t j = 0; j < doc.groups; ++j)
    {
        auto key = Monstey_U32(bytes, at), count = Monstey_U32(bytes, at + 4);
        at += 8;
        Monstey_WObjectGroup group{key, {}};
        for (std::uint32_t k = 0; k < count; ++k)
        {
            group.records.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(at),
                                       bytes.begin() + static_cast<std::ptrdiff_t>(at + 50));
            at += 50;
        }
        groups.push_back(std::move(group));
    }
    return groups;
}

B Monstey_WEncodeObjectGroups(const B &classes, const B &names, const B &extra,
                              const std::vector<Monstey_WObjectGroup> &groups)
{
    std::uint64_t total{};
    for (auto &g : groups)
        total += g.records.size();
    Monstey_Require(total <= 0xffffffffull && classes.size() <= 0xffffffffull && names.size() <= 0xffffffffull &&
                        extra.size() <= 0xffffffffull,
                    "OBJL counts exceed uint32");
    B out{'O', 'B', 'J', 'L'};
    for (auto n : {5u, static_cast<std::uint32_t>(groups.size()), static_cast<std::uint32_t>(total),
                   static_cast<std::uint32_t>(classes.size()), static_cast<std::uint32_t>(names.size()),
                   static_cast<std::uint32_t>(extra.size())})
        Monstey_AppendU32(out, n);
    out.insert(out.end(), classes.begin(), classes.end());
    out.insert(out.end(), names.begin(), names.end());
    out.insert(out.end(), extra.begin(), extra.end());
    for (auto &g : groups)
    {
        Monstey_AppendU32(out, g.identity);
        Monstey_AppendU32(out, static_cast<std::uint32_t>(g.records.size()));
        for (auto &bytes : g.records)
        {
            Monstey_Require(bytes.size() == 50, "Object record must be 50 bytes");
            out.insert(out.end(), bytes.begin(), bytes.end());
        }
    }
    return out;
}

Monstey_WObjres::Object Monstey_WSelectObject(const Monstey_WObjres &doc, const J &a, const char *name_key,
                                              const char *group_key, const char *index_key)
{
    auto name = a.at(name_key).as_string();
    auto &bytes = doc.chunks[doc.obj_chunk].bytes;
    std::vector<Monstey_WObjres::Object> matches;
    if (a.contains(group_key) || a.contains(index_key))
    {
        Monstey_Require(a.contains(group_key) && a.contains(index_key), "Both stable object indices required");
        auto group = a.at(group_key).as_uint(), index = a.at(index_key).as_uint();
        for (auto &obj : doc.objects)
            if (obj.group == group && obj.index == index)
                matches.push_back(obj);
    }
    else
        for (auto &obj : doc.objects)
            if (Monstey_WName(doc.names, Monstey_U32(bytes, obj.at + 4)) == name)
                matches.push_back(obj);
    Monstey_Require(matches.size() == 1 && Monstey_WName(doc.names, Monstey_U32(bytes, matches[0].at + 4)) == name,
                    "Object identity does not match selector");
    if (a.contains("class_name"))
        Monstey_Require(Monstey_WName(doc.classes, Monstey_U32(bytes, matches[0].at)) == a.at("class_name").as_string(),
                        "Selected object class changed");
    return matches[0];
}

void Monstey_WNewIdentity(const Monstey_WObjres &doc, const std::string &name)
{
    Monstey_Require(!name.empty() && name.size() < 65536 &&
                        std::all_of(name.begin(), name.end(), [](char c) { return c >= 32 && c < 127; }),
                    "Object identity must be nonempty printable ASCII");
    auto hash = Monstey_NativeHash(name);
    auto &bytes = doc.chunks[doc.obj_chunk].bytes;
    for (auto &obj : doc.objects)
    {
        auto existing = Monstey_WName(doc.names, Monstey_U32(bytes, obj.at + 4));
        Monstey_Require(existing != name && Monstey_NativeHash(existing) != hash,
                        "Object name or native identity hash collides");
    }
}

void Monstey_WQualifiedIdentity(const Monstey_WObjres &doc, const Monstey_WObjres::Object &obj, bool include_self)
{
    auto &bytes = doc.chunks[doc.obj_chunk].bytes;
    auto name = Monstey_WName(doc.names, Monstey_U32(bytes, obj.at + 4));
    auto prefix = Monstey_U16(bytes, obj.at + 8);
    Monstey_Require(prefix == 65535 || Monstey_WName(doc.extra, prefix).empty(),
                    "Object has an unresolved native prefab prefix");
    B native_name(name.begin(), name.end());
    native_name.push_back(0);
    B native_hash;
    Monstey_AppendU32(native_hash, Monstey_NativeHash(name));
    for (std::size_t j = 0; j < doc.chunks.size(); ++j)
        if (j != doc.obj_chunk && j != doc.db_chunk)
        {
            auto &c = doc.chunks[j].bytes;
            Monstey_Require(std::search(c.begin(), c.end(), native_name.begin(), native_name.end()) == c.end() &&
                                std::search(c.begin(), c.end(), native_hash.begin(), native_hash.end()) == c.end(),
                            "Object identity has unqualified links in an opaque native chunk");
        }
    for (auto &other : doc.objects)
    {
        if (!include_self && other.group == obj.group && other.index == obj.index)
            continue;
        auto &record = doc.db->records[Monstey_U32(bytes, other.at + 10)];
        for (auto &d : record.fields)
            if (d.type == 4 || d.type == 5 || d.type == 68)
                for (std::size_t k = 0; k < d.count; ++k)
                    Monstey_Require(Monstey_WName(doc.db->pool, record.words[d.offset + k]) != name,
                                    "Object identity is referenced by an unqualified property string");
    }
}

void Monstey_WQualifiedClone(const Monstey_WObjres &doc, const Monstey_WObjres::Object &object)
{
    auto &bytes = doc.chunks[doc.obj_chunk].bytes;
    auto name = Monstey_WName(doc.names, Monstey_U32(bytes, object.at + 4));
    auto prefix = Monstey_U16(bytes, object.at + 8);
    Monstey_Require(prefix == 65535 || Monstey_WName(doc.extra, prefix).empty(),
                    "Cloned owner has an unresolved native prefab prefix");
    // Existing incoming references remain attached to the preserved source owner.
    // Only copied self-identity strings would require an unqualified relocation.
    auto &record = doc.db->records[Monstey_U32(bytes, object.at + 10)];
    for (auto &field : record.fields)
        if (field.type == 4 || field.type == 5 || field.type == 68)
            for (std::size_t i = 0; i < field.count; ++i)
                Monstey_Require(Monstey_WName(doc.db->pool, record.words[field.offset + i]) != name,
                                "Cloned owner property has an unqualified self-identity string");
}

J Monstey_WPropertyTypes(const Monstey_WObjres &doc, const std::string &kind, const J &properties,
                         const J &explicit_types, std::optional<std::uint32_t> record)
{
    std::map<std::uint32_t, std::set<unsigned>> native;
    std::size_t examples{};
    auto &bytes = doc.chunks[doc.obj_chunk].bytes;
    for (auto &obj : doc.objects)
        if (Monstey_WName(doc.classes, Monstey_U32(bytes, obj.at)) == kind)
        {
            ++examples;
            for (auto &d : doc.db->records[Monstey_U32(bytes, obj.at + 10)].fields)
                native[d.hash].insert(d.type);
        }
    Monstey_Require(examples > 0, "Object class has no native schema example");
    J out = J::object();
    for (auto &[name, value] : properties.as_object())
    {
        (void)value;
        auto hash = name.starts_with("0x") ? Monstey_WUint(name) : Monstey_NativeHash(name);
        auto possible = native[hash];
        std::optional<unsigned> exact;
        if (record)
            for (auto &d : doc.db->records.at(*record).fields)
                if (d.hash == hash)
                    exact = d.type;
        if (explicit_types.contains(name))
        {
            auto type = Monstey_WUint(explicit_types.at(name));
            Monstey_Require((!exact || *exact == type) && (possible.empty() || possible.contains(type)),
                            "Explicit property type conflicts with native schema");
            out[name] = type;
        }
        else if (exact)
            out[name] = *exact;
        else
        {
            Monstey_Require(possible.size() == 1,
                            "Property type absent or ambiguous; provide an exact native property_types entry");
            out[name] = *possible.begin();
        }
    }
    return out;
}

J Monstey_WEditObject(Monstey_WObjres &doc, const J &entry, const std::string &operation)
{
    static const std::set<std::string> update_keys{
        "name",       "group_index",       "object_index",   "class_name",    "position", "quaternion",
        "properties", "remove_properties", "property_types", "schema_source", "rename_to"};
    static const std::set<std::string> clone_keys{
        "source_name", "name",       "source_group_index", "source_object_index", "group_index",  "position",
        "quaternion",  "properties", "remove_properties",  "property_types",      "schema_source"};
    static const std::set<std::string> add_keys{"class_name", "name",        "position",       "quaternion",
                                                "properties", "group_index", "property_types", "schema_source"};
    static const std::set<std::string> delete_keys{"name", "group_index", "object_index", "class_name"};
    const auto &allowed = operation == "add"      ? add_keys
                          : operation == "clone"  ? clone_keys
                          : operation == "delete" ? delete_keys
                                                  : update_keys;
    for (auto &[key, value] : entry.as_object())
    {
        (void)value;
        Monstey_Require(allowed.contains(key), "Unknown object operation argument");
    }
    auto groups = Monstey_WObjectGroups(doc);
    auto classes = doc.classes, names = doc.names;
    B properties = doc.chunks[doc.db_chunk].bytes;
    auto &bytes = doc.chunks[doc.obj_chunk].bytes;
    J proof =
        J::object({{"operation", operation}, {"other_property_ordinals_preserved", true}, {"runtime_verified", false}});
    B object;
    std::size_t target{};
    std::optional<std::uint32_t> source_record;
    std::string kind;
    if (operation == "add")
    {
        auto name = entry.at("name").as_string();
        Monstey_WNewIdentity(doc, name);
        kind = entry.at("class_name").as_string();
        Monstey_Require(!kind.empty() &&
                            std::all_of(kind.begin(), kind.end(), [](char c) { return c >= 32 && c < 127; }),
                        "Class name must be printable ASCII");
        target = entry.value("group_index", 0).as_uint();
        Monstey_Require(target < groups.size(), "Object target group outside table");
        object.resize(50);
        std::optional<std::uint32_t> class_offset;
        for (auto &obj : doc.objects)
            if (Monstey_WName(doc.classes, Monstey_U32(bytes, obj.at)) == kind)
            {
                class_offset = Monstey_U32(bytes, obj.at);
                break;
            }
        Monstey_PutU32(object, 0, class_offset ? *class_offset : Monstey_WAddString(classes, kind));
        Monstey_PutU32(object, 4, Monstey_WAddString(names, name));
        Monstey_PutU16(object, 8, 65535);
        proof["name"] = name;
    }
    else
    {
        auto selected = operation == "clone" ? Monstey_WSelectObject(doc, entry, "source_name", "source_group_index",
                                                                     "source_object_index")
                                             : Monstey_WSelectObject(doc, entry);
        target = selected.group;
        object = groups[selected.group].records[selected.index];
        kind = Monstey_WName(classes, Monstey_U32(object, 0));
        source_record = Monstey_U32(object, 10);
        if (operation == "delete")
        {
            Monstey_WQualifiedIdentity(doc, selected, false);
            groups[selected.group].records.erase(groups[selected.group].records.begin() + selected.index);
            doc.chunks[doc.obj_chunk].bytes = Monstey_WEncodeObjectGroups(classes, names, doc.extra, groups);
            doc = Monstey_WObjres(doc.Encode());
            proof["name"] = entry.at("name");
            proof["retained_property_index"] = *source_record;
            return proof;
        }
        if (operation == "clone")
        {
            Monstey_WQualifiedClone(doc, selected);
            auto name = entry.at("name").as_string();
            Monstey_WNewIdentity(doc, name);
            target = entry.value("group_index", selected.group).as_uint();
            Monstey_Require(target < groups.size(), "Clone target group outside table");
            Monstey_PutU32(object, 4, Monstey_WAddString(names, name));
            proof["source_name"] = entry.at("source_name");
            proof["name"] = name;
        }
        else if (entry.contains("rename_to"))
        {
            auto name = entry.at("rename_to").as_string();
            if (name != entry.at("name").as_string())
            {
                Monstey_WQualifiedIdentity(doc, selected, true);
                Monstey_WNewIdentity(doc, name);
                Monstey_PutU32(object, 4, Monstey_WAddString(names, name));
            }
            proof["source_name"] = entry.at("name");
            proof["name"] = name;
        }
        else
            proof["name"] = entry.at("name");
        proof["source_group_index"] = selected.group;
        proof["source_object_index"] = selected.index;
    }
    auto changed = entry.value("properties", J::object()), removed = entry.value("remove_properties", J::array());
    Monstey_Require(changed.is_object() && removed.is_array(),
                    "Object properties must be object and remove_properties array");
    bool cow = operation == "add" || !changed.empty() || !removed.empty();
    std::uint32_t property = source_record.value_or(0);
    if (cow)
    {
        std::unique_ptr<Monstey_WObjres> external;
        if (entry.contains("schema_source"))
            external = std::make_unique<Monstey_WObjres>(Monstey_ReadFile(Monstey_WPath(entry.at("schema_source"))));
        auto &schema = external ? *external : doc;
        auto types = Monstey_WPropertyTypes(schema, kind, changed, entry.value("property_types", J::object()),
                                            external ? std::optional<std::uint32_t>{} : source_record);
        auto appended = doc.db->Overlay(operation == "add" ? std::optional<std::uint32_t>{} : source_record, changed,
                                        types, removed);
        properties = std::move(appended.first);
        property = appended.second;
        Monstey_PutU32(object, 10, property);
    }
    if (entry.contains("position") || operation == "add")
    {
        auto position = Monstey_WVector(entry.at("position"), 3);
        for (std::size_t k = 0; k < 3; ++k)
            Monstey_WPutFloat(object, 14 + 4 * k, position[k]);
    }
    if (entry.contains("quaternion") || operation == "add")
    {
        auto q = Monstey_WVector(entry.at("quaternion"), 4);
        double norm{};
        for (auto f : q)
            norm += f * f;
        Monstey_Require(norm > 0, "Object quaternion cannot be zero");
        for (std::size_t k = 0; k < 4; ++k)
            Monstey_WPutFloat(object, 26 + 4 * k, q[k]);
    }
    if (operation == "update")
        groups[proof.at("source_group_index").as_uint()].records[proof.at("source_object_index").as_uint()] = object;
    else
        groups[target].records.push_back(object);
    doc.chunks[doc.db_chunk].bytes = std::move(properties);
    doc.chunks[doc.obj_chunk].bytes = Monstey_WEncodeObjectGroups(classes, names, doc.extra, groups);
    doc = Monstey_WObjres(doc.Encode());
    proof["copy_on_write"] = cow;
    proof["new_property_index"] = property;
    return proof;
}

J Monstey_WObjresEdit(Monstey_WObjres &doc, const J &spec)
{
    for (auto &[key, v] : spec.as_object())
    {
        (void)v;
        Monstey_Require(key == "source_sha256" || key == "updates" || key == "clones" || key == "deletes" ||
                            key == "objects",
                        "Unknown OBJRES transaction field");
    }
    if (spec.contains("source_sha256"))
        Monstey_Require(spec.at("source_sha256").as_string() == Monstey_HashSha256(doc.raw),
                        "OBJRES transaction source hash differs");
    J::Array changes;
    for (auto key : {"updates", "clones"})
        if (spec.contains(key))
            for (auto &entry : spec.at(key).as_array())
                changes.push_back(
                    Monstey_WEditObject(doc, entry, std::string_view(key) == "updates" ? "update" : "clone"));
    if (spec.contains("deletes"))
    {
        std::vector<std::pair<std::pair<std::uint32_t, std::uint32_t>, J>> deletes;
        std::set<std::pair<std::uint32_t, std::uint32_t>> seen;
        for (auto &entry : spec.at("deletes").as_array())
        {
            auto selected = Monstey_WSelectObject(doc, entry);
            auto identity = std::make_pair(selected.group, selected.index);
            Monstey_Require(seen.insert(identity).second, "Duplicate object deletion");
            auto stable = entry;
            stable["group_index"] = selected.group;
            stable["object_index"] = selected.index;
            deletes.emplace_back(identity, stable);
        }
        std::sort(deletes.begin(), deletes.end(), [](auto &x, auto &y) { return x.first > y.first; });
        for (auto &pair : deletes)
            changes.push_back(Monstey_WEditObject(doc, pair.second, "delete"));
    }
    if (spec.contains("objects"))
        for (auto &entry : spec.at("objects").as_array())
            changes.push_back(Monstey_WEditObject(doc, entry, "add"));
    return changes;
}

} // namespace Monstey_WorldPrivate
using namespace Monstey_WorldPrivate;

std::pair<Monstey_Bytes, Monstey_Json> Monstey_ObjresImportOwner(
    std::span<const std::uint8_t> target_raw, std::span<const std::uint8_t> donor_raw, std::string_view donor_name,
    std::string_view new_name, const Monstey_Json &position, const Monstey_Json &quaternion,
    const Monstey_Json &properties, std::uint32_t group_index)
{
    Monstey_WObjres target(B(target_raw.begin(), target_raw.end())), donor(B(donor_raw.begin(), donor_raw.end()));
    auto selected = Monstey_WSelectObject(donor, J::object({{"name", std::string(donor_name)}}));
    Monstey_WQualifiedClone(donor, selected);
    Monstey_WNewIdentity(target, std::string(new_name));
    auto groups = Monstey_WObjectGroups(target);
    Monstey_Require(group_index < groups.size(), "Imported owner target group is absent");
    auto &source_bytes = donor.chunks[donor.obj_chunk].bytes;
    auto root = Monstey_U32(source_bytes, selected.at + 10);
    auto imported = target.db->ImportClosure(*donor.db, root);
    auto root_ordinal = Monstey_WUint(imported.second.at("root_ordinal"));
    target.chunks[target.db_chunk].bytes = std::move(imported.first);
    target = Monstey_WObjres(target.Encode());
    auto classes = target.classes, names = target.names;
    auto kind = Monstey_WName(donor.classes, Monstey_U32(source_bytes, selected.at));
    std::optional<std::uint32_t> class_offset;
    auto &target_bytes = target.chunks[target.obj_chunk].bytes;
    for (auto &object : target.objects)
        if (Monstey_WName(classes, Monstey_U32(target_bytes, object.at)) == kind)
        {
            class_offset = Monstey_U32(target_bytes, object.at);
            break;
        }
    B object(source_bytes.begin() + static_cast<std::ptrdiff_t>(selected.at),
             source_bytes.begin() + static_cast<std::ptrdiff_t>(selected.at + 50));
    Monstey_PutU32(object, 0, class_offset ? *class_offset : Monstey_WAddString(classes, kind));
    Monstey_PutU32(object, 4, Monstey_WAddString(names, std::string(new_name)));
    Monstey_PutU16(object, 8, 65535); // Explicit full new identity has no donor prefix-pool offset.
    Monstey_PutU32(object, 10, root_ordinal);
    auto p = Monstey_WVector(position, 3), q = Monstey_WVector(quaternion, 4);
    double norm{};
    for (auto value : q)
        norm += value * value;
    Monstey_Require(norm > 0, "Imported owner quaternion cannot be zero");
    for (std::size_t axis = 0; axis < 3; ++axis)
        Monstey_WPutFloat(object, 14 + 4 * axis, p[axis]);
    for (std::size_t axis = 0; axis < 4; ++axis)
        Monstey_WPutFloat(object, 26 + 4 * axis, q[axis]);
    auto index = groups[group_index].records.size();
    groups[group_index].records.push_back(object);
    target.chunks[target.obj_chunk].bytes = Monstey_WEncodeObjectGroups(classes, names, target.extra, groups);
    target = Monstey_WObjres(target.Encode());
    Monstey_Require(properties.is_object(), "Imported owner properties must be an object");
    if (!properties.empty())
        Monstey_WObjresEdit(
            target, J::object({{"updates", J::array({J::object({{"name", std::string(new_name)},
                                                                {"group_index", group_index},
                                                                {"object_index", static_cast<std::uint64_t>(index)},
                                                                {"properties", properties}})})}}));
    auto output = target.Encode();
    auto owners = Monstey_ObjresOwners(output);
    Monstey_Require(owners.size() == Monstey_ObjresOwners(target_raw, false).size() + 1,
                    "Imported owner count readback differs");
    return {output, J::object({{"name", std::string(new_name)},
                               {"source_name", std::string(donor_name)},
                               {"class_name", kind},
                               {"group_index", group_index},
                               {"object_index", static_cast<std::uint64_t>(index)},
                               {"property_closure", imported.second},
                               {"source_sha256", Monstey_HashSha256(donor_raw)},
                               {"target_sha256", Monstey_HashSha256(target_raw)},
                               {"runtime_verified", false}})};
}

} // namespace Monstey
