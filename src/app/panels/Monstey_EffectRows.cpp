#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

J Monstey_FxValue(const J &record, std::string_view name, const J &fallback)
{
    if (record.contains("fields"))
        for (const auto &field : record.at("fields").as_array())
            if (field.at("name").as_string() == name)
                return field.at("value");
    return fallback;
}

std::string Monstey_FxLabel(const J &record)
{
    for (const auto name : {"KeyName", "FxName"})
    {
        auto value = Monstey_FxValue(record, name);
        if (value.is_string() && !value.as_string().empty())
            return value.as_string();
    }
    for (const auto name : {"key_name", "name", "category", "reference"})
        if (record.contains(name) && record.at(name).is_string() && !record.at(name).as_string().empty())
            return record.at(name).as_string();
    return "Empty native key";
}

void Monstey_FxMenu(HMENU menu)
{
    auto resource = CreatePopupMenu(), tracks = CreatePopupMenu(), keys = CreatePopupMenu();
    for (auto [id, label] :
         {std::pair{Monstey_FxCreate, L"Create effect..."}, std::pair{Monstey_FxRename, L"Rename resource..."},
          std::pair{Monstey_FxDuplicate, L"Duplicate to new file..."}, std::pair{Monstey_FxDuration, L"Duration..."},
          std::pair{Monstey_FxDelete, L"Delete effect"}})
        AppendMenuW(resource, MF_STRING, id, label);
    for (auto [id, label] :
         {std::pair{Monstey_FxTrackCreate, L"New track..."}, std::pair{Monstey_FxTrackRename, L"Rename track..."},
          std::pair{Monstey_FxTrackDelete, L"Unlink track"}, std::pair{Monstey_FxTrackUp, L"Move track up"},
          std::pair{Monstey_FxTrackDown, L"Move track down"}})
        AppendMenuW(tracks, MF_STRING, id, label);
    for (auto [id, label] :
         {std::pair{Monstey_FxKeyCreate, L"Create native element..."},
          std::pair{Monstey_FxKeyDuplicate, L"Duplicate element..."},
          std::pair{Monstey_FxKeyRename, L"Rename element..."},
          std::pair{Monstey_FxKeyDelete, L"Delete physical element"},
          std::pair{Monstey_FxKeyLink, L"Link existing element..."},
          std::pair{Monstey_FxKeyUnlink, L"Unlink selected key"}, std::pair{Monstey_FxKeyUp, L"Move key up"},
          std::pair{Monstey_FxKeyDown, L"Move key down"}, std::pair{Monstey_FxKeyRetime, L"Start / end time..."}})
        AppendMenuW(keys, MF_STRING, id, label);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(resource), L"Resource");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(tracks), L"Tracks");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(keys), L"Keys / elements");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    for (auto [id, label] :
         {std::pair{Monstey_AddField, L"Add property..."}, std::pair{Monstey_DeleteField, L"Delete property"},
          std::pair{Monstey_MoveFieldUp, L"Move property up"}, std::pair{Monstey_MoveFieldDown, L"Move property down"},
          std::pair{Monstey_FxReconcile, L"Reconcile tracks from native KeyList"}})
        AppendMenuW(menu, MF_STRING, id, label);
}

void Monstey_App::LoadFx()
{
    const auto request = ++fxLoadRequest;
    fxLoading = true;
    metadata = J::object();
    fieldData.clear();
    ListView_DeleteAllItems(fields);
    UpdateButtons();
    SetWindowTextW(details, L"Refreshing native effect tracks and component properties ...");
    Queue("fx_workflow", J::object(),
          [this, request](const J &reply)
          {
              if (tab != 3 || request != fxLoadRequest)
                  return;
              if (!reply.at("ok").as_bool())
              {
                  fxLoading = false;
                  UpdateButtons();
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              fxWorkflow = reply.at("result");
              UpdateState(fxWorkflow, 3);
              auto finish = [this, request]
              {
                  if (tab != 3 || request != fxLoadRequest)
                      return;
                  fxTreeRows.clear();
                  fxTreeRows.push_back(J::object(
                      {{"id", "global"}, {"name", fxGlobal.empty() ? "Effect settings" : Monstey_FxLabel(fxGlobal)}}));
                  fxTreeRows.push_back(J::object({{"id", "all"}, {"name", "All native records"}}));
                  for (const auto &track : fxWorkflow.at("layout").at("tracks").as_array())
                      fxTreeRows.push_back(J::object({{"id", track.at("id")},
                                                      {"name", track.at("name").as_string() + " (" +
                                                                   std::to_string(track.at("keys").size()) + ")"}}));
                  fxTreeRows.push_back(J::object({{"id", "unlinked"}, {"name", "Unlinked element library"}}));
                  fxTreeRows.push_back(J::object({{"id", "prefabs"}, {"name", "Component prefab library"}}));
                  if (fxLibrary.contains("groups"))
                      for (const auto &entry : fxLibrary.at("groups").as_object())
                          fxTreeRows.push_back(J::object({{"id", "library-group:" + entry.first},
                                                          {"name", "  " + entry.second.at("name").as_string()}}));
                  if (fxView == "pending-element")
                  {
                      fxView = "unlinked";
                      for (const auto &track : fxWorkflow.at("layout").at("tracks").as_array())
                          for (const auto &key : track.at("keys").as_array())
                              if (key.is_string() && key.as_string() == selector)
                                  fxView = track.at("id").as_string();
                  }
                  populating = true;
                  TreeView_DeleteAllItems(tree);
                  int preferred = 1;
                  for (std::size_t i = 0; i < fxTreeRows.size(); ++i)
                  {
                      auto text = Monstey_Display(fxTreeRows[i].at("name"));
                      TVINSERTSTRUCTW item{};
                      item.hParent = TVI_ROOT;
                      item.hInsertAfter = TVI_LAST;
                      item.item.mask = TVIF_TEXT | TVIF_PARAM;
                      item.item.pszText = text.data();
                      item.item.lParam = static_cast<LPARAM>(i);
                      auto handle = TreeView_InsertItem(tree, &item);
                      if (fxTreeRows[i].at("id").as_string() == fxView)
                      {
                          preferred = static_cast<int>(i);
                          TreeView_SelectItem(tree, handle);
                      }
                  }
                  populating = false;
                  SelectFxView(preferred);
                  fxLoading = false;
                  UpdateButtons();
                  if (!fxWorkflow.at("layout_synchronized").as_bool())
                      SetStatus(L"Native KeyList differs from project tracks. Effects > "
                                L"Reconcile tracks updates the editor layout.");
                  if (!smokeReport.empty())
                      SmokeStep();
              };
              auto loadLibrary = [this, finish, request]
              {
                  Queue("fx_library", J::object(),
                        [this, finish, request](const J &reply)
                        {
                            if (tab != 3 || request != fxLoadRequest)
                                return;
                            if (!reply.at("ok").as_bool())
                            {
                                fxLoading = false;
                                UpdateButtons();
                                Error(reply.at("error").at("message").as_string());
                                return;
                            }
                            fxLibrary = reply.at("result").at("library");
                            finish();
                        });
              };
              const auto global = fxWorkflow.at("global_record").as_string();
              if (global.empty())
              {
                  fxGlobal = J::object();
                  loadLibrary();
                  return;
              }
              Queue("fx_record", J::object({{"record", global}}),
                    [this, loadLibrary, request](const J &record)
                    {
                        if (tab != 3 || request != fxLoadRequest)
                            return;
                        if (!record.at("ok").as_bool())
                        {
                            fxLoading = false;
                            UpdateButtons();
                            Error(record.at("error").at("message").as_string());
                            return;
                        }
                        fxGlobal = record.at("result");
                        loadLibrary();
                    });
          });
}

void Monstey_App::SelectFxView(int index)
{
    if (tab != 3 || index < 0 || static_cast<std::size_t>(index) >= fxTreeRows.size())
        return;
    auto next = fxTreeRows[static_cast<std::size_t>(index)].at("id").as_string();
    if (next != fxView)
    {
        offset = 0;
        selector.clear();
    }
    fxView = next;
    while (ListView_DeleteColumn(records, 0))
    {
    }
    Monstey_ListColumn(records, 0, fxView == "all" ? L"Native record / element" : L"Element / key", 250);
    Monstey_ListColumn(records, 1, fxView == "all" ? L"Reference" : L"Class", 200);
    Monstey_ListColumn(records, 2, fxView == "all" ? L"Class" : L"Start - end (seconds)", 200);
    LoadRecords();
}

void Monstey_App::PopulateFxRows()
{
    std::vector<J> candidates;
    if (fxView == "global")
    {
        if (!fxGlobal.empty())
            candidates.push_back(fxGlobal);
    }
    else if (fxView == "prefabs" || fxView.starts_with("library-group:"))
    {
        if (fxLibrary.contains("prefabs"))
            for (const auto &[id, prefab] : fxLibrary.at("prefabs").as_object())
            {
                if (fxView != "prefabs" && prefab.value("group", "").as_string() != fxView.substr(14))
                    continue;
                candidates.push_back(J::object({{"reference", prefab.at("record")},
                                                {"name", prefab.at("name")},
                                                {"category", prefab.at("class_name")},
                                                {"fx_prefab", id}}));
            }
    }
    else if (fxView == "unlinked")
        candidates = fxWorkflow.at("unlinked_elements").as_array();
    else
        for (const auto &track : fxWorkflow.at("layout").at("tracks").as_array())
            if (track.at("id").as_string() == fxView)
            {
                for (std::size_t i = 0; i < track.at("keys").size(); ++i)
                {
                    const auto &key = track.at("keys")[i];
                    J row = J::object();
                    if (!key.is_null())
                        for (const auto &activeKey : fxWorkflow.at("active_keys").as_array())
                            if (activeKey.contains("reference") && activeKey.at("reference") == key)
                            {
                                row = activeKey;
                                break;
                            }
                    row["fx_track"] = track.at("id");
                    row["fx_key_index"] = i;
                    candidates.push_back(row);
                }
                break;
            }
    const auto folded = [](std::string text)
    {
        for (auto &c : text)
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c + 32);
        return text;
    };
    const auto filter = folded(Monstey_Utf8(Monstey_WindowText(query)));
    rowData.clear();
    total = 0;
    for (const auto &row : candidates)
    {
        const auto text = folded(Monstey_FxLabel(row) + " " + row.value("category", "").as_string() + " " +
                                 row.value("reference", "").as_string());
        if (!filter.empty() && text.find(filter) == text.npos)
            continue;
        auto ordinal = total++;
        if (ordinal >= offset && rowData.size() < 250)
            rowData.push_back(row);
    }
    populating = true;
    ListView_DeleteAllItems(records);
    ListView_DeleteAllItems(fields);
    fieldData.clear();
    metadata = J::object();
    int preferred = 0;
    for (std::size_t i = 0; i < rowData.size(); ++i)
    {
        auto &row = rowData[i];
        Monstey_ListCell(records, static_cast<int>(i), 0, Monstey_Wide(Monstey_FxLabel(row)));
        Monstey_ListCell(records, static_cast<int>(i), 1, Monstey_Display(row.value("category", "")));
        auto start = Monstey_FxValue(row, "StartTime"), end = Monstey_FxValue(row, "EndTime");
        Monstey_ListCell(records, static_cast<int>(i), 2,
                         start.is_null() ? L"" : Monstey_Display(start) + L" - " + Monstey_Display(end));
        if (row.contains("reference") && row.at("reference").as_string() == selector)
            preferred = static_cast<int>(i);
    }
    populating = false;
    SetWindowTextW(pageLabel, (L"Rows " + std::to_wstring(rowData.empty() ? 0 : offset + 1) + L" - " +
                               std::to_wstring(offset + rowData.size()) + L" of " + std::to_wstring(total) +
                               L". Tracks flatten into native KeyList; unlinked elements "
                               L"remain in the file.")
                                  .c_str());
    EnableWindow(GetDlgItem(window, Monstey_Previous), offset > 0);
    EnableWindow(GetDlgItem(window, Monstey_Next), offset + rowData.size() < total);
    if (!rowData.empty())
    {
        Monstey_ListSelect(records, preferred);
        ReadRecord(preferred);
    }
    else
    {
        selector.clear();
        ShowReport(fxWorkflow);
    }
}

} // namespace Monstey::Monstey_AppInternal
