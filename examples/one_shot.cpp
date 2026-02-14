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

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace {

constexpr const char* kDefaultModel = "zai-org/glm-4.7-flash";
constexpr const char* kModelEnvName = "LMSCHAT_MODEL";

void print_usage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " [--model <model>] [--reasoning off|low|medium|high|on] <question>\n"
            << "  model priority: --model > $" << kModelEnvName
            << " > " << kDefaultModel << "\n";
}

std::optional<lmschat::Reasoning> parse_reasoning(const std::string& value) {
  if (value == "off") return lmschat::Reasoning::Off;
  if (value == "low") return lmschat::Reasoning::Low;
  if (value == "medium") return lmschat::Reasoning::Medium;
  if (value == "high") return lmschat::Reasoning::High;
  if (value == "on") return lmschat::Reasoning::On;
  return std::nullopt;
}

}  // namespace

int main(int argc, char** argv) {
  std::string model = kDefaultModel;
  if (const char* env_model = std::getenv(kModelEnvName);
      env_model != nullptr && env_model[0] != '\0') {
    model = env_model;
  }
  std::optional<lmschat::Reasoning> reasoning;
  std::optional<std::string> question;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--model") {
      if (i + 1 >= argc) {
        std::cerr << "error: --model requires a value\n";
        print_usage(argv[0]);
        return 2;
      }
      model = argv[++i];
      continue;
    }
    if (arg == "--reasoning") {
      if (i + 1 >= argc) {
        std::cerr << "error: --reasoning requires a value\n";
        print_usage(argv[0]);
        return 2;
      }
      std::optional<lmschat::Reasoning> parsed = parse_reasoning(argv[++i]);
      if (!parsed) {
        std::cerr << "error: invalid reasoning value\n";
        print_usage(argv[0]);
        return 2;
      }
      reasoning = parsed;
      continue;
    }
    if (question) {
      std::cerr << "error: question must be a single argument (use quotes)\n";
      print_usage(argv[0]);
      return 2;
    }
    question = std::move(arg);
  }

  if (!question) {
    print_usage(argv[0]);
    return 2;
  }

  try {
    lmschat::Client client;
    lmschat::ChatSession session(std::move(client), model);

    lmschat::Response response = reasoning ? session.send(*question, *reasoning)
                                           : session.send(*question);
    for (const auto& chunk : response.output) {
      if (chunk.type != "message") continue;
      if (!chunk.thinking.empty()) {
        std::cout << "[thinking]\n" << chunk.thinking << "\n";
      }
      std::cout << chunk.content << "\n";
    }
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
