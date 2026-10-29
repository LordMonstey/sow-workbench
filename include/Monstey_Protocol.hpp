#pragma once
#include "Monstey_Runtime.hpp"
#include <optional>
namespace Monstey {
Monstey_Json Monstey_RpcError(const Monstey_Json &id, int code,
                              const std::string &message);
class Monstey_Mcp {
public:
  explicit Monstey_Mcp(Monstey_Runtime &runtime) : runtime_(runtime) {}
  std::optional<Monstey_Json> Handle(const Monstey_Json &request);

private:
  Monstey_Runtime &runtime_;
  std::mutex mutex_;
  bool initialized_{};
};
} // namespace Monstey
