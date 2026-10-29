#include "Monstey_MaterialProject.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <type_traits>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
void Monstey_Check(bool b, const char *s) {
  if (!b)
    throw Monstey_Error("material_project", s);
}
std::uint32_t Monstey_Word(const J &v, std::uint32_t maximum) {
  Monstey_Check(v.is_number() && std::isfinite(v.as_number()) &&
                    std::floor(v.as_number()) == v.as_number() &&
                    v.as_number() >= 0 && v.as_number() <= maximum,
                "Expected unsigned integer in stored range");
  return static_cast<std::uint32_t>(v.as_number());
}
B Monstey_Text(std::string_view text) { return B(text.begin(), text.end()); }
B Monstey_HexBytes(std::string_view text) {
  Monstey_Check(text.size() % 2 == 0,
                "Hex payload must contain complete bytes");
  B out;
  auto digit = [](char c) -> unsigned {
    if (c >= '0' && c <= '9')
      return unsigned(c - '0');
    if (c >= 'a' && c <= 'f')
      return unsigned(c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
      return unsigned(c - 'A' + 10);
    throw Monstey_Error("material_project", "Invalid hexadecimal byte");
  };
  for (std::size_t i = 0; i < text.size(); i += 2)
    out.push_back(
        static_cast<std::uint8_t>((digit(text[i]) << 4) | digit(text[i + 1])));
  return out;
}
B Monstey_Payload(std::uint16_t type, const J &operation) {
  if (operation.contains("bytes_hex")) {
    Monstey_Check(!operation.contains("value"), "Choose value or bytes_hex");
    return Monstey_HexBytes(operation["bytes_hex"].as_string());
  }
  const auto &value = operation.at("value");
  B bytes;
  const auto low = static_cast<std::uint8_t>(type);
  if (low == 1 || low == 8) {
    bytes = Monstey_Text(value.as_string());
    bytes.push_back(0);
    return bytes;
  }
  if (low == 4) {
    bytes.resize(4);
    Monstey_PutU32(bytes, 0, Monstey_Word(value, 0xffffffffU));
    return bytes;
  }
  if (low == 6) {
    bytes.resize(4);
    Monstey_PutU32(bytes, 0, value.as_bool() ? 1U : 0U);
    return bytes;
  }
  std::size_t count = 0;
  switch (low) {
  case 2:
    count = 3;
    break;
  case 3:
    count = 4;
    break;
  case 5:
    count = 1;
    break;
  case 9:
    count = 16;
    break;
  case 10:
    count = 12;
    break;
  case 11:
    count = 2;
    break;
  default:
    throw Monstey_Error("material_project",
                        "Use a qualified material value type");
  }
  Monstey_Check(count == 1 || value.is_array(),
                "Vector/matrix value must be an array");
  if (count > 1)
    Monstey_Check(value.size() == count,
                  "Vector/matrix width differs from native type");
  bytes.resize(count * 4);
  for (std::size_t i = 0; i < count; ++i) {
    const auto &element = count == 1 ? value : value[i];
    Monstey_Check(element.is_number(), "Expected numeric material value");
    const auto n = element.as_number();
    Monstey_Check(
        std::isfinite(n) && std::abs(n) <= std::numeric_limits<float>::max(),
        "Material value exceeds finite F32 range; raw words use bytes_hex");
    Monstey_PutU32(bytes, i * 4,
                   std::bit_cast<std::uint32_t>(static_cast<float>(n)));
  }
  return bytes;
}
void Monstey_Keys(const J &value,
                  std::initializer_list<std::string_view> keys) {
  Monstey_Check(value.is_object(), "Expected JSON object");
  for (const auto &[key, unused] : value.as_object()) {
    (void)unused;
    Monstey_Check(std::find(keys.begin(), keys.end(), key) != keys.end(),
                  "Unknown LTMI edit field");
  }
}
void Monstey_Apply(Monstey_LtmiMaterial &material, const J &operation) {
  Monstey_Check(operation.is_object(), "Expected material operation object");
  const auto op = operation.at("op").as_string();
  if (op == "add")
    Monstey_Keys(operation,
                 {"op", "table", "name", "raw_name_hex", "type_u16", "flag_a",
                  "flag_b", "legacy_word", "value", "bytes_hex"});
  else if (op == "set")
    Monstey_Keys(operation, {"op", "table", "index", "value", "bytes_hex"});
  else if (op == "remove")
    Monstey_Keys(operation, {"op", "table", "index"});
  else if (op == "rename")
    Monstey_Keys(operation, {"op", "table", "index", "name", "raw_name_hex"});
  else
    throw Monstey_Error("material_project", "Unknown LTMI operation");
  const auto tableName = operation.at("table").as_string();
  Monstey_Check(tableName == "a" || tableName == "b", "Table must be a or b");
  const std::size_t table = tableName == "a" ? 0 : 1;
  auto name = [&]() {
    Monstey_Check(operation.contains("name") !=
                      operation.contains("raw_name_hex"),
                  "Choose name or raw_name_hex");
    return operation.contains("name")
               ? Monstey_Text(operation["name"].as_string())
               : Monstey_HexBytes(operation["raw_name_hex"].as_string());
  };
  if (op == "add") {
    Monstey_LtmiProperty p;
    p.type = static_cast<std::uint16_t>(
        Monstey_Word(operation.at("type_u16"), 65535));
    p.name = name();
    if (operation.contains("flag_a"))
      p.flag_a =
          static_cast<std::uint8_t>(Monstey_Word(operation["flag_a"], 255));
    if (operation.contains("flag_b"))
      p.flag_b =
          static_cast<std::uint8_t>(Monstey_Word(operation["flag_b"], 255));
    if (operation.contains("legacy_word")) {
      Monstey_Check(material.version == 7, "legacy_word belongs to LTMI7 only");
      p.legacy_word = static_cast<std::uint16_t>(
          Monstey_Word(operation["legacy_word"], 65535));
    }
    auto payload = Monstey_Payload(p.type, operation);
    Monstey_LtmiAdd(material, table, std::move(p), payload);
    return;
  }
  const auto index = Monstey_Word(operation.at("index"), 127);
  Monstey_Check(index < material.tables[table].size(),
                "Material ordinal is outside selected table");
  if (op == "set") {
    auto payload =
        Monstey_Payload(material.tables[table][index].type, operation);
    Monstey_LtmiSetValue(material, table, index, payload);
  } else if (op == "remove")
    Monstey_LtmiRemove(material, table, index);
  else {
    auto bytes = name();
    Monstey_LtmiRename(material, table, index, bytes);
  }
}
std::string Monstey_LtmiExpectedHash(std::string value) {
  Monstey_Check(value.size() == 64,
                "Expected source SHA256 must contain64 hexadecimal digits");
  for (auto &c : value) {
    if (c >= 'A' && c <= 'F')
      c = static_cast<char>(c - 'A' + 'a');
    Monstey_Check((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'),
                  "Expected source SHA256 contains non-hexadecimal data");
  }
  return value;
}

std::string Monstey_MPath(const std::filesystem::path &p) {
  return Monstey_Utf8(
      std::filesystem::absolute(p).lexically_normal().wstring());
}
struct Monstey_MInput {
  B bytes;
  std::string sha;
};
Monstey_MInput Monstey_MRead(const std::filesystem::path &path,
                             std::string_view expected = {}) {
  Monstey_Check(!path.empty(), "Material source path must be explicit");
  auto bytes = Monstey_ReadFile(path);
  auto sha = Monstey_HashSha256(bytes);
  if ((!expected.empty() &&
       sha != Monstey_LtmiExpectedHash(std::string(expected))) ||
      Monstey_HashSha256(path) != sha)
    throw Monstey_Error("source_hash_mismatch",
                        "Material input changed or hash differs");
  return {std::move(bytes), std::move(sha)};
}
J Monstey_MJson(const B &bytes) {
  auto text = std::string_view(reinterpret_cast<const char *>(bytes.data()),
                               bytes.size());
  if (text.starts_with("\xef\xbb\xbf"))
    text.remove_prefix(3);
  (void)Monstey_Wide(text);
  return J::Parse(text);
}
std::string Monstey_MHex(std::span<const std::uint8_t> data) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(data.size() * 2);
  for (auto v : data) {
    result.push_back(digits[v >> 4]);
    result.push_back(digits[v & 15]);
  }
  return result;
}
J Monstey_MMetadata(const Monstey_LtmiMaterial &m) {
  auto result = Monstey_LtmiDescribe(m);
  for (auto &entry : result["entries"].as_array()) {
    const auto table = entry["table"].as_string() == "a" ? 0u : 1u;
    const auto &property = m.tables[table].at(
        static_cast<std::size_t>(entry["ordinal"].as_uint()));
    entry["raw_name_hex"] = Monstey_MHex(property.name);
    entry["value_bytes_hex"] = Monstey_MHex(Monstey_LtmiValue(m, property));
  }
  return result;
}
void Monstey_MPublish(const std::filesystem::path &output, const B &bytes,
                      std::span<const std::filesystem::path> sources,
                      const std::function<void()> &check) {
  Monstey_RequireOfflineOutput(output, sources);
  check();
  // Core owns the exclusive handle through flush/rename and performs
  // object-based failure cleanup. This layer never deletes a pathname after
  // publication.
  Monstey_WriteNewFile(output, bytes);
  Monstey_Check(Monstey_ReadFile(output) == bytes,
                "Material output readback differs");
  check();
}
} // namespace
void Monstey_LtmiApplyOperations(Monstey_LtmiMaterial &material,
                                 const J &operations) {
  Monstey_Check(operations.is_array() && operations.size() <= 10000,
                "Material batch requires at most10000 operations");
  auto candidate = material;
  for (const auto &operation : operations.as_array())
    Monstey_Apply(candidate, operation);
  (void)Monstey_LtmiBuild(candidate);
  material = std::move(candidate);
}
Monstey_Json Monstey_LtmiOperationSchema() {
  const auto string = J::object({{"type", "string"}});
  const auto integer = [](unsigned max) {
    return J::object({{"type", "integer"}, {"minimum", 0}, {"maximum", max}});
  };
  return J::object(
      {{"type", "object"},
       {"properties",
        J::object({{"op", J::object({{"type", "string"},
                                     {"enum", J::array({"set", "add", "remove",
                                                        "rename"})}})},
                   {"table", J::object({{"type", "string"},
                                        {"enum", J::array({"a", "b"})}})},
                   {"index", integer(127)},
                   {"name", string},
                   {"raw_name_hex", string},
                   {"type_u16", integer(65535)},
                   {"flag_a", integer(255)},
                   {"flag_b", integer(255)},
                   {"legacy_word", integer(65535)},
                   {"value", J::object()},
                   {"bytes_hex", string}})},
       {"required", J::array({"op", "table"})},
       {"additionalProperties", false}});
}
struct Monstey_MaterialProject::Monstey_Impl {
  struct Monstey_State {
    Monstey_LtmiMaterial material;
    J metadata;
    std::string hash;
    explicit Monstey_State(Monstey_LtmiMaterial value)
        : material(std::move(value)), metadata(Monstey_MMetadata(material)),
          hash(Monstey_HashSha256(Monstey_LtmiBuild(material))) {}
  };
  std::filesystem::path source, project;
  std::string sha, projectSha, baseHash;
  Monstey_LtmiMaterial base;
  std::unique_ptr<Monstey_State> current;
  std::vector<std::unique_ptr<J>> steps;
  std::size_t cursor{};
  Monstey_Impl(const std::filesystem::path &path, Monstey_MInput input)
      : source(std::filesystem::absolute(path).lexically_normal()),
        sha(std::move(input.sha)), base(Monstey_LtmiRead(input.bytes)),
        current(std::make_unique<Monstey_State>(base)) {
    baseHash = current->hash;
    Monstey_Check(baseHash == sha, "Material baseline must be byte exact");
    Check();
  }
  void Check() const {
    if (Monstey_HashSha256(source) != sha ||
        (!project.empty() && Monstey_HashSha256(project) != projectSha))
      throw Monstey_Error(
          "source_hash_mismatch",
          "Material source or loaded project changed; reopen it");
  }
  std::vector<std::filesystem::path> Sources() const {
    auto result = std::vector<std::filesystem::path>{source};
    if (!project.empty())
      result.push_back(project);
    return result;
  }
  std::unique_ptr<Monstey_State> Replay(std::size_t count) const {
    auto material = base;
    std::string hash = baseHash;
    for (std::size_t i = 0; i < count; ++i) {
      const auto &step = *steps.at(i);
      Monstey_Check(step.at("before_sha256").as_string() == hash,
                    "Material history input hash differs");
      const auto kind = step.at("kind").as_string();
      if (kind == "revert")
        material = base;
      else {
        Monstey_Check(kind == "edit", "Unknown material history step");
        Monstey_LtmiApplyOperations(material, step.at("operations"));
      }
      hash = Monstey_HashSha256(Monstey_LtmiBuild(material));
      Monstey_Check(step.at("after_sha256").as_string() == hash,
                    "Material history output hash differs");
    }
    return std::make_unique<Monstey_State>(std::move(material));
  }
  void Install(std::unique_ptr<Monstey_State> next, J step) {
    Monstey_Check(cursor < 10000, "Material history exceeds 10000 steps");
    step["before_sha256"] = current->hash;
    step["after_sha256"] = next->hash;
    auto ownedStep = std::make_unique<J>(std::move(step));
    steps.reserve(cursor + 1);
    Check();
    steps.resize(cursor);
    steps.push_back(std::move(ownedStep));
    current = std::move(next);
    cursor = steps.size();
  }
  J Describe() const {
    return J::object(
        {{"source", Monstey_MPath(source)},
         {"source_sha256", sha},
         {"native_baseline_sha256", baseHash},
         {"effective_sha256", current->hash},
         {"modified", current->hash != baseHash},
         {"dirty", current->hash != baseHash},
         {"can_undo", cursor > 0},
         {"can_redo", cursor < steps.size()},
         {"undo_steps", static_cast<std::uint64_t>(cursor)},
         {"redo_steps", static_cast<std::uint64_t>(steps.size() - cursor)},
         {"material", current->metadata},
         {"implementation", "C++20"},
         {"python_runtime", false},
         {"runtime_material_rendering_verified", false}});
  }
};
Monstey_MaterialProject::Monstey_MaterialProject(
    const std::filesystem::path &source, std::string_view expected)
    : impl_(std::make_unique<Monstey_Impl>(source,
                                           Monstey_MRead(source, expected))) {}
Monstey_MaterialProject::~Monstey_MaterialProject() = default;
J Monstey_MaterialProject::Describe() const {
  impl_->Check();
  return impl_->Describe();
}
J Monstey_MaterialProject::Edit(const J &operations) {
  Monstey_Check(operations.is_array() && !operations.empty(),
                "Material session edit needs at least one operation");
  auto &p = *impl_;
  p.Check();
  auto material = p.current->material;
  Monstey_LtmiApplyOperations(material, operations);
  auto next =
      std::make_unique<Monstey_Impl::Monstey_State>(std::move(material));
  if (next->hash != p.current->hash)
    p.Install(std::move(next),
              J::object({{"kind", "edit"}, {"operations", operations}}));
  return Describe();
}
J Monstey_MaterialProject::EditFile(const std::filesystem::path &path,
                                    std::string_view expected) {
  auto &p = *impl_;
  p.Check();
  const auto input = Monstey_MRead(path, expected);
  const auto operations = Monstey_MJson(input.bytes);
  Monstey_Check(operations.is_array() && !operations.empty(),
                "Material session edit needs at least one operation");
  auto material = p.current->material;
  Monstey_LtmiApplyOperations(material, operations);
  auto next =
      std::make_unique<Monstey_Impl::Monstey_State>(std::move(material));
  if (Monstey_HashSha256(path) != input.sha)
    throw Monstey_Error("source_hash_mismatch",
                        "Material operation file changed");
  if (next->hash != p.current->hash)
    p.Install(std::move(next),
              J::object({{"kind", "edit"}, {"operations", operations}}));
  return Describe();
}
J Monstey_MaterialProject::Undo() {
  auto &p = *impl_;
  p.Check();
  Monstey_Check(p.cursor > 0, "No material edit to undo");
  auto next = p.Replay(p.cursor - 1);
  p.Check();
  p.current = std::move(next);
  --p.cursor;
  return Describe();
}
J Monstey_MaterialProject::Redo() {
  auto &p = *impl_;
  p.Check();
  Monstey_Check(p.cursor < p.steps.size(), "No material edit to redo");
  auto next = p.Replay(p.cursor + 1);
  p.Check();
  p.current = std::move(next);
  ++p.cursor;
  return Describe();
}
J Monstey_MaterialProject::Revert() {
  auto &p = *impl_;
  p.Check();
  if (p.current->hash != p.baseHash)
    p.Install(std::make_unique<Monstey_Impl::Monstey_State>(p.base),
              J::object({{"kind", "revert"}}));
  return Describe();
}
J Monstey_MaterialProject::Changes(const J &options) const {
  auto &p = *impl_;
  p.Check();
  Monstey_Keys(options, {"include_operations", "offset", "limit"});
  const auto offset = options.value("offset", 0u).as_uint(),
             limit = options.value("limit", 10000u).as_uint();
  const bool include = options.value("include_operations", true).as_bool();
  Monstey_Check(offset <= p.cursor && limit > 0 && limit <= 10000,
                "Material history page is outside journal bounds");
  auto rows = J::array();
  const auto count = std::min<std::uint64_t>(p.cursor - offset, limit);
  for (auto i = offset; i < offset + count; ++i) {
    auto row = *p.steps[static_cast<std::size_t>(i)];
    if (!include && row.contains("operations")) {
      row["operation_count"] =
          static_cast<std::uint64_t>(row["operations"].size());
      row.as_object().erase("operations");
    }
    rows.as_array().push_back(std::move(row));
  }
  auto result = p.Describe();
  result["steps"] = std::move(rows);
  result["total_steps"] = static_cast<std::uint64_t>(p.cursor);
  result["offset"] = offset;
  result["limit"] = limit;
  result["returned_steps"] = count;
  result["has_more"] = offset + count < p.cursor;
  result["operations_included"] = include;
  return result;
}
J Monstey_MaterialProject::SaveProject(
    const std::filesystem::path &output) const {
  const auto &p = *impl_;
  p.Check();
  auto steps = J::array();
  for (const auto &step : p.steps)
    steps.as_array().push_back(*step);
  const auto doc = J::object({{"format", "MONSTEY_CPP_LTMI_PROJECT"},
                              {"version", 2},
                              {"source", Monstey_MPath(p.source)},
                              {"source_sha256", p.sha},
                              {"native_baseline_sha256", p.baseHash},
                              {"effective_sha256", p.current->hash},
                              {"cursor", static_cast<std::uint64_t>(p.cursor)},
                              {"steps", steps}});
  const auto text = doc.dump(2) + "\n";
  const B bytes(text.begin(), text.end());
  Monstey_MPublish(output, bytes, p.Sources(), [&] { p.Check(); });
  return J::object({{"path", Monstey_MPath(output)},
                    {"sha256", Monstey_HashSha256(bytes)},
                    {"bytes", static_cast<std::uint64_t>(bytes.size())},
                    {"source_qualified", true}});
}
std::unique_ptr<Monstey_MaterialProject>
Monstey_MaterialProject::LoadProject(const std::filesystem::path &path,
                                     std::string_view expected) {
  const auto input = Monstey_MRead(path, expected);
  const auto doc = Monstey_MJson(input.bytes);
  Monstey_Keys(doc, {"format", "version", "source", "source_sha256",
                     "native_baseline_sha256", "effective_sha256", "cursor",
                     "steps"});
  Monstey_Check(doc.at("format").as_string() == "MONSTEY_CPP_LTMI_PROJECT" &&
                    doc.at("version").as_uint() == 2,
                "Unsupported native material project; version 2 requires bounded-exclusive COW value storage");
  const auto source =
      std::filesystem::path(Monstey_Wide(doc.at("source").as_string()));
  Monstey_Check(source.is_absolute(),
                "Material project source path must be absolute");
  auto result = std::make_unique<Monstey_MaterialProject>(
      source, doc.at("source_sha256").as_string());
  auto &p = *result->impl_;
  Monstey_Check(doc.at("native_baseline_sha256").as_string() == p.baseHash,
                "Material project baseline hash differs");
  const auto &steps = doc.at("steps");
  const auto cursor = doc.at("cursor").as_uint();
  Monstey_Check(steps.is_array() && steps.size() <= 10000 &&
                    cursor <= steps.size(),
                "Material project history bounds invalid");
  p.steps.reserve(steps.size());
  for (const auto &step : steps.as_array()) {
    const auto kind = step.at("kind").as_string();
    if (kind == "edit")
      Monstey_Keys(step,
                   {"kind", "operations", "before_sha256", "after_sha256"});
    else {
      Monstey_Check(kind == "revert", "Unknown material project step");
      Monstey_Keys(step, {"kind", "before_sha256", "after_sha256"});
    }
    (void)Monstey_LtmiExpectedHash(step.at("before_sha256").as_string());
    (void)Monstey_LtmiExpectedHash(step.at("after_sha256").as_string());
    p.steps.push_back(std::make_unique<J>(step));
  }
  (void)p.Replay(
      p.steps.size()); // Qualify the redo branch too, before exposing it.
  auto current = p.Replay(static_cast<std::size_t>(cursor));
  Monstey_Check(current->hash == doc.at("effective_sha256").as_string(),
                "Material project effective hash differs");
  if (Monstey_HashSha256(path) != input.sha)
    throw Monstey_Error("source_hash_mismatch",
                        "Material project changed while reading");
  p.Check();
  p.current = std::move(current);
  p.cursor = static_cast<std::size_t>(cursor);
  p.project = std::filesystem::absolute(path).lexically_normal();
  p.projectSha = input.sha;
  p.Check();
  return result;
}
J Monstey_MaterialProject::Build(const std::filesystem::path &output) const {
  const auto &p = *impl_;
  p.Check();
  const auto bytes = Monstey_LtmiBuild(p.current->material);
  Monstey_Check(Monstey_HashSha256(bytes) == p.current->hash,
                "Material build differs from verified session");
  const auto checked = Monstey_LtmiRead(bytes);
  Monstey_Check(Monstey_LtmiBuild(checked) == bytes,
                "Material build readback is not byte exact");
  Monstey_MPublish(output, bytes, p.Sources(), [&] { p.Check(); });
  return J::object({{"status", "MONSTEY_LTMI_BUILD_VALIDATED"},
                    {"path", Monstey_MPath(output)},
                    {"sha256", p.current->hash},
                    {"bytes", static_cast<std::uint64_t>(bytes.size())},
                    {"no_op_byte_exact", p.current->hash == p.baseHash},
                    {"source_sha256", p.sha},
                    {"version", checked.version},
                    {"runtime_material_rendering_verified", false},
                    {"game_written", false}});
}
J Monstey_MaterialCatalog() {
  const auto path =
      J::object({{"type", "string"}, {"format", "path"}, {"minLength", 1}});
  const auto hash =
      J::object({{"type", "string"}, {"minLength", 64}, {"maxLength", 64}});
  auto result = J::array();
  auto add = [&](const char *name, const char *description, bool read,
                 J properties, J required = J::array()) {
    result.as_array().push_back(J::object(
        {{"name", name},
         {"description", description},
         {"read_only", read},
         {"inputSchema", J::object({{"type", "object"},
                                    {"properties", properties},
                                    {"required", required},
                                    {"additionalProperties", false}})}}));
  };
  add("material_open", "Open a source-qualified LTMI7/8 material.", false,
      J::object({{"path", path}, {"expected_sha256", hash}}),
      J::array({"path"}));
  add("material_load_project",
      "Load and hash-qualify a persistent native material journal.", false,
      J::object({{"path", path}, {"expected_sha256", hash}}),
      J::array({"path"}));
  add("material_describe",
      "Inspect effective material values and native preservation metadata.",
      true, J::object());
  add("material_edit",
      "Apply one atomic batch using the shared LTMI authoring grammar.", false,
      J::object({{"operations",
                  J::object({{"type", "array"},
                             {"minItems", 1},
                             {"maxItems", 10000},
                             {"items", Monstey_LtmiOperationSchema()}})},
                 {"operations_path", path},
                 {"expected_sha256", hash}}));
  for (const auto *name : {"material_undo", "material_redo", "material_revert"})
    add(name, "Change the effective material history cursor.", false,
        J::object());
  add("material_changes", "Page the applied material journal.", true,
      J::object({{"include_operations", J::object({{"type", "boolean"}})},
                 {"offset", J::object({{"type", "integer"}, {"minimum", 0}})},
                 {"limit", J::object({{"type", "integer"},
                                      {"minimum", 1},
                                      {"maximum", 10000}})}}));
  add("material_save_project",
      "Save the source-qualified journal and redo branch to a new file.", false,
      J::object({{"path", path}}), J::array({"path"}));
  add("material_build", "Build and read back a new offline LTMI candidate.",
      false, J::object({{"output", path}}), J::array({"output"}));
  return result;
}
} // namespace Monstey
