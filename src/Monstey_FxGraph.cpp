#include "Monstey_FxGraph.hpp"
#include <cmath>
#include <set>
namespace Monstey {
namespace {
using J = Monstey_Json;
std::string Monstey_GHash(const J &value) {
    const auto text = value.dump();
    return Monstey_HashSha256(std::span(reinterpret_cast<const std::uint8_t *>(text.data()), text.size()));
}
void Monstey_GRequire(bool value, std::string_view message) {
    if (!value)
        throw Monstey_Error("fx_graph", std::string(message));
}
J Monstey_GState(Monstey_Gadb &db) {
    auto state = db.ProjectMetadata().value("fx_graph", J::object({{"version", 1}, {"paste_sequence", 1u}}));
    Monstey_GRequire(state.is_object() && state["version"].as_uint() == 1 &&
                         state["paste_sequence"].as_uint() > 0,
                     "Unsupported FX clipboard project metadata");
    return state;
}
void Monstey_GSave(Monstey_Gadb &db, const J &state) {
    auto metadata = db.ProjectMetadata();
    metadata["fx_graph"] = state;
    db.SetProjectMetadata(metadata);
}
std::string Monstey_GKey(Monstey_Gadb &db, std::string_view selector) {
    const auto ref = Monstey_Hex(db.Resolve(selector));
    const auto record = db.Metadata(ref);
    Monstey_GRequire(!record["pending_delete"].as_bool() &&
                         record["category"].as_string().starts_with("ClientFX."),
                     "Graph selection must contain undeleted ClientFX elements");
    return ref;
}
void Monstey_GCheck(const J &clipboard) {
    Monstey_GRequire(clipboard.is_object() && clipboard["format"].as_string() == "Monstey_FxGraphClipboard" &&
                         clipboard["version"].as_uint() == 1 && clipboard["selected"].is_array() &&
                         !clipboard["selected"].empty() && clipboard["selected"].size() <= 65535,
                     "Invalid FX graph clipboard");
    auto body = clipboard;
    body.as_object().erase("sha256");
    Monstey_GRequire(Monstey_GHash(body) == clipboard["sha256"].as_string(),
                     "FX clipboard content hash changed");
}
J Monstey_GClipboard(Monstey_Gadb &db, const J &args) {
    if (args.contains("clipboard")) {
        Monstey_GCheck(args["clipboard"]);
        return args["clipboard"];
    }
    const auto state = Monstey_GState(db);
    Monstey_GRequire(state.contains("clipboard"), "Copy or load a graph before pasting");
    Monstey_GCheck(state["clipboard"]);
    return state["clipboard"];
}
} // namespace
Monstey_Json Monstey_FxGraphCapture(Monstey_Gadb &db, const J &args) {
    Monstey_GRequire(args.is_object() && args.contains("records") && args["records"].is_array() &&
                         !args["records"].empty() && args["records"].size() <= 65535,
                     "Graph capture needs 1..65535 selected record selectors");
    Monstey_GRequire(db.Describe()["format"].as_string() == "LTFX8/GADB17", "FX capture requires LTFX8");
    auto selected = J::array(), unique = J::array();
    std::set<std::string> seen;
    for (const auto &selector : args["records"].as_array()) {
        const auto ref = Monstey_GKey(db, selector.as_string());
        selected.as_array().push_back(ref);
        if (seen.insert(ref).second)
            unique.as_array().push_back(ref);
    }
    const auto snapshot = db.ExportRecords(
        unique, J::object({{"include_references", args.value("include_references", false)}}));
    auto clipboard = J::object({{"format", "Monstey_FxGraphClipboard"},
                                {"version", 1},
                                {"source_sha256", db.Describe()["source_sha256"]},
                                {"source_state_sha256", Monstey_GHash(db.Changes())},
                                {"selected", selected},
                                {"snapshot", snapshot},
                                {"physical_records", static_cast<std::uint64_t>(seen.size())}});
    clipboard["sha256"] = Monstey_GHash(clipboard);
    return clipboard;
}
Monstey_Json Monstey_FxGraphCall(Monstey_Gadb &db, std::string_view op, const J &args) {
    Monstey_GRequire(args.is_object(), "FX graph arguments must be an object");
    if (op == "fx_graph_capture")
        return Monstey_FxGraphCapture(db, args);
    if (op == "fx_clipboard")
        return Monstey_GState(db).value("clipboard", J(nullptr));
    if (op == "fx_graph_save") {
        const auto clipboard =
            args.contains("records") ? Monstey_FxGraphCapture(db, args) : Monstey_GClipboard(db, args);
        const auto output = std::filesystem::path(Monstey_Wide(args["output"].as_string()));
        Monstey_RequireOfflineOutput(output);
        Monstey_WriteNewText(output, clipboard.dump(2) + "\n");
        return J::object({{"output", Monstey_Utf8(output.wstring())},
                          {"sha256", Monstey_HashSha256(output)},
                          {"clipboard_sha256", clipboard["sha256"]}});
    }
    J detail;
    const auto report = db.Transaction([&](Monstey_Gadb &) {
        auto state = Monstey_GState(db);
        if (op == "fx_copy_keys" || op == "fx_cut_keys") {
            const auto clipboard = Monstey_FxGraphCapture(db, args);
            state["clipboard"] = clipboard;
            if (op == "fx_cut_keys") {
                std::set<std::string> unique;
                for (const auto &key : clipboard["selected"].as_array())
                    unique.insert(key.as_string());
                for (const auto &key : unique)
                    Monstey_FxWorkflowCall(
                        db, "fx_delete_element",
                        J::object({{"record", key}, {"replacement", args.value("replacement", J(nullptr))}}));
            }
            Monstey_GSave(db, state);
            detail = clipboard;
            return;
        }
        if (op == "fx_graph_load") {
            const auto source = std::filesystem::path(Monstey_Wide(args["source"].as_string()));
            const auto bytes = Monstey_ReadFile(source);
            const auto hash = Monstey_HashSha256(bytes);
            Monstey_GRequire(args.contains("expected_sha256") && hash == args["expected_sha256"].as_string(),
                             "Graph file differs from the expected source snapshot");
            const auto clipboard = J::Parse(std::string(bytes.begin(), bytes.end()));
            Monstey_GCheck(clipboard);
            state["clipboard"] = clipboard;
            Monstey_GSave(db, state);
            detail = J::object({{"source_sha256", hash}, {"clipboard_sha256", clipboard["sha256"]}});
            return;
        }
        Monstey_GRequire(op == "fx_paste_keys" || op == "fx_import_keys",
                         "Unknown native FX graph operation");
        const auto clipboard = Monstey_GClipboard(db, args);
        double time = 0;
        if (op == "fx_paste_keys") {
            Monstey_GRequire(args.contains("time"), "Original paste requires a cursor time in seconds");
            time = args["time"].as_number();
            Monstey_GRequire(std::isfinite(time), "Cursor time is nonfinite");
        }
        const auto sequence = state["paste_sequence"].as_uint();
        Monstey_GRequire(sequence < UINT32_MAX, "FX paste sequence exhausted");
        auto options = J::object(
            {{"name_prefix", args.value("name_prefix", "Monstey_Paste_" + std::to_string(sequence) + "_")},
             {"bindings", args.value("bindings", J::object())}});
        if (args.contains("names"))
            options["names"] = args["names"];
        const auto imported = db.ImportRecords(clipboard["snapshot"], options);
        auto selected = J::array();
        std::set<std::string> timed;
        std::size_t offset = 0;
        for (const auto &old : clipboard["selected"].as_array()) {
            Monstey_GRequire(imported["reference_map"].contains(old.as_string()),
                             "Imported graph lost a selected record");
            const auto key = Monstey_GKey(db, imported["reference_map"][old.as_string()].as_string());
            if (op == "fx_paste_keys" && timed.insert(key).second)
                Monstey_FxWorkflowCall(db, "fx_retime_key",
                                       J::object({{"record", key}, {"start", time - 1}, {"end", time + 1}}));
            auto link = J::object({{"record", key}});
            if (args.contains("track"))
                link["track"] = args["track"];
            if (args.contains("index"))
                link["index"] = args["index"].as_uint() + offset;
            Monstey_FxWorkflowCall(db, "fx_link_key", link);
            selected.as_array().push_back(key);
            ++offset;
        }
        state["paste_sequence"] = sequence + 1;
        Monstey_GSave(db, state);
        detail = J::object({{"selected", selected},
                            {"imported", imported},
                            {"timing", op == "fx_paste_keys" ? "cursor_minus_plus_one_second" : "preserved"},
                            {"physical_copy", true},
                            {"native_parent_attachment_created", false}});
    });
    auto result = report;
    result["operation"] = std::string(op);
    result["result"] = detail;
    return result;
}
} // namespace Monstey
