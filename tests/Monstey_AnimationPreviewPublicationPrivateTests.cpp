#include "../src/Monstey_AnimationPreviewApi.cpp"
#include <fstream>
#include <iostream>
using namespace Monstey;
namespace
{
void Monstey_Check(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
void Monstey_ReplaceFixture(const std::filesystem::path &path, const Monstey_Bytes &bytes)
{
    std::ofstream f(Monstey_WindowsFilePath(path), std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Monstey_Check(bool(f), "Cannot replace private fixture");
}
std::string Monstey_Rejected(const std::function<void()> &f, const char *expected_code)
{
    try
    {
        f();
    }
    catch (const Monstey_Error &e)
    {
        Monstey_Check(e.code == expected_code, "Unexpected publication verification error");
        return e.what();
    }
    throw std::runtime_error("Expected verification failure");
}
} // namespace
int wmain(int argc, wchar_t **argv)
{
    try
    {
        Monstey_Check(argc == 2, "Usage AnimationPreviewPublicationPrivateTests NEW-directory");
        const std::filesystem::path directory = argv[1];
        Monstey_Check(!std::filesystem::exists(directory), "Directory exists");
        Monstey_RequireOfflineOutput(directory);
        std::filesystem::create_directories(directory);
        const auto source = directory / L"owned-input.fixture", output = directory / L"owned-output.bmp";
        const Monstey_Bytes input{1, 2, 3}, bitmap{0x42, 0x4d, 1, 2, 3}, competing{9, 8, 7, 6};
        Monstey_WriteNewFile(source, input);
        Monstey_WriteNewFile(output, bitmap);
        const std::vector<Monstey_AnimationPreviewApiSnapshot> snapshots{
            {source, Monstey_HashSha256(input), input.size(), false}};
        Monstey_AnimationPreviewApiVerifyPublished(output, bitmap, snapshots);
        Monstey_Check(Monstey_ReadFile(output) == bitmap, "Successful verification changed artifact");
        Monstey_ReplaceFixture(source, competing);
        const auto changed_source = Monstey_Rejected(
            [&] { Monstey_AnimationPreviewApiVerifyPublished(output, bitmap, snapshots); }, "source_changed");
        Monstey_Check(Monstey_ReadFile(output) == bitmap &&
                          changed_source.find("not removed or restored") != std::string::npos,
                      "Source change deleted output or hid preservation diagnostic");
        Monstey_ReplaceFixture(source, input);
        Monstey_ReplaceFixture(output, competing);
        const auto changed_output =
            Monstey_Rejected([&] { Monstey_AnimationPreviewApiVerifyPublished(output, bitmap, snapshots); },
                             "animation_preview");
        Monstey_Check(Monstey_ReadFile(output) == competing &&
                          changed_output.find("not removed or restored") != std::string::npos,
                      "Changed competing destination was deleted or overwritten");
        const auto absent = directory / L"absent-output.bmp";
        const auto missing = Monstey_Rejected(
            [&] { Monstey_AnimationPreviewApiVerifyPublished(absent, bitmap, snapshots); }, "read_failed");
        Monstey_Check(!std::filesystem::exists(absent) &&
                          missing.find("not removed or restored") != std::string::npos,
                      "Missing destination was recreated");
        Monstey_ReplaceFixture(source, competing);
        (void)Monstey_Rejected([&] { Monstey_AnimationPreviewApiUnchanged(snapshots); }, "source_changed");
        Monstey_Check(!std::filesystem::exists(absent), "Prepublication snapshot failure published output");
        const auto report =
            Monstey_Json::object({{"schema", "Monstey.AnimationPreviewPublicationPrivateTests/1"},
                                  {"groups", 5},
                                  {"failed", 0},
                                  {"published_own_artifact_preserved_on_source_change", true},
                                  {"competing_changed_destination_preserved", true},
                                  {"missing_destination_not_recreated", true},
                                  {"prepublication_changed_snapshot_rejected", true},
                                  {"destination_deletion_path", false},
                                  {"source_files_are_synthetic", true}});
        Monstey_WriteNewText(directory / L"report.json", report.dump(2));
        std::cout << report.dump(2) << '\n';
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
