#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

Monstey_WInst::Monstey_WInst(B input) : raw(std::move(input))
{
    Monstey_Require(raw.size() >= 76 && std::memcmp(raw.data(), "PRFB", 4) == 0 && Monstey_U32(raw, 4) == 8,
                    "Expected PRFB8");
    for (std::size_t j = 0; j < 14; ++j)
        header[j] = Monstey_U32(raw, 4 * j);
    std::size_t at = 56;
    auto take = [&](std::size_t n)
    {
        Monstey_CheckRange(raw, at, n);
        B b(raw.begin() + static_cast<std::ptrdiff_t>(at), raw.begin() + static_cast<std::ptrdiff_t>(at + n));
        at += n;
        return b;
    };
    auto u32 = [&]()
    {
        auto v = Monstey_U32(raw, at);
        at += 4;
        return v;
    };
    Monstey_Require(header[3] <= (raw.size() - at) / 44, "PRFB primary definitions exceed extent");
    for (std::uint32_t j = 0; j < header[3]; ++j)
        primary.push_back(take(44));
    Monstey_Require(header[7] <= (raw.size() - at) / 44, "PRFB secondary definitions exceed extent");
    for (std::uint32_t j = 0; j < header[7]; ++j)
        secondary.push_back(take(44));
    geometry_start = at;
    auto declared = u32(), vb = u32(), ib = u32();
    (void)take(vb);
    (void)take(ib);
    auto nl = u32();
    J::Array ls;
    std::uint64_t attributes{};
    Monstey_Require(nl <= (raw.size() - at) / 4, "PRFB layout count outside file");
    for (std::uint32_t j = 0; j < nl; ++j)
    {
        auto n = u32();
        Monstey_Require(n <= (raw.size() - at) / 4, "PRFB attributes outside file");
        J::Array elements;
        for (std::uint32_t k = 0; k < n; ++k)
        {
            auto bytes = take(4);
            elements.push_back(J::array({bytes[0], bytes[1], bytes[2], bytes[3]}));
        }
        attributes += n;
        ls.push_back(elements);
    }
    layouts = ls;
    auto nd = u32();
    Monstey_Require(nd == declared && nd == header[4] && nl == header[9] && attributes == header[10] &&
                        ((std::uint64_t(vb) + 3) & ~3ull) + ((std::uint64_t(ib) + 3) & ~3ull) == header[11],
                    "PRFB geometry/header counts differ");
    J::Array ds;
    Monstey_Require(nd <= (raw.size() - at) / 52, "PRFB submeshes outside file");
    for (std::uint32_t j = 0; j < nd; ++j)
    {
        J::Array row;
        auto b = take(52);
        for (std::size_t k = 0; k < 13; ++k)
            row.push_back(Monstey_U32(b, 4 * k));
        ds.push_back(row);
    }
    submeshes = ds;
    geometry_end = at;
    geometry.assign(raw.begin() + static_cast<std::ptrdiff_t>(geometry_start),
                    raw.begin() + static_cast<std::ptrdiff_t>(at));
    materials = take(header[8]);
    std::size_t mp{};
    J::Array mats;
    for (std::uint32_t j = 0; j < header[5]; ++j)
    {
        auto name = Monstey_WName(materials, static_cast<std::uint32_t>(mp));
        mats.push_back(name);
        auto end = std::find(materials.begin() + static_cast<std::ptrdiff_t>(mp), materials.end(), 0);
        auto stop = static_cast<std::size_t>(end - materials.begin());
        auto next = (stop + 4) & ~std::size_t(3);
        Monstey_Require(
            next <= materials.size() &&
                std::all_of(end, materials.begin() + static_cast<std::ptrdiff_t>(next), [](auto c) { return c == 0; }),
            "PRFB material padding invalid");
        mp = next;
    }
    Monstey_Require(mp == materials.size(), "PRFB material count differs");
    material_names = mats;
    instance_start = at;
    Monstey_Require(80ull * header[6] + header[13] == raw.size() - at, "PRFB instance/name extents disagree");
    for (std::uint32_t j = 0; j < header[6]; ++j)
        instances.push_back(take(80));
    names_blob = take(header[13]);
    std::size_t np{};
    for (std::uint32_t j = 0; j < header[12]; ++j)
    {
        auto name = Monstey_WName(names_blob, static_cast<std::uint32_t>(np));
        Monstey_Require(!name.empty(), "Empty PRFB prefab name");
        np += name.size() + 1;
        std::replace(name.begin(), name.end(), '\\', '/');
        names.push_back(Monstey_WLower(name));
        name_bytes.emplace_back(names_blob.begin() + static_cast<std::ptrdiff_t>(np - name.size() - 1),
                                names_blob.begin() + static_cast<std::ptrdiff_t>(np - 1));
    }
    Monstey_Require(np <= names_blob.size() && std::all_of(names_blob.begin() + static_cast<std::ptrdiff_t>(np),
                                                           names_blob.end(), [](auto c) { return c == 0; }),
                    "PRFB names trailing bytes");
    Validate();
    original_names = names;
    Monstey_Require(Encode() == raw, "PRFB identity reconstruction differs");
}

void Monstey_WInst::Validate() const
{
    for (auto &i : instances)
    {
        Monstey_Require(Monstey_U32(i, 0) < names.size(), "PRFB prefab index outside names");
        (void)Monstey_WFloats(i, 12, 15);
        auto lo = Monstey_WFloats(i, 16, 3), hi = Monstey_WFloats(i, 28, 3);
        for (std::size_t j = 0; j < 3; ++j)
            Monstey_Require(lo[j].as_number() <= hi[j].as_number(), "PRFB inverted native culling AABB");
        (void)Monstey_WRotation(Monstey_WVector(Monstey_WFloats(i, 52, 4), 4));
    }
    auto check = [&](const B &d)
    {
        auto first = Monstey_U32(d, 32), n = Monstey_U32(d, 36);
        Monstey_Require(first <= 65535 && n <= 65535 && first <= instances.size() && n <= instances.size() - first,
                        "PRFB owner range outside instances");
    };
    for (auto &d : primary)
        check(d);
    for (auto &d : secondary)
        check(d);
}

B Monstey_WInst::Encode() const
{
    auto h = header;
    h[3] = static_cast<std::uint32_t>(primary.size());
    h[7] = static_cast<std::uint32_t>(secondary.size());
    h[6] = static_cast<std::uint32_t>(instances.size());
    auto encoded_names = names_blob;
    if (names != original_names)
    {
        encoded_names.clear();
        for (std::size_t j = 0; j < names.size(); ++j)
        {
            if (j < original_names.size() && names[j] == original_names[j])
                encoded_names.insert(encoded_names.end(), name_bytes[j].begin(), name_bytes[j].end());
            else
            {
                auto name = Monstey_ArchiveVirtual(names[j]);
                std::replace(name.begin(), name.end(), '/', '\\');
                encoded_names.insert(encoded_names.end(), name.begin(), name.end());
            }
            encoded_names.push_back(0);
        }
        while (encoded_names.size() % 8)
            encoded_names.push_back(0);
    }
    h[12] = static_cast<std::uint32_t>(names.size());
    h[13] = static_cast<std::uint32_t>(encoded_names.size());
    B out;
    for (auto w : h)
        Monstey_AppendU32(out, w);
    for (auto &d : primary)
        out.insert(out.end(), d.begin(), d.end());
    for (auto &d : secondary)
        out.insert(out.end(), d.begin(), d.end());
    out.insert(out.end(), geometry.begin(), geometry.end());
    out.insert(out.end(), materials.begin(), materials.end());
    for (auto &i : instances)
        out.insert(out.end(), i.begin(), i.end());
    out.insert(out.end(), encoded_names.begin(), encoded_names.end());
    return out;
}

J Monstey_WInst::Owners(std::size_t index) const
{
    J::Array owners;
    auto scan = [&](const std::vector<B> &definitions, const char *kind)
    {
        for (std::size_t j = 0; j < definitions.size(); ++j)
        {
            auto first = Monstey_U32(definitions[j], 32), n = Monstey_U32(definitions[j], 36);
            if (index >= first && index - first < n)
                owners.push_back(J::object({{"kind", kind},
                                            {"definition_index", static_cast<std::uint64_t>(j)},
                                            {"definition_key", Monstey_Hex(Monstey_U32(definitions[j], 0))}}));
        }
    };
    scan(primary, "primary");
    scan(secondary, "secondary");
    return owners;
}

J Monstey_WInst::Report(std::uint64_t limit, bool all) const
{
    Monstey_Require(all || limit <= 500, "Instance page exceeds 500");
    J::Array ins, definitions;
    std::size_t active{};
    for (std::size_t j = 0; j < instances.size(); ++j)
    {
        auto owners = Owners(j);
        if (!owners.empty())
            ++active;
        if (j >= limit)
            continue;
        auto &i = instances[j];
        ins.push_back(J::object({{"index", static_cast<std::uint64_t>(j)},
                                 {"owned", !owners.empty()},
                                 {"owners", owners},
                                 {"prefab_path", names[Monstey_U32(i, 0)]},
                                 {"definition_key", Monstey_Hex(Monstey_U32(i, 4))},
                                 {"word8_raw", Monstey_U32(i, 8)},
                                 {"lod_distance", static_cast<double>(Monstey_WReadFloat(i, 12))},
                                 {"bounds_min", Monstey_WFloats(i, 16, 3)},
                                 {"bounds_max", Monstey_WFloats(i, 28, 3)},
                                 {"position", Monstey_WFloats(i, 40, 3)},
                                 {"quaternion", Monstey_WFloats(i, 52, 4)},
                                 {"scale", static_cast<double>(Monstey_WReadFloat(i, 68))}}));
    }
    for (std::size_t j = 0; j < primary.size(); ++j)
        definitions.push_back(J::object(
            {{"index", static_cast<std::uint64_t>(j)},
             {"definition_key", Monstey_Hex(Monstey_U32(primary[j], 0))},
             {"nested_instance_range", J::array({Monstey_U32(primary[j], 32), Monstey_U32(primary[j], 36)})}}));
    return J::object({{"format", "PRFB8"},
                      {"source_bytes", static_cast<std::uint64_t>(raw.size())},
                      {"source_sha256", Monstey_HashSha256(raw)},
                      {"primary_definition_count", static_cast<std::uint64_t>(primary.size())},
                      {"secondary_definition_count", static_cast<std::uint64_t>(secondary.size())},
                      {"primary_definitions", definitions},
                      {"geometry", J::object({{"bytes", static_cast<std::uint64_t>(geometry.size())},
                                              {"sha256", Monstey_HashSha256(geometry)},
                                              {"layouts", layouts},
                                              {"submeshes", submeshes}})},
                      {"materials", material_names},
                      {"instances", ins},
                      {"instances_total", static_cast<std::uint64_t>(instances.size())},
                      {"instances_truncated", instances.size() > limit},
                      {"effective_instance_count", static_cast<std::uint64_t>(active)},
                      {"runtime_verified", false}});
}

B Monstey_WInst::Transform(const B &original, const J &update) const
{
    auto result = original;
    auto oldp = Monstey_WVector(Monstey_WFloats(original, 40, 3), 3),
         oldq = Monstey_WVector(Monstey_WFloats(original, 52, 4), 4);
    auto oldscale = Monstey_WReadFloat(original, 68);
    auto pos = Monstey_WVector(update.value("position", Monstey_WFloats(original, 40, 3)), 3),
         q = Monstey_WVector(update.value("quaternion", Monstey_WFloats(original, 52, 4)), 4);
    auto scale = Monstey_WFloat(update.value("scale", static_cast<double>(oldscale)));
    Monstey_Require(std::abs(scale) > 1e-12 && std::abs(oldscale) > 1e-12, "Cannot remap zero-scale instance");
    auto oldrot = Monstey_WRotation(oldq), newrot = Monstey_WRotation(q);
    std::array<std::array<double, 3>, 3> matrix{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j)
            for (std::size_t k = 0; k < 3; ++k)
                matrix[i][j] += newrot[i][k] * oldrot[j][k] * scale / oldscale;
    std::array<double, 3> lo, hi;
    lo.fill(std::numeric_limits<double>::infinity());
    hi.fill(-std::numeric_limits<double>::infinity());
    for (unsigned corner = 0; corner < 8; ++corner)
    {
        std::array<double, 3> xyz;
        for (std::size_t j = 0; j < 3; ++j)
            xyz[j] = Monstey_WReadFloat(original, ((corner >> j) & 1 ? 28 : 16) + 4 * j);
        for (std::size_t i = 0; i < 3; ++i)
        {
            double v = pos[i];
            for (std::size_t j = 0; j < 3; ++j)
                v += matrix[i][j] * (xyz[j] - oldp[j]);
            lo[i] = std::min(lo[i], v);
            hi[i] = std::max(hi[i], v);
        }
    }
    for (std::size_t i = 0; i < 3; ++i)
    {
        float lower = static_cast<float>(lo[i]), upper = static_cast<float>(hi[i]);
        if (lower > lo[i])
            lower = std::nextafter(lower, -std::numeric_limits<float>::infinity());
        if (upper < hi[i])
            upper = std::nextafter(upper, std::numeric_limits<float>::infinity());
        Monstey_WPutFloat(result, 16 + 4 * i, lower);
        Monstey_WPutFloat(result, 28 + 4 * i, upper);
        Monstey_WPutFloat(result, 40 + 4 * i, pos[i]);
    }
    for (std::size_t i = 0; i < 4; ++i)
        Monstey_WPutFloat(result, 52 + 4 * i, q[i]);
    Monstey_WPutFloat(result, 68, scale);
    return result;
}

Monstey_WInst Monstey_WInst::Donor(const J &description)
{
    auto source = Monstey_WPath(description.at("source"));
    auto raw = Monstey_ReadFile(source);
    auto sha = Monstey_HashSha256(raw);
    if (description.contains("sha256"))
        Monstey_Require(description.at("sha256").as_string() == sha, "PRFB donor checksum differs");
    Monstey_Require(Monstey_HashSha256(source) == sha, "PRFB donor changed during read");
    return Monstey_WInst(std::move(raw));
}

J Monstey_WInst::AddOwner(const J &description)
{
    auto donor = Donor(description);
    auto index = description.value("definition_index", 0).as_uint();
    Monstey_Require(index < donor.primary.size(), "Donor owner definition outside table");
    auto definition = donor.primary[static_cast<std::size_t>(index)];
    auto key = Monstey_U32(definition, 0);
    Monstey_Require(Monstey_U32(definition, 8) == 0 && Monstey_U32(definition, 12) == 0 &&
                        Monstey_U32(definition, 16) == 0xffffffffu && key != 0xffffffffu,
                    "Donor owner carries unqualified local geometry or LOD dependencies");
    for (auto &d : primary)
        Monstey_Require(Monstey_U32(d, 0) != key, "Owner definition key already present");
    Monstey_PutU32(definition, 32, 0);
    Monstey_PutU32(definition, 36, 0);
    primary.push_back(std::move(definition));
    return J::object({{"operation", "add_owner_definition"},
                      {"definition_key", Monstey_Hex(key)},
                      {"donor_sha256", Monstey_HashSha256(donor.raw)},
                      {"nested_instance_range", J::array({0, 0})},
                      {"mesh_or_LOD_references_inherited", false},
                      {"world_WDSC_binding_required", true}});
}

J Monstey_WInst::AddPlacement(B record, const std::string &input_path, const J &entry)
{
    auto path = Monstey_ArchiveVirtual(input_path);
    Monstey_Require(path.ends_with(".inst") && std::all_of(path.begin(), path.end(),
                                                           [](char c) { return static_cast<unsigned char>(c) < 128; }),
                    "Expected ASCII .inst prefab path");
    std::size_t owner = primary.size();
    if (entry.contains("owner_definition_key"))
    {
        auto key = Monstey_WUint(entry.at("owner_definition_key"));
        for (std::size_t j = 0; j < primary.size(); ++j)
            if (Monstey_U32(primary[j], 0) == key)
            {
                Monstey_Require(owner == primary.size(), "Ambiguous render owner");
                owner = j;
            }
    }
    else
    {
        Monstey_Require(primary.size() == 1, "Explicit render owner required");
        owner = 0;
    }
    Monstey_Require(owner < primary.size(), "Render owner not found");
    auto first = Monstey_U32(primary[owner], 32), count = Monstey_U32(primary[owner], 36);
    Monstey_Require(first <= instances.size() && count <= instances.size() - first && count < 65535,
                    "Owner nested range invalid");
    Monstey_Require(entry.contains("position") && entry.contains("quaternion"),
                    "Placement requires explicit position and quaternion");
    record = Transform(record, entry);
    auto name = std::find(names.begin(), names.end(), path);
    std::uint32_t name_index;
    if (name == names.end())
    {
        name_index = static_cast<std::uint32_t>(names.size());
        names.push_back(path);
    }
    else
        name_index = static_cast<std::uint32_t>(name - names.begin());
    Monstey_PutU32(record, 0, name_index);
    auto old_first = first;
    bool relocated = count && std::size_t(first) + count != instances.size();
    Monstey_Require(instances.size() + (relocated ? count : 0) + 1 <= 65535,
                    "Owner additions exceed native uint16 capacity");
    if (relocated)
    {
        std::vector<B> copies(instances.begin() + first, instances.begin() + first + count);
        first = static_cast<std::uint32_t>(instances.size());
        instances.insert(instances.end(), copies.begin(), copies.end());
    }
    else if (!count)
        first = static_cast<std::uint32_t>(instances.size());
    auto index = instances.size();
    instances.push_back(std::move(record));
    Monstey_PutU32(primary[owner], 32, first);
    Monstey_PutU32(primary[owner], 36, count + 1);
    Validate();
    return J::object({{"operation", "add_instance"},
                      {"index", static_cast<std::uint64_t>(index)},
                      {"prefab_path", path},
                      {"definition_key", Monstey_Hex(Monstey_U32(instances.back(), 4))},
                      {"owner_definition_key", Monstey_Hex(Monstey_U32(primary[owner], 0))},
                      {"owner_range_before", J::array({old_first, count})},
                      {"owner_range_after", J::array({first, count + 1})},
                      {"previous_range_copied_to_tail", relocated},
                      {"old_instance_records_byte_exact", true},
                      {"geometry_and_material_prefix_byte_exact", true},
                      {"world_WDSC_binding_required", true}});
}

J Monstey_WInst::AddInstance(const J &entry)
{
    for (auto &[key, value] : entry.as_object())
    {
        (void)value;
        Monstey_Require(key == "prefab_path" || key == "position" || key == "quaternion" || key == "scale" ||
                            key == "donor" || key == "owner_definition_key",
                        "Unknown donor-backed instance argument");
    }
    auto path = Monstey_ArchiveVirtual(entry.at("prefab_path").as_string());
    std::optional<Monstey_WInst> external;
    if (entry.contains("donor"))
        external.emplace(Donor(entry.at("donor")));
    const auto &donor = external ? *external : *this;
    std::size_t selected = donor.instances.size();
    if (entry.contains("donor"))
        selected = static_cast<std::size_t>(entry.at("donor").at("index").as_uint());
    else
        for (std::size_t j = 0; j < donor.instances.size(); ++j)
            if (donor.names[Monstey_U32(donor.instances[j], 0)] == path)
            {
                selected = j;
                break;
            }
    Monstey_Require(selected < donor.instances.size() && donor.names[Monstey_U32(donor.instances[selected], 0)] == path,
                    "Donor prefab path or instance index differs");
    auto record = donor.instances[selected];
    auto sha = Monstey_HashSha256(donor.raw);
    auto result = AddPlacement(std::move(record), path, entry);
    result["donor_sha256"] = sha;
    result["donor_index"] = static_cast<std::uint64_t>(selected);
    result["definition_key_LOD_flags_preserved"] = true;
    return result;
}

J Monstey_WInst::Apply(const J &spec)
{
    Monstey_Require(spec.is_object(), "PRFB spec must be object");
    for (auto &[key, value] : spec.as_object())
    {
        (void)value;
        Monstey_Require(key == "source_sha256" || key == "updates" || key == "clones" || key == "deletes" ||
                            key == "resource_path" || key == "owner_donor" || key == "instances" ||
                            key == "defined_instances",
                        "Unknown PRFB authoring field");
    }
    if (spec.contains("source_sha256"))
        Monstey_Require(spec.at("source_sha256").as_string() == Monstey_HashSha256(raw), "PRFB source hash differs");
    J::Array changes;
    if (spec.contains("owner_donor"))
        changes.push_back(AddOwner(spec.at("owner_donor")));
    std::map<std::pair<std::uint32_t, std::size_t>, std::optional<std::size_t>> remap;
    auto operation = [&](const J &input, const std::string &op)
    {
        for (auto &[key, value] : input.as_object())
        {
            (void)value;
            Monstey_Require(key == "index" || key == "owner_definition_key" ||
                                (op != "delete" && (key == "position" || key == "quaternion" || key == "scale")),
                            "Unknown instance operation argument");
        }
        auto requested = input.at("index").as_uint();
        Monstey_Require(requested < instances.size(), "PRFB instance outside table");
        std::optional<std::uint32_t> owner;
        if (input.contains("owner_definition_key"))
            owner = Monstey_WUint(input.at("owner_definition_key"));
        if (op != "update" && !owner)
        {
            auto owners = Owners(static_cast<std::size_t>(requested));
            for (auto &o : owners.as_array())
                if (o.at("kind").as_string() == "primary")
                {
                    Monstey_Require(!owner, "Ambiguous primary owner");
                    owner = Monstey_WUint(o.at("definition_key"));
                }
            Monstey_Require(owner.has_value(), "Clone/delete needs a primary owner");
        }
        auto index = static_cast<std::size_t>(requested);
        if (owner)
        {
            auto m = remap.find({*owner, index});
            if (m != remap.end())
            {
                Monstey_Require(m->second.has_value(), "Instance already removed from selected owner");
                index = *m->second;
            }
        }
        Monstey_Require(index < instances.size(), "Remapped instance outside table");
        auto changed = op == "delete" ? B{} : Transform(instances[index], input);
        if (!owner)
        {
            instances[index] = std::move(changed);
            changes.push_back(J::object({{"operation", op},
                                         {"index", static_cast<std::uint64_t>(index)},
                                         {"owner_scope", "global_instance_record"}}));
            return;
        }
        std::size_t definition = primary.size();
        for (std::size_t j = 0; j < primary.size(); ++j)
            if (Monstey_U32(primary[j], 0) == *owner)
            {
                Monstey_Require(definition == primary.size(), "Ambiguous owner definition key");
                definition = j;
            }
        Monstey_Require(definition < primary.size(), "Primary owner not found");
        auto first = Monstey_U32(primary[definition], 32), n = Monstey_U32(primary[definition], 36);
        Monstey_Require(op == "clone" || (index >= first && index - first < n), "Instance outside selected owner");
        std::vector<std::pair<std::size_t, B>> copies;
        for (std::size_t j = first; j < static_cast<std::size_t>(first) + n; ++j)
            if (op != "delete" || j != index)
                copies.emplace_back(j, op == "update" && j == index ? changed : instances[j]);
        if (op == "clone")
            copies.emplace_back(std::numeric_limits<std::size_t>::max(), changed);
        Monstey_Require(instances.size() + copies.size() <= 65535, "Owner copy-on-write range exceeds uint16");
        auto start = instances.size();
        std::map<std::size_t, std::size_t> mapping;
        for (auto &[old, bytes] : copies)
        {
            if (old != std::numeric_limits<std::size_t>::max())
                mapping.emplace(old, instances.size());
            instances.push_back(std::move(bytes));
        }
        Monstey_PutU32(primary[definition], 32, copies.empty() ? 0 : static_cast<std::uint32_t>(start));
        Monstey_PutU32(primary[definition], 36, static_cast<std::uint32_t>(copies.size()));
        for (auto &[identity, current] : remap)
            if (identity.first == *owner && current)
            {
                auto found = mapping.find(*current);
                if (found != mapping.end())
                    current = found->second;
                else if (op == "delete" && *current == index)
                    current.reset();
            }
        for (auto &[old, next] : mapping)
            remap.try_emplace(std::make_pair(*owner, old), next);
        if (op == "delete")
            remap[{*owner, index}] = std::nullopt;
        changes.push_back(
            J::object({{"operation", op},
                       {"owner_definition_key", Monstey_Hex(*owner)},
                       {"owner_range_before", J::array({first, n})},
                       {"owner_range_after", J::array({static_cast<std::uint64_t>(copies.empty() ? 0 : start),
                                                       static_cast<std::uint64_t>(copies.size())})},
                       {"old_instance_slots_preserved", true},
                       {"world_WDSC_binding_required", true}}));
    };
    for (auto name : {"updates", "clones", "deletes"})
        if (spec.contains(name))
            for (auto &row : spec.at(name).as_array())
                operation(row, std::string(name) == "updates"  ? "update"
                               : std::string(name) == "clones" ? "clone"
                                                               : "delete");
    if (spec.contains("instances"))
        for (auto &entry : spec.at("instances").as_array())
            changes.push_back(AddInstance(entry));
    if (spec.contains("defined_instances"))
        for (auto &entry : spec.at("defined_instances").as_array())
            changes.push_back(Monstey_WAddDefined(*this, entry));
    if (spec.contains("resource_path"))
    {
        auto path = Monstey_ArchiveVirtual(spec.at("resource_path").as_string());
        Monstey_Require(path.ends_with(".inst"), "Expected .inst resource path");
        header[2] = Monstey_PathHash(path);
    }
    Validate();
    return changes;
}

J Monstey_WAddDefined(Monstey_WInst &target, const J &entry)
{
    for (auto &[key, value] : entry.as_object())
    {
        (void)value;
        Monstey_Require(key == "target_resource_path" || key == "position" || key == "quaternion" || key == "scale" ||
                            key == "target" || key == "placement_donor" || key == "target_definition_key" ||
                            key == "owner_definition_key",
                        "Unknown defined-prefab placement argument");
    }
    auto prefab = Monstey_WInst::Donor(entry.at("target"));
    auto placement = Monstey_WInst::Donor(entry.at("placement_donor"));
    auto path = Monstey_ArchiveVirtual(entry.at("target_resource_path").as_string());
    Monstey_Require(path.ends_with(".inst") && prefab.header[2] == Monstey_PathHash(path),
                    "Defined target path does not match native PRFB resource identity");
    std::size_t selected = prefab.primary.size();
    if (entry.contains("target_definition_key"))
    {
        auto key = Monstey_WUint(entry.at("target_definition_key"));
        for (std::size_t j = 0; j < prefab.primary.size(); ++j)
            if (Monstey_U32(prefab.primary[j], 0) == key)
            {
                Monstey_Require(selected == prefab.primary.size(), "Ambiguous target definition key");
                selected = j;
            }
    }
    else
    {
        Monstey_Require(prefab.primary.size() == 1, "Explicit target definition key required");
        selected = 0;
    }
    Monstey_Require(selected < prefab.primary.size(), "Target native definition absent");
    auto definition = prefab.primary[selected];
    auto target_key = Monstey_U32(definition, 0);
    B mesh{'M', 'E', 'S', 'H'};
    for (auto word : {5u, 1u, prefab.header[4], prefab.header[9], prefab.header[10], prefab.header[11], 0u})
        Monstey_AppendU32(mesh, word);
    mesh.insert(mesh.end(), prefab.geometry.begin(), prefab.geometry.end());
    Monstey_WMesh geometry(std::move(mesh));
    std::vector<std::size_t> chosen;
    std::set<std::uint32_t> visited;
    for (;;)
    {
        Monstey_Require(Monstey_U32(definition, 32) == 0 && Monstey_U32(definition, 36) == 0,
                        "Defined target has nested instances; qualified static leaf required");
        auto first = Monstey_U32(definition, 8) & 65535u, count = Monstey_U32(definition, 12) & 65535u;
        Monstey_Require(first <= geometry.groups[0].draws.size() && count <= geometry.groups[0].draws.size() - first,
                        "Target static draw range invalid");
        for (std::size_t j = first; j < std::size_t(first) + count; ++j)
            chosen.push_back(j);
        auto link = Monstey_U32(definition, 16);
        if (link == 0xffffffffu)
            break;
        Monstey_Require(link < prefab.secondary.size() && visited.insert(link).second,
                        "Target static LOD chain invalid or cyclic");
        definition = prefab.secondary[link];
    }
    Monstey_Require(!chosen.empty(), "Target native definition contains no static geometry");
    std::array<double, 3> lo, hi;
    lo.fill(std::numeric_limits<double>::infinity());
    hi.fill(-std::numeric_limits<double>::infinity());
    for (auto draw : chosen)
    {
        auto attributes = geometry.Attributes(0, draw);
        auto position = attributes.find("POSITION");
        Monstey_Require(position != attributes.end() && position->second.width >= 3,
                        "Target native geometry has no 3D position channel");
        for (std::size_t row = 0; row < position->second.count; ++row)
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                auto value = geometry.Component(position->second, row, axis);
                Monstey_Require(std::isfinite(value), "Target native position is nonfinite");
                lo[axis] = std::min(lo[axis], value);
                hi[axis] = std::max(hi[axis], value);
            }
    }
    auto donor_index = entry.at("placement_donor").value("index", 1).as_uint();
    Monstey_Require(donor_index < placement.instances.size(), "Native placement donor index outside table");
    auto record = placement.instances[static_cast<std::size_t>(donor_index)];
    Monstey_PutU32(record, 0, 0);
    Monstey_PutU32(record, 4, target_key);
    J::Array lower, upper;
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        Monstey_WPutFloat(record, 16 + 4 * axis, static_cast<float>(lo[axis]));
        Monstey_WPutFloat(record, 28 + 4 * axis, static_cast<float>(hi[axis]));
        Monstey_WPutFloat(record, 40 + 4 * axis, 0);
        lower.push_back(lo[axis]);
        upper.push_back(hi[axis]);
    }
    for (std::size_t axis = 0; axis < 4; ++axis)
        Monstey_WPutFloat(record, 52 + 4 * axis, axis == 3 ? 1.f : 0.f);
    Monstey_WPutFloat(record, 68, 1);
    auto proof = target.AddPlacement(std::move(record), path, entry);
    proof["operation"] = "add_defined_prefab";
    proof["target_definition_key"] = Monstey_Hex(target_key);
    proof["target_prefab_sha256"] = Monstey_HashSha256(prefab.raw);
    proof["placement_donor_sha256"] = Monstey_HashSha256(placement.raw);
    proof["local_bounds_from_target_vertices"] = J::array({lower, upper});
    proof["native_donor_bounds_not_used"] = true;
    proof["native_LOD_distance_and_flags_only"] = true;
    return proof;
}

std::pair<B, J> Monstey_WBindingEncode(const B &inst_raw, const B &donor, std::string resource, std::uint32_t owner)
{
    Monstey_WInst inst(inst_raw);
    resource = Monstey_WLower(resource);
    std::replace(resource.begin(), resource.end(), '\\', '/');
    Monstey_Require(resource.ends_with(".inst") && resource.find('\0') == std::string::npos,
                    "Expected .inst resource path");
    Monstey_Require(inst.header[2] == Monstey_PathHash(resource), "PRFB resource hash does not match virtual path");
    Monstey_Require(donor.size() >= 44 && std::memcmp(donor.data(), "WDSC", 4) == 0 && Monstey_U32(donor, 4) == 9,
                    "Expected native WDSC9 donor");
    constexpr std::uint32_t prefix[] = {1, 0, 1, 1, 0, 0, 0, 0};
    for (std::size_t j = 0; j < 8; ++j)
        Monstey_Require(Monstey_U32(donor, 8 + 4 * j) == prefix[j], "Donor is not the qualified single-binding graph");
    auto strings = Monstey_U32(donor, 40);
    auto at = 44ull + strings;
    Monstey_Require(at + 76 + 24 + 52 == donor.size(), "WDSC single binding extent differs");
    constexpr std::uint32_t group[] = {0, 0, 1, 0, 1, 0};
    for (std::size_t j = 0; j < 6; ++j)
        Monstey_Require(Monstey_U32(donor, at + 76 + 4 * j) == group[j], "WDSC donor owner group differs");
    for (std::size_t j = 0; j < 13; ++j)
        Monstey_Require(Monstey_U32(donor, at + 100 + 4 * j) == 0, "WDSC donor scene is not empty");
    std::size_t selected = inst.primary.size();
    for (std::size_t j = 0; j < inst.primary.size(); ++j)
        if (Monstey_U32(inst.primary[j], 0) == owner)
        {
            Monstey_Require(selected == inst.primary.size(), "Ambiguous PRFB owner");
            selected = j;
        }
    Monstey_Require(selected < inst.primary.size(), "PRFB primary owner not found");
    auto first = Monstey_U32(inst.primary[selected], 32), count = Monstey_U32(inst.primary[selected], 36);
    Monstey_Require(count > 0, "PRFB owner has no leaves");
    std::array<float, 3> lo, hi;
    lo.fill(std::numeric_limits<float>::infinity());
    hi.fill(-std::numeric_limits<float>::infinity());
    for (std::size_t j = first; j < std::size_t(first) + count; ++j)
        for (std::size_t k = 0; k < 3; ++k)
        {
            lo[k] = std::min(lo[k], Monstey_WReadFloat(inst.instances[j], 16 + 4 * k));
            hi[k] = std::max(hi[k], Monstey_WReadFloat(inst.instances[j], 28 + 4 * k));
        }
    B names;
    auto native = resource;
    std::replace(native.begin(), native.end(), '/', '\\');
    Monstey_WAddString(names, native);
    while (names.size() % 4)
        names.push_back(0);
    B output{'W', 'D', 'S', 'C'};
    Monstey_AppendU32(output, 9);
    for (auto word : prefix)
        Monstey_AppendU32(output, word);
    Monstey_AppendU32(output, static_cast<std::uint32_t>(names.size()));
    output.insert(output.end(), names.begin(), names.end());
    for (auto value : lo)
        Monstey_AppendU32(output, std::bit_cast<std::uint32_t>(value));
    for (auto value : hi)
        Monstey_AppendU32(output, std::bit_cast<std::uint32_t>(value));
    for (auto value : {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f})
        Monstey_AppendU32(output, std::bit_cast<std::uint32_t>(value));
    Monstey_AppendU32(output, Monstey_U32(donor, at + 56));
    Monstey_AppendU32(output, Monstey_U32(donor, at + 60));
    Monstey_AppendU32(output, owner);
    Monstey_AppendU32(output, Monstey_U32(donor, at + 68));
    Monstey_AppendU32(output, 0);
    for (auto word : group)
        Monstey_AppendU32(output, word);
    output.resize(output.size() + 52);
    (void)Monstey_WSection(output, "WDSC9");
    auto result = J::object({{"status", "NATIVE_WDSC9_SINGLE_PREFAB_BINDING_VALIDATED"},
                             {"resource", resource},
                             {"owner_key", owner},
                             {"owner_primary_index", static_cast<std::uint64_t>(selected)},
                             {"owned_instance_range", J::array({first, count})},
                             {"identity_transform", true},
                             {"exact_leaf_AABB_union", true},
                             {"collision_generated", false},
                             {"navmesh_generated", false}});
    return {output, result};
}

J Monstey_WBinding(const J &a)
{
    auto inst_path = Monstey_WPath(a.at("inst")), donor_path = Monstey_WPath(a.at("donor"));
    auto inst = Monstey_ReadFile(inst_path), donor = Monstey_ReadFile(donor_path);
    auto output = Monstey_WBindingEncode(inst, donor, a.at("resource").as_string(), Monstey_WUint(a.at("owner_key")));
    Monstey_Require(Monstey_HashSha256(donor_path) == Monstey_HashSha256(donor), "Donor changed during binding build");
    return Monstey_WPublish(inst_path, inst, a, output.first, output.second);
}

} // namespace Monstey_WorldPrivate
using namespace Monstey_WorldPrivate;

Monstey_InstGeometryData Monstey_InstReadGeometry(std::span<const std::uint8_t> raw)
{
    Monstey_WInst doc(B(raw.begin(), raw.end()));
    B encoded{'M', 'E', 'S', 'H'};
    for (auto word : {5u, 1u, doc.header[4], doc.header[9], doc.header[10], doc.header[11], 0u})
        Monstey_AppendU32(encoded, word);
    encoded.insert(encoded.end(), doc.geometry.begin(), doc.geometry.end());
    Monstey_WMesh mesh(std::move(encoded));
    auto &group = mesh.groups[0];
    Monstey_InstGeometryData data;
    data.header = doc.header;
    data.primary = doc.primary;
    data.secondary = doc.secondary;
    data.vertex_buffer.assign(mesh.raw.begin() + static_cast<std::ptrdiff_t>(group.vertex),
                              mesh.raw.begin() + static_cast<std::ptrdiff_t>(group.vertex + group.vertex_bytes));
    data.index_buffer.assign(mesh.raw.begin() + static_cast<std::ptrdiff_t>(group.index),
                             mesh.raw.begin() + static_cast<std::ptrdiff_t>(group.index + group.index_bytes));
    data.layouts = group.layouts;
    data.draws = group.draws;
    for (auto &name : doc.material_names.as_array())
        data.material_names.push_back(name.as_string());
    for (auto &draw : data.draws)
        Monstey_Require((draw[6] & 65535) < data.material_names.size(),
                        "PRFB draw material slot is outside native table");
    return data;
}

Monstey_Json Monstey_InstSceneInstances(std::span<const std::uint8_t> raw)
{
    Monstey_WInst doc(B(raw.begin(), raw.end()));
    return doc.Report(doc.instances.size(), true);
}

Monstey_Json Monstey_InstDecodeGeometry(std::span<const std::uint8_t> raw)
{
    Monstey_WInst doc(B(raw.begin(), raw.end()));
    B encoded{'M', 'E', 'S', 'H'};
    for (auto word : {5u, 1u, doc.header[4], doc.header[9], doc.header[10], doc.header[11], 0u})
        Monstey_AppendU32(encoded, word);
    encoded.insert(encoded.end(), doc.geometry.begin(), doc.geometry.end());
    Monstey_WMesh mesh(std::move(encoded));
    J::Array parts;
    for (std::size_t di = 0; di < mesh.groups[0].draws.size(); ++di)
    {
        J::Object channels;
        auto attributes = mesh.Attributes(0, di);
        for (auto &[name, attribute] : attributes)
        {
            Monstey_Require(attribute.semantic <= 5, "PRFB rigid geometry has an unqualified non-rigid semantic");
            auto width = attribute.semantic == 0 ? std::size_t(3) : std::size_t(attribute.width);
            Monstey_Require(attribute.width >= width, "Native rigid position has fewer than3 coordinates");
            J::Array rows;
            for (std::size_t vertex = 0; vertex < attribute.count; ++vertex)
            {
                J::Array values;
                for (std::size_t axis = 0; axis < width; ++axis)
                {
                    auto value = mesh.Component(attribute, vertex, axis);
                    Monstey_Require(std::isfinite(value), "Nonfinite native rigid geometry component");
                    if (attribute.semantic == 4 && attribute.storage == 9)
                        value /= 32767.0;
                    if (attribute.storage == 4 || (attribute.storage == 9 && attribute.semantic != 4))
                        values.push_back(static_cast<std::int64_t>(value));
                    else
                        values.push_back(value);
                }
                rows.push_back(std::move(values));
            }
            channels.emplace(name, J(std::move(rows)));
        }
        J::Array triangles;
        auto indices = mesh.Indices(0, di);
        for (std::size_t i = 0; i < indices.size(); i += 3)
            triangles.push_back(J::array({indices[i], indices[i + 1], indices[i + 2]}));
        auto &draw = mesh.groups[0].draws[di];
        Monstey_Require((draw[6] & 65535) < doc.material_names.size(), "Native rigid material index outside table");
        channels.emplace("INDICES", J(std::move(triangles)));
        channels.emplace("material_index", draw[6] & 65535);
        channels.emplace("draw_parameter_words", J::array({draw[11], draw[12]}));
        parts.push_back(J(std::move(channels)));
    }
    return parts;
}

Monstey_Bytes Monstey_InstBuildRigid(std::span<const std::uint8_t> template_bytes, std::string_view resource_path,
                                     std::string_view material_path, std::span<const std::uint8_t> geometry_bytes)
{
    Monstey_WInst donor(B(template_bytes.begin(), template_bytes.end()));
    Monstey_Require(!donor.primary.empty() && !donor.secondary.empty(),
                    "Rigid conversion needs a native terminal secondary definition donor");
    auto definition = donor.secondary.back();
    Monstey_Require(Monstey_U32(definition, 16) == 0xffffffffu && Monstey_U32(definition, 32) == 0 &&
                        Monstey_U32(definition, 36) == 0 && Monstey_U32(definition, 40) == 0,
                    "Native rigid definition donor is nested or has unqualified render grouping");
    auto resource = Monstey_ArchiveVirtual(std::string(resource_path)),
         material = Monstey_ArchiveVirtual(std::string(material_path));
    Monstey_Require(resource.ends_with(".inst") && material.ends_with(".mat"),
                    "Rigid resource/material extensions differ");
    Monstey_Require(geometry_bytes.size() >= 20, "Rigid geometry block is truncated");
    auto draws = Monstey_U32(geometry_bytes, 0), vb = Monstey_U32(geometry_bytes, 4),
         ib = Monstey_U32(geometry_bytes, 8);
    Monstey_Require(draws > 0 && draws <= 65535 && 12ull + vb + ib + 4 <= geometry_bytes.size(),
                    "Rigid geometry counts exceed native bounds");
    std::size_t at = 12ull + vb + ib;
    auto layouts = Monstey_U32(geometry_bytes, at);
    at += 4;
    std::uint64_t attributes{};
    Monstey_Require(layouts > 0 && layouts <= 256, "Rigid layout count exceeds native byte index");
    for (std::uint32_t i = 0; i < layouts; ++i)
    {
        auto count = Monstey_U32(geometry_bytes, at);
        at += 4;
        Monstey_Require(count > 0 && count <= 256, "Rigid attribute count invalid");
        Monstey_CheckRange(geometry_bytes, at, 4ull * count);
        at += 4ull * count;
        attributes += count;
    }
    Monstey_Require(Monstey_U32(geometry_bytes, at) == draws && at + 4ull + 52ull * draws == geometry_bytes.size(),
                    "Rigid submesh metadata extent differs");
    auto allocation = ((std::uint64_t(vb) + 3) & ~3ull) + ((std::uint64_t(ib) + 3) & ~3ull);
    Monstey_Require(allocation <= 0xffffffffull && attributes <= 0xffffffffull,
                    "Rigid allocation or attributes overflow");
    std::copy(donor.primary[0].begin(), donor.primary[0].begin() + 8, definition.begin());
    Monstey_PutU32(definition, 8, 0);
    Monstey_PutU32(definition, 12, draws);
    Monstey_PutU32(definition, 16, 0xffffffffu);
    Monstey_PutU32(definition, 20, 0);
    Monstey_PutU32(definition, 32, 0);
    Monstey_PutU32(definition, 36, 0);
    Monstey_PutU32(definition, 40, 0);
    B material_bytes(material.begin(), material.end());
    std::replace(material_bytes.begin(), material_bytes.end(), '/', '\\');
    material_bytes.push_back(0);
    while (material_bytes.size() % 4)
        material_bytes.push_back(0);
    B output;
    for (auto word : {0x42465250u, 8u, Monstey_PathHash(resource), 1u, draws, 1u, 0u, 0u,
                      static_cast<std::uint32_t>(material_bytes.size()), layouts,
                      static_cast<std::uint32_t>(attributes), static_cast<std::uint32_t>(allocation), 0u, 0u})
        Monstey_AppendU32(output, word);
    output.insert(output.end(), definition.begin(), definition.end());
    output.insert(output.end(), geometry_bytes.begin(), geometry_bytes.end());
    output.insert(output.end(), material_bytes.begin(), material_bytes.end());
    Monstey_WInst checked(output);
    Monstey_Require(checked.Encode() == output, "Authored rigid PRFB identity readback differs");
    (void)Monstey_InstReadGeometry(output);
    (void)Monstey_InstDecodeGeometry(output);
    return output;
}

std::pair<Monstey_Bytes, Monstey_Json> Monstey_InstApplyBytes(std::span<const std::uint8_t> raw, const J &spec)
{
    Monstey_WInst doc(B(raw.begin(), raw.end()));
    auto report = doc.Apply(spec);
    auto output = doc.Encode();
    Monstey_WInst checked(output);
    Monstey_Require(checked.Encode() == output, "In-memory PRFB readback differs");
    return {output, report};
}

std::pair<Monstey_Bytes, Monstey_Json> Monstey_WdscBindingBytes(std::span<const std::uint8_t> inst,
                                                                std::string_view resource, std::uint32_t owner_key,
                                                                std::span<const std::uint8_t> donor)
{
    return Monstey_WBindingEncode(B(inst.begin(), inst.end()), B(donor.begin(), donor.end()), std::string(resource),
                                  owner_key);
}

} // namespace Monstey
