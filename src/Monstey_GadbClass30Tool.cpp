#include "Monstey_GadbBanks.hpp"
#include "Monstey_GadbClassDeclaration.hpp"
#include <array>
#include <iostream>
using namespace Monstey;
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Require(argc == 4,
                    "Usage: GadbClass30 IMAGE EXPECTED_SHA256 NEW_REPORT");
    const std::filesystem::path source = argv[1], output = argv[3];
    const auto expected = Monstey_Utf8(argv[2]);
    Monstey_RequireOfflineOutput(output, std::array{source});
    const auto image = Monstey_ReadFile(source, 256ull * 1024 * 1024);
    Monstey_Require(Monstey_HashSha256(image) == expected,
                    "Source identity differs");
    const auto banks = Monstey_GadbBankRegistration(image);
    std::vector<std::uint64_t> pointers;
    auto membership = Monstey_Json::array();
    for (const auto &bank : banks.at("banks").as_array())
      for (const auto &row : bank.at("classes").as_array())
        if (std::stoull(row.at("descriptor_qword_plus_30").as_string(), nullptr,
                        0)) {
          pointers.push_back(
              std::stoull(row.at("class_pointer").as_string(), nullptr, 0));
          auto selected = row;
          selected["bank_index"] = bank.at("bank_index");
          selected["bank_name"] = bank.at("name");
          membership.as_array().push_back(std::move(selected));
        }
    auto result = Monstey_GadbInspectClassPointers(image, pointers, expected);
    result["static_bank_memberships"] = std::move(membership);
    const auto bankJson = banks.dump();
    result["static_bank_recipe_report_sha256"] =
        Monstey_HashSha256(std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t *>(bankJson.data()),
            bankJson.size()));
    // The bank reader revalidates its fixed constructor profiles. Runtime
    // BSS, name selection, callback execution and game loading are absent.
    result["bank_source"] = "QUALIFIED_STATIC_CONSTRUCTOR_RECIPE";
    result["source_unchanged"] = Monstey_HashSha256(source) == expected;
    Monstey_Require(result.at("source_unchanged").as_bool(),
                    "Source changed during read");
    Monstey_WriteNewText(output, result.dump(2) + "\n");
    std::cout << "PASS " << pointers.size()
              << " explicit class+30 bank references; "
              << result.at("classes").size()
              << " pointer identities including parents\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
