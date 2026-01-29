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

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace lmschat::json {

struct Value;
using Object = std::map<std::string, Value>;
using Array = std::vector<Value>;
using Number = double;
using String = std::string;
using Bool = bool;
using Null = std::monostate;

struct Value {
  using Variant = std::variant<Null, Bool, Number, String, Array, Object>;
  Variant data;

  Value() : data(Null{}) {}
  Value(Null) : data(Null{}) {}
  Value(Bool b) : data(b) {}
  Value(Number n) : data(n) {}
  Value(String s) : data(std::move(s)) {}
  Value(const char* s) : data(String(s)) {}
  Value(Array a) : data(std::move(a)) {}
  Value(Object o) : data(std::move(o)) {}

  bool is_null() const { return std::holds_alternative<Null>(data); }
  bool is_bool() const { return std::holds_alternative<Bool>(data); }
  bool is_number() const { return std::holds_alternative<Number>(data); }
  bool is_string() const { return std::holds_alternative<String>(data); }
  bool is_array() const { return std::holds_alternative<Array>(data); }
  bool is_object() const { return std::holds_alternative<Object>(data); }

  const Bool& as_bool() const { return std::get<Bool>(data); }
  const Number& as_number() const { return std::get<Number>(data); }
  const String& as_string() const { return std::get<String>(data); }
  const Array& as_array() const { return std::get<Array>(data); }
  const Object& as_object() const { return std::get<Object>(data); }
};

class Parser {
 public:
  explicit Parser(std::string_view input) : input_(input), pos_(0) {}

  Value parse() {
    skip_ws();
    Value v = parse_value();
    skip_ws();
    if (pos_ != input_.size()) {
      throw std::runtime_error("extra data after JSON value");
    }
    return v;
  }

 private:
  std::string_view input_;
  size_t pos_;

  void skip_ws() {
    while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
      ++pos_;
    }
  }

  char peek() const {
    if (pos_ >= input_.size()) {
      return '\0';
    }
    return input_[pos_];
  }

  char get() {
    if (pos_ >= input_.size()) {
      throw std::runtime_error("unexpected end of input");
    }
    return input_[pos_++];
  }

  bool consume(char c) {
    if (peek() == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  Value parse_value() {
    skip_ws();
    char c = peek();
    if (c == '"') return Value(parse_string());
    if (c == '{') return Value(parse_object());
    if (c == '[') return Value(parse_array());
    if (c == 't' || c == 'f') return Value(parse_bool());
    if (c == 'n') return Value(parse_null());
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return Value(parse_number());
    throw std::runtime_error("invalid JSON value");
  }

  String parse_string() {
    if (!consume('"')) {
      throw std::runtime_error("expected string");
    }
    std::string out;
    while (true) {
      char c = get();
      if (c == '"') break;
      if (c == '\\') {
        char e = get();
        switch (e) {
          case '"': out.push_back('"'); break;
          case '\\': out.push_back('\\'); break;
          case '/': out.push_back('/'); break;
          case 'b': out.push_back('\b'); break;
          case 'f': out.push_back('\f'); break;
          case 'n': out.push_back('\n'); break;
          case 'r': out.push_back('\r'); break;
          case 't': out.push_back('\t'); break;
          case 'u': {
            uint32_t code = 0;
            for (int i = 0; i < 4; ++i) {
              char h = get();
              code <<= 4;
              if (h >= '0' && h <= '9') code |= (h - '0');
              else if (h >= 'a' && h <= 'f') code |= (h - 'a' + 10);
              else if (h >= 'A' && h <= 'F') code |= (h - 'A' + 10);
              else throw std::runtime_error("invalid unicode escape");
            }
            append_utf8(out, code);
            break;
          }
          default:
            throw std::runtime_error("invalid escape");
        }
      } else {
        out.push_back(c);
      }
    }
    return out;
  }

  static void append_utf8(std::string& out, uint32_t code) {
    if (code <= 0x7F) {
      out.push_back(static_cast<char>(code));
    } else if (code <= 0x7FF) {
      out.push_back(static_cast<char>(0xC0 | ((code >> 6) & 0x1F)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else if (code <= 0xFFFF) {
      out.push_back(static_cast<char>(0xE0 | ((code >> 12) & 0x0F)));
      out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | ((code >> 18) & 0x07)));
      out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
  }

  Object parse_object() {
    if (!consume('{')) {
      throw std::runtime_error("expected object");
    }
    skip_ws();
    Object obj;
    if (consume('}')) return obj;
    while (true) {
      skip_ws();
      String key = parse_string();
      skip_ws();
      if (!consume(':')) throw std::runtime_error("expected ':'");
      skip_ws();
      obj.emplace(std::move(key), parse_value());
      skip_ws();
      if (consume('}')) break;
      if (!consume(',')) throw std::runtime_error("expected ','");
    }
    return obj;
  }

  Array parse_array() {
    if (!consume('[')) {
      throw std::runtime_error("expected array");
    }
    skip_ws();
    Array arr;
    if (consume(']')) return arr;
    while (true) {
      arr.push_back(parse_value());
      skip_ws();
      if (consume(']')) break;
      if (!consume(',')) throw std::runtime_error("expected ','");
    }
    return arr;
  }

  Bool parse_bool() {
    if (input_.substr(pos_, 4) == "true") {
      pos_ += 4;
      return true;
    }
    if (input_.substr(pos_, 5) == "false") {
      pos_ += 5;
      return false;
    }
    throw std::runtime_error("invalid boolean");
  }

  Null parse_null() {
    if (input_.substr(pos_, 4) == "null") {
      pos_ += 4;
      return Null{};
    }
    throw std::runtime_error("invalid null");
  }

  Number parse_number() {
    size_t start = pos_;
    if (peek() == '-') ++pos_;
    if (peek() == '0') {
      ++pos_;
    } else {
      while (std::isdigit(static_cast<unsigned char>(peek()))) ++pos_;
    }
    if (peek() == '.') {
      ++pos_;
      while (std::isdigit(static_cast<unsigned char>(peek()))) ++pos_;
    }
    if (peek() == 'e' || peek() == 'E') {
      ++pos_;
      if (peek() == '+' || peek() == '-') ++pos_;
      while (std::isdigit(static_cast<unsigned char>(peek()))) ++pos_;
    }
    std::string num(input_.substr(start, pos_ - start));
    char* end = nullptr;
    double val = std::strtod(num.c_str(), &end);
    if (!end || end == num.c_str()) throw std::runtime_error("invalid number");
    return val;
  }
};

inline Value parse(std::string_view input) {
  Parser p(input);
  return p.parse();
}

inline std::string escape_string(std::string_view s) {
  std::string out;
  out.reserve(s.size() + 2);
  for (char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[7];
          std::snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out.push_back(c);
        }
    }
  }
  return out;
}

}  // namespace lmschat::json
