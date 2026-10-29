#include "Monstey_Runtime.hpp"
#include <functional>
#include <iostream>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
std::string Monstey_LegacyPath(const std::filesystem::path &p) {
    return Monstey_Utf8(std::filesystem::absolute(p).wstring());
}
void Monstey_LegacyFresh(const std::filesystem::path &p) {
    if (std::filesystem::exists(p))
        std::filesystem::remove(p);
}
J Monstey_LegacyCall(Monstey_Runtime &r, const char *operation, const J &args) {
    return r.Call(J::object({{"operation", operation}, {"arguments", args}}));
}
void Monstey_LegacyOk(const J &r) { Monstey_Require(r["ok"].as_bool(), r.dump()); }
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        Monstey_Require(argc >= 4, "Usage: LegacyModelTests outputDir fixture.mesh fixture.skel");
        const std::filesystem::path dir = std::filesystem::absolute(argv[1]), mesh = argv[2], skel = argv[3];
        std::filesystem::create_directories(dir);
        Monstey_Runtime runtime({}, dir / L"settings.json");
        const auto sourceHash = Monstey_HashSha256(mesh), skelHash = Monstey_HashSha256(skel);
        const auto blend = dir / L"legacy.blend", native = dir / L"legacy.mesh";
        unsigned passed = 0;
        std::set<std::string> names;
        J report = J::object({{"status", "RUNNING"}, {"cases", J::array()}, {"python_runtime_used", false}});
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
        test("legacy_operations_keep_original_argument_schema", [&] {
            const auto catalog = Monstey_Runtime::Catalog();
            unsigned count = 0;
            for (const auto &op : catalog.as_array()) {
                const auto name = op["name"].as_string();
                if (name != "model_import_native" && name != "model_export_native")
                    continue;
                ++count;
                const auto &props = op["inputSchema"]["properties"];
                Monstey_Require(op["implemented"].as_bool() && op["implementation"].as_string() == "C++20",
                                "Legacy model operation is not native");
                if (name == "model_import_native")
                    Monstey_Require(props.contains("output_blend") &&
                                        props.contains("expected_skeleton_sha256") &&
                                        props["output_blend"]["format"].as_string() == "path",
                                    "Legacy import contract changed");
                else
                    Monstey_Require(props.contains("blend") && props.contains("output_mesh") &&
                                        props.contains("expected_sha256") &&
                                        props["blend"]["format"].as_string() == "path",
                                    "Legacy export contract changed");
            }
            Monstey_Require(count == 2, "Legacy model API aliases missing or duplicated");
        });
        test("legacy_import_uses_cpp_and_ignores_nonexistent_blender_executable", [&] {
            Monstey_LegacyFresh(blend);
            auto r = Monstey_LegacyCall(runtime, "model_import_native",
                                        J::object({{"source", Monstey_LegacyPath(mesh)},
                                                   {"output_blend", Monstey_LegacyPath(blend)},
                                                   {"skeleton", Monstey_LegacyPath(skel)},
                                                   {"blender", "Z:\\not-installed\\blender.exe"},
                                                   {"expected_source_sha256", sourceHash},
                                                   {"expected_skeleton_sha256", skelHash}}));
            Monstey_LegacyOk(r);
            const auto &result = r["result"];
            Monstey_Require(result["pipeline"].as_string() == "native_cpp" &&
                                result["blender_argument_ignored"].as_bool() &&
                                result["output_blend"].as_string() == Monstey_LegacyPath(blend) &&
                                result["skeleton_sha256"].as_string() == skelHash,
                            "Legacy import compatibility result changed");
            report["import"] = result;
        });
        test("legacy_export_hash_qualifies_actual_blend_and_is_byte_exact", [&] {
            Monstey_LegacyFresh(native);
            const auto blendHash = Monstey_HashSha256(blend);
            auto r = Monstey_LegacyCall(runtime, "model_export_native",
                                        J::object({{"blend", Monstey_LegacyPath(blend)},
                                                   {"output_mesh", Monstey_LegacyPath(native)},
                                                   {"blender", "Z:\\not-installed\\blender.exe"},
                                                   {"expected_sha256", blendHash}}));
            Monstey_LegacyOk(r);
            const auto &result = r["result"];
            Monstey_Require(
                result["byte_exact_noop"].as_bool() && result["source_sha256"].as_string() == blendHash &&
                    result["output_mesh"].as_string() == Monstey_LegacyPath(native) &&
                    result["blender_argument_ignored"].as_bool() && Monstey_HashSha256(native) == sourceHash,
                "Legacy native export lost hash/identity semantics");
            report["export"] = result;
        });
        test("legacy_export_rejects_embedded_native_hash_as_blend_hash", [&] {
            const auto out = dir / L"bad-export.mesh";
            Monstey_LegacyFresh(out);
            const auto r = Monstey_LegacyCall(runtime, "model_export_native",
                                              J::object({{"blend", Monstey_LegacyPath(blend)},
                                                         {"output_mesh", Monstey_LegacyPath(out)},
                                                         {"expected_sha256", sourceHash}}));
            Monstey_Require(!r["ok"].as_bool() && !std::filesystem::exists(out),
                            "Native baseline hash incorrectly qualified a blend");
        });
        test("legacy_import_rejects_mismatching_source_snapshot", [&] {
            const auto out = dir / L"bad-source.blend";
            Monstey_LegacyFresh(out);
            const auto r = Monstey_LegacyCall(runtime, "model_import_native",
                                              J::object({{"source", Monstey_LegacyPath(mesh)},
                                                         {"output_blend", Monstey_LegacyPath(out)},
                                                         {"expected_source_sha256", std::string(64, '0')}}));
            Monstey_Require(!r["ok"].as_bool() && !std::filesystem::exists(out),
                            "Wrong source hash published a blend");
        });
        test("legacy_import_qualifies_captured_skeleton_snapshot", [&] {
            const auto out = dir / L"bad-skeleton.blend";
            Monstey_LegacyFresh(out);
            const auto r =
                Monstey_LegacyCall(runtime, "model_import_native",
                                   J::object({{"source", Monstey_LegacyPath(mesh)},
                                              {"output_blend", Monstey_LegacyPath(out)},
                                              {"skeleton", Monstey_LegacyPath(skel)},
                                              {"expected_skeleton_sha256", std::string(64, '0')}}));
            Monstey_Require(!r["ok"].as_bool() && r["error"]["code"].as_string() == "source_hash_mismatch" &&
                                !std::filesystem::exists(out),
                            "Wrong skeleton snapshot was accepted");
        });
        test("expected_skeleton_hash_requires_an_actual_skeleton_input", [&] {
            const auto out = dir / L"missing-skeleton.blend";
            Monstey_LegacyFresh(out);
            const auto r = Monstey_LegacyCall(runtime, "model_import_native",
                                              J::object({{"source", Monstey_LegacyPath(mesh)},
                                                         {"output_blend", Monstey_LegacyPath(out)},
                                                         {"expected_skeleton_sha256", skelHash}}));
            Monstey_Require(!r["ok"].as_bool() && r["error"]["code"].as_string() == "source_hash_mismatch" &&
                                !std::filesystem::exists(out),
                            "Missing skeleton silently satisfied its expected hash");
        });
        test("legacy_required_output_keys_and_unknown_arguments_are_validated", [&] {
            auto r = Monstey_LegacyCall(runtime, "model_import_native",
                                        J::object({{"source", Monstey_LegacyPath(mesh)}}));
            Monstey_Require(!r["ok"].as_bool() && r["error"]["code"].as_string() == "missing_argument",
                            "Legacy required output key not enforced");
            r = Monstey_LegacyCall(runtime, "model_export_native",
                                   J::object({{"blend", Monstey_LegacyPath(blend)},
                                              {"output_mesh", Monstey_LegacyPath(dir / L"unused.mesh")},
                                              {"execute_python", true}}));
            Monstey_Require(!r["ok"].as_bool() && r["error"]["code"].as_string() == "unknown_argument",
                            "Legacy alias accepted unsupported execution behavior");
        });
        Monstey_Require(Monstey_HashSha256(mesh) == sourceHash && Monstey_HashSha256(skel) == skelHash,
                        "Read-only source changed");
        report["status"] = "PASS";
        report["passed"] = passed;
        const auto out = dir / L"legacy-model-tests.json";
        Monstey_LegacyFresh(out);
        Monstey_WriteNewText(out, report.dump(2) + "\n");
        std::cout << "Native legacy model tests: " << passed << " passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Native legacy model failure: " << e.what() << "\n";
        return 1;
    }
}
