#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::LoadFxNested()
{
    if (tab != 3 || !fxNestedMode || metadata.empty() ||
        metadata.value("category", "").as_string() != "ClientFX.Create")
        return;
    const auto ref = metadata.at("reference").as_string();
    const auto selected = selector;
    const auto request = ++fxNestedRequest;
    fieldData.clear();
    ListView_DeleteAllItems(fields);
    fxNestedView = J::object();
    Queue("fx_nested_inspect", J::object({{"record", ref}}),
          [this, ref, selected, request](const J &reply)
          {
              if (tab != 3 || !fxNestedMode || selector != selected || fxNestedRequest != request ||
                  metadata.value("reference", "").as_string() != ref)
                  return;
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              fxNestedView = reply.at("result");
              fxNestedView["ui_generation"] = reply.at("generation");
              fxNestedView["ui_revision"] = reply.at("revision");
              fieldData = fxNestedView.at("slots").as_array();
              populating = true;
              ListView_DeleteAllItems(fields);
              while (ListView_DeleteColumn(fields, 0))
              {
              }
              Monstey_ListColumn(fields, 0, L"Native slot", 110);
              Monstey_ListColumn(fields, 1, L"Group members", 210);
              Monstey_ListColumn(fields, 2, L"Raw native group", 350);
              for (std::size_t i = 0; i < fieldData.size(); ++i)
              {
                  const auto &row = fieldData[i];
                  const auto activeSlot = row.at("selected_by_native_random_choice").as_bool();
                  Monstey_ListCell(fields, static_cast<int>(i), 0,
                                   L"Effect" + std::to_wstring(row.at("slot").as_uint()));
                  Monstey_ListCell(fields, static_cast<int>(i), 1,
                                   row.value("valid", false).as_bool()
                                       ? std::to_wstring(row.at("effects").size()) +
                                             (activeSlot ? L" resource(s), selectable" : L" resource(s), disabled")
                                       : L"Invalid group; inspect warnings");
                  Monstey_ListCell(fields, static_cast<int>(i), 2, Monstey_Display(row.at("raw")));
              }
              if (!fieldData.empty())
                  Monstey_ListSelect(fields, 0);
              populating = false;
              auto text = L"Nested CreateFX: native iterations " +
                          Monstey_Display(fxNestedView.at("native_num_to_create")) + L" (stored " +
                          Monstey_Display(fxNestedView.at("authored_num_to_create")) + L"), Loop " +
                          Monstey_Display(fxNestedView.at("loop")) + L".\r\nCreateContext: " +
                          Monstey_Display(fxNestedView.at("create_context")) +
                          L"\r\nOne random nonempty slot per iteration; all resources in "
                          L"the chosen group are requested. Playback and original parent "
                          L"attachment are unverified.";
              for (const auto &warning : fxNestedView.at("warnings").as_array())
                  text += L"\r\n" + Monstey_Display(warning);
              SetWindowTextW(details, text.c_str());
              SetWindowTextW(pageLabel, L"16 native alternatives. Double-click a slot to edit one "
                                        L"resource name per line; an empty list disables it.");
              SmokeStep();
          });
}

void Monstey_App::FxNestedCommand(int command)
{
    if (fxLoading)
    {
        SetStatus(L"Wait for the native effect refresh to finish.");
        return;
    }
    if (tab != 3)
        return;
    if (editor && editor->window)
    {
        SetForegroundWindow(editor->window);
        return;
    }
    if (command == Monstey_FxNested)
    {
        auto menu = CreatePopupMenu();
        for (auto [id, label] : {std::pair{Monstey_FxNestedInspect, L"Inspect selected CreateFX: 16 slots"},
                                 std::pair{Monstey_FxNestedSlot, L"Edit selected group..."},
                                 std::pair{Monstey_FxNestedOptions, L"Iteration count and Loop..."},
                                 std::pair{Monstey_FxNestedBack, L"Back to native properties"}})
            AppendMenuW(menu, MF_STRING, id, label);
        RECT rect{};
        GetWindowRect(GetDlgItem(window, Monstey_FxNested), &rect);
        auto chosen = TrackPopupMenu(menu, TPM_RETURNCMD, rect.left, rect.bottom, 0, window, nullptr);
        DestroyMenu(menu);
        if (chosen)
            Command(static_cast<int>(chosen));
        return;
    }
    if (command == Monstey_FxNestedBack)
    {
        fxNestedMode = false;
        fxNestedView = J::object();
        ++fxNestedRequest;
        while (ListView_DeleteColumn(fields, 0))
        {
        }
        Monstey_ListColumn(fields, 0, L"Property", 190);
        Monstey_ListColumn(fields, 1, L"Type", 100);
        Monstey_ListColumn(fields, 2, L"Value", 350);
        auto row = ListView_GetNextItem(records, -1, LVNI_SELECTED);
        if (row >= 0)
            ReadRecord(row);
        return;
    }
    Monstey_Require(metadata.value("category", "").as_string() == "ClientFX.Create",
                    "Select a native ClientFX.Create record and wait for its fields");
    if (command == Monstey_FxNestedInspect)
    {
        fxNestedMode = true;
        LoadFxNested();
        return;
    }
    Monstey_Require(fxNestedMode && fxNestedView.contains("record") &&
                        fxNestedView.at("record") == metadata.at("reference"),
                    "Inspect this CreateFX record's slots first");
    auto args = J::object({{"record", fxNestedView.at("record")},
                           {"expected_generation", fxNestedView.at("ui_generation")},
                           {"expected_revision", fxNestedView.at("ui_revision")}});
    const auto accepted = [this](const J &result) { FxAccepted(result); };
    if (command == Monstey_FxNestedOptions)
    {
        StructureForm(L"Native nested effect options",
                      J::array({Monstey_FxNumber("num_to_create", "Native iteration count (0..255)",
                                                 fxNestedView.at("native_num_to_create"), true),
                                J::object({{"name", "loop"},
                                           {"label", "Loop nested creation"},
                                           {"kind", "bool"},
                                           {"value", fxNestedView.at("loop")}})}),
                      accepted, "fx_nested_set_options", args);
        return;
    }
    if (command == Monstey_FxNestedSlot)
    {
        const auto selected = ListView_GetNextItem(fields, -1, LVNI_SELECTED);
        Monstey_Require(selected >= 0 && static_cast<std::size_t>(selected) < fieldData.size(),
                        "Select one of the sixteen native slots");
        const auto row = fieldData[static_cast<std::size_t>(selected)];
        args["slot"] = row.at("slot");
        auto item = std::make_shared<Monstey_Editor>();
        item->app = this;
        item->title = L"Nested " + Monstey_Display(row.at("field")) + L" group";
        item->field = J::object({{"name", "One native resource name per line; "
                                          "empty list disables this alternative"},
                                 {"native_type", 4},
                                 {"array", true},
                                 {"value", row.value("effects", J::array())},
                                 {"editable", true}});
        item->lineList = true;
        item->operation = "fx_nested_set_slot";
        item->arguments = args;
        item->valueKey = "effects";
        item->generation = fxNestedView.at("ui_generation").as_uint();
        item->revision = fxNestedView.at("ui_revision").as_uint();
        item->success = accepted;
        ShowEditor(item);
    }
}

} // namespace Monstey::Monstey_AppInternal
