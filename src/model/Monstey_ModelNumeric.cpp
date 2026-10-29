#include "Monstey_ModelPrivate.hpp"

namespace Monstey
{
namespace Monstey_ModelPrivate
{

void Monstey_MRequire(bool condition, std::string_view message)
{
    if (!condition)
        throw Monstey_Error("model_invalid", std::string(message));
}

std::uint32_t Monstey_Size(std::size_t n)
{
    Monstey_MRequire(n <= 0xffffffff, "Model UINT32 extent overflow");
    return static_cast<std::uint32_t>(n);
}

double Monstey_F32(Monstey_Span b, std::size_t p)
{
    const auto f = std::bit_cast<float>(Monstey_U32(b, p));
    Monstey_MRequire(std::isfinite(f), "Nonfinite model float at byte " + std::to_string(p));
    return f;
}

void Monstey_AddF32(Monstey_Bytes &b, double v)
{
    const float f = static_cast<float>(v);
    Monstey_MRequire(std::isfinite(f), "Model float exceeds FLOAT32");
    Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(f));
}

float Monstey_Half(std::uint16_t h)
{
    const auto sign = static_cast<std::uint32_t>(h & 0x8000) << 16;
    const auto e = (h >> 10) & 31u, m = h & 1023u;
    std::uint32_t bits;
    if (e == 0)
    {
        if (m == 0)
            bits = sign;
        else
        {
            std::uint32_t x = m;
            int exp = -14;
            while ((x & 1024) == 0)
            {
                x <<= 1;
                --exp;
            }
            bits = sign | (static_cast<std::uint32_t>(exp + 127) << 23) | ((x & 1023) << 13);
        }
    }
    else
        bits = sign | ((e == 31 ? 255 : e + 112) << 23) | (m << 13);
    return std::bit_cast<float>(bits);
}

std::uint16_t Monstey_ToHalf(double value)
{
    const auto f = static_cast<float>(value);
    Monstey_MRequire(std::isfinite(f) && std::abs(f) <= 65504, "Position exceeds native FLOAT16 range");
    const auto bits = std::bit_cast<std::uint32_t>(f);
    const auto sign = static_cast<std::uint16_t>((bits >> 16) & 0x8000);
    int e = static_cast<int>((bits >> 23) & 255) - 127 + 15;
    std::uint32_t m = bits & 0x7fffff;
    if (e <= 0)
    {
        if (e < -10)
            return sign;
        m |= 0x800000;
        const unsigned shift = static_cast<unsigned>(14 - e);
        const auto q = m >> shift, rem = m & ((1u << shift) - 1), half = 1u << (shift - 1);
        return static_cast<std::uint16_t>(sign + q + (rem > half || (rem == half && (q & 1))));
    }
    auto q = m >> 13;
    const auto rem = m & 8191;
    if (rem > 4096 || (rem == 4096 && (q & 1)))
    {
        if (++q == 1024)
        {
            q = 0;
            ++e;
        }
    }
    Monstey_MRequire(e < 31, "Native half rounding overflow");
    return static_cast<std::uint16_t>(sign | (static_cast<unsigned>(e) << 10) | q);
}

Monstey_M4 Monstey_Identity()
{
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

Monstey_M4 Monstey_Mul(const Monstey_M4 &a, const Monstey_M4 &b)
{
    Monstey_M4 c{};
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned col = 0; col < 4; ++col)
            for (unsigned k = 0; k < 4; ++k)
                c[r * 4 + col] += a[r * 4 + k] * b[k * 4 + col];
    return c;
}

Monstey_M4 Monstey_Inv(Monstey_M4 a)
{
    auto out = Monstey_Identity();
    for (unsigned c = 0; c < 4; ++c)
    {
        unsigned pivot = c;
        for (unsigned r = c + 1; r < 4; ++r)
            if (std::abs(a[r * 4 + c]) > std::abs(a[pivot * 4 + c]))
                pivot = r;
        Monstey_MRequire(std::abs(a[pivot * 4 + c]) > 1e-12, "Singular model transform");
        for (unsigned k = 0; k < 4; ++k)
        {
            std::swap(a[c * 4 + k], a[pivot * 4 + k]);
            std::swap(out[c * 4 + k], out[pivot * 4 + k]);
        }
        const auto divisor = a[c * 4 + c];
        for (unsigned k = 0; k < 4; ++k)
        {
            a[c * 4 + k] /= divisor;
            out[c * 4 + k] /= divisor;
        }
        for (unsigned r = 0; r < 4; ++r)
            if (r != c)
            {
                const auto f = a[r * 4 + c];
                for (unsigned k = 0; k < 4; ++k)
                {
                    a[r * 4 + k] -= f * a[c * 4 + k];
                    out[r * 4 + k] -= f * out[c * 4 + k];
                }
            }
    }
    return out;
}

Monstey_V3 Monstey_Cross(const Monstey_V3 &a, const Monstey_V3 &b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

double Monstey_Dot(const Monstey_V3 &a, const Monstey_V3 &b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

Monstey_V3 Monstey_Unit(Monstey_V3 a, const Monstey_V3 &fallback)
{
    const auto len = std::sqrt(Monstey_Dot(a, a));
    if (len <= 1e-12)
        return fallback;
    for (auto &x : a)
        x /= len;
    return a;
}

Monstey_V3 Monstey_Vector(const Monstey_M4 &m, const Monstey_V3 &p, bool point)
{
    Monstey_V3 r{};
    for (unsigned i = 0; i < 3; ++i)
    {
        r[i] = point ? m[i * 4 + 3] : 0;
        for (unsigned j = 0; j < 3; ++j)
            r[i] += m[i * 4 + j] * p[j];
    }
    return r;
}

Monstey_M4 Monstey_Transform(const Monstey_V3 &t, const std::array<double, 4> &q, const Monstey_V3 &s)
{
    const double n = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
    Monstey_MRequire(std::isfinite(n) && std::abs(n - 1) < 1e-3, "Invalid quaternion XYZW");
    const double inverseNorm = 1 / std::sqrt(n), x = q[0] * inverseNorm, y = q[1] * inverseNorm, z = q[2] * inverseNorm,
                 w = q[3] * inverseNorm;
    Monstey_M4 m = {1 - 2 * (y * y + z * z),
                    2 * (x * y - z * w),
                    2 * (x * z + y * w),
                    t[0],
                    2 * (x * y + z * w),
                    1 - 2 * (x * x + z * z),
                    2 * (y * z - x * w),
                    t[1],
                    2 * (x * z - y * w),
                    2 * (y * z + x * w),
                    1 - 2 * (x * x + y * y),
                    t[2],
                    0,
                    0,
                    0,
                    1};
    for (unsigned r = 0; r < 3; ++r)
        for (unsigned c = 0; c < 3; ++c)
            m[r * 4 + c] *= s[c];
    return m;
}

Monstey_J Monstey_MatrixJson(const Monstey_M4 &m)
{
    auto out = Monstey_J::array();
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned r = 0; r < 4; ++r)
            out.as_array().push_back(m[r * 4 + c]);
    return out;
}

std::size_t Monstey_Part::Count() const
{
    return channels.at("POSITION").v.size() / 3;
}

} // namespace Monstey_ModelPrivate

} // namespace Monstey
