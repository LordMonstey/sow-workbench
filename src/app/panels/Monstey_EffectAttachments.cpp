#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::UpdateFxAttachmentButton()
{
    bool ready =
        tab == 3 && !sourceNames[3].empty() && metadata.value("category", "").as_string().starts_with("ClientFX.") &&
        metadata.value("reference", "") == J(selector) && metadata.value("generation", 0).as_uint() == generation &&
        metadata.value("revision", 0).as_uint() == revision && !metadata.value("pending_delete", false).as_bool() &&
        metadata.contains("fields");
    if (ready)
    {
        for (const auto &[hash, type] : {std::pair{"0x23e7c5b3", 5u}, std::pair{"0x1fcd6c38", 4u}})
        {
            const auto &nativeFields = metadata.at("fields").as_array();
            ready = ready && std::any_of(nativeFields.begin(), nativeFields.end(),
                                         [&](const J &field)
                                         {
                                             return field.at("hash") == J(hash) &&
                                                    field.at("native_type").as_uint() == type &&
                                                    !field.at("array").as_bool() && field.at("count").as_uint() == 1;
                                         });
        }
    }
    EnableWindow(GetDlgItem(window, Monstey_FxAttachment), ready);
}

void Monstey_App::FxAttachmentCommand()
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
    Monstey_Require(!metadata.empty() && metadata.contains("reference") &&
                        !metadata.value("pending_delete", false).as_bool(),
                    "Select a native effect component and wait for its fields");
    const auto ref = metadata.at("reference").as_string(), selected = selector;
    const auto selectedGeneration = generation, selectedRevision = revision;
    const auto request = ++fxAttachmentRequest;
    Queue("fx_attachment_inspect", J::object({{"record", ref}}),
          [this, ref, selected, selectedGeneration, selectedRevision, request](const J &reply)
          {
              if (tab != 3 || selector != selected || generation != selectedGeneration ||
                  revision != selectedRevision || request != fxAttachmentRequest ||
                  metadata.value("reference", "") != J(ref) || reply.at("generation").as_uint() != selectedGeneration ||
                  reply.at("revision").as_uint() != selectedRevision)
                  return;
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              fxAttachmentView = reply.at("result");
              auto choices = J::array();
              for (const auto &mode : fxAttachmentView.at("modes").as_array())
                  choices.as_array().push_back(J::object({{"label", mode.at("token")}, {"value", mode.at("token")}}));
              auto name = fxAttachmentView.at("attach_name");
              if (name.is_null())
                  name = "";
              auto nameField = Monstey_StructureText(
                  "attach_name", "Owner node or socket name (empty uses the context frame)", "", false, true);
              nameField["value"] = name;
              auto item = std::make_shared<Monstey_Editor>();
              item->app = this;
              item->title = L"Native owner attachment";
              item->field = J::object({{"name", "Native owner attachment"},
                                       {"native_type", 0},
                                       {"value", J::object()},
                                       {"editable", true},
                                       {"review_text", "This attaches to the effect's external owner, not another FX "
                                                       "key. "
                                                       "A missing name or unavailable owner falls back to the context "
                                                       "frame. "
                                                       "Changing to an unnamed mode preserves the dormant name."}});
              item->formFields = J::array({Monstey_StructureChoice("update_pos", "Native follow mode", choices,
                                                                   fxAttachmentView.at("effective_update_pos")),
                                           nameField});
              item->operation = "fx_attachment_set";
              item->arguments = J::object({{"record", ref},
                                           {"expected_generation", reply.at("generation")},
                                           {"expected_revision", reply.at("revision")},
                                           {"expected_database_sha256", fxAttachmentView.at("source_sha256")},
                                           {"expected_schema_sha256", fxAttachmentView.at("schema_sha256")}});
              item->generation = selectedGeneration;
              item->revision = selectedRevision;
              item->success = [this](const J &result)
              {
                  fxAttachmentView = result.at("result");
                  FxAccepted(result);
              };
              ShowReport(fxAttachmentView);
              ShowEditor(item);
          });
}

} // namespace Monstey::Monstey_AppInternal
