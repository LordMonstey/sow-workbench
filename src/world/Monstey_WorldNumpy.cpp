#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

Monstey_WNpy Monstey_WNpy::Parse(const B &b)
{
    Monstey_Require(b.size() >= 10 && std::memcmp(b.data(), "\x93NUMPY", 6) == 0, "NPZ entry is not NPY");
    auto version = b[6];
    Monstey_Require((version == 1 || version == 2 || version == 3) && b[7] == 0, "Unsupported NPY version");
    auto length = version == 1 ? Monstey_U16(b, 8) : Monstey_U32(b, 8);
    auto start = version == 1 ? 10ull : 12ull;
    Monstey_CheckRange(b, static_cast<std::size_t>(start), length);
    std::string header(b.begin() + static_cast<std::ptrdiff_t>(start),
                       b.begin() + static_cast<std::ptrdiff_t>(start + length));
    auto value = [&](const std::string &key)
    {
        auto p = header.find("'" + key + "'");
        if (p == std::string::npos)
            p = header.find("\"" + key + "\"");
        Monstey_Require(p != std::string::npos, "NPY header key missing");
        p = header.find(':', p);
        Monstey_Require(p != std::string::npos, "NPY header colon missing");
        ++p;
        while (p < header.size() && (header[p] == ' ' || header[p] == '\t'))
            ++p;
        return p;
    };
    auto fp = value("fortran_order");
    Monstey_Require(header.compare(fp, 5, "False") == 0, "Fortran-order NPY is unsupported");
    auto dp = value("descr");
    Monstey_Require(dp < header.size() && (header[dp] == '\'' || header[dp] == '\"'),
                    "NPY dtype must be a scalar string");
    auto stop = header.find(header[dp], dp + 1);
    Monstey_Require(stop != std::string::npos, "NPY dtype unterminated");
    auto dtype = header.substr(dp + 1, stop - dp - 1);
    Monstey_Require(dtype.size() >= 3, "NPY dtype truncated");
    Monstey_WNpy result;
    result.endian = dtype[0];
    result.kind = dtype[1];
    auto [end, error] = std::from_chars(dtype.data() + 2, dtype.data() + dtype.size(), result.unit);
    Monstey_Require(
        end == dtype.data() + dtype.size() && error == std::errc{} &&
            (result.endian == '<' || result.endian == '>' || result.endian == '|' || result.endian == '=') &&
            ((result.kind == 'f' && (result.unit == 2 || result.unit == 4 || result.unit == 8)) ||
             ((result.kind == 'i' || result.kind == 'u') &&
              (result.unit == 1 || result.unit == 2 || result.unit == 4 || result.unit == 8))),
        "NPY requires a supported numeric scalar dtype");
    auto shape = value("shape");
    Monstey_Require(shape < header.size() && header[shape] == '(', "NPY shape tuple missing");
    ++shape;
    std::vector<std::size_t> dimensions;
    while (shape < header.size())
    {
        while (shape < header.size() && (header[shape] == ' ' || header[shape] == '\t' || header[shape] == ','))
            ++shape;
        if (shape < header.size() && header[shape] == ')')
            break;
        std::size_t number{};
        auto [next, ec] = std::from_chars(header.data() + shape, header.data() + header.size(), number);
        Monstey_Require(ec == std::errc{} && next != header.data() + shape, "Invalid NPY shape dimension");
        dimensions.push_back(number);
        shape = static_cast<std::size_t>(next - header.data());
        Monstey_Require(dimensions.size() <= 2, "NPY must be a numeric matrix");
    }
    Monstey_Require(dimensions.size() == 2 && shape < header.size() && header[shape] == ')',
                    "NPY must have exactly two dimensions");
    result.count = dimensions[0];
    result.width = dimensions[1];
    Monstey_Require(result.width > 0 && result.width <= 256 && result.count <= 100000000 &&
                        result.count <= std::numeric_limits<std::size_t>::max() / result.width / result.unit,
                    "NPY shape exceeds limits");
    auto bytes = result.count * result.width * result.unit;
    Monstey_Require(bytes <= 256ull * 1024 * 1024 && start + length + bytes == b.size(),
                    "NPY shape/data extent differs");
    result.data.assign(b.begin() + static_cast<std::ptrdiff_t>(start + length), b.end());
    return result;
}

double Monstey_WNpy::Number(std::size_t row, std::size_t column) const
{
    Monstey_Require(row < count && column < width, "NPY component outside matrix");
    auto at = (row * width + column) * unit;
    std::uint64_t bits{};
    if (endian == '>')
        for (std::size_t j = 0; j < unit; ++j)
            bits = (bits << 8) | data[at + j];
    else
        for (std::size_t j = 0; j < unit; ++j)
            bits |= std::uint64_t(data[at + j]) << (8 * j);
    if (kind == 'f')
    {
        if (unit == 2)
            return Monstey_WHalf(static_cast<std::uint16_t>(bits));
        if (unit == 4)
            return std::bit_cast<float>(static_cast<std::uint32_t>(bits));
        return std::bit_cast<double>(bits);
    }
    if (kind == 'u')
        return static_cast<double>(bits);
    if (unit == 8)
        return static_cast<double>(std::bit_cast<std::int64_t>(bits));
    if (unit == 4)
        return static_cast<std::int32_t>(bits);
    if (unit == 2)
        return static_cast<std::int16_t>(bits);
    return static_cast<std::int8_t>(bits);
}

B Monstey_WNpyEncode(const std::string &dtype, std::size_t count, std::size_t width, const B &data)
{
    std::string header = "{'descr': '" + dtype + "', 'fortran_order': False, 'shape': (" + std::to_string(count) +
                         ", " + std::to_string(width) + "), }";
    while ((10 + header.size() + 1) % 64)
        header.push_back(' ');
    header.push_back('\n');
    Monstey_Require(header.size() <= 65535, "NPY header exceeds v1 limit");
    B out{0x93, 'N', 'U', 'M', 'P', 'Y', 1, 0};
    Monstey_AppendU16(out, static_cast<std::uint16_t>(header.size()));
    out.insert(out.end(), header.begin(), header.end());
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

B Monstey_WNpzEncode(const std::map<std::string, B> &entries)
{
    B out, central;
    Monstey_Require(entries.size() <= 1024, "NPZ entry count exceeds limit");
    for (auto &[key, data] : entries)
    {
        auto name = key + ".npy";
        Monstey_Require(name.size() <= 65535 && out.size() <= 0xffffffffull && data.size() <= 0xffffffffull,
                        "NPZ exceeds classic ZIP capacity");
        auto offset = static_cast<std::uint32_t>(out.size()), size = static_cast<std::uint32_t>(data.size()),
             crc = Monstey_WCrc(data);
        Monstey_AppendU32(out, 0x04034b50);
        for (auto n : {20u, 0u, 0u, 0u, 0u})
            Monstey_AppendU16(out, static_cast<std::uint16_t>(n));
        Monstey_AppendU32(out, crc);
        Monstey_AppendU32(out, size);
        Monstey_AppendU32(out, size);
        Monstey_AppendU16(out, static_cast<std::uint16_t>(name.size()));
        Monstey_AppendU16(out, 0);
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), data.begin(), data.end());
        Monstey_AppendU32(central, 0x02014b50);
        for (auto n : {20u, 20u, 0u, 0u, 0u, 0u})
            Monstey_AppendU16(central, static_cast<std::uint16_t>(n));
        Monstey_AppendU32(central, crc);
        Monstey_AppendU32(central, size);
        Monstey_AppendU32(central, size);
        Monstey_AppendU16(central, static_cast<std::uint16_t>(name.size()));
        for (unsigned j = 0; j < 4; ++j)
            Monstey_AppendU16(central, 0);
        Monstey_AppendU32(central, 0);
        Monstey_AppendU32(central, offset);
        central.insert(central.end(), name.begin(), name.end());
    }
    Monstey_Require(out.size() + central.size() + 22 <= 512ull * 1024 * 1024, "NPZ output exceeds limit");
    auto offset = static_cast<std::uint32_t>(out.size());
    out.insert(out.end(), central.begin(), central.end());
    Monstey_AppendU32(out, 0x06054b50);
    Monstey_AppendU16(out, 0);
    Monstey_AppendU16(out, 0);
    Monstey_AppendU16(out, static_cast<std::uint16_t>(entries.size()));
    Monstey_AppendU16(out, static_cast<std::uint16_t>(entries.size()));
    Monstey_AppendU32(out, static_cast<std::uint32_t>(central.size()));
    Monstey_AppendU32(out, offset);
    Monstey_AppendU16(out, 0);
    return out;
}

std::map<std::string, Monstey_WNpy> Monstey_WNpzRead(const B &raw)
{
    Monstey_Require(raw.size() >= 22, "Truncated NPZ ZIP");
    std::optional<std::size_t> eocd;
    auto first = raw.size() > 65557 ? raw.size() - 65557 : 0;
    for (std::size_t p = raw.size() - 22;; --p)
    {
        if (Monstey_U32(raw, p) == 0x06054b50 && p + 22ull + Monstey_U16(raw, p + 20) == raw.size())
        {
            eocd = p;
            break;
        }
        if (p == first)
            break;
    }
    Monstey_Require(eocd.has_value(), "NPZ ZIP end directory missing");
    auto e = *eocd;
    Monstey_Require(Monstey_U16(raw, e + 4) == 0 && Monstey_U16(raw, e + 6) == 0 &&
                        Monstey_U16(raw, e + 8) == Monstey_U16(raw, e + 10),
                    "Multi-disk ZIP unsupported");
    auto count = Monstey_U16(raw, e + 10);
    auto central_size = Monstey_U32(raw, e + 12), central_offset = Monstey_U32(raw, e + 16);
    Monstey_Require(count > 0 && count <= 1024 && std::uint64_t(central_offset) + central_size == e,
                    "NPZ central directory extent invalid");
    std::size_t at = central_offset;
    std::map<std::string, Monstey_WNpy> out;
    std::uint64_t total{};
    for (unsigned j = 0; j < count; ++j)
    {
        Monstey_CheckRange(raw, at, 46);
        Monstey_Require(Monstey_U32(raw, at) == 0x02014b50, "NPZ central record missing");
        auto flags = Monstey_U16(raw, at + 8), method = Monstey_U16(raw, at + 10),
             name_size = Monstey_U16(raw, at + 28), extra_size = Monstey_U16(raw, at + 30),
             comment = Monstey_U16(raw, at + 32);
        auto crc = Monstey_U32(raw, at + 16);
        std::uint64_t compressed = Monstey_U32(raw, at + 20), decoded = Monstey_U32(raw, at + 24),
                      local = Monstey_U32(raw, at + 42);
        Monstey_Require(!(flags & 1u) && (method == 0 || method == 8) && Monstey_U16(raw, at + 34) == 0,
                        "Unsupported encrypted or non-DEFLATE NPZ entry");
        Monstey_CheckRange(raw, at + 46, std::size_t(name_size) + extra_size + comment);
        std::string name(raw.begin() + static_cast<std::ptrdiff_t>(at + 46),
                         raw.begin() + static_cast<std::ptrdiff_t>(at + 46 + name_size));
        Monstey_Require(name.ends_with(".npy") && name.find('/') == std::string::npos &&
                            name.find('\\') == std::string::npos && name.find('\0') == std::string::npos,
                        "NPZ requires flat NPY entries");
        if (compressed == 0xffffffffull || decoded == 0xffffffffull || local == 0xffffffffull)
        {
            auto p = at + 46 + name_size, end = p + extra_size;
            bool found = false;
            while (p + 4 <= end)
            {
                auto tag = Monstey_U16(raw, p), n = Monstey_U16(raw, p + 2);
                p += 4;
                Monstey_Require(n <= end - p, "Invalid ZIP extra extent");
                if (tag == 1)
                {
                    auto cursor = p;
                    auto next = [&]()
                    {
                        Monstey_Require(cursor + 8 <= p + n, "Truncated ZIP64 sizes");
                        auto v = Monstey_U64(raw, cursor);
                        cursor += 8;
                        return v;
                    };
                    if (decoded == 0xffffffffull)
                        decoded = next();
                    if (compressed == 0xffffffffull)
                        compressed = next();
                    if (local == 0xffffffffull)
                        local = next();
                    found = true;
                    break;
                }
                p += n;
            }
            Monstey_Require(found, "ZIP64 values absent");
        }
        Monstey_Require(decoded <= 256ull * 1024 * 1024 && total + decoded <= 512ull * 1024 * 1024 &&
                            compressed <= raw.size() && local <= raw.size(),
                        "NPZ entry exceeds extent limit");
        Monstey_CheckRange(raw, static_cast<std::size_t>(local), 30);
        Monstey_Require(Monstey_U32(raw, local) == 0x04034b50 && Monstey_U16(raw, local + 8) == method &&
                            !(Monstey_U16(raw, local + 6) & 1u),
                        "NPZ local header differs");
        auto ln = Monstey_U16(raw, local + 26), le = Monstey_U16(raw, local + 28);
        auto payload = local + 30ull + ln + le;
        Monstey_CheckRange(raw, static_cast<std::size_t>(local + 30), ln);
        Monstey_Require(std::string(raw.begin() + static_cast<std::ptrdiff_t>(local + 30),
                                    raw.begin() + static_cast<std::ptrdiff_t>(local + 30 + ln)) == name &&
                            payload <= central_offset && compressed <= central_offset - payload,
                        "NPZ local name or payload invalid");
        auto bytes = std::span(raw).subspan(static_cast<std::size_t>(payload), static_cast<std::size_t>(compressed));
        B data;
        if (method == 0)
        {
            Monstey_Require(compressed == decoded, "Stored NPZ entry lengths differ");
            data.assign(bytes.begin(), bytes.end());
        }
        else
            data = Monstey_InflateRaw(bytes, static_cast<std::size_t>(decoded));
        Monstey_Require(Monstey_WCrc(data) == crc, "NPZ CRC32 differs");
        auto key = name.substr(0, name.size() - 4);
        Monstey_Require(out.emplace(key, Monstey_WNpy::Parse(data)).second, "Duplicate NPZ geometry attribute");
        total += decoded;
        at += 46 + name_size + extra_size + comment;
    }
    Monstey_Require(at == e, "NPZ directory count differs");
    return out;
}

} // namespace Monstey_WorldPrivate

} // namespace Monstey
