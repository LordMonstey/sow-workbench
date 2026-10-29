#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

std::wstring Monstey_WindowText(HWND window)
{
    const auto count = GetWindowTextLengthW(window);
    std::wstring text(static_cast<std::size_t>(count) + 1, L'\0');
    GetWindowTextW(window, text.data(), count + 1);
    text.resize(static_cast<std::size_t>(count));
    return text;
}

std::wstring Monstey_Display(const J &value)
{
    return Monstey_Wide(value.is_string() ? value.as_string() : value.dump());
}

void Monstey_ListColumn(HWND list, int index, const wchar_t *name, int width)
{
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.pszText = const_cast<LPWSTR>(name);
    column.cx = width;
    column.iSubItem = index;
    ListView_InsertColumn(list, index, &column);
}

void Monstey_ListCell(HWND list, int row, int column, std::wstring text)
{
    if (column == 0)
    {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.pszText = text.data();
        ListView_InsertItem(list, &item);
    }
    else
        ListView_SetItemText(list, row, column, text.data());
}

void Monstey_ListSelect(HWND list, int row)
{
    ListView_SetItemState(list, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(list, row, FALSE);
}

} // namespace Monstey::Monstey_AppInternal
