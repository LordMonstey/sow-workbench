#include "Monstey_World.hpp"
#include "Monstey_WorldGeometry.hpp"
#include "Monstey_WorldMerge.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <iostream>
using namespace Monstey;
namespace {
using B = Monstey_Bytes;
using J = Monstey_Json;
unsigned passed = 0;
void Check(bool value, const char *text) {
    if (!value)
        throw std::runtime_error(text);
}
template <class F> void Case(const char *name, F fn) {
    fn();
    ++passed;
    std::cout << "PASS " << name << '\n';
}
template <class F> void Reject(F fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::exception &) {
        rejected = true;
    }
    Check(rejected, "expected rejection");
}
void U(B &b, std::uint32_t x) {
    for (unsigned i = 0; i < 4; ++i)
        b.push_back(static_cast<std::uint8_t>(x >> (8 * i)));
}
void H(B &b, std::uint16_t x) {
    b.push_back(static_cast<std::uint8_t>(x));
    b.push_back(static_cast<std::uint8_t>(x >> 8));
}
void F(B &b, float f) { U(b, std::bit_cast<std::uint32_t>(f)); }
struct FixtureGroup {
    B vertex, index;
    std::vector<std::array<std::uint8_t, 4>> layout;
    std::array<std::uint32_t, 13> draw{};
};
FixtureGroup Group(unsigned seed = 0, std::uint32_t count = 3, bool full = true, bool extras = true,
                   unsigned semantic = 8, unsigned channel = 0) {
    FixtureGroup g;
    g.layout = {{0, 2, 0, 0}};
    if (full)
        g.layout.insert(g.layout.end(),
                        {{0, 2, 1, 0},
                         {0, 3, 2, 0},
                         {0, 2, 3, 0},
                         {1, 1, 4, 0},
                         {1, 4, 5, 0},
                         {1, 4, static_cast<std::uint8_t>(semantic), static_cast<std::uint8_t>(channel)},
                         {1, 4, 6, 0},
                         {1, 4, 7, 0}});
    auto position = [&](std::uint32_t v) {
        F(g.vertex, float(seed + v));
        F(g.vertex, v % 2 ? 0.f : -0.f);
        F(g.vertex, float(v % 3));
    };
    for (std::uint32_t v = 0; v < count; ++v) {
        position(v);
        if (full) {
            for (float f : {-0.f, 0.f, 1.f, 1.f, 0.f, 0.f, -1.f, 0.f, 1.f, 0.f})
                F(g.vertex, f);
        }
    }
    if (full)
        for (std::uint32_t v = 0; v < count; ++v) {
            F(g.vertex, float(v) / float(count));
            F(g.vertex, 0.25f);
            g.vertex.insert(g.vertex.end(), {std::uint8_t(seed), 0xab, 0xcd, 0xef, 0xde, 0xad, 0xbe, 0xef,
                                             255, 0, 0, 0, 0, 1, 2, 3});
        }
    auto expected = count * (full ? 76u : 12u);
    Check(g.vertex.size() == expected, "independent fixture stride mismatch");
    for (auto i : {0u, count - 1, 1u})
        H(g.index, static_cast<std::uint16_t>(i));
    if (extras) {
        g.vertex.insert(g.vertex.end(), {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70});
        g.index.push_back(0xd1);
    }
    g.draw = {0x11000000u | seed, 0x00440000u | count, 0xa5a5a5a5u, 0,           0,
              0x7e000001u,        0x9a9a0001u,         0xdeadbeefu, 0x01020304u, 0xface0000u,
              expected,           0x12345678u,         0xabcdef01u};
    return g;
}
B Mesh(const std::vector<FixtureGroup> &groups, B opaque = {}) {
    B b{'M', 'E', 'S', 'H'};
    std::uint32_t attrs = 0, allocation = 0;
    for (auto &g : groups) {
        attrs += static_cast<std::uint32_t>(g.layout.size());
        allocation += std::uint32_t((g.vertex.size() + 3) & ~std::size_t(3)) +
                      std::uint32_t((g.index.size() + 3) & ~std::size_t(3));
    }
    for (auto value : {5u, std::uint32_t(groups.size()), std::uint32_t(groups.size()),
                       std::uint32_t(groups.size()), attrs, allocation, std::uint32_t(opaque.size())})
        U(b, value);
    b.insert(b.end(), opaque.begin(), opaque.end());
    for (auto &g : groups) {
        U(b, 1);
        U(b, std::uint32_t(g.vertex.size()));
        U(b, std::uint32_t(g.index.size()));
        b.insert(b.end(), g.vertex.begin(), g.vertex.end());
        b.insert(b.end(), g.index.begin(), g.index.end());
        U(b, 1);
        U(b, std::uint32_t(g.layout.size()));
        for (auto &a : g.layout)
            b.insert(b.end(), a.begin(), a.end());
        U(b, 1);
        for (auto value : g.draw)
            U(b, value);
    }
    return b;
}
Monstey_WorldMergeSource Source(std::string id, B mesh, std::size_t context = 0) {
    Monstey_WorldMergeSource s;
    s.id = std::move(id);
    s.resource_path = "worlds/" + s.id + ".wldmsh";
    s.expected_sha256 = Monstey_HashSha256(mesh);
    s.mesh = std::move(mesh);
    s.context = context;
    s.material_paths = {"materials/unused.mat", "materials/" + s.id + ".mat"};
    return s;
}
void VerifyRegions(const std::vector<Monstey_WorldMergeSource> &sources,
                   const Monstey_WorldMergeResult &result) {
    auto out = Monstey_WorldMeshReadContexts(result.mesh);
    auto context = result.report.at("target_context").as_uint();
    auto &merged = out.groups[context];
    for (std::size_t i = 0; i < sources.size(); ++i) {
        auto in = Monstey_WorldMeshReadContexts(sources[i].mesh);
        auto &native = in.groups[sources[i].context];
        auto &region = result.report.at("encoded_regions")[i];
        auto v = region.at("vertex_byte_offset").as_uint(), ib = region.at("index_byte_offset").as_uint();
        Check(std::equal(native.vertex_buffer.begin(), native.vertex_buffer.end(),
                         merged.vertex_buffer.begin() + static_cast<std::ptrdiff_t>(v)),
              "raw vertex bytes were lost");
        Check(std::equal(native.index_buffer.begin(), native.index_buffer.end(),
                         merged.index_buffer.begin() + static_cast<std::ptrdiff_t>(ib)),
              "raw index bytes were lost");
    }
}
std::string P(const std::filesystem::path &p) { return Monstey_Utf8(p.wstring()); }
} // namespace
int main(int argc, char **argv) {
    try {
        auto base =
            argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1])) : std::filesystem::temp_directory_path();
        std::filesystem::create_directories(Monstey_WindowsFilePath(base));
        auto dir = base / ("Monstey_CppWorldMergeTests_" +
                           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(Monstey_WindowsFilePath(dir));
        Case("single-source native bytes retain opaque header, flags and unused buffers", [&] {
            auto source = Source("identity", Mesh({Group(3)}, B{0xab, 0xcd, 0xef}));
            auto result = Monstey_WorldMerge(std::span<const Monstey_WorldMergeSource>(&source, 1));
            Check(result.mesh == source.mesh && result.material_paths == source.material_paths &&
                      result.report.at("single_source_byte_exact").as_bool(),
                  "one-source no-op is not exact");
        });
        Case("multi-source composition retains every encoded channel and material slot", [&] {
            std::vector<Monstey_WorldMergeSource> sources{Source("first", Mesh({Group(1)})),
                                                          Source("second", Mesh({Group(2)}))};
            auto result = Monstey_WorldMerge(sources);
            VerifyRegions(sources, result);
            auto decoded = Monstey_WorldMeshReadContexts(result.mesh);
            Check(decoded.groups.size() == 1 && decoded.groups[0].draws.size() == 2 &&
                      decoded.groups[0].layouts.size() == 1 && result.material_paths.size() == 4,
                  "common context or complete materials missing");
            auto &a = decoded.groups[0].draws[0];
            auto &b = decoded.groups[0].draws[1];
            Check((a[6] & 65535) == 1 && (b[6] & 65535) == 3 && b[3] == 4 && b[4] == 4,
                  "native relocation fields differ");
            for (auto word : {0u, 1u, 2u, 5u, 7u, 8u, 11u, 12u})
                Check(b[word] == Group(2).draw[word], "unknown draw word changed");
            Check((b[6] & 0xffff0000) == 0x9a9a0000 && (b[9] & 0xffffff00) == 0xface0000,
                  "packed native flags changed");
        });
        Case("shared source buffers and unused trailing bytes are not discarded", [&] {
            auto group = Group(9);
            auto source = Source("buffer", Mesh({group}));
            std::vector<Monstey_WorldMergeSource> sources{source, source};
            sources[1].id = "buffer2";
            auto result = Monstey_WorldMerge(sources);
            VerifyRegions(sources, result);
            auto raw = Monstey_WorldMeshReadContexts(result.mesh);
            Check(raw.groups[0].vertex_buffer.size() > group.vertex.size() * 2 &&
                      raw.groups[0].index_buffer.size() == 15,
                  "source gaps or odd index tail lost");
        });
        Case("selected nonzero context and all other base contexts remain exact", [&] {
            std::vector<Monstey_WorldMergeSource> sources{
                Source("base", Mesh({Group(4), Group(5, 3, false)}), 1), Source("added", Mesh({Group(6)}))};
            auto result = Monstey_WorldMerge(sources);
            VerifyRegions(sources, result);
            auto original = Monstey_WorldMeshReadContexts(sources[0].mesh),
                 now = Monstey_WorldMeshReadContexts(result.mesh);
            Check(now.groups.size() == 2 && now.groups[0].vertex_buffer == original.groups[0].vertex_buffer &&
                      now.groups[0].draws == original.groups[0].draws &&
                      result.report.at("target_context").as_uint() == 1,
                  "unselected context dropped or selected binding changed");
        });
        Case("large common buffers retain independent local uint16 draw domains", [&] {
            std::vector<Monstey_WorldMergeSource> sources{Source("largeA", Mesh({Group(0, 40000, false)})),
                                                          Source("largeB", Mesh({Group(1, 40000, false)}))};
            auto result = Monstey_WorldMerge(sources);
            VerifyRegions(sources, result);
            auto a = Monstey_WorldMeshDecodeGeometry(result.mesh, 0, 0),
                 b = Monstey_WorldMeshDecodeGeometry(result.mesh, 0, 1);
            Check(a.at("POSITION").size() == 40000 && b.at("POSITION").size() == 40000 &&
                      a.at("INDICES")[0][1].as_uint() == 39999 && b.at("INDICES")[0][1].as_uint() == 39999,
                  "local uint16 partitions were converted to overflowing global indices");
        });
        Case("material slot65535 is exact and the next slot cannot wrap", [&] {
            auto first = Source("materialBase", Mesh({Group()})),
                 second = Source("materialAdded", Mesh({Group(1)}));
            first.material_paths.resize(65534, "materials/retained-alias.mat");
            std::vector<Monstey_WorldMergeSource> sources{first, second};
            auto result = Monstey_WorldMerge(sources);
            auto native = Monstey_WorldMeshReadContexts(result.mesh);
            Check(result.material_paths.size() == 65536 && (native.groups[0].draws[1][6] & 65535) == 65535 &&
                      (native.groups[0].draws[1][6] & 0xffff0000u) == 0x9a9a0000u,
                  "native material slot boundary or packed flags changed");
            auto third = Source("materialOverflow", Mesh({Group(2)}));
            sources.push_back(third);
            Reject([&] { Monstey_WorldMerge(sources); });
        });
        Case("opaque headers are not silently relocated in multi-source output", [&] {
            std::vector<Monstey_WorldMergeSource> sources{Source("opaque", Mesh({Group()}, B{1, 2, 3})),
                                                          Source("other", Mesh({Group()}))};
            Reject([&] { Monstey_WorldMerge(sources); });
        });
        Case("source checksum mismatch and malformed input fail without mutation", [&] {
            auto source = Source("bad", Mesh({Group()}));
            auto before = source.mesh;
            source.mesh.back() ^= 1;
            Reject([&] { Monstey_WorldMerge(std::span<const Monstey_WorldMergeSource>(&source, 1)); });
            source.mesh = before;
            source.mesh.resize(40);
            source.expected_sha256 = Monstey_HashSha256(source.mesh);
            Reject([&] { Monstey_WorldMerge(std::span<const Monstey_WorldMergeSource>(&source, 1)); });
            Check(before == Mesh({Group()}), "source changed");
        });
        Case("missing material slots, duplicate source IDs and invalid contexts reject", [&] {
            auto source = Source("invalid", Mesh({Group()}));
            source.material_paths.resize(1);
            Reject([&] { Monstey_WorldMerge(std::span<const Monstey_WorldMergeSource>(&source, 1)); });
            source = Source("same", Mesh({Group()}));
            std::vector<Monstey_WorldMergeSource> sources{source, source};
            Reject([&] { Monstey_WorldMerge(sources); });
            source.context = 1;
            Reject([&] { Monstey_WorldMerge(std::span<const Monstey_WorldMergeSource>(&source, 1)); });
        });
        Case("active layout-byte capacity cannot overflow silently", [&] {
            std::vector<Monstey_WorldMergeSource> sources;
            for (unsigned i = 0; i < 257; ++i)
                sources.push_back(Source("layout" + std::to_string(i),
                                         Mesh({Group(i, 3, true, true, 8 + i % 248, i / 248)})));
            Reject([&] { Monstey_WorldMerge(sources); });
        });
        if (argc > 3)
            Case("actual native terrain common context retains channels and all material associations", [&] {
                auto mesh = std::filesystem::path(Monstey_Wide(argv[2])),
                     render = std::filesystem::path(Monstey_Wide(argv[3]));
                auto before = Monstey_HashSha256(mesh), before_render = Monstey_HashSha256(render);
                auto graph =
                    Monstey_WorldCall("sow_world_render_inspect", J::object({{"source", P(render)}}));
                auto source = Source("nativeA", Monstey_ReadFile(mesh));
                source.context =
                    static_cast<std::size_t>(graph.at("models")[0].at("geometry_group").as_uint());
                source.material_paths.clear();
                for (auto &path : graph.at("models")[0].at("materials").as_array())
                    source.material_paths.push_back(path.as_string());
                auto second = source;
                second.id = "nativeB";
                std::vector<Monstey_WorldMergeSource> sources{source, second};
                auto result = Monstey_WorldMerge(sources);
                VerifyRegions(sources, result);
                auto parsed = Monstey_WorldMeshReadContexts(result.mesh);
                Check(parsed.groups[source.context].draws.size() == 50 &&
                          result.material_paths.size() == source.material_paths.size() * 2 &&
                          Monstey_HashSha256(mesh) == before && Monstey_HashSha256(render) == before_render,
                      "native merge lost original data or changed source");
                Monstey_WriteNewFile(dir / "native-common-context.wldmsh", result.mesh);
                Monstey_WriteNewText(dir / "native-merge-proof.json", result.report.dump(2) + "\n");
            });
        if (argc > 5)
            Case("two distinct original root contexts keep every native material binding", [&] {
                std::array<std::filesystem::path, 4> paths{std::filesystem::path(Monstey_Wide(argv[2])),
                                                           std::filesystem::path(Monstey_Wide(argv[3])),
                                                           std::filesystem::path(Monstey_Wide(argv[4])),
                                                           std::filesystem::path(Monstey_Wide(argv[5]))};
                std::array<std::string, 4> hashes;
                for (std::size_t i = 0; i < paths.size(); ++i)
                    hashes[i] = Monstey_HashSha256(paths[i]);
                std::vector<Monstey_WorldMergeSource> sources;
                for (std::size_t i = 0; i < 2; ++i) {
                    auto graph = Monstey_WorldCall("sow_world_render_inspect",
                                                   J::object({{"source", P(paths[2 * i + 1])}}));
                    auto source = Source("native" + std::to_string(i), Monstey_ReadFile(paths[2 * i]));
                    source.context =
                        static_cast<std::size_t>(graph.at("models")[0].at("geometry_group").as_uint());
                    source.resource_path = graph.at("models")[0].at("path").as_string();
                    source.material_paths.clear();
                    for (auto &path : graph.at("models")[0].at("materials").as_array())
                        source.material_paths.push_back(path.as_string());
                    sources.push_back(std::move(source));
                }
                auto result = Monstey_WorldMerge(sources);
                VerifyRegions(sources, result);
                auto native = Monstey_WorldMeshReadContexts(result.mesh);
                Check(native.groups[sources[0].context].draws.size() == 43 &&
                          result.material_paths.size() ==
                              sources[0].material_paths.size() + sources[1].material_paths.size(),
                      "distinct source draw/material association was lost");
                for (std::size_t i = 0; i < paths.size(); ++i)
                    Check(hashes[i] == Monstey_HashSha256(paths[i]), "native source changed");
                Monstey_WriteNewFile(dir / "distinct-native-contexts.wldmsh", result.mesh);
                Monstey_WriteNewText(dir / "distinct-native-contexts-proof.json",
                                     result.report.dump(2) + "\n");
            });
        std::cout << "RESULT " << passed << " merge groups passed; outputs " << dir.string() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}