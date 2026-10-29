#include "Monstey_BlendGeometry.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <set>
#include <windows.h>
namespace Monstey {
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
constexpr std::string_view Monstey_BGTriangleBandAttribute = "monstey_triangle_band";
void Monstey_BGRequire(bool ok, std::string_view text) {
    if (!ok)
        throw Monstey_Error("blend_model", std::string(text));
}
std::size_t Monstey_BGIndex(const J &row) {
    return static_cast<std::size_t>(row[row.contains("index") ? "index" : "block"].as_uint());
}
std::uint64_t Monstey_BGAddress(const J &row) { return row["address"].as_uint(); }
std::uint64_t Monstey_BGPtr(Monstey_Blend &b, std::size_t i, std::string_view f) {
    return b.ReadField(i, f)["pointer"].as_uint();
}
std::uint64_t Monstey_BGData(Monstey_Blend &b, B bytes) {
    return Monstey_BGAddress(b.NewData(std::move(bytes)));
}
std::uint64_t Monstey_BGString(Monstey_Blend &b, std::string_view text) {
    B bytes(text.begin(), text.end());
    bytes.push_back(0);
    return Monstey_BGData(b, std::move(bytes));
}
J Monstey_BGVec(std::initializer_list<double> v) {
    J j = J::array();
    for (auto x : v)
        j.as_array().push_back(x);
    return j;
}
std::string Monstey_BGReadString(Monstey_Blend &b, std::uint64_t p, std::size_t n, std::size_t owner) {
    auto data = b.BytesAtAddress(p, n, owner);
    auto z = std::find(data.begin(), data.end(), 0);
    Monstey_BGRequire(z != data.end(), "Unterminated Blender string");
    return std::string(data.begin(), z);
}
std::size_t Monstey_BGOwner(Monstey_Blend &b, std::size_t id) {
    // A scene's master Collection is an embedded ID serialized as DATA beneath SC.
    const auto scenes = b.Blocks("SC")["rows"];
    for (const auto &row : scenes.as_array()) {
        const auto si = Monstey_BGIndex(row);
        const auto pointer = Monstey_BGPtr(b, si, "master_collection");
        if (pointer && b.BlockAtAddress(pointer, si) == id)
            return si;
    }
    return id;
}
void Monstey_BGList(Monstey_Blend &b, std::size_t owner, std::string_view field, const std::vector<J> &rows) {
    const auto first = rows.empty() ? 0 : Monstey_BGAddress(rows.front()),
               last = rows.empty() ? 0 : Monstey_BGAddress(rows.back());
    b.SetField(owner, std::string(field) + ".first", first);
    b.SetField(owner, std::string(field) + ".last", last);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        b.SetField(Monstey_BGIndex(rows[i]), "prev", i ? Monstey_BGAddress(rows[i - 1]) : 0);
        b.SetField(Monstey_BGIndex(rows[i]), "next",
                   i + 1 < rows.size() ? Monstey_BGAddress(rows[i + 1]) : 0);
    }
}
void Monstey_BGId(Monstey_Blend &b, std::size_t i, std::string name) {
    b.SetField(i, "id.name", name);
    b.SetField(i, "id.us", 1);
}
J Monstey_BGObject(Monstey_Blend &b, std::string name, int type, std::uint64_t data) {
    auto row = b.NewBlock("OB", "Object");
    auto i = Monstey_BGIndex(row);
    Monstey_BGId(b, i, "OB" + name);
    b.SetField(i, "type", type);
    b.SetField(i, "data", data);
    b.SetField(i, "size", Monstey_BGVec({1, 1, 1}));
    b.SetField(i, "dsize", Monstey_BGVec({1, 1, 1}));
    b.SetField(i, "dscale", Monstey_BGVec({1, 1, 1}));
    b.SetField(i, "quat", Monstey_BGVec({1, 0, 0, 0}));
    b.SetField(i, "rotmode", 1);
    b.SetField(i, "rot", Monstey_BGVec({0, 0, 0}));
    b.SetField(i, "dquat", Monstey_BGVec({1, 0, 0, 0}));
    b.SetField(i, "dt", 2);
    b.SetField(i, "lay", 1);
    b.SetField(i, "col", Monstey_BGVec({1, 1, 1, 1}));
    b.SetField(i, "empty_drawsize", 1);
    J mat = J::array();
    for (unsigned k = 0; k < 16; ++k)
        mat.as_array().push_back(k % 5 == 0 ? 1.0 : 0.0);
    b.SetField(i, "parentinv", mat);
    b.SetField(i, "constinv", mat);
    return row;
}
void Monstey_BGAttribute(Monstey_Blend &b, std::size_t attrs, std::size_t ordinal, std::string name, int type,
                         int domain, B bytes, std::size_t count) {
    auto arr = b.NewBlock("DATA", "AttributeArray");
    auto ai = Monstey_BGIndex(arr);
    b.SetField(ai, "data", Monstey_BGData(b, std::move(bytes)));
    b.SetField(ai, "size", static_cast<std::uint64_t>(count));
    b.SetField(attrs, "name", Monstey_BGString(b, name), ordinal);
    b.SetField(attrs, "data_type", type, ordinal);
    b.SetField(attrs, "domain", domain, ordinal);
    b.SetField(attrs, "storage_type", 0, ordinal);
    b.SetField(attrs, "data", Monstey_BGAddress(arr), ordinal);
}
B Monstey_BGFloats(const std::vector<double> &values, double scale = 1) {
    B bytes;
    bytes.reserve(values.size() * 4);
    for (auto x : values)
        Monstey_AppendU32(bytes, std::bit_cast<std::uint32_t>(static_cast<float>(x * scale)));
    return bytes;
}
B Monstey_BGInts(const std::vector<std::uint32_t> &values) {
    B bytes;
    bytes.reserve(values.size() * 4);
    for (auto x : values)
        Monstey_AppendU32(bytes, x);
    return bytes;
}
bool Monstey_BGQualifiedTriangleBands(const Monstey_ModelPart &part) {
    if (part.triangle_band_topology_sha256.empty())
        return false;
    Monstey_BGRequire(part.indices.size() % 3 == 0 &&
                          part.triangle_bands.size() == part.indices.size() / 3,
                      "Native triangle band count does not match the index topology");
    for (const auto band : part.triangle_bands)
        Monstey_BGRequire(band <= 16, "Native triangle band must be in range 0..16");
    Monstey_BGRequire(part.triangle_band_topology_sha256 == Monstey_ModelTriangleTopologyHash(part.indices),
                      "Native triangle bands reference stale index topology; assign current face bands");
    return true;
}
std::map<std::string, Monstey_BlendGeometryAttribute> Monstey_BGAttributes(Monstey_Blend &b,
                                                                           std::size_t mesh) {
    auto ptr = Monstey_BGPtr(b, mesh, "attribute_storage.dna_attributes");
    const auto count = b.ReadField(mesh, "attribute_storage.dna_attributes_num").as_uint();
    Monstey_BGRequire(count < 1024, "Attribute count limit");
    std::map<std::string, Monstey_BlendGeometryAttribute> out;
    if (!count)
        return out;
    auto ab = b.BlockAtAddress(ptr, mesh);
    std::uint64_t expandedBytes = 0;
    constexpr std::uint64_t maximumBytes = 512ull * 1024 * 1024;
    for (std::size_t k = 0; k < count; ++k) {
        auto np = b.ReadField(ab, "name", k)["pointer"].as_uint();
        auto name = Monstey_BGReadString(b, np, b.BlockData(b.BlockAtAddress(np, ab)).size(), ab);
        Monstey_BlendGeometryAttribute attr;
        attr.type = static_cast<int>(b.ReadField(ab, "data_type", k).as_int());
        attr.domain = static_cast<int>(b.ReadField(ab, "domain", k).as_int());
        const auto storage = b.ReadField(ab, "storage_type", k).as_int();
        Monstey_BGRequire(storage == 0 || storage == 1, "Unsupported Blender attribute storage type");
        auto arr = b.BlockAtAddress(b.ReadField(ab, "data", k)["pointer"].as_uint(), ab);
        auto dp = Monstey_BGPtr(b, arr, "data");
        if (storage == 0) {
            attr.count = static_cast<std::size_t>(b.ReadField(arr, "size").as_uint());
            if (dp)
                attr.data = b.BlockData(b.BlockAtAddress(dp, arr));
            Monstey_BGRequire(attr.data.size() <= maximumBytes - expandedBytes,
                              "Expanded Blender attribute byte limit");
            expandedBytes += attr.data.size();
        } else {
            // AttributeSingle owns one typed value; logical length is the mesh domain.
            // Resolve both pointers through the existing ID-local SDNA engine.
            constexpr const char *domains[] = {"totvert", "totedge", "totpoly", "totloop"};
            Monstey_BGRequire(attr.domain >= 0 && attr.domain < 4,
                              "Single Blender attribute has an unsupported mesh domain");
            attr.count = static_cast<std::size_t>(b.ReadField(mesh, domains[attr.domain]).as_uint());
            std::size_t stride = 0;
            switch (attr.type) {
            case 0: // Bool
            case 1: // Int8
                stride = 1;
                break;
            case 2: // Int16 pair
            case 3: // Int32
            case 5: // Float
            case 9: // Byte RGBA
                stride = 4;
                break;
            case 4: // Int32 pair
            case 6: // Float2
                stride = 8;
                break;
            case 7: // Float3
                stride = 12;
                break;
            case 8: // Float4x4
                stride = 64;
                break;
            case 10: // Float RGBA
            case 11: // Quaternion
            case 13: // Float4
                stride = 16;
                break;
            default:
                throw Monstey_Error("blend_geometry", "Unsupported Blender single attribute data type " +
                                                        std::to_string(attr.type));
            }
            Monstey_BGRequire(attr.count <= (maximumBytes - expandedBytes) / stride,
                              "Expanded Blender single attribute byte limit");
            const auto bytes = attr.count * stride;
            if (bytes) {
                Monstey_BGRequire(dp != 0, "Single Blender attribute is missing its value");
                const auto value = b.BytesAtAddress(dp, stride, arr);
                attr.data.resize(bytes);
                for (std::size_t offset = 0; offset < bytes; offset += stride)
                    std::copy(value.begin(), value.end(), attr.data.begin() + static_cast<std::ptrdiff_t>(offset));
            }
            expandedBytes += bytes;
        }
        Monstey_BGRequire(out.emplace(name, std::move(attr)).second, "Duplicate Blender attribute");
    }
    return out;
}
std::vector<double> Monstey_BGReadFloatAttr(const Monstey_BlendGeometryAttribute &a, unsigned width,
                                            std::size_t count, int type, int domain, double scale = 1) {
    Monstey_BGRequire(a.type == type && a.domain == domain && a.count == count &&
                          a.data.size() == count * width * 4,
                      "Blender attribute shape/type mismatch");
    std::vector<double> v(count * width);
    for (std::size_t k = 0; k < v.size(); ++k)
        v[k] = static_cast<double>(std::bit_cast<float>(Monstey_U32(a.data, k * 4))) * scale;
    return v;
}
void Monstey_BGApplyCornerChannels(Monstey_ModelPart &part,
                                   const std::map<std::string, std::vector<double>> &corners) {
    if (corners.empty())
        return;
    const auto source = part.channels;
    const auto originalCount = source.at("POSITION").values.size() / 3;
    std::map<std::pair<std::uint32_t, std::vector<std::uint32_t>>, std::uint32_t> variants;
    std::vector<bool> assigned(originalCount, false);
    for (std::size_t corner = 0; corner < part.indices.size(); ++corner) {
        const auto old = part.indices[corner];
        Monstey_BGRequire(old < originalCount, "Corner source vertex outside point attributes");
        std::vector<std::uint32_t> key;
        for (const auto &[channel, values] : corners) {
            const auto width = source.at(channel).width;
            Monstey_BGRequire(values.size() == part.indices.size() * width, "Corner channel count mismatch");
            for (unsigned k = 0; k < width; ++k)
                key.push_back(std::bit_cast<std::uint32_t>(static_cast<float>(values[corner * width + k])));
        }
        auto it = variants.find({old, key});
        if (it != variants.end()) {
            part.indices[corner] = it->second;
            continue;
        }
        std::uint32_t next = old;
        if (assigned[old]) {
            next = static_cast<std::uint32_t>(part.channels.at("POSITION").values.size() / 3);
            for (auto &[channel, data] : part.channels) {
                const auto &oldData = source.at(channel);
                data.values.insert(
                    data.values.end(), oldData.values.begin() + static_cast<std::ptrdiff_t>(old * data.width),
                    oldData.values.begin() + static_cast<std::ptrdiff_t>((old + 1) * data.width));
            }
        } else
            assigned[old] = true;
        for (const auto &[channel, values] : corners) {
            auto &data = part.channels.at(channel);
            for (unsigned k = 0; k < data.width; ++k)
                data.values[next * data.width + k] = values[corner * data.width + k];
        }
        variants.emplace(std::pair{old, std::move(key)}, next);
        part.indices[corner] = next;
    }
}
} // namespace
namespace {
std::wstring Monstey_BGEnvironment(const wchar_t *name) {
    const auto length = GetEnvironmentVariableW(name, nullptr, 0);
    if (!length)
        return {};
    if (length > 32768)
        throw Monstey_Error("blender_template", "Blender environment path exceeds Windows limit");
    std::wstring value(length, L'\0');
    const auto read = GetEnvironmentVariableW(name, value.data(), length);
    if (!read || read >= length)
        throw Monstey_Error("blender_template", "Blender environment path changed during resolution");
    value.resize(read);
    return value;
}
std::filesystem::path Monstey_BGTemplateFile(const std::filesystem::path &path, std::string_view provenance) {
    if (path.empty() || !std::filesystem::is_regular_file(Monstey_WindowsFilePath(path)))
        throw Monstey_Error("blender_template",
                            std::string(provenance) +
                                " is not a readable template file: " + Monstey_Utf8(path.wstring()));
    return std::filesystem::absolute(path).lexically_normal();
}
std::vector<unsigned> Monstey_BGVersion(std::wstring_view name) {
    std::vector<unsigned> result;
    unsigned value = 0;
    bool digit = false;
    for (const auto c : name) {
        if (c == L'.') {
            if (!digit)
                return {};
            result.push_back(value);
            value = 0;
            digit = false;
        } else {
            if (c < L'0' || c > L'9' || value > 100000)
                return {};
            value = value * 10 + static_cast<unsigned>(c - L'0');
            digit = true;
        }
    }
    if (!digit)
        return {};
    result.push_back(value);
    return result;
}
std::filesystem::path Monstey_BGTemplateAtExecutable(const std::filesystem::path &executable,
                                                     std::string_view provenance) {
    if (executable.empty() || !std::filesystem::is_regular_file(Monstey_WindowsFilePath(executable)))
        throw Monstey_Error("blender_template", std::string(provenance) + " executable does not exist: " +
                                                    Monstey_Utf8(executable.wstring()));
    const auto parent = std::filesystem::absolute(executable).parent_path();
    constexpr auto suffix = L"scripts/startup/bl_app_templates_system/Sculpting/startup.blend";
    const auto direct = parent / suffix;
    if (std::filesystem::is_regular_file(Monstey_WindowsFilePath(direct)))
        return direct.lexically_normal();
    std::vector<std::pair<std::vector<unsigned>, std::filesystem::path>> candidates;
    for (const auto &entry : std::filesystem::directory_iterator(Monstey_WindowsFilePath(parent))) {
        if (!entry.is_directory())
            continue;
        const auto version = Monstey_BGVersion(entry.path().filename().wstring());
        if (version.empty())
            continue;
        const auto path = parent / entry.path().filename() / suffix;
        if (std::filesystem::is_regular_file(Monstey_WindowsFilePath(path)))
            candidates.emplace_back(version, path.lexically_normal());
    }
    if (candidates.empty())
        throw Monstey_Error("blender_template", std::string(provenance) +
                                                    " has no stock Sculpting startup template; supply "
                                                    "blend_template or MONSTEY_BLENDER_TEMPLATE: " +
                                                    Monstey_Utf8(parent.wstring()));
    std::sort(candidates.begin(), candidates.end());
    return candidates.back().second;
}
} // namespace
std::filesystem::path Monstey_BlendGeometryTemplate(const J &options) {
    if (options.contains("blend_template") && !options["blend_template"].is_null() &&
        !options["blend_template"].as_string().empty())
        return Monstey_BGTemplateFile(
            std::filesystem::path(Monstey_Wide(options["blend_template"].as_string())),
            "Explicit blend_template");
    const auto environmentTemplate = Monstey_BGEnvironment(L"MONSTEY_BLENDER_TEMPLATE");
    if (!environmentTemplate.empty())
        return Monstey_BGTemplateFile(environmentTemplate, "MONSTEY_BLENDER_TEMPLATE");
    if (options.contains("blender") && !options["blender"].is_null() &&
        !options["blender"].as_string().empty())
        return Monstey_BGTemplateAtExecutable(
            std::filesystem::path(Monstey_Wide(options["blender"].as_string())), "Configured Blender");
    const auto environmentExecutable = Monstey_BGEnvironment(L"MONSTEY_BLENDER");
    if (!environmentExecutable.empty())
        return Monstey_BGTemplateAtExecutable(environmentExecutable, "MONSTEY_BLENDER");
    return Monstey_BGTemplateAtExecutable(
        L"C:/Program Files (x86)/Steam/steamapps/common/Blender/blender.exe",
        "Compatibility Blender default");
}
J Monstey_BlendGeometryCreateMesh(Monstey_Blend &b, const Monstey_ModelPart &part,
                                  std::string_view requestedName, double positionScale) {
    Monstey_BGRequire(!requestedName.empty() && requestedName.find('\0') == std::string_view::npos,
                      "Mesh name is empty or contains NUL");
    Monstey_BGRequire(std::isfinite(positionScale) && positionScale != 0 &&
                          part.channels.contains("POSITION") && part.channels.at("POSITION").width == 3,
                      "Mesh needs a finite nonzero scale and POSITION VEC3");
    const auto pointValues = part.channels.at("POSITION").values.size();
    Monstey_BGRequire(pointValues > 0 && pointValues % 3 == 0 && pointValues / 3 <= 8000000 &&
                          !part.indices.empty() && part.indices.size() % 3 == 0 &&
                          part.indices.size() <= 24000000,
                      "Mesh vertex/triangle count limit");
    for (const auto index : part.indices)
        Monstey_BGRequire(index < pointValues / 3, "Mesh index outside point channels");
    for (const auto &[channel, data] : part.channels) {
        Monstey_BGRequire((data.width == 2 || data.width == 3 || data.width == 4) &&
                              data.values.size() == pointValues / 3 * data.width,
                          "Mesh point-channel shape mismatch");
        Monstey_BGRequire(!channel.starts_with("UV") || data.width == 2, "Mesh UV channels require VEC2");
        Monstey_BGRequire((channel != "NORMAL" && channel != "TANGENT" && channel != "BITANGENT") ||
                              data.width == 3,
                          "Mesh direction channels require VEC3");
        if (channel.starts_with("COLOR") || channel == "WEIGHTS_U8")
            for (const auto value : data.values)
                Monstey_BGRequire(data.width == 4 && std::isfinite(value) && value >= 0 && value <= 255 &&
                                      std::floor(value) == value,
                                  "Mesh byte color/weight channel requires integral RGBA bytes");
    }
    for (const auto value : part.channels.at("POSITION").values)
        Monstey_BGRequire(std::isfinite(value) && std::isfinite(static_cast<float>(value * positionScale)),
                          "Mesh position is not representable as finite Float32");
    const auto hasTriangleBands = Monstey_BGQualifiedTriangleBands(part);
    const auto count = part.channels.at("POSITION").values.size() / 3, faces = part.indices.size() / 3;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> edges;
    std::vector<std::uint32_t> edgeVerts, cornerEdges;
    for (std::size_t k = 0; k < part.indices.size(); ++k) {
        auto a = part.indices[k], c = part.indices[(k / 3) * 3 + (k + 1) % 3];
        auto key = std::minmax(a, c);
        auto [it, fresh] = edges.emplace(key, static_cast<std::uint32_t>(edges.size()));
        if (fresh) {
            edgeVerts.push_back(key.first);
            edgeVerts.push_back(key.second);
        }
        cornerEdges.push_back(it->second);
    }
    auto mesh = b.NewBlock("ME", "Mesh");
    const auto mi = Monstey_BGIndex(mesh);
    const std::string name(requestedName);
    Monstey_BGId(b, mi, "ME" + name);
    b.SetField(mi, "totvert", static_cast<std::uint64_t>(count));
    b.SetField(mi, "totedge", static_cast<std::uint64_t>(edges.size()));
    b.SetField(mi, "totpoly", static_cast<std::uint64_t>(faces));
    b.SetField(mi, "totloop", static_cast<std::uint64_t>(part.indices.size()));
    b.SetField(mi, "size", Monstey_BGVec({1, 1, 1}));
    std::vector<std::uint32_t> offsets;
    for (std::size_t k = 0; k <= faces; ++k)
        offsets.push_back(static_cast<std::uint32_t>(k * 3));
    b.SetField(mi, "poly_offset_indices", Monstey_BGData(b, Monstey_BGInts(offsets)));
    std::size_t uvCount = 0;
    for (const auto &[channel, data] : part.channels) {
        (void)data;
        if (channel.starts_with("UV"))
            ++uvCount;
    }
    const auto attrCount = part.channels.size() + 3 + uvCount + (hasTriangleBands ? 1 : 0);
    auto attrs = b.NewBlock("DATA", "Attribute", attrCount);
    auto ati = Monstey_BGIndex(attrs);
    b.SetField(mi, "attribute_storage.dna_attributes", Monstey_BGAddress(attrs));
    b.SetField(mi, "attribute_storage.dna_attributes_num", static_cast<std::uint64_t>(attrCount));
    std::size_t ordinal = 0;
    Monstey_BGAttribute(b, ati, ordinal++, ".edge_verts", 4, 1, Monstey_BGInts(edgeVerts), edges.size());
    Monstey_BGAttribute(b, ati, ordinal++, ".corner_vert", 3, 3, Monstey_BGInts(part.indices),
                        part.indices.size());
    Monstey_BGAttribute(b, ati, ordinal++, ".corner_edge", 3, 3, Monstey_BGInts(cornerEdges),
                        part.indices.size());
    if (hasTriangleBands) {
        std::vector<std::uint32_t> bands;
        bands.reserve(faces);
        for (const auto band : part.triangle_bands)
            bands.push_back(static_cast<std::uint32_t>(band) + 1);
        Monstey_BGAttribute(b, ati, ordinal++, std::string(Monstey_BGTriangleBandAttribute), 3, 2,
                            Monstey_BGInts(bands), faces);
    }
    for (const auto &[channel, data] : part.channels) {
        auto nameAttr = channel == "POSITION" ? "position"
                        : channel == "NORMAL" ? "custom_normal"
                                              : "_SOW_" + channel;
        int type = data.width == 2 ? 6 : data.width == 3 ? 7 : 13;
        B bytes;
        auto values = data.values;
        if (channel.starts_with("COLOR") || channel == "WEIGHTS_U8") {
            type = 9;
            for (auto value : values)
                bytes.push_back(static_cast<std::uint8_t>(value));
        } else
            bytes = Monstey_BGFloats(values, channel == "POSITION" ? positionScale : 1);
        Monstey_BGAttribute(b, ati, ordinal++, nameAttr, type, 0, std::move(bytes), count);
    }
    for (const auto &[channel, data] : part.channels)
        if (channel.starts_with("UV")) {
            const auto uvName = channel == "UV0" ? "UVMap" : "UVMap" + channel.substr(2);
            std::vector<double> corner;
            corner.reserve(part.indices.size() * 2);
            for (auto index : part.indices) {
                corner.push_back(data.values[index * 2]);
                corner.push_back(data.values[index * 2 + 1]);
            }
            Monstey_BGAttribute(b, ati, ordinal++, uvName, 6, 3, Monstey_BGFloats(corner),
                                part.indices.size());
            if (channel == "UV0") {
                b.SetField(mi, "active_uv_map_attribute", Monstey_BGString(b, uvName));
                b.SetField(mi, "default_uv_map_attribute", Monstey_BGString(b, uvName));
            }
        }
    return mesh;
}
Monstey_ModelPart
Monstey_BlendGeometryReadMesh(Monstey_Blend &b, std::size_t mesh, const Monstey_ModelPart &baseline,
                              double positionScale,
                              const std::function<void(Monstey_ModelPart &)> &beforeCornerSplit,
                              std::optional<std::uint8_t> uniformTriangleBand) {
    Monstey_BGRequire(std::isfinite(positionScale) && positionScale != 0,
                      "Mesh read scale must be finite and nonzero");
    Monstey_BGRequire(!uniformTriangleBand || *uniformTriangleBand <= 16,
                      "Explicit triangle band must be ordinal 0-16");
    auto part = baseline;
    part.triangle_bands.clear();
    part.triangle_band_topology_sha256.clear();
    const auto count = static_cast<std::size_t>(b.ReadField(mesh, "totvert").as_uint());
    const auto corners = static_cast<std::size_t>(b.ReadField(mesh, "totloop").as_uint());
    const auto faces = static_cast<std::size_t>(b.ReadField(mesh, "totpoly").as_uint());
    Monstey_BGRequire(count > 0 && count <= 8000000 && corners > 0 && corners <= 24000000 &&
                          corners == faces * 3,
                      "Mesh read requires bounded triangulated geometry");
    const auto attrs = Monstey_BGAttributes(b, mesh);
    Monstey_BGRequire(attrs.contains("position") && attrs.contains(".corner_vert"),
                      "Mesh position/corner attributes missing");
    part.channels["POSITION"] = {
        3, Monstey_BGReadFloatAttr(attrs.at("position"), 3, count, 7, 0, positionScale)};
    for (const auto value : part.channels["POSITION"].values)
        Monstey_BGRequire(std::isfinite(value), "Mesh contains nonfinite positions");
    const auto &ci = attrs.at(".corner_vert");
    Monstey_BGRequire(ci.type == 3 && ci.domain == 3 && ci.count == corners && ci.data.size() == corners * 4,
                      "Invalid mesh corner vertex attribute");
    part.indices.clear();
    part.indices.reserve(corners);
    for (std::size_t k = 0; k < corners; ++k) {
        const auto index = Monstey_U32(ci.data, k * 4);
        Monstey_BGRequire(index < count, "Mesh corner vertex is outside point attributes");
        part.indices.push_back(index);
    }
    std::vector<std::uint8_t> triangleBands;
    const auto authoredBands = attrs.find(std::string(Monstey_BGTriangleBandAttribute));
    if (authoredBands != attrs.end()) {
        const auto &a = authoredBands->second;
        Monstey_BGRequire(a.type == 3 && a.domain == 2 && a.count == faces && a.data.size() == faces * 4,
                          "monstey_triangle_band requires FACE-domain INT data with one value per triangle");
        triangleBands.reserve(faces);
        for (std::size_t face = 0; face < faces; ++face) {
            const auto stored = Monstey_U32(a.data, face * 4);
            Monstey_BGRequire(stored <= 17 && (stored >= 1 || uniformTriangleBand.has_value()),
                              "monstey_triangle_band face " + std::to_string(face) +
                                  " is unassigned or outside 1..17; assign a native triangle band");
            triangleBands.push_back(uniformTriangleBand ? *uniformTriangleBand
                                                      : static_cast<std::uint8_t>(stored - 1));
        }
    } else if (uniformTriangleBand) {
        triangleBands.assign(faces, *uniformTriangleBand);
    } else if (Monstey_BGQualifiedTriangleBands(baseline)) {
        if (part.indices == baseline.indices)
            triangleBands = baseline.triangle_bands;
        else {
            Monstey_BGRequire(!baseline.triangle_bands.empty() &&
                                  std::all_of(baseline.triangle_bands.begin(), baseline.triangle_bands.end(),
                                              [&](auto band) { return band == baseline.triangle_bands.front(); }),
                              "monstey_triangle_band is missing after topology changed; nonuniform native "
                              "triangle bands require explicit face assignments");
            triangleBands.assign(faces, baseline.triangle_bands.front());
        }
    }
    for (auto &[channel, data] : part.channels) {
        if (channel == "POSITION")
            continue;
        const auto name = channel == "NORMAL" ? "custom_normal" : "_SOW_" + channel;
        Monstey_BGRequire(attrs.contains(name), "Mesh point channel missing: " + name);
        const auto &a = attrs.at(name);
        if (channel.starts_with("COLOR") || channel == "WEIGHTS_U8") {
            Monstey_BGRequire(data.width == 4 && a.type == 9 && a.domain == 0 && a.count == count &&
                                  a.data.size() == count * 4,
                              "Mesh byte channel shape changed");
            data.values.assign(a.data.begin(), a.data.end());
        } else
            data.values = Monstey_BGReadFloatAttr(a, data.width, count,
                                                  data.width == 2   ? 6
                                                  : data.width == 3 ? 7
                                                                    : 13,
                                                  0);
    }
    if (beforeCornerSplit)
        beforeCornerSplit(part);
    std::map<std::string, std::vector<double>> cornerChannels;
    for (const auto &[channel, data] : part.channels)
        if (channel.starts_with("UV")) {
            const auto name = channel == "UV0" ? "UVMap" : "UVMap" + channel.substr(2);
            if (attrs.contains(name))
                cornerChannels[channel] = Monstey_BGReadFloatAttr(attrs.at(name), data.width, corners, 6, 3);
        }
    Monstey_BGApplyCornerChannels(part, cornerChannels);
    if (!triangleBands.empty())
        Monstey_ModelSetTriangleBands(part, triangleBands);
    return part;
}
J Monstey_BlendGeometryCreateObject(Monstey_Blend &b, std::string_view name, int type,
                                    std::uint64_t dataAddress) {
    Monstey_BGRequire(!name.empty() && name.find('\0') == std::string_view::npos,
                      "Object name is empty or contains NUL");
    return Monstey_BGObject(b, std::string(name), type, dataAddress);
}
std::map<std::string, Monstey_BlendGeometryAttribute> Monstey_BlendGeometryAttributes(Monstey_Blend &b,
                                                                                      std::size_t mesh) {
    return Monstey_BGAttributes(b, mesh);
}
namespace {
std::vector<J> Monstey_BGFollow(Monstey_Blend &b, std::size_t owner, std::string_view list) {
    auto pointer = Monstey_BGPtr(b, owner, std::string(list) + ".first");
    std::vector<J> rows;
    std::set<std::uint64_t> seen;
    while (pointer) {
        Monstey_BGRequire(rows.size() < 1000000 && seen.insert(pointer).second,
                          "Cyclic or oversized Blender list");
        const auto i = b.BlockAtAddress(pointer, owner);
        rows.push_back(J::object({{"block", static_cast<std::uint64_t>(i)}, {"address", pointer}}));
        pointer = Monstey_BGPtr(b, i, "next");
    }
    return rows;
}
} // namespace
void Monstey_BlendGeometrySetProperty(Monstey_Blend &b, std::size_t id, std::string_view name,
                                      const J &value) {
    Monstey_BGRequire(!name.empty() && name.size() < 64 && name.find('\0') == std::string_view::npos,
                      "Property name is empty, too long or contains NUL");
    auto groupPointer = Monstey_BGPtr(b, id, "id.properties");
    if (!groupPointer) {
        auto group = b.NewBlock("DATA", "IDProperty");
        const auto gi = Monstey_BGIndex(group);
        b.SetDataOwner(gi, Monstey_BGOwner(b, id));
        b.SetField(gi, "type", 6);
        b.SetField(gi, "name", "Monstey");
        groupPointer = Monstey_BGAddress(group);
        b.SetField(id, "id.properties", groupPointer);
    }
    const auto gi = b.BlockAtAddress(groupPointer, id);
    auto rows = Monstey_BGFollow(b, gi, "data.group");
    auto found = std::find_if(rows.begin(), rows.end(), [&](const auto &row) {
        return b.ReadField(Monstey_BGIndex(row), "name").as_string() == name;
    });
    const auto property = found != rows.end() ? *found : b.NewBlock("DATA", "IDProperty");
    const auto pi = Monstey_BGIndex(property);
    b.SetDataOwner(pi, Monstey_BGOwner(b, id));
    b.SetField(pi, "name", std::string(name));
    if (value.is_string() || value.is_array() || value.is_object()) {
        const auto text = value.is_string() ? value.as_string() : value.dump();
        Monstey_BGRequire(text.find('\0') == std::string::npos && text.size() < 16ull * 1024 * 1024,
                          "Property string has embedded NUL or exceeds limit");
        const auto pointer = Monstey_BGString(b, text);
        b.SetDataOwner(b.BlockAtAddress(pointer), Monstey_BGOwner(b, id));
        b.SetField(pi, "type", 0);
        b.SetField(pi, "subtype", 0);
        b.SetField(pi, "len", static_cast<std::uint64_t>(text.size() + 1));
        b.SetField(pi, "totallen", static_cast<std::uint64_t>(text.size() + 1));
        b.SetField(pi, "data.pointer", pointer);
    } else if (value.is_bool() || value.is_integer()) {
        const auto number = value.is_bool() ? (value.as_bool() ? 1ll : 0ll) : value.as_int();
        Monstey_BGRequire(number >= INT32_MIN && number <= INT32_MAX, "Property integer exceeds int32");
        b.SetField(pi, "type", 1);
        b.SetField(pi, "data.val", number);
    } else if (value.is_number()) {
        const auto number = value.as_number();
        Monstey_BGRequire(std::isfinite(number), "Property number must be finite");
        const auto bits = std::bit_cast<std::uint64_t>(number);
        b.SetField(pi, "type", 8);
        b.SetField(pi, "data.val", std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits)));
        b.SetField(pi, "data.val2", std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32)));
    } else
        throw Monstey_Error("blend_geometry", "Property value must be scalar or serialized JSON");
    if (found == rows.end())
        rows.push_back(property);
    Monstey_BGList(b, gi, "data.group", rows);
    b.SetField(gi, "len", static_cast<std::uint64_t>(rows.size()));
}
J Monstey_BlendGeometryProperties(Monstey_Blend &b, std::size_t id) {
    auto result = J::object();
    const auto group = Monstey_BGPtr(b, id, "id.properties");
    if (!group)
        return result;
    const auto gi = b.BlockAtAddress(group, id);
    for (const auto &row : Monstey_BGFollow(b, gi, "data.group")) {
        const auto pi = Monstey_BGIndex(row);
        const auto name = b.ReadField(pi, "name").as_string();
        const auto type = b.ReadField(pi, "type").as_int();
        if (type == 0) {
            const auto len = b.ReadField(pi, "len").as_uint();
            Monstey_BGRequire(len > 0 && len <= 16ull * 1024 * 1024, "Property string extent limit");
            result[name] = Monstey_BGReadString(b, Monstey_BGPtr(b, pi, "data.pointer"),
                                                static_cast<std::size_t>(len), id);
        } else if (type == 1)
            result[name] = b.ReadField(pi, "data.val");
        else if (type == 2) {
            const auto bits = static_cast<std::uint32_t>(b.ReadField(pi, "data.val").as_int());
            const auto number = std::bit_cast<float>(bits);
            Monstey_BGRequire(std::isfinite(number), "Property Float32 is nonfinite");
            result[name] = number;
        } else if (type == 8) {
            const auto lo = static_cast<std::uint32_t>(b.ReadField(pi, "data.val").as_int());
            const auto hi = static_cast<std::uint32_t>(b.ReadField(pi, "data.val2").as_int());
            const auto number = std::bit_cast<double>((static_cast<std::uint64_t>(hi) << 32) | lo);
            Monstey_BGRequire(std::isfinite(number), "Property Float64 is nonfinite");
            result[name] = number;
        } else
            result[name] = J::object({{"unsupported_type", type}});
    }
    return result;
}
std::size_t Monstey_BlendGeometryRootCollection(Monstey_Blend &b) {
    const auto scenes = b.Blocks("SC")["rows"];
    Monstey_BGRequire(!scenes.empty(), "Blender template has no scene");
    const auto si = Monstey_BGIndex(scenes[0u]);
    const auto pointer = Monstey_BGPtr(b, si, "master_collection");
    Monstey_BGRequire(pointer != 0, "Blender scene has no root collection");
    return b.BlockAtAddress(pointer);
}
J Monstey_BlendGeometryCollection(Monstey_Blend &b, std::string_view name) {
    Monstey_BGRequire(!name.empty() && name.find('\0') == std::string_view::npos,
                      "Collection name is empty or contains NUL");
    auto row = b.NewBlock("GR", "Collection");
    Monstey_BGId(b, Monstey_BGIndex(row), "GR" + std::string(name));
    return row;
}
void Monstey_BlendGeometryLinkCollection(Monstey_Blend &b, std::size_t parent, const J &child) {
    auto rows = Monstey_BGFollow(b, parent, "children");
    const auto pointer = Monstey_BGAddress(child);
    Monstey_BGRequire(pointer != 0 && Monstey_BGIndex(child) != parent, "Cannot link a collection to itself");
    for (const auto &row : rows)
        Monstey_BGRequire(Monstey_BGPtr(b, Monstey_BGIndex(row), "collection") != pointer,
                          "Collection is already linked to parent");
    auto link = b.NewBlock("DATA", "CollectionChild");
    const auto li = Monstey_BGIndex(link);
    b.SetDataOwner(li, Monstey_BGOwner(b, parent));
    b.SetField(li, "collection", pointer);
    rows.push_back(link);
    Monstey_BGList(b, parent, "children", rows);
}
void Monstey_BlendGeometryLinkObjects(Monstey_Blend &b, std::size_t collection, const std::vector<J> &objects,
                                      bool replace) {
    auto rows = replace ? std::vector<J>{} : Monstey_BGFollow(b, collection, "gobject");
    std::set<std::uint64_t> present;
    for (const auto &row : rows)
        present.insert(Monstey_BGPtr(b, Monstey_BGIndex(row), "ob"));
    for (const auto &object : objects) {
        const auto pointer = Monstey_BGAddress(object);
        Monstey_BGRequire(pointer != 0, "Cannot link null object");
        if (!present.insert(pointer).second)
            continue;
        auto link = b.NewBlock("DATA", "CollectionObject");
        const auto li = Monstey_BGIndex(link);
        b.SetDataOwner(li, Monstey_BGOwner(b, collection));
        b.SetField(li, "ob", pointer);
        rows.push_back(link);
    }
    Monstey_BGList(b, collection, "gobject", rows);
}
J Monstey_BlendGeometryObjects(Monstey_Blend &b) {
    auto result = J::array();
    std::size_t offset = 0;
    while (true) {
        const auto rows = b.Blocks("OB", offset, 250)["rows"];
        for (const auto &row : rows.as_array()) {
            const auto oi = Monstey_BGIndex(row);
            auto object = J::object({{"block", static_cast<std::uint64_t>(oi)},
                                     {"address", Monstey_BGAddress(row)},
                                     {"name", b.ReadField(oi, "id.name").as_string().substr(2)},
                                     {"type", b.ReadField(oi, "type")},
                                     {"data", Monstey_BGPtr(b, oi, "data")},
                                     {"parent", Monstey_BGPtr(b, oi, "parent")},
                                     {"properties", Monstey_BlendGeometryProperties(b, oi)}});
            for (const auto field : {"loc", "rot", "quat", "size", "dloc", "drot", "dquat", "dscale",
                                     "rotAxis", "rotAngle", "drotAxis", "drotAngle", "rotmode", "parentinv"})
                object[field] = b.ReadField(oi, field);
            result.as_array().push_back(std::move(object));
        }
        if (rows.size() < 250)
            break;
        offset += rows.size();
    }
    return result;
}
void Monstey_BlendGeometryWriteText(Monstey_Blend &b, std::string_view name, std::string_view text) {
    Monstey_BGRequire(!name.empty() && name.find('\0') == std::string_view::npos &&
                          text.find('\0') == std::string_view::npos && text.size() < 256ull * 1024 * 1024,
                      "Text name/content is empty, contains NUL or exceeds limit");
    const auto tx = b.NewBlock("TX", "Text");
    const auto ti = Monstey_BGIndex(tx);
    Monstey_BGId(b, ti, "TX" + std::string(name));
    b.SetField(ti, "id.flag", 512);
    b.SetField(ti, "flags", 1);
    std::vector<J> lines;
    std::size_t start = 0;
    do {
        const auto end = text.find('\n', start);
        const auto last = end == std::string_view::npos;
        const auto value = text.substr(start, last ? text.size() - start : end - start);
        auto line = b.NewBlock("DATA", "TextLine");
        const auto li = Monstey_BGIndex(line);
        b.SetDataOwner(li, ti);
        const auto pointer = Monstey_BGString(b, value);
        b.SetDataOwner(b.BlockAtAddress(pointer), ti);
        b.SetField(li, "line", pointer);
        b.SetField(li, "len", static_cast<std::uint64_t>(value.size()));
        lines.push_back(line);
        if (last)
            break;
        start = end + 1;
    } while (start <= text.size());
    Monstey_BGList(b, ti, "lines", lines);
    b.SetField(ti, "curl", Monstey_BGAddress(lines[0]));
    b.SetField(ti, "sell", Monstey_BGAddress(lines[0]));
}
std::string Monstey_BlendGeometryReadText(Monstey_Blend &b, std::string_view name) {
    std::size_t offset = 0;
    while (true) {
        const auto rows = b.Blocks("TX", offset, 250)["rows"];
        for (const auto &row : rows.as_array()) {
            const auto ti = Monstey_BGIndex(row);
            if (b.ReadField(ti, "id.name").as_string() != "TX" + std::string(name))
                continue;
            std::string result;
            bool first = true;
            for (const auto &line : Monstey_BGFollow(b, ti, "lines")) {
                const auto li = Monstey_BGIndex(line);
                const auto n = b.ReadField(li, "len").as_uint();
                Monstey_BGRequire(n < 256ull * 1024 * 1024 && result.size() + n < 256ull * 1024 * 1024,
                                  "Text content exceeds read limit");
                if (!first)
                    result += '\n';
                result += Monstey_BGReadString(b, Monstey_BGPtr(b, li, "line"),
                                               static_cast<std::size_t>(n) + 1, ti);
                first = false;
            }
            return result;
        }
        if (rows.size() < 250)
            break;
        offset += rows.size();
    }
    throw Monstey_Error("blend_geometry", "Named Text datablock is missing");
}
} // namespace Monstey