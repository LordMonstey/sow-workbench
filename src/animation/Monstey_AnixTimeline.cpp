#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
namespace Monstey_AnixPrivate
{

void Monstey_ARegrid(Monstey_AClip &c, const J &events)
{
    Monstey_ARequire(c.duration <= 0xffffff, "Clip duration exceeds 24-bit footer ticks");
    std::vector<std::pair<std::uint32_t, std::optional<std::string>>> markers;
    markers.emplace_back(0, std::nullopt);
    if (c.duration)
        markers.emplace_back(c.duration, std::nullopt);
    Monstey_ARequire(events.is_array(), "Events must be an array");
    for (const auto &e : events.as_array())
    {
        const auto tick = Monstey_AJsonU(e.at("tick"));
        const auto text = Monstey_AEventBytes(e);
        Monstey_ARequire(tick <= c.duration, "Event lies beyond clip duration");
        markers.emplace_back(tick, text);
    }
    std::stable_sort(markers.begin(), markers.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    std::vector<std::uint32_t> grid;
    const auto count = static_cast<std::uint64_t>(std::nearbyint(c.duration * 0.03));
    Monstey_ARequire(count <= Monstey_SampleLimit, "Footer time grid exceeds limit");
    for (std::uint64_t k = 0; k <= count; ++k)
        grid.push_back(Monstey_A32(k * 100 / 3));
    for (const auto &[tick, text] : markers)
    {
        (void)text;
        grid.push_back(tick);
    }
    std::sort(grid.begin(), grid.end());
    grid.erase(std::unique(grid.begin(), grid.end()), grid.end());
    c.footer.frames = Monstey_A32(grid.size());
    c.footer.markers.clear();
    for (const auto &[tick, text] : markers)
        c.footer.markers.push_back(
            {Monstey_A32(static_cast<std::size_t>(std::lower_bound(grid.begin(), grid.end(), tick) - grid.begin())),
             tick, text});
}

void Monstey_AUnique(const Monstey_ALibrary &l)
{
    std::set<std::uint32_t> hashes;
    for (const auto &c : l.clips)
        Monstey_ARequire(hashes.insert(c.hash).second, "Clip names/hash aliases collide");
    Monstey_ARequire(!l.clips.empty(), "Animation library cannot be empty");
}

void Monstey_ARequireTimeline(const Monstey_ALibrary &l, const Monstey_AClip &c)
{
    for (const auto &code : c.codecs)
        Monstey_ARequire(Monstey_APoseCodec(code) || Monstey_AAuxiliaryCodec(code) || code == "STRI",
                         "Timeline edits require qualified authoring for every "
                         "pose/event/auxiliary codec");
    Monstey_ARequire(Monstey_AUnsupportedAuxiliary(l, c).empty(),
                     "Timeline edit cannot encode unqualified auxiliary variants "
                     "or varying static tracks");
    for (const auto &s : c.segments)
        for (const auto &e : s.extras)
        {
            Monstey_ARequire(e.hash == Monstey_Lith, "Timeline edit cannot infer opaque segment extra timing");
            Monstey_ARequire(Monstey_AU32(e.bytes, 4) == 0 && Monstey_AU32(e.bytes, 8) == 0,
                             "Timeline edit cannot infer auxiliary LITH timing");
        }
}

J Monstey_ATimelineScene(const Monstey_ALibrary &l, const Monstey_AClip &c, const J &op, std::string_view kind)
{
    Monstey_ARequireSegments(c);
    Monstey_ARequireTimeline(l, c);
    Monstey_ARequire(c.segments.front().first == 0 && c.segments.back().last == c.duration,
                     "Timeline edit requires a complete descriptor/segment time range");
    auto source = Monstey_ASampleTicks(c);
    std::set<double> times(source.begin(), source.end());
    std::uint32_t start = 0, end = c.duration, delay = 0, at = 0, newduration = 0;
    double scale = 1;
    if (kind == "trim")
    {
        start = Monstey_AJsonU(op.at("start_tick"));
        end = Monstey_AJsonU(op.at("end_tick"));
        Monstey_ARequire(start < end && end <= c.duration, "Trim range is outside clip");
        times.insert(start);
        times.insert(end);
        for (auto it = times.begin(); it != times.end();)
            if (*it < start || *it > end)
                it = times.erase(it);
            else
                ++it;
        newduration = end - start;
    }
    else if (kind == "delay")
    {
        at = Monstey_AJsonU(op.at("at_tick"));
        delay = Monstey_AJsonU(op.at("duration_ticks"));
        Monstey_ARequire(at <= c.duration && delay > 0, "Invalid inserted delay");
        times.insert(at);
        newduration = Monstey_A32(static_cast<std::uint64_t>(c.duration) + delay);
    }
    else
    {
        if (kind == "rate")
        {
            const auto factor = op.at("factor").as_number();
            Monstey_ARequire(std::isfinite(factor) && factor > 0, "Rate factor must be positive");
            scale = 1 / factor;
            const auto length = std::nearbyint(c.duration * scale);
            Monstey_ARequire(std::isfinite(length) && length >= 1 && length <= 0xffffff,
                             "Rate produces invalid native duration");
            newduration = static_cast<std::uint32_t>(length);
        }
        else
        {
            newduration = Monstey_AJsonU(op.at("duration_ticks"));
            Monstey_ARequire(c.duration > 0, "Cannot scale zero-duration animation");
            scale = static_cast<double>(newduration) / c.duration;
        }
    }
    Monstey_ARequire(newduration > 0 && newduration <= 0xffffff, "Edited timeline duration outside native limits");
    source.assign(times.begin(), times.end());
    auto scene = Monstey_ASample(l, c, source, false);
    scene["name"] = c.name;
    scene["duration_ticks"] = newduration;
    scene["flags"] = c.flags;
    scene["unknown116"] = c.unknown116;
    scene["extents_cm"] = Monstey_AArray(c.footer.extents);
    if (const auto channel = Monstey_AClipEventChannel(l, c))
        scene["event_channel_index"] = *channel;
    if (kind != "trim")
    {
        // Timing changes do not change the clip's declared root transforms.
        // They may intentionally differ from the sampled Null channels.
        scene["root_start"] = Monstey_AArray(c.root_start);
        scene["root_end"] = Monstey_AArray(c.root_end);
    }
    else
    {
        // Crop components with qualified Null channels at the sampled boundaries.
        // A missing channel can retain a constant descriptor component, but two
        // different endpoints do not establish an interpolation policy.
        auto root_start = c.root_start;
        auto root_end = c.root_end;
        bool position = false, rotation = false;
        for (const auto &track : scene["tracks"].as_array())
        {
            const auto channel = Monstey_A16(track["channel_index"].as_uint());
            if (l.channels[channel].bone != 0x88baa7e7)
                continue;
            const auto type = l.channels[channel].type;
            if (type != Monstey_Position && type != Monstey_Rotation)
                continue;
            const auto offset = type == Monstey_Position ? 0u : 3u;
            const auto &values = track["values"];
            const auto &first = values[0];
            const auto &last = values.as_array().back();
            for (std::size_t component = 0; component < first.size(); ++component)
            {
                root_start[offset + component] = Monstey_AFloat(first[component].as_number());
                root_end[offset + component] = Monstey_AFloat(last[component].as_number());
            }
            (type == Monstey_Position ? position : rotation) = true;
        }
        for (std::size_t component = 0; component < 7; ++component)
            if ((component < 3 && !position) || (component >= 3 && !rotation))
                Monstey_ARequire(c.root_start[component] == c.root_end[component],
                                 "Trim cannot infer varying root descriptors without a Null channel");
        scene["root_start"] = Monstey_AArray(root_start);
        scene["root_end"] = Monstey_AArray(root_end);
    }

    J newtimes = J::array();
    std::vector<std::size_t> repeat;
    for (std::size_t i = 0; i < source.size(); ++i)
    {
        auto t = static_cast<std::uint32_t>(source[i]);
        std::uint32_t mapped = 0;
        if (kind == "trim")
            mapped = t - start;
        else if (kind == "delay")
            mapped = t + (t > at ? delay : 0);
        else
            mapped = Monstey_A32(static_cast<std::uint64_t>(std::nearbyint(t * scale)));
        if (!newtimes.empty())
            Monstey_ARequire(mapped > newtimes.as_array().back().as_uint(),
                             "Timeline edit collapses distinct sample ticks");
        newtimes.as_array().emplace_back(mapped);
        repeat.push_back(i);
        if (kind == "delay" && t == at)
        {
            newtimes.as_array().emplace_back(mapped + delay);
            repeat.push_back(i);
        }
    }
    scene["ticks"] = std::move(newtimes);
    if (kind == "delay")
        for (auto &tr : scene["tracks"].as_array())
        {
            auto values = tr.at("values");
            J expanded = J::array();
            for (auto i : repeat)
                expanded.as_array().push_back(values[i]);
            tr["values"] = std::move(expanded);
        }
    J events = J::array();
    const auto source_events = Monstey_AClipEvents(c);
    for (auto ev : source_events.as_array())
    {
        auto t = Monstey_AJsonU(ev.at("tick"));
        if (kind == "trim")
        {
            if (t < start || t > end)
                continue;
            t -= start;
        }
        else if (kind == "delay")
        {
            if (t >= at)
                t += delay;
        }
        else
            t = Monstey_A32(static_cast<std::uint64_t>(std::nearbyint(t * scale)));
        ev["tick"] = t;
        events.as_array().push_back(std::move(ev));
    }
    scene["events"] = std::move(events);
    return scene;
}

} // namespace Monstey_AnixPrivate
using namespace Monstey_AnixPrivate;

J Monstey_Anix::Edit(const J &operations)
{
    Monstey_ARequire(operations.is_array() && !operations.empty() && operations.size() <= 10000,
                     "Edit needs 1..10000 operations");
    auto next = std::make_unique<Monstey_State>(*state_);
    J log = J::array();
    for (const auto &op : operations.as_array())
    {
        Monstey_ARequire(op.is_object(), "Animation operation must be an object");
        const auto &kind = op.at("op").as_string();
        if (kind == "import")
        {
            auto c = Monstey_AImportClip(*next, op.at("clip"));
            next->clips.push_back(std::move(c));
        }
        else if (kind == "held")
        {
            const auto index = Monstey_AFind(*next, op.value("source", "bind").as_string());
            const auto &source = next->clips[index];
            Monstey_ARequireSegments(source);
            const auto tick =
                op.contains("tick") ? op.at("tick").as_number() : static_cast<double>(source.segments.front().first);
            const std::array<double, 1> one{tick};
            auto scene = Monstey_ASample(*next, source, one, op.value("bind_fallback", false).as_bool());
            Monstey_ARequire(scene["complete_pose_codecs"].as_bool(),
                             "Held-pose creation has unqualified source codecs");
            const auto duration = Monstey_AJsonU(op.value("duration_ticks", 1000));
            scene["name"] = op.at("name");
            scene["duration_ticks"] = duration;
            scene["ticks"] = J::array({0, duration});
            for (auto &tr : scene["tracks"].as_array())
            {
                const auto value = tr["values"][0];
                Monstey_ARequire(!value.is_null(), "Held source has absent track");
                tr["values"] = J::array({value, value});
            }
            scene["events"] = J::array();
            scene["extents_cm"] = Monstey_AArray(source.footer.extents);
            next->clips.push_back(Monstey_AImportClip(*next, scene));
        }
        else
        {
            const auto index = Monstey_AFind(*next, op.at("clip").as_string());
            auto &c = next->clips[index];
            if (kind == "rename")
            {
                const auto name = op.at("name").as_string();
                Monstey_AName(name, true);
                Monstey_ARequire(c.hash != Monstey_AHash("bind"), "Native bind fallback cannot be renamed");
                c.name = name;
                c.hash = Monstey_AHash(name);
            }
            else if (kind == "duplicate")
            {
                auto copy = c;
                copy.name = op.at("name").as_string();
                Monstey_AName(copy.name, true);
                copy.hash = Monstey_AHash(copy.name);
                next->clips.push_back(std::move(copy));
            }
            else if (kind == "delete")
            {
                Monstey_ARequire(c.hash != Monstey_AHash("bind"), "Native bind fallback cannot be deleted");
                next->clips.erase(next->clips.begin() + index);
            }
            else if (kind == "events")
                Monstey_AReplaceEvents(c, *next, op.at("events"), op);
            else if (kind == "rate" || kind == "length" || kind == "delay" || kind == "trim")
            {
                const auto scene = Monstey_ATimelineScene(*next, c, op, kind);
                auto replacement = Monstey_AImportClip(*next, scene);
                if (kind != "trim")
                {
                    // Preserve the original native float bits, including -0,
                    // instead of deriving independent descriptor metadata.
                    replacement.root_start = c.root_start;
                    replacement.root_end = c.root_end;
                }

                replacement.footer.unknown = c.footer.unknown;
                replacement.footer.name_offset = c.footer.name_offset;
                for (const auto &e : c.extras)
                    if (e.hash != Monstey_KeyString)
                        replacement.extras.push_back(e);
                c = std::move(replacement);
            }
            else
                throw Monstey_Error("anix_operation", "Unknown native animation operation: " + kind);
        }
        Monstey_AUnique(*next);
        next->changed = true;
        log.as_array().push_back(
            J::object({{"op", kind}, {"clip_count", static_cast<std::uint64_t>(next->clips.size())}}));
    }
    // Serialize and parse before commit: directory, descriptors, segment extents,
    // footer arena and every supported codec must be valid together.
    auto raw = Monstey_ABuild(*next);
    Monstey_Anix verified(raw);
    const auto proof = verified.Describe();
    state_ = std::move(next);
    return J::object({{"operations", log},
                      {"output_sha256", Monstey_HashSha256(raw)},
                      {"output_bytes", static_cast<std::uint64_t>(raw.size())},
                      {"clip_count", proof["clip_count"]},
                      {"footer_arena_bytes", proof["footer_arena_bytes"]},
                      {"runtime_verified", false}});
}

} // namespace Monstey
