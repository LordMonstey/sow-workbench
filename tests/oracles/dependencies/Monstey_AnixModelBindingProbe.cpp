// Offline diagnostic. No original executable is loaded or executed.
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using Monstey_Bytes = std::vector<std::uint8_t>;
using Monstey_View = std::span<const std::uint8_t>;
constexpr std::size_t Monstey_MaxBytes = 512u * 1024u * 1024u;
constexpr std::uint32_t Monstey_Pos = 0x00ce66fcu;
constexpr std::uint32_t Monstey_Rotation = 0x27436396u;
constexpr std::uint16_t Monstey_Unmapped = 0xffffu;
void Monstey_Require(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}
void Monstey_Range(Monstey_View bytes, std::size_t at, std::size_t size) {
    Monstey_Require(at <= bytes.size() && size <= bytes.size() - at, "range exceeds owning bytes");
}
std::uint16_t Monstey_U16(Monstey_View bytes, std::size_t at) {
    Monstey_Range(bytes, at, 2);
    return static_cast<std::uint16_t>(bytes[at] | (static_cast<unsigned>(bytes[at + 1]) << 8));
}
std::uint32_t Monstey_U32(Monstey_View bytes, std::size_t at) {
    Monstey_Range(bytes, at, 4);
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(bytes[at + i]) << (i * 8);
    return value;
}
std::uint64_t Monstey_U64(Monstey_View bytes, std::size_t at) {
    return Monstey_U32(bytes, at) | (static_cast<std::uint64_t>(Monstey_U32(bytes, at + 4)) << 32);
}
void Monstey_Put32(Monstey_Bytes& bytes, std::size_t at, std::uint32_t value) {
    Monstey_Range(bytes, at, 4);
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
Monstey_Bytes Monstey_Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    Monstey_Require(file.good(), "cannot open input");
    const auto length = file.tellg();
    Monstey_Require(length >= 0 && static_cast<std::uint64_t>(length) <= Monstey_MaxBytes, "input exceeds limit");
    Monstey_Bytes bytes(static_cast<std::size_t>(length));
    file.seekg(0);
    if (!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Monstey_Require(file.good(), "short input read");
    return bytes;
}
unsigned Monstey_HexDigit(char digit) {
    if (digit >= '0' && digit <= '9') return static_cast<unsigned>(digit - '0');
    if (digit >= 'a' && digit <= 'f') return static_cast<unsigned>(digit - 'a' + 10);
    if (digit >= 'A' && digit <= 'F') return static_cast<unsigned>(digit - 'A' + 10);
    throw std::runtime_error("invalid hex digit");
}
Monstey_Bytes Monstey_Unhex(std::string_view text) {
    Monstey_Require(!text.empty() && text.size() % 2 == 0 && text.size() / 2 <= Monstey_MaxBytes, "invalid hex size");
    Monstey_Bytes bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t at = 0; at < text.size(); at += 2)
        bytes.push_back(static_cast<std::uint8_t>((Monstey_HexDigit(text[at]) << 4) | Monstey_HexDigit(text[at + 1])));
    return bytes;
}
std::string Monstey_Hex(Monstey_View bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const auto value : bytes) { text.push_back(digits[value >> 4]); text.push_back(digits[value & 15]); }
    return text;
}
std::uint32_t Monstey_Hash(std::string_view text) {
    constexpr std::string_view table =
        "0000000000000000000000000000000000000000000000000000000000000000"
        "253a333c3d3e403242434128362737341b1c1d1e1f2021222324313038293935"
        "3b0102030405060708090a0b0c0d0e0f101112131415161718191a2c2a2d3f26"
        "440102030405060708090a0b0c0d0e0f101112131415161718191a2e2b2f4500"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000";
    static_assert(table.size() == 512);
    std::uint32_t hash = 0;
    for (const unsigned char value : text)
        hash = hash * 0x397u + (Monstey_HexDigit(table[value * 2]) << 4) + Monstey_HexDigit(table[value * 2 + 1]);
    return hash;
}
struct Monstey_Channel {
    std::uint32_t bone = 0, type = 0;
    bool operator==(const Monstey_Channel&) const = default;
};
bool Monstey_ChannelLess(const Monstey_Channel& left, const Monstey_Channel& right) {
    return left.type < right.type || (left.type == right.type && left.bone < right.bone);
}
struct Monstey_Destination { Monstey_Channel identity; std::uint16_t byteOffset = 0; };
struct Monstey_MapResult { std::vector<std::uint16_t> offsets; std::size_t matchedDescriptors = 0; };
Monstey_MapResult Monstey_BuildMap(std::span<const Monstey_Channel> shared, std::span<const Monstey_Destination> destinations) {
    Monstey_Require(shared.size() <= 65535 && std::is_sorted(shared.begin(), shared.end(), Monstey_ChannelLess),
                    "shared pairs must be sorted by unsigned type then bone");
    Monstey_MapResult result{std::vector<std::uint16_t>(shared.size(), Monstey_Unmapped), 0};
    for (const auto& destination : destinations) {
        const auto found = std::lower_bound(shared.begin(), shared.end(), destination.identity, Monstey_ChannelLess);
        if (found != shared.end() && *found == destination.identity) {
            result.offsets[static_cast<std::size_t>(found - shared.begin())] = destination.byteOffset;
            ++result.matchedDescriptors;
        }
    }
    // Native frees the vector when matchedDescriptors==0; retaining sentinels here makes the diagnostic explicit.
    return result;
}
std::vector<std::size_t> Monstey_Route(std::span<const std::uint16_t> tracks, const Monstey_MapResult& map) {
    std::vector<std::size_t> result;
    for (const auto channel : tracks)
        result.push_back(channel < map.offsets.size() && map.offsets[channel] != Monstey_Unmapped
                             ? map.offsets[channel] : std::numeric_limits<std::size_t>::max());
    return result; // max is diagnostic scratch identity, never an output offset.
}
bool Monstey_AnyReal(std::span<const std::size_t> routes) {
    return std::any_of(routes.begin(), routes.end(), [](auto value) { return value != std::numeric_limits<std::size_t>::max(); });
}
const Monstey_Bytes& Monstey_DefaultBuffer(unsigned descriptorFlags, const Monstey_Bytes& normal, const Monstey_Bytes& alternate) {
    return (descriptorFlags & 1u) == 0 ? normal : alternate;
}
std::vector<Monstey_Channel> Monstey_ParseSharedPairs(Monstey_View bytes) {
    Monstey_Require(bytes.size() % 8 == 0 && bytes.size() / 8 <= 65535, "bad raw pair extent");
    std::vector<Monstey_Channel> result;
    for (std::size_t at = 0; at < bytes.size(); at += 8) result.push_back({Monstey_U32(bytes, at), Monstey_U32(bytes, at + 4)});
    return result;
}
std::vector<Monstey_Destination> Monstey_ParseDestinations(Monstey_View bytes) {
    Monstey_Require(bytes.size() % 16 == 0, "bad destination descriptor extent");
    std::vector<Monstey_Destination> result;
    for (std::size_t at = 0; at < bytes.size(); at += 16)
        result.push_back({{Monstey_U32(bytes, at), Monstey_U32(bytes, at + 4)}, Monstey_U16(bytes, at + 12)});
    return result;
}
std::vector<Monstey_Channel> Monstey_ParseAnixShared(Monstey_View bytes) {
    Monstey_Range(bytes, 0, 48);
    Monstey_Require(Monstey_U32(bytes, 0) == 0x58494e41u && Monstey_U32(bytes, 4) == 44 && Monstey_U32(bytes, 8) == 48,
                    "expected diagnostic ANIX44 header profile");
    Monstey_Require(Monstey_U32(bytes, 12) == bytes.size(), "ANIX declared file size mismatch");
    const auto count = Monstey_U32(bytes, 16), begin = Monstey_U32(bytes, 20), payload = Monstey_U32(bytes, 24);
    const auto payloadBytes = Monstey_U32(bytes, 28), metadata = Monstey_U32(bytes, 32), metadataBytes = Monstey_U32(bytes, 36);
    Monstey_Require(count > 0 && count <= 65535 && begin >= 48 && begin <= payload, "ANIX channel header range");
    Monstey_Require(static_cast<std::uint64_t>(payload) + payloadBytes <= metadata &&
                    static_cast<std::uint64_t>(metadata) + metadataBytes == bytes.size(), "ANIX independent section extents");
    Monstey_Range(bytes, begin, payload - begin);
    std::size_t at = begin;
    std::vector<Monstey_Channel> shared;
    while (shared.size() < count) {
        Monstey_Require(at <= payload && 8 <= payload - at, "ANIX truncated channel group");
        const auto type = Monstey_U32(bytes, at), group = Monstey_U32(bytes, at + 4);
        at += 8;
        Monstey_Require(group > 0 && group <= count - shared.size() && group <= (payload - at) / 4, "ANIX channel group count");
        for (std::size_t i = 0; i < group; ++i) shared.push_back({Monstey_U32(bytes, at + 4 * i), type});
        at += static_cast<std::size_t>(group) * 4;
    }
    return shared; // Codec descriptors, payload and footer semantics are outside this diagnostic parser.
}
struct Monstey_Bone {
    std::string name;
    std::uint32_t hash = 0, children = 0;
    std::uint8_t rawFlag = 0;
    std::vector<std::size_t> childSources;
};
struct Monstey_Skeleton {
    std::vector<Monstey_Bone> source;
    std::vector<std::size_t> runtimeToSource;
    std::size_t boneStart = 0, boneEnd = 0, opaqueTail = 0;
};
std::string Monstey_BoundedString(Monstey_View bytes, std::size_t at, std::size_t end) {
    Monstey_Require(at < end && end <= bytes.size(), "SKEL string offset outside pool");
    const auto first = at;
    while (at < end && bytes[at] != 0) ++at;
    Monstey_Require(at < end, "unterminated SKEL string");
    return std::string(reinterpret_cast<const char*>(bytes.data() + first), at - first);
}
Monstey_Skeleton Monstey_ParseSkel(Monstey_View bytes) {
    Monstey_Range(bytes, 0, 96);
    Monstey_Require(Monstey_U32(bytes, 0) == 0x4c454b53u && Monstey_U32(bytes, 4) == 17, "expected SKEL17 diagnostic profile");
    const auto count = Monstey_U32(bytes, 40), stringBytes = Monstey_U32(bytes, 12), tableCount = Monstey_U32(bytes, 32);
    Monstey_Require(count > 0 && count < 256 && tableCount <= bytes.size() / 4, "SKEL pose diagnostic count limit");
    std::size_t at = 96;
    for (std::size_t i = 0; i < tableCount; ++i) {
        const auto rows = Monstey_U32(bytes, at); at += 4;
        Monstey_Require(rows <= (bytes.size() - at) / 14, "SKEL prefix row count");
        at += static_cast<std::size_t>(rows) * 14;
    }
    Monstey_Require(Monstey_U32(bytes, at) == 0xdcdcff00u, "SKEL missing string marker"); at += 4;
    const auto pool = at; Monstey_Range(bytes, at, stringBytes); at += stringBytes;
    Monstey_Skeleton skeleton; skeleton.boneStart = at;
    Monstey_Range(bytes, at, static_cast<std::size_t>(count) * 37);
    std::vector<std::pair<std::size_t, std::uint32_t>> stack;
    for (std::size_t i = 0; i < count; ++i) {
        while (!stack.empty() && stack.back().second == 0) stack.pop_back();
        Monstey_Require(i == 0 || !stack.empty(), "SKEL diagnostic requires one complete rooted preorder tree");
        const auto nameOffset = Monstey_U32(bytes, at);
        Monstey_Require(nameOffset < stringBytes, "SKEL invalid name offset");
        Monstey_Bone bone;
        bone.name = Monstey_BoundedString(bytes, pool + nameOffset, pool + stringBytes);
        bone.hash = Monstey_Hash(bone.name); bone.rawFlag = bytes[at + 4];
        bone.children = Monstey_U32(bytes, at + 33);
        Monstey_Require(bone.children <= count - i - 1, "SKEL children exceed remaining source rows");
        if (!stack.empty()) { skeleton.source[stack.back().first].childSources.push_back(i); --stack.back().second; }
        skeleton.source.push_back(std::move(bone));
        if (skeleton.source.back().children) stack.emplace_back(i, skeleton.source.back().children);
        at += 37;
    }
    Monstey_Require(std::all_of(stack.begin(), stack.end(), [](const auto& row) { return row.second == 0; }), "SKEL incomplete child tree");
    skeleton.runtimeToSource.push_back(0);
    const auto reserveChildren = [&](auto&& self, std::size_t source) -> void {
        const auto& children = skeleton.source[source].childSources;
        skeleton.runtimeToSource.insert(skeleton.runtimeToSource.end(), children.begin(), children.end());
        for (const auto child : children) self(self, child);
    };
    reserveChildren(reserveChildren, 0);
    Monstey_Require(skeleton.runtimeToSource.size() == count, "SKEL runtime permutation count");
    skeleton.boneEnd = at; skeleton.opaqueTail = bytes.size() - at;
    return skeleton; // No interpretation or validation of post-bone SKEL sections.
}
std::vector<Monstey_Destination> Monstey_PoseDestinations(const Monstey_Skeleton& skeleton) {
    std::vector<Monstey_Destination> result;
    for (std::size_t runtime = 0; runtime < skeleton.runtimeToSource.size(); ++runtime) {
        const auto& bone = skeleton.source[skeleton.runtimeToSource[runtime]];
        const auto positionType = (bone.rawFlag & 1u) == 0 ? Monstey_Pos : 0xffffu;
        result.push_back({{bone.hash, positionType}, static_cast<std::uint16_t>(runtime * 32)});
        result.push_back({{bone.hash, Monstey_Rotation}, static_cast<std::uint16_t>(runtime * 32 + 16)});
    }
    return result;
}
struct Monstey_PE {
    Monstey_View bytes;
    std::size_t sectionTable = 0;
    std::uint16_t sectionCount = 0;
    std::uint64_t imageBase = 0;
    explicit Monstey_PE(Monstey_View input) : bytes(input) {
        Monstey_Require(Monstey_U16(bytes, 0) == 0x5a4du, "PE missing DOS signature");
        const auto header = static_cast<std::size_t>(Monstey_U32(bytes, 0x3c));
        Monstey_Range(bytes, header, 24);
        Monstey_Require(Monstey_U32(bytes, header) == 0x4550u && Monstey_U16(bytes, header + 4) == 0x8664u, "expected x64 PE");
        sectionCount = Monstey_U16(bytes, header + 6);
        const auto optionalSize = Monstey_U16(bytes, header + 20);
        Monstey_Require(optionalSize >= 32, "short PE optional header");
        Monstey_Range(bytes, header + 24, optionalSize);
        Monstey_Require(Monstey_U16(bytes, header + 24) == 0x20bu, "expected PE32+");
        imageBase = Monstey_U64(bytes, header + 48);
        sectionTable = header + 24 + optionalSize;
        Monstey_Range(bytes, sectionTable, static_cast<std::size_t>(sectionCount) * 40);
        for (std::size_t i = 0; i < sectionCount; ++i) {
            const auto section = sectionTable + i * 40;
            Monstey_Range(bytes, Monstey_U32(bytes, section + 20), Monstey_U32(bytes, section + 16));
        }
    }
    Monstey_View Monstey_At(std::uint64_t address, std::size_t size) const {
        Monstey_Require(address >= imageBase && address - imageBase <= UINT32_MAX, "PE VA outside RVA space");
        const auto rva = address - imageBase;
        for (std::size_t i = 0; i < sectionCount; ++i) {
            const auto section = sectionTable + i * 40;
            const auto start = Monstey_U32(bytes, section + 12), rawSize = Monstey_U32(bytes, section + 16);
            if (rva >= start && rva - start <= rawSize && size <= rawSize - (rva - start)) {
                const auto raw = static_cast<std::size_t>(Monstey_U32(bytes, section + 20)) + static_cast<std::size_t>(rva - start);
                Monstey_Range(bytes, raw, size);
                return bytes.subspan(raw, size);
            }
        }
        throw std::runtime_error("PE VA has no complete raw section range");
    }
};
std::size_t Monstey_VerifyNative(const std::filesystem::path& manifest, const std::filesystem::path& first, const std::filesystem::path& second) {
    const auto a = Monstey_Read(first), b = Monstey_Read(second);
    const Monstey_PE p(a), q(b);
    Monstey_Require(p.imageBase == 0x140000000ull && q.imageBase == p.imageBase, "native PE image base differs");
    std::ifstream input(manifest);
    Monstey_Require(input.good(), "cannot open native manifest");
    std::string line;
    std::size_t rows = 0;
    while (std::getline(input, line)) {
        Monstey_Require(!line.empty() && line.size() <= 1024, "bad native manifest line size");
        std::istringstream row(line);
        std::string addressText, hex, extra;
        Monstey_Require(static_cast<bool>(row >> addressText >> hex) && !(row >> extra), "bad native manifest row");
        std::size_t used = 0;
        const auto address = std::stoull(addressText, &used, 16);
        Monstey_Require(used == addressText.size(), "bad native address");
        const auto expected = Monstey_Unhex(hex);
        const auto x = p.Monstey_At(address, expected.size()), y = q.Monstey_At(address, expected.size());
        Monstey_Require(std::equal(x.begin(), x.end(), expected.begin()) && std::equal(y.begin(), y.end(), expected.begin()),
                        "native byte mismatch");
        ++rows;
    }
    Monstey_Require(input.eof() && rows > 0, "empty or failed native manifest");
    return rows;
}
void Monstey_PrintMap(std::span<const Monstey_Channel> shared, const Monstey_MapResult& map) {
    std::cout << "\"shared_channels\":" << shared.size() << ",\"matched_destination_descriptors\":" << map.matchedDescriptors << ",\"map\":[";
    for (std::size_t i = 0; i < shared.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << "{\"channel_index\":" << i << ",\"bone_hash\":" << shared[i].bone << ",\"type_hash\":" << shared[i].type
                  << ",\"byte_offset\":" << map.offsets[i] << ",\"mapped\":" << (map.offsets[i] != Monstey_Unmapped ? "true" : "false") << '}';
    }
    std::cout << ']';
}

Monstey_Bytes Monstey_SyntheticSkel() {
    constexpr std::array<std::string_view, 7> names{"root", "A", "A1", "A2", "B", "B1", "B11"};
    constexpr std::array<std::uint32_t, 7> children{2, 2, 0, 0, 1, 1, 0};
    Monstey_Bytes bytes(100);
    Monstey_Put32(bytes, 0, 0x4c454b53); Monstey_Put32(bytes, 4, 17); Monstey_Put32(bytes, 40, 7);
    Monstey_Put32(bytes, 96, 0xdcdcff00);
    std::array<std::uint32_t, 7> offsets{};
    for (std::size_t i = 0; i < names.size(); ++i) {
        offsets[i] = static_cast<std::uint32_t>(bytes.size() - 100);
        bytes.insert(bytes.end(), names[i].begin(), names[i].end()); bytes.push_back(0);
    }
    Monstey_Put32(bytes, 12, static_cast<std::uint32_t>(bytes.size() - 100));
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto at = bytes.size(); bytes.resize(at + 37);
        Monstey_Put32(bytes, at, offsets[i]);
        bytes[at + 4] = i == 4 ? 1 : 0;
        Monstey_Put32(bytes, at + 29, 0x3f800000); Monstey_Put32(bytes, at + 33, children[i]);
    }
    return bytes;
}
Monstey_Bytes Monstey_SyntheticAnix() {
    Monstey_Bytes bytes(70);
    Monstey_Put32(bytes, 0, 0x58494e41); Monstey_Put32(bytes, 4, 44); Monstey_Put32(bytes, 8, 48);
    Monstey_Put32(bytes, 12, 70); Monstey_Put32(bytes, 16, 2); Monstey_Put32(bytes, 20, 48);
    Monstey_Put32(bytes, 24, 64); Monstey_Put32(bytes, 28, 1); Monstey_Put32(bytes, 32, 67); Monstey_Put32(bytes, 36, 3);
    Monstey_Put32(bytes, 48, Monstey_Pos); Monstey_Put32(bytes, 52, 2);
    Monstey_Put32(bytes, 56, 1); Monstey_Put32(bytes, 60, 9);
    bytes[64] = 0xa7; bytes[65] = 0x91; bytes[66] = 0x92;
    return bytes;
}
Monstey_Bytes Monstey_SyntheticPE() {
    Monstey_Bytes bytes(264);
    bytes[0] = 'M'; bytes[1] = 'Z'; Monstey_Put32(bytes, 0x3c, 64); Monstey_Put32(bytes, 64, 0x4550);
    bytes[68] = 0x64; bytes[69] = 0x86; bytes[70] = 1; bytes[84] = 32;
    bytes[88] = 0x0b; bytes[89] = 0x02; Monstey_Put32(bytes, 112, 0x40000000); Monstey_Put32(bytes, 116, 1);
    Monstey_Put32(bytes, 132, 0x1000); Monstey_Put32(bytes, 136, 8); Monstey_Put32(bytes, 140, 256);
    bytes[256] = 0xa7; return bytes;
}
struct Monstey_TestSuite {
    std::size_t total = 0, failures = 0;
    void Monstey_Test(std::string_view name, const std::function<void()>& body) {
        ++total;
        try { body(); std::cout << "{\"name\":\"" << name << "\",\"passed\":true}\n"; }
        catch (const std::exception& error) { ++failures; std::cerr << name << ": " << error.what() << '\n'; std::cout << "{\"name\":\"" << name << "\",\"passed\":false}\n"; }
    }
};
void Monstey_Rejects(const std::function<void()>& body) {
    bool rejected = false;
    try { body(); } catch (const std::exception&) { rejected = true; }
    Monstey_Require(rejected, "malformed input unexpectedly accepted");
}
int Monstey_RunTests() {
    Monstey_TestSuite suite;
    suite.Monstey_Test("native_hash_literals", [] { Monstey_Require(Monstey_Hash("Pos") == Monstey_Pos && Monstey_Hash("Rotation") == Monstey_Rotation && Monstey_Hash("Null") == 0x88baa7e7u, "hash mismatch"); });
    suite.Monstey_Test("native_case_table_identity", [] { Monstey_Require(Monstey_Hash("BONE") == Monstey_Hash("bone"), "native case table differs"); });
    suite.Monstey_Test("tuple_both_words_required", [] { const std::array shared{Monstey_Channel{4, 1}, Monstey_Channel{4, 2}}; const std::array dest{Monstey_Destination{{5, 1}, 16}, Monstey_Destination{{4, 2}, 32}}; Monstey_Require(Monstey_BuildMap(shared, dest).offsets == std::vector<std::uint16_t>{65535, 32}, "tuple matching"); });
    suite.Monstey_Test("unsigned_type_then_bone_order", [] { const std::array shared{Monstey_Channel{0xffffffffu, 1}, Monstey_Channel{1, 0xffffffffu}}; const std::array dest{Monstey_Destination{{1, 0xffffffffu}, 48}}; Monstey_Require(Monstey_BuildMap(shared, dest).offsets[1] == 48, "unsigned ordering"); });
    suite.Monstey_Test("unsorted_shared_rejected", [] { const std::array shared{Monstey_Channel{1, 2}, Monstey_Channel{1, 1}}; Monstey_Rejects([&] { (void)Monstey_BuildMap(shared, {}); }); });
    suite.Monstey_Test("duplicate_shared_first_lower_bound", [] { const std::array shared{Monstey_Channel{1, 2}, Monstey_Channel{1, 2}}; const std::array dest{Monstey_Destination{{1, 2}, 12}}; Monstey_Require(Monstey_BuildMap(shared, dest).offsets == std::vector<std::uint16_t>{12, 65535}, "first equal shared entry"); });
    suite.Monstey_Test("duplicate_destination_last_write", [] { const std::array shared{Monstey_Channel{1, 2}}; const std::array dest{Monstey_Destination{{1, 2}, 12}, Monstey_Destination{{1, 2}, 28}}; const auto map = Monstey_BuildMap(shared, dest); Monstey_Require(map.offsets[0] == 28 && map.matchedDescriptors == 2, "last destination and match count"); });
    suite.Monstey_Test("no_matches_and_empty_inputs", [] { const std::array shared{Monstey_Channel{1, 2}}; const auto map = Monstey_BuildMap(shared, {}); Monstey_Require(map.offsets[0] == 65535 && map.matchedDescriptors == 0 && Monstey_BuildMap({}, {}).offsets.empty(), "unmatched policy"); });
    suite.Monstey_Test("u16_destination_sentinel", [] { const std::array shared{Monstey_Channel{1, 2}}; const std::array dest{Monstey_Destination{{1, 2}, 65535}}; const auto map = Monstey_BuildMap(shared, dest); const std::array<std::uint16_t, 1> tracks{0}; Monstey_Require(map.matchedDescriptors == 1 && !Monstey_AnyReal(Monstey_Route(tracks, map)), "sentinel route"); });
    suite.Monstey_Test("out_of_range_and_ffff_tracks_scratch", [] { const Monstey_MapResult map{{16, 65535}, 1}; const std::array<std::uint16_t, 4> tracks{0, 1, 2, 65535}; const auto routes = Monstey_Route(tracks, map); Monstey_Require(routes[0] == 16 && routes[1] == routes[2] && routes[2] == routes[3] && Monstey_AnyReal(routes), "scratch routing"); });
    suite.Monstey_Test("codec_without_real_destinations_skips", [] { const Monstey_MapResult map{{65535}, 0}; const std::array<std::uint16_t, 2> tracks{0, 65535}; Monstey_Require(!Monstey_AnyReal(Monstey_Route(tracks, map)), "codec gating"); });
    suite.Monstey_Test("repeated_tracks_preserve_order_and_alias", [] { const Monstey_MapResult map{{16, 48}, 2}; const std::array<std::uint16_t, 3> tracks{1, 0, 1}; Monstey_Require(Monstey_Route(tracks, map) == std::vector<std::size_t>{48, 16, 48}, "track alias and order"); });
    suite.Monstey_Test("descriptor_bit0_selects_exact_buffer", [] { const Monstey_Bytes a{1, 2}, b{8, 9}; Monstey_Require(Monstey_DefaultBuffer(2, a, b) == a && Monstey_DefaultBuffer(3, a, b) == b, "default flag selector"); });
    suite.Monstey_Test("raw_descriptor_offset_is_byte_offset", [] { Monstey_Bytes bytes(16); Monstey_Put32(bytes, 0, 9); Monstey_Put32(bytes, 4, 7); bytes[12] = 3; const auto d = Monstey_ParseDestinations(bytes); Monstey_Require(d[0].identity == Monstey_Channel{9, 7} && d[0].byteOffset == 3, "descriptor word layout"); });
    suite.Monstey_Test("raw_pair_and_descriptor_widths", [] { const Monstey_Bytes pair(7), destination(15); Monstey_Rejects([&] { (void)Monstey_ParseSharedPairs(pair); }); Monstey_Rejects([&] { (void)Monstey_ParseDestinations(destination); }); });
    suite.Monstey_Test("skel_nontrivial_native_sibling_reservation", [] { const auto rig = Monstey_ParseSkel(Monstey_SyntheticSkel()); Monstey_Require(rig.runtimeToSource == std::vector<std::size_t>{0, 1, 4, 2, 3, 5, 6}, "preorder and runtime order conflated"); });
    suite.Monstey_Test("raw1_disables_position_keeps_rotation_and_slot", [] { const auto rig = Monstey_ParseSkel(Monstey_SyntheticSkel()); const auto d = Monstey_PoseDestinations(rig); Monstey_Require(d[4].identity.type == 65535 && d[4].byteOffset == 64 && d[5].identity.type == Monstey_Rotation && d[5].byteOffset == 80 && d[6].byteOffset == 96, "flag polarity"); });
    suite.Monstey_Test("raw_even_upper_bits_do_not_disable_position", [] { auto bytes = Monstey_SyntheticSkel(); const auto rig = Monstey_ParseSkel(bytes); bytes[rig.boneStart + 4] = 2; const auto changed = Monstey_ParseSkel(bytes); Monstey_Require(Monstey_PoseDestinations(changed)[0].identity.type == Monstey_Pos, "bit mask not full byte boolean"); });
    suite.Monstey_Test("skel_truncated_bone", [] { auto bytes = Monstey_SyntheticSkel(); bytes.pop_back(); Monstey_Rejects([&] { (void)Monstey_ParseSkel(bytes); }); });
    suite.Monstey_Test("skel_name_offset_and_nul_bounds", [] { auto bytes = Monstey_SyntheticSkel(); const auto rig = Monstey_ParseSkel(bytes); Monstey_Put32(bytes, rig.boneStart, Monstey_U32(bytes, 12)); Monstey_Rejects([&] { (void)Monstey_ParseSkel(bytes); }); });
    suite.Monstey_Test("skel_incomplete_tree", [] { auto bytes = Monstey_SyntheticSkel(); const auto rig = Monstey_ParseSkel(bytes); Monstey_Put32(bytes, rig.boneStart + 33, 6); Monstey_Rejects([&] { (void)Monstey_ParseSkel(bytes); }); });
    suite.Monstey_Test("skel_diagnostic_single_root_precondition", [] { auto bytes = Monstey_SyntheticSkel(); const auto rig = Monstey_ParseSkel(bytes); Monstey_Put32(bytes, rig.boneStart + 33, 0); Monstey_Rejects([&] { (void)Monstey_ParseSkel(bytes); }); });
    suite.Monstey_Test("skel_prefix_count_bounds", [] { auto bytes = Monstey_SyntheticSkel(); Monstey_Put32(bytes, 32, 1); Monstey_Rejects([&] { (void)Monstey_ParseSkel(bytes); }); });
    suite.Monstey_Test("skel_tail_explicitly_uninterpreted", [] { auto bytes = Monstey_SyntheticSkel(); bytes.push_back(0xa7); Monstey_Require(Monstey_ParseSkel(bytes).opaqueTail == 1, "opaque tail extent"); });
    suite.Monstey_Test("anix_group_expansion_and_independent_extents", [] { const auto shared = Monstey_ParseAnixShared(Monstey_SyntheticAnix()); Monstey_Require(shared == std::vector<Monstey_Channel>{{1, Monstey_Pos}, {9, Monstey_Pos}}, "ANIX grouped runtime pair order"); });
    suite.Monstey_Test("anix_zero_group_rejected", [] { auto bytes = Monstey_SyntheticAnix(); Monstey_Put32(bytes, 52, 0); Monstey_Rejects([&] { (void)Monstey_ParseAnixShared(bytes); }); });
    suite.Monstey_Test("anix_group_overflow_rejected", [] { auto bytes = Monstey_SyntheticAnix(); Monstey_Put32(bytes, 52, 3); Monstey_Rejects([&] { (void)Monstey_ParseAnixShared(bytes); }); });
    suite.Monstey_Test("anix_overlap_rejected", [] { auto bytes = Monstey_SyntheticAnix(); Monstey_Put32(bytes, 32, 64); Monstey_Put32(bytes, 36, 6); Monstey_Rejects([&] { (void)Monstey_ParseAnixShared(bytes); }); });
    suite.Monstey_Test("anix_u32_extent_overflow_rejected", [] { auto bytes = Monstey_SyntheticAnix(); Monstey_Put32(bytes, 28, 0xffffffffu); Monstey_Rejects([&] { (void)Monstey_ParseAnixShared(bytes); }); });
    suite.Monstey_Test("anix_declared_size_mismatch", [] { auto bytes = Monstey_SyntheticAnix(); bytes.push_back(0); Monstey_Rejects([&] { (void)Monstey_ParseAnixShared(bytes); }); });
    suite.Monstey_Test("pe_bounded_raw_va_and_content", [] { const auto bytes = Monstey_SyntheticPE(); const Monstey_PE pe(bytes); Monstey_Require(pe.imageBase == 0x140000000ull && pe.Monstey_At(0x140001000ull, 1)[0] == 0xa7, "PE RVA mapping"); });
    suite.Monstey_Test("pe_unmapped_and_cross_raw_extent", [] { const auto bytes = Monstey_SyntheticPE(); const Monstey_PE pe(bytes); Monstey_Rejects([&] { (void)pe.Monstey_At(0x140001007ull, 2); }); Monstey_Rejects([&] { (void)pe.Monstey_At(0x13fffffffull, 1); }); });
    suite.Monstey_Test("pe_bad_header_and_raw_section_bounds", [] { auto bytes = Monstey_SyntheticPE(); Monstey_Put32(bytes, 140, 260); Monstey_Rejects([&] { (void)Monstey_PE(bytes); }); Monstey_Put32(bytes, 0x3c, 0xffffffffu); Monstey_Rejects([&] { (void)Monstey_PE(bytes); }); });
    suite.Monstey_Test("hex_malformed_is_rejected", [] { Monstey_Rejects([] { (void)Monstey_Unhex("abc"); }); Monstey_Rejects([] { (void)Monstey_Unhex("ag"); }); });
    std::cout << "{\"schema\":\"Monstey.AnixModelBindingSynthetic/1\",\"groups\":" << suite.total << ",\"failures\":" << suite.failures << ",\"original_executable_executed\":false}\n";
    return suite.failures ? 1 : 0;
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--test")) return Monstey_RunTests();
        if (argc == 5 && std::string_view(argv[1]) == "--verify-native") {
            const auto count = Monstey_VerifyNative(argv[2], argv[3], argv[4]);
            std::cout << "{\"schema\":\"Monstey.AnixModelBindingByteVerification/1\",\"manifest_rows\":" << count << ",\"images\":2,\"differences\":0,\"original_executable_executed\":false}\n";
            return 0;
        }
        if (argc == 4 && std::string_view(argv[1]) == "--map") {
            const auto sharedBytes = Monstey_Read(argv[2]), destinationBytes = Monstey_Read(argv[3]);
            const auto shared = Monstey_ParseSharedPairs(sharedBytes);
            const auto destinations = Monstey_ParseDestinations(destinationBytes);
            const auto map = Monstey_BuildMap(shared, destinations);
            std::cout << "{\"schema\":\"Monstey.AnixGenericDestinationMap/1\",";
            Monstey_PrintMap(shared, map); std::cout << ",\"original_executable_executed\":false}\n"; return 0;
        }
        if (argc == 4 && std::string_view(argv[1]) == "--bind") {
            const auto anixBytes = Monstey_Read(argv[2]), skelBytes = Monstey_Read(argv[3]);
            const auto shared = Monstey_ParseAnixShared(anixBytes);
            const auto rig = Monstey_ParseSkel(skelBytes);
            const auto destinations = Monstey_PoseDestinations(rig);
            const auto map = Monstey_BuildMap(shared, destinations);
            std::cout << "{\"schema\":\"Monstey.AnixSkelPoseProjection/1\",\"diagnostic_only\":true,\"anix_codec_and_footer_semantics_checked\":false,\"skel_tail_bytes_not_interpreted\":" << rig.opaqueTail << ",\"runtime_bones\":[";
            for (std::size_t runtime = 0; runtime < rig.runtimeToSource.size(); ++runtime) {
                if (runtime) std::cout << ',';
                const auto source = rig.runtimeToSource[runtime]; const auto& bone = rig.source[source];
                const auto name = Monstey_View(reinterpret_cast<const std::uint8_t*>(bone.name.data()), bone.name.size());
                std::cout << "{\"runtime_index\":" << runtime << ",\"source_index\":" << source << ",\"name_bytes_hex\":\"" << Monstey_Hex(name)
                          << "\",\"bone_hash\":" << bone.hash << ",\"raw_flag_byte\":" << static_cast<unsigned>(bone.rawFlag)
                          << ",\"position_channel_enabled\":" << ((bone.rawFlag & 1u) == 0 ? "true" : "false")
                          << ",\"position_byte_offset\":" << runtime * 32 << ",\"rotation_byte_offset\":" << runtime * 32 + 16 << '}';
            }
            std::cout << "],"; Monstey_PrintMap(shared, map);
            std::cout << ",\"auxiliary_event_destinations_not_in_pose_projection\":true,\"native_model_or_animation_written\":false,\"original_executable_executed\":false}\n"; return 0;
        }
        std::cerr << "Usage: Monstey_AnixModelBindingProbe [--test | --bind file.anix file.skel | --map shared-pairs.bin destination-descriptors.bin | --verify-native manifest.txt pinned.exe installed.exe]\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Monstey_AnixModelBindingProbe: " << error.what() << '\n'; return 1;
    }
}
