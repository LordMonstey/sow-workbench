#include "Monstey_AnixPrivate.hpp"

namespace Monstey
{
namespace Monstey_AnixPrivate
{

template <std::size_t Width>
B Monstey_AEncodeUncompressed(std::span<const std::uint32_t> ticks,
                              const std::vector<std::vector<std::array<float, Width>>> &values)
{
    Monstey_ARequire(ticks.size() >= 2 && ticks.size() == values.size(), "UAQT/UAVT needs at least two matching keys");
    const auto tracks = values.front().size();
    Monstey_ARequire(tracks > 0 && tracks <= UINT16_MAX / 2 && ticks.size() <= Monstey_SampleLimit / tracks,
                     "UAQT/UAVT track/key allocation limit");
    const auto record_size = 4 + ticks.size() * (Width + 1) * 4;
    Monstey_ARequire(record_size <= UINT16_MAX, "UAQT/UAVT record exceeds native block limit");
    for (std::size_t key = 0; key < ticks.size(); ++key)
        Monstey_ARequire(values[key].size() == tracks && (!key || ticks[key] > ticks[key - 1]),
                         "UAQT/UAVT key width/tick order mismatch");
    B output(2 * tracks);
    std::map<B, std::uint16_t> offsets;
    for (std::size_t track = 0; track < tracks; ++track)
    {
        B record;
        record.reserve(record_size);
        Monstey_AppendU32(record, Monstey_A32(ticks.size()));
        for (const auto tick : ticks)
            Monstey_AppendU32(record, tick);
        for (const auto &row : values)
            for (const float value : row[track])
                Monstey_AFAdd(record, value);
        auto found = offsets.find(record);
        if (found == offsets.end())
        {
            Monstey_ARequire(record.size() <= UINT16_MAX - output.size(),
                             "UAQT/UAVT payload exceeds native block limit");
            const auto offset = Monstey_A16(output.size());
            Monstey_AAppend(output, record);
            found = offsets.emplace(std::move(record), offset).first;
        }
        Monstey_PutU16(output, 2 * track, found->second);
    }
    return output;
}

} // namespace Monstey_AnixPrivate
using namespace Monstey_AnixPrivate;

B Monstey_AnixEncodeUaqt(const Monstey_AnixQuaternionKeys &values)
{
    return Monstey_AEncodeUncompressed<4>(values.ticks, values.values);
}

B Monstey_AnixEncodeUavt(std::span<const std::uint32_t> ticks, const std::vector<std::vector<V>> &values)
{
    return Monstey_AEncodeUncompressed<3>(ticks, values);
}

B Monstey_AnixEncodeCaf4(const Monstey_AnixScalarKeys &v)
{
    Monstey_ARequire(v.ticks.size() >= 2 && v.ticks.size() == v.values.size(), "CAF4 needs at least two matching keys");
    const auto tracks = v.values.front().size();
    Monstey_ARequire(tracks > 0 && tracks <= 65535 && v.ticks.size() <= Monstey_SampleLimit / tracks &&
                         v.ticks.size() <= Monstey_Limit / (4 * tracks),
                     "CAF4 track/key allocation limit");
    B out;
    Monstey_AppendU32(out, 1);
    Monstey_AppendU32(out, v.ticks.front());
    Monstey_AppendU32(out, v.ticks.back());
    Monstey_AppendU32(out, Monstey_A32(v.ticks.size()));
    Monstey_AppendU32(out, Monstey_A32(tracks));
    for (std::size_t key = 0; key < v.ticks.size(); ++key)
    {
        Monstey_ARequire(v.values[key].size() == tracks && (!key || v.ticks[key] > v.ticks[key - 1]),
                         "CAF4 key width/tick order mismatch");
        if (key)
            for (std::size_t track = 0; track < tracks; ++track)
            {
                const float lower = v.values[key - 1][track];
                const float difference = v.values[key][track] - lower;
                const float upper = lower + difference;
                Monstey_ARequire(std::isfinite(difference) && std::isfinite(upper),
                                 "CAF4 native interpolation would overflow float32");
            }
        Monstey_AppendU16(out, key ? Monstey_A16(v.ticks[key] - v.ticks[key - 1]) : 0);
    }
    Monstey_APad(out, 16);
    for (const auto &row : v.values)
        for (float value : row)
            Monstey_AFAdd(out, value);
    (void)Monstey_AReadCaf4(out, tracks);
    return out;
}

B Monstey_AnixEncodeUsft(std::span<const std::uint32_t> words)
{
    Monstey_ARequire(!words.empty() && words.size() <= 65535 / 2, "USFT raw-word offset/allocation limit");
    B out(words.size() * 2);
    std::map<std::uint32_t, std::uint16_t> offsets;
    for (std::size_t track = 0; track < words.size(); ++track)
    {
        const auto existing = offsets.find(words[track]);
        std::uint16_t offset;
        if (existing != offsets.end())
            offset = existing->second;
        else
        {
            Monstey_ARequire(out.size() <= 65535 - 4, "USFT payload exceeds native block limit");
            offset = Monstey_A16(out.size());
            offsets.emplace(words[track], offset);
            Monstey_AppendU32(out, words[track]);
        }
        Monstey_PutU16(out, track * 2, offset);
    }
    return out;
}

B Monstey_AnixEncodeUsbt(std::span<const std::uint8_t> values)
{
    Monstey_ARequire(!values.empty() && values.size() <= 65535 / 2, "USBT byte offset/allocation limit");
    B out(values.size() * 2);
    std::array<std::optional<std::uint16_t>, 256> offsets;
    for (std::size_t track = 0; track < values.size(); ++track)
    {
        auto &offset = offsets[values[track]];
        if (!offset)
        {
            Monstey_ARequire(out.size() < 65535, "USBT payload exceeds native block limit");
            offset = Monstey_A16(out.size());
            out.push_back(values[track]);
        }
        Monstey_PutU16(out, track * 2, *offset);
    }
    return out;
}

B Monstey_AnixPackRow(std::span<const Q> q)
{
    Monstey_ARequire(q.size() <= 65535, "Quaternion track limit");
    B out;
    const auto words = (q.size() + 2) / 3;
    out.resize(words * 16);
    constexpr std::uint64_t identity = 4095ull | (4095ull << 13) | (4095ull << 26);
    for (std::size_t g = 0; g < words; ++g)
        for (unsigned s = 0; s < 3; ++s)
        {
            const auto w = g * 3 + s < q.size() ? Monstey_APackQuaternion(q[g * 3 + s]) : identity;
            for (unsigned bit = 0; bit < 42; ++bit)
                if (w & (1ull << bit))
                {
                    const auto dest = 42 * s + bit;
                    out[g * 16 + dest / 8] |= static_cast<std::uint8_t>(1u << (dest % 8));
                }
        }
    return out;
}

B Monstey_AnixEncodeAq03(const Monstey_AnixQuaternionKeys &v)
{
    Monstey_ARequire(v.ticks.size() >= 2 && v.ticks.size() == v.values.size(), "AQ03 needs at least two matching keys");
    const auto tracks = v.values.front().size();
    Monstey_ARequire(tracks > 0 && tracks <= 65535, "Invalid AQ03 track count");
    B b;
    Monstey_AppendU32(b, v.ticks.front());
    Monstey_AppendU32(b, v.ticks.back());
    Monstey_AppendU32(b, Monstey_A32(v.ticks.size()));
    for (std::size_t i = 0; i < v.ticks.size(); ++i)
    {
        Monstey_ARequire(v.values[i].size() == tracks, "AQ03 key width mismatch");
        Monstey_ARequire(i == 0 || v.ticks[i] > v.ticks[i - 1], "AQ03 ticks must increase");
        Monstey_AppendU16(b, i ? Monstey_A16(v.ticks[i] - v.ticks[i - 1]) : 0);
    }
    Monstey_APad(b, 16);
    for (const auto &row : v.values)
        Monstey_AAppend(b, Monstey_AnixPackRow(row));
    return b;
}

B Monstey_AnixEncodeUatr(const std::vector<std::vector<V>> &v)
{
    Monstey_ARequire(v.size() >= 2 && v.size() <= 256 && !v.front().empty(), "UATR needs 2..256 samples and tracks");
    const auto tracks = v.front().size(), table = Monstey_AAlign(2 * tracks),
               per = Monstey_AAlign(2 * v.size()) + 12 * v.size();
    Monstey_ARequire(tracks <= 65535 && table + per * tracks <= 65535, "UATR exceeds 16-bit track offsets");
    B b;
    for (std::size_t t = 0; t < tracks; ++t)
        Monstey_AppendU16(b, Monstey_A16(table + t * per));
    b.resize(table, 0);
    for (std::size_t t = 0; t < tracks; ++t)
    {
        for (std::size_t i = 0; i < v.size(); ++i)
        {
            Monstey_ARequire(v[i].size() == tracks, "UATR sample width mismatch");
            b.push_back(static_cast<std::uint8_t>(i + 1 == v.size() ? i - 1 : i));
            b.push_back(i + 1 == v.size() ? 255 : 0);
        }
        Monstey_APad(b);
        for (const auto &row : v)
            for (float f : row[t])
                Monstey_AFAdd(b, f);
    }
    return b;
}

} // namespace Monstey
