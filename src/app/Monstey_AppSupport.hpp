#pragma once

#include "Monstey_Runtime.hpp"
#include <windows.h>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Monstey::Monstey_AppInternal
{

using J = Monstey_Json;

extern std::deque<std::filesystem::path> Monstey_SmokeDialogPaths;
extern std::size_t Monstey_SmokeDialogCount;

std::optional<std::filesystem::path> Monstey_DirectoryDialog(HWND parent, const wchar_t *title);
std::wstring Monstey_WindowText(HWND window);
std::wstring Monstey_Display(const J &value);
void Monstey_ListColumn(HWND list, int index, const wchar_t *name, int width);
void Monstey_ListCell(HWND list, int row, int column, std::wstring text);
void Monstey_ListSelect(HWND list, int row);
std::optional<std::filesystem::path> Monstey_FileDialog(HWND parent, bool save, const wchar_t *filter,
                                                        const wchar_t *extension, const wchar_t *title);
const wchar_t *Monstey_SourceFilter(int tab);
std::string Monstey_JsonPath(const std::filesystem::path &path);
std::wstring Monstey_ModelOperationLabel(std::string_view operation);
const wchar_t *Monstey_ModelFileFilter(std::string_view operation, std::string_view key);
const wchar_t *Monstey_ModelOutputExtension(std::string_view operation);
std::filesystem::path Monstey_ExecutableDirectory();
std::filesystem::path Monstey_DefaultSchema();
std::filesystem::path Monstey_DefaultFxSchema();
J Monstey_StructureText(const char *name, const char *label, const char *value = "", bool optional = false,
                        bool allowEmpty = false);
J Monstey_StructureChoice(const char *name, const char *label, const J &choices, const J &value);
J Monstey_StructureDefault(unsigned type);
J Monstey_StructureTypes();
J Monstey_FxNumber(const char *name, const char *label, const J &value, bool integer = false);
J Monstey_FxValue(const J &record, std::string_view name, const J &fallback = J{});
std::string Monstey_FxLabel(const J &record);
void Monstey_FxMenu(HMENU menu);
std::wstring Monstey_TypedLines(const J &value, unsigned type);
J Monstey_ParseTypedLines(const std::wstring &input, unsigned type);

} // namespace Monstey::Monstey_AppInternal
