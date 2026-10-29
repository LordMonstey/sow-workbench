#pragma once
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <utility>
#include <vector>

namespace Monstey {
class Monstey_Json {
public:
    using Array = std::vector<Monstey_Json>;
    using Object = std::map<std::string, Monstey_Json, std::less<>>;
    using Value = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, Array, Object>;
    Monstey_Json() noexcept : value_(nullptr) {}
    Monstey_Json(std::nullptr_t) noexcept : value_(nullptr) {}
    Monstey_Json(bool v) : value_(v) {}
    Monstey_Json(int v) : value_(static_cast<std::int64_t>(v)) {}
    Monstey_Json(unsigned int v) : value_(static_cast<std::int64_t>(v)) {}
    Monstey_Json(std::int64_t v) : value_(v) {}
    Monstey_Json(std::uint64_t v);
    Monstey_Json(double v);
    Monstey_Json(const char* v) : value_(std::string(v)) {}
    Monstey_Json(std::string v) : value_(std::move(v)) {}
    Monstey_Json(std::string_view v) : value_(std::string(v)) {}
    Monstey_Json(Array v) : value_(std::move(v)) {}
    Monstey_Json(Object v) : value_(std::move(v)) {}
    static Monstey_Json object(std::initializer_list<Object::value_type> values = {});
    static Monstey_Json array(std::initializer_list<Monstey_Json> values = {});
    static Monstey_Json Parse(std::string_view text);
    std::string dump(int indent = -1) const;
    bool is_null() const noexcept;
    bool is_bool() const noexcept;
    bool is_integer() const noexcept;
    bool is_number() const noexcept;
    bool is_string() const noexcept;
    bool is_array() const noexcept;
    bool is_object() const noexcept;
    bool as_bool() const;
    std::int64_t as_int() const;
    std::uint64_t as_uint() const;
    double as_number() const;
    const std::string& as_string() const;
    const Array& as_array() const;
    Array& as_array();
    const Object& as_object() const;
    Object& as_object();
    bool contains(std::string_view key) const;
    const Monstey_Json& at(std::string_view key) const;
    Monstey_Json& at(std::string_view key);
    const Monstey_Json& at(std::size_t index) const;
    Monstey_Json& at(std::size_t index);
    Monstey_Json& operator[](std::string_view key);
    const Monstey_Json& operator[](std::string_view key) const;
    Monstey_Json& operator[](std::size_t index);
    const Monstey_Json& operator[](std::size_t index) const;
    Monstey_Json value(std::string_view key, const Monstey_Json& fallback) const;
    std::size_t size() const noexcept;
    bool empty() const noexcept { return size() == 0; }
    bool operator==(const Monstey_Json&) const = default;
private:
    Value value_;
};
}
