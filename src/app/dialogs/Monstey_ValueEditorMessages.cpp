#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

LRESULT CALLBACK Monstey_Editor::Procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto *self = reinterpret_cast<Monstey_Editor *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto *create = reinterpret_cast<const CREATESTRUCTW *>(lparam);
        self = static_cast<Monstey_Editor *>(create->lpCreateParams);
        self->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self)
        return DefWindowProcW(window, message, wparam, lparam);
    try
    {
        switch (message)
        {
        case WM_CREATE:
        {
            const auto control = [&](const wchar_t *type, const wchar_t *text, DWORD style, int id)
            {
                const auto item =
                    CreateWindowExW(type == std::wstring_view(L"EDIT") ? WS_EX_CLIENTEDGE : 0, type, text,
                                    WS_CHILD | WS_VISIBLE | style, 16, 16, 680, 30, window,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), self->app->instance, nullptr);
                SendMessageW(item, WM_SETFONT, reinterpret_cast<WPARAM>(self->app->font), TRUE);
                return item;
            };
            control(L"STATIC", Monstey_Display(self->field.at("name")).c_str(), SS_LEFT, 0);
            std::wstring explanation = self->formFields.empty()
                                           ? (self->lineList ? L"One value per line. Vectors use comma-separated "
                                                               L"components; an empty list is blank. Use two quotes "
                                                               L"for an empty string."
                                                             : L"Native validation runs before this value changes.")
                                           : L"Complete the named fields. Native validation runs before any "
                                             L"change.";
            for (const auto key : {"min", "max", "target", "constraint"})
                if (self->field.contains(key) && !self->field.at(key).is_null())
                    explanation += L"  " + Monstey_Wide(key) + L": " + Monstey_Display(self->field.at(key));
            if (self->field.contains("review_text"))
                explanation = Monstey_Display(self->field.at("review_text"));
            const auto hint = control(L"STATIC", explanation.c_str(), SS_LEFT, 0);
            MoveWindow(hint, 16, 50, 700, self->field.contains("review_text") ? 152 : 52, TRUE);
            if (!self->formFields.empty())
            {
                for (std::size_t i = 0; i < self->formFields.size(); ++i)
                {
                    auto &row = self->formFields[i];
                    self->formLabels.push_back(
                        control(L"STATIC", Monstey_Display(row.at("label")).c_str(), SS_LEFT, 0));
                    auto kind = row.at("kind").as_string();
                    HWND input = nullptr;
                    if (kind == "choice")
                    {
                        input = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                                        400 + static_cast<int>(i));
                        int chosen = 0, index = 0;
                        for (auto &choice : row.at("choices").as_array())
                        {
                            auto label = Monstey_Display(choice.at("label"));
                            SendMessageW(input, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
                            if (choice.at("value") == row.at("value"))
                                chosen = index;
                            ++index;
                        }
                        SendMessageW(input, CB_SETCURSEL, chosen, 0);
                    }
                    else if (kind == "bool")
                    {
                        input = control(L"BUTTON", L"Yes", BS_AUTOCHECKBOX | WS_TABSTOP, 400 + static_cast<int>(i));
                        SendMessageW(input, BM_SETCHECK, row.at("value").as_bool() ? BST_CHECKED : BST_UNCHECKED, 0);
                    }
                    else
                        input = control(L"EDIT", Monstey_Display(row.at("value")).c_str(),
                                        ES_AUTOHSCROLL | WS_TABSTOP | (kind == "integer" ? ES_NUMBER : 0),
                                        400 + static_cast<int>(i));
                    self->formControls.push_back(input);
                }
            }
            else
            {
                const auto type = self->field.at("native_type").as_int();
                const bool array = self->field.value("array", false).as_bool();
                const auto &value = self->field.at("value");
                if (type == 12 || type == 13 || type == 15 || type == 16)
                {
                    try
                    {
                        self->curve = std::make_shared<Monstey_CurveControl>(static_cast<unsigned>(type), value);
                    }
                    catch (const Monstey_Error &)
                    {
                        self->curve.reset();
                    }
                }
                if (self->curve)
                    self->curve->Create(window, self->app->font);
                else if (!array && (type == 1 || (type == 5 && self->field.contains("enum_values") &&
                                                  !self->field.at("enum_values").empty())))
                {
                    self->combo =
                        control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, Monstey_EditorValue);
                    if (type == 1)
                    {
                        SendMessageW(self->combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"False"));
                        SendMessageW(self->combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"True"));
                        SendMessageW(self->combo, CB_SETCURSEL, value.as_bool() ? 1 : 0, 0);
                    }
                    else
                    {
                        int selected = 0, index = 0;
                        for (const auto &item : self->field.at("enum_values").as_array())
                        {
                            const auto text = Monstey_Display(item);
                            SendMessageW(self->combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
                            if (item == value)
                                selected = index;
                            ++index;
                        }
                        SendMessageW(self->combo, CB_SETCURSEL, selected, 0);
                    }
                }
                else if (!array && type >= 7 && type <= 9)
                {
                    for (std::size_t i = 0; i < value.size(); ++i)
                    {
                        const auto item =
                            control(L"EDIT", Monstey_Display(value.at(i)).c_str(), ES_AUTOHSCROLL | WS_TABSTOP,
                                    Monstey_EditorValue + static_cast<int>(i));
                        self->components.push_back(item);
                    }
                }
                else
                {
                    const bool plain =
                        self->plainText || (!array && (type == 4 || type == 5 || type == 6 || type == 68));
                    const auto text = self->lineList ? Monstey_TypedLines(value, static_cast<unsigned>(type))
                                      : plain        ? Monstey_Display(value)
                                      : !array && (type == 10 || type == 11)
                                          ? value.is_null() ? std::wstring(L"null") : Monstey_Display(value)
                                          : Monstey_Wide(value.dump(2));
                    self->input = control(L"EDIT", text.c_str(),
                                          ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | WS_VSCROLL |
                                              WS_HSCROLL | WS_TABSTOP,
                                          Monstey_EditorValue);
                    if (!plain && (array || type >= 12))
                    {
                        control(L"BUTTON", self->lineList ? L"Advanced JSON" : L"Format JSON", WS_TABSTOP,
                                Monstey_EditorFormat);
                    }
                }
            }
            self->status = control(L"STATIC", L"", SS_LEFT, 0);
            control(L"BUTTON", L"Apply", BS_DEFPUSHBUTTON | WS_TABSTOP, Monstey_EditorApply);
            control(L"BUTTON", L"Cancel", WS_TABSTOP, Monstey_EditorCancel);
            self->Layout();
            SetFocus(!self->formControls.empty() ? self->formControls.front()
                     : self->curve               ? self->curve->Window()
                     : self->combo               ? self->combo
                     : !self->components.empty() ? self->components.front()
                                                 : self->input);
            return 0;
        }
        case WM_GETMINMAXINFO:
        {
            const auto type = self->field.at("native_type").as_int();
            if (!self->formFields.empty())
            {
                reinterpret_cast<MINMAXINFO *>(lparam)->ptMinTrackSize = {
                    500, static_cast<LONG>(self->formFields.size()) * 66 + 255};
                return 0;
            }
            if (type == 12 || type == 13 || type == 15 || type == 16)
            {
                reinterpret_cast<MINMAXINFO *>(lparam)->ptMinTrackSize = {600, 500};
                return 0;
            }
            break;
        }
        case WM_SIZE:
            self->Layout();
            return 0;
        case WM_COMMAND:
            if (LOWORD(wparam) == Monstey_EditorApply)
            {
                self->Apply();
                return 0;
            }
            if (LOWORD(wparam) == Monstey_EditorCancel)
            {
                self->Close();
                return 0;
            }
            if (LOWORD(wparam) == Monstey_EditorFormat && self->input)
            {
                const auto value =
                    self->lineList
                        ? Monstey_ParseTypedLines(Monstey_WindowText(self->input),
                                                  static_cast<unsigned>(self->field.at("native_type").as_uint()))
                        : J::Parse(Monstey_Utf8(Monstey_WindowText(self->input)));
                self->lineList = false;
                SetWindowTextW(self->input, Monstey_Wide(value.dump(2)).c_str());
                return 0;
            }
            break;
        case WM_CLOSE:
            self->Close();
            return 0;
        case WM_NCDESTROY:
            self->window = nullptr;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            return DefWindowProcW(window, message, wparam, lparam);
        default:
            break;
        }
    }
    catch (const std::exception &e)
    {
        if (self->status)
            SetWindowTextW(self->status, Monstey_Wide(e.what()).c_str());
        else
            self->app->Error(e.what());
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace Monstey::Monstey_AppInternal
