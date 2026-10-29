#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

namespace
{
class Monstey_TreePopulationScope
{
    bool *target_;
    bool previous_;

  public:
    explicit Monstey_TreePopulationScope(bool &target) : target_(&target), previous_(target)
    {
        target = true;
    }
    ~Monstey_TreePopulationScope()
    {
        Restore();
    }
    void Restore() noexcept
    {
        if (target_)
        {
            *target_ = previous_;
            target_ = nullptr;
        }
    }
};

int CALLBACK Monstey_CompareDatabaseTreeItems(LPARAM a, LPARAM b, LPARAM context)
{
    const auto &app = *reinterpret_cast<const Monstey_App *>(context);
    const auto first = app.databaseTreeBindings.find(a), second = app.databaseTreeBindings.find(b);
    if (first == app.databaseTreeBindings.end() || second == app.databaseTreeBindings.end())
        return first == app.databaseTreeBindings.end() ? (second == app.databaseTreeBindings.end() ? 0 : 1) : -1;
    const auto order = Monstey_CompareDatabaseTreeLabels(first->second.value.kind, first->second.label,
                                                         second->second.value.kind, second->second.label);
    return order != 0 ? order : a < b ? -1 : a > b ? 1 : 0;
}
} // namespace

void Monstey_App::Monstey_ResetDatabaseTree()
{
    ++databaseTreeGeneration;
    databaseTreeRoot = nullptr;
    databaseTreeBindings.clear();
    databaseTreeCategories.clear();
    databaseTreeInspectSelector.clear();
    databaseTreeQuery = Monstey_WindowText(query);
    databaseTreeRevision = revision;
    databaseTreeSession = generation;
}

LPARAM Monstey_App::Monstey_BindDatabaseTreeItem(HTREEITEM item, Monstey_DatabaseTreeValue value, std::string label)
{
    Monstey_Require(item != nullptr, "Cannot insert Database tree item");
    Monstey_Require(databaseTreeNextToken < std::numeric_limits<LPARAM>::max(), "Database tree token space exhausted");
    const auto token = databaseTreeNextToken++;
    TVITEMW update{};
    update.mask = TVIF_PARAM;
    update.hItem = item;
    update.lParam = token;
    Monstey_Require(TreeView_SetItem(tree, &update) != FALSE, "Cannot bind Database tree item");
    databaseTreeBindings.emplace(token, Monstey_DatabaseTreeBinding{item, std::move(value), std::move(label)});
    return token;
}

bool Monstey_App::Monstey_DatabaseTreeItemCurrent(LPARAM token, HTREEITEM item) const
{
    const auto found = databaseTreeBindings.find(token);
    if (found == databaseTreeBindings.end() || found->second.item != item)
        return false;
    TVITEMW current{};
    current.mask = TVIF_PARAM;
    current.hItem = item;
    return TreeView_GetItem(tree, &current) != FALSE && current.lParam == token;
}

void Monstey_App::Monstey_AddDatabaseTreePlaceholder(HTREEITEM parent)
{
    TVINSERTSTRUCTW insert{};
    insert.hParent = parent;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM;
    insert.item.pszText = const_cast<wchar_t *>(L"Expand to load records");
    insert.item.lParam = Monstey_DatabaseLoadingToken;
    Monstey_Require(TreeView_InsertItem(tree, &insert) != nullptr, "Cannot insert Database tree loading item");
}

void Monstey_App::Monstey_ClearDatabaseTreeRecords(std::uint32_t index)
{
    const auto found = databaseTreeCategories.find(index);
    if (found == databaseTreeCategories.end())
        return;
    auto &node = found->second;
    ++node.request;
    node.loading = false;
    node.loaded = false;
    node.more = nullptr;
    node.nextOffset = 0;
    std::erase_if(databaseTreeBindings,
                  [index](const auto &entry)
                  {
                      return entry.second.value.categoryIndex == index &&
                             entry.second.value.kind != Monstey_DatabaseTreeKind::Category;
                  });
    while (const auto child = TreeView_GetChild(tree, node.item))
        TreeView_DeleteItem(tree, child);
    Monstey_AddDatabaseTreePlaceholder(node.item);
}

void Monstey_App::Monstey_RefreshDatabaseTreeQuery()
{
    const auto filter = Monstey_WindowText(query);
    if (filter == databaseTreeQuery && revision == databaseTreeRevision && generation == databaseTreeSession)
        return;
    Monstey_TreePopulationScope population(populating);
    for (const auto &[index, node] : databaseTreeCategories)
    {
        TreeView_Expand(tree, node.item, TVE_COLLAPSE);
        Monstey_ClearDatabaseTreeRecords(index);
    }
    databaseTreeQuery = filter;
    databaseTreeRevision = revision;
    databaseTreeSession = generation;
    databaseTreeInspectSelector.clear();
    population.Restore();
}

void Monstey_App::Monstey_LoadDatabaseTreeRecords(std::uint32_t index, std::size_t pageOffset)
{
    if (closing || tab != 0 || databaseNativeMode)
        return;
    Monstey_RefreshDatabaseTreeQuery();
    const auto found = databaseTreeCategories.find(index);
    if (found == databaseTreeCategories.end())
        return;
    auto &node = found->second;
    if (node.loading || (pageOffset == 0 && node.loaded) ||
        (pageOffset != 0 && (!node.loaded || pageOffset != node.nextOffset)))
        return;
    TVITEMW identity{};
    identity.mask = TVIF_PARAM;
    identity.hItem = node.item;
    if (TreeView_GetItem(tree, &identity) == FALSE || !Monstey_DatabaseTreeItemCurrent(identity.lParam, node.item))
        return;
    node.loading = true;
    const auto request = ++node.request;
    const auto requestedEpoch = epoch, requestedTree = databaseTreeGeneration;
    const auto requestedSession = generation, requestedRevision = revision;
    const auto requestedQuery = databaseTreeQuery;
    const auto handle = node.item;
    const auto token = identity.lParam;
    const auto row = node.categoryRow;
    Queue(
        "records",
        J::object({{"category_index", index},
                   {"query", Monstey_Utf8(requestedQuery)},
                   {"offset", pageOffset},
                   {"limit", 250}}),
        [this, index, row, pageOffset, request, requestedEpoch, requestedTree, requestedSession, requestedRevision,
         requestedQuery, handle, token](const J &reply)
        {
            if (closing || tab != 0 || databaseNativeMode || epoch != requestedEpoch ||
                databaseTreeGeneration != requestedTree || generation != requestedSession ||
                revision != requestedRevision || databaseTreeQuery != requestedQuery ||
                Monstey_WindowText(query) != requestedQuery)
                return;
            const auto current = databaseTreeCategories.find(index);
            if (current == databaseTreeCategories.end() || current->second.item != handle ||
                current->second.categoryRow != row || current->second.request != request ||
                !Monstey_DatabaseTreeItemCurrent(token, handle))
                return;
            const auto &binding = databaseTreeBindings.at(token).value;
            if (binding.kind != Monstey_DatabaseTreeKind::Category || binding.categoryIndex != index ||
                binding.categoryRow != row)
                return;
            auto &state = current->second;
            state.loading = false;
            if (!reply.at("ok").as_bool())
            {
                Error(reply.at("error").at("message").as_string());
                return;
            }
            const auto &result = reply.at("result");
            std::vector<Monstey_DatabaseRecordName> names;
            for (const auto &record : result.at("rows").as_array())
            {
                Monstey_Require(record.at("category_index").as_uint() == index,
                                "Database tree page returned a different native category");
                names.push_back({record.at("name").is_null() ? std::string{} : record.at("name").as_string(),
                                 record.at("reference").as_string()});
            }
            const auto page = Monstey_DatabaseRecordPage(index, row, names, pageOffset, result.at("total").as_uint());
            Monstey_TreePopulationScope population(populating);
            const bool expanded = (TreeView_GetItemState(tree, handle, TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
            const bool syncSelection = TreeView_GetSelection(tree) == handle && category == index;
            if (pageOffset == 0)
            {
                std::erase_if(databaseTreeBindings,
                              [index](const auto &entry)
                              {
                                  return entry.second.value.categoryIndex == index &&
                                         entry.second.value.kind != Monstey_DatabaseTreeKind::Category;
                              });
                while (const auto child = TreeView_GetChild(tree, handle))
                    TreeView_DeleteItem(tree, child);
            }
            else if (state.more)
            {
                const auto oldMore = state.more;
                std::erase_if(databaseTreeBindings,
                              [oldMore](const auto &entry) { return entry.second.item == oldMore; });
                TreeView_DeleteItem(tree, oldMore);
            }
            state.more = nullptr;
            for (const auto &record : page)
            {
                auto text = Monstey_Wide(record.label);
                TVINSERTSTRUCTW insert{};
                insert.hParent = handle;
                insert.hInsertAfter = TVI_LAST;
                insert.item.mask = TVIF_TEXT;
                insert.item.pszText = text.data();
                const auto item = TreeView_InsertItem(tree, &insert);
                Monstey_BindDatabaseTreeItem(item, record.value, record.label);
                if (record.value.kind == Monstey_DatabaseTreeKind::More)
                    state.more = item;
            }
            TVSORTCB sort{};
            sort.hParent = handle;
            sort.lpfnCompare = Monstey_CompareDatabaseTreeItems;
            sort.lParam = reinterpret_cast<LPARAM>(this);
            TreeView_SortChildrenCB(tree, &sort, FALSE);
            state.loaded = true;
            state.nextOffset = pageOffset + names.size();
            if (expanded)
                TreeView_Expand(tree, handle, TVE_EXPAND);
            population.Restore();
            if (syncSelection)
                Monstey_SyncDatabaseTreeSelection();
            if (!smokeReport.empty())
                SmokeStep();
        },
        true, false);
}

void Monstey_App::Monstey_ExpandDatabaseTreeItem(LPARAM token)
{
    const auto found = databaseTreeBindings.find(token);
    if (found == databaseTreeBindings.end() || found->second.value.kind != Monstey_DatabaseTreeKind::Category ||
        !Monstey_DatabaseTreeItemCurrent(token, found->second.item))
        return;
    Monstey_LoadDatabaseTreeRecords(*found->second.value.categoryIndex);
}

void Monstey_App::Monstey_SelectDatabaseTreeItem(LPARAM token, bool inspect)
{
    if (closing || tab != 0 || databaseNativeMode)
        return;
    Monstey_RefreshDatabaseTreeQuery();
    const auto found = databaseTreeBindings.find(token);
    if (found == databaseTreeBindings.end() || !Monstey_DatabaseTreeItemCurrent(token, found->second.item))
        return;
    const auto value = found->second.value;
    if (value.kind == Monstey_DatabaseTreeKind::Category)
    {
        if (!inspect)
            SelectCategory(static_cast<int>(*value.categoryRow));
        return;
    }
    if (value.kind == Monstey_DatabaseTreeKind::More)
    {
        if (!inspect)
            Monstey_LoadDatabaseTreeRecords(*value.categoryIndex, value.offset);
        return;
    }
    if (value.kind != Monstey_DatabaseTreeKind::Record)
        return;
    const bool currentPage = category == value.categoryIndex && offset == value.offset;
    category = value.categoryIndex;
    offset = value.offset;
    selector = value.selector;
    databaseTreeInspectSelector = inspect ? value.selector : std::string{};
    if (currentPage)
    {
        for (std::size_t i = 0; i < rowData.size(); ++i)
            if (rowData[i].at("reference").as_string() == value.selector)
            {
                Monstey_TreePopulationScope population(populating);
                Monstey_ListSelect(records, static_cast<int>(i));
                population.Restore();
                databaseTreeInspectSelector.clear();
                ReadRecord(static_cast<int>(i), inspect);
                return;
            }
    }
    SetWindowTextW(details, Monstey_CategoryDetails(*category).c_str());
    LoadRecords();
}

void Monstey_App::Monstey_SyncDatabaseTreeSelection()
{
    if (tab != 0 || databaseNativeMode || !category)
        return;
    HTREEITEM selected = nullptr;
    for (const auto &[token, binding] : databaseTreeBindings)
    {
        (void)token;
        if (binding.value.kind == Monstey_DatabaseTreeKind::Record && binding.value.categoryIndex == category &&
            binding.value.selector == selector)
        {
            selected = binding.item;
            break;
        }
    }
    if (!selected)
    {
        const auto found = databaseTreeCategories.find(*category);
        if (found != databaseTreeCategories.end())
            selected = found->second.item;
    }
    if (!selected)
        return;
    Monstey_TreePopulationScope population(populating);
    TreeView_SelectItem(tree, selected);
    TreeView_EnsureVisible(tree, selected);
    population.Restore();
}

} // namespace Monstey::Monstey_AppInternal
