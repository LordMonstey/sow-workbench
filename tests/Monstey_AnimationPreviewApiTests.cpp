#include "Monstey_AnimationPreviewApi.hpp"
#include "Monstey_AnimationPreviewApiFixture.hpp"
#include <array>
#include <functional>
#include <future>
#include <iostream>
#include <thread>
using namespace Monstey;
namespace
{
using J = Monstey_Json;
void Monstey_Check(bool value, const char *message)
{
    if (!value)
        throw Monstey_Error("animation_preview_test", message);
}
std::string Monstey_PathText(const std::filesystem::path &p) { return Monstey_Utf8(p.wstring()); }
J Monstey_Args(const std::filesystem::path &mesh, const std::filesystem::path &skel,
               const std::filesystem::path &anix, const std::filesystem::path &output, double tick = 500)
{
    return J::object({{"source", Monstey_PathText(mesh)},
                      {"skeleton", Monstey_PathText(skel)},
                      {"animation", Monstey_PathText(anix)},
                      {"clip", "walk"},
                      {"tick_ms", tick},
                      {"output", Monstey_PathText(output)},
                      {"width", 64},
                      {"height", 64}});
}
void Monstey_NoPartials(const std::filesystem::path &directory)
{
    for (const auto &entry : std::filesystem::recursive_directory_iterator(directory))
        Monstey_Check(!entry.path().filename().wstring().ends_with(L".partial"),
                      "Staging file survived operation");
}
void Monstey_Failure(const std::function<void()> &f, const std::filesystem::path &output,
                     const std::string &code = {})
{
    bool failed = false;
    try
    {
        f();
    }
    catch (const Monstey_Error &e)
    {
        failed = true;
        Monstey_Check(code.empty() || e.code == code, "Unexpected native error code");
    }
    catch (const std::exception &)
    {
        failed = true;
        Monstey_Check(code.empty(), "Expected a typed error");
    }
    Monstey_Check(failed, "Invalid animation preview request accepted");
    if (!output.empty())
        Monstey_Check(!std::filesystem::exists(output), "Rejected request published an output");
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        Monstey_Check(
            argc == 2 || argc == 6,
            "Usage AnimationPreviewApiTests NEW-directory [actual.mesh actual.skel actual.anix clip]");
        const std::filesystem::path directory = argv[1];
        Monstey_Check(!std::filesystem::exists(directory), "Output directory must be new");
        Monstey_RequireOfflineOutput(directory);
        std::filesystem::create_directories(directory);
        const auto mesh = directory / L"synthetic.mesh", skel = directory / L"synthetic.skel",
                   anix = directory / L"synthetic.anix";
        const auto mesh_bytes = Monstey_MeshFixture(), skel_bytes = Monstey_SkelFixture(),
                   anix_bytes = Monstey_AnimationFixture(skel_bytes);
        Monstey_WriteNewFile(mesh, mesh_bytes);
        Monstey_WriteNewFile(skel, skel_bytes);
        Monstey_WriteNewFile(anix, anix_bytes);
        const auto mesh_hash = Monstey_HashSha256(mesh), skel_hash = Monstey_HashSha256(skel),
                   anix_hash = Monstey_HashSha256(anix);
        auto tests = J::array();
        unsigned failed = 0;
        const auto test = [&](const std::string &name, const std::function<void()> &f)
        {
            try
            {
                f();
                Monstey_NoPartials(directory);
                tests.as_array().push_back(J::object({{"name", name}, {"passed", true}}));
            }
            catch (const std::exception &e)
            {
                ++failed;
                tests.as_array().push_back(
                    J::object({{"name", name}, {"passed", false}, {"error", e.what()}}));
            }
        };
        test("catalog_required_paths_clip_tick_and_typed_flat_options",
             [&]
             {
                 const auto c = Monstey_AnimationPreviewCatalog();
                 Monstey_Check(c.size() == 1, "Catalog size differs");
                 Monstey_Check(c[0u]["name"].as_string() == "model_animation_preview_render" &&
                                   !c[0u]["read_only"].as_bool(),
                               "Catalog identity differs");
                 const auto &s = c[0u]["inputSchema"];
                 Monstey_Check(s["required"] == J::array({"source", "skeleton", "animation", "clip",
                                                          "tick_ms", "output"}) &&
                                   !s["additionalProperties"].as_bool(),
                               "Required fields differ");
                 Monstey_Check(s["properties"]["tick_ms"]["type"].as_string() == "number" &&
                                   s["properties"]["width"]["type"].as_string() == "integer" &&
                                   s["properties"].contains("expected_animation_sha256"),
                               "Typed schema is incomplete");
             });
        test("guardless_sources_automatic_hashes_and_honest_scope",
             [&]
             {
                 const auto out = directory / L"guardless.bmp";
                 const auto r = Monstey_AnimationPreviewCall("model_animation_preview_render",
                                                             Monstey_Args(mesh, skel, anix, out));
                 const auto bmp = Monstey_ReadFile(out);
                 Monstey_Check(Monstey_U16(bmp, 0) == 0x4d42 && Monstey_U32(bmp, 18) == 64 &&
                                   Monstey_U32(bmp, 22) == static_cast<std::uint32_t>(-64),
                               "BMP layout differs");
                 Monstey_Check(r["inputs"]["source"]["sha256"].as_string() == mesh_hash &&
                                   r["inputs"]["skeleton"]["sha256"].as_string() == skel_hash &&
                                   r["inputs"]["animation"]["sha256"].as_string() == anix_hash,
                               "Automatic source identities missing");
                 for (const char *key : {"source", "skeleton", "animation"})
                     Monstey_Check(!r["inputs"][key]["expected_hash_guard"].as_bool(),
                                   "Optional guard became mandatory");
                 Monstey_Check(
                     r["host_ordinary_cpu_geometry"].as_bool() && r["animation_sample_evaluated"].as_bool() &&
                         !r["native_controller_verified"].as_bool() && !r["native_gpu_materials"].as_bool() &&
                         !r["preview"]["host_bind_geometry_only"].as_bool(),
                     "Animated scope flags differ");
                 Monstey_Check(r["output_sha256"].as_string() == Monstey_HashSha256(out) &&
                                   r["output_bytes"].as_uint() == bmp.size(),
                               "Published BMP identity differs");
             });
        test("shared_core_exact_bitmap_equivalence",
             [&]
             {
                 const auto out = directory / L"core.bmp";
                 const auto r = Monstey_AnimationPreviewRenderFile(Monstey_Args(mesh, skel, anix, out));
                 const auto binding = Monstey_AnimationPreviewBinding::Prepare(
                     Monstey_ModelReadScene(mesh, skel), skel_bytes, anix_bytes, "walk");
                 auto pose = binding.CreateFrame();
                 binding.Evaluate(500, pose);
                 Monstey_ModelPreviewOptions options;
                 options.width = options.height = 64;
                 const auto expected =
                     Monstey_ModelPreviewBitmap(Monstey_ModelPreviewRender(pose.Scene(), options));
                 Monstey_Check(Monstey_ReadFile(out) == expected && r["tick_ms"].as_number() == 500,
                               "Wrapper does not use shared animation core");
             });
        test("three_optional_expected_guards_and_uppercase",
             [&]
             {
                 auto a = Monstey_Args(mesh, skel, anix, directory / L"guarded.bmp");
                 auto upper = anix_hash;
                 for (auto &c : upper)
                     if (c >= 'a' && c <= 'f')
                         c = static_cast<char>(c - 'a' + 'A');
                 a["expected_source_sha256"] = mesh_hash;
                 a["expected_skeleton_sha256"] = skel_hash;
                 a["expected_animation_sha256"] = upper;
                 const auto r = Monstey_AnimationPreviewRenderFile(a);
                 for (const char *key : {"source", "skeleton", "animation"})
                     Monstey_Check(r["inputs"][key]["expected_hash_guard"].as_bool(),
                                   "Expected guard was ignored");
                 Monstey_Check(r["output_sha256"].as_string() ==
                                   Monstey_HashSha256(directory / L"guardless.bmp"),
                               "Guard changed render output");
             });
        test("typed_viewport_options_forwarded",
             [&]
             {
                 auto a = Monstey_Args(mesh, skel, anix, directory / L"options.bmp");
                 a["width"] = 96;
                 a["height"] = 80;
                 a["all_containers"] = true;
                 a["bones"] = false;
                 a["wireframe"] = true;
                 a["vertex_colors"] = true;
                 a["yaw_degrees"] = 25.5;
                 a["pitch_degrees"] = -8;
                 a["zoom"] = 1.5;
                 a["pan_x"] = 0.1;
                 a["pan_y"] = -0.1;
                 const auto r = Monstey_AnimationPreviewRenderFile(a);
                 Monstey_Check(r["viewport"]["width"].as_uint() == 96 &&
                                   r["viewport"]["height"].as_uint() == 80 &&
                                   r["preview"]["parts"].as_uint() == 2 && r["preview"]["bones"].empty() &&
                                   r["viewport"]["yaw_degrees"].as_number() == 25.5,
                               "Viewport option lost");
             });
        test("three_hash_mismatch_controls_leave_no_output",
             [&]
             {
                 for (const char *key :
                      {"expected_source_sha256", "expected_skeleton_sha256", "expected_animation_sha256"})
                 {
                     const auto out = directory / L"hash-mismatch.bmp";
                     auto a = Monstey_Args(mesh, skel, anix, out);
                     a[key] = std::string(64, '0');
                     Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out,
                                     "source_hash_mismatch");
                 }
             });
        test("malformed_hash_values_reject",
             [&]
             {
                 for (const char *key :
                      {"expected_source_sha256", "expected_skeleton_sha256", "expected_animation_sha256"})
                     for (const J bad : J::Array{J(""), J(std::string(64, 'x')), J(42), J(true)})
                     {
                         const auto out = directory / L"malformed-hash.bmp";
                         auto a = Monstey_Args(mesh, skel, anix, out);
                         a[key] = bad;
                         Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out,
                                         "animation_preview");
                     }
             });
        test("unknown_operation_and_argument_reject",
             [&]
             {
                 const auto out = directory / L"unknown.bmp";
                 auto a = Monstey_Args(mesh, skel, anix, out);
                 Monstey_Failure([&]
                                 { (void)Monstey_AnimationPreviewCall("model_animation_preview_guess", a); },
                                 out, "unknown_operation");
                 a["guess"] = true;
                 Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out,
                                 "animation_preview");
             });
        test("six_required_fields_object_shape_and_NUL",
             [&]
             {
                 const auto out = directory / L"missing.bmp";
                 for (const char *key : {"source", "skeleton", "animation", "clip", "tick_ms", "output"})
                 {
                     auto a = Monstey_Args(mesh, skel, anix, out);
                     a.as_object().erase(key);
                     Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out,
                                     "animation_preview");
                 }
                 Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(J::array()); }, out,
                                 "animation_preview");
                 for (const char *key : {"source", "skeleton", "animation", "clip", "output"})
                 {
                     auto a = Monstey_Args(mesh, skel, anix, out);
                     a[key] = std::string("bad\0path", 8);
                     Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out,
                                     "animation_preview");
                 }
             });
        test("tick_type_and_native_clip_bounds_reject",
             [&]
             {
                 const auto out = directory / L"bad-tick.bmp";
                 for (const J bad : J::Array{J("500"), J(true), J(nullptr), J::array()})
                 {
                     auto a = Monstey_Args(mesh, skel, anix, out);
                     a["tick_ms"] = bad;
                     Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out,
                                     "animation_preview");
                 }
                 for (double bad : {-1.0, 1001.0})
                 {
                     auto a = Monstey_Args(mesh, skel, anix, out, bad);
                     Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out,
                                     "animation_preview");
                 }
             });
        test("raw_native_integer_tick_policy_preserved",
             [&]
             {
                 const auto raw = directory / L"raw.anix";
                 Monstey_WriteNewFile(raw, Monstey_AnimationFixture(skel_bytes, true));
                 const auto out = directory / L"fractional.bmp";
                 auto a = Monstey_Args(mesh, skel, raw, out, 500.5);
                 Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out);
                 a["tick_ms"] = 500;
                 const auto r = Monstey_AnimationPreviewRenderFile(a);
                 Monstey_Check(r["tick_ms"].as_number() == 500, "Valid raw integer tick changed");
             });
        test("clip_name_missing_or_bad_type_reject",
             [&]
             {
                 const auto out = directory / L"bad-clip.bmp";
                 for (const J bad : J::Array{J(""), J(42), J(true)})
                 {
                     auto a = Monstey_Args(mesh, skel, anix, out);
                     a["clip"] = bad;
                     Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out,
                                     "animation_preview");
                 }
                 auto a = Monstey_Args(mesh, skel, anix, out);
                 a["clip"] = "absent";
                 Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out,
                                 "animation_preview");
             });
        test("viewport_type_and_limit_controls",
             [&]
             {
                 const J cases = J::array({J::object({{"width", 31}}), J::object({{"width", 4097}}),
                                           J::object({{"width", 64.5}}), J::object({{"height", true}}),
                                           J::object({{"width", 4096}, {"height", 4096}}),
                                           J::object({{"bones", 1}}), J::object({{"container", -1}}),
                                           J::object({{"zoom", 0}}), J::object({{"zoom", 21}}),
                                           J::object({{"pan_x", 9}}), J::object({{"yaw_degrees", false}})});
                 for (const auto &c : cases.as_array())
                 {
                     const auto out = directory / L"bad-options.bmp";
                     auto a = Monstey_Args(mesh, skel, anix, out);
                     for (const auto &[key, v] : c.as_object())
                         a[key] = v;
                     Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out,
                                     "animation_preview");
                 }
             });
        test("missing_input_and_nonregular_input_reject",
             [&]
             {
                 const auto out = directory / L"bad-source.bmp";
                 for (const char *key : {"source", "skeleton", "animation"})
                 {
                     auto a = Monstey_Args(mesh, skel, anix, out);
                     a[key] = Monstey_PathText(directory / L"absent");
                     Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out);
                     a[key] = Monstey_PathText(directory);
                     Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out,
                                     "animation_preview");
                 }
             });
        test("malformed_native_inputs_reject",
             [&]
             {
                 const auto bad = directory / L"malformed.bin";
                 Monstey_WriteNewFile(bad, Monstey_Bytes{1, 2, 3, 4});
                 const auto out = directory / L"malformed.bmp";
                 for (const char *key : {"source", "skeleton", "animation"})
                 {
                     auto a = Monstey_Args(mesh, skel, anix, out);
                     a[key] = Monstey_PathText(bad);
                     Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out);
                 }
             });
        test("mismatched_model_native_name_identity_reject",
             [&]
             {
                 auto raw = mesh_bytes;
                 const auto info = 20 + Monstey_U32(raw, 8);
                 bool patched = false;
                 for (std::size_t at = info; at + 4 < raw.size(); ++at)
                     if (Monstey_U32(raw, at) == 0x93d9a424u)
                     {
                         Monstey_PutU32(raw, at + 4, 0x12345678);
                         patched = true;
                         break;
                     }
                 Monstey_Check(patched, "Synthetic bone marker not found");
                 const auto bad = directory / L"mismatch.mesh";
                 Monstey_WriteNewFile(bad, raw);
                 const auto out = directory / L"mismatch.bmp";
                 Monstey_Failure(
                     [&] { (void)Monstey_AnimationPreviewRenderFile(Monstey_Args(bad, skel, anix, out)); },
                     out, "animation_preview");
             });
        test("alternate_clip_reject",
             [&]
             {
                 auto raw = anix_bytes;
                 const auto meta = Monstey_U32(raw, 32), desc = meta + Monstey_U32(raw, meta + 28);
                 Monstey_PutU16(raw, desc + 50, 1);
                 const auto bad = directory / L"alternate.anix";
                 Monstey_WriteNewFile(bad, raw);
                 const auto out = directory / L"alternate.bmp";
                 Monstey_Failure(
                     [&] { (void)Monstey_AnimationPreviewRenderFile(Monstey_Args(mesh, skel, bad, out)); },
                     out, "animation_preview");
             });
        test("missing_container_reject",
             [&]
             {
                 const auto out = directory / L"container.bmp";
                 auto a = Monstey_Args(mesh, skel, anix, out);
                 a["container"] = 99;
                 Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, out, "model_preview");
             });
        test("all_three_inputs_protected_from_overwrite",
             [&]
             {
                 for (const auto &input : {mesh, skel, anix})
                 {
                     auto a = Monstey_Args(mesh, skel, anix, input);
                     Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, {});
                 }
                 Monstey_Check(Monstey_ReadFile(mesh) == mesh_bytes && Monstey_ReadFile(skel) == skel_bytes &&
                                   Monstey_ReadFile(anix) == anix_bytes,
                               "Overwrite altered source");
             });
        test("existing_destination_preserved_before_source_read",
             [&]
             {
                 const auto out = directory / L"existing.bmp";
                 const Monstey_Bytes marker{9, 8, 7};
                 Monstey_WriteNewFile(out, marker);
                 auto a = Monstey_Args(directory / L"absent.mesh", skel, anix, out);
                 Monstey_Failure([&] { (void)Monstey_AnimationPreviewRenderFile(a); }, {}, "output_exists");
                 Monstey_Check(Monstey_ReadFile(out) == marker, "Existing destination changed");
             });
        test("publication_obstruction_preserved",
             [&]
             {
                 const auto block = directory / L"block";
                 Monstey_WriteNewFile(block, Monstey_Bytes{1});
                 const auto out = block / L"frame.bmp";
                 Monstey_Failure(
                     [&] { (void)Monstey_AnimationPreviewRenderFile(Monstey_Args(mesh, skel, anix, out)); },
                     out);
                 Monstey_Check(Monstey_ReadFile(block) == Monstey_Bytes{1},
                               "Publication obstruction changed");
             });
        test("parallel_same_destination_has_one_complete_winner",
             [&]
             {
                 const auto out = directory / L"parallel.bmp";
                 std::promise<void> start;
                 const auto signal = start.get_future().share();
                 std::array<unsigned, 2> successes{};
                 const auto work = [&](std::size_t i)
                 {
                     signal.wait();
                     try
                     {
                         (void)Monstey_AnimationPreviewRenderFile(Monstey_Args(mesh, skel, anix, out));
                         successes[i] = 1;
                     }
                     catch (const std::exception &)
                     {
                     }
                 };
                 std::thread first(work, 0);
                 try
                 {
                     std::thread second(work, 1);
                     start.set_value();
                     first.join();
                     second.join();
                 }
                 catch (...)
                 {
                     try
                     {
                         start.set_value();
                     }
                     catch (...)
                     {
                     }
                     if (first.joinable())
                         first.join();
                     throw;
                 }
                 Monstey_Check(successes[0] + successes[1] == 1 &&
                                   Monstey_U16(Monstey_ReadFile(out), 0) == 0x4d42,
                               "Atomic publication did not produce exactly one complete winner");
             });
        test("different_ticks_produce_different_complete_images",
             [&]
             {
                 const auto r0 = Monstey_AnimationPreviewRenderFile(
                     Monstey_Args(mesh, skel, anix, directory / L"tick0.bmp", 0));
                 const auto r1 = Monstey_AnimationPreviewRenderFile(
                     Monstey_Args(mesh, skel, anix, directory / L"tick1000.bmp", 1000));
                 Monstey_Check(r0["output_sha256"] != r1["output_sha256"],
                               "Moving fixture did not change rendered image");
             });
        J actual = nullptr;
        if (argc == 6)
            test("actual_guarded_and_unguarded_same_core_render",
                 [&]
                 {
                     const std::filesystem::path am = argv[2], as = argv[3], aa = argv[4];
                     const auto mh = Monstey_HashSha256(am), sh = Monstey_HashSha256(as),
                                ah = Monstey_HashSha256(aa);
                     auto a = Monstey_Args(am, as, aa, directory / L"actual-guarded.bmp", 733);
                     a["clip"] = Monstey_Utf8(argv[5]);
                     a["width"] = a["height"] = 256;
                     a["expected_source_sha256"] = mh;
                     a["expected_skeleton_sha256"] = sh;
                     a["expected_animation_sha256"] = ah;
                     actual = Monstey_AnimationPreviewRenderFile(a);
                     a["output"] = Monstey_PathText(directory / L"actual-unguarded.bmp");
                     for (const char *key :
                          {"expected_source_sha256", "expected_skeleton_sha256", "expected_animation_sha256"})
                         a.as_object().erase(key);
                     const auto unguarded = Monstey_AnimationPreviewRenderFile(a);
                     Monstey_Check(actual["output_sha256"] == unguarded["output_sha256"],
                                   "Optional guards changed actual image");
                     Monstey_Check(actual["binding"]["bones"].as_uint() == 128 &&
                                       actual["binding"]["mapped_shared_position"].as_uint() == 7 &&
                                       actual["binding"]["mapped_shared_rotation"].as_uint() == 75,
                                   "Actual same-rig mapping differs");
                     Monstey_Check(Monstey_HashSha256(am) == mh && Monstey_HashSha256(as) == sh &&
                                       Monstey_HashSha256(aa) == ah,
                                   "Actual source changed");
                 });
        Monstey_Check(Monstey_HashSha256(mesh) == mesh_hash && Monstey_HashSha256(skel) == skel_hash &&
                          Monstey_HashSha256(anix) == anix_hash,
                      "Synthetic sources changed");
        const auto report = J::object({{"schema", "Monstey.AnimationPreviewApiTests/1"},
                                       {"groups", static_cast<std::uint64_t>(tests.size())},
                                       {"failed", failed},
                                       {"tests", tests},
                                       {"actual", actual},
                                       {"source_unchanged", true},
                                       {"python_product_runtime", false}});
        Monstey_WriteNewText(directory / L"report.json", report.dump(2));
        std::cout << "Animation preview API: " << tests.size() << " groups; failed=" << failed << '\n';
        return failed ? 1 : 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
