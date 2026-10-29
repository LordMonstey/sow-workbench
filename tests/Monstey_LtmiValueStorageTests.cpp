#include "Monstey_LtmiMaterial.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
using namespace Monstey;
namespace
{
using B = Monstey_Bytes;
using M = Monstey_LtmiMaterial;
using P = Monstey_LtmiProperty;
using J = Monstey_Json;
void Monstey_Check(bool b, const char *s)
{
    if (!b)
        throw std::runtime_error(s);
}
B Monstey_Text(std::string_view s, bool nul = false)
{
    B b(s.begin(), s.end());
    if (nul)
        b.push_back(0);
    return b;
}
B Monstey_Word(std::uint32_t x)
{
    B b;
    Monstey_AppendU32(b, x);
    return b;
}
P Monstey_Property(std::uint16_t type, std::uint32_t offset, std::string name)
{
    P p;
    p.type = type;
    p.offset = offset;
    p.flag_a = 0xa7;
    p.flag_b = 0x93;
    p.legacy_word = 0x7123;
    p.name = Monstey_Text(name);
    return p;
}
void Monstey_Put(M &m, std::size_t at, const B &b)
{
    std::copy(b.begin(), b.end(), m.values.begin() + static_cast<std::ptrdiff_t>(at));
}
M Monstey_Make(std::size_t size = 256)
{
    M m;
    m.version = 7;
    m.shader_name = Monstey_Text("shader\\fixture.fxo");
    m.shader_id = 0xfedcba98;
    m.unknown_20 = 0x12345678;
    m.unknown_24 = 0x87654321;
    m.legacy_blob = {4, 3, 0, 2, 1};
    m.trailer = {0xe1, 0, 0xff};
    m.values = B(size, 0xa5);
    Monstey_Put(m, 0, Monstey_Text("Surface", true));
    Monstey_Put(m, 32, Monstey_Word(0x3f000000));
    Monstey_Put(m, 64, B(12, 0x71));
    Monstey_Put(m, 96, Monstey_Text("textures\\original.tex", true));
    Monstey_Put(m, 128, B(8, 0xff));
    m.tables[0] = {Monstey_Property(8, 0, "SurfaceRecord"), Monstey_Property(0xab05, 0x98760020, "Scalar"),
                   Monstey_Property(2, 64, "Vector")};
    m.tables[1] = {Monstey_Property(1, 96, "Texture"), Monstey_Property(14, 128, "Opaque8")};
    m.source = Monstey_LtmiBuild(m);
    return m;
}
void Monstey_MetadataUnchanged(const M &before, const M &after, std::size_t table, std::size_t index)
{
    Monstey_Check(after.source == before.source, "Original source baseline changed");
    auto restored = after;
    restored.values = before.values;
    restored.tables[table][index].offset = before.tables[table][index].offset;
    Monstey_Check(Monstey_LtmiBuild(restored) == Monstey_LtmiBuild(before), "Unrelated metadata changed");
}
void Monstey_OnlyOverwrite(const M &before, const M &after, std::size_t t, std::size_t i, const B &value)
{
    auto expected = before.values;
    const auto at = std::size_t(static_cast<std::uint16_t>(before.tables[t][i].offset));
    std::copy(value.begin(), value.end(), expected.begin() + static_cast<std::ptrdiff_t>(at));
    Monstey_Check(after.values == expected && after.tables[t][i].offset == before.tables[t][i].offset,
                  "Exclusive edit grew blob, moved offset or changed unrelated bytes");
    Monstey_MetadataUnchanged(before, after, t, i);
}
void Monstey_OnlyAppend(const M &before, const M &after, std::size_t t, std::size_t i, const B &value)
{
    Monstey_Check(after.values.size() >= before.values.size() &&
                      std::equal(before.values.begin(), before.values.end(), after.values.begin()),
                  "Copy-on-write changed original bytes");
    Monstey_Check((after.tables[t][i].offset & 0xffff0000u) == (before.tables[t][i].offset & 0xffff0000u),
                  "Opaque high offset bits were lost");
    Monstey_Check(Monstey_LtmiValue(after, after.tables[t][i]) == value, "Appended selected value differs");
    Monstey_MetadataUnchanged(before, after, t, i);
}
void Monstey_Reject(const std::function<void()> &fn)
{
    bool bad = false;
    try
    {
        fn();
    }
    catch (const std::exception &)
    {
        bad = true;
    }
    Monstey_Check(bad, "Expected transactional failure");
}
M Monstey_RawType8(std::uint32_t offset)
{
    M m;
    m.shader_name = Monstey_Text("fixture.fxo");
    m.values = B(64, 0x71);
    m.tables[0] = {Monstey_Property(4, 32, "Scalar"), Monstey_Property(8, offset, "RawType8")};
    m.source = Monstey_LtmiBuild(m);
    return m;
}
std::uint32_t Monstey_Random(std::uint32_t &s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        Monstey_Check(argc == 2, "Usage: ValueStorageTests NEW-directory");
        const std::filesystem::path dir = argv[1];
        Monstey_Check(!std::filesystem::exists(dir), "New test directory required");
        std::filesystem::create_directories(dir);
        J tests = J::array();
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
                tests.as_array().push_back(
                    J::object({{"name", name}, {"passed", false}, {"error", e.what()}}));
            }
        };
        test("4096_exclusive_scalar_edits_never_grow_and_preserve_raw_metadata", [] {
            auto m = Monstey_Make();
            const auto baseline = m;
            for (std::uint32_t k = 0; k < 4096; ++k)
            {
                auto before = m;
                auto value = Monstey_Word(0x7fc00000u ^ k);
                Monstey_LtmiSetValue(m, 0, 1, value);
                Monstey_OnlyOverwrite(before, m, 0, 1, value);
            }
            Monstey_Check(m.values.size() == 256 && m.tables[0][1].offset == 0x98760020 &&
                              m.source == baseline.source,
                          "Repeated edit storage/source regression");
        });
        test("full65536_blob_allows_exclusive_scalar_edit", [] {
            auto m = Monstey_Make(65536);
            auto before = m;
            auto value = Monstey_Word(0xffc12345);
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyOverwrite(before, m, 0, 1, value);
            Monstey_Check(m.values.size() == 65536, "Full blob grew");
        });
        test("shared_scalar_copy_on_write_preserves_other_value_and_high_offsets", [] {
            auto m = Monstey_Make();
            auto alias = m.tables[0][1];
            alias.name = Monstey_Text("Shared");
            alias.offset = 0x11110020;
            m.tables[1].push_back(alias);
            m.source = Monstey_LtmiBuild(m);
            auto before = m;
            auto observed = Monstey_LtmiValue(m, alias);
            auto value = Monstey_Word(0x3f800000);
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyAppend(before, m, 0, 1, value);
            Monstey_Check(Monstey_LtmiValue(m, m.tables[1].back()) == observed &&
                              m.tables[1].back().offset == 0x11110020,
                          "Shared source changed");
        });
        test("partial_overlap_copy_on_write", [] {
            auto m = Monstey_Make();
            m.tables[1].push_back(Monstey_Property(4, 34, "Partial"));
            auto before = m;
            const auto observed = Monstey_LtmiValue(m, m.tables[1].back());
            auto value = Monstey_Word(0x01020304);
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyAppend(before, m, 0, 1, value);
            Monstey_Check(Monstey_LtmiValue(m, m.tables[1].back()) == observed,
                          "Partially overlapping word changed");
        });
        test("adjacent_spans_do_not_force_copy_on_write", [] {
            auto m = Monstey_Make();
            m.tables[1].push_back(Monstey_Property(4, 36, "Adjacent"));
            auto before = m;
            auto value = Monstey_Word(0x01020304);
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyOverwrite(before, m, 0, 1, value);
        });
        test("same_table_alias_is_protected", [] {
            auto m = Monstey_Make();
            m.tables[0].push_back(Monstey_Property(4, 32, "SameTable"));
            auto before = m;
            auto value = Monstey_Word(0x12345678);
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyAppend(before, m, 0, 1, value);
        });
        test("bounded_type8_observation_protects_shared_bytes", [] {
            auto m = Monstey_Make();
            m.tables[1].push_back(Monstey_Property(8, 32, "ObservedScalarBytes"));
            auto before = m;
            auto observed = Monstey_LtmiValue(m, m.tables[1].back());
            auto value = Monstey_Word(0x3f800001);
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyAppend(before, m, 0, 1, value);
            Monstey_Check(Monstey_LtmiValue(m, m.tables[1].back()) == observed,
                          "Observed type8 string changed");
        });
        test("unbounded_type8_raw_tail_preserved_with_append_diagnostic_change_allowed", [] {
            auto m = Monstey_RawType8(30);
            auto before = m;
            Monstey_Check(Monstey_LtmiValue(m, m.tables[0][1]).empty(), "Fixture is not unterminated");
            auto value = Monstey_Word(0x3f800000);
            Monstey_LtmiSetValue(m, 0, 0, value);
            Monstey_OnlyAppend(before, m, 0, 0, value);
            Monstey_Check(!Monstey_LtmiValue(m, m.tables[0][1]).empty(),
                          "Appended NUL optional observation control");
        });
        test("unbounded_type8_tail_after_selected_span_does_not_block_in_place", [] {
            auto m = Monstey_RawType8(40);
            auto before = m;
            auto value = Monstey_Word(0x3f800000);
            Monstey_LtmiSetValue(m, 0, 0, value);
            Monstey_OnlyOverwrite(before, m, 0, 0, value);
            Monstey_Check(Monstey_LtmiValue(m, m.tables[0][1]).empty(), "Unrelated raw tail was normalized");
        });
        test("out_of_blob_zero_copy_type8_does_not_invent_a_span", [] {
            auto m = Monstey_RawType8(65535);
            auto before = m;
            auto value = Monstey_Word(0x3f800000);
            Monstey_LtmiSetValue(m, 0, 0, value);
            Monstey_OnlyOverwrite(before, m, 0, 0, value);
        });
        test("shorter_texture_string_reuses_bounded_span_without_clearing_tail", [] {
            auto m = Monstey_Make();
            auto before = m;
            auto value = Monstey_Text("tiny", true);
            Monstey_LtmiSetValue(m, 1, 0, value);
            Monstey_OnlyOverwrite(before, m, 1, 0, value);
            Monstey_Check(Monstey_LtmiValue(m, m.tables[1][0]) == value, "Short string mismatch");
        });
        test("longer_texture_string_appends", [] {
            auto m = Monstey_Make();
            auto before = m;
            auto value = Monstey_Text("textures\\a_much_longer_texture_name_than_before.tex", true);
            Monstey_LtmiSetValue(m, 1, 0, value);
            Monstey_OnlyAppend(before, m, 1, 0, value);
        });
        test("later_string_growth_uses_observed_span_not_guessed_old_capacity", [] {
            auto m = Monstey_Make();
            Monstey_LtmiSetValue(m, 1, 0, Monstey_Text("x", true));
            auto before = m;
            auto value = Monstey_Text("middle", true);
            Monstey_LtmiSetValue(m, 1, 0, value);
            Monstey_OnlyAppend(before, m, 1, 0, value);
        });
        test("bounded_type8_shorter_string_reuses_span", [] {
            auto m = Monstey_Make();
            auto before = m;
            auto value = Monstey_Text("Rock", true);
            Monstey_LtmiSetValue(m, 0, 0, value);
            Monstey_OnlyOverwrite(before, m, 0, 0, value);
        });
        test("type8_explicit_empty_clear_relocates_without_erasing_observed_bytes", [] {
            auto m = Monstey_Make();
            m.tables[0][0].offset = 0xabcd0000;
            auto before = m;
            Monstey_LtmiSetValue(m, 0, 0, {});
            Monstey_OnlyAppend(before, m, 0, 0, {});
            Monstey_Check(m.values == before.values && m.tables[0][0].offset == 0xabcd0100 &&
                              Monstey_LtmiValue(m, m.tables[0][0]).empty(),
                          "Empty clear became a zero-length overwrite");
        });
        test("selected_unbounded_type8_is_not_given_guessed_capacity", [] {
            auto m = Monstey_RawType8(0);
            auto before = m;
            auto value = Monstey_Text("Bounded", true);
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyAppend(before, m, 0, 1, value);
        });
        test("unknown_opaque_extent_forces_append_without_touching_existing_bytes", [] {
            auto m = Monstey_Make();
            m.tables[1].push_back(Monstey_Property(24, 32, "UnknownOpaque"));
            auto before = m;
            auto value = Monstey_Word(0x87654321);
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyAppend(before, m, 0, 1, value);
        });
        test("aliased_input_from_neighbor_value_is_snapshotted_before_commit", [] {
            auto m = Monstey_Make();
            auto before = m;
            std::span<const std::uint8_t> value(m.values.data() + 64, 4);
            B captured(value.begin(), value.end());
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyOverwrite(before, m, 0, 1, captured);
        });
        test("aliased_input_from_original_source_keeps_baseline_immutable", [] {
            auto m = Monstey_Make();
            auto before = m;
            std::span<const std::uint8_t> value(m.source.data() + 8, 4);
            B captured(value.begin(), value.end());
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyOverwrite(before, m, 0, 1, captured);
        });
        test("aliased_input_from_metadata_does_not_invalidate", [] {
            auto m = Monstey_Make();
            auto before = m;
            std::span<const std::uint8_t> value(m.tables[0][1].name.data(), 4);
            B captured(value.begin(), value.end());
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyOverwrite(before, m, 0, 1, captured);
        });
        test("invalid_payload_rolls_back_complete_document", [] {
            auto m = Monstey_Make();
            const auto before = Monstey_LtmiBuild(m);
            const auto source = m.source;
            Monstey_Reject([&] { Monstey_LtmiSetValue(m, 0, 1, B{1, 2, 3}); });
            Monstey_Reject([&] { Monstey_LtmiSetValue(m, 1, 0, B{'x'}); });
            Monstey_Reject([&] { Monstey_LtmiSetValue(m, 1, 0, B{'a', 0, 'b', 0}); });
            Monstey_Check(Monstey_LtmiBuild(m) == before && m.source == source,
                          "Invalid input mutated state");
        });
        test("post_copy_validation_failure_rolls_back_even_after_tentative_overwrite", [] {
            auto m = Monstey_Make();
            m.tables[1][0].name.resize(256, 'x');
            const auto before_values = m.values, before_source = m.source, before_name = m.tables[1][0].name;
            const auto before_offset = m.tables[0][1].offset;
            Monstey_Reject([&] { Monstey_LtmiSetValue(m, 0, 1, Monstey_Word(0x12345678)); });
            Monstey_Check(m.values == before_values && m.source == before_source &&
                              m.tables[1][0].name == before_name && m.tables[0][1].offset == before_offset,
                          "Failed Build committed partial mutation");
        });
        test("shared_full_blob_append_failure_rolls_back", [] {
            auto m = Monstey_Make(65536);
            m.tables[1].push_back(m.tables[0][1]);
            const auto before = Monstey_LtmiBuild(m);
            Monstey_Reject([&] { Monstey_LtmiSetValue(m, 0, 1, Monstey_Word(0x12345678)); });
            Monstey_Check(Monstey_LtmiBuild(m) == before, "Failed shared append mutated state");
        });
        test("zero_copy_known_width_fields_do_not_claim_observed_bytes", [] {
            auto m = Monstey_Make(65536);
            for (auto type : {0, 7, 12, 16})
                m.tables[1].push_back(Monstey_Property(static_cast<std::uint16_t>(type), 32, "ZeroCopy"));
            auto before = m;
            auto value = Monstey_Word(0x7fc12345);
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyOverwrite(before, m, 0, 1, value);
            for (std::size_t i = 2; i < m.tables[1].size(); ++i)
            {
                auto raw = m.values;
                auto offset = m.tables[1][i].offset;
                Monstey_LtmiSetValue(m, 1, i, {});
                Monstey_Check(m.values == raw && m.tables[1][i].offset == offset,
                              "Zero-copy no-op allocated storage");
            }
        });
        test("all_qualified_fixed_width_raw_payloads_can_reuse_exclusive_spans", [] {
            for (auto type : std::array<std::uint16_t, 11>{2, 3, 4, 5, 6, 9, 10, 11, 13, 14, 15})
            {
                M m;
                m.shader_name = Monstey_Text("fixture.fxo");
                m.values = B(256, 0xa5);
                m.tables[0] = {Monstey_Property(type, 64, "Target"), Monstey_Property(4, 192, "Neighbor")};
                m.source = Monstey_LtmiBuild(m);
                auto before = m;
                B value(*Monstey_LtmiNativeSize(type), 0x7f);
                for (std::size_t i = 0; i < value.size(); ++i)
                    value[i] ^= static_cast<std::uint8_t>(i);
                Monstey_LtmiSetValue(m, 0, 0, value);
                Monstey_OnlyOverwrite(before, m, 0, 0, value);
                Monstey_Check(Monstey_LtmiValue(m, m.tables[0][0]) == value, "Raw payload normalized");
            }
        });
        test("1024_randomized_overlap_layouts_obey_copy_on_write_and_preserve_other_values", [] {
            std::uint32_t seed = 0x7123cdef;
            for (unsigned trial = 0; trial < 1024; ++trial)
            {
                M m;
                m.shader_name = Monstey_Text("fixture.fxo");
                m.values = B(256);
                for (auto &v : m.values)
                    v = static_cast<std::uint8_t>(Monstey_Random(seed));
                const auto at = Monstey_Random(seed) % 245;
                m.tables[0] = {Monstey_Property(4, 0x12340040, "Target")};
                m.tables[1] = {Monstey_Property(2, at, "Other")};
                m.source = Monstey_LtmiBuild(m);
                auto before = m;
                auto other = Monstey_LtmiValue(m, m.tables[1][0]);
                auto value = Monstey_LtmiValue(m, m.tables[0][0]);
                value[0] ^= 0x5a;
                const bool overlap = 64 < at + 12 && at < 68;
                Monstey_LtmiSetValue(m, 0, 0, value);
                if (overlap)
                    Monstey_OnlyAppend(before, m, 0, 0, value);
                else
                    Monstey_OnlyOverwrite(before, m, 0, 0, value);
                Monstey_Check(Monstey_LtmiValue(m, m.tables[1][0]) == other, "Randomized neighbor changed");
            }
        });
        test("aliased_input_copy_on_write_snapshots_before_append", [] {
            auto m = Monstey_Make();
            m.tables[1].push_back(m.tables[0][1]);
            auto before = m;
            std::span<const std::uint8_t> value(m.values.data() + 64, 4);
            B captured(value.begin(), value.end());
            Monstey_LtmiSetValue(m, 0, 1, value);
            Monstey_OnlyAppend(before, m, 0, 1, captured);
            Monstey_Check(Monstey_LtmiValue(m, m.tables[1].back()) == Monstey_Word(0x3f000000),
                          "Shared original value changed on aliased append");
        });
        test("type8_unknown_empty_assignment_is_a_true_raw_noop", [] {
            auto m = Monstey_RawType8(0);
            const auto before = Monstey_LtmiBuild(m);
            const auto baseline = m.source;
            Monstey_LtmiSetValue(m, 0, 1, {});
            Monstey_Check(Monstey_LtmiBuild(m) == before && m.source == baseline,
                          "Unknown zero-copy data was normalized by empty assignment");
        });
        test("type8_explicit_clear_full_blob_fails_without_false_success", [] {
            auto m = Monstey_Make(65536);
            const auto before = Monstey_LtmiBuild(m);
            const auto baseline = m.source;
            Monstey_Reject([&] { Monstey_LtmiSetValue(m, 0, 0, {}); });
            Monstey_Check(Monstey_LtmiBuild(m) == before && m.source == baseline &&
                              Monstey_LtmiValue(m, m.tables[0][0]) == Monstey_Text("Surface", true),
                          "Unrepresentable empty clear partially committed");
        });
        auto report = J::object({{"schema", "Monstey.LtmiValueStorageTests/1"},
                                 {"groups", static_cast<std::uint64_t>(tests.size())},
                                 {"failed", failed},
                                 {"tests", tests},
                                 {"original_assets", false},
                                 {"runtime_material_rendering_verified", false}});
        Monstey_WriteNewText(dir / L"report.json", report.dump(2) + "\n");
        std::cout << tests.size() << " storage groups, failed=" << failed << '\n';
        return failed ? 1 : 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
