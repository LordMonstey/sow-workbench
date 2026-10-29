#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::AnimationEvent(int command)
{
    Monstey_Require(!animationClip.empty() && animationClip.contains("events"),
                    "Load a clip with native event metadata first");
    const auto snapshot = animationClip;
    auto events = snapshot.at("events");
    const auto row = ListView_GetNextItem(fields, -1, LVNI_SELECTED);
    if (command != Monstey_AnimationEventAdd && command != Monstey_AnimationEventClear)
        Monstey_Require(row >= 0 && static_cast<std::size_t>(row) < events.size(),
                        "Select an event in the right-hand list");
    const auto submit = [this, snapshot](J next, const J &channel)
    {
        std::stable_sort(next.as_array().begin(), next.as_array().end(),
                         [](const J &a, const J &b) { return a.at("tick").as_uint() < b.at("tick").as_uint(); });
        J operation = J::object({{"op", "events"}, {"clip", snapshot.at("name")}, {"events", next}});
        if (!channel.is_null())
            operation["event_channel_index"] = channel;
        AnimationSubmit("animation_edit", J::object({{"operations", J::array({operation})},
                                                     {"expected_generation", snapshot.at("ui_generation")},
                                                     {"expected_revision", snapshot.at("ui_revision")}}));
    };
    if (command == Monstey_AnimationEventClear)
    {
        Monstey_Require(!snapshot.value("empty_native_clip", false).as_bool(),
                        "Empty native clips support structural edits only");
        submit(J::array(), J());
        return;
    }
    Monstey_Require(
        snapshot.value("event_edit_supported", false).as_bool(),
        snapshot.value("event_message", "Native event binding is not qualified for regular editing").as_string());
    if (command == Monstey_AnimationEventRemove)
    {
        events.as_array().erase(events.as_array().begin() + row);
        submit(events, snapshot.value("event_channel_index", J()));
        return;
    }
    const bool adding = command == Monstey_AnimationEventAdd;
    const bool raw = !adding && events[static_cast<std::size_t>(row)].contains("text_bytes_hex");
    const auto field = raw ? "text_bytes_hex" : "text";
    J form = J::array(
        {Monstey_FxNumber("tick", "Event tick (milliseconds)",
                          adding ? J(0) : events[static_cast<std::size_t>(row)].at("tick"), true),
         Monstey_StructureText(field, raw ? "Native event bytes (hex, without NUL)" : "Native ASCII event text",
                               adding ? "" : events[static_cast<std::size_t>(row)].at(field).as_string().c_str())});
    auto channel = snapshot.value("event_channel_index", J());
    if (snapshot.contains("event_channels") && !snapshot.at("event_channels").empty())
    {
        J choices = J::array();
        for (const auto &entry : snapshot.at("event_channels").as_array())
            choices.as_array().push_back(J::object({{"label", "Event channel " + entry.at("index").dump() + " / bone " +
                                                                  entry.at("bone_hash").as_string()},
                                                    {"value", entry.at("index")}}));
        if (channel.is_null())
            channel = choices[0].at("value");
        form.as_array().push_back(Monstey_StructureChoice("channel", "Native Event channel", choices, channel));
    }
    StructureForm(adding ? L"Add animation event" : L"Edit animation event", form,
                  [events, row, adding, raw, channel, submit](const J &value) mutable
                  {
                      auto event = adding ? J::object() : events[static_cast<std::size_t>(row)];
                      event["tick"] = value.at("tick");
                      if (raw)
                      {
                          event["text_bytes_hex"] = value.at("text_bytes_hex");
                          // Raw bytes are edited explicitly; the native response regenerates
                          // the display mirror instead of submitting a stale text value.
                          event.as_object().erase("text");
                      }
                      else
                          event["text"] = value.at("text");
                      if (adding)
                          events.as_array().push_back(std::move(event));
                      else
                          events[static_cast<std::size_t>(row)] = std::move(event);
                      submit(events, value.value("channel", channel));
                  });
}

} // namespace Monstey::Monstey_AppInternal
