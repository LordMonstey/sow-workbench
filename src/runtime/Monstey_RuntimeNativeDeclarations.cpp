#include "Monstey_RuntimeNativeDeclarations.hpp"
#include "Monstey_RuntimeArguments.hpp"
#include "Monstey_GadbClassCache.hpp"
#include "Monstey_FxLibrary.hpp"
#include <algorithm>
#include <array>

namespace Monstey
{
namespace Monstey_RuntimeDetail
{
Monstey_Json Monstey_BuildWithNativeDeclarations(Monstey_Gadb &database, const Monstey_Json &arguments,
                                                 Monstey_Settings &settings, bool effects)
{
    using J = Monstey_Json;
    const auto output = Monstey_Path(arguments, "output");
    const bool hasPath = arguments.contains("native_declarations"),
               hasHash = arguments.contains("expected_native_declarations_sha256");
    if (hasPath != hasHash)
        throw Monstey_Error("invalid_arguments",
                            "native_declarations and expected_native_declarations_sha256 must be supplied together");
    auto values = arguments;
    values.as_object().erase("expected_generation");
    values.as_object().erase("expected_revision");
    if (!hasPath)
        return effects ? Monstey_FxLibraryNativeCall(database, "fx_build", values) : database.Build(output);
    const auto profile = std::filesystem::absolute(Monstey_Path(arguments, "native_declarations"));
    const auto &expected = arguments.at("expected_native_declarations_sha256").as_string();
    if (expected.size() != 64 || !std::all_of(expected.begin(), expected.end(),
                                              [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
        throw Monstey_Error("invalid_arguments",
                            "Expected native declaration SHA256 must be 64 lowercase hex characters");
    Monstey_RequireOfflineOutput(output, std::array{profile});
    const auto bytes = Monstey_ReadFile(profile, 128ull * 1024 * 1024);
    const auto digest = Monstey_HashSha256(bytes);
    if (digest != expected)
        throw Monstey_Error("source_hash_mismatch", "Native declaration snapshot SHA256 mismatch");
    const auto reflection = J::Parse(std::string(bytes.begin(), bytes.end()));
    const Monstey_GadbClassCache prepared(reflection);
    settings.Protect(profile);
    const auto provenance = J::object({{"path", Monstey_Utf8(profile.wstring())},
                                       {"sha256", digest},
                                       {"bytes", bytes.size()},
                                       {"input_contract", "loaded_pinned_snapshot"},
                                       {"path_rechecked_after_publication", false}});
    values.as_object().erase("native_declarations");
    values.as_object().erase("expected_native_declarations_sha256");
    auto report = effects ? Monstey_FxLibraryNativeCall(database, "fx_build", values, &prepared)
                          : database.Build(output, prepared);
    report["native_declaration_profile"] = provenance;
    return report;
}
} // namespace Monstey_RuntimeDetail
} // namespace Monstey
