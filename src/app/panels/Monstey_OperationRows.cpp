#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::LoadOperations()
{
    const int requestedTab = tab;
    Queue(tab == 5 ? "world_catalog" : "catalog", J::object(),
          [this, requestedTab](const J &reply)
          {
              if (tab != requestedTab)
                  return;
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              const auto filter = Monstey_Utf8(Monstey_WindowText(query));
              const auto folded = [](std::string value)
              {
                  for (auto &c : value)
                      if (c >= 'A' && c <= 'Z')
                          c = static_cast<char>(c + 32);
                  return value;
              };
              rowData.clear();
              for (const auto &operation : reply.at("result").as_array())
              {
                  const auto name = operation.at("name").as_string();
                  if (Monstey_DatabaseForm() && name != "database_native_banks" && name != "database_native_class")
                      continue;
                  if (tab == 4 && (name == "model_import_native" || name == "model_export_native"))
                      continue;
                  if (tab == 4 && !name.starts_with("model_"))
                      continue;
                  if (tab == 6 && name != "configure" && name != "components" && name != "archive_mounts" &&
                      name != "archive_catalog" && name != "archive_catalog_read" &&
                      name != "archive_extract_embedded" && name != "archive_read_embedded_range")
                      continue;
                  if (!filter.empty() &&
                      folded(name + " " + operation.at("description").as_string()).find(folded(filter)) ==
                          std::string::npos)
                      continue;
                  rowData.push_back(operation);
              }
              populating = true;
              ListView_DeleteAllItems(records);
              ListView_DeleteAllItems(fields);
              fieldData.clear();
              for (std::size_t i = 0; i < rowData.size(); ++i)
              {
                  const auto &op = rowData[i];
                  Monstey_ListCell(records, static_cast<int>(i), 0,
                                   tab == 4 ? Monstey_ModelOperationLabel(op.at("name").as_string())
                                            : Monstey_Display(op.at("name")));
                  Monstey_ListCell(
                      records, static_cast<int>(i), 1,
                      Monstey_Display(
                          op.value("status", op.value("implemented", true).as_bool() ? J("native") : J("pending"))));
                  Monstey_ListCell(records, static_cast<int>(i), 2, Monstey_Display(op.at("description")));
              }
              populating = false;
              total = rowData.size();
              SetWindowTextW(
                  pageLabel,
                  (std::to_wstring(total) + L" native operations. Double-click an argument to edit it.").c_str());
              EnableWindow(GetDlgItem(window, Monstey_Previous), FALSE);
              EnableWindow(GetDlgItem(window, Monstey_Next), FALSE);
              if (!rowData.empty())
              {
                  int chosen = 0;
                  if (!smokeOperation.empty())
                  {
                      const auto found = std::find_if(rowData.begin(), rowData.end(), [this](const auto &row)
                                                      { return row.at("name").as_string() == smokeOperation; });
                      if (found == rowData.end())
                      {
                          Error("GUI smoke operation not found in native catalogue");
                          return;
                      }
                      chosen = static_cast<int>(found - rowData.begin());
                      operationPresets[smokeOperation] = smokeArguments;
                  }
                  if (smokeOperation.empty() && !preferredOperation.empty())
                  {
                      const auto requested = std::find_if(rowData.begin(), rowData.end(), [this](const auto &row)
                                                          { return row.at("name").as_string() == preferredOperation; });
                      if (requested != rowData.end())
                          chosen = static_cast<int>(requested - rowData.begin());
                      preferredOperation.clear();
                  }
                  Monstey_ListSelect(records, chosen);
                  SelectOperation(chosen);
                  if (!smokeOperation.empty())
                      SmokeStep();
              }
              else
              {
                  selectedOperation = J::object();
                  EnableWindow(GetDlgItem(window, Monstey_Build), FALSE);
                  ShowReport(J::object({{"status", "No native operations for this domain in the current build"},
                                        {"python_fallback", false}}));
              }
          });
}

void Monstey_App::SelectOperation(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= rowData.size())
        return;
    if (Monstey_DatabaseForm() && !selectedOperation.empty() &&
        (selectedOperation.at("name") == J("database_native_banks") ||
         selectedOperation.at("name") == J("database_native_class")))
        for (const auto key : {"path", "expected_sha256"})
            if (operationArguments.contains(key))
                databaseNativeSource[key] = operationArguments.at(key);
    if (!selectedOperation.empty())
        operationPresets[selectedOperation.at("name").as_string()] = operationArguments;
    if (!smokeReport.empty() && smokeDomain == 0)
    {
        smoke["native_selection_index"] = index;
        smoke["native_selection_row"] = rowData[static_cast<std::size_t>(index)];
    }
    selectedOperation = rowData[static_cast<std::size_t>(index)];
    const auto name = selectedOperation.at("name").as_string();
    operationArguments = operationPresets.contains(name) ? operationPresets.at(name) : J::object();
    if (Monstey_DatabaseForm())
        for (const auto key : {"path", "expected_sha256"})
            if (databaseNativeSource.contains(key))
                operationArguments[key] = databaseNativeSource.at(key);
    const auto &schemaObject = selectedOperation.at("inputSchema");
    if (schemaObject.contains("properties"))
        for (const auto &[key, rule] : schemaObject.at("properties").as_object())
            if (!operationArguments.contains(key) && rule.contains("default"))
                operationArguments[key] = rule.at("default");
    if (!Monstey_DatabaseForm())
        sourceNames[tab] = Monstey_Wide(name);
    UpdateState(J::object());
    EnableWindow(GetDlgItem(window, Monstey_Build),
                 selectedOperation.value("available", selectedOperation.value("implemented", true)).as_bool());
    PopulateOperationFields();
    ShowReport(J::object({{"operation", name},
                          {"description", selectedOperation.at("description")},
                          {"status", selectedOperation.value("status", "native")},
                          {"arguments", operationArguments},
                          {"instructions", "Double-click an argument, choose Source, or use Advanced "
                                           "arguments. Run uses the shared native API."}}));
}

void Monstey_App::PopulateOperationFields()
{
    fieldData.clear();
    populating = true;
    ListView_DeleteAllItems(fields);
    if (selectedOperation.empty())
    {
        populating = false;
        return;
    }
    const auto &object = selectedOperation.at("inputSchema");
    if (!object.contains("properties"))
    {
        populating = false;
        return;
    }
    for (const auto &[name, rule] : object.at("properties").as_object())
    {
        if (name == "expected_generation" || name == "expected_revision")
            continue;
        std::string type = "object";
        if (rule.contains("type"))
        {
            const auto &t = rule.at("type");
            if (t.is_string())
                type = t.as_string();
            else
                for (const auto &value : t.as_array())
                    if (value.as_string() != "null")
                    {
                        type = value.as_string();
                        break;
                    }
        }
        const bool nestedBand =
            name == "triangle_band" && !operationArguments.contains(name) && operationArguments.contains("options") &&
            operationArguments.at("options").is_object() && operationArguments.at("options").contains(name);
        const bool present = operationArguments.contains(name) || nestedBand;
        J value = nestedBand          ? operationArguments.at("options").at(name)
                  : present           ? operationArguments.at(name)
                  : type == "string"  ? J("")
                  : type == "integer" ? J(0)
                  : type == "number"  ? J(0.0)
                  : type == "boolean" ? J(false)
                  : type == "array"   ? J::array()
                                      : J::object();
        if (Monstey_DatabaseForm() && name == "class_pointer" && value.is_integer() && value.as_int() >= 0)
            value = Monstey_Hex(value.as_uint());
        int nativeType = type == "string"    ? 4
                         : type == "integer" ? 3
                         : type == "number"  ? 2
                         : type == "boolean" ? 1
                                             : 99;
        bool array = type == "array";
        J enumValues = J::array();
        if (rule.contains("enum") && std::all_of(rule.at("enum").as_array().begin(), rule.at("enum").as_array().end(),
                                                 [](const J &item) { return item.is_string(); }))
        {
            enumValues = rule.at("enum");
            nativeType = 5;
            if (!present && !enumValues.empty())
                value = enumValues.at(0);
        }
        if (type == "array" && rule.contains("minItems") && rule.contains("maxItems") &&
            rule.at("minItems") == rule.at("maxItems") && rule.at("minItems").as_int() >= 2 &&
            rule.at("maxItems").as_int() <= 4 && rule.contains("items") && rule.at("items").contains("type") &&
            rule.at("items").at("type").as_string() == "number")
        {
            nativeType = static_cast<int>(rule.at("minItems").as_int()) + 5;
            array = false;
            if (!present)
            {
                value = J::array();
                for (int i = 0; i < nativeType - 5; ++i)
                    value.as_array().push_back(0.0);
            }
        }
        bool required = false;
        if (object.contains("required"))
            for (const auto &item : object.at("required").as_array())
                if (item.as_string() == name)
                    required = true;
        J field = J::object({{"name", name},
                             {"native_type", nativeType},
                             {"array", array},
                             {"editable", true},
                             {"value", value},
                             {"type_name", type},
                             {"argument_schema", rule},
                             {"present", present},
                             {"required", required},
                             {"enum_values", enumValues}});
        if (rule.contains("minimum"))
            field["min"] = rule.at("minimum");
        if (rule.contains("maximum"))
            field["max"] = rule.at("maximum");
        const auto index = static_cast<int>(fieldData.size());
        fieldData.push_back(field);
        Monstey_ListCell(fields, index, 0, Monstey_Wide(name) + (required ? L" *" : L""));
        Monstey_ListCell(fields, index, 1, Monstey_Wide(type));
        Monstey_ListCell(fields, index, 2, present ? Monstey_Display(value) : L"(not set)");
    }
    populating = false;
}

} // namespace Monstey::Monstey_AppInternal
