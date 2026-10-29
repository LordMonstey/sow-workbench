#include "Monstey_FearUnpackedOwned.hpp"
#include <array>
#include <charconv>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <set>
using namespace Monstey;
namespace {
using Monstey_UnpackedU32 = std::uint32_t;
Monstey_UnpackedU32 Monstey_Number(const std::wstring &w, bool hex = false) {
    auto s = Monstey_Utf8(w);
    int base = 10;
    if (hex && s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s.erase(0, 2);
        base = 16;
    }
    Monstey_UnpackedU32 n = 0;
    auto [p, e] = std::from_chars(s.data(), s.data() + s.size(), n, base);
    if (s.empty() || e != std::errc{} || p != s.data() + s.size())
        throw std::invalid_argument("Expected an unsigned uint32 argument");
    return n;
}
std::string Monstey_HashArgument(const std::wstring &w) {
    auto s = Monstey_Utf8(w);
    if (s.size() != 64)
        throw std::invalid_argument("SHA256 must contain64 hexadecimal characters");
    for (auto &c : s) {
        if (c >= 'A' && c <= 'F')
            c = static_cast<char>(c - 'A' + 'a');
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            throw std::invalid_argument("SHA256 must contain64 hexadecimal characters");
    }
    return s;
}
Monstey_Json Monstey_Help() {
    return Monstey_Json::object(
        {{"tool", "Monstey_FearUnpackedInspector"},
         {"schema", "Monstey.FearUnpackedInspectorHelp/1"},
         {"usage", "--text <source.txt> --constraints <DBConstraints> [--summary | --category N "
                   "[--record N [--attribute N]]] [--offset N --limit N --string-bytes N] "
                   "[--arena-bytes N] [--callback-opaque U32 --insert-opaque U32] [--text-sha256 "
                   "HEX --constraints-sha256 HEX] [--output <NEW.json>]"},
         {"paths", "Unicode Windows paths; explicit override content remains original 8bit bytes; "
                   "no guessed UTF8 or automatic .s editor-metadata routing"},
         {"queries", "Zero-based ordinals; default categories. Category lists records, record "
                     "lists attributes, attribute lists native visible values."},
         {"page_limits", "offset uint32;limit1..10000 default50;string-bytes2..1048576 default256"},
         {"arena", "Bounded native32 arena256KiB..1GiB;default67108864bytes. Input buffers and "
                   "host control containers are separate. Exhaustion is an explicit error."},
         {"opaque_words", "Explicit owned defaults callback0x4D535459,insert0x554E504B. Overrides "
                          "accept decimal or0xhex; no historical value inferred."},
         {"numeric_profile", "Owned MSVC2026 dedicated C locale; native protocol bodies are "
                             "concrete; original CRT/STL/allocator/x87 ABI/history unqualified."},
         {"output",
          "JSON stdout; optional new JSON report only on success. Existing output and source "
          "aliases rejected. No GADB writer, graph export or cross-source conversion."},
         {"exit_codes", "0 success/help;1 input/load/query/output failure;2 invalid arguments"}});
}
struct Monstey_Source {
    std::filesystem::path path;
    std::string before, after;
    std::uintmax_t bytes = 0;
    Monstey_Bytes data;
};
void Monstey_Read(Monstey_Source &s, const std::string &expected, Monstey_UnpackedU32 budget) {
    s.path = std::filesystem::absolute(Monstey_WindowsFilePath(s.path));
    s.bytes = std::filesystem::file_size(s.path);
    if (s.bytes > budget)
        throw Monstey_FearUnpackedOwnedFault(
            "unpacked_input_exceeds_arena_budget",
            "Input file exceeds the configured arena budget; choose an explicit larger budget");
    s.before = Monstey_HashSha256(s.path);
    if (!expected.empty() && s.before != expected)
        throw Monstey_FearUnpackedOwnedFault(
            "unpacked_source_hash_mismatch",
            "Input SHA256 does not match the requested source identity");
    s.data = Monstey_ReadFile(s.path);
    if (Monstey_HashSha256(s.data) != s.before)
        throw Monstey_FearUnpackedOwnedFault("unpacked_source_changed",
                                             "Input changed during read");
}
Monstey_Json Monstey_Observation(Monstey_Source &s) {
    auto out = Monstey_Json::object(
        {{"path", s.path.empty() ? std::string() : Monstey_Utf8(s.path.wstring())},
         {"bytes", static_cast<std::uint64_t>(s.bytes)},
         {"sha256_before", s.before}});
    if (s.before.empty()) {
        out["post_hash_status"] = "no_initial_hash";
        return out;
    }
    try {
        s.after = Monstey_HashSha256(s.path);
        out["sha256_after"] = s.after;
        out["unchanged"] = s.before == s.after;
    } catch (const std::exception &e) {
        out["post_hash_error"] = e.what();
        out["unchanged"] = false;
    }
    return out;
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    _setmode(_fileno(stdout), _O_BINARY);
    auto report =
        Monstey_Json::object({{"schema", "Monstey.FearUnpackedInspection/1"}, {"ok", false}});
    int failure = 2;
    Monstey_Source text, constraints;
    std::filesystem::path output;
    try {
        if (argc == 2 && std::wstring_view(argv[1]) == L"--help") {
            std::cout << Monstey_Help().dump(2) << '\n';
            return 0;
        }
        Monstey_FearUnpackedOwnedOptions owned;
        Monstey_FearInspectorOptions query;
        std::string expectedText, expectedConstraints;
        std::set<std::wstring> seen;
        for (int n = 1; n < argc; ++n) {
            const std::wstring flag = argv[n];
            if (!seen.insert(flag).second)
                throw std::invalid_argument("Duplicate option");
            if (flag == L"--summary") {
                query.summary = true;
                continue;
            }
            if (flag != L"--text" && flag != L"--constraints" && flag != L"--output" &&
                flag != L"--category" && flag != L"--record" && flag != L"--attribute" &&
                flag != L"--offset" && flag != L"--limit" && flag != L"--string-bytes" &&
                flag != L"--arena-bytes" && flag != L"--callback-opaque" &&
                flag != L"--insert-opaque" && flag != L"--text-sha256" &&
                flag != L"--constraints-sha256")
                throw std::invalid_argument("Unknown option;use --help");
            if (++n == argc)
                throw std::invalid_argument("Missing option argument");
            const std::wstring value = argv[n];
            if (flag == L"--text")
                text.path = value;
            else if (flag == L"--constraints")
                constraints.path = value;
            else if (flag == L"--output")
                output = value;
            else if (flag == L"--category")
                query.category = Monstey_Number(value);
            else if (flag == L"--record")
                query.record = Monstey_Number(value);
            else if (flag == L"--attribute")
                query.attribute = Monstey_Number(value);
            else if (flag == L"--offset")
                query.offset = Monstey_Number(value);
            else if (flag == L"--limit")
                query.limit = Monstey_Number(value);
            else if (flag == L"--string-bytes")
                query.string_bytes = Monstey_Number(value);
            else if (flag == L"--arena-bytes")
                owned.arena_budget_bytes = Monstey_Number(value);
            else if (flag == L"--callback-opaque")
                owned.callback_opaque_word = Monstey_Number(value, true);
            else if (flag == L"--insert-opaque")
                owned.insert_opaque_word = Monstey_Number(value, true);
            else if (flag == L"--text-sha256")
                expectedText = Monstey_HashArgument(value);
            else
                expectedConstraints = Monstey_HashArgument(value);
        }
        if (text.path.empty() || constraints.path.empty())
            throw std::invalid_argument("--text and --constraints are required");
        if (query.limit == 0 || query.limit > 10000 || query.string_bytes < 2 ||
            query.string_bytes > 1048576 || (query.record && !query.category) ||
            (query.attribute && !query.record) ||
            (query.summary && (query.category || query.record || query.attribute)))
            throw std::invalid_argument("Invalid filter hierarchy or display bounds");
        if (owned.arena_budget_bytes < 256u * 1024u ||
            owned.arena_budget_bytes > 1024u * 1024u * 1024u)
            throw std::invalid_argument("arena-bytes must be256KiB..1GiB");
        failure = 1;
        Monstey_Read(text, expectedText, owned.arena_budget_bytes);
        Monstey_Read(constraints, expectedConstraints, owned.arena_budget_bytes);
        report["text_byte_profile"] = Monstey_FearUnpackedTextProfile(text.data);
        if (!output.empty()) {
            const std::array<std::filesystem::path, 2> sources{text.path, constraints.path};
            Monstey_RequireOfflineOutput(output, sources);
        }
        auto image = Monstey_FearLoadUnpackedOwned(text.data, constraints.data, owned);
        report["inspection"] = Monstey_FearInspectUnpackedOwned(image, query);
        report["text_source"] = Monstey_Observation(text);
        report["constraints_source"] = Monstey_Observation(constraints);
        if (text.after != text.before || constraints.after != constraints.before)
            throw Monstey_FearUnpackedOwnedFault("unpacked_source_changed",
                                                 "Source changed during inspection");
        report["ok"] = true;
        const auto json = report.dump(2) + "\n";
        if (!output.empty())
            Monstey_WriteNewText(output, json);
        std::cout << json;
        return 0;
    } catch (const Monstey_FearUnpackedOwnedFault &e) {
        report["error_code"] = e.code;
        report["error"] = e.what();
        report["details"] = e.details;
    } catch (const std::invalid_argument &e) {
        report["error_code"] = failure == 2 ? "invalid_arguments" : "unpacked_inspector_failure";
        report["error"] = e.what();
    } catch (const std::exception &e) {
        report["error_code"] = failure == 2 ? "invalid_arguments" : "unpacked_inspector_failure";
        report["error"] = e.what();
    }
    report["ok"] = false;
    report["text_source"] = Monstey_Observation(text);
    report["constraints_source"] = Monstey_Observation(constraints);
    std::cout << report.dump(2) << '\n';
    return failure;
}
