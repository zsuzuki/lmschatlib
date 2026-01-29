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
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lmschat {

struct OutputChunk {
  std::string type;
  std::string content;
};

struct Stats {
  std::optional<int64_t> input_tokens;
  std::optional<int64_t> total_output_tokens;
  std::optional<int64_t> reasoning_output_tokens;
  std::optional<double> tokens_per_second;
  std::optional<double> time_to_first_token_seconds;
};

struct Response {
  std::string model_instance_id;
  std::vector<OutputChunk> output;
  Stats stats;
  std::string response_id;
};

struct ClientConfig {
  std::string base_url = "http://localhost:1234";
  std::optional<std::string> api_token;
  int timeout_seconds = 120;
};

class Client {
 public:
  explicit Client(ClientConfig config = {});

  Response chat(std::string model, std::string input,
                std::optional<std::string> previous_response_id = std::nullopt);

  void set_api_token(std::optional<std::string> token);
  void set_base_url(std::string url);
  void set_timeout_seconds(int seconds);

  const ClientConfig& config() const { return config_; }

 private:
  ClientConfig config_;
};

class ChatSession {
 public:
  explicit ChatSession(Client client, std::string model);

  Response send(std::string message);
  Response send(std::string message, std::optional<std::string> override_previous_response_id);

  const std::optional<std::string>& previous_response_id() const { return previous_response_id_; }
  const std::optional<Stats>& last_stats() const { return last_stats_; }

  void set_previous_response_id(std::optional<std::string> id) { previous_response_id_ = std::move(id); }

 private:
  Client client_;
  std::string model_;
  std::optional<std::string> previous_response_id_;
  std::optional<Stats> last_stats_;
};

}  // namespace lmschat
