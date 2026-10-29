#include "Monstey_GadbPrivate.hpp"

namespace Monstey
{
using namespace Monstey_GadbPrivate;

J Monstey_Gadb::Build(const std::filesystem::path &output) const
{
    return BuildImpl(output, nullptr);
}

J Monstey_Gadb::Build(const std::filesystem::path &output, const Monstey_GadbClassCache &declarations) const
{
    return BuildImpl(output, &declarations);
}

J Monstey_Gadb::BuildImpl(const std::filesystem::path &output, const Monstey_GadbClassCache *declarations) const
{
    const auto &p = *impl_;
    p.Unchanged();
    Monstey_OutsideGame(output);
    const auto absolute = std::filesystem::absolute(output);
    Monstey_Require(absolute != p.source && !std::filesystem::exists(Monstey_WindowsFilePath(absolute)),
                    "Build needs a new output distinct from its source");
    Monstey_Require(absolute.extension() == (p.wrapper.empty() ? ".gamedb" : ".fxp"),
                    p.wrapper.empty() ? "Use a .gamedb output filename" : "Use a .fxp output filename");
    const auto reportPath = std::filesystem::path(absolute.wstring() + L".report.json"),
               mapPath = std::filesystem::path(absolute.wstring() + L".namemap.json");
    Monstey_Require(!std::filesystem::exists(Monstey_WindowsFilePath(reportPath)) &&
                        !std::filesystem::exists(Monstey_WindowsFilePath(mapPath)),
                    "Build sidecar already exists");
    struct Built
    {
        std::uint32_t oldref = 0, ref = 0;
        const Monstey_RecordData *source = nullptr;
        std::string name;
        std::vector<std::uint32_t> words;
        std::vector<Monstey_Descriptor> ds;
        std::uint32_t descriptorStart = 0;
        bool named = false, layout = false, clone = false;
    };
    std::vector<std::vector<Built>> groups(p.CategoryCount());
    B atoms;
    Monstey_Append(atoms, p.data, p.stringBase, p.stringBase + p.stringSize);
    B extraDescriptors;
    std::map<std::string, std::uint32_t> interned;
    std::size_t utf16Added = 0;
    auto atom = [&](const J &value, std::uint8_t type) -> std::uint32_t
    {
        B encoded;
        std::string key;
        if (type == 12 || type == 13 || type == 15 || type == 16)
        {
            const auto &knots = value.at("knots").as_array();
            Monstey_AppendU32(encoded, static_cast<std::uint32_t>(knots.size()));
            for (const auto &knot : knots)
            {
                Monstey_AppendU32(encoded,
                                  std::bit_cast<std::uint32_t>(static_cast<float>(knot.at("time").as_number())));
                const auto &v = knot.at("value");
                if (type == 12)
                    Monstey_AppendU32(encoded, static_cast<std::uint32_t>(v.as_uint()));
                else if (type == 13)
                    Monstey_AppendU32(encoded, std::bit_cast<std::uint32_t>(static_cast<float>(v.as_number())));
                else if (type == 15)
                    Monstey_AppendU32(encoded, static_cast<std::uint32_t>(v.as_int()));
                else
                    for (const auto &component : v.as_array())
                        Monstey_AppendU32(encoded,
                                          std::bit_cast<std::uint32_t>(static_cast<float>(component.as_number())));
            }
            key = "c" + std::to_string(type);
        }
        else if (type >= 7 && type <= 9)
        {
            for (const auto &f : value.as_array())
                Monstey_AppendU32(encoded, std::bit_cast<std::uint32_t>(static_cast<float>(f.as_number())));
            key = "v";
        }
        else if (type == 6)
        {
            const auto text = Monstey_Wide(value.as_string());
            for (const auto wc : text)
            {
                const auto c = static_cast<std::uint32_t>(wc);
                if constexpr (sizeof(wchar_t) == 2)
                    Monstey_AppendU16(encoded, static_cast<std::uint16_t>(c));
                else if (c < 0x10000)
                    Monstey_AppendU16(encoded, static_cast<std::uint16_t>(c));
                else
                {
                    Monstey_AppendU16(encoded, static_cast<std::uint16_t>(0xd800 + ((c - 0x10000) >> 10)));
                    Monstey_AppendU16(encoded, static_cast<std::uint16_t>(0xdc00 + ((c - 0x10000) & 1023)));
                }
            }
            Monstey_AppendU16(encoded, 0);
            key = "w";
        }
        else
        {
            const auto &text = value.as_string();
            encoded.assign(text.begin(), text.end());
            encoded.push_back(0);
            key = "s";
        }
        key.append(reinterpret_cast<const char *>(encoded.data()), encoded.size());
        if (const auto it = interned.find(key); it != interned.end())
            return it->second;
        Monstey_Aligned(atoms);
        Monstey_Require(atoms.size() <= 0xffffffffu && encoded.size() <= 0xffffffffu - atoms.size(),
                        "Atom pool exceeds uint32");
        const auto pointer = static_cast<std::uint32_t>(atoms.size());
        atoms.insert(atoms.end(), encoded.begin(), encoded.end());
        Monstey_Aligned(atoms);
        interned.emplace(std::move(key), pointer);
        if (type == 6)
            ++utf16Added;
        return pointer;
    };
    auto encode = [&](const J &value, std::uint8_t type) -> std::uint32_t
    {
        switch (type)
        {
        case 2:
            return std::bit_cast<std::uint32_t>(static_cast<float>(value.as_number()));
        case 3:
            return static_cast<std::uint32_t>(value.as_int());
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 12:
        case 13:
        case 15:
        case 16:
        case 68:
            return atom(value, type);
        case 10:
        case 11:
            return p.ValueRef(value);
        default:
            throw Monstey_Error("gadb_type", "Unsupported field encoding");
        }
    };
    auto make = [&](std::uint32_t ref, bool clone, std::string name) -> Built
    {
        const auto &r = p.Base(ref);
        Built out;
        out.oldref = ref;
        out.source = &r;
        out.name = std::move(name);
        out.named = r.named || clone;
        out.clone = clone;
        out.layout = r.payload != nullptr;
        out.words = p.Words(r);
        out.ds = p.Descriptors(r);
        out.descriptorStart = ((r.flags & 65535u) << 8) | (r.packed >> 24);
        const auto &edits = p.Edits(ref);
        for (const auto &[hash, value] : edits)
        {
            const auto found =
                std::find_if(out.ds.begin(), out.ds.end(), [&](const auto &d) { return d.hash == hash; });
            Monstey_Require(found != out.ds.end(), "Edited descriptor disappeared");
            const auto index = static_cast<std::size_t>(found - out.ds.begin());
            const auto d = *found;
            const auto &f = p.SchemaField(ref, hash);
            if (d.type == 1)
            {
                auto &word = out.words[d.offset / 32];
                const auto bit = 1u << (d.offset % 32);
                word = value.as_bool() ? word | bit : word & ~bit;
                continue;
            }
            std::vector<std::uint32_t> values;
            if (p.Array(d, f))
                for (const auto &v : value.as_array())
                    values.push_back(encode(v, d.type));
            else
                values.push_back(encode(value, d.type));
            if (values.size() == d.count)
            {
                std::copy(values.begin(), values.end(), out.words.begin() + d.offset);
                continue;
            }
            Monstey_Require(p.Variable(d, f), "Fixed native array changed length");
            std::size_t offset = d.count ? d.offset : out.words.size();
            std::size_t boolWords = 0;
            for (const auto &other : out.ds)
                if (other.type == 1)
                    boolWords = std::max(boolWords, static_cast<std::size_t>(other.offset / 32 + 1));
            Monstey_Require(offset >= boolWords, "Array resize intersects packed boolean words");
            for (std::size_t j = 0; j < out.ds.size(); ++j)
            {
                const auto &other = out.ds[j];
                if (j == index || other.type == 1 || other.count == 0)
                    continue;
                Monstey_Require(std::max(offset, static_cast<std::size_t>(other.offset)) >=
                                    std::min(offset + d.count, static_cast<std::size_t>(other.offset) + other.count),
                                "Overlapping native fields cannot be resized");
            }
            const auto delta = static_cast<std::int64_t>(values.size()) - d.count;
            out.words.erase(out.words.begin() + static_cast<std::ptrdiff_t>(offset),
                            out.words.begin() + static_cast<std::ptrdiff_t>(offset + d.count));
            out.words.insert(out.words.begin() + static_cast<std::ptrdiff_t>(offset), values.begin(), values.end());
            Monstey_Require(out.words.size() <= 65535, "Native record value count overflow");
            for (std::size_t j = 0; j < out.ds.size(); ++j)
            {
                auto &other = out.ds[j];
                if (j != index && other.type != 1 && other.offset >= offset + d.count)
                {
                    const auto moved = static_cast<std::int64_t>(other.offset) + delta;
                    Monstey_Require(moved >= 0 && moved <= 65535, "Descriptor offset overflow");
                    other.offset = static_cast<std::uint16_t>(moved);
                }
            }
            out.ds[index].offset = static_cast<std::uint16_t>(offset);
            out.ds[index].count = static_cast<std::uint16_t>(values.size());
            out.layout = true;
        }
        return out;
    };
    J nameMap = J::array();
    std::vector<std::uint32_t> tails;
    tails.resize(p.CategoryCount(), 0);
    for (std::uint32_t ci = 0; ci < p.CategoryCount(); ++ci)
        for (const auto ref : p.References(ci))
            if (!p.Clone(ref) && p.Base(ref).named)
                tails[ci] = std::max(tails[ci], Monstey_NativeHash(p.Name(ref)));
    for (std::uint32_t ci = 0; ci < p.categories.size(); ++ci)
        for (const auto &r : p.categories[ci].records)
            if (!p.state.deletes.contains(r.reference))
                groups[ci].push_back(make(r.reference, false, p.Name(r.reference)));
    for (const auto &r : p.state.records)
        if (!p.state.deletes.contains(r.reference))
            groups[r.reference >> 20].push_back(make(r.reference, false, p.Name(r.reference)));
    for (const auto &c : p.state.clones)
    {
        if (p.state.deletes.contains(c.reference))
            continue;
        const auto ci = c.reference >> 20;
        Monstey_Require(tails[ci] != 0xffffffffu, "No native hash tail capacity");
        const auto name =
            p.state.renames.contains(c.reference) ? p.state.renames.at(c.reference) : Monstey_Suffix(c.id, ++tails[ci]);
        groups[ci].push_back(make(c.reference, true, name));
        nameMap.as_array().push_back(J::object({{"id", c.id},
                                                {"name", name},
                                                {"reference", Monstey_Hex(c.reference)},
                                                {"native_hash", Monstey_Hex(Monstey_NativeHash(name))}}));
    }
    const bool transformed =
        !p.state.renames.empty() || !p.state.deletes.empty() || !p.state.categories.empty() || !p.state.records.empty();
    std::vector<std::uint32_t> categoryOrder;
    for (std::uint32_t ci = 0; ci < p.CategoryCount(); ++ci)
        categoryOrder.push_back(ci);
    std::sort(categoryOrder.begin(), categoryOrder.end(), [&](auto a, auto b)
              { return Monstey_NativeHash(p.CategoryText(a, 0)) < Monstey_NativeHash(p.CategoryText(b, 0)); });
    std::vector<std::uint32_t> outputCategory(p.CategoryCount());
    for (std::uint32_t ci = 0; ci < categoryOrder.size(); ++ci)
        outputCategory[categoryOrder[ci]] = ci;
    std::unordered_map<std::uint32_t, std::uint32_t> remap;
    remap.reserve(static_cast<std::size_t>(p.namedCount) + p.anonymousCount + p.state.clones.size());
    std::size_t totalNamed = 0, totalAnonymous = 0, totalWords = 0;
    for (std::uint32_t ci = 0; ci < groups.size(); ++ci)
    {
        auto &group = groups[ci];
        if (transformed)
            std::stable_sort(group.begin(), group.end(),
                             [](const Built &a, const Built &b)
                             {
                                 if (a.named != b.named)
                                     return !a.named;
                                 return !a.named ? false : Monstey_NativeHash(a.name) < Monstey_NativeHash(b.name);
                             });
        std::optional<std::uint32_t> previous;
        for (std::uint32_t i = 0; i < group.size(); ++i)
        {
            auto &r = group[i];
            r.ref = outputCategory[ci] << 20 | i;
            remap[r.oldref] = r.ref;
            if (r.named)
            {
                const auto hash = Monstey_NativeHash(r.name);
                Monstey_Require(!previous || *previous < hash, "Output named hash collision/order error");
                previous = hash;
                ++totalNamed;
            }
            else
                ++totalAnonymous;
        }
    }
    auto mapped = [&](std::uint32_t old) -> std::uint32_t
    {
        if (old == 0xffffffffu)
            return old;
        std::set<std::uint32_t> visited;
        while (p.state.deletes.contains(old))
        {
            Monstey_Require(visited.insert(old).second, "Deleted replacement cycle");
            const auto &replacement = p.state.deletes.at(old);
            Monstey_Require(!replacement.is_null(),
                            "Deleted record has incoming references; provide replacement or explicit 0xffffffff");
            old = p.ValueRef(replacement);
            if (old == 0xffffffffu)
                return old;
        }
        const auto found = remap.find(old);
        Monstey_Require(found != remap.end(), "Typed reference target is outside output");
        return found->second;
    };
    std::map<std::string, std::string> renamed;
    for (const auto &[ref, name] : p.state.renames)
        if (!p.Clone(ref) && !p.Created(ref))
            renamed[p.String(p.Native(ref).name)] = name;
    std::size_t referenceWords = 0;
    for (auto &group : groups)
        for (auto &r : group)
        {
            for (const auto &d : r.ds)
            {
                if (d.type == 10 || d.type == 11)
                {
                    const auto &f = p.SchemaField(r.oldref, d.hash);
                    for (std::size_t i = 0; i < d.count; ++i)
                    {
                        auto &word = r.words[d.offset + i];
                        if (word != 0xffffffffu && p.state.deletes.contains(word) &&
                            !p.state.deletes.at(word).is_null())
                        {
                            const auto target = p.ValueRef(p.state.deletes.at(word));
                            if (target != 0xffffffffu)
                            {
                                if (f.contains("target") && f.at("target").is_string())
                                    Monstey_Require(
                                        p.Allowed(f.at("target").as_string(), p.CategoryText(target >> 20, 1)),
                                        "Deletion replacement violates reference category");
                                else
                                    Monstey_Require((target >> 20) == (word >> 20),
                                                    "Unknown schema cannot qualify cross-category replacement");
                            }
                        }
                        word = mapped(word);
                        ++referenceWords;
                    }
                }
                else if ((d.type == 4 || d.type == 68) && !renamed.empty())
                    for (std::size_t i = 0; i < d.count; ++i)
                    {
                        auto &word = r.words[d.offset + i];
                        const auto original = word < p.stringSize ? p.String(word) : std::string{};
                        if (const auto hit = renamed.find(original); hit != renamed.end())
                            word = atom(J(hit->second), 4);
                    }
            }
            if (r.layout)
            {
                const auto start = static_cast<std::size_t>(p.descriptorCount) + extraDescriptors.size() / 15;
                Monstey_Require(start < 0x1000000, "Descriptor pointer exceeds native 24-bit range");
                r.descriptorStart = static_cast<std::uint32_t>(start);
                for (const auto &d : r.ds)
                {
                    Monstey_AppendU32(extraDescriptors, d.hash);
                    Monstey_AppendU32(extraDescriptors, d.sorted);
                    extraDescriptors.push_back(d.ordinal);
                    extraDescriptors.push_back(d.index);
                    extraDescriptors.push_back(d.type);
                    Monstey_AppendU16(extraDescriptors, d.count);
                    Monstey_AppendU16(extraDescriptors, d.offset);
                }
            }
            totalWords += r.words.size();
        }
    Monstey_Require(totalNamed <= 0xffffffffu && totalAnonymous <= 0xffffffffu && totalWords <= 0xffffffffu,
                    "Native header count overflow");
    std::map<std::uint32_t, std::uint32_t> names;
    for (const auto &group : groups)
        for (const auto &r : group)
            if (r.named)
                names[r.oldref] = !r.clone && !p.Created(r.oldref) && !p.state.renames.contains(r.oldref)
                                      ? r.source->name
                                      : atom(J(r.name), 4);
    std::vector<std::array<std::uint32_t, 3>> categoryAtoms(p.CategoryCount());
    for (std::uint32_t ci = 0; ci < p.CategoryCount(); ++ci)
    {
        if (ci < p.categories.size())
        {
            const auto &c = p.categories[ci];
            categoryAtoms[ci] = {c.schema, c.name, c.path};
        }
        else
            categoryAtoms[ci] = {atom(J(p.CategoryText(ci, 0)), 4), atom(J(p.CategoryText(ci, 1)), 4),
                                 atom(J(p.CategoryText(ci, 2)), 4)};
    }
    B out;
    Monstey_Append(out, p.data, 0, p.stringBase);
    Monstey_PutU32(out, 8, static_cast<std::uint32_t>(atoms.size()));
    Monstey_PutU32(out, 12, static_cast<std::uint32_t>(p.CategoryCount()));
    Monstey_PutU32(out, 16, static_cast<std::uint32_t>(totalNamed));
    Monstey_PutU32(out, 20, static_cast<std::uint32_t>(totalAnonymous));
    Monstey_PutU32(out, 24, static_cast<std::uint32_t>(totalWords));
    Monstey_PutU32(out, 28, p.descriptorCount + static_cast<std::uint32_t>(extraDescriptors.size() / 15));
    out.insert(out.end(), atoms.begin(), atoms.end());
    Monstey_Append(out, p.data, p.descriptorBase, p.descriptorBase + static_cast<std::size_t>(p.descriptorCount) * 15);
    out.insert(out.end(), extraDescriptors.begin(), extraDescriptors.end());
    Monstey_AppendU32(out, 0);
    Monstey_AppendU32(out, static_cast<std::uint32_t>(p.CategoryCount()));
    for (const auto ci : categoryOrder)
    {
        const auto &group = groups[ci];
        const auto anonymous = static_cast<std::uint32_t>(
            std::count_if(group.begin(), group.end(), [](const auto &r) { return !r.named; }));
        for (const auto word :
             {categoryAtoms[ci][0], categoryAtoms[ci][1], categoryAtoms[ci][2], anonymous,
              static_cast<std::uint32_t>(group.size()) - anonymous, Monstey_NativeHash(p.CategoryText(ci, 0))})
            Monstey_AppendU32(out, word);
        for (const auto &r : group)
        {
            if (r.named)
                Monstey_AppendU32(out, names.at(r.oldref));
            Monstey_AppendU32(out, ((r.descriptorStart & 255u) << 24) |
                                       (static_cast<std::uint32_t>(r.ds.size()) << 16) |
                                       static_cast<std::uint32_t>(r.words.size()));
            Monstey_AppendU32(out, (r.named ? 0x08000000u : 0u) | (r.descriptorStart >> 8));
            for (const auto word : r.words)
                Monstey_AppendU32(out, word);
        }
    }
    Monstey_AppendU32(out, 0);
    // Verify a complete second parse before publication. No Python or engine DLL is used.
    auto check = std::make_unique<Monstey_Impl>(std::filesystem::path{}, p.schemaPath,
                                                p.schemaInline ? &p.schema : nullptr, &out);
    Monstey_Require(check->stringSize >= p.stringSize &&
                        std::equal(p.data.begin() + static_cast<std::ptrdiff_t>(p.stringBase),
                                   p.data.begin() + static_cast<std::ptrdiff_t>(p.stringBase + p.stringSize),
                                   check->data.begin() + static_cast<std::ptrdiff_t>(check->stringBase)),
                    "Original atom prefix changed");
    Monstey_Require(
        std::equal(p.data.begin() + static_cast<std::ptrdiff_t>(p.descriptorBase),
                   p.data.begin() +
                       static_cast<std::ptrdiff_t>(p.descriptorBase + static_cast<std::size_t>(p.descriptorCount) * 15),
                   check->data.begin() + static_cast<std::ptrdiff_t>(check->descriptorBase)),
        "Original descriptor prefix changed");
    std::size_t untouched = 0, typedReadbacks = 0;
    J referenceMap = J::object();
    for (const auto &group : groups)
        for (const auto &r : group)
        {
            referenceMap[Monstey_Hex(r.oldref)] = Monstey_Hex(r.ref);
            const auto &actual = check->Native(r.ref);
            Monstey_Require(check->Words(actual) == r.words, "Writer value readback mismatch");
            Monstey_Require(actual.named == r.named && (!r.named || check->String(actual.name) == r.name),
                            "Writer record name readback mismatch");
            const auto ds = check->Descriptors(actual);
            Monstey_Require(ds.size() == r.ds.size(), "Writer descriptor readback mismatch");
            if (!transformed && !r.clone && !r.layout && p.Edits(r.oldref).empty())
            {
                const auto &old = p.Native(r.oldref);
                Monstey_Require(old.size == actual.size &&
                                    std::equal(p.data.begin() + static_cast<std::ptrdiff_t>(old.offset),
                                               p.data.begin() + static_cast<std::ptrdiff_t>(old.offset + old.size),
                                               check->data.begin() + static_cast<std::ptrdiff_t>(actual.offset)),
                                "Untouched record bytes changed");
                ++untouched;
            }
            for (const auto &[hash, expected] : p.Edits(r.oldref))
            {
                const auto d = std::find_if(ds.begin(), ds.end(), [&](const auto &v) { return v.hash == hash; });
                Monstey_Require(d != ds.end(), "Edited field missing at readback");
                const auto &f = p.SchemaField(r.oldref, hash);
                auto value = check->Decode(actual, *d, p.Array(*d, f));
                auto wanted = expected;
                if (d->type == 10 || d->type == 11)
                {
                    auto convert = [&](const J &v) -> J
                    {
                        const auto ref = p.ValueRef(v);
                        return ref == 0xffffffffu ? J() : J(Monstey_Hex(mapped(ref)));
                    };
                    if (p.Array(*d, f))
                    {
                        wanted = J::array();
                        for (const auto &v : expected.as_array())
                            wanted.as_array().push_back(convert(v));
                    }
                    else
                        wanted = convert(expected);
                }
                Monstey_Require(value == wanted, "Typed edit semantic readback mismatch");
                ++typedReadbacks;
            }
        }
    for (const auto &[ref, policy] : p.state.deletes)
    {
        (void)policy;
        referenceMap[Monstey_Hex(ref)] = nullptr;
    }
    for (auto &row : nameMap.as_array())
        row["reference"] = Monstey_Hex(remap.at(Monstey_ParseRef(row.at("reference").as_string())));
    B publishedBytes = p.wrapper;
    publishedBytes.insert(publishedBytes.end(), out.begin(), out.end());
    const auto digest = Monstey_HashSha256(publishedBytes);
    J report = J::object({{"status", "MONSTEY_NATIVE_CPP_GADB_BUILD_VALIDATED"},
                          {"source", Monstey_Utf8(p.source.wstring())},
                          {"source_sha256", p.sourceHash},
                          {"path", Monstey_Utf8(absolute.wstring())},
                          {"sha256", digest},
                          {"bytes", publishedBytes.size()},
                          {"format", p.wrapper.empty() ? "GADB17" : "LTFX8/GADB17"},
                          {"warnings", J::array()},
                          {"clones", p.state.clones.size()},
                          {"typed_field_readbacks", typedReadbacks},
                          {"typed_reference_words_checked", referenceWords},
                          {"unmodified_records_byte_exact", untouched},
                          {"original_atoms_byte_exact", true},
                          {"original_descriptor_prefix_byte_exact", true},
                          {"old_record_ordinals_unchanged", !transformed},
                          {"strict_named_order", true},
                          {"utf16_atoms_added", utf16Added},
                          {"reference_map", transformed ? referenceMap : J::object()},
                          {"external_asset_consumers_verified", false},
                          {"native_cpp", true},
                          {"game_written", false},
                          {"runtime_verified", false}});
    if (declarations)
    {
        // These are the writer's completed Built rows, not project snapshots:
        // clone names, native category/named order, resizes and deletions are final.
        Monstey_Require(categoryOrder.size() <= 4096, "Native declaration checker category limit exceeded");
        J bindingCounts = J::object();
        for (const auto ci : categoryOrder)
        {
            const auto name = p.CategoryText(ci, 1);
            bindingCounts[name] = bindingCounts.contains(name) ? bindingCounts.at(name).as_uint() + 1 : 1;
        }
        constexpr std::array counters{"qualified_categories",      "unknown_categories",
                                      "unsafe_categories",         "records_checked",
                                      "descriptor_rows_checked",   "field_predicates_checked",
                                      "cached_slot_hazards",       "unqualified_value_contracts",
                                      "later_predicate_conflicts", "diagnostics_total"};
        J audit = J::object();
        for (const auto key : counters)
            audit[key] = 0;
        audit["categories"] = J::array();
        audit["diagnostics"] = J::array();
        for (const auto ci : categoryOrder)
        {
            J records = J::array();
            for (const auto &r : groups[ci])
            {
                J fields = J::array();
                for (const auto &d : r.ds)
                    fields.as_array().push_back(J::object({{"hash", Monstey_Hex(d.hash)},
                                                           {"native_ordinal", d.ordinal},
                                                           {"native_type", d.type},
                                                           {"count", d.count}}));
                records.as_array().push_back(
                    J::object({{"reference", Monstey_Hex(r.ref)},
                               {"name", Monstey_DisplayUtf8(r.name)},
                               {"name_hash", r.named ? J(Monstey_Hex(Monstey_NativeHash(r.name))) : J()},
                               {"named", r.named},
                               {"fields", std::move(fields)}}));
            }
            const auto checked = declarations->Check(
                J::object({{"format", "Monstey_GadbClassCacheLayout"},
                           {"version", 1},
                           {"source_sha256", digest},
                           {"binding_name_counts", bindingCounts},
                           {"categories", J::array({J::object({{"schema", p.CategoryText(ci, 0)},
                                                               {"name", p.CategoryText(ci, 1)},
                                                               {"records", std::move(records)}})})}}));
            for (const auto key : counters)
                audit[key] = audit.at(key).as_uint() + checked.at(key).as_uint();
            for (const auto &category : checked.at("categories").as_array())
                audit["categories"].as_array().push_back(category);
            for (const auto &diagnostic : checked.at("diagnostics").as_array())
                if (audit["diagnostics"].size() < 256)
                    audit["diagnostics"].as_array().push_back(diagnostic);
            audit["reflection_source_sha256"] = checked.at("reflection_source_sha256");
        }
        // An empty database still qualifies the explicitly supplied evidence source.
        if (categoryOrder.empty())
        {
            const auto checked = declarations->Check(J::object({{"format", "Monstey_GadbClassCacheLayout"},
                                                                {"version", 1},
                                                                {"source_sha256", digest},
                                                                {"categories", J::array()}}));
            audit["reflection_source_sha256"] = checked.at("reflection_source_sha256");
        }
        audit["format"] = "Monstey_GadbClassCacheCheck";
        audit["version"] = 1;
        audit["status"] = audit.at("unsafe_categories").as_uint()    ? "unsafe"
                          : audit.at("unknown_categories").as_uint() ? "unknown"
                                                                     : "qualified";
        audit["scope"] = "proved_direct_first_record_binder_and_cached_descriptor_layout";
        audit["layout_source_sha256"] = digest;
        audit["input_source_sha256"] = p.sourceHash;
        audit["layout_stage"] = "final_native_writer_plan";
        audit["category_count"] = categoryOrder.size();
        audit["diagnostics_truncated"] = audit.at("diagnostics_total").as_uint() > audit["diagnostics"].size();
        audit["assumes_unbound_field_cache"] = true;
        audit["shared_ancestor_state_simulated"] = false;
        audit["engine_executed"] = false;
        audit["gameplay_qualified"] = false;
        audit["runtime_verified"] = false;
        audit["explicit_native_declarations"] = true;
        audit["unsafe_policy"] = "reject_before_any_output_publication";
        audit["unknown_policy"] = "retain_without_engine_compatibility_claim";
        if (audit.at("unsafe_categories").as_uint())
            throw Monstey_Error("gadb_class_cache_unsafe",
                                "Final native class-cache layout is unsafe; no output published.\n" + audit.dump(2));
        report["native_class_cache"] = std::move(audit);
    }
    p.Unchanged();
    if (!p.memorySource)
        Monstey_Require(Monstey_HashSha256(p.source) == p.sourceHash, "Source hash changed during build");
    std::vector<std::filesystem::path> published;
    try
    {
        Monstey_WriteNewFile(absolute, publishedBytes);
        published.push_back(absolute);
        const auto map = J::object({{"records", nameMap},
                                    {"reference_map", transformed ? referenceMap : J::object()},
                                    {"old_ordinals_unchanged", !transformed}})
                             .dump(2) +
                         "\n";
        Monstey_WriteNewText(mapPath, map);
        published.push_back(mapPath);
        Monstey_WriteNewText(reportPath, report.dump(2) + "\n");
        published.push_back(reportPath);
        Monstey_Require(Monstey_HashSha256(absolute) == digest, "Published GADB hash mismatch");
    }
    catch (...)
    {
        for (auto i = published.rbegin(); i != published.rend(); ++i)
        {
            std::error_code error;
            std::filesystem::remove(Monstey_WindowsFilePath(*i), error);
        }
        throw;
    }
    report["report_path"] = Monstey_Utf8(reportPath.wstring());
    report["namemap_path"] = Monstey_Utf8(mapPath.wstring());
    return report;
}

} // namespace Monstey
