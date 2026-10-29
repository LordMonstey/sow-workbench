#include "Monstey_GadbPrivate.hpp"

namespace Monstey
{
using namespace Monstey_GadbPrivate;

J Monstey_Gadb::SaveProject(const std::filesystem::path &output) const
{
    impl_->Unchanged();
    Monstey_OutsideGame(output);
    auto document = J::object({{"format", "MONSTEY_CPP_GADB_PROJECT"},
                               {"version", 2},
                               {"source_kind", impl_->memorySource ? "empty" : "file"},
                               {"source", Monstey_Utf8(impl_->source.wstring())},
                               {"source_sha256", impl_->sourceHash},
                               {"schema", Monstey_Utf8(impl_->schemaPath.wstring())},
                               {"schema_sha256", impl_->schemaHash},
                               {"changes", Changes()}});
    document["source_format"] = impl_->wrapper.empty() ? "GADB17" : "LTFX8/GADB17";
    document["source_prefix"] = J::array();
    for (const auto byte : impl_->wrapper)
        document["source_prefix"].as_array().push_back(byte);
    if (impl_->schemaInline)
        document["inline_schema"] = impl_->schema;
    Monstey_WriteNewText(output, document.dump(2) + "\n");
    return J::object({{"project_path", Monstey_Utf8(std::filesystem::absolute(output).wstring())},
                      {"source_sha256", impl_->sourceHash}});
}

std::unique_ptr<Monstey_Gadb> Monstey_Gadb::LoadProject(const std::filesystem::path &path)
{
    const auto doc = J::Parse(Monstey_ReadText(path));
    const auto version = doc.at("version").as_int();
    Monstey_Require(doc.at("format").as_string() == "MONSTEY_CPP_GADB_PROJECT" && (version == 1 || version == 2),
                    "Unsupported C++ GADB project");
    const auto source = std::filesystem::path(Monstey_Wide(doc.at("source").as_string()));
    const auto schemaPath = std::filesystem::path(Monstey_Wide(doc.at("schema").as_string()));
    const bool inlineSchema = doc.contains("inline_schema"),
               empty = version == 2 && doc.value("source_kind", J("file")).as_string() == "empty";
    Monstey_Require(!inlineSchema || schemaPath.empty(), "Project declares conflicting inline/file schemas");
    Monstey_Require(!empty || source.empty(), "Empty source project must not declare a source path");
    auto project = empty          ? New(schemaPath)
                   : inlineSchema ? WithSchema(source, doc.at("inline_schema"))
                                  : std::make_unique<Monstey_Gadb>(source, schemaPath);
    if (empty && doc.value("source_format", J("GADB17")).as_string() == "LTFX8/GADB17")
    {
        B prefix;
        Monstey_Require(doc.contains("source_prefix") && doc.at("source_prefix").size() == 8,
                        "Empty FX source prefix missing");
        for (const auto &byte : doc.at("source_prefix").as_array())
        {
            Monstey_Require(byte.as_uint() <= 255, "Invalid FX prefix byte");
            prefix.push_back(static_cast<std::uint8_t>(byte.as_uint()));
        }
        project = NewFx(prefix, schemaPath);
    }
    if (empty && inlineSchema)
    {
        auto raw = project->impl_->wrapper;
        raw.insert(raw.end(), project->impl_->data.begin(), project->impl_->data.end());
        project->impl_ =
            std::make_unique<Monstey_Impl>(std::filesystem::path{}, schemaPath, &doc.at("inline_schema"), &raw);
    }
    auto &p = *project->impl_;
    Monstey_Require(p.sourceHash == doc.at("source_sha256").as_string() &&
                        p.schemaHash == doc.at("schema_sha256").as_string(),
                    "Project source or schema hash changed");
    const auto &changes = doc.at("changes");
    if (version == 2)
    {
        Monstey_Require(changes.at("categories").size() <= 4096 && changes.at("records").size() <= 1000000 &&
                            changes.at("clones").size() <= 1000000,
                        "Project structure limit exceeded");
        for (const auto &c : changes.at("categories").as_array())
            project->CreateCategory(c.at("schema").as_string(), c.at("name").as_string(), c.at("path").as_string());
        std::vector<std::pair<std::uint32_t, const J *>> creations;
        for (const auto &row : changes.at("records").as_array())
            creations.emplace_back(Monstey_ParseRef(row.at("reference").as_string()), &row);
        for (const auto &row : changes.at("clones").as_array())
            creations.emplace_back(Monstey_ParseRef(row.at("reference").as_string()), &row);
        std::sort(creations.begin(), creations.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        for (const auto &[ref, row] : creations)
        {
            Monstey_Require(p.NextReference(ref >> 20) == ref,
                            "Project has duplicate or noncontiguous authored reference");
            if (row->contains("id"))
                project->CloneRecord(row->at("source").as_string(), row->at("id").as_string());
            else
                project->CreateRecord(ref >> 20, row->at("name").as_string(), row->at("anonymous").as_bool());
        }
        auto loadPayload = [&](const J &encoded)
        {
            Monstey_Payload payload;
            Monstey_Require(encoded.at("words").size() <= 65535 && encoded.at("descriptors").size() <= 255,
                            "Project payload exceeds native limits");
            for (const auto &word : encoded.at("words").as_array())
            {
                Monstey_Require(word.as_uint() <= 0xffffffffu, "Project value exceeds uint32");
                payload.words.push_back(static_cast<std::uint32_t>(word.as_uint()));
            }
            for (const auto &row : encoded.at("descriptors").as_array())
            {
                Monstey_Require(row.size() == 7, "Invalid project descriptor");
                const std::array<std::uint64_t, 7> maximum = {0xffffffffu, 0xffffffffu, 255, 255, 255, 65535, 65535};
                for (std::size_t i = 0; i < 7; ++i)
                    Monstey_Require(row.at(i).as_uint() <= maximum[i], "Project descriptor integer overflow");
                Monstey_Descriptor d{
                    static_cast<std::uint32_t>(row.at(0).as_uint()), static_cast<std::uint32_t>(row.at(1).as_uint()),
                    static_cast<std::uint8_t>(row.at(2).as_uint()),  static_cast<std::uint8_t>(row.at(3).as_uint()),
                    static_cast<std::uint8_t>(row.at(4).as_uint()),  static_cast<std::uint16_t>(row.at(5).as_uint()),
                    static_cast<std::uint16_t>(row.at(6).as_uint())};
                Monstey_Require(d.type == 1 ? (d.count == 1 && d.offset < payload.words.size() * 32)
                                            : static_cast<std::size_t>(d.offset) + d.count <= payload.words.size(),
                                "Project field exceeds payload");
                payload.descriptors.push_back(d);
            }
            const auto saved = payload.descriptors;
            p.DescriptorOrder(payload);
            for (std::size_t i = 0; i < saved.size(); ++i)
            {
                // Native records can omit class fields/group markers, leaving ordinal gaps.
                // The lookup proof is sorted hash plus sorted-to-physical index, not ordinal=i.
                Monstey_Require(saved[i].sorted == payload.descriptors[i].sorted &&
                                    saved[i].index == payload.descriptors[i].index,
                                "Project descriptor lookup is invalid");
                payload.descriptors[i].ordinal = saved[i].ordinal;
            }
            for (const auto &[hash, f] : encoded.at("metadata").as_object())
            {
                const auto h = Monstey_ParseRef(hash);
                Monstey_Require(f.is_object() && f.contains("name") &&
                                    Monstey_NativeHash(f.at("name").as_string()) == h,
                                "Project field metadata hash mismatch");
                payload.metadata[h] = f;
            }
            for (const auto &hash : encoded.at("introduced").as_array())
            {
                const auto h = Monstey_ParseRef(hash.as_string());
                Monstey_Require(payload.metadata.contains(h), "Authored field has no metadata");
                payload.introduced.insert(h);
            }
            return payload;
        };
        for (const auto &row : changes.at("clones").as_array())
        {
            const auto ref = Monstey_ParseRef(row.at("reference").as_string());
            auto c = const_cast<Monstey_Clone *>(p.Clone(ref));
            Monstey_Require(c && c->id == row.at("id").as_string(), "Project clone identity mismatch");
            auto snapshot = c->snapshot;
            auto restored = loadPayload(row.at("snapshot"));
            const bool sameNative = !snapshot.payload && restored.metadata.empty() && restored.introduced.empty() &&
                                    restored.words == p.Words(snapshot) &&
                                    restored.descriptors == p.Descriptors(snapshot);
            if (row.contains("snapshot_native_layout"))
                Monstey_Require(row.at("snapshot_native_layout").is_bool(),
                                "Clone snapshot layout marker must be Boolean");
            const bool nativeSnapshot =
                row.contains("snapshot_native_layout") ? !row.at("snapshot_native_layout").as_bool() : sameNative;
            if (nativeSnapshot)
                Monstey_Require(sameNative, "Native clone snapshot differs from its immutable source payload");
            else
            {
                snapshot.payload = std::make_shared<const Monstey_Payload>(std::move(restored));
                snapshot.packed = static_cast<std::uint32_t>(snapshot.payload->words.size()) |
                                  static_cast<std::uint32_t>(snapshot.payload->descriptors.size()) << 16;
            }
            c->snapshot = std::move(snapshot);
        }
        for (const auto &row : changes.at("layouts").as_array())
            p.Install(p.Select(row.at("record").as_string()), loadPayload(row.at("payload")));
    }
    else
        for (const auto &c : changes.at("clones").as_array())
            project->CloneRecord(c.at("source").as_string(), c.at("id").as_string());
    for (const auto &c : changes.at("clones").as_array())
        for (const auto &[hash, value] : c.at("fields").as_object())
            project->SetField(c.at("id").as_string(), hash, value);
    for (const auto &edit : changes.at("patches").as_array())
        for (const auto &[hash, value] : edit.at("fields").as_object())
            project->SetField(edit.at("record").as_string(), hash, value);
    for (const auto &t : changes.at("transforms").as_array())
        if (t.at("operation").as_string() == "rename")
            project->RenameRecord(t.at("record").as_string(), t.at("name").as_string());
        else
            project->DeleteRecord(t.at("record").as_string(), t.at("replacement"));
    for (std::uint32_t ci = 0; ci < p.CategoryCount(); ++ci)
        for (const auto ref : p.References(ci))
        {
            const auto &record = p.Base(ref);
            if (record.payload)
                for (const auto hash : record.payload->introduced)
                    Monstey_Require(p.Edits(ref).contains(hash), "Authored field has no typed value");
        }
    if (version == 2)
        project->SetProjectMetadata(changes.value("editor_metadata", J::object()));
    p.undo.clear();
    p.redo.clear();
    return project;
}

} // namespace Monstey
