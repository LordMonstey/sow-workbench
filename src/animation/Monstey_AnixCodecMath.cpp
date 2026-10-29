#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
namespace Monstey_AnixPrivate
{

std::uint64_t Monstey_APackQuaternion(Q input)
{
    double norm = 0;
    for (float f : input)
    {
        Monstey_ARequire(std::isfinite(f), "Nonfinite quaternion");
        norm += static_cast<double>(f) * f;
    }
    Monstey_ARequire(norm > 1e-24, "Zero quaternion");
    const double inv = 1 / std::sqrt(norm);
    std::array<double, 4> q{};
    for (std::size_t i = 0; i < 4; ++i)
        q[i] = input[i] * inv;
    std::size_t omitted = 0;
    for (std::size_t i = 1; i < 4; ++i)
        if (std::abs(q[i]) > std::abs(q[omitted]))
            omitted = i;
    if (q[omitted] < 0)
        for (auto &f : q)
            f = -f;
    std::uint64_t word = static_cast<std::uint64_t>(3 - omitted) << 39;
    unsigned slot = 0;
    for (std::size_t i = 0; i < 4; ++i)
        if (i != omitted)
        {
            const auto u = static_cast<std::uint64_t>(
                std::clamp(std::nearbyint((q[i] + std::sqrt(0.5)) / std::sqrt(2.0) * 8190), 0., 8191.));
            word |= u << (13 * slot++);
        }
    return word;
}

Q Monstey_AUnpackQuaternion(std::uint64_t w)
{
    constexpr float inv = std::bit_cast<float>(0x39000801u), scale = std::bit_cast<float>(0x3fb504f3u),
                    bias = std::bit_cast<float>(0xbf3504f3u);
    std::array<float, 3> c{};
    for (unsigned i = 0; i < 3; ++i)
        c[i] = static_cast<float>((w >> (13 * i)) & 8191u) * inv * scale + bias;
    const float rad = 1.f - ((c[0] * c[0] + c[1] * c[1]) + c[2] * c[2]);
    Monstey_ARequire(rad >= 0 && std::isfinite(rad), "Packed quaternion would decode NaN (including padding)");
    const auto meta = static_cast<unsigned>((w >> 39) & 7u), omit = 3 - (meta & 3);
    float missing = std::sqrt(rad);
    if (meta & 4)
        missing = -missing;
    Q q{};
    unsigned ci = 0;
    for (unsigned i = 0; i < 4; ++i)
        q[i] = i == omit ? missing : c[ci++];
    return q;
}

Q Monstey_ANlerp(Q a, Q b, double f)
{
    float dot = 0;
    for (std::size_t i = 0; i < 4; ++i)
        dot += a[i] * b[i];
    if (dot < 0)
        for (auto &v : b)
            v = -v;
    const float t = Monstey_AFloat(f);
    Q q{};
    float n = 0;
    for (std::size_t i = 0; i < 4; ++i)
    {
        q[i] = (1 - t) * a[i] + t * b[i];
        n += q[i] * q[i];
    }
    Monstey_ARequire(n > 0 && std::isfinite(n), "Invalid nlerp quaternion");
    n = std::sqrt(n);
    for (auto &v : q)
        v /= n;
    return q;
}

Monstey_ABracket Monstey_ABracketAt(std::span<const std::uint32_t> t, double x)
{
    Monstey_ARequire(!t.empty() && std::isfinite(x), "Invalid animation sample time");
    x = std::clamp(x, static_cast<double>(t.front()), static_cast<double>(t.back()));
    if (t.size() == 1)
        return {0, 0, 0};
    auto it = std::lower_bound(t.begin(), t.end(), x);
    std::size_t hi = static_cast<std::size_t>(it - t.begin());
    hi = std::clamp<std::size_t>(hi, 1, t.size() - 1);
    const auto lo = hi - 1;
    return {lo, hi, t[hi] == t[lo] ? 0 : (x - t[lo]) / (t[hi] - t[lo])};
}

Monstey_AUncompressedBracket Monstey_AUncompressedBracketAt(std::span<const std::uint32_t> ticks, std::uint32_t sample)
{
    std::size_t lo = ticks.size() - 2;
    if (sample >= ticks.front() && sample <= ticks.back())
    {
        const auto found = std::lower_bound(ticks.begin(), ticks.end(), sample);
        const auto hi = std::max<std::size_t>(1, static_cast<std::size_t>(found - ticks.begin()));
        lo = hi - 1;
    }
    const auto hi = lo + 1;
    float fraction = 1.f;
    if (sample < ticks[hi])
    {
        // The native subtraction wraps before conversion to float. A query
        // before the first key uses the last pair, without a lower clamp.
        const float numerator = static_cast<float>(static_cast<std::uint32_t>(sample - ticks[lo]));
        const float denominator = static_cast<float>(ticks[hi] - ticks[lo]);
        fraction = numerator / denominator;
    }
    return {lo, hi, fraction};
}

float Monstey_ANativeAtanPositive(float y, float x)
{
    constexpr float half_pi = std::bit_cast<float>(0x3fc90fdbu);
    if (x == 0.f)
        return y == 0.f ? 0.f : half_pi;
    const float ratio = y / x;
    const bool small = std::abs(ratio) <= 1.f;
    const float value = small ? ratio : 1.f / ratio;
    const float square = value * value;
    float polynomial = std::bit_cast<float>(0x3b3bd74au);
    constexpr std::array<std::uint32_t, 8> coefficients{0xbc846e02u, 0x3d2fc1feu, 0xbd9a3174u, 0x3dda3d83u,
                                                        0xbe117fc7u, 0x3e4cbbe5u, 0xbeaaaa6cu, 0x3f800000u};
    for (const auto word : coefficients)
    {
        const float product = polynomial * square;
        polynomial = product + std::bit_cast<float>(word);
    }
    const float angle = polynomial * value;
    return small ? angle : half_pi - angle;
}

float Monstey_ANativeSin(float angle)
{
    const float scaled = angle * std::bit_cast<float>(0x3e22f983u);
    const float magic = std::signbit(scaled) ? -8388608.f : 8388608.f;
    const float rounded_sum = magic + scaled;
    const float rounded = rounded_sum - magic;
    const float multiples = rounded * std::bit_cast<float>(0x40c90fdbu);
    const float reduced = angle - multiples;
    const float pi = std::bit_cast<float>(0x40490fdbu);
    const float folded = (std::signbit(reduced) ? -pi : pi) - reduced;
    const float value = std::abs(reduced) <= std::bit_cast<float>(0x3fc90fdbu) ? reduced : folded;
    const float square = value * value;
    float polynomial = std::bit_cast<float>(0xb2cd365bu);
    constexpr std::array<std::uint32_t, 5> coefficients{0x3638b88eu, 0xb9500bf1u, 0x3c088886u, 0xbe2aaaabu,
                                                        0x3f800000u};
    for (const auto word : coefficients)
    {
        const float product = polynomial * square;
        polynomial = product + std::bit_cast<float>(word);
    }
    return polynomial * value;
}

Q Monstey_ANativeUaqt(Q a, Q b, float fraction)
{
    const float x = a[0] * b[0], y = a[1] * b[1], z = a[2] * b[2], w = a[3] * b[3];
    const float xz = x + z, yw = y + w;
    const float dot = xz + yw;
    const float sign = dot < 0.f ? -1.f : 1.f;
    const float cosine = dot * sign;
    const float complement = 1.f - fraction;
    float weight0 = complement, weight1 = fraction;
    if (cosine < std::bit_cast<float>(0x3f7fff58u))
    {
        const float square = cosine * cosine;
        const float radicand = 1.f - square;
        const float sine = std::sqrt(radicand);
        const float angle = Monstey_ANativeAtanPositive(sine, cosine);
        const float angle0 = angle * complement, angle1 = angle * fraction;
        weight0 = Monstey_ANativeSin(angle0) / sine;
        weight1 = Monstey_ANativeSin(angle1) / sine;
    }
    weight1 *= sign;
    Q output{};
    for (std::size_t component = 0; component < output.size(); ++component)
    {
        const float lower = a[component] * weight0;
        const float upper = b[component] * weight1;
        output[component] = upper + lower;
        Monstey_ARequire(std::isfinite(output[component]), "UAQT interpolation produced a nonfinite float32");
    }
    return output;
}

} // namespace Monstey_AnixPrivate

} // namespace Monstey
