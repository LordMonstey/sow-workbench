#pragma once
#include "Monstey_Protocol.hpp"
#include <memory>
namespace Monstey {
struct Monstey_HttpOptions {
  std::string host = "127.0.0.1";
  std::uint16_t port{};
  std::vector<std::string> origins;
};
// Owns the listener and all connection threads. Stop interrupts network waits;
// Wait/destruction drains accepted native operations before releasing Runtime.
class Monstey_HttpServer {
public:
  explicit Monstey_HttpServer(Monstey_Runtime &runtime,
                              Monstey_HttpOptions options = {});
  ~Monstey_HttpServer();
  Monstey_HttpServer(const Monstey_HttpServer &) = delete;
  Monstey_HttpServer &operator=(const Monstey_HttpServer &) = delete;
  std::string BaseUrl() const;
  bool Running() const noexcept;
  void Stop() noexcept;
  void Wait();

private:
  struct Monstey_Impl;
  std::unique_ptr<Monstey_Impl> impl_;
};
} // namespace Monstey
