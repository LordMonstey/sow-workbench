#include "Monstey_ModelPreview.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
namespace Monstey
{
namespace
{
using Monstey_PreviewVector = std::array<double, 3>;
struct Monstey_PreviewPoint
{
    double x = 0, y = 0, z = 0;
    std::array<double, 3> color{175, 183, 195};
};
constexpr std::uint32_t Monstey_PreviewBackground = 0xff1b1f25;
void Monstey_PreviewRequire(bool valid, const char *message)
{
    if (!valid)
        throw Monstey_Error("model_preview", message);
}
Monstey_PreviewVector Monstey_PreviewPosition(const Monstey_ModelChannel &channel, std::size_t i)
{
    return {channel.values[i * 3] / 100, channel.values[i * 3 + 1] / 100, channel.values[i * 3 + 2] / 100};
}
std::uint32_t Monstey_PreviewColor(const std::array<double, 3> &value, double light = 1)
{
    const auto byte = [light](double v) {
        return static_cast<std::uint32_t>(std::clamp(std::round(v * light), 0.0, 255.0));
    };
    return 0xff000000u | byte(value[0]) << 16 | byte(value[1]) << 8 | byte(value[2]);
}
double Monstey_PreviewEdge(const Monstey_PreviewPoint &a, const Monstey_PreviewPoint &b, double x, double y)
{
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}
void Monstey_PreviewLine(Monstey_ModelPreviewFrame &frame, std::vector<double> &depth, Monstey_PreviewPoint a,
                         Monstey_PreviewPoint b, std::uint32_t color, bool inFront)
{
    const double right = frame.width - 1, bottom = frame.height - 1;
    const auto region = [right, bottom](const Monstey_PreviewPoint &v) {
        return (v.x < 0 ? 1u : v.x > right ? 2u : 0u) | (v.y < 0 ? 4u : v.y > bottom ? 8u : 0u);
    };
    bool visible = false;
    // Set the selected boundary coordinate explicitly. Reconstructing both ends
    // from nearly equal global t values collapses e.g. [-1e20,+1e20] to a point.
    for (unsigned attempt = 0; attempt < 16; ++attempt)
    {
        const auto ca = region(a), cb = region(b);
        if (!(ca | cb))
        {
            visible = true;
            break;
        }
        if (ca & cb)
            return;
        const auto outside = ca ? ca : cb;
        const double dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
        Monstey_PreviewPoint clipped;
        double t = 0;
        if (outside & 4u)
        {
            clipped.y = 0;
            t = (clipped.y - a.y) / dy;
            clipped.x = std::fma(dx, t, a.x);
        }
        else if (outside & 8u)
        {
            clipped.y = bottom;
            t = (clipped.y - a.y) / dy;
            clipped.x = std::fma(dx, t, a.x);
        }
        else if (outside & 2u)
        {
            clipped.x = right;
            t = (clipped.x - a.x) / dx;
            clipped.y = std::fma(dy, t, a.y);
        }
        else
        {
            clipped.x = 0;
            t = (clipped.x - a.x) / dx;
            clipped.y = std::fma(dy, t, a.y);
        }
        clipped.z = std::fma(dz, t, a.z);
        Monstey_PreviewRequire(std::isfinite(clipped.x) && std::isfinite(clipped.y) && std::isfinite(clipped.z),
                               "Clipped preview segment is not finite");
        if (ca)
            a = clipped;
        else
            b = clipped;
    }
    if (!visible)
        return;
    const auto steps = static_cast<unsigned>(std::ceil(std::max(std::abs(b.x - a.x), std::abs(b.y - a.y))));
    for (unsigned k = 0; k <= steps; ++k)
    {
        const double t = steps ? double(k) / steps : 0;
        const auto x = static_cast<int>(std::lround(std::fma(b.x - a.x, t, a.x)));
        const auto y = static_cast<int>(std::lround(std::fma(b.y - a.y, t, a.y)));
        if (x < 0 || y < 0 || x >= static_cast<int>(frame.width) || y >= static_cast<int>(frame.height))
            continue;
        const auto at = static_cast<std::size_t>(y) * frame.width + static_cast<unsigned>(x);
        const double z = std::fma(b.z - a.z, t, a.z);
        if (inFront || z >= depth[at])
        {
            frame.pixels[at] = color;
            if (!inFront)
                depth[at] = z;
        }
    }
}

} // namespace
static Monstey_ModelPreviewFrame Monstey_PreviewRenderImpl(const Monstey_ModelScene &scene,
    const Monstey_ModelPreviewOptions &o, const Monstey_ModelScene* framing)
{
    Monstey_PreviewRequire(o.width >= 32 && o.height >= 32 && o.width <= 4096 && o.height <= 4096 &&
                               static_cast<std::uint64_t>(o.width) * o.height <= 8388608,
                           "Preview pixel dimensions exceed budget");
    Monstey_PreviewRequire(std::isfinite(o.yaw_degrees) && std::isfinite(o.pitch_degrees) && std::isfinite(o.zoom) &&
                               o.zoom >= 0.05 && o.zoom <= 20 && std::isfinite(o.pan_x) && std::isfinite(o.pan_y) &&
                               std::abs(o.pan_x) <= 8 && std::abs(o.pan_y) <= 8,
                           "Invalid preview view transform");
    std::vector<const Monstey_ModelPart *> parts;
    Monstey_PreviewVector minimum{}, maximum{};
    bool first = true;
    std::size_t vertices = 0, triangles = 0;
    for (const auto &p : scene.parts)
        if (o.all_containers || p.container == o.container)
        {
            const auto position = p.channels.find("POSITION");
            Monstey_PreviewRequire(position != p.channels.end() && position->second.width == 3 &&
                                       position->second.values.size() % 3 == 0,
                                   "Invalid preview position channel");
            const auto count = position->second.values.size() / 3;
            Monstey_PreviewRequire(count > 0 && p.indices.size() % 3 == 0, "Invalid preview topology");
            for (auto id : p.indices)
                Monstey_PreviewRequire(id < count, "Preview index outside vertices");
            if (o.vertex_colors && p.channels.contains("COLOR0"))
            {
                const auto &c = p.channels.at("COLOR0");
                Monstey_PreviewRequire(c.width == 4 && c.values.size() == count * 4, "Invalid preview color channel");
                for (auto v : c.values)
                    Monstey_PreviewRequire(std::isfinite(v) && v >= 0 && v <= 255, "Invalid preview color value");
            }
            for (std::size_t i = 0; i < count; ++i)
            {
                const auto v = Monstey_PreviewPosition(position->second, i);
                for (unsigned k = 0; k < 3; ++k)
                {
                    Monstey_PreviewRequire(std::isfinite(v[k]) && std::abs(v[k]) <= 1e38,
                                           "Nonfinite or unbounded preview position");
                    if (first)
                        minimum[k] = maximum[k] = v[k];
                    else
                    {
                        minimum[k] = std::min(minimum[k], v[k]);
                        maximum[k] = std::max(maximum[k], v[k]);
                    }
                }
                first = false;
            }
            vertices += count;
            triangles += p.indices.size() / 3;
            parts.push_back(&p);
        }
    Monstey_PreviewRequire(!first && !parts.empty(), "No geometry in the selected native container");
    if (framing) {
        bool referenceFirst = true;
        for (const auto& part : framing->parts) {
            if (!o.all_containers && part.container != o.container) continue;
            const auto p = part.channels.find("POSITION");
            Monstey_PreviewRequire(p != part.channels.end() && p->second.width == 3 &&
                !p->second.values.empty() && p->second.values.size() % 3 == 0, "Invalid framing position channel");
            for (std::size_t i = 0; i < p->second.values.size() / 3; ++i) {
                const auto v = Monstey_PreviewPosition(p->second, i);
                for (unsigned k = 0; k < 3; ++k) {
                    Monstey_PreviewRequire(std::isfinite(v[k]) && std::abs(v[k]) <= 1e38,
                        "Nonfinite or unbounded framing position");
                    if (referenceFirst) minimum[k] = maximum[k] = v[k];
                    else { minimum[k] = std::min(minimum[k], v[k]); maximum[k] = std::max(maximum[k], v[k]); }
                }
                referenceFirst = false;
            }
        }
        Monstey_PreviewRequire(!referenceFirst, "No geometry in the selected framing container");
    }
    Monstey_PreviewVector center{};
    double radiusSquared = 0;
    for (unsigned k = 0; k < 3; ++k)
    {
        center[k] = std::midpoint(minimum[k], maximum[k]);
        const auto half = (maximum[k] - minimum[k]) / 2;
        radiusSquared += half * half;
    }
    const double radius = std::sqrt(radiusSquared),
                 scale = 0.85 * std::min(o.width, o.height) * o.zoom / (radius > 0 ? 2 * radius : 1);
    const double yaw = std::remainder(o.yaw_degrees, 360.0) * std::numbers::pi / 180;
    const double pitch = std::remainder(o.pitch_degrees, 360.0) * std::numbers::pi / 180;
    const auto project = [&](const Monstey_PreviewVector &v) {
        const double x = v[0] - center[0], y = v[1] - center[1], z = v[2] - center[2];
        const double rx = std::cos(yaw) * x + std::sin(yaw) * z, rz = -std::sin(yaw) * x + std::cos(yaw) * z;
        const double ry = std::cos(pitch) * y - std::sin(pitch) * rz,
                     depth = std::sin(pitch) * y + std::cos(pitch) * rz;
        Monstey_PreviewPoint p{double(o.width) / 2 + rx * scale + o.pan_x * o.width,
                               double(o.height) / 2 - ry * scale + o.pan_y * o.height,
                               depth * scale,
                               {175, 183, 195}};
        Monstey_PreviewRequire(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z),
                               "Preview projection is not finite");
        return p;
    };
    Monstey_ModelPreviewFrame frame{
        o.width,
        o.height,
        std::vector<std::uint32_t>(static_cast<std::size_t>(o.width) * o.height, Monstey_PreviewBackground),
        {}};
    std::vector<double> depth(frame.pixels.size(), -std::numeric_limits<double>::infinity());
    std::size_t drawn = 0, degenerate = 0, boneEdges = 0;
    for (const auto *p : parts)
        if (o.mesh)
        {
            const auto &pos = p->channels.at("POSITION");
            std::vector<Monstey_PreviewPoint> points;
            points.reserve(pos.values.size() / 3);
            for (std::size_t i = 0; i < pos.values.size() / 3; ++i)
            {
                auto v = project(Monstey_PreviewPosition(pos, i));
                if (o.vertex_colors && p->channels.contains("COLOR0"))
                    for (unsigned k = 0; k < 3; ++k)
                        v.color[k] = p->channels.at("COLOR0").values[i * 4 + k];
                points.push_back(v);
            }
            for (std::size_t i = 0; i < p->indices.size(); i += 3)
            {
                auto a = points[p->indices[i]], b = points[p->indices[i + 1]], c = points[p->indices[i + 2]];
                double area = Monstey_PreviewEdge(a, b, c.x, c.y);
                if (std::abs(area) < 1e-12)
                {
                    ++degenerate;
                    continue;
                }
                if (area < 0)
                {
                    std::swap(b, c);
                    area = -area;
                }
                if (o.wireframe)
                {
                    for (const auto &pair :
                         std::array<std::pair<Monstey_PreviewPoint, Monstey_PreviewPoint>, 3>{{{a, b}, {b, c}, {c, a}}})
                        Monstey_PreviewLine(frame, depth, pair.first, pair.second, 0xffbbc9da, false);
                    ++drawn;
                    continue;
                }
                const double lowX = std::max(0.0, std::ceil(std::min({a.x, b.x, c.x}) - 0.5)),
                             highX = std::min(double(o.width - 1), std::floor(std::max({a.x, b.x, c.x}) - 0.5));
                const double lowY = std::max(0.0, std::ceil(std::min({a.y, b.y, c.y}) - 0.5)),
                             highY = std::min(double(o.height - 1), std::floor(std::max({a.y, b.y, c.y}) - 0.5));
                if (lowX > highX || lowY > highY)
                    continue;
                const Monstey_PreviewVector u{b.x - a.x, b.y - a.y, b.z - a.z}, v{c.x - a.x, c.y - a.y, c.z - a.z};
                const Monstey_PreviewVector n{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                                              u[0] * v[1] - u[1] * v[0]};
                const double norm = std::hypot(n[0], n[1], n[2]);
                const double light = norm > 0 ? 0.30 + 0.70 * std::abs(n[2]) / norm : 1;
                for (auto y = static_cast<unsigned>(lowY); y <= static_cast<unsigned>(highY); ++y)
                    for (auto x = static_cast<unsigned>(lowX); x <= static_cast<unsigned>(highX); ++x)
                    {
                        const double w0 = Monstey_PreviewEdge(b, c, x + 0.5, y + 0.5) / area,
                                     w1 = Monstey_PreviewEdge(c, a, x + 0.5, y + 0.5) / area, w2 = 1 - w0 - w1;
                        if (w0 < -1e-10 || w1 < -1e-10 || w2 < -1e-10)
                            continue;
                        const double z = w0 * a.z + w1 * b.z + w2 * c.z;
                        const auto at = static_cast<std::size_t>(y) * o.width + x;
                        if (z < depth[at])
                            continue;
                        std::array<double, 3> color{};
                        for (unsigned k = 0; k < 3; ++k)
                            color[k] = w0 * a.color[k] + w1 * b.color[k] + w2 * c.color[k];
                        frame.pixels[at] = Monstey_PreviewColor(color, light);
                        depth[at] = z;
                    }
                ++drawn;
            }
        }
    auto boneRows = Monstey_Json::array();
    if (o.bones)
    {
        std::vector<Monstey_PreviewPoint> joints;
        joints.reserve(scene.bones.size());
        for (std::size_t i = 0; i < scene.bones.size(); ++i)
        {
            const auto &b = scene.bones[i];
            Monstey_PreviewRequire(
                b.hk_index == i && b.parent_hk >= -1 &&
                    (b.parent_hk < 0 || static_cast<std::size_t>(b.parent_hk) < scene.bones.size()) &&
                    b.parent_hk != static_cast<int>(i),
                "Invalid preview bone identity/parent");
            const auto &m = b.global_bind_metres;
            for (const auto component : {m[3], m[7], m[11]})
                Monstey_PreviewRequire(std::isfinite(component) && std::abs(component) <= 1e38,
                                       "Nonfinite or unbounded preview bone translation");
            joints.push_back(project({m[3], m[7], m[11]}));
        }
        for (std::size_t i = 0; i < scene.bones.size(); ++i)
        {
            const auto &b = scene.bones[i];
            const auto &j = joints[i];
            if (b.parent_hk >= 0)
            {
                Monstey_PreviewLine(frame, depth, joints[static_cast<std::size_t>(b.parent_hk)], j, 0xff52d6f5,
                                    o.bones_in_front);
                ++boneEdges;
            }
            const double x = std::round(j.x), y = std::round(j.y);
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx)
                {
                    const double px = x + dx, py = y + dy;
                    if (dx * dx + dy * dy > 4 || px < 0 || py < 0 || px >= o.width || py >= o.height)
                        continue;
                    const auto at = static_cast<std::size_t>(py) * o.width + static_cast<std::size_t>(px);
                    if (o.bones_in_front || j.z >= depth[at])
                        frame.pixels[at] = 0xffc0f4ff;
                }
            boneRows.as_array().push_back(Monstey_Json::object(
                {{"hk_index", static_cast<std::uint64_t>(i)},
                 {"parent_hk", b.parent_hk},
                 {"name", b.name},
                 {"world_metres",
                  Monstey_Json::array({b.global_bind_metres[3], b.global_bind_metres[7], b.global_bind_metres[11]})},
                 {"screen_xy", Monstey_Json::array({j.x, j.y})}}));
        }
    }
    frame.report = Monstey_Json::object({{"schema", "Monstey.ModelPreview/1"},
                                         {"projection", "orthographic_native_y_up"},
                                         {"source_sha256", scene.source_sha256},
                                         {"container", static_cast<std::uint64_t>(o.container)},
                                         {"all_containers", o.all_containers},
                                         {"parts", static_cast<std::uint64_t>(parts.size())},
                                         {"vertices", static_cast<std::uint64_t>(vertices)},
                                         {"triangles", static_cast<std::uint64_t>(triangles)},
                                         {"rasterized_triangles", static_cast<std::uint64_t>(drawn)},
                                         {"degenerate_triangles", static_cast<std::uint64_t>(degenerate)},
                                         {"bone_edges", static_cast<std::uint64_t>(boneEdges)},
                                         {"bones", boneRows},
                                         {"native_materials", false},
                                         {"animation_playback", false},
                                         {"mesh_positions_cm", true},
                                         {"bone_positions_metres", true},
                                         {"host_bind_geometry_only", true}});
    return frame;
}
Monstey_ModelPreviewFrame Monstey_ModelPreviewRender(const Monstey_ModelScene& scene,
    const Monstey_ModelPreviewOptions& options) {
    return Monstey_PreviewRenderImpl(scene, options, nullptr);
}
Monstey_ModelPreviewFrame Monstey_ModelPreviewRenderFramed(const Monstey_ModelScene& scene,
    const Monstey_ModelScene& reference, const Monstey_ModelPreviewOptions& options) {
    auto frame = Monstey_PreviewRenderImpl(scene, options, &reference);
    frame.report["fixed_framing"] = true;
    frame.report["framing_source_sha256"] = reference.source_sha256;
    return frame;
}
Monstey_Bytes Monstey_ModelPreviewBitmap(const Monstey_ModelPreviewFrame &frame)
{
    Monstey_PreviewRequire(frame.width >= 32 && frame.height >= 32 && frame.width <= 4096 && frame.height <= 4096 &&
                               static_cast<std::uint64_t>(frame.width) * frame.height <= 8388608 &&
                               frame.pixels.size() == static_cast<std::size_t>(frame.width) * frame.height,
                           "Invalid preview bitmap dimensions");
    Monstey_Bytes b;
    b.reserve(54 + frame.pixels.size() * 4);
    Monstey_AppendU16(b, 0x4d42);
    Monstey_AppendU32(b, static_cast<std::uint32_t>(54 + frame.pixels.size() * 4));
    Monstey_AppendU32(b, 0);
    Monstey_AppendU32(b, 54);
    Monstey_AppendU32(b, 40);
    Monstey_AppendU32(b, frame.width);
    Monstey_AppendU32(b, std::bit_cast<std::uint32_t>(-static_cast<std::int32_t>(frame.height)));
    Monstey_AppendU16(b, 1);
    Monstey_AppendU16(b, 32);
    Monstey_AppendU32(b, 0);
    Monstey_AppendU32(b, static_cast<std::uint32_t>(frame.pixels.size() * 4));
    for (unsigned i = 0; i < 4; ++i)
        Monstey_AppendU32(b, 0);
    for (auto pixel : frame.pixels)
        Monstey_AppendU32(b, pixel);
    return b;
}
} // namespace Monstey