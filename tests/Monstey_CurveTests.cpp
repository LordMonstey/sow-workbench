#include "Monstey_Curve.hpp"
#include "Monstey_Gadb.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
J Monstey_CurveKnots(std::initializer_list<std::pair<double, J>> values) {
    auto a = J::array();
    for (const auto &[time, value] : values)
        a.as_array().push_back(J::object({{"time", time}, {"value", value}}));
    return J::object({{"knots", a}});
}
J Monstey_CurveField(const J &m, std::string_view name) {
    for (const auto &f : m["fields"].as_array())
        if (f["name"].as_string() == name)
            return f["value"];
    throw Monstey_Error("test", "Curve field missing");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        const std::filesystem::path dir = argc > 1 ? argv[1] : L"curve-test-output";
        std::filesystem::create_directories(dir);
        unsigned passed = 0;
        std::set<std::string> names;
        J report = J::object({{"cases", J::array()}, {"python_runtime", false}});
        auto test = [&](const std::string &name, const std::function<void()> &fn) {
            Monstey_Require(names.insert(name).second, "Duplicate test name");
            try {
                fn();
            } catch (const std::exception &e) {
                throw Monstey_Error("test", name + ": " + e.what());
            }
            report["cases"].as_array().push_back(name);
            ++passed;
        };
        auto reject = [](const std::function<void()> &fn) {
            bool failed = false;
            try {
                fn();
            } catch (const std::exception &) {
                failed = true;
            }
            Monstey_Require(failed, "Invalid curve edit accepted");
        };
        test("color_argb_bytes_roundtrip_without_gamma_or_alpha_changes", [&] {
            Monstey_CurveDraft c(12, Monstey_CurveKnots({{0, J(0xff112233u)}, {1, J(0x0512fe01u)}}));
            Monstey_Require(c.Components() == 4 &&
                                c.Points()[0].value == std::array<double, 4>{17, 34, 51, 255} &&
                                c.Value()["knots"][1u]["value"].as_uint() == 0x0512fe01u,
                            "ARGB packing changed");
        });
        test("float_and_time_are_quantized_to_native_float32", [&] {
            Monstey_CurveDraft c(13, Monstey_CurveKnots({{.1234567890123, J(1.234567890123)}}));
            Monstey_Require(c.Points()[0].time == static_cast<double>(static_cast<float>(.1234567890123)) &&
                                c.Points()[0].value[0] ==
                                    static_cast<double>(static_cast<float>(1.234567890123)),
                            "Native Float32 projection differs");
        });
        test("int32_limits_are_exact_and_fractional_source_is_rejected", [&] {
            Monstey_CurveDraft c(15, Monstey_CurveKnots({{0, J(-2147483648ll)}, {1, J(2147483647)}}));
            Monstey_Require(c.Value()["knots"][0u]["value"].as_int() == -2147483648ll,
                            "int32 minimum changed");
            reject([&] { Monstey_CurveDraft bad(15, Monstey_CurveKnots({{0, J(1.5)}})); });
            reject([&] { c.Set(0, 0, {2147483648.0, 0, 0, 0}); });
        });
        test("vector_drag_changes_one_component_and_preserves_other_axes", [&] {
            Monstey_CurveDraft c(16,
                                 Monstey_CurveKnots({{0, J::array({1, 2, 3})}, {1, J::array({4, 5, 6})}}));
            const Monstey_CurveView v{0, 2, 0, 10};
            const auto i = c.Drag(0, 1, 50, 20, v, 100, 100);
            Monstey_Require(c.Points()[i].value == std::array<double, 4>{1, 8, 3, 0},
                            "Vector drag overwrote another axis");
        });
        test("point_time_reordering_is_sorted_and_duplicate_order_is_stable", [&] {
            Monstey_CurveDraft c(13, Monstey_CurveKnots({{0, J(1)}, {1, J(2)}, {1, J(3)}, {2, J(4)}}));
            const auto a = c.Set(1, 1, {9, 0, 0, 0});
            Monstey_Require(a == 1 && c.Points()[2].value[0] == 3, "Same-time edit reordered native ties");
            const auto b = c.Set(0, 1.5, {1, 0, 0, 0});
            Monstey_Require(b == 2 && c.Points()[2].time == 1.5, "Crossing edit did not sort knots");
        });
        test("failed_edits_are_transactional", [&] {
            Monstey_CurveDraft c(13, Monstey_CurveKnots({{0, J(1)}, {1, J(2)}}));
            const auto original = c.Value();
            reject([&] { c.Set(0, -1, {3, 0, 0, 0}); });
            reject([&] { c.Set(0, 0, {std::numeric_limits<double>::infinity(), 0, 0, 0}); });
            reject([&] { c.Add(std::numeric_limits<double>::quiet_NaN(), {0, 0, 0, 0}); });
            Monstey_Require(c.Value() == original, "Failed edit changed draft");
        });
        test("add_delete_and_empty_source_authoring_retain_valid_minimum", [&] {
            Monstey_CurveDraft c(12, J::object({{"knots", J::array()}}));
            Monstey_Require(c.Fit().value_max == 255, "Empty color view has wrong scale");
            c.Add(0, {255, 128, 0, 255});
            reject([&] { c.Erase(0); });
            auto i = c.Add(.5, {0, 0, 0, 255});
            c.Erase(i);
            Monstey_Require(c.Points().size() == 1, "Point delete cardinality differs");
        });
        test("viewport_projection_hit_and_inverse_coordinates_agree", [&] {
            Monstey_CurveDraft c(13, Monstey_CurveKnots({{.5, J(25)}, {1, J(75)}}));
            const Monstey_CurveView v{0, 2, 0, 100};
            auto p = c.Project(0, 0, v, 400, 200);
            Monstey_Require(p.x == 100 && p.y == 150 && c.Hit(102, 149, 0, v, 400, 200) == 0 &&
                                c.Hit(399, 1, 0, v, 400, 200) == Monstey_CurveDraft::NoPoint &&
                                c.PixelTime(100, v, 400) == .5 && c.PixelValue(150, v, 200) == 25,
                            "Curve hit/projection differs");
            reject([&] { c.Hit(0, 0, 2, v, 400, 200); });
            reject([&] { c.Project(0, 0, {0, 0, 0, 1}, 400, 200); });
        });
        test("drag_clamps_to_native_integer_color_and_time_domains", [&] {
            Monstey_CurveDraft c(15, Monstey_CurveKnots({{0, J(1)}, {1, J(2)}}));
            auto i = c.Drag(0, 0, -100, 40, {0, 1, 0, 10}, 100, 100);
            Monstey_Require(c.Points()[i].time == 0 && c.Points()[i].value[0] == 6,
                            "Integer drag was not rounded/clamped");
            Monstey_CurveDraft color(12, Monstey_CurveKnots({{0, J(0xff000000u)}}));
            color.Drag(0, 0, 0, -50, {0, 1, -100, 500}, 100, 100);
            Monstey_Require(color.Points()[0].value[0] == 255, "Color drag escaped byte range");
        });
        test("fit_is_finite_for_constant_extreme_and_negative_values", [&] {
            for (double x : {0., -5., static_cast<double>(std::numeric_limits<float>::max())}) {
                Monstey_CurveDraft c(13, Monstey_CurveKnots({{0, J(x)}}));
                const auto v = c.Fit();
                Monstey_Require(std::isfinite(v.value_min) && std::isfinite(v.value_max) &&
                                    v.value_max > v.value_min,
                                "Degenerate constant viewport");
            }
        });
        test("native_knot_limit_is_enforced_without_partial_insert", [&] {
            auto knots = J::array();
            for (unsigned i = 0; i < 65536; ++i)
                knots.as_array().push_back(J::object({{"time", static_cast<double>(i)}, {"value", 0}}));
            Monstey_CurveDraft c(13, J::object({{"knots", knots}}));
            reject([&] { c.Add(65536, {0, 0, 0, 0}); });
            Monstey_Require(c.Points().size() == 65536, "Knot limit edit changed data");
        });
        if (argc > 3)
            test("graphical_drafts_compile_and_read_back_all_four_native_curve_kinds", [&] {
                Monstey_Gadb fx(argv[2], argv[3]);
                const std::pair<const char *, unsigned> fields[] = {
                    {"ColorCurve", 12}, {"FloatCurve", 13}, {"IntCurve", 15}, {"VectorCurve", 16}};
                std::map<std::string, J> expected;
                for (const auto &[name, type] : fields) {
                    Monstey_CurveDraft c(type, Monstey_CurveField(fx.Metadata("Mixed:Alpha"), name));
                    c.Add(.5, type == 12   ? std::array<double, 4>{17, 34, 51, 255}
                              : type == 16 ? std::array<double, 4>{7, 8, 9, 0}
                                           : std::array<double, 4>{123, 0, 0, 0});
                    expected[name] = c.Value();
                    fx.SetField("Mixed:Alpha", name, c.Value());
                }
                const auto out = dir / L"graphical-curves.fxp";
                if (std::filesystem::exists(out))
                    std::filesystem::remove(out);
                fx.Build(out);
                Monstey_Gadb check(out, argv[3]);
                for (const auto &[name, value] : expected)
                    Monstey_Require(Monstey_CurveField(check.Metadata("Mixed:Alpha"), name) == value,
                                    "Graphical draft native compile/readback differs");
            });
        report["status"] = "PASS";
        report["passed"] = passed;
        const auto out = dir / L"curve-tests.json";
        if (std::filesystem::exists(out))
            std::filesystem::remove(out);
        Monstey_WriteNewText(out, report.dump(2) + "\n");
        std::cout << "Native curve tests: " << passed << " passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Native curve failure: " << e.what() << "\n";
        return 1;
    }
}
