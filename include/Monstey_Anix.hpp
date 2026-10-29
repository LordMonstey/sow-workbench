#pragma once
#include "Monstey_Core.hpp"
#include <array>
#include <memory>
namespace Monstey {
using Monstey_AnixVector = std::array<float, 3>;
using Monstey_AnixQuaternion = std::array<float, 4>;
struct Monstey_AnixScalarKeys {
    std::vector<std::uint32_t> ticks;
    std::vector<std::vector<float>> values;
};
struct Monstey_AnixQuaternionKeys {
    std::vector<std::uint32_t> ticks;
    std::vector<std::vector<Monstey_AnixQuaternion>> values;
};
// All byte APIs are offline and own their output. Native coordinates are centimetres,
// Y-up, XYZW; ticks are milliseconds. Unknown codecs are retained, never guessed.
Monstey_Bytes Monstey_AnixPackRow(std::span<const Monstey_AnixQuaternion> values);
std::vector<Monstey_AnixQuaternion> Monstey_AnixUnpackRow(std::span<const std::uint8_t> bytes,
                                                          std::size_t tracks, bool check_padding = true);
Monstey_Bytes Monstey_AnixEncodeAq03(const Monstey_AnixQuaternionKeys &values);
Monstey_AnixQuaternionKeys Monstey_AnixDecodeAq03(std::span<const std::uint8_t> bytes, std::size_t tracks);
Monstey_Bytes Monstey_AnixEncodeUatr(const std::vector<std::vector<Monstey_AnixVector>> &samples);
// Uncompressed native records store absolute u32 ticks and finite raw values.
// UAQT does not normalize quaternions; native sampling requires integer u32 ticks.
Monstey_Bytes Monstey_AnixEncodeUaqt(const Monstey_AnixQuaternionKeys &values);
Monstey_Bytes Monstey_AnixEncodeUavt(std::span<const std::uint32_t> ticks,
                                    const std::vector<std::vector<Monstey_AnixVector>> &values);
Monstey_Bytes Monstey_AnixEncodeCaf4(const Monstey_AnixScalarKeys &values);
Monstey_Bytes Monstey_AnixEncodeUsft(std::span<const std::uint32_t> raw_words);
Monstey_Bytes Monstey_AnixEncodeUsbt(std::span<const std::uint8_t> values);
// Auxiliary rows: CAF4 returns float32 numbers; USFT returns raw_u32 plus
// nullable float32 views; USBT returns unnormalized unsigned bytes.
Monstey_Json Monstey_AnixDecodeCodec(std::string_view codec, std::span<const std::uint8_t> bytes,
                                     std::size_t tracks, std::span<const std::uint32_t> lith_ticks,
                                     std::span<const double> sample_ticks,
                                     std::span<const std::uint8_t> key_strings = {});
struct Monstey_AnixSkeletonResult {
    Monstey_Bytes bytes;
    Monstey_Json report;
};
Monstey_Json Monstey_AnixSkeletonInspect(std::span<const std::uint8_t> bytes);
Monstey_AnixSkeletonResult Monstey_AnixSkeletonRegister(std::span<const std::uint8_t> bytes,
                                                        std::string_view resource);
class Monstey_Anix {
  public:
    explicit Monstey_Anix(std::span<const std::uint8_t> bytes);
    ~Monstey_Anix();
    Monstey_Anix(const Monstey_Anix &);
    Monstey_Anix &operator=(const Monstey_Anix &);
    Monstey_Anix(Monstey_Anix &&) noexcept;
    Monstey_Anix &operator=(Monstey_Anix &&) noexcept;
    static Monstey_Anix Create(const Monstey_Json &scene);
    Monstey_Json Describe() const;
    Monstey_Json Export() const;
    Monstey_Json ExportClip(std::string_view name) const;
    Monstey_Json EventInfo(std::string_view name) const;
    Monstey_Json Sample(std::string_view name, std::span<const double> ticks,
                        bool bind_fallback = true) const;
    // One call is transactional. Operations: rename, duplicate, delete, import,
    // held, events, rate, length, delay, trim. Full schemas are in anix-native.md.
    Monstey_Json Edit(const Monstey_Json &operations);
    Monstey_Bytes Build() const;

  private:
    struct Monstey_State;
    std::unique_ptr<Monstey_State> state_;
    explicit Monstey_Anix(std::unique_ptr<Monstey_State> state);
};
} // namespace Monstey
