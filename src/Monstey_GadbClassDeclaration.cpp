#include "Monstey_GadbClassDeclaration.hpp"
#include "Monstey_FearAudit.hpp"
#include <map>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
using S = std::span<const std::uint8_t>;
std::string Monstey_ClassHex(S bytes) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string out(bytes.size() * 2, '0');
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    out[i * 2] = digits[bytes[i] >> 4];
    out[i * 2 + 1] = digits[bytes[i] & 15];
  }
  return out;
}
struct Monstey_ClassReader {
  S image;
  S At(std::uint64_t address, std::size_t count) const {
    Monstey_Require(count != 0, "Class declaration: empty consumed range");
    return Monstey_FearPeRange(image, address, count);
  }
  J Text(std::uint64_t address) const {
    if (!address)
      return nullptr;
    std::string text;
    for (std::size_t i = 0; i < 4096; ++i) {
      Monstey_Require(address <= UINT64_MAX - i, "Class text address overflow");
      const auto byte = At(address + i, 1)[0];
      if (!byte) {
        (void)Monstey_Wide(text);
        return text;
      }
      // Preserve native bytes without locale folding. Invalid UTF-8 is
      // explicitly unavailable text; raw descriptors remain intact.
      text.push_back(static_cast<char>(byte));
    }
    throw Monstey_Error("class_declaration_text",
                        "Native text exceeds 4096-byte reader profile");
  }
  J SafeText(std::uint64_t address) const {
    try {
      return J::object({{"status", "FILE_BACKED"}, {"value", Text(address)}});
    } catch (const Monstey_Error &e) {
      return J::object(
          {{"status", "UNAVAILABLE_OR_INVALID"}, {"error", e.what()}});
    }
  }
  J Field(std::uint64_t address, std::uint32_t index) const {
    const auto raw = At(address, 72);
    const auto namePointer = Monstey_U64(raw, 0);
    auto name = SafeText(namePointer);
    auto hashMatches = J(nullptr);
    if (name.at("status").as_string() == "FILE_BACKED" &&
        name.at("value").is_string())
      hashMatches = Monstey_NativeHash(name.at("value").as_string()) ==
                    Monstey_U32(raw, 32);
    return J::object(
        {{"declaration_index", index},
         {"field_pointer", Monstey_Hex(address)},
         {"raw72_hex", Monstey_ClassHex(raw)},
         {"raw72_sha256", Monstey_HashSha256(raw)},
         {"name_pointer", Monstey_Hex(namePointer)},
         {"name", name},
         {"constraint_pointer", Monstey_Hex(Monstey_U64(raw, 8))},
         {"constraint", SafeText(Monstey_U64(raw, 8))},
         {"default16_hex", Monstey_ClassHex(raw.subspan(16, 16))},
         {"name_hash_u32", Monstey_Hex(Monstey_U32(raw, 32))},
         {"name_hash_matches", hashMatches},
         {"type_u32", Monstey_U32(raw, 36)},
         {"count_u32", Monstey_U32(raw, 40)},
         {"flags_u32", Monstey_U32(raw, 44)},
         {"group_marker", (Monstey_U32(raw, 44) & 0x280u) != 0},
         {"captured_binding_state_u32", Monstey_U32(raw, 48)},
         {"index_callback_pointer", Monstey_Hex(Monstey_U64(raw, 56))},
         {"captured_cache_byte_u8", raw[64]},
         {"index_callback_execution", "NOT_PERFORMED"}});
  }
};
} // namespace
Monstey_Json
Monstey_GadbInspectClassPointers(S image,
                                 std::span<const std::uint64_t> pointers,
                                 std::string_view expected) {
  Monstey_Require(!pointers.empty() && pointers.size() <= 4096,
                  "Class declarations require 1..4096 explicit pointers");
  const auto sha = Monstey_HashSha256(image);
  if (!expected.empty() && sha != expected)
    throw Monstey_Error("source_hash_mismatch",
                        "Class declaration image differs from expected SHA256");
  const Monstey_ClassReader reader{image};
  auto classes = J::array(), requested = J::array();
  std::map<std::uint64_t, std::size_t> seen;
  std::set<std::uint64_t> active;
  const auto visit = [&](auto &&self, std::uint64_t address) -> void {
    if (seen.contains(address))
      return;
    Monstey_Require(
        address != 0 && active.size() < 64 &&
            seen.size() + active.size() < 4096 && active.insert(address).second,
        "Class declaration null pointer, cycle, depth or row limit");
    const auto raw = reader.At(address, 64);
    const auto table = Monstey_U64(raw, 0), parent = Monstey_U64(raw, 40);
    const auto count = Monstey_U32(raw, 8);
    Monstey_Require(count <= 4096,
                    "Class field count exceeds bounded reader profile");
    Monstey_Require(count == 0 || table <= UINT64_MAX - 72ull * count,
                    "Class field table address overflow");
    auto row = J::object(
        {{"class_pointer", Monstey_Hex(address)},
         {"raw64_hex", Monstey_ClassHex(raw)},
         {"raw64_sha256", Monstey_HashSha256(raw)},
         {"field_table_pointer", Monstey_Hex(table)},
         {"field_count_u32", count},
         {"short_name", reader.SafeText(Monstey_U64(raw, 16))},
         {"description", reader.SafeText(Monstey_U64(raw, 24))},
         {"full_name", reader.SafeText(Monstey_U64(raw, 32))},
         {"parent_pointer", Monstey_Hex(parent)},
         {"opaque_class_pointer_30", Monstey_Hex(Monstey_U64(raw, 48))},
         {"class_30_role", "NOT_INFERRED"},
         {"flags_u32", Monstey_U32(raw, 56)},
         {"captured_initialized_u8", raw[60]},
         {"fields", J::array()},
         {"field_storage_status", "COMPLETE_FILE_BACKED"}});
    // Read the complete table range first: no partial row is invented if
    // the declared table crosses a raw section boundary or virtual tail.
    bool available = true;
    if (count) {
      try {
        (void)reader.At(table, static_cast<std::size_t>(count) * 72);
      } catch (const Monstey_Error &e) {
        available = false;
        row["field_storage_status"] = "UNAVAILABLE_FILE_RANGE";
        row["field_storage_error"] = e.what();
      }
    }
    if (available)
      for (std::uint32_t i = 0; i < count; ++i)
        row["fields"].as_array().push_back(reader.Field(table + 72ull * i, i));
    if (parent)
      self(self, parent);
    seen.emplace(address, classes.size());
    classes.as_array().push_back(std::move(row));
    active.erase(address);
  };
  for (const auto p : pointers) {
    requested.as_array().push_back(Monstey_Hex(p));
    visit(visit, p);
  }
  return J::object({{"schema", "Monstey.GadbExplicitClassDeclarations/1"},
                    {"source_sha256", sha},
                    {"requested_class_pointers", requested},
                    {"classes", classes},
                    {"identity", "EXACT_CLASS_POINTER"},
                    {"class_name_selection", "NOT_PERFORMED"},
                    {"binding_or_initialization_execution", "NOT_PERFORMED"},
                    {"original_code_executed", false}});
}
} // namespace Monstey
