#ifndef FLASHPOINT_ENCODING_HPP
#define FLASHPOINT_ENCODING_HPP

#include <map>
#include <stdexcept>
#include <string>

namespace flashpoint::util {

std::string b64ParseFrom(const std::string &data);
std::string b64Serialize(const std::string &data);

std::map<std::string, std::string> b64JsonParseFrom(const std::string &data);
std::string b64JsonSerialize(const std::map<std::string, std::string> &data);

}// namespace flashpoint::util

#endif//FLASHPOINT_ENCODING_HPP
