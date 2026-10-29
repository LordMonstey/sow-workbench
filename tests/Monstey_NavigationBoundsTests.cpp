#include "Monstey_Core.hpp"
#include "Monstey_NavigationBounds.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
using namespace Monstey;
namespace {
using V = Monstey_NavigationVector4;
using T = Monstey_NavigationAffine4;
using B = Monstey_NavigationAabb;
T Identity() {
  return T{{V{1, 0, 0, 0}, V{0, 1, 0, 0}, V{0, 0, 1, 0}, V{0, 0, 0, 1}}};
}
float Add(float a, float b) {
  volatile float result = a + b;
  return result;
}
float Mul(float a, float b) {
  volatile float result = a * b;
  return result;
}
float Neg(float a) {
  return std::bit_cast<float>(std::bit_cast<std::uint32_t>(a) ^ 0x80000000u);
}
B Scalar(const T &t, const B &b, const V &e) {
  for (unsigned k = 0; k < 3; ++k)
    if (b.maximum[k] < b.minimum[k])
      return b;
  B out{};
  V c{}, h{};
  for (unsigned k = 0; k < 4; ++k) {
    c[k] = Mul(Add(b.minimum[k], b.maximum[k]), .5f);
    h[k] = Mul(Add(b.maximum[k], Neg(b.minimum[k])), .5f);
  }
  for (unsigned k = 0; k < 4; ++k) {
    const float radius =
        Add(Add(std::abs(Mul(h[0], t[0][k])), std::abs(Mul(h[1], t[1][k]))),
            Add(std::abs(Mul(h[2], t[2][k])), e[k]));
    const float center = Add(
        Add(Add(Mul(c[0], t[0][k]), Mul(c[1], t[1][k])), Mul(c[2], t[2][k])),
        t[3][k]);
    out.minimum[k] = Add(Neg(radius), center);
    out.maximum[k] = Add(center, radius);
  }
  return out;
}
void Equal(const B &a, const B &b) {
  for (unsigned k = 0; k < 4; ++k) {
    Monstey_Require(
        std::bit_cast<std::uint32_t>(a.minimum[k]) ==
            std::bit_cast<std::uint32_t>(b.minimum[k]),
        "Minimum differs bitwise from scalar native-order reference");
    Monstey_Require(
        std::bit_cast<std::uint32_t>(a.maximum[k]) ==
            std::bit_cast<std::uint32_t>(b.maximum[k]),
        "Maximum differs bitwise from scalar native-order reference");
  }
}
void Corners(const T &t, const B &b, const B &actual, const V &e) {
  for (unsigned k = 0; k < 4; ++k) {
    double low = std::numeric_limits<double>::infinity(), high = -low;
    for (unsigned mask = 0; mask < 8; ++mask) {
      double value = t[3][k];
      for (unsigned axis = 0; axis < 3; ++axis)
        value += static_cast<double>((mask & (1u << axis)) ? b.maximum[axis]
                                                           : b.minimum[axis]) *
                 t[axis][k];
      low = std::min(low, value);
      high = std::max(high, value);
    }
    low -= e[k];
    high += e[k];
    const double tolerance =
        2e-5 * (1 + std::max(std::abs(low), std::abs(high)));
    Monstey_Require(
        std::abs(actual.minimum[k] - low) <= tolerance &&
            std::abs(actual.maximum[k] - high) <= tolerance,
        "Transformed bound does not match independent eight-corner reference");
  }
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Monstey_Require(argc == 2, "Usage: NEW_REPORT");
    const std::filesystem::path out = argv[1];
    Monstey_RequireOfflineOutput(out);
    unsigned groups = 0, cases = 0;
    const B base{V{-3, -2, -1, 77}, V{5, 4, 3, 99}};
    auto Check = [&](const T &t, const B &b, const V &e) {
      const B got = Monstey_NavigationTransformedAabb(t, b, e);
      Equal(got, Scalar(t, b, e));
      ++cases;
      return got;
    };
    T t = Identity();
    Check(t, base, {});
    ++groups;
    t[3] = V{5, -8, 13, 4};
    Check(t, base, {});
    ++groups;
    t = Identity();
    t[0] = V{0, 1, 0, 0};
    t[1] = V{-1, 0, 0, 0};
    Check(t, base, {});
    ++groups;
    t = Identity();
    t[0][0] = 2;
    t[1][1] = 3;
    t[2][2] = .25f;
    Check(t, base, {});
    ++groups;
    t[0][0] = -2;
    t[1][1] = -3;
    Check(t, base, {});
    ++groups;
    t[1][0] = .7f;
    t[2][1] = -.125f;
    Check(t, base, {});
    ++groups;
    Check(t, base, V{.25f, .5f, .75f, 2});
    ++groups;
    B invalid = base;
    invalid.maximum[0] = -4;
    Equal(Check(t, invalid, {}), invalid);
    ++groups;
    invalid = base;
    invalid.maximum[2] = -2;
    Equal(Check(t, invalid, {}), invalid);
    ++groups;
    invalid = base;
    invalid.maximum[3] = 1;
    Check(t, invalid, {});
    ++groups;
    t = Identity();
    t[0][3] = .5f;
    t[1][3] = 3;
    t[2][3] = -1;
    auto opaque = Check(t, base, {});
    Monstey_Require(opaque.maximum[3] != 1 && opaque.minimum[3] != 1,
                    "W was incorrectly normalized");
    ++groups;
    B zero{V{-0.0f, 0.0f, -0.0f, 0}, V{0.0f, -0.0f, 0.0f, 0}};
    Check(Identity(), zero, {});
    ++groups;
    std::mt19937 random(0x26b7c8);
    std::uniform_real_distribution<float> value(-8, 8), positive(0, 4);
    for (unsigned i = 0; i < 4096; ++i) {
      B b{};
      V e{};
      for (unsigned k = 0; k < 4; ++k) {
        b.minimum[k] = value(random);
        b.maximum[k] = b.minimum[k] + positive(random);
        e[k] = positive(random);
        for (unsigned j = 0; j < 4; ++j)
          t[j][k] = value(random);
      }
      const B got = Check(t, b, e);
      Corners(t, b, got, e);
    }
    ++groups;
    const float qnan = std::bit_cast<float>(0x7fc12345u);
    invalid = base;
    invalid.minimum[3] = qnan;
    const B got = Monstey_NavigationTransformedAabb(Identity(), invalid);
    Monstey_Require(std::isfinite(got.minimum[0]) &&
                        std::isfinite(got.maximum[2]),
                    "Ignored input W contaminated XYZ");
    ++cases;
    ++groups;
    Monstey_WriteNewText(
        out, Monstey_Json::object(
                 {{"schema", "Monstey.NavigationBoundsTests/1"},
                  {"groups", groups},
                  {"owned_input_cases", cases},
                  {"independent_eight_corner_cases", 4096},
                  {"native_program_called", false},
                  {"process_accessed", false},
                  {"status", "PASS"},
                  {"scope", "Native-order arithmetic and finite affine "
                            "eight-corner agreement; no nav loading, mediator, "
                            "seam, routing or game qualification"}})
                     .dump(2) +
                 "\n");
    std::cout << "PASS " << groups << " bounds groups, " << cases
              << " owned-input cases\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}