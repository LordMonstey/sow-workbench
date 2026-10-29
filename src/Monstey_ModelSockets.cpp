#include "Monstey_ModelSockets.hpp"
#include "Monstey_Model.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
void Monstey_SRequire(bool value, const std::string &message) {
  if (!value)
    throw Monstey_Error("model_socket", message);
}
std::string Monstey_SText(const J &a, const char *key,
                          std::string fallback = {}) {
  return a.contains(key) ? a.at(key).as_string() : std::move(fallback);
}
const J *Monstey_SField(const J &record, std::string_view name) {
  const auto hash = Monstey_Hex(Monstey_NativeHash(name));
  for (const auto &field : record.at("fields").as_array())
    if (field.at("hash").as_string() == hash)
      return &field;
  return nullptr;
}
J Monstey_SValue(const J &record, std::string_view name) {
  auto f = Monstey_SField(record, name);
  Monstey_SRequire(f != nullptr,
                   "Missing native socket field " + std::string(name));
  return f->at("value");
}
const J &Monstey_SDefinition(const J &schema, const char *name) {
  Monstey_SRequire(schema.contains("categories") &&
                       schema.at("categories").contains(name),
                   std::string("Qualified schema lacks ") + name);
  return schema.at("categories").at(name);
}
const J &Monstey_SSchemaField(const J &definition, const char *name,
                              unsigned type) {
  for (const auto &field : definition.at("fields").as_array())
    if (field.at("name").as_string() == name) {
      auto actual = field.contains("type_code")
                        ? field.at("type_code").as_uint()
                        : field.at("native_type").as_uint();
      Monstey_SRequire(actual == type,
                       std::string("Native schema type mismatch for ") + name);
      return field;
    }
  throw Monstey_Error("model_socket_schema",
                      std::string("Qualified schema lacks field ") + name);
}
J Monstey_SSource(Monstey_Gadb &db, const J &args) {
  auto d = db.Describe();
  Monstey_SRequire(d.at("format").as_string() == "GADB17",
                   "Sockets require the native game GADB17 session");
  for (const auto pair :
       {std::pair{"expected_database_sha256", "source_sha256"},
        std::pair{"expected_schema_sha256", "schema_sha256"}})
    if (args.contains(pair.first))
      Monstey_SRequire(
          args.at(pair.first).as_string() == d.at(pair.second).as_string(),
          std::string("Socket source identity mismatch: ") + pair.first);
  const auto path = d.value("source", "").as_string();
  if (!path.empty())
    Monstey_SRequire(Monstey_HashSha256(std::filesystem::path(Monstey_Wide(
                         path))) == d.at("source_sha256").as_string(),
                     "Source database bytes changed; reopen it");
  return d;
}
J Monstey_SModel(Monstey_Gadb &db, const std::string &selector) {
  auto r = db.Metadata(selector);
  Monstey_SRequire(r.at("category").as_string() == "Model" &&
                       !r.at("pending_delete").as_bool(),
                   "Select a live native Model category record");
  return r;
}
J Monstey_SOverride(Monstey_Gadb &db, const J &ref) {
  Monstey_SRequire(ref.is_string(),
                   "Socket list slot has no physical override record");
  auto r = db.Metadata(ref.as_string());
  Monstey_SRequire(r.at("category").as_string() == "ModelSocketOverride" &&
                       !r.at("pending_delete").as_bool(),
                   "Socket list reference is not a live ModelSocketOverride");
  for (auto pair : {std::pair{"SocketName", 4u}, std::pair{"ParentNode", 4u},
                    std::pair{"PosOffsetFromParent", 8u},
                    std::pair{"RotOffsetFromParent", 9u}}) {
    auto f = Monstey_SField(r, pair.first);
    Monstey_SRequire(f && f->at("native_type").as_uint() == pair.second &&
                         !f->at("array").as_bool(),
                     std::string("Invalid native socket descriptor: ") +
                         pair.first);
  }
  return r;
}
J Monstey_SLinks(const J &model) {
  const auto f = Monstey_SField(model, "SimpleModelSocketOffsets");
  if (!f)
    return J::array();
  Monstey_SRequire(f->at("native_type").as_uint() == 11 &&
                       f->at("value").is_array(),
                   "SimpleModelSocketOffsets must be native type11");
  return f->at("value");
}
std::size_t Monstey_SIndex(const J &a, const char *name, std::size_t count,
                           bool end = false) {
  Monstey_SRequire(a.contains(name),
                   std::string("Missing socket index: ") + name);
  auto n = a.at(name).as_uint();
  Monstey_SRequire(n < count || (end && n == count),
                   std::string("Socket index outside model list: ") + name);
  return static_cast<std::size_t>(n);
}
std::string Monstey_SPolicy(const J &a) {
  const auto p = Monstey_SText(a, "policy");
  Monstey_SRequire(p == "shared" || p == "independent",
                   "Choose explicit shared or independent socket policy");
  return p;
}
void Monstey_SName(const J &value, const char *name) {
  Monstey_SRequire(value.is_string() && !value.as_string().empty() &&
                       value.as_string().find('\0') == std::string::npos,
                   std::string(name) +
                       " needs a nonempty native UTF-8 name without NUL");
  (void)Monstey_Wide(value.as_string());
}
void Monstey_SVector(const J &value, unsigned width, const char *name) {
  Monstey_SRequire(value.is_array() && value.size() == width,
                   std::string(name) +
                       " has the wrong number of native components");
  double norm = 0;
  for (const auto &v : value.as_array()) {
    const auto n = v.as_number();
    Monstey_SRequire(std::isfinite(n) &&
                         std::abs(n) <= std::numeric_limits<float>::max(),
                     std::string(name) + " exceeds finite Float32");
    norm += n * n;
  }
  if (width == 4)
    Monstey_SRequire(std::isfinite(norm) && std::abs(norm - 1.0) < 1e-3,
                     "Native socket quaternion must be unit length; values are "
                     "never normalized silently");
}
J Monstey_SValues(const J &record, const J &args, const J &schema) {
  const auto &definition = Monstey_SDefinition(schema, "ModelSocketOverride");
  auto out = J::object();
  for (auto pair : {std::pair{"socket_name", "SocketName"},
                    std::pair{"parent_node", "ParentNode"},
                    std::pair{"position_cm", "PosOffsetFromParent"},
                    std::pair{"rotation_xyzw", "RotOffsetFromParent"}}) {
    if (args.contains(pair.first))
      out[pair.second] = args.at(pair.first);
    else if (!record.empty())
      out[pair.second] = Monstey_SValue(record, pair.second);
    else {
      const auto type = std::string_view(pair.first) == "position_cm"     ? 8u
                        : std::string_view(pair.first) == "rotation_xyzw" ? 9u
                                                                          : 4u;
      const auto &field = Monstey_SSchemaField(definition, pair.second, type);
      Monstey_SRequire(field.contains("default") &&
                           !field.at("default").is_null(),
                       std::string("Supply explicit ") + pair.first);
      out[pair.second] = field.at("default");
    }
  }
  Monstey_SName(out.at("SocketName"), "SocketName");
  Monstey_SName(out.at("ParentNode"), "ParentNode");
  Monstey_SVector(out.at("PosOffsetFromParent"), 3, "PosOffsetFromParent");
  Monstey_SVector(out.at("RotOffsetFromParent"), 4, "RotOffsetFromParent");
  return out;
}
struct Monstey_SkeletonCheck {
  std::filesystem::path path;
  std::string hash;
  J report = J::object({{"provided", false},
                        {"parent_node_verified", false},
                        {"socket_name_kind", "attachment_label"},
                        {"socket_name_bone_membership_required", false}});
  void Unchanged() const {
    if (!path.empty())
      Monstey_SRequire(Monstey_HashSha256(path) == hash,
                       "Supplied skeleton changed during socket operation");
  }
};
Monstey_SkeletonCheck Monstey_SCheckSkeleton(const J &a, const J &values,
                                             const J &model) {
  Monstey_SkeletonCheck check;
  if (!a.contains("skeleton")) {
    Monstey_SRequire(!a.contains("expected_skeleton_sha256") &&
                         !a.contains("skeleton_resource") &&
                         !a.value("require_parent_node", false).as_bool(),
                     "Skeleton validation options require a supplied SKEL");
    return check;
  }
  check.path =
      std::filesystem::path(Monstey_Wide(a.at("skeleton").as_string()));
  auto raw = Monstey_ReadFile(check.path);
  check.hash = Monstey_HashSha256(raw);
  if (a.contains("expected_skeleton_sha256"))
    Monstey_SRequire(check.hash == a.at("expected_skeleton_sha256").as_string(),
                     "Supplied SKEL SHA256 mismatch");
  const auto rig = Monstey_ModelInspectSkeleton(raw);
  const auto names = rig.at("bone_names");
  const auto parent = values.at("ParentNode").as_string(),
             socket = values.at("SocketName").as_string();
  const auto present = [&](const std::string &name) {
    return std::count_if(names.as_array().begin(), names.as_array().end(),
                         [&](const J &n) { return n.as_string() == name; });
  };
  const auto parentMatches = present(parent), socketMatches = present(socket);
  const bool require = a.value("require_parent_node", false).as_bool();
  if (require)
    Monstey_SRequire(parentMatches == 1,
                     "ParentNode must resolve to exactly one node in the "
                     "supplied complete native rig");
  bool match = false;
  if (a.contains("skeleton_resource")) {
    const auto field = Monstey_SField(model, "ModelFile");
    Monstey_SRequire(field && field->at("value").is_string(),
                     "Model lacks its native skeleton resource path");
    Monstey_SRequire(
        Monstey_PathHash(a.at("skeleton_resource").as_string()) ==
            Monstey_PathHash(field->at("value").as_string()),
        "Supplied logical skeleton resource differs from Model.ModelFile");
    auto normalize = [](std::string path) {
      std::replace(path.begin(), path.end(), '/', '\\');
      return path;
    };
    Monstey_SRequire(
        normalize(a.at("skeleton_resource").as_string()) ==
            normalize(field->at("value").as_string()),
        "Skeleton resource hash collision or different native path");
    match = true;
  }
  check.report = J::object(
      {{"provided", true},
       {"path", Monstey_Utf8(check.path.wstring())},
       {"sha256", check.hash},
       {"format", "SKEL17"},
       {"bone_count", names.size()},
       {"parent_node_verified", parentMatches == 1},
       {"parent_node_matches", static_cast<std::uint64_t>(parentMatches)},
       {"parent_node", parent},
       {"parent_node_membership_required", require},
       {"parent_node_status", parentMatches == 1
                                  ? "present_in_supplied_skel"
                                  : "not_resolved_in_supplied_skin_skeleton"},
       {"full_attachment_rig_verified", false},
       {"socket_name", socket},
       {"socket_name_in_bone_table", socketMatches > 0},
       {"socket_name_kind", "attachment_label"},
       {"socket_name_bone_membership_required", false},
       {"model_resource_path_verified", match}});
  check.Unchanged();
  return check;
}
void Monstey_SWriteLinks(Monstey_Gadb &db, const J &model, const J &links,
                         const J &schema) {
  const auto ref = model.at("reference").as_string();
  if (Monstey_SField(model, "SimpleModelSocketOffsets"))
    db.SetField(ref, "SimpleModelSocketOffsets", links);
  else
    db.AddField(ref, "SimpleModelSocketOffsets", 11, links,
                Monstey_SSchemaField(Monstey_SDefinition(schema, "Model"),
                                     "SimpleModelSocketOffsets", 11));
}
std::uint32_t Monstey_SCategory(Monstey_Gadb &db) {
  const auto cats = db.Categories();
  for (const auto &cat : cats.at("rows").as_array())
    if (cat.at("name").as_string() == "ModelSocketOverride")
      return static_cast<std::uint32_t>(cat.at("index").as_uint());
  return static_cast<std::uint32_t>(
      db.CreateCategory("_StructuresModelSocketOverride", "ModelSocketOverride")
          .at("category")
          .at("index")
          .as_uint());
}
std::string Monstey_SClone(Monstey_Gadb &db, const J &record) {
  const auto ci =
      static_cast<std::uint32_t>(record.at("category_index").as_uint());
  std::set<std::uint32_t> used;
  for (std::size_t begin = 0;; begin += 10000) {
    const auto page = db.Records(ci, {}, begin, 10000);
    for (const auto &row : page.at("rows").as_array())
      if (row.at("named").as_bool())
        used.insert(Monstey_NativeHash(row.at("name").as_string()));
    if (begin + page.at("rows").size() >= page.at("total").as_uint())
      break;
  }
  const auto stem =
      "Monstey_ModelSocket_" +
      std::to_string(db.Resolve(record.at("reference").as_string())) + "_";
  std::uint64_t ordinal = 0;
  std::string name;
  do {
    name = stem + std::to_string(ordinal++);
  } while (used.contains(Monstey_NativeHash(name)));
  return db.CloneRecord(record.at("reference").as_string(), name)
      .at("clone")
      .at("reference")
      .as_string();
}
J Monstey_SSummary(Monstey_Gadb &db, const J &record, bool usage) {
  auto out = record;
  out["socket_name"] = Monstey_SValue(record, "SocketName");
  out["parent_node"] = Monstey_SValue(record, "ParentNode");
  out["position_cm"] = Monstey_SValue(record, "PosOffsetFromParent");
  out["rotation_xyzw"] = Monstey_SValue(record, "RotOffsetFromParent");
  if (usage)
    out["incoming_references"] =
        db.IncomingReferences(record.at("reference").as_string(), 0, 250);
  return out;
}
} // namespace
Monstey_Json Monstey_ModelSocketsCatalog() {
  const auto text = J::object({{"type", "string"}}),
             integer = J::object({{"type", "integer"}, {"minimum", 0}}),
             boolean = J::object({{"type", "boolean"}});
  const auto vector = [](unsigned n) {
    return J::object({{"type", "array"},
                      {"minItems", n},
                      {"maxItems", n},
                      {"items", J::object({{"type", "number"}})}});
  };
  auto rows = J::array();
  for (const auto name :
       {"model_sockets_list", "model_socket_create", "model_socket_edit",
        "model_socket_remove", "model_socket_copy_to_model"}) {
    const auto op = std::string_view(name);
    auto props = J::object(
        {{"expected_database_sha256", text}, {"expected_schema_sha256", text}});
    auto required = J::array();
    if (op != "model_socket_copy_to_model")
      props["model"] = text;
    if (op != "model_socket_remove")
      props["require_parent_node"] = boolean;
    if (op != "model_socket_remove")
      for (const auto key :
           {"skeleton", "skeleton_resource", "expected_skeleton_sha256"})
        props[key] = text;
    if (op == "model_sockets_list") {
      props["offset"] = integer;
      props["limit"] = integer;
      props["include_usage"] = boolean;
    } else {
      props["index"] = integer;
      if (op != "model_socket_remove") {
        props["socket_name"] = text;
        props["parent_node"] = text;
        props["position_cm"] = vector(3);
        props["rotation_xyzw"] = vector(4);
      }
      if (op == "model_socket_copy_to_model") {
        props["source_model"] = text;
        props["source_index"] = integer;
        props["target_model"] = text;
        for (auto key :
             {"source_model", "source_index", "target_model", "policy"})
          required.as_array().push_back(key);
      } else {
        required.as_array().push_back("model");
        if (op == "model_socket_create") {
          required.as_array().push_back("socket_name");
          required.as_array().push_back("parent_node");
        } else
          required.as_array().push_back("index");
      }
      if (op == "model_socket_edit" || op == "model_socket_copy_to_model") {
        props["policy"] =
            J::object({{"type", "string"},
                       {"enum", J::array({"shared", "independent"})}});
        if (op == "model_socket_edit")
          required.as_array().push_back("policy");
      }
      if (op == "model_socket_remove")
        props["delete_orphan"] = boolean;
    }
    rows.as_array().push_back(J::object(
        {{"name", name},
         {"read_only", op == "model_sockets_list"},
         {"description",
          "Native Model socket override workflow in the shared GADB session."},
         {"inputSchema", J::object({{"type", "object"},
                                    {"properties", props},
                                    {"required", required},
                                    {"additionalProperties", false}})}}));
  }
  return rows;
}
Monstey_Json Monstey_ModelSocketsCall(Monstey_Gadb &db,
                                      std::string_view operation,
                                      const J &args) {
  Monstey_SRequire(args.is_object(), "Socket arguments must be an object");
  const auto catalog = Monstey_ModelSocketsCatalog();
  const auto found = std::find_if(
      catalog.as_array().begin(), catalog.as_array().end(),
      [&](const J &op) { return op.at("name").as_string() == operation; });
  Monstey_SRequire(found != catalog.as_array().end(),
                   "Unknown native socket operation");
  for (const auto &[key, value] : args.as_object()) {
    (void)value;
    Monstey_SRequire(found->at("inputSchema").at("properties").contains(key),
                     "Unknown native socket argument: " + key);
  }
  const auto source = Monstey_SSource(db, args);
  const auto schema = db.Schema();
  const auto &overrideDefinition =
      Monstey_SDefinition(schema, "ModelSocketOverride");
  for (auto pair : {std::pair{"SocketName", 4u}, std::pair{"ParentNode", 4u},
                    std::pair{"PosOffsetFromParent", 8u},
                    std::pair{"RotOffsetFromParent", 9u}})
    (void)Monstey_SSchemaField(overrideDefinition, pair.first, pair.second);
  (void)Monstey_SSchemaField(Monstey_SDefinition(schema, "Model"),
                             "SimpleModelSocketOffsets", 11);
  if (operation == "model_sockets_list") {
    const auto offset = args.value("offset", 0).as_uint(),
               limit = args.value("limit", 250).as_uint();
    Monstey_SRequire(limit <= 10000, "Socket page exceeds 10,000 rows");
    const bool usage = args.value("include_usage", true).as_bool();
    auto rows = J::array();
    std::uint64_t total = 0;
    J model = J::object();
    if (args.contains("model")) {
      model = Monstey_SModel(db, args.at("model").as_string());
      const auto links = Monstey_SLinks(model);
      total = links.size();
      for (std::uint64_t i = offset; i < total && rows.size() < limit; ++i) {
        J row =
            links[static_cast<std::size_t>(i)].is_null()
                ? J::object({{"reference", nullptr}})
                : Monstey_SSummary(
                      db,
                      Monstey_SOverride(db, links[static_cast<std::size_t>(i)]),
                      usage);
        if (args.contains("skeleton") &&
            !links[static_cast<std::size_t>(i)].is_null()) {
          const auto record =
              Monstey_SOverride(db, links[static_cast<std::size_t>(i)]);
          auto check = Monstey_SCheckSkeleton(
              args, Monstey_SValues(record, J::object(), schema), model);
          row["skeleton_validation"] = check.report;
          check.Unchanged();
        }
        row["index"] = i;
        rows.as_array().push_back(row);
      }
    } else {
      Monstey_SRequire(!args.contains("skeleton") &&
                           !args.contains("expected_skeleton_sha256") &&
                           !args.contains("skeleton_resource"),
                       "Library skeleton validation requires a selected Model");
      const auto cats = db.Categories();
      for (const auto &cat : cats.at("rows").as_array())
        if (cat.at("name").as_string() == "ModelSocketOverride") {
          for (std::size_t begin = 0;; begin += 10000) {
            const auto page = db.Records(
                static_cast<std::uint32_t>(cat.at("index").as_uint()), {},
                begin, 10000);
            for (const auto &r : page.at("rows").as_array())
              if (!r.at("pending_delete").as_bool()) {
                auto ordinal = total++;
                if (ordinal >= offset && rows.size() < limit)
                  rows.as_array().push_back(Monstey_SSummary(
                      db, Monstey_SOverride(db, r.at("reference")), usage));
              }
            if (begin + page.at("rows").size() >= page.at("total").as_uint())
              break;
          }
        }
    }
    auto out =
        J::object({{"source", source},
                   {"model", model},
                   {"rows", rows},
                   {"total", total},
                   {"offset", offset},
                   {"limit", limit},
                   {"position_units", "native centimetres"},
                   {"rotation_components",
                    "native XYZW; no normalization or component reorder"},
                   {"runtime_verified", false}});
    Monstey_SSource(db, args);
    return out;
  }
  J model = Monstey_SModel(
      db, Monstey_SText(args, operation == "model_socket_copy_to_model"
                                  ? "target_model"
                                  : "model"));
  auto links = Monstey_SLinks(model);
  J detail = J::object();
  Monstey_SkeletonCheck skel;
  const auto result = db.Transaction([&](Monstey_Gadb &) {
    if (operation == "model_socket_remove") {
      auto index = Monstey_SIndex(args, "index", links.size());
      const auto ref = links[index];
      links.as_array().erase(links.as_array().begin() +
                             static_cast<std::ptrdiff_t>(index));
      Monstey_SWriteLinks(db, model, links, schema);
      detail = J::object({{"removed_index", index},
                          {"reference", ref},
                          {"record_deleted", false}});
      if (args.value("delete_orphan", false).as_bool() && !ref.is_null()) {
        const auto uses = db.IncomingReferences(ref.as_string(), 0, 1);
        Monstey_SRequire(uses.at("total").as_uint() == 0,
                         "Override is still referenced; choose unlink without "
                         "delete_orphan");
        db.DeleteRecord(ref.as_string());
        detail["record_deleted"] = true;
      }
    } else {
      J original = J::object();
      std::string ref, policy = "independent";
      std::size_t index = links.size();
      if (operation == "model_socket_edit") {
        index = Monstey_SIndex(args, "index", links.size());
        original = Monstey_SOverride(db, links[index]);
        policy = Monstey_SPolicy(args);
      } else if (operation == "model_socket_copy_to_model") {
        const auto donor =
            Monstey_SModel(db, args.at("source_model").as_string());
        const auto donorLinks = Monstey_SLinks(donor);
        original = Monstey_SOverride(
            db, donorLinks[Monstey_SIndex(args, "source_index",
                                          donorLinks.size())]);
        policy = Monstey_SPolicy(args);
        if (policy == "shared")
          for (auto key :
               {"socket_name", "parent_node", "position_cm", "rotation_xyzw"})
            Monstey_SRequire(!args.contains(key),
                             "Shared copy cannot alter the source override; "
                             "choose independent for edits");
        if (args.contains("index"))
          index = Monstey_SIndex(args, "index", links.size(), true);
      } else if (args.contains("index"))
        index = Monstey_SIndex(args, "index", links.size(), true);
      const auto values = Monstey_SValues(original, args, schema);
      skel = Monstey_SCheckSkeleton(args, values, model);
      if (original.empty()) {
        const auto created = db.CreateRecord(Monstey_SCategory(db), {}, true);
        ref = created.at("record").at("reference").as_string();
        for (auto pair :
             {std::pair{"SocketName", 4u}, std::pair{"ParentNode", 4u},
              std::pair{"PosOffsetFromParent", 8u},
              std::pair{"RotOffsetFromParent", 9u}})
          db.AddField(ref, pair.first, static_cast<std::uint8_t>(pair.second),
                      values.at(pair.first),
                      Monstey_SSchemaField(overrideDefinition, pair.first,
                                           pair.second));
      } else {
        ref = original.at("reference").as_string();
        detail["source_reference"] = ref;
        detail["incoming_references_before"] =
            db.IncomingReferences(ref, 0, 250);
        if (policy == "independent")
          ref = Monstey_SClone(db, original);
        for (auto pair : {std::pair{"socket_name", "SocketName"},
                          std::pair{"parent_node", "ParentNode"},
                          std::pair{"position_cm", "PosOffsetFromParent"},
                          std::pair{"rotation_xyzw", "RotOffsetFromParent"}})
          if (args.contains(pair.first))
            db.SetField(ref, pair.second, values.at(pair.second));
      }
      if (operation == "model_socket_edit")
        links[index] = ref;
      else
        links.as_array().insert(
            links.as_array().begin() + static_cast<std::ptrdiff_t>(index), ref);
      Monstey_SWriteLinks(db, model, links, schema);
      detail["reference"] = ref;
      detail["index"] = index;
      detail["policy"] = policy;
      detail["override"] = Monstey_SSummary(db, db.Metadata(ref), true);
      detail["skeleton_validation"] = skel.report;
      detail["unknown_source_fields_preserved"] = !original.empty();
      detail["old_unlinked_record_retained"] =
          !original.empty() && policy == "independent";
    }
    detail["model_reference"] = model.at("reference");
    detail["model_name"] = model.at("name");
    skel.Unchanged();
    (void)Monstey_SSource(db, args);
  });
  auto out = result;
  out["operation"] = std::string(operation);
  out["result"] = detail;
  out["position_units"] = "native centimetres";
  out["rotation_components"] =
      "native XYZW; no normalization or component reorder";
  out["game_written"] = false;
  out["runtime_verified"] = false;
  return out;
}
} // namespace Monstey