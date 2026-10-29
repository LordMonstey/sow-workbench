#include "Monstey_Application.hpp"
#include "Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

class Monstey_ComScope
{
    HRESULT status_ = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  public:
    ~Monstey_ComScope()
    {
        if (SUCCEEDED(status_))
            CoUninitialize();
    }
};

} // namespace Monstey::Monstey_AppInternal

namespace Monstey
{

int Monstey_RunApplication(HINSTANCE instance, int show)
{
    std::filesystem::path Monstey_StartupReport;
    try
    {
        Monstey::Monstey_AppInternal::Monstey_ComScope apartment;
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_TAB_CLASSES |
                                                            ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&controls);
        int argc = 0;
        LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        Monstey::Monstey_Require(argv != nullptr, "Cannot read command line");
        std::filesystem::path schema = Monstey::Monstey_AppInternal::Monstey_DefaultSchema(), database, report,
                              argumentsPath, settingsPath;
        std::string smokeOperation, smokeExpectedError;
        int smokeDomain = 5;
        bool Monstey_SowAtomSmoke = false;
        bool smokeCloseDuringBuild = false, smokeStructure = false, smokeEffects = false, smokeSockets = false,
             smokePrefabs = false, smokeEnvelope = false, smokeExtras = false, smokeNested = false,
             smokeArchive = false, smokeStrings = false, smokeAnimation = false, smokeNativeProfile = false,
             smokeMaterial = false, smokeTexture = false, smokeAttachment = false, smokePreview = false,
             smokePreviewClose = false, smokePreviewAnimation = false, smokeStartupFailure = false,
             schemaExplicit = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::wstring_view name(argv[i]);
            if (name == L"--smoke-sow-atom")
            {
                Monstey_SowAtomSmoke = true;
                continue;
            }
            if (name == L"--smoke-native-profile")
            {
                smokeNativeProfile = true;
                continue;
            }
            if (name == L"--smoke-texture")
            {
                smokeTexture = true;
                continue;
            }
            if (name == L"--smoke-material")
            {
                smokeMaterial = true;
                continue;
            }
            if (name == L"--smoke-startup-failure")
            {
                smokeStartupFailure = true;
                continue;
            }
            if (name == L"--smoke-preview-animation" || name == L"--smoke-preview-animation-close")
            {
                smokePreview = smokePreviewAnimation = true;
                smokePreviewClose = name == L"--smoke-preview-animation-close";
                continue;
            }
            if (name == L"--smoke-preview" || name == L"--smoke-preview-close")
            {
                smokePreview = true;
                smokePreviewClose = name == L"--smoke-preview-close";
                continue;
            }
            if (name == L"--smoke-attachment")
            {
                smokeAttachment = true;
                continue;
            }
            if (name == L"--smoke-animation")
            {
                smokeAnimation = true;
                continue;
            }
            if (name == L"--smoke-archive")
            {
                smokeArchive = true;
                continue;
            }
            if (name == L"--smoke-strings")
            {
                smokeStrings = true;
                continue;
            }
            if (name == L"--smoke-structure")
            {
                smokeStructure = true;
                continue;
            }
            if (name == L"--smoke-effects")
            {
                smokeEffects = true;
                continue;
            }
            if (name == L"--smoke-sockets")
            {
                smokeSockets = true;
                continue;
            }
            if (name == L"--smoke-extras")
            {
                smokeExtras = true;
                continue;
            }
            if (name == L"--smoke-nested")
            {
                smokeNested = true;
                continue;
            }
            if (name == L"--smoke-envelope")
            {
                smokeEnvelope = true;
                continue;
            }
            if (name == L"--smoke-prefabs")
            {
                smokePrefabs = true;
                continue;
            }
            if (name == L"--smoke-close-during-build")
            {
                smokeCloseDuringBuild = true;
                continue;
            }
            if ((name == L"--database" || name == L"--schema" || name == L"--smoke-report" ||
                 name == L"--smoke-arguments" || name == L"--smoke-operation" || name == L"--smoke-domain" ||
                 name == L"--smoke-expect-error" || name == L"--settings") &&
                i + 1 < argc)
            {
                const std::filesystem::path value(argv[++i]);
                if (name == L"--database")
                    database = value;
                else if (name == L"--schema")
                {
                    schema = value;
                    schemaExplicit = true;
                }
                else if (name == L"--smoke-report")
                {
                    report = value;
                    Monstey_StartupReport = report;
                }
                else if (name == L"--smoke-arguments")
                    argumentsPath = value;
                else if (name == L"--settings")
                    settingsPath = value;
                else if (name == L"--smoke-operation")
                    smokeOperation = Monstey::Monstey_Utf8(value.wstring());
                else if (name == L"--smoke-expect-error")
                    smokeExpectedError = Monstey::Monstey_Utf8(value.wstring());
                else
                    smokeDomain = value == L"database" ? 0 : value == L"models" ? 4 : value == L"settings" ? 6 : 5;
            }
        }
        LocalFree(argv);
        Monstey::Monstey_AppInternal::Monstey_App app(instance, schema, settingsPath);
        app.initialDatabase = database;
        app.smokeReport = report;
        app.smokeOperation = smokeOperation;
        app.smokeExpectedError = smokeExpectedError;
        app.smokeDomain = Monstey_SowAtomSmoke ? 0 : smokeDomain;
        app.Monstey_SowAtomSmoke = Monstey_SowAtomSmoke;
        if (Monstey_SowAtomSmoke)
            Monstey::Monstey_Require(
                !report.empty() && database.empty() && smokeOperation.empty(),
                "SoW atom smoke requires a report and its own synthetic fixture, without database/operation arguments");
        app.smokeCloseDuringBuild = smokeCloseDuringBuild;
        app.smokeStructure = smokeStructure;
        app.smokeEffects = smokeEffects;
        app.smokeSockets = smokeSockets;
        app.smokePrefabs = smokePrefabs;
        app.smokeEnvelope = smokeEnvelope;
        app.smokeExtras = smokeExtras;
        app.smokeNested = smokeNested;
        app.smokeArchive = smokeArchive;
        app.smokeStrings = smokeStrings;
        app.smokeAnimation = smokeAnimation;
        app.smokeMaterial = smokeMaterial;
        app.smokeTexture = smokeTexture;
        app.smokeNativeProfile = smokeNativeProfile;
        app.smokeAttachment = smokeAttachment;
        app.smokePreview = smokePreview;
        app.smokePreviewClose = smokePreviewClose;
        app.smokePreviewAnimation = smokePreviewAnimation;
        app.smokeStartupFailure = smokeStartupFailure;
        if (schemaExplicit)
            app.fxSchema = schema;
        if (!argumentsPath.empty())
            app.smokeArguments = Monstey::Monstey_Json::Parse(Monstey::Monstey_ReadText(argumentsPath, 1024 * 1024));
        if (!report.empty())
        {
            Monstey::Monstey_Require(!database.empty() || !smokeOperation.empty() || smokeStructure || smokeEffects ||
                                         smokePrefabs || smokeEnvelope || smokeNested || smokeArchive || smokeStrings ||
                                         Monstey_SowAtomSmoke,
                                     "GUI smoke test needs --database or --smoke-operation");
            app.smokeDirectory = std::filesystem::absolute(report).parent_path() /
                                 (report.stem().wstring() + L"-artifacts-" + std::to_wstring(GetCurrentProcessId()));
            std::filesystem::create_directories(app.smokeDirectory);
            auto smokeSource = database;
            if (smokeSource.empty() && app.smokeArguments.contains("source"))
                smokeSource = std::filesystem::path(Monstey::Monstey_Wide(app.smokeArguments.at("source").as_string()));
            if (!smokeSource.empty())
                app.smoke =
                    Monstey::Monstey_Json::object({{"source_sha256", Monstey::Monstey_HashSha256(smokeSource)}});
        }
        app.Create((smokeNativeProfile || Monstey_SowAtomSmoke) ? SW_SHOWNOACTIVATE : show);
        ACCEL shortcuts[] = {{FCONTROL | FVIRTKEY, 'O', Monstey::Monstey_AppInternal::Monstey_Open},
                             {FCONTROL | FVIRTKEY, 'S', Monstey::Monstey_AppInternal::Monstey_Save},
                             {FCONTROL | FVIRTKEY, 'Z', Monstey::Monstey_AppInternal::Monstey_Undo},
                             {FCONTROL | FVIRTKEY, 'Y', Monstey::Monstey_AppInternal::Monstey_Redo}};
        const auto accelerator = CreateAcceleratorTableW(shortcuts, static_cast<int>(std::size(shortcuts)));
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0)
        {
            if (app.texturePreview && app.texturePreview->RouteMessage(message))
                continue;
            if (app.modelPreview && app.modelPreview->RouteMessage(message))
                continue;
            if (app.editor && app.editor->window && IsDialogMessageW(app.editor->window, &message))
                continue;
            if (!TranslateAcceleratorW(app.window, accelerator, &message))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        if (accelerator)
            DestroyAcceleratorTable(accelerator);
        return app.smokeFailed ? 1 : 0;
    }
    catch (const std::exception &e)
    {
        if (!Monstey_StartupReport.empty())
        {
            unsigned owned = 0;
            EnumThreadWindows(
                GetCurrentThreadId(),
                [](HWND hwnd, LPARAM data) -> BOOL
                {
                    wchar_t name[256]{};
                    GetClassNameW(hwnd, name, 256);
                    if (std::wstring_view(name).starts_with(L"Monstey_"))
                        ++*reinterpret_cast<unsigned *>(data);
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&owned));
            try
            {
                if (!std::filesystem::exists(Monstey_StartupReport))
                    Monstey::Monstey_WriteNewText(Monstey_StartupReport,
                                                  Monstey::Monstey_Json::object({{"status", "FAIL"},
                                                                                 {"phase", "startup"},
                                                                                 {"error", e.what()},
                                                                                 {"owned_windows_after_unwind", owned},
                                                                                 {"native_gui", true},
                                                                                 {"implementation", "C++20/Win32"}})
                                                          .dump(2) +
                                                      "\n");
            }
            catch (...)
            {
            }
            return 1;
        }
        MessageBoxW(nullptr, Monstey::Monstey_Wide(e.what()).c_str(), L"Monstey Workbench", MB_OK | MB_ICONERROR);
        return 1;
    }
}

} // namespace Monstey
