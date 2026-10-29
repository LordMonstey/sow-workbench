#include "Monstey_GadbPrivate.hpp"

namespace Monstey
{
namespace Monstey_GadbPrivate
{

std::string Monstey_Hex(std::uint32_t value)
{
    std::ostringstream s;
    s << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return s.str();
}

std::string Monstey_Lower(std::string_view text)
{
    std::string out(text);
    for (auto &c : out)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + 32);
    return out;
}

bool Monstey_Has(std::string_view value, std::string_view part)
{
    return Monstey_Lower(value).find(Monstey_Lower(part)) != std::string::npos;
}

std::uint32_t Monstey_ParseRef(std::string_view text)
{
    std::uint32_t out = 0;
    if (text.starts_with("0x") || text.starts_with("0X"))
        text.remove_prefix(2);
    auto [p, e] = std::from_chars(text.data(), text.data() + text.size(), out, 16);
    Monstey_Require(e == std::errc() && p == text.data() + text.size(), "Invalid hexadecimal reference");
    return out;
}

bool Monstey_KeyEqual(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (Monstey_NativeHash(a.substr(i, 1)) != Monstey_NativeHash(b.substr(i, 1)))
            return false;
    return true;
}

std::string Monstey_Type(std::uint8_t type)
{
    switch (type)
    {
    case 1:
        return "bool";
    case 2:
        return "float";
    case 3:
        return "int";
    case 4:
        return "string";
    case 5:
        return "enum";
    case 6:
        return "text";
    case 7:
        return "vec2";
    case 8:
        return "vec3";
    case 9:
        return "vec4";
    case 10:
        return "ref";
    case 11:
        return "list";
    case 12:
        return "color-curve";
    case 13:
        return "float-curve";
    case 15:
        return "int-curve";
    case 16:
        return "vec3-curve";
    case 68:
        return "string-like";
    default:
        return "unknown";
    }
}

J Monstey_FloatJson(std::uint32_t word)
{
    const auto v = std::bit_cast<float>(word);
    if (std::isfinite(v))
        return J(static_cast<double>(v));
    return J(std::isnan(v) ? "nan" : v < 0 ? "-inf" : "inf");
}

void Monstey_Append(B &to, const B &from, std::size_t begin, std::size_t end)
{
    Monstey_Require(begin <= end && end <= from.size(), "Copy range outside source");
    to.insert(to.end(), from.begin() + static_cast<std::ptrdiff_t>(begin),
              from.begin() + static_cast<std::ptrdiff_t>(end));
}

void Monstey_Aligned(B &bytes)
{
    while (bytes.size() % 4)
        bytes.push_back(0);
}

void Monstey_OutsideGame(const std::filesystem::path &output)
{
    auto p = std::filesystem::absolute(output).parent_path();
    for (; !p.empty();)
    {
        Monstey_Require(!std::filesystem::is_regular_file(p / "ShadowOfWar.exe") &&
                            !std::filesystem::is_regular_file(p / "x64" / "ShadowOfWar.exe"),
                        "Choose an output outside the game installation");
        auto next = p.parent_path();
        if (next == p)
            break;
        p = next;
    }
}

} // namespace Monstey_GadbPrivate

} // namespace Monstey
