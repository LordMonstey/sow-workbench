#include "Monstey_GadbBanks.hpp"
#include <array>
#include <bit>
#include <functional>
#include <iostream>
#include <random>
using namespace Monstey;
int main() {
  try {
    unsigned groups = 0;
    std::mt19937 random(0x26397e0u);
    auto reject = [](const std::function<void()> &f) {
      bool rejected = false;
      try {
        f();
      } catch (const std::exception &) {
        rejected = true;
      }
      Monstey_Require(rejected, "Invalid literal accepted");
    };
    for (const bool store : {false, true})
      for (unsigned i = 0; i < 4096; ++i) {
        const auto word = random();
        const auto delta = std::bit_cast<std::int32_t>(word);
        Monstey_Bytes bytes{
            0x48, static_cast<std::uint8_t>(store ? 0x89 : 0x8d), 0x05, 0, 0, 0,
            0};
        Monstey_PutU32(bytes, 3, word);
        const std::uint64_t address = 0x140000000ull + i * 16;
        const auto expected = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(address) + 7 + delta);
        Monstey_Require(Monstey_GadbBankRipLiteral(bytes, address, store) ==
                            expected,
                        "Signed RIP displacement differs");
      }
    ++groups;
    Monstey_Bytes bytes{0x48, 0x8d, 0x05, 0, 0, 0, 0};
    for (unsigned position = 0; position < 3; ++position) {
      auto changed = bytes;
      changed[position] ^= 0x10;
      reject([&] {
        (void)Monstey_GadbBankRipLiteral(changed, 0x140000000ull, false);
      });
    }
    reject(
        [&] { (void)Monstey_GadbBankRipLiteral(bytes, 0x140000000ull, true); });
    reject([&] {
      (void)Monstey_GadbBankRipLiteral(std::span(bytes).first(6), 0, false);
    });
    ++groups;
    reject([&] { (void)Monstey_GadbBankRipLiteral(bytes, UINT64_MAX, false); });
    Monstey_PutU32(bytes, 3, 0x7fffffffu);
    reject([&] {
      (void)Monstey_GadbBankRipLiteral(bytes, UINT64_MAX - 8, false);
    });
    Monstey_PutU32(bytes, 3, 0x80000000u);
    reject([&] { (void)Monstey_GadbBankRipLiteral(bytes, 0, false); });
    Monstey_Require(
        Monstey_GadbBankRipLiteral(bytes, 0x80000000ull - 7, false) == 0,
        "INT32_MIN endpoint differs");
    ++groups;
    reject([&] { (void)Monstey_GadbBankRegistration(Monstey_Bytes(128)); });
    ++groups;
    std::cout << "PASS " << groups
              << " native bank literal groups,8192 signed cases\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}