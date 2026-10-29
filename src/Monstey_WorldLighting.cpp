#include "Monstey_WorldLighting.hpp"
#include "Monstey_Archive.hpp"
#include "Monstey_Gadb.hpp"
#include "Monstey_World.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <map>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
J Monstey_LSchema() {
    auto fields = [](std::initializer_list<const char *> names) {
        J::Array result;
        for (auto name : names)
            result.push_back(J::object({{"name", name}}));
        return J::object({{"fields", result}});
    };
    auto schema = J::object(
        {{"categories",
          J::object({{"FxGlobalProps", fields({"KeyList", "FXName", "ResourceLenMS", "MaxUpdateDistance",
                                               "PauseWhenNotVisible"})},
                     {"ClientFX.DynaLight",
                      fields({"Type", "StartTime", "EndTime", "Radius", "InnerRadius", "Intensity",
                              "FlickerScale", "COLOR", "TranslucentColor", "SpecularColor", "Offset",
                              "RotationOffset", "Rotation", "RotationIntegral", "RelativeLight",
                              "FadeStartDist", "FadeEndDist", "FOVx", "FOVY"})}})}});
    schema["categories"]["FxGlobalProps"]["fields"][0]["array"] = -1;
    return schema;
}
const J &Monstey_LField(const J &metadata, std::string_view name) {
    auto hash = Monstey_Hex(Monstey_NativeHash(name));
    for (auto &field : metadata.at("fields").as_array())
        if (field.at("hash").as_string() == hash)
            return field;
    throw Monstey_Error("world_light_contract", "Native light field is absent: " + std::string(name));
}
void Monstey_LKind(const J &metadata, std::string_view name, std::uint32_t kind) {
    auto &field = Monstey_LField(metadata, name);
    Monstey_Require(field.at("native_type").as_uint() == kind && field.at("count").as_uint() == 1,
                    "Unexpected native light type/count: " + std::string(name));
}
std::filesystem::path Monstey_LPath(const J &v) {
    return std::filesystem::absolute(std::filesystem::path(Monstey_Wide(v.as_string())));
}
std::string Monstey_LText(const std::filesystem::path &p) { return Monstey_Utf8(p.wstring()); }
double Monstey_LNumber(const J &spec, const char *name, double fallback) {
    auto v = spec.value(name, fallback);
    Monstey_Require(v.is_number() && std::isfinite(v.as_number()), "Light number must be finite");
    return v.as_number();
}
std::uint32_t Monstey_LRound(double value) {
    auto lo = std::floor(value), fraction = value - lo;
    if (fraction > 0.5 || (fraction == 0.5 && std::fmod(lo, 2.0) != 0))
        ++lo;
    Monstey_Require(lo >= 0 && lo <= 0xffffffffull, "Rounded light value exceeds DWORD");
    return static_cast<std::uint32_t>(lo);
}
J Monstey_LBuild(const J &a) {
    for (auto &[key, value] : a.as_object()) {
        (void)value;
        Monstey_Require(key == "source" || key == "spec" || key == "output",
                        "Unknown light operation argument");
    }
    auto source = Monstey_LPath(a.at("source")), output = Monstey_LPath(a.at("output"));
    auto raw = Monstey_ReadFile(source);
    auto source_hash = Monstey_HashSha256(raw);
    auto spec = a.at("spec");
    static const std::set<std::string> keys{"source_sha256",   "resource_path", "type",           "radius_cm",
                                            "inner_radius_cm", "intensity",     "period_seconds", "flicker",
                                            "color_rgb",       "fov_degrees"};
    for (auto &[key, value] : spec.as_object()) {
        (void)value;
        Monstey_Require(keys.contains(key), "Unknown light recipe field");
    }
    Monstey_Require(spec.at("source_sha256").as_string() == source_hash, "Stale native light donor");
    auto resource = Monstey_ArchiveVirtual(spec.at("resource_path").as_string());
    Monstey_Require(resource.starts_with("fx/monstey/") && resource.ends_with(".fxp"),
                    "Use a private fx/monstey/*.fxp resource");
    auto radius = Monstey_LNumber(spec, "radius_cm", 0), inner = Monstey_LNumber(spec, "inner_radius_cm", 0),
         intensity = Monstey_LNumber(spec, "intensity", 4),
         seconds = Monstey_LNumber(spec, "period_seconds", 2), flicker = Monstey_LNumber(spec, "flicker", 0);
    Monstey_Require(radius > 0 && radius <= 1e9 && inner >= 0 && inner < radius && intensity >= 0 &&
                        intensity <= 1e6 && seconds >= 0.01 && seconds <= 86400 && flicker >= 0 &&
                        flicker <= 1e6,
                    "Invalid native light radius/intensity/period");
    auto color = spec.at("color_rgb");
    Monstey_Require(color.is_array() && color.size() == 3, "RGB requires exactly three channels");
    std::uint32_t argb = 0xff000000u;
    for (std::size_t i = 0; i < 3; ++i) {
        auto n = color[i].as_number();
        Monstey_Require(std::isfinite(n) && n >= 0 && n <= 1,
                        "RGB channels must be normalized finite numbers");
        argb |= Monstey_LRound(n * 255) << (16 - 8 * i);
    }
    auto schema = Monstey_LSchema();
    auto db = Monstey_Gadb::WithSchema(source, schema);
    Monstey_Require(db->Describe().at("format").as_string() == "LTFX8/GADB17",
                    "World light donor must be native LTFX8");
    std::optional<std::string> global, light;
    auto categories = db->Categories();
    for (auto &category : categories.at("rows").as_array()) {
        auto name = category.at("name").as_string();
        if (name != "FxGlobalProps" && name != "ClientFX.DynaLight")
            continue;
        auto rows = db->Records(static_cast<std::uint32_t>(category.at("index").as_uint()), "", 0, 2);
        Monstey_Require(rows.at("total").as_uint() == 1,
                        "Choose a native donor with exactly one global/light record");
        (name == "FxGlobalProps" ? global : light) = rows.at("rows")[0].at("native_reference").as_string();
    }
    Monstey_Require(global.has_value() && light.has_value(),
                    "Native donor needs one FxGlobalProps and one DynaLight");
    auto before_global = db->Metadata(*global), before_light = db->Metadata(*light);
    auto key_field = Monstey_LField(before_global, "KeyList");
    Monstey_Require(
        (key_field.at("native_type").as_uint() == 10 || key_field.at("native_type").as_uint() == 11) &&
            key_field.at("value").is_array(),
        "Native KeyList is not a reference list");
    auto active = key_field.at("value");
    Monstey_Require(std::find(active.as_array().begin(), active.as_array().end(), J(*light)) !=
                        active.as_array().end(),
                    "DynaLight is not an active native key");
    auto type_field = Monstey_LField(before_light, "Type");
    Monstey_Require(type_field.at("count").as_uint() == 1 && (type_field.at("native_type").as_uint() == 4 ||
                                                              type_field.at("native_type").as_uint() == 5),
                    "Native Type must be one enum/text token");
    auto type = type_field.at("value").as_string(), requested = spec.value("type", type).as_string();
    Monstey_Require((type == "Point" || type == "Spot") && requested == type,
                    "Use a native donor of the requested Point/Spot type");
    std::set<std::pair<std::string, std::string>> changed;
    J::Array changes;
    auto set = [&](const std::string &selector, std::string_view name, std::uint32_t kind, const J &value) {
        const auto &before = selector == *global ? before_global : before_light;
        Monstey_LKind(before, name, kind);
        db->SetField(selector, name, value);
        auto hash = Monstey_Hex(Monstey_NativeHash(name));
        changed.emplace(selector, hash);
        changes.push_back(J::object({{"reference", selector},
                                     {"field", std::string(name)},
                                     {"native_type", kind},
                                     {"before", Monstey_LField(before, name).at("value")},
                                     {"after", value}}));
    };
    db->SetField(*global, "KeyList", J::array({*light}));
    changed.emplace(*global, key_field.at("hash").as_string());
    auto native_path = resource;
    std::replace(native_path.begin(), native_path.end(), '/', '\\');
    set(*global, "FXName", 4, native_path);
    set(*global, "ResourceLenMS", 3, Monstey_LRound(seconds * 1000));
    set(*global, "MaxUpdateDistance", 2, std::max(radius * 4, 5000.0));
    set(*global, "PauseWhenNotVisible", 1, false);
    set(*light, "StartTime", 2, 0);
    set(*light, "EndTime", 2, seconds);
    for (auto &[name, value] : std::map<std::string, double>{
             {"Radius", radius}, {"InnerRadius", inner}, {"Intensity", intensity}, {"FlickerScale", flicker}})
        set(*light, name, 13, J::object({{"constant", value}}));
    set(*light, "COLOR", 12, J::object({{"constant", argb}}));
    set(*light, "TranslucentColor", 3, static_cast<std::int64_t>(std::bit_cast<std::int32_t>(argb)));
    set(*light, "SpecularColor", 3, static_cast<std::int64_t>(std::bit_cast<std::int32_t>(argb)));
    for (auto name : {"Offset", "RotationOffset", "Rotation", "RotationIntegral"})
        set(*light, name, 16, J::object({{"constant", J::array({0, 0, 0})}}));
    set(*light, "RelativeLight", 1, false);
    set(*light, "FadeStartDist", 2, 0);
    set(*light, "FadeEndDist", 2, 0);
    if (type == "Spot") {
        auto fov = Monstey_LNumber(spec, "fov_degrees", 70);
        Monstey_Require(fov > 0 && fov < 170, "Spot FOV must be between0 and170 degrees");
        set(*light, "FOVx", 13, J::object({{"constant", fov}}));
        set(*light, "FOVY", 13, J::object({{"constant", fov}}));
    }
    std::array<std::filesystem::path, 1> sources{source};
    Monstey_RequireOfflineOutput(output, sources);
    Monstey_Require(!std::filesystem::exists(Monstey_WindowsFilePath(output)),
                    "Light component output already exists");
    Monstey_Require(Monstey_HashSha256(source) == source_hash,
                    "Native light donor changed during validation");
    std::filesystem::create_directories(Monstey_WindowsFilePath(output.parent_path()));
    Monstey_Require(std::filesystem::create_directory(Monstey_WindowsFilePath(output)),
                    "Cannot create new light component");
    auto target = output / "assets" / std::filesystem::path(Monstey_Wide(resource));
    std::vector<std::filesystem::path> published;
    J report;
    try {
        auto build = db->Build(target);
        published.push_back(target);
        published.emplace_back(Monstey_Wide(build.at("report_path").as_string()));
        published.emplace_back(Monstey_Wide(build.at("namemap_path").as_string()));
        auto checked = Monstey_Gadb::WithSchema(target, schema);
        auto final_global = checked->Metadata(*global), final_light = checked->Metadata(*light);
        Monstey_Require(Monstey_LField(final_global, "KeyList").at("value") == J::array({*light}),
                        "Authored recipe contains active nonlight keys");
        Monstey_Require(Monstey_LField(final_light, "Type").at("value").as_string() == type,
                        "Native light type changed");
        for (auto &old : {before_global, before_light}) {
            auto selector = old.at("selector").as_string();
            auto now = checked->Metadata(selector);
            for (auto &field : old.at("fields").as_array())
                if (!changed.contains({selector, field.at("hash").as_string()})) {
                    auto found =
                        std::find_if(now.at("fields").as_array().begin(), now.at("fields").as_array().end(),
                                     [&](const auto &f) { return f.at("hash") == field.at("hash"); });
                    Monstey_Require(found != now.at("fields").as_array().end() &&
                                        found->at("value") == field.at("value"),
                                    "Unedited native light field changed");
                }
        }
        auto bytes = Monstey_ReadFile(target);
        report =
            J::object({{"status", "NATIVE_DYNALIGHT_RECIPE_AUTHORED"},
                       {"resources", J::array({J::object({{"resource_path", resource},
                                                          {"path", Monstey_LText(target)},
                                                          {"bytes", static_cast<std::uint64_t>(bytes.size())},
                                                          {"sha256", Monstey_HashSha256(bytes)},
                                                          {"role", "native_point_or_spot_light"}})})},
                       {"donor", J::object({{"path", Monstey_LText(source)},
                                            {"bytes", static_cast<std::uint64_t>(raw.size())},
                                            {"sha256", source_hash}})},
                       {"type", type},
                       {"active_key_list", J::array({*light})},
                       {"active_categories", J::array({"ClientFX.DynaLight"})},
                       {"radius_cm", radius},
                       {"inner_radius_cm", inner},
                       {"intensity", intensity},
                       {"color_argb", Monstey_Hex(argb)},
                       {"color_contract", "Native packed ARGB channels /255; intensity is separate"},
                       {"period_seconds", seconds},
                       {"curves_constant", true},
                       {"world_owner_contract", J::object({{"class_name", "SpecialFX"},
                                                           {"FXName", native_path},
                                                           {"Loop", true},
                                                           {"StartOn", true},
                                                           {"BubbleRadius", std::max(radius * 4, 5000.0)}})},
                       {"changes", changes},
                       {"native_build_validation", build},
                       {"qualified_schema_sha256", db->Describe().at("schema_sha256")},
                       {"native_shadow_and_projected_texture_policy_preserved", true},
                       {"runtime_verified", false},
                       {"game_written", false}});
        auto manifest = output / "resource-manifest.json";
        Monstey_WriteNewText(manifest, report.dump(2) + "\n");
        published.push_back(manifest);
        Monstey_Require(Monstey_HashSha256(source) == source_hash,
                        "Native light donor changed during publication");
    } catch (...) {
        std::error_code ec;
        for (auto it = published.rbegin(); it != published.rend(); ++it)
            std::filesystem::remove(Monstey_WindowsFilePath(*it), ec);
        std::vector<std::filesystem::path> dirs;
        auto parent = target.parent_path();
        while (parent != output && parent.wstring().size() > output.wstring().size()) {
            dirs.push_back(parent);
            parent = parent.parent_path();
        }
        for (auto &dir : dirs)
            std::filesystem::remove(Monstey_WindowsFilePath(dir), ec);
        std::filesystem::remove(Monstey_WindowsFilePath(output), ec);
        throw;
    }
    return report;
}
} // namespace
J Monstey_WorldLightInspect(const std::filesystem::path &source,
    std::string_view resource, std::span<const std::uint8_t> owner,
    std::string_view owner_name) {
    const auto raw = Monstey_ReadFile(source);
    const auto hash = Monstey_HashSha256(raw);
    auto db = Monstey_Gadb::WithSchema(source, Monstey_LSchema());
    Monstey_Require(db->Describe().at("format").as_string() == "LTFX8/GADB17",
        "Light candidate must be native LTFX8/GADB17");
    std::optional<std::string> global, light;
    const auto categories=db->Categories();
    for (const auto &category : categories.at("rows").as_array()) {
        const auto &name = category.at("name").as_string();
        if (name != "FxGlobalProps" && name != "ClientFX.DynaLight") continue;
        const auto rows = db->Records(static_cast<std::uint32_t>(category.at("index").as_uint()), "", 0, 2);
        Monstey_Require(rows.at("total").as_uint() == 1,
            "Static light needs exactly one global and one DynaLight");
        auto &slot = name == "FxGlobalProps" ? global : light;
        Monstey_Require(!slot.has_value(), "Repeated native light schema category");
        slot = rows.at("rows")[0].at("native_reference").as_string();
    }
    Monstey_Require(global.has_value() && light.has_value(), "Native light records absent");
    const auto gm = db->Metadata(*global), lm = db->Metadata(*light);
    const auto &keys = Monstey_LField(gm,"KeyList");
    Monstey_Require((keys.at("native_type").as_uint()==10 || keys.at("native_type").as_uint()==11) &&
        keys.at("value") == J::array({*light}), "Light must be the sole active native FX key");
    const auto &type = Monstey_LField(lm,"Type");
    Monstey_Require(type.at("count").as_uint()==1 &&
        (type.at("native_type").as_uint()==4 || type.at("native_type").as_uint()==5) &&
        (type.at("value").as_string()=="Point" || type.at("value").as_string()=="Spot"),
        "Static light is not qualified Point/Spot");
    const auto resource_path = Monstey_ArchiveVirtual(std::string(resource));
    Monstey_LKind(gm,"FXName",4);
    const auto fx = Monstey_LField(gm,"FXName").at("value").as_string();
    Monstey_Require(fx.find('/')==std::string::npos && Monstey_ArchiveVirtual(fx)==resource_path,
        "Native FXName must match its canonical backslash resource");
    J::Object curves;
    for (const auto *field : {"Radius","InnerRadius","Intensity","FlickerScale"}) {
        Monstey_LKind(lm,field,13);
        curves.emplace(field,Monstey_LField(lm,field).at("value"));
    }
    Monstey_LKind(lm,"COLOR",12);
    const auto owners = Monstey_ObjresOwners(owner);
    std::optional<J> selected;
    for (const auto &entry : owners.as_array())
        if (entry.at("name").as_string()==owner_name) {
            Monstey_Require(!selected, "Duplicate light owner name"); selected=entry;
        }
    Monstey_Require(selected && selected->at("class_name").as_string()=="SpecialFX",
        "Light owner must be an existing native SpecialFX");
    const auto &values=selected->at("property_values");
    const auto &owner_fx=values.at(Monstey_Hex(Monstey_NativeHash("FXName"))).as_string();
    Monstey_Require(owner_fx.find('/')==std::string::npos &&
        Monstey_ArchiveVirtual(owner_fx)==resource_path &&
        values.at(Monstey_Hex(Monstey_NativeHash("Loop"))).as_bool() &&
        values.at(Monstey_Hex(Monstey_NativeHash("StartOn"))).as_bool(),
        "Native light owner FXName/Loop/StartOn contract differs");
    Monstey_Require(Monstey_HashSha256(source)==hash,"Light source changed while inspected");
    return J::object({{"status","NATIVE_LIGHT_OWNER_CONTRACT_VALIDATED_OFFLINE"},
        {"format","LTFX8/GADB17"},{"sha256",hash},{"type",type.at("value")},
        {"active_keys",keys.at("value")},{"curves",curves},{"color",Monstey_LField(lm,"COLOR").at("value")},
        {"owner_name",std::string(owner_name)},{"owner_class","SpecialFX"},
        {"resource_path",resource_path},{"runtime_verified",false},
        {"light_visible_in_game","NOT_EVALUATED"}});
}
J Monstey_WorldLightingCatalog() {
    auto number = J::object({{"type", "number"}}), path = J::object({{"type", "string"}, {"format", "path"}}),
         text = J::object({{"type", "string"}});
    auto positive = number;
    positive["exclusiveMinimum"] = 0;
    positive["maximum"] = 1000000000;
    auto nonnegative = number;
    nonnegative["minimum"] = 0;
    nonnegative["maximum"] = 1000000;
    auto channel = number;
    channel["minimum"] = 0;
    channel["maximum"] = 1;
    auto spec = J::object(
        {{"type", "object"},
         {"properties",
          J::object(
              {{"source_sha256", text},
               {"resource_path", text},
               {"type", J::object({{"type", "string"}, {"enum", J::array({"Point", "Spot"})}})},
               {"radius_cm", positive},
               {"inner_radius_cm", nonnegative},
               {"intensity", nonnegative},
               {"period_seconds", J::object({{"type", "number"}, {"minimum", 0.01}, {"maximum", 86400}})},
               {"flicker", nonnegative},
               {"color_rgb",
                J::object({{"type", "array"}, {"items", channel}, {"minItems", 3}, {"maxItems", 3}})},
               {"fov_degrees",
                J::object({{"type", "number"}, {"exclusiveMinimum", 0}, {"exclusiveMaximum", 170}})}})},
         {"required", J::array({"source_sha256", "resource_path", "radius_cm", "color_rgb"})},
         {"additionalProperties", false}});
    auto schema = J::object({{"type", "object"},
                             {"properties", J::object({{"source", path}, {"spec", spec}, {"output", path}})},
                             {"required", J::array({"source", "spec", "output"})},
                             {"additionalProperties", false}});
    return J::array(
        {J::object({{"name", "sow_world_light_build"},
                    {"description", "Native Point/Spot DynaLight recipe and SpecialFX owner contract"},
                    {"status", "complete"},
                    {"available", true},
                    {"implemented", true},
                    {"inputSchema", schema},
                    {"implementation_language", "C++20"},
                    {"runtime_verified", false}})});
}
J Monstey_WorldLightingCall(std::string_view name, const J &a) {
    Monstey_Require(a.is_object(), "Expected native light arguments");
    if (name == "sow_world_light_build")
        return Monstey_LBuild(a);
    throw Monstey_Error("world_light_operation", "Unknown native light operation");
}
} // namespace Monstey
