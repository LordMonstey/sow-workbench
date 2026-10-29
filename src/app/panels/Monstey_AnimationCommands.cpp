#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::AnimationCommand(int command)
{
    if (command == Monstey_Animations)
    {
        animationMode = !animationMode;
        if (animationMode)
        {
            socketMode = false;
            materialMode = false;
            animatedExtraMode = false;
            sourceNames[4] = animationSource;
        }
        TabChanged();
        return;
    }
    if (tab != 4 || !animationMode)
        return;
    if (command == Monstey_Search || command == Monstey_Previous || command == Monstey_Next)
    {
        if (command == Monstey_Search)
            offset = 0;
        else if (command == Monstey_Previous)
            offset = offset >= 250 ? offset - 250 : 0;
        else if (offset + 250 < total)
            offset += 250;
        AnimationLoadRows();
        return;
    }
    if (command == Monstey_Open || command == Monstey_New || command == Monstey_Load)
    {
        auto chosen =
            Monstey_FileDialog(window, false,
                               command == Monstey_Open ? L"Native animation (*.anix)\0*.anix\0All files\0*.*\0\0"
                                                       : L"Animation JSON (*.json)\0*.json\0\0",
                               command == Monstey_Open ? L"anix" : L"json",
                               command == Monstey_New ? L"Create native library from semantic scene"
                                                      : L"Open animation source or project");
        if (!chosen)
            return;
        if (dirty[4] && smokeReport.empty() &&
            MessageBoxW(window, L"Discard the current animation edits?", L"Open animation",
                        MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
            return;
        ++epoch;
        animationOpenEpoch = epoch;
        animationOpening = true;
        ++animationRowsRequest;
        ++animationClipRequest;
        animationClip = J::object();
        animationSelected.clear();
        rowData.clear();
        fieldData.clear();
        ListView_DeleteAllItems(records);
        ListView_DeleteAllItems(fields);
        offset = 0;
        SetWindowTextW(query, L"");
        UpdateButtons();
        Queue(
            command == Monstey_Open  ? "animation_open"
            : command == Monstey_New ? "animation_new"
                                     : "animation_load_project",
            J::object({{command == Monstey_New ? "scene_path" : "path", Monstey_JsonPath(*chosen)}}),
            [this](const J &reply)
            {
                if (!reply.at("ok").as_bool())
                {
                    Error(reply.at("error").at("message").as_string());
                    return;
                }
                animationOpening = false;
                AnimationAccept(reply.at("result"));
            },
            false);
        return;
    }
    if (command == Monstey_Save || command == Monstey_Build || command == Monstey_Extract ||
        command == Monstey_AnimationExportAll)
    {
        const bool build = command == Monstey_Build;
        const bool save = command == Monstey_Save;
        const auto output = Monstey_FileDialog(
            window, true, build ? L"Native animation (*.anix)\0*.anix\0\0" : L"Animation JSON (*.json)\0*.json\0\0",
            build ? L"anix" : L"json", L"Write a NEW animation output");
        if (!output)
            return;
        J args = J::object({{save ? "path" : "output", Monstey_JsonPath(*output)}});
        if (command == Monstey_Extract)
        {
            Monstey_Require(!animationClip.empty(), "Select a native clip first");
            args["clip"] = animationClip.at("name");
            args["expected_generation"] = animationClip.at("ui_generation");
            args["expected_revision"] = animationClip.at("ui_revision");
        }
        AnimationSubmit(save ? "animation_save_project" : build ? "animation_build" : "animation_export", args);
        return;
    }
    if (command == Monstey_AnimationChanges)
    {
        const auto historyCount = animationView.value("undo_steps", 0u).as_uint();
        StructureForm(L"Native animation history",
                      J::array({Monstey_FxNumber("offset", "First history step",
                                                 historyCount > 100 ? historyCount - 100 : 0, true),
                                Monstey_FxNumber("limit", "History steps (1-10000)", 100, true),
                                J::object({{"name", "include_operations"},
                                           {"label", "Include full operation payloads"},
                                           {"kind", "bool"},
                                           {"value", false}})}),
                      [this](const J &options) { AnimationSubmit("animation_changes", options); });
        return;
    }
    if (command == Monstey_Undo || command == Monstey_Redo || command == Monstey_Revert)
    {
        AnimationSubmit(command == Monstey_Undo   ? "animation_undo"
                        : command == Monstey_Redo ? "animation_redo"
                                                  : "animation_revert",
                        J::object());
        return;
    }
    if (command == Monstey_AnimationSkeleton)
    {
        const auto source = Monstey_FileDialog(window, false, L"Native rig (*.skel)\0*.skel\0\0", L"skel",
                                               L"Inspect animation library registration");
        if (!source)
            return;
        Queue("animation_skeleton_inspect", J::object({{"source", Monstey_JsonPath(*source)}}),
              [this, source](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  animationLastReport = reply.at("result");
                  AnimationPopulate();
                  if (!reply.at("result").at("qualified_for_append").as_bool())
                  {
                      SetStatus(L"Rig inspected. Appending libraries is qualified only "
                                L"for the 128-bone SKEL17 profile.");
                      return;
                  }
                  const auto sha = reply.at("result").at("source_sha256");
                  StructureForm(
                      L"SKEL17: 128-bone registration profile qualified",
                      J::array({Monstey_StructureText("resource", "Native ANIX resource path",
                                                      "characters/animationcharacters/monstey/private.anix")}),
                      [this, source, sha](const J &value)
                      {
                          const auto output = Monstey_FileDialog(window, true, L"Native rig (*.skel)\0*.skel\0\0",
                                                                 L"skel", L"Write NEW registered SKEL");
                          if (output)
                              AnimationSubmit("animation_skeleton_register",
                                              J::object({{"source", Monstey_JsonPath(*source)},
                                                         {"resource", value.at("resource")},
                                                         {"output", Monstey_JsonPath(*output)},
                                                         {"expected_sha256", sha}}));
                      });
              });
        return;
    }
    if (command == Monstey_Add)
    {
        const auto path = Monstey_FileDialog(window, false, L"Semantic animation scene (*.json)\0*.json\0\0", L"json",
                                             L"Import clip from semantic scene");
        if (!path)
            return;
        const auto sourceHash = Monstey_HashSha256(*path);
        auto text = Monstey_ReadText(*path, 512ull * 1024 * 1024);
        if (text.starts_with("\xef\xbb\xbf"))
            text.erase(0, 3);
        const auto scene = J::Parse(text);
        Monstey_Require(Monstey_HashSha256(*path) == sourceHash,
                        "Semantic animation scene changed while reading its clip list");
        Monstey_Require(scene.at("format") == J("Monstey_ANIX_Semantic_v1") && !scene.at("clips").empty(),
                        "Choose a complete semantic animation scene");
        J choices = J::array();
        for (const auto &clip : scene.at("clips").as_array())
            choices.as_array().push_back(J::object({{"label", clip.at("name")}, {"value", clip.at("name")}}));
        const auto g = generation, r = revision;
        StructureForm(L"Import native-mapped clip",
                      J::array({Monstey_StructureChoice("clip", "Source clip", choices, choices[0].at("value")),
                                Monstey_StructureText("name", "New clip name", "Monstey_Imported")}),
                      [this, path, sourceHash, g, r](const J &value)
                      {
                          AnimationSubmit("animation_import_clip",
                                          J::object({{"scene_path", Monstey_JsonPath(*path)},
                                                     {"expected_sha256", sourceHash},
                                                     {"clip", value.at("clip")},
                                                     {"name", value.at("name")},
                                                     {"expected_generation", g},
                                                     {"expected_revision", r}}),
                                          value.at("name").as_string());
                      });
        return;
    }
    if (command == Monstey_AnimationEvents)
    {
        animationEventMode = !animationEventMode;
        AnimationPopulate();
        UpdateButtons();
        return;
    }
    if (command == Monstey_AnimationEventAdd || command == Monstey_AnimationEventEdit ||
        command == Monstey_AnimationEventRemove || command == Monstey_AnimationEventClear)
    {
        AnimationEvent(command);
        return;
    }
    if (command == Monstey_AnimationHeld)
    {
        Monstey_Require(!animationView.empty(), "Open a native animation library first");
        const auto g = generation, r = revision;
        StructureForm(L"Create held pose from bind",
                      J::array({Monstey_StructureText("name", "New clip name", "Monstey_Held"),
                                Monstey_FxNumber("tick", "Source tick (milliseconds)", 0, true),
                                Monstey_FxNumber("duration_ticks", "Held duration (milliseconds)", 1000, true)}),
                      [this, g, r](const J &value)
                      {
                          auto operation = value;
                          operation["op"] = "held";
                          operation["source"] = "bind";
                          operation["bind_fallback"] = false;
                          AnimationSubmit("animation_edit",
                                          J::object({{"operations", J::array({operation})},
                                                     {"expected_generation", g},
                                                     {"expected_revision", r}}),
                                          value.at("name").as_string());
                      });
        return;
    }
    if (command != Monstey_Rename && command != Monstey_Clone && command != Monstey_Delete &&
        command != Monstey_AnimationHeld && command != Monstey_AnimationTiming && command != Monstey_AnimationSample)
        return;
    Monstey_Require(!animationClip.empty() && animationClip.at("name") == J(animationSelected),
                    "Select a native animation clip first");
    const auto snapshot = animationClip;
    if (command == Monstey_AnimationTiming || command == Monstey_AnimationSample)
        Monstey_Require(snapshot.value("sample_supported", false).as_bool(),
                        "This native clip does not support sampling or timing edits");
    const auto edit = [this, snapshot](J operation, std::string selected = {})
    {
        AnimationSubmit("animation_edit",
                        J::object({{"operations", J::array({operation})},
                                   {"expected_generation", snapshot.at("ui_generation")},
                                   {"expected_revision", snapshot.at("ui_revision")}}),
                        selected);
    };
    if (command == Monstey_Rename || command == Monstey_Clone)
    {
        const auto duplicate = command == Monstey_Clone;
        StructureForm(
            duplicate ? L"Duplicate native clip" : L"Rename native clip",
            J::array({Monstey_StructureText("name", "Clip name (1-47 ASCII bytes)",
                                            duplicate ? "Monstey_Clip" : snapshot.at("name").as_string().c_str())}),
            [snapshot, edit, duplicate](const J &value)
            {
                edit(J::object({{"op", duplicate ? "duplicate" : "rename"},
                                {"clip", snapshot.at("name")},
                                {"name", value.at("name")}}),
                     value.at("name").as_string());
            });
    }
    else if (command == Monstey_Delete)
        edit(J::object({{"op", "delete"}, {"clip", snapshot.at("name")}}));
    else if (command == Monstey_AnimationTiming)
    {
        J choices = J::array({J::object({{"label", "Set duration (milliseconds)"}, {"value", "length"}}),
                              J::object({{"label", "Playback rate factor"}, {"value", "rate"}}),
                              J::object({{"label", "Insert held delay"}, {"value", "delay"}}),
                              J::object({{"label", "Trim interval"}, {"value", "trim"}})});
        StructureForm(
            L"Native clip timing", J::array({Monstey_StructureChoice("op", "Timing operation", choices, "length")}),
            [this, snapshot, edit](const J &choice)
            {
                const auto op = choice.at("op").as_string();
                J form = J::array();
                if (op == "length")
                    form.as_array().push_back(Monstey_FxNumber("duration_ticks", "New duration (milliseconds)",
                                                               snapshot.at("duration_ticks"), true));
                else if (op == "rate")
                    form.as_array().push_back(Monstey_FxNumber("factor", "Positive playback rate factor", 1.0));
                else if (op == "delay")
                {
                    form.as_array().push_back(Monstey_FxNumber("at_tick", "Insert at tick (milliseconds)", 0, true));
                    form.as_array().push_back(
                        Monstey_FxNumber("duration_ticks", "Inserted duration (milliseconds)", 250, true));
                }
                else
                {
                    form.as_array().push_back(Monstey_FxNumber("start_tick", "First tick (milliseconds)", 0, true));
                    form.as_array().push_back(
                        Monstey_FxNumber("end_tick", "Last tick (milliseconds)", snapshot.at("duration_ticks"), true));
                }
                StructureForm(L"Timing values", form,
                              [snapshot, op, edit](const J &value)
                              {
                                  auto operation = value;
                                  operation["op"] = op;
                                  operation["clip"] = snapshot.at("name");
                                  edit(operation);
                              });
            });
    }
    else if (command == Monstey_AnimationSample)
    {
        const auto &segments = snapshot.at("segments");
        Monstey_Require(!segments.empty(), "This clip has no native sample segments");
        const auto first = segments[0].at("first_tick");
        const auto last = segments[segments.size() - 1].at("last_tick");
        StructureForm(L"Native parent-local tick samples",
                      J::array({Monstey_FxNumber("first", "First tick (milliseconds)", first),
                                Monstey_FxNumber("last", "Last tick (milliseconds)", last),
                                Monstey_FxNumber("count", "Samples (1-1024)", first == last ? 1 : 3, true),
                                J::object({{"name", "fallback"},
                                           {"label", "Overlay library bind fallback"},
                                           {"kind", "bool"},
                                           {"value", (snapshot.at("flags").as_uint() & 1) == 0}})}),
                      [this, snapshot](const J &value)
                      {
                          const auto count = value.at("count").as_uint();
                          Monstey_Require(count > 0 && count <= 1024, "Choose 1-1024 tick samples");
                          const auto firstTick = value.at("first").as_number(), lastTick = value.at("last").as_number();
                          Monstey_Require(count == 1 || lastTick > firstTick, "Multiple sample ticks must increase");
                          J times = J::array();
                          for (std::uint64_t i = 0; i < count; ++i)
                              times.as_array().push_back(count == 1 ? firstTick
                                                                    : firstTick + (lastTick - firstTick) *
                                                                                      static_cast<double>(i) /
                                                                                      static_cast<double>(count - 1));
                          Queue("animation_sample",
                                J::object({{"clip", snapshot.at("name")},
                                           {"ticks", times},
                                           {"bind_fallback", value.at("fallback")},
                                           {"expected_generation", snapshot.at("ui_generation")},
                                           {"expected_revision", snapshot.at("ui_revision")}}),
                                [this, snapshot](const J &reply)
                                {
                                    if (tab != 4 || !animationMode ||
                                        animationSelected != snapshot.at("name").as_string())
                                        return;
                                    if (!reply.at("ok").as_bool())
                                    {
                                        Error(reply.at("error").at("message").as_string());
                                        return;
                                    }
                                    animationLastReport = reply.at("result");
                                    AnimationPopulate();
                                });
                      });
    }
}

} // namespace Monstey::Monstey_AppInternal
