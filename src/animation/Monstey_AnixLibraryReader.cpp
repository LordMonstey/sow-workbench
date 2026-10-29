#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
namespace Monstey_AnixPrivate
{

void Monstey_ARequireSegments(const Monstey_AClip &clip)
{
    if (clip.segments.empty())
        throw Monstey_Error(Monstey_AEmptyClipCode, Monstey_AEmptyClipMessage);
}

std::size_t Monstey_ACodecTracks(const Monstey_AClip &c, std::size_t codec)
{
    return static_cast<std::size_t>(
        std::count_if(c.tracks.begin(), c.tracks.end(), [&](auto t) { return t.codec == codec; }));
}

std::span<const std::uint8_t> Monstey_AKeyStrings(const Monstey_AClip &c)
{
    for (const auto &e : c.extras)
        if (e.hash == Monstey_KeyString)
            return e.bytes;
    return {};
}

Monstey_ASegment Monstey_AReadSegment(std::span<const std::uint8_t> raw, std::size_t codec_count, std::uint32_t first,
                                      std::uint32_t last, std::uint32_t offset)
{
    Monstey_ASegment s;
    s.raw = B(raw.begin(), raw.end());
    s.first = first;
    s.last = last;
    s.old_offset = offset;
    const auto e = Monstey_AU32(raw, 0);
    Monstey_ARequire(e <= 256, "Excessive ANIX segment extras");
    const auto total = codec_count + e, hash_at = Monstey_AAlign(4 + 2 * total), head = hash_at + 4 * e;
    Monstey_ARange(raw, 4, 2 * total);
    Monstey_ARange(raw, hash_at, 4 * e);
    std::vector<std::size_t> bounds;
    for (std::size_t i = 0; i < total; ++i)
    {
        auto off = Monstey_AU16(raw, 4 + 2 * i);
        Monstey_ARequire(off >= head && off <= raw.size(), "Segment offset overlaps header or exceeds segment");
        if (!bounds.empty())
            Monstey_ARequire(bounds.back() <= off, "Segment offsets are not ordered");
        bounds.push_back(off);
    }
    bounds.push_back(raw.size());
    for (std::size_t i = 0; i < codec_count; ++i)
        s.blocks.push_back(Monstey_ASlice(raw, bounds[i], bounds[i + 1] - bounds[i]));
    std::set<std::uint32_t> hashes;
    for (std::size_t i = 0; i < e; ++i)
    {
        const auto h = Monstey_AU32(raw, hash_at + 4 * i);
        Monstey_ARequire(hashes.insert(h).second, "Duplicate segment extra hash");
        s.extras.push_back(
            {h, Monstey_ASlice(raw, bounds[codec_count + i], bounds[codec_count + i + 1] - bounds[codec_count + i])});
        if (h == Monstey_Lith)
        {
            const auto &b = s.extras.back().bytes;
            const auto n = Monstey_AU32(b, 0), a = Monstey_AU32(b, 4), z = Monstey_AU32(b, 8);
            Monstey_ARequire(n > 0 && n <= Monstey_SampleLimit, "Invalid LITH sample count");
            Monstey_ARange(b, 12, static_cast<std::size_t>(n) * 4);
            for (std::size_t k = 0; k < n; ++k)
                s.ticks.push_back(Monstey_AU32(b, 12 + k * 4));
            Monstey_ARequire(s.ticks.front() == first && s.ticks.back() == last &&
                                 std::is_sorted(s.ticks.begin(), s.ticks.end()),
                             "LITH ticks disagree with segment");
            const std::uint64_t aux = static_cast<std::uint64_t>(a) * 6 + z;
            Monstey_ARequire(aux <= b.size() - 12 - 4 * n, "LITH auxiliary extent exceeds block");
        }
    }
    return s;
}

void Monstey_AReadFooter(Monstey_ALibrary &l, std::span<const std::uint8_t> f)
{
    Monstey_ARange(f, 0, 24);
    std::array<std::uint32_t, 6> h{};
    for (std::size_t i = 0; i < 6; ++i)
        h[i] = Monstey_AU32(f, 4 * i);
    const auto body = f.subspan(24);
    Monstey_ARequire(h[4] <= h[0] && h[0] >= 5, "Invalid footer string table");
    Monstey_ARange(body, h[0], h[5]);
    Monstey_ARequire(static_cast<std::size_t>(h[0]) + h[5] == body.size(), "Footer stream does not end file");
    Monstey_ARequire(h[4] > 0 && h[5] >= 53, "Invalid footer size");
    l.path = Monstey_AString(body, 0, h[4]);
    l.footer_names = Monstey_ASlice(body, 0, h[4]);
    std::size_t cur = h[4];
    while (cur < h[0])
    {
        auto text = Monstey_AEventString(body, cur, h[0]);
        cur += text.size() + 1;
        l.event_strings.push_back(std::move(text));
    }
    Monstey_ARequire(l.event_strings.size() <= 255, "Footer has more than 255 event strings");
    Monstey_ARequire(Monstey_AU32(body, h[0]) == h[0] - 5, "Footer stream header size is inconsistent");
    l.footer_header = Monstey_ASlice(body, h[0], 41);
    cur = static_cast<std::size_t>(h[0]) + 41;
    std::uint64_t frames = 0, markers = 0, events = 0;
    for (auto &c : l.clips)
    {
        Monstey_ARange(body, cur, 40);
        auto &r = c.footer;
        for (std::size_t k = 0; k < 3; ++k)
        {
            r.extents[k] = Monstey_AF32(body, cur + 4 * k);
            Monstey_ARequire(std::isfinite(r.extents[k]), "Nonfinite footer extent");
        }
        const auto name_offset = Monstey_AU32(body, cur + 12);
        auto name = Monstey_AString(body, name_offset, h[4]);
        Monstey_ARequire(name == c.name, "Footer clip order/name differs from directory");
        r.name_offset = name_offset;
        for (std::size_t k = 0; k < 4; ++k)
            r.unknown[k] = Monstey_AU32(body, cur + 16 + 4 * k);
        r.frames = Monstey_AU32(body, cur + 32);
        const auto n = Monstey_AU32(body, cur + 36);
        Monstey_ARequire(r.frames > 0 && n > 0 && n <= Monstey_SampleLimit, "Invalid footer frame or marker count");
        Monstey_ARange(body, cur + 40, static_cast<std::size_t>(n) * 8);
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto mark = Monstey_AU32(body, cur + 40 + k * 4), word = Monstey_AU32(body, cur + 40 + 4 * n + k * 4),
                       tag = word >> 24;
            Monstey_ARequire(mark < r.frames, "Footer marker index exceeds time grid");
            Monstey_AMarker m{mark, word & 0xffffffu, std::nullopt};
            if (tag != 255)
            {
                Monstey_ARequire(tag < l.event_strings.size(), "Footer marker tag outside string table");
                m.event = l.event_strings[tag];
                ++events;
            }
            r.markers.push_back(std::move(m));
        }
        frames += r.frames;
        markers += n;
        cur += 40 + static_cast<std::size_t>(n) * 8;
    }
    Monstey_ARequire(frames == h[1] && events == h[2] && markers == h[3],
                     "Footer arena totals disagree with native records");
    Monstey_ARequire(cur <= body.size() && body.size() - cur >= 12, "Footer trailer is truncated");
    l.footer_trailer = Monstey_ASlice(body, cur, body.size() - cur);
}

Monstey_ALibrary Monstey_AParse(std::span<const std::uint8_t> b)
{
    Monstey_ARequire(b.size() <= Monstey_Limit && b.size() >= 48, "ANIX size outside limits");
    Monstey_ARequire(std::equal(b.begin(), b.begin() + 4, "ANIX") && Monstey_AU32(b, 4) == 44 &&
                         Monstey_AU32(b, 8) == 48,
                     "Expected ANIX44");
    Monstey_ARequire(Monstey_AU32(b, 12) == b.size(), "ANIX declared size differs");
    const auto count = Monstey_AU32(b, 16), start = Monstey_AU32(b, 20), payload = Monstey_AU32(b, 24),
               psize = Monstey_AU32(b, 28), meta = Monstey_AU32(b, 32), msize = Monstey_AU32(b, 36);
    Monstey_ARequire(count > 0 && count <= 65535 && start >= 48 && start <= payload && payload % 16 == 0,
                     "Invalid ANIX channels/payload alignment");
    Monstey_ARequire(static_cast<std::uint64_t>(payload) + psize <= meta &&
                         static_cast<std::uint64_t>(meta) + msize == b.size(),
                     "ANIX section extents differ");
    Monstey_ALibrary l;
    l.original = B(b.begin(), b.end());
    l.channel_start = start;
    l.header_unknown = {Monstey_AU32(b, 40), Monstey_AU32(b, 44)};
    l.channel_prefix = Monstey_ASlice(b, 48, payload - 48);
    l.payload = Monstey_ASlice(b, payload, psize);
    l.section_gap =
        Monstey_ASlice(b, static_cast<std::size_t>(payload) + psize, meta - static_cast<std::size_t>(payload) - psize);
    std::size_t cur = start;
    while (l.channels.size() < count)
    {
        const auto type = Monstey_AU32(b, cur), n = Monstey_AU32(b, cur + 4);
        cur += 8;
        Monstey_ARequire(n > 0 && n <= count - l.channels.size() && cur <= payload && 4ull * n <= payload - cur,
                         "Invalid ANIX channel group");
        for (std::size_t k = 0; k < n; ++k)
            l.channels.push_back({type, Monstey_AU32(b, cur + 4 * k)});
        cur += 4 * n;
    }
    const auto metadata = b.subspan(meta, msize);
    const auto table = Monstey_AU32(metadata, 0), nclips = Monstey_AU32(metadata, 4),
               footer = Monstey_AU32(metadata, 12);
    l.metadata_unknown = Monstey_AU32(metadata, 8);
    Monstey_ARequire(table == 16 && nclips > 0 && nclips <= 65535 && 16ull + 8ull * nclips <= footer && footer < msize,
                     "Invalid ANIX metadata directory");
    std::map<std::size_t, std::size_t> limits;
    std::set<std::uint32_t> clip_hashes;
    std::vector<std::size_t> positions;
    for (std::size_t i = 0; i < nclips; ++i)
    {
        const auto pos = Monstey_AU32(metadata, table + 8 * i + 4);
        Monstey_ARequire(pos % 4 == 0 && pos >= 16ull + 8ull * nclips && pos < footer, "Descriptor outside metadata");
        positions.push_back(pos);
    }
    auto sorted = positions;
    std::sort(sorted.begin(), sorted.end());
    Monstey_ARequire(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end(),
                     "Duplicate clip descriptor offset");
    for (std::size_t i = 0; i < sorted.size(); ++i)
        limits.emplace(sorted[i], i + 1 < sorted.size() ? sorted[i + 1] : footer);
    for (std::size_t i = 0; i < nclips; ++i)
    {
        Monstey_AClip c;
        const auto pos = positions[i], end = limits.at(pos);
        Monstey_ARange(metadata, pos, 124);
        Monstey_ARequire(pos + 124 <= end, "Truncated clip descriptor");
        c.descriptor = Monstey_ASlice(metadata, pos, end - pos);
        c.old_descriptor = pos;
        auto d = std::span<const std::uint8_t>(c.descriptor);
        c.name = Monstey_AString(d, 0, 48);
        Monstey_AName(c.name, true);
        c.hash = Monstey_AU32(metadata, table + 8 * i);
        Monstey_ARequire(c.hash == Monstey_AHash(c.name) && clip_hashes.insert(c.hash).second,
                         "Clip hash mismatch/collision");
        c.flags = Monstey_AU16(d, 50);
        const auto ns = Monstey_AU16(d, 52), ne = Monstey_AU16(d, 54), nc = Monstey_AU16(d, 56),
                   nt = Monstey_AU16(d, 58);
        const bool empty = ns == 0 && ne == 0 && nc == 0 && nt == 0;
        Monstey_ARequire((empty || (ns > 0 && nc > 0)) && nc <= 32 &&
                             Monstey_AU16(d, 48) == 124 + 16ull * ns + 8ull * ne,
                         "Invalid clip counts/header size");
        for (std::size_t k = 0; k < 7; ++k)
        {
            c.root_start[k] = Monstey_AF32(d, 60 + 4 * k);
            c.root_end[k] = Monstey_AF32(d, 88 + 4 * k);
        }
        c.unknown116 = Monstey_AU32(d, 116);
        c.duration = Monstey_AU32(d, 120);
        const auto codec_end = 124 + 4ull * nc, ids = Monstey_AAlign(codec_end + nt),
                   segment_at = Monstey_AAlign(ids + 2ull * nt), extra_at = segment_at + 16ull * ns;
        Monstey_ARange(d, 124, 4ull * nc + nt);
        Monstey_ARange(d, ids, 2ull * nt);
        Monstey_ARange(d, segment_at, 16ull * ns + 8ull * ne);
        std::set<std::string> codes;
        for (std::size_t k = 0; k < nc; ++k)
        {
            std::string code(reinterpret_cast<const char *>(d.data() + 124 + 4 * k), 4);
            Monstey_ARequire(
                std::all_of(code.begin(), code.end(), [](unsigned char z) { return z >= 32 && z < 127; }) &&
                    codes.insert(code).second,
                "Invalid/repeated codec tag");
            c.codecs.push_back(std::move(code));
        }
        for (std::size_t k = 0; k < nt; ++k)
        {
            const auto codec = d[codec_end + k];
            const auto channel = Monstey_AU16(d, ids + 2 * k);
            // Native dispatch keeps every codec-local row in source order;
            // repeated valid IDs intentionally alias the same output pointer.
            Monstey_ARequire(codec < nc && channel < count, "Invalid track codec/channel");
            c.tracks.push_back({codec, channel});
        }
        std::vector<std::size_t> extra_offsets;
        std::set<std::uint32_t> extra_hashes;
        for (std::size_t k = 0; k < ne; ++k)
        {
            const auto eo = Monstey_AU32(d, extra_at + 8 * k + 4);
            Monstey_ARequire(eo >= extra_at + 8ull * ne && eo < d.size(), "Descriptor extra outside descriptor");
            extra_offsets.push_back(eo);
        }
        auto exsorted = extra_offsets;
        std::sort(exsorted.begin(), exsorted.end());
        Monstey_ARequire(std::adjacent_find(exsorted.begin(), exsorted.end()) == exsorted.end(),
                         "Descriptor extras alias");
        for (std::size_t k = 0; k < ne; ++k)
        {
            const auto h = Monstey_AU32(d, extra_at + 8 * k);
            Monstey_ARequire(extra_hashes.insert(h).second, "Duplicate descriptor extra hash");
            auto next = std::upper_bound(exsorted.begin(), exsorted.end(), extra_offsets[k]);
            auto stop = next == exsorted.end() ? d.size() : *next;
            c.extras.push_back({h, Monstey_ASlice(d, extra_offsets[k], stop - extra_offsets[k])});
        }
        for (std::size_t k = 0; k < ns; ++k)
        {
            const auto at = segment_at + 16 * k;
            const auto off = Monstey_AU32(d, at), len = Monstey_AU32(d, at + 4), first = Monstey_AU32(d, at + 8),
                       last = Monstey_AU32(d, at + 12);
            Monstey_ARequire(off % 16 == 0 && len >= 4 && first <= last && (!k || first == c.segments.back().last),
                             "Invalid/noncontiguous clip segment");
            Monstey_ARange(l.payload, off, len);
            const auto segment_bytes = std::span<const std::uint8_t>(l.payload).subspan(off, len);
            auto parsed_segment = Monstey_AReadSegment(segment_bytes, nc, first, last, off);
            for (std::size_t ci = 0; ci < c.codecs.size(); ++ci)
            {
                const auto &code = c.codecs[ci];
                const bool caf4_mode1 = code == "CAF4" && parsed_segment.blocks[ci].size() >= 4 &&
                                        Monstey_AU32(parsed_segment.blocks[ci], 0) == 1;
                const auto align =
                    code == "AQ03" || code == "SQ03" || code == "SV01" || code == "AV01" || caf4_mode1 ? 16u : 4u;
                Monstey_ARequire(Monstey_AU16(segment_bytes, 4 + 2 * ci) % align == 0,
                                 "Native codec block is misaligned");
            }
            c.segments.push_back(std::move(parsed_segment));
        }
        l.clips.push_back(std::move(c));
    }
    Monstey_AReadFooter(l, metadata.subspan(footer));
    return l;
}

std::size_t Monstey_AFind(const Monstey_ALibrary &l, std::string_view name)
{
    const auto h = Monstey_AHash(name);
    for (std::size_t i = 0; i < l.clips.size(); ++i)
        if (l.clips[i].hash == h)
            return i;
    throw Monstey_Error("anix_clip_not_found", "Animation clip not found: " + std::string(name));
}

} // namespace Monstey_AnixPrivate

} // namespace Monstey
