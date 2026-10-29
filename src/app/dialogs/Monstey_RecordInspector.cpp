#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::OpenInspector(const J &snapshot)
{
    if (!inspector)
        inspector = std::make_shared<Monstey_Inspector>();
    inspector->app = this;
    inspector->metadata = snapshot;
    if (!inspector->window)
        inspector->Create();
    else
    {
        inspector->Populate();
        ShowWindow(inspector->window, SW_SHOW);
        SetForegroundWindow(inspector->window);
    }
    if (!smokeReport.empty())
        SmokeStep();
}

void Monstey_Inspector::Create()
{
    const auto title = L"Record properties - " + Monstey_Display(metadata.at("name"));
    window = CreateWindowExW(WS_EX_TOOLWINDOW, Monstey_InspectorClass, title.c_str(), WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT, 1000, 680, app->window, nullptr, app->instance, this);
    Monstey_Require(window != nullptr, "Cannot open record properties");
    Monstey_ThemeApply(window);
    Populate();
}

void Monstey_Inspector::Populate()
{
    if (!window || !list)
        return;
    ListView_DeleteAllItems(list);
    const auto &fields = metadata.at("fields").as_array();
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        const auto &f = fields[i];
        Monstey_ListCell(list, static_cast<int>(i), 0, Monstey_Display(f.at("name")));
        Monstey_ListCell(list, static_cast<int>(i), 1, Monstey_Display(f.at("type_name")));
        Monstey_ListCell(list, static_cast<int>(i), 2, Monstey_Display(f.at("value")));
    }
    if (!fields.empty())
        Monstey_ListSelect(list, 0);
    for (auto id : {Monstey_AddField, Monstey_DeleteField, Monstey_MoveFieldUp, Monstey_MoveFieldDown})
        ShowWindow(GetDlgItem(window, id),
                   (metadata.value("view_tab", 0).as_int() == 0 || metadata.value("view_tab", 0).as_int() == 3)
                       ? SW_SHOW
                       : SW_HIDE);
}

LRESULT CALLBACK Monstey_Inspector::Procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto *self = reinterpret_cast<Monstey_Inspector *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        self = static_cast<Monstey_Inspector *>(reinterpret_cast<const CREATESTRUCTW *>(lparam)->lpCreateParams);
        self->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self)
        return DefWindowProcW(window, message, wparam, lparam);
    try
    {
        switch (message)
        {
        case WM_CREATE:
        {
            self->list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                         WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_TABSTOP, 10, 10, 900,
                                         570, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(Monstey_Fields)),
                                         self->app->instance, nullptr);
            ListView_SetExtendedListViewStyle(self->list,
                                              LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
            SendMessageW(self->list, WM_SETFONT, reinterpret_cast<WPARAM>(self->app->font), TRUE);
            Monstey_ListColumn(self->list, 0, L"Property", 240);
            Monstey_ListColumn(self->list, 1, L"Type", 120);
            Monstey_ListColumn(self->list, 2, L"Value", 570);
            const auto edit = CreateWindowExW(
                0, L"BUTTON", L"Edit property", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 10, 590, 130, 28, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(Monstey_EditorApply)), self->app->instance, nullptr);
            SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(self->app->font), TRUE);
            for (const auto &[id, label] :
                 {std::pair{Monstey_AddField, L"Add property..."}, std::pair{Monstey_DeleteField, L"Delete property"},
                  std::pair{Monstey_MoveFieldUp, L"Move up"}, std::pair{Monstey_MoveFieldDown, L"Move down"}})
            {
                auto button =
                    CreateWindowExW(0, L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP, 150, 590, 120, 28, window,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), self->app->instance, nullptr);
                SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(self->app->font), TRUE);
            }
            return 0;
        }
        case WM_SIZE:
        {
            RECT rect{};
            GetClientRect(window, &rect);
            MoveWindow(self->list, 10, 10, std::max(100L, rect.right - 20), std::max(100L, rect.bottom - 55), TRUE);
            MoveWindow(GetDlgItem(window, Monstey_EditorApply), 10, rect.bottom - 37, 130, 28, TRUE);
            {
                int x = 150;
                for (auto id : {Monstey_AddField, Monstey_DeleteField, Monstey_MoveFieldUp, Monstey_MoveFieldDown})
                {
                    MoveWindow(GetDlgItem(window, id), x, rect.bottom - 37, 120, 28, TRUE);
                    x += 130;
                }
            }
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) >= Monstey_AddField && LOWORD(wparam) <= Monstey_MoveFieldDown)
            {
                self->app->StructureCommand(LOWORD(wparam), &self->metadata,
                                            ListView_GetNextItem(self->list, -1, LVNI_SELECTED));
                return 0;
            }
            if (LOWORD(wparam) == Monstey_EditorApply)
            {
                self->app->EditField(ListView_GetNextItem(self->list, -1, LVNI_SELECTED), &self->metadata);
                return 0;
            }
            break;
        case WM_NOTIFY:
        {
            const auto *header = reinterpret_cast<const NMHDR *>(lparam);
            if (header->hwndFrom == self->list && header->code == NM_DBLCLK)
            {
                const auto *activation = reinterpret_cast<const NMITEMACTIVATE *>(lparam);
                const int row =
                    activation->iItem >= 0 ? activation->iItem : ListView_GetNextItem(self->list, -1, LVNI_SELECTED);
                ++self->app->fieldDoubleClicks;
                self->app->EditField(row, &self->metadata);
                return 0;
            }
            if (header->hwndFrom == self->list && header->code == LVN_KEYDOWN)
            {
                const auto *key = reinterpret_cast<const NMLVKEYDOWN *>(lparam);
                if ((self->metadata.value("view_tab", 0).as_int() == 0 ||
                     self->metadata.value("view_tab", 0).as_int() == 3) &&
                    (key->wVKey == VK_INSERT || key->wVKey == VK_DELETE))
                {
                    self->app->StructureCommand(key->wVKey == VK_INSERT ? Monstey_AddField : Monstey_DeleteField,
                                                &self->metadata, ListView_GetNextItem(self->list, -1, LVNI_SELECTED));
                    return 0;
                }
                if (key->wVKey == VK_RETURN || key->wVKey == VK_F2)
                {
                    self->app->EditField(ListView_GetNextItem(self->list, -1, LVNI_SELECTED), &self->metadata);
                    return 0;
                }
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_NCDESTROY:
            self->window = nullptr;
            self->list = nullptr;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            return 0;
        default:
            break;
        }
    }
    catch (const std::exception &e)
    {
        self->app->Error(e.what());
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace Monstey::Monstey_AppInternal
