#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "Monstey_FearDbInspector.hpp"
#include <array>
#include <iostream>
using namespace Monstey;
namespace {
std::uint64_t Monstey_Checks = 0, Monstey_Cases = 0;
void Monstey_Check(bool value, const char* message) {
    ++Monstey_Checks;
    if (!value) throw std::runtime_error(message);
}
struct Monstey_Attribute { std::uint32_t name, type, usage; std::vector<std::uint32_t> words; };
struct Monstey_Record { std::uint32_t name; std::vector<Monstey_Attribute> attributes; };
struct Monstey_Category { std::uint32_t name; std::vector<Monstey_Record> records; };
struct Monstey_Fixture {
    Monstey_Bytes pool{0};
    std::vector<Monstey_Category> categories;
    std::uint32_t String(std::string_view text) {
        const auto offset = static_cast<std::uint32_t>(pool.size());
        pool.insert(pool.end(), text.begin(), text.end()); pool.push_back(0); return offset;
    }
    Monstey_Bytes Serialize() const {
        std::array<std::uint32_t, 4> counts{static_cast<std::uint32_t>(categories.size()), 0, 0, 0};
        for (const auto& category : categories) {
            counts[1] += static_cast<std::uint32_t>(category.records.size());
            for (const auto& record : category.records) {
                counts[2] += static_cast<std::uint32_t>(record.attributes.size());
                for (const auto& attribute : record.attributes) counts[3] += static_cast<std::uint32_t>(attribute.words.size());
            }
        }
        Monstey_Bytes bytes;
        Monstey_AppendU32(bytes, 0x42444147); Monstey_AppendU32(bytes, 3);
        Monstey_AppendU32(bytes, static_cast<std::uint32_t>(pool.size()));
        for (auto count : counts) Monstey_AppendU32(bytes, count);
        bytes.insert(bytes.end(), pool.begin(), pool.end());
        Monstey_AppendU32(bytes, counts[0]);
        for (const auto& category : categories) {
            Monstey_AppendU32(bytes, category.name);
            Monstey_AppendU32(bytes, static_cast<std::uint32_t>(category.records.size()));
            for (const auto& record : category.records) {
                Monstey_AppendU32(bytes, record.name);
                Monstey_AppendU32(bytes, static_cast<std::uint32_t>(record.attributes.size()));
                for (const auto& attribute : record.attributes) {
                    Monstey_AppendU32(bytes, attribute.name); Monstey_AppendU32(bytes, attribute.type);
                    Monstey_AppendU32(bytes, attribute.usage);
                    Monstey_AppendU32(bytes, static_cast<std::uint32_t>(attribute.words.size()));
                    for (auto word : attribute.words) Monstey_AppendU32(bytes, word);
                }
            }
        }
        return bytes;
    }
};
struct Monstey_Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Monstey_Handle() { if (value != INVALID_HANDLE_VALUE && value != nullptr) CloseHandle(value); }
};
std::wstring Monstey_Quote(std::wstring_view value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (auto ch : value) {
        if (ch == L'\\') { ++slashes; continue; }
        if (ch == L'\"') { result.append(slashes * 2 + 1, L'\\'); result += ch; slashes = 0; continue; }
        result.append(slashes, L'\\'); slashes = 0; result += ch;
    }
    result.append(slashes * 2, L'\\'); result += L'\"'; return result;
}
struct Monstey_RunResult { DWORD code; Monstey_Json json; std::string text; };
Monstey_RunResult Monstey_Run(const std::filesystem::path& exe, const std::filesystem::path& root,
                             const std::vector<std::wstring>& arguments) {
    ++Monstey_Cases;
    const auto stdout_path = root / (L"stdout-" + std::to_wstring(Monstey_Cases) + L".json");
    const auto stderr_path = root / (L"stderr-" + std::to_wstring(Monstey_Cases) + L".txt");
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Monstey_Handle stdout_handle{CreateFileW(stdout_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    Monstey_Handle stderr_handle{CreateFileW(stderr_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
    Monstey_Check(stdout_handle.value != INVALID_HANDLE_VALUE && stderr_handle.value != INVALID_HANDLE_VALUE, "test output handle creation");
    auto command = Monstey_Quote(exe.wstring());
    for (const auto& argument : arguments) { command += L" "; command += Monstey_Quote(argument); }
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = stdout_handle.value; startup.hStdError = stderr_handle.value; startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    Monstey_Check(CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE, "test CLI process creation");
    Monstey_Handle process_handle{process.hProcess}, thread_handle{process.hThread};
    const auto wait = WaitForSingleObject(process_handle.value, 30000);
    if (wait != WAIT_OBJECT_0) { TerminateProcess(process_handle.value, 99); throw std::runtime_error("CLI test timed out"); }
    DWORD code = 99; Monstey_Check(GetExitCodeProcess(process_handle.value, &code) != FALSE, "test CLI process exit");
    const auto text = Monstey_ReadText(stdout_path);
    Monstey_Check(Monstey_ReadText(stderr_path).empty(), "CLI stderr must be empty; stdout holds JSON errors");
    return {code, Monstey_Json::Parse(text), text};
}
}
int wmain(int argc, wchar_t** argv) {
    try {
        if (argc != 3) throw std::runtime_error("InspectorTests <CLI exe> <NEW output directory>");
        const std::filesystem::path exe = argv[1], root = argv[2];
        Monstey_Check(std::filesystem::create_directory(root), "test directory must be new");
        Monstey_Fixture fixture;
        const auto category = fixture.String("same"), record = fixture.String("record"), attribute = fixture.String("value");
        const auto string = fixture.String(std::string("A\\B\xff", 4));
        const auto wide = static_cast<std::uint32_t>(fixture.pool.size());
        fixture.pool.insert(fixture.pool.end(), {0x41,0,0x3d,0xd8,0,0xde,0,0});
        const auto vector = static_cast<std::uint32_t>(fixture.pool.size());
        Monstey_AppendU32(fixture.pool, 0x3f800000); Monstey_AppendU32(fixture.pool, 0x80000000);
        Monstey_AppendU32(fixture.pool, 0x7fc12345); Monstey_AppendU32(fixture.pool, 0x7f800000);
        const auto unterminated = static_cast<std::uint32_t>(fixture.pool.size());
        fixture.pool.insert(fixture.pool.end(), {'x','y','z'});
        fixture.categories = {{category, {{record, {
            {attribute,3,0x12345678,{0xffffffff,0x80000000,7}},
            {attribute,2,0,{0x80000000,0x7fc12345,0x7f800000,0xff800000}},
            {attribute,1,0,{0x12345680}}, {attribute,4,0,{string,0xfffffff0,unterminated,static_cast<std::uint32_t>(fixture.pool.size())+20u}},
            {attribute,5,0,{wide}}, {attribute,6,0,{vector}}, {attribute,7,0,{vector}},
            {attribute,8,0,{vector,0x7ffffff0,static_cast<std::uint32_t>(fixture.pool.size())+20u,static_cast<std::uint32_t>(fixture.pool.size())-1u}}, {attribute,9,0,{0x00010000,0xffffffff}},
            {attribute,77,0,{0xdeadbeef}}, {0x7fffffff,0x109,0x123,{0x00010000}}
        }}}}, {category, {{record,{}}}}};
        auto bytes = fixture.Serialize(); bytes.insert(bytes.end(), {0xaa,0xbb,0xcc});
        const auto input = root / L"synthetic-é.gadb";
        Monstey_WriteNewFile(input, bytes);
        const auto before = Monstey_HashSha256(input);
        auto run = [&](std::vector<std::wstring> args) {
            args.insert(args.begin(), input.wstring()); args.insert(args.begin(), L"--input");
            return Monstey_Run(exe, root, args);
        };
        auto response = run({L"--limit",L"1"});
        Monstey_Check(response.code == 0 && response.json["ok"].as_bool(), "root CLI success");
        Monstey_Check(response.json["source_sha256"].as_string() == before && response.json["trailing_bytes"].as_uint() == 3, "source hash and trailing bytes honest");
        Monstey_Check(response.json["consumed_bytes"].as_uint() + 3 == bytes.size(), "consumed count excludes suffix");
        Monstey_Check(response.json["inspection"]["page"]["has_more"].as_bool() && response.json["inspection"]["page"]["next_offset"].as_uint() == 1, "root pagination");
        response = run({L"--offset",L"1",L"--limit",L"1"});
        Monstey_Check(response.json["inspection"]["items"][0]["ordinal"].as_uint() == 1, "duplicate names selected by ordinal");
        response = run({L"--category",L"0"});
        Monstey_Check(response.json["inspection"]["listing"].as_string() == "records", "record listing");
        response = run({L"--category",L"0",L"--record",L"0"});
        Monstey_Check(response.json["inspection"]["items"].size() == 11 && response.json["inspection"]["items"][0]["runtime_usage_u8"].as_uint() == 0x78, "attribute usage narrowing");
        Monstey_Check(response.json["inspection"]["items"][10]["name"]["status"].as_string() == "unavailable_outside_owned_allocation", "invalid name explicit");
        auto values = [&](unsigned a, std::vector<std::wstring> tail = {}) {
            std::vector<std::wstring> args{L"--category",L"0",L"--record",L"0",L"--attribute",std::to_wstring(a)};
            args.insert(args.end(), tail.begin(), tail.end()); return run(args);
        };
        response = values(0,{L"--offset",L"1",L"--limit",L"1"});
        Monstey_Check(response.json["inspection"]["items"][0]["signed_value"].as_int() == -2147483648ll, "int32 signed bits");
        response = values(1);
        const auto& floats = response.json["inspection"]["items"];
        Monstey_Check(floats[0]["float32"]["bits_u32"].as_uint() == 0x80000000 && floats[0]["float32"]["classification"].as_string() == "negative_zero", "float negative zero payload");
        Monstey_Check(floats[1]["float32"]["bits_u32"].as_uint() == 0x7fc12345 && floats[1]["float32"]["value"].is_null(), "NaN payload retained with JSON null value");
        Monstey_Check(floats[2]["float32"]["classification"].as_string() == "positive_infinity" && floats[3]["float32"]["classification"].as_string() == "negative_infinity", "infinities explicit");
        response = values(2); Monstey_Check(response.json["inspection"]["items"][0]["byte_u8"].as_uint() == 128, "bool AL not normalized");
        response = values(3);
        Monstey_Check(response.json["inspection"]["items"][0]["string"]["bytes_hex"].as_string() == "415c42ff", "string bytes exact without locale");
        Monstey_Check(response.json["inspection"]["items"][1]["string"]["status"].as_string() == "unavailable_outside_owned_allocation", "bad string identity explicit");
        Monstey_Check(response.json["inspection"]["items"][2]["string"]["status"].as_string() == "unavailable_terminator_in_pool", "unterminated pool cannot read into graph");
        Monstey_Check(response.json["inspection"]["items"][3]["string"]["status"].as_string() == "outside_pool_not_decoded" && response.json["inspection"]["items"][3]["string"]["identity"]["pointer_region"].as_string() == "runtime_graph_or_diagnostic_caller_name", "owned non-pool string identity is not called unavailable memory");
        response = values(3,{L"--string-bytes",L"2"});
        Monstey_Check(response.json["inspection"]["items"][0]["string"]["status"].as_string() == "truncated_at_display_limit", "string truncation explicit");
        response = values(4); Monstey_Check(response.json["inspection"]["items"][0]["string"]["bytes_hex"].as_string() == "41003dd800de", "wide code units exact");
        for (unsigned a = 5; a <= 7; ++a) {
            response = values(a); Monstey_Check(response.json["inspection"]["items"][0]["components"].size() == a - 3u, "bounded vector components");
        }
        Monstey_Check(response.json["inspection"]["items"][1]["status"].as_string() == "unavailable_outside_owned_allocation", "bad vector identity explicit");
        Monstey_Check(response.json["inspection"]["items"][2]["status"].as_string() == "outside_pool_not_decoded", "owned non-pool vector identity is not called unavailable memory");
        Monstey_Check(response.json["inspection"]["items"][3]["status"].as_string() == "pool_span_crosses_boundary_not_decoded", "vector cannot cross the pool boundary into owned runtime graph");
        response = values(8);
        Monstey_Check(response.json["inspection"]["items"][0]["target_category_ordinal"].as_uint() == 1 && response.json["inspection"]["items"][1]["status"].as_string() == "null_after_loader_fixup_original_target_unavailable", "resolved and unresolved record identity provenance");
        response = values(9); Monstey_Check(response.json["inspection"]["items"][0]["runtime_word_u32"].as_uint() == 0xdeadbeef && response.json["inspection"]["items"][0]["kind"].as_string() == "unknown_runtime_type", "unknown type word retained");
        response = values(10); Monstey_Check(response.json["inspection"]["selected_attribute"]["runtime_type_u8"].as_uint() == 9 && response.json["inspection"]["items"][0]["target_category_ordinal"].as_uint() == 1, "full packed type narrows and link fixup uses stored u8");
        response = values(0,{L"--offset",L"4294967295"}); Monstey_Check(response.json["inspection"]["items"].empty(), "huge offset yields empty page without overflow");
        response = run({L"--summary"}); Monstey_Check(!response.json["inspection"].contains("items") && response.json["inspection"]["totals"]["attributes"].as_uint() == 11, "summary contains counts not names or values");
        const auto output = root / L"new-report.json";
        response = run({L"--summary",L"--output",output.wstring()});
        Monstey_Check(response.code == 0 && Monstey_ReadText(output) == response.text, "optional new output equals JSON stdout");
        const auto output_hash = Monstey_HashSha256(output);
        response = run({L"--summary",L"--output",output.wstring()}); Monstey_Check(response.code == 1 && !response.json["ok"].as_bool() && Monstey_HashSha256(output) == output_hash, "existing output untouched");
        response = run({L"--summary",L"--output",input.wstring()}); Monstey_Check(response.code == 1 && Monstey_HashSha256(input) == before, "source output alias rejected");
        for (auto args : std::vector<std::vector<std::wstring>>{{L"--limit",L"0"},{L"--limit",L"10001"},{L"--record",L"0"},{L"--attribute",L"0"},{L"--category",L"-1"},{L"--category",L"4294967296"},{L"--wat"},{L"--limit",L"1",L"--limit",L"2"},{L"--summary",L"--category",L"0"}}) {
            response = run(args); Monstey_Check(response.code == 2 && !response.json["ok"].as_bool(), "invalid command argument rejected as JSON");
        }
        response = run({L"--category",L"2"}); Monstey_Check(response.code == 1 && !response.json["ok"].as_bool(), "out of range category rejected");
        response = Monstey_Run(exe, root, {L"--help"}); Monstey_Check(response.code == 0 && response.json.contains("examples"), "JSON help documents exact commands");
        Monstey_Fixture narrowed;
        narrowed.categories = {{0,{{0,{{0,3,0,std::vector<std::uint32_t>(65536,0x41414141)}}}}}};
        const auto narrowed_path = root / L"narrowed.gadb";
        const auto narrow_bytes = narrowed.Serialize(); Monstey_WriteNewFile(narrowed_path,narrow_bytes);
        response = Monstey_Run(exe,root,{L"--input",narrowed_path.wstring(),L"--category",L"0",L"--record",L"0",L"--attribute",L"0"});
        Monstey_Check(response.code == 0 && response.json["consumed_bytes"].as_uint() == narrow_bytes.size() && response.json["inspection"]["page"]["total"].as_uint() == 0, "65536 physical words consumed while stored u16 value count is zero");
        for (std::size_t cut : std::array<std::size_t,5>{0,27,28,29,bytes.size()-4}) {
            const auto path = root / (L"short-"+std::to_wstring(cut)+L".gadb"); Monstey_WriteNewFile(path,std::span(bytes).first(cut));
            response = Monstey_Run(exe,root,{L"--input",path.wstring()});
            Monstey_Check(response.code == 1 && !response.json["ok"].as_bool() && response.json.contains("consumed_bytes"), "short read yields JSON and consumed boundary");
        }
        auto bad = bytes; bad[0] ^= 0xff; const auto bad_path = root/L"bad.gadb"; Monstey_WriteNewFile(bad_path,bad);
        response = Monstey_Run(exe,root,{L"--input",bad_path.wstring()}); Monstey_Check(response.code == 1 && response.json["load_status"].as_string() == "invalid_header", "wrong header rejected");
        Monstey_Check(Monstey_HashSha256(input) == before, "synthetic source unchanged across all CLI cases");

        const auto baseline_cases = Monstey_Cases, baseline_checks = Monstey_Checks;
        if (baseline_cases != 39 || baseline_checks != 209)
            throw std::runtime_error("The original 39 CLI cases / 209 checks must remain intact");
        Monstey_Fixture odd_wide;
        odd_wide.pool = {0,'s','a','m','e',0};
        odd_wide.pool.insert(odd_wide.pool.end(), {0x41,0x00,0x7f});
        odd_wide.categories = {{1,{{1,{{1,5,0,{6}}}}}}};
        const auto odd_path = root / L"D001-odd-wide-pool.gadb";
        const auto odd_bytes = odd_wide.Serialize();
        Monstey_Check(odd_bytes.size() == 77, "D001 exact synthetic packed fixture size");
        Monstey_WriteNewFile(odd_path,odd_bytes);
        const auto odd_before = Monstey_HashSha256(odd_path);
        for (auto cap : std::array<unsigned,3>{2,3,256}) {
            response = Monstey_Run(exe,root,{L"--input",odd_path.wstring(),L"--category",L"0",
                L"--record",L"0",L"--attribute",L"0",L"--string-bytes",std::to_wstring(cap)});
            Monstey_Check(response.code == 0 && response.json["ok"].as_bool(), "D001 CLI succeeds");
            const auto& wide_value = response.json["inspection"]["items"][0]["string"];
            Monstey_Check(wide_value["status"].as_string() == (cap == 2 ?
                "truncated_at_display_limit" : "unavailable_terminator_in_pool"),
                "D001 cap2 truncates; cap3/cap256 reach incomplete pool boundary");
            Monstey_Check(wide_value["encoding"].as_string() == "raw_utf16le_code_units_no_locale_conversion",
                "D001 raw UTF16LE code units are not a validated Unicode claim");
            Monstey_Check(wide_value["bytes_hex"].as_string() == "4100" &&
                wide_value["emitted_bytes"].as_uint() == 2, "D001 emits only the complete code unit");
            Monstey_Check(!wide_value["terminator_observed"].as_bool(), "D001 never invents a terminator");
            Monstey_Check(wide_value["identity"]["pointer_region"].as_string() == "serialized_pool",
                "D001 does not consume runtime graph bytes");
            Monstey_Check(response.json["source_sha256"].as_string() == odd_before &&
                response.json["source_unchanged"].as_bool(), "D001 source identity unchanged");
        }
        Monstey_Check(Monstey_HashSha256(odd_path) == odd_before, "D001 source remains read-only");

        const auto report = Monstey_Json::object({{"schema","Monstey.FearDbInspectorTests/1"},{"passed",true},{"cli_cases",Monstey_Cases},{"checks",Monstey_Checks},{"baseline_cli_cases",baseline_cases},{"baseline_checks",baseline_checks},{"odd_wide_cli_cases",3},{"fixtures","synthetic_CXX_only"},{"original_program_executed",false}});
        Monstey_WriteNewText(root/L"receipt.json",report.dump(2)+"\n"); std::cout << report.dump(2) << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
