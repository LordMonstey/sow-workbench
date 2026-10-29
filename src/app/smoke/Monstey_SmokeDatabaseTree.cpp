#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

bool Monstey_App::Monstey_DatabaseTreeSmokeStep()
{
    if (databaseTreeSmokeStage >= 3)
        return true;
    if (!Monstey_SowAtomAvailable() || !category)
        return false;
    const auto found = databaseTreeCategories.find(*category);
    Monstey_Require(found != databaseTreeCategories.end(), "Smoke category missing from Database tree");
    const auto categoryItem = found->second.item;
    if (databaseTreeSmokeStage == 0)
    {
        Monstey_Require(databaseTreeRoot && TreeView_GetRoot(tree) == databaseTreeRoot,
                        "Database tree has no GameDatabase root");
        auto item = categoryItem;
        for (const auto *expected : {L"Level", L"Alertness", L"AI", L"GameDatabase"})
        {
            wchar_t label[128]{};
            TVITEMW value{};
            value.mask = TVIF_TEXT;
            value.hItem = item;
            value.pszText = label;
            value.cchTextMax = static_cast<int>(std::size(label));
            Monstey_Require(item && TreeView_GetItem(tree, &value) != FALSE && std::wstring_view(label) == expected,
                            "Database tree category ancestry differs from slash path");
            item = TreeView_GetParent(tree, item);
        }
        Monstey_Require(item == nullptr && !found->second.loaded,
                        "Database tree eagerly loaded records or fabricated another root");
        smoke["database_tree_root_present"] = true;
        databaseTreeSmokeStage = 1;
        TreeView_Expand(tree, categoryItem, TVE_EXPAND);
        return false;
    }
    if (databaseTreeSmokeStage == 1)
    {
        if (!found->second.loaded || found->second.loading)
            return false;
        HTREEITEM recordItem = nullptr;
        for (const auto &[token, binding] : databaseTreeBindings)
        {
            (void)token;
            if (binding.value.kind == Monstey_DatabaseTreeKind::Record && binding.value.categoryIndex == category &&
                binding.value.selector == selector)
            {
                Monstey_Require(binding.label == "Monstey_StorageProbe" &&
                                    TreeView_GetParent(tree, binding.item) == categoryItem &&
                                    binding.value.offset == 0 && binding.value.recordRow == 0,
                                "Database tree record binding differs from native page");
                recordItem = binding.item;
                break;
            }
        }
        Monstey_Require(recordItem != nullptr, "Lazy Database expansion omitted the actual record");
        smoke["database_tree_lazy_category_expanded"] = true;
        databaseTreeSmokeStage = 2;
        const auto previous = populating;
        populating = true;
        TreeView_SelectItem(tree, categoryItem);
        populating = previous;
        Monstey_Require(TreeView_SelectItem(tree, recordItem) != FALSE, "Cannot select actual Database record leaf");
        return false;
    }
    if (metadata.empty() || metadata.value("selector", "") != J(selector) || fieldData.empty())
        return false;
    const auto selected = TreeView_GetSelection(tree);
    const auto binding = std::find_if(databaseTreeBindings.begin(), databaseTreeBindings.end(),
                                      [selected](const auto &entry) { return entry.second.item == selected; });
    Monstey_Require(binding != databaseTreeBindings.end() &&
                        binding->second.value.kind == Monstey_DatabaseTreeKind::Record &&
                        binding->second.value.categoryIndex == category && binding->second.value.selector == selector &&
                        metadata.at("category_index").as_uint() == *category,
                    "Database record leaf did not dispatch native properties");
    const auto gridRow = ListView_GetNextItem(records, -1, LVNI_SELECTED);
    Monstey_Require(gridRow >= 0 && static_cast<std::size_t>(gridRow) < rowData.size() &&
                        rowData[static_cast<std::size_t>(gridRow)].at("reference") == J(selector) &&
                        fieldData.size() == 1 && fieldData[0].at("name") == J("StorageValue") &&
                        ListView_GetItemCount(fields) == 1,
                    "Database tree selection did not synchronize grid and real properties");
    smoke["database_tree_record_selected"] = true;
    smoke["database_tree_properties_loaded"] = true;
    smoke["database_tree_grid_synced"] = true;
    databaseTreeSmokeStage = 3;
    return true;
}

} // namespace Monstey::Monstey_AppInternal
