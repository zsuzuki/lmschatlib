/*
MIT License

Copyright (c) 2026 lmschatlib contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "lmschat/client.h"
#include "lmschat/json.h"

#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace lmschat {
namespace {

struct UrlParts {
  std::string host;
  int port = 80;
  std::string path = "/";
};

UrlParts parse_url(std::string_view url) {
  UrlParts parts;
  std::string_view work = url;

  if (work.rfind("http://", 0) == 0) {
    work.remove_prefix(7);
  } else if (work.rfind("https://", 0) == 0) {
    throw std::runtime_error("https is not supported in this minimal client");
  }

  auto slash_pos = work.find('/');
  std::string_view hostport = (slash_pos == std::string_view::npos) ? work : work.substr(0, slash_pos);
  parts.path = (slash_pos == std::string_view::npos) ? "/" : std::string(work.substr(slash_pos));

  auto colon_pos = hostport.find(':');
  if (colon_pos == std::string_view::npos) {
    parts.host = std::string(hostport);
  } else {
    parts.host = std::string(hostport.substr(0, colon_pos));
    std::string port_str(hostport.substr(colon_pos + 1));
    parts.port = std::stoi(port_str);
  }

  if (parts.host.empty()) {
    throw std::runtime_error("invalid base_url");
  }
  return parts;
}

std::string read_all(int sockfd) {
  std::string data;
  char buf[4096];
  while (true) {
    ssize_t n = ::recv(sockfd, buf, sizeof(buf), 0);
    if (n == 0) break;
    if (n < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error("recv failed");
    }
    data.append(buf, static_cast<size_t>(n));
  }
  return data;
}

std::string decode_chunked(std::string_view body) {
  std::string out;
  size_t pos = 0;
  while (true) {
    size_t line_end = body.find("\r\n", pos);
    if (line_end == std::string_view::npos) {
      throw std::runtime_error("invalid chunked encoding");
    }
    std::string_view size_str = body.substr(pos, line_end - pos);
    size_t size = 0;
    try {
      size = static_cast<size_t>(std::stoul(std::string(size_str), nullptr, 16));
    } catch (...) {
      throw std::runtime_error("invalid chunk size");
    }
    pos = line_end + 2;
    if (size == 0) break;
    if (pos + size > body.size()) throw std::runtime_error("chunk size overflow");
    out.append(body.substr(pos, size));
    pos += size;
    if (body.substr(pos, 2) != "\r\n") throw std::runtime_error("invalid chunk trailer");
    pos += 2;
  }
  return out;
}

struct HttpResponse {
  int status_code = 0;
  std::string body;
  bool chunked = false;
};

HttpResponse http_post(const ClientConfig& config, std::string_view path, std::string body) {
  UrlParts parts = parse_url(config.base_url);
  std::string full_path = parts.path.empty() ? "/" : parts.path;
  if (!full_path.empty() && full_path.back() == '/' && !path.empty() && path.front() == '/') {
    full_path.pop_back();
  }
  full_path += std::string(path);

  std::string port_str = std::to_string(parts.port);
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* res = nullptr;
  int gai = ::getaddrinfo(parts.host.c_str(), port_str.c_str(), &hints, &res);
  if (gai != 0) {
    throw std::runtime_error("getaddrinfo failed");
  }

  int sockfd = -1;
  for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
    sockfd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd < 0) continue;
    if (::connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
      break;
    }
    ::close(sockfd);
    sockfd = -1;
  }
  ::freeaddrinfo(res);

  if (sockfd < 0) {
    throw std::runtime_error("connect failed");
  }

  if (config.timeout_seconds > 0) {
    timeval tv{};
    tv.tv_sec = config.timeout_seconds;
    tv.tv_usec = 0;
    ::setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  }

  std::string request;
  request.reserve(512 + body.size());
  request += "POST ";
  request += full_path;
  request += " HTTP/1.1\r\n";
  request += "Host: ";
  request += parts.host;
  request += ":" + std::to_string(parts.port) + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  if (config.api_token && !config.api_token->empty()) {
    request += "Authorization: Bearer ";
    request += *config.api_token;
    request += "\r\n";
  }
  request += "Connection: close\r\n\r\n";
  request += body;

  ssize_t sent = 0;
  while (sent < static_cast<ssize_t>(request.size())) {
    ssize_t n = ::send(sockfd, request.data() + sent, request.size() - static_cast<size_t>(sent), 0);
    if (n < 0) {
      if (errno == EINTR) continue;
      ::close(sockfd);
      throw std::runtime_error("send failed");
    }
    sent += n;
  }

  std::string raw = read_all(sockfd);
  ::close(sockfd);

  auto header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) throw std::runtime_error("invalid HTTP response");
  std::string header = raw.substr(0, header_end);
  std::string body_part = raw.substr(header_end + 4);

  HttpResponse resp;
  {
    auto line_end = header.find("\r\n");
    std::string status_line = (line_end == std::string::npos) ? header : header.substr(0, line_end);
    auto first_space = status_line.find(' ');
    if (first_space == std::string::npos) throw std::runtime_error("invalid status line");
    auto second_space = status_line.find(' ', first_space + 1);
    std::string code_str = status_line.substr(first_space + 1, second_space - first_space - 1);
    resp.status_code = std::stoi(code_str);
  }

  std::string header_lower = header;
  for (char& c : header_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  resp.chunked = header_lower.find("transfer-encoding: chunked") != std::string::npos;

  if (resp.chunked) {
    resp.body = decode_chunked(body_part);
  } else {
    resp.body = std::move(body_part);
  }

  return resp;
}

std::optional<int64_t> get_int_field(const json::Object& obj, const char* key) {
  auto it = obj.find(key);
  if (it == obj.end()) return std::nullopt;
  if (!it->second.is_number()) return std::nullopt;
  return static_cast<int64_t>(it->second.as_number());
}

std::optional<double> get_double_field(const json::Object& obj, const char* key) {
  auto it = obj.find(key);
  if (it == obj.end()) return std::nullopt;
  if (!it->second.is_number()) return std::nullopt;
  return it->second.as_number();
}

Response parse_response(std::string_view body) {
  json::Value root = json::parse(body);
  if (!root.is_object()) throw std::runtime_error("response JSON is not an object");
  const auto& obj = root.as_object();

  Response r;
  if (auto it = obj.find("model_instance_id"); it != obj.end() && it->second.is_string()) {
    r.model_instance_id = it->second.as_string();
  }
  if (auto it = obj.find("response_id"); it != obj.end() && it->second.is_string()) {
    r.response_id = it->second.as_string();
  }

  if (auto it = obj.find("output"); it != obj.end() && it->second.is_array()) {
    for (const auto& v : it->second.as_array()) {
      if (!v.is_object()) continue;
      const auto& o = v.as_object();
      OutputChunk chunk;
      if (auto t = o.find("type"); t != o.end() && t->second.is_string()) {
        chunk.type = t->second.as_string();
      }
      if (auto c = o.find("content"); c != o.end() && c->second.is_string()) {
        chunk.content = c->second.as_string();
      }
      r.output.push_back(std::move(chunk));
    }
  }

  if (auto it = obj.find("stats"); it != obj.end() && it->second.is_object()) {
    const auto& s = it->second.as_object();
    r.stats.input_tokens = get_int_field(s, "input_tokens");
    r.stats.total_output_tokens = get_int_field(s, "total_output_tokens");
    r.stats.reasoning_output_tokens = get_int_field(s, "reasoning_output_tokens");
    r.stats.tokens_per_second = get_double_field(s, "tokens_per_second");
    r.stats.time_to_first_token_seconds = get_double_field(s, "time_to_first_token_seconds");
  }

  return r;
}

std::string build_request_body(std::string_view model, std::string_view input,
                               const std::optional<std::string>& previous_response_id) {
  std::string body;
  body.reserve(256 + model.size() + input.size() + (previous_response_id ? previous_response_id->size() : 0));
  body += "{";
  body += "\"model\":\"";
  body += json::escape_string(model);
  body += "\",\"input\":\"";
  body += json::escape_string(input);
  body += "\"";
  if (previous_response_id && !previous_response_id->empty()) {
    body += ",\"previous_response_id\":\"";
    body += json::escape_string(*previous_response_id);
    body += "\"";
  }
  body += "}";
  return body;
}

}  // namespace

Client::Client(ClientConfig config) : config_(std::move(config)) {}

Response Client::chat(std::string model, std::string input,
                      std::optional<std::string> previous_response_id) {
  std::string body = build_request_body(model, input, previous_response_id);
  HttpResponse resp = http_post(config_, "/api/v1/chat", std::move(body));
  if (resp.status_code < 200 || resp.status_code >= 300) {
    throw std::runtime_error("HTTP error: " + std::to_string(resp.status_code));
  }
  return parse_response(resp.body);
}

void Client::set_api_token(std::optional<std::string> token) { config_.api_token = std::move(token); }
void Client::set_base_url(std::string url) { config_.base_url = std::move(url); }
void Client::set_timeout_seconds(int seconds) { config_.timeout_seconds = seconds; }

ChatSession::ChatSession(Client client, std::string model)
    : client_(std::move(client)), model_(std::move(model)) {}

Response ChatSession::send(std::string message) {
  Response r = client_.chat(model_, std::move(message), previous_response_id_);
  if (!r.response_id.empty()) {
    previous_response_id_ = r.response_id;
  }
  last_stats_ = r.stats;
  return r;
}

Response ChatSession::send(std::string message, std::optional<std::string> override_previous_response_id) {
  if (override_previous_response_id) {
    previous_response_id_ = override_previous_response_id;
  }
  return send(std::move(message));
}

}  // namespace lmschat
