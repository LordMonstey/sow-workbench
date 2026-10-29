#include "Monstey_FxLibrary.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
void Monstey_LRequire(bool value, std::string_view message) {
    if (!value)
        throw Monstey_Error("fx_prefab", std::string(message));
}
std::string Monstey_LText(const J &args, std::string_view key, std::string_view fallback = {}) {
    return args.contains(key) && !args[key].is_null() ? args[key].as_string() : std::string(fallback);
}
void Monstey_LName(std::string_view name) {
    Monstey_LRequire(!name.empty() && name.size() <= 512 && name.find('\0') == std::string_view::npos,
                     "Library name needs 1..512 UTF-8 bytes without NUL");
    Monstey_Wide(name);
}
J Monstey_LState(Monstey_Gadb &db) {
    auto state = db.ProjectMetadata().value("fx_library", J::object({{"version", 1},
                                                                     {"next_id", 1u},
                                                                     {"groups", J::object()},
                                                                     {"prefabs", J::object()},
                                                                     {"instances", J::object()}}));
    Monstey_LRequire(state.is_object() && state["version"].as_uint() == 1 && state["next_id"].as_uint() > 0 &&
                         state["groups"].is_object() && state["prefabs"].is_object() &&
                         state["instances"].is_object() && state["groups"].size() <= 65535 &&
                         state["prefabs"].size() <= 65535 && state["instances"].size() <= 65535,
                     "Unsupported component prefab project metadata");
    return state;
}
void Monstey_LSave(Monstey_Gadb &db, const J &state) {
    auto metadata = db.ProjectMetadata();
    metadata["fx_library"] = state;
    db.SetProjectMetadata(metadata);
}
std::string Monstey_LId(J &state, std::string_view kind) {
    const auto next = state["next_id"].as_uint();
    Monstey_LRequire(next < UINT32_MAX, "Prefab ID sequence exhausted");
    state["next_id"] = next + 1;
    return std::string(kind) + "-" + std::to_string(next);
}
std::string Monstey_LRecord(Monstey_Gadb &db, std::string_view selector) {
    const auto ref = Monstey_Hex(db.Resolve(selector));
    const auto metadata = db.Metadata(ref);
    Monstey_LRequire(!metadata["pending_delete"].as_bool() &&
                         metadata["category"].as_string().starts_with("ClientFX."),
                     "Prefab records must be undeleted native ClientFX components");
    return ref;
}
const J *Monstey_LDeclaration(const J &schema, std::string_view category, std::string_view hash) {
    if (!schema.contains("categories") || !schema["categories"].contains(category))
        return nullptr;
    for (const auto &field : schema["categories"][category]["fields"].as_array())
        if (Monstey_Hex(Monstey_NativeHash(field["name"].as_string())) == hash)
            return &field;
    return nullptr;
}
bool Monstey_LProperty(const J &field) {
    for (const auto name : {"KeyName", "StartTime", "EndTime"})
        if (field["hash"].as_string() == Monstey_Hex(Monstey_NativeHash(name)))
            return false;
    const auto constraint = Monstey_LText(field, "constraint");
    return !constraint.starts_with("GROUP_BEGIN=") && !constraint.starts_with("GROUP_END=");
}
J Monstey_LProperties(const J &record) {
    auto fields = J::object();
    for (const auto &field : record["fields"].as_array())
        if (Monstey_LProperty(field))
            fields[field["hash"].as_string()] = field;
    return fields;
}
J Monstey_LValues(const J &fields) {
    auto values = J::object();
    for (const auto &[hash, field] : fields.as_object())
        values[hash] = field["value"];
    return values;
}
J Monstey_LAllLocal(const J &fields) {
    auto local = J::array();
    for (const auto &[hash, field] : fields.as_object()) {
        (void)field;
        local.as_array().push_back(hash);
    }
    return local;
}
bool Monstey_LHas(const J &local, std::string_view hash) {
    return std::any_of(local.as_array().begin(), local.as_array().end(),
                       [&](const auto &v) { return v.as_string() == hash; });
}
void Monstey_LSetLocal(J &owner, std::string_view hash, bool local) {
    auto &values = owner["local_fields"].as_array();
    values.erase(
        std::remove_if(values.begin(), values.end(), [&](const auto &v) { return v.as_string() == hash; }),
        values.end());
    if (local)
        values.push_back(std::string(hash));
}
void Monstey_LObserve(Monstey_Gadb &db, J &state) {
    const auto observe = [&](J &owner, std::string_view record) {
        const auto values = Monstey_LValues(Monstey_LProperties(db.Metadata(Monstey_LRecord(db, record))));
        for (const auto &[hash, value] : values.as_object())
            if (!owner["baseline"].contains(hash) || owner["baseline"][hash] != value)
                Monstey_LSetLocal(owner, hash, true);
        for (const auto &[hash, value] : owner["baseline"].as_object()) {
            (void)value;
            Monstey_LRequire(values.contains(hash),
                             "Bound prefab field was removed; detach before structural deletion");
        }
    };
    for (auto &[id, prefab] : state["prefabs"].as_object()) {
        (void)id;
        observe(prefab, prefab["record"].as_string());
    }
    for (auto &[record, instance] : state["instances"].as_object())
        observe(instance, record);
}
void Monstey_LApplyFields(Monstey_Gadb &db, std::string_view record, const J &effective, const J &schema) {
    const auto metadata = db.Metadata(record);
    const auto fields = Monstey_LProperties(metadata);
    for (const auto &[hash, field] : effective.as_object()) {
        if (fields.contains(hash)) {
            if (fields[hash]["value"] != field["value"])
                db.SetField(record, hash, field["value"]);
        } else {
            const auto declaration = Monstey_LDeclaration(schema, metadata["category"].as_string(), hash);
            Monstey_LRequire(declaration != nullptr,
                             "Inherited absent field has no qualified native declaration");
            db.AddField(record, (*declaration)["name"].as_string(),
                        static_cast<std::uint8_t>(field["native_type"].as_uint()), field["value"],
                        *declaration);
        }
    }
}
void Monstey_LApply(Monstey_Gadb &db, J &state, const J &schema) {
    std::set<std::string> active;
    auto resolved = J::object();
    std::function<J(std::string)> resolve = [&](std::string id) {
        if (resolved.contains(id))
            return resolved[id];
        Monstey_LRequire(state["prefabs"].contains(id) && active.size() < 64 && active.insert(id).second,
                         "Prefab parent is absent, cyclic or too deep");
        auto &prefab = state["prefabs"][id];
        const auto record = Monstey_LRecord(db, prefab["record"].as_string());
        const auto metadata = db.Metadata(record);
        Monstey_LRequire(metadata["category"] == prefab["class_name"],
                         "Prefab record differs from its stored native component type");
        auto fields = Monstey_LProperties(metadata);
        const auto parentId = Monstey_LText(prefab, "parent");
        if (!parentId.empty()) {
            Monstey_LRequire(state["prefabs"].contains(parentId), "Parent prefab does not exist");
            Monstey_LRequire(state["prefabs"][parentId]["class_name"] == prefab["class_name"],
                             "Prefab inheritance needs exact native component type equality");
            const auto inherited = resolve(parentId);
            for (const auto &[hash, field] : inherited.as_object())
                if (!Monstey_LHas(prefab["local_fields"], hash))
                    fields[hash] = field;
            Monstey_LApplyFields(db, record, fields, schema);
        }
        prefab["baseline"] = Monstey_LValues(Monstey_LProperties(db.Metadata(record)));
        resolved[id] = fields;
        active.erase(id);
        return fields;
    };
    for (const auto &[id, prefab] : state["prefabs"].as_object()) {
        (void)prefab;
        resolve(id);
    }
    for (auto &[record, instance] : state["instances"].as_object()) {
        const auto id = instance["prefab"].as_string();
        const auto inherited = resolve(id);
        const auto metadata = db.Metadata(Monstey_LRecord(db, record));
        Monstey_LRequire(metadata["category"] == state["prefabs"][id]["class_name"],
                         "Instance and prefab native component types differ");
        auto fields = Monstey_LProperties(metadata);
        for (const auto &[hash, field] : inherited.as_object())
            if (!Monstey_LHas(instance["local_fields"], hash))
                fields[hash] = field;
        Monstey_LApplyFields(db, record, fields, schema);
        instance["baseline"] = Monstey_LValues(Monstey_LProperties(db.Metadata(record)));
    }
}
void Monstey_LDeletePrefab(Monstey_Gadb &db, J &state, std::string_view id, const J &args, const J &schema) {
    Monstey_LRequire(state["prefabs"].contains(id), "Prefab not found");
    Monstey_LApply(db, state, schema);
    const auto record = state["prefabs"][id]["record"].as_string();
    for (auto &[childId, child] : state["prefabs"].as_object()) {
        (void)childId;
        if (Monstey_LText(child, "parent") == id) {
            child["parent"] = "";
            child["local_fields"] =
                Monstey_LAllLocal(Monstey_LProperties(db.Metadata(child["record"].as_string())));
        }
    }
    for (auto it = state["instances"].as_object().begin(); it != state["instances"].as_object().end();)
        if (it->second["prefab"].as_string() == id)
            it = state["instances"].as_object().erase(it);
        else
            ++it;
    state["prefabs"].as_object().erase(std::string(id));
    if (Monstey_LText(state, "active_prefab") == id)
        state["active_prefab"] = "";
    if (Monstey_FxWorkflowCall(db, "fx_workflow")["effect_exists"].as_bool())
        Monstey_FxWorkflowCall(
            db, "fx_delete_element",
            J::object({{"record", record}, {"replacement", args.value("replacement", J(nullptr))}}));
    else
        db.DeleteRecord(record, args.value("replacement", J(nullptr)));
}
J Monstey_LOwner(Monstey_Gadb &db, std::string_view record, J &state) {
    const auto ref = Monstey_LRecord(db, record);
    if (state["instances"].contains(ref))
        return J::object({{"kind", "instance"}, {"key", ref}});
    for (const auto &[id, prefab] : state["prefabs"].as_object())
        if (prefab["record"].as_string() == ref)
            return J::object({{"kind", "prefab"}, {"key", id}});
    throw Monstey_Error("fx_prefab", "Record has no prefab association");
}
J &Monstey_LOwnerValue(J &state, const J &owner) {
    return state[owner["kind"].as_string() == "prefab" ? "prefabs" : "instances"][owner["key"].as_string()];
}
std::string Monstey_LField(Monstey_Gadb &db, std::string_view record, std::string_view selector) {
    const auto properties = Monstey_LProperties(db.Metadata(record));
    for (const auto &[hash, field] : properties.as_object())
        if (selector == hash || Monstey_NativeHash(selector) == Monstey_NativeHash(field["name"].as_string()))
            return hash;
    throw Monstey_Error("fx_prefab", "Select an existing component property, excluding key name/timing");
}
std::string Monstey_LClone(Monstey_Gadb &db, std::string_view source, std::string_view id) {
    std::uint32_t ordinal = 1;
    for (;;) {
        const auto name = "Monstey_Prefab_" + std::string(id) + "_" + std::to_string(ordinal++);
        bool exists = false;
        try {
            db.Resolve(name);
            exists = true;
        } catch (const std::exception &) {
        }
        if (!exists)
            return Monstey_Hex(db.Resolve(db.CloneRecord(source, name)["clone"]["reference"].as_string()));
    }
}
} // namespace
Monstey_Json Monstey_FxLibraryCall(Monstey_Gadb &db, std::string_view op, const J &args) {
    Monstey_LRequire(args.is_object() && db.Describe()["format"].as_string() == "LTFX8/GADB17",
                     "Component library requires LTFX8 and object arguments");
    if (op == "fx_library") {
        auto report = db.Describe();
        report["library"] = Monstey_LState(db);
        return report;
    }
    J detail;
    auto report = db.Transaction([&](Monstey_Gadb &) {
        auto state = Monstey_LState(db);
        const auto schema = db.Schema();
        Monstey_LObserve(db, state);
        Monstey_LApply(db, state, schema);
        if (op == "fx_library_group_create" || op == "fx_library_group_rename") {
            const auto name = Monstey_LText(args, "name");
            Monstey_LName(name);
            const auto id =
                op == "fx_library_group_create" ? Monstey_LId(state, "group") : args["group"].as_string();
            if (op == "fx_library_group_create") {
                const auto parentId = Monstey_LText(args, "parent");
                Monstey_LRequire(parentId.empty() || state["groups"].contains(parentId),
                                 "Library parent group does not exist");
                state["groups"][id] = J::object({{"name", name}, {"parent", parentId}});
            } else {
                Monstey_LRequire(state["groups"].contains(id), "Library group not found");
                state["groups"][id]["name"] = name;
            }
            detail = J::object({{"group", id}});
        } else if (op == "fx_prefab_create" || op == "fx_prefab_from_key" || op == "fx_prefab_duplicate") {
            const auto name = Monstey_LText(args, "name");
            Monstey_LName(name);
            const auto id = Monstey_LId(state, "prefab");
            const auto group = Monstey_LText(args, "group");
            Monstey_LRequire(group.empty() || state["groups"].contains(group), "Prefab group not found");
            std::string record;
            if (op == "fx_prefab_create")
                record = Monstey_Hex(
                    db.Resolve(Monstey_FxCreateElement(db, args)["result"]["selector"].as_string()));
            else {
                const auto source = op == "fx_prefab_duplicate"
                                        ? state["prefabs"][args["prefab"].as_string()]["record"].as_string()
                                        : args["record"].as_string();
                record = Monstey_LClone(db, Monstey_LRecord(db, source), id);
                db.SetField(record, "KeyName", name);
            }
            const auto metadata = db.Metadata(record);
            const auto fields = Monstey_LProperties(metadata);
            auto localFields = Monstey_LAllLocal(fields);
            if (op == "fx_prefab_create") {
                localFields = J::array();
                const auto supplied = args.value("values", J::object());
                for (const auto &[hash, field] : fields.as_object())
                    for (const auto &[suppliedName, value] : supplied.as_object()) {
                        (void)value;
                        if (Monstey_Hex(Monstey_NativeHash(suppliedName)) == hash) {
                            localFields.as_array().push_back(hash);
                            break;
                        }
                    }
            }
            state["prefabs"][id] = J::object({{"name", name},
                                              {"group", group},
                                              {"record", record},
                                              {"class_name", metadata["category"]},
                                              {"parent", ""},
                                              {"local_fields", localFields},
                                              {"baseline", Monstey_LValues(fields)},
                                              {"source_sha256", db.Describe()["source_sha256"]}});
            detail = J::object({{"prefab", id}, {"record", record}, {"inactive_native_template", true}});
        } else if (op == "fx_library_group_move") {
            const auto id = args["group"].as_string(), parentId = Monstey_LText(args, "parent");
            Monstey_LRequire(state["groups"].contains(id) &&
                                 (parentId.empty() || state["groups"].contains(parentId)),
                             "Group or target parent is absent");
            std::set<std::string> seen;
            for (auto ancestor = parentId; !ancestor.empty();
                 ancestor = Monstey_LText(state["groups"][ancestor], "parent"))
                Monstey_LRequire(ancestor != id && seen.insert(ancestor).second,
                                 "Library group parent cycle");
            state["groups"][id]["parent"] = parentId;
            detail = J::object({{"group", id}});
        } else if (op == "fx_prefab_delete") {
            const auto id = args["prefab"].as_string();
            Monstey_LDeletePrefab(db, state, id, args, schema);
            detail = J::object({{"prefab", id}, {"effective_child_properties_preserved", true}});
        } else if (op == "fx_library_group_delete") {
            const auto id = args["group"].as_string();
            Monstey_LRequire(state["groups"].contains(id), "Library group not found");
            std::set<std::string> groups{id};
            bool changed = true;
            while (changed) {
                changed = false;
                for (const auto &[candidate, group] : state["groups"].as_object())
                    if (groups.contains(Monstey_LText(group, "parent")))
                        changed |= groups.insert(candidate).second;
            }
            std::vector<std::string> prefabs;
            for (const auto &[prefabId, prefab] : state["prefabs"].as_object())
                if (groups.contains(Monstey_LText(prefab, "group")))
                    prefabs.push_back(prefabId);
            Monstey_LRequire(args.value("recursive", true).as_bool() ||
                                 (groups.size() == 1 && prefabs.empty()),
                             "Nonempty group deletion requires recursive selection");
            for (const auto &prefabId : prefabs)
                Monstey_LDeletePrefab(db, state, prefabId, args, schema);
            for (const auto &groupId : groups)
                state["groups"].as_object().erase(groupId);
            detail = J::object({{"deleted_groups", static_cast<std::uint64_t>(groups.size())},
                                {"deleted_prefabs", static_cast<std::uint64_t>(prefabs.size())}});
        } else if (op == "fx_prefab_set_active") {
            const auto id = Monstey_LText(args, "prefab");
            Monstey_LRequire(id.empty() || state["prefabs"].contains(id), "Active prefab does not exist");
            state["active_prefab"] = id;
            detail = J::object({{"prefab", id}});
        } else if (op == "fx_prefab_rename" || op == "fx_prefab_move" || op == "fx_prefab_parent") {
            const auto id = args["prefab"].as_string();
            Monstey_LRequire(state["prefabs"].contains(id), "Prefab not found");
            auto &prefab = state["prefabs"][id];
            if (op == "fx_prefab_rename") {
                const auto name = args["name"].as_string();
                Monstey_LName(name);
                prefab["name"] = name;
                db.SetField(prefab["record"].as_string(), "KeyName", name);
            } else if (op == "fx_prefab_move") {
                const auto group = Monstey_LText(args, "group");
                Monstey_LRequire(group.empty() || state["groups"].contains(group), "Target group missing");
                prefab["group"] = group;
            } else {
                prefab["parent"] = Monstey_LText(args, "parent");
                if (!args.value("keep_overrides", true).as_bool())
                    prefab["local_fields"] = J::array();
            }
            detail = J::object({{"prefab", id}});
        } else if (op == "fx_prefab_assign" || op == "fx_prefab_add_child") {
            const auto id = Monstey_LText(args, "prefab", Monstey_LText(state, "active_prefab"));
            Monstey_LRequire(state["prefabs"].contains(id), "Prefab not found");
            const auto &prefab = state["prefabs"][id];
            std::string record;
            if (op == "fx_prefab_add_child") {
                Monstey_LRequire(args.contains("time") && std::isfinite(args["time"].as_number()),
                                 "Prefab child insertion needs finite cursor time");
                record = Monstey_LClone(db, prefab["record"].as_string(), Monstey_LId(state, "instance"));
                const auto time = args["time"].as_number();
                db.SetField(record, "StartTime", time - 1);
                db.SetField(record, "EndTime", time + 1);
                const auto name = Monstey_LText(args, "name", prefab["name"].as_string());
                Monstey_LName(name);
                db.SetField(record, "KeyName", name);
                auto link = J::object({{"record", record}});
                if (args.contains("track"))
                    link["track"] = args["track"];
                if (args.contains("index"))
                    link["index"] = args["index"];
                Monstey_FxWorkflowCall(db, "fx_link_key", link);
            } else
                record = Monstey_LRecord(db, args["record"].as_string());
            for (const auto &[otherId, other] : state["prefabs"].as_object()) {
                (void)otherId;
                Monstey_LRequire(other["record"].as_string() != record,
                                 "Use prefab parent assignment for template records");
            }
            const auto metadata = db.Metadata(record);
            Monstey_LRequire(metadata["category"] == prefab["class_name"],
                             "Assigning a prefab requires exact native type equality");
            const auto fields = Monstey_LProperties(metadata);
            const bool keep = op != "fx_prefab_add_child" && args.value("keep_overrides", true).as_bool();
            state["instances"][record] =
                J::object({{"prefab", id},
                           {"local_fields", keep ? Monstey_LAllLocal(fields) : J::array()},
                           {"baseline", Monstey_LValues(fields)}});
            detail =
                J::object({{"record", record}, {"prefab", id}, {"native_parent_attachment_created", false}});
        } else if (op == "fx_prefab_override" || op == "fx_prefab_inherit") {
            const auto record = Monstey_LRecord(db, args["record"].as_string());
            const auto owner = Monstey_LOwner(db, record, state);
            auto &value = Monstey_LOwnerValue(state, owner);
            const auto hash = Monstey_LField(db, record, args["field"].as_string());
            if (op == "fx_prefab_override") {
                db.SetField(record, hash, args["value"]);
                Monstey_LSetLocal(value, hash, true);
            } else {
                Monstey_LRequire(owner["kind"].as_string() == "instance" ||
                                     !Monstey_LText(value, "parent").empty(),
                                 "Property has no parent prefab to inherit from");
                Monstey_LSetLocal(value, hash, false);
            }
            detail = J::object(
                {{"record", record}, {"field_hash", hash}, {"local_override", op == "fx_prefab_override"}});
        } else if (op == "fx_prefab_detach") {
            const auto record = Monstey_LRecord(db, args["record"].as_string());
            const auto owner = Monstey_LOwner(db, record, state);
            if (owner["kind"].as_string() == "instance")
                state["instances"].as_object().erase(record);
            else {
                auto &value = Monstey_LOwnerValue(state, owner);
                value["parent"] = "";
                value["local_fields"] = Monstey_LAllLocal(Monstey_LProperties(db.Metadata(record)));
            }
            detail = J::object({{"record", record}, {"native_values_preserved", true}});
        } else if (op != "fx_prefab_sync")
            throw Monstey_Error("unknown_fx_library_operation", "Unknown component prefab operation");
        Monstey_LApply(db, state, schema);
        Monstey_LSave(db, state);
    });
    report["operation"] = std::string(op);
    report["result"] = detail;
    return report;
}
Monstey_Json Monstey_FxLibraryNativeCall(Monstey_Gadb &db, std::string_view op, const J &args) {
    return Monstey_FxLibraryNativeCall(db, op, args, nullptr);
}
Monstey_Json Monstey_FxLibraryNativeCall(Monstey_Gadb &db, std::string_view op, const J &args,
                                         const Monstey_GadbClassCache *declarations) {
    Monstey_LRequire(args.is_object(), "Native prefab integration arguments must be an object");
    Monstey_LRequire(!declarations || op == "fx_build", "Native declarations guard applies only to fx_build");
    Monstey_LRequire(op == "fx_set" || op == "fx_delete_element" || op == "fx_delete_effect" ||
                         op == "fx_build" || op == "fx_save_project",
                     "Unsupported native operation for component prefab integration");
    if (!db.ProjectMetadata().contains("fx_library")) {
        if (op == "fx_set")
            return db.SetField(args["record"].as_string(), args["field"].as_string(), args["value"]);
        if (op == "fx_build")
            return (declarations ? db.Build(std::filesystem::path(Monstey_Wide(args["output"].as_string())), *declarations)
                                : db.Build(std::filesystem::path(Monstey_Wide(args["output"].as_string()))));
        if (op == "fx_save_project")
            return db.SaveProject(std::filesystem::path(Monstey_Wide(args["path"].as_string())));
        return Monstey_FxWorkflowCall(db, op, args);
    }
    J detail;
    const bool writer = op == "fx_build" || op == "fx_save_project";
    const auto stateAfter = db.Transaction([&](Monstey_Gadb &) {
        auto state = Monstey_LState(db);
        const auto schema = db.Schema();
        Monstey_LObserve(db, state);
        Monstey_LApply(db, state, schema);
        if (op == "fx_set") {
            detail = db.SetField(args["record"].as_string(), args["field"].as_string(), args["value"]);
            Monstey_LObserve(db, state);
        } else if (op == "fx_delete_element") {
            const auto record = Monstey_LRecord(db, args["record"].as_string());
            std::string templateId;
            for (const auto &[id, prefab] : state["prefabs"].as_object())
                if (prefab["record"].as_string() == record)
                    templateId = id;
            if (!templateId.empty()) {
                Monstey_LDeletePrefab(db, state, templateId, args, schema);
                detail = db.Describe();
                detail["operation"] = "fx_delete_element";
                detail["result"] = db.Describe();
            } else {
                detail = Monstey_FxWorkflowCall(db, op, args);
                state["instances"].as_object().erase(record);
            }
        } else if (op == "fx_delete_effect") {
            const auto active = Monstey_FxWorkflowCall(db, "fx_workflow")["active_keys"];
            std::set<std::string> deletedRecords;
            for (const auto &key : active.as_array())
                if (key.contains("reference"))
                    deletedRecords.insert(key["reference"].as_string());
            std::vector<std::string> deletedTemplates;
            for (const auto &[id, prefab] : state["prefabs"].as_object())
                if (deletedRecords.contains(prefab["record"].as_string()))
                    deletedTemplates.push_back(id);
            for (const auto &id : deletedTemplates)
                Monstey_LDeletePrefab(db, state, id, args, schema);
            detail = Monstey_FxWorkflowCall(db, op, args);
            for (const auto &record : deletedRecords)
                state["instances"].as_object().erase(record);
        }
        Monstey_LApply(db, state, schema);
        Monstey_LSave(db, state);
        if (op == "fx_build")
            detail = (declarations ? db.Build(std::filesystem::path(Monstey_Wide(args["output"].as_string())), *declarations)
                                : db.Build(std::filesystem::path(Monstey_Wide(args["output"].as_string()))));
        else if (op == "fx_save_project")
            detail = db.SaveProject(std::filesystem::path(Monstey_Wide(args["path"].as_string())));
    });
    if (!writer)
        for (const auto &[key, value] : stateAfter.as_object())
            detail[key] = value;
    return detail;
}
} // namespace Monstey
