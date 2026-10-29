#include "Monstey_FearDbInspector.hpp"
#include <array>
#include <charconv>
#include <iostream>
#include <set>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
using namespace Monstey;
namespace {
std::uint32_t Monstey_Number(const std::wstring& text) {
    const auto utf8 = Monstey_Utf8(text);
    std::uint32_t number = 0;
    const auto [end, error] = std::from_chars(utf8.data(), utf8.data() + utf8.size(), number);
    if (error != std::errc{} || end != utf8.data() + utf8.size())
        throw std::invalid_argument("ordinal/page argument must be an unsigned decimal uint32");
    return number;
}
const char* Monstey_Status(Monstey_FearDbPackedStatus status) {
    switch (status) {
    case Monstey_FearDbPackedStatus::loaded: return "loaded";
    case Monstey_FearDbPackedStatus::header_read_failed: return "header_read_failed";
    case Monstey_FearDbPackedStatus::invalid_header: return "invalid_header";
    case Monstey_FearDbPackedStatus::pool_read_failed: return "pool_read_failed";
    case Monstey_FearDbPackedStatus::graph_read_failed: return "graph_read_failed";
    }
    return "unknown";
}
Monstey_Json Monstey_Help() {
    return Monstey_Json::object({{"tool", "Monstey_FearDbInspector"}, {"schema", "Monstey.FearDbInspectorHelp/1"},
        {"usage", "Monstey_FearDbInspector.exe --input <original.gadb> [--category N [--record N [--attribute N]]] [--offset N] [--limit N] [--string-bytes N] [--output <NEW.json>]"},
        {"summary_usage", "Monstey_FearDbInspector.exe --input <original.gadb> --summary [--output <NEW.json>]"},
        {"help_usage", "Monstey_FearDbInspector.exe --help"},
        {"selection", "No filter lists categories; category lists records; category+record lists attributes; category+record+attribute lists selected values. Ordinals are zero-based. No name/locale lookup."},
        {"pagination", "offset defaults to 0; limit defaults to 50, range 1..10000. Each page has total, emitted, has_more, next_offset. Offset beyond count emits an empty page."},
        {"strings", "string-bytes defaults to 256, range 2..1048576. Names/strings retain hex bytes, escaped ASCII display, bounded-pool and terminator status. Wide strings retain little-endian code-unit bytes. No locale guessing."},
        {"output", "JSON stdout always. --output additionally writes exactly the same success JSON to a new offline file; existing outputs and source aliases are rejected."},
        {"exit_codes", "0 success/help; 1 load, input, output or inspection failure; 2 invalid command arguments"},
        {"limits", "Legacy FEAR packed GADB version 3 only; input read cap 512 MiB. Accepted loader determines allocation and consumed bytes. Runtime counts/types are narrowed native fields. Runtime words follow relocation/link fixup; full original packed fields are not retained. Diagnostic owned identities are not native pointers. No original native calling ABI, portable graph/project export, writer, import, or full-file losslessness claim."},
        {"examples", Monstey_Json::array({
            "Monstey_FearDbInspector.exe --input D:\\Data\\original.gadb --limit 20",
            "Monstey_FearDbInspector.exe --input D:\\Data\\original.gadb --category 0 --record 3 --offset 20 --limit 20",
            "Monstey_FearDbInspector.exe --input D:\\Data\\original.gadb --category 0 --record 3 --attribute 2 --limit 20 --output D:\\Reports\\new-inspection.json",
            "Monstey_FearDbInspector.exe --input D:\\Data\\original.gadb --summary"})}});
}
}
int wmain(int argc, wchar_t** argv) {
    _setmode(_fileno(stdout), _O_BINARY);
    auto report = Monstey_Json::object({{"schema", "Monstey.FearDbInspection/1"}, {"ok", false}});
    int failure_code = 2;
    try {
        if (argc == 2 && std::wstring_view(argv[1]) == L"--help") {
            std::cout << Monstey_Help().dump(2) << '\n'; return 0;
        }
        Monstey_FearInspectorOptions options;
        std::filesystem::path source, output;
        std::set<std::wstring> seen;
        for (int i = 1; i < argc; ++i) {
            const std::wstring flag = argv[i];
            if (!seen.insert(flag).second) throw std::invalid_argument("duplicate command option");
            if (flag == L"--summary") { options.summary = true; continue; }
            if (flag != L"--input" && flag != L"--output" && flag != L"--category" && flag != L"--record" &&
                flag != L"--attribute" && flag != L"--offset" && flag != L"--limit" && flag != L"--string-bytes")
                throw std::invalid_argument("unknown option; use --help");
            if (++i >= argc) throw std::invalid_argument("missing command option argument");
            const std::wstring value = argv[i];
            if (flag == L"--input") source = value;
            else if (flag == L"--output") output = value;
            else if (flag == L"--category") options.category = Monstey_Number(value);
            else if (flag == L"--record") options.record = Monstey_Number(value);
            else if (flag == L"--attribute") options.attribute = Monstey_Number(value);
            else if (flag == L"--offset") options.offset = Monstey_Number(value);
            else if (flag == L"--limit") options.limit = Monstey_Number(value);
            else options.string_bytes = Monstey_Number(value);
        }
        if (source.empty()) throw std::invalid_argument("--input is required; use --help");
        if (options.limit == 0 || options.limit > 10000 || options.string_bytes < 2 || options.string_bytes > 1048576 ||
            (options.record && !options.category) || (options.attribute && !options.record) ||
            (options.summary && (options.category || options.record || options.attribute)))
            throw std::invalid_argument("invalid filter hierarchy or display bounds; use --help");
        failure_code = 1;
        source = std::filesystem::absolute(Monstey_WindowsFilePath(source));
        const auto before = Monstey_HashSha256(source);
        report["source_path"] = Monstey_Utf8(source.wstring());
        report["source_sha256"] = before;
        const auto bytes = Monstey_ReadFile(source);
        if (Monstey_HashSha256(bytes) != before) throw std::runtime_error("source changed during read");
        report["source_bytes"] = static_cast<std::uint64_t>(bytes.size());
        // Stable diagnostic caller name, not a purported name serialized by this format.
        const std::array<std::uint8_t, 15> name{'i','n','s','p','e','c','t','e','d','.','g','a','d','b',0};
        Monstey_FearDbPackedInput input{bytes};
        Monstey_FearDbPackedLoad loaded;
        try { loaded = Monstey_FearDbLoadPacked(input, name, 0x24000000); }
        catch (...) {
            report["consumed_bytes"] = static_cast<std::uint64_t>(input.position);
            report["trailing_bytes"] = static_cast<std::uint64_t>(bytes.size() - input.position);
            report["read_calls"] = input.read_calls;
            throw;
        }
        report["load_status"] = Monstey_Status(loaded.status);
        report["consumed_bytes"] = static_cast<std::uint64_t>(input.position);
        report["trailing_bytes"] = static_cast<std::uint64_t>(bytes.size() - input.position);
        report["read_calls"] = input.read_calls;
        if (!loaded.image) throw std::runtime_error("accepted packed loader did not produce an image");
        const auto& image = *loaded.image;
        report["allocation_bytes"] = static_cast<std::uint64_t>(image.allocation.size());
        report["used_bytes"] = image.used_bytes;
        report["serialized_pool_bytes"] = image.database - image.base;
        report["inspection"] = Monstey_FearInspectPackedImage(image, options);
        const auto after = Monstey_HashSha256(source);
        if (before != after) throw std::runtime_error("source changed during inspection");
        report["source_unchanged"] = true;
        report["diagnostic_caller_name"] = "inspected.gadb";
        report["ok"] = true;
        const auto text = report.dump(2) + "\n";
        if (!output.empty()) {
            const std::array<std::filesystem::path, 1> sources{source};
            Monstey_RequireOfflineOutput(output, sources);
            Monstey_WriteNewText(output, text);
        }
        std::cout << text;
        return 0;
    } catch (const Monstey_FearDbFault& error) {
        report["ok"] = false;
        report["error"] = error.what();
        report["fault_identity"] = Monstey_Hex(error.address);
        report["fault_width"] = error.width;
        report["fault_writing_owned_image"] = error.writing;
    } catch (const std::exception& error) {
        report["ok"] = false;
        report["error"] = error.what();
    }
    std::cout << report.dump(2) << '\n';
    return failure_code;
}
