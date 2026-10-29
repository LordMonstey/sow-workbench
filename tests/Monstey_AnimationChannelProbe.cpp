#include "Monstey_Model.hpp"
#include <cmath>
#include <iostream>
using namespace Monstey;
int main(int argc, char **argv)
{
    try
    {
        if (argc != 3)
            return 2;
        const auto s = Monstey_ModelReadScene(argv[1], argv[2]);
        auto report = Monstey_Json::array();
        for (const auto &p : s.parts)
            for (const auto &[name, c] : p.channels)
            {
                std::vector<std::size_t> nonfinite(c.width);
                std::size_t zero = 0;
                for (std::size_t i = 0; i < c.values.size(); ++i)
                    if (!std::isfinite(c.values[i]))
                        ++nonfinite[i % c.width];
                if (c.width >= 3)
                    for (std::size_t i = 0; i < c.values.size(); i += c.width)
                        if (c.values[i] == 0 && c.values[i + 1] == 0 && c.values[i + 2] == 0)
                            ++zero;
                auto counts = Monstey_Json::array();
                for (auto n : nonfinite)
                    counts.as_array().emplace_back(static_cast<std::uint64_t>(n));
                report.as_array().push_back(
                    Monstey_Json::object({{"part", static_cast<std::uint64_t>(p.part)},
                                          {"channel", name},
                                          {"width", c.width},
                                          {"nonfinite_per_lane", counts},
                                          {"zero_xyz", static_cast<std::uint64_t>(zero)}}));
            }
        std::cout << report.dump(2) << '\n';
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
