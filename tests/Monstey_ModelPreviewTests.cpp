#include <chrono>
#include "Monstey_ModelPreview.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
using namespace Monstey;
namespace
{
void Monstey_Check(bool value, const char *text)
{
    if (!value)
        throw Monstey_Error("preview_test", text);
}
Monstey_ModelScene Monstey_Fixture()
{
    Monstey_ModelScene s;
    Monstey_ModelPart p;
    p.channels["POSITION"] = {3,
                              {-100, -100, 0, 100, -100, 0, 0, 100, 0, -100, -100, 100, 100, -100, 100, 0, 100, 100}};
    p.channels["COLOR0"] = {
        4, {0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255}};
    p.indices = {0, 1, 2, 3, 4, 5};
    s.parts.push_back(p);
    for (unsigned i = 0; i < 3; ++i)
    {
        Monstey_ModelBone b;
        b.hk_index = i;
        b.source_index = i;
        b.parent_hk = i ? 0 : -1;
        b.name = "Bone" + std::to_string(i);
        b.global_bind_metres = {1, 0, 0, double(i == 1), 0, 1, 0, double(i == 2), 0, 0, 1, 0, 0, 0, 0, 1};
        s.bones.push_back(b);
    }
    return s;
}
void Monstey_Save(const std::filesystem::path &p, const Monstey_ModelPreviewFrame &f)
{
    if (std::filesystem::exists(p))
        std::filesystem::remove(p);
    Monstey_WriteNewFile(p, Monstey_ModelPreviewBitmap(f));
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        Monstey_Check(argc >= 2, "Usage PreviewTests output-directory [mesh skel]");
        const std::filesystem::path dir = std::filesystem::path(argv[1]) / (L"run-" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(dir);
        unsigned groups = 0;
        auto report = Monstey_Json::object();
        auto scene = Monstey_Fixture();
        Monstey_ModelPreviewOptions o;
        o.width = 256;
        o.height = 256;
        o.vertex_colors = true;
        o.bones = false;
        const auto source = scene.parts[0].channels.at("POSITION").values;
        auto a = Monstey_ModelPreviewRender(scene, o);
        Monstey_Check(a.width == 256 && a.pixels.size() == 65536 && a.pixels[128 * 256 + 128] == 0xffff0000,
                      "Front triangle not visible red");
        ++groups;
        scene.parts[0].indices = {3, 4, 5, 0, 1, 2};
        const auto b = Monstey_ModelPreviewRender(scene, o);
        Monstey_Check(a.pixels == b.pixels, "Triangle order changes depth result");
        ++groups;
        scene.parts[0].indices = {2, 1, 0, 5, 4, 3};
        const auto reversed = Monstey_ModelPreviewRender(scene, o);
        Monstey_Check(a.pixels == reversed.pixels, "Two-sided winding changes output");
        ++groups;
        o.bones = true;
        const auto bones = Monstey_ModelPreviewRender(scene, o);
        const auto &rows = bones.report["bones"];
        Monstey_Check(rows.size() == 3 && bones.report["bone_edges"].as_uint() == 2,
                      "Unexpected synthetic bone relations");
        const double rootX = rows[0u]["screen_xy"][0u].as_number(), rightX = rows[1u]["screen_xy"][0u].as_number();
        const double rootY = rows[0u]["screen_xy"][1u].as_number(), upY = rows[2u]["screen_xy"][1u].as_number();
        Monstey_Check(std::abs(rightX - rootX - (rootY - upY)) < 1e-10 && rightX > rootX && upY < rootY,
                      "Bone centimetre/metre or Y-up mismatch");
        Monstey_Check(rows[1u]["world_metres"][0u].as_number() == 1, "Bone translation should be metres");
        ++groups;
        o.bones_in_front = false;
        const auto occludedBones = Monstey_ModelPreviewRender(scene, o);
        Monstey_Check(occludedBones.pixels[128 * 256 + 128] == 0xffff0000 &&
                          bones.pixels[128 * 256 + 128] != occludedBones.pixels[128 * 256 + 128],
                      "Bone depth/front policy failed");
        o.bones_in_front = true;
        ++groups;
        auto farBones = scene;
        farBones.bones[1].global_bind_metres[3] = 1e36;
        farBones.bones[2].global_bind_metres[3] = -1e36;
        farBones.bones[2].global_bind_metres[7] = 0;
        farBones.bones[2].parent_hk = 1;
        const auto farFrame = Monstey_ModelPreviewRender(farBones, o);
        Monstey_Check(farFrame.pixels.size() == 65536 && farFrame.report["bone_edges"].as_uint() == 2,
                      "Extreme clipped bone segment did not remain bounded");
        std::size_t crossing = 0;
        for (unsigned x = 0; x < 256; ++x)
            if (farFrame.pixels[128 * 256 + x] == 0xff52d6f5 || farFrame.pixels[128 * 256 + x] == 0xffc0f4ff)
                ++crossing;
        Monstey_Check(crossing == 256, "Clipping collapsed a long horizontal bone segment");
        ++groups;
        o.mesh = false;
        const auto onlyBones = Monstey_ModelPreviewRender(scene, o);
        Monstey_Check(onlyBones.report["rasterized_triangles"].as_uint() == 0 && onlyBones.pixels != a.pixels,
                      "Skeleton-only display failed");
        ++groups;
        o.mesh = true;
        o.bones = false;
        o.wireframe = true;
        const auto wire = Monstey_ModelPreviewRender(scene, o);
        Monstey_Check(wire.pixels != a.pixels && wire.report["rasterized_triangles"].as_uint() == 2,
                      "Wireframe display failed");
        ++groups;
        o.wireframe = false;
        o.zoom = 20;
        o.pan_x = 8;
        o.pan_y = -8;
        const auto clipped = Monstey_ModelPreviewRender(scene, o);
        Monstey_Check(clipped.pixels.size() == 65536, "Clipped output extent wrong");
        ++groups;
        o.zoom = 1;
        o.pan_x = 0;
        o.pan_y = 0;
        o.yaw_degrees = 90;
        const auto side = Monstey_ModelPreviewRender(scene, o);
        Monstey_Check(side.pixels != a.pixels && std::all_of(side.pixels.begin(), side.pixels.end(),
                                                             [](auto pixel) { return pixel == 0xff1b1f25; }),
                      "Orthographic side view not edge-on");
        ++groups;
        o.yaw_degrees = 0;
        scene.parts[0].container = 2;
        bool rejected = false;
        try
        {
            (void)Monstey_ModelPreviewRender(scene, o);
        }
        catch (const Monstey_Error &e)
        {
            rejected = e.code == "model_preview";
        }
        Monstey_Check(rejected, "Nonexistent container not rejected");
        o.all_containers = true;
        Monstey_Check(Monstey_ModelPreviewRender(scene, o).pixels == a.pixels, "All-container display differs");
        ++groups;
        const auto bytes = Monstey_ModelPreviewBitmap(a);
        Monstey_Check(Monstey_U16(bytes, 0) == 0x4d42 && Monstey_U32(bytes, 2) == bytes.size() &&
                          Monstey_U32(bytes, 10) == 54 && Monstey_U32(bytes, 18) == 256 &&
                          std::bit_cast<std::int32_t>(Monstey_U32(bytes, 22)) == -256 && Monstey_U16(bytes, 28) == 32 &&
                          Monstey_U32(bytes, 54 + (128 * 256 + 128) * 4) == 0xffff0000,
                      "Bitmap structure or top-down color mismatch");
        ++groups;
        const auto fail = [&](const Monstey_ModelScene &s, Monstey_ModelPreviewOptions options) {
            bool bad = false;
            try
            {
                (void)Monstey_ModelPreviewRender(s, options);
            }
            catch (const Monstey_Error &e)
            {
                bad = e.code == "model_preview";
            }
            Monstey_Check(bad, "Malformed preview input accepted");
        };
        auto invalid = scene;
        invalid.parts[0].indices[0] = 100;
        fail(invalid, o);
        invalid = scene;
        invalid.parts[0].channels["POSITION"].values[0] = std::numeric_limits<double>::quiet_NaN();
        fail(invalid, o);
        invalid = scene;
        invalid.parts[0].channels["COLOR0"].values.pop_back();
        fail(invalid, o);
        invalid = scene;
        invalid.bones[1].parent_hk = 99;
        auto boneOptions = o;
        boneOptions.bones = true;
        fail(invalid, boneOptions);
        auto invalidOptions = o;
        invalidOptions.width = 4096;
        invalidOptions.height = 4096;
        fail(scene, invalidOptions);
        invalidOptions = o;
        invalidOptions.zoom = 0;
        fail(scene, invalidOptions);
        invalidOptions = o;
        invalidOptions.pitch_degrees = std::numeric_limits<double>::infinity();
        fail(scene, invalidOptions);
        ++groups;

        for (unsigned n = 0; n < 32; ++n)
        {
            const float farZ = 100.0f + static_cast<float>(n) * 0.1f;
            const float high = std::nextafter(farZ, std::numeric_limits<float>::infinity());
            const float low = std::nextafter(farZ, -std::numeric_limits<float>::infinity());
            Monstey_ModelScene intersecting;
            Monstey_ModelPart part;
            part.channels["POSITION"] = {3, {-100, -100, farZ, 100,  -100, farZ, 0,   100, farZ, -100, -100,
                                             high, 100,  -100, high, 0,    100,  low, 0,   0,    -100}};
            part.channels["COLOR0"] = {4, {0, 0,   255, 255, 0, 0,   255, 255, 0, 0,   255, 255, 255, 0,
                                           0, 255, 255, 0,   0, 255, 255, 0,   0, 255, 255, 255, 255, 255}};
            part.indices = {0, 1, 2, 3, 4, 5};
            intersecting.parts.push_back(part);
            Monstey_ModelPreviewOptions view;
            view.width = 64;
            view.height = 64;
            view.bones = false;
            view.vertex_colors = true;
            const auto forward = Monstey_ModelPreviewRender(intersecting, view);
            intersecting.parts[0].indices = {3, 4, 5, 0, 1, 2};
            const auto backward = Monstey_ModelPreviewRender(intersecting, view);
            Monstey_Check(forward.pixels == backward.pixels, "F32 near-intersecting depth depends on triangle order");
        }
        ++groups;
        Monstey_Check(scene.parts[0].channels.at("POSITION").values == source, "Preview mutated source geometry");
        Monstey_Save(dir / L"synthetic-shaded.bmp", a);
        Monstey_Save(dir / L"synthetic-bones.bmp", bones);
        Monstey_Save(dir / L"synthetic-wireframe.bmp", wire);
        if (argc >= 4)
        {
            const std::filesystem::path mesh = argv[2], skel = argv[3];
            const auto mh = Monstey_HashSha256(mesh), sh = Monstey_HashSha256(skel);
            const auto actual = Monstey_ModelReadScene(mesh, skel);
            o = {};
            o.width = 1000;
            o.height = 1000;
            o.yaw_degrees = 15;
            o.pitch_degrees = -8;
            const auto actualFrame = Monstey_ModelPreviewRender(actual, o);
            Monstey_Save(dir / L"actual-bind-with-bones.bmp", actualFrame);
            o.bones = false;
            const auto actualMesh = Monstey_ModelPreviewRender(actual, o);
            Monstey_Save(dir / L"actual-bind-shaded.bmp", actualMesh);
            Monstey_Check(actualFrame.report["bone_edges"].as_uint() + 1 == actual.bones.size(),
                          "Actual tree edge count wrong");
            Monstey_Check(
                std::any_of(actualMesh.pixels.begin(), actualMesh.pixels.end(), [](auto p) { return p != 0xff1b1f25; }),
                "Actual geometry not rasterized");
            Monstey_Check(Monstey_HashSha256(mesh) == mh && Monstey_HashSha256(skel) == sh, "Actual input changed");
            report["actual"] = actualFrame.report;
            report["actual_source_unchanged"] = true;
            ++groups;
        }
        report["groups"] = groups;
        report["status"] = "PASS";
        report["synthetic"] = a.report;
        const auto path = dir / L"report.json";
        if (std::filesystem::exists(path))
            std::filesystem::remove(path);
        Monstey_WriteNewText(path, report.dump(2) + "\n");
        std::cout << "Native preview tests: " << groups << " groups passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}