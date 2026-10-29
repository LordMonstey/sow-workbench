#pragma once
#include "Monstey_Core.hpp"
#include <array>
#include <optional>
namespace Monstey
{
struct Monstey_LtmiProperty
{
    std::uint16_t type = 0;
    std::uint8_t flag_a = 0, flag_b = 0;
    Monstey_Bytes name;
    std::uint32_t offset = 0;
    std::uint16_t legacy_word = 0;
};
struct Monstey_LtmiMaterial
{
    std::uint32_t version = 8, shader_id = 0, unknown_20 = 0, unknown_24 = 0;
    Monstey_Bytes shader_name, values, legacy_blob, trailer, source;
    std::array<std::vector<Monstey_LtmiProperty>, 2> tables;
};
std::optional<std::uint8_t> Monstey_LtmiNativeSize(std::uint16_t type);
std::uint32_t Monstey_LtmiPropertyHash(const Monstey_LtmiProperty &property);
Monstey_LtmiMaterial Monstey_LtmiRead(std::span<const std::uint8_t> bytes);
Monstey_Bytes Monstey_LtmiBuild(const Monstey_LtmiMaterial &material);
Monstey_Bytes Monstey_LtmiValue(const Monstey_LtmiMaterial &material, const Monstey_LtmiProperty &property);
void Monstey_LtmiSetValue(Monstey_LtmiMaterial &material, std::size_t table, std::size_t index,
                          std::span<const std::uint8_t> value);
void Monstey_LtmiAdd(Monstey_LtmiMaterial &material, std::size_t table, Monstey_LtmiProperty property,
                     std::span<const std::uint8_t> value);
void Monstey_LtmiRemove(Monstey_LtmiMaterial &material, std::size_t table, std::size_t index);
void Monstey_LtmiRename(Monstey_LtmiMaterial &material, std::size_t table, std::size_t index,
                        std::span<const std::uint8_t> name);
Monstey_Json Monstey_LtmiDescribe(const Monstey_LtmiMaterial &material);
} // namespace Monstey