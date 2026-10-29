#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

std::wstring Monstey_TypedLines(const J &value, unsigned type)
{
    std::wstring text;
    for (const auto &item : value.as_array())
    {
        if (!text.empty())
            text += L"\r\n";
        if (item.is_string())
            text += item.as_string().empty() ? L"\"\"" : Monstey_Display(item);
        else if (item.is_array())
            for (std::size_t i = 0; i < item.size(); ++i)
            {
                if (i)
                    text += L", ";
                text += Monstey_Display(item[i]);
            }
        else if (type == 1)
            text += item.as_bool() ? L"True" : L"False";
        else
            text += Monstey_Display(item);
    }
    return text;
}

J Monstey_ParseTypedLines(const std::wstring &input, unsigned type)
{
    J values = J::array();
    std::istringstream stream(Monstey_Utf8(input));
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (type == 4 || type == 5 || type == 6 || type == 68)
        {
            if (!line.empty())
                values.as_array().push_back(line == "\"\"" ? std::string{} : line);
            continue;
        }
        auto first = line.find_first_not_of(" \t");
        if (first == line.npos)
            continue;
        auto last = line.find_last_not_of(" \t");
        line = line.substr(first, last - first + 1);
        if (type == 10 || type == 11)
            values.as_array().push_back(line == "null" ? J{} : J(line));
        else if (type == 1)
        {
            std::transform(line.begin(), line.end(), line.begin(), [](unsigned char c)
                           { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : static_cast<char>(c); });
            Monstey_Require(line == "true" || line == "false" || line == "1" || line == "0",
                            "Use True or False on each line");
            values.as_array().push_back(line == "true" || line == "1");
        }
        else if (type >= 7 && type <= 9)
        {
            J vector;
            if (line.starts_with("["))
                vector = J::Parse(line);
            else
            {
                for (auto &c : line)
                    if (c == ',' || c == ';')
                        c = ' ';
                std::istringstream components(line);
                std::string component;
                vector = J::array();
                while (components >> component)
                {
                    auto n = J::Parse(component);
                    Monstey_Require(n.is_number(), "Vector components must be numbers");
                    vector.as_array().push_back(n);
                }
            }
            Monstey_Require(vector.is_array() && vector.size() == type - 5,
                            "Use the required number of vector components on each line");
            values.as_array().push_back(vector);
        }
        else
        {
            auto value = J::Parse(line);
            Monstey_Require(value.is_number(), "Use one numeric value per line");
            values.as_array().push_back(value);
        }
    }
    return values;
}

} // namespace Monstey::Monstey_AppInternal
