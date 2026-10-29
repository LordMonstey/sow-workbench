#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SmokeTextureStep()
{
    if (!texturePreview)
        return;
    const auto view = texturePreview->Inspect();
    if (smokeStage == 1 || smokeStage == 5)
    {
        if (view.at("loading").as_bool())
            return;
        const bool opaque = smokeArguments.value("expect_opaque", false).as_bool();
        Monstey_Require(view.at("source_loaded").as_bool() && view.at("source_sha256") == smoke.at("source_sha256"),
                        "Texture GUI source identity differs");
        if (opaque)
            Monstey_Require(!view.at("ready").as_bool() && !view.at("description").at("decode_supported").as_bool(),
                            "Opaque texture acquired guessed pixels");
        else
            Monstey_Require(view.at("ready").as_bool() && view.at("error").as_string().empty(),
                            "Texture GUI decode failed: " + view.at("error").as_string());
        Monstey_Require(TabCtrl_GetItemCount(tabs) == 7, "Texture inspector added a top-level tab");
        if (smokeStage == 5)
        {
            smoke["reopened_texture_view"] = view;
            smoke["modeless_close_and_reopen"] = true;
            texturePreview->Close();
            SmokeComplete();
            return;
        }
        smoke["first_texture_view"] = view;
        Monstey_WriteNewFile(smokeDirectory / L"texture-window.bmp", texturePreview->Snapshot());
        TabCtrl_SetCurSel(tabs, 1);
        TabChanged();
        TabCtrl_SetCurSel(tabs, 4);
        TabChanged();
        Monstey_Require(texturePreview->Handle() != nullptr, "Tab switching closed texture inspector");
        Command(Monstey_Materials);
        Monstey_Require(materialMode, "Texture GUI parent material mode did not activate");
        smoke["independent_tab_and_material_context"] = true;
        if (opaque)
        {
            smoke["opaque_texture_reported_without_pixels"] = true;
            smokeStage = 3;
            return;
        }
        Monstey_Require(texturePreview->SavePng(smokeDirectory / L"texture-preview.png"),
                        "Texture GUI rejected pinned PNG export");
        smokeStage = 2;
        return;
    }
    if (smokeStage == 2)
    {
        if (view.at("save_pending").as_bool())
            return;
        Monstey_Require(view.at("error").as_string().empty() &&
                            std::filesystem::is_regular_file(smokeDirectory / L"texture-preview.png"),
                        "Texture GUI PNG export failed");
        smoke["texture_png"] = view.at("last_save");
        smokeStage = 3;
        return;
    }
    if (smokeStage == 3)
    {
        const auto before = Monstey_SmokeDialogCount;
        smoke["file_dialogs_before_reopen"] = before;
        PostMessageW(texturePreview->Handle(), WM_KEYDOWN, VK_ESCAPE, 0);
        smokeStage = 4;
        return;
    }
    if (smokeStage == 4)
    {
        if (texturePreview->Handle())
            return;
        Monstey_Require(IsWindow(window), "Texture Escape destroyed the parent window");
        Monstey_SmokeDialogPaths.push_back(initialDatabase);
        SendMessageW(GetDlgItem(window, Monstey_TexturePreview), BM_CLICK, 0, 0);
        Monstey_Require(materialMode && Monstey_SmokeDialogCount > smoke.at("file_dialogs_before_reopen").as_uint(),
                        "Material-mode texture action was not routed");
        smokeStage = 5;
    }
}

} // namespace Monstey::Monstey_AppInternal
