#include "Monstey_FearUnpackedOwned.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <immintrin.h>
#include <iostream>
#include <string>
#include <vector>
using namespace Monstey;
namespace {
using U = std::uint32_t;
using Q = std::uint64_t;
Q checks = 0, cases = 0;
std::string current;
void Check(bool v, const char *message) {
    ++checks;
    if (!v)
        throw std::runtime_error(message);
}
struct Row {
    std::string key;
    U type = 3, lo = 0, hi = 100;
};
std::vector<std::uint8_t> Bytes(const std::string &s) { return {s.begin(), s.end()}; }
void Append(std::vector<std::uint8_t> &v, U word, U n = 4) {
    for (U k = 0; k < n; ++k)
        v.push_back(std::uint8_t(word >> (8 * k)));
}
std::vector<std::uint8_t> Constraints(const std::vector<Row> &rows) {
    std::vector<std::uint8_t> out;
    for (const auto &r : rows) {
        Append(out, static_cast<U>(r.key.size()));
        out.insert(out.end(), r.key.begin(), r.key.end());
        Append(out, r.type, 1);
        Append(out, 0xABCD1234u);
        Append(out, r.lo);
        Append(out, r.hi);
    }
    return out;
}
Monstey_FearUnpackedOwnedOptions Small() {
    Monstey_FearUnpackedOwnedOptions o;
    o.arena_budget_bytes = 1024 * 1024;
    return o;
}
Monstey_FearUnpackedOwnedImage Load(const std::vector<Row> &rows, const std::string &text,
                                    Monstey_FearUnpackedOwnedOptions o = Small()) {
    auto t = Bytes(text), c = Constraints(rows);
    auto th = Monstey_HashSha256(t), ch = Monstey_HashSha256(c);
    auto image = Monstey_FearLoadUnpackedOwned(t, c, o);
    Check(Monstey_HashSha256(t) == th && Monstey_HashSha256(c) == ch, "source spans changed");
    Check(image.profile.at("input_text_sha256").as_string() == th &&
              image.profile.at("input_constraints_sha256").as_string() == ch,
          "source profile hashes");
    return image;
}
Monstey_Json Summary(const Monstey_FearUnpackedOwnedImage &i) {
    Monstey_FearInspectorOptions q;
    q.summary = true;
    return Monstey_FearInspectUnpackedOwned(i, q);
}
Monstey_Json Values(const Monstey_FearUnpackedOwnedImage &i, U attr, U cat = 0, U record = 0) {
    Monstey_FearInspectorOptions q;
    q.category = cat;
    q.record = record;
    q.attribute = attr;
    return Monstey_FearInspectUnpackedOwned(i, q);
}
U Raw(const Monstey_FearUnpackedOwnedImage &i, U address, U n = 4) {
    Check(address >= i.arena_base && Q(address) - i.arena_base + n <= i.arena_used,
          "test raw range");
    U out = 0;
    for (U k = 0; k < n; ++k)
        out |= U(i.arena[address - i.arena_base + k]) << (8 * k);
    return out;
}
template <class Function> void Fault(const char *code, Function f) {
    bool caught = false;
    try {
        f();
    } catch (const Monstey_FearUnpackedOwnedFault &e) {
        caught = e.code == code;
    }
    Check(caught, "wrong/missing precise owned error");
    ++cases;
}
std::string BadKey() {
    return std::string("C") + std::bit_cast<char>(std::uint8_t{0xFF}) + "/R" +
           std::bit_cast<char>(std::uint8_t{0x80}) + "/A" + std::bit_cast<char>(std::uint8_t{0xC0});
}
Monstey_Json Fixtures(const std::filesystem::path &directory) {
    if (std::filesystem::exists(directory))
        throw std::runtime_error("Fresh fixture directory required");
    std::filesystem::create_directories(directory);
    const auto unicode = directory / std::filesystem::path(L"epreuve_\u00E9_\u6D4B\u8BD5");
    std::filesystem::create_directory(unicode);
    auto one = Constraints({{"Cat/Rec/A"}}), bad = one;
    bad.pop_back();
    auto key = BadKey();
    auto raw = Constraints({{key}});
    const auto t = unicode / L"source_\u00E9.txt", c = unicode / L"DBConstraints_\u6D4B\u8BD5.bin",
               shortC = directory / L"short.bin", overflow = directory / L"overflow.txt",
               byteT = directory / L"byte-names.txt", byteC = directory / L"byte-names.bin",
               large = directory / L"budget.txt";
    Monstey_WriteNewFile(t, Bytes("Cat/Rec/A=11\n"));
    Monstey_WriteNewFile(c, one);
    Monstey_WriteNewFile(shortC, bad);
    Monstey_WriteNewFile(overflow, Bytes(std::string(1024, 'x')));
    Monstey_WriteNewFile(byteT, Bytes(key + "=11\n"));
    Monstey_WriteNewFile(byteC, raw);
    Monstey_WriteNewFile(large, Bytes(std::string(70000, 'x')));
    return Monstey_Json::object(
        {{"text", Monstey_Utf8(std::filesystem::absolute(t).wstring())},
         {"constraints", Monstey_Utf8(std::filesystem::absolute(c).wstring())},
         {"short_constraints", Monstey_Utf8(std::filesystem::absolute(shortC).wstring())},
         {"overflow_text", Monstey_Utf8(std::filesystem::absolute(overflow).wstring())},
         {"budget_text", Monstey_Utf8(std::filesystem::absolute(large).wstring())},
         {"byte_text", Monstey_Utf8(std::filesystem::absolute(byteT).wstring())},
         {"byte_constraints", Monstey_Utf8(std::filesystem::absolute(byteC).wstring())},
         {"text_sha256", Monstey_HashSha256(t)},
         {"constraints_sha256", Monstey_HashSha256(c)},
         {"original_assets", false}});
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    try {
        if (argc == 4 && std::wstring_view(argv[1]) == L"--fixtures") {
            Monstey_WriteNewText(argv[3], Fixtures(argv[2]).dump(2) + "\n");
            return 0;
        }
        if (argc != 2)
            throw std::runtime_error("NEW_REPORT or --fixtures NEWDIR NEW_REPORT required");
        std::vector<std::string> groups;
        auto G = [&](const char *s) {
            current = s;
            groups.push_back(s);
        };
        G("complete_integer_graph_and_actual_pool");
        {
            auto i = Load({{"Cat/Rec/A"}}, "Cat/Rec/A=11\n");
            auto s = Summary(i);
            Check(s.at("totals").at("categories").as_uint() == 1 &&
                      s.at("totals").at("records").as_uint() == 1 &&
                      s.at("totals").at("attributes").as_uint() == 1,
                  "one complete graph");
            auto v = Values(i, 0);
            Check(v.at("items").at(0).at("signed_value").as_int() == 11, "actual typed integer");
            Check(v.at("selected_attribute").at("name").at("bytes_hex").as_string() == "41",
                  "unpacked pool name");
            Check(i.pool_begin > i.root && i.pool_end == i.root + i.root_allocation_bytes,
                  "pool after graph");
            Check(Raw(i, i.root + 16) == 1, "wrapper reference count");
            ++cases;
        }
        G("owned_numeric_all_types_and_partial_vector_writes");
        {
            std::vector<Row> rows{{"C/R/A1", 1}, {"C/R/A2", 2, 0, 0x41200000u},
                                  {"C/R/A3", 3}, {"C/R/A4", 4},
                                  {"C/R/A5", 5}, {"C/R/A6", 6},
                                  {"C/R/A7", 7}, {"C/R/A8", 8},
                                  {"C/R/A9", 9}, {"C/R/Z", 255}};
            auto i = Load(rows, "C/R/A1=-2\nC/R/A2=1.25\nC/R/A3=0x20\nC/R/A4=hello "
                                "world\nC/R/A5=wide\nC/R/A6=1.5,-2\nC/R/A7=3,bad\nC/R/"
                                "A8=4,5,6,7\nC/R/A9=ignored\nC/R/Z=ignored\n");
            Check(Summary(i).at("totals").at("attributes").as_uint() == 10,
                  "all attribute types retained");
            Check(Values(i, 0).at("items").at(0).at("byte_u8").as_uint() == 1,
                  "boolean normalization");
            Check(Values(i, 1).at("items").at(0).at("float32").at("bits_u32").as_uint() ==
                      0x3FA00000u,
                  "owned float conversion");
            Check(Values(i, 2).at("items").at(0).at("signed_value").as_int() == 0,
                  "native atoi differs from validator strtol base0");
            Check(Values(i, 3).at("items").at(0).at("string").at("bytes_hex").as_string() ==
                      "68656c6c6f20776f726c64",
                  "unpacked raw string");
            Check(Values(i, 4).at("items").empty() && Values(i, 8).at("items").empty() &&
                      Values(i, 9).at("items").empty(),
                  "native type5/unknown continuation not fabricated");
            auto a = Values(i, 5).at("items").at(0).at("components");
            Check(a.at(0).at("bits_u32").as_uint() == 0x3FC00000u &&
                      a.at(1).at("bits_u32").as_uint() == 0xC0000000u,
                  "vector2 components");
            auto b = Values(i, 6).at("items").at(0).at("components");
            Check(b.at(0).at("bits_u32").as_uint() == 0x40400000u &&
                      b.at(1).at("bits_u32").as_uint() == 0 &&
                      b.at(2).at("bits_u32").as_uint() == 0,
                  "earlier native strncpy padding remains visible");
            Check(Values(i, 7).at("items").at(0).at("components").at(3).at("bits_u32").as_uint() ==
                      0x40E00000u,
                  "vector4 final component");
            auto first=Load({{"C/R/A",7}},"C/R/A=3,bad\n");
            auto partial=Values(first,0).at("items").at(0).at("components");
            Check(partial.at(1).at("bits_u32").as_uint()==0xCDCDCDCDu&&partial.at(2).at("bits_u32").as_uint()==0xCDCDCDCDu,"first partial vector retains owned poison; adapter has no zero fallback");++cases;

            ++cases;
        }
        G("validator_false_AL_is_not_a_filter");
        {
            auto i = Load({{"C/R/A", 3, 0, 10}}, "C/R/A=-1\n");
            Check(i.profile.at("statistics")
                          .at("validation_false_results_ignored_by_native_callback")
                          .as_uint() == 1,
                  "real validator rejection recorded");
            Check(Values(i, 0).at("items").at(0).at("signed_value").as_int() == -1,
                  "native callback ignores validator AL");
            ++cases;
        }
        G("numeric_specials_subnormal_and_caller_MXCSR");
        {
            const U old = _mm_getcsr();
            _mm_setcsr(0xFFC0u);
            auto i = Load({{"C/R/A", 2, 0xFF800000u, 0x7F800000u}}, "C/R/A=-0\n");
            Check(_mm_getcsr() == 0xFFC0u, "load restores caller MXCSR");
            auto v = Values(i, 0);
            Check(v.at("items").at(0).at("float32").at("classification").as_string() ==
                          "negative_zero" &&
                      _mm_getcsr() == 0xFFC0u,
                  "query restores caller state and signed zero");
            _mm_setcsr(old);
            ++cases;
            for (const auto &token :
                 std::array<std::string, 3>{"nan", "1e5000", "1.401298464324817e-45"}) {
                auto x = Load({{"C/R/A", 2, 0xFF800000u, 0x7F800000u}}, "C/R/A=" + token + "\n");
                auto f = Values(x, 0).at("items").at(0).at("float32");
                Check(token == "nan" ? f.at("classification").as_string() == "nan"
                      : token == "1e5000"
                          ? f.at("classification").as_string() == "positive_infinity"
                          : f.at("bits_u32").as_uint() == 1,
                      "special numeric profile bits");
                ++cases;
            }
        }
        G("empty_root_and_actual_C_branch");
        {
            auto i = Load({}, "");
            Check(i.root_allocation_bytes == 20 &&
                      Summary(i).at("totals").at("categories").as_uint() == 0,
                  "empty scalar root");
            ++cases;
            auto c = Load({{"/R/A"}}, "/R/A=11\n");
            Check(c.root_allocation_bytes == 20 &&
                      c.profile.at("sizing_context_after_materialization").at(0).as_uint() == 0 &&
                      c.profile.at("sizing_context_after_materialization").at(2).as_uint() == 1,
                  "C0/A1 branch retained");
            ++cases;
        }
        G("constraint_short_reads_and_huge_name_policy");
        {
            auto text = Bytes("C/R/A=11\n"), full = Constraints({{"C/R/A"}});
            for (std::size_t n = 1; n < full.size(); ++n) {
                auto part = std::span(full).first(n);
                Fault("unpacked_open_returned_null",
                      [&] { (void)Monstey_FearLoadUnpackedOwned(text, part, Small()); });
            }
            std::vector<std::uint8_t> huge;
            Append(huge, 0x01000000u);
            Fault("unpacked_arena_exhausted",
                  [&] { (void)Monstey_FearLoadUnpackedOwned(text, huge, Small()); });
        }
        G("explicit_budget_and_line_overflow");
        Fault("unpacked_open_returned_null", [] { (void)Load({}, std::string(1024, 'x')); });
        {
            auto o = Small();
            o.arena_budget_bytes = 256 * 1024;
            Fault("unpacked_arena_exhausted", [&] { (void)Load({}, std::string(70000, 'x'), o); });
            o.arena_budget_bytes = 0;
            Fault("unpacked_arena_budget_invalid", [&] { (void)Load({}, "", o); });
        }
        G("raw_nonUTF8_names_remain_ordinal_queryable");
        {
            auto key = BadKey();
            auto i = Load({{key}}, key + "=11\n");
            Monstey_FearInspectorOptions q;
            auto c = Monstey_FearInspectUnpackedOwned(i, q);
            Check(c.at("items").at(0).at("name").at("bytes_hex").as_string() == "43ff",
                  "category raw bytes");
            q.category = 0;
            auto r = Monstey_FearInspectUnpackedOwned(i, q);
            Check(r.at("items").at(0).at("name").at("bytes_hex").as_string() == "5280",
                  "record raw bytes");
            auto v = Values(i, 0);
            Check(v.at("selected_attribute").at("name").at("bytes_hex").as_string() == "41c0" &&
                      v.at("items").at(0).at("signed_value").as_int() == 11,
                  "invalid UTF8 does not block ordinal access");
            Check(Monstey_Json::Parse(v.dump()).at("listing").as_string() == "values",
                  "JSON remains valid");
            const auto encoded=v.dump();Check(std::all_of(encoded.begin(),encoded.end(),[](unsigned char c){return c<128;}),"raw name bytes never become assumed UTF8 JSON text");
            ++cases;
        }
        G("opaque_profile_is_explicit_nonhistorical");
        {
            auto o = Small();
            o.callback_opaque_word = 0xA1B2C3D4u;
            o.insert_opaque_word = 0x1234ABCDu;
            auto i = Load({{"C/R/A"}}, "C/R/A=11\n", o);
            Check(i.profile.at("callback_opaque_word").as_uint() == 0xA1B2C3D4u &&
                      i.profile.at("insert_opaque_word").as_uint() == 0x1234ABCDu,
                  "explicit words reported");
            const U f = i.arena_base + 0x20000u, b = ((f - 0x18u) & 0xFFFFFFF8u) - 0x410u;
            Check(Raw(i, b - 0x10u) == o.callback_opaque_word &&
                      Raw(i, b - 0xE8u) == o.insert_opaque_word,
                  "two supplied words preserved without blanket zero");
            ++cases;
        }
        G("pagination_caps_and_argument_errors");
        {
            std::vector<Row> rows;
            std::string text;
            for (U n = 0; n < 200; ++n) {
                auto name = "C/R/A" + std::to_string(1000 + n);
                rows.push_back({name});
                text += name + "=11\n";
            }
            auto i = Load(rows, text);
            Monstey_FearInspectorOptions q;
            q.category = 0;
            q.record = 0;
            q.limit = 7;
            U emitted = 0;
            for (U offset = 0; offset < 210; offset += 7) {
                q.offset = offset;
                auto page = Monstey_FearInspectUnpackedOwned(i, q).at("page");
                emitted += static_cast<U>(page.at("emitted").as_uint());
                Check(page.at("total").as_uint() == 200, "page total");
                ++cases;
            }
            Check(emitted == 200, "all ordinals covered once");
            q.category = 1;
            Fault("unpacked_category_ordinal",
                  [&] { (void)Monstey_FearInspectUnpackedOwned(i, q); });
            q = {};
            q.record = 0;
            Fault("unpacked_query_arguments",
                  [&] { (void)Monstey_FearInspectUnpackedOwned(i, q); });
            q = {};
            q.limit = 0;
            Fault("unpacked_query_arguments",
                  [&] { (void)Monstey_FearInspectUnpackedOwned(i, q); });
        }
        G("multivalue_cursor_and_later_record_parent_are_native");
        {
            auto i = Load({{"C/R/A"}, {"C/R/B"}}, "C/R/A=11;22\nC/R/B=33\n");
            auto a = Values(i, 0);
            Check(a.at("items").size() == 2 &&
                      a.at("items").at(0).at("signed_value").as_int() == 11 &&
                      a.at("items").at(1).at("signed_value").as_int() == 33,
                  "per-attribute four-byte cursor is not repaired");
            ++cases;
            auto j = Load({{"C/R/A"}, {"C/S/B"}}, "C/R/A=11\nC/S/B=22\n");
            auto mem = j.Memory();
            U c0 = Monstey_FearDbChildByIndex(mem, j.root, 0),
              c1 = c0,
              r0 = Monstey_FearDbChildByIndex(mem, c0, 0),
              r1 = Monstey_FearDbChildByIndex(mem, c1, 1),
              a1 = Monstey_FearDbAttributeByIndex(mem, r1, 0);
            Check(Raw(j, a1 + 4u) == r0 && r1 != r0, "native local parent formula is retained");
            Check(Values(j, 0, 0, 1).at("items").at(0).at("signed_value").as_int() == 22,
                  "later record ordinal remains usable");
            ++cases;
        }
        G("unknown_keys_comments_and_duplicate_constraints");
        {
            auto i = Load({{"C/R/A"}, {"C/R/A", 3, 0, 50}},
                          ";comment\r\nUnknown/R/A=99\r\nC/R/A=11\r\nC/R/A=22\r\n");
            Check(Summary(i).at("totals").at("attributes").as_uint() == 1 &&
                      Values(i, 0).at("items").at(0).at("signed_value").as_int() == 22,
                  "native map duplicate/update flow");
            Check(i.profile.at("statistics").at("validation_calls").as_uint() == 2,
                  "unknown keys/comments do not validate");
            ++cases;
        }

        G("BOM_is_metadata_not_automatic_editor_source_routing");
        {
            std::vector<std::uint8_t> utf16{0xFF, 0xFE};
            for (char16_t c : std::u16string(u"[meta]\n")) {
                utf16.push_back(static_cast<std::uint8_t>(c & 255));
                utf16.push_back(static_cast<std::uint8_t>(c >> 8));
            }
            auto constraints = Constraints({{"C/R/A"}});
            auto image = Monstey_FearLoadUnpackedOwned(utf16, constraints, Small());
            Check(Summary(image).at("totals").at("categories").as_uint() == 0,
                  "literal UTF16 signature not promoted to source metadata parser");
            auto profile = image.profile.at("input_text_byte_profile");
            Check(profile.at("bom_signature").as_string() == "utf16le_bom" &&
                      profile.at("contains_NUL").as_bool() &&
                      !profile.at("decoder_applied").as_bool(),
                  "explicit signature and no-decoder profile");
            ++cases;
            std::vector<std::uint8_t> utf8{0xEF, 0xBB, 0xBF};
            auto tail = Bytes("C/R/A=11\n");
            utf8.insert(utf8.end(), tail.begin(), tail.end());
            auto unchanged = Monstey_FearLoadUnpackedOwned(utf8, constraints, Small());
            Check(Summary(unchanged).at("totals").at("categories").as_uint() == 0 &&
                      unchanged.profile.at("input_text_byte_profile")
                              .at("bom_signature")
                              .as_string() == "utf8_bom",
                  "UTF8 BOM is not silently stripped");
            ++cases;
        }


        G("declared_numeric_base_and_raw_display_cap");
        {
            auto base0=Load({{"C/R/A",3,1,50}},"C/R/A=0x20\n");
            Check(base0.profile.at("statistics").at("validation_false_results_ignored_by_native_callback").as_uint()==0,"owned validator strtol uses base0");
            Check(Values(base0,0).at("items").at(0).at("signed_value").as_int()==0,"materializer decimal atoi remains distinct");++cases;
            auto named=Load({{"Cat/Rec/Attr"}},"Cat/Rec/Attr=11\n");Monstey_FearInspectorOptions cap;cap.string_bytes=2;
            auto name=Monstey_FearInspectUnpackedOwned(named,cap).at("items").at(0).at("name");
            Check(name.at("bytes_hex").as_string()=="4361"&&name.at("status").as_string()=="truncated_at_display_limit"&&!name.at("terminator_observed").as_bool(),"display cap is exact raw bytes without invented terminator");++cases;
        }

        auto names = Monstey_Json::array();
        for (const auto &name : groups)
            names.as_array().push_back(name);
        Monstey_WriteNewText(argv[1], Monstey_Json::object({{"status", "PASS"},
                                                            {"groups", groups.size()},
                                                            {"cases", cases},
                                                            {"checks", checks},
                                                            {"group_names", names},
                                                            {"original_assets", false},
                                                            {"original_executed", false},
                                                            {"CLI_U16_wrap_claim", false}})
                                              .dump(2) +
                                          "\n");
        std::cout << "PASS " << groups.size() << " groups " << cases << " cases " << checks
                  << " checks\n";
        return 0;
    } catch (const Monstey_FearUnpackedOwnedFault &e) {
        std::cerr << current << ": " << e.code << ": " << e.what() << " " << e.details.dump()
                  << '\n';
        return 1;
    } catch (const std::exception &e) {
        std::cerr << current << ": " << e.what() << '\n';
        return 1;
    }
}
