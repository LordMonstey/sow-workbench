#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
namespace Monstey_AnixPrivate
{

template <std::size_t N> std::array<float, N> Monstey_AVec(const J &j)
{
    Monstey_ARequire(j.is_array() && j.size() == N, "Invalid ANIX vector width");
    std::array<float, N> a{};
    for (std::size_t i = 0; i < N; ++i)
        a[i] = Monstey_AFloat(j[i].as_number());
    return a;
}

B Monstey_AEncodeSegment(const std::vector<std::string> &codes, const std::vector<B> &blocks,
                         std::span<const std::uint32_t> ticks, const std::vector<Monstey_AExtra> &other_extras)
{
    std::vector<Monstey_AExtra> extras = other_extras;
    if (!ticks.empty())
    {
        B lith;
        Monstey_AppendU32(lith, Monstey_A32(ticks.size()));
        Monstey_AppendU32(lith, 0);
        Monstey_AppendU32(lith, 0);
        for (auto t : ticks)
            Monstey_AppendU32(lith, t);
        extras.push_back({Monstey_Lith, std::move(lith)});
    }
    const auto count = codes.size() + extras.size(), hash_at = Monstey_AAlign(4 + 2 * count);
    B out(hash_at + 4 * extras.size());
    Monstey_PutU32(out, 0, Monstey_A32(extras.size()));
    for (std::size_t i = 0; i < codes.size(); ++i)
    {
        Monstey_APad(out, codes[i] == "AQ03" || codes[i] == "SQ03" || codes[i] == "SV01" || codes[i] == "AV01" ||
                                  codes[i] == "CAF4"
                              ? 16
                              : 4);
        Monstey_PutU16(out, 4 + 2 * i, Monstey_A16(out.size()));
        Monstey_AAppend(out, blocks[i]);
    }
    for (std::size_t i = 0; i < extras.size(); ++i)
    {
        Monstey_APad(out);
        Monstey_PutU16(out, 4 + 2 * (codes.size() + i), Monstey_A16(out.size()));
        Monstey_PutU32(out, hash_at + 4 * i, extras[i].hash);
        Monstey_AAppend(out, extras[i].bytes);
    }
    Monstey_APad(out, 16);
    Monstey_ARequire(out.size() <= 65535, "Encoded segment exceeds 64 KiB; split the samples");
    return out;
}

Monstey_AClip Monstey_AImportClip(const Monstey_ALibrary &l, const J &scene)
{
    Monstey_ARequire(!scene.value("empty_native_clip", false).as_bool(),
                     "Empty native clips remain structural-only for semantic import");
    Monstey_ARequire(!scene.contains("complete_pose_codecs") || scene["complete_pose_codecs"].as_bool(),
                     "Semantic import cannot discard opaque source codecs");
    Monstey_ARequire(!scene.contains("ignored_codecs") || scene["ignored_codecs"].empty(),
                     "Semantic import contains opaque codecs");
    if (scene.contains("unsupported_authoring_codecs"))
        for (const auto &codec : scene["unsupported_authoring_codecs"].as_array())
            if (!Monstey_AAuxiliaryCodec(codec.as_string()))
                throw Monstey_Error("unsupported_anix_authoring", "Semantic source contains an unqualified codec");
    Monstey_AClip c;
    c.name = scene.at("name").as_string();
    Monstey_AName(c.name, true);
    c.hash = Monstey_AHash(c.name);
    c.duration = Monstey_AJsonU(scene.at("duration_ticks"));
    Monstey_ARequire(c.duration > 0 && c.duration <= 0xffffff, "Imported clips need duration 1..16777215 ms");
    c.flags = Monstey_A16(scene.value("flags", 0).as_uint());
    c.unknown116 = Monstey_AJsonU(scene.value("unknown116", 0));
    c.root_start = {0, 0, 0, 0, 0, 0, 1};
    c.root_end = c.root_start;
    const auto &jtimes = scene.at("ticks");
    Monstey_ARequire(jtimes.is_array() && jtimes.size() >= 2 && jtimes.size() <= Monstey_SampleLimit,
                     "Import needs 2..1000000 time samples");
    std::vector<std::uint32_t> ticks;
    for (const auto &t : jtimes.as_array())
    {
        const auto n = t.as_number();
        Monstey_ARequire(n == std::floor(n) && n >= 0 && n <= c.duration, "Import tick must be an integer in clip");
        ticks.push_back(static_cast<std::uint32_t>(n));
    }
    Monstey_ARequire(ticks.front() == 0 && ticks.back() == c.duration &&
                         std::adjacent_find(ticks.begin(), ticks.end(), std::greater_equal<>()) == ticks.end(),
                     "Import ticks must increase from 0 through duration");
    const auto &sourceTracks = scene.at("tracks");
    const auto events = scene.value("events", J::array());
    Monstey_ARequire(sourceTracks.is_array() && sourceTracks.size() <= Monstey_SampleLimit / ticks.size(),
                     "Semantic track/sample allocation limit");
    Monstey_ARequire(events.is_array(), "Events must be an array");
    if (sourceTracks.empty())
    {
        Monstey_ARequire(scene.contains("event_channel_index") || !events.empty(),
                         "Event-only semantic clips require a native Event binding or events");
        (void)Monstey_AResolveEventChannel(l, scene);
    }
    std::vector<Monstey_ASemanticTrack> rot, pos, rawrot, rawpos, scalar, words, bytes;
    std::set<std::uint16_t> seen;
    for (const auto &tr : sourceTracks.as_array())
    {
        const auto ch = Monstey_A16(tr.at("channel_index").as_uint());
        Monstey_ARequire(ch < l.channels.size() && seen.insert(ch).second, "Import channel is invalid/duplicate");
        const auto type = l.channels[ch].type;
        std::string auxiliary;
        if (type == Monstey_Auxiliary)
        {
            Monstey_ARequire(tr.contains("native_codec"), "Auxiliary authoring requires native_codec");
            auxiliary = tr.at("native_codec").as_string();
            Monstey_ARequire(Monstey_AAuxiliaryCodec(auxiliary), "Unqualified auxiliary authoring codec");
            Monstey_ARequire(!tr.contains("native_role") || tr.at("native_role").as_string() == "auxiliary",
                             "Auxiliary channel has a contradictory native_role");
        }
        else
            Monstey_ARequire(type == Monstey_Rotation || type == Monstey_Position,
                             "Semantic import channel type is not qualified");
        const auto requested = tr.contains("native_codec") ? tr.at("native_codec").as_string() : std::string{};
        if (type == Monstey_Rotation)
            Monstey_ARequire(requested.empty() || requested == "AQ03" || requested == "SQ03" || requested == "UAQT",
                             "Unqualified or contradictory rotation authoring codec");
        else if (type == Monstey_Position)
            Monstey_ARequire(requested.empty() || requested == "UATR" || requested == "SV01" || requested == "AV01" ||
                                 requested == "UAVT",
                             "Unqualified or contradictory position authoring codec");
        Monstey_ASemanticTrack track;
        track.channel = ch;
        const auto &values = tr.at("values");
        Monstey_ARequire(values.is_array() && values.size() == ticks.size(), "Imported values do not match times");
        for (const auto &value : values.as_array())
        {
            if (type == Monstey_Rotation)
            {
                auto q = Monstey_AVec<4>(value);
                if (requested != "UAQT")
                    (void)Monstey_APackQuaternion(q);
            }
            else if (type == Monstey_Position)
                (void)Monstey_AVec<3>(value);
            else if (auxiliary == "CAF4")
                (void)Monstey_AFloat(value.as_number());
            else if (auxiliary == "USFT")
                (void)Monstey_AAuxiliaryWord(value);
            else
                (void)Monstey_AAuxiliaryByte(value);
            track.values.push_back(value);
        }
        if (auxiliary == "USFT" || auxiliary == "USBT")
            for (const auto &value : track.values)
                Monstey_ARequire(Monstey_AStaticAuxiliaryEqual(auxiliary, value, track.values.front()),
                                 "Static USFT/USBT authoring cannot encode varying values");
        auto *collection = type == Monstey_Rotation   ? (requested == "UAQT" ? &rawrot : &rot)
                           : type == Monstey_Position ? (requested == "UAVT" ? &rawpos : &pos)
                           : auxiliary == "CAF4"      ? &scalar
                           : auxiliary == "USFT"      ? &words
                                                      : &bytes;
        collection->push_back(std::move(track));
    }
    // AQ03 and CAF4 store u16 deltas. Insert known interpolated samples across
    // larger gaps.
    if (!rot.empty() || !scalar.empty())
    {
        std::vector<std::uint32_t> expanded{ticks.front()};
        std::vector<std::pair<std::size_t, std::uint32_t>> mapping{{0, ticks.front()}};
        const auto budget = Monstey_SampleLimit / sourceTracks.size();
        for (std::size_t i = 1; i < ticks.size(); ++i)
        {
            std::uint64_t x = ticks[i - 1];
            while (static_cast<std::uint64_t>(ticks[i]) - x > 65535)
            {
                x += 65535;
                Monstey_ARequire(expanded.size() < budget, "Expanded key allocation limit");
                expanded.push_back(Monstey_A32(x));
                mapping.push_back({i - 1, Monstey_A32(x)});
            }
            Monstey_ARequire(expanded.size() < budget, "Expanded key allocation limit");
            expanded.push_back(ticks[i]);
            mapping.push_back({i - 1, ticks[i]});
        }
        if (expanded.size() != ticks.size())
        {
            for (auto *collection : {&rot, &pos, &rawrot, &rawpos, &scalar, &words, &bytes})
                for (auto &tr : *collection)
                {
                    std::vector<J> values;
                    for (const auto &[i, time] : mapping)
                    {
                        const auto next = std::min(i + 1, tr.values.size() - 1);
                        if (time == ticks[i])
                        {
                            values.push_back(tr.values[i]);
                            continue;
                        }
                        if (time == ticks[next])
                        {
                            values.push_back(tr.values[next]);
                            continue;
                        }
                        const double fraction = static_cast<double>(time - ticks[i]) / (ticks[next] - ticks[i]);
                        if (collection == &rot)
                            values.push_back(Monstey_AArray(Monstey_ANlerp(
                                Monstey_AVec<4>(tr.values[i]), Monstey_AVec<4>(tr.values[next]), fraction)));
                        else if (collection == &rawrot)
                        {
                            const float native_fraction =
                                static_cast<float>(time - ticks[i]) / static_cast<float>(ticks[next] - ticks[i]);
                            values.push_back(Monstey_AArray(Monstey_ANativeUaqt(
                                Monstey_AVec<4>(tr.values[i]), Monstey_AVec<4>(tr.values[next]), native_fraction)));
                        }
                        else if (collection == &pos || collection == &rawpos)
                        {
                            auto a = Monstey_AVec<3>(tr.values[i]), b = Monstey_AVec<3>(tr.values[next]);
                            V value{};
                            const float weight = collection == &rawpos ? static_cast<float>(time - ticks[i]) /
                                                                             static_cast<float>(ticks[next] - ticks[i])
                                                                       : static_cast<float>(fraction);
                            for (std::size_t k = 0; k < 3; ++k)
                            {
                                const float difference = b[k] - a[k];
                                const float weighted = difference * weight;
                                value[k] = Monstey_AFloat(a[k] + weighted);
                            }
                            values.push_back(Monstey_AArray(value));
                        }
                        else if (collection == &scalar)
                        {
                            const float a = Monstey_AFloat(tr.values[i].as_number());
                            const float b = Monstey_AFloat(tr.values[next].as_number());
                            const float f =
                                static_cast<float>(time - ticks[i]) / static_cast<float>(ticks[next] - ticks[i]);
                            const float difference = b - a;
                            const float weighted = difference * f;
                            values.emplace_back(static_cast<double>(Monstey_AFloat(a + weighted)));
                        }
                        else
                            values.push_back(tr.values[i]);
                    }
                    tr.values = std::move(values);
                }
            ticks = std::move(expanded);
        }
    }
    auto addCodec = [&](std::string_view name,
                        const std::vector<Monstey_ASemanticTrack> &tracks) -> std::optional<std::uint8_t>
    {
        if (tracks.empty())
            return std::nullopt;
        Monstey_ARequire(c.codecs.size() < 32, "Semantic codec limit");
        const auto codec = static_cast<std::uint8_t>(c.codecs.size());
        c.codecs.emplace_back(name);
        for (const auto &track : tracks)
            c.tracks.push_back({codec, track.channel});
        return codec;
    };
    const auto pcodec = addCodec("UATR", pos), qcodec = addCodec("AQ03", rot), uqcodec = addCodec("UAQT", rawrot),
               uvcodec = addCodec("UAVT", rawpos), fcodec = addCodec("CAF4", scalar), wcodec = addCodec("USFT", words),
               bcodec = addCodec("USBT", bytes);
    std::optional<std::uint8_t> ecodec;
    auto encoded_events = Monstey_AEncodeEvents(events);
    if (!events.empty() || scene.contains("event_channel_index"))
    {
        const auto event = Monstey_AResolveEventChannel(l, scene);
        ecodec = static_cast<std::uint8_t>(c.codecs.size());
        c.codecs.push_back("STRI");
        c.tracks.push_back({*ecodec, event});
        if (encoded_events.second.empty())
            encoded_events.second.push_back(0);
        c.extras.push_back({Monstey_KeyString, std::move(encoded_events.second)});
    }
    std::size_t begin = 0;
    while (begin + 1 < ticks.size())
    {
        std::size_t end = std::min(ticks.size() - 1, begin + 255);
        B raw;
        bool fit = false;
        while (end > begin)
        {
            try
            {
                std::vector<B> blocks(c.codecs.size());
                if (pcodec)
                {
                    std::vector<std::vector<V>> values;
                    for (std::size_t k = begin; k <= end; ++k)
                    {
                        std::vector<V> row;
                        for (const auto &track : pos)
                            row.push_back(Monstey_AVec<3>(track.values[k]));
                        values.push_back(std::move(row));
                    }
                    blocks[*pcodec] = Monstey_AnixEncodeUatr(values);
                }
                if (qcodec)
                {
                    Monstey_AnixQuaternionKeys keys;
                    keys.ticks.assign(ticks.begin() + begin, ticks.begin() + end + 1);
                    for (std::size_t k = begin; k <= end; ++k)
                    {
                        std::vector<Q> row;
                        for (const auto &track : rot)
                            row.push_back(Monstey_AVec<4>(track.values[k]));
                        keys.values.push_back(std::move(row));
                    }
                    blocks[*qcodec] = Monstey_AnixEncodeAq03(keys);
                }
                if (uqcodec)
                {
                    Monstey_AnixQuaternionKeys keys;
                    keys.ticks.assign(ticks.begin() + begin, ticks.begin() + end + 1);
                    for (std::size_t k = begin; k <= end; ++k)
                    {
                        std::vector<Q> row;
                        for (const auto &track : rawrot)
                            row.push_back(Monstey_AVec<4>(track.values[k]));
                        keys.values.push_back(std::move(row));
                    }
                    blocks[*uqcodec] = Monstey_AnixEncodeUaqt(keys);
                }
                if (uvcodec)
                {
                    std::vector<std::vector<V>> values;
                    for (std::size_t k = begin; k <= end; ++k)
                    {
                        std::vector<V> row;
                        for (const auto &track : rawpos)
                            row.push_back(Monstey_AVec<3>(track.values[k]));
                        values.push_back(std::move(row));
                    }
                    blocks[*uvcodec] = Monstey_AnixEncodeUavt(
                        std::span<const std::uint32_t>(ticks).subspan(begin, end - begin + 1), values);
                }
                if (fcodec)
                {
                    Monstey_AnixScalarKeys keys;
                    keys.ticks.assign(ticks.begin() + begin, ticks.begin() + end + 1);
                    for (std::size_t k = begin; k <= end; ++k)
                    {
                        std::vector<float> row;
                        for (const auto &track : scalar)
                            row.push_back(Monstey_AFloat(track.values[k].as_number()));
                        keys.values.push_back(std::move(row));
                    }
                    blocks[*fcodec] = Monstey_AnixEncodeCaf4(keys);
                }
                if (wcodec)
                {
                    std::vector<std::uint32_t> values;
                    for (const auto &track : words)
                        values.push_back(Monstey_AAuxiliaryWord(track.values[begin]));
                    blocks[*wcodec] = Monstey_AnixEncodeUsft(values);
                }
                if (bcodec)
                {
                    std::vector<std::uint8_t> values;
                    for (const auto &track : bytes)
                        values.push_back(Monstey_AAuxiliaryByte(track.values[begin]));
                    blocks[*bcodec] = Monstey_AnixEncodeUsbt(values);
                }
                if (ecodec)
                    blocks[*ecodec] = encoded_events.first;
                raw = Monstey_AEncodeSegment(c.codecs, blocks,
                                             std::span<const std::uint32_t>(ticks).subspan(begin, end - begin + 1));
                fit = true;
                break;
            }
            catch (const Monstey_Error &error)
            {
                if (error.code != "anix" || end == begin + 1)
                    throw;
                end = begin + std::max<std::size_t>(1, (end - begin) / 2);
            }
        }
        Monstey_ARequire(fit, "No valid segment split");
        auto segment = Monstey_AReadSegment(raw, c.codecs.size(), ticks[begin], ticks[end], 0);
        segment.appended = true;
        c.segments.push_back(std::move(segment));
        begin = end;
    }
    c.footer.extents = scene.contains("extents_cm") ? Monstey_AVec<3>(scene.at("extents_cm")) : V{40, 104, 40};
    Monstey_ARegrid(c, events);
    c.descriptor_rebuild = true;
    constexpr std::uint32_t root = 0x88baa7e7;
    for (const auto *collection : {&pos, &rot, &rawpos, &rawrot})
        for (const auto &track : *collection)
            if (l.channels[track.channel].bone == root)
            {
                const auto &first = track.values.front(), &last = track.values.back();
                const auto offset = collection == &pos || collection == &rawpos ? 0u : 3u;
                for (std::size_t k = 0; k < first.size(); ++k)
                {
                    c.root_start[offset + k] = Monstey_AFloat(first[k].as_number());
                    c.root_end[offset + k] = Monstey_AFloat(last[k].as_number());
                }
            }
    if (scene.contains("root_start"))
        c.root_start = Monstey_AVec<7>(scene.at("root_start"));
    if (scene.contains("root_end"))
        c.root_end = Monstey_AVec<7>(scene.at("root_end"));
    const std::vector<std::uint32_t> offsets(c.segments.size(), 0);
    (void)Monstey_ABuildDescriptor(c, offsets);
    return c;
}

} // namespace Monstey_AnixPrivate
using namespace Monstey_AnixPrivate;

Monstey_Anix Monstey_Anix::Create(const J &scene)
{
    auto state = std::make_unique<Monstey_State>();
    state->path = scene.at("library_path").as_string();
    Monstey_AName(state->path);
    const auto &channels = scene.at("channels");
    Monstey_ARequire(channels.is_array() && !channels.empty() && channels.size() <= 65535,
                     "Invalid shared ANIX channel table");
    std::vector<Monstey_AChannel> source_channels;
    std::vector<std::size_t> order;
    source_channels.reserve(channels.size());
    order.reserve(channels.size());
    for (const auto &ch : channels.as_array())
    {
        source_channels.push_back({Monstey_AHashJson(ch.at("type_hash")), Monstey_AHashJson(ch.at("bone_hash"))});
        order.push_back(order.size());
    }
    std::stable_sort(order.begin(), order.end(),
                     [&](const auto a, const auto b)
                     {
                         const auto &left = source_channels[a], &right = source_channels[b];
                         return left.type != right.type ? left.type < right.type : left.bone < right.bone;
                     });
    std::vector<std::uint16_t> source_to_native(order.size());
    for (const auto source_index : order)
    {
        source_to_native[source_index] = Monstey_A16(state->channels.size());
        const auto &channel = source_channels[source_index];
        Monstey_ARequire(state->channels.empty() || state->channels.back().type != channel.type ||
                             state->channels.back().bone != channel.bone,
                         "Duplicate shared ANIX channel identity");
        state->channels.push_back(channel);
    }
    const auto remap = [&](const J &index)
    {
        const auto source_index = index.as_uint();
        Monstey_ARequire(source_index < source_to_native.size(), "Import channel is outside the semantic table");
        return source_to_native[static_cast<std::size_t>(source_index)];
    };
    std::size_t at = 0;
    while (at < state->channels.size())
    {
        auto end = at + 1;
        while (end < state->channels.size() && state->channels[end].type == state->channels[at].type)
            ++end;
        Monstey_AppendU32(state->channel_prefix, state->channels[at].type);
        Monstey_AppendU32(state->channel_prefix, Monstey_A32(end - at));
        for (auto i = at; i < end; ++i)
            Monstey_AppendU32(state->channel_prefix, state->channels[i].bone);
        at = end;
    }
    while ((48 + state->channel_prefix.size()) % 16)
        state->channel_prefix.push_back(0);
    state->footer_header.resize(41);
    Monstey_PutU32(state->footer_header, 29, std::bit_cast<std::uint32_t>(1.f));
    state->footer_trailer.resize(12);
    for (const auto &source_clip : scene.at("clips").as_array())
    {
        auto clip = source_clip;
        for (auto &track : clip.at("tracks").as_array())
            track["channel_index"] = static_cast<std::uint64_t>(remap(track.at("channel_index")));
        if (clip.contains("event_channel_index"))
            clip["event_channel_index"] = static_cast<std::uint64_t>(remap(clip.at("event_channel_index")));
        state->clips.push_back(Monstey_AImportClip(*state, clip));
    }
    Monstey_AUnique(*state);
    (void)Monstey_AFind(*state, "bind");
    state->changed = true;
    auto raw = Monstey_ABuild(*state);
    return Monstey_Anix(raw);
}

} // namespace Monstey
