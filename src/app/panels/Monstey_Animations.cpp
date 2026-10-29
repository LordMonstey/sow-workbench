#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::AnimationAccept(const J &result)
{
    animationLastReport = result;
    if (result.contains("dirty"))
    {
        animationView = result;
        if (result.contains("source") && result.at("source").is_string())
            animationSource = Monstey_Wide(result.at("source").as_string());
        UpdateState(result, 4);
    }
    if (tab == 4 && animationMode)
        AnimationLoadRows();
}

void Monstey_App::AnimationSubmit(std::string operation, J args, std::string selected)
{
    if (!args.contains("expected_generation"))
        args["expected_generation"] = generation;
    if (!args.contains("expected_revision"))
        args["expected_revision"] = revision;
    Queue(operation, args,
          [this, selected](const J &reply)
          {
              if (!reply.at("ok").as_bool())
              {
                  if (smokeAnimation && smokeStage == 609 && smokeArguments.value("raw_event_bytes", false).as_bool())
                  {
                      // The typed form has reached normal backend validation. This explicit
                      // smoke mode observes the expected rejection; it does not waive it.
                      Monstey_Require(reply.at("error").at("code") == J("anix") &&
                                          reply.at("error").at("message") ==
                                              J("Event text_bytes_hex contains a non-hex digit") &&
                                          reply.at("generation") == smoke.at("raw_reject_generation") &&
                                          reply.at("revision") == smoke.at("raw_reject_revision"),
                                      "Malformed raw hex did not produce the expected transactional rejection");
                      smoke["malformed_raw_hex_error"] = reply.at("error");
                      smokeStage = 610;
                      AnimationLoadRows();
                      return;
                  }
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              if (!selected.empty())
              {
                  animationSelected = selected;
                  offset = 0;
                  SetWindowTextW(query, L"");
              }
              AnimationAccept(reply.at("result"));
          });
}

void Monstey_App::AnimationLoadRows()
{
    if (tab != 4 || !animationMode || animationView.empty() || animationOpening)
        return;
    const auto request = ++animationRowsRequest;
    ++animationClipRequest;
    animationClip = J::object();
    fieldData.clear();
    rowData.clear();
    populating = true;
    ListView_DeleteAllItems(records);
    ListView_DeleteAllItems(fields);
    populating = false;
    UpdateButtons();
    const auto queryText = Monstey_Utf8(Monstey_WindowText(query));
    const auto page = offset;
    Queue("animation_describe", J::object(),
          [this, request, queryText, page](const J &description)
          {
              if (tab != 4 || !animationMode || request != animationRowsRequest)
                  return;
              if (!description.at("ok").as_bool())
              {
                  Error(description.at("error").at("message").as_string());
                  return;
              }
              animationView = description.at("result");
              animationSource = Monstey_Wide(animationView.at("source").as_string());
              UpdateState(animationView, 4);
              const auto sourceGeneration = description.at("generation").as_uint();
              Queue("animation_clips",
                    J::object({{"query", queryText},
                               {"offset", page},
                               {"limit", 250},
                               {"expected_generation", sourceGeneration},
                               {"expected_revision", description.at("revision")}}),
                    [this, request, queryText, page, sourceGeneration](const J &reply)
                    {
                        if (tab != 4 || !animationMode || request != animationRowsRequest ||
                            sourceGeneration != reply.at("generation").as_uint() || offset != page ||
                            queryText != Monstey_Utf8(Monstey_WindowText(query)))
                            return;
                        if (!reply.at("ok").as_bool())
                        {
                            Error(reply.at("error").at("message").as_string());
                            return;
                        }
                        rowData = reply.at("result").at("rows").as_array();
                        total = reply.at("result").at("total").as_uint();
                        populating = true;
                        ListView_DeleteAllItems(records);
                        int chosen = 0;
                        for (std::size_t i = 0; i < rowData.size(); ++i)
                        {
                            const auto &row = rowData[i];
                            Monstey_ListCell(records, static_cast<int>(i), 0, Monstey_Display(row.at("name")));
                            Monstey_ListCell(records, static_cast<int>(i), 1,
                                             Monstey_Display(row.at("duration_ticks")));
                            Monstey_ListCell(records, static_cast<int>(i), 2, Monstey_Display(row.at("codecs")));
                            if (row.at("name") == J(animationSelected))
                                chosen = static_cast<int>(i);
                        }
                        if (!rowData.empty())
                            Monstey_ListSelect(records, chosen);
                        else
                            animationSelected.clear();
                        populating = false;
                        SetWindowTextW(pageLabel,
                                       (L"Clips " + std::to_wstring(rowData.empty() ? 0 : offset + 1) + L" - " +
                                        std::to_wstring(offset + rowData.size()) + L" of " + std::to_wstring(total) +
                                        L". Native milliseconds; parent-local centimetres / XYZW. "
                                        L"No 3D playback.")
                                           .c_str());
                        EnableWindow(GetDlgItem(window, Monstey_Previous), offset > 0);
                        EnableWindow(GetDlgItem(window, Monstey_Next), offset + rowData.size() < total);
                        if (!rowData.empty())
                            AnimationReadClip(chosen);
                        else
                        {
                            UpdateButtons();
                            ShowReport(J::object({{"library", animationView}, {"last_action", animationLastReport}}));
                        }
                    });
          });
}

void Monstey_App::AnimationReadClip(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= rowData.size())
        return;
    animationSelected = rowData[static_cast<std::size_t>(index)].at("name").as_string();
    const auto name = animationSelected;
    const auto hash = rowData[static_cast<std::size_t>(index)].at("hash");
    const auto request = ++animationClipRequest;
    const auto sourceGeneration = generation;
    animationClip = J::object();
    fieldData.clear();
    ListView_DeleteAllItems(fields);
    UpdateButtons();
    Queue("animation_clip",
          J::object({{"clip", name}, {"expected_generation", sourceGeneration}, {"expected_revision", revision}}),
          [this, name, hash, request, sourceGeneration](const J &reply)
          {
              if (tab != 4 || !animationMode || request != animationClipRequest || name != animationSelected ||
                  sourceGeneration != reply.at("generation").as_uint())
                  return;
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              Monstey_Require(reply.at("result").at("hash") == hash,
                              "Animation clip identity changed while loading metadata");
              animationClip = reply.at("result");
              animationClip["ui_generation"] = reply.at("generation");
              animationClip["ui_revision"] = reply.at("revision");
              AnimationPopulate();
              UpdateButtons();
              if (!smokeReport.empty())
                  SmokeStep();
          });
}

void Monstey_App::AnimationPopulate()
{
    fieldData.clear();
    populating = true;
    ListView_DeleteAllItems(fields);
    if (!animationClip.empty())
    {
        if (animationEventMode && animationClip.contains("events"))
        {
            fieldData = animationClip.at("events").as_array();
            for (std::size_t i = 0; i < fieldData.size(); ++i)
            {
                Monstey_ListCell(fields, static_cast<int>(i), 0, Monstey_Display(fieldData[i].at("tick")));
                Monstey_ListCell(fields, static_cast<int>(i), 1, Monstey_Display(fieldData[i].at("text")));
                Monstey_ListCell(fields, static_cast<int>(i), 2, L"Native Event");
            }
        }
        else
        {
            for (const auto key : {"name", "hash", "duration_ticks", "flags", "tracks", "codecs", "descriptor_extras",
                                   "segments", "event_channel_index", "empty_native_clip", "sample_supported",
                                   "unique_track_channels", "repeated_channel_bindings"})
            {
                if (!animationClip.contains(key))
                    continue;
                const auto row = static_cast<int>(fieldData.size());
                fieldData.push_back(J::object({{"name", key}, {"value", animationClip.at(key)}}));
                Monstey_ListCell(fields, row, 0, Monstey_Wide(key));
                Monstey_ListCell(fields, row, 1, Monstey_Display(animationClip.at(key)));
                Monstey_ListCell(fields, row, 2, L"Read only");
            }
        }
    }
    populating = false;
    ShowReport(
        J::object({{"library", animationView},
                   {"clip", animationClip},
                   {"last_action", animationLastReport},
                   {"sampling_space", "native parent-local, Y-up centimetres, XYZW; milliseconds"},
                   {"opaque_codec_policy", "Structural edits preserve opaque data; "
                                           "unsupported pose/timing edits are rejected"},
                   {"auxiliary_codec_policy", "Qualified CAF4 curves and static USFT/USBT tracks support native "
                                              "authoring; unknown variants, varying static tracks and opaque timing "
                                              "remain explicit"},
                   {"runtime_playback_verified", false}}));
}

} // namespace Monstey::Monstey_AppInternal
