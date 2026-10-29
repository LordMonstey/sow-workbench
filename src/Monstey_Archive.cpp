#include "Monstey_Archive.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <list>
#include <mutex>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Monstey {
namespace {
void Monstey_ArchiveRequire(bool value, const std::string &text) {
    if (!value)
        throw std::runtime_error("LTAR: " + text);
}
std::string Monstey_Lower(std::string s) {
    for (auto &c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + ('a' - 'A'));
    return s;
}
std::string Monstey_PathText(const std::filesystem::path &p) {
    auto s = p.generic_u8string();
    return {reinterpret_cast<const char *>(s.data()), s.size()};
}
std::filesystem::path Monstey_TextPath(const std::string &text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size()));
}
Monstey_Bytes Monstey_Exact(std::istream &stream, std::size_t n) {
    Monstey_Bytes result(n);
    if (n)
        stream.read(reinterpret_cast<char *>(result.data()), static_cast<std::streamsize>(n));
    Monstey_ArchiveRequire(static_cast<std::size_t>(stream.gcount()) == n || n == 0, "truncated input");
    return result;
}
std::uint32_t Monstey_AU32(std::span<const std::uint8_t> b, std::size_t p, bool be) {
    Monstey_ArchiveRequire(p <= b.size() && b.size() - p >= 4, "truncated integer");
    std::uint32_t v{};
    if (be)
        for (std::size_t j = 0; j < 4; ++j)
            v = (v << 8) | b[p + j];
    else
        for (std::size_t j = 0; j < 4; ++j)
            v |= static_cast<std::uint32_t>(b[p + j]) << (8 * j);
    return v;
}
std::uint64_t Monstey_AU64(std::span<const std::uint8_t> b, std::size_t p, bool be) {
    Monstey_ArchiveRequire(p <= b.size() && b.size() - p >= 8, "truncated integer");
    std::uint64_t v{};
    if (be)
        for (std::size_t j = 0; j < 8; ++j)
            v = (v << 8) | b[p + j];
    else
        for (std::size_t j = 0; j < 8; ++j)
            v |= static_cast<std::uint64_t>(b[p + j]) << (8 * j);
    return v;
}
void Monstey_A32(Monstey_Bytes &b, std::uint32_t n) {
    for (unsigned j = 0; j < 4; ++j)
        b.push_back(static_cast<std::uint8_t>(n >> (8 * j)));
}
void Monstey_A64(Monstey_Bytes &b, std::uint64_t n) {
    for (unsigned j = 0; j < 8; ++j)
        b.push_back(static_cast<std::uint8_t>(n >> (8 * j)));
}
void Monstey_AString(Monstey_Bytes &b, const std::string &s) {
    Monstey_ArchiveRequire(s.size() <= 16 * 1024 * 1024, "project string exceeds limit");
    Monstey_A32(b, static_cast<std::uint32_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}
struct Monstey_Bits {
    std::span<const std::uint8_t> data;
    std::size_t position{};
    unsigned Get(unsigned count) {
        Monstey_ArchiveRequire(count <= 16 && position <= data.size() * 8 &&
                                   count <= data.size() * 8 - position,
                               "truncated DEFLATE bits");
        unsigned v{};
        for (unsigned j = 0; j < count; ++j, ++position)
            v |= ((data[position / 8] >> (position % 8)) & 1u) << j;
        return v;
    }
    void Align() { position = (position + 7) & ~std::size_t(7); }
};
struct Monstey_Huffman {
    std::map<unsigned, unsigned> symbols;
    unsigned max_bits{};
    explicit Monstey_Huffman(const std::vector<unsigned> &lengths, bool allow_empty = false,
                             bool code_lengths = false) {
        std::array<unsigned, 16> counts{}, next{};
        for (auto n : lengths) {
            Monstey_ArchiveRequire(n <= 15, "DEFLATE code too long");
            if (n) {
                ++counts[n];
                max_bits = std::max(max_bits, n);
            }
        }
        Monstey_ArchiveRequire(max_bits || allow_empty, "empty DEFLATE tree");
        int left = 1;
        for (unsigned n = 1; n <= 15; ++n) {
            left = 2 * left - static_cast<int>(counts[n]);
            Monstey_ArchiveRequire(left >= 0, "oversubscribed DEFLATE tree");
        }
        Monstey_ArchiveRequire(left == 0 || (!code_lengths && max_bits == 1) ||
                                   (allow_empty && max_bits == 0),
                               "incomplete DEFLATE tree");
        unsigned code{};
        for (unsigned n = 1; n <= 15; ++n) {
            code = (code + counts[n - 1]) << 1;
            next[n] = code;
        }
        for (unsigned symbol = 0; symbol < lengths.size(); ++symbol) {
            auto n = lengths[symbol];
            if (!n)
                continue;
            unsigned c = next[n]++, reverse{};
            for (unsigned k = 0; k < n; ++k)
                reverse = (reverse << 1) | ((c >> k) & 1u);
            symbols.emplace((1u << n) | reverse, symbol);
        }
    }
    unsigned Decode(Monstey_Bits &bits) const {
        unsigned code{};
        for (unsigned n = 1; n <= max_bits; ++n) {
            code |= bits.Get(1) << (n - 1);
            auto it = symbols.find((1u << n) | code);
            if (it != symbols.end())
                return it->second;
        }
        throw std::runtime_error("LTAR: invalid DEFLATE symbol");
    }
};
Monstey_Bytes Monstey_Inflate(std::span<const std::uint8_t> input, std::size_t expected, bool wrapped = true,
                              std::size_t limit = 256ull * 1024 * 1024) {
    Monstey_ArchiveRequire(expected <= limit && input.size() <= limit,
                           "DEFLATE exceeds decoder extent limit");
    if (wrapped)
        Monstey_ArchiveRequire(input.size() >= 6 && (input[0] & 15) == 8 && (input[0] >> 4) <= 7 &&
                                   ((input[0] * 256u + input[1]) % 31) == 0 && !(input[1] & 32),
                               "invalid zlib header");
    Monstey_Bits bits{wrapped ? input.subspan(2, input.size() - 6) : input};
    Monstey_Bytes out;
    out.reserve(expected);
    constexpr unsigned lb[] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                               31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    constexpr unsigned le[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                               2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    constexpr unsigned db[] = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                               33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                               1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    constexpr unsigned de[] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                               6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    bool final{};
    while (!final) {
        final = bits.Get(1) != 0;
        auto type = bits.Get(2);
        if (type == 0) {
            bits.Align();
            auto len = bits.Get(16), inverse = bits.Get(16);
            Monstey_ArchiveRequire((len ^ inverse) == 65535, "invalid DEFLATE stored length");
            Monstey_ArchiveRequire(len <= expected - out.size(), "DEFLATE output exceeds chunk");
            for (unsigned j = 0; j < len; ++j)
                out.push_back(static_cast<std::uint8_t>(bits.Get(8)));
            continue;
        }
        Monstey_ArchiveRequire(type != 3, "reserved DEFLATE block type");
        std::vector<unsigned> lit, dist;
        if (type == 1) {
            lit.resize(288);
            for (unsigned j = 0; j < 288; ++j)
                lit[j] = j < 144 ? 8 : (j < 256 ? 9 : (j < 280 ? 7 : 8));
            dist.assign(32, 5);
        } else {
            auto nl = bits.Get(5) + 257, nd = bits.Get(5) + 1, nc = bits.Get(4) + 4;
            Monstey_ArchiveRequire(nl <= 286, "invalid DEFLATE literal count");
            // RFC1951 code-length permutation (19 symbols).
            constexpr unsigned permutation[] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                11, 4,  12, 3, 13, 2, 14, 1, 15};
            std::vector<unsigned> lengths(19);
            for (unsigned j = 0; j < nc; ++j)
                lengths[permutation[j]] = bits.Get(3);
            Monstey_Huffman cl(lengths, false, true);
            std::vector<unsigned> all;
            while (all.size() < nl + nd) {
                auto s = cl.Decode(bits);
                if (s <= 15)
                    all.push_back(s);
                else {
                    unsigned repeat{}, value{};
                    if (s == 16) {
                        Monstey_ArchiveRequire(!all.empty(), "DEFLATE repeat without previous code");
                        repeat = bits.Get(2) + 3;
                        value = all.back();
                    } else if (s == 17)
                        repeat = bits.Get(3) + 3;
                    else if (s == 18)
                        repeat = bits.Get(7) + 11;
                    else
                        throw std::runtime_error("LTAR: invalid code-length symbol");
                    Monstey_ArchiveRequire(repeat <= nl + nd - all.size(),
                                           "DEFLATE code-length repeat overflow");
                    all.insert(all.end(), repeat, value);
                }
            }
            lit.assign(all.begin(), all.begin() + nl);
            dist.assign(all.begin() + nl, all.end());
        }
        Monstey_ArchiveRequire(lit.size() > 256 && lit[256], "DEFLATE end marker absent");
        Monstey_Huffman literals(lit), distances(dist, true);
        for (;;) {
            auto symbol = literals.Decode(bits);
            if (symbol < 256) {
                Monstey_ArchiveRequire(out.size() < expected, "DEFLATE output exceeds chunk");
                out.push_back(static_cast<std::uint8_t>(symbol));
            } else if (symbol == 256)
                break;
            else {
                Monstey_ArchiveRequire(symbol <= 285, "invalid DEFLATE length symbol");
                auto index = symbol - 257;
                auto length = lb[index] + bits.Get(le[index]);
                auto ds = distances.Decode(bits);
                Monstey_ArchiveRequire(ds < 30, "invalid DEFLATE distance symbol");
                auto distance = db[ds] + bits.Get(de[ds]);
                Monstey_ArchiveRequire(distance <= out.size() &&
                                           distance <= (wrapped ? (1u << (8 + (input[0] >> 4))) : 32768u) &&
                                           length <= expected - out.size(),
                                       "DEFLATE back-reference outside output");
                for (unsigned j = 0; j < length; ++j)
                    out.push_back(out[out.size() - distance]);
            }
        }
    }
    bits.Align();
    Monstey_ArchiveRequire(bits.position == bits.data.size() * 8 && out.size() == expected,
                           "zlib extent or decoded size mismatch");
    if (wrapped) {
        std::uint32_t a = 1, b = 0;
        for (auto c : out) {
            a = (a + c) % 65521;
            b = (b + a) % 65521;
        }
        Monstey_ArchiveRequire(((b << 16) | a) == Monstey_AU32(input, input.size() - 4, true),
                               "zlib Adler32 mismatch");
    }
    return out;
}
class Monstey_OodleBinding {
#ifdef _WIN32
    HMODULE module_{};
    using Decode = std::intptr_t(__cdecl *)(const void *, std::intptr_t, void *, std::intptr_t, int, int, int,
                                            void *, std::intptr_t, void *, void *, void *, std::intptr_t,
                                            int);
    Decode decode_{};
#endif
  public:
    explicit Monstey_OodleBinding(const std::filesystem::path &dll) {
#ifdef _WIN32
        static_assert(sizeof(void *) == 8, "LTAR v4 Oodle requires 64 bit");
        auto full = std::filesystem::absolute(dll);
        Monstey_ArchiveRequire(!dll.empty() && std::filesystem::is_regular_file(full),
                               "compressed LTAR4 requires the user's Oodle DLL");
        module_ = LoadLibraryExW(full.c_str(), nullptr,
                                 LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        Monstey_ArchiveRequire(module_ != nullptr, "could not load Oodle DLL");
        decode_ = reinterpret_cast<Decode>(GetProcAddress(module_, "OodleLZ_Decompress"));
        if (!decode_) {
            FreeLibrary(module_);
            module_ = nullptr;
            throw std::runtime_error("LTAR: Oodle export absent");
        }
#else
        (void)dll;
        throw std::runtime_error("LTAR: Oodle decoding requires Windows x64");
#endif
    }
    ~Monstey_OodleBinding() {
#ifdef _WIN32
        if (module_)
            FreeLibrary(module_);
#endif
    }
    Monstey_OodleBinding(const Monstey_OodleBinding &) = delete;
    Monstey_OodleBinding &operator=(const Monstey_OodleBinding &) = delete;
    Monstey_Bytes Run(const Monstey_Bytes &data, std::size_t size) {
#ifdef _WIN32
        Monstey_Bytes out(size);
        auto n =
            decode_(data.data(), static_cast<std::intptr_t>(data.size()), out.data(),
                    static_cast<std::intptr_t>(size), 1, 0, 0, nullptr, 0, nullptr, nullptr, nullptr, 0, 3);
        Monstey_ArchiveRequire(n == static_cast<std::intptr_t>(size), "Oodle decoded length mismatch");
        return out;
#else
        (void)data;
        (void)size;
        return {};
#endif
    }
};
class Monstey_NewOutput {
    std::filesystem::path path_;
#ifdef _WIN32
    HANDLE file_{INVALID_HANDLE_VALUE};
#else
    int file_{-1};
#endif
    bool keep_{};

  public:
     explicit Monstey_NewOutput(const std::filesystem::path &path) : path_(Monstey_WindowsFilePath(path)) {
        Monstey_RequireOfflineOutput(path_);
        if (!path_.parent_path().empty())
            std::filesystem::create_directories(path_.parent_path());
#ifdef _WIN32
        file_ = CreateFileW(path_.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
        Monstey_ArchiveRequire(file_ != INVALID_HANDLE_VALUE, "output already exists or cannot be created");
#else
        file_ = open(path_.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
        Monstey_ArchiveRequire(file_ >= 0, "output already exists or cannot be created");
#endif
    }
    ~Monstey_NewOutput() {
        Close();
        if (!keep_) {
            std::error_code ec;
            std::filesystem::remove(path_, ec);
        }
    }
    void Close() {
#ifdef _WIN32
        if (file_ != INVALID_HANDLE_VALUE) {
            CloseHandle(file_);
            file_ = INVALID_HANDLE_VALUE;
        }
#else
        if (file_ >= 0) {
            close(file_);
            file_ = -1;
        }
#endif
    }
    void Write(std::span<const std::uint8_t> data) {
        while (!data.empty()) {
            auto size = static_cast<unsigned>(std::min<std::size_t>(data.size(), 1024 * 1024));
#ifdef _WIN32
            DWORD written{};
            Monstey_ArchiveRequire(WriteFile(file_, data.data(), size, &written, nullptr) && written == size,
                                   "output write failed");
#else
            Monstey_ArchiveRequire(write(file_, data.data(), size) == size, "output write failed");
#endif
            data = data.subspan(size);
        }
    }
    void Seek(std::uint64_t pos) {
#ifdef _WIN32
        LARGE_INTEGER n{};
        n.QuadPart = static_cast<LONGLONG>(pos);
        Monstey_ArchiveRequire(SetFilePointerEx(file_, n, nullptr, FILE_BEGIN) != 0, "output seek failed");
#else
        Monstey_ArchiveRequire(lseek(file_, static_cast<off_t>(pos), SEEK_SET) >= 0, "output seek failed");
#endif
    }
    void Finish() {
#ifdef _WIN32
        Monstey_ArchiveRequire(FlushFileBuffers(file_) != 0, "output flush failed");
#else
        Monstey_ArchiveRequire(fsync(file_) == 0, "output flush failed");
#endif
        Close();
    }
    void Keep() { keep_ = true; }
};
} // namespace
std::uint32_t Monstey_Crc32(std::span<const std::uint8_t> input) {
    static const auto table = []() {
        std::array<std::uint32_t, 256> values{};
        for (unsigned j = 0; j < 256; ++j) {
            auto crc = j;
            for (unsigned bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
            values[j] = crc;
        }
        return values;
    }();
    std::uint32_t crc = 0xffffffffu;
    for (auto byte : input)
        crc = (crc >> 8) ^ table[(crc ^ byte) & 255u];
    return ~crc;
}
Monstey_Bytes Monstey_InflateGzip(std::span<const std::uint8_t> input, std::size_t maximum) {
    Monstey_Require(maximum <= 512ull * 1024 * 1024 && input.size() >= 18 &&
                        input.size() <= 512ull * 1024 * 1024,
                    "gzip exceeds bounds or has a truncated header");
    Monstey_Require(input[0] == 0x1f && input[1] == 0x8b && input[2] == 8 && (input[3] & 0xe0u) == 0,
                    "Expected gzip DEFLATE with valid header flags");
    auto flags = input[3];
    std::size_t at = 10, end = input.size() - 8;
    auto checked = [&](std::size_t n) {
        Monstey_Require(at <= end && n <= end - at && at + n <= 1024 * 1024,
                        "Truncated or excessive gzip optional header");
    };
    if (flags & 4u) {
        checked(2);
        auto size = std::uint32_t(input[at]) | (std::uint32_t(input[at + 1]) << 8);
        at += 2;
        checked(size);
        at += size;
    }
    for (auto flag : {8u, 16u})
        if (flags & flag) {
            for (;;) {
                checked(1);
                if (input[at++] == 0)
                    break;
            }
        }
    if (flags & 2u) {
        checked(2);
        auto expected = std::uint32_t(input[at]) | (std::uint32_t(input[at + 1]) << 8);
        Monstey_Require((Monstey_Crc32(input.first(at)) & 65535u) == expected, "gzip header CRC16 mismatch");
        at += 2;
    }
    Monstey_Require(at <= end, "gzip payload header outside member");
    auto crc = Monstey_AU32(input, input.size() - 8, false),
         size = Monstey_AU32(input, input.size() - 4, false);
    Monstey_Require(size <= maximum, "gzip decoded size exceeds caller limit");
    Monstey_Bytes decoded;
    try {
        decoded = Monstey_Inflate(input.subspan(at, end - at), size, false, 512ull * 1024 * 1024);
    } catch (const std::exception &e) {
        throw Monstey_Error("gzip_invalid",
                            std::string("Invalid gzip stream; concatenated members are unsupported: ") +
                                e.what());
    }
    Monstey_Require(Monstey_Crc32(decoded) == crc, "gzip payload CRC32 mismatch");
    return decoded;
}
Monstey_Bytes Monstey_InflateRaw(std::span<const std::uint8_t> input, std::size_t decoded_size) {
    return Monstey_Inflate(input, decoded_size, false);
}
std::string Monstey_ArchiveVirtual(const std::string &path, bool allow_empty) {
    std::string s = path;
    std::replace(s.begin(), s.end(), '\\', '/');
    if (allow_empty && s.empty())
        return s;
    Monstey_ArchiveRequire(!s.empty() && s.find(':') == std::string::npos &&
                               s.find('\0') == std::string::npos,
                           "unsafe virtual path");
    std::size_t p{};
    while (p < s.size()) {
        auto end = s.find('/', p);
        auto part = s.substr(p, end - p);
        Monstey_ArchiveRequire(!part.empty() && part != "." && part != "..", "unsafe virtual path component");
        if (end == std::string::npos)
            break;
        p = end + 1;
        Monstey_ArchiveRequire(p < s.size(), "trailing directory separator");
    }
    return Monstey_Lower(s);
}
Monstey_Archive::Monstey_Archive(const std::filesystem::path &path) : path_(std::filesystem::absolute(path)) {
    length_ = std::filesystem::file_size(Monstey_WindowsFilePath(path_));
    time_ = std::filesystem::last_write_time(Monstey_WindowsFilePath(path_));
    std::ifstream in(Monstey_WindowsFilePath(path_), std::ios::binary);
    Monstey_ArchiveRequire(in.good(), "cannot open input");
    auto h = Monstey_Exact(in, 48);
    Monstey_ArchiveRequire(std::memcmp(h.data(), "LTAR", 4) == 0 || std::memcmp(h.data(), "RATL", 4) == 0,
                           "invalid magic");
    big_endian_ = h[0] == 'R';
    version_ = Monstey_AU32(h, 4, big_endian_);
    auto ns = Monstey_AU32(h, 8, big_endian_), nf = Monstey_AU32(h, 12, big_endian_),
         ne = Monstey_AU32(h, 16, big_endian_);
    Monstey_ArchiveRequire((version_ == 3 || version_ == 4) && nf > 0 &&
                               std::all_of(h.begin() + 24, h.end(), [](auto c) { return c == 0; }),
                           "unsupported version, counts or reserved bytes");
    auto rs = version_ == 4 ? 30ull : 32ull;
    auto extent = 48ull + ns + rs * ne + 16ull * nf;
    index_length_ = extent;
    Monstey_ArchiveRequire(extent <= length_ && extent <= 256ull * 1024 * 1024,
                           "index exceeds extent or 256 MiB limit");
    auto data = Monstey_Exact(in, static_cast<std::size_t>(extent - 48));
    auto names = std::span(data).first(ns);
    auto name = [&](std::uint32_t offset) {
        Monstey_ArchiveRequire(offset < names.size(), "name offset out of range");
        auto begin = names.begin() + offset, end = std::find(begin, names.end(), 0);
        Monstey_ArchiveRequire(end != names.end(), "unterminated name");
        std::string s(begin, end);
        (void)Monstey_Wide(s);
        return s;
    };
    struct File {
        std::string name;
        std::uint64_t off, stored, size;
        std::uint32_t flags;
    };
    std::vector<File> files;
    files.reserve(ne);
    for (std::uint32_t j = 0; j < ne; ++j) {
        auto p = static_cast<std::size_t>(ns + rs * j);
        auto off = Monstey_AU64(data, p + 4, big_endian_), stored = Monstey_AU64(data, p + 12, big_endian_),
             size = Monstey_AU64(data, p + 20, big_endian_);
        Monstey_ArchiveRequire(off >= extent && off <= length_ && stored <= length_ - off &&
                                   size <=
                                       static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()),
                               "payload range invalid");
        auto n = Monstey_ArchiveVirtual(name(Monstey_AU32(data, p, big_endian_)));
        Monstey_ArchiveRequire(n.find('/') == std::string::npos, "filename contains separator");
        if (version_ == 4)
            Monstey_ArchiveRequire(data[p + 28] == 1, "unsupported LTAR4 file marker");
        files.push_back(
            {n, off, stored, size, version_ == 4 ? data[p + 29] : Monstey_AU32(data, p + 28, big_endian_)});
    }
    struct Folder {
        std::string path;
        std::int32_t child, sibling;
        std::uint32_t count;
    };
    std::vector<Folder> folders;
    folders.reserve(nf);
    std::size_t cursor{};
    for (std::uint32_t j = 0; j < nf; ++j) {
        auto p = static_cast<std::size_t>(ns + rs * ne + 16ull * j);
        auto folder = Monstey_ArchiveVirtual(name(Monstey_AU32(data, p, big_endian_)), true);
        auto child = static_cast<std::int32_t>(Monstey_AU32(data, p + 4, big_endian_)),
             sibling = static_cast<std::int32_t>(Monstey_AU32(data, p + 8, big_endian_));
        auto count = Monstey_AU32(data, p + 12, big_endian_);
        Monstey_ArchiveRequire(
            (child == -1 || (child >= 0 && static_cast<std::uint32_t>(child) < nf)) &&
                (sibling == -1 || (sibling >= 0 && static_cast<std::uint32_t>(sibling) < nf)) &&
                count <= files.size() - cursor,
            "folder link or file count invalid");
        folders.push_back({folder, child, sibling, count});
        folders_.push_back(folder);
        for (std::size_t k = 0; k < count; ++k) {
            auto &f = files[cursor++];
            auto virtual_name = folder.empty() ? f.name : folder + "/" + f.name;
            Monstey_ArchiveRequire(by_path_.emplace(virtual_name, entries_.size()).second,
                                   "duplicate case insensitive path");
            entries_.push_back({virtual_name, f.off, f.stored, f.size, f.flags});
        }
    }
    Monstey_ArchiveRequire(folders[0].path.empty() && folders[0].sibling == -1 && cursor == files.size(),
                           "invalid root or unassigned files");
    std::vector<std::pair<std::int32_t, std::optional<std::string>>> todo{{0, std::nullopt}};
    std::set<std::int32_t> visited;
    while (!todo.empty()) {
        auto [i, parent] = todo.back();
        todo.pop_back();
        Monstey_ArchiveRequire(visited.insert(i).second, "folder tree cycle");
        auto &f = folders[static_cast<std::size_t>(i)];
        if (parent) {
            auto slash = f.path.rfind('/');
            auto actual = slash == std::string::npos ? "" : f.path.substr(0, slash);
            Monstey_ArchiveRequire(!f.path.empty() && actual == *parent, "folder is not immediate child");
        }
        if (f.sibling != -1)
            todo.emplace_back(f.sibling, parent);
        if (f.child != -1)
            todo.emplace_back(f.child, f.path);
    }
    Monstey_ArchiveRequire(visited.size() == nf, "unreachable folders");
    CheckUnchanged();
}
void Monstey_Archive::CheckUnchanged() const {
    Monstey_ArchiveRequire(std::filesystem::file_size(Monstey_WindowsFilePath(path_)) == length_ &&
                               std::filesystem::last_write_time(Monstey_WindowsFilePath(path_)) == time_,
                           "source archive changed since indexing");
}
const Monstey_ArchiveEntry *Monstey_Archive::Find(const std::string &path) const {
    auto it = by_path_.find(Monstey_ArchiveVirtual(path));
    return it == by_path_.end() ? nullptr : &entries_[it->second];
}
void Monstey_Archive::Stream(const Monstey_ArchiveEntry &entry, const Sink &sink,
                             const std::filesystem::path &dll) const {
    auto own = Find(entry.path);
    Monstey_ArchiveRequire(own && own->offset == entry.offset && own->stored_size == entry.stored_size &&
                               own->size == entry.size && own->flags == entry.flags,
                           "foreign entry");
    CheckUnchanged();
    std::ifstream in(Monstey_WindowsFilePath(path_), std::ios::binary);
    in.seekg(static_cast<std::streamoff>(entry.offset));
    auto cursor = entry.offset, end = entry.offset + entry.stored_size;
    std::uint64_t decoded{};
    std::unique_ptr<Monstey_OodleBinding> oodle;
    while (decoded < entry.size) {
        Monstey_ArchiveRequire(cursor <= end && end - cursor >= 8, "chunk header outside payload");
        auto h = Monstey_Exact(in, 8);
        auto stored = Monstey_AU32(h, 0, big_endian_), size = Monstey_AU32(h, 4, big_endian_);
        Monstey_ArchiveRequire(stored > 0 && stored <= size && size <= 65536 &&
                                   size <= entry.size - decoded && stored <= end - cursor - 8,
                               "invalid chunk extent");
        auto payload = Monstey_Exact(in, stored);
        Monstey_Bytes raw;
        if (stored == size)
            raw = std::move(payload);
        else if (version_ == 3)
            raw = Monstey_Inflate(payload, size);
        else {
            if (!oodle)
                oodle = std::make_unique<Monstey_OodleBinding>(dll);
            raw = oodle->Run(payload, size);
        }
        cursor += 8 + stored;
        auto pad = static_cast<std::size_t>((4 - (cursor - entry.offset) % 4) % 4);
        Monstey_ArchiveRequire(pad <= end - cursor, "chunk padding outside payload");
        (void)Monstey_Exact(in, pad);
        cursor += pad;
        decoded += size;
        sink(raw);
    }
    Monstey_ArchiveRequire(cursor == end, "unconsumed stored payload");
    CheckUnchanged();
}
Monstey_Bytes Monstey_Archive::Read(const std::string &path, const std::filesystem::path &dll,
                                    std::uint64_t limit) const {
    auto e = Find(path);
    Monstey_ArchiveRequire(e != nullptr, "resource not found");
    Monstey_ArchiveRequire(e->size <= limit && e->size <= std::numeric_limits<std::size_t>::max(),
                           "selected resource exceeds read limit");
    Monstey_Bytes out;
    out.reserve(static_cast<std::size_t>(e->size));
    Stream(*e, [&](auto b) { out.insert(out.end(), b.begin(), b.end()); }, dll);
    return out;
}
void Monstey_Archive::Extract(const std::string &path, const std::filesystem::path &output,
                              const std::filesystem::path &dll) const {
    auto e = Find(path);
    Monstey_ArchiveRequire(e != nullptr, "resource not found");
    Monstey_NewOutput out(output);
    Stream(*e, [&](auto b) { out.Write(b); }, dll);
    out.Finish();
    out.Keep();
}

Monstey_Bytes Monstey_BuildArchivePatchList(const std::vector<std::string> &paths) {
    std::map<std::uint32_t, std::string> sorted;
    std::set<std::string> seen;
    Monstey_Bytes names;
    std::map<std::string, std::uint32_t> offsets;
    for (const auto &p : paths) {
        auto s = Monstey_ArchiveVirtual(p);
        for (auto c : s)
            Monstey_ArchiveRequire(static_cast<unsigned char>(c) < 128, "patch list requires ASCII paths");
        std::replace(s.begin(), s.end(), '/', '\\');
        Monstey_ArchiveRequire(seen.insert(s).second, "duplicate patch-list path");
        auto hash = Monstey_PathHash(s);
        auto [it, ok] = sorted.emplace(hash, s);
        Monstey_ArchiveRequire(ok || it->second == s, "native path hash collision");
        Monstey_ArchiveRequire(names.size() <= std::numeric_limits<std::uint32_t>::max() - s.size() - 1,
                               "patch strings too large");
        offsets[s] = static_cast<std::uint32_t>(names.size());
        names.insert(names.end(), s.begin(), s.end());
        names.push_back(0);
    }
    Monstey_Bytes out;
    Monstey_A32(out, 1);
    Monstey_A32(out, static_cast<std::uint32_t>(names.size()));
    out.insert(out.end(), names.begin(), names.end());
    Monstey_A32(out, static_cast<std::uint32_t>(sorted.size()));
    for (auto &[hash, s] : sorted) {
        Monstey_A32(out, hash);
        Monstey_A32(out, offsets.at(s));
    }
    return out;
}
std::vector<std::string> Monstey_ReadArchivePatchList(std::span<const std::uint8_t> b) {
    Monstey_ArchiveRequire(b.size() >= 12 && Monstey_AU32(b, 0, false) == 1, "invalid patch-list header");
    auto ns = Monstey_AU32(b, 4, false);
    Monstey_ArchiveRequire(ns <= b.size() - 12, "patch-list string extent invalid");
    auto n = Monstey_AU32(b, 8 + ns, false);
    Monstey_ArchiveRequire(12ull + ns + 8ull * n == b.size(), "patch-list row count invalid");
    std::vector<std::string> out;
    std::set<std::string> seen;
    std::uint32_t previous{};
    for (std::uint32_t j = 0; j < n; ++j) {
        auto p = 12ull + ns + 8ull * j;
        auto hash = Monstey_AU32(b, static_cast<std::size_t>(p), false),
             off = Monstey_AU32(b, static_cast<std::size_t>(p + 4), false);
        Monstey_ArchiveRequire(off < ns, "patch-list name offset invalid");
        auto first = b.begin() + 8 + off, end = std::find(first, b.begin() + 8 + ns, 0);
        Monstey_ArchiveRequire(end != b.begin() + 8 + ns, "patch-list name unterminated");
        auto s = Monstey_ArchiveVirtual(std::string(first, end));
        Monstey_ArchiveRequire((j == 0 || hash > previous) && Monstey_PathHash(s) == hash &&
                                   seen.insert(s).second,
                               "patch-list hash ordering, collision or value invalid");
        out.push_back(s);
        previous = hash;
    }
    return out;
}
struct Monstey_ArchiveRange::Monstey_Impl {
    struct Chunk {
        std::uint64_t decoded_offset, payload_offset;
        std::uint32_t stored, decoded;
    };
    std::shared_ptr<const Monstey_Archive> archive;
    Monstey_ArchiveEntry entry;
    std::filesystem::path dll;
    std::vector<Chunk> chunks;
    std::list<std::pair<std::size_t, Monstey_Bytes>> cache;
    std::unique_ptr<Monstey_OodleBinding> oodle;
    mutable std::mutex mutex;
    std::uint64_t loads{}, hits{}, window_reads{}, metadata_bytes{}, window_peak{};
    Monstey_Impl(std::shared_ptr<const Monstey_Archive> input, const Monstey_ArchiveEntry &selected,
                 const std::filesystem::path &decoder)
        : archive(std::move(input)), entry(selected), dll(decoder) {
        Monstey_ArchiveRequire(archive != nullptr, "Range archive owner is missing");
        auto own = archive->Find(entry.path);
        Monstey_ArchiveRequire(own && own->offset == entry.offset && own->stored_size == entry.stored_size &&
                                   own->size == entry.size && own->flags == entry.flags,
                               "Range entry does not belong to archive");
        archive->CheckUnchanged();
        std::ifstream in(Monstey_WindowsFilePath(archive->Path()), std::ios::binary);
        Monstey_ArchiveRequire(in.good(), "Cannot open archive for chunk metadata");
        auto cursor = entry.offset, end = entry.offset + entry.stored_size;
        Monstey_Bytes window;
        std::uint64_t window_start{}, window_end{};
        auto read_header = [&](std::uint64_t position) -> std::span<const std::uint8_t> {
            if (window.empty() || position < window_start || position > window_end ||
                window_end - position < 8) {
                auto n =
                    static_cast<std::size_t>(std::min<std::uint64_t>(4ull * 1024 * 1024, end - position));
                window.resize(n);
                in.clear();
                in.seekg(static_cast<std::streamoff>(position));
                if (n)
                    in.read(reinterpret_cast<char *>(window.data()), static_cast<std::streamsize>(n));
                Monstey_ArchiveRequire(in.good() && static_cast<std::size_t>(in.gcount()) == n,
                                       "Truncated archive metadata window");
                window_start = position;
                window_end = position + n;
                ++window_reads;
                metadata_bytes += n;
                window_peak = std::max<std::uint64_t>(window_peak, n);
            }
            return std::span<const std::uint8_t>(window).subspan(
                static_cast<std::size_t>(position - window_start), 8);
        };
        std::uint64_t decoded{};
        while (decoded < entry.size) {
            Monstey_ArchiveRequire(cursor <= end && end - cursor >= 8,
                                   "Range chunk header exceeds selected asset");
            auto h = read_header(cursor);
            auto stored = Monstey_AU32(h, 0, archive->BigEndian()),
                 size = Monstey_AU32(h, 4, archive->BigEndian());
            Monstey_ArchiveRequire(stored > 0 && stored <= size && size <= 65536 &&
                                       size <= entry.size - decoded && stored <= end - cursor - 8,
                                   "Invalid range chunk extent");
            Monstey_ArchiveRequire(chunks.size() < (64ull * 1024 * 1024) / sizeof(Chunk),
                                   "Range metadata exceeds chunk limit");
            chunks.push_back({decoded, cursor + 8, stored, size});
            cursor += 8 + stored;
            auto pad = (4 - (cursor - entry.offset) % 4) % 4;
            Monstey_ArchiveRequire(pad <= end - cursor, "Range chunk padding exceeds selected asset");
            cursor += pad;
            decoded += size;
        }
        Monstey_ArchiveRequire(decoded == entry.size && cursor == end,
                               "Range chunk totals differ from selected asset");
        archive->CheckUnchanged();
    }
    const Monstey_Bytes &Load(std::size_t index) {
        for (auto it = cache.begin(); it != cache.end(); ++it)
            if (it->first == index) {
                cache.splice(cache.begin(), cache, it);
                ++hits;
                return cache.front().second;
            }
        auto &chunk = chunks.at(index);
        std::ifstream in(Monstey_WindowsFilePath(archive->Path()), std::ios::binary);
        Monstey_ArchiveRequire(in.good(), "Cannot open archive chunk payload");
        in.seekg(static_cast<std::streamoff>(chunk.payload_offset));
        auto payload = Monstey_Exact(in, chunk.stored);
        Monstey_Bytes decoded;
        if (chunk.stored == chunk.decoded)
            decoded = std::move(payload);
        else if (archive->Version() == 3)
            decoded = Monstey_Inflate(payload, chunk.decoded);
        else {
            if (!oodle)
                oodle = std::make_unique<Monstey_OodleBinding>(dll);
            decoded = oodle->Run(payload, chunk.decoded);
        }
        Monstey_ArchiveRequire(decoded.size() == chunk.decoded, "Range decoder output length differs");
        cache.emplace_front(index, std::move(decoded));
        if (cache.size() > 4)
            cache.pop_back();
        ++loads;
        return cache.front().second;
    }
};
Monstey_ArchiveRange::Monstey_ArchiveRange(std::shared_ptr<const Monstey_Archive> archive,
                                           const Monstey_ArchiveEntry &entry,
                                           const std::filesystem::path &dll)
    : impl_(std::make_unique<Monstey_Impl>(std::move(archive), entry, dll)) {}
Monstey_ArchiveRange::~Monstey_ArchiveRange() = default;
Monstey_ArchiveRange::Monstey_ArchiveRange(Monstey_ArchiveRange &&) noexcept = default;
Monstey_ArchiveRange &Monstey_ArchiveRange::operator=(Monstey_ArchiveRange &&) noexcept = default;
std::uint64_t Monstey_ArchiveRange::Size() const noexcept { return impl_->entry.size; }
Monstey_ArchiveRangeStats Monstey_ArchiveRange::Stats() const {
    std::lock_guard lock(impl_->mutex);
    return {impl_->chunks.size(), impl_->loads,          impl_->hits,       impl_->cache.size(),
            impl_->window_reads,  impl_->metadata_bytes, impl_->window_peak};
}
Monstey_Bytes Monstey_ArchiveRange::Read(std::uint64_t start, std::uint64_t count) {
    std::lock_guard lock(impl_->mutex);
    auto &p = *impl_;
    Monstey_ArchiveRequire(start <= p.entry.size && count <= p.entry.size - start &&
                               count <= 64ull * 1024 * 1024,
                           "Range read exceeds asset or 64 MiB limit");
    p.archive->CheckUnchanged();
    Monstey_Bytes out;
    out.reserve(static_cast<std::size_t>(count));
    if (!count)
        return out;
    auto it = std::upper_bound(p.chunks.begin(), p.chunks.end(), start,
                               [](auto offset, const auto &chunk) { return offset < chunk.decoded_offset; });
    Monstey_ArchiveRequire(it != p.chunks.begin(), "Range lookup has no containing chunk");
    --it;
    auto index = static_cast<std::size_t>(it - p.chunks.begin());
    auto cursor = start;
    while (out.size() < count) {
        auto &chunk = p.chunks.at(index);
        auto &bytes = p.Load(index);
        auto within = cursor - chunk.decoded_offset;
        Monstey_ArchiveRequire(within < bytes.size(), "Range cursor outside decoded chunk");
        auto take =
            static_cast<std::size_t>(std::min<std::uint64_t>(count - out.size(), bytes.size() - within));
        out.insert(out.end(), bytes.begin() + static_cast<std::ptrdiff_t>(within),
                   bytes.begin() + static_cast<std::ptrdiff_t>(within + take));
        cursor += take;
        ++index;
    }
    p.archive->CheckUnchanged();
    return out;
}
Monstey_ArchiveProject::Monstey_ArchiveProject() : history_{initial_} {}
Monstey_ArchiveProject::Monstey_ArchiveProject(const std::filesystem::path &archive)
    : original_(std::make_shared<Monstey_Archive>(archive)) {
    for (auto &e : original_->Entries())
        initial_.files.emplace(e.path, Item{e.path, {}, e.size, {}, {}});
    for (auto &f : original_->Folders())
        initial_.folders.insert(f);
    history_.push_back(initial_);
}
const Monstey_ArchiveProject::State &Monstey_ArchiveProject::Current() const { return history_.at(cursor_); }
std::optional<std::filesystem::path> Monstey_ArchiveProject::Source() const {
    return original_ ? std::optional(original_->Path()) : std::nullopt;
}
bool Monstey_ArchiveProject::Dirty() const noexcept { return !(history_[cursor_] == initial_); }
bool Monstey_ArchiveProject::CanUndo() const noexcept { return cursor_ > 0; }
bool Monstey_ArchiveProject::CanRedo() const noexcept { return cursor_ + 1 < history_.size(); }
void Monstey_ArchiveProject::Commit(State s) {
    if (s == Current())
        return;
    history_.resize(cursor_ + 1);
    history_.push_back(std::move(s));
    ++cursor_;
}
bool Monstey_ArchiveProject::Undo() {
    if (!CanUndo())
        return false;
    --cursor_;
    return true;
}
bool Monstey_ArchiveProject::Redo() {
    if (!CanRedo())
        return false;
    ++cursor_;
    return true;
}
void Monstey_ArchiveProject::Revert() { Commit(initial_); }
Monstey_ArchiveProject::Item Monstey_ArchiveProject::Local(const std::filesystem::path &input) {
    auto p = std::filesystem::absolute(input);
    Monstey_ArchiveRequire(std::filesystem::is_regular_file(Monstey_WindowsFilePath(p)), "source is not a regular file");
    Item result;
    result.local = p;
    result.size = std::filesystem::file_size(Monstey_WindowsFilePath(p));
    result.time = std::filesystem::last_write_time(Monstey_WindowsFilePath(p));
    result.sha256 = Monstey_HashSha256(p);
    Monstey_ArchiveRequire(result.size == std::filesystem::file_size(Monstey_WindowsFilePath(p)) &&
                               result.time == std::filesystem::last_write_time(Monstey_WindowsFilePath(p)),
                           "source changed while hashing");
    return result;
}
void Monstey_ArchiveProject::Parents(State &state, const std::string &path) {
    auto p = path.rfind('/');
    while (p != std::string::npos) {
        auto folder = path.substr(0, p);
        Monstey_ArchiveRequire(!state.files.contains(folder), "parent path is a file");
        state.folders.insert(folder);
        p = folder.rfind('/');
    }
}
std::vector<Monstey_ArchiveEntry> Monstey_ArchiveProject::Entries() const {
    std::vector<Monstey_ArchiveEntry> out;
    for (auto &[p, item] : Current().files)
        out.push_back({p, 0, item.size, item.size, 9});
    return out;
}
std::vector<std::string> Monstey_ArchiveProject::Folders() const {
    return {Current().folders.begin(), Current().folders.end()};
}
void Monstey_ArchiveProject::AddFile(const std::string &path, const std::filesystem::path &source) {
    auto s = Monstey_ArchiveVirtual(path);
    auto state = Current();
    Monstey_ArchiveRequire(!state.files.contains(s) && !state.folders.contains(s),
                           "destination already exists");
    auto item = Local(source);
    Parents(state, s);
    state.files.emplace(s, std::move(item));
    Commit(std::move(state));
}
void Monstey_ArchiveProject::Replace(const std::string &path, const std::filesystem::path &source) {
    auto s = Monstey_ArchiveVirtual(path);
    auto state = Current();
    Monstey_ArchiveRequire(state.files.contains(s), "resource to replace not found");
    state.files[s] = Local(source);
    Commit(std::move(state));
}
void Monstey_ArchiveProject::AddDirectory(const std::string &virtual_directory,
                                          const std::filesystem::path &source) {
    auto prefix = Monstey_ArchiveVirtual(virtual_directory, true);
    auto root = std::filesystem::absolute(source);
    Monstey_ArchiveRequire(std::filesystem::is_directory(root) && !std::filesystem::is_symlink(root),
                           "directory source invalid or symbolic");
    auto state = Current();
    if (!prefix.empty()) {
        Monstey_ArchiveRequire(!state.files.contains(prefix), "directory destination is file");
        Parents(state, prefix);
        state.folders.insert(prefix);
    }
    for (auto &e : std::filesystem::recursive_directory_iterator(root)) {
        Monstey_ArchiveRequire(!e.is_symlink(), "symbolic directory entries are not followed");
        auto rel = Monstey_PathText(e.path().lexically_relative(root));
        auto path = Monstey_ArchiveVirtual((prefix.empty() ? "" : prefix + "/") + rel);
        Parents(state, path);
        if (e.is_directory()) {
            Monstey_ArchiveRequire(!state.files.contains(path), "directory collides with file");
            state.folders.insert(path);
        } else if (e.is_regular_file()) {
            Monstey_ArchiveRequire(!state.files.contains(path) && !state.folders.contains(path),
                                   "directory addition collides with existing path");
            state.files.emplace(path, Local(e.path()));
        } else
            throw std::runtime_error("LTAR: unsupported directory entry");
    }
    Commit(std::move(state));
}
void Monstey_ArchiveProject::Extract(const std::string &path, const std::filesystem::path &output,
                                     const std::filesystem::path &dll) const {
    auto key = Monstey_ArchiveVirtual(path);
    auto found = Current().files.find(key);
    Monstey_ArchiveRequire(found != Current().files.end(), "Project resource not found");
    auto &item = found->second;
    Monstey_NewOutput out(output);
    if (!item.local.empty()) {
        auto check = [&]() {
            Monstey_ArchiveRequire(std::filesystem::file_size(Monstey_WindowsFilePath(item.local)) == item.size &&
                                       std::filesystem::last_write_time(Monstey_WindowsFilePath(item.local)) == item.time &&
                                       Monstey_HashSha256(item.local) == item.sha256,
                                   "Project local source changed since addition");
        };
        check();
        std::ifstream in(Monstey_WindowsFilePath(item.local), std::ios::binary);
        Monstey_ArchiveRequire(in.good(), "Cannot open project local source");
        std::uint64_t remaining = item.size;
        while (remaining) {
            auto bytes =
                Monstey_Exact(in, static_cast<std::size_t>(std::min<std::uint64_t>(65536, remaining)));
            remaining -= bytes.size();
            out.Write(bytes);
        }
        check();
    } else {
        Monstey_ArchiveRequire(original_ != nullptr, "Project original archive unavailable");
        auto e = original_->Find(item.original);
        Monstey_ArchiveRequire(e && e->size == item.size, "Project original resource unavailable");
        original_->Stream(*e, [&](auto bytes) { out.Write(bytes); }, dll);
    }
    out.Finish();
    out.Keep();
}
void Monstey_ArchiveProject::CreateFolder(const std::string &path) {
    auto s = Monstey_ArchiveVirtual(path);
    auto state = Current();
    Monstey_ArchiveRequire(!state.files.contains(s) && !state.folders.contains(s),
                           "folder destination exists");
    Parents(state, s);
    state.folders.insert(s);
    Commit(std::move(state));
}
void Monstey_ArchiveProject::Remove(const std::string &path) {
    auto s = Monstey_ArchiveVirtual(path);
    auto state = Current();
    bool found = state.files.erase(s) != 0;
    if (state.folders.contains(s)) {
        found = true;
        auto prefix = s + "/";
        std::erase_if(state.files, [&](auto &row) { return row.first.starts_with(prefix); });
        std::erase_if(state.folders, [&](auto &p) { return p == s || p.starts_with(prefix); });
    }
    Monstey_ArchiveRequire(found, "path to remove not found");
    Commit(std::move(state));
}
void Monstey_ArchiveProject::Rename(const std::string &path, const std::string &destination) {
    auto s = Monstey_ArchiveVirtual(path), d = Monstey_ArchiveVirtual(destination);
    if (s == d)
        return;
    auto state = Current();
    Monstey_ArchiveRequire(!state.files.contains(d) && !state.folders.contains(d),
                           "rename destination exists");
    Parents(state, d);
    if (auto it = state.files.find(s); it != state.files.end()) {
        auto item = std::move(it->second);
        state.files.erase(it);
        state.files.emplace(d, std::move(item));
    } else {
        Monstey_ArchiveRequire(state.folders.contains(s), "rename source not found");
        Monstey_ArchiveRequire(!d.starts_with(s + "/"), "cannot move folder into itself");
        State renamed;
        renamed.folders.clear();
        for (auto &[p, item] : state.files) {
            auto n = p.starts_with(s + "/") ? d + p.substr(s.size()) : p;
            Monstey_ArchiveRequire(renamed.files.emplace(n, item).second, "rename file collision");
        }
        for (auto &p : state.folders) {
            auto n = (p == s || p.starts_with(s + "/")) ? d + p.substr(s.size()) : p;
            Monstey_ArchiveRequire(renamed.folders.insert(n).second, "rename folder collision");
        }
        state = std::move(renamed);
    }
    Commit(std::move(state));
}
void Monstey_ArchiveProject::Save(const std::filesystem::path &output) const {
    Monstey_Bytes b{'M', 'S', 'A', 'P'};
    Monstey_A32(b, 1);
    Monstey_AString(b, original_ ? Monstey_PathText(original_->Path()) : "");
    if (original_)
        original_->CheckUnchanged();
    Monstey_AString(b, original_ ? Monstey_HashSha256(original_->Path()) : "");
    if (original_)
        original_->CheckUnchanged();
    auto &s = Current();
    Monstey_ArchiveRequire(s.files.size() <= 1000000 && s.folders.size() <= 1000000,
                           "project count limit exceeded");
    Monstey_A32(b, static_cast<std::uint32_t>(s.folders.size()));
    for (auto &f : s.folders)
        Monstey_AString(b, f);
    Monstey_A32(b, static_cast<std::uint32_t>(s.files.size()));
    for (auto &[p, item] : s.files) {
        if (!item.local.empty())
            Monstey_ArchiveRequire(std::filesystem::file_size(Monstey_WindowsFilePath(item.local)) == item.size &&
                                       std::filesystem::last_write_time(Monstey_WindowsFilePath(item.local)) == item.time &&
                                       Monstey_HashSha256(item.local) == item.sha256,
                                   "local source changed before project save");
        Monstey_AString(b, p);
        Monstey_AString(b, item.original);
        Monstey_AString(b, Monstey_PathText(item.local));
        Monstey_A64(b, item.size);
        Monstey_AString(b, item.sha256);
    }
    Monstey_WriteNewFile(output, b);
}
Monstey_ArchiveProject Monstey_ArchiveProject::Load(const std::filesystem::path &input) {
    auto b = Monstey_ReadFile(input, 128ull * 1024 * 1024);
    Monstey_ArchiveRequire(b.size() >= 8 && std::memcmp(b.data(), "MSAP", 4) == 0 &&
                               Monstey_AU32(b, 4, false) == 1,
                           "invalid project magic or version");
    std::size_t p = 8;
    auto u32 = [&]() {
        auto v = Monstey_AU32(b, p, false);
        p += 4;
        return v;
    };
    auto text = [&]() {
        auto n = u32();
        Monstey_ArchiveRequire(n <= 16 * 1024 * 1024 && p <= b.size() && n <= b.size() - p,
                               "project string outside extent");
        std::string s(b.begin() + static_cast<std::ptrdiff_t>(p),
                      b.begin() + static_cast<std::ptrdiff_t>(p + n));
        p += n;
        (void)Monstey_Wide(s);
        return s;
    };
    auto archive = text(), expected = text();
    Monstey_ArchiveProject result;
    if (!archive.empty()) {
        auto source = Monstey_TextPath(archive);
        Monstey_ArchiveRequire(Monstey_HashSha256(source) == expected,
                               "project source archive hash mismatch");
        result = Monstey_ArchiveProject(source);
    } else
        Monstey_ArchiveRequire(expected.empty(), "hash without archive");
    State state;
    state.folders.clear();
    auto n = u32();
    Monstey_ArchiveRequire(n > 0 && n <= 1000000, "invalid project folder count");
    for (std::uint32_t j = 0; j < n; ++j)
        Monstey_ArchiveRequire(state.folders.insert(Monstey_ArchiveVirtual(text(), true)).second,
                               "duplicate project folder");
    Monstey_ArchiveRequire(state.folders.contains(""), "project root missing");
    n = u32();
    Monstey_ArchiveRequire(n <= 1000000, "project file count limit");
    for (std::uint32_t j = 0; j < n; ++j) {
        auto path = Monstey_ArchiveVirtual(text());
        auto original = text(), local = text();
        auto size = Monstey_AU64(b, p, false);
        p += 8;
        auto hash = text();
        Item item;
        if (!local.empty()) {
            Monstey_ArchiveRequire(original.empty(), "project item has two sources");
            item = Local(Monstey_TextPath(local));
            Monstey_ArchiveRequire(item.size == size && item.sha256 == hash,
                                   "project local source hash mismatch");
        } else {
            Monstey_ArchiveRequire(result.original_ && !original.empty(), "project item has no source");
            auto e = result.original_->Find(original);
            Monstey_ArchiveRequire(e && e->size == size && hash.empty(), "project original resource missing");
            item.original = e->path;
            item.size = size;
        }
        Monstey_ArchiveRequire(!state.folders.contains(path) &&
                                   state.files.emplace(path, std::move(item)).second,
                               "project path collision");
        Parents(state, path);
    }
    Monstey_ArchiveRequire(p == b.size(), "project trailing data");
    for (auto &f : state.folders)
        if (!f.empty())
            Parents(state, f);
    result.Commit(std::move(state));
    return result;
}
Monstey_ArchiveBuildReport Monstey_ArchiveProject::Build(const std::filesystem::path &output,
                                                         bool include_patchlist,
                                                         const std::filesystem::path &dll) const {
    if (original_)
        original_->CheckUnchanged();
    auto state = Current();
    constexpr const char *patch_name = "archivepatchloosefiles.txt";
    Monstey_Bytes patch;
    if (include_patchlist) {
        std::vector<std::string> paths;
        for (auto &[p, item] : state.files) {
            (void)item;
            if (p != patch_name)
                paths.push_back(p);
        }
        patch = Monstey_BuildArchivePatchList(paths);
        state.files[patch_name] = Item{"", {}, patch.size(), {}, ""};
    }
    struct Folder {
        std::string path;
        std::int32_t child{-1}, sibling{-1};
        std::uint32_t count{};
    };
    std::map<std::string, std::vector<std::string>> children;
    std::map<std::string, std::vector<std::string>> groups;
    for (auto &f : state.folders) {
        for (auto c : f)
            Monstey_ArchiveRequire(static_cast<unsigned char>(c) < 128,
                                   "LTAR writer requires ASCII virtual paths");
        if (f.empty())
            continue;
        auto slash = f.rfind('/');
        auto parent = slash == std::string::npos ? "" : f.substr(0, slash);
        Monstey_ArchiveRequire(state.folders.contains(parent), "folder parent missing");
        children[parent].push_back(f);
    }
    for (auto &[p, item] : state.files) {
        (void)item;
        for (auto c : p)
            Monstey_ArchiveRequire(static_cast<unsigned char>(c) < 128,
                                   "LTAR writer requires ASCII virtual paths");
        auto slash = p.rfind('/');
        auto parent = slash == std::string::npos ? "" : p.substr(0, slash);
        Monstey_ArchiveRequire(state.folders.contains(parent) && !state.folders.contains(p),
                               "file/folder tree inconsistent");
        groups[parent].push_back(p);
    }
    std::vector<Folder> folders;
    std::vector<std::string> paths;
    std::function<std::int32_t(const std::string &, unsigned)> flatten;
    flatten = [&](const std::string &path, unsigned depth) {
        Monstey_ArchiveRequire(depth <= 256, "folder nesting exceeds 256");
        Monstey_ArchiveRequire(folders.size() <
                                   static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()),
                               "too many folders");
        auto index = static_cast<std::int32_t>(folders.size());
        auto &group = groups[path];
        folders.push_back({path, -1, -1, static_cast<std::uint32_t>(group.size())});
        paths.insert(paths.end(), group.begin(), group.end());
        std::int32_t previous = -1;
        for (auto &child : children[path]) {
            auto ci = flatten(child, depth + 1);
            if (previous == -1)
                folders[static_cast<std::size_t>(index)].child = ci;
            else
                folders[static_cast<std::size_t>(previous)].sibling = ci;
            previous = ci;
        }
        return index;
    };
    flatten("", 0);
    Monstey_ArchiveRequire(paths.size() <= 10000000 && folders.size() <= 1000000,
                           "archive count limit exceeded");
    Monstey_Bytes names;
    std::map<std::string, std::uint32_t> offsets;
    auto add_name = [&](const std::string &s) {
        if (offsets.contains(s))
            return;
        Monstey_ArchiveRequire(names.size() + s.size() + 1 <=
                                   static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()),
                               "archive name table too large");
        offsets[s] = static_cast<std::uint32_t>(names.size());
        names.insert(names.end(), s.begin(), s.end());
        names.push_back(0);
    };
    for (auto &f : folders) {
        auto s = f.path;
        std::replace(s.begin(), s.end(), '/', '\\');
        add_name(s);
    }
    for (auto &p : paths)
        add_name(p.substr(p.rfind('/') == std::string::npos ? 0 : p.rfind('/') + 1));
    Monstey_Bytes header{'L', 'T', 'A', 'R'};
    Monstey_A32(header, 4);
    Monstey_A32(header, static_cast<std::uint32_t>(names.size()));
    Monstey_A32(header, static_cast<std::uint32_t>(folders.size()));
    Monstey_A32(header, static_cast<std::uint32_t>(paths.size()));
    Monstey_A32(header, 1);
    header.resize(48);
    auto records_offset = 48ull + names.size();
    auto position = records_offset + 30ull * paths.size() + 16ull * folders.size() + 6ull * paths.size();
    Monstey_ArchiveRequire(position <= 256ull * 1024 * 1024, "archive index exceeds reader limit");
    Monstey_NewOutput out(output);
    out.Write(header);
    out.Write(names);
    out.Write(Monstey_Bytes(paths.size() * 30));
    for (auto &f : folders) {
        auto s = f.path;
        std::replace(s.begin(), s.end(), '/', '\\');
        Monstey_Bytes record;
        Monstey_A32(record, offsets.at(s));
        Monstey_A32(record, static_cast<std::uint32_t>(f.child));
        Monstey_A32(record, static_cast<std::uint32_t>(f.sibling));
        Monstey_A32(record, f.count);
        out.Write(record);
    }
    out.Write(Monstey_Bytes(paths.size() * 6));
    std::vector<Monstey_ArchiveEntry> records;
    auto source_stream = [&](const std::string &p, const Item &item, const Monstey_Archive::Sink &sink) {
        if (include_patchlist && p == patch_name) {
            for (std::size_t j = 0; j < patch.size(); j += 65536)
                sink(std::span(patch).subspan(j, std::min<std::size_t>(65536, patch.size() - j)));
            return;
        }
        if (!item.local.empty()) {
            Monstey_ArchiveRequire(std::filesystem::file_size(Monstey_WindowsFilePath(item.local)) == item.size &&
                                       std::filesystem::last_write_time(Monstey_WindowsFilePath(item.local)) == item.time &&
                                       Monstey_HashSha256(item.local) == item.sha256,
                                   "local source changed since addition");
            std::ifstream in(Monstey_WindowsFilePath(item.local), std::ios::binary);
            Monstey_ArchiveRequire(in.good(), "local source open failed");
            std::uint64_t remaining = item.size;
            while (remaining) {
                auto chunk =
                    Monstey_Exact(in, static_cast<std::size_t>(std::min<std::uint64_t>(65536, remaining)));
                remaining -= chunk.size();
                sink(chunk);
            }
            Monstey_ArchiveRequire(std::filesystem::file_size(Monstey_WindowsFilePath(item.local)) == item.size &&
                                       std::filesystem::last_write_time(Monstey_WindowsFilePath(item.local)) == item.time,
                                   "local source changed during build");
        } else {
            Monstey_ArchiveRequire(original_ != nullptr, "original source unavailable");
            auto e = original_->Find(item.original);
            Monstey_ArchiveRequire(e && e->size == item.size, "original resource unavailable");
            original_->Stream(*e, sink, dll);
        }
    };
    for (auto &p : paths) {
        auto &item = state.files.at(p);
        auto start = position;
        std::uint64_t count{};
        source_stream(p, item, [&](auto chunk) {
            Monstey_ArchiveRequire(!chunk.empty() && chunk.size() <= 65536, "source chunk invalid");
            Monstey_Bytes h;
            Monstey_A32(h, static_cast<std::uint32_t>(chunk.size()));
            Monstey_A32(h, static_cast<std::uint32_t>(chunk.size()));
            out.Write(h);
            out.Write(chunk);
            position += 8 + chunk.size();
            count += chunk.size();
            auto padding = static_cast<std::size_t>((4 - (position - start) % 4) % 4);
            out.Write(Monstey_Bytes(padding, 'X'));
            position += padding;
        });
        Monstey_ArchiveRequire(count == item.size, "source length changed");
        records.push_back({p, start, position - start, item.size, 9});
    }
    out.Seek(records_offset);
    for (auto &r : records) {
        Monstey_Bytes record;
        auto slash = r.path.rfind('/');
        Monstey_A32(record, offsets.at(r.path.substr(slash == std::string::npos ? 0 : slash + 1)));
        Monstey_A64(record, r.offset);
        Monstey_A64(record, r.stored_size);
        Monstey_A64(record, r.size);
        record.push_back(1);
        record.push_back(9);
        out.Write(record);
    }
    out.Finish();
    Monstey_Archive reread(output);
    Monstey_ArchiveRequire(reread.Version() == 4 && reread.Length() == position &&
                               reread.Entries().size() == paths.size() &&
                               reread.Folders().size() == folders.size(),
                           "archive readback index mismatch");
    std::ifstream verified(Monstey_WindowsFilePath(output), std::ios::binary);
    for (auto &p : paths) {
        auto e = reread.Find(p);
        Monstey_ArchiveRequire(e && e->size == state.files.at(p).size, "archive readback entry mismatch");
        verified.seekg(static_cast<std::streamoff>(e->offset));
        auto cursor = e->offset;
        source_stream(p, state.files.at(p), [&](auto expected) {
            auto h = Monstey_Exact(verified, 8);
            auto stored = Monstey_AU32(h, 0, false), decoded = Monstey_AU32(h, 4, false);
            Monstey_ArchiveRequire(stored == expected.size() && decoded == expected.size(),
                                   "readback raw chunk header mismatch");
            auto actual = Monstey_Exact(verified, stored);
            Monstey_ArchiveRequire(std::equal(actual.begin(), actual.end(), expected.begin(), expected.end()),
                                   "readback payload differs from source");
            cursor += 8 + stored;
            auto pad = static_cast<std::size_t>((4 - (cursor - e->offset) % 4) % 4);
            auto padding = Monstey_Exact(verified, pad);
            Monstey_ArchiveRequire(
                std::all_of(padding.begin(), padding.end(), [](auto c) { return c == 'X'; }),
                "readback padding mismatch");
            cursor += pad;
        });
        Monstey_ArchiveRequire(cursor == e->offset + e->stored_size, "readback stored length mismatch");
    }
    if (include_patchlist) {
        auto loaded = Monstey_ReadArchivePatchList(reread.Read(patch_name));
        Monstey_ArchiveRequire(loaded.size() + 1 == paths.size(), "readback patch-list count mismatch");
    }
    if (original_)
        original_->CheckUnchanged();
    auto sha = Monstey_HashSha256(output);
    out.Keep();
    return {std::filesystem::absolute(output), position, paths.size(), folders.size(), sha, true};
}
} // namespace Monstey
