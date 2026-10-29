#pragma once
#include "Monstey_NavStaticTreeApi.hpp"
#include "Monstey_World.hpp"
#include "Monstey_WorldEdit.hpp"
#include "Monstey_WorldBlender.hpp"
#include "Monstey_StaticPrefab.hpp"
#include "Monstey_InstGeometry.hpp"
#include "Monstey_WorldGeometry.hpp"
#include "Monstey_WorldMerge.hpp"
#include "Monstey_WorldScene.hpp"
#include "Monstey_WorldLighting.hpp"
#include "Monstey_AssetInspect.hpp"
#include "Monstey_WorldAssets.hpp"
#include "Monstey_WorldComposition.hpp"
#include "Monstey_NavigationWorld.hpp"
#include "Monstey_PhysicsWorld.hpp"
#include "Monstey_Archive.hpp"
#include <algorithm>
#include <cctype>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

namespace Monstey
{
namespace Monstey_WorldPrivate
{
using J = Monstey_Json;

using B = Monstey_Bytes;

std::string Monstey_WLower(std::string s);

float Monstey_WFloat(const J &v);

float Monstey_WReadFloat(std::span<const std::uint8_t> b, std::size_t p);

void Monstey_WPutFloat(B &b, std::size_t p, float v);

J Monstey_WFloats(std::span<const std::uint8_t> b, std::size_t p, std::size_t n);

std::vector<float> Monstey_WVector(const J &v, std::size_t n);

std::array<std::array<double, 3>, 3> Monstey_WRotation(const std::vector<float> &q);

std::uint32_t Monstey_WUint(const J &v);

std::filesystem::path Monstey_WPath(const J &v);

std::string Monstey_WTextPath(const std::filesystem::path &p);

std::string Monstey_WName(std::span<const std::uint8_t> b, std::uint32_t p);

std::uint32_t Monstey_WAddString(B &pool, const std::string &text);

void Monstey_WExpected(const J &a, const B &raw);

J Monstey_WPublish(const std::filesystem::path &source, const B &original, const J &a, const B &raw, J result);

struct Monstey_WPropertyDb
{
    struct Desc
    {
        std::uint32_t hash, sorted;
        std::uint8_t index, lookup, type;
        std::uint16_t count, offset;
    };
    struct Record
    {
        std::size_t at, end;
        std::uint32_t name, packed, flags;
        std::vector<std::uint32_t> words;
        std::vector<Desc> fields;
    };
    B raw, pool;
    std::size_t pool_start{}, pool_end{}, descriptor_end{}, category_start{};
    std::uint32_t descriptors{};
    std::vector<Record> records;
    explicit Monstey_WPropertyDb(B input);
    J Value(const Record &r, const Desc &d) const;
    std::pair<B, std::uint32_t> Overlay(std::optional<std::uint32_t> source, const J &properties, const J &types,
                                        const J &removals) const;
    std::pair<B, J> ImportClosure(const Monstey_WPropertyDb &donor, std::uint32_t root) const;
    J Fields(std::uint32_t ref) const;
};

struct Monstey_WObjres
{
    struct Chunk
    {
        std::uint32_t kind;
        B bytes;
    };
    struct Object
    {
        std::size_t at;
        std::uint32_t group, index;
    };
    B raw;
    std::vector<Chunk> chunks;
    std::size_t obj_chunk{}, db_chunk{};
    B classes, names, extra;
    std::vector<Object> objects;
    std::uint32_t groups{};
    std::unique_ptr<Monstey_WPropertyDb> db;
    explicit Monstey_WObjres(B input);
    J Query(const J &a) const;
    J Report() const;
    B Encode() const;
    void Apply(const J &updates);
};

struct Monstey_WObjectGroup
{
    std::uint32_t identity;
    std::vector<B> records;
};

std::vector<Monstey_WObjectGroup> Monstey_WObjectGroups(const Monstey_WObjres &doc);

B Monstey_WEncodeObjectGroups(const B &classes, const B &names, const B &extra,
                              const std::vector<Monstey_WObjectGroup> &groups);

Monstey_WObjres::Object Monstey_WSelectObject(const Monstey_WObjres &doc, const J &a, const char *name_key = "name",
                                              const char *group_key = "group_index",
                                              const char *index_key = "object_index");

void Monstey_WNewIdentity(const Monstey_WObjres &doc, const std::string &name);

void Monstey_WQualifiedIdentity(const Monstey_WObjres &doc, const Monstey_WObjres::Object &obj, bool include_self);

void Monstey_WQualifiedClone(const Monstey_WObjres &doc, const Monstey_WObjres::Object &object);

J Monstey_WPropertyTypes(const Monstey_WObjres &doc, const std::string &kind, const J &properties,
                         const J &explicit_types, std::optional<std::uint32_t> record = {});

J Monstey_WEditObject(Monstey_WObjres &doc, const J &entry, const std::string &operation);

J Monstey_WObjresEdit(Monstey_WObjres &doc, const J &spec);

struct Monstey_WInst;

struct Monstey_WRender
{
    struct Model
    {
        std::uint32_t path_offset, geometry_group;
        std::vector<std::uint32_t> material_offsets;
    };
    struct Group
    {
        std::vector<B> records, lods;
        std::vector<std::pair<std::uint32_t, std::vector<std::uint32_t>>> owners;
    };
    B raw, strings, bindings, scene;
    std::array<std::uint32_t, 9> header{};
    std::vector<Model> models;
    std::vector<Group> groups;
    explicit Monstey_WRender(B input);
    std::string Name(std::uint32_t offset) const;
    std::uint32_t Intern(const std::string &input);
    B Encode() const;
    J Report() const;
};

std::map<std::uint16_t, B> Monstey_WCullPrototypes(const Monstey_WRender &source, bool preserve_group_identity = false);

J Monstey_WAddDefined(Monstey_WInst &target, const J &entry);

struct Monstey_WInst
{
    B raw, geometry, materials, names_blob;
    std::array<std::uint32_t, 14> header{};
    std::vector<B> primary, secondary, instances;
    std::vector<std::string> names, original_names;
    std::vector<B> name_bytes;
    std::size_t geometry_start{}, geometry_end{}, instance_start{};
    J layouts, submeshes, material_names;
    explicit Monstey_WInst(B input);
    void Validate() const;
    B Encode() const;
    J Owners(std::size_t index) const;
    J Report(std::uint64_t limit, bool all = false) const;
    B Transform(const B &original, const J &update) const;
    static Monstey_WInst Donor(const J &description);
    J AddOwner(const J &description);
    J AddPlacement(B record, const std::string &input_path, const J &entry);
    J AddInstance(const J &entry);
    J Apply(const J &spec);
};

J Monstey_WSection(const B &raw, const std::string &requested);

float Monstey_WHalf(std::uint16_t h);

std::uint16_t Monstey_WPackHalf(float value);

std::uint32_t Monstey_WCrc(std::span<const std::uint8_t> bytes);

struct Monstey_WNpy
{
    char kind{}, endian{};
    std::size_t unit{}, count{}, width{};
    B data;
    static Monstey_WNpy Parse(const B &b);
    double Number(std::size_t row, std::size_t column) const;
};

B Monstey_WNpyEncode(const std::string &dtype, std::size_t count, std::size_t width, const B &data);

B Monstey_WNpzEncode(const std::map<std::string, B> &entries);

std::map<std::string, Monstey_WNpy> Monstey_WNpzRead(const B &raw);

struct Monstey_WMesh
{
    struct Attr
    {
        std::size_t offset, stride;
        std::uint32_t count;
        std::uint8_t storage, semantic, channel, width, unit;
        bool operator==(const Attr &) const = default;
    };
    struct Group
    {
        std::size_t start, end, vertex, index, vertex_bytes, index_bytes;
        std::vector<std::vector<std::array<std::uint8_t, 4>>> layouts;
        std::vector<std::array<std::uint32_t, 13>> draws;
    };
    B raw;
    std::array<std::uint32_t, 8> header{};
    std::vector<Group> groups;
    static std::pair<unsigned, unsigned> Storage(unsigned kind);
    static std::string Semantic(unsigned code, unsigned channel);
    explicit Monstey_WMesh(B input);
    std::map<std::string, Attr> Attributes(std::size_t gi, std::size_t di) const;
    std::vector<std::uint16_t> Indices(std::size_t gi, std::size_t di) const;
    double Component(const Attr &a, std::size_t vertex, std::size_t component, const B *source = nullptr) const;
    J Report() const;
    B Export(std::size_t gi, std::size_t di) const;
    J ApplyNpz(const J &update, const std::map<std::string, Attr> &attrs, B &written);
    J Apply(const J &spec);
};

B Monstey_WEncodeNativeNumber(const Monstey_WMesh::Attr &a, double value);

std::pair<B, J> Monstey_WTopology(const Monstey_WMesh &mesh, const J &spec);

std::pair<B, J> Monstey_WRenderBuild(const Monstey_WMesh &mesh, const J &topology, const B &original,
                                     const std::string &source_path, const std::string &target_path, bool preserve,
                                     const B *scene_template);

J Monstey_WAddDefined(Monstey_WInst &target, const J &entry);

J Monstey_WAuthoredCall(std::string_view name, const J &a);

std::pair<B, J> Monstey_WBindingEncode(const B &inst_raw, const B &donor, std::string resource, std::uint32_t owner);

J Monstey_WBinding(const J &a);

using Monstey_WMatrix = std::array<std::array<double, 4>, 4>;

Monstey_WMatrix Monstey_WSceneMatrix(const J &position, const J &quaternion, double scale = 1);

J Monstey_WSceneJsonMatrix(const Monstey_WMatrix &matrix);

Monstey_WMatrix Monstey_WSceneMultiply(const Monstey_WMatrix &a, const Monstey_WMatrix &b);

std::pair<J, J> Monstey_WSceneSelect(const J::Array &rows, const J &select);

std::pair<J, std::string> Monstey_WScenePrefab(const B &raw, const J &provenance, const J &definition_key);

J Monstey_WScenePrepare(const J &a);

J Monstey_WSchemaObject(J::Object properties, std::initializer_list<const char *> required = {});

J Monstey_WSchemaArray(J item, unsigned minimum = 0);

J Monstey_WSchema(std::string_view name);

struct Monstey_WTool
{
    const char *name;
    const char *description;
    const char *status;
    bool read_only;
};

extern const Monstey_WTool Monstey_WTools[41];
} // namespace Monstey_WorldPrivate
} // namespace Monstey
