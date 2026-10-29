#include "Monstey_Anix.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <functional>
#include <iostream>
#include <set>
#include <utility>

using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
using Q = Monstey_AnixQuaternion;
using V = Monstey_AnixVector;
constexpr const char *Monstey_EmptyError = "unsupported_anix_empty_clip";

void Monstey_TestRequire(bool value, std::string_view message) {
    if (!value)
        throw std::runtime_error(std::string(message));
}
void Monstey_Throws(const std::function<void()> &function, std::string_view code = {}) {
    try {
        function();
    } catch (const Monstey_Error &error) {
        Monstey_TestRequire(code.empty() || error.code == code,
                            "Wrong validation error: " + error.code + ": " + error.what());
        return;
    }
    throw std::runtime_error("Expected Monstey_Error was not thrown");
}
std::size_t Monstey_Align(std::size_t value, std::size_t alignment = 4) {
    return (value + alignment - 1) / alignment * alignment;
}
std::size_t Monstey_DescriptorAt(const B &bytes, std::size_t index) {
    const auto metadata = Monstey_U32(bytes, 32);
    return metadata + Monstey_U32(bytes, metadata + 20 + 8 * index);
}
std::string Monstey_DescriptorName(const B &bytes, std::size_t at) {
    std::string name;
    for (std::size_t i = at; i < at + 48 && bytes.at(i); ++i)
        name.push_back(static_cast<char>(bytes.at(i)));
    return name;
}
std::size_t Monstey_FindIndex(const B &bytes, std::string_view name) {
    const auto metadata = Monstey_U32(bytes, 32), count = Monstey_U32(bytes, metadata + 4);
    for (std::size_t i = 0; i < count; ++i)
        if (Monstey_DescriptorName(bytes, Monstey_DescriptorAt(bytes, i)) == name)
            return i;
    throw std::runtime_error("Missing native descriptor " + std::string(name));
}
B Monstey_DescriptorBytes(const B &bytes, std::string_view name, bool clear_name = false) {
    const auto metadata = Monstey_U32(bytes, 32), count = Monstey_U32(bytes, metadata + 4);
    const auto at = Monstey_DescriptorAt(bytes, Monstey_FindIndex(bytes, name));
    std::size_t end = metadata + Monstey_U32(bytes, metadata + 12);
    for (std::size_t i = 0; i < count; ++i) {
        const auto next = Monstey_DescriptorAt(bytes, i);
        if (next > at)
            end = std::min(end, next);
    }
    B descriptor(bytes.begin() + at, bytes.begin() + end);
    if (clear_name)
        std::fill_n(descriptor.begin(), 48, std::uint8_t{0});
    return descriptor;
}
// The native record layout is independently checked, with only the relocated
// string-table name offset excluded from comparisons after structural edits.
B Monstey_FooterRecord(const B &bytes, std::size_t wanted) {
    const auto metadata = Monstey_U32(bytes, 32);
    const auto footer = metadata + Monstey_U32(bytes, metadata + 12);
    const std::size_t body = footer + 24;
    std::size_t cursor = body + Monstey_U32(bytes, footer) + 41;
    for (std::size_t index = 0; index <= wanted; ++index) {
        const auto markers = Monstey_U32(bytes, cursor + 36);
        const std::size_t size = 40 + 8ull * markers;
        Monstey_CheckRange(bytes, cursor, size);
        if (index == wanted) {
            B record(bytes.begin() + cursor, bytes.begin() + cursor + size);
            Monstey_PutU32(record, 12, 0);
            return record;
        }
        cursor += size;
    }
    throw std::runtime_error("Missing native footer record");
}
const J &Monstey_FindClip(const J &description, std::string_view name) {
    for (const auto &clip : description["clips"].as_array())
        if (clip["name"].as_string() == name)
            return clip;
    throw std::runtime_error("Missing described clip " + std::string(name));
}
const J &Monstey_TrackValue(const J &sample, unsigned channel, std::size_t frame = 0) {
    for (const auto &track : sample["tracks"].as_array())
        if (track["channel_index"].as_uint() == channel)
            return track["values"][frame];
    throw std::runtime_error("Missing sampled channel " + std::to_string(channel));
}
void Monstey_Near(const J &value, std::span<const double> expected, double tolerance) {
    Monstey_TestRequire(value.size() == expected.size(), "Unexpected sampled value width");
    for (std::size_t i = 0; i < expected.size(); ++i)
        Monstey_TestRequire(std::abs(value[i].as_number() - expected[i]) <= tolerance,
                            "Sampled value differs at component " + std::to_string(i));
}
J Monstey_Op(std::string_view operation, std::string_view name) {
    return J::object({{"op", operation}, {"clip", name}});
}
void Monstey_RequireEmptyExport(const J &clip) {
    Monstey_TestRequire(clip["empty_native_clip"].as_bool(), "Missing empty export metadata");
    Monstey_TestRequire(clip["structural_only"].as_bool(), "Empty export is not structural-only");
    Monstey_TestRequire(clip["ticks"].is_array() && clip["ticks"].empty() &&
                            clip["tracks"].is_array() && clip["tracks"].empty(),
                        "Empty export invented a timeline or pose track");
    Monstey_TestRequire(!clip["semantic_reimport_supported"].as_bool() &&
                            clip["semantic_reimport_error"]["code"].as_string() == Monstey_EmptyError,
                        "Empty export falsely promises semantic reimport");
}
void Monstey_RequireRollback(const B &raw, const J &bad_operation) {
    Monstey_Anix library(raw);
    const auto before = library.Describe();
    auto good = Monstey_Op("duplicate", "bind");
    good["name"] = "Monstey_Transaction_Probe";
    Monstey_Throws([&] { (void)library.Edit(J::array({good, bad_operation})); }, Monstey_EmptyError);
    Monstey_TestRequire(library.Build() == raw && library.Describe() == before,
                        "Rejected empty operation committed a partial edit");
}
void Monstey_CheckEmptySemantics(const B &raw, std::string_view name, unsigned duration) {
    const Monstey_Anix library(raw);
    const std::array<double, 1> tick{0};
    const auto event_info = library.EventInfo(name);
    Monstey_TestRequire(event_info["empty_native_clip"].as_bool() && !event_info["event_edit_supported"].as_bool() &&
                            event_info["event_error"].as_string() == Monstey_EmptyError,
                        "Empty EventInfo falsely promises event authoring");
    Monstey_Throws([&] { (void)library.Sample(name, tick, false); }, Monstey_EmptyError);
    Monstey_Throws([&] { (void)library.Sample(name, tick, true); }, Monstey_EmptyError);
    Monstey_Throws([&] { (void)library.Sample(name, {}, false); }, Monstey_EmptyError);
    for (bool explicit_tick : {false, true}) {
        auto held = J::object({{"op", "held"}, {"source", name},
                              {"name", "Monstey_Empty_Held"}, {"duration_ticks", 1000}});
        if (explicit_tick)
            held["tick"] = 0;
        Monstey_RequireRollback(raw, held);
    }
    auto rate = Monstey_Op("rate", name);
    rate["factor"] = 2.0;
    auto length = Monstey_Op("length", name);
    length["duration_ticks"] = duration + 1;
    auto delay = Monstey_Op("delay", name);
    delay["at_tick"] = 0;
    delay["duration_ticks"] = 1;
    auto trim = Monstey_Op("trim", name);
    trim["start_tick"] = 0;
    trim["end_tick"] = duration;
    auto events = Monstey_Op("events", name);
    events["events"] = J::array();
    for (const auto &operation : {rate, length, delay, trim, events})
        Monstey_RequireRollback(raw, operation);
    auto populated_events = events;
    populated_events["events"] = J::array({J::object({{"tick", 0}, {"text", "Monstey_Empty_Event"}})});
    Monstey_RequireRollback(raw, populated_events);
    Monstey_TestRequire(library.Build() == raw, "Read-only empty API changed native bytes");
}
void Monstey_CheckEmptyStructure(const B &raw, std::string_view name) {
    const auto descriptor = Monstey_DescriptorBytes(raw, name, true);
    const auto footer = Monstey_FooterRecord(raw, Monstey_FindIndex(raw, name));
    Monstey_Anix renamed(raw);
    auto rename = Monstey_Op("rename", name);
    rename["name"] = "Monstey_Empty_Renamed";
    (void)renamed.Edit(J::array({rename}));
    auto rebuilt = renamed.Build();
    Monstey_Anix reopen_rename(rebuilt);
    Monstey_TestRequire(Monstey_DescriptorBytes(rebuilt, "Monstey_Empty_Renamed", true) == descriptor,
                        "Rename changed empty descriptor fields, byte count or padding");
    Monstey_TestRequire(Monstey_FooterRecord(rebuilt, Monstey_FindIndex(rebuilt, "Monstey_Empty_Renamed")) == footer,
                        "Rename changed empty footer extents, unknowns, frames or markers");
    Monstey_RequireEmptyExport(reopen_rename.ExportClip("Monstey_Empty_Renamed"));
    auto duplicate = Monstey_Op("duplicate", "Monstey_Empty_Renamed");
    duplicate["name"] = "Monstey_Empty_Duplicate";
    (void)renamed.Edit(J::array({duplicate}));
    rebuilt = renamed.Build();
    Monstey_Anix reopen_duplicate(rebuilt);
    for (const auto *copy : {"Monstey_Empty_Renamed", "Monstey_Empty_Duplicate"}) {
        Monstey_TestRequire(Monstey_DescriptorBytes(rebuilt, copy, true) == descriptor,
                            "Duplicate changed empty descriptor or padding");
        Monstey_TestRequire(Monstey_FooterRecord(rebuilt, Monstey_FindIndex(rebuilt, copy)) == footer,
                            "Duplicate changed native footer markers or non-name metadata");
        Monstey_RequireEmptyExport(reopen_duplicate.ExportClip(copy));
    }
    (void)renamed.Edit(J::array({Monstey_Op("delete", "Monstey_Empty_Renamed")}));
    rebuilt = renamed.Build();
    Monstey_Anix reopen_delete(rebuilt);
    Monstey_TestRequire(Monstey_DescriptorBytes(rebuilt, "Monstey_Empty_Duplicate", true) == descriptor &&
                            Monstey_FooterRecord(rebuilt, Monstey_FindIndex(rebuilt, "Monstey_Empty_Duplicate")) == footer,
                        "Delete changed the surviving empty clip");
    Monstey_TestRequire(reopen_delete.Describe()["clip_count"].as_uint() ==
                            Monstey_Anix(raw).Describe()["clip_count"].as_uint(),
                        "Duplicate/delete did not restore clip count");
    Monstey_Throws([&] { (void)reopen_delete.ExportClip("Monstey_Empty_Renamed"); }, "anix_clip_not_found");
    const Monstey_Anix original(raw);
    const auto before = original.Describe(), after = reopen_delete.Describe();
    for (const auto &other : before["clips"].as_array()) {
        const auto other_name = other["name"].as_string();
        if (other_name == name)
            continue;
        Monstey_TestRequire(Monstey_FindClip(after, other_name) == other,
                            "Empty structural edits changed an independent clip description");
        Monstey_TestRequire(Monstey_FooterRecord(raw, Monstey_FindIndex(raw, other_name)) ==
                                Monstey_FooterRecord(rebuilt, Monstey_FindIndex(rebuilt, other_name)),
                            "Empty structural edits changed an independent native footer record");
        if (!other["empty_native_clip"].as_bool()) {
            const std::array<double, 1> tick{other["segments"][0]["first_tick"].as_number()};
            Monstey_TestRequire(original.Sample(other_name, tick, false)["tracks"] ==
                                    reopen_delete.Sample(other_name, tick, false)["tracks"],
                                "Empty structural edits changed an independent pose sample");
        }
    }
}
J Monstey_SeedScene() {
    auto track = [](unsigned channel, J value) {
        return J::object({{"channel_index", channel}, {"values", J::array({value, value})}});
    };
    const auto tracks = J::array({track(0, J::array({1, 2, 3})), track(1, J::array({4, 5, 6})),
                                  track(2, J::array({0, 0, 0, 1})), track(3, J::array({1, 0, 0, 0}))});
    auto clip = [&](const char *name) {
        return J::object({{"name", name}, {"duration_ticks", 1000}, {"ticks", J::array({0, 1000})},
                          {"tracks", tracks}, {"events", J::array()}});
    };
    return J::object({{"library_path", "synthetic/special-clips.anix"},
                      {"channels", J::array({J::object({{"type_hash", "00ce66fc"}, {"bone_hash", "88baa7e7"}}),
                                               J::object({{"type_hash", "00ce66fc"}, {"bone_hash", "12345678"}}),
                                               J::object({{"type_hash", "27436396"}, {"bone_hash", "88baa7e7"}}),
                                               J::object({{"type_hash", "27436396"}, {"bone_hash", "12345678"}})})},
                      {"clips", J::array({clip("bind"), clip("walk")})}});
}
B Monstey_PortableEmptyFixture(bool padded) {
    const auto seed = Monstey_Anix::Create(Monstey_SeedScene()).Build();
    const auto metadata = Monstey_U32(seed, 32), count = Monstey_U32(seed, metadata + 4);
    const auto footer_at = metadata + Monstey_U32(seed, metadata + 12);
    const auto footer = std::span<const std::uint8_t>(seed).subspan(footer_at);
    const auto body = footer.subspan(24);
    const auto old_stream = Monstey_U32(footer, 0), old_names = Monstey_U32(footer, 16);
    Monstey_TestRequire(body.size() >= old_stream + 53 && old_names <= old_stream,
                        "Portable seed footer is inconsistent");
    B names(body.begin(), body.begin() + old_names);
    const auto name_offset = static_cast<std::uint32_t>(names.size());
    for (const char value : std::string("base"))
        names.push_back(static_cast<std::uint8_t>(value));
    names.push_back(0);
    B events(body.begin() + old_names, body.begin() + old_stream);
    const auto stream_at = static_cast<std::uint32_t>(names.size() + events.size());
    B stream(body.begin() + old_stream, body.end() - 12);
    Monstey_PutU32(stream, 0, stream_at - 5);
    B record(40);
    Monstey_PutU32(record, 12, name_offset);
    Monstey_PutU32(record, 32, 2);
    Monstey_PutU32(record, 36, 2);
    for (auto value : {0u, 1u, 0xff000000u, 0xff0003e8u})
        Monstey_AppendU32(record, value);
    stream.insert(stream.end(), record.begin(), record.end());
    stream.insert(stream.end(), body.end() - 12, body.end());
    B new_footer;
    for (auto value : {stream_at, Monstey_U32(footer, 4) + 2,
                       Monstey_U32(footer, 8), Monstey_U32(footer, 12) + 2,
                       static_cast<std::uint32_t>(names.size()), static_cast<std::uint32_t>(stream.size())})
        Monstey_AppendU32(new_footer, value);
    new_footer.insert(new_footer.end(), names.begin(), names.end());
    new_footer.insert(new_footer.end(), events.begin(), events.end());
    new_footer.insert(new_footer.end(), stream.begin(), stream.end());
    const auto source_at = Monstey_DescriptorAt(seed, 1);
    B empty(seed.begin() + source_at, seed.begin() + source_at + 124);
    std::fill_n(empty.begin(), 48, std::uint8_t{0});
    std::copy_n("base", 4, empty.begin());
    Monstey_PutU16(empty, 48, 124);
    for (auto field : {50u, 52u, 54u, 56u, 58u})
        Monstey_PutU16(empty, field, 0);
    Monstey_PutU32(empty, 120, 1000);
    if (padded)
        empty.resize(128);
    B new_metadata(16 + 8ull * (count + 1));
    Monstey_PutU32(new_metadata, 0, 16);
    Monstey_PutU32(new_metadata, 4, count + 1);
    Monstey_PutU32(new_metadata, 8, Monstey_U32(seed, metadata + 8));
    for (std::size_t index = 0; index <= count; ++index) {
        const auto hash = index == count ? Monstey_NativeHash("base") : Monstey_U32(seed, metadata + 16 + 8 * index);
        const auto descriptor = index == count ? empty : Monstey_DescriptorBytes(
            seed, Monstey_DescriptorName(seed, Monstey_DescriptorAt(seed, index)));
        new_metadata.resize(Monstey_Align(new_metadata.size()));
        Monstey_PutU32(new_metadata, 16 + 8 * index, hash);
        Monstey_PutU32(new_metadata, 20 + 8 * index, static_cast<std::uint32_t>(new_metadata.size()));
        new_metadata.insert(new_metadata.end(), descriptor.begin(), descriptor.end());
    }
    Monstey_PutU32(new_metadata, 12, static_cast<std::uint32_t>(new_metadata.size()));
    new_metadata.insert(new_metadata.end(), new_footer.begin(), new_footer.end());
    B output(seed.begin(), seed.begin() + metadata);
    output.insert(output.end(), new_metadata.begin(), new_metadata.end());
    Monstey_PutU32(output, 12, static_cast<std::uint32_t>(output.size()));
    Monstey_PutU32(output, 36, static_cast<std::uint32_t>(new_metadata.size()));
    return output;
}
B Monstey_CodecSegment(const std::vector<std::string> &codecs, const std::vector<B> &blocks) {
    const auto hash_at = Monstey_Align(4 + 2 * (codecs.size() + 1));
    B segment(hash_at + 4);
    Monstey_PutU32(segment, 0, 1);
    Monstey_PutU32(segment, hash_at, 0x2b99dbc1);
    for (std::size_t i = 0; i < codecs.size(); ++i) {
        const std::size_t alignment = codecs[i] == "SQ03" || codecs[i] == "AQ03" || codecs[i] == "SV01" ? 16 : 4;
        segment.resize(Monstey_Align(segment.size(), alignment));
        Monstey_PutU16(segment, 4 + 2 * i, static_cast<std::uint16_t>(segment.size()));
        segment.insert(segment.end(), blocks[i].begin(), blocks[i].end());
    }
    segment.resize(Monstey_Align(segment.size()));
    Monstey_PutU16(segment, 4 + 2 * codecs.size(), static_cast<std::uint16_t>(segment.size()));
    for (auto word : {2u, 0u, 0u, 0u, 1000u})
        Monstey_AppendU32(segment, word);
    segment.resize(Monstey_Align(segment.size(), 16));
    return segment;
}
B Monstey_RepeatedFixture(bool cross_codec) {
    const auto seed = Monstey_Anix::Create(Monstey_SeedScene()).Build();
    const std::array<Q, 3> quaternions{Q{0, 0, 0, 1}, Q{1, 0, 0, 0}, Q{0, 1, 0, 0}};
    const std::vector<V> vectors{V{1, 2, 3}, V{4, 5, 6}, V{7, 8, 9}};
    std::vector<std::string> codecs{"SQ03", "UATR"};
    std::vector<B> blocks{Monstey_AnixPackRow(quaternions), Monstey_AnixEncodeUatr({vectors, vectors})};
    std::vector<std::pair<unsigned, unsigned>> tracks{{0, 2}, {1, 0}, {0, 2}, {1, 0}, {0, 2}, {1, 0}};
    if (cross_codec) {
        Monstey_AnixQuaternionKeys keys{{0, 1000}, {{Q{0, 0, 1, 0}}, {Q{0, 0, 1, 0}}}};
        B zero_vector;
        Monstey_AppendU32(zero_vector, (511u << 2) | (511u << 12) | (511u << 22));
        zero_vector.resize(16);
        codecs = {"SQ03", "AQ03", "UATR", "SV01"};
        blocks = {Monstey_AnixPackRow(quaternions), Monstey_AnixEncodeAq03(keys),
                  Monstey_AnixEncodeUatr({vectors, vectors}), zero_vector};
        // Global-last rotation is SQ03 and global-last position is UATR.
        // Native codec order instead leaves AQ03 and SV01 as the winners.
        tracks = {{1, 2}, {3, 0}, {0, 2}, {2, 0}, {0, 2}, {2, 0}, {0, 2}, {2, 0}};
    }
    const auto segment = Monstey_CodecSegment(codecs, blocks);
    const auto payload = Monstey_U32(seed, 24), metadata = Monstey_U32(seed, 32);
    const auto footer = metadata + Monstey_U32(seed, metadata + 12);
    B replacement(32);
    Monstey_PutU32(replacement, 0, 16);
    Monstey_PutU32(replacement, 4, 2);
    for (std::size_t i = 0; i < 2; ++i) {
        const auto old = Monstey_DescriptorAt(seed, i);
        B descriptor(seed.begin() + old, seed.begin() + old + 124);
        Monstey_PutU16(descriptor, 48, 140);
        Monstey_PutU16(descriptor, 52, 1);
        Monstey_PutU16(descriptor, 54, 0);
        Monstey_PutU16(descriptor, 56, static_cast<std::uint16_t>(codecs.size()));
        Monstey_PutU16(descriptor, 58, static_cast<std::uint16_t>(tracks.size()));
        for (const auto &codec : codecs)
            descriptor.insert(descriptor.end(), codec.begin(), codec.end());
        for (const auto &track : tracks)
            descriptor.push_back(static_cast<std::uint8_t>(track.first));
        descriptor.resize(Monstey_Align(descriptor.size()));
        for (const auto &track : tracks)
            Monstey_AppendU16(descriptor, static_cast<std::uint16_t>(track.second));
        descriptor.resize(Monstey_Align(descriptor.size()));
        for (auto word : {static_cast<std::uint32_t>(i * segment.size()),
                           static_cast<std::uint32_t>(segment.size()), 0u, 1000u})
            Monstey_AppendU32(descriptor, word);
        descriptor.resize(Monstey_Align(descriptor.size(), 16));
        Monstey_PutU32(replacement, 16 + 8 * i, Monstey_U32(seed, metadata + 16 + 8 * i));
        Monstey_PutU32(replacement, 20 + 8 * i, static_cast<std::uint32_t>(replacement.size()));
        replacement.insert(replacement.end(), descriptor.begin(), descriptor.end());
    }
    Monstey_PutU32(replacement, 12, static_cast<std::uint32_t>(replacement.size()));
    replacement.insert(replacement.end(), seed.begin() + footer, seed.end());
    B output(seed.begin(), seed.begin() + payload);
    output.insert(output.end(), segment.begin(), segment.end());
    output.insert(output.end(), segment.begin(), segment.end());
    Monstey_PutU32(output, 28, static_cast<std::uint32_t>(2 * segment.size()));
    Monstey_PutU32(output, 32, static_cast<std::uint32_t>(output.size()));
    Monstey_PutU32(output, 36, static_cast<std::uint32_t>(replacement.size()));
    output.insert(output.end(), replacement.begin(), replacement.end());
    Monstey_PutU32(output, 12, static_cast<std::uint32_t>(output.size()));
    return output;
}
void Monstey_CheckRepeated(bool cross_codec, const std::filesystem::path &out) {
    const auto raw = Monstey_RepeatedFixture(cross_codec);
    const Monstey_Anix library(raw);
    const auto description = library.Describe();
    const auto &clip = Monstey_FindClip(description, "walk");
    const auto expected_tracks = cross_codec ? 8u : 6u;
    Monstey_TestRequire(description["channel_count"].as_uint() == 4 && clip["tracks"].as_uint() == expected_tracks &&
                            clip["unique_track_channels"].as_uint() == 2 &&
                            clip["repeated_channel_bindings"].as_uint() == expected_tracks - 2 &&
                            !clip["empty_native_clip"].as_bool() && clip["sample_supported"].as_bool(),
                        "Repeated native descriptor was rejected, deduplicated or misreported");
    Monstey_TestRequire(library.Build() == raw, "Repeated native no-op changed bytes");
    const std::array<double, 3> ticks{0, 500, 1000};
    const auto sample = library.Sample("walk", ticks, false);
    Monstey_TestRequire(sample["tracks"].size() == 2, "Repeated channels produced multiple output bindings");
    for (std::size_t frame = 0; frame < ticks.size(); ++frame) {
        const std::array<double, 4> rotation = cross_codec ? std::array<double, 4>{0, 0, 1, 0}
                                                        : std::array<double, 4>{0, 1, 0, 0};
        const std::array<double, 3> position = cross_codec ? std::array<double, 3>{0, 0, 0}
                                                        : std::array<double, 3>{7, 8, 9};
        Monstey_Near(Monstey_TrackValue(sample, 2, frame), rotation, 0.0002);
        Monstey_Near(Monstey_TrackValue(sample, 0, frame), position, 0.00001);
    }
    Monstey_Anix edited(raw);
    auto rename = Monstey_Op("rename", "walk");
    rename["name"] = "Monstey_Repeated_Renamed";
    (void)edited.Edit(J::array({rename}));
    const auto rebuilt = edited.Build();
    Monstey_TestRequire(Monstey_Anix(rebuilt).Sample("Monstey_Repeated_Renamed", ticks, false)["tracks"] == sample["tracks"],
                        "Structural rebuild reordered repeated writes");
    const auto prefix = cross_codec ? "synthetic-cross-codec" : "synthetic-repeated";
    Monstey_WriteNewFile(out / (std::string(prefix) + ".anix"), raw);
    Monstey_WriteNewText(out / (std::string(prefix) + "-sample.json"), sample.dump(2));
}
} // namespace

int wmain(int argc, wchar_t **argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: Monstey_AnixSpecialClipTests.exe output-directory [actual-special-clip-structures.json]\n";
        return 2;
    }
    const auto manifest_path = argc == 3 ? std::filesystem::path(argv[2]) : std::filesystem::path();
    const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto out = std::filesystem::path(argv[1]) / ("Special_" + run);
    std::filesystem::create_directories(out);
    J results = J::array(), source_hashes = J::array(), actual_results = J::array();
    unsigned failed = 0, empty_count = 0, sauron_count = 0, descriptor124 = 0, descriptor128 = 0;
    auto test = [&](std::string name, const std::function<void()> &function) {
        try {
            function();
            results.as_array().push_back(J::object({{"name", name}, {"passed", true}}));
            std::cout << "PASS " << name << "\n";
        } catch (const std::exception &error) {
            ++failed;
            results.as_array().push_back(J::object({{"name", name}, {"passed", false}, {"error", error.what()}}));
            std::cout << "FAIL " << name << ": " << error.what() << "\n";
        }
    };
    J manifest;
    B first_empty = Monstey_PortableEmptyFixture(false);
    B first_empty_padded = Monstey_PortableEmptyFixture(true);
    std::string first_empty_name = "base";
    unsigned portable_empty_count = 0;
    for (bool padded : {false, true})
        test(padded ? "portable-empty-128-metadata-structure-and-semantics"
                    : "portable-empty-124-metadata-structure-and-semantics", [&] {
            const auto &raw = padded ? first_empty_padded : first_empty;
            const Monstey_Anix library(raw);
            Monstey_TestRequire(library.Build() == raw && Monstey_DescriptorBytes(raw, "base").size() == (padded ? 128u : 124u),
                                "Portable empty fixture extent or no-op changed");
            const auto description = library.Describe();
            const auto &clip = Monstey_FindClip(description, "base");
            Monstey_TestRequire(clip["empty_native_clip"].as_bool() && !clip["sample_supported"].as_bool() &&
                                    clip["segments"].empty() && clip["tracks"].as_uint() == 0,
                                "Portable empty clip metadata differs");
            Monstey_RequireEmptyExport(library.ExportClip("base"));
            Monstey_RequireEmptyExport(Monstey_FindClip(library.Export(), "base"));
            Monstey_CheckEmptySemantics(raw, "base", 1000);
            Monstey_CheckEmptyStructure(raw, "base");
            ++portable_empty_count;
            Monstey_WriteNewFile(out / (padded ? "portable-empty-128.anix" : "portable-empty-124.anix"), raw);
        });
    if (!manifest_path.empty())
        test("load-pinned-59-file-evidence", [&] {
        manifest = J::Parse(Monstey_ReadText(manifest_path));
        Monstey_TestRequire(manifest["source_files"].size() == 59 && manifest["observations"].size() == 59,
                            "Expected exactly 59 pinned source files and observations");
    });
    if (manifest.is_object() && manifest.contains("observations")) {
        for (const auto &observation : manifest["observations"].as_array()) {
            const auto path_string = observation["path"].as_string();
            const std::filesystem::path path(Monstey_Wide(path_string));
            std::string before;
            test("source-prehash:" + path_string, [&] {
                before = Monstey_HashSha256(path);
                Monstey_TestRequire(before == observation["sha256"].as_string(), "Source differs from pinned evidence");
            });
            test("actual-special-clip:" + path_string, [&] {
                const auto raw = Monstey_ReadFile(path);
                Monstey_TestRequire(Monstey_HashSha256(raw) == before, "Source changed between prehash and read");
                const Monstey_Anix library(raw);
                const auto description = library.Describe();
                Monstey_TestRequire(library.Build() == raw, "Actual special clip no-op is not byte exact");
                unsigned file_empties = 0;
                for (const auto &descriptor : observation["clip_descriptors"].as_array()) {
                    const auto name = descriptor["name"].as_string();
                    const auto &clip = Monstey_FindClip(description, name);
                    if (!descriptor["empty_profile"].as_bool())
                        continue;
                    ++empty_count;
                    ++file_empties;
                    if (descriptor["bytes"].as_uint() == 124)
                        ++descriptor124;
                    else if (descriptor["bytes"].as_uint() == 128) {
                        ++descriptor128;
                        if (first_empty_padded.empty())
                            first_empty_padded = raw;
                    }
                    else
                        throw std::runtime_error("Unexpected pinned empty descriptor size");
                    if (first_empty.empty()) {
                        first_empty = raw;
                        first_empty_name = name;
                    }
                    Monstey_TestRequire(clip["empty_native_clip"].as_bool() && !clip["sample_supported"].as_bool() &&
                                            clip["tracks"].as_uint() == 0 && clip["segments"].empty() &&
                                            clip["codecs"].empty() && clip["unique_track_channels"].as_uint() == 0 &&
                                            clip["repeated_channel_bindings"].as_uint() == 0,
                                        "Describe did not expose a structural empty clip");
                    Monstey_RequireEmptyExport(library.ExportClip(name));
                    const auto project = library.Export();
                    Monstey_RequireEmptyExport(Monstey_FindClip(project, name));
                    Monstey_CheckEmptySemantics(raw, name, static_cast<unsigned>(descriptor["duration_ticks"].as_uint()));
                    Monstey_CheckEmptyStructure(raw, name);
                }
                if (path.filename() == L"sauron_warform.anix") {
                    ++sauron_count;
                    const auto &bind = Monstey_FindClip(description, "bind");
                    Monstey_TestRequire(description["channel_count"].as_uint() == 145 && bind["tracks"].as_uint() == 146 &&
                                            bind["unique_track_channels"].as_uint() == 144 &&
                                            bind["repeated_channel_bindings"].as_uint() == 2 && bind["sample_supported"].as_bool(),
                                        "Sauron repeated track counts changed");
                    const std::array<double, 1> tick{0};
                    const auto sample = library.Sample("bind", tick, false);
                    const std::array<double, 3> position{0.00850801169872284, 78.51881408691406, 9.245737075805664};
                    const std::array<double, 4> rotation{0, 0, 0, 1};
                    Monstey_Near(Monstey_TrackValue(sample, 62), position, 0.000001);
                    Monstey_Near(Monstey_TrackValue(sample, 135), rotation, 0.00001);
                    auto edited = library;
                    auto duplicate = Monstey_Op("duplicate", "bind");
                    duplicate["name"] = "Monstey_Sauron_Duplicate";
                    (void)edited.Edit(J::array({duplicate}));
                    const auto rebuilt = edited.Build();
                    Monstey_TestRequire(Monstey_DescriptorBytes(rebuilt, "Monstey_Sauron_Duplicate", true) ==
                                            Monstey_DescriptorBytes(raw, "bind", true),
                                        "Sauron structural duplicate changed track IDs or ordinals");
                    Monstey_TestRequire(Monstey_Anix(rebuilt).Sample("Monstey_Sauron_Duplicate", tick, false)["tracks"] ==
                                            sample["tracks"], "Sauron structural duplicate changed native sample");
                    Monstey_WriteNewText(out / "actual-sauron-tick0.json", sample.dump(2));
                }
                actual_results.as_array().push_back(J::object({{"path", path_string}, {"sha256", before},
                                                                {"bytes", static_cast<std::uint64_t>(raw.size())},
                                                                {"empty_clips_checked", file_empties}, {"byte_exact", true}}));
            });
            test("source-posthash:" + path_string, [&] {
                const auto after = Monstey_HashSha256(path);
                source_hashes.as_array().push_back(J::object({{"path", path_string}, {"sha256_before", before},
                                                              {"sha256_after", after}, {"unchanged", before == after}}));
                Monstey_TestRequire(after == before && after == observation["sha256"].as_string(),
                                    "Source hash changed during native regression tests");
            });
        }
    }
    if (!manifest_path.empty())
        test("actual-corpus-coverage-58-empty-plus-Sauron", [&] {
        Monstey_TestRequire(empty_count == 58 && sauron_count == 1 && descriptor124 == 56 && descriptor128 == 2,
                            "Actual corpus coverage differs from pinned 58/1 and descriptor 56/2 counts");
        Monstey_TestRequire(source_hashes.size() == 59 && actual_results.size() == 59,
                            "An actual source did not complete all checks");
    });
    test("empty-bind-fallback-rejected-and-direct-pose-preserved", [&] {
        Monstey_TestRequire(!first_empty.empty(), "Portable empty fixture missing");
        const auto metadata = Monstey_U32(first_empty, 32), count = Monstey_U32(first_empty, metadata + 4);
        const auto footer = metadata + Monstey_U32(first_empty, metadata + 12);
        B replacement(first_empty.begin() + metadata, first_empty.begin() + metadata + 16 + 8ull * count);
        for (std::size_t i = 0; i < count; ++i) {
            const auto name = Monstey_DescriptorName(first_empty, Monstey_DescriptorAt(first_empty, i));
            auto descriptor = Monstey_DescriptorBytes(first_empty, name);
            if (name == "bind") {
                descriptor.resize(124);
                Monstey_PutU16(descriptor, 48, 124);
                for (const auto field : {52u, 54u, 56u, 58u})
                    Monstey_PutU16(descriptor, field, 0);
            }
            replacement.resize(Monstey_Align(replacement.size()));
            Monstey_PutU32(replacement, 20 + 8 * i, static_cast<std::uint32_t>(replacement.size()));
            replacement.insert(replacement.end(), descriptor.begin(), descriptor.end());
        }
        Monstey_PutU32(replacement, 12, static_cast<std::uint32_t>(replacement.size()));
        replacement.insert(replacement.end(), first_empty.begin() + footer, first_empty.end());
        B raw(first_empty.begin(), first_empty.begin() + metadata);
        raw.insert(raw.end(), replacement.begin(), replacement.end());
        Monstey_PutU32(raw, 36, static_cast<std::uint32_t>(replacement.size()));
        Monstey_PutU32(raw, 12, static_cast<std::uint32_t>(raw.size()));
        const Monstey_Anix library(raw);
        const auto description = library.Describe();
        std::string nonempty_name;
        double first_tick = 0;
        for (const auto &clip : description["clips"].as_array())
            if (!clip["empty_native_clip"].as_bool()) {
                nonempty_name = clip["name"].as_string();
                first_tick = clip["segments"][0]["first_tick"].as_number();
                break;
            }
        Monstey_TestRequire(!nonempty_name.empty(), "Fallback fixture lost all nonempty clips");
        const std::array<double, 1> tick{first_tick};
        Monstey_TestRequire(!library.Sample(nonempty_name, tick, false)["tracks"].empty(),
                            "Nonempty direct pose was lost");
        Monstey_Throws([&] { (void)library.Sample(nonempty_name, tick, true); }, Monstey_EmptyError);
        auto held = J::object({{"op", "held"}, {"source", nonempty_name},
                              {"name", "Monstey_Held_Fallback"}, {"duration_ticks", 1000},
                              {"bind_fallback", true}, {"tick", tick[0]}});
        Monstey_RequireRollback(raw, held);
        Monstey_TestRequire(library.Build() == raw, "Fallback error mutated source");
    });
    test("malformed-neighbors-of-coherent-empty-remain-rejected", [&] {
        Monstey_TestRequire(!first_empty.empty(), "Portable empty fixture missing");
        const auto at = Monstey_DescriptorAt(first_empty, Monstey_FindIndex(first_empty, first_empty_name));
        for (const auto field : {48u, 52u, 54u, 56u, 58u}) {
            auto malformed = first_empty;
            Monstey_PutU16(malformed, at + field, field == 48 ? 123 : 1);
            Monstey_Throws([&] { (void)Monstey_Anix(malformed); });
        }
        auto malformed_footer = first_empty;
        const auto metadata = Monstey_U32(first_empty, 32);
        const auto footer = metadata + Monstey_U32(first_empty, metadata + 12);
        std::size_t cursor = footer + 24 + Monstey_U32(first_empty, footer) + 41;
        const auto wanted = Monstey_FindIndex(first_empty, first_empty_name);
        for (std::size_t i = 0; i < wanted; ++i)
            cursor += 40 + 8ull * Monstey_U32(first_empty, cursor + 36);
        Monstey_PutU32(malformed_footer, cursor + 32, 0);
        Monstey_Throws([&] { (void)Monstey_Anix(malformed_footer); });
    });
    test("empty-opaque-padding-and-independent-metadata-preserved", [&] {
        Monstey_TestRequire(!first_empty_padded.empty(), "Portable 128-byte empty fixture missing");
        auto raw = first_empty_padded;
        const auto at = Monstey_DescriptorAt(raw, Monstey_FindIndex(raw, first_empty_name));
        Monstey_PutU32(raw, at + 124, 0x34125aa5);
        Monstey_PutU32(raw, at + 116, 0xfeed1234);
        Monstey_PutU32(raw, at + 60, 0x80000000); // Preserve the native negative-zero bit pattern.
        const auto metadata = Monstey_U32(raw, 32), footer = metadata + Monstey_U32(raw, metadata + 12);
        std::size_t cursor = footer + 24 + Monstey_U32(raw, footer) + 41;
        const auto wanted = Monstey_FindIndex(raw, first_empty_name);
        for (std::size_t i = 0; i < wanted; ++i)
            cursor += 40 + 8ull * Monstey_U32(raw, cursor + 36);
        Monstey_PutU32(raw, cursor + 16, 0x13579bdf);
        Monstey_TestRequire(Monstey_Anix(raw).Build() == raw, "Opaque native empty metadata lost on no-op");
        Monstey_CheckEmptyStructure(raw, first_empty_name);
    });
    test("repeated-SQ03-and-UATR-last-local-write-wins", [&] { Monstey_CheckRepeated(false, out); });
    test("interleaved-tracks-preserve-native-codec-dispatch-order", [&] { Monstey_CheckRepeated(true, out); });
    test("invalid-repeated-native-bindings-still-validate-each-track", [&] {
        const auto raw = Monstey_RepeatedFixture(false);
        const auto at = Monstey_DescriptorAt(raw, 1);
        const auto codecs = Monstey_U16(raw, at + 56), tracks = Monstey_U16(raw, at + 58);
        const auto types = at + 124 + 4 * codecs, ids = Monstey_Align(types + tracks);
        auto invalid_codec = raw;
        invalid_codec[types + 2] = static_cast<std::uint8_t>(codecs);
        Monstey_Throws([&] { (void)Monstey_Anix(invalid_codec); });
        auto invalid_channel = raw;
        Monstey_PutU16(invalid_channel, ids + 2 * 2, 4);
        Monstey_Throws([&] { (void)Monstey_Anix(invalid_channel); });
        auto invalid_type = raw;
        Monstey_PutU16(invalid_type, ids + 2 * 2, 0);
        const std::array<double, 1> tick{0};
        Monstey_Throws([&] { (void)Monstey_Anix(invalid_type).Sample("walk", tick, false); });
    });
    test("authored-semantic-import-still-rejects-duplicate-channels-transactionally", [&] {
        auto scene = Monstey_SeedScene();
        auto &tracks = scene["clips"][1]["tracks"].as_array();
        tracks.push_back(tracks.front());
        Monstey_Throws([&] { (void)Monstey_Anix::Create(scene); });
        const auto raw = Monstey_Anix::Create(Monstey_SeedScene()).Build();
        Monstey_Anix library(raw);
        auto imported = library.ExportClip("walk");
        imported["name"] = "Monstey_Duplicate_Import";
        auto &import_tracks = imported["tracks"].as_array();
        import_tracks.push_back(import_tracks.front());
        Monstey_Throws([&] { (void)library.Edit(J::array({J::object({{"op", "import"}, {"clip", imported}})})); });
        Monstey_TestRequire(library.Build() == raw, "Invalid semantic import changed library");
    });
    const auto receipt = J::object({{"schema", "Monstey.AnixSpecialClipTests/1"},
                                    {"manifest_path", manifest_path.empty() ? J() : J(Monstey_Utf8(manifest_path.wstring()))},
                                    {"manifest_sha256", manifest_path.empty() ? J() : J(Monstey_HashSha256(manifest_path))},
                                    {"actual_manifest_supplied", !manifest_path.empty()},
                                    {"portable_empty_fixtures_checked", portable_empty_count},
                                    {"output_directory", Monstey_Utf8(out.wstring())},
                                    {"tests", results}, {"failures", failed},
                                    {"empty_clips_checked", empty_count}, {"sauron_files_checked", sauron_count},
                                    {"empty_descriptors_124", descriptor124}, {"empty_descriptors_128", descriptor128},
                                    {"actual_sources", actual_results}, {"source_hashes", source_hashes},
                                    {"source_writes", 0}, {"original_runtime_executed", false}});
    Monstey_WriteNewText(out / "special-clip-tests.json", receipt.dump(2));
    std::cout << "Tests=" << results.size() << " Failures=" << failed << " Empty=" << empty_count
              << " Sauron=" << sauron_count << "\n";
    return failed ? 1 : 0;
}
