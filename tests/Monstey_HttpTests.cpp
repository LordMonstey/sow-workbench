#include "Monstey_Http.hpp"
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <array>
#include <charconv>
#include <chrono>
#include <iostream>

using namespace Monstey;
using J = Monstey_Json;
namespace {
void Check(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
struct Connection {
  SOCKET socket = INVALID_SOCKET;
  std::string buffered;
  explicit Connection(unsigned port) {
    socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Check(socket != INVALID_SOCKET, "client socket failed");
    const DWORD timeout = 5000;
    (void)setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
                     reinterpret_cast<const char *>(&timeout), sizeof(timeout));
    (void)setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
                     reinterpret_cast<const char *>(&timeout), sizeof(timeout));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(socket, reinterpret_cast<const sockaddr *>(&address),
                sizeof(address)) != 0) {
      closesocket(socket);
      socket = INVALID_SOCKET;
      throw std::runtime_error("client connect failed");
    }
  }
  ~Connection() {
    if (socket != INVALID_SOCKET)
      closesocket(socket);
  }
  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;
  void Send(std::string_view bytes) {
    while (!bytes.empty()) {
      const auto sent =
          send(socket, bytes.data(),
               static_cast<int>(std::min<std::size_t>(bytes.size(), 65536)), 0);
      Check(sent > 0, "client send failed");
      bytes.remove_prefix(static_cast<std::size_t>(sent));
    }
  }
  std::string Until(std::string_view delimiter) {
    while (buffered.find(delimiter) == buffered.npos) {
      char bytes[4096];
      const int count = recv(socket, bytes, sizeof(bytes), 0);
      Check(count > 0, "stream ended before delimiter");
      buffered.append(bytes, static_cast<std::size_t>(count));
      Check(buffered.size() < 4u * 1024 * 1024,
            "client buffer exceeded bounds");
    }
    const auto end = buffered.find(delimiter) + delimiter.size();
    auto result = buffered.substr(0, end);
    buffered.erase(0, end);
    return result;
  }
  std::string Drain() {
    std::string result = std::move(buffered);
    for (;;) {
      char bytes[4096];
      const int count = recv(socket, bytes, sizeof(bytes), 0);
      if (count == 0)
        break;
      if (count < 0)
        throw std::runtime_error("client receive failed: Winsock " +
                                 std::to_string(WSAGetLastError()) +
                                 ", bytes=" + std::to_string(result.size()) +
                                 ", prefix=" + result.substr(0, 80));
      result.append(bytes, static_cast<std::size_t>(count));
      Check(result.size() < 4u * 1024 * 1024, "response exceeds client bounds");
    }
    return result;
  }
};
struct Response {
  unsigned status{};
  std::string raw, body;
  explicit Response(std::string wire) : raw(std::move(wire)) {
    Check(raw.starts_with("HTTP/1.1 "), "invalid HTTP status");
    const auto parsed =
        std::from_chars(raw.data() + 9, raw.data() + 12, status);
    Check(parsed.ec == std::errc{}, "invalid HTTP status number");
    const auto split = raw.find("\r\n\r\n");
    Check(split != raw.npos, "missing response header terminator");
    body = raw.substr(split + 4);
  }
  std::string Header(const std::string &key) const {
    const auto start = raw.find("\r\n" + key + ": ");
    if (start == raw.npos)
      return {};
    const auto value = start + key.size() + 4;
    return raw.substr(value, raw.find("\r\n", value) - value);
  }
  J Json() const { return J::Parse(body); }
};
unsigned Port(const std::string &url) {
  const auto text = std::string_view(url).substr(url.rfind(':') + 1);
  unsigned port{};
  Check(std::from_chars(text.data(), text.data() + text.size(), port).ec ==
            std::errc{},
        "invalid listener port");
  return port;
}
std::string Request(unsigned port, const std::string &method,
                    const std::string &path, const std::string &body = {},
                    const std::string &extra = {}) {
  return method + " " + path +
         " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) + "\r\n" +
         extra +
         (method == "POST"
              ? "Content-Type: application/json\r\nContent-Length: " +
                    std::to_string(body.size()) + "\r\n"
              : "") +
         "\r\n" + body;
}
Response Raw(unsigned port, const std::string &request, bool end = false) {
  Connection connection(port);
  connection.Send(request);
  if (end)
    shutdown(connection.socket, SD_SEND);
  return Response(connection.Drain());
}
Response Api(unsigned port, const std::string &name, J arguments = J::object(),
             const std::string &extra = {}) {
  return Raw(port, Request(port, "POST", "/v1/tools/" + name, arguments.dump(),
                           extra));
}
J Rpc(unsigned id, const char *method, J params = J::object()) {
  return J::object(
      {{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}});
}
J SseMessage(Connection &connection) {
  const auto text = connection.Until("\n\n");
  const auto data = text.find("data: ");
  Check(data != text.npos, "missing SSE data");
  return J::Parse(
      std::string_view(text).substr(data + 6, text.size() - data - 8));
}
} // namespace
int main(int argc, char **argv) {
  try {
    const auto root = argc > 1 ? std::filesystem::path(Monstey_Wide(argv[1]))
                               : std::filesystem::temp_directory_path();
    const auto directory =
        root /
        ("http-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    Monstey_Runtime runtime;
    Monstey_HttpOptions options;
    options.origins = {"http://example.test"};
    Monstey_HttpServer server(runtime, options);
    const auto port = Port(server.BaseUrl());
    unsigned passed{};
    auto test = [&](const char *name, auto &&run) {
      run();
      ++passed;
      std::cout << "PASS " << name << '\n';
    };
    test("health and HEAD response", [&] {
      auto response = Raw(port, Request(port, "GET", "/health"));
      Check(response.status == 200 &&
                response.Json().at("implementation").as_string() == "C++20",
            "health failed");
      auto head = Raw(port, Request(port, "HEAD", "/health"));
      Check(head.status == 200 && head.body.empty() &&
                !head.Header("Content-Length").empty(),
            "HEAD failed");
    });
    test("native catalog and OpenAPI", [&] {
      const auto catalog = Raw(port, Request(port, "GET", "/v1/tools")).Json();
      Check(catalog.at("tools").size() >= 64, "catalog incomplete");
      const auto api = Raw(port, Request(port, "GET", "/openapi.json")).Json();
      Check(api.at("paths").size() == catalog.at("tools").size(),
            "OpenAPI path coverage differs");
      Check(api.at("paths")
                    .at("/v1/tools/strings_set")
                    .at("post")
                    .at("requestBody")
                    .at("content")
                    .at("application/json")
                    .at("schema")
                    .at("required")
                    .size() == 2,
            "OpenAPI field requirements lost");
    });
    test("REST shared state and Unicode", [&] {
      Check(Api(port, "strings_new").status == 200, "strings_new failed");
      Check(Api(port, "strings_set",
                J::object({{"key", "Http_Name"}, {"text", "Native \xce\xa9"}}))
                .Json()
                .at("ok")
                .as_bool(),
            "strings_set failed");
      const auto page = Api(port, "strings_entries").Json().at("result");
      Check(page.at("rows").size() == 1 &&
                page.at("rows")[0].at("text").as_string() == "Native \xce\xa9",
            "Unicode failed");
      Check(Api(port, "unknown_tool").status == 404,
            "unknown operation status");
    });
    test("Host validation", [&] {
      Check(Raw(port, "GET /health HTTP/1.1\r\nHost: remote.example:" +
                          std::to_string(port) + "\r\n\r\n")
                    .status == 421,
            "remote Host accepted");
      Check(Raw(port, "GET /health HTTP/1.1\r\nHost: localhost:1\r\n\r\n")
                    .status == 421,
            "wrong Host port accepted");
      Check(Raw(port, "GET /health HTTP/1.1\r\nHost: localhost:" +
                          std::to_string(port) + "\r\nHost: localhost:" +
                          std::to_string(port) + "\r\n\r\n")
                    .status == 400,
            "duplicate Host accepted");
      Check(Raw(port, "GET /health HTTP/1.1\r\n\r\n").status == 400,
            "missing Host accepted");
    });
    test("exact CORS and preflight", [&] {
      Check(Raw(port, Request(port, "GET", "/health", {},
                              "Origin: http://evil.test\r\n"))
                    .status == 403,
            "unlisted Origin accepted");
      Check(Raw(port, Request(port, "GET", "/health", {}, "Origin: \r\n"))
                    .status == 403,
            "empty Origin accepted");
      const auto allowed =
          Raw(port, Request(port, "GET", "/health", {},
                            "Origin: http://example.test\r\n"));
      Check(allowed.status == 200 &&
                allowed.Header("Access-Control-Allow-Origin") ==
                    "http://example.test",
            "allowed Origin lost");
      Check(Raw(port,
                Request(port, "OPTIONS", "/mcp", {},
                        "Origin: "
                        "http://example.test\r\nAccess-Control-Request-Method: "
                        "POST\r\nAccess-Control-Request-Headers: content-type, "
                        "mcp-session-id\r\n"))
                    .status == 204,
            "preflight failed");
    });
    test("HTTP length and transfer framing", [&] {
      const std::string prefix =
          "POST /v1/tools/describe HTTP/1.1\r\nHost: localhost:" +
          std::to_string(port) + "\r\n";
      Check(
          Raw(port, prefix + "Content-Type: application/json\r\n\r\n").status ==
              411,
          "length missing accepted");
      Check(Raw(port, prefix + "Content-Length: 2\r\nContent-Length: "
                               "2\r\nContent-Type: application/json\r\n\r\n{}")
                    .status == 400,
            "duplicate length accepted");
      Check(Raw(port, prefix + "Transfer-Encoding: chunked\r\n\r\n").status ==
                400,
            "chunking accepted");
      Check(Raw(port, prefix + "Content-Length: 65537\r\n\r\n").status == 413,
            "oversized body accepted");
      Check(Raw(port, prefix + "Content-Length: 5\r\n\r\n{}", true).status ==
                400,
            "partial body accepted");
      Check(Raw(port,
                prefix +
                    "Content-Length: 2\r\nContent-Type: text/plain\r\n\r\n{}")
                    .status == 415,
            "bad content type accepted");
    });
    test("strict JSON and bounded headers", [&] {
      Check(Raw(port, Request(port, "POST", "/v1/tools/describe",
                              "{\"a\":1,\"a\":2}"))
                    .status == 400,
            "duplicate JSON accepted");
      Check(Raw(port, "GET /health HTTP/1.1\r\nHost: localhost:" +
                          std::to_string(port) + "\r\nX-Large: " +
                          std::string(17000, 'a') + "\r\n\r\n")
                    .status == 431,
            "oversized headers accepted");
    });
    std::string session;
    test("modern MCP initialize and notification", [&] {
      const auto initialize =
          Raw(port, Request(port, "POST", "/mcp",
                            Rpc(1, "initialize",
                                J::object({{"protocolVersion", "2025-03-26"},
                                           {"capabilities", J::object()},
                                           {"clientInfo",
                                            J::object({{"name", "Native test"},
                                                       {"version", "1"}})}}))
                                .dump()));
      Check(initialize.status == 200 && initialize.Json()
                                                .at("result")
                                                .at("protocolVersion")
                                                .as_string() == "2025-03-26",
            "MCP initialize failed");
      session = initialize.Header("Mcp-Session-Id");
      Check(session.size() == 48, "missing MCP session ID");
      const auto notification = J::object(
          {{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
      const auto response =
          Raw(port, Request(port, "POST", "/mcp", notification.dump(),
                            "Mcp-Session-Id: " + session + "\r\n"));
      Check(response.status == 202 && response.body.empty(),
            "MCP notification response failed");
    });
    test("modern MCP native tools and shared session", [&] {
      const auto catalog =
          Raw(port, Request(port, "POST", "/mcp", Rpc(2, "tools/list").dump(),
                            "Mcp-Session-Id: " + session +
                                "\r\nMcp-Protocol-Version: 2025-03-26\r\n"));
      Check(catalog.status == 200 &&
                catalog.Json().at("result").at("tools").size() >= 64,
            "MCP tools missing");
      const auto response =
          Raw(port, Request(port, "POST", "/mcp",
                            Rpc(3, "tools/call",
                                J::object({{"name", "strings_entries"},
                                           {"arguments", J::object()}}))
                                .dump(),
                            "Mcp-Session-Id: " + session + "\r\n"));
      const auto content = J::Parse(
          response.Json().at("result").at("content")[0].at("text").as_string());
      Check(content.at("result").at("rows")[0].at("text").as_string() ==
                "Native \xce\xa9",
            "REST/MCP state split");
    });
    test("MCP unknown sessions and operation errors", [&] {
      Check(Raw(port, Request(port, "POST", "/mcp", Rpc(4, "ping").dump()))
                    .status == 400,
            "missing session accepted");
      Check(
          Raw(port, Request(port, "POST", "/mcp", Rpc(4, "ping").dump(),
                            "Mcp-Session-Id: " + std::string(48, '0') + "\r\n"))
                  .status == 404,
          "unknown session accepted");
      const auto response = Raw(
          port,
          Request(port, "POST", "/mcp",
                  Rpc(4, "tools/call",
                      J::object({{"name", "strings_set"},
                                 {"arguments", J::object({{"key", false},
                                                          {"text", "bad"}})}}))
                      .dump(),
                  "Mcp-Session-Id: " + session + "\r\n"));
      Check(response.status == 200 &&
                response.Json().at("result").at("isError").as_bool(),
            "MCP execution error lost");
      Check(Raw(port, Request(port, "GET", "/mcp", {},
                              "Mcp-Session-Id: " + session + "\r\n"))
                    .status == 405,
            "optional MCP stream not explicit");
    });
    test("modern MCP termination", [&] {
      Check(Raw(port, Request(port, "DELETE", "/mcp", {},
                              "Mcp-Session-Id: " + session + "\r\n"))
                    .status == 200,
            "MCP delete failed");
      Check(Raw(port, Request(port, "POST", "/mcp", Rpc(5, "ping").dump(),
                              "Mcp-Session-Id: " + session + "\r\n"))
                    .status == 404,
            "deleted session accepted");
    });
    test("legacy MCP SSE transport", [&] {
      Connection stream(port);
      stream.Send(Request(port, "GET", "/sse"));
      const auto headers = stream.Until("\r\n\r\n");
      Check(headers.find("text/event-stream") != headers.npos,
            "SSE content type");
      const auto endpoint = stream.Until("\n\n");
      const auto begin = endpoint.find("data: ");
      Check(begin != endpoint.npos, "SSE endpoint missing");
      const auto path = endpoint.substr(begin + 6, endpoint.size() - begin - 8);
      Check(path.starts_with("/messages?session_id="), "SSE endpoint invalid");
      Check(
          Raw(port,
              Request(port, "POST", path,
                      Rpc(10, "initialize",
                          J::object({{"protocolVersion", "2024-11-05"},
                                     {"capabilities", J::object()},
                                     {"clientInfo",
                                      J::object({{"name", "Legacy native test"},
                                                 {"version", "1"}})}}))
                          .dump()))
                  .status == 202,
          "legacy init POST failed");
      Check(SseMessage(stream).at("id").as_int() == 10,
            "legacy init event missing");
      Check(Raw(port, Request(port, "POST", path, Rpc(11, "tools/list").dump()))
                    .status == 202,
            "legacy tools POST failed");
      const auto tools = SseMessage(stream);
      Check(tools.at("result").at("tools").size() >= 64,
            "legacy tools event missing");
    });
    test("bind validation and port ownership", [&] {
      bool rejected = false;
      try {
        Monstey_HttpOptions bad;
        bad.host = "0.0.0.0";
        Monstey_HttpServer invalid(runtime, bad);
      } catch (const std::exception &) {
        rejected = true;
      }
      Check(rejected, "non-loopback bind accepted");
      rejected = false;
      try {
        Monstey_HttpOptions same;
        same.port = static_cast<std::uint16_t>(port);
        Monstey_HttpServer invalid(runtime, same);
      } catch (const std::exception &) {
        rejected = true;
      }
      Check(rejected, "occupied listener port reused");
    });
    test("shutdown interrupts incomplete HTTP and SSE", [&] {
      Connection idle(port);
      Connection stream(port);
      stream.Send(Request(port, "GET", "/sse"));
      (void)stream.Until("\r\n\r\n");
      (void)stream.Until("\n\n");
      const auto start = std::chrono::steady_clock::now();
      server.Stop();
      server.Wait();
      const auto elapsed = std::chrono::steady_clock::now() - start;
      if (elapsed >= std::chrono::seconds(3))
        throw std::runtime_error(
            "network waits were not interrupted: " +
            std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                    .count()) +
            " ms");
      Check(!server.Running(), "listener still active");
    });
    Monstey_WriteNewText(
        directory / "http-tests.json",
        J::object({{"passed", passed},
                   {"failed", 0},
                   {"implementation", "C++20"},
                   {"transports", J::array({"REST", "MCP streamable HTTP",
                                            "MCP legacy SSE"})}})
            .dump(2));
    std::cout << passed << " native HTTP test groups passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
