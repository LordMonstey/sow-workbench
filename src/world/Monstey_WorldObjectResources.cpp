#include "Monstey_WorldPrivate.hpp"

namespace Monstey
{
namespace Monstey_WorldPrivate
{

Monstey_WObjres::Monstey_WObjres(B input) : raw(std::move(input))
{
    Monstey_Require(raw.size() >= 16 && std::memcmp(raw.data(), "OBJR", 4) == 0 && Monstey_U32(raw, 4) == 13,
                    "Expected OBJR13");
    auto count = Monstey_U32(raw, 12);
    Monstey_Require(count <= (raw.size() - 16) / 8, "OBJR chunk count exceeds extent");
    std::size_t at = 16;
    unsigned no{}, nd{};
    for (std::uint32_t j = 0; j < count; ++j)
    {
        auto kind = Monstey_U32(raw, at), size = Monstey_U32(raw, at + 4);
        at += 8;
        Monstey_CheckRange(raw, at, size);
        B bytes(raw.begin() + static_cast<std::ptrdiff_t>(at), raw.begin() + static_cast<std::ptrdiff_t>(at + size));
        if (bytes.size() >= 4 && std::memcmp(bytes.data(), "OBJL", 4) == 0)
        {
            obj_chunk = j;
            ++no;
        }
        if (bytes.size() >= 4 && std::memcmp(bytes.data(), "GADB", 4) == 0)
        {
            db_chunk = j;
            ++nd;
        }
        chunks.push_back({kind, std::move(bytes)});
        at += size;
    }
    Monstey_Require(at == raw.size() && no == 1 && nd == 1, "OBJR has trailing bytes or ambiguous OBJL/GADB chunks");
    db = std::make_unique<Monstey_WPropertyDb>(chunks[db_chunk].bytes);
    auto &obj = chunks[obj_chunk].bytes;
    Monstey_Require(obj.size() >= 28 && Monstey_U32(obj, 4) == 5, "Expected OBJL5");
    groups = Monstey_U32(obj, 8);
    auto total = Monstey_U32(obj, 12), nc = Monstey_U32(obj, 16), nn = Monstey_U32(obj, 20), ne = Monstey_U32(obj, 24);
    auto table_end = 28ull + nc + nn + ne;
    Monstey_Require(table_end <= obj.size() && table_end + 8ull * groups + 50ull * total == obj.size(),
                    "OBJL table/count extent differs");
    classes.assign(obj.begin() + 28, obj.begin() + 28 + nc);
    names.assign(obj.begin() + 28 + nc, obj.begin() + 28 + nc + nn);
    extra.assign(obj.begin() + 28 + nc + nn, obj.begin() + static_cast<std::ptrdiff_t>(table_end));
    at = static_cast<std::size_t>(table_end);
    for (std::uint32_t group = 0; group < groups; ++group)
    {
        auto n = Monstey_U32(obj, at + 4);
        at += 8;
        Monstey_Require(n <= total - objects.size(), "OBJL group count exceeds total");
        for (std::uint32_t index = 0; index < n; ++index)
        {
            Monstey_CheckRange(obj, at, 50);
            (void)Monstey_WName(classes, Monstey_U32(obj, at));
            (void)Monstey_WName(names, Monstey_U32(obj, at + 4));
            auto prefix = Monstey_U16(obj, at + 8);
            if (prefix != 65535)
                (void)Monstey_WName(extra, prefix);
            Monstey_Require(Monstey_U32(obj, at + 10) < db->records.size(), "OBJL property index outside ObjectDB");
            objects.push_back({at, group, index});
            at += 50;
        }
    }
    Monstey_Require(at == obj.size() && objects.size() == total, "OBJL groups do not consume all objects");
}

J Monstey_WObjres::Query(const J &a) const
{
    auto query = Monstey_WLower(a.value("name_contains", "").as_string()), type = a.value("class_name", "").as_string();
    auto offset = a.value("offset", 0).as_uint(), limit = a.value("limit", 100).as_uint();
    Monstey_Require(limit >= 1 && limit <= 500, "Page size must be 1..500");
    bool props = a.value("include_properties", true).as_bool();
    J::Array rows;
    std::uint64_t total{};
    auto &b = chunks[obj_chunk].bytes;
    for (auto &obj : objects)
    {
        auto name = Monstey_WName(names, Monstey_U32(b, obj.at + 4)),
             kind = Monstey_WName(classes, Monstey_U32(b, obj.at));
        if ((!type.empty() && kind != type) || Monstey_WLower(name).find(query) == std::string::npos)
            continue;
        if (total >= offset && rows.size() < limit)
        {
            auto row = J::object({{"name", name},
                                  {"class_name", kind},
                                  {"group_index", obj.group},
                                  {"object_index", obj.index},
                                  {"property_index", Monstey_U32(b, obj.at + 10)},
                                  {"position", Monstey_WFloats(b, obj.at + 14, 3)},
                                  {"quaternion", Monstey_WFloats(b, obj.at + 26, 4)},
                                  {"prefix_offset", Monstey_U16(b, obj.at + 8)}});
            if (props)
                row["properties"] = db->Fields(Monstey_U32(b, obj.at + 10));
            rows.push_back(std::move(row));
        }
        ++total;
    }
    return J::object({{"total", total},
                      {"offset", offset},
                      {"items", rows},
                      {"next_offset", offset + rows.size() < total ? J(offset + rows.size()) : J{}}});
}

J Monstey_WObjres::Report() const
{
    J::Array cs;
    for (auto &c : chunks)
        cs.push_back(J::object({{"kind", c.kind},
                                {"bytes", static_cast<std::uint64_t>(c.bytes.size())},
                                {"sha256", Monstey_HashSha256(c.bytes)}}));
    return J::object({{"format", "OBJR13/OBJL5/ObjectDB17"},
                      {"bytes", static_cast<std::uint64_t>(raw.size())},
                      {"objects", static_cast<std::uint64_t>(objects.size())},
                      {"groups", groups},
                      {"property_records", static_cast<std::uint64_t>(db->records.size())},
                      {"chunks", cs},
                      {"roundtrip_byte_exact", Encode() == raw},
                      {"opaque_chunks_preserved", true}});
}

B Monstey_WObjres::Encode() const
{
    B out(raw.begin(), raw.begin() + 16);
    for (auto &c : chunks)
    {
        Monstey_AppendU32(out, c.kind);
        Monstey_AppendU32(out, static_cast<std::uint32_t>(c.bytes.size()));
        out.insert(out.end(), c.bytes.begin(), c.bytes.end());
    }
    return out;
}

void Monstey_WObjres::Apply(const J &updates)
{
    std::set<std::string> seen;
    auto &b = chunks[obj_chunk].bytes;
    for (auto &update : updates.as_array())
    {
        auto name = update.at("name").as_string();
        Monstey_Require(seen.insert(name).second, "Duplicate object transform update");
        std::vector<Object> matches;
        for (auto &obj : objects)
            if (Monstey_WName(names, Monstey_U32(b, obj.at + 4)) == name)
                matches.push_back(obj);
        Monstey_Require(matches.size() == 1, "Expected one object by name");
        auto &obj = matches[0];
        if (update.contains("class_name"))
            Monstey_Require(Monstey_WName(classes, Monstey_U32(b, obj.at)) == update.at("class_name").as_string(),
                            "Object class changed");
        auto pos = Monstey_WVector(update.at("position"), 3), q = Monstey_WVector(update.at("quaternion"), 4);
        double norm{};
        for (auto f : q)
            norm += f * f;
        Monstey_Require(norm > 0, "Quaternion cannot be zero");
        for (std::size_t k = 0; k < 3; ++k)
            Monstey_WPutFloat(b, obj.at + 14 + 4 * k, pos[k]);
        for (std::size_t k = 0; k < 4; ++k)
            Monstey_WPutFloat(b, obj.at + 26 + 4 * k, q[k]);
    }
}

} // namespace Monstey_WorldPrivate
using namespace Monstey_WorldPrivate;

Monstey_Bytes Monstey_ObjresReadChunk(std::span<const std::uint8_t> raw, std::uint32_t kind)
{
    Monstey_WObjres doc(B(raw.begin(), raw.end()));
    const B *selected = nullptr;
    for (auto &chunk : doc.chunks)
        if (chunk.kind == kind)
        {
            Monstey_Require(selected == nullptr, "OBJRES chunk kind is ambiguous");
            selected = &chunk.bytes;
        }
    Monstey_Require(selected != nullptr, "OBJRES chunk kind is absent");
    return *selected;
}

Monstey_Bytes Monstey_ObjresReplaceChunk(std::span<const std::uint8_t> raw, std::uint32_t kind,
                                         std::span<const std::uint8_t> replacement)
{
    Monstey_WObjres doc(B(raw.begin(), raw.end()));
    std::size_t selected = doc.chunks.size();
    for (std::size_t j = 0; j < doc.chunks.size(); ++j)
        if (doc.chunks[j].kind == kind)
        {
            Monstey_Require(selected == doc.chunks.size(), "OBJRES replacement chunk kind is ambiguous");
            selected = j;
        }
    Monstey_Require(selected < doc.chunks.size(), "OBJRES replacement chunk kind is absent");
    doc.chunks[selected].bytes = B(replacement.begin(), replacement.end());
    auto result = doc.Encode();
    Monstey_WObjres checked(result);
    Monstey_Require(checked.chunks.size() == doc.chunks.size(), "OBJRES replacement changed chunk count");
    for (std::size_t j = 0; j < doc.chunks.size(); ++j)
        if (j != selected)
            Monstey_Require(checked.chunks[j].kind == doc.chunks[j].kind &&
                                checked.chunks[j].bytes == doc.chunks[j].bytes,
                            "OBJRES replacement changed unrelated chunk");
    return result;
}

Monstey_Json Monstey_ObjresOwners(std::span<const std::uint8_t> raw, bool include_properties)
{
    Monstey_WObjres doc(B(raw.begin(), raw.end()));
    auto &bytes = doc.chunks[doc.obj_chunk].bytes;
    J::Array owners;
    for (auto &obj : doc.objects)
    {
        auto name = Monstey_WName(doc.names, Monstey_U32(bytes, obj.at + 4));
        auto ref = Monstey_U32(bytes, obj.at + 10);
        auto row = J::object({{"name", name},
                              {"name_hash", Monstey_Hex(Monstey_NativeHash(name))},
                              {"class_name", Monstey_WName(doc.classes, Monstey_U32(bytes, obj.at))},
                              {"group_index", obj.group},
                              {"object_index", obj.index},
                              {"property_index", ref},
                              {"position", Monstey_WFloats(bytes, obj.at + 14, 3)},
                              {"quaternion", Monstey_WFloats(bytes, obj.at + 26, 4)},
                              {"prefix_offset", Monstey_U16(bytes, obj.at + 8)}});
        if (include_properties)
        {
            row["properties"] = doc.db->Fields(ref);
            J::Object values, types;
            for (auto &descriptor : doc.db->records[ref].fields)
            {
                auto value = doc.db->Value(doc.db->records[ref], descriptor);
                if (descriptor.type == 10 || descriptor.type == 11)
                {
                    auto scalar = [](const J &input) { return input.is_null() ? J{} : input.at("reference"); };
                    if (descriptor.count == 1)
                        value = scalar(value);
                    else
                    {
                        J::Array refs;
                        for (auto &item : value.as_array())
                            refs.push_back(scalar(item));
                        value = refs;
                    }
                }
                values.emplace(Monstey_Hex(descriptor.hash), value);
                types.emplace(Monstey_Hex(descriptor.hash), descriptor.type);
            }
            row["property_values"] = std::move(values);
            row["property_types"] = std::move(types);
        }
        owners.push_back(std::move(row));
    }
    return owners;
}

std::pair<Monstey_Bytes, Monstey_Json> Monstey_ObjresApply(std::span<const std::uint8_t> raw, const Monstey_Json &spec)
{
    Monstey_WObjres doc(B(raw.begin(), raw.end()));
    auto changes = Monstey_WObjresEdit(doc, spec);
    auto result = doc.Encode();
    Monstey_WObjres checked(result);
    Monstey_Require(checked.Encode() == result, "In-memory object transaction readback differs");
    return {result, changes};
}

} // namespace Monstey
