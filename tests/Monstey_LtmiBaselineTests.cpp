#include "Monstey_LtmiMaterial.hpp"
#include <algorithm>
#include <bit>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace
{
using B = Monstey_Bytes;
using J = Monstey_Json;
void Monstey_Check(bool b, const char *s)
{
    if (!b)
        throw std::runtime_error(s);
}
B Monstey_Text(std::string_view text, bool nul = false)
{
    B out(text.begin(), text.end());
    if (nul)
        out.push_back(0);
    return out;
}
B Monstey_Floats(std::initializer_list<float> values)
{
    B out;
    for (auto value : values)
        Monstey_AppendU32(out, std::bit_cast<std::uint32_t>(value));
    return out;
}
struct Monstey_Fixture
{
    B bytes;
    std::size_t metadata;
    std::vector<std::size_t> offsets;
};
Monstey_Fixture Monstey_Make(std::uint32_t version = 8)
{
    const B shader = Monstey_Text("shaders\\fixture.fxo");
    B values(256, 0xcc);
    auto put = [&](std::size_t at, const B &v) {
        std::copy(v.begin(), v.end(), values.begin() + static_cast<std::ptrdiff_t>(at));
    };
    put(0, Monstey_Text("Default", true));
    put(16, Monstey_Floats({0.5F}));
    put(20, Monstey_Floats({1, 2}));
    put(32, Monstey_Text("characters\\source.tex", true));
    Monstey_PutU32(values, 64, 0x100);
    Monstey_PutU32(values, 68, 0xffffffffU);
    for (unsigned i = 0; i < 16; ++i)
        Monstey_PutU32(values, 80 + 4 * i, std::bit_cast<std::uint32_t>(float(i)));
    put(144, Monstey_Floats({3, 4, 5}));
    put(160, Monstey_Floats({6, 7, 8, 9}));
    for (unsigned i = 0; i < 12; ++i)
        Monstey_PutU32(values, 176 + 4 * i, std::bit_cast<std::uint32_t>(float(30 + i)));
    struct Row
    {
        std::uint16_t type;
        std::uint8_t a, b;
        const char *name;
        std::uint32_t offset;
    };
    const std::array<Row, 10> rows{{{8, 0, 0, "SurfaceRecord", 0},
                                    {5, 0, 0, "Scalar", 16},
                                    {11, 0, 0, "Vector2", 20},
                                    {1, 1, 3, "tDiffuseMap", 32},
                                    {6, 0, 4, "Bool", 64},
                                    {4, 0, 0, "Word", 68},
                                    {9, 0, 0, "Matrix4", 80},
                                    {2, 0, 0, "Vector3", 144},
                                    {3, 0, 0, "Vector4", 160},
                                    {10, 0, 0, "Matrix3x4", 176}}};
    B out(30);
    Monstey_PutU32(out, 0, 0x494d544c);
    Monstey_PutU32(out, 4, version);
    Monstey_PutU32(out, 8, 0x12345678);
    Monstey_PutU32(out, 12, 256);
    Monstey_PutU16(out, 16, 3);
    Monstey_PutU16(out, 18, 7);
    Monstey_PutU32(out, 20, 0xdeadbeefU);
    Monstey_PutU32(out, 24, 0x718293a4);
    Monstey_PutU16(out, 28, static_cast<std::uint16_t>(shader.size()));
    out.insert(out.end(), shader.begin(), shader.end());
    out.insert(out.end(), values.begin(), values.end());
    if (version == 7)
    {
        Monstey_AppendU32(out, 3);
        out.insert(out.end(), {'L', 'E', 'G'});
    }
    Monstey_Fixture result;
    result.metadata = out.size();
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const auto &row = rows[i];
        const auto name = Monstey_Text(row.name);
        Monstey_AppendU16(out, row.type);
        out.push_back(row.a);
        out.push_back(row.b);
        Monstey_AppendU16(out, static_cast<std::uint16_t>(name.size()));
        out.insert(out.end(), name.begin(), name.end());
        result.offsets.push_back(out.size());
        Monstey_AppendU32(out, row.offset);
        if (version == 7)
            Monstey_AppendU16(out, static_cast<std::uint16_t>(0xa000 + i));
    }
    out.insert(out.end(), {0xee, 0, 0xff});
    result.bytes = std::move(out);
    return result;
}
void Monstey_Reject(const std::function<void()> &body)
{
    bool rejected = false;
    try
    {
        body();
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    Monstey_Check(rejected, "Expected native-profile rejection");
}
std::uint32_t Monstey_AsciiHash(std::string_view s)
{
    std::uint32_t h = 0;
    for (auto c : s)
    {
        auto v = static_cast<unsigned char>(c);
        if (v >= 'A' && v <= 'Z')
            v = static_cast<unsigned char>(v - 'A' + 1);
        else if (v >= 'a' && v <= 'z')
            v = static_cast<unsigned char>(v - 'a' + 1);
        else
            throw std::runtime_error("Independent hash fixture is letters only");
        h = h * 0x397U + v;
    }
    return h;
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        Monstey_Check(argc == 2, "Usage LtmiTests NEW-directory");
        const std::filesystem::path dir = argv[1];
        Monstey_Check(!std::filesystem::exists(dir), "New output directory required");
        std::filesystem::create_directories(dir);
        auto tests = J::array();
        unsigned failed = 0;
        auto test = [&](const char *name, const std::function<void()> &fn) {
            try
            {
                fn();
                tests.as_array().push_back(J::object({{"name", name}, {"passed", true}}));
            }
            catch (const std::exception &e)
            {
                ++failed;
                tests.as_array().push_back(J::object({{"name", name}, {"passed", false}, {"error", e.what()}}));
            }
        };
        test("independent_LTMI8_complete_byte_exact_rebuild", [&] {
            auto f = Monstey_Make();
            auto m = Monstey_LtmiRead(f.bytes);
            Monstey_Check(Monstey_LtmiBuild(m) == f.bytes && m.tables[0].size() == 3 && m.tables[1].size() == 7 &&
                              m.unknown_20 == 0xdeadbeefU && m.trailer == B({0xee, 0, 0xff}),
                          "Version8 fields or raw bytes changed");
            Monstey_WriteNewFile(dir / L"fixture8.mat", f.bytes);
        });
        test("LTMI7_legacy_blob_and_two_byte_row_suffix", [&] {
            auto f = Monstey_Make(7);
            auto m = Monstey_LtmiRead(f.bytes);
            Monstey_Check(m.legacy_blob == Monstey_Text("LEG") && m.tables[1][0].legacy_word == 0xa003 &&
                              Monstey_LtmiBuild(m) == f.bytes,
                          "Version7 grammar differs");
            auto next = Monstey_Floats({0.75F});
            Monstey_LtmiSetValue(m, 0, 1, next);
            auto n = Monstey_LtmiRead(Monstey_LtmiBuild(m));
            Monstey_Check(n.legacy_blob == m.legacy_blob && n.tables[1][0].legacy_word == 0xa003 &&
                              n.trailer == m.trailer,
                          "Version7 edit lost opaque state");
            Monstey_WriteNewFile(dir / L"fixture7.mat", f.bytes);
        });
        test("separate_type_and_both_flag_bytes", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            const auto &p = m.tables[1][0];
            Monstey_Check(p.type == 1 && p.flag_a == 1 && p.flag_b == 3 && Monstey_LtmiNativeSize(p.type) == 16,
                          "PackedU32 kind incorrectly used as native type");
        });
        test("native_hash_uses_type_byte_and_name_before_NUL", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            auto p = m.tables[0][1];
            const auto expected = (Monstey_AsciiHash("Scalar") & 0x0fffffffU) | 0x50000000U;
            Monstey_Check(Monstey_LtmiPropertyHash(p) == expected, "Hash arithmetic differs");
            p.flag_a = 200;
            p.flag_b = 255;
            Monstey_Check(Monstey_LtmiPropertyHash(p) == expected, "Flags affected type/name identity");
            p.name = Monstey_Text(std::string_view("SCALAR\0tail", 11));
            p.type = 0x105;
            Monstey_Check(Monstey_LtmiPropertyHash(p) == expected, "Native CL/C-string semantics differ");
        });
        test("all24_initialized_native_size_slots_and_outside_profile", [&] {
            constexpr std::array<int, 24> widths{0,  16, 12, 16, 4,  4, 4,  0, 0, 64, 48, 8,
                                                 0, 8,  8,  8,  0, 8, 12, 16, 4, 8,  12, 16};
            for (unsigned i = 0; i < 256; ++i)
            {
                auto value = Monstey_LtmiNativeSize(static_cast<std::uint16_t>(i));
                if (i < widths.size())
                    Monstey_Check(value && *value == widths[i], "Initialized native slot differs");
                else
                    Monstey_Check(!value, "Out-of-table native width fabricated");
            }
            Monstey_Check(Monstey_LtmiNativeSize(0x105) == 4, "Native size does not consume CL");
        });
        test("SurfaceRecord_string_is_separate_from_zero_runtime_copy", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            Monstey_Check(Monstey_LtmiNativeSize(8) == 0 &&
                              Monstey_LtmiValue(m, m.tables[0][0]) == Monstey_Text("Default", true),
                          "File string/native size conflated");
        });
        test("native_U16_offset_truncation_is_preserved_not_silently_fixed", [&] {
            auto f = Monstey_Make();
            Monstey_PutU32(f.bytes, f.offsets[1], 0x10010);
            auto m = Monstey_LtmiRead(f.bytes);
            Monstey_Check(m.tables[0][1].offset == 0x10010 &&
                              Monstey_LtmiValue(m, m.tables[0][1]) == Monstey_Floats({0.5F}) &&
                              Monstey_LtmiBuild(m) == f.bytes,
                          "Stored/effective offset conflated");
            Monstey_LtmiRename(m, 0, 1, Monstey_Text("Renamed"));
            auto n = Monstey_LtmiRead(Monstey_LtmiBuild(m));
            Monstey_Check(n.tables[0][1].offset == 0x10010, "Unrelated edit rewrote wire offset");
        });
        test("numeric_set_reuses_exclusive_span_without_altering_other_bytes", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            const auto before = m.values;
            const auto texture = m.tables[1][0];
            Monstey_LtmiSetValue(m, 0, 1, Monstey_Floats({0.75F}));
            auto n = Monstey_LtmiRead(Monstey_LtmiBuild(m));
            Monstey_Check(n.tables[0][1].offset == 16 && n.values.size() == before.size() &&
                              Monstey_LtmiValue(n, n.tables[0][1]) == Monstey_Floats({0.75F}) &&
                              std::equal(before.begin(), before.begin() + 16, n.values.begin()) &&
                              std::equal(before.begin() + 20, before.end(), n.values.begin() + 20) &&
                              Monstey_LtmiValue(n, n.tables[1][0]) == Monstey_LtmiValue(m, texture),
                          "Changed scalar damaged opaque bytes or neighboring texture");
        });
        test("identical_set_is_a_true_noop", [&] {
            auto f = Monstey_Make();
            auto m = Monstey_LtmiRead(f.bytes);
            Monstey_LtmiSetValue(m, 0, 1, Monstey_Floats({0.5F}));
            Monstey_Check(Monstey_LtmiBuild(m) == f.bytes, "Identical assignment appended data");
        });
        test("longer_texture_path_and_zero_runtime_string_edits", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            auto texture = Monstey_Text("characters\\replacement\\longer_texture.tex", true);
            Monstey_LtmiSetValue(m, 1, 0, texture);
            Monstey_LtmiSetValue(m, 0, 0, Monstey_Text("Stone", true));
            auto n = Monstey_LtmiRead(Monstey_LtmiBuild(m));
            Monstey_Check(Monstey_LtmiValue(n, n.tables[1][0]) == texture &&
                              Monstey_LtmiValue(n, n.tables[0][0]) == Monstey_Text("Stone", true) &&
                              n.tables[1][0].flag_b == 3,
                          "Resource/string edit failed");
        });
        test("CRUD_retains_table_flags_and_other_properties", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make(7).bytes);
            Monstey_LtmiProperty p;
            p.type = 5;
            p.name = Monstey_Text("NewParameter");
            p.flag_a = 7;
            p.flag_b = 9;
            p.legacy_word = 0xbeef;
            Monstey_LtmiAdd(m, 0, p, Monstey_Floats({2.5F}));
            Monstey_Check(m.tables[0].size() == 4 && m.tables[0][3].legacy_word == 0xbeef, "Add lost metadata");
            Monstey_LtmiRename(m, 1, 1, Monstey_Text("RenamedWord"));
            Monstey_LtmiRemove(m, 0, 0);
            auto n = Monstey_LtmiRead(Monstey_LtmiBuild(m));
            Monstey_Check(n.tables[0].size() == 3 && n.tables[1].size() == 7 && n.tables[0][2].flag_a == 7 &&
                              n.tables[0][2].flag_b == 9 && n.legacy_blob == m.legacy_blob,
                          "CRUD changed unrelated state");
        });
        test("max128_parameters_and_transactional_overflow", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            while (m.tables[0].size() + m.tables[1].size() < 128)
            {
                Monstey_LtmiProperty p;
                p.type = 5;
                p.name = Monstey_Text("P" + std::to_string(m.tables[0].size()));
                Monstey_LtmiAdd(m, 0, p, Monstey_Floats({1}));
            }
            const auto bytes = Monstey_LtmiBuild(m);
            Monstey_LtmiProperty p;
            p.type = 5;
            p.name = Monstey_Text("TooMany");
            Monstey_Reject([&] { Monstey_LtmiAdd(m, 1, p, Monstey_Floats({1})); });
            Monstey_Check(Monstey_LtmiBuild(m) == bytes, "Rejected count changed document");
        });
        test("property_name255_allowed256_rejected_transactionally", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            B name(255, 'X');
            Monstey_LtmiRename(m, 0, 1, name);
            const auto before = Monstey_LtmiBuild(m);
            name.push_back('X');
            Monstey_Reject([&] { Monstey_LtmiRename(m, 0, 1, name); });
            Monstey_Check(Monstey_LtmiBuild(m) == before, "Oversize name corrupted document");
        });
        test("shader_reader_truncation_reported_and_raw_bytes_preserved", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            m.shader_name = B(300, 'S');
            const auto encoded = Monstey_LtmiBuild(m);
            auto n = Monstey_LtmiRead(encoded);
            auto report = Monstey_LtmiDescribe(n);
            Monstey_Check(report["native_shader_name_truncated"].as_bool() &&
                              report["native_shader_name"].as_string().size() == 259 && Monstey_LtmiBuild(n) == encoded,
                          "Native shader name cap was hidden or rewrote source");
        });
        test("wrong_width_string_NUL_and_unsupported_type_rejections_leave_state", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            const auto before = Monstey_LtmiBuild(m);
            Monstey_Reject([&] { Monstey_LtmiSetValue(m, 0, 1, B{1, 2}); });
            Monstey_Reject([&] { Monstey_LtmiSetValue(m, 1, 0, Monstey_Text("NoTerminator")); });
            Monstey_Reject([&] { Monstey_LtmiSetValue(m, 1, 0, B{'a', 0, 'b', 0}); });
            Monstey_LtmiProperty p;
            p.type = 7;
            p.name = Monstey_Text("Unknown");
            Monstey_Reject([&] { Monstey_LtmiAdd(m, 1, p, B{0, 0, 0, 0}); });
            Monstey_Check(Monstey_LtmiBuild(m) == before, "Bad payload changed state");
        });
        test("U16_append_limit_does_not_wrap_or_mutate", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            m.values.resize(65536, 0x31);
            auto alias = m.tables[0][1];
            alias.name = Monstey_Text("ScalarAlias");
            m.tables[1].push_back(alias); // This case tests append overflow, not exclusive overwrite.
            const auto before = Monstey_LtmiBuild(m);
            Monstey_Reject([&] { Monstey_LtmiSetValue(m, 0, 1, Monstey_Floats({3})); });
            Monstey_Check(Monstey_LtmiBuild(m) == before, "Native16 offset overflow mutated material");
        });
        test("raw_nonfinite_words_preserved", [&] {
            auto f = Monstey_Make();
            const auto values = 30 + Monstey_U16(f.bytes, 28);
            Monstey_PutU32(f.bytes, values + 16, 0xffc00000);
            auto m = Monstey_LtmiRead(f.bytes);
            Monstey_Check(Monstey_LtmiBuild(m) == f.bytes &&
                              Monstey_U32(Monstey_LtmiValue(m, m.tables[0][1]), 0) == 0xffc00000,
                          "NaN payload normalized");
            (void)Monstey_LtmiDescribe(m);
        });
        test("all_truncation_prefixes_reject_until_complete_metadata", [&] {
            auto f = Monstey_Make(7);
            for (std::size_t n = 0; n < f.bytes.size() - 3; ++n)
                Monstey_Reject([&] { (void)Monstey_LtmiRead(std::span<const std::uint8_t>(f.bytes).first(n)); });
        });
        test("bad_header_count_name_value_string_bounds", [&] {
            auto f = Monstey_Make();
            auto run = [&](B bytes) { Monstey_Reject([&] { (void)Monstey_LtmiRead(bytes); }); };
            auto b = f.bytes;
            b[0] = 0;
            run(b);
            b = f.bytes;
            Monstey_PutU32(b, 4, 9);
            run(b);
            b = f.bytes;
            Monstey_PutU16(b, 16, 129);
            run(b);
            b = f.bytes;
            Monstey_PutU32(b, 12, 0xffffffffU);
            run(b);
            b = f.bytes;
            Monstey_PutU16(b, f.metadata + 4, 256);
            run(b);
            b = f.bytes;
            Monstey_PutU32(b, f.offsets[3], 256);
            run(b);
            b = f.bytes;
            const auto values = 30 + Monstey_U16(b, 28);
            std::fill(b.begin() + static_cast<std::ptrdiff_t>(values),
                      b.begin() + static_cast<std::ptrdiff_t>(values + 256), std::uint8_t{1});
            run(b);
        });
        test("invalid_table_index_operations_are_transactional", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            const auto before = Monstey_LtmiBuild(m);
            Monstey_Reject([&] { Monstey_LtmiRemove(m, 2, 0); });
            Monstey_Reject([&] { Monstey_LtmiRename(m, 0, 128, Monstey_Text("x")); });
            Monstey_Reject([&] { Monstey_LtmiSetValue(m, 1, 128, B{}); });
            Monstey_Check(Monstey_LtmiBuild(m) == before, "Invalid selection mutated document");
        });
        test("native_zero_copy_type8_empty_and_nonterminated_profiles", [&] {
            Monstey_LtmiMaterial m;
            m.shader_name = Monstey_Text("fixture.fxo");
            Monstey_LtmiProperty p;
            p.type = 8;
            p.name = Monstey_Text("ZeroCopy");
            m.tables[0].push_back(p);
            for (const auto offset : {0U, 65535U, 0x10000U})
            {
                m.tables[0][0].offset = offset;
                auto bytes = Monstey_LtmiBuild(m);
                auto n = Monstey_LtmiRead(bytes);
                Monstey_Check(Monstey_LtmiBuild(n) == bytes && Monstey_LtmiValue(n, n.tables[0][0]).empty(),
                              "Zero-copy type8 incorrectly required source bytes");
                auto r = Monstey_LtmiDescribe(n);
                Monstey_Check(r["entries"][0u]["native_zero_copy"].as_bool() &&
                                  !r["entries"][0u]["serialized_string_observed"].as_bool(),
                              "Type8 observation claimed mandatory string");
            }
            m.values = B{'x', 'y', 'z'};
            m.tables[0][0].offset = 0;
            auto bytes = Monstey_LtmiBuild(m);
            Monstey_Check(Monstey_LtmiBuild(Monstey_LtmiRead(bytes)) == bytes, "Nonterminated type8 raw blob changed");
        });
        test("type8_string_convention_and_empty_value_have_explicit_scope", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            auto report = Monstey_LtmiDescribe(m);
            const auto &r = report["entries"][0u];
            Monstey_Check(r["serialized_string_observed"].as_bool() && !r["string_consumer_verified"].as_bool() &&
                              !r["authored_value_qualified"].as_bool(),
                          "Observed type8 string treated as native copy requirement");
            Monstey_LtmiSetValue(m, 0, 0, B{});
            auto n = Monstey_LtmiRead(Monstey_LtmiBuild(m));
            Monstey_Check(Monstey_LtmiValue(n, n.tables[0][0]).empty(), "Explicit zero-copy value failed");
        });
        test("opaque_native13_14_15_storage_roundtrips_without_semantic_claim", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            for (std::uint16_t type = 13; type <= 15; ++type)
            {
                Monstey_LtmiProperty p;
                p.type = type;
                p.name = Monstey_Text("Opaque" + std::to_string(type));
                B words;
                Monstey_AppendU32(words, 0x12345678);
                Monstey_AppendU32(words, 0x9abcdef0);
                Monstey_LtmiAdd(m, 1, p, words);
                auto n = Monstey_LtmiRead(Monstey_LtmiBuild(m));
                Monstey_Check(Monstey_LtmiValue(n, n.tables[1].back()) == words, "Opaque8byte payload changed");
                const auto description = Monstey_LtmiDescribe(n);
                const auto &r = description["entries"].as_array().back();
                Monstey_Check(r["raw_storage_editable"].as_bool() && !r["authored_value_qualified"].as_bool(),
                              "Opaque bytes acquired typed semantics");
            }
        });
        test("outside_native_size_table_keeps_raw_noop_without_dereference", [&] {
            Monstey_LtmiMaterial m;
            m.shader_name = Monstey_Text("fixture.fxo");
            for (auto type : {24U, 255U})
            {
                Monstey_LtmiProperty p;
                p.type = static_cast<std::uint16_t>(type);
                p.name = Monstey_Text("Unknown" + std::to_string(type));
                p.offset = 65535;
                m.tables[0].push_back(p);
            }
            auto bytes = Monstey_LtmiBuild(m);
            auto n = Monstey_LtmiRead(bytes);
            Monstey_Check(Monstey_LtmiBuild(n) == bytes &&
                              Monstey_LtmiDescribe(n)["entries"][0u]["native_copy_bytes"].is_null(),
                          "Unknown runtime slot fabricated source access or width");
        });
        test("value_and_name_spans_aliasing_the_source_document_stay_valid", [&] {
            auto m = Monstey_LtmiRead(Monstey_Make().bytes);
            std::span<const std::uint8_t> other(m.values.data() + 20, 4);
            Monstey_LtmiSetValue(m, 0, 1, other);
            Monstey_Check(Monstey_LtmiValue(m, m.tables[0][1]) == Monstey_Floats({1}), "Aliased value became dangling");
            std::span<const std::uint8_t> name(m.tables[0][1].name);
            Monstey_LtmiRename(m, 0, 0, name);
            Monstey_Check(m.tables[0][0].name == Monstey_Text("Scalar"), "Aliased name became dangling");
            Monstey_LtmiProperty p;
            p.type = 5;
            p.name = Monstey_Text("AliasAdd");
            std::span<const std::uint8_t> value(m.values.data() + 16, 4);
            const B captured(value.begin(), value.end());
            Monstey_LtmiAdd(m, 1, p, value);
            Monstey_Check(Monstey_LtmiValue(m, m.tables[1].back()) == captured,
                          "Aliased add value became dangling");
        });
        test("QWORD_upper_dwords_qualify_zero_slots7_12_16", [&] {
            Monstey_LtmiMaterial m;
            m.shader_name = Monstey_Text("fixture.fxo");
            m.values = B(65536, 0x7f);
            for (auto type : {0U, 7U, 12U, 16U})
            {
                Monstey_Check(Monstey_LtmiNativeSize(static_cast<std::uint16_t>(type)) == 0,
                              "Native QWORD upper zero was omitted");
                Monstey_LtmiProperty p;
                p.type = static_cast<std::uint16_t>(type);
                p.name = Monstey_Text("Zero" + std::to_string(type));
                const auto before = m.values;
                Monstey_LtmiAdd(m, 0, p, B{});
                Monstey_Check(m.values == before && Monstey_LtmiValue(m, m.tables[0].back()).empty(),
                              "Zero-copy add allocated or dereferenced bytes");
                auto report = Monstey_LtmiDescribe(m);
                const auto &row = report["entries"].as_array().back();
                Monstey_Check(row["native_copy_bytes"].as_uint() == 0 && row["native_zero_copy"].as_bool() &&
                                  !row["authored_value_qualified"].as_bool(),
                              "Zero-copy metadata acquired typed semantics");
            }
        });
        auto report = J::object({{"schema", "Monstey.LtmiTests/1"},
                                 {"groups", static_cast<std::uint64_t>(tests.size())},
                                 {"failed", failed},
                                 {"tests", tests},
                                 {"fixtures", "independently_encoded_synthetic_LTMI7_and8"},
                                 {"original_assets", false},
                                 {"runtime_rendering_verified", false}});
        Monstey_WriteNewText(dir / L"report.json", report.dump(2) + "\n");
        std::cout << tests.size() << " LTMI groups, failed=" << failed << '\n';
        return failed ? 1 : 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}