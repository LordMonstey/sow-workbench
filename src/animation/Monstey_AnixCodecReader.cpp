#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
namespace Monstey_AnixPrivate
{

std::vector<Monstey_AUncompressedTrack> Monstey_AReadUncompressed(std::span<const std::uint8_t> bytes,
                                                                  std::size_t tracks, std::size_t width)
{
    Monstey_ARequire(tracks > 0 && tracks <= UINT16_MAX / 2 && (width == 3 || width == 4),
                     "Invalid UAQT/UAVT track count");
    Monstey_ARange(bytes, 0, 2 * tracks);
    std::vector<Monstey_AUncompressedTrack> layout;
    layout.reserve(tracks);
    std::size_t keys = 0;
    for (std::size_t track = 0; track < tracks; ++track)
    {
        const auto record = static_cast<std::size_t>(Monstey_AU16(bytes, 2 * track));
        Monstey_ARequire(record >= 2 * tracks, "UAQT/UAVT record overlaps its offset table");
        const auto count = static_cast<std::size_t>(Monstey_AU32(bytes, record));
        Monstey_ARequire(count >= 2 && count <= Monstey_SampleLimit - keys,
                         "Invalid UAQT/UAVT key count or decoded allocation limit");
        Monstey_ARange(bytes, record + 4, count * (width + 1) * 4);
        Monstey_AUncompressedTrack row;
        row.data = record + 4 + 4 * count;
        row.ticks.reserve(count);
        for (std::size_t key = 0; key < count; ++key)
        {
            const auto tick = Monstey_AU32(bytes, record + 4 + 4 * key);
            Monstey_ARequire(!key || tick > row.ticks.back(), "UAQT/UAVT ticks must strictly increase");
            row.ticks.push_back(tick);
            for (std::size_t component = 0; component < width; ++component)
                Monstey_ARequire(std::isfinite(Monstey_AF32(bytes, row.data + 4 * (key * width + component))),
                                 "UAQT/UAVT contains a nonfinite raw component");
        }
        keys += count;
        layout.push_back(std::move(row));
    }
    return layout;
}

Monstey_AAv01Layout Monstey_AReadAv01(std::span<const std::uint8_t> bytes, std::size_t tracks)
{
    Monstey_ARange(bytes, 0, 16);
    const auto start = Monstey_AU32(bytes, 0), end = Monstey_AU32(bytes, 4), count = Monstey_AU32(bytes, 8);
    Monstey_ARequire(tracks > 0 && tracks <= 65535 && count >= 2 && count <= Monstey_SampleLimit / tracks,
                     "Invalid AV01 key/track count or decoded allocation limit");
    Monstey_ARange(bytes, 16, static_cast<std::size_t>(count) * 2);
    Monstey_AAv01Layout layout;
    std::uint64_t tick = start;
    for (std::size_t key = 0; key < count; ++key)
    {
        const auto delta = Monstey_AU16(bytes, 16 + 2 * key);
        Monstey_ARequire(key ? delta > 0 : delta == 0, "Invalid AV01 key delta");
        tick += delta;
        layout.ticks.push_back(Monstey_A32(tick));
    }
    Monstey_ARequire(tick == end, "AV01 key deltas do not reach the declared end tick");
    // The native evaluator ignores word12 and rounds an absolute row address
    // down to 16 bytes. Its owning segment stores this block at a 16-byte boundary.
    layout.data = Monstey_AAlign(16 + 2 * static_cast<std::size_t>(count), 16);
    layout.stride = Monstey_AAlign(tracks, 4);
    Monstey_ARange(bytes, layout.data, static_cast<std::size_t>(count) * layout.stride * 4);
    return layout;
}

Monstey_ACaf4Layout Monstey_AReadCaf4(std::span<const std::uint8_t> bytes, std::size_t tracks)
{
    Monstey_ARange(bytes, 0, 4);
    if (Monstey_AU32(bytes, 0) != 1)
        throw Monstey_Error("unsupported_anix_codec_variant", "CAF4 mode is not the qualified native mode 1");
    Monstey_ARange(bytes, 0, 20);
    const auto start = Monstey_AU32(bytes, 4), end = Monstey_AU32(bytes, 8);
    const auto count = Monstey_AU32(bytes, 12), stride = Monstey_AU32(bytes, 16);
    Monstey_ARequire(tracks > 0 && tracks <= 65535 && count >= 2 && count <= Monstey_SampleLimit / tracks &&
                         stride >= tracks && stride <= Monstey_Limit / 4 && count <= Monstey_Limit / (4 * stride),
                     "Invalid CAF4 key count, row stride or decoded allocation limit");
    Monstey_ARange(bytes, 20, static_cast<std::size_t>(count) * 2);
    Monstey_ACaf4Layout layout;
    std::uint64_t tick = start;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto delta = Monstey_AU16(bytes, 20 + 2 * i);
        Monstey_ARequire(i ? delta > 0 : delta == 0, "Invalid CAF4 key delta");
        tick += delta;
        layout.ticks.push_back(Monstey_A32(tick));
    }
    Monstey_ARequire(tick == end, "CAF4 key deltas do not reach the declared end tick");
    // The qualified CAF4 block starts at a native 16-byte boundary. The
    // evaluator aligns (block + 35 + 2 * count) down to 16.
    layout.data = Monstey_AAlign(20 + 2 * static_cast<std::size_t>(count), 16);
    layout.stride = stride;
    Monstey_ARange(bytes, layout.data, static_cast<std::size_t>(count) * stride * 4);
    for (std::size_t key = 0; key < count; ++key)
        for (std::size_t track = 0; track < tracks; ++track)
            Monstey_ARequire(std::isfinite(Monstey_AF32(bytes, layout.data + 4 * (key * stride + track))),
                             "CAF4 contains a nonfinite sampled float32 value");
    return layout;
}

} // namespace Monstey_AnixPrivate
using namespace Monstey_AnixPrivate;

std::vector<Q> Monstey_AnixUnpackRow(std::span<const std::uint8_t> b, std::size_t tracks, bool check_padding)
{
    Monstey_ARequire(tracks <= 65535, "Quaternion track limit");
    const auto words = (tracks + 2) / 3;
    Monstey_ARange(b, 0, words * 16);
    std::vector<Q> out;
    for (std::size_t g = 0; g < words; ++g)
        for (unsigned s = 0; s < 3; ++s)
        {
            if (g * 3 + s >= tracks && !check_padding)
                continue;
            std::uint64_t w = 0;
            for (unsigned bit = 0; bit < 42; ++bit)
            {
                const auto src = 42 * s + bit;
                if (b[g * 16 + src / 8] & (1u << (src % 8)))
                    w |= 1ull << bit;
            }
            auto q = Monstey_AUnpackQuaternion(w);
            if (g * 3 + s < tracks)
                out.push_back(q);
        }
    return out;
}

Monstey_AnixQuaternionKeys Monstey_AnixDecodeAq03(std::span<const std::uint8_t> b, std::size_t tracks)
{
    Monstey_ARange(b, 0, 12);
    const auto start = Monstey_AU32(b, 0), end = Monstey_AU32(b, 4), count = Monstey_AU32(b, 8);
    Monstey_ARequire(count >= 2 && count <= Monstey_SampleLimit && tracks > 0 && tracks <= 65535 &&
                         count <= Monstey_SampleLimit / tracks,
                     "Invalid AQ03 key count or decoded allocation limit");
    Monstey_ARange(b, 12, static_cast<std::size_t>(count) * 2);
    Monstey_AnixQuaternionKeys v;
    std::uint64_t tick = start;
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto d = Monstey_AU16(b, 12 + 2 * i);
        Monstey_ARequire(i ? d > 0 : d == 0, "Invalid AQ03 delta");
        tick += d;
        v.ticks.push_back(Monstey_A32(tick));
    }
    Monstey_ARequire(tick == end, "AQ03 final tick differs from end");
    const auto data = Monstey_AAlign(12 + 2 * count, 16), stride = (tracks + 2) / 3 * 16;
    Monstey_ARequire(count <= Monstey_Limit / std::max<std::size_t>(stride, 1), "AQ03 allocation limit");
    Monstey_ARange(b, data, static_cast<std::size_t>(count) * stride);
    for (std::size_t i = 0; i < count; ++i)
        v.values.push_back(Monstey_AnixUnpackRow(b.subspan(data + i * stride, stride), tracks, true));
    Monstey_AZero(b, data + count * stride);
    return v;
}

J Monstey_AnixDecodeCodec(std::string_view codec, std::span<const std::uint8_t> b, std::size_t tracks,
                          std::span<const std::uint32_t> lith, std::span<const double> times,
                          std::span<const std::uint8_t> strings)
{
    Monstey_ARequire(b.size() <= Monstey_Limit && tracks <= 65535 && times.size() <= Monstey_SampleLimit &&
                         (!tracks || times.size() <= Monstey_SampleLimit / tracks),
                     "Codec sample allocation limit");
    for (double t : times)
        Monstey_ARequire(std::isfinite(t), "Nonfinite sample time");
    J out = J::array();
    if (codec == "STRI")
    {
        Monstey_ARange(b, 0, 10);
        Monstey_ARequire(Monstey_AU16(b, 0) == 2 && Monstey_AU16(b, 8) == 0, "Invalid STRI header");
        const auto n = Monstey_AU32(b, 4);
        Monstey_ARequire(n <= Monstey_SampleLimit, "STRI event allocation limit");
        Monstey_ARange(b, 10, static_cast<std::size_t>(n) * 8);
        Monstey_ARequire(Monstey_AU16(b, 2) <= strings.size(), "STRI declared string bytes exceed KeyString");
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto p = 10 + 8 * i;
            const auto at = Monstey_AU16(b, p + 4), len = Monstey_AU16(b, p + 6);
            Monstey_ARequire(len > 0, "Zero STRI string length");
            Monstey_ARange(strings, at, len);
            Monstey_ARequire(strings[static_cast<std::size_t>(at) + len - 1] == 0, "STRI text missing terminator");
            auto text = Monstey_AEventString(strings, at, static_cast<std::size_t>(at) + len);
            out.as_array().push_back(Monstey_AEventJson(Monstey_AU32(b, p), text));
        }
        Monstey_AZero(b, 10 + static_cast<std::size_t>(n) * 8);
        return out;
    }
    if (codec == "AQ03")
    {
        auto v = Monstey_AnixDecodeAq03(b, tracks);
        for (double t : times)
        {
            auto k = Monstey_ABracketAt(v.ticks, t);
            J row = J::array();
            for (std::size_t i = 0; i < tracks; ++i)
                row.as_array().push_back(Monstey_AArray(Monstey_ANlerp(v.values[k.lo][i], v.values[k.hi][i], k.f)));
            out.as_array().push_back(std::move(row));
        }
        return out;
    }
    if (codec == "UAQT" || codec == "UAVT")
    {
        const auto width = codec == "UAQT" ? 4u : 3u;
        const auto layout = Monstey_AReadUncompressed(b, tracks, width);
        for (const double time : times)
        {
            Monstey_ARequire(time >= 0 && time <= UINT32_MAX && time == std::floor(time),
                             "UAQT/UAVT sampling requires an integer u32 native tick");
            J row = J::array();
            for (const auto &track : layout)
            {
                const auto bracket = Monstey_AUncompressedBracketAt(track.ticks, static_cast<std::uint32_t>(time));
                Q lower{}, upper{};
                for (std::size_t component = 0; component < width; ++component)
                {
                    lower[component] = Monstey_AF32(b, track.data + 4 * (bracket.lo * width + component));
                    upper[component] = Monstey_AF32(b, track.data + 4 * (bracket.hi * width + component));
                }
                if (codec == "UAQT")
                    row.as_array().push_back(Monstey_AArray(Monstey_ANativeUaqt(lower, upper, bracket.fraction)));
                else
                {
                    V value{};
                    for (std::size_t component = 0; component < value.size(); ++component)
                    {
                        const float difference = upper[component] - lower[component];
                        const float weighted = difference * bracket.fraction;
                        value[component] = lower[component] + weighted;
                        Monstey_ARequire(std::isfinite(value[component]),
                                         "UAVT interpolation produced a nonfinite float32");
                    }
                    row.as_array().push_back(Monstey_AArray(value));
                }
            }
            out.as_array().push_back(std::move(row));
        }
        return out;
    }
    if (codec == "SQ03")
    {
        auto v = Monstey_AnixUnpackRow(b, tracks, true);
        Monstey_AZero(b, (tracks + 2) / 3 * 16);
        J row = J::array();
        for (const auto &q : v)
            row.as_array().push_back(Monstey_AArray(q));
        for (std::size_t i = 0; i < times.size(); ++i)
            out.as_array().push_back(row);
        return out;
    }
    if (codec == "AV01")
    {
        const auto layout = Monstey_AReadAv01(b, tracks);
        for (double time : times)
        {
            const auto sample =
                std::clamp(time, static_cast<double>(layout.ticks.front()), static_cast<double>(layout.ticks.back()));
            const auto bracket = Monstey_ABracketAt(layout.ticks, sample);
            const float numerator = Monstey_AFloat(sample - layout.ticks[bracket.lo]);
            const float denominator = static_cast<float>(layout.ticks[bracket.hi] - layout.ticks[bracket.lo]);
            const float fraction = numerator / denominator;
            J row = J::array();
            for (std::size_t track = 0; track < tracks; ++track)
            {
                const auto lower = Monstey_AU32(b, layout.data + 4 * (bracket.lo * layout.stride + track));
                const auto upper = Monstey_AU32(b, layout.data + 4 * (bracket.hi * layout.stride + track));
                V value{};
                for (unsigned component = 0; component < 3; ++component)
                {
                    const auto shift = 2 + 10 * component;
                    const float a = static_cast<float>((lower >> shift) & 1023u) / 1022.f * 512.f - 256.f;
                    const float z = static_cast<float>((upper >> shift) & 1023u) / 1022.f * 512.f - 256.f;
                    const float difference = z - a;
                    const float weighted = difference * fraction;
                    value[component] = a + weighted;
                }
                row.as_array().push_back(Monstey_AArray(value));
            }
            out.as_array().push_back(std::move(row));
        }
        return out;
    }
    if (codec == "SV01")
    {
        Monstey_ARange(b, 0, tracks * 4);
        J row = J::array();
        for (std::size_t i = 0; i < tracks; ++i)
        {
            auto w = Monstey_AU32(b, i * 4);
            V v{};
            for (unsigned c = 0; c < 3; ++c)
                v[c] = static_cast<float>((w >> (2 + 10 * c)) & 1023) / 1022.f * 512.f - 256.f;
            row.as_array().push_back(Monstey_AArray(v));
        }
        for (std::size_t i = 0; i < times.size(); ++i)
            out.as_array().push_back(row);
        return out;
    }

    if (codec == "USFT" || codec == "USBT")
    {
        Monstey_ARange(b, 0, tracks * 2);
        J row = J::array();
        for (std::size_t track = 0; track < tracks; ++track)
        {
            const auto offset = Monstey_AU16(b, 2 * track);
            Monstey_ARange(b, offset, codec == "USFT" ? 4 : 1);
            if (codec == "USFT")
            {
                const auto bits = Monstey_AU32(b, offset);
                const auto view = std::bit_cast<float>(bits);
                row.as_array().push_back(J::object(
                    {{"raw_u32", bits}, {"float32", std::isfinite(view) ? J(static_cast<double>(view)) : J(nullptr)}}));
            }
            else
            {
                // The native callback copies one byte. It does not canonicalize
                // the value to bool, and does not copy the remaining slot bytes.
                row.as_array().emplace_back(static_cast<unsigned int>(b[offset]));
            }
        }
        for (std::size_t i = 0; i < times.size(); ++i)
            out.as_array().push_back(row);
        return out;
    }
    if (codec == "CAF4")
    {
        const auto layout = Monstey_AReadCaf4(b, tracks);
        for (double time : times)
        {
            const auto sample =
                std::clamp(time, static_cast<double>(layout.ticks.front()), static_cast<double>(layout.ticks.back()));
            const auto bracket = Monstey_ABracketAt(layout.ticks, sample);
            const float numerator = Monstey_AFloat(sample - layout.ticks[bracket.lo]);
            const float denominator = static_cast<float>(layout.ticks[bracket.hi] - layout.ticks[bracket.lo]);
            const float fraction = numerator / denominator;
            J row = J::array();
            for (std::size_t track = 0; track < tracks; ++track)
            {
                const auto lower = Monstey_AF32(b, layout.data + 4 * (bracket.lo * layout.stride + track));
                const auto upper = Monstey_AF32(b, layout.data + 4 * (bracket.hi * layout.stride + track));
                const float difference = upper - lower;
                const float weighted = difference * fraction;
                const float value = lower + weighted;
                Monstey_ARequire(std::isfinite(value), "CAF4 interpolation produced a nonfinite float32");
                row.as_array().emplace_back(static_cast<double>(value));
            }
            out.as_array().push_back(std::move(row));
        }
        return out;
    }

    if (codec == "UATR")
    {
        Monstey_ARequire(lith.size() >= 2 &&
                             std::adjacent_find(lith.begin(), lith.end(), std::greater_equal<>()) == lith.end(),
                         "UATR needs strictly increasing LITH ticks");
        Monstey_ARange(b, 0, tracks * 2);
        std::vector<std::size_t> offsets;
        for (std::size_t i = 0; i < tracks; ++i)
            offsets.push_back(Monstey_AU16(b, i * 2));
        offsets.push_back(b.size());
        if (tracks)
            Monstey_ARequire(offsets.front() >= Monstey_AAlign(tracks * 2), "UATR track table overlaps data");
        struct Track
        {
            std::vector<std::array<std::uint8_t, 2>> codes;
            std::vector<V> controls;
        };
        std::vector<Track> decoded;
        for (std::size_t i = 0; i < tracks; ++i)
        {
            const auto first = offsets[i], last = offsets[i + 1], control = Monstey_AAlign(first + 2 * lith.size());
            Monstey_ARequire(first % 4 == 0 && first <= b.size() && last <= b.size() && last >= control &&
                                 control <= b.size(),
                             "Invalid UATR track extent");
            Monstey_ARange(b, first, 2 * lith.size());
            const auto count = (last - control) / 12;
            Monstey_ARequire(count >= 2, "UATR needs two controls");
            Track tr;
            for (std::size_t k = 0; k < lith.size(); ++k)
            {
                auto idx = b[first + 2 * k];
                Monstey_ARequire(static_cast<std::size_t>(idx) + 1 < count, "UATR control index outside track");
                tr.codes.push_back({idx, b[first + 2 * k + 1]});
            }
            for (std::size_t k = 0; k < count; ++k)
            {
                V v{};
                for (std::size_t c = 0; c < 3; ++c)
                {
                    v[c] = Monstey_AF32(b, control + 12 * k + 4 * c);
                    Monstey_ARequire(std::isfinite(v[c]), "Nonfinite UATR control");
                }
                tr.controls.push_back(v);
            }
            Monstey_ARequire(
                std::all_of(b.begin() + control + 12 * count, b.begin() + last, [](auto z) { return z == 0; }),
                "Nonzero UATR trailing bytes");
            decoded.push_back(std::move(tr));
        }
        for (double t : times)
        {
            auto k = Monstey_ABracketAt(lith, t);
            J row = J::array();
            for (const auto &tr : decoded)
            {
                auto lo = tr.codes[k.lo], hi = tr.codes[k.hi];
                const float a = lo[1] / 255.f, z = lo[0] == hi[0] ? hi[1] / 255.f : 1.f,
                            w = a + (z - a) * static_cast<float>(k.f);
                V v{};
                for (std::size_t c = 0; c < 3; ++c)
                    v[c] = tr.controls[lo[0]][c] + (tr.controls[lo[0] + 1][c] - tr.controls[lo[0]][c]) * w;
                row.as_array().push_back(Monstey_AArray(v));
            }
            out.as_array().push_back(std::move(row));
        }
        return out;
    }
    throw Monstey_Error("unsupported_anix_codec",
                        "ANIX codec is retained but has no qualified native decoder: " + std::string(codec));
}

} // namespace Monstey
