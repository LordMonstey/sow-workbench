#pragma once
#include "Monstey_Model.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <set>

namespace Monstey
{
namespace Monstey_ModelPrivate
{
using Monstey_J = Monstey_Json;

using Monstey_Span = std::span<const std::uint8_t>;

using Monstey_V3 = std::array<double, 3>;

using Monstey_M4 = std::array<double, 16>;

void Monstey_MRequire(bool condition, std::string_view message);

std::uint32_t Monstey_Size(std::size_t n);

double Monstey_F32(Monstey_Span b, std::size_t p);

void Monstey_AddF32(Monstey_Bytes &b, double v);

float Monstey_Half(std::uint16_t h);

std::uint16_t Monstey_ToHalf(double value);

Monstey_M4 Monstey_Identity();

Monstey_M4 Monstey_Mul(const Monstey_M4 &a, const Monstey_M4 &b);

Monstey_M4 Monstey_Inv(Monstey_M4 a);

Monstey_V3 Monstey_Cross(const Monstey_V3 &a, const Monstey_V3 &b);

double Monstey_Dot(const Monstey_V3 &a, const Monstey_V3 &b);

Monstey_V3 Monstey_Unit(Monstey_V3 a, const Monstey_V3 &fallback = {0, 0, 1});

Monstey_V3 Monstey_Vector(const Monstey_M4 &m, const Monstey_V3 &p, bool point = false);

Monstey_M4 Monstey_Transform(const Monstey_V3 &t, const std::array<double, 4> &q, const Monstey_V3 &s);

Monstey_J Monstey_MatrixJson(const Monstey_M4 &m);

struct Monstey_Channel
{
    unsigned width = 0;
    std::vector<double> v;
};

struct Monstey_Part
{
    std::size_t container = 0, part = 0;
    std::map<std::string, Monstey_Channel> channels;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint8_t> triangle_bands;
    std::string triangle_band_topology_sha256;
    std::size_t Count() const;
};

struct Monstey_Attr
{
    std::uint8_t stream = 0, type = 0, semantic = 0, channel = 0;
    bool operator==(const Monstey_Attr &) const = default;
};

struct Monstey_ModelData
{
    Monstey_Bytes raw, descPrefix, infoPrefix, lodSection, bones;
    std::array<std::uint32_t, 18> h{};
    std::vector<std::array<std::uint32_t, 3>> sizes;
    std::vector<std::array<std::uint32_t, 20>> lods;
    std::vector<std::array<std::uint32_t, 10>> parts;
    std::vector<std::vector<Monstey_Attr>> layouts;
    std::array<std::vector<Monstey_Bytes>, 3> buffers;
};

std::size_t Monstey_AttrBytes(unsigned type);

unsigned Monstey_AttrWidth(unsigned type);

Monstey_ModelData Monstey_LoadModel(Monstey_Bytes raw);

struct Monstey_ModelGraph
{
    std::vector<std::uint32_t> flags, refs, thresholds, counts;
    std::vector<std::int32_t> rawMaps, effectiveMaps;
    std::vector<std::size_t> materialSource;
    std::vector<std::array<std::uint32_t, 4>> connections;
};

Monstey_ModelGraph Monstey_ReadModelGraph(const Monstey_ModelData &m);

Monstey_J Monstey_Allocation(const Monstey_ModelData &m);

void Monstey_ExpandModelConnections(Monstey_ModelData &m, const Monstey_ModelData &native,
                                    const std::vector<std::vector<std::size_t>> &children);

enum class Monstey_DecodePurpose
{
    Rigged,
    RigidGeometry,
    Inspection
};

void Monstey_SetPartBands(Monstey_Part &part, std::span<const std::uint8_t> bands);

bool Monstey_HasPartBands(const Monstey_Part &part);

void Monstey_ResolvePartBands(Monstey_Part &part, const Monstey_Part &baseline, const Monstey_J &options);

void Monstey_PackPartBands(Monstey_Part &part);

void Monstey_SetContainerBands(std::array<std::uint32_t, 20> &row, const Monstey_Part &part);

std::vector<Monstey_Part> Monstey_DecodeParts(const Monstey_ModelData &m,
                                              Monstey_DecodePurpose purpose = Monstey_DecodePurpose::Rigged);

struct Monstey_Bone
{
    std::uint32_t id = 0;
    int parent = -1;
    unsigned children = 0;
    Monstey_V3 t{};
    std::array<double, 4> q{};
    double scale = 1;
    std::string name;
    Monstey_M4 global{};
};

struct Monstey_Rig
{
    std::vector<Monstey_Bone> source;
    std::vector<std::size_t> hkToSource, sourceToHk;
    bool namesVerified = false;
    std::string skelHash;
};

std::string Monstey_ModelString(Monstey_Span b, std::size_t start, std::size_t end);

struct Monstey_SkelBone
{
    Monstey_Bone bone;
    std::array<std::uint32_t, 7> pose_bits{};
    bool active = false, finite = true;
};

struct Monstey_SkelPage
{
    std::uint32_t table_offset = 0, table_limit = 4096, row_offset = 0, row_limit = 10000;
};

struct Monstey_SkelData
{
    std::vector<Monstey_SkelBone> bones;
    Monstey_J piece_tables = Monstey_J::array();
    std::uint64_t piece_rows = 0, shown_rows = 0;
    std::size_t prefix_end = 96;
    std::uint32_t table_count = 0, string_bytes = 0;
    std::size_t bone_offset = 0, bone_end = 0;
    std::string sha256;
};

Monstey_SkelData Monstey_ReadSkel(Monstey_Span raw, const Monstey_SkelPage *page = nullptr);

Monstey_J Monstey_InspectSkelResourceRecordEnvelope(Monstey_Span raw, std::size_t first, std::uint32_t count);

struct Monstey_SkelClothResources
{
    bool qualified = false;
    std::vector<std::string> names;
    Monstey_J metadata = Monstey_J::object({{"section_locator_qualified", false},
                                            {"resource_count", nullptr},
                                            {"resource_names", nullptr},
                                            {"record_grammar_qualified", false},
                                            {"record_envelope_qualified", false},
                                            {"authoring_supported", false}});
};

Monstey_SkelClothResources Monstey_ReadSkelClothResources(Monstey_Span raw, const Monstey_SkelData &skeleton);

Monstey_Rig Monstey_ReadModelRig(const Monstey_ModelData &m, const std::filesystem::path &skel);

Monstey_J Monstey_RigJson(const Monstey_Rig &rig);

struct Monstey_GlbPrimitive
{
    std::size_t node = 0, mesh = 0, primitive = 0;
    int skin = -1;
    Monstey_M4 world = Monstey_Identity();
    Monstey_J definition;
};

class Monstey_GlbEnvelope
{
  public:
    Monstey_Bytes raw, bin;
    Monstey_J doc;
    explicit Monstey_GlbEnvelope(Monstey_Bytes data);
};

class Monstey_Glb : public Monstey_GlbEnvelope
{
  public:
    std::vector<Monstey_GlbPrimitive> primitives;
    std::vector<Monstey_M4> world;
    explicit Monstey_Glb(Monstey_Bytes data);
    Monstey_Span View(std::size_t i) const;
    static unsigned Width(std::string_view type);
    static std::size_t ComponentBytes(std::uint64_t type);
    static double Component(Monstey_Span b, std::size_t p, std::uint64_t type);
    Monstey_Channel Accessor(std::size_t index) const;
    static Monstey_M4 Transform(const Monstey_J &n);
};

class Monstey_GlbBuilder
{
  public:
    Monstey_J doc = Monstey_J::object(
        {{"asset", Monstey_J::object({{"version", "2.0"}, {"generator", "Monstey native C++ Model Publisher"}})},
         {"accessors", Monstey_J::array()},
         {"bufferViews", Monstey_J::array()}});
    Monstey_Bytes binary;
    std::size_t View(Monstey_Span bytes);
    std::size_t Accessor(const Monstey_Channel &channel, std::uint32_t component = 5126, bool normalized = false,
                         std::string type = {});
    Monstey_Bytes Finish();
};

Monstey_Channel Monstey_Channel3(std::size_t count, const Monstey_V3 &value);

Monstey_V3 Monstey_At3(const Monstey_Channel &c, std::size_t i);

void Monstey_Set3(Monstey_Channel &c, std::size_t i, const Monstey_V3 &value);

void Monstey_QuantWeights(Monstey_Channel &w);

Monstey_Part Monstey_FromGlb(const Monstey_Glb &glb, const Monstey_GlbPrimitive &primitive,
                             const Monstey_ModelData &target, const Monstey_J &options, bool nativeMode);

Monstey_Bytes Monstey_SerializeModel(const Monstey_ModelData &m);

constexpr std::size_t Monstey_ModelDrawPaletteLimit = 64;

using Monstey_ModelPalette = std::bitset<256>;

Monstey_ModelPalette Monstey_VertexModelPalette(const Monstey_Part &source, std::size_t vertex, bool preserveZero);

std::vector<Monstey_Part> Monstey_SplitModelPart(const Monstey_Part &source, bool preserveZero);

struct Monstey_EncodedPart
{
    std::vector<Monstey_Attr> layout;
    Monstey_Bytes vertices, indices, palette;
};

Monstey_EncodedPart Monstey_EncodeModelPart(const Monstey_Part &p, bool half, bool preserveZero);

bool Monstey_NativeEquivalent(const Monstey_Part &current, const Monstey_Part &original, bool nativeExact = false);

bool Monstey_PatchNativePart(const Monstey_ModelData &native, const std::array<std::uint32_t, 10> &donor,
                             const Monstey_Part &original, const Monstey_Part &authored, bool half, bool nativeExact,
                             bool extendPalette, Monstey_EncodedPart &out);

Monstey_Bytes Monstey_RebuildModel(const Monstey_ModelData &native,
                                   const std::map<std::size_t, std::vector<Monstey_Part>> &replacements,
                                   bool preserveZero, const Monstey_J &options, bool nativeExact = false);

Monstey_Bytes Monstey_BuildGlb(const Monstey_ModelData &m, const Monstey_Rig &rig);

bool Monstey_NativeEquivalent(const Monstey_Part &current, const Monstey_Part &original, bool nativeExact);

void Monstey_CheckNativeSkin(const Monstey_Glb &glb, const Monstey_Rig &rig, const Monstey_GlbPrimitive &p);
} // namespace Monstey_ModelPrivate
} // namespace Monstey
