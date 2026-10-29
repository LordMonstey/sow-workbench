#include "Monstey_AnimationPreviewApi.hpp"
#include "Monstey_Runtime.hpp"
#include <iostream>
using namespace Monstey;
int wmain(int argc, wchar_t **argv)
{
    try
    {
        Monstey_Require(argc == 5, "Usage AnimationPreviewRuntimeTests NEW-directory mesh skel anix");
        const std::filesystem::path directory = argv[1], mesh = argv[2], skel = argv[3], anix = argv[4];
        Monstey_Require(!std::filesystem::exists(directory), "Directory exists");
        std::filesystem::create_directories(directory);
        const auto catalog = Monstey_Runtime::Catalog();
        unsigned found = 0;
        for (const auto &row : catalog.as_array())
            if (row["name"].as_string() == "model_animation_preview_render")
            {
                ++found;
                Monstey_Require(!row["read_only"].as_bool() && row["implementation"].as_string() == "C++20",
                                "Runtime operation descriptor differs");
                Monstey_Require(row["inputSchema"]["required"] ==
                                        Monstey_Json::array({"source", "skeleton", "animation", "clip",
                                                             "tick_ms", "output"}) &&
                                    row["inputSchema"]["properties"].contains("expected_generation") &&
                                    row["inputSchema"]["properties"].contains("expected_revision"),
                                "Runtime common guards/schema missing");
            }
        Monstey_Require(found == 1 && catalog.size() == 227,
                        "Runtime animated-preview descriptor count differs");
        Monstey_Runtime runtime(directory / L"unused-schema.json", directory / L"settings.json");
        const auto hashes = std::array<std::string, 3>{Monstey_HashSha256(mesh), Monstey_HashSha256(skel),
                                                       Monstey_HashSha256(anix)};
        auto args = Monstey_Json::object({{"source", Monstey_Utf8(mesh.wstring())},
                                          {"skeleton", Monstey_Utf8(skel.wstring())},
                                          {"animation", Monstey_Utf8(anix.wstring())},
                                          {"clip", "walk"},
                                          {"tick_ms", 500},
                                          {"output", Monstey_Utf8((directory / L"runtime.bmp").wstring())},
                                          {"width", 64},
                                          {"height", 64},
                                          {"expected_generation", 0},
                                          {"expected_revision", 0}});
        const auto request = [](const Monstey_Json &value)
        {
            return Monstey_Json::object(
                {{"operation", "model_animation_preview_render"}, {"arguments", value}});
        };
        const auto success = runtime.Call(request(args));
        Monstey_Require(success["ok"].as_bool() && success["revision"].as_uint() == 1 &&
                            success["result"]["host_ordinary_cpu_geometry"].as_bool() &&
                            success["result"]["tick_ms"].as_number() == 500,
                        "Runtime route failed or common guards leaked into domain");
        Monstey_Require(Monstey_U16(Monstey_ReadFile(directory / L"runtime.bmp"), 0) == 0x4d42,
                        "Runtime output is not BMP");
        args["output"] = Monstey_Utf8((directory / L"rejected.bmp").wstring());
        args["expected_revision"] = 1;
        args["expected_animation_sha256"] = std::string(64, '0');
        const auto hash_rejected = runtime.Call(request(args));
        Monstey_Require(!hash_rejected["ok"].as_bool() &&
                            hash_rejected["error"]["code"].as_string() == "source_hash_mismatch" &&
                            hash_rejected["revision"].as_uint() == 1 &&
                            !std::filesystem::exists(directory / L"rejected.bmp"),
                        "Runtime hash rejection mutated revision/output");
        args.as_object().erase("expected_animation_sha256");
        args["tick_ms"] = "500";
        const auto type_rejected = runtime.Call(request(args));
        Monstey_Require(!type_rejected["ok"].as_bool() &&
                            type_rejected["error"]["code"].as_string() == "invalid_argument_type" &&
                            type_rejected["revision"].as_uint() == 1,
                        "Runtime type validation differs");
        args["tick_ms"] = 500;
        args["expected_revision"] = 0;
        const auto stale_rejected = runtime.Call(request(args));
        Monstey_Require(!stale_rejected["ok"].as_bool() && stale_rejected["revision"].as_uint() == 1 &&
                            !std::filesystem::exists(directory / L"rejected.bmp"),
                        "Stale common guard published output or changed revision");
        args["expected_revision"] = 1;
        args["clip"] = "missing";
        const auto clip_rejected = runtime.Call(request(args));
        Monstey_Require(!clip_rejected["ok"].as_bool() && clip_rejected["revision"].as_uint() == 1 &&
                            !std::filesystem::exists(directory / L"rejected.bmp"),
                        "Missing clip changed runtime state");
        Monstey_Require(Monstey_HashSha256(mesh) == hashes[0] && Monstey_HashSha256(skel) == hashes[1] &&
                            Monstey_HashSha256(anix) == hashes[2],
                        "Runtime changed source files");
        const auto report =
            Monstey_Json::object({{"schema", "Monstey.AnimationPreviewRuntimeTests/1"},
                                  {"groups", 6},
                                  {"failed", 0},
                                  {"catalog_operations", static_cast<std::uint64_t>(catalog.size())},
                                  {"descriptors", found},
                                  {"success", success},
                                  {"hash_rejected", hash_rejected},
                                  {"type_rejected", type_rejected},
                                  {"stale_rejected", stale_rejected},
                                  {"clip_rejected", clip_rejected},
                                  {"sources_unchanged", true}});
        Monstey_WriteNewText(directory / L"report.json", report.dump(2));
        std::cout << "Runtime animation preview6 groups PASS; catalog=" << catalog.size() << '\n';
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
