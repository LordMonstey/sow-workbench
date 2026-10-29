#include "Monstey_RuntimeRegistry.hpp"
#include "Monstey_Core.hpp"

namespace Monstey
{
namespace Monstey_RuntimeDetail
{
namespace
{
bool Monstey_IsType(const Monstey_Json &value, std::string_view type)
{
    if (type == "null")
        return value.is_null();
    if (type == "string")
        return value.is_string();
    if (type == "integer")
        return value.is_integer();
    if (type == "boolean")
        return value.is_bool();
    if (type == "number")
        return value.is_number();
    if (type == "object")
        return value.is_object();
    if (type == "array")
        return value.is_array();
    return false;
}
} // namespace
void Monstey_Validate(const Monstey_Json &arguments, const Monstey_Json &schema)
{
    if (!arguments.is_object())
        throw Monstey_Error("invalid_arguments", "Arguments must be an object");
    for (const auto &required : schema.at("required").as_array())
        if (!arguments.contains(required.as_string()))
            throw Monstey_Error("missing_argument", "Missing argument: " + required.as_string());
    const auto &properties = schema.at("properties");
    for (const auto &[name, value] : arguments.as_object())
    {
        if (!properties.contains(name))
            throw Monstey_Error("unknown_argument", "Unknown argument: " + name);
        const auto &rule = properties.at(name);
        if (!rule.contains("type"))
            continue;
        const auto &type = rule.at("type");
        bool valid = false;
        if (type.is_string())
            valid = Monstey_IsType(value, type.as_string());
        else
            for (const auto &alternative : type.as_array())
                valid = valid || Monstey_IsType(value, alternative.as_string());
        if (!valid)
            throw Monstey_Error("invalid_argument_type", "Invalid type for argument: " + name);
        if (value.is_integer() && ((rule.contains("minimum") && value.as_int() < rule.at("minimum").as_int()) ||
                                   (rule.contains("maximum") && value.as_int() > rule.at("maximum").as_int())))
            throw Monstey_Error("argument_out_of_range", "Argument is outside its bounds: " + name);
        if (value.is_string() && ((rule.contains("minLength") && value.size() < rule.at("minLength").as_uint()) ||
                                  (rule.contains("maxLength") && value.size() > rule.at("maxLength").as_uint())))
            throw Monstey_Error("argument_out_of_range", "Argument length is outside its bounds: " + name);
    }
}
} // namespace Monstey_RuntimeDetail
} // namespace Monstey
