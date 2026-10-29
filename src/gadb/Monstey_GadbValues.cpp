#include "Monstey_GadbPrivate.hpp"

namespace Monstey
{
using namespace Monstey_GadbPrivate;

J Monstey_Gadb::Monstey_Impl::Curve(std::uint32_t pointer, std::uint8_t type) const
{
    const std::size_t stride = type == 16 ? 16 : 8;
    Monstey_Require(pointer % 4 == 0 && pointer <= stringSize && 4 <= stringSize - pointer,
                    "FX curve pointer outside atom pool");
    const auto count = Monstey_U32(data, stringBase + pointer);
    Monstey_Require(count <= 65536 && count <= (stringSize - pointer - 4) / stride, "FX curve knots exceed atom pool");
    J knots = J::array();
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto at = stringBase + pointer + 4 + i * stride;
        J value;
        if (type == 12)
            value = J(Monstey_U32(data, at + 4));
        else if (type == 13)
            value = Monstey_FloatJson(Monstey_U32(data, at + 4));
        else if (type == 15)
            value = J(static_cast<std::int64_t>(static_cast<std::int32_t>(Monstey_U32(data, at + 4))));
        else
        {
            value = J::array();
            for (std::size_t k = 1; k <= 3; ++k)
                value.as_array().push_back(Monstey_FloatJson(Monstey_U32(data, at + k * 4)));
        }
        knots.as_array().push_back(J::object({{"time", Monstey_FloatJson(Monstey_U32(data, at))}, {"value", value}}));
    }
    return J::object({{"value_type", type == 12   ? "color"
                                     : type == 13 ? "float"
                                     : type == 15 ? "int"
                                                  : "vec3"},
                      {"knots", knots}});
}

J Monstey_Gadb::Monstey_Impl::NormalizeCurve(std::uint8_t type, const J &input) const
{
    auto number = [](const J &value)
    {
        Monstey_Require(value.is_number() && std::isfinite(value.as_number()) &&
                            std::abs(value.as_number()) <= std::numeric_limits<float>::max(),
                        "Curve needs finite float32 components");
        return static_cast<double>(static_cast<float>(value.as_number()));
    };
    J values;
    if (input.is_object() && input.contains("constant"))
        values = J::array({J::object({{"time", 0}, {"value", input.at("constant")}}),
                           J::object({{"time", 1}, {"value", input.at("constant")}})});
    else if (input.is_object() && input.contains("knots"))
        values = input.at("knots");
    else
        values = input;
    Monstey_Require(values.is_array() && values.size() >= 1 && values.size() <= 65536, "Curve needs 1..65536 knots");
    J knots = J::array();
    double previous = -1;
    for (const auto &knot : values.as_array())
    {
        Monstey_Require(knot.is_object() && knot.contains("time") && knot.contains("value"),
                        "Curve knot needs time and value");
        const auto time = number(knot.at("time"));
        Monstey_Require(time >= 0 && time >= previous, "Curve times must be nonnegative and sorted");
        previous = time;
        const auto &sourceValue = knot.at("value");
        J value;
        if (type == 12)
        {
            Monstey_Require(sourceValue.is_integer() && sourceValue.as_int() >= 0 &&
                                sourceValue.as_uint() <= 0xffffffffu,
                            "Color curve requires uint32 ARGB");
            value = sourceValue;
        }
        else if (type == 13)
            value = number(sourceValue);
        else if (type == 15)
        {
            Monstey_Require(sourceValue.is_integer() &&
                                sourceValue.as_int() >= std::numeric_limits<std::int32_t>::min() &&
                                sourceValue.as_int() <= std::numeric_limits<std::int32_t>::max(),
                            "Integer curve requires int32");
            value = sourceValue;
        }
        else
        {
            Monstey_Require(sourceValue.is_array() && sourceValue.size() == 3,
                            "Vector curve requires three components");
            value = J::array();
            for (const auto &v : sourceValue.as_array())
                value.as_array().push_back(number(v));
        }
        knots.as_array().push_back(J::object({{"time", time}, {"value", value}}));
    }
    return J::object({{"value_type", type == 12   ? "color"
                                     : type == 13 ? "float"
                                     : type == 15 ? "int"
                                                  : "vec3"},
                      {"knots", knots}});
}

bool Monstey_Gadb::Monstey_Impl::Variable(const Monstey_Descriptor &d, const J &f) const
{
    // Reflection marks curve knots as arrays, but the descriptor stores one atom pointer.
    if (d.type == 12 || d.type == 13 || d.type == 15 || d.type == 16)
        return false;
    return d.type == 11 || (f.contains("array") && f.at("array").is_integer() && f.at("array").as_int() == -1) ||
           (f.contains("reflected_array") && f.at("reflected_array").is_bool() && f.at("reflected_array").as_bool());
}

bool Monstey_Gadb::Monstey_Impl::Array(const Monstey_Descriptor &d, const J &f) const
{
    return Variable(d, f) || d.count != 1;
}

J Monstey_Gadb::Monstey_Impl::Decode(const Monstey_RecordData &r, const Monstey_Descriptor &d, bool array) const
{
    const auto words = Words(r);
    if (d.type == 1)
        return J((words[d.offset / 32] & (1u << (d.offset % 32))) != 0);
    J out = J::array();
    for (std::size_t i = 0; i < d.count; ++i)
    {
        const auto w = words[d.offset + i];
        J v;
        switch (d.type)
        {
        case 2:
            v = Monstey_FloatJson(w);
            break;
        case 3:
            v = J(static_cast<std::int64_t>(static_cast<std::int32_t>(w)));
            break;
        case 4:
        case 5:
        case 68:
            v = J(Monstey_DisplayUtf8(String(w)));
            break;
        case 6:
            v = J(WideText(w));
            break;
        case 7:
        case 8:
        case 9:
        {
            Monstey_Require(w % 4 == 0 && w <= stringSize && static_cast<std::size_t>(d.type - 5) * 4 <= stringSize - w,
                            "Vector atom outside pool");
            v = J::array();
            for (int k = 0; k < d.type - 5; ++k)
                v.as_array().push_back(
                    Monstey_FloatJson(Monstey_U32(data, stringBase + w + static_cast<std::size_t>(k) * 4)));
            break;
        }
        case 12:
        case 13:
        case 15:
        case 16:
            v = Curve(w, d.type);
            break;
        case 10:
        case 11:
            if (w == 0xffffffffu)
                v = nullptr;
            else
            {
                Base(w);
                v = J(Monstey_Hex(w));
            }
            break;
        default:
            v = J::object({{"raw", Monstey_Hex(w)}, {"undecoded_type", d.type}});
        }
        out.as_array().push_back(v);
    }
    return array ? out : (out.empty() ? J() : out.at(0));
}

J Monstey_Gadb::Monstey_Impl::Normalize(std::uint32_t ref, const Monstey_Descriptor &d, const J &f,
                                        const J &value) const
{
    auto scalar = [&](const J &item) -> J
    {
        switch (d.type)
        {
        case 1:
            Monstey_Require(item.is_bool(), "Boolean requires true or false");
            return item;
        case 2:
        {
            Monstey_Require(item.is_number(), "Float requires a number");
            const auto n = item.as_number();
            Monstey_Require(std::isfinite(n) && std::abs(n) <= std::numeric_limits<float>::max(),
                            "Finite float32 required");
            for (const auto key : {"min", "max"})
                if (f.contains(key) && !f.at(key).is_null() &&
                    !(f.at(key).is_string() && f.at(key).as_string().empty()))
                {
                    const auto b = f.at(key).is_number() ? f.at(key).as_number() : std::stod(f.at(key).as_string());
                    Monstey_Require(std::string_view(key) == "min" ? n >= b : n <= b,
                                    "Float violates native schema range");
                }
            return J(static_cast<double>(static_cast<float>(n)));
        }
        case 3:
        {
            J v = item;
            if (item.is_string() && f.contains("enum") && schema.contains("enums") &&
                schema.at("enums").contains(f.at("enum").as_string()))
            {
                bool found = false;
                for (const auto &e : schema.at("enums").at(f.at("enum").as_string()).as_array())
                    if (e.at("name").as_string() == item.as_string())
                    {
                        v = e.at("index");
                        found = true;
                        break;
                    }
                Monstey_Require(found, "Unknown enum value");
            }
            Monstey_Require(v.is_integer() && v.as_int() >= std::numeric_limits<std::int32_t>::min() &&
                                v.as_int() <= std::numeric_limits<std::int32_t>::max(),
                            "Int32 required");
            for (const auto key : {"min", "max"})
                if (f.contains(key) && !f.at(key).is_null() &&
                    !(f.at(key).is_string() && f.at(key).as_string().empty()))
                {
                    const auto b = f.at(key).is_number() ? f.at(key).as_number() : std::stod(f.at(key).as_string());
                    Monstey_Require(std::string_view(key) == "min" ? static_cast<double>(v.as_int()) >= b
                                                                   : static_cast<double>(v.as_int()) <= b,
                                    "Integer violates native schema range");
                }
            return v;
        }
        case 4:
        case 5:
        case 6:
        case 68:
        {
            Monstey_Require(item.is_string() && item.as_string().find('\0') == std::string::npos,
                            "Text must be a string without NUL");
            Monstey_Wide(item.as_string());
            if (d.type == 5 && f.contains("enum_values") && !f.at("enum_values").empty())
            {
                bool found = false;
                for (const auto &e : f.at("enum_values").as_array())
                    if (e == item)
                        found = true;
                Monstey_Require(found, "Unknown native enum token");
            }
            return item;
        }
        case 7:
        case 8:
        case 9:
        {
            Monstey_Require(item.is_array() && item.size() == static_cast<std::size_t>(d.type - 5),
                            "Wrong vector dimension");
            J out = J::array();
            for (const auto &v : item.as_array())
            {
                Monstey_Require(v.is_number() && std::isfinite(v.as_number()) &&
                                    std::abs(v.as_number()) <= std::numeric_limits<float>::max(),
                                "Vector components must be finite float32");
                out.as_array().emplace_back(static_cast<double>(static_cast<float>(v.as_number())));
            }
            return out;
        }
        case 12:
        case 13:
        case 15:
        case 16:
            return NormalizeCurve(d.type, item);
        case 10:
        case 11:
        {
            const auto target = ValueRef(item);
            if (target != 0xffffffffu && f.contains("target") && f.at("target").is_string())
                Monstey_Require(Allowed(f.at("target").as_string(), CategoryText(target >> 20, 1)),
                                "Reference target violates native category hierarchy");
            return target == 0xffffffffu ? J() : J(Monstey_Hex(target));
        }
        default:
            throw Monstey_Error("gadb_type", "Unsupported native field type");
        }
    };
    (void)ref;
    if (Array(d, f))
    {
        Monstey_Require(value.is_array() && value.size() <= 65535, "Array needs at most 65535 elements");
        Monstey_Require(Variable(d, f) || value.size() == d.count, "Cannot resize a fixed native array");
        J out = J::array();
        for (const auto &v : value.as_array())
            out.as_array().push_back(scalar(v));
        return out;
    }
    return scalar(value);
}

} // namespace Monstey
