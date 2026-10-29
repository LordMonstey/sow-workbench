#include "Monstey_Theme.hpp"
#include <algorithm>
#include <commctrl.h>
#include <dwmapi.h>
#include <memory>
#include <string>
#include <uxtheme.h>
#include <vector>

namespace Monstey {
namespace {
constexpr UINT_PTR Monstey_ThemeSubclass = 0x4d54484d;
struct Monstey_ThemeBrushes {
  HBRUSH background = CreateSolidBrush(Monstey_ThemeBackground);
  HBRUSH surface = CreateSolidBrush(Monstey_ThemeSurface);
  ~Monstey_ThemeBrushes() {
    DeleteObject(background);
    DeleteObject(surface);
  }
};
Monstey_ThemeBrushes &Monstey_Brushes() {
  static Monstey_ThemeBrushes b;
  return b;
}
struct Monstey_MenuEntry {
  HMENU menu{};
  UINT position{}, type{};
  ULONG_PTR originalData{};
  bool top{};
  std::wstring text;
};
struct Monstey_ThemeState {
  std::wstring kind;
  bool hover = false;
  std::vector<std::unique_ptr<Monstey_MenuEntry>> menus;
  void RestoreMenu(HMENU menu) {
    std::erase_if(menus, [menu](const auto &entry) {
      if (entry->menu != menu)
        return false;
      MENUITEMINFOW current{sizeof(current)};
      current.fMask = MIIM_DATA;
      if (IsMenu(menu) &&
          GetMenuItemInfoW(menu, entry->position, TRUE, &current) &&
          current.dwItemData == reinterpret_cast<ULONG_PTR>(entry.get())) {
        current.fMask = MIIM_FTYPE | MIIM_DATA;
        current.fType = entry->type;
        current.dwItemData = entry->originalData;
        SetMenuItemInfoW(menu, entry->position, TRUE, &current);
      }
      return true;
    });
  }
  void Menu(HMENU menu, bool top) {
    if (!IsMenu(menu))
      return;
    RestoreMenu(menu);
    MENUINFO info{sizeof(info)};
    info.fMask = MIM_BACKGROUND;
    info.hbrBack = Monstey_Brushes().background;
    SetMenuInfo(menu, &info);
    for (int i = 0; i < GetMenuItemCount(menu); ++i) {
      MENUITEMINFOW item{sizeof(item)};
      item.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_DATA;
      if (!GetMenuItemInfoW(menu, static_cast<UINT>(i), TRUE, &item) ||
          (item.fType & MFT_OWNERDRAW))
        continue;
      auto entry = std::make_unique<Monstey_MenuEntry>();
      entry->menu = menu;
      entry->position = static_cast<UINT>(i);
      entry->type = item.fType;
      entry->originalData = item.dwItemData;
      entry->top = top;
      entry->text.resize(static_cast<std::size_t>(item.cch) + 1);
      item.dwTypeData = entry->text.data();
      ++item.cch;
      if (!GetMenuItemInfoW(menu, entry->position, TRUE, &item))
        continue;
      entry->text.resize(item.cch);
      item.fMask = MIIM_FTYPE | MIIM_DATA;
      item.fType |= MFT_OWNERDRAW;
      item.dwItemData = reinterpret_cast<ULONG_PTR>(entry.get());
      menus.push_back(std::move(entry));
      if (!SetMenuItemInfoW(menu, static_cast<UINT>(i), TRUE, &item))
        menus.pop_back();
    }
  }
  Monstey_MenuEntry *Entry(ULONG_PTR data) const {
    for (const auto &entry : menus)
      if (reinterpret_cast<ULONG_PTR>(entry.get()) == data)
        return entry.get();
    return nullptr;
  }
};
std::wstring Monstey_ThemeTextOf(HWND window) {
  std::wstring text(static_cast<std::size_t>(GetWindowTextLengthW(window)) + 1,
                    L'\0');
  const auto n =
      GetWindowTextW(window, text.data(), static_cast<int>(text.size()));
  text.resize(static_cast<std::size_t>(std::max(n, 0)));
  return text;
}
void Monstey_ThemeFill(HDC dc, const RECT &rect, COLORREF color) {
  SetDCBrushColor(dc, color);
  FillRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}
void Monstey_ThemeLabel(HDC dc, HWND window, const std::wstring &text,
                        RECT rect, UINT format) {
  const auto font =
      reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0));
  const auto old =
      SelectObject(dc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, IsWindowEnabled(window) ? Monstey_ThemeText
                                           : Monstey_ThemeMuted);
  DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rect, format);
  SelectObject(dc, old);
}
void Monstey_ThemePaint(HWND window, HDC dc, const Monstey_ThemeState &s) {
  const int saved = SaveDC(dc);
  RECT rect{};
  GetClientRect(window, &rect);
  Monstey_ThemeFill(dc, rect, Monstey_ThemeBackground);
  if (s.kind == WC_TABCONTROLW) {
    for (int i = 0; i < TabCtrl_GetItemCount(window); ++i) {
      RECT item{};
      TabCtrl_GetItemRect(window, i, &item);
      const bool selected = TabCtrl_GetCurSel(window) == i;
      Monstey_ThemeFill(
          dc, item, selected ? Monstey_ThemeSelection : Monstey_ThemeSurface);
      wchar_t text[256]{};
      TCITEMW tab{};
      tab.mask = TCIF_TEXT;
      tab.pszText = text;
      tab.cchTextMax = 256;
      TabCtrl_GetItem(window, i, &tab);
      Monstey_ThemeLabel(dc, window, text, item,
                         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      if (selected) {
        RECT line{item.left, item.bottom - 2, item.right, item.bottom};
        Monstey_ThemeFill(dc, line, RGB(99, 176, 255));
      }
      if (GetFocus() == window && TabCtrl_GetCurFocus(window) == i) {
        InflateRect(&item, -4, -4);
        DrawFocusRect(dc, &item);
      }
    }
  } else if (s.kind == WC_HEADERW) {
    Monstey_ThemeFill(dc, rect, Monstey_ThemeSurface);
    for (int i = 0; i < Header_GetItemCount(window); ++i) {
      RECT item{};
      Header_GetItemRect(window, i, &item);
      RECT edge{item.right - 1, item.top + 4, item.right, item.bottom - 4};
      Monstey_ThemeFill(dc, edge, Monstey_ThemeBorder);
      wchar_t text[512]{};
      HDITEMW header{};
      header.mask = HDI_TEXT | HDI_FORMAT;
      header.pszText = text;
      header.cchTextMax = 512;
      Header_GetItem(window, i, &header);
      InflateRect(&item, -8, 0);
      const UINT alignment = (header.fmt & HDF_CENTER)  ? DT_CENTER
                             : (header.fmt & HDF_RIGHT) ? DT_RIGHT
                                                        : DT_LEFT;
      Monstey_ThemeLabel(dc, window, text, item,
                         alignment | DT_VCENTER | DT_SINGLELINE |
                             DT_END_ELLIPSIS);
    }
  } else if (s.kind == TRACKBAR_CLASSW) {
    RECT channel{}, thumb{};
    SendMessageW(window, TBM_GETCHANNELRECT, 0,
                 reinterpret_cast<LPARAM>(&channel));
    SendMessageW(window, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
    Monstey_ThemeFill(dc, channel, Monstey_ThemeBorder);
    Monstey_ThemeFill(dc, thumb,
                      IsWindowEnabled(window) ? RGB(99, 176, 255)
                                              : Monstey_ThemeMuted);
  } else if (s.kind == L"BUTTON") {
    const auto style = GetWindowLongPtrW(window, GWL_STYLE) & BS_TYPEMASK;
    const auto state = SendMessageW(window, BM_GETSTATE, 0, 0);
    if (style == BS_CHECKBOX || style == BS_AUTOCHECKBOX ||
        style == BS_RADIOBUTTON || style == BS_AUTORADIOBUTTON) {
      RECT box{rect.left + 2, (rect.bottom - 16) / 2, rect.left + 18,
               (rect.bottom + 16) / 2};
      Monstey_ThemeFill(dc, box, Monstey_ThemeSurface);
      SetDCBrushColor(dc, Monstey_ThemeBorder);
      FrameRect(dc, &box, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
      if (SendMessageW(window, BM_GETCHECK, 0, 0) != BST_UNCHECKED) {
        InflateRect(&box, -4, -4);
        Monstey_ThemeFill(dc, box, RGB(99, 176, 255));
      }
      rect.left += 25;
    } else {
      const auto color = (state & BST_PUSHED) ? Monstey_ThemeSelection
                         : s.hover            ? RGB(48, 55, 67)
                                              : Monstey_ThemeSurface;
      Monstey_ThemeFill(dc, rect, color);
      SetDCBrushColor(dc, (style == BS_DEFPUSHBUTTON) ? RGB(99, 176, 255)
                                                      : Monstey_ThemeBorder);
      FrameRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    }
    Monstey_ThemeLabel(dc, window, Monstey_ThemeTextOf(window), rect,
                       (style == BS_CHECKBOX || style == BS_AUTOCHECKBOX ||
                                style == BS_RADIOBUTTON ||
                                style == BS_AUTORADIOBUTTON
                            ? DT_LEFT
                            : DT_CENTER) |
                           DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (GetFocus() == window) {
      InflateRect(&rect, -4, -4);
      DrawFocusRect(dc, &rect);
    }
  } else if (s.kind == L"COMBOBOX") {
    Monstey_ThemeFill(dc, rect, Monstey_ThemeSurface);
    SetDCBrushColor(dc, Monstey_ThemeBorder);
    FrameRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    RECT text = rect;
    text.left += 8;
    text.right -= 25;
    Monstey_ThemeLabel(dc, window, Monstey_ThemeTextOf(window), text,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SetDCPenColor(dc, Monstey_ThemeMuted);
    const auto pen = SelectObject(dc, GetStockObject(DC_PEN));
    const LONG x = rect.right - 14, y = (rect.top + rect.bottom) / 2;
    MoveToEx(dc, x - 4, y - 2, nullptr);
    LineTo(dc, x, y + 2);
    LineTo(dc, x + 5, y - 3);
    SelectObject(dc, pen);
    if (GetFocus() == window) {
      InflateRect(&text, -1, -4);
      DrawFocusRect(dc, &text);
    }
  }
  if (saved)
    RestoreDC(dc, saved);
}
LRESULT CALLBACK Monstey_ThemeProcedure(HWND window, UINT message, WPARAM w,
                                        LPARAM l, UINT_PTR, DWORD_PTR data) {
  auto *s = reinterpret_cast<Monstey_ThemeState *>(data);
  try {
    switch (message) {
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORDLG: {
      const auto dc = reinterpret_cast<HDC>(w);
      SetTextColor(dc, IsWindowEnabled(reinterpret_cast<HWND>(l))
                           ? Monstey_ThemeText
                           : Monstey_ThemeMuted);
      const bool surface = message == WM_CTLCOLOREDIT ||
                           message == WM_CTLCOLORLISTBOX ||
                           (message == WM_CTLCOLORSTATIC && [](HWND h) {
                             wchar_t c[32]{};
                             GetClassNameW(h, c, 32);
                             return _wcsicmp(c, L"EDIT") == 0;
                           }(reinterpret_cast<HWND>(l)));
      SetBkColor(dc, surface ? Monstey_ThemeSurface : Monstey_ThemeBackground);
      return reinterpret_cast<LRESULT>(surface ? Monstey_Brushes().surface
                                               : Monstey_Brushes().background);
    }
    case WM_NOTIFY: {
      const auto *header = reinterpret_cast<const NMHDR *>(l);
      if (header && header->code == NM_CUSTOMDRAW) {
        wchar_t kind[64]{};
        GetClassNameW(header->hwndFrom, kind, 64);
        if (_wcsicmp(kind, WC_LISTVIEWW) == 0) {
          auto *draw = reinterpret_cast<NMLVCUSTOMDRAW *>(l);
          if (draw->nmcd.dwDrawStage == CDDS_PREPAINT)
            return CDRF_NOTIFYITEMDRAW;
          if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            draw->clrText = Monstey_ThemeText;
            const bool selected =
                ListView_GetItemState(header->hwndFrom,
                                      static_cast<int>(draw->nmcd.dwItemSpec),
                                      LVIS_SELECTED) != 0;
            draw->clrTextBk =
                selected ? Monstey_ThemeSelection : Monstey_ThemeBackground;
            draw->nmcd.uItemState &= ~CDIS_SELECTED;
            return CDRF_DODEFAULT;
          }
        } else if (_wcsicmp(kind, WC_TREEVIEWW) == 0) {
          auto *draw = reinterpret_cast<NMTVCUSTOMDRAW *>(l);
          if (draw->nmcd.dwDrawStage == CDDS_PREPAINT)
            return CDRF_NOTIFYITEMDRAW;
          if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
            draw->clrText = Monstey_ThemeText;
            const bool selected =
                TreeView_GetItemState(
                    header->hwndFrom,
                    reinterpret_cast<HTREEITEM>(draw->nmcd.dwItemSpec),
                    TVIS_SELECTED) != 0;
            draw->clrTextBk =
                selected ? Monstey_ThemeSelection : Monstey_ThemeBackground;
            draw->nmcd.uItemState &= ~CDIS_SELECTED;
            return CDRF_DODEFAULT;
          }
        }
      }
      break;
    }
    case WM_INITMENUPOPUP:
      s->Menu(reinterpret_cast<HMENU>(w), false);
      break;
    case WM_UNINITMENUPOPUP:
      s->RestoreMenu(reinterpret_cast<HMENU>(w));
      break;
    case WM_MEASUREITEM: {
      auto *item = reinterpret_cast<MEASUREITEMSTRUCT *>(l);
      if (item && item->CtlType == ODT_MENU)
        if (auto *entry = s->Entry(item->itemData)) {
          const auto dc = GetDC(window);
          SIZE size{};
          const auto font =
              reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0));
          const auto old =
              SelectObject(dc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
          GetTextExtentPoint32W(dc, entry->text.c_str(),
                                static_cast<int>(entry->text.size()), &size);
          SelectObject(dc, old);
          ReleaseDC(window, dc);
          item->itemWidth = static_cast<UINT>(size.cx + (entry->top ? 20 : 56));
          item->itemHeight =
              (entry->type & MFT_SEPARATOR)
                  ? 9U
                  : static_cast<UINT>(std::max(26L, size.cy + 10));
          return TRUE;
        }
      break;
    }
    case WM_DRAWITEM: {
      const auto *item = reinterpret_cast<const DRAWITEMSTRUCT *>(l);
      if (item && item->CtlType == ODT_MENU)
        if (auto *entry = s->Entry(item->itemData)) {
          const auto saved = SaveDC(item->hDC);
          Monstey_ThemeFill(item->hDC, item->rcItem,
                            item->itemState & ODS_SELECTED
                                ? Monstey_ThemeSelection
                                : Monstey_ThemeBackground);
          RECT text = item->rcItem;
          if (entry->type & MFT_SEPARATOR) {
            text.top += 4;
            text.bottom = text.top + 1;
            InflateRect(&text, -10, 0);
            Monstey_ThemeFill(item->hDC, text, Monstey_ThemeBorder);
          } else {
            text.left += entry->top ? 10 : 28;
            text.right -= 12;
            Monstey_ThemeLabel(item->hDC, window, entry->text, text,
                               DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                                   DT_EXPANDTABS);
            if (item->itemState & (ODS_DISABLED | ODS_GRAYED)) {
              SetTextColor(item->hDC, Monstey_ThemeMuted);
              DrawTextW(item->hDC, entry->text.c_str(),
                        static_cast<int>(entry->text.size()), &text,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_EXPANDTABS);
            }
            if (item->itemState & ODS_CHECKED) {
              RECT check = item->rcItem;
              check.left += 9;
              check.right = check.left + 7;
              check.top += 9;
              check.bottom = check.top + 7;
              Monstey_ThemeFill(item->hDC, check, RGB(99, 176, 255));
            }
          }
          if (saved)
            RestoreDC(item->hDC, saved);
          return TRUE;
        }
      break;
    }
    case WM_MOUSEMOVE:
      if (!s->hover) {
        s->hover = true;
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
        TrackMouseEvent(&track);
        InvalidateRect(window, nullptr, FALSE);
      }
      break;
    case WM_MOUSELEAVE:
      s->hover = false;
      InvalidateRect(window, nullptr, FALSE);
      break;
    case WM_PAINT:
    case WM_PRINT:
    case WM_PRINTCLIENT:
      if (s->kind == L"BUTTON" || s->kind == L"COMBOBOX" ||
          s->kind == WC_TABCONTROLW || s->kind == WC_HEADERW ||
          s->kind == TRACKBAR_CLASSW) {
        PAINTSTRUCT paint{};
        const auto dc = message == WM_PAINT ? BeginPaint(window, &paint)
                                            : reinterpret_cast<HDC>(w);
        Monstey_ThemePaint(window, dc, *s);
        if (message == WM_PAINT)
          EndPaint(window, &paint);
        return 0;
      }
      break;
    case WM_ERASEBKGND:
      if (s->kind.starts_with(L"Monstey_SoW") || s->kind == WC_TABCONTROLW) {
        RECT r{};
        GetClientRect(window, &r);
        FillRect(reinterpret_cast<HDC>(w), &r, Monstey_Brushes().background);
        return 1;
      }
      break;
    case WM_NCDESTROY: {
      RemoveWindowSubclass(window, Monstey_ThemeProcedure,
                           Monstey_ThemeSubclass);
      while (!s->menus.empty())
        s->RestoreMenu(s->menus.back()->menu);
      const auto result = DefSubclassProc(window, message, w, l);
      delete s;
      return result;
    }
    }
  } catch (...) { /* Window callbacks must not propagate allocation failures. */
  }
  return DefSubclassProc(window, message, w, l);
}
void Monstey_ThemeInstall(HWND window) {
  DWORD_PTR existing{};
  if (GetWindowSubclass(window, Monstey_ThemeProcedure, Monstey_ThemeSubclass,
                        &existing))
    return;
  auto state = std::make_unique<Monstey_ThemeState>();
  wchar_t kind[128]{};
  GetClassNameW(window, kind, 128);
  state->kind = kind;
  if (_wcsicmp(kind, L"ComboBox") == 0)
    state->kind = L"COMBOBOX";
  if (_wcsicmp(kind, L"Button") == 0)
    state->kind = L"BUTTON";
  if (!SetWindowSubclass(window, Monstey_ThemeProcedure, Monstey_ThemeSubclass,
                         reinterpret_cast<DWORD_PTR>(state.get())))
    return;
  auto *owned = state.release();
  SetWindowTheme(window, L"DarkMode_Explorer", nullptr);
  if (owned->kind == WC_LISTVIEWW) {
    ListView_SetExtendedListViewStyleEx(window, LVS_EX_GRIDLINES, 0);
    ListView_SetBkColor(window, Monstey_ThemeBackground);
    ListView_SetTextBkColor(window, Monstey_ThemeBackground);
    ListView_SetTextColor(window, Monstey_ThemeText);
  }
  if (owned->kind == WC_TREEVIEWW) {
    TreeView_SetBkColor(window, Monstey_ThemeBackground);
    TreeView_SetTextColor(window, Monstey_ThemeText);
    TreeView_SetLineColor(window, Monstey_ThemeBorder);
  }
  if ((_wcsicmp(kind, L"EDIT") == 0 || owned->kind == WC_LISTVIEWW ||
       owned->kind == WC_TREEVIEWW) &&
      (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_CLIENTEDGE)) {
    SetWindowLongPtrW(window, GWL_EXSTYLE,
                      GetWindowLongPtrW(window, GWL_EXSTYLE) &
                          ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE));
    SetWindowLongPtrW(window, GWL_STYLE,
                      GetWindowLongPtrW(window, GWL_STYLE) | WS_BORDER);
    SetWindowPos(window, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
  }
  if (GetMenu(window)) {
    owned->Menu(GetMenu(window), true);
    DrawMenuBar(window);
  }
  if (!(GetWindowLongPtrW(window, GWL_STYLE) & WS_CHILD)) {
    const BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(window, 20, &dark, sizeof(dark))))
      DwmSetWindowAttribute(window, 19, &dark, sizeof(dark));
    const COLORREF caption = Monstey_ThemeBackground, text = Monstey_ThemeText,
                   border = Monstey_ThemeBorder;
    DwmSetWindowAttribute(window, 35, &border, sizeof(border));
    DwmSetWindowAttribute(window, 36, &caption, sizeof(caption));
    DwmSetWindowAttribute(window, 37, &text, sizeof(text));
  }
}
BOOL CALLBACK Monstey_ThemeChild(HWND child, LPARAM) {
  Monstey_ThemeInstall(child);
  return TRUE;
}
} // namespace
HBRUSH Monstey_ThemeBackgroundBrush() { return Monstey_Brushes().background; }
void Monstey_ThemeApply(HWND window) {
  if (!IsWindow(window))
    return;
  Monstey_ThemeInstall(window);
  EnumChildWindows(window, Monstey_ThemeChild, 0);
  RedrawWindow(window, nullptr, nullptr,
               RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}
} // namespace Monstey