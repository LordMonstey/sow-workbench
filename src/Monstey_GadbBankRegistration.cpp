#include "Monstey_GadbBanks.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <map>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
using S = std::span<const std::uint8_t>;
#include "Monstey_GadbBankProfile.inc"
constexpr std::string_view Monstey_BankMapSha =
    "eaecf6858e80a6cccc086b2f1b09fa2fedb5c50a6c8065237dfaf5c3ee3e3fde";
std::uint64_t Monstey_BankAddress(const J &value) {
  Monstey_Require(value.is_string(), "Expected address string");
  const auto &s = value.as_string();
  Monstey_Require(s.starts_with("0x") && s.size() > 2 && s.size() <= 18,
                  "Invalid bank address");
  std::uint64_t n = 0;
  for (std::size_t i = 2; i < s.size(); ++i) {
    const auto c = s[i];
    const auto digit = c >= '0' && c <= '9'   ? c - '0'
                       : c >= 'a' && c <= 'f' ? c - 'a' + 10
                       : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                              : -1;
    Monstey_Require(digit >= 0, "Invalid hexadecimal address");
    n = (n << 4) | static_cast<unsigned>(digit);
  }
  return n;
}
struct Monstey_BankReader {
  S image;
  S At(std::uint64_t address, std::size_t n) const {
    return Monstey_FearPeRange(image, address, n);
  }
  std::string Text(std::uint64_t address) const {
    Monstey_Require(address != 0, "Null class/bank name pointer");
    std::string result;
    for (std::size_t i = 0; i < 4096; ++i) {
      Monstey_Require(address <= UINT64_MAX - i, "Native name overflow");
      const auto c = At(address + i, 1)[0];
      if (!c) {
        Monstey_Wide(result);
        return result;
      }
      result.push_back(static_cast<char>(c));
    }
    throw Monstey_Error("bank_text", "Unterminated native name");
  }
};
std::string Monstey_BankAsciiDiagnostic(std::string text) {
  for (auto &c : text)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + ('a' - 'A'));
  return text;
}
} // namespace
std::uint64_t Monstey_GadbBankRipLiteral(S instruction, std::uint64_t address,
                                         bool store) {
  Monstey_Require(instruction.size() == 7,
                  "Fixed bank RIP literal requires seven bytes");
  Monstey_Require(instruction[0] == 0x48 &&
                      instruction[1] == (store ? 0x89 : 0x8d) &&
                      instruction[2] == 0x05,
                  "Unsupported bank RIP literal opcode");
  Monstey_Require(address <= UINT64_MAX - 7,
                  "Bank instruction address overflow");
  const auto end = address + 7;
  const auto delta = std::bit_cast<std::int32_t>(Monstey_U32(instruction, 3));
  if (delta < 0) {
    const auto distance =
        static_cast<std::uint64_t>(-static_cast<std::int64_t>(delta));
    Monstey_Require(distance <= end, "Bank RIP literal address underflow");
    return end - distance;
  }
  Monstey_Require(end <= UINT64_MAX - static_cast<std::uint32_t>(delta),
                  "Bank RIP literal address overflow");
  return end + static_cast<std::uint32_t>(delta);
}
J Monstey_GadbBankRegistration(S image) {
  const auto sourceSha = Monstey_HashSha256(image);
  Monstey_Require(sourceSha == "6b8db778135c082c934463b6524e6b220478fb6987f082a"
                               "b37ca48a870acb455" ||
                      sourceSha == "ed0d95cabb033cd66aaa99d699374881846c3cb03ec"
                                   "345f1123d82d45e0bdaef",
                  "Unsupported bank source PE profile");
  const auto proof = J::Parse(Monstey_BankProfileData);
  Monstey_Require(proof.at("schema").as_string() ==
                          "Monstey.GadbBankCompiledProfile/1" &&
                      proof.at("banks").size() == 6,
                  "Unsupported bank proof schema");
  const Monstey_BankReader r{image};
  J banks = J::array();
  std::size_t total = 0, nativeAnchors = 0;
  std::map<std::uint64_t, J> allPointers;
  std::map<std::string, J> allNames;
  for (std::size_t bi = 0; bi < 6; ++bi) {
    const auto &b = proof.at("banks").at(bi);
    Monstey_Require(b.at("bank_index").as_uint() == bi,
                    "Bank table order differs");
    const auto entry = Monstey_BankAddress(b.at("entry")),
               enumerator = Monstey_BankAddress(b.at("enumerator")),
               array = Monstey_BankAddress(b.at("array"));
    Monstey_Require(entry == 0x1426397e0ull + 24 * bi,
                    "Native bank table slot differs");
    const auto raw = r.At(entry, 24);
    const auto name = r.Text(Monstey_U64(raw, 0));
    Monstey_Require(name == b.at("name").as_string() &&
                        Monstey_U64(raw, 8) == enumerator,
                    "Bank table name/enumerator differs");
    const auto count = b.at("count").as_uint();
    Monstey_Require(count <= 65536, "Invalid qualified bank recipe count");
    if (bi == 5) {
      Monstey_Require(Monstey_HashSha256(r.At(
                          enumerator, b.at("enumerator_bytes").as_uint())) ==
                          b.at("enumerator_sha256").as_string(),
                      "Empty bank leaf digest differs");
      Monstey_Require(
          count == 0 && Monstey_GadbBankRipLiteral(r.At(enumerator, 7),
                                                   enumerator, false) == array,
          "Empty native bank leaf differs");
      const auto leaf = r.At(enumerator + 7, 10);
      Monstey_Require(leaf[0] == 0xc7 && leaf[1] == 0x01 &&
                          Monstey_U32(leaf, 2) == 0 && leaf[6] == 0x48 &&
                          leaf[7] == 0x89 && leaf[8] == 0x02 && leaf[9] == 0xc3,
                      "Empty bank leaf return differs");
      ++nativeAnchors;
    } else {
      const auto init = Monstey_BankAddress(b.at("initializer"));
      const auto bytes =
          static_cast<std::size_t>(b.at("initializer_bytes").as_uint());
      Monstey_Require(bytes == count * 14 + 8 &&
                          Monstey_HashSha256(r.At(init, bytes)) ==
                              b.at("initializer_sha256").as_string(),
                      "Native bank constructor extent/hash differs");
      for (const auto &window : b.at("enumerator_windows").as_array()) {
        Monstey_Require(
            Monstey_HashSha256(r.At(Monstey_BankAddress(window.at("address")),
                                    window.at("bytes").as_uint())) ==
                window.at("sha256").as_string(),
            "Native enumerator digest differs");
        ++nativeAnchors;
      }
      const auto load =
                     Monstey_BankAddress(b.at("enumerator_array_instruction")),
                 store =
                     Monstey_BankAddress(b.at("enumerator_count_instruction"));
      Monstey_Require(Monstey_GadbBankRipLiteral(r.At(load, 7), load, false) ==
                          array,
                      "Enumerator returned array literal differs");
      const auto countBytes = r.At(store, 6);
      Monstey_Require(countBytes[0] == 0xc7 && countBytes[1] == 0x07 &&
                          Monstey_U32(countBytes, 2) == count,
                      "Enumerator class count literal differs");
      const auto finalLoad = Monstey_BankAddress(b.at("return_load_address")),
                 ret = Monstey_BankAddress(b.at("return_address"));
      Monstey_Require(finalLoad == init + count * 14 && ret == finalLoad + 7 &&
                          Monstey_GadbBankRipLiteral(
                              r.At(finalLoad, 7), finalLoad, false) == array &&
                          r.At(ret, 1)[0] == 0xc3,
                      "Constructor returned array differs");
    }
    J classes = J::array(), pointerDuplicates = J::array(),
      nameDuplicates = J::array(), asciiDuplicates = J::array();
    std::map<std::uint64_t, std::size_t> pointers;
    std::map<std::string, std::size_t> names, ascii;
    for (std::size_t i = 0; i < count; ++i) {
      const auto init = Monstey_BankAddress(b.at("initializer"));
      const auto slot = array + 8 * i, load = init + 14 * i, store = load + 7;
      const auto descriptor =
          Monstey_GadbBankRipLiteral(r.At(load, 7), load, false);
      Monstey_Require(Monstey_GadbBankRipLiteral(r.At(store, 7), store, true) ==
                          slot,
                      "Native ordered class destination differs");
      nativeAnchors += 2;
      const auto cl = r.At(descriptor, 64);
      const auto fullName = r.Text(Monstey_U64(cl, 32)),
                 shortName = r.Text(Monstey_U64(cl, 16));
      Monstey_Require(!fullName.empty() && !shortName.empty(),
                      "Empty native class name");
      const auto parent = Monstey_U64(cl, 40);
      J row = J::object(
          {{"array_index", i},
           {"class_pointer", Monstey_Hex(descriptor)},
           {"class_pointer_source", "native_constructor_store_literal"},
           {"bank_slot", Monstey_Hex(slot)},
           {"full_name", fullName},
           {"full_name_pointer_at_class_plus_20",
            Monstey_Hex(Monstey_U64(cl, 32))},
           {"short_name_at_class_plus_10", shortName},
           {"field_table", Monstey_Hex(Monstey_U64(cl, 0))},
           {"field_count_u32", Monstey_U32(cl, 8)},
           {"descriptor_qword_plus_08", Monstey_Hex(Monstey_U64(cl, 8))},
           {"descriptor_qword_plus_30", Monstey_Hex(Monstey_U64(cl, 48))},
           {"descriptor_qword_plus_38", Monstey_Hex(Monstey_U64(cl, 56))},
           {"parent_pointer_at_class_plus_28", Monstey_Hex(parent)},
           {"class_flags_u32_plus_38", Monstey_U32(cl, 56)},
           {"captured_file_initialized_byte_plus_3c", cl[60]},
           {"descriptor_sha256", Monstey_HashSha256(cl)}});
      row["introspection_status"] = "PRESENT_UNQUALIFIED";
      row["bank_membership_status"] = "STATIC_CONSTRUCTOR_PRESENT";
      row["unsupported_by_legacy_zero_30_shape"] = Monstey_U64(cl, 48) != 0;
      row["opaque_class_pointer_30_role"] =
          "UNKNOWN; not read by the selected complete RVA 0x79E2A0 route";
      if (Monstey_U64(cl, 48))
        row["opaque_class_pointer_30_head_sha256"] =
            Monstey_HashSha256(r.At(Monstey_U64(cl, 48), 16));
      if (parent)
        row["parent_full_name"] = r.Text(Monstey_U64(r.At(parent, 64), 32));
      const auto [p, pNew] = pointers.emplace(descriptor, i);
      if (!pNew)
        pointerDuplicates.as_array().push_back(
            J::object({{"first_index", p->second},
                       {"later_index", i},
                       {"class_pointer", Monstey_Hex(descriptor)}}));
      const auto [n, nNew] = names.emplace(fullName, i);
      row["first_exact_byte_name_index"] = n->second;
      if (!nNew)
        nameDuplicates.as_array().push_back(J::object(
            {{"name", fullName},
             {"first_index", n->second},
             {"later_index", i},
             {"first_pointer", classes.at(n->second).at("class_pointer")},
             {"later_pointer", Monstey_Hex(descriptor)}}));
      const auto [f, fNew] =
          ascii.emplace(Monstey_BankAsciiDiagnostic(fullName), i);
      if (!fNew)
        asciiDuplicates.as_array().push_back(
            J::object({{"diagnostic_ascii_folded_name", f->first},
                       {"first_index", f->second},
                       {"later_index", i}}));
      auto location = J::object({{"bank_index", bi},
                                 {"array_index", i},
                                 {"class_pointer", Monstey_Hex(descriptor)},
                                 {"full_name", fullName}});
      if (!allPointers.contains(descriptor))
        allPointers[descriptor] = J::array();
      allPointers[descriptor].as_array().push_back(location);
      if (!allNames.contains(fullName))
        allNames[fullName] = J::array();
      allNames[fullName].as_array().push_back(location);
      classes.as_array().push_back(std::move(row));
    }
    banks.as_array().push_back(
        J::object({{"bank_index", bi},
                   {"name", name},
                   {"entry_address", Monstey_Hex(entry)},
                   {"entry_sha256", Monstey_HashSha256(raw)},
                   {"enumerator", Monstey_Hex(enumerator)},
                   {"third_qword_not_used_by_outer_binder",
                    Monstey_Hex(Monstey_U64(raw, 16))},
                   {"returned_count", count},
                   {"returned_array", Monstey_Hex(array)},
                   {"array_values_source",
                    "complete_native_constructor_recipe_not_live_BSS"},
                   {"classes", classes},
                   {"duplicate_class_pointers", pointerDuplicates},
                   {"exact_byte_duplicate_full_names", nameDuplicates},
                   {"ascii_fold_duplicate_diagnostic_only", asciiDuplicates},
                   {"runtime_array_contents_verified", false}}));
    total += static_cast<std::size_t>(count);
  }
  J crossPointers = J::array(), crossNames = J::array();
  for (const auto &[p, locations] : allPointers)
    if (locations.size() > 1)
      crossPointers.as_array().push_back(J::object(
          {{"class_pointer", Monstey_Hex(p)}, {"locations", locations}}));
  for (const auto &[name, locations] : allNames)
    if (locations.size() > 1)
      crossNames.as_array().push_back(
          J::object({{"name", name}, {"locations", locations}}));
  return J::object(
      {{"schema", "Monstey.GadbBankRegistration/1"},
       {"status", "STATIC_BANK_ORDER_AND_CONSTRUCTOR_RECIPES_QUALIFIED"},
       {"PE_sha256", sourceSha},
       {"semantic_map_sha256", std::string(Monstey_BankMapSha)},
       {"semantic_map_required", false},
       {"compiled_profile_sha256",
        Monstey_HashSha256(S(reinterpret_cast<const std::uint8_t *>(
                                 Monstey_BankProfileData.data()),
                             Monstey_BankProfileData.size()))},
       {"native_anchor_check_model", "whole-profile and function digests plus "
                                     "fixed native-proved literal positions"},
       {"bank_table", proof.at("bank_table")},
       {"banks", banks},
       {"class_pointer_assignments", total},
       {"unique_class_pointers", allPointers.size()},
       {"unique_exact_byte_full_names", allNames.size()},
       {"repeated_class_pointers_across_all_banks", crossPointers},
       {"repeated_exact_byte_full_names_across_all_banks", crossNames},
       {"native_code_anchor_checks", nativeAnchors},
       {"binder_class_name_source",
        "QWORD[class+0x20], not short label at+0x10"},
       {"native_bank_and_class_selection",
        "first predicate match in the reported table/array order"},
       {"native_nonnull_name_comparator",
        "external cell 0x141d935b0; IDA annotation suggests _stricmp, external "
        "behavior/locale not qualified here"},
       {"ASCII_fold_is_only_a_diagnostic_not_native_equivalence", true},
       {"actual_prior_initialization_and_binding_effects", "UNKNOWN"},
       {"runtime_BSS_contents_verified", false},
       {"original_code_executed", false},
       {"game_memory_accessed", false},
       {"runtime_verified", false},
       {"checker_promoted", false}});
}
Monstey_Json Monstey_GadbBankRegistration(S image, S mapBytes) {
  Monstey_Require(Monstey_HashSha256(mapBytes) == Monstey_BankMapSha,
                  "Unqualified or forged native semantic map");
  auto report = Monstey_GadbBankRegistration(image);
  report["legacy_semantic_map_validated"] = true;
  return report;
}
} // namespace Monstey