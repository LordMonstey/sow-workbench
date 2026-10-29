#include "Monstey_CurveControl.hpp"
#include "Monstey_Gadb.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
J Monstey_GraphFixture(unsigned kind) {
    const J a = kind == 12   ? J(0xff112233u)
                : kind == 16 ? J::array({1, 2, 3})
                             : J(0),
            b = kind == 12   ? J(0xff445566u)
                : kind == 16 ? J::array({4, 5, 6})
                             : J(10);
    return J::object({{"knots", J::array({J::object({{"time", 0}, {"value", a}}),
                                          J::object({{"time", 1}, {"value", b}})})}});
}
struct Monstey_GraphHost {
    HWND window;
    Monstey_GraphHost() {
        window = CreateWindowExW(0, L"STATIC", L"Native curve test", WS_OVERLAPPEDWINDOW, 0, 0, 1000, 700,
                                 nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        Monstey_Require(window != nullptr, "Cannot create native test host");
    }
    ~Monstey_GraphHost() {
        if (window)
            DestroyWindow(window);
    }
};
void Monstey_GraphCommand(Monstey_CurveControl &c, int id, int code = BN_CLICKED) {
    SendMessageW(c.Window(), WM_COMMAND, MAKEWPARAM(id, code),
                 reinterpret_cast<LPARAM>(GetDlgItem(c.Window(), id)));
}
void Monstey_GraphText(Monstey_CurveControl &c, int id, const wchar_t *text) {
    SetWindowTextW(GetDlgItem(c.Window(), id), text);
}
void Monstey_GraphCapture(Monstey_CurveControl &control, const std::filesystem::path &path) {
    RECT r{};
    GetClientRect(control.Window(), &r);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = r.right;
    info.bmiHeader.biHeight = -r.bottom;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *pixels = nullptr;
    const auto dc = CreateCompatibleDC(nullptr);
    const auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    Monstey_Require(dc && bitmap && pixels, "Cannot capture native curve renderer");
    const auto old = SelectObject(dc, bitmap);
    SendMessageW(control.Window(), WM_PRINT, reinterpret_cast<WPARAM>(dc),
                 PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    const auto bytes = static_cast<std::size_t>(r.right) * r.bottom * 4;
    const auto p = static_cast<const std::uint8_t *>(pixels);
    std::size_t colored = 0;
    for (std::size_t i = 0; i < bytes; i += 4)
        if (p[i] != p[i + 1] || p[i + 1] != p[i + 2])
            ++colored;
    Monstey_Require(colored > 20, "Curve renderer did not draw colored data");
    BITMAPFILEHEADER header{};
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(info.bmiHeader);
    header.bfSize = static_cast<DWORD>(header.bfOffBits + bytes);
    Monstey_Bytes out;
    const auto append = [&](const void *data, std::size_t n) {
        const auto first = static_cast<const std::uint8_t *>(data);
        out.insert(out.end(), first, first + n);
    };
    append(&header, sizeof(header));
    append(&info.bmiHeader, sizeof(info.bmiHeader));
    append(pixels, bytes);
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    if (std::filesystem::exists(path))
        std::filesystem::remove(path);
    Monstey_WriteNewFile(path, out);
}
J Monstey_GraphField(const J &meta, std::string_view name) {
    for (const auto &f : meta["fields"].as_array())
        if (f["name"].as_string() == name)
            return f["value"];
    throw Monstey_Error("test", "Native curve field missing");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        const std::filesystem::path dir = argc > 1 ? argv[1] : L"curve-gui-output";
        std::filesystem::create_directories(dir);
        Monstey_CurveControl::Register(GetModuleHandleW(nullptr));
        const auto font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        unsigned passed = 0;
        J report =
            J::object({{"cases", J::array()}, {"native_win32_control", true}, {"python_runtime", false}});
        auto test = [&](const std::string &name, const std::function<void()> &fn) {
            try {
                fn();
            } catch (const std::exception &e) {
                throw Monstey_Error("test", name + ": " + e.what());
            }
            report["cases"].as_array().push_back(name);
            ++passed;
        };
        test("real_mouse_messages_drag_a_float_knot", [&] {
            Monstey_GraphHost host;
            Monstey_CurveControl control(13, Monstey_GraphFixture(13));
            control.Create(host.window, font);
            control.Layout(0, 0, 900, 500);
            const auto r = control.PlotBounds();
            Monstey_CurveDraft model(13, Monstey_GraphFixture(13));
            const auto view = model.Fit();
            const auto point = model.Project(0, 0, view, r.right - r.left, r.bottom - r.top);
            SendMessageW(control.Window(), WM_LBUTTONDOWN, MK_LBUTTON,
                         MAKELPARAM(r.left + static_cast<int>(std::lround(point.x)),
                                    r.top + static_cast<int>(std::lround(point.y))));
            const auto x = r.left + static_cast<int>(std::lround(.5 / view.time_max * (r.right - r.left))),
                       y = r.top + static_cast<int>(
                                       std::lround((view.value_max - 5) / (view.value_max - view.value_min) *
                                                   (r.bottom - r.top)));
            SendMessageW(control.Window(), WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, y));
            SendMessageW(control.Window(), WM_LBUTTONUP, 0, MAKELPARAM(x, y));
            const auto value = control.Value();
            Monstey_Require(std::abs(value["knots"][0u]["time"].as_number() - .5) < .01 &&
                                std::abs(value["knots"][0u]["value"].as_number() - 5) < .1,
                            "Mouse drag did not edit actual curve state");
            Monstey_GraphCapture(control, dir / L"float-curve.bmp");
        });
        test("numeric_vector_edit_changes_selected_component_only", [&] {
            Monstey_GraphHost host;
            Monstey_CurveControl control(16, Monstey_GraphFixture(16));
            control.Create(host.window, font);
            control.Layout(0, 0, 700, 420);
            SendMessageW(GetDlgItem(control.Window(), Monstey_CurveChannel), CB_SETCURSEL, 1, 0);
            Monstey_GraphCommand(control, Monstey_CurveChannel, CBN_SELCHANGE);
            Monstey_GraphText(control, Monstey_CurveTime, L"0.25");
            Monstey_GraphText(control, Monstey_CurveValue, L"8");
            Monstey_GraphCommand(control, Monstey_CurveSet);
            const auto v = control.Value()["knots"][0u]["value"];
            Monstey_Require(v[0u].as_number() == 1 && v[1u].as_number() == 8 && v[2u].as_number() == 3,
                            "Vector numeric edit changed other components");
            Monstey_GraphCapture(control, dir / L"vector-curve.bmp");
        });
        test("color_alpha_and_rgb_are_edited_as_native_bytes", [&] {
            Monstey_GraphHost host;
            Monstey_CurveControl control(12, Monstey_GraphFixture(12));
            control.Create(host.window, font);
            control.Layout(0, 0, 700, 420);
            SendMessageW(GetDlgItem(control.Window(), Monstey_CurveChannel), CB_SETCURSEL, 1, 0);
            Monstey_GraphCommand(control, Monstey_CurveChannel, CBN_SELCHANGE);
            Monstey_GraphText(control, Monstey_CurveValue, L"173");
            Monstey_GraphCommand(control, Monstey_CurveSet);
            Monstey_Require(control.Value()["knots"][0u]["value"].as_uint() == 0xff11ad33u,
                            "ARGB byte order changed");
            Monstey_GraphCapture(control, dir / L"color-curve.bmp");
        });
        test("add_delete_double_click_and_keyboard_preserve_minimum", [&] {
            Monstey_GraphHost host;
            Monstey_CurveControl control(15, Monstey_GraphFixture(15));
            control.Create(host.window, font);
            control.Layout(0, 0, 700, 420);
            Monstey_GraphCommand(control, Monstey_CurveAdd);
            Monstey_Require(control.Value()["knots"].size() == 3, "Add button failed");
            Monstey_GraphCommand(control, Monstey_CurveDelete);
            Monstey_Require(control.Value()["knots"].size() == 2, "Delete button failed");
            const auto r = control.PlotBounds();
            SendMessageW(control.Window(), WM_LBUTTONDBLCLK, MK_LBUTTON,
                         MAKELPARAM((r.left + r.right) / 2, (r.top + r.bottom) / 2));
            Monstey_Require(control.Value()["knots"].size() == 3, "Double-click insertion failed");
            SendMessageW(control.Window(), WM_KEYDOWN, VK_DELETE, 0);
            Monstey_Require(control.Value()["knots"].size() == 2, "Delete key failed");
        });
        test("curve_array_selector_preserves_unselected_native_curves", [&] {
            Monstey_GraphHost host;
            const auto source = Monstey_GraphFixture(13);
            Monstey_CurveControl control(13, J::array({source, source}));
            control.Create(host.window, font);
            control.Layout(0, 0, 700, 420);
            SendMessageW(GetDlgItem(control.Window(), Monstey_CurveSelector), CB_SETCURSEL, 1, 0);
            Monstey_GraphCommand(control, Monstey_CurveSelector, CBN_SELCHANGE);
            Monstey_GraphText(control, Monstey_CurveValue, L"99");
            Monstey_GraphCommand(control, Monstey_CurveSet);
            auto value = control.Value();
            Monstey_CurveDraft initial(13, source);
            Monstey_Require(value[0u] == initial.Value() && value[1u]["knots"][0u]["value"].as_number() == 99,
                            "Editing one curve changed another array entry");
        });
        test("invalid_numeric_text_does_not_commit_or_poison_the_draft", [&] {
            Monstey_GraphHost host;
            Monstey_CurveControl control(13, Monstey_GraphFixture(13));
            control.Create(host.window, font);
            control.Layout(0, 0, 700, 420);
            const auto original = control.Value();
            Monstey_GraphText(control, Monstey_CurveValue, L"invalid");
            bool rejected = false;
            try {
                control.Value();
            } catch (const std::exception &) {
                rejected = true;
            }
            Monstey_Require(rejected, "Invalid numeric edit accepted");
            Monstey_GraphText(control, Monstey_CurveValue, L"0");
            Monstey_Require(control.Value() == original, "Rejected edit poisoned curve data");
        });
        test("parent_teardown_releases_child_handles_capture_and_gdi_objects", [&] {
            const auto before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            for (unsigned k = 0; k < 8; ++k) {
                Monstey_GraphHost host;
                Monstey_CurveControl control(13, Monstey_GraphFixture(13));
                control.Create(host.window, font);
                control.Layout(0, 0, 700, 420);
                DestroyWindow(host.window);
                host.window = nullptr;
                Monstey_Require(control.Window() == nullptr, "Curve control kept a stale native HWND");
            }
            const auto after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            Monstey_Require(after <= before + 2, "Curve control leaked GDI objects");
        });
        if (argc > 3)
            test("actual_graphical_control_values_compile_into_native_fx", [&] {
                Monstey_Gadb fx(argv[2], argv[3]);
                Monstey_GraphHost host;
                const std::pair<const char *, unsigned> fields[] = {
                    {"ColorCurve", 12}, {"FloatCurve", 13}, {"IntCurve", 15}, {"VectorCurve", 16}};
                std::map<std::string, J> expected;
                for (const auto &[name, type] : fields) {
                    Monstey_CurveControl control(type, Monstey_GraphField(fx.Metadata("Mixed:Alpha"), name));
                    control.Create(host.window, font);
                    control.Layout(0, 0, 700, 420);
                    Monstey_GraphCommand(control, Monstey_CurveAdd);
                    expected[name] = control.Value();
                    fx.SetField("Mixed:Alpha", name, expected[name]);
                }
                const auto out = dir / L"interactive-curves.fxp";
                if (std::filesystem::exists(out))
                    std::filesystem::remove(out);
                fx.Build(out);
                Monstey_Gadb check(out, argv[3]);
                for (const auto &[name, value] : expected)
                    Monstey_Require(Monstey_GraphField(check.Metadata("Mixed:Alpha"), name) == value,
                                    "Graphical control lost native curve data");
            });
        report["status"] = "PASS";
        report["passed"] = passed;
        const auto out = dir / L"curve-gui-tests.json";
        if (std::filesystem::exists(out))
            std::filesystem::remove(out);
        Monstey_WriteNewText(out, report.dump(2) + "\n");
        std::cout << "Native curve GUI tests: " << passed << " passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Native curve GUI failure: " << e.what() << "\n";
        return 1;
    }
}
