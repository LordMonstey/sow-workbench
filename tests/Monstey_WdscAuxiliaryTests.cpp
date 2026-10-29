#include "Monstey_WdscAuxiliary.hpp"
#include <array>
#include <bit>
#include <chrono>
#include <functional>
#include <iostream>
#include <set>
using namespace Monstey;
namespace
{
using B = Monstey_Bytes;
using J = Monstey_Json;
int Monstey_Passed = 0;
void Monstey_Check(bool value, std::string_view message)
{
    Monstey_Require(value, message);
    ++Monstey_Passed;
    std::cout << "PASS " << message << '\n';
}
void Monstey_Reject(const std::function<void()> &action, std::string_view message)
{
    try
    {
        action();
    }
    catch (const std::exception &)
    {
        Monstey_Check(true, message);
        return;
    }
    throw std::runtime_error("Expected rejection: " + std::string(message));
}
struct Monstey_Fixture
{
    B bytes;
    std::size_t descriptor{}, group{}, polygon{}, owner{};
};
Monstey_Fixture Monstey_MakeFixture()
{
    Monstey_Fixture result;
    auto &raw = result.bytes;
    raw.resize(44);
    raw[0] = 'W';
    raw[1] = 'D';
    raw[2] = 'S';
    raw[3] = 'C';
    Monstey_PutU32(raw, 4, 9);
    B pool;
    auto name = [&](std::string_view text) {
        const auto offset = static_cast<std::uint32_t>(pool.size());
        pool.insert(pool.end(), text.begin(), text.end());
        pool.push_back(0);
        return offset;
    };
    const auto path = name("synthetic\\room.wldmsh");
    const auto material = name("synthetic\\flat.mat");
    const std::array<std::uint32_t, 9> counts{1, 2, 1, 1, 1, 1, 1, 4, static_cast<std::uint32_t>(pool.size())};
    for (std::size_t i = 0; i < counts.size(); ++i)
        Monstey_PutU32(raw, 8 + 4 * i, counts[i]);
    raw.insert(raw.end(), pool.begin(), pool.end());
    result.descriptor = raw.size();
    for (auto word : {1u, path, 0u, material})
        Monstey_AppendU32(raw, word);
    raw.resize(raw.size() + 76);
    result.group = raw.size();
    for (auto word : {2u, 1u, 1u})
        Monstey_AppendU32(raw, word);
    for (std::uint16_t draw = 0; draw < 2; ++draw)
    {
        for (auto value : {-1.f, -2.f, -3.f, 4.f, 5.f, 6.f})
            Monstey_AppendU32(raw, std::bit_cast<std::uint32_t>(value));
        Monstey_AppendU32(raw, 1);
        Monstey_AppendU16(raw, draw);
        Monstey_AppendU16(raw, 0);
    }
    result.polygon = raw.size();
    raw.push_back(4);
    for (auto value : {-1.25f, 2.f, 3.5f, 4.5f, 2.f, 3.5f, 4.5f, 2.f, 8.25f, -1.25f, 2.f, 8.25f})
        Monstey_AppendU32(raw, std::bit_cast<std::uint32_t>(value));
    result.owner = raw.size();
    for (auto word : {123u, 1u, 0u})
        Monstey_AppendU32(raw, word);
    raw.resize(raw.size() + 52);
    return result;
}
void Monstey_Synthetic(const std::filesystem::path &output)
{
    auto fixture = Monstey_MakeFixture();
    const auto &raw = fixture.bytes;
    const auto hash = Monstey_HashSha256(raw);
    const auto report = Monstey_WdscAuxiliaryInspect(raw);
    const auto &group = report.at("groups")[0];
    const auto &polygon = group.at("auxiliary_polygon_arrays")[0];
    Monstey_Check(report.at("header_words")[0].as_uint() == 1 && group.at("ordinal").as_uint() == 0,
                  "native group cardinality");
    Monstey_Check(group.at("static_records").size() == 2 &&
                      group.at("static_records")[1].at("draw_ordinal_u16_28").as_uint() == 1,
                  "independent draw ordinals");
    Monstey_Check(polygon.at("offset").as_uint() == fixture.polygon && polygon.at("payload_bytes").as_uint() == 48,
                  "exact polygon offsets and extent");
    Monstey_Check(polygon.at("point_count_u8").as_uint() == 4 && polygon.at("points").size() == 4,
                  "U8 count with 12-byte points");
    Monstey_Check(polygon.at("points")[0].at("xyz")[0].as_number() == -1.25 &&
                      polygon.at("points")[2].at("xyz")[2].as_number() == 8.25,
                  "exact finite float32 values");
    Monstey_Check(polygon.at("points")[1].at("offset").as_uint() == fixture.polygon + 13, "point stride 12");
    Monstey_Check(polygon.at("all_xyz_finite").as_bool(), "finite native-style point payload");
    Monstey_Check(group.at("owner_groups")[0].at("binding_ordinals")[0].as_uint() == 0, "owner table is independent");
    Monstey_Check(Monstey_HashSha256(raw) == hash, "memory input remains byte exact");
    Monstey_Check(!report.at("polygon_render_semantics_qualified").as_bool() &&
                      !report.at("draw_reference_semantics_qualified").as_bool(),
                  "no invented runtime semantics");
    auto malformed = raw;
    malformed[0] = 0;
    Monstey_Reject([&] { (void)Monstey_WdscAuxiliaryInspect(malformed); }, "invalid magic");
    malformed = raw;
    Monstey_PutU32(malformed, 4, 8);
    Monstey_Reject([&] { (void)Monstey_WdscAuxiliaryInspect(malformed); }, "invalid version");
    malformed = raw;
    malformed.resize(95);
    Monstey_Reject([&] { (void)Monstey_WdscAuxiliaryInspect(malformed); }, "short header extent");
    malformed = raw;
    Monstey_PutU32(malformed, 40, 0xffffffffu);
    Monstey_Reject([&] { (void)Monstey_WdscAuxiliaryInspect(malformed); }, "dictionary overflow");
    malformed = raw;
    Monstey_PutU32(malformed, fixture.descriptor + 4, 0xffffffffu);
    Monstey_Reject([&] { (void)Monstey_WdscAuxiliaryInspect(malformed); }, "path offset bounds");
    malformed = raw;
    Monstey_PutU32(malformed, 28, 99);
    Monstey_Reject([&] { (void)Monstey_WdscAuxiliaryInspect(malformed); }, "material allocation total");
    malformed = raw;
    Monstey_PutU32(malformed, fixture.group, 0xffffffffu);
    Monstey_Reject([&] { (void)Monstey_WdscAuxiliaryInspect(malformed); }, "static count extent");
    malformed = raw;
    Monstey_PutU32(malformed, fixture.group + 4, 200001);
    Monstey_Reject([&] { (void)Monstey_WdscAuxiliaryInspect(malformed); }, "metadata row budget");
    malformed = raw;
    malformed[fixture.polygon] = 255;
    Monstey_Reject([&] { (void)Monstey_WdscAuxiliaryInspect(malformed); }, "point count extent");
    malformed = raw;
    Monstey_PutU32(malformed, 36, 3);
    Monstey_Reject([&] { (void)Monstey_WdscAuxiliaryInspect(malformed); }, "point allocation total");
    malformed = raw;
    Monstey_PutU32(malformed, fixture.owner + 8, 1);
    Monstey_Reject([&] { (void)Monstey_WdscAuxiliaryInspect(malformed); }, "owner ordinal bounds");
    malformed = raw;
    malformed.pop_back();
    Monstey_Reject([&] { (void)Monstey_WdscAuxiliaryInspect(malformed); }, "scene header extent");
    malformed = raw;
    Monstey_PutU32(malformed, fixture.polygon + 1, 0x7fc01234u);
    auto special = Monstey_WdscAuxiliaryInspect(malformed).at("groups")[0].at("auxiliary_polygon_arrays")[0];
    Monstey_Check(!special.at("all_xyz_finite").as_bool() && special.at("points")[0].at("xyz")[0].is_null() &&
                      special.at("points")[0].at("raw_words")[0].as_uint() == 0x7fc01234u,
                  "nonfinite values retain exact bits");
    malformed = raw;
    Monstey_PutU32(malformed, fixture.polygon + 1, 0x80000000u);
    special = Monstey_WdscAuxiliaryInspect(malformed).at("groups")[0].at("auxiliary_polygon_arrays")[0];
    Monstey_Check(special.at("points")[0].at("raw_words")[0].as_uint() == 0x80000000u, "negative zero bits retained");
    B empty(96);
    empty[0] = 'W';
    empty[1] = 'D';
    empty[2] = 'S';
    empty[3] = 'C';
    Monstey_PutU32(empty, 4, 9);
    Monstey_Check(Monstey_WdscAuxiliaryInspect(empty).at("groups").empty(), "empty native envelope");
    const auto source = output / L"synthetic.wldclnt";
    Monstey_WriteNewFile(source, raw);
    Monstey_Check(Monstey_WdscAuxiliaryInspectFile(source).at("source_unchanged").as_bool(),
                  "file source identity checked");
    const auto target = output / L"synthetic-report.json";
    Monstey_WriteNewText(target, report.dump(2));
    const auto receipt_hash = Monstey_HashSha256(target);
    Monstey_Reject([&] { Monstey_WriteNewText(target, "invalid overwrite"); }, "new-only report publication");
    Monstey_Check(Monstey_HashSha256(target) == receipt_hash, "existing report unchanged");
}
void Monstey_Native(const std::filesystem::path &source, const std::filesystem::path &output)
{
    auto report = Monstey_WdscAuxiliaryInspectFile(source);
    Monstey_Check(report.at("source_sha256").as_string() ==
                      "3604d2a9c0465d54903be1e79182c3b27246e504651a598bdfc85004b40601c0",
                  "original Snowmtn source identity");
    const auto &groups = report.at("groups");
    Monstey_Check(groups.size() == 1 && groups[0].at("static_records").size() == 25,
                  "original one group and 25 static draws");
    Monstey_Check(report.at("models").size() == 1 && report.at("models")[0].at("geometry_group").as_uint() == 0,
                  "original common geometry descriptor");
    const auto &arrays = groups[0].at("auxiliary_polygon_arrays");
    Monstey_Check(arrays.size() == 5, "original five auxiliary arrays");
    std::set<std::pair<std::size_t, double>> planes;
    std::size_t points = 0;
    for (const auto &polygon : arrays.as_array())
    {
        Monstey_Check(polygon.at("point_count_u8").as_uint() == 4 && polygon.at("all_xyz_finite").as_bool(),
                      "original finite four-point array");
        const auto &vertices = polygon.at("points");
        std::size_t constant_count = 0;
        std::size_t constant_axis = 0;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            const auto fixed = vertices[0].at("xyz")[axis].as_number();
            bool same = true;
            for (const auto &vertex : vertices.as_array())
                same = same && vertex.at("xyz")[axis].as_number() == fixed;
            if (same)
            {
                ++constant_count;
                constant_axis = axis;
                planes.insert({axis, fixed});
            }
        }
        Monstey_Check(constant_count == 1, "original one constant plane axis per quad");
        std::array<double, 3> a{}, b{};
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            a[axis] = vertices[1].at("xyz")[axis].as_number() - vertices[0].at("xyz")[axis].as_number();
            b[axis] = vertices[2].at("xyz")[axis].as_number() - vertices[1].at("xyz")[axis].as_number();
        }
        const std::array<double, 3> normal{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                                           a[0] * b[1] - a[1] * b[0]};
        Monstey_Check(normal[constant_axis] != 0, "original nondegenerate quad winding");
        points += vertices.size();
    }
    Monstey_Check(points == 20 && planes.size() == 5 && !planes.contains({1, -22400.0}) &&
                      planes.contains({1, 77000.0}),
                  "original box boundary without lower Y face");
    Monstey_Check(groups[0].at("offset").as_uint() == 376783 && arrays[0].at("offset").as_uint() == 377595 &&
                      arrays[4].at("offset").as_uint() == 377791 && report.at("scene_offset").as_uint() == 396976,
                  "original exact prefix byte offsets");
    Monstey_WriteNewText(output / L"native-auxiliary-proof.json", report.dump(2));
    Monstey_Check(Monstey_HashSha256(source) == report.at("source_sha256").as_string(),
                  "original source unchanged after publication");
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        Monstey_Require(argc >= 2 && argc <= 3, "Usage: Monstey_WdscAuxiliaryTests NEW-OUTPUT-PARENT [NATIVE-WDSC]");
        const auto nonce = std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto output = std::filesystem::path(argv[1]) / nonce;
        Monstey_RequireOfflineOutput(output);
        std::filesystem::create_directories(Monstey_WindowsFilePath(output));
        Monstey_Synthetic(output);
        if (argc == 3)
            Monstey_Native(argv[2], output);
        Monstey_WriteNewText(
            output / L"receipt.json",
            J::object({{"checks", Monstey_Passed}, {"native_source_checked", argc == 3}, {"game_files_written", false}})
                .dump(2));
        std::cout << "PASS TOTAL " << Monstey_Passed << " OUTPUT " << Monstey_Utf8(output.wstring()) << '\n';
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}