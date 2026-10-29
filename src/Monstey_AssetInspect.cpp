#include "Monstey_AssetInspect.hpp"
#include "Monstey_TextureResource.hpp"
#include "Monstey_LtmiMaterial.hpp"
#include "Monstey_Catalog.hpp"
#include "Monstey_Model.hpp"
#include "Monstey_WorldComposition.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
std::string Monstey_IText(std::span<const std::uint8_t> bytes) {
  return Monstey_DisplayUtf8(
      std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
}
std::string Monstey_IString(std::span<const std::uint8_t> bytes,
                            std::size_t at) {
  Monstey_CheckRange(bytes, at, 1);
  const auto begin = bytes.begin() + static_cast<std::ptrdiff_t>(at),
             end = std::find(begin, bytes.end(), 0);
  Monstey_Require(end != bytes.end(), "Unterminated material string");
  return Monstey_IText(
      bytes.subspan(at, static_cast<std::size_t>(end - begin)));
}
J Monstey_IFloat(std::span<const std::uint8_t> bytes, std::size_t at) {
  const auto bits = Monstey_U32(bytes, at);
  const auto value = std::bit_cast<float>(bits);
  return std::isfinite(value)
             ? J(double(value))
             : J::object({{"float32_bits", bits}, {"nonfinite", true}});
}
J Monstey_IFloats(std::span<const std::uint8_t> bytes, std::size_t at,
                  std::size_t count) {
  auto out = J::array();
  for (std::size_t i = 0; i < count; ++i)
    out.as_array().push_back(Monstey_IFloat(bytes, at + i * 4));
  return out;
}
} // namespace
J Monstey_MaterialInspect(std::span<const std::uint8_t> bytes) {
  const auto material = Monstey_LtmiRead(bytes);
  const auto native = Monstey_LtmiDescribe(material);
  const std::size_t base = 30 + material.shader_name.size();
  auto shaderName = Monstey_IText(material.shader_name);
  while (!shaderName.empty() && shaderName.back() == 0)
    shaderName.pop_back();
  const auto hex = [](std::span<const std::uint8_t> data) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(data.size() * 2);
    for (const auto byte : data) {
      result.push_back(digits[byte >> 4]);
      result.push_back(digits[byte & 15]);
    }
    return result;
  };
  auto properties = J::object(), entries = J::array();
  std::uint64_t duplicates = 0;
  for (std::size_t table = 0; table < material.tables.size(); ++table)
    for (std::size_t ordinal = 0; ordinal < material.tables[table].size(); ++ordinal) {
      const auto &property = material.tables[table][ordinal];
      const auto kind = std::uint32_t(property.type) |
                        (std::uint32_t(property.flag_a) << 16) |
                        (std::uint32_t(property.flag_b) << 24);
      const auto type = static_cast<std::uint8_t>(property.type);
      auto name = Monstey_IText(property.name);
      while (!name.empty() && name.back() == 0)
        name.pop_back();
      const auto payload = Monstey_LtmiValue(material, property);
      const auto offset = static_cast<std::uint16_t>(property.offset);
      const auto valueAt = static_cast<std::uint64_t>(base) + offset;
      J value;
      switch (type) {
      case 1:
        value = Monstey_IString(payload, 0);
        break;
      case 2:
        value = Monstey_IFloats(payload, 0, 3);
        break;
      case 3:
        value = Monstey_IFloats(payload, 0, 4);
        break;
      case 4:
        value = Monstey_U32(payload, 0);
        break;
      case 5:
        value = Monstey_IFloat(payload, 0);
        break;
      case 6:
        value = Monstey_U32(payload, 0) != 0;
        break;
      case 8:
        // A bounded observed file string is optional; the native copy size is0.
        value = payload.empty() ? J(nullptr) : J(Monstey_IString(payload, 0));
        break;
      case 9:
        value = Monstey_IFloats(payload, 0, 16);
        break;
      case 10:
        value = Monstey_IFloats(payload, 0, 12);
        break;
      case 11:
        value = Monstey_IFloats(payload, 0, 2);
        break;
      default:
        value = J::object({{"unsupported_type", kind}, {"offset", valueAt}});
        break;
      }
      if (properties.contains(name))
        ++duplicates;
      properties[name] = value;
      auto entry = native.at("entries").at(entries.size());
      entry["name"] = name;
      entry["kind"] = kind; // Public compatibility: all four stored metadata bytes.
      entry["type"] = kind;
      entry["table"] = table == 0 ? "a" : "b";
      entry["ordinal"] = static_cast<std::uint64_t>(ordinal);
      entry["value_offset"] = valueAt;
      entry["stored_value_offset"] = static_cast<std::uint64_t>(base) + property.offset;
      entry["raw_name_hex"] = hex(property.name);
      entry["value_bytes_hex"] = hex(payload);
      entry["value"] = value;
      entries.as_array().push_back(std::move(entry));
    }
  const auto end = bytes.size() - material.trailer.size();
  return J::object(
      {{"format", "LTMI"},
       {"version", material.version},
       {"shader_id", material.shader_id},
       {"shader_key", material.shader_id},
       {"shader", shaderName},
       {"shader_name", shaderName},
       {"property_bytes", static_cast<std::uint64_t>(material.values.size())},
       {"table_a_count", static_cast<std::uint64_t>(material.tables[0].size())},
       {"table_b_count", static_cast<std::uint64_t>(material.tables[1].size())},
       {"parameter_count", static_cast<std::uint64_t>(entries.size())},
       {"properties", properties},
       {"properties_duplicate_name_policy", "last_entry_wins_in_table_a_then_b_order"},
       {"duplicate_display_name_count", duplicates},
       {"entries", entries},
       {"metadata_end", static_cast<std::uint64_t>(end)},
       {"trailing_bytes", static_cast<std::uint64_t>(material.trailer.size())},
       {"source_sha256", Monstey_HashSha256(bytes)},
       {"native_layout", native}});
}
J Monstey_TextureInspect(std::span<const std::uint8_t> bytes) {
  Monstey_TextureResource texture(bytes);
  const auto native = texture.Describe();
  auto result = J::object({{"format", native.at("container")},
      {"source_sha256", Monstey_HashSha256(bytes)}, {"native_layout", native}});
  if (native.contains("native_logical_dimensions")) {
    const auto &dimensions=native.at("native_logical_dimensions");
    result["logical_dimensions"]=J::array({dimensions.at(0),dimensions.at(1)});
    const auto length=Monstey_U16(bytes,16);
    result["peer"]=Monstey_IText(bytes.subspan(18,length));
  }
  const auto dds=static_cast<std::size_t>(native.at("dds_offset").as_uint());
  if (dds || native.at("container")==J("DDS")) {
    Monstey_CheckRange(bytes,dds,128);
    result["dds_dimensions"]=J::array({Monstey_U32(bytes,dds+16),Monstey_U32(bytes,dds+12)});
    result["dds_mips"]=Monstey_U32(bytes,dds+28);
    result["fourcc"]=Monstey_IText(bytes.subspan(dds+84,4));
    result["dds_offset"]=static_cast<std::uint64_t>(dds);
    result["dds_bytes"]=static_cast<std::uint64_t>(bytes.size()-dds);
    result["dds_sha256"]=Monstey_HashSha256(bytes.subspan(dds));
  }
  return result;
}
J Monstey_AssetInspect(const std::filesystem::path &source) {
  std::ifstream input(Monstey_WindowsFilePath(source), std::ios::binary);
  B prefix(4);
  input.read(reinterpret_cast<char *>(prefix.data()), 4);
  Monstey_Require(input.gcount() == 4, "Truncated asset header");
  if (Monstey_U32(prefix, 0) == 0x42424d45) {
    auto report = Monstey_InspectEmbeddedResource(source);
    report["path"] = Monstey_Utf8(source.wstring());
    report["implementation_language"] = "C++20";
    return report;
  }
  const auto bytes = Monstey_ReadFile(source);
  const auto magic = Monstey_U32(bytes, 0);
  J result;
  if (magic == 0x48534d4d || magic == 0x46546c67)
    result = Monstey_ModelInspectBytes(bytes);
  else if (magic == 0x4c454b53)
    result = Monstey_ModelInspectSkeleton(bytes);
  else if (magic == 0x494d544c)
    result = J::object(
        {{"format", "LTMI"}, {"material", Monstey_MaterialInspect(bytes)}});
  else if (magic == 0x52584554 || magic == 0x41584554 ||
           magic == 0x54555043 || magic == 0x20534444)
    result = Monstey_TextureInspect(bytes);
  else if (magic == 0x4c444e42) {
    const auto bundle = Monstey_ReadBundle(bytes);
    auto rows = J::array();
    for (const auto &row : bundle.resources)
      rows.as_array().push_back(
          J::object({{"path", row.resource}, {"size_hint", row.size_hint}}));
    result = J::object({{"format", "BNDL"},
                        {"version", Monstey_U32(bytes, 4)},
                        {"kind", bundle.kind},
                        {"resources", rows}});
  } else
    throw Monstey_Error(
        "unsupported_asset_format",
        "Supported asset formats: MMSH, SKEL, LTMI, TEXR/TEXA/DDS/CPUT, BNDL, EMBB and GLB2");
  if (result.contains("source_sha256"))
    Monstey_Require(result.at("source_sha256").as_string() ==
                        Monstey_HashSha256(bytes),
                    "Native inspector snapshot differs from asset input");
  const auto hash = Monstey_HashSha256(bytes);
  Monstey_Require(Monstey_HashSha256(source) == hash,
                  "Asset source changed during inspection");
  result["source"] = Monstey_Utf8(source.wstring());
  result["path"] = Monstey_Utf8(source.wstring());
  result["bytes"] = static_cast<std::uint64_t>(bytes.size());
  result["sha256"] = hash;
  result["implementation_language"] = "C++20";
  result["runtime_verified"] = false;
  return result;
}
} // namespace Monstey