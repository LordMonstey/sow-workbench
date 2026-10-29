#pragma once
#include "Monstey_Json.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Monstey {
using Monstey_Bytes = std::vector<std::uint8_t>;
class Monstey_Error : public std::runtime_error {
public:
    std::string code;
    Monstey_Error(std::string error_code, std::string message)
        : std::runtime_error(std::move(message)), code(std::move(error_code)) {}
};
void Monstey_Require(bool condition, std::string_view message);
void Monstey_CheckRange(std::span<const std::uint8_t> data, std::size_t offset, std::size_t count);
std::uint16_t Monstey_U16(std::span<const std::uint8_t> data, std::size_t offset);
std::uint32_t Monstey_U32(std::span<const std::uint8_t> data, std::size_t offset);
std::uint64_t Monstey_U64(std::span<const std::uint8_t> data, std::size_t offset);
void Monstey_PutU16(Monstey_Bytes& data, std::size_t offset, std::uint16_t value);
void Monstey_PutU32(Monstey_Bytes& data, std::size_t offset, std::uint32_t value);
void Monstey_PutU64(Monstey_Bytes& data, std::size_t offset, std::uint64_t value);
void Monstey_AppendU16(Monstey_Bytes& data, std::uint16_t value);
void Monstey_AppendU32(Monstey_Bytes& data, std::uint32_t value);
void Monstey_AppendU64(Monstey_Bytes& data, std::uint64_t value);
std::filesystem::path Monstey_WindowsFilePath(const std::filesystem::path& input);
Monstey_Bytes Monstey_ReadFile(const std::filesystem::path& path, std::uint64_t maximum = 512ull * 1024 * 1024);
std::string Monstey_ReadText(const std::filesystem::path& path, std::uint64_t maximum = 32ull * 1024 * 1024);
void Monstey_WriteNewFile(const std::filesystem::path& path, std::span<const std::uint8_t> data);
void Monstey_WriteNewText(const std::filesystem::path& path, std::string_view text);
void Monstey_RequireOfflineOutput(const std::filesystem::path& path, std::span<const std::filesystem::path> sources = {});
std::string Monstey_HashSha256(const std::filesystem::path& path);
std::string Monstey_HashSha256(std::span<const std::uint8_t> data);
std::string Monstey_HashSha256Chunks(std::uint64_t length,
    const std::function<Monstey_Bytes(std::uint64_t,std::uint64_t)> &reader);
std::uint32_t Monstey_NativeHash(std::string_view bytes, std::uint32_t seed = 0);
std::uint32_t Monstey_PathHash(std::string_view bytes, std::uint32_t seed = 0);
std::wstring Monstey_Wide(std::string_view utf8);
std::string Monstey_Utf8(std::wstring_view text);
std::string Monstey_DisplayUtf8(std::string_view bytes);
std::string Monstey_Hex(std::uint64_t value, unsigned width = 8);
}
