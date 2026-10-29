#include "Monstey_GadbPrivate.hpp"

namespace Monstey
{
using namespace Monstey_GadbPrivate;

bool Monstey_Gadb::Monstey_Impl::Allowed(std::string_view required, std::string actual) const
{
    std::string resolved(required);
    if (schema.contains("reference_aliases"))
    {
        const auto &aliases = schema.at("reference_aliases");
        std::size_t depth = 0;
        while (aliases.contains(resolved))
        {
            Monstey_Require(++depth <= 64, "Reference alias depth exceeded");
            resolved = aliases.at(resolved).as_string();
        }
    }
    required = resolved;
    std::set<std::string> seen;
    for (;;)
    {
        if (actual == required)
            return true;
        if (!seen.insert(actual).second)
            return false;
        if (!schema.contains("categories") || !schema.at("categories").contains(actual))
            return false;
        const auto &c = schema.at("categories").at(actual);
        if (!c.contains("parent") || !c.at("parent").is_string())
            return false;
        actual = c.at("parent").as_string();
    }
}

std::uint32_t Monstey_Gadb::Monstey_Impl::ValueRef(const J &value) const
{
    if (value.is_null())
        return 0xffffffffu;
    if (value.is_integer())
    {
        const auto n = value.as_uint();
        Monstey_Require(n <= 0xffffffffu, "Reference outside uint32");
        if (n != 0xffffffffu)
            Base(static_cast<std::uint32_t>(n));
        return static_cast<std::uint32_t>(n);
    }
    if (value.is_object())
    {
        if (value.contains("record"))
            return ValueRef(value.at("record"));
        if (value.contains("clone"))
            return ValueRef(value.at("clone"));
    }
    Monstey_Require(value.is_string(), "Reference needs null, selector or reference");
    if (value.as_string() == "0xffffffff")
        return 0xffffffffu;
    return Select(value.as_string());
}

J Monstey_Gadb::Dependencies() const
{
    const auto &p = *impl_;
    p.Unchanged();
    J resources = J::array();
    auto add = [&](std::uint32_t ref)
    {
        const auto meta = p.Metadata(ref);
        for (const auto &field : meta.at("fields").as_array())
        {
            const auto type = field.at("native_type").as_int();
            if (type != 4 && type != 68)
                continue;
            auto value = [&](const J &item)
            {
                if (!item.is_string())
                    return;
                const auto lower = Monstey_Lower(item.as_string());
                bool resource = false;
                for (const auto ext : {".fxp", ".mat", ".mesh", ".skel", ".tex", ".texa", ".txanm", ".bndl", ".embb"})
                    if (lower.ends_with(ext))
                        resource = true;
                if (resource)
                    resources.as_array().push_back(J::object({{"path", item},
                                                              {"record", meta.at("selector")},
                                                              {"category", meta.at("category")},
                                                              {"field", field.at("name")},
                                                              {"field_hash", field.at("hash")}}));
            };
            if (field.at("value").is_array())
                for (const auto &item : field.at("value").as_array())
                    value(item);
            else
                value(field.at("value"));
        }
    };
    for (const auto &category : p.categories)
        for (const auto &record : category.records)
            add(record.reference);
    for (const auto &clone : p.state.clones)
        add(clone.reference);
    for (const auto &record : p.state.records)
        add(record.reference);
    return J::object({{"resources", resources},
                      {"count", resources.size()},
                      {"recursive_assets_loaded", false},
                      {"runtime_verified", false}});
}

J Monstey_Gadb::IncomingReferences(std::string_view selector, std::size_t offset, std::size_t limit) const
{
    const auto &p = *impl_;
    p.Unchanged();
    Monstey_Require(limit <= 10000, "Incoming reference page limit exceeds10000");
    const auto target = p.Select(selector);
    J rows = J::array();
    std::size_t total = 0;
    for (std::uint32_t ci = 0; ci < p.CategoryCount(); ++ci)
        for (const auto owner : p.References(ci))
        {
            if (p.state.deletes.contains(owner))
                continue;
            const auto &record = p.Base(owner);
            const auto &edits = p.Edits(owner);
            for (const auto &descriptor : p.Descriptors(record))
            {
                if (descriptor.type != 10 && descriptor.type != 11)
                    continue;
                const auto append = [&](std::uint32_t value, std::size_t index)
                {
                    if (value != target)
                        return;
                    if (total++ < offset || rows.size() >= limit)
                        return;
                    const auto &metadata = p.SchemaField(owner, descriptor.hash);
                    const auto label = metadata.contains("name")                ? metadata.at("name").as_string()
                                       : p.fieldNames.contains(descriptor.hash) ? p.fieldNames.at(descriptor.hash)
                                                                                : Monstey_Hex(descriptor.hash);
                    rows.as_array().push_back(J::object(
                        {{"record", p.Clone(owner) ? p.Clone(owner)->id : Monstey_Hex(owner)},
                         {"reference", Monstey_Hex(owner)},
                         {"category", p.CategoryText(ci, 1)},
                         {"category_schema", p.CategoryText(ci, 0)},
                         {"field", label},
                         {"field_hash", Monstey_Hex(descriptor.hash)},
                         {"native_type", descriptor.type},
                         {"index", p.Array(descriptor, metadata) ? J(static_cast<std::uint64_t>(index)) : J()}}));
                };
                if (const auto changed = edits.find(descriptor.hash); changed != edits.end())
                {
                    const auto &value = changed->second;
                    const auto &metadata = p.SchemaField(owner, descriptor.hash);
                    if (p.Array(descriptor, metadata))
                        for (std::size_t index = 0; index < value.size(); ++index)
                            append(p.ValueRef(value.at(index)), index);
                    else
                        append(p.ValueRef(value), 0);
                }
                else
                    for (std::size_t index = 0; index < descriptor.count; ++index)
                    {
                        const auto word = static_cast<std::size_t>(descriptor.offset) + index;
                        const auto value = record.payload ? record.payload->words.at(word)
                                                          : Monstey_U32(p.data, record.values + word * 4);
                        if (value != 0xffffffffu)
                            (void)p.Base(value);
                        append(value, index);
                    }
            }
        }
    p.Unchanged();
    return J::object({{"reference", Monstey_Hex(target)},
                      {"rows", rows},
                      {"total", total},
                      {"offset", offset},
                      {"limit", limit},
                      {"source_sha256", p.sourceHash},
                      {"schema_sha256", p.schemaHash},
                      {"effective_typed_references", true}});
}

} // namespace Monstey
