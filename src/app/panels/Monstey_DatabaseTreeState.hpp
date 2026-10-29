#pragma once

#include "Monstey_CategoryPresentation.hpp"
#include <windows.h>
#include <commctrl.h>

namespace Monstey::Monstey_AppInternal
{

inline constexpr LPARAM Monstey_DatabaseFolderToken = -1;
inline constexpr LPARAM Monstey_DatabaseRootToken = -2;
inline constexpr LPARAM Monstey_DatabaseLoadingToken = -3;

struct Monstey_DatabaseTreeBinding
{
    HTREEITEM item = nullptr;
    Monstey_DatabaseTreeValue value;
    std::string label;
};

struct Monstey_DatabaseCategoryNode
{
    HTREEITEM item = nullptr;
    HTREEITEM more = nullptr;
    std::size_t categoryRow = 0;
    std::size_t nextOffset = 0;
    std::uint64_t request = 0;
    bool loading = false;
    bool loaded = false;
};

} // namespace Monstey::Monstey_AppInternal
