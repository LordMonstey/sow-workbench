#include "Monstey_TextureApi.hpp"
#include "Monstey_TexturePreviewWindow.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
#include <windowsx.h>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
void Check(bool ok, const char *why) {
  if (!ok)
    throw Monstey_Error("texture_window_test", why);
}
B Dds(std::uint32_t width = 2, std::uint32_t height = 2,
      std::uint32_t depth = 1, std::uint32_t mips = 1,
      std::uint32_t dimension = 3, std::uint32_t layers = 1,
      std::uint32_t format = 87) {
  B b(148);
  Monstey_PutU32(b, 0, 0x20534444);
  Monstey_PutU32(b, 4, 124);
  Monstey_PutU32(b, 8, 0x1007);
  Monstey_PutU32(b, 12, height);
  Monstey_PutU32(b, 16, width);
  Monstey_PutU32(b, 24, depth);
  Monstey_PutU32(b, 28, mips);
  Monstey_PutU32(b, 76, 32);
  Monstey_PutU32(b, 80, 4);
  Monstey_PutU32(b, 84, 0x30315844);
  Monstey_PutU32(b, 108, 0x1000);
  Monstey_PutU32(b, 128, format);
  Monstey_PutU32(b, 132, dimension);
  Monstey_PutU32(b, 140, layers);
  for (std::uint32_t a = 0; a < layers; ++a)
    for (std::uint32_t m = 0; m < mips; ++m) {
      const auto w = std::max(1U, width >> m), h = std::max(1U, height >> m),
                 d = dimension == 4 ? std::max(1U, depth >> m) : 1U;
      if (format == 71) {
        const auto n = std::size_t((w + 3) / 4) * ((h + 3) / 4) * 8 * d;
        const auto old = b.size();
        b.resize(old + n, 0);
        for (std::size_t i = old; i < b.size(); i += 8) {
          b[i] = 0;
          b[i + 1] = 0xf8;
        }
      } else
        for (std::uint32_t z = 0; z < d; ++z)
          for (std::uint32_t i = 0; i < w * h; ++i) {
            const auto c =
                static_cast<std::uint8_t>(a * 30 + m * 10 + z * 80 + 1);
            b.insert(b.end(), {c, static_cast<std::uint8_t>(c + 1),
                               static_cast<std::uint8_t>(c + 2), 255});
          }
    }
  return b;
}
void Pump(Monstey_TexturePreviewWindow &view, unsigned milliseconds) {
  const auto end = GetTickCount64() + milliseconds;
  do {
    MSG m{};
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
      Check(m.message != WM_QUIT, "Texture window posted WM_QUIT");
      if (!view.RouteMessage(m)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
      }
    }
    Sleep(1);
  } while (GetTickCount64() < end);
}
void Wait(Monstey_TexturePreviewWindow &view,
          const std::function<bool(const J &)> &condition,
          unsigned timeout = 10000) {
  const auto end = GetTickCount64() + timeout;
  do {
    Pump(view, 5);
    if (condition(view.Inspect()))
      return;
  } while (GetTickCount64() < end);
  throw Monstey_Error("texture_window_test",
                      "Timeout waiting for native texture worker");
}
void Ready(Monstey_TexturePreviewWindow &view) {
  Wait(view, [](const J &r) { return !r.at("loading").as_bool(); });
  const auto r = view.Inspect();
  if (!r.at("ready").as_bool())
    throw Monstey_Error("texture_window_test",
                        "Texture not ready: " + r.at("error").as_string());
}
void Command(Monstey_TexturePreviewWindow &view, int id,
             WORD code = BN_CLICKED) {
  SendMessageW(view.Handle(), WM_COMMAND, MAKEWPARAM(id, code),
               reinterpret_cast<LPARAM>(GetDlgItem(view.Handle(), id)));
}
void Select(Monstey_TexturePreviewWindow &view, int index) {
  SendDlgItemMessageW(view.Handle(), 100, CB_SETCURSEL, index, 0);
  Command(view, 100, CBN_SELCHANGE);
  Ready(view);
}
std::array<unsigned, 3> Pixel(const B &bmp, int x, int y) {
  const auto width = Monstey_U32(bmp, 18);
  const auto at = 54 + (std::size_t(y) * width + static_cast<unsigned>(x)) * 4;
  Check(at + 4 <= bmp.size(), "Snapshot pixel outside bitmap");
  return {bmp[at], bmp[at + 1], bmp[at + 2]};
}
HWND Monstey_TestDialogOwner = nullptr;
unsigned Monstey_TestDialogCanceled = 0;
BOOL CALLBACK Monstey_TestCancelOwnedDialog(HWND hwnd, LPARAM) {
  wchar_t name[64]{};
  GetClassNameW(hwnd, name, 64);
  if (std::wstring(name) != L"#32770")
    return TRUE;
  bool owned = false;
  for (auto owner = GetWindow(hwnd, GW_OWNER); owner;
       owner = GetWindow(owner, GW_OWNER))
    owned |= owner == Monstey_TestDialogOwner;
  if (owned) {
    PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED),
                 reinterpret_cast<LPARAM>(GetDlgItem(hwnd, IDCANCEL)));
    ++Monstey_TestDialogCanceled;
  }
  return TRUE;
}
void CALLBACK Monstey_TestCancelDialogTimer(HWND, UINT, UINT_PTR, DWORD) {
  EnumThreadWindows(GetCurrentThreadId(), Monstey_TestCancelOwnedDialog, 0);
}
std::pair<int, int> ImagePoint(const J &r, unsigned x, unsigned y) {
  return {static_cast<int>(std::floor(r.at("image_origin")[0].as_number() +
                                      (x + 0.5) * r.at("scale").as_number())),
          static_cast<int>(std::floor(r.at("image_origin")[1].as_number() +
                                      (y + 0.5) * r.at("scale").as_number()))};
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  HWND owner = nullptr;
  try {
    Check(argc == 2, "Need fresh output directory");
    const std::filesystem::path dir = argv[1];
    Check(!std::filesystem::exists(dir), "Fresh output directory required");
    std::filesystem::create_directories(dir);
    auto basic = Dds();
    const B colors{0, 0,   255, 255, 255, 0,   0,   0,
                   0, 255, 0,   128, 0,   255, 255, 255};
    std::copy(colors.begin(), colors.end(), basic.begin() + 148);
    const auto source = dir / L"texture_\u00e9_\u6c34.dds";
    Monstey_WriteNewFile(source, basic);
    const auto hash = Monstey_HashSha256(source);
    owner = CreateWindowExW(0, L"STATIC", L"Owned texture test host",
                            WS_OVERLAPPEDWINDOW, 30, 30, 800, 600, nullptr,
                            nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(owner != nullptr, "Cannot create owned host");
    auto names = J::array();
    const auto test = [&](const char *name, auto f) {
      f();
      names.as_array().emplace_back(name);
    };
    Monstey_TexturePreviewWindow view;
    test(
        "Asynchronous explicit Unicode source loads with pinned metadata", [&] {
          const auto request = view.Open(owner, source, hash, SW_HIDE);
          Ready(view);
          const auto r = view.Inspect();
          Check(r.at("request") == J(request) &&
                    r.at("source_sha256") == J(hash) && r.at("width") == J(2) &&
                    r.at("height") == J(2) &&
                    r.at("native_material_rendering_verified") == J(false),
                "Source pin or decoded extent differs");
          Check(GetWindow(view.Handle(), GW_OWNER) == owner,
                "Preview is not owner-associated");
          Check(SendDlgItemMessageW(view.Handle(), 100, CB_GETCOUNT, 0, 0) == 1,
                "Subresource combo missing");
          wchar_t label[256]{};
          SendDlgItemMessageW(view.Handle(), 100, CB_GETLBTEXT, 0,
                              reinterpret_cast<LPARAM>(label));
          Check(std::wstring(label).find(
                    L"Mip 0 | Array 0 | Face 0 | 2 x 2 x 1") !=
                    std::wstring::npos,
                "Subresource label lacks exact data");
        });
    test("Owner monitor placement minimum controls and DPI changes stay within "
         "work area",
         [&] {
           MONITORINFO work{};
           work.cbSize = sizeof(work);
           Check(GetMonitorInfoW(
                     MonitorFromWindow(owner, MONITOR_DEFAULTTOPRIMARY),
                     &work) != FALSE,
                 "Cannot read owner work area");
           RECT original{};
           GetWindowRect(view.Handle(), &original);
           Check(original.left >= work.rcWork.left &&
                     original.top >= work.rcWork.top &&
                     original.right <= work.rcWork.right &&
                     original.bottom <= work.rcWork.bottom,
                 "Initial window escaped owner monitor");
           MINMAXINFO minimum{};
           SendMessageW(view.Handle(), WM_GETMINMAXINFO, 0,
                        reinterpret_cast<LPARAM>(&minimum));
           SetWindowPos(view.Handle(), nullptr, original.left, original.top,
                        minimum.ptMinTrackSize.x, minimum.ptMinTrackSize.y,
                        SWP_NOZORDER | SWP_NOACTIVATE);
           Pump(view, 30);
           RECT client{};
           GetClientRect(view.Handle(), &client);
           for (const auto id : {110, 111, 112, 100, 108, 109}) {
             const auto control = GetDlgItem(view.Handle(), id);
             RECT r{};
             GetWindowRect(control, &r);
             MapWindowPoints(nullptr, view.Handle(),
                             reinterpret_cast<POINT *>(&r), 2);
             Check(r.left >= 0 && r.right <= client.right && r.top >= 0 &&
                       r.bottom <= client.bottom && r.right > r.left,
                   "Control clipped at minimum size");
           }
           for (const auto id : {110, 112}) {
             const auto control = GetDlgItem(view.Handle(), id);
             Check((GetWindowLongPtrW(control, GWL_STYLE) & ES_READONLY) != 0,
                   "Source/hash not copyable read-only fields");
             SendMessageW(control, EM_SETSEL, 0, -1);
             DWORD start = 0, end = 0;
             SendMessageW(control, EM_GETSEL, reinterpret_cast<WPARAM>(&start),
                          reinterpret_cast<LPARAM>(&end));
             Check(start == 0 &&
                       end == static_cast<DWORD>(GetWindowTextLengthW(control)),
                   "Source/hash full text not selectable");
           }
           const auto realDpi = GetDpiForWindow(view.Handle());
           RECT proposed{work.rcWork.left - 300, work.rcWork.top - 300,
                         work.rcWork.right + 300, work.rcWork.bottom + 300};
           SendMessageW(view.Handle(), WM_DPICHANGED, MAKEWPARAM(144, 144),
                        reinterpret_cast<LPARAM>(&proposed));
           RECT actual{};
           GetWindowRect(view.Handle(), &actual);
           MONITORINFO target{};
           target.cbSize = sizeof(target);
           Check(GetMonitorInfoW(
                     MonitorFromWindow(view.Handle(), MONITOR_DEFAULTTONEAREST),
                     &target) != FALSE,
                 "Cannot read target work area");
           Check(actual.left >= target.rcWork.left &&
                     actual.top >= target.rcWork.top &&
                     actual.right <= target.rcWork.right &&
                     actual.bottom <= target.rcWork.bottom &&
                     view.Inspect().at("dpi") == J(144),
                 "DPI rectangle not clamped");
           SendMessageW(view.Handle(), WM_DPICHANGED,
                        MAKEWPARAM(realDpi, realDpi),
                        reinterpret_cast<LPARAM>(&original));
           Pump(view, 30);
           Check(view.Inspect().at("ready") == J(true),
                 "DPI change lost decoded image");
         });
    test("Actual paint pixels and raw hover distinguish opaque transparent and "
         "half alpha",
         [&] {
           auto r = view.Inspect();
           auto point = ImagePoint(r, 0, 0);
           SendMessageW(view.Handle(), WM_MOUSEMOVE, 0,
                        MAKELPARAM(point.first, point.second));
           auto bmp = view.Snapshot();
           Check(Pixel(bmp, point.first, point.second) ==
                     std::array<unsigned, 3>{0, 0, 255},
                 "Opaque painted pixel differs");
           Check(view.Inspect().at("hover").at("rgba8") ==
                     J::array({255, 0, 0, 255}),
                 "Raw hover pixel differs");
           point = ImagePoint(r, 1, 0);
           const auto transparent = Pixel(bmp, point.first, point.second);
           Check(transparent[0] == transparent[1] &&
                     transparent[1] == transparent[2] &&
                     (transparent[0] == 56 || transparent[0] == 88),
                 "Transparent pixel did not expose checkerboard");
           SendDlgItemMessageW(view.Handle(), 107, BM_SETCHECK, BST_UNCHECKED,
                               0);
           Command(view, 107);
           bmp = view.Snapshot();
           Check(Pixel(bmp, point.first, point.second) ==
                         std::array<unsigned, 3>{255, 0, 0} &&
                     !view.Inspect().at("composite_alpha").as_bool(),
                 "RGB-only toggle changed raw color");
           Monstey_WriteNewFile(dir / L"synthetic-view.bmp", bmp);
         });
    test("Fit one-to-one zoom and real keyboard routing remain local", [&] {
      Command(view, 104);
      Check(view.Inspect().at("scale") == J(1.0) &&
                !view.Inspect().at("fit").as_bool(),
            "1:1 command lost");
      Command(view, 106);
      Check(view.Inspect().at("scale") == J(2.0), "Zoom-in command lost");
      Command(view, 105);
      Check(view.Inspect().at("scale") == J(1.0), "Zoom-out command lost");
      Command(view, 103);
      Check(view.Inspect().at("fit").as_bool(), "Fit command lost");
      MSG unrelated{};
      unrelated.hwnd = owner;
      unrelated.message = WM_KEYDOWN;
      unrelated.wParam = 'O';
      Check(!view.RouteMessage(unrelated),
            "Viewer consumed owner keyboard input");
      MSG local{};
      local.hwnd = GetDlgItem(view.Handle(), 101);
      local.message = WM_KEYDOWN;
      local.wParam = VK_TAB;
      Check(view.RouteMessage(local), "Viewer child keyboard input escaped");
    });
    test("Focused child Ctrl shortcuts never reach the parent and save uses "
         "the PNG dialog",
         [&] {
           std::array<BYTE, 256> original{};
           Check(GetKeyboardState(original.data()) != FALSE,
                 "Cannot read UI-thread key state");
           auto keys = original;
           keys[VK_CONTROL] = 0x80;
           Check(SetKeyboardState(keys.data()) != FALSE,
                 "Cannot set own UI-thread Ctrl state");
           const auto before = view.Inspect().at("request");
           MSG key{};
           key.hwnd = GetDlgItem(view.Handle(), 112);
           key.message = WM_KEYDOWN;
           key.wParam = 'O';
           Check(view.RouteMessage(key) &&
                     view.Inspect().at("request") == before,
                 "Ctrl+O escaped or changed source");
           ShowWindow(view.Handle(), SW_SHOWNOACTIVATE);
           const auto combo = GetDlgItem(view.Handle(), 100);
           SendMessageW(combo, CB_SHOWDROPDOWN, TRUE, 0);
           COMBOBOXINFO popup{};
           popup.cbSize = sizeof(popup);
           Check(GetComboBoxInfo(combo, &popup) != FALSE,
                 "Cannot inspect owned combo popup");
           key.hwnd = popup.hwndList;
           key.wParam = 'O';
           Check(view.RouteMessage(key), "Owned combo popup Ctrl+O escaped");
           key.wParam = VK_ESCAPE;
           Check(view.RouteMessage(key) && view.Handle() &&
                     SendMessageW(combo, CB_GETDROPPEDSTATE, 0, 0) == FALSE,
                 "Popup Escape closed viewer or escaped");
           key.hwnd = GetDlgItem(view.Handle(), 112);
           Monstey_TestDialogOwner = view.Handle();
           Monstey_TestDialogCanceled = 0;
           const auto timer =
               SetTimer(nullptr, 0, 10, Monstey_TestCancelDialogTimer);
           Check(timer != 0, "Cannot install owned-dialog cancel timer");
           const auto count =
               view.Inspect().at("save_dialog_requests").as_uint();
           key.wParam = 'S';
           const auto routed = view.RouteMessage(key);
           KillTimer(nullptr, timer);
           SetKeyboardState(original.data());
           Monstey_TestDialogOwner = nullptr;
           Check(routed && Monstey_TestDialogCanceled > 0 &&
                     view.Inspect().at("save_dialog_requests").as_uint() ==
                         count + 1,
                 "Ctrl+S did not route to the owned PNG dialog");
           Check(view.Inspect().at("ready") == J(true),
                 "Canceled save dialog changed source view");
         });
    test("Real combo selection chooses exact array mip and encoded source "
         "remains intact",
         [&] {
           const auto array = dir / L"array.dds";
           const auto bytes = Dds(2, 2, 1, 2, 3, 2);
           Monstey_WriteNewFile(array, bytes);
           view.Open(owner, array, {}, SW_HIDE);
           Ready(view);
           Check(SendDlgItemMessageW(view.Handle(), 100, CB_GETCOUNT, 0, 0) ==
                     4,
                 "Array combo count differs");
           Select(view, 3);
           auto r = view.Inspect();
           Check(r.at("selected_subresource") == J(3) &&
                     r.at("width") == J(1) && r.at("height") == J(1),
                 "Selected array/mip extent differs");
           SendDlgItemMessageW(view.Handle(), 107, BM_SETCHECK, BST_UNCHECKED,
                               0);
           Command(view, 107);
           const auto xy = ImagePoint(r, 0, 0);
           Check(Pixel(view.Snapshot(), xy.first, xy.second) ==
                     std::array<unsigned, 3>{41, 42, 43},
                 "Combo selected wrong subresource pixels");
           Check(Monstey_ReadFile(array) == bytes, "Viewer edited source");
         });
    test("Volume depth edit apply and Enter use per-mip bounds", [&] {
      const auto volume = dir / L"volume.dds";
      Monstey_WriteNewFile(volume, Dds(2, 2, 2, 2, 4));
      view.Open(owner, volume, {}, SW_HIDE);
      Ready(view);
      Check(IsWindowEnabled(GetDlgItem(view.Handle(), 101)),
            "Volume depth selector disabled");
      SetDlgItemTextW(view.Handle(), 101, L"1");
      Command(view, 102);
      Ready(view);
      auto r = view.Inspect();
      Check(r.at("depth_slice") == J(1), "Depth command lost");
      const auto xy = ImagePoint(r, 0, 0);
      Check(Pixel(view.Snapshot(), xy.first, xy.second) ==
                std::array<unsigned, 3>{81, 82, 83},
            "Wrong volume depth pixels");
      Select(view, 1);
      Check(view.Inspect().at("depth_slice") == J(0) &&
                !IsWindowEnabled(GetDlgItem(view.Handle(), 101)),
            "Depth did not reset for one-slice mip");
      Select(view, 0);
      SetDlgItemTextW(view.Handle(), 101, L"2");
      Command(view, 102);
      Check(!view.Inspect().at("error").as_string().empty() &&
                view.Inspect().at("depth_slice") == J(0),
            "Out-of-bounds depth was applied");
      SetDlgItemTextW(view.Handle(), 101, L"1");
      MSG key{};
      key.hwnd = GetDlgItem(view.Handle(), 101);
      key.message = WM_KEYDOWN;
      key.wParam = VK_RETURN;
      Check(view.RouteMessage(key), "Depth Enter not consumed");
      Ready(view);
      Check(view.Inspect().at("depth_slice") == J(1),
            "Depth Enter not applied");
      Monstey_WriteNewFile(dir / L"volume-view.bmp", view.Snapshot());
    });
    test("Nonfinite R32 pixels stay safe in paint hover and serialized Inspect "
         "JSON",
         [&] {
           auto bytes = Dds(3, 1, 1, 1, 3, 1, 41);
           Monstey_PutU32(bytes, 148, 0x7fc12345);
           Monstey_PutU32(bytes, 152, 0x7f800000);
           Monstey_PutU32(bytes, 156, 0xff800000);
           const auto file = dir / L"nonfinite.dds";
           Monstey_WriteNewFile(file, bytes);
           view.Open(owner, file, {}, SW_HIDE);
           Ready(view);
           SendDlgItemMessageW(view.Handle(), 107, BM_SETCHECK, BST_UNCHECKED,
                               0);
           Command(view, 107);
           for (unsigned i = 0; i < 3; ++i) {
             const auto point = ImagePoint(view.Inspect(), i, 0);
             SendMessageW(view.Handle(), WM_MOUSEMOVE, 0,
                          MAKELPARAM(point.first, point.second));
             const auto report = view.Inspect();
             const auto again = J::Parse(report.dump());
             Check(again.at("hover").at("raw_rgba_text").is_string() &&
                       again.at("hover").at("rgba8") ==
                           J::array({i == 1 ? 255 : 0, 0, 0, 255}),
                   "Nonfinite pixel entered numeric JSON or quantization "
                   "differed");
             Check(Pixel(view.Snapshot(), point.first, point.second) ==
                       std::array<unsigned, 3>{0, 0, i == 1 ? 255U : 0U},
                   "Nonfinite painted pixel mismatch");
           }
           view.Open(owner, dir / L"volume.dds", {}, SW_HIDE);
           Ready(view);
           SetDlgItemTextW(view.Handle(), 101, L"1");
           Command(view, 102);
           Ready(view);
         });
    test("Queued PNG export uses pinned SHA and exclusive reviewed publisher",
         [&] {
           const auto output = dir / L"export_\u00e9_\u6c34.png";
           Check(view.SavePng(output), "Save action not accepted");
           Wait(view,
                [](const J &r) { return !r.at("save_pending").as_bool(); });
           auto r = view.Inspect();
           Check(
               r.at("error").as_string().empty() &&
                   std::filesystem::exists(output) &&
                   r.at("last_save").at("source_verified_before_publication") ==
                       J(true) &&
                   r.at("last_save").at("depth_slice") == J(1),
               "Pinned export failed");
           const auto bytes = Monstey_ReadFile(output);
           Check(view.SavePng(output),
                 "Repeated export request not accepted for error reporting");
           Wait(view,
                [](const J &v) { return !v.at("save_pending").as_bool(); });
           Check(!view.Inspect().at("error").as_string().empty() &&
                     Monstey_ReadFile(output) == bytes,
                 "Existing output overwritten");
         });
    test(
        "Rapid loads discard obsolete results without stale metadata or images",
        [&] {
          const auto volume = dir / L"volume.dds";
          const auto old = view.Open(owner, volume, {}, SW_HIDE);
          const auto fresh = view.Open(owner, source, hash, SW_HIDE);
          Ready(view);
          const auto r = view.Inspect();
          Check(fresh > old && r.at("request") == J(fresh) &&
                    r.at("source_sha256") == J(hash) && r.at("width") == J(2),
                "Obsolete load changed current source");
        });
    test("Close while CPU decode is active is immediate and late completion is "
         "harmless",
         [&] {
           const auto large = dir / L"large.dds";
           Monstey_WriteNewFile(large, Dds(4096, 4096, 1, 1, 3, 1, 71));
           view.Open(owner, large, {}, SW_HIDE);
           Wait(view, [](const J &r) { return r.at("worker_phase") == J(2); });
           const auto old = view.Handle();
           const auto before = GetTickCount64();
           SendMessageW(old, WM_CLOSE, 0, 0);
           const auto elapsed = GetTickCount64() - before;
           Check(elapsed < 250 && !view.Handle() && !IsWindow(old) &&
                     IsWindow(owner),
                 "Close waited for worker or destroyed owner");
           view.Open(owner, source, hash, SW_HIDE);
           Ready(view);
           Check(view.Inspect().at("source_sha256") == J(hash) &&
                     view.Inspect().at("width") == J(2),
                 "Late closed-generation image was accepted");
         });
    test("Destruction during active decode joins owned work without "
         "use-after-free or deadlock",
         [&] {
           const auto before = GetTickCount64();
           {
             Monstey_TexturePreviewWindow pending;
             pending.Open(owner, dir / L"large.dds", {}, SW_HIDE);
             Wait(pending,
                  [](const J &r) { return r.at("worker_phase") == J(2); });
           }
           Check(GetTickCount64() - before < 10000 && IsWindow(owner),
                 "Owned worker teardown did not finish");
           Pump(view, 50);
         });
    test("Owner destruction during decode destroys only its owned viewer and "
         "drains safely",
         [&] {
           auto host = CreateWindowExW(
               0, L"STATIC", L"Secondary test owner", WS_OVERLAPPEDWINDOW, 0, 0,
               500, 400, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
           Check(host != nullptr, "Secondary owner failed");
           {
             Monstey_TexturePreviewWindow owned;
             owned.Open(host, dir / L"large.dds", {}, SW_HIDE);
             Wait(owned,
                  [](const J &r) { return r.at("worker_phase") == J(2); });
             DestroyWindow(host);
             host = nullptr;
             Check(!owned.Handle(),
                   "Owner destruction left owned window alive");
           }
           Check(IsWindow(owner), "Other owner was affected");
         });
    test("Bad source wrong pin and unqualified data show inline errors without "
         "a fake image",
         [&] {
           view.Open(owner, dir / L"missing.dds", {}, SW_HIDE);
           Wait(view, [](const J &r) { return !r.at("loading").as_bool(); });
           Check(!view.Inspect().at("ready").as_bool() &&
                     !view.Inspect().at("error").as_string().empty(),
                 "Missing source silently accepted");
           view.Open(owner, source, std::string(64, '0'), SW_HIDE);
           Wait(view, [](const J &r) { return !r.at("loading").as_bool(); });
           Check(!view.Inspect().at("ready").as_bool() &&
                     !view.Inspect().at("error").as_string().empty(),
                 "Wrong source pin silently accepted");
           B raw(8);
           Monstey_PutU32(raw, 0, 0x54555043);
           Monstey_PutU32(raw, 4, 99);
           const auto opaque = dir / L"opaque.cput";
           Monstey_WriteNewFile(opaque, raw);
           view.Open(owner, opaque, {}, SW_HIDE);
           Wait(view, [](const J &r) { return !r.at("loading").as_bool(); });
           auto r = view.Inspect();
           Check(!r.at("ready").as_bool() && r.at("source_loaded").as_bool() &&
                     !r.at("description").at("decode_supported").as_bool() &&
                     !r.at("error").as_string().empty() &&
                     !IsWindowEnabled(GetDlgItem(view.Handle(), 108)),
                 "Opaque data displayed fabricated RGBA");
           Monstey_WriteNewFile(dir / L"unsupported-view.bmp", view.Snapshot());
         });
    test("Native source changes invalidate PNG export instead of changing the "
         "pinned display",
         [&] {
           const auto p = dir / L"changed.dds";
           Monstey_WriteNewFile(p, basic);
           view.Open(owner, p, {}, SW_HIDE);
           Ready(view);
           auto changed = basic;
           changed.back() ^= 1;
           HANDLE f =
               CreateFileW(Monstey_WindowsFilePath(p).c_str(), GENERIC_WRITE, 0,
                           nullptr, OPEN_EXISTING, 0, nullptr);
           Check(f != INVALID_HANDLE_VALUE,
                 "Cannot mutate audit-owned fixture");
           DWORD written = 0;
           const auto ok =
               WriteFile(f, changed.data(), static_cast<DWORD>(changed.size()),
                         &written, nullptr);
           CloseHandle(f);
           Check(ok && written == changed.size(),
                 "Owned fixture mutation failed");
           const auto out = dir / L"stale.png";
           Check(view.SavePng(out), "Pinned save request not accepted");
           Wait(view,
                [](const J &r) { return !r.at("save_pending").as_bool(); });
           Check(!view.Inspect().at("error").as_string().empty() &&
                     !std::filesystem::exists(out) &&
                     Monstey_ReadFile(p) == changed,
                 "Stale source was exported or overwritten");
         });
    test("Escape without an open popup closes only the viewer", [&] {
      MSG key{};
      key.hwnd = GetDlgItem(view.Handle(), 112);
      key.message = WM_KEYDOWN;
      key.wParam = VK_ESCAPE;
      Check(view.RouteMessage(key) && !view.Handle() && IsWindow(owner),
            "No-popup Escape failed or destroyed owner");
    });
    view.Open(owner, source, hash, SW_SHOWNORMAL);
    Ready(view);
    Command(view, 103);
    Pump(view, 100);
    Monstey_WriteNewFile(dir / L"texture-viewer.bmp", view.Snapshot());
    Check(Monstey_ReadFile(source) == basic &&
              Monstey_HashSha256(source) == hash,
          "Original source changed");
    const auto final = view.Inspect();
    view.Close();
    Check(IsWindow(owner), "Closing component destroyed owner");
    DestroyWindow(owner);
    owner = nullptr;
    const auto result = J::object(
        {{"status", "PASS"},
         {"groups", names.size()},
         {"tests", names},
         {"final_view", final},
         {"screenshots", J::array({"texture-viewer.bmp", "synthetic-view.bmp",
                                   "volume-view.bmp", "unsupported-view.bmp"})},
         {"original_or_GPU_code_executed", false},
         {"N_or_shared_B_written", false}});
    Monstey_WriteNewText(dir / L"report.json", result.dump(2));
    std::cout << result.dump(2) << '\n';
    return 0;
  } catch (const std::exception &e) {
    if (owner)
      DestroyWindow(owner);
    std::cerr << e.what() << '\n';
    return 1;
  }
}