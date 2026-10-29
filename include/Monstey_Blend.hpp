#pragma once
#include "Monstey_Core.hpp"
#include <memory>
namespace Monstey {
class Monstey_Blend {
  public:
    explicit Monstey_Blend(const std::filesystem::path &source);
    ~Monstey_Blend();
    Monstey_Blend(Monstey_Blend &&) noexcept;
    Monstey_Blend &operator=(Monstey_Blend &&) noexcept;
    Monstey_Blend(const Monstey_Blend &) = delete;
    Monstey_Blend &operator=(const Monstey_Blend &) = delete;
    Monstey_Json Describe() const;
    Monstey_Json Structs(std::string_view query = {}) const;
    Monstey_Json Fields(std::string_view type) const;
    Monstey_Json Blocks(std::string_view code = {}, std::size_t offset = 0, std::size_t limit = 250) const;
    Monstey_Json ReadField(std::size_t block, std::string_view path, std::size_t element = 0) const;
    Monstey_Json SetField(std::size_t block, std::string_view path, const Monstey_Json &value,
                          std::size_t element = 0);
    Monstey_Bytes BytesAtAddress(std::uint64_t address, std::size_t length, std::size_t owner=static_cast<std::size_t>(-1)) const;
    // New typed blocks use zero-filled SDNA records and unique file-pointer IDs.
    Monstey_Json NewBlock(std::string_view code, std::string_view type, std::size_t count = 1);
    std::size_t BlockAtAddress(std::uint64_t address, std::size_t owner=static_cast<std::size_t>(-1)) const;
    Monstey_Bytes BlockData(std::size_t block) const;
    Monstey_Json CloneBlock(std::size_t block);
    Monstey_Json NewData(Monstey_Bytes data);
    void SetDataOwner(std::size_t block, std::size_t owner);
    void ReplaceTypedData(std::size_t block, Monstey_Bytes data);
    void ReplaceData(std::size_t block, Monstey_Bytes data);
    Monstey_Json Write(const std::filesystem::path &output) const;

  private:
    struct Monstey_Impl;
    std::unique_ptr<Monstey_Impl> impl_;
};
} // namespace Monstey
