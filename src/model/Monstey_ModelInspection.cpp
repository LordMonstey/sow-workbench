#include "Monstey_ModelPrivate.hpp"

namespace Monstey
{
using namespace Monstey_ModelPrivate;

Monstey_Json Monstey_ModelNativePartMetadata(std::span<const std::uint8_t> bytes)
{
    Monstey_MRequire(bytes.size() <= 512ull * 1024 * 1024, "Native model metadata size limit");
    const auto native = Monstey_LoadModel(Monstey_Bytes(bytes.begin(), bytes.end()));
    auto parts = Monstey_J::array();
    std::size_t rowIndex = 0;
    for (std::size_t container = 0; container < native.lods.size(); ++container)
        for (std::size_t part = 0; part < native.lods[container][3]; ++part)
        {
            const auto &row = native.parts[rowIndex++];
            auto words = Monstey_J::array();
            for (const auto word : row)
                words.as_array().push_back(word);
            auto layout = Monstey_J::array();
            Monstey_J color = nullptr;
            for (const auto &attr : native.layouts[row[9]])
            {
                auto item =
                    Monstey_J::object({{"stream", attr.stream},
                                       {"type", attr.type},
                                       {"semantic", attr.semantic},
                                       {"channel", attr.channel},
                                       {"storage_bytes", static_cast<std::uint64_t>(Monstey_AttrBytes(attr.type))},
                                       {"storage_width", Monstey_AttrWidth(attr.type)}});
                layout.as_array().push_back(item);
                if (attr.semantic == 5 && attr.channel == 0)
                    color = item;
            }
            parts.as_array().push_back(Monstey_J::object({{"container", static_cast<std::uint64_t>(container)},
                                                          {"part", static_cast<std::uint64_t>(part)},
                                                          {"native_words", words},
                                                          {"draw_parameter_words", Monstey_J::array({row[7], row[8]})},
                                                          {"source_layout", layout},
                                                          {"color0_storage", color}}));
        }
    return Monstey_J::object({{"format", "MMSH17"},
                              {"source_sha256", Monstey_HashSha256(bytes)},
                              {"containers", static_cast<std::uint64_t>(native.lods.size())},
                              {"layouts", static_cast<std::uint64_t>(native.layouts.size())},
                              {"submeshes", static_cast<std::uint64_t>(native.parts.size())},
                              {"bones", native.h[16]},
                              {"parts", parts}});
}

Monstey_Json Monstey_ModelInspectBytes(std::span<const std::uint8_t> bytes, const std::filesystem::path &skel)
{
    return Monstey_ModelInspectBytes(bytes, skel, Monstey_J::object());
}

Monstey_Json Monstey_ModelInspectBytes(std::span<const std::uint8_t> bytes, const std::filesystem::path &skel,
                                       const Monstey_J &inspection)
{
    Monstey_MRequire(bytes.size() <= 512ull * 1024 * 1024, "Model inspection size limit");
    Monstey_MRequire(inspection.is_object(), "Model inspection options need an object");
    if (Monstey_U32(bytes, 0) == 0x4c454b53)
        return Monstey_ModelInspectSkeleton(bytes, inspection);
    Monstey_MRequire(inspection.empty(), "Piece table/row inspection options require a SKEL17 source");
    if (Monstey_U32(bytes, 0) == 0x46546c67)
    {
        const auto glb = Monstey_GlbEnvelope(Monstey_Bytes(bytes.begin(), bytes.end()));
        const auto &d = glb.doc;
        const auto count = [&](std::string_view key)
        { return d.contains(key) ? static_cast<std::uint64_t>(d[key].size()) : 0ull; };
        auto attributes = Monstey_J::array();
        if (d.contains("meshes"))
            for (const auto &mesh : d["meshes"].as_array())
                for (const auto &primitive : mesh["primitives"].as_array())
                {
                    auto names = Monstey_J::array();
                    for (const auto &[name, value] : primitive["attributes"].as_object())
                    {
                        (void)value;
                        names.as_array().push_back(name);
                    }
                    attributes.as_array().push_back(names);
                }
        return Monstey_J::object({{"format", "GLB2"},
                                  {"source_sha256", Monstey_HashSha256(bytes)},
                                  {"bytes", static_cast<std::uint64_t>(bytes.size())},
                                  {"primitives", static_cast<std::uint64_t>(attributes.size())},
                                  {"nodes", count("nodes")},
                                  {"meshes", count("meshes")},
                                  {"skins", count("skins")},
                                  {"animations", count("animations")},
                                  {"images", count("images")},
                                  {"materials", count("materials")},
                                  {"primitive_attributes", attributes},
                                  {"extensions_required", d.value("extensionsRequired", Monstey_J::array())},
                                  {"source_metadata", d.value("asset", Monstey_J::object())},
                                  {"rig_present", count("skins") > 0},
                                  {"runtime_verified", false}});
    }
    const auto m = Monstey_LoadModel(Monstey_Bytes(bytes.begin(), bytes.end()));
    const auto parts = Monstey_DecodeParts(m, Monstey_DecodePurpose::Inspection);
    auto items = Monstey_J::array();
    for (const auto &p : parts)
    {
        auto channels = Monstey_J::array();
        for (const auto &[name, ch] : p.channels)
            channels.as_array().push_back(
                Monstey_J::object({{"name", name},
                                   {"width", ch.width},
                                   {"nonfinite_components",
                                    static_cast<std::uint64_t>(std::count_if(ch.v.begin(), ch.v.end(), [](double v)
                                                                             { return !std::isfinite(v); }))}}));
        auto entry = Monstey_J::object({{"container", static_cast<std::uint64_t>(p.container)},
                                        {"part", static_cast<std::uint64_t>(p.part)},
                                        {"vertices", static_cast<std::uint64_t>(p.Count())},
                                        {"triangles", static_cast<std::uint64_t>(p.indices.size() / 3)},
                                        {"channels", channels}});
        entry["triangle_bands_qualified"] = Monstey_HasPartBands(p);
        auto bandCounts = Monstey_J::array();
        std::array<std::uint64_t, 17> counts{};
        for (const auto band : p.triangle_bands)
            ++counts[band];
        for (const auto count : counts)
            bandCounts.as_array().push_back(count);
        entry["triangle_band_counts"] = std::move(bandCounts);
        std::array<double, 3> lo = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
                                    std::numeric_limits<double>::max()},
                              hi = {std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest(),
                                    std::numeric_limits<double>::lowest()};
        bool finite = true;
        const auto &positions = p.channels.at("POSITION");
        for (std::size_t v = 0; v < p.Count(); ++v)
            for (unsigned k = 0; k < 3; ++k)
            {
                const auto value = positions.v[v * 3 + k];
                finite = finite && std::isfinite(value);
                if (std::isfinite(value))
                {
                    lo[k] = std::min(lo[k], value);
                    hi[k] = std::max(hi[k], value);
                }
            }
        entry["finite_positions"] = finite;
        entry["bounds_cm"] =
            finite
                ? Monstey_J::array({Monstey_J::array({lo[0], lo[1], lo[2]}), Monstey_J::array({hi[0], hi[1], hi[2]})})
                : Monstey_J(nullptr);
        if (p.channels.contains("WEIGHTS_U8"))
        {
            double minimum = std::numeric_limits<double>::max(), maximum = 0;
            const auto &w = p.channels.at("WEIGHTS_U8");
            for (std::size_t v = 0; v < p.Count(); ++v)
            {
                double sum = 0;
                for (unsigned k = 0; k < w.width; ++k)
                    sum += w.v[v * w.width + k];
                sum /= 255;
                minimum = std::min(minimum, sum);
                maximum = std::max(maximum, sum);
            }
            entry["weight_sum_minmax"] = Monstey_J::array({minimum, maximum});
        }
        items.as_array().push_back(std::move(entry));
    }
    Monstey_J rig;
    if (!skel.empty())
        rig = Monstey_RigJson(Monstey_ReadModelRig(m, skel));
    else
        try
        {
            rig = Monstey_RigJson(Monstey_ReadModelRig(m, {}));
        }
        catch (const Monstey_Error &error)
        {
            rig = Monstey_J::object({{"available", false}, {"error", error.what()}, {"skel_matching_verified", false}});
        }
    const auto allocation = Monstey_Allocation(m);
    return Monstey_J::object({{"format", "MMSH17"},
                              {"source_sha256", Monstey_HashSha256(bytes)},
                              {"bytes", static_cast<std::uint64_t>(bytes.size())},
                              {"containers", static_cast<std::uint64_t>(m.lods.size())},
                              {"bones", m.h[16]},
                              {"submeshes", static_cast<std::uint64_t>(m.parts.size())},
                              {"layouts", static_cast<std::uint64_t>(m.layouts.size())},
                              {"parts", items},
                              {"allocation", allocation},
                              {"native_allocations_valid", allocation["safe"]},
                              {"rig", rig},
                              {"runtime_verified", false}});
}

} // namespace Monstey
