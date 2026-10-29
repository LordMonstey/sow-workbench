#include "Monstey_Anix.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <random>
#include <set>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
using Q = Monstey_AnixQuaternion;
using V = Monstey_AnixVector;
void Monstey_TestRequire(bool v, std::string_view m) {
    if (!v)
        throw std::runtime_error(std::string(m));
}
void Monstey_Throws(const std::function<void()> &f) {
    bool caught = false;
    try {
        f();
    } catch (const Monstey_Error &) {
        caught = true;
    }
    Monstey_TestRequire(caught, "Expected validation error");
}
std::uint16_t Monstey_TestChannel(const Monstey_Anix &library, std::uint32_t type, std::uint32_t bone) {
    const auto description = library.Describe();
    for (const auto &channel : description["channels"].as_array())
        if (channel["type_hash"].as_string() == Monstey_Hex(type) &&
            channel["bone_hash"].as_string() == Monstey_Hex(bone))
            return static_cast<std::uint16_t>(channel["index"].as_uint());
    throw std::runtime_error("Expected native channel identity is absent");
}
const J &Monstey_TestTrack(const J &clip, std::uint16_t channel) {
    for (const auto &track : clip["tracks"].as_array())
        if (track["channel_index"].as_uint() == channel)
            return track;
    throw std::runtime_error("Expected native track identity is absent");
}
J Monstey_Track(unsigned channel, J a, J b) {
    return J::object({{"channel_index", channel}, {"values", J::array({std::move(a), std::move(b)})}});
}
J Monstey_Clip(std::string name, bool animated = false) {
    return J::object(
        {{"name", name},
         {"duration_ticks", 1000},
         {"ticks", J::array({0, 1000})},
         {"tracks",
          J::array(
              {Monstey_Track(0, J::array({0, 0, 0}), animated ? J::array({20, 0, 0}) : J::array({0, 0, 0})),
               Monstey_Track(1, J::array({0, 0, 0, 1}),
                             animated ? J::array({0, 0, 0.7071067811865476, 0.7071067811865476})
                                      : J::array({0, 0, 0, 1})),
               Monstey_Track(2, J::array({1, 20, 3}), animated ? J::array({4, 25, 6}) : J::array({1, 20, 3})),
               Monstey_Track(3, J::array({0, 0, 0, 1}), J::array({0, 0, 0, 1}))})},
         {"events", animated ? J::array({J::object({{"tick", 333}, {"text", "SYNTH_HIT"}}),
                                         J::object({{"tick", 667}, {"text", "SYNTH_STOP"}})})
                             : J::array()}});
}
J Monstey_Scene() {
    return J::object(
        {{"library_path", "synthetic/native.anix"},
         {"channels",
          J::array(
              {J::object({{"type_hash", "00ce66fc"}, {"bone_hash", "88baa7e7"}}),
               J::object({{"type_hash", "27436396"}, {"bone_hash", "88baa7e7"}}),
               J::object({{"type_hash", "00ce66fc"}, {"bone_hash", Monstey_Hex(Monstey_NativeHash("Arm"))}}),
               J::object({{"type_hash", "27436396"}, {"bone_hash", Monstey_Hex(Monstey_NativeHash("Arm"))}}),
               J::object({{"type_hash", "210c8f5a"}, {"bone_hash", "88baa7e7"}})})},
         {"clips", J::array({Monstey_Clip("bind"), Monstey_Clip("walk", true)})}});
}
std::size_t Monstey_Descriptor(const B &b, std::size_t index) {
    auto m = Monstey_U32(b, 32);
    return m + Monstey_U32(b, m + 20 + 8 * index);
}
std::size_t Monstey_SegmentTable(const B &b, std::size_t d) {
    const auto nc = Monstey_U16(b, d + 56), nt = Monstey_U16(b, d + 58);
    return ((d + 124 + 4 * nc + nt + 3) / 4 * 4 + 2 * nt + 3) / 4 * 4;
}
J Monstey_Op(std::string op, std::string clip) { return J::object({{"op", op}, {"clip", clip}}); }
double Monstey_QError(Q a, Q b) {
    double dot = 0;
    for (std::size_t i = 0; i < 4; ++i)
        dot += static_cast<double>(a[i]) * b[i];
    return 2 * std::acos(std::clamp(std::abs(dot), 0., 1.));
}
void Monstey_Write(const std::filesystem::path &p, const B &b) { Monstey_WriteNewFile(p, b); }
} // namespace

B Monstey_StaticFixture(const B &source, bool extra = false) {
    const auto oldpayload = Monstey_U32(source, 24), oldmeta = Monstey_U32(source, 32),
               oldfooter = oldmeta + Monstey_U32(source, oldmeta + 12);
    B segment(extra ? 16 : 16);
    Monstey_PutU32(segment, 0, extra ? 1 : 0);
    Monstey_PutU16(segment, 4, 16);
    if (extra) {
        Monstey_PutU16(segment, 6, 32);
        Monstey_PutU32(segment, 8, 0x1234abcd);
    }
    const std::array<Q, 1> q{Q{0, 0, 0, 1}};
    auto row = Monstey_AnixPackRow(q);
    segment.insert(segment.end(), row.begin(), row.end());
    if (extra) {
        Monstey_AppendU32(segment, 0x5a55a55a);
        segment.resize(48);
    }
    B meta(32);
    Monstey_PutU32(meta, 0, 16);
    Monstey_PutU32(meta, 4, 2);
    for (std::size_t i = 0; i < 2; ++i) {
        const auto old = Monstey_Descriptor(source, i);
        B desc(source.begin() + old, source.begin() + old + 124);
        Monstey_PutU16(desc, 48, 140);
        Monstey_PutU16(desc, 52, 1);
        Monstey_PutU16(desc, 54, 0);
        Monstey_PutU16(desc, 56, 1);
        Monstey_PutU16(desc, 58, 1);
        const std::string code = "SQ03";
        desc.insert(desc.end(), code.begin(), code.end());
        desc.push_back(0);
        desc.resize(132);
        Monstey_AppendU16(desc, Monstey_TestChannel(Monstey_Anix(source), 0x27436396, 0x88baa7e7));
        desc.resize(136);
        for (auto v : {static_cast<std::uint32_t>(i * segment.size()),
                       static_cast<std::uint32_t>(segment.size()), 0u, 1000u})
            Monstey_AppendU32(desc, v);
        desc.resize(160);
        Monstey_PutU32(meta, 16 + 8 * i, Monstey_U32(source, oldmeta + 16 + 8 * i));
        Monstey_PutU32(meta, 20 + 8 * i, static_cast<std::uint32_t>(meta.size()));
        meta.insert(meta.end(), desc.begin(), desc.end());
    }
    Monstey_PutU32(meta, 12, static_cast<std::uint32_t>(meta.size()));
    meta.insert(meta.end(), source.begin() + oldfooter, source.end());
    B out(source.begin(), source.begin() + oldpayload);
    out.insert(out.end(), segment.begin(), segment.end());
    out.insert(out.end(), segment.begin(), segment.end());
    Monstey_PutU32(out, 28, static_cast<std::uint32_t>(2 * segment.size()));
    Monstey_PutU32(out, 32, static_cast<std::uint32_t>(out.size()));
    Monstey_PutU32(out, 36, static_cast<std::uint32_t>(meta.size()));
    out.insert(out.end(), meta.begin(), meta.end());
    Monstey_PutU32(out, 12, static_cast<std::uint32_t>(out.size()));
    return out;
}
B Monstey_ExtraFixture(const B &source) {
    const auto oldmeta = Monstey_U32(source, 32), payload = Monstey_U32(source, 24);
    const auto d = Monstey_Descriptor(source, 0), table = Monstey_SegmentTable(source, d);
    const auto nc = Monstey_U16(source, d + 56);
    const auto oldoff = Monstey_U32(source, table), oldlen = Monstey_U32(source, table + 4);
    const auto old = std::span<const std::uint8_t>(source).subspan(payload + oldoff, oldlen);
    std::vector<std::size_t> offsets;
    for (std::size_t i = 0; i < static_cast<std::size_t>(nc) + 1; ++i)
        offsets.push_back(Monstey_U16(old, 4 + 2 * i));
    offsets.push_back(old.size());
    const auto hashat = (4 + 2 * (nc + 2) + 3) / 4 * 4;
    B segment(hashat + 8);
    Monstey_PutU32(segment, 0, 2);
    Monstey_PutU32(segment, hashat, 0x2b99dbc1);
    Monstey_PutU32(segment, hashat + 4, 0x5555aaaa);
    for (std::size_t i = 0; i < nc; ++i) {
        const std::string code(reinterpret_cast<const char *>(source.data() + d + 124 + 4 * i), 4);
        const auto align = code == "AQ03" || code == "SQ03" || code == "SV01" ? 16u : 4u;
        segment.resize((segment.size() + align - 1) / align * align);
        Monstey_PutU16(segment, 4 + 2 * i, static_cast<std::uint16_t>(segment.size()));
        segment.insert(segment.end(), old.begin() + offsets[i], old.begin() + offsets[i + 1]);
    }
    segment.resize((segment.size() + 3) / 4 * 4);
    Monstey_PutU16(segment, 4 + 2 * nc, static_cast<std::uint16_t>(segment.size()));
    segment.insert(segment.end(), old.begin() + offsets[nc], old.end());
    segment.resize((segment.size() + 3) / 4 * 4);
    Monstey_PutU16(segment, 6 + 2 * nc, static_cast<std::uint16_t>(segment.size()));
    Monstey_AppendU32(segment, 0xfeed1234);
    segment.resize((segment.size() + 15) / 16 * 16);
    B metadata(source.begin() + oldmeta, source.end());
    const auto rel = table - oldmeta;
    Monstey_PutU32(metadata, rel, oldmeta - payload);
    Monstey_PutU32(metadata, rel + 4, static_cast<std::uint32_t>(segment.size()));
    B out(source.begin(), source.begin() + oldmeta);
    out.insert(out.end(), segment.begin(), segment.end());
    Monstey_PutU32(out, 28, oldmeta - payload + static_cast<std::uint32_t>(segment.size()));
    Monstey_PutU32(out, 32, static_cast<std::uint32_t>(out.size()));
    out.insert(out.end(), metadata.begin(), metadata.end());
    Monstey_PutU32(out, 12, static_cast<std::uint32_t>(out.size()));
    return out;
}

B Monstey_SkelFixture() {
    struct Entry {
        std::string path;
        std::uint32_t offset = 0;
    };
    std::vector<Entry> entries{{"characters\\animationcharacters\\synthetic\\one.anix"},
                               {"characters\\animationcharacters\\synthetic\\two.anix"}};
    B table;
    for (auto &e : entries) {
        e.offset = static_cast<std::uint32_t>(table.size());
        table.insert(table.end(), e.path.begin(), e.path.end());
        table.push_back(0);
    }
    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
        return Monstey_PathHash(a.path) < Monstey_PathHash(b.path);
    });
    B out(64);
    Monstey_PutU32(out, 0, 0x4c454b53);
    Monstey_PutU32(out, 4, 17);
    Monstey_PutU32(out, 8, 64);
    Monstey_PutU32(out, 12, static_cast<std::uint32_t>(table.size()));
    Monstey_PutU32(out, 16, 2);
    Monstey_PutU32(out, 28, 2);
    Monstey_PutU32(out, 40, 128);
    out.insert(out.end(), {0x21, 0x43, 0x65, 0x87});
    Monstey_AppendU32(out, 0xdcdcff00);
    out.insert(out.end(), table.begin(), table.end());
    out.insert(out.end(), {0x11, 0x22, 0x33, 0x44, 0x55});
    Monstey_AppendU32(out, 0xdcdcff01);
    for (const auto &e : entries)
        Monstey_AppendU32(out, e.offset);
    out.insert(out.end(), {1, 2, 3, 4, 5, 6, 7, 8});
    Monstey_AppendU32(out, 0xdcdcff02);
    for (std::size_t i = 0; i < entries.size(); ++i)
        Monstey_AppendU32(out, static_cast<std::uint32_t>(i + 1));
    for (auto tag : {3u, 4u, 5u, 0x99u}) {
        Monstey_AppendU32(out, 0xdcdcff00 | tag);
        out.insert(out.end(), {0x5a, 0xa5});
    }
    return out;
}

B Monstey_ExplicitRootFixture(bool partial, bool constant = false) {
    auto scene = Monstey_Scene();
    if (partial) {
        auto &tracks = scene["clips"][1]["tracks"].as_array();
        tracks.erase(tracks.begin(), tracks.begin() + 2);
    }
    auto raw = Monstey_Anix::Create(scene).Build();
    const auto descriptor = Monstey_Descriptor(raw, 1);
    const std::array<float, 7> start{-0.0f, 30.25f, -20.5f, 0.0f, 0.38268343f, 0.0f, 0.9238795f};
    const std::array<float, 7> end{77.5f, -80.125f, 99.25f, 0.70710677f, 0.0f, 0.0f, 0.70710677f};
    for (std::size_t i = 0; i < 7; ++i) {
        Monstey_PutU32(raw, descriptor + 60 + 4 * i, std::bit_cast<std::uint32_t>(start[i]));
        Monstey_PutU32(raw, descriptor + 88 + 4 * i,
                       std::bit_cast<std::uint32_t>(constant ? start[i] : end[i]));
    }
    return raw;
}
B Monstey_RootDescriptorBytes(const B &raw) {
    const auto descriptor = Monstey_Descriptor(raw, 1);
    return B(raw.begin() + descriptor + 60, raw.begin() + descriptor + 116);
}

J Monstey_MultipleEventScene() {
    auto scene = Monstey_Scene();
    scene["channels"].as_array().push_back(
        J::object({{"type_hash", "210c8f5a"}, {"bone_hash", Monstey_Hex(Monstey_NativeHash("Arm"))}}));
    scene["clips"][1]["event_channel_index"] = 5;
    return scene;
}
B Monstey_MultipleStriTrackFixture() {
    auto raw = Monstey_Anix::Create(Monstey_MultipleEventScene()).Build();
    const auto descriptor = Monstey_Descriptor(raw, 1);
    const auto codecs = Monstey_U16(raw, descriptor + 56), tracks = Monstey_U16(raw, descriptor + 58);
    const auto types = descriptor + 124 + 4 * codecs;
    const auto ids = (types + tracks + 3) / 4 * 4;
    std::uint8_t stri = 0;
    for (std::size_t i = 0; i < codecs; ++i)
        if (std::string(reinterpret_cast<const char *>(raw.data() + descriptor + 124 + 4 * i), 4) == "STRI")
            stri = static_cast<std::uint8_t>(i);
    for (std::size_t i = 0; i < tracks; ++i)
        if (Monstey_U16(raw, ids + 2 * i) == Monstey_TestChannel(Monstey_Anix(raw), 0x27436396, Monstey_NativeHash("Arm"))) {
            raw[types + i] = stri;
            Monstey_PutU16(raw, ids + 2 * i, Monstey_TestChannel(Monstey_Anix(raw), 0x210c8f5a, 0x88baa7e7));
        }
    return raw;
}

B Monstey_DifferingStriFixture() {
    auto scene = Monstey_MultipleEventScene();
    auto &clip = scene["clips"][1];
    clip["duration_ticks"] = 900;
    clip["ticks"] = J::array();
    for (unsigned i = 0; i <= 300; ++i)
        clip["ticks"].as_array().emplace_back(i * 3);
    for (auto &track : clip["tracks"].as_array()) {
        const auto first = track["values"][0];
        track["values"] = J::array();
        for (unsigned i = 0; i <= 300; ++i)
            track["values"].as_array().push_back(first);
    }
    auto raw = Monstey_Anix::Create(scene).Build();
    const auto descriptor = Monstey_Descriptor(raw, 1);
    Monstey_TestRequire(Monstey_U16(raw, descriptor + 52) >= 2, "STRI fixture did not split");
    const auto codec_count = Monstey_U16(raw, descriptor + 56);
    std::size_t stri = codec_count;
    for (std::size_t i = 0; i < codec_count; ++i)
        if (std::string(reinterpret_cast<const char *>(raw.data() + descriptor + 124 + 4 * i), 4) == "STRI")
            stri = i;
    Monstey_TestRequire(stri < codec_count, "STRI fixture codec is absent");
    const auto table = Monstey_SegmentTable(raw, descriptor);
    const auto segment = Monstey_U32(raw, 24) + Monstey_U32(raw, table + 16);
    const auto block = segment + Monstey_U16(raw, segment + 4 + 2 * stri);
    Monstey_PutU32(raw, block + 10, 334);
    return raw;
}

B Monstey_TestCaf4Block() {
    B bytes;
    for (auto value : {1u, 0u, 1000u, 3u, 3u})
        Monstey_AppendU32(bytes, value);
    for (auto delta : {0u, 200u, 800u})
        Monstey_AppendU16(bytes, static_cast<std::uint16_t>(delta));
    bytes.resize(32, 0x55);
    for (const auto &row : std::array<std::array<float, 2>, 3>{{{{0, 100}}, {{10, 200}}, {{30, 400}}}}) {
        for (float value : row)
            Monstey_AppendU32(bytes, std::bit_cast<std::uint32_t>(value));
        Monstey_AppendU32(bytes, 0x7fc01234); // Unused row-stride column, never read by the callback.
    }
    bytes.resize(80, 0xa5); // Opaque unused tail is not an extra key row.
    return bytes;
}
B Monstey_TestStaticWords(bool nonfinite = false) {
    B bytes;
    for (auto offset : {8u, 12u, 16u})
        Monstey_AppendU16(bytes, static_cast<std::uint16_t>(offset));
    bytes.resize(8, 0x5a);
    Monstey_AppendU32(bytes, 0x3f800000);
    Monstey_AppendU32(bytes, 0x80000000);
    Monstey_AppendU32(bytes, nonfinite ? 0x7fc01234 : 0x42280000);
    bytes.resize(24, 0x37);
    return bytes;
}
B Monstey_TestStaticByte() {
    B bytes;
    Monstey_AppendU16(bytes, 3);
    bytes.push_back(0xa5);
    bytes.push_back(0xff);
    return bytes;
}
B Monstey_TestCodecSegment(const std::vector<std::string> &codecs, const std::vector<B> &blocks) {
    B bytes((4 + 2 * codecs.size() + 3) / 4 * 4);
    Monstey_PutU32(bytes, 0, 0); // No LITH: these three codecs have no external-time dependency.
    for (std::size_t i = 0; i < codecs.size(); ++i) {
        const auto alignment = codecs[i] == "CAF4" ? 16u : 4u;
        bytes.resize((bytes.size() + alignment - 1) / alignment * alignment);
        Monstey_PutU16(bytes, 4 + 2 * i, static_cast<std::uint16_t>(bytes.size()));
        bytes.insert(bytes.end(), blocks[i].begin(), blocks[i].end());
    }
    bytes.resize((bytes.size() + 15) / 16 * 16);
    return bytes;
}
B Monstey_RareFixture(bool nonfinite = false) {
    auto scene = Monstey_Scene();
    for (auto hash : {"11111111", "22222222", "33333333", "44444444"})
        scene["channels"].as_array().push_back(J::object({{"type_hash", "5f1d8eed"}, {"bone_hash", hash}}));
    const auto source = Monstey_Anix::Create(scene).Build();
    const auto old_payload = Monstey_U32(source, 24), old_meta = Monstey_U32(source, 32);
    const auto footer = old_meta + Monstey_U32(source, old_meta + 12);
    const std::array<std::vector<std::string>, 2> codecs{{{"USFT", "USBT"}, {"CAF4"}}};
    const std::array<std::vector<std::pair<unsigned int, unsigned int>>, 2> tracks{
        {{{0u, 5u}, {0u, 6u}, {0u, 8u}, {1u, 7u}}, {{0u, 5u}, {0u, 6u}}}};
    const std::array<B, 2> segments{
        Monstey_TestCodecSegment(codecs[0], {Monstey_TestStaticWords(nonfinite), Monstey_TestStaticByte()}),
        Monstey_TestCodecSegment(codecs[1], {Monstey_TestCaf4Block()})};
    B metadata(32);
    Monstey_PutU32(metadata, 0, 16);
    Monstey_PutU32(metadata, 4, 2);
    std::uint32_t payload_offset = 0;
    for (std::size_t i = 0; i < 2; ++i) {
        const auto old = Monstey_Descriptor(source, i);
        B descriptor(source.begin() + old, source.begin() + old + 124);
        Monstey_PutU16(descriptor, 48, 140);
        Monstey_PutU16(descriptor, 52, 1);
        Monstey_PutU16(descriptor, 54, 0);
        Monstey_PutU16(descriptor, 56, static_cast<std::uint16_t>(codecs[i].size()));
        Monstey_PutU16(descriptor, 58, static_cast<std::uint16_t>(tracks[i].size()));
        for (const auto &code : codecs[i])
            descriptor.insert(descriptor.end(), code.begin(), code.end());
        for (const auto &track : tracks[i])
            descriptor.push_back(static_cast<std::uint8_t>(track.first));
        descriptor.resize((descriptor.size() + 3) / 4 * 4);
        for (const auto &track : tracks[i])
            Monstey_AppendU16(descriptor, static_cast<std::uint16_t>(track.second));
        descriptor.resize((descriptor.size() + 3) / 4 * 4);
        for (auto word : {payload_offset, static_cast<std::uint32_t>(segments[i].size()), 0u, 1000u})
            Monstey_AppendU32(descriptor, word);
        descriptor.resize((descriptor.size() + 15) / 16 * 16);
        Monstey_PutU32(metadata, 16 + 8 * i, Monstey_U32(source, old_meta + 16 + 8 * i));
        Monstey_PutU32(metadata, 20 + 8 * i, static_cast<std::uint32_t>(metadata.size()));
        metadata.insert(metadata.end(), descriptor.begin(), descriptor.end());
        payload_offset += static_cast<std::uint32_t>(segments[i].size());
    }
    Monstey_PutU32(metadata, 12, static_cast<std::uint32_t>(metadata.size()));
    metadata.insert(metadata.end(), source.begin() + footer, source.end());
    B output(source.begin(), source.begin() + old_payload);
    for (const auto &segment : segments)
        output.insert(output.end(), segment.begin(), segment.end());
    Monstey_PutU32(output, 28, payload_offset);
    Monstey_PutU32(output, 32, static_cast<std::uint32_t>(output.size()));
    Monstey_PutU32(output, 36, static_cast<std::uint32_t>(metadata.size()));
    output.insert(output.end(), metadata.begin(), metadata.end());
    Monstey_PutU32(output, 12, static_cast<std::uint32_t>(output.size()));
    return output;
}

int wmain(int argc, wchar_t **argv) {
    std::filesystem::path out = argc > 1 ? argv[1] : std::filesystem::temp_directory_path();
    out /= L"Anix_" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(out);
    J results = J::array();
    unsigned failed = 0;
    auto test = [&](const char *name, const std::function<void()> &f) {
        try {
            f();
            results.as_array().push_back(J::object({{"name", name}, {"passed", true}}));
            std::cout << "PASS " << name << "\n";
        } catch (const std::exception &e) {
            ++failed;
            results.as_array().push_back(J::object({{"name", name}, {"passed", false}, {"error", e.what()}}));
            std::cout << "FAIL " << name << ": " << e.what() << "\n";
        }
    };
    B fixture;
    std::unique_ptr<Monstey_Anix> base;
    test("native-name-hash", [] {
        Monstey_TestRequire(Monstey_NativeHash("LITH") == 0x2b99dbc1 &&
                                Monstey_NativeHash("bind") == 0x5cfa71ed,
                            "Native hash mismatch");
    });
    test("identity-padding-slots", [] {
        const std::array<Q, 1> q{Q{0, 0, 0, 1}};
        auto b = Monstey_AnixPackRow(q);
        auto d = Monstey_AnixUnpackRow(b, 1, true);
        Monstey_TestRequire(d.size() == 1 && std::abs(d[0][3] - 1) < 1e-6, "Identity round-trip");
        const auto bad = B(16);
        Monstey_Throws([&] { (void)Monstey_AnixUnpackRow(bad, 1, true); });
    });
    test("quaternion-error-and-sign", [] {
        std::mt19937 rng(9382);
        std::normal_distribution<float> normal;
        double worst = 0;
        for (unsigned i = 0; i < 2000; ++i) {
            Q q{};
            float n = 0;
            for (auto &x : q) {
                x = normal(rng);
                n += x * x;
            }
            n = std::sqrt(n);
            for (auto &x : q)
                x /= n;
            std::array<Q, 1> one{q};
            auto b = Monstey_AnixPackRow(one);
            auto decoded = Monstey_AnixUnpackRow(b, 1, true);
            worst = std::max(worst, Monstey_QError(q, decoded[0]));
        }
        Monstey_TestRequire(worst < 0.0015, "Quaternion error exceeds 0.086 degrees");
    });
    test("zero-and-nonfinite-quaternion-rejection", [] {
        std::array<Q, 1> zero{Q{}};
        Monstey_Throws([&] { (void)Monstey_AnixPackRow(zero); });
        zero[0][0] = std::numeric_limits<float>::infinity();
        Monstey_Throws([&] { (void)Monstey_AnixPackRow(zero); });
    });
    test("aq03-nonuniform-nlerp", [] {
        Monstey_AnixQuaternionKeys k{{0, 33, 1000},
                                     {{Q{0, 0, 0, 1}}, {Q{0, 0, 0.70710677f, 0.70710677f}}, {Q{0, 0, 1, 0}}}};
        auto b = Monstey_AnixEncodeAq03(k);
        auto d = Monstey_AnixDecodeAq03(b, 1);
        Monstey_TestRequire(d.ticks == k.ticks, "AQ times");
        const std::array<double, 3> t{-1, 33, 2000};
        auto sample = Monstey_AnixDecodeCodec("AQ03", b, 1, {}, t);
        Monstey_TestRequire(sample.size() == 3 && std::abs(sample[1][0][2].as_number() - 0.7071) < 0.001,
                            "AQ clamp/sample");
    });
    test("aq03-invalid-delta-and-padding", [] {
        Monstey_AnixQuaternionKeys k{{0, 1000}, {{Q{0, 0, 0, 1}}, {Q{0, 0, 0, 1}}}};
        auto b = Monstey_AnixEncodeAq03(k);
        Monstey_PutU16(b, 14, 0);
        Monstey_Throws([&] { (void)Monstey_AnixDecodeAq03(b, 1); });
        k.ticks = {0, 65536};
        Monstey_Throws([&] { (void)Monstey_AnixEncodeAq03(k); });
    });
    test("uatr-exact-samples", [] {
        std::vector<std::vector<V>> v{
            {V{0, 1, 2}, V{3, 4, 5}}, {V{10, 11, 12}, V{13, 14, 15}}, {V{-1, -2, -3}, V{-4, -5, -6}}};
        auto b = Monstey_AnixEncodeUatr(v);
        const std::array<std::uint32_t, 3> ticks{0, 33, 100};
        const std::array<double, 4> t{0, 33, 100, 16.5};
        auto sample = Monstey_AnixDecodeCodec("UATR", b, 2, ticks, t);
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t tr = 0; tr < 2; ++tr)
                for (std::size_t c = 0; c < 3; ++c)
                    Monstey_TestRequire(sample[i][tr][c].as_number() == v[i][tr][c], "UATR sample differs");
        Monstey_TestRequire(sample[3][0][0].as_number() == 5, "UATR interpolation differs");
    });
    test("uatr-control-and-u16-limits", [] {
        std::vector<std::vector<V>> v(257, std::vector<V>(1));
        Monstey_Throws([&] { (void)Monstey_AnixEncodeUatr(v); });
        v.resize(2);
        v[0].resize(3000);
        v[1].resize(3000);
        Monstey_Throws([&] { (void)Monstey_AnixEncodeUatr(v); });
        v.assign(2, std::vector<V>(1));
        auto b = Monstey_AnixEncodeUatr(v);
        b[4] = 255;
        const std::array<std::uint32_t, 2> ticks{0, 1};
        const std::array<double, 1> t{0};
        Monstey_Throws([&] { (void)Monstey_AnixDecodeCodec("UATR", b, 1, ticks, t); });
    });
    test("sv01-static-vector", [] {
        B b;
        Monstey_AppendU32(b, (511u << 2) | (511u << 12) | (511u << 22));
        b.resize(16);
        const std::array<double, 1> t{0};
        auto d = Monstey_AnixDecodeCodec("SV01", b, 1, {}, t);
        Monstey_TestRequire(std::abs(d[0][0][0].as_number()) < 1e-7 &&
                                std::abs(d[0][0][1].as_number()) < 1e-7 &&
                                std::abs(d[0][0][2].as_number()) < 1e-7,
                            "SV01 zero point");
    });
    test("synthetic-container-create", [&] {
        auto a = Monstey_Anix::Create(Monstey_Scene());
        fixture = a.Build();
        base = std::make_unique<Monstey_Anix>(fixture);
        Monstey_Write(out / "fixture.anix", fixture);
        Monstey_TestRequire(a.Describe()["clip_count"].as_uint() == 2, "Create clip count");
    });
    if (!base) {
        Monstey_WriteNewText(out / "anix-tests.json",
                             J::object({{"tests", results}, {"failures", failed}}).dump(2));
        return 1;
    }
    test("byte-exact-no-op", [&] {
        Monstey_TestRequire(base->Build() == fixture, "No-op changed bytes");
        Monstey_Anix copy = *base;
        Monstey_TestRequire(copy.Build() == fixture, "Copy changed bytes");
    });
    test("semantic-export-and-native-sample", [&] {
        auto scene = base->ExportClip("walk");
        Monstey_TestRequire(scene["events"].size() == 2 && scene["semantic_reimport_supported"].as_bool(),
                            "Export events");
        const std::array<double, 3> ticks{0, 500, 1000};
        auto pose = base->Sample("walk", ticks, true);
        Monstey_TestRequire(std::abs(Monstey_TestTrack(pose, Monstey_TestChannel(*base, 0x00ce66fc, 0x88baa7e7))["values"][1][0].as_number() - 10) < 1e-6,
                            "Native interpolation");
    });
    test("rename-directory-and-footer", [&] {
        auto a = *base;
        auto op = Monstey_Op("rename", "walk");
        op["name"] = "a_much_longer_native_name";
        a.Edit(J::array({op}));
        auto b = a.Build();
        Monstey_Anix check(b);
        Monstey_TestRequire(check.Describe()["clips"][1]["name"].as_string() == "a_much_longer_native_name",
                            "Rename not reflected");
        Monstey_TestRequire(a.Describe()["clips"][0]["segments"] == base->Describe()["clips"][0]["segments"],
                            "Unchanged bind payload changed");
        Monstey_Write(out / "renamed.anix", b);
    });
    test("duplicate-delete-unified-footer", [&] {
        auto a = *base;
        auto op = Monstey_Op("duplicate", "walk");
        op["name"] = "copy";
        a.Edit(J::array({op}));
        Monstey_TestRequire(a.Describe()["clip_count"].as_uint() == 3, "Duplicate missing");
        a.Edit(J::array({Monstey_Op("delete", "walk")}));
        auto d = Monstey_Anix(a.Build()).Describe();
        Monstey_TestRequire(d["clip_count"].as_uint() == 2 && d["footer_total_event_markers"].as_uint() == 2,
                            "Delete footer totals wrong");
        Monstey_Write(out / "copy-only.anix", a.Build());
    });
    test("transaction-rollback", [&] {
        auto a = *base;
        auto good = Monstey_Op("rename", "walk");
        good["name"] = "new";
        auto bad = Monstey_Op("delete", "bind");
        Monstey_Throws([&] { a.Edit(J::array({good, bad})); });
        Monstey_TestRequire(a.Build() == fixture, "Failed transaction mutated source");
    });
    test("case-folded-hash-collision", [&] {
        auto a = *base;
        auto op = Monstey_Op("duplicate", "walk");
        op["name"] = "BIND";
        Monstey_Throws([&] { a.Edit(J::array({op})); });
    });
    test("held-and-bind-pose-creation", [&] {
        auto a = *base;
        a.Edit(J::array({J::object(
            {{"op", "held"}, {"source", "bind"}, {"name", "new_bind_pose"}, {"duration_ticks", 2000}})}));
        auto s = a.ExportClip("new_bind_pose");
        Monstey_TestRequire(s["duration_ticks"].as_uint() == 2000 && s["events"].empty(), "Held metadata");
        Monstey_Write(out / "held.anix", a.Build());
    });
    test("event-edit-native-stri-footer", [&] {
        auto a = *base;
        auto op = Monstey_Op("events", "walk");
        op["events"] = J::array({J::object({{"tick", 125}, {"text", "SYNTH_AUDIO"}}),
                                 J::object({{"tick", 500}, {"text", "SYNTH_AUDIO"}})});
        a.Edit(J::array({op}));
        auto s = a.ExportClip("walk");
        Monstey_TestRequire(s["events"].size() == 2 &&
                                a.Describe()["footer_total_event_markers"].as_uint() == 2,
                            "Event totals");
        Monstey_Write(out / "events.anix", a.Build());
    });
    test("event-add-to-eventless-clip", [&] {
        auto a = *base;
        auto op = Monstey_Op("events", "bind");
        op["events"] = J::array({J::object({{"tick", 0}, {"text", "SYNTH_BIND"}})});
        a.Edit(J::array({op}));
        Monstey_TestRequire(a.ExportClip("bind")["events"].size() == 1, "New STRI missing");
    });
    test("rate-timing", [&] {
        auto a = *base;
        auto op = Monstey_Op("rate", "walk");
        op["factor"] = 2.;
        a.Edit(J::array({op}));
        auto s = a.ExportClip("walk");
        Monstey_TestRequire(s["duration_ticks"].as_uint() == 500 && s["events"][0]["tick"].as_uint() == 166,
                            "Rate scaling");
        Monstey_Write(out / "rate.anix", a.Build());
    });
    test("length-timing", [&] {
        auto a = *base;
        auto op = Monstey_Op("length", "walk");
        op["duration_ticks"] = 2000;
        a.Edit(J::array({op}));
        Monstey_TestRequire(a.ExportClip("walk")["events"][0]["tick"].as_uint() == 666, "Length scaling");
    });
    test("delay-holds-pose", [&] {
        auto a = *base;
        auto op = Monstey_Op("delay", "walk");
        op["at_tick"] = 500;
        op["duration_ticks"] = 250;
        a.Edit(J::array({op}));
        const std::array<double, 3> t{500, 625, 750};
        auto p = a.Sample("walk", t, false);
        const auto &v = p["tracks"][0]["values"];
        Monstey_TestRequire(v[0] == v[1] && v[1] == v[2], "Inserted delay is not held");
        Monstey_TestRequire(a.ExportClip("walk")["duration_ticks"].as_uint() == 1250, "Delay duration");
    });
    test("trim-roots-and-events", [&] {
        auto a = *base;
        auto op = Monstey_Op("trim", "walk");
        op["start_tick"] = 250;
        op["end_tick"] = 750;
        a.Edit(J::array({op}));
        auto s = a.ExportClip("walk");
        Monstey_TestRequire(s["duration_ticks"].as_uint() == 500 && s["events"][0]["tick"].as_uint() == 83 &&
                                std::abs(s["root_start"][0].as_number() - 5) < 1e-6,
                            "Trim semantics");
    });
    test("semantic-import", [&] {
        auto a = *base;
        auto scene = a.ExportClip("walk");
        scene["name"] = "imported";
        a.Edit(J::array({J::object({{"op", "import"}, {"clip", scene}})}));
        const std::array<double, 1> t{500};
        auto p = a.Sample("walk", t, false), q = a.Sample("imported", t, false);
        Monstey_TestRequire(p["tracks"] == q["tracks"], "Import values changed at test sample");
    });
    test("segment-splitting-u16-controls", [&] {
        auto scene = Monstey_Scene();
        auto clip = Monstey_Clip("split");
        clip["duration_ticks"] = 100000;
        J ticks = J::array();
        for (unsigned i = 0; i <= 400; ++i)
            ticks.as_array().emplace_back(i * 250);
        clip["ticks"] = ticks;
        for (auto &tr : clip["tracks"].as_array()) {
            const auto value = tr["values"][0];
            J values = J::array();
            for (unsigned i = 0; i <= 400; ++i)
                values.as_array().push_back(value);
            tr["values"] = values;
        }
        scene["clips"].as_array().push_back(clip);
        auto a = Monstey_Anix::Create(scene);
        auto d = a.Describe();
        Monstey_TestRequire(d["clips"][2]["segments"].size() >= 2, "Segment split missing");
        for (const auto &s : d["clips"][2]["segments"].as_array())
            Monstey_TestRequire(s["bytes"].as_uint() < 65536 && s["lith_samples"].as_uint() <= 256,
                                "Segment bound violated");
        Monstey_Write(out / "split.anix", a.Build());
    });
    test("large-aq03-delta-splitting", [&] {
        auto scene = Monstey_Scene();
        scene["clips"][1]["duration_ticks"] = 200000;
        scene["clips"][1]["ticks"] = J::array({0, 200000});
        scene["clips"][1]["events"] = J::array();
        auto a = Monstey_Anix::Create(scene);
        Monstey_TestRequire(a.ExportClip("walk")["ticks"].size() > 2, "Long delta not split");
    });
    test("footer-event-tag-limit", [&] {
        auto a = *base;
        J events = J::array();
        for (unsigned i = 0; i < 256; ++i)
            events.as_array().push_back(J::object({{"tick", i}, {"text", "SYNTH_" + std::to_string(i)}}));
        auto op = Monstey_Op("events", "walk");
        op["events"] = events;
        Monstey_Throws([&] { a.Edit(J::array({op})); });
        Monstey_TestRequire(a.Build() == fixture, "Event limit changed input");
    });
    test("corrupt-header-and-channel-ranges", [&] {
        for (auto offset : {4u, 8u, 12u, 24u, 28u, 32u, 36u}) {
            auto b = fixture;
            Monstey_PutU32(b, offset, 0xffffffffu);
            Monstey_Throws([&] { Monstey_Anix invalid(b); });
        }
        auto b = fixture;
        Monstey_PutU32(b, 52, 65535);
        Monstey_Throws([&] { Monstey_Anix invalid(b); });
    });
    test("corrupt-directory-and-descriptor", [&] {
        auto b = fixture;
        auto m = Monstey_U32(b, 32);
        Monstey_PutU32(b, m + 20, 0xfffffff0u);
        Monstey_Throws([&] { Monstey_Anix invalid(b); });
        b = fixture;
        auto d = Monstey_Descriptor(b, 1);
        Monstey_PutU16(b, d + 58, 65535);
        Monstey_Throws([&] { Monstey_Anix invalid(b); });
    });
    test("corrupt-footer-arena-totals", [&] {
        auto b = fixture;
        auto m = Monstey_U32(b, 32), f = m + Monstey_U32(b, m + 12);
        Monstey_PutU32(b, f + 8, 999);
        Monstey_Throws([&] { Monstey_Anix invalid(b); });
    });
    test("opaque-codec-preservation", [&] {
        auto b = fixture;
        auto d = Monstey_Descriptor(b, 1);
        auto nc = Monstey_U16(b, d + 56);
        for (std::size_t i = 0; i < nc; ++i)
            if (std::string(reinterpret_cast<char *>(b.data() + d + 124 + 4 * i), 4) == "AQ03")
                std::copy_n("ZZZZ", 4, b.begin() + d + 124 + 4 * i);
        Monstey_Anix a(b);
        Monstey_TestRequire(a.Build() == b && a.Describe()["opaque_codecs"].size() == 1,
                            "Opaque parse/no-op failed");
        auto op = Monstey_Op("rename", "walk");
        op["name"] = "opaque_renamed";
        a.Edit(J::array({op}));
        auto desc = a.Describe();
        Monstey_TestRequire(desc["clips"][1]["segments"] ==
                                Monstey_Anix(b).Describe()["clips"][1]["segments"],
                            "Opaque bytes changed");
        auto rate = Monstey_Op("rate", "opaque_renamed");
        rate["factor"] = 2;
        Monstey_Throws([&] { a.Edit(J::array({rate})); });
        Monstey_Write(out / "opaque.anix", a.Build());
    });
    test("semantic-invalid-samples", [&] {
        auto scene = Monstey_Scene();
        scene["clips"][1]["ticks"] = J::array({0, 0});
        Monstey_Throws([&] { (void)Monstey_Anix::Create(scene); });
        scene = Monstey_Scene();
        scene["clips"][1]["tracks"][0]["values"][0] = J::array({0, 0});
        Monstey_Throws([&] { (void)Monstey_Anix::Create(scene); });
    });

    test("static-segment-E-zero", [&] {
        auto raw = Monstey_StaticFixture(fixture);
        Monstey_Anix a(raw);
        Monstey_TestRequire(a.Build() == raw &&
                                a.Describe()["clips"][0]["segments"][0]["lith_samples"].as_uint() == 0,
                            "Static E=0 rejected");
        const std::array<double, 1> t{500};
        Monstey_TestRequire(a.Sample("walk", t, false)["tracks"].size() == 1, "Static sample missing");
        Monstey_Write(out / "static-E0.anix", raw);
    });
    test("opaque-extra-preserved", [&] {
        auto raw = Monstey_StaticFixture(fixture, true);
        Monstey_Anix a(raw);
        auto old = a.Describe()["clips"][0]["segments"];
        auto op = Monstey_Op("rename", "walk");
        op["name"] = "opaque_extra";
        a.Edit(J::array({op}));
        Monstey_TestRequire(a.Describe()["clips"][0]["segments"] == old, "Unknown extra changed");
        Monstey_Write(out / "opaque-extra.anix", a.Build());
    });
    test("LITH-found-by-hash-not-last", [&] {
        auto raw = Monstey_ExtraFixture(fixture);
        Monstey_Anix a(raw);
        auto d = a.Describe();
        Monstey_TestRequire(d["clips"][0]["segments"][0]["extras"].size() == 2 &&
                                d["clips"][0]["segments"][0]["lith_samples"].as_uint() == 2,
                            "LITH not found by hash");
        auto op = Monstey_Op("events", "bind");
        op["events"] = J::array({J::object({{"tick", 100}, {"text", "SYNTH_EXTRA"}})});
        a.Edit(J::array({op}));
        auto after = a.Describe()["clips"][0]["segments"][0]["extras"];
        Monstey_TestRequire(after.size() == 2, "Unknown extra lost during events");
        Monstey_Write(out / "LITH-not-last.anix", a.Build());
    });
    test("untouched-descriptor-opaque-name-padding", [&] {
        auto raw = fixture;
        const auto d = Monstey_Descriptor(raw, 0);
        raw[d + 40] = 0x71;
        Monstey_Anix a(raw);
        auto op = Monstey_Op("rename", "walk");
        op["name"] = "longer";
        a.Edit(J::array({op}));
        auto built = a.Build();
        const auto after = Monstey_Descriptor(built, 0);
        Monstey_TestRequire(built[after + 40] == 0x71, "Untouched descriptor padding was rewritten");
    });
    test("SV01-opaque-trailer", [&] {
        B b;
        Monstey_AppendU32(b, 0);
        b.insert(b.end(), {0xff, 0x13, 0x37});
        const std::array<double, 1> t{0};
        auto d = Monstey_AnixDecodeCodec("SV01", b, 1, {}, t);
        Monstey_TestRequire(d[0][0][0].as_number() == -256, "SV01 opaque trailer changed decode");
    });
    test("corrupt-STRI-string-bounds", [&] {
        B block{2, 0, 4, 0, 1, 0, 0, 0, 0, 0};
        Monstey_AppendU32(block, 0);
        Monstey_AppendU16(block, 3);
        Monstey_AppendU16(block, 4);
        const B strings{'x', 0};
        Monstey_Throws([&] { (void)Monstey_AnixDecodeCodec("STRI", block, 1, {}, {}, strings); });
    });
    test("sample-range-and-alternate-bind", [&] {
        const std::array<double, 1> t{1001};
        Monstey_Throws([&] { (void)base->Sample("walk", t, true); });
        auto raw = fixture;
        const auto d = Monstey_Descriptor(raw, 1);
        Monstey_PutU16(raw, d + 50, 1);
        Monstey_Anix a(raw);
        const std::array<double, 1> ok{10};
        Monstey_Throws([&] { (void)a.Sample("walk", ok, true); });
        Monstey_TestRequire(a.Sample("walk", ok, false)["tracks"].size() == 4,
                            "Direct-channel sample unexpectedly rejected");
    });

    test("all-255-event-tags-usable", [&] {
        auto a = *base;
        J events = J::array();
        for (unsigned i = 0; i < 255; ++i)
            events.as_array().push_back(J::object({{"tick", i}, {"text", "TAG_" + std::to_string(i)}}));
        auto op = Monstey_Op("events", "walk");
        op["events"] = events;
        a.Edit(J::array({op}));
        Monstey_TestRequire(a.Describe()["footer_total_event_markers"].as_uint() == 255,
                            "Event tags pruned incorrectly");
        Monstey_Write(out / "events-255.anix", a.Build());
    });
    test("tiny-rate-overflow-rejection", [&] {
        auto a = *base;
        auto op = Monstey_Op("rate", "walk");
        op["factor"] = 1e-300;
        Monstey_Throws([&] { a.Edit(J::array({op})); });
        Monstey_TestRequire(a.Build() == fixture, "Rate failure changed input");
    });
    test("opaque-semantic-import-refused", [&] {
        auto a = *base;
        auto scene = a.ExportClip("walk");
        scene["name"] = "partial";
        scene["complete_pose_codecs"] = false;
        scene["ignored_codecs"] = J::array({"ZZZZ"});
        Monstey_Throws([&] { a.Edit(J::array({J::object({{"op", "import"}, {"clip", scene}})})); });
    });
    test("prototype-duration-range-explicit", [&] {
        auto raw = fixture;
        auto d = Monstey_Descriptor(raw, 1);
        Monstey_PutU32(raw, d + 120, 2000);
        Monstey_Anix a(raw);
        auto s = a.ExportClip("walk");
        Monstey_TestRequire(!s["segment_range_matches_duration"].as_bool() &&
                                !s["semantic_reimport_supported"].as_bool(),
                            "Prototype range falsely qualified");
        auto rate = Monstey_Op("rate", "walk");
        rate["factor"] = 2;
        Monstey_Throws([&] { a.Edit(J::array({rate})); });
        Monstey_TestRequire(a.Build() == raw, "Prototype preservation failed");
    });
    test("UATR-next-offset-outside-buffer", [&] {
        std::vector<std::vector<V>> v(2, std::vector<V>(2));
        auto b = Monstey_AnixEncodeUatr(v);
        Monstey_PutU16(b, 2, static_cast<std::uint16_t>(b.size() + 1));
        const std::array<std::uint32_t, 2> ticks{0, 1};
        const std::array<double, 1> t{0};
        Monstey_Throws([&] { (void)Monstey_AnixDecodeCodec("UATR", b, 2, ticks, t); });
    });
    test("truncated-fixture-bounded-rejection", [&] {
        for (std::size_t n = 0; n < fixture.size(); n += 7) {
            const auto truncated = std::span<const std::uint8_t>(fixture).first(n);
            Monstey_Throws([&] { Monstey_Anix a(truncated); });
        }
    });

    test("event-removal-rebuilds-codec-directory", [&] {
        auto a = *base;
        auto op = Monstey_Op("events", "walk");
        op["events"] = J::array();
        a.Edit(J::array({op}));
        auto d = a.Describe();
        Monstey_TestRequire(a.ExportClip("walk")["events"].empty() &&
                                d["footer_total_event_markers"].as_uint() == 0,
                            "Removed events remain active");
        Monstey_Write(out / "events-cleared.anix", a.Build());
    });

    test("SKEL17-library-registration-preserves-regions", [&] {
        const auto raw = Monstey_SkelFixture();
        auto before = Monstey_AnixSkeletonInspect(raw);
        auto added = Monstey_AnixSkeletonRegister(raw, "characters/animationcharacters/synthetic/new.anix");
        auto after = Monstey_AnixSkeletonInspect(added.bytes);
        Monstey_TestRequire(before["animation_count"].as_uint() == 2 &&
                                after["animation_count"].as_uint() == 3 &&
                                added.report["changed_header_dwords"] == J::array({3, 4, 7}),
                            "SKEL counts/header edits differ");
        for (const auto *key : {"prefix_sha256", "rig_sha256", "lods_sha256", "tail_sha256"})
            Monstey_TestRequire(before[key] == after[key], "Opaque SKEL region changed");
        Monstey_Write(out / "fixture.skel", raw);
        Monstey_Write(out / "registered.skel", added.bytes);
    });
    test("SKEL17-duplicate-hash-and-traversal-refused", [&] {
        const auto raw = Monstey_SkelFixture();
        Monstey_Throws([&] {
            (void)Monstey_AnixSkeletonRegister(raw, "CHARACTERS/ANIMATIONCHARACTERS/SYNTHETIC/ONE.ANIX");
        });
        Monstey_Throws(
            [&] { (void)Monstey_AnixSkeletonRegister(raw, "characters/animationcharacters/../other.anix"); });
    });
    test("SKEL17-malformed-markers-and-pointers", [&] {
        auto raw = Monstey_SkelFixture();
        Monstey_AppendU32(raw, 0xdcdcff01);
        Monstey_Throws([&] { (void)Monstey_AnixSkeletonInspect(raw); });
        raw = Monstey_SkelFixture();
        const B marker{1, 255, 220, 220};
        auto at = std::search(raw.begin(), raw.end(), marker.begin(), marker.end());
        Monstey_TestRequire(at != raw.end(), "Fixture marker absent");
        Monstey_PutU32(raw, static_cast<std::size_t>(at - raw.begin()) + 4, 0xffffffffu);
        Monstey_Throws([&] { (void)Monstey_AnixSkeletonInspect(raw); });
    });
    test("SKEL17-other-rig-profile-read-only", [&] {
        auto raw = Monstey_SkelFixture();
        Monstey_PutU32(raw, 40, 76);
        Monstey_TestRequire(!Monstey_AnixSkeletonInspect(raw)["qualified_for_append"].as_bool(),
                            "Unknown profile falsely qualified");
        Monstey_Throws([&] {
            (void)Monstey_AnixSkeletonRegister(raw, "characters/animationcharacters/synthetic/new.anix");
        });
    });

    test("explicit-root-descriptors-preserved-by-retiming", [&] {
        const auto raw = Monstey_ExplicitRootFixture(false);
        const auto roots = Monstey_RootDescriptorBytes(raw);
        auto rate = Monstey_Op("rate", "walk");
        rate["factor"] = 1.0;
        auto length = Monstey_Op("length", "walk");
        length["duration_ticks"] = 1000;
        auto delay = Monstey_Op("delay", "walk");
        delay["at_tick"] = 500;
        delay["duration_ticks"] = 250;
        for (const auto &operation : {rate, length, delay}) {
            Monstey_Anix edited(raw);
            const auto ops = J::array({operation});
            edited.Edit(ops);
            const auto result = edited.Build();
            Monstey_TestRequire(Monstey_RootDescriptorBytes(result) == roots,
                                "Retiming changed independent native root descriptor bits");
            Monstey_Write(out / ("explicit-roots-" + operation["op"].as_string() + ".anix"), result);
            const auto persisted_operations = J::Parse(ops.dump());
            Monstey_Anix replay(raw);
            replay.Edit(persisted_operations);
            Monstey_TestRequire(replay.Build() == result,
                                "Saved operation replay changed native descriptor bytes");
        }
        Monstey_Write(out / "explicit-root-descriptors.anix", raw);
    });
    test("partial-no-Null-root-descriptors-preserved", [&] {
        const auto raw = Monstey_ExplicitRootFixture(true);
        const auto roots = Monstey_RootDescriptorBytes(raw);
        auto rate = Monstey_Op("rate", "walk");
        rate["factor"] = 1.0;
        auto length = Monstey_Op("length", "walk");
        length["duration_ticks"] = 1000;
        auto delay = Monstey_Op("delay", "walk");
        delay["at_tick"] = 250;
        delay["duration_ticks"] = 100;
        for (const auto &operation : {rate, length, delay}) {
            Monstey_Anix edited(raw);
            edited.Edit(J::array({operation}));
            Monstey_TestRequire(Monstey_RootDescriptorBytes(edited.Build()) == roots,
                                "Partial clip retiming cleared native root descriptors");
            Monstey_Write(out / ("partial-roots-" + operation["op"].as_string() + ".anix"), edited.Build());
        }
        Monstey_Write(out / "partial-root-descriptors.anix", raw);
    });
    test("explicit-root-build-reopen-no-op", [&] {
        for (const bool partial : {false, true}) {
            const auto raw = Monstey_ExplicitRootFixture(partial);
            Monstey_Anix source(raw);
            const auto saved = source.Build();
            Monstey_Anix reopened(saved);
            Monstey_Anix copied = reopened;
            Monstey_TestRequire(saved == raw && copied.Build() == raw,
                                "Unedited save/reopen changed explicit native root descriptors");
            Monstey_TestRequire(Monstey_U32(Monstey_RootDescriptorBytes(copied.Build()), 0) == 0x80000000,
                                "Negative-zero root bit was not preserved");
        }
    });
    test("trim-recalculates-independent-root-endpoints", [&] {
        const auto raw = Monstey_ExplicitRootFixture(false);
        Monstey_Anix edited(raw);
        auto trim = Monstey_Op("trim", "walk");
        trim["start_tick"] = 250;
        trim["end_tick"] = 750;
        edited.Edit(J::array({trim}));
        const auto result = edited.ExportClip("walk");
        Monstey_TestRequire(std::abs(result["root_start"][0].as_number() - 5.0) < 1e-6 &&
                                std::abs(result["root_end"][0].as_number() - 15.0) < 1e-6,
                            "Trim copied independent descriptors instead of cropped Null endpoints");
        Monstey_TestRequire(Monstey_RootDescriptorBytes(edited.Build()) != Monstey_RootDescriptorBytes(raw),
                            "Trim did not update native root descriptors");
    });
    test("trim-missing-Null-varying-roots-rejected", [&] {
        const auto raw = Monstey_ExplicitRootFixture(true);
        Monstey_Anix edited(raw);
        auto trim = Monstey_Op("trim", "walk");
        trim["start_tick"] = 250;
        trim["end_tick"] = 750;
        Monstey_Throws([&] { edited.Edit(J::array({trim})); });
        Monstey_TestRequire(edited.Build() == raw, "Unqualified missing-root crop changed the source state");
    });
    test("trim-missing-Null-constant-roots-retained", [&] {
        const auto raw = Monstey_ExplicitRootFixture(true, true);
        Monstey_Anix edited(raw);
        auto trim = Monstey_Op("trim", "walk");
        trim["start_tick"] = 250;
        trim["end_tick"] = 750;
        edited.Edit(J::array({trim}));
        Monstey_TestRequire(Monstey_RootDescriptorBytes(edited.Build()) == Monstey_RootDescriptorBytes(raw),
                            "Constant root descriptors changed during partial crop");
    });

    test("semantic-project-root-endpoints-roundtrip", [&] {
        for (const bool partial : {false, true}) {
            const auto raw = Monstey_ExplicitRootFixture(partial);
            const auto scene = Monstey_Anix(raw).Export();
            const auto persisted = J::Parse(scene.dump());
            const auto restored = Monstey_Anix::Create(persisted).Build();
            Monstey_TestRequire(Monstey_RootDescriptorBytes(restored) == Monstey_RootDescriptorBytes(raw),
                                "Semantic project save/load changed explicit root descriptor float bits");
        }
    });

    test("semantic-Event-channel-identity-roundtrip", [&] {
        auto source = Monstey_Anix::Create(Monstey_MultipleEventScene());
        const auto scene = source.ExportClip("walk");
        Monstey_TestRequire(scene["event_channel_index"].as_uint() == 2,
                            "Export selected first Event channel instead of native binding");
        auto imported = scene;
        imported["name"] = "event_roundtrip";
        source.Edit(J::array({J::object({{"op", "import"}, {"clip", imported}})}));
        Monstey_TestRequire(source.ExportClip("event_roundtrip")["event_channel_index"].as_uint() == 2,
                            "Semantic import changed Event channel identity");
        auto project = J::Parse(source.Export().dump());
        auto reopened = Monstey_Anix::Create(project);
        Monstey_TestRequire(reopened.ExportClip("walk")["event_channel_index"].as_uint() == 2,
                            "Semantic project lost Event channel identity");
        Monstey_Write(out / "multiple-event-channels.anix", reopened.Build());
    });
    test("single-nonNull-Event-channel-legacy-inference", [&] {
        auto scene = Monstey_Scene();
        scene["channels"][4]["bone_hash"] = Monstey_Hex(Monstey_NativeHash("Arm"));
        const auto source = Monstey_Anix::Create(scene);
        Monstey_TestRequire(source.ExportClip("walk")["event_channel_index"].as_uint() == 2,
                            "Unique non-Null Event channel was not retained");
        Monstey_Write(out / "single-nonNull-event.anix", source.Build());
    });
    test("ambiguous-Event-import-refused-without-selection", [&] {
        auto scene = Monstey_MultipleEventScene();
        scene["clips"][1].as_object().erase("event_channel_index");
        Monstey_Throws([&] { (void)Monstey_Anix::Create(scene); });
        auto source = Monstey_Anix::Create(Monstey_MultipleEventScene());
        auto imported = source.ExportClip("walk");
        imported["name"] = "ambiguous";
        imported.as_object().erase("event_channel_index");
        const auto before = source.Build();
        Monstey_Throws([&] { source.Edit(J::array({J::object({{"op", "import"}, {"clip", imported}})})); });
        Monstey_TestRequire(source.Build() == before, "Ambiguous import changed source");
    });
    test("explicit-Event-index-type-and-bounds-validation", [&] {
        for (const auto &bad : {J(0), J(6), J(-1), J("5")}) {
            auto scene = Monstey_MultipleEventScene();
            scene["clips"][1]["event_channel_index"] = bad;
            Monstey_Throws([&] { (void)Monstey_Anix::Create(scene); });
        }
        auto empty = Monstey_MultipleEventScene();
        empty["clips"][1]["events"] = J::array();
        empty["clips"][1]["event_channel_index"] = 0;
        Monstey_Throws([&] { (void)Monstey_Anix::Create(empty); });
    });
    test("timeline-preserves-exact-Event-channel", [&] {
        auto rate = Monstey_Op("rate", "walk");
        rate["factor"] = 1.0;
        auto length = Monstey_Op("length", "walk");
        length["duration_ticks"] = 2000;
        auto delay = Monstey_Op("delay", "walk");
        delay["at_tick"] = 500;
        delay["duration_ticks"] = 100;
        auto trim = Monstey_Op("trim", "walk");
        trim["start_tick"] = 250;
        trim["end_tick"] = 750;
        for (const auto &operation : {rate, length, delay, trim}) {
            auto source = Monstey_Anix::Create(Monstey_MultipleEventScene());
            source.Edit(J::array({operation}));
            Monstey_TestRequire(source.ExportClip("walk")["event_channel_index"].as_uint() == 2,
                                "Timeline edit rebound Event track to the first global channel");
            Monstey_Write(out / ("event-identity-" + operation["op"].as_string() + ".anix"), source.Build());
        }
    });
    test("event-edit-preserves-binding-and-explicit-rebind", [&] {
        auto source = Monstey_Anix::Create(Monstey_MultipleEventScene());
        auto operation = Monstey_Op("events", "walk");
        operation["events"] = J::array({J::object({{"tick", 100}, {"text", "SYNTH_NEW"}})});
        source.Edit(J::array({operation}));
        Monstey_TestRequire(source.ExportClip("walk")["event_channel_index"].as_uint() == 2,
                            "Event content edit changed existing channel");
        operation["event_channel_index"] = 3;
        source.Edit(J::array({operation}));
        Monstey_TestRequire(source.ExportClip("walk")["event_channel_index"].as_uint() == 3,
                            "Explicit valid Event rebind was ignored");
        Monstey_Write(out / "event-explicit-rebind.anix", source.Build());
        const auto before = source.Build();
        operation["event_channel_index"] = 0;
        Monstey_Throws([&] { source.Edit(J::array({operation})); });
        Monstey_TestRequire(source.Build() == before, "Invalid Event rebind was not atomic");
    });
    test("new-events-require-ambiguous-channel-selection", [&] {
        auto source = Monstey_Anix::Create(Monstey_MultipleEventScene());
        auto operation = Monstey_Op("events", "bind");
        operation["events"] = J::array({J::object({{"tick", 0}, {"text", "SYNTH_BIND"}})});
        const auto before = source.Build();
        Monstey_Throws([&] { source.Edit(J::array({operation})); });
        Monstey_TestRequire(source.Build() == before, "Ambiguous new Event track changed source");
        operation["event_channel_index"] = 2;
        source.Edit(J::array({operation}));
        Monstey_TestRequire(source.ExportClip("bind")["event_channel_index"].as_uint() == 2,
                            "Explicit new Event channel was not applied");
        Monstey_Write(out / "event-selected-new-track.anix", source.Build());
    });
    test("empty-STRI-retains-explicit-semantic-binding", [&] {
        auto scene = Monstey_MultipleEventScene();
        scene["clips"][1]["events"] = J::array();
        auto source = Monstey_Anix::Create(scene);
        auto exported = source.ExportClip("walk");
        Monstey_TestRequire(exported["events"].empty() && exported["event_channel_index"].as_uint() == 2,
                            "Empty STRI binding was dropped");
        auto project = J::Parse(source.Export().dump());
        auto reopened = Monstey_Anix::Create(project);
        Monstey_TestRequire(reopened.ExportClip("walk")["event_channel_index"].as_uint() == 2,
                            "Empty Event identity did not survive project import");
        auto trim = Monstey_Op("trim", "walk");
        trim["start_tick"] = 100;
        trim["end_tick"] = 900;
        reopened.Edit(J::array({trim}));
        Monstey_TestRequire(reopened.ExportClip("walk")["event_channel_index"].as_uint() == 2,
                            "Empty Event identity did not survive timeline edit");
        Monstey_Write(out / "empty-event-binding.anix", reopened.Build());
        auto remove = Monstey_Op("events", "walk");
        remove["events"] = J::array();
        reopened.Edit(J::array({remove}));
        Monstey_TestRequire(!reopened.ExportClip("walk").contains("event_channel_index"),
                            "Explicit event removal retained its binding");
    });
    test("multi-STRI-binding-opaque-until-explicit-removal", [&] {
        const auto raw = Monstey_MultipleStriTrackFixture();
        Monstey_Anix source(raw);
        Monstey_TestRequire(source.Build() == raw, "Opaque multi-STRI no-op changed bytes");
        (void)source.Describe();
        Monstey_Throws([&] { (void)source.ExportClip("walk"); });
        auto rate = Monstey_Op("rate", "walk");
        rate["factor"] = 1.0;
        Monstey_Throws([&] { source.Edit(J::array({rate})); });
        auto events = Monstey_Op("events", "walk");
        events["events"] = J::array({J::object({{"tick", 0}, {"text", "SYNTH"}})});
        Monstey_Throws([&] { source.Edit(J::array({events})); });
        Monstey_TestRequire(source.Build() == raw, "Unqualified multi-STRI edit changed source");
        events["events"] = J::array();
        source.Edit(J::array({events}));
        Monstey_TestRequire(source.ExportClip("walk")["events"].empty(),
                            "Explicit multi-STRI removal did not remove events");
    });

    test("EventInfo-ordered-readonly-metadata-and-choices", [&] {
        const auto source = Monstey_Anix::Create(Monstey_MultipleEventScene());
        const auto before = source.Build();
        const auto info = source.EventInfo("walk");
        Monstey_TestRequire(
            info["event_edit_supported"].as_bool() && info["event_channel_index"].as_uint() == 2 &&
                info["events"][0]["tick"].as_uint() == 333 && info["events"][1]["tick"].as_uint() == 667 &&
                info["event_channel_choices"].size() == 2,
            "EventInfo did not expose qualified ordered metadata");
        Monstey_TestRequire(info["event_channel_choices"][0]["index"].as_uint() == 2 &&
                                info["event_channel_choices"][0]["type_hash"].as_string() ==
                                    Monstey_Hex(0x210c8f5a) &&
                                info["event_channel_choices"][0]["bone_hash"].as_string() ==
                                    Monstey_Hex(Monstey_NativeHash("Arm")),
                            "EventInfo Event choices lost type/bone identity");
        Monstey_TestRequire(source.Build() == before, "EventInfo changed source state");
        auto reordered_raw = before;
        const auto descriptor = Monstey_Descriptor(reordered_raw, 1);
        const auto table = Monstey_SegmentTable(reordered_raw, descriptor);
        const auto segment = Monstey_U32(reordered_raw, 24) + Monstey_U32(reordered_raw, table);
        const auto stri = segment + Monstey_U16(reordered_raw, segment + 8);
        Monstey_PutU32(reordered_raw, stri + 10, 667);
        Monstey_PutU32(reordered_raw, stri + 18, 333);
        const Monstey_Anix reordered(reordered_raw);
        const auto ordered_info = reordered.EventInfo("walk");
        Monstey_TestRequire(ordered_info["events"][0]["tick"].as_uint() == 333 &&
                                ordered_info["events"][0]["text"].as_string() == "SYNTH_STOP" &&
                                ordered_info["events"][1]["tick"].as_uint() == 667 &&
                                reordered.Build() == reordered_raw,
                            "EventInfo did not order its display copy without changing native bytes");
        const auto empty = source.EventInfo("bind");
        Monstey_TestRequire(empty["events"].empty() && empty["event_channel_index"].is_null() &&
                                empty["event_edit_supported"].as_bool(),
                            "EventInfo invented an event binding for an eventless clip");
    });
    test("EventInfo-does-not-sample-invalid-pose-data", [&] {
        auto raw = Monstey_Anix::Create(Monstey_MultipleEventScene()).Build();
        const auto descriptor = Monstey_Descriptor(raw, 1);
        const auto table = Monstey_SegmentTable(raw, descriptor);
        const auto segment = Monstey_U32(raw, 24) + Monstey_U32(raw, table);
        const auto aq03 = segment + Monstey_U16(raw, segment + 6);
        std::fill(raw.begin() + aq03 + 16, raw.begin() + aq03 + 32, std::uint8_t{0});
        Monstey_Anix source(raw);
        Monstey_Throws([&] { (void)source.ExportClip("walk"); });
        const auto info = source.EventInfo("walk");
        Monstey_TestRequire(info["event_edit_supported"].as_bool() && info["events"].size() == 2 &&
                                info["event_channel_index"].as_uint() == 2,
                            "EventInfo sampled or rejected unrelated invalid pose bytes");
        Monstey_TestRequire(source.Build() == raw, "Read-only event inspection changed opaque pose data");
    });
    test("EventInfo-multi-track-binding-explicit-unsupported", [&] {
        const auto raw = Monstey_MultipleStriTrackFixture();
        const Monstey_Anix source(raw);
        const auto info = source.EventInfo("walk");
        Monstey_TestRequire(!info["event_edit_supported"].as_bool() &&
                                info["event_error"].as_string() == "unsupported_anix_event_binding" &&
                                info["event_channel_index"].is_null() && info["events"].size() == 2,
                            "Unsupported multi-track events were hidden or falsely editable");
        Monstey_TestRequire(source.Build() == raw, "Unsupported EventInfo changed bytes");
    });
    test("EventInfo-differing-segment-layout-inspectable", [&] {
        const auto raw = Monstey_DifferingStriFixture();
        Monstey_Anix source(raw);
        const auto info = source.EventInfo("walk");
        Monstey_TestRequire(!info["event_edit_supported"].as_bool() &&
                                info["event_error"].as_string() == "unsupported_anix_event_layout" &&
                                info["event_channel_index"].as_uint() == 2 && info.contains("event_message"),
                            "Differing STRI layout did not retain explicit inspection status");
        auto operation = Monstey_Op("events", "walk");
        operation["events"] = J::array({J::object({{"tick", 0}, {"text", "SYNTH"}})});
        Monstey_Throws([&] { source.Edit(J::array({operation})); });
        Monstey_TestRequire(source.Build() == raw, "Unsupported event edit changed source");
        operation["events"] = J::array();
        source.Edit(J::array({operation}));
        Monstey_TestRequire(source.EventInfo("walk")["events"].empty(),
                            "Explicit unsupported-layout event removal failed");
    });
    test("EventInfo-missing-shared-Event-channel", [&] {
        auto scene = Monstey_Scene();
        scene["channels"].as_array().pop_back();
        scene["clips"][1]["events"] = J::array();
        const auto source = Monstey_Anix::Create(scene);
        const auto info = source.EventInfo("walk");
        Monstey_TestRequire(!info["event_edit_supported"].as_bool() &&
                                info["event_error"].as_string() == "missing_anix_event_channel" &&
                                info["event_channel_choices"].empty() && info["events"].empty() &&
                                info["event_channel_index"].is_null(),
                            "EventInfo falsely enabled editing without an eligible shared channel");
    });

    test("CAF4-native-scalar-layout-and-goldens", [&] {
        const auto block = Monstey_TestCaf4Block();
        const std::array<double, 6> ticks{-10, 0, 100, 200, 600, 2000};
        const auto sampled = Monstey_AnixDecodeCodec("CAF4", block, 2, {}, ticks);
        const std::array<std::array<double, 2>, 6> expected{
            {{{0, 100}}, {{0, 100}}, {{5, 150}}, {{10, 200}}, {{20, 300}}, {{30, 400}}}};
        for (std::size_t i = 0; i < expected.size(); ++i)
            for (std::size_t track = 0; track < 2; ++track)
                Monstey_TestRequire(sampled[i][track].as_number() == expected[i][track],
                                    "CAF4 differs from hand-computed native scalar interpolation");
        const std::array<std::uint32_t, 2> unrelated_lith{123, 456};
        Monstey_TestRequire(Monstey_AnixDecodeCodec("CAF4", block, 2, unrelated_lith, ticks) == sampled,
                            "CAF4 incorrectly used external LITH keys");
    });
    test("CAF4-unknown-mode-is-explicit-not-empty", [&] {
        auto block = Monstey_TestCaf4Block();
        const std::array<double, 1> tick{0};
        for (auto mode : {0u, 2u, 0xffffffffu}) {
            Monstey_PutU32(block, 0, mode);
            bool rejected = false;
            try {
                (void)Monstey_AnixDecodeCodec("CAF4", block, 2, {}, tick);
            } catch (const Monstey_Error &error) {
                rejected = error.code == "unsupported_anix_codec_variant";
            }
            Monstey_TestRequire(rejected, "Unsupported CAF4 mode was silently treated as empty");
        }
    });
    test("CAF4-malformed-times-stride-and-rows", [&] {
        const auto good = Monstey_TestCaf4Block();
        const std::array<double, 1> tick{100};
        for (const auto &[offset, value] : std::array<std::pair<std::size_t, std::uint32_t>, 5>{
                 {{12, 1}, {12, 0xffffffffu}, {16, 1}, {16, 0xffffffffu}, {8, 999}}}) {
            auto bad = good;
            Monstey_PutU32(bad, offset, value);
            Monstey_Throws([&] { (void)Monstey_AnixDecodeCodec("CAF4", bad, 2, {}, tick); });
        }
        for (auto offset : {20u, 22u}) {
            auto bad = good;
            Monstey_PutU16(bad, offset, offset == 20 ? 1 : 0);
            Monstey_Throws([&] { (void)Monstey_AnixDecodeCodec("CAF4", bad, 2, {}, tick); });
        }
        auto truncated = good;
        truncated.resize(67);
        Monstey_Throws([&] { (void)Monstey_AnixDecodeCodec("CAF4", truncated, 2, {}, tick); });
        auto nonfinite = good;
        Monstey_PutU32(nonfinite, 32, 0x7fc01234);
        Monstey_Throws([&] { (void)Monstey_AnixDecodeCodec("CAF4", nonfinite, 2, {}, tick); });
        const std::array<double, 1> nan{std::numeric_limits<double>::quiet_NaN()};
        Monstey_Throws([&] { (void)Monstey_AnixDecodeCodec("CAF4", good, 2, {}, nan); });
    });
    test("USFT-offset-table-raw-words-and-nullable-float-view", [&] {
        B block;
        for (auto offset : {13u, 8u, 13u})
            Monstey_AppendU16(block, static_cast<std::uint16_t>(offset));
        block.resize(17, 0x37);
        Monstey_PutU32(block, 8, 0x80000000);
        Monstey_PutU32(block, 13, 0x7fc01234);
        const std::array<double, 2> ticks{0, 1000};
        const auto values = Monstey_AnixDecodeCodec("USFT", block, 3, {}, ticks);
        Monstey_TestRequire(values[0] == values[1] && values[0][0]["raw_u32"].as_uint() == 0x7fc01234 &&
                                values[0][2] == values[0][0] && values[0][0]["float32"].is_null() &&
                                values[0][1]["raw_u32"].as_uint() == 0x80000000 &&
                                std::signbit(values[0][1]["float32"].as_number()),
                            "USFT lost aliased/out-of-order raw words, NaN payload or negative zero");
    });
    test("USBT-offset-table-copies-bytes-without-bool-normalization", [&] {
        B block;
        for (auto offset : {10u, 8u, 9u, 10u})
            Monstey_AppendU16(block, static_cast<std::uint16_t>(offset));
        block.insert(block.end(), {0, 2, 255});
        const std::array<double, 2> ticks{0, 1000};
        const auto values = Monstey_AnixDecodeCodec("USBT", block, 4, {}, ticks);
        Monstey_TestRequire(values[0] == J::array({255, 0, 2, 255}) && values[1] == values[0],
                            "USBT normalized or widened a native byte copy");
    });
    test("static-auxiliary-offset-corruption-rejected", [&] {
        const std::array<double, 1> tick{0};
        auto word = Monstey_TestStaticWords();
        Monstey_PutU16(word, 0, static_cast<std::uint16_t>(word.size() - 3));
        Monstey_Throws([&] { (void)Monstey_AnixDecodeCodec("USFT", word, 3, {}, tick); });
        B truncated{0, 0, 0};
        Monstey_Throws([&] { (void)Monstey_AnixDecodeCodec("USFT", truncated, 2, {}, tick); });
        auto byte = Monstey_TestStaticByte();
        Monstey_PutU16(byte, 0, static_cast<std::uint16_t>(byte.size()));
        Monstey_Throws([&] { (void)Monstey_AnixDecodeCodec("USBT", byte, 1, {}, tick); });
    });
    test("auxiliary-container-sampling-kind-bits-and-bind-overlay", [&] {
        const auto raw = Monstey_RareFixture();
        const Monstey_Anix source(raw);
        const auto metadata = source.Describe();
        Monstey_TestRequire(source.Build() == raw && metadata["qualified_codec_blocks"].as_uint() == 3 &&
                                metadata["auxiliary_codec_blocks"].as_uint() == 3 &&
                                metadata["auxiliary_authoring_supported"].as_bool() &&
                                metadata["unsupported_authoring_codecs"].empty() &&
                                metadata["opaque_codecs"].empty(),
                            "Qualified auxiliary blocks remained opaque or changed bytes");
        const std::array<double, 2> ticks{100, 600};
        const auto direct = source.Sample("walk", ticks, false);
        Monstey_TestRequire(direct["complete_channel_decode"].as_bool() && direct["tracks"].size() == 2 &&
                                direct["tracks"][0]["values"] == J::array({5.0, 20.0}) &&
                                direct["tracks"][0]["native_value_kind"].as_string() == "float32" &&
                                direct["tracks"][0]["native_role"].as_string() == "auxiliary",
                            "CAF4 auxiliary identity or sampled values were lost");
        const auto fallback = source.Sample("walk", ticks, true);
        Monstey_TestRequire(fallback["tracks"].size() == 4, "Auxiliary bind channels were not overlaid");
        const auto &byte = fallback["tracks"][2];
        const auto &word = fallback["tracks"][3];
        Monstey_TestRequire(byte["channel_index"].as_uint() == 7 &&
                                byte["native_value_kind"].as_string() == "uint8" &&
                                byte["values"] == J::array({255, 255}) &&
                                word["native_value_kind"].as_string() == "word32_with_float32_view" &&
                                word["values"][0]["raw_u32"].as_uint() == 0x42280000,
                            "Fallback auxiliary primitive width or raw bits changed");
        Monstey_Write(out / "auxiliary-native.anix", raw);
    });
    test("auxiliary-raw-nonfinite-word-preserved-with-explicit-view-status", [&] {
        const auto raw = Monstey_RareFixture(true);
        const Monstey_Anix source(raw);
        const auto metadata = source.Describe();
        Monstey_TestRequire(!metadata["raw_word_float_views_finite"].as_bool() &&
                                !metadata["qualified_codecs_finite"].as_bool() && source.Build() == raw,
                            "USFT raw NaN word was discarded or falsely described as a finite float");
        const std::array<double, 1> tick{0};
        const auto sampled = source.Sample("bind", tick, false);
        const auto &track = sampled["tracks"][3];
        Monstey_TestRequire(track["values"][0]["raw_u32"].as_uint() == 0x7fc01234 &&
                                track["values"][0]["float32"].is_null(),
                            "USFT nonfinite float view lost its original payload bits");
    });
    test("auxiliary-export-import-and-timeline-retain-qualified-scalars", [&] {
        const auto raw = Monstey_RareFixture();
        Monstey_Anix source(raw);
        auto scene = source.ExportClip("walk");
        Monstey_TestRequire(
            scene["complete_channel_decode"].as_bool() && scene["semantic_reimport_supported"].as_bool() &&
                scene["auxiliary_authoring_supported"].as_bool() &&
                scene["unsupported_authoring_codecs"].empty() && scene["tracks"].size() == 2,
            "Qualified auxiliary semantic export lost channels or capability");
        scene["name"] = "qualified";
        source.Edit(J::array({J::object({{"op", "import"}, {"clip", scene}})}));
        const std::array<double, 3> ticks{0, 600, 1000}, mapped{0, 300, 500};
        const auto expected = source.Sample("walk", ticks, false);
        const auto actual = source.Sample("qualified", ticks, false);
        Monstey_TestRequire(expected["tracks"] == actual["tracks"], "Auxiliary import changed scalar samples");
        auto rate = Monstey_Op("rate", "qualified");
        rate["factor"] = 2.0;
        source.Edit(J::array({rate}));
        const auto retimed = source.Sample("qualified", mapped, false);
        Monstey_TestRequire(expected["tracks"] == retimed["tracks"], "Auxiliary timeline changed scalar samples");
    });
    test("CAF4-alignment-and-event-rebuild-preserve-compressed-bytes", [&] {
        const auto raw = Monstey_RareFixture();
        Monstey_Anix source(raw);
        const auto before = source.Describe()["clips"][1]["segments"][0]["blocks"][0]["sha256"];
        auto event = Monstey_Op("events", "walk");
        event["events"] = J::array({J::object({{"tick", 100}, {"text", "SYNTH_AUX_EVENT"}})});
        source.Edit(J::array({event}));
        const auto built = source.Build();
        const auto descriptor = Monstey_Descriptor(built, 1);
        const auto table = Monstey_SegmentTable(built, descriptor);
        const auto segment = Monstey_U32(built, 24) + Monstey_U32(built, table);
        Monstey_TestRequire(Monstey_U16(built, segment + 4) % 16 == 0 &&
                                source.Describe()["clips"][1]["segments"][0]["blocks"][0]["sha256"] == before,
                            "Event rebuild misaligned or rewrote native CAF4 bytes");
        auto misaligned = raw;
        const auto original_descriptor = Monstey_Descriptor(misaligned, 1);
        const auto original_table = Monstey_SegmentTable(misaligned, original_descriptor);
        const auto original_segment = Monstey_U32(misaligned, 24) + Monstey_U32(misaligned, original_table);
        Monstey_PutU16(misaligned, original_segment + 4, 20);
        Monstey_PutU32(misaligned, original_segment + 20, 1); // Proven mode1 requires16 alignment.
        Monstey_Throws([&] { Monstey_Anix invalid(misaligned); });
        Monstey_Write(out / "auxiliary-event-rebuild.anix", built);
    });

    J native = J::array();
    for (int ai = 2; ai < argc; ++ai) {
        const std::filesystem::path path = argv[ai];
        const auto label = "native-" + std::to_string(ai - 1);

        if (path.extension() == L".skel") {
            test((label + "-SKEL17-actual-registration").c_str(), [&] {
                const auto raw = Monstey_ReadFile(path);
                const auto before = Monstey_AnixSkeletonInspect(raw);
                auto added = Monstey_AnixSkeletonRegister(
                    raw, "characters/animationcharacters/monstey/talion/nazgul_confrontation.anix");
                const auto after = Monstey_AnixSkeletonInspect(added.bytes);
                Monstey_TestRequire(after["animation_count"].as_uint() ==
                                        before["animation_count"].as_uint() + 1,
                                    "Actual SKEL count differs");
                if (Monstey_HashSha256(raw) ==
                    "5d8f05d1f2dea3152e04a42a8749eec23cb5263fe3934ef57e271bc4c5d2ac67")
                    Monstey_TestRequire(
                        Monstey_HashSha256(added.bytes) ==
                            "43806c4bd1d6cb3372db8e3725ec5689b2b5a34b2bdc7792f1bced8cbe3cadb8",
                        "Actual SKEL differs from prior qualified preload candidate");
                Monstey_TestRequire(Monstey_HashSha256(path) == Monstey_HashSha256(raw),
                                    "Actual source SKEL changed");
                Monstey_Write(out / (label + "-registered.skel"), added.bytes);
                Monstey_WriteNewText(out / (label + "-SKEL-proof.json"), added.report.dump(2));
                native.as_array().push_back(added.report);
            });
            continue;
        }

        test((label + "-bounded-parse-codec-arena").c_str(), [&] {
            const auto raw = Monstey_ReadFile(path);
            Monstey_Anix a(raw);
            auto d = a.Describe();
            Monstey_TestRequire(a.Build() == raw, "Native no-op changed");
            Monstey_WriteNewText(out / (label + "-describe.json"), d.dump(2));
            native.as_array().push_back(J::object({{"path", Monstey_Utf8(path.wstring())},
                                                   {"sha256", Monstey_HashSha256(raw)},
                                                   {"clips", d["clip_count"]},
                                                   {"blocks", d["codec_blocks"]},
                                                   {"footer_arena_bytes", d["footer_arena_bytes"]},
                                                   {"no_op_byte_exact", true}}));
            std::uint64_t event_metadata_clips = 0;
            for (const auto &clip : d["clips"].as_array()) {
                const auto info = a.EventInfo(clip["name"].as_string());
                Monstey_TestRequire(info["event_edit_supported"].as_bool(),
                                    "Actual native Event metadata was not qualified");
                const auto &codecs = clip["codecs"].as_array();
                const bool has_stri = std::any_of(codecs.begin(), codecs.end(), [](const auto &code) {
                    return code.as_string() == "STRI";
                });
                Monstey_TestRequire(has_stri != info["event_channel_index"].is_null(),
                                    "Actual native Event binding metadata differs from its codec tracks");
                ++event_metadata_clips;
            }
            native.as_array().back()["event_metadata_clips_qualified"] = event_metadata_clips;
            const auto original = d;
            std::string source;
            for (const auto &c : d["clips"].as_array())
                if (c["name"].as_string() != "bind") {
                    source = c["name"].as_string();
                    break;
                }
            Monstey_TestRequire(!source.empty(), "Native lacks editable clip");
            auto dup = Monstey_Op("duplicate", source);
            dup["name"] = "Monstey_Native_Offline_Duplicate";
            a.Edit(J::array({dup}));
            auto del = Monstey_Op("delete", source);
            a.Edit(J::array({del}));
            auto after = Monstey_Anix(a.Build()).Describe();
            Monstey_TestRequire(after["clip_count"] == original["clip_count"],
                                "Native directory/footer mismatch");
            for (const auto &c : original["clips"].as_array())
                if (c["name"].as_string() != source) {
                    auto it = std::find_if(after["clips"].as_array().begin(), after["clips"].as_array().end(),
                                           [&](const auto &n) { return n["name"] == c["name"]; });
                    Monstey_TestRequire(it != after["clips"].as_array().end() &&
                                            (*it)["segments"] == c["segments"],
                                        "Unchanged native segment differs");
                }
            Monstey_Write(out / (label + "-edited.anix"), a.Build());
        });

        test((label + "-semantic-pose-event-timeline").c_str(), [&] {
            const auto raw = Monstey_ReadFile(path);
            Monstey_Anix a(raw);
            auto d = a.Describe();
            std::string source;
            for (const auto &clip : d["clips"].as_array())
                if (clip["name"].as_string() != "bind" && clip["duration_ticks"].as_uint() > 100 &&
                    clip["segments"][0]["first_tick"].as_uint() == 0 &&
                    clip["segments"].as_array().back()["last_tick"] == clip["duration_ticks"]) {
                    source = clip["name"].as_string();
                    break;
                }
            auto semantic = a.ExportClip(source);
            Monstey_TestRequire(semantic["semantic_reimport_supported"].as_bool(),
                                "Native semantic export incomplete");
            semantic["name"] = "Monstey_Native_Semantic_Import";
            a.Edit(J::array({J::object({{"op", "import"}, {"clip", semantic}})}));
            const auto duration = semantic["duration_ticks"].as_number();
            const std::array<double, 3> times{0, duration / 2, duration};
            const auto original_pose = a.Sample(source, times, false),
                       imported_pose = a.Sample("Monstey_Native_Semantic_Import", times, false);
            Monstey_TestRequire(original_pose["tracks"].size() == imported_pose["tracks"].size(),
                                "Native imported channels differ");
            double max_vector = 0, max_quat = 0;
            for (std::size_t ti = 0; ti < original_pose["tracks"].size(); ++ti) {
                const auto &ov = original_pose["tracks"][ti]["values"];
                const auto &nv = imported_pose["tracks"][ti]["values"];
                for (std::size_t k = 0; k < ov.size(); ++k) {
                    double sign = 1;
                    if (ov[k].size() == 4) {
                        double dot = 0;
                        for (std::size_t c = 0; c < 4; ++c)
                            dot += ov[k][c].as_number() * nv[k][c].as_number();
                        if (dot < 0)
                            sign = -1;
                    }
                    for (std::size_t c = 0; c < ov[k].size(); ++c) {
                        const auto error = std::abs(ov[k][c].as_number() - nv[k][c].as_number() * sign);
                        if (ov[k].size() == 4)
                            max_quat = std::max(max_quat, error);
                        else
                            max_vector = std::max(max_vector, error);
                    }
                }
            }
            Monstey_TestRequire(max_vector < 0.001 && max_quat < 0.005,
                                "Native semantic encoding tolerance exceeded");
            auto events = Monstey_Op("events", source);
            events["events"] = J::array(
                {J::object({{"tick", 0}, {"text", "SYNTH_NATIVE_AUDIO"}}),
                 J::object({{"tick", static_cast<std::uint32_t>(duration)}, {"text", "SYNTH_NATIVE_END"}})});
            a.Edit(J::array({events}));
            Monstey_TestRequire(a.ExportClip(source)["events"].size() == 2, "Actual event edit failed");
            auto rate = Monstey_Op("rate", source);
            rate["factor"] = 0.5;
            a.Edit(J::array({rate}));
            auto trim = Monstey_Op("trim", source);
            trim["start_tick"] = 100;
            trim["end_tick"] = static_cast<std::uint32_t>(duration * 2) - 100;
            a.Edit(J::array({trim}));
            Monstey_Write(out / (label + "-semantic-edited.anix"), a.Build());
            Monstey_WriteNewText(out / (label + "-semantic-proof.json"),
                                 J::object({{"source_clip", source},
                                            {"max_position_axis_error_cm", max_vector},
                                            {"max_quaternion_component_error", max_quat},
                                            {"sample_count", 3},
                                            {"events_edited", true},
                                            {"rate_and_trim_rebuilt", true},
                                            {"runtime_verified", false}})
                                     .dump(2));
        });

        if (path.extension() == L".anix" &&
            Monstey_HashSha256(path) == "89a3cea2bb003ede8f723855686baf9d8eb601321745ada6563fa3ad1da822a5") {
            test((label + "-all578-blocks-all30-clips-and-auxiliary-bind").c_str(), [&] {
                const auto raw = Monstey_ReadFile(path);
                const Monstey_Anix source(raw);
                const auto metadata = source.Describe();
                Monstey_TestRequire(metadata["qualified_codec_blocks"].as_uint() == 578 &&
                                        metadata["opaque_codec_blocks"].as_uint() == 0 &&
                                        metadata["auxiliary_codec_blocks"].as_uint() == 11 &&
                                        metadata["opaque_codecs"].empty() &&
                                        metadata["qualified_codecs_finite"].as_bool(),
                                    "Actual rare blocks remained unqualified");
                std::size_t sampled_clips = 0;
                for (const auto &clip : metadata["clips"].as_array()) {
                    std::set<double> distinct;
                    for (const auto &segment : clip["segments"].as_array()) {
                        const auto first = segment["first_tick"].as_uint();
                        const auto last = segment["last_tick"].as_uint();
                        distinct.insert(static_cast<double>(first));
                        distinct.insert(static_cast<double>(first + (last - first) / 2));
                        distinct.insert(static_cast<double>(last));
                    }
                    const std::vector<double> ticks(distinct.begin(), distinct.end());
                    const auto sampled = source.Sample(clip["name"].as_string(), ticks, false);
                    Monstey_TestRequire(sampled["ignored_codecs"].empty() &&
                                            sampled["complete_channel_decode"].as_bool(),
                                        "Actual clip sampling silently omitted an auxiliary codec");
                    ++sampled_clips;
                }
                const std::array<double, 3> ticks{0, 5000, 10666};
                const auto combined = source.Sample("OV_GetUp_Inf_A", ticks, true);
                std::size_t scalar = 0, word = 0, byte = 0;
                for (const auto &track : combined["tracks"].as_array())
                    if (track.contains("native_role")) {
                        const auto &kind = track["native_value_kind"].as_string();
                        scalar += kind == "float32";
                        word += kind == "word32_with_float32_view";
                        byte += kind == "uint8";
                        Monstey_TestRequire(track["native_bits"].size() == ticks.size() &&
                                                track["present_at_all_samples"].as_bool(),
                                            "Actual auxiliary sample lost native bit mirrors or coverage");
                    }
                Monstey_TestRequire(sampled_clips == 30 && scalar == 4 && word == 28 && byte == 8,
                                    "Native bind overlay did not retain all40 auxiliary channels");
                Monstey_TestRequire(Monstey_HashSha256(path) == Monstey_HashSha256(raw) &&
                                        source.Build() == raw,
                                    "Rare decoder qualification changed actual source bytes");
                Monstey_WriteNewText(out / "actual-all578-rare-proof.json",
                                     J::object({{"qualified_blocks", 578},
                                                {"opaque_blocks", 0},
                                                {"auxiliary_blocks", 11},
                                                {"sampled_clips", 30},
                                                {"fallback_CAF4_tracks", 4},
                                                {"fallback_USFT_tracks", 28},
                                                {"fallback_USBT_tracks", 8},
                                                {"source_unchanged", true},
                                                {"no_op_byte_exact", true},
                                                {"runtime_verified", false}})
                                         .dump(2));
            });
            test((label + "-actual-CAF4-event-rebuild-preserves-native-alignment").c_str(), [&] {
                const auto raw = Monstey_ReadFile(path);
                Monstey_Anix source(raw);
                const auto before = source.Describe();
                const auto before_clip = std::find_if(
                    before["clips"].as_array().begin(), before["clips"].as_array().end(),
                    [](const auto &clip) { return clip["name"].as_string() == "OV_GetUp_Inf_A"; });
                Monstey_TestRequire(before_clip != before["clips"].as_array().end(),
                                    "Actual CAF4 fixture clip is absent");
                std::vector<J> hashes;
                for (const auto &segment : (*before_clip)["segments"].as_array())
                    for (const auto &block : segment["blocks"].as_array())
                        if (block["codec"].as_string() == "CAF4")
                            hashes.push_back(block["sha256"]);
                auto operation = Monstey_Op("events", "OV_GetUp_Inf_A");
                operation["events"] = J::array({J::object({{"tick", 100}, {"text", "SYNTH_RARE_EVENT"}})});
                source.Edit(J::array({operation}));
                const auto after = source.Describe();
                const auto after_clip = std::find_if(
                    after["clips"].as_array().begin(), after["clips"].as_array().end(),
                    [](const auto &clip) { return clip["name"].as_string() == "OV_GetUp_Inf_A"; });
                std::size_t index = 0;
                for (const auto &segment : (*after_clip)["segments"].as_array())
                    for (const auto &block : segment["blocks"].as_array())
                        if (block["codec"].as_string() == "CAF4")
                            Monstey_TestRequire(index < hashes.size() && block["sha256"] == hashes[index++],
                                                "Actual event rebuild changed a CAF4 compressed block");
                Monstey_TestRequire(index == 3 && after["qualified_codec_blocks"].as_uint() == 578 &&
                                        Monstey_HashSha256(path) == Monstey_HashSha256(raw),
                                    "Actual rare event rebuild lost a segment or changed its source");
                Monstey_Write(out / "actual-rare-event-rebuild.anix", source.Build());
            });
        }
    }
    J proof = J::object({{"status", failed ? "FAIL" : "PASS"},
                         {"tests", results},
                         {"test_count", static_cast<std::uint64_t>(results.size())},
                         {"failures", failed},
                         {"native", native},
                         {"output_directory", Monstey_Utf8(out.wstring())},
                         {"original_program_executed", false},
                         {"game_written", false},
                         {"runtime_verified", false}});
    Monstey_WriteNewText(out / "anix-tests.json", proof.dump(2));
    std::cout << proof.dump(2) << "\n";
    return failed ? 1 : 0;
}
