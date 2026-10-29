#include "Monstey_WdscAuxiliary.hpp"
#include <array>
#include <iostream>
int wmain(int argc, wchar_t **argv)
{
    try
    {
        if (argc < 2 || argc > 3)
        {
            std::cerr << "Usage: Monstey_WdscAuxiliaryProbe SOURCE.wldclnt [NEW-REPORT.json]\n";
            return 2;
        }
        const std::filesystem::path source(argv[1]);
        auto report = Monstey::Monstey_WdscAuxiliaryInspectFile(source);
        if (argc == 3)
        {
            const std::filesystem::path output(argv[2]);
            const std::array<std::filesystem::path, 1> protected_paths{source};
            Monstey::Monstey_RequireOfflineOutput(output, protected_paths);
            Monstey::Monstey_WriteNewText(output, report.dump(2));
            std::cout << "Saved read-only evidence: " << Monstey::Monstey_Utf8(output.wstring()) << '\n';
        }
        else
        {
            std::cout << report.dump(2) << '\n';
        }
        return 0;
    }
    catch (const Monstey::Monstey_Error &error)
    {
        std::cerr << error.code << ": " << error.what() << '\n';
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
    }
    return 1;
}