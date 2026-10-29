#include "Monstey_CategoryPresentation.hpp"

#include <algorithm>
#include <map>
#include <numeric>

namespace Monstey::Monstey_AppInternal
{

Monstey_CategoryPresentation Monstey_PresentCategory(std::string_view rawName, std::string_view rawSchema,
                                                     std::string_view rawPath)
{
    return {std::string(rawName), std::string(rawSchema), std::string(rawPath)};
}

std::string_view Monstey_CategoryDisplayPath(const Monstey_CategoryPresentation &category) noexcept
{
    return category.categoryPath.empty() ? std::string_view(category.schemaName)
                                         : std::string_view(category.categoryPath);
}

std::vector<Monstey_CategoryTreeItem> Monstey_CategoryTree(const std::vector<Monstey_CategoryPresentation> &categories)
{
    std::vector<std::size_t> order(categories.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(
        order.begin(), order.end(), [&](auto a, auto b)
        { return Monstey_CategoryDisplayPath(categories[a]) < Monstey_CategoryDisplayPath(categories[b]); });
    std::map<std::vector<std::string>, std::size_t> folders;
    std::vector<Monstey_CategoryTreeItem> items;
    for (const auto row : order)
    {
        const auto path = Monstey_CategoryDisplayPath(categories[row]);
        std::vector<std::string> parts;
        std::size_t begin = 0;
        for (;;)
        {
            const auto end = path.find('/', begin);
            parts.emplace_back(path.substr(begin, end == std::string_view::npos ? end : end - begin));
            if (end == std::string_view::npos)
                break;
            begin = end + 1;
        }
        std::optional<std::size_t> parent;
        std::vector<std::string> prefix;
        for (std::size_t i = 0; i + 1 < parts.size(); ++i)
        {
            prefix.push_back(parts[i]);
            auto found = folders.find(prefix);
            if (found == folders.end())
            {
                const auto index = items.size();
                items.push_back({parts[i].empty() ? "(empty)" : parts[i], parent, std::nullopt});
                found = folders.emplace(prefix, index).first;
            }
            parent = found->second;
        }
        items.push_back({parts.back().empty() ? "(unnamed category)" : parts.back(), parent, row,
                         Monstey_DatabaseTreeKind::Category});
    }
    return items;
}

std::vector<Monstey_CategoryTreeItem> Monstey_DatabaseCategoryTree(
    const std::vector<Monstey_CategoryPresentation> &categories)
{
    auto items = Monstey_CategoryTree(categories);
    for (auto &item : items)
        item.parent = item.parent ? *item.parent + 1 : 0;
    items.insert(items.begin(), {"GameDatabase", std::nullopt, std::nullopt, Monstey_DatabaseTreeKind::Root});
    return items;
}

std::vector<Monstey_DatabaseRecordTreeItem> Monstey_DatabaseRecordPage(
    std::uint32_t categoryIndex, std::size_t categoryRow, const std::vector<Monstey_DatabaseRecordName> &records,
    std::size_t offset, std::size_t total)
{
    std::vector<Monstey_DatabaseRecordTreeItem> items;
    items.reserve(records.size() + 1);
    for (std::size_t i = 0; i < records.size(); ++i)
    {
        const auto &record = records[i];
        const auto label = record.name.empty() ? "(anonymous) [" + record.selector + "]" : record.name;
        items.push_back(
            {label, {Monstey_DatabaseTreeKind::Record, categoryRow, categoryIndex, record.selector, offset, i}});
    }
    if (!records.empty() && offset < total && records.size() < total - offset)
        items.push_back(
            {"More...", {Monstey_DatabaseTreeKind::More, categoryRow, categoryIndex, {}, offset + records.size()}});
    return items;
}

int Monstey_CompareDatabaseTreeLabels(Monstey_DatabaseTreeKind kindA, std::string_view labelA,
                                      Monstey_DatabaseTreeKind kindB, std::string_view labelB) noexcept
{
    const bool sentinelA = kindA != Monstey_DatabaseTreeKind::Record;
    const bool sentinelB = kindB != Monstey_DatabaseTreeKind::Record;
    if (sentinelA != sentinelB)
        return sentinelA ? 1 : -1;
    const auto fold = [](unsigned char c)
    { return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c + ('a' - 'A')) : c; };
    const auto length = std::min(labelA.size(), labelB.size());
    for (std::size_t i = 0; i < length; ++i)
    {
        const auto a = fold(static_cast<unsigned char>(labelA[i]));
        const auto b = fold(static_cast<unsigned char>(labelB[i]));
        if (a != b)
            return a < b ? -1 : 1;
    }
    return labelA.size() < labelB.size() ? -1 : labelA.size() > labelB.size() ? 1 : 0;
}

} // namespace Monstey::Monstey_AppInternal
