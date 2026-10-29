#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::Monstey_DatabaseNativeSmokePrepare()
{
    Monstey_Require(smokeDomain == 0 && !smokeReport.empty(), "Native Database smoke has no explicit test context");
    smokeStage = -4;
    Queue(
        "database_new", J::object(),
        [this](const J &created)
        {
            Monstey_Require(created.at("ok").as_bool(), "Synthetic Database creation failed");
            UpdateState(created.at("result"), 0);
            Queue("database_create_category",
                  J::object({{"schema_name", "Monstey.NativeUi"}, {"name", "Unsaved native UI"}}),
                  [this](const J &categoryReply)
                  {
                      Monstey_Require(categoryReply.at("ok").as_bool(), "Synthetic Database category failed");
                      UpdateState(categoryReply.at("result"), 0);
                      Queue("database_create_record",
                            J::object({{"category_index", 0}, {"name", "Monstey_UnpublishedUiRecord"}}),
                            [this](const J &recordReply)
                            {
                                Monstey_Require(recordReply.at("ok").as_bool(), "Synthetic Database record failed");
                                UpdateState(recordReply.at("result"), 0);
                                Monstey_Require(dirty[0] && canUndo[0],
                                                "Synthetic Database did not retain unsaved edits/history");
                                smoke["native_database_fixture_created"] = true;
                                smokeStage = -2;
                                TabCtrl_SetCurSel(tabs, 0);
                                TabChanged();
                            });
                  });
        },
        false);
}

void Monstey_App::Monstey_DatabaseNativeSmokeStart()
{
    smokeStage = -3;
    Queue("describe", J::object(),
          [this](const J &before)
          {
              Monstey_Require(before.at("ok").as_bool(), "Native UI initial project description failed");
              smoke["native_database_before_project"] = before.at("result");
              smoke["native_database_before_generation"] = before.at("generation");
              smoke["native_database_before_revision"] = before.at("revision");
              Queue("database_changes", J::object(),
                    [this](const J &changes)
                    {
                        Monstey_Require(changes.at("ok").as_bool(), "Native UI initial changes read failed");
                        smoke["native_database_before_changes"] = changes.at("result");
                        smoke["native_database_before_source_label"] = Monstey_Utf8(sourceNames[0]);
                        smoke["native_database_before_selector"] = selector;
                        smoke["native_database_before_filter"] = Monstey_Utf8(Monstey_WindowText(query));
                        smoke["native_database_before_offset"] = offset;
                        smoke["native_database_before_dirty"] = dirty[0];
                        smoke["native_database_before_can_undo"] = canUndo[0];
                        smoke["native_database_before_can_redo"] = canRedo[0];
                        smokeStage = 0;
                        SendMessageW(window, WM_COMMAND,
                                     smokeOperation == "database_native_class" ? Monstey_NativeClassMenu
                                                                               : Monstey_NativeBanksMenu,
                                     0);
                    });
          });
}

void Monstey_App::Monstey_DatabaseNativeSmokeFinish()
{
    smokeStage = 15;
    SendMessageW(window, WM_COMMAND, Monstey_DatabaseRecordsMenu, 0);
}

void Monstey_App::Monstey_SowAtomSmokePrepare()
{
    Monstey_Require(!smokeReport.empty(), "SoW atom smoke needs an explicit report");
    smokeStage = 0;
    smoke["sow_atom_synthetic_only"] = true;
    Queue(
        "database_new", J::object(),
        [this](const J &created)
        {
            Monstey_Require(created.at("ok").as_bool(), "SoW atom synthetic database failed: " + created.dump());
            UpdateState(created.at("result"), 0);
            Queue("database_create_category",
                  J::object({{"schema_name", "AI/Alertness/Level"}, {"name", "Monstey.SowAtomGui"}}),
                  [this](const J &categoryReply)
                  {
                      Monstey_Require(categoryReply.at("ok").as_bool(),
                                      "SoW atom synthetic category failed: " + categoryReply.dump());
                      UpdateState(categoryReply.at("result"), 0);
                      Queue("database_create_record",
                            J::object({{"category_index", 0}, {"name", "Monstey_StorageProbe"}}),
                            [this](const J &recordReply)
                            {
                                Monstey_Require(recordReply.at("ok").as_bool(),
                                                "SoW atom synthetic record failed: " + recordReply.dump());
                                UpdateState(recordReply.at("result"), 0);
                                Queue("database_add_field",
                                      J::object({{"record", "0x00000000"},
                                                 {"field", "StorageValue"},
                                                 {"native_type", 4},
                                                 {"value", "synthetic storage"}}),
                                      [this](const J &fieldReply)
                                      {
                                          Monstey_Require(fieldReply.at("ok").as_bool(),
                                                          "SoW atom synthetic field failed: " + fieldReply.dump());
                                          UpdateState(fieldReply.at("result"), 0);
                                          Monstey_Require(dirty[0] && canUndo[0] && !canRedo[0],
                                                          "SoW atom fixture has no pending history");
                                          smokeStage = 1;
                                          TabCtrl_SetCurSel(tabs, 0);
                                          TabChanged();
                                      });
                            });
                  });
        },
        false);
}

void Monstey_App::Monstey_SowAtomSmokeStep()
{
    const auto Monstey_MenuEnabled = [this]()
    {
        SendMessageW(window, WM_INITMENU, reinterpret_cast<WPARAM>(GetMenu(window)), 0);
        const auto state = GetMenuState(GetSubMenu(GetMenu(window), 2), Monstey_InspectStorage, MF_BYCOMMAND);
        Monstey_Require(state != UINT(-1), "SoW storage menu item missing");
        return (state & (MF_GRAYED | MF_DISABLED)) == 0;
    };
    if (smokeStage == 1 && Monstey_SowAtomAvailable())
    {
        if (!Monstey_DatabaseTreeSmokeStep())
            return;
        smokeStage = 2;
        smoke["sow_atom_before_selector"] = selector;
        smoke["sow_atom_before_source_label"] = Monstey_Utf8(sourceNames[0]);
        smoke["sow_atom_before_dirty"] = dirty[0];
        smoke["sow_atom_before_can_undo"] = canUndo[0];
        smoke["sow_atom_before_can_redo"] = canRedo[0];
        smoke["sow_atom_before_selected_row"] = ListView_GetNextItem(records, -1, LVNI_SELECTED);
        Queue("describe", J::object(),
              [this](const J &before)
              {
                  Monstey_Require(before.at("ok").as_bool(), "SoW atom before describe failed");
                  smoke["sow_atom_before_project"] = before.at("result");
                  smoke["sow_atom_before_generation"] = before.at("generation");
                  smoke["sow_atom_before_revision"] = before.at("revision");
                  Queue("database_changes", J::object(),
                        [this](const J &changes)
                        {
                            Monstey_Require(changes.at("ok").as_bool(), "SoW atom before history failed");
                            smoke["sow_atom_before_changes"] = changes.at("result");
                            smokeStage = 3;
                        });
              });
        return;
    }
    if (smokeStage == 3)
    {
        Monstey_Require(Monstey_MenuEnabled(), "Current SoW record storage menu disabled");
        smoke["sow_atom_menu_present_and_enabled"] = true;
        smokeStage = 4;
        SendMessageW(window, WM_COMMAND, Monstey_InspectStorage, 0);
        return;
    }
    if (smokeStage == 4 && smoke.value("sow_atom_reports_shown", 0u).as_uint() == 1)
    {
        Monstey_Require(selector == smoke.at("sow_atom_before_selector").as_string() &&
                            dirty[0] == smoke.at("sow_atom_before_dirty").as_bool() &&
                            canUndo[0] == smoke.at("sow_atom_before_can_undo").as_bool() &&
                            canRedo[0] == smoke.at("sow_atom_before_can_redo").as_bool(),
                        "Storage report changed current selection/project/history");
        smoke["sow_atom_positive_selection_history_preserved"] = true;
        smokeStage = 5;
        SendMessageW(window, WM_COMMAND, Monstey_InspectStorage, 0);
        TabCtrl_SetCurSel(tabs, 3);
        TabChanged();
        return;
    }
    if (smokeStage == 5 && tab == 3 && smoke.value("sow_atom_stale_callbacks_discarded", 0u).as_uint() >= 1)
    {
        Monstey_Require(!Monstey_MenuEnabled(), "Storage action enabled in Fx");
        SendMessageW(window, WM_COMMAND, Monstey_InspectStorage, 0);
        smoke["sow_atom_fx_guarded"] = true;
        smokeStage = 6;
        TabCtrl_SetCurSel(tabs, 0);
        TabChanged();
        return;
    }
    if (smokeStage == 6 && Monstey_SowAtomAvailable())
    {
        smokeStage = 7;
        SendMessageW(window, WM_COMMAND, Monstey_NativeBanksMenu, 0);
        return;
    }
    if (smokeStage == 7 && databaseNativeMode && rowData.size() == 2 && !selectedOperation.empty())
    {
        Monstey_Require(!Monstey_MenuEnabled(), "Storage action enabled in native PE form");
        SendMessageW(window, WM_COMMAND, Monstey_InspectStorage, 0);
        smoke["sow_atom_pe_guarded_two_readers_preserved"] = true;
        smokeStage = 8;
        SendMessageW(window, WM_COMMAND, Monstey_DatabaseRecordsMenu, 0);
        return;
    }
    if (smokeStage == 8 && Monstey_SowAtomAvailable())
    {
        const int selected = ListView_GetNextItem(records, -1, LVNI_SELECTED);
        populating = true;
        ListView_SetItemState(records, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
        populating = false;
        Monstey_Require(!Monstey_MenuEnabled() && !Monstey_SowAtomAvailable(),
                        "Storage action enabled without selection");
        SendMessageW(window, WM_COMMAND, Monstey_InspectStorage, 0);
        populating = true;
        Monstey_ListSelect(records, selected);
        populating = false;
        smoke["sow_atom_no_selection_guarded"] = true;
        Monstey_Require(Monstey_SowAtomAvailable(), "SoW record did not restore after guard probe");
        smokeStage = 9;
        SendMessageW(window, WM_COMMAND, Monstey_InspectStorage, 0);
        return;
    }
    if (smokeStage == 9 && smoke.value("sow_atom_reports_shown", 0u).as_uint() == 2)
    {
        smokeStage = 10;
        Queue("describe", J::object(),
              [this](const J &after)
              {
                  Monstey_Require(after.at("ok").as_bool() &&
                                      after.at("result") == smoke.at("sow_atom_before_project") &&
                                      after.at("generation") == smoke.at("sow_atom_before_generation") &&
                                      after.at("revision") == smoke.at("sow_atom_before_revision"),
                                  "Storage report changed Runtime project/session");
                  Queue("database_changes", J::object(),
                        [this](const J &changes)
                        {
                            Monstey_Require(changes.at("ok").as_bool() &&
                                                changes.at("result") == smoke.at("sow_atom_before_changes"),
                                            "Storage report changed pending edits/undo state");
                            Monstey_Require(selector == smoke.at("sow_atom_before_selector").as_string() &&
                                                Monstey_Utf8(sourceNames[0]) ==
                                                    smoke.at("sow_atom_before_source_label").as_string() &&
                                                dirty[0] == smoke.at("sow_atom_before_dirty").as_bool() &&
                                                canUndo[0] == smoke.at("sow_atom_before_can_undo").as_bool() &&
                                                canRedo[0] == smoke.at("sow_atom_before_can_redo").as_bool() &&
                                                ListView_GetNextItem(records, -1, LVNI_SELECTED) ==
                                                    smoke.at("sow_atom_before_selected_row").as_int(),
                                            "Storage report changed restored source/selection/history");
                            Monstey_Require(smoke.at("sow_atom_report").at("rows").size() == 1 &&
                                                smoke.at("sow_atom_report").at("rows").at(0).at("status") ==
                                                    J("not_yet_serialized") &&
                                                smoke.at("sow_atom_report").at("rows").at(0).at("raw_u32").is_null(),
                                            "Logical New storage report fabricated a serialized atom");
                            Monstey_Require(smoke.value("sow_atom_guard_rejections", 0u).as_uint() == 3 &&
                                                smoke.value("sow_atom_requests_queued", 0u).as_uint() == 3 &&
                                                smoke.value("sow_atom_reports_shown", 0u).as_uint() == 2,
                                            "Storage guard/dispatch/stale request counts differ");
                            smoke["sow_atom_project_history_source_selection_preserved"] = true;
                            smoke["sow_atom_logical_new_pending_observed"] = true;
                            smoke["sow_atom_real_menu_dispatch"] = true;
                            SmokeComplete();
                        });
              });
    }
}

} // namespace Monstey::Monstey_AppInternal
