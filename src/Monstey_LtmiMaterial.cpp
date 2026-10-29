#include "Monstey_LtmiMaterial.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
namespace Monstey
{
namespace
{
using B = Monstey_Bytes;
using J = Monstey_Json;
void Monstey_LtmiRequire(bool value, const char *message)
{
    if (!value)
        throw Monstey_Error("ltmi_invalid", message);
}
std::string Monstey_LtmiCString(std::span<const std::uint8_t> bytes)
{
    if (bytes.empty())
        return {};
    auto end = std::find(bytes.begin(), bytes.end(), 0);
    return std::string(reinterpret_cast<const char *>(bytes.data()), static_cast<std::size_t>(end - bytes.begin()));
}
B Monstey_LtmiSlice(std::span<const std::uint8_t> bytes, std::size_t start, std::size_t count)
{
    Monstey_CheckRange(bytes, start, count);
    return B(bytes.begin() + static_cast<std::ptrdiff_t>(start),
             bytes.begin() + static_cast<std::ptrdiff_t>(start + count));
}
std::size_t Monstey_LtmiCount(const Monstey_LtmiMaterial &m)
{
    return m.tables[0].size() + m.tables[1].size();
}
void Monstey_LtmiName(std::span<const std::uint8_t> bytes)
{
    Monstey_LtmiRequire(bytes.size() < 256, "Native LTMI property name length must be below256");
}
Monstey_LtmiProperty &Monstey_LtmiAt(Monstey_LtmiMaterial &m, std::size_t table, std::size_t index)
{
    Monstey_LtmiRequire(table < 2 && index < m.tables[table].size(), "LTMI property selection is outside its table");
    return m.tables[table][index];
}
bool Monstey_LtmiEditable(std::uint16_t type)
{
    switch (static_cast<std::uint8_t>(type))
    {
    case 0:
    case 7:
    case 12:
    case 16:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 8:
    case 9:
    case 10:
    case 11:
    case 13:
    case 14:
    case 15:
        return true;
    default:
        return false;
    }
}
void Monstey_LtmiCheckPayload(std::uint16_t type, std::span<const std::uint8_t> value)
{
    Monstey_LtmiRequire(Monstey_LtmiEditable(type), "LTMI authored value storage is not qualified");
    const auto low = static_cast<std::uint8_t>(type);
    const auto native_size = Monstey_LtmiNativeSize(type);
    if (low != 8 && native_size && *native_size == 0)
    {
        Monstey_LtmiRequire(value.empty(), "Native zero-copy parameter has no value payload");
        return;
    }
    if (low == 8 && value.empty())
        return;
    if (low == 1 || low == 8)
    {
        Monstey_LtmiRequire(!value.empty() && value.back() == 0, "LTMI string value needs a terminal NUL");
        Monstey_LtmiRequire(std::find(value.begin(), value.end() - 1, 0) == value.end() - 1,
                            "LTMI authored string contains an embedded NUL");
    }
    else
    {
        const auto size = Monstey_LtmiNativeSize(type);
        Monstey_LtmiRequire(size && value.size() == *size, "LTMI value payload differs from native storage width");
    }
}
std::uint32_t Monstey_LtmiAppendValue(B &blob, std::span<const std::uint8_t> value)
{
    const auto padding = (16 - (blob.size() % 16)) % 16;
    Monstey_LtmiRequire(blob.size() <= 65535 && padding <= 65535 - blob.size(),
                        "LTMI appended value offset exceeds native U16 storage");
    const auto offset = blob.size() + padding;
    Monstey_LtmiRequire(value.size() <= 65536 - offset, "LTMI appended value exceeds the qualified U16-addressed blob");
    B next = blob;
    next.insert(next.end(), padding, 0);
    next.insert(next.end(), value.begin(), value.end());
    blob.swap(next);
    return static_cast<std::uint32_t>(offset);
}
// This is storage ownership, not a new runtime type/consumer claim.
// Optional type8 strings protect an observed bounded span; an unterminated
// view protects its existing raw tail only. Appending may change its optional
// diagnostic, but never repairs or overwrites those original bytes.
bool Monstey_LtmiCanOverwrite(const Monstey_LtmiMaterial &m, std::size_t table, std::size_t index,
                              std::span<const std::uint8_t> current, std::size_t replacement_size)
{
    if (current.empty() || replacement_size == 0 || replacement_size > current.size())
        return false;
    const auto begin = std::size_t(static_cast<std::uint16_t>(m.tables[table][index].offset));
    const auto end = begin + current.size();
    for (std::size_t other_table = 0; other_table < 2; ++other_table)
        for (std::size_t other_index = 0; other_index < m.tables[other_table].size(); ++other_index)
        {
            if (other_table == table && other_index == index)
                continue;
            const auto &other = m.tables[other_table][other_index];
            const auto low = static_cast<std::uint8_t>(other.type);
            const auto native_size = Monstey_LtmiNativeSize(other.type);
            const auto other_begin = std::size_t(static_cast<std::uint16_t>(other.offset));
            std::size_t protected_bytes = 0;
            if (low == 8)
            {
                if (other_begin >= m.values.size())
                    continue;
                const auto observed = Monstey_LtmiValue(m, other);
                protected_bytes = observed.empty() ? m.values.size() - other_begin : observed.size();
            }
            else if (native_size && *native_size == 0)
                continue;
            else if (low > 15 || !native_size)
                return false; // Unknown serialized extent: retain append/copy-on-write.
            else
                protected_bytes = Monstey_LtmiValue(m, other).size();
            const auto other_end = other_begin + protected_bytes;
            if (begin < other_end && other_begin < end)
                return false;
        }
    return true;
}
J Monstey_LtmiWords(std::span<const std::uint8_t> value, bool floats)
{
    auto rows = J::array();
    for (std::size_t at = 0; at + 4 <= value.size(); at += 4)
    {
        auto bits = Monstey_U32(value, at);
        if (floats)
        {
            const auto f = std::bit_cast<float>(bits);
            rows.as_array().push_back(std::isfinite(f) ? J(double(f))
                                                       : J::object({{"float32_bits", bits}, {"nonfinite", true}}));
        }
        else
            rows.as_array().push_back(bits);
    }
    return rows;
}
} // namespace
std::optional<std::uint8_t> Monstey_LtmiNativeSize(std::uint16_t type)
{
    // QWORD stores at178087,17809D,1780B3 also initialize slots7,12,16 to zero.
    constexpr std::array<std::uint8_t, 24> sizes{0, 16, 12, 16, 4, 4, 4,  0,  0, 64, 48, 8,
                                                 0, 8,  8,  8,  0, 8, 12, 16, 4, 8,  12, 16};
    const auto index = static_cast<std::uint8_t>(type);
    return index < sizes.size() ? std::optional<std::uint8_t>(sizes[index]) : std::nullopt;
}
std::uint32_t Monstey_LtmiPropertyHash(const Monstey_LtmiProperty &p)
{
    return (Monstey_NativeHash(Monstey_LtmiCString(p.name)) & 0x0fffffffU) |
           (std::uint32_t(static_cast<std::uint8_t>(p.type)) << 28);
}
Monstey_Bytes Monstey_LtmiValue(const Monstey_LtmiMaterial &m, const Monstey_LtmiProperty &p)
{
    const auto at = std::size_t(static_cast<std::uint16_t>(p.offset));
    const auto low = static_cast<std::uint8_t>(p.type);
    if (low == 8)
    {
        // Native storage width is zero. A bounded string is an observed file convention,
        // not a prerequisite of the selected native material loader.
        if (at >= m.values.size())
            return {};
        auto start = m.values.begin() + static_cast<std::ptrdiff_t>(at);
        auto end = std::find(start, m.values.end(), 0);
        return end == m.values.end() ? B{} : B(start, end + 1);
    }
    if (low == 1)
    {
        Monstey_CheckRange(m.values, at, 1);
        auto start = m.values.begin() + static_cast<std::ptrdiff_t>(at);
        auto end = std::find(start, m.values.end(), 0);
        Monstey_LtmiRequire(end != m.values.end(),
                            "LTMI texture string is not terminated inside its serialized value blob");
        return B(start, end + 1);
    }
    const auto size = Monstey_LtmiNativeSize(p.type);
    if (low > 15 || !size || *size == 0)
        return {};
    return Monstey_LtmiSlice(m.values, at, *size);
}
Monstey_LtmiMaterial Monstey_LtmiRead(std::span<const std::uint8_t> bytes)
{
    Monstey_LtmiRequire(bytes.size() >= 30 && Monstey_U32(bytes, 0) == 0x494d544c, "Expected LTMI header");
    Monstey_LtmiMaterial m;
    m.version = Monstey_U32(bytes, 4);
    Monstey_LtmiRequire(m.version == 7 || m.version == 8, "Native LTMI profile is version7 or8");
    m.shader_id = Monstey_U32(bytes, 8);
    const auto value_bytes = Monstey_U32(bytes, 12);
    const auto a = Monstey_U16(bytes, 16), b = Monstey_U16(bytes, 18);
    Monstey_LtmiRequire(std::uint32_t(a) + b <= 128, "Native LTMI loader rejects more than128 parameters");
    m.unknown_20 = Monstey_U32(bytes, 20);
    m.unknown_24 = Monstey_U32(bytes, 24);
    const auto name_bytes = Monstey_U16(bytes, 28);
    m.shader_name = Monstey_LtmiSlice(bytes, 30, name_bytes);
    std::size_t at = 30 + std::size_t(name_bytes);
    m.values = Monstey_LtmiSlice(bytes, at, value_bytes);
    at += value_bytes;
    if (m.version == 7)
    {
        const auto count = Monstey_U32(bytes, at);
        at += 4;
        m.legacy_blob = Monstey_LtmiSlice(bytes, at, count);
        at += count;
    }
    for (std::size_t table = 0; table < 2; ++table)
    {
        const auto count = table == 0 ? a : b;
        auto &rows = m.tables[table];
        rows.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            Monstey_CheckRange(bytes, at, 6);
            Monstey_LtmiProperty p;
            p.type = Monstey_U16(bytes, at);
            p.flag_a = bytes[at + 2];
            p.flag_b = bytes[at + 3];
            const auto length = Monstey_U16(bytes, at + 4);
            at += 6;
            Monstey_LtmiRequire(length < 256, "Native LTMI property-name terminator would exceed256-byte buffer");
            p.name = Monstey_LtmiSlice(bytes, at, length);
            at += length;
            p.offset = Monstey_U32(bytes, at);
            at += 4;
            if (m.version == 7)
            {
                p.legacy_word = Monstey_U16(bytes, at);
                at += 2;
            }
            (void)Monstey_LtmiValue(m, p);
            rows.push_back(std::move(p));
        }
    }
    m.trailer = Monstey_LtmiSlice(bytes, at, bytes.size() - at);
    m.source.assign(bytes.begin(), bytes.end());
    return m;
}
Monstey_Bytes Monstey_LtmiBuild(const Monstey_LtmiMaterial &m)
{
    Monstey_LtmiRequire(m.version == 7 || m.version == 8, "Native LTMI profile is version7 or8");
    Monstey_LtmiRequire(Monstey_LtmiCount(m) <= 128, "Native LTMI loader rejects more than128 parameters");
    Monstey_LtmiRequire(m.shader_name.size() <= 65535 && m.values.size() <= std::numeric_limits<std::uint32_t>::max() &&
                            m.legacy_blob.size() <= std::numeric_limits<std::uint32_t>::max(),
                        "LTMI header length exceeds stored field");
    B out(30);
    Monstey_PutU32(out, 0, 0x494d544c);
    Monstey_PutU32(out, 4, m.version);
    Monstey_PutU32(out, 8, m.shader_id);
    Monstey_PutU32(out, 12, static_cast<std::uint32_t>(m.values.size()));
    Monstey_PutU16(out, 16, static_cast<std::uint16_t>(m.tables[0].size()));
    Monstey_PutU16(out, 18, static_cast<std::uint16_t>(m.tables[1].size()));
    Monstey_PutU32(out, 20, m.unknown_20);
    Monstey_PutU32(out, 24, m.unknown_24);
    Monstey_PutU16(out, 28, static_cast<std::uint16_t>(m.shader_name.size()));
    out.insert(out.end(), m.shader_name.begin(), m.shader_name.end());
    out.insert(out.end(), m.values.begin(), m.values.end());
    if (m.version == 7)
    {
        Monstey_AppendU32(out, static_cast<std::uint32_t>(m.legacy_blob.size()));
        out.insert(out.end(), m.legacy_blob.begin(), m.legacy_blob.end());
    }
    for (const auto &table : m.tables)
        for (const auto &p : table)
        {
            Monstey_LtmiName(p.name);
            Monstey_AppendU16(out, p.type);
            out.push_back(p.flag_a);
            out.push_back(p.flag_b);
            Monstey_AppendU16(out, static_cast<std::uint16_t>(p.name.size()));
            out.insert(out.end(), p.name.begin(), p.name.end());
            Monstey_AppendU32(out, p.offset);
            if (m.version == 7)
                Monstey_AppendU16(out, p.legacy_word);
        }
    out.insert(out.end(), m.trailer.begin(), m.trailer.end());
    (void)Monstey_LtmiRead(out);
    return out;
}
void Monstey_LtmiSetValue(Monstey_LtmiMaterial &m, std::size_t table, std::size_t index,
                          std::span<const std::uint8_t> value)
{
    const auto &current = Monstey_LtmiAt(m, table, index);
    Monstey_LtmiCheckPayload(current.type, value);
    const B replacement(value.begin(), value.end()); // May alias any owning input field.
    const auto previous = Monstey_LtmiValue(m, current);
    if (previous == replacement)
        return;
    const bool overwrite = Monstey_LtmiCanOverwrite(m, table, index, previous, replacement.size());
    auto next = m;
    if (overwrite)
    {
        const auto at = std::size_t(static_cast<std::uint16_t>(current.offset));
        std::copy(replacement.begin(), replacement.end(),
                  next.values.begin() + static_cast<std::ptrdiff_t>(at));
    }
    else
    {
        const auto offset = Monstey_LtmiAppendValue(next.values, replacement);
        // Native addressing uses the lowU16; preserve unrelated high stored bits.
        next.tables[table][index].offset = (current.offset & 0xffff0000U) | offset;
    }
    (void)Monstey_LtmiBuild(next);
    m = std::move(next);
}
void Monstey_LtmiAdd(Monstey_LtmiMaterial &m, std::size_t table, Monstey_LtmiProperty p,
                     std::span<const std::uint8_t> value)
{
    Monstey_LtmiRequire(table < 2 && Monstey_LtmiCount(m) < 128, "LTMI table/count cannot accept another parameter");
    Monstey_LtmiName(p.name);
    Monstey_LtmiCheckPayload(p.type, value);
    auto next = m;
    const auto native_size = Monstey_LtmiNativeSize(p.type);
    p.offset = (static_cast<std::uint8_t>(p.type) != 8 && native_size && *native_size == 0)
                   ? 0
                   : Monstey_LtmiAppendValue(next.values, value);
    next.tables[table].push_back(std::move(p));
    (void)Monstey_LtmiBuild(next);
    m = std::move(next);
}
void Monstey_LtmiRemove(Monstey_LtmiMaterial &m, std::size_t table, std::size_t index)
{
    (void)Monstey_LtmiAt(m, table, index);
    auto next = m;
    next.tables[table].erase(next.tables[table].begin() + static_cast<std::ptrdiff_t>(index));
    (void)Monstey_LtmiBuild(next);
    m = std::move(next);
}
void Monstey_LtmiRename(Monstey_LtmiMaterial &m, std::size_t table, std::size_t index,
                        std::span<const std::uint8_t> name)
{
    (void)Monstey_LtmiAt(m, table, index);
    Monstey_LtmiName(name);
    auto next = m;
    next.tables[table][index].name.assign(name.begin(), name.end());
    (void)Monstey_LtmiBuild(next);
    m = std::move(next);
}
Monstey_Json Monstey_LtmiDescribe(const Monstey_LtmiMaterial &m)
{
    auto entries = J::array();
    std::size_t count = 0;
    for (std::size_t table = 0; table < 2; ++table)
        for (std::size_t index = 0; index < m.tables[table].size(); ++index)
        {
            const auto &p = m.tables[table][index];
            const auto value = Monstey_LtmiValue(m, p);
            const auto size = Monstey_LtmiNativeSize(p.type);
            const auto low = static_cast<std::uint8_t>(p.type);
            const bool floats = low == 2 || low == 3 || low == 5 || low == 9 || low == 10 || low == 11;
            auto row = J::object({{"table", table == 0 ? "a" : "b"},
                                  {"ordinal", static_cast<std::uint64_t>(index)},
                                  {"type_u16", p.type},
                                  {"native_type_u8", low},
                                  {"flag_a", p.flag_a},
                                  {"flag_b", p.flag_b},
                                  {"name", Monstey_DisplayUtf8(Monstey_LtmiCString(p.name))},
                                  {"name_bytes", static_cast<std::uint64_t>(p.name.size())},
                                  {"native_name_type_hash", Monstey_LtmiPropertyHash(p)},
                                  {"stored_offset_u32", p.offset},
                                  {"runtime_offset_u16", static_cast<std::uint16_t>(p.offset)},
                                  {"offset_truncated_by_native", p.offset > 65535},
                                  {"native_copy_bytes", size ? J(*size) : J()},
                                  {"serialized_value_bytes", static_cast<std::uint64_t>(value.size())},
                                  {"authored_value_qualified", Monstey_LtmiEditable(p.type) && low > 0 && low < 13 &&
                                                                   low != 7 && low != 8 && low != 12},
                                  {"raw_storage_editable", Monstey_LtmiEditable(p.type)},
                                  {"native_value_stride_bytes", size ? J(*size) : J()}});
            if (low == 1 || (low == 8 && !value.empty()))
                row["serialized_string"] = Monstey_DisplayUtf8(Monstey_LtmiCString(value));
            else if (low != 8)
                row["value_words_u32"] = Monstey_LtmiWords(value, false);
            if (low == 8)
            {
                row["native_zero_copy"] = true;
                row["serialized_string_observed"] = !value.empty();
                row["file_profile_string_edit_supported"] = true;
                row["string_consumer_verified"] = false;
            }
            row["native_zero_copy"] = size && *size == 0;
            row["native_hash_type_tag"] = std::uint32_t(low) & 15U;
            if (floats)
                row["value_f32"] = Monstey_LtmiWords(value, true);
            if (m.version == 7)
                row["legacy_word"] = p.legacy_word;
            entries.as_array().push_back(std::move(row));
            ++count;
        }
    const auto built = Monstey_LtmiBuild(m);
    const auto shader = Monstey_LtmiCString(m.shader_name);
    const auto native_shader = shader.substr(0, 259);
    return J::object({{"schema", "Monstey.LtmiMaterial/1"},
                      {"version", m.version},
                      {"shader_id", m.shader_id},
                      {"shader_name", Monstey_DisplayUtf8(shader)},
                      {"native_shader_name", Monstey_DisplayUtf8(native_shader)},
                      {"native_shader_name_truncated", shader.size() > 259},
                      {"shader_name_bytes", static_cast<std::uint64_t>(m.shader_name.size())},
                      {"unknown_20", m.unknown_20},
                      {"unknown_24", m.unknown_24},
                      {"value_blob_bytes", static_cast<std::uint64_t>(m.values.size())},
                      {"table_a_count", static_cast<std::uint64_t>(m.tables[0].size())},
                      {"table_b_count", static_cast<std::uint64_t>(m.tables[1].size())},
                      {"parameter_count", static_cast<std::uint64_t>(count)},
                      {"legacy_blob_bytes", static_cast<std::uint64_t>(m.legacy_blob.size())},
                      {"trailing_bytes", static_cast<std::uint64_t>(m.trailer.size())},
                      {"entries", entries},
                      {"rebuilt_sha256", Monstey_HashSha256(built)},
                      {"source_sha256", Monstey_HashSha256(m.source)},
                      {"no_op_byte_exact", built == m.source},
                      {"runtime_material_rendering_verified", false},
                      {"product_runtime", "C++20"}});
}
} // namespace Monstey