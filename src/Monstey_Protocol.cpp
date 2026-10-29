#include "Monstey_Protocol.hpp"
namespace Monstey {
using J = Monstey_Json;
J Monstey_RpcError(const J &id, int code, const std::string &message) {
  return J::object(
      {{"jsonrpc", "2.0"},
       {"id", id},
       {"error", J::object({{"code", code}, {"message", message}})}});
}

std::optional<J> Monstey_Mcp::Handle(const J &request) {
  std::lock_guard lock(mutex_);
  if (!request.is_object() || request.value("jsonrpc", "") != J("2.0") ||
      !request.contains("method") || !request.at("method").is_string())
    return Monstey_RpcError(nullptr, -32600, "Invalid JSON-RPC request");
  const auto id = request.value("id", nullptr);
  const auto method = request.at("method").as_string();
  if (!request.contains("id")) {
    if (method.starts_with("notifications/"))
      return std::nullopt;
    return std::nullopt;
  }
  if (!(id.is_null() || id.is_string() || id.is_integer()))
    return Monstey_RpcError(nullptr, -32600, "Invalid JSON-RPC id");
  try {
    const auto params = request.value("params", J::object());
    if (!params.is_object())
      return Monstey_RpcError(id, -32602, "Params must be an object");
    J result;
    if (method == "initialize") {
      initialized_ = true;
      result = J::object(
          {{"protocolVersion", "2025-03-26"},
           {"capabilities",
            J::object({{"tools", J::object({{"listChanged", false}})}})},
           {"serverInfo", J::object({{"name", "Monstey SoW Native Workbench"},
                                     {"version", "0.3.0-dev"}})}});
    } else if (method == "ping")
      result = J::object();
    else if (!initialized_)
      return Monstey_RpcError(id, -32002, "Initialize the MCP session first");
    else if (method == "tools/list")
      result = J::object({{"tools", Monstey_Runtime::Catalog()}});
    else if (method == "tools/call") {
      if (!params.contains("name") || !params.at("name").is_string())
        return Monstey_RpcError(id, -32602, "Tool name must be a string");
      const auto response = runtime_.Call(
          J::object({{"operation", params.at("name")},
                     {"arguments", params.value("arguments", J::object())}}));
      result = J::object(
          {{"content", J::array({J::object(
                           {{"type", "text"}, {"text", response.dump()}})})},
           {"isError", !response.at("ok").as_bool()}});
    } else
      return Monstey_RpcError(id, -32601, "Method not found");
    return J::object({{"jsonrpc", "2.0"}, {"id", id}, {"result", result}});
  } catch (const std::exception &error) {
    return Monstey_RpcError(id, -32602, error.what());
  }
}
} // namespace Monstey
