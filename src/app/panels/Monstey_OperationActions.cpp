#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::EditOperationField(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= fieldData.size() || selectedOperation.empty())
        return;
    const auto field = fieldData[static_cast<std::size_t>(index)];
    const auto name = field.at("name").as_string();
    const auto op = selectedOperation.at("name").as_string();
    const auto &rule = field.at("argument_schema");
    if (name == "triangle_band")
    {
        auto choices = J::array({J::object({{"label", "Keep qualified per-face bands"}, {"value", -1}})});
        for (int band = 0; band <= 16; ++band)
            choices.as_array().push_back(J::object(
                {{"label", "Assign every supplied face to native band " + std::to_string(band)}, {"value", band}}));
        auto value = operationArguments.value(name, -1);
        if (!operationArguments.contains(name) && operationArguments.contains("options") &&
            operationArguments.at("options").is_object() && operationArguments.at("options").contains(name))
            value = operationArguments.at("options").at(name);
        SetStatus(L"Native bands select triangle draw ranges. Keep preserves face "
                  L"membership; an explicit band assigns all supplied faces.");
        StructureForm(
            L"Native triangle bands", J::array({Monstey_StructureChoice("band", "Face assignment", choices, value)}),
            [this, name, op](const J &values)
            {
                if (selectedOperation.empty() || selectedOperation.at("name").as_string() != op)
                    return;
                const auto band = values.at("band").as_int();
                Monstey_Require(band >= -1 && band <= 16, "Native triangle band is outside 0..16");
                if (band < 0)
                {
                    operationArguments.as_object().erase(name);
                    if (operationArguments.contains("options") && operationArguments.at("options").is_object())
                        operationArguments["options"].as_object().erase(name);
                }
                else
                    operationArguments[name] = band;
                operationPresets[op] = operationArguments;
                PopulateOperationFields();
            });
        return;
    }
    if ((rule.value("format", "").as_string() == "directory" || name == "game_root" || name == "converter_root") &&
        smokeOperation.empty())
    {
        const auto chosen = Monstey_DirectoryDialog(window, (L"Choose " + Monstey_Wide(name)).c_str());
        if (chosen)
        {
            operationArguments[name] = Monstey_JsonPath(*chosen);
            PopulateOperationFields();
        }
        return;
    }
    if (rule.value("format", "").is_string() && rule.value("format", "").as_string() == "path" &&
        smokeOperation.empty())
    {
        const bool output = name == "output" || name == "report" || name == "destination" || name == "output_blend" ||
                            name == "output_mesh";
        const auto chosen =
            Monstey_FileDialog(window, output, tab == 4 ? Monstey_ModelFileFilter(op, name) : L"All files\0*.*\0\0",
                               tab == 4 && output ? Monstey_ModelOutputExtension(op) : nullptr,
                               output ? L"Choose NEW output path" : L"Choose native input");
        if (chosen)
        {
            operationArguments[name] = Monstey_JsonPath(*chosen);
            PopulateOperationFields();
        }
        return;
    }
    auto item = std::make_shared<Monstey_Editor>();
    item->app = this;
    item->title = L"Argument " + Monstey_Wide(name);
    item->field = field;
    item->operation = "";
    item->generation = generation;
    item->revision = revision;
    item->success = [this, name, op](const J &value)
    {
        if (selectedOperation.empty() || selectedOperation.at("name").as_string() != op)
            return;
        operationArguments[name] = value;
        operationPresets[op] = operationArguments;
        PopulateOperationFields();
    };
    ShowEditor(item);
}

void Monstey_App::RunOperation()
{
    if (selectedOperation.empty())
        return;
    const auto name = selectedOperation.at("name").as_string();
    if (!selectedOperation.value("available", selectedOperation.value("implemented", true)).as_bool())
    {
        SetStatus(L"This operation is still being ported to C++.");
        return;
    }
    operationPresets[name] = operationArguments;
    const int originTab = tab;
    const bool directDatabaseRead =
        Monstey_DatabaseForm() && (name == "database_native_class" || name == "database_native_banks");
    const bool directStaticTreeRead = tab == 5 && name == "sow_world_navigation_static_tree";
    const auto beforeGeneration = generation, beforeRevision = revision;
    const auto arguments = tab == 5 && !directStaticTreeRead
                               ? J::object({{"name", name}, {"arguments", operationArguments}})
                               : operationArguments;
    Queue(tab == 5 && !directStaticTreeRead ? "world_call" : name, arguments,
          [this, name, originTab, arguments, directStaticTreeRead, directDatabaseRead, beforeGeneration,
           beforeRevision](const J &reply)
          {
              if (originTab != tab)
                  return;
              if (directDatabaseRead && !smokeReport.empty() && smokeDomain == 0 && name == smokeOperation)
              {
                  Monstey_Require(reply.at("generation").as_uint() == beforeGeneration &&
                                      reply.at("revision").as_uint() == beforeRevision,
                                  "Native Database inspection changed Runtime counters");
                  const bool ok = reply.at("ok").as_bool();
                  smoke["native_database_direct_dispatch"] = true;
                  smoke["native_dispatched_operation"] = name;
                  smoke["native_operation_completed"] = true;
                  smoke["native_operation_succeeded"] = ok;
                  if (ok)
                  {
                      Monstey_Require(smokeExpectedError.empty(),
                                      "Native operation unexpectedly succeeded instead of the requested error");
                      smoke["operation_result"] = reply.at("result");
                      ShowReport(J::object({{"operation", name}, {"result", reply.at("result")}}));
                  }
                  else
                  {
                      Monstey_Require(!smokeExpectedError.empty() &&
                                          reply.at("error").at("code").as_string() == smokeExpectedError,
                                      "Native Database operation failed outside its explicit expected-error contract");
                      smoke["native_operation_expected_error"] = reply.at("error");
                      smoke["native_operation_expected_error_observed"] = true;
                      ShowReport(J::object({{"operation", name}, {"error", reply.at("error")}}));
                  }
                  Monstey_DatabaseNativeSmokeFinish();
                  return;
              }
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              if (originTab == 4 && name == "model_import_blend" && reply.at("result").contains("output"))
              {
                  modelBlendPreview = std::filesystem::path(Monstey_Wide(reply.at("result").at("output").as_string()));
                  UpdateButtons();
              }
              else if (originTab == 4 && name == "model_export_blend" && arguments.contains("source"))
              {
                  modelBlendPreview = std::filesystem::path(Monstey_Wide(arguments.at("source").as_string()));
                  UpdateButtons();
              }
              ShowReport(J::object({{"operation", name}, {"result", reply.at("result")}}));
              if (!smokeOperation.empty() && name == smokeOperation)
              {
                  if (smoke.contains("triangle_band_expected"))
                  {
                      Monstey_Require(reply.at("result").at("triangle_band_policy").as_string() == "explicit_uniform" &&
                                          reply.at("result").at("triangle_band") == smoke.at("triangle_band_expected"),
                                      "Native model export did not use the selected triangle band");
                      smoke["triangle_band_result_verified"] = true;
                  }
                  if (directStaticTreeRead)
                  {
                      Monstey_Require(reply.at("generation").as_uint() == beforeGeneration &&
                                          reply.at("revision").as_uint() == beforeRevision &&
                                          reply.at("result").at("read_only").as_bool(),
                                      "Native static-tree GUI inspection changed Runtime counters");
                      smoke["static_tree_direct_readonly_dispatch"] = true;
                      smoke["source_unchanged"] = reply.at("result").at("source_unchanged");
                      smoke["source_sha256"] = reply.at("result").at("source_sha256");
                      smoke["static_tree_before_generation"] = beforeGeneration;
                      smoke["static_tree_before_revision"] = beforeRevision;
                      smoke["static_tree_after_generation"] = reply.at("generation");
                      smoke["static_tree_after_revision"] = reply.at("revision");
                  }
                  smoke["operation_result"] = reply.at("result");
                  smoke["native_operation_completed"] = true;
                  smokeStage = 14;
                  SmokeComplete();
              }
          });
}

void Monstey_App::FormCommand(int command)
{
    if (tab == 4 && (command == Monstey_Add || command == Monstey_Extract || command == Monstey_Replace))
    {
        if (command == Monstey_Replace)
            OpenModelBlender();
        else
            ModelWorkflow(command == Monstey_Add ? "model_import_blend" : "model_export_blend");
        return;
    }
    if (command == Monstey_Search)
    {
        LoadOperations();
        return;
    }
    if (command == Monstey_Build)
    {
        RunOperation();
        return;
    }
    if (command == Monstey_Open)
    {
        if (selectedOperation.empty())
            return;
        const auto &props = selectedOperation.at("inputSchema").at("properties");
        std::string key = props.contains("source")                           ? "source"
                          : props.contains("input")                          ? "input"
                          : props.contains("game_root")                      ? "game_root"
                          : Monstey_DatabaseForm() && props.contains("path") ? "path"
                                                                             : "";
        if (key.empty())
        {
            SetStatus(L"Select an operation with a source or input argument.");
            return;
        }
        const auto path =
            key == "game_root"
                ? Monstey_DirectoryDialog(window, L"Choose game folder containing x64/default.archcfg")
                : Monstey_FileDialog(window, false,
                                     tab == 4 ? Monstey_ModelFileFilter(selectedOperation.at("name").as_string(), key)
                                              : L"All files\0*.*\0\0",
                                     nullptr, L"Choose native input");
        if (path)
        {
            operationArguments[key] = Monstey_JsonPath(*path);
            if (Monstey_DatabaseForm() && key == "path")
            {
                operationArguments["expected_sha256"] = Monstey_HashSha256(*path);
                databaseNativeSource["path"] = operationArguments.at("path");
                databaseNativeSource["expected_sha256"] = operationArguments.at("expected_sha256");
            }
            if (tab == 4 && selectedOperation.at("name").as_string() == "model_export_blend")
            {
                modelBlendPreview = *path;
                UpdateButtons();
            }
            PopulateOperationFields();
        }
        return;
    }
    if (command == Monstey_Delete)
    {
        const auto index = ListView_GetNextItem(fields, -1, LVNI_SELECTED);
        if (index >= 0 && static_cast<std::size_t>(index) < fieldData.size())
        {
            operationArguments.as_object().erase(fieldData[static_cast<std::size_t>(index)].at("name").as_string());
            PopulateOperationFields();
        }
        return;
    }
    if (command == Monstey_Revert)
    {
        auto item = std::make_shared<Monstey_Editor>();
        item->app = this;
        item->title = L"Advanced operation arguments";
        item->field = J::object(
            {{"name", "Arguments JSON"}, {"native_type", 99}, {"array", false}, {"value", operationArguments}});
        item->success = [this](const J &value)
        {
            Monstey_Require(value.is_object(), "Arguments must be a JSON object");
            operationArguments = value;
            PopulateOperationFields();
        };
        ShowEditor(item);
        return;
    }
    if (command == Monstey_Save)
    {
        if (selectedOperation.empty())
            return;
        const auto path = Monstey_FileDialog(window, true, L"Operation preset (*.json)\0*.json\0\0", L"json",
                                             L"Save NEW native operation preset");
        if (path)
        {
            const auto preset = J::object({{"format", "MONSTEY_NATIVE_OPERATION_PRESET"},
                                           {"version", 1},
                                           {"operation", selectedOperation.at("name")},
                                           {"domain", Monstey_DatabaseForm() ? "database"
                                                      : tab == 5             ? "world"
                                                      : tab == 6             ? "settings"
                                                                             : "models"},
                                           {"arguments", operationArguments}});
            Monstey_WriteNewText(*path, preset.dump(2) + "\n");
            SetStatus(L"Native operation arguments saved.");
        }
        return;
    }
    if (command == Monstey_Load)
    {
        const auto path = Monstey_FileDialog(window, false, L"Operation preset (*.json)\0*.json\0\0", L"json",
                                             L"Load native operation preset");
        if (!path)
            return;
        const auto preset = J::Parse(Monstey_ReadText(*path, 1024 * 1024));
        Monstey_Require(preset.at("format").as_string() == "MONSTEY_NATIVE_OPERATION_PRESET" &&
                            preset.at("arguments").is_object(),
                        "Invalid native operation preset");
        const auto name = preset.at("operation").as_string();
        const auto found = std::find_if(rowData.begin(), rowData.end(),
                                        [&](const auto &op) { return op.at("name").as_string() == name; });
        Monstey_Require(found != rowData.end(), "Preset operation is not in this domain catalogue");
        operationPresets[name] = preset.at("arguments");
        const auto index = static_cast<int>(found - rowData.begin());
        Monstey_ListSelect(records, index);
        SelectOperation(index);
        return;
    }
}

} // namespace Monstey::Monstey_AppInternal
