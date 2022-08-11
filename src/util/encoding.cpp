#include "util/encoding.hpp"

namespace flashpoint::util {

constexpr std::string_view base64Scheme = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64ParseFrom(const std::string &data) {
  if (data.length() % 3 != 0)
    throw std::length_error("b64 string length must be multiple of three");

  std::string result = {};
  for (int i = 0; i < data.length(); i += 3) {
    result += static_cast<char>(base64Scheme.find(data[i]) << 2 | base64Scheme.find(data[i + 1]) >> 4);
    if (data[i + 2] == '=')
      break;
    result += static_cast<char>(base64Scheme.find(data[i]) << 4 | base64Scheme.find(data[i + 1]) >> 2);
    if (data[i + 3] == '=')
      break;
    result += static_cast<char>(base64Scheme.find(data[i + 2]) << 6 | base64Scheme.find(data[i + 3]));
  }

  return result;
}
std::string b64Serialize(const std::string &data) {
  std::string result = {};
  for (int i = 0; i < data.length(); i += 3) {
    result += base64Scheme.at(data[i] >> 2);
    result += base64Scheme.at(data[i] << 4 | ((i + 1 == data.length()) ? 0 : data[i + 1] >> 4));
    if (i + 1 == data.length()) {
      result += "==";
      break;
    }
    result += base64Scheme.at(data[i + 1] << 2 | ((i + 2 == data.length()) ? 0 : data[i + 2] >> 6));
    result += (i + 2 == data.length()) ? '=' : base64Scheme[data[i + 2] % 64];
  }

  return result;
}

std::size_t consume(const std::string &data, std::size_t &position, char c) {
  if (data.at(position) != c)
    throw std::runtime_error("improper format");
  return position++;
}
std::map<std::string, std::string> b64JsonParseFrom(const std::string &data) {
  std::size_t position = 0;
  consume(data, position, '{');

  std::map<std::string, std::string> result;
  while (data.at(position) != '}') {
    auto key_start = consume(data, position, '"') + 1;
    while (!(data.at(position - 1) != '\\' && data.at(position) == '"'))
      position++;
    auto key_end = consume(data, position, '"');
    auto key = data.substr(key_start, key_end);
    key.replace(key.begin(), key.end(), "\\\"", "\"");

    consume(data, position, ':');

    auto value_start = consume(data, position, '"') + 1;
    auto value_end = data.find('"', value_start);
    auto value = b64ParseFrom(data.substr(value_start, value_end));

    result.insert({key, value});
  }

  return result;
}
std::string b64JsonSerialize(const std::map<std::string, std::string> &data) {
  std::string result = "{";
  int processed = 0;
  for (auto &[key, value] : data) {
    auto formatted_key = key;
    formatted_key.replace(key.begin(), key.end(), "\"", "\\\"");
    result += "\"" + formatted_key + "\":\"" + b64Serialize(value) + "\"";
    processed++;
    if (processed == data.size())
      result += ",";
  }

  return result;
}

}// namespace flashpoint::util