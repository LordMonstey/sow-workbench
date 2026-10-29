#include "Monstey_InstGeometry.hpp"
#include "Monstey_World.hpp"
#include "Monstey_WorldScene.hpp"
#include "Monstey_WorldGeometry.hpp"
#include "Monstey_WorldMerge.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
using namespace Monstey;
namespace {
using J = Monstey_Json;
using B = Monstey_Bytes;
unsigned passed{};
void Check(bool b, const char *text) {
    if (!b)
        throw std::runtime_error(text);
}
template <class F> void Case(const char *name, F &&f) {
    f();
    ++passed;
    std::cout << "PASS " << name << '\n';
}
template <class F> void Reject(F &&f) {
    bool no = false;
    try {
        f();
    } catch (const std::exception &) {
        no = true;
    }
    Check(no, "expected rejection");
}
void A32(B &b, std::uint32_t n) {
    for (unsigned j = 0; j < 4; ++j)
        b.push_back(static_cast<std::uint8_t>(n >> (j * 8)));
}
void A16(B &b, std::uint16_t n) {
    b.push_back(static_cast<std::uint8_t>(n));
    b.push_back(static_cast<std::uint8_t>(n >> 8));
}
void Float(B &b, float f) { A32(b, std::bit_cast<std::uint32_t>(f)); }
std::uint32_t String(B &b, const std::string &s) {
    auto n = static_cast<std::uint32_t>(b.size());
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
    return n;
}
B ObjectFixture() {
    B pool;
    auto schema = String(pool, "ObjectDB"), empty = String(pool, ""), record = String(pool, "ObjectRec");
    while (pool.size() % 4)
        pool.push_back(0);
    B db{'G', 'A', 'D', 'B'};
    for (auto n : {17u, static_cast<std::uint32_t>(pool.size()), 1u, 1u, 0u, 1u, 1u, 0u, 0u, 44u})
        A32(db, n);
    A16(db, 0);
    db.insert(db.end(), pool.begin(), pool.end());
    auto hash = Monstey_NativeHash("Enabled");
    A32(db, hash);
    A32(db, hash);
    db.insert(db.end(), {0, 0, 1});
    A16(db, 1);
    A16(db, 0);
    A32(db, 0);
    A32(db, 1);
    for (auto n : {schema, empty, empty, 0u, 1u, 0u})
        A32(db, n);
    A32(db, record);
    A32(db, 0x00010001);
    A32(db, 0x08000000);
    A32(db, 1);
    A32(db, 0);
    B classes, names;
    String(classes, "Light");
    String(names, "NativeLight");
    B obj{'O', 'B', 'J', 'L'};
    for (auto n : {5u, 1u, 1u, static_cast<std::uint32_t>(classes.size()),
                   static_cast<std::uint32_t>(names.size()), 0u})
        A32(obj, n);
    obj.insert(obj.end(), classes.begin(), classes.end());
    obj.insert(obj.end(), names.begin(), names.end());
    A32(obj, 0x1234);
    A32(obj, 1);
    A32(obj, 0);
    A32(obj, 0);
    A16(obj, 65535);
    A32(obj, 0);
    for (auto f : {10.f, 20.f, 30.f, 0.f, 0.f, 0.f, 1.f})
        Float(obj, f);
    A32(obj, 0xdeadbeef);
    A32(obj, 0xabcdef01);
    B out{'O', 'B', 'J', 'R'};
    A32(out, 13);
    A32(out, 0x17);
    A32(out, 3);
    for (auto pair : {std::pair{1u, db}, std::pair{2u, obj}, std::pair{77u, B{1, 2, 3, 4, 5}}}) {
        A32(out, pair.first);
        A32(out, static_cast<std::uint32_t>(pair.second.size()));
        out.insert(out.end(), pair.second.begin(), pair.second.end());
    }
    return out;
}
B GraphObjectFixture() {
    B pool;
    auto schema = String(pool, "ObjectDB"), empty = String(pool, ""), record = String(pool, "ObjectRec"),
         label = String(pool, "Donor graph label");
    while (pool.size() % 4)
        pool.push_back(0);
    B db{'G', 'A', 'D', 'B'};
    for (auto n : {17u, static_cast<std::uint32_t>(pool.size()), 1u, 3u, 0u, 5u, 4u, 0u, 0u, 44u})
        A32(db, n);
    A16(db, 0);
    db.insert(db.end(), pool.begin(), pool.end());
    struct Field {
        const char *name;
        std::uint8_t type;
        std::uint16_t count, offset;
    };
    for (const auto &fields :
         {std::vector<Field>{{"Next", 10, 1, 0}, {"Label", 4, 1, 1}}, std::vector<Field>{{"Edges", 11, 2, 0}},
          std::vector<Field>{{"Next", 10, 1, 0}}}) {
        std::vector<std::pair<std::uint32_t, std::size_t>> order;
        for (std::size_t i = 0; i < fields.size(); ++i)
            order.emplace_back(Monstey_NativeHash(fields[i].name), i);
        std::sort(order.begin(), order.end());
        for (std::size_t i = 0; i < fields.size(); ++i) {
            A32(db, Monstey_NativeHash(fields[i].name));
            A32(db, order[i].first);
            db.insert(db.end(), {static_cast<std::uint8_t>(i), static_cast<std::uint8_t>(order[i].second),
                                 fields[i].type});
            A16(db, fields[i].count);
            A16(db, fields[i].offset);
        }
    }
    for (auto n : {0u, 1u, schema, empty, empty, 0u, 3u, 0u})
        A32(db, n);
    A32(db, record);
    A32(db, 0x00020002);
    A32(db, 0x08000000);
    A32(db, 1);
    A32(db, label);
    A32(db, record);
    A32(db, 0x02010002);
    A32(db, 0x08000000);
    A32(db, 2);
    A32(db, 0);
    A32(db, record);
    A32(db, 0x03010001);
    A32(db, 0x08000000);
    A32(db, 0);
    A32(db, 0);
    return Monstey_ObjresReplaceChunk(ObjectFixture(), 1, db);
}
B InstFixture() {
    B names;
    String(names, "prefabs/test.inst");
    while (names.size() % 8)
        names.push_back(0);
    B out;
    for (auto n : {0x42465250u, 8u, Monstey_PathHash("worlds/test.inst"), 1u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u,
                   1u, static_cast<std::uint32_t>(names.size())})
        A32(out, n);
    B definition(44);
    Monstey_PutU32(definition, 0, 0x12345678);
    Monstey_PutU32(definition, 36, 1);
    out.insert(out.end(), definition.begin(), definition.end());
    for (unsigned j = 0; j < 5; ++j)
        A32(out, 0);
    A32(out, 0);
    A32(out, 0x90);
    A32(out, 0xfeed1234);
    for (auto f : {100.f, -1.f, -2.f, -3.f, 1.f, 2.f, 3.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f})
        Float(out, f);
    A32(out, 0xaabbccdd);
    A32(out, 0x11223344);
    out.insert(out.end(), names.begin(), names.end());
    return out;
}
B MeshFixture(unsigned aliases = 1) {
    B out{'M', 'E', 'S', 'H'};
    for (auto n : {5u, 1u, aliases, 1u, 2u, 80u, 3u})
        A32(out, n);
    out.insert(out.end(), {0xab, 0xcd, 0xef});
    A32(out, aliases);
    A32(out, 72);
    A32(out, 6);
    for (auto p : {std::array{0.f, 0.f, 0.f}, std::array{1.f, 0.f, 0.f}, std::array{0.f, 1.f, 0.f}}) {
        for (auto f : p)
            Float(out, f);
        for (auto f : {-0.f, 0.f, 1.f})
            Float(out, f);
    }
    for (auto index : {0u, 1u, 2u})
        A16(out, static_cast<std::uint16_t>(index));
    A32(out, 1);
    A32(out, 2);
    out.insert(out.end(), {0, 2, 0, 0, 0, 2, 1, 0});
    A32(out, aliases);
    for (unsigned draw = 0; draw < aliases; ++draw) {
        std::array<std::uint32_t, 13> words{};
        words[0] = 0x10 + draw;
        words[1] = 3;
        words[5] = 1;
        words[6] = 7;
        words[10] = 72;
        for (auto n : words)
            A32(out, n);
    }
    return out;
}
B IndependentNpz(const std::string &dtype, bool zip64 = false) {
    std::string header = "{'descr': '" + dtype + "', 'fortran_order': False, 'shape': (3, 3), }";
    while ((10 + header.size() + 1) % 16)
        header += ' ';
    header += '\n';
    B npy{0x93, 'N', 'U', 'M', 'P', 'Y', 1, 0};
    A16(npy, static_cast<std::uint16_t>(header.size()));
    npy.insert(npy.end(), header.begin(), header.end());
    for (float f : {9.f, 8.f, 7.f, 6.f, 5.f, 4.f, 3.f, 2.f, 1.f}) {
        if (dtype == "<f8") {
            auto bits = std::bit_cast<std::uint64_t>(static_cast<double>(f));
            for (unsigned j = 0; j < 8; ++j)
                npy.push_back(static_cast<std::uint8_t>(bits >> (8 * j)));
        } else
            Float(npy, f);
    }
    std::uint32_t crc = ~0u;
    for (auto byte : npy) {
        crc ^= byte;
        for (unsigned j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    }
    crc = ~crc;
    // Independently emit a raw RFC1951 stored block in ZIP method 8.
    B deflate{1};
    A16(deflate, static_cast<std::uint16_t>(npy.size()));
    A16(deflate, static_cast<std::uint16_t>(~npy.size()));
    deflate.insert(deflate.end(), npy.begin(), npy.end());
    std::string name = "POSITION.npy";
    B zip;
    A32(zip, 0x04034b50);
    for (auto n : {20u, 0u, 8u, 0u, 0u})
        A16(zip, static_cast<std::uint16_t>(n));
    A32(zip, crc);
    A32(zip, zip64 ? 0xffffffffu : static_cast<std::uint32_t>(deflate.size()));
    A32(zip, zip64 ? 0xffffffffu : static_cast<std::uint32_t>(npy.size()));
    A16(zip, static_cast<std::uint16_t>(name.size()));
    A16(zip, zip64 ? 20 : 0);
    zip.insert(zip.end(), name.begin(), name.end());
    if (zip64) {
        A16(zip, 1);
        A16(zip, 16);
        for (auto n : {std::uint64_t(npy.size()), std::uint64_t(deflate.size())})
            for (unsigned j = 0; j < 8; ++j)
                zip.push_back(static_cast<std::uint8_t>(n >> (8 * j)));
    }
    zip.insert(zip.end(), deflate.begin(), deflate.end());
    auto central = zip.size();
    A32(zip, 0x02014b50);
    for (auto n : {20u, 20u, 0u, 8u, 0u, 0u})
        A16(zip, static_cast<std::uint16_t>(n));
    A32(zip, crc);
    A32(zip, static_cast<std::uint32_t>(deflate.size()));
    A32(zip, static_cast<std::uint32_t>(npy.size()));
    A16(zip, static_cast<std::uint16_t>(name.size()));
    for (unsigned j = 0; j < 4; ++j)
        A16(zip, 0);
    A32(zip, 0);
    A32(zip, 0);
    zip.insert(zip.end(), name.begin(), name.end());
    auto central_bytes = zip.size() - central;
    A32(zip, 0x06054b50);
    A16(zip, 0);
    A16(zip, 0);
    A16(zip, 1);
    A16(zip, 1);
    A32(zip, static_cast<std::uint32_t>(central_bytes));
    A32(zip, static_cast<std::uint32_t>(central));
    A16(zip, 0);
    return zip;
}
B DefinedPrefabFixture() {
    auto mesh = MeshFixture();
    B geometry(mesh.begin() + 35, mesh.end());
    Monstey_PutU32(geometry, geometry.size() - 52 + 24, 0);
    B material;
    String(material, "test.mat");
    while (material.size() % 4)
        material.push_back(0);
    B out;
    for (auto n : {0x42465250u, 8u, Monstey_PathHash("custom/target.inst"), 1u, 1u, 1u, 0u, 0u,
                   static_cast<std::uint32_t>(material.size()), 1u, 2u, 80u, 0u, 0u})
        A32(out, n);
    B definition(44);
    Monstey_PutU32(definition, 0, 0x76543210);
    Monstey_PutU32(definition, 12, 1);
    Monstey_PutU32(definition, 16, 0xffffffff);
    out.insert(out.end(), definition.begin(), definition.end());
    out.insert(out.end(), geometry.begin(), geometry.end());
    out.insert(out.end(), material.begin(), material.end());
    return out;
}
B NativeSceneFixture() {
    B out;
    for (auto word : {2u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u})
        A32(out, word);
    for (unsigned i = 0; i < 2; ++i) {
        A32(out, i ? 0x102u : 0x101u);
        A32(out, 0);
        A32(out, 1);
        for (auto value : {-10.f, -20.f, -30.f, 10.f, 20.f, 30.f})
            Float(out, value);
        for (auto word : {0x11223344u, 0x55667788u, 0u, i ? 1u : 0u,
                          i ? UINT32_MAX : 0u, 0u})
            A32(out, word);
        if (i)
            A32(out, 0);
    }
    for (auto word : {1u, 3u, 0u, UINT32_MAX, UINT32_MAX, 0u})
        A32(out, word);
    Check(out.size() == 200, "Independent scene fixture extent differs");
    return out;
}
B RenderFixture() {
    B strings;
    auto source = String(strings, "worlds/source.wldmsh");
    std::vector<std::uint32_t> mats;
    for (unsigned j = 0; j < 8; ++j)
        mats.push_back(String(strings, "materials/m" + std::to_string(j) + ".mat"));
    B out{'W', 'D', 'S', 'C'};
    for (auto n : {9u, 1u, 1u, 0u, 1u, 1u, 8u, 0u, 0u, static_cast<std::uint32_t>(strings.size())})
        A32(out, n);
    out.insert(out.end(), strings.begin(), strings.end());
    A32(out, 8);
    A32(out, source);
    A32(out, 0);
    for (auto offset : mats)
        A32(out, offset);
    A32(out, 1);
    A32(out, 0);
    A32(out, 0);
    for (auto f : {0.f, 0.f, 0.f, 1.f, 1.f, 0.f})
        Float(out, f);
    A32(out, 0xdeadbeef);
    A16(out, 0);
    A16(out, 0x1234);
    const auto scene = NativeSceneFixture();
    out.insert(out.end(), scene.begin(), scene.end());
    return out;
}
B RenderManyFixture(unsigned count, bool lod = false) {
    auto original = RenderFixture();
    auto group_at = 44 + Monstey_U32(original, 40) + 12 + 8 * 4;
    B out(original.begin(), original.begin() + group_at);
    Monstey_PutU32(out, 12, count);
    Monstey_PutU32(out, 32, lod ? 1 : 0);
    Monstey_PutU32(out, 36, lod ? 1 : 0);
    A32(out, count);
    A32(out, lod ? 1 : 0);
    A32(out, 0);
    B prototype(original.begin() + group_at + 12, original.begin() + group_at + 44);
    for (unsigned i = 0; i < count; ++i) {
        auto record = prototype;
        Monstey_PutU16(record, 28, static_cast<std::uint16_t>(i));
        out.insert(out.end(), record.begin(), record.end());
    }
    if (lod) {
        out.push_back(1);
        out.resize(out.size() + 12);
    }
    const auto scene = NativeSceneFixture();
    out.insert(out.end(), scene.begin(), scene.end());
    return out;
}
B BindingFixture() {
    B names;
    String(names, "worlds/old.inst");
    while (names.size() % 4)
        names.push_back(0);
    B out{'W', 'D', 'S', 'C'};
    for (auto n : {9u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 0u, static_cast<std::uint32_t>(names.size())})
        A32(out, n);
    out.insert(out.end(), names.begin(), names.end());
    for (auto f : {-1.f, -1.f, -1.f, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f})
        Float(out, f);
    for (auto n : {0x1111u, 0x2222u, 1u, 0x3333u, 0u, 0u, 0u, 1u, 0u, 1u, 0u})
        A32(out, n);
    out.resize(out.size() + 52);
    return out;
}
B LargeTopologyNpz() {
    constexpr std::size_t n = 65538;
    std::map<std::string, B> files;
    auto npy = [](const std::string &dtype, std::size_t rows, std::size_t width, const B &data) {
        std::string header = "{'descr': '" + dtype + "', 'fortran_order': False, 'shape': (" +
                             std::to_string(rows) + ", " + std::to_string(width) + "), }";
        while ((10 + header.size() + 1) % 16)
            header += ' ';
        header += '\n';
        B out{0x93, 'N', 'U', 'M', 'P', 'Y', 1, 0};
        A16(out, static_cast<std::uint16_t>(header.size()));
        out.insert(out.end(), header.begin(), header.end());
        out.insert(out.end(), data.begin(), data.end());
        return out;
    };
    B position, normal, indices;
    for (std::size_t j = 0; j < n; ++j) {
        Float(position, static_cast<float>(j % 1000));
        Float(position, static_cast<float>(j / 1000));
        Float(position, 0);
        Float(normal, 0);
        Float(normal, 0);
        Float(normal, 1);
        A32(indices, static_cast<std::uint32_t>(j));
    }
    files["POSITION.npy"] = npy("<f4", n, 3, position);
    files["NORMAL.npy"] = npy("<f4", n, 3, normal);
    files["INDICES.npy"] = npy("<u4", n / 3, 3, indices);
    B out, central;
    for (auto &[name, data] : files) {
        std::uint32_t crc = ~0u;
        for (auto byte : data) {
            crc ^= byte;
            for (unsigned bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
        }
        crc = ~crc;
        auto offset = static_cast<std::uint32_t>(out.size());
        A32(out, 0x04034b50);
        for (auto v : {20u, 0u, 0u, 0u, 0u})
            A16(out, static_cast<std::uint16_t>(v));
        A32(out, crc);
        A32(out, static_cast<std::uint32_t>(data.size()));
        A32(out, static_cast<std::uint32_t>(data.size()));
        A16(out, static_cast<std::uint16_t>(name.size()));
        A16(out, 0);
        out.insert(out.end(), name.begin(), name.end());
        out.insert(out.end(), data.begin(), data.end());
        A32(central, 0x02014b50);
        for (auto v : {20u, 20u, 0u, 0u, 0u, 0u})
            A16(central, static_cast<std::uint16_t>(v));
        A32(central, crc);
        A32(central, static_cast<std::uint32_t>(data.size()));
        A32(central, static_cast<std::uint32_t>(data.size()));
        A16(central, static_cast<std::uint16_t>(name.size()));
        for (unsigned j = 0; j < 4; ++j)
            A16(central, 0);
        A32(central, 0);
        A32(central, offset);
        central.insert(central.end(), name.begin(), name.end());
    }
    auto offset = static_cast<std::uint32_t>(out.size());
    out.insert(out.end(), central.begin(), central.end());
    A32(out, 0x06054b50);
    A16(out, 0);
    A16(out, 0);
    A16(out, 3);
    A16(out, 3);
    A32(out, static_cast<std::uint32_t>(central.size()));
    A32(out, offset);
    A16(out, 0);
    return out;
}
std::string Path(const std::filesystem::path &p) { return Monstey_Utf8(p.wstring()); }
} // namespace
int main(int argc, char **argv) {
    try {
        auto base =
            argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1])) : std::filesystem::temp_directory_path();
        std::filesystem::create_directories(base);
        auto dir = base / ("Monstey_CppWorldTests_" +
                           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directory(dir);
        auto put = [&](const char *name, const B &bytes) {
            auto p = dir / name;
            Monstey_WriteNewFile(p, bytes);
            return p;
        };
        auto objraw = ObjectFixture(), instraw = InstFixture(), meshraw = MeshFixture();
        auto obj = put("native.objres", objraw), inst = put("native.inst", instraw),
             mesh = put("native.wldmsh", meshraw);
        auto args = [&](const std::filesystem::path &p) { return J::object({{"source", Path(p)}}); };
        Case("41-operation catalogue reports exact native qualification counts", [&] {
            auto catalogue = Monstey_WorldCatalog();
            Check(catalogue.size() == 41, "catalogue lost original operations");
            auto capabilities = Monstey_WorldCall("sow_capabilities", J::object());
            std::uint64_t complete = 0, partial = 0, pending = 0;
            for (auto &row : catalogue.as_array()) {
                auto status = row.at("status").as_string();
                if (status == "complete")
                    ++complete;
                else if (status == "partial")
                    ++partial;
                else {
                    Check(status == "pending", "unknown world qualification state");
                    ++pending;
                }
                Check(row.at("implemented").as_bool() == (status == "complete") &&
                          row.at("available").as_bool() == (status != "pending"),
                      "world availability differs from qualification");
            }
            Check(capabilities.at("complete_operations").as_uint() == complete &&
                      capabilities.at("partial_operations").as_uint() == partial &&
                      capabilities.at("pending_operations").as_uint() == pending &&
                      complete + partial + pending == 41 && !capabilities.at("python_runtime").as_bool(),
                  "capability counts or native runtime contract differ");
            Reject([&] { Monstey_WorldCall("sow_world_collision_triangles_build", J::object()); });
        });
        Case("native human-form schemas expose actual required arguments", [&] {
            auto rows = Monstey_WorldCatalog();
            bool checked = false;
            for (auto &tool : rows.as_array())
                if (tool.at("available").as_bool()) {
                    auto &schema = tool.at("inputSchema");
                    Check(!schema.at("additionalProperties").as_bool(),
                          "qualified schema accepts unknown arguments");
                    if (tool.at("name").as_string() == "sow_objres_add") {
                        Check(schema.at("properties").contains("objects"),
                              "object authoring form missing object array");
                        auto &required = schema.at("required").as_array();
                        Check(std::any_of(required.begin(), required.end(),
                                          [](auto &v) { return v.as_string() == "objects"; }),
                              "objects are not marked required");
                        checked = true;
                    }
                    if (tool.at("name").as_string() == "sow_objres_apply")
                        Check(schema.at("properties").contains("updates"), "transform form missing updates");
                    if (tool.at("name").as_string() == "sow_section_inspect")
                        Check(schema.at("properties").at("format").at("enum").size() == 4,
                              "section form missing native format choices");
                }
            Check(checked, "qualified object form absent");
        });
        Case("independent OBJR13 OBJL5 ObjectDB17 fixture", [&] {
            auto report = Monstey_WorldCall("sow_objres_inspect", args(obj));
            Check(report.at("objects").as_uint() == 1 && report.at("roundtrip_byte_exact").as_bool(),
                  "OBJRES parse/identity failed");
            auto q = Monstey_WorldCall("sow_objres_query", args(obj));
            Check(q.at("items")[0].at("name").as_string() == "NativeLight" &&
                      q.at("items")[0].at("properties")[0].at("value").as_bool(),
                  "OBJRES properties decode failed");
        });
        Case("shared OBJRES native owner helpers and transactional rename", [&] {
            auto owners = Monstey_ObjresOwners(objraw);
            Check(owners.size() == 1 && owners[0].at("name").as_string() == "NativeLight" &&
                      owners[0].at("property_values").contains(Monstey_Hex(Monstey_NativeHash("Enabled"))),
                  "shared owner data omitted native properties");
            auto raw_chunk = Monstey_ObjresReadChunk(objraw, 77);
            Check(raw_chunk == B({1, 2, 3, 4, 5}), "shared opaque chunk reader differs");
            auto replaced = Monstey_ObjresReplaceChunk(objraw, 77, B{9, 8, 7});
            Check(Monstey_ObjresReadChunk(replaced, 77) == B({9, 8, 7}) &&
                      Monstey_ObjresReadChunk(replaced, 1) == Monstey_ObjresReadChunk(objraw, 1) &&
                      Monstey_ObjresReadChunk(replaced, 2) == Monstey_ObjresReadChunk(objraw, 2),
                  "shared chunk replacement changed unrelated native bytes");
            auto spec = J::object(
                {{"source_sha256", Monstey_HashSha256(objraw)},
                 {"updates",
                  J::array({J::object({{"name", "NativeLight"}, {"rename_to", "RenamedLight"}})})}});
            auto result = Monstey_ObjresApply(objraw, spec);
            auto after = Monstey_ObjresOwners(result.first);
            Check(after[0].at("name").as_string() == "RenamedLight" &&
                      after[0].at("property_index") == owners[0].at("property_index") &&
                      after[0].at("group_index") == owners[0].at("group_index") &&
                      after[0].at("object_index") == owners[0].at("object_index") &&
                      after[0].at("position") == owners[0].at("position") &&
                      after[0].at("prefix_offset") == owners[0].at("prefix_offset") &&
                      Monstey_ObjresReadChunk(result.first, 1) == Monstey_ObjresReadChunk(objraw, 1),
                  "rename changed owner ordinal, pose, prefix or native property record");
            auto unchanged = spec;
            unchanged["updates"][0]["rename_to"] = "NativeLight";
            Check(Monstey_ObjresApply(objraw, unchanged).first == objraw, "identity rename changed bytes");
            spec["source_sha256"] = "wrong";
            Reject([&] { Monstey_ObjresApply(objraw, spec); });
            B linked;
            A32(linked, Monstey_NativeHash("NativeLight"));
            auto blocked = Monstey_ObjresReplaceChunk(objraw, 77, linked);
            spec.as_object().erase("source_sha256");
            Reject([&] { Monstey_ObjresApply(blocked, spec); });
            Check(Monstey_ObjresOwners(blocked)[0].at("name").as_string() == "NativeLight",
                  "failed linked rename changed its source");
        });
        Case("cross-file owner import remaps cyclic typed reference closure", [&] {
            auto target = objraw;
            for (unsigned i = 0; i < 3; ++i)
                target =
                    Monstey_ObjresApply(
                        target, J::object({{"updates",
                                            J::array({J::object(
                                                {{"name", "NativeLight"},
                                                 {"properties", J::object({{"Enabled", i % 2 == 0}})}})})}}))
                        .first;
            auto donor = GraphObjectFixture();
            auto result = Monstey_ObjresImportOwner(target, donor, "NativeLight", "ImportedOwner",
                                                    J::array({5, 6, 7}), J::array({0, 0, 0, 1}));
            auto owners = Monstey_ObjresOwners(result.first);
            Check(owners.size() == 2 && owners[0].at("name").as_string() == "NativeLight" &&
                      owners[1].at("property_index").as_uint() == 4 &&
                      owners[1].at("position")[0].as_number() == 5 &&
                      result.second.at("property_closure").at("records").size() == 3,
                  "import root, owner pose or reachable closure differs");
            auto db = Monstey_ObjresReadChunk(result.first, 1);
            auto original = Monstey_ObjresReadChunk(target, 1);
            auto old_pool = Monstey_U32(original, 8), new_pool = Monstey_U32(db, 8);
            Check(std::equal(original.begin() + 46, original.begin() + 46 + old_pool, db.begin() + 46),
                  "closure changed target atom prefix");
            auto at = 46ull + new_pool + 15ull * Monstey_U32(db, 28) + 8 + 24;
            std::vector<std::vector<std::uint32_t>> records;
            for (std::size_t i = 0; i < Monstey_U32(db, 16); ++i) {
                auto n = Monstey_U32(db, at + 4) & 65535;
                std::vector<std::uint32_t> words;
                for (std::size_t j = 0; j < n; ++j)
                    words.push_back(Monstey_U32(db, at + 12 + 4 * j));
                records.push_back(words);
                at += 12 + 4 * n;
            }
            Check(records.size() == 7 && records[4][0] == 5 && records[5][0] == 6 && records[5][1] == 4 &&
                      records[6][0] == 4,
                  "donor ordinals were copied blindly or cyclic/forward edges were lost");
            auto label = owners[1].at("property_values").at(Monstey_Hex(Monstey_NativeHash("Label")));
            Check(label.as_string() == "Donor graph label", "donor atom pointer was not rebased");
            auto bad_db = Monstey_ObjresReadChunk(donor, 1);
            auto first_descriptor = 46 + Monstey_U32(bad_db, 8);
            bad_db[first_descriptor + 10] = 55;
            auto bad_donor = Monstey_ObjresReplaceChunk(donor, 1, bad_db);
            Reject([&] {
                Monstey_ObjresImportOwner(target, bad_donor, "NativeLight", "RejectedOwner",
                                          J::array({0, 0, 0}), J::array({0, 0, 0, 1}));
            });
            Check(Monstey_ObjresOwners(target).size() == 1, "failed closure import changed target");
        });
        Case("imported full owner identity never retains a donor prefix offset", [&] {
            auto add_extra = [](const B &raw, const std::string &extra, std::uint16_t prefix) {
                auto obj = Monstey_ObjresReadChunk(raw, 2);
                auto split = 28ull + Monstey_U32(obj, 16) + Monstey_U32(obj, 20);
                B edited(obj.begin(), obj.begin() + split);
                Monstey_PutU32(edited, 24, static_cast<std::uint32_t>(extra.size()));
                edited.insert(edited.end(), extra.begin(), extra.end());
                edited.insert(edited.end(), obj.begin() + split + Monstey_U32(obj, 24), obj.end());
                Monstey_PutU16(edited, split + extra.size() + 8 + 8, prefix);
                return Monstey_ObjresReplaceChunk(raw, 2, edited);
            };
            auto donor = add_extra(GraphObjectFixture(), std::string(1, '\0'), 0);
            auto target = add_extra(objraw, std::string("Different\0", 10), 65535);
            auto result = Monstey_ObjresImportOwner(target, donor, "NativeLight", "PrefixIndependentOwner",
                                                    J::array({0, 0, 0}), J::array({0, 0, 0, 1}));
            auto owners = Monstey_ObjresOwners(result.first);
            Check(owners[1].at("prefix_offset").as_uint() == 65535 &&
                      owners[1].at("name").as_string() == "PrefixIndependentOwner",
                  "donor prefix offset was copied into a different target atom table");
        });
        Case("native object transform retains unknown chunks and properties", [&] {
            auto a = args(obj);
            a["output"] = Path(dir / "translated.objres");
            a["updates"] = J::array({J::object({{"name", "NativeLight"},
                                                {"position", J::array({100, 200, 300})},
                                                {"quaternion", J::array({0, 0, 0, 1})}})});
            Monstey_WorldCall("sow_objres_apply", a);
            auto query = Monstey_WorldCall("sow_objres_query", args(dir / "translated.objres"));
            Check(query.at("items")[0].at("position")[0].as_number() == 100, "OBJRES transform missed");
            auto before = Monstey_WorldCall("sow_objres_inspect", args(obj)),
                 after = Monstey_WorldCall("sow_objres_inspect", args(dir / "translated.objres"));
            Check(before.at("chunks")[0] == after.at("chunks")[0] &&
                      before.at("chunks")[2] == after.at("chunks")[2],
                  "unrelated OBJRES chunk changed");
        });
        Case("OBJRES exact roundtrip and class property discovery", [&] {
            auto a = args(obj);
            a["output"] = Path(dir / "identity.objres");
            Monstey_WorldCall("sow_objres_roundtrip", a);
            Check(Monstey_ReadFile(dir / "identity.objres") == objraw, "OBJRES roundtrip differs");
            Check(Monstey_WorldCall("sow_objres_classes", args(obj))
                          .at("classes")[0]
                          .at("class_name")
                          .as_string() == "Light",
                  "class discovery failed");
        });
        Case("independent object addition does not inherit native properties", [&] {
            auto a = args(obj);
            a["output"] = Path(dir / "added.objres");
            a["objects"] = J::array({J::object({{"class_name", "Light"},
                                                {"name", "NewLight"},
                                                {"position", J::array({1, 2, 3})},
                                                {"quaternion", J::array({0, 0, 0, 1})},
                                                {"properties", J::object({{"OtherInteger", 123}})},
                                                {"property_types", J::object({{"OtherInteger", 3}})}})});
            Monstey_WorldCall("sow_objres_add", a);
            auto q = args(dir / "added.objres");
            q["name_contains"] = "NewLight";
            auto row = Monstey_WorldCall("sow_objres_query", q).at("items")[0];
            Check(row.at("properties").size() == 1 && row.at("properties")[0].at("value").as_int() == 123,
                  "new object inherited unrelated source fields");
            Check(Monstey_WorldCall("sow_objres_inspect", args(dir / "added.objres"))
                          .at("property_records")
                          .as_uint() == 2,
                  "new ObjectRec not added");
        });
        Case("typed ObjectDB scalar vector and UTF16 text authoring", [&] {
            auto a = args(obj);
            a["output"] = Path(dir / "typed.objres");
            a["objects"] = J::array(
                {J::object({{"class_name", "Light"},
                            {"name", "TypedLight"},
                            {"position", J::array({0, 0, 0})},
                            {"quaternion", J::array({0, 0, 0, 1})},
                            {"properties", J::object({{"Enabled", false},
                                                      {"FloatVal", 1.25},
                                                      {"Vector", J::array({1.5, 2.5, 3.5})},
                                                      {"Text", "Wide \xce\xa9 \xf0\x9f\x94\xa5"}})},
                            {"property_types", J::object({{"FloatVal", 2}, {"Vector", 8}, {"Text", 6}})}})});
            Monstey_WorldCall("sow_objres_add", a);
            auto q = args(dir / "typed.objres");
            q["name_contains"] = "TypedLight";
            auto properties = Monstey_WorldCall("sow_objres_query", q).at("items")[0].at("properties");
            bool text = false, vector = false;
            for (auto &field : properties.as_array()) {
                if (field.at("hash").as_string() == Monstey_Hex(Monstey_NativeHash("Text")))
                    text = field.at("value").as_string() == "Wide \xce\xa9 \xf0\x9f\x94\xa5";
                if (field.at("hash").as_string() == Monstey_Hex(Monstey_NativeHash("Vector")))
                    vector = field.at("value")[2].as_number() == 3.5;
            }
            Check(text && vector && properties.size() == 4, "typed ObjectDB value readback differs");
        });
        Case("unsupported native instance arguments are never ignored", [&] {
            auto a = args(inst);
            a["output"] = Path(dir / "unknown-inst-argument.inst");
            a["spec"] =
                J::object({{"updates", J::array({J::object({{"index", 0}, {"rotation", "ignored?"}})})}});
            Reject([&] { Monstey_WorldCall("sow_inst_apply", a); });
            Check(!std::filesystem::exists(dir / "unknown-inst-argument.inst"),
                  "unsupported instance argument wrote output");
        });
        Case("object clone property overlay is copy-on-write", [&] {
            auto a = args(obj);
            a["output"] = Path(dir / "cloned.objres");
            a["spec"] = J::object(
                {{"clones", J::array({J::object({{"source_name", "NativeLight"},
                                                 {"name", "CloneLight"},
                                                 {"position", J::array({100, 0, 0})},
                                                 {"properties", J::object({{"Enabled", false}})}})})}});
            auto result = Monstey_WorldCall("sow_objres_edit", a);
            Check(result.at("changes")[0].at("copy_on_write").as_bool(), "clone did not COW");
            auto q = Monstey_WorldCall("sow_objres_query", args(dir / "cloned.objres"));
            Check(q.at("items").size() == 2 && q.at("items")[0].at("properties")[0].at("value").as_bool() &&
                      !q.at("items")[1].at("properties")[0].at("value").as_bool(),
                  "clone property mutation changed original");
        });
        Case("object property deletion and object deletion retain old ordinals", [&] {
            auto a = args(dir / "cloned.objres");
            a["output"] = Path(dir / "removed-property.objres");
            a["spec"] = J::object(
                {{"updates", J::array({J::object(
                                 {{"name", "CloneLight"}, {"remove_properties", J::array({"Enabled"})}})})}});
            Monstey_WorldCall("sow_objres_edit", a);
            auto q = args(dir / "removed-property.objres");
            q["name_contains"] = "CloneLight";
            Check(Monstey_WorldCall("sow_objres_query", q).at("items")[0].at("properties").empty(),
                  "property removal failed");
            a = args(dir / "removed-property.objres");
            a["output"] = Path(dir / "removed-object.objres");
            a["spec"] = J::object({{"deletes", J::array({J::object({{"name", "CloneLight"}})})}});
            Monstey_WorldCall("sow_objres_edit", a);
            auto report = Monstey_WorldCall("sow_objres_inspect", args(dir / "removed-object.objres"));
            Check(report.at("objects").as_uint() == 1 && report.at("property_records").as_uint() == 3,
                  "deletion compacted property ordinals");
        });
        Case("object property type conflicts reject transaction", [&] {
            auto a = args(obj);
            a["output"] = Path(dir / "wrong-property.objres");
            a["spec"] = J::object(
                {{"updates", J::array({J::object({{"name", "NativeLight"},
                                                  {"properties", J::object({{"Enabled", 4}})},
                                                  {"property_types", J::object({{"Enabled", 3}})}})})}});
            Reject([&] { Monstey_WorldCall("sow_objres_edit", a); });
            Check(!std::filesystem::exists(dir / "wrong-property.objres"), "type conflict wrote output");
        });
        Case("opaque incoming links retain the source and permit independent clones", [&] {
            auto linked = objraw;
            auto key = Monstey_NativeHash("NativeLight");
            for (std::size_t k = 0; k < 4; ++k)
                linked[linked.size() - 5 + k] = static_cast<std::uint8_t>(key >> (8 * k));
            auto source = put("linked.objres", linked);
            auto a = args(source);
            a["output"] = Path(dir / "linked-delete.objres");
            a["spec"] = J::object({{"deletes", J::array({J::object({{"name", "NativeLight"}})})}});
            Reject([&] { Monstey_WorldCall("sow_objres_edit", a); });
            auto clone_spec = J::object(
                {{"clones",
                  J::array({J::object({{"source_name", "NativeLight"}, {"name", "NewIndependentLight"}})})}});
            auto cloned = Monstey_ObjresApply(linked, clone_spec).first;
            auto owners = Monstey_ObjresOwners(cloned, false);
            Check(owners.size() == 2 && owners[0].at("name").as_string() == "NativeLight" &&
                      owners[1].at("name").as_string() == "NewIndependentLight" &&
                      Monstey_ObjresReadChunk(cloned, 77) == Monstey_ObjresReadChunk(linked, 77),
                  "independent clone changed existing source or incoming opaque links");
            auto imported =
                Monstey_ObjresImportOwner(objraw, linked, "NativeLight", "ImportedIndependentLight",
                                          J::array({0, 0, 0}), J::array({0, 0, 0, 1}));
            Check(Monstey_ObjresOwners(imported.first, false).size() == 2,
                  "cross-file clone rejected preserved donor incoming links");
            Check(!std::filesystem::exists(dir / "linked-delete.objres"),
                  "unqualified linked identity deleted");
        });
        Case("OBJRES source hash and truncation rejected", [&] {
            auto a = args(obj);
            a["expected_sha256"] = "bad";
            Reject([&] { Monstey_WorldCall("sow_objres_inspect", a); });
            auto broken = objraw;
            broken.resize(20);
            Reject([&] { Monstey_WorldCall("sow_objres_inspect", args(put("broken.objres", broken))); });
        });
        Case("independent PRFB8 full-prefix identity fixture", [&] {
            auto report = Monstey_WorldCall("sow_inst_inspect", args(inst));
            Check(report.at("instances_total").as_uint() == 1 &&
                      report.at("primary_definition_count").as_uint() == 1,
                  "PRFB parse failed");
            auto a = args(inst);
            a["output"] = Path(dir / "identity.inst");
            Monstey_WorldCall("sow_inst_roundtrip", a);
            Check(Monstey_ReadFile(dir / "identity.inst") == instraw, "PRFB identity differs");
        });
        Case("PRFB transformed bounds and owner copy-on-write", [&] {
            auto a = args(inst);
            a["output"] = Path(dir / "moved.inst");
            a["spec"] = J::object({{"updates", J::array({J::object({{"index", 0},
                                                                    {"owner_definition_key", "0x12345678"},
                                                                    {"position", J::array({100, 200, 300})},
                                                                    {"quaternion", J::array({0, 0, 0, 1})},
                                                                    {"scale", 2}})})}});
            Monstey_WorldCall("sow_inst_apply", a);
            auto page = Monstey_WorldCall("sow_inst_inspect", args(dir / "moved.inst"));
            Check(page.at("instances_total").as_uint() == 2 &&
                      page.at("instances")[0].at("position")[0].as_number() == 0,
                  "old instance slot changed");
            Check(page.at("instances")[1].at("bounds_min")[0].as_number() == 98 &&
                      page.at("instances")[1].at("bounds_max")[2].as_number() == 306,
                  "world AABB remap wrong");
        });
        Case("PRFB owner clone and delete preserve old physical slots", [&] {
            auto clone = args(inst);
            clone["output"] = Path(dir / "clone.inst");
            clone["spec"] = J::object(
                {{"clones", J::array({J::object({{"index", 0}, {"position", J::array({10, 0, 0})}})})}});
            Monstey_WorldCall("sow_inst_apply", clone);
            auto c = Monstey_WorldCall("sow_inst_inspect", args(dir / "clone.inst"));
            Check(c.at("instances_total").as_uint() == 3 && c.at("effective_instance_count").as_uint() == 2,
                  "clone owner range wrong");
            auto remove = args(inst);
            remove["output"] = Path(dir / "delete.inst");
            remove["spec"] = J::object({{"deletes", J::array({J::object({{"index", 0}})})}});
            Monstey_WorldCall("sow_inst_apply", remove);
            auto d = Monstey_WorldCall("sow_inst_inspect", args(dir / "delete.inst"));
            Check(d.at("instances_total").as_uint() == 1 && d.at("effective_instance_count").as_uint() == 0,
                  "delete compacted physical table");
        });
        Case("native donor-backed placement preserves definition and raw flags", [&] {
            auto a = args(inst);
            a["output"] = Path(dir / "donor-added.inst");
            a["spec"] = J::object(
                {{"instances",
                  J::array({J::object({{"prefab_path", "prefabs/test.inst"},
                                       {"position", J::array({50, 0, 0})},
                                       {"quaternion", J::array({0, 0, 0, 1})},
                                       {"donor", J::object({{"source", Path(inst)}, {"index", 0}})}})})}});
            Monstey_WorldCall("sow_inst_apply", a);
            auto report = Monstey_WorldCall("sow_inst_inspect", args(dir / "donor-added.inst"));
            Check(report.at("instances_total").as_uint() == 2 &&
                      report.at("effective_instance_count").as_uint() == 2,
                  "donor addition is not owned");
            auto &row = report.at("instances")[1];
            Check(row.at("position")[0].as_number() == 50 &&
                      row.at("definition_key").as_string() == "0x00000090" &&
                      row.at("word8_raw").as_uint() == 0xfeed1234,
                  "native donor identity/flags changed");
        });
        Case("qualified render owner copy imports no geometry or LOD", [&] {
            auto donor = instraw;
            Monstey_PutU32(donor, 56, 0x87654321);
            Monstey_PutU32(donor, 56 + 16, 0xffffffff);
            auto p = put("owner-donor.inst", donor);
            auto a = args(inst);
            a["output"] = Path(dir / "owner-added.inst");
            a["spec"] =
                J::object({{"owner_donor", J::object({{"source", Path(p)}, {"definition_index", 0}})}});
            Monstey_WorldCall("sow_inst_apply", a);
            auto report = Monstey_WorldCall("sow_inst_inspect", args(dir / "owner-added.inst"));
            Check(report.at("primary_definition_count").as_uint() == 2 &&
                      report.at("primary_definitions")[1].at("nested_instance_range") == J::array({0, 0}),
                  "owner imported stale instance range");
        });
        Case("defined prefab placement uses actual target vertices and key", [&] {
            auto target = put("defined-target.inst", DefinedPrefabFixture());
            auto a = args(inst);
            a["output"] = Path(dir / "defined-added.inst");
            a["spec"] =
                J::object({{"defined_instances",
                            J::array({J::object({{"target_resource_path", "custom/target.inst"},
                                                 {"position", J::array({100, 0, 0})},
                                                 {"quaternion", J::array({0, 0, 0, 1})},
                                                 {"scale", 2},
                                                 {"target", J::object({{"source", Path(target)}})},
                                                 {"placement_donor",
                                                  J::object({{"source", Path(inst)}, {"index", 0}})}})})}});
            auto result = Monstey_WorldCall("sow_inst_apply", a);
            Check(result.at("changes")[0].at("native_donor_bounds_not_used").as_bool(),
                  "defined bounds not proven");
            auto report = Monstey_WorldCall("sow_inst_inspect", args(dir / "defined-added.inst"));
            auto &added = report.at("instances")[1];
            Check(added.at("prefab_path").as_string() == "custom/target.inst" &&
                      added.at("definition_key").as_string() == "0x76543210",
                  "defined target identity was replaced by donor");
            Check(added.at("bounds_min")[0].as_number() == 100 &&
                      added.at("bounds_min")[2].as_number() == 0 &&
                      added.at("bounds_max")[1].as_number() == 2,
                  "defined placement used donor AABB instead of actual target geometry");
        });
        Case("unsupported PRFB donor fields are explicit", [&] {
            auto a = args(inst);
            a["output"] = Path(dir / "unsupported.inst");
            a["spec"] = J::object({{"unknown_donor_field", J::array()}});
            Reject([&] { Monstey_WorldCall("sow_inst_apply", a); });
            Check(!std::filesystem::exists(dir / "unsupported.inst"), "unsupported operation wrote output");
        });
        Case("native MESH5 geometry layout and draw fixture", [&] {
            auto report = Monstey_WorldCall("sow_worldmesh_inspect", args(mesh));
            auto draw = report.at("groups")[0].at("draws")[0];
            Check(draw.at("vertices").as_uint() == 3 && draw.at("triangles").as_uint() == 1 &&
                      draw.at("bounds_max")[0].as_number() == 1,
                  "MESH5 decode failed");
        });
        Case("MESH5 true vertex transformation with untouched normals and indices", [&] {
            auto a = args(mesh);
            a["output"] = Path(dir / "moved.wldmsh");
            a["spec"] = J::object(
                {{"updates",
                  J::array({J::object(
                      {{"group_index", 0}, {"draw_index", 0}, {"translation", J::array({10, 20, 30})}})})}});
            Monstey_WorldCall("sow_worldmesh_apply", a);
            auto output = Monstey_ReadFile(dir / "moved.wldmsh");
            Check(output.size() == meshraw.size(), "fixed topology changed extent");
            auto draw = Monstey_WorldCall("sow_worldmesh_inspect", args(dir / "moved.wldmsh"))
                            .at("groups")[0]
                            .at("draws")[0];
            Check(draw.at("bounds_min")[0].as_number() == 10 && draw.at("bounds_max")[1].as_number() == 21,
                  "MESH5 translation wrong");
            auto position = draw.at("attributes").at("POSITION").at("offset").as_uint();
            for (std::size_t v = 0; v < 3; ++v)
                for (std::size_t b = 12; b < 24; ++b)
                    Check(output[position + v * 24 + b] == meshraw[position + v * 24 + b],
                          "native normal/signed zero bits changed");
        });
        Case("aliased draw edits use original values and reject contradictions", [&] {
            auto alias = put("alias.wldmsh", MeshFixture(2));
            auto a = args(alias);
            a["output"] = Path(dir / "alias-moved.wldmsh");
            auto one =
                     J::object({{"group_index", 0}, {"draw_index", 0}, {"translation", J::array({1, 0, 0})}}),
                 two = one;
            two["draw_index"] = 1;
            a["spec"] = J::object({{"updates", J::array({one, two})}});
            Monstey_WorldCall("sow_worldmesh_apply", a);
            two["translation"] = J::array({2, 0, 0});
            a["output"] = Path(dir / "alias-conflict.wldmsh");
            a["spec"] = J::object({{"updates", J::array({one, two})}});
            Reject([&] { Monstey_WorldCall("sow_worldmesh_apply", a); });
            Check(!std::filesystem::exists(dir / "alias-conflict.wldmsh"), "conflicting alias wrote file");
        });
        Case("MESH5 identity transaction is byte exact", [&] {
            auto a = args(mesh);
            a["output"] = Path(dir / "identity.wldmsh");
            a["spec"] =
                J::object({{"updates", J::array({J::object({{"group_index", 0}, {"draw_index", 0}})})}});
            Monstey_WorldCall("sow_worldmesh_apply", a);
            Check(Monstey_ReadFile(dir / "identity.wldmsh") == meshraw, "identity geometry bits changed");
        });
        Case("standard native NPZ all-channel export and exact reimport", [&] {
            auto a = args(mesh);
            a["output"] = Path(dir / "geometry.npz");
            Monstey_WorldCall("sow_worldmesh_export", a);
            auto npz = Monstey_ReadFile(dir / "geometry.npz");
            Check(Monstey_U32(npz, 0) == 0x04034b50 && Monstey_U32(npz, npz.size() - 22) == 0x06054b50,
                  "not a standard ZIP NPZ");
            a = args(mesh);
            a["output"] = Path(dir / "npz-identity.wldmsh");
            a["spec"] = J::object(
                {{"updates", J::array({J::object({{"group_index", 0},
                                                  {"draw_index", 0},
                                                  {"attributes_npz", Path(dir / "geometry.npz")}})})}});
            Monstey_WorldCall("sow_worldmesh_apply", a);
            Check(Monstey_ReadFile(dir / "npz-identity.wldmsh") == meshraw,
                  "NPZ roundtrip lost raw channels or signed zero");
        });
        Case("independent compressed ZIP method8 NPY import and float64 conversion", [&] {
            auto input = put("independent.npz", IndependentNpz("<f8", true));
            auto a = args(mesh);
            a["output"] = Path(dir / "npz-edited.wldmsh");
            a["spec"] = J::object({{"updates", J::array({J::object({{"group_index", 0},
                                                                    {"draw_index", 0},
                                                                    {"attributes_npz", Path(input)}})})}});
            Monstey_WorldCall("sow_worldmesh_apply", a);
            auto draw = Monstey_WorldCall("sow_worldmesh_inspect", args(dir / "npz-edited.wldmsh"))
                            .at("groups")[0]
                            .at("draws")[0];
            Check(draw.at("bounds_min")[0].as_number() == 3 && draw.at("bounds_max")[2].as_number() == 7,
                  "compressed NPZ import failed");
        });
        Case("NPZ CRC and unsafe numeric dtype rejected before output", [&] {
            auto damaged = IndependentNpz("<f4");
            auto central = Monstey_U32(damaged, damaged.size() - 6);
            damaged[central + 16] ^= 1;
            auto input = put("badcrc.npz", damaged);
            auto a = args(mesh);
            a["output"] = Path(dir / "badcrc.wldmsh");
            a["spec"] = J::object({{"updates", J::array({J::object({{"group_index", 0},
                                                                    {"draw_index", 0},
                                                                    {"attributes_npz", Path(input)}})})}});
            Reject([&] { Monstey_WorldCall("sow_worldmesh_apply", a); });
            Check(!std::filesystem::exists(dir / "badcrc.wldmsh"), "bad CRC wrote output");
            auto object = put("objectdtype.npz", IndependentNpz("|O4"));
            a["output"] = Path(dir / "objectdtype.wldmsh");
            a["spec"]["updates"][0]["attributes_npz"] = Path(object);
            Reject([&] { Monstey_WorldCall("sow_worldmesh_apply", a); });
        });
        Case("shared world geometry reader retains raw channels and triangles", [&] {
            auto row = Monstey_WorldMeshDecodeGeometry(meshraw, 0, 0);
            Check(row.at("POSITION").size() == 3 && row.at("POSITION")[1][0].as_number() == 1 &&
                      row.at("INDICES")[0] == J::array({0, 1, 2}),
                  "world geometry byte API differs");
            Reject([&] { Monstey_WorldMeshDecodeGeometry(meshraw, 1, 0); });
        });
        Case("raw native context getter preserves opaque bytes and encoded layouts", [&] {
            auto data = Monstey_WorldMeshReadContexts(meshraw);
            Check(data.groups.size() == 1 && data.header[7] == 3 &&
                      data.opaque_header == B({0xab, 0xcd, 0xef}) && data.groups[0].layouts.size() == 1 &&
                      data.groups[0].layout_strides[0] == 24 && data.groups[0].draws.size() == 1 &&
                      data.groups[0].vertex_buffer.size() == 72 && data.groups[0].index_buffer.size() == 6,
                  "raw native mesh context discarded unknown data or encoded structure");
            Check(data.groups[0].draws[0][6] == 7, "raw native draw words changed");
        });
        Case("complete PRFB scene reader does not inherit inspector pagination", [&] {
            auto raw = instraw;
            auto names = Monstey_U32(raw, 52);
            auto at = raw.size() - names - 80;
            B repeated(raw.begin(), raw.begin() + static_cast<std::ptrdiff_t>(at));
            for (unsigned i = 0; i < 600; ++i)
                repeated.insert(repeated.end(), raw.begin() + static_cast<std::ptrdiff_t>(at),
                                raw.begin() + static_cast<std::ptrdiff_t>(at + 80));
            repeated.insert(repeated.end(), raw.end() - names, raw.end());
            Monstey_PutU32(repeated, 24, 600);
            Monstey_PutU32(repeated, 56 + 36, 600);
            auto report = Monstey_InstSceneInstances(repeated);
            Check(report.at("instances").size() == 600 && !report.at("instances_truncated").as_bool() &&
                      report.at("instances")[599].at("owners").size() == 1,
                  "scene attachment lost native placements");
            auto paged = put("six-hundred.inst", repeated);
            auto a = args(paged);
            a["instances_limit"] = 500;
            Check(Monstey_WorldCall("sow_inst_inspect", a).at("instances").size() == 500,
                  "bounded inspector pagination changed");
        });
        Case("shared PRFB geometry extraction and exact numeric channel decode", [&] {
            auto raw = DefinedPrefabFixture();
            auto data = Monstey_InstReadGeometry(raw);
            Check(data.primary.size() == 1 && data.secondary.empty() && data.vertex_buffer.size() == 72 &&
                      data.index_buffer.size() == 6 && data.layouts.size() == 1 && data.draws.size() == 1 &&
                      data.material_names[0] == "test.mat",
                  "shared PRFB geometry fields differ");
            auto decoded = Monstey_InstDecodeGeometry(raw);
            Check(decoded.size() == 1 && decoded[0].at("POSITION").size() == 3 &&
                      decoded[0].at("NORMAL")[0][2].as_number() == 1 &&
                      decoded[0].at("INDICES")[0][2].as_uint() == 2,
                  "shared static numeric channel/indices decode differs");
        });
        Case("rigid PRFB authoring preserves native terminal donor and draw parameters", [&] {
            auto raw = DefinedPrefabFixture();
            auto secondary = B(raw.begin() + 56, raw.begin() + 100);
            auto donor = raw;
            donor.insert(donor.begin() + 100, secondary.begin(), secondary.end());
            Monstey_PutU32(donor, 28, 1);
            auto first = std::size_t(100);
            auto end = raw.size() - Monstey_U32(raw, 32);
            B geometry(raw.begin() + first, raw.begin() + static_cast<std::ptrdiff_t>(end));
            Monstey_PutU32(geometry, geometry.size() - 8, 0x1234abcd);
            Monstey_PutU32(geometry, geometry.size() - 4, 0x5678ef01);
            auto output = Monstey_InstBuildRigid(donor, "prefabs/custom/rigid.inst",
                                                 "materials/custom/rigid.mat", geometry);
            auto decoded = Monstey_InstDecodeGeometry(output);
            auto metadata = Monstey_InstReadGeometry(output);
            Check(metadata.header[2] == Monstey_PathHash("prefabs/custom/rigid.inst") &&
                      metadata.primary.size() == 1 && metadata.secondary.empty() &&
                      Monstey_U32(metadata.primary[0], 0) == Monstey_U32(raw, 56) &&
                      metadata.material_names[0] == "materials\\custom\\rigid.mat" &&
                      decoded[0].at("draw_parameter_words")[0].as_uint() == 0x1234abcd &&
                      decoded[0].at("draw_parameter_words")[1].as_uint() == 0x5678ef01,
                  "rigid resource/header/material/draw parameters were not preserved");
            auto source = Monstey_InstDecodeGeometry(raw);
            Check(decoded[0].at("POSITION") == source[0].at("POSITION") &&
                      decoded[0].at("NORMAL") == source[0].at("NORMAL") &&
                      decoded[0].at("INDICES") == source[0].at("INDICES"),
                  "rigid conversion modified original channel or triangle data");
            auto invalid = donor;
            Monstey_PutU32(invalid, 100 + 16, 0);
            Reject([&] {
                Monstey_InstBuildRigid(invalid, "prefabs/custom/bad.inst", "materials/custom/bad.mat",
                                       geometry);
            });
            geometry.pop_back();
            Reject([&] {
                Monstey_InstBuildRigid(donor, "prefabs/custom/truncated.inst", "materials/custom/bad.mat",
                                       geometry);
            });
        });
        Case("in-memory native PRFB MESH and binding APIs share the actual codecs", [&] {
            Check(Monstey_InstApplyBytes(instraw, J::object()).first == instraw,
                  "memory PRFB identity differs");
            Check(Monstey_WorldMeshApplyBytes(meshraw, J::object()).first == meshraw,
                  "memory MESH identity differs");
            auto edited = Monstey_WorldMeshApplyBytes(
                meshraw,
                J::object({{"updates", J::array({J::object({{"group_index", 0},
                                                            {"draw_index", 0},
                                                            {"translation", J::array({10, 20, 30})}})})}}));
            auto output = put("memory-translated.wldmsh", edited.first);
            auto report = Monstey_WorldCall("sow_worldmesh_inspect", args(output));
            Check(report.at("groups")[0].at("draws")[0].at("bounds_min")[0].as_number() == 10,
                  "memory mesh transform not applied");
            auto binding =
                Monstey_WdscBindingBytes(instraw, "worlds/test.inst", 0x12345678, BindingFixture());
            Check(binding.second.at("owned_instance_range")[1].as_uint() == 1 &&
                      binding.second.at("exact_leaf_AABB_union").as_bool(),
                  "memory binding owner association differs");
            Reject([&] {
                Monstey_WdscBindingBytes(instraw, "worlds/wrong.inst", 0x12345678, BindingFixture());
            });
        });
        auto scene_prefab = put("scene-target.inst", DefinedPrefabFixture());
        auto scene_spec = J::object(
            {{"sources",
              J::array({J::object({{"id", "objects"},
                                   {"source", Path(obj)},
                                   {"resource_path", "worlds/a.objres"},
                                   {"frame", "local"},
                                   {"frame_to_scene", J::object({{"position", J::array({100, 0, 0})},
                                                                 {"quaternion", J::array({0, 0, 0, 1})},
                                                                 {"scale", 1}})}}),
                        J::object({{"id", "mesh"},
                                   {"source", Path(mesh)},
                                   {"resource_path", "worlds/a.wldmsh"},
                                   {"frame", "world"}}),
                        J::object({{"id", "prefab"},
                                   {"source", Path(scene_prefab)},
                                   {"resource_path", "custom/target.inst"},
                                   {"frame", "world"}})})}});
        Case("mixed native scene retains source frames properties and real geometry", [&] {
            auto result =
                Monstey_WorldCall("sow_world_scene_prepare",
                                  J::object({{"spec", scene_spec}, {"output", Path(dir / "scene")}}));
            auto scene = J::Parse(Monstey_ReadText(dir / "scene/scene.json"));
            Check(result.at("sources").as_uint() == 3 && scene.at("objects").size() == 1 &&
                      scene.at("geometry").size() == 2,
                  "scene source or native geometry missing");
            Check(scene.at("objects")[0].at("scene_matrix_native")[0][3].as_number() == 110 &&
                      scene.at("origin_native_cm")[0].as_number() == 110,
                  "native frame or median origin differs");
            Check(scene.at("objects")[0].at("property_fields").size() == 1 &&
                      scene.at("geometry")[1].at("representation").as_string() ==
                          "REAL_NATIVE_RIGID_TRIANGLES" &&
                      scene.at("source_spec") == scene_spec,
                  "scene native data or reproducible spec was lost");
        });
        Case("local scene source needs an explicit frame and invalid edits publish nothing", [&] {
            auto bad = scene_spec;
            bad["sources"][0].as_object().erase("frame_to_scene");
            Reject([&] {
                Monstey_WorldCall("sow_world_scene_prepare",
                                  J::object({{"spec", bad}, {"output", Path(dir / "invalid-scene")}}));
            });
            Check(!std::filesystem::exists(dir / "invalid-scene/scene.json"),
                  "invalid local frame published a scene");
        });
        Case("native scene metadata never overwrites an existing output", [&] {
            auto before = Monstey_HashSha256(dir / "scene/scene.json");
            Reject([&] {
                Monstey_WorldCall("sow_world_scene_prepare",
                                  J::object({{"spec", scene_spec}, {"output", Path(dir / "scene")}}));
            });
            Check(Monstey_HashSha256(dir / "scene/scene.json") == before, "scene output overwritten");
        });
        auto render = put("source.wldclnt", RenderFixture());
        Case("full native WDSC model material culling graph", [&] {
            auto report = Monstey_WorldCall("sow_world_render_inspect", args(render));
            Check(report.at("roundtrip_byte_exact").as_bool() &&
                      report.at("models")[0].at("materials").size() == 8 &&
                      report.at("groups")[0].at("static_draws")[0].as_uint() == 0,
                  "full WDSC graph lost associations");
        });
        Case("arbitrary native topology and paired material associations", [&] {
            auto a = args(mesh);
            a["expected_sha256"] = Monstey_HashSha256(meshraw);
            a["output"] = Path(dir / "topology.wldmsh");
            a["spec"] = J::object(
                {{"groups", J::array({J::object(
                                {{"group_index", 0},
                                 {"draws", J::array({J::object({{"source_draw", 0}}),
                                                     J::object({{"source_draw", 0},
                                                                {"material", "materials/new.mat"}})})}})})}});
            a["render_source"] = Path(render);
            a["render_expected_sha256"] = Monstey_HashSha256(RenderFixture());
            a["render_output"] = Path(dir / "topology.wldclnt");
            a["source_resource"] = "worlds/source.wldmsh";
            a["target_resource"] = "worlds/target.wldmsh";
            a["render_mode"] = "private";
            auto proof = Monstey_WorldCall("sow_world_topology_build", a);
            Check(proof.at("topology").at("all_vertex_channels_retained").as_bool(),
                  "topology omitted channels");
            auto mesh_report = Monstey_WorldCall("sow_worldmesh_inspect", args(dir / "topology.wldmsh"));
            Check(mesh_report.at("groups")[0].at("draws").size() == 2 &&
                      mesh_report.at("groups")[0].at("draws")[1].at("material_slot_raw").as_uint() == 8,
                  "topology draw/material slots wrong");
            auto graph = Monstey_WorldCall("sow_world_render_inspect", args(dir / "topology.wldclnt"));
            Check(graph.at("models")[0].at("path").as_string() == "worlds/target.wldmsh" &&
                      graph.at("models")[0].at("materials")[8].as_string() == "materials/new.mat",
                  "paired native material association lost");
        });
        Case("private topology emits native scene group routing and indexed union bounds", [&] {
            const auto raw = Monstey_ReadFile(dir / "topology.wldclnt");
            const auto scene = Monstey_WorldScene::Read(std::span(raw).last(200), 1);
            Check(scene.Encode(1).size() == 200 &&
                      scene.Inspect(1).at("nodes")[0].at("group").as_uint() == 0,
                  "Private render has no real group-routed scene");
            const auto report = scene.Inspect(1);
            const auto mesh_report = Monstey_WorldCall(
                "sow_worldmesh_inspect", args(dir / "topology.wldmsh"));
            const auto &draw = mesh_report.at("groups")[0].at("draws")[0];
            Check(report.at("nodes")[0].at("bounds")[0] == draw.at("bounds_min")[0] &&
                      report.at("nodes")[0].at("bounds")[3] == draw.at("bounds_max")[0],
                  "Private native scene bounds do not cover indexed geometry");
        });
        Case("native descriptor selects nonzero context without losing unrelated geometry", [&] {
            auto multi = meshraw;
            const auto group_at = 32 + Monstey_U32(meshraw, 28);
            multi.insert(multi.end(), meshraw.begin() + group_at, meshraw.end());
            for (auto offset : {8u, 12u, 16u, 20u, 24u})
                Monstey_PutU32(multi, offset, 2 * Monstey_U32(meshraw, offset));
            const auto multi_source = put("two-contexts.wldmsh", multi);
            auto native_render = RenderFixture();
            const auto model_at = 44 + Monstey_U32(native_render, 40);
            Monstey_PutU32(native_render, model_at + 8, 1);
            const auto render_source = put("second-context.wldclnt", native_render);
            auto a = args(multi_source);
            a["expected_sha256"] = Monstey_HashSha256(multi);
            a["output"] = Path(dir / "second-context.wldmsh");
            a["spec"] = J::object();
            a["render_source"] = Path(render_source);
            a["render_expected_sha256"] = Monstey_HashSha256(native_render);
            a["render_output"] = Path(dir / "second-context-result.wldclnt");
            a["source_resource"] = "worlds/source.wldmsh";
            a["target_resource"] = "worlds/second-context.wldmsh";
            const auto result = Monstey_WorldCall("sow_world_topology_build", a);
            const auto output = Monstey_ReadFile(dir / "second-context.wldmsh");
            Check(output == multi &&
                      result.at("render").at("static_geometry_context").as_uint() == 1 &&
                      result.at("render").at("models")[0].at("geometry_group").as_uint() == 1 &&
                      result.at("render").at("native_scene").at("group_routing_built").as_bool(),
                  "Nonzero native context selection changed another context or its model binding");
        });
        auto merge_meshraw = meshraw;
        const auto merge_opaque = Monstey_U32(merge_meshraw, 28);
        merge_meshraw.erase(merge_meshraw.begin() + 32,
                            merge_meshraw.begin() + 32 + merge_opaque);
        Monstey_PutU32(merge_meshraw, 28, 0);
        const auto merge_mesh = put("merge-source.wldmsh", merge_meshraw);
        const auto common_merge_args = [&](const std::string &suffix) {
            auto a = args(merge_mesh);
            a["expected_sha256"] = Monstey_HashSha256(merge_meshraw);
            a["output"] = Path(dir / (suffix + ".wldmsh"));
            a["spec"] = J::object({{"groups", J::array({J::object({
                {"group_index", 0}, {"draws", J::array({
                    J::object({{"source_draw", 0}}),
                    J::object({{"source_draw", 0}, {"material", "materials/merged-new.mat"}})
                })}})})}});
            a["render_source"] = Path(render);
            a["render_expected_sha256"] = Monstey_HashSha256(RenderFixture());
            a["render_output"] = Path(dir / (suffix + ".wldclnt"));
            a["source_resource"] = "worlds/source.wldmsh";
            a["target_resource"] = "worlds/" + suffix + ".wldmsh";
            a["merge_sources"] = J::array({J::object({
                {"id", "additional"}, {"mesh_source", Path(merge_mesh)},
                {"mesh_expected_sha256", Monstey_HashSha256(merge_meshraw)},
                {"render_source", Path(render)},
                {"render_expected_sha256", Monstey_HashSha256(RenderFixture())},
                {"source_resource", "worlds/source.wldmsh"}
            })});
            return a;
        };
        Case("paired multi-source composition remaps every draw material and spatial route", [&] {
            auto a = common_merge_args("merged-common");
            const auto result = Monstey_WorldCall("sow_world_topology_build", a);
            const auto &merged = result.at("merge");
            const auto &renderer = result.at("render");
            const auto geometry = Monstey_WorldMeshReadContexts(
                Monstey_ReadFile(dir / "merged-common.wldmsh"));
            Check(geometry.groups[0].draws.size() == 3 &&
                      renderer.at("models")[0].at("materials").size() == 17 &&
                      (geometry.groups[0].draws[1][6] & 65535) == 8 &&
                      (geometry.groups[0].draws[2][6] & 65535) == 16,
                  "Merged slot table lost unused native slots or base authored material");
            Check(merged.at("encoded_channels_byte_exact").as_bool() &&
                      merged.at("render_associations_written").as_bool() &&
                      merged.at("native_scene_group_routing_built").as_bool() &&
                      renderer.at("all_native_draws_have_cull_record").as_bool() &&
                      !renderer.at("native_scene").at("explicit_scene_template").as_bool() &&
                      renderer.at("native_scene").at("prototype_origin").as_string() == "base_render_source" &&
                      result.at("topology").at("output_sha256").as_string() ==
                          Monstey_HashSha256(dir / "merged-common.wldmsh"),
                  "Native MESH, material, cull and scene reports disagree");
            Check(renderer.at("models")[0].at("path").as_string() ==
                      "worlds/merged-common.wldmsh" &&
                      Monstey_ReadFile(merge_mesh) == merge_meshraw &&
                      Monstey_ReadFile(render) == RenderFixture(),
                  "Source mutated or final common resource descriptor is missing");
        });
        Case("additional cull prototypes retain raw flags while indexed bounds are rebuilt", [&] {
            auto translated = args(merge_mesh);
            translated["output"] = Path(dir / "merge-translated-source.wldmsh");
            translated["spec"] = J::object({{"updates", J::array({
                J::object({{"group_index", 0}, {"draw_index", 0},
                           {"translation", J::array({10, 20, 30})}})
            })}});
            Monstey_WorldCall("sow_worldmesh_apply", translated);
            auto donor = RenderFixture();
            const auto old_cull = 44 + Monstey_U32(donor, 40) + 12 + 8 * 4 + 12;
            donor[old_cull + 24] = 0;
            donor[old_cull + 25] = 0xa5;
            donor[old_cull + 30] = 0x5a;
            const auto donor_path = put("merge-opaque-source.wldclnt", donor);
            auto a = common_merge_args("merge-opaque");
            a["merge_sources"][0]["mesh_source"] = translated.at("output");
            a["merge_sources"][0]["mesh_expected_sha256"] =
                Monstey_HashSha256(dir / "merge-translated-source.wldmsh");
            a["merge_sources"][0]["render_source"] = Path(donor_path);
            a["merge_sources"][0]["render_expected_sha256"] = Monstey_HashSha256(donor);
            const auto result = Monstey_WorldCall("sow_world_topology_build", a);
            const auto &group = result.at("render").at("groups")[0];
            Check(group.at("eligible_static_prefix_count").as_uint() == 2 &&
                      group.at("static_draws")[2].as_uint() == 2 &&
                      group.at("bounds")[2][0].as_number() == 10 &&
                      group.at("bounds")[2][5].as_number() == 30,
                  "Additional source culls lost flags or retained obsolete donor bounds");
            const auto output = Monstey_ReadFile(dir / "merge-opaque.wldclnt");
            const auto cull = 44 + Monstey_U32(output, 40) + 12 + 17 * 4 + 12;
            Check(output[cull + 2 * 32 + 25] == 0xa5 &&
                      output[cull + 2 * 32 + 30] == 0x5a,
                  "Opaque cull bytes were interpreted as a fabricated model selector");
        });
        Case("merge rejects changed snapshots and protects every additional source output", [&] {
            auto a = common_merge_args("merge-invalid");
            a["merge_sources"][0]["mesh_expected_sha256"] = std::string(64, '0');
            Reject([&] { Monstey_WorldCall("sow_world_topology_build", a); });
            Check(!std::filesystem::exists(dir / "merge-invalid.wldmsh") &&
                      !std::filesystem::exists(dir / "merge-invalid.wldclnt"),
                  "Invalid additional snapshot published half an edit");
            a = common_merge_args("merge-protected");
            a["render_output"] = Path(render);
            Reject([&] { Monstey_WorldCall("sow_world_topology_build", a); });
            Check(!std::filesystem::exists(dir / "merge-protected.wldmsh") &&
                      Monstey_ReadFile(render) == RenderFixture(),
                  "Merged publication overwrote a qualified input or left a partial pair");
            a = common_merge_args("merge-opaque-header");
            a["merge_sources"][0]["mesh_source"] = Path(mesh);
            a["merge_sources"][0]["mesh_expected_sha256"] = Monstey_HashSha256(meshraw);
            Reject([&] { Monstey_WorldCall("sow_world_topology_build", a); });
            a = common_merge_args("merge-duplicate");
            a["merge_sources"][0]["id"] = "base";
            Reject([&] { Monstey_WorldCall("sow_world_topology_build", a); });
            a = common_merge_args("merge-preserve");
            a["render_mode"] = "preserve-section";
            Reject([&] { Monstey_WorldCall("sow_world_topology_build", a); });
        });
        Case("merge uses a native nonzero base context and retains unrelated encoded contexts", [&] {
            auto a = common_merge_args("merge-nonzero");
            auto multi = merge_meshraw;
            multi.insert(multi.end(), merge_meshraw.begin() + 32, merge_meshraw.end());
            for (auto offset : {8u, 12u, 16u, 20u, 24u})
                Monstey_PutU32(multi, offset, 2 * Monstey_U32(merge_meshraw, offset));
            put("merge-two-contexts.wldmsh", multi);
            a["source"] = Path(dir / "merge-two-contexts.wldmsh");
            a["expected_sha256"] = Monstey_HashSha256(dir / "merge-two-contexts.wldmsh");
            a["render_source"] = Path(dir / "second-context.wldclnt");
            a["render_expected_sha256"] = Monstey_HashSha256(dir / "second-context.wldclnt");
            a["spec"] = J::object();
            const auto before = Monstey_WorldMeshReadContexts(
                Monstey_ReadFile(dir / "merge-two-contexts.wldmsh"));
            const auto result = Monstey_WorldCall("sow_world_topology_build", a);
            const auto after = Monstey_WorldMeshReadContexts(
                Monstey_ReadFile(dir / "merge-nonzero.wldmsh"));
            Check(result.at("merge").at("target_context").as_uint() == 1 &&
                      result.at("render").at("models")[0].at("geometry_group").as_uint() == 1 &&
                      after.groups.size() == 2 && after.groups[1].draws.size() == 2 &&
                      before.groups[0].vertex_buffer == after.groups[0].vertex_buffer &&
                      before.groups[0].index_buffer == after.groups[0].index_buffer &&
                      before.groups[0].draws == after.groups[0].draws &&
                      before.groups[0].layouts == after.groups[0].layouts,
                  "Common-context composition changed unselected geometry or native descriptor binding");
            auto additional = common_merge_args("merge-additional-nonzero");
            additional["merge_sources"][0]["mesh_source"] =
                Path(dir / "merge-two-contexts.wldmsh");
            additional["merge_sources"][0]["mesh_expected_sha256"] =
                Monstey_HashSha256(dir / "merge-two-contexts.wldmsh");
            additional["merge_sources"][0]["render_source"] =
                Path(dir / "second-context.wldclnt");
            additional["merge_sources"][0]["render_expected_sha256"] =
                Monstey_HashSha256(dir / "second-context.wldclnt");
            const auto additional_result = Monstey_WorldCall("sow_world_topology_build", additional);
            Check(additional_result.at("merge").at("draw_remap")[2]
                        .at("source_context").as_uint() == 1 &&
                      additional_result.at("render").at("models")[0]
                        .at("geometry_group").as_uint() == 0 &&
                      additional_result.at("render").at("groups")[0].at("static_draws").size() == 3,
                  "Additional source selected context zero instead of its native descriptor context");
        });
        Case("late paired publication failure rolls back already published mesh and render", [&] {
            const auto blocked = put("merge-parent-is-file", B{0x42, 0x13});
            auto a = common_merge_args("merge-late-second");
            a["render_output"] = Path(blocked / "render.wldclnt");
            bool late = false;
            try {
                Monstey_WorldCall("sow_world_topology_build", a);
            } catch (const std::filesystem::filesystem_error &error) {
                late = std::string(error.what()).find("create_directories") != std::string::npos;
            }
            Check(late && !std::filesystem::exists(dir / "merge-late-second.wldmsh") &&
                      Monstey_ReadFile(blocked) == B({0x42, 0x13}),
                  "Second publication did not fail late or failed to roll back its mesh");
            a = common_merge_args("merge-late-third");
            a["report"] = Path(blocked / "report.json");
            late = false;
            try {
                Monstey_WorldCall("sow_world_topology_build", a);
            } catch (const std::filesystem::filesystem_error &error) {
                late = std::string(error.what()).find("create_directories") != std::string::npos;
            }
            Check(late && !std::filesystem::exists(dir / "merge-late-third.wldmsh") &&
                      !std::filesystem::exists(dir / "merge-late-third.wldclnt") &&
                      Monstey_ReadFile(mesh) == meshraw && Monstey_ReadFile(render) == RenderFixture(),
                  "Third publication did not roll back both resources or changed its source");
        });
        const auto duplicate_group_render = [&](bool conflicting) {
            const auto one = RenderFixture();
            const auto group_at = 44 + Monstey_U32(one, 40) + 12 + 8 * 4;
            B two(one.begin(), one.begin() + group_at + 44);
            two.insert(two.end(), one.begin() + group_at, one.begin() + group_at + 44);
            const auto scene = NativeSceneFixture();
            two.insert(two.end(), scene.begin(), scene.end());
            Monstey_PutU32(two, 8, 2);
            Monstey_PutU32(two, 12, 2);
            if (conflicting)
                two[group_at + 44 + 12 + 25] ^= 1;
            else
                Monstey_PutU32(two, group_at + 44 + 12,
                               std::bit_cast<std::uint32_t>(-50.f));
            return two;
        };
        Case("private flattening rejects ambiguous draw prototypes while preserve mode retains groups", [&] {
            const auto raw = duplicate_group_render(true);
            const auto donor = put("conflicting-cull-groups.wldclnt", raw);
            auto a = common_merge_args("merge-conflicting-culls");
            a["merge_sources"][0]["render_source"] = Path(donor);
            a["merge_sources"][0]["render_expected_sha256"] = Monstey_HashSha256(raw);
            Reject([&] { Monstey_WorldCall("sow_world_topology_build", a); });
            Check(!std::filesystem::exists(dir / "merge-conflicting-culls.wldmsh"),
                  "Ambiguous merge prototype published a mesh");
            a = args(mesh);
            a["expected_sha256"] = Monstey_HashSha256(meshraw);
            a["output"] = Path(dir / "private-conflicting.wldmsh");
            a["spec"] = J::object();
            a["render_source"] = Path(donor);
            a["render_expected_sha256"] = Monstey_HashSha256(raw);
            a["render_output"] = Path(dir / "private-conflicting.wldclnt");
            a["source_resource"] = "worlds/source.wldmsh";
            a["target_resource"] = "worlds/source.wldmsh";
            Reject([&] { Monstey_WorldCall("sow_world_topology_build", a); });
            Check(!std::filesystem::exists(dir / "private-conflicting.wldmsh"),
                  "Ambiguous private prototype published an output");
            a["render_mode"] = "preserve-section";
            const auto result = Monstey_WorldCall("sow_world_topology_build", a);
            const auto preserved = Monstey_ReadFile(dir / "private-conflicting.wldclnt");
            const auto old_group = 44 + Monstey_U32(raw, 40) + 12 + 8 * 4;
            const auto new_group = 44 + Monstey_U32(preserved, 40) + 12 + 8 * 4;
            Check(result.at("render").at("groups").size() == 2 &&
                      std::equal(raw.begin() + old_group, raw.begin() + old_group + 88,
                                 preserved.begin() + new_group) &&
                      std::equal(raw.end() - 200, raw.end(), preserved.end() - 200),
                  "Preserve-section discarded legitimate per-group native flags");
        });
        Case("duplicate draw prototypes with different old bounds rebuild from actual geometry", [&] {
            const auto raw = duplicate_group_render(false);
            const auto donor = put("bounds-only-cull-groups.wldclnt", raw);
            auto a = common_merge_args("merge-duplicate-bounds");
            a["merge_sources"][0]["render_source"] = Path(donor);
            a["merge_sources"][0]["render_expected_sha256"] = Monstey_HashSha256(raw);
            const auto result = Monstey_WorldCall("sow_world_topology_build", a);
            const auto &group = result.at("render").at("groups")[0];
            Check(group.at("static_draws").size() == 3 &&
                      group.at("bounds")[2][0].as_number() == 0 &&
                      group.at("bounds")[2][3].as_number() == 1,
                  "Bounds-only duplicates were rejected or propagated an obsolete bound");
        });
        Case("empty render scene needs an explicit qualified template and SHA", [&] {
            auto empty = RenderFixture();
            empty.resize(empty.size() - NativeSceneFixture().size());
            empty.resize(empty.size() + 52);
            const auto source = put("empty-scene-source.wldclnt", empty);
            auto a = args(mesh);
            a["expected_sha256"] = Monstey_HashSha256(meshraw);
            a["output"] = Path(dir / "scene-template.wldmsh");
            a["spec"] = J::object({{"groups", J::array({J::object(
                {{"group_index", 0}, {"draws", J::array({J::object({{"source_draw", 0}})})}})})}});
            a["render_source"] = Path(source);
            a["render_expected_sha256"] = Monstey_HashSha256(empty);
            a["render_output"] = Path(dir / "scene-template.wldclnt");
            a["source_resource"] = "worlds/source.wldmsh";
            a["target_resource"] = "worlds/scene-template.wldmsh";
            a["render_mode"] = "private";
            Reject([&] { Monstey_WorldCall("sow_world_topology_build", a); });
            Check(!std::filesystem::exists(dir / "scene-template.wldmsh") &&
                      !std::filesystem::exists(dir / "scene-template.wldclnt"),
                  "Missing native scene prototype published an incomplete pair");
            a["scene_template"] = Path(render);
            a["scene_expected_sha256"] = std::string(64, '0');
            Reject([&] { Monstey_WorldCall("sow_world_topology_build", a); });
            a["scene_expected_sha256"] = Monstey_HashSha256(RenderFixture());
            const auto result = Monstey_WorldCall("sow_world_topology_build", a);
            Check(result.at("render").at("native_scene").at("group_routing_built").as_bool() &&
                      result.at("render").at("native_scene").at("explicit_scene_template").as_bool(),
                  "Explicit native scene prototype did not reach the writer");
            Check(Monstey_ReadFile(source) == empty &&
                      Monstey_ReadFile(render) == RenderFixture(),
                  "A scene prototype source changed");
        });
        Case("one native material model retains all 25 distinct draw culls", [&] {
            auto many_mesh = put("many.wldmsh", MeshFixture(25));
            auto many_render = put("many.wldclnt", RenderManyFixture(25));
            auto a = args(many_mesh);
            a["expected_sha256"] = Monstey_HashSha256(many_mesh);
            a["render_expected_sha256"] = Monstey_HashSha256(many_render);
            a["output"] = Path(dir / "many-authored.wldmsh");
            a["spec"] = J::object();
            a["render_source"] = Path(many_render);
            a["render_output"] = Path(dir / "many-authored.wldclnt");
            a["source_resource"] = "worlds/source.wldmsh";
            a["target_resource"] = "worlds/many.wldmsh";
            a["render_mode"] = "private";
            Monstey_WorldCall("sow_world_topology_build", a);
            auto graph = Monstey_WorldCall("sow_world_render_inspect", args(dir / "many-authored.wldclnt"));
            Check(graph.at("models").size() == 1 && graph.at("groups").size() == 1 &&
                      graph.at("groups")[0].at("static_draws").size() == 25,
                  "draw ordinals were collapsed into a single model-selected aggregate");
            for (std::size_t i = 0; i < 25; ++i)
                Check(graph.at("groups")[0].at("static_draws")[i].as_uint() == i,
                      "a native draw has no distinct culling submission");
            auto raw = Monstey_ReadFile(dir / "many-authored.wldclnt");
            auto start = 44 + Monstey_U32(raw, 40) + 12 + 8 * 4 + 12;
            for (std::size_t i = 0; i < 25; ++i)
                Check(Monstey_U32(raw, start + 32 * i + 24) == 0xdeadbeef &&
                          Monstey_U16(raw, start + 32 * i + 30) == 0x1234,
                      "opaque native cull bytes were repurposed as model selectors");
        });
        Case("authored eligible static culls form a stable native prefix", [&] {
            auto source_mesh = put("prefix.wldmsh", MeshFixture(4));
            auto source = RenderManyFixture(4);
            auto begin = 44 + Monstey_U32(source, 40) + 12 + 8 * 4 + 12;
            for (std::size_t i = 0; i < 4; ++i) {
                source[begin + 32 * i + 24] = i % 2 == 0 ? 1 : 0;
                source[begin + 32 * i + 25] = static_cast<std::uint8_t>(i + 80);
            }
            auto source_render = put("prefix.wldclnt", source);
            auto a = args(source_mesh);
            a["expected_sha256"] = Monstey_HashSha256(source_mesh);
            a["render_expected_sha256"] = Monstey_HashSha256(source);
            a["spec"] = J::object();
            a["output"] = Path(dir / "prefix-authored.wldmsh");
            a["render_source"] = Path(source_render);
            a["render_output"] = Path(dir / "prefix-authored.wldclnt");
            a["source_resource"] = "worlds/source.wldmsh";
            a["target_resource"] = "worlds/prefix.wldmsh";
            a["render_mode"] = "private";
            auto result = Monstey_WorldCall("sow_world_topology_build", a);
            auto &group = result.at("render").at("groups")[0];
            Check(group.at("eligible_static_prefix_valid").as_bool() &&
                      group.at("eligible_static_prefix_count").as_uint() == 2 &&
                      group.at("static_draws") == J::array({0, 2, 1, 3}),
                  "eligible/ineligible relative order or draw ordinals changed");
            auto output = Monstey_ReadFile(dir / "prefix-authored.wldclnt");
            auto start = 44 + Monstey_U32(output, 40) + 12 + 8 * 4 + 12;
            for (std::size_t i = 0; i < 4; ++i) {
                auto ordinal = Monstey_U16(output, start + 32 * i + 28);
                Check(output[start + 32 * i + 25] == 80 + ordinal &&
                          Monstey_U16(output, start + 32 * i + 30) == 0x1234,
                      "stable prefix sort changed opaque static bytes");
            }
            Check(Monstey_ReadFile(source_render) == source, "prefix authoring changed source");
        });
        Case("authored static counts cannot truncate native uint16 counters", [&] {
            auto too_many = RenderManyFixture(65536);
            auto start = 44 + Monstey_U32(too_many, 40) + 12 + 8 * 4 + 12;
            for (std::size_t i = 0; i < 65536; ++i)
                Monstey_PutU16(too_many, start + 32 * i + 28, 0);
            auto source = put("oversized-prefix.wldclnt", too_many);
            auto a = args(mesh);
            a["spec"] = J::object();
            a["output"] = Path(dir / "oversized-prefix.wldmsh");
            a["expected_sha256"] = Monstey_HashSha256(mesh);
            a["render_expected_sha256"] = Monstey_HashSha256(too_many);
            a["render_source"] = Path(source);
            a["render_output"] = Path(dir / "oversized-prefix-authored.wldclnt");
            a["source_resource"] = "worlds/source.wldmsh";
            a["target_resource"] = "worlds/oversized-prefix.wldmsh";
            a["render_mode"] = "preserve-section";
            Reject([&] { Monstey_WorldCall("sow_world_topology_build", a); });
            Check(!std::filesystem::exists(dir / "oversized-prefix.wldmsh") &&
                      !std::filesystem::exists(dir / "oversized-prefix-authored.wldclnt"),
                  "truncated runtime counter candidate was published");
        });
        Case("changed native LOD draw order rejects before either publication", [&] {
            auto with_lod = put("lod.wldclnt", RenderManyFixture(1, true));
            auto a = args(mesh);
            a["output"] = Path(dir / "lod-changed.wldmsh");
            a["spec"] = J::object(
                {{"groups",
                  J::array({J::object({{"group_index", 0},
                                       {"draws", J::array({J::object({{"source_draw", 0}}),
                                                           J::object({{"source_draw", 0}})})}})})}});
            a["expected_sha256"] = Monstey_HashSha256(mesh);
            a["render_expected_sha256"] = Monstey_HashSha256(with_lod);
            a["render_source"] = Path(with_lod);
            a["render_output"] = Path(dir / "lod-changed.wldclnt");
            a["source_resource"] = "worlds/source.wldmsh";
            a["target_resource"] = "worlds/lod.wldmsh";
            a["render_mode"] = "preserve-section";
            Reject([&] { Monstey_WorldCall("sow_world_topology_build", a); });
            Check(!std::filesystem::exists(dir / "lod-changed.wldmsh") &&
                      !std::filesystem::exists(dir / "lod-changed.wldclnt"),
                  "unqualified LOD edit published half an output");
        });
        Case("existing WDSC scene and prefab data remain exact", [&] {
            auto topology_args = args(mesh);
            topology_args["expected_sha256"] = Monstey_HashSha256(meshraw);
            topology_args["output"] = Path(dir / "preserved-topology.wldmsh");
            topology_args["spec"] = J::object(
                {{"groups",
                  J::array({J::object(
                      {{"group_index", 0}, {"draws", J::array({J::object({{"source_draw", 0}})})}})})}});
            auto topology = Monstey_WorldCall("sow_world_topology_build", topology_args).at("topology");
            auto a =
                J::object({{"mesh_source", Path(dir / "preserved-topology.wldmsh")},
                           {"mesh_expected_sha256", Monstey_HashSha256(dir / "preserved-topology.wldmsh")},
                           {"topology", topology},
                           {"render_source", Path(render)},
                           {"render_expected_sha256", Monstey_HashSha256(RenderFixture())},
                           {"source_resource", "worlds/source.wldmsh"},
                           {"target_resource", "worlds/preserved-target.wldmsh"},
                           {"mode", "preserve-section"},
                           {"output", Path(dir / "preserved-render.wldclnt")}});
            auto result = Monstey_WorldCall("sow_world_render_build", a);
            Check(result.at("render").at("scene_portals_preserved_exact").as_bool() &&
                      result.at("render").at("unrelated_prefab_bindings_preserved").as_bool(),
                  "preserve mode dropped unrelated render data");
            auto before = Monstey_WorldCall("sow_world_render_inspect", args(render)),
                 after =
                     Monstey_WorldCall("sow_world_render_inspect", args(dir / "preserved-render.wldclnt"));
            Check(before.at("scene_suffix_sha256") == after.at("scene_suffix_sha256") &&
                      after.at("groups").size() == 1,
                  "scene suffix or stable native group ordinal changed");
        });
        Case("new topology partitions at native uint16 vertex limit", [&] {
            auto npz = put("large-topology.npz", LargeTopologyNpz());
            auto a = args(mesh);
            a["expected_sha256"] = Monstey_HashSha256(meshraw);
            a["output"] = Path(dir / "partitioned.wldmsh");
            a["spec"] = J::object(
                {{"groups", J::array({J::object(
                                {{"group_index", 0},
                                 {"draws", J::array({J::object(
                                               {{"source_draw", 0}, {"attributes_npz", Path(npz)}})})}})})}});
            auto result = Monstey_WorldCall("sow_world_topology_build", a);
            auto &draws = result.at("topology").at("groups")[0].at("draws");
            Check(draws.size() == 2 && draws[0].at("vertices").as_uint() == 65535 &&
                      draws[1].at("vertices").as_uint() == 3 && draws[0].at("triangles").as_uint() == 21845 &&
                      draws[1].at("triangles").as_uint() == 1,
                  "native partition dropped or reordered triangles");
        });
        Case("paired output preflight never publishes half an edit", [&] {
            auto a = args(mesh);
            a["expected_sha256"] = Monstey_HashSha256(meshraw);
            a["output"] = Path(dir / "must-not-exist-topology.wldmsh");
            a["spec"] = J::object({{"groups", J::array()}});
            a["render_source"] = Path(render);
            a["render_expected_sha256"] = Monstey_HashSha256(RenderFixture());
            a["render_output"] = Path(render);
            a["source_resource"] = "worlds/source.wldmsh";
            a["target_resource"] = "worlds/new.wldmsh";
            Reject([&] { Monstey_WorldCall("sow_world_topology_build", a); });
            Check(!std::filesystem::exists(dir / "must-not-exist-topology.wldmsh") &&
                      Monstey_ReadFile(render) == RenderFixture(),
                  "paired preflight left output or changed source");
        });
        Case("invalid MESH5 triangle and quaternion rejected", [&] {
            auto bad = meshraw;
            Monstey_PutU16(bad, 35 + 12 + 72, 65535);
            Reject([&] { Monstey_WorldCall("sow_worldmesh_inspect", args(put("bad-index.wldmsh", bad))); });
            auto a = args(mesh);
            a["output"] = Path(dir / "bad-quaternion.wldmsh");
            a["spec"] = J::object(
                {{"updates",
                  J::array({J::object(
                      {{"group_index", 0}, {"draw_index", 0}, {"quaternion", J::array({0, 0, 0, 0})}})})}});
            Reject([&] { Monstey_WorldCall("sow_worldmesh_apply", a); });
        });
        Case("WDSC9 empty section and HKAI3 exact envelopes", [&] {
            B wdsc{'W', 'D', 'S', 'C'};
            A32(wdsc, 9);
            wdsc.resize(96);
            auto report = Monstey_WorldCall("sow_section_inspect", args(put("empty.wldclnt", wdsc)));
            Check(report.at("format").as_string() == "WDSC9", "WDSC parse");
            B hkai{'H', 'K', 'A', 'I'};
            A32(hkai, 3);
            A32(hkai, 0xffffd00d);
            A32(hkai, 8);
            hkai.insert(hkai.end(), {0, 0, 0, 8, 'T', 'A', 'G', '0'});
            A32(hkai, 0xffffdead);
            auto nav = put("empty.hkai", hkai);
            auto a = args(nav);
            a["output"] = Path(dir / "identity.hkai");
            Monstey_WorldCall("sow_section_roundtrip", a);
            Check(Monstey_ReadFile(dir / "identity.hkai") == hkai, "HKAI envelope copy differs");
        });
        Case("source and existing outputs remain immutable", [&] {
            auto a = args(obj);
            a["output"] = Path(obj);
            Reject([&] { Monstey_WorldCall("sow_objres_roundtrip", a); });
            Check(Monstey_ReadFile(obj) == objraw, "world source overwritten");
        });
        if (argc > 2) {
            auto root = std::filesystem::path(Monstey_Wide(argv[2]));
            auto native_obj =
                root / L"sow-port/world-objects/fixtures-v001/source/worlds/retail/snowmtn/snowmtn.objres";
            auto native_inst = root / L"sow-port/world-edit/inst-v001/source/prefabs/common/architecture/"
                                      L"fort/keep/keepinterior.inst";
            auto native_mesh = root / L"sow-port/world-edit/section-formats-v001/source/worlds/retail/island/"
                                      L"sectionsandlayers/dynamicfort/keepinterior.wldmsh";
            auto native_wdsc = root / L"sow-port/world-edit/section-formats-v001/source/worlds/retail/island/"
                                      L"sectionsandlayers/dynamicfort/keepinterior.wldclnt";
            auto native_nav = root / L"sow-port/world-edit/section-formats-v001/source/worlds/retail/island/"
                                     L"sectionsandlayers/dynamicfort/keepinterior.hkai";
            Case("native OBJRES complete parse, properties and byte-exact identity", [&] {
                auto before = Monstey_HashSha256(native_obj);
                auto report = Monstey_WorldCall("sow_objres_inspect", args(native_obj));
                auto a = args(native_obj);
                a["output"] = Path(dir / "native-identity.objres");
                Monstey_WorldCall("sow_objres_roundtrip", a);
                Check(Monstey_HashSha256(dir / "native-identity.objres") == before &&
                          Monstey_HashSha256(native_obj) == before,
                      "native OBJRES changed");
                std::cout << "NATIVE_OBJRES " << report.at("objects").as_uint() << " objects " << before
                          << '\n';
            });
            Case("native PRFB8 exact reconstruction", [&] {
                auto before = Monstey_HashSha256(native_inst);
                auto report = Monstey_WorldCall("sow_inst_inspect", args(native_inst));
                auto a = args(native_inst);
                a["output"] = Path(dir / "native-identity.inst");
                Monstey_WorldCall("sow_inst_roundtrip", a);
                Check(Monstey_HashSha256(dir / "native-identity.inst") == before, "native PRFB changed");
                std::cout << "NATIVE_PRFB " << report.at("instances_total").as_uint() << " instances "
                          << before << '\n';
            });
            Case("native MESH5 full geometry and metadata decode", [&] {
                auto report = Monstey_WorldCall("sow_worldmesh_inspect", args(native_mesh));
                Check(report.at("geometry_decoder").as_bool(), "native mesh not decoded");
                auto a = args(native_mesh);
                a["spec"] = J::object({{"updates", J::array()}});
                a["output"] = Path(dir / "native-identity.wldmsh");
                Monstey_WorldCall("sow_worldmesh_apply", a);
                Check(Monstey_HashSha256(dir / "native-identity.wldmsh") == Monstey_HashSha256(native_mesh),
                      "native mesh identity changed");
            });
            Case("native WDSC and HKAI envelopes", [&] {
                Check(Monstey_WorldCall("sow_section_inspect", args(native_wdsc))
                          .at("roundtrip_byte_exact")
                          .as_bool(),
                      "native WDSC envelope");
                Check(Monstey_WorldCall("sow_section_inspect", args(native_nav))
                          .at("roundtrip_byte_exact")
                          .as_bool(),
                      "native HKAI envelope");
            });
        }
        if (argc > 3) {
            auto source = std::filesystem::path(Monstey_Wide(argv[3]));
            Case("real native object collection and indexed terrain scene preparation", [&] {
                auto project = std::filesystem::path(Monstey_Wide(argv[2]));
                auto native =
                    project /
                    "sow-port/world-objects/fixtures-v001/source/worlds/retail/snowmtn/snowmtn.objres";
                auto ground = source.parent_path() / "snowmtn.wldmsh";
                auto before = Monstey_HashSha256(native), before_mesh = Monstey_HashSha256(ground);
                auto spec = J::object(
                    {{"sources",
                      J::array(
                          {J::object({{"id", "native"},
                                      {"source", Path(native)},
                                      {"resource_path", "worlds/retail/snowmtn/snowmtn.objres"},
                                      {"frame", "world"},
                                      {"select", J::object({{"offset", 0},
                                                            {"limit", 1},
                                                            {"class_names", J::array({"TeleportPoint"})}})}}),
                           J::object({{"id", "terrain"},
                                      {"source", Path(ground)},
                                      {"resource_path", "worlds/retail/snowmtn/snowmtn.wldmsh"},
                                      {"frame", "world"},
                                      {"select", J::object({{"indices", J::array({13})}})}})})}});
                auto result =
                    Monstey_WorldCall("sow_world_scene_prepare",
                                      J::object({{"spec", spec}, {"output", Path(dir / "native-scene")}}));
                auto scene = J::Parse(Monstey_ReadText(dir / "native-scene/scene.json"));
                Check(result.at("native_geometry_references").as_uint() == 1 &&
                          scene.at("geometry")[0].at("triangles").as_uint() == 20,
                      "native indexed draw13 subset was lost");
                Check(Monstey_HashSha256(native) == before && Monstey_HashSha256(ground) == before_mesh,
                      "native scene source changed");
            });
            Case("complex shipped WDSC full native render graph", [&] {
                auto before = Monstey_HashSha256(source);
                auto report = Monstey_WorldCall("sow_world_render_inspect", args(source));
                Check(report.at("roundtrip_byte_exact").as_bool() && !report.at("models").empty(),
                      "complex native WDSC did not decode its material models");
                Check(Monstey_HashSha256(source) == before, "complex WDSC source changed");
                auto paired = source.parent_path() / "snowmtn.wldmsh";
                if (std::filesystem::exists(paired)) {
                    auto geometry = Monstey_WorldCall("sow_worldmesh_inspect", args(paired));
                    Check(geometry.at("groups").size() == 1,
                          "Native paired MESH does not have one common geometry context");
                    auto &draws = geometry.at("groups")[0].at("draws");
                    std::size_t checked{};
                    for (auto &group : report.at("groups").as_array())
                        for (std::size_t index = 0; index < group.at("static_draws").size(); ++index) {
                            auto ordinal = group.at("static_draws")[index].as_uint();
                            Check(ordinal < draws.size(),
                                  "Native culling ordinal outside actual mesh draw list");
                            auto &bound = group.at("bounds")[index];
                            for (std::size_t axis = 0; axis < 3; ++axis) {
                                Check(std::abs(bound[axis].as_number() -
                                               draws[ordinal].at("bounds_min")[axis].as_number()) < 0.01,
                                      "Native per-draw cull minimum differs from actual vertices");
                                Check(std::abs(bound[axis + 3].as_number() -
                                               draws[ordinal].at("bounds_max")[axis].as_number()) < 0.01,
                                      "Native per-draw cull maximum differs from actual vertices");
                            }
                            ++checked;
                        }
                    auto a = args(paired);
                    a["expected_sha256"] = Monstey_HashSha256(paired);
                    a["render_expected_sha256"] = Monstey_HashSha256(source);
                    a["spec"] = J::object();
                    a["output"] = Path(dir / "native-full-identity.wldmsh");
                    a["render_source"] = Path(source);
                    a["render_output"] = Path(dir / "native-full-identity.wldclnt");
                    a["source_resource"] = "worlds/retail/snowmtn/snowmtn.wldmsh";
                    a["target_resource"] = a["source_resource"];
                    a["render_mode"] = "preserve-section";
                    Monstey_WorldCall("sow_world_topology_build", a);
                    Check(Monstey_ReadFile(paired) == Monstey_ReadFile(dir / "native-full-identity.wldmsh") &&
                              Monstey_ReadFile(source) ==
                                  Monstey_ReadFile(dir / "native-full-identity.wldclnt"),
                          "full native paired topology/render identity authoring differs byte for byte");
                    J::Array authored_draws;
                    for (std::size_t ordinal = 0; ordinal < 25; ++ordinal)
                        authored_draws.push_back(
                            J::object({{"source_draw", static_cast<std::uint64_t>(ordinal)}}));
                    authored_draws.push_back(J::object({{"source_draw", 13}}));
                    a["spec"] = J::object(
                        {{"groups", J::array({J::object({{"group_index", 0}, {"draws", authored_draws}})})}});
                    a["output"] = Path(dir / "native-full-26draw.wldmsh");
                    a["render_output"] = Path(dir / "native-full-26draw.wldclnt");
                    a["target_resource"] = "worlds/custom/native-26draw.wldmsh";
                    a["render_mode"] = "private";
                    auto changed = Monstey_WorldCall("sow_world_topology_build", a);
                    Check(changed.at("render").at("all_native_draws_have_cull_record").as_bool() &&
                              changed.at("render").at("groups")[0].at("static_draws").size() == 26 &&
                              changed.at("topology").at("groups")[0].at("draws").size() == 26,
                          "real native topology change omitted an indexed draw or culling record");
                    auto &prefix = changed.at("render").at("groups")[0];
                    Check(prefix.at("eligible_static_prefix_valid").as_bool() &&
                              prefix.at("eligible_static_prefix_count").as_uint() == 17 &&
                              prefix.at("static_draws")[16].as_uint() == 25 &&
                              prefix.at("static_draws")[17].as_uint() == 16,
                          "native appended draw13 is omitted from the eligible static prefix");
                    Check(Monstey_HashSha256(source) == before,
                          "changed native topology authoring modified source scene");
                    Check(checked == 25 && draws.size() == 25,
                          "Native 25 draw-association counterexample not covered");
                }
                std::cout << "NATIVE_COMPLEX_WDSC " << report.at("models").size() << " models "
                          << report.at("groups").size() << " groups " << before << '\n';
            });
        }
        if (argc > 4) {
            Case("distinct native region contexts compose with complete indexed WDSC routing", [&] {
                const auto first = std::filesystem::path(Monstey_Wide(argv[3]));
                const auto second = std::filesystem::path(Monstey_Wide(argv[4]));
                auto first_mesh = first; first_mesh.replace_extension(L".wldmsh");
                auto second_mesh = second; second_mesh.replace_extension(L".wldmsh");
                const auto first_render = Monstey_WorldCall("sow_world_render_inspect", args(first));
                const auto second_render = Monstey_WorldCall("sow_world_render_inspect", args(second));
                const auto first_sha = Monstey_HashSha256(first_mesh);
                const auto second_sha = Monstey_HashSha256(second_mesh);
                auto a = args(first_mesh);
                a["expected_sha256"] = first_sha;
                a["spec"] = J::object();
                a["render_source"] = Path(first);
                a["render_expected_sha256"] = Monstey_HashSha256(first);
                a["source_resource"] = first_render.at("models")[0].at("path");
                a["target_resource"] = "worlds/custom/native-common-context.wldmsh";
                a["output"] = Path(dir / "native-merged.wldmsh");
                a["render_output"] = Path(dir / "native-merged.wldclnt");
                a["report"] = Path(dir / "native-merged.json");
                a["merge_sources"] = J::array({J::object({
                    {"id", "second-region"}, {"mesh_source", Path(second_mesh)},
                    {"mesh_expected_sha256", second_sha}, {"render_source", Path(second)},
                    {"render_expected_sha256", Monstey_HashSha256(second)},
                    {"source_resource", second_render.at("models")[0].at("path")}
                })});
                const auto result = Monstey_WorldCall("sow_world_topology_build", a);
                const auto &renderer = result.at("render");
                const auto contexts = Monstey_WorldMeshReadContexts(
                    Monstey_ReadFile(dir / "native-merged.wldmsh"));
                const auto context = static_cast<std::size_t>(
                    result.at("merge").at("target_context").as_uint());
                const auto expected_draws = result.at("merge").at("draw_remap").size();
                Check(expected_draws == 43 && contexts.groups[context].draws.size() == 43 &&
                          renderer.at("groups")[0].at("static_draws").size() == 43 &&
                          renderer.at("models")[0].at("materials").size() == 39 &&
                          renderer.at("native_scene").at("group_routing_built").as_bool() &&
                          renderer.at("eligible_static_prefix_qualified").as_bool() &&
                          result.at("merge").at("encoded_channels_byte_exact").as_bool(),
                      "Actual Snow/Minas geometry/material/cull/spatial associations differ");
                Check(Monstey_HashSha256(first_mesh) == first_sha &&
                          Monstey_HashSha256(second_mesh) == second_sha &&
                          Monstey_HashSha256(first) == a.at("render_expected_sha256").as_string() &&
                          Monstey_HashSha256(second) ==
                              a.at("merge_sources")[0].at("render_expected_sha256").as_string(),
                      "Actual native merge source changed");
                std::cout << "NATIVE_COMMON_CONTEXT " << expected_draws << " draws39materials "
                          << Monstey_HashSha256(dir / "native-merged.wldmsh") << '\n';
            });
        }
        std::cout << "RESULT " << passed << " world test groups passed; outputs " << dir.string() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
