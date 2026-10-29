#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
namespace Monstey_AnixPrivate
{

std::vector<double> Monstey_ASampleTicks(const Monstey_AClip &c)
{
    std::set<std::uint32_t> t;
    for (const auto &s : c.segments)
    {
        t.insert(s.first);
        t.insert(s.last);
        t.insert(s.ticks.begin(), s.ticks.end());
        for (std::size_t k = 0; k < c.codecs.size(); ++k)
            if (c.codecs[k] == "AQ03")
            {
                auto a = Monstey_AnixDecodeAq03(s.blocks[k], Monstey_ACodecTracks(c, k));
                t.insert(a.ticks.begin(), a.ticks.end());
            }
            else if (c.codecs[k] == "UAQT" || c.codecs[k] == "UAVT")
            {
                const auto layout =
                    Monstey_AReadUncompressed(s.blocks[k], Monstey_ACodecTracks(c, k), c.codecs[k] == "UAQT" ? 4u : 3u);
                for (const auto &track : layout)
                    t.insert(track.ticks.begin(), track.ticks.end());
            }
            else if (c.codecs[k] == "AV01")
            {
                const auto layout = Monstey_AReadAv01(s.blocks[k], Monstey_ACodecTracks(c, k));
                t.insert(layout.ticks.begin(), layout.ticks.end());
            }
            else if (c.codecs[k] == "CAF4")
            {
                const auto layout = Monstey_AReadCaf4(s.blocks[k], Monstey_ACodecTracks(c, k));
                t.insert(layout.ticks.begin(), layout.ticks.end());
            }
    }
    Monstey_ARequire(t.size() <= Monstey_SampleLimit, "Clip sample set exceeds limit");
    return std::vector<double>(t.begin(), t.end());
}

bool Monstey_APoseCodec(std::string_view c)
{
    return c == "AQ03" || c == "SQ03" || c == "UATR" || c == "SV01" || c == "AV01" || c == "UAQT" || c == "UAVT";
}

bool Monstey_AAuxiliaryCodec(std::string_view codec)
{
    return codec == "CAF4" || codec == "USFT" || codec == "USBT";
}

std::string_view Monstey_AChannelCodec(const Monstey_AClip &clip, std::uint16_t channel)
{
    for (const auto &track : clip.tracks)
        if (track.channel == channel)
            return clip.codecs[track.codec];
    return {};
}

std::uint32_t Monstey_AAuxiliaryWord(const J &value)
{
    Monstey_ARequire(value.is_object() && value.contains("raw_u32"), "USFT authoring needs an exact raw_u32 word");
    return Monstey_A32(value.at("raw_u32").as_uint());
}

std::uint8_t Monstey_AAuxiliaryByte(const J &value)
{
    const auto byte = value.as_uint();
    Monstey_ARequire(byte <= 255, "USBT authoring needs an unsigned byte 0..255");
    return static_cast<std::uint8_t>(byte);
}

bool Monstey_AStaticAuxiliaryEqual(std::string_view codec, const J &a, const J &b)
{
    return codec == "USFT" ? Monstey_AAuxiliaryWord(a) == Monstey_AAuxiliaryWord(b)
                           : Monstey_AAuxiliaryByte(a) == Monstey_AAuxiliaryByte(b);
}

bool Monstey_ACodecBindingsQualified(const Monstey_ALibrary &library, const Monstey_AClip &clip, std::size_t codec)
{
    const auto &name = clip.codecs[codec];
    const auto type = Monstey_AAuxiliaryCodec(name)                                          ? Monstey_Auxiliary
                      : name == "AQ03" || name == "SQ03" || name == "UAQT"                   ? Monstey_Rotation
                      : name == "UATR" || name == "SV01" || name == "AV01" || name == "UAVT" ? Monstey_Position
                      : name == "STRI"                                                       ? Monstey_Event
                                                                                             : 0u;
    return type && std::all_of(clip.tracks.begin(), clip.tracks.end(),
                               [&](const auto &track)
                               {
                                   return track.codec != codec || (track.channel < library.channels.size() &&
                                                                   library.channels[track.channel].type == type);
                               });
}

std::set<std::string> Monstey_AUnsupportedAuxiliary(const Monstey_ALibrary &library, const Monstey_AClip &clip)
{
    std::set<std::string> unsupported;
    for (std::size_t codec = 0; codec < clip.codecs.size(); ++codec)
    {
        const auto &name = clip.codecs[codec];
        if (!Monstey_AAuxiliaryCodec(name))
            continue;
        if (!Monstey_ACodecBindingsQualified(library, clip, codec))
        {
            unsupported.insert(name);
            continue;
        }
        J baseline;
        bool have = false;
        for (const auto &segment : clip.segments)
        {
            if (name == "CAF4")
            {
                try
                {
                    (void)Monstey_AReadCaf4(segment.blocks[codec], Monstey_ACodecTracks(clip, codec));
                }
                catch (const Monstey_Error &error)
                {
                    if (error.code != "unsupported_anix_codec_variant")
                        throw;
                    unsupported.insert(name);
                }
                continue;
            }
            const std::array<double, 1> tick{static_cast<double>(segment.first)};
            auto values =
                Monstey_AnixDecodeCodec(name, segment.blocks[codec], Monstey_ACodecTracks(clip, codec), {}, tick)[0];
            try
            {
                if (name == "USFT")
                {
                    std::vector<std::uint32_t> words;
                    for (const auto &value : values.as_array())
                        words.push_back(Monstey_AAuxiliaryWord(value));
                    (void)Monstey_AnixEncodeUsft(words);
                }
                else
                {
                    B bytes;
                    for (const auto &value : values.as_array())
                        bytes.push_back(Monstey_AAuxiliaryByte(value));
                    (void)Monstey_AnixEncodeUsbt(bytes);
                }
            }
            catch (const Monstey_Error &error)
            {
                if (error.code != "anix")
                    throw;
                unsupported.insert(name);
            }
            if (have)
            {
                for (std::size_t track = 0; track < values.size(); ++track)
                    if (!Monstey_AStaticAuxiliaryEqual(name, values[track], baseline[track]))
                        unsupported.insert(name);
            }
            else
            {
                baseline = std::move(values);
                have = true;
            }
        }
    }
    return unsupported;
}

void Monstey_AApply(const Monstey_ALibrary &l, const Monstey_AClip &c, std::span<const double> ticks,
                    std::vector<Monstey_AFrame> &frames, std::set<std::string> &ignored)
{
    for (std::size_t si = 0; si < c.segments.size(); ++si)
    {
        const auto &s = c.segments[si];
        std::vector<double> local;
        std::vector<std::size_t> indices;
        for (std::size_t i = 0; i < ticks.size(); ++i)
            if (ticks[i] >= s.first && ticks[i] <= s.last && (si == 0 || ticks[i] > s.first))
            {
                local.push_back(ticks[i]);
                indices.push_back(i);
            }
        if (local.empty())
            continue;
        for (std::size_t k = 0; k < c.codecs.size(); ++k)
        {
            const auto &code = c.codecs[k];
            if (code == "STRI")
                continue;
            if (!Monstey_APoseCodec(code) && !Monstey_AAuxiliaryCodec(code))
            {
                ignored.insert(code);
                continue;
            }
            std::vector<std::uint16_t> channels;
            for (auto tr : c.tracks)
                if (tr.codec == k)
                {
                    const auto type = l.channels[tr.channel].type;
                    if (Monstey_AAuxiliaryCodec(code))
                        Monstey_ARequire(type == Monstey_Auxiliary,
                                         "Auxiliary codec binding has an unqualified native channel type");
                    else
                        Monstey_ARequire(type == ((code == "AQ03" || code == "SQ03" || code == "UAQT")
                                                      ? Monstey_Rotation
                                                      : Monstey_Position),
                                         "Pose codec bound to wrong channel type");
                    channels.push_back(tr.channel);
                }
            if (channels.empty())
                continue;
            auto rows = Monstey_AnixDecodeCodec(code, s.blocks[k], channels.size(), s.ticks, local);
            for (std::size_t i = 0; i < local.size(); ++i)
                for (std::size_t tr = 0; tr < channels.size(); ++tr)
                    frames[indices[i]][channels[tr]] = rows[i][tr];
        }
    }
}

J Monstey_ASample(const Monstey_ALibrary &l, const Monstey_AClip &c, std::span<const double> ticks, bool fallback)
{
    Monstey_ARequireSegments(c);
    Monstey_ARequire(!ticks.empty() && ticks.size() <= Monstey_SampleLimit &&
                         ticks.size() <= Monstey_SampleLimit / std::max<std::size_t>(l.channels.size(), 1),
                     "Pose allocation limit");
    for (std::size_t i = 0; i < ticks.size(); ++i)
        Monstey_ARequire(std::isfinite(ticks[i]) && ticks[i] >= c.segments.front().first &&
                             ticks[i] <= c.segments.back().last && (!i || ticks[i] > ticks[i - 1]),
                         "Sample ticks must increase within clip bounds");
    std::vector<Monstey_AFrame> frames(ticks.size());
    std::set<std::string> ignored, unsupported_authoring;
    unsupported_authoring = Monstey_AUnsupportedAuxiliary(l, c);
    const Monstey_AClip *bind = nullptr;
    if (fallback)
    {
        Monstey_ARequire((c.flags & 1) == 0, "Alternate bind buffer policy is not qualified");
        bind = &l.clips[Monstey_AFind(l, "bind")];
        Monstey_ARequireSegments(*bind);
        const auto unsupported_bind = Monstey_AUnsupportedAuxiliary(l, *bind);
        unsupported_authoring.insert(unsupported_bind.begin(), unsupported_bind.end());
        std::vector<Monstey_AFrame> one(1);
        const std::array<double, 1> t{static_cast<double>(bind->segments.front().first)};
        Monstey_AApply(l, *bind, t, one, ignored);
        for (auto &f : frames)
            f = one.front();
    }
    Monstey_AApply(l, c, ticks, frames, ignored);
    std::set<std::uint16_t> channels;
    for (const auto &f : frames)
        for (const auto &[ch, v] : f)
        {
            (void)v;
            channels.insert(ch);
        }
    J tracks = J::array(), outtimes = J::array(), unhandled = J::array(), authoring = J::array();
    for (double t : ticks)
        outtimes.as_array().emplace_back(t);
    for (auto ch : channels)
    {
        J values = J::array();
        bool complete = true;
        for (const auto &f : frames)
        {
            auto it = f.find(ch);
            if (it == f.end())
            {
                values.as_array().emplace_back(nullptr);
                complete = false;
            }
            else
                values.as_array().push_back(it->second);
        }
        auto row = J::object({{"channel_index", ch},
                              {"type_hash", Monstey_Hex(l.channels[ch].type)},
                              {"bone_hash", Monstey_Hex(l.channels[ch].bone)},
                              {"values", values},
                              {"present_at_all_samples", complete}});
        auto codec = Monstey_AChannelCodec(c, ch);
        if (codec.empty() && bind)
            codec = Monstey_AChannelCodec(*bind, ch);
        if (codec == "UAQT" || codec == "UAVT")
            row["native_codec"] = codec;
        if (Monstey_AAuxiliaryCodec(codec))
        {
            if (codec == "USFT" || codec == "USBT")
                for (const auto &value : values.as_array())
                    if (value.is_null() || values[0].is_null() ||
                        !Monstey_AStaticAuxiliaryEqual(codec, value, values[0]))
                        unsupported_authoring.insert(std::string(codec));
            row["native_role"] = "auxiliary";
            row["native_codec"] = codec;
            row["native_value_kind"] = codec == "USFT"   ? "word32_with_float32_view"
                                       : codec == "USBT" ? "uint8"
                                                         : "float32";
            J bits = J::array();
            for (const auto &value : values.as_array())
            {
                if (value.is_null())
                    bits.as_array().emplace_back(nullptr);
                else if (codec == "USFT")
                    bits.as_array().emplace_back(Monstey_Hex(value["raw_u32"].as_uint()));
                else if (codec == "USBT")
                    bits.as_array().emplace_back(Monstey_Hex(value.as_uint(), 2));
                else
                    bits.as_array().emplace_back(
                        Monstey_Hex(std::bit_cast<std::uint32_t>(Monstey_AFloat(value.as_number()))));
            }
            row["native_bits"] = std::move(bits);
        }
        tracks.as_array().push_back(std::move(row));
    }
    for (const auto &x : ignored)
        unhandled.as_array().emplace_back(x);
    for (const auto &x : unsupported_authoring)
        authoring.as_array().emplace_back(x);
    return J::object({{"clip", c.name},
                      {"ticks", outtimes},
                      {"tracks", tracks},
                      {"ignored_codecs", unhandled},
                      {"unsupported_authoring_codecs", authoring},
                      {"auxiliary_authoring_supported", authoring.empty()},
                      {"complete_pose_codecs", ignored.empty()},
                      {"complete_channel_decode", ignored.empty()},
                      {"bind_fallback_applied", fallback},
                      {"space", "native_y_up_cm_xyzw"},
                      {"runtime_verified", false}});
}

} // namespace Monstey_AnixPrivate
using namespace Monstey_AnixPrivate;

J Monstey_Anix::Sample(std::string_view name, std::span<const double> ticks, bool fallback) const
{
    return Monstey_ASample(*state_, state_->clips[Monstey_AFind(*state_, name)], ticks, fallback);
}

} // namespace Monstey
