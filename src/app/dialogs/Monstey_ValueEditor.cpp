#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

void Monstey_App::ShowEditor(std::shared_ptr<Monstey_Editor> item)
{
    if (editor && editor->window)
    {
        SetForegroundWindow(editor->window);
        return;
    }
    editor = std::move(item);
    editor->Create();
}

void Monstey_App::Prompt(const std::wstring &title, std::string operation, J args, const std::string &valueKey,
                         std::string initial, std::function<void(const J &)> success)
{
    auto item = std::make_shared<Monstey_Editor>();
    item->app = this;
    item->field = J::object(
        {{"name", Monstey_Utf8(title)}, {"native_type", 4}, {"array", false}, {"value", initial}, {"editable", true}});
    item->title = title;
    item->plainText = true;
    item->operation = std::move(operation);
    item->arguments = std::move(args);
    item->valueKey = valueKey;
    item->generation = generation;
    item->revision = revision;
    item->success = [this, weak = std::weak_ptr<Monstey_Editor>(item), success = std::move(success)](const J &result)
    {
        if (const auto submitted = weak.lock())
        {
            if (submitted->operation == "archive_rename" || submitted->operation == "archive_add" ||
                submitted->operation == "archive_create_folder")
                selector = Monstey_ArchiveVirtual(submitted->arguments.at(submitted->valueKey).as_string());
            else if (submitted->operation == "strings_rename")
            {
                const auto text = submitted->arguments.at("new").as_string();
                // The backend has already validated this submitted key successfully.
                const auto hash = text.starts_with("0x") || text.starts_with("0X")
                                      ? static_cast<std::uint32_t>(std::stoull(text.substr(2), nullptr, 16))
                                      : Monstey_PathHash(text);
                selector = Monstey_Hex(hash);
            }
            else if (submitted->operation == "strings_set" && result.contains("hash"))
                selector = result.at("hash").as_string();
        }
        if (success)
            success(result);
    };
    ShowEditor(item);
}

void Monstey_Editor::Create()
{
    const auto type = field.at("native_type").as_int();
    const bool graphical = type == 12 || type == 13 || type == 15 || type == 16;
    window = CreateWindowExW(WS_EX_DLGMODALFRAME, Monstey_EditorClass, title.c_str(),
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, CW_USEDEFAULT, CW_USEDEFAULT, 760,
                             !formFields.empty() ? static_cast<int>(formFields.size()) * 66 + 255 +
                                                       (field.contains("review_text") ? 110 : 0)
                             : graphical         ? 650
                                                 : 530,
                             app->window, nullptr, app->instance, this);
    Monstey_Require(window != nullptr, "Cannot create property editor");
    Monstey_ThemeApply(window);
    EnableWindow(app->window, FALSE);
    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);
}

void Monstey_Editor::Layout()
{
    if (!window)
        return;
    RECT rect{};
    GetClientRect(window, &rect);
    const int width = std::max(400L, rect.right), height = std::max(250L, rect.bottom);
    for (std::size_t i = 0; i < formControls.size(); ++i)
    {
        auto y = (field.contains("review_text") ? 220 : 110) + static_cast<int>(i) * 66;
        MoveWindow(formLabels[i], 16, y, width - 32, 20, TRUE);
        MoveWindow(formControls[i], 16, y + 23, width - 32, formFields[i].at("kind").as_string() == "choice" ? 300 : 28,
                   TRUE);
    }
    if (curve)
        curve->Layout(16, 108, width - 32, height - 208);
    if (input)
        MoveWindow(input, 16, 108, width - 32, height - 208, TRUE);
    if (combo)
        MoveWindow(combo, 16, 108, width - 32, 280, TRUE);
    for (std::size_t i = 0; i < components.size(); ++i)
        MoveWindow(components[i], 16 + static_cast<int>(i) * (width - 32) / static_cast<int>(components.size()), 112,
                   (width - 48) / static_cast<int>(components.size()), 28, TRUE);
    MoveWindow(status, 16, height - 92, width - 32, 38, TRUE);
    MoveWindow(GetDlgItem(window, Monstey_EditorApply), width - 210, height - 45, 90, 28, TRUE);
    MoveWindow(GetDlgItem(window, Monstey_EditorCancel), width - 110, height - 45, 90, 28, TRUE);
    if (GetDlgItem(window, Monstey_EditorFormat))
        MoveWindow(GetDlgItem(window, Monstey_EditorFormat), 16, height - 45, 110, 28, TRUE);
}

void Monstey_Editor::Close()
{
    if (pending && !app->closing)
        return;
    if (!window)
        return;
    const auto closed = window;
    window = nullptr;
    DestroyWindow(closed);
    if (!app->closing)
    {
        EnableWindow(app->window, TRUE);
        SetForegroundWindow(app->window);
    }
}

void Monstey_Editor::Apply()
{
    auto self = shared_from_this();
    if (pending)
        return;
    try
    {
        const auto type = field.at("native_type").as_int();
        const bool array = field.value("array", false).as_bool();
        J value;
        if (!formFields.empty())
        {
            value = J::object();
            for (std::size_t i = 0; i < formFields.size(); ++i)
            {
                auto &row = formFields[i];
                auto kind = row.at("kind").as_string(), key = row.at("name").as_string();
                if (kind == "choice")
                {
                    auto index = SendMessageW(formControls[i], CB_GETCURSEL, 0, 0);
                    Monstey_Require(index >= 0, "Select a value from the list");
                    value[key] = row.at("choices")[static_cast<std::size_t>(index)].at("value");
                }
                else if (kind == "bool")
                    value[key] = SendMessageW(formControls[i], BM_GETCHECK, 0, 0) == BST_CHECKED;
                else
                {
                    auto text = Monstey_Utf8(Monstey_WindowText(formControls[i]));
                    if (kind == "number")
                    {
                        auto number = J::Parse(text);
                        Monstey_Require(number.is_number(), "Enter a number for " + row.at("label").as_string());
                        value[key] = number;
                    }
                    else if (kind == "integer")
                    {
                        auto n = J::Parse(text);
                        Monstey_Require(n.is_integer() && n.as_int() >= 0, "Array size must be a nonnegative integer");
                        value[key] = n;
                    }
                    else
                    {
                        if (text.empty() && row.value("optional", false).as_bool())
                            continue;
                        Monstey_Require(!text.empty() || row.value("allow_empty", false).as_bool(),
                                        row.at("label").as_string() + " is required");
                        value[key] = text;
                    }
                }
            }
        }
        else if (lineList)
            value = Monstey_ParseTypedLines(Monstey_WindowText(input), static_cast<unsigned>(type));
        else if (curve)
            value = curve->Value();
        else if (combo)
        {
            const auto index = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
            Monstey_Require(index >= 0, "Select a value");
            if (type == 1)
                value = index == 1;
            else
            {
                std::wstring text(static_cast<std::size_t>(SendMessageW(combo, CB_GETLBTEXTLEN, index, 0)) + 1, L'\0');
                SendMessageW(combo, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(text.data()));
                text.resize(text.size() - 1);
                value = Monstey_Utf8(text);
            }
        }
        else if (!components.empty())
        {
            value = J::array();
            for (const auto control : components)
            {
                const auto v = J::Parse(Monstey_Utf8(Monstey_WindowText(control)));
                Monstey_Require(v.is_number(), "Vector component must be numeric");
                value.as_array().push_back(v);
            }
        }
        else
        {
            const auto text = Monstey_Utf8(Monstey_WindowText(input));
            if (plainText || (!array && (type == 4 || type == 5 || type == 6 || type == 68)))
                value = text;
            else if (!array && (type == 10 || type == 11))
                value = text.empty() || text == "null" ? J() : J(text);
            else
                value = J::Parse(text);
        }
        if (validate)
            validate(value);
        if (operation.empty())
        {
            auto callback = success;
            Close();
            if (callback)
                callback(value);
            return;
        }
        J args = arguments;
        if (operation == "material_edit")
        {
            Monstey_Require(args.at("operations").is_array() && args.at("operations").size() == 1,
                            "Material form requires one operation");
            if (!formFields.empty())
            {
                for (const auto &[key, item] : value.as_object())
                    args["operations"][0][key] = item;
            }
            else
                args["operations"][0][valueKey] = value;
        }
        else if (!formFields.empty())
        {
            for (auto &[key, item] : value.as_object())
                args[key] = item;
        }
        else
            args[valueKey] = value;
        if (operation == "database_create_record" && args.at("anonymous").as_bool())
            args.as_object().erase("name");
        args["expected_generation"] = generation;
        args["expected_revision"] = revision;
        pending = true;
        EnableWindow(GetDlgItem(window, Monstey_EditorApply), FALSE);
        SetWindowTextW(status, L"Validating the native field ...");
        app->Queue(operation, args,
                   [self, args](const J &reply)
                   {
                       if (!self->window)
                           return;
                       self->pending = false;
                       if (!reply.at("ok").as_bool())
                       {
                           SetWindowTextW(self->status,
                                          Monstey_Wide(reply.at("error").at("message").as_string()).c_str());
                           EnableWindow(GetDlgItem(self->window, Monstey_EditorApply), TRUE);
                           if (!self->app->smokeReport.empty())
                               self->app->Error(reply.at("error").at("message").as_string());
                           return;
                       }
                       self->arguments = args;
                       auto callback = self->success;
                       self->Close();
                       if (callback)
                           callback(reply.at("result"));
                   });
    }
    catch (const std::exception &e)
    {
        SetWindowTextW(status, Monstey_Wide(e.what()).c_str());
        if (!app->smokeReport.empty())
            app->Error(e.what());
    }
}

} // namespace Monstey::Monstey_AppInternal
