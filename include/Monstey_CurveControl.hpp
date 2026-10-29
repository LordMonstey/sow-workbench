#pragma once
#include "Monstey_Curve.hpp"
#include <memory>
#include <windows.h>
namespace Monstey {
enum Monstey_CurveControlId {
    Monstey_CurveSelector = 801,
    Monstey_CurveChannel = 802,
    Monstey_CurveTime = 803,
    Monstey_CurveValue = 804,
    Monstey_CurveSet = 805,
    Monstey_CurveAdd = 806,
    Monstey_CurveDelete = 807,
    Monstey_CurveFit = 808
};
class Monstey_CurveControl {
  public:
    Monstey_CurveControl(unsigned nativeType, const Monstey_Json &value);
    ~Monstey_CurveControl();
    Monstey_CurveControl(const Monstey_CurveControl &) = delete;
    Monstey_CurveControl &operator=(const Monstey_CurveControl &) = delete;
    static void Register(HINSTANCE instance);
    void Create(HWND parent, HFONT font);
    void Layout(int x, int y, int width, int height);
    HWND Window() const noexcept;
    RECT PlotBounds() const;
    Monstey_Json Value();

  private:
    struct Monstey_Impl;
    std::unique_ptr<Monstey_Impl> impl_;
};
} // namespace Monstey
