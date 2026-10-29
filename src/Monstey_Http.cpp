#include "Monstey_Http.hpp"
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <bcrypt.h>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <limits>
#include <map>
#include <thread>

namespace Monstey {
namespace {
using J = Monstey_Json;
using Clock = std::chrono::steady_clock;
constexpr std::size_t Monstey_MaxHeader = 16u * 1024;
constexpr std::size_t Monstey_MaxBody = 64u * 1024;
struct Monstey_Wsa {
  Monstey_Wsa() {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
      throw Monstey_Error("winsock_startup", "Cannot start Winsock");
  }
  ~Monstey_Wsa() { WSACleanup(); }
};
struct Monstey_Socket {
  SOCKET value = INVALID_SOCKET;
  explicit Monstey_Socket(SOCKET socket = INVALID_SOCKET) : value(socket) {}
  ~Monstey_Socket() {
    if (value != INVALID_SOCKET)
      closesocket(value);
  }
  SOCKET Release() {
    const auto socket = value;
    value = INVALID_SOCKET;
    return socket;
  }
  Monstey_Socket(const Monstey_Socket &) = delete;
  Monstey_Socket &operator=(const Monstey_Socket &) = delete;
};
struct Monstey_HttpError : std::runtime_error {
  int status;
  std::string code;
  Monstey_HttpError(int s, std::string c, std::string message)
      : std::runtime_error(std::move(message)), status(s), code(std::move(c)) {}
};
std::string Monstey_LowerHttp(std::string text) {
  for (char &c : text)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c - 'A' + 'a');
  return text;
}
std::string Monstey_Trim(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
    text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
    text.remove_suffix(1);
  return std::string(text);
}
unsigned Monstey_Decimal(std::string_view text, unsigned maximum) {
  unsigned value{};
  const auto parsed =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || parsed.ec != std::errc{} ||
      parsed.ptr != text.data() + text.size() || value > maximum)
    throw Monstey_HttpError(400, "invalid_number",
                            "Invalid decimal header value");
  return value;
}
std::string Monstey_Token() {
  unsigned char raw[24]{};
  if (BCryptGenRandom(nullptr, raw, sizeof(raw),
                      BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    throw Monstey_Error("random_failed", "Cannot create session ID");
  constexpr char alphabet[] = "0123456789abcdef";
  std::string result;
  result.reserve(48);
  for (const auto c : raw) {
    result.push_back(alphabet[c >> 4]);
    result.push_back(alphabet[c & 15]);
  }
  return result;
}
bool Monstey_ValidToken(std::string_view text) {
  return text.size() == 48 && std::all_of(text.begin(), text.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}
std::pair<std::string, unsigned> Monstey_Authority(std::string_view text,
                                                   unsigned defaultPort) {
  if (text.empty() || text.size() > 1024 ||
      text.find_first_of("/\\?#@ \t\r\n") != text.npos)
    throw Monstey_HttpError(421, "bad_host", "Invalid HTTP authority");
  std::string_view host = text, port;
  if (text.front() == '[') {
    const auto end = text.find(']');
    if (end == text.npos)
      throw Monstey_HttpError(421, "bad_host", "Invalid IPv6 authority");
    host = text.substr(1, end - 1);
    if (end + 1 < text.size()) {
      if (text[end + 1] != ':')
        throw Monstey_HttpError(421, "bad_host", "Invalid IPv6 port");
      port = text.substr(end + 2);
    }
  } else if (const auto colon = text.find(':'); colon != text.npos) {
    host = text.substr(0, colon);
    port = text.substr(colon + 1);
    if (port.find(':') != port.npos)
      throw Monstey_HttpError(421, "bad_host", "IPv6 authority needs brackets");
  }
  if (host.empty())
    throw Monstey_HttpError(421, "bad_host", "Missing host");
  for (unsigned char c : host)
    if (c <= 32 || c >= 127 || c == '*')
      throw Monstey_HttpError(421, "bad_host", "Invalid authority character");
  if (text.back() == ':')
    throw Monstey_HttpError(421, "bad_host", "Missing port");
  return {Monstey_LowerHttp(std::string(host)),
          port.empty() ? defaultPort : Monstey_Decimal(port, 65535)};
}
bool Monstey_Loopback(std::string_view host) {
  if (host == "localhost")
    return true;
  IN_ADDR v4{};
  IN6_ADDR v6{};
  const std::string address(host);
  return (InetPtonA(AF_INET, address.c_str(), &v4) == 1 &&
          reinterpret_cast<const unsigned char *>(&v4)[0] == 127) ||
         (InetPtonA(AF_INET6, address.c_str(), &v6) == 1 &&
          IN6_IS_ADDR_LOOPBACK(&v6));
}
void Monstey_ValidateOrigin(const std::string &origin) {
  const bool tls = origin.starts_with("https://");
  const auto prefix = tls ? 8u : 7u;
  if (!tls && !origin.starts_with("http://"))
    throw Monstey_Error("invalid_origin",
                        "Origin must be an exact HTTP(S) origin");
  const auto [host, port] = Monstey_Authority(
      std::string_view(origin).substr(prefix), tls ? 443 : 80);
  const std::string authority =
      host.find(':') == host.npos ? host : "[" + host + "]";
  const std::string canonical =
      std::string(tls ? "https://" : "http://") + authority +
      ((port == (tls ? 443u : 80u)) ? "" : ":" + std::to_string(port));
  if (origin != canonical)
    throw Monstey_Error("invalid_origin", "Use canonical origin: " + canonical);
}
std::string Monstey_Unquote(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  const auto hex = [](char c) -> int {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  };
  for (std::size_t i = 0; i < value.size(); ++i) {
    char c = value[i];
    if (c == '%') {
      if (i + 2 >= value.size() || hex(value[i + 1]) < 0 ||
          hex(value[i + 2]) < 0)
        throw Monstey_HttpError(400, "bad_path", "Invalid URL escape");
      c = static_cast<char>((hex(value[i + 1]) << 4) | hex(value[i + 2]));
      i += 2;
    }
    if (c == '\0' || c == '\r' || c == '\n')
      throw Monstey_HttpError(400, "bad_path", "Invalid path character");
    result.push_back(c);
  }
  (void)Monstey_Wide(result);
  return result;
}
void Monstey_WaitSocket(SOCKET socket, bool writing, Clock::time_point deadline,
                        const std::atomic<bool> *stopped) {
  for (;;) {
    if (stopped && stopped->load())
      throw Monstey_Error("connection_stopped", "HTTP server is stopping");
    const auto left = std::chrono::duration_cast<std::chrono::microseconds>(
                          deadline - Clock::now())
                          .count();
    if (left <= 0) {
      if (!writing)
        throw Monstey_HttpError(408, "request_timeout",
                                "HTTP request deadline exceeded");
      throw Monstey_Error("send_timeout", "HTTP response deadline exceeded");
    }
    fd_set ready;
    FD_ZERO(&ready);
    FD_SET(socket, &ready);
    timeval interval{};
    interval.tv_usec = static_cast<long>(std::min<std::int64_t>(left, 100000));
    const int count = select(0, writing ? nullptr : &ready,
                             writing ? &ready : nullptr, nullptr, &interval);
    if (count > 0)
      return;
    if (count == SOCKET_ERROR)
      throw Monstey_Error("socket_wait_failed",
                          "HTTP connection closed during readiness wait");
  }
}
void Monstey_SendAll(SOCKET socket, std::string_view bytes,
                     const std::atomic<bool> *stopped = nullptr) {
  const auto deadline = Clock::now() + std::chrono::seconds(10);
  while (!bytes.empty()) {
    Monstey_WaitSocket(socket, true, deadline, stopped);
    const auto length =
        static_cast<int>(std::min<std::size_t>(bytes.size(), 1u << 20));
    const int sent = send(socket, bytes.data(), length, 0);
    if (sent == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
      continue;
    if (sent <= 0)
      throw Monstey_Error("http_send_failed",
                          "HTTP connection closed while sending");
    bytes.remove_prefix(static_cast<std::size_t>(sent));
  }
}
void Monstey_FinishHttp(SOCKET socket) noexcept {
  // Publish the response FIN before closing a rejected request. Unread queued
  // input would otherwise make Winsock reset a connection after its error body.
  (void)shutdown(socket, SD_SEND);
  u_long nonblocking = 1;
  if (ioctlsocket(socket, FIONBIO, &nonblocking) != 0)
    return;
  char discarded[4096];
  for (std::size_t bytes = 0; bytes < 256u * 1024;) {
    const int count = recv(socket, discarded, sizeof(discarded), 0);
    if (count <= 0)
      break;
    bytes += static_cast<std::size_t>(count);
  }
}
const char *Monstey_StatusText(int status) {
  switch (status) {
  case 200:
    return "OK";
  case 202:
    return "Accepted";
  case 204:
    return "No Content";
  case 400:
    return "Bad Request";
  case 403:
    return "Forbidden";
  case 404:
    return "Not Found";
  case 405:
    return "Method Not Allowed";
  case 408:
    return "Request Timeout";
  case 411:
    return "Length Required";
  case 413:
    return "Content Too Large";
  case 415:
    return "Unsupported Media Type";
  case 421:
    return "Misdirected Request";
  case 429:
    return "Too Many Requests";
  case 431:
    return "Request Header Fields Too Large";
  case 503:
    return "Service Unavailable";
  default:
    return "Internal Server Error";
  }
}
J Monstey_ApiError(const char *code, const char *message) {
  return J::object(
      {{"ok", false},
       {"api_version", "0.3.0-dev"},
       {"error", J::object({{"code", code}, {"message", message}})},
       {"runtime_verified", false}});
}
struct Monstey_Request {
  std::string method, path, body, origin;
  std::map<std::string, std::string, std::less<>> headers;
  bool head{};
};
std::string Monstey_Header(const Monstey_Request &request,
                           std::string_view name) {
  const auto found = request.headers.find(name);
  return found == request.headers.end() ? std::string{} : found->second;
}
Monstey_Request Monstey_ReadRequest(SOCKET socket,
                                    const std::atomic<bool> &stopped) {
  Monstey_Request request;
  std::string raw;
  std::size_t split = std::string::npos;
  const auto deadline = Clock::now() + std::chrono::seconds(10);
  const auto receive = [&](std::size_t maximum) {
    for (;;) {
      Monstey_WaitSocket(socket, false, deadline, &stopped);
      char buffer[4096];
      const int count =
          recv(socket, buffer,
               static_cast<int>(std::min(maximum, sizeof(buffer))), 0);
      if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
        continue;
      if (count <= 0)
        throw Monstey_HttpError(
            400, "incomplete_request",
            "HTTP request ended before its declared length");
      raw.append(buffer, static_cast<std::size_t>(count));
      return;
    }
  };
  while ((split = raw.find("\r\n\r\n")) == raw.npos) {
    if (raw.size() >= Monstey_MaxHeader)
      throw Monstey_HttpError(431, "header_too_large",
                              "HTTP headers exceed 16 KiB");
    receive(Monstey_MaxHeader - raw.size());
  }
  if (split > Monstey_MaxHeader - 4)
    throw Monstey_HttpError(431, "header_too_large",
                            "HTTP headers exceed 16 KiB");
  const auto lineEnd = raw.find("\r\n");
  const std::string_view first(raw.data(), lineEnd);
  const auto firstSpace = first.find(' '),
             secondSpace =
                 first.find(' ', firstSpace == first.npos ? 0 : firstSpace + 1);
  if (firstSpace == first.npos || secondSpace == first.npos ||
      first.substr(secondSpace + 1) != "HTTP/1.1")
    throw Monstey_HttpError(400, "bad_request",
                            "Expected HTTP/1.1 request line");
  request.method = first.substr(0, firstSpace);
  request.path = first.substr(firstSpace + 1, secondSpace - firstSpace - 1);
  request.head = request.method == "HEAD";
  if (request.path.empty() || request.path.front() != '/' ||
      request.path.find_first_of("# \t\r\n") != request.path.npos)
    throw Monstey_HttpError(400, "bad_path",
                            "Only origin-form HTTP paths are accepted");
  for (std::size_t at = lineEnd + 2; at < split;) {
    const auto end = raw.find("\r\n", at);
    if (end == raw.npos || end > split)
      throw Monstey_HttpError(400, "bad_header", "Malformed header");
    const std::string_view line(raw.data() + at, end - at);
    const auto colon = line.find(':');
    if (colon == line.npos || colon == 0)
      throw Monstey_HttpError(400, "bad_header", "Malformed header field");
    for (char c : line.substr(0, colon))
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-'))
        throw Monstey_HttpError(400, "bad_header", "Invalid header name");
    const auto name = Monstey_LowerHttp(std::string(line.substr(0, colon)));
    const auto value = Monstey_Trim(line.substr(colon + 1));
    for (unsigned char c : value)
      if ((c < 32 && c != '\t') || c == 127)
        throw Monstey_HttpError(400, "bad_header", "Invalid header value");
    if (!request.headers.emplace(name, value).second)
      throw Monstey_HttpError(400, "duplicate_header", "Duplicate HTTP header");
    at = end + 2;
  }
  if (!request.headers.contains("host"))
    throw Monstey_HttpError(400, "bad_host",
                            "Exactly one Host header is required");
  if (request.headers.contains("transfer-encoding"))
    throw Monstey_HttpError(
        400, "bad_request",
        "Transfer-Encoding is unsupported; use Content-Length");
  if (request.method == "POST" && !request.headers.contains("content-length"))
    throw Monstey_HttpError(411, "length_required",
                            "Content-Length is required");
  const auto length =
      request.headers.contains("content-length")
          ? Monstey_Decimal(request.headers.at("content-length"),
                            std::numeric_limits<unsigned>::max())
          : 0;
  if (length > Monstey_MaxBody)
    throw Monstey_HttpError(413, "payload_too_large",
                            "HTTP JSON body exceeds 64 KiB");
  if (request.method != "POST" && length != 0)
    throw Monstey_HttpError(400, "unexpected_body",
                            "This method does not accept a body");
  const auto bodyStart = split + 4;
  while (raw.size() - bodyStart < length)
    receive(length - (raw.size() - bodyStart));
  if (raw.size() - bodyStart != length)
    throw Monstey_HttpError(400, "trailing_data",
                            "HTTP pipelining is unsupported");
  request.body = raw.substr(bodyStart);
  request.origin = Monstey_Header(request, "origin");
  return request;
}
} // namespace
struct Monstey_HttpServer::Monstey_Impl {
  struct Session {
    Monstey_Mcp rpc;
    bool legacy{};
    bool closed{};
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<std::string> messages;
    std::size_t bytes{};
    Clock::time_point last = Clock::now();
    explicit Session(Monstey_Runtime &runtime, bool old)
        : rpc(runtime), legacy(old) {}
  };
  struct Client {
    SOCKET socket = INVALID_SOCKET;
    std::thread thread;
    bool done{};
  };
  Monstey_Wsa winsock;
  Monstey_Runtime &runtime;
  Monstey_HttpOptions options;
  std::atomic<SOCKET> listener{INVALID_SOCKET};
  std::atomic<bool> stopping{};
  unsigned port{};
  std::string base;
  std::thread acceptThread;
  std::mutex clientsMutex, sessionsMutex, waitMutex;
  std::vector<std::shared_ptr<Client>> clients;
  std::map<std::string, std::shared_ptr<Session>> sessions;
  explicit Monstey_Impl(Monstey_Runtime &api, Monstey_HttpOptions settings)
      : runtime(api), options(std::move(settings)) {
    options.host = Monstey_LowerHttp(options.host);
    if (options.host == "localhost")
      options.host = "127.0.0.1";
    if (!Monstey_Loopback(options.host))
      throw Monstey_Error("invalid_bind",
                          "HTTP binds only to an explicit loopback address");
    for (const auto &origin : options.origins)
      Monstey_ValidateOrigin(origin);
    const bool ipv6 = options.host.find(':') != options.host.npos;
    const int family = ipv6 ? AF_INET6 : AF_INET;
    Monstey_Socket socket(::socket(family, SOCK_STREAM, IPPROTO_TCP));
    if (socket.value == INVALID_SOCKET)
      throw Monstey_Error("listen_socket", "Cannot create listener socket");
    BOOL exclusive = TRUE;
    if (setsockopt(socket.value, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char *>(&exclusive),
                   sizeof(exclusive)) != 0)
      throw Monstey_Error("listen_exclusive",
                          "Cannot protect listener address");
    sockaddr_storage address{};
    int length{};
    if (ipv6) {
      auto &native = *reinterpret_cast<sockaddr_in6 *>(&address);
      native.sin6_family = AF_INET6;
      native.sin6_port = htons(options.port);
      (void)InetPtonA(AF_INET6, options.host.c_str(), &native.sin6_addr);
      length = sizeof(native);
    } else {
      auto &native = *reinterpret_cast<sockaddr_in *>(&address);
      native.sin_family = AF_INET;
      native.sin_port = htons(options.port);
      (void)InetPtonA(AF_INET, options.host.c_str(), &native.sin_addr);
      length = sizeof(native);
    }
    if (bind(socket.value, reinterpret_cast<const sockaddr *>(&address),
             length) != 0 ||
        listen(socket.value, 32) != 0)
      throw Monstey_Error("listen_failed",
                          "Cannot bind the requested loopback port");
    int actual = sizeof(address);
    if (getsockname(socket.value, reinterpret_cast<sockaddr *>(&address),
                    &actual) != 0)
      throw Monstey_Error("listen_address", "Cannot query listener address");
    port = ipv6 ? ntohs(reinterpret_cast<sockaddr_in6 *>(&address)->sin6_port)
                : ntohs(reinterpret_cast<sockaddr_in *>(&address)->sin_port);
    base = "http://" + (ipv6 ? "[" + options.host + "]" : options.host) + ":" +
           std::to_string(port);
    u_long nonblocking = 1;
    if (ioctlsocket(socket.value, FIONBIO, &nonblocking) != 0)
      throw Monstey_Error("listener_nonblocking",
                          "Cannot configure listener readiness");
    listener.store(socket.Release());
  }
  ~Monstey_Impl() {
    Stop();
    Wait();
  }
  void Start() {
    acceptThread = std::thread([this] { AcceptLoop(); });
  }
  void Stop() noexcept {
    if (stopping.exchange(true))
      return;
    const auto socket = listener.exchange(INVALID_SOCKET);
    if (socket != INVALID_SOCKET) {
      shutdown(socket, SD_BOTH);
      closesocket(socket);
    }
    {
      std::lock_guard lock(clientsMutex);
      for (const auto &client : clients)
        if (client->socket != INVALID_SOCKET)
          shutdown(client->socket, SD_BOTH);
    }
    {
      std::lock_guard lock(sessionsMutex);
      for (const auto &[id, session] : sessions) {
        (void)id;
        // Synchronize with the predicate check and transition into wait_for.
        // Otherwise the stop notification can arrive just before the wait.
        std::lock_guard state(session->mutex);
        session->condition.notify_all();
      }
    }
  }
  void Wait() {
    std::lock_guard lock(waitMutex);
    if (acceptThread.joinable())
      acceptThread.join();
    std::vector<std::shared_ptr<Client>> pending;
    {
      std::lock_guard guard(clientsMutex);
      pending.swap(clients);
    }
    for (auto &client : pending)
      if (client->thread.joinable())
        client->thread.join();
  }
  void Send(SOCKET socket, int status, const J &body,
            const Monstey_Request *request = nullptr,
            const std::string &extra = {}) {
    const std::string payload = body.is_null() ? std::string{} : body.dump();
    std::string headers = "HTTP/1.1 " + std::to_string(status) + " " +
                          Monstey_StatusText(status) +
                          "\r\nContent-Type: application/json; "
                          "charset=utf-8\r\nContent-Length: " +
                          std::to_string(payload.size()) +
                          "\r\nConnection: close\r\nCache-Control: "
                          "no-store\r\nX-Content-Type-Options: nosniff\r\n";
    if (request && !request->origin.empty() &&
        std::find(options.origins.begin(), options.origins.end(),
                  request->origin) != options.origins.end())
      headers += "Access-Control-Allow-Origin: " + request->origin +
                 "\r\nVary: Origin\r\n";
    headers += extra + "\r\n";
    Monstey_SendAll(socket, headers, &stopping);
    if (!request || !request->head)
      Monstey_SendAll(socket, payload, &stopping);
  }
  void Check(Monstey_Request &request) {
    const auto [host, requestedPort] =
        Monstey_Authority(Monstey_Header(request, "host"), 80);
    if (!Monstey_Loopback(host) || requestedPort != port)
      throw Monstey_HttpError(421, "bad_host",
                              "Host must identify this local server");
    if (request.headers.contains("origin") &&
        std::find(options.origins.begin(), options.origins.end(),
                  request.origin) == options.origins.end())
      throw Monstey_HttpError(403, "origin_not_allowed",
                              "Browser origin is not explicitly allowed");
  }
  std::pair<std::string, std::shared_ptr<Session>> CreateSession(bool legacy) {
    std::lock_guard lock(sessionsMutex);
    for (auto at = sessions.begin(); at != sessions.end();) {
      const auto retained = at->second;
      std::lock_guard state(retained->mutex);
      if (!retained->legacy && retained.use_count() == 2 &&
          Clock::now() - at->second->last > std::chrono::hours(1))
        at = sessions.erase(at);
      else
        ++at;
    }
    if (sessions.size() >= 128)
      throw Monstey_HttpError(503, "session_limit",
                              "Too many active MCP sessions");
    const auto id = Monstey_Token();
    auto session = std::make_shared<Session>(runtime, legacy);
    sessions.emplace(id, session);
    return {id, std::move(session)};
  }
  std::shared_ptr<Session> FindSession(const std::string &id, bool legacy) {
    if (!Monstey_ValidToken(id))
      throw Monstey_HttpError(404, "session_not_found", "Unknown MCP session");
    std::lock_guard lock(sessionsMutex);
    const auto found = sessions.find(id);
    if (found == sessions.end() || found->second->legacy != legacy)
      throw Monstey_HttpError(404, "session_not_found", "Unknown MCP session");
    auto session = found->second;
    {
      std::lock_guard state(session->mutex);
      if (session->closed)
        throw Monstey_HttpError(404, "session_closed", "MCP session is closed");
      session->last = Clock::now();
    }
    return session;
  }
  J ParseBody(const Monstey_Request &request) {
    const auto content =
        Monstey_LowerHttp(Monstey_Header(request, "content-type"));
    const auto semicolon = content.find(';');
    if (Monstey_Trim(std::string_view(content).substr(0, semicolon)) !=
        "application/json")
      throw Monstey_HttpError(415, "unsupported_media_type",
                              "Use Content-Type: application/json");
    try {
      return J::Parse(request.body);
    } catch (const std::exception &e) {
      throw Monstey_HttpError(400, "invalid_json", e.what());
    }
  }
  void Modern(SOCKET socket, const Monstey_Request &request) {
    const auto version = Monstey_Header(request, "mcp-protocol-version");
    if (!version.empty() && version != "2025-03-26")
      throw Monstey_HttpError(400, "protocol_version",
                              "Unsupported MCP protocol version");
    const auto supplied = Monstey_Header(request, "mcp-session-id");
    if (request.method == "DELETE") {
      auto session = FindSession(supplied, false);
      {
        std::lock_guard lock(sessionsMutex);
        sessions.erase(supplied);
      }
      {
        std::lock_guard state(session->mutex);
        session->closed = true;
      }
      session->condition.notify_all();
      Send(socket, 200, nullptr, &request);
      return;
    }
    if (request.method != "POST") {
      Send(socket, 405,
           Monstey_ApiError("method_not_allowed",
                            "Use POST or DELETE for this MCP endpoint"),
           &request, "Allow: POST, DELETE\r\n");
      return;
    }
    const auto message = ParseBody(request);
    const bool initialize =
        message.is_object() && message.value("method", "") == J("initialize");
    std::string id = supplied;
    std::shared_ptr<Session> session;
    if (initialize) {
      if (!supplied.empty())
        throw Monstey_HttpError(400, "already_initialized",
                                "Initialize without a session ID");
      auto created = CreateSession(false);
      id = std::move(created.first);
      session = std::move(created.second);
    } else {
      if (supplied.empty())
        throw Monstey_HttpError(
            400, "session_required",
            "Mcp-Session-Id is required after initialization");
      session = FindSession(supplied, false);
    }
    const auto response = session->rpc.Handle(message);
    if (initialize && (!response || !response->contains("result"))) {
      std::lock_guard lock(sessionsMutex);
      sessions.erase(id);
    }
    Send(socket, response ? 200 : 202, response ? *response : J(nullptr),
         &request,
         "Mcp-Session-Id: " + id +
             "\r\nAccess-Control-Expose-Headers: Mcp-Session-Id\r\n");
  }
  void LegacyStream(SOCKET socket, const Monstey_Request &request) {
    if (request.method != "GET") {
      Send(socket, 405,
           Monstey_ApiError("method_not_allowed",
                            "Use GET for the legacy event stream"),
           &request);
      return;
    }
    auto [id, session] = CreateSession(true);
    try {
      std::string header =
          "HTTP/1.1 200 OK\r\nContent-Type: "
          "text/event-stream\r\nCache-Control: no-store\r\nConnection: "
          "close\r\nX-Content-Type-Options: nosniff\r\n";
      if (!request.origin.empty())
        header += "Access-Control-Allow-Origin: " + request.origin +
                  "\r\nVary: Origin\r\n";
      Monstey_SendAll(
          socket,
          header + "\r\nevent: endpoint\ndata: /messages?session_id=" + id +
              "\n\n",
          &stopping);
      for (;;) {
        std::string message;
        {
          std::unique_lock lock(session->mutex);
          session->condition.wait_for(lock, std::chrono::seconds(15), [&] {
            return stopping.load() || session->closed ||
                   !session->messages.empty();
          });
          if (stopping.load() || session->closed)
            break;
          if (!session->messages.empty()) {
            message = std::move(session->messages.front());
            session->messages.pop_front();
            session->bytes -= message.size();
          }
        }
        if (message.empty())
          Monstey_SendAll(socket, ": keepalive\n\n", &stopping);
        else
          Monstey_SendAll(socket, "event: message\ndata: " + message + "\n\n",
                          &stopping);
      }
    } catch (...) {
      {
        std::lock_guard state(session->mutex);
        session->closed = true;
      }
      {
        std::lock_guard lock(sessionsMutex);
        sessions.erase(id);
      }
      throw;
    }
    {
      std::lock_guard state(session->mutex);
      session->closed = true;
    }
    {
      std::lock_guard lock(sessionsMutex);
      sessions.erase(id);
    }
  }
  void LegacyPost(SOCKET socket, const Monstey_Request &request,
                  std::string_view id) {
    if (request.method != "POST")
      throw Monstey_HttpError(405, "method_not_allowed",
                              "Use POST for legacy MCP messages");
    const auto session = FindSession(std::string(id), true);
    const auto response = session->rpc.Handle(ParseBody(request));
    if (response) {
      auto message = response->dump();
      std::lock_guard state(session->mutex);
      if (session->closed)
        throw Monstey_HttpError(404, "session_closed",
                                "MCP event stream closed");
      if (session->messages.size() >= 128 ||
          session->bytes + message.size() > 4u * 1024 * 1024)
        throw Monstey_HttpError(
            429, "event_queue_full",
            "MCP event receiver is not draining its stream");
      session->bytes += message.size();
      session->messages.push_back(std::move(message));
      session->condition.notify_one();
    }
    Send(socket, 202, nullptr, &request);
  }
  J OpenApi() {
    auto paths = J::object();
    const auto catalog = Monstey_Runtime::Catalog();
    for (const auto &tool : catalog.as_array())
      paths["/v1/tools/" + tool.at("name").as_string()] = J::object(
          {{"post",
            J::object(
                {{"operationId", tool.at("name")},
                 {"summary", tool.at("description")},
                 {"requestBody",
                  J::object(
                      {{"required", true},
                       {"content",
                        J::object(
                            {{"application/json",
                              J::object(
                                  {{"schema", tool.at("inputSchema")}})}})}})},
                 {"responses",
                  J::object(
                      {{"200",
                        J::object({{"description", "Native API response"}})},
                       {"400", J::object({{"description",
                                           "Invalid arguments"}})}})}})}});
    return J::object(
        {{"openapi", "3.1.0"},
         {"info", J::object({{"title", "Monstey SoW Native Workbench"},
                             {"version", "0.3.0-dev"}})},
         {"servers", J::array({J::object({{"url", base}})})},
         {"paths", paths}});
  }
  void Handle(SOCKET socket) {
    Monstey_Request request;
    try {
      request = Monstey_ReadRequest(socket, stopping);
      Check(request);
      const auto query = request.path.find('?');
      const auto path =
          Monstey_Unquote(std::string_view(request.path).substr(0, query));
      if (query != request.path.npos &&
          !request.path.starts_with("/messages?session_id="))
        throw Monstey_HttpError(400, "bad_path",
                                "Endpoint does not accept a query string");
      if (request.method == "OPTIONS") {
        const auto method =
            Monstey_Header(request, "access-control-request-method");
        const auto headers = Monstey_LowerHttp(
            Monstey_Header(request, "access-control-request-headers"));
        if (method != "GET" && method != "POST" && method != "DELETE" &&
            method != "HEAD")
          throw Monstey_HttpError(403, "preflight_not_allowed",
                                  "Unsupported preflight method");
        for (std::size_t at = 0; at < headers.size();) {
          const auto end = headers.find(',', at);
          const auto field = Monstey_Trim(std::string_view(headers).substr(
              at, end == headers.npos ? headers.npos : end - at));
          if (field != "content-type" && field != "mcp-session-id" &&
              field != "mcp-protocol-version" && field != "accept" &&
              field != "last-event-id")
            throw Monstey_HttpError(403, "preflight_not_allowed",
                                    "Unsupported preflight header");
          if (end == headers.npos)
            break;
          at = end + 1;
        }
        Send(socket, 204, nullptr, &request,
             "Access-Control-Allow-Methods: GET, HEAD, POST, DELETE, "
             "OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type, "
             "Mcp-Session-Id, Mcp-Protocol-Version, Accept, "
             "Last-Event-ID\r\nAccess-Control-Max-Age: 600\r\n");
        return;
      }
      if (path == "/mcp") {
        Modern(socket, request);
        return;
      }
      if (path == "/sse") {
        LegacyStream(socket, request);
        return;
      }
      if (request.path.starts_with("/messages?session_id=")) {
        LegacyPost(socket, request, std::string_view(request.path).substr(21));
        return;
      }
      if (request.method == "GET" || request.method == "HEAD") {
        if (path == "/health")
          Send(socket, 200,
               J::object({{"ok", true},
                          {"implementation", "C++20"},
                          {"base_url", base},
                          {"runtime_verified", false}}),
               &request);
        else if (path == "/v1/tools")
          Send(socket, 200,
               J::object({{"ok", true},
                          {"api_version", "0.3.0-dev"},
                          {"tools", Monstey_Runtime::Catalog()}}),
               &request);
        else if (path == "/openapi.json")
          Send(socket, 200, OpenApi(), &request);
        else
          Send(socket, 404,
               Monstey_ApiError("not_found", "Unknown HTTP endpoint"),
               &request);
        return;
      }
      if (request.method == "POST" && path.starts_with("/v1/tools/")) {
        const auto name = std::string_view(path).substr(10);
        if (name.empty() || name.size() > 128 ||
            !std::all_of(name.begin(), name.end(), [](char c) {
              return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
            }))
          throw Monstey_HttpError(400, "invalid_tool_name",
                                  "Invalid native tool name");
        const auto reply =
            runtime.Call(J::object({{"operation", std::string(name)},
                                    {"arguments", ParseBody(request)}}));
        const bool ok = reply.at("ok").as_bool();
        const auto code =
            ok ? std::string{} : reply.at("error").at("code").as_string();
        Send(socket, ok ? 200 : (code == "unknown_operation" ? 404 : 400),
             reply, &request);
        return;
      }
      Send(socket, 405,
           Monstey_ApiError("method_not_allowed",
                            "Unsupported method for this endpoint"),
           &request);
    } catch (const Monstey_HttpError &error) {
      Send(socket, error.status,
           Monstey_ApiError(error.code.c_str(), error.what()), &request);
    }
  }
  void AcceptLoop() noexcept {
    try {
      while (!stopping.load()) {
        const auto listening = listener.load();
        if (listening == INVALID_SOCKET)
          break;
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(listening, &readable);
        timeval interval{0, 100000};
        const auto ready = select(0, &readable, nullptr, nullptr, &interval);
        if (ready == 0)
          continue;
        if (ready == SOCKET_ERROR) {
          Stop();
          break;
        }
        const SOCKET incoming = accept(listening, nullptr, nullptr);
        if (incoming == INVALID_SOCKET) {
          if (WSAGetLastError() == WSAEWOULDBLOCK)
            continue;
          if (!stopping.load())
            Stop();
          break;
        }
        Monstey_Socket guard(incoming);
        u_long nonblocking = 1;
        if (ioctlsocket(incoming, FIONBIO, &nonblocking) != 0)
          continue;
        const DWORD timeout = 10000;
        (void)setsockopt(incoming, SOL_SOCKET, SO_SNDTIMEO,
                         reinterpret_cast<const char *>(&timeout),
                         sizeof(timeout));
        std::vector<std::shared_ptr<Client>> finished;
        {
          std::lock_guard lock(clientsMutex);
          for (auto at = clients.begin(); at != clients.end();)
            if ((*at)->done) {
              finished.push_back(*at);
              at = clients.erase(at);
            } else
              ++at;
        }
        for (auto &client : finished)
          if (client->thread.joinable())
            client->thread.join();
        std::unique_lock lock(clientsMutex);
        if (stopping.load())
          break;
        if (clients.size() >= 32) {
          lock.unlock();
          try {
            Send(incoming, 503,
                 Monstey_ApiError("connection_limit",
                                  "Too many active connections"));
          } catch (...) {
          }
          continue;
        }
        auto client = std::make_shared<Client>();
        client->socket = incoming;
        clients.push_back(client);
        try {
          client->thread = std::thread([this, client] {
            try {
              Handle(client->socket);
            } catch (const std::exception &) {
              try {
                Send(client->socket, 500,
                     Monstey_ApiError("connection_failed",
                                      "Native HTTP request failed"));
              } catch (...) {
              }
            }
            Monstey_FinishHttp(client->socket);
            std::lock_guard guardLock(clientsMutex);
            closesocket(client->socket);
            client->socket = INVALID_SOCKET;
            client->done = true;
          });
        } catch (...) {
          clients.pop_back();
          throw;
        }
        (void)guard.Release();
      }
    } catch (...) {
      Stop();
    }
  }
};
Monstey_HttpServer::Monstey_HttpServer(Monstey_Runtime &runtime,
                                       Monstey_HttpOptions options)
    : impl_(std::make_unique<Monstey_Impl>(runtime, std::move(options))) {
  impl_->Start();
}
Monstey_HttpServer::~Monstey_HttpServer() = default;
std::string Monstey_HttpServer::BaseUrl() const { return impl_->base; }
bool Monstey_HttpServer::Running() const noexcept {
  return !impl_->stopping.load();
}
void Monstey_HttpServer::Stop() noexcept { impl_->Stop(); }
void Monstey_HttpServer::Wait() { impl_->Wait(); }
} // namespace Monstey
