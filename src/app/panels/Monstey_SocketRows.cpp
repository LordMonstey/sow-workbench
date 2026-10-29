#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::LoadSocketModels()
{
    if (tab != 4 || !socketMode)
        return;
    UpdateButtons();
    if (sourceNames[0].empty())
    {
        ++socketRequest;
        socketModel.clear();
        socketTotal = 0;
        socketView["ui_loading"] = true;
        rowData.clear();
        fieldData.clear();
        ListView_DeleteAllItems(records);
        ListView_DeleteAllItems(fields);
        SetWindowTextW(details, L"Open your game database here or in the Database tab. "
                                L"Sockets use the same database project and undo history.");
        return;
    }
    if (!category)
    {
        Queue("categories", J::object(),
              [this](const J &reply)
              {
                  if (tab != 4 || !socketMode)
                      return;
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  for (const auto &row : reply.at("result").at("rows").as_array())
                      if (row.at("name").as_string() == (animatedExtraMode ? "Character.Models" : "Model"))
                      {
                          category = static_cast<std::uint32_t>(row.at("index").as_uint());
                          break;
                      }
                  Monstey_Require(category.has_value(), "This database has no Model category");
                  LoadSocketModels();
              });
        return;
    }
    Queue("records",
          J::object({{"category_index", *category},
                     {"query", Monstey_Utf8(Monstey_WindowText(query))},
                     {"offset", offset},
                     {"limit", 250}}),
          [this](const J &reply)
          {
              if (tab != 4 || !socketMode)
                  return;
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              populating = true;
              while (ListView_DeleteColumn(records, 0))
              {
              }
              while (ListView_DeleteColumn(fields, 0))
              {
              }
              Monstey_ListColumn(records, 0, animatedExtraMode ? L"Character.Models owner" : L"Model database record",
                                 300);
              Monstey_ListColumn(records, 1, L"Reference", 120);
              Monstey_ListColumn(
                  fields, 0, animatedExtraMode ? L"Animated extra / attachment node" : L"Socket / parent node", 230);
              Monstey_ListColumn(fields, 1, animatedExtraMode ? L"Model reference" : L"Reference", 110);
              Monstey_ListColumn(
                  fields, 2, animatedExtraMode ? L"Animation / start state" : L"Position (cm) / rotation XYZW", 370);
              rowData = reply.at("result").at("rows").as_array();
              total = reply.at("result").at("total").as_uint();
              ListView_DeleteAllItems(records);
              ListView_DeleteAllItems(fields);
              fieldData.clear();
              int chosen = 0;
              for (std::size_t i = 0; i < rowData.size(); ++i)
              {
                  Monstey_ListCell(records, static_cast<int>(i), 0, Monstey_Display(rowData[i].at("name")));
                  Monstey_ListCell(records, static_cast<int>(i), 1, Monstey_Display(rowData[i].at("reference")));
                  if (rowData[i].at("reference").as_string() == socketModel)
                      chosen = static_cast<int>(i);
              }
              if (!rowData.empty())
                  Monstey_ListSelect(records, chosen);
              populating = false;
              SetWindowTextW(pageLabel, (L"Models " + std::to_wstring(rowData.empty() ? 0 : offset + 1) + L" - " +
                                         std::to_wstring(offset + rowData.size()) + L" / " + std::to_wstring(total) +
                                         L". Double-click a socket to edit. Rig > Socket page "
                                         L"changes the right-hand page.")
                                            .c_str());
              EnableWindow(GetDlgItem(window, Monstey_Previous), offset > 0);
              EnableWindow(GetDlgItem(window, Monstey_Next), offset + rowData.size() < total);
              if (!rowData.empty())
                  ReadRecord(chosen);
              else
              {
                  ++socketRequest;
                  socketModel.clear();
                  socketTotal = 0;
                  socketView["ui_loading"] = true;
                  SetWindowTextW(details, L"No matching Model records.");
              }
          });
}

void Monstey_App::LoadSocketOverrides()
{
    if (socketModel.empty() || tab != 4 || !socketMode)
        return;
    auto args = J::object({{animatedExtraMode ? "owner" : "model", socketModel},
                           {"offset", socketOffset},
                           {"limit", 250},
                           {"include_usage", !animatedExtraMode}});
    if (!socketRig.empty())
    {
        args["skeleton"] = Monstey_JsonPath(socketRig);
        args["expected_skeleton_sha256"] = socketView.value("rig_sha256", "");
        args[animatedExtraMode ? "require_attach_node" : "require_parent_node"] =
            socketView.value("require_parent_node", false);
    }
    auto requested = socketModel;
    auto requestedOffset = socketOffset;
    const auto request = ++socketRequest;
    auto rigHash = socketView.value("rig_sha256", "");
    auto requireParent = socketView.value("require_parent_node", false);
    fieldData.clear();
    ListView_DeleteAllItems(fields);
    socketView = J::object({{"rig_sha256", rigHash},
                            {"require_parent_node", requireParent},
                            {"ui_loading", true},
                            {"ui_model", requested}});
    SetWindowTextW(details, L"Loading this model's native socket slots...");
    Queue(animatedExtraMode ? "animated_extras_list" : "model_sockets_list", args,
          [this, requested, requestedOffset, request](const J &reply)
          {
              if (tab != 4 || !socketMode || socketModel != requested || socketOffset != requestedOffset ||
                  socketRequest != request)
                  return;
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              auto preserved = socketView;
              socketView = reply.at("result");
              socketView["ui_loading"] = false;
              socketView["ui_model"] = requested;
              socketView["ui_generation"] = reply.at("generation");
              socketView["ui_revision"] = reply.at("revision");
              socketView["rig_sha256"] = preserved.value("rig_sha256", "");
              socketView["require_parent_node"] = preserved.value("require_parent_node", false);
              fieldData = socketView.at("rows").as_array();
              socketTotal = socketView.at("total").as_uint();
              populating = true;
              ListView_DeleteAllItems(fields);
              for (std::size_t i = 0; i < fieldData.size(); ++i)
              {
                  const auto &row = fieldData[i];
                  const auto empty = row.at("reference").is_null();
                  if (animatedExtraMode)
                  {
                      Monstey_ListCell(fields, static_cast<int>(i), 0,
                                       empty ? L"(empty slot)" : Monstey_Display(row.at("owner_attach_node")));
                      Monstey_ListCell(fields, static_cast<int>(i), 1, empty ? L"" : Monstey_Display(row.at("model")));
                      Monstey_ListCell(
                          fields, static_cast<int>(i), 2,
                          empty ? L""
                                : Monstey_Display(row.at("base_animation")) +
                                      (row.at("start_attached").as_bool() ? L" / attached" : L" / detached") +
                                      (row.at("start_visible").as_bool() ? L" / visible" : L" / hidden"));
                  }
                  else
                  {
                      Monstey_ListCell(fields, static_cast<int>(i), 0,
                                       empty ? L"(empty slot)"
                                             : Monstey_Display(row.at("socket_name")) + L" / " +
                                                   Monstey_Display(row.at("parent_node")));
                      Monstey_ListCell(fields, static_cast<int>(i), 1, Monstey_Display(row.at("reference")));
                      Monstey_ListCell(fields, static_cast<int>(i), 2,
                                       empty ? L""
                                             : Monstey_Display(row.at("position_cm")) + L" / " +
                                                   Monstey_Display(row.at("rotation_xyzw")));
                  }
              }
              if (!fieldData.empty())
                  Monstey_ListSelect(fields, 0);
              populating = false;
              ShowReport(socketView);
              SetWindowTextW(pageLabel,
                             (L"Models " + std::to_wstring(offset + 1) + L" - " +
                              std::to_wstring(offset + rowData.size()) + L" / " + std::to_wstring(total) +
                              (animatedExtraMode ? L"; animated extras " : L"; sockets ") +
                              std::to_wstring(fieldData.empty() ? 0 : socketOffset + 1) + L" - " +
                              std::to_wstring(socketOffset + fieldData.size()) + L" / " + std::to_wstring(socketTotal) +
                              (animatedExtraMode ? L". Empty animation and hidden start are supported."
                                                 : L". Positions: cm; quaternion: XYZW."))
                                 .c_str());
              SmokeStep();
          });
}

void Monstey_App::SocketAccepted(const J &result)
{
    UpdateState(result, 0);
    UpdateButtons();
    ShowReport(result);
    LoadSocketOverrides();
    if (inspector && inspector->window && inspector->metadata.value("view_tab", 0).as_int() == 0)
    {
        auto ref = inspector->metadata.at("selector").as_string();
        Queue("database_metadata", J::object({{"record", ref}}),
              [this, ref](const J &reply)
              {
                  if (!reply.at("ok").as_bool() || !inspector || !inspector->window ||
                      inspector->metadata.at("selector").as_string() != ref)
                      return;
                  inspector->metadata = reply.at("result");
                  inspector->metadata["view_tab"] = 0;
                  inspector->metadata["generation"] = reply.at("generation");
                  inspector->metadata["revision"] = reply.at("revision");
                  inspector->Populate();
              });
    }
}

void Monstey_App::SocketTransform(std::string operation, J args, const J &position, const J &rotation)
{
    auto item = std::make_shared<Monstey_Editor>();
    item->app = this;
    item->title = L"Socket position in centimetres";
    item->field = J::object(
        {{"name", "Position (cm)"}, {"native_type", 8}, {"array", false}, {"value", position}, {"editable", true}});
    item->success = [this, operation = std::move(operation), args = std::move(args), rotation](const J &value) mutable
    {
        args["position_cm"] = value;
        auto next = std::make_shared<Monstey_Editor>();
        next->app = this;
        next->title = L"Socket rotation quaternion XYZW";
        next->field = J::object({{"name", "Quaternion XYZW (unit length; retained exactly)"},
                                 {"native_type", 9},
                                 {"array", false},
                                 {"value", rotation},
                                 {"editable", true}});
        next->success = [this, operation, args](const J &q) mutable
        {
            args["rotation_xyzw"] = q;
            if (!socketRig.empty())
            {
                args["skeleton"] = Monstey_JsonPath(socketRig);
                args["expected_skeleton_sha256"] = socketView.value("rig_sha256", "");
                args["require_parent_node"] = socketView.value("require_parent_node", false);
            }
            Queue(operation, args,
                  [this](const J &reply)
                  {
                      if (!reply.at("ok").as_bool())
                      {
                          Error(reply.at("error").at("message").as_string());
                          return;
                      }
                      SocketAccepted(reply.at("result"));
                  });
        };
        ShowEditor(next);
    };
    ShowEditor(item);
}

} // namespace Monstey::Monstey_AppInternal
