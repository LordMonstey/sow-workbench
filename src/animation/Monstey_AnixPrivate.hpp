#pragma once
#include "Monstey_Anix.hpp"
#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <utility>

namespace Monstey
{
namespace Monstey_AnixPrivate
{
using J = Monstey_Json;

using B = Monstey_Bytes;

using Q = Monstey_AnixQuaternion;

using V = Monstey_AnixVector;

constexpr std::uint32_t Monstey_Rotation = 0x27436396, Monstey_Position = 0x00ce66fc, Monstey_Lith = 0x2b99dbc1,
                        Monstey_KeyString = 0xb9010be0, Monstey_Event = 0x210c8f5a, Monstey_Auxiliary = 0x5f1d8eed;

constexpr std::size_t Monstey_Limit = 512u * 1024u * 1024u, Monstey_SampleLimit = 1000000;

void Monstey_ARequire(bool v, std::string_view m);

std::size_t Monstey_AAlign(std::size_t v, std::size_t n = 4);

void Monstey_ARange(std::span<const std::uint8_t> b, std::size_t p, std::size_t n);

std::uint32_t Monstey_AU32(std::span<const std::uint8_t> b, std::size_t p);

std::uint16_t Monstey_AU16(std::span<const std::uint8_t> b, std::size_t p);

float Monstey_AF32(std::span<const std::uint8_t> b, std::size_t p);

B Monstey_ASlice(std::span<const std::uint8_t> b, std::size_t p, std::size_t n);

void Monstey_AAppend(B &b, std::span<const std::uint8_t> v);

void Monstey_APad(B &b, std::size_t n = 4);

void Monstey_AFAdd(B &b, float f);

std::uint32_t Monstey_A32(std::uint64_t v);

std::uint16_t Monstey_A16(std::size_t v);

float Monstey_AFloat(double v);

std::string Monstey_AString(std::span<const std::uint8_t> b, std::size_t p, std::size_t end);

std::string Monstey_AEventString(std::span<const std::uint8_t> b, std::size_t p, std::size_t end);

void Monstey_AEventValidate(std::string_view text);

J Monstey_AEventJson(std::uint32_t tick, std::string_view text);

std::string Monstey_AEventBytes(const J &event);

void Monstey_AName(std::string_view n, bool clip = false);

std::uint32_t Monstey_AHash(std::string_view s);

void Monstey_AZero(std::span<const std::uint8_t> b, std::size_t p);

J Monstey_AArray(std::span<const float> v);

std::uint32_t Monstey_AJsonU(const J &j);

std::uint32_t Monstey_AHashJson(const J &j);

template <std::size_t N> std::array<float, N> Monstey_AVec(const J &j);

std::uint64_t Monstey_APackQuaternion(Q input);

Q Monstey_AUnpackQuaternion(std::uint64_t w);

Q Monstey_ANlerp(Q a, Q b, double f);

struct Monstey_ABracket
{
    std::size_t lo, hi;
    double f;
};

Monstey_ABracket Monstey_ABracketAt(std::span<const std::uint32_t> t, double x);

struct Monstey_AUncompressedTrack
{
    std::vector<std::uint32_t> ticks;
    std::size_t data = 0;
};

std::vector<Monstey_AUncompressedTrack> Monstey_AReadUncompressed(std::span<const std::uint8_t> bytes,
                                                                  std::size_t tracks, std::size_t width);

struct Monstey_AUncompressedBracket
{
    std::size_t lo = 0, hi = 1;
    float fraction = 0;
};

Monstey_AUncompressedBracket Monstey_AUncompressedBracketAt(std::span<const std::uint32_t> ticks, std::uint32_t sample);

float Monstey_ANativeAtanPositive(float y, float x);

float Monstey_ANativeSin(float angle);

Q Monstey_ANativeUaqt(Q a, Q b, float fraction);

template <std::size_t Width>
B Monstey_AEncodeUncompressed(std::span<const std::uint32_t> ticks,
                              const std::vector<std::vector<std::array<float, Width>>> &values);

struct Monstey_AAv01Layout
{
    std::vector<std::uint32_t> ticks;
    std::size_t data = 0, stride = 0;
};

Monstey_AAv01Layout Monstey_AReadAv01(std::span<const std::uint8_t> bytes, std::size_t tracks);

struct Monstey_ACaf4Layout
{
    std::vector<std::uint32_t> ticks;
    std::size_t data = 0, stride = 0;
};

Monstey_ACaf4Layout Monstey_AReadCaf4(std::span<const std::uint8_t> bytes, std::size_t tracks);

struct Monstey_AChannel
{
    std::uint32_t type = 0, bone = 0;
};

struct Monstey_ATrack
{
    std::uint8_t codec = 0;
    std::uint16_t channel = 0;
};

struct Monstey_AExtra
{
    std::uint32_t hash = 0;
    B bytes;
};

struct Monstey_ASegment
{
    std::uint32_t first = 0, last = 0, old_offset = 0;
    B raw;
    std::vector<B> blocks;
    std::vector<Monstey_AExtra> extras;
    std::vector<std::uint32_t> ticks;
    bool appended = false;
};

struct Monstey_AMarker
{
    std::uint32_t mark = 0, tick = 0;
    std::optional<std::string> event;
};

struct Monstey_AFooterRecord
{
    std::array<float, 3> extents{};
    std::array<std::uint32_t, 4> unknown{};
    std::uint32_t frames = 0, name_offset = 0;
    std::vector<Monstey_AMarker> markers;
};

struct Monstey_AClip
{
    std::string name;
    std::uint32_t hash = 0, duration = 0, unknown116 = 0;
    std::uint16_t flags = 0;
    std::array<float, 7> root_start{}, root_end{};
    std::vector<std::string> codecs;
    std::vector<Monstey_ATrack> tracks;
    std::vector<Monstey_ASegment> segments;
    std::vector<Monstey_AExtra> extras;
    Monstey_AFooterRecord footer;
    B descriptor;
    std::size_t old_descriptor = 0;
    bool descriptor_rebuild = false;
};

constexpr char Monstey_AEmptyClipCode[] = "unsupported_anix_empty_clip";

constexpr char Monstey_AEmptyClipMessage[] = "Native empty clip has no qualified semantic sampling or authoring policy";

void Monstey_ARequireSegments(const Monstey_AClip &clip);

struct Monstey_ALibrary
{
    B original, channel_prefix, payload, section_gap, footer_names, footer_header, footer_trailer;
    std::array<std::uint32_t, 2> header_unknown{1, 1};
    std::uint32_t metadata_unknown = 0, channel_start = 48;
    std::vector<Monstey_AChannel> channels;
    std::vector<Monstey_AClip> clips;
    std::string path;
    std::vector<std::string> event_strings;
    bool changed = false;
};

std::size_t Monstey_ACodecTracks(const Monstey_AClip &c, std::size_t codec);

std::span<const std::uint8_t> Monstey_AKeyStrings(const Monstey_AClip &c);

Monstey_ASegment Monstey_AReadSegment(std::span<const std::uint8_t> raw, std::size_t codec_count, std::uint32_t first,
                                      std::uint32_t last, std::uint32_t offset);

void Monstey_AReadFooter(Monstey_ALibrary &l, std::span<const std::uint8_t> f);

Monstey_ALibrary Monstey_AParse(std::span<const std::uint8_t> b);

std::size_t Monstey_AFind(const Monstey_ALibrary &l, std::string_view name);

std::vector<J> Monstey_AEventList(const Monstey_AClip &c);

B Monstey_ABuildFooter(const Monstey_ALibrary &l);

B Monstey_ABuildDescriptor(const Monstey_AClip &c, const std::vector<std::uint32_t> &offsets);

B Monstey_ABuild(const Monstey_ALibrary &l);

std::vector<double> Monstey_ASampleTicks(const Monstey_AClip &c);

bool Monstey_APoseCodec(std::string_view c);

bool Monstey_AAuxiliaryCodec(std::string_view codec);

std::string_view Monstey_AChannelCodec(const Monstey_AClip &clip, std::uint16_t channel);

std::uint32_t Monstey_AAuxiliaryWord(const J &value);

std::uint8_t Monstey_AAuxiliaryByte(const J &value);

bool Monstey_AStaticAuxiliaryEqual(std::string_view codec, const J &a, const J &b);

bool Monstey_ACodecBindingsQualified(const Monstey_ALibrary &library, const Monstey_AClip &clip, std::size_t codec);

std::set<std::string> Monstey_AUnsupportedAuxiliary(const Monstey_ALibrary &library, const Monstey_AClip &clip);

using Monstey_AFrame = std::map<std::uint16_t, J>;

void Monstey_AApply(const Monstey_ALibrary &l, const Monstey_AClip &c, std::span<const double> ticks,
                    std::vector<Monstey_AFrame> &frames, std::set<std::string> &ignored);

J Monstey_ASample(const Monstey_ALibrary &l, const Monstey_AClip &c, std::span<const double> ticks, bool fallback);

std::optional<std::uint16_t> Monstey_AClipEventChannel(const Monstey_ALibrary &l, const Monstey_AClip &c);

std::uint16_t Monstey_AResolveEventChannel(const Monstey_ALibrary &l, const J &options,
                                           std::optional<std::uint16_t> existing = std::nullopt);

J Monstey_AClipEvents(const Monstey_AClip &c);

Monstey_AClip Monstey_AImportClip(const Monstey_ALibrary &l, const J &scene);

J Monstey_AExportClip(const Monstey_ALibrary &l, const Monstey_AClip &c);

void Monstey_ARegrid(Monstey_AClip &c, const J &events);

std::pair<B, B> Monstey_AEncodeEvents(const J &events);

B Monstey_AEncodeSegment(const std::vector<std::string> &codes, const std::vector<B> &blocks,
                         std::span<const std::uint32_t> ticks, const std::vector<Monstey_AExtra> &other_extras = {});

struct Monstey_ASemanticTrack
{
    std::uint16_t channel = 0;
    std::vector<J> values;
};

Monstey_AClip Monstey_AImportClip(const Monstey_ALibrary &l, const J &scene);

void Monstey_AReplaceEvents(Monstey_AClip &c, const Monstey_ALibrary &l, const J &events, const J &options);

void Monstey_AUnique(const Monstey_ALibrary &l);

void Monstey_ARequireTimeline(const Monstey_ALibrary &l, const Monstey_AClip &c);

J Monstey_ATimelineScene(const Monstey_ALibrary &l, const Monstey_AClip &c, const J &op, std::string_view kind);

struct Monstey_ASkeleton
{
    B raw, table, prefix, rig, lods, tail;
    std::array<std::uint32_t, 16> header{};
    std::array<std::size_t, 7> marks{};
    std::vector<std::uint32_t> pointers, types;
    std::vector<std::string> paths;
};

Monstey_ASkeleton Monstey_AReadSkeleton(std::span<const std::uint8_t> raw);

J Monstey_ASkeletonJson(const Monstey_ASkeleton &s);
} // namespace Monstey_AnixPrivate

struct Monstey_Anix::Monstey_State : Monstey_AnixPrivate::Monstey_ALibrary
{
};
} // namespace Monstey
