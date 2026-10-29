#include "Monstey_GadbPrivate.hpp"

namespace Monstey
{
using namespace Monstey_GadbPrivate;

Monstey_Gadb::Monstey_Impl::Monstey_Impl(const std::filesystem::path &path, const std::filesystem::path &nativeSchema,
                                         const J *qualifiedSchema, const B *memory)
    : source(memory ? std::filesystem::path{} : std::filesystem::absolute(path)),
      schemaPath(nativeSchema.empty() ? nativeSchema : std::filesystem::absolute(nativeSchema)),
      data(memory ? *memory : Monstey_ReadFile(source))
{
    memorySource = memory != nullptr;
    sourceHash = memorySource ? Monstey_HashSha256(data) : Monstey_HashSha256(source);
    sourceSize = memorySource ? data.size() : std::filesystem::file_size(Monstey_WindowsFilePath(source));
    if (!memorySource)
        sourceTime = std::filesystem::last_write_time(Monstey_WindowsFilePath(source));
    if (data.size() >= 8 && std::string_view(reinterpret_cast<const char *>(data.data()), 4) == "LTFX")
    {
        Monstey_Require(Monstey_U32(data, 4) == 8, "Only verified LTFX8 is supported");
        wrapper.assign(data.begin(), data.begin() + 8);
        data.erase(data.begin(), data.begin() + 8);
    }
    Parse();
    if (qualifiedSchema)
    {
        schema = *qualifiedSchema;
        schemaInline = true;
        const auto encoded = schema.dump();
        schemaHash = Monstey_HashSha256(
            std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(encoded.data()), encoded.size()));
    }
    else if (!schemaPath.empty())
    {
        schema = J::Parse(Monstey_ReadText(schemaPath));
        schemaHash = Monstey_HashSha256(schemaPath);
    }
    if (schemaInline || !schemaPath.empty())
    {

        Monstey_Require(schema.contains("categories") && schema.at("categories").is_object(),
                        "Native schema needs a categories object");

        if (schema.contains("reference_aliases"))
        {
            const auto &aliases = schema.at("reference_aliases");
            Monstey_Require(aliases.is_object() && aliases.size() <= 4096, "Reference aliases need a bounded object");
            for (const auto &[alias, target] : aliases.as_object())
            {
                Monstey_Require(!alias.empty() && alias.size() <= 512 && alias.find('\0') == std::string::npos &&
                                    !schema.at("categories").contains(alias),
                                "Reference alias cannot shadow a native category");
                Monstey_Wide(alias);
                Monstey_Require(target.is_string(), "Reference alias target must be a category name");
                std::set<std::string> seen;
                std::string resolved = alias;
                while (aliases.contains(resolved))
                {
                    Monstey_Require(seen.insert(resolved).second && seen.size() <= 64,
                                    "Reference alias cycle/depth exceeded");
                    const auto &next = aliases.at(resolved);
                    Monstey_Require(next.is_string(), "Reference alias target must be a category name");
                    resolved = next.as_string();
                    Monstey_Require(!resolved.empty() && resolved.size() <= 512 &&
                                        resolved.find('\0') == std::string::npos,
                                    "Invalid reference alias target");
                    Monstey_Wide(resolved);
                }
                Monstey_Require(schema.at("categories").contains(resolved),
                                "Reference alias target category is absent");
            }
        }
        for (const auto &[label, cat] : schema.at("categories").as_object())
        {
            (void)label;
            if (!cat.contains("fields"))
                continue;
            for (const auto &f : cat.at("fields").as_array())
                if (f.contains("name") && f.at("name").is_string())
                    fieldNames.emplace(Monstey_NativeHash(f.at("name").as_string()), f.at("name").as_string());
        }
    }
    schemaFields.resize(categories.size());
    if (schema.contains("categories"))
        for (std::size_t ci = 0; ci < categories.size(); ++ci)
        {
            const auto label = String(categories[ci].name);
            if (!schema.at("categories").contains(label))
                continue;
            const auto &category = schema.at("categories").at(label);
            if (!category.contains("fields"))
                continue;
            for (const auto &field : category.at("fields").as_array())
                if (field.contains("name") && field.at("name").is_string())
                    schemaFields[ci].emplace(Monstey_NativeHash(field.at("name").as_string()), &field);
        }
}

void Monstey_Gadb::Monstey_Impl::Unchanged() const
{
    if (!memorySource)
        Monstey_Require(std::filesystem::file_size(Monstey_WindowsFilePath(source)) == sourceSize &&
                            std::filesystem::last_write_time(Monstey_WindowsFilePath(source)) == sourceTime,
                        "Source database changed; reopen it");
    if (!schemaPath.empty())
        Monstey_Require(Monstey_HashSha256(schemaPath) == schemaHash, "Native schema changed; reopen it");
}

std::string Monstey_Gadb::Monstey_Impl::String(std::uint32_t pointer) const
{
    Monstey_Require(pointer < stringSize, "String pointer outside atom pool");
    const auto begin = stringBase + pointer;
    auto end = begin;
    while (end < stringBase + stringSize && data[end])
        ++end;
    Monstey_Require(end < stringBase + stringSize, "Unterminated GADB string");
    return std::string(reinterpret_cast<const char *>(data.data() + begin), end - begin);
}

std::string Monstey_Gadb::Monstey_Impl::WideText(std::uint32_t pointer) const
{
    Monstey_Require(pointer % 2 == 0 && pointer < stringSize, "UTF-16 atom outside pool");
    std::wstring text;
    std::size_t at = stringBase + pointer;
    for (;; at += 2)
    {
        Monstey_Require(at + 2 <= stringBase + stringSize, "Unterminated UTF-16 atom");
        const auto c = Monstey_U16(data, at);
        if (!c)
            break;
        text.push_back(static_cast<wchar_t>(c));
    }
    return Monstey_Utf8(text);
}

void Monstey_Gadb::Monstey_Impl::Parse()
{
    Monstey_Require(data.size() >= 46 && std::string_view(reinterpret_cast<const char *>(data.data()), 4) == "GADB",
                    "Not a GADB file");
    Monstey_Require(Monstey_U32(data, 4) == 17,
                    "This native adapter supports GADB17; FEAR GADB3 has a different binary contract");
    const auto categoryCount = Monstey_U32(data, 12);
    Monstey_Require(categoryCount <= 4096, "Category code exceeds native 12-bit reference");
    stringSize = Monstey_U32(data, 8);
    namedCount = Monstey_U32(data, 16);
    anonymousCount = Monstey_U32(data, 20);
    wordCount = Monstey_U32(data, 24);
    descriptorCount = Monstey_U32(data, 28);
    Monstey_Require(Monstey_U32(data, 40) == 44, "Unexpected GADB header boundary");
    stringBase = 46 + Monstey_U16(data, 44);
    Monstey_Require(stringBase <= data.size() && stringSize <= data.size() - stringBase, "Truncated atom pool");
    descriptorBase = stringBase + stringSize;
    Monstey_Require(descriptorCount <= (data.size() - descriptorBase) / 15, "Truncated descriptor table");
    const auto prefix = descriptorBase + static_cast<std::size_t>(descriptorCount) * 15;
    Monstey_Require(prefix + 8 <= data.size() && Monstey_U32(data, prefix) == 0 &&
                        Monstey_U32(data, prefix + 4) == categoryCount,
                    "Registry prefix mismatch");
    registryStart = prefix + 8;
    std::size_t at = registryStart;
    std::uint64_t ac = 0, nc = 0, wc = 0;
    std::uint32_t previousCategory = 0;
    categories.reserve(categoryCount);
    for (std::uint32_t ci = 0; ci < categoryCount; ++ci)
    {
        Monstey_Require(at + 24 <= data.size(), "Truncated category header");
        Monstey_CategoryData c;
        c.schema = Monstey_U32(data, at);
        c.name = Monstey_U32(data, at + 4);
        c.path = Monstey_U32(data, at + 8);
        c.anonymous = Monstey_U32(data, at + 12);
        const auto named = Monstey_U32(data, at + 16);
        c.hash = Monstey_U32(data, at + 20);
        Monstey_Require(Monstey_NativeHash(String(c.schema)) == c.hash,
                        "Category schema hash does not match stored key");
        String(c.name);
        String(c.path);
        Monstey_Require(ci == 0 || previousCategory < c.hash, "Category hashes are not strictly increasing");
        previousCategory = c.hash;
        Monstey_Require(static_cast<std::uint64_t>(c.anonymous) + named <= 0x100000,
                        "Record ordinal exceeds 20-bit encoding");
        ac += c.anonymous;
        nc += named;
        at += 24;
        std::uint32_t previousName = 0;
        for (std::uint32_t ri = 0; ri < c.anonymous + named; ++ri)
        {
            Monstey_RecordData r;
            r.offset = at;
            r.reference = ci << 20 | ri;
            r.named = ri >= c.anonymous;
            const std::size_t header = r.named ? 12 : 8;
            Monstey_Require(at + header <= data.size(), "Truncated record header");
            if (r.named)
            {
                r.name = Monstey_U32(data, at);
                const auto h = Monstey_NativeHash(String(r.name));
                Monstey_Require(ri == c.anonymous || previousName < h,
                                "Named record hashes are not strictly increasing");
                previousName = h;
                at += 4;
            }
            r.packed = Monstey_U32(data, at);
            r.flags = Monstey_U32(data, at + 4);
            Monstey_Require((r.flags & 0xffff0000) == (r.named ? 0x08000000u : 0u), "Unsupported native record flags");
            at += 8;
            r.values = at;
            const auto count = r.packed & 65535u;
            Monstey_Require(count <= (data.size() - at) / 4, "Truncated record values");
            at += static_cast<std::size_t>(count) * 4;
            r.size = at - r.offset;
            const auto ds = ((r.flags & 65535u) << 8) | (r.packed >> 24);
            const auto dc = (r.packed >> 16) & 255u;
            Monstey_Require(ds <= descriptorCount && dc <= descriptorCount - ds, "Record descriptors outside table");
            wc += count;
            c.records.push_back(r);
        }
        categories.push_back(std::move(c));
    }
    Monstey_Require(at + 4 == data.size() && Monstey_U32(data, at) == 0, "Unexpected native trailer");
    Monstey_Require(ac == anonymousCount && nc == namedCount && wc == wordCount,
                    "Native header totals disagree with registry");
}

} // namespace Monstey
