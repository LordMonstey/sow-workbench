#include "Monstey_Blend.hpp"
#include "Monstey_Archive.hpp"
#include "zstd.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <set>
namespace Monstey {
namespace {
using Monstey_J = Monstey_Json;
using Monstey_Span = std::span<const std::uint8_t>;
constexpr std::size_t Monstey_MaxBlend = 512 * 1024 * 1024;
void Monstey_BRequire(bool v, std::string_view message) {
    if (!v)
        throw Monstey_Error("blend_invalid", std::string(message));
}
std::uint64_t Monstey_BInt(Monstey_Span b, std::size_t at, unsigned size, bool little) {
    Monstey_CheckRange(b, at, size);
    std::uint64_t out = 0;
    for (unsigned i = 0; i < size; ++i)
        out |= static_cast<std::uint64_t>(b[at + i]) << ((little ? i : size - i - 1) * 8);
    return out;
}
void Monstey_BPut(Monstey_Bytes &b, std::size_t at, unsigned size, std::uint64_t value, bool little) {
    Monstey_CheckRange(b, at, size);
    for (unsigned i = 0; i < size; ++i)
        b[at + i] = static_cast<std::uint8_t>(value >> ((little ? i : size - i - 1) * 8));
}
void Monstey_BAppend(Monstey_Bytes &b, unsigned size, std::uint64_t value, bool little) {
    const auto at = b.size();
    b.resize(at + size);
    Monstey_BPut(b, at, size, value, little);
}
std::string Monstey_BText(Monstey_Span b, std::size_t at, std::size_t count) {
    Monstey_CheckRange(b, at, count);
    return {reinterpret_cast<const char *>(b.data() + at), count};
}
std::string Monstey_BCode(Monstey_Span b, std::size_t at) {
    auto out = Monstey_BText(b, at, 4);
    while (!out.empty() && out.back() == 0)
        out.pop_back();
    return out;
}
Monstey_Bytes Monstey_UnwrapBlend(const Monstey_Bytes &source, bool &compressed) {
    compressed = false;
    if (source.size() >= 4 && Monstey_U32(source, 0) == 0xfd2fb528) {
        compressed = true;
        std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> context(ZSTD_createDCtx(), &ZSTD_freeDCtx);
        Monstey_BRequire(context != nullptr, "Cannot allocate Zstandard decoder");
        ZSTD_inBuffer in{source.data(), source.size(), 0};
        Monstey_Bytes out;
        std::array<std::uint8_t, 64 * 1024> block{};
        std::size_t remaining = 1;
        while (in.pos < in.size) {
            ZSTD_outBuffer decoded{block.data(), block.size(), 0};
            const auto before = in.pos;
            remaining = ZSTD_decompressStream(context.get(), &decoded, &in);
            Monstey_BRequire(!ZSTD_isError(remaining), "Invalid Zstandard blend stream");
            Monstey_BRequire(decoded.pos > 0 || in.pos > before, "Zstandard decoder made no progress");
            Monstey_BRequire(decoded.pos <= Monstey_MaxBlend - out.size(),
                             "Decompressed blend exceeds 512 MiB");
            out.insert(out.end(), block.begin(), block.begin() + static_cast<std::ptrdiff_t>(decoded.pos));
        }
        Monstey_BRequire(remaining == 0, "Truncated Zstandard blend frame");
        return out;
    }
    if (source.size() >= 2 && source[0] == 0x1f && source[1] == 0x8b) {
        compressed = true;
        return Monstey_InflateGzip(source, Monstey_MaxBlend);
    }
    return source;
}
struct Monstey_DnaField {
    std::uint16_t type = 0, name = 0;
    std::size_t offset = 0, size = 0, array = 1;
    bool pointer = false;
    std::string base;
};
struct Monstey_DnaStruct {
    std::uint16_t type = 0;
    std::vector<Monstey_DnaField> fields;
    bool exact = false;
};
struct Monstey_BBlock {
    std::string code;
    std::uint32_t dna = 0;
    std::uint64_t address = 0, count = 0;
    bool shortEnd = false;
    Monstey_Bytes data;
};
std::string Monstey_FieldBase(const std::string &name) {
    std::size_t p = 0;
    while (p < name.size() && !(std::isalpha(static_cast<unsigned char>(name[p])) || name[p] == '_'))
        ++p;
    const auto begin = p;
    while (p < name.size() && (std::isalnum(static_cast<unsigned char>(name[p])) || name[p] == '_'))
        ++p;
    Monstey_BRequire(p > begin, "Invalid SDNA declarator");
    return name.substr(begin, p - begin);
}
std::size_t Monstey_ArrayCount(const std::string &name) {
    std::size_t total = 1, at = 0;
    const bool indirect = name.find("(*") != std::string::npos;
    const auto close = indirect ? name.find(')') : std::string::npos;
    while ((at = name.find('[', at)) != std::string::npos) {
        if (indirect && close != std::string::npos && at > close)
            break;
        const auto end = name.find(']', at);
        Monstey_BRequire(end != std::string::npos && end > at + 1, "Invalid SDNA array");
        std::size_t n = 0;
        for (std::size_t p = at + 1; p < end; ++p) {
            Monstey_BRequire(name[p] >= '0' && name[p] <= '9', "Non-numeric SDNA array");
            Monstey_BRequire(n <= (Monstey_MaxBlend - static_cast<unsigned>(name[p] - '0')) / 10,
                             "SDNA array overflow");
            n = n * 10 + static_cast<unsigned>(name[p] - '0');
        }
        Monstey_BRequire(n > 0 && n <= Monstey_MaxBlend / total, "SDNA array extent overflow");
        total *= n;
        at = end + 1;
    }
    return total;
}
} // namespace
struct Monstey_Blend::Monstey_Impl {
    std::filesystem::path source;
    Monstey_Bytes input, decoded, header;
    bool compressed = false, little = true;
    unsigned pointer = 8, format = 0, version = 0;
    std::vector<Monstey_BBlock> blocks;
    std::map<std::uint64_t, std::uint64_t> authored_owners;
    std::uint64_t next_address = 0;
    std::uint64_t AllocateAddress(std::size_t bytes) {
        if (!next_address) {
            next_address = 0x10000;
            for (const auto &b : blocks) {
                Monstey_BRequire(b.address <= 0x7fffffffffffffffull - b.data.size() - 0x100,
                                 "File pointer allocation overflow");
                next_address = std::max(next_address, (b.address + b.data.size() + 0x100) & ~0xfull);
            }
        }
        const auto address = next_address;
        Monstey_BRequire(address <= 0x7fffffffffffffffull - bytes - 0x100,
                         "File pointer allocation overflow");
        next_address = (address + bytes + 0x100) & ~0xfull;
        Monstey_BRequire(pointer == 8 || next_address <= 0xffffffff,
                         "32-bit file pointer allocation overflow");
        return address;
    }
    std::vector<std::string> names, types;
    std::vector<std::uint16_t> lengths;
    std::vector<Monstey_DnaStruct> structs;
    std::map<std::string, std::size_t, std::less<>> byName;
    std::map<std::uint16_t, std::size_t> byType;
    std::uint64_t I(Monstey_Span b, std::size_t p, unsigned size) const {
        return Monstey_BInt(b, p, size, little);
    }
    std::size_t Type(std::string_view name) const {
        const auto found = byName.find(name);
        Monstey_BRequire(found != byName.end(), "Unknown SDNA struct");
        return found->second;
    }
    mutable std::vector<std::size_t> pointer_scopes;
    std::size_t Scope(std::size_t block) const {
        Monstey_BRequire(block < blocks.size(), "Pointer owner block outside file");
        if (pointer_scopes.size() != blocks.size()) {
            pointer_scopes.resize(blocks.size());
            auto owner = static_cast<std::size_t>(-1);
            for (std::size_t i = 0; i < blocks.size(); ++i) {
                if (blocks[i].code.size() == 2)
                    owner = i;
                pointer_scopes[i] = owner;
            }
        }
        return pointer_scopes[block];
    }
    void ReadDna(const Monstey_Bytes &data) {
        std::size_t p = 0;
        const auto marker = [&](std::string_view value) {
            Monstey_BRequire(Monstey_BText(data, p, 4) == value, "Invalid SDNA section marker");
            p += 4;
        };
        const auto strings = [&](std::vector<std::string> &result) {
            const auto n = I(data, p, 4);
            p += 4;
            Monstey_BRequire(n <= 1000000, "SDNA string count limit");
            for (std::size_t i = 0; i < n; ++i) {
                const auto begin = p;
                while (p < data.size() && data[p])
                    ++p;
                Monstey_BRequire(p < data.size(), "Unterminated SDNA string");
                result.push_back(Monstey_BText(data, begin, p - begin));
                ++p;
            }
            p = (p + 3) & ~std::size_t(3);
        };
        marker("SDNA");
        marker("NAME");
        strings(names);
        marker("TYPE");
        strings(types);
        Monstey_BRequire(types.size() <= 65535, "SDNA type index exceeds UINT16");
        marker("TLEN");
        for (std::size_t i = 0; i < types.size(); ++i) {
            lengths.push_back(static_cast<std::uint16_t>(I(data, p, 2)));
            p += 2;
        }
        p = (p + 3) & ~std::size_t(3);
        marker("STRC");
        const auto n = I(data, p, 4);
        p += 4;
        Monstey_BRequire(n <= types.size(), "SDNA struct count exceeds types");
        for (std::size_t i = 0; i < n; ++i) {
            Monstey_DnaStruct s;
            s.type = static_cast<std::uint16_t>(I(data, p, 2));
            const auto count = I(data, p + 2, 2);
            p += 4;
            Monstey_BRequire(s.type < types.size() && !byType.contains(s.type),
                             "SDNA invalid or repeated struct type");
            std::size_t offset = 0;
            for (std::size_t j = 0; j < count; ++j) {
                Monstey_DnaField f;
                f.type = static_cast<std::uint16_t>(I(data, p, 2));
                f.name = static_cast<std::uint16_t>(I(data, p + 2, 2));
                p += 4;
                Monstey_BRequire(f.type < types.size() && f.name < names.size(),
                                 "SDNA field index outside table");
                f.pointer = names[f.name].find('*') != std::string::npos;
                f.base = Monstey_FieldBase(names[f.name]);
                f.array = Monstey_ArrayCount(names[f.name]);
                const auto scalar = f.pointer ? pointer : lengths[f.type];
                Monstey_BRequire(f.array <= Monstey_MaxBlend / std::max<std::size_t>(scalar, 1),
                                 "SDNA field extent overflow");
                f.offset = offset;
                f.size = f.array * scalar;
                Monstey_BRequire(f.size <= Monstey_MaxBlend - offset, "SDNA struct extent overflow");
                offset += f.size;
                s.fields.push_back(f);
            }
            s.exact = offset == lengths[s.type];
            Monstey_BRequire(offset <= lengths[s.type], "SDNA fields exceed declared struct length");
            byType[s.type] = i;
            Monstey_BRequire(byName.emplace(types[s.type], i).second, "Duplicate SDNA struct name");
            structs.push_back(std::move(s));
        }
        Monstey_BRequire(p == data.size(), "SDNA trailing bytes");
    }
    explicit Monstey_Impl(const std::filesystem::path &path) : Monstey_Impl(path, Monstey_ReadFile(path)) {}
    Monstey_Impl(const std::filesystem::path &path, Monstey_Bytes raw) : source(path), input(std::move(raw)) {
        decoded = Monstey_UnwrapBlend(input, compressed);
        Monstey_BRequire(Monstey_BText(decoded, 0, 7) == "BLENDER", "Expected native BLENDER header");
        Monstey_CheckRange(decoded, 0, 12);
        std::size_t p = 0;
        const auto number = [&](std::size_t at, std::size_t n) {
            unsigned out = 0;
            for (std::size_t i = 0; i < n; ++i) {
                const auto c = decoded[at + i];
                Monstey_BRequire(c >= '0' && c <= '9', "Invalid blend version digits");
                out = out * 10 + c - '0';
            }
            return out;
        };
        if (decoded[7] == '_' || decoded[7] == '-') {
            format = 0;
            pointer = decoded[7] == '_' ? 4u : 8u;
            Monstey_BRequire(decoded[8] == 'v' || decoded[8] == 'V', "Invalid legacy blend endian");
            little = decoded[8] == 'v';
            version = number(9, 3);
            p = 12;
        } else {
            Monstey_CheckRange(decoded, 0, 17);
            Monstey_BRequire(number(7, 2) == 17 && decoded[9] == '-' && number(10, 2) == 1 &&
                                 decoded[12] == 'v',
                             "Unknown modern blend file header");
            format = 1;
            pointer = 8;
            little = true;
            version = number(13, 4);
            p = 17;
        }
        header.assign(decoded.begin(), decoded.begin() + static_cast<std::ptrdiff_t>(p));
        bool end = false;
        std::optional<std::size_t> dna;
        while (p < decoded.size()) {
            Monstey_BRequire(blocks.size() < 1000000, "Blend block count limit");
            Monstey_BBlock b;
            b.code = Monstey_BCode(decoded, p);
            const auto size = format ? 32u : 16u + pointer;
            if (format == 0 && b.code == "ENDB" && decoded.size() - p == 8) {
                Monstey_BRequire(I(decoded, p + 4, 4) == 0, "Invalid short ENDB");
                b.shortEnd = true;
                blocks.push_back(std::move(b));
                p += 8;
                end = true;
                break;
            }
            Monstey_CheckRange(decoded, p, size);
            std::uint64_t length = 0;
            if (format) {
                b.dna = static_cast<std::uint32_t>(I(decoded, p + 4, 4));
                b.address = I(decoded, p + 8, 8);
                length = I(decoded, p + 16, 8);
                b.count = I(decoded, p + 24, 8);
            } else {
                length = I(decoded, p + 4, 4);
                b.address = I(decoded, p + 8, pointer);
                b.dna = static_cast<std::uint32_t>(I(decoded, p + 8 + pointer, 4));
                b.count = I(decoded, p + 12 + pointer, 4);
            }
            p += size;
            Monstey_BRequire(length <= Monstey_MaxBlend && b.count <= 0x7fffffffffffffffull,
                             "Invalid blend block length/count");
            Monstey_CheckRange(decoded, p, static_cast<std::size_t>(length));
            b.data.assign(decoded.begin() + static_cast<std::ptrdiff_t>(p),
                          decoded.begin() + static_cast<std::ptrdiff_t>(p + length));
            p += static_cast<std::size_t>(length);
            if (b.code == "DNA1") {
                Monstey_BRequire(!dna.has_value(), "Duplicate SDNA block");
                dna = blocks.size();
            }
            if (b.code == "ENDB") {
                Monstey_BRequire(length == 0 && b.count == 0, "Invalid ENDB");
                end = true;
                blocks.push_back(std::move(b));
                break;
            }
            blocks.push_back(std::move(b));
        }
        Monstey_BRequire(end && p == decoded.size() && dna.has_value(),
                         "Blend file missing DNA/ENDB or trailing bytes");
        ReadDna(blocks[*dna].data);
        std::set<std::pair<std::size_t, std::uint64_t>> addresses;
        for (const auto &b : blocks) {
            if (b.address && b.code != "REND" && b.code != "TEST" && b.code != "DNA1" && b.code != "ENDB")
                Monstey_BRequire(
                    addresses
                        .insert({b.code == "DATA" ? Scope(static_cast<std::size_t>(&b - blocks.data()))
                                                  : static_cast<std::size_t>(-1),
                                 b.address})
                        .second,
                    "Duplicate blend block pointer identifier");
            if (b.code != "DNA1" && b.code != "ENDB" && b.code != "REND" && b.code != "TEST" && b.count &&
                b.dna != 0) {
                Monstey_BRequire(b.dna < structs.size(), "Blend block SDNA index invalid");
                const auto bytes = lengths[structs[b.dna].type];
                Monstey_BRequire(bytes == 0 || b.count <= b.data.size() / bytes,
                                 "Typed blend block smaller than declared records");
            }
        }
    }
    struct Monstey_Location {
        std::size_t offset = 0, size = 0, array = 1;
        std::uint16_t type = 0;
        bool pointer = false;
    };
    Monstey_Location Location(const Monstey_BBlock &block, std::string_view path, std::size_t element) const {
        Monstey_BRequire(block.dna < structs.size() && element < block.count,
                         "Blend record index outside block");
        const auto *current = &structs[block.dna];
        Monstey_BRequire(current->exact, "SDNA struct padding is not qualified for editing");
        std::size_t base = element * lengths[current->type];
        std::size_t pos = 0;
        while (pos < path.size()) {
            const auto stop = path.find('.', pos);
            const auto segment =
                path.substr(pos, stop == std::string_view::npos ? path.size() - pos : stop - pos);
            const auto bracket = segment.find('[');
            const auto name = segment.substr(0, bracket);
            const auto found = std::find_if(current->fields.begin(), current->fields.end(),
                                            [&](const auto &f) { return f.base == name; });
            Monstey_BRequire(found != current->fields.end(), "Unknown SDNA field path");
            base += found->offset;
            Monstey_Location result{base, found->size, found->array, found->type, found->pointer};
            if (bracket != std::string_view::npos) {
                Monstey_BRequire(segment.back() == ']', "Invalid SDNA array selector");
                std::size_t index = 0;
                for (std::size_t j = bracket + 1; j + 1 < segment.size(); ++j) {
                    Monstey_BRequire(segment[j] >= '0' && segment[j] <= '9', "Invalid SDNA array index");
                    Monstey_BRequire(index <= Monstey_MaxBlend / 10, "SDNA selector overflow");
                    index = index * 10 + static_cast<unsigned>(segment[j] - '0');
                }
                Monstey_BRequire(index < found->array, "SDNA array selector outside field");
                result.size = found->pointer ? pointer : lengths[found->type];
                result.array = 1;
                base += index * result.size;
                result.offset = base;
            }
            if (stop == std::string_view::npos) {
                Monstey_CheckRange(block.data, result.offset, result.size);
                return result;
            }
            Monstey_BRequire(!found->pointer && result.array == 1 && byType.contains(found->type),
                             "Cannot traverse pointer/array without explicit resolution");
            current = &structs[byType.at(found->type)];
            Monstey_BRequire(current->exact, "Nested SDNA struct layout unqualified");
            pos = stop + 1;
        }
        throw Monstey_Error("blend_field", "Field path is empty");
    }
    Monstey_J Value(const Monstey_BBlock &b, const Monstey_Location &f, std::size_t index = 0) const {
        const auto n = f.pointer ? pointer : lengths[f.type];
        const auto at = f.offset + index * n;
        const auto &type = types[f.type];
        if (f.pointer)
            return Monstey_J::object({{"pointer", I(b.data, at, pointer)},
                                      {"hex", Monstey_Hex(I(b.data, at, pointer), pointer * 2)}});
        if (byType.contains(f.type))
            return Monstey_J::object(
                {{"struct", type}, {"offset", static_cast<std::uint64_t>(at)}, {"bytes", n}});
        if (type == "float" || type == "double") {
            const auto bits = I(b.data, at, n);
            const double value =
                n == 4 ? static_cast<double>(std::bit_cast<float>(static_cast<std::uint32_t>(bits)))
                       : std::bit_cast<double>(bits);
            if (!std::isfinite(value))
                return Monstey_J::object({{"nonfinite", true}, {"raw_bits", Monstey_Hex(bits, n * 2)}});
            return value;
        }
        Monstey_BRequire(n == 1 || n == 2 || n == 4 || n == 8, "Unsupported SDNA scalar width");
        const auto v = I(b.data, at, n);
        const bool sign = type == "char" || type == "short" || type == "int" || type == "long" ||
                          type == "int8_t" || type == "int16_t" || type == "int32_t" || type == "int64_t";
        if (!sign)
            return v;
        const unsigned shift = (8 - n) * 8;
        return static_cast<std::int64_t>(v << shift) >> shift;
    }
};
Monstey_Blend::Monstey_Blend(const std::filesystem::path &source)
    : impl_(std::make_unique<Monstey_Impl>(source)) {}
Monstey_Blend::~Monstey_Blend() = default;
Monstey_Blend::Monstey_Blend(Monstey_Blend &&) noexcept = default;
Monstey_Blend &Monstey_Blend::operator=(Monstey_Blend &&) noexcept = default;
Monstey_J Monstey_Blend::Describe() const {
    const auto &m = *impl_;
    std::uint64_t exact = 0;
    for (const auto &s : m.structs)
        if (s.exact)
            ++exact;
    return Monstey_J::object(
        {{"format", "BLEND"},
         {"container_version", m.format},
         {"blender_file_version", m.version},
         {"pointer_bytes", m.pointer},
         {"little_endian", m.little},
         {"zstd_compressed",
          m.compressed && !(m.input.size() >= 2 && m.input[0] == 0x1f && m.input[1] == 0x8b)},
         {"gzip_compressed", m.input.size() >= 2 && m.input[0] == 0x1f && m.input[1] == 0x8b},
         {"source_sha256", Monstey_HashSha256(m.input)},
         {"decoded_sha256", Monstey_HashSha256(m.decoded)},
         {"source_bytes", static_cast<std::uint64_t>(m.input.size())},
         {"decoded_bytes", static_cast<std::uint64_t>(m.decoded.size())},
         {"blocks", static_cast<std::uint64_t>(m.blocks.size())},
         {"types", static_cast<std::uint64_t>(m.types.size())},
         {"structs", static_cast<std::uint64_t>(m.structs.size())},
         {"exact_struct_layouts", exact},
         {"runtime_verified", false}});
}
Monstey_J Monstey_Blend::Structs(std::string_view query) const {
    auto out = Monstey_J::array();
    for (std::size_t i = 0; i < impl_->structs.size(); ++i) {
        const auto &s = impl_->structs[i];
        const auto &name = impl_->types[s.type];
        if (name.find(query) != std::string::npos)
            out.as_array().push_back(
                Monstey_J::object({{"index", static_cast<std::uint64_t>(i)},
                                   {"name", name},
                                   {"bytes", impl_->lengths[s.type]},
                                   {"fields", static_cast<std::uint64_t>(s.fields.size())},
                                   {"exact", s.exact}}));
    }
    return out;
}
Monstey_J Monstey_Blend::Fields(std::string_view type) const {
    const auto &s = impl_->structs[impl_->Type(type)];
    auto out = Monstey_J::array();
    for (const auto &f : s.fields)
        out.as_array().push_back(Monstey_J::object({{"name", f.base},
                                                    {"declarator", impl_->names[f.name]},
                                                    {"type", impl_->types[f.type]},
                                                    {"offset", static_cast<std::uint64_t>(f.offset)},
                                                    {"bytes", static_cast<std::uint64_t>(f.size)},
                                                    {"array", static_cast<std::uint64_t>(f.array)},
                                                    {"pointer", f.pointer}}));
    return out;
}
Monstey_J Monstey_Blend::Blocks(std::string_view code, std::size_t offset, std::size_t limit) const {
    Monstey_BRequire(limit <= 10000, "Blend page limit exceeds 10000");
    auto rows = Monstey_J::array();
    std::size_t total = 0;
    for (std::size_t i = 0; i < impl_->blocks.size(); ++i) {
        const auto &b = impl_->blocks[i];
        if (!code.empty() && b.code != code)
            continue;
        if (total++ < offset || rows.size() >= limit)
            continue;
        rows.as_array().push_back(Monstey_J::object(
            {{"index", static_cast<std::uint64_t>(i)},
             {"code", b.code},
             {"address", b.address},
             {"address_hex", Monstey_Hex(b.address, impl_->pointer * 2)},
             {"bytes", static_cast<std::uint64_t>(b.data.size())},
             {"count", b.count},
             {"sdna", b.dna},
             {"type", b.dna < impl_->structs.size() ? impl_->types[impl_->structs[b.dna].type] : ""}}));
    }
    return Monstey_J::object({{"rows", rows},
                              {"total", static_cast<std::uint64_t>(total)},
                              {"offset", static_cast<std::uint64_t>(offset)},
                              {"limit", static_cast<std::uint64_t>(limit)}});
}
Monstey_J Monstey_Blend::ReadField(std::size_t block, std::string_view path, std::size_t element) const {
    Monstey_BRequire(block < impl_->blocks.size(), "Blend block index invalid");
    const auto &b = impl_->blocks[block];
    const auto f = impl_->Location(b, path, element);
    if (!f.pointer && impl_->types[f.type] == "char" && f.array > 1) {
        auto text = Monstey_BText(b.data, f.offset, f.size);
        const auto zero = text.find(char(0));
        if (zero != std::string::npos)
            text.resize(zero);
        return Monstey_DisplayUtf8(text);
    }
    if (f.array == 1)
        return impl_->Value(b, f);
    auto out = Monstey_J::array();
    for (std::size_t i = 0; i < f.array; ++i)
        out.as_array().push_back(impl_->Value(b, f, i));
    return out;
}
Monstey_J Monstey_Blend::SetField(std::size_t block, std::string_view path, const Monstey_J &value,
                                  std::size_t element) {
    Monstey_BRequire(block < impl_->blocks.size(), "Blend block index invalid");
    auto &b = impl_->blocks[block];
    const auto f = impl_->Location(b, path, element);
    const auto &type = impl_->types[f.type];
    auto staged = b.data;
    if (!f.pointer && type == "char" && f.array > 1 && value.is_string()) {
        const auto &text = value.as_string();
        Monstey_BRequire(text.size() < f.size && text.find(char(0)) == std::string::npos,
                         "SDNA name exceeds field");
        std::fill(staged.begin() + static_cast<std::ptrdiff_t>(f.offset),
                  staged.begin() + static_cast<std::ptrdiff_t>(f.offset + f.size), std::uint8_t(0));
        std::copy(text.begin(), text.end(), staged.begin() + static_cast<std::ptrdiff_t>(f.offset));
    } else {
        Monstey_BRequire(!impl_->byType.contains(f.type) || f.pointer,
                         "Set nested struct members explicitly");
        const auto n = f.pointer ? impl_->pointer : impl_->lengths[f.type];
        if (f.array > 1)
            Monstey_BRequire(value.is_array() && value.size() == f.array, "SDNA array shape mismatch");
        const bool sign = type == "char" || type == "short" || type == "int" || type == "long" ||
                          type == "int8_t" || type == "int16_t" || type == "int32_t" || type == "int64_t";
        for (std::size_t i = 0; i < f.array; ++i) {
            const auto &v = f.array > 1 ? value[i] : value;
            std::uint64_t bits = 0;
            if (type == "float" && !f.pointer) {
                const auto number = static_cast<float>(v.as_number());
                Monstey_BRequire(std::isfinite(number), "SDNA FLOAT32 overflow");
                bits = std::bit_cast<std::uint32_t>(number);
            } else if (type == "double" && !f.pointer)
                bits = std::bit_cast<std::uint64_t>(v.as_number());
            else if (type == "bool" && !f.pointer)
                bits = v.as_bool() ? 1u : 0u;
            else if (sign && !f.pointer) {
                const auto number = v.as_int();
                if (n < 8) {
                    const auto minimum = -(1ll << (n * 8 - 1)), maximum = (1ll << (n * 8 - 1)) - 1;
                    Monstey_BRequire(number >= minimum && number <= maximum,
                                     "Signed SDNA value outside field");
                }
                bits = static_cast<std::uint64_t>(number);
            } else {
                bits = v.as_uint();
                if (n < 8)
                    Monstey_BRequire(bits < (1ull << (n * 8)), "Unsigned SDNA value outside field");
            }
            Monstey_BPut(staged, f.offset + i * n, n, bits, impl_->little);
        }
    }
    b.data = std::move(staged);
    return ReadField(block, path, element);
}
Monstey_Bytes Monstey_Blend::BytesAtAddress(std::uint64_t address, std::size_t length,
                                            std::size_t owner) const {
    Monstey_BRequire(address != 0, "Cannot resolve null file pointer");
    const auto block = BlockAtAddress(address, owner);
    const auto &b = impl_->blocks[block];
    const auto offset = static_cast<std::size_t>(address - b.address);
    Monstey_CheckRange(b.data, offset, length);
    return Monstey_Bytes(b.data.begin() + static_cast<std::ptrdiff_t>(offset),
                         b.data.begin() + static_cast<std::ptrdiff_t>(offset + length));
}
Monstey_J Monstey_Blend::NewBlock(std::string_view code, std::string_view type, std::size_t count) {
    Monstey_BRequire(!code.empty() && code.size() <= 4 && code != "DNA1" && code != "ENDB",
                     "Invalid new blend block code");
    const auto dna = impl_->Type(type);
    const auto bytes = impl_->lengths[impl_->structs[dna].type];
    Monstey_BRequire(impl_->structs[dna].exact && count > 0 && bytes > 0 && count <= Monstey_MaxBlend / bytes,
                     "New SDNA block extent invalid");
    const auto address = impl_->AllocateAddress(bytes * count);
    Monstey_BBlock block;
    block.code = code;
    block.dna = static_cast<std::uint32_t>(dna);
    block.address = address;
    block.count = count;
    block.data.resize(bytes * count);
    const auto index = static_cast<std::size_t>(std::find_if(impl_->blocks.begin(), impl_->blocks.end(),
                                                             [](const auto &b) { return b.code == "DNA1"; }) -
                                                impl_->blocks.begin());
    impl_->blocks.insert(impl_->blocks.begin() + static_cast<std::ptrdiff_t>(index), std::move(block));
    return Monstey_J::object({{"block", static_cast<std::uint64_t>(index)},
                              {"address", address},
                              {"bytes", static_cast<std::uint64_t>(bytes * count)},
                              {"sdna", static_cast<std::uint64_t>(dna)}});
}
std::size_t Monstey_Blend::BlockAtAddress(std::uint64_t address, std::size_t owner) const {
    Monstey_BRequire(address != 0, "Cannot resolve null file pointer");
    const auto missing = static_cast<std::size_t>(-1);
    const auto scope = owner == missing ? missing : impl_->Scope(owner);
    auto find = [&](bool local, bool interior) {
        std::size_t found = missing;
        for (std::size_t i = 0; i < impl_->blocks.size(); ++i) {
            const auto &b = impl_->blocks[i];
            if (!b.address || b.code == "REND" || b.code == "TEST" || b.code == "DNA1" || b.code == "ENDB")
                continue;
            if (local && (b.code != "DATA" || impl_->Scope(i) != scope))
                continue;
            const bool match =
                interior ? address > b.address && address - b.address < b.data.size() : b.address == address;
            if (!match)
                continue;
            Monstey_BRequire(found == missing, "Ambiguous file pointer; specify its owning ID datablock");
            found = i;
        }
        return found;
    };
    if (scope != missing) {
        const auto found = find(true, false);
        if (found != missing)
            return found;
    }
    if (const auto found = find(false, false); found != missing)
        return found;
    if (scope != missing) {
        const auto found = find(true, true);
        if (found != missing)
            return found;
    }
    if (const auto found = find(false, true); found != missing)
        return found;
    throw Monstey_Error("blend_pointer", "Stored file pointer does not resolve");
}
Monstey_Bytes Monstey_Blend::BlockData(std::size_t block) const {
    Monstey_BRequire(block < impl_->blocks.size(), "Blend block index invalid");
    return impl_->blocks[block].data;
}
Monstey_J Monstey_Blend::CloneBlock(std::size_t block) {
    Monstey_BRequire(block < impl_->blocks.size(), "Blend block index invalid");
    const auto original = impl_->blocks[block];
    Monstey_BRequire(original.code != "DNA1" && original.code != "ENDB" &&
                         original.dna < impl_->structs.size(),
                     "Cannot clone metadata block");
    auto result = NewBlock(original.code, impl_->types[impl_->structs[original.dna].type],
                           static_cast<std::size_t>(original.count));
    impl_->blocks[static_cast<std::size_t>(result["block"].as_uint())].data = original.data;
    return result;
}
Monstey_J Monstey_Blend::NewData(Monstey_Bytes data) {
    Monstey_BRequire(!data.empty() && data.size() <= Monstey_MaxBlend, "Raw DATA extent invalid");
    const auto address = impl_->AllocateAddress(data.size());
    Monstey_BBlock b;
    b.code = "DATA";
    b.address = address;
    b.count = 1;
    b.dna = 0;
    b.data = std::move(data);
    const auto index =
        static_cast<std::size_t>(std::find_if(impl_->blocks.begin(), impl_->blocks.end(),
                                              [](const auto &item) { return item.code == "DNA1"; }) -
                                 impl_->blocks.begin());
    const auto size = b.data.size();
    impl_->blocks.insert(impl_->blocks.begin() + static_cast<std::ptrdiff_t>(index), std::move(b));
    return Monstey_J::object({{"block", static_cast<std::uint64_t>(index)},
                              {"address", address},
                              {"bytes", static_cast<std::uint64_t>(size)},
                              {"sdna", 0u}});
}
void Monstey_Blend::SetDataOwner(std::size_t block, std::size_t owner) {
    Monstey_BRequire(block < impl_->blocks.size() && owner < impl_->blocks.size(),
                     "Data ownership index invalid");
    const auto &data = impl_->blocks[block];
    const auto &id = impl_->blocks[owner];
    Monstey_BRequire(data.code == "DATA" && id.code.size() == 2 && id.address != 0,
                     "Data ownership requires DATA and ID blocks");
    impl_->authored_owners[data.address] = id.address;
}
void Monstey_Blend::ReplaceTypedData(std::size_t block, Monstey_Bytes data) {
    Monstey_BRequire(block < impl_->blocks.size(), "Typed block index invalid");
    auto &b = impl_->blocks[block];
    Monstey_BRequire(b.dna < impl_->structs.size() && impl_->structs[b.dna].exact &&
                         data.size() == b.data.size(),
                     "Typed replacement must preserve its exact SDNA extent");
    b.data = std::move(data);
}
void Monstey_Blend::ReplaceData(std::size_t block, Monstey_Bytes data) {
    Monstey_BRequire(block < impl_->blocks.size(), "Blend block index invalid");
    auto &b = impl_->blocks[block];
    Monstey_BRequire(b.code == "DATA" && b.dna == 0 && !data.empty() && data.size() <= Monstey_MaxBlend,
                     "ReplaceData requires a raw DATA block");
    b.data = std::move(data);
    impl_->next_address=0;
}
Monstey_J Monstey_Blend::Write(const std::filesystem::path &output) const {
    const std::array<std::filesystem::path, 1> sources = {impl_->source};
    Monstey_RequireOfflineOutput(output, sources);
    auto bytes = impl_->header;
    std::vector<const Monstey_BBlock*> ordered;
    for(const auto& block:impl_->blocks){
      if(impl_->authored_owners.contains(block.address)&&block.code=="DATA")continue;
      ordered.push_back(&block);
      if(block.code.size()==2)for(const auto& child:impl_->blocks){auto it=impl_->authored_owners.find(child.address);if(child.code=="DATA"&&it!=impl_->authored_owners.end()&&it->second==block.address)ordered.push_back(&child);}
    }
    for (const auto* item : ordered) {
        const auto& b=*item;
        for (unsigned i = 0; i < 4; ++i)
            bytes.push_back(i < b.code.size() ? static_cast<std::uint8_t>(b.code[i]) : 0);
        if (b.shortEnd) {
            Monstey_BAppend(bytes, 4, 0, impl_->little);
            continue;
        }
        if (impl_->format) {
            Monstey_BAppend(bytes, 4, b.dna, true);
            Monstey_BAppend(bytes, 8, b.address, true);
            Monstey_BAppend(bytes, 8, b.data.size(), true);
            Monstey_BAppend(bytes, 8, b.count, true);
        } else {
            Monstey_BRequire(b.data.size() <= 0x7fffffff && b.count <= 0x7fffffff,
                             "Legacy block exceeds signed UINT32 extent");
            Monstey_BAppend(bytes, 4, b.data.size(), impl_->little);
            Monstey_BAppend(bytes, impl_->pointer, b.address, impl_->little);
            Monstey_BAppend(bytes, 4, b.dna, impl_->little);
            Monstey_BAppend(bytes, 4, b.count, impl_->little);
        }
        bytes.insert(bytes.end(), b.data.begin(), b.data.end());
    }
    Monstey_Impl check(output, bytes);
    Monstey_BRequire(check.blocks.size() == impl_->blocks.size(),
                     "Blend writer readback block count mismatch");
    Monstey_WriteNewFile(output, bytes);
    return Monstey_J::object({{"output", Monstey_Utf8(output.wstring())},
                              {"sha256", Monstey_HashSha256(bytes)},
                              {"bytes", static_cast<std::uint64_t>(bytes.size())},
                              {"compressed", false},
                              {"byte_exact_decoded", bytes == impl_->decoded},
                              {"blocks", static_cast<std::uint64_t>(impl_->blocks.size())},
                              {"runtime_verified", false}});
}
} // namespace Monstey
