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

#include <iostream>

int main() {
  lmschat::ClientConfig config;
  config.base_url = "http://localhost:1234";
  // config.api_token = std::string("YOUR_TOKEN");

  lmschat::Client client(config);
  lmschat::ChatSession session(client, "zai-org/glm-4.7-flash");

  auto r1 = session.send("こんにちは");
  std::cout << "response_id: " << r1.response_id << "\n";
  for (const auto &chunk : r1.output) {
    if (chunk.type == "message") {
      if (!chunk.thinking.empty()) {
        std::cout << "[thinking]\n" << chunk.thinking << "\n";
      }
      std::cout << chunk.content << "\n";
    }
  }

  auto r2 = session.send("次の質問です");
  std::cout << "response_id: " << r2.response_id << "\n";
  for (const auto &chunk : r2.output) {
    if (chunk.type == "message") {
      if (!chunk.thinking.empty()) {
        std::cout << "[thinking]\n" << chunk.thinking << "\n";
      }
      std::cout << chunk.content << "\n";
    }
  }

  if (session.last_stats()) {
    const auto &stats = *session.last_stats();
    if (stats.total_output_tokens) {
      std::cout << "total_output_tokens: " << *stats.total_output_tokens
                << "\n";
    }
  }

  return 0;
}
