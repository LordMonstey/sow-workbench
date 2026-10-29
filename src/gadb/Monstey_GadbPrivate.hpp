#pragma once
#include "Monstey_Gadb.hpp"
#include "Monstey_GadbClassCache.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace Monstey
{
namespace Monstey_GadbPrivate
{

using J = Monstey_Json;

using B = Monstey_Bytes;

std::string Monstey_Hex(std::uint32_t value);

std::string Monstey_Lower(std::string_view text);

bool Monstey_Has(std::string_view value, std::string_view part);

std::uint32_t Monstey_ParseRef(std::string_view text);

bool Monstey_KeyEqual(std::string_view a, std::string_view b);

std::string Monstey_Type(std::uint8_t type);

J Monstey_FloatJson(std::uint32_t word);

void Monstey_Append(B &to, const B &from, std::size_t begin, std::size_t end);

void Monstey_Aligned(B &bytes);

void Monstey_OutsideGame(const std::filesystem::path &output);

struct Monstey_Descriptor
{
    std::uint32_t hash = 0, sorted = 0;
    std::uint8_t ordinal = 0, index = 0, type = 0;
    std::uint16_t count = 0, offset = 0;
    bool operator==(const Monstey_Descriptor &) const = default;
};

struct Monstey_Payload
{
    std::vector<std::uint32_t> words;
    std::vector<Monstey_Descriptor> descriptors;
    std::map<std::uint32_t, J> metadata;
    std::set<std::uint32_t> introduced;
};

struct Monstey_RecordData
{
    std::size_t offset = 0, size = 0, values = 0;
    std::uint32_t name = 0, packed = 0, flags = 0, reference = 0;
    bool named = false;
    std::shared_ptr<const Monstey_Payload> payload;
};

struct Monstey_CategoryData
{
    std::uint32_t schema = 0, name = 0, path = 0, anonymous = 0, hash = 0;
    std::vector<Monstey_RecordData> records;
};

struct Monstey_Clone
{
    std::uint32_t source = 0, reference = 0;
    std::string id;
    std::map<std::uint32_t, J> edits;
    Monstey_RecordData snapshot;
};

struct Monstey_NewCategory
{
    std::string schema, name, path;
};

struct Monstey_NewRecord
{
    std::uint32_t reference = 0;
    std::string name;
    Monstey_RecordData record;
};

struct Monstey_State
{
    std::map<std::uint32_t, std::map<std::uint32_t, J>> edits;
    std::vector<Monstey_Clone> clones;
    std::map<std::uint32_t, std::string> renames;
    std::map<std::uint32_t, J> deletes;
    std::vector<Monstey_NewCategory> categories;
    std::vector<Monstey_NewRecord> records;
    std::map<std::uint32_t, Monstey_RecordData> layouts;
    J editorMetadata = J::object();
};

std::string Monstey_Suffix(std::string_view name, std::uint32_t target);

} // namespace Monstey_GadbPrivate

struct Monstey_Gadb::Monstey_Impl
{
    using J = Monstey_Json;
    using B = Monstey_Bytes;
    using Monstey_Descriptor = Monstey_GadbPrivate::Monstey_Descriptor;
    using Monstey_Payload = Monstey_GadbPrivate::Monstey_Payload;
    using Monstey_RecordData = Monstey_GadbPrivate::Monstey_RecordData;
    using Monstey_CategoryData = Monstey_GadbPrivate::Monstey_CategoryData;
    using Monstey_Clone = Monstey_GadbPrivate::Monstey_Clone;
    using Monstey_NewCategory = Monstey_GadbPrivate::Monstey_NewCategory;
    using Monstey_NewRecord = Monstey_GadbPrivate::Monstey_NewRecord;
    using Monstey_State = Monstey_GadbPrivate::Monstey_State;

    std::filesystem::path source, schemaPath;

    B data;

    J schema = J::object();

    std::string sourceHash, schemaHash;

    std::uintmax_t sourceSize = 0;

    std::filesystem::file_time_type sourceTime{};

    std::size_t stringBase = 0, stringSize = 0, descriptorBase = 0, registryStart = 0;

    std::uint32_t descriptorCount = 0, namedCount = 0, anonymousCount = 0, wordCount = 0;

    std::vector<Monstey_CategoryData> categories;

    std::map<std::uint32_t, std::string> fieldNames;

    std::vector<std::unordered_map<std::uint32_t, const J *>> schemaFields;

    B wrapper;

    bool schemaInline = false, memorySource = false;

    Monstey_State state;

    std::vector<Monstey_State> undo, redo;

    explicit Monstey_Impl(const std::filesystem::path &path, const std::filesystem::path &nativeSchema,
                          const J *qualifiedSchema = nullptr, const B *memory = nullptr);

    void Unchanged() const;

    std::string String(std::uint32_t pointer) const;

    std::string WideText(std::uint32_t pointer) const;

    J Curve(std::uint32_t pointer, std::uint8_t type) const;

    J NormalizeCurve(std::uint8_t type, const J &input) const;

    void Parse();

    const Monstey_RecordData &Native(std::uint32_t ref) const;

    const Monstey_Clone *Clone(std::uint32_t ref) const;

    std::size_t CategoryCount() const;

    std::string CategoryText(std::uint32_t ci, int which) const;

    const Monstey_NewRecord *Created(std::uint32_t ref) const;

    const Monstey_RecordData &Base(std::uint32_t ref) const;

    std::vector<std::uint32_t> References(std::uint32_t ci) const;

    std::uint32_t NextReference(std::uint32_t ci) const;

    std::uint32_t Select(std::string_view selector) const;

    std::vector<std::uint32_t> Words(const Monstey_RecordData &r) const;

    std::vector<Monstey_Descriptor> Descriptors(const Monstey_RecordData &r) const;

    const J &SchemaField(std::uint32_t ref, std::uint32_t hash) const;

    Monstey_Payload Payload(std::uint32_t ref) const;

    static void DescriptorOrder(Monstey_Payload &p);

    void Install(std::uint32_t ref, Monstey_Payload payload);

    bool Variable(const Monstey_Descriptor &d, const J &f) const;

    bool Array(const Monstey_Descriptor &d, const J &f) const;

    J Decode(const Monstey_RecordData &r, const Monstey_Descriptor &d, bool array) const;

    std::string Name(std::uint32_t ref) const;

    const std::map<std::uint32_t, J> &Edits(std::uint32_t ref) const;

    J Metadata(std::uint32_t ref) const;

    bool Allowed(std::string_view required, std::string actual) const;

    std::uint32_t ValueRef(const J &value) const;

    J Normalize(std::uint32_t ref, const Monstey_Descriptor &d, const J &f, const J &value) const;

    void Commit();
};

} // namespace Monstey
