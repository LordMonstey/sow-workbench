#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Monstey::Monstey_AppInternal
{

struct Monstey_CategoryPresentation
{
    std::string schemaName;
    std::string categoryPath;
    std::string sourcePath;
};

enum class Monstey_DatabaseTreeKind
{
    Root,
    Folder,
    Category,
    Record,
    More,
    Loading
};

struct Monstey_DatabaseTreeValue
{
    Monstey_DatabaseTreeKind kind = Monstey_DatabaseTreeKind::Folder;
    std::optional<std::size_t> categoryRow;
    std::optional<std::uint32_t> categoryIndex;
    std::string selector;
    std::size_t offset = 0;
    std::size_t recordRow = 0;
};

struct Monstey_DatabaseRecordTreeItem
{
    std::string label;
    Monstey_DatabaseTreeValue value;
};

struct Monstey_DatabaseRecordName
{
    std::string name;
    std::string selector;
};

struct Monstey_CategoryTreeItem
{
    std::string label;
    std::optional<std::size_t> parent;
    std::optional<std::size_t> categoryRow;
    Monstey_DatabaseTreeKind kind = Monstey_DatabaseTreeKind::Folder;
};

Monstey_CategoryPresentation Monstey_PresentCategory(std::string_view rawName, std::string_view rawSchema,
                                                     std::string_view rawPath);
std::string_view Monstey_CategoryDisplayPath(const Monstey_CategoryPresentation &category) noexcept;
std::vector<Monstey_CategoryTreeItem> Monstey_CategoryTree(const std::vector<Monstey_CategoryPresentation> &categories);
std::vector<Monstey_CategoryTreeItem> Monstey_DatabaseCategoryTree(
    const std::vector<Monstey_CategoryPresentation> &categories);
std::vector<Monstey_DatabaseRecordTreeItem> Monstey_DatabaseRecordPage(
    std::uint32_t categoryIndex, std::size_t categoryRow, const std::vector<Monstey_DatabaseRecordName> &records,
    std::size_t offset, std::size_t total);
int Monstey_CompareDatabaseTreeLabels(Monstey_DatabaseTreeKind kindA, std::string_view labelA,
                                      Monstey_DatabaseTreeKind kindB, std::string_view labelB) noexcept;

} // namespace Monstey::Monstey_AppInternal
