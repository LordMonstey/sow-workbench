#include "Monstey_FxWorkflow.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
void Monstey_FRequire(bool condition, std::string_view message) {
    if (!condition)
        throw Monstey_Error("fx_workflow", std::string(message));
}
std::string Monstey_FText(const J &a, std::string_view name, std::string_view fallback = {}) {
    if (!a.contains(name) || a[name].is_null())
        return std::string(fallback);
    return a[name].as_string();
}
std::string Monstey_FRef(Monstey_Gadb &db, std::string_view selector) {
    return Monstey_Hex(db.Resolve(selector));
}
const J *Monstey_FField(const J &metadata, std::string_view name) {
    const auto hash = Monstey_NativeHash(name);
    for (const auto &field : metadata["fields"].as_array())
        if (field["hash"].as_string() == Monstey_Hex(hash))
            return &field;
    return nullptr;
}
J Monstey_FRecords(Monstey_Gadb &db) {
    auto result = J::array();
    const auto categories = db.Categories();
    for (const auto &category : categories["rows"].as_array()) {
        std::size_t offset = 0;
        while (true) {
            const auto page =
                db.Records(static_cast<std::uint32_t>(category["index"].as_uint()), {}, offset, 10000);
            for (const auto &row : page["rows"].as_array()) {
                if (row.value("pending_delete", false).as_bool())
                    continue;
                const auto ref = row["native_reference"].as_string();
                auto metadata = db.Metadata(ref);
                if (metadata.value("pending_delete", false).as_bool())
                    continue;
                result.as_array().push_back(std::move(metadata));
            }
            offset += page["rows"].size();
            if (offset >= page["total"].as_uint())
                break;
        }
    }
    return result;
}
std::string Monstey_FGlobal(Monstey_Gadb &db, bool required = true) {
    std::string global;
    const auto records = Monstey_FRecords(db);
    for (const auto &record : records.as_array())
        if (Monstey_NativeHash(record["category"].as_string()) == Monstey_NativeHash("FxGlobalProps")) {
            Monstey_FRequire(global.empty(),
                             "LTFX workflow requires one FxGlobalProps record per effect file");
            global = Monstey_FRef(db, record["selector"].as_string());
        }
    Monstey_FRequire(!required || !global.empty(), "Create an independent effect before adding keys/tracks");
    return global;
}
bool Monstey_FElement(const J &metadata) { return metadata["category"].as_string().starts_with("ClientFX."); }
J Monstey_FKey(Monstey_Gadb &db, const J &reference) {
    if (reference.is_null())
        return nullptr;
    if (reference.is_string() && reference.as_string() == "0xffffffff")
        return nullptr;
    Monstey_FRequire(reference.is_string(), "FX keys require native record selectors or null");
    const auto selector = Monstey_FRef(db, reference.as_string());
    const auto record = db.Metadata(selector);
    Monstey_FRequire(!record.value("pending_delete", false).as_bool() && Monstey_FElement(record),
                     "FX key selector must name an undeleted ClientFX element");
    return selector;
}
J Monstey_FKeys(Monstey_Gadb &db, std::string_view global) {
    const auto metadata = db.Metadata(global);
    const auto field = Monstey_FField(metadata, "KeyList");
    if (!field)
        return J::array();
    Monstey_FRequire(((*field)["native_type"].as_uint() == 10 || (*field)["native_type"].as_uint() == 11) &&
                         (*field)["value"].is_array(),
                     "FxGlobalProps.KeyList must be a native reference array");
    auto keys = J::array();
    for (const auto &key : (*field)["value"].as_array())
        keys.as_array().push_back(Monstey_FKey(db, key));
    return keys;
}
J Monstey_FFlatten(const J &layout) {
    auto keys = J::array();
    for (const auto &track : layout["tracks"].as_array())
        for (const auto &key : track["keys"].as_array())
            keys.as_array().push_back(key);
    Monstey_FRequire(keys.size() <= 65535, "FX KeyList exceeds the native 65535-value bound");
    return keys;
}
J Monstey_FDefaultLayout(const J &keys) {
    auto tracks = J::array();
    if (!keys.empty())
        tracks.as_array().push_back(
            J::object({{"id", "track-1"}, {"name", "Imported keys"}, {"keys", keys}}));
    return J::object({{"format", "Monstey_FxWorkflow"},
                      {"version", 1u},
                      {"next_track_id", keys.empty() ? 1u : 2u},
                      {"tracks", tracks}});
}
J Monstey_FLayout(Monstey_Gadb &db, std::string_view global, bool requireSynchronized = true) {
    const auto keys = global.empty() ? J::array() : Monstey_FKeys(db, global);
    const auto metadata = db.ProjectMetadata();
    if (!metadata.contains("fx_workflow"))
        return Monstey_FDefaultLayout(keys);
    auto layout = metadata["fx_workflow"];
    Monstey_FRequire(layout.is_object() && layout["format"].as_string() == "Monstey_FxWorkflow" &&
                         layout["version"].as_uint() == 1 && layout["tracks"].is_array() &&
                         layout["tracks"].size() <= 65535 && layout["next_track_id"].as_uint() > 0,
                     "Unsupported FX editor track metadata");
    std::set<std::string> ids;
    for (auto &track : layout["tracks"].as_array()) {
        Monstey_FRequire(track.is_object() && track["keys"].is_array() &&
                             !track["name"].as_string().empty() && ids.insert(track["id"].as_string()).second,
                         "FX track names/IDs/key arrays are invalid");
        for (auto &key : track["keys"].as_array())
            key = Monstey_FKey(db, key);
    }
    if (requireSynchronized && Monstey_FFlatten(layout) != keys)
        throw Monstey_Error(
            "fx_layout_out_of_sync",
            "Native KeyList changed outside the track workflow; reconcile the layout explicitly");
    return layout;
}
void Monstey_FSaveLayout(Monstey_Gadb &db, std::string_view global, const J &layout) {
    const auto keys = Monstey_FFlatten(layout);
    const auto record = db.Metadata(global);
    if (Monstey_FField(record, "KeyList"))
        db.SetField(global, "KeyList", keys);
    else
        db.AddField(global, "KeyList", 10, keys, J::object({{"array", -1}, {"target", "FxKey"}}));
    auto metadata = db.ProjectMetadata();
    metadata["fx_workflow"] = layout;
    db.SetProjectMetadata(metadata);
}
std::size_t Monstey_FTrack(const J &layout, std::string_view selector) {
    std::size_t found = SIZE_MAX;
    for (std::size_t i = 0; i < layout["tracks"].size(); ++i)
        if (layout["tracks"][i]["id"].as_string() == selector)
            return i;
    for (std::size_t i = 0; i < layout["tracks"].size(); ++i)
        if (layout["tracks"][i]["name"].as_string() == selector) {
            Monstey_FRequire(found == SIZE_MAX, "Track name is ambiguous; use its stable editor ID");
            found = i;
        }
    Monstey_FRequire(found != SIZE_MAX, "FX track does not exist");
    return found;
}
std::size_t Monstey_FIndex(const J &a, std::string_view key, std::size_t limit, bool allowEnd = false) {
    Monstey_FRequire(a.contains(key), std::string("Missing index: ") + std::string(key));
    const auto value = a[key].as_uint();
    Monstey_FRequire(value < limit || (allowEnd && value == limit), "FX track/key index is outside its list");
    return static_cast<std::size_t>(value);
}
std::size_t Monstey_FCreateTrack(J &layout, std::string name, std::size_t index = SIZE_MAX) {
    Monstey_FRequire(!name.empty() && name.size() <= 512 && name.find('\0') == std::string::npos,
                     "Track name is invalid");
    Monstey_Wide(name);
    auto &tracks = layout["tracks"].as_array();
    const auto ordinal = layout["next_track_id"].as_uint();
    Monstey_FRequire(ordinal < UINT32_MAX && tracks.size() < 65535, "FX editor track limit exceeded");
    layout["next_track_id"] = ordinal + 1;
    if (index == SIZE_MAX)
        index = tracks.size();
    Monstey_FRequire(index <= tracks.size(), "New track position is outside its list");
    tracks.insert(
        tracks.begin() + static_cast<std::ptrdiff_t>(index),
        J::object({{"id", "track-" + std::to_string(ordinal)}, {"name", name}, {"keys", J::array()}}));
    return index;
}
std::size_t Monstey_FDestinationTrack(J &layout, const J &arguments) {
    const auto selector = Monstey_FText(arguments, "track");
    if (!selector.empty())
        return Monstey_FTrack(layout, selector);
    if (layout["tracks"].empty())
        return Monstey_FCreateTrack(layout, "Track 1");
    return 0;
}
void Monstey_FInsertKey(J &layout, std::size_t track, const J &key, const J &args) {
    auto &keys = layout["tracks"][track]["keys"].as_array();
    const auto index =
        args.contains("index") ? Monstey_FIndex(args, "index", keys.size(), true) : keys.size();
    keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(index), key);
}
const J &Monstey_FClass(const J &schema, std::string_view label) {
    Monstey_FRequire(schema.contains("categories"), "Load a qualified native effect schema first");
    const auto hash = Monstey_NativeHash(label);
    for (const auto &[name, value] : schema["categories"].as_object())
        if (Monstey_NativeHash(name) == hash)
            return value;
    throw Monstey_Error("fx_schema", std::string("Effect class is absent from the qualified schema: ") +
                                         std::string(label));
}
std::uint32_t Monstey_FCategory(Monstey_Gadb &db, std::string_view label) {
    const auto rows = db.Categories();
    const auto hash = Monstey_NativeHash(label);
    for (const auto &row : rows["rows"].as_array())
        if (Monstey_NativeHash(row["schema"].as_string()) == hash)
            return static_cast<std::uint32_t>(row["index"].as_uint());
    return static_cast<std::uint32_t>(db.CreateCategory(label, label, label)["category"]["index"].as_uint());
}
void Monstey_FPut(J &values, std::string_view name, const J &value) {
    Monstey_FRequire(values.is_object(), "Effect property values must be an object");
    const auto hash = Monstey_NativeHash(name);
    for (auto it = values.as_object().begin(); it != values.as_object().end();)
        if (Monstey_NativeHash(it->first) == hash)
            it = values.as_object().erase(it);
        else
            ++it;
    values[std::string(name)] = value;
}
const J *Monstey_FOverride(const J &values, std::string_view name) {
    const auto hash = Monstey_NativeHash(name);
    for (const auto &[key, value] : values.as_object())
        if (Monstey_NativeHash(key) == hash)
            return &value;
    return nullptr;
}
J Monstey_FNativeFields(const J &definition) {
    Monstey_FRequire(definition.contains("fields") && definition["fields"].is_array(),
                     "Effect class has no native fields");
    const auto &fields = definition["fields"];
    const bool complete = definition.contains("field_count");
    if (complete)
        Monstey_FRequire(definition["field_count"].as_uint() == fields.size() &&
                             definition.value("unreadable_fields", 0).as_uint() == 0 && fields.size() <= 256,
                         "Native reflection field table is incomplete or exceeds UInt8 identities");
    auto result = J::array();
    std::map<std::uint32_t, bool> hashes;
    std::set<std::uint8_t> identities;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        auto field = fields[i];
        const auto hash = Monstey_NativeHash(field["name"].as_string());
        if (field.contains("hash"))
            Monstey_FRequire(field["hash"].as_string() == Monstey_Hex(hash),
                             "Qualified native effect field hash differs from its name");
        const auto flags = field.value("flags", 0).as_uint();
        Monstey_FRequire(flags <= UINT32_MAX && (!complete || field.contains("flags")),
                         "Qualified native effect field flags are absent or out of range");
        const bool marker = (flags & 0x280u) != 0;
        const auto [previous, inserted] = hashes.emplace(hash, marker);
        Monstey_FRequire(inserted || (marker && previous->second),
                         "Duplicate consumed native effect field hash");
        if (marker)
            continue;
        if (field.contains("native_ordinal")) {
            Monstey_FRequire(field["native_ordinal"].is_integer() && field["native_ordinal"].as_int() >= 0 &&
                                 field["native_ordinal"].as_uint() <= 255 &&
                                 (!complete || field["native_ordinal"].as_uint() == i),
                             "Native callback identity differs from the full reflection index");
        } else if (complete) {
            // +64/ordinal is a mutable compact cache. The complete native table includes marker gaps.
            field["native_ordinal"] = static_cast<std::uint64_t>(i);
        }
        if (field.contains("native_ordinal"))
            Monstey_FRequire(
                identities.insert(static_cast<std::uint8_t>(field["native_ordinal"].as_uint())).second,
                "Duplicate qualified native field identity");
        result.as_array().push_back(std::move(field));
    }
    Monstey_FRequire(result.size() <= 255, "Native effect exceeds the 255 descriptor bound");
    return result;
}
J Monstey_FRequiredValues(const J &definition) {
    auto result = J::array();
    const auto fields = Monstey_FNativeFields(definition);
    for (const auto &field : fields.as_array())
        if ((!field.contains("default") || field["default"].is_null()) &&
            Monstey_NativeHash(field["name"].as_string()) != Monstey_NativeHash("KeyName")) {
            const auto type =
                field.contains("type_code") ? field["type_code"].as_uint() : field["native_type"].as_uint();
            if (type != 10 && type != 11)
                result.as_array().push_back(field["name"]);
        }
    return result;
}
std::string Monstey_FCreateRecord(Monstey_Gadb &db, const J &schema, std::string_view label,
                                  const J &values) {
    Monstey_FRequire(values.is_object(), "Effect property values must be an object");
    const auto &definition = Monstey_FClass(schema, label);
    const auto fields = Monstey_FNativeFields(definition);
    std::set<std::uint32_t> known;
    // Historical callers may supply editor marker defaults; they remain accepted but are not serialized.
    for (const auto &field : definition["fields"].as_array())
        known.insert(Monstey_NativeHash(field["name"].as_string()));
    std::set<std::uint32_t> supplied;
    for (const auto &[name, value] : values.as_object()) {
        (void)value;
        const auto hash = Monstey_NativeHash(name);
        Monstey_FRequire(known.contains(hash),
                         "Property is not declared by the native effect class: " + name);
        Monstey_FRequire(supplied.insert(hash).second,
                         "Effect properties contain duplicate native field-name hashes: " + name);
    }
    const auto category = Monstey_FCategory(db, label);
    const auto created = db.CreateRecord(category, {}, true);
    const auto selector = created["record"]["selector"].as_string();
    for (const auto &field : fields.as_array()) {
        const auto name = field["name"].as_string();
        Monstey_FRequire(field.contains("type_code") || field.contains("native_type"),
                         "Effect field lacks a qualified native type");
        const auto type =
            field.contains("type_code") ? field["type_code"].as_uint() : field["native_type"].as_uint();
        Monstey_FRequire(type > 0 && type <= 255, "Effect field type is outside the native descriptor");
        J value;
        if (const auto overrideValue = Monstey_FOverride(values, name))
            value = *overrideValue;
        else if (field.contains("default") && !field["default"].is_null())
            value = field["default"];
        else if (type == 10)
            value = field.value("array", 1).as_int() == -1 ? J::array() : J(nullptr);
        else if (type == 11)
            value = J::array();
        else
            throw Monstey_Error("fx_default_required",
                                "Supply a qualified initial value for " + std::string(label) + "." + name);
        db.AddField(selector, name, static_cast<std::uint8_t>(type), value, field);
    }
    return Monstey_FRef(db, selector);
}
void Monstey_FTiming(Monstey_Gadb &db, std::string_view selector) {
    const auto metadata = db.Metadata(selector);
    const auto start = Monstey_FField(metadata, "StartTime"), end = Monstey_FField(metadata, "EndTime");
    Monstey_FRequire(start && end && (*start)["native_type"].as_uint() == 2 &&
                         (*end)["native_type"].as_uint() == 2,
                     "Effect keys need native Float32 StartTime and EndTime");
    const auto a = (*start)["value"].as_number(), b = (*end)["value"].as_number();
    Monstey_FRequire(std::isfinite(a) && std::isfinite(b) && b >= a, "Effect key time range is invalid");
}
void Monstey_FRemoveLinks(J &layout, const J &key) {
    for (auto &track : layout["tracks"].as_array()) {
        auto &keys = track["keys"].as_array();
        keys.erase(std::remove(keys.begin(), keys.end(), key), keys.end());
    }
}
} // namespace
Monstey_Bytes Monstey_FxDefaultPrefix() { return {'L', 'T', 'F', 'X', 8, 0, 0, 0}; }
Monstey_Bytes Monstey_FxQualifiedPrefix(const std::filesystem::path &source,
                                        std::string_view expectedSha256) {
    Monstey_Gadb parsed(source);
    Monstey_FRequire(parsed.Describe()["format"].as_string() == "LTFX8/GADB17",
                     "Prefix template is not native LTFX8");
    const auto bytes = Monstey_ReadFile(source);
    const auto sha = Monstey_HashSha256(bytes);
    if (parsed.Describe()["source_sha256"].as_string() != sha ||
        (!expectedSha256.empty() && sha != expectedSha256))
        throw Monstey_Error("source_hash_mismatch",
                            "Effect prefix template snapshot changed or differs from expected hash");
    return Monstey_Bytes(bytes.begin(), bytes.begin() + 8);
}
Monstey_Json Monstey_FxVerifyNativeBindings(Monstey_Gadb &db) {
    const auto schema = db.Schema();
    const auto records = Monstey_FRecords(db);
    const auto changes = db.Changes();
    std::set<std::string> renamed;
    for (const auto &change : changes["transforms"].as_array())
        if (change["operation"].as_string() == "rename")
            renamed.insert(change["record"].as_string());
    std::map<std::string, std::vector<J>> groups;
    for (const auto &record : records.as_array()) {
        const auto label = record["category"].as_string();
        if (label.starts_with("ClientFX.") || label == "FxGlobalProps")
            groups[label].push_back(record);
    }
    auto categories = J::array(), unqualified = J::array();
    bool all = true;
    std::uint64_t conflicts = 0, total = 0, unqualifiedRecords = 0;
    for (auto &[label, group] : groups) {
        J expected;
        std::uint64_t flags = 0;
        try {
            const auto &definition = Monstey_FClass(schema, label);
            Monstey_FRequire(definition.contains("field_count"),
                             "Native binding verification needs a complete reflected field_count");
            expected = Monstey_FNativeFields(definition);
            const auto categoryFlags = definition.value("flags", "0x0");
            flags = categoryFlags.is_string() ? std::stoull(categoryFlags.as_string(), nullptr, 0)
                                              : categoryFlags.as_uint();
        } catch (const std::exception &error) {
            all = false;
            unqualifiedRecords += group.size();
            unqualified.as_array().push_back(J::object({{"category", label},
                                                        {"record_count", group.size()},
                                                        {"qualification_diagnostic", error.what()}}));
            continue;
        }
        // Build gives unrenamed clones increasing hashes beyond the native named tail.
        // Their friendly editor IDs are not their published native lookup names.
        std::map<std::string, std::uint32_t> nameHashes;
        std::uint64_t tail = 0;
        for (const auto &record : group)
            if (!record["anonymous"].as_bool() && !record["clone"].as_bool())
                tail = std::max(tail,
                                static_cast<std::uint64_t>(Monstey_NativeHash(record["name"].as_string())));
        std::vector<J> clones;
        for (const auto &record : group)
            if (record["clone"].as_bool())
                clones.push_back(record);
            else if (!record["anonymous"].as_bool())
                nameHashes[record["reference"].as_string()] = Monstey_NativeHash(record["name"].as_string());
        std::sort(clones.begin(), clones.end(), [](const J &a, const J &b) {
            return std::stoul(a["reference"].as_string(), nullptr, 0) <
                   std::stoul(b["reference"].as_string(), nullptr, 0);
        });
        for (const auto &record : clones) {
            const auto reference = record["reference"].as_string();
            if (renamed.contains(reference))
                nameHashes[reference] = Monstey_NativeHash(record["name"].as_string());
            else {
                Monstey_FRequire(tail < UINT32_MAX, "Native clone hash tail is exhausted");
                nameHashes[reference] = static_cast<std::uint32_t>(++tail);
            }
        }
        std::sort(group.begin(), group.end(), [&](const J &a, const J &b) {
            if (a["anonymous"] != b["anonymous"])
                return !a["anonymous"].as_bool();
            if (!a["anonymous"].as_bool())
                return nameHashes.at(a["reference"].as_string()) < nameHashes.at(b["reference"].as_string());

            return std::stoul(a["reference"].as_string(), nullptr, 0) <
                   std::stoul(b["reference"].as_string(), nullptr, 0);
        });
        auto rows = J::array();
        std::vector<bool> first;
        std::uint64_t categoryConflicts = 0;
        for (std::size_t r = 0; r < group.size(); ++r) {
            const auto &record = group[r];
            const auto &fields = record["fields"];
            auto predicates = J::array();
            bool compatible = fields.size() == expected.size();
            std::vector<bool> matches;
            for (std::size_t i = 0; i < expected.size(); ++i) {
                const auto &field = expected[i];
                const auto type = field.contains("type_code") ? field["type_code"].as_uint()
                                                              : field["native_type"].as_uint();
                const auto count = field.value("array", 1).as_int();
                const auto hash = Monstey_Hex(Monstey_NativeHash(field["name"].as_string()));
                const bool present = i < fields.size();
                const bool hashMatch = present && fields[i]["hash"].as_string() == hash;
                const bool identityMatch = present && fields[i]["native_ordinal"] == field["native_ordinal"];
                const auto storedType = present ? fields[i]["native_type"].as_uint() & 0x3fu : 0u;
                const bool typeMatch = present && (storedType == type || (type == 11 && storedType == 10));
                const bool countMatch = present && (count == -1 || fields[i]["count"].as_uint() ==
                                                                       static_cast<std::uint64_t>(count));
                const bool pass = hashMatch && identityMatch && typeMatch && countMatch && !(flags & 4u);
                matches.push_back(pass);
                compatible &= pass;
                const bool laterConflict = r > 0 && first[i] && !pass;
                if (laterConflict)
                    ++categoryConflicts;
                predicates.as_array().push_back(
                    J::object({{"field", field["name"]},
                               {"physical_index", static_cast<std::uint64_t>(i)},
                               {"expected_native_ordinal", field["native_ordinal"]},
                               {"actual_native_ordinal", present ? fields[i]["native_ordinal"] : J(nullptr)},
                               {"hash_match", hashMatch},
                               {"identity_match", identityMatch},
                               {"type_match", typeMatch},
                               {"count_match", countMatch},
                               {"native_fast_predicates_pass", pass},
                               {"later_record_cache_conflict", laterConflict}}));
            }
            if (r == 0)
                first = matches;
            rows.as_array().push_back(J::object({{"record", record["reference"]},
                                                 {"native_first_record", r == 0},
                                                 {"descriptor_count", fields.size()},
                                                 {"compatible", compatible},
                                                 {"fields", predicates}}));
            all &= compatible;
            ++total;
        }
        categories.as_array().push_back(J::object({{"category", label},
                                                   {"expected_descriptor_count", expected.size()},
                                                   {"first_record", group.front()["reference"]},
                                                   {"later_record_cache_conflicts", categoryConflicts},
                                                   {"records", rows}}));
        conflicts += categoryConflicts;
    }
    return J::object({{"format", "Monstey_FxNativeBindingPredicates"},
                      {"version", 1},
                      {"native_consumer", "0x140196A34"},
                      {"engine_executed", false},
                      {"assumes_unbound_field_cache", true},
                      {"verified_records", total},
                      {"all_records_compatible", all},
                      {"qualification_complete", unqualified.empty()},
                      {"unqualified_records", unqualifiedRecords},
                      {"total_records", total + unqualifiedRecords},
                      {"later_record_cache_conflicts", conflicts},
                      {"categories", categories},
                      {"unqualified_categories", unqualified}});
}
Monstey_Json Monstey_FxCreateElement(Monstey_Gadb &db, const J &args) {
    Monstey_FRequire(args.is_object(), "FX element arguments must be an object");
    Monstey_FRequire(db.Describe()["format"].as_string() == "LTFX8/GADB17",
                     "FX element creation needs the shared native LTFX8 database");
    J detail;
    auto result = db.Transaction([&](Monstey_Gadb &) {
        const auto name = Monstey_FText(args, "name"), label = Monstey_FText(args, "class_name");
        Monstey_FRequire(!name.empty(), "New element needs a key name");
        Monstey_FRequire(label.starts_with("ClientFX."), "Select a qualified ClientFX class");
        auto values = args.value("values", J::object());
        Monstey_FPut(values, "KeyName", name);
        if (args.contains("start"))
            Monstey_FPut(values, "StartTime", args["start"]);
        if (args.contains("end"))
            Monstey_FPut(values, "EndTime", args["end"]);
        const auto reference = Monstey_FCreateRecord(db, db.Schema(), label, values);
        Monstey_FTiming(db, reference);
        detail = db.Metadata(reference);
        detail["independent_record"] = true;
    });
    result["operation"] = "fx_create_element_independent";
    result["result"] = detail;
    return result;
}
Monstey_Json Monstey_FxWorkflowCall(Monstey_Gadb &db, std::string_view op, const J &args) {
    Monstey_FRequire(args.is_object(), "FX workflow arguments must be an object");
    Monstey_FRequire(db.Describe()["format"].as_string() == "LTFX8/GADB17",
                     "FX workflow needs the shared native LTFX8 database");
    if (op == "fx_schema") {
        const auto schema = db.Schema();
        const auto selected = Monstey_FText(args, "class_name");
        if (!selected.empty()) {
            const auto &definition = Monstey_FClass(schema, selected);
            return J::object({{"class_name", selected},
                              {"creation_qualified", true},
                              {"definition", definition},
                              {"native_fields", Monstey_FNativeFields(definition)},
                              {"required_initial_values", Monstey_FRequiredValues(definition)},
                              {"schema_sha256", db.Describe()["schema_sha256"]}});
        }
        auto classes = J::array(), unqualified = J::array();
        const auto query = Monstey_FText(args, "query");
        for (const auto &[name, definition] : schema["categories"].as_object()) {
            if (!name.starts_with("ClientFX.") || (!query.empty() && name.find(query) == std::string::npos))
                continue;
            auto row =
                J::object({{"class_name", name},
                           {"creation_qualified", false},
                           {"declared_field_count", definition.value("field_count", J(nullptr))},
                           {"unreadable_fields", definition.value("unreadable_fields", J(nullptr))},
                           {"field_count", definition.contains("fields") && definition["fields"].is_array()
                                               ? J(definition["fields"].size())
                                               : J(nullptr)}});
            try {
                const auto fields = Monstey_FNativeFields(definition);
                row["native_field_count"] = fields.size();
                row["required_initial_values"] = Monstey_FRequiredValues(definition);
                row["creation_qualified"] = true;
                classes.as_array().push_back(std::move(row));
            } catch (const std::exception &error) {
                // Browsing must isolate one incomplete class; creation still uses the strict factory.
                row["creation_diagnostic"] = error.what();
                unqualified.as_array().push_back(std::move(row));
            }
        }
        return J::object({{"classes", classes},
                          {"unqualified_classes", unqualified},
                          {"total_class_count", classes.size() + unqualified.size()},
                          {"qualified_class_count", classes.size()},
                          {"unqualified_class_count", unqualified.size()},
                          {"schema_sha256", db.Describe()["schema_sha256"]}});
    }
    if (op == "fx_workflow") {
        const auto global = Monstey_FGlobal(db, false);
        auto result = db.Describe();
        result["effect_exists"] = !global.empty();
        result["global_record"] = global;
        auto layout = Monstey_FLayout(db, global, false);
        const auto active = global.empty() ? J::array() : Monstey_FKeys(db, global);
        result["layout"] = layout;
        result["layout_synchronized"] = Monstey_FFlatten(layout) == active;
        auto keys = J::array(), inactive = J::array();
        std::set<std::uint32_t> activeRefs;
        for (std::size_t i = 0; i < active.size(); ++i) {
            if (active[i].is_null()) {
                keys.as_array().push_back(
                    J::object({{"index", static_cast<std::uint64_t>(i)}, {"record", nullptr}}));
                continue;
            }
            const auto ref = active[i].as_string();
            activeRefs.insert(db.Resolve(ref));
            auto entry = db.Metadata(ref);
            entry["index"] = static_cast<std::uint64_t>(i);
            keys.as_array().push_back(entry);
        }
        const auto records = Monstey_FRecords(db);
        for (const auto &record : records.as_array())
            if (Monstey_FElement(record) && !activeRefs.contains(db.Resolve(record["selector"].as_string())))
                inactive.as_array().push_back(record);
        result["active_keys"] = keys;
        result["unlinked_elements"] = inactive;
        return result;
    }
    J detail = J::object();
    auto state = db.Transaction([&](Monstey_Gadb &) {
        if (op == "fx_create_effect") {
            Monstey_FRequire(Monstey_FGlobal(db, false).empty(),
                             "This native file already has an effect; create a separate empty LTFX source");
            const auto schema = db.Schema();
            auto values = args.value("values", J::object());
            Monstey_FPut(values, "FxName", Monstey_FText(args, "name"));
            Monstey_FPut(values, "KeyList", J::array());
            Monstey_FRequire(!values["FxName"].as_string().empty(), "New effect needs a resource name");
            if (args.contains("duration_ms")) {
                Monstey_FRequire(args["duration_ms"].as_int() >= 0,
                                 "Effect duration_ms must be nonnegative int32");
                Monstey_FPut(values, "ResourceLenMS", args["duration_ms"]);
            }
            const auto global = Monstey_FCreateRecord(db, schema, "FxGlobalProps", values);
            auto layout = Monstey_FDefaultLayout(J::array());
            Monstey_FSaveLayout(db, global, layout);
            detail = J::object({{"global_record", global}, {"independent_record", true}});
            return;
        }
        const auto global = Monstey_FGlobal(db);
        if (op == "fx_rename_effect" || op == "fx_duplicate_effect") {
            const auto name = Monstey_FText(args, "name");
            Monstey_FRequire(!name.empty(), "Effect resource name is empty");
            db.SetField(global, "FxName", name);
            if (op == "fx_duplicate_effect") {
                const auto output = std::filesystem::path(Monstey_Wide(Monstey_FText(args, "output")));
                Monstey_FRequire(!output.empty(), "Duplicate effect requires a new .fxp output path");
                detail = db.Build(output);
                detail["intentional_whole_effect_copy"] = true;
            } else
                detail = J::object({{"global_record", global}, {"name", name}});
            return;
        }
        if (op == "fx_set_duration") {
            Monstey_FRequire(args.contains("duration_ms") && args["duration_ms"].as_int() >= 0,
                             "Effect duration_ms must be nonnegative int32");
            db.SetField(global, "ResourceLenMS", args["duration_ms"]);
            return;
        }
        if (op == "fx_retime_key" || op == "fx_rename_element") {
            const auto key = Monstey_FKey(db, Monstey_FText(args, "record"));
            Monstey_FRequire(!key.is_null(), "Select a physical effect key");
            const auto ref = key.as_string();
            if (op == "fx_rename_element") {
                const auto name = Monstey_FText(args, "name");
                Monstey_FRequire(!name.empty(), "Element name is empty");
                db.SetField(ref, "KeyName", name);
            } else {
                Monstey_FRequire(args.contains("start") && args.contains("end"),
                                 "Retiming requires start and end in seconds");
                db.SetField(ref, "StartTime", args["start"]);
                db.SetField(ref, "EndTime", args["end"]);
                Monstey_FTiming(db, ref);
            }
            detail = db.Metadata(ref);
            return;
        }
        if (op == "fx_add_field" || op == "fx_delete_field" || op == "fx_move_field") {
            const auto ref = Monstey_FRef(db, Monstey_FText(args, "record"));
            const auto metadata = db.Metadata(ref);
            Monstey_FRequire(ref == global || Monstey_FElement(metadata),
                             "FX attribute editing needs an effect/key record");
            if (op == "fx_add_field") {
                Monstey_FRequire(args.contains("native_type") && args["native_type"].as_uint() <= 255 &&
                                     args.contains("value"),
                                 "Field needs native_type and value");
                detail = db.AddField(ref, Monstey_FText(args, "field"),
                                     static_cast<std::uint8_t>(args["native_type"].as_uint()), args["value"],
                                     args.value("metadata", J::object()));
            } else if (op == "fx_delete_field")
                detail = db.DeleteField(ref, Monstey_FText(args, "field"));
            else
                detail = db.MoveField(ref, Monstey_FText(args, "field"),
                                      static_cast<std::size_t>(args["ordinal"].as_uint()));
            return;
        }
        auto layout = op == "fx_reconcile_layout" ? Monstey_FDefaultLayout(Monstey_FKeys(db, global))
                                                  : Monstey_FLayout(db, global);
        if (op == "fx_reconcile_layout") {
            Monstey_FSaveLayout(db, global, layout);
            detail = layout;
            return;
        }
        if (op == "fx_create_track") {
            const auto index = args.contains("index")
                                   ? Monstey_FIndex(args, "index", layout["tracks"].size(), true)
                                   : layout["tracks"].size();
            const auto track = Monstey_FCreateTrack(layout, Monstey_FText(args, "name"), index);
            detail = layout["tracks"][track];
        } else if (op == "fx_rename_track" || op == "fx_delete_track" || op == "fx_move_track") {
            const auto track = Monstey_FTrack(layout, Monstey_FText(args, "track"));
            auto &tracks = layout["tracks"].as_array();
            if (op == "fx_rename_track") {
                const auto name = Monstey_FText(args, "name");
                Monstey_FRequire(!name.empty() && name.size() <= 512 && name.find('\0') == std::string::npos,
                                 "Track name is invalid");
                Monstey_Wide(name);
                tracks[track]["name"] = name;
                detail = tracks[track];
            } else if (op == "fx_delete_track") {
                detail = J::object({{"unlinked_keys", tracks[track]["keys"]}, {"records_deleted", false}});
                tracks.erase(tracks.begin() + static_cast<std::ptrdiff_t>(track));
            } else {
                const auto to = Monstey_FIndex(args, "index", tracks.size());
                auto value = tracks[track];
                tracks.erase(tracks.begin() + static_cast<std::ptrdiff_t>(track));
                tracks.insert(tracks.begin() + static_cast<std::ptrdiff_t>(to), value);
                detail = value;
            }
        } else if (op == "fx_create_element" || op == "fx_duplicate_element") {
            const auto name = Monstey_FText(args, "name");
            Monstey_FRequire(!name.empty(), "New element needs a key name");
            std::string ref;
            if (op == "fx_create_element") {
                const auto created = Monstey_FxCreateElement(db, args);
                ref = Monstey_FRef(db, created["result"]["selector"].as_string());
            } else {
                const auto source = Monstey_FKey(db, Monstey_FText(args, "source"));
                Monstey_FRequire(!source.is_null(), "Select an element to duplicate");
                const auto records = Monstey_FRecords(db);
                std::set<std::uint32_t> used;
                for (const auto &record : records.as_array())
                    if (!record["anonymous"].as_bool())
                        used.insert(Monstey_NativeHash(record["name"].as_string()));
                std::string id;
                std::uint32_t ordinal = 1;
                do {
                    id = "Monstey_FxKey_" + std::to_string(db.Resolve(source.as_string())) + "_" +
                         std::to_string(ordinal++);
                } while (used.contains(Monstey_NativeHash(id)));
                const auto copied = db.CloneRecord(source.as_string(), id);
                ref = Monstey_FRef(db, copied["clone"]["reference"].as_string());
                db.SetField(ref, "KeyName", name);
            }
            const auto track = Monstey_FDestinationTrack(layout, args);
            Monstey_FInsertKey(layout, track, ref, args);
            detail = db.Metadata(ref);
            detail["independent_record"] = true;
        } else if (op == "fx_link_key") {
            const auto key = Monstey_FKey(db, Monstey_FText(args, "record"));
            Monstey_FRequire(!key.is_null(), "Select an existing key to link");
            const auto track = Monstey_FDestinationTrack(layout, args);
            Monstey_FInsertKey(layout, track, key, args);
            detail = J::object({{"record", key}, {"shared_native_record", true}});
        } else if (op == "fx_unlink_key" || op == "fx_move_key") {
            const auto track = Monstey_FTrack(layout, Monstey_FText(args, "track"));
            auto &keys = layout["tracks"][track]["keys"].as_array();
            const auto index = Monstey_FIndex(args, "index", keys.size());
            auto key = keys[index];
            keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(index));
            if (op == "fx_move_key") {
                const auto to = Monstey_FIndex(args, "to", keys.size(), true);
                keys.insert(keys.begin() + static_cast<std::ptrdiff_t>(to), key);
            }
            detail = J::object({{"record", key}, {"record_deleted", false}});
        } else if (op == "fx_delete_element") {
            const auto key = Monstey_FKey(db, Monstey_FText(args, "record"));
            Monstey_FRequire(!key.is_null(), "Select a physical element to delete");
            const auto replacement = args.value("replacement", J(nullptr));
            if (!replacement.is_null() && replacement != J("0xffffffff")) {
                const auto target = Monstey_FKey(db, replacement);
                for (auto &track : layout["tracks"].as_array())
                    for (auto &entry : track["keys"].as_array())
                        if (entry == key)
                            entry = target;
            } else
                Monstey_FRemoveLinks(layout, key);
            Monstey_FSaveLayout(db, global, layout);
            detail = db.DeleteRecord(key.as_string(), replacement);
            return;
        } else if (op == "fx_delete_effect") {
            const auto keys = Monstey_FKeys(db, global);
            std::set<std::string> physical;
            for (const auto &key : keys.as_array())
                if (!key.is_null())
                    physical.insert(key.as_string());
            db.SetField(global, "KeyList", J::array());
            for (const auto &key : physical)
                db.DeleteRecord(key, args.value("replacement", J(nullptr)));
            db.DeleteRecord(global);
            auto metadata = db.ProjectMetadata();
            metadata["fx_workflow"] = Monstey_FDefaultLayout(J::array());
            db.SetProjectMetadata(metadata);
            detail = J::object({{"effect_deleted", true}, {"unlinked_records_retained", true}});
            return;
        } else
            throw Monstey_Error("unknown_fx_operation",
                                "Unknown native FX workflow operation: " + std::string(op));
        Monstey_FSaveLayout(db, global, layout);
    });
    state["operation"] = std::string(op);
    state["result"] = detail;
    return state;
}
} // namespace Monstey