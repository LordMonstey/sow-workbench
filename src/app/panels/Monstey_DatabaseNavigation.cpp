#include "../Monstey_AppState.hpp"
#include "Monstey_CategoryPresentation.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::ClearStorageRows()
{
    if (tab != 1 && tab != 2)
        return;
    rowData.clear();
    fieldData.clear();
    selector.clear();
    total = 0;
    populating = true;
    ListView_DeleteAllItems(records);
    ListView_DeleteAllItems(fields);
    populating = false;
    SetWindowTextW(details, L"");
}

void Monstey_App::OpenSource(std::optional<std::filesystem::path> input)
{
    if (!input)
        input = Monstey_FileDialog(window, false, Monstey_SourceFilter(tab), nullptr, L"Open native source");
    if (!input)
        return;
    if (dirty[tab] && smokeReport.empty() &&
        MessageBoxW(window,
                    L"The current project has edits. Open a different source and "
                    L"discard those edits?",
                    L"Open source", MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
        return;
    ++epoch;
    offset = 0;
    selector.clear();
    category.reset();
    if (tab == 0)
    {
        categoryRows.clear();
        Monstey_ResetDatabaseTree();
    }
    ClearStorageRows();
    const std::string operation = tab == 0   ? "open_database"
                                  : tab == 1 ? "open_archive"
                                  : tab == 2 ? "strings_new"
                                             : "fx_open";
    J args = J::object({{tab == 2 ? "source" : "path", Monstey_JsonPath(*input)}});
    if (tab == 0 && !schema.empty())
        args["schema"] = Monstey_JsonPath(schema);
    if (tab == 3 && !fxSchema.empty())
        args["schema"] = Monstey_JsonPath(fxSchema);
    Queue(
        operation, args,
        [this](const J &reply)
        {
            if (!reply.at("ok").as_bool())
            {
                Error(reply.at("error").at("message").as_string());
                return;
            }
            UpdateState(reply.at("result"));
            Refresh(true);
        },
        false);
}

void Monstey_App::Refresh(bool reset)
{
    if (tab == 4 && materialMode)
    {
        if (reset)
            offset = 0;
        MaterialLoadRows();
        return;
    }
    if (tab == 4 && animationMode)
    {
        if (reset)
            offset = 0;
        AnimationLoadRows();
        return;
    }
    if (tab == 3)
    {
        if (reset)
            offset = 0;
        LoadFx();
        return;
    }
    if (tab == 4 && socketMode)
    {
        if (reset)
            offset = 0;
        LoadSocketModels();
        return;
    }
    if (Monstey_OperationForm())
    {
        LoadOperations();
        return;
    }
    if (reset)
        offset = 0;
    if (tab == 0)
    {
        if (categoryRows.empty())
            LoadCategories();
        else
            LoadRecords();
    }
    else
        LoadRecords();
}

void Monstey_App::LoadCategories()
{
    Queue("categories", J::object(),
          [this](const J &reply)
          {
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              populating = true;
              Monstey_ResetDatabaseTree();
              TreeView_DeleteAllItems(tree);
              SetWindowLongPtrW(tree, GWL_STYLE, GetWindowLongPtrW(tree, GWL_STYLE) | TVS_HASBUTTONS);
              categoryRows = reply.at("result").at("rows").as_array();
              std::sort(categoryRows.begin(), categoryRows.end(), [](const auto &a, const auto &b)
                        { return a.at("name").as_string() < b.at("name").as_string(); });
              const bool restoreNativeView = tab == 0 && databaseRestorePending;
              if (restoreNativeView)
                  category = databaseSavedCategory;
              int preferred = 0;
              std::vector<Monstey_CategoryPresentation> presentations;
              presentations.reserve(categoryRows.size());
              for (std::size_t i = 0; i < categoryRows.size(); ++i)
              {
                  const auto &row = categoryRows[i];
                  presentations.push_back(Monstey_PresentCategory(
                      row.at("name").as_string(), row.at("schema").as_string(), row.at("path").as_string()));
                  if (category && row.at("index").as_uint() == *category)
                      preferred = static_cast<int>(i);
                  if (row.at("name").as_string() == "Combat.VariableFloat" && !smokeReport.empty() && !smokeStructure &&
                      !smokeNativeProfile && !Monstey_SowAtomSmoke && smokeOperation.empty())
                      preferred = static_cast<int>(i);
              }
              const auto nodes = Monstey_DatabaseCategoryTree(presentations);
              std::vector<HTREEITEM> items(nodes.size()), leaves(categoryRows.size());
              for (std::size_t i = 0; i < nodes.size(); ++i)
              {
                  const auto &node = nodes[i];
                  auto text = Monstey_Wide(node.label);
                  TVINSERTSTRUCTW insert{};
                  insert.hParent = node.parent ? items[*node.parent] : TVI_ROOT;
                  insert.hInsertAfter = TVI_LAST;
                  insert.item.mask = TVIF_TEXT | TVIF_PARAM;
                  insert.item.pszText = text.data();
                  insert.item.lParam = node.kind == Monstey_DatabaseTreeKind::Root ? Monstey_DatabaseRootToken
                                                                                   : Monstey_DatabaseFolderToken;
                  items[i] = TreeView_InsertItem(tree, &insert);
                  Monstey_Require(items[i] != nullptr, "Cannot insert Database category hierarchy");
                  if (node.kind == Monstey_DatabaseTreeKind::Root)
                      databaseTreeRoot = items[i];
                  if (node.categoryRow)
                  {
                      const auto index =
                          static_cast<std::uint32_t>(categoryRows[*node.categoryRow].at("index").as_uint());
                      Monstey_BindDatabaseTreeItem(
                          items[i], {Monstey_DatabaseTreeKind::Category, node.categoryRow, index, {}, 0});
                      databaseTreeCategories.emplace(
                          index, Monstey_DatabaseCategoryNode{items[i], nullptr, *node.categoryRow});
                      Monstey_AddDatabaseTreePlaceholder(items[i]);
                      leaves[*node.categoryRow] = items[i];
                  }
              }
              TreeView_Expand(tree, databaseTreeRoot, TVE_EXPAND);
              if (!categoryRows.empty() && !(restoreNativeView && !databaseSavedCategory))
              {
                  const auto selected = leaves[static_cast<std::size_t>(preferred)];
                  for (auto parent = TreeView_GetParent(tree, selected); parent;
                       parent = TreeView_GetParent(tree, parent))
                      TreeView_Expand(tree, parent, TVE_EXPAND);
                  TreeView_SelectItem(tree, selected);
                  TreeView_EnsureVisible(tree, selected);
              }
              populating = false;
              if (restoreNativeView)
              {
                  databaseRestorePending = false;
                  offset = databaseSavedOffset;
                  selector = databaseSavedSelector;
                  SetWindowTextW(query, databaseSavedFilter.c_str());
                  if (databaseSavedCategory &&
                      std::any_of(categoryRows.begin(), categoryRows.end(), [this](const auto &row)
                                  { return row.at("index").as_uint() == *databaseSavedCategory; }))
                  {
                      category = databaseSavedCategory;
                      SetWindowTextW(details, Monstey_CategoryDetails(*category).c_str());
                      LoadRecords();
                  }
                  else if (!categoryRows.empty() && databaseSavedCategory)
                      SelectCategory(preferred);
              }
              else if (!categoryRows.empty())
                  SelectCategory(preferred);
          });
}

const J *Monstey_App::Monstey_CategoryRow(std::uint32_t index) const
{
    const auto found = std::find_if(categoryRows.begin(), categoryRows.end(),
                                    [index](const auto &row) { return row.at("index").as_uint() == index; });
    return found == categoryRows.end() ? nullptr : &*found;
}

std::wstring Monstey_App::Monstey_CategoryDetails(std::uint32_t index) const
{
    const auto *row = Monstey_CategoryRow(index);
    if (!row)
        return {};
    const auto presentation = Monstey_PresentCategory(row->at("name").as_string(), row->at("schema").as_string(),
                                                      row->at("path").as_string());
    return L"Category path: " + Monstey_Wide(presentation.categoryPath) + L"\r\nSchema name: " +
           Monstey_Wide(presentation.schemaName) + L"\r\nSource path: " + Monstey_Wide(presentation.sourcePath);
}

void Monstey_App::SelectCategory(int index)
{
    if (Monstey_DatabaseForm())
        return;
    if (index < 0 || static_cast<std::size_t>(index) >= categoryRows.size())
        return;
    category = static_cast<std::uint32_t>(categoryRows[static_cast<std::size_t>(index)].at("index").as_uint());
    offset = 0;
    selector.clear();
    databaseTreeInspectSelector.clear();
    SetWindowTextW(details, Monstey_CategoryDetails(*category).c_str());
    if (!smokeReport.empty() && !smokeStructure && !smokeNativeProfile && !Monstey_SowAtomSmoke &&
        smokeOperation.empty())
        SetWindowTextW(query, L"Float_CurrentHealth");
    LoadRecords();
}

void Monstey_App::Transaction(std::string operation, J arguments)
{
    if (!arguments.is_object())
        arguments = J::object();
    Queue(operation, arguments,
          [this](const J &reply)
          {
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              UpdateState(reply.at("result"));
              ShowReport(reply.at("result"));
              Refresh();
          });
}

} // namespace Monstey::Monstey_AppInternal
