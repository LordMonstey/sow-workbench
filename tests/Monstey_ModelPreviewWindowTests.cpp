#include <chrono>
#include <windows.h>

#include "Monstey_ModelPreviewWindow.hpp"
#include <iostream>
using namespace Monstey;
namespace
{
void Monstey_PreviewTestRequire(bool condition, const char *message)
{
    if (!condition)
        throw Monstey_Error("preview_window_test", message);
}
void Monstey_PreviewTestPaint(HWND window)
{
    InvalidateRect(window, nullptr, FALSE);
    SendMessageW(window, WM_PAINT, 0, 0);
    GdiFlush();
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    HWND owner = nullptr;
    try
    {
        Monstey_PreviewTestRequire(argc == 4, "Usage ModelessPreviewTests NEW-output-dir mesh skel");
        const std::filesystem::path dir = std::filesystem::path(argv[1]) / (L"run-" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count())), mesh = argv[2], skel = argv[3];
        Monstey_PreviewTestRequire(!std::filesystem::exists(dir), "New output directory required");
        std::filesystem::create_directories(dir);
        const auto mh = Monstey_HashSha256(mesh), sh = Monstey_HashSha256(skel);
        auto scene = std::make_shared<Monstey_ModelScene>(Monstey_ModelReadScene(mesh, skel));
        owner = CreateWindowExW(0, L"STATIC", L"Native preview owner", WS_OVERLAPPEDWINDOW, 0, 0, 900, 700, nullptr,
                                nullptr, GetModuleHandleW(nullptr), nullptr);
        Monstey_PreviewTestRequire(owner != nullptr, "Owner creation failed");
        auto reports = Monstey_Json::array();
        unsigned groups = 0;
        {
            Monstey_ModelPreviewWindow preview;
            preview.Create(owner, SW_HIDE);
            const auto first = preview.BeginLoad(mesh, skel), second = preview.BeginLoad(mesh, skel);
            Monstey_PreviewTestRequire(!preview.Accepts(first) && preview.Accepts(second),
                                       "Superseded load remained active");
            Monstey_PreviewTestRequire(!preview.SetScene(first, scene) && preview.Inspect()["loading"].as_bool(),
                                       "Stale scene changed loading state");
            Monstey_PreviewTestRequire(preview.SetScene(second, scene), "Current scene rejected");
            Monstey_PreviewTestPaint(preview.Handle());
            Monstey_PreviewTestRequire(preview.Inspect()["ready"].as_bool() &&
                                           preview.Inspect()["report"]["bone_edges"].as_uint() + 1 ==
                                               scene->bones.size(),
                                       "Modeless scene did not render actual skeleton");
            ++groups;
            const auto before = preview.Snapshot();
            SendMessageW(preview.Handle(), WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
            Monstey_PreviewTestRequire(preview.Inspect()["zoom"].as_number() > 1, "Wheel event lost");
            MSG key{};
            key.hwnd = GetDlgItem(preview.Handle(), 100);
            key.message = WM_KEYDOWN;
            key.wParam = 'F';
            Monstey_PreviewTestRequire(preview.RouteMessage(key) && preview.Inspect()["zoom"].as_number() == 1,
                                       "Child-focus F route failed");
            ++groups;
            MSG mainKey{};
            mainKey.hwnd = owner;
            mainKey.message = WM_KEYDOWN;
            mainKey.wParam = 'O';
            Monstey_PreviewTestRequire(!preview.RouteMessage(mainKey), "Preview consumed owner input");
            key.wParam = 'O';
            Monstey_PreviewTestRequire(preview.RouteMessage(key),
                                       "Child input could fall through to main accelerators");
            ++groups;
            SendMessageW(preview.Handle(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(400, 300));
            SendMessageW(preview.Handle(), WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(430, 320));
            SendMessageW(preview.Handle(), WM_LBUTTONUP, 0, 0);
            Monstey_PreviewTestPaint(preview.Handle());
            Monstey_PreviewTestRequire(preview.Inspect()["yaw_degrees"].as_number() == 15 &&
                                           preview.Inspect()["pitch_degrees"].as_number() == 10,
                                       "Orbit gesture failed");
            ++groups;
            Monstey_WriteNewFile(dir / L"modeless-view.bmp", preview.Snapshot());
            const auto pending = preview.BeginLoad(mesh, skel);
            const auto oldHandle = preview.Handle();
            preview.Close();
            Monstey_PreviewTestRequire(!preview.Handle() && !IsWindow(oldHandle) && !preview.SetScene(pending, scene),
                                       "Closed preview accepted delayed scene");
            MSG quit{};
            Monstey_PreviewTestRequire(!PeekMessageW(&quit, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) && IsWindow(owner),
                                       "Closing preview quit/destroyed owner");
            ++groups;
            preview.Create(owner, SW_HIDE);
            const auto fresh = preview.BeginLoad(mesh, skel);
            Monstey_PreviewTestRequire(fresh != pending && !preview.SetScene(pending, scene) &&
                                           preview.SetScene(fresh, scene),
                                       "Reopened preview accepted old identity");
            Monstey_PreviewTestPaint(preview.Handle());
            Monstey_PreviewTestRequire(preview.Snapshot() == before, "Reopened view changed default image");
            ++groups;
            const auto failed = preview.BeginLoad(mesh, skel);
            Monstey_PreviewTestRequire(preview.SetError(failed, "Synthetic load failure") &&
                                           !preview.Inspect()["loading"].as_bool() &&
                                           preview.Inspect()["failed"].as_bool(),
                                       "Load failure left UI pending");
            Monstey_PreviewTestRequire(!preview.SetScene(failed, scene), "Failed request later accepted");
            const auto recover = preview.BeginLoad(mesh, skel);
            Monstey_PreviewTestRequire(preview.SetScene(recover, scene), "Preview did not recover from failed input");
            ++groups;
            Monstey_PreviewTestPaint(preview.Handle());
            const auto gdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS),
                       user = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
            for (unsigned i = 0; i < 64; ++i)
                Monstey_PreviewTestPaint(preview.Handle());
            Monstey_PreviewTestRequire(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == gdi &&
                                           GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS) == user,
                                       "Modeless repaint resource growth");
            ++groups;
            reports.as_array().push_back(preview.Inspect());
            DestroyWindow(owner);
            owner = nullptr;
            Monstey_PreviewTestRequire(!preview.Handle(), "Owned preview outlived owner destruction");
            ++groups;
        }
        Monstey_PreviewTestRequire(Monstey_HashSha256(mesh) == mh && Monstey_HashSha256(skel) == sh,
                                   "Preview modified source assets");
        Monstey_WriteNewText(dir / L"report.json", Monstey_Json::object({{"status", "PASS"},
                                                                         {"groups", groups},
                                                                         {"native_gui", true},
                                                                         {"modeless", true},
                                                                         {"source_unchanged", true},
                                                                         {"no_preview_WM_QUIT", true},
                                                                         {"reports", reports}})
                                                           .dump(2) +
                                                       "\n");
        std::cout << "Native modeless preview: " << groups << " groups passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        if (owner)
            DestroyWindow(owner);
        std::cerr << e.what() << '\n';
        return 1;
    }
}