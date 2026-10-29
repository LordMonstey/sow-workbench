#include "Monstey_Theme.hpp"
#include "Monstey_CurveControl.hpp"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>
#include <windowsx.h>
namespace Monstey {
namespace {
constexpr wchar_t Monstey_CurveClass[] = L"Monstey_NativeFxCurve";
constexpr COLORREF Monstey_CurveColors[] = {RGB(255, 108, 118), RGB(95, 211, 135), RGB(101, 176, 255),
                                            RGB(208, 188, 255)};
std::wstring Monstey_CurveText(HWND window) {
    const auto count = GetWindowTextLengthW(window);
    std::wstring text(static_cast<std::size_t>(count) + 1, L'\0');
    GetWindowTextW(window, text.data(), count + 1);
    text.resize(static_cast<std::size_t>(count));
    return text;
}
std::wstring Monstey_CurveNumber(double value) { return Monstey_Wide(Monstey_Json(value).dump()); }
int Monstey_CurvePixelInt(double value) {
    return static_cast<int>(std::lround(std::clamp(value, -100000.0, 100000.0)));
}
struct Monstey_CurvePaint {
    HWND window;
    PAINTSTRUCT state{};
    HDC dc;
    explicit Monstey_CurvePaint(HWND w) : window(w), dc(BeginPaint(w, &state)) {}
    ~Monstey_CurvePaint() { EndPaint(window, &state); }
};
} // namespace
struct Monstey_CurveControl::Monstey_Impl {
    HWND window = nullptr, curveCombo = nullptr, channelCombo = nullptr, timeEdit = nullptr,
         valueEdit = nullptr, message = nullptr;
    HFONT font = nullptr;
    unsigned type;
    bool array = false, updating = false, dirty = false, dragging = false;
    std::vector<POINT> render_points;
    std::vector<Monstey_CurveDraft> curves;
    std::size_t curve = 0, selected = Monstey_CurveDraft::NoPoint;
    unsigned channel = 0;
    Monstey_CurveView view;
    explicit Monstey_Impl(unsigned t, const Monstey_Json &value) : type(t) {
        array = value.is_array();
        if (array) {
            Monstey_Require(!value.empty(), "No native curves to plot");
            for (const auto &v : value.as_array())
                curves.emplace_back(t, v);
        } else
            curves.emplace_back(t, value);
        view = Active().Fit();
        if (!Active().Points().empty())
            selected = 0;
    }
    Monstey_CurveDraft &Active() { return curves.at(curve); }
    const Monstey_CurveDraft &Active() const { return curves.at(curve); }
    RECT Plot() const {
        RECT rect{};
        GetClientRect(window, &rect);
        return {56, rect.right < 600 ? 108 : 78, std::max(57L, rect.right - 16),
                std::max(rect.right < 600 ? 109L : 79L, rect.bottom - 51)};
    }
    void Message(const std::wstring &text) {
        if (message)
            SetWindowTextW(message, text.c_str());
    }
    void Refresh() {
        updating = true;
        const bool valid = selected < Active().Points().size();
        EnableWindow(timeEdit, valid);
        EnableWindow(valueEdit, valid);
        EnableWindow(GetDlgItem(window, Monstey_CurveSet), valid);
        EnableWindow(GetDlgItem(window, Monstey_CurveDelete), Active().Points().size() > 1);
        EnableWindow(GetDlgItem(window, Monstey_CurveAdd), Active().Points().size() < 65536);
        if (valid) {
            const auto &p = Active().Points()[selected];
            SetWindowTextW(timeEdit, Monstey_CurveNumber(p.time).c_str());
            SetWindowTextW(valueEdit, Monstey_CurveNumber(p.value[channel]).c_str());
            Message(L"Point " + std::to_wstring(selected + 1) + L" / " +
                    std::to_wstring(Active().Points().size()) +
                    L". Drag a point or edit Time and Value. Double-click adds a point.");
        } else {
            SetWindowTextW(timeEdit, L"");
            SetWindowTextW(valueEdit, L"");
            Message(L"No points. Double-click in the graph or choose Add.");
        }
        dirty = false;
        updating = false;
        InvalidateRect(window, nullptr, FALSE);
    }
    void Commit() {
        if (!dirty || selected >= Active().Points().size())
            return;
        const auto time = Monstey_Json::Parse(Monstey_Utf8(Monstey_CurveText(timeEdit))),
                   value = Monstey_Json::Parse(Monstey_Utf8(Monstey_CurveText(valueEdit)));
        Monstey_Require(time.is_number() && value.is_number(), "Point Time and Value must be numeric");
        auto components = Active().Points()[selected].value;
        components[channel] = value.as_number();
        selected = Active().Set(selected, time.as_number(), components);
        dirty = false;
    }
    void Add(double time, double value, bool fit) {
        Commit();
        std::array<double, 4> components{};
        if (selected < Active().Points().size())
            components = Active().Points()[selected].value;
        else if (type == 12)
            components[3] = 255;
        components[channel] = value;
        selected = Active().Add(time, components);
        if (fit)
            view = Active().Fit();
        Refresh();
    }
    void Erase() {
        Commit();
        Active().Erase(selected);
        selected = std::min(selected, Active().Points().size() - 1);
        Refresh();
    }
    void Layout() {
        if (!window)
            return;
        RECT r{};
        GetClientRect(window, &r);
        const int width = std::max(300L, r.right), height = std::max(160L, r.bottom);
        MoveWindow(GetDlgItem(window, 809), 0, 2, 40, 24, TRUE);
        MoveWindow(curveCombo, 44, 0, 90, 180, TRUE);
        MoveWindow(GetDlgItem(window, 810), 145, 2, 55, 24, TRUE);
        MoveWindow(channelCombo, 202, 0, 94, 180, TRUE);
        const int buttonY = width < 600 ? 60 : 0, buttonX = width < 600 ? 0 : width - 218;
        MoveWindow(GetDlgItem(window, Monstey_CurveAdd), buttonX, buttonY, 64, 25, TRUE);
        MoveWindow(GetDlgItem(window, Monstey_CurveDelete), buttonX + 70, buttonY, 70, 25, TRUE);
        MoveWindow(GetDlgItem(window, Monstey_CurveFit), buttonX + 146, buttonY, 70, 25, TRUE);
        const int componentWidth = (width - 160) / 2;
        MoveWindow(GetDlgItem(window, 811), 0, 33, 32, 24, TRUE);
        MoveWindow(timeEdit, 34, 30, componentWidth, 25, TRUE);
        MoveWindow(GetDlgItem(window, 812), 44 + componentWidth, 33, 42, 24, TRUE);
        MoveWindow(valueEdit, 88 + componentWidth, 30, componentWidth, 25, TRUE);
        MoveWindow(GetDlgItem(window, Monstey_CurveSet), width - 70, 30, 70, 25, TRUE);
        MoveWindow(message, 0, height - 25, width, 25, TRUE);
        InvalidateRect(window, nullptr, TRUE);
    }
    void Paint(HDC target) {
        RECT r{};
        GetClientRect(window, &r);
        if (r.right <= 0 || r.bottom <= 0)
            return;
        const auto memory = CreateCompatibleDC(target);
        const auto bitmap = memory ? CreateCompatibleBitmap(target, r.right, r.bottom) : nullptr;
        if (!memory || !bitmap) {
            if (bitmap)
                DeleteObject(bitmap);
            if (memory)
                DeleteDC(memory);
            return;
        }
        const auto oldBitmap = SelectObject(memory, bitmap);
        const auto saved = SaveDC(memory);
        try {
            FillRect(memory, &r, Monstey_ThemeBackgroundBrush());
            SelectObject(memory, font);
            SetBkMode(memory, TRANSPARENT);
            SetTextColor(memory, Monstey_ThemeMuted);
            const auto plot = Plot();
            const auto width = plot.right - plot.left, height = plot.bottom - plot.top;
            const auto grid = CreatePen(PS_SOLID, 1, Monstey_ThemeBorder);
            const auto oldPen = SelectObject(memory, grid);
            for (unsigned k = 0; k <= 4; ++k) {
                const int x = plot.left + static_cast<int>(k) * width / 4,
                          y = plot.top + static_cast<int>(k) * height / 4;
                MoveToEx(memory, x, plot.top, nullptr);
                LineTo(memory, x, plot.bottom);
                MoveToEx(memory, plot.left, y, nullptr);
                LineTo(memory, plot.right, y);
                wchar_t text[64]{};
                swprintf_s(text, L"%.3g", view.time_min + (view.time_max - view.time_min) * k / 4);
                TextOutW(memory, x - 10, plot.bottom + 5, text, static_cast<int>(wcslen(text)));
                swprintf_s(text, L"%.3g", view.value_max - (view.value_max - view.value_min) * k / 4);
                TextOutW(memory, 0, y - 7, text, static_cast<int>(wcslen(text)));
            }
            SelectObject(memory, oldPen);
            DeleteObject(grid);
            const auto clip = SaveDC(memory);
            IntersectClipRect(memory, plot.left - 6, plot.top - 6, plot.right + 7, plot.bottom + 7);
            auto &points = render_points;
            points.reserve(Active().Points().size());
            for (unsigned c = 0; c < Active().Components(); ++c) {
                points.clear();
                for (std::size_t i = 0; i < Active().Points().size(); ++i) {
                    const auto p = Active().Project(i, c, view, width, height);
                    points.push_back(
                        {plot.left + Monstey_CurvePixelInt(p.x), plot.top + Monstey_CurvePixelInt(p.y)});
                }
                const auto base = Active().Components() == 1 ? RGB(101, 176, 255) : Monstey_CurveColors[c];
                const auto color = c == channel
                                       ? base
                                       : RGB((GetRValue(base) + (Monstey_ThemeBackground & 0xffU)) / 2, (GetGValue(base) + ((Monstey_ThemeBackground >> 8) & 0xffU)) / 2,
                                             (GetBValue(base) + ((Monstey_ThemeBackground >> 16) & 0xffU)) / 2);
                const auto pen = CreatePen(PS_SOLID, c == channel ? 2 : 1, color);
                const auto previous = SelectObject(memory, pen);
                if (points.size() > 1)
                    Polyline(memory, points.data(), static_cast<int>(points.size()));
                if (c == channel) {
                    const auto brush = CreateSolidBrush(color);
                    const auto oldBrush = SelectObject(memory, brush);
                    const auto step = std::max<std::size_t>(1, points.size() / 2000);
                    for (std::size_t i = 0; i < points.size(); i += step) {
                        const auto &p = points[i];
                        Ellipse(memory, p.x - 3, p.y - 3, p.x + 4, p.y + 4);
                    }
                    if (selected < points.size()) {
                        const auto &p = points[selected];
                        Ellipse(memory, p.x - 6, p.y - 6, p.x + 7, p.y + 7);
                    }
                    SelectObject(memory, oldBrush);
                    DeleteObject(brush);
                }
                SelectObject(memory, previous);
                DeleteObject(pen);
            }
            RestoreDC(memory, clip);
            if (GetFocus() == window)
                DrawFocusRect(memory, &plot);
            RestoreDC(memory, saved);
            BitBlt(target, 0, 0, r.right, r.bottom, memory, 0, 0, SRCCOPY);
        } catch (...) {
            RestoreDC(memory, saved);
            SelectObject(memory, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(memory);
            throw;
        }
        SelectObject(memory, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(memory);
    }
    static LRESULT CALLBACK Procedure(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
        auto *self = reinterpret_cast<Monstey_Impl *>(GetWindowLongPtrW(w, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            self = static_cast<Monstey_Impl *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
            self->window = w;
            SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self)
            return DefWindowProcW(w, msg, wp, lp);
        try {
            switch (msg) {
            case WM_SIZE:
                self->Layout();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PRINTCLIENT:
                self->Paint(reinterpret_cast<HDC>(wp));
                return 0;
            case WM_PAINT: {
                Monstey_CurvePaint paint(w);
                self->Paint(paint.dc);
                return 0;
            }
            case WM_SETFOCUS:
            case WM_KILLFOCUS:
                InvalidateRect(w, nullptr, FALSE);
                return 0;
            case WM_COMMAND: {
                const auto id = LOWORD(wp), code = HIWORD(wp);
                if ((id == Monstey_CurveTime || id == Monstey_CurveValue) && code == EN_CHANGE &&
                    !self->updating) {
                    self->dirty = true;
                    return 0;
                }
                if ((id == Monstey_CurveSelector || id == Monstey_CurveChannel) && code == CBN_SELCHANGE) {
                    self->Commit();
                    if (id == Monstey_CurveSelector) {
                        const auto index = SendMessageW(self->curveCombo, CB_GETCURSEL, 0, 0);
                        Monstey_Require(index >= 0 && static_cast<std::size_t>(index) < self->curves.size(),
                                        "Select a curve");
                        self->curve = static_cast<std::size_t>(index);
                        self->selected = self->Active().Points().empty() ? Monstey_CurveDraft::NoPoint : 0;
                        self->view = self->Active().Fit();
                    } else {
                        const auto index = SendMessageW(self->channelCombo, CB_GETCURSEL, 0, 0);
                        Monstey_Require(index >= 0 &&
                                            static_cast<unsigned>(index) < self->Active().Components(),
                                        "Select a component");
                        self->channel = static_cast<unsigned>(index);
                    }
                    self->Refresh();
                    return 0;
                }
                if (id == Monstey_CurveSet) {
                    self->Commit();
                    self->Refresh();
                    return 0;
                }
                if (id == Monstey_CurveAdd) {
                    self->Commit();
                    const auto &pts = self->Active().Points();
                    const auto time =
                        pts.empty() ? 0
                                    : pts.back().time +
                                          std::max(.01, (self->view.time_max - self->view.time_min) / 4);
                    const auto value =
                        self->selected < pts.size() ? pts[self->selected].value[self->channel] : 0;
                    self->Add(time, value, true);
                    return 0;
                }
                if (id == Monstey_CurveDelete) {
                    self->Erase();
                    return 0;
                }
                if (id == Monstey_CurveFit) {
                    self->Commit();
                    self->view = self->Active().Fit();
                    self->Refresh();
                    return 0;
                }
                break;
            }
            case WM_LBUTTONDOWN: {
                self->Commit();
                SetFocus(w);
                const auto r = self->Plot();
                const auto x = GET_X_LPARAM(lp) - r.left, y = GET_Y_LPARAM(lp) - r.top;
                if (x < 0 || y < 0 || x > r.right - r.left || y > r.bottom - r.top)
                    return 0;
                const auto i =
                    self->Active().Hit(x, y, self->channel, self->view, r.right - r.left, r.bottom - r.top);
                if (i != Monstey_CurveDraft::NoPoint) {
                    self->selected = i;
                    self->dragging = true;
                    SetCapture(w);
                    self->Refresh();
                }
                return 0;
            }
            case WM_LBUTTONDBLCLK: {
                self->Commit();
                const auto r = self->Plot();
                const auto x = GET_X_LPARAM(lp) - r.left, y = GET_Y_LPARAM(lp) - r.top;
                if (x >= 0 && y >= 0 && x <= r.right - r.left && y <= r.bottom - r.top)
                    self->Add(self->Active().PixelTime(x, self->view, r.right - r.left),
                              self->Active().PixelValue(y, self->view, r.bottom - r.top), false);
                return 0;
            }
            case WM_MOUSEMOVE:
                if (self->dragging && (wp & MK_LBUTTON)) {
                    const auto r = self->Plot();
                    self->selected = self->Active().Drag(self->selected, self->channel,
                                                         GET_X_LPARAM(lp) - r.left, GET_Y_LPARAM(lp) - r.top,
                                                         self->view, r.right - r.left, r.bottom - r.top);
                    self->Refresh();
                }
                return 0;
            case WM_LBUTTONUP:
                if (self->dragging) {
                    self->dragging = false;
                    if (GetCapture() == w)
                        ReleaseCapture();
                    self->Refresh();
                }
                return 0;
            case WM_CAPTURECHANGED:
                self->dragging = false;
                return 0;
            case WM_KEYDOWN:
                if (wp == VK_DELETE) {
                    self->Erase();
                    return 0;
                }
                if (!self->Active().Points().empty() &&
                    (wp == VK_LEFT || wp == VK_RIGHT || wp == VK_HOME || wp == VK_END)) {
                    self->Commit();
                    if (wp == VK_HOME)
                        self->selected = 0;
                    else if (wp == VK_END)
                        self->selected = self->Active().Points().size() - 1;
                    else if (wp == VK_LEFT)
                        self->selected = self->selected == 0 ? 0 : self->selected - 1;
                    else
                        self->selected = std::min(self->selected + 1, self->Active().Points().size() - 1);
                    self->Refresh();
                    return 0;
                }
                break;
            case WM_NCDESTROY:
                self->window = nullptr;
                self->curveCombo = self->channelCombo = self->timeEdit = self->valueEdit = self->message =
                    nullptr;
                self->dirty = false;
                self->dragging = false;
                SetWindowLongPtrW(w, GWLP_USERDATA, 0);
                return 0;
            default:
                break;
            }
        } catch (const std::exception &e) {
            self->Message(Monstey_Wide(e.what()));
            if (self->curveCombo)
                SendMessageW(self->curveCombo, CB_SETCURSEL, self->curve, 0);
            if (self->channelCombo)
                SendMessageW(self->channelCombo, CB_SETCURSEL, self->channel, 0);
            if (self->dragging) {
                self->dragging = false;
                if (GetCapture() == w)
                    ReleaseCapture();
            }
            return 0;
        }
        return DefWindowProcW(w, msg, wp, lp);
    }
};
Monstey_CurveControl::Monstey_CurveControl(unsigned type, const Monstey_Json &value)
    : impl_(std::make_unique<Monstey_Impl>(type, value)) {}
Monstey_CurveControl::~Monstey_CurveControl() {
    if (impl_->window)
        DestroyWindow(impl_->window);
}
void Monstey_CurveControl::Register(HINSTANCE instance) {
    WNDCLASSEXW c{};
    c.cbSize = sizeof(c);
    c.hInstance = instance;
    c.lpfnWndProc = Monstey_Impl::Procedure;
    c.lpszClassName = Monstey_CurveClass;
    c.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    c.style = CS_DBLCLKS;
    c.hbrBackground = Monstey_ThemeBackgroundBrush();
    Monstey_Require(RegisterClassExW(&c) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS,
                    "Cannot register native curve control");
}
void Monstey_CurveControl::Create(HWND parent, HFONT font) {
    Monstey_Require(!impl_->window, "Curve control already created");
    impl_->font = font;
    const auto instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE));
    impl_->window =
        CreateWindowExW(WS_EX_CLIENTEDGE | WS_EX_CONTROLPARENT, Monstey_CurveClass,
                        L"Native FX control points", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN, 0,
                        0, 100, 100, parent, nullptr, instance, impl_.get());
    Monstey_Require(impl_->window != nullptr, "Cannot create native curve plot");
    auto control = [&](const wchar_t *kind, const wchar_t *text, DWORD style, int id) {
        const auto w = CreateWindowExW(std::wstring_view(kind) == L"EDIT" ? WS_EX_CLIENTEDGE : 0, kind, text,
                                       WS_CHILD | WS_VISIBLE | style, 0, 0, 100, 24, impl_->window,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
        Monstey_Require(w != nullptr, "Cannot create curve editor control");
        SendMessageW(w, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return w;
    };
    control(L"STATIC", L"Curve", 0, 809);
    impl_->curveCombo =
        control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, Monstey_CurveSelector);
    for (std::size_t i = 0; i < impl_->curves.size(); ++i) {
        const auto name = std::to_wstring(i + 1);
        SendMessageW(impl_->curveCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
    }
    SendMessageW(impl_->curveCombo, CB_SETCURSEL, 0, 0);
    EnableWindow(impl_->curveCombo, impl_->curves.size() > 1);
    control(L"STATIC", L"Channel", 0, 810);
    impl_->channelCombo =
        control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, Monstey_CurveChannel);
    for (unsigned c = 0; c < impl_->Active().Components(); ++c) {
        const wchar_t *name = impl_->type == 12   ? (c == 0   ? L"Red"
                                                     : c == 1 ? L"Green"
                                                     : c == 2 ? L"Blue"
                                                              : L"Alpha")
                              : impl_->type == 16 ? (c == 0   ? L"X"
                                                     : c == 1 ? L"Y"
                                                              : L"Z")
                                                  : L"Value";
        SendMessageW(impl_->channelCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
    }
    SendMessageW(impl_->channelCombo, CB_SETCURSEL, 0, 0);
    control(L"STATIC", L"Time", 0, 811);
    impl_->timeEdit = control(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, Monstey_CurveTime);
    control(L"STATIC", L"Value", 0, 812);
    impl_->valueEdit = control(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, Monstey_CurveValue);
    control(L"BUTTON", L"Set point", WS_TABSTOP, Monstey_CurveSet);
    control(L"BUTTON", L"Add", WS_TABSTOP, Monstey_CurveAdd);
    control(L"BUTTON", L"Delete", WS_TABSTOP, Monstey_CurveDelete);
    control(L"BUTTON", L"Fit view", WS_TABSTOP, Monstey_CurveFit);
    impl_->message = control(L"STATIC", L"", SS_LEFT, 813);
    Monstey_ThemeApply(impl_->window);
    impl_->Layout();
    impl_->Refresh();
}
void Monstey_CurveControl::Layout(int x, int y, int width, int height) {
    MoveWindow(impl_->window, x, y, std::max(1, width), std::max(1, height), TRUE);
}
HWND Monstey_CurveControl::Window() const noexcept { return impl_->window; }
RECT Monstey_CurveControl::PlotBounds() const { return impl_->Plot(); }
Monstey_Json Monstey_CurveControl::Value() {
    impl_->Commit();
    for (const auto &curve : impl_->curves)
        Monstey_Require(!curve.Points().empty(), "Add at least one point to each native curve");
    if (!impl_->array)
        return impl_->curves[0].Value();
    auto result = Monstey_Json::array();
    for (const auto &curve : impl_->curves)
        result.as_array().push_back(curve.Value());
    return result;
}
} // namespace Monstey
