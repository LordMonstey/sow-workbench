#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

J Monstey_StructureText(const char *name, const char *label, const char *value, bool optional, bool allowEmpty)
{
    return J::object({{"name", name},
                      {"label", label},
                      {"kind", "text"},
                      {"value", value},
                      {"optional", optional},
                      {"allow_empty", allowEmpty}});
}

J Monstey_StructureChoice(const char *name, const char *label, const J &choices, const J &value)
{
    return J::object({{"name", name}, {"label", label}, {"kind", "choice"}, {"choices", choices}, {"value", value}});
}

J Monstey_StructureDefault(unsigned type)
{
    if (type == 1)
        return false;
    if (type == 2 || type == 3)
        return 0;
    if (type == 4 || type == 5 || type == 6 || type == 68)
        return "";
    if (type >= 7 && type <= 9)
    {
        J values = J::array();
        for (unsigned i = 0; i < type - 5; ++i)
            values.as_array().push_back(0);
        return values;
    }
    if (type == 10)
        return J{};
    if (type == 11)
        return J::array();
    if (type == 12 || type == 13 || type == 15)
        return J::object({{"constant", 0}});
    if (type == 16)
        return J::object({{"constant", J::array({0, 0, 0})}});
    throw Monstey_Error("native_type", "Choose a supported native field type");
}

J Monstey_StructureTypes()
{
    J rows = J::array();
    for (auto [type, label] :
         {std::pair{1, "Boolean"}, std::pair{2, "Float"}, std::pair{3, "Integer"}, std::pair{4, "String"},
          std::pair{5, "Enum"}, std::pair{6, "Unicode text"}, std::pair{7, "Vector2"}, std::pair{8, "Vector3"},
          std::pair{9, "Vector4"}, std::pair{10, "Record reference"}, std::pair{11, "Reference list"},
          std::pair{12, "ARGB curve"}, std::pair{13, "Float curve"}, std::pair{15, "Integer curve"},
          std::pair{16, "Vector3 curve"}, std::pair{68, "String (native 68)"}})
        rows.as_array().push_back(J::object({{"label", label}, {"value", type}}));
    return rows;
}

void Monstey_App::StructureForm(const std::wstring &title, const J &rows, std::function<void(const J &)> apply,
                                std::string operation, J arguments)
{
    auto item = std::make_shared<Monstey_Editor>();
    item->app = this;
    item->title = title;
    item->field =
        J::object({{"name", Monstey_Utf8(title)}, {"native_type", 0}, {"value", J::object()}, {"editable", true}});
    item->formFields = rows;
    item->operation = std::move(operation);
    item->arguments = std::move(arguments);
    item->generation = item->arguments.value("expected_generation", generation).as_uint();
    item->revision = item->arguments.value("expected_revision", revision).as_uint();
    item->success = std::move(apply);
    ShowEditor(item);
}

void Monstey_App::RefreshProperties(const std::string &record, const std::string &field)
{
    Queue("database_metadata", J::object({{"record", record}}),
          [this, record, field](const J &reply)
          {
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              auto snapshot = reply.at("result");
              snapshot["generation"] = reply.at("generation");
              snapshot["revision"] = reply.at("revision");
              snapshot["view_tab"] = 0;
              auto select = [&](HWND list, const J &model)
              {
                  if (field.empty())
                      return;
                  auto &rows = model.at("fields").as_array();
                  for (std::size_t i = 0; i < rows.size(); ++i)
                      if (rows[i].at("hash").as_string() == field)
                      {
                          Monstey_ListSelect(list, static_cast<int>(i));
                          break;
                      }
              };
              if (tab == 0 && selector == record)
              {
                  PopulateFields(reply);
                  select(fields, snapshot);
              }
              if (inspector && inspector->window && inspector->metadata.value("view_tab", 0).as_int() == 0 &&
                  inspector->metadata.at("selector").as_string() == record)
              {
                  inspector->metadata = snapshot;
                  inspector->Populate();
                  select(inspector->list, snapshot);
              }
              if (smokeStructure)
                  SmokeStep();
          });
}

void Monstey_App::StructureCommand(int command, const J *snapshot, int fieldIndex)
{
    if (Monstey_DatabaseForm())
    {
        SetStatus(L"Return to records to edit the database.");
        return;
    }
    if (tab == 3 && fxLoading)
    {
        SetStatus(L"Wait for the native effect refresh to finish.");
        return;
    }
    if (tab == 3 && fxNestedMode && snapshot == nullptr)
    {
        SetStatus(L"Nested FX > Back to properties enables native property "
                  L"structure edits.");
        return;
    }
    if (tab != 0 && tab != 3 && snapshot == nullptr)
        return;
    if (editor && editor->window)
    {
        SetForegroundWindow(editor->window);
        return;
    }
    if (command == Monstey_Structure)
    {
        auto menu = CreatePopupMenu();
        for (auto [id, label] :
             {std::pair{Monstey_CreateCategory, L"New category..."}, std::pair{Monstey_CreateRecord, L"New record..."},
              std::pair{Monstey_AddField, L"Add property..."},
              std::pair{Monstey_DeleteField, L"Delete selected property"},
              std::pair{Monstey_MoveFieldUp, L"Move property up"},
              std::pair{Monstey_MoveFieldDown, L"Move property down"}})
            AppendMenuW(menu, MF_STRING, id, label);
        RECT rect{};
        GetWindowRect(GetDlgItem(window, Monstey_Structure), &rect);
        auto chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN, rect.left, rect.bottom, 0, window, nullptr);
        DestroyMenu(menu);
        if (chosen)
            StructureCommand(static_cast<int>(chosen));
        return;
    }
    if ((command == Monstey_CreateCategory || command == Monstey_CreateRecord) && tab != 0)
        return;
    if (command == Monstey_CreateCategory)
    {
        Monstey_Require(!sourceNames[0].empty(), "Create or open a database first");
        StructureForm(
            L"New native category",
            J::array({Monstey_StructureText("schema_name", "Category path", "Monstey/NewCategory"),
                      Monstey_StructureText("name", "Schema name", "Monstey.NewCategory", true),
                      Monstey_StructureText("path", "Source path (optional)", "", true)}),
            [this](const J &result)
            {
                UpdateState(result);
                category = static_cast<std::uint32_t>(result.at("category").at("index").as_uint());
                SetWindowTextW(query, L"");
                categoryRows.clear();
                LoadCategories();
                ShowReport(result);
                if (smokeStructure)
                {
                    smoke["category_dialog_applied"] = true;
                    smokeStage = 3;
                }
            },
            "database_create_category");
        return;
    }
    if (command == Monstey_CreateRecord)
    {
        Monstey_Require(category.has_value(), "Select a category for the new record");
        auto chosen = *category;
        StructureForm(
            L"New native record",
            J::array({Monstey_StructureText("name", "Record name", "Monstey_NewRecord", true),
                      J::object(
                          {{"name", "anonymous"}, {"label", "Anonymous record"}, {"kind", "bool"}, {"value", false}})}),
            [this, chosen](const J &result)
            {
                UpdateState(result);
                category = chosen;
                offset = 0;
                auto &created = result.at("record");
                selector = created.at("selector").as_string();
                metadata = J::object();
                fieldData.clear();
                ListView_DeleteAllItems(fields);
                if (created.contains("name") && created.at("name").is_string() &&
                    !created.at("name").as_string().empty())
                    SetWindowTextW(query, Monstey_Display(created.at("name")).c_str());
                else
                    SetWindowTextW(query, Monstey_Wide(selector).c_str());
                LoadRecords();
                ShowReport(result);
                if (smokeStructure)
                {
                    if (smokeStage == 34)
                    {
                        smoke["anonymous_record_dialog_applied"] = true;
                        smoke["anonymous_selector"] = selector;
                        smokeStage = 35;
                    }
                    else
                    {
                        smoke["record_dialog_applied"] = true;
                        smokeStage = 5;
                    }
                }
            },
            "database_create_record", J::object({{"category_index", chosen}}));
        return;
    }
    auto model = snapshot ? *snapshot : metadata;
    Monstey_Require(model.is_object() && model.contains("selector") && model.contains("fields") &&
                        (model.value("view_tab", 0).as_int() == 0 || model.value("view_tab", 0).as_int() == 3),
                    "Select a database record first");
    auto record = model.at("selector").as_string();
    const auto origin = static_cast<int>(model.value("view_tab", 0).as_int());
    if (command == Monstey_AddField)
    {
        J multiplicity = J::array({J::object({{"label", "Single value"}, {"value", 0}}),
                                   J::object({{"label", "Variable array"}, {"value", -1}}),
                                   J::object({{"label", "Fixed array"}, {"value", 1}})});
        StructureForm(
            L"Add native property",
            J::array(
                {Monstey_StructureText("field", "Property name", "Monstey_NewProperty"),
                 Monstey_StructureChoice("native_type", "Native type", Monstey_StructureTypes(), 2),
                 Monstey_StructureChoice("multiplicity", "Values", multiplicity, 0),
                 J::object(
                     {{"name", "count"}, {"label", "Array size / starting values"}, {"kind", "integer"}, {"value", 1}}),
                 Monstey_StructureText("target", "Reference category (optional)", "", true)}),
            [this, model, record, origin](const J &value)
            {
                auto type = static_cast<unsigned>(value.at("native_type").as_uint());
                auto mode = value.at("multiplicity").as_int();
                auto count = value.at("count").as_uint();
                Monstey_Require(count > 0 && count <= 65535, "Choose 1 to 65,535 initial array values");
                auto scalar = Monstey_StructureDefault(type);
                auto array = mode < 0 || (mode > 0 && count > 1) || type == 11;
                J initial = scalar, extra = J::object();
                if (type == 11)
                {
                    initial = J::array();
                    extra["array"] = -1;
                }
                else if (array)
                {
                    initial = J::array();
                    for (std::uint64_t i = 0; i < count; ++i)
                        initial.as_array().push_back(scalar);
                    extra["array"] = mode < 0 ? J(-1) : J(count);
                }
                else
                    extra["array"] = 1;
                if (value.contains("target") && !value.at("target").as_string().empty())
                    extra["target"] = value.at("target");
                auto item = std::make_shared<Monstey_Editor>();
                item->app = this;
                item->title = L"Initial value: " + Monstey_Display(value.at("field"));
                item->field = J::object({{"name", value.at("field")},
                                         {"native_type", type},
                                         {"array", array},
                                         {"value", initial},
                                         {"editable", true}});
                item->lineList = array && type != 12 && type != 13 && type != 15 && type != 16;
                item->operation = origin == 0 ? "database_add_field" : "fx_add_field";
                item->arguments = J::object(
                    {{"record", record}, {"field", value.at("field")}, {"native_type", type}, {"metadata", extra}});
                item->generation = model.at("generation").as_uint();
                item->revision = model.at("revision").as_uint();
                auto field = Monstey_Hex(Monstey_NativeHash(value.at("field").as_string()));
                item->success = [this, record, field, origin](const J &result)
                {
                    UpdateState(result, origin);
                    ShowReport(result);
                    if (origin == 0)
                        RefreshProperties(record, field);
                    else
                    {
                        selector = record;
                        LoadFx();
                    }
                    if (smokeStructure)
                    {
                        auto added = result.at("metadata").at("fields").size();
                        smoke["properties_added"] = added;
                        smokeStage = smokeStage == 31 ? 32 : added == 1 ? 8 : 11;
                    }
                };
                ShowEditor(item);
            });
        if (editor && editor->window)
            editor->validate = [model](const J &value)
            {
                auto count = value.at("count").as_uint();
                Monstey_Require(count >= 1 && count <= 65535, "Choose 1 to 65,535 initial array values");
                auto type = value.at("native_type").as_uint();
                auto mode = value.at("multiplicity").as_int();
                Monstey_Require(type != 1 || mode == 0 || (mode > 0 && count == 1),
                                "Native Boolean properties contain one value; choose Single value");
                auto hash = Monstey_Hex(Monstey_NativeHash(value.at("field").as_string()));
                for (auto &field : model.at("fields").as_array())
                    Monstey_Require(field.at("hash").as_string() != hash,
                                    "This property already exists; choose a new name");
            };
        return;
    }
    auto &rows = model.at("fields").as_array();
    if (fieldIndex < 0 && snapshot == nullptr)
        fieldIndex = ListView_GetNextItem(fields, -1, LVNI_SELECTED);
    Monstey_Require(fieldIndex >= 0 && static_cast<std::size_t>(fieldIndex) < rows.size(), "Select a property first");
    auto hash = rows[static_cast<std::size_t>(fieldIndex)].at("hash").as_string();
    auto args = J::object({{"record", record},
                           {"field", hash},
                           {"expected_generation", model.at("generation")},
                           {"expected_revision", model.at("revision")}});
    auto operation = origin == 0 ? "database_delete_field" : "fx_delete_field";
    if (command == Monstey_MoveFieldUp || command == Monstey_MoveFieldDown)
    {
        auto ordinal = fieldIndex + (command == Monstey_MoveFieldUp ? -1 : 1);
        if (ordinal < 0 || static_cast<std::size_t>(ordinal) >= rows.size())
            return;
        args["ordinal"] = ordinal;
        operation = origin == 0 ? "database_move_field" : "fx_move_field";
    }
    else
        Monstey_Require(command == Monstey_DeleteField, "Unknown structural database action");
    Queue(operation, args,
          [this, record, hash, command, origin](const J &reply)
          {
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              UpdateState(reply.at("result"), origin);
              ShowReport(reply.at("result"));
              if (origin == 0)
                  RefreshProperties(record, command == Monstey_DeleteField ? std::string{} : hash);
              else
              {
                  selector = record;
                  LoadFx();
              }
              if (smokeStructure)
              {
                  smoke[command == Monstey_DeleteField ? "property_deleted" : "property_moved"] = true;
                  smokeStage = command == Monstey_DeleteField ? 14 : 12;
              }
          });
}

} // namespace Monstey::Monstey_AppInternal
