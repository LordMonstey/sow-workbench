#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

namespace
{
std::wstring Monstey_MaterialValueLabel(const J &row)
{
    if (row.contains("serialized_string"))
        return Monstey_Display(row.at("serialized_string"));
    if (row.contains("value_f32"))
        return Monstey_Display(row.at("value_f32"));
    return row.contains("value_words_u32") ? Monstey_Display(row.at("value_words_u32")) : L"Raw bytes";
}
bool Monstey_MaterialContains(const std::wstring &text, const std::wstring &query)
{
    if (query.empty())
        return true;
    if (query.size() > text.size())
        return false;
    for (std::size_t i = 0; i <= text.size() - query.size(); ++i)
        if (CompareStringOrdinal(text.data() + i, static_cast<int>(query.size()), query.data(),
                                 static_cast<int>(query.size()), TRUE) == CSTR_EQUAL)
            return true;
    return false;
}
} // namespace

void Monstey_App::MaterialAccept(const J &result)
{
    materialLastReport = result;
    if (result.contains("material"))
    {
        materialView = result;
        materialView["ui_generation"] = generation;
        materialView["ui_revision"] = revision;
        materialSource = Monstey_Wide(result.at("source").as_string());
        if (tab == 4 && materialMode)
            UpdateState(result, 4);
    }
    if (!materialView.empty())
    {
        materialView["ui_generation"] = generation;
        materialView["ui_revision"] = revision;
    }
    if (tab == 4 && materialMode)
    {
        MaterialPopulate();
        UpdateButtons();
    }
}

void Monstey_App::MaterialSubmit(std::string operation, J args)
{
    if (!args.contains("expected_generation"))
        args["expected_generation"] = generation;
    if (!args.contains("expected_revision"))
        args["expected_revision"] = revision;
    Queue(std::move(operation), std::move(args),
          [this](const J &reply)
          {
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              MaterialAccept(reply.at("result"));
          });
}

void Monstey_App::MaterialLoadRows()
{
    if (tab != 4 || !materialMode || materialView.empty() || materialOpening)
        return;
    const auto request = ++materialRowsRequest;
    Queue("material_describe", J::object(),
          [this, request](const J &reply)
          {
              if (tab != 4 || !materialMode || request != materialRowsRequest)
                  return;
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              MaterialAccept(reply.at("result"));
          });
}

void Monstey_App::MaterialPopulate()
{
    if (tab != 4 || !materialMode)
        return;
    rowData.clear();
    fieldData.clear();
    populating = true;
    ListView_DeleteAllItems(records);
    ListView_DeleteAllItems(fields);
    const auto queryText = Monstey_WindowText(query);
    int chosen = -1;
    if (!materialView.empty())
    {
        for (const auto &row : materialView.at("material").at("entries").as_array())
        {
            if (!Monstey_MaterialContains(Monstey_Display(row.at("name")), queryText))
                continue;
            const auto i = static_cast<int>(rowData.size());
            rowData.push_back(row);
            const auto key = row.at("table").as_string() + ":" + std::to_string(row.at("ordinal").as_uint());
            Monstey_ListCell(records, i, 0, Monstey_Display(row.at("name")));
            Monstey_ListCell(records, i, 1, Monstey_Display(row.at("type_u16")));
            Monstey_ListCell(records, i, 2, Monstey_MaterialValueLabel(row));
            if (key == materialSelected)
                chosen = i;
        }
    }
    total = rowData.size();
    offset = 0;
    if (chosen < 0 && !rowData.empty())
        chosen = 0;
    if (chosen >= 0)
        Monstey_ListSelect(records, chosen);
    else
        materialSelected.clear();
    populating = false;
    EnableWindow(GetDlgItem(window, Monstey_Previous), FALSE);
    EnableWindow(GetDlgItem(window, Monstey_Next), FALSE);
    SetWindowTextW(pageLabel, (std::to_wstring(total) +
                               L" material properties. Double-click to edit. Native rendering is not previewed.")
                                  .c_str());
    if (chosen >= 0)
        MaterialReadRow(chosen);
    else
        ShowReport(J::object({{"material", materialView}, {"last_action", materialLastReport}}));
}

void Monstey_App::MaterialReadRow(int index, bool edit)
{
    if (index < 0 || static_cast<std::size_t>(index) >= rowData.size())
        return;
    const auto row = rowData[static_cast<std::size_t>(index)];
    materialSelected = row.at("table").as_string() + ":" + std::to_string(row.at("ordinal").as_uint());
    fieldData.clear();
    populating = true;
    ListView_DeleteAllItems(fields);
    fieldData.push_back(J::object({{"name", "value"}, {"value", Monstey_Utf8(Monstey_MaterialValueLabel(row))}}));
    for (const auto *key :
         {"table", "ordinal", "type_u16", "native_type_u8", "flag_a", "flag_b", "native_name_type_hash",
          "stored_offset_u32", "runtime_offset_u16", "native_copy_bytes", "serialized_value_bytes", "raw_name_hex",
          "value_bytes_hex", "authored_value_qualified", "raw_storage_editable", "legacy_word"})
        if (row.contains(key))
            fieldData.push_back(J::object({{"name", key}, {"value", row.at(key)}}));
    for (std::size_t i = 0; i < fieldData.size(); ++i)
    {
        Monstey_ListCell(fields, static_cast<int>(i), 0, Monstey_Display(fieldData[i].at("name")));
        Monstey_ListCell(fields, static_cast<int>(i), 1, Monstey_Display(fieldData[i].at("value")));
        Monstey_ListCell(fields, static_cast<int>(i), 2, i == 0 ? L"Edit value" : L"Native metadata");
    }
    populating = false;
    ShowReport(J::object({{"session", materialView}, {"selected_property", row}, {"last_action", materialLastReport}}));
    UpdateButtons();
    if (edit && row.at("raw_storage_editable").as_bool())
        MaterialValue(J::object({{"op", "set"}, {"table", row.at("table")}, {"index", row.at("ordinal")}}), row, false);
}

void Monstey_App::MaterialValue(J draft, const J &row, bool raw)
{
    Monstey_Require(!materialView.empty() && !materialOpening, "Open a material first");
    Monstey_Require(!row.contains("raw_storage_editable") || row.at("raw_storage_editable").as_bool(),
                    "This native storage profile is read-only");
    const auto low = static_cast<unsigned>(row.at("type_u16").as_uint() & 255U);
    J value;
    unsigned editorType = 0;
    bool array = false, plain = false;
    if (!raw && row.contains("value_f32"))
    {
        value = row.at("value_f32");
        for (const auto &element : value.as_array())
            if (!element.is_number())
                raw = true;
        if (!raw)
        {
            if (low == 5)
            {
                J scalar(value[0]);
                value = std::move(scalar);
            }
            else if (value.size() >= 2 && value.size() <= 4)
                editorType = static_cast<unsigned>(value.size() + 5);
            else
                array = true;
        }
    }
    else if (!raw && (low == 1 || low == 8) && row.contains("serialized_string"))
    {
        value = row.at("serialized_string");
        editorType = 4;
        plain = true;
    }
    else if (!raw && low == 4 && row.contains("value_words_u32"))
        value = row.at("value_words_u32")[0];
    else if (!raw && low == 6 && row.contains("value_words_u32"))
    {
        editorType = 1;
        value = row.at("value_words_u32")[0].as_uint() != 0;
    }
    else if (!raw)
        raw = true;
    if (raw)
    {
        value = row.value("value_bytes_hex", "");
        editorType = 4;
        plain = true;
        array = false;
    }
    auto item = std::make_shared<Monstey_Editor>();
    item->app = this;
    item->title = (raw ? L"Material raw value bytes: " : L"Material value: ") + Monstey_Display(row.at("name"));
    item->plainText = plain;
    item->field = J::object({{"name", row.at("name")},
                             {"native_type", editorType},
                             {"array", array},
                             {"value", value},
                             {"editable", true}});
    const auto capturedGeneration = materialView.at("ui_generation");
    const auto capturedRevision = materialView.at("ui_revision");
    item->validate = [raw, low](const J &next)
    {
        if (raw)
        {
            const auto text = next.as_string();
            Monstey_Require(text.size() % 2 == 0, "Hex bytes need pairs of digits");
            Monstey_Require(
                std::all_of(text.begin(), text.end(), [](char c)
                            { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }),
                "Invalid hexadecimal byte");
        }
        else if (low == 4)
        {
            Monstey_Require(next.is_integer() && next.as_number() >= 0 && next.as_number() <= UINT32_MAX,
                            "Enter an unsigned 32-bit word");
        }
        else if (low == 1 || low == 8)
        {
            Monstey_Require(next.as_string().find('\0') == std::string::npos,
                            "Use raw bytes to retain embedded NUL bytes");
        }
        else if (low != 6)
        {
            const auto check = [](const J &n)
            {
                Monstey_Require(n.is_number() && std::isfinite(n.as_number()) &&
                                    std::abs(n.as_number()) <= std::numeric_limits<float>::max(),
                                "Enter a finite F32 value; use raw bytes for non-finite words");
            };
            if (next.is_array())
                for (const auto &n : next.as_array())
                    check(n);
            else
                check(next);
        }
    };
    item->operation = "material_edit";
    item->arguments = J::object({{"operations", J::array({draft})}});
    item->valueKey = raw ? "bytes_hex" : "value";
    item->generation = capturedGeneration.as_uint();
    item->revision = capturedRevision.as_uint();
    item->success = [this](const J &result) { MaterialAccept(result); };
    ShowEditor(item);
}

void Monstey_App::MaterialCommand(int command)
{
    if (command == Monstey_Materials)
    {
        materialMode = !materialMode;
        if (materialMode)
        {
            animationMode = false;
            socketMode = false;
            animatedExtraMode = false;
        }
        TabChanged();
        return;
    }
    if (tab != 4 || !materialMode)
        return;
    if (command != Monstey_Open && command != Monstey_Load && command != Monstey_Save && command != Monstey_Build &&
        command != Monstey_Undo && command != Monstey_Redo && command != Monstey_Revert &&
        command != Monstey_MaterialChanges && command != Monstey_Add && command != Monstey_Rename &&
        command != Monstey_Delete && command != Monstey_MaterialEdit && command != Monstey_MaterialRaw &&
        command != Monstey_Search && command != Monstey_Previous && command != Monstey_Next)
        return;
    if (command == Monstey_Search)
    {
        MaterialPopulate();
        return;
    }
    if (command == Monstey_Previous || command == Monstey_Next)
        return;
    if (materialOpening)
    {
        SetStatus(L"Wait for the material source to finish loading.");
        return;
    }
    if (command == Monstey_Open || command == Monstey_Load)
    {
        auto chosen =
            Monstey_FileDialog(window, false,
                               command == Monstey_Open ? L"Native material (*.mat)\0*.mat\0All files\0*.*\0\0"
                                                       : L"Material project (*.json)\0*.json\0\0",
                               command == Monstey_Open ? L"mat" : L"json", L"Open native material or project");
        if (!chosen)
            return;
        if (!materialView.empty() && materialView.at("dirty").as_bool() && smokeReport.empty() &&
            MessageBoxW(window, L"Discard the current material edits?", L"Open material",
                        MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
            return;
        ++epoch;
        ++materialRowsRequest;
        materialOpenEpoch = epoch;
        materialOpening = true;
        UpdateButtons();
        Queue(
            command == Monstey_Open ? "material_open" : "material_load_project",
            J::object({{"path", Monstey_JsonPath(*chosen)}}),
            [this](const J &reply)
            {
                materialOpening = false;
                if (!reply.at("ok").as_bool())
                {
                    UpdateButtons();
                    Error(reply.at("error").at("message").as_string());
                    return;
                }
                materialSelected.clear();
                SetWindowTextW(query, L"");
                MaterialAccept(reply.at("result"));
            },
            false);
        return;
    }
    if (materialView.empty())
    {
        SetStatus(L"Open a native material first.");
        return;
    }
    if (command == Monstey_Save || command == Monstey_Build)
    {
        const bool build = command == Monstey_Build;
        auto output = Monstey_FileDialog(
            window, true, build ? L"Native material (*.mat)\0*.mat\0\0" : L"Material project (*.json)\0*.json\0\0",
            build ? L"mat" : L"json", L"Write a NEW material output");
        if (output)
            MaterialSubmit(build ? "material_build" : "material_save_project",
                           J::object({{build ? "output" : "path", Monstey_JsonPath(*output)}}));
        return;
    }
    if (command == Monstey_Undo || command == Monstey_Redo || command == Monstey_Revert ||
        command == Monstey_MaterialChanges)
    {
        MaterialSubmit(command == Monstey_Undo     ? "material_undo"
                       : command == Monstey_Redo   ? "material_redo"
                       : command == Monstey_Revert ? "material_revert"
                                                   : "material_changes",
                       J::object());
        return;
    }
    if (command == Monstey_Add)
    {
        auto types = J::array();
        for (const auto &[type, label] :
             std::array<std::pair<unsigned, const char *>, 17>{{{1, "Texture path (1)"},
                                                                {2, "Float3 (2)"},
                                                                {3, "Float4 (3)"},
                                                                {4, "UInt32 word (4)"},
                                                                {5, "Float (5)"},
                                                                {6, "Boolean word (6)"},
                                                                {8, "Observed SurfaceRecord string (8)"},
                                                                {9, "Matrix, 16 floats (9)"},
                                                                {10, "Matrix, 12 floats (10)"},
                                                                {11, "Float2 (11)"},
                                                                {13, "Raw 8 bytes (13)"},
                                                                {14, "Raw 8 bytes (14)"},
                                                                {15, "Raw 8 bytes (15)"},
                                                                {7, "Zero storage (7)"},
                                                                {12, "Zero storage (12)"},
                                                                {0, "Zero storage (0)"},
                                                                {16, "Zero storage (16)"}}})
            types.as_array().push_back(J::object({{"value", type}, {"label", label}}));
        const auto savedGeneration = materialView.at("ui_generation"), savedRevision = materialView.at("ui_revision");
        StructureForm(L"Add material property",
                      J::array({Monstey_StructureText("name", "Property name", "Monstey_Property"),
                                Monstey_StructureChoice("table", "Native table",
                                                        J::array({J::object({{"value", "a"}, {"label", "Table A"}}),
                                                                  J::object({{"value", "b"}, {"label", "Table B"}})}),
                                                        "b"),
                                Monstey_StructureChoice("type_u16", "Value type", types, 5),
                                Monstey_FxNumber("flag_a", "Flag A (0-255)", 0, true),
                                Monstey_FxNumber("flag_b", "Flag B (0-255)", 0, true)}),
                      [this, savedGeneration, savedRevision](const J &form)
                      {
                          Monstey_Require(savedGeneration == J(generation) && savedRevision == J(revision),
                                          "Material changed while the property form was open; reopen it");
                          auto draft = form;
                          draft["op"] = "add";
                          const auto type = form.at("type_u16").as_uint();
                          auto row =
                              J::object({{"name", form.at("name")}, {"type_u16", type}, {"value_bytes_hex", ""}});
                          if (type == 1 || type == 8)
                              row["serialized_string"] = "";
                          else if (type == 4 || type == 6)
                              row["value_words_u32"] = J::array({0});
                          else if (type == 2 || type == 3 || type == 5 || type == 9 || type == 10 || type == 11)
                          {
                              auto values = J::array();
                              const auto width = type == 2    ? 3
                                                 : type == 3  ? 4
                                                 : type == 5  ? 1
                                                 : type == 9  ? 16
                                                 : type == 10 ? 12
                                                              : 2;
                              for (int i = 0; i < width; ++i)
                                  values.as_array().push_back(0);
                              row["value_f32"] = values;
                          }
                          else if (type >= 13 && type <= 15)
                              row["value_bytes_hex"] = "0000000000000000";
                          MaterialValue(draft, row, false);
                      });
        return;
    }
    const auto selected = ListView_GetNextItem(records, -1, LVNI_SELECTED);
    Monstey_Require(selected >= 0 && static_cast<std::size_t>(selected) < rowData.size(),
                    "Select a material property first");
    const auto row = rowData[static_cast<std::size_t>(selected)];
    J draft = J::object({{"op", "set"}, {"table", row.at("table")}, {"index", row.at("ordinal")}});
    if (command == Monstey_MaterialEdit || command == Monstey_MaterialRaw)
    {
        MaterialValue(draft, row, command == Monstey_MaterialRaw);
        return;
    }
    if (command == Monstey_Delete)
    {
        draft["op"] = "remove";
        MaterialSubmit("material_edit", J::object({{"operations", J::array({draft})},
                                                   {"expected_generation", materialView.at("ui_generation")},
                                                   {"expected_revision", materialView.at("ui_revision")}}));
        return;
    }
    if (command == Monstey_Rename)
    {
        draft["op"] = "rename";
        StructureForm(
            L"Rename material property",
            J::array({Monstey_StructureText("name", "New property name", row.at("name").as_string().c_str())}),
            [this](const J &result) { MaterialAccept(result); }, "material_edit",
            J::object({{"operations", J::array({draft})}}));
    }
}

} // namespace Monstey::Monstey_AppInternal
