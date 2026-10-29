#include "../Monstey_AppState.hpp"

namespace Monstey::Monstey_AppInternal
{

J Monstey_FxNumber(const char *name, const char *label, const J &value, bool integer)
{
    return J::object({{"name", name}, {"label", label}, {"kind", integer ? "integer" : "number"}, {"value", value}});
}

void Monstey_App::FxAccepted(const J &result)
{
    UpdateState(result, 3);
    ShowReport(result);
    if (result.contains("result") && result.at("result").is_object())
    {
        auto &detail = result.at("result");
        if (detail.contains("reference") && detail.at("reference").is_string())
            selector = detail.at("reference").as_string();
        else if (detail.contains("selector"))
            selector = detail.at("selector").as_string();
        else if (detail.contains("record") && detail.at("record").is_string())
            selector = detail.at("record").as_string();
        const auto operation = result.value("operation", "").as_string();
        if (operation == "fx_prefab_add_child")
            fxView = "pending-element";
        else if (operation == "fx_prefab_create" || operation == "fx_prefab_duplicate" ||
                 operation == "fx_prefab_from_key")
            fxView = "prefabs";
        if (result.value("operation", "").as_string() == "fx_create_element" ||
            result.value("operation", "").as_string() == "fx_duplicate_element")
            fxView = "pending-element";
        if (detail.contains("id") && detail.at("id").is_string() && detail.at("id").as_string().starts_with("track-"))
            fxView = detail.at("id").as_string();
        if (detail.contains("global_record"))
        {
            fxView = "global";
            selector = detail.at("global_record").as_string();
        }
    }
    LoadFx();
}

void Monstey_App::FxApply(std::string operation, J arguments)
{
    arguments["expected_revision"] = revision;
    Queue(std::move(operation), std::move(arguments),
          [this](const J &reply)
          {
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              FxAccepted(reply.at("result"));
          });
}

void Monstey_App::FxCreateValues(std::string operation, J arguments, std::string className)
{
    Queue("fx_schema", J::object({{"class_name", className}}),
          [this, operation = std::move(operation), arguments = std::move(arguments), className](const J &reply)
          {
              if (!reply.at("ok").as_bool())
              {
                  Error(reply.at("error").at("message").as_string());
                  return;
              }
              auto required = J::array();
              const auto &schemaReply = reply.at("result");
              Monstey_Require(schemaReply.contains("native_fields") && schemaReply.contains("required_initial_values"),
                              "The FX backend must supply qualified native fields "
                              "and required values");
              const auto &names = schemaReply.at("required_initial_values").as_array();
              for (const auto &field : schemaReply.at("native_fields").as_array())
              {
                  auto name = field.at("name").as_string();
                  if (std::find(names.begin(), names.end(), J(name)) == names.end())
                      continue;
                  if (name == "KeyName" || name == "FxName" || name == "KeyList" ||
                      (name == "StartTime" && arguments.contains("start")) ||
                      (name == "EndTime" && arguments.contains("end")) ||
                      (name == "ResourceLenMS" && arguments.contains("duration_ms")))
                      continue;
                  required.as_array().push_back(field);
              }
              if (!smokeReport.empty())
              {
                  smoke["required_schema_counts"][className] = required.size();
                  if (operation == "fx_create_element")
                      smoke["required_native_dialogs_expected"] = required.size();
              }
              auto draft = std::make_shared<J>(J::object({{"operation", operation},
                                                          {"arguments", arguments},
                                                          {"required", required},
                                                          {"next", 0},
                                                          {"values", J::object()},
                                                          {"generation", generation},
                                                          {"revision", revision}}));
              FxRequiredValue(draft);
          });
}

void Monstey_App::FxRequiredValue(std::shared_ptr<J> draft)
{
    const auto index = draft->at("next").as_uint();
    if (index >= draft->at("required").size())
    {
        auto args = draft->at("arguments");
        args["values"] = draft->at("values");
        args["expected_generation"] = draft->at("generation");
        args["expected_revision"] = draft->at("revision");
        Queue(draft->at("operation").as_string(), args,
              [this](const J &reply)
              {
                  if (!reply.at("ok").as_bool())
                  {
                      Error(reply.at("error").at("message").as_string());
                      return;
                  }
                  FxAccepted(reply.at("result"));
              });
        return;
    }
    const auto definition = draft->at("required")[static_cast<std::size_t>(index)];
    const auto type = static_cast<unsigned>(definition.contains("type_code") ? definition.at("type_code").as_uint()
                                                                             : definition.at("native_type").as_uint());
    auto initial = Monstey_StructureDefault(type);
    const auto count = definition.value("array", 1).as_int();
    const bool curve = type == 12 || type == 13 || type == 15 || type == 16;
    const bool array = !curve && (count == -1 || count > 1);
    if (array)
    {
        initial = J::array();
        for (std::int64_t i = 0; i < (count > 1 ? count : 1); ++i)
            initial.as_array().push_back(Monstey_StructureDefault(type));
    }
    if (type == 5 && definition.contains("enum_values") && !definition.at("enum_values").empty())
        initial = definition.at("enum_values")[0];
    auto item = std::make_shared<Monstey_Editor>();
    item->app = this;
    item->title = L"Required initial value " + std::to_wstring(index + 1) + L" / " +
                  std::to_wstring(draft->at("required").size());
    item->field = definition;
    item->field["native_type"] = type;
    item->field["value"] = initial;
    item->field["array"] = array;
    item->field["editable"] = true;
    item->lineList = array;
    item->generation = draft->at("generation").as_uint();
    item->revision = draft->at("revision").as_uint();
    item->success = [this, draft, name = definition.at("name").as_string()](const J &value)
    {
        (*draft)["values"][name] = value;
        (*draft)["next"] = draft->at("next").as_uint() + 1;
        FxRequiredValue(draft);
    };
    ShowEditor(item);
}

} // namespace Monstey::Monstey_AppInternal
