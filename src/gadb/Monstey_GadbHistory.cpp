#include "Monstey_GadbPrivate.hpp"

namespace Monstey
{
using namespace Monstey_GadbPrivate;

void Monstey_Gadb::Monstey_Impl::Commit()
{
    undo.push_back(state);
    redo.clear();
}

J Monstey_Gadb::ProjectMetadata() const
{
    impl_->Unchanged();
    return impl_->state.editorMetadata;
}

J Monstey_Gadb::SetProjectMetadata(const J &metadata)
{
    auto &p = *impl_;
    p.Unchanged();
    Monstey_Require(metadata.is_object(), "Project metadata must be an object");
    Monstey_Require(metadata.dump().size() <= 16u * 1024u * 1024u, "Project metadata exceeds the 16 MiB limit");
    if (p.state.editorMetadata != metadata)
    {
        p.Commit();
        p.state.editorMetadata = metadata;
    }
    return Describe();
}

J Monstey_Gadb::Transaction(const std::function<void(Monstey_Gadb &)> &action)
{
    auto &p = *impl_;
    p.Unchanged();
    Monstey_Require(static_cast<bool>(action), "Transaction callback is empty");
    const auto before = p.state;
    const auto undo = p.undo, redo = p.redo;
    const auto changes = Changes();
    try
    {
        action(*this);
        p.Unchanged();
        const bool changed = Changes() != changes;
        p.undo = undo;
        p.redo = redo;
        if (changed)
        {
            p.undo.push_back(before);
            p.redo.clear();
        }
    }
    catch (...)
    {
        p.state = before;
        p.undo = undo;
        p.redo = redo;
        throw;
    }
    return Describe();
}

J Monstey_Gadb::Undo()
{
    auto &p = *impl_;
    Monstey_Require(!p.undo.empty(), "Nothing to undo");
    p.redo.push_back(p.state);
    p.state = std::move(p.undo.back());
    p.undo.pop_back();
    return Describe();
}

J Monstey_Gadb::Redo()
{
    auto &p = *impl_;
    Monstey_Require(!p.redo.empty(), "Nothing to redo");
    p.undo.push_back(p.state);
    p.state = std::move(p.redo.back());
    p.redo.pop_back();
    return Describe();
}

J Monstey_Gadb::Revert()
{
    auto &p = *impl_;
    p.Commit();
    p.state = {};
    return Describe();
}

J Monstey_Gadb::Changes() const
{
    const auto &p = *impl_;
    J edits = J::array(), clones = J::array(), transforms = J::array();
    auto fields = [](const auto &values)
    {
        J out = J::object();
        for (const auto &[hash, v] : values)
            out[Monstey_Hex(hash)] = v;
        return out;
    };
    for (const auto &[ref, values] : p.state.edits)
        edits.as_array().push_back(J::object({{"record", Monstey_Hex(ref)}, {"fields", fields(values)}}));
    for (const auto &c : p.state.clones)
        clones.as_array().push_back(J::object({{"id", c.id},
                                               {"source", Monstey_Hex(c.source)},
                                               {"reference", Monstey_Hex(c.reference)},
                                               {"fields", fields(c.edits)}}));
    for (const auto &[ref, name] : p.state.renames)
        transforms.as_array().push_back(
            J::object({{"operation", "rename"}, {"record", Monstey_Hex(ref)}, {"name", name}}));
    for (const auto &[ref, replacement] : p.state.deletes)
        transforms.as_array().push_back(
            J::object({{"operation", "delete"}, {"record", Monstey_Hex(ref)}, {"replacement", replacement}}));
    auto payload = [&](const Monstey_RecordData &record)
    {
        J words = J::array(), descriptors = J::array(), metadata = J::object(), introduced = J::array();
        for (const auto word : p.Words(record))
            words.as_array().push_back(word);
        for (const auto &d : p.Descriptors(record))
            descriptors.as_array().push_back(
                J::array({d.hash, d.sorted, d.ordinal, d.index, d.type, d.count, d.offset}));
        if (record.payload)
        {
            for (const auto &[hash, f] : record.payload->metadata)
                metadata[Monstey_Hex(hash)] = f;
            for (const auto hash : record.payload->introduced)
                introduced.as_array().push_back(Monstey_Hex(hash));
        }
        return J::object(
            {{"words", words}, {"descriptors", descriptors}, {"metadata", metadata}, {"introduced", introduced}});
    };
    J categories = J::array(), records = J::array(), layouts = J::array();
    for (const auto &c : p.state.categories)
        categories.as_array().push_back(J::object({{"schema", c.schema}, {"name", c.name}, {"path", c.path}}));
    for (const auto &r : p.state.records)
        records.as_array().push_back(J::object({{"reference", Monstey_Hex(r.reference)},
                                                {"category", r.reference >> 20},
                                                {"name", r.name},
                                                {"anonymous", !r.record.named}}));
    for (const auto &[ref, record] : p.state.layouts)
        layouts.as_array().push_back(J::object({{"record", Monstey_Hex(ref)}, {"payload", payload(record)}}));
    for (std::size_t i = 0; i < p.state.clones.size(); ++i)
    {
        clones[i]["snapshot"] = payload(p.state.clones[i].snapshot);
        clones[i]["snapshot_native_layout"] = p.state.clones[i].snapshot.payload != nullptr;
    }
    auto referenceOrder = [](const J &a, const J &b)
    { return Monstey_ParseRef(a.at("reference").as_string()) < Monstey_ParseRef(b.at("reference").as_string()); };
    std::sort(records.as_array().begin(), records.as_array().end(), referenceOrder);
    std::sort(clones.as_array().begin(), clones.as_array().end(), referenceOrder);
    return J::object({{"patches", edits},
                      {"clones", clones},
                      {"transforms", transforms},
                      {"categories", categories},
                      {"records", records},
                      {"layouts", layouts},
                      {"editor_metadata", p.state.editorMetadata}});
}

} // namespace Monstey
