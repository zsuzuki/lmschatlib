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

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr const char* kDefaultModel = "qwen2-vl-2b-instruct";
constexpr const char* kModelEnvName = "LMSCHAT_MODEL";
constexpr const char* kApiTokenEnvName = "LMSCHAT_API_TOKEN";

void print_usage(const char* argv0) {
  std::cerr << "Usage: " << argv0
            << " [--model <model>] <image-path> <question>\n"
            << "  model priority: --model > $" << kModelEnvName
            << " > " << kDefaultModel << "\n"
            << "  api token: $" << kApiTokenEnvName << " (optional)\n";
}

std::string read_file(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to open image: " + path);
  }

  std::string data;
  file.seekg(0, std::ios::end);
  const std::streampos size = file.tellg();
  if (size > 0) {
    data.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(data.data(), static_cast<std::streamsize>(data.size()));
  }
  return data;
}

std::string lowercase(std::string value) {
  for (char& c : value) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return value;
}

std::string mime_type_for_path(const std::string& path) {
  const size_t dot = path.find_last_of('.');
  const std::string ext = dot == std::string::npos ? "" : lowercase(path.substr(dot + 1));
  if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
  if (ext == "png") return "image/png";
  if (ext == "webp") return "image/webp";
  throw std::runtime_error("unsupported image extension: " + ext);
}

std::string base64_encode(std::string_view input) {
  static constexpr char kTable[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string output;
  output.reserve(((input.size() + 2) / 3) * 4);

  for (size_t i = 0; i < input.size(); i += 3) {
    const unsigned char b0 = static_cast<unsigned char>(input[i]);
    const unsigned char b1 = i + 1 < input.size() ? static_cast<unsigned char>(input[i + 1]) : 0;
    const unsigned char b2 = i + 2 < input.size() ? static_cast<unsigned char>(input[i + 2]) : 0;

    output.push_back(kTable[b0 >> 2]);
    output.push_back(kTable[((b0 & 0x03) << 4) | (b1 >> 4)]);
    output.push_back(i + 1 < input.size() ? kTable[((b1 & 0x0F) << 2) | (b2 >> 6)] : '=');
    output.push_back(i + 2 < input.size() ? kTable[b2 & 0x3F] : '=');
  }

  return output;
}

}  // namespace

int main(int argc, char** argv) {
  std::string model = kDefaultModel;
  if (const char* env_model = std::getenv(kModelEnvName);
      env_model != nullptr && env_model[0] != '\0') {
    model = env_model;
  }

  std::optional<std::string> image_path;
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
    if (!image_path) {
      image_path = std::move(arg);
      continue;
    }
    if (!question) {
      question = std::move(arg);
      continue;
    }
    std::cerr << "error: question must be a single argument (use quotes)\n";
    print_usage(argv[0]);
    return 2;
  }

  if (!image_path || !question) {
    print_usage(argv[0]);
    return 2;
  }

  try {
    lmschat::ClientConfig config;
    if (const char* env_token = std::getenv(kApiTokenEnvName);
        env_token != nullptr && env_token[0] != '\0') {
      config.api_token = std::string(env_token);
    }

    const std::string image_data = read_file(*image_path);
    lmschat::ImageInput image = lmschat::ImageInput::from_base64(
        mime_type_for_path(*image_path), base64_encode(image_data));

    lmschat::Client client(config);
    lmschat::Response response = client.chat(model, *question, {std::move(image)});

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
