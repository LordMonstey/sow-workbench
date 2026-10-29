#include "Monstey_GadbClassDeclaration.hpp"
#include <iostream>
#include <vector>
using namespace Monstey;
namespace {
unsigned checks = 0, groups = 0;
void Check(bool value, const char *message) {
  ++checks;
  Monstey_Require(value, message);
}
void Case(std::size_t roots, std::size_t tail, bool accepted,
          std::size_t expected) {
  constexpr std::uint64_t base = 0x140000000, va = base + 0x1000;
  const auto all = roots + tail;
  Monstey_Bytes b(0x200 + all * 64, 0);
  Monstey_PutU16(b, 0, 0x5a4d);
  Monstey_PutU32(b, 0x3c, 0x80);
  Monstey_PutU32(b, 0x80, 0x4550);
  Monstey_PutU16(b, 0x84, 0x8664);
  Monstey_PutU16(b, 0x86, 1);
  Monstey_PutU16(b, 0x94, 0xf0);
  Monstey_PutU16(b, 0x98, 0x20b);
  Monstey_PutU64(b, 0x98 + 24, base);
  Monstey_PutU32(b, 0x188 + 8, static_cast<std::uint32_t>(all * 64));
  Monstey_PutU32(b, 0x188 + 12, 0x1000);
  Monstey_PutU32(b, 0x188 + 16, static_cast<std::uint32_t>(all * 64));
  Monstey_PutU32(b, 0x188 + 20, 0x200);
  std::vector<std::uint64_t> requested;
  for (std::size_t i = 0; i < roots; ++i)
    requested.push_back(va + 64 * i);
  if (tail)
    requested.push_back(va + 64 * roots);
  for (std::size_t i = roots; i + 1 < all; ++i)
    Monstey_PutU64(b, 0x200 + 64 * i + 40, va + 64 * (i + 1));
  const auto original = b;
  bool pass = false;
  std::size_t returned = 0;
  try {
    const auto j = Monstey_GadbInspectClassPointers(b, requested);
    returned = j.at("classes").size();
    pass = true;
  } catch (const Monstey_Error &) {
  }
  Check(pass == accepted, "Class graph acceptance mismatch");
  if (pass)
    Check(returned == expected, "Valid bounded class graph lost identities");
  Check(b == original, "Class row guard changed source bytes");
  ++groups;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Check(argc == 2, "Need fresh output report");
    Case(4095, 64, false, 0);
    Case(4033, 64, false, 0);
    Case(4032, 64, true, 4096);
    Case(4095, 1, true, 4096);
    Case(4096, 0, true, 4096);
    Case(0, 64, true, 64);
    Case(0, 65, false, 0);
    constexpr std::uint64_t va = 0x140001000;
    Monstey_Bytes b(0x240, 0);
    Monstey_PutU16(b, 0, 0x5a4d);
    Monstey_PutU32(b, 0x3c, 0x80);
    Monstey_PutU32(b, 0x80, 0x4550);
    Monstey_PutU16(b, 0x84, 0x8664);
    Monstey_PutU16(b, 0x86, 1);
    Monstey_PutU16(b, 0x94, 0xf0);
    Monstey_PutU16(b, 0x98, 0x20b);
    Monstey_PutU64(b, 0x98 + 24, 0x140000000);
    Monstey_PutU32(b, 0x188 + 8, 64);
    Monstey_PutU32(b, 0x188 + 12, 0x1000);
    Monstey_PutU32(b, 0x188 + 16, 64);
    Monstey_PutU32(b, 0x188 + 20, 0x200);
    const std::vector<std::uint64_t> repeated(4096, va);
    const auto j = Monstey_GadbInspectClassPointers(b, repeated);
    Check(j.at("classes").size() == 1 &&
              j.at("requested_class_pointers").size() == 4096,
          "Repeated pointer consumed new identity budget");
    ++groups;
    Monstey_WriteNewText(argv[1], Monstey_Json::object({{"pass", true},
                                                        {"groups", groups},
                                                        {"assertions", checks}})
                                          .dump(2) +
                                      "\n");
    std::cout << "PASS " << groups << " groups " << checks << " assertions\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
