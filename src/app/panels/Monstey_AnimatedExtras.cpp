#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::ExtraValues(std::string operation, J args, const J &row)
{
    auto value = [&](const char *key, const J &fallback) { return row.value(key, fallback); };
    auto refText = [&](const char *key)
    {
        auto v = value(key, J{});
        return v.is_null() ? std::string{} : v.as_string();
    };
    auto rows = J::array({Monstey_StructureText("model", "Model record name / reference", refText("model").c_str()),
                          Monstey_StructureText("base_animation", "Base animation (empty is supported)",
                                                value("base_animation", "").as_string().c_str()),
                          Monstey_StructureText("owner_attach_node", "Owner attachment node",
                                                value("owner_attach_node", "").as_string().c_str()),
                          J::object({{"name", "start_attached"},
                                     {"label", "Start attached"},
                                     {"kind", "bool"},
                                     {"value", value("start_attached", true)}}),
                          J::object({{"name", "start_visible"},
                                     {"label", "Start visible (unchecked keeps an initially hidden extra)"},
                                     {"kind", "bool"},
                                     {"value", value("start_visible", true)}})});
    for (auto &field : rows.as_array())
        if (field.at("name").as_string() == "base_animation")
            field["allow_empty"] = true;
    if (operation == "animated_extra_create")
        rows.as_array().insert(
            rows.as_array().begin(),
            Monstey_StructureText("name", "Optional physical record name (blank = anonymous)", "", true));
    else
        rows.as_array().push_back(Monstey_StructureChoice(
            "policy", "Ownership policy",
            J::array({J::object({{"label", "Independent: clone this slot only"}, {"value", "independent"}}),
                      J::object({{"label", "Shared: edit every referring owner"}, {"value", "shared"}})}),
            "independent"));
    StructureForm(
        L"Native animated extra", rows,
        [this, operation, args, row](const J &values) mutable
        {
            for (const auto &pair : values.as_object())
                args[pair.first] = pair.second;

            auto text = [&](const char *key)
            {
                auto value = row.value(key, J{});
                return value.is_null() ? std::string{} : value.as_string();
            };
            StructureForm(
                L"Animated extra native FX and dissolve references",
                J::array({Monstey_StructureText("persistent_fx", "Client.ClientFXSequence record (blank = None)",
                                                text("persistent_fx").c_str(), false, true),
                          Monstey_StructureText("dissolve_pitch_range", "BaseTypes.FloatRange record (blank = None)",
                                                text("dissolve_pitch_range").c_str(), false, true)}),
                [this, operation, args, row](const J &refs) mutable
                {
                    for (const auto &pair : refs.as_object())
                        args[pair.first] = pair.second.as_string().empty() ? J{} : pair.second;
                    auto item = std::make_shared<Monstey_Editor>();
                    item->app = this;
                    item->title = L"Animated extra dissolve parameter range";
                    item->field = J::object({{"name", "DissolveParamRange (two native Float32 values)"},
                                             {"native_type", 7},
                                             {"array", false},
                                             {"value", row.value("dissolve_param_range", J::array({0.0, 1.0}))},
                                             {"editable", true}});
                    item->operation = operation;
                    item->arguments = args;
                    item->valueKey = "dissolve_param_range";
                    item->generation = args.at("expected_generation").as_uint();
                    item->revision = args.at("expected_revision").as_uint();
                    item->success = [this](const J &result) { SocketAccepted(result); };
                    ShowEditor(item);
                });
        });
}

void Monstey_App::ExtraCommand(int command)
{
    if (tab != 4 || !socketMode || !animatedExtraMode)
        return;
    if (editor && editor->window)
    {
        SetForegroundWindow(editor->window);
        return;
    }
    if (command == Monstey_SocketRig)
    {
        auto menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, Monstey_ExtraUsage, L"Selected extra: native fields and incoming references");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (socketOffset ? 0 : MF_GRAYED), Monstey_SocketPrev, L"Previous extra page");
        AppendMenuW(menu, MF_STRING | (socketOffset + fieldData.size() < socketTotal ? 0 : MF_GRAYED),
                    Monstey_SocketNext, L"Next extra page");
        RECT rect{};
        GetWindowRect(GetDlgItem(window, Monstey_SocketRig), &rect);
        auto chosen = TrackPopupMenu(menu, TPM_RETURNCMD, rect.left, rect.bottom, 0, window, nullptr);
        DestroyMenu(menu);
        if (chosen)
            SocketCommand(static_cast<int>(chosen));
        return;
    }
    Monstey_Require(!socketModel.empty() && !socketView.value("ui_loading", true).as_bool() &&
                        socketView.value("ui_model", "").as_string() == socketModel,
                    "Select an owner and wait for its animated extra slots");
    auto args = J::object({{"owner", socketModel},
                           {"expected_generation", socketView.at("ui_generation")},
                           {"expected_revision", socketView.at("ui_revision")}});
    if (command == Monstey_SocketCreate)
    {
        ExtraValues("animated_extra_create", args, J::object());
        return;
    }
    auto selected = ListView_GetNextItem(fields, -1, LVNI_SELECTED);
    Monstey_Require(selected >= 0 && static_cast<std::size_t>(selected) < fieldData.size(),
                    "Select an animated extra slot");
    auto row = fieldData[static_cast<std::size_t>(selected)];
    args["index"] = row.at("index");
    args["expected_reference"] = row.at("reference");
    if (command == Monstey_ExtraUsage)
    {
        const auto request = socketRequest;
        const auto owner = socketModel;
        Queue("animated_extras_list",
              J::object({{"owner", owner}, {"offset", row.at("index")}, {"limit", 1}, {"include_usage", true}}),
              [this, request, owner](const J &reply)
              {
                  if (tab != 4 || !animatedExtraMode || socketRequest != request || socketModel != owner)
                      return;
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  socketView["selected_usage"] = reply.at("result");
                  ShowReport(reply.at("result"));
                  SmokeStep();
              });
        return;
    }
    auto accepted = [this](const J &result) { SocketAccepted(result); };
    if (command == Monstey_SocketRemove)
    {
        StructureForm(L"Unlink animated extra",
                      J::array({J::object({{"name", "delete_orphan"},
                                           {"label", "Delete the physical record only when "
                                                     "no owner still refers to it"},
                                           {"kind", "bool"},
                                           {"value", false}})}),
                      accepted, "animated_extra_remove", args);
        return;
    }
    Monstey_Require(!row.at("reference").is_null(), "Selected animated extra slot is empty");
    if (command == Monstey_SocketEdit)
    {
        ExtraValues("animated_extra_edit", args, row);
        return;
    }
    if (command == Monstey_SocketCopy)
    {
        auto copy = J::object({{"source_owner", socketModel},
                               {"source_index", row.at("index")},
                               {"expected_reference", row.at("reference")},
                               {"expected_generation", socketView.at("ui_generation")},
                               {"expected_revision", socketView.at("ui_revision")}});
        StructureForm(
            L"Copy animated extra to owner",
            J::array(
                {Monstey_StructureText("target_owner", "Character.Models record name / reference", socketModel.c_str()),
                 Monstey_StructureChoice(
                     "policy", "Ownership policy",
                     J::array({J::object({{"label", "Independent: clone source record"}, {"value", "independent"}}),
                               J::object({{"label", "Shared: link the same physical record"}, {"value", "shared"}})}),
                     "independent")}),
            accepted, "animated_extra_copy_to_owner", copy);
    }
}

} // namespace Monstey::Monstey_AppInternal
