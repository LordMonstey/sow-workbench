#pragma once
#include <windows.h>

namespace Monstey {
inline constexpr COLORREF Monstey_ThemeBackground = RGB(25, 28, 34);
inline constexpr COLORREF Monstey_ThemeSurface = RGB(34, 38, 46);
inline constexpr COLORREF Monstey_ThemeText = RGB(224, 229, 237);
inline constexpr COLORREF Monstey_ThemeMuted = RGB(155, 166, 182);
inline constexpr COLORREF Monstey_ThemeBorder = RGB(62, 70, 84);
inline constexpr COLORREF Monstey_ThemeSelection = RGB(46, 78, 112);
HBRUSH Monstey_ThemeBackgroundBrush();
void Monstey_ThemeApply(HWND window);
} // namespace Monstey