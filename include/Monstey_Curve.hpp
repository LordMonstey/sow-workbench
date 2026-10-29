#pragma once
#include "Monstey_Core.hpp"
#include <array>
namespace Monstey {
struct Monstey_CurvePoint {
    double time = 0;
    std::array<double, 4> value{};
};
struct Monstey_CurveView {
    double time_min = 0, time_max = 1, value_min = 0, value_max = 1;
};
struct Monstey_CurvePixel {
    double x = 0, y = 0;
};
class Monstey_CurveDraft {
  public:
    Monstey_CurveDraft(unsigned nativeType, const Monstey_Json &value);
    unsigned NativeType() const noexcept { return type_; }
    unsigned Components() const noexcept { return type_ == 12 ? 4 : type_ == 16 ? 3 : 1; }
    const std::vector<Monstey_CurvePoint> &Points() const noexcept { return points_; }
    Monstey_Json Value() const;
    std::size_t Set(std::size_t index, double time, const std::array<double, 4> &value);
    std::size_t Add(double time, const std::array<double, 4> &value);
    void Erase(std::size_t index);
    Monstey_CurveView Fit() const;
    Monstey_CurvePixel Project(std::size_t index, unsigned component, const Monstey_CurveView &view,
                               double width, double height) const;
    std::size_t Hit(double x, double y, unsigned component, const Monstey_CurveView &view, double width,
                    double height, double radius = 9) const;
    std::size_t Drag(std::size_t index, unsigned component, double x, double y, const Monstey_CurveView &view,
                     double width, double height);
    double PixelTime(double x, const Monstey_CurveView &view, double width) const;
    double PixelValue(double y, const Monstey_CurveView &view, double height) const;
    static constexpr std::size_t NoPoint = static_cast<std::size_t>(-1);

  private:
    unsigned type_;
    std::vector<Monstey_CurvePoint> points_;
    Monstey_CurvePoint Normalize(double time, const std::array<double, 4> &value) const;
};
} // namespace Monstey
