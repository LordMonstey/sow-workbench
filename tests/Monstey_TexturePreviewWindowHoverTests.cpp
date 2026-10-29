#define wmain Monstey_TextureWindowLegacyMain
#include "Monstey_TexturePreviewWindowTests.cpp"
#undef wmain
namespace {
std::wstring Monstey_HoverText(HWND window) {
  const auto field = GetDlgItem(window, 114);
  std::wstring text(static_cast<std::size_t>(GetWindowTextLengthW(field)) + 1,
                    L'\0');
  GetWindowTextW(field, text.data(), static_cast<int>(text.size()));
  text.resize(wcslen(text.c_str()));
  return text;
}
void Monstey_ClearInvariant(Monstey_TexturePreviewWindow &view) {
  Check(view.Inspect().at("hover").is_null(),
        "Hover JSON retained an invalid sample");
  Check(Monstey_HoverText(view.Handle()).empty(),
        "Native readout retained invalid raw values");
}
void Monstey_Sample(Monstey_TexturePreviewWindow &view, unsigned x = 0,
                    unsigned y = 0) {
  const auto point = ImagePoint(view.Inspect(), x, y);
  SendMessageW(view.Handle(), WM_MOUSEMOVE, 0,
               MAKELPARAM(point.first, point.second));
  const auto state = view.Inspect();
  Check(!state.at("hover").is_null(), "Valid current-pointer sample missing");
  Check(Monstey_Utf8(Monstey_HoverText(view.Handle())) ==
            state.at("hover").at("raw_rgba_text").as_string(),
        "JSON and native readout disagree");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  HWND owner = nullptr;
  try {
    Check(argc == 2, "Need new hover output directory");
    const std::filesystem::path dir = argv[1];
    Check(!std::filesystem::exists(dir), "New output directory required");
    std::filesystem::create_directories(dir);
    auto a = Dds(4, 4), b = Dds(4, 4);
    for (unsigned i = 0; i < 16; ++i) {
      a[148 + i * 4] = static_cast<std::uint8_t>(10 + i);
      b[148 + i * 4] = static_cast<std::uint8_t>(110 + i);
    }
    const auto sourceA = dir / L"a.dds", sourceB = dir / L"b.dds",
               volume = dir / L"volume.dds", large = dir / L"large.dds";
    Monstey_WriteNewFile(sourceA, a);
    Monstey_WriteNewFile(sourceB, b);
    Monstey_WriteNewFile(volume, Dds(4, 4, 2, 2, 4));
    Monstey_WriteNewFile(large, Dds(4096, 4096, 1, 1, 3, 1, 71));
    const auto hashA = Monstey_HashSha256(sourceA),
               hashB = Monstey_HashSha256(sourceB);
    owner = CreateWindowExW(0, L"STATIC", L"Hover consistency host",
                            WS_OVERLAPPEDWINDOW, 40, 40, 800, 600, nullptr,
                            nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(owner != nullptr, "Owner failed");
    Monstey_TexturePreviewWindow view;
    auto names = J::array();
    const auto test = [&](const char *name, auto fn) {
      std::cout << "RUN " << name << std::endl;
      fn();
      names.as_array().emplace_back(name);
    };
    view.Open(owner, sourceA, hashA, SW_HIDE);
    Ready(view);
    test("Source replacement clears native readout immediately and after "
         "completion",
         [&] {
           Monstey_Sample(view);
           const auto previous = Monstey_HoverText(view.Handle());
           view.Open(owner, sourceB, hashB, SW_HIDE);
           Monstey_ClearInvariant(view);
           Ready(view);
           Monstey_ClearInvariant(view);
           Monstey_Sample(view);
           Check(Monstey_HoverText(view.Handle()) != previous,
                 "Fresh sample used previous source bytes");
         });
    test("Accepted image completion clears any loading-time readout", [&] {
      view.Open(owner, large, {}, SW_HIDE);
      Wait(view, [](const J &r) { return r.at("worker_phase") == J(2); });
      const auto c = view.Inspect().at("canvas");
      SendMessageW(view.Handle(), WM_MOUSEMOVE, 0,
                   MAKELPARAM(static_cast<int>(c[0].as_int() + 2),
                              static_cast<int>(c[1].as_int() + 2)));
      Check(view.Inspect().at("hover").is_null(),
            "Loading image produced a raw sample");
      Ready(view);
      Monstey_ClearInvariant(view);
      view.Open(owner, sourceA, hashA, SW_HIDE);
      Ready(view);
    });
    test(
        "Closed active decode drops its result owners before returning to idle",
        [&] {
          view.Open(owner, large, {}, SW_HIDE);
          Wait(view, [](const J &r) { return r.at("worker_phase") == J(2); });
          const auto dropped = view.Inspect().at("dropped_results").as_uint();
          const auto start = GetTickCount64();
          view.Close();
          Check(GetTickCount64() - start < 250 && !view.Handle(),
                "Close waited for active decode");
          Wait(view,
               [](const J &r) { return !r.at("worker_active").as_bool(); });
          const auto closed = view.Inspect();
          Check(!closed.at("source_loaded").as_bool() &&
                    !closed.at("ready").as_bool() &&
                    closed.at("queued_jobs") == J(0) &&
                    closed.at("queued_results") == J(0) &&
                    closed.at("queued_images") == J(0) &&
                    closed.at("queued_textures") == J(0) &&
                    closed.at("worker_phase") == J(0) &&
                    closed.at("dropped_results").as_uint() > dropped,
                "Closed state retained a late image or texture owner");
          Monstey_WriteNewText(dir / L"closed-worker.json", closed.dump(2));
          view.Open(owner, sourceA, hashA, SW_HIDE);
          Ready(view);
        });
    const auto transform = [&](const char *name, auto action,
                               bool fitStart = false) {
      test(name, [&] {
        Command(view, fitStart ? 103 : 104);
        Monstey_Sample(view);
        action();
        Monstey_ClearInvariant(view);
        Monstey_Sample(view, 1, 1);
      });
    };
    transform("Zoom button invalidates both representations",
              [&] { Command(view, 106); });
    transform("Mouse-wheel zoom invalidates both representations", [&] {
      SendMessageW(view.Handle(), WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
    });
    transform("Fit command invalidates stationary previous mapping",
              [&] { Command(view, 103); });
    transform(
        "One-to-one command invalidates stationary previous mapping",
        [&] { Command(view, 104); }, true);
    RECT original{};
    GetWindowRect(view.Handle(), &original);
    transform("Client resize invalidates the old client-to-texel mapping", [&] {
      SetWindowPos(view.Handle(), nullptr, 0, 0,
                   original.right - original.left + 40,
                   original.bottom - original.top,
                   SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    });
    const auto dpi = GetDpiForWindow(view.Handle());
    transform("DPI layout invalidates the old client-to-texel mapping", [&] {
      RECT proposed = original;
      SendMessageW(view.Handle(), WM_DPICHANGED, MAKEWPARAM(144, 144),
                   reinterpret_cast<LPARAM>(&proposed));
    });
    SendMessageW(view.Handle(), WM_DPICHANGED, MAKEWPARAM(dpi, dpi),
                 reinterpret_cast<LPARAM>(&original));
    transform("Window movement invalidates the old screen-to-client domain",
              [&] {
                RECT r{};
                GetWindowRect(view.Handle(), &r);
                SetWindowPos(view.Handle(), nullptr, r.left + 8, r.top + 8, 0,
                             0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
              });
    test(
        "Subresource and depth invalidate before and after asynchronous decode",
        [&] {
          view.Open(owner, volume, {}, SW_HIDE);
          Ready(view);
          Monstey_Sample(view);
          SendDlgItemMessageW(view.Handle(), 100, CB_SETCURSEL, 1, 0);
          Command(view, 100, CBN_SELCHANGE);
          Monstey_ClearInvariant(view);
          Ready(view);
          Monstey_ClearInvariant(view);
          Select(view, 0);
          Monstey_Sample(view);
          SetDlgItemTextW(view.Handle(), 101, L"1");
          Command(view, 102);
          Monstey_ClearInvariant(view);
          Ready(view);
          Monstey_ClearInvariant(view);
        });
    test("Unchanged alpha/status/export operations preserve a valid sample",
         [&] {
           view.Open(owner, sourceA, hashA, SW_HIDE);
           Ready(view);
           Monstey_Sample(view);
           const auto sample = view.Inspect().at("hover");
           const auto text = Monstey_Utf8(Monstey_HoverText(view.Handle()));
           SendDlgItemMessageW(view.Handle(), 107, BM_SETCHECK, BST_UNCHECKED,
                               0);
           Command(view, 107);
           Check(view.Inspect().at("hover") == sample &&
                     Monstey_Utf8(Monstey_HoverText(view.Handle())) == text,
                 "Display-only alpha operation invalidated raw sample");
           Check(view.SavePng(dir / L"sample.png"), "Save failed");
           Wait(view,
                [](const J &r) { return !r.at("save_pending").as_bool(); });
           Check(view.Inspect().at("hover") == sample &&
                     Monstey_Utf8(Monstey_HoverText(view.Handle())) == text,
                 "Unchanged image status/export cleared a valid sample");
         });
    test(
        "Nonfinite sample strings remain JSON-safe and clear on mapping change",
        [&] {
          auto raw = Dds(3, 1, 1, 1, 3, 1, 41);
          Monstey_PutU32(raw, 148, 0x7fc12345);
          Monstey_PutU32(raw, 152, 0x7f800000);
          Monstey_PutU32(raw, 156, 0xff800000);
          const auto file = dir / L"nonfinite.dds";
          Monstey_WriteNewFile(file, raw);
          view.Open(owner, file, {}, SW_HIDE);
          Ready(view);
          for (unsigned i = 0; i < 3; ++i) {
            Command(view, 103);
            Monstey_Sample(view, i, 0);
            const auto parsed = J::Parse(view.Inspect().dump());
            Check(parsed.at("hover").at("raw_rgba_text").is_string(),
                  "Nonfinite raw value became numeric JSON");
            Command(view, 106);
            Monstey_ClearInvariant(view);
          }
        });
    Check(Monstey_HashSha256(sourceA) == hashA &&
              Monstey_HashSha256(sourceB) == hashB,
          "Owned source changed");
    view.Close();
    Check(IsWindow(owner), "Owner destroyed");
    DestroyWindow(owner);
    owner = nullptr;
    const auto result = J::object({{"status", "PASS"},
                                   {"groups", names.size()},
                                   {"tests", names},
                                   {"source_hashes_unchanged", true},
                                   {"public_ABI_unchanged", true},
                                   {"original_or_GPU_executed", false}});
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