#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
namespace Monstey_AnixPrivate
{

void Monstey_ARequire(bool v, std::string_view m)
{
    if (!v)
        throw Monstey_Error("anix", std::string(m));
}

std::size_t Monstey_AAlign(std::size_t v, std::size_t n)
{
    Monstey_ARequire(v <= Monstey_Limit - n, "ANIX alignment exceeds limit");
    return (v + n - 1) / n * n;
}

void Monstey_ARange(std::span<const std::uint8_t> b, std::size_t p, std::size_t n)
{
    Monstey_ARequire(p <= b.size() && n <= b.size() - p, "ANIX range exceeds its owning section");
}

std::uint32_t Monstey_AU32(std::span<const std::uint8_t> b, std::size_t p)
{
    Monstey_ARange(b, p, 4);
    return Monstey_U32(b, p);
}

std::uint16_t Monstey_AU16(std::span<const std::uint8_t> b, std::size_t p)
{
    Monstey_ARange(b, p, 2);
    return Monstey_U16(b, p);
}

float Monstey_AF32(std::span<const std::uint8_t> b, std::size_t p)
{
    return std::bit_cast<float>(Monstey_AU32(b, p));
}

B Monstey_ASlice(std::span<const std::uint8_t> b, std::size_t p, std::size_t n)
{
    Monstey_ARange(b, p, n);
    return B(b.begin() + p, b.begin() + p + n);
}

void Monstey_AAppend(B &b, std::span<const std::uint8_t> v)
{
    Monstey_ARequire(v.size() <= Monstey_Limit - b.size(), "ANIX output exceeds limit");
    b.insert(b.end(), v.begin(), v.end());
}

void Monstey_APad(B &b, std::size_t n)
{
    b.resize(Monstey_AAlign(b.size(), n), 0);
}

void Monstey_AFAdd(B &b, float f)
{
    Monstey_ARequire(std::isfinite(f), "Nonfinite ANIX float");
    Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(f));
}

std::uint32_t Monstey_A32(std::uint64_t v)
{
    Monstey_ARequire(v <= UINT32_MAX, "ANIX u32 overflow");
    return static_cast<std::uint32_t>(v);
}

std::uint16_t Monstey_A16(std::size_t v)
{
    Monstey_ARequire(v <= UINT16_MAX, "ANIX u16 offset/count overflow");
    return static_cast<std::uint16_t>(v);
}

float Monstey_AFloat(double v)
{
    const float f = static_cast<float>(v);
    Monstey_ARequire(std::isfinite(v) && std::isfinite(f), "ANIX value is not finite float32");
    return f;
}

std::string Monstey_AString(std::span<const std::uint8_t> b, std::size_t p, std::size_t end)
{
    Monstey_ARequire(p < end && end <= b.size(), "Invalid ANIX string extent");
    std::string s;
    for (; p < end && b[p]; ++p)
    {
        Monstey_ARequire(b[p] < 128, "ANIX string is not ASCII");
        s.push_back(static_cast<char>(b[p]));
    }
    Monstey_ARequire(p < end, "Unterminated ANIX string");
    return s;
}

void Monstey_AName(std::string_view n, bool clip)
{
    Monstey_ARequire(!n.empty() && (!clip || n.size() < 48) &&
                         std::all_of(n.begin(), n.end(), [](unsigned char c) { return c > 0 && c < 128; }),
                     "Invalid ASCII ANIX name");
}

std::uint32_t Monstey_AHash(std::string_view s)
{
    Monstey_AName(s);
    return Monstey_NativeHash(s);
}

void Monstey_AZero(std::span<const std::uint8_t> b, std::size_t p)
{
    Monstey_ARange(b, p, 0);
    Monstey_ARequire(std::all_of(b.begin() + p, b.end(), [](auto v) { return v == 0; }),
                     "Nonzero codec trailing alignment");
}

J Monstey_AArray(std::span<const float> v)
{
    J a = J::array();
    for (float f : v)
        a.as_array().emplace_back(static_cast<double>(f));
    return a;
}

std::uint32_t Monstey_AJsonU(const J &j)
{
    return Monstey_A32(j.as_uint());
}

std::uint32_t Monstey_AHashJson(const J &j)
{
    if (j.is_integer())
        return Monstey_AJsonU(j);
    auto s = j.as_string();
    if (s.starts_with("0x"))
        s.erase(0, 2);
    Monstey_ARequire(!s.empty() && s.size() <= 8, "Invalid ANIX hash");
    std::uint64_t out = 0;
    for (char c : s)
    {
        unsigned v = c >= '0' && c <= '9'   ? static_cast<unsigned>(c - '0')
                     : c >= 'a' && c <= 'f' ? static_cast<unsigned>(c - 'a' + 10)
                     : c >= 'A' && c <= 'F' ? static_cast<unsigned>(c - 'A' + 10)
                                            : 99;
        Monstey_ARequire(v < 16, "Invalid ANIX hex hash");
        out = out * 16 + v;
    }
    return Monstey_A32(out);
}

} // namespace Monstey_AnixPrivate

} // namespace Monstey
