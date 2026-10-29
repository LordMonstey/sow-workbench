#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

std::string Monstey_WLower(std::string s)
{
    for (auto &c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + 32);
    return s;
}

float Monstey_WFloat(const J &v)
{
    Monstey_Require(v.is_number(), "Finite number required");
    auto d = v.as_number();
    auto f = static_cast<float>(d);
    Monstey_Require(std::isfinite(d) && std::isfinite(f), "Number exceeds finite float32");
    return f;
}

float Monstey_WReadFloat(std::span<const std::uint8_t> b, std::size_t p)
{
    return std::bit_cast<float>(Monstey_U32(b, p));
}

void Monstey_WPutFloat(B &b, std::size_t p, float v)
{
    Monstey_Require(std::isfinite(v), "Nonfinite float32 output");
    Monstey_PutU32(b, p, std::bit_cast<std::uint32_t>(v));
}

J Monstey_WFloats(std::span<const std::uint8_t> b, std::size_t p, std::size_t n)
{
    J::Array a;
    for (std::size_t j = 0; j < n; ++j)
    {
        auto f = Monstey_WReadFloat(b, p + 4 * j);
        Monstey_Require(std::isfinite(f), "Nonfinite native transform or geometry");
        a.push_back(static_cast<double>(f));
    }
    return a;
}

std::vector<float> Monstey_WVector(const J &v, std::size_t n)
{
    Monstey_Require(v.is_array() && v.size() == n, "Vector dimensions differ");
    std::vector<float> result;
    for (auto &x : v.as_array())
        result.push_back(Monstey_WFloat(x));
    return result;
}

std::array<std::array<double, 3>, 3> Monstey_WRotation(const std::vector<float> &q)
{
    double x = q[0], y = q[1], z = q[2], w = q[3], norm = std::sqrt(x * x + y * y + z * z + w * w);
    Monstey_Require(norm > 0 && std::abs(norm - 1) <= 0.001, "Quaternion must be unit XYZW");
    x /= norm;
    y /= norm;
    z /= norm;
    w /= norm;
    return {{{1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)},
             {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)},
             {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)}}};
}

std::uint32_t Monstey_WUint(const J &v)
{
    if (v.is_string())
    {
        auto s = v.as_string();
        std::size_t used{};
        auto n = std::stoull(s, &used, 0);
        Monstey_Require(used == s.size() && n <= 0xffffffffull, "Expected uint32");
        return static_cast<std::uint32_t>(n);
    }
    auto n = v.as_uint();
    Monstey_Require(n <= 0xffffffffull, "Expected uint32");
    return static_cast<std::uint32_t>(n);
}

std::filesystem::path Monstey_WPath(const J &v)
{
    return std::filesystem::absolute(std::filesystem::path(Monstey_Wide(v.as_string())));
}

std::string Monstey_WTextPath(const std::filesystem::path &p)
{
    return Monstey_Utf8(p.wstring());
}

std::string Monstey_WName(std::span<const std::uint8_t> b, std::uint32_t p)
{
    Monstey_Require(p < b.size(), "Native name offset outside table");
    auto first = b.begin() + p, end = std::find(first, b.end(), 0);
    Monstey_Require(end != b.end(), "Unterminated native name");
    return Monstey_DisplayUtf8(std::string(first, end));
}

std::uint32_t Monstey_WAddString(B &pool, const std::string &text)
{
    Monstey_Require(text.find('\0') == std::string::npos && pool.size() + text.size() + 1 <= 0xffffffffull,
                    "Invalid or excessive native string");
    (void)Monstey_Wide(text);
    auto p = static_cast<std::uint32_t>(pool.size());
    pool.insert(pool.end(), text.begin(), text.end());
    pool.push_back(0);
    return p;
}

void Monstey_WExpected(const J &a, const B &raw)
{
    if (a.contains("expected_sha256"))
        Monstey_Require(Monstey_WLower(a.at("expected_sha256").as_string()) == Monstey_HashSha256(raw),
                        "Source SHA256 differs");
}

J Monstey_WPublish(const std::filesystem::path &source, const B &original, const J &a, const B &raw, J result)
{
    auto output = Monstey_WPath(a.at("output"));
    std::array<std::filesystem::path, 1> sources{source};
    Monstey_RequireOfflineOutput(output, sources);
    Monstey_Require(Monstey_HashSha256(source) == Monstey_HashSha256(original), "Source changed during world edit");
    bool made = false;
    try
    {
        Monstey_WriteNewFile(output, raw);
        made = true;
        Monstey_Require(Monstey_ReadFile(output) == raw, "World output readback differs");
        Monstey_Require(Monstey_HashSha256(source) == Monstey_HashSha256(original),
                        "Source changed during world publication");
    }
    catch (...)
    {
        if (made)
        {
            std::error_code ec;
            std::filesystem::remove(output, ec);
        }
        throw;
    }
    result["output"] = Monstey_WTextPath(output);
    result["bytes"] = static_cast<std::uint64_t>(raw.size());
    result["sha256"] = Monstey_HashSha256(raw);
    result["source_sha256"] = Monstey_HashSha256(original);
    result["runtime_verified"] = false;
    result["game_written"] = false;
    if (a.contains("report"))
        Monstey_WriteNewText(Monstey_WPath(a.at("report")), result.dump(2) + "\n");
    return result;
}

float Monstey_WHalf(std::uint16_t h)
{
    auto sign = std::uint32_t(h & 0x8000) << 16;
    auto exponent = (h >> 10) & 31u, mantissa = h & 1023u;
    std::uint32_t bits;
    if (exponent == 0)
    {
        if (!mantissa)
            bits = sign;
        else
        {
            int e = -14;
            while (!(mantissa & 1024u))
            {
                mantissa <<= 1;
                --e;
            }
            bits = sign | (static_cast<std::uint32_t>(e + 127) << 23) | ((mantissa & 1023u) << 13);
        }
    }
    else if (exponent == 31)
        bits = sign | 0x7f800000u | (mantissa << 13);
    else
        bits = sign | ((exponent + 112u) << 23) | (mantissa << 13);
    return std::bit_cast<float>(bits);
}

std::uint16_t Monstey_WPackHalf(float value)
{
    auto b = std::bit_cast<std::uint32_t>(value), sign = (b >> 16) & 0x8000u;
    int exponent = static_cast<int>((b >> 23) & 255) - 127 + 15;
    auto mantissa = b & 0x7fffffu;
    Monstey_Require(std::isfinite(value), "Nonfinite half output");
    if (exponent <= 0)
    {
        if (exponent < -10)
            return static_cast<std::uint16_t>(sign);
        mantissa |= 0x800000u;
        auto shift = static_cast<unsigned>(14 - exponent), result = mantissa >> shift;
        auto round = mantissa & ((1u << shift) - 1), mid = 1u << (shift - 1);
        if (round > mid || (round == mid && (result & 1)))
            ++result;
        return static_cast<std::uint16_t>(sign | result);
    }
    Monstey_Require(exponent < 31, "Half attribute overflow");
    auto result = mantissa >> 13, round = mantissa & 8191u;
    if (round > 4096u || (round == 4096u && (result & 1u)))
        ++result;
    if (result == 1024)
    {
        result = 0;
        ++exponent;
    }
    Monstey_Require(exponent < 31, "Half attribute overflow");
    return static_cast<std::uint16_t>(sign | (static_cast<unsigned>(exponent) << 10) | result);
}

std::uint32_t Monstey_WCrc(std::span<const std::uint8_t> bytes)
{
    return Monstey_Crc32(bytes);
}

} // namespace Monstey_WorldPrivate

} // namespace Monstey
