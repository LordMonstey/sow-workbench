#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
namespace Monstey_AnixPrivate
{

std::string Monstey_AEventString(std::span<const std::uint8_t> b, std::size_t p, std::size_t end)
{
    Monstey_ARequire(p < end && end <= b.size(), "Invalid ANIX event string extent");
    std::string text;
    for (; p < end && b[p]; ++p)
        text.push_back(static_cast<char>(b[p]));
    Monstey_ARequire(p < end, "Unterminated ANIX event string");
    return text;
}

void Monstey_AEventValidate(std::string_view text)
{
    Monstey_ARequire(!text.empty() && text.find('\0') == std::string_view::npos,
                     "Native event bytes must be nonempty without NUL");
}

J Monstey_AEventJson(std::uint32_t tick, std::string_view text)
{
    auto event = J::object({{"tick", tick}, {"text", Monstey_DisplayUtf8(text)}});
    if (std::any_of(text.begin(), text.end(), [](unsigned char value) { return value >= 128; }))
    {
        constexpr char digits[] = "0123456789abcdef";
        std::string hex;
        hex.reserve(text.size() * 2);
        for (const unsigned char value : text)
        {
            hex.push_back(digits[value >> 4]);
            hex.push_back(digits[value & 15]);
        }
        event["text_bytes_hex"] = std::move(hex);
    }
    return event;
}

std::string Monstey_AEventBytes(const J &event)
{
    Monstey_ARequire(event.is_object(), "ANIX event must be an object");
    std::string text;
    if (event.contains("text_bytes_hex"))
    {
        const auto &raw = event.at("text_bytes_hex");
        Monstey_ARequire(raw.is_string(), "Event text_bytes_hex must be a string");
        const auto &hex = raw.as_string();
        Monstey_ARequire(!hex.empty() && hex.size() % 2 == 0 && hex.size() / 2 <= UINT16_MAX - 1,
                         "Event text_bytes_hex has invalid native byte length");
        const auto digit = [](unsigned char value) -> unsigned
        {
            return value >= '0' && value <= '9'   ? static_cast<unsigned>(value - '0')
                   : value >= 'a' && value <= 'f' ? static_cast<unsigned>(value - 'a' + 10)
                   : value >= 'A' && value <= 'F' ? static_cast<unsigned>(value - 'A' + 10)
                                                  : 16u;
        };
        Monstey_ARequire(std::all_of(hex.begin(), hex.end(), [&](unsigned char value) { return digit(value) < 16; }),
                         "Event text_bytes_hex contains a non-hex digit");
        text.reserve(hex.size() / 2);
        for (std::size_t i = 0; i < hex.size(); i += 2)
            text.push_back(static_cast<char>((digit(static_cast<unsigned char>(hex[i])) << 4) |
                                             digit(static_cast<unsigned char>(hex[i + 1]))));
        Monstey_AEventValidate(text);
        if (event.contains("text"))
            Monstey_ARequire(event.at("text").is_string() && event.at("text").as_string() == Monstey_DisplayUtf8(text),
                             "Event text contradicts its text_bytes_hex display; omit text when editing raw bytes");
    }
    else
    {
        Monstey_ARequire(event.contains("text") && event.at("text").is_string(),
                         "Event text must be an ASCII string or provide text_bytes_hex");
        text = event.at("text").as_string();
        Monstey_ARequire(text.size() <= UINT16_MAX - 1 &&
                             std::all_of(text.begin(), text.end(), [](unsigned char value) { return value < 128; }),
                         "Event text must be ASCII; provide text_bytes_hex for native high bytes");
        Monstey_AEventValidate(text);
    }
    return text;
}

std::vector<J> Monstey_AEventList(const Monstey_AClip &c)
{
    std::vector<J> out;
    for (const auto &m : c.footer.markers)
        if (m.event)
            out.push_back(Monstey_AEventJson(m.tick, *m.event));
    return out;
}

std::optional<std::uint16_t> Monstey_AClipEventChannel(const Monstey_ALibrary &l, const Monstey_AClip &c)
{
    const auto codec = std::find(c.codecs.begin(), c.codecs.end(), "STRI");
    if (codec == c.codecs.end())
        return std::nullopt;
    const auto index = static_cast<std::size_t>(codec - c.codecs.begin());
    std::optional<std::uint16_t> channel;
    for (const auto &track : c.tracks)
        if (track.codec == index)
        {
            if (channel)
                throw Monstey_Error("unsupported_anix_event_binding",
                                    "Multiple STRI tracks have no qualified single-channel semantic mapping");
            Monstey_ARequire(l.channels[track.channel].type == Monstey_Event,
                             "STRI track does not use a native Event channel");
            channel = track.channel;
        }
    Monstey_ARequire(channel.has_value(), "STRI codec has no qualified Event track");
    return channel;
}

std::uint16_t Monstey_AResolveEventChannel(const Monstey_ALibrary &l, const J &options,
                                           std::optional<std::uint16_t> existing)
{
    if (options.contains("event_channel_index"))
    {
        const auto channel = Monstey_A16(options.at("event_channel_index").as_uint());
        Monstey_ARequire(channel < l.channels.size() && l.channels[channel].type == Monstey_Event,
                         "event_channel_index must select a shared native Event channel");
        return channel;
    }
    if (existing)
        return *existing;
    std::optional<std::uint16_t> selected;
    for (std::size_t i = 0; i < l.channels.size(); ++i)
        if (l.channels[i].type == Monstey_Event)
        {
            if (selected)
                throw Monstey_Error("ambiguous_anix_event_channel",
                                    "Multiple shared Event channels require explicit event_channel_index");
            selected = Monstey_A16(i);
        }
    Monstey_ARequire(selected.has_value(), "Events require a native Event channel in the shared table");
    return *selected;
}

J Monstey_AClipEvents(const Monstey_AClip &c)
{
    auto it = std::find(c.codecs.begin(), c.codecs.end(), "STRI");
    if (it == c.codecs.end())
        return J::array();
    const auto index = static_cast<std::size_t>(it - c.codecs.begin());
    J first;
    bool set = false;
    for (const auto &s : c.segments)
    {
        auto ev = Monstey_AnixDecodeCodec("STRI", s.blocks[index], Monstey_ACodecTracks(c, index), {}, {},
                                          Monstey_AKeyStrings(c));
        if (!set)
        {
            first = ev;
            set = true;
        }
        else if (ev != first)
            throw Monstey_Error("unsupported_anix_event_layout",
                                "Native STRI differs between segments; semantic event export is ambiguous");
    }
    return set ? first : J::array();
}

std::pair<B, B> Monstey_AEncodeEvents(const J &events)
{
    B pool, block;
    std::map<std::string, std::uint16_t> offsets;
    struct E
    {
        std::uint32_t tick;
        std::uint16_t offset, length;
    };
    std::vector<E> entries;
    std::optional<std::uint32_t> previous_tick;
    for (const auto &e : events.as_array())
    {
        const auto tick = Monstey_AJsonU(e.at("tick"));
        Monstey_ARequire(!previous_tick || tick >= *previous_tick, "Events must be ordered by tick");
        previous_tick = tick;
        const auto text = Monstey_AEventBytes(e);
        auto it = offsets.find(text);
        if (it == offsets.end())
        {
            auto p = Monstey_A16(pool.size());
            Monstey_ARequire(text.size() + 1 <= 65535 - p, "KeyString exceeds 16-bit offsets");
            pool.insert(pool.end(), text.begin(), text.end());
            pool.push_back(0);
            it = offsets.emplace(text, p).first;
        }
        entries.push_back({Monstey_AJsonU(e.at("tick")), it->second, Monstey_A16(text.size() + 1)});
    }
    Monstey_AppendU16(block, 2);
    Monstey_AppendU16(block, Monstey_A16(pool.size()));
    Monstey_AppendU32(block, Monstey_A32(entries.size()));
    Monstey_AppendU16(block, 0);
    for (const auto &e : entries)
    {
        Monstey_AppendU32(block, e.tick);
        Monstey_AppendU16(block, e.offset);
        Monstey_AppendU16(block, e.length);
    }
    Monstey_APad(block);
    return {std::move(block), std::move(pool)};
}

void Monstey_AReplaceEvents(Monstey_AClip &c, const Monstey_ALibrary &l, const J &events, const J &options)
{
    Monstey_ARequireSegments(c);
    Monstey_ARequire(events.is_array(), "Events must be an array");
    auto encoded = Monstey_AEncodeEvents(events);
    auto it = std::find(c.codecs.begin(), c.codecs.end(), "STRI");
    std::size_t codec = static_cast<std::size_t>(it - c.codecs.begin());
    if (options.contains("event_channel_index"))
        (void)Monstey_AResolveEventChannel(l, options);
    if (events.empty() && it != c.codecs.end())
    {
        c.codecs.erase(it);
        Monstey_ARequire(!c.codecs.empty(), "Cannot remove the only codec in a clip");
        c.tracks.erase(std::remove_if(c.tracks.begin(), c.tracks.end(), [&](auto t) { return t.codec == codec; }),
                       c.tracks.end());
        for (auto &t : c.tracks)
            if (t.codec > codec)
                --t.codec;
        c.extras.erase(
            std::remove_if(c.extras.begin(), c.extras.end(), [](const auto &e) { return e.hash == Monstey_KeyString; }),
            c.extras.end());
        for (auto &s : c.segments)
        {
            auto blocks = s.blocks;
            blocks.erase(blocks.begin() + codec);
            auto raw = Monstey_AEncodeSegment(c.codecs, blocks, {}, s.extras);
            auto next = Monstey_AReadSegment(raw, c.codecs.size(), s.first, s.last, 0);
            next.appended = true;
            s = std::move(next);
        }
        c.descriptor_rebuild = true;
        Monstey_ARegrid(c, events);
        return;
    }

    if (it == c.codecs.end())
    {
        if (events.empty())
        {
            Monstey_ARegrid(c, events);
            return;
        }
        Monstey_ARequire(c.codecs.size() < 32, "No free codec slot for events");
        c.codecs.push_back("STRI");
        const auto channel = Monstey_AResolveEventChannel(l, options);
        c.tracks.push_back({static_cast<std::uint8_t>(codec), channel});
    }
    else
    {
        (void)Monstey_AClipEvents(c);
        const auto channel = Monstey_AResolveEventChannel(l, options, Monstey_AClipEventChannel(l, c));
        for (auto &track : c.tracks)
            if (track.codec == codec)
                track.channel = channel;
    }
    auto extra =
        std::find_if(c.extras.begin(), c.extras.end(), [](const auto &e) { return e.hash == Monstey_KeyString; });
    if (extra == c.extras.end())
        c.extras.push_back({Monstey_KeyString, encoded.second});
    else
        extra->bytes = encoded.second;
    for (auto &s : c.segments)
    {
        auto blocks = s.blocks;
        if (codec == blocks.size())
            blocks.push_back(encoded.first);
        else
            blocks[codec] = encoded.first;
        // Retain arbitrary non-LITH extras and the entire native LITH bytes, including auxiliary tables.
        auto raw = Monstey_AEncodeSegment(c.codecs, blocks, {}, s.extras);
        auto next = Monstey_AReadSegment(raw, c.codecs.size(), s.first, s.last, 0);
        next.appended = true;
        s = std::move(next);
    }
    c.descriptor_rebuild = true;
    Monstey_ARegrid(c, events);
}

} // namespace Monstey_AnixPrivate
using namespace Monstey_AnixPrivate;

J Monstey_Anix::EventInfo(std::string_view name) const
{
    const auto &library = *state_;
    const auto &clip = library.clips[Monstey_AFind(library, name)];
    J choices = J::array();
    for (std::size_t index = 0; index < library.channels.size(); ++index)
        if (library.channels[index].type == Monstey_Event)
            choices.as_array().push_back(J::object({{"index", static_cast<std::uint64_t>(index)},
                                                    {"type_hash", Monstey_Hex(library.channels[index].type)},
                                                    {"bone_hash", Monstey_Hex(library.channels[index].bone)}}));
    J result = J::object({{"events", J::array()},
                          {"event_channel_index", nullptr},
                          {"event_edit_supported", true},
                          {"event_channel_choices", choices}});
    auto unsupported = [&](const Monstey_Error &error)
    {
        if (result["event_edit_supported"].as_bool())
        {
            result["event_error"] = error.code;
            result["event_message"] = error.what();
        }
        result["event_edit_supported"] = false;
    };
    if (clip.segments.empty())
    {
        result["empty_native_clip"] = true;
        unsupported(Monstey_Error(Monstey_AEmptyClipCode, Monstey_AEmptyClipMessage));
        return result;
    }
    try
    {
        auto events = Monstey_AClipEvents(clip);
        std::stable_sort(events.as_array().begin(), events.as_array().end(),
                         [](const auto &a, const auto &b) { return a["tick"].as_uint() < b["tick"].as_uint(); });
        result["events"] = std::move(events);
    }
    catch (const Monstey_Error &error)
    {
        unsupported(error);
    }
    try
    {
        if (const auto channel = Monstey_AClipEventChannel(library, clip))
            result["event_channel_index"] = *channel;
    }
    catch (const Monstey_Error &error)
    {
        unsupported(error);
    }
    if (choices.empty() && result["event_edit_supported"].as_bool())
        unsupported(Monstey_Error("missing_anix_event_channel",
                                  "No shared native Event channel is available for event editing"));
    return result;
}

} // namespace Monstey
