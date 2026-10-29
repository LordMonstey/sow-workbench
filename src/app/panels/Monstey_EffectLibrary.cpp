#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::FxLibraryCommand(int command)
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
    if (fxNestedMode && (command == Monstey_FxPrefabOverride || command == Monstey_FxPrefabInherit))
    {
        SetStatus(L"Nested FX > Back to properties selects native fields for "
                  L"template overrides.");
        return;
    }
    if (command == Monstey_FxLibrary)
    {
        auto menu = CreatePopupMenu();
        for (auto [id, label] : {std::pair{Monstey_FxGroupCreate, L"New library group..."},
                                 std::pair{Monstey_FxGroupRename, L"Rename selected group..."},
                                 std::pair{Monstey_FxGroupMove, L"Move selected group..."},
                                 std::pair{Monstey_FxGroupDelete, L"Delete group and materialize children"},
                                 std::pair{Monstey_FxPrefabCreate, L"Create native component template..."},
                                 std::pair{Monstey_FxPrefabFromKey, L"Make template from selected key..."},
                                 std::pair{Monstey_FxPrefabDuplicate, L"Duplicate selected template..."},
                                 std::pair{Monstey_FxPrefabRename, L"Rename selected template..."},
                                 std::pair{Monstey_FxPrefabMove, L"Move selected template..."},
                                 std::pair{Monstey_FxPrefabParent, L"Template inheritance..."},
                                 std::pair{Monstey_FxPrefabAssign, L"Assign template to selected key..."},
                                 std::pair{Monstey_FxPrefabActive, L"Choose active template..."},
                                 std::pair{Monstey_FxPrefabChild, L"Add independent child key..."},
                                 std::pair{Monstey_FxPrefabOverride, L"Override selected property..."},
                                 std::pair{Monstey_FxPrefabInherit, L"Inherit selected property"},
                                 std::pair{Monstey_FxPrefabDetach, L"Detach selected key from template"},
                                 std::pair{Monstey_FxPrefabDelete, L"Delete selected template, keep live keys"},
                                 std::pair{Monstey_FxPrefabSync, L"Synchronize template properties"}})
            AppendMenuW(menu, MF_STRING, id, label);
        RECT rect{};
        GetWindowRect(GetDlgItem(window, Monstey_FxLibrary), &rect);
        auto chosen = TrackPopupMenu(menu, TPM_RETURNCMD, rect.left, rect.bottom, 0, window, nullptr);
        DestroyMenu(menu);
        if (chosen)
            Command(static_cast<int>(chosen));
        return;
    }
    Monstey_Require(!sourceNames[3].empty(), "Create or open an Effects project first");
    auto groupChoices = J::array({J::object({{"label", "Library root"}, {"value", ""}})});
    if (fxLibrary.contains("groups"))
        for (const auto &[id, group] : fxLibrary.at("groups").as_object())
            groupChoices.as_array().push_back(J::object({{"label", group.at("name")}, {"value", id}}));
    auto prefabChoices = J::array({J::object({{"label", "None / detach"}, {"value", ""}})});
    if (fxLibrary.contains("prefabs"))
        for (const auto &[id, prefab] : fxLibrary.at("prefabs").as_object())
            prefabChoices.as_array().push_back(
                J::object({{"label", prefab.at("name").as_string() + " [" + prefab.at("class_name").as_string() + "]"},
                           {"value", id}}));
    const auto group = fxView.starts_with("library-group:") ? fxView.substr(14) : "";
    auto selected = ListView_GetNextItem(records, -1, LVNI_SELECTED);
    auto row = selected >= 0 && static_cast<std::size_t>(selected) < rowData.size()
                   ? rowData[static_cast<std::size_t>(selected)]
                   : J::object();
    const auto ref = row.value("reference", "").as_string();
    std::string prefabId = row.value("fx_prefab", "").as_string();
    if (prefabId.empty() && fxLibrary.contains("prefabs"))
        for (const auto &[id, prefab] : fxLibrary.at("prefabs").as_object())
            if (prefab.at("record").as_string() == ref)
            {
                prefabId = id;
                break;
            }
    const auto accepted = [this](const J &result) { FxAccepted(result); };
    if (command == Monstey_FxGroupCreate || command == Monstey_FxGroupRename)
    {
        if (command == Monstey_FxGroupRename)
            Monstey_Require(!group.empty(), "Select a library group in the Effects tree");
        auto args = J::object();
        if (command == Monstey_FxGroupRename)
            args["group"] = group;
        auto name =
            command == Monstey_FxGroupRename ? fxLibrary.at("groups").at(group).at("name").as_string() : "New group";
        auto rows = J::array({Monstey_StructureText("name", "Group name", name.c_str())});
        if (command == Monstey_FxGroupCreate)
            rows.as_array().push_back(Monstey_StructureChoice("parent", "Parent group", groupChoices, group));
        StructureForm(L"Component library group", rows, accepted,
                      command == Monstey_FxGroupCreate ? "fx_library_group_create" : "fx_library_group_rename", args);
        return;
    }
    if (command == Monstey_FxGroupMove || command == Monstey_FxGroupDelete)
    {
        Monstey_Require(!group.empty(), "Select a library group in the Effects tree");
        if (command == Monstey_FxGroupMove)
            StructureForm(L"Move library group",
                          J::array({Monstey_StructureChoice("parent", "Parent group", groupChoices,
                                                            fxLibrary.at("groups").at(group).value("parent", ""))}),
                          accepted, "fx_library_group_move", J::object({{"group", group}}));
        else
            FxApply("fx_library_group_delete", J::object({{"group", group}, {"recursive", true}}));
        return;
    }
    if (command == Monstey_FxPrefabCreate)
    {
        Queue("fx_schema", J::object(),
              [this, groupChoices, group](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  auto classes = J::array();
                  std::string initial;
                  for (const auto &definition : reply.at("result").at("classes").as_array())
                  {
                      auto name = definition.at("class_name").as_string();
                      classes.as_array().push_back(J::object({{"label", name}, {"value", name}}));
                      if (initial.empty() || name == "ClientFX.DynaLight")
                          initial = name;
                  }
                  Monstey_Require(!classes.empty(), "The schema has no native component classes");
                  StructureForm(L"Create native component template",
                                J::array({Monstey_StructureText("name", "Template name", "New template"),
                                          Monstey_StructureChoice("class_name", "Native class", classes, initial),
                                          Monstey_StructureChoice("group", "Library group", groupChoices, group),
                                          Monstey_FxNumber("start", "Template start (seconds)", 0),
                                          Monstey_FxNumber("end", "Template end (seconds)", 1)}),
                                [this](const J &values)
                                { FxCreateValues("fx_prefab_create", values, values.at("class_name").as_string()); });
              });
        return;
    }
    if (command == Monstey_FxPrefabActive)
    {
        StructureForm(L"Active component template",
                      J::array({Monstey_StructureChoice("prefab", "Template for new children", prefabChoices,
                                                        fxLibrary.value("active_prefab", ""))}),
                      accepted, "fx_prefab_set_active");
        return;
    }
    if (command == Monstey_FxPrefabChild)
    {
        auto tracks = J::array({J::object({{"label", "Default / first track"}, {"value", ""}})});
        for (const auto &track : fxWorkflow.at("layout").at("tracks").as_array())
            tracks.as_array().push_back(J::object({{"label", track.at("name")}, {"value", track.at("id")}}));
        Monstey_Require(prefabChoices.size() > 1, "Create a template before adding a child");
        auto chosen = prefabId.empty() ? fxLibrary.value("active_prefab", "").as_string() : prefabId;
        if (chosen.empty())
            chosen = prefabChoices[1].at("value").as_string();
        auto requiredChoices = prefabChoices;
        requiredChoices.as_array().erase(requiredChoices.as_array().begin());
        StructureForm(L"Add independent component child",
                      J::array({Monstey_StructureChoice("prefab", "Template", requiredChoices, chosen),
                                Monstey_StructureText("name", "Optional key name", "", true),
                                Monstey_FxNumber("time", "Cursor time (seconds)", 1),
                                Monstey_StructureChoice("track", "Destination track", tracks,
                                                        fxView.starts_with("track-") ? fxView : "")}),
                      accepted, "fx_prefab_add_child");
        return;
    }
    if (command == Monstey_FxPrefabFromKey || command == Monstey_FxPrefabAssign || command == Monstey_FxPrefabDetach ||
        command == Monstey_FxPrefabOverride || command == Monstey_FxPrefabInherit)
    {
        Monstey_Require(!ref.empty() && row.value("category", "").as_string().starts_with("ClientFX."),
                        "Select a physical native component key first");
        if (command == Monstey_FxPrefabFromKey)
        {
            StructureForm(
                L"Make template from native key",
                J::array({Monstey_StructureText("name", "Template name", (Monstey_FxLabel(row) + " template").c_str()),
                          Monstey_StructureChoice("group", "Library group", groupChoices, group)}),
                accepted, "fx_prefab_from_key", J::object({{"record", ref}}));
            return;
        }
        if (command == Monstey_FxPrefabAssign)
        {
            Monstey_Require(prefabChoices.size() > 1, "Create a template first");
            auto required = prefabChoices;
            required.as_array().erase(required.as_array().begin());
            StructureForm(L"Assign native component template",
                          J::array({Monstey_StructureChoice("prefab", "Template (same native class)", required,
                                                            required[0].at("value")),
                                    J::object({{"name", "keep_overrides"},
                                               {"label", "Retain local property overrides"},
                                               {"kind", "bool"},
                                               {"value", true}})}),
                          accepted, "fx_prefab_assign", J::object({{"record", ref}}));
            return;
        }
        if (command == Monstey_FxPrefabDetach)
        {
            FxApply("fx_prefab_detach", J::object({{"record", ref}}));
            return;
        }
        auto fieldIndex = ListView_GetNextItem(fields, -1, LVNI_SELECTED);
        Monstey_Require(metadata.value("reference", "").as_string() == ref && fieldIndex >= 0 &&
                            static_cast<std::size_t>(fieldIndex) < fieldData.size(),
                        "Select a native property of this key first");
        const auto field = fieldData[static_cast<std::size_t>(fieldIndex)];
        auto args = J::object({{"record", ref}, {"field", field.at("name")}});
        if (command == Monstey_FxPrefabInherit)
        {
            FxApply("fx_prefab_inherit", args);
            return;
        }
        auto item = std::make_shared<Monstey_Editor>();
        item->app = this;
        item->title = L"Local template property override";
        item->field = field;
        item->operation = "fx_prefab_override";
        item->arguments = args;
        item->generation = generation;
        item->revision = revision;
        item->success = accepted;
        ShowEditor(item);
        return;
    }
    if (command == Monstey_FxPrefabSync)
    {
        FxApply("fx_prefab_sync");
        return;
    }
    Monstey_Require(!prefabId.empty(), "Select a component template in the library");
    const auto prefab = fxLibrary.at("prefabs").at(prefabId);
    auto args = J::object({{"prefab", prefabId}});
    if (command == Monstey_FxPrefabDelete)
    {
        FxApply("fx_prefab_delete", args);
        return;
    }
    if (command == Monstey_FxPrefabRename || command == Monstey_FxPrefabDuplicate)
    {
        auto rows = J::array({Monstey_StructureText(
            "name", "Template name",
            (prefab.at("name").as_string() + (command == Monstey_FxPrefabDuplicate ? " copy" : "")).c_str())});
        if (command == Monstey_FxPrefabDuplicate)
            rows.as_array().push_back(
                Monstey_StructureChoice("group", "Library group", groupChoices, prefab.value("group", "")));
        StructureForm(L"Component template", rows, accepted,
                      command == Monstey_FxPrefabRename ? "fx_prefab_rename" : "fx_prefab_duplicate", args);
        return;
    }
    if (command == Monstey_FxPrefabMove)
    {
        StructureForm(
            L"Move component template",
            J::array({Monstey_StructureChoice("group", "Library group", groupChoices, prefab.value("group", ""))}),
            accepted, "fx_prefab_move", args);
        return;
    }
    if (command == Monstey_FxPrefabParent)
    {
        StructureForm(L"Template inheritance",
                      J::array({Monstey_StructureChoice("parent", "Parent template (same native class)", prefabChoices,
                                                        prefab.value("parent", "")),
                                J::object({{"name", "keep_overrides"},
                                           {"label", "Retain local property overrides"},
                                           {"kind", "bool"},
                                           {"value", true}})}),
                      accepted, "fx_prefab_parent", args);
    }
}

} // namespace Monstey::Monstey_AppInternal
