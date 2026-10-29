#include "Monstey_Core.hpp"
#include "Monstey_StaticNavFaceBounds.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
namespace {
using namespace Monstey;
unsigned groups = 0;
void Check(bool value, const char *message) { Monstey_Require(value, message); }
void PutVertex(Monstey_Bytes &bytes, std::size_t n,
               const std::array<float, 4> &lanes) {
  std::memcpy(bytes.data() + 16 * n, lanes.data(), 16);
}
void Reject(const std::function<void()> &call) {
  bool failed = false;
  try {
    call();
  } catch (const std::exception &) {
    failed = true;
  }
  Check(failed, "Expected owned-range rejection");
  ++groups;
}
void Equal(float actual, float expected) {
  Check(std::bit_cast<std::uint32_t>(actual) ==
            std::bit_cast<std::uint32_t>(expected),
        "Float bits differ");
}
float NativeMin(float a, float b) {
  return std::isnan(a) || std::isnan(b) || a == b ? b : std::min(a, b);
}
float NativeMax(float a, float b) {
  return std::isnan(a) || std::isnan(b) || a == b ? b : std::max(a, b);
}
} // namespace
int main() {
  try {
    Monstey_Bytes faces(16), edges(60), vertices(48);
    Monstey_PutU16(faces, 8, 3);
    for (std::uint32_t n = 0; n < 3; ++n)
      Monstey_PutU32(edges, 20 * n, n);
    PutVertex(vertices, 0, {-5, 2, 8, 17});
    PutVertex(vertices, 1, {4, -3, 1, 23});
    PutVertex(vertices, 2, {2, 9, -7, -11});
    auto result = Monstey_StaticNavFaceBounds(faces, edges, vertices, 0);
    Check(result.minimum == std::array<float, 4>{-5, -3, -7, -11},
          "Four-lane minimum");
    Check(result.maximum == std::array<float, 4>{4, 9, 8, 23},
          "Four-lane maximum");
    Check(result.vertex_iterations == 3, "Three edge-start vertex loads");
    ++groups;
    auto padded =
        Monstey_StaticNavFaceBounds(faces, edges, vertices, 0, {1, 2, 3, 4});
    Check(padded.minimum == std::array<float, 4>{-6, -5, -10, -15},
          "Padding minimum");
    Check(padded.maximum == std::array<float, 4>{5, 11, 11, 27},
          "Padding maximum");
    ++groups;
    for (const auto count : std::array<std::uint16_t, 3>{0, 0xffff, 0x8000}) {
      Monstey_PutU16(faces, 8, count);
      Monstey_PutU32(faces, 0, 0xffffffff);
      result = Monstey_StaticNavFaceBounds(faces, {}, {}, 0);
      Check(result.vertex_iterations == 0,
            "Signed nonpositive count must not load vertices");
      Equal(result.minimum[0], std::bit_cast<float>(std::uint32_t{0x7f7fffee}));
      Equal(result.maximum[0], std::bit_cast<float>(std::uint32_t{0xff7fffee}));
      ++groups;
    }
    Monstey_PutU16(faces, 8, 3);
    Monstey_PutU32(faces, 0, 0);
    Reject([&] {
      Monstey_StaticNavFaceBounds(std::span(faces).first(15), edges, vertices,
                                  0);
    });
    Reject([&] {
      Monstey_StaticNavFaceBounds(faces, std::span(edges).first(59), vertices,
                                  0);
    });
    Reject([&] {
      Monstey_StaticNavFaceBounds(faces, edges, std::span(vertices).first(47),
                                  0);
    });
    Reject([&] { Monstey_StaticNavFaceBounds(faces, edges, vertices, 1); });
    Monstey_PutU32(faces, 0, 0xffffffff);
    Reject([&] { Monstey_StaticNavFaceBounds(faces, edges, vertices, 0); });
    Monstey_PutU32(faces, 0, 1);
    Reject([&] { Monstey_StaticNavFaceBounds(faces, edges, vertices, 0); });
    Monstey_PutU32(faces, 0, 0);
    Monstey_PutU32(edges, 0, 3);
    Reject([&] { Monstey_StaticNavFaceBounds(faces, edges, vertices, 0); });
    Monstey_PutU32(edges, 0, 0xffffffff);
    Reject([&] { Monstey_StaticNavFaceBounds(faces, edges, vertices, 0); });
    Monstey_PutU32(edges, 0, 0);
    // Independent scalar SSE selection oracle; original code is not called.
    std::mt19937 random(0x3cd554);
    const auto scalar = [&](float x) {
      return std::round(x * 128.0f) / 128.0f;
    };
    std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
    for (unsigned test = 0; test < 1024; ++test) {
      std::array<float, 4> lo, hi, pad;
      lo.fill(std::bit_cast<float>(std::uint32_t{0x7f7fffee}));
      hi.fill(std::bit_cast<float>(std::uint32_t{0xff7fffee}));
      for (auto &x : pad)
        x = scalar(dist(random) / 100.0f);
      for (unsigned n = 0; n < 3; ++n) {
        std::array<float, 4> value;
        for (unsigned lane = 0; lane < 4; ++lane) {
          value[lane] = scalar(dist(random));
          if (test % 19 == 0 && n == 1)
            value[lane] = std::bit_cast<float>(std::uint32_t{0x7fc12345});
          if (test % 17 == 0 && lane == 3)
            value[lane] = n % 2 ? -0.0f : 0.0f;
          lo[lane] = NativeMin(lo[lane], value[lane]);
          hi[lane] = NativeMax(hi[lane], value[lane]);
        }
        PutVertex(vertices, n, value);
      }
      result = Monstey_StaticNavFaceBounds(faces, edges, vertices, 0, pad);
      for (unsigned lane = 0; lane < 4; ++lane) {
        Equal(result.minimum[lane], lo[lane] - pad[lane]);
        Equal(result.maximum[lane], hi[lane] + pad[lane]);
      }
    }
    ++groups;
    std::cout
        << "PASS " << groups
        << " groups, 1024 independent four-lane bound cases; no engine calls\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}