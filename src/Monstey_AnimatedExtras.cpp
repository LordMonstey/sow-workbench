#include "Monstey_AnimatedExtras.hpp"
#include "Monstey_Model.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
struct Monstey_AField {
  const char *argument;
  const char *name;
  unsigned type;
  const char *target;
};
constexpr std::array<Monstey_AField, 8> Monstey_AFields{
    {{"model", "Model", 10, "Model"},
     {"base_animation", "BaseAnimation", 4, ""},
     {"owner_attach_node", "OwnerAttachNode", 4, ""},
     {"start_attached", "StartAttached", 1, ""},
     {"start_visible", "StartVisible", 1, ""},
     {"persistent_fx", "PersistentFX", 10, "Client.ClientFXSequence"},
     {"dissolve_pitch_range", "DissolvePitchRange", 10, "BaseTypes.FloatRange"},
     {"dissolve_param_range", "DissolveParamRange", 7, ""}}};
void Monstey_ARequire(bool ok, const std::string &message) {
  if (!ok)
    throw Monstey_Error("animated_extra", message);
}
const J *Monstey_AFieldIn(const J &record, std::string_view name) {
  const auto hash = Monstey_Hex(Monstey_NativeHash(name));
  for (const auto &field : record.at("fields").as_array())
    if (field.at("hash").as_string() == hash)
      return &field;
  return nullptr;
}
const J &Monstey_ADefinition(const J &schema, const char *name) {
  Monstey_ARequire(schema.contains("categories") && schema.at("categories").contains(name),
                   std::string("Qualified schema lacks ") + name);
  return schema.at("categories").at(name);
}
const J &Monstey_ASchemaField(const J &definition, const char *name, unsigned type,
                              const char *target = "", bool list = false) {
  for (const auto &f : definition.at("fields").as_array())
    if (f.at("name").as_string() == name) {
      const auto actual =
          f.contains("type_code") ? f.at("type_code").as_uint() : f.at("native_type").as_uint();
      Monstey_ARequire(actual == type && f.value("array", 1).as_int() == (list ? -1 : 1),
                       std::string("Native schema shape mismatch: ") + name);
      if (*target)
        Monstey_ARequire(f.value("target", "").as_string() == target,
                         std::string("Native schema reference target mismatch: ") + name);
      return f;
    }
  throw Monstey_Error("animated_extra_schema", std::string("Missing native field: ") + name);
}
void Monstey_ASchema(const J &schema) {
  const auto &d = Monstey_ADefinition(schema, "AnimatedExtra.Object");
  for (const auto &f : Monstey_AFields)
    (void)Monstey_ASchemaField(d, f.name, f.type, f.target);
  (void)Monstey_ASchemaField(Monstey_ADefinition(schema, "Character.Models"), "AttachedAnimExtras",
                             10, "AnimatedExtra.Object", true);
}
J Monstey_ASource(Monstey_Gadb &db, const J &args) {
  const auto d = db.Describe();
  Monstey_ARequire(d.at("format").as_string() == "GADB17", "Animated extras require native GADB17");
  for (const auto pair : {std::pair{"expected_database_sha256", "source_sha256"},
                          std::pair{"expected_schema_sha256", "schema_sha256"}})
    if (args.contains(pair.first))
      Monstey_ARequire(args.at(pair.first) == d.at(pair.second),
                       std::string("Source identity mismatch: ") + pair.first);
  for (const auto pair :
       {std::pair{"source", "source_sha256"}, std::pair{"schema_path", "schema_sha256"}}) {
    const auto path = d.value(pair.first, "").as_string();
    if (!path.empty())
      Monstey_ARequire(Monstey_HashSha256(std::filesystem::path(Monstey_Wide(path))) ==
                           d.at(pair.second).as_string(),
                       std::string("Input bytes changed; reopen session: ") + pair.first);
  }
  return d;
}
J Monstey_ARecord(Monstey_Gadb &db, const std::string &selector, const char *category) {
  auto r = db.Metadata(selector);
  Monstey_ARequire(r.at("category").as_string() == category && !r.at("pending_delete").as_bool(),
                   std::string("Select a live native ") + category + " record");
  const auto categories = db.Categories(category);
  const auto matching =
      std::find_if(categories.at("rows").as_array().begin(), categories.at("rows").as_array().end(),
                   [&](const J &c) { return c.at("index") == r.at("category_index"); });
  auto nativeClass = std::string(category);
  std::replace(nativeClass.begin(), nativeClass.end(), '.', '/');
  if (std::string_view(category) == "AnimatedExtra.Object")
    nativeClass = "AnimatedExtras";
  Monstey_ARequire(matching != categories.at("rows").as_array().end() &&
                       (matching->at("schema").as_string() == nativeClass ||
                        matching->at("schema").as_string() == category),
                   std::string("Native class differs from category label: ") + category);
  return r;
}
J Monstey_AExtra(Monstey_Gadb &db, const J &ref) {
  Monstey_ARequire(ref.is_string(), "Selected slot has no physical animated extra");
  auto r = Monstey_ARecord(db, ref.as_string(), "AnimatedExtra.Object");
  for (const auto &f : Monstey_AFields) {
    const auto field = Monstey_AFieldIn(r, f.name);
    Monstey_ARequire(field && field->at("native_type").as_uint() == f.type &&
                         !field->at("array").as_bool(),
                     std::string("Native record descriptor mismatch: ") + f.name);
    if (*f.target)
      Monstey_ARequire(field->value("target", "").as_string() == f.target,
                       std::string("Native reference target mismatch: ") + f.name);
  }
  return r;
}
J Monstey_ALinks(const J &owner) {
  const auto f = Monstey_AFieldIn(owner, "AttachedAnimExtras");
  if (!f)
    return J::array();
  Monstey_ARequire(f->at("native_type").as_uint() == 10 && f->at("array").as_bool() &&
                       f->at("variable_array").as_bool() && f->at("value").is_array() &&
                       f->value("target", "").as_string() == "AnimatedExtra.Object",
                   "AttachedAnimExtras must be a variable native type10 reference list");
  return f->at("value");
}
void Monstey_AWriteLinks(Monstey_Gadb &db, const J &owner, const J &links, const J &schema) {
  const auto ref = owner.at("reference").as_string();
  if (Monstey_AFieldIn(owner, "AttachedAnimExtras"))
    db.SetField(ref, "AttachedAnimExtras", links);
  else
    db.AddField(ref, "AttachedAnimExtras", 10, links,
                Monstey_ASchemaField(Monstey_ADefinition(schema, "Character.Models"),
                                     "AttachedAnimExtras", 10, "AnimatedExtra.Object", true));
}
J Monstey_AValues(Monstey_Gadb &db, const J &record, const J &args, const J &schema) {
  const auto &d = Monstey_ADefinition(schema, "AnimatedExtra.Object");
  auto out = J::object();
  for (const auto &f : Monstey_AFields) {
    J value;
    if (args.contains(f.argument))
      value = args.at(f.argument);
    else if (!record.empty())
      value = Monstey_AFieldIn(record, f.name)->at("value");
    else {
      const auto &sf = Monstey_ASchemaField(d, f.name, f.type, f.target);
      Monstey_ARequire(sf.contains("default"), std::string("Supply explicit ") + f.argument);
      value = sf.at("default");
      if (f.type == 4)
        Monstey_ARequire(!value.is_null(), std::string("Supply explicit ") + f.argument);
    }
    if (f.type == 4) {
      Monstey_ARequire(value.is_string() && value.as_string().find('\0') == std::string::npos,
                       std::string(f.name) + " requires native UTF-8 text without NUL");
      (void)Monstey_Wide(value.as_string());
    } else if (f.type == 1) {
      Monstey_ARequire(value.is_bool(), std::string(f.name) + " requires a Boolean");
    } else if (f.type == 10) {
      Monstey_ARequire(value.is_null() || value.is_string(),
                       std::string(f.name) + " requires a reference or null");
      if (!value.is_null())
        value = Monstey_ARecord(db, value.as_string(), f.target).at("reference");
    } else {
      Monstey_ARequire(value.is_array() && value.size() == 2,
                       "DissolveParamRange requires two Float32 values");
      for (const auto &n : value.as_array()) {
        const auto number = n.as_number();
        Monstey_ARequire(std::isfinite(number) &&
                             std::abs(number) <= std::numeric_limits<float>::max(),
                         "DissolveParamRange exceeds finite Float32");
      }
    }
    out[f.name] = value;
  }
  return out;
}
std::size_t Monstey_AIndex(const J &args, const char *name, std::size_t count, bool end = false) {
  Monstey_ARequire(args.contains(name), std::string("Missing index: ") + name);
  const auto n = args.at(name).as_uint();
  Monstey_ARequire(n < count || (end && n == count),
                   std::string("Index outside owner list: ") + name);
  return static_cast<std::size_t>(n);
}
void Monstey_AExpectedSlot(const J &args, const J &ref) {
  if (args.contains("expected_reference"))
    Monstey_ARequire(args.at("expected_reference") == ref,
                     "Selected animated-extra slot changed; refresh it");
}
std::string Monstey_APolicy(const J &args) {
  const auto p = args.value("policy", "").as_string();
  Monstey_ARequire(p == "shared" || p == "independent",
                   "Choose explicit shared or independent policy");
  return p;
}
std::uint32_t Monstey_ACategory(Monstey_Gadb &db) {
  const auto cats = db.Categories();
  for (const auto &c : cats.at("rows").as_array())
    if (c.at("name").as_string() == "AnimatedExtra.Object")
      return static_cast<std::uint32_t>(c.at("index").as_uint());
  return static_cast<std::uint32_t>(db.CreateCategory("AnimatedExtras", "AnimatedExtra.Object")
                                        .at("category")
                                        .at("index")
                                        .as_uint());
}
std::string Monstey_AClone(Monstey_Gadb &db, const J &record) {
  const auto ci = static_cast<std::uint32_t>(record.at("category_index").as_uint());
  std::set<std::uint32_t> used;
  for (std::size_t off = 0;; off += 10000) {
    const auto p = db.Records(ci, {}, off, 10000);
    for (const auto &r : p.at("rows").as_array())
      if (r.at("named").as_bool())
        used.insert(Monstey_NativeHash(r.at("name").as_string()));
    if (off + p.at("rows").size() >= p.at("total").as_uint())
      break;
  }
  const auto stem = "Monstey_AnimatedExtra_" +
                    std::to_string(db.Resolve(record.at("reference").as_string())) + "_";
  std::string name;
  std::uint64_t n = 0;
  do {
    name = stem + std::to_string(n++);
  } while (used.contains(Monstey_NativeHash(name)));
  return db.CloneRecord(record.at("reference").as_string(), name)
      .at("clone")
      .at("reference")
      .as_string();
}
struct Monstey_ARig {
  std::filesystem::path path;
  std::string hash;
  J report = J::object({{"provided", false},
                        {"owner_attach_node_verified", false},
                        {"full_attachment_rig_verified", false}});
  void Unchanged() const {
    if (!path.empty())
      Monstey_ARequire(Monstey_HashSha256(path) == hash, "Supplied SKEL changed during operation");
  }
};
Monstey_ARig Monstey_ACheckRig(Monstey_Gadb &db, const J &args, const J &values, const J &owner) {
  Monstey_ARig check;
  if (!args.contains("skeleton")) {
    Monstey_ARequire(!args.contains("expected_skeleton_sha256") &&
                         !args.contains("skeleton_resource") &&
                         !args.value("require_attach_node", false).as_bool(),
                     "Rig validation options require a supplied SKEL");
    return check;
  }
  check.path = std::filesystem::path(Monstey_Wide(args.at("skeleton").as_string()));
  const auto raw = Monstey_ReadFile(check.path);
  check.hash = Monstey_HashSha256(raw);
  if (args.contains("expected_skeleton_sha256"))
    Monstey_ARequire(args.at("expected_skeleton_sha256").as_string() == check.hash,
                     "Supplied skeleton SHA256 mismatch");
  const auto rig = Monstey_ModelInspectSkeleton(raw);
  const auto node = values.at("OwnerAttachNode").as_string();
  const auto &names = rig.at("bone_names").as_array();
  const auto matches =
      std::count_if(names.begin(), names.end(), [&](const J &v) { return v.as_string() == node; });
  const bool required = args.value("require_attach_node", false).as_bool();
  if (required)
    Monstey_ARequire(matches == 1,
                     "OwnerAttachNode must resolve exactly once in supplied complete rig");
  bool resource = false;
  if (args.contains("skeleton_resource")) {
    const auto modelRef = Monstey_AFieldIn(owner, "ModelRecord");
    Monstey_ARequire(modelRef && modelRef->at("value").is_string(),
                     "Owner lacks ModelRecord skeleton route");
    const auto model = Monstey_ARecord(db, modelRef->at("value").as_string(), "Model");
    const auto file = Monstey_AFieldIn(model, "ModelFile");
    Monstey_ARequire(file && file->at("value").is_string(), "Owner ModelRecord lacks ModelFile");
    auto normalize = [](std::string s) {
      std::replace(s.begin(), s.end(), '/', '\\');
      return s;
    };
    Monstey_ARequire(normalize(args.at("skeleton_resource").as_string()) ==
                         normalize(file->at("value").as_string()),
                     "Supplied logical skeleton resource differs from owner ModelRecord.ModelFile");
    resource = true;
  }
  check.report = J::object({{"provided", true},
                            {"path", Monstey_Utf8(check.path.wstring())},
                            {"sha256", check.hash},
                            {"format", "SKEL17"},
                            {"bone_count", names.size()},
                            {"owner_attach_node", node},
                            {"owner_attach_node_matches", static_cast<std::uint64_t>(matches)},
                            {"owner_attach_node_verified", matches == 1},
                            {"owner_attach_node_membership_required", required},
                            {"status", matches == 1 ? "present_in_supplied_skel"
                                                    : "not_resolved_in_supplied_skin_skeleton"},
                            {"full_attachment_rig_verified", false},
                            {"model_resource_path_verified", resource},
                            {"animation_library_verified", false}});
  check.Unchanged();
  return check;
}
J Monstey_ASummary(Monstey_Gadb &db, const J &record, bool usage) {
  auto out = record;
  for (const auto &f : Monstey_AFields)
    out[f.argument] = Monstey_AFieldIn(record, f.name)->at("value");
  if (usage)
    out["incoming_references"] = db.IncomingReferences(record.at("reference").as_string(), 0, 250);
  return out;
}
bool Monstey_AMatches(const J &row, const std::string &query) {
  if (query.empty())
    return true;
  for (const auto key : {"name", "owner_attach_node", "base_animation", "reference"})
    if (row.contains(key) && row.at(key).is_string() &&
        row.at(key).as_string().find(query) != std::string::npos)
      return true;
  return false;
}
} // namespace
J Monstey_AnimatedExtrasCatalog() {
  const auto text = J::object({{"type", "string"}}),
             integer = J::object({{"type", "integer"}, {"minimum", 0}}),
             boolean = J::object({{"type", "boolean"}}),
             reference = J::object({{"type", J::array({"string", "null"})}});
  auto rows = J::array();
  for (const char *name : {"animated_extras_list", "animated_extra_create", "animated_extra_edit",
                           "animated_extra_copy_to_owner", "animated_extra_remove"}) {
    const auto op = std::string_view(name);
    auto props = J::object({{"expected_database_sha256", text}, {"expected_schema_sha256", text}}),
         required = J::array();
    if (op != "animated_extra_copy_to_owner")
      props["owner"] = text;
    if (op != "animated_extra_remove") {
      props["skeleton"] = text;
      props["skeleton_resource"] = text;
      props["expected_skeleton_sha256"] = text;
      props["require_attach_node"] = boolean;
    }
    if (op == "animated_extras_list") {
      props["query"] = text;
      props["offset"] = integer;
      props["limit"] = integer;
      props["include_usage"] = boolean;
    } else {
      props["index"] = integer;
      if (op != "animated_extra_remove")
        for (const auto &f : Monstey_AFields)
          props[f.argument] = f.type == 10  ? reference
                              : f.type == 1 ? boolean
                              : f.type == 7
                                  ? J::object({{"type", "array"},
                                               {"minItems", 2},
                                               {"maxItems", 2},
                                               {"items", J::object({{"type", "number"}})}})
                                  : text;
      if (op == "animated_extra_create") {
        props["name"] = text;
        required = J::array({"owner", "model", "base_animation", "owner_attach_node"});
      } else if (op == "animated_extra_copy_to_owner") {
        props["source_owner"] = text;
        props["source_index"] = integer;
        props["target_owner"] = text;
        required = J::array({"source_owner", "source_index", "target_owner", "policy"});
      } else
        required = J::array({"owner", "index"});
      if (op == "animated_extra_edit" || op == "animated_extra_copy_to_owner") {
        props["policy"] =
            J::object({{"type", "string"}, {"enum", J::array({"shared", "independent"})}});
        if (op == "animated_extra_edit")
          required.as_array().push_back("policy");
      }
      if (op != "animated_extra_create")
        props["expected_reference"] = reference;
      if (op == "animated_extra_remove")
        props["delete_orphan"] = boolean;
    }
    rows.as_array().push_back(J::object(
        {{"name", name},
         {"read_only", op == "animated_extras_list"},
         {"description",
          "Native Character.Models AttachedAnimExtras authoring in the shared GADB session."},
         {"inputSchema", J::object({{"type", "object"},
                                    {"properties", props},
                                    {"required", required},
                                    {"additionalProperties", false}})}}));
  }
  return rows;
}
J Monstey_AnimatedExtrasCall(Monstey_Gadb &db, std::string_view operation, const J &args) {
  Monstey_ARequire(args.is_object(), "Animated extra arguments must be an object");
  const auto catalog = Monstey_AnimatedExtrasCatalog();
  const auto op = std::find_if(catalog.as_array().begin(), catalog.as_array().end(),
                               [&](const J &v) { return v.at("name").as_string() == operation; });
  Monstey_ARequire(op != catalog.as_array().end(), "Unknown animated-extra operation");
  const auto &schemaInput = op->at("inputSchema");
  for (const auto &[key, value] : args.as_object()) {
    (void)value;
    Monstey_ARequire(schemaInput.at("properties").contains(key),
                     "Unknown animated-extra argument: " + key);
  }
  for (const auto &key : schemaInput.at("required").as_array())
    Monstey_ARequire(args.contains(key.as_string()),
                     "Missing required argument: " + key.as_string());
  const auto source = Monstey_ASource(db, args), schema = db.Schema();
  Monstey_ASchema(schema);
  if (!args.contains("skeleton"))
    Monstey_ARequire(!args.contains("expected_skeleton_sha256") &&
                         !args.contains("skeleton_resource") &&
                         !args.value("require_attach_node", false).as_bool(),
                     "Rig validation options require a supplied SKEL");
  if (operation == "animated_extras_list") {
    const auto offset = args.value("offset", 0).as_uint(),
               limit = args.value("limit", 250).as_uint();
    Monstey_ARequire(limit <= 10000, "Animated-extra page exceeds 10000 rows");
    const bool usage = args.value("include_usage", true).as_bool();
    const auto query = args.value("query", "").as_string();
    auto rows = J::array(), owner = J::object();
    std::uint64_t total = 0;
    const auto emit = [&](const J &ref, const J &ownerRecord, std::uint64_t index) {
      auto row = ref.is_null() ? J::object({{"reference", nullptr}})
                               : Monstey_ASummary(db, Monstey_AExtra(db, ref), false);
      row["index"] = index;
      if (!Monstey_AMatches(row, query))
        return;
      const auto ordinal = total++;
      if (ordinal < offset || rows.size() >= limit)
        return;
      if (usage && !ref.is_null())
        row["incoming_references"] = db.IncomingReferences(ref.as_string(), 0, 250);
      if (!ref.is_null() && args.contains("skeleton")) {
        const auto record = Monstey_AExtra(db, ref);
        const auto check = Monstey_ACheckRig(
            db, args, Monstey_AValues(db, record, J::object(), schema), ownerRecord);
        row["skeleton_validation"] = check.report;
        check.Unchanged();
      }
      rows.as_array().push_back(row);
    };
    if (args.contains("owner")) {
      owner = Monstey_ARecord(db, args.at("owner").as_string(), "Character.Models");
      const auto links = Monstey_ALinks(owner);
      if (args.contains("skeleton") && links.empty()) {
        auto inspectArgs = args;
        inspectArgs["require_attach_node"] = false;
        const auto check =
            Monstey_ACheckRig(db, inspectArgs, J::object({{"OwnerAttachNode", ""}}), owner);
        check.Unchanged();
      }
      for (std::size_t i = 0; i < links.size(); ++i)
        emit(links[i], owner, i);
    } else {
      Monstey_ARequire(!args.contains("skeleton") && !args.contains("expected_skeleton_sha256") &&
                           !args.contains("skeleton_resource") &&
                           !args.value("require_attach_node", false).as_bool(),
                       "Library rig validation requires a selected owner");
      const auto cats = db.Categories();
      for (const auto &cat : cats.at("rows").as_array())
        if (cat.at("name").as_string() == "AnimatedExtra.Object") {
          for (std::size_t off = 0;; off += 10000) {
            const auto page =
                db.Records(static_cast<std::uint32_t>(cat.at("index").as_uint()), {}, off, 10000);
            for (const auto &r : page.at("rows").as_array())
              if (!r.at("pending_delete").as_bool())
                emit(r.at("reference"), J::object(), 0);
            if (off + page.at("rows").size() >= page.at("total").as_uint())
              break;
          }
        }
    }
    (void)Monstey_ASource(db, args);
    return J::object({{"source", source},
                      {"owner", owner},
                      {"rows", rows},
                      {"total", total},
                      {"offset", offset},
                      {"limit", limit},
                      {"runtime_verified", false},
                      {"animation_merge_verified", false}});
  }
  auto owner = Monstey_ARecord(
      db,
      args.at(operation == "animated_extra_copy_to_owner" ? "target_owner" : "owner").as_string(),
      "Character.Models");
  auto links = Monstey_ALinks(owner), detail = J::object();
  Monstey_ARig rig;
  const auto result = db.Transaction([&](Monstey_Gadb &) {
    if (operation == "animated_extra_remove") {
      const auto index = Monstey_AIndex(args, "index", links.size());
      const auto ref = links[index];
      Monstey_AExpectedSlot(args, ref);
      if (!ref.is_null())
        (void)Monstey_AExtra(db, ref);
      links.as_array().erase(links.as_array().begin() + static_cast<std::ptrdiff_t>(index));
      Monstey_AWriteLinks(db, owner, links, schema);
      detail = J::object({{"reference", ref}, {"removed_index", index}, {"record_deleted", false}});
      if (args.value("delete_orphan", false).as_bool() && !ref.is_null()) {
        Monstey_ARequire(db.IncomingReferences(ref.as_string(), 0, 1).at("total").as_uint() == 0,
                         "Animated extra is still referenced; choose unlink without delete_orphan");
        db.DeleteRecord(ref.as_string());
        detail["record_deleted"] = true;
      }
    } else {
      auto original = J::object();
      std::size_t index = links.size();
      std::string policy = "independent";
      if (operation == "animated_extra_edit") {
        index = Monstey_AIndex(args, "index", links.size());
        Monstey_AExpectedSlot(args, links[index]);
        original = Monstey_AExtra(db, links[index]);
        policy = Monstey_APolicy(args);
      } else if (operation == "animated_extra_copy_to_owner") {
        const auto donor =
            Monstey_ARecord(db, args.at("source_owner").as_string(), "Character.Models");
        const auto donorLinks = Monstey_ALinks(donor);
        const auto sourceIndex = Monstey_AIndex(args, "source_index", donorLinks.size());
        Monstey_AExpectedSlot(args, donorLinks[sourceIndex]);
        original = Monstey_AExtra(db, donorLinks[sourceIndex]);
        policy = Monstey_APolicy(args);
        if (policy == "shared")
          for (const auto &f : Monstey_AFields)
            Monstey_ARequire(!args.contains(f.argument),
                             "Shared copy cannot change source values; choose independent");
        if (args.contains("index"))
          index = Monstey_AIndex(args, "index", links.size(), true);
      } else if (args.contains("index"))
        index = Monstey_AIndex(args, "index", links.size(), true);
      const auto values = Monstey_AValues(db, original, args, schema);
      rig = Monstey_ACheckRig(db, args, values, owner);
      std::string ref;
      if (original.empty()) {
        const auto name = args.value("name", "").as_string();
        Monstey_ARequire(name.find('\0') == std::string::npos, "Record name cannot contain NUL");
        (void)Monstey_Wide(name);
        ref = db.CreateRecord(Monstey_ACategory(db), name, name.empty())
                  .at("record")
                  .at("reference")
                  .as_string();
        for (const auto &f : Monstey_AFields)
          db.AddField(ref, f.name, static_cast<std::uint8_t>(f.type), values.at(f.name),
                      Monstey_ASchemaField(Monstey_ADefinition(schema, "AnimatedExtra.Object"),
                                           f.name, f.type, f.target));
      } else {
        ref = original.at("reference").as_string();
        detail["source_reference"] = ref;
        detail["incoming_references_before"] = db.IncomingReferences(ref, 0, 250);
        if (policy == "independent")
          ref = Monstey_AClone(db, original);
        for (const auto &f : Monstey_AFields)
          if (args.contains(f.argument))
            db.SetField(ref, f.name, values.at(f.name));
      }
      if (operation == "animated_extra_edit")
        links[index] = ref;
      else
        links.as_array().insert(links.as_array().begin() + static_cast<std::ptrdiff_t>(index), ref);
      Monstey_AWriteLinks(db, owner, links, schema);
      detail["reference"] = ref;
      detail["index"] = index;
      detail["policy"] = policy;
      detail["extra"] = Monstey_ASummary(db, Monstey_AExtra(db, J(ref)), true);
      detail["skeleton_validation"] = rig.report;
      detail["unknown_source_fields_preserved"] = !original.empty();
      detail["old_unlinked_record_retained"] = !original.empty() && policy == "independent";
    }
    detail["owner_reference"] = owner.at("reference");
    detail["owner_name"] = owner.at("name");
    rig.Unchanged();
    (void)Monstey_ASource(db, args);
  });
  auto out = result;
  out["operation"] = std::string(operation);
  out["result"] = detail;
  out["game_written"] = false;
  out["runtime_verified"] = false;
  out["animation_merge_verified"] = false;
  return out;
}
} // namespace Monstey
