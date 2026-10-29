#include "Monstey_Curve.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace Monstey {
namespace {
void Monstey_CurveRequire(bool value, std::string_view message) {
    if (!value)
        throw Monstey_Error("curve_edit", std::string(message));
}
double Monstey_CurveFloat(double value) {
    Monstey_CurveRequire(std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max(),
                         "Curve component must fit a finite Float32");
    return static_cast<double>(static_cast<float>(value));
}
void Monstey_CurveViewCheck(const Monstey_CurveView &v, double width, double height) {
    Monstey_CurveRequire(std::isfinite(width) && std::isfinite(height) && width > 0 && height > 0 &&
                             std::isfinite(v.time_min) && std::isfinite(v.time_max) &&
                             v.time_max > v.time_min && std::isfinite(v.value_min) &&
                             std::isfinite(v.value_max) && v.value_max > v.value_min,
                         "Curve viewport is invalid");
}
} // namespace
Monstey_CurvePoint Monstey_CurveDraft::Normalize(double time, const std::array<double, 4> &value) const {
    Monstey_CurvePoint out;
    out.time = Monstey_CurveFloat(time);
    Monstey_CurveRequire(out.time >= 0, "Curve time must be nonnegative");
    for (unsigned c = 0; c < Components(); ++c) {
        const auto x = value[c];
        if (type_ == 12) {
            Monstey_CurveRequire(std::isfinite(x) && x >= 0 && x <= 255 && std::floor(x) == x,
                                 "Color component must be a byte");
            out.value[c] = x;
        } else if (type_ == 15) {
            Monstey_CurveRequire(std::isfinite(x) && x >= std::numeric_limits<std::int32_t>::min() &&
                                     x <= std::numeric_limits<std::int32_t>::max() && std::floor(x) == x,
                                 "Curve value must be int32");
            out.value[c] = x;
        } else
            out.value[c] = Monstey_CurveFloat(x);
    }
    return out;
}
Monstey_CurveDraft::Monstey_CurveDraft(unsigned type, const Monstey_Json &input) : type_(type) {
    Monstey_CurveRequire(type == 12 || type == 13 || type == 15 || type == 16,
                         "Unsupported native curve type");
    auto knots = input.is_object() && input.contains("knots") ? input["knots"] : input;
    if (input.is_object() && input.contains("constant"))
        knots = Monstey_Json::array({Monstey_Json::object({{"time", 0}, {"value", input["constant"]}}),
                                     Monstey_Json::object({{"time", 1}, {"value", input["constant"]}})});
    Monstey_CurveRequire(knots.is_array() && knots.size() <= 65536, "Curve requires at most 65536 knots");
    double previous = -1;
    for (const auto &k : knots.as_array()) {
        Monstey_CurveRequire(k.is_object() && k.contains("time") && k.contains("value") &&
                                 k["time"].is_number(),
                             "Curve knot needs numeric time and value");
        std::array<double, 4> v{};
        const auto &j = k["value"];
        if (type == 12) {
            Monstey_CurveRequire(j.is_integer() && j.as_int() >= 0 && j.as_uint() <= 0xffffffff,
                                 "Color curve requires uint32 ARGB");
            const auto argb = static_cast<std::uint32_t>(j.as_uint());
            v = {static_cast<double>((argb >> 16) & 255), static_cast<double>((argb >> 8) & 255),
                 static_cast<double>(argb & 255), static_cast<double>(argb >> 24)};
        } else if (type == 16) {
            Monstey_CurveRequire(j.is_array() && j.size() == 3, "Vector curve requires XYZ");
            for (unsigned c = 0; c < 3; ++c) {
                Monstey_CurveRequire(j[c].is_number(), "Curve vector component must be numeric");
                v[c] = j[c].as_number();
            }
        } else {
            Monstey_CurveRequire(j.is_number() && (type != 15 || j.is_integer()),
                                 "Scalar curve value has wrong numeric type");
            v[0] = j.as_number();
        }
        auto point = Normalize(k["time"].as_number(), v);
        Monstey_CurveRequire(point.time >= previous, "Native curve knots must be time sorted");
        previous = point.time;
        points_.push_back(point);
    }
}
Monstey_Json Monstey_CurveDraft::Value() const {
    using J = Monstey_Json;
    J knots = J::array();
    for (const auto &p : points_) {
        J value;
        if (type_ == 12) {
            const auto argb = (static_cast<std::uint32_t>(p.value[3]) << 24) |
                              (static_cast<std::uint32_t>(p.value[0]) << 16) |
                              (static_cast<std::uint32_t>(p.value[1]) << 8) |
                              static_cast<std::uint32_t>(p.value[2]);
            value = argb;
        } else if (type_ == 15)
            value = static_cast<std::int64_t>(p.value[0]);
        else if (type_ == 16)
            value = J::array({p.value[0], p.value[1], p.value[2]});
        else
            value = p.value[0];
        knots.as_array().push_back(J::object({{"time", p.time}, {"value", value}}));
    }
    return J::object({{"value_type", type_ == 12   ? "color"
                                     : type_ == 13 ? "float"
                                     : type_ == 15 ? "int"
                                                   : "vec3"},
                      {"knots", knots}});
}
std::size_t Monstey_CurveDraft::Add(double time, const std::array<double, 4> &v) {
    Monstey_CurveRequire(points_.size() < 65536, "Native curve knot limit reached");
    const auto p = Normalize(time, v);
    const auto it = std::upper_bound(points_.begin(), points_.end(), p.time,
                                     [](double t, const auto &point) { return t < point.time; });
    const auto index = static_cast<std::size_t>(it - points_.begin());
    points_.insert(it, p);
    return index;
}
std::size_t Monstey_CurveDraft::Set(std::size_t index, double time, const std::array<double, 4> &v) {
    Monstey_CurveRequire(index < points_.size(), "Curve point index is invalid");
    const auto p = Normalize(time, v);
    if (p.time == points_[index].time) {
        points_[index] = p;
        return index;
    }
    points_.erase(points_.begin() + static_cast<std::ptrdiff_t>(index));
    const auto it = std::upper_bound(points_.begin(), points_.end(), p.time,
                                     [](double t, const auto &point) { return t < point.time; });
    const auto next = static_cast<std::size_t>(it - points_.begin());
    points_.insert(it, p);
    return next;
}
void Monstey_CurveDraft::Erase(std::size_t index) {
    Monstey_CurveRequire(index < points_.size() && points_.size() > 1,
                         "A curve must retain at least one point");
    points_.erase(points_.begin() + static_cast<std::ptrdiff_t>(index));
}
Monstey_CurveView Monstey_CurveDraft::Fit() const {
    Monstey_CurveView v;
    if (type_ == 12) {
        v.value_min = 0;
        v.value_max = 255;
        if (!points_.empty())
            v.time_max = std::max(1.0, points_.back().time * 1.05);
        return v;
    }
    if (points_.empty())
        return v;
    v.time_max = std::max(1.0, points_.back().time * 1.05);
    if (type_ == 12) {
        v.value_min = 0;
        v.value_max = 255;
        return v;
    }
    v.value_min = std::numeric_limits<double>::max();
    v.value_max = std::numeric_limits<double>::lowest();
    for (const auto &p : points_)
        for (unsigned c = 0; c < Components(); ++c) {
            v.value_min = std::min(v.value_min, p.value[c]);
            v.value_max = std::max(v.value_max, p.value[c]);
        }
    const auto span = v.value_max - v.value_min;
    const auto pad = span > 0 ? span * .1 : std::max(1.0, std::abs(v.value_min) * .1);
    v.value_min -= pad;
    v.value_max += pad;
    return v;
}
Monstey_CurvePixel Monstey_CurveDraft::Project(std::size_t index, unsigned c, const Monstey_CurveView &v,
                                               double w, double h) const {
    Monstey_CurveViewCheck(v, w, h);
    Monstey_CurveRequire(index < points_.size() && c < Components(), "Curve selection is invalid");
    const auto &p = points_[index];
    return {(p.time - v.time_min) / (v.time_max - v.time_min) * w,
            (v.value_max - p.value[c]) / (v.value_max - v.value_min) * h};
}
std::size_t Monstey_CurveDraft::Hit(double x, double y, unsigned c, const Monstey_CurveView &v, double w,
                                    double h, double radius) const {
    Monstey_CurveRequire(std::isfinite(x) && std::isfinite(y) && std::isfinite(radius) && radius >= 0,
                         "Curve pointer coordinates are invalid");
    Monstey_CurveViewCheck(v, w, h);
    Monstey_CurveRequire(c < Components(), "Curve component is invalid");
    auto best = NoPoint;
    auto distance = radius * radius;
    for (std::size_t i = 0; i < points_.size(); ++i) {
        const auto p = Project(i, c, v, w, h);
        const auto d = (x - p.x) * (x - p.x) + (y - p.y) * (y - p.y);
        if (d <= distance) {
            best = i;
            distance = d;
        }
    }
    return best;
}
double Monstey_CurveDraft::PixelTime(double x, const Monstey_CurveView &v, double width) const {
    Monstey_CurveViewCheck(v, width, 1);
    Monstey_CurveRequire(std::isfinite(x), "Curve pointer is invalid");
    return std::clamp(v.time_min + std::clamp(x / width, 0.0, 1.0) * (v.time_max - v.time_min), 0.0,
                      static_cast<double>(std::numeric_limits<float>::max()));
}
double Monstey_CurveDraft::PixelValue(double y, const Monstey_CurveView &v, double height) const {
    Monstey_CurveViewCheck(v, 1, height);
    Monstey_CurveRequire(std::isfinite(y), "Curve pointer is invalid");
    auto x = v.value_max - std::clamp(y / height, 0.0, 1.0) * (v.value_max - v.value_min);
    if (type_ == 12)
        return std::round(std::clamp(x, 0.0, 255.0));
    if (type_ == 15)
        return std::round(std::clamp(x, static_cast<double>(std::numeric_limits<std::int32_t>::min()),
                                     static_cast<double>(std::numeric_limits<std::int32_t>::max())));
    return std::clamp(x, -static_cast<double>(std::numeric_limits<float>::max()),
                      static_cast<double>(std::numeric_limits<float>::max()));
}
std::size_t Monstey_CurveDraft::Drag(std::size_t index, unsigned c, double x, double y,
                                     const Monstey_CurveView &v, double w, double h) {
    Monstey_CurveRequire(index < points_.size() && c < Components(), "Curve selection is invalid");
    auto value = points_[index].value;
    value[c] = PixelValue(y, v, h);
    return Set(index, PixelTime(x, v, w), value);
}
} // namespace Monstey
