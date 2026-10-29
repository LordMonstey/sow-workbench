#include "Monstey_Theme.hpp"
#include <algorithm>
#include <commctrl.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>
using namespace Monstey;
namespace {
unsigned groups = 0, clicks = 0;
void Check(bool ok, const char *text) {
  if (!ok)
    throw std::runtime_error(text);
}
LRESULT CALLBACK HostProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  if (m == WM_COMMAND && HIWORD(w) == BN_CLICKED) {
    ++clicks;
    return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
void Pump() {
  MSG msg{};
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
}
HWND Host() {
  auto h =
      CreateWindowExW(0, L"Monstey_SoWThemeReview", L"Native Dark Theme Test",
                      WS_OVERLAPPEDWINDOW, 0, 0, 1000, 700, nullptr, nullptr,
                      GetModuleHandleW(nullptr), nullptr);
  Check(h != nullptr, "host creation");
  return h;
}
HWND Child(HWND h, const wchar_t *kind, const wchar_t *text, DWORD flags,
           int id, int x, int y, int width = 220, int height = 32) {
  const std::wstring_view className(kind);
  const DWORD originalEdge = className == L"EDIT" ||
                                     className == WC_LISTVIEWW ||
                                     className == WC_TREEVIEWW
                                 ? WS_EX_CLIENTEDGE
                                 : 0;
  auto c = CreateWindowExW(
      originalEdge, kind, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | flags, x,
      y, width, height, h, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      GetModuleHandleW(nullptr), nullptr);
  Check(c != nullptr, "child creation");
  SendMessageW(c, WM_SETFONT,
               reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
               FALSE);
  return c;
}
struct Bitmap {
  HDC dc{};
  HBITMAP bitmap{}, old{};
  std::uint32_t *bits{};
  int width{}, height{};
  explicit Bitmap(HWND h, UINT printMessage = WM_PRINTCLIENT) {
    RECT r{};
    GetClientRect(h, &r);
    width = r.right;
    height = r.bottom;
    Check(width > 0 && height > 0, "snapshot extent");
    dc = CreateCompatibleDC(nullptr);
    Check(dc != nullptr, "memory DC");
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS,
                              reinterpret_cast<void **>(&bits), nullptr, 0);
    Check(bitmap != nullptr, "DIB creation");
    old = static_cast<HBITMAP>(SelectObject(dc, bitmap));
    std::fill(bits, bits + static_cast<std::size_t>(width) * height,
              0x00ffffffU);
    SendMessageW(h, printMessage, reinterpret_cast<WPARAM>(dc),
                 PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    GdiFlush();
  }
  ~Bitmap() {
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
  }
  COLORREF Pixel(int x, int y) const {
    auto p = bits[static_cast<std::size_t>(y) * width + x];
    return RGB((p >> 16) & 255, (p >> 8) & 255, p & 255);
  }
  std::size_t Count(COLORREF color) const {
    std::size_t n = 0;
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x)
        if (Pixel(x, y) == color)
          ++n;
    return n;
  }
  void Save(const std::filesystem::path &p) const {
    BITMAPFILEHEADER file{};
    BITMAPINFOHEADER info{};
    const auto bytes = static_cast<DWORD>(width * height * 4);
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + bytes;
    info.biSize = sizeof(info);
    info.biWidth = width;
    info.biHeight = -height;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    info.biSizeImage = bytes;
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char *>(&file), sizeof(file));
    out.write(reinterpret_cast<const char *>(&info), sizeof(info));
    out.write(reinterpret_cast<const char *>(bits), bytes);
    Check(out.good(), "bitmap write");
  }
};
COLORREF BrushColor(HBRUSH b) {
  LOGBRUSH brush{};
  Check(b && GetObjectW(b, sizeof(brush), &brush) == sizeof(brush),
        "valid brush");
  return brush.lbColor;
}
void ColorMessage(HWND host, HWND child, UINT message, COLORREF expected) {
  const auto dc = CreateCompatibleDC(nullptr);
  Check(dc != nullptr, "color DC");
  const auto brush = reinterpret_cast<HBRUSH>(
      SendMessageW(host, message, reinterpret_cast<WPARAM>(dc),
                   reinterpret_cast<LPARAM>(child)));
  Check(BrushColor(brush) == expected, "control brush palette");
  Check(GetBkColor(dc) == expected, "control DC background");
  Check(GetTextColor(dc) ==
            (IsWindowEnabled(child) ? Monstey_ThemeText : Monstey_ThemeMuted),
        "control DC text");
  DeleteDC(dc);
}
void Group() { ++groups; }
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Check(argc == 2, "usage: NativeThemeReview NEW_OUTPUT_DIR");
    const std::filesystem::path output = argv[1];
    Check(!std::filesystem::exists(output), "new output required");
    std::filesystem::create_directories(output);
    INITCOMMONCONTROLSEX cc{sizeof(cc), ICC_LISTVIEW_CLASSES |
                                            ICC_TREEVIEW_CLASSES |
                                            ICC_TAB_CLASSES | ICC_BAR_CLASSES};
    Check(InitCommonControlsEx(&cc) != FALSE, "common controls");
    WNDCLASSW cls{};
    cls.lpfnWndProc = HostProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"Monstey_SoWThemeReview";
    Check(RegisterClassW(&cls) != 0, "class registration");
    auto host = Host();
    auto edit =
        Child(host, L"EDIT", L"Editable text", ES_AUTOHSCROLL, 101, 20, 20);
    auto ro = Child(host, L"EDIT", L"Read-only text", ES_READONLY, 102, 20, 60);
    auto label = Child(host, L"STATIC", L"Native label", 0, 103, 20, 100);
    auto button = Child(host, L"BUTTON", L"Apply", BS_PUSHBUTTON, 104, 20, 140);
    auto check =
        Child(host, L"BUTTON", L"Enabled", BS_AUTOCHECKBOX, 105, 20, 180);
    auto combo = Child(host, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
                       106, 20, 220, 220, 180);
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"One"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Two"));
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
    auto tabs = Child(host, WC_TABCONTROLW, L"", 0, 107, 280, 20, 350, 200);
    TCITEMW ti{};
    ti.mask = TCIF_TEXT;
    wchar_t one[] = L"Database", two[] = L"World";
    ti.pszText = one;
    TabCtrl_InsertItem(tabs, 0, &ti);
    ti.pszText = two;
    TabCtrl_InsertItem(tabs, 1, &ti);
    auto list = Child(host, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL, 108,
                      280, 240, 350, 140);
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = one;
    col.cx = 250;
    ListView_InsertColumn(list, 0, &col);
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.pszText = two;
    ListView_InsertItem(list, &item);
    item.iItem = 1;
    item.pszText = one;
    ListView_InsertItem(list, &item);
    ListView_SetExtendedListViewStyle(list,
                                      LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    ListView_SetItemState(list, 0, LVIS_SELECTED, LVIS_SELECTED);
    auto header = ListView_GetHeader(list);
    auto tree =
        Child(host, WC_TREEVIEWW, L"", TVS_HASLINES, 109, 650, 20, 250, 250);
    TVINSERTSTRUCTW node{};
    node.hParent = TVI_ROOT;
    node.hInsertAfter = TVI_LAST;
    node.item.mask = TVIF_TEXT;
    node.item.pszText = one;
    Check(TreeView_InsertItem(tree, &node) != nullptr, "tree item");
    auto track = Child(host, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS, 110,
                       20, 280, 220, 32);
    SendMessageW(track, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessageW(track, TBM_SETPOS, TRUE, 33);
    Monstey_ThemeApply(host);
    Monstey_ThemeApply(host);
    Pump();
    for (const auto input : {edit, ro, list, tree}) {
      Check((GetWindowLongPtrW(input, GWL_EXSTYLE) & WS_EX_CLIENTEDGE) == 0,
            "bright native client edge removed");
      Check((GetWindowLongPtrW(input, GWL_STYLE) & WS_BORDER) != 0,
            "thin native control border retained");
    }
    Group();
    COMBOBOXINFO comboInfo{sizeof(comboInfo)};
    Check(GetComboBoxInfo(combo, &comboInfo) != FALSE &&
              comboInfo.hwndList != nullptr,
          "combo list window");
    ColorMessage(combo, comboInfo.hwndList, WM_CTLCOLORLISTBOX,
                 Monstey_ThemeSurface);
    Group();
    Check(BrushColor(Monstey_ThemeBackgroundBrush()) == Monstey_ThemeBackground,
          "background brush");
    Check(ListView_GetBkColor(list) == Monstey_ThemeBackground &&
              ListView_GetTextColor(list) == Monstey_ThemeText &&
              ListView_GetTextBkColor(list) == Monstey_ThemeBackground,
          "list palette");
    Check(TreeView_GetBkColor(tree) == Monstey_ThemeBackground &&
              TreeView_GetTextColor(tree) == Monstey_ThemeText &&
              TreeView_GetLineColor(tree) == Monstey_ThemeBorder,
          "tree palette");
    Group();
    ColorMessage(host, edit, WM_CTLCOLOREDIT, Monstey_ThemeSurface);
    ColorMessage(host, ro, WM_CTLCOLORSTATIC, Monstey_ThemeSurface);
    ColorMessage(host, label, WM_CTLCOLORSTATIC, Monstey_ThemeBackground);
    EnableWindow(edit, FALSE);
    ColorMessage(host, edit, WM_CTLCOLORSTATIC, Monstey_ThemeSurface);
    EnableWindow(edit, TRUE);
    Group();
    for (const auto &pair : std::vector<std::pair<HWND, const wchar_t *>>{
             {button, L"button.bmp"},
             {check, L"checkbox.bmp"},
             {combo, L"combo.bmp"},
             {tabs, L"tabs.bmp"},
             {header, L"header.bmp"},
             {track, L"trackbar.bmp"}}) {
      Bitmap b(pair.first);
      Check(b.Count(RGB(255, 255, 255)) == 0,
            "white pixels remain in themed custom paint");
      Check(b.Count(Monstey_ThemeSurface) + b.Count(Monstey_ThemeBackground) >
                static_cast<std::size_t>(b.width) * b.height / 3,
            "custom paint lacks dark surface");
      b.Save(output / pair.second);
    }
    Group();
    Check((ListView_GetExtendedListViewStyle(list) & LVS_EX_GRIDLINES) == 0,
          "list bright gridlines removed");
    {
      Bitmap b(list);
      RECT selected{}, unselected{};
      Check(ListView_GetItemRect(list, 0, &selected, LVIR_BOUNDS) != FALSE &&
                ListView_GetItemRect(list, 1, &unselected, LVIR_BOUNDS) !=
                    FALSE,
            "native list row rectangles");
      const int x = 180, selectedY = (selected.top + selected.bottom) / 2,
                unselectedY = (unselected.top + unselected.bottom) / 2;
      Check(b.Pixel(x, selectedY) == Monstey_ThemeSelection,
            "selected list row background");
      Check(b.Pixel(x, unselectedY) == Monstey_ThemeBackground,
            "unselected list row background");
      Check((ListView_GetItemState(list, 0, LVIS_SELECTED) & LVIS_SELECTED) !=
                    0 &&
                (ListView_GetItemState(list, 1, LVIS_SELECTED) &
                 LVIS_SELECTED) == 0,
            "list drawing preserves native selected state");
      b.Save(output / L"list-selected-unselected.bmp");
    }
    Group();
    for (const auto &pair : std::vector<std::pair<HWND, const wchar_t *>>{
             {button, L"button-wm-print.bmp"},
             {check, L"checkbox-wm-print.bmp"},
             {combo, L"combo-wm-print.bmp"},
             {tabs, L"tabs-wm-print.bmp"},
             {header, L"header-wm-print.bmp"},
             {track, L"trackbar-wm-print.bmp"}}) {
      Bitmap b(pair.first, WM_PRINT);
      Check(b.Count(RGB(255, 255, 255)) == 0,
            "white pixels remain in themed WM_PRINT");
      Check(b.Count(Monstey_ThemeSurface) + b.Count(Monstey_ThemeBackground) >
                static_cast<std::size_t>(b.width) * b.height / 3,
            "WM_PRINT lacks dark surface");
      b.Save(output / pair.second);
    }
    Group();
    auto beforeClicks = clicks;
    SendMessageW(button, BM_CLICK, 0, 0);
    Check(clicks == beforeClicks + 1, "button click semantic");
    Check(SendMessageW(check, BM_GETCHECK, 0, 0) == BST_UNCHECKED,
          "initial checkbox");
    SendMessageW(check, BM_CLICK, 0, 0);
    Check(SendMessageW(check, BM_GETCHECK, 0, 0) == BST_CHECKED,
          "native auto checkbox click");
    SendMessageW(check, BM_CLICK, 0, 0);
    Check(SendMessageW(check, BM_GETCHECK, 0, 0) == BST_UNCHECKED,
          "native auto checkbox toggle");
    Group();
    SendMessageW(button, WM_KEYDOWN, VK_SPACE, 0);
    Check((SendMessageW(button, BM_GETSTATE, 0, 0) & BST_PUSHED) != 0,
          "button native key down");
    SendMessageW(button, WM_KEYUP, VK_SPACE, 0);
    Check(clicks == beforeClicks + 4, "button native key up notification");
    Group();
    SendMessageW(combo, WM_KEYDOWN, VK_DOWN, 0);
    Check(SendMessageW(combo, CB_GETCURSEL, 0, 0) == 1,
          "combo native key navigation");
    Check(SendMessageW(combo, CB_GETCOUNT, 0, 0) == 2,
          "combo contents preserved");
    SendMessageW(edit, EM_SETSEL, 0, -1);
    SendMessageW(edit, EM_REPLACESEL, TRUE,
                 reinterpret_cast<LPARAM>(L"New text"));
    wchar_t current[32]{};
    GetWindowTextW(edit, current, 32);
    Check(std::wstring(current) == L"New text", "edit semantics");
    TabCtrl_SetCurSel(tabs, 1);
    Check(TabCtrl_GetCurSel(tabs) == 1, "tab selection semantic");
    SendMessageW(track, WM_KEYDOWN, VK_RIGHT, 0);
    Check(SendMessageW(track, TBM_GETPOS, 0, 0) == 34, "trackbar key semantic");
    Group();
    HMENU popup = CreatePopupMenu();
    Check(popup != nullptr, "popup create");
    Check(AppendMenuW(popup, MF_STRING, 201, L"Action") != FALSE, "popup item");
    MENUITEMINFOW mi{sizeof(mi)};
    mi.fMask = MIIM_DATA;
    mi.dwItemData = 0x12345678;
    Check(SetMenuItemInfoW(popup, 0, TRUE, &mi) != FALSE,
          "popup original data");
    for (unsigned i = 0; i < 8; ++i) {
      SendMessageW(host, WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(popup), 0);
      mi.fMask = MIIM_FTYPE | MIIM_DATA;
      Check(GetMenuItemInfoW(popup, 0, TRUE, &mi) != FALSE &&
                (mi.fType & MFT_OWNERDRAW) != 0 && mi.dwItemData != 0x12345678,
            "popup ownerdraw installed");
      SendMessageW(host, WM_UNINITMENUPOPUP, reinterpret_cast<WPARAM>(popup),
                   0);
      Check(GetMenuItemInfoW(popup, 0, TRUE, &mi) != FALSE &&
                (mi.fType & MFT_OWNERDRAW) == 0 && mi.dwItemData == 0x12345678,
            "popup restored");
    }
    DestroyMenu(popup);
    Group();
    DestroyWindow(host);
    Pump();
    const auto gdiBefore = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS),
               userBefore =
                   GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    for (unsigned n = 0; n < 80; ++n) {
      host = Host();
      button = Child(host, L"BUTTON", L"Apply", BS_PUSHBUTTON, 1, 0, 0);
      combo = Child(host, L"COMBOBOX", L"", CBS_DROPDOWNLIST, 2, 0, 40);
      Child(host, L"EDIT", L"Text", 0, 3, 0, 80);
      for (unsigned k = 0; k < 6; ++k)
        Monstey_ThemeApply(host);
      {
        Bitmap a(button);
        Bitmap b(combo);
      }
      DestroyWindow(host);
      Pump();
    }
    const auto gdiAfter = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS),
               userAfter = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    Check(gdiAfter <= gdiBefore + 2, "GDI object growth");
    Check(userAfter <= userBefore + 2, "USER object growth");
    Group();
    std::ofstream report(output / L"report.json");
    report << "{\"status\":\"PASS\",\"groups\":" << groups
           << ",\"iterations\":80,\"gdi_before\":" << gdiBefore
           << ",\"gdi_after\":" << gdiAfter << ",\"user_before\":" << userBefore
           << ",\"user_after\":" << userAfter
           << ",\"original_asset_access\":false,\"process_memory_access\":"
              "false}\n";
    Check(report.good(), "report write");
    std::cout << "Native theme review PASS " << groups << " groups\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Native theme review FAIL: " << e.what() << "\n";
    return 1;
  }
}
