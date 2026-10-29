#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

bool Monstey_App::Monstey_SowAtomAvailable() const
{
    if (closing || tab != 0 || databaseNativeMode || !Monstey_SowAtomGadb17 || !records || selector.empty() ||
        metadata.empty() || !metadata.contains("generation") || !metadata.contains("revision") ||
        metadata.value("view_tab", -1) != J(0) || metadata.at("generation").as_uint() != generation ||
        metadata.at("revision").as_uint() != revision)
        return false;
    const auto selected = ListView_GetNextItem(records, -1, LVNI_SELECTED);
    if (selected < 0 || static_cast<std::size_t>(selected) >= rowData.size() ||
        rowData[static_cast<std::size_t>(selected)].value("reference", "") != J(selector))
        return false;
    return metadata.value("reference", "") == J(selector) || metadata.value("selector", "") == J(selector);
}

void Monstey_App::Monstey_SowAtomCommand()
{
    if (!Monstey_SowAtomAvailable())
    {
        if (Monstey_SowAtomSmoke)
            smoke["sow_atom_guard_rejections"] = smoke.value("sow_atom_guard_rejections", 0u).as_uint() + 1;
        SetStatus(L"Select a current SoW database record before inspecting storage.");
        return;
    }
    const auto requestedEpoch = epoch, requestedGeneration = generation, requestedRevision = revision;
    const auto requestedSelector = selector;
    const auto requestedReference = metadata.at("reference");
    const auto requestedSource = sourceNames[0];
    if (Monstey_SowAtomSmoke)
        smoke["sow_atom_requests_queued"] = smoke.value("sow_atom_requests_queued", 0u).as_uint() + 1;
    Queue("database_atom_uses",
          J::object({{"record", requestedSelector},
                     {"offset", 0},
                     {"limit", 500},
                     {"expected_generation", requestedGeneration},
                     {"expected_revision", requestedRevision}}),
          [this, requestedEpoch, requestedGeneration, requestedRevision, requestedSelector, requestedReference,
           requestedSource](const J &reply)
          {
              if (closing || epoch != requestedEpoch || tab != 0 || databaseNativeMode ||
                  selector != requestedSelector || sourceNames[0] != requestedSource ||
                  generation != requestedGeneration || revision != requestedRevision || !Monstey_SowAtomAvailable())
                  return;
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              if (reply.at("generation").as_uint() != requestedGeneration ||
                  reply.at("revision").as_uint() != requestedRevision)
                  return;
              const auto &report = reply.at("result");
              Monstey_Require(report.at("schema") == J("Monstey.GadbAtomUses/1") &&
                                  report.at("reference") == requestedReference,
                              "Storage inspection returned a different record/report");
              ShowReport(report);
              if (Monstey_SowAtomSmoke)
              {
                  smoke["sow_atom_reports_shown"] = smoke.value("sow_atom_reports_shown", 0u).as_uint() + 1;
                  smoke["sow_atom_report"] = report;
                  smoke["sow_atom_raw_report_panel"] = Monstey_WindowText(details) == Monstey_Wide(report.dump(2));
              }
          });
}

void Monstey_App::Monstey_SaveDatabaseView()
{
    if (tab != 0 || databaseNativeMode || databaseRestorePending)
        return;
    databaseSavedCategory = category;
    databaseSavedSelector = selector;
    databaseSavedOffset = offset;
    databaseSavedFilter = Monstey_WindowText(query);
    databaseSavedField = ListView_GetNextItem(fields, -1, LVNI_SELECTED);
    databaseViewSaved = true;
}

void Monstey_App::Monstey_DatabaseNativeCommand(int command)
{
    if (closing)
        return;
    if (editor && editor->window)
    {
        SetForegroundWindow(editor->window);
        SetStatus(L"Finish or cancel the current editor before changing view.");
        return;
    }
    const bool back =
        command == Monstey_DatabaseRecordsMenu || (command == Monstey_NativeDatabaseTools && databaseNativeMode);
    if (back)
    {
        if (!selectedOperation.empty())
            operationPresets[selectedOperation.at("name").as_string()] = operationArguments;
        databaseNativeMode = false;
        databaseRestorePending = databaseViewSaved;
        databaseRestoreField = databaseViewSaved;
        selectedOperation = J::object();
        operationArguments = J::object();
        preferredOperation.clear();
        TabCtrl_SetCurSel(tabs, 0);
        TabChanged();
        if (sourceNames[0].empty())
            databaseRestorePending = false;
        SetStatus(L"Database records restored. Native inspection did not change the project.");
        return;
    }
    if (!databaseNativeMode)
        Monstey_SaveDatabaseView();
    databaseNativeMode = true;
    preferredOperation = command == Monstey_NativeClassMenu ? "database_native_class" : "database_native_banks";
    TabCtrl_SetCurSel(tabs, 0);
    TabChanged();
}

} // namespace Monstey::Monstey_AppInternal
