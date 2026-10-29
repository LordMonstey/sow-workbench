#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
using namespace Monstey_AnixPrivate;

J Monstey_Anix::Describe() const
{
    const auto &l = *state_;
    J clips = J::array(), channels = J::array(), unqualified_variants = J::array();
    std::map<std::string, std::uint64_t> codec_counts;
    std::uint64_t blocks = 0, segments = 0, frames = 0, markers = 0, events = 0;
    std::uint64_t decoded_blocks = 0, auxiliary_blocks = 0;
    bool raw_word_float_views_finite = true;
    std::set<std::string> unknown, auxiliary_authoring;
    for (std::size_t i = 0; i < l.channels.size(); ++i)
        channels.as_array().push_back(J::object({{"index", static_cast<std::uint64_t>(i)},
                                                 {"type_hash", Monstey_Hex(l.channels[i].type)},
                                                 {"bone_hash", Monstey_Hex(l.channels[i].bone)}}));
    for (const auto &c : l.clips)
    {
        std::set<std::uint16_t> unique_track_channels;
        for (const auto &track : c.tracks)
            unique_track_channels.insert(track.channel);
        const auto unsupported = Monstey_AUnsupportedAuxiliary(l, c);
        auxiliary_authoring.insert(unsupported.begin(), unsupported.end());
        J ss = J::array(), codes = J::array(), extras = J::array();
        for (const auto &e : c.extras)
            extras.as_array().push_back(J::object({{"hash", Monstey_Hex(e.hash)},
                                                   {"bytes", static_cast<std::uint64_t>(e.bytes.size())},
                                                   {"sha256", Monstey_HashSha256(e.bytes)}}));
        for (const auto &code : c.codecs)
            codes.as_array().emplace_back(code);
        for (const auto &s : c.segments)
        {
            J bb = J::array(), se = J::array();
            for (std::size_t k = 0; k < c.codecs.size(); ++k)
            {
                const auto &code = c.codecs[k];
                const auto n = Monstey_ACodecTracks(c, k);
                ++codec_counts[code];
                ++blocks;
                if (Monstey_APoseCodec(code) || Monstey_AAuxiliaryCodec(code) || code == "STRI")
                {
                    const std::array<double, 1> t{static_cast<double>(s.first)};
                    try
                    {
                        const auto decoded =
                            Monstey_AnixDecodeCodec(code, s.blocks[k], n, s.ticks, t, Monstey_AKeyStrings(c));
                        if (!Monstey_ACodecBindingsQualified(l, c, k))
                        {
                            unknown.insert(code);
                            if (Monstey_AAuxiliaryCodec(code))
                                auxiliary_authoring.insert(code);
                            unqualified_variants.as_array().push_back(J::object(
                                {{"clip", c.name},
                                 {"codec", code},
                                 {"first_tick", s.first},
                                 {"last_tick", s.last},
                                 {"error", "unsupported_anix_channel_binding"},
                                 {"message", "Native codec binding has an unqualified shared channel type"}}));
                        }
                        else
                        {
                            ++decoded_blocks;
                            if (Monstey_AAuxiliaryCodec(code))
                                ++auxiliary_blocks;
                            if (code == "USFT")
                                for (const auto &value : decoded[0].as_array())
                                    raw_word_float_views_finite =
                                        raw_word_float_views_finite && !value["float32"].is_null();
                        }
                    }
                    catch (const Monstey_Error &e)
                    {
                        if (e.code == "unsupported_anix_codec_variant")
                        {
                            unknown.insert(code);
                            unqualified_variants.as_array().push_back(J::object({{"clip", c.name},
                                                                                 {"codec", code},
                                                                                 {"first_tick", s.first},
                                                                                 {"last_tick", s.last},
                                                                                 {"error", e.code},
                                                                                 {"message", e.what()}}));
                        }
                        else
                        {
                            throw Monstey_Error(e.code, c.name + " / " + code + ": " + e.what());
                        }
                    }
                }
                else
                    unknown.insert(code);
                bb.as_array().push_back(J::object({{"codec", code},
                                                   {"tracks", static_cast<std::uint64_t>(n)},
                                                   {"bytes", static_cast<std::uint64_t>(s.blocks[k].size())},
                                                   {"sha256", Monstey_HashSha256(s.blocks[k])}}));
            }
            for (const auto &e : s.extras)
                se.as_array().push_back(J::object({{"hash", Monstey_Hex(e.hash)},
                                                   {"bytes", static_cast<std::uint64_t>(e.bytes.size())},
                                                   {"sha256", Monstey_HashSha256(e.bytes)}}));
            ss.as_array().push_back(J::object({{"first_tick", s.first},
                                               {"last_tick", s.last},
                                               {"bytes", static_cast<std::uint64_t>(s.raw.size())},
                                               {"sha256", Monstey_HashSha256(s.raw)},
                                               {"lith_samples", static_cast<std::uint64_t>(s.ticks.size())},
                                               {"blocks", bb},
                                               {"extras", se}}));
        }
        segments += c.segments.size();
        frames += c.footer.frames;
        markers += c.footer.markers.size();
        for (const auto &m : c.footer.markers)
            events += m.event.has_value() ? 1 : 0;
        clips.as_array().push_back(J::object(
            {{"name", c.name},
             {"hash", Monstey_Hex(c.hash)},
             {"duration_ticks", c.duration},
             {"flags", c.flags},
             {"tracks", static_cast<std::uint64_t>(c.tracks.size())},
             {"unique_track_channels", static_cast<std::uint64_t>(unique_track_channels.size())},
             {"repeated_channel_bindings", static_cast<std::uint64_t>(c.tracks.size() - unique_track_channels.size())},
             {"empty_native_clip", c.segments.empty()},
             {"sample_supported", !c.segments.empty()},
             {"codecs", codes},
             {"segments", ss},
             {"descriptor_extras", extras},
             {"footer_frames", c.footer.frames},
             {"footer_markers", static_cast<std::uint64_t>(c.footer.markers.size())}}));
    }
    J counts = J::object(), unsupported = J::array(), unsupported_authoring = J::array();
    for (const auto &[k, v] : codec_counts)
        counts[k] = v;
    for (const auto &codec : auxiliary_authoring)
        unsupported_authoring.as_array().emplace_back(codec);
    for (const auto &c : unknown)
        unsupported.as_array().emplace_back(c);
    return J::object({{"format", "ANIX44"},
                      {"source_sha256", Monstey_HashSha256(l.original)},
                      {"library_path", l.path},
                      {"channel_count", static_cast<std::uint64_t>(l.channels.size())},
                      {"clip_count", static_cast<std::uint64_t>(l.clips.size())},
                      {"segment_count", segments},
                      {"codec_blocks", blocks},
                      {"channels", channels},
                      {"clips", clips},
                      {"codec_counts", counts},
                      {"opaque_codecs", unsupported},
                      {"unqualified_variants", unqualified_variants},
                      {"unsupported_authoring_codecs", unsupported_authoring},
                      {"auxiliary_authoring_supported", unsupported_authoring.empty()},
                      {"complete_channel_decode", unknown.empty()},
                      {"qualified_codecs_finite", raw_word_float_views_finite},
                      {"qualified_codec_blocks", decoded_blocks},
                      {"opaque_codec_blocks", blocks - decoded_blocks},
                      {"auxiliary_codec_blocks", auxiliary_blocks},
                      {"raw_word_float_views_finite", raw_word_float_views_finite},
                      {"footer_total_frames", frames},
                      {"footer_total_markers", markers},
                      {"footer_total_event_markers", events},
                      {"footer_arena_bytes", 8 * (events + markers + 10 * l.clips.size())},
                      {"directory_footer_consistent", true},
                      {"changed", l.changed},
                      {"runtime_verified", false}});
}

} // namespace Monstey
