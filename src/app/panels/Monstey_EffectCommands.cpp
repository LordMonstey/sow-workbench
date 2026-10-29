#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::FxCommand(int command)
{
    if (fxLoading && command != Monstey_FxCreate)
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
    if (command == Monstey_Effects)
    {
        auto menu = CreatePopupMenu();
        Monstey_FxMenu(menu);
        RECT rect{};
        GetWindowRect(GetDlgItem(window, Monstey_Effects), &rect);
        auto chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN, rect.left, rect.bottom, 0, window, nullptr);
        DestroyMenu(menu);
        if (chosen)
            Command(static_cast<int>(chosen));
        return;
    }
    Monstey_Require(!sourceNames[3].empty(), "Create or open an effect project first");
    const auto accepted = [this](const J &result) { FxAccepted(result); };
    auto trackChoices = J::array();
    trackChoices.as_array().push_back(J::object({{"label", "Default / first track"}, {"value", ""}}));
    J track = J::object();
    std::size_t trackIndex = 0;
    if (fxWorkflow.contains("layout"))
        for (std::size_t i = 0; i < fxWorkflow.at("layout").at("tracks").size(); ++i)
        {
            const auto &row = fxWorkflow.at("layout").at("tracks")[i];
            trackChoices.as_array().push_back(J::object({{"label", row.at("name")}, {"value", row.at("id")}}));
            if (row.at("id").as_string() == fxView)
            {
                track = row;
                trackIndex = i;
            }
        }
    const auto selected = ListView_GetNextItem(records, -1, LVNI_SELECTED);
    const auto row = selected >= 0 && static_cast<std::size_t>(selected) < rowData.size()
                         ? rowData[static_cast<std::size_t>(selected)]
                         : J::object();
    if (command == Monstey_FxCreate)
    {
        StructureForm(
            L"Create independent native effect",
            J::array({Monstey_StructureText("name", "Resource path / name", "resources/fx/monstey/new_effect.fxp"),
                      Monstey_FxNumber("duration_ms", "Duration (milliseconds)", 1000, true)}),
            [this](const J &values) { FxCreateValues("fx_create_effect", values, "FxGlobalProps"); });
        return;
    }
    if (command == Monstey_FxRename || command == Monstey_FxDuplicate)
    {
        auto args = J::object();
        if (command == Monstey_FxDuplicate)
        {
            auto output = Monstey_FileDialog(window, true, Monstey_SourceFilter(3), L"fxp",
                                             L"Duplicate whole effect to a NEW file");
            if (!output)
                return;
            args["output"] = Monstey_JsonPath(*output);
        }
        StructureForm(command == Monstey_FxRename ? L"Rename effect resource" : L"Duplicate whole native effect",
                      J::array({Monstey_StructureText("name", "Resource path / name",
                                                      Monstey_FxValue(fxGlobal, "FxName", "").as_string().c_str())}),
                      accepted, command == Monstey_FxRename ? "fx_rename_effect" : "fx_duplicate_effect", args);
        return;
    }
    if (command == Monstey_FxDuration)
    {
        StructureForm(L"Native effect duration",
                      J::array({Monstey_FxNumber("duration_ms", "Milliseconds",
                                                 Monstey_FxValue(fxGlobal, "ResourceLenMS", 0), true)}),
                      accepted, "fx_set_duration");
        return;
    }
    if (command == Monstey_FxDelete || command == Monstey_FxReconcile)
    {
        FxApply(command == Monstey_FxDelete ? "fx_delete_effect" : "fx_reconcile_layout");
        return;
    }
    if (command == Monstey_FxTrackCreate)
    {
        StructureForm(L"New editor track", J::array({Monstey_StructureText("name", "Track name", "New track")}),
                      accepted, "fx_create_track");
        return;
    }
    if (command >= Monstey_FxTrackRename && command <= Monstey_FxTrackDown)
    {
        Monstey_Require(!track.empty(), "Select a track in the Effects tree");
        auto args = J::object({{"track", track.at("id")}});
        if (command == Monstey_FxTrackRename)
            StructureForm(L"Rename editor track",
                          J::array({Monstey_StructureText("name", "Track name", track.at("name").as_string().c_str())}),
                          accepted, "fx_rename_track", args);
        else if (command == Monstey_FxTrackDelete)
            FxApply("fx_delete_track", args);
        else
        {
            const auto count = fxWorkflow.at("layout").at("tracks").size();
            if (command == Monstey_FxTrackUp && trackIndex == 0)
                return;
            if (command == Monstey_FxTrackDown && trackIndex + 1 >= count)
                return;
            args["index"] = command == Monstey_FxTrackUp ? trackIndex - 1 : trackIndex + 1;
            FxApply("fx_move_track", args);
        }
        return;
    }
    if (command == Monstey_FxKeyCreate)
    {
        Queue("fx_schema", J::object(),
              [this, trackChoices](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  auto classes = J::array();
                  std::string chosen;
                  for (const auto &row : reply.at("result").at("classes").as_array())
                  {
                      classes.as_array().push_back(
                          J::object({{"label", row.at("class_name")}, {"value", row.at("class_name")}}));
                      if (chosen.empty() || row.at("class_name").as_string() == "ClientFX.DynaLight")
                          chosen = row.at("class_name").as_string();
                  }
                  Monstey_Require(!classes.empty(), "The native schema declares no ClientFX classes");
                  StructureForm(
                      L"Create native effect element",
                      J::array({Monstey_StructureChoice("class_name", "Native element class", classes, chosen),
                                Monstey_StructureText("name", "Key name", "New element"),
                                Monstey_FxNumber("start", "Start (seconds)", 0),
                                Monstey_FxNumber("end", "End (seconds)", 1),
                                Monstey_StructureChoice("track", "Track", trackChoices,
                                                        fxView.starts_with("track-") ? J(fxView) : J(""))}),
                      [this](const J &values)
                      {
                          auto args = values;
                          auto className = values.at("class_name").as_string();
                          FxCreateValues("fx_create_element", args, className);
                      });
              });
        return;
    }
    if (command == Monstey_FxKeyLink)
    {
        auto choices = J::array();
        std::set<std::string> refs;
        for (const auto *list : {&fxWorkflow.at("active_keys"), &fxWorkflow.at("unlinked_elements")})
            for (const auto &key : list->as_array())
                if (key.contains("reference") && refs.insert(key.at("reference").as_string()).second)
                    choices.as_array().push_back(
                        J::object({{"label", Monstey_FxLabel(key) + " [" + key.at("reference").as_string() + "]"},
                                   {"value", key.at("reference")}}));
        Monstey_Require(!choices.empty(), "Create a physical element before linking an existing key");
        StructureForm(L"Link shared native element",
                      J::array({Monstey_StructureChoice("record", "Existing element", choices, choices[0].at("value")),
                                Monstey_StructureChoice("track", "Destination track", trackChoices,
                                                        track.empty() ? J("") : track.at("id"))}),
                      accepted, "fx_link_key");
        return;
    }
    Monstey_Require(row.contains("reference") && row.value("category", "").as_string().starts_with("ClientFX."),
                    "Select a physical ClientFX element first");
    const auto ref = row.at("reference").as_string();
    auto args = J::object({{"record", ref}});
    if (command == Monstey_FxKeyDuplicate)
    {
        args = J::object({{"source", ref}});
        StructureForm(L"Duplicate physical element",
                      J::array({Monstey_StructureText("name", "New key name", (Monstey_FxLabel(row) + " copy").c_str()),
                                Monstey_StructureChoice("track", "Destination track", trackChoices,
                                                        track.empty() ? J("") : track.at("id"))}),
                      accepted, "fx_duplicate_element", args);
        return;
    }
    if (command == Monstey_FxKeyRename)
    {
        StructureForm(L"Rename native element",
                      J::array({Monstey_StructureText("name", "Key name", Monstey_FxLabel(row).c_str())}), accepted,
                      "fx_rename_element", args);
        return;
    }
    if (command == Monstey_FxKeyDelete)
    {
        FxApply("fx_delete_element", args);
        return;
    }
    if (command == Monstey_FxKeyRetime)
    {
        StructureForm(L"Native key timing",
                      J::array({Monstey_FxNumber("start", "Start (seconds)", Monstey_FxValue(row, "StartTime", 0)),
                                Monstey_FxNumber("end", "End (seconds)", Monstey_FxValue(row, "EndTime", 1))}),
                      accepted, "fx_retime_key", args);
        return;
    }
    Monstey_Require(row.contains("fx_track") && row.contains("fx_key_index"), "Select a linked key inside a track");
    auto index = row.at("fx_key_index").as_uint();
    args = J::object({{"track", row.at("fx_track")}, {"index", index}});
    if (command == Monstey_FxKeyUnlink)
        FxApply("fx_unlink_key", args);
    else
    {
        const auto size = track.at("keys").size();
        if (command == Monstey_FxKeyUp && index == 0)
            return;
        if (command == Monstey_FxKeyDown && index + 1 >= size)
            return;
        args["to"] = command == Monstey_FxKeyUp ? index - 1 : index + 1;
        FxApply("fx_move_key", args);
    }
}

} // namespace Monstey::Monstey_AppInternal
