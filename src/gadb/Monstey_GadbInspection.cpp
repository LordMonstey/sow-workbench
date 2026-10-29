#include "Monstey_GadbPrivate.hpp"

namespace Monstey
{
using namespace Monstey_GadbPrivate;

J Monstey_Gadb::Describe() const
{
    const auto &p = *impl_;
    std::size_t changes = p.state.clones.size() + p.state.renames.size() + p.state.deletes.size() +
                          p.state.categories.size() + p.state.records.size() + p.state.layouts.size();
    for (const auto &[ref, fields] : p.state.edits)
    {
        (void)ref;
        changes += fields.size();
    }
    for (const auto &c : p.state.clones)
        changes += c.edits.size();
    const auto nativeChanges = changes;
    const bool metadataDirty = !p.state.editorMetadata.empty();
    if (metadataDirty)
        ++changes;
    return J::object({{"source", Monstey_Utf8(p.source.wstring())},
                      {"source_sha256", p.sourceHash},
                      {"schema_path", Monstey_Utf8(p.schemaPath.wstring())},
                      {"schema_sha256", p.schemaHash},
                      {"schema_inline", p.schemaInline},
                      {"version", 17},
                      {"format", p.wrapper.empty() ? "GADB17" : "LTFX8/GADB17"},
                      {"container_version", p.wrapper.empty() ? 17 : 8},
                      {"bytes", static_cast<std::uint64_t>(p.sourceSize)},
                      {"category_count", p.CategoryCount()},
                      {"source_kind", p.memorySource ? "empty" : "file"},
                      {"named_count", p.namedCount},
                      {"anonymous_count", p.anonymousCount},
                      {"descriptor_count", p.descriptorCount},
                      {"dirty", changes != 0},
                      {"native_dirty", nativeChanges != 0},
                      {"native_changes_count", nativeChanges},
                      {"editor_metadata_dirty", metadataDirty},
                      {"changes_count", changes},
                      {"can_undo", !p.undo.empty()},
                      {"can_redo", !p.redo.empty()},
                      {"clone_count", p.state.clones.size()},
                      {"capabilities", J::object({{"typed_edit", true},
                                                  {"clones", true},
                                                  {"rename_record", true},
                                                  {"delete_record", true},
                                                  {"create_category", true},
                                                  {"create_record", true},
                                                  {"add_field", true},
                                                  {"delete_field", true},
                                                  {"move_field", true},
                                                  {"resize_utf16_text_arrays", true},
                                                  {"build_new_database", true},
                                                  {"native_cpp", true},
                                                  {"runtime_verified", false}})}});
}

J Monstey_Gadb::Categories(std::string_view query) const
{
    const auto &p = *impl_;
    p.Unchanged();
    J rows = J::array();
    for (std::uint32_t ci = 0; ci < p.CategoryCount(); ++ci)
    {
        const auto name = p.CategoryText(ci, 1), schema = p.CategoryText(ci, 0);
        if (!Monstey_Has(name, query) && !Monstey_Has(schema, query))
            continue;
        const auto refs = p.References(ci);
        std::size_t anonymous = 0;
        for (const auto ref : refs)
            if (!p.Base(ref).named)
                ++anonymous;
        rows.as_array().push_back(J::object({{"index", ci},
                                             {"category_index", ci},
                                             {"name", name},
                                             {"schema", schema},
                                             {"path", p.CategoryText(ci, 2)},
                                             {"anonymous_count", anonymous},
                                             {"named_count", refs.size() - anonymous},
                                             {"records", refs.size()},
                                             {"hash", Monstey_Hex(Monstey_NativeHash(schema))},
                                             {"pending_create", ci >= p.categories.size()}}));
    }
    return J::object({{"rows", rows}, {"total", rows.size()}, {"offset", 0}, {"limit", rows.size()}});
}

J Monstey_Gadb::Records(std::uint32_t category, std::string_view query, std::size_t offset, std::size_t limit) const
{
    const auto &p = *impl_;
    p.Unchanged();
    Monstey_Require(category < p.CategoryCount() && limit <= 10000, "Invalid category or page limit");
    J rows = J::array();
    std::size_t total = 0;
    auto add = [&](std::uint32_t ref, bool named, bool clone, std::uint32_t count)
    {
        const auto name = p.Name(ref);
        if (!Monstey_Has(name, query) && !Monstey_Has(Monstey_Hex(ref), query))
            return;
        const auto index = total++;
        if (index < offset || rows.size() >= limit)
            return;
        rows.as_array().push_back(
            J::object({{"reference", clone ? J(p.Clone(ref)->id) : J(Monstey_Hex(ref))},
                       {"native_reference", Monstey_Hex(ref)},
                       {"name", Monstey_DisplayUtf8(name)},
                       {"name_hash", named || clone ? J(Monstey_Hex(Monstey_NativeHash(name))) : J()},
                       {"named", named},
                       {"ordinal", ref & 0xfffffu},
                       {"category_index", category},
                       {"clone", clone},
                       {"pending_create", p.Created(ref) != nullptr},
                       {"value_count", count},
                       {"pending_delete", p.state.deletes.contains(ref)}}));
    };
    for (const auto ref : p.References(category))
        add(ref, p.Base(ref).named, p.Clone(ref) != nullptr, static_cast<std::uint32_t>(p.Words(p.Base(ref)).size()));
    return J::object({{"rows", rows}, {"total", total}, {"offset", offset}, {"limit", limit}});
}

J Monstey_Gadb::DescriptorLayouts(std::uint32_t category, std::size_t offset, std::size_t limit) const
{
    const auto &p = *impl_;
    p.Unchanged();
    Monstey_Require(category < p.CategoryCount() && limit <= 10000, "Invalid descriptor category or page limit");
    Monstey_Require(offset <= static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()),
                    "Descriptor page offset exceeds JSON integer range");
    const auto references = p.References(category);
    const auto begin = std::min(offset, references.size());
    const auto end = begin + std::min(limit, references.size() - begin);
    J rows = J::array();
    for (auto i = begin; i < end; ++i)
    {
        const auto ref = references[i];
        const auto &record = p.Base(ref);
        const auto *clone = p.Clone(ref);
        const auto name = p.Name(ref);
        const bool named = record.named || clone != nullptr;
        J fields = J::array();
        for (const auto &descriptor : p.Descriptors(record))
            fields.as_array().push_back(J::object({{"hash", Monstey_Hex(descriptor.hash)},
                                                   {"native_ordinal", descriptor.ordinal},
                                                   {"native_type", descriptor.type},
                                                   {"count", descriptor.count}}));
        rows.as_array().push_back(J::object({{"reference", Monstey_Hex(ref)},
                                             {"selector", clone ? J(clone->id) : J(Monstey_Hex(ref))},
                                             {"name", Monstey_DisplayUtf8(name)},
                                             {"name_hash", named ? J(Monstey_Hex(Monstey_NativeHash(name))) : J()},
                                             {"named", named},
                                             {"anonymous", !named},
                                             {"clone", clone != nullptr},
                                             {"pending_create", p.Created(ref) != nullptr},
                                             {"pending_delete", p.state.deletes.contains(ref)},
                                             {"fields", fields}}));
    }
    const auto description = Describe();
    const bool nativeDirty = description["native_dirty"].as_bool();
    return J::object(
        {{"rows", rows},
         {"total", references.size()},
         {"offset", offset},
         {"limit", limit},
         {"category_index", category},
         {"name", p.CategoryText(category, 1)},
         {"schema", p.CategoryText(category, 0)},
         {"source_sha256", p.sourceHash},
         {"layout_stage", "current_record_snapshot"},
         {"name_hash_stage", "current_raw_name_bytes"},
         {"record_order", !p.memorySource && !nativeDirty ? "native_source_order" : "project_reference_order"},
         {"published_order_provenance", !p.memorySource && !nativeDirty ? "native_source" : "not_computed"},
         {"has_pending_native_changes", nativeDirty},
         {"values_decoded", false},
         {"publication_plan_computed", false}});
}

J Monstey_Gadb::FxRecords(std::string_view query, std::size_t offset, std::size_t limit) const
{
    const auto &p = *impl_;
    p.Unchanged();
    Monstey_Require(limit <= 10000, "FX page limit exceeds 10000");
    J rows = J::array();
    std::size_t total = 0;
    auto add = [&](std::uint32_t ref)
    {
        const auto meta = p.Metadata(ref);
        std::string key;
        for (const auto &field : meta.at("fields").as_array())
            if (field.at("name").as_string() == "KeyName" && field.at("value").is_string())
                key = field.at("value").as_string();
        const auto text =
            meta.at("category").as_string() + " " + meta.at("name").as_string() + " " + key + " " + Monstey_Hex(ref);
        if (!Monstey_Has(text, query))
            return;
        const auto index = total++;
        if (index < offset || rows.size() >= limit)
            return;
        rows.as_array().push_back(J::object({{"reference", meta.at("selector")},
                                             {"name", meta.at("name")},
                                             {"category", meta.at("category")},
                                             {"key_name", key},
                                             {"field_count", meta.at("fields").size()}}));
    };
    for (const auto &category : p.categories)
        for (const auto &record : category.records)
            add(record.reference);
    for (const auto &clone : p.state.clones)
        add(clone.reference);
    for (const auto &record : p.state.records)
        add(record.reference);
    return J::object({{"rows", rows}, {"items", rows}, {"total", total}, {"offset", offset}, {"limit", limit}});
}

J Monstey_Gadb::Record(std::string_view selector) const
{
    return Metadata(selector);
}

J Monstey_Gadb::Metadata(std::string_view selector) const
{
    impl_->Unchanged();
    return impl_->Metadata(impl_->Select(selector));
}

} // namespace Monstey
