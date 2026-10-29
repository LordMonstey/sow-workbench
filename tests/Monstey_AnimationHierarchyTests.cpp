#include "Monstey_AnimationHierarchy.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <immintrin.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace Monstey;
namespace
{
using Monstey_Words = Monstey_HierarchyPoseWords;
#include "Monstey_Legacy017.inc"
void Monstey_Check(bool b, const char *why)
{
    if (!b)
        throw std::runtime_error(why);
}
__m128 Monstey_Load(const Monstey_Words &w, unsigned o)
{
    const std::array<float, 4> f{std::bit_cast<float>(w[o]), std::bit_cast<float>(w[o + 1]),
                                 std::bit_cast<float>(w[o + 2]), std::bit_cast<float>(w[o + 3])};
    return _mm_loadu_ps(f.data());
}
// Literal independent SSE transcription of 34F660 register instructions.
__m128 Monstey_SseQ(__m128 xmm0, __m128 xmm1)
{
    __m128 xmm2 = _mm_shuffle_ps(xmm0, xmm0, 0x1b), xmm5 = xmm0, xmm4 = xmm1;
    xmm0 = _mm_shuffle_ps(xmm1, xmm1, 0xaa);
    __m128 xmm6 = _mm_shuffle_ps(xmm1, xmm1, 0xff);
    xmm4 = _mm_shuffle_ps(xmm1, xmm1, 0);
    xmm4 = _mm_mul_ps(xmm4, xmm2);
    xmm2 = _mm_shuffle_ps(xmm2, xmm2, 0xb1);
    xmm6 = _mm_mul_ps(xmm6, xmm5);
    __m128 xmm3 = _mm_shuffle_ps(xmm2, xmm2, 0x1b);
    xmm3 = _mm_mul_ps(xmm3, xmm0);
    xmm0 = _mm_shuffle_ps(xmm1, xmm1, 0x55);
    xmm1 = _mm_setr_ps(1, -1, 1, -1);
    xmm0 = _mm_mul_ps(xmm0, xmm2);
    xmm3 = _mm_mul_ps(xmm3, _mm_setr_ps(-1, 1, 1, -1));
    xmm1 = _mm_mul_ps(xmm1, xmm4);
    xmm0 = _mm_mul_ps(xmm0, _mm_setr_ps(1, 1, -1, -1));
    xmm6 = _mm_add_ps(xmm6, xmm1);
    xmm0 = _mm_add_ps(xmm0, xmm3);
    return _mm_add_ps(xmm0, xmm6);
}
// Literal independent SSE transcription of 34F540, including all four lanes.
__m128 Monstey_SseRotate(__m128 xmm0, __m128 xmm1)
{
    const auto mask = _mm_castsi128_ps(_mm_setr_epi32(-1, -1, -1, 0));
    __m128 xmm2 = mask, xmm3 = _mm_mul_ps(_mm_setr_ps(-1, -1, -1, 1), xmm1);
    __m128 xmm7 = _mm_andnot_ps(xmm2, xmm2);
    xmm2 = _mm_and_ps(xmm2, xmm0);
    xmm7 = _mm_or_ps(xmm7, xmm2);
    __m128 xmm9 = xmm1;
    xmm2 = _mm_shuffle_ps(xmm7, xmm7, 0);
    xmm1 = _mm_shuffle_ps(xmm3, xmm3, 0x1b);
    xmm0 = _mm_shuffle_ps(xmm7, xmm7, 0xaa);
    xmm2 = _mm_mul_ps(xmm2, xmm1);
    xmm1 = _mm_shuffle_ps(xmm1, xmm1, 0xb1);
    __m128 xmm8 = _mm_shuffle_ps(xmm1, xmm1, 0x1b);
    xmm8 = _mm_mul_ps(xmm8, xmm0);
    xmm0 = _mm_shuffle_ps(xmm7, xmm7, 0x55);
    xmm0 = _mm_mul_ps(xmm0, xmm1);
    xmm8 = _mm_mul_ps(xmm8, _mm_setr_ps(-1, 1, 1, -1));
    xmm7 = _mm_shuffle_ps(xmm7, xmm7, 0xff);
    xmm0 = _mm_mul_ps(xmm0, _mm_setr_ps(1, 1, -1, -1));
    xmm7 = _mm_mul_ps(xmm7, xmm3);
    xmm3 = _mm_shuffle_ps(xmm9, xmm9, 0);
    xmm8 = _mm_add_ps(xmm8, xmm0);
    xmm0 = _mm_mul_ps(_mm_setr_ps(1, -1, 1, -1), xmm2);
    xmm7 = _mm_add_ps(xmm7, xmm0);
    xmm0 = _mm_shuffle_ps(xmm9, xmm9, 0xaa);
    xmm8 = _mm_add_ps(xmm8, xmm7);
    xmm1 = _mm_shuffle_ps(xmm8, xmm8, 0x1b);
    xmm3 = _mm_mul_ps(xmm3, xmm1);
    xmm1 = _mm_shuffle_ps(xmm1, xmm1, 0xb1);
    xmm2 = _mm_shuffle_ps(xmm1, xmm1, 0x1b);
    xmm3 = _mm_mul_ps(xmm3, _mm_setr_ps(1, -1, 1, -1));
    xmm2 = _mm_mul_ps(xmm2, xmm0);
    xmm0 = _mm_shuffle_ps(xmm9, xmm9, 0x55);
    xmm0 = _mm_mul_ps(xmm0, xmm1);
    xmm2 = _mm_mul_ps(xmm2, _mm_setr_ps(-1, 1, 1, -1));
    xmm9 = _mm_shuffle_ps(xmm9, xmm9, 0xff);
    xmm0 = _mm_mul_ps(xmm0, _mm_setr_ps(1, 1, -1, -1));
    xmm9 = _mm_mul_ps(xmm9, xmm8);
    xmm0 = _mm_add_ps(xmm0, xmm2);
    xmm9 = _mm_add_ps(xmm9, xmm3);
    return _mm_add_ps(xmm0, xmm9);
}
Monstey_Words Monstey_SseCompose(const Monstey_Words &p, const Monstey_Words &c)
{
    std::array<float, 8> f{};
    _mm_storeu_ps(f.data() + 4, Monstey_SseQ(Monstey_Load(c, 4), Monstey_Load(p, 4)));
    _mm_storeu_ps(f.data(),
                  _mm_add_ps(Monstey_SseRotate(Monstey_Load(c, 0), Monstey_Load(p, 4)), Monstey_Load(p, 0)));
    Monstey_Words w{};
    for (unsigned i = 0; i < 8; ++i)
        w[i] = std::bit_cast<std::uint32_t>(f[i]);
    return w;
}
Monstey_Words Monstey_Make(std::array<float, 3> p, std::array<float, 4> q, float w = 0)
{
    Monstey_Words r{};
    for (unsigned i = 0; i < 3; ++i)
        r[i] = std::bit_cast<std::uint32_t>(p[i]);
    r[3] = std::bit_cast<std::uint32_t>(w);
    for (unsigned i = 0; i < 4; ++i)
        r[i + 4] = std::bit_cast<std::uint32_t>(q[i]);
    return r;
}
Monstey_Words Monstey_Legacy(const Monstey_Words &p, const Monstey_Words &c)
{
    Monstey_Pose pp{}, cc{};
    for (unsigned i = 0; i < 3; ++i)
    {
        pp.position_cm[i] = std::bit_cast<float>(p[i]);
        cc.position_cm[i] = std::bit_cast<float>(c[i]);
    }
    for (unsigned i = 0; i < 4; ++i)
    {
        pp.quaternion_xyzw[i] = std::bit_cast<float>(p[i + 4]);
        cc.quaternion_xyzw[i] = std::bit_cast<float>(c[i + 4]);
    }
    auto r = Monstey_APCompose(pp, cc);
    return Monstey_Make(r.position_cm, r.quaternion_xyzw);
}
std::uint32_t Monstey_Random(std::uint32_t &s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}
} // namespace
int main()
{
    try
    {
        Monstey_Check((_mm_getcsr() & 0xe040u) == 0, "Tests require round-nearest, DAZ off and FTZ off");
        unsigned groups = 0;
        std::uint64_t random_cases = 0, legacy_differences = 0;
        const Monstey_Words identity{0, 0, 0, 0, 0, 0, 0, 0x3f800000};
        Monstey_Words p{0x40500000, 0xc0d00000, 0x3e000000, 0,
                        0x3d180520, 0x3f048c8e, 0x3f470592, 0xbeb5d817};
        Monstey_Words c{0x41480000, 0x80000000, 0x40980000, 0x42c80000,
                        0x3efcc539, 0x3ec1fc0d, 0x3e994b72, 0xbf39265b};
        const Monstey_Words expected{0xc0f11de4, 0xc10f9240, 0x40f447e7, 0xb5000000,
                                     0xbeaef4cc, 0xbe0b9c0b, 0xbf690077, 0xbe42f55f};
        Monstey_Check(Monstey_HierarchyComposeF32(p, c) == expected, "Pinned exact eight-word fixture");
        ++groups;
        Monstey_Check(Monstey_SseCompose(p, c) == expected, "Independent literal SSE fixture");
        ++groups;
        auto legacy = Monstey_Legacy(p, c);
        Monstey_Check(legacy[1] == 0xc10f923f && legacy[2] == 0x40f447e8 && legacy[5] == 0xbe0b9c0a &&
                          legacy[6] == 0xbf690078 && legacy[7] == 0xbe42f55e,
                      "Pinned old 017 divergence");
        ++groups;
        Monstey_Check(Monstey_HierarchyComposeF32(c, p) != expected, "Noncommuting negative order control");
        ++groups;
        auto changed_w = c;
        changed_w[3] = 0x7fc12345;
        Monstey_Check(Monstey_HierarchyComposeF32(p, changed_w) == expected,
                      "Local W must be masked before rotation");
        ++groups;
        auto parent_w = p;
        parent_w[3] = 0x3f800000;
        auto result_w = Monstey_HierarchyComposeF32(parent_w, c);
        Monstey_Check(result_w[3] == 0x3f7ffff8, "Parent W plus nonzero rotated rounding W");
        ++groups;
        std::uint32_t seed = 0xa56c3917;
        for (unsigned n = 0; n < 20000; ++n)
        {
            Monstey_Words a{}, b{};
            for (unsigned i = 0; i < 8; ++i)
            {
                const auto ra = static_cast<std::int32_t>(Monstey_Random(seed) % 2000001) - 1000000;
                const auto rb = static_cast<std::int32_t>(Monstey_Random(seed) % 2000001) - 1000000;
                a[i] = std::bit_cast<std::uint32_t>(float(ra) / (i < 4 ? 8192.0f : 1048576.0f));
                b[i] = std::bit_cast<std::uint32_t>(float(rb) / (i < 4 ? 8192.0f : 1048576.0f));
            }
            auto got = Monstey_HierarchyComposeF32(a, b);
            Monstey_Check(got == Monstey_SseCompose(a, b), "Random exact native SSE comparison");
            auto old = Monstey_Legacy(a, b);
            bool differs = false;
            for (unsigned i = 0; i < 8; ++i)
                if (i != 3 && got[i] != old[i])
                    differs = true;
            legacy_differences += differs;
            ++random_cases;
        }
        ++groups;
        Monstey_Check(legacy_differences > 0, "Random old 017 negative control");
        ++groups;
        for (std::uint32_t x : {0u, 0x80000000u, 1u, 0x80000001u, 0x007fffffu, 0x807fffffu, 0x00800000u})
        {
            auto q = identity;
            q[0] = x;
            q[3] = x;
            Monstey_Check(Monstey_HierarchyComposeF32(p, q) == Monstey_SseCompose(p, q),
                          "Signed zero and subnormal exact lanes");
        }
        ++groups;
        auto nonunit = Monstey_Make({2, 3, 4}, {0.2f, -0.3f, 0.5f, 1.7f}, 9);
        Monstey_Check(Monstey_HierarchyComposeF32(nonunit, c) == Monstey_SseCompose(nonunit, c),
                      "Nonunit Q is not normalized");
        ++groups;
        std::vector<std::uint8_t> parents{255, 0, 0, 1, 3};
        std::vector<Monstey_Words> local{c, p, c, nonunit, p}, output(5, identity);
        for (std::uint32_t mode : {0u, 1u, 2u, 3u, 0xffffffffu})
        {
            Monstey_HierarchyEvaluateF32(parents, local, nonunit, mode, output);
            Monstey_Check(output[0] == (mode == 2 ? c : nonunit), "Native mode==2 root selection");
            for (std::size_t i = 1; i < local.size(); ++i)
                Monstey_Check(output[i] == Monstey_SseCompose(output[parents[i]], local[i]),
                              "Nontrivial ancestor hierarchy");
        }
        ++groups;
        auto original_local = local;
        Monstey_HierarchyEvaluateF32(parents, local, nonunit, 2, local);
        for (std::size_t i = 1; i < local.size(); ++i)
            Monstey_Check(local[i] == Monstey_SseCompose(local[parents[i]], original_local[i]),
                          "Aliased input/output transaction");
        ++groups;
        local = original_local;
        auto immutable_local = local;
        auto immutable_parents = parents;
        auto invalid = [&](std::vector<std::uint8_t> pp, std::span<const Monstey_Words> ll,
                           std::span<Monstey_Words> oo) {
            auto before = output;
            bool caught = false;
            try
            {
                Monstey_HierarchyEvaluateF32(pp, ll, identity, 2, oo);
            }
            catch (const std::invalid_argument &)
            {
                caught = true;
            }
            Monstey_Check(caught && output == before, "Malformed layout transactional rollback");
        };
        auto bad = parents;
        bad[0] = 0;
        invalid(bad, local, output);
        bad = parents;
        bad[3] = 255;
        invalid(bad, local, output);
        bad = parents;
        bad[1] = 4;
        invalid(bad, local, output);
        invalid(parents, std::span<const Monstey_Words>(local).first(3), output);
        invalid(parents, local, std::span<Monstey_Words>(output).first(3));
        ++groups;
        std::vector<std::uint8_t> large_parents(257, 0);
        large_parents[0] = 255;
        std::vector<Monstey_Words> large(257, identity);
        bool rejected = false;
        try
        {
            Monstey_HierarchyEvaluateF32(large_parents, large, identity, 2, large);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Monstey_Check(rejected, "257 record native ordinal rejection");
        ++groups;
        std::vector<std::uint8_t> maximum(256, 0);
        maximum[0] = 255;
        std::vector<Monstey_Words> max_local(256, identity), max_out(256);
        Monstey_HierarchyEvaluateF32(maximum, max_local, identity, 2, max_out);
        Monstey_Check(max_out == max_local, "256 record tree");
        ++groups;
        Monstey_HierarchyEvaluateF32({}, {}, identity, 2, {});
        ++groups;
        auto before_descendants = output;
        Monstey_HierarchyResetOrdinalZero(output);
        Monstey_Check(output[0] == identity, "Explicit post-hierarchy root reset");
        for (std::size_t i = 1; i < output.size(); ++i)
            Monstey_Check(output[i] == before_descendants[i], "Reset must not recompute descendants");
        Monstey_HierarchyResetOrdinalZero({});
        ++groups;
        std::vector<Monstey_Words> projected(output.size());
        Monstey_HierarchyEvaluateF32(parents, original_local, nonunit, 2, output);
        projected[0] = original_local[0];
        projected[0][3] = 0;
        for (std::size_t i = 1; i < projected.size(); ++i)
        {
            auto projected_parent = projected[parents[i]];
            projected_parent[3] = 0;
            projected[i] = Monstey_HierarchyComposeF32(projected_parent, original_local[i]);
            projected[i][3] = 0;
            for (unsigned k = 0; k < 8; ++k)
                if (k != 3)
                    Monstey_Check(projected[i][k] == output[i][k],
                                  "Omitted parent W cannot affect XYZ/quaternion projection");
        }
        ++groups;
        Monstey_Check(local == immutable_local && parents == immutable_parents, "Immutable input spans");
        ++groups;
        std::cout << "{\"passed_groups\":" << groups
                  << ",\"failed\":0,\"random_exact_sse_cases\":" << random_cases
                  << ",\"legacy017_differing_random_cases\":" << legacy_differences
                  << ",\"original_code_executed\":false,\"mxcsr\":" << _mm_getcsr() << "}\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
