#include "../src/app/panels/Monstey_CategoryPresentation.hpp"

#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
using namespace Monstey::Monstey_AppInternal;

void Monstey_Check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::size_t Monstey_Leaf(const std::vector<Monstey_CategoryTreeItem> &items, std::size_t row)
{
    std::optional<std::size_t> found;
    for (std::size_t i = 0; i < items.size(); ++i)
        if (items[i].categoryRow == row)
        {
            Monstey_Check(!found, "An input category was duplicated");
            found = i;
        }
    Monstey_Check(found.has_value(), "An input category was omitted");
    return *found;
}

std::string Monstey_TreePath(const std::vector<Monstey_CategoryTreeItem> &items, std::size_t index)
{
    std::vector<std::string> parts;
    for (;;)
    {
        Monstey_Check(index < items.size(), "Invalid tree parent");
        parts.push_back(items[index].label);
        if (!items[index].parent)
            break;
        Monstey_Check(*items[index].parent < index, "Parent must precede child");
        index = *items[index].parent;
        Monstey_Check(!items[index].categoryRow, "Folder must not select a category");
    }
    std::reverse(parts.begin(), parts.end());
    std::string path;
    for (const auto &part : parts)
    {
        if (!path.empty())
            path += '/';
        path += part;
    }
    return path;
}

} // namespace

int main()
{
    using namespace Monstey::Monstey_AppInternal;
    try
    {
        std::size_t cases = 0;
        const auto test = [&](const char *name, const std::function<void()> &run)
        {
            run();
            ++cases;
            std::cout << "PASS " << name << '\n';
        };
        test("real_non_bijective_category_and_schema_names",
             []
             {
                 const std::vector<Monstey_CategoryPresentation> rows = {
                     Monstey_PresentCategory("Inventory.ItemGroup", "Inventory/ItemGroups",
                                             R"(\\Inventory\\ItemGroups\\gdb.category06)"),
                     Monstey_PresentCategory("Inventory.UITypeData", "Inventory/UIItemData/UITypeData",
                                             R"(\\Inventory\\UIItemData\\UITypeData\\gdb.category06)"),
                     Monstey_PresentCategory("Inventory.EquipmentPose", "Inventory/EquipmentPose",
                                             R"(\\Inventory\\EquipmentPose\\gdb.category06)")};
                 const std::vector<unsigned> nativeIndices = {175, 169, 94};
                 const auto items = Monstey_CategoryTree(rows);
                 for (std::size_t i = 0; i < rows.size(); ++i)
                 {
                     const auto leaf = Monstey_Leaf(items, i);
                     Monstey_Check(Monstey_TreePath(items, leaf) == rows[i].categoryPath,
                                   "Category hierarchy must use raw schema/category path");
                     Monstey_Check(nativeIndices[*items[leaf].categoryRow] == nativeIndices[i],
                                   "Visual sorting changed the actual native index");
                 }
                 Monstey_Check(items[Monstey_Leaf(items, 0)].label == "ItemGroups",
                               "Do not derive singular/plural category from dotted schema");
                 Monstey_Check(items[Monstey_Leaf(items, 1)].label == "UITypeData" &&
                                   rows[1].schemaName == "Inventory.UITypeData",
                               "Extra category ancestor must not rewrite schema identity");
             });
        test("duplicate_schema_types_retain_distinct_leaf_mappings",
             []
             {
                 const auto rows = std::vector{Monstey_PresentCategory("Combat.Int", "Combat/Weapon/Int", ""),
                                               Monstey_PresentCategory("Combat.Int", "AI/Tuning/Int", ""),
                                               Monstey_PresentCategory("Combat.Int", "Progression/Int", "")};
                 const auto items = Monstey_CategoryTree(rows);
                 Monstey_Check(Monstey_TreePath(items, Monstey_Leaf(items, 0)) == "Combat/Weapon/Int" &&
                                   Monstey_TreePath(items, Monstey_Leaf(items, 1)) == "AI/Tuning/Int" &&
                                   Monstey_TreePath(items, Monstey_Leaf(items, 2)) == "Progression/Int",
                               "Shared type must not merge unrelated native categories");
             });
        test("structure_without_authoring_path",
             []
             {
                 const auto row =
                     Monstey_PresentCategory("InventoryWeaponModelPicker", "_StructuresInventoryWeaponModelPicker", "");
                 const auto items = Monstey_CategoryTree({row});
                 Monstey_Check(items.size() == 1 && !items[0].parent && items[0].categoryRow == std::size_t{0} &&
                                   items[0].label == "_StructuresInventoryWeaponModelPicker",
                               "Native structure key is an opaque root category");
             });
        test("dotted_identifiers_are_not_separators",
             []
             {
                 const auto row = Monstey_PresentCategory("Schema.Type.v2", "Folder.v1/Leaf.Name", "");
                 const auto items = Monstey_CategoryTree({row});
                 Monstey_Check(items.size() == 2 && items[0].label == "Folder.v1" && items[1].label == "Leaf.Name" &&
                                   row.schemaName == "Schema.Type.v2",
                               "Only slash in category key defines hierarchy");
             });
        test("authoring_path_is_kept_exact_and_not_used_as_tree",
             []
             {
                 const std::string source = R"(\\Inventory\\UIItemData\\UITypeData\\gdb.category06)";
                 const auto row =
                     Monstey_PresentCategory("Inventory.UITypeData", "Inventory/UIItemData/UITypeData", source);
                 const auto items = Monstey_CategoryTree({row});
                 Monstey_Check(row.sourcePath == source && row.schemaName == "Inventory.UITypeData" &&
                                   row.categoryPath == "Inventory/UIItemData/UITypeData",
                               "Presentation must not normalize stored atoms");
                 Monstey_Check(std::none_of(items.begin(), items.end(),
                                            [](const auto &item)
                                            {
                                                return item.label.find("gdb.category06") != std::string::npos ||
                                                       item.label.find('\\') != std::string::npos;
                                            }),
                               "Authoring filename leaked into category tree");
             });
        test("common_folder_is_not_a_category",
             []
             {
                 const auto items = Monstey_CategoryTree(
                     {Monstey_PresentCategory("B", "Root/B", ""), Monstey_PresentCategory("A", "Root/A", "")});
                 Monstey_Check(items.size() == 3 && items[0].label == "Root" && !items[0].categoryRow &&
                                   !items[0].parent,
                               "Shared parent folder must not map to native category zero");
                 Monstey_Check(items[Monstey_Leaf(items, 0)].parent == std::size_t{0} &&
                                   items[Monstey_Leaf(items, 1)].parent == std::size_t{0},
                               "Sibling categories must share one folder");
             });
        test("category_matching_folder_prefix_keeps_own_leaf",
             []
             {
                 const auto items = Monstey_CategoryTree({Monstey_PresentCategory("Type.Root", "Root", ""),
                                                          Monstey_PresentCategory("Type.Child", "Root/Child", "")});
                 const auto rootLeaf = Monstey_Leaf(items, 0);
                 const auto child = Monstey_Leaf(items, 1);
                 Monstey_Check(!items[rootLeaf].parent && items[child].parent && *items[child].parent != rootLeaf &&
                                   !items[*items[child].parent].categoryRow,
                               "Real category and synthetic parent must not share selection payload");
             });
        test("identical_display_paths_do_not_drop_native_rows",
             []
             {
                 const auto items = Monstey_CategoryTree({Monstey_PresentCategory("First.Type", "Shared/Leaf", ""),
                                                          Monstey_PresentCategory("Second.Type", "Shared/Leaf", "")});
                 Monstey_Check(Monstey_Leaf(items, 0) != Monstey_Leaf(items, 1) &&
                                   Monstey_TreePath(items, Monstey_Leaf(items, 0)) == "Shared/Leaf" &&
                                   Monstey_TreePath(items, Monstey_Leaf(items, 1)) == "Shared/Leaf",
                               "Presentation must retain both actual row mappings");
             });
        test("case_and_utf8_are_preserved",
             []
             {
                 const auto rows = std::vector{Monstey_PresentCategory("Réglages.État", "Réglages/État", "source"),
                                               Monstey_PresentCategory("Lower.Type", "réglages/état", "")};
                 const auto items = Monstey_CategoryTree(rows);
                 Monstey_Check(items.size() == 4 &&
                                   Monstey_TreePath(items, Monstey_Leaf(items, 0)) == "Réglages/État" &&
                                   Monstey_TreePath(items, Monstey_Leaf(items, 1)) == "réglages/état" &&
                                   rows[0].schemaName == "Réglages.État",
                               "No case folding, path normalization or UTF8 mutation");
             });
        test("empty_category_key_falls_back_to_intact_schema_name",
             []
             {
                 const auto row = Monstey_PresentCategory("Monstey.Legacy.Type", "", "authoring/ignored");
                 const auto items = Monstey_CategoryTree({row});
                 Monstey_Check(items.size() == 1 && items[0].label == "Monstey.Legacy.Type" && row.categoryPath.empty(),
                               "Fallback must not invent slash category or alter stored key");
             });
        test("empty_input_has_no_synthetic_category",
             [] { Monstey_Check(Monstey_CategoryTree({}).empty(), "Empty database gained a category"); });
        test("registry_root_and_category_chain",
             []
             {
                 const auto items = Monstey_DatabaseCategoryTree(
                     {Monstey_PresentCategory("AI.Alertness.Level", "AI/Alertness/Level", "")});
                 Monstey_Check(items.size() == 4 && items[0].kind == Monstey_DatabaseTreeKind::Root &&
                                   items[0].label == "GameDatabase" && !items[0].parent && !items[0].categoryRow &&
                                   items[1].label == "AI" && items[1].parent == std::size_t{0} &&
                                   items[2].label == "Alertness" && items[2].parent == std::size_t{1} &&
                                   items[3].label == "Level" && items[3].parent == std::size_t{2} &&
                                   items[3].categoryRow == std::size_t{0} &&
                                   items[3].kind == Monstey_DatabaseTreeKind::Category,
                               "Registry root/category identity or parent chain differs");
             });
        test("first_lazy_page_keeps_native_mapping_and_more_offset",
             []
             {
                 std::vector<Monstey_DatabaseRecordName> records;
                 for (std::size_t i = 0; i < 250; ++i)
                     records.push_back({"Record_" + std::to_string(i), "ref_" + std::to_string(i)});
                 const auto page = Monstey_DatabaseRecordPage(175, 42, records, 0, 750000);
                 Monstey_Check(page.size() == 251 && page.back().label == "More..." &&
                                   page.back().value.kind == Monstey_DatabaseTreeKind::More &&
                                   page.back().value.offset == std::size_t{250},
                               "Lazy first page must expose one bounded continuation");
                 for (std::size_t i = 0; i < 250; ++i)
                     Monstey_Check(page[i].value.kind == Monstey_DatabaseTreeKind::Record &&
                                       page[i].value.categoryIndex == std::uint32_t{175} &&
                                       page[i].value.categoryRow == std::size_t{42} &&
                                       page[i].value.selector == records[i].selector &&
                                       page[i].value.offset == std::size_t{0} && page[i].value.recordRow == i,
                                   "Record leaf lost native category, selector, page or row identity");
             });
        test("last_page_has_actual_offset_without_more",
             []
             {
                 const auto page =
                     Monstey_DatabaseRecordPage(169, 5, {{"LastA", "ref250"}, {"LastB", "ref251"}}, 250, 252);
                 Monstey_Check(page.size() == 2 && page[0].value.offset == std::size_t{250} &&
                                   page[1].value.offset == std::size_t{250} &&
                                   page[1].value.recordRow == std::size_t{1} && page[1].value.selector == "ref251" &&
                                   page[1].value.kind == Monstey_DatabaseTreeKind::Record,
                               "Continuation page must synchronize original native grid page");
             });
        test("same_record_label_retains_distinct_selectors",
             []
             {
                 const auto page =
                     Monstey_DatabaseRecordPage(342, 8, {{"Buddy", "0x15600001"}, {"Buddy", "clone:actual"}}, 0, 2);
                 Monstey_Check(page.size() == 2 && page[0].label == page[1].label &&
                                   page[0].value.selector == "0x15600001" && page[1].value.selector == "clone:actual" &&
                                   page[1].value.recordRow == std::size_t{1},
                               "Native record identity must not be derived from display name");
             });
        test("anonymous_records_use_actual_selector",
             []
             {
                 const auto page = Monstey_DatabaseRecordPage(94, 3, {{"", "0x05e00000"}}, 0, 1);
                 Monstey_Check(page.size() == 1 && page[0].label == "(anonymous) [0x05e00000]" &&
                                   page[0].value.selector == "0x05e00000",
                               "Anonymous record must remain selectable without a fabricated name");
             });
        test("empty_or_end_page_does_not_offer_an_infinite_continuation",
             []
             {
                 Monstey_Check(Monstey_DatabaseRecordPage(1, 0, {}, 0, 100).empty() &&
                                   Monstey_DatabaseRecordPage(1, 0, {}, 250, 250).empty(),
                               "Empty page must not create retry/more loops or fake record leaves");
                 const auto last = std::numeric_limits<std::size_t>::max();
                 const auto page = Monstey_DatabaseRecordPage(1, 0, {{"End", "actual"}}, last - 1, last);
                 Monstey_Check(page.size() == 1 && page[0].value.offset == last - 1,
                               "Bounded final page arithmetic must not wrap into More");
             });
        test("visible_name_sort_keeps_more_last_and_native_page_rows",
             []
             {
                 auto items = Monstey_DatabaseRecordPage(
                     29, 2, {{"Alert", "ref0"}, {"ActiveBuddy", "ref1"}, {"AcquiringTarget", "ref2"}}, 250, 1000);
                 std::stable_sort(
                     items.begin(), items.end(), [](const auto &a, const auto &b)
                     { return Monstey_CompareDatabaseTreeLabels(a.value.kind, a.label, b.value.kind, b.label) < 0; });
                 Monstey_Check(items.size() == 4 && items[0].label == "AcquiringTarget" &&
                                   items[0].value.recordRow == std::size_t{2} && items[0].value.selector == "ref2" &&
                                   items[0].value.offset == std::size_t{250} && items[1].label == "ActiveBuddy" &&
                                   items[1].value.recordRow == std::size_t{1} && items[2].label == "Alert" &&
                                   items[2].value.recordRow == std::size_t{0} && items[3].label == "More..." &&
                                   items[3].value.offset == std::size_t{253},
                               "Visual alphabetic ordering must preserve native record positions and continuation");
             });
        std::cout << "Category presentation: " << cases << " cases passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Category presentation: " << error.what() << '\n';
        return 1;
    }
}
