#include "Monstey_GadbBanks.hpp"
#include <array>
#include <iostream>
using namespace Monstey;
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Require(argc == 4 || argc == 5,
                    "Usage: GadbBankRegistration IMAGE IMAGE_SHA "
                    "[OPTIONAL_LEGACY_MAP] NEW_REPORT");
    const std::filesystem::path source = argv[1], output = argv[argc - 1];
    Monstey_RequireOfflineOutput(output, std::array{source});
    const auto image = Monstey_ReadFile(source, 256ull * 1024 * 1024);
    const auto expected = Monstey_Utf8(argv[2]);
    Monstey_Require(Monstey_HashSha256(image) == expected,
                    "Source PE identity differs");
    Monstey_Json result;
    if (argc == 5) {
      const std::filesystem::path map = argv[3];
      Monstey_RequireOfflineOutput(output, std::array{source, map});
      const auto bytes = Monstey_ReadFile(map, 4ull * 1024 * 1024);
      result = Monstey_GadbBankRegistration(image, bytes);
      Monstey_Require(Monstey_HashSha256(map) == Monstey_HashSha256(bytes),
                      "Legacy map changed during read");
    } else
      result = Monstey_GadbBankRegistration(image);
    Monstey_Require(Monstey_HashSha256(source) == expected,
                    "Source changed during read");
    result["source_unchanged"] = true;
    Monstey_WriteNewText(output, result.dump(2) + "\n");
    std::cout << "PASS " << result.at("banks").size() << " banks, "
              << result.at("class_pointer_assignments").as_uint()
              << " class references; self-contained C++ reader\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}