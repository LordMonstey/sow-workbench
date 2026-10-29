#include "Monstey_GadbPrivate.hpp"

namespace Monstey
{
namespace Monstey_GadbPrivate
{

std::string Monstey_Suffix(std::string_view name, std::uint32_t target)
{
    if (Monstey_NativeHash(name) == target)
        return std::string(name);
    constexpr std::string_view chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    constexpr std::uint32_t base = 919;
    static const auto left = []()
    {
        constexpr std::string_view chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::unordered_map<std::uint32_t, std::string> m;
        m.reserve(50000);
        for (char a : chars)
            for (char b : chars)
                for (char c : chars)
                {
                    std::string s;
                    s.push_back(a);
                    s.push_back(b);
                    s.push_back(c);
                    m.emplace(Monstey_NativeHash(s), s);
                }
        return m;
    }();
    std::uint32_t power4 = 1, power3 = 1;
    for (int i = 0; i < 4; ++i)
        power4 *= base;
    for (int i = 0; i < 3; ++i)
        power3 *= base;
    std::uint32_t inverse = power4;
    for (int i = 0; i < 5; ++i)
        inverse *= 2 - power4 * inverse;
    const auto prefix = std::string(name) + "_";
    const std::uint32_t seed = Monstey_NativeHash(prefix) * power3;
    for (char a : chars)
        for (char b : chars)
            for (char c : chars)
            {
                const std::string triple{a, b, c};
                const auto h = Monstey_NativeHash(triple);
                for (char d : chars)
                {
                    const std::string last(1, d);
                    const auto tail = h * base + Monstey_NativeHash(last);
                    const auto wanted = (target - tail) * inverse - seed;
                    const auto found = left.find(wanted);
                    if (found != left.end())
                    {
                        auto out = prefix + found->second + triple + d;
                        Monstey_Require(Monstey_NativeHash(out) == target, "Native suffix solver mismatch");
                        return out;
                    }
                }
            }
    throw Monstey_Error("gadb_name_capacity", "No seven-character native hash suffix solution");
}

} // namespace Monstey_GadbPrivate

} // namespace Monstey
