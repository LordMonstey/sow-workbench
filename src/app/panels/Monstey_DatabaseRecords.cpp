#include "../Monstey_AppState.hpp"
#include "Monstey_CategoryPresentation.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::LoadRecords()
{
    if (tab == 3 && fxView != "all")
    {
        PopulateFxRows();
        return;
    }
    std::string operation = tab == 0   ? "records"
                            : tab == 1 ? "archive_entries"
                            : tab == 2 ? "strings_entries"
                                       : "fx_records";
    J args = J::object({{"query", Monstey_Utf8(Monstey_WindowText(query))}, {"offset", offset}, {"limit", 250}});
    if (tab == 0)
    {
        if (!category)
            return;
        args["category_index"] = *category;
    }
    if (tab == 0)
        Monstey_RefreshDatabaseTreeQuery();
    const int requestedTab = tab;
    const auto requestedCategory = category;
    const auto requestedOffset = offset;
    const auto requestedQuery = args.at("query").as_string();
    const auto requestedTree = databaseTreeGeneration;
    const auto requestedSession = generation, requestedRevision = revision;
    const auto requestedFxLoad = fxLoadRequest;
    const auto requestedFxView = fxView;
    Queue(operation, args,
          [this, requestedTab, requestedCategory, requestedOffset, requestedQuery, requestedTree, requestedSession,
           requestedRevision, requestedFxLoad, requestedFxView](const J &reply)
          {
              if (requestedTab != tab ||
                  (requestedTab == 0 && (requestedCategory != category || requestedOffset != offset ||
                                         requestedQuery != Monstey_Utf8(Monstey_WindowText(query)) ||
                                         requestedTree != databaseTreeGeneration || requestedSession != generation ||
                                         requestedRevision != revision)) ||
                  (requestedTab == 3 && (requestedFxLoad != fxLoadRequest || requestedFxView != fxView)))
                  return;
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              const auto &result = reply.at("result");
              populating = true;
              ListView_DeleteAllItems(records);
              ListView_DeleteAllItems(fields);
              fieldData.clear();
              metadata = J::object();
              rowData = result.at("rows").as_array();
              total = result.at("total").as_uint();
              const auto fileRows = rowData.size();
              if (tab == 1 && offset == 0 && result.contains("folders"))
              {
                  auto filter = Monstey_Utf8(Monstey_WindowText(query));
                  const auto fold = [](std::string text)
                  {
                      std::transform(
                          text.begin(), text.end(), text.begin(), [](unsigned char c)
                          { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : static_cast<char>(c); });
                      return text;
                  };
                  filter = fold(filter);
                  for (const auto &folder : result.at("folders").as_array())
                      if (!folder.as_string().empty() && fold(folder.as_string()).find(filter) != std::string::npos)
                          rowData.push_back(J::object({{"path", folder}, {"folder", true}}));
              }
              for (std::size_t i = 0; i < rowData.size(); ++i)
              {
                  const auto &row = rowData[i];
                  std::wstring name;
                  if (tab == 0 || tab == 3)
                  {
                      name = row.contains("key_name") && !row.at("key_name").as_string().empty()
                                 ? Monstey_Display(row.at("key_name"))
                             : row.at("name").is_null() ? L"(anonymous)"
                                                        : Monstey_Display(row.at("name"));
                      if (name.empty())
                          name = L"(anonymous)";
                      Monstey_ListCell(records, static_cast<int>(i), 0, name);
                      Monstey_ListCell(records, static_cast<int>(i), 1, Monstey_Display(row.at("reference")));
                      auto categoryText = row.contains("category") ? Monstey_Display(row.at("category")) : L"";
                      if (tab == 0 && row.contains("category_index"))
                      {
                          const auto *categoryRow =
                              Monstey_CategoryRow(static_cast<std::uint32_t>(row.at("category_index").as_uint()));
                          if (categoryRow)
                          {
                              const auto presentation = Monstey_PresentCategory(categoryRow->at("name").as_string(),
                                                                                categoryRow->at("schema").as_string(),
                                                                                categoryRow->at("path").as_string());
                              categoryText = Monstey_Wide(Monstey_CategoryDisplayPath(presentation));
                          }
                      }
                      Monstey_ListCell(records, static_cast<int>(i), 2, categoryText);
                  }
                  else if (tab == 1)
                  {
                      Monstey_ListCell(records, static_cast<int>(i), 0, Monstey_Display(row.at("path")));
                      const auto folder = row.value("folder", false).as_bool();
                      Monstey_ListCell(records, static_cast<int>(i), 1,
                                       folder ? L"Folder" : Monstey_Display(row.at("size")));
                      Monstey_ListCell(records, static_cast<int>(i), 2,
                                       folder ? L"" : Monstey_Display(row.at("stored_size")));
                  }
                  else
                  {
                      Monstey_ListCell(records, static_cast<int>(i), 0, Monstey_Display(row.at("hash")));
                      Monstey_ListCell(records, static_cast<int>(i), 1, Monstey_Display(row.at("text")));
                      Monstey_ListCell(records, static_cast<int>(i), 2, row.at("dirty").as_bool() ? L"Yes" : L"");
                  }
              }
              SetWindowTextW(pageLabel, (L"Rows " + std::to_wstring(fileRows == 0 ? 0 : offset + 1) + L" - " +
                                         std::to_wstring(offset + fileRows) + L" of " + std::to_wstring(total) +
                                         (tab == 1 ? L" files; folders listed on first page" : L""))
                                            .c_str());
              EnableWindow(GetDlgItem(window, Monstey_Previous), offset > 0);
              EnableWindow(GetDlgItem(window, Monstey_Next), offset + fileRows < total);
              populating = false;
              if (!rowData.empty())
              {
                  int chosen = 0;
                  for (std::size_t i = 0; i < rowData.size(); ++i)
                      if (rowData[i].value(tab == 1 ? "path" : tab == 2 ? "hash" : "reference", "") == J(selector))
                          chosen = static_cast<int>(i);
                  // Programmatic selection has one explicit metadata read below.
                  // Suppress LVN_ITEMCHANGED so it cannot queue a duplicate read that
                  // would clear the just-restored field selection on its second reply.
                  populating = true;
                  Monstey_ListSelect(records, chosen);
                  populating = false;
                  const bool inspectTree = tab == 0 && !databaseTreeInspectSelector.empty() &&
                                           rowData[static_cast<std::size_t>(chosen)].at("reference").as_string() ==
                                               databaseTreeInspectSelector;
                  if (tab == 0)
                      databaseTreeInspectSelector.clear();
                  ReadRecord(chosen, inspectTree);
              }
              if (!smokeReport.empty())
                  SmokeStep();
          });
}

void Monstey_App::ReadRecord(int row, bool explicitActivation)
{
    if (tab == 4 && materialMode)
    {
        if (explicitActivation)
            ++recordDoubleClicks;
        MaterialReadRow(row, explicitActivation);
        return;
    }
    if (tab == 4 && animationMode)
    {
        if (explicitActivation)
            ++recordDoubleClicks;
        AnimationReadClip(row);
        return;
    }
    if (tab == 4 && socketMode)
    {
        if (row >= 0 && static_cast<std::size_t>(row) < rowData.size())
        {
            socketModel = rowData[static_cast<std::size_t>(row)].at("reference").as_string();
            socketOffset = 0;
            LoadSocketOverrides();
        }
        return;
    }
    if (Monstey_OperationForm())
    {
        SelectOperation(row);
        return;
    }
    if (row < 0 || static_cast<std::size_t>(row) >= rowData.size())
        return;
    if (explicitActivation)
        ++recordDoubleClicks;
    if (tab == 1)
    {
        const auto &item = rowData[static_cast<std::size_t>(row)];
        selector = item.at("path").as_string();
        const auto folder = item.value("folder", false).as_bool();
        for (auto id : {Monstey_Replace, Monstey_Extract})
            EnableWindow(GetDlgItem(window, id), !folder);
        ShowReport(item);
        return;
    }
    if (tab == 2)
    {
        const auto &item = rowData[static_cast<std::size_t>(row)];
        selector = item.at("hash").as_string();
        ShowReport(item);
        if (explicitActivation)
            Prompt(L"Edit Unicode string", "strings_set", J::object({{"key", selector}}), "text",
                   item.at("text").as_string(),
                   [this](const J &result)
                   {
                       UpdateState(result);
                       Refresh();
                   });
        return;
    }
    selector = rowData[static_cast<std::size_t>(row)].at("reference").as_string();
    if (tab == 0)
        Monstey_SyncDatabaseTreeSelection();
    const auto requested = selector;
    const int requestedTab = tab;
    const auto requestedFxLoad = fxLoadRequest;
    metadata = J::object();
    UpdateFxAttachmentButton();
    fieldData.clear();
    ListView_DeleteAllItems(fields);
    SetWindowTextW(details, L"Loading native record properties ...");
    Queue(tab == 0 ? "database_metadata" : "fx_record", J::object({{"record", selector}}),
          [this, requested, requestedTab, explicitActivation, requestedFxLoad](const J &reply)
          {
              if (requestedTab != tab || requested != selector ||
                  (requestedTab == 3 && requestedFxLoad != fxLoadRequest))
                  return;
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              PopulateFields(reply);
              if (explicitActivation)
                  OpenInspector(metadata);
          });
}

void Monstey_App::PopulateFields(const J &reply)
{
    metadata = reply.at("result");
    metadata["generation"] = reply.at("generation");
    metadata["revision"] = reply.at("revision");
    metadata["view_tab"] = tab;
    UpdateFxAttachmentButton();
    if (inspector && inspector->window && inspector->metadata.at("selector") == metadata.at("selector") &&
        inspector->metadata.value("view_tab", 0).as_int() == tab)
    {
        inspector->metadata = metadata;
        inspector->Populate();
    }
    if (tab == 3 && fxNestedMode)
    {
        if (metadata.value("category", "").as_string() == "ClientFX.Create")
        {
            LoadFxNested();
            return;
        }
        fxNestedMode = false;
        fxNestedView = J::object();
        while (ListView_DeleteColumn(fields, 0))
        {
        }
        Monstey_ListColumn(fields, 0, L"Property", 190);
        Monstey_ListColumn(fields, 1, L"Type", 100);
        Monstey_ListColumn(fields, 2, L"Value", 350);
    }
    fieldData = metadata.at("fields").as_array();
    populating = true;
    ListView_DeleteAllItems(fields);
    for (std::size_t i = 0; i < fieldData.size(); ++i)
    {
        const auto &f = fieldData[i];
        Monstey_ListCell(fields, static_cast<int>(i), 0,
                         (f.at("dirty").as_bool() ? L"* " : L"") + Monstey_Display(f.at("name")));
        Monstey_ListCell(fields, static_cast<int>(i), 1, Monstey_Display(f.at("type_name")));
        auto value = Monstey_Display(f.at("value"));
        std::replace(value.begin(), value.end(), L'\n', L' ');
        if (value.size() > 300)
            value = value.substr(0, 300) + L" ...";
        Monstey_ListCell(fields, static_cast<int>(i), 2, value);
    }
    populating = false;
    auto description = L"Double-click a property, or select it and press "
                       L"Enter/F2.\r\nRecord: " +
                       Monstey_Display(metadata.at("name"));
    if (tab == 0 && metadata.contains("category_index"))
    {
        const auto categoryDetails =
            Monstey_CategoryDetails(static_cast<std::uint32_t>(metadata.at("category_index").as_uint()));
        if (!categoryDetails.empty())
            description += L"\r\n" + categoryDetails;
    }
    description += L"\r\nNative values and schema constraints are preserved.";
    SetWindowTextW(details, description.c_str());
    if (tab == 0 && !databaseNativeMode && databaseRestoreField && selector == databaseSavedSelector)
    {
        databaseRestoreField = false;
        if (databaseSavedField >= 0 && static_cast<std::size_t>(databaseSavedField) < fieldData.size())
            Monstey_ListSelect(fields, databaseSavedField);
    }
    if (!smokeReport.empty())
        SmokeStep();
}

void Monstey_App::EditField(int index, const J *snapshot)
{
    if (Monstey_DatabaseForm() && snapshot != nullptr)
    {
        SetStatus(L"Return to records to edit database properties.");
        return;
    }
    if (tab == 4 && materialMode && snapshot == nullptr)
    {
        if (index == 0)
            MaterialCommand(Monstey_MaterialEdit);
        return;
    }
    if (tab == 4 && animationMode && snapshot == nullptr)
    {
        if (animationEventMode && index >= 0 && static_cast<std::size_t>(index) < fieldData.size())
        {
            Monstey_ListSelect(fields, index);
            AnimationEvent(Monstey_AnimationEventEdit);
        }
        return;
    }
    if (tab == 4 && socketMode && snapshot == nullptr)
    {
        if (index >= 0 && static_cast<std::size_t>(index) < fieldData.size())
        {
            Monstey_ListSelect(fields, index);
            SocketCommand(Monstey_SocketEdit);
        }
        return;
    }
    if (tab == 3 && fxNestedMode && snapshot == nullptr)
    {
        if (index >= 0 && static_cast<std::size_t>(index) < fieldData.size())
        {
            Monstey_ListSelect(fields, index);
            FxNestedCommand(Monstey_FxNestedSlot);
        }
        return;
    }
    if (Monstey_OperationForm() && snapshot == nullptr)
    {
        EditOperationField(index);
        return;
    }
    const auto &model = snapshot ? *snapshot : metadata;
    const auto &modelFields = model.at("fields").as_array();
    if (index < 0 || static_cast<std::size_t>(index) >= modelFields.size())
        return;
    const auto &field = modelFields[static_cast<std::size_t>(index)];
    if (!field.at("editable").as_bool())
    {
        SetWindowTextW(details, Monstey_Display(field.at("reason")).c_str());
        return;
    }
    auto item = std::make_shared<Monstey_Editor>();
    item->app = this;
    item->field = field;
    item->title = L"Edit " + Monstey_Display(field.at("name"));
    const auto originTab = static_cast<int>(model.value("view_tab", tab).as_int());
    const auto editedSelector = model.at("selector").as_string();
    item->operation = originTab == 0 ? "database_edit" : "fx_set";
    item->arguments = J::object({{"record", model.at("selector")}, {"field", field.at("hash")}});
    item->generation = model.at("generation").as_uint();
    item->revision = model.at("revision").as_uint();
    item->success = [this, originTab, editedSelector](const J &result)
    {
        if (originTab == tab)
            UpdateState(result);
        else
        {
            dirty[originTab] = result.at("dirty").as_bool();
            canUndo[originTab] = result.at("can_undo").as_bool();
            canRedo[originTab] = result.at("can_redo").as_bool();
        }
        if (originTab == 0 && tab == 4 && socketMode)
        {
            UpdateButtons();
            category.reset();
            LoadSocketModels();
        }
        Queue(originTab == 0 ? "database_metadata" : "fx_record", J::object({{"record", editedSelector}}),
              [this, originTab, editedSelector](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                      return;
                  if (originTab == tab && selector == editedSelector)
                      PopulateFields(reply);
                  else if (inspector && inspector->window &&
                           inspector->metadata.at("selector").as_string() == editedSelector &&
                           inspector->metadata.value("view_tab", 0).as_int() == originTab)
                  {
                      inspector->metadata = reply.at("result");
                      inspector->metadata["generation"] = reply.at("generation");
                      inspector->metadata["revision"] = reply.at("revision");
                      inspector->metadata["view_tab"] = originTab;
                      inspector->Populate();
                  }
              });
        if (!smokeReport.empty())
        {
            smoke["editor_applied"] = true;
            smokeStage = 4;
            SmokeStep();
        }
    };
    ShowEditor(item);
}

} // namespace Monstey::Monstey_AppInternal
