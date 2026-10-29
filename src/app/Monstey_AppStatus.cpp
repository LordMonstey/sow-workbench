#include "Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SetStatus(const std::wstring &text)
{
    if (status)
        SetWindowTextW(status, text.c_str());
}

void Monstey_App::Error(const std::string &message)
{
    SetStatus(Monstey_Wide(message));
    if (!smokeReport.empty())
    {
        if (smokeNativeProfile && smokeStage == 4 && smoke.contains("expected_error_code") &&
            smoke.contains("profile_build_reply") && !smoke["profile_build_reply"]["ok"].as_bool() &&
            smoke["profile_build_reply"]["error"]["code"] == smoke["expected_error_code"])
        {
            smoke["observed_expected_error"] = message;
            return;
        }
        if (smokeMaterial && smokeStage == 6 && smoke.value("expect_invalid_raw_hex", false).as_bool() &&
            message == "Hex bytes need pairs of digits")
        {
            smoke["observed_invalid_raw_hex_error"] = message;
            return;
        }
        if (smokeMaterial && smokeStage == 601 && smoke.value("expect_backend_width", false).as_bool() &&
            message == "LTMI value payload differs from native storage width")
        {
            smoke["observed_backend_width_error"] = message;
            return;
        }
        smokeFailed = true;
        smoke["error"] = message;
        smoke["failed_stage"] = smokeStage;
        smoke["failed_selected_operation"] = selectedOperation;
        smoke["failed_tab"] = tab;
        smoke["failed_native_mode"] = databaseNativeMode;
        smoke["failed_ui_epoch"] = epoch;
        smoke["failed_row_count"] = rowData.size();
        smoke["failed_field_count"] = fieldData.size();
        smoke["failed_ui_generation"] = generation;
        smoke["failed_ui_revision"] = revision;
        if (smokeMaterial)
            smoke["failed_material"] = materialView;
        smoke["failed_selector"] = selector;
        smoke["failed_metadata"] = metadata;
        if (editor)
        {
            smoke["failed_editor"] = J::object({{"operation", editor->operation},
                                                {"arguments", editor->arguments},
                                                {"generation", editor->generation},
                                                {"revision", editor->revision},
                                                {"pending", editor->pending}});
        }
        SmokeComplete();
    }
    else
        MessageBoxW(window, Monstey_Wide(message).c_str(), L"Monstey Workbench", MB_OK | MB_ICONERROR);
}

void Monstey_App::ShowReport(const J &value)
{
    SetWindowTextW(details, Monstey_Wide(value.dump(2)).c_str());
}

} // namespace Monstey::Monstey_AppInternal
