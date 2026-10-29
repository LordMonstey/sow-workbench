#include "Monstey_Core.hpp"
#include <charconv>
#include <cmath>
#include <limits>
#include <sstream>
#include <system_error>
#include <type_traits>
#include <utility>

namespace Monstey {
Monstey_Json::Monstey_Json(std::uint64_t v) {
    Monstey_Require(v <= static_cast<std::uint64_t>(INT64_MAX), "JSON integer exceeds int64");
    value_ = static_cast<std::int64_t>(v);
}
Monstey_Json::Monstey_Json(double v) : value_(v) { Monstey_Require(std::isfinite(v), "JSON number must be finite"); }
Monstey_Json Monstey_Json::object(std::initializer_list<Object::value_type> v) { return Object(v); }
Monstey_Json Monstey_Json::array(std::initializer_list<Monstey_Json> v) { return Array(v); }
bool Monstey_Json::is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(value_); }
bool Monstey_Json::is_bool() const noexcept { return std::holds_alternative<bool>(value_); }
bool Monstey_Json::is_integer() const noexcept { return std::holds_alternative<std::int64_t>(value_); }
bool Monstey_Json::is_number() const noexcept { return is_integer() || std::holds_alternative<double>(value_); }
bool Monstey_Json::is_string() const noexcept { return std::holds_alternative<std::string>(value_); }
bool Monstey_Json::is_array() const noexcept { return std::holds_alternative<Array>(value_); }
bool Monstey_Json::is_object() const noexcept { return std::holds_alternative<Object>(value_); }
bool Monstey_Json::as_bool() const { Monstey_Require(is_bool(), "Expected JSON Boolean"); return std::get<bool>(value_); }
std::int64_t Monstey_Json::as_int() const {
    if (is_integer()) return std::get<std::int64_t>(value_);
    Monstey_Require(is_number(), "Expected JSON integer");
    const auto n = std::get<double>(value_);
    Monstey_Require(std::trunc(n) == n && n >= -9223372036854775808.0 && n < 9223372036854775808.0, "Expected representable JSON integer");
    return static_cast<std::int64_t>(n);
}
std::uint64_t Monstey_Json::as_uint() const { const auto n = as_int(); Monstey_Require(n >= 0, "Expected unsigned JSON integer"); return static_cast<std::uint64_t>(n); }
double Monstey_Json::as_number() const { Monstey_Require(is_number(), "Expected JSON number"); return is_integer() ? static_cast<double>(std::get<std::int64_t>(value_)) : std::get<double>(value_); }
const std::string& Monstey_Json::as_string() const { Monstey_Require(is_string(), "Expected JSON string"); return std::get<std::string>(value_); }
const Monstey_Json::Array& Monstey_Json::as_array() const { Monstey_Require(is_array(), "Expected JSON array"); return std::get<Array>(value_); }
Monstey_Json::Array& Monstey_Json::as_array() { Monstey_Require(is_array(), "Expected JSON array"); return std::get<Array>(value_); }
const Monstey_Json::Object& Monstey_Json::as_object() const { Monstey_Require(is_object(), "Expected JSON object"); return std::get<Object>(value_); }
Monstey_Json::Object& Monstey_Json::as_object() { Monstey_Require(is_object(), "Expected JSON object"); return std::get<Object>(value_); }
bool Monstey_Json::contains(std::string_view key) const { return is_object() && as_object().find(key) != as_object().end(); }
const Monstey_Json& Monstey_Json::at(std::string_view key) const { const auto& o = as_object(); const auto p = o.find(key); Monstey_Require(p != o.end(), "JSON key missing: " + std::string(key)); return p->second; }
Monstey_Json& Monstey_Json::at(std::string_view key) { return const_cast<Monstey_Json&>(std::as_const(*this).at(key)); }
const Monstey_Json& Monstey_Json::at(std::size_t index) const { const auto& a = as_array(); Monstey_Require(index < a.size(), "JSON array index outside bounds"); return a[index]; }
Monstey_Json& Monstey_Json::at(std::size_t index) { return const_cast<Monstey_Json&>(std::as_const(*this).at(index)); }
Monstey_Json& Monstey_Json::operator[](std::string_view key) { if (is_null()) value_ = Object{}; return as_object()[std::string(key)]; }
const Monstey_Json& Monstey_Json::operator[](std::string_view key) const { return at(key); }
Monstey_Json& Monstey_Json::operator[](std::size_t index) { return at(index); }
const Monstey_Json& Monstey_Json::operator[](std::size_t index) const { return at(index); }
Monstey_Json Monstey_Json::value(std::string_view key, const Monstey_Json& fallback) const { return contains(key) ? at(key) : fallback; }
std::size_t Monstey_Json::size() const noexcept {
    if (const auto* a = std::get_if<Array>(&value_)) return a->size();
    if (const auto* o = std::get_if<Object>(&value_)) return o->size();
    if (const auto* s = std::get_if<std::string>(&value_)) return s->size();
    return 0;
}
namespace {
void Monstey_AppendUtf8(std::string& s, std::uint32_t c) {
    if (c <= 0x7f) s.push_back(static_cast<char>(c));
    else if (c <= 0x7ff) { s.push_back(static_cast<char>(0xc0 | (c >> 6))); s.push_back(static_cast<char>(0x80 | (c & 63))); }
    else if (c <= 0xffff) { s.push_back(static_cast<char>(0xe0 | (c >> 12))); s.push_back(static_cast<char>(0x80 | ((c >> 6) & 63))); s.push_back(static_cast<char>(0x80 | (c & 63))); }
    else { s.push_back(static_cast<char>(0xf0 | (c >> 18))); s.push_back(static_cast<char>(0x80 | ((c >> 12) & 63))); s.push_back(static_cast<char>(0x80 | ((c >> 6) & 63))); s.push_back(static_cast<char>(0x80 | (c & 63))); }
}
class Monstey_JsonParser {
public:
    explicit Monstey_JsonParser(std::string_view text) : text_(text) {
        Monstey_Require(text.size() <= 64ull * 1024 * 1024, "JSON input exceeds 64 MiB");
        if (text_.starts_with("\xef\xbb\xbf")) at_ = 3;
    }
    Monstey_Json parse() { auto v = parse_value(0); ws(); Monstey_Require(at_ == text_.size(), "Trailing JSON data"); return v; }
private:
    std::string_view text_; std::size_t at_ = 0;
    void ws() { while (at_ < text_.size() && (text_[at_] == ' ' || text_[at_] == '\r' || text_[at_] == '\n' || text_[at_] == '\t')) ++at_; }
    char get() { Monstey_Require(at_ < text_.size(), "Unexpected end of JSON"); return text_[at_++]; }
    bool take(char c) { ws(); if (at_ < text_.size() && text_[at_] == c) { ++at_; return true; } return false; }
    std::uint32_t hex4() {
        std::uint32_t n = 0;
        for (int i = 0; i != 4; ++i) { const auto c = get(); const int x = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; Monstey_Require(x >= 0, "Invalid JSON Unicode escape"); n = (n << 4) | static_cast<unsigned>(x); }
        return n;
    }
    std::string string() {
        Monstey_Require(get() == '"', "Expected JSON string"); std::string out;
        while (true) {
            auto c = get();
            if (c == '"') break;
            Monstey_Require(static_cast<unsigned char>(c) >= 0x20, "Unescaped JSON control byte");
            if (c != '\\') { out.push_back(c); continue; }
            c = get();
            switch (c) {
            case '"': case '\\': case '/': out.push_back(c); break;
            case 'b': out.push_back('\b'); break; case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break; case 'r': out.push_back('\r'); break; case 't': out.push_back('\t'); break;
            case 'u': {
                auto n = hex4();
                if (n >= 0xd800 && n <= 0xdbff) { Monstey_Require(get() == '\\' && get() == 'u', "Missing low surrogate"); const auto low = hex4(); Monstey_Require(low >= 0xdc00 && low <= 0xdfff, "Invalid low surrogate"); n = 0x10000 + ((n - 0xd800) << 10) + low - 0xdc00; }
                else Monstey_Require(n < 0xdc00 || n > 0xdfff, "Unpaired low surrogate");
                Monstey_AppendUtf8(out, n); break;
            }
            default: throw Monstey_Error("invalid_json", "Invalid JSON escape");
            }
        }
        (void)Monstey_Wide(out);
        return out;
    }
    Monstey_Json parse_value(unsigned depth) {
        Monstey_Require(depth <= 128, "JSON nesting exceeds 128");
        ws(); Monstey_Require(at_ < text_.size(), "Missing JSON value");
        if (text_[at_] == '"') return string();
        if (take('{')) {
            auto o = Monstey_Json::object();
            if (take('}')) return o;
            do { ws(); Monstey_Require(at_ < text_.size() && text_[at_] == '"', "Expected object key"); auto key = string(); Monstey_Require(!o.contains(key), "Duplicate JSON member: " + key); Monstey_Require(take(':'), "Missing JSON colon"); o[key] = parse_value(depth + 1); } while (take(','));
            Monstey_Require(take('}'), "Missing JSON object terminator"); return o;
        }
        if (take('[')) { auto a = Monstey_Json::array(); if (take(']')) return a; do { a.as_array().push_back(parse_value(depth + 1)); } while (take(',')); Monstey_Require(take(']'), "Missing JSON array terminator"); return a; }
        for (const auto literal : {std::string_view("null"), std::string_view("true"), std::string_view("false")}) {
            if (text_.substr(at_, literal.size()) == literal) { at_ += literal.size(); return literal == "null" ? Monstey_Json(nullptr) : Monstey_Json(literal == "true"); }
        }
        const auto start = at_;
        if (text_[at_] == '-') ++at_;
        Monstey_Require(at_ < text_.size(), "Incomplete JSON number");
        if (text_[at_] == '0') ++at_;
        else { Monstey_Require(text_[at_] >= '1' && text_[at_] <= '9', "Invalid JSON number"); while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') ++at_; }
        bool integer = true;
        if (at_ < text_.size() && text_[at_] == '.') { integer = false; ++at_; const auto first = at_; while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') ++at_; Monstey_Require(at_ != first, "Missing JSON fractional digits"); }
        if (at_ < text_.size() && (text_[at_] == 'e' || text_[at_] == 'E')) { integer = false; ++at_; if (at_ < text_.size() && (text_[at_] == '-' || text_[at_] == '+')) ++at_; const auto first = at_; while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') ++at_; Monstey_Require(at_ != first, "Missing JSON exponent"); }
        const auto token = text_.substr(start, at_ - start);
        if (integer) { std::int64_t n = 0; const auto r = std::from_chars(token.data(), token.data() + token.size(), n); Monstey_Require(r.ec == std::errc{} && r.ptr == token.data() + token.size(), "JSON integer outside int64"); return n; }
        double n = 0; const auto r = std::from_chars(token.data(), token.data() + token.size(), n); Monstey_Require(r.ec == std::errc{} && r.ptr == token.data() + token.size() && std::isfinite(n), "JSON number not finite/representable"); return n;
    }
};
void Monstey_JsonQuote(std::string& out, std::string_view text) {
    (void)Monstey_Wide(text);
    constexpr char hex[] = "0123456789abcdef";
    out.push_back('"');
    for (const auto raw : text) { const auto c = static_cast<unsigned char>(raw); switch (c) {
        case '"': out += "\\\""; break; case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break; case '\f': out += "\\f"; break; case '\n': out += "\\n"; break; case '\r': out += "\\r"; break; case '\t': out += "\\t"; break;
        default: if (c < 32) { out += "\\u00"; out.push_back(hex[c >> 4]); out.push_back(hex[c & 15]); } else out.push_back(raw);
    }}
    out.push_back('"');
}
void Monstey_JsonDump(const Monstey_Json& v, std::string& out, int indent, unsigned depth) {
    Monstey_Require(depth <= 128, "JSON output nesting exceeds 128");
    auto newline = [&](unsigned level) { if (indent >= 0) { out.push_back('\n'); out.append(static_cast<std::size_t>(indent) * level, ' '); } };
    if (v.is_null()) out += "null";
    else if (v.is_bool()) out += v.as_bool() ? "true" : "false";
    else if (v.is_integer()) out += std::to_string(v.as_int());
    else if (v.is_number()) { char buf[64]{}; const auto r = std::to_chars(buf, buf + sizeof(buf), v.as_number(), std::chars_format::general, std::numeric_limits<double>::max_digits10); Monstey_Require(r.ec == std::errc{}, "Cannot encode JSON number"); out.append(buf, r.ptr); const std::string_view token(buf, static_cast<std::size_t>(r.ptr-buf)); if (token.find_first_of(".eE") == std::string_view::npos) out += ".0"; }
    else if (v.is_string()) Monstey_JsonQuote(out, v.as_string());
    else if (v.is_array()) { out.push_back('['); bool first = true; for (const auto& item : v.as_array()) { if (!first) out.push_back(','); newline(depth + 1); Monstey_JsonDump(item, out, indent, depth + 1); first = false; } if (!first) newline(depth); out.push_back(']'); }
    else { out.push_back('{'); bool first = true; for (const auto& [key, item] : v.as_object()) { if (!first) out.push_back(','); newline(depth + 1); Monstey_JsonQuote(out, key); out += indent >= 0 ? ": " : ":"; Monstey_JsonDump(item, out, indent, depth + 1); first = false; } if (!first) newline(depth); out.push_back('}'); }
}
}
Monstey_Json Monstey_Json::Parse(std::string_view text) { return Monstey_JsonParser(text).parse(); }
std::string Monstey_Json::dump(int indent) const { Monstey_Require(indent >= -1 && indent <= 16, "JSON indent must be -1..16"); std::string out; Monstey_JsonDump(*this, out, indent, 0); return out; }
}
