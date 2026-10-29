#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
namespace Monstey_AnixPrivate
{

B Monstey_ABuildFooter(const Monstey_ALibrary &l)
{
    B names = l.footer_names;
    auto add_name = [&](std::string_view n)
    {
        auto at = Monstey_A32(names.size());
        names.insert(names.end(), n.begin(), n.end());
        names.push_back(0);
        return at;
    };
    if (names.empty())
        add_name(l.path);
    else
        Monstey_ARequire(Monstey_AString(names, 0, names.size()) == l.path,
                         "Preserved footer path differs from library path");
    std::vector<std::uint32_t> name_offsets;
    for (const auto &c : l.clips)
    {
        // The original names area may contain unused names or opaque padding.
        // Retain it verbatim and reuse this clip's validated original offset.
        if (!l.footer_names.empty() && c.footer.name_offset < l.footer_names.size() &&
            Monstey_AString(l.footer_names, c.footer.name_offset, l.footer_names.size()) == c.name)
            name_offsets.push_back(c.footer.name_offset);
        else
            name_offsets.push_back(add_name(c.name));
    }
    auto strings = l.event_strings;
    for (const auto &c : l.clips)
        for (const auto &m : c.footer.markers)
            if (m.event && std::find(strings.begin(), strings.end(), *m.event) == strings.end())
                strings.push_back(*m.event);
    if (strings.size() > 255)
    {
        std::set<std::string> used;
        for (const auto &c : l.clips)
            for (const auto &m : c.footer.markers)
                if (m.event)
                    used.insert(*m.event);
        strings.erase(std::remove_if(strings.begin(), strings.end(), [&](const auto &x) { return !used.contains(x); }),
                      strings.end());
    }
    Monstey_ARequire(strings.size() <= 255, "Library exceeds 255 unique event strings");
    const auto event_at = Monstey_A32(names.size());
    for (const auto &s : strings)
    {
        Monstey_AEventValidate(s);
        names.insert(names.end(), s.begin(), s.end());
        names.push_back(0);
    }
    const auto stream_at = Monstey_A32(names.size());
    B stream = l.footer_header;
    Monstey_ARequire(stream.size() == 41, "Invalid saved footer stream header");
    Monstey_PutU32(stream, 0, stream_at - 5);
    std::uint64_t frames = 0, markers = 0, events = 0;
    for (std::size_t i = 0; i < l.clips.size(); ++i)
    {
        const auto &r = l.clips[i].footer;
        for (float f : r.extents)
            Monstey_AFAdd(stream, f);
        Monstey_AppendU32(stream, name_offsets[i]);
        for (auto u : r.unknown)
            Monstey_AppendU32(stream, u);
        Monstey_AppendU32(stream, r.frames);
        Monstey_AppendU32(stream, Monstey_A32(r.markers.size()));
        frames += r.frames;
        markers += r.markers.size();
        for (const auto &m : r.markers)
        {
            Monstey_ARequire(m.mark < r.frames, "Footer mark exceeds generated time grid");
            Monstey_AppendU32(stream, m.mark);
        }
        for (const auto &m : r.markers)
        {
            Monstey_ARequire(m.tick <= 0xffffffu, "Footer marker tick exceeds 24 bits");
            std::uint32_t tag = 255;
            if (m.event)
            {
                tag = Monstey_A32(
                    static_cast<std::size_t>(std::find(strings.begin(), strings.end(), *m.event) - strings.begin()));
                ++events;
            }
            Monstey_AppendU32(stream, (tag << 24) | m.tick);
        }
    }
    Monstey_AAppend(stream, l.footer_trailer);
    B out;
    for (auto h : {stream_at, Monstey_A32(frames), Monstey_A32(events), Monstey_A32(markers), event_at,
                   Monstey_A32(stream.size())})
        Monstey_AppendU32(out, h);
    Monstey_AAppend(out, names);
    Monstey_AAppend(out, stream);
    return out;
}

B Monstey_ABuildDescriptor(const Monstey_AClip &c, const std::vector<std::uint32_t> &offsets)
{
    if (!c.descriptor_rebuild)
    {
        auto d = c.descriptor;
        if (Monstey_AString(d, 0, 48) != c.name)
        {
            std::fill(d.begin(), d.begin() + 48, std::uint8_t{0});
            std::copy(c.name.begin(), c.name.end(), d.begin());
        }
        const auto nt = c.tracks.size(), ids = Monstey_AAlign(124 + 4 * c.codecs.size() + nt),
                   at = Monstey_AAlign(ids + 2 * nt);
        for (std::size_t i = 0; i < offsets.size(); ++i)
            Monstey_PutU32(d, at + 16 * i, offsets[i]);
        return d;
    }
    B d(48);
    std::copy(c.name.begin(), c.name.end(), d.begin());
    Monstey_AppendU16(d, Monstey_A16(124 + 16 * c.segments.size() + 8 * c.extras.size()));
    for (auto n : {c.flags, Monstey_A16(c.segments.size()), Monstey_A16(c.extras.size()), Monstey_A16(c.codecs.size()),
                   Monstey_A16(c.tracks.size())})
        Monstey_AppendU16(d, n);
    for (float f : c.root_start)
        Monstey_AFAdd(d, f);
    for (float f : c.root_end)
        Monstey_AFAdd(d, f);
    Monstey_AppendU32(d, c.unknown116);
    Monstey_AppendU32(d, c.duration);
    for (const auto &s : c.codecs)
        d.insert(d.end(), s.begin(), s.end());
    for (auto t : c.tracks)
        d.push_back(t.codec);
    Monstey_APad(d);
    for (auto t : c.tracks)
        Monstey_AppendU16(d, t.channel);
    Monstey_APad(d);
    for (std::size_t i = 0; i < c.segments.size(); ++i)
    {
        const auto &s = c.segments[i];
        for (auto v : {offsets[i], Monstey_A32(s.raw.size()), s.first, s.last})
            Monstey_AppendU32(d, v);
    }
    const auto extra_at = d.size();
    d.resize(d.size() + 8 * c.extras.size());
    for (std::size_t i = 0; i < c.extras.size(); ++i)
    {
        Monstey_APad(d);
        Monstey_PutU32(d, extra_at + 8 * i, c.extras[i].hash);
        Monstey_PutU32(d, extra_at + 8 * i + 4, Monstey_A32(d.size()));
        Monstey_AAppend(d, c.extras[i].bytes);
    }
    Monstey_APad(d, 16);
    return d;
}

B Monstey_ABuild(const Monstey_ALibrary &l)
{
    if (!l.changed)
        return l.original;
    Monstey_ARequire(!l.clips.empty() && l.clips.size() <= 65535, "Library must contain clips");
    auto payload = l.payload;
    std::vector<B> descriptors;
    for (const auto &c : l.clips)
    {
        std::vector<std::uint32_t> offsets;
        for (const auto &s : c.segments)
        {
            if (!s.appended)
                offsets.push_back(s.old_offset);
            else
            {
                Monstey_APad(payload, 16);
                offsets.push_back(Monstey_A32(payload.size()));
                Monstey_AAppend(payload, s.raw);
                Monstey_APad(payload, 16);
            }
        }
        descriptors.push_back(Monstey_ABuildDescriptor(c, offsets));
    }
    // Retained native descriptors may occupy only 124 bytes. Padding every
    // record to sixteen would silently enlarge their owned opaque extent.
    const auto metadata_alignment = std::any_of(descriptors.begin(), descriptors.end(),
                                                [](const auto &descriptor) { return descriptor.size() % 16 != 0; })
                                        ? 4u
                                        : 16u;
    const auto file_bias = (48 + l.channel_prefix.size() + payload.size() + l.section_gap.size()) % 16;
    const auto metadata_bias = file_bias % 4 == 0 ? file_bias : 0u;
    B meta(16 + 8 * l.clips.size());
    Monstey_PutU32(meta, 0, 16);
    Monstey_PutU32(meta, 4, Monstey_A32(l.clips.size()));
    Monstey_PutU32(meta, 8, l.metadata_unknown);
    meta.resize(Monstey_AAlign(meta.size() + metadata_bias, metadata_alignment) - metadata_bias, 0);
    for (std::size_t i = 0; i < l.clips.size(); ++i)
    {
        Monstey_PutU32(meta, 16 + 8 * i, l.clips[i].hash);
        Monstey_PutU32(meta, 20 + 8 * i, Monstey_A32(meta.size()));
        Monstey_AAppend(meta, descriptors[i]);
        meta.resize(Monstey_AAlign(meta.size() + metadata_bias, metadata_alignment) - metadata_bias, 0);
    }
    Monstey_PutU32(meta, 12, Monstey_A32(meta.size()));
    Monstey_AAppend(meta, Monstey_ABuildFooter(l));
    B out(48);
    std::copy_n("ANIX", 4, out.begin());
    Monstey_PutU32(out, 4, 44);
    Monstey_PutU32(out, 8, 48);
    Monstey_PutU32(out, 16, Monstey_A32(l.channels.size()));
    Monstey_PutU32(out, 20, l.channel_start);
    Monstey_AAppend(out, l.channel_prefix);
    Monstey_PutU32(out, 24, Monstey_A32(out.size()));
    Monstey_PutU32(out, 28, Monstey_A32(payload.size()));
    Monstey_AAppend(out, payload);
    Monstey_AAppend(out, l.section_gap);
    Monstey_PutU32(out, 32, Monstey_A32(out.size()));
    Monstey_PutU32(out, 36, Monstey_A32(meta.size()));
    Monstey_PutU32(out, 40, l.header_unknown[0]);
    Monstey_PutU32(out, 44, l.header_unknown[1]);
    Monstey_AAppend(out, meta);
    Monstey_PutU32(out, 12, Monstey_A32(out.size()));
    (void)Monstey_AParse(out);
    return out;
}

} // namespace Monstey_AnixPrivate
using namespace Monstey_AnixPrivate;

B Monstey_Anix::Build() const
{
    return Monstey_ABuild(*state_);
}

} // namespace Monstey
