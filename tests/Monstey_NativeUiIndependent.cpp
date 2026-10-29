// Independent in-process UI test linked to the current application modules.
#include "../src/app/Monstey_AppState.hpp"
#include <iostream>

using namespace Monstey;
using namespace Monstey::Monstey_AppInternal;

namespace {
class Monstey_ComScope {
  HRESULT status_ = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
public:
  ~Monstey_ComScope() {
    if (SUCCEEDED(status_))
      CoUninitialize();
  }
};
unsigned checks = 0, groups = 0;
void Check(bool v, const char *m) {
  ++checks;
  Monstey_Require(v, m);
}
template <class F> void Until(F predicate, const char *message) {
  const auto end = GetTickCount64() + 15000;
  do {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message != WM_QUIT) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }
    if (predicate())
      return;
    Sleep(1);
  } while (GetTickCount64() < end);
  throw std::runtime_error(message);
}
void Idle(Monstey_App &a) {
  Until(
      [&] {
        std::lock_guard lock(a.mutex);
        return a.jobs.empty() && a.results.empty() && !a.active.load();
      },
      "Owned GUI did not become idle");
}
J Good(Monstey_App &a, const char *op, J args = J::object()) {
  auto p = a.runtime->Call(J::object({{"operation", op}, {"arguments", args}}));
  Check(p.at("ok").as_bool(), "Owned review Runtime fixture call failed");
  a.generation = p.at("generation").as_uint();
  a.revision = p.at("revision").as_uint();
  return p;
}
void NativeReady(Monstey_App &a) {
  Until(
      [&] {
        return a.Monstey_DatabaseForm() && a.rowData.size() == 2 &&
               !a.selectedOperation.empty();
      },
      "Native menu route did not load two operations");
  Idle(a);
}
void NativeMenu(Monstey_App &a, int id) {
  SendMessageW(a.window, WM_COMMAND, id, 0);
  NativeReady(a);
}
void Stable(Monstey_App &a, const J &before, const J &changes) {
  Idle(a);
  auto b = Good(a, "describe"), c = Good(a, "database_changes");
  Check(b.at("result") == before.at("result") &&
            c.at("result") == changes.at("result"),
        "Native UI changed actual project/history");
  Check(b.at("generation") == before.at("generation") &&
            b.at("revision") == before.at("revision"),
        "Native UI changed Runtime counters");
}
} // namespace
int wmain(int argc, wchar_t **argv) {
  try {
    Check(argc == 2, "Need NEW_REPORT");
    const std::filesystem::path report = argv[1];
    const auto settings =
        report.parent_path() / L"absent-independent-settings.json";
    Monstey_ComScope com;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES |
                                                        ICC_TREEVIEW_CLASSES |
                                                        ICC_TAB_CLASSES};
    Check(InitCommonControlsEx(&controls) != FALSE, "Common controls failed");
    Monstey_App app(GetModuleHandleW(nullptr), {}, settings);
    app.Create(SW_HIDE);
    Idle(app);
    const auto noDb = Good(app, "describe");
    NativeMenu(app, Monstey_NativeBanksMenu);
    Check(app.selectedOperation.at("name") == J("database_native_banks"),
          "Banks menu failed without Database");
    NativeMenu(app, Monstey_NativeClassMenu);
    Check(app.selectedOperation.at("name") == J("database_native_class") &&
              app.fieldData.size() == 5,
          "Class menu failed without Database");
    Check(app.sourceNames[0].empty(),
          "Native PE operation occupied database source label");
    SendMessageW(app.window, WM_COMMAND, Monstey_DatabaseRecordsMenu, 0);
    Idle(app);
    Check(!app.databaseNativeMode && app.tab == 0 && app.rowData.empty(),
          "Back without Database did not restore empty records");
    auto noDbAfter = Good(app, "describe");
    Check(noDbAfter.at("result") == noDb.at("result") &&
              noDbAfter.at("generation") == noDb.at("generation") &&
              noDbAfter.at("revision") == noDb.at("revision"),
          "Native menu created or changed a database");
    ++groups;
    auto seed = Good(app, "database_new");
    app.UpdateState(seed.at("result"), 0);
    Good(app, "database_create_category",
         J::object({{"schema_name", "Monstey.Review.Ui"},
                    {"name", "Review dirty records"}}));
    for (unsigned i = 0; i < 270; ++i) {
      const auto name = "Keep_" + std::to_string(1000 + i);
      seed = Good(app, "database_create_record",
                  J::object({{"category_index", 0}, {"name", name}}));
    }
    app.UpdateState(seed.at("result"), 0);
    TabCtrl_SetCurSel(app.tabs, 0);
    app.TabChanged();
    Until([&] { return !app.metadata.empty() && !app.selector.empty(); },
          "Initial records did not load");
    Idle(app);
    SetWindowTextW(app.query, L"Keep_");
    app.offset = 250;
    app.category = 0;
    app.LoadRecords();
    Until([&] { return app.rowData.size() == 20 && !app.metadata.empty(); },
          "Nonzero record page did not load");
    Idle(app);
    Monstey_ListSelect(app.records, 5);
    app.ReadRecord(5);
    Until(
        [&] {
          return !app.metadata.empty() &&
                 app.selector == app.rowData[5].at("reference").as_string();
        },
        "Selected last-page record did not load");
    Idle(app);
    const auto selector = app.selector;
    for (const char *name : {"ReviewA", "ReviewB", "ReviewC"})
      seed = Good(app, "database_add_field",
                  J::object({{"record", selector},
                             {"field", name},
                             {"native_type", 3},
                             {"value", 42}}));
    seed = Good(app, "database_undo");
    app.UpdateState(seed.at("result"), 0);
    app.ReadRecord(5);
    Until([&] { return app.fieldData.size() == 2; },
          "Review fields did not refresh");
    Idle(app);
    Monstey_ListSelect(app.fields, 1);
    const auto savedField = ListView_GetNextItem(app.fields, -1, LVNI_SELECTED);
    const auto savedSource = app.sourceNames[0];
    const auto savedFields = app.fieldData;
    const auto savedMetadata = app.metadata;
    const auto savedCategory = app.category;
    const auto before = Good(app, "describe"),
               changes = Good(app, "database_changes");
    Check(app.dirty[0] && app.canUndo[0] && app.canRedo[0] && savedField == 1,
          "Dirty undo/redo/field fixture not real");
    bool stale = false;
    app.Queue("describe", J::object(), [&](const J &) { stale = true; });
    const auto oldEpoch = app.epoch;
    NativeMenu(app, Monstey_NativeBanksMenu);
    Check(app.epoch > oldEpoch && !stale,
          "Old record callback was not invalidated");
    Check(app.sourceNames[0] == savedSource,
          "Native bank selection overwrote DB source");
    app.operationArguments["path"] = "C:/review-only/PE.exe";
    app.operationArguments["expected_sha256"] = std::string(64, 'a');
    int ci = -1, bi = -1;
    for (std::size_t i = 0; i < app.rowData.size(); ++i) {
      if (app.rowData[i].at("name") == J("database_native_class"))
        ci = static_cast<int>(i);
      if (app.rowData[i].at("name") == J("database_native_banks"))
        bi = static_cast<int>(i);
    }
    Check(ci >= 0 && bi >= 0, "Native operation identities missing");
    app.SelectOperation(ci);
    Check(app.operationArguments.at("path") == J("C:/review-only/PE.exe") &&
              app.operationArguments.at("expected_sha256") ==
                  J(std::string(64, 'a')),
          "Banks to class did not preserve only native source/hash");
    const auto args = app.operationArguments;
    for (const auto id :
         {Monstey_Undo, Monstey_Redo, Monstey_Delete, Monstey_Clone,
          Monstey_Rename, Monstey_New, Monstey_CreateCategory,
          Monstey_CreateRecord, Monstey_AddField, Monstey_DeleteField,
          Monstey_MoveFieldUp, Monstey_MoveFieldDown, Monstey_Structure})
      SendMessageW(app.window, WM_COMMAND, id, 0);
    app.StructureCommand(Monstey_AddField);
    const auto snapshot = savedMetadata;
    app.EditField(0, &snapshot);
    Check(!app.editor, "Old inspector edit opened in Native mode");
    for (const auto list : {app.records, app.fields})
      for (const auto key : {VK_INSERT, VK_DELETE}) {
        NMLVKEYDOWN k{};
        k.hdr.hwndFrom = list;
        k.hdr.code = LVN_KEYDOWN;
        k.wVKey = static_cast<WORD>(key);
        SendMessageW(app.window, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&k));
      }
    Check(app.operationArguments == args,
          "Record commands altered native arguments");
    Stable(app, before, changes);
    ++groups;
    for (const auto tab : {6, 4, 5}) {
      TabCtrl_SetCurSel(app.tabs, tab);
      app.TabChanged();
      Idle(app);
      Check(app.tab == tab && !app.Monstey_DatabaseForm(),
            "Legacy tab entered native DB mode");
      Check(!app.rowData.empty(),
            "Legacy operation tab lost its operation catalogue");
    }
    TabCtrl_SetCurSel(app.tabs, 0);
    app.TabChanged();
    NativeReady(app);
    Check(app.sourceNames[0] == savedSource,
          "Legacy tab roundtrip changed DB source");
    Stable(app, before, changes);
    ++groups;
    bool lateNative = false;
    app.Queue("describe", J::object(), [&](const J &) { lateNative = true; });
    SendMessageW(app.window, WM_COMMAND, Monstey_DatabaseRecordsMenu, 0);
    Until(
        [&] {
          return !app.databaseNativeMode && !app.metadata.empty() &&
                 app.selector == selector;
        },
        "Back did not restore selected record");
    Idle(app);
    Check(!lateNative, "Native completion callback escaped epoch transition");
    Check(app.category == savedCategory && app.offset == 250 &&
              Monstey_WindowText(app.query) == L"Keep_" &&
              ListView_GetNextItem(app.records, -1, LVNI_SELECTED) == 5,
          "Back lost category/filter/page/record selection");
    Monstey_WriteNewText(
        report.parent_path() / (report.stem().wstring() + L"-field-state.json"),
        J::object({{"saved_field", savedField},
                   {"actual_field",
                    ListView_GetNextItem(app.fields, -1, LVNI_SELECTED)},
                   {"same_fields", app.fieldData == savedFields},
                   {"restore_field_flag", app.databaseRestoreField},
                   {"saved_fields", savedFields},
                   {"actual_fields", app.fieldData},
                   {"selector", app.selector},
                   {"offset", app.offset},
                   {"filter", Monstey_Utf8(Monstey_WindowText(app.query))}})
                .dump(2) +
            "\n");
    Check(ListView_GetNextItem(app.fields, -1, LVNI_SELECTED) == savedField &&
              app.fieldData == savedFields,
          "Back lost selected field or field metadata");
    Check(app.sourceNames[0] == savedSource && app.dirty[0] && app.canUndo[0] &&
              app.canRedo[0],
          "Back lost source or dirty history flags");
    Stable(app, before, changes);
    Check(!std::filesystem::exists(settings), "GUI wrote settings");
    ++groups;
    Monstey_WriteNewText(
        report,
        J::object({{"pass", true},
                   {"groups", groups},
                   {"assertions", checks},
                   {"owned_hidden_UI", true},
                   {"fixture_records", 270},
                   {"selected_page_offset", 250},
                   {"selected_record_row", 5},
                   {"selected_field_row", 1},
                   {"actual_project_history_and_source_preserved", true},
                   {"no_CUA_or_Game", true}})
                .dump(2) +
            "\n");
    std::cout << "PASS " << groups << " groups " << checks << " checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
