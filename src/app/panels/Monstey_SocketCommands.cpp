#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::SocketCommand(int command)
{
    if (tab != 4)
        return;
    if (editor && editor->window)
    {
        SetForegroundWindow(editor->window);
        return;
    }
    if (command == Monstey_Sockets || command == Monstey_Extras)
    {
        socketMode = command == Monstey_Extras ? true : !socketMode;
        animatedExtraMode = command == Monstey_Extras;
        socketRig.clear();
        socketView = J::object();
        category.reset();
        socketOffset = 0;
        TabChanged();
        return;
    }
    if (!socketMode)
        return;
    if (command == Monstey_Open)
    {
        auto path =
            Monstey_FileDialog(window, false, Monstey_SourceFilter(0), L"gamedb", L"Open shared database project");
        if (!path)
            return;
        if (dirty[0] && smokeReport.empty() &&
            MessageBoxW(window, L"Discard current database project edits?", L"Open database",
                        MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
            return;
        ++epoch;
        category.reset();
        socketModel.clear();
        socketOffset = 0;
        auto args = J::object({{"path", Monstey_JsonPath(*path)}});
        if (!schema.empty())
            args["schema"] = Monstey_JsonPath(schema);
        Queue(
            "open_database", args,
            [this](const J &reply)
            {
                if (!reply.at("ok").as_bool())
                {
                    Error(reply.at("error").at("message").as_string());
                    return;
                }
                UpdateState(reply.at("result"), 0);
                LoadSocketModels();
            },
            false);
        return;
    }
    if (command == Monstey_Search || command == Monstey_Previous || command == Monstey_Next)
    {
        if (command == Monstey_Search)
            offset = 0;
        if (command == Monstey_Previous)
            offset = offset >= 250 ? offset - 250 : 0;
        if (command == Monstey_Next && offset + 250 < total)
            offset += 250;
        LoadSocketModels();
        return;
    }
    if (command == Monstey_Undo || command == Monstey_Redo)
    {
        Queue(command == Monstey_Undo ? "database_undo" : "database_redo", J::object(),
              [this](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  UpdateState(reply.at("result"), 0);
                  category.reset();
                  LoadSocketModels();
              });
        return;
    }
    if (command == Monstey_Save || command == Monstey_Build)
    {
        auto output = Monstey_FileDialog(
            window, true, command == Monstey_Save ? L"Project (*.json)\0*.json\0\0" : Monstey_SourceFilter(0),
            command == Monstey_Save ? L"json" : L"gamedb", L"Write a NEW database project or candidate");
        if (output)
            Queue(command == Monstey_Save ? "database_save_project" : "database_build",
                  J::object({{command == Monstey_Save ? "path" : "output", Monstey_JsonPath(*output)}}),
                  [this](const J &reply)
                  {
                      if (!reply.at("ok").as_bool())
                      {
                          Error(reply.at("error").at("message").as_string());
                          return;
                      }
                      ShowReport(reply.at("result"));
                  });
        return;
    }
    if (animatedExtraMode && command == Monstey_SocketRig)
    {
        ExtraCommand(command);
        return;
    }
    if (command == Monstey_SocketRig)
    {
        auto menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, Monstey_SocketRig, L"Select optional SKEL and membership policy...");
        AppendMenuW(menu, MF_STRING, Monstey_SocketRigClear, L"Clear optional rig");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (socketOffset ? 0 : MF_GRAYED), Monstey_SocketPrev, L"Previous socket page");
        AppendMenuW(menu, MF_STRING | (socketOffset + fieldData.size() < socketTotal ? 0 : MF_GRAYED),
                    Monstey_SocketNext, L"Next socket page");
        RECT rect{};
        GetWindowRect(GetDlgItem(window, Monstey_SocketRig), &rect);
        auto chosen = TrackPopupMenu(menu, TPM_RETURNCMD, rect.left, rect.bottom, 0, window, nullptr);
        DestroyMenu(menu);
        if (chosen == Monstey_SocketRig)
        {
            auto rig = Monstey_FileDialog(window, false, L"SoW skeleton (*.skel)\0*.skel\0\0", L"skel",
                                          L"Optional supplied skin or complete attachment rig");
            if (!rig)
                return;
            StructureForm(L"Rig membership validation",
                          J::array({J::object({{"name", "require_parent_node"},
                                               {"label", "Require parent in this rig (enable "
                                                         "only for a complete attachment rig)"},
                                               {"kind", "bool"},
                                               {"value", false}})}),
                          [this, path = *rig](const J &value)
                          {
                              socketRig = path;
                              socketView["rig_sha256"] = Monstey_HashSha256(path);
                              socketView["require_parent_node"] = value.at("require_parent_node");
                              LoadSocketOverrides();
                          });
        }
        else if (chosen)
            SocketCommand(static_cast<int>(chosen));
        return;
    }
    if (command == Monstey_SocketRigClear)
    {
        socketRig.clear();
        socketView["rig_sha256"] = "";
        socketView["require_parent_node"] = false;
        LoadSocketOverrides();
        return;
    }
    if (command == Monstey_SocketPrev || command == Monstey_SocketNext)
    {
        if (command == Monstey_SocketPrev)
            socketOffset = socketOffset >= 250 ? socketOffset - 250 : 0;
        else if (socketOffset + 250 < socketTotal)
            socketOffset += 250;
        LoadSocketOverrides();
        return;
    }
    if (animatedExtraMode && (command == Monstey_SocketRig || command == Monstey_ExtraUsage))
    {
        ExtraCommand(command);
        return;
    }
    if (command != Monstey_SocketCreate && command != Monstey_SocketEdit && command != Monstey_SocketCopy &&
        command != Monstey_SocketRemove)
        return;
    Monstey_Require(!socketModel.empty(), "Select a Model database record first");
    Monstey_Require(!socketView.value("ui_loading", true).as_bool() &&
                        socketView.value("ui_model", "").as_string() == socketModel,
                    "Wait for this model's socket slots to load");
    if (animatedExtraMode)
    {
        ExtraCommand(command);
        return;
    }
    const auto policies =
        J::array({J::object({{"label", "Independent: clone only this slot"}, {"value", "independent"}}),
                  J::object({{"label", "Shared: affect every linked model"}, {"value", "shared"}})});
    auto args = J::object({{"model", socketModel},
                           {"expected_generation", socketView.at("ui_generation")},
                           {"expected_revision", socketView.at("ui_revision")}});
    auto accepted = [this](const J &result) { SocketAccepted(result); };
    if (command == Monstey_SocketCreate)
    {
        StructureForm(L"Create native model socket",
                      J::array({Monstey_StructureText("socket_name", "Socket attachment label", "NewSocket"),
                                Monstey_StructureText("parent_node", "Parent attachment / rig node", "Root")}),
                      [this, args](const J &values) mutable
                      {
                          for (const auto &[key, value] : values.as_object())
                              args[key] = value;
                          SocketTransform("model_socket_create", args, J::array({0, 0, 0}), J::array({0, 0, 0, 1}));
                      });
        return;
    }
    auto selected = ListView_GetNextItem(fields, -1, LVNI_SELECTED);
    Monstey_Require(selected >= 0 && static_cast<std::size_t>(selected) < fieldData.size(),
                    "Select a socket slot first");
    const auto row = fieldData[static_cast<std::size_t>(selected)];
    args["index"] = row.at("index");
    if (command == Monstey_SocketRemove)
    {
        StructureForm(L"Unlink selected socket slot",
                      J::array({J::object({{"name", "delete_orphan"},
                                           {"label", "Delete physical override only when no "
                                                     "model still references it"},
                                           {"kind", "bool"},
                                           {"value", false}})}),
                      accepted, "model_socket_remove", args);
        return;
    }
    Monstey_Require(!row.at("reference").is_null(), "The selected slot is empty");
    if (command == Monstey_SocketEdit)
    {
        StructureForm(L"Edit native socket",
                      J::array({Monstey_StructureText("socket_name", "Socket attachment label",
                                                      row.at("socket_name").as_string().c_str()),
                                Monstey_StructureText("parent_node", "Parent attachment / rig node",
                                                      row.at("parent_node").as_string().c_str()),
                                Monstey_StructureChoice("policy", "Ownership policy", policies, "independent")}),
                      [this, args, row](const J &values) mutable
                      {
                          for (const auto &[key, value] : values.as_object())
                              args[key] = value;
                          SocketTransform("model_socket_edit", args, row.at("position_cm"), row.at("rotation_xyzw"));
                      });
        return;
    }
    if (command == Monstey_SocketCopy)
    {
        auto copy = J::object({{"source_model", socketModel},
                               {"source_index", row.at("index")},
                               {"expected_generation", socketView.at("ui_generation")},
                               {"expected_revision", socketView.at("ui_revision")}});
        StructureForm(L"Copy socket to another model",
                      J::array({Monstey_StructureText("target_model", "Target Model record name or reference",
                                                      socketModel.c_str()),
                                Monstey_StructureChoice("policy", "Ownership policy", policies, "independent")}),
                      accepted, "model_socket_copy_to_model", copy);
    }
}

} // namespace Monstey::Monstey_AppInternal
