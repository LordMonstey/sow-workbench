#include "Monstey_ModelPrivate.hpp"

namespace Monstey
{
namespace Monstey_ModelPrivate
{

std::string Monstey_ModelString(Monstey_Span b, std::size_t start, std::size_t end)
{
    Monstey_MRequire(start < end && end <= b.size(), "SKEL name outside string table");
    auto p = start;
    while (p < end && b[p])
        ++p;
    Monstey_MRequire(p < end && p > start, "SKEL unterminated/empty bone name");
    return std::string(reinterpret_cast<const char *>(b.data() + start), p - start);
}

Monstey_SkelData Monstey_ReadSkel(Monstey_Span raw, const Monstey_SkelPage *page)
{
    Monstey_MRequire(raw.size() >= 96 && raw.size() <= 512ull * 1024 * 1024 && Monstey_U32(raw, 0) == 0x4c454b53 &&
                         Monstey_U32(raw, 4) == 17,
                     "Expected SKEL17");
    const auto count = Monstey_U32(raw, 40);
    Monstey_MRequire(count <= 65536, "SKEL bone count exceeds inspection limit");
    Monstey_SkelData out;
    out.table_count = Monstey_U32(raw, 32);
    out.string_bytes = Monstey_U32(raw, 12);
    out.sha256 = Monstey_HashSha256(raw);
    std::size_t p = 96;
    Monstey_MRequire(out.table_count <= raw.size() / 4, "SKEL table count exceeds input");
    for (std::uint32_t table = 0; table < out.table_count; ++table)
    {
        const auto tableStart = p;
        const auto entries = Monstey_U32(raw, p);
        p += 4;
        const auto rowBytes = static_cast<std::size_t>(entries) * 14;
        Monstey_CheckRange(raw, p, rowBytes);
        out.piece_rows += entries;
        if (page && table >= page->table_offset &&
            static_cast<std::uint64_t>(table) - page->table_offset < page->table_limit)
        {
            auto rows = Monstey_J::array();
            const auto first = std::min(page->row_offset, entries);
            const auto shown = std::min<std::uint64_t>(entries - first, page->row_limit - out.shown_rows);
            for (std::uint64_t i = 0; i < shown; ++i)
            {
                const auto ordinal = first + i, at = p + static_cast<std::size_t>(ordinal) * 14;
                const auto bits = Monstey_U32(raw, at + 2), word6 = Monstey_U32(raw, at + 6),
                           word10 = Monstey_U32(raw, at + 10);
                const bool finite = (bits & 0x7f800000u) != 0x7f800000u;
                Monstey_J value =
                    finite ? Monstey_J(static_cast<double>(std::bit_cast<float>(bits)))
                           : Monstey_J::object({{"nonfinite", true}, {"float32_bits", Monstey_Hex(bits, 8)}});
                std::string rawHex;
                rawHex.reserve(28);
                constexpr const char *hex = "0123456789abcdef";
                for (std::size_t k = 0; k < 14; ++k)
                {
                    rawHex.push_back(hex[raw[at + k] >> 4]);
                    rawHex.push_back(hex[raw[at + k] & 15]);
                }
                rows.as_array().push_back(Monstey_J::object({{"index", ordinal},
                                                             {"source_offset", static_cast<std::uint64_t>(at)},
                                                             {"source_bytes", 14},
                                                             {"byte0", raw[at]},
                                                             {"byte1", raw[at + 1]},
                                                             {"float2", value},
                                                             {"float32_bits", Monstey_Hex(bits, 8)},
                                                             {"finite_float", finite},
                                                             {"word6", word6},
                                                             {"word10", word10},
                                                             {"native_sentinel_flag", word10 != 0xffffffffu},
                                                             {"raw_hex", rawHex}}));
            }
            out.shown_rows += shown;
            out.piece_tables.as_array().push_back(
                Monstey_J::object({{"index", table},
                                   {"source_offset", static_cast<std::uint64_t>(tableStart)},
                                   {"source_bytes", static_cast<std::uint64_t>(4 + rowBytes)},
                                   {"rows_offset", static_cast<std::uint64_t>(p)},
                                   {"rows_total", entries},
                                   {"sha256", Monstey_HashSha256(raw.subspan(tableStart, 4 + rowBytes))},
                                   {"row_offset", page->row_offset},
                                   {"rows_shown", shown},
                                   {"rows_truncated", first != 0 || shown != entries},
                                   {"rows", rows}}));
        }
        p += rowBytes;
    }
    out.prefix_end = p;
    Monstey_MRequire(Monstey_U32(raw, p) == 0xdcdcff00, "Missing SKEL string marker");
    p += 4;
    const auto strings = p;
    Monstey_CheckRange(raw, p, out.string_bytes);
    p += out.string_bytes;
    out.bone_offset = p;
    Monstey_CheckRange(raw, p, static_cast<std::size_t>(count) * 37);
    std::vector<std::pair<std::size_t, unsigned>> stack;
    out.bones.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        Monstey_SkelBone entry;
        auto &b = entry.bone;
        const auto offset = Monstey_U32(raw, p);
        Monstey_MRequire(offset < out.string_bytes, "Invalid SKEL name/activity");
        entry.active = raw[p + 4] != 0;
        b.children = Monstey_U32(raw, p + 33);
        Monstey_MRequire(b.children <= count - i - 1, "SKEL child count exceeds remaining bones");
        while (!stack.empty() && stack.back().second == 0)
            stack.pop_back();
        b.parent = stack.empty() ? -1 : static_cast<int>(stack.back().first);
        if (!stack.empty())
            --stack.back().second;
        b.name = Monstey_ModelString(raw, strings + offset, strings + out.string_bytes);
        for (unsigned k = 0; k < 7; ++k)
        {
            const auto bits = Monstey_U32(raw, p + 5 + k * 4);
            entry.pose_bits[k] = bits;
            const bool finite = (bits & 0x7f800000u) != 0x7f800000u;
            entry.finite = entry.finite && finite;
            const auto value = finite ? static_cast<double>(std::bit_cast<float>(bits)) : 0;
            if (k < 3)
                b.t[k] = value;
            else
                b.q[k - 3] = value;
        }
        if (b.children)
            stack.emplace_back(i, b.children);
        out.bones.push_back(std::move(entry));
        p += 37;
    }
    Monstey_MRequire(std::all_of(stack.begin(), stack.end(), [](const auto &x) { return x.second == 0; }),
                     "SKEL incomplete preorder hierarchy");
    out.bone_end = p;
    return out;
}

Monstey_J Monstey_InspectSkelResourceRecordEnvelope(Monstey_Span raw, std::size_t first, std::uint32_t count)
{
    Monstey_CheckRange(raw, first, 4);
    Monstey_MRequire(count <= (raw.size() - first - 4) / 8, "SKEL resource record headers exceed source extent");
    auto records = Monstey_J::array();
    auto cursor = first;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        Monstey_CheckRange(raw, cursor, 8);
        const auto tag = Monstey_U32(raw, cursor), bytes = Monstey_U32(raw, cursor + 4);
        const auto payload = cursor + 8;
        Monstey_CheckRange(raw, payload, bytes);
        records.as_array().push_back(
            Monstey_J::object({{"ordinal", i},
                               {"header_offset", static_cast<std::uint64_t>(cursor)},
                               {"raw_tag_word", tag},
                               {"payload_offset", static_cast<std::uint64_t>(payload)},
                               {"payload_bytes", bytes},
                               {"payload_sha256", Monstey_HashSha256(raw.subspan(payload, bytes))}}));
        cursor = payload + bytes;
    }
    Monstey_CheckRange(raw, cursor, 4);
    const auto endWord = Monstey_U32(raw, cursor);
    const auto section5 = cursor + 4;
    Monstey_MRequire(Monstey_U32(raw, section5) == 0xdcdcff05,
                     "Missing sequential SKEL section5 after resource container");
    return Monstey_J::object({{"records", records},
                              {"records_offset", static_cast<std::uint64_t>(first)},
                              {"records_and_end_word_bytes", static_cast<std::uint64_t>(section5 - first)},
                              {"container_end_word_offset", static_cast<std::uint64_t>(cursor)},
                              {"raw_container_end_word", endWord},
                              {"section5_marker_offset", static_cast<std::uint64_t>(section5)},
                              {"following_bytes", static_cast<std::uint64_t>(raw.size() - section5)},
                              {"following_sha256", Monstey_HashSha256(raw.subspan(section5))},
                              {"record_payload_alignment", 1},
                              {"native_loader_validates_record_tags", false},
                              {"payload_internal_grammar_qualified", false},
                              {"authoring_supported", false}});
}

Monstey_SkelClothResources Monstey_ReadSkelClothResources(Monstey_Span raw, const Monstey_SkelData &skeleton)
{
    Monstey_SkelClothResources out;
    // Historical synthetic fixtures stop at the bones or retain an opaque tail.
    // Qualify only the native ordinary-node path with a proved sequential extent.
    if (Monstey_U32(raw, 8) != 64 || Monstey_U32(raw, 20) != skeleton.bones.size() ||
        Monstey_U32(raw, 24) != skeleton.bones.size() * 37)
    {
        out.metadata["unknown_reason"] = "post_bone_header_layout_unqualified";
        return out;
    }
    if (Monstey_U32(raw, 64) != 0)
    {
        out.metadata["unknown_reason"] = "additional_node_property_layout_unqualified";
        return out;
    }
    const auto strings = skeleton.prefix_end + 4;
    const auto stringEnd = strings + skeleton.string_bytes;
    auto name = [&](std::uint32_t offset)
    {
        Monstey_MRequire(offset < skeleton.string_bytes, "SKEL resource/name offset outside string pool");
        return Monstey_ModelString(raw, strings + offset, stringEnd);
    };
    std::size_t p = skeleton.bone_end;
    const auto channelCount = Monstey_U32(raw, p);
    p += 4;
    Monstey_MRequire(channelCount <= (raw.size() - p) / 12, "SKEL node-channel count exceeds input");
    for (std::uint32_t i = 0; i < channelCount; ++i)
    {
        Monstey_CheckRange(raw, p, 12);
        (void)name(Monstey_U32(raw, p));
        const auto elements = Monstey_U32(raw, p + 4);
        Monstey_MRequire(elements == skeleton.bones.size(), "SKEL node-channel element count mismatch");
        Monstey_MRequire(Monstey_U32(raw, p + 8) <= elements, "SKEL selected node count exceeds channel");
        p += 12;
        Monstey_CheckRange(raw, p, elements);
        p += elements;
    }
    const auto additionalNodes = Monstey_U32(raw, p);
    p += 4;
    const auto additionalBytes = static_cast<std::size_t>(additionalNodes) * 40;
    Monstey_CheckRange(raw, p, additionalBytes);
    for (std::uint32_t i = 0; i < additionalNodes; ++i)
        (void)name(Monstey_U32(raw, p + static_cast<std::size_t>(i) * 40 + 4));
    p += additionalBytes;
    const auto marker1 = p;
    Monstey_MRequire(Monstey_U32(raw, p) == 0xdcdcff01, "Missing sequential SKEL name-offset marker");
    p += 4;
    const auto nameCount = Monstey_U32(raw, 16), resourceFlags = Monstey_U32(raw, 28);
    Monstey_MRequire(static_cast<std::uint64_t>(resourceFlags) + skeleton.table_count <= nameCount,
                     "SKEL shared name-offset vector is shorter than resource/piece counts");
    const auto nameBytes = static_cast<std::size_t>(nameCount) * 4;
    Monstey_CheckRange(raw, p, nameBytes);
    for (std::uint32_t i = 0; i < nameCount; ++i)
        (void)name(Monstey_U32(raw, p + static_cast<std::size_t>(i) * 4));
    p += nameBytes;
    Monstey_MRequire(Monstey_U32(raw, p) == 0xdcdcff02, "Missing sequential SKEL resource-flag marker");
    p += 4;
    const auto flagBytes = static_cast<std::size_t>(resourceFlags) * 4;
    Monstey_CheckRange(raw, p, flagBytes);
    p += flagBytes;
    Monstey_MRequire(Monstey_U32(raw, p) == 0xdcdcff03, "Missing sequential SKEL node-flag marker");
    p += 4;
    Monstey_CheckRange(raw, p, skeleton.bones.size());
    p += skeleton.bones.size();
    const auto marker4 = p;
    Monstey_MRequire(Monstey_U32(raw, p) == 0xdcdcff04, "Missing sequential SKEL cloth-resource marker");
    const auto count = Monstey_U32(raw, p + 4);
    p += 8;
    const auto offsetBytes = static_cast<std::size_t>(count) * 4;
    Monstey_CheckRange(raw, p, offsetBytes);
    auto names = Monstey_J::array();
    for (std::uint32_t i = 0; i < count; ++i)
    {
        auto path = name(Monstey_U32(raw, p + static_cast<std::size_t>(i) * 4));
        names.as_array().push_back(path);
        out.names.push_back(std::move(path));
    }
    p += offsetBytes;
    out.qualified = true;
    out.metadata["section_locator_qualified"] = true;
    out.metadata["resource_count"] = count;
    out.metadata["resource_names"] = std::move(names);
    out.metadata["name_offsets_marker_offset"] = static_cast<std::uint64_t>(marker1);
    out.metadata["section4_marker_offset"] = static_cast<std::uint64_t>(marker4);
    out.metadata["resource_records_offset"] = static_cast<std::uint64_t>(p);
    out.metadata["retained_records_and_tail_bytes"] = static_cast<std::uint64_t>(raw.size() - p);
    out.metadata["retained_records_and_tail_sha256"] = Monstey_HashSha256(raw.subspan(p));
    out.metadata["node_channel_count"] = channelCount;
    out.metadata["additional_node_count"] = additionalNodes;
    out.metadata["record_envelope_qualified"] = false;
    try
    {
        auto envelope = Monstey_InspectSkelResourceRecordEnvelope(raw, p, count);
        for (std::size_t i = 0; i < out.names.size(); ++i)
            envelope["records"][i]["resource_name"] = out.names[i];
        out.metadata["record_envelope"] = std::move(envelope);
        out.metadata["record_envelope_qualified"] = true;
    }
    catch (const Monstey_Error &error)
    {
        out.metadata["record_envelope_error"] = Monstey_J::object({{"code", error.code}, {"message", error.what()}});
    }
    return out;
}

Monstey_Rig Monstey_ReadModelRig(const Monstey_ModelData &m, const std::filesystem::path &skel)
{
    Monstey_Rig rig;
    const auto n = m.h[16];
    std::vector<std::vector<std::size_t>> children(n + 1);
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto p = 4 + i * 40;
        Monstey_Bone b;
        b.id = Monstey_U32(m.bones, p);
        b.parent = static_cast<std::int16_t>(Monstey_U16(m.bones, p + 4));
        b.children = Monstey_U16(m.bones, p + 6);
        Monstey_MRequire(b.parent >= -1 && b.parent < static_cast<int>(n) && b.parent != static_cast<int>(i),
                         "MMSH rig parent outside hierarchy");
        for (unsigned k = 0; k < 3; ++k)
            b.t[k] = Monstey_F32(m.bones, p + 8 + k * 4);
        for (unsigned k = 0; k < 4; ++k)
            b.q[k] = Monstey_F32(m.bones, p + 20 + k * 4);
        b.scale = Monstey_F32(m.bones, p + 36);
        Monstey_MRequire(b.scale > 0, "MMSH rig scale must be positive");
        Monstey_V3 metres = b.t;
        for (auto &x : metres)
            x /= 100;
        b.global = Monstey_Transform(metres, b.q, {b.scale, b.scale, b.scale});
        children[b.parent < 0 ? n : static_cast<std::size_t>(b.parent)].push_back(i);
        rig.source.push_back(b);
    }
    rig.hkToSource = children[n];
    Monstey_MRequire(!rig.hkToSource.empty(), "MMSH rig has no root");
    std::vector<bool> visited(n, false);
    const auto walk = [&](auto &&self, std::size_t i) -> void
    {
        Monstey_MRequire(!visited[i], "MMSH cyclic or repeated rig bone");
        visited[i] = true;
        Monstey_MRequire(children[i].size() == rig.source[i].children, "MMSH parent and child counts disagree");
        rig.hkToSource.insert(rig.hkToSource.end(), children[i].begin(), children[i].end());
        for (const auto j : children[i])
            self(self, j);
    };
    for (const auto i : children[n])
        walk(walk, i);
    Monstey_MRequire(rig.hkToSource.size() == n &&
                         std::all_of(visited.begin(), visited.end(), [](bool v) { return v; }),
                     "MMSH disconnected rig");
    rig.sourceToHk.resize(n);
    for (std::size_t hk = 0; hk < n; ++hk)
        rig.sourceToHk[rig.hkToSource[hk]] = hk;
    if (!skel.empty())
    {
        const auto skeleton = Monstey_ReadSkel(Monstey_ReadFile(skel));
        Monstey_MRequire(skeleton.bones.size() == n, "SKEL/MMSH bone count mismatch");
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto &entry = skeleton.bones[i];
            const auto &bone = entry.bone;
            Monstey_MRequire(entry.finite, "SKEL bind contains nonfinite components");
            Monstey_MRequire(bone.parent == rig.source[i].parent && bone.children == rig.source[i].children,
                             "SKEL/MMSH parent-child mismatch");
            rig.source[i].name = bone.name;
            for (unsigned k = 0; k < 3; ++k)
                Monstey_MRequire(std::abs(bone.t[k] - rig.source[i].t[k]) <= 2e-5,
                                 "SKEL/MMSH bind translation mismatch");
            double same = 0, opposite = 0;
            for (unsigned k = 0; k < 4; ++k)
            {
                same = std::max(same, std::abs(bone.q[k] - rig.source[i].q[k]));
                opposite = std::max(opposite, std::abs(bone.q[k] + rig.source[i].q[k]));
            }
            Monstey_MRequire(std::min(same, opposite) <= 2e-5, "SKEL/MMSH bind quaternion mismatch");
        }
        rig.namesVerified = true;
        rig.skelHash = skeleton.sha256;
    }

    std::set<std::string> names;
    for (std::size_t hk = 0; hk < n; ++hk)
    {
        auto &b = rig.source[rig.hkToSource[hk]];
        if (b.name.empty())
            b.name = "Bone_HK_" + std::to_string(hk);
        if (names.contains(b.name))
            b.name += "__HK_" + std::to_string(hk);
        Monstey_MRequire(names.insert(b.name).second, "Rig bone names cannot be unique");
    }
    return rig;
}

Monstey_J Monstey_RigJson(const Monstey_Rig &rig)
{
    auto out = Monstey_J::object();
    out["bones"] = Monstey_J::array();
    out["skel_matching_verified"] = rig.namesVerified;
    out["skel_sha256"] = rig.skelHash;
    out["joint_space"] = "Havok sibling-block order";
    out["bind_space"] = "global native transforms, centimetres";
    for (std::size_t hk = 0; hk < rig.hkToSource.size(); ++hk)
    {
        const auto source = rig.hkToSource[hk];
        const auto &b = rig.source[source];
        const int parent = b.parent < 0 ? -1 : static_cast<int>(rig.sourceToHk[static_cast<std::size_t>(b.parent)]);
        out["bones"].as_array().push_back(Monstey_J::object({{"hk_index", static_cast<std::uint64_t>(hk)},
                                                             {"source_index", static_cast<std::uint64_t>(source)},
                                                             {"parent_hk", parent},
                                                             {"name", b.name},
                                                             {"global_bind_metres", Monstey_MatrixJson(b.global)}}));
    }
    return out;
}

} // namespace Monstey_ModelPrivate
using namespace Monstey_ModelPrivate;

Monstey_Json Monstey_ModelInspectSkeleton(std::span<const std::uint8_t> bytes)
{
    return Monstey_ModelInspectSkeleton(bytes, Monstey_J::object());
}

Monstey_Json Monstey_ModelInspectSkeleton(std::span<const std::uint8_t> bytes, const Monstey_J &options)
{
    Monstey_MRequire(options.is_object(), "SKEL inspection options need an object");
    for (const auto &[key, value] : options.as_object())
    {
        (void)value;
        Monstey_MRequire(key == "table_offset" || key == "table_limit" || key == "row_offset" || key == "row_limit",
                         "Unknown SKEL inspection option");
    }
    auto number = [&](const char *name, std::uint32_t fallback, std::uint32_t maximum)
    {
        const auto value = options.value(name, Monstey_J(fallback)).as_uint();
        Monstey_MRequire(value <= maximum, "SKEL metadata paging limit exceeded");
        return static_cast<std::uint32_t>(value);
    };
    Monstey_SkelPage page{number("table_offset", 0, 0xffffffffu), number("table_limit", 4096, 4096),
                          number("row_offset", 0, 0xffffffffu), number("row_limit", 10000, 10000)};
    const auto skeleton = Monstey_ReadSkel(bytes, &page);
    const auto resources = Monstey_ReadSkelClothResources(bytes, skeleton);
    auto tables = skeleton.piece_tables;
    for (auto &table : tables.as_array())
        for (auto &row : table["rows"].as_array())
        {
            const auto index = row["word10"].as_uint();
            const bool sentinel = index == 0xffffffffu;
            row["cloth_resource_index"] = index;
            row["cloth_resource_path"] = nullptr;
            row["cloth_reference_valid"] = sentinel ? Monstey_J(true) : Monstey_J(nullptr);
            if (!sentinel && resources.qualified)
            {
                const bool referenceValid = index < resources.names.size();
                row["cloth_reference_valid"] = referenceValid;
                if (referenceValid)
                    row["cloth_resource_path"] = resources.names[static_cast<std::size_t>(index)];
            }
        }
    auto names = Monstey_J::array(), entries = Monstey_J::array();
    std::size_t roots = 0;
    bool finite = true, valid = true;
    for (std::size_t i = 0; i < skeleton.bones.size(); ++i)
    {
        const auto &entry = skeleton.bones[i];
        const auto &bone = entry.bone;
        names.as_array().push_back(bone.name);
        if (bone.parent < 0)
            ++roots;
        finite = finite && entry.finite;
        auto translation = Monstey_J::array(), quaternion = Monstey_J::array();
        for (unsigned k = 0; k < 7; ++k)
        {
            const auto bits = entry.pose_bits[k];
            auto value = (bits & 0x7f800000u) == 0x7f800000u
                             ? Monstey_J::object({{"nonfinite", true}, {"float32_bits", Monstey_Hex(bits, 8)}})
                             : Monstey_J(static_cast<double>(std::bit_cast<float>(bits)));
            (k < 3 ? translation : quaternion).as_array().push_back(value);
        }
        double norm = 0;
        for (const auto q : bone.q)
            norm += q * q;
        const bool bindValid = entry.finite && std::abs(norm - 1) < 1e-3;
        valid = valid && bindValid;
        Monstey_J matrix = nullptr;
        if (bindValid)
        {
            auto metres = bone.t;
            for (auto &v : metres)
                v /= 100;
            matrix = Monstey_MatrixJson(Monstey_Transform(metres, bone.q, {1, 1, 1}));
        }
        entries.as_array().push_back(
            Monstey_J::object({{"source_index", static_cast<std::uint64_t>(i)},
                               {"name", bone.name},
                               {"parent_source", bone.parent},
                               {"children", bone.children},
                               {"active", entry.active},
                               {"raw_flag_byte", static_cast<std::uint64_t>(bytes[skeleton.bone_offset + i * 37 + 4])},
                               {"position_channel_enabled", (bytes[skeleton.bone_offset + i * 37 + 4] & 1u) == 0},
                               {"rotation_channel_enabled", true},
                               {"translation_cm", translation},
                               {"quaternion_xyzw", quaternion},
                               {"finite_bind", entry.finite},
                               {"valid_bind_matrix", bindValid},
                               {"global_bind_metres", matrix}}));
    }
    return Monstey_J::object(
        {{"format", "SKEL17"},
         {"source_sha256", skeleton.sha256},
         {"bytes", static_cast<std::uint64_t>(bytes.size())},
         {"bones", static_cast<std::uint64_t>(skeleton.bones.size())},
         {"bone_names", names},
         {"entries", entries},
         {"root_count", static_cast<std::uint64_t>(roots)},
         {"finite_bindings", finite},
         {"valid_bind_matrices", valid},
         {"table_count", skeleton.table_count},
         {"string_pool_bytes", skeleton.string_bytes},
         {"bone_data_offset", static_cast<std::uint64_t>(skeleton.bone_offset)},
         {"trailing_bytes", static_cast<std::uint64_t>(bytes.size() - skeleton.bone_end)},
         {"trailing_offset", static_cast<std::uint64_t>(skeleton.bone_end)},
         {"trailing_sha256", Monstey_HashSha256(bytes.subspan(skeleton.bone_end))},
         {"cloth_resources", resources.metadata},
         {"piece_lod_metadata",
          Monstey_J::object({{"native_reader_rva", "0x4B84E8"},
                             {"disk_row_bytes", 14},
                             {"runtime_row_bytes", 16},
                             {"disk_row_layout", "U8,U8,F32,U32,U32"},
                             {"native_derived_flag_offset", 2},
                             {"native_derived_flag_rule", "word10 != 0xffffffff"},
                             {"source_offset", 96},
                             {"source_bytes", static_cast<std::uint64_t>(skeleton.prefix_end - 96)},
                             {"sha256", Monstey_HashSha256(bytes.subspan(96, skeleton.prefix_end - 96))},
                             {"tables_total", skeleton.table_count},
                             {"rows_total", skeleton.piece_rows},
                             {"table_offset", page.table_offset},
                             {"table_limit", page.table_limit},
                             {"row_offset", page.row_offset},
                             {"row_limit", page.row_limit},
                             {"tables_shown", skeleton.piece_tables.size()},
                             {"rows_shown", skeleton.shown_rows},
                             {"tables_truncated", skeleton.piece_tables.size() != skeleton.table_count},
                             {"rows_truncated", skeleton.shown_rows != skeleton.piece_rows},
                             {"tables", tables},
                             {"row_semantics_qualified", false},
                             {"authoring_supported", false}})},
         {"bind_matrix_layout", "column_major"},
         {"runtime_verified", false}});
}

} // namespace Monstey
